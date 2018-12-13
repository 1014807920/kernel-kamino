#include "axp2585-charger.h"

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/power_supply.h>
#include "../axp-core.h"
#include "../axp-charger.h"

static int axp2585_get_ac_voltage(struct axp_charger_dev *cdev)
{
	return 0;
}

static int axp2585_get_ac_current(struct axp_charger_dev *cdev)
{
	return 0;
}

static int axp2585_set_ac_vhold(struct axp_charger_dev *cdev, int vol)
{
	return 0;
}

static int axp2585_get_ac_vhold(struct axp_charger_dev *cdev)
{
	return 0;
}

static int axp2585_set_ac_ihold(struct axp_charger_dev *cdev, int cur)
{
	return 0;
}

static int axp2585_get_ac_ihold(struct axp_charger_dev *cdev)
{
	return 0;
}

static struct axp_ac_info axp2585_ac_info = {
	.det_bit = 1,
	.det_offset = 0,
	.valid_offset = 0,
	.valid_bit = 1,
	.get_ac_voltage = axp2585_get_ac_voltage,
	.get_ac_current = axp2585_get_ac_current,
	.set_ac_vhold = axp2585_set_ac_vhold,
	.get_ac_vhold = axp2585_get_ac_vhold,
	.set_ac_ihold = axp2585_set_ac_ihold,
	.get_ac_ihold = axp2585_get_ac_ihold,
};

static int axp2585_get_usb_voltage(struct axp_charger_dev *cdev)
{
	return 0;
}

static int axp2585_get_usb_current(struct axp_charger_dev *cdev)
{
	return 0;
}

static int axp2585_set_usb_vhold(struct axp_charger_dev *cdev, int vol)
{
	u8 tmp;
	struct axp_regmap *map = cdev->chip->regmap;

	if (vol) {
		/*axp_regmap_set_bits(map, 0xff,0x60); */
		if (vol >= 3880 && vol <= 5080) {
			tmp = (vol - 3880) / 80;
			axp_regmap_update(map, 0x11, tmp, 0x0f);
		} else {
			pr_err("set usb limit voltage error, %d mV\n",
			       axp2585_config.pmu_usbpc_vol);
		}
	} else {
		/*axp_regmap_clr_bits(map, 0xff,0x60); */
	}
	return 0;
}

static int axp2585_get_usb_vhold(struct axp_charger_dev *cdev)
{
	u8 tmp;
	struct axp_regmap *map = cdev->chip->regmap;

	axp_regmap_read(map, 0x11, &tmp);
	return (tmp * 80 + 3880);
}

static int axp2585_set_usb_ihold(struct axp_charger_dev *cdev, int cur)
{
	u8 tmp;
	struct axp_regmap *map = cdev->chip->regmap;

	if (cur) {
		/*axp_regmap_set_bits(map, 0xff,0x60); */
		if (cur >= 100 && cur <= 3250) {
			tmp = (cur - 100) / 50;
			axp_regmap_update(map, 0x10, tmp, 0x3f);
		} else {
			pr_err("set usb limit voltage error, %d mV\n",
			       axp2585_config.pmu_usbpc_vol);
		}
	} else {
		/*axp_regmap_clr_bits(map, 0xff,0x60); */
	}
	return 0;
}

static int axp2585_get_usb_ihold(struct axp_charger_dev *cdev)
{
	u8 tmp;
	struct axp_regmap *map = cdev->chip->regmap;

	axp_regmap_read(map, 0x10, &tmp);
	return (tmp * 50 + 100);
}

static struct axp_usb_info axp2585_usb_info = {
	.det_bit = 1,
	.det_offset = 0,
	.valid_offset = 0,
	.valid_bit = 1,
	.get_usb_voltage = axp2585_get_usb_voltage,
	.get_usb_current = axp2585_get_usb_current,
	.set_usb_vhold = axp2585_set_usb_vhold,
	.get_usb_vhold = axp2585_get_usb_vhold,
	.set_usb_ihold = axp2585_set_usb_ihold,
	.get_usb_ihold = axp2585_get_usb_ihold,
};

