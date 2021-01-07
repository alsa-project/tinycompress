/* SPDX-License-Identifier: (LGPL-2.1-only OR BSD-3-Clause) */
/*
 * wave header and parsing
 *
 * Copyright 2020 NXP
 */

#ifndef __TINYWAVE_H
#define __TINYWAVE_H

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

#endif
