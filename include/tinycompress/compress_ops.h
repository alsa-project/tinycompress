/* SPDX-License-Identifier: (LGPL-2.1-only OR BSD-3-Clause) */
/* Copyright (c) 2020 The Linux Foundation. All rights reserved. */

#ifndef __COMPRESS_OPS_H__
#define __COMPRESS_OPS_H__

#include "sound/compress_params.h"
#include "sound/compress_offload.h"
#include "tinycompress.h"

/*
 * struct compress_ops:
 * ops structure containing ops corresponding to exposed
 * compress APIs, needs to be implemented by plugin lib for
 * virtual compress nodes. Real compress node handling is
 * done in compress_hw.c
 */
struct compress_ops {
	void *(*open_by_name)(const char *name,
			unsigned int flags, struct compr_config *config);
	void (*close)(void *compress_data);
	int (*get_hpointer)(void *compress_data,
			unsigned int *avail, struct timespec *tstamp);
	int (*get_tstamp)(void *compress_data,
			unsigned int *samples, unsigned int *sampling_rate);
	int (*write)(void *compress_data, const void *buf, size_t size);
	int (*read)(void *compress_data, void *buf, size_t size);
	int (*start)(void *compress_data);
	int (*stop)(void *compress_data);
	int (*pause)(void *compress_data);
	int (*resume)(void *compress_data);
	int (*drain)(void *compress_data);
	int (*partial_drain)(void *compress_data);
	int (*next_track)(void *compress_data);
	int (*set_gapless_metadata)(void *compress_data,
			struct compr_gapless_mdata *mdata);
	void (*set_max_poll_wait)(void *compress_data, int milliseconds);
	void (*set_nonblock)(void *compress_data, int nonblock);
	int (*wait)(void *compress_data, int timeout_ms);
	bool (*is_codec_supported_by_name) (const char *name,
			unsigned int flags, struct snd_codec *codec);
	int (*is_compress_running)(void *compress_data);
	int (*is_compress_ready)(void *compress_data);
	const char *(*get_error)(void *compress_data);
	int (*set_codec_params)(void *compress_data, struct snd_codec *codec);
	int (*task_create)(void *compress_data, struct compr_task *task);
	int (*task_start)(void *compress_data, struct compr_task *task);
	int (*task_stop)(void *compress_data, struct compr_task *task);
	int (*task_free)(void *compress_data, struct compr_task *task);
	int (*task_status)(void *compress_data, struct compr_task_status *status);
};

#endif /* end of __COMPRESS_OPS_H__ */
