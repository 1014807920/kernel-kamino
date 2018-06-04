#ifndef __GXASOC_PCM_CONTROL_H__
#define __GXASOC_PCM_CONTROL_H__

#define GXASOC_CONTROL_VOLUME(xiface, xname, xindex, xvalue) { \
	.iface  = xiface, \
	.access = SNDRV_CTL_ELEM_ACCESS_READWRITE, \
	.name   = xname, \
	.index  = xindex, \
	.info = gxasoc_control_volume_info, \
	.get  = gxasoc_control_volume_get, \
	.put  = gxasoc_control_volume_put, \
	.private_value = xvalue, \
}

#define GXASOC_CONTROL_MUTE(xiface, xname, xindex, xvalue) { \
	.iface  = xiface, \
	.access = SNDRV_CTL_ELEM_ACCESS_READWRITE, \
	.name   = xname,   \
	.index  = xindex, \
	.info = gxasoc_control_mute_info, \
	.get  = gxasoc_control_mute_get, \
	.put  = gxasoc_control_mute_put, \
	.private_value = xvalue, \
}

#define GXASOC_CONTROL_TRACK(xiface, xname, xindex, xvalue) { \
	.iface  = xiface, \
	.access = SNDRV_CTL_ELEM_ACCESS_READWRITE, \
	.name   = xname,   \
	.index  = xindex, \
	.info = gxasoc_control_track_info, \
	.get  = gxasoc_control_track_get, \
	.put  = gxasoc_control_track_put, \
	.private_value = xvalue, \
}

extern int gxasoc_control_track_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol);
extern int gxasoc_control_track_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol);
extern int gxasoc_control_track_info(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_info *uinfo);
extern int gxasoc_control_mute_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol);
extern int gxasoc_control_mute_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol);
extern int gxasoc_control_mute_info(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_info *uinfo);
extern int gxasoc_control_volume_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol);
extern int gxasoc_control_volume_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol);
extern int gxasoc_control_volume_info(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_info *uinfo);

#endif
