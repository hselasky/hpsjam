/*-
 * Copyright (c) 2022 Hans Petter Selasky. All rights reserved.
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

#include <oboe/Oboe.h>

class hpsjam_oboe;

static uint32_t audioBufferDefSamples = 2 * HPSJAM_DEF_SAMPLES;
static hpsjam_oboe *audioUnit;
static unsigned audioInputSelection[2];
static unsigned audioOutputSelection[2];

class hpsjam_oboe : public oboe::AudioStreamCallback
{
private:
	enum { MAX_BUFFER = 64 };

	float *audioBuffer[MAX_BUFFER + 1];

	uint32_t producer;
	uint32_t consumer;

	oboe::ManagedStream recStream;
	oboe::ManagedStream playStream;

	oboe::AudioStreamBuilder *setStreamParams(oboe::AudioStreamBuilder *ptr) {
		return (ptr->setFormat(oboe::AudioFormat::Float)->
		  setSharingMode(oboe::SharingMode::Exclusive)->
		  setChannelCount(oboe::ChannelCount::Stereo)->
		  setSampleRate(HPSJAM_SAMPLE_RATE)->
		  setFramesPerDataCallback(audioBufferDefSamples)->
		  setSampleRateConversionQuality(oboe::SampleRateConversionQuality::Medium)->
		  setPerformanceMode(oboe::PerformanceMode::LowLatency));
	};
public:
	hpsjam_oboe() {
		producer = consumer = 0;

		for (int x = 0; x != MAX_BUFFER + 1; x++) {
			audioBuffer[x] = new float [audioBufferDefSamples];
			memset(audioBuffer[x], 0, sizeof(audioBuffer[x][0]) * audioBufferDefSamples);
		}
	};
	~hpsjam_oboe() {
		for (int x = 0; x != MAX_BUFFER + 1; x++)
			delete [] audioBuffer[x];
	};
	oboe::DataCallbackResult onAudioReady(oboe::AudioStream *stream, void *data, int32_t frames)
	{
		if (data != 0 && frames == audioBufferDefSamples) {
			if (stream == recStream.get()) {
				for (uint32_t ch = 0; ch != 2; ch++) {
					for (int32_t x = 0; x != frames; x++) {
						audioBuffer[ch + producer][x] = ((float *)data)[x * 2 + audioInputSelection[ch]];
					}
				}
				/* buffer up data */
				producer += 2;
				producer %= MAX_BUFFER;
			}
			if (stream == playStream.get()) {
				hpsjam_client_peer->sound_process(
				    audioBuffer[0 + consumer], audioBuffer[1 + consumer], frames);

				/* check for mono output */
				if (audioOutputSelection[0] == audioOutputSelection[1]) {
					for (int32_t x = 0; x != frames; x++)
						audioBuffer[0 + consumer][x] =
						    (audioBuffer[0 + consumer][x] + audioBuffer[1 + consumer][x]) / 2.0f;
				}

				/* fill in audio output data */
				for (uint32_t ch = 0; ch != 2; ch++) {
					float *src = audioBuffer[
					    (ch == audioOutputSelection[0]) ? (0 + consumer) :
					    ((ch == audioOutputSelection[1]) ? (1 + consumer) : MAX_BUFFER)];

					for (int32_t x = 0; x != frames; x++)
						((float *)data)[x * 2 + ch] = src[x];
				}

				/* clear audio buffer */
				for (int32_t x = 0; x != frames; x++) {
					audioBuffer[0 + consumer][x] = 0.0f;
					audioBuffer[1 + consumer][x] = 0.0f;
				}

				/* check if ring-buffer is empty */
				if (producer != consumer) {
					consumer += 2;
					consumer %= MAX_BUFFER;
				}
			}
		} else if (data != 0 && stream == playStream.get()) {
			memset(data, 0, frames * stream->getBytesPerFrame());
		}
		return (oboe::DataCallbackResult::Continue);
	};
	void onErrorAfterClose(oboe::AudioStream *, oboe::Result) {};
	void onErrorBeforeClose(oboe::AudioStream *, oboe::Result) {};

	oboe::Result openPlayStream() {
		oboe::AudioStreamBuilder builder;
		oboe::Result retval;

		builder.setDirection(oboe::Direction::Output);
		builder.setCallback(this);

		setStreamParams(&builder);

		retval = builder.openManagedStream(playStream);
		if (retval == oboe::Result::OK)
			playStream->setBufferSizeInFrames(audioBufferDefSamples * 2);
		return (retval);
	};
	oboe::Result openRecStream() {
		oboe::AudioStreamBuilder builder;
		oboe::Result retval;

		builder.setDirection(oboe::Direction::Input);
		builder.setCallback(this);

		setStreamParams(&builder);

		retval = builder.openManagedStream(recStream);
		if (retval == oboe::Result::OK)
			recStream->setBufferSizeInFrames(audioBufferDefSamples * 2);
		return (retval);
	};
	void startStream() {
		recStream->requestStart();
		playStream->requestStart();
	};
	void stopStream() {
		playStream->requestStop();
		recStream->requestStop();
	};
	void closePlayStream() {
		playStream->close();
		playStream.reset();
	};
	void closeRecStream() {
		recStream->close();
		recStream.reset();
	};
};

