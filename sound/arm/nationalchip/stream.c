/*
 *  ALSA Platform Device Driver
 *
 * Copyright (C) 1991-2017 NationalChip Co., Ltd
 * All rights reserved!
 *
 * core.c: ALSA Core Implement
 *
 */

#include <linux/types.h>
#include <linux/time.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <asm/cacheflush.h>
#include <asm/uaccess.h>

#include "config.h"
#include "reg.h"
#include "core.h"
#include "stream.h"

#define PERIODS_COUNT 3
#define PERIODS_POINTS 512

#if defined(AOUT_RECORD)
static struct aout_buffer  recordBuffer;
#endif

#define MAX_DB_VALUE (18)
#define MIN_DB_VALUE (-25)
#define DAC_MAX_DB_VALUE  (6)
#define DAC_MIN_DB_VALUE  (-100)
#define DAC_INIT_DB_VALUE (-2)
#define DAC_MAX_DB_INDEX  (0x7f)

struct stream_db_table {
	unsigned int dbIndex;
	long     int dbValue;
};

struct stream_db_table dbTable[] = {
	{0  , -25}, {1  , -24}, {2  , -23}, {4  , -22}, {6  , -21},
	{8  , -20}, {11 , -19}, {16 , -18}, {18 , -17}, {20 , -16},
	{23 , -15}, {25 , -14}, {28 , -13}, {32 , -12}, {36 , -11},
	{40 , -10}, {45 ,  -9}, {51 ,  -8}, {57 ,  -7}, {64 ,  -6},
	{72 ,  -5}, {81 ,  -4}, {91 ,  -3}, {102,  -2}, {114,  -1},
	{128,   0}, {144,   1}, {162,   2}, {181,   3}, {203,   4},
	{228,   5}, {256,   6}, {288,   7}, {323,   8}, {362,   9},
	{407,  10}, {256,  11}, {512,  12}, {576,  13}, {646,  14},
	{725,  15}, {814,  16}, {913,  17}, {1023, 18}
};

static unsigned int _map_dbIndex(long int dbValue)
{
	int i = 0;

	dbValue = (dbValue > MAX_DB_VALUE) ? MAX_DB_VALUE : dbValue;
	dbValue = (dbValue < MIN_DB_VALUE) ? MIN_DB_VALUE : dbValue;

	for (i = 0; i < sizeof(dbTable)/sizeof(struct stream_db_table); i++) {
		if (dbTable[i].dbValue  == dbValue)
			break;
	}

	return dbTable[i].dbIndex;
}

static int _set_loadec_volume(struct aout_stream *stream, long int dbValue)
{
	int dbIndex = DAC_MAX_DB_INDEX - (DAC_MAX_DB_VALUE - dbValue);

	REG_SET_VAL(&(stream->lodecReg->LODEC), ((0x3<<12)|(dbIndex<<4)|(0x1<<2)|(0x1<<0)));
	REG_SET_BIT(&(stream->lodecReg->LODEC), 1);
	REG_CLR_BIT(&(stream->lodecReg->LODEC), 1);
	stream->globalDacDBValue = dbValue;

	return 0;
}

static int _set_loadec_mute(struct aout_stream *stream, bool enable)
{
	if (enable) {
		REG_SET_VAL(&(stream->lodecReg->LODEC), ((0x3<<12)|(0xf9<<4)|(0x1<<2)|(0x1<<0)));
		REG_SET_BIT(&(stream->lodecReg->LODEC), 1);
		REG_CLR_BIT(&(stream->lodecReg->LODEC), 1);
		stream->globalDacMute = 1;
	} else {
		REG_SET_VAL(&(stream->lodecReg->LODEC), ((0x3<<12)|(0x79<<4)|(0x1<<2)|(0x1<<0)));
		REG_SET_BIT(&(stream->lodecReg->LODEC), 1);
		REG_CLR_BIT(&(stream->lodecReg->LODEC), 1);
		stream->globalDacMute = 0;
	}

	return 0;
}

