/*-
 * Copyright (c) 2020-2021 Hans Petter Selasky. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <QObject>
#include <QMutex>
#include <QMutexLocker>
#include <QString>

#include "../src/peer.h"

#include <AudioUnit/AudioUnit.h>
#include <AudioToolbox/AudioToolbox.h>
#include <CoreMIDI/CoreMIDI.h>

#include <mach/mach_time.h>

static AudioDeviceID audioInputDevice;
static AudioDeviceID audioOutputDevice;
static AudioDeviceIOProcID audioInputProcID;
static AudioDeviceIOProcID audioOutputProcID;
static bool audioInit;
static uint32_t audioBufferSamples;
static uint32_t audioInputChannels;
static uint32_t audioOutputChannels;
static float *audioInputBuffer[3];
static QMutex audioMutex;
static uint32_t audioInputDeviceSelection;
static uint32_t audioOutputDeviceSelection;
static uint32_t audioMaxDeviceSelection;
static unsigned audioInputSelection[2];
static unsigned audioOutputSelection[2];
static QString audioInputDeviceName;
static QString audioOutputDeviceName;

static MIDIClientRef hpsjam_midi_client;
static MIDIEndpointRef hpsjam_midi_input_endpoint;
static MIDIEndpointRef hpsjam_midi_output_endpoint;

static OSStatus
hpsjam_device_notification(AudioDeviceID,
    uint32_t,
    const AudioObjectPropertyAddress * inAddresses,
    void *)
{
	if (inAddresses->mSelector == kAudioDevicePropertyDeviceHasChanged) {

	}
	return (noErr);
}

static void
hpsjam_midi_read_event(const MIDIPacketList * pktList, void *refCon, void *connRefCon)
{
	QMutexLocker lock(&hpsjam_client_peer->lock);

	/* Only buffer up MIDI data when connected. */
	if (hpsjam_client_peer->address.valid()) {
		const MIDIPacket *packet = &pktList->packet[0];

		for (unsigned n = 0; n != pktList->numPackets; n++) {
			hpsjam_default_midi[0].addData(packet->data, packet->length);
			packet = MIDIPacketNext(packet);
		}
	}
}

static void
hpsjam_midi_write_event(void)
{
	uint8_t mbuf[4] = {};
	MIDIPacketList pktList;
	MIDIPacket *pkt;
	int len;

	if (hpsjam_midi_client == 0)
		return;

	while (1) {
		len = hpsjam_client_peer->midi_process(mbuf);
		if (len < 1)
			break;
		pkt = MIDIPacketListInit(&pktList);
		pkt = MIDIPacketListAdd(&pktList, sizeof(pktList),
		    pkt, (MIDITimeStamp)mach_absolute_time(), len, mbuf);
		MIDIReceived(hpsjam_midi_output_endpoint, &pktList);
	}
}

static void
hpsjam_midi_notify(const MIDINotification *message, void *refCon)
{
}

static CFStringRef
hpsjam_midi_create_cfstr(const char *str)
{
	return (CFStringCreateWithCString(kCFAllocatorDefault,
	    str, kCFStringEncodingMacRoman));
}

Q_DECL_EXPORT void
hpsjam_midi_init(const char *name)
{
	char devname[64];

	/* Check if already initialized. */
	if (hpsjam_midi_client != 0 || name == 0)
		return;

	MIDIClientCreate(hpsjam_midi_create_cfstr(name),
	    &hpsjam_midi_notify, NULL, &hpsjam_midi_client);

	/* Check if failure. */
	if (hpsjam_midi_client == 0)
		return;

	snprintf(devname, sizeof(devname), "%s_output", name);

	MIDISourceCreate(hpsjam_midi_client,
	    hpsjam_midi_create_cfstr(devname),
	    &hpsjam_midi_output_endpoint);

	snprintf(devname, sizeof(devname), "%s_input", name);

	MIDIDestinationCreate(hpsjam_midi_client,
	    hpsjam_midi_create_cfstr(devname), &hpsjam_midi_read_event,
	    0, &hpsjam_midi_input_endpoint);
}

