/*
 *  ALSA Platform Device Driver
 *
 * Copyright (C) 1991-2017 NationalChip Co., Ltd
 * All rights reserved!
 *
 * core.c: ALSA Core Implement
 *
 */

#include <linux/init.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>

#include "config.h"
#include "reg.h"
#include "core.h"
#include "stream.h"

#define MAX_AOUT_SUB_DEVICE 4

struct aout_manager {
	unsigned int          count;
	enum aout_subdevice   subdev;
	const char            *name;
	const char            *info;
	struct aout_substream *substream;
	void                  *priv;
};

static struct aout_manager subdev_manager[MAX_AOUT_SUB_DEVICE] = {
	{0, AOUT_PLAYBACK0, "src 0", "audio playback 0", NULL, NULL},
	{0, AOUT_PLAYBACK1, "src 1", "audio playback 1", NULL, NULL},
	{0, AOUT_PLAYBACK2, "src 2", "audio playback 2", NULL, NULL},
	{0, AOUT_CAPTURE,   "src 3", "audio capture",    NULL, NULL}
};

static long int defaultDBValue[MAX_AOUT_SUB_DEVICE] = {0, 0, 0, 0};
static long int defaultMute   [MAX_AOUT_SUB_DEVICE] = {0, 0, 0, 0};
static enum aout_track defaultTrack[MAX_AOUT_SUB_DEVICE] = {STEREO_TRACK, STEREO_TRACK, STEREO_TRACK, STEREO_TRACK};

static struct aout_substream *aout_substream_create(enum aout_subdevice subdev)
{
	if (subdev >= MAX_AOUT_SUB_DEVICE)
		return NULL;

	if (subdev_manager[subdev].count == 0) {
		subdev_manager[subdev].substream = kmalloc(sizeof(struct aout_substream), GFP_KERNEL);
		if (!subdev_manager[subdev].substream)
			return NULL;
		subdev_manager[subdev].substream->subdev = subdev;
	}
	subdev_manager[subdev].count++;

	return subdev_manager[subdev].substream;
}

static struct aout_substream *aout_substream_search(enum aout_subdevice subdev)
{
	if (subdev >= MAX_AOUT_SUB_DEVICE)
		return NULL;

	if (subdev_manager[subdev].count == 0)
		return NULL;;

	return subdev_manager[subdev].substream;
}

static void aout_substream_delete(enum aout_subdevice subdev)
{
	if (subdev >= MAX_AOUT_SUB_DEVICE)
		return;

	subdev_manager[subdev].count--;
	if (subdev_manager[subdev].count == 0) {
		kfree(subdev_manager[subdev].substream);
		subdev_manager[subdev].count  = 0;
		subdev_manager[subdev].substream = NULL;
	}

	return;
}

static int aout_substream_get_count(enum aout_subdevice subdev)
{
	return subdev_manager[subdev].count;
}

static unsigned int aout_substream_get_max_dev(void)
{
	return MAX_AOUT_SUB_DEVICE;
}

static struct aout_reg_des regDes = {
#if defined(CONFIG_ARCH_LEO)
	.num   = 3,
#elif defined(CONFIG_ARCH_LEO_MPW)
	.num   = 4,
#endif
	.name  = {
		"gx8010.aout.opt.regs",
		"gx8010.aout.lodec.regs",
		"gx8010.aout.cold_rst_regs",
#if defined(CONFIG_ARCH_LEO_MPW)
		"gx8010.aout.irq.regs",
#endif
	},
	.baseAddr = {
		0x01200000,
		0x0030a1a0,
		0x0030a000,
#if defined(CONFIG_ARCH_LEO_MPW)
		0x0030a274,
#endif
	},
	.length = {
		sizeof(struct aout_reg),
		sizeof(struct aout_lodec),
		sizeof(unsigned int),
#if defined(CONFIG_ARCH_LEO_MPW)
		sizeof(struct aout_irq),
#endif
	},
};

