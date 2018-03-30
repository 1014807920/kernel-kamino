/*
 * Memory-mapped interface driver for DW SPI Core
 *
 * Copyright (c) 2010, Octasic semiconductor.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 */

#include <linux/clk.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/spi/spi.h>
#include <linux/scatterlist.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_platform.h>
#include <linux/property.h>

#include "spi-dw.h"

#define DRIVER_NAME "dw_spi_mmio"

struct dw_spi_mmio {
	struct dw_spi  dws;
	struct clk     *clk;
};

static int dw_spi_mmio_probe(struct platform_device *pdev)
{
	struct dw_spi_mmio *dwsmmio;
	struct dw_spi *dws;
	struct resource *mem;
	int ret;
	int num_cs;
	uint32_t cs_reg;
	uint32_t data;
	uint32_t size;

	dwsmmio = devm_kzalloc(&pdev->dev, sizeof(struct dw_spi_mmio),
			GFP_KERNEL);
	if (!dwsmmio)
		return -ENOMEM;

	dws = &dwsmmio->dws;

	/* Get basic io resource and map it */
	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!mem) {
		dev_err(&pdev->dev, "no mem resource?\n");
		return -EINVAL;
	}

	dws->regs = devm_ioremap_resource(&pdev->dev, mem);
	if (IS_ERR(dws->regs)) {
		dev_err(&pdev->dev, "SPI region map failed\n");
		return PTR_ERR(dws->regs);
	}

	ret = device_property_read_u32(&pdev->dev, "cs-reg", &cs_reg);
	if(!ret){
		dws->cs_reg = ioremap(cs_reg, 4);
		if(!dws->cs_reg){
			dev_err(&pdev->dev, "SPI CS region map failed\n");
			return -EINVAL;
		}
	}

	dws->irq = platform_get_irq(pdev, 0);
	if (dws->irq < 0) {
		dev_err(&pdev->dev, "no irq resource?\n");
		return dws->irq; /* -ENXIO */
	}

	dws->is_int_from_mcu = of_property_read_bool(pdev->dev.of_node, "is-int-from-mcu");
	if(dws->is_int_from_mcu == true){
		/* get intc register base addr */
		of_property_read_u32_index(pdev->dev.of_node, "ifc-reg", 0, &data);
		of_property_read_u32_index(pdev->dev.of_node, "ifc-reg", 1, &size);
		dws->ifm_reg = ioremap(data, size);
		if(!dws->ifm_reg){
			dev_err(&pdev->dev, "intc reg region map failed\n");
			return -EINVAL;
		}

		/* get intc spi interrupt bit */
		ret = device_property_read_u32(&pdev->dev,"ifc-chan", &dws->ifm_chan);
		if(ret < 0)
			return -EINVAL;

		/* clear ck to arm intc spi bit */
		data = 1 << dws->ifm_chan;
		writel(data, dws->ifm_reg + 4);

		/* enable spi transmit channel */
		data = __raw_readl(dws->ifm_reg + 8);
		data |= 1 << dws->ifm_chan;
		writel(data, dws->ifm_reg + 8);
	}

	dwsmmio->clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(dwsmmio->clk))
		return PTR_ERR(dwsmmio->clk);
	ret = clk_prepare_enable(dwsmmio->clk);
	if (ret)
		return ret;

	dws->bus_num = pdev->id;

	dws->max_freq = clk_get_rate(dwsmmio->clk);
	device_property_read_u32(&pdev->dev, "poll-mode", &dws->poll_mode);
	device_property_read_u32(&pdev->dev, "reg-io-width", &dws->reg_io_width);

	num_cs = 4;

	device_property_read_u32(&pdev->dev, "num-cs", &num_cs);

	dws->num_cs = num_cs;

	if (pdev->dev.of_node) {
		int i;

		for (i = 0; i < dws->num_cs; i++) {
			int cs_gpio = of_get_named_gpio(pdev->dev.of_node,
					"cs-gpios", i);

			if (cs_gpio == -EPROBE_DEFER) {
				ret = cs_gpio;
				goto out;
			}

			if (gpio_is_valid(cs_gpio)) {
				ret = devm_gpio_request(&pdev->dev, cs_gpio,
						dev_name(&pdev->dev));
				if (ret)
					goto out;
			}
		}
	}

	ret = dw_spi_add_host(&pdev->dev, dws);
	if (ret)
		goto out;

	platform_set_drvdata(pdev, dwsmmio);
	return 0;

out:
	clk_disable_unprepare(dwsmmio->clk);
	return ret;
}

static int dw_spi_mmio_remove(struct platform_device *pdev)
{
	struct dw_spi_mmio *dwsmmio = platform_get_drvdata(pdev);

	clk_disable_unprepare(dwsmmio->clk);
	dw_spi_remove_host(&dwsmmio->dws);

	return 0;
}

static int dw_spi_mmio_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct dw_spi_mmio *dwsmmio = platform_get_drvdata(pdev);
	return dw_spi_suspend_host(&dwsmmio->dws);
}

static int dw_spi_mmio_resume(struct platform_device *pdev)
{
	struct dw_spi_mmio *dwsmmio = platform_get_drvdata(pdev);
	return dw_spi_resume_host(&dwsmmio->dws);
}

static const struct of_device_id dw_spi_mmio_of_match[] = {
	{ .compatible = "snps,dw-apb-ssi", },
	{ /* end of table */}
};
MODULE_DEVICE_TABLE(of, dw_spi_mmio_of_match);

static struct platform_driver dw_spi_mmio_driver = {
	.probe		= dw_spi_mmio_probe,
	.remove		= dw_spi_mmio_remove,
	.driver		= {
		.name	= DRIVER_NAME,
		.of_match_table = dw_spi_mmio_of_match,
	},
	.suspend = dw_spi_mmio_suspend,
	.resume  = dw_spi_mmio_resume,
};
module_platform_driver(dw_spi_mmio_driver);

MODULE_AUTHOR("Jean-Hugues Deschenes <jean-hugues.deschenes@octasic.com>");
MODULE_DESCRIPTION("Memory-mapped I/O interface driver for DW SPI Core");
MODULE_LICENSE("GPL v2");