static OSStatus
hpsjam_audio_callback(AudioDeviceID deviceID,
    const AudioTimeStamp *,
    const AudioBufferList * inData,
    const AudioTimeStamp *,
    AudioBufferList * outData,
    const AudioTimeStamp *,
    void *)
{
	QMutexLocker locker(&audioMutex);

	/* sanity check */
	if (inData == 0 || outData == 0)
		return (kAudioHardwareNoError);

	size_t n_in = inData->mNumberBuffers;
	size_t n_out = outData->mNumberBuffers;

	/* sanity check */
	for (size_t x = 0; x != n_out; x++) {
		if (outData->mBuffers[x].mDataByteSize !=
		    (audioBufferSamples * audioOutputChannels * sizeof(float)))
			goto error;
	}

	/* sanity check */
	for (size_t x = 0; x != n_in; x++) {
		if (inData->mBuffers[x].mDataByteSize !=
		    (audioBufferSamples * audioInputChannels * sizeof(float)))
			goto error;
	}

	if (n_in > 1 || n_out > 1 || (n_in == 0 && n_out == 0) ||
	    audioInputBuffer[0] == 0 || audioInputBuffer[1] == 0 ||
	    audioInputBuffer[2] == 0)
		goto error;

	/* copy input to buffer */
	if (n_in == 1 && deviceID == audioInputDevice) {
		for (unsigned ch = 0; ch != 2; ch++) {
			for (uint32_t x = 0; x != audioBufferSamples; x++) {
				audioInputBuffer[ch][x] = ((float *)inData->mBuffers[0].mData)
				    [x * audioInputChannels + audioInputSelection[ch]];
			}
		}
	}

	/* process audio on output */
	if (n_out == 1) {
		if (deviceID != audioOutputDevice)
			goto error;

		hpsjam_client_peer->sound_process(audioInputBuffer[0],
		    audioInputBuffer[1], audioBufferSamples);

		/* Move MIDI data, if any */
		hpsjam_midi_write_event();

		/* check for mono output */
		if (audioInputSelection[0] == audioInputSelection[1]) {
			for (uint32_t x = 0; x != audioBufferSamples; x++) {
				audioInputBuffer[0][x] =
				    (audioInputBuffer[0][x] + audioInputBuffer[1][x]) / 2.0f;
			}
		}

		/* fill in audio output data */
		for (uint32_t ch = 0; ch != audioOutputChannels; ch++) {
			const float *src = audioInputBuffer[
			    (ch == audioOutputSelection[0]) ? 0 :
			   ((ch == audioOutputSelection[1]) ? 1 : 2)];

			for (uint32_t x = 0; x != audioBufferSamples; x++)
				((float *)outData->mBuffers[0].mData)[x * audioOutputChannels + ch] = src[x];
		}

		/* clear old buffer, just in case there is no input */
		memset(audioInputBuffer[0], 0, audioBufferSamples * sizeof(float));
		memset(audioInputBuffer[1], 0, audioBufferSamples * sizeof(float));
	}
	return (kAudioHardwareNoError);
error:
	/* fill silence in all outputs, if any */
	for (size_t x = 0; x != n_out; x++) {
		memset(outData->mBuffers[x].mData, 0,
		    outData->mBuffers[x].mDataByteSize);
	}
	return (kAudioHardwareNoError);
}

static uint32_t
hpsjam_set_buffer_size(AudioDeviceID & audioDeviceID,
    uint32_t mScope, uint32_t nframes)
{
	AudioObjectPropertyAddress address = {};
	uint32_t size = sizeof(uint32_t);
	uint32_t aframes = 0;

	address.mSelector = kAudioDevicePropertyBufferFrameSize;
	address.mScope = mScope;
	address.mElement = kAudioObjectPropertyElementMaster;

	AudioObjectSetPropertyData(audioDeviceID,
	    &address, 0, 0, size, &nframes);

	AudioObjectGetPropertyData(audioDeviceID,
	    &address, 0, 0, &size, &aframes);

	return (aframes);
}

static bool
hpsjam_sanity_check_format(const AudioStreamBasicDescription &sd)
{
	return ((sd.mFormatID != kAudioFormatLinearPCM) ||
	    (sd.mFramesPerPacket != 1) ||
	    (sd.mBitsPerChannel != 32) ||
	    (!(sd.mFormatFlags & kAudioFormatFlagIsFloat)) ||
	    (!(sd.mFormatFlags & kAudioFormatFlagIsPacked)) ||
	    (sd.mFormatFlags & kAudioFormatFlagIsNonInterleaved));
}

