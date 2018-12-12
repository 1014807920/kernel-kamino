#include "../axp152/axp152.h"

#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/mfd/core.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/err.h>
#include "../axp-core.h"

static struct axp_dev *axp152_pm_power;
struct axp_config_info axp152_config;
struct wakeup_source *axp152_ws;
static int axp152_pmu_num;

static struct axp_regmap_irq_chip axp152_regmap_irq_chip = {
	.name = "axp152_irq_chip",
	.status_base = AXP152_INTSTS1,
	.enable_base = AXP152_INTEN1,
	.num_regs = 3,

};

static const struct axp_compatible_name_mapping axp152_cn_mapping[] = {
	{
	 .device_name = "axp152",
	 .mfd_name = {
		      .powerkey_name = "axp152-powerkey",
		      .regulator_name = "axp152-regulator",
		      .gpio_name = "axp152-gpio",
		      },
	 },
};

static struct resource axp152_pek_resources[] = {
//{AXP152_IRQ_PEKRE, AXP152_IRQ_PEKRE, "PEK_DBR",   IORESOURCE_IRQ,},
	//{AXP152_IRQ_PEKFE, AXP152_IRQ_PEKFE, "PEK_DBF",   IORESOURCE_IRQ,},
	{AXP152_IRQ_PEKLO, AXP152_IRQ_PEKLO, "PEK_LONG", IORESOURCE_IRQ,},
	{AXP152_IRQ_PEKSH, AXP152_IRQ_PEKSH, "PEK_SHORT", IORESOURCE_IRQ,}
};

static struct resource axp152_gpio_resources[] = {
	{AXP152_IRQ_GPIO0, AXP152_IRQ_GPIO0, "gpio0", IORESOURCE_IRQ,},
	{AXP152_IRQ_GPIO1, AXP152_IRQ_GPIO1, "gpio1", IORESOURCE_IRQ,},
	{AXP152_IRQ_GPIO2, AXP152_IRQ_GPIO2, "gpio2", IORESOURCE_IRQ,},
	{AXP152_IRQ_GPIO3, AXP152_IRQ_GPIO3, "gpio3", IORESOURCE_IRQ,},
};

static struct mfd_cell axp152_cells[] = {
	{
	 .name = "axp152-powerkey",
	 .num_resources = ARRAY_SIZE(axp152_pek_resources),
	 .resources = axp152_pek_resources,
	 .of_compatible = "axp152-powerkey",
	 },
	{
	 .name = "axp152-regulator",
	 .of_compatible = "axp152-regulator",
	 },
	{
	 .name = "axp152-gpio",
	 .num_resources = ARRAY_SIZE(axp152_gpio_resources),
	 .resources = axp152_gpio_resources,
	 .of_compatible = "axp152-gpio",
	 },
};

void axp152_power_off(void)
{
	pr_info("[axp] send power-off command!\n");
	axp_regmap_write(axp152_pm_power->regmap, AXP152_OFF_CTL, 0x80);
}

static int axp152_init_chip(struct axp_dev *axp152)
{
	uint8_t chip_id;
	int err;

	err = axp_regmap_read(axp152->regmap, AXP152_IC_TYPE, &chip_id);
	if (err) {
		pr_err("[%s] try to read chip id failed!\n",
		       axp_name[axp152_pmu_num]);
		return err;
	}

	if ((chip_id & 0xff) == 0x5)
		pr_info("[%s] chip id detect 0x%x !\n",
			axp_name[axp152_pmu_num], chip_id);
	else
		pr_info("[%s] chip id not detect 0x%x !\n",
			axp_name[axp152_pmu_num], chip_id);

	/* enable dcdc2 dvm */
	err = axp_regmap_update(axp152->regmap, AXP152_DCDC2_DVM_CTRL,
				0x4, 0x4);
	if (err) {
		pr_err("[%s] enable dcdc2 dvm failed!\n",
		       axp_name[axp152_pmu_num]);
		return err;
	} else {
		pr_info("[%s] enable dcdc2 dvm.\n", axp_name[axp152_pmu_num]);
	}

	/*init irq wakeup en */
	if (axp152_config.pmu_irq_wakeup)
		axp_regmap_set_bits(axp152->regmap, AXP152_HOTOVER_CTL, 0x80);
	else
		axp_regmap_clr_bits(axp152->regmap, AXP152_HOTOVER_CTL, 0x80);

	/*init pmu over temperature protection */
	if (axp152_config.pmu_hot_shutdown)
		axp_regmap_set_bits(axp152->regmap, AXP152_HOTOVER_CTL, 0x04);
	else
		axp_regmap_clr_bits(axp152->regmap, AXP152_HOTOVER_CTL, 0x04);

	return 0;
}

static void axp152_wakeup_event(void)
{
	__pm_wakeup_event(axp152_ws, 0);
}

static s32 axp152_usb_det(void)
{
	return 0;
}

static s32 axp152_usb_vbus_output(int high)
{
	u8 ret = 0;
#if 0
	if (high)
		ret = axp_regmap_update_sync(axp152_pm_power->regmap,
					     AXP152_GPIO1_CTL, 0x1, 0x7);
	else
		ret = axp_regmap_update_sync(axp152_pm_power->regmap,
					     AXP152_GPIO1_CTL, 0x0, 0x7);
#endif
	return ret;
}

static int axp152_cfg_pmux_para(int num, struct aw_pm_info *api, int *pmu_id)
{
	char name[8];
	struct device_node *np;

	sprintf(name, "pmu%d", num);

	np = of_find_node_by_type(NULL, name);
	if (NULL == np) {
		pr_err("can not find device_type for %s\n", name);
		return -1;
	}

	if (!of_device_is_available(np)) {
		pr_err("can not find node for %s\n", name);
		return -1;
	}
	*pmu_id = axp152_config.pmu_id;

	return 0;
}

