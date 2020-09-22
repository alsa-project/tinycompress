/*
 * This file is provided under a dual BSD/LGPLv2.1 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * BSD LICENSE
 *
 * tinycompress library for compress audio offload in alsa
 * Copyright (c) 2011-2012, Intel Corporation
 * All rights reserved.
 *
 * Author: Vinod Koul <vkoul@kernel.org>
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
 * tinycompress library for compress audio offload in alsa
 * Copyright (c) 2011-2012, Intel Corporation.
 *
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

#include <sys/time.h>
#include "tinycompress/tinycompress.h"
#include "tinycompress/compress_ops.h"

struct compress {
	struct compress_ops *ops;
	void *data;
};

extern struct compress_ops compress_hw_ops;

const char *compress_get_error(struct compress *compress)
{
	return compress->ops->get_error(compress->data);
}

int is_compress_running(struct compress *compress)
{
	return compress->ops->is_compress_running(compress->data);
}

int is_compress_ready(struct compress *compress)
{
	return compress->ops->is_compress_ready(compress->data);
}

struct compress *compress_open(unsigned int card, unsigned int device,
		unsigned int flags, struct compr_config *config)
{
	struct compress *compress;

	compress = calloc(1, sizeof(struct compress));
	if (!compress)
		return NULL;

	compress->ops = &compress_hw_ops;
	compress->data =  compress->ops->open(card, device, flags, config);
	if (compress->data == NULL) {
		free(compress);
		return NULL;
	}
	return compress;
}

void compress_close(struct compress *compress)
{
	compress->ops->close(compress->data);
	free(compress);
}

int compress_get_hpointer(struct compress *compress,
		unsigned int *avail, struct timespec *tstamp)
{
	return compress->ops->get_hpointer(compress->data, avail, tstamp);
}

int compress_get_tstamp(struct compress *compress,
			unsigned int *samples, unsigned int *sampling_rate)
{
	return compress->ops->get_tstamp(compress->data, samples, sampling_rate);
}

int compress_write(struct compress *compress, const void *buf, unsigned int size)
{
	return compress->ops->write(compress->data, buf, size);
}

int compress_read(struct compress *compress, void *buf, unsigned int size)
{
	return compress->ops->read(compress->data, buf, size);
}

int compress_start(struct compress *compress)
{
	return compress->ops->start(compress->data);
}

int compress_stop(struct compress *compress)
{
	return compress->ops->stop(compress->data);
}

int compress_pause(struct compress *compress)
{
	return compress->ops->pause(compress->data);
}

int compress_resume(struct compress *compress)
{
	return compress->ops->resume(compress->data);
}

int compress_drain(struct compress *compress)
{
	return compress->ops->drain(compress->data);
}

int compress_partial_drain(struct compress *compress)
{
	return compress->ops->partial_drain(compress->data);
}

int compress_next_track(struct compress *compress)
{
	return compress->ops->next_track(compress->data);
}

int compress_set_gapless_metadata(struct compress *compress,
	struct compr_gapless_mdata *mdata)
{
	return compress->ops->set_gapless_metadata(compress->data, mdata);
}

bool is_codec_supported(unsigned int card, unsigned int device,
		unsigned int flags, struct snd_codec *codec)
{
	struct compress_ops *ops = &compress_hw_ops;

	return ops->is_codec_supported(card, device, flags, codec);
}

void compress_set_max_poll_wait(struct compress *compress, int milliseconds)
{
	compress->ops->set_max_poll_wait(compress->data, milliseconds);
}

void compress_nonblock(struct compress *compress, int nonblock)
{
	compress->ops->set_nonblock(compress->data, nonblock);
}

int compress_wait(struct compress *compress, int timeout_ms)
{
	return compress->ops->wait(compress->data, timeout_ms);
}

