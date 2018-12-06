/*
 * alc1311.c  --  ALC1311 ALSA SoC audio codec driver
 *
 * Copyright 2018 Realtek Semiconductor Corp.
 * Author: Bard Liao <bardliao@realtek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/spi/spi.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <sound/tlv.h>
#include <asm/div64.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/pinctrl/consumer.h>
#include <linux/of_gpio.h>
#include <linux/io.h>
#include <linux/of_irq.h>

#include "alc1311.h"

#define ALC1311_I2C_ADDR	0x54

#define DEVICE_ID_REG 0x007D
#define DEVICE_ID_DATA 0x6433

struct alc1311_init_reg {
	u16 reg;
	u16 val;
	u16 delay;
};

struct alc1311_eq_reg {
	u8 level;
	u16 reg;
	u16 val;
};

static struct alc1311_init_reg init_list[] = {
	/////////////////initial

		{ 0x0000, 0xFFFF, 0 },  
		{ 0x0004, 0x1000, 0 }, 

		{ 0x0006, 0x3570, 0 },  //48kHz
		{ 0x0008, 0x3000, 0 },  //48kHz
		
		{ 0x000A, 0x105E, 0 },  //48kHz

		{ 0x000C, 0x0000, 0 },
		
		{ 0x0114, 0x0804, 0 },
		{ 0x0322, 0x400F, 0 },  
		{ 0x0326, 0x0001, 0 },  
		
		{ 0x0350, 0x0188, 0 },  
		{ 0x0324, 0x00A0, 0 },  
		
		{ 0x0350, 0x0008, 0 },   
		
		{ 0x006A, 0x0510, 0 },
		{ 0x006C, 0x2500, 0 },
		
		{ 0x0103, 0x033e, 0 },  
		{ 0x0104, 0x033e, 0 },
		
		{ 0x0106, 0x8787, 0 },  
		
		{ 0x0300, 0xB803, 0 },   
		
		{ 0x0646, 0x0506, 0 },
		{ 0x0616, 0x0506, 0 },
		{ 0x0626, 0x0506, 0 },
		{ 0x0636, 0x0506, 0 },
		
		{ 0x006A, 0x0641, 0 },
		{ 0x006C, 0xE155, 0 },
		{ 0x006A, 0x0611, 0 }, 
		{ 0x006C, 0xE105, 0 },
		{ 0x006A, 0x0621, 0 },
		{ 0x006C, 0xE105, 0 },
		{ 0x006A, 0x0631, 0 }, 
		{ 0x006C, 0xE105, 0 },
		
		{ 0x0600, 0x03C0, 0 },
		{ 0x0641, 0xDF5F, 0 },
		{ 0x0644, 0xDD00, 0 },
		
		{ 0x006A, 0x0002, 0 },
		{ 0x006C, 0x6505, 0 },  
		{ 0x006A, 0x0300, 0 },
		{ 0x006C, 0x0145, 0 },  
		
		{ 0x0324, 0xC0A0, 0 },  
		{ 0x0326, 0x000F, 0 },  
		
		{ 0x006A, 0x030C, 0 },
		{ 0x006C, 0x0FC0, 0 },  
		{ 0x006A, 0x000D, 0 },
		{ 0x006C, 0x0001, 0 },
		{ 0x006A, 0x010C, 0 },
		{ 0x006C, 0xC000, 0 },
		
		{ 0x0810, 0x0000, 0 },
		{ 0x0812, 0x0080, 0 },
		{ 0x081A, 0x0080, 0 },
		{ 0x0800, 0x800C, 0 },
		
		{ 0x0350, 0x0108, 0 },

};
#define ALC1311_INIT_REG_LEN ARRAY_SIZE(init_list)

static struct alc1311_init_reg clk_48k_reg[3] = {

		{ 0x0006, 0x3570, 0 },  //48kHz
		{ 0x0008, 0x3000, 0 },  //48kHz

		{ 0x000A, 0x105E, 0 }  //48kHz
};

static struct alc1311_init_reg clk_16k_reg[3] = {

		{ 0x0006, 0x6570, 0 },  //16kHz
		{ 0x0008, 0x3000, 0 },  //16kHz
		
		{ 0x000A, 0x085E, 0 }  //16kHz
};

static int alc1311_reg_init(struct snd_soc_codec *codec)
{
	int i;

	codec->cache_bypass = 1;
	for (i = 0; i < ALC1311_INIT_REG_LEN; i++) {
		snd_soc_write(codec, init_list[i].reg, init_list[i].val);
		mdelay(init_list[i].delay);
	}

	return 0;
}

static struct alc1311_eq_reg eq_list[] = {

//

};


static const struct reg_default alc1311_reg[] = {
	{ 0x0000,   0x0    },
	{ 0x0004,   0x1000 },
	{ 0x0006,   0x0    },
	{ 0x0008,   0x0    },
	{ 0x000A,   0x87e  },
	{ 0x000C,   0x20   },
	{ 0x0080,   0x1    },
	{ 0x0103,   0x33e  },
	{ 0x0104,   0x33e  },
	{ 0x0106,   0xafaf },
	{ 0x0214,   0x0    },
	{ 0x0300,   0xb000 },
	{ 0x0322,   0x0    },
	{ 0x0324,   0x0    },
	{ 0x0326,   0x0    },
	{ 0x0350,   0x88   },

};


static void alc1311_index_sync(struct snd_soc_codec *codec)
{
	const u16 *reg_cache = codec->reg_cache;
	int i;

	/* Sync back cached values if they're different from the
	 * hardware default.
	 */
	for (i = 1; i < ARRAY_SIZE(alc1311_reg); i++) {
		if (reg_cache[i] == alc1311_reg[i].reg)
			continue;
		snd_soc_write(codec, i, reg_cache[i]);
	}
}


