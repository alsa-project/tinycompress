/* SPDX-License-Identifier: (LGPL-2.1-only OR BSD-3-Clause) */
/* Copyright (c) 2011-2012, Intel Corporation */

#ifndef __TINYCOMPRESS_H
#define __TINYCOMPRESS_H

#include <linux/types.h>
#include <stdbool.h>

#if defined(__cplusplus)
extern "C" {
#endif
/*
 * struct compr_config: config structure, needs to be filled by app
 * If fragment_size or fragments are zero, this means "don't care"
 * and tinycompress will choose values that the driver supports
 *
 * @fragment_size: size of fragment requested, in bytes
 * @fragments: number of fragments
 * @codec: codec type and parameters requested
 */
struct compr_config {
	__u32 fragment_size;
	__u32 fragments;
	struct snd_codec *codec;
};

struct compr_gapless_mdata {
	__u32 encoder_delay;
	__u32 encoder_padding;
};

#define COMPRESS_OUT        0x20000000
#define COMPRESS_IN         0x10000000

struct compress;
struct snd_compr_tstamp;

/*
 * compress_open: open a new compress stream
 * returns the valid struct compress on success, NULL on failure
 * If config does not specify a requested fragment size, on return
 * it will be updated with the size and number of fragments that
 * were configured
 *
 * @card: sound card number
 * @device: device number
 * @flags: device flags can be COMPRESS_OUT or COMPRESS_IN
 * @config: stream config requested. Returns actual fragment config
 */
struct compress *compress_open(unsigned int card, unsigned int device,
		unsigned int flags, struct compr_config *config);

/*
 * compress_close: close the compress stream
 *
 * @compress: compress stream to be closed
 */
void compress_close(struct compress *compress);

/*
 * compress_get_hpointer: get the hw timestamp
 * return 0 on success, negative on error
 *
 * @compress: compress stream on which query is made
 * @avail: buffer availble for write/read, in bytes
 * @tstamp: hw time
 */
int compress_get_hpointer(struct compress *compress,
		unsigned int *avail, struct timespec *tstamp);


/*
 * compress_get_tstamp: get the raw hw timestamp
 * return 0 on success, negative on error
 *
 * @compress: compress stream on which query is made
 * @samples: number of decoded samples played
 * @sampling_rate: sampling rate of decoded samples
 */
int compress_get_tstamp(struct compress *compress,
		unsigned int *samples, unsigned int *sampling_rate);

/*
 * compress_write: write data to the compress stream
 * return bytes written on success, negative on error
 * By default this is a blocking call and will not return
 * until all bytes have been written or there was a
 * write error.
 * If non-blocking mode has been enabled with compress_nonblock(),
 * this function will write all bytes that can be written without
 * blocking and will then return the number of bytes successfully
 * written. If the return value is not an error and is < size
 * the caller can use compress_wait() to block until the driver
 * is ready for more data.
 *
 * @compress: compress stream to be written to
 * @buf: pointer to data
 * @size: number of bytes to be written
 */
int compress_write(struct compress *compress, const void *buf, unsigned int size);

/*
 * compress_read: read data from the compress stream
 * return bytes read on success, negative on error
 * By default this is a blocking call and will block until
 * size bytes have been written or there was a read error.
 * If non-blocking mode was enabled using compress_nonblock()
 * the behaviour will change to read only as many bytes as
 * are currently available (if no bytes are available it
 * will return immediately). The caller can then use
 * compress_wait() to block until more bytes are available.
 *
 * @compress: compress stream from where data is to be read
 * @buf: pointer to data buffer
 * @size: size of given buffer
 */
int compress_read(struct compress *compress, void *buf, unsigned int size);

/*
 * compress_start: start the compress stream
 * return 0 on success, negative on error
 *
 * @compress: compress stream to be started
 */
int compress_start(struct compress *compress);

/*
 * compress_stop: stop the compress stream
 * return 0 on success, negative on error
 *
 * @compress: compress stream to be stopped
 */
int compress_stop(struct compress *compress);

/*
 * compress_pause: pause the compress stream
 * return 0 on success, negative on error
 *
 * @compress: compress stream to be paused
 */
int compress_pause(struct compress *compress);

/*
 * compress_resume: resume the compress stream
 * return 0 on success, negative on error
 *
 * @compress: compress stream to be resumed
 */
int compress_resume(struct compress *compress);

/*
 * compress_drain: drain the compress stream
 * return 0 on success, negative on error
 *
 * @compress: compress stream to be drain
 */
int compress_drain(struct compress *compress);

/*
 * compress_next_track: set the next track for stream
 *
 * return 0 on success, negative on error
 *
 * @compress: compress stream to be transistioned to next track
 */
int compress_next_track(struct compress *compress);

/*
 * compress_partial_drain: drain will return after the last frame is decoded
 * by DSP and will play the , All the data written into compressed
 * ring buffer is decoded
 *
 * return 0 on success, negative on error
 *
 * @compress: compress stream to be drain
 */
int compress_partial_drain(struct compress *compress);

/*
 * compress_set_gapless_metadata: set gapless metadata of a compress strem
 *
 * return 0 on success, negative on error
 *
 * @compress: compress stream for which metadata has to set
 * @mdata: metadata encoder delay and  padding
 */

int compress_set_gapless_metadata(struct compress *compress,
			struct compr_gapless_mdata *mdata);

/*
 * is_codec_supported:check if the given codec is supported
 * returns true when supported, false if not
 *
 * @card: sound card number
 * @device: device number
 * @flags: stream flags
 * @codec: codec type and parameters to be checked
 */
bool is_codec_supported(unsigned int card, unsigned int device,
	       unsigned int flags, struct snd_codec *codec);

/*
 * compress_set_max_poll_wait: set the maximum time tinycompress
 * will wait for driver to signal a poll(). Interval is in
 * milliseconds.
 * Pass interval of -1 to disable timeout and make poll() wait
 * until driver signals.
 * If this function is not used the timeout defaults to 20 seconds.
 */
void compress_set_max_poll_wait(struct compress *compress, int milliseconds);

/* Enable or disable non-blocking mode for write and read */
void compress_nonblock(struct compress *compress, int nonblock);

/* Wait for ring buffer to ready for next read or write */
int compress_wait(struct compress *compress, int timeout_ms);

int is_compress_running(struct compress *compress);

int is_compress_ready(struct compress *compress);

/* Returns a human readable reason for the last error */
const char *compress_get_error(struct compress *compress);

#if defined(__cplusplus)
} // extern "C"
#endif

#endif