static struct aout_irq_des irqDes = {
	.num       = 1,
	.name      = {"gx8010.aout.irqs"},
#if defined(CONFIG_ARCH_LEO)
	.irq       = {49},
#elif defined(CONFIG_ARCH_LEO_MPW)
	.irq       = {34},
#endif
	.irqFlags  = {IRQF_SHARED},
};

#define CHECK_SUBSTREAM(substream) do { \
	if (substream == NULL) return -1;   \
} while(0);

static irqreturn_t gx8010_core_interrupt(int irq, void *dev_t)
{
	int i = 0;
	struct aout_stream *stream = (struct aout_stream *)dev_t;

	for (i = 0; i < irqDes.num; i++) {
		if (irq == irqDes.irq[i])
			break;
	}

	if (i >= irqDes.num)
		return IRQ_NONE;

	return gx8010_stream_interrupt(stream)?IRQ_HANDLED:IRQ_NONE;
}

struct aout_stream *gx8010_core_int(struct platform_device *dev)
{
	int i = 0;
	struct aout_stream *stream = kmalloc(sizeof(struct aout_stream), GFP_KERNEL);

	if (stream == NULL)
		goto err0;

	for (i = 0; i < regDes.num; i++) {
		if (regDes.name[i]) {
			struct resource *reg = platform_get_resource_byname(dev, IORESOURCE_MEM, regDes.name[i]);
			if (reg) {
				regDes.baseAddr[i] = (unsigned int)reg->start;
				regDes.length[i]   = (unsigned int)(reg->end - reg->start);
			}
		}
	}

	for (i = 0; i < irqDes.num; i++) {
		if (irqDes.name[i]) {
			struct resource *irq = platform_get_resource_byname(dev, IORESOURCE_IRQ, irqDes.name[i]);
			if (irq) {
				irqDes.irq[i] = (unsigned int)irq->start;
			}
		}
	}

	stream->optReg = (struct aout_reg*)ioremap(regDes.baseAddr[0], regDes.length[0]);
	if (stream->optReg == NULL)
		goto err1;

	stream->lodecReg = (struct aout_lodec*)ioremap(regDes.baseAddr[1], regDes.length[1]);
	if (stream->lodecReg == NULL)
		goto err1;

	stream->rstReg = (unsigned int*)ioremap(regDes.baseAddr[2], regDes.length[2]);
	if (stream->rstReg == NULL)
		goto err1;

#if defined(CONFIG_ARCH_LEO_MPW)
	stream->irqReg = (struct aout_irq*)ioremap(regDes.baseAddr[3], regDes.length[3]);
	if (stream->irqReg == NULL)
		goto err1;
#endif

	gx8010_stream_init(stream, aout_substream_search, aout_substream_get_max_dev);

	for (i = 0; i < irqDes.num; i++) {
		if (0 != request_irq(irqDes.irq[i],
					gx8010_core_interrupt, irqDes.irqFlags[i], "gxalsa", (void*)stream)) {
			goto err2;
		}
	}

	return stream;

err2:
	for (i = 0; i < irqDes.num; i++)
		free_irq(irqDes.irq[i], (void*)stream);

err1:
	if (stream->optReg) {
		iounmap(stream->optReg);
		stream->optReg = NULL;
	}

	if (stream->lodecReg) {
		iounmap(stream->lodecReg);
		stream->lodecReg = NULL;
	}

	if (stream->rstReg) {
		iounmap(stream->rstReg);
		stream->rstReg = NULL;
	}

#if defined(CONFIG_ARCH_LEO_MPW)
	if (stream->irqReg) {
		iounmap(stream->irqReg);
		stream->irqReg = NULL;
	}
#endif

	kfree(stream);

err0:
	return NULL;
}

void gx8010_core_unit(struct aout_stream *stream)
{
	unsigned int i = 0;
	gx8010_stream_uninit(stream);

	for (i = 0; i < irqDes.num; i++)
		free_irq(irqDes.irq[i], (void*)stream);

	if (stream->optReg) {
		iounmap(stream->optReg);
		stream->optReg = NULL;
	}

	if (stream->lodecReg) {
		iounmap(stream->lodecReg);
		stream->lodecReg = NULL;
	}

	if (stream->rstReg) {
		iounmap(stream->rstReg);
		stream->rstReg = NULL;
	}

#if defined(CONFIG_ARCH_LEO_MPW)
	if (stream->irqReg) {
		iounmap(stream->irqReg);
		stream->irqReg = NULL;
	}
#endif

	kfree(stream);

	return;
}

