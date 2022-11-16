/*-
 * Copyright (c) 2020-2022 Hans Petter Selasky.
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
#include "../src/clientdlg.h"
#include "../src/configdlg.h"

#include <AudioUnit/AudioUnit.h>
#include <AudioToolbox/AudioToolbox.h>
#include <CoreMIDI/CoreMIDI.h>
#include <AVFoundation/AVFoundation.h>

#include <mach/mach_time.h>

#define	OUTPUT_ELEMENT 0
#define	INPUT_ELEMENT 1

static uint32_t audioDevicesMax;
static QString * audioDeviceDescription;
static uint32_t *audioDeviceIOStatus;

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
static AudioUnit audioUnit;

static MIDIClientRef hpsjam_midi_client;
static MIDIEndpointRef hpsjam_midi_input_endpoint;
static MIDIEndpointRef hpsjam_midi_output_endpoint;

static void
hpsjam_midi_read_event(const MIDIPacketList * pktList, void *refCon, void *connRefCon)
{
	QMutexLocker lock(&hpsjam_client_peer->lock);

	/* Only buffer up MIDI data when connected. */
	if (hpsjam_client_peer->address[0].valid()) {
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
		    pkt, (MIDITimeStamp) mach_absolute_time(), len, mbuf);
		MIDIReceived(hpsjam_midi_output_endpoint, &pktList);
	}
}

static void
hpsjam_midi_notify(const MIDINotification * message, void *refCon)
{
}

