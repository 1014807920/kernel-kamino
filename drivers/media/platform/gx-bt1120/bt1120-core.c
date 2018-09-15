#include <linux/bug.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/version.h>

#include <media/media-device.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-clk.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf2-v4l2.h>
#include <media/videobuf2-dma-contig.h>

#include "bt1120-core.h"
#include "bt1120-reg.h"

static struct bt1120_clk_info gx_bt1120_clk_info = {
	.num = 2,
	.name = {
		"gx.bt1120_div.clk",
		"gx.bt1120_ddr.clk",
	},
};

static struct bt1120_reg_info gx_bt1120_reg_info = {
	.num     = 2,
	.name    = {
		"gx.bt1120.regs",
		"gx.bt1120.reset_regs"
	},
	.length   = {0x1bc, 0x1c},
	.baseAddr = {0x04d00000, 0x0230a000},
};

static struct bt1120_irq_info gx_bt1120_irq_info = {
	.num = 1,
	.name = {"gx.bt1120.irqs"},
	.irq = {47,},
	.irqFlag = {IRQF_SHARED,},
};

static struct clk *sensor_clk = NULL;
static int set_sensor_clk_24M(struct bt1120_dev *bt1120, struct platform_device *pdev)
{
	char clk_name[] = "xvclk";
	unsigned long rate = 24000000;
	int ret = -1;
	struct clk *bt1120_div_clk;

	sensor_clk = devm_clk_get(&pdev->dev, clk_name);
	if (IS_ERR(sensor_clk)) {
		dev_err(&pdev->dev, "get sensor clk error!!!!\n");
		return -1;
	}

	bt1120_div_clk = devm_clk_get(&pdev->dev, gx_bt1120_clk_info.name[0]);
	if (IS_ERR(bt1120_div_clk)) {
		dev_err(&pdev->dev, "get bt1120 div clk error!!!!\n");
		return -1;
	}

	ret = clk_set_parent(sensor_clk, bt1120_div_clk);
	ret = clk_set_rate(sensor_clk, rate);
	dev_info(&pdev->dev, "round_rate:%ld\n", clk_round_rate(sensor_clk, rate));
	dev_info(&pdev->dev, "##sensor_clk rate:%ld ret:%d\n", clk_get_rate(sensor_clk), ret);
	clk_prepare_enable(sensor_clk);

	return 0;
}

static int gx_bt1120_dt_parse(struct bt1120_dev *bt1120, struct platform_device *pdev)
{
	int i = 0, ret = 0;
	struct device_node *np = pdev->dev.of_node;
	struct resource *res = NULL;

	for (i = 0; i < gx_bt1120_reg_info.num; i++) {
		if (gx_bt1120_reg_info.name[i]) {
			res = platform_get_resource_byname(pdev, IORESOURCE_MEM, gx_bt1120_reg_info.name[i]);
			if (res) {
				gx_bt1120_reg_info.baseAddr[i] = res->start;
				gx_bt1120_reg_info.length[i]= res->end - res->start;
			}
		}
	}

	for (i = 0; i < gx_bt1120_irq_info.num; i++) {
		if (gx_bt1120_irq_info.name[i]) {
			res = platform_get_resource_byname(pdev, IORESOURCE_IRQ, gx_bt1120_irq_info.name[i]);
			if (res) {
				gx_bt1120_irq_info.irq[i] = res->start;
			}
		}
	}

//	for (i = 0; i < gx_bt1120_clk_info.num; i++) {
//		if (gx_bt1120_clk_info.name[i]) {
//			bt1120->clk[i] = v4l2_clk_get(&pdev->dev, gx_bt1120_clk_info.name[i]);
//			if (IS_ERR(bt1120->clk[i])) {
//				dev_err(&pdev->dev, "get clk error!!!!\n");
//				return -1;
//			}
//		}
//	}
//	bt1120->clk_info = &gx_bt1120_clk_info;

	bt1120->reg = (struct bt1120_reg *)ioremap(gx_bt1120_reg_info.baseAddr[0], gx_bt1120_reg_info.length[0]);
	if (!bt1120->reg) {
		dev_err(&pdev->dev, "ioremap reg error\n");
		goto err_ioremap_reg;
	}

	bt1120->reset_reg = (struct bt1120_reset_reg *)ioremap(gx_bt1120_reg_info.baseAddr[1], gx_bt1120_reg_info.length[1]);
	if (!bt1120->reset_reg) {
		dev_err(&pdev->dev, "ioremap reset_reg error\n");
		goto err_ioremap_reset_reg;
	}

	if (request_irq(gx_bt1120_irq_info.irq[0], bt1120_interrupt,
					gx_bt1120_irq_info.irqFlag[0], "bt1120-interrupt", bt1120) < 0) {
		dev_err(&pdev->dev, "request irq error\n");
		goto err_request_irq;
	}

	ret = of_property_read_u32(np, "i2c_bus", &bt1120->i2c_bus);
	if (ret != 0)
		bt1120->i2c_bus = 0;

	set_sensor_clk_24M(bt1120, pdev);

	return 0;
err_request_irq:
	iounmap(bt1120->reset_reg);
err_ioremap_reset_reg:
	iounmap(bt1120->reg);
err_ioremap_reg:
	for (i = 0; i < gx_bt1120_clk_info.num; i++)
		v4l2_clk_put(bt1120->clk[i]);
	return -1;
}

