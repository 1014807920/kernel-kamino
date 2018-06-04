/*
 *  ALSA Platform Device Driver
 *
 * Copyright (C) 1991-2017 NationalChip Co., Ltd
 * All rights reserved!
 *
 * pcm.c: ALSA PCM Playback And Capture Implement
 *
 */

#include <linux/init.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/jiffies.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/wait.h>
#include <linux/hrtimer.h>
#include <linux/math64.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/control.h>
#include <sound/tlv.h>
#include <sound/rawmidi.h>
#include <sound/info.h>
#include <sound/initval.h>
#include <linux/delay.h>

#include "pcm.h"
#include "core.h"
#include "config.h"

struct pcm_buffer {
	unsigned int  full;
	unsigned char *buffer;
	unsigned int  bufferSize;
	unsigned int  readAddr;
	unsigned int  writeAddr;
	unsigned int  appUpdate;
};

static struct snd_pcm_hardware gx8010_pcm_playback = {
	.info = (SNDRV_PCM_INFO_INTERLEAVED |
				 SNDRV_PCM_INFO_NONINTERLEAVED |
				 SNDRV_PCM_INFO_RESUME |
				 SNDRV_PCM_INFO_PAUSE |
				 SNDRV_PCM_INFO_DRAIN_TRIGGER),
	//SNDRV_PCM_INFO_MMAP |SNDRV_PCM_INFO_MMAP_VALID
	.formats = (SNDRV_PCM_FMTBIT_S16_BE|
			SNDRV_PCM_FMTBIT_S16_LE|
			SNDRV_PCM_FMTBIT_S32_BE|
			SNDRV_PCM_FMTBIT_S32_LE),
	.rates    = SNDRV_PCM_RATE_8000_48000,
	.rate_min = 8000,
	.rate_max = 48000,
	.channels_min = 1,
	.channels_max = 2,
	.buffer_bytes_max = (128*1024),
	.period_bytes_min = (64),
	.period_bytes_max = (64*1024),
	.periods_min      = 2,
	.periods_max      = 2*1024,
	.fifo_size        = 0,
};

static struct snd_pcm_hardware gx8010_pcm_capture = {
	.info = (SNDRV_PCM_INFO_NONINTERLEAVED),
	//SNDRV_PCM_INFO_MMAP |SNDRV_PCM_INFO_MMAP_VALID
	.formats = (SNDRV_PCM_FMTBIT_S16_BE|
			SNDRV_PCM_FMTBIT_S16_LE),
	.rates    = (SNDRV_PCM_RATE_8000 |
			SNDRV_PCM_RATE_16000 |
			SNDRV_PCM_RATE_48000),
	.rate_min = 8000,
	.rate_max = 48000,
	.channels_min = 1,
	.channels_max = 8,
	.buffer_bytes_max = (64*1024),
	.period_bytes_min = 64,
	.period_bytes_max = (64*1024),
	.periods_min      = 1,
	.periods_max      = 1024,
	.fifo_size        = 0,
};

#define PCM_PLAYBACK_COUNT 3
#define PCM_CAPTURE_COUNT  0
static struct snd_pcm *gx8010_pcm = NULL;

static int _pcm_buffer_set_wptr(struct snd_pcm_substream *substream, unsigned int ptr);

static int _pcm_buffer_alloc(struct snd_pcm_substream *substream, unsigned int size)
{
	struct pcm_buffer *buf = kmalloc(sizeof(struct pcm_buffer), GFP_KERNEL);

#ifdef AOUT_DEBUG
	printk("%s %d\n", __FUNCTION__, __LINE__);
#endif

	buf->buffer = kmalloc(size, GFP_KERNEL);
	buf->bufferSize = size;
	buf->readAddr   = 0;
	buf->writeAddr  = 0;
	buf->full       = 0;
	buf->appUpdate  = 0;
	gx8010_core_set_priv(substream->number, (void*)buf);

	return 0;
}


static int _pcm_buffer_free(struct snd_pcm_substream *substream)
{
	struct pcm_buffer *buf = (struct pcm_buffer*)gx8010_core_get_priv(substream->number);

#ifdef AOUT_DEBUG
	printk("%s %d\n", __FUNCTION__, __LINE__);
#endif
	if (buf) {
		if (buf->buffer) {
			kfree(buf->buffer);
			buf->buffer     = NULL;
			buf->full       = 0;
			buf->bufferSize = 0;
			buf->appUpdate  = 0;
		}
		buf->readAddr  = 0;
		buf->writeAddr = 0;
		kfree(buf);
		gx8010_core_set_priv(substream->number, NULL);
	}

	return 0;
}

