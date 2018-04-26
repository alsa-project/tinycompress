/*
 * This file is provided under a dual BSD/LGPLv2.1 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * BSD LICENSE
 *
 * crecord command line recorder for compress audio record in alsa
 * Copyright (c) 2011-2012, Intel Corporation
 * Copyright (c) 2013-2014, Wolfson Microelectronic Ltd.
 * All rights reserved.
 *
 * Author: Vinod Koul <vkoul@kernel.org>
 * Author: Charles Keepax <ckeepax@opensource.wolfsonmicro.com>
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
 * crecord command line recorder for compress audio record in alsa
 * Copyright (c) 2011-2012, Intel Corporation
 * Copyright (c) 2013-2014, Wolfson Microelectronic Ltd.
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
#include <sys/types.h>
#include <sys/stat.h>
#define __force
#define __bitwise
#define __user
#include "sound/compress_params.h"
#include "sound/compress_offload.h"
#include "tinycompress/tinycompress.h"

static int verbose;
static int file;
static FILE *finfo;
static bool streamed;

static const unsigned int DEFAULT_CHANNELS = 1;
static const unsigned int DEFAULT_RATE = 44100;
static const unsigned int DEFAULT_FORMAT = SNDRV_PCM_FORMAT_S16_LE;
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
#define CREC_NUM_CODEC_IDS (sizeof(codec_ids) / sizeof(codec_ids[0]))

struct riff_chunk {
	char desc[4];
	uint32_t size;
} __attribute__((__packed__));

struct wave_header {
	struct {
		struct riff_chunk chunk;
		char format[4];
	} __attribute__((__packed__)) riff;

	struct {
		struct riff_chunk chunk;
		uint16_t type;
		uint16_t channels;
		uint32_t rate;
		uint32_t byterate;
		uint16_t blockalign;
		uint16_t samplebits;
	} __attribute__((__packed__)) fmt;

	struct {
		struct riff_chunk chunk;
	} __attribute__((__packed__)) data;
} __attribute__((__packed__));

static const struct wave_header blank_wave_header = {
	.riff = {
		.chunk = {
			.desc = "RIFF",
		},
		.format = "WAVE",
	},
	.fmt = {
		.chunk = {
			.desc = "fmt ", /* Note the space is important here */
			.size = sizeof(blank_wave_header.fmt) -
				sizeof(blank_wave_header.fmt.chunk),
		},
		.type = 0x01,   /* PCM */
	},
	.data = {
		.chunk = {
			.desc = "data",
		},
	},
};

static void init_wave_header(struct wave_header *header, uint16_t channels,
			     uint32_t rate, uint16_t samplebits)
{
	memcpy(header, &blank_wave_header, sizeof(blank_wave_header));

	header->fmt.channels = channels;
	header->fmt.rate = rate;
	header->fmt.byterate = channels * rate * (samplebits / 8);
	header->fmt.blockalign = channels * (samplebits / 8);
	header->fmt.samplebits = samplebits;
}

static void size_wave_header(struct wave_header *header, uint32_t size)
{
	header->riff.chunk.size = sizeof(*header) -
				  sizeof(header->riff.chunk) + size;
	header->data.chunk.size = size;
}

static const char *codec_name_from_id(unsigned int id)
{
	static char hexname[12];
	int i;

	for (i = 0; i < CREC_NUM_CODEC_IDS; ++i) {
		if (codec_ids[i].id == id)
			return codec_ids[i].name;
	}

	snprintf(hexname, sizeof(hexname), "0x%x", id);
	return hexname; /* a static is safe because we're single-threaded */
}

static void usage(void)
{
	int i;

	fprintf(stderr, "usage: crecord [OPTIONS] [filename.wav]\n"
		"-c\tcard number\n"
		"-d\tdevice node\n"
		"-b\tbuffer size\n"
		"-f\tfragments\n"
		"-v\tverbose mode\n"
		"-l\tlength of record in seconds\n"
		"-h\tPrints this help list\n\n"
		"-C\tSpecify the number of channels (default %u)\n"
		"-R\tSpecify the sample rate (default %u)\n"
		"-F\tSpecify the format: S16_LE, S32_LE (default S16_LE)\n"
		"-I\tSpecify codec ID (default %s)\n\n"
		"If filename.wav is not given the output is written to stdout\n"
		"Only PCM data can be written to a WAV file.\n\n"
		"Example:\n"
		"\tcrecord -c 1 -d 2 test.wav\n"
		"\tcrecord -f 5 test.wav\n"
		"\tcrecord -I BESPOKE >raw.bin\n\n"
		"Valid codec IDs:\n",
		DEFAULT_CHANNELS, DEFAULT_RATE,
		codec_name_from_id(DEFAULT_CODEC_ID));

	for (i = 0; i < CREC_NUM_CODEC_IDS; ++i)
		fprintf(stderr, "%s%c", codec_ids[i].name,
		((i + 1) % 8) ? ' ' : '\n');

	fprintf(stderr, "\nor the value in decimal or hex\n");

	exit(EXIT_FAILURE);
}