static int _set_loadec(struct aout_stream *stream)
{
	int dbIndex = DAC_MAX_DB_INDEX - (DAC_MAX_DB_VALUE - DAC_INIT_DB_VALUE);

	REG_SET_BIT(&(stream->lodecReg->LODEC), 0);

	REG_SET_BIT(&(stream->lodecReg->LODEC), 1);
	REG_CLR_BIT(&(stream->lodecReg->LODEC), 1);

	REG_SET_BIT(&(stream->lodecReg->LODEC), 1);
	REG_CLR_BIT(&(stream->lodecReg->LODEC), 1);

	switch (aout_dac_get_mclk(stream->optReg)) {
	case I2S_DAC_FS_256:
		REG_SET_VAL(&(stream->lodecReg->LODEC), ((0x0<<12)|(0xa8<<4)|(0x1<<2)|(0x1<<0)));
		break;
	case I2S_DAC_FS_512:
		REG_SET_VAL(&(stream->lodecReg->LODEC), ((0x0<<12)|(0x98<<4)|(0x1<<2)|(0x1<<0)));
		break;
	case I2S_DAC_FS_1024:
		REG_SET_VAL(&(stream->lodecReg->LODEC), ((0x0<<12)|(0x88<<4)|(0x1<<2)|(0x1<<0)));
		break;
	default:
		break;
	}

	REG_SET_BIT(&(stream->lodecReg->LODEC), 1);
	REG_CLR_BIT(&(stream->lodecReg->LODEC), 1);
	REG_SET_VAL(&(stream->lodecReg->LODEC), ((0x0<<12)|(0x1<<3)|(0x1<<0)));

	REG_SET_BIT(&(stream->lodecReg->LODEC), 1);
	REG_CLR_BIT(&(stream->lodecReg->LODEC), 1);
	REG_SET_VAL(&(stream->lodecReg->LODEC), ((0x2<<12)|(0x38<<4)|(0x1<<2)|(0x1<<0)));

	REG_SET_BIT(&(stream->lodecReg->LODEC), 1);
	REG_CLR_BIT(&(stream->lodecReg->LODEC), 1);

	REG_SET_VAL(&(stream->lodecReg->LODEC), ((0x3<<12)|(dbIndex<<4)|(0x1<<2)|(0x1<<0))); //-2db
	REG_SET_BIT(&(stream->lodecReg->LODEC), 1);
	REG_CLR_BIT(&(stream->lodecReg->LODEC), 1);

	return 0;
}

#if defined(CONFIG_ARCH_LEO_MPW)
static void _clr_external_irq(struct aout_stream *stream)
{
	if (REG_GET_BIT(&(stream->irqReg->INT_STATUS), 12))
		REG_SET_VAL(&(stream->irqReg->INT_CLEAR), 0x1<<12);
}
#endif

static int _playback_start(struct aout_substream *substream)
{
	struct aout_stream *stream = substream->stream;
	struct aout_buffer *buf    = &substream->buffer;
	unsigned int frameBytes    = ((buf->writeAddr + buf->size - buf->frameAddr) % buf->size);

	if (substream->status == AOUT_PAUSED)
		return 0;

	if (frameBytes >= substream->periodBytes) {
		unsigned int newFrameStartAddr = buf->frameAddr;
		unsigned int newFrameEndAddr   = (buf->frameAddr + substream->periodBytes - 1) % buf->size;

//		printk("readAddr %x\t writeAddr %x\t FrameStartAddr %x\t FrameEndAddr %x\n",
//				buf->readAddr, buf->writeAddr, newFrameStartAddr, newFrameEndAddr);
		__cpuc_flush_dcache_area((void*)buf->startAddr,  buf->alignSize);
		outer_flush_range(buf->physAddr, buf->physAddr + buf->alignSize);
		aout_dev_set_frame_s_addr(stream->optReg, (0x1<<substream->subdev), newFrameStartAddr);
		aout_dev_set_frame_e_addr(stream->optReg, (0x1<<substream->subdev), newFrameEndAddr);
		aout_dev_set_frame_finish(stream->optReg, (0x1<<substream->subdev), 1);
		buf->frameAddr = (newFrameEndAddr + 1) % buf->size;
		substream->comeFrameCount++;
		substream->needData = 0;
	} else {
		substream->needData = 1;
	}

	return 0;
}

static int _playback_irq(struct aout_substream *substream)
{
	struct aout_stream *stream = substream->stream;
	struct aout_buffer *buf    = &substream->buffer;
	unsigned int frameReadAddr = 0;

	aout_dev_clr_irq_status(stream->optReg, (0x1<<substream->subdev));
	frameReadAddr = aout_dev_get_frame_r_addr(stream->optReg, (0x1<<substream->subdev));
	buf->readAddr = (frameReadAddr / substream->periodBytes) * substream->periodBytes;

#if defined(CONFIG_ARCH_LEO_MPW)
	aout_dev_set_irq_enable(stream->optReg, (0x1<<substream->subdev), true);
#endif

	if (substream->consume_callback)
		substream->consume_callback(substream->priv);

	_playback_start(substream);
	return 0;
}