static int _pcm_buffer_write(struct snd_pcm_substream *substream,
		unsigned char *buffer, unsigned int pos, unsigned int size)
{
	int ret = 0;
	struct pcm_buffer *buf = (struct pcm_buffer*)gx8010_core_get_priv(substream->number);

	if (!buf) return -1;

	pos %= buf->bufferSize;
	if (pos != buf->writeAddr) {
#ifdef AOUT_DEBUG
		printk("[%d] update wptr: %x, writeAddr: %x\n", __LINE__, pos, buf->writeAddr);
#endif
		_pcm_buffer_set_wptr(substream, pos);
	}

	if (buf->full) {
		printk("warning: %s %d\n", __FUNCTION__, __LINE__);
		return 0;
	}

	if ((pos + size) > buf->bufferSize) {
		unsigned int copyLen = buf->bufferSize - pos;
		unsigned int dstAddr = (unsigned int)buf->buffer + pos;
		unsigned int srcAddr = (unsigned int)buffer;

		ret = copy_from_user((void*)dstAddr, (void*)srcAddr, copyLen);
		if (ret < 0) {
			printk("warning: %s %d\n", __FUNCTION__, __LINE__);
			return -1;
		}
		dstAddr  = (unsigned int)buf->buffer;
		srcAddr += copyLen;
		copyLen  = size - copyLen;
		ret = copy_from_user((void*)dstAddr, (void*)srcAddr, copyLen);
		if (ret < 0) {
			printk("warning: %s %d\n", __FUNCTION__, __LINE__);
			return -1;
		}
	} else {
		unsigned int copyLen = size;
		unsigned int dstAddr = (unsigned int)buf->buffer + pos;
		unsigned int srcAddr = (unsigned int)buffer;

		ret = copy_from_user((void*)dstAddr, (void*)srcAddr, copyLen);
		if (ret < 0) {
			printk("warning: %s %d\n", __FUNCTION__, __LINE__);
			return -1;
		}
	}

	buf->writeAddr = (pos + size) % buf->bufferSize;
	buf->full = (buf->writeAddr == buf->readAddr) ? 1 : buf->full;
	buf->appUpdate = 0;

	return size;
}

static int _pcm_buffer_read(struct snd_pcm_substream *substream,
		unsigned char *buffer, unsigned int pos, unsigned int size)
{
	//fix it
	return 0;
}

static int _pcm_buffer_copy(struct snd_pcm_substream *substream)
{
	struct pcm_buffer *buf = (struct pcm_buffer*)gx8010_core_get_priv(substream->number);
	unsigned int freeLen = 0, dataLen = 0, copyLen = 0;

	if (!buf) return -1;

	freeLen = gx8010_core_free_space(substream->number);
	dataLen = (buf->writeAddr + buf->bufferSize - buf->readAddr) % buf->bufferSize ;
	dataLen = buf->full ? buf->bufferSize : dataLen;
	dataLen = (freeLen > dataLen) ? dataLen : freeLen;

	if (!dataLen) return 0;

	if ((buf->readAddr + dataLen) > buf->bufferSize) {
		copyLen = buf->bufferSize - buf->readAddr;
		gx8010_core_write(substream->number, buf->buffer + buf->readAddr, copyLen);
		copyLen = dataLen - copyLen;
		gx8010_core_write(substream->number, buf->buffer, copyLen);
	} else {
		copyLen = dataLen;
		gx8010_core_write(substream->number, buf->buffer + buf->readAddr, copyLen);
	}

	buf->readAddr = (buf->readAddr + dataLen) % buf->bufferSize;
	buf->full = (buf->writeAddr != buf->readAddr) ? 0 : buf->full;

	return 0;
}

static int _pcm_buffer_set_rptr(struct snd_pcm_substream *substream, unsigned int ptr)
{
	struct pcm_buffer *buf = (struct pcm_buffer*)gx8010_core_get_priv(substream->number);

	if (!buf) return -1;

	buf->full = 0;
	buf->readAddr = ptr;

	return 0;
}