static const char *axp152_get_pmu_name(void)
{
	return axp_name[axp152_pmu_num];
}

static struct axp_dev *axp152_get_pmu_dev(void)
{
	return axp152_pm_power;
}

struct axp_platform_ops axp152_platform_ops = {
	.usb_det = axp152_usb_det,
	.usb_vbus_output = axp152_usb_vbus_output,
//      .cfg_pmux_para = axp152_cfg_pmux_para,
	.get_pmu_name = axp152_get_pmu_name,
	.get_pmu_dev = axp152_get_pmu_dev,

};

static const struct i2c_device_id axp152_id_table[] = {
	{"axp152", 0},
	{}
};

static const struct of_device_id axp152_dt_ids[] = {
	{.compatible = "axp152",},
	{},
};

MODULE_DEVICE_TABLE(of, axp152_dt_ids);

static int axp152_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	int ret;
	struct axp_dev *axp152;
	struct device_node *node = client->dev.of_node;

	axp152_pmu_num =
	    axp_get_pmu_num(axp152_cn_mapping, ARRAY_SIZE(axp152_cn_mapping));
	if (axp152_pmu_num < 0) {
		pr_err("%s: get pmu num failed\n", __func__);
		return axp152_pmu_num;
	}

	if (node) {
		/* get dt and sysconfig */
		if (!of_device_is_available(node)) {
			axp152_config.pmu_used = 0;
			pr_err("%s: pmu_used = %u\n", __func__,
			       axp152_config.pmu_used);
			return -EPERM;
		} else {
			axp152_config.pmu_used = 1;
			ret =
			    axp_dt_parse(node, axp152_pmu_num, &axp152_config);
			if (ret) {
				pr_err("%s parse device tree err\n", __func__);
				return -EINVAL;
			}
		}
	} else {
		pr_err("AXP15 device tree err!\n");
		return -EBUSY;
	}

	axp152 = devm_kzalloc(&client->dev, sizeof(*axp152), GFP_KERNEL);
	if (!axp152)
		return -ENOMEM;

	axp152->dev = &client->dev;
	axp152->nr_cells = ARRAY_SIZE(axp152_cells);
	axp152->cells = axp152_cells;
	axp152->pmu_num = axp152_pmu_num;
	axp152->is_slave = axp152_config.pmu_as_slave;
#if 0
	ret = axp_mfd_cell_name_init(&axp152_platform_ops,
				     ARRAY_SIZE(axp152_dt_ids), axp152->pmu_num,
				     axp152->nr_cells, axp152->cells);
	if (ret)
		return ret;
#endif

	axp152->regmap = axp_regmap_init_i2c(&client->dev);
	if (IS_ERR(axp152->regmap)) {
		ret = PTR_ERR(axp152->regmap);
		dev_err(&client->dev, "regmap init failed: %d\n", ret);
		return ret;
	}
	i2c_set_clientdata(client, axp152);

	ret = axp_mfd_add_devices(axp152);
	if (ret) {
		dev_err(axp152->dev, "failed to add MFD devices: %d\n", ret);
		return ret;
	}

	if (axp152->is_slave) {
		axp152->irq_data = axp_irq_chip_register(axp152->regmap,
					client->irq,
					IRQF_SHARED | IRQF_NO_SUSPEND,
					&axp152_regmap_irq_chip,
					NULL);
	} else {
		axp152->irq_data = axp_irq_chip_register(axp152->regmap,
					client->irq,
					IRQF_SHARED | IRQF_NO_SUSPEND,
					&axp152_regmap_irq_chip,
					axp152_wakeup_event);
	}

	if (IS_ERR(axp152->irq_data)) {
		ret = PTR_ERR(axp152->irq_data);
		dev_err(&client->dev, "axp init irq failed: %d\n", ret);
		return ret;
	}

	axp152_pm_power = axp152;

	if (!axp152->is_slave) {
		if (!pm_power_off)
			pm_power_off = axp152_power_off;
	}

	axp_platform_ops_set(axp152->pmu_num, &axp152_platform_ops);

	if (!axp152->is_slave)
		axp152_ws = wakeup_source_register("axp152_wakeup_source");

	ret = axp152_init_chip(axp152);
	if (ret)
		return ret;

	return 0;
}

static int axp152_remove(struct i2c_client *client)
{
	struct axp_dev *axp152 = i2c_get_clientdata(client);

	if (axp152 == axp152_pm_power) {
		axp152_pm_power = NULL;
		pm_power_off = NULL;
	}

	axp_mfd_remove_devices(axp152);
	axp_irq_chip_unregister(client->irq, axp152->irq_data);

	return 0;
}

static struct i2c_driver axp152_driver = {
	.driver = {
		   .name = "axp152",
		   .owner = THIS_MODULE,
		   .of_match_table = axp152_dt_ids,
		   },
	.probe = axp152_probe,
	.remove = axp152_remove,
	.id_table = axp152_id_table,
};

static int __init axp152_i2c_init(void)
{
	int ret;
	ret = i2c_add_driver(&axp152_driver);
	if (ret != 0)
		pr_err("Failed to register axp152 I2C driver: %d\n", ret);
	return ret;
}

arch_initcall(axp152_i2c_init);

static void __exit axp152_i2c_exit(void)
{
	i2c_del_driver(&axp152_driver);
}

module_exit(axp152_i2c_exit);

MODULE_DESCRIPTION("PMIC Driver for AXP15");
MODULE_AUTHOR("Qin <qinyongshen@allwinnertech.com>");
MODULE_LICENSE("GPL");