/**
 * alc1311_index_write - Write private register.
 * @codec: SoC audio codec device.
 * @reg: Private register index.
 * @value: Private register Data.
 *
 * Modify private register for advanced setting. It can be written through
 * private index (0x6a) and data (0x6c) register.
 *
 * Returns 0 for success or negative error code.
 */
static int alc1311_index_write(struct snd_soc_codec *codec,
		unsigned int reg, unsigned int value)
{
	int ret;

	ret = snd_soc_write(codec, ALC1311_PRIV_INDEX, reg);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to set private addr: %d\n", ret);
		goto err;
	}
	ret = snd_soc_write(codec, ALC1311_PRIV_DATA, value);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to set private value: %d\n", ret);
		goto err;
	}
	return 0;

err:
	return ret;
}

/**
 * alc1311_index_read - Read private register.
 * @codec: SoC audio codec device.
 * @reg: Private register index.
 *
 * Read advanced setting from private register. It can be read through
 * private index (0x6a) and data (0x6c) register.
 *
 * Returns private register value or negative error code.
 */
static unsigned int alc1311_index_read(
	struct snd_soc_codec *codec, unsigned int reg)
{
	int ret;

	ret = snd_soc_write(codec, ALC1311_PRIV_INDEX, reg);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to set private addr: %d\n", ret);
		return ret;
	}
	return snd_soc_read(codec, ALC1311_PRIV_DATA);
}

/**
 * alc1311_index_update_bits - update private register bits
 * @codec: audio codec
 * @reg: Private register index.
 * @mask: register mask
 * @value: new value
 *
 * Writes new register value.
 *
 * Returns 1 for change, 0 for no change, or negative error code.
 */
static int alc1311_index_update_bits(struct snd_soc_codec *codec,
	unsigned int reg, unsigned int mask, unsigned int value)
{
	unsigned int old, new1;
	int change, ret;

	ret = alc1311_index_read(codec, reg);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to read private reg: %d\n", ret);
		goto err;
	}

	old = ret;
	new1 = (old & ~mask) | (value & mask);
	change = old != new1;
	if (change) {
		ret = alc1311_index_write(codec, reg, new1);
		if (ret < 0) {
			dev_err(codec->dev,
				"Failed to write private reg: %d\n", ret);
			goto err;
		}
	}
	return change;

