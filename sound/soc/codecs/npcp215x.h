#ifndef __NPCP215X_H__
#define __NPCP215X_H__

/*
 * Register values.
 */

/* codec private data */
struct npcp215x_priv {
	struct i2c_client *i2c;
	struct gpio_desc  *gpio_dsp_rst;
	struct gpio_desc  *gpio_amp_pdn;
	struct gpio_desc  *gpio_hp_det;
	struct device     *dev;
	unsigned int      mute_index;
	unsigned int      vol_index;
	unsigned int      alg_index;
	unsigned int      playback_index;
	bool              is_init;
	bool              need_init;
	bool              need_hp_det;
};

#endif  /* __NPCP215X_H__ */
