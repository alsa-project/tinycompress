// SPDX-License-Identifier: (LGPL-2.1-only OR BSD-3-Clause)
/*
 * Copyright 2024 NXP
 */

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>

#include "sound/compress_params.h"
#include "sound/compress_offload.h"
#include "tinycompress/tinycompress.h"
#include "tinycompress/tinywave.h"

#include "alsa_wrap.h"

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#define DMA_BUF_SIZE  4096
#define MAP_BUF_SIZE (512 * 1024)

static int verbose;

struct audio_info {
	unsigned int card;
	unsigned int device;
	char *infile;
	char *outfile;
	unsigned int channels;
	unsigned int in_rate;
	unsigned short in_samplebits;
	unsigned short in_blockalign;
	unsigned int out_rate;
	unsigned short out_samplebits;
	unsigned short out_blockalign;
	snd_pcm_format_t in_format;
	snd_pcm_format_t out_format;
	unsigned int in_dmabuf_size;
};

static void usage(void)
{
	fprintf(stderr, "usage: caccel [OPTIONS]\n"
		"-c\tcard number\n"
		"-d\tdevice node\n"
		"-i\tinput wave file\n"
		"-o\toutput wave file\n"
		"-r\toutput rate\n"
		"-f\toutput format\n"
		"-b\tbuffer size\n"
		"-v\tverbose mode\n"
		"-h\tPrints this help list\n\n"
		"Example:\n"
		"\tcmemtomem -c 1 -d 2 -i input.wav -o output.wav\n"
		"Valid codec: SRC\n");
}

static int parse_arguments(int argc, const char *argv[], struct audio_info *info)
{
	int c, option_index;
	static const char short_options[] = "hvc:d:r:i:o:f:";
	static const struct option long_options[] = {
		{"help", 0, 0, 'h'},
		{"verbose", 0, 0, 'v'},
		{"card", 1, 0, 'c'},
		{"device", 1, 0, 'd'},
		{"inFile", 1, 0, 'i'},
		{"outFile", 1, 0, 'o'},
		{"outRate", 1, 0, 'r'},
		{"outFormat", 1, 0, 'f'},
		{0, 0, 0, 0}
	};

	if (argc < 3)
		usage();

	while ((c = getopt_long(argc, (char * const*)argv, short_options,
				long_options, &option_index)) != -1) {
		switch (c) {
		case 'c':
			info->card = strtol(optarg, NULL, 0);
			break;
		case 'd':
			info->device = strtol(optarg, NULL, 0);
			break;
		case 'i':
			info->infile = optarg;
			break;
		case 'o':
			info->outfile = optarg;
			break;
		case 'r':
			info->out_rate = strtol(optarg, NULL, 0);
			break;
		case 'f':
			info->out_format = alsa_snd_pcm_format_value(optarg);
			break;
		case 'h':
			usage();
			exit(EXIT_FAILURE);
		case 'v':
			verbose = 1;
			break;
		default:
			fprintf(stderr, "Unknown Command  -%c\n", c);
			exit(EXIT_FAILURE);
		}
	}

	return 0;
}

