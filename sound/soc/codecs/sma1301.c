/* sma1301.c -- sma1301 ALSA SoC Audio driver
 *
 * r002, 2018.05.16	- initial version  sma1301
 *
 * Copyright 2018 Silicon Mitus Corporation / Iron Device Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/regmap.h>
#include <linux/version.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/initval.h>
#include <sound/tlv.h>
#include <linux/of_device.h>
#include <linux/slab.h>
#include <asm/div64.h>

#include "sma1301.h"

/* register dump to Linux console for EV board */
#define SMA1301_REGISTER_DUMP

enum sma1301_type {
	SMA1301,
};

struct sma1301_priv {
	enum sma1301_type devtype;
	struct regmap *regmap;
	unsigned int sysclk;
	unsigned int init_vol;
	bool stereo_select;
	bool sck_pll_enable;
	const uint32_t *reg_array;
	uint32_t reg_array_len;
	char board_rev[10];
	int i2s_mode;
};

/* Initial register value - {register, value} 2018.01.17
 * EQ Band : 1 to 5 / 0x40 to 0x8A (15EA register for each EQ Band)
 * Currently all EQ Bands are flat frequency response
 */
static const struct reg_default sma1301_reg_def[] = {
	{ 0x00, 0x80 }, /* 0x00 SystemCTRL  */
	{ 0x01, 0x00 }, /* 0x01 InputCTRL1  */
	{ 0x02, 0x00 }, /* 0x02 InputCTRL2  */
	{ 0x03, 0x01 }, /* 0x03 InputCTRL3  */
	{ 0x04, 0x17 }, /* 0x04 InputCTRL4  */
	{ 0x09, 0x00 }, /* 0x09 OutputCTRL  */
	{ 0x0A, 0x30 }, /* 0x0A SPK_VOL  */
	{ 0x0E, 0x0F }, /* 0x0E MUTE_VOL_CTRL  */
	{ 0x10, 0x04 }, /* 0x10 SystemCTRL1  */
	{ 0x11, 0x00 }, /* 0x11 SystemCTRL2  */
	{ 0x12, 0x00 }, /* 0x12 SystemCTRL3  */
	{ 0x14, 0x12 }, /* 0x14 Modulator  */
	{ 0x15, 0x01 }, /* 0x15 BassSpk1  */
	{ 0x16, 0x0F }, /* 0x16 BassSpk2  */
	{ 0x17, 0x0F }, /* 0x17 BassSpk3  */
	{ 0x18, 0x0F }, /* 0x18 BassSpk4  */
	{ 0x19, 0x00 }, /* 0x19 BassSpk5  */
	{ 0x1A, 0x00 }, /* 0x1A BassSpk6  */
	{ 0x1B, 0x00 }, /* 0x1B BassSpk7  */
	{ 0x23, 0x19 }, /* 0x23 CompLim1  */
	{ 0x24, 0x00 }, /* 0x24 CompLim2  */
	{ 0x25, 0x00 }, /* 0x25 CompLim3  */
	{ 0x26, 0x04 }, /* 0x26 CompLim4  */
	{ 0x2B, 0x00 }, /* 0x2B EqMode  */
	{ 0x2C, 0x2C }, /* 0x2C EqGraphic1  */
	{ 0x2D, 0x2C }, /* 0x2D EqGraphic2  */
	{ 0x2E, 0x2C }, /* 0x2E EqGraphic3  */
	{ 0x2F, 0x2C }, /* 0x2F EqGraphic4  */
	{ 0x30, 0x2C }, /* 0x30 EqGraphic5  */
	{ 0x33, 0x00 }, /* 0x33 FLLCTRL  */
	{ 0x36, 0x92 }, /* 0x36 Protection  */
	{ 0x37, 0x3F }, /* 0x37 SlopeCTRL  */
	{ 0x3B, 0x00 }, /* 0x3B Test1  */
	{ 0x3C, 0x00 }, /* 0x3C Test2  */
	{ 0x3D, 0x00 }, /* 0x3D Test3  */
	{ 0x3E, 0x03 }, /* 0x3E ATest1  */
	{ 0x3F, 0x00 }, /* 0x3F ATest2  */
	{ 0x40, 0x3E }, /* 0x40 EQCTRL1 */
	{ 0x41, 0x55 }, /* 0x41 EQCTRL2  */
	{ 0x42, 0x05 }, /* 0x42 EQCTRL3  */
	{ 0x43, 0x88 }, /* 0x43 EQCTRL4  */
	{ 0x44, 0xAF }, /* 0x44 EQCTRL5  */
	{ 0x45, 0x07 }, /* 0x45 EQCTRL6  */
	{ 0x46, 0x40 }, /* 0x46 EQCTRL7  */
	{ 0x47, 0x00 }, /* 0x47 EQCTRL8  */
	{ 0x48, 0x00 }, /* 0x48 EQCTRL9  */
	{ 0x49, 0xC1 }, /* 0x49 EQCTRL10  */
	{ 0x4A, 0xAA }, /* 0x4A EQCTRL11  */
	{ 0x4B, 0x03 }, /* 0x4B EQCTRL12  */
	{ 0x4C, 0x7E }, /* 0x4C EQCTRL13  */
	{ 0x4D, 0x50 }, /* 0x4D EQCTRL14  */
	{ 0x4E, 0x01 }, /* 0x4E EQCTRL15  */
	{ 0x4F, 0x3B }, /* 0x4F EQCTRL16 : EQ BAND2 */
	{ 0x50, 0xE3 }, /* 0x50 EQCTRL17  */
	{ 0x51, 0x05 }, /* 0x51 EQCTRL18  */
	{ 0x52, 0x84 }, /* 0x52 EQCTRL19  */
	{ 0x53, 0x3E }, /* 0x53 EQCTRL20  */
	{ 0x54, 0x05 }, /* 0x54 EQCTRL21  */
	{ 0x55, 0x40 }, /* 0x55 EQCTRL22  */
	{ 0x56, 0x00 }, /* 0x56 EQCTRL23  */
	{ 0x57, 0x00 }, /* 0x57 EQCTRL24  */
	{ 0x58, 0xC4 }, /* 0x58 EQCTRL25  */
	{ 0x59, 0x1C }, /* 0x59 EQCTRL26  */
	{ 0x5A, 0x03 }, /* 0x5A EQCTRL27  */
	{ 0x5B, 0x7B }, /* 0x5B EQCTRL28  */
	{ 0x5C, 0xC1 }, /* 0x5C EQCTRL29  */
	{ 0x5D, 0x03 }, /* 0x5D EQCTRL30  */
	{ 0x5E, 0x35 }, /* 0x5E EQCTRL31 : EQ BAND3 */
	{ 0x5F, 0x15 }, /* 0x5F EQCTRL32  */
	{ 0x60, 0x03 }, /* 0x60 EQCTRL33  */
	{ 0x61, 0x8B }, /* 0x61 EQCTRL34  */
	{ 0x62, 0xEB }, /* 0x62 EQCTRL35  */
	{ 0x63, 0x00 }, /* 0x63 EQCTRL36  */
	{ 0x64, 0x40 }, /* 0x64 EQCTRL37  */
	{ 0x65, 0x00 }, /* 0x65 EQCTRL38  */
	{ 0x66, 0x00 }, /* 0x66 EQCTRL39  */
	{ 0x67, 0xCA }, /* 0x67 EQCTRL40  */
	{ 0x68, 0xEA }, /* 0x68 EQCTRL41  */
	{ 0x69, 0x05 }, /* 0x69 EQCTRL42  */
	{ 0x6A, 0x74 }, /* 0x6A EQCTRL43  */
	{ 0x6B, 0x15 }, /* 0x6B EQCTRL44  */
	{ 0x6C, 0x00 }, /* 0x6C EQCTRL45  */
	{ 0x6D, 0x25 }, /* 0x6D EQCTRL46 : EQ BAND4 */
	{ 0x6E, 0xE9 }, /* 0x6E EQCTRL47  */
	{ 0x6F, 0x05 }, /* 0x6F EQCTRL48  */
	{ 0x70, 0xA0 }, /* 0x70 EQCTRL49  */
	{ 0x71, 0xDB }, /* 0x71 EQCTRL50  */
	{ 0x72, 0x02 }, /* 0x72 EQCTRL51  */
	{ 0x73, 0x40 }, /* 0x73 EQCTRL52  */
	{ 0x74, 0x00 }, /* 0x74 EQCTRL53  */
	{ 0x75, 0x00 }, /* 0x75 EQCTRL54  */
	{ 0x76, 0xDA }, /* 0x76 EQCTRL55  */
	{ 0x77, 0x16 }, /* 0x77 EQCTRL56  */
	{ 0x78, 0x03 }, /* 0x78 EQCTRL57  */
	{ 0x79, 0x5F }, /* 0x79 EQCTRL58  */
	{ 0x7A, 0x24 }, /* 0x7A EQCTRL59  */
	{ 0x7B, 0x06 }, /* 0x7B EQCTRL60  */
	{ 0x7C, 0x0D }, /* 0x7C EQCTRL61 : EQ BAND5 */
	{ 0x7D, 0x48 }, /* 0x7D EQCTRL62  */
	{ 0x7E, 0x00 }, /* 0x7E EQCTRL63  */
	{ 0x7F, 0xE1 }, /* 0x7F EQCTRL64  */
	{ 0x80, 0x7E }, /* 0x80 EQCTRL65  */
	{ 0x81, 0x03 }, /* 0x81 EQCTRL66  */
	{ 0x82, 0x40 }, /* 0x82 EQCTRL67  */
	{ 0x83, 0x00 }, /* 0x83 EQCTRL68  */
	{ 0x84, 0x00 }, /* 0x84 EQCTRL69  */
	{ 0x85, 0xF2 }, /* 0x85 EQCTRL70  */
	{ 0x86, 0xB8 }, /* 0x86 EQCTRL71  */
	{ 0x87, 0x00 }, /* 0x87 EQCTRL72  */
	{ 0x88, 0x1E }, /* 0x88 EQCTRL73  */
	{ 0x89, 0x81 }, /* 0x89 EQCTRL74  */
	{ 0x8A, 0x05 }, /* 0x8A EQCTRL75  */
	{ 0x8B, 0x06 }, /* 0x8B PLL_POST_N  */
	{ 0x8C, 0xC0 }, /* 0x8C PLL_N  */
	{ 0x8D, 0x00 }, /* 0x8D PLL_F1  */
	{ 0x8E, 0x00 }, /* 0x8E PLL_F2  */
	{ 0x8F, 0x02 }, /* 0x8F PLL_F3,P,CP  */
	{ 0x91, 0x82 }, /* 0x91 ClassG Control  */
	{ 0x92, 0xC0 }, /* 0x92 FDPEC Control  */
	{ 0x94, 0x33 }, /* 0x94 Boost Control1  */
	{ 0x95, 0x39 }, /* 0x95 Boost Control2  */
	{ 0x96, 0x93 }, /* 0x96 Boost Control3  */
	{ 0x97, 0x6A }, /* 0x97 Boost Control4  */
	{ 0xA2, 0x88 }, /* 0xA2 TOP_MAN1  */
	{ 0xA3, 0x28 }, /* 0xA3 TOP_MAN2  */
	{ 0xA4, 0x40 }, /* 0xA4 TOP_MAN3  */
	{ 0xFA, 0xC0 }, /* 0xFA Status1  */
	{ 0xFB, 0x06 }, /* 0xFB Status2  */
	{ 0xFD, 0x00 }, /* 0xFD Status3  */
	{ 0xFF, 0x0A }, /* 0xFF Device Index  */
};

