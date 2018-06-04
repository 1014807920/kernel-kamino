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
#include <sound/soc.h>

#include "control.h"
#include "core.h"

int gxasoc_control_volume_info(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_info *uinfo)
{
	uinfo->type  = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 1;

	gxasoc_core_volume_range(&uinfo->value.integer.min, &uinfo->value.integer.max);
	uinfo->value.integer.step = 1;

	return 0;
}

int gxasoc_control_volume_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	int ret = -1;

	if (kcontrol->id.iface == SNDRV_CTL_ELEM_IFACE_CARD) {
		struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
		struct aout_stream *stream = dev_get_drvdata(component->dev);

		ret = gxasoc_core_get_global_volume(stream, &ucontrol->value.integer.value[0]);
	} else {
		enum aout_subdevice subdev = kcontrol->private_value;

		ret = gxasoc_core_get_volume(subdev, &ucontrol->value.integer.value[0]);
	}

	return ret;
}

int gxasoc_control_volume_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	int ret = -1;

	if (kcontrol->id.iface == SNDRV_CTL_ELEM_IFACE_CARD) {
		struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
		struct aout_stream *stream = dev_get_drvdata(component->dev);

		ret = gxasoc_core_set_global_volume(stream, ucontrol->value.integer.value[0]);
	} else {
		enum aout_subdevice subdev = kcontrol->private_value;

		ret = gxasoc_core_set_volume(subdev, ucontrol->value.integer.value[0]);
	}

	return ret;
}

int gxasoc_control_mute_info(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_info *uinfo)
{
	uinfo->type  = SNDRV_CTL_ELEM_TYPE_BOOLEAN;
	uinfo->count = 1;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 1;
	uinfo->value.integer.step = 1;

	return 0;
}

int gxasoc_control_mute_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	int ret = -1;

	if (kcontrol->id.iface == SNDRV_CTL_ELEM_IFACE_CARD) {
		struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
		struct aout_stream *stream = dev_get_drvdata(component->dev);

		ret = gxasoc_core_get_global_mute(stream, &ucontrol->value.integer.value[0]);
	} else {
		enum aout_subdevice subdev = kcontrol->private_value;

		ret = gxasoc_core_get_mute(subdev, &ucontrol->value.integer.value[0]);
	}

	return ret;
}

int gxasoc_control_mute_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	int ret = -1;

	if (kcontrol->id.iface == SNDRV_CTL_ELEM_IFACE_CARD) {
		struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
		struct aout_stream *stream = dev_get_drvdata(component->dev);

		ret = gxasoc_core_set_global_mute(stream, ucontrol->value.integer.value[0]);
	} else {
		enum aout_subdevice subdev = kcontrol->private_value;

		ret = gxasoc_core_set_mute(subdev, ucontrol->value.integer.value[0]);
	}

	return ret;
}

int gxasoc_control_track_info(struct snd_kcontrol *kcontrol,
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

int gxasoc_control_track_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	int ret = -1;

	if (kcontrol->id.iface == SNDRV_CTL_ELEM_IFACE_CARD) {
		struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
		struct aout_stream *stream = dev_get_drvdata(component->dev);

		ret = gxasoc_core_get_global_track(stream, &ucontrol->value.enumerated.item[0]);
	} else {
		enum aout_subdevice subdev = kcontrol->private_value;

		ret = gxasoc_core_get_track(subdev, &ucontrol->value.enumerated.item[0]);
	}

	return ret;
}

int gxasoc_control_track_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	int ret = -1;

	if (kcontrol->id.iface == SNDRV_CTL_ELEM_IFACE_CARD) {
		struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
		struct aout_stream *stream = dev_get_drvdata(component->dev);

		ret = gxasoc_core_set_global_track(stream, ucontrol->value.enumerated.item[0]);
	} else {
		enum aout_subdevice subdev = kcontrol->private_value;

		ret = gxasoc_core_set_track(subdev, ucontrol->value.enumerated.item[0]);
	}

	return ret;
}
