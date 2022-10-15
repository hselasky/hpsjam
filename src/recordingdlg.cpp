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

#include "recordingdlg.h"
#include "peer.h"

#include <string.h>

#include <QDateTime>
#include <QStandardPaths>
#include <QByteArray>
#include <QString>
#include <QMessageBox>
#include <QDir>
#include <QMutexLocker>

static size_t
hpsjam_wav_header_length()
{
	size_t mod;
	size_t len;

	mod = HPSJAM_CHANNELS * HPSJAM_SAMPLE_BYTES;

	/* align to next sample */
	len = 44 + mod - 1;
	len -= len % mod;

	return (len);
}

static size_t
hpsjam_generate_wav_header(uint8_t *ptr, size_t max)
{
	size_t mod;
	size_t len;
	size_t buflen;

	mod = HPSJAM_CHANNELS * HPSJAM_SAMPLE_BYTES;

	/* align to next sample */
	len = 44 + mod - 1;
	len -= len % mod;

	if (max != len)
		return (0);

	buflen = len;

	/* clear block */
	memset(ptr, 0, len);

	/* fill out data header */
	ptr[len - 8] = 'd';
	ptr[len - 7] = 'a';
	ptr[len - 6] = 't';
	ptr[len - 5] = 'a';

	/* magic for unspecified length */
	ptr[len - 4] = 0x00;
	ptr[len - 3] = 0xF0;
	ptr[len - 2] = 0xFF;
	ptr[len - 1] = 0x7F;

	/* fill out header */
	*ptr++ = 'R';
	*ptr++ = 'I';
	*ptr++ = 'F';
	*ptr++ = 'F';

	/* total chunk size - unknown */

	*ptr++ = 0;
	*ptr++ = 0;
	*ptr++ = 0;
	*ptr++ = 0;

	*ptr++ = 'W';
	*ptr++ = 'A';
	*ptr++ = 'V';
	*ptr++ = 'E';
	*ptr++ = 'f';
	*ptr++ = 'm';
	*ptr++ = 't';
	*ptr++ = ' ';

	/* make sure header fits in PCM block */
	len -= 28;

	*ptr++ = len;
	*ptr++ = len >> 8;
	*ptr++ = len >> 16;
	*ptr++ = len >> 24;

	/* audioformat = PCM */

	*ptr++ = 0x01;
	*ptr++ = 0x00;

	/* number of channels */

	len = HPSJAM_CHANNELS;

	*ptr++ = len;
	*ptr++ = len >> 8;

	/* sample rate */

	len = HPSJAM_SAMPLE_RATE;

	*ptr++ = len;
	*ptr++ = len >> 8;
	*ptr++ = len >> 16;
	*ptr++ = len >> 24;

	/* byte rate */

	len = HPSJAM_SAMPLE_RATE * HPSJAM_CHANNELS * HPSJAM_SAMPLE_BYTES;

	*ptr++ = len;
	*ptr++ = len >> 8;
	*ptr++ = len >> 16;
	*ptr++ = len >> 24;

	/* block align */

	len = HPSJAM_CHANNELS * HPSJAM_SAMPLE_BYTES;

	*ptr++ = len;
	*ptr++ = len >> 8;

	/* bits per sample */

	len = HPSJAM_SAMPLE_BYTES * 8;

	*ptr++ = len;
	*ptr++ = len >> 8;

	return (buflen);
}

void
HpsJamFileListBox :: refresh()
{
	QDir dir(QStandardPaths::writableLocation(QStandardPaths::MusicLocation));

	if (!dir.exists()) {
		dir.mkpath(".");
		if (!dir.exists())
			return;
	}
	dir.setFilter(QDir::Files | QDir::NoSymLinks);
	dir.setSorting(QDir::Name);

	const QString str(textCursor().selectedText());

	QFileInfoList list = dir.entryInfoList();
	QString result;
	int select = -1;

	for (int i = 0; i != list.size(); i++) {
		const QString fname(list.at(i).fileName());
		if (fname.isEmpty() ||
		    fname[0] != QChar('H') ||
		    fname[1] != QChar('p') ||
		    fname[2] != QChar('s') ||
		    fname[3] != QChar('J') ||
		    fname[4] != QChar('a') ||
		    fname[5] != QChar('m') ||
		    fname[6] != QChar('-') ||
		    fname[fname.size() - 1] != QChar('v') ||
		    fname[fname.size() - 2] != QChar('a') ||
		    fname[fname.size() - 3] != QChar('w') ||
		    fname[fname.size() - 4] != QChar('.'))
			continue;
		result += fname;
		result += "\n";
		if (select == -1 && str == fname)
			select = i;
	}

	setPlainText(result);

	if (select != -1)
		updateSelection(select);

}

