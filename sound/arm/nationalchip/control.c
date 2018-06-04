/*
 *  ALSA Platform Device Driver
 *
 * Copyright (C) 1991-2017 NationalChip Co., Ltd
 * All rights reserved!
 *
 * control.c: ALSA Control Implement
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

#include "control.h"
#include "core.h"

static int gx8010_control_volume_info(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_info *uinfo)
{
	uinfo->type  = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 1;

	gx8010_core_volume_range(&uinfo->value.integer.min, &uinfo->value.integer.max);
	uinfo->value.integer.step = 1;

	return 0;
}

static int gx8010_control_volume_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	int ret = -1;

	if (kcontrol->id.iface == SNDRV_CTL_ELEM_IFACE_CARD) {
		struct snd_card *card = kcontrol->private_data;
		struct aout_stream *stream = card->private_data;

		ret = gx8010_core_get_global_volume(stream, &ucontrol->value.integer.value[0]);
	} else {
		enum aout_subdevice subdev = kcontrol->private_value;

		ret = gx8010_core_get_volume(subdev, &ucontrol->value.integer.value[0]);
	}

	return ret;
}

static int gx8010_control_volume_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	int ret = -1;

	if (kcontrol->id.iface == SNDRV_CTL_ELEM_IFACE_CARD) {
		struct snd_card *card = kcontrol->private_data;
		struct aout_stream *stream = card->private_data;

		ret = gx8010_core_set_global_volume(stream, ucontrol->value.integer.value[0]);
	} else {
		enum aout_subdevice subdev = kcontrol->private_value;

		ret = gx8010_core_set_volume(subdev, ucontrol->value.integer.value[0]);
	}

	return ret;
}

static int gx8010_control_mute_info(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_info *uinfo)
{
	uinfo->type  = SNDRV_CTL_ELEM_TYPE_BOOLEAN;
	uinfo->count = 1;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 1;
	uinfo->value.integer.step = 1;

	return 0;
}

static int gx8010_control_mute_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	int ret = -1;

	if (kcontrol->id.iface == SNDRV_CTL_ELEM_IFACE_CARD) {
		struct snd_card *card = kcontrol->private_data;
		struct aout_stream *stream = card->private_data;

		ret = gx8010_core_get_global_mute(stream, &ucontrol->value.integer.value[0]);
	} else {
		enum aout_subdevice subdev = kcontrol->private_value;

		ret = gx8010_core_get_mute(subdev, &ucontrol->value.integer.value[0]);
	}

	return ret;
}

static int gx8010_control_mute_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	int ret = -1;

	if (kcontrol->id.iface == SNDRV_CTL_ELEM_IFACE_CARD) {
		struct snd_card *card = kcontrol->private_data;
		struct aout_stream *stream = card->private_data;

		ret = gx8010_core_set_global_mute(stream, ucontrol->value.integer.value[0]);
	} else {
		enum aout_subdevice subdev = kcontrol->private_value;

		ret = gx8010_core_set_mute(subdev, ucontrol->value.integer.value[0]);
	}

	return ret;
}

static int gx8010_control_track_info(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_info *uinfo)
{
	const char *track[4] = {"stero", "left", "right"};
	uinfo->type  = SNDRV_CTL_ELEM_TYPE_ENUMERATED;
	uinfo->count = 1;
	uinfo->value.enumerated.items = 3;
	uinfo->value.enumerated.item  = (uinfo->value.enumerated.item >= 3) ? 3 : uinfo->value.enumerated.item;
	strcpy(uinfo->value.enumerated.name, track[uinfo->value.enumerated.item]);
	return 0;
}

static int gx8010_control_track_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	int ret = -1;

	if (kcontrol->id.iface == SNDRV_CTL_ELEM_IFACE_CARD) {
		struct snd_card *card = kcontrol->private_data;
		struct aout_stream *stream = card->private_data;

		ret = gx8010_core_get_global_track(stream, &ucontrol->value.enumerated.item[0]);
	} else {
		enum aout_subdevice subdev = kcontrol->private_value;

		ret = gx8010_core_get_track(subdev, &ucontrol->value.enumerated.item[0]);
	}

	return ret;
}

static int gx8010_control_track_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	int ret = -1;

	if (kcontrol->id.iface == SNDRV_CTL_ELEM_IFACE_CARD) {
		struct snd_card *card = kcontrol->private_data;
		struct aout_stream *stream = card->private_data;

		ret = gx8010_core_set_global_track(stream, ucontrol->value.enumerated.item[0]);
	} else {
		enum aout_subdevice subdev = kcontrol->private_value;

		ret = gx8010_core_set_track(subdev, ucontrol->value.enumerated.item[0]);
	}

	return ret;
}

static int gx8010_control_dac_mute_info(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_info *uinfo)
{
	uinfo->type  = SNDRV_CTL_ELEM_TYPE_BOOLEAN;
	uinfo->count = 1;
	uinfo->value.integer.min  = 0;
	uinfo->value.integer.max  = 1;
	uinfo->value.integer.step = 1;

	return 0;
}

static int gx8010_control_dac_mute_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	int ret = -1;
	struct snd_card *card = kcontrol->private_data;
	struct aout_stream *stream = card->private_data;

	ret = gx8010_core_get_global_dac_mute(stream, &ucontrol->value.integer.value[0]);

	return ret;
}

static int gx8010_control_dac_mute_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	int ret = -1;
	struct snd_card *card = kcontrol->private_data;
	struct aout_stream *stream = card->private_data;

	ret = gx8010_core_set_global_dac_mute(stream, ucontrol->value.integer.value[0]);

	return ret;
}

static int gx8010_control_dac_volume_info(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_info *uinfo)
{
	uinfo->type  = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 1;
	uinfo->value.integer.step = 1;
	gx8010_core_dac_volume_range(&uinfo->value.integer.min, &uinfo->value.integer.max);

	return 0;
}

static int gx8010_control_dac_volume_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	int ret = -1;
	struct snd_card *card = kcontrol->private_data;
	struct aout_stream *stream = card->private_data;

	ret = gx8010_core_get_global_dac_volume(stream, &ucontrol->value.integer.value[0]);

	return ret;
}

static int gx8010_control_dac_volume_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	int ret = -1;
	struct snd_card *card = kcontrol->private_data;
	struct aout_stream *stream = card->private_data;

	ret = gx8010_core_set_global_dac_volume(stream, ucontrol->value.integer.value[0]);

	return ret;
}

#define GX8010_CONTROL_VOLUME(xiface, xname, xindex, xvalue) { \
	.iface  = xiface, \
	.access = SNDRV_CTL_ELEM_ACCESS_READWRITE, \
	.name   = xname, \
	.index  = xindex, \
	.info = gx8010_control_volume_info, \
	.get  = gx8010_control_volume_get, \
	.put  = gx8010_control_volume_put, \
	.private_value = xvalue, \
}

#define GX8010_CONTROL_MUTE(xiface, xname, xindex, xvalue) { \
	.iface  = xiface, \
	.access = SNDRV_CTL_ELEM_ACCESS_READWRITE, \
	.name   = xname,   \
	.index  = xindex, \
	.info = gx8010_control_mute_info, \
	.get  = gx8010_control_mute_get, \
	.put  = gx8010_control_mute_put, \
	.private_value = xvalue, \
}

#define GX8010_CONTROL_TRACK(xiface, xname, xindex, xvalue) { \
	.iface  = xiface, \
	.access = SNDRV_CTL_ELEM_ACCESS_READWRITE, \
	.name   = xname,   \
	.index  = xindex, \
	.info = gx8010_control_track_info, \
	.get  = gx8010_control_track_get, \
	.put  = gx8010_control_track_put, \
	.private_value = xvalue, \
}

#define GX8010_CONTROL_DAC_MUTE(xiface, xname, xindex, xvalue) { \
	.iface  = xiface, \
	.access = SNDRV_CTL_ELEM_ACCESS_READWRITE, \
	.name   = xname,   \
	.index  = xindex, \
	.info = gx8010_control_dac_mute_info, \
	.get  = gx8010_control_dac_mute_get, \
	.put  = gx8010_control_dac_mute_put, \
	.private_value = xvalue, \
}

#define GX8010_CONTROL_DAC_VOLUME(xiface, xname, xindex, xvalue) { \
	.iface  = xiface, \
	.access = SNDRV_CTL_ELEM_ACCESS_READWRITE, \
	.name   = xname,   \
	.index  = xindex, \
	.info = gx8010_control_dac_volume_info, \
	.get  = gx8010_control_dac_volume_get, \
	.put  = gx8010_control_dac_volume_put, \
	.private_value = xvalue, \
}

static struct snd_kcontrol_new gx8010_controls[] = {
	GX8010_CONTROL_VOLUME    (SNDRV_CTL_ELEM_IFACE_MIXER,   "PCM0P Playback Volume (DB)",     0, AOUT_PLAYBACK0),
	GX8010_CONTROL_MUTE      (SNDRV_CTL_ELEM_IFACE_MIXER,   "PCM0P Playback Mute (on/off)",   0, AOUT_PLAYBACK0),
	GX8010_CONTROL_TRACK     (SNDRV_CTL_ELEM_IFACE_MIXER,   "PCM0P Playback Track",           0, AOUT_PLAYBACK0),
	GX8010_CONTROL_VOLUME    (SNDRV_CTL_ELEM_IFACE_MIXER,   "PCM1P Playback Volume (DB)",     0, AOUT_PLAYBACK1),
	GX8010_CONTROL_MUTE      (SNDRV_CTL_ELEM_IFACE_MIXER,   "PCM1P Playback Mute (on/off)",   0, AOUT_PLAYBACK1),
	GX8010_CONTROL_TRACK     (SNDRV_CTL_ELEM_IFACE_MIXER,   "PCM1P Playback Track (s/l/r)",   0, AOUT_PLAYBACK1),
	GX8010_CONTROL_VOLUME    (SNDRV_CTL_ELEM_IFACE_MIXER,   "PCM2P Playback Volume (DB)",     0, AOUT_PLAYBACK2),
	GX8010_CONTROL_MUTE      (SNDRV_CTL_ELEM_IFACE_MIXER,   "PCM2P Playback Mute (on/off)",   0, AOUT_PLAYBACK2),
	GX8010_CONTROL_TRACK     (SNDRV_CTL_ELEM_IFACE_MIXER,   "PCM2P Playback Track (s/l/r)",   0, AOUT_PLAYBACK2),
	GX8010_CONTROL_VOLUME    (SNDRV_CTL_ELEM_IFACE_CARD,  "Global Playback Volume (DB)",      0, -1),
	GX8010_CONTROL_MUTE      (SNDRV_CTL_ELEM_IFACE_CARD,  "Global Playback Mute (on/off)",    0, -1),
	GX8010_CONTROL_TRACK     (SNDRV_CTL_ELEM_IFACE_CARD,  "Global Playback Track",            0, -1),
	GX8010_CONTROL_DAC_MUTE  (SNDRV_CTL_ELEM_IFACE_CARD,  "Global Playback Dac Mute(on/off)", 0, -1),
	GX8010_CONTROL_DAC_VOLUME(SNDRV_CTL_ELEM_IFACE_CARD,  "Global Playback Dac Volume(DB)",   0, -1)
};

int gx8010_control_new(struct snd_card *card)
{
	unsigned int idx;

	strcpy(card->mixername, "GX8010 Alsa Mixer");

	for (idx = 0; idx < ARRAY_SIZE(gx8010_controls); idx++) {
		int err = snd_ctl_add(card, snd_ctl_new1(&gx8010_controls[idx], (void*)card));
		if (err < 0) {
			return err;
		}
	}

	return 0;
}
EXPORT_SYMBOL(gx8010_control_new);

MODULE_AUTHOR("linxsh");
MODULE_DESCRIPTION("audio driver for the gx8010 chip");
MODULE_LICENSE("GPL");
