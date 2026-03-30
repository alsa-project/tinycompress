/*
 * This file is provided under a dual BSD/LGPLv2.1 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * BSD LICENSE
 *
 * sofprobeclient - SOF probe client for compress audio capture
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
 * sofprobeclient - SOF probe client for compress audio capture
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

#include "probes_demux.h"

static int verbose;
static int parser_debug;
static FILE *finfo;

static const unsigned int DEFAULT_CHANNELS = 4;
static const unsigned int DEFAULT_RATE = 48000;
static const unsigned int DEFAULT_FORMAT = SNDRV_PCM_FORMAT_S32_LE;
static const unsigned int DEFAULT_CODEC_ID = SND_AUDIOCODEC_PCM;

static struct dma_frame_parser *parser;

static void usage(void)
{
	fprintf(stderr, "usage: sofprobeclient [OPTIONS]\n"
		"-c\tcard number (default 3)\n"
		"-d\tdevice node (default 0)\n"
		"-b\tbuffer size (default 8192)\n"
		"-f\tfragments (default 4)\n"
		"-v\tverbose mode\n"
		"-D\tenable parser debug messages\n"
		"-l\tlength of record in seconds (0 = unlimited)\n"
		"-h\tPrints this help list\n\n"
		"-C\tSpecify the number of channels (default %u)\n"
		"-R\tSpecify the sample rate (default %u)\n"
		"-F\tSpecify the format: S16_LE, S32_LE (default S32_LE)\n\n"
		"Captured probe data is parsed in real time.\n"
		"Log output (non-audio probes) is written to stdout.\n"
		"Audio probe data is written to buffer_<id>.wav files\n"
		"in the current directory.\n\n"
		"Example:\n"
		"\tsofprobeclient\n"
		"\tsofprobeclient -c 1 -d 2\n",
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
		fprintf(finfo, "DSP recorded %jd.%jd\n",
		       (intmax_t)tstamp.tv_sec, (intmax_t)tstamp.tv_nsec*1000);
	}
	return 0;
}

static void capture_and_parse(unsigned int card, unsigned int device,
			      unsigned long buffer_size, unsigned int frag,
			      unsigned int length, unsigned int rate,
			      unsigned int channels, unsigned int format)
{
	struct compr_config config;
	struct snd_codec codec;
	struct compress *compress;
	char *buffer;
	int read, ret;
	unsigned int size, total_read = 0;
	unsigned int samplebits;
	uint8_t *parse_buf;
	size_t parse_len;

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

	memset(&codec, 0, sizeof(codec));
	memset(&config, 0, sizeof(config));
	codec.id = DEFAULT_CODEC_ID;
	codec.ch_in = channels;
	codec.ch_out = channels;
	codec.sample_rate = rate;
	if (!codec.sample_rate) {
		fprintf(stderr, "invalid sample rate %d\n", rate);
		return;
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
		return;
	};

	if (verbose)
		fprintf(finfo, "%s: Opened compress device\n", __func__);

	size = config.fragments * config.fragment_size;
	buffer = malloc(size);
	if (!buffer) {
		fprintf(stderr, "Unable to allocate %d bytes\n", size);
		goto comp_exit;
	}

	fprintf(finfo, "Capturing probes on Card %u device %u, buffer %u bytes\n",
		card, device, size);
	fprintf(finfo, "Codec %u Format %u Channels %u, %u Hz\n",
	       codec.id, codec.format, codec.ch_out, rate);

	compress_start(compress);

	if (verbose)
		fprintf(finfo, "%s: Capturing probe data NOW!!!\n", __func__);

	do {
		read = compress_read(compress, buffer, size);
		if (read < 0) {
			fprintf(stderr, "Error reading sample\n");
			fprintf(stderr, "ERR: %s\n", compress_get_error(compress));
			goto buf_exit;
		}
		if (parser_debug && (unsigned int)read != size) {
			fprintf(stderr, "We read %d, DSP sent %d\n",
				size, read);
		}

		if (read > 0) {
			int remaining = read;
			char *src = buffer;

			total_read += read;

			/* Feed captured data to the parser in chunks
			 * that fit its internal buffer.
			 */
			while (remaining > 0) {
				int chunk;

				parser_fetch_free_buffer(parser, &parse_buf, &parse_len);
				chunk = remaining < (int)parse_len ? remaining : (int)parse_len;
				memcpy(parse_buf, src, chunk);
				ret = parser_parse_data(parser, chunk);
				if (ret < 0) {
					fprintf(stderr, "Parser error %d, stopping\n", ret);
					goto buf_exit;
				}
				src += chunk;
				remaining -= chunk;
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

	/* Finalize any open wave files */
	finalize_wave_files(parser);

	if (verbose)
		fprintf(finfo, "%s: exit success\n", __func__);

	free(buffer);
	compress_close(compress);

	return;
buf_exit:
	free(buffer);
comp_exit:
	compress_close(compress);

	if (verbose)
		fprintf(finfo, "%s: exit failure\n", __func__);

	exit(EXIT_FAILURE);
}

static void sig_handler(int signum __attribute__ ((unused)))
{
	/* Finalize wave files on signal */
	if (parser)
		finalize_wave_files(parser);

	_exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
	unsigned long buffer_size = 8192;
	int c;
	unsigned int card = 3, device = 0, frag = 4, length = 0;
	unsigned int rate = DEFAULT_RATE, channels = DEFAULT_CHANNELS;
	unsigned int format = DEFAULT_FORMAT;

	if (signal(SIGINT, sig_handler) == SIG_ERR) {
		fprintf(stderr, "Error registering signal handler\n");
		exit(EXIT_FAILURE);
	}

	/* Initialize the probe parser with log-to-stdout */
	parser = parser_init();
	if (!parser) {
		fprintf(stderr, "Failed to initialize probe parser\n");
		exit(EXIT_FAILURE);
	}
	parser_log_to_stdout(parser);

	verbose = 0;
	finfo = stderr;

	while ((c = getopt(argc, argv, "hvDl:R:C:F:b:f:c:d:")) != -1) {
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
		case 'D':
			parser_debug = 1;
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

	capture_and_parse(card, device, buffer_size, frag, length,
			  rate, channels, format);

	fprintf(finfo, "Finish capturing... Close Normally\n");

	parser_free(parser);
	exit(EXIT_SUCCESS);
}
