/*
 * FB driver for the ST7789V LCD Controller
 *
 * Copyright (C) 2015 Dennis Menschel
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include "fbtft.h"

#define DRVNAME		"fb_st7789v"
#define WIDTH		240
#define HEIGHT		240
#define BPP			16

#define DEFAULT_GAMMA \
   "70 2C 2E 15 10 09 48 33 53 18 2B 28 2E 32\n"\
   "70 2C 2E 15 10 09 48 33 53 18 2B 28 2E 32"

/* this init sequence matches PiScreen */
static int default_init_sequence[] = {
	/* Sleep OUT */
	-1, 0x11,
	-2, 80,
	/* Memory Data Access Control */
	-1, 0x36, 0x00,
	/* Interface Pixel Format */
	-1, 0x3A, 0x05,
	/* RAM Control */
	-1, 0xB0, 0x00, 0xE0,
	/* Porch Setting */
	-1, 0xB2, 0x0C, 0x0C, 0x00, 0x33, 0x33,
	/* Gate Control */
	-1, 0xB7, 0x35,
	/* VCOMS Setting */
	-1, 0xBB, 0x2B,
	/* LCM Control */
	-1, 0xC0, 0x2C,
	/* VDV and VRH Command Enable */
	-1, 0xC2, 0x01,
	/* VRH Set */
	-1, 0xC3, 0x11,
	/* VDV Set */
	-1, 0xC4, 0x20,
	/* VCOMS Offset Set */
	-1, 0xC5, 0x20,
	/* Frame Rate Control in Normal Mode */
	-1, 0xC6, 0x0F,
	/* Power Control1 */
	-1, 0xD0, 0xA4, 0xA1,
	/* Display Inversion On */
    -1, 0x21,
	/* Display ON */
	-1, 0x29,
	/* Memory write */
	-1, 0x2C,
	/* end marker */
	-3
};

static void set_addr_win(struct fbtft_par *par, int xs, int ys, int xe, int ye)
{
	/* Column address */
	write_reg(par, 0x2A, xs >> 8, xs & 0xFF, xe >> 8, xe & 0xFF),
	/* Row address */
	write_reg(par, 0x2A, ys >> 8, ys & 0xFF, ye >> 8, ye & 0xFF),
	/* Memory write */
	write_reg(par, 0x2C);
}

#define MADCTL_MH  BIT(2)  /* bitmask for horizontal refresh order */
#define MADCTL_BGR BIT(3) /* bitmask for RGB/BGR order */
#define MADCTL_ML  BIT(4)  /* bitmask for vertical refresh order */
#define MADCTL_MV  BIT(5)  /* bitmask for page/column order */
#define MADCTL_MX  BIT(6)  /* bitmask for column address order */
#define MADCTL_MY  BIT(7)  /* bitmask for page address order */

static int set_var(struct fbtft_par *par)
{
	u8 madctl_par = 0;

	if (par->bgr)
		madctl_par |= MADCTL_BGR;
	switch (par->info->var.rotate) {
	case 0:
		break;
	case 90:
		madctl_par |= (MADCTL_MV | MADCTL_MY);
		break;
	case 180:
		madctl_par |= (MADCTL_MX | MADCTL_MY);
		break;
	case 270:
		madctl_par |= (MADCTL_MV | MADCTL_MX);
		break;
	}
	write_reg(par, 0x36, madctl_par);
	return 0;
}

#define CURVE(num, idx)  curves[num * par->gamma.num_values + idx]
static int set_gamma(struct fbtft_par *par, unsigned long *curves)
{
	int i;

	for (i = 0; i < par->gamma.num_curves; i++)
		write_reg(par, 0xE0 + i,
			CURVE(i, 0), CURVE(i, 1), CURVE(i, 2),
			CURVE(i, 3), CURVE(i, 4), CURVE(i, 5),
			CURVE(i, 6), CURVE(i, 7), CURVE(i, 8),
			CURVE(i, 9), CURVE(i, 10), CURVE(i, 11),
			CURVE(i, 12), CURVE(i, 13));

	return 0;
}
#undef CURVE

static struct fbtft_display display = {
	.regwidth = 8,
	.width = WIDTH,
	.height = HEIGHT,
	.bpp = BPP,
	.gamma_num = 2,
	.gamma_len = 14,
	.gamma = DEFAULT_GAMMA,
	.init_sequence = default_init_sequence,
	.fbtftops = {
		.set_addr_win = set_addr_win,
		.set_var = set_var,
		.set_gamma = set_gamma,
	},
};

FBTFT_REGISTER_DRIVER(DRVNAME, "sitronix,st7789v", &display);

MODULE_ALIAS("spi:" DRVNAME);
MODULE_ALIAS("platform:" DRVNAME);
MODULE_ALIAS("spi:st7789v");
MODULE_ALIAS("platform:st7789v");

MODULE_DESCRIPTION("FB driver for the ST7789V LCD display Controller");
MODULE_AUTHOR("TJT");
MODULE_LICENSE("GPL");
