#include "axp152-regu.h"

/*
 * Regulators driver for allwinnertech AXP15X
 *
 * Copyright (C) 2014 allwinnertech Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/regmap.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/module.h>
#include "../axp-core.h"
#include "../axp-regulator.h"
#include "axp152.h"

/* Reverse engineered partly from Platformx drivers */
enum axp_regls {
	VCC_DCDC1,
	VCC_DCDC2,
	VCC_DCDC3,
	VCC_DCDC4,
	VCC_LDO1,
	VCC_LDO2,
	VCC_LDO3,
	VCC_LDO4,
	VCC_LDO5,
	VCC_LDO6,
	VCC_LDO7,
	VCC_152_MAX,
};

struct axp152_regulators {
	struct regulator_dev *regulators[VCC_152_MAX];
	struct axp_dev *chip;
};

static const int axp152_ldo0_table[] = {
	5000, 3300, 2800, 2500
};

static const int axp152_dcdc1_table[] = {
	1700, 1800, 1900, 2000,
	2100, 2400, 2500, 2600,
	2700, 2800, 3000, 3100,
	3200, 3300, 3400, 3500
};

static const int axp152_aldo12_table[] = {
	1200, 1300, 1400, 1500,
	1600, 1700, 1800, 1900,
	2000, 2500, 2700, 2800,
	3000, 3100, 3200, 3300
};

#define AXP152_LDO(_id, min, max, step1, vreg, shift, nbits,\
		ereg, emask, enval, disval, switch_vol, step2, new_level,\
		mode_addr, freq_addr, dvm_ereg, dvm_ebit, dvm_flag)\
	AXP_LDO(AXP152, _id, min, max, step1, vreg, shift, nbits,\
		ereg, emask, enval, disval, switch_vol, step2, new_level,\
		mode_addr, freq_addr, dvm_ereg, dvm_ebit, dvm_flag)

#define AXP152_LDO_SEL(_id, min, max, vreg, shift, nbits,\
		ereg, emask, enval, disval, vtable,\
		mode_addr, freq_addr, dvm_ereg, dvm_ebit, dvm_flag) \
	AXP_LDO_SEL(AXP152, _id, min, max, vreg, shift, nbits,\
		ereg, emask, enval, disval, vtable,\
		mode_addr, freq_addr, dvm_ereg, dvm_ebit, dvm_flag)

#define AXP152_DCDC(_id, min, max, step1, vreg, shift, nbits,\
		ereg, emask, enval, disval, switch_vol, step2, new_level,\
		mode_addr, mode_bit, freq_addr, dvm_ereg, dvm_ebit, dvm_flag) \
	AXP_DCDC(AXP152, _id, min, max, step1, vreg, shift, nbits,\
		ereg, emask, enval, disval, switch_vol, step2, new_level,\
		mode_addr, mode_bit, freq_addr, dvm_ereg, dvm_ebit, dvm_flag)

#define AXP152_DCDC_SEL(_id, min, max, vreg, shift, nbits,\
		ereg, emask, enval, disval, vtable,\
		mode_addr, mode_bit, freq_addr, dvm_ereg, dvm_ebit, dvm_flag) \
	AXP_DCDC_SEL(AXP152, _id, min, max, vreg, shift, nbits,\
		ereg, emask, enval, disval, vtable,\
		mode_addr, mode_bit, freq_addr, dvm_ereg, dvm_ebit, dvm_flag)

