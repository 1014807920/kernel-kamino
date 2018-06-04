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
#include <linux/i2c.h>

#include "sy6025.h"

#define SY6025_FORMATS (SNDRV_PCM_FMTBIT_S8 | SNDRV_PCM_FMTBIT_S16_LE | \
		SNDRV_PCM_FMTBIT_S18_3LE | SNDRV_PCM_FMTBIT_S20_3LE)

static unsigned char rock_eq_tab[][20] = {
	{0x1f, 0x81, 0x27, 0x77, 0x00, 0xfe, 0xd7, 0x33, 0x00, 0x7e, 0x7d, 0xa8, 0x1f, 0x01, 0x29, 0xe3, 0x00, 0x80, 0x5b,0xf8}, //BQ0
	{0x1f, 0x83, 0x60, 0xae, 0x00, 0xfc, 0x93, 0xc4, 0x00, 0x7c, 0x27, 0xdd, 0x1f, 0x03, 0x6f, 0xef, 0x00, 0x80, 0x7b, 0x29}, //BQ1
	{0x1f, 0x89, 0x51, 0x85, 0x00, 0xf6, 0x54, 0x55, 0x00, 0x18, 0x51, 0xf6, 0x1f, 0xcb, 0x66, 0x6f, 0x00, 0x1c, 0xa1, 0xc1}, //BQ2
	{0x1f, 0xb1, 0xb2, 0x78, 0x00, 0xc1, 0xf7, 0x42, 0x01, 0x13, 0x56, 0x36, 0x1d, 0x8d, 0x49, 0xc0, 0x01, 0x6b, 0xb6,0x50}, //BQ3
	{0x1f, 0xc2, 0x71, 0x81, 0x00, 0xa4, 0xa2, 0xc3, 0x00, 0xe3, 0x2a, 0x9e, 0x1d, 0xdc, 0x6e, 0xdc, 0x01, 0x59, 0x52, 0x43}, //BQ4
};

static unsigned char classical_eq_tab[][20] = {
	{0x1f, 0xad, 0xd3, 0x78, 0x00, 0xc7, 0xef, 0x8d, 0x00, 0x1c, 0xeb, 0x58, 0x1f, 0xb3, 0x7c, 0x3d, 0x00, 0x39, 0xd5, 0x66}, //BQ0
};

static unsigned char dance_eq_tab[][20] = {
	{0x1f, 0x81, 0x30, 0xdb, 0x00, 0xfe, 0xcd, 0xb9, 0x00, 0x7e, 0x90, 0x9e, 0x1f, 0x01, 0x32, 0xfd, 0x00, 0x80, 0x3f, 0x3d}, //BQ0
	{0x1f, 0x88, 0x68, 0x2c, 0x00, 0xf7, 0x4e, 0xbc, 0x00, 0x76, 0xd4, 0x86, 0x1f, 0x08, 0xc0, 0x58, 0x00, 0x80, 0xd2, 0x62}, //BQ1
	{0x1f, 0xb3, 0x29, 0x0f, 0x00, 0xbf, 0xa3, 0xf8, 0x00, 0x33, 0xf2, 0xc6, 0x1f, 0x7a, 0x41, 0xa0, 0x00, 0x5e, 0xfe, 0x93}, //BQ2
};

static unsigned char pop_eq_tab[][20] = {
	{0x1f, 0x85, 0x86, 0x2f, 0x00, 0xfa, 0x48, 0x62, 0x00, 0x78, 0xdc, 0x3f, 0x1f, 0x05, 0xb7, 0x9e, 0x00, 0x81, 0x9d, 0x92}, //BQ0
	{0x1f, 0x88, 0x30, 0xc7, 0x00, 0xf7, 0x0b, 0xaa, 0x00, 0x70, 0x5c, 0xdb, 0x1f, 0x08, 0xf4, 0x57, 0x00, 0x87, 0x72, 0x5f}, //BQ1
};


struct device *dev;
static struct snd_soc_dai_driver sy6025_dai[] = {
	{
		.name     = "sy6025-dai",
		.playback = {
			.stream_name = "Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_48000,
			.formats = SY6025_FORMATS,
		},
	},
};

int sy6025_read_i2c(struct i2c_client *client, u8 reg, u8 *val)
{
	int ret;
	unsigned char data = reg;

	ret = i2c_master_send(client, &data, 1);
	if (ret < 1) {
		dev_err(&client->dev, "%s: i2c read error, reg: %x\n",
					__func__, reg);
		return ret < 0 ? ret : -EIO;
	}

	ret = i2c_master_recv(client, val, 3);
	if (ret < 3) {
		dev_err(&client->dev, "%s: i2c read error, reg: %x\n",
					__func__, reg);
		return ret < 0 ? ret : -EIO;
	}
	return 0;
}

