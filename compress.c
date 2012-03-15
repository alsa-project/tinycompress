/*
 * BSD LICENSE
 *
 * tinycompress library for compress audio offload in alsa
 * Copyright (c) 2011-2012, Intel Corporation
 * All rights reserved.
 *
 * Author: Vinod Koul <vinod.koul@linux.intel.com>
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

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <poll.h>
#include <stdbool.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <limits.h>

#include <linux/ioctl.h>
#define __force
#define __bitwise
#define __user
#include <sound/asound.h>
#include "sound/compress_params.h"
#include "sound/compress_offload.h"
#include "tinycompress/tinycompress.h"

#define COMPR_ERR_MAX 32

struct compress {
	int fd;
	unsigned int flags;
	__u64 buffer_size;
	char error[COMPR_ERR_MAX];
	struct compr_config *config;
	int running:1;
};

static int oops(struct compress *compress, int e, const char *fmt, ...)
{
	va_list ap;
	int sz;

	va_start(ap, fmt);
	vsnprintf(compress->error, COMPR_ERR_MAX, fmt, ap);
	va_end(ap);
	sz = strlen(compress->error);

	if (errno)
		snprintf(compress->error + sz, COMPR_ERR_MAX - sz,
			": %s", strerror(e));
	return e;
}

const char *compress_get_error(struct compress *compress)
{
	return compress->error;
}
static struct compress bad_compress = {
	.fd = -1,
};

int is_compress_running(struct compress *compress)
{
	return ((compress->fd > 0) && compress->running) ? 1 : 0;
}

int is_compress_ready(struct compress *compress)
{
	return (compress->fd > 0) ? 1 : 0;
}

static bool _is_codec_supported(struct compress *compress, struct compr_config *config)
{
	struct snd_compr_caps caps;
	bool codec = false;
	unsigned int i;

	if (ioctl(compress->fd, SNDRV_COMPRESS_GET_CAPS, &caps)) {
		oops(compress, errno, "cannot get device caps");
		return false;
	}

	for (i = 0; i < caps.num_codecs; i++) {
		if (caps.codecs[i] == config->codec->id) {
			/* found the codec */
			codec = true;
			break;
		}
	}
	if (codec == false) {
		oops(compress, -ENXIO, "this codec is not supported");
		return false;
	}

	if (config->fragment_size < caps.min_fragment_size) {
		oops(compress, -EINVAL, "requested fragment size %d is below min supported %d\n",
				config->fragment_size, caps.min_fragment_size);
		return false;
	}
	if (config->fragment_size > caps.max_fragment_size) {
		oops(compress, -EINVAL, "requested fragment size %d is above max supported %d\n",
				config->fragment_size, caps.max_fragment_size);
		return false;
	}
	if (config->fragments < caps.min_fragments) {
		oops(compress, -EINVAL, "requested fragments %d are below min supported %d\n",
				config->fragments, caps.min_fragments);
		return false;
	}
	if (config->fragments > caps.max_fragments) {
		oops(compress, -EINVAL, "requested fragments %d are above max supported %d\n",
				config->fragments, caps.max_fragments);
		return false;
	}

	/* TODO: match the codec properties */
	return true;
}

static bool _is_codec_type_supported(int fd, struct snd_codec *codec)
{
	struct snd_compr_caps caps;
	bool found = false;
	unsigned int i;

	if (ioctl(fd, SNDRV_COMPRESS_GET_CAPS, &caps)) {
		oops(&bad_compress, errno, "cannot get device caps");
		return false;
	}

	for (i = 0; i < caps.num_codecs; i++) {
		if (caps.codecs[i] == codec->id) {
			/* found the codec */
			found = true;
			break;
		}
	}
	/* TODO: match the codec properties */
	return found;
}

static inline void
fill_compress_params(struct compr_config *config, struct snd_compr_params *params)
{
	params->buffer.fragment_size = config->fragment_size;
	params->buffer.fragments = config->fragments;
	memcpy(&params->codec, config->codec, sizeof(params->codec));
}

struct compress *compress_open(unsigned int card, unsigned int device,
		unsigned int flags, struct compr_config *config)
{
	struct compress *compress;
	struct snd_compr_params params;
	char fn[256];
	int rc;

	compress = calloc(1, sizeof(struct compress));
	if (!compress || !config)
		return &bad_compress;

	compress->config = config;

	snprintf(fn, sizeof(fn), "/dev/snd/comprC%uD%u", card, device);

	compress->flags = flags;
	if (!((flags & COMPRESS_OUT) || (flags & COMPRESS_IN))) {
		oops(compress, -EINVAL, "can't deduce device direction from given flags\n");
		goto input_fail;
	}
	if (flags & COMPRESS_OUT) {
		/* this should be removed once we have capture tested */
		oops(compress, -EINVAL, "this version doesnt support capture\n");
		goto input_fail;
	}

	compress->fd = open(fn, O_WRONLY);
	if (compress->fd < 0) {
		oops(compress, errno, "cannot open device '%s'", fn);
		goto input_fail;
	}
#if 0
	/* FIXME need to turn this On when DSP supports
	 * and treat in no support case
	 */
	if (_is_codec_supported(compress, config) == false) {
		oops(compress, errno, "codec not supported\n");
		goto codec_fail;
	}
#endif
	fill_compress_params(config, &params);

	if (ioctl(compress->fd, SNDRV_COMPRESS_SET_PARAMS, &params)) {
		oops(compress, errno, "cannot set device");
		goto codec_fail;
	}

	return compress;

codec_fail:
	close(compress->fd);
	compress->fd = -1;
input_fail:
	free(compress);
	return &bad_compress;
}