err:
	return ret;
}

extern unsigned int serial_in_i2c(unsigned int addr, int offset);
extern unsigned int serial_out_i2c(unsigned int addr, int offset, int value);

static bool alc1311_volatile_register(
	struct device *dev, unsigned int reg)
{

	return 1;

}

static bool alc1311_readable_register(
	struct device *dev, unsigned int reg)
{
	switch (reg) {
	case 0x0000 ... 0x000D:
	case 0x0020 ... 0x0022:
	case 0x006a ... 0x0080:
	case 0x00f0 ... 0x00f4:
	case 0x0100 ... 0x0114:
	case 0x0214:
	case 0x0300 ... 0x0352:
	case 0x0500 ... 0x052a:
	case 0x0600 ... 0x0646:
	case 0x0700 ... 0x0745:
	case 0x0800 ... 0x08ff:
		return 1;
	default:
		return 0;
	}
}

static int alc1311_index_readable_register(
	struct snd_soc_codec *codec, unsigned int reg)
{
	switch (reg) {
	case 0x0002 ... 0x0022:
	case 0x0060 ... 0x0062:
	case 0x0070 ... 0x008f:
	case 0x0106 ... 0x010c:
	case 0x0200 ... 0x0229:
	case 0x0300 ... 0x0316:
	case 0x0350:
	case 0x0500 ... 0x051d:
	case 0x0528:
	case 0x0600 ... 0x064b:
	case 0x0700:
	case 0x0711:
	case 0x0721:
	case 0x0731:
	case 0x0741:
		return 1;
	default:
		return 0;
	}
}


#if 0 
/*Bard: keep it as a sample code*/
static const DECLARE_TLV_DB_SCALE(out_vol_tlv, -4650, 150, 0);
static const DECLARE_TLV_DB_SCALE(dac_vol_tlv, -65625, 375, 0);
static const DECLARE_TLV_DB_SCALE(in_vol_tlv, -3450, 150, 0);
static const DECLARE_TLV_DB_SCALE(adc_vol_tlv, -17625, 375, 0);
static const DECLARE_TLV_DB_SCALE(adc_bst_tlv, 0, 1200, 0);

/* {0, +20, +24, +30, +35, +40, +44, +50, +52} dB */
static unsigned int bst_tlv[] = {
	TLV_DB_RANGE_HEAD(7),
	0, 0, TLV_DB_SCALE_ITEM(0, 0, 0),
	1, 1, TLV_DB_SCALE_ITEM(2000, 0, 0),
	2, 2, TLV_DB_SCALE_ITEM(2400, 0, 0),
	3, 5, TLV_DB_SCALE_ITEM(3000, 500, 0),
	6, 6, TLV_DB_SCALE_ITEM(4400, 0, 0),
	7, 7, TLV_DB_SCALE_ITEM(5000, 0, 0),
	8, 8, TLV_DB_SCALE_ITEM(5200, 0, 0),
};


static const char *alc1311_input_mode[] = {
	"Single ended", "Differential"};

static const struct soc_enum alc1311_in1_mode_enum = SOC_ENUM_SINGLE(ALC1311_IN1_IN2,
	ALC1311_IN_SFT1, ARRAY_SIZE(alc1311_input_mode), alc1311_input_mode);
#endif


static const DECLARE_TLV_DB_SCALE(dac_vol_tlv, -6475, 37, 0);


static const char *alc1311_init_type_mode[] = {
	"nothing", "init"
};

static const SOC_ENUM_SINGLE_DECL(alc1311_init_type_enum, 0, 0, alc1311_init_type_mode);

static int alc1311_init_type_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = 0;

	return 0;
}