int sy6025_write_i2c(struct i2c_client *client, u8 reg, u32 val, u8 count)
{
	int ret;
	unsigned char data[5] = {reg};

	switch (count) {
		case 0:
			break;
		case 1:
			data[1] = (unsigned char)(val & 0xff);
			break;
		case 2:
			data[1] = (unsigned char)((val >> 8) & 0xff);
			data[2] = (unsigned char)(val & 0xff);
			break;
		case 3:
			data[1] = (unsigned char)((val >> 16) & 0xff);
			data[2] = (unsigned char)((val >> 8) & 0xff);
			data[3] = (unsigned char)(val & 0xff);
			break;
		case 4:
			data[1] = (unsigned char)((val >> 24) & 0xff);
			data[2] = (unsigned char)((val >> 16) & 0xff);
			data[3] = (unsigned char)((val >> 8) & 0xff);
			data[4] = (unsigned char)(val & 0xff);
			break;
		default:
			return -1;
	}

	ret = i2c_master_send(client, data, count + 1);
	if (ret < (count + 1)) {
		dev_err(&client->dev, "%s: i2c write error, reg: %x\n",
					__func__, reg);
		return ret < 0 ? ret : -EIO;
	}
	mdelay(3);

	return 0;
}

int sy6025_write_eq(struct i2c_client *client, unsigned char (*eq_tab)[20],
				int index, unsigned char reg)
{
	int i = 0, ret = 0;
	unsigned char data[SY6025_BQ_NUM + 1] = {0};

	data[0] = reg;
	for(i = 0; i < SY6025_BQ_NUM; i++)
		data[i + 1] = eq_tab[index][i];

	for (i = 0; i < (SY6025_BQ_NUM + 1); i++)
		dev_err(&client->dev, "i:%d data:%#x", i, data[i]);

	ret = i2c_master_send(client, data, SY6025_BQ_NUM + 1);
	if (ret < (SY6025_BQ_NUM + 1)) {
		dev_err(&client->dev, "%s: i2c write error, reg: %x\n",
					__func__, reg);
		return ret < 0 ? ret : -EIO;
	}

	return 0;
}

static int sy6025_set_eq(struct sy6025_info *sy6025, unsigned char (*eq_tab)[20], unsigned int tab_num)
{
	unsigned int i;
	unsigned int addr = SY6025_BQ_BASEADDR;
	unsigned int filter1_reg_val = 0, filter2_reg_val = 0;

	sy6025_write_i2c(sy6025->i2c, 0x04, 0x16, 1); /*enable eq sew*/
	sy6025_write_i2c(sy6025->i2c, 0x03, 0x19, 1); /*enable the iic bus access to 0x30~4D*/

	for (i = 0; i < tab_num; i++) {
		sy6025_write_eq(sy6025->i2c, eq_tab, i, addr);
		addr++;
		filter1_reg_val |= 1 << i;
		filter2_reg_val |= 1 << i;
	}
	sy6025_write_i2c(sy6025->i2c, 0x25, filter1_reg_val, 1);
	sy6025_write_i2c(sy6025->i2c, 0x27, filter2_reg_val, 1);
	sy6025_write_i2c(sy6025->i2c, 0x03, 0x18, 1);

	return 0;
}

static int sy6025_set_audio_effect(struct sy6025_info *sy6025)
{
	switch (sy6025->audioEffect) {
		case DEFAULT_MODE:
			sy6025_write_i2c(sy6025->i2c, 0x04, 0x00, 1);
			break;
		case ROCK_MODE:
			sy6025_set_eq(sy6025, rock_eq_tab, 5);
			break;
		case CLASSICAL_MODE:
			sy6025_set_eq(sy6025, classical_eq_tab, 1);
			break;
		case DANCE_MODE:
			sy6025_set_eq(sy6025, dance_eq_tab, 3);
			break;
		case POP_MODE:
			sy6025_set_eq(sy6025, pop_eq_tab, 2);
			break;
		default:
			return -1;
	}

	return 0;
}

static int sy6025_set_volume(struct sy6025_info *sy6025, int dbValue)
{
	unsigned int dbIndex = 0;

	if (dbValue < -127)
		dbValue = -127;
	if (dbValue > 0)
		dbValue = 0;

	dbIndex = 2 * dbValue + 254;

	sy6025_write_i2c(sy6025->i2c, 0x07, dbIndex, 1);

	return 0;
}

