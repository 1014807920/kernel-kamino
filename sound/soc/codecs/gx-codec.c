#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <sound/core.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/initval.h>
#include <sound/tlv.h>
#include <linux/io.h>

#include "gx-codec.h"

#define LODAC_FORMAT (SNDRV_PCM_FMTBIT_S8 | SNDRV_PCM_FMTBIT_S16_LE | \
		SNDRV_PCM_FMTBIT_S18_3LE | SNDRV_PCM_FMTBIT_S20_3LE)

static struct lodac_reg_des regDes = {
	.num   = 1,
	.name  = {
		"gxasoc.lodac.regs",
	},
	.baseAddr = {
		0x0030a1a0,
	},
	.length = {
		sizeof(struct aout_lodac),
	},
};

static int gxasoc_set_global_lodac_volume(struct lodac_info *lodac, long int dacDBValue);

static int gxasoc_set_lodac(struct lodac_info *lodac)
{
	int value   = 0;
	int dbIndex = DAC_MAX_DB_INDEX - (DAC_MAX_DB_VALUE - DAC_INIT_DB_VALUE);

	REG_SET_BIT(&(lodac->lodacReg->LODAC_CTRL), 0);

	REG_SET_BIT(&(lodac->lodacReg->LODAC_CTRL), 1);
	REG_CLR_BIT(&(lodac->lodacReg->LODAC_CTRL), 1);

	REG_SET_BIT(&(lodac->lodacReg->LODAC_CTRL), 1);
	REG_CLR_BIT(&(lodac->lodacReg->LODAC_CTRL), 1);

	REG_SET_VAL(&(lodac->lodacReg->LODAC_CTRL), ((0x0<<12)|(0xa8<<4)|(0x1<<2)|(0x1<<0))); //fs 256

	REG_SET_BIT(&(lodac->lodacReg->LODAC_CTRL), 1);
	REG_CLR_BIT(&(lodac->lodacReg->LODAC_CTRL), 1);

	REG_SET_VAL(&(lodac->lodacReg->LODAC_CTRL), ((0x0<<12)|(0x1<<3)|(0x1<<0)));
	REG_SET_BIT(&(lodac->lodacReg->LODAC_CTRL), 1);
	REG_CLR_BIT(&(lodac->lodacReg->LODAC_CTRL), 1);

	value = REG_GET_VAL(&(lodac->lodacReg->LODAC_DATA)) & 0xfffffffe;

	REG_SET_VAL(&(lodac->lodacReg->LODAC_CTRL), ((0<<12)|((0x00|value)<<4)|(1<<2)|(1<<0)));
	REG_SET_BIT(&(lodac->lodacReg->LODAC_CTRL), 1);
	REG_CLR_BIT(&(lodac->lodacReg->LODAC_CTRL), 1);

	REG_SET_VAL(&(lodac->lodacReg->LODAC_CTRL), ((3<<12)|(dbIndex<<4)|(1<<2)|(1<<0)));
	REG_SET_BIT(&(lodac->lodacReg->LODAC_CTRL), 1);
	REG_CLR_BIT(&(lodac->lodacReg->LODAC_CTRL), 1);

	REG_SET_VAL(&(lodac->lodacReg->LODAC_CTRL), ((0<<12)|((0x01|value)<<4)|(1<<2)|(1<<0)));
	REG_SET_BIT(&(lodac->lodacReg->LODAC_CTRL), 1);
	REG_CLR_BIT(&(lodac->lodacReg->LODAC_CTRL), 1);

	REG_SET_VAL(&(lodac->lodacReg->LODAC_CTRL), ((3<<12)|(dbIndex<<4)|(1<<2)|(1<<0)));
	REG_SET_BIT(&(lodac->lodacReg->LODAC_CTRL), 1);
	REG_CLR_BIT(&(lodac->lodacReg->LODAC_CTRL), 1);

	REG_SET_VAL(&(lodac->lodacReg->LODAC_CTRL), ((2<<12)|(0x38<<4)|(1<<2)|(1<<0)));
	REG_SET_BIT(&(lodac->lodacReg->LODAC_CTRL), 1);
	REG_CLR_BIT(&(lodac->lodacReg->LODAC_CTRL), 1);

	return 0;
}

static struct snd_soc_dai_driver gxasoc_lodac_dai[] = {
	{
		.name     = "gxasoc-lodac-dai",
		.playback = {
			.stream_name = "Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_48000,
			.formats = LODAC_FORMAT,
		},
	},
};

static int gxasoc_control_lodac_mute_info(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_info *uinfo)
{
	uinfo->type  = SNDRV_CTL_ELEM_TYPE_BOOLEAN;
	uinfo->count = 1;
	uinfo->value.integer.min  = 0;
	uinfo->value.integer.max  = 1;
	uinfo->value.integer.step = 1;

	return 0;
}

