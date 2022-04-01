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

static uint32_t audioDevicesMax;
static QString *audioDeviceDescription;
static AudioDeviceID *audioDevicesID;

static AudioDeviceID audioOutputDevice;
static AudioDeviceID audioInputDevice;
static AudioDeviceIOProcID audioInputProcID;
static AudioDeviceIOProcID audioOutputProcID;
static bool audioInit;
static uint32_t audioBufferDefSamples = 2 * HPSJAM_DEF_SAMPLES;
static uint32_t audioBufferSamples;
static uint32_t audioInputChannels;
static uint32_t audioOutputChannels;
static float *audioInputBuffer[5];
static uint32_t audioInputCount;
static uint32_t audioOutputCount;
static QMutex audioMutex;
static uint32_t audioInputDeviceSelection;
static uint32_t audioOutputDeviceSelection;
static uint32_t audioMaxDeviceSelection;
static unsigned audioInputSelection[2];
static unsigned audioOutputSelection[2];

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

	/* set correct number of output bytes */
	for (size_t x = 0; x != n_out; x++) {
		outData->mBuffers[x].mDataByteSize =
		    audioBufferSamples * audioOutputChannels * sizeof(float);
	}

	/* sanity check */
	for (size_t x = 0; x != n_in; x++) {
		if (inData->mBuffers[x].mDataByteSize !=
		    (audioBufferSamples * audioInputChannels * sizeof(float)))
			goto error;
	}

	if (n_in > 1 || n_out > 1 || (n_in == 0 && n_out == 0) ||
	    audioInputBuffer[0] == 0 || audioInputBuffer[1] == 0 ||
	    audioInputBuffer[2] == 0 || audioInputBuffer[3] == 0 ||
	    audioInputBuffer[4] == 0)
		goto error;

	/* copy input to buffer */
	if (n_in == 1 && deviceID == audioInputDevice) {
		const unsigned map[2] = {
		    (audioInputCount & 1) ? 0 : 3,
		    (audioInputCount & 1) ? 1 : 4
		};

		for (unsigned ch = 0; ch != 2; ch++) {
			for (uint32_t x = 0; x != audioBufferSamples; x++) {
				audioInputBuffer[map[ch]][x] = ((float *)inData->mBuffers[0].mData)
				    [x * audioInputChannels + audioInputSelection[ch]];
			}
		}

		hpsjam_client_peer->sound_process(audioInputBuffer[map[0]],
		    audioInputBuffer[map[1]], audioBufferSamples);

		/* Move MIDI data, if any */
		hpsjam_midi_write_event();

		audioInputCount++;
	}

	/* process audio on output */
	if (n_out == 1) {
		const uint32_t delta = audioInputCount - audioOutputCount;

		if (deviceID != audioOutputDevice || delta == 0)
			goto error;

		if (delta > 2) {
			/* too far behind - skip a buffer */
			audioOutputCount++;
		}

		const unsigned map[2] = {
		    (audioOutputCount & 1) ? 0 : 3,
		    (audioOutputCount & 1) ? 1 : 4
		};

		/* check for mono output */
		if (audioInputSelection[0] == audioInputSelection[1]) {
			for (uint32_t x = 0; x != audioBufferSamples; x++) {
				audioInputBuffer[map[0]][x] =
				    (audioInputBuffer[map[0]][x] + audioInputBuffer[map[1]][x]) / 2.0f;
			}
		}

		/* fill in audio output data */
		for (uint32_t ch = 0; ch != audioOutputChannels; ch++) {
			const float *src = audioInputBuffer[
			    (ch == audioOutputSelection[0]) ? map[0] :
			   ((ch == audioOutputSelection[1]) ? map[1] : 2)];

			for (uint32_t x = 0; x != audioBufferSamples; x++)
				((float *)outData->mBuffers[0].mData)[x * audioOutputChannels + ch] = src[x];
		}

		audioOutputCount++;
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
hpsjam_set_buffer_samples(AudioDeviceID & audioDeviceID,
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
	const uint32_t defSamples[4] = {
		audioBufferDefSamples,
		2 * HPSJAM_DEF_SAMPLES,
		64,
		128
	};
	AudioObjectPropertyAddress address = {};
	uint32_t size = 0;
	uint32_t frameSize;

	if (audioInit == true)
		return (true);

	AudioObjectSetPropertyData(kAudioObjectSystemObject, &property, 0, 0,
	    sizeof(theRunLoop), &theRunLoop);

	address.mScope = kAudioObjectPropertyScopeGlobal;
	address.mElement = kAudioObjectPropertyElementMaster;

	switch (audioInputDeviceSelection) {
	case 0:
		size = sizeof(audioInputDevice);
		address.mSelector = kAudioHardwarePropertyDefaultInputDevice;

		if (AudioObjectGetPropertyData(kAudioObjectSystemObject,
		    &address, 0, 0, &size, &audioInputDevice))
			return (true);
		break;
	default:
		if ((audioInputDeviceSelection - 1) >= audioDevicesMax)
			return (true);
		audioInputDevice = audioDevicesID[audioInputDeviceSelection - 1];
		break;
	}

	switch (audioOutputDeviceSelection) {
	case 0:
		size = sizeof(audioOutputDevice);
		address.mSelector = kAudioHardwarePropertyDefaultOutputDevice;

		if (AudioObjectGetPropertyData(kAudioObjectSystemObject,
		    &address, 0, 0, &size, &audioOutputDevice))
			return (true);
		break;
	default:
		if ((audioOutputDeviceSelection - 1) >= audioDevicesMax)
			return (true);
		audioOutputDevice = audioDevicesID[audioOutputDeviceSelection - 1];
		break;
	}

	for (unsigned x = 0;; x++) {
		if (x == 4)
			return (true);
		frameSize = hpsjam_set_buffer_samples(audioInputDevice,
		    kAudioDevicePropertyScopeInput, defSamples[x]);

		if (hpsjam_set_buffer_samples(audioOutputDevice,
		    kAudioDevicePropertyScopeOutput, frameSize) == frameSize)
			break;
	}

	audioBufferSamples = frameSize;

	if (hpsjam_set_sample_rate_and_format())
		return (true);

	audioInputBuffer[0] = new float [audioBufferSamples];
	audioInputBuffer[1] = new float [audioBufferSamples];
	audioInputBuffer[2] = new float [audioBufferSamples];
	audioInputBuffer[3] = new float [audioBufferSamples];
	audioInputBuffer[4] = new float [audioBufferSamples];

	memset(audioInputBuffer[2], 0, sizeof(float) * audioBufferSamples);

	audioInputCount = 0;
	audioOutputCount = 0;

	audioInit = true;

	hpsjam_sound_set_input_channel(0, 0);
	hpsjam_sound_set_input_channel(1, 1);
	hpsjam_sound_set_output_channel(0, 0);
	hpsjam_sound_set_output_channel(1, 1);

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
	delete [] audioInputBuffer[2];
	delete [] audioInputBuffer[3];
	delete [] audioInputBuffer[4];

	audioInputBuffer[0] = 0;
	audioInputBuffer[1] = 0;
	audioInputBuffer[2] = 0;
	audioInputBuffer[3] = 0;
	audioInputBuffer[4] = 0;
}

Q_DECL_EXPORT int
hpsjam_sound_set_input_device(int value)
{
	if (value < 0) {
		if (audioInit == false)
			return (-1);
		else
			return (audioInputDeviceSelection);
	}

	if ((unsigned)value >= audioDevicesMax)
		value = 0;

	if ((unsigned)value == audioInputDeviceSelection &&
	    audioInit == true)
		return (value);

	hpsjam_sound_uninit();

	audioInputDeviceSelection = value;

	if (hpsjam_sound_init(0,0) == false)
		return (audioInputDeviceSelection);
	else
		return (-1);
}

Q_DECL_EXPORT int
hpsjam_sound_set_output_device(int value)
{
	if (value < 0) {
		if (audioInit == false)
			return (-1);
		else
			return (audioOutputDeviceSelection);
	}

	if ((unsigned)value >= audioDevicesMax)
		value = 0;

	if ((unsigned)value == audioOutputDeviceSelection &&
	    audioInit == true)
		return (value);

	hpsjam_sound_uninit();

	audioOutputDeviceSelection = value;

	if (hpsjam_sound_init(0,0) == false)
		return (audioOutputDeviceSelection);
	else
		return (-1);
}

Q_DECL_EXPORT int
hpsjam_sound_set_input_channel(int ch, int which)
{
	if (audioInit == false)
		return (-1);

	if (which > -1)
		audioInputSelection[ch] = which;

	if (audioInputChannels != 0)
		audioInputSelection[ch] %= audioInputChannels;
	else
		audioInputSelection[ch] = 0;

	return (audioInputSelection[ch]);
}

Q_DECL_EXPORT int
hpsjam_sound_set_output_channel(int ch, int which)
{
	if (audioInit == false)
		return (-1);

	if (which > -1)
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
	const int adev = hpsjam_sound_set_input_device(-1);
	if (adev < 0) {
		status = "Selection of audio input device failed";
	} else {
		status = QString("Input is %1")
		    .arg(audioDeviceDescription[adev]);
	}
}

Q_DECL_EXPORT void
hpsjam_sound_get_output_status(QString &status)
{
	const int adev = hpsjam_sound_set_output_device(-1);
	if (adev < 0) {
		status = "Selection of audio output device failed";
	} else {
		status = QString("Output is %1")
		    .arg(audioDeviceDescription[adev]);
	}
}

Q_DECL_EXPORT int
hpsjam_sound_toggle_buffer_samples(int value)
{
	if (value > 0 && value <= HPSJAM_MAX_BUFFER_SAMPLES) {
		if (audioBufferDefSamples != (uint32_t)value) {
			audioBufferDefSamples = value;
			hpsjam_sound_uninit();
			hpsjam_sound_init(0,0);
		}
	}
	return (audioBufferDefSamples);
}

Q_DECL_EXPORT void
hpsjam_sound_rescan()
{
	AudioObjectPropertyAddress address = {};
	CFStringRef cfstring;
	uint32_t size = 0;

	address.mScope = kAudioObjectPropertyScopeGlobal;
	address.mElement = kAudioObjectPropertyElementMaster;
	address.mSelector = kAudioHardwarePropertyDevices;

	AudioObjectGetPropertyDataSize(kAudioObjectSystemObject,
	    &address, 0, 0, &size);

	audioDevicesMax = size / sizeof(AudioDeviceID);

	delete [] audioDeviceDescription;
	audioDeviceDescription = 0;

	delete [] audioDevicesID;
	audioDevicesID = 0;

	if (audioDevicesMax == 0)
		return;

	/* get list of audio device IDs */
	audioDevicesID = new AudioDeviceID[audioDevicesMax];
	size = sizeof(AudioDeviceID) * audioDevicesMax;

	AudioObjectGetPropertyData(kAudioObjectSystemObject,
	    &address, 0, 0, &size, audioDevicesID);

	audioDevicesMax++;
	audioDeviceDescription = new QString [audioDevicesMax];
	audioDeviceDescription[0] = QString("Default");

	address.mScope = kAudioObjectPropertyScopeGlobal;
	address.mSelector = kAudioObjectPropertyName;
	address.mElement = kAudioObjectPropertyElementMaster;

	for (uint32_t x = 1; x != audioDevicesMax; x++) {
		cfstring = 0;
		size = sizeof(CFStringRef);
		AudioObjectGetPropertyData(audioDevicesID[x - 1],
		    &address, 0, 0, &size, &cfstring);
		audioDeviceDescription[x] = QString::fromCFString(cfstring);
	}
}

Q_DECL_EXPORT int
hpsjam_sound_max_devices()
{
	return (audioDevicesMax);
}

Q_DECL_EXPORT QString
hpsjam_sound_get_device_name(int index)
{
	if (index < 0 || (unsigned)index >= audioDevicesMax)
		return (QString("Unknown"));
	else
		return (audioDeviceDescription[index]);
}