int gx8010_core_power_suspend(struct aout_stream *stream)
{
	return gx8010_stream_power_suspend(stream);
}

int gx8010_core_power_resume(struct aout_stream *stream)
{
	return gx8010_stream_power_resume(stream);
}

int gx8010_core_set_global_volume(struct aout_stream *stream, long int dbValue)
{
	return gx8010_stream_set_global_volume(stream, dbValue);
}

int gx8010_core_get_global_volume(struct aout_stream *stream, long int *dbValue)
{
	return gx8010_stream_get_global_volume(stream, dbValue);
}

int gx8010_core_set_global_mute(struct aout_stream *stream, long int mute)
{
	return gx8010_stream_set_global_mute(stream, mute);
}

int gx8010_core_get_global_mute(struct aout_stream *stream, long int *mute)
{
	return gx8010_stream_get_global_mute(stream, mute);
}

int gx8010_core_set_global_track(struct aout_stream *stream, enum aout_track track)
{
	return gx8010_stream_set_global_track(stream, track);
}

int gx8010_core_get_global_track(struct aout_stream *stream, enum aout_track *track)
{
	return gx8010_stream_get_global_track(stream, track);
}

int gx8010_core_set_global_dac_mute(struct aout_stream *stream, long int dacMute)
{
	return gx8010_stream_set_global_dac_mute(stream, dacMute);
}

int gx8010_core_get_global_dac_mute(struct aout_stream *stream, long int *dacMute)
{
	return gx8010_stream_get_global_dac_mute(stream, dacMute);
}

int gx8010_core_set_global_dac_volume(struct aout_stream *stream, long int dacDBValue)
{
	return gx8010_stream_set_global_dac_volume(stream, dacDBValue);
}

int gx8010_core_get_global_dac_volume(struct aout_stream *stream, long int *dacDBValue)
{
	return gx8010_stream_get_global_dac_volume(stream, dacDBValue);
}

int gx8010_core_open(struct aout_stream *stream,
		enum aout_subdevice subdev,
		int (*__callback)(void *priv),
		void *priv)
{
	struct aout_substream *substream = aout_substream_create(subdev);

	CHECK_SUBSTREAM(substream);
	if (aout_substream_get_count(subdev) == 1) {
		substream->stream = stream;
		gx8010_stream_open(substream, __callback, priv);
	}

	return 0;
}

int gx8010_core_close(enum aout_subdevice subdev)
{
	struct aout_substream *substream = aout_substream_search(subdev);

	CHECK_SUBSTREAM(substream);
	if (aout_substream_get_count(subdev) == 1) {
		gx8010_stream_close(substream);
		substream->stream = NULL;
	}
	aout_substream_delete(subdev);

	return 0;
}

int gx8010_core_config(enum aout_subdevice subdev, struct aout_params *params)
{
	struct aout_substream *substream = aout_substream_search(subdev);

	CHECK_SUBSTREAM(substream);

	return gx8010_stream_config(substream, params);
}

int gx8010_core_run(enum aout_subdevice subdev)
{
	struct aout_substream *substream = aout_substream_search(subdev);

	CHECK_SUBSTREAM(substream);

	return gx8010_stream_run(substream);
}

int gx8010_core_rerun(enum aout_subdevice subdev)
{
	struct aout_substream *substream = aout_substream_search(subdev);

	CHECK_SUBSTREAM(substream);

	return gx8010_stream_rerun(substream);
}

int gx8010_core_stop(enum aout_subdevice subdev)
{
	struct aout_substream *substream = aout_substream_search(subdev);

	CHECK_SUBSTREAM(substream);

	return gx8010_stream_stop(substream);
}