static int _pcm_buffer_set_wptr(struct snd_pcm_substream *substream, unsigned int ptr)
{
	struct pcm_buffer *buf = (struct pcm_buffer*)gx8010_core_get_priv(substream->number);

	if (!buf) return -1;

	buf->full = 0;
	buf->writeAddr = ptr;

	return 0;
}

static int _pcm_buffer_get_rptr(struct snd_pcm_substream *substream, unsigned int *ptr)
{
	struct pcm_buffer *buf = (struct pcm_buffer*)gx8010_core_get_priv(substream->number);

	if (!buf) return -1;

	if (ptr)
		*ptr = buf->readAddr;

	return 0;
}

static int _pcm_buffer_get_wptr(struct snd_pcm_substream *substream, unsigned int *ptr)
{
	struct pcm_buffer *buf = (struct pcm_buffer*)gx8010_core_get_priv(substream->number);

	if (!buf) return -1;

	if (ptr)
		*ptr = buf->writeAddr;

	return 0;
}

static unsigned int _pcm_buffer_get_space(struct snd_pcm_substream *substream)
{
	struct pcm_buffer *buf = (struct pcm_buffer*)gx8010_core_get_priv(substream->number);

	if (!buf) return 0;

	if (buf->full)
		return buf->bufferSize;

	return ((buf->writeAddr + buf->bufferSize - buf->readAddr) % buf->bufferSize);
}

static int _pcm_buffer_drain(struct snd_pcm_substream *substream)
{
	unsigned int retry = 0;
	unsigned int space = 0;
#define DARIN_RETRY_COUNT 100

	while (retry < DARIN_RETRY_COUNT) {
		 space = _pcm_buffer_get_space(substream);

		if (space == 0) break;

		_pcm_buffer_copy(substream);
		gx8010_core_drain(substream->number);
		retry++;
		mdelay(10);
	}

	if (retry == DARIN_RETRY_COUNT)
		printk("warning: %s %d space: %d\n", __FUNCTION__, __LINE__, space);

	return 0;
}

static int _playback_callback(void *priv)
{
	struct snd_pcm_substream *substream = (struct snd_pcm_substream *) priv;
	struct pcm_buffer *buf = (struct pcm_buffer*)gx8010_core_get_priv(substream->number);
	struct snd_pcm_runtime   *runtime   = substream->runtime;

	if (!buf) return 0;

	snd_pcm_period_elapsed(substream);
	if (buf->appUpdate) {
		unsigned int avail = 0, space = 0;
		avail = snd_pcm_playback_hw_avail(runtime);
		avail = frames_to_bytes(runtime, avail);
		space = _pcm_buffer_get_space(substream);

		if (avail != space) {
			unsigned int wptr = (buf->writeAddr + buf->bufferSize + avail - space) % buf->bufferSize;

#ifdef AOUT_DEBUG
			printk("[%d] update wptr: %x, writeAddr: %x\n", __LINE__, wptr, buf->writeAddr);
#endif
			_pcm_buffer_set_wptr(substream, wptr);
		}
	}
	_pcm_buffer_copy(substream);

	return 0;
}

static int _capture_callback(void *priv)
{
	struct snd_pcm_substream *substream = (struct snd_pcm_substream *) priv;

	//fix it
	_pcm_buffer_set_rptr(substream, 0);
	return 0;
}

static int gx8010_pcm_open(struct snd_pcm_substream *substream)
{
	int ret = -1;
	struct snd_pcm_runtime *runtime = substream->runtime;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		runtime->hw = gx8010_pcm_playback;
		ret = gx8010_core_open(substream->pcm->card->private_data,
				substream->number,
				_playback_callback,
				(void *)substream);
	} else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		runtime->hw = gx8010_pcm_capture;
		ret = gx8010_core_open(substream->pcm->card->private_data,
				AOUT_CAPTURE,
				_capture_callback,
				(void *)substream);
	}

	return ret;
}

static int gx8010_pcm_close(struct snd_pcm_substream *substream)
{
	int ret = -1;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		_pcm_buffer_free(substream);
		ret = gx8010_core_close(substream->number);
	} else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		//fix it
		ret = gx8010_core_close(AOUT_CAPTURE);
	}

	return ret;
}