static bool
hpsjam_set_sample_rate_and_format()
{
	AudioStreamBasicDescription sd = {};
	AudioObjectPropertyAddress address = {};
	const double fSystemSampleRate = HPSJAM_SAMPLE_RATE;
	double inputSampleRate = 0;
	double outputSampleRate = 0;
	uint32_t size;

	address.mScope = kAudioObjectPropertyScopeGlobal;
	address.mElement = kAudioObjectPropertyElementMaster;
	address.mSelector = kAudioDevicePropertyNominalSampleRate;
	size = sizeof(double);

	AudioObjectGetPropertyData(audioInputDevice,
	    &address, 0, 0, &size, &inputSampleRate);

	if (inputSampleRate != fSystemSampleRate) {
		if (AudioObjectSetPropertyData(audioInputDevice,
		    &address, 0, 0, sizeof(double),
		    &fSystemSampleRate) != noErr)
			return (true);
	}

	size = sizeof(double);

	AudioObjectGetPropertyData(audioOutputDevice,
	    &address, 0, 0, &size, &outputSampleRate);

	if (outputSampleRate != fSystemSampleRate) {
		if (AudioObjectSetPropertyData(audioOutputDevice,
		    &address, 0, 0, sizeof(double),
		    &fSystemSampleRate) != noErr)
			return (true);
	}

	size = 0;
	address.mSelector = kAudioDevicePropertyStreams;
	address.mScope = kAudioObjectPropertyScopeInput;

	AudioObjectGetPropertyDataSize(audioInputDevice,
	    &address, 0, 0, &size);

	if (size == 0)
		return (true);

	AudioStreamID vInputStreamIDList[size];

	AudioObjectGetPropertyData(audioInputDevice,
	    &address, 0, 0, &size, &vInputStreamIDList[0]);

	const AudioStreamID inputStreamID = vInputStreamIDList[0];

	size = 0;
	address.mSelector = kAudioDevicePropertyStreams;
	address.mScope = kAudioObjectPropertyScopeOutput;

	AudioObjectGetPropertyDataSize(audioOutputDevice,
	    &address, 0, 0, &size);

	if (size == 0)
		return (true);

	AudioStreamID vOutputStreamIDList[size];

	AudioObjectGetPropertyData(audioOutputDevice,
	    &address, 0, 0, &size, &vOutputStreamIDList[0]);

	const AudioStreamID outputStreamID = vOutputStreamIDList[0];

	size = sizeof(AudioStreamBasicDescription);
	address.mSelector = kAudioStreamPropertyVirtualFormat;
	address.mScope = kAudioObjectPropertyScopeGlobal;

	AudioObjectGetPropertyData(inputStreamID, &address, 0, 0, &size, &sd);

	if (hpsjam_sanity_check_format(sd))
		return (true);

	audioInputChannels = sd.mChannelsPerFrame;

	AudioObjectGetPropertyData(outputStreamID, &address, 0, 0, &size, &sd);

	if (hpsjam_sanity_check_format(sd))
		return (true);

	audioOutputChannels = sd.mChannelsPerFrame;

	return (false);
}