static int gxasoc_control_lodac_mute_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct snd_soc_codec *codec = component->codec;
	struct lodac_info *lodac = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = lodac->globalDacMute;

	return 0;
}

static int gxasoc_set_global_lodac_mute(struct lodac_info *lodac, long int dacMute)
{
	if (dacMute) {
		REG_SET_VAL(&(lodac->lodacReg->LODAC_CTRL), ((0x3<<12)|(0xf9<<4)|(0x1<<2)|(0x1<<0)));
		REG_SET_BIT(&(lodac->lodacReg->LODAC_CTRL), 1);
		REG_CLR_BIT(&(lodac->lodacReg->LODAC_CTRL), 1);
		lodac->globalDacMute = 1;
	} else {
		REG_SET_VAL(&(lodac->lodacReg->LODAC_CTRL), ((0x3<<12)|(0x79<<4)|(0x1<<2)|(0x1<<0)));
		REG_SET_BIT(&(lodac->lodacReg->LODAC_CTRL), 1);
		REG_CLR_BIT(&(lodac->lodacReg->LODAC_CTRL), 1);
		gxasoc_set_global_lodac_volume(lodac, lodac->globalDacDBValue);
		lodac->globalDacMute = 0;
	}

	return 0;
}

static int gxasoc_control_lodac_mute_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct snd_soc_codec *codec = component->codec;
	struct lodac_info *lodac = snd_soc_codec_get_drvdata(codec);
	int ret = -1;

	ret = gxasoc_set_global_lodac_mute(lodac, ucontrol->value.integer.value[0]);

	return ret;
}

static int gxasoc_lodac_volume_range(long int *minDBValue, long int *maxDBValue)
{
	if (minDBValue)
		*minDBValue = DAC_MIN_DB_VALUE;

	if (maxDBValue)
		*maxDBValue = DAC_MAX_DB_VALUE;

	return 0;
}

static int gxasoc_control_lodac_volume_info(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_info *uinfo)
{
	uinfo->type  = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 1;
	uinfo->value.integer.step = 1;
	gxasoc_lodac_volume_range(&uinfo->value.integer.min, &uinfo->value.integer.max);

	return 0;
}

static int gxasoc_control_lodac_volume_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct snd_soc_codec *codec = component->codec;
	struct lodac_info *lodac = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = lodac->globalDacDBValue;

	return 0;
}

static int gxasoc_set_global_lodac_volume(struct lodac_info *lodac, long int dacDBValue)
{
	int dbIndex = DAC_MAX_DB_INDEX - (DAC_MAX_DB_VALUE - dacDBValue);
	int value   = REG_GET_VAL(&(lodac->lodacReg->LODAC_DATA));

	REG_SET_VAL(&(lodac->lodacReg->LODAC_CTRL), ((0x0<<12)|(0x1<<3)|(0x1<<0)));
	REG_SET_BIT(&(lodac->lodacReg->LODAC_CTRL), 1);
	REG_CLR_BIT(&(lodac->lodacReg->LODAC_CTRL), 1);

	value = REG_GET_VAL(&(lodac->lodacReg->LODAC_DATA)) & 0xfffffffe;

	REG_SET_VAL(&(lodac->lodacReg->LODAC_CTRL), ((0<<12)|((0x00|value)<<4)|(1<<2)|(1<<0)));
	REG_SET_BIT(&(lodac->lodacReg->LODAC_CTRL), 1);
	REG_CLR_BIT(&(lodac->lodacReg->LODAC_CTRL), 1);

	REG_SET_VAL(&(lodac->lodacReg->LODAC_CTRL), ((3<<12)|(dbIndex<<4)|(1<<2)|(1<<0)));
	REG_SET_BIT(&(lodac->lodacReg->LODAC_CTRL), 1);
	REG_CLR_BIT(&(lodac->lodacReg->LODAC_CTRL), 1);

	REG_SET_VAL(&(lodac->lodacReg->LODAC_CTRL), ((0<<12)|((0x01|value)<<4)|(1<<2)|(1<<0)));
	REG_SET_BIT(&(lodac->lodacReg->LODAC_CTRL), 1);
	REG_CLR_BIT(&(lodac->lodacReg->LODAC_CTRL), 1);

	REG_SET_VAL(&(lodac->lodacReg->LODAC_CTRL), ((3<<12)|(dbIndex<<4)|(1<<2)|(1<<0)));
	REG_SET_BIT(&(lodac->lodacReg->LODAC_CTRL), 1);
	REG_CLR_BIT(&(lodac->lodacReg->LODAC_CTRL), 1);

	lodac->globalDacDBValue = dacDBValue;

	return 0;
}

static int gxasoc_control_lodac_volume_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct snd_soc_codec *codec = component->codec;
	struct lodac_info *lodac = snd_soc_codec_get_drvdata(codec);
	int ret = -1;

	ret = gxasoc_set_global_lodac_volume(lodac, ucontrol->value.integer.value[0]);

	return ret;
}