static int alc1311_init_type_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	int init = ucontrol->value.integer.value[0];

	if (init)
		alc1311_reg_init(codec);

	return 0;
}

static const struct snd_kcontrol_new alc1311_snd_controls[] = {

	//SOC_DOUBLE_TLV("DAC Playback Volume", 0x12,
	//		8, 0, 0xff, 0, dac_vol_tlv),
	SOC_DOUBLE_TLV("DAC Playback Volume", 0x0106,
			8, 0, 0xAf, 0, dac_vol_tlv),

	SOC_ENUM_EXT("AMP Init Control",  alc1311_init_type_enum,
		alc1311_init_type_get, alc1311_init_type_put),

};

static const struct snd_soc_dapm_widget alc1311_dapm_widgets[] = {
	/* Audio Interface */
	SND_SOC_DAPM_AIF_IN("AIF1RX", "AIF1 Playback", 0, SND_SOC_NOPM, 0, 0),

	/* DACs */
	SND_SOC_DAPM_DAC("DAC", NULL, SND_SOC_NOPM, 0, 0),

	/* Output Lines */
	SND_SOC_DAPM_OUTPUT("Amp"),
};

static const struct snd_soc_dapm_route alc1311_dapm_routes[] = {
	{"DAC", NULL, "AIF1RX"},
	{"Amp", NULL, "DAC"},
};

static int alc1311_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params, struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct alc1311_priv *alc1311 = snd_soc_codec_get_drvdata(codec);
	int i;

	printk("enter %s\n",__func__);

	//if (params_format(params) == SNDRV_PCM_FORMAT_S16_LE) {
	//	snd_soc_write(codec, 0,0);
	//}

	switch (params_rate(params)) {
		case 16000:
			printk("%s, set clk=16000 \n",__func__);
			for(i=0; i<3; i++){
				snd_soc_write(codec, clk_16k_reg[i].reg, clk_16k_reg[i].val);
			}
		break;
		case 48000:
			printk("%s, set clk=48000 \n",__func__);
			for(i=0; i<3; i++){
				snd_soc_write(codec, clk_48k_reg[i].reg, clk_48k_reg[i].val);
			}
		break;
		default:
		break;
	}

	return 0;
}

static int alc1311_prepare(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	//struct snd_soc_codec *codec = dai->codec;
	//struct alc1311_priv *alc1311 = snd_soc_codec_get_drvdata(codec);

	printk("enter %s\n",__func__);
	return 0;
}

static int alc1311_set_dai_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	//struct snd_soc_codec *codec = dai->codec;
	//struct alc1311_priv *alc1311 = snd_soc_codec_get_drvdata(codec);

	printk("enter %s\n",__func__);

	return 0;
}

static int alc1311_set_dai_sysclk(struct snd_soc_dai *dai,
		int clk_id, unsigned int freq, int dir)
{
	//struct snd_soc_codec *codec = dai->codec;
	//struct alc1311_priv *alc1311 = snd_soc_codec_get_drvdata(codec);

	printk("enter %s\n",__func__);


	return 0;
}

/**
 * alc1311_index_show - Dump private registers.
 * @dev: codec device.
 * @attr: device attribute.
 * @buf: buffer for display.
 *
 * To show non-zero values of all private registers.
 *
 * Returns buffer length.
 */
static ssize_t alc1311_index_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct alc1311_priv *alc1311 = i2c_get_clientdata(client);
	struct snd_soc_codec *codec = alc1311->codec;
	unsigned int val;
	int cnt = 0, i;

	codec->cache_bypass = 1;
	cnt += sprintf(buf, "ALC1311 index register\n");
	for (i = 0; i < 0x741; i++) {
		if (cnt + ALC1311_REG_DISP_LEN >= PAGE_SIZE)
			break;
		if (alc1311_index_readable_register(codec, i)) {
			val = alc1311_index_read(codec, i);
			if (!val)
				continue;
			cnt += snprintf(buf + cnt, ALC1311_REG_DISP_LEN,
					"%02x: %04x\n", i, val);
		}
	}

	if (cnt >= PAGE_SIZE)
		cnt = PAGE_SIZE - 1;
	//codec->cache_bypass = 0;

	return cnt;
}