Q_DECL_EXPORT bool
hpsjam_sound_init(const char *name, bool auto_connect)
{
	const CFRunLoopRef theRunLoop = CFRunLoopGetCurrent();
	const AudioObjectPropertyAddress property = {
		kAudioHardwarePropertyRunLoop,
		kAudioObjectPropertyScopeGlobal,
		kAudioObjectPropertyElementMaster
	};
	AudioObjectPropertyAddress address = {};
	CFStringRef cfstring;
	uint32_t size = 0;
	uint32_t frameSize;

	if (audioInit == true)
		return (true);

	AudioObjectSetPropertyData(kAudioObjectSystemObject, &property, 0, 0,
	    sizeof(theRunLoop), &theRunLoop);

	address.mScope = kAudioObjectPropertyScopeGlobal;
	address.mElement = kAudioObjectPropertyElementMaster;
	address.mSelector = kAudioHardwarePropertyDevices;

	AudioObjectGetPropertyDataSize(kAudioObjectSystemObject,
	    &address, 0, 0, &size);

	audioMaxDeviceSelection = size / sizeof(AudioDeviceID);

	if (audioMaxDeviceSelection == 0)
		return (true);

	/* get list of audio device IDs */
	AudioDeviceID audioDevices[audioMaxDeviceSelection];
	AudioObjectGetPropertyData(kAudioObjectSystemObject,
	    &address, 0, 0, &size, audioDevices);

	/* account for default audio device */
	audioMaxDeviceSelection++;

	switch (audioInputDeviceSelection) {
	case 0:
		size = sizeof(audioInputDevice);
		address.mSelector = kAudioHardwarePropertyDefaultInputDevice;

		if (AudioObjectGetPropertyData(kAudioObjectSystemObject,
		    &address, 0, 0, &size, &audioInputDevice))
			return (true);
		break;
	default:
		if (audioInputDeviceSelection >= audioMaxDeviceSelection)
			return (true);
		audioInputDevice = audioDevices[audioInputDeviceSelection - 1];
		break;
	}

	/* get input device name */
	cfstring = 0;
	address.mScope = kAudioObjectPropertyScopeGlobal;
	address.mSelector = kAudioObjectPropertyName;
	address.mElement = kAudioObjectPropertyElementMaster;
	size = sizeof(CFStringRef);
	AudioObjectGetPropertyData(audioInputDevice, &address, 0, 0, &size, &cfstring);
	audioInputDeviceName = QString::fromCFString(cfstring);

	switch (audioOutputDeviceSelection) {
	case 0:
		size = sizeof(AudioDeviceID);
		address.mSelector = kAudioHardwarePropertyDefaultOutputDevice;

		if (AudioObjectGetPropertyData(kAudioObjectSystemObject,
		    &address, 0, 0, &size, &audioOutputDevice))
			return (true);
		break;
	default:
		if (audioOutputDeviceSelection >= audioMaxDeviceSelection)
			return (true);
		audioOutputDevice = audioDevices[audioOutputDeviceSelection - 1];
		break;
	}

	/* get output device name */
	cfstring = 0;
	address.mScope = kAudioObjectPropertyScopeGlobal;
	address.mSelector = kAudioObjectPropertyName;
	address.mElement = kAudioObjectPropertyElementMaster;
	size = sizeof(CFStringRef);
	AudioObjectGetPropertyData(audioOutputDevice, &address, 0, 0, &size, &cfstring);
	audioOutputDeviceName = QString::fromCFString(cfstring);

	frameSize = hpsjam_set_buffer_size(audioInputDevice,
	    kAudioDevicePropertyScopeInput, 2 * HPSJAM_DEF_SAMPLES);

	/* try different buffer sizes before giving up */
	if (hpsjam_set_buffer_size(audioOutputDevice,
	    kAudioDevicePropertyScopeOutput, frameSize) != frameSize) {

		frameSize = hpsjam_set_buffer_size(audioInputDevice,
		    kAudioDevicePropertyScopeInput, 64);

		if (hpsjam_set_buffer_size(audioOutputDevice,
		    kAudioDevicePropertyScopeOutput, frameSize) != frameSize) {

			frameSize = hpsjam_set_buffer_size(audioInputDevice,
			    kAudioDevicePropertyScopeInput, 128);

			if (hpsjam_set_buffer_size(audioOutputDevice,
			    kAudioDevicePropertyScopeOutput, frameSize) != frameSize)
				return (true);
		}
	}

	audioBufferSamples = frameSize;

	if (hpsjam_set_sample_rate_and_format())
		return (true);

	audioInputBuffer[0] = new float [audioBufferSamples];
	audioInputBuffer[1] = new float [audioBufferSamples];
	audioInputBuffer[2] = new float [audioBufferSamples];

	memset(audioInputBuffer[2], 0, sizeof(float) * audioBufferSamples);

	audioInit = true;

	hpsjam_sound_toggle_input_channel(0, 0);
	hpsjam_sound_toggle_input_channel(1, 1);
	hpsjam_sound_toggle_output_channel(0, 0);
	hpsjam_sound_toggle_output_channel(1, 1);

	/* install callbacks */
	address.mSelector = kAudioDevicePropertyDeviceHasChanged;

	AudioObjectAddPropertyListener(audioInputDevice,
	    &address, hpsjam_device_notification, 0);

	if (audioOutputDevice != audioInputDevice) {
		AudioObjectAddPropertyListener(audioOutputDevice,
		    &address, hpsjam_device_notification, 0);
	}

	AudioDeviceCreateIOProcID(audioInputDevice,
	    hpsjam_audio_callback, 0, &audioInputProcID);

	if (audioOutputDevice != audioInputDevice) {
		AudioDeviceCreateIOProcID(audioOutputDevice,
		    hpsjam_audio_callback, 0, &audioOutputProcID);
	}

	/* start audio */
	AudioDeviceStart(audioInputDevice, audioInputProcID);

	if (audioOutputDevice != audioInputDevice) {
		AudioDeviceStart(audioOutputDevice, audioOutputProcID);
	}

	return (false);
}