int gx8010_core_drain(enum aout_subdevice subdev)
{
	struct aout_substream *substream = aout_substream_search(subdev);

	CHECK_SUBSTREAM(substream);
	CHECK_SUBSTREAM(substream->stream);

	gx8010_stream_interrupt(substream->stream);

	return 0;
}

int gx8010_core_pause(enum aout_subdevice subdev)
{
	struct aout_substream *substream = aout_substream_search(subdev);

	CHECK_SUBSTREAM(substream);

	return gx8010_stream_pause(substream);
}

int gx8010_core_resume(enum aout_subdevice subdev)
{
	struct aout_substream *substream = aout_substream_search(subdev);

	CHECK_SUBSTREAM(substream);

	return gx8010_stream_resume(substream);
}

int gx8010_core_read(enum aout_subdevice subdev, unsigned char *buf, unsigned int size)
{
	struct aout_substream *substream = aout_substream_search(subdev);

	CHECK_SUBSTREAM(substream);

	return gx8010_stream_read(substream, buf, size);
}

int gx8010_core_write(enum aout_subdevice subdev, unsigned char *buf, unsigned int size)
{
	struct aout_substream *substream = aout_substream_search(subdev);

	CHECK_SUBSTREAM(substream);

	return gx8010_stream_write(substream, buf, size);
}

int gx8010_core_set_priv(enum aout_subdevice subdev, void *priv)
{
	struct aout_substream *substream = aout_substream_search(subdev);

	CHECK_SUBSTREAM(substream);

	subdev_manager[subdev].priv = priv;

	return 0;
}

void *gx8010_core_get_priv(enum aout_subdevice subdev)
{
	struct aout_substream *substream = aout_substream_search(subdev);

	if (!substream) return NULL;

	return subdev_manager[subdev].priv;
}

unsigned int gx8010_core_free_space(enum aout_subdevice subdev)
{
	struct aout_substream *substream = aout_substream_search(subdev);

	CHECK_SUBSTREAM(substream);

	return gx8010_stream_free_space(substream);
}

int gx8010_core_set_volume(enum aout_subdevice subdev, long int dbValue)
{
	struct aout_substream *substream = aout_substream_search(subdev);

	defaultDBValue[subdev] = dbValue;
	if (substream == NULL)
		return 0;

	return gx8010_stream_set_volume(substream, dbValue);
}

int gx8010_core_get_volume(enum aout_subdevice subdev, long int *dbValue)
{
	struct aout_substream *substream = aout_substream_search(subdev);

	if (substream == NULL) {
		*dbValue = defaultDBValue[subdev];
		return 0;
	}

	return gx8010_stream_get_volume(substream, dbValue);
}

int gx8010_core_set_mute(enum aout_subdevice subdev, long int mute)
{
	struct aout_substream *substream = aout_substream_search(subdev);

	defaultMute[subdev] = mute;
	if (substream == NULL)
		return 0;

	return gx8010_stream_set_mute(substream, mute);
}

int gx8010_core_get_mute(enum aout_subdevice subdev, long int *mute)
{
	struct aout_substream *substream = aout_substream_search(subdev);

	if (substream == NULL) {
		*mute = defaultMute[subdev];
		return 0;
	}

	return gx8010_stream_get_mute(substream, mute);
}

int gx8010_core_set_track(enum aout_subdevice subdev, enum aout_track track)
{
	struct aout_substream *substream = aout_substream_search(subdev);

	defaultTrack[subdev] = track;
	if (substream == NULL)
		return 0;

	return gx8010_stream_set_track(substream, track);
}

int gx8010_core_get_track(enum aout_subdevice subdev, enum aout_track *track)
{
	struct aout_substream *substream = aout_substream_search(subdev);

	if (substream == NULL) {
		*track = defaultTrack[subdev];
		return 0;
	}

	return gx8010_stream_get_track(substream, track);
}

int gx8010_core_volume_range(long int *minDBValue, long int *maxDBValue)
{
	return gx8010_stream_volume_range(minDBValue, maxDBValue);
}

int gx8010_core_dac_volume_range(long int *minDBValue, long int *maxDBValue)
{
	return gx8010_stream_dac_volume_range(minDBValue, maxDBValue);
}