static ssize_t alc1311_codec_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct alc1311_priv *alc1311 = i2c_get_clientdata(client);
	struct snd_soc_codec *codec = alc1311->codec;
	unsigned int val;
	int cnt = 0, i;

	codec->cache_bypass = 1;
	cnt += sprintf(buf, "ALC1311 codec register\n");
	for (i = 0; i <= 0x8ff; i++) {
		if (cnt + 22 >= PAGE_SIZE)
			break;
		if (alc1311_readable_register(dev, i)) {
			val = snd_soc_read(codec, i);

			cnt += snprintf(buf + cnt, 22,
					"%04x: %04x\n", i, val);
		}
	}

	if (cnt >= PAGE_SIZE)
		cnt = PAGE_SIZE - 1;
	//codec->cache_bypass = 0;

	return cnt;
}

static ssize_t alc1311_codec_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct alc1311_priv *alc1311 = i2c_get_clientdata(client);
	struct snd_soc_codec *codec = alc1311->codec;
	unsigned int val = 0, addr = 0;
	int i;

//	pr_debug("register \"%s\" count=%d\n", buf, count);
	for (i = 0; i < count; i++) {	/*address */
		if (*(buf + i) <= '9' && *(buf + i) >= '0')
			addr = (addr << 4) | (*(buf + i) - '0');
		else if (*(buf + i) <= 'f' && *(buf + i) >= 'a')
			addr = (addr << 4) | ((*(buf + i) - 'a') + 0xa);
		else if (*(buf + i) <= 'F' && *(buf + i) >= 'A')
			addr = (addr << 4) | ((*(buf + i) - 'A') + 0xa);
		else
			break;
	}

	for (i = i + 1; i < count; i++) {
		if (*(buf + i) <= '9' && *(buf + i) >= '0')
			val = (val << 4) | (*(buf + i) - '0');
		else if (*(buf + i) <= 'f' && *(buf + i) >= 'a')
			val = (val << 4) | ((*(buf + i) - 'a') + 0xa);
		else if (*(buf + i) <= 'F' && *(buf + i) >= 'A')
			val = (val << 4) | ((*(buf + i) - 'A') + 0xa);
		else
			break;
	}
	printk("addr=0x%x val=0x%x, i=%d, count=%d\n", addr, val, i, count);

	if (i == count) {
		pr_debug("0x%02x = 0x%04x\n", addr,
		snd_soc_read(codec, addr));
	} else {
		snd_soc_write(codec, addr, val);
	}

	return count;
}

static ssize_t alc1311_clk_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct alc1311_priv *alc1311 = i2c_get_clientdata(client);
	struct snd_soc_codec *codec = alc1311->codec;
	unsigned int val = 0, addr = 0;
	int i;

	printk("enter %s, buf=%s \n",__func__, buf);
	if(strncmp(buf,"16000",5) == 0){
		printk("%s, set clk=16000 \n",__func__);
		for(i=0; i<3; i++){
			snd_soc_write(codec, clk_16k_reg[i].reg, clk_16k_reg[i].val);
		}
	}else if(strncmp(buf,"48000",5) == 0){
		printk("%s, set clk=48000 \n",__func__);
		for(i=0; i<3; i++){
			snd_soc_write(codec, clk_48k_reg[i].reg, clk_48k_reg[i].val);
		}
	}else{
		printk("%s, set clk fail buf=%s\n",__func__, buf);
	}

	return count;
}

static DEVICE_ATTR(codec_reg, S_IWUSR | S_IRUGO, alc1311_codec_show, alc1311_codec_store);
static DEVICE_ATTR(index_reg, S_IRUGO, alc1311_index_show, NULL);
static DEVICE_ATTR(clk_reg, S_IWUSR, NULL, alc1311_clk_store);

