/*-
 * Copyright (c) 2021-2022 Hans Petter Selasky.
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

#include "hpsjam.h"

#include <QString>

Q_DECL_EXPORT bool
hpsjam_sound_init(const char *, bool)
{
	return (false);			/* success */
}

Q_DECL_EXPORT void
hpsjam_sound_uninit()
{
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

Q_DECL_EXPORT int
hpsjam_sound_set_input_channel(int ch, int)
{
	return (ch);
}

Q_DECL_EXPORT int
hpsjam_sound_set_output_channel(int ch, int)
{
	return (ch);
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
hpsjam_sound_toggle_buffer_samples(int)
{
	return (-1);
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
	return (QString("Dummy"));
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