static int _capture_start(struct aout_substream *substream)
{
	return 0;
}

static int _capture_irq(struct aout_substream *substream)
{
	return 0;
}

static unsigned int _free_space(struct aout_substream *substream)
{
	struct aout_buffer *buf = &substream->buffer;
	unsigned int freeSpace  = 0;

	if (buf->writeAddr == buf->readAddr)
		freeSpace = buf->size;
	else
		freeSpace = ((buf->readAddr + buf->size - buf->writeAddr) % buf->size);

	if (freeSpace > substream->periodBytes)
		freeSpace = (freeSpace - substream->periodBytes);
	else
		freeSpace = 0;

	return freeSpace;
}

static int _wait_frame_over(struct aout_substream *substream)
{
	struct aout_stream *stream = substream->stream;

	aout_dev_set_frame_over_irq_enable(stream->optReg, (0x1<<substream->subdev), true);
	return wait_for_completion_timeout(&stream->pauseComp, WAIT_TIME);
}

static int _stop_substream(struct aout_substream *substream)
{
	int ret = 0;
	struct aout_stream *stream = substream->stream;

#if defined(CONFIG_ARCH_LEO_MPW)
	substream->optIrq = 0;
#endif

	if (substream->status == AOUT_IDLE ||
			substream->status == AOUT_READY)
		return 0;

	ret = _wait_frame_over(substream);
	if (ret == 0)
		printk("%s %d time out\n", __FUNCTION__, __LINE__);

	aout_dev_set_irq_enable(stream->optReg, (0x1<<substream->subdev), false);
	aout_dev_clr_irq_status(stream->optReg, (0x1<<substream->subdev));

	aout_dev_set_buffer_s_addr(stream->optReg, (0x1<<substream->subdev), 0x0);
	aout_dev_set_buffer_size  (stream->optReg, (0x1<<substream->subdev), 0x0);
	aout_dev_set_frame_s_addr (stream->optReg, (0x1<<substream->subdev), 0x0);
	aout_dev_set_frame_e_addr (stream->optReg, (0x1<<substream->subdev), 0x0);
	aout_dev_set_work         (stream->optReg, (0x1<<substream->subdev), false);
	aout_dev_set_mix          (stream->optReg, (0x1<<substream->subdev), false);

	free_pages((unsigned long)substream->buffer.startAddr, get_order(substream->buffer.alignSize));
	substream->buffer.startAddr = 0;
	substream->buffer.physAddr  = 0;
	substream->buffer.size      = 0;
	substream->buffer.alignSize = 0;
	substream->buffer.readAddr  = 0;
	substream->buffer.writeAddr = 0;

	substream->status = AOUT_IDLE;

#if defined(AOUT_RECORD)
	__cpuc_flush_dcache_area((void*)recordBuffer.startAddr,  recordBuffer.size);
	outer_flush_range(recordBuffer.physAddr, recordBuffer.physAddr + recordBuffer.size);
	printk("startAddr:0x%08x, physAddr: 0x%08x, writeAddr: 0x%x\n",
			recordBuffer.startAddr, recordBuffer.physAddr, recordBuffer.writeAddr);
	recordBuffer.readAddr  = 0;
	recordBuffer.writeAddr = 0;
#endif

	return 0;
}

int gx8010_stream_init(struct aout_stream *stream,
		struct aout_substream* (*search_substream_callback)(enum aout_subdevice subdev),
		unsigned int (*max_subdev_callback)(void))
{
	aout_spd_set_mode  (stream->optReg, SPD_PLAY_OFF);
	aout_spd_set_enable(stream->optReg, false);
	aout_spd_set_mute  (stream->optReg, false);
	aout_i2s_set_mute  (stream->optReg, false);
	aout_src_set_enable(stream->optReg, false);
	aout_dac_set_mclk  (stream->optReg, I2S_DAC_FS_256);
	aout_dac_set_bits  (stream->optReg, I2S_DAC_BIT_24);
	aout_dac_set_format(stream->optReg, I2S_DAC_FORMAT_I2S);
	aout_dac_set_l_channel(stream->optReg, CHANNEL_0);
	aout_dac_set_r_channel(stream->optReg, CHANNEL_1);
#if defined(CONFIG_ARCH_LEO_MPW)
	aout_irq_set_enable(stream->irqReg, true);
#endif

	_set_loadec(stream);

#if defined(AOUT_RECORD)
	recordBuffer.size      = AOUT_RECORD_BUFFER_SIZE;
	recordBuffer.startAddr =
		(unsigned int)__get_free_pages(GFP_KERNEL|__GFP_REPEAT, get_order(recordBuffer.size));
	recordBuffer.physAddr  = virt_to_phys((void *)recordBuffer.startAddr);
	recordBuffer.readAddr  = 0;
	recordBuffer.writeAddr = 0;
#endif

	stream->globalMute                = 0;
	stream->globalDacMute             = 0;
	stream->globalDBValue             = 0;
	stream->globalDacDBValue          = -2;
	stream->globalTrack               = STEREO_TRACK;
	stream->max_subdev_callback       = max_subdev_callback;
	stream->search_substream_callback = search_substream_callback;

	return 0;
}