static struct attribute *alc1311_attributes[] = {
    &dev_attr_codec_reg.attr,
    &dev_attr_index_reg.attr,
    &dev_attr_clk_reg.attr,
    NULL
};

static struct attribute_group alc1311_attribute_group = {
    .attrs = alc1311_attributes
};


static int alc1311_set_bias_level(struct snd_soc_codec *codec,
			enum snd_soc_bias_level level)
{
	//static int init_once = 0;

	printk("enter %s, level=%d \n",__func__, level);
	switch (level) {
	case SND_SOC_BIAS_ON:
		snd_soc_write(codec, 0x0350, 0x0108);//ysh
		break;

	case SND_SOC_BIAS_PREPARE:

		break;

	case SND_SOC_BIAS_STANDBY:
//		if (SND_SOC_BIAS_OFF == snd_soc_codec_get_bias_level(codec)) {
//
//		}
		break;

	case SND_SOC_BIAS_OFF:
		snd_soc_write(codec, 0x0350, 0x0088);
		break;

	default:
		break;
	}

	return 0;
}

static int alc1311_init(struct snd_soc_codec *codec)
{
	//struct alc1311_priv *alc1311 = snd_soc_codec_get_drvdata(codec);
	//int ret;
	printk("enter %s\n",__func__);

	alc1311_reg_init(codec);

	alc1311_set_bias_level(codec, SND_SOC_BIAS_STANDBY);

	return 0;
}


static int alc1311_probe(struct snd_soc_codec *codec)
{
	struct alc1311_priv *alc1311 = snd_soc_codec_get_drvdata(codec);
	int ret = 0;

	printk("enter %s\n",__func__);

/*
	codec->dapm.idle_bias_off = 1;
	ret = snd_soc_codec_set_cache_io(codec, 16, 16, SND_SOC_I2C);
	if (ret != 0) {
		dev_err(codec->dev, "Failed to set cache I/O: %d\n", ret);
		return ret;
	}
*/
	codec->cache_bypass = 1;		// no cache
	alc1311->codec = codec;

	alc1311_init(codec);

	return 0;

}

static int alc1311_remove(struct snd_soc_codec *codec)
{
	if (codec->control_data)
		alc1311_set_bias_level(codec, SND_SOC_BIAS_OFF);
	return 0;
}

#ifdef CONFIG_PM
static int alc1311_suspend(struct snd_soc_codec *codec)
{
	struct alc1311_priv *alc1311 = snd_soc_codec_get_drvdata(codec);

	if(alc1311->pa_en_desc){
		gpiod_set_value(alc1311->pa_en_desc, 0);
		printk("####%s... pa_en=%d\n",__func__,gpiod_get_value(alc1311->pa_en_desc));
	}
	alc1311_set_bias_level(codec, SND_SOC_BIAS_OFF);
	return 0;
}

static int alc1311_resume(struct snd_soc_codec *codec)
{
	struct alc1311_priv *alc1311 = snd_soc_codec_get_drvdata(codec);

	codec->cache_bypass = 1;
	snd_soc_cache_sync(codec);
	//alc1311_index_sync(codec);
	alc1311_reg_init(codec);
	if(alc1311->pa_en_desc){
		gpiod_set_value(alc1311->pa_en_desc, 1);
		printk("####%s... pa_en=%d\n",__func__,gpiod_get_value(alc1311->pa_en_desc));
	}
	return 0;
}
#else
#define alc1311_suspend NULL
#define alc1311_resume NULL
#endif

#define ALC1311_STEREO_RATES SNDRV_PCM_RATE_8000_96000
#define ALC1311_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE | \
			SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S8 | SNDRV_PCM_FMTBIT_S16_BE)

