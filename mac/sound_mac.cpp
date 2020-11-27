/*-
 * Copyright (c) 2020 Hans Petter Selasky. All rights reserved.
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

static OSStatus
hpsjam_audio_callback(AudioDeviceID inDevice,
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
	    audioInputBuffer[2] == 0) {
error:
		/* fill silence in all outputs */
		for (size_t x = 0; x != n_out; x++) {
			memset(outData->mBuffers[x].mData, 0,
			    outData->mBuffers[x].mDataByteSize);
		}
		return (kAudioHardwareNoError);
	}

	/* copy input to buffer */
	if (n_in == 1) {
		for (unsigned ch = 0; ch != 2; ch++) {
			for (uint32_t x = 0; x != audioBufferSamples; x++) {
				audioInputBuffer[ch][x] = ((float *)inData->mBuffers[0].mData)
				    [x * audioInputChannels + audioInputSelection[ch]];
			}
		}
	}

	/* process audio on output */
	if (n_out == 1) {
		hpsjam_client_peer->sound_process(audioInputBuffer[0],
		    audioInputBuffer[1], audioBufferSamples);

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
	    (!(sd.mFormatFlags & kAudioFormatFlagIsPacked)));
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

	AudioStreamID vInputStreamIDList[size];

	AudioObjectGetPropertyData(audioInputDevice,
	    &address, 0, 0, &size, &vInputStreamIDList[0]);

	const AudioStreamID inputStreamID = vInputStreamIDList[0];

	size = 0;
	address.mSelector = kAudioDevicePropertyStreams;
	address.mScope = kAudioObjectPropertyScopeOutput;

	AudioObjectGetPropertyDataSize(audioOutputDevice,
	    &address, 0, 0, &size);

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
	address.mSelector = kAudioObjectPropertyName;
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
	address.mSelector = kAudioObjectPropertyName;
	size = sizeof(CFStringRef);
	AudioObjectGetPropertyData(audioOutputDevice, &address, 0, 0, &size, &cfstring);
	audioOutputDeviceName = QString::fromCFString(cfstring);

	frameSize = hpsjam_set_buffer_size(audioInputDevice,
	    kAudioDevicePropertyScopeInput, 2 * HPSJAM_DEF_SAMPLES);

	if (hpsjam_set_buffer_size(audioOutputDevice,
	    kAudioDevicePropertyScopeOutput, frameSize) != frameSize)
		return (true);

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

	AudioObjectAddPropertyListener(audioOutputDevice,
	    &address, hpsjam_device_notification, 0);

	AudioDeviceCreateIOProcID(audioInputDevice,
	    hpsjam_audio_callback, 0, &audioInputProcID);

	AudioDeviceCreateIOProcID(audioOutputDevice,
	    hpsjam_audio_callback, 0, &audioOutputProcID);

	/* start audio */
	AudioDeviceStart(audioInputDevice, audioInputProcID);
	AudioDeviceStart(audioOutputDevice, audioOutputProcID);

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
	AudioDeviceStop(audioOutputDevice, audioOutputProcID);

	AudioDeviceDestroyIOProcID(audioInputDevice, audioInputProcID);
	AudioDeviceDestroyIOProcID(audioOutputDevice, audioOutputProcID);

	address.mElement = kAudioObjectPropertyElementMaster;
	address.mScope = kAudioObjectPropertyScopeGlobal;
	address.mSelector = kAudioDevicePropertyDeviceHasChanged;

	AudioObjectRemovePropertyListener(audioOutputDevice,
	    &address, hpsjam_device_notification, 0);
	AudioObjectRemovePropertyListener(audioInputDevice,
	    &address, hpsjam_device_notification, 0);

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

Q_DECL_EXPORT void
hpsjam_sound_get_input_status(QString &status)
{
	if (audioInit == false) {
		status = "Selection of audio input device failed";
		return;
	}
	const int adev = hpsjam_sound_toggle_input_device(-2);
	const int aich[2] = {
		hpsjam_sound_toggle_input_channel(0, -2),
		hpsjam_sound_toggle_input_channel(1, -2)
	};
	status = QString("Selected input audio device is %1:%2 "
			 "and channel %3,%4")
	    .arg(adev)
	    .arg(audioInputDeviceName)
	    .arg(aich[0])
	    .arg(aich[1]);
}

Q_DECL_EXPORT void
hpsjam_sound_get_output_status(QString &status)
{
	if (audioInit == false) {
		status = "Selection of audio output device failed";
		return;
	}
	const int adev = hpsjam_sound_toggle_output_device(-2);
	const int aoch[2] = {
		hpsjam_sound_toggle_output_channel(0, -2),
		hpsjam_sound_toggle_output_channel(1, -2)
	};
	status = QString("Selected output audio device is %1:%2 "
			 "and channel %3,%4")
	    .arg(adev)
	    .arg(audioOutputDeviceName)
	    .arg(aoch[0])
	    .arg(aoch[1]);
}
