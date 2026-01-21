/* SPDX-License-Identifier: (LGPL-2.1-only OR BSD-3-Clause) */
/* Copyright (c) 2011-2012, Intel Corporation. All rights reserved. */
/* Copyright (c) 2020 The Linux Foundation. All rights reserved. */

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

#include <linux/types.h>
#include <linux/ioctl.h>
#define __force
#define __bitwise
#define __user
#include <sound/asound.h>
#include "sound/compress_params.h"
#include "sound/compress_offload.h"
#include "tinycompress/tinycompress.h"
#include "tinycompress/compress_ops.h"

#define COMPR_ERR_MAX 128

/* Default maximum time we will wait in a poll() - 20 seconds */
#define DEFAULT_MAX_POLL_WAIT_MS    20000

struct compress_hw_data {
	int fd;
	unsigned int flags;
	char error[COMPR_ERR_MAX];
	int ioctl_version;
	struct compr_config *config;
	int running;
	int max_poll_wait_ms;
	int nonblocking;
	unsigned int gapless_metadata;
	unsigned int next_track;
};

static int oops(struct compress_hw_data *compress, int e, const char *fmt, ...)
{
	va_list ap;
	int sz;

	va_start(ap, fmt);
	vsnprintf(compress->error, COMPR_ERR_MAX, fmt, ap);
	va_end(ap);
	sz = strlen(compress->error);

	snprintf(compress->error + sz, COMPR_ERR_MAX - sz,
		": %s", strerror(e));
	errno = e;

	return -1;
}

static struct compress_hw_data bad_compress = {
	.fd = -1,
};

static const char *compress_hw_get_error(void *data)
{
	struct compress_hw_data *compress = (struct compress_hw_data *)data;

	return compress->error;
}

static int is_compress_hw_running(void *data)
{
	struct compress_hw_data *compress = (struct compress_hw_data *)data;

	return ((compress->fd > 0) && compress->running) ? 1 : 0;
}

static int is_compress_hw_ready(void *data)
{
	struct compress_hw_data *compress = (struct compress_hw_data *)data;

	return (compress->fd > 0) ? 1 : 0;
}