static int axp2585_get_rest_cap(struct axp_charger_dev *cdev)
{
	u8 val, temp_val[2], tmp[2];
	u8 ocv_percent = 0;
	u8 coulomb_percent = 0;
	int batt_max_cap, coulumb_counter;
	int rest_vol;
	struct axp_regmap *map = cdev->chip->regmap;

	axp_regmap_read(map, AXP2585_CAP, &val);
	if (!(val & 0x80))
		return 0;
	rest_vol = (int)(val & 0x7F);
	axp_regmap_read(map, 0xe4, &tmp[0]);
	if (tmp[0] & 0x80) {
		ocv_percent = tmp[0] & 0x7f;
		AXP_DEBUG(AXP_SPLY, cdev->chip->pmu_num,
			  "ocv_percent = %d\n", ocv_percent);
	}
	axp_regmap_read(map, 0xe5, &tmp[0]);
	if (tmp[0] & 0x80) {
		coulomb_percent = tmp[0] & 0x7f;
		AXP_DEBUG(AXP_SPLY, cdev->chip->pmu_num,
			  "coulomb_percent = %d\n", coulomb_percent);
	}
	if (ocv_percent == 100 && cdev->charging == 0 && rest_vol == 99
	    && (cdev->ac_valid == 1 || cdev->usb_valid == 1)) {
		axp_regmap_clr_bits(map, AXP2585_COULOMB_CTL, 0x80);
		axp_regmap_set_bits(map, AXP2585_COULOMB_CTL, 0x80);
		AXP_DEBUG(AXP_SPLY, cdev->chip->pmu_num, "Reset coulumb\n");
		rest_vol = 100;
	}
	axp_regmap_reads(map, 0xe2, 2, temp_val);
	coulumb_counter = (((temp_val[0] & 0x7f) << 8) + temp_val[1])
	    * 1456 / 1000;
	AXP_DEBUG(AXP_SPLY, cdev->chip->pmu_num,
			  "coulumb_counter = %d\n", coulumb_counter);

	axp_regmap_reads(map, 0xe0, 2, temp_val);
	batt_max_cap = (((temp_val[0] & 0x7f) << 8) + temp_val[1])
	    * 1456 / 1000;

	AXP_DEBUG(AXP_SPLY, cdev->chip->pmu_num,
		  "batt_max_cap = %d\n", batt_max_cap);
	return rest_vol;
}

static int axp2585_get_bat_health(struct axp_charger_dev *cdev)
{
	return POWER_SUPPLY_HEALTH_GOOD;
}

static inline int axp2585_vbat_to_mV(u32 reg)
{
	return ((int)(((reg >> 8) << 4) | (reg & 0x000F))) * 1200 / 1000;
}

static int axp2585_get_vbat(struct axp_charger_dev *cdev)
{
	u8 tmp[2];
	u32 res;
	struct axp_regmap *map = cdev->chip->regmap;

	axp_regmap_reads(map, AXP2585_VBATH_RES, 2, tmp);
	res = (tmp[0] << 8) | tmp[1];

	return axp2585_vbat_to_mV(res);
}

static inline int axp2585_ibat_to_mA(u32 reg)
{
	return (int)((((reg >> 8) << 4) | (reg & 0x000F)) << 1);
}

static inline int axp2585_icharge_to_mA(u32 reg)
{
	return (int)((((reg >> 8) << 4) | (reg & 0x000F)) << 1);
}

static int axp2585_get_ibat(struct axp_charger_dev *cdev)
{
	u8 tmp[2];
	u32 res;
	struct axp_regmap *map = cdev->chip->regmap;

	axp_regmap_reads(map, AXP2585_IBATH_REG, 2, tmp);
	res = (tmp[0] << 8) | tmp[1];

	return axp2585_icharge_to_mA(res);
}

static int axp2585_get_disibat(struct axp_charger_dev *cdev)
{
	u8 tmp[2];
	u32 dis_res;
	struct axp_regmap *map = cdev->chip->regmap;

	axp_regmap_reads(map, AXP2585_DISIBATH_REG, 2, tmp);
	dis_res = (tmp[0] << 8) | tmp[1];

	return axp2585_ibat_to_mA(dis_res);
}

static int axp2585_set_chg_cur(struct axp_charger_dev *cdev, int cur)
{
	uint8_t tmp = 0;
	struct axp_regmap *map = cdev->chip->regmap;
/*
	if (cur == 0)
		axp_regmap_clr_bits(map, axp2585_CHARGE_CONTROL1, 0x80);
	else
		axp_regmap_set_bits(map, axp2585_CHARGE_CONTROL1, 0x80);
*/
	if (cur >= 0 && cur <= 3072) {

		tmp = (cur) / 64;
		if (tmp > 0x3f)
			tmp = 0x3f;
		axp_regmap_update(map, 0x8b, tmp, 0x3F);
	}
	return 0;
}

