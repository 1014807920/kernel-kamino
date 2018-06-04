#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/initval.h>
#include <sound/tlv.h>
#include <sound/jack.h>
#include <asm/div64.h>
#include <linux/gpio.h>

#include "nau8822.h"

//48K 32BIT MCLK 12.288M
static const struct reg_default nau8822_reg_init[] = {
	{ NAU8822_REG_POWER_MANAGEMENT_1, 0x0000 },
	{ NAU8822_REG_POWER_MANAGEMENT_2, 0x0000 },
	{ NAU8822_REG_POWER_MANAGEMENT_3, 0x0000 },
	{ NAU8822_REG_AUDIO_INTERFACE, 0x0070 },
	{ NAU8822_REG_COMPANDING_CONTROL, 0x0000 },
	{ NAU8822_REG_CLOCKING, 0x000C },
	{ NAU8822_REG_ADDITIONAL_CONTROL, 0x0000 },
	{ NAU8822_REG_GPIO_CONTROL, 0x0000 },
	{ NAU8822_REG_JACK_DETECT_CONTROL_1, 0x0000 },
	{ NAU8822_REG_DAC_CONTROL, 0x0048 },
	{ NAU8822_REG_LEFT_DAC_DIGITAL_VOLUME, 0x01ff },
	{ NAU8822_REG_RIGHT_DAC_DIGITAL_VOLUME, 0x01ff },
	{ NAU8822_REG_JACK_DETECT_CONTROL_2, 0x0000 },
	{ NAU8822_REG_ADC_CONTROL, 0x0100 },
	{ NAU8822_REG_LEFT_ADC_DIGITAL_VOLUME, 0x01ff },
	{ NAU8822_REG_RIGHT_ADC_DIGITAL_VOLUME, 0x01ff },
	{ 17, 0x0000 },
	{ NAU8822_REG_EQ1, 0x012c },
	{ NAU8822_REG_EQ2, 0x002c },
	{ NAU8822_REG_EQ3, 0x002c },
	{ NAU8822_REG_EQ4, 0x002c },
	{ NAU8822_REG_EQ5, 0x002c },
	{ 23, 0x0000 },
	{ NAU8822_REG_DAC_LIMITER_1, 0x0032 },
	{ NAU8822_REG_DAC_LIMITER_2, 0x0000 },
	{ 26, 0x0000 },
	{ NAU8822_REG_NOTCH_FILTER_1, 0x0000 },
	{ NAU8822_REG_NOTCH_FILTER_2, 0x0000 },
	{ NAU8822_REG_NOTCH_FILTER_3, 0x0000 },
	{ NAU8822_REG_NOTCH_FILTER_4, 0x0000 },
	{ 31, 0x0000 },
	{ NAU8822_REG_ALC_CONTROL_1, 0x0038 },
	{ NAU8822_REG_ALC_CONTROL_2, 0x000b },
	{ NAU8822_REG_ALC_CONTROL_3, 0x0032 },
	{ NAU8822_REG_NOISE_GATE, 0x0010 },
	{ NAU8822_REG_PLL_N, 0x0008 },
	{ NAU8822_REG_PLL_K1, 0x000c },
	{ NAU8822_REG_PLL_K2, 0x0093 },
	{ NAU8822_REG_PLL_K3, 0x00e9 },
	{ 40, 0x0000 },
	{ NAU8822_REG_3D_CONTROL, 0x0000 },
	{ 42, 0x0000 },
	{ NAU8822_REG_R_SPEAKER_CONTROL, 0x0000 },
	{ NAU8822_REG_INPUT_CONTROL, 0x0000 },
	{ NAU8822_REG_LEFT_INP_PGA_CONTROL, 0x0110 },
	{ NAU8822_REG_RIGHT_INP_PGA_CONTROL, 0x0110 },
	{ NAU8822_REG_LEFT_ADC_BOOST_CONTROL, 0x0100 },
	{ NAU8822_REG_RIGHT_ADC_BOOST_CONTROL, 0x0100 },
	{ NAU8822_REG_OUTPUT_CONTROL, 0x0002 },
	{ NAU8822_REG_LEFT_MIXER_CONTROL, 0x0001 },
	{ NAU8822_REG_RIGHT_MIXER_CONTROL, 0x0001 },
	{ NAU8822_REG_LHP_VOLUME, 0x0139 },
	{ NAU8822_REG_RHP_VOLUME, 0x0139 },
	{ NAU8822_REG_LSPKOUT_VOLUME, 0x0139 },
	{ NAU8822_REG_RSPKOUT_VOLUME, 0x0139 },
	{ NAU8822_REG_AUX2_MIXER, 0x0000 },
	{ NAU8822_REG_AUX1_MIXER, 0x0000 },
	{ NAU8822_REG_POWER_MANAGEMENT_4, 0x0000 },
	{ NAU8822_REG_LEFT_TIME_SLOT, 0x0000 },
	{ NAU8822_REG_MISC, 0x0020 },
	{ NAU8822_REG_RIGHT_TIME_SLOT, 0x0000 },
	{ NAU8822_REG_DEVICE_REVISION, 0x007f },
	{ NAU8822_REG_DEVICE_ID, 0x001a },
	{ 64, 0x0000 },
	{ NAU8822_REG_DAC_DITHER, 0x0114 },
	{ 66, 0x0000 },
	{ 67, 0x0000 },
	{ 68, 0x0000 },
	{ 69, 0x0000 },
	{ NAU8822_REG_ALC_ENHANCE_1, 0x0000 },
	{ NAU8822_REG_ALC_ENHANCE_2, 0x0000 },
	{ NAU8822_REG_192KHZ_SAMPLING, 0x0008 },
	{ NAU8822_REG_MISC_CONTROL, 0x0000 },
	{ NAU8822_REG_INPUT_TIEOFF, 0x0000 },
	{ NAU8822_REG_POWER_REDUCTION, 0x0000 },
	{ NAU8822_REG_AGC_PEAK2PEAK, 0x0000 },
	{ NAU8822_REG_AGC_PEAK_DETECT, 0x0000 },
	{ NAU8822_REG_AUTOMUTE_CONTROL, 0x0000 },
	{ NAU8822_REG_OUTPUT_TIEOFF, 0x0000 },
};