Q_DECL_EXPORT void
hpsjam_sound_uninit()
{
	AudioObjectPropertyAddress address = {};

	if (audioInit == false)
		return;

	audioInit = false;
	AudioDeviceStop(audioInputDevice, audioInputProcID);
	if (audioOutputDevice != audioInputDevice) {
		AudioDeviceStop(audioOutputDevice, audioOutputProcID);
	}

	AudioDeviceDestroyIOProcID(audioInputDevice, audioInputProcID);
	if (audioOutputDevice != audioInputDevice) {
		AudioDeviceDestroyIOProcID(audioOutputDevice, audioOutputProcID);
	}

	address.mElement = kAudioObjectPropertyElementMaster;
	address.mScope = kAudioObjectPropertyScopeGlobal;
	address.mSelector = kAudioDevicePropertyDeviceHasChanged;

	AudioObjectRemovePropertyListener(audioInputDevice,
	    &address, hpsjam_device_notification, 0);
	if (audioOutputDevice != audioInputDevice) {
		AudioObjectRemovePropertyListener(audioOutputDevice,
		    &address, hpsjam_device_notification, 0);
	}

	QMutexLocker locker(&audioMutex);

	delete [] audioInputBuffer[0];
	delete [] audioInputBuffer[1];

	audioInputBuffer[0] = 0;
	audioInputBuffer[1] = 0;
}

Q_DECL_EXPORT int
hpsjam_sound_toggle_input_device(int value)
{
	if (value < -1) {
		if (audioInit == false)
			return (-1);
		else
			return (audioInputDeviceSelection);
	}

	hpsjam_sound_uninit();

	for (uint32_t x = 0; x < audioMaxDeviceSelection; x++) {
		audioInputDeviceSelection++;
		if (audioInputDeviceSelection >= audioMaxDeviceSelection)
			audioInputDeviceSelection = 0;
		if (value > -1 && (uint32_t)value != audioInputDeviceSelection)
			continue;
		if (hpsjam_sound_init(0,0) == false)
			return (audioInputDeviceSelection);
	}
	return (-1);
}

Q_DECL_EXPORT int
hpsjam_sound_toggle_output_device(int value)
{
	if (value < -1) {
		if (audioInit == false)
			return (-1);
		else
			return (audioOutputDeviceSelection);
	}

	hpsjam_sound_uninit();

	for (uint32_t x = 0; x < audioMaxDeviceSelection; x++) {
		audioOutputDeviceSelection++;
		if (audioOutputDeviceSelection >= audioMaxDeviceSelection)
			audioOutputDeviceSelection = 0;
		if (value > -1 && (uint32_t)value != audioOutputDeviceSelection)
			continue;
		if (hpsjam_sound_init(0,0) == false)
			return (audioOutputDeviceSelection);
	}
	return (-1);
}

Q_DECL_EXPORT int
hpsjam_sound_toggle_input_channel(int ch, int which)
{
	if (audioInit == false)
		return (-1);

	if (which < -1)
		;
	else if (which == -1)
		audioInputSelection[ch] += 1;
	else
		audioInputSelection[ch] = which;

	if (audioInputChannels != 0)
		audioInputSelection[ch] %= audioInputChannels;
	else
		audioInputSelection[ch] = 0;

	return (audioInputSelection[ch]);
}

Q_DECL_EXPORT int
hpsjam_sound_toggle_output_channel(int ch, int which)
{
	if (audioInit == false)
		return (-1);

	if (which < -1)
		;
	else if (which == -1)
		audioOutputSelection[ch] += 1;
	else
		audioOutputSelection[ch] = which;

	if (audioOutputChannels != 0)
		audioOutputSelection[ch] %= audioOutputChannels;
	else
		audioOutputSelection[ch] = 0;

	return (audioOutputSelection[ch]);
}

Q_DECL_EXPORT int
hpsjam_sound_max_input_channel()
{
	if (audioInputChannels == 0)
		return (1);
	else
		return (audioInputChannels);
}

Q_DECL_EXPORT int
hpsjam_sound_max_output_channel()
{
	if (audioOutputChannels == 0)
		return (1);
	else
		return (audioOutputChannels);
}

Q_DECL_EXPORT void
hpsjam_sound_get_input_status(QString &status)
{
	const int adev = hpsjam_sound_toggle_input_device(-2);
	if (adev < 0) {
		status = "Selection of audio input device failed";
	} else {
		status = QString("Input is %1:%2")
		    .arg(adev)
		    .arg(audioInputDeviceName);
	}
}

Q_DECL_EXPORT void
hpsjam_sound_get_output_status(QString &status)
{
	const int adev = hpsjam_sound_toggle_output_device(-2);
	if (adev < 0) {
		status = "Selection of audio output device failed";
	} else {
		status = QString("Output is %1:%2")
		    .arg(adev)
		    .arg(audioOutputDeviceName);
	}
}
