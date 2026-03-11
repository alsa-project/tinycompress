//SPDX-License-Identifier: (LGPL-2.1-only OR BSD-3-Clause)

//
// WAVE helper functions
//
// Copyright 2021 NXP

#include <stdio.h>
#include <stdint.h>
#include <string.h>
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

/* WAVE_FORMAT_EXTENSIBLE sub-format GUIDs (first 2 bytes are the format tag) */
static const uint8_t pcm_subformat_guid[16] = {
	0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00,
	0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71
};

/**
 * get_default_channel_mask() - Get default WAV channel mask for a given
 *                              channel count (per Microsoft WAV specification)
 */
static unsigned int get_default_channel_mask(unsigned int channels)
{
	switch (channels) {
	case 1:
		return WAV_CHANNEL_MASK_MONO;
	case 2:
		return WAV_CHANNEL_MASK_STEREO;
	case 3:
		return WAV_SPEAKER_FRONT_LEFT | WAV_SPEAKER_FRONT_RIGHT |
		       WAV_SPEAKER_FRONT_CENTER;
	case 4:
		return WAV_SPEAKER_FRONT_LEFT | WAV_SPEAKER_FRONT_RIGHT |
		       WAV_SPEAKER_BACK_LEFT | WAV_SPEAKER_BACK_RIGHT;
	case 5:
		return WAV_SPEAKER_FRONT_LEFT | WAV_SPEAKER_FRONT_RIGHT |
		       WAV_SPEAKER_FRONT_CENTER |
		       WAV_SPEAKER_BACK_LEFT | WAV_SPEAKER_BACK_RIGHT;
	case 6:
		return WAV_CHANNEL_MASK_5_1;
	case 7:
		return WAV_SPEAKER_FRONT_LEFT | WAV_SPEAKER_FRONT_RIGHT |
		       WAV_SPEAKER_FRONT_CENTER | WAV_SPEAKER_LOW_FREQUENCY |
		       WAV_SPEAKER_BACK_LEFT | WAV_SPEAKER_BACK_RIGHT |
		       WAV_SPEAKER_BACK_CENTER;
	case 8:
		return WAV_CHANNEL_MASK_7_1;
	default:
		return 0;
	}
}

static const char *speaker_name(unsigned int mask)
{
	switch (mask) {
	case WAV_SPEAKER_FRONT_LEFT:		return "FL";
	case WAV_SPEAKER_FRONT_RIGHT:		return "FR";
	case WAV_SPEAKER_FRONT_CENTER:		return "FC";
	case WAV_SPEAKER_LOW_FREQUENCY:		return "LFE";
	case WAV_SPEAKER_BACK_LEFT:		return "BL";
	case WAV_SPEAKER_BACK_RIGHT:		return "BR";
	case WAV_SPEAKER_FRONT_LEFT_OF_CENTER:	return "FLC";
	case WAV_SPEAKER_FRONT_RIGHT_OF_CENTER:	return "FRC";
	case WAV_SPEAKER_BACK_CENTER:		return "BC";
	case WAV_SPEAKER_SIDE_LEFT:		return "SL";
	case WAV_SPEAKER_SIDE_RIGHT:		return "SR";
	default:				return "?";
	}
}

static void print_channel_map(unsigned int channels, unsigned int channel_mask)
{
	unsigned int i, ch = 0;

	fprintf(stderr, "Channel map (%u ch, mask 0x%04x): ", channels,
		channel_mask);

	for (i = 0; i < 18 && ch < channels; i++) {
		unsigned int bit = 1u << i;

		if (channel_mask & bit) {
			if (ch > 0)
				fprintf(stderr, ", ");
			fprintf(stderr, "ch%u=%s", ch, speaker_name(bit));
			ch++;
		}
	}
	fprintf(stderr, "\n");
}