/*
static const struct reg_default sma1301_reg_init[] = {
	{ 0x00 , 0x81 },
	{ 0x01 , 0x00 },
	{ 0x02 , 0x00  },
	{ 0x03 , 0x01  },
	{ 0x04 , 0x17 },
	{ 0x05 , 0x01  },
	{ 0x06 , 0x00  },
	{ 0x07 , 0x01  },
	{ 0x08 , 0x17 },
	{ 0x09 , 0x00  },
	{ 0x0A , 0x18 },
	{ 0x0B , 0x30 },
	{ 0x0C , 0x58 },
	{ 0x0D , 0x58 },
	{ 0x0E , 0xfe },
	{ 0x0F , 0x0f  },
	{ 0x10, 0x04  },
	{ 0x11, 0xe0 },
	{ 0x12, 0x00  },
	{ 0x13, 0x23 },
	{ 0x14, 0x12 },
	{ 0x15, 0x01  },
	{ 0x16, 0x08  },
	{ 0x17, 0x08  },
	{ 0x18, 0x14 },
	{ 0x19, 0x09  },
	{ 0x1A, 0x27 },
	{ 0x1B, 0x06  },
	{ 0x1C, 0x01  },
	{ 0x1D, 0x0f  },
	{ 0x1E, 0x0f  },
	{ 0x1F, 0x0f  },
	{ 0x20, 0x00  },
	{ 0x21, 0x00  },
	{ 0x22, 0x00  },
	{ 0x23, 0x1e },
	{ 0x24, 0x05  },
	{ 0x25, 0x05  },
	{ 0x26, 0x41 },
	{ 0x27, 0x19 },
	{ 0x28, 0x00  },
	{ 0x29, 0x00  },
	{ 0x2A, 0x04  },
	{ 0x2B, 0x00  },
	{ 0x2C, 0x0c  },
	{ 0x2D, 0x0c  },
	{ 0x2E, 0x2c },
	{ 0x2F, 0x2c },
	{ 0x30, 0x2c },
	{ 0x31, 0x2c },
	{ 0x32, 0x38 },
	{ 0x33, 0x00  },
	{ 0x34, 0x00  },
	{ 0x35, 0x59 },
	{ 0x36, 0x92 },
	{ 0x37, 0x3f },
	{ 0x38, 0x00  },
	{ 0x39, 0x1d },
	{ 0x3A, 0x00  },
	{ 0x3B, 0x00  },
	{ 0x3C, 0x00  },
	{ 0x3D, 0x00  },
	{ 0x3E, 0x03  },
	{ 0x3F, 0x00  },
	{ 0x40, 0x38 },
	{ 0x41, 0x2c },
	{ 0x42, 0x04  },
	{ 0x43, 0x8a },
	{ 0x44, 0x31 },
	{ 0x45, 0x01  },
	{ 0x46, 0x3f },
	{ 0x47, 0x1a },
	{ 0x48, 0x02  },
	{ 0x49, 0xc8 },
	{ 0x4A, 0xb9 },
	{ 0x4B, 0x02  },
	{ 0x4C, 0x75 },
	{ 0x4D, 0xce },
	{ 0x4E, 0x07  },
	{ 0x4F, 0x27 },
	{ 0x50, 0x35 },
	{ 0x51, 0x06  },
	{ 0x52, 0xc0 },
	{ 0x53, 0x4d },
	{ 0x54, 0x01  },
	{ 0x55, 0x35 },
	{ 0x56, 0x53 },
	{ 0x57, 0x07  },
	{ 0x58, 0xe3 },
	{ 0x59, 0x76 },
	{ 0x5A, 0x02  },
	{ 0x5B, 0x3f },
	{ 0x5C, 0xb2 },
	{ 0x5D, 0x07  },
	{ 0x5E, 0x35 },
	{ 0x5F, 0x15 },
	{ 0x60, 0x03  },
	{ 0x61, 0x8b },
	{ 0x62, 0xeb },
	{ 0x63, 0x00  },
	{ 0x64, 0x40 },
	{ 0x65, 0x00  },
	{ 0x66, 0x00  },
	{ 0x67, 0xca },
	{ 0x68, 0xea },
	{ 0x69, 0x05  },
	{ 0x6A, 0x74 },
	{ 0x6B, 0x15 },
	{ 0x6C, 0x00  },
	{ 0x6D, 0x25 },
	{ 0x6E, 0xe9 },
	{ 0x6F, 0x05  },
	{ 0x70, 0xa0 },
	{ 0x71, 0xdb },
	{ 0x72, 0x02  },
	{ 0x73, 0x40 },
	{ 0x74, 0x00  },
	{ 0x75, 0x00  },
	{ 0x76, 0xda },
	{ 0x77, 0x16 },
	{ 0x78, 0x03  },
	{ 0x79, 0x5f },
	{ 0x7A, 0x24 },
	{ 0x7B, 0x06  },
	{ 0x7C, 0x0d  },
	{ 0x7D, 0x48 },
	{ 0x7E, 0x00  },
	{ 0x7F, 0xe1 },
	{ 0x80, 0x7e },
	{ 0x81, 0x03  },
	{ 0x82, 0x40 },
	{ 0x83, 0x00  },
	{ 0x84, 0x00  },
	{ 0x85, 0xf2 },
	{ 0x86, 0xb8 },
	{ 0x87, 0x00  },
	{ 0x88, 0x1e },
	{ 0x89, 0x81 },
	{ 0x8A, 0x05  },
	{ 0x8B, 0x08  },
	{ 0x8C, 0x20 },
	{ 0x8D, 0x00  },
	{ 0x8E, 0x00  },
	{ 0x8F, 0x02  },
	{ 0x90, 0x00  },
	{ 0x91, 0x42 },
	{ 0x92, 0x80 },
	{ 0x93, 0x02  },
	{ 0x94, 0x37 },
	{ 0x95, 0x0f  },
	{ 0x96, 0x82 },
	{ 0x97, 0x6a },
	{ 0x98, 0xdf },
	{ 0x99, 0xff },
	{ 0x9A, 0xff },
	{ 0x9B, 0xfd },
	{ 0x9C, 0xd7 },
	{ 0x9D, 0xdb },
	{ 0x9E, 0xf9 },
	{ 0x9F, 0x37 },
	{ 0xA0, 0xf3 },
	{ 0xA1, 0xff },
	{ 0xA2, 0x68 },
	{ 0xA3, 0x28 },
	{ 0xA4, 0x40 },
	{ 0xA5, 0xff },
	{ 0xA6, 0xf8 },
	{ 0xA7, 0xea },
	{ 0xA8, 0xc8 },
	{ 0xA9, 0xff },
	{ 0xAA, 0x7d },
	{ 0xAB, 0x3d },
	{ 0xAC, 0x60 },
	{ 0xAD, 0xff },
	{ 0xAE, 0xe9 },
	{ 0xAF, 0x6f },
	{ 0xB0, 0xf3 },
	{ 0xB1, 0xff },
	{ 0xB2, 0xea },
	{ 0xB3, 0x6a },
	{ 0xB4, 0x73 },
	{ 0xB5, 0xff },
	{ 0xB6, 0xfa },
	{ 0xB7, 0x7e },
	{ 0xB8, 0xff },
	{ 0xB9, 0xff },
	{ 0xBA, 0xff },
	{ 0xBB, 0xff },
	{ 0xBC, 0xdf },
	{ 0xBD, 0xff },
	{ 0xBE, 0xf9 },
	{ 0xBF, 0xff },
	{ 0xC0, 0x00  },
	{ 0xC1, 0x00  },
	{ 0xC2, 0x00 },
	{ 0xC3, 0x00  },
	{ 0xC4, 0x00  },
	{ 0xC5, 0x00  },
	{ 0xC6, 0x00  },
	{ 0xC7, 0x00  },
	{ 0xC8, 0x00  },
	{ 0xC9, 0x00  },
	{ 0xCA, 0x00  },
	{ 0xCB, 0x00  },
	{ 0xCC, 0x00  },
	{ 0xCD, 0x00  },
	{ 0xCE, 0x00  },
	{ 0xCF, 0x00  },
	{ 0xD0, 0x00  },
	{ 0xD1, 0x00  },
	{ 0xD2, 0x00  },
	{ 0xD3, 0x00  },
	{ 0xD4, 0x00  },
	{ 0xD5, 0x00  },
	{ 0xD6, 0x00  },
	{ 0xD7, 0x00  },
	{ 0xD8, 0x00  },
	{ 0xD9, 0x00  },
	{ 0xDA, 0x00  },
	{ 0xDB, 0x00  },
	{ 0xDC, 0x00  },
	{ 0xDD, 0x00  },
	{ 0xDE, 0x00  },
	{ 0xDF, 0x00  },
	{ 0xE0, 0x00  },
	{ 0xE1, 0x00  },
	{ 0xE2, 0x00  },
	{ 0xE3, 0x00  },
	{ 0xE4, 0x00  },
	{ 0xE5, 0x00  },
	{ 0xE6, 0x00  },
	{ 0xE7, 0x00  },
	{ 0xE8, 0x00  },
	{ 0xE9, 0x00  },
	{ 0xEA, 0x00  },
	{ 0xEB, 0x00  },
	{ 0xEC, 0x00  },
	{ 0xED, 0x00  },
	{ 0xEE, 0x00  },
	{ 0xEF, 0x00  },
	{ 0xF0, 0x00  },
	{ 0xF1, 0x00  },
	{ 0xF2, 0x00  },
	{ 0xF3, 0x00  },
	{ 0xF4, 0x00  },
	{ 0xF5, 0x00  },
	{ 0xF6, 0x00  },
	{ 0xF7, 0x00  },
	{ 0xF8, 0x00  },
	{ 0xF9, 0x00  },
	{ 0xFA, 0xc0 },
	{ 0xFB, 0x00  },
	{ 0xFC, 0x3f },
	{ 0xFD, 0x00  },
	{ 0xFE, 0x00  },
	{ 0xFF, 0x0a  },
};
*/

static bool sma1301_readable_register(struct device *dev, unsigned int reg)
{
	if (reg > SMA1301_FF_DEVICE_INDEX)
		return false;

	switch (reg) {
	case SMA1301_00_SYSTEM_CTRL ... SMA1301_04_INPUT1_CTRL4:
	case SMA1301_09_OUTPUT_CTRL ... SMA1301_0A_SPK_VOL:
	case SMA1301_0E_MUTE_VOL_CTRL:
	case SMA1301_10_SYSTEM_CTRL1 ... SMA1301_12_SYSTEM_CTRL3:
	case SMA1301_14_MODULATOR ... SMA1301_1B_BASS_SPK7:
	case SMA1301_23_COMPLIM1 ... SMA1301_26_COMPLIM4:
	case SMA1301_2B_EQ_MODE ... SMA1301_30_EQ_GRAPHIC5:
	case SMA1301_36_PROTECTION ... SMA1301_37_SLOPE_CTRL:
	case SMA1301_3B_TEST1 ... SMA1301_8F_PLL_F3:
	case SMA1301_91_CLASS_G_CTRL ... SMA1301_92_FDPEC_CTRL:
	case SMA1301_94_BOOST_CTRL1 ... SMA1301_97_BOOST_CTRL4:
	case SMA1301_A2_TOP_MAN1 ... SMA1301_A4_TOP_MAN3:
	case SMA1301_FA_STATUS1 ... SMA1301_FB_STATUS2:
	case SMA1301_FD_STATUS3:
	case SMA1301_FF_DEVICE_INDEX:
		return true;
	default:
		return false;
	}
}

static bool sma1301_writeable_register(struct device *dev, unsigned int reg)
{
	if (reg > SMA1301_FF_DEVICE_INDEX)
		return false;

	switch (reg) {
	case SMA1301_00_SYSTEM_CTRL ... SMA1301_04_INPUT1_CTRL4:
	case SMA1301_09_OUTPUT_CTRL ... SMA1301_0A_SPK_VOL:
	case SMA1301_0E_MUTE_VOL_CTRL:
	case SMA1301_10_SYSTEM_CTRL1 ... SMA1301_12_SYSTEM_CTRL3:
	case SMA1301_14_MODULATOR ... SMA1301_1B_BASS_SPK7:
	case SMA1301_23_COMPLIM1 ... SMA1301_26_COMPLIM4:
	case SMA1301_2B_EQ_MODE ... SMA1301_30_EQ_GRAPHIC5:
	case SMA1301_33_FLL_CTRL:
	case SMA1301_36_PROTECTION ... SMA1301_37_SLOPE_CTRL:
	case SMA1301_3B_TEST1 ... SMA1301_8F_PLL_F3:
	case SMA1301_91_CLASS_G_CTRL ... SMA1301_92_FDPEC_CTRL:
	case SMA1301_94_BOOST_CTRL1 ... SMA1301_97_BOOST_CTRL4:
	case SMA1301_A2_TOP_MAN1 ... SMA1301_A4_TOP_MAN3:
		return true;
	default:
		return false;
	}
}

static bool sma1301_volatile_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case SMA1301_FA_STATUS1 ... SMA1301_FB_STATUS2:
	case SMA1301_FD_STATUS3:
	case SMA1301_FF_DEVICE_INDEX:
		return true;
	default:
		return false;
	}
}

/* DB scale conversion of speaker volume */
static const DECLARE_TLV_DB_SCALE(sma1301_spk_tlv, -5950, 50, 0);

/* InputCTRL1 Set */
static const char * const sma1301_input_format_text[] = {
	"Philips standard I2S", "Left justified", "Not used",
	"Not used", "Right justified 16bits", "Right justified 18bits",
	"Right justified 20bits", "Right justified 24bits"};

static const struct soc_enum sma1301_input_format_enum =
SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(sma1301_input_format_text),
		sma1301_input_format_text);

static int sma1301_input_format_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sma1301_priv *sma1301 = snd_soc_codec_get_drvdata(codec);
	int val;

	regmap_read(sma1301->regmap, SMA1301_01_INPUT1_CTRL1, &val);
	ucontrol->value.integer.value[0] = ((val & 0x70) >> 4);

	return 0;
}

static int sma1301_input_format_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sma1301_priv *sma1301 = snd_soc_codec_get_drvdata(codec);
	int sel = (int)ucontrol->value.integer.value[0];

	regmap_update_bits(sma1301->regmap,
		SMA1301_01_INPUT1_CTRL1, 0x70, (sel << 4));

	return 0;
}

/* InputCTRL2 Set */
static const char * const sma1301_in_audio_mode_text[] = {
	"I2S mode", "PCM/IOM2 short sync", "PCM/IOM2 long sync", "Reserved"};

static const struct soc_enum sma1301_in_audio_mode_enum =
SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(sma1301_in_audio_mode_text),
		sma1301_in_audio_mode_text);

static int sma1301_in_audio_mode_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sma1301_priv *sma1301 = snd_soc_codec_get_drvdata(codec);
	int val;

	regmap_read(sma1301->regmap, SMA1301_02_INPUT1_CTRL2, &val);
	ucontrol->value.integer.value[0] = ((val & 0xC0) >> 6);

	return 0;
}

static int sma1301_in_audio_mode_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sma1301_priv *sma1301 = snd_soc_codec_get_drvdata(codec);
	int sel = (int)ucontrol->value.integer.value[0];

	regmap_update_bits(sma1301->regmap,
		SMA1301_02_INPUT1_CTRL2, 0xC0, (sel << 6));

	return 0;
}

/* InputCTRL3 Set */
static const char * const sma1301_pcm_n_slot_text[] = {
	"1", "2", "3", "4", "5", "6", "7", "8", "9",
	"10", "11", "12", "13", "14", "15", "16"};