static int get_compress_hw_version(struct compress_hw_data *compress)
{
	return compress->ioctl_version;
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
fill_compress_hw_params(struct compr_config *config, struct snd_compr_params *params)
{
	params->buffer.fragment_size = config->fragment_size;
	params->buffer.fragments = config->fragments;
	memcpy(&params->codec, config->codec, sizeof(params->codec));
}

static void *compress_hw_open_by_name(const char *name,
		unsigned int flags, struct compr_config *config)
{
	struct compress_hw_data *compress;
	struct snd_compr_params params;
	struct snd_compr_caps caps;
	unsigned int card, device;
	char fn[256];

	if (!config) {
		oops(&bad_compress, EINVAL, "passed bad config");
		return &bad_compress;
	}

	if (sscanf(&name[3], "%u,%u", &card, &device) != 2) {
		oops(&bad_compress, errno, "Invalid device name %s", name);
		return &bad_compress;
	}

	compress = calloc(1, sizeof(struct compress_hw_data));
	if (!compress) {
		oops(&bad_compress, errno, "cannot allocate compress object");
		return &bad_compress;
	}

	compress->next_track = 0;
	compress->gapless_metadata = 0;
	compress->config = calloc(1, sizeof(*config));
	if (!compress->config)
		goto input_fail;

	snprintf(fn, sizeof(fn), "/dev/snd/comprC%uD%u", card, device);

	compress->max_poll_wait_ms = DEFAULT_MAX_POLL_WAIT_MS;

	compress->flags = flags;
	if (!((flags & COMPRESS_OUT) || (flags & COMPRESS_IN))) {
		oops(&bad_compress, EINVAL, "can't deduce device direction from given flags");
		goto config_fail;
	}

	if (flags & COMPRESS_OUT) {
		compress->fd = open(fn, O_RDONLY);
	} else {
		compress->fd = open(fn, O_WRONLY);
	}
	if (compress->fd < 0) {
		oops(&bad_compress, errno, "cannot open device '%s'", fn);
		goto config_fail;
	}

	if (ioctl(compress->fd, SNDRV_COMPRESS_IOCTL_VERSION, &compress->ioctl_version)) {
		oops(&bad_compress, errno, "cannot read version");
		goto codec_fail;
	}

	if (ioctl(compress->fd, SNDRV_COMPRESS_GET_CAPS, &caps)) {
		oops(compress, errno, "cannot get device caps");
		goto codec_fail;
	}

	/* If caller passed "don't care" fill in default values */
	if ((config->fragment_size == 0) || (config->fragments == 0)) {
		config->fragment_size = caps.min_fragment_size;
		config->fragments = caps.max_fragments;
	}

	memcpy(compress->config, config, sizeof(*compress->config));
	fill_compress_hw_params(config, &params);

	if (ioctl(compress->fd, SNDRV_COMPRESS_SET_PARAMS, &params)) {
		oops(&bad_compress, errno, "cannot set device");
		goto codec_fail;
	}

	return compress;

codec_fail:
	close(compress->fd);
	compress->fd = -1;
config_fail:
	free(compress->config);
input_fail:
	free(compress);
	return &bad_compress;
}

static void compress_hw_close(void *data)
{
	struct compress_hw_data *compress = (struct compress_hw_data *)data;

	if (compress == &bad_compress)
		return;

	if (compress->fd >= 0)
		close(compress->fd);
	compress->running = 0;
	compress->fd = -1;
	free(compress->config);
	free(compress);
}

static void compress_hw_avail64_from_32(struct snd_compr_avail64 *avail64,
					const struct snd_compr_avail *avail32)
{
	avail64->avail = avail32->avail;

	avail64->tstamp.byte_offset = avail32->tstamp.byte_offset;
	avail64->tstamp.copied_total = avail32->tstamp.copied_total;
	avail64->tstamp.pcm_frames = avail32->tstamp.pcm_frames;
	avail64->tstamp.pcm_io_frames = avail32->tstamp.pcm_io_frames;
	avail64->tstamp.sampling_rate = avail32->tstamp.sampling_rate;
}

static int compress_hw_get_hpointer(void *data,
		unsigned int *avail, struct timespec *tstamp)
{
	struct compress_hw_data *compress = (struct compress_hw_data *)data;
	struct snd_compr_avail kavail32;
	struct snd_compr_avail64 kavail64;
	__u64 time;

	if (!is_compress_hw_ready(compress))
		return oops(compress, ENODEV, "device not ready");

	const int version = get_compress_hw_version(compress);
	if (version <= 0)
		return -1;

	if (version < SNDRV_PROTOCOL_VERSION(0, 4, 0)) {
		/* SNDRV_COMPRESS_AVAIL64 not supported, fallback to SNDRV_COMPRESS_AVAIL */
		if (ioctl(compress->fd, SNDRV_COMPRESS_AVAIL, &kavail32))
			return oops(compress, errno, "cannot get avail");
		compress_hw_avail64_from_32(&kavail64, &kavail32);
	} else {
		if (ioctl(compress->fd, SNDRV_COMPRESS_AVAIL64, &kavail64))
			return oops(compress, errno, "cannot get avail64");
	}

	if (0 == kavail64.tstamp.sampling_rate)
		return oops(compress, ENODATA, "sample rate unknown");
	*avail = (unsigned int)kavail64.avail;
	time = kavail64.tstamp.pcm_io_frames / kavail64.tstamp.sampling_rate;
	tstamp->tv_sec = time;
	time = kavail64.tstamp.pcm_io_frames % kavail64.tstamp.sampling_rate;
	tstamp->tv_nsec = time * 1000000000 / kavail64.tstamp.sampling_rate;
	return 0;
}

static int compress_hw_get_tstamp(void *data,
			unsigned int *samples, unsigned int *sampling_rate)
{
	struct compress_hw_data *compress = (struct compress_hw_data *)data;
	struct snd_compr_tstamp ktstamp;

	if (!is_compress_hw_ready(compress))
		return oops(compress, ENODEV, "device not ready");

	if (ioctl(compress->fd, SNDRV_COMPRESS_TSTAMP, &ktstamp))
		return oops(compress, errno, "cannot get tstamp");

	*samples = ktstamp.pcm_io_frames;
	*sampling_rate = ktstamp.sampling_rate;
	return 0;
}

static int compress_hw_get_tstamp64(void *data,
			unsigned long long *samples, unsigned int *sampling_rate)
{
	struct compress_hw_data *compress = (struct compress_hw_data *)data;
	struct snd_compr_tstamp64 ktstamp;

	if (!is_compress_hw_ready(compress))
		return oops(compress, ENODEV, "device not ready");

	if (ioctl(compress->fd, SNDRV_COMPRESS_TSTAMP64, &ktstamp))
		return oops(compress, errno, "cannot get tstamp64");

	*samples = ktstamp.pcm_io_frames;
	*sampling_rate = ktstamp.sampling_rate;
	return 0;
}

static int compress_hw_write(void *data, const void *buf, size_t size)
{
	struct compress_hw_data *compress = (struct compress_hw_data *)data;
	struct snd_compr_avail avail;
	struct pollfd fds;
	int to_write = 0;	/* zero indicates we haven't written yet */
	int written, total = 0, ret;
	const char* cbuf = buf;
	const unsigned int frag_size = compress->config->fragment_size;

	if (!(compress->flags & COMPRESS_IN))
		return oops(compress, EINVAL, "Invalid flag set");
	if (!is_compress_hw_ready(compress))
		return oops(compress, ENODEV, "device not ready");
	fds.fd = compress->fd;
	fds.events = POLLOUT;

	/*TODO: treat auto start here first */
	while (size) {
		if (ioctl(compress->fd, SNDRV_COMPRESS_AVAIL, &avail))
			return oops(compress, errno, "cannot get avail");

		/* We can write if we have at least one fragment available
		 * or there is enough space for all remaining data
		 */
		if ((avail.avail < frag_size) && (avail.avail < size)) {

			if (compress->nonblocking)
				return total;

			ret = poll(&fds, 1, compress->max_poll_wait_ms);
			if (fds.revents & POLLERR) {
				return oops(compress, EIO, "poll returned error!");
			}
			/* A pause will cause -EBADFD or zero.
			 * This is not an error, just stop writing */
			if ((ret == 0) || (ret < 0 && errno == EBADFD))
				break;
			if (ret < 0)
				return oops(compress, errno, "poll error");
			if (fds.revents & POLLOUT) {
				continue;
			}
		}
		/* write avail bytes */
		if (size > avail.avail)
			to_write =  avail.avail;
		else
			to_write = size;
		written = write(compress->fd, cbuf, to_write);
		if (written < 0) {
			/* If play was paused the write returns -EBADFD */
			if (errno == EBADFD)
				break;
			return oops(compress, errno, "write failed!");
		}

		size -= written;
		cbuf += written;
		total += written;
	}
	return total;
}

static int compress_hw_read(void *data, void *buf, size_t size)
{
	struct compress_hw_data *compress = (struct compress_hw_data *)data;
	struct snd_compr_avail avail;
	struct pollfd fds;
	int to_read = 0;
	int num_read, total = 0, ret;
	char* cbuf = buf;
	const unsigned int frag_size = compress->config->fragment_size;

	if (!(compress->flags & COMPRESS_OUT))
		return oops(compress, EINVAL, "Invalid flag set");
	if (!is_compress_hw_ready(compress))
		return oops(compress, ENODEV, "device not ready");
	fds.fd = compress->fd;
	fds.events = POLLIN;

	while (size) {
		if (ioctl(compress->fd, SNDRV_COMPRESS_AVAIL, &avail))
			return oops(compress, errno, "cannot get avail");

		if ( (avail.avail < frag_size) && (avail.avail < size) ) {
			/* Less than one fragment available and not at the
			 * end of the read, so poll
			 */
			if (compress->nonblocking)
				return total;

			ret = poll(&fds, 1, compress->max_poll_wait_ms);
			if (fds.revents & POLLERR) {
				return oops(compress, EIO, "poll returned error!");
			}
			/* A pause will cause -EBADFD or zero.
			 * This is not an error, just stop reading */
			if ((ret == 0) || (ret < 0 && errno == EBADFD))
				break;
			if (ret < 0)
				return oops(compress, errno, "poll error");
			if (fds.revents & POLLIN) {
				continue;
			}
		}
		/* read avail bytes */
		if (size > avail.avail)
			to_read = avail.avail;
		else
			to_read = size;
		num_read = read(compress->fd, cbuf, to_read);
		if (num_read < 0) {
			/* If play was paused the read returns -EBADFD */
			if (errno == EBADFD)
				break;
			return oops(compress, errno, "read failed!");
		}

		size -= num_read;
		cbuf += num_read;
		total += num_read;
	}

	return total;
}

static int compress_hw_start(void *data)
{
	struct compress_hw_data *compress = (struct compress_hw_data *)data;

	if (!is_compress_hw_ready(compress))
		return oops(compress, ENODEV, "device not ready");
	if (ioctl(compress->fd, SNDRV_COMPRESS_START))
		return oops(compress, errno, "cannot start the stream");
	compress->running = 1;
	return 0;

}

static int compress_hw_stop(void *data)
{
	struct compress_hw_data *compress = (struct compress_hw_data *)data;

	if (!is_compress_hw_running(compress))
		return oops(compress, ENODEV, "device not ready");
	if (ioctl(compress->fd, SNDRV_COMPRESS_STOP))
		return oops(compress, errno, "cannot stop the stream");
	return 0;
}

static int compress_hw_pause(void *data)
{
	struct compress_hw_data *compress = (struct compress_hw_data *)data;

	if (!is_compress_hw_running(compress))
		return oops(compress, ENODEV, "device not ready");
	if (ioctl(compress->fd, SNDRV_COMPRESS_PAUSE))
		return oops(compress, errno, "cannot pause the stream");
	return 0;
}

static int compress_hw_resume(void *data)
{
	struct compress_hw_data *compress = (struct compress_hw_data *)data;

	if (ioctl(compress->fd, SNDRV_COMPRESS_RESUME))
		return oops(compress, errno, "cannot resume the stream");
	return 0;
}

static int compress_hw_drain(void *data)
{
	struct compress_hw_data *compress = (struct compress_hw_data *)data;

	if (!is_compress_hw_running(compress))
		return oops(compress, ENODEV, "device not ready");
	if (ioctl(compress->fd, SNDRV_COMPRESS_DRAIN))
		return oops(compress, errno, "cannot drain the stream");
	return 0;
}

static int compress_hw_partial_drain(void *data)
{
	struct compress_hw_data *compress = (struct compress_hw_data *)data;

	if (!is_compress_hw_running(compress))
		return oops(compress, ENODEV, "device not ready");

	if (!compress->next_track)
		return oops(compress, EPERM, "next track not signalled");
	if (ioctl(compress->fd, SNDRV_COMPRESS_PARTIAL_DRAIN))
		return oops(compress, errno, "cannot drain the stream\n");
	compress->next_track = 0;
	return 0;
}

static int compress_hw_next_track(void *data)
{
	struct compress_hw_data *compress = (struct compress_hw_data *)data;

	if (!is_compress_hw_running(compress))
		return oops(compress, ENODEV, "device not ready");

	if (!compress->gapless_metadata)
		return oops(compress, EPERM, "metadata not set");
	if (ioctl(compress->fd, SNDRV_COMPRESS_NEXT_TRACK))
		return oops(compress, errno, "cannot set next track\n");
	compress->next_track = 1;
	compress->gapless_metadata = 0;
	return 0;
}

static int compress_hw_set_gapless_metadata(void *data,
	struct compr_gapless_mdata *mdata)
{
	struct compress_hw_data *compress = (struct compress_hw_data *)data;
	struct snd_compr_metadata metadata;
	int version;

	if (!is_compress_hw_ready(compress))
		return oops(compress, ENODEV, "device not ready");

	version = get_compress_hw_version(compress);
	if (version <= 0)
		return -1;

	if (version < SNDRV_PROTOCOL_VERSION(0, 1, 1))
		return oops(compress, ENXIO, "gapless apis not supported in kernel");

	metadata.key = SNDRV_COMPRESS_ENCODER_PADDING;
	metadata.value[0] = mdata->encoder_padding;
	if (ioctl(compress->fd, SNDRV_COMPRESS_SET_METADATA, &metadata))
		return oops(compress, errno, "can't set metadata for stream\n");

	metadata.key = SNDRV_COMPRESS_ENCODER_DELAY;
	metadata.value[0] = mdata->encoder_delay;
	if (ioctl(compress->fd, SNDRV_COMPRESS_SET_METADATA, &metadata))
		return oops(compress, errno, "can't set metadata for stream\n");
	compress->gapless_metadata = 1;
	return 0;
}

static bool compress_hw_is_codec_supported_by_name(const char *name,
		unsigned int flags, struct snd_codec *codec)
{
	unsigned int card, device;
	unsigned int dev_flag;
	bool ret;
	int fd;
	char fn[256];

	if (sscanf(&name[3], "%u,%u", &card, &device) != 2)
		return false;

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

static void compress_hw_set_max_poll_wait(void *data, int milliseconds)
{
	struct compress_hw_data *compress = (struct compress_hw_data *)data;

	compress->max_poll_wait_ms = milliseconds;
}

static void compress_hw_set_nonblock(void *data, int nonblock)
{
	struct compress_hw_data *compress = (struct compress_hw_data *)data;

	compress->nonblocking = !!nonblock;
}

static int compress_hw_wait(void *data, int timeout_ms)
{
	struct pollfd fds;
	int ret;
	struct compress_hw_data *compress = (struct compress_hw_data *)data;

	fds.fd = compress->fd;
	fds.events = POLLOUT | POLLIN;

	ret = poll(&fds, 1, timeout_ms);
	if (ret > 0) {
		if (fds.revents & POLLERR)
			return oops(compress, EIO, "poll returned error!");
		if (fds.revents & (POLLOUT | POLLIN))
			return 0;
	}
	if (ret == 0)
		return oops(compress, ETIME, "poll timed out");
	if (ret < 0)
		return oops(compress, errno, "poll error");

	return oops(compress, EIO, "poll signalled unhandled event");
}

static int compress_hw_set_codec_params(void *data, struct snd_codec *codec)
{
	struct compress_hw_data *compress = (struct compress_hw_data *)data;
	struct snd_compr_params params;

	if (!is_compress_hw_ready(compress))
		return oops(compress, ENODEV, "device not ready\n");

	if (!codec)
		return oops(compress, EINVAL, "passed bad config\n");

	if (!compress->next_track)
		return oops(compress, EPERM,
			    "set CODEC params while next track not signalled is not allowed");

	params.buffer.fragment_size = compress->config->fragment_size;
	params.buffer.fragments = compress->config->fragments;
	memcpy(&params.codec, codec, sizeof(params.codec));

	if (ioctl(compress->fd, SNDRV_COMPRESS_SET_PARAMS, &params))
		return oops(compress, errno, "cannot set param for next track\n");

	return 0;
}

struct compress_ops compress_hw_ops = {
	.magic = COMPRESS_OPS_V2,
	.open_by_name = compress_hw_open_by_name,
	.close = compress_hw_close,
	.get_hpointer = compress_hw_get_hpointer,
	.get_tstamp = compress_hw_get_tstamp,
	.get_tstamp64 = compress_hw_get_tstamp64,
	.write = compress_hw_write,
	.read = compress_hw_read,
	.start = compress_hw_start,
	.stop = compress_hw_stop,
	.pause = compress_hw_pause,
	.resume = compress_hw_resume,
	.drain = compress_hw_drain,
	.partial_drain = compress_hw_partial_drain,
	.next_track = compress_hw_next_track,
	.set_gapless_metadata = compress_hw_set_gapless_metadata,
	.set_max_poll_wait = compress_hw_set_max_poll_wait,
	.set_nonblock = compress_hw_set_nonblock,
	.wait = compress_hw_wait,
	.is_codec_supported_by_name = compress_hw_is_codec_supported_by_name,
	.is_compress_running = is_compress_hw_running,
	.is_compress_ready = is_compress_hw_ready,
	.get_error = compress_hw_get_error,
	.set_codec_params = compress_hw_set_codec_params,
};