static struct axp_regulator_info axp152_regulator_info[] = {
	AXP152_DCDC_SEL(1,  1700, 3500,       DCDC1, 0, 4,  DCDC1EN, 0x80,
		0x80,    0,  axp152_dcdc1,  0x80, 0x08, 0x37, 0, 0, 0),
	AXP152_DCDC(2,       700, 2275, 25,   DCDC2, 0, 6,  DCDC2EN, 0x40,
		0x40,    0,      0, 0, 0,  0x80, 0x04, 0x37, 0x25, 2, 0),
	AXP152_DCDC(3,       700, 3500, 50,   DCDC3, 0, 6,  DCDC3EN, 0x20,
		0x20,    0,      0, 0, 0,  0x80, 0x02, 0x37, 0, 0, 0),
	AXP152_DCDC(4,       700, 3500, 25,   DCDC4, 0, 7,  DCDC4EN, 0x10,
		0x10,    0,      0, 0, 0,  0x80, 0x01, 0x37, 0, 0, 0),
	AXP152_LDO_SEL(0,   2500, 5000,        LDO0, 4, 2,   LDO0EN, 0x80,
		0x80,    0,   axp152_ldo0,     0,    0,    0, 0, 0),
	AXP152_LDO(1,       3100, 3100,  0,     RTC, 0, 0, RTCLDOEN, 0x01,
		0x01,    0,      0, 0, 0,     0,    0,    0, 0, 0),
	AXP152_LDO_SEL(2,   1200, 3300,       ALDO1, 4, 4,  ALDO1EN, 0x08,
		0x08,    0, axp152_aldo12,     0,    0,    0, 0, 0),
	AXP152_LDO_SEL(3,   1200, 3300,       ALDO2, 0, 4,  ALDO2EN, 0x04,
		0x04,    0, axp152_aldo12,     0,    0,    0, 0, 0),
	AXP152_LDO(4,        700, 3500, 100,  DLDO1, 0, 5,  DLDO1EN, 0x02,
		0x02,    0,      0, 0, 0,     0,    0,    0, 0, 0),
	AXP152_LDO(5,        700, 3500, 100,  DLDO2, 0, 5,  DLDO2EN, 0x01,
		0x01,    0,      0, 0, 0,     0,    0,    0, 0, 0),
	AXP152_LDO(IO0,     1800, 3300, 100, LDOIO0, 0, 4,  LDOI0EN, 0x07,
		0x02, 0x07,      0, 0, 0,     0,    0,    0, 0, 0),
};

static struct regulator_init_data axp_regl_init_data[] = {
	[VCC_DCDC1] = {
		.constraints = {
			.name = "axp152_dcdc1",
			.min_uV = 1700000,
			.max_uV = 3500000,
			.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE
				| REGULATOR_CHANGE_STATUS,
			.boot_on=1,
			.always_on=1,
		},
	},
	[VCC_DCDC2] = {
		.constraints = {
			.name = "axp152_dcdc2",
			.min_uV = 700000,
			.max_uV = 2275000,
			.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE
				| REGULATOR_CHANGE_STATUS,
			.boot_on=1,
			.always_on=1,
		},
	},
	[VCC_DCDC3] = {
		.constraints = {
			.name = "axp152_dcdc3",
			.min_uV = 700000,
			.max_uV = 3500000,
			.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE
				| REGULATOR_CHANGE_STATUS,
			.boot_on=1,
			.always_on=1,
		},
	},
	[VCC_DCDC4] = {
		.constraints = {
			.name = "axp152_dcdc4",
			.min_uV = 700000,
			.max_uV = 3500000,
			.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE
				| REGULATOR_CHANGE_STATUS,
			.boot_on=1,
			.always_on=1,
		},
	},
	[VCC_LDO1] = {
		.constraints = {
			.name = "axp152_ldo0",
			.min_uV =  2500000,
			.max_uV =  5000000,
			.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE
				| REGULATOR_CHANGE_STATUS,
			.boot_on=1,
			.always_on=1,
		},
	},
	[VCC_LDO2] = {
		.constraints = {
			.name = "axp152_rtc",
			.min_uV = 3100000,
			.max_uV = 3100000,
		    .boot_on=1,
		    .always_on=1,
		},
	},
	[VCC_LDO3] = {
		.constraints = {
			.name = "axp152_aldo1",
			.min_uV = 1200000,
			.max_uV = 3300000,
			.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE
				| REGULATOR_CHANGE_STATUS,
			.boot_on=1,
			.always_on=1,
		},

	},
	[VCC_LDO4] = {
		.constraints = {
			.name = "axp152_aldo2",
			.min_uV = 1200000,
			.max_uV = 3300000,
			.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE
				| REGULATOR_CHANGE_STATUS,
			.boot_on=1,
			.always_on=1,
		},
	},
	[VCC_LDO5] = {
		.constraints = {
			.name = "axp152_dldo1",
			.min_uV = 700000,
			.max_uV = 3500000,
			.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE
				| REGULATOR_CHANGE_STATUS,
			.boot_on=1,
			.always_on=1,
		},
	},
	[VCC_LDO6] = {
		.constraints = {
			.name = "axp152_dldo2",
			.min_uV = 700000,
			.max_uV = 3500000,
			.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE
				| REGULATOR_CHANGE_STATUS,
		    .boot_on=1,
		    .always_on=1,
		},
	},
	[VCC_LDO7] = {
		.constraints = {
			.name = "axp152_gpioldo",
			.min_uV = 1800000,
			.max_uV = 3300000,
			.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE
				| REGULATOR_CHANGE_STATUS,
			.boot_on=1,
			.always_on=1,
		},
	},
};


