/*
 * This file is provided under a dual BSD/LGPLv2.1 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * BSD LICENSE
 *
 * tinyplay command line player for compress audio offload in alsa
 * Copyright (c) 2011-2012, Intel Corporation
 * All rights reserved.
 *
 * Author: Vinod Koul <vkoul@kernel.org>
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
 * tinyplay command line player for compress audio offload in alsa
 * Copyright (c) 2011-2012, Intel Corporation.
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
#include <signal.h>
#include <stdbool.h>
#include <getopt.h>
#include <sys/time.h>
#include <config.h>
#define __force
#define __bitwise
#define __user
#include "sound/compress_params.h"
#include "tinycompress/tinycompress.h"
#include "tinycompress/tinymp3.h"
#include "tinycompress/tinywave.h"

static int verbose;
static const unsigned int DEFAULT_CODEC_ID = SND_AUDIOCODEC_PCM;

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
/* BESPOKE isn't defined on older kernels */
#ifdef SND_AUDIOCODEC_BESPOKE
	{ "BESPOKE", SND_AUDIOCODEC_BESPOKE },
#endif
};
#define CPLAY_NUM_CODEC_IDS (sizeof(codec_ids) / sizeof(codec_ids[0]))

static void usage(void)
{
	int i;

	fprintf(stderr, "usage: cplay [OPTIONS] filename\n"
		"-c\tcard number\n"
		"-d\tdevice node\n"
		"-I\tspecify codec ID (default is mp3)\n"
		"-b\tbuffer size\n"
		"-f\tfragments\n\n"
		"-v\tverbose mode\n"
		"-h\tPrints this help list\n\n"
		"Example:\n"
		"\tcplay -c 1 -d 2 test.mp3\n"
		"\tcplay -f 5 test.mp3\n\n"
		"Valid codec IDs:\n");

	for (i = 0; i < CPLAY_NUM_CODEC_IDS; ++i)
		fprintf(stderr, "%s%c", codec_ids[i].name,
			((i + 1) % 8) ? ' ' : '\n');

	fprintf(stderr, "\nor the value in decimal or hex\n");

	exit(EXIT_FAILURE);
}

void play_samples(char *name, unsigned int card, unsigned int device,
		unsigned long buffer_size, unsigned int frag,
		unsigned long codec_id);

struct mp3_header {
	uint16_t sync;
	uint8_t format1;
	uint8_t format2;
};

int find_adts_header(FILE *file, unsigned int *num_channels, unsigned int *sample_rate, unsigned int *format)
{
	int ret;
	unsigned char buf[5];

	ret = fread(buf, sizeof(buf), 1, file);
	if (ret < 0) {
		fprintf(stderr, "open file error: %d\n", ret);
		return 0;
	}
	fseek(file, 0, SEEK_SET);

	if ((buf[0] != 0xff) || (buf[1] & 0xf0 != 0xf0))
		return 0;
	/* mpeg id */
	switch (buf[1]>>3 & 0x1) {
	case 0x0:
		*format = SND_AUDIOSTREAMFORMAT_MP4ADTS;
		break;
	case 0x1:
		*format = SND_AUDIOSTREAMFORMAT_MP2ADTS;
		break;
	default:
		fprintf(stderr, "can't find stream format\n");
		break;
	}
	/* sample_rate */
	switch (buf[2]>>2 & 0xf) {
	case 0x0:
		*sample_rate = 96000;
		break;
	case 0x1:
		*sample_rate = 88200;
		break;
	case 0x2:
		*sample_rate = 64000;
		break;
	case 0x3:
		*sample_rate = 48000;
		break;
	case 0x4:
		*sample_rate = 44100;
		break;
	case 0x5:
		*sample_rate = 32000;
		break;
	case 0x6:
		*sample_rate = 24000;
		break;
	case 0x7:
		*sample_rate = 22050;
		break;
	case 0x8:
		*sample_rate = 16000;
		break;
	case 0x9:
		*sample_rate = 12000;
		break;
	case 0xa:
		*sample_rate = 11025;
		break;
	case 0xb:
		*sample_rate = 8000;
		break;
	case 0xc:
		*sample_rate = 7350;
		break;
	default:
		break;
	}
	/* channel */
	switch (((buf[2]&0x1) << 2) | (buf[3]>>6)) {
	case 1:
		*num_channels = 1;
		break;
	case 2:
		*num_channels = 2;
		break;
	case 3:
		*num_channels = 3;
		break;
	case 4:
		*num_channels = 4;
		break;
	case 5:
		*num_channels = 5;
		break;
	case 6:
		*num_channels = 6;
		break;
	case 7:
		*num_channels = 7;
		break;
	default:
		break;
	}
	return 1;
}