static int axp2585_set_chg_vol(struct axp_charger_dev *cdev, int vol)
{
	uint8_t tmp = 0;
	struct axp_regmap *map = cdev->chip->regmap;

	if (vol > 3840 && vol < 4608)
		tmp = (vol - 3840) / 16;
	else {
		pr_warn("unsupported voltage: %dmv, use default 4200mv\n", vol);
		tmp = (4200 - 3840) / 16;
	}
	axp_regmap_update(map, 0x8c, tmp << 2, 0xfc);
	return 0;
}

static struct axp_battery_info axp2585_batt_info = {
	.chgstat_bit = 2,	//2--4
	.chgstat_offset = 0,
	.det_bit = 4,
	.det_offset = 2,
	.det_valid_bit = 3,
	.det_valid = 1,
	.cur_direction_bit = 0,
	.cur_direction_offset = 2,
	.get_rest_cap = axp2585_get_rest_cap,
	.get_bat_health = axp2585_get_bat_health,
	.get_vbat = axp2585_get_vbat,
	.get_ibat = axp2585_get_ibat,
	.get_disibat = axp2585_get_disibat,
	.set_chg_cur = axp2585_set_chg_cur,
	.set_chg_vol = axp2585_set_chg_vol,
};

static struct power_supply_info battery_data = {
	.name = "PTI PL336078",
	.technology = POWER_SUPPLY_TECHNOLOGY_LiFe,
	.voltage_max_design = 4200000,
	.voltage_min_design = 3500000,
	.use_for_apm = 1,
};

#ifdef TYPE_C
static struct axp_tc_info axp2585_tc_info = {
	.det_bit = 2,		/*2--4 */
};
#endif
static struct axp_supply_info axp2585_spy_info = {
	.ac = &axp2585_ac_info,
	.usb = &axp2585_usb_info,
	.batt = &axp2585_batt_info,
#ifdef TYPE_C
	.tc = &axp2585_tc_info,
#endif
};

void axp2585_bc_detect_init(struct axp_dev *axp_dev)
{
	struct axp_regmap *map = axp_dev->regmap;
	u8 val = 0, count;

	axp_regmap_read(map, AXP2585_STATUS2, &val);
	val = (val & 0xe0) >> 5;

	if (val == BC_SDP) {

		axp_usbcur(CHARGE_USB_20);
		axp_usbvol(CHARGE_USB_20);

	} else {

		axp_usbcur(CHARGE_AC);
		axp_usbvol(CHARGE_AC);

	}
	switch (val) {
	case BC_SDP:
		if (axp2585_config.pmu_SDP_vbus_currentlimit >= 100
		    && axp2585_config.pmu_SDP_vbus_currentlimit <= 3250) {
			count =
			    ((axp2585_config.pmu_SDP_vbus_currentlimit -
			      100) / 50);
			axp_regmap_update(map, AXP2585_INPUT_CURRENTLIMIT,
					  count, 0x3f);

		} else {
			pr_err("set Vbus limit voltage error, %d mV\n",
			       axp2585_config.pmu_SDP_vbus_currentlimit);

		}

		if (axp2585_config.pmu_SDP_Cbat_currentlimit >= 0
		    && axp2585_config.pmu_SDP_Cbat_currentlimit <= 3072) {
			count =
			    ((axp2585_config.pmu_SDP_Cbat_currentlimit -
			      0) / 64);
			axp_regmap_update(map, AXP2585_CHARGER_CONTROL2, count,
					  0x3f);

		} else {
			pr_err("set usb limit voltage error, %d mV\n",
			       axp2585_config.pmu_SDP_Cbat_currentlimit);

		}

		break;
	case BC_CDP:
		if (axp2585_config.pmu_CDP_vbus_currentlimit >= 100
		    && axp2585_config.pmu_CDP_vbus_currentlimit <= 3250) {
			count =
			    ((axp2585_config.pmu_CDP_vbus_currentlimit -
			      100) / 50);
			axp_regmap_update(map, AXP2585_INPUT_CURRENTLIMIT,
					  count, 0x3f);

		} else {
			pr_err("set Vbus limit voltage error, %d mV\n",
			       axp2585_config.pmu_CDP_vbus_currentlimit);

		}

		if (axp2585_config.pmu_CDP_Cbat_currentlimit >= 0
		    && axp2585_config.pmu_CDP_Cbat_currentlimit <= 3072) {
			count =
			    ((axp2585_config.pmu_CDP_Cbat_currentlimit -
			      0) / 64);
			axp_regmap_update(map, AXP2585_CHARGER_CONTROL2, count,
					  0x3f);

		} else {
			pr_err("set usb limit voltage error, %d mV\n",
			       axp2585_config.pmu_CDP_Cbat_currentlimit);

		}
		break;
	case BC_RESULT_DCP:
		if (axp2585_config.pmu_DCP_vbus_currentlimit >= 100
		    && axp2585_config.pmu_DCP_vbus_currentlimit <= 3250) {
			count =
			    ((axp2585_config.pmu_DCP_vbus_currentlimit -
			      100) / 50);
			axp_regmap_update(map, AXP2585_INPUT_CURRENTLIMIT,
					  count, 0x3f);

		} else {
			pr_err("set Vbus limit voltage error, %d mV\n",
			       axp2585_config.pmu_DCP_vbus_currentlimit);

		}

		if (axp2585_config.pmu_DCP_Cbat_currentlimit >= 0
		    && axp2585_config.pmu_DCP_Cbat_currentlimit <= 3072) {
			count =
			    ((axp2585_config.pmu_DCP_Cbat_currentlimit -
			      0) / 64);
			axp_regmap_update(map, AXP2585_CHARGER_CONTROL2, count,
					  0x3f);

		} else {
			pr_err("set usb limit voltage error, %d mV\n",
			       axp2585_config.pmu_DCP_Cbat_currentlimit);

		}
		break;

	}
}