static const struct soc_enum sma1301_pcm_n_slot_enum =
SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(sma1301_pcm_n_slot_text),
		sma1301_pcm_n_slot_text);

static int sma1301_pcm_n_slot_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sma1301_priv *sma1301 = snd_soc_codec_get_drvdata(codec);
	int val;

	regmap_read(sma1301->regmap, SMA1301_03_INPUT1_CTRL3, &val);
	ucontrol->value.integer.value[0] = (val & 0x0F);

	return 0;
}

static int sma1301_pcm_n_slot_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sma1301_priv *sma1301 = snd_soc_codec_get_drvdata(codec);
	int sel = (int)ucontrol->value.integer.value[0];

	regmap_update_bits(sma1301->regmap,
		SMA1301_03_INPUT1_CTRL3, 0x0F, sel);

	return 0;
}

/* InputCTRL4 Set */
static const char * const sma1301_pcm1_slot_text[] = {
	"1", "2", "3", "4", "5", "6", "7", "8", "9",
	"10", "11", "12", "13", "14", "15", "16"};

static const struct soc_enum sma1301_pcm1_slot_enum =
SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(sma1301_pcm1_slot_text),
		sma1301_pcm1_slot_text);

static int sma1301_pcm1_slot_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sma1301_priv *sma1301 = snd_soc_codec_get_drvdata(codec);
	int val;

	regmap_read(sma1301->regmap, SMA1301_04_INPUT1_CTRL4, &val);
	ucontrol->value.integer.value[0] = ((val & 0xF0) >> 4);

	return 0;
}

static int sma1301_pcm1_slot_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sma1301_priv *sma1301 = snd_soc_codec_get_drvdata(codec);
	int sel = (int)ucontrol->value.integer.value[0];

	regmap_update_bits(sma1301->regmap,
		SMA1301_04_INPUT1_CTRL4, 0xF0, (sel << 4));

	return 0;
}

/* InputCTRL4 Set */
static const char * const sma1301_pcm2_slot_text[] = {
	"1", "2", "3", "4", "5", "6", "7", "8", "9",
	"10", "11", "12", "13", "14", "15", "16"};

static const struct soc_enum sma1301_pcm2_slot_enum =
SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(sma1301_pcm2_slot_text),
		sma1301_pcm2_slot_text);

static int sma1301_pcm2_slot_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sma1301_priv *sma1301 = snd_soc_codec_get_drvdata(codec);
	int val;

	regmap_read(sma1301->regmap, SMA1301_04_INPUT1_CTRL4, &val);
	ucontrol->value.integer.value[0] = (val & 0x0F);

	return 0;
}

static int sma1301_pcm2_slot_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sma1301_priv *sma1301 = snd_soc_codec_get_drvdata(codec);
	int sel = (int)ucontrol->value.integer.value[0];

	regmap_update_bits(sma1301->regmap,
		SMA1301_04_INPUT1_CTRL4, 0x0F, sel);

	return 0;
}

/* Output port config */
static const char * const sma1301_port_config_text[] = {
"Input port only", "Reserved", "Output port enable", "Reserved"};

static const struct soc_enum sma1301_port_config_enum =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(sma1301_port_config_text),
			sma1301_port_config_text);

static int sma1301_port_config_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sma1301_priv *sma1301 = snd_soc_codec_get_drvdata(codec);
	int val;

	regmap_read(sma1301->regmap, SMA1301_09_OUTPUT_CTRL, &val);
	ucontrol->value.integer.value[0] = ((val & 0x60) >> 5);

	return 0;
}

static int sma1301_port_config_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sma1301_priv *sma1301 = snd_soc_codec_get_drvdata(codec);
	int sel = (int)ucontrol->value.integer.value[0];

	regmap_update_bits(sma1301->regmap,
		SMA1301_09_OUTPUT_CTRL, 0x60, (sel << 5));

	return 0;
}

/* Output format select */
static const char * const sma1301_port_out_format_text[] = {
"I2S 32 SCK", "I2S 64 SCK", "PCM Short sync 128xFS", "Reserved"};

static const struct soc_enum sma1301_port_out_format_enum =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(sma1301_port_out_format_text),
	sma1301_port_out_format_text);

static int sma1301_port_out_format_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sma1301_priv *sma1301 = snd_soc_codec_get_drvdata(codec);
	int val;

	regmap_read(sma1301->regmap, SMA1301_09_OUTPUT_CTRL, &val);
	ucontrol->value.integer.value[0] = ((val & 0x18) >> 3);

	return 0;
}

static int sma1301_port_out_format_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sma1301_priv *sma1301 = snd_soc_codec_get_drvdata(codec);
	int sel = (int)ucontrol->value.integer.value[0];

	regmap_update_bits(sma1301->regmap,
		SMA1301_09_OUTPUT_CTRL, 0x18, (sel << 3));

	return 0;
}

/* Output source */
static const char * const sma1301_port_out_sel_text[] = {
	"Disable", "Format Converter", "Mixer Outout",
	"SPK path, EQ, Bass, Vol, DRC",
	"Half bridge path, EQ, Bass, Vol, DRC",
	"Reserved", "Reserved", "Reserved"};

static const struct soc_enum sma1301_port_out_sel_enum =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(sma1301_port_out_sel_text),
	sma1301_port_out_sel_text);

static int sma1301_port_out_sel_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sma1301_priv *sma1301 = snd_soc_codec_get_drvdata(codec);
	int val;

	regmap_read(sma1301->regmap, SMA1301_09_OUTPUT_CTRL, &val);
	ucontrol->value.integer.value[0] = (val & 0x07);

	return 0;
}

static int sma1301_port_out_sel_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sma1301_priv *sma1301 = snd_soc_codec_get_drvdata(codec);
	int sel = (int)ucontrol->value.integer.value[0];

	regmap_update_bits(sma1301->regmap,
		SMA1301_09_OUTPUT_CTRL, 0x07, sel);

	return 0;
}

/* Volume slope */
static const char * const sma1301_vol_slope_text[] = {
"Off", "Slow(approximately 1sec)", "Medium(approximately 0.5sec)",
"Fast(approximately 0.1sec)"};

static const struct soc_enum sma1301_vol_slope_enum =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(sma1301_vol_slope_text),
	sma1301_vol_slope_text);

static int sma1301_vol_slope_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sma1301_priv *sma1301 = snd_soc_codec_get_drvdata(codec);
	int val;

	regmap_read(sma1301->regmap, SMA1301_0E_MUTE_VOL_CTRL, &val);
	ucontrol->value.integer.value[0] = ((val & 0xC0) >> 6);

	return 0;
}

static int sma1301_vol_slope_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sma1301_priv *sma1301 = snd_soc_codec_get_drvdata(codec);
	int sel = (int)ucontrol->value.integer.value[0];

	regmap_update_bits(sma1301->regmap,
		SMA1301_0E_MUTE_VOL_CTRL, 0xC0, (sel << 6));

	return 0;
}

/* Mute slope */
static const char * const sma1301_mute_slope_text[] = {
"Off", "Slow(approximately 200msec)", "Medium(approximately 50msec)",
"Fast(approximately 10msec)"};

static const struct soc_enum sma1301_mute_slope_enum =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(sma1301_mute_slope_text),
	sma1301_mute_slope_text);

static int sma1301_mute_slope_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sma1301_priv *sma1301 = snd_soc_codec_get_drvdata(codec);
	int val;

	regmap_read(sma1301->regmap, SMA1301_0E_MUTE_VOL_CTRL, &val);
	ucontrol->value.integer.value[0] = ((val & 0x30) >> 4);

	return 0;
}

static int sma1301_mute_slope_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sma1301_priv *sma1301 = snd_soc_codec_get_drvdata(codec);
	int sel = (int)ucontrol->value.integer.value[0];

	regmap_update_bits(sma1301->regmap,
		SMA1301_0E_MUTE_VOL_CTRL, 0x30, (sel << 4));

	return 0;
}

/* Speaker mode */
static const char * const sma1301_spkmode_text[] = {
	"Off", "Mono for one chip solution", "Reserved",
	"Reserved", "Stereo for two chip solution",
	"Reserved", "Reserved", "Reserved"};

static const struct soc_enum sma1301_spkmode_enum =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(sma1301_spkmode_text),
	sma1301_spkmode_text);

static int sma1301_spkmode_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sma1301_priv *sma1301 = snd_soc_codec_get_drvdata(codec);
	int val;

	regmap_read(sma1301->regmap, SMA1301_10_SYSTEM_CTRL1, &val);
	ucontrol->value.integer.value[0] = ((val & 0x1C) >> 2);

	return 0;
}

static int sma1301_spkmode_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sma1301_priv *sma1301 = snd_soc_codec_get_drvdata(codec);
	int sel = (int)ucontrol->value.integer.value[0];

	regmap_update_bits(sma1301->regmap,
		SMA1301_10_SYSTEM_CTRL1, 0x1C, (sel << 2));

	return 0;
}

/* SystemCTRL3 Set */
static const char * const sma1301_input_gain_text[] = {
	"0dB", "-6dB", "-12dB", "-Infinity"};

static const struct soc_enum sma1301_input_gain_enum =
SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(sma1301_input_gain_text),
		sma1301_input_gain_text);

static int sma1301_input_gain_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sma1301_priv *sma1301 = snd_soc_codec_get_drvdata(codec);
	int val;

	regmap_read(sma1301->regmap, SMA1301_12_SYSTEM_CTRL3, &val);
	ucontrol->value.integer.value[0] = ((val & 0xC0) >> 6);

	return 0;
}

static int sma1301_input_gain_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sma1301_priv *sma1301 = snd_soc_codec_get_drvdata(codec);
	int sel = (int)ucontrol->value.integer.value[0];

	regmap_update_bits(sma1301->regmap,
		SMA1301_12_SYSTEM_CTRL3, 0xC0, (sel << 6));

	return 0;
}

/* SystemCTRL3 Set */
static const char * const sma1301_input_r_gain_text[] = {
	"0dB", "-6dB", "-12dB", "-Infinity"};

static const struct soc_enum sma1301_input_r_gain_enum =
SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(sma1301_input_r_gain_text),
		sma1301_input_r_gain_text);

static int sma1301_input_r_gain_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sma1301_priv *sma1301 = snd_soc_codec_get_drvdata(codec);
	int val;

	regmap_read(sma1301->regmap, SMA1301_12_SYSTEM_CTRL3, &val);
	ucontrol->value.integer.value[0] = ((val & 0x30) >> 4);

	return 0;
}

static int sma1301_input_r_gain_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sma1301_priv *sma1301 = snd_soc_codec_get_drvdata(codec);
	int sel = (int)ucontrol->value.integer.value[0];

	regmap_update_bits(sma1301->regmap,
		SMA1301_12_SYSTEM_CTRL3, 0x30, (sel << 4));

	return 0;
}

/* Modulator Set */
static const char * const sma1301_spk_hysfb_text[] = {
	"625kHz", "414kHz", "297kHz", "226kHz"};

static const struct soc_enum sma1301_spk_hysfb_enum =
SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(sma1301_spk_hysfb_text), sma1301_spk_hysfb_text);

static int sma1301_spk_hysfb_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sma1301_priv *sma1301 = snd_soc_codec_get_drvdata(codec);
	int val;

	regmap_read(sma1301->regmap, SMA1301_14_MODULATOR, &val);
	ucontrol->value.integer.value[0] = ((val & 0xC0) >> 6);

	return 0;
}

static int sma1301_spk_hysfb_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sma1301_priv *sma1301 = snd_soc_codec_get_drvdata(codec);
	int sel = (int)ucontrol->value.integer.value[0];

	regmap_update_bits(sma1301->regmap,
		SMA1301_14_MODULATOR, 0xC0, (sel << 6));

	return 0;
}

/* PWM LR Delay Set */
static const char * const sma1301_lr_delay_text[] = {
	"00", "01", "10", "11"};

static const struct soc_enum sma1301_lr_delay_enum =
SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(sma1301_lr_delay_text), sma1301_lr_delay_text);

static int sma1301_lr_delay_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sma1301_priv *sma1301 = snd_soc_codec_get_drvdata(codec);
	int val;

	regmap_read(sma1301->regmap, SMA1301_36_PROTECTION, &val);
	ucontrol->value.integer.value[0] = ((val & 0x60) >> 5);

	return 0;
}

static int sma1301_lr_delay_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sma1301_priv *sma1301 = snd_soc_codec_get_drvdata(codec);
	int sel = (int)ucontrol->value.integer.value[0];

	regmap_update_bits(sma1301->regmap,
		SMA1301_36_PROTECTION, 0x60, (sel << 5));

	return 0;
}

/* OTP MODE Set */
static const char * const sma1301_otp_mode_text[] = {
	"Disable", "MODE1", "MODE2", "MODE3"};

static const struct soc_enum sma1301_otp_mode_enum =
SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(sma1301_otp_mode_text), sma1301_otp_mode_text);

static int sma1301_otp_mode_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sma1301_priv *sma1301 = snd_soc_codec_get_drvdata(codec);
	int val;

	regmap_read(sma1301->regmap, SMA1301_36_PROTECTION, &val);
	ucontrol->value.integer.value[0] = (val & 0x03);

	return 0;
}

static int sma1301_otp_mode_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sma1301_priv *sma1301 = snd_soc_codec_get_drvdata(codec);
	int sel = (int)ucontrol->value.integer.value[0];

	regmap_update_bits(sma1301->regmap,
		SMA1301_36_PROTECTION, 0x03, sel);

	return 0;
}

/* Clock System Set */
static const char * const sma1301_clk_system_text[] = {
	"Reserved", "Reserved", "Reserved",
	"External clock 19.2MHz", "External clock 24.576MHz",
	"Reserved", "Reserved", "Reserved"};