static int alc1311_dai_digital_mute(struct snd_soc_dai *codec_dai, int mute)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	struct alc1311_priv *alc1311 = snd_soc_codec_get_drvdata(codec);

	dev_info(codec->dev, "%s : %s\n", __func__, mute ? "MUTE" : "UNMUTE");

	if (!mute) {
		//snd_soc_write(codec, STATE_CTL_3, 0x00);	//--unmute amp
	} else {
		//snd_soc_write(codec, STATE_CTL_3, 0x7f);	//--mute amp
	}

	return 0;
}

static int alc1311_startup(struct snd_pcm_substream *substream,
                            struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct alc1311_priv *alc1311 = snd_soc_codec_get_drvdata(codec);

	if(alc1311->pa_en_desc){
		gpiod_set_value(alc1311->pa_en_desc, 1);
		printk("####%s... pa_en=%d\n",__func__,gpiod_get_value(alc1311->pa_en_desc));
	}
	return 0;
}

static int alc1311_shutdown(struct snd_pcm_substream *substream,
                             struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct alc1311_priv *alc1311 = snd_soc_codec_get_drvdata(codec);

	if(alc1311->pa_en_desc){
		gpiod_set_value(alc1311->pa_en_desc, 0);
		printk("####%s... pa_en=%d\n",__func__,gpiod_get_value(alc1311->pa_en_desc));
	}
	return 0;
}

struct snd_soc_dai_ops alc1311_aif_dai_ops = {
	.hw_params = alc1311_hw_params,
	.prepare = alc1311_prepare,
	.set_fmt = alc1311_set_dai_fmt,
	.digital_mute = alc1311_dai_digital_mute,
	.set_sysclk = alc1311_set_dai_sysclk,
	.startup = alc1311_startup,
	.shutdown = alc1311_shutdown,
};

struct snd_soc_dai_driver alc1311_dai = {

	.name = "alc1311-aif1",
	.id = 0,
	.playback = {
		.stream_name = "AIF1 Playback",
		.channels_min = 1,
		.channels_max = 2,
		.rates = ALC1311_STEREO_RATES,
		.formats = ALC1311_FORMATS,
	},
	.ops = &alc1311_aif_dai_ops,

};

static struct snd_soc_codec_driver soc_codec_dev_alc1311 = {
	.probe = alc1311_probe,
	.remove = alc1311_remove,
	.suspend = alc1311_suspend,
	.resume = alc1311_resume,
	.set_bias_level = alc1311_set_bias_level,
	.controls = alc1311_snd_controls,
	.num_controls = ARRAY_SIZE(alc1311_snd_controls),
	.dapm_widgets = alc1311_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(alc1311_dapm_widgets),
	.dapm_routes = alc1311_dapm_routes,
	.num_dapm_routes = ARRAY_SIZE(alc1311_dapm_routes),
};


static const struct regmap_config alc1311_regmap_config = {
	.reg_bits = 16,
	.val_bits = 16,

	.readable_reg = alc1311_readable_register,
	.volatile_reg = alc1311_volatile_register,
	.max_register = 0x8FF,
	.reg_defaults = alc1311_reg,
	.num_reg_defaults = ARRAY_SIZE(alc1311_reg),
	.cache_type = REGCACHE_RBTREE,
};