int gx8010_stream_uninit(struct aout_stream *stream)
{
#if defined(AOUT_RECORD)
	free_pages((unsigned long)recordBuffer.startAddr, get_order(recordBuffer.size));
	recordBuffer.startAddr = 0;
	recordBuffer.physAddr  = 0;
	recordBuffer.size      = 0;
	recordBuffer.writeAddr = 0;
#endif

#if defined(CONFIG_ARCH_LEO_MPW)
	aout_irq_set_enable(stream->irqReg, false);
#endif
	aout_spd_set_enable(stream->optReg, false);
	aout_spd_set_mute  (stream->optReg, true);
	aout_i2s_set_mute  (stream->optReg, true);

	return 0;
}

static int _wait_axi_idle(struct aout_stream *stream)
{
	aout_dev_set_axi_idle_irq_enable(stream->optReg, true);
	return wait_for_completion_timeout(&stream->suspendComp, WAIT_TIME);
}

int gx8010_stream_power_suspend(struct aout_stream *stream)
{
	int ret = 0;

	aout_cpu_request_stop(stream->optReg);
	ret = _wait_axi_idle(stream);
	if (ret == 0)
		printk("%s %d time out\n", __FUNCTION__, __LINE__);
	aout_cpu_set_reset_enable(stream->optReg, true);

	return 0;
}

int gx8010_stream_power_resume(struct aout_stream *stream)
{
	aout_cpu_set_reset_enable(stream->optReg, false);
	aout_cpu_request_open(stream->optReg);

	return 0;
}

static int _post_frame_over(struct aout_substream *substream)
{
	struct aout_buffer *buf    = &substream->buffer;
	struct aout_stream *stream = substream->stream;
	unsigned int frameReadAddr = 0;

	frameReadAddr = aout_dev_get_frame_r_addr(stream->optReg, (0x1<<substream->subdev));
	buf->readAddr = (frameReadAddr / substream->periodBytes) * substream->periodBytes;

	aout_dev_set_frame_over_irq_enable(stream->optReg, (0x1<<substream->subdev), false);
	aout_dev_clr_frame_over_irq_status(stream->optReg, (0x1<<substream->subdev));
	complete(&stream->pauseComp);

	return 0;
}

static int _post_axi_idle(struct aout_substream *substream)
{
	struct aout_stream *stream = substream->stream;

	aout_dev_set_axi_idle_irq_enable(stream->optReg, false);
	aout_dev_clr_axi_idle_irq_status(stream->optReg);
	complete(&stream->suspendComp);

	return 0;
}