static int axp2585_charger_init(struct axp_dev *axp_dev)
{
	u8 ocv_cap[32];
	u8 val = 0;
	int cur_coulomb_counter, rdc;
	struct axp_regmap *map = axp_dev->regmap;
	int i, ocv_cou_adjust_time[4] = { 60, 120, 15, 30 };
	int update_min_times[8] = { 30, 60, 120, 164, 0, 5, 10, 20 };

	/*set chg time */
	if (axp2585_config.pmu_init_chg_pretime < 40)
		axp2585_config.pmu_init_chg_pretime = 40;
	val = (axp2585_config.pmu_init_chg_pretime - 40) / 10;
	if (val >= 3)
		val = 3;
	val = 0x80 + (val << 5);
	axp_regmap_update(map, 0x8e, val, 0xe0);

	if (axp2585_config.pmu_init_chg_csttime <= 60 * 5)
		val = 0;
	else if (axp2585_config.pmu_init_chg_csttime <= 60 * 8)
		val = 1;
	else if (axp2585_config.pmu_init_chg_csttime <= 60 * 12)
		val = 2;
	else if (axp2585_config.pmu_init_chg_csttime <= 60 * 20)
		val = 3;
	else
		val = 3;
	val = (val << 1) + 0x01;
	axp_regmap_update(map, 0x8d, val, 0x07);
	/* adc set */
	val = AXP2585_ADC_BATVOL_ENABLE | AXP2585_ADC_BATCUR_ENABLE;
	if (axp2585_config.pmu_bat_temp_enable != 0)
		val = val | AXP2585_ADC_TSVOL_ENABLE;
	axp_regmap_update(map, AXP2585_ADC_CONTROL, val,
			  AXP2585_ADC_BATVOL_ENABLE
			  | AXP2585_ADC_BATCUR_ENABLE
			  | AXP2585_ADC_TSVOL_ENABLE);

	axp_regmap_read(map, AXP2585_TS_PIN_CONTROL, &val);
	switch (axp2585_config.pmu_init_adc_freq / 100) {
	case 1:
		val &= ~(3 << 5);
		break;
	case 2:
		val &= ~(3 << 5);
		val |= 1 << 5;
		break;
	case 4:
		val &= ~(3 << 5);
		val |= 2 << 5;
		break;
	case 8:
		val |= 3 << 5;
		break;
	default:
		break;
	}

	if (axp2585_config.pmu_bat_temp_enable != 0)
		val &= (~(1 << 7));
	axp_regmap_write(map, AXP2585_TS_PIN_CONTROL, val);

	/* bat para */
	axp_regmap_write(map, AXP2585_WARNING_LEVEL,
			 ((axp2585_config.pmu_battery_warning_level1 - 5) << 4)
			 + axp2585_config.pmu_battery_warning_level2);

	if (axp2585_config.pmu_init_chgvol < 3840)
		axp2585_config.pmu_init_chgvol = 3840;
	val = (axp2585_config.pmu_init_chgvol - 3840) / 16;
	if (val > 0x30)
		val = 0x30;
	val <<= 2;
	axp_regmap_update(map, AXP2585_CHARGE_CONTROL3, val, 0xfc);

	ocv_cap[0] = axp2585_config.pmu_bat_para1;
	ocv_cap[1] = axp2585_config.pmu_bat_para2;
	ocv_cap[2] = axp2585_config.pmu_bat_para3;
	ocv_cap[3] = axp2585_config.pmu_bat_para4;
	ocv_cap[4] = axp2585_config.pmu_bat_para5;
	ocv_cap[5] = axp2585_config.pmu_bat_para6;
	ocv_cap[6] = axp2585_config.pmu_bat_para7;
	ocv_cap[7] = axp2585_config.pmu_bat_para8;
	ocv_cap[8] = axp2585_config.pmu_bat_para9;
	ocv_cap[9] = axp2585_config.pmu_bat_para10;
	ocv_cap[10] = axp2585_config.pmu_bat_para11;
	ocv_cap[11] = axp2585_config.pmu_bat_para12;
	ocv_cap[12] = axp2585_config.pmu_bat_para13;
	ocv_cap[13] = axp2585_config.pmu_bat_para14;
	ocv_cap[14] = axp2585_config.pmu_bat_para15;
	ocv_cap[15] = axp2585_config.pmu_bat_para16;
	ocv_cap[16] = axp2585_config.pmu_bat_para17;
	ocv_cap[17] = axp2585_config.pmu_bat_para18;
	ocv_cap[18] = axp2585_config.pmu_bat_para19;
	ocv_cap[19] = axp2585_config.pmu_bat_para20;
	ocv_cap[20] = axp2585_config.pmu_bat_para21;
	ocv_cap[21] = axp2585_config.pmu_bat_para22;
	ocv_cap[22] = axp2585_config.pmu_bat_para23;
	ocv_cap[23] = axp2585_config.pmu_bat_para24;
	ocv_cap[24] = axp2585_config.pmu_bat_para25;
	ocv_cap[25] = axp2585_config.pmu_bat_para26;
	ocv_cap[26] = axp2585_config.pmu_bat_para27;
	ocv_cap[27] = axp2585_config.pmu_bat_para28;
	ocv_cap[28] = axp2585_config.pmu_bat_para29;
	ocv_cap[29] = axp2585_config.pmu_bat_para30;
	ocv_cap[30] = axp2585_config.pmu_bat_para31;
	ocv_cap[31] = axp2585_config.pmu_bat_para32;
	//axp_regmap_writes(map, 0xC0, 32, ocv_cap);
	for (i = 0; i < 32; i++)
		axp_regmap_write(map, 0xC0 + i, ocv_cap[i]);

	/*Init CHGLED function */
	if (axp2585_config.pmu_chgled_func)
		axp_regmap_set_bits(map, 0x90, 0x80);	/* control by charger */
	else
		axp_regmap_clr_bits(map, 0x90, 0x80);	/* drive MOTO */
#if 0
	/*set CHGLED Indication Type */
	if (axp2585_config.pmu_chgled_type)
		axp_regmap_set_bits(map, 0x90, 0x01);	/* Type B */
	else
		axp_regmap_clr_bits(map, 0x90, 0x07);	/* Type A */
#else
	axp_regmap_set_bits(map, 0x90, axp2585_config.pmu_chgled_type & 0x07);
#endif
	/*Init battery capacity correct function */
	if (axp2585_config.pmu_batt_cap_correct)
		axp_regmap_set_bits(map, 0xb8, 0x20);	/* enable */
	else
		axp_regmap_clr_bits(map, 0xb8, 0x20);	/* disable */

	/*battery detect enable */
	if (axp2585_config.pmu_batdeten)
		axp_regmap_set_bits(map, 0x8e, 0x08);
	else
		axp_regmap_clr_bits(map, 0x8e, 0x08);

	/* RDC initial */
	axp_regmap_read(map, AXP2585_RDC0, &val);
	if ((axp2585_config.pmu_battery_rdc) && (!(val & 0x40))) {
		rdc = (axp2585_config.pmu_battery_rdc * 10000 + 5371) / 10742;
		axp_regmap_write(map, AXP2585_RDC0, ((rdc >> 8) & 0x1F) | 0x80);
		axp_regmap_write(map, AXP2585_RDC1, rdc & 0x00FF);
	}

	axp_regmap_read(map, AXP2585_BATCAP0, &val);
	if ((axp2585_config.pmu_battery_cap) && (!(val & 0x80))) {
		cur_coulomb_counter = axp2585_config.pmu_battery_cap
		    * 1000 / 1456;
		axp_regmap_write(map, AXP2585_BATCAP0,
				 ((cur_coulomb_counter >> 8) | 0x80));
		axp_regmap_write(map, AXP2585_BATCAP1,
				 cur_coulomb_counter & 0x00FF);
	} else if (!axp2585_config.pmu_battery_cap) {
		axp_regmap_write(map, AXP2585_BATCAP0, 0x00);
		axp_regmap_write(map, AXP2585_BATCAP1, 0x00);
	}

	if (axp2585_config.pmu_bat_unused == 1)
		axp2585_spy_info.batt->det_unused = 1;
	else
		axp2585_spy_info.batt->det_unused = 0;

	if (axp2585_config.pmu_bat_temp_enable != 0) {
		axp_regmap_write(map, AXP2585_VLTF_CHARGE,
				 (u8) (axp2585_config.pmu_bat_charge_ltf >> 4));
		axp_regmap_write(map, AXP2585_VHTF_CHARGE,
				 (u8) (axp2585_config.pmu_bat_charge_htf >> 4));
		axp_regmap_write(map, AXP2585_VLTF_WORK,
				 (u8) (axp2585_config.pmu_bat_shutdown_ltf >>
				       4));
		axp_regmap_write(map, AXP2585_VHTF_WORK,
				 (u8) (axp2585_config.pmu_bat_shutdown_htf >>
				       4));
	}
	/*enable fast charge */
	axp_regmap_update(map, 0x31, 0x04, 0x04);
	/*set POR time as 16s */
	axp_regmap_update(map, AXP2585_POK_SET, 0x30, 0x30);
	for (i = 0; i < ARRAY_SIZE(update_min_times); i++) {
		if (update_min_times[i] == axp2585_config.pmu_update_min_time)
			break;
	}
	axp_regmap_update(map, AXP2585_ADJUST_PARA, i, 0x7);
	/*initial the ocv_cou_adjust_time */
	for (i = 0; i < ARRAY_SIZE(ocv_cou_adjust_time); i++) {
		if (ocv_cou_adjust_time[i] ==
		    axp2585_config.pmu_ocv_cou_adjust_time)
			break;
	}
	i <<= 6;
	axp_regmap_update(map, AXP2585_ADJUST_PARA1, i, 0xC0);

	axp_regmap_clr_bits(map, 0x23, 0x40);	/* disable bc1.2 detect */
/*
	axp_regmap_write(map, AXP2585_DPDMOTGCC_CTRL, 0x30);
	axp_regmap_read(map, AXP2585_DPDMOTGCC_CTRL, &val);
*/


	return 0;
}