int main(int argc, const char *argv[])
{
	struct wave_header in_header;
	struct wave_header out_header;
	struct audio_info info;
	size_t read, written;
	struct compr_task task = {};
	struct compr_task_status status = {};
	struct compress *compress;
	struct compr_config config;
	struct snd_codec codec;
	FILE *fd_dst = NULL;
	FILE *fd_src = NULL;
	void *bufin_start;
	void *bufout_start;
	int length = 0;
	int err = 0;

	verbose = 0;
	memset(&info, 0, sizeof(struct audio_info));

	info.out_format = SNDRV_PCM_FORMAT_S16_LE;

	if (parse_arguments(argc, argv, &info) != 0)
		exit(EXIT_FAILURE);

	if (info.out_rate == 0) {
		fprintf(stderr, "invalid output rate %d\n", info.out_rate);
		exit(EXIT_FAILURE);
	}

	fd_dst = fopen(info.outfile, "wb+");
	if (fd_dst <= 0) {
		fprintf(stderr, "output file not found\n");
		goto err_dst_not_found;
	}

	fd_src = fopen(info.infile, "r");
	if (fd_src <= 0) {
		fprintf(stderr, "input file not found\n");
		goto err_src_not_found;
	}

	read = fread(&in_header, 1, sizeof(in_header), fd_src);
	if (read != sizeof(in_header)) {
		fprintf(stderr, "Unable to read header\n");
		goto err_header_read;
	}

	if (parse_wave_header(&in_header, &info.channels, &info.in_rate,
			      (unsigned int *)&info.in_format) == -1) {
		fprintf(stderr, "Unable to parse header\n");
		goto err_header_read;
	}

	info.in_blockalign = info.channels * in_header.fmt.samplebits / 8;
	info.in_dmabuf_size = (DMA_BUF_SIZE / info.in_blockalign) * info.in_blockalign;
	info.out_samplebits = alsa_snd_pcm_format_width(info.out_format);

	init_wave_header(&out_header, info.channels, info.out_rate, info.out_samplebits);

	written = fwrite(&out_header, 1, sizeof(out_header), fd_dst);
	if (written != sizeof(out_header)) {
		fprintf(stderr, "Error writing output file header: %s\n",
			strerror(errno));
		goto err_header_read;
	}

	codec.id = SND_AUDIOCODEC_PCM;
	codec.ch_in  = info.channels;
	codec.ch_out = info.channels;
	codec.format = info.in_format;
	codec.sample_rate = info.in_rate;
	codec.pcm_format = info.out_format;
	codec.options.src_d.out_sample_rate = info.out_rate;

	config.codec = &codec;
	compress = compress_open(info.card, info.device, COMPRESS_ACCEL, &config);
	if (!compress || !is_compress_ready(compress)) {
		fprintf(stderr, "Unable to open Compress device %d:%d\n",
				info.card, info.device);
		fprintf(stderr, "ERR: %s\n", compress_get_error(compress));
		goto err_compress_open;
	};

	err = compress_task_create(compress, &task);
	if (err < 0) {
		fprintf(stderr, "ERR: %s\n", compress_get_error(compress));
		goto err_task_create;
	}

	bufin_start = mmap(NULL, MAP_BUF_SIZE, PROT_READ | PROT_WRITE,
			   MAP_SHARED, task.input_fd, 0);
	if (bufin_start == MAP_FAILED) {
		fprintf(stderr, "Error mapping input buffer\n");
		goto err_mmap_in;
	}
	memset(bufin_start, 0, MAP_BUF_SIZE);

	bufout_start = mmap(NULL, MAP_BUF_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED,
			    task.output_fd, 0);
	if (bufout_start == MAP_FAILED) {
		fprintf(stderr, "Error mapping output buffer\n");
		goto err_mmap_out;
	}
	memset(bufout_start, 0, MAP_BUF_SIZE);

	if (verbose)
		printf("conversion is started\n");

	status.seqno = task.seqno;

	do {

		read = fread(bufin_start, 1, info.in_dmabuf_size, fd_src);
		if (read <= 0)
			break;

		task.input_size = read;

		err = compress_task_start(compress, &task);
		if (err < 0) {
			fprintf(stderr, "ERR: %s\n", compress_get_error(compress));
			goto err_process_err;
		}

		err = compress_task_status(compress, &status);
		if (err < 0) {
			fprintf(stderr, "ERR: %s\n", compress_get_error(compress));
			goto err_process_err;
		}

		err = compress_task_stop(compress, &task);
		if (err < 0) {
			fprintf(stderr, "ERR: %s\n", compress_get_error(compress));
			goto err_process_err;
		}

		written = fwrite(bufout_start, 1, status.output_size, fd_dst);
		if (written != (size_t)status.output_size) {
			fprintf(stderr, "Error writing output file: %s\n",
				strerror(errno));
			goto err_process_err;
		}

	} while (read > 0);

	fseek(fd_dst, 0L, SEEK_END);
	length = ftell(fd_dst);
	size_wave_header(&out_header, length - sizeof(out_header));
	fseek(fd_dst, 0L, SEEK_SET);
	fwrite(&out_header, 1, sizeof(out_header), fd_dst);

	if (verbose)
		printf("Conversion is finished\n");

err_process_err:
	munmap(bufout_start, MAP_BUF_SIZE);
err_mmap_out:
	munmap(bufin_start, MAP_BUF_SIZE);
err_mmap_in:
	if (compress_task_free(compress, &task) < 0)
		fprintf(stderr, "ERR: %s\n", compress_get_error(compress));
err_task_create:
	compress_close(compress);
err_compress_open:
err_header_read:
	fclose(fd_src);
err_src_not_found:
	fclose(fd_dst);
err_dst_not_found:
	return err;
}