int gx8010_stream_interrupt(struct aout_stream *stream)
{
	unsigned int completeFlags  = 0;
	unsigned int audioIrqStatus = aout_dev_get_irq_status(stream->optReg);
	enum aout_subdevice subdev  = AOUT_PLAYBACK0;

	for (subdev = 0; subdev < stream->max_subdev_callback(); subdev++) {
		struct aout_substream *substream = stream->search_substream_callback(subdev);

		if (substream == NULL)
			continue;

#if defined(CONFIG_ARCH_LEO_MPW)
		if (!substream->optIrq) {
			aout_dev_set_irq_enable(stream->optReg, (0x1<<substream->subdev), false);
			aout_dev_clr_irq_status(stream->optReg, 0x1<<substream->subdev);
			continue;
		}
#endif
		switch (substream->subdev) {
		case AOUT_PLAYBACK0:
			if (audioIrqStatus & (0x1 << 0)) {
				_playback_irq(substream);
				completeFlags = 1;
			}

			if (audioIrqStatus & (0x1 << 9))
				_post_frame_over(substream);

			if (audioIrqStatus & (0x1 << 20))
				_post_axi_idle(substream);
			break;
		case AOUT_PLAYBACK1:
			if (audioIrqStatus & (0x1 << 1)) {
				_playback_irq(substream);
				completeFlags = 1;
			}

			if (audioIrqStatus & (0x1 << 10))
				_post_frame_over(substream);

			if (audioIrqStatus & (0x1 << 20))
				_post_axi_idle(substream);
			break;
		case AOUT_PLAYBACK2:
			if (audioIrqStatus & (0x1 << 2)) {
				_playback_irq(substream);
				completeFlags = 1;
			}

			if (audioIrqStatus & (0x1 << 11))
				_post_frame_over(substream);

			if (audioIrqStatus & (0x1 << 20))
				_post_axi_idle(substream);
			break;
		case AOUT_CAPTURE:
			if (audioIrqStatus & (0x1 << 8)) {
				_capture_irq(substream);
				completeFlags = 1;
			}

			if (audioIrqStatus & (0x1 << 20))
				_post_axi_idle(substream);
			break;
		default:
			break;
		}
	}
#if defined(CONFIG_ARCH_LEO_MPW)
	_clr_external_irq(stream);
#endif

	return completeFlags;
}

int gx8010_stream_set_global_volume(struct aout_stream *stream, long int dbValue)
{
	enum aout_subdevice subdev = AOUT_PLAYBACK0;

	for (subdev = 0; subdev < stream->max_subdev_callback(); subdev++) {
		struct aout_substream *substream = stream->search_substream_callback(subdev);
		unsigned int dbIndex = 0;

		if (substream == NULL)
			continue;

		dbIndex = _map_dbIndex(substream->dbValue + dbValue);
		aout_dev_set_volume(stream->optReg, (0x1<<subdev), dbIndex);
	}

	stream->globalDBValue = dbValue;

	return 0;
}

int gx8010_stream_get_global_volume(struct aout_stream *stream, long int *dbValue)
{
	if (dbValue)
		*dbValue = stream->globalDBValue;

	return 0;
}

int gx8010_stream_set_global_mute(struct aout_stream *stream, long int mute)
{
	if (mute) {
		aout_i2s_set_mute(stream->optReg, true);
		aout_spd_set_mute(stream->optReg, true);
		stream->globalMute = 1;
	} else {
		aout_i2s_set_mute(stream->optReg, false);
		aout_spd_set_mute(stream->optReg, false);
		stream->globalMute = 0;
	}

	return 0;
}

int gx8010_stream_get_global_track(struct aout_stream *stream, enum aout_track *track)
{
	if (track)
		*track = stream->globalTrack;

	return 0;
}

int gx8010_stream_set_global_track(struct aout_stream *stream, enum aout_track track)
{
#if defined (CONFIG_ARCH_LEO_MPW)
	stream->globalTrack = STEREO_TRACK;
#endif

	return 0;
}

int gx8010_stream_get_global_mute(struct aout_stream *stream, long int *mute)
{
	*mute = (long int)stream->globalMute;

	return 0;
}

int gx8010_stream_get_global_dac_mute(struct aout_stream *stream, long int *dacMute)
{
	if (dacMute)
		*dacMute = (long int)stream->globalDacMute;

	return 0;
}

int gx8010_stream_set_global_dac_mute(struct aout_stream *stream, long int dacMute)
{
	_set_loadec_mute(stream, dacMute);

	return 0;
}

int gx8010_stream_get_global_dac_volume(struct aout_stream *stream, long int *dacDBValue)
{
	if (dacDBValue)
		*dacDBValue = (long int)stream->globalDacDBValue;

	return 0;
}

int gx8010_stream_set_global_dac_volume(struct aout_stream *stream, long int dacDBValue)
{
	_set_loadec_volume(stream, dacDBValue);

	return 0;
}

