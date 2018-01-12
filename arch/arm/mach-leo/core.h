/*
 * Copyright 2012 Pavel Machek <pavel@denx.de>
 * Copyright (C) 2012-2015 Altera Corporation
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __MACH_CORE_H
#define __MACH_CORE_H

#define SOCFPGA_RSTMGR_CTRL		0x04
#define SOCFPGA_RSTMGR_MODMPURST	0x10
#define SOCFPGA_RSTMGR_MODPERRST	0x14
#define SOCFPGA_RSTMGR_BRGMODRST	0x1c

/* System Manager bits */
#define RSTMGR_CTRL_SWCOLDRSTREQ	0x1	/* Cold Reset */
#define RSTMGR_CTRL_SWWARMRSTREQ	0x2	/* Warm Reset */

extern void socfpga_init_clocks(void);
extern void socfpga_sysmgr_init(void);

#define LEO_CPU_INT_STAT	0X00
#define LEO_CPU_INT_CLEAN	0X04
#define LEO_CPU_INT_ENABLE	0X08
#define LEO_CPU_MSG_VALUE	0X00
#define LEO_CPU_MSG_FLAGS	0X04

enum reset_mode {
	RESET_REBOOT,
	RESET_POWEROFF,
	RESET_SUSPEND,
	RESET_RESUME,
};

struct gx_pm {
	void __iomem *sdr_ctl_base_addr;
	void __iomem *cpu_int_msg;		// cpu传递消息给mcu通用寄存器
	void __iomem *cpu_int_base;		// cpu传递mcu中断控制器基地址
	int cpu_int_chan;			// reboot/halt对应通道
};

extern struct gx_pm *gx_pm_get(void);
extern void leo_suspend(void);

#ifdef CONFIG_SUSPEND
void v7_cpu_resume(void);
#else
static inline void v7_cpu_resume(void) {}
#endif

u32 leo_sdram_self_refresh(u32 sdr_base);
extern unsigned int leo_sdram_self_refresh_sz;

extern char secondary_trampoline, secondary_trampoline_end;

extern unsigned long socfpga_cpu1start_addr;

#define SOCFPGA_SCU_VIRT_BASE   0xfee00000

#endif