static int sy6025_init(struct sy6025_info *sy6025)
{
	u32 val;

	// SY6025 initial setting
	sy6025_write_i2c(sy6025->i2c, 0x00, 0x1a, 1);
	sy6025_write_i2c(sy6025->i2c, 0x85, 0x00000201, 4);
	sy6025_write_i2c(sy6025->i2c, 0x86, 0x00000001, 4);
	sy6025_write_i2c(sy6025->i2c, 0x76, 0x0f, 1);
	sy6025_write_i2c(sy6025->i2c, 0x06, 0x10, 1);

	// Exit Shutdown
	sy6025_write_i2c(sy6025->i2c, 0x22, 0x01, 1);
	sy6025_write_i2c(sy6025->i2c, 0x06, 0x00, 1);

	// Disable Eq
	sy6025_write_i2c(sy6025->i2c, 0x04, 0x00, 1);

	// Set DRC
	sy6025_write_i2c(sy6025->i2c, 0x4e, 0x010000, 3);
	sy6025_write_i2c(sy6025->i2c, 0x4f, 0x7ff800, 3);
	sy6025_write_i2c(sy6025->i2c, 0x50, 0x010000, 3);
	sy6025_write_i2c(sy6025->i2c, 0x51, 0x7ff000, 3);
	sy6025_write_i2c(sy6025->i2c, 0x52, 0x010000, 3);
	sy6025_write_i2c(sy6025->i2c, 0x53, 0x7f0000, 3);
	sy6025_write_i2c(sy6025->i2c, 0x6d, 0x010000, 3);
	sy6025_write_i2c(sy6025->i2c, 0x6e, 0x7ff000, 3);

	// Release time setting
	sy6025_write_i2c(sy6025->i2c, 0x62, 0x7fbbcd, 3);
	sy6025_write_i2c(sy6025->i2c, 0x65, 0x7fbbcd, 3);
	sy6025_write_i2c(sy6025->i2c, 0x68, 0x7fbbcd, 3);
	sy6025_write_i2c(sy6025->i2c, 0x68, 0x7fbbcd, 3);
	sy6025_write_i2c(sy6025->i2c, 0x63, 0x7ffc96, 3);
	sy6025_write_i2c(sy6025->i2c, 0x66, 0x7ffc96, 3);
	sy6025_write_i2c(sy6025->i2c, 0x69, 0x7ffc96, 3);
	sy6025_write_i2c(sy6025->i2c, 0x6c, 0x7ffc96, 3);

	// Set DRC Gain
	sy6025_write_i2c(sy6025->i2c, 0x61, 0x3cc30c, 3);
	sy6025_write_i2c(sy6025->i2c, 0x64, 0x3cc30c, 3);
	sy6025_write_i2c(sy6025->i2c, 0x67, 0x3cc30c, 3);

	sy6025_set_volume(sy6025, SY6025_INIT_DB_VALUE);
	sy6025->dbValue = SY6025_INIT_DB_VALUE;
	sy6025->audioEffect = DEFAULT_MODE;

	sy6025_read_i2c(sy6025->i2c, 0x61, (u8 *)(&val));
	dev_err(dev, "0x61:%#x\n", val);
	sy6025_read_i2c(sy6025->i2c, 0x68, (u8 *)(&val));
	dev_err(dev, "0x68:%#x\n", val);

	return 0;
}


static int sy6025_probe(struct snd_soc_codec *codec)
{
	struct sy6025_info *sy6025 = snd_soc_codec_get_drvdata(codec);

	sy6025_init(sy6025);

	return 0;
}

static int sy6025_remove(struct snd_soc_codec *codec)
{
	return 0;
}

static int sy6025_control_volume_info(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_info *uinfo)
{
	uinfo->type  = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 1;

	uinfo->value.integer.min = SY6025_MIN_DB_VALUE;
	uinfo->value.integer.max = SY6025_MAX_DB_VALUE;
	uinfo->value.integer.step = 1;

	return 0;
}

static int sy6025_control_volume_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct snd_soc_codec *codec = component->codec;
	struct sy6025_info *sy6025 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = sy6025->dbValue;

	return 0;
}

static int sy6025_control_volume_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct snd_soc_codec *codec = component->codec;
	struct sy6025_info *sy6025 = snd_soc_codec_get_drvdata(codec);

	sy6025_set_volume(sy6025, ucontrol->value.integer.value[0]);
	sy6025->dbValue = ucontrol->value.integer.value[0];

	return 0;
}