static int gx_v4l2_dev_register(struct bt1120_dev *bt1120)
{
	struct v4l2_device *v4l2_dev = &bt1120->v4l2_dev;
	int ret = 0;

	strlcpy(v4l2_dev->name, "gx-bt1120", sizeof(v4l2_dev->name));
	v4l2_ctrl_handler_init(&bt1120->hdl, 5);
	v4l2_dev->ctrl_handler = &bt1120->hdl;
	ret = v4l2_device_register(bt1120->dev, v4l2_dev);

	return ret;
}

static int gx_bt1120_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct bt1120_dev *bt1120;
	int ret = 0, i = 0;

	bt1120 = devm_kzalloc(dev, sizeof(struct bt1120_dev), GFP_KERNEL);
	if (!bt1120)
		return -ENOMEM;

	bt1120->dev = dev;
	mutex_init(&bt1120->lock);
	tasklet_init(&bt1120->tasklet, bt1120_update_video_buf, (unsigned long)bt1120);
	bt1120->alloc_ctx = vb2_dma_contig_init_ctx(&pdev->dev);
	if (IS_ERR(bt1120->alloc_ctx))
		return PTR_ERR(bt1120->alloc_ctx);

	ret = gx_bt1120_dt_parse(bt1120, pdev);
	if (ret < 0)
		return ret;

//	gx_bt1120_clk_enable(bt1120);

	platform_set_drvdata(pdev, bt1120);

	ret = gx_v4l2_dev_register(bt1120);
	if (ret < 0) {
		dev_err(&pdev->dev, "register v4l2 device error\n");
		goto err_register_dev;
	}

//	ret = bt1120_register_sensor(bt1120);
//	if (ret < 0) {
//		dev_err(&pdev->dev, "register sensor error\n");
//		goto err_register_sensor;
//	}

	ret = bt1120_register_video_nodes(bt1120);
	if (ret < 0) {
		dev_err(&pdev->dev, "register video nodes error\n");
		goto err_regiseter_nodes;
	}

	bt1120->is_open = false;
//	gx_bt1120_clk_disable(bt1120);

	return 0;

err_regiseter_nodes:
//err_register_sensor:
	v4l2_device_unregister(&bt1120->v4l2_dev);
err_register_dev:
	free_irq(gx_bt1120_irq_info.irq[0], bt1120);
	iounmap(bt1120->reset_reg);
	iounmap(bt1120->reg);
	for (i = 0; i < gx_bt1120_clk_info.num; i++)
		v4l2_clk_put(bt1120->clk[i]);
	return -1;
}

static int gx_bt1120_remove(struct platform_device *pdev)
{
	struct bt1120_dev *bt1120 = platform_get_drvdata(pdev);

	clk_disable_unprepare(sensor_clk);
	iounmap(bt1120->reg);
	iounmap(bt1120->reset_reg);
	free_irq(gx_bt1120_irq_info.irq[0], bt1120);
	v4l2_device_unregister(&bt1120->v4l2_dev);
	video_unregister_device(&bt1120->vdev);

	return 0;
}

static int gx_bt1120_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct bt1120_dev *bt1120 = platform_get_drvdata(pdev);

	if (bt1120->is_open) {
		bt1120_request_stop(bt1120);
		bt1120_stop(bt1120);
		gx_bt1120_clk_disable(bt1120);
	}

	return 0;
}

static int gx_bt1120_resume(struct platform_device *pdev)
{
	struct bt1120_dev *bt1120 = platform_get_drvdata(pdev);

	if (bt1120->is_open) {
		gx_bt1120_clk_enable(bt1120);
		bt1120_request_run(bt1120);
		gx_bt1120_hw_init(bt1120);
	}

	return 0;
}

static const struct of_device_id gx_bt1120_device_match[] = {
	{
		.compatible = "NationalChip,BT1120-Video",
	}, {
	},
};

static struct platform_driver gx_bt1120_driver = {
	.probe  = gx_bt1120_probe,
	.remove = gx_bt1120_remove,
	.suspend = gx_bt1120_suspend,
	.resume = gx_bt1120_resume,
	.driver = {
		.name           = GX_BT1120_DRIVER_NAME,
		.of_match_table = gx_bt1120_device_match,
	}
};

module_platform_driver(gx_bt1120_driver);

MODULE_AUTHOR("ouyangcsh");
MODULE_DESCRIPTION("gx801X SoC camera interface driver");
MODULE_LICENSE("GPL");