static int gx8010_pcm_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *hw_params)
{
	int ret = -1;
	struct aout_params aoutParams;

	switch (params_rate(hw_params)) {
	case 8000:
		aoutParams.sampleRate = SAMPLERATE_8KHZ;
		break;
	case 11025:
		aoutParams.sampleRate = SAMPLERATE_11KDOT025HZ;
		break;
	case 16000:
		aoutParams.sampleRate = SAMPLERATE_16KHZ;
		break;
	case 22050:
		aoutParams.sampleRate = SAMPLERATE_22KDOT05HZ;
		break;
	case 24000:
		aoutParams.sampleRate = SAMPLERATE_24KHZ;
		break;
	case 32000:
		aoutParams.sampleRate = SAMPLERATE_32KHZ;
		break;
	case 44100:
		aoutParams.sampleRate = SAMPLERATE_44KDOT1HZ;
		break;
	case 48000:
		aoutParams.sampleRate = SAMPLERATE_48KHZ;
		break;
	default:
		return -1;
	}

	switch (params_channels(hw_params)) {
	case 1:
		aoutParams.channelNum = SIGNAL_CHANNEL;
		break;
	case 2:
		aoutParams.channelNum = DOUBLE_CHANNEL;
		break;
	default:
		return -1;
	}

	switch (params_format(hw_params)) {
	case SNDRV_PCM_FORMAT_S16_BE:
		aoutParams.bitSize = BIT16;
		aoutParams.endian  = BIG_ENDIAN;
		break;
	case SNDRV_PCM_FORMAT_S16_LE:
		aoutParams.bitSize = BIT16;
		aoutParams.endian  = LITTLE_ENDIAN;
		break;
	case SNDRV_PCM_FORMAT_S32_BE:
		aoutParams.bitSize = BIT32;
		aoutParams.endian  = BIG_ENDIAN;
		break;
	case SNDRV_PCM_FORMAT_S32_LE:
		aoutParams.bitSize = BIT32;
		aoutParams.endian  = LITTLE_ENDIAN;
		break;
	default:
		return -1;
	}

	switch (params_access(hw_params)) {
	case SNDRV_PCM_ACCESS_RW_INTERLEAVED:
	case SNDRV_PCM_ACCESS_MMAP_INTERLEAVED:
		aoutParams.interlace = INTERLACE_TRUE;
		break;
	case SNDRV_PCM_ACCESS_RW_NONINTERLEAVED:
	case SNDRV_PCM_ACCESS_MMAP_NONINTERLEAVED:
		aoutParams.interlace = INTERLACE_FALSE;
		break;
	default:
		return -1;
	}

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		_pcm_buffer_alloc(substream, params_buffer_bytes(hw_params));
		ret = gx8010_core_config(substream->number, &aoutParams);
	} else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		//fix it
		ret = gx8010_core_config(AOUT_CAPTURE, &aoutParams);
	}

	return ret;
}

int gx8010_pcm_hw_free(struct snd_pcm_substream *substream)
{
	return 0;
}

static int gx8010_pcm_prepare(struct snd_pcm_substream *substream)
{
	return 0;
}

static int gx8010_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
	int ret = -1;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			ret = gx8010_core_run(substream->number);
		else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
			ret = gx8010_core_run(AOUT_CAPTURE);
		break;
	case SNDRV_PCM_TRIGGER_RESUME:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			ret = gx8010_core_resume(substream->number);
		else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
			ret = gx8010_core_resume(AOUT_CAPTURE);
		break;
	case SNDRV_PCM_TRIGGER_DRAIN:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			ret = _pcm_buffer_drain(substream);
		} else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		//fix it
		}
		break;
	case SNDRV_PCM_TRIGGER_STOP:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			ret = gx8010_core_stop(substream->number);
		else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
			ret = gx8010_core_stop(AOUT_CAPTURE);
		break;
	case SNDRV_PCM_TRIGGER_SUSPEND:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			ret = gx8010_core_pause(substream->number);
		else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
			ret = gx8010_core_pause(AOUT_CAPTURE);
		break;
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		break;
	default:
		break;
	}

	return ret;
}

static snd_pcm_uframes_t gx8010_pcm_pointer(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	unsigned int pos = 0;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		_pcm_buffer_get_rptr(substream, &pos);
	} else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		_pcm_buffer_get_wptr(substream, &pos);
	}

	return bytes_to_frames(runtime, pos);
}