static const int aac_sample_rates[] = { 96000, 88200, 64000, 48000, 44100,
  32000, 24000, 22050, 16000, 12000, 11025, 8000, 7350
};

#define MAX_SR_NUM sizeof(aac_sample_rates)/sizeof(aac_sample_rates[0])

static int get_sample_rate_from_index(int sr_index)
{
	if (sr_index >= 0 && sr_index < MAX_SR_NUM)
		return aac_sample_rates[sr_index];

	return 0;
}

int find_adif_header(FILE *file, unsigned int *num_channels, unsigned int *sample_rate, unsigned int *format)
{
	int ret;
	unsigned char adif_id[4];
	unsigned char adif_header[20];
	int bitstream_type;
	int bitrate;
	int object_type;
	int sr_index;
	int skip_size = 0;

	ret = fread(adif_id, sizeof(unsigned char), 4, file);
	if (ret < 0) {
		fprintf(stderr, "read data from file err: %d\n", ret);
		return 0;
	}
	/* adif id */
	if ((adif_id[0] != 0x41) || (adif_id[1] != 0x44) ||
		(adif_id[2] != 0x49) || (adif_id[3] != 0x46))
		return 0;

	fread(adif_header, sizeof(unsigned char), 20, file);

	/* copyright string */
	if (adif_header[0] & 0x80)
		skip_size = 9;

	bitstream_type = adif_header[0 + skip_size] & 0x10;
	bitrate =
		 ((unsigned int) (adif_header[0 + skip_size] & 0x0f) << 19) |
		 ((unsigned int) adif_header[1 + skip_size] << 11) |
		 ((unsigned int) adif_header[2 + skip_size] << 3) |
		 ((unsigned int) adif_header[3 + skip_size] & 0xe0);

	if (bitstream_type == 0) {
		object_type = ((adif_header[6 + skip_size] & 0x01) << 1) |
			((adif_header[7 + skip_size] & 0x80) >> 7);
		sr_index = (adif_header[7 + skip_size] & 0x78) >> 3;
	}
	/* VBR */
	else {
		object_type = (adif_header[4 + skip_size] & 0x18) >> 3;
		sr_index = ((adif_header[4 + skip_size] & 0x07) << 1) |
			((adif_header[5 + skip_size] & 0x80) >> 7);
	}

	/* sample rate */
	*sample_rate = get_sample_rate_from_index(sr_index);

	/* FIXME: assume channels is 2 */
	*num_channels = 2;

	*format = SND_AUDIOSTREAMFORMAT_ADIF;
	fseek(file, 0, SEEK_SET);
	return 1;
}

static int parse_aac_header(FILE *file, unsigned int *num_channels, unsigned int *sample_rate, unsigned int *format)
{
	if (find_adts_header(file, num_channels, sample_rate, format))
		return 1;
	else if (find_adif_header(file, num_channels, sample_rate, format))
		return 1;
	else {
		fprintf(stderr, "can't find streams format\n");
		return 0;
	}

	return 1;
}