static const struct soc_enum sma1301_clk_system_enum =
SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(sma1301_clk_system_text),
		sma1301_clk_system_text);

static int sma1301_clk_system_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sma1301_priv *sma1301 = snd_soc_codec_get_drvdata(codec);
	int val;

	regmap_read(sma1301->regmap, SMA1301_00_SYSTEM_CTRL, &val);
	ucontrol->value.integer.value[0] = ((val & 0xE0) >> 5);

	return 0;
}

static int sma1301_clk_system_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sma1301_priv *sma1301 = snd_soc_codec_get_drvdata(codec);
	int sel = (int)ucontrol->value.integer.value[0];

	regmap_update_bits(sma1301->regmap,
		SMA1301_00_SYSTEM_CTRL, 0xE0, (sel << 5));

	return 0;
}

/* ClassG control Set */
static const char * const sma1301_release_time_text[] = {
	"48kHz-20ms,96kHz-10ms ", "48kHz-40ms,96kHz-20ms",
	"48kHz-60ms,96kHz-30ms", "48kHz-80ms,96kHz-40ms",
	"48kHz-100ms,96kHz-50ms", "48kHz-120ms,96kHz-60ms",
	"48kHz-140ms,96kHz-70ms", "48kHz-160ms,96kHz-80ms",
	"48kHz-180ms,96kHz-90ms", "48kHz-200ms,96kHz-100ms",
	"48kHz-220ms,96kHz-110ms", "48kHz-240ms,96kHz-120ms",
	"48kHz-260ms,96kHz-130ms", "48kHz-280ms,96kHz-140ms",
	"48kHz-300kms,96Hz-150ms", "48kHz-320ms,96kHz-160ms"};

static const struct soc_enum sma1301_release_time_enum =
SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(sma1301_release_time_text),
		sma1301_release_time_text);

static int sma1301_release_time_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sma1301_priv *sma1301 = snd_soc_codec_get_drvdata(codec);
	int val;

	regmap_read(sma1301->regmap, SMA1301_91_CLASS_G_CTRL, &val);
	ucontrol->value.integer.value[0] = (val & 0x0F);

	return 0;
}

static int sma1301_release_time_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sma1301_priv *sma1301 = snd_soc_codec_get_drvdata(codec);
	int sel = (int)ucontrol->value.integer.value[0];

	regmap_update_bits(sma1301->regmap,
		SMA1301_91_CLASS_G_CTRL, 0x0F, sel);

	return 0;
}

/* ClassG control Set */
static const char * const sma1301_attack_lvl_text[] = {
	"Disabled(LOW_PW='L')", "0.03125FS", "0.0625FS", "0.09375FS",
	"0.125FS", "0.15625FS", "0.1875FS", "0.21875FS", "0.25FS",
	"0.28125FS", "0.3125FS", "0.34375FS", "0.375FS", "0.40625FS",
	"0.4375FS", "Disabled(LOW_PW='H')"};

static const struct soc_enum sma1301_attack_lvl_enum =
SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(sma1301_attack_lvl_text),
		sma1301_attack_lvl_text);

static int sma1301_attack_lvl_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sma1301_priv *sma1301 = snd_soc_codec_get_drvdata(codec);
	int val;

	regmap_read(sma1301->regmap, SMA1301_91_CLASS_G_CTRL, &val);
	ucontrol->value.integer.value[0] = ((val & 0xF0) >> 4);

	return 0;
}

static int sma1301_attack_lvl_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sma1301_priv *sma1301 = snd_soc_codec_get_drvdata(codec);
	int sel = (int)ucontrol->value.integer.value[0];

	regmap_update_bits(sma1301->regmap,
		SMA1301_91_CLASS_G_CTRL, 0xF0, (sel << 4));

	return 0;
}

/* FDPEC control Set */
static const char * const sma1301_flt_vdd_gain_text[] = {
	"2.4V", "2.45V", "2.5V", "2.55V", "2.6V",
	"2.65V", "2.7V", "2.75V", "2.8V", "2.85V",
	"2.9V", "2.95V", "3.0V", "3.05V", "3.1V", "3.15V"};

static const struct soc_enum sma1301_flt_vdd_gain_enum =
SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(sma1301_flt_vdd_gain_text),
		sma1301_flt_vdd_gain_text);

static int sma1301_flt_vdd_gain_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sma1301_priv *sma1301 = snd_soc_codec_get_drvdata(codec);
	int val;

	regmap_read(sma1301->regmap, SMA1301_92_FDPEC_CTRL, &val);
	ucontrol->value.integer.value[0] = ((val & 0xF0) >> 4);

	return 0;
}

static int sma1301_flt_vdd_gain_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sma1301_priv *sma1301 = snd_soc_codec_get_drvdata(codec);
	int sel = (int)ucontrol->value.integer.value[0];

	regmap_update_bits(sma1301->regmap,
		SMA1301_92_FDPEC_CTRL, 0xF0, (sel << 4));

	return 0;
}

/* Boost control1 Set */
static const char * const sma1301_trm_osc_text[] = {
	"1.4MHz", "1.6MHz", "1.8MHz", "2.0MHz",
	"2.2MHz", "2.4MHz", "2.6MHz", "2.8MHz"};

static const struct soc_enum sma1301_trm_osc_enum =
SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(sma1301_trm_osc_text), sma1301_trm_osc_text);

static int sma1301_trm_osc_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sma1301_priv *sma1301 = snd_soc_codec_get_drvdata(codec);
	int val;

	regmap_read(sma1301->regmap, SMA1301_94_BOOST_CTRL1, &val);
	ucontrol->value.integer.value[0] = ((val & 0x70) >> 4);

	return 0;
}

static int sma1301_trm_osc_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sma1301_priv *sma1301 = snd_soc_codec_get_drvdata(codec);
	int sel = (int)ucontrol->value.integer.value[0];

	regmap_update_bits(sma1301->regmap,
		SMA1301_94_BOOST_CTRL1, 0x70, (sel << 4));

	return 0;
}

/* Boost control1 Set */
static const char * const sma1301_trm_rmp_text[] = {
	"0.49A/us", "0.98A/us", "1.47A/us", "1.96A/us",
	"2.45A/us", "2.94A/us", "3.43A/us", "3.92A/us"};

static const struct soc_enum sma1301_trm_rmp_enum =
SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(sma1301_trm_rmp_text), sma1301_trm_rmp_text);

static int sma1301_trm_rmp_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sma1301_priv *sma1301 = snd_soc_codec_get_drvdata(codec);
	int val;

	regmap_read(sma1301->regmap, SMA1301_94_BOOST_CTRL1, &val);
	ucontrol->value.integer.value[0] = (val & 0x07);

	return 0;
}

static int sma1301_trm_rmp_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sma1301_priv *sma1301 = snd_soc_codec_get_drvdata(codec);
	int sel = (int)ucontrol->value.integer.value[0];

	regmap_update_bits(sma1301->regmap,
		SMA1301_94_BOOST_CTRL1, 0x07, sel);

	return 0;
}

/* Boost control2 Set */
static const char * const sma1301_trm_ocl_text[] = {
	"2.06A", "2.48A", "2.89A", "3.30A",
	"3.71A", "4.13A", "4.54A", "4.95A"};

static const struct soc_enum sma1301_trm_ocl_enum =
SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(sma1301_trm_ocl_text), sma1301_trm_ocl_text);

static int sma1301_trm_ocl_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sma1301_priv *sma1301 = snd_soc_codec_get_drvdata(codec);
	int val;

	regmap_read(sma1301->regmap, SMA1301_95_BOOST_CTRL2, &val);
	ucontrol->value.integer.value[0] = ((val & 0x70) >> 4);

	return 0;
}

static int sma1301_trm_ocl_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sma1301_priv *sma1301 = snd_soc_codec_get_drvdata(codec);
	int sel = (int)ucontrol->value.integer.value[0];

	regmap_update_bits(sma1301->regmap,
		SMA1301_95_BOOST_CTRL2, 0x70, (sel << 4));

	return 0;
}

/* Boost control2 Set */
static const char * const sma1301_trm_comp_text[] = {
	"PI compensation & 50pF I-gain & 1.5Mohm P-gain",
	"PI compensation & 50pF I-gain & 1.0Mohm P-gain",
	"PI compensation & 50pF I-gain & 0.75Mohm P-gain",
	"PI compensation & 50pF I-gain & 0.25ohm P-gain",
	"PI compensation & 80pF I-gain & 1.5Mohm P-gain",
	"PI compensation & 80pF I-gain & 1.0Mohm P-gain",
	"PI compensation & 80pF I-gain & 0.75Mohm P-gain",
	"PI compensation & 80pF I-gain & 0.25ohm P-gain",
	"Type-2 compensation & 50pF I-gain & 1.5Mohm P-gain",
	"Type-2 compensation & 50pF I-gain & 1.0Mohm P-gain",
	"Type-2 compensation & 50pF I-gain & 0.75Mohm P-gain",
	"Type-2 compensation & 50pF I-gain & 0.25ohm P-gain",
	"Type-2 compensation & 80pF I-gain & 1.5Mohm P-gain",
	"Type-2 compensation & 80pF I-gain & 1.0Mohm P-gain",
	"Type-2 compensation & 80pF I-gain & 0.75Mohm P-gain",
	"Type-2 compensation & 80pF I-gain & 0.25ohm P-gain"};

static const struct soc_enum sma1301_trm_comp_enum =
SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(sma1301_trm_comp_text), sma1301_trm_comp_text);

static int sma1301_trm_comp_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sma1301_priv *sma1301 = snd_soc_codec_get_drvdata(codec);
	int val;

	regmap_read(sma1301->regmap, SMA1301_95_BOOST_CTRL2, &val);
	ucontrol->value.integer.value[0] = (val & 0x0F);

	return 0;
}

static int sma1301_trm_comp_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sma1301_priv *sma1301 = snd_soc_codec_get_drvdata(codec);
	int sel = (int)ucontrol->value.integer.value[0];

	regmap_update_bits(sma1301->regmap,
		SMA1301_95_BOOST_CTRL2, 0x0F, sel);

	return 0;
}

/* Boost control3 Set */
static const char * const sma1301_trm_dt_text[] = {
	"25ns", "18ns", "12ns", "10ns", "4.8ns", "4.5ns",
	"4.2ns", "4.0ns", "1.7ns", "1.7ns", "1.7ns", "1.7ns",
	"1.5ns", "1.5ns", "1.5ns", "1.5ns"};

static const struct soc_enum sma1301_trm_dt_enum =
SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(sma1301_trm_dt_text), sma1301_trm_dt_text);

static int sma1301_trm_dt_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sma1301_priv *sma1301 = snd_soc_codec_get_drvdata(codec);
	int val;

	regmap_read(sma1301->regmap, SMA1301_96_BOOST_CTRL3, &val);
	ucontrol->value.integer.value[0] = ((val & 0xF0) >> 4);

	return 0;
}

static int sma1301_trm_dt_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sma1301_priv *sma1301 = snd_soc_codec_get_drvdata(codec);
	int sel = (int)ucontrol->value.integer.value[0];

	regmap_update_bits(sma1301->regmap,
		SMA1301_96_BOOST_CTRL3, 0xF0, (sel << 4));

	return 0;
}

/* Boost control3 Set */
static const char * const sma1301_trm_slw_text[] = {
	"8ns", "4ns", "2ns", "1ns"};

static const struct soc_enum sma1301_trm_slw_enum =
SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(sma1301_trm_slw_text), sma1301_trm_slw_text);

static int sma1301_trm_slw_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sma1301_priv *sma1301 = snd_soc_codec_get_drvdata(codec);
	int val;

	regmap_read(sma1301->regmap, SMA1301_96_BOOST_CTRL3, &val);
	ucontrol->value.integer.value[0] = (val & 0x03);

	return 0;
}

static int sma1301_trm_slw_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sma1301_priv *sma1301 = snd_soc_codec_get_drvdata(codec);
	int sel = (int)ucontrol->value.integer.value[0];

	regmap_update_bits(sma1301->regmap,
		SMA1301_96_BOOST_CTRL3, 0x03, sel);

	return 0;
}

/* Boost control4 Set */
static const char * const sma1301_trm_vref_text[] = {
	"1.24V", "1.23V", "1.22V", "1.21V",
	"1.20V", "1.19V", "1.18V", "1.17V"};

static const struct soc_enum sma1301_trm_vref_enum =
SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(sma1301_trm_vref_text), sma1301_trm_vref_text);

static int sma1301_trm_vref_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sma1301_priv *sma1301 = snd_soc_codec_get_drvdata(codec);
	int val;

	regmap_read(sma1301->regmap, SMA1301_97_BOOST_CTRL4, &val);
	ucontrol->value.integer.value[0] = ((val & 0xE0) >> 5);

	return 0;
}

static int sma1301_trm_vref_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sma1301_priv *sma1301 = snd_soc_codec_get_drvdata(codec);
	int sel = (int)ucontrol->value.integer.value[0];

	regmap_update_bits(sma1301->regmap,
		SMA1301_97_BOOST_CTRL4, 0xE0, (sel << 5));

	return 0;
}

/* Boost control4 Set */
static const char * const sma1301_trm_vbst_text[] = {
	"5.4V", "5.5V", "5.6V", "5.7V",
	"5.8V", "5.9V", "6.0V", "6.1V"};

static const struct soc_enum sma1301_trm_vbst_enum =
SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(sma1301_trm_vbst_text), sma1301_trm_vbst_text);

