/*
 * Copyright (C) 2010 Google, Inc.
 *
 * Author:
 *	Colin Cross <ccross@google.com>
 *	Based on arch/arm/plat-omap/cpu-omap.c, (C) 2005 Nokia Corporation
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/cpufreq.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/of_device.h>

#define NUM_CPUS	1
#define KHZ_TO_HZ	1000
#define HZ_TO_KHZ	1000

#define AXI_FREQ (300000 * (KHZ_TO_HZ))
#define AHB_FREQ (150000 * (KHZ_TO_HZ))

#define FREQ_STEPS 4

struct leo_cpufreq {
	struct device_node *np;
	struct clk *cpu_clk;
	struct clk *axi_clk;
	struct clk *ahb_clk;
	unsigned int cur_freq;
};

static struct leo_cpufreq leo_clk_info;
static struct cpufreq_frequency_table freq_table[FREQ_STEPS+1];

static int leo_target(struct cpufreq_policy *policy, unsigned int index)
{
	int ret = 0;
	unsigned long freq_hz = freq_table[index].frequency * KHZ_TO_HZ;

	ret = clk_set_rate(leo_clk_info.cpu_clk,
			clk_round_rate(leo_clk_info.cpu_clk, freq_hz));
	ret |= clk_set_rate(leo_clk_info.axi_clk,
			clk_round_rate(leo_clk_info.axi_clk, AXI_FREQ));
	ret |= clk_set_rate(leo_clk_info.ahb_clk,
			clk_round_rate(leo_clk_info.ahb_clk, AHB_FREQ));
	leo_clk_info.cur_freq = freq_table[index].frequency;

	return ret;
}

static unsigned int leo_cpufreq_get(unsigned int cpu)
{
	return leo_clk_info.cur_freq;
}

static int leo_cpu_init(struct cpufreq_policy *policy)
{
	int ret;
	if (policy->cpu >= NUM_CPUS)
		return -EINVAL;


	/* FIXME: what's the actual transition time? */
	ret = cpufreq_generic_init(policy, freq_table, 300 * 1000);
	if (ret) {
		return ret;
	}

	policy->suspend_freq = freq_table[FREQ_STEPS-1].frequency;
	return 0;
}

static int leo_cpu_exit(struct cpufreq_policy *policy)
{
	return 0;
}

static struct cpufreq_driver leo_cpufreq_driver = {
	.flags			= CPUFREQ_NEED_INITIAL_FREQ_CHECK,
	.verify			= cpufreq_generic_frequency_table_verify,
	.target_index		= leo_target,
	.get			= leo_cpufreq_get,
	.init			= leo_cpu_init,
	.exit			= leo_cpu_exit,
	.name			= "leo-cpufreq",
	.attr			= cpufreq_generic_attr,
	.suspend		= cpufreq_generic_suspend,
};

static int __init leo_cpufreq_init(void)
{
	int i = 0;
	unsigned long freq_hz = 0;

	leo_clk_info.np = of_find_compatible_node(NULL, NULL, "nationalchip,arm-clk");
	if (!leo_clk_info.np) {
		printk("failed to get cpu device node\n");
		return -ENODEV;
	}

	leo_clk_info.cpu_clk = of_clk_get_by_name(leo_clk_info.np, "arm-clk-cpu");
	if (IS_ERR(leo_clk_info.cpu_clk)) {
		printk("Unable to get cpu clk\n");
		return PTR_ERR(leo_clk_info.cpu_clk);
	}

	leo_clk_info.axi_clk = of_clk_get_by_name(leo_clk_info.np, "arm-clk-axi");
	if (IS_ERR(leo_clk_info.axi_clk)) {
		printk("Unable to get axi clk\n");
		return PTR_ERR(leo_clk_info.axi_clk);
	}

	leo_clk_info.ahb_clk = of_clk_get_by_name(leo_clk_info.np, "arm-clk-ahb");
	if (IS_ERR(leo_clk_info.ahb_clk)) {
		printk("Unable to get ahb clk\n");
		return PTR_ERR(leo_clk_info.ahb_clk);
	}

	freq_hz = clk_get_rate(leo_clk_info.cpu_clk);
	leo_clk_info.cur_freq = freq_hz / HZ_TO_KHZ;

	/* Fill freq table */
	for (i = 1; i <= FREQ_STEPS; i++)
		freq_table[i-1].frequency = leo_clk_info.cur_freq / i;
	freq_table[FREQ_STEPS].frequency = CPUFREQ_TABLE_END;

	return cpufreq_register_driver(&leo_cpufreq_driver);
}

static void __exit leo_cpufreq_exit(void)
{
	cpufreq_unregister_driver(&leo_cpufreq_driver);
}

MODULE_AUTHOR("gx driver team");
MODULE_DESCRIPTION("cpufreq driver for gx leo");
MODULE_LICENSE("GPL");

late_initcall(leo_cpufreq_init);
module_exit(leo_cpufreq_exit);
