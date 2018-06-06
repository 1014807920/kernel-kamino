/*
 *  Copyright (C) 2012-2015 Altera Corporation
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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <linux/irqchip.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/reboot.h>

#include <asm/hardware/cache-l2x0.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/cacheflush.h>

#include "core.h"
static struct gx_pm pm_info;

inline struct gx_pm *gx_pm_get(void)
{
	return &pm_info;
}

void do_gx_pm(enum reset_mode mode)
{
	u32 tmp;
	struct gx_pm *pm = gx_pm_get();

	writel(mode, pm->cpu_int_msg_flag);

	tmp  = readl(pm->cpu_int_base + LEO_CPU_INT_ENABLE);
	tmp |= 1 << pm->cpu_int_chan;
	writel(tmp, pm->cpu_int_base + LEO_CPU_INT_ENABLE);
}

static void leo_power_off(void)
{
	do_gx_pm(RESET_POWEROFF);
}

void __init leo_sysmgr_init(void)
{
	struct device_node *np;
	struct gx_pm *pm = gx_pm_get();

	np = of_find_compatible_node(NULL, NULL, "nationalchip,sdr-ctl");
	pm->sdr_ctl_base_addr = of_iomap(np, 0);

	np = of_find_compatible_node(NULL, NULL, "nationalchip,gx_pm");
	pm->cpu_int_base     = of_iomap(np, 2);
	pm->cpu_int_msg      = of_iomap(np, 0);
	pm->cpu_int_msg_flag = of_iomap(np, 1);

	if(of_property_read_u32(np, "cpu_int_chan", &pm->cpu_int_chan) < 0)
		pm->cpu_int_chan = -1;

	pm_power_off = leo_power_off;
}

static void __init leo_init_irq(void)
{
	irqchip_init();
	leo_sysmgr_init();
}

static void leo_restart(enum reboot_mode mode, const char *cmd)
{
	leo_pm_mode_enter(RESET_REBOOT);
}

static const char *nationalchip_dt_match[] = {
	"nationalchip,leo",
	NULL
};

DT_MACHINE_START(leo, "nationalchip leo")
	.l2c_aux_val	= 0,
	.l2c_aux_mask	= ~0,
	.init_irq	= leo_init_irq,
	.restart	= leo_restart,
	.dt_compat	= nationalchip_dt_match,
MACHINE_END