static int sma1301_trm_vbst_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sma1301_priv *sma1301 = snd_soc_codec_get_drvdata(codec);
	int val;

	regmap_read(sma1301->regmap, SMA1301_97_BOOST_CTRL4, &val);
	ucontrol->value.integer.value[0] = ((val & 0x1C) >> 2);

	return 0;
}

static int sma1301_trm_vbst_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sma1301_priv *sma1301 = snd_soc_codec_get_drvdata(codec);
	int sel = (int)ucontrol->value.integer.value[0];

	regmap_update_bits(sma1301->regmap,
		SMA1301_97_BOOST_CTRL4, 0x1C, (sel << 2));

	return 0;
}

/* Boost control4 Set */
static const char * const sma1301_trm_tmin_text[] = {
	"56ns", "59ns", "62ns", "65ns"};

static const struct soc_enum sma1301_trm_tmin_enum =
SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(sma1301_trm_tmin_text), sma1301_trm_tmin_text);

static int sma1301_trm_tmin_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sma1301_priv *sma1301 = snd_soc_codec_get_drvdata(codec);
	int val;

	regmap_read(sma1301->regmap, SMA1301_97_BOOST_CTRL4, &val);
	ucontrol->value.integer.value[0] = (val & 0x03);

	return 0;
}

static int sma1301_trm_tmin_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sma1301_priv *sma1301 = snd_soc_codec_get_drvdata(codec);
	int sel = (int)ucontrol->value.integer.value[0];

	regmap_update_bits(sma1301->regmap,
		SMA1301_97_BOOST_CTRL4, 0x03, sel);

	return 0;
}

/* TOP_MAN3 Set */
static const char * const sma1301_o_format_text[] = {
	"RJ", "LJ", "I2S", "SPI", "PCM short",
	"PCM long", "Reserved", "Reserved"};

static const struct soc_enum sma1301_o_format_enum =
SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(sma1301_o_format_text), sma1301_o_format_text);

static int sma1301_o_format_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sma1301_priv *sma1301 = snd_soc_codec_get_drvdata(codec);
	int val;

	regmap_read(sma1301->regmap, SMA1301_A4_TOP_MAN3, &val);
	ucontrol->value.integer.value[0] = ((val & 0xE0) >> 5);

	return 0;
}

static int sma1301_o_format_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sma1301_priv *sma1301 = snd_soc_codec_get_drvdata(codec);
	int sel = (int)ucontrol->value.integer.value[0];

	regmap_update_bits(sma1301->regmap,
		SMA1301_A4_TOP_MAN3, 0xE0, (sel << 5));

	return 0;
}

/* TOP_MAN3 Set */
static const char * const sma1301_sck_rate_text[] = {
	"64fs", "64fs", "32fs", "32fs"};

static const struct soc_enum sma1301_sck_rate_enum =
SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(sma1301_sck_rate_text), sma1301_sck_rate_text);

static int sma1301_sck_rate_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sma1301_priv *sma1301 = snd_soc_codec_get_drvdata(codec);
	int val;

	regmap_read(sma1301->regmap, SMA1301_A4_TOP_MAN3, &val);
	ucontrol->value.integer.value[0] = ((val & 0x18) >> 3);

	return 0;
}

static int sma1301_sck_rate_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sma1301_priv *sma1301 = snd_soc_codec_get_drvdata(codec);
	int sel = (int)ucontrol->value.integer.value[0];

	regmap_update_bits(sma1301->regmap,
		SMA1301_A4_TOP_MAN3, 0x18, (sel << 3));

	return 0;
}

/* EQ Mode SELECT */
static const char * const sma1301_eq_mode_text[] = {"User Defined", "Classic ",
	"Rock/Pop", "Jazz", "R&B", "Dance", "Speech", "Parametric"};

static const struct soc_enum sma1301_eq_mode_enum =
SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(sma1301_eq_mode_text), sma1301_eq_mode_text);

static int sma1301_eq_mode_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sma1301_priv *sma1301 = snd_soc_codec_get_drvdata(codec);
	int val;

	regmap_read(sma1301->regmap, SMA1301_2B_EQ_MODE, &val);
	ucontrol->value.integer.value[0] = (val & 0x07);

	return 0;
}

static int sma1301_eq_mode_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sma1301_priv *sma1301 = snd_soc_codec_get_drvdata(codec);
	int sel = (int)ucontrol->value.integer.value[0];

	regmap_update_bits(sma1301->regmap,
		SMA1301_2B_EQ_MODE, 0x07, sel);

	return 0;
}

/* common bytes ext functions */
static int bytes_ext_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol, int reg)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sma1301_priv *sma1301 = snd_soc_codec_get_drvdata(codec);
	struct soc_bytes_ext *params = (void *)kcontrol->private_value;
	unsigned int i, reg_val;
	u8 *val;

	val = (u8 *)ucontrol->value.bytes.data;
	for (i = 0; i < params->max; i++) {
		regmap_read(sma1301->regmap, reg + i, &reg_val);
		if (sizeof(reg_val) > 2)
			reg_val = cpu_to_le32(reg_val);
		else
			reg_val = cpu_to_le16(reg_val);
		memcpy(val + i, &reg_val, sizeof(u8));
	}

	return 0;
}

static int bytes_ext_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol, int reg)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sma1301_priv *sma1301 = snd_soc_codec_get_drvdata(codec);
	struct soc_bytes_ext *params = (void *)kcontrol->private_value;
	void *data;
	u8 *val;
	int i, ret;

	data = kmemdup(ucontrol->value.bytes.data,
			params->max, GFP_KERNEL | GFP_DMA);
	if (!data)
		return -ENOMEM;

	val = (u8 *)data;
	for (i = 0; i < params->max; i++) {
		ret = regmap_write(sma1301->regmap, reg + i, *(val + i));
		if (ret) {
			dev_err(codec->dev,
				"configuration fail, register: %x ret: %d\n",
				reg + i, ret);
			kfree(data);
			return ret;
		}
	}
	kfree(data);

	return 0;
}

/* EqGraphic 1~5 */
static int eqgraphic_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	return bytes_ext_get(kcontrol, ucontrol, SMA1301_2C_EQ_GRAPHIC1);
}

static int eqgraphic_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	return bytes_ext_put(kcontrol, ucontrol, SMA1301_2C_EQ_GRAPHIC1);
}

/* Modulator */
static int spk_bdelay_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	return bytes_ext_get(kcontrol, ucontrol, SMA1301_14_MODULATOR);
}

static int spk_bdelay_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	return bytes_ext_put(kcontrol, ucontrol, SMA1301_14_MODULATOR);
}

/* Slope CTRL */
static int slope_ctrl_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	return bytes_ext_get(kcontrol, ucontrol, SMA1301_37_SLOPE_CTRL);
}

static int slope_ctrl_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	return bytes_ext_put(kcontrol, ucontrol, SMA1301_37_SLOPE_CTRL);
}

/* Test 1~3, ATEST 1~2 */
static int test_mode_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	return bytes_ext_get(kcontrol, ucontrol, SMA1301_3B_TEST1);
}

static int test_mode_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	return bytes_ext_put(kcontrol, ucontrol, SMA1301_3B_TEST1);
}

/* PEQ Band1 */
static int eq_ctrl_band1_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	return bytes_ext_get(kcontrol, ucontrol, SMA1301_40_EQ_CTRL1);
}

static int eq_ctrl_band1_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	return bytes_ext_put(kcontrol, ucontrol, SMA1301_40_EQ_CTRL1);
}

/* PEQ Band2 */
static int eq_ctrl_band2_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	return bytes_ext_get(kcontrol, ucontrol, SMA1301_4F_EQ_CTRL16);
}

static int eq_ctrl_band2_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	return bytes_ext_put(kcontrol, ucontrol, SMA1301_4F_EQ_CTRL16);
}

/* PEQ Band3 */
static int eq_ctrl_band3_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	return bytes_ext_get(kcontrol, ucontrol, SMA1301_5E_EQ_CTRL31);
}

static int eq_ctrl_band3_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	return bytes_ext_put(kcontrol, ucontrol, SMA1301_5E_EQ_CTRL31);
}

/* PEQ Band4 */
static int eq_ctrl_band4_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	return bytes_ext_get(kcontrol, ucontrol, SMA1301_6D_EQ_CTRL46);
}

static int eq_ctrl_band4_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	return bytes_ext_put(kcontrol, ucontrol, SMA1301_6D_EQ_CTRL46);
}

/* PEQ Band5 */
static int eq_ctrl_band5_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	return bytes_ext_get(kcontrol, ucontrol, SMA1301_7C_EQ_CTRL61);
}

static int eq_ctrl_band5_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	return bytes_ext_put(kcontrol, ucontrol, SMA1301_7C_EQ_CTRL61);
}

/* bass boost speaker coeff */
static int bass_spk_coeff_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	return bytes_ext_get(kcontrol, ucontrol, SMA1301_15_BASS_SPK1);
}

static int bass_spk_coeff_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	return bytes_ext_put(kcontrol, ucontrol, SMA1301_15_BASS_SPK1);
}

/* DRC speaker coeff */
static int comp_lim_spk_coeff_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	return bytes_ext_get(kcontrol, ucontrol, SMA1301_23_COMPLIM1);
}

static int comp_lim_spk_coeff_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	return bytes_ext_put(kcontrol, ucontrol, SMA1301_23_COMPLIM1);
}

/* PLL setting */
static int pll_setting_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	return bytes_ext_get(kcontrol, ucontrol, SMA1301_8B_PLL_POST_N);
}

static int pll_setting_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	return bytes_ext_put(kcontrol, ucontrol, SMA1301_8B_PLL_POST_N);
}

