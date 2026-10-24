/*
 * This file is provided under a dual BSD/LGPLv2.1 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * BSD LICENSE
 *
 * ccaps command line capability query tool for compress audio offload in alsa
 * Copyright (c) 2024, Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * Neither the name of Intel Corporation nor the names of its contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 * LGPL LICENSE
 *
 * ccaps command line capability query tool for compress audio offload in alsa
 * Copyright (c) 2024, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU Lesser General Public License,
 * version 2.1, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to
 * the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <stdint.h>
#include <linux/types.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <sys/ioctl.h>
#include <config.h>
#define __force
#define __bitwise
#define __user
#include "sound/compress_params.h"
#include "sound/compress_offload.h"

static const struct {
	const char *name;
	unsigned int id;
} codec_ids[] = {
	{ "PCM", SND_AUDIOCODEC_PCM },
	{ "MP3", SND_AUDIOCODEC_MP3 },
	{ "AMR", SND_AUDIOCODEC_AMR },
	{ "AMRWB", SND_AUDIOCODEC_AMRWB },
	{ "AMRWBPLUS", SND_AUDIOCODEC_AMRWBPLUS },
	{ "AAC", SND_AUDIOCODEC_AAC },
	{ "WMA", SND_AUDIOCODEC_WMA },
	{ "REAL", SND_AUDIOCODEC_REAL },
	{ "VORBIS", SND_AUDIOCODEC_VORBIS },
	{ "FLAC", SND_AUDIOCODEC_FLAC },
	{ "IEC61937", SND_AUDIOCODEC_IEC61937 },
	{ "G723_1", SND_AUDIOCODEC_G723_1 },
	{ "G729", SND_AUDIOCODEC_G729 },
#ifdef SND_AUDIOCODEC_BESPOKE
	{ "BESPOKE", SND_AUDIOCODEC_BESPOKE },
#endif
#ifdef SND_AUDIOCODEC_ALAC
	{ "ALAC", SND_AUDIOCODEC_ALAC },
#endif
#ifdef SND_AUDIOCODEC_APE
	{ "APE", SND_AUDIOCODEC_APE },
#endif
};
#define CCAPS_NUM_CODEC_IDS (sizeof(codec_ids) / sizeof(codec_ids[0]))

static void usage(void)
{
	fprintf(stderr, "usage: ccaps [OPTIONS]\n"
		"-c\tcard number\n"
		"-d\tdevice node\n"
		"-D\tdirection: p(layback) or c(apture), default is autodetect\n"
		"-h\tPrints this help list\n\n"
		"Example:\n"
		"\tccaps -c 1 -d 2\n"
		"\tccaps -c 0 -d 0 -D c\n");

	exit(EXIT_FAILURE);
}

static const char *codec_name(unsigned int id)
{
	unsigned int i;

	for (i = 0; i < CCAPS_NUM_CODEC_IDS; i++)
		if (codec_ids[i].id == id)
			return codec_ids[i].name;

	return "UNKNOWN";
}

static const char *direction_name(unsigned int dir)
{
	switch (dir) {
	case SND_COMPRESS_PLAYBACK:
		return "playback";
	case SND_COMPRESS_CAPTURE:
		return "capture";
#ifdef SND_COMPRESS_ACCEL
	case SND_COMPRESS_ACCEL:
		return "accel";
#endif
	default:
		return "unknown";
	}
}

static void print_u32_list(const char *label, const __u32 *values,
			   unsigned int count, unsigned int max)
{
	unsigned int i;

	if (count > max)
		count = max;

	printf("    %s (%u):", label, count);
	for (i = 0; i < count; i++)
		printf(" %u", values[i]);
	printf("\n");
}

static void print_codec_caps(int fd, unsigned int codec)
{
	struct snd_compr_codec_caps ccaps;
	unsigned int i;

	memset(&ccaps, 0, sizeof(ccaps));
	ccaps.codec = codec;

	if (ioctl(fd, SNDRV_COMPRESS_GET_CODEC_CAPS, &ccaps)) {
		printf("    no codec descriptors (%s)\n", strerror(errno));
		return;
	}

	if (ccaps.num_descriptors > MAX_NUM_CODEC_DESCRIPTORS)
		ccaps.num_descriptors = MAX_NUM_CODEC_DESCRIPTORS;

	for (i = 0; i < ccaps.num_descriptors; i++) {
		struct snd_codec_desc *d = &ccaps.descriptor[i];

		printf("    descriptor %u:\n", i);
		printf("      max channels: %u\n", d->max_ch);
		print_u32_list("sample rates", d->sample_rates,
			       d->num_sample_rates, MAX_NUM_SAMPLE_RATES);
		print_u32_list("bit rates", d->bit_rate,
			       d->num_bitrates, MAX_NUM_BITRATES);
		printf("      rate control: 0x%08x%s%s\n", d->rate_control,
		       (d->rate_control & SND_RATECONTROLMODE_CONSTANTBITRATE) ? " CBR" : "",
		       (d->rate_control & SND_RATECONTROLMODE_VARIABLEBITRATE) ? " VBR" : "");
		printf("      profiles:     0x%08x\n", d->profiles);
		printf("      modes:        0x%08x\n", d->modes);
		printf("      formats:      0x%08x\n", d->formats);
		printf("      min buffer:   %u\n", d->min_buffer);
	}
}

static int open_device(unsigned int card, unsigned int device, int dir_flags)
{
	char fn[256];
	int fd;

	snprintf(fn, sizeof(fn), "/dev/snd/comprC%uD%u", card, device);

	if (dir_flags != -1)
		return open(fn, dir_flags);

	/* autodetect: the kernel derives the stream direction from the flags */
	fd = open(fn, O_WRONLY);
	if (fd < 0)
		fd = open(fn, O_RDONLY);

	return fd;
}