static int parse_mp3_header(struct mp3_header *header, unsigned int *num_channels,
		unsigned int *sample_rate, unsigned int *bit_rate)
{
	int ver_idx, mp3_version, layer, bit_rate_idx, sample_rate_idx, channel_idx;

	/* check sync bits */
	if ((header->sync & MP3_SYNC) != MP3_SYNC) {
		fprintf(stderr, "Error: Can't find sync word\n");
		return -1;
	}
	ver_idx = (header->sync >> 11) & 0x03;
	mp3_version = ver_idx == 0 ? MPEG25 : ((ver_idx & 0x1) ? MPEG1 : MPEG2);
	layer = 4 - ((header->sync >> 9) & 0x03);
	bit_rate_idx = ((header->format1 >> 4) & 0x0f);
	sample_rate_idx = ((header->format1 >> 2) & 0x03);
	channel_idx = ((header->format2 >> 6) & 0x03);

	if (sample_rate_idx == 3 || layer == 4 || bit_rate_idx == 15) {
		fprintf(stderr, "Error: Can't find valid header\n");
		return -1;
	}
	*num_channels = (channel_idx == MONO ? 1 : 2);
	*sample_rate = mp3_sample_rates[mp3_version][sample_rate_idx];
	*bit_rate = (mp3_bit_rates[mp3_version][layer - 1][bit_rate_idx]) * 1000;
	if (verbose)
		printf("%s: exit\n", __func__);
	return 0;
}

static int print_time(struct compress *compress)
{
	unsigned int avail;
	struct timespec tstamp;

	if (compress_get_hpointer(compress, &avail, &tstamp) != 0) {
		fprintf(stderr, "Error querying timestamp\n");
		fprintf(stderr, "ERR: %s\n", compress_get_error(compress));
		return -1;
	} else
		fprintf(stderr, "DSP played %jd.%jd\n", (intmax_t)tstamp.tv_sec, (intmax_t)tstamp.tv_nsec*1000);
	return 0;
}

int main(int argc, char **argv)
{
	char *file;
	unsigned long buffer_size = 0;
	int c, i;
	unsigned int card = 0, device = 0, frag = 0;
	unsigned int codec_id = SND_AUDIOCODEC_MP3;

	if (argc < 2)
		usage();

	verbose = 0;
	while ((c = getopt(argc, argv, "hvb:f:c:d:I:")) != -1) {
		switch (c) {
		case 'h':
			usage();
			break;
		case 'b':
			buffer_size = strtol(optarg, NULL, 0);
			break;
		case 'f':
			frag = strtol(optarg, NULL, 10);
			break;
		case 'c':
			card = strtol(optarg, NULL, 10);
			break;
		case 'd':
			device = strtol(optarg, NULL, 10);
			break;
		case 'I':
			if (optarg[0] == '0') {
				codec_id = strtol(optarg, NULL, 0);
			} else {
				for (i = 0; i < CPLAY_NUM_CODEC_IDS; ++i) {
					if (strcmp(optarg,
						   codec_ids[i].name) == 0) {
						codec_id = codec_ids[i].id;
						break;
					}
				}

				if (i == CPLAY_NUM_CODEC_IDS) {
					fprintf(stderr, "Unrecognised ID: %s\n",
						optarg);
					usage();
				}
			}
			break;
		case 'v':
			verbose = 1;
			break;
		default:
			exit(EXIT_FAILURE);
		}
	}
	if (optind >= argc)
		usage();

	file = argv[optind];

	play_samples(file, card, device, buffer_size, frag, codec_id);

	fprintf(stderr, "Finish Playing.... Close Normally\n");
	exit(EXIT_SUCCESS);
}

#if ENABLE_PCM
void get_codec_pcm(FILE *file, struct compr_config *config,
		   struct snd_codec *codec)
{
	size_t read;
	struct wave_header header;
	unsigned int channels, rate, format;

	read = fread(&header, 1, sizeof(header), file);
	if (read != sizeof(header)) {
		fprintf(stderr, "Unable to read header \n");
		fclose(file);
		exit(EXIT_FAILURE);
	}

	if (parse_wave_header(&header, &channels, &rate, &format) == -1) {
		fclose(file);
		exit(EXIT_FAILURE);
	}