static int nau8822_codec_reset(struct nau8822_priv *nau8822);
static int nau8822_codec_init(struct nau8822_priv *nau8822);

static int nau8822_reg_read(void *context, unsigned int reg,
			     unsigned int *value)
{
	struct i2c_client *client = context;
	struct i2c_msg xfer[2];
	uint8_t reg_buf;
	uint16_t val_buf;
	int ret;

	reg_buf = (uint8_t)(reg << 1);
	xfer[0].addr = client->addr;
	xfer[0].len = sizeof(reg_buf);
	xfer[0].buf = &reg_buf;
	xfer[0].flags = 0;

	xfer[1].addr = client->addr;
	xfer[1].len = sizeof(val_buf);
	xfer[1].buf = (uint8_t *)&val_buf;
	xfer[1].flags = I2C_M_RD;

	ret = i2c_transfer(client->adapter, xfer, ARRAY_SIZE(xfer));
	if (ret < 0)
		return ret;
	else if (ret != ARRAY_SIZE(xfer))
		return -EIO;

	*value = be16_to_cpu(val_buf);

	return 0;
}

static int nau8822_reg_write(void *context, unsigned int reg,
			      unsigned int value)
{
	struct i2c_client *client = context;
	uint8_t buf[2];
	__be16 *out = (void *)buf;
	int ret;

	*out = cpu_to_be16((reg << 9) | value);
	ret = i2c_master_send(client, buf, sizeof(buf));
	if (ret == sizeof(buf))
		return 0;
	else if (ret < 0)
		return ret;
	else
		return -EIO;
}

static int nau8822_reg_update_bits(void *context, unsigned int reg,
			      unsigned int mask, unsigned int value)
{
	unsigned int temp = 0;
	int ret = 0;

	ret = nau8822_reg_read(context, reg, &temp);
	if (ret != 0)
		return ret;
	temp &=(~mask);
	temp |=(value & mask);
	ret = nau8822_reg_write(context, reg, temp);

	return ret;
}

