#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/reboot.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/mfd/core.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_device.h>
#include <linux/err.h>
//#include <linux/power/aw_pm.h>
#include <linux/gpio.h>
#include "../axp-core.h"
#include "../axp-charger.h"
#include "axp2585.h"

static struct axp_dev *axp2585_pm_power;
struct axp_config_info axp2585_config;
struct wakeup_source *axp2585_ws;
static int axp2585_pmu_num;

static const struct axp_compatible_name_mapping axp2585_cn_mapping[] = {
	{
		.device_name = "axp2585",
		.mfd_name = {
#ifndef CONFIG_DUAL_AXP_USED
			.powerkey_name  = "axp2585-powerkey",
#endif
			.charger_name   = "axp2585-charger",
		},
	},
};

static struct axp_regmap_irq_chip axp2585_regmap_irq_chip = {
	.name           = "AXP2585_IRQ_chip",
	.status_base    = AXP2585_INTSTS1,
	.enable_base    = AXP2585_INTEN1,
	.num_regs       = 6,
};

static struct resource axp2585_pek_resources[] = {
	{AXP2585_IRQ_PEKRE,  AXP2585_IRQ_PEKRE,  "PEK_DBR",      IORESOURCE_IRQ,},
	{AXP2585_IRQ_PEKFE,  AXP2585_IRQ_PEKFE,  "PEK_DBF",      IORESOURCE_IRQ,},
};

static struct resource axp2585_charger_resources[] = {
	{AXP2585_IRQ_ACIN,   AXP2585_IRQ_ACIN,   "ac in",        IORESOURCE_IRQ,},
	{AXP2585_IRQ_ACRE,   AXP2585_IRQ_ACRE,   "ac out",       IORESOURCE_IRQ,},
	{AXP2585_IRQ_BATIN,  AXP2585_IRQ_BATIN,  "bat in",       IORESOURCE_IRQ,},
	{AXP2585_IRQ_BATRE,  AXP2585_IRQ_BATRE,  "bat out",      IORESOURCE_IRQ,},
	{AXP2585_IRQ_CHAST,  AXP2585_IRQ_CHAST,  "charging",     IORESOURCE_IRQ,},
	{AXP2585_IRQ_CHAOV,  AXP2585_IRQ_CHAOV,  "charge over",  IORESOURCE_IRQ,},
	{AXP2585_IRQ_LOWN1,  AXP2585_IRQ_LOWN1,  "low warning1", IORESOURCE_IRQ,},
	{AXP2585_IRQ_LOWN2,  AXP2585_IRQ_LOWN2,  "low warning2", IORESOURCE_IRQ,},
#ifdef TYPE_C
	{AXP2585_IRQ_TCIN,  AXP2585_IRQ_TCIN,  "tc in", IORESOURCE_IRQ,},
	{AXP2585_IRQ_TCRE,  AXP2585_IRQ_TCRE, "tc out", IORESOURCE_IRQ,},
#endif
};

static struct mfd_cell axp2585_cells[] = {
	{
		.name          = "axp2585-powerkey",
		.num_resources = ARRAY_SIZE(axp2585_pek_resources),
		.resources     = axp2585_pek_resources,
	},
	{
		.name          = "axp2585-charger",
		.num_resources = ARRAY_SIZE(axp2585_charger_resources),
		.resources     = axp2585_charger_resources,
	},
};

void axp2585_power_off(void)
{
	pr_info("[%s] send power-off command!\n", axp_name[axp2585_pmu_num]);
	axp_regmap_set_bits(axp2585_pm_power->regmap, 0x10, 0x80); /* enable */
	axp_regmap_set_bits(axp2585_pm_power->regmap, 0x17, 0x01);
	mdelay(20);
	pr_warn("[%s] warning!!! axp can't power-off,\"\
		\" maybe some error happened!\n", axp_name[axp2585_pmu_num]);
}

static int axp2585_init_chip(struct axp_dev *axp2585)
{
	uint8_t chip_id;
	int err;

	err = axp_regmap_read(axp2585->regmap, AXP2585_IC_TYPE, &chip_id);
	if (err) {
		pr_err("[%s] try to read chip id failed!\n",
				axp_name[axp2585_pmu_num]);
		return err;
	}

	/*only support axp2585*/
	if (((chip_id & 0xc0) == 0x40) &&
		((chip_id & 0x0f) == 0x06)
		) {
		pr_info("[%s] chip id detect 0x%x !\n",
				axp_name[axp2585_pmu_num], chip_id);
	} else {
		pr_info("[%s] chip id is error 0x%x !\n",
				axp_name[axp2585_pmu_num], chip_id);
	}

	/*Init IRQ wakeup en*/
	if (axp2585_config.pmu_irq_wakeup)
		axp_regmap_set_bits(axp2585->regmap, 0x17, 0x10); /* enable */
	else
		axp_regmap_clr_bits(axp2585->regmap, 0x17, 0x10); /* disable */

	/*Init PMU Over Temperature protection*/
	if (axp2585_config.pmu_hot_shutdown)
		axp_regmap_set_bits(axp2585->regmap, 0xf3, 0x08); /* enable */
	else
		axp_regmap_clr_bits(axp2585->regmap, 0xf3, 0x08); /* disable */

	/*enable send seq to pmu when power off */
	axp_regmap_update(axp2585->regmap, 0x16, 0x40, 0xc0);
	return 0;
}