int gx8010_stream_open(struct aout_substream *substream,
		int (*__callback)(void *priv),
		void *priv)
{
	struct aout_stream *stream = substream->stream;
	unsigned int dbIndex = 0;

#if defined(CONFIG_ARCH_LEO_MPW)
	substream->optIrq = 0;
#endif

	substream->currentVolume = 0;
	substream->targetVolume  = 0;
	substream->dbValue       = 0;
	dbIndex = _map_dbIndex(substream->dbValue + stream->globalDBValue);

	aout_dev_set_mono     (stream->optReg, (0x1<<substream->subdev), false);
	aout_dev_set_mix      (stream->optReg, (0x1<<substream->subdev), false);
	aout_dev_set_work     (stream->optReg, (0x1<<substream->subdev), false);
	aout_dev_set_source   (stream->optReg, (0x1<<substream->subdev), DEV_SRC_OFF);
	aout_dev_set_volume   (stream->optReg, (0x1<<substream->subdev), dbIndex);
	aout_dev_set_r_channel(stream->optReg, (0x1<<substream->subdev), CHANNEL_1);
	aout_dev_set_l_channel(stream->optReg, (0x1<<substream->subdev), CHANNEL_0);
	aout_dev_set_r_mute   (stream->optReg, (0x1<<substream->subdev), false);
	aout_dev_set_l_mute   (stream->optReg, (0x1<<substream->subdev), false);

#if defined(CONFIG_ARCH_LEO)
	aout_dev_set_i2s_disable (stream->optReg, false);
	aout_cpu_set_reset_enable(stream->optReg, false);
#endif

	if (substream->subdev == AOUT_CAPTURE) {
		substream->produce_callback = __callback;
		substream->priv             = priv;
	} else {
		substream->consume_callback = __callback;
		substream->priv             = priv;
	}

	init_completion(&stream->pauseComp);
	init_completion(&stream->suspendComp);
	substream->status = AOUT_IDLE;

	return 0;
}

int gx8010_stream_close(struct aout_substream *substream)
{
	struct aout_stream *stream = substream->stream;

	_stop_substream(substream);

#if defined(CONFIG_ARCH_LEO_MPW)
	substream->optIrq = 0;
#endif

	aout_dev_set_r_mute(stream->optReg, (0x1<<substream->subdev), true);
	aout_dev_set_l_mute(stream->optReg, (0x1<<substream->subdev), true);
	aout_dev_set_mix   (stream->optReg, (0x1<<substream->subdev), false);
	aout_dev_set_work  (stream->optReg, (0x1<<substream->subdev), false);
	aout_dev_set_source(stream->optReg, (0x1<<substream->subdev), DEV_SRC_OFF);

#if defined(CONFIG_ARCH_LEO)
	aout_dev_set_i2s_disable (stream->optReg, true);
#endif
	substream->produce_callback = NULL;
	substream->consume_callback = NULL;
	substream->priv             = NULL;
	substream->status = AOUT_IDLE;

	return 0;
}

int gx8010_stream_config(struct aout_substream *substream, struct aout_params *params)
{
	struct aout_stream *stream = substream->stream;
	unsigned int channels = 0, bytes = 0;

	if (substream->status >= AOUT_READY)
		return 0;

	if (substream->subdev == AOUT_PLAYBACK2 &&
			(params->sampleRate != SAMPLERATE_48KHZ))
		return -1;

	if ((substream->subdev == AOUT_CAPTURE) &&
			(params->bitSize != BIT16) &&
			(params->interlace != INTERLACE_FALSE))
		return -1;

	switch (params->channelNum) {
		case SIGNAL_CHANNEL:
			channels = 1;
			break;
		case DOUBLE_CHANNEL:
			channels = 2;
			break;
		default:
			return -1;
	}

	switch (params->bitSize) {
		case BIT16:
			bytes = 2;
			break;
		case BIT32:
			bytes = 4;
			break;
		default:
			return -1;
	}

	substream->periodPoints  = PERIODS_POINTS;
	substream->periodBytes   = substream->periodPoints * channels * bytes;
	substream->interlace     = params->interlace;
	substream->channelNum    = params->channelNum;

	if (substream->subdev == AOUT_CAPTURE) {
		if ((params->sampleRate == SAMPLERATE_8KHZ))
			aout_dev_set_samplerate(stream->optReg, (0x1<<substream->subdev), 1);
		else if (params->sampleRate == SAMPLERATE_16KHZ)
			aout_dev_set_samplerate(stream->optReg, (0x1<<substream->subdev), 0);
		else if (params->sampleRate == SAMPLERATE_48KHZ)
			aout_dev_set_samplerate(stream->optReg, (0x1<<substream->subdev), 2);
		else
		  return -1;

		aout_dev_set_silent(stream->optReg, true);
		aout_dev_set_points(stream->optReg, (0x1<<substream->subdev), substream->periodPoints);
		aout_dev_set_endian(stream->optReg, (0x1<<substream->subdev), params->endian);
		aout_dev_set_ch_num(stream->optReg, (0x1<<substream->subdev), params->channelNum);
	} else {
		aout_dev_set_source    (stream->optReg, (0x1<<AOUT_CAPTURE),      0);
		aout_dev_set_source    (stream->optReg, (0x1<<substream->subdev), substream->subdev);
		aout_dev_set_points    (stream->optReg, (0x1<<substream->subdev), substream->periodPoints);
		aout_dev_set_endian    (stream->optReg, (0x1<<substream->subdev), params->endian);
		aout_dev_set_ch_num    (stream->optReg, (0x1<<substream->subdev), params->channelNum);
		aout_dev_set_bit_size  (stream->optReg, (0x1<<substream->subdev), params->bitSize);
		aout_dev_set_samplerate(stream->optReg, (0x1<<substream->subdev), params->sampleRate);
		aout_dev_set_interlace (stream->optReg, (0x1<<substream->subdev), params->interlace);
	}

	//hardware need 1024 multi data
	substream->buffer.size      = (PERIODS_COUNT * substream->periodBytes);
	substream->buffer.alignSize = substream->buffer.size;
	if (substream->buffer.alignSize % 1024)
		substream->buffer.alignSize = (substream->buffer.alignSize / 1024 + 1) * 1024;
	substream->buffer.startAddr =
		(unsigned int)__get_free_pages(GFP_KERNEL|__GFP_REPEAT, get_order(substream->buffer.alignSize));
	substream->buffer.physAddr  = virt_to_phys((void *)substream->buffer.startAddr);
	aout_dev_set_buffer_s_addr(stream->optReg, (0x1<<substream->subdev), substream->buffer.physAddr);
	aout_dev_set_buffer_size  (stream->optReg, (0x1<<substream->subdev), substream->buffer.alignSize);

	substream->buffer.readAddr  = 0;
	substream->buffer.writeAddr = 0;
	substream->buffer.frameAddr = 0;
	substream->status           = AOUT_READY;
	substream->needData         = 0;
	substream->comeFrameCount   = 0;

	return 0;
}