static int alc1311_i2c_probe(struct i2c_client *i2c,
		    const struct i2c_device_id *id)
{
	struct alc1311_priv *alc1311;
	int ret;
	int value_id;
	const char *dev_name = NULL;
	const char *dai_name = NULL;
	struct device_node *np = i2c->dev.of_node;

	if(np){
		ret = of_property_read_string_index(np, "dev-name", 0, &dev_name);
		if (ret == 0)
			dev_set_name(&i2c->dev, dev_name);

		ret = of_property_read_string_index(np, "dai-name", 0, &dai_name);
		if(ret == 0){
			strcpy(alc1311_dai.name, dai_name);
			dev_err(&i2c->dev, "dai_name=%s\n", alc1311_dai.name);
		}
	}

	alc1311 = kzalloc(sizeof(struct alc1311_priv), GFP_KERNEL);
	if (NULL == alc1311)
		return -ENOMEM;

	i2c_set_clientdata(i2c, alc1311);

	alc1311->regmap = devm_regmap_init_i2c(i2c, &alc1311_regmap_config);
	if (IS_ERR(alc1311->regmap))
		return PTR_ERR(alc1311->regmap);

	ret = regmap_read(alc1311->regmap, DEVICE_ID_REG, &value_id);
	if (ret < 0 || (value_id != DEVICE_ID_DATA)) {
		dev_err(&i2c->dev, "Failed to read device id from the alc1311: ret=%d,value_id(%d) != %d\n",
			ret, value_id, DEVICE_ID_DATA);
		return ret;
	}

	ret = snd_soc_register_codec(&i2c->dev, &soc_codec_dev_alc1311,
			&alc1311_dai, 1);
	if (ret < 0)
		kfree(alc1311);

	ret = sysfs_create_group(&i2c->dev.kobj, &alc1311_attribute_group);
    if (ret) {
        dev_err(&i2c->dev, "codec sysfs ret: %d\n", ret);
        return -1;
    }

/*
	ret = device_create_file(&i2c->dev, &dev_attr_index_reg);
	if (ret != 0) {
		dev_err(&i2c->dev,
			"Failed to create index_reg sysfs files: %d\n", ret);
		return ret;
	}
	ret = device_create_file(&i2c->dev, &dev_attr_codec_reg);
	if (ret != 0) {
		dev_err(&i2c->dev,
			"Failed to create codex_reg sysfs files: %d\n", ret);
		return ret;
	}
*/
	alc1311->pa_en_desc = devm_gpiod_get(&i2c->dev, "pa_en", GPIOD_OUT_HIGH);
	if (IS_ERR(alc1311->pa_en_desc)) {
		dev_err(&i2c->dev, "######unable to get gpio pa_en\n");
		alc1311->pa_en_desc = NULL;
	}
	if (alc1311->pa_en_desc){
		gpiod_set_value(alc1311->pa_en_desc, 1);
		printk("####%s... pa_en=%d\n",__func__,gpiod_get_value(alc1311->pa_en_desc));
	}

	return ret;
}


static int alc1311_i2c_remove(struct i2c_client *i2c)
{
	snd_soc_unregister_codec(&i2c->dev);
	kfree(i2c_get_clientdata(i2c));
	return 0;
}

void alc1311_i2c_shutdown(struct i2c_client *client)
{
	struct alc1311_priv *alc1311 = i2c_get_clientdata(client);
	struct snd_soc_codec *codec = alc1311->codec;

	if (codec != NULL)
		alc1311_set_bias_level(codec, SND_SOC_BIAS_OFF);
}

static const struct i2c_device_id alc1311_i2c_id[] = {
	{ "alc1311", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, alc1311_i2c_id);

#ifdef CONFIG_OF
static const struct of_device_id alc1311_of_match[] = {
	{.compatible = "realtek,alc1311"},
	{},
};
MODULE_DEVICE_TABLE(of, alc1311_of_match);
#endif


struct i2c_driver alc1311_i2c_driver = {
	.driver = {
		.name = "alc1311",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(alc1311_of_match),
	},
	.id_table = alc1311_i2c_id,
	.probe = 	alc1311_i2c_probe,
	.remove = 	alc1311_i2c_remove,
	.shutdown = alc1311_i2c_shutdown,
};


static int __init alc1311_modinit(void)
{
	return i2c_add_driver(&alc1311_i2c_driver);
}
module_init(alc1311_modinit);

static void __exit alc1311_modexit(void)
{
	i2c_del_driver(&alc1311_i2c_driver);
}
module_exit(alc1311_modexit);


MODULE_DESCRIPTION("ASoC ALC1311 driver");
MODULE_AUTHOR("Bard Liao <bardliao@realtek.com>");
MODULE_LICENSE("GPL");
