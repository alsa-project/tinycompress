//SPDX-License-Identifier: (LGPL-2.1-only OR BSD-3-Clause)

//
// WAVE helper functions
//
// Copyright 2021 NXP

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <sound/asound.h>

#include "tinycompress/tinywave.h"

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

void init_wave_header(struct wave_header *header, uint16_t channels,
		      uint32_t rate, uint16_t samplebits)
{
	memcpy(header, &blank_wave_header, sizeof(blank_wave_header));

	header->fmt.channels = channels;
	header->fmt.rate = rate;
	header->fmt.byterate = channels * rate * (samplebits / 8);
	header->fmt.blockalign = channels * (samplebits / 8);
	header->fmt.samplebits = samplebits;
}

void size_wave_header(struct wave_header *header, uint32_t size)
{
	header->riff.chunk.size = sizeof(*header) -
				  sizeof(header->riff.chunk) + size;
	header->data.chunk.size = size;
}

int parse_wave_header(struct wave_header *header, unsigned int *channels,
		      unsigned int *rate, unsigned int *format)
{
	if (strncmp(header->riff.chunk.desc, "RIFF", 4) != 0) {
		fprintf(stderr, "RIFF magic not found\n");
		return -1;
	}

	if (strncmp(header->riff.format, "WAVE", 4) != 0) {
		fprintf(stderr, "WAVE magic not found\n");
		return -1;
	}

	if (strncmp(header->fmt.chunk.desc, "fmt", 3) != 0) {
		fprintf(stderr, "FMT section not found");
		return -1;
	}

	*channels = header->fmt.channels;
	*rate = header->fmt.rate;

	switch(header->fmt.samplebits) {
	case 8:
		*format = SNDRV_PCM_FORMAT_U8;
		break;
	case 16:
		*format = SNDRV_PCM_FORMAT_S16_LE;
		break;
	case 32:
		*format = SNDRV_PCM_FORMAT_S32_LE;
		break;
	default:
		fprintf(stderr, "Unsupported sample bits %d\n",
			header->fmt.samplebits);
		return -1;
	}

	return 0;
}