void compress_close(struct compress *compress)
{
	if (compress == &bad_compress)
		return;

	if (compress->fd >= 0)
		close(compress->fd);
	compress->running = 0;
	compress->fd = -1;
	free(compress);
}

int compress_get_hpointer(struct compress *compress,
		unsigned int *avail, struct timespec *tstamp)
{
	struct snd_compr_avail kavail;
	size_t time;

	if (!is_compress_ready(compress))
		return oops(compress, -ENODEV, "device not ready");

	if (ioctl(compress->fd, SNDRV_COMPRESS_AVAIL, &kavail))
		return oops(compress, errno, "cannot get avail");
	*avail = (unsigned int)kavail.avail;

	time = (kavail.tstamp.pcm_io_frames) / kavail.tstamp.sampling_rate;
	tstamp->tv_sec = time;
	time = (kavail.tstamp.pcm_io_frames * 1000000000) / kavail.tstamp.sampling_rate;
	tstamp->tv_nsec = time;
	return 0;
}

int compress_write(struct compress *compress, char *buf, unsigned int size)
{
	struct snd_compr_avail avail;
	struct pollfd fds;
	int to_write, written, total = 0, ret;

	if (!(compress->flags & COMPRESS_IN))
		return oops(compress, -EINVAL, "Invalid flag set");
	if (!is_compress_ready(compress))
		return oops(compress, -ENODEV, "device not ready");
	fds.fd = compress->fd;
	fds.events = POLLOUT;

	/*TODO: treat auto start here first */
	while (size) {
		if (ioctl(compress->fd, SNDRV_COMPRESS_AVAIL, &avail))
			return oops(compress, errno, "cannot get avail");

		if (avail.avail == 0) {
			/* nothing to write so wait for 10secs */
			ret = poll(&fds, 1, 1000000);
			if (ret < 0)
				return oops(compress, errno, "poll error");
			if (ret == 0)
				return oops(compress, -EPIPE, "Poll timeout, Broken Pipe");
			if (fds.revents & POLLOUT) {
				if (ioctl(compress->fd, SNDRV_COMPRESS_AVAIL, &avail))
					return oops(compress, errno, "cannot get avail");
				if (avail.avail == 0) {
					oops(compress, -EIO, "woken up even when avail is 0!!!");
					continue;
				}
			}
			if (fds.revents & POLLERR) {
				return oops(compress, -EIO, "poll returned error!");
			}
		}
		/* write avail bytes */
		if (size > avail.avail)
			to_write =  avail.avail;
		else
			to_write = size;
		written = write(compress->fd, buf, to_write);
		if (written < 0)
			return oops(compress, errno, "write failed!\n");

		size -= written;
		buf += written;
		total += written;
	}
	return total;
}

int compress_read(struct compress *compress, void *buf, unsigned int size)
{
	return oops(compress, -ENOTTY, "Not supported yet in lib");
}

int compress_start(struct compress *compress)
{
	if (!is_compress_ready(compress))
		return oops(compress, -ENODEV, "device not ready");
	if (ioctl(compress->fd, SNDRV_COMPRESS_START))
		return oops(compress, errno, "cannot start the stream\n");
	compress->running = 1;
	return 0;

}

int compress_stop(struct compress *compress)
{
	if (!is_compress_running(compress))
		return oops(compress, -ENODEV, "device not ready");
	if (ioctl(compress->fd, SNDRV_COMPRESS_STOP))
		return oops(compress, errno, "cannot stop the stream\n");
	return 0;
}

int compress_pause(struct compress *compress)
{
	if (!is_compress_running(compress))
		return oops(compress, -ENODEV, "device not ready");
	if (ioctl(compress->fd, SNDRV_COMPRESS_PAUSE))
		return oops(compress, errno, "cannot pause the stream\n");
	return 0;
}

int compress_drain(struct compress *compress)
{
	if (!is_compress_running(compress))
		return oops(compress, -ENODEV, "device not ready");
	if (ioctl(compress->fd, SNDRV_COMPRESS_DRAIN))
		return oops(compress, errno, "cannot drain the stream\n");
	return 0;
}

bool is_codec_supported(unsigned int card, unsigned int device,
		unsigned int flags, struct snd_codec *codec)
{
	struct snd_compr_params params;
	unsigned int dev_flag;
	bool ret;
	int fd;
	char fn[256];
	int rc;

	snprintf(fn, sizeof(fn), "/dev/snd/comprC%uD%u", card, device);

	if (flags & COMPRESS_OUT)
		dev_flag = O_RDONLY;
	else
		dev_flag = O_WRONLY;

	fd = open(fn, dev_flag);
	if (fd < 0)
		return oops(&bad_compress, errno, "cannot open device '%s'", fn);

	ret = _is_codec_type_supported(fd, codec);

	close(fd);
	return ret;
}

