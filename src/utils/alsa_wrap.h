/* SPDX-License-Identifier: (LGPL-2.1-only OR BSD-3-Clause) */

#ifndef __ALSA_WRAP_API_H
#define __ALSA_WRAP_API_H

snd_pcm_format_t alsa_snd_pcm_format_value(const char *name);
int alsa_snd_pcm_format_physical_width(snd_pcm_format_t format);
int alsa_snd_pcm_format_width(snd_pcm_format_t format);
int alsa_snd_pcm_format_linear(snd_pcm_format_t format);
int alsa_snd_pcm_format_signed(snd_pcm_format_t format);
#endif