int gx8010_stream_run(struct aout_substream *substream)
{
	struct aout_stream *stream = substream->stream;

	if (substream->status == AOUT_READY) {
		substream->status = AOUT_RUNNING;
#if defined(CONFIG_ARCH_LEO_MPW)
		substream->optIrq         = 1;
#endif

		aout_dev_set_mix       (stream->optReg, (0x1<<substream->subdev), true);
		aout_dev_set_work      (stream->optReg, (0x1<<substream->subdev), true);
		aout_dev_set_irq_enable(stream->optReg, (0x1<<substream->subdev), true);
	}

	if (substream->status != AOUT_RUNNING)
		return -1;

	if (substream->subdev == AOUT_CAPTURE)
		_capture_start(substream);
	else
		_playback_start(substream);

	return 0;
}

int gx8010_stream_rerun(struct aout_substream *substream)
{
	if (substream->needData) {
		substream->needData = 0;
		_playback_start(substream);
	}

	return 0;
}

int gx8010_stream_stop(struct aout_substream *substream)
{
	return 0;
}

int gx8010_stream_pause(struct aout_substream *substream)
{
	int ret = 0;
	struct aout_stream *stream = substream->stream;

	if (substream->status != AOUT_RUNNING)
		return -1;

#if defined(CONFIG_ARCH_LEO_MPW)
	substream->optIrq = 0;
#endif
	substream->status = AOUT_PAUSED;

	ret = _wait_frame_over(substream);
	if (ret == 0)
		printk("%s %d time out\n", __FUNCTION__, __LINE__);

	aout_dev_set_irq_enable(stream->optReg, (0x1<<substream->subdev), false);

	return 0;
}

int gx8010_stream_resume(struct aout_substream *substream)
{
	struct aout_stream *stream = substream->stream;

	if (substream->status != AOUT_PAUSED)
		return -1;

#if defined(CONFIG_ARCH_LEO_MPW)
	substream->optIrq = 1;
#endif
	aout_dev_set_irq_enable(stream->optReg, (0x1<<substream->subdev), true);
	substream->status = AOUT_RUNNING;
	_playback_start(substream);

	return 0;
}

int gx8010_stream_read(struct aout_substream *substream, unsigned char *buffer, unsigned int size)
{
	printk("=======> %s %d\n", __FUNCTION__, __LINE__);
	return 0;
}

