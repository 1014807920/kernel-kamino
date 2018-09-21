#include <linux/init.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/regulator/machine.h>
#include <linux/i2c.h>
#include <linux/power_supply.h>
#include <linux/module.h>

static struct platform_device virt[] = {
	{
			.name = "reg-152-cs-ldo0",
			.id = -1,
			.dev        = {
				.platform_data = "axp152_ldo0",
			}
	},
	{
			.name = "reg-152-cs-aldo1",
			.id = -1,
			.dev        = {
				.platform_data = "axp152_aldo1",
			}
	},
	{
			.name = "reg-152-cs-aldo2",
			.id = -1,
			.dev        = {
				.platform_data = "axp152_aldo2",
			}
	},
	{
			.name = "reg-152-cs-dldo1",
			.id = -1,
			.dev        = {
				.platform_data = "axp152_dldo1",
			}
	},
	{
			.name = "reg-152-cs-dldo2",
			.id = -1,
			.dev        = {
				.platform_data = "axp152_dldo2",
			}
	},
	{
			.name = "reg-152-cs-dcdc1",
			.id = -1,
			.dev        = {
				.platform_data = "axp152_dcdc1",
			}
	},
	{
			.name = "reg-152-cs-dcdc2",
			.id = -1,
			.dev        = {
				.platform_data = "axp152_dcdc2",
			}
	},
	{
			.name = "reg-152-cs-dcdc3",
			.id = -1,
			.dev        = {
				.platform_data = "axp152_dcdc3",
			}
	},
	{
			.name = "reg-152-cs-dcdc4",
			.id = -1,
			.dev        = {
				.platform_data = "axp152_dcdc4",
			}
	},
	{
			.name = "reg-152-cs-ldoio0",
			.id = -1,
			.dev        = {
				.platform_data = "axp152_gpioldo",
			}
	},
};

static int __init virtual_init(void)
{
	int j, ret;

	for (j = 0; j < ARRAY_SIZE(virt); j++) {
		ret = platform_device_register(&virt[j]);
		if (ret)
				goto create_devices_failed;
	}

	return ret;

create_devices_failed:
	while (j--)
		platform_device_unregister(&virt[j]);
	return ret;

}

module_init(virtual_init);

static void __exit virtual_exit(void)
{
	int j;

	for (j = ARRAY_SIZE(virt) - 1; j >= 0; j--)
		platform_device_unregister(&virt[j]);
}
module_exit(virtual_exit);

MODULE_DESCRIPTION("Axp regulator test");
MODULE_AUTHOR("Kyle Cheung");
MODULE_LICENSE("GPL");