static int print_time(struct compress *compress)
{
	unsigned int avail;
	struct timespec tstamp;

	if (compress_get_hpointer(compress, &avail, &tstamp) != 0) {
		fprintf(stderr, "Error querying timestamp\n");
		fprintf(stderr, "ERR: %s\n", compress_get_error(compress));
		return -1;
	} else {
		fprintf(finfo, "DSP recorded %jd.%jd\n",
		       (intmax_t)tstamp.tv_sec, (intmax_t)tstamp.tv_nsec*1000);
	}
	return 0;
}

static int finish_record(void)
{
	struct wave_header header;
	int ret;
	size_t nread, written;

	if (!file)
		return -ENOENT;

	/* can't rewind if streaming to stdout */
	if (streamed)
		return 0;

	/* Get amount of data written to file */
	ret = lseek(file, 0, SEEK_END);
	if (ret < 0)
		return -errno;

	written = ret;
	if (written < sizeof(header))
		return -ENOENT;
	written -= sizeof(header);

	/* Sync file header from file */
	ret = lseek(file, 0, SEEK_SET);
	if (ret < 0)
		return -errno;

	nread = read(file, &header, sizeof(header));
	if (nread != sizeof(header))
		return -errno;

	/* Update file header */
	ret = lseek(file, 0, SEEK_SET);
	if (ret < 0)
		return -errno;

	size_wave_header(&header, written);

	written = write(file, &header, sizeof(header));
	if (written != sizeof(header))
		return -errno;

	return 0;
}

