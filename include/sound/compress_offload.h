/****************************************************************************
 ****************************************************************************
 ***
 ***   This header was automatically generated from a Linux kernel header
 ***   of the same name, to make information necessary for userspace to
 ***   call into the kernel available to libc.  It contains only constants,
 ***   structures, and macros generated from the original header, and thus,
 ***   contains no copyrightable information.
 ***
 ***   To edit the content of this header, modify the corresponding
 ***   source file (e.g. under external/kernel-headers/original/) then
 ***   run bionic/libc/kernel/tools/update_all.py
 ***
 ***   Any manual change here will be lost the next time this script will
 ***   be run. You've been warned!
 ***
 ****************************************************************************
 ****************************************************************************/
#ifndef __COMPRESS_OFFLOAD_H
#define __COMPRESS_OFFLOAD_H
#include <linux/types.h>
#include <sound/asound.h>
#include <sound/compress_params.h>
#define SNDRV_COMPRESS_VERSION SNDRV_PROTOCOL_VERSION(0, 3, 0)

struct snd_compressed_buffer {
 __u32 fragment_size;
 __u32 fragments;
}__attribute__((packed, aligned(4)));

struct snd_compr_params {
 struct snd_compressed_buffer buffer;
 struct snd_codec codec;
 __u8 no_wake_mode;
}__attribute__((packed, aligned(4)));

struct snd_compr_tstamp {
 __u32 byte_offset;
 __u32 copied_total;
 __u32 pcm_frames;
 __u32 pcm_io_frames;
 __u32 sampling_rate;
}__attribute__((packed, aligned(4)));

struct snd_compr_avail {
 __u64 avail;
 struct snd_compr_tstamp tstamp;
}__attribute__((packed, aligned(4)));

enum snd_compr_direction {
 SND_COMPRESS_PLAYBACK = 0,
 SND_COMPRESS_CAPTURE,
 SND_COMPRESS_ACCEL
};

struct snd_compr_caps {
 __u32 num_codecs;
 __u32 direction;
 __u32 min_fragment_size;
 __u32 max_fragment_size;
 __u32 min_fragments;
 __u32 max_fragments;
 __u32 codecs[MAX_NUM_CODECS];
 __u32 reserved[11];
}__attribute__((packed, aligned(4)));

struct snd_compr_codec_caps {
 __u32 codec;
 __u32 num_descriptors;
 struct snd_codec_desc descriptor[MAX_NUM_CODEC_DESCRIPTORS];
}__attribute__((packed, aligned(4)));

enum {
	SNDRV_COMPRESS_ENCODER_PADDING = 1,
	SNDRV_COMPRESS_ENCODER_DELAY = 2,
};

struct snd_compr_metadata {
	 __u32 key;
	 __u32 value[8];
}__attribute__((packed, aligned(4)));


/* flags for struct snd_compr_task */
#define SND_COMPRESS_TFLG_NEW_STREAM           (1<<0)  /* mark for the new stream data */

/**
 * struct snd_compr_task - task primitive for non-realtime operation
 * @seqno: sequence number (task identifier)
 * @origin_seqno: previous sequence number (task identifier) - for reuse
 * @input_fd: data input file descriptor (dma-buf)
 * @output_fd: data output file descriptor (dma-buf)
 * @input_size: filled data in bytes (from caller, must not exceed fragment size)
 * @flags: see SND_COMPRESS_TFLG_* defines
 */
struct snd_compr_task {
	__u64 seqno;
	__u64 origin_seqno;
	int input_fd;
	int output_fd;
	__u64 input_size;
	__u32 flags;
	__u8 reserved[16];
} __attribute__((packed, aligned(4)));

/**
 * enum snd_compr_state - task state
 * @SND_COMPRESS_TASK_STATE_IDLE: task is not queued
 * @SND_COMPRESS_TASK_STATE_ACTIVE: task is in the queue
 * @SND_COMPRESS_TASK_STATE_FINISHED: task was processed, output is available
 */
enum snd_compr_state {
	SND_COMPRESS_TASK_STATE_IDLE = 0,
	SND_COMPRESS_TASK_STATE_ACTIVE,
	SND_COMPRESS_TASK_STATE_FINISHED
};

/**
 * struct snd_compr_task_status - task status
 * @seqno: sequence number (task identifier)
 * @input_size: filled data in bytes (from user space)
 * @output_size: filled data in bytes (from driver)
 * @output_flags: reserved for future (all zeros - from driver)
 * @state: actual task state (SND_COMPRESS_TASK_STATE_*)
 */
struct snd_compr_task_status {
	__u64 seqno;
	__u64 input_size;
	__u64 output_size;
	__u32 output_flags;
	__u8 state;
	__u8 reserved[15];
} __attribute__((packed, aligned(4)));

#define SNDRV_COMPRESS_IOCTL_VERSION _IOR('C', 0x00, int)
#define SNDRV_COMPRESS_GET_CAPS _IOWR('C', 0x10, struct snd_compr_caps)
#define SNDRV_COMPRESS_GET_CODEC_CAPS _IOWR('C', 0x11,  struct snd_compr_codec_caps)
#define SNDRV_COMPRESS_SET_PARAMS _IOW('C', 0x12, struct snd_compr_params)
#define SNDRV_COMPRESS_GET_PARAMS _IOR('C', 0x13, struct snd_codec)
#define SNDRV_COMPRESS_SET_METADATA _IOW('C', 0x14,  struct snd_compr_metadata)
#define SNDRV_COMPRESS_GET_METADATA _IOWR('C', 0x15,  struct snd_compr_metadata)
#define SNDRV_COMPRESS_TSTAMP _IOR('C', 0x20, struct snd_compr_tstamp)
#define SNDRV_COMPRESS_AVAIL _IOR('C', 0x21, struct snd_compr_avail)
#define SNDRV_COMPRESS_PAUSE _IO('C', 0x30)
#define SNDRV_COMPRESS_RESUME _IO('C', 0x31)
#define SNDRV_COMPRESS_START _IO('C', 0x32)
#define SNDRV_COMPRESS_STOP _IO('C', 0x33)
#define SNDRV_COMPRESS_DRAIN _IO('C', 0x34)
#define SNDRV_COMPRESS_NEXT_TRACK _IO('C', 0x35)
#define SNDRV_COMPRESS_PARTIAL_DRAIN _IO('C', 0x36)
#define SND_COMPR_TRIGGER_DRAIN 7
#define SND_COMPR_TRIGGER_NEXT_TRACK 8
#define SND_COMPR_TRIGGER_PARTIAL_DRAIN 9

#define SNDRV_COMPRESS_TASK_CREATE     _IOWR('C', 0x60, struct snd_compr_task)
#define SNDRV_COMPRESS_TASK_FREE       _IOW('C', 0x61, __u64)
#define SNDRV_COMPRESS_TASK_START      _IOWR('C', 0x62, struct snd_compr_task)
#define SNDRV_COMPRESS_TASK_STOP       _IOW('C', 0x63, __u64)
#define SNDRV_COMPRESS_TASK_STATUS     _IOWR('C', 0x68, struct snd_compr_task_status)

#endif
