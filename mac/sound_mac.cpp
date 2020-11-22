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
static float *audioInputBuffer[2];
static QMutex audioMutex;
static uint32_t audioInputSelection;
static uint32_t audioOutputSelection;
static uint32_t audioMaxSelection;

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
	    audioInputBuffer[0] == 0 || audioInputBuffer[1] == 0) {
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
		if (audioInputChannels == 1) {
			for (uint32_t x = 0; x != audioBufferSamples; x++) {
				audioInputBuffer[0][x] = ((float *)inData->mBuffers[0].mData)[x];
				audioInputBuffer[1][x] = ((float *)inData->mBuffers[0].mData)[x];
			}
		} else if (audioInputChannels >= 2) {
			for (uint32_t x = 0; x != audioBufferSamples; x++) {
				audioInputBuffer[0][x] = ((float *)inData->mBuffers[0].mData)[x * audioInputChannels + 0];
				audioInputBuffer[1][x] = ((float *)inData->mBuffers[0].mData)[x * audioInputChannels + 1];
			}
		}
	}

	/* process audio on output */
	if (n_out == 1) {
		hpsjam_client_peer->sound_process(audioInputBuffer[0],
		    audioInputBuffer[1], audioBufferSamples);

		/* copy input to output */
		if (audioOutputChannels == 1) {
			for (uint32_t x = 0; x != audioBufferSamples; x++) {
				((float *)outData->mBuffers[0].mData)[x] =
				    (audioInputBuffer[0][x] + audioInputBuffer[1][x]) / 2.0f;
			}
		} else if (audioOutputChannels >= 2) {
			/* zero output buffer, if more than two channels */
			if (audioOutputChannels > 2) {
				memset(outData->mBuffers[0].mData, 0,
				       outData->mBuffers[0].mDataByteSize);
			}
			/* copy samples */
			for (uint32_t x = 0; x != audioBufferSamples; x++) {
				((float *)outData->mBuffers[0].mData)[x * audioOutputChannels + 0] = audioInputBuffer[0][x];
				((float *)outData->mBuffers[0].mData)[x * audioOutputChannels + 1] = audioInputBuffer[1][x];
			}
			/* clear old buffer, just in case there is no input */
			memset(audioInputBuffer[0], 0, audioBufferSamples * sizeof(float));
			memset(audioInputBuffer[1], 0, audioBufferSamples * sizeof(float));
		}
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
	uint32_t size = 0;
	uint32_t frameSize;

	AudioObjectSetPropertyData(kAudioObjectSystemObject, &property, 0, 0,
	    sizeof(theRunLoop), &theRunLoop);

	address.mScope = kAudioObjectPropertyScopeGlobal;
	address.mElement = kAudioObjectPropertyElementMaster;
	address.mSelector = kAudioHardwarePropertyDevices;

	AudioObjectGetPropertyDataSize(kAudioObjectSystemObject,
	    &address, 0, 0, &size);

	audioMaxSelection = size / sizeof(AudioDeviceID);

	if (audioMaxSelection == 0)
		return (true);

	/* get list of audio devices */
	AudioDeviceID audioDevices[audioMaxSelection];
	AudioObjectGetPropertyData(kAudioObjectSystemObject, &property, 0, 0, &size, audioDevices);

	/* account for default audio device */
	audioMaxSelection++;

	switch (audioInputSelection) {
	case 0:
		size = sizeof(audioInputDevice);
		address.mSelector = kAudioHardwarePropertyDefaultInputDevice;

		if (AudioObjectGetPropertyData(kAudioObjectSystemObject,
		    &address, 0, 0, &size, &audioInputDevice))
			return (true);
		break;
	default:
		if (audioInputSelection >= audioMaxSelection)
			return (true);
		audioInputDevice = audioDevices[audioInputSelection - 1];
		break;
	}

	switch (audioOutputSelection) {
	case 0:
		size = sizeof(AudioDeviceID);
		address.mSelector = kAudioHardwarePropertyDefaultOutputDevice;

		if (AudioObjectGetPropertyData(kAudioObjectSystemObject,
		    &address, 0, 0, &size, &audioOutputDevice))
			return (true);
		break;
	default:
		if (audioOutputSelection >= audioMaxSelection)
			return (true);
		audioOutputDevice = audioDevices[audioOutputSelection - 1];
		break;
	}

	address.mSelector = kAudioDevicePropertyDeviceHasChanged;

	AudioObjectAddPropertyListener(audioInputDevice,
	    &address, hpsjam_device_notification, 0);

	AudioObjectAddPropertyListener(audioOutputDevice,
	    &address, hpsjam_device_notification, 0);

	AudioDeviceCreateIOProcID(audioInputDevice,
	    hpsjam_audio_callback, 0, &audioInputProcID);

	AudioDeviceCreateIOProcID(audioOutputDevice,
	    hpsjam_audio_callback, 0, &audioOutputProcID);

	frameSize = hpsjam_set_buffer_size(audioInputDevice,
	    kAudioDevicePropertyScopeInput, 2 * (HPSJAM_SAMPLE_RATE / 1000));

	if (hpsjam_set_buffer_size(audioOutputDevice,
	    kAudioDevicePropertyScopeOutput, frameSize) != frameSize)
		return (true);

	audioBufferSamples = frameSize;

	if (hpsjam_set_sample_rate_and_format())
		return (true);

	audioInputBuffer[0] = new float [audioBufferSamples];
	audioInputBuffer[1] = new float [audioBufferSamples];

	AudioDeviceStart(audioInputDevice, audioInputProcID);
	AudioDeviceStart(audioOutputDevice, audioOutputProcID);

	audioInit = true;

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
hpsjam_sound_toggle_input(int value)
{
	hpsjam_sound_uninit();

	for (uint32_t x = 0; x < audioMaxSelection; x++) {
		audioInputSelection++;
		if (audioInputSelection >= audioMaxSelection)
			audioInputSelection = 0;
		if (value > -1 && (uint32_t)value != audioInputSelection)
			continue;
		if (hpsjam_sound_init(0,0) == false)
			return (audioInputSelection);
	}
	return (-1);
}

Q_DECL_EXPORT int
hpsjam_sound_toggle_output(int value)
{
	hpsjam_sound_uninit();

	for (uint32_t x = 0; x < audioMaxSelection; x++) {
		audioOutputSelection++;
		if (audioOutputSelection >= audioMaxSelection)
			audioOutputSelection = 0;
		if (value > -1 && (uint32_t)value != audioOutputSelection)
			continue;
		if (hpsjam_sound_init(0,0) == false)
			return (audioOutputSelection);
	}
	return (-1);
}