static const struct snd_kcontrol_new sma1301_snd_controls[] = {

SOC_SINGLE("Power-up/down Switch", SMA1301_00_SYSTEM_CTRL, 0, 1, 0),
SOC_ENUM_EXT("External Clock System", sma1301_clk_system_enum,
	sma1301_clk_system_get, sma1301_clk_system_put),

SOC_SINGLE("I2S/PCM Clock mode(1:Master,2:Slave)",
		SMA1301_01_INPUT1_CTRL1, 7, 1, 0),
SOC_ENUM_EXT("I2S input format(I2S,LJ,RJ)", sma1301_input_format_enum,
	sma1301_input_format_get, sma1301_input_format_put),
SOC_SINGLE("First-channel pol for I2S(1:High,0:Low)",
		SMA1301_01_INPUT1_CTRL1, 3, 1, 0),
SOC_SINGLE("Data written on SCK edge(1:rise,0:fall)",
		SMA1301_01_INPUT1_CTRL1, 2, 1, 0),

SOC_ENUM_EXT("Input audio mode", sma1301_in_audio_mode_enum,
	sma1301_in_audio_mode_get, sma1301_in_audio_mode_put),
SOC_SINGLE("Data inversion(1:right-first,0:left-first)",
		SMA1301_02_INPUT1_CTRL2, 5, 1, 0),
SOC_SINGLE("Decoding select(1:A-law,0:u-law)",
		SMA1301_02_INPUT1_CTRL2, 4, 1, 0),
SOC_SINGLE("Companding PCM data(1:Companding,2:Linear)",
		SMA1301_02_INPUT1_CTRL2, 3, 1, 0),
SOC_SINGLE("PCM sample freq(1:16kHZ,0:8kHz)",
		SMA1301_02_INPUT1_CTRL2, 2, 1, 0),
SOC_SINGLE("PCM stero/mono sel(1:stereo,0:mono)",
		SMA1301_02_INPUT1_CTRL2, 1, 1, 0),
SOC_SINGLE("PCM data length(1:16-bit,0:8-bit)",
		SMA1301_02_INPUT1_CTRL2, 0, 1, 0),

SOC_SINGLE("SR converted bypass(1:SRC bypass,0:normal)",
		SMA1301_03_INPUT1_CTRL3, 4, 1, 0),
SOC_ENUM_EXT("Number of slots per sampling period(PCM)",
		sma1301_pcm_n_slot_enum, sma1301_pcm_n_slot_get,
		sma1301_pcm_n_slot_put),

SOC_ENUM_EXT("Position of the first sample at 8,16kHz",
		sma1301_pcm1_slot_enum, sma1301_pcm1_slot_get,
		sma1301_pcm1_slot_put),
SOC_ENUM_EXT("Position of the second sample at 16kHz",
		sma1301_pcm2_slot_enum, sma1301_pcm2_slot_get,
		sma1301_pcm2_slot_put),

SOC_SINGLE_TLV("Speaker Volume", SMA1301_0A_SPK_VOL,
		0, 167, 1, sma1301_spk_tlv),

SOC_ENUM_EXT("Volume slope", sma1301_vol_slope_enum,
	sma1301_vol_slope_get, sma1301_vol_slope_put),
SOC_ENUM_EXT("Mute slope", sma1301_mute_slope_enum,
	sma1301_mute_slope_get, sma1301_mute_slope_put),
SOC_SINGLE("Speaker Mute Switch(1:muted,0:un-muted)",
		SMA1301_0E_MUTE_VOL_CTRL, 0, 1, 0),

SOC_ENUM_EXT("Port In/Out port configuration", sma1301_port_config_enum,
	sma1301_port_config_get, sma1301_port_config_put),
SOC_ENUM_EXT("Port Output Format", sma1301_port_out_format_enum,
	sma1301_port_out_format_get, sma1301_port_out_format_put),
SOC_ENUM_EXT("Port Output Source", sma1301_port_out_sel_enum,
	sma1301_port_out_sel_get, sma1301_port_out_sel_put),

SOC_SINGLE("Speaker Terminal Swap Switch",
		SMA1301_10_SYSTEM_CTRL1, 5, 1, 0),
SOC_ENUM_EXT("Speaker Mode", sma1301_spkmode_enum,
	sma1301_spkmode_get, sma1301_spkmode_put),

SOC_SINGLE("Speaker EQ Switch", SMA1301_11_SYSTEM_CTRL2, 7, 1, 0),
SOC_SINGLE("Speaker Bass Switch", SMA1301_11_SYSTEM_CTRL2, 6, 1, 0),
SOC_SINGLE("Speaker Comp/Limiter Switch", SMA1301_11_SYSTEM_CTRL2, 5, 1, 0),
SOC_SINGLE("LR_DATA_SW Switch", SMA1301_11_SYSTEM_CTRL2, 4, 1, 0),
SOC_SINGLE("Mono Mix Switch", SMA1301_11_SYSTEM_CTRL2, 0, 1, 0),

SOC_ENUM_EXT("Input gain", sma1301_input_gain_enum,
	sma1301_input_gain_get, sma1301_input_gain_put),
SOC_ENUM_EXT("Input gain for right channel", sma1301_input_r_gain_enum,
	sma1301_input_r_gain_get, sma1301_input_r_gain_put),

SOC_ENUM_EXT("Speaker HYSFB", sma1301_spk_hysfb_enum,
	sma1301_spk_hysfb_get, sma1301_spk_hysfb_put),
SND_SOC_BYTES_EXT("Speaker BDELAY", 1, spk_bdelay_get, spk_bdelay_put),

SOC_SINGLE("HB path modulator(1:disable,0:enable)",
		SMA1301_33_FLL_CTRL, 3, 1, 1),
SOC_SINGLE("Speaker modulator sync(1:1/8,0:1/4)",
		SMA1301_33_FLL_CTRL, 2, 1, 1),

SOC_SINGLE("Edge displacement(1:disable,0:enable)",
		SMA1301_36_PROTECTION, 7, 1, 1),
SOC_ENUM_EXT("PWM LR delay", sma1301_lr_delay_enum,
		sma1301_lr_delay_get, sma1301_lr_delay_put),
SOC_SINGLE("SRC random jitter(1:disable,0:added)",
		SMA1301_36_PROTECTION, 4, 1, 1),
SOC_SINGLE("OCP spk output state(1:disable,0:enable)",
		SMA1301_36_PROTECTION, 3, 1, 1),
SOC_SINGLE("OCP mode(1:Permanent SD,0:Auto recover)",
		SMA1301_36_PROTECTION, 2, 1, 1),
SOC_ENUM_EXT("OTP MODE", sma1301_otp_mode_enum,
		sma1301_otp_mode_get, sma1301_otp_mode_put),

SND_SOC_BYTES_EXT("SlopeCTRL", 1, slope_ctrl_get, slope_ctrl_put),

SOC_SINGLE("EQ Bank", SMA1301_2B_EQ_MODE, 3, 1, 0),
SOC_ENUM_EXT("EQ Mode", sma1301_eq_mode_enum,
		sma1301_eq_mode_get, sma1301_eq_mode_put),

SOC_SINGLE("EQ1 Band1 Bypass Switch", SMA1301_2C_EQ_GRAPHIC1, 5, 1, 0),
SOC_SINGLE("EQ1 Band2 Bypass Switch", SMA1301_2D_EQ_GRAPHIC2, 5, 1, 0),
SOC_SINGLE("EQ1 Band3 Bypass Switch", SMA1301_2E_EQ_GRAPHIC3, 5, 1, 0),
SOC_SINGLE("EQ1 Band4 Bypass Switch", SMA1301_2F_EQ_GRAPHIC4, 5, 1, 0),
SOC_SINGLE("EQ1 Band5 Bypass Switch", SMA1301_30_EQ_GRAPHIC5, 5, 1, 0),
SND_SOC_BYTES_EXT("5 band equalizer-1(EqGraphic 1~5)",
		5, eqgraphic_get, eqgraphic_put),
SND_SOC_BYTES_EXT("Test mode(Test 1~3, ATEST 1~2)",
		5, test_mode_get, test_mode_put),

SND_SOC_BYTES_EXT("EQ Ctrl Band1", 15,
		eq_ctrl_band1_get, eq_ctrl_band1_put),
SND_SOC_BYTES_EXT("EQ Ctrl Band2", 15,
		eq_ctrl_band2_get, eq_ctrl_band2_put),
SND_SOC_BYTES_EXT("EQ Ctrl Band3", 15,
		eq_ctrl_band3_get, eq_ctrl_band3_put),
SND_SOC_BYTES_EXT("EQ Ctrl Band4", 15,
		eq_ctrl_band4_get, eq_ctrl_band4_put),
SND_SOC_BYTES_EXT("EQ Ctrl Band5", 15,
		eq_ctrl_band5_get, eq_ctrl_band5_put),

SND_SOC_BYTES_EXT("Bass Boost SPK Coeff", 7,
	bass_spk_coeff_get, bass_spk_coeff_put),
SND_SOC_BYTES_EXT("DRC SPK Coeff", 4,
	comp_lim_spk_coeff_get, comp_lim_spk_coeff_put),

SOC_ENUM_EXT("Attack level control", sma1301_attack_lvl_enum,
	sma1301_attack_lvl_get, sma1301_attack_lvl_put),
SOC_ENUM_EXT("Release time control", sma1301_release_time_enum,
	sma1301_release_time_get, sma1301_release_time_put),

SOC_ENUM_EXT("Filtered VDD gain control", sma1301_flt_vdd_gain_enum,
	sma1301_flt_vdd_gain_get, sma1301_flt_vdd_gain_put),
SOC_SINGLE("Fast charge(1:Disable,0:Enable)",
		SMA1301_92_FDPEC_CTRL, 2, 1, 0),

SOC_ENUM_EXT("Trimming of switching frequency", sma1301_trm_osc_enum,
	sma1301_trm_osc_get, sma1301_trm_osc_put),
SOC_ENUM_EXT("Trimming of ramp compensation", sma1301_trm_rmp_enum,
	sma1301_trm_rmp_get, sma1301_trm_rmp_put),
SOC_ENUM_EXT("Trimming of over current limit", sma1301_trm_ocl_enum,
	sma1301_trm_ocl_get, sma1301_trm_ocl_put),
SOC_ENUM_EXT("Trimming of loop compensation", sma1301_trm_comp_enum,
	sma1301_trm_comp_get, sma1301_trm_comp_put),
SOC_ENUM_EXT("Trimming of driver deadtime", sma1301_trm_dt_enum,
	sma1301_trm_dt_get, sma1301_trm_dt_put),
SOC_ENUM_EXT("Trimming of switching slew", sma1301_trm_slw_enum,
	sma1301_trm_slw_get, sma1301_trm_slw_put),
SOC_ENUM_EXT("Trimming of reference voltage", sma1301_trm_vref_enum,
	sma1301_trm_vref_get, sma1301_trm_vref_put),
SOC_ENUM_EXT("Trimming of boost voltage", sma1301_trm_vbst_enum,
	sma1301_trm_vbst_get, sma1301_trm_vbst_put),
SOC_ENUM_EXT("Trimming of minimum on-time", sma1301_trm_tmin_enum,
	sma1301_trm_tmin_get, sma1301_trm_tmin_put),

SOC_SINGLE("PLL Lock Skip Mode(1:disable,0:enable)",
		SMA1301_A2_TOP_MAN1, 7, 1, 0),
SOC_SINGLE("PLL Power Down(1:PLL PD en,0:PLL operation)",
		SMA1301_A2_TOP_MAN1, 6, 1, 1),
SOC_SINGLE("MCLK Selection(1:Ext CLK,0:PLL CLK)",
		SMA1301_A2_TOP_MAN1, 5, 1, 1),
SOC_SINGLE("PLL Reference Clock1(1:Int OSC,0:Ext CLK)",
		SMA1301_A2_TOP_MAN1, 4, 1, 0),
SOC_SINGLE("PLL Reference Clock2(1:SCK,0:PLL_REF_CLK1)",
		SMA1301_A2_TOP_MAN1, 3, 1, 1),
SOC_SINGLE("DAC Down Conversion(1:Down Con,0:Normal)",
		SMA1301_A2_TOP_MAN1, 2, 1, 0),
SOC_SINGLE("SDO Pad Output Control(1:LRCK L,0:LRCK H)",
		SMA1301_A2_TOP_MAN1, 1, 1, 0),
SOC_SINGLE("Master Mode Enable(1:Master,0:Slave)",
		SMA1301_A2_TOP_MAN1, 0, 1, 0),

SOC_SINGLE("Monitoring at SDO(1:OSC,0:PLL)",
		SMA1301_A3_TOP_MAN2, 7, 1, 0),
SOC_SINGLE("Test clock output en(1:Clock out,0:Normal)",
		SMA1301_A3_TOP_MAN2, 6, 1, 0),
SOC_SINGLE("PLL SDM PD(1:SDM off,0:SDM on)",
		SMA1301_A3_TOP_MAN2, 5, 1, 1),
SOC_SINGLE("Clock monitoring time(1:4usec,0:2usec)",
		SMA1301_A3_TOP_MAN2, 4, 1, 0),
SOC_SINGLE("SDO output(1:High-Z,0:Normal output)",
		SMA1301_A3_TOP_MAN2, 3, 1, 1),
SOC_SINGLE("SDO output only(1:output only,0:normal)",
		SMA1301_A3_TOP_MAN2, 2, 1, 0),
SOC_SINGLE("Clock Monitoring(1:Not,0:Monitoring)",
		SMA1301_A3_TOP_MAN2, 1, 1, 0),
SOC_SINGLE("OSC PD(1:Power down,0:Normal operation)",
		SMA1301_A3_TOP_MAN2, 0, 1, 0),

SOC_ENUM_EXT("Top Manager Output Format", sma1301_o_format_enum,
	sma1301_o_format_get, sma1301_o_format_put),
SOC_ENUM_EXT("Top Manager SCK rate", sma1301_sck_rate_enum,
	sma1301_sck_rate_get, sma1301_sck_rate_put),
SOC_SINGLE("Top Manager LRCK Pol(1:H valid,0:L valid)",
		SMA1301_A4_TOP_MAN3, 0, 1, 0),

SND_SOC_BYTES_EXT("PLL Setting", 5, pll_setting_get, pll_setting_put),
};

/* MIC IN detection function and MIC bias enable function must be implemented
 * 0xFB MIC in function not operating correctly,
 * So, GPIO detection function must be implemented by H/W.
 */

static int sma1301_dac_event(struct snd_soc_dapm_widget *w,
			struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct sma1301_priv *sma1301 = snd_soc_codec_get_drvdata(codec);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		dev_info(codec->dev, "%s : Speaker mode Config for PRE_PMU\n"
				, __func__);
		if (sma1301->stereo_select == true) {
		/* SPK Mode (Stereo) */
		regmap_update_bits(sma1301->regmap, SMA1301_10_SYSTEM_CTRL1,
				SPK_MODE_MASK, SPK_STEREO);
		/* MONO MIX Off */
		regmap_update_bits(sma1301->regmap, SMA1301_11_SYSTEM_CTRL2,
				MONOMIX_MASK, MONOMIX_OFF);
		} else {
		/* SPK Mode (Mono) */
		regmap_update_bits(sma1301->regmap, SMA1301_10_SYSTEM_CTRL1,
				SPK_MODE_MASK, SPK_MONO);
		/* MONO MIX ON */
		regmap_update_bits(sma1301->regmap, SMA1301_11_SYSTEM_CTRL2,
				MONOMIX_MASK, MONOMIX_ON);
		}
		break;

	case SND_SOC_DAPM_PRE_PMD:
		/* SPK_MODE should SPK_OFF */
		dev_info(codec->dev, "%s : Speaker mode config for PRE_PMD\n"
				, __func__);
		regmap_update_bits(sma1301->regmap, SMA1301_10_SYSTEM_CTRL1,
				SPK_MODE_MASK, SPK_OFF);
		break;
	}
	return 0;
}

static const struct snd_soc_dapm_widget sma1301_dapm_widgets[] = {
SND_SOC_DAPM_DAC_E("DAC", "Playback", SND_SOC_NOPM, 0, 0, sma1301_dac_event,
				SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),
SND_SOC_DAPM_OUTPUT("SPK"),
SND_SOC_DAPM_INPUT("IN"),
};

static const struct snd_soc_dapm_route sma1301_audio_map[] = {
/* sink, control, source */
{"SPK", NULL, "DAC"},
{"IN", NULL, "Capture"},
};

static int sma1301_pll_clock_set(struct snd_soc_codec *codec,
		u32 post_n, u32 n, u32 f1, u32 f2, u32 f3)
{
	struct sma1301_priv *sma1301 = snd_soc_codec_get_drvdata(codec);

	regmap_write(sma1301->regmap, SMA1301_8B_PLL_POST_N, post_n);
	regmap_write(sma1301->regmap, SMA1301_8C_PLL_N, n);
	regmap_write(sma1301->regmap, SMA1301_8D_PLL_F1, f1);
	regmap_write(sma1301->regmap, SMA1301_8E_PLL_F2, f2);
	regmap_write(sma1301->regmap, SMA1301_8F_PLL_F3, f3);
	return 0;
}