Q_DECL_EXPORT bool
hpsjam_sound_init(const char *name, bool auto_connect)
{
	if (audioUnit != 0)
		return (true);

	audioUnit = new hpsjam_oboe();
	if (audioUnit->openRecStream() != oboe::Result::OK) {
		delete audioUnit;
		audioUnit = 0;
		return (true);
	}
	if (audioUnit->openPlayStream() != oboe::Result::OK) {
		audioUnit->closeRecStream();
		delete audioUnit;
		audioUnit = 0;
		return (true);
	}

	hpsjam_sound_set_input_channel(0, 0);
	hpsjam_sound_set_input_channel(1, 1);
	hpsjam_sound_set_output_channel(0, 0);
	hpsjam_sound_set_output_channel(1, 1);

	audioUnit->startStream();

	return (false);			/* success */
}

Q_DECL_EXPORT void
hpsjam_sound_uninit()
{
	if (audioUnit == 0)
		return;
	audioUnit->stopStream();
	audioUnit->closePlayStream();
	audioUnit->closeRecStream();
	delete audioUnit;
	audioUnit = 0;
}

Q_DECL_EXPORT int
hpsjam_sound_set_input_device(int)
{
	return (0);
}

Q_DECL_EXPORT int
hpsjam_sound_set_output_device(int)
{
	return (0);
}

Q_DECL_EXPORT bool
hpsjam_sound_is_input_device(int)
{
	return (true);
}

Q_DECL_EXPORT bool
hpsjam_sound_is_output_device(int)
{
	return (true);
}

Q_DECL_EXPORT int
hpsjam_sound_set_input_channel(int ch, int which)
{
	if (audioUnit == 0)
		return (-1);

	if (which > -1)
		audioInputSelection[ch] = which;

	audioInputSelection[ch] %= 2;

	return (audioInputSelection[ch]);
}

Q_DECL_EXPORT int
hpsjam_sound_set_output_channel(int ch, int which)
{
	if (audioUnit == 0)
		return (-1);

	if (which > -1)
		audioOutputSelection[ch] = which;

	audioOutputSelection[ch] %= 2;

	return (audioOutputSelection[ch]);
}

Q_DECL_EXPORT int
hpsjam_sound_max_input_channel()
{
	return (2);
}

Q_DECL_EXPORT int
hpsjam_sound_max_output_channel()
{
	return (2);
}

Q_DECL_EXPORT void
hpsjam_sound_get_input_status(QString &status)
{
	status = "Default audio input device";
}

Q_DECL_EXPORT void
hpsjam_sound_get_output_status(QString &status)
{
	status = "Default audio output device";
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

}

Q_DECL_EXPORT int
hpsjam_sound_max_devices()
{
	return (1);
}

Q_DECL_EXPORT QString
hpsjam_sound_get_device_name(int)
{
	return (QString("OBOE"));
}