static void capture_samples(char *name, unsigned int card, unsigned int device,
			    unsigned long buffer_size, unsigned int frag,
			    unsigned int length, unsigned int rate,
			    unsigned int channels, unsigned int format,
			    unsigned int codec_id)
{
	struct compr_config config;
	struct snd_codec codec;
	struct compress *compress;
	struct wave_header header;
	char *buffer;
	size_t written;
	int read, ret;
	unsigned int size, total_read = 0;
	unsigned int samplebits;

	switch (format) {
	case SNDRV_PCM_FORMAT_S32_LE:
		samplebits = 32;
		break;
	default:
		samplebits = 16;
		break;
	}

	/* Convert length from seconds to bytes */
	length = length * rate * (samplebits / 8) * channels;

	if (verbose)
		fprintf(finfo, "%s: entry, reading %u bytes\n", __func__, length);
        if (!name) {
                file = STDOUT_FILENO;
        } else {
	        file = open(name, O_RDWR | O_CREAT,
			    S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
	        if (file == -1) {
		       fprintf(stderr, "Unable to open file '%s'\n", name);
		       exit(EXIT_FAILURE);
	        }
        }

	/* Write a header, will update with size once record is complete */
        if (!streamed) {
	    init_wave_header(&header, channels, rate, samplebits);
	    written = write(file, &header, sizeof(header));
	    if (written != sizeof(header)) {
		fprintf(stderr, "Error writing output file header: %s\n",
			strerror(errno));
		goto file_exit;
	    }
        }

	memset(&codec, 0, sizeof(codec));
	memset(&config, 0, sizeof(config));
	codec.id = codec_id;
	codec.ch_in = channels;
	codec.ch_out = channels;
	codec.sample_rate = rate;
	if (!codec.sample_rate) {
		fprintf(stderr, "invalid sample rate %d\n", rate);
		goto file_exit;
	}
	codec.format = format;
	if ((buffer_size != 0) && (frag != 0)) {
		config.fragment_size = buffer_size/frag;
		config.fragments = frag;
	}
	config.codec = &codec;

	compress = compress_open(card, device, COMPRESS_OUT, &config);
	if (!compress || !is_compress_ready(compress)) {
		fprintf(stderr, "Unable to open Compress device %d:%d\n",
			card, device);
		fprintf(stderr, "ERR: %s\n", compress_get_error(compress));
		goto file_exit;
	};

	if (verbose)
		fprintf(finfo, "%s: Opened compress device\n", __func__);

	size = config.fragments * config.fragment_size;
	buffer = malloc(size);
	if (!buffer) {
		fprintf(stderr, "Unable to allocate %d bytes\n", size);
		goto comp_exit;
	}

	fprintf(finfo, "Recording file %s On Card %u device %u, with buffer of %u bytes\n",
	       name, card, device, size);
	fprintf(finfo, "Codec %u Format %u Channels %u, %u Hz\n",
	       codec.id, codec.format, codec.ch_out, rate);

	compress_start(compress);

	if (verbose)
		fprintf(finfo, "%s: Capturing audio NOW!!!\n", __func__);

	do {
		read = compress_read(compress, buffer, size);
		if (read < 0) {
			fprintf(stderr, "Error reading sample\n");
			fprintf(stderr, "ERR: %s\n", compress_get_error(compress));
			goto buf_exit;
		}
		if ((unsigned int)read != size) {
			fprintf(stderr, "We read %d, DSP sent %d\n",
				size, read);
		}

		if (read > 0) {
			total_read += read;

			written = write(file, buffer, read);
			if (written != (size_t)read) {
				fprintf(stderr, "Error writing output file: %s\n",
					strerror(errno));
				goto buf_exit;
			}
			if (verbose) {
				print_time(compress);
				fprintf(finfo, "%s: read %d\n", __func__, read);
			}
		}
	} while (!length || total_read < length);

	ret = compress_stop(compress);
	if (ret < 0) {
		fprintf(stderr, "Error closing stream\n");
		fprintf(stderr, "ERR: %s\n", compress_get_error(compress));
	}

	ret = finish_record();
	if (ret < 0) {
		fprintf(stderr, "Failed to finish header: %s\n", strerror(ret));
		goto buf_exit;
	}

	if (verbose)
		fprintf(finfo, "%s: exit success\n", __func__);

	free(buffer);
	close(file);
	file = 0;

	compress_close(compress);

	return;
buf_exit:
	free(buffer);
comp_exit:
	compress_close(compress);
file_exit:
	close(file);

	if (verbose)
		fprintf(finfo, "%s: exit failure\n", __func__);

	exit(EXIT_FAILURE);
}

static void sig_handler(int signum __attribute__ ((unused)))
{
	finish_record();

	if (file)
		close(file);

	_exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
	char *file;
	unsigned long buffer_size = 0;
	int c, i;
	unsigned int card = 0, device = 0, frag = 0, length = 0;
	unsigned int rate = DEFAULT_RATE, channels = DEFAULT_CHANNELS;
	unsigned int format = DEFAULT_FORMAT;
	unsigned int codec_id = DEFAULT_CODEC_ID;

	if (signal(SIGINT, sig_handler) == SIG_ERR) {
		fprintf(stderr, "Error registering signal handler\n");
		exit(EXIT_FAILURE);
	}

	if (argc < 1)
		usage();

	verbose = 0;
	while ((c = getopt(argc, argv, "hvl:R:C:F:I:b:f:c:d:")) != -1) {
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
		case 'v':
			verbose = 1;
			break;
		case 'l':
			length = strtol(optarg, NULL, 10);
			break;
		case 'R':
			rate = strtol(optarg, NULL, 10);
			break;
		case 'C':
			channels = strtol(optarg, NULL, 10);
			break;
		case 'F':
			if (strcmp(optarg, "S16_LE") == 0) {
				format = SNDRV_PCM_FORMAT_S16_LE;
			} else if (strcmp(optarg, "S32_LE") == 0) {
				format = SNDRV_PCM_FORMAT_S32_LE;
			} else {
				fprintf(stderr, "Unrecognised format: %s\n",
					optarg);
				usage();
			}
			break;
		case 'I':
			if (optarg[0] == '0') {
				codec_id = strtol(optarg, NULL, 0);
			} else {
				for (i = 0; i < CREC_NUM_CODEC_IDS; ++i) {
					if (strcmp(optarg,
						   codec_ids[i].name) == 0) {
						codec_id = codec_ids[i].id;
						break;
					}
				}

				if (i == CREC_NUM_CODEC_IDS) {
					fprintf(stderr, "Unrecognised ID: %s\n",
						optarg);
					usage();
				}
			}
			break;
		default:
			exit(EXIT_FAILURE);
		}
	}

	if (optind >= argc) {
		file = NULL;
		finfo = fopen("/dev/null", "w");
		streamed = true;
	} else if (codec_id == SND_AUDIOCODEC_PCM) {
		file = argv[optind];
		finfo = stdout;
		streamed = false;
	} else {
		fprintf(stderr, "ERROR: Only PCM can be written to a WAV file\n");
		exit(EXIT_FAILURE);
	}

	capture_samples(file, card, device, buffer_size, frag, length,
			rate, channels, format, codec_id);

	fprintf(finfo, "Finish capturing... Close Normally\n");

	exit(EXIT_SUCCESS);
}
