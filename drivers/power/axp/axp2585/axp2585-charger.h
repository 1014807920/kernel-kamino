/*
 * drivers/power/supply/bmu/AXP2585-charger.h
 * (C) Copyright 2010-2016
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * Pannan <pannan@allwinnertech.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 */
#ifndef AXP2585_CHARGER_H

#define AXP2585_CHARGER_H
#include "axp2585.h"

/* AXP2585 */
#define AXP2585_CHARGE_STATUS            AXP2585_STATUS
#define AXP2585_CAP                      (0xB9)
#define AXP2585_BATCAP0                  (0xe0)
#define AXP2585_BATCAP1                  (0xe1)
#define AXP2585_RDC0                     (0xba)
#define AXP2585_RDC1                     (0xbb)
#define AXP2585_VLTF_CHARGE              (0x84)
#define AXP2585_VHTF_CHARGE              (0x85)
#define AXP2585_VLTF_WORK                (0x86)
#define AXP2585_VHTF_WORK		 (0x87)
#define AXP2585_CHARGE_CONTROL2		 (0x8c)

#define AXP2585_ADC_CONTROL              (0x80)
#define AXP2585_ADC_BATVOL_ENABLE        (1 << 4)
#define AXP2585_ADC_BATCUR_ENABLE        (1 << 6)
//#define AXP2585_ADC_DCINVOL_ENABLE       (1 << 5)
//#define AXP2585_ADC_DCINCUR_ENABLE       (1 << 4)
#define AXP2585_ADC_DIETMP_ENABLE        (1 << 7)
#define AXP2585_ADC_TSVOL_ENABLE         (1 << 5)

#define AXP2585_VBATH_RES                (0x78)
#define AXP2585_IBATH_REG                (0x7a)
#define AXP2585_DISIBATH_REG             (0x7c)
#define AXP2585_COULOMB_CTL              (0xB8)
#define AXP2585_ADJUST_PARA              (0xE8)
#define AXP2585_ADJUST_PARA1             (0xE9)
#define AXP_CHG_ATTR(_name)                     \
{                                               \
	.attr = { .name = #_name, .mode = 0644 },    \
	.show =  _name##_show,                      \
	.store = _name##_store,                     \
}

#endif
