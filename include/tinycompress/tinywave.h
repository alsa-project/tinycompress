/* SPDX-License-Identifier: (LGPL-2.1-only OR BSD-3-Clause) */
/*
 * wave header and parsing
 *
 * Copyright 2020 NXP
 */

#ifndef __TINYWAVE_H
#define __TINYWAVE_H

#include <stdio.h>

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

/* WAVE format types */
#define WAVE_FORMAT_PCM			0x0001
#define WAVE_FORMAT_EXTENSIBLE		0xFFFE

/* WAV channel mask - speaker position bit flags (Microsoft standard) */
#define WAV_SPEAKER_FRONT_LEFT			0x00000001
#define WAV_SPEAKER_FRONT_RIGHT			0x00000002
#define WAV_SPEAKER_FRONT_CENTER		0x00000004
#define WAV_SPEAKER_LOW_FREQUENCY		0x00000008
#define WAV_SPEAKER_BACK_LEFT			0x00000010
#define WAV_SPEAKER_BACK_RIGHT			0x00000020
#define WAV_SPEAKER_FRONT_LEFT_OF_CENTER	0x00000040
#define WAV_SPEAKER_FRONT_RIGHT_OF_CENTER	0x00000080
#define WAV_SPEAKER_BACK_CENTER			0x00000100
#define WAV_SPEAKER_SIDE_LEFT			0x00000200
#define WAV_SPEAKER_SIDE_RIGHT			0x00000400
#define WAV_SPEAKER_TOP_CENTER			0x00000800
#define WAV_SPEAKER_TOP_FRONT_LEFT		0x00001000
#define WAV_SPEAKER_TOP_FRONT_CENTER		0x00002000
#define WAV_SPEAKER_TOP_FRONT_RIGHT		0x00004000
#define WAV_SPEAKER_TOP_BACK_LEFT		0x00008000
#define WAV_SPEAKER_TOP_BACK_CENTER		0x00010000
#define WAV_SPEAKER_TOP_BACK_RIGHT		0x00020000

/* Standard channel masks for common multi-channel configurations */
#define WAV_CHANNEL_MASK_MONO		(WAV_SPEAKER_FRONT_CENTER)
#define WAV_CHANNEL_MASK_STEREO		(WAV_SPEAKER_FRONT_LEFT | \
					 WAV_SPEAKER_FRONT_RIGHT)
#define WAV_CHANNEL_MASK_5_1		(WAV_SPEAKER_FRONT_LEFT | \
					 WAV_SPEAKER_FRONT_RIGHT | \
					 WAV_SPEAKER_FRONT_CENTER | \
					 WAV_SPEAKER_LOW_FREQUENCY | \
					 WAV_SPEAKER_BACK_LEFT | \
					 WAV_SPEAKER_BACK_RIGHT)
#define WAV_CHANNEL_MASK_7_1		(WAV_SPEAKER_FRONT_LEFT | \
					 WAV_SPEAKER_FRONT_RIGHT | \
					 WAV_SPEAKER_FRONT_CENTER | \
					 WAV_SPEAKER_LOW_FREQUENCY | \
					 WAV_SPEAKER_BACK_LEFT | \
					 WAV_SPEAKER_BACK_RIGHT | \
					 WAV_SPEAKER_SIDE_LEFT | \
					 WAV_SPEAKER_SIDE_RIGHT)

void init_wave_header(struct wave_header *header, uint16_t channels,
		      uint32_t rate, uint16_t samplebits);
void size_wave_header(struct wave_header *header, uint32_t size);

int parse_wave_header(struct wave_header *header, unsigned int *channels,
		      unsigned int *rate, unsigned int *format);

/**
 * parse_wave_file() - Parse WAV file with proper chunk scanning
 * @file:         FILE pointer positioned at beginning of file
 * @channels:     output - number of channels
 * @rate:         output - sample rate in Hz
 * @format:       output - ALSA PCM format (SNDRV_PCM_FORMAT_*)
 * @channel_mask: output - WAV channel mask (speaker position bitmask),
 *                0 if not available (basic PCM format infers default mapping)
 *
 * Handles both basic PCM (type 0x0001) and WAVE_FORMAT_EXTENSIBLE (type 0xFFFE)
 * formats. Properly scans chunks to find the "data" chunk, leaving the file
 * pointer positioned at the start of the audio data.
 *
 * Returns 0 on success, -1 on error.
 */
int parse_wave_file(FILE *file, unsigned int *channels, unsigned int *rate,
		    unsigned int *format, unsigned int *channel_mask);
#endif