static void axp2585_wakeup_event(void)
{
	__pm_wakeup_event(axp2585_ws, 0);
}

static s32 axp2585_usb_det(void)
{
	u8 value = 0;
	int ret = 0;

	axp_regmap_read(axp2585_pm_power->regmap, 0x0, &value);
	if (value & 0x02) {
		axp_usb_connect = 1;
		ret = 1;
	}
	return ret;
}

static s32 axp2585_usb_vbus_output(int high)
{
	u8 ret = 0;

	if (high) {
		ret = axp_regmap_set_bits_sync(axp2585_pm_power->regmap,
								0x11, 0x40);
		if (ret)
			return ret;
	} else {
		ret = axp_regmap_clr_bits_sync(axp2585_pm_power->regmap,
								0x11, 0x40);
		if (ret)
			return ret;
	}
	return ret;
}

static int axp2585_cfg_pmux_para(int num, struct aw_pm_info *api, int *pmu_id)
{
	char name[8];
	struct device_node *np;

	sprintf(name, "pmu%d", num);

	np = of_find_node_by_type(NULL, name);
	if (np == NULL) {
		pr_err("can not find device_type for %s\n", name);
		return -1;
	}

	if (!of_device_is_available(np)) {
		pr_err("can not find node for %s\n", name);
		return -1;
	}
#if 0
#ifdef CONFIG_AXP_TWI_USED
	if (api != NULL) {
		api->pmu_arg.twi_port = axp2585_pm_power->regmap->client->adapter->nr;
		api->pmu_arg.dev_addr = axp2585_pm_power->regmap->client->addr;
	}
#endif
#endif
	*pmu_id = axp2585_config.pmu_id;

	return 0;
}

static const char *axp2585_get_pmu_name(void)
{
	return axp_name[axp2585_pmu_num];
}

static struct axp_dev *axp2585_get_pmu_dev(void)
{
	return axp2585_pm_power;
}

struct axp_platform_ops axp2585_platform_ops = {
	.usb_det = axp2585_usb_det,
	.usb_vbus_output = axp2585_usb_vbus_output,
	//.cfg_pmux_para = axp2585_cfg_pmux_para,
	.get_pmu_name = axp2585_get_pmu_name,
	.get_pmu_dev  = axp2585_get_pmu_dev,
};

static const struct i2c_device_id axp2585_id_table[] = {
	{ "axp2585", 0 },
	{}
};

static const struct of_device_id axp2585_dt_ids[] = {
	{ .compatible = "axp2585", },
	{},
};
MODULE_DEVICE_TABLE(of, axp2585_dt_ids);


static int axp2585_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
    printk("[axp2585]Entering %s\n",__func__);
	int ret;
	struct axp_dev *axp2585;

	struct device *device = &client->dev;
	struct device_node *node = client->dev.of_node;

	axp2585_pmu_num = axp_get_pmu_num(axp2585_cn_mapping,
				ARRAY_SIZE(axp2585_cn_mapping));
	if (axp2585_pmu_num < 0) {
		pr_err("%s get pmu num failed\n", __func__);
		return axp2585_pmu_num;
	}

	if (node) {
		/* get dt and sysconfig */
		if (!of_device_is_available(node)) {
			axp2585_config.pmu_used = 0;
			pr_err("%s: pmu_used = %u\n", __func__,
					axp2585_config.pmu_used);
			return -EPERM;
		} else {
			axp2585_config.pmu_used = 1;
			ret = axp_dt_parse(node, axp2585_pmu_num,
					&axp2585_config);
			if (ret) {
				pr_err("%s parse device tree err\n", __func__);
				return -EINVAL;
			}
		}
	} else {
		pr_err("AXP2585 device tree err!\n");
		return -EBUSY;
	}

	axp2585 = devm_kzalloc(device, sizeof(*axp2585), GFP_KERNEL);
	if (!axp2585)
		return -ENOMEM;

	axp2585->dev = device;
	axp2585->nr_cells = ARRAY_SIZE(axp2585_cells);
	axp2585->cells = axp2585_cells;
	axp2585->pmu_num = axp2585_pmu_num;

	ret = axp_mfd_cell_name_init(axp2585_cn_mapping,
				ARRAY_SIZE(axp2585_cn_mapping), axp2585->pmu_num,
				axp2585->nr_cells, axp2585->cells);
	if (ret)
		return ret;

	axp2585->regmap = axp_regmap_init_i2c(&client->dev);

	if (IS_ERR(axp2585->regmap)) {
		ret = PTR_ERR(axp2585->regmap);
		dev_err(device, "regmap init failed: %d\n", ret);
		return ret;
	}

	i2c_set_clientdata(client, axp2585);
  printk("==%s==line:%d==file:%s==\n",__func__,__LINE__,__FILE__);
	ret = axp2585_init_chip(axp2585);
	if (ret)
		return ret;
	  printk("==%s==line:%d==file:%s==\n",__func__,__LINE__,__FILE__);

	ret = axp_mfd_add_devices(axp2585);
	if (ret) {
		dev_err(axp2585->dev, "failed to add MFD devices: %d\n", ret);
		return ret;
	}
	printk("==%s==line:%d==file:%s==\n",__func__,__LINE__,__FILE__);
	axp2585->irq = client->irq;
