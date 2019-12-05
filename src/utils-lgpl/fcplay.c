//SPDX-License-Identifier: LGPL-2.1-only

//Copyright (c) 2011-2012, Intel Corporation
//Copyright (c) 2018-2019, Linaro Ltd

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
#define __force
#define __user
#include "sound/compress_params.h"
#include "tinycompress/tinycompress.h"
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>

static int verbose;

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
		"\tfcplay -c 1 -d 2 test.mp3\n"
		"\tfcplay -f 5 test.mp3\n\n"
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

static int get_codec_id(int codec_id)
{
	switch (codec_id) {
	case AV_CODEC_ID_MP3:
		return SND_AUDIOCODEC_MP3;
	case AV_CODEC_ID_AAC:
		return SND_AUDIOCODEC_AAC;
	case AV_CODEC_ID_WMAV1:
	case AV_CODEC_ID_WMAV2:
		return SND_AUDIOCODEC_WMA;
	case AV_CODEC_ID_VORBIS:
		return SND_AUDIOCODEC_VORBIS;
	case AV_CODEC_ID_FLAC:
		return SND_AUDIOCODEC_FLAC;
	case AV_CODEC_ID_RA_144:
	case AV_CODEC_ID_RA_288:
		return SND_AUDIOCODEC_REAL;
	case AV_CODEC_ID_AMR_NB:
		return SND_AUDIOCODEC_AMR;
	case AV_CODEC_ID_AMR_WB:
		return SND_AUDIOCODEC_AMRWB;
	case AV_CODEC_ID_PCM_S16LE ... AV_CODEC_ID_PCM_S16BE_PLANAR:
		return SND_AUDIOCODEC_PCM;
	default:
		fprintf(stderr, "Not supported AVcodec: %d\n", codec_id);
		exit(EXIT_FAILURE);
	}
}

static int parse_file(char *file, struct snd_codec *codec)
{
	AVFormatContext *ctx = NULL;
	AVStream *stream;
	char errbuf[50];
	int err = 0, i, filled = 0;

	err = avformat_open_input(&ctx, file, NULL, NULL);
	if (err < 0) {
		av_strerror(err, errbuf, sizeof(errbuf));
		fprintf(stderr, "Unable to open %s: %s\n", file, errbuf);
		exit(EXIT_FAILURE);
	}

	err = avformat_find_stream_info(ctx, NULL);
	if (err < 0) {
		av_strerror(err, errbuf, sizeof(errbuf));
		fprintf(stderr, "Unable to identify %s: %s\n", file, errbuf);
		goto exit;
	}

	if (ctx->nb_streams < 1) {
		fprintf(stderr, "No streams found in %s\n", file);
		goto exit;
	}

	if (verbose)
		fprintf(stderr, "Streams: %d\n", ctx->nb_streams);

	for (i = 0; i < ctx->nb_streams; i++) {
		stream =  ctx->streams[i];

		if (verbose)
			fprintf(stderr, "StreamType: %d", stream->codecpar->codec_type);

		if (stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
			fprintf(stderr, "Stream:%d is audio type\n", i);

			if (!filled) {
				/* we fill params from 1st audio stream */
				codec->id = get_codec_id(stream->codecpar->codec_id);
				codec->ch_in = stream->codecpar->channels;
				codec->ch_out = stream->codecpar->channels;
				codec->sample_rate = stream->codecpar->sample_rate;
				codec->bit_rate = ctx->bit_rate;
				codec->profile = stream->codecpar->profile;
				codec->format = 0; /* need codec format type */
				codec->align = stream->codecpar->block_align;
				codec->level = 0;
				codec->rate_control = 0;
				codec->ch_mode = 0;
				filled = 1;

				if (codec->id == SND_AUDIOCODEC_FLAC) {
					codec->options.flac_d.sample_size = stream->codecpar->bits_per_raw_sample;
					/* use these values from libav/flac* where 16 is mind block size for
					 * flac and 64K max. 11 is min frame and avg is 8192, so take 4 times
					 * that
					 */
					codec->options.flac_d.min_blk_size = 16;
					codec->options.flac_d.max_blk_size = 65535;
					codec->options.flac_d.min_frame_size = 11;
					codec->options.flac_d.max_frame_size = 8192*4;
				}
			}

			if (verbose) {
				fprintf(stderr, "Stream:%d", i);
				fprintf(stderr, "  Codec: %d", stream->codecpar->codec_id);
				fprintf(stderr, "  Format: %d", stream->codecpar->format);
				fprintf(stderr, "  Bit Rate: %ld", stream->codecpar->bit_rate);
				fprintf(stderr, "  Bits coded: %d", stream->codecpar->bits_per_coded_sample);
				fprintf(stderr, "  Profile: %d", stream->codecpar->profile);
				fprintf(stderr, "  Codec tag: %d", stream->codecpar->codec_tag);
				fprintf(stderr, "  Channels: %d", stream->codecpar->channels);
				fprintf(stderr, "  Sample rate: %d", stream->codecpar->sample_rate);
				fprintf(stderr, "  block_align: %d", stream->codecpar->block_align);
				if (codec->id == SND_AUDIOCODEC_FLAC) {
					fprintf(stderr, "  Sample Size %d",  codec->options.flac_d.sample_size);
					fprintf(stderr, "  Min Block Size  %d",  codec->options.flac_d.min_blk_size);
					fprintf(stderr, "  Max Block Size  %d",  codec->options.flac_d.max_blk_size);
					fprintf(stderr, "  Min Frame Size  %d",  codec->options.flac_d.min_frame_size);
					fprintf(stderr, "  Max Frame Size  %d",  codec->options.flac_d.max_frame_size);

				}
				fprintf(stderr, "\n");
			}
		}
	}

	if (verbose)
		av_dump_format(ctx, 0, file, 0);

	avformat_close_input(&ctx);
	return 0;

exit:
	avformat_close_input(&ctx);
	exit(EXIT_FAILURE);

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

	memset(&codec, 0, sizeof(codec));
	memset(&config, 0, sizeof(config));

	parse_file(name, &codec);

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
	size = config.fragment_size;
	buffer = malloc(size * config.fragments);
	if (!buffer) {
		fprintf(stderr, "Unable to allocate %d bytes\n", size);
		goto COMP_EXIT;
	}

	/* we will write frag fragment_size and then start */
	num_read = fread(buffer, 1, size * config.fragments, file);
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
			/* TODO: Buufer pointer needs to be set here */
			fprintf(stderr, "We wrote %d, DSP accepted %d\n", num_read, wrote);
		}
	}
	printf("Playing file %s On Card %u device %u, with buffer of %lu bytes\n",
			name, card, device, buffer_size);
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