static CFStringRef
hpsjam_midi_create_cfstr(const char *str){
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
hpsjam_audio_callback(void *,
    AudioUnitRenderActionFlags *ioActionFlags,
    const AudioTimeStamp *inTimeStamp,
    uint32_t inBusNumber,
    uint32_t inNumberFrames,
    AudioBufferList * ioData){
	QMutexLocker locker(&audioMutex);

	const size_t n_in = ioData->mNumberBuffers;
	const size_t n_out = ioData->mNumberBuffers;

	/* set correct number of output bytes */
	for (size_t x = 0; x != n_out; x++) {
		ioData->mBuffers[x].mDataByteSize =
		    audioBufferSamples * audioOutputChannels * sizeof(float);
	}

	/* render input audio */
	AudioUnitRender(audioUnit, ioActionFlags, inTimeStamp, 1,
	    inNumberFrames, ioData);

	/* sanity checks */
	if ((n_in == 0 && n_out == 0) ||
	    audioInputBuffer[0] == 0 || audioInputBuffer[1] == 0 ||
	    audioInputBuffer[2] == 0 || audioInputBuffer[3] == 0 ||
	    audioInputBuffer[4] == 0)
		goto error;

	/* copy input to buffer */
	if (n_in == 1) {
		const unsigned map[2] = {
			(audioInputCount & 1) ? 0U : 3U,
			(audioInputCount & 1) ? 1U : 4U
		};

		for (unsigned ch = 0; ch != 2; ch++) {
			for (uint32_t x = 0; x != audioBufferSamples; x++) {
				audioInputBuffer[map[ch]][x] = ((float *)ioData->mBuffers[0].mData)
				    [x * audioInputChannels + audioInputSelection[ch]];
			}
		}

		hpsjam_client_peer->sound_process(audioInputBuffer[map[0]],
		    audioInputBuffer[map[1]], audioBufferSamples);

		/* Move MIDI data, if any */
		hpsjam_midi_write_event();

		audioInputCount++;
	} else if (n_in == audioInputChannels) {
		const unsigned map[2] = {
		    (audioInputCount & 1) ? 0U : 3U,
		    (audioInputCount & 1) ? 1U : 4U
		};

		for (unsigned ch = 0; ch != 2; ch++) {
			for (uint32_t x = 0; x != audioBufferSamples; x++) {
				audioInputBuffer[map[ch]][x] =
				    ((float *)ioData->mBuffers[audioInputSelection[ch]].mData)[x];
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

		if (delta == 0)
			goto error;

		if (delta > 2) {
			/* too far behind - skip a buffer */
			audioOutputCount++;
		}

		const unsigned map[2] = {
			(audioOutputCount & 1) ? 0U : 3U,
			(audioOutputCount & 1) ? 1U : 4U
		};

		/* check for mono output */
		if (audioOutputSelection[0] == audioOutputSelection[1]) {
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
				((float *)ioData->mBuffers[0].mData)[x * audioOutputChannels + ch] = src[x];
		}

		audioOutputCount++;
	}
	return (noErr);
error:
	/* fill silence in all outputs, if any */
	for (size_t x = 0; x != n_out; x++) {
		memset(ioData->mBuffers[x].mData, 0,
		    ioData->mBuffers[x].mDataByteSize);
	}
	return (noErr);
}

#define	HpsJamCheckErr(cond) \
({ if (cond) printf("%s:%u result=%d\n", __FILE__, __LINE__, result); (cond); })

Q_DECL_EXPORT bool
hpsjam_sound_init(const char *name, bool auto_connect)
{
	static bool sessionInit;
	AVAudioSession *sessionInstance;
	NSTimeInterval bufferDuration;
	AudioComponent inputComponent;
	AudioComponentDescription desc = {};
	AudioComponent comp;
	AudioStreamBasicDescription desiredFormat = {};
	AURenderCallbackStruct rcbs = {};
	NSError *nsErr;
	OSStatus result;
	uint32_t enableIO;
	uint32_t size;

	if (audioInit == true)
		return (true);

	if (sessionInit == false) {
		nsErr = nil;
		[[AVAudioSession sharedInstance]
		    setCategory:AVAudioSessionCategoryPlayAndRecord error:&nsErr];
		[[AVAudioSession sharedInstance] requestRecordPermission:^( BOOL granted ) {}];
		[[AVAudioSession sharedInstance] setMode:AVAudioSessionModeMeasurement error:&nsErr];

		[[NSNotificationCenter defaultCenter]
		    addObserverForName:AVAudioSessionRouteChangeNotification object:nil queue:nil
		    usingBlock:^(NSNotification* notification)
		{
		    uint8_t reason = [[notification.userInfo valueForKey:AVAudioSessionRouteChangeReasonKey] intValue];
		    if (reason == AVAudioSessionRouteChangeReasonNewDeviceAvailable ||
			reason == AVAudioSessionRouteChangeReasonOldDeviceUnavailable)
				emit hpsjam_client->w_config->audio_dev.reconfigure_audio();
		}];

		sessionInit = true;
	}

	sessionInstance = [AVAudioSession sharedInstance];
	nsErr = nil;

	[sessionInstance setCategory:AVAudioSessionCategoryPlayAndRecord
	    withOptions:AVAudioSessionCategoryOptionMixWithOthers |
	    AVAudioSessionCategoryOptionDefaultToSpeaker |
	    AVAudioSessionCategoryOptionAllowBluetooth |
	    AVAudioSessionCategoryOptionAllowBluetoothA2DP |
	    AVAudioSessionCategoryOptionAllowAirPlay error:&nsErr];

	bufferDuration = (float) audioBufferDefSamples / (float) HPSJAM_SAMPLE_RATE;
	[sessionInstance setPreferredIOBufferDuration:bufferDuration error:&nsErr];

	[sessionInstance setPreferredSampleRate:HPSJAM_SAMPLE_RATE error:&nsErr];
	[[AVAudioSession sharedInstance] setActive:YES error:&nsErr];

	desc.componentType = kAudioUnitType_Output;
	desc.componentSubType = kAudioUnitSubType_RemoteIO;
	desc.componentManufacturer = kAudioUnitManufacturer_Apple;

	inputComponent = AudioComponentFindNext(NULL, &desc);

	result = AudioComponentInstanceNew(inputComponent, &audioUnit);
	if (HpsJamCheckErr(result != noErr))
		goto error;

	enableIO = 1;
	result = AudioUnitSetProperty(
	    audioUnit,
	    kAudioOutputUnitProperty_EnableIO,
	    kAudioUnitScope_Input,
	    INPUT_ELEMENT,
	    &enableIO,
	    sizeof(enableIO));
	if (HpsJamCheckErr(result != noErr))
		goto error;

	result = AudioUnitSetProperty(
	    audioUnit,
	    kAudioOutputUnitProperty_EnableIO,
	    kAudioUnitScope_Output,
	    OUTPUT_ELEMENT,
	    &enableIO,
	    sizeof(enableIO));
	if (HpsJamCheckErr(result != noErr))
		goto error;

	desiredFormat.mSampleRate = HPSJAM_SAMPLE_RATE;
	desiredFormat.mFormatID = kAudioFormatLinearPCM;
	desiredFormat.mFormatFlags = kAudioFormatFlagsNativeEndian | kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
	desiredFormat.mFramesPerPacket = 1;
	desiredFormat.mChannelsPerFrame = audioInputChannels = audioOutputChannels = 2;
	desiredFormat.mBitsPerChannel = 8 * sizeof(float);
	desiredFormat.mBytesPerPacket = 2 * sizeof(float);
	desiredFormat.mBytesPerFrame = 2 * sizeof(float);

	result = AudioUnitSetProperty(
	    audioUnit,
	    kAudioUnitProperty_StreamFormat,
	    kAudioUnitScope_Output,
	    INPUT_ELEMENT,
	    &desiredFormat,
	    sizeof(desiredFormat));
	if (HpsJamCheckErr(result != noErr))
		goto error;

	result = AudioUnitSetProperty(
	    audioUnit,
	    kAudioUnitProperty_StreamFormat,
	    kAudioUnitScope_Input,
	    OUTPUT_ELEMENT,
	    &desiredFormat,
	    sizeof(desiredFormat));
	if (HpsJamCheckErr(result != noErr))
		goto error;

	bufferDuration = [[AVAudioSession sharedInstance] IOBufferDuration];
	audioBufferSamples = roundf((float) HPSJAM_SAMPLE_RATE * bufferDuration);
	if (HpsJamCheckErr(audioBufferSamples == 0))
		goto error;

	rcbs.inputProc = &hpsjam_audio_callback;
	rcbs.inputProcRefCon = NULL;

	result = AudioUnitSetProperty(
	    audioUnit,
	    kAudioUnitProperty_SetRenderCallback,
	    kAudioUnitScope_Global,
	    OUTPUT_ELEMENT,
	    &rcbs,
	    sizeof(rcbs));
	if (HpsJamCheckErr(result != noErr))
		goto error;

	audioInputBuffer[0] = new float[audioBufferSamples];
	audioInputBuffer[1] = new float[audioBufferSamples];
	audioInputBuffer[2] = new float[audioBufferSamples];
	audioInputBuffer[3] = new float[audioBufferSamples];
	audioInputBuffer[4] = new float[audioBufferSamples];

	memset(audioInputBuffer[2], 0, sizeof(float) * audioBufferSamples);

	audioInputCount = 0;
	audioOutputCount = 0;

	result = AudioUnitInitialize(audioUnit);
	if (HpsJamCheckErr(result != noErr))
		goto error;

	result = AudioOutputUnitStart(audioUnit);
	if (HpsJamCheckErr(result != noErr))
		goto error;

	audioInit = true;

	hpsjam_sound_set_input_channel(0, 0);
	hpsjam_sound_set_input_channel(1, 1);
	hpsjam_sound_set_output_channel(0, 0);
	hpsjam_sound_set_output_channel(1, 1);

	return (false);
error:
	AudioComponentInstanceDispose(audioUnit);

	QMutexLocker locker(&audioMutex);

	delete[] audioInputBuffer[0];
	delete[] audioInputBuffer[1];
	delete[] audioInputBuffer[2];
	delete[] audioInputBuffer[3];
	delete[] audioInputBuffer[4];

	audioInputBuffer[0] = 0;
	audioInputBuffer[1] = 0;
	audioInputBuffer[2] = 0;
	audioInputBuffer[3] = 0;
	audioInputBuffer[4] = 0;

	return (true);
}

Q_DECL_EXPORT void
hpsjam_sound_uninit()
{
	if (audioInit == false)
		return;

	audioInit = false;

	AudioOutputUnitStop(audioUnit);
	AudioComponentInstanceDispose(audioUnit);

	QMutexLocker locker(&audioMutex);

	delete[] audioInputBuffer[0];
	delete[] audioInputBuffer[1];
	delete[] audioInputBuffer[2];
	delete[] audioInputBuffer[3];
	delete[] audioInputBuffer[4];

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

	if (hpsjam_sound_init(0, 0) == false)
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

	if (hpsjam_sound_init(0, 0) == false)
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
hpsjam_sound_get_input_status(QString & status)
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
hpsjam_sound_get_output_status(QString & status)
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
			hpsjam_sound_init(0, 0);
		}
	}
	return (audioBufferDefSamples);
}

Q_DECL_EXPORT void
hpsjam_sound_rescan()
{
	audioDevicesMax = 1;

	delete[] audioDeviceDescription;
	audioDeviceDescription = 0;

	delete[] audioDeviceIOStatus;
	audioDeviceIOStatus = 0;

	audioDeviceDescription = new QString[audioDevicesMax];
	audioDeviceDescription[0] = QString("Default");

	audioDeviceIOStatus = new uint32_t[audioDevicesMax];

	audioDeviceIOStatus[0] = 3;	/* both is supported */
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

Q_DECL_EXPORT bool
hpsjam_sound_is_input_device(int index)
{
	if (index < 0 || (unsigned)index >= audioDevicesMax)
		return (false);
	else
		return (audioDeviceIOStatus[index] & 1);
}

Q_DECL_EXPORT bool
hpsjam_sound_is_output_device(int index)
{
	if (index < 0 || (unsigned)index >= audioDevicesMax)
		return (false);
	else
		return (audioDeviceIOStatus[index] & 2);
}