static int axp152_regulator_probe(struct platform_device *pdev)
{
	s32 i= 0;
	struct axp_regulator_info *info;
	struct axp152_regulators *regu_data;
	struct axp_dev *axp_dev = dev_get_drvdata(pdev->dev.parent);
    printk("[axp152]Entering %s line:%d\n",__func__,__LINE__);
    printk("[axp152] pointer to of_node is %p in line:%d in %s\n",
                       pdev->dev.of_node,__LINE__,__func__);
	regu_data = devm_kzalloc(&pdev->dev, sizeof(*regu_data),
					GFP_KERNEL);
	if (!regu_data)
		return -ENOMEM;
    printk("[axp152]Entering %s line:%d\n",__func__,__LINE__);

	regu_data->chip = axp_dev;
	platform_set_drvdata(pdev, regu_data);

	for (i = 0; i < VCC_152_MAX; i++) {
		info = &axp152_regulator_info[i];
		info->pmu_num = axp_dev->pmu_num;
		if (info->desc.id == AXP152_ID_LDO4
				|| info->desc.id == AXP152_ID_LDO5
				|| info->desc.id == AXP152_ID_DCDC2
				|| info->desc.id == AXP152_ID_DCDC3
				|| info->desc.id == AXP152_ID_DCDC4
				|| info->desc.id == AXP152_ID_LDO1
				|| info->desc.id == AXP152_ID_LDOIO0) {
			regu_data->regulators[i] = axp_regulator_register(
					&pdev->dev, axp_dev->regmap,
					&axp_regl_init_data[i], info);
		} else if (info->desc.id == AXP152_ID_DCDC1
				|| info->desc.id == AXP152_ID_LDO0
				|| info->desc.id == AXP152_ID_LDO3
				|| info->desc.id == AXP152_ID_LDO2) {
			regu_data->regulators[i] = axp_regulator_sel_register
					(&pdev->dev, axp_dev->regmap,
					&axp_regl_init_data[i], info);
		}

		if (IS_ERR(regu_data->regulators[i])) {
			dev_err(&pdev->dev,
				"failed to register regulator %s\n",
				info->desc.name);
			while (--i >= 0)
				axp_regulator_unregister(
					regu_data->regulators[i]);

			return -1;
		}

	}
   printk("[axp152]Quit func %s in line:%d\n",__func__,__LINE__);
	return 0;
}

static int axp152_regulator_remove(struct platform_device *pdev)
{
	struct axp152_regulators *regu_data = platform_get_drvdata(pdev);
	int i;

	for (i = 0; i < VCC_152_MAX; i++)
		regulator_unregister(regu_data->regulators[i]);

	return 0;
}

static int axp152_regulator_suspend(struct platform_device *pdev,
				pm_message_t state)
{
	struct axp152_regulators *regu_data = platform_get_drvdata(pdev);
	struct axp_dev *axp152 = regu_data->chip;

	/* disable dcdc2 dvm */
	axp_regmap_clr_bits(axp152->regmap, AXP152_DCDC2_DVM_CTRL, 0x04);
	return 0;
}

static int axp152_regulator_resume(struct platform_device *pdev)
{
	struct axp152_regulators *regu_data = platform_get_drvdata(pdev);
	struct axp_dev *axp152 = regu_data->chip;

	/* enable dcdc2 dvm */
	axp_regmap_set_bits(axp152->regmap, AXP152_DCDC2_DVM_CTRL, 0x04);
	return 0;
}


static const struct of_device_id axp152_regu_dt_ids[] = {
	{ .compatible = "axp152-regulator", },
	{},
};
MODULE_DEVICE_TABLE(of, axp152_regu_dt_ids);

static struct platform_driver axp152_regulator_driver = {
	.driver     = {
		.name   = "axp152-regulator",
		.of_match_table = axp152_regu_dt_ids,
	},
	.probe      = axp152_regulator_probe,
	.remove     = axp152_regulator_remove,
	.suspend    = axp152_regulator_suspend,
	.resume     = axp152_regulator_resume,
};

static int __init axp152_regulator_initcall(void)
{
	int ret;

	ret = platform_driver_register(&axp152_regulator_driver);
	if (IS_ERR_VALUE(ret)) {
		pr_err("%s: failed, errno %d\n", __func__, ret);
		return -EINVAL;
	}

	return 0;
}
subsys_initcall(axp152_regulator_initcall);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Qin <qinyongshen@allwinnertech.com>");
MODULE_DESCRIPTION("Regulator Driver for axp152 PMIC");
MODULE_ALIAS("platform:axp152-regulator");