static int nau8822_mute(struct nau8822_priv *nau8822, int mute)
{
	if (!nau8822) {
		dev_err(nau8822->dev, "%s nau8822 is NULL \n", __func__);
		return -1;
	}

	if (mute) {
		nau8822_reg_update_bits(nau8822->i2c, NAU8822_REG_DAC_CONTROL,
			0x40, 0x40);
	} else {
		nau8822_reg_update_bits(nau8822->i2c, NAU8822_REG_DAC_CONTROL,
			0x40, 0);
	}

	return 0;
}

static int nau8822_set_bias_level(struct nau8822_priv *nau8822,
				 enum snd_soc_bias_level level)
{
	if (!nau8822)
		return -1;

	switch (level) {
	case SND_SOC_BIAS_ON:
	case SND_SOC_BIAS_PREPARE:
		if (nau8822->status != SND_SOC_BIAS_ON) {
			nau8822_reg_update_bits(nau8822->i2c, NAU8822_REG_POWER_MANAGEMENT_1,
				NAU8822_REFIMP_MASK, NAU8822_REFIMP_80K);
			nau8822_reg_write(nau8822->i2c, NAU8822_REG_POWER_MANAGEMENT_2, 0x0180);
			nau8822_reg_write(nau8822->i2c, NAU8822_REG_POWER_MANAGEMENT_3, 0x000F);
			nau8822->status = SND_SOC_BIAS_ON;
		}
		break;
	case SND_SOC_BIAS_STANDBY:
		if (nau8822->status != SND_SOC_BIAS_STANDBY) {
			nau8822_reg_write(nau8822->i2c, NAU8822_REG_POWER_MANAGEMENT_1, 0x010C);
			nau8822_reg_write(nau8822->i2c, NAU8822_REG_POWER_MANAGEMENT_2, 0);
			nau8822_reg_write(nau8822->i2c, NAU8822_REG_POWER_MANAGEMENT_3, 0);
			nau8822->status = SND_SOC_BIAS_STANDBY;
		}
		break;
	case SND_SOC_BIAS_OFF:
		if (nau8822->status != SND_SOC_BIAS_OFF) {
			nau8822_reg_write(nau8822->i2c, NAU8822_REG_POWER_MANAGEMENT_1, 0);
			nau8822_reg_write(nau8822->i2c, NAU8822_REG_POWER_MANAGEMENT_2, 0);
			nau8822_reg_write(nau8822->i2c, NAU8822_REG_POWER_MANAGEMENT_3, 0);
			nau8822->status = SND_SOC_BIAS_OFF;
		}
		break;
	}

	return 0;
}

static int nau8822_vol_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct snd_soc_codec *codec = component->codec;
	struct nau8822_priv *nau8822 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.enumerated.item[0] = nau8822->vol_index;

	return 0;
}

static int nau8822_set_vol(struct nau8822_priv *nau8822)
{
	const unsigned int nau8822_reg_val[] = {
		0x1FF, 0x1F7, 0x1EF, 0x1E7, 0x1DF, 0x1D7
	};

	nau8822_reg_write(nau8822->i2c, NAU8822_REG_LEFT_DAC_DIGITAL_VOLUME, nau8822_reg_val[nau8822->vol_index]);
	nau8822_reg_write(nau8822->i2c, NAU8822_REG_RIGHT_DAC_DIGITAL_VOLUME, nau8822_reg_val[nau8822->vol_index]);

	return 0;
}

static int nau8822_vol_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct snd_soc_codec *codec = component->codec;
	struct nau8822_priv *nau8822 = snd_soc_codec_get_drvdata(codec);

	if (nau8822 == NULL) {
		dev_err(nau8822->dev, "%s nau8822 is NULL\n",__func__);
		return -1;
	}

	nau8822->vol_index = ucontrol->value.enumerated.item[0];
	nau8822_set_vol(nau8822);

	return 0;
}

static int nau8822_mute_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct snd_soc_codec *codec = component->codec;
	struct nau8822_priv *nau8822 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.enumerated.item[0] = nau8822->mute_index;

	return 0;
}