#define GXASOC_CONTROL_DAC_MUTE(xiface, xname, xindex, xvalue) { \
	    .iface  = xiface, \
	    .access = SNDRV_CTL_ELEM_ACCESS_READWRITE, \
	    .name   = xname,   \
	    .index  = xindex, \
	    .info = gxasoc_control_lodac_mute_info, \
	    .get  = gxasoc_control_lodac_mute_get, \
	    .put  = gxasoc_control_lodac_mute_put, \
	    .private_value = xvalue, \
}

#define GXASOC_CONTROL_DAC_VOLUME(xiface, xname, xindex, xvalue) { \
	    .iface  = xiface, \
	    .access = SNDRV_CTL_ELEM_ACCESS_READWRITE, \
	    .name   = xname,   \
	    .index  = xindex, \
	    .info = gxasoc_control_lodac_volume_info, \
	    .get  = gxasoc_control_lodac_volume_get, \
	    .put  = gxasoc_control_lodac_volume_put, \
	    .private_value = xvalue, \
}

const struct snd_kcontrol_new gxasoc_lodac_controls[] = {
	GXASOC_CONTROL_DAC_MUTE  (SNDRV_CTL_ELEM_IFACE_CARD,  "Global Playback Dac Mute(on/off)", 0, -1),
	GXASOC_CONTROL_DAC_VOLUME(SNDRV_CTL_ELEM_IFACE_CARD,  "Global Playback Dac Volume(DB)",   0, -1),
};

static int gxasoc_lodac_init(struct snd_soc_codec *codec)
{
	struct lodac_info *lodac = snd_soc_codec_get_drvdata(codec);

	gxasoc_set_lodac(lodac);

	return 0;
}

static struct snd_soc_codec_driver soc_gxasoc_lodac_drv = {
	.probe        = gxasoc_lodac_init,
	.controls     = gxasoc_lodac_controls,
	.num_controls = ARRAY_SIZE(gxasoc_lodac_controls),
};

static struct lodac_info *gxasoc_lodac_dt_parse(struct platform_device *pdev)
{
	struct lodac_info *lodac = NULL;
	int i = 0, ret = 0;
	struct device_node *np = pdev->dev.of_node;
	const char *dev_name = NULL;

	lodac = devm_kzalloc(&pdev->dev, sizeof(struct lodac_info), GFP_KERNEL);
	if (lodac == NULL)
		return NULL;

	for (i = 0; i < regDes.num; i++) {
		if (regDes.name[i]) {
			struct resource *reg = platform_get_resource_byname(pdev, IORESOURCE_MEM, regDes.name[i]);
			if (reg) {
				regDes.baseAddr[i] = (unsigned int)reg->start;
				regDes.length[i]   = (unsigned int)(reg->end - reg->start);
			}
		}
	}

	lodac->lodacReg = (struct aout_lodac*)ioremap(regDes.baseAddr[0], regDes.length[0]);
	if (lodac->lodacReg == NULL) {
		kfree(lodac);
		return NULL;
	}

	ret = of_property_read_string_index(np, "dev-name", 0, &dev_name);
	if (ret == 0)
		dev_set_name(&pdev->dev, dev_name);

	return lodac;
}

static int gxasoc_lodac_probe(struct platform_device *pdev)
{
	struct lodac_info *lodac = NULL;

	lodac = gxasoc_lodac_dt_parse(pdev);
	if (lodac == NULL)
		return -ENOMEM;

	lodac->globalDacDBValue = DAC_INIT_DB_VALUE;
	lodac->globalDacMute    = 0;
	dev_set_drvdata(&pdev->dev, lodac);

	return snd_soc_register_codec(&pdev->dev, &soc_gxasoc_lodac_drv,
				gxasoc_lodac_dai, ARRAY_SIZE(gxasoc_lodac_dai));
}

static int gxasoc_lodac_remove(struct platform_device *pdev)
{
	struct lodac_info *lodac = dev_get_drvdata(&pdev->dev);

	snd_soc_unregister_codec(&pdev->dev);

	if (lodac->lodacReg) {
		iounmap(lodac->lodacReg);
		lodac->lodacReg = NULL;
	}
	return 0;
}

static const struct of_device_id gxasoc_lodac_of_match[] = {
    { .compatible = "NationalChip,ASoc-lodac"},
    {},
};

static struct platform_driver gxasoc_lodac_drv = {
	.probe = gxasoc_lodac_probe,
	.remove = gxasoc_lodac_remove,
	.driver = {
		.name = "gxasoc-lodac",
		.of_match_table = gxasoc_lodac_of_match,
	},
};

module_platform_driver(gxasoc_lodac_drv);
MODULE_LICENSE("GPL");