int main(int argc, char **argv)
{
	struct snd_compr_caps caps;
	unsigned int card = 0, device = 0;
	int dir_flags = -1;
	int version = 0;
	unsigned int i;
	int c, fd;

	while ((c = getopt(argc, argv, "hc:d:D:")) != -1) {
		switch (c) {
		case 'h':
			usage();
			break;
		case 'c':
			card = strtol(optarg, NULL, 10);
			break;
		case 'd':
			device = strtol(optarg, NULL, 10);
			break;
		case 'D':
			if (optarg[0] == 'p')
				dir_flags = O_WRONLY;
			else if (optarg[0] == 'c')
				dir_flags = O_RDONLY;
			else
				usage();
			break;
		default:
			exit(EXIT_FAILURE);
		}
	}

	fd = open_device(card, device, dir_flags);
	if (fd < 0) {
		fprintf(stderr, "Unable to open /dev/snd/comprC%uD%u: %s\n",
			card, device, strerror(errno));
		exit(EXIT_FAILURE);
	}

	if (ioctl(fd, SNDRV_COMPRESS_IOCTL_VERSION, &version))
		fprintf(stderr, "Unable to read protocol version: %s\n",
			strerror(errno));

	memset(&caps, 0, sizeof(caps));
	if (ioctl(fd, SNDRV_COMPRESS_GET_CAPS, &caps)) {
		fprintf(stderr, "Unable to get device caps: %s\n", strerror(errno));
		close(fd);
		exit(EXIT_FAILURE);
	}

	if (caps.num_codecs > MAX_NUM_CODECS)
		caps.num_codecs = MAX_NUM_CODECS;

	printf("Compress device hw:%u,%u\n", card, device);
	printf("  protocol version:  %d.%d.%d\n",
	       (version >> 16) & 0xffff, (version >> 8) & 0xff, version & 0xff);
	printf("  direction:         %s\n", direction_name(caps.direction));
	printf("  fragment size:     %u - %u bytes\n",
	       caps.min_fragment_size, caps.max_fragment_size);
	printf("  fragments:         %u - %u\n",
	       caps.min_fragments, caps.max_fragments);
	printf("  codecs:            %u\n", caps.num_codecs);

	for (i = 0; i < caps.num_codecs; i++) {
		printf("  [%u] %s (0x%x)\n", i, codec_name(caps.codecs[i]),
		       caps.codecs[i]);
		print_codec_caps(fd, caps.codecs[i]);
	}

	close(fd);
	exit(EXIT_SUCCESS);
}
