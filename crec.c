/*
 * BSD LICENSE
 *
 * crec command line recorder for compress audio record in alsa
 * Copyright (c) 2011-2012, Intel Corporation
 * Copyright (c) 2013-2014, Wolfson Microelectronic Ltd.
 * All rights reserved.
 *
 * Author: Vinod Koul <vinod.koul@linux.intel.com>
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
 * crec command line recorder for compress audio record in alsa
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

static const unsigned int DEFAULT_CHANNELS = 1;
static const unsigned int DEFAULT_RATE = 44100;
static const unsigned int DEFAULT_FORMAT = SNDRV_PCM_FORMAT_S16_LE;

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

const struct wave_header blank_wave_header = {
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

static void usage(void)
{
	fprintf(stderr, "usage: crec [OPTIONS] filename\n"
		"-c\tcard number\n"
		"-d\tdevice node\n"
		"-b\tbuffer size\n"
		"-f\tfragments\n"
		"-v\tverbose mode\n"
		"-l\tlength of record in seconds\n"
		"-h\tPrints this help list\n\n"
		"-C\tSpecify the number of channels (default %u)\n"
		"-R\tSpecify the sample rate (default %u)\n"
		"-F\tSpecify the format: S16_LE, S32_LE (default S16_LE)\n\n"
		"Example:\n"
		"\tcrec -c 1 -d 2 test.wav\n"
		"\tcrec -f 5 test.wav\n",
		DEFAULT_CHANNELS, DEFAULT_RATE);

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
		printf("DSP recorded %jd.%jd\n",
		       (intmax_t)tstamp.tv_sec, (intmax_t)tstamp.tv_nsec*1000);
	}
	return 0;
}

static int finish_record()
{
	struct wave_header header;
	int ret;
	size_t nread, written;

	if (!file)
		return -ENOENT;

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

void capture_samples(char *name, unsigned int card, unsigned int device,
		     unsigned long buffer_size, unsigned int frag,
		     unsigned int length, unsigned int rate,
		     unsigned int channels, unsigned int format)
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
		printf("%s: entry, reading %u bytes\n", __func__, length);

	file = open(name, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
	if (file == -1) {
		fprintf(stderr, "Unable to open file '%s'\n", name);
		exit(EXIT_FAILURE);
	}

	/* Write a header, will update with size once record is complete */
	init_wave_header(&header, channels, rate, samplebits);
	written = write(file, &header, sizeof(header));
	if (written != sizeof(header)) {
		fprintf(stderr, "Error writing output file header: %s\n",
			strerror(errno));
		goto file_exit;
	}

	memset(&codec, 0, sizeof(codec));
	memset(&config, 0, sizeof(config));
	codec.id = SND_AUDIOCODEC_PCM;
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
		printf("%s: Opened compress device\n", __func__);

	size = config.fragment_size;
	buffer = malloc(size * config.fragments);
	if (!buffer) {
		fprintf(stderr, "Unable to allocate %d bytes\n", size);
		goto comp_exit;
	}

	printf("Recording file %s On Card %u device %u, with buffer of %lu bytes\n",
	       name, card, device, buffer_size);
	printf("Format %u Channels %u, %u Hz\n",
	       codec.id, codec.ch_out, rate);

	compress_start(compress);

	if (verbose)
		printf("%s: Capturing audio NOW!!!\n", __func__);

	do {
		if (length && size > length - total_read)
			size = length - total_read;

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
				printf("%s: read %d\n", __func__, read);
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
		printf("%s: exit success\n", __func__);

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
		printf("%s: exit failure\n", __func__);

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
	int c;
	unsigned int card = 0, device = 0, frag = 0, length = 0;
	unsigned int rate = DEFAULT_RATE, channels = DEFAULT_CHANNELS;
	unsigned int format = DEFAULT_FORMAT;

	if (signal(SIGINT, sig_handler) == SIG_ERR) {
		fprintf(stderr, "Error registering signal handler\n");
		exit(EXIT_FAILURE);
	}

	if (argc < 2)
		usage();

	verbose = 0;
	while ((c = getopt(argc, argv, "hvl:R:C:F:b:f:c:d:")) != -1) {
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
		default:
			exit(EXIT_FAILURE);
		}
	}
	if (optind >= argc)
		usage();

	file = argv[optind];

	capture_samples(file, card, device, buffer_size, frag, length,
			rate, channels, format);

	printf("Finish capturing... Close Normally\n");
	exit(EXIT_SUCCESS);
}