void
HpsJamFileListBox :: updateSelection(int which)
{
	QTextCursor c = textCursor();
	int old = c.blockNumber();

	if (which > -1) {
		c.movePosition(QTextCursor::Start, QTextCursor::MoveAnchor);
		c.movePosition(QTextCursor::NextBlock, QTextCursor::MoveAnchor, which);
	}
	c.movePosition(QTextCursor::StartOfLine, QTextCursor::MoveAnchor);
	c.movePosition(QTextCursor::EndOfLine, QTextCursor::KeepAnchor);

	if (which == -1 || old != c.blockNumber()) {
		QString s = c.selectedText();
		if (!s.isEmpty())
			emit valueChanged(s);
	}
	setTextCursor(c);
}

void
HpsJamRecording :: handle_start_record()
{
	recording.b_start.setFlat(true);
	recording.b_stop.setFlat(false);
}

void
HpsJamRecording :: handle_stop_record()
{
	recording.b_start.setFlat(false);
	recording.b_stop.setFlat(true);
}

void
HpsJamRecording :: handle_start_playback()
{
	playback.b_start.setFlat(true);
	playback.b_stop.setFlat(false);
}

void
HpsJamRecording :: handle_stop_playback()
{
	playback.b_start.setFlat(false);
	playback.b_stop.setFlat(true);
}

void
HpsJamRecording :: handle_slider_value(int _value)
{
	if (playbuf.file == 0 || playbuf.filesize == 0)
		return;

	const off_t hlen = hpsjam_wav_header_length();
	off_t off = playbuf.filesize * (off_t)_value / (off_t)999;
	off -= off % hlen;
	if (off == 0)
		off = hlen;
	::fseek(playbuf.file, off, SEEK_SET);
}

FILE *
HpsJamRecording :: open(const QString &str, const char *mod, off_t &filesize)
{
	FILE *retval;
	const QString fname(
	    QStandardPaths::writableLocation(QStandardPaths::MusicLocation) +
	    QString("/") + str);
	QByteArray ba(fname.toUtf8());
	filesize = 0;

	retval = ::fopen(ba.data(), mod);

	if (retval == 0) {
		QMessageBox::information(this, tr("ERROR"),
		    tr("Cannot open file %1: %2").arg(fname).arg(QString(strerror(errno))));
	} else {
		const size_t hlen = hpsjam_wav_header_length();
		uint8_t ref[hlen];

		if (hpsjam_generate_wav_header(ref, hlen) == 0) {
			::fclose(retval);
			retval = 0;
		} else if (mod[0] != 'w') {
			uint8_t buffer[hlen];

			if (::fread(buffer, sizeof(buffer), 1, retval) != 1 ||
			    ::bcmp(buffer, ref, hlen) != 0) {
				QMessageBox::information(this, tr("ERROR"),
				    tr("Cannot open file %1 or unsupported audio format").arg(fname));
				::fclose(retval);
				retval = 0;
			} else {
				::fseek(retval, 0, SEEK_END);
				filesize = ::ftello(retval);
				if (filesize < (off_t)hlen) {
					::fclose(retval);
					retval = 0;
				} else {
					filesize -= hlen;
					if (filesize == 0) {
						::fclose(retval);
						retval = 0;
					} else {
						::fseek(retval, hlen, SEEK_SET);
					}
				}
			}
		} else {
			if (::fwrite(ref, hlen, 1, retval) != 1) {
				::fclose(retval);
				retval = 0;
			}
		}
	}
	return (retval);
}

QString
HpsJamRecording :: newFileName()
{
	QDateTime date = QDateTime::currentDateTime();

	return (QString("HpsJam-") +
		date.toString("dd-MM-yyyy--hh-mm-ss") +
		QString(".wav"));
}