printk("[axp2585] client->irq=%d in line:%d of %s\n",client->irq,__LINE__,__func__);

	axp2585->irq_data = axp_irq_chip_register(axp2585->regmap, axp2585->irq,
						IRQF_SHARED
						| IRQF_NO_SUSPEND,
						&axp2585_regmap_irq_chip,
						axp2585_wakeup_event);
	if (IS_ERR(axp2585->irq_data)) {
		ret = PTR_ERR(axp2585->irq_data);
		dev_err(device, "axp init irq failed: %d\n", ret);
		return ret;
	}
	printk("%s %d:axp2585->irq_data pointer:%p\n",__func__,__LINE__,axp2585->irq_data);
	//if(axp2585->irq_data->chip!=NULL)
	//{
	//    printk("[axp2585]axp2585->irq_data->chip->name=%s\n",axp2585->irq_data->chip->name);
//	}
printk("[%s]===%s===line:%d=========\n",__FILE__,__func__,__LINE__);
printk("[axp2585]pointer to axp2585->irq_data->irqs is %p in %s line:%d\n",\
       axp2585->irq_data->irqs,__func__,__LINE__);
printk("[axp2585]pointer to axp2585->irq_data is %p in %s line:%d\n",\
       axp2585->irq_data,__func__,__LINE__);
printk("[%s]===%s===line:%d=========\n",__FILE__,__func__,__LINE__);

	axp2585_pm_power = axp2585;
/*
	if (!pm_power_off)
		pm_power_off = axp2585_power_off;
*/
	axp_platform_ops_set(axp2585->pmu_num, &axp2585_platform_ops);
	printk("[%s]===%s===line:%d=========\n",__FILE__,__func__,__LINE__);

	axp2585_ws = wakeup_source_register("axp2585_wakeup_source");
printk("Quit %s\n",__func__);
	return 0;
}


static int axp2585_remove(struct i2c_client *client)
{
	struct axp_dev *axp2585 = i2c_get_clientdata(client);

	if (axp2585 == axp2585_pm_power) {
		axp2585_pm_power = NULL;
		pm_power_off = NULL;
	}
	axp_mfd_remove_devices(axp2585);
	axp_irq_chip_unregister(axp2585->irq, axp2585->irq_data);

	return 0;
}

/*
 * enable restart pmic when pwrok drive low
 */

static void axp2585_shutdown(struct i2c_client *client)

{
	axp_regmap_set_bits(axp2585_pm_power->regmap, 0x14, 0x40);
}


static struct i2c_driver axp2585_driver = {

	.driver = {
		.name   = "axp2585",
		.owner  = THIS_MODULE,
		.of_match_table = axp2585_dt_ids,
	},
	.probe      = axp2585_probe,
	.remove     = axp2585_remove,
	.shutdown   = axp2585_shutdown,
	.id_table   = axp2585_id_table,

};

static int __init axp2585_init(void)
{
	int ret;
	ret = i2c_add_driver(&axp2585_driver);
	if (ret != 0)
		pr_err("[axp2585]Failed to register axp2585 I2C driver: %d\n", ret);
	return ret;
}
subsys_initcall(axp2585_init);
//arch_initcall(axp2585_init);

static void __exit axp2585_exit(void)
{
	i2c_del_driver(&axp2585_driver);
}
module_exit(axp2585_exit);

MODULE_DESCRIPTION("BMU Driver for axp2585");
MODULE_AUTHOR("roy <qiuzhigang@allwinnertech.com>");
MODULE_LICENSE("GPL");