static struct axp_interrupts axp2585_charger_irq[] = {
	{"ac in", axp_usb_in_isr},
	{"ac out", axp_usb_out_isr},
	{"bat in", axp_capchange_isr},
	{"bat out", axp_capchange_isr},
	{"charging", axp_change_isr},
	{"charge over", axp_change_isr},
	{"low warning1", axp_low_warning1_isr},
	{"low warning2", axp_low_warning2_isr},
	{"bc1.2 detect", axp_bc_detect_isr},
#ifdef TYPE_C
	{"tc in", axp_tc_in_isr},
	{"tc out", axp_tc_out_isr},
#endif
};

static int tc_mode = 1;
static int boost_mode = 1;
static int power = 1;
static ssize_t show_tc_mode(struct device *dev,
			    struct device_attribute *attr, char *buf)
{
	char *s = buf;
	char *end = (char *)((ptrdiff_t) buf + (ptrdiff_t) PAGE_SIZE);

	s += scnprintf(s, end - s, "%s\n", "0: close 1: sink 2: source 3: drp");
	s += scnprintf(s, end - s, "tc_mode=%d\n", tc_mode);
	return s - buf;
}

static ssize_t store_tc_mode(struct device *dev,
			     struct device_attribute *attr, const char *buf,
			     size_t count)
{
	int val, err;
	struct axp_charger_dev *chg_dev = dev_get_drvdata(dev);
	struct axp_regmap *map = chg_dev->chip->regmap;

	err = kstrtoint(buf, 16, &val);
	if (err)
		return err;
	if (val > 3)
		val = 1;
	tc_mode = val;
	axp_regmap_update(map, 0x33, tc_mode, 0x03);
	return count;
}

