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
//#include <linux/delay.h>
#include <linux/init.h>
#include <linux/err.h>
//#include <linux/clk.h>
#include <linux/io.h>

static int leo_pmode_cur;
#define NUM_CPUS	1

#define   LEO_CONFIG_ADDRESS      0x0230a000
#define   CONFIG_SOURCE_SEL       0x170
#define   PLL_DDR_CONFIG_BASE     0xe0
#define   PLL_ARM_CONFIG_BASE     0xd0 //0xcc -> 0xd0
#define   CONFIG_CLOCK_DIV_CONFIG 0x24

static struct cpufreq_frequency_table freq_table[] = {
	{ .frequency = 216000000 },
	{ .frequency = 432000000 },
	{ .frequency = 600000000 },
	{ .frequency = CPUFREQ_TABLE_END },
};

static struct param{
	unsigned int   freq;
	unsigned char  clkbp;
	unsigned char  clkod;
	unsigned char  clkn;
	unsigned char  clkm;
} param_table[] = {
	{216000000  , 0  , 3  , 0  , 71 }, // 216MHz
	{432000000  , 0  , 2  , 0  , 71 }, // 432MHz
	{600000000  , 0  , 1  , 0  , 49 }, // 600MHz
	/*{1200000000 , 0  , 0  , 0  , 49 }, // 1.2GHz DTO */
};

void __iomem *g_leo_config_addr_base = NULL;

// fclkout = fvco / (2 ^ clkod)
static void PLL_Config(void * pll, unsigned int freq)
{
	int i;
	volatile unsigned int j = 100;
	for (i=0; i < sizeof(param_table) / sizeof(struct param); i++) {
		if (freq == param_table[i].freq) {
			//unsigned int clkbp = param_table[i].clkbp;
			unsigned int clkod = param_table[i].clkod;
			unsigned int clkn = param_table[i].clkn;
			unsigned int clkm = param_table[i].clkm;
			*(volatile unsigned int*)(pll) = (1<<15)|(1<<14)|(clkod<<12)|(clkn<<7)|clkm;
			while(j--);
			*(volatile unsigned int*)(pll) = (0<<15)|(1<<14)|(clkod<<12)|(clkn<<7)|clkm;
			j=100;
			while(j--);
			*(volatile unsigned int*)(pll) = (0<<15)|(0<<14)|(clkod<<12)|(clkn<<7)|clkm;
		}
	}
	j=100;
	while(j--);
}

static unsigned int PLL_Config_query(void)
{
	volatile unsigned int j;
	unsigned char  clkod;
	unsigned char  clkn;
	unsigned char  clkm;
	void * pll = g_leo_config_addr_base + PLL_ARM_CONFIG_BASE;

	j = *(volatile unsigned int*)(pll);
	clkod = (j>>12)&0xff;
	clkn = (j>>7)&0xf;
	clkm = j&0x3f;

	if (clkn == 0)
	{
		if (clkod == 3 && clkm == 71)
		{
			return 0;
		}
		else if (clkod == 2 && clkm == 71)
		{
			return 1;
		}
		else if (clkod == 1 && clkm == 49)
		{
			return 2;
		}
		else 
			return -1;
	}
	else
		return -1;
	
}

static void cpu_select_pll(int speed_mode)
{
	PLL_Config(g_leo_config_addr_base + PLL_ARM_CONFIG_BASE,freq_table[speed_mode].frequency);
	*(volatile unsigned int*)(g_leo_config_addr_base+CONFIG_SOURCE_SEL) |= 1;
}

static void cpu_select_xtal(void)
{
	*(volatile unsigned int*)(g_leo_config_addr_base+CONFIG_SOURCE_SEL) &= ~0x1;
}

static int leo_switch_freq(int speed_mode)
{
	unsigned long flags;

	local_irq_save(flags);
	cpu_select_xtal();
	cpu_select_pll(speed_mode);
	local_irq_restore(flags);
	leo_pmode_cur = speed_mode;

	return 0;
}

static int leo_query_freq(void)
{
	unsigned long freq_idx;
	freq_idx = PLL_Config_query();

	return freq_idx;
}

static int leo_target(struct cpufreq_policy *policy, unsigned int index)
{
	int ret = 0;
	ret = leo_switch_freq(index);

	return ret;
}

static unsigned int leo_cpufreq_get(unsigned int cpu)
{
	return freq_table[leo_pmode_cur].frequency;
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

	policy->suspend_freq = freq_table[0].frequency;
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
	leo_pmode_cur = -1;
	
	g_leo_config_addr_base = ioremap(LEO_CONFIG_ADDRESS, PAGE_SIZE);
	if (!g_leo_config_addr_base) {
		return -ENOMEM;
	}
	leo_switch_freq(leo_query_freq());
	return cpufreq_register_driver(&leo_cpufreq_driver);
}

static void __exit leo_cpufreq_exit(void)
{
	iounmap(g_leo_config_addr_base);
	cpufreq_unregister_driver(&leo_cpufreq_driver);
}

MODULE_AUTHOR("gx driver team");
MODULE_DESCRIPTION("cpufreq driver for gx leo");
MODULE_LICENSE("GPL");

late_initcall(leo_cpufreq_init);
module_exit(leo_cpufreq_exit);