int gx8010_stream_write(struct aout_substream *substream, unsigned char *buffer, unsigned int size)
{
	struct aout_buffer *buf = &substream->buffer;
	unsigned int freeSpace  = _free_space(substream);
	unsigned int writeSize  = (freeSpace > size) ? size : freeSpace;

	if (writeSize == 0)
		return 0;

#if defined(AOUT_RECORD)
	if (recordBuffer.writeAddr < recordBuffer.size) {
		unsigned int dstAddr = recordBuffer.startAddr + recordBuffer.writeAddr;
		unsigned int srcAddr = (unsigned int)buffer;

		memcpy((void*)dstAddr, (void*)srcAddr, writeSize);
		recordBuffer.writeAddr += writeSize;
	}
#endif

	if ((buf->writeAddr + writeSize) > buf->size) {
		unsigned int copyLen = buf->size - buf->writeAddr;
		unsigned int dstAddr = buf->startAddr + buf->writeAddr;
		unsigned int srcAddr = (unsigned int)buffer;

		memcpy((void*)dstAddr, (void*)srcAddr, copyLen);

		dstAddr = buf->startAddr;
		srcAddr += copyLen;
		copyLen  = writeSize - copyLen;
		memcpy((void*)dstAddr, (void*)srcAddr, copyLen);
	} else {
		unsigned int copyLen = writeSize;
		unsigned int dstAddr = buf->startAddr + buf->writeAddr;
		unsigned int srcAddr = (unsigned int)buffer;

		memcpy((void*)dstAddr, (void*)srcAddr, copyLen);
	}

	buf->writeAddr = (buf->writeAddr + writeSize) % buf->size;

	return writeSize;
}

unsigned int gx8010_stream_free_space(struct aout_substream *substream)
{
	return _free_space(substream);
}

int gx8010_stream_set_volume(struct aout_substream *substream, long int dbValue)
{
	struct aout_stream *stream = substream->stream;
	unsigned int dbIndex = _map_dbIndex(dbValue + stream->globalDBValue);

	aout_dev_set_volume(stream->optReg, (0x1<<substream->subdev), dbIndex);
	substream->dbValue = dbValue;

	return 0;
}

int gx8010_stream_get_volume(struct aout_substream *substream, long int *dbValue)
{
	*dbValue = (long int)substream->dbValue;

	return 0;
}

int gx8010_stream_set_mute(struct aout_substream *substream, long int  mute)
{
	struct aout_stream *stream = substream->stream;

	if (mute) {
		aout_dev_set_l_mute(stream->optReg, (0x1<<substream->subdev), true);
		aout_dev_set_r_mute(stream->optReg, (0x1<<substream->subdev), true);
		substream->mute = 1;
	} else {
		aout_dev_set_l_mute(stream->optReg, (0x1<<substream->subdev), false);
		aout_dev_set_r_mute(stream->optReg, (0x1<<substream->subdev), false);
		substream->mute = 0;
	}

	return 0;
}

int gx8010_stream_get_mute(struct aout_substream *substream, long int *mute)
{
	*mute = (long int)substream->mute;

	return 0;
}

int gx8010_stream_set_track(struct aout_substream *substream, enum aout_track track)
{
	struct aout_stream *stream = substream->stream;

	if (track == STEREO_TRACK) {
		aout_dev_set_r_channel(stream->optReg, (0x1<<substream->subdev), CHANNEL_1);
		aout_dev_set_l_channel(stream->optReg, (0x1<<substream->subdev), CHANNEL_0);
	} else if (track == LEFT_TRACK) {
		aout_dev_set_r_channel(stream->optReg, (0x1<<substream->subdev), CHANNEL_0);
		aout_dev_set_l_channel(stream->optReg, (0x1<<substream->subdev), CHANNEL_0);
	} else if (track == RIGHT_TRACK) {
		aout_dev_set_r_channel(stream->optReg, (0x1<<substream->subdev), CHANNEL_1);
		aout_dev_set_l_channel(stream->optReg, (0x1<<substream->subdev), CHANNEL_1);
	}
	substream->track = track;

	return 0;
}

int gx8010_stream_get_track(struct aout_substream *substream, enum aout_track *track)
{
	*track = substream->track;

	return 0;
}

int gx8010_stream_volume_range(long int *minDBValue, long int *maxDBValue)
{
	if (minDBValue)
		*minDBValue = MIN_DB_VALUE;

	if (maxDBValue)
		*maxDBValue = MAX_DB_VALUE;

	return 0;
}

int gx8010_stream_dac_volume_range(long int *minDBValue, long int *maxDBValue)
{
	if (minDBValue)
		*minDBValue = DAC_MIN_DB_VALUE;

	if (maxDBValue)
		*maxDBValue = DAC_MAX_DB_VALUE;

	return 0;
}