static ssize_t show_boost_mode(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	char *s = buf;
	char *end = (char *)((ptrdiff_t) buf + (ptrdiff_t) PAGE_SIZE);

	s += scnprintf(s, end - s, "%s\n", "1: open  0: close");
	s += scnprintf(s, end - s, "boost_mode=%d\n", boost_mode);
	return s - buf;
}

static ssize_t store_boost_mode(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	int val, err;
	struct axp_charger_dev *chg_dev = dev_get_drvdata(dev);
	struct axp_regmap *map = chg_dev->chip->regmap;

	err = kstrtoint(buf, 16, &val);
	if (err)
		return err;
	if (val != 1)
		val = 0;
	boost_mode = val;
	if (boost_mode == 1)
		axp_regmap_set_bits(map, 0x12, 0x80);
	else
		axp_regmap_clr_bits(map, 0x12, 0x80);
	return count;
}

static ssize_t axp2585_power_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct axp_charger_dev *chg_dev = dev_get_drvdata(dev);
	char *s = buf;
	char *end = (char *)((ptrdiff_t) buf + (ptrdiff_t) PAGE_SIZE);
	int vbat, ibat;
	vbat = axp2585_get_vbat(chg_dev);
	ibat = axp2585_get_disibat(chg_dev);
	power = vbat * ibat / 1000;
	s += scnprintf(s, end - s, "%dmV\n", power);
	return s - buf;
}