static int sma1301_dai_hw_params_amp(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params, struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct sma1301_priv *sma1301 = snd_soc_codec_get_drvdata(codec);

	dev_info(codec->dev, "%s : mode %d, params_rate=%d Hz : params_width=%d bit\n",
		 __func__, sma1301->i2s_mode, params_rate(params), params_width(params));

	/* Right justified setting according to transmitted bit */
	if (sma1301->i2s_mode == SND_SOC_DAIFMT_RIGHT_J) {
		switch (params_width(params)) {
		case 16:
		regmap_update_bits(sma1301->regmap, SMA1301_01_INPUT1_CTRL1,
				I2S_MODE_MASK, RJ_16BIT);
		break;
		case 18:
		regmap_update_bits(sma1301->regmap, SMA1301_01_INPUT1_CTRL1,
				I2S_MODE_MASK, RJ_18BIT);
		break;
		case 20:
		regmap_update_bits(sma1301->regmap, SMA1301_01_INPUT1_CTRL1,
				I2S_MODE_MASK, RJ_20BIT);
		break;
		case 24:
		regmap_update_bits(sma1301->regmap, SMA1301_01_INPUT1_CTRL1,
				I2S_MODE_MASK, RJ_24BIT);
		break;
		}
	}

	/* Check DAC down conversion for 192kHz sampling rate */
	if (params_rate(params) == 192000) {
		regmap_update_bits(sma1301->regmap, SMA1301_A2_TOP_MAN1,
				DAC_DN_CONV_MASK, DAC_DN_CONV_ENABLE);
		regmap_update_bits(sma1301->regmap, SMA1301_01_INPUT1_CTRL1,
				LEFTPOL_MASK, HIGH_FIRST_CH);

	} else {
		regmap_update_bits(sma1301->regmap, SMA1301_A2_TOP_MAN1,
				DAC_DN_CONV_MASK, DAC_DN_CONV_DISABLE);
		regmap_update_bits(sma1301->regmap, SMA1301_01_INPUT1_CTRL1,
				LEFTPOL_MASK, LOW_FIRST_CH);
	}

	/* The sigma delta modulation setting for
	 * using the fractional divider in the PLL clock
	 */
	if (params_format(params) == SNDRV_PCM_FORMAT_S24_LE) {
		regmap_update_bits(sma1301->regmap, SMA1301_A3_TOP_MAN2,
					PLL_SDM_PD_MASK, SDM_ON);
	} else {
		regmap_update_bits(sma1301->regmap, SMA1301_A3_TOP_MAN2,
					PLL_SDM_PD_MASK, SDM_OFF);
	}

	/* PLL clock setting according to sample rate and bit */
	if (sma1301->sck_pll_enable) {
	regmap_update_bits(sma1301->regmap, SMA1301_00_SYSTEM_CTRL,
					POWER_MASK, POWER_OFF);
	switch (params_rate(params)) {
	case 8000:
	case 12000:
	case 16000:
	case 24000:
	case 32000:
	if (params_format(params) == SNDRV_PCM_FORMAT_S16_LE) {
		sma1301_pll_clock_set(codec, 0x07, 0xA8,
				0x00, 0x00, 0x03);
	} else if (params_format(params) == SNDRV_PCM_FORMAT_S24_LE) {
		sma1301_pll_clock_set(codec, 0x06, 0xC0,
				0x00, 0x00, 0x02);
	} else if (params_format(params) == SNDRV_PCM_FORMAT_S32_LE) {
		sma1301_pll_clock_set(codec, 0x07, 0xA8,
				0x00, 0x00, 0x03);
	}
	break;
	case 44100:
	if (params_format(params) == SNDRV_PCM_FORMAT_S16_LE) {
		sma1301_pll_clock_set(codec, 0x07, 0xF3,
				0xCF, 0x3C, 0xF3);
	} else if (params_format(params) == SNDRV_PCM_FORMAT_S24_LE) {
		sma1301_pll_clock_set(codec, 0x07, 0xA2,
				0x8A, 0x28, 0xA3);
	} else if (params_format(params) == SNDRV_PCM_FORMAT_S32_LE) {
		sma1301_pll_clock_set(codec, 0x07, 0xF3,
				0xCF, 0x3C, 0xF7);
	}
	break;
	case 48000:
	if (params_format(params) == SNDRV_PCM_FORMAT_S16_LE) {
		sma1301_pll_clock_set(codec, 0x06, 0xC0,
				0x00, 0x00, 0x02);
	} else if (params_format(params) == SNDRV_PCM_FORMAT_S24_LE) {
		sma1301_pll_clock_set(codec, 0x07, 0x95,
				0x55, 0x55, 0x53);
	} else if (params_format(params) == SNDRV_PCM_FORMAT_S32_LE) {
		sma1301_pll_clock_set(codec, 0x04, 0x40,
				0x00, 0x00, 0x00);
	}
	break;
	case 96000:
	if (params_format(params) == SNDRV_PCM_FORMAT_S16_LE) {
		sma1301_pll_clock_set(codec, 0x04, 0x40,
				0x00, 0x00, 0x00);
	} else if (params_format(params) == SNDRV_PCM_FORMAT_S24_LE) {
		sma1301_pll_clock_set(codec, 0x07, 0x95,
				0x55, 0x55, 0x57);
	} else if (params_format(params) == SNDRV_PCM_FORMAT_S32_LE) {
		sma1301_pll_clock_set(codec, 0x04, 0x40,
				0x00, 0x00, 0x04);
	}
	break;
	case 192000:
	if (params_format(params) == SNDRV_PCM_FORMAT_S16_LE) {
		sma1301_pll_clock_set(codec, 0x04, 0x20,
				0x00, 0x00, 0x00);
	} else if (params_format(params) == SNDRV_PCM_FORMAT_S24_LE) {
		sma1301_pll_clock_set(codec, 0x07, 0x95,
				0x55, 0x55, 0x5B);
	} else if (params_format(params) == SNDRV_PCM_FORMAT_S32_LE) {
		sma1301_pll_clock_set(codec, 0x04, 0x10,
				0x00, 0x00, 0x00);
	}
	break;
	default:
		sma1301_pll_clock_set(codec, 0x06, 0xC0,
				0x00, 0x00, 0x02);
	}

	regmap_update_bits(sma1301->regmap,
			SMA1301_00_SYSTEM_CTRL, POWER_MASK, POWER_ON);
	}
	return 0;
}

static int sma1301_dai_set_sysclk_amp(struct snd_soc_dai *dai,
				int clk_id, unsigned int freq, int dir)
{
	struct snd_soc_codec *codec = dai->codec;
	struct sma1301_priv *sma1301 = snd_soc_codec_get_drvdata(codec);

	dev_info(codec->dev, "%s\n", __func__);
	return 0;
	/* Requested clock frequency is already setup */
	if (freq == sma1301->sysclk)
		return 0;

	switch (clk_id) {
	case SMA1301_EXTERNAL_CLOCK_19_2:
		regmap_update_bits(sma1301->regmap, SMA1301_00_SYSTEM_CTRL,
					CLKSYSTEM_MASK, EXT_19_2);
		break;

	case SMA1301_EXTERNAL_CLOCL_24_576:
		regmap_update_bits(sma1301->regmap, SMA1301_00_SYSTEM_CTRL,
					CLKSYSTEM_MASK, EXT_24_576);
		break;

	default:
		return -EINVAL;
	}

	/* Setup clocks for slave mode, and using the PLL */
	sma1301->sysclk = freq;
	return 0;
}

static int sma1301_dai_digital_mute(struct snd_soc_dai *codec_dai, int mute)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	struct sma1301_priv *sma1301 = snd_soc_codec_get_drvdata(codec);

	dev_info(codec->dev, "%s : %s\n", __func__, mute ? "MUTE" : "UNMUTE");

	if (mute) {
		regmap_update_bits(sma1301->regmap, SMA1301_0E_MUTE_VOL_CTRL,
					SPK_MUTE_MASK, SPK_MUTE);
	} else {
		regmap_update_bits(sma1301->regmap, SMA1301_0E_MUTE_VOL_CTRL,
					SPK_MUTE_MASK, SPK_UNMUTE);
	}

	return 0;
}

static int sma1301_dai_set_fmt_amp(struct snd_soc_dai *codec_dai,
					unsigned int fmt)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	struct sma1301_priv *sma1301 = snd_soc_codec_get_drvdata(codec);

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {

	case SND_SOC_DAIFMT_CBS_CFS:
		dev_info(codec->dev, "%s : %s\n", __func__, "I2S slave mode");
		/* I2S/PCM clock mode - slave mode */
		regmap_update_bits(sma1301->regmap, SMA1301_01_INPUT1_CTRL1,
					MASTER_SLAVE_MASK, SLAVE_MODE);
		regmap_update_bits(sma1301->regmap, SMA1301_A2_TOP_MAN1,
					MAS_IO_MASK, MAS_IO_SLAVE);
		break;

	case SND_SOC_DAIFMT_CBM_CFM:
		dev_info(codec->dev, "%s : %s\n", __func__, "I2S master mode");
		/* I2S/PCM clock mode - master mode */
		regmap_update_bits(sma1301->regmap, SMA1301_01_INPUT1_CTRL1,
					MASTER_SLAVE_MASK, MASTER_MODE);
		regmap_update_bits(sma1301->regmap, SMA1301_A2_TOP_MAN1,
					MAS_IO_MASK, MAS_IO_MASTER);
		break;

	default:
		dev_err(codec->dev, "Unsupported MASTER/SLAVE : 0x%x\n", fmt);
		return -EINVAL;
	}

	sma1301->i2s_mode = fmt & SND_SOC_DAIFMT_FORMAT_MASK;

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {

	case SND_SOC_DAIFMT_I2S:
		dev_info(codec->dev, "%s : %s\n",
			__func__, "I2S standard mode");
		regmap_update_bits(sma1301->regmap, SMA1301_01_INPUT1_CTRL1,
					I2S_MODE_MASK, STANDARD_I2S);
		break;

	case SND_SOC_DAIFMT_RIGHT_J:
		dev_info(codec->dev, "%s : %s\n",
			__func__, "I2S Right justified 16 bits mode");
		regmap_update_bits(sma1301->regmap, SMA1301_01_INPUT1_CTRL1,
					I2S_MODE_MASK, RJ_16BIT);
		break;

	case SND_SOC_DAIFMT_LEFT_J:
		dev_info(codec->dev, "%s : %s\n",
			__func__, "I2S Left justified 16 bits mode");
		regmap_update_bits(sma1301->regmap, SMA1301_01_INPUT1_CTRL1,
					I2S_MODE_MASK, LJ);
		break;

	default:
		dev_err(codec->dev, "Unsupported I2S FORMAT : 0x%x\n", fmt);
		return -EINVAL;
	}

	return 0;
}

static const struct snd_soc_dai_ops sma1301_dai_ops_amp = {
	.set_sysclk = sma1301_dai_set_sysclk_amp,
	.set_fmt = sma1301_dai_set_fmt_amp,
	.hw_params = sma1301_dai_hw_params_amp,
	.digital_mute = sma1301_dai_digital_mute,
};

#define SMA1301_RATES SNDRV_PCM_RATE_8000_192000
#define SMA1301_FORMATS (SNDRV_PCM_FMTBIT_S16_LE| \
		SNDRV_PCM_FMTBIT_S24_LE|SNDRV_PCM_FMTBIT_S32_LE)

static struct snd_soc_dai_driver sma1301_dai = {
	.name = "sma1301-voice",
	//.id = 0,
	.playback = {
	.stream_name = "Playback",
	.channels_min = 1,
	.channels_max = 2,
	.rates = SMA1301_RATES,
	.formats = SMA1301_FORMATS,
	},
	.capture = {
	.stream_name = "Capture",
	.channels_min = 1,
	.channels_max = 2,
	.rates = SMA1301_RATES,
	.formats = SMA1301_FORMATS,
	},
	.ops = &sma1301_dai_ops_amp,
	.symmetric_rates = 1,
};

static int sma1301_set_bias_level(struct snd_soc_codec *codec,
				enum snd_soc_bias_level level)
{
	struct sma1301_priv *sma1301 = snd_soc_codec_get_drvdata(codec);

	switch (level) {
	case SND_SOC_BIAS_ON:

		dev_info(codec->dev, "%s\n", "SND_SOC_BIAS_ON");

		break;

	case SND_SOC_BIAS_PREPARE:

		dev_info(codec->dev, "%s\n", "SND_SOC_BIAS_PREPARE");
		regmap_update_bits(sma1301->regmap, SMA1301_01_INPUT1_CTRL1,
					MASTER_SLAVE_MASK, SLAVE_MODE);
		regmap_update_bits(sma1301->regmap, SMA1301_A2_TOP_MAN1,
					MAS_IO_MASK, MAS_IO_SLAVE);
		regmap_update_bits(sma1301->regmap, SMA1301_01_INPUT1_CTRL1,
					I2S_MODE_MASK, STANDARD_I2S);
		sma1301->i2s_mode = SND_SOC_DAIFMT_I2S;

		regmap_update_bits(sma1301->regmap, SMA1301_00_SYSTEM_CTRL,
					POWER_MASK, POWER_ON);

		regmap_update_bits(sma1301->regmap, SMA1301_0E_MUTE_VOL_CTRL,
					SPK_MUTE_MASK, SPK_UNMUTE);
		break;

	case SND_SOC_BIAS_STANDBY:
		dev_info(codec->dev, "%s\n", "SND_SOC_BIAS_STANDBY");
	case SND_SOC_BIAS_OFF:
		dev_info(codec->dev, "%s\n", "SND_SOC_BIAS_OFF");
		/* power-down */
		regmap_update_bits(sma1301->regmap, SMA1301_0E_MUTE_VOL_CTRL,
					SPK_MUTE_MASK, SPK_MUTE);
		regmap_update_bits(sma1301->regmap, SMA1301_10_SYSTEM_CTRL1,
					SPK_MODE_MASK, SPK_OFF);
		regmap_update_bits(sma1301->regmap, SMA1301_00_SYSTEM_CTRL,
					POWER_MASK, POWER_OFF);

		break;
	}