static int nau8822_mute_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct snd_soc_codec *codec = component->codec;
	struct nau8822_priv *nau8822 = snd_soc_codec_get_drvdata(codec);

	if (nau8822 == NULL) {
		dev_err(nau8822->dev, "%s nau8822 is NULL\n",__func__);
		return -1;
	}

	nau8822->mute_index = ucontrol->value.enumerated.item[0];
	nau8822_mute(nau8822, nau8822->mute_index);

	return 0;
}

static int nau8822_playback_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct snd_soc_codec *codec = component->codec;
	struct nau8822_priv *nau8822 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.enumerated.item[0] = nau8822->playback_index;

	return 0;
}

static int nau8822_playback_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct snd_soc_codec *codec = component->codec;
	struct nau8822_priv *nau8822 = snd_soc_codec_get_drvdata(codec);

	if (nau8822 == NULL) {
		dev_err(nau8822->dev, "%s nau8822 is NULL\n",__func__);
		return -1;
	}

	if (nau8822->playback_index == ucontrol->value.enumerated.item[0])
		return 0;

	nau8822->playback_index = ucontrol->value.enumerated.item[0];
	if (nau8822->playback_index == 0x1) {
		nau8822_codec_init(nau8822);
	} else {
		nau8822_codec_reset(nau8822);
	}

	return 0;
}

static int nau8822_hp_det_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct snd_soc_codec *codec = component->codec;
	struct nau8822_priv *nau8822 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.enumerated.item[0] = nau8822->gpio_val;

	return 0;
}

#define NAU8822_VOL(xname, xenum) \
	SOC_ENUM_EXT(xname, xenum, nau8822_vol_get, nau8822_vol_put)
#define NAU8822_MUTE(xname, xenum) \
	SOC_ENUM_EXT(xname, xenum, nau8822_mute_get, nau8822_mute_put)
#define NAU8822_PLAYBACK(xname, xenum) \
	SOC_ENUM_EXT(xname, xenum, nau8822_playback_get, nau8822_playback_put)
#define NAU8822_HP_DET(xname, xenum) \
	SOC_ENUM_EXT(xname, xenum, nau8822_hp_det_get, NULL)

static const char *nau8822_vol_text[] = {
	"0.00", "-4.00", "-8.00", "-12.00", "-16.00", "-20.00"
};
static SOC_ENUM_SINGLE_EXT_DECL(nau8822_vol_sel, nau8822_vol_text);

static const char *nau8822_mute_text[] = {
	"off", "on",
};
static SOC_ENUM_SINGLE_EXT_DECL(nau8822_mute_sel, nau8822_mute_text);

static const char *nau8822_playback_text[] = {
	"off", "on",
};
static SOC_ENUM_SINGLE_EXT_DECL(nau8822_playback_sel, nau8822_playback_text);

static const char *nau8822_hp_det_text[] = {
	"out", "in",
};
static SOC_ENUM_SINGLE_EXT_DECL(nau8822_hp_det, nau8822_hp_det_text);

static const struct snd_kcontrol_new nau8822_snd_controls[] = {
	NAU8822_VOL("Nau8822 MaxxVolume Control (DB)", nau8822_vol_sel),
	NAU8822_MUTE("Nau8822 SoftMute Control (on/off)", nau8822_mute_sel),
	NAU8822_PLAYBACK("Nau8822 Playback Control (on/off)", nau8822_playback_sel),
	NAU8822_HP_DET("Nau8822 Headphone Detect (out/in)", nau8822_hp_det),
};

static int nau8822_codec_reset(struct nau8822_priv *nau8822)
{
	int ret = 0;

	nau8822_mute(nau8822, 1);
	ret = nau8822_reg_write(nau8822->i2c, NAU8822_REG_RESET, 0x00);
	if (ret != 0)
		dev_err(nau8822->dev, "Failed to issue reset: %d\n", ret);

	return ret;
}