static DEVICE_ATTR(tc_mode, 0644, show_tc_mode, store_tc_mode);
static DEVICE_ATTR(boost_mode, 0644, show_boost_mode, store_boost_mode);
static DEVICE_ATTR(power, 0644, axp2585_power_show, NULL);
static struct attribute *bmu_control_attrs[] = {
	&dev_attr_tc_mode.attr,
	&dev_attr_boost_mode.attr,
	&dev_attr_power.attr,
	NULL,
};

static struct attribute_group bmu_control_attr_group = {
	.name = "bmu_control",
	.attrs = bmu_control_attrs,
};

static void axp2585_private_debug(struct axp_charger_dev *cdev)
{
	u8 tmp[2];
	struct axp_regmap *map = cdev->chip->regmap;
#if 0
	axp_regmap_reads(map, 0x5a, 2, tmp);
	AXP_DEBUG(AXP_SPLY, cdev->chip->pmu_num,
		  "adcin_vol = %d\n", ((tmp[0] << 4) | (tmp[1] & 0xF))
		  * 8);
#endif
	axp_regmap_reads(map, 0xbc, 2, tmp);
	AXP_DEBUG(AXP_SPLY, cdev->chip->pmu_num,
		  "ocv_vol = %d\n", ((tmp[0] << 4) | (tmp[1] & 0xF))
		  * 1200 / 1000);

}

static int axp2585_charger_probe(struct platform_device *pdev)
{
	struct axp_charger_dev *chg_dev;
	struct axp_dev *axp_dev = dev_get_drvdata(pdev->dev.parent);
	int ret, i, irq;

	pr_err("######machao: %s!\n", __func__);
	if (pdev->dev.of_node) {
		/* get dt and sysconfig */
		ret = axp_charger_dt_parse(pdev->dev.of_node, &axp2585_config);
		if (ret) {
			pr_err("%s parse device tree err\n", __func__);
			return -EINVAL;
		}
	} else {
		pr_err("axp2585 charger device tree err!\n");
		return -EBUSY;
	}

	axp2585_ac_info.ac_vol = axp2585_config.pmu_ac_vol;
	axp2585_ac_info.ac_cur = axp2585_config.pmu_ac_cur;
	axp2585_usb_info.usb_pc_vol = axp2585_config.pmu_usbpc_vol;
	axp2585_usb_info.usb_pc_cur = axp2585_config.pmu_usbpc_cur;
	axp2585_usb_info.usb_ad_vol = axp2585_config.pmu_ac_vol;
	axp2585_usb_info.usb_ad_cur = axp2585_config.pmu_ac_cur;
	axp2585_batt_info.runtime_chgcur = axp2585_config.pmu_runtime_chgcur;
	axp2585_batt_info.suspend_chgcur = axp2585_config.pmu_suspend_chgcur;
	axp2585_batt_info.shutdown_chgcur = axp2585_config.pmu_shutdown_chgcur;
	battery_data.voltage_max_design = axp2585_config.pmu_init_chgvol * 1000;
	battery_data.voltage_min_design = axp2585_config.pmu_pwroff_vol * 1000;
	battery_data.energy_full_design = axp2585_config.pmu_battery_cap;

	axp2585_charger_init(axp_dev);

	chg_dev = axp_power_supply_register(&pdev->dev, axp_dev,
					    &battery_data, &axp2585_spy_info);
	if (IS_ERR_OR_NULL(chg_dev))
		goto fail;
	chg_dev->private_debug = axp2585_private_debug;
	chg_dev->pmic_temp_offset = 0x56;
	chg_dev->spy_info->batt->bat_temp_offset = 0x58;

	for (i = 0; i < ARRAY_SIZE(axp2585_charger_irq); i++) {
		irq =
		    platform_get_irq_byname(pdev, axp2585_charger_irq[i].name);

		if (irq < 0)
			continue;

		if (axp_dev->irq_data != NULL) {

			ret =
			    axp_request_irq(axp_dev, irq,
					    axp2585_charger_irq[i].isr,
					    chg_dev);
			if (ret != 0) {
				dev_err(&pdev->dev,
					"failed to request %s IRQ %d: %d\n",
					axp2585_charger_irq[i].name, irq, ret);
				goto out_irq;
			}

			dev_dbg(&pdev->dev, "Requested %s IRQ %d: %d\n",
				axp2585_charger_irq[i].name, irq, ret);
		}
	}

	platform_set_drvdata(pdev, chg_dev);
	ret = sysfs_create_group(&pdev->dev.kobj, &bmu_control_attr_group);
	if (ret)
		dev_warn(&pdev->dev, "failed to create attr group\n");

	return 0;
out_irq:
	for (i = i - 1; i >= 0; i--) {
		irq =
		    platform_get_irq_byname(pdev, axp2585_charger_irq[i].name);
		if (irq < 0)
			continue;
		axp_free_irq(axp_dev, irq);
	}
fail:
	return -1;
}