	codec->id = SND_AUDIOCODEC_PCM;
	codec->ch_in = channels;
	codec->ch_out = channels;
	codec->sample_rate = rate;
	if (!codec->sample_rate) {
		fprintf(stderr, "invalid sample rate %d\n", rate);
		fclose(file);
		exit(EXIT_FAILURE);
	}
	codec->bit_rate = 0;
	codec->rate_control = 0;
	codec->profile = SND_AUDIOCODEC_PCM;
	codec->level = 0;
	codec->ch_mode = 0;
	codec->format = format;
}
#endif

void get_codec_aac(FILE *file, struct compr_config *config,
		struct snd_codec *codec)
{
	unsigned int channels, rate, format;

	if (parse_aac_header(file, &channels, &rate, &format) == 0) {
		fclose(file);
		exit(EXIT_FAILURE);
	};
	fseek(file, 0, SEEK_SET);

	codec->id = SND_AUDIOCODEC_AAC;
	codec->ch_in = channels;
	codec->ch_out = channels;
	codec->sample_rate = rate;
	codec->bit_rate = 0;
	codec->rate_control = 0;
	codec->profile = SND_AUDIOPROFILE_AAC;
	codec->level = 0;
	codec->ch_mode = 0;
	codec->format = format;
}

void get_codec_mp3(FILE *file, struct compr_config *config,
		struct snd_codec *codec)
{
	size_t read;
	struct mp3_header header;
	unsigned int channels, rate, bits;

	read = fread(&header, 1, sizeof(header), file);
	if (read != sizeof(header)) {
		fprintf(stderr, "Unable to read header \n");
		fclose(file);
		exit(EXIT_FAILURE);
	}

	if (parse_mp3_header(&header, &channels, &rate, &bits) == -1) {
		fclose(file);
		exit(EXIT_FAILURE);
	}

	codec->id = SND_AUDIOCODEC_MP3;
	codec->ch_in = channels;
	codec->ch_out = channels;
	codec->sample_rate = rate;
	if (!codec->sample_rate) {
		fprintf(stderr, "invalid sample rate %d\n", rate);
		fclose(file);
		exit(EXIT_FAILURE);
	}
	codec->bit_rate = bits;
	codec->rate_control = 0;
	codec->profile = 0;
	codec->level = 0;
	codec->ch_mode = 0;
	codec->format = 0;

	/* reset file cursor to start
	 *
	 * this is done because if we leave it as is
	 * the program will hang in a poll call waiting
	 * for ring buffer to empty out.
	 *
	 * this never actually happens because of the fact
	 * that the codec probably expects to receive the
	 * MP3 header along with its associated MP3 data
	 * so, if we leave the file cursor positioned at
	 * the first MP3 data then the codec will most
	 * likely hang because it's expecting to also get
	 * the MP3 header
	 */
	if (fseek(file, 0, SEEK_SET) < 0) {
		fprintf(stderr, "Failed to set cursor to start.\n");
		fclose(file);
		exit(EXIT_FAILURE);
	}
}

void get_codec_iec(FILE *file, struct compr_config *config,
		struct snd_codec *codec)
{
	codec->id = SND_AUDIOCODEC_IEC61937;
	/* FIXME: cannot get accurate ch_in, any channels may be accepted */
	codec->ch_in = 2;
	codec->ch_out = 2;
	codec->sample_rate = 0;
	codec->bit_rate = 0;
	codec->rate_control = 0;
	codec->profile = SND_AUDIOPROFILE_IEC61937_SPDIF;
	codec->level = 0;
	codec->ch_mode = 0;
	codec->format = 0;
}

