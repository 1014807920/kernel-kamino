/*
 * Atmel SSC driver
 *
 * Copyright (C) 2007 Atmel Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/platform_device.h>
#include <linux/list.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/interrupt.h>

#include <linux/of.h>

#define COUNTER_MAX_NUM        4

#define NC_COUNTER_1_STATUS   0x00
#define NC_COUNTER_1_VALUE    0x04
#define NC_COUNTER_1_CONTROL  0x10
#define NC_COUNTER_1_CONFIG   0x20
#define NC_COUNTER_1_PRE      0x24
#define NC_COUNTER_1_INI      0x28

#define NC_COUNTER_2_STATUS   0x40
#define NC_COUNTER_2_VALUE    0x44
#define NC_COUNTER_2_CONTROL  0x50
#define NC_COUNTER_2_CONFIG   0x60
#define NC_COUNTER_2_PRE      0x64
#define NC_COUNTER_2_INI      0x68

#define NC_COUNTER_3_STATUS   0x80
#define NC_COUNTER_3_VALUE    0x84
#define NC_COUNTER_3_CONTROL  0x90
#define NC_COUNTER_3_CONFIG   0xa0
#define NC_COUNTER_3_PRE      0xa4
#define NC_COUNTER_3_INI      0xa8

#define NC_COUNTER_4_STATUS   0xc0
#define NC_COUNTER_4_VALUE    0xc4
#define NC_COUNTER_4_CONTROL  0xd0
#define NC_COUNTER_4_CONFIG   0xe0
#define NC_COUNTER_4_PRE      0xe4
#define NC_COUNTER_4_INI      0xe8

/* Register access macros */
#define ncc_readl(base, reg)         __raw_readl(base + reg)
#define ncc_writel(base, reg, value) __raw_writel((value), base + reg)

#define COUNTER2_OVERFLOW_TIME 0xd693a400  //1h (60*60*1000*1000)
#define COUNTER3_OVERFLOW_TIME 0xd64758c0  //59m55s (59*60*1000*1000 + 55*1000*1000)

struct nc_counter_device {
	void __iomem            *regs;
	struct platform_device  *pdev;
	u32                     clk;
	int                     irq[COUNTER_MAX_NUM];
	u32                     overflow[COUNTER_MAX_NUM];
};

static struct nc_counter_device *ncc_dev;

static irqreturn_t counter2_isr(int irq, void *data)
{
	u32 value;
	struct nc_counter_device *ncc = ncc_dev;

	value = ncc_readl(ncc->regs, NC_COUNTER_2_STATUS);
	ncc_writel(ncc->regs, NC_COUNTER_2_STATUS, value|(0x1<<1));
	ncc->overflow[1]++;
	return IRQ_HANDLED;
}

static irqreturn_t counter3_isr(int irq, void *data)
{
	u32 value;
	struct nc_counter_device *ncc = ncc_dev;

	value = ncc_readl(ncc->regs, NC_COUNTER_3_STATUS);
	ncc_writel(ncc->regs, NC_COUNTER_3_STATUS, value|(0x1<<1));
	ncc->overflow[2]++;
	return IRQ_HANDLED;
}

static int nc_counter_init(struct nc_counter_device *ncc)
{
	int i;
	int error = 0;

	for (i=0; i<COUNTER_MAX_NUM; i++)
		ncc->overflow[i] = 0;

	ncc_writel(ncc->regs, NC_COUNTER_2_CONTROL, 1);
	ncc_writel(ncc->regs, NC_COUNTER_2_CONTROL, 0);
	ncc_writel(ncc->regs, NC_COUNTER_2_CONFIG, 3);
	ncc_writel(ncc->regs, NC_COUNTER_2_PRE, (ncc->clk/(1000000))-1); //1us 1/pre_clk
	ncc_writel(ncc->regs, NC_COUNTER_2_INI, 0-COUNTER2_OVERFLOW_TIME);

	ncc_writel(ncc->regs, NC_COUNTER_3_CONTROL, 1);
	ncc_writel(ncc->regs, NC_COUNTER_3_CONTROL, 0);
	ncc_writel(ncc->regs, NC_COUNTER_3_CONFIG, 3);
	ncc_writel(ncc->regs, NC_COUNTER_3_PRE, (ncc->clk/(1000000))-1); //1us 1/pre_clk
	ncc_writel(ncc->regs, NC_COUNTER_3_INI, 0-COUNTER3_OVERFLOW_TIME);

	error = request_irq(ncc->irq[1], (irq_handler_t)counter2_isr, 0, "counter2", NULL);
	if (error) {
		return error;
	}
	error = request_irq(ncc->irq[2], (irq_handler_t)counter3_isr, 0, "counter3", NULL);
	if (error) {
		return error;
	}

	ncc_writel(ncc->regs, NC_COUNTER_2_CONTROL, 2); //begin count
	ncc_writel(ncc->regs, NC_COUNTER_3_CONTROL, 2); //begin count
	return 0;
}