static int axp2585_charger_remove(struct platform_device *pdev)
{
	int i, irq;
	struct axp_charger_dev *chg_dev = platform_get_drvdata(pdev);
	struct axp_dev *axp_dev = dev_get_drvdata(pdev->dev.parent);

	for (i = 0; i < ARRAY_SIZE(axp2585_charger_irq); i++) {
		irq =
		    platform_get_irq_byname(pdev, axp2585_charger_irq[i].name);
		if (irq < 0)
			continue;
		axp_free_irq(axp_dev, irq);
	}

	axp_power_supply_unregister(chg_dev);

	return 0;
}

static int axp2585_charger_suspend(struct platform_device *dev,
				   pm_message_t state)
{
	struct axp_charger_dev *chg_dev = platform_get_drvdata(dev);

	axp_suspend_flag = AXP_WAS_SUSPEND;
	axp_charger_suspend(chg_dev);

	return 0;
}

static int axp2585_charger_resume(struct platform_device *dev)
{
	struct axp_charger_dev *chg_dev = platform_get_drvdata(dev);
	int pre_rest_vol;

	if (axp_suspend_flag == AXP_SUSPEND_WITH_IRQ) {
		axp_suspend_flag = AXP_NOT_SUSPEND;
		//      sunxi_nmi_enable();
	} else {
		axp_suspend_flag = AXP_NOT_SUSPEND;
	}
	pre_rest_vol = chg_dev->rest_vol;
	axp_charger_resume(chg_dev);

	if (chg_dev->rest_vol - pre_rest_vol) {
		pr_info("battery vol change: %d->%d\n",
			pre_rest_vol, chg_dev->rest_vol);
		/*axp_regmap_write(map, 0x05, chg_dev->rest_vol | 0x80); */
	}

	return 0;
}

static void axp2585_charger_shutdown(struct platform_device *dev)
{
	struct axp_charger_dev *chg_dev = platform_get_drvdata(dev);

	axp_charger_shutdown(chg_dev);
}

static const struct of_device_id axp2585_charger_dt_ids[] = {
	{.compatible = "axp, axp2585-charger",},
	{},
};

MODULE_DEVICE_TABLE(of, axp2585_charger_dt_ids);

static struct platform_driver axp2585_charger_driver = {
	.driver = {
		   .name = "axp2585-charger",
		   .of_match_table = axp2585_charger_dt_ids,
		   },
	.probe = axp2585_charger_probe,
	.remove = axp2585_charger_remove,
	.suspend = axp2585_charger_suspend,
	.resume = axp2585_charger_resume,
	.shutdown = axp2585_charger_shutdown,
};

static int __init axp2585_charger_initcall(void)
{
	int ret;

	ret = platform_driver_register(&axp2585_charger_driver);
	if (ret < 0) {
		pr_err("%s: failed, errno %d\n", __func__, ret);
		return -EINVAL;
	}

	return 0;
}

//fs_initcall_sync(axp2585_charger_initcall);
late_initcall(axp2585_charger_initcall);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Roy <qiuzhigang@allwinnertech.com>");
MODULE_DESCRIPTION("Charger Driver for axp2585 BMU");
MODULE_ALIAS("platform:axp2585-charger");