void play_samples(char *name, unsigned int card, unsigned int device,
		unsigned long buffer_size, unsigned int frag,
		unsigned long codec_id)
{
	struct compr_config config;
	struct snd_codec codec;
	struct compress *compress;
	FILE *file;
	char *buffer;
	int size, num_read, wrote;

	if (verbose)
		printf("%s: entry\n", __func__);
	file = fopen(name, "rb");
	if (!file) {
		fprintf(stderr, "Unable to open file '%s'\n", name);
		exit(EXIT_FAILURE);
	}

	switch (codec_id) {
#if ENABLE_PCM
	case SND_AUDIOCODEC_PCM:
		get_codec_pcm(file, &config, &codec);
		break;
#endif
	case SND_AUDIOCODEC_AAC:
		get_codec_aac(file, &config, &codec);
		break;
	case SND_AUDIOCODEC_MP3:
		get_codec_mp3(file, &config, &codec);
		break;
	case SND_AUDIOCODEC_IEC61937:
		get_codec_iec(file, &config, &codec);
		break;
	default:
		fprintf(stderr, "codec ID %ld is not supported\n", codec_id);
		exit(EXIT_FAILURE);
	}

	if ((buffer_size != 0) && (frag != 0)) {
		config.fragment_size = buffer_size/frag;
		config.fragments = frag;
	} else {
		/* use driver defaults */
		config.fragment_size = 0;
		config.fragments = 0;
	}
	config.codec = &codec;

	compress = compress_open(card, device, COMPRESS_IN, &config);
	if (!compress || !is_compress_ready(compress)) {
		fprintf(stderr, "Unable to open Compress device %d:%d\n",
				card, device);
		fprintf(stderr, "ERR: %s\n", compress_get_error(compress));
		goto FILE_EXIT;
	};
	if (verbose)
		printf("%s: Opened compress device\n", __func__);
	size = config.fragments * config.fragment_size;
	buffer = malloc(size);
	if (!buffer) {
		fprintf(stderr, "Unable to allocate %d bytes\n", size);
		goto COMP_EXIT;
	}

	/* we will write frag fragment_size and then start */
	num_read = fread(buffer, 1, size, file);
	if (num_read > 0) {
		if (verbose)
			printf("%s: Doing first buffer write of %d\n", __func__, num_read);
		wrote = compress_write(compress, buffer, num_read);
		if (wrote < 0) {
			fprintf(stderr, "Error %d playing sample\n", wrote);
			fprintf(stderr, "ERR: %s\n", compress_get_error(compress));
			goto BUF_EXIT;
		}
		if (wrote != num_read) {
			/* TODO: Buffer pointer needs to be set here */
			fprintf(stderr, "We wrote %d, DSP accepted %d\n", num_read, wrote);
		}
	}
	printf("Playing file %s On Card %u device %u, with buffer of %d bytes\n",
			name, card, device, size);
	printf("Format %u Channels %u, %u Hz, Bit Rate %d\n",
			codec.id, codec.ch_in, codec.sample_rate, codec.bit_rate);

	compress_start(compress);
	if (verbose)
		printf("%s: You should hear audio NOW!!!\n", __func__);

	do {
		num_read = fread(buffer, 1, size, file);
		if (num_read > 0) {
			wrote = compress_write(compress, buffer, num_read);
			if (wrote < 0) {
				fprintf(stderr, "Error playing sample\n");
				fprintf(stderr, "ERR: %s\n", compress_get_error(compress));
				goto BUF_EXIT;
			}
			if (wrote != num_read) {
				/* TODO: Buffer pointer needs to be set here */
				fprintf(stderr, "We wrote %d, DSP accepted %d\n", num_read, wrote);
			}
			if (verbose) {
				print_time(compress);
				printf("%s: wrote %d\n", __func__, wrote);
			}
		}
	} while (num_read > 0);

	if (verbose)
		printf("%s: exit success\n", __func__);
	/* issue drain if it supports */
	compress_drain(compress);
	free(buffer);
	fclose(file);
	compress_close(compress);
	return;
BUF_EXIT:
	free(buffer);
COMP_EXIT:
	compress_close(compress);
FILE_EXIT:
	fclose(file);
	if (verbose)
		printf("%s: exit failure\n", __func__);
	exit(EXIT_FAILURE);
}

