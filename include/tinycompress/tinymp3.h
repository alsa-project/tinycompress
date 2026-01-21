/* SPDX-License-Identifier: (LGPL-2.1-only OR BSD-3-Clause) */
/* Copyright (c) 2011-2012, Intel Corporation */
/* Author: Vinod Koul <vkoul@kernel.org> */

#ifndef __TINYMP3_H
#define __TINYMP3_H

#if defined(__cplusplus)
extern "C" {
#endif


#define MP3_SYNC 0xe0ff

static const int mp3_sample_rates[3][3] = {
	{44100, 48000, 32000},        /* MPEG-1 */
	{22050, 24000, 16000},        /* MPEG-2 */
	{11025, 12000,  8000},        /* MPEG-2.5 */
};

static const int mp3_bit_rates[3][3][15] = {
	{
		/* MPEG-1 */
		{  0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448}, /* Layer 1 */
		{  0, 32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384}, /* Layer 2 */
		{  0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320}, /* Layer 3 */
	},
	{
		/* MPEG-2 */
		{  0, 32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 256}, /* Layer 1 */
		{  0,  8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160}, /* Layer 2 */
		{  0,  8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160}, /* Layer 3 */
	},
	{
		/* MPEG-2.5 */
		{  0, 32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 256}, /* Layer 1 */
		{  0,  8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160}, /* Layer 2 */
		{  0,  8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160}, /* Layer 3 */
	},
};

enum mpeg_version {
	MPEG1  = 0,
	MPEG2  = 1,
	MPEG25 = 2
};

enum mp3_stereo_mode {
	STEREO = 0x00,
	JOINT = 0x01,
	DUAL = 0x02,
	MONO = 0x03
};

#endif