static int nau8822_codec_init(struct nau8822_priv *nau8822)
{
	int i = 0;
	int ret = 0;

	ret = nau8822_codec_reset(nau8822);
	if (ret != 0)
		return ret;

	for (i = 0; i < ARRAY_SIZE(nau8822_reg_init); i++)
		ret = nau8822_reg_write(nau8822->i2c, nau8822_reg_init[i].reg, nau8822_reg_init[i].def);

	nau8822_set_bias_level(nau8822, SND_SOC_BIAS_STANDBY);
	msleep(10);
	nau8822_set_bias_level(nau8822, SND_SOC_BIAS_ON);
	msleep(10);
	nau8822_set_vol(nau8822);
	nau8822_mute(nau8822, nau8822->mute_index);

	return ret;
}

void nau8822_hp_detect(unsigned long data)
{
	int gpio_val = 0;
	struct nau8822_priv *nau8822 = (struct nau8822_priv *)data;

	gpio_val = gpiod_get_value(nau8822->gpio_hp_det);
	if (gpio_val != nau8822->gpio_val) {
		if (gpio_val)
			snd_soc_jack_report(&nau8822->jack, 0, 0xff);
		else
			snd_soc_jack_report(&nau8822->jack, 1, 0xff);

		nau8822->gpio_val = gpio_val;
	}
	mod_timer(&nau8822->timer, (jiffies + HP_DETECT_DELAY));
}

static int nau8822_probe(struct snd_soc_codec *codec)
{
	struct snd_soc_card *card = codec->component.card;
	struct nau8822_priv *nau8822 = snd_soc_codec_get_drvdata(codec);
	int ret = 0;

	if (nau8822->need_hp_det) {
		ret = snd_soc_card_jack_new(card, "Headset", SND_JACK_HEADPHONE,
					&nau8822->jack, NULL, 0);
		if (ret != 0) {
			dev_err(nau8822->dev, "Failed to new jack: %d\n", ret);
			return ret;
		}

		nau8822->gpio_hp_det = devm_gpiod_get(nau8822->dev, "hp_det", GPIOD_IN);
		if (IS_ERR(nau8822->gpio_hp_det)) {
			dev_err(nau8822->dev, "unable to get gpio_hp_det\n");
			nau8822->gpio_hp_det = NULL;
			return -1;
		}

		nau8822->gpio_val = gpiod_get_value(nau8822->gpio_hp_det);
		if (nau8822->gpio_val) {
			ret = nau8822_codec_init(nau8822);
			if (ret != 0)
			  dev_err(nau8822->dev, "Failed to init CODEC: %d\n", ret);

			snd_soc_jack_report(&nau8822->jack, 0x0, 0xff);
			nau8822->playback_index = 1;
		} else {
			ret = nau8822_codec_reset(nau8822);
			snd_soc_jack_report(&nau8822->jack, 0x1, 0xff);
			nau8822->playback_index = 0;
		}

		init_timer(&nau8822->timer);
		nau8822->timer.data = (unsigned long)nau8822;
		nau8822->timer.function = nau8822_hp_detect;
		nau8822->timer.expires = jiffies + HP_DETECT_DELAY;
		add_timer(&nau8822->timer);
	} else {
		ret = nau8822_codec_init(nau8822);
		if (ret != 0)
			dev_err(nau8822->dev, "Failed to init CODEC: %d\n", ret);

		nau8822->playback_index = 1;
	}

	return ret;
}

static int nau8822_remove(struct snd_soc_codec *codec)
{
	struct nau8822_priv *nau8822 = snd_soc_codec_get_drvdata(codec);

	del_timer(&nau8822->timer);

	return nau8822_codec_reset(nau8822);
}

static int nau8822_suspend(struct snd_soc_codec *codec)
{
	struct nau8822_priv *nau8822 = snd_soc_codec_get_drvdata(codec);

	if (nau8822->need_hp_det)
		devm_gpiod_put(nau8822->dev, nau8822->gpio_hp_det);

	return nau8822_codec_reset(nau8822);
}