int parse_wave_file(FILE *file, unsigned int *channels, unsigned int *rate,
		    unsigned int *format, unsigned int *channel_mask)
{
	struct riff_chunk chunk;
	char riff_format[4];
	uint16_t fmt_type, fmt_channels, fmt_blockalign, fmt_samplebits;
	uint32_t fmt_rate, fmt_byterate;
	uint16_t fmt_cb_size;
	uint16_t fmt_valid_bits;
	uint32_t fmt_channel_mask;
	uint8_t fmt_subformat[16];
	uint32_t fmt_chunk_size;
	long fmt_end;
	int found_fmt = 0, found_data = 0;
	size_t nread;

	/* Seek to beginning */
	if (fseek(file, 0, SEEK_SET) < 0) {
		fprintf(stderr, "Failed to seek to start of file\n");
		return -1;
	}

	/* Read RIFF header */
	nread = fread(&chunk, 1, sizeof(chunk), file);
	if (nread != sizeof(chunk)) {
		fprintf(stderr, "Failed to read RIFF header\n");
		return -1;
	}

	if (strncmp(chunk.desc, "RIFF", 4) != 0) {
		fprintf(stderr, "RIFF magic not found\n");
		return -1;
	}

	nread = fread(riff_format, 1, sizeof(riff_format), file);
	if (nread != sizeof(riff_format)) {
		fprintf(stderr, "Failed to read RIFF format\n");
		return -1;
	}

	if (strncmp(riff_format, "WAVE", 4) != 0) {
		fprintf(stderr, "WAVE magic not found\n");
		return -1;
	}

	/* Scan chunks until we find both "fmt " and "data" */
	while (!found_data) {
		nread = fread(&chunk, 1, sizeof(chunk), file);
		if (nread != sizeof(chunk)) {
			fprintf(stderr, "Unexpected end of file while scanning chunks\n");
			return -1;
		}

		if (strncmp(chunk.desc, "fmt ", 4) == 0) {
			fmt_chunk_size = chunk.size;
			fmt_end = ftell(file) + fmt_chunk_size;

			/* Read basic fmt fields (16 bytes) */
			if (fmt_chunk_size < 16) {
				fprintf(stderr, "fmt chunk too small (%u)\n",
					fmt_chunk_size);
				return -1;
			}

			nread = fread(&fmt_type, 1, sizeof(fmt_type), file);
			nread += fread(&fmt_channels, 1, sizeof(fmt_channels), file);
			nread += fread(&fmt_rate, 1, sizeof(fmt_rate), file);
			nread += fread(&fmt_byterate, 1, sizeof(fmt_byterate), file);
			nread += fread(&fmt_blockalign, 1, sizeof(fmt_blockalign), file);
			nread += fread(&fmt_samplebits, 1, sizeof(fmt_samplebits), file);
			if (nread != 16) {
				fprintf(stderr, "Failed to read fmt fields\n");
				return -1;
			}

			fmt_channel_mask = 0;

			if (fmt_type == WAVE_FORMAT_EXTENSIBLE) {
				/* Need at least 2 (cbSize) + 2 (validBits) +
				 * 4 (channelMask) + 16 (subformat) = 24 extra bytes
				 */
				if (fmt_chunk_size < 40) {
					fprintf(stderr,
						"WAVE_FORMAT_EXTENSIBLE fmt chunk too small (%u, need 40)\n",
						fmt_chunk_size);
					return -1;
				}

				nread = fread(&fmt_cb_size, 1, sizeof(fmt_cb_size), file);
				nread += fread(&fmt_valid_bits, 1, sizeof(fmt_valid_bits), file);
				nread += fread(&fmt_channel_mask, 1, sizeof(fmt_channel_mask), file);
				nread += fread(fmt_subformat, 1, sizeof(fmt_subformat), file);
				if (nread != 24) {
					fprintf(stderr,
						"Failed to read extensible fmt fields\n");
					return -1;
				}

				/* Verify subformat GUID is PCM */
				if (memcmp(fmt_subformat, pcm_subformat_guid,
					   sizeof(pcm_subformat_guid)) != 0) {
					fprintf(stderr,
						"Unsupported subformat (not PCM)\n");
					return -1;
				}

				/* Use valid bits for format selection */
				fmt_samplebits = fmt_valid_bits;

				fprintf(stderr,
					"WAVE_FORMAT_EXTENSIBLE: %u ch, %u Hz, %u bits, channel mask 0x%04x\n",
					fmt_channels, fmt_rate, fmt_samplebits,
					fmt_channel_mask);
			} else if (fmt_type == WAVE_FORMAT_PCM) {
				/* Basic PCM - use default channel mask */
				fmt_channel_mask = get_default_channel_mask(fmt_channels);

				if (fmt_channels > 2)
					fprintf(stderr,
						"Basic PCM with %u channels, using default channel mask 0x%04x\n",
						fmt_channels, fmt_channel_mask);
			} else {
				fprintf(stderr,
					"Unsupported WAVE format type: 0x%04x\n",
					fmt_type);
				return -1;
			}

			/* Seek to end of fmt chunk (skip any remaining bytes) */
			if (fseek(file, fmt_end, SEEK_SET) < 0) {
				fprintf(stderr, "Failed to seek past fmt chunk\n");
				return -1;
			}

			found_fmt = 1;

		} else if (strncmp(chunk.desc, "data", 4) == 0) {
			if (!found_fmt) {
				fprintf(stderr,
					"data chunk found before fmt chunk\n");
				return -1;
			}
			/* File pointer is now at start of audio data */
			found_data = 1;

		} else {
			/* Unknown chunk - skip it */
			if (fseek(file, chunk.size, SEEK_CUR) < 0) {
				fprintf(stderr,
					"Failed to skip chunk '%.4s' (%u bytes)\n",
					chunk.desc, chunk.size);
				return -1;
			}
		}
	}

	*channels = fmt_channels;
	*rate = fmt_rate;
	*channel_mask = fmt_channel_mask;

	switch (fmt_samplebits) {
	case 8:
		*format = SNDRV_PCM_FORMAT_U8;
		break;
	case 16:
		*format = SNDRV_PCM_FORMAT_S16_LE;
		break;
	case 24:
		*format = SNDRV_PCM_FORMAT_S24_LE;
		break;
	case 32:
		*format = SNDRV_PCM_FORMAT_S32_LE;
		break;
	default:
		fprintf(stderr, "Unsupported sample bits %d\n", fmt_samplebits);
		return -1;
	}

	print_channel_map(fmt_channels, fmt_channel_mask);

	return 0;
}