	/* Don't use codec->dapm.bias_level,
	 * use snd_soc_component_get_dapm() if it is needed
	 */

	return 0;
}

static int sma1301_suspend(struct snd_soc_codec *codec)
{
	struct sma1301_priv *sma1301 = snd_soc_codec_get_drvdata(codec);

	dev_info(codec->dev, "%s\n", __func__);

	sma1301_set_bias_level(codec, SND_SOC_BIAS_OFF);

	regcache_mark_dirty(sma1301->regmap);
	return 0;
}

static int sma1301_resume(struct snd_soc_codec *codec)
{
	dev_info(codec->dev, "%s\n", __func__);

	sma1301_set_bias_level(codec, SND_SOC_BIAS_STANDBY);
	return 0;
}

static int sma1301_reset(struct snd_soc_codec *codec)
{
	struct sma1301_priv *sma1301 = snd_soc_codec_get_drvdata(codec);
	struct reg_default *reg_val;
	int cnt;
	int len = sma1301->reg_array_len / sizeof(uint32_t);

	dev_info(codec->dev, "%s\n", __func__);

	/* External clock 24.576MHz */
	regmap_write(sma1301->regmap, SMA1301_00_SYSTEM_CTRL,	0x80);
	/* Volume control (0dB/0x30) */
	regmap_write(sma1301->regmap, SMA1301_0A_SPK_VOL, sma1301->init_vol);
	/* VOL_SLOPE - Off, MUTE_SLOPE - Off, SPK_MUTE - muted */
	regmap_write(sma1301->regmap, SMA1301_0E_MUTE_VOL_CTRL,	0x0F);
	/* Mono for one chip solution */
	regmap_write(sma1301->regmap, SMA1301_10_SYSTEM_CTRL1,	0x04);
	/* Delay control between OUTA and OUTB
	 * with main clock duty cycle
	 */
	regmap_write(sma1301->regmap, SMA1301_14_MODULATOR,	0x12);
	/* Attack level - 0.25FS,
	 * Release time - 40ms(48kHz) & 20ms(96kHz)
	 */
	regmap_write(sma1301->regmap, SMA1301_91_CLASS_G_CTRL,	0x82);
	/* Filtered VDD gain control */
	regmap_write(sma1301->regmap, SMA1301_92_FDPEC_CTRL,	0xC0);
	/* Trimming of driver deadtime - 1.7ns, switching slew - 1ns */
	regmap_write(sma1301->regmap, SMA1301_96_BOOST_CTRL3,	0x93);
	/* Trimming of reference voltage - 1.21V,
	 * boost voltage - 5.6V, minimum on-time - 62ns
	 */
	regmap_write(sma1301->regmap, SMA1301_97_BOOST_CTRL4,	0x6A);

	if (sma1301->sck_pll_enable) {
		/* PLL operation, PLL Clock, External Clock,
		 * PLL reference SCK clock
		 */
		regmap_write(sma1301->regmap,
				SMA1301_A2_TOP_MAN1, 0x88);
		regmap_write(sma1301->regmap,
				SMA1301_8B_PLL_POST_N, 0x06);
		/* PLL clock Input Freq - 3.0702MHz,
		 * Output Freq - 24.576MHz
		 */
		regmap_write(sma1301->regmap, SMA1301_8C_PLL_N, 0xC0);
		if (!strcmp(sma1301->board_rev, "MVT1")) {
		dev_info(codec->dev, "%s - MVT1 PLL setting\n", __func__);
		regmap_write(sma1301->regmap, SMA1301_3B_TEST1, 0x5A);
		regmap_write(sma1301->regmap, SMA1301_3C_TEST2, 0x00);
		regmap_write(sma1301->regmap, SMA1301_3D_TEST3, 0x00);
		regmap_write(sma1301->regmap, SMA1301_3E_ATEST1, 0x03);
		regmap_write(sma1301->regmap, SMA1301_3F_ATEST2, 0x06);
		regmap_write(sma1301->regmap, SMA1301_8F_PLL_F3, 0x02);
		} else if (!strcmp(sma1301->board_rev, "MVT2")) {
		dev_info(codec->dev, "%s - MVT2 PLL setting\n", __func__);
		regmap_write(sma1301->regmap, SMA1301_8B_PLL_POST_N, 0x06);
		}
	} else {
		/* External clock  operation */
		regmap_write(sma1301->regmap, SMA1301_A2_TOP_MAN1, 0x68);
	}
	if (sma1301->stereo_select == true) {
		/* SPK Mode (Stereo) */
		regmap_write(sma1301->regmap, SMA1301_10_SYSTEM_CTRL1, 0x10);
	} else {
		/* SPK Mode (Mono) */
		regmap_write(sma1301->regmap, SMA1301_10_SYSTEM_CTRL1, 0x04);
	}

	dev_info(codec->dev,
		"%s init_vol is 0x%x\n", __func__, sma1301->init_vol);

	/* Register value writing if register value is avaliable from DT */
	if (sma1301->reg_array != NULL) {
		for (cnt = 0; cnt < len; cnt += 2) {
			reg_val = (struct reg_default *)
				&sma1301->reg_array[cnt];
			dev_info(codec->dev, "%s reg_write [0x%02x, 0x%02x]",
					__func__, be32_to_cpu(reg_val->reg),
						be32_to_cpu(reg_val->def));
			regmap_write(sma1301->regmap, be32_to_cpu(reg_val->reg),
					be32_to_cpu(reg_val->def));
		}
	}

	/* Bass/Boost/CompLimiter OFF & EQ Disable
	 * MONO_MIX Off(TW) for SPK Signal Path
	 */
	regmap_write(sma1301->regmap, SMA1301_11_SYSTEM_CTRL2, 0x00);
	return 0;
}

static int sma1301_probe(struct snd_soc_codec *codec)
{
	dev_info(codec->dev, "%s\n", __func__);

	sma1301_reset(codec);
	sma1301_set_bias_level(codec, SND_SOC_BIAS_STANDBY);

	return 0;
}

static int sma1301_remove(struct snd_soc_codec *codec)
{
	sma1301_set_bias_level(codec, SND_SOC_BIAS_OFF);

	return 0;
}

static struct snd_soc_codec_driver soc_codec_dev_sma1301 = {
	.probe = sma1301_probe,
	.remove = sma1301_remove,
	.suspend = sma1301_suspend,
	.resume = sma1301_resume,
	.set_bias_level = sma1301_set_bias_level,
	.component_driver = {
		.controls = sma1301_snd_controls,
		.num_controls = ARRAY_SIZE(sma1301_snd_controls),
		.dapm_widgets = sma1301_dapm_widgets,
		.num_dapm_widgets = ARRAY_SIZE(sma1301_dapm_widgets),
		.dapm_routes = sma1301_audio_map,
		.num_dapm_routes = ARRAY_SIZE(sma1301_audio_map),
	},
};

const struct regmap_config sma_i2c_regmap = {
	.reg_bits = 8,
	.val_bits = 8,

	.max_register = SMA1301_FF_DEVICE_INDEX,
	.readable_reg = sma1301_readable_register,
	.writeable_reg = sma1301_writeable_register,
	.volatile_reg = sma1301_volatile_register,

	.cache_type = REGCACHE_RBTREE,
	.reg_defaults = sma1301_reg_def,
	.num_reg_defaults = ARRAY_SIZE(sma1301_reg_def),
};


static ssize_t show_sma1301_codec_reg(struct device *dev, struct device_attribute *attr,
					char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct sma1301_priv *sma1301 = i2c_get_clientdata(client);
	int ret, cnt = 0, i, val;

	for (i = 0; i <= SMA1301_FF_DEVICE_INDEX; i ++) {
		if(!sma1301_readable_register(dev, i))
			continue;
		ret = regmap_read(sma1301->regmap, i, &val);
		if (ret == 0)
			cnt += snprintf(buf + cnt, 10, "%x:0x%02x\n", i, val);
	}
	if (cnt >= PAGE_SIZE)
		cnt = PAGE_SIZE - 1;

	return cnt;
}

static DEVICE_ATTR(codec_reg, 0444, show_sma1301_codec_reg, NULL);

static ssize_t show_sma1301_volume(struct device *dev, struct device_attribute *attr,
					char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct sma1301_priv *sma1301 = i2c_get_clientdata(client);
	int ret, vol = 0;

	ret = regmap_read(sma1301->regmap, SMA1301_0A_SPK_VOL, &vol);


	return snprintf(buf, 16, "0x%x\n", vol);
}

static ssize_t store_sma1301_volume(struct device *dev, struct device_attribute *attr,
						const char *buf, size_t size)
{
	unsigned long state;
	int vol = 0;
	int ret = -EINVAL;
	struct i2c_client *client = to_i2c_client(dev);
	struct sma1301_priv *sma1301 = i2c_get_clientdata(client);

	ret = kstrtoul(buf, 16, &state);
	if (ret < 0)
		return ret;

	vol = state & 0xff;
	ret = regmap_write(sma1301->regmap, SMA1301_0A_SPK_VOL, vol);

	return size;
}


static DEVICE_ATTR(volume, 0660, show_sma1301_volume, store_sma1301_volume);

static int sma1301_i2c_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	struct sma1301_priv *sma1301;
	struct device_node *np = client->dev.of_node;
	int ret;
	u32 value;
	const char *str_value;
	const char *dev_name = NULL;

	dev_info(&client->dev, "%s is here\n", __func__);

	sma1301 = devm_kzalloc(&client->dev, sizeof(struct sma1301_priv),
							GFP_KERNEL);

	if (!sma1301)
		return -ENOMEM;

	sma1301->regmap = devm_regmap_init_i2c(client, &sma_i2c_regmap);
	if (IS_ERR(sma1301->regmap)) {
		ret = PTR_ERR(sma1301->regmap);
		dev_err(&client->dev,
			"Failed to allocate register map: %d\n", ret);
		return ret;
	}

	if (np) {
		ret = of_property_read_string_index(np, "dev-name", 0, &dev_name);
		if (ret == 0)
			dev_set_name(&client->dev, dev_name);
		if (!of_property_read_u32(np, "init-vol", &value)) {
			dev_info(&client->dev,
				"init_vol is 0x%x from DT\n", value);
			sma1301->init_vol = value;
		} else{
			dev_info(&client->dev,
				"init_vol is set with 0x30(0dB)\n");
			sma1301->init_vol = 0x30;
		}
		if (of_property_read_bool(np, "stereo-select")) {
			dev_info(&client->dev, "Stereo for two chip solution\n");
				sma1301->stereo_select = true;
		} else{
			dev_info(&client->dev, "Mono for one chip solution\n");
				sma1301->stereo_select = false;
		}
		if (of_property_read_bool(np, "sck-pll-enable")) {
			dev_info(&client->dev, "Use SCK clock as the PLL reference clock\n");
				sma1301->sck_pll_enable = true;
		} else{
			dev_info(&client->dev, "Use External clock as the PLL reference clock\n");
				sma1301->sck_pll_enable = false;
		}
		if (of_property_read_string(np, "board-revision",
					&str_value) == 0) {
			strcpy(sma1301->board_rev, str_value);
			dev_info(&client->dev,
				"Read the value of board revision - %s\n",
				sma1301->board_rev);
		}
		sma1301->reg_array = of_get_property(np,
				"registers-of-amp", &sma1301->reg_array_len);

		if (sma1301->reg_array == NULL)
			dev_info(&client->dev,
				"There is no registers-of-amp from DT\n");
		}
	sma1301->devtype = id->driver_data;
	i2c_set_clientdata(client, sma1301);

	ret = snd_soc_register_codec(&client->dev,
		&soc_codec_dev_sma1301, &sma1301_dai, 1);
	if (ret != 0) {
		dev_err(&client->dev, "Failed to register CODEC: %d\n", ret);
		kfree(sma1301);
	}
	device_create_file(&client->dev, &dev_attr_codec_reg);
	device_create_file(&client->dev, &dev_attr_volume);
	dev_info(&client->dev,
				"%s done\n", __func__);
	return ret;
}

static int sma1301_i2c_remove(struct i2c_client *client)
{
	snd_soc_unregister_codec(&client->dev);
	device_remove_file(&client->dev, &dev_attr_codec_reg);
	device_create_file(&client->dev, &dev_attr_volume);

	return 0;
}

static const struct i2c_device_id sma1301_i2c_id[] = {
	{"sma1301", 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, sma1301_i2c_id);

static const struct of_device_id sma1301_of_match[] = {
	{ .compatible = "siliconmitus,sma1301", },
	{ }
};
MODULE_DEVICE_TABLE(of, sma1301_of_match);

static struct i2c_driver sma1301_i2c_driver = {
	.driver = {
		.name = "sma1301",
		.of_match_table = sma1301_of_match,
	},
	.probe = sma1301_i2c_probe,
	.remove = sma1301_i2c_remove,
	.id_table = sma1301_i2c_id,
};

module_i2c_driver(sma1301_i2c_driver);
/*
static int __init sma1301_init(void)
{
	int ret;

	ret = i2c_add_driver(&sma1301_i2c_driver);

	if (ret)
		pr_err("Failed to register sma1301 I2C driver: %d\n", ret);

	return ret;
}

module_init(sma1301_init);

static void __exit sma1301_exit(void)
{
	i2c_del_driver(&sma1301_i2c_driver);
}

module_exit(sma1301_exit);
*/
MODULE_DESCRIPTION("ALSA SoC SMA1301 driver");
MODULE_AUTHOR("Brian Pyun, <moosung.pyun@siliconmitus.com>");
MODULE_AUTHOR("GH Park, <gyuhwa.park@irondevice.com>");
MODULE_LICENSE("GPL");
