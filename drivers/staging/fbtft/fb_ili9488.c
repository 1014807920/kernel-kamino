/*
 * FB driver for the ILI9488 LCD Controller
 *
 * Copyright (C) 2014 Noralf Tronnes
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

#include "fbtft.h"

#define DRVNAME		"fb_ili9488"
#define WIDTH		320
#define HEIGHT		480
#define BPP			24

/* this init sequence matches PiScreen */
static int default_init_sequence[] = {
	/* Interface Mode Control */
	-1, 0xb0, 0x0,
	/* Sleep OUT */
	-1, 0x11,
	-2, 250,
	/* Interface Pixel Format */
	-1, 0x3A, 0x66,
	/* Power Control 1 */
	-1, 0xC0, 0x10, 0x10,
	/* Power Control 2 */
	-1, 0xC1, 0x41,
	/* VCOM Control 1 */
	-1, 0xC5, 0x00, 0x22, 0x00, 0x80,
	/*  Frame Rate Control */
	-1, 0xB1, 0xB0, 0x11,
	/* Display Inversion Control */
	-1, 0xB4, 0x02,
	/* Display Function Control */
	-1, 0xB6, 0x02, 0x02,
	/* Entry Mode Set */
	-1, 0xB7, 0xC6,
	/* Set Image Function */
	-1, 0xE9, 0x0,
	/* Adjust Control 3 */
	-1, 0xF7, 0xA9, 0x51, 0x2C, 0x82,
	/* PGAMCTRL(Positive Gamma Control) */
	-1, 0xE0, 0x00, 0x07, 0x0F, 0x0D, 0x1B, 0x0A, 0x3C, 0x78,
		  0x4A, 0x07, 0x0E, 0x09, 0x1B, 0x1E, 0x0F,
	/* NGAMCTRL(Negative Gamma Control) */
	-1, 0xE1, 0x00, 0x22, 0x24, 0x06, 0x12, 0x07, 0x36, 0x47,
		  0x47, 0x06, 0x0a, 0x07, 0x30, 0x37, 0x0F,
	/* Sleep OUT */
	-1, 0x11,
	/* Display ON */
	-1, 0x29,
	/* end marker */
	-3
};

static void set_addr_win(struct fbtft_par *par, int xs, int ys, int xe, int ye)
{
	/* Column address */
	write_reg(par, 0x2A, xs >> 8, xs & 0xFF, xe >> 8, xe & 0xFF);

	/* Row address */
	write_reg(par, 0x2B, ys >> 8, ys & 0xFF, ye >> 8, ye & 0xFF);

	/* Memory write */
	write_reg(par, 0x2C);
}

static int set_var(struct fbtft_par *par)
{
	par->info->var.red.offset = 16;
	par->info->var.red.length = 8;
	par->info->var.green.offset = 8;
	par->info->var.green.length = 8;
	par->info->var.blue.offset = 0;
	par->info->var.blue.length = 8;

	switch (par->info->var.rotate) {
	case 0:
		write_reg(par, 0x36, 0x80 | (par->bgr << 3));
		break;
	case 90:
		write_reg(par, 0x36, 0x20 | (par->bgr << 3));
		break;
	case 180:
		write_reg(par, 0x36, 0x40 | (par->bgr << 3));
		break;
	case 270:
		write_reg(par, 0x36, 0xE0 | (par->bgr << 3));
		break;
	default:
		break;
	}

	return 0;
}

static int write_vmem(struct fbtft_par *par, size_t offset, size_t len)
{
	u8 *vmem8;
	int to_copy = 3072;

	fbtft_par_dbg(DEBUG_WRITE_VMEM, par, "%s(offset=%zu, len=%zu)\n",
				__func__, offset, len);

	vmem8 = (u8 *)(par->info->screen_buffer + offset);

	if (par->gpio.dc != -1)
		gpio_set_value(par->gpio.dc, 1);

	while (len > to_copy) {
		par->fbtftops.write(par, vmem8, to_copy);
		vmem8 = vmem8 + to_copy;
		len = len - to_copy;
	}

	par->fbtftops.write(par, vmem8, len);

	return 0;
}

static struct fbtft_display display = {
	.regwidth = 8,
	.width = WIDTH,
	.height = HEIGHT,
	.bpp = BPP,
	.init_sequence = default_init_sequence,
	.fbtftops = {
		.set_addr_win = set_addr_win,
		.set_var = set_var,
		.write_vmem = write_vmem,
	},
};

FBTFT_REGISTER_DRIVER(DRVNAME, "ilitek,ili9488", &display);

MODULE_ALIAS("spi:" DRVNAME);
MODULE_ALIAS("platform:" DRVNAME);
MODULE_ALIAS("spi:ili9488");
MODULE_ALIAS("platform:ili9488");

MODULE_DESCRIPTION("FB driver for the ILI9488 LCD Controller");
MODULE_AUTHOR("Noralf Tronnes");
MODULE_LICENSE("GPL");