static int sy6025_control_audio_effect_info(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_info *uinfo)
{
	const char *audio_effect[5] = {"default_mode", "rock_mode",
					"classical_mode", "dance_mode", "pop_mode"};
	uinfo->type  = SNDRV_CTL_ELEM_TYPE_ENUMERATED;
	uinfo->count = 1;
	uinfo->value.enumerated.items = 5;
	uinfo->value.enumerated.item  = (uinfo->value.enumerated.item >= 4) ? 4 : uinfo->value.enumerated.item;
	strcpy(uinfo->value.enumerated.name, audio_effect[uinfo->value.enumerated.item]);

	return 0;
}

static int sy6025_control_audio_effect_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct snd_soc_codec *codec = component->codec;
	struct sy6025_info *sy6025 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.enumerated.item[0] = sy6025->audioEffect;

	return 0;
}

static int sy6025_control_audio_effect_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct snd_soc_codec *codec = component->codec;
	struct sy6025_info *sy6025 = snd_soc_codec_get_drvdata(codec);

	sy6025->audioEffect = ucontrol->value.enumerated.item[0];
	sy6025_set_audio_effect(sy6025);

	return 0;
}

#define SY6025_CONTROL_VOLUME(xiface, xname, xindex, xvalue) { \
	.iface  = xiface, \
	.access = SNDRV_CTL_ELEM_ACCESS_READWRITE, \
	.name   = xname, \
	.index  = xindex, \
	.info = sy6025_control_volume_info, \
	.get  = sy6025_control_volume_get, \
	.put  = sy6025_control_volume_put, \
	.private_value = xvalue, \
}

#define SY6025_CONTROL_AUDIO_EFFECT(xiface, xname, xindex, xvalue) { \
	.iface  = xiface, \
	.access = SNDRV_CTL_ELEM_ACCESS_READWRITE, \
	.name   = xname, \
	.index  = xindex, \
	.info = sy6025_control_audio_effect_info, \
	.get  = sy6025_control_audio_effect_get, \
	.put  = sy6025_control_audio_effect_put, \
	.private_value = xvalue, \
}

static struct snd_kcontrol_new sy6025_controls[] = {
	SY6025_CONTROL_VOLUME(SNDRV_CTL_ELEM_IFACE_CARD, "SY6025 Playback Volume (DB)", 0, -1),
	SY6025_CONTROL_AUDIO_EFFECT(SNDRV_CTL_ELEM_IFACE_CARD, "SY6025 Playback Audio Effect", 0, -1),
};

static struct snd_soc_codec_driver soc_sy6025_drv = {
	.probe = sy6025_probe,
	.remove = sy6025_remove,
	.component_driver = {
		.controls = sy6025_controls,
		.num_controls = ARRAY_SIZE(sy6025_controls),
	},
};

static int sy6025_i2c_probe(struct i2c_client *client,
						    const struct i2c_device_id *id)
{
	struct sy6025_info *sy6025 = NULL;
	struct i2c_adapter *adapter = client->adapter;

	dev = &client->dev;

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA))
		return -ENODEV;

	sy6025 = devm_kzalloc(&client->dev, sizeof(struct sy6025_info), GFP_KERNEL);
	if (sy6025 == NULL)
		return -ENOMEM;

	dev_err(&client->dev, "sy6025:%x\n", (unsigned int)sy6025);
	sy6025->i2c = client;
	i2c_set_clientdata(client, sy6025);

	return snd_soc_register_codec(&client->dev, &soc_sy6025_drv,
				sy6025_dai, sizeof(sy6025_dai)/sizeof(struct snd_soc_dai_driver));

}

static int sy6025_i2c_remove(struct i2c_client *client)
{
	snd_soc_unregister_codec(&client->dev);
	return 0;
}

static const struct i2c_device_id sy6025_id[] = {
	{"sy6025", 0},
	{},
};

static const struct of_device_id sy6025_match_table[] = {
	{.compatible = "sy,sy6025"},
	{},
};

static struct i2c_driver sy6025_i2c_drv = {
	.probe  = sy6025_i2c_probe,
	.remove = sy6025_i2c_remove,
	.id_table = sy6025_id,
	.driver = {
		.name = "sy6025",
		.of_match_table = sy6025_match_table,
	},
};

module_i2c_driver(sy6025_i2c_drv);
MODULE_LICENSE("GPL");
