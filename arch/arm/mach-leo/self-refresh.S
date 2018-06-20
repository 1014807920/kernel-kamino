/*
 * Copyright (C) 2014-2015 Altera Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <linux/linkage.h>
#include <asm/assembler.h>
#include <asm/asm-offsets.h>

#define MAX_LOOP_COUNT		1000

/*
 * This code assumes that when the bootloader configured
 * the sdram controller for the DDR on the board it
 * configured the following fields depending on the DDR
 */

.arch   armv7-a
.text
.align 3

/*
 * leo_sdram_self_refresh
 *
 *  r0 : sdr_ctl_base_addr
 *  r1 : resume_base_addr
 *
 *  return value: lower 16 bits: loop count going into self refresh
 *                upper 16 bits: loop count exiting self refresh
 */ 
ENTRY(leo_sdram_self_refresh)
	mrc p15, 0, r5, c1, c0, 0
	bic r5, r5, #0x1000
	bic r5, r5, #0x4
	mcr p15, 0, r5, c1, c0, 0 

	CLREX

	ldr     r3, [r1, #0x00]
	ldr     r3, [r0, #0x00]

	/* Enable dynamic clock gating in the Power Control Register. */
	/* maybe to be done*/   
	/* Enable self refresh: xxx */
	/* r0 is sdr controller base addr to be used to enable self refresh code to be done*/
	/* Enable self refresh: set DENALI_CTL_20 = 0x4a000000 */
	ldr	r3, [r0, #0x50]
	ldr	r4, =0x00ffffff
	and	r3, r3, r4
	ldr	r4, =0x6a000000
	orr	r3, r3, r4
	str	r3, [r0, #0x50]

	/* Poll until DENALI_CTL_21 == 0x07 or hit max loops */
	mov	r4, #0
while_ack_0:
	ldr	r3, [r0, #0x54]
	and	r3, r3, #0x3f
	cmp	r3, #0x7
	beq	ack_1

	add	r4, #1
	cmp	r4, #MAX_LOOP_COUNT
	bne	while_ack_0

ack_1:
	nop
	nop
	nop

	/*
	 * Execute an ISB instruction to ensure that all of the
	 * CP15 register changes have been committed.
	 */
	isb

	/*
	 * Execute a barrier instruction to ensure that all cache,
	 * TLB and branch predictor maintenance operations issued
	 * by any CPU in the cluster have completed.
	 */
	dsb
	dmb

	wfi
	nop
	nop
	nop
	nop

	/* Disable self-refresh */
	ldr	r3, [r0, #0x50]
	ldr	r4, =0x00ffffff
	and	r3, r3, r4
	ldr	r4, =0x01000000
	orr	r3, r3, r4
	str	r3, [r0, #0x50]

	/*
	 * run to here means there is pending
	 * wakeup source, system should auto
	 * resume, we need to restore MMDC IO first
	 */
	/* return to suspend finish */
	ret	lr

ENDPROC(leo_sdram_self_refresh)


	/*
	 * The following code must assume it is running from physical address
	 * where absolute virtual addresses to the data section have to be
	 * turned into relative ones.
	 */

ENTRY(v7_cpu_resume)
	bl	v7_invalidate_l1
#ifdef CONFIG_CACHE_L2X0
	bl	l2c310_early_resume
#endif
b	cpu_resume
ENDPROC(v7_cpu_resume)


ENTRY(leo_sdram_self_refresh_sz)
	.word	. - leo_sdram_self_refresh