static u32 get_counter2_offset(void)
{
	u32 value;
	struct nc_counter_device *ncc = ncc_dev;

	value = ncc_readl(ncc->regs, NC_COUNTER_2_VALUE);
	value -= 0-COUNTER2_OVERFLOW_TIME;

	return value;
}

static u32 get_counter3_offset(void)
{
	u32 value;
	struct nc_counter_device *ncc = ncc_dev;

	value = ncc_readl(ncc->regs, NC_COUNTER_2_VALUE);
	value -= 0-COUNTER2_OVERFLOW_TIME;

	return value;
}

u64 gx_time_get_us(void)
{
	u32 counter2_overflow;
	u32 counter2_offset;
	u32 counter3_overflow;
	u32 counter3_offset;
	u64 counter2_us;
	u64 counter3_us;
	u64 old_counter_us;
	u64 new_counter_us;
	struct nc_counter_device *ncc = ncc_dev;

	counter2_overflow = ncc->overflow[1];
	counter2_offset = get_counter2_offset();
	counter3_overflow = ncc->overflow[2];
	counter3_offset = get_counter3_offset();
	counter2_us = (u64)counter2_overflow*COUNTER2_OVERFLOW_TIME+counter2_offset;
	counter3_us = (u64)counter3_overflow*COUNTER3_OVERFLOW_TIME+counter3_offset;
	old_counter_us = counter2_us > counter3_us ? counter2_us : counter3_us;

	counter2_overflow = ncc->overflow[1];
	counter2_offset = get_counter2_offset();
	counter3_overflow = ncc->overflow[2];
	counter3_offset = get_counter3_offset();
	counter2_us = (u64)counter2_overflow*COUNTER2_OVERFLOW_TIME+counter2_offset;
	counter3_us = (u64)counter3_overflow*COUNTER3_OVERFLOW_TIME+counter3_offset;
	new_counter_us = counter2_us > counter3_us ? counter2_us : counter3_us;

	return (new_counter_us > old_counter_us ? new_counter_us : old_counter_us);
}
EXPORT_SYMBOL(gx_time_get_us);

u32 gx_time_get_ms(void)
{
	u64 time;
	time = gx_time_get_us();
	do_div(time,1000);
	return (u32)time;
}
EXPORT_SYMBOL(gx_time_get_ms);

void gx_time_delay_us(u32 delay)
{
	u64 crt;
	u64 init;

	init = gx_time_get_us();
	while (1) {
		crt = gx_time_get_us();
		if (crt>(init+delay))
			break;
	}
}
EXPORT_SYMBOL(gx_time_delay_us);

static int nc_counter_pltfm_probe(struct platform_device *pdev)
{
	int i;
	int error = 0;
	struct resource *regs;
	struct nc_counter_device *ncc;
	struct clk *clk;

	ncc = devm_kzalloc(&pdev->dev, sizeof(struct nc_counter_device), GFP_KERNEL);
	if (!ncc) {
		dev_dbg(&pdev->dev, "out of memory\n");
		return -ENOMEM;
	}

	ncc->pdev = pdev;
	ncc_dev = ncc;

	regs = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	ncc->regs = devm_ioremap_resource(&pdev->dev, regs);
	if (IS_ERR(ncc->regs))
		return PTR_ERR(ncc->regs);

	clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(clk)) {
		dev_dbg(&pdev->dev, "no clock defined\n");
		return -ENXIO;
	}
	ncc->clk= clk_get_rate(clk);

	for (i=0; i<COUNTER_MAX_NUM; i++) {
		ncc->irq[i] = platform_get_irq(pdev, i);
		if (!ncc->irq[i]) {
			dev_dbg(&pdev->dev, "could not get irq\n");
			return -ENXIO;
		}
	}

	platform_set_drvdata(pdev, ncc);

	dev_info(&pdev->dev, "nationalchip counter device at 0x%p (irq %d - %d)\n",
			ncc->regs, ncc->irq[0], ncc->irq[COUNTER_MAX_NUM-1]);

	error = nc_counter_init(ncc);
	if (error) {
		dev_err(&pdev->dev, "can't counter ISR\n");
		return -ENXIO;
	}

	return 0;
}

static int nc_counter_pltfm_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id nc_counter_pltfm_match[] = {
	{ .compatible = "nationalchip,counter-v1", },
	{},
};

MODULE_DEVICE_TABLE(of, nc_counter_pltfm_match);

static struct platform_driver nc_counter_pltfm_driver = {
	.probe         = nc_counter_pltfm_probe,
	.remove        = nc_counter_pltfm_remove,
	.driver        = {
		.name = "counter",
		.of_match_table = nc_counter_pltfm_match,
		//.pm  = &nc_counter_pltfm_pmops,
	},
};

module_platform_driver(nc_counter_pltfm_driver);

MODULE_DESCRIPTION("support for NationalChilp counter modules");
MODULE_AUTHOR("liuyx");
MODULE_LICENSE("GPL");
MODULE_SUPPORTED_DEVICE("leo");
MODULE_VERSION("V1.0");