static int gx8010_pcm_mmap(struct snd_pcm_substream *substream,
	struct vm_area_struct *vma)
{
	return 0;
}

static int gx8010_pcm_copy(struct snd_pcm_substream *substream, int channel,
		    snd_pcm_uframes_t pos,
		    void __user *buf, snd_pcm_uframes_t frames)
{
	int len = 0;
	struct snd_pcm_runtime *runtime = substream->runtime;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		len = _pcm_buffer_write(substream, buf,
				frames_to_bytes(runtime, pos), frames_to_bytes(runtime, frames));
		_pcm_buffer_copy(substream);
		gx8010_core_rerun(substream->number);
	} else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		len = _pcm_buffer_read(substream, buf,
				frames_to_bytes(runtime, pos), frames_to_bytes(runtime, frames));
	}

	return len;
}

static int gx8010_pcm_silence(struct snd_pcm_substream *substream,
			     int channel, snd_pcm_uframes_t pos,
			     snd_pcm_uframes_t count)
{
	return 0;
}

static struct page *gx8010_pcm_page(struct snd_pcm_substream *substream,
				   unsigned long offset)
{
	return 0;
}

static int gx8010_pcm_reset(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	unsigned long flags;
	unsigned int ptr = 0;

	snd_pcm_stream_lock_irqsave(substream, flags);
	ptr = frames_to_bytes(runtime, (runtime->status->hw_ptr % runtime->buffer_size));
	_pcm_buffer_set_rptr(substream, ptr);
	_pcm_buffer_set_wptr(substream, ptr);
	snd_pcm_stream_unlock_irqrestore(substream, flags);

	return 0;
}

static int gx8010_pcm_ioctl(struct snd_pcm_substream *substream,
		unsigned int cmd, void *arg)
{
	int ret = 0;

	ret = snd_pcm_lib_ioctl(substream, cmd, arg);

	if (cmd == SNDRV_PCM_IOCTL1_RESET)
		gx8010_pcm_reset(substream);

	return ret;
}

static int gx8010_pcm_ack(struct snd_pcm_substream *substream)
{
	struct pcm_buffer *buf = (struct pcm_buffer*)gx8010_core_get_priv(substream->number);

	buf->appUpdate = 1;
	return 0;
}

static struct snd_pcm_ops gx8010_pcm_ops = {
	.open       = gx8010_pcm_open,
	.close      = gx8010_pcm_close,
	.ioctl      = gx8010_pcm_ioctl,
	.hw_params  = gx8010_pcm_hw_params,
	.hw_free    = gx8010_pcm_hw_free,
	.prepare    = gx8010_pcm_prepare,
	.trigger    = gx8010_pcm_trigger,
	.pointer    = gx8010_pcm_pointer,
	.mmap       = gx8010_pcm_mmap,
	.copy       = gx8010_pcm_copy,
	.silence    = gx8010_pcm_silence,
	.page       = gx8010_pcm_page,
	.ack        = gx8010_pcm_ack,
};

int gx8010_pcm_new(struct snd_card *card)
{
	struct snd_pcm *pcm;
	int ret;
	int playback = PCM_PLAYBACK_COUNT;
	int capture  = PCM_CAPTURE_COUNT;

	ret = snd_pcm_new(card, "gx8010-PCM", 0, playback, capture, &pcm);
	if (ret)
		goto out;

	if (playback)
		snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_PLAYBACK, &gx8010_pcm_ops);

	if (capture)
		snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_CAPTURE, &gx8010_pcm_ops);

	gx8010_pcm = pcm;
	ret = 0;

 out:
	return ret;
}

EXPORT_SYMBOL(gx8010_pcm_new);

int gx8010_pcm_suspend(void)
{
	snd_pcm_suspend_all(gx8010_pcm);

	return 0;
}

EXPORT_SYMBOL(gx8010_pcm_suspend);

int gx8010_pcm_resume(void)
{
	//hw resume
	return 0;
}

EXPORT_SYMBOL(gx8010_pcm_resume);

MODULE_AUTHOR("linxsh");
MODULE_DESCRIPTION("audio driver for the gx8010 chip");
MODULE_LICENSE("GPL");
