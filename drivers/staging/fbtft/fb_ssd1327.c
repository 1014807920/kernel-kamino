/*
 * FB driver for the SSD1327 OLED Controller
 *
 * Copyright (C) 2013 Noralf Tronnes
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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/gpio.h>
#include <linux/delay.h>

#include "fbtft.h"

#define DRVNAME		"fb_ssd1327"
#define WIDTH		128
#define HEIGHT		128

static int init_display(struct fbtft_par *par)
{
	par->fbtftops.reset(par);
	/* Set Display OFF */
	write_reg(par, 0xAE);

	/* Set Contrast Control */
	write_reg(par, 0x81);
	write_reg(par, 0x80);

	/* Set Re-map */
	write_reg(par, 0xA0);
	write_reg(par, 0x51);

	/* Set Display Start Line */
	write_reg(par, 0xA1);
	write_reg(par, 0x00);

	/* Set Display Offset */
	write_reg(par, 0xA2);
	write_reg(par, 0x00);

	/* rmal display */
	write_reg(par, 0xA4);

	/* Set Mux Ratio */
	write_reg(par, 0xA8);
	write_reg(par, 0x7F);

	/* Set Phase Length */
	write_reg(par, 0xB1);
	write_reg(par, 0xF1);

	/* Set Display Clock Divide Ratio/Oscillator Frequency */
	write_reg(par, 0xB3);
	write_reg(par, 0x00);

	/* Set Function SelectionA */
	write_reg(par, 0xAB);
	write_reg(par, 0x01);

	/* Set Second Pre-charge period */
	write_reg(par, 0xB6);
	write_reg(par, 0x0F);

	/*Set Prechange Voltage */
	write_reg(par, 0xBC);
	write_reg(par, 0x08);

	/* Set VCOMH Voltage */
	write_reg(par, 0xBE);
	write_reg(par, 0x07);

	/* Set Function selection B */
	write_reg(par, 0xD5);
	write_reg(par, 0x62);

	/* Set Display On */
	write_reg(par, 0xAF);

	return 0;
}

static void set_addr_win(struct fbtft_par *par, int xs, int ys, int xe, int ye)
{
	fbtft_par_dbg(DEBUG_SET_ADDR_WIN, par, "%s(xs=%d,xe=%d,ys=%d,ye=%d)\n",
		__func__, xs, xe, ys, ye);

	/* Set Column Address */
	write_reg(par, 0x15);
	write_reg(par, 0x00);
	write_reg(par, 0x7F);

	/* Set Row Address */
	write_reg(par, 0x75);
	write_reg(par, 0x00);
	write_reg(par, 0x7F);
}

static int set_var(struct fbtft_par *par)
{
	switch (par->info->var.rotate) {
	case 0:
		write_reg(par, 0xA0);
		write_reg(par, 0x51);
		break;
	case 180:
		write_reg(par, 0xA0);
		write_reg(par, 0x42);
		break;
	}

	return 0;
}

static int write_vmem(struct fbtft_par *par, size_t offset, size_t len)
{
	u8 *vmem8;

	fbtft_par_dbg(DEBUG_WRITE_VMEM, par, "%s(offset=%zu, len=%zu)\n",
		__func__, offset, len);

	vmem8 = (u8 *)(par->info->screen_buffer + offset);

	if (par->gpio.dc != -1)
		gpio_set_value(par->gpio.dc, 1);

	return par->fbtftops.write(par, vmem8, len);
}

static struct fbtft_display display = {
	.regwidth = 8,
	.width = WIDTH,
	.height = HEIGHT,
	.fbtftops = {
		.init_display = init_display,
		.set_addr_win = set_addr_win,
		.set_var = set_var,
		.write_vmem = write_vmem,
	},
};

FBTFT_REGISTER_DRIVER(DRVNAME, "solomon,ssd1327", &display);

MODULE_ALIAS("spi:" DRVNAME);
MODULE_ALIAS("platform:" DRVNAME);
MODULE_ALIAS("spi:ssd1327");
MODULE_ALIAS("platform:ssd1327");

MODULE_DESCRIPTION("SSD1327 OLED Driver");
MODULE_AUTHOR("Noralf Tronnes");
MODULE_LICENSE("GPL");