static int nau8822_resume(struct snd_soc_codec *codec)
{
	struct nau8822_priv *nau8822 = snd_soc_codec_get_drvdata(codec);
	int ret = 0;

	if (nau8822->need_hp_det) {
		nau8822->gpio_hp_det = devm_gpiod_get(nau8822->dev, "hp_det", GPIOD_IN);
		if (IS_ERR(nau8822->gpio_hp_det)) {
			dev_err(nau8822->dev, "unable to get gpio_hp_det\n");
			nau8822->gpio_hp_det = NULL;
			return -1;
		}

		if (nau8822->gpio_val) {
			ret = nau8822_codec_init(nau8822);
			if (ret != 0)
				dev_err(nau8822->dev, "Failed to init CODEC: %d\n", ret);
		}
	} else {
		ret = nau8822_codec_init(nau8822);
		if (ret != 0)
			dev_err(nau8822->dev, "Failed to init CODEC: %d\n", ret);
	}

	return ret;
}

#define nau8822_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE | \
	SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE)

static struct snd_soc_dai_driver nau8822_dai = {
	.name = "nau8822-voice",
	.playback = {
		.stream_name = "Playback",
		.channels_min = 1,
		.channels_max = 2,
		.rates = SNDRV_PCM_RATE_8000_96000,
		.formats = nau8822_FORMATS,
	},
	.capture = {
		.stream_name = "Capture",
		.channels_min = 1,
		.channels_max = 2,
		.rates =  SNDRV_PCM_RATE_8000_96000,
		.formats = nau8822_FORMATS,
	},
	.symmetric_rates = 1,
};

static struct snd_soc_codec_driver soc_codec_dev_nau8822 = {
	.probe   = nau8822_probe,
	.remove  = nau8822_remove,
	.suspend = nau8822_suspend,
	.resume  = nau8822_resume,
	.component_driver = {
		.controls = nau8822_snd_controls,
		.num_controls = ARRAY_SIZE(nau8822_snd_controls),
	}
};

static int nau8822_dt_parse(struct i2c_client *client)
{
	struct device_node *np = client->dev.of_node;
	struct nau8822_priv *nau8822 = i2c_get_clientdata(client);
	int ret = 0;
	const char *dev_name = NULL;

	if (of_property_read_bool(np, "hp_det-gpio") == true)
		nau8822->need_hp_det = true;
	else
		nau8822->need_hp_det = false;

	ret = of_property_read_string_index(np, "dev-name", 0, &dev_name);
	if (ret == 0)
		dev_set_name(&client->dev, dev_name);

	return ret;
}

static int nau8822_i2c_probe(struct i2c_client *client,
			    const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct nau8822_priv *nau8822 = NULL;
	int ret = 0;

	nau8822 = devm_kzalloc(dev, sizeof(struct nau8822_priv),GFP_KERNEL);
	if(nau8822 == NULL)
		return -ENOMEM;

	i2c_set_clientdata(client, nau8822);

	nau8822->i2c = client;
	nau8822->dev = dev;
	nau8822->status = -1;
	nau8822->mute_index = 0;
	nau8822->vol_index = 0;
	nau8822->playback_index = 1;

	nau8822_dt_parse(client);
	ret = snd_soc_register_codec(&client->dev,
			&soc_codec_dev_nau8822, &nau8822_dai, 1);

	return ret;
}

static int nau8822_i2c_remove(struct i2c_client *client)
{
	struct nau8822_priv *nau8822 = i2c_get_clientdata(client);

	nau8822_set_bias_level(nau8822, SND_SOC_BIAS_OFF);
	if (nau8822->need_hp_det)
		devm_gpiod_put(nau8822->dev, nau8822->gpio_hp_det);
	snd_soc_unregister_codec(&client->dev);

	return 0;
}

static const struct i2c_device_id nau8822_i2c_id[] = {
	{ "nau8822", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, nau8822_i2c_id);

static struct i2c_driver nau8822_i2c_driver = {
	.driver = {
		.name = "nau8822",
	},
	.probe =    nau8822_i2c_probe,
	.remove =   nau8822_i2c_remove,
	.id_table = nau8822_i2c_id,
};

module_i2c_driver(nau8822_i2c_driver);

MODULE_DESCRIPTION("ASoC NAU8822 codec driver");
MODULE_LICENSE("GPL v2");