void
HpsJamRecording :: handle_watchdog()
{
	if (recording.b_start.isFlat()) {
		if (recbuf.file == 0) {
			recbuf.file = open(newFileName(), "w", recbuf.filesize);
			if (recbuf.file != 0)
				filelist.refresh();
			else
				handle_stop_record();
		}
	}
	if (recbuf.file != 0) {
		const size_t max = recbuf.pull_max();
		if (max != 0) {
			int32_t buffer[max];
			recbuf.pull_samples_s32_le(buffer, max);
			if (::fwrite(buffer, sizeof(buffer[0]), max, recbuf.file) != max) {
				::fclose(recbuf.file);
				recbuf.reset();
				handle_stop_record();
			}
		}
	}
	if (!recording.b_start.isFlat()) {
		if (recbuf.file != 0) {
			::fflush(recbuf.file);
			::fclose(recbuf.file);
			recbuf.reset();
		}
	}

	if (playback.b_start.isFlat()) {
		if (playbuf.file == 0) {
			const QString str(filelist.textCursor().selectedText());
			if (!str.isEmpty())
				playbuf.file = open(str, "r", playbuf.filesize);
			handle_slider_value(playback.s_progress.value());

			if (playbuf.file == 0)
				handle_stop_playback();
		}
		if (playbuf.file != 0) {
			const size_t max = playbuf.push_max();
			if (max != 0) {
				int32_t buffer[max];
				size_t num = ::fread(buffer, sizeof(buffer[0]), max, playbuf.file);
				if (num > 0)
					playbuf.push_samples_s32_le(buffer, num);
			}
		}
	} else {
		if (playbuf.file != 0) {
			::fclose(playbuf.file);
			playbuf.reset();
			HPSJAM_NO_SIGNAL(playback.s_progress,setValue(0));
		}
	}

	if (playbuf.file != 0 && playbuf.filesize != 0) {
		const off_t hlen = hpsjam_wav_header_length();
		off_t off = ::ftello(playbuf.file);
		if (off >= hlen) {
			off = ((off - hlen) * 999) / playbuf.filesize;
			if (off > 999)
				off = 999;
			else if (off < 0)
				off = 0;
			HPSJAM_NO_SIGNAL(playback.s_progress,setValue((int)off));
		} else {
			HPSJAM_NO_SIGNAL(playback.s_progress,setValue(999));
		}
	} else {
		HPSJAM_NO_SIGNAL(playback.s_progress,setValue(0));
	}
}

size_t
HpsJamRecordingBuffer :: pull_add_samples(float *pl, float *pr, size_t num)
{
	size_t retval = 0;
	size_t next;

	while (retval != num &&
	       consumer_off != producer_off) {
		next = consumer_off + 2;
		pl[retval] += buffer[consumer_off + 0];
		pr[retval] += buffer[consumer_off + 1];
		retval++;
		if (next == (sizeof(buffer) / sizeof(float)))
			next = 0;
		consumer_off = next;
	}
	return (retval);
}

size_t
HpsJamRecordingBuffer :: pull_samples_s32_le(int32_t *ptr, size_t num)
{
	size_t retval = 0;
	size_t next;

	while (retval != num &&
	       consumer_off != producer_off) {
		next = consumer_off + 1;
		int32_t sample = buffer[consumer_off] * limit;
		uint8_t *p = (uint8_t *)(ptr + retval);
		p[0] = sample & 0xFF;
		p[1] = (sample >> 8) & 0xFF;
		p[2] = (sample >> 16) & 0xFF;
		p[3] = (sample >> 24) & 0xFF;
		retval++;
		if (next == (sizeof(buffer) / sizeof(float)))
			next = 0;
		consumer_off = next;
	}
	return (retval);
}

size_t
HpsJamRecordingBuffer :: push_samples(const float *pl, const float *pr, size_t num)
{
	size_t retval = 0;
	size_t next;

	while (retval != num) {
		next = producer_off + 2;
		if (next == (sizeof(buffer) / sizeof(float)))
			next = 0;
		if (consumer_off == next)
			break;
		buffer[producer_off + 0] = pl[retval];
		buffer[producer_off + 1] = pr[retval];
		retval ++;
		producer_off = next;
	}
	return (retval);
}

size_t
HpsJamRecordingBuffer :: push_samples_s32_le(const int32_t *ptr, size_t num)
{
	size_t retval = 0;
	size_t next;

	while (retval != num) {
		next = producer_off + 1;
		if (next == (sizeof(buffer) / sizeof(float)))
			next = 0;
		if (consumer_off == next)
			break;
		const uint8_t *p = (const uint8_t *)(ptr + retval);
		int32_t sample = p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
		if (sample > limit)
			sample = limit;
		else if (sample < -limit)
			sample = -limit;
		buffer[producer_off] = (float)sample / (float)limit;
		retval++;
		producer_off = next;
	}
	return (retval);
}

size_t
HpsJamRecordingBuffer :: pull_max() const
{
	QMutexLocker locker(&hpsjam_client_peer->lock);
	size_t retval = ((sizeof(buffer) / sizeof(float)) +
	    producer_off - consumer_off) % (sizeof(buffer) / sizeof(float));
	return (retval & ~1);
}

size_t
HpsJamRecordingBuffer :: push_max() const
{
	QMutexLocker locker(&hpsjam_client_peer->lock);
	size_t retval = ((sizeof(buffer) / sizeof(float)) +
	    consumer_off - producer_off - 2) % (sizeof(buffer) / sizeof(float));
	return (retval & ~1);
}
