#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/tlv.h>
#include <linux/pm.h>
#include <linux/platform_device.h>
#include <linux/of_gpio.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <linux/proc_fs.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
//#include <linux/amlogic/aml_gpio_consumer.h>
#include <linux/gpio.h>

#include "ad82584f.h"

#define	AD82584F_REG_RAM_CHECK

unsigned int g_speaker_power = 0;

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
static void ad82584f_early_suspend(struct early_suspend *h);
static void ad82584f_late_resume(struct early_suspend *h);
#endif

#define AD82584F_RATES (SNDRV_PCM_RATE_32000 | \
		       SNDRV_PCM_RATE_44100 | \
		       SNDRV_PCM_RATE_48000 | \
		       SNDRV_PCM_RATE_64000 | \
		       SNDRV_PCM_RATE_88200 | \
		       SNDRV_PCM_RATE_96000 | \
		       SNDRV_PCM_RATE_176400 | \
		       SNDRV_PCM_RATE_192000)

#define AD82584F_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | \
	 SNDRV_PCM_FMTBIT_S24_LE | \
	 SNDRV_PCM_FMTBIT_S32_LE)

static const DECLARE_TLV_DB_SCALE(mvol_tlv, -10300, 50, 1);
static const DECLARE_TLV_DB_SCALE(chvol_tlv, -10300, 50, 1);

static int ad82584f_mute_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ad82584f_priv *ad82584f = snd_soc_codec_get_drvdata(codec);
	int val;

	val = snd_soc_read(codec, STATE_CTL_3);
	dev_info(codec->dev, "%s,val=%d\n", __func__, val);

	if(val == 0x7f){
		ucontrol->value.integer.value[0] = 1;
	}else{
		ucontrol->value.integer.value[0] = 0;
	}
	//regmap_read(ad82584f->regmap, STATE_CTL_3, &val);
	//ucontrol->value.integer.value[0] = ((val & 0x70) >> 4);

	return 0;
}

static int ad82584f_mute_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ad82584f_priv *ad82584f = snd_soc_codec_get_drvdata(codec);
	int sel = (int)ucontrol->value.integer.value[0];

	dev_info(codec->dev, "%s,ucontrol_val=%d\n", __func__, sel);
	if(sel == 0){  //mute off
		snd_soc_write(codec, STATE_CTL_3, 0x00);//--unmute amp
	}else{	//mute on
		snd_soc_write(codec, STATE_CTL_3, 0x7f);//--mute amp
	}

	//regmap_update_bits(ad82584f->regmap,STATE_CTL_3, 0x70, (sel << 4));
	return 0;
}

//off:mute disable
//on:mute enable
static const char *ad82584f_mute_text[] = {
	"off", "on",
};
static SOC_ENUM_SINGLE_EXT_DECL(ad82584f_mute_sel, ad82584f_mute_text);

static const struct snd_kcontrol_new ad82584f_snd_controls[] = {
	SOC_SINGLE_TLV("Master Volume", MVOL, 0, 0xff, 1, mvol_tlv),
	SOC_SINGLE_TLV("Ch1 Volume", C1VOL, 0, 0xff, 1, chvol_tlv),
	SOC_SINGLE_TLV("Ch2 Volume", C2VOL, 0, 0xff, 1, chvol_tlv),
	SOC_ENUM_EXT("SoftMute Control (on/off)", ad82584f_mute_sel, ad82584f_mute_get, ad82584f_mute_put)
};

static int m_reg_tab[AD82584F_REGISTER_COUNT][2];
static int m_ram1_tab[AD82584F_RAM_TABLE_COUNT][4];
static int m_ram2_tab[AD82584F_RAM_TABLE_COUNT][4];

static int spk10w_m_reg_tab[AD82584F_REGISTER_COUNT][2] =
{
       {0x00, 0x04},//##State_Control_1
       {0x01, 0x81},//##State_Control_2
       {0x02, 0x00},//##State_Control_3
       {0x03, 0x1d},//##Master_volume_control
       {0x04, 0x18},//##Channel_1_volume_control
       {0x05, 0x18},//##Channel_2_volume_control
       {0x06, 0x18},//##Channel_3_volume_control
       {0x07, 0x18},//##Channel_4_volume_control
       {0x08, 0x18},//##Channel_5_volume_control
       {0x09, 0x18},//##Channel_6_volume_control
       {0x0a, 0x10},//##Bass_Tone_Boost_and_Cut
       {0x0b, 0x10},//##treble_Tone_Boost_and_Cut
       {0x0c, 0x90},//##State_Control_4
       {0x0d, 0x00},//##Channel_1_configuration_registers
       {0x0e, 0x00},//##Channel_2_configuration_registers
       {0x0f, 0x00},//##Channel_3_configuration_registers
       {0x10, 0x00},//##Channel_4_configuration_registers
       {0x11, 0x00},//##Channel_5_configuration_registers
       {0x12, 0x00},//##Channel_6_configuration_registers
       {0x13, 0x00},//##Channel_7_configuration_registers
       {0x14, 0x00},//##Channel_8_configuration_registers
       {0x15, 0x6a},//##DRC1_limiter_attack/release_rate
       {0x16, 0x6a},//##DRC2_limiter_attack/release_rate
       {0x17, 0x6a},//##DRC3_limiter_attack/release_rate
       {0x18, 0x6a},//##DRC4_limiter_attack/release_rate
       {0x19, 0x06},//##Error_Delay
       {0x1a, 0x72},//##State_Control_5
       {0x1b, 0x01},//##HVUV_selection
       {0x1c, 0x00},//##State_Control_6
       {0x1d, 0x7f},//##Coefficient_RAM_Base_Address
       {0x1e, 0x00},//##Top_8-bits_of_coefficients_A1
       {0x1f, 0x00},//##Middle_8-bits_of_coefficients_A1
       {0x20, 0x00},//##Bottom_8-bits_of_coefficients_A1
       {0x21, 0x00},//##Top_8-bits_of_coefficients_A2
       {0x22, 0x00},//##Middle_8-bits_of_coefficients_A2
       {0x23, 0x00},//##Bottom_8-bits_of_coefficients_A2
       {0x24, 0x00},//##Top_8-bits_of_coefficients_B1
       {0x25, 0x00},//##Middle_8-bits_of_coefficients_B1
       {0x26, 0x00},//##Bottom_8-bits_of_coefficients_B1
       {0x27, 0x00},//##Top_8-bits_of_coefficients_B2
       {0x28, 0x00},//##Middle_8-bits_of_coefficients_B2
       {0x29, 0x00},//##Bottom_8-bits_of_coefficients_B2
       {0x2a, 0x40},//##Top_8-bits_of_coefficients_A0
       {0x2b, 0x00},//##Middle_8-bits_of_coefficients_A0
       {0x2c, 0x00},//##Bottom_8-bits_of_coefficients_A0
       {0x2d, 0x40},//##Coefficient_R/W_control
       {0x2e, 0x00},//##Protection_Enable/Disable
       {0x2f, 0x00},//##Memory_BIST_status
       {0x30, 0x00},//##Power_Stage_Status(Read_only)
       {0x31, 0x00},//##PWM_Output_Control
       {0x32, 0x00},//##Test_Mode_Control_Reg.
       {0x33, 0x6d},//##Qua-Ternary/Ternary_Switch_Level
       {0x34, 0x40},//##Volume_Fine_tune
       {0x35, 0x00},//##Volume_Fine_tune
       {0x36, 0x60},//##OC_bypass_&_GVDD_selection
       {0x37, 0x52},//##Device_ID_register
       {0x38, 0x00},//##RAM1_test_register_address
       {0x39, 0x00},//##Top_8-bits_of_RAM1_Data
       {0x3a, 0x00},//##Middle_8-bits_of_RAM1_Data
       {0x3b, 0x00},//##Bottom_8-bits_of_RAM1_Data
       {0x3c, 0x00},//##RAM1_test_r/w_control
       {0x3d, 0x00},//##RAM2_test_register_address
       {0x3e, 0x00},//##Top_8-bits_of_RAM2_Data
       {0x3f, 0x00},//##Middle_8-bits_of_RAM2_Data
       {0x40, 0x00},//##Bottom_8-bits_of_RAM2_Data
       {0x41, 0x00},//##RAM2_test_r/w_control
       {0x42, 0x00},//##Level_Meter_Clear
       {0x43, 0x00},//##Power_Meter_Clear
       {0x44, 0x7f},//##TOP_of_C1_Level_Meter
       {0x45, 0xff},//##Middle_of_C1_Level_Meter
       {0x46, 0xff},//##Bottom_of_C1_Level_Meter
       {0x47, 0x7f},//##TOP_of_C2_Level_Meter
       {0x48, 0xff},//##Middle_of_C2_Level_Meter
       {0x49, 0xff},//##Bottom_of_C2_Level_Meter
       {0x4a, 0x00},//##TOP_of_C3_Level_Meter
       {0x4b, 0x00},//##Middle_of_C3_Level_Meter
       {0x4c, 0x00},//##Bottom_of_C3_Level_Meter
       {0x4d, 0x00},//##TOP_of_C4_Level_Meter
       {0x4e, 0x00},//##Middle_of_C4_Level_Meter
       {0x4f, 0x00},//##Bottom_of_C4_Level_Meter
       {0x50, 0x00},//##TOP_of_C5_Level_Meter
       {0x51, 0x00},//##Middle_of_C5_Level_Meter
       {0x52, 0x00},//##Bottom_of_C5_Level_Meter
       {0x53, 0x00},//##TOP_of_C6_Level_Meter
       {0x54, 0x00},//##Middle_of_C6_Level_Meter
       {0x55, 0x00},//##Bottom_of_C6_Level_Meter
       {0x56, 0x00},//##TOP_of_C7_Level_Meter
       {0x57, 0x00},//##Middle_of_C7_Level_Meter
       {0x58, 0x00},//##Bottom_of_C7_Level_Meter
       {0x59, 0x00},//##TOP_of_C8_Level_Meter
       {0x5a, 0x00},//##Middle_of_C8_Level_Meter
       {0x5b, 0x00},//##Bottom_of_C8_Level_Meter
       {0x5c, 0x06},//##I2S_Data_Output_Selection_Register
       {0x5d, 0x00},//##Reserve
       {0x5e, 0x00},//##Reserve
       {0x5f, 0x00},//##Reserve
       {0x60, 0x00},//##Reserve
       {0x61, 0x00},//##Reserve
       {0x62, 0x00},//##Reserve
       {0x63, 0x00},//##Reserve
       {0x64, 0x00},//##Reserve
       {0x65, 0x00},//##Reserve
       {0x66, 0x00},//##Reserve
       {0x67, 0x00},//##Reserve
       {0x68, 0x00},//##Reserve
       {0x69, 0x00},//##Reserve
       {0x6a, 0x00},//##Reserve
       {0x6b, 0x00},//##Reserve
       {0x6c, 0x00},//##Reserve
       {0x6d, 0x00},//##Reserve
       {0x6e, 0x00},//##Reserve
       {0x6f, 0x00},//##Reserve
       {0x70, 0x00},//##Reserve
       {0x71, 0x00},//##Reserve
       {0x72, 0x00},//##Reserve
       {0x73, 0x00},//##Reserve
       {0x74, 0x30},//##Mono_Key_High_Byte
       {0x75, 0x06},//##Mono_Key_Low_Byte
       {0x76, 0x00},//##Boost_Control
       {0x77, 0x07},//##Hi-res_Item
       {0x78, 0x40},//##Test_Mode_register
       {0x79, 0x62},//##Boost_Strap_OV/UV_Selection
       {0x7a, 0x8c},//##OC_Selection_2
       {0x7b, 0x55},//##MBIST_User_Program_Top_Byte_Even
       {0x7c, 0x55},//##MBIST_User_Program_Middle_Byte_Even
       {0x7d, 0x55},//##MBIST_User_Program_Bottom_Byte_Even
       {0x7e, 0x55},//##MBIST_User_Program_Top_Byte_Odd
       {0x7f, 0x55},//##MBIST_User_Program_Middle_Byte_Odd
       {0x80, 0x55},//##MBIST_User_Program_Bottom_Byte_Odd
       {0x81, 0x00},//##ERROR_clear_register
       {0x82, 0x0c},//##Minimum_duty_test
       {0x83, 0x06},//##Reserve
       {0x84, 0xfe},//##Reserve
       {0x85, 0x7e},//##Reserve
};

static int spk10w_m_ram1_tab[AD82584F_RAM_TABLE_COUNT][4] =
{
       {0x00, 0x00, 0x00, 0x00},//##Channel_1_EQ1_A1 
       {0x01, 0x00, 0x00, 0x00},//##Channel_1_EQ1_A2 
       {0x02, 0x00, 0x00, 0x00},//##Channel_1_EQ1_B1 
       {0x03, 0x00, 0x00, 0x00},//##Channel_1_EQ1_B2 
       {0x04, 0x20, 0x00, 0x00},//##Channel_1_EQ1_A0 
       {0x05, 0x00, 0x00, 0x00},//##Channel_1_EQ2_A1 
       {0x06, 0x00, 0x00, 0x00},//##Channel_1_EQ2_A2 
       {0x07, 0x00, 0x00, 0x00},//##Channel_1_EQ2_B1 
       {0x08, 0x00, 0x00, 0x00},//##Channel_1_EQ2_B2 
       {0x09, 0x20, 0x00, 0x00},//##Channel_1_EQ2_A0 
       {0x0a, 0x00, 0x00, 0x00},//##Channel_1_EQ3_A1 
       {0x0b, 0x00, 0x00, 0x00},//##Channel_1_EQ3_A2 
       {0x0c, 0x00, 0x00, 0x00},//##Channel_1_EQ3_B1 
       {0x0d, 0x00, 0x00, 0x00},//##Channel_1_EQ3_B2 
       {0x0e, 0x20, 0x00, 0x00},//##Channel_1_EQ3_A0 
       {0x0f, 0x00, 0x00, 0x00},//##Channel_1_EQ4_A1 
       {0x10, 0x00, 0x00, 0x00},//##Channel_1_EQ4_A2 
       {0x11, 0x00, 0x00, 0x00},//##Channel_1_EQ4_B1 
       {0x12, 0x00, 0x00, 0x00},//##Channel_1_EQ4_B2 
       {0x13, 0x20, 0x00, 0x00},//##Channel_1_EQ4_A0 
       {0x14, 0x00, 0x00, 0x00},//##Channel_1_EQ5_A1 
       {0x15, 0x00, 0x00, 0x00},//##Channel_1_EQ5_A2 
       {0x16, 0x00, 0x00, 0x00},//##Channel_1_EQ5_B1 
       {0x17, 0x00, 0x00, 0x00},//##Channel_1_EQ5_B2 
       {0x18, 0x20, 0x00, 0x00},//##Channel_1_EQ5_A0 
       {0x19, 0x00, 0x00, 0x00},//##Channel_1_EQ6_A1 
       {0x1a, 0x00, 0x00, 0x00},//##Channel_1_EQ6_A2 
       {0x1b, 0x00, 0x00, 0x00},//##Channel_1_EQ6_B1 
       {0x1c, 0x00, 0x00, 0x00},//##Channel_1_EQ6_B2 
       {0x1d, 0x20, 0x00, 0x00},//##Channel_1_EQ6_A0 
       {0x1e, 0x00, 0x00, 0x00},//##Channel_1_EQ7_A1 
       {0x1f, 0x00, 0x00, 0x00},//##Channel_1_EQ7_A2 
       {0x20, 0x00, 0x00, 0x00},//##Channel_1_EQ7_B1 
       {0x21, 0x00, 0x00, 0x00},//##Channel_1_EQ7_B2 
       {0x22, 0x20, 0x00, 0x00},//##Channel_1_EQ7_A0 
       {0x23, 0x00, 0x00, 0x00},//##Channel_1_EQ8_A1 
       {0x24, 0x00, 0x00, 0x00},//##Channel_1_EQ8_A2 
       {0x25, 0x00, 0x00, 0x00},//##Channel_1_EQ8_B1 
       {0x26, 0x00, 0x00, 0x00},//##Channel_1_EQ8_B2 
       {0x27, 0x20, 0x00, 0x00},//##Channel_1_EQ8_A0 
       {0x28, 0x00, 0x00, 0x00},//##Channel_1_EQ9_A1 
       {0x29, 0x00, 0x00, 0x00},//##Channel_1_EQ9_A2 
       {0x2a, 0x00, 0x00, 0x00},//##Channel_1_EQ9_B1 
       {0x2b, 0x00, 0x00, 0x00},//##Channel_1_EQ9_B2 
       {0x2c, 0x20, 0x00, 0x00},//##Channel_1_EQ9_A0 
       {0x2d, 0x00, 0x00, 0x00},//##Channel_1_EQ10_A1 
       {0x2e, 0x00, 0x00, 0x00},//##Channel_1_EQ10_A2 
       {0x2f, 0x00, 0x00, 0x00},//##Channel_1_EQ10_B1 
       {0x30, 0x00, 0x00, 0x00},//##Channel_1_EQ10_B2 
       {0x31, 0x20, 0x00, 0x00},//##Channel_1_EQ10_A0 
       {0x32, 0x00, 0x00, 0x00},//##Channel_1_EQ11_A1 
       {0x33, 0x00, 0x00, 0x00},//##Channel_1_EQ11_A2 
       {0x34, 0x00, 0x00, 0x00},//##Channel_1_EQ11_B1 
       {0x35, 0x00, 0x00, 0x00},//##Channel_1_EQ11_B2 
       {0x36, 0x20, 0x00, 0x00},//##Channel_1_EQ11_A0 
       {0x37, 0x00, 0x00, 0x00},//##Channel_1_EQ12_A1 
       {0x38, 0x00, 0x00, 0x00},//##Channel_1_EQ12_A2 
       {0x39, 0x00, 0x00, 0x00},//##Channel_1_EQ12_B1 
       {0x3a, 0x00, 0x00, 0x00},//##Channel_1_EQ12_B2 
       {0x3b, 0x20, 0x00, 0x00},//##Channel_1_EQ12_A0 
       {0x3c, 0x00, 0x00, 0x00},//##Channel_1_EQ13_A1 
       {0x3d, 0x00, 0x00, 0x00},//##Channel_1_EQ13_A2 
       {0x3e, 0x00, 0x00, 0x00},//##Channel_1_EQ13_B1 
       {0x3f, 0x00, 0x00, 0x00},//##Channel_1_EQ13_B2 
       {0x40, 0x20, 0x00, 0x00},//##Channel_1_EQ13_A0 
       {0x41, 0x00, 0x00, 0x00},//##Channel_1_EQ14_A1 
       {0x42, 0x00, 0x00, 0x00},//##Channel_1_EQ14_A2 
       {0x43, 0x00, 0x00, 0x00},//##Channel_1_EQ14_B1 
       {0x44, 0x00, 0x00, 0x00},//##Channel_1_EQ14_B2 
       {0x45, 0x20, 0x00, 0x00},//##Channel_1_EQ14_A0 
       {0x46, 0x00, 0x00, 0x00},//##Channel_1_EQ15_A1 
       {0x47, 0x00, 0x00, 0x00},//##Channel_1_EQ15_A2 
       {0x48, 0x00, 0x00, 0x00},//##Channel_1_EQ15_B1 
       {0x49, 0x00, 0x00, 0x00},//##Channel_1_EQ15_B2 
       {0x4a, 0x20, 0x00, 0x00},//##Channel_1_EQ15_A0 
       {0x4b, 0x40, 0x00, 0x00},//##Channel_1_Mixer1 
       {0x4c, 0x40, 0x00, 0x00},//##Channel_1_Mixer2 
       {0x4d, 0x7f, 0xff, 0xff},//##Channel_1_Prescale 
       {0x4e, 0x7f, 0xff, 0xff},//##Channel_1_Postscale 
       {0x4f, 0xc7, 0xb6, 0x91},//##A0_of_L_channel_SRS_HPF 
       {0x50, 0x38, 0x49, 0x6e},//##A1_of_L_channel_SRS_HPF 
       {0x51, 0x0c, 0x46, 0xf8},//##B1_of_L_channel_SRS_HPF 
       {0x52, 0x0e, 0x81, 0xb9},//##A0_of_L_channel_SRS_LPF 
       {0x53, 0xf2, 0x2c, 0x12},//##A1_of_L_channel_SRS_LPF 
       {0x54, 0x0f, 0xca, 0xbb},//##B1_of_L_channel_SRS_LPF 
       {0x55, 0x20, 0x00, 0x00},//##CH1.2_Power_Clipping 
       {0x56, 0x20, 0x00, 0x00},//##CCH1.2_DRC1_Attack_threshold 
       {0x57, 0x08, 0x00, 0x00},//##CH1.2_DRC1_Release_threshold 
       {0x58, 0x20, 0x00, 0x00},//##CH3.4_DRC2_Attack_threshold 
       {0x59, 0x08, 0x00, 0x00},//##CH3.4_DRC2_Release_threshold 
       {0x5a, 0x20, 0x00, 0x00},//##CH5.6_DRC3_Attack_threshold 
       {0x5b, 0x08, 0x00, 0x00},//##CH5.6_DRC3_Release_threshold 
       {0x5c, 0x20, 0x00, 0x00},//##CH7.8_DRC4_Attack_threshold 
       {0x5d, 0x08, 0x00, 0x00},//##CH7.8_DRC4_Release_threshold 
       {0x5e, 0x00, 0x00, 0x1a},//##Noise_Gate_Attack_Level 
       {0x5f, 0x00, 0x00, 0x53},//##Noise_Gate_Release_Level 
       {0x60, 0x00, 0x80, 0x00},//##DRC1_Energy_Coefficient 
       {0x61, 0x00, 0x20, 0x00},//##DRC2_Energy_Coefficient 
       {0x62, 0x00, 0x80, 0x00},//##DRC3_Energy_Coefficient 
       {0x63, 0x00, 0x80, 0x00},//##DRC4_Energy_Coefficient 
       {0x64, 0x08, 0xf8, 0x57},//DRC1_Power_Meter
       {0x65, 0x00, 0x00, 0x00},//DRC3_Power_Mete
       {0x66, 0x00, 0x00, 0x00},//DRC5_Power_Meter
       {0x67, 0x00, 0x00, 0x00},//DRC7_Power_Meter
       {0x68, 0x00, 0x00, 0x00},//##Channel_1_DEQ1_A1 
       {0x69, 0x00, 0x00, 0x00},//##Channel_1_DEQ1_A2
       {0x6a, 0x00, 0x00, 0x00},//##Channel_1_DEQ1_B1
       {0x6b, 0x00, 0x00, 0x00},//##Channel_1_DEQ1_B2 
       {0x6c, 0x20, 0x00, 0x00},//##Channel_1_DEQ1_A0
       {0x6d, 0x00, 0x00, 0x00},//##Channel_1_DEQ2_A1 
       {0x6e, 0x00, 0x00, 0x00},//##Channel_1_DEQ2_A2 
       {0x6f, 0x00, 0x00, 0x00},//##Channel_1_DEQ2_B1 
       {0x70, 0x00, 0x00, 0x00},//##Channel_1_DEQ2_B2 
       {0x71, 0x20, 0x00, 0x00},//##Channel_1_DEQ2_A0 
       {0x72, 0x00, 0x00, 0x00},//##Channel_1_DEQ3_A1 
       {0x73, 0x00, 0x00, 0x00},//##Channel_1_DEQ3_A2 
       {0x74, 0x00, 0x00, 0x00},//##Channel_1_DEQ3_B1 
       {0x75, 0x00, 0x00, 0x00},//##Channel_1_DEQ3_B2 
       {0x76, 0x20, 0x00, 0x00},//##Channel_1_DEQ3_A0 
       {0x77, 0x00, 0x00, 0x00},//##Channel_1_DEQ4_A1 
       {0x78, 0x00, 0x00, 0x00},//##Channel_1_DEQ4_A2 
       {0x79, 0x00, 0x00, 0x00},//##Channel_1_DEQ4_B1 
       {0x7a, 0x00, 0x00, 0x00},//##Channel_1_DEQ4_B2 
       {0x7b, 0x20, 0x00, 0x00},//##Channel_1_DEQ4_A0 
       {0x7c, 0x00, 0x00, 0x00},//##Reserve
       {0x7d, 0x00, 0x00, 0x00},//##Reserve
       {0x7e, 0x00, 0x00, 0x00},//##Reserve
       {0x7f, 0x00, 0x00, 0x00},//##Reserve
};

static int spk10w_m_ram2_tab[AD82584F_RAM_TABLE_COUNT][4] =
{
       {0x00, 0x00, 0x00, 0x00},//##Channel_2_EQ1_A1 
       {0x01, 0x00, 0x00, 0x00},//##Channel_2_EQ1_A2 
       {0x02, 0x00, 0x00, 0x00},//##Channel_2_EQ1_B1 
       {0x03, 0x00, 0x00, 0x00},//##Channel_2_EQ1_B2 
       {0x04, 0x20, 0x00, 0x00},//##Channel_2_EQ1_A0 
       {0x05, 0x00, 0x00, 0x00},//##Channel_2_EQ2_A1 
       {0x06, 0x00, 0x00, 0x00},//##Channel_2_EQ2_A2 
       {0x07, 0x00, 0x00, 0x00},//##Channel_2_EQ2_B1 
       {0x08, 0x00, 0x00, 0x00},//##Channel_2_EQ2_B2 
       {0x09, 0x20, 0x00, 0x00},//##Channel_2_EQ2_A0 
       {0x0a, 0x00, 0x00, 0x00},//##Channel_2_EQ3_A1 
       {0x0b, 0x00, 0x00, 0x00},//##Channel_2_EQ3_A2 
       {0x0c, 0x00, 0x00, 0x00},//##Channel_2_EQ3_B1 
       {0x0d, 0x00, 0x00, 0x00},//##Channel_2_EQ3_B2 
       {0x0e, 0x20, 0x00, 0x00},//##Channel_2_EQ3_A0 
       {0x0f, 0x00, 0x00, 0x00},//##Channel_2_EQ4_A1 
       {0x10, 0x00, 0x00, 0x00},//##Channel_2_EQ4_A2 
       {0x11, 0x00, 0x00, 0x00},//##Channel_2_EQ4_B1 
       {0x12, 0x00, 0x00, 0x00},//##Channel_2_EQ4_B2 
       {0x13, 0x20, 0x00, 0x00},//##Channel_2_EQ4_A0 
       {0x14, 0x00, 0x00, 0x00},//##Channel_2_EQ5_A1 
       {0x15, 0x00, 0x00, 0x00},//##Channel_2_EQ5_A2 
       {0x16, 0x00, 0x00, 0x00},//##Channel_2_EQ5_B1 
       {0x17, 0x00, 0x00, 0x00},//##Channel_2_EQ5_B2 
       {0x18, 0x20, 0x00, 0x00},//##Channel_2_EQ5_A0 
       {0x19, 0x00, 0x00, 0x00},//##Channel_2_EQ6_A1 
       {0x1a, 0x00, 0x00, 0x00},//##Channel_2_EQ6_A2 
       {0x1b, 0x00, 0x00, 0x00},//##Channel_2_EQ6_B1 
       {0x1c, 0x00, 0x00, 0x00},//##Channel_2_EQ6_B2 
       {0x1d, 0x20, 0x00, 0x00},//##Channel_2_EQ6_A0 
       {0x1e, 0x00, 0x00, 0x00},//##Channel_2_EQ7_A1 
       {0x1f, 0x00, 0x00, 0x00},//##Channel_2_EQ7_A2 
       {0x20, 0x00, 0x00, 0x00},//##Channel_2_EQ7_B1 
       {0x21, 0x00, 0x00, 0x00},//##Channel_2_EQ7_B2 
       {0x22, 0x20, 0x00, 0x00},//##Channel_2_EQ7_A0 
       {0x23, 0x00, 0x00, 0x00},//##Channel_2_EQ8_A1 
       {0x24, 0x00, 0x00, 0x00},//##Channel_2_EQ8_A2 
       {0x25, 0x00, 0x00, 0x00},//##Channel_2_EQ8_B1 
       {0x26, 0x00, 0x00, 0x00},//##Channel_2_EQ8_B2 
       {0x27, 0x20, 0x00, 0x00},//##Channel_2_EQ8_A0 
       {0x28, 0x00, 0x00, 0x00},//##Channel_2_EQ9_A1 
       {0x29, 0x00, 0x00, 0x00},//##Channel_2_EQ9_A2 
       {0x2a, 0x00, 0x00, 0x00},//##Channel_2_EQ9_B1 
       {0x2b, 0x00, 0x00, 0x00},//##Channel_2_EQ9_B2 
       {0x2c, 0x20, 0x00, 0x00},//##Channel_2_EQ9_A0 
       {0x2d, 0x00, 0x00, 0x00},//##Channel_2_EQ10_A1 
       {0x2e, 0x00, 0x00, 0x00},//##Channel_2_EQ10_A2 
       {0x2f, 0x00, 0x00, 0x00},//##Channel_2_EQ10_B1 
       {0x30, 0x00, 0x00, 0x00},//##Channel_2_EQ10_B2 
       {0x31, 0x20, 0x00, 0x00},//##Channel_2_EQ10_A0 
       {0x32, 0x00, 0x00, 0x00},//##Channel_2_EQ11_A1 
       {0x33, 0x00, 0x00, 0x00},//##Channel_2_EQ11_A2 
       {0x34, 0x00, 0x00, 0x00},//##Channel_2_EQ11_B1 
       {0x35, 0x00, 0x00, 0x00},//##Channel_2_EQ11_B2 
       {0x36, 0x20, 0x00, 0x00},//##Channel_2_EQ11_A0 
       {0x37, 0x00, 0x00, 0x00},//##Channel_2_EQ12_A1 
       {0x38, 0x00, 0x00, 0x00},//##Channel_2_EQ12_A2 
       {0x39, 0x00, 0x00, 0x00},//##Channel_2_EQ12_B1 
       {0x3a, 0x00, 0x00, 0x00},//##Channel_2_EQ12_B2 
       {0x3b, 0x20, 0x00, 0x00},//##Channel_2_EQ12_A0 
       {0x3c, 0x00, 0x00, 0x00},//##Channel_2_EQ13_A1 
       {0x3d, 0x00, 0x00, 0x00},//##Channel_2_EQ13_A2 
       {0x3e, 0x00, 0x00, 0x00},//##Channel_2_EQ13_B1 
       {0x3f, 0x00, 0x00, 0x00},//##Channel_2_EQ13_B2 
       {0x40, 0x20, 0x00, 0x00},//##Channel_2_EQ13_A0 
       {0x41, 0x00, 0x00, 0x00},//##Channel_2_EQ14_A1 
       {0x42, 0x00, 0x00, 0x00},//##Channel_2_EQ14_A2 
       {0x43, 0x00, 0x00, 0x00},//##Channel_2_EQ14_B1 
       {0x44, 0x00, 0x00, 0x00},//##Channel_2_EQ14_B2 
       {0x45, 0x20, 0x00, 0x00},//##Channel_2_EQ14_A0 
       {0x46, 0x00, 0x00, 0x00},//##Channel_2_EQ15_A1 
       {0x47, 0x00, 0x00, 0x00},//##Channel_2_EQ15_A2 
       {0x48, 0x00, 0x00, 0x00},//##Channel_2_EQ15_B1 
       {0x49, 0x00, 0x00, 0x00},//##Channel_2_EQ15_B2 
       {0x4a, 0x20, 0x00, 0x00},//##Channel_2_EQ15_A0 
       {0x4b, 0x40, 0x00, 0x00},//##Channel_2_Mixer1 
       {0x4c, 0x40, 0x00, 0x00},//##Channel_2_Mixer2 
       {0x4d, 0x7f, 0xff, 0xff},//##Channel_2_Prescale 
       {0x4e, 0x7f, 0xff, 0xff},//##Channel_2_Postscale 
       {0x4f, 0xc7, 0xb6, 0x91},//##A0_of_R_channel_SRS_HPF 
       {0x50, 0x38, 0x49, 0x6e},//##A1_of_R_channel_SRS_HPF 
       {0x51, 0x0c, 0x46, 0xf8},//##B1_of_R_channel_SRS_HPF 
       {0x52, 0x0e, 0x81, 0xb9},//##A0_of_R_channel_SRS_LPF 
       {0x53, 0xf2, 0x2c, 0x12},//##A1_of_R_channel_SRS_LPF 
       {0x54, 0x0f, 0xca, 0xbb},//##B1_of_R_channel_SRS_LPF 
       {0x55, 0x00, 0x00, 0x00},//##Reserve
       {0x56, 0x00, 0x00, 0x00},//##Reserve
       {0x57, 0x00, 0x00, 0x00},//##Reserve
       {0x58, 0x00, 0x00, 0x00},//##Reserve
       {0x59, 0x00, 0x00, 0x00},//##Reserve
       {0x5a, 0x00, 0x00, 0x00},//##Reserve
       {0x5b, 0x00, 0x00, 0x00},//##Reserve
       {0x5c, 0x00, 0x00, 0x00},//##Reserve
       {0x5d, 0x00, 0x00, 0x00},//##Reserve
       {0x5e, 0x00, 0x00, 0x00},//##Reserve
       {0x5f, 0x00, 0x00, 0x00},//##Reserve
       {0x60, 0x00, 0x00, 0x00},//##Reserve
       {0x61, 0x00, 0x00, 0x00},//##Reserve
       {0x62, 0x00, 0x00, 0x00},//##Reserve
       {0x63, 0x00, 0x00, 0x00},//##Reserve
       {0x64, 0x08, 0xf8, 0xf8},//DRC2_Power_Meter
       {0x65, 0x00, 0x00, 0x00},//DRC4_Power_Mete
       {0x66, 0x00, 0x00, 0x00},//DRC6_Power_Meter
       {0x67, 0x00, 0x00, 0x00},//DRC8_Power_Meter
       {0x68, 0x00, 0x00, 0x00},//##Channel_2_DEQ1_A1 
       {0x69, 0x00, 0x00, 0x00},//##Channel_2_DEQ1_A2
       {0x6a, 0x00, 0x00, 0x00},//##Channel_2_DEQ1_B1
       {0x6b, 0x00, 0x00, 0x00},//##Channel_2_DEQ1_B2 
       {0x6c, 0x20, 0x00, 0x00},//##Channel_2_DEQ1_A0
       {0x6d, 0x00, 0x00, 0x00},//##Channel_2_DEQ2_A1 
       {0x6e, 0x00, 0x00, 0x00},//##Channel_2_DEQ2_A2 
       {0x6f, 0x00, 0x00, 0x00},//##Channel_2_DEQ2_B1 
       {0x70, 0x00, 0x00, 0x00},//##Channel_2_DEQ2_B2 
       {0x71, 0x20, 0x00, 0x00},//##Channel_2_DEQ2_A0 
       {0x72, 0x00, 0x00, 0x00},//##Channel_2_DEQ3_A1 
       {0x73, 0x00, 0x00, 0x00},//##Channel_2_DEQ3_A2 
       {0x74, 0x00, 0x00, 0x00},//##Channel_2_DEQ3_B1 
       {0x75, 0x00, 0x00, 0x00},//##Channel_2_DEQ3_B2 
       {0x76, 0x20, 0x00, 0x00},//##Channel_2_DEQ3_A0 
       {0x77, 0x00, 0x00, 0x00},//##Channel_2_DEQ4_A1 
       {0x78, 0x00, 0x00, 0x00},//##Channel_2_DEQ4_A2 
       {0x79, 0x00, 0x00, 0x00},//##Channel_2_DEQ4_B1 
       {0x7a, 0x00, 0x00, 0x00},//##Channel_2_DEQ4_B2 
       {0x7b, 0x20, 0x00, 0x00},//##Channel_2_DEQ4_A0 
       {0x7c, 0x00, 0x00, 0x00},//##Reserve
       {0x7d, 0x00, 0x00, 0x00},//##Reserve
       {0x7e, 0x00, 0x00, 0x00},//##Reserve
       {0x7f, 0x00, 0x00, 0x00},//##Reserve
};

static int spk3w_m_reg_tab[AD82584F_REGISTER_COUNT][2] = 
{
       {0x00, 0x04},//##State_Control_1
       {0x01, 0x81},//##State_Control_2
       {0x02, 0x00},//##State_Control_3
       {0x03, 0x26},//##Master_volume_control
       {0x04, 0x18},//##Channel_1_volume_control
       {0x05, 0x18},//##Channel_2_volume_control
       {0x06, 0x18},//##Channel_3_volume_control
       {0x07, 0x18},//##Channel_4_volume_control
       {0x08, 0x18},//##Channel_5_volume_control
       {0x09, 0x18},//##Channel_6_volume_control
       {0x0a, 0x10},//##Bass_Tone_Boost_and_Cut
       {0x0b, 0x10},//##treble_Tone_Boost_and_Cut
       {0x0c, 0x90},//##State_Control_4
       {0x0d, 0x00},//##Channel_1_configuration_registers
       {0x0e, 0x00},//##Channel_2_configuration_registers
       {0x0f, 0x00},//##Channel_3_configuration_registers
       {0x10, 0x00},//##Channel_4_configuration_registers
       {0x11, 0x00},//##Channel_5_configuration_registers
       {0x12, 0x00},//##Channel_6_configuration_registers
       {0x13, 0x00},//##Channel_7_configuration_registers
       {0x14, 0x00},//##Channel_8_configuration_registers
       {0x15, 0x6a},//##DRC1_limiter_attack/release_rate
       {0x16, 0x6a},//##DRC2_limiter_attack/release_rate
       {0x17, 0x6a},//##DRC3_limiter_attack/release_rate
       {0x18, 0x6a},//##DRC4_limiter_attack/release_rate
       {0x19, 0x06},//##Error_Delay
       {0x1a, 0x72},//##State_Control_5
       {0x1b, 0x01},//##HVUV_selection
       {0x1c, 0x00},//##State_Control_6
       {0x1d, 0x7f},//##Coefficient_RAM_Base_Address
       {0x1e, 0x00},//##Top_8-bits_of_coefficients_A1
       {0x1f, 0x00},//##Middle_8-bits_of_coefficients_A1
       {0x20, 0x00},//##Bottom_8-bits_of_coefficients_A1
       {0x21, 0x00},//##Top_8-bits_of_coefficients_A2
       {0x22, 0x00},//##Middle_8-bits_of_coefficients_A2
       {0x23, 0x00},//##Bottom_8-bits_of_coefficients_A2
       {0x24, 0x00},//##Top_8-bits_of_coefficients_B1
       {0x25, 0x00},//##Middle_8-bits_of_coefficients_B1
       {0x26, 0x00},//##Bottom_8-bits_of_coefficients_B1
       {0x27, 0x00},//##Top_8-bits_of_coefficients_B2
       {0x28, 0x00},//##Middle_8-bits_of_coefficients_B2
       {0x29, 0x00},//##Bottom_8-bits_of_coefficients_B2
       {0x2a, 0x40},//##Top_8-bits_of_coefficients_A0
       {0x2b, 0x00},//##Middle_8-bits_of_coefficients_A0
       {0x2c, 0x00},//##Bottom_8-bits_of_coefficients_A0
       {0x2d, 0x00},//##Coefficient_R/W_control
       {0x2e, 0x00},//##Protection_Enable/Disable
       {0x2f, 0x00},//##Memory_BIST_status
       {0x30, 0x00},//##Power_Stage_Status(Read_only)
       {0x31, 0x00},//##PWM_Output_Control
       {0x32, 0x00},//##Test_Mode_Control_Reg.
       {0x33, 0x6d},//##Qua-Ternary/Ternary_Switch_Level
       {0x34, 0x00},//##Volume_Fine_tune
       {0x35, 0x00},//##Volume_Fine_tune
       {0x36, 0x60},//##OC_bypass_&_GVDD_selection
       {0x37, 0x52},//##Device_ID_register
       {0x38, 0x00},//##RAM1_test_register_address
       {0x39, 0x00},//##Top_8-bits_of_RAM1_Data
       {0x3a, 0x00},//##Middle_8-bits_of_RAM1_Data
       {0x3b, 0x00},//##Bottom_8-bits_of_RAM1_Data
       {0x3c, 0x00},//##RAM1_test_r/w_control
       {0x3d, 0x00},//##RAM2_test_register_address
       {0x3e, 0x00},//##Top_8-bits_of_RAM2_Data
       {0x3f, 0x00},//##Middle_8-bits_of_RAM2_Data
       {0x40, 0x00},//##Bottom_8-bits_of_RAM2_Data
       {0x41, 0x00},//##RAM2_test_r/w_control
       {0x42, 0x00},//##Level_Meter_Clear
       {0x43, 0x00},//##Power_Meter_Clear
       {0x44, 0x7f},//##TOP_of_C1_Level_Meter
       {0x45, 0xff},//##Middle_of_C1_Level_Meter
       {0x46, 0xff},//##Bottom_of_C1_Level_Meter
       {0x47, 0x7f},//##TOP_of_C2_Level_Meter
       {0x48, 0xff},//##Middle_of_C2_Level_Meter
       {0x49, 0xff},//##Bottom_of_C2_Level_Meter
       {0x4a, 0x00},//##TOP_of_C3_Level_Meter
       {0x4b, 0x00},//##Middle_of_C3_Level_Meter
       {0x4c, 0x00},//##Bottom_of_C3_Level_Meter
       {0x4d, 0x00},//##TOP_of_C4_Level_Meter
       {0x4e, 0x00},//##Middle_of_C4_Level_Meter
       {0x4f, 0x00},//##Bottom_of_C4_Level_Meter
       {0x50, 0x00},//##TOP_of_C5_Level_Meter
       {0x51, 0x00},//##Middle_of_C5_Level_Meter
       {0x52, 0x00},//##Bottom_of_C5_Level_Meter
       {0x53, 0x00},//##TOP_of_C6_Level_Meter
       {0x54, 0x00},//##Middle_of_C6_Level_Meter
       {0x55, 0x00},//##Bottom_of_C6_Level_Meter
       {0x56, 0x00},//##TOP_of_C7_Level_Meter
       {0x57, 0x00},//##Middle_of_C7_Level_Meter
       {0x58, 0x00},//##Bottom_of_C7_Level_Meter
       {0x59, 0x00},//##TOP_of_C8_Level_Meter
       {0x5a, 0x00},//##Middle_of_C8_Level_Meter
       {0x5b, 0x00},//##Bottom_of_C8_Level_Meter
       {0x5c, 0x06},//##I2S_Data_Output_Selection_Register
       {0x5d, 0x00},//##Reserve
       {0x5e, 0x00},//##Reserve
       {0x5f, 0x00},//##Reserve
       {0x60, 0x00},//##Reserve
       {0x61, 0x00},//##Reserve
       {0x62, 0x00},//##Reserve
       {0x63, 0x00},//##Reserve
       {0x64, 0x00},//##Reserve
       {0x65, 0x00},//##Reserve
       {0x66, 0x00},//##Reserve
       {0x67, 0x00},//##Reserve
       {0x68, 0x00},//##Reserve
       {0x69, 0x00},//##Reserve
       {0x6a, 0x00},//##Reserve
       {0x6b, 0x00},//##Reserve
       {0x6c, 0x00},//##Reserve
       {0x6d, 0x00},//##Reserve
       {0x6e, 0x00},//##Reserve
       {0x6f, 0x00},//##Reserve
       {0x70, 0x00},//##Reserve
       {0x71, 0x00},//##Reserve
       {0x72, 0x00},//##Reserve
       {0x73, 0x00},//##Reserve
       {0x74, 0x30},//##Mono_Key_High_Byte
       {0x75, 0x06},//##Mono_Key_Low_Byte
       {0x76, 0x00},//##Boost_Control
       {0x77, 0x07},//##Hi-res_Item
       {0x78, 0x40},//##Test_Mode_register
       {0x79, 0x62},//##Boost_Strap_OV/UV_Selection
       {0x7a, 0x8c},//##OC_Selection_2
       {0x7b, 0x55},//##MBIST_User_Program_Top_Byte_Even
       {0x7c, 0x55},//##MBIST_User_Program_Middle_Byte_Even
       {0x7d, 0x55},//##MBIST_User_Program_Bottom_Byte_Even
       {0x7e, 0x55},//##MBIST_User_Program_Top_Byte_Odd
       {0x7f, 0x55},//##MBIST_User_Program_Middle_Byte_Odd
       {0x80, 0x55},//##MBIST_User_Program_Bottom_Byte_Odd
       {0x81, 0x00},//##ERROR_clear_register
       {0x82, 0x0c},//##Minimum_duty_test
       {0x83, 0x06},//##Reserve
       {0x84, 0xfe},//##Reserve
       {0x85, 0xfe},//##Reserve
};

static int spk3w_m_ram1_tab[AD82584F_RAM_TABLE_COUNT][4] = 
{
       {0x00, 0x00, 0x00, 0x00},//##Channel_1_EQ1_A1 
       {0x01, 0x00, 0x00, 0x00},//##Channel_1_EQ1_A2 
       {0x02, 0x00, 0x00, 0x00},//##Channel_1_EQ1_B1 
       {0x03, 0x00, 0x00, 0x00},//##Channel_1_EQ1_B2 
       {0x04, 0x20, 0x00, 0x00},//##Channel_1_EQ1_A0 
       {0x05, 0x00, 0x00, 0x00},//##Channel_1_EQ2_A1 
       {0x06, 0x00, 0x00, 0x00},//##Channel_1_EQ2_A2 
       {0x07, 0x00, 0x00, 0x00},//##Channel_1_EQ2_B1 
       {0x08, 0x00, 0x00, 0x00},//##Channel_1_EQ2_B2 
       {0x09, 0x20, 0x00, 0x00},//##Channel_1_EQ2_A0 
       {0x0a, 0x00, 0x00, 0x00},//##Channel_1_EQ3_A1 
       {0x0b, 0x00, 0x00, 0x00},//##Channel_1_EQ3_A2 
       {0x0c, 0x00, 0x00, 0x00},//##Channel_1_EQ3_B1 
       {0x0d, 0x00, 0x00, 0x00},//##Channel_1_EQ3_B2 
       {0x0e, 0x20, 0x00, 0x00},//##Channel_1_EQ3_A0 
       {0x0f, 0x00, 0x00, 0x00},//##Channel_1_EQ4_A1 
       {0x10, 0x00, 0x00, 0x00},//##Channel_1_EQ4_A2 
       {0x11, 0x00, 0x00, 0x00},//##Channel_1_EQ4_B1 
       {0x12, 0x00, 0x00, 0x00},//##Channel_1_EQ4_B2 
       {0x13, 0x20, 0x00, 0x00},//##Channel_1_EQ4_A0 
       {0x14, 0x00, 0x00, 0x00},//##Channel_1_EQ5_A1 
       {0x15, 0x00, 0x00, 0x00},//##Channel_1_EQ5_A2 
       {0x16, 0x00, 0x00, 0x00},//##Channel_1_EQ5_B1 
       {0x17, 0x00, 0x00, 0x00},//##Channel_1_EQ5_B2 
       {0x18, 0x20, 0x00, 0x00},//##Channel_1_EQ5_A0 
       {0x19, 0x00, 0x00, 0x00},//##Channel_1_EQ6_A1 
       {0x1a, 0x00, 0x00, 0x00},//##Channel_1_EQ6_A2 
       {0x1b, 0x00, 0x00, 0x00},//##Channel_1_EQ6_B1 
       {0x1c, 0x00, 0x00, 0x00},//##Channel_1_EQ6_B2 
       {0x1d, 0x20, 0x00, 0x00},//##Channel_1_EQ6_A0 
       {0x1e, 0x00, 0x00, 0x00},//##Channel_1_EQ7_A1 
       {0x1f, 0x00, 0x00, 0x00},//##Channel_1_EQ7_A2 
       {0x20, 0x00, 0x00, 0x00},//##Channel_1_EQ7_B1 
       {0x21, 0x00, 0x00, 0x00},//##Channel_1_EQ7_B2 
       {0x22, 0x20, 0x00, 0x00},//##Channel_1_EQ7_A0 
       {0x23, 0x00, 0x00, 0x00},//##Channel_1_EQ8_A1 
       {0x24, 0x00, 0x00, 0x00},//##Channel_1_EQ8_A2 
       {0x25, 0x00, 0x00, 0x00},//##Channel_1_EQ8_B1 
       {0x26, 0x00, 0x00, 0x00},//##Channel_1_EQ8_B2 
       {0x27, 0x20, 0x00, 0x00},//##Channel_1_EQ8_A0 
       {0x28, 0x00, 0x00, 0x00},//##Channel_1_EQ9_A1 
       {0x29, 0x00, 0x00, 0x00},//##Channel_1_EQ9_A2 
       {0x2a, 0x00, 0x00, 0x00},//##Channel_1_EQ9_B1 
       {0x2b, 0x00, 0x00, 0x00},//##Channel_1_EQ9_B2 
       {0x2c, 0x20, 0x00, 0x00},//##Channel_1_EQ9_A0 
       {0x2d, 0x00, 0x00, 0x00},//##Channel_1_EQ10_A1 
       {0x2e, 0x00, 0x00, 0x00},//##Channel_1_EQ10_A2 
       {0x2f, 0x00, 0x00, 0x00},//##Channel_1_EQ10_B1 
       {0x30, 0x00, 0x00, 0x00},//##Channel_1_EQ10_B2 
       {0x31, 0x20, 0x00, 0x00},//##Channel_1_EQ10_A0 
       {0x32, 0x00, 0x00, 0x00},//##Channel_1_EQ11_A1 
       {0x33, 0x00, 0x00, 0x00},//##Channel_1_EQ11_A2 
       {0x34, 0x00, 0x00, 0x00},//##Channel_1_EQ11_B1 
       {0x35, 0x00, 0x00, 0x00},//##Channel_1_EQ11_B2 
       {0x36, 0x20, 0x00, 0x00},//##Channel_1_EQ11_A0 
       {0x37, 0x00, 0x00, 0x00},//##Channel_1_EQ12_A1 
       {0x38, 0x00, 0x00, 0x00},//##Channel_1_EQ12_A2 
       {0x39, 0x00, 0x00, 0x00},//##Channel_1_EQ12_B1 
       {0x3a, 0x00, 0x00, 0x00},//##Channel_1_EQ12_B2 
       {0x3b, 0x20, 0x00, 0x00},//##Channel_1_EQ12_A0 
       {0x3c, 0x00, 0x00, 0x00},//##Channel_1_EQ13_A1 
       {0x3d, 0x00, 0x00, 0x00},//##Channel_1_EQ13_A2 
       {0x3e, 0x00, 0x00, 0x00},//##Channel_1_EQ13_B1 
       {0x3f, 0x00, 0x00, 0x00},//##Channel_1_EQ13_B2 
       {0x40, 0x20, 0x00, 0x00},//##Channel_1_EQ13_A0 
       {0x41, 0x00, 0x00, 0x00},//##Channel_1_EQ14_A1 
       {0x42, 0x00, 0x00, 0x00},//##Channel_1_EQ14_A2 
       {0x43, 0x00, 0x00, 0x00},//##Channel_1_EQ14_B1 
       {0x44, 0x00, 0x00, 0x00},//##Channel_1_EQ14_B2 
       {0x45, 0x20, 0x00, 0x00},//##Channel_1_EQ14_A0 
       {0x46, 0x00, 0x00, 0x00},//##Channel_1_EQ15_A1 
       {0x47, 0x00, 0x00, 0x00},//##Channel_1_EQ15_A2 
       {0x48, 0x00, 0x00, 0x00},//##Channel_1_EQ15_B1 
       {0x49, 0x00, 0x00, 0x00},//##Channel_1_EQ15_B2 
       {0x4a, 0x20, 0x00, 0x00},//##Channel_1_EQ15_A0 
       {0x4b, 0x40, 0x00, 0x00},//##Channel_1_Mixer1 
       {0x4c, 0x40, 0x00, 0x00},//##Channel_1_Mixer2 
       {0x4d, 0x7f, 0xff, 0xff},//##Channel_1_Prescale 
       {0x4e, 0x7f, 0xff, 0xff},//##Channel_1_Postscale 
       {0x4f, 0xc7, 0xb6, 0x91},//##A0_of_L_channel_SRS_HPF 
       {0x50, 0x38, 0x49, 0x6e},//##A1_of_L_channel_SRS_HPF 
       {0x51, 0x0c, 0x46, 0xf8},//##B1_of_L_channel_SRS_HPF 
       {0x52, 0x0e, 0x81, 0xb9},//##A0_of_L_channel_SRS_LPF 
       {0x53, 0xf2, 0x2c, 0x12},//##A1_of_L_channel_SRS_LPF 
       {0x54, 0x0f, 0xca, 0xbb},//##B1_of_L_channel_SRS_LPF 
       {0x55, 0x20, 0x00, 0x00},//##CH1.2_Power_Clipping 
       {0x56, 0x20, 0x00, 0x00},//##CCH1.2_DRC1_Attack_threshold 
       {0x57, 0x08, 0x00, 0x00},//##CH1.2_DRC1_Release_threshold 
       {0x58, 0x20, 0x00, 0x00},//##CH3.4_DRC2_Attack_threshold 
       {0x59, 0x08, 0x00, 0x00},//##CH3.4_DRC2_Release_threshold 
       {0x5a, 0x20, 0x00, 0x00},//##CH5.6_DRC3_Attack_threshold 
       {0x5b, 0x08, 0x00, 0x00},//##CH5.6_DRC3_Release_threshold 
       {0x5c, 0x20, 0x00, 0x00},//##CH7.8_DRC4_Attack_threshold 
       {0x5d, 0x08, 0x00, 0x00},//##CH7.8_DRC4_Release_threshold 
       {0x5e, 0x00, 0x00, 0x1a},//##Noise_Gate_Attack_Level 
       {0x5f, 0x00, 0x00, 0x53},//##Noise_Gate_Release_Level 
       {0x60, 0x00, 0x80, 0x00},//##DRC1_Energy_Coefficient 
       {0x61, 0x00, 0x20, 0x00},//##DRC2_Energy_Coefficient 
       {0x62, 0x00, 0x80, 0x00},//##DRC3_Energy_Coefficient 
       {0x63, 0x00, 0x80, 0x00},//##DRC4_Energy_Coefficient 
       {0x64, 0x03, 0x17, 0x93},//DRC1_Power_Meter
       {0x65, 0x00, 0x00, 0x00},//DRC3_Power_Mete
       {0x66, 0x00, 0x00, 0x00},//DRC5_Power_Meter
       {0x67, 0x00, 0x00, 0x00},//DRC7_Power_Meter
       {0x68, 0x00, 0x00, 0x00},//##Channel_1_DEQ1_A1 
       {0x69, 0x00, 0x00, 0x00},//##Channel_1_DEQ1_A2
       {0x6a, 0x00, 0x00, 0x00},//##Channel_1_DEQ1_B1
       {0x6b, 0x00, 0x00, 0x00},//##Channel_1_DEQ1_B2 
       {0x6c, 0x20, 0x00, 0x00},//##Channel_1_DEQ1_A0
       {0x6d, 0x00, 0x00, 0x00},//##Channel_1_DEQ2_A1 
       {0x6e, 0x00, 0x00, 0x00},//##Channel_1_DEQ2_A2 
       {0x6f, 0x00, 0x00, 0x00},//##Channel_1_DEQ2_B1 
       {0x70, 0x00, 0x00, 0x00},//##Channel_1_DEQ2_B2 
       {0x71, 0x20, 0x00, 0x00},//##Channel_1_DEQ2_A0 
       {0x72, 0x00, 0x00, 0x00},//##Channel_1_DEQ3_A1 
       {0x73, 0x00, 0x00, 0x00},//##Channel_1_DEQ3_A2 
       {0x74, 0x00, 0x00, 0x00},//##Channel_1_DEQ3_B1 
       {0x75, 0x00, 0x00, 0x00},//##Channel_1_DEQ3_B2 
       {0x76, 0x20, 0x00, 0x00},//##Channel_1_DEQ3_A0 
       {0x77, 0x00, 0x00, 0x00},//##Channel_1_DEQ4_A1 
       {0x78, 0x00, 0x00, 0x00},//##Channel_1_DEQ4_A2 
       {0x79, 0x00, 0x00, 0x00},//##Channel_1_DEQ4_B1 
       {0x7a, 0x00, 0x00, 0x00},//##Channel_1_DEQ4_B2 
       {0x7b, 0x20, 0x00, 0x00},//##Channel_1_DEQ4_A0 
       {0x7c, 0x00, 0x00, 0x00},//##Reserve
       {0x7d, 0x00, 0x00, 0x00},//##Reserve
       {0x7e, 0x00, 0x00, 0x00},//##Reserve
       {0x7f, 0x00, 0x00, 0x00},//##Reserve
};

static int spk3w_m_ram2_tab[AD82584F_RAM_TABLE_COUNT][4] = 
{
       {0x00, 0x00, 0x00, 0x00},//##Channel_2_EQ1_A1 
       {0x01, 0x00, 0x00, 0x00},//##Channel_2_EQ1_A2 
       {0x02, 0x00, 0x00, 0x00},//##Channel_2_EQ1_B1 
       {0x03, 0x00, 0x00, 0x00},//##Channel_2_EQ1_B2 
       {0x04, 0x20, 0x00, 0x00},//##Channel_2_EQ1_A0 
       {0x05, 0x00, 0x00, 0x00},//##Channel_2_EQ2_A1 
       {0x06, 0x00, 0x00, 0x00},//##Channel_2_EQ2_A2 
       {0x07, 0x00, 0x00, 0x00},//##Channel_2_EQ2_B1 
       {0x08, 0x00, 0x00, 0x00},//##Channel_2_EQ2_B2 
       {0x09, 0x20, 0x00, 0x00},//##Channel_2_EQ2_A0 
       {0x0a, 0x00, 0x00, 0x00},//##Channel_2_EQ3_A1 
       {0x0b, 0x00, 0x00, 0x00},//##Channel_2_EQ3_A2 
       {0x0c, 0x00, 0x00, 0x00},//##Channel_2_EQ3_B1 
       {0x0d, 0x00, 0x00, 0x00},//##Channel_2_EQ3_B2 
       {0x0e, 0x20, 0x00, 0x00},//##Channel_2_EQ3_A0 
       {0x0f, 0x00, 0x00, 0x00},//##Channel_2_EQ4_A1 
       {0x10, 0x00, 0x00, 0x00},//##Channel_2_EQ4_A2 
       {0x11, 0x00, 0x00, 0x00},//##Channel_2_EQ4_B1 
       {0x12, 0x00, 0x00, 0x00},//##Channel_2_EQ4_B2 
       {0x13, 0x20, 0x00, 0x00},//##Channel_2_EQ4_A0 
       {0x14, 0x00, 0x00, 0x00},//##Channel_2_EQ5_A1 
       {0x15, 0x00, 0x00, 0x00},//##Channel_2_EQ5_A2 
       {0x16, 0x00, 0x00, 0x00},//##Channel_2_EQ5_B1 
       {0x17, 0x00, 0x00, 0x00},//##Channel_2_EQ5_B2 
       {0x18, 0x20, 0x00, 0x00},//##Channel_2_EQ5_A0 
       {0x19, 0x00, 0x00, 0x00},//##Channel_2_EQ6_A1 
       {0x1a, 0x00, 0x00, 0x00},//##Channel_2_EQ6_A2 
       {0x1b, 0x00, 0x00, 0x00},//##Channel_2_EQ6_B1 
       {0x1c, 0x00, 0x00, 0x00},//##Channel_2_EQ6_B2 
       {0x1d, 0x20, 0x00, 0x00},//##Channel_2_EQ6_A0 
       {0x1e, 0x00, 0x00, 0x00},//##Channel_2_EQ7_A1 
       {0x1f, 0x00, 0x00, 0x00},//##Channel_2_EQ7_A2 
       {0x20, 0x00, 0x00, 0x00},//##Channel_2_EQ7_B1 
       {0x21, 0x00, 0x00, 0x00},//##Channel_2_EQ7_B2 
       {0x22, 0x20, 0x00, 0x00},//##Channel_2_EQ7_A0 
       {0x23, 0x00, 0x00, 0x00},//##Channel_2_EQ8_A1 
       {0x24, 0x00, 0x00, 0x00},//##Channel_2_EQ8_A2 
       {0x25, 0x00, 0x00, 0x00},//##Channel_2_EQ8_B1 
       {0x26, 0x00, 0x00, 0x00},//##Channel_2_EQ8_B2 
       {0x27, 0x20, 0x00, 0x00},//##Channel_2_EQ8_A0 
       {0x28, 0x00, 0x00, 0x00},//##Channel_2_EQ9_A1 
       {0x29, 0x00, 0x00, 0x00},//##Channel_2_EQ9_A2 
       {0x2a, 0x00, 0x00, 0x00},//##Channel_2_EQ9_B1 
       {0x2b, 0x00, 0x00, 0x00},//##Channel_2_EQ9_B2 
       {0x2c, 0x20, 0x00, 0x00},//##Channel_2_EQ9_A0 
       {0x2d, 0x00, 0x00, 0x00},//##Channel_2_EQ10_A1 
       {0x2e, 0x00, 0x00, 0x00},//##Channel_2_EQ10_A2 
       {0x2f, 0x00, 0x00, 0x00},//##Channel_2_EQ10_B1 
       {0x30, 0x00, 0x00, 0x00},//##Channel_2_EQ10_B2 
       {0x31, 0x20, 0x00, 0x00},//##Channel_2_EQ10_A0 
       {0x32, 0x00, 0x00, 0x00},//##Channel_2_EQ11_A1 
       {0x33, 0x00, 0x00, 0x00},//##Channel_2_EQ11_A2 
       {0x34, 0x00, 0x00, 0x00},//##Channel_2_EQ11_B1 
       {0x35, 0x00, 0x00, 0x00},//##Channel_2_EQ11_B2 
       {0x36, 0x20, 0x00, 0x00},//##Channel_2_EQ11_A0 
       {0x37, 0x00, 0x00, 0x00},//##Channel_2_EQ12_A1 
       {0x38, 0x00, 0x00, 0x00},//##Channel_2_EQ12_A2 
       {0x39, 0x00, 0x00, 0x00},//##Channel_2_EQ12_B1 
       {0x3a, 0x00, 0x00, 0x00},//##Channel_2_EQ12_B2 
       {0x3b, 0x20, 0x00, 0x00},//##Channel_2_EQ12_A0 
       {0x3c, 0x00, 0x00, 0x00},//##Channel_2_EQ13_A1 
       {0x3d, 0x00, 0x00, 0x00},//##Channel_2_EQ13_A2 
       {0x3e, 0x00, 0x00, 0x00},//##Channel_2_EQ13_B1 
       {0x3f, 0x00, 0x00, 0x00},//##Channel_2_EQ13_B2 
       {0x40, 0x20, 0x00, 0x00},//##Channel_2_EQ13_A0 
       {0x41, 0x00, 0x00, 0x00},//##Channel_2_EQ14_A1 
       {0x42, 0x00, 0x00, 0x00},//##Channel_2_EQ14_A2 
       {0x43, 0x00, 0x00, 0x00},//##Channel_2_EQ14_B1 
       {0x44, 0x00, 0x00, 0x00},//##Channel_2_EQ14_B2 
       {0x45, 0x20, 0x00, 0x00},//##Channel_2_EQ14_A0 
       {0x46, 0x00, 0x00, 0x00},//##Channel_2_EQ15_A1 
       {0x47, 0x00, 0x00, 0x00},//##Channel_2_EQ15_A2 
       {0x48, 0x00, 0x00, 0x00},//##Channel_2_EQ15_B1 
       {0x49, 0x00, 0x00, 0x00},//##Channel_2_EQ15_B2 
       {0x4a, 0x20, 0x00, 0x00},//##Channel_2_EQ15_A0 
       {0x4b, 0x40, 0x00, 0x00},//##Channel_2_Mixer1 
       {0x4c, 0x40, 0x00, 0x00},//##Channel_2_Mixer2 
       {0x4d, 0x7f, 0xff, 0xff},//##Channel_2_Prescale 
       {0x4e, 0x7f, 0xff, 0xff},//##Channel_2_Postscale 
       {0x4f, 0xc7, 0xb6, 0x91},//##A0_of_R_channel_SRS_HPF 
       {0x50, 0x38, 0x49, 0x6e},//##A1_of_R_channel_SRS_HPF 
       {0x51, 0x0c, 0x46, 0xf8},//##B1_of_R_channel_SRS_HPF 
       {0x52, 0x0e, 0x81, 0xb9},//##A0_of_R_channel_SRS_LPF 
       {0x53, 0xf2, 0x2c, 0x12},//##A1_of_R_channel_SRS_LPF 
       {0x54, 0x0f, 0xca, 0xbb},//##B1_of_R_channel_SRS_LPF 
       {0x55, 0x00, 0x00, 0x00},//##Reserve
       {0x56, 0x00, 0x00, 0x00},//##Reserve
       {0x57, 0x00, 0x00, 0x00},//##Reserve
       {0x58, 0x00, 0x00, 0x00},//##Reserve
       {0x59, 0x00, 0x00, 0x00},//##Reserve
       {0x5a, 0x00, 0x00, 0x00},//##Reserve
       {0x5b, 0x00, 0x00, 0x00},//##Reserve
       {0x5c, 0x00, 0x00, 0x00},//##Reserve
       {0x5d, 0x00, 0x00, 0x00},//##Reserve
       {0x5e, 0x00, 0x00, 0x00},//##Reserve
       {0x5f, 0x00, 0x00, 0x00},//##Reserve
       {0x60, 0x00, 0x00, 0x00},//##Reserve
       {0x61, 0x00, 0x00, 0x00},//##Reserve
       {0x62, 0x00, 0x00, 0x00},//##Reserve
       {0x63, 0x00, 0x00, 0x00},//##Reserve
       {0x64, 0x03, 0x20, 0x21},//DRC2_Power_Meter
       {0x65, 0x00, 0x00, 0x00},//DRC4_Power_Mete
       {0x66, 0x00, 0x00, 0x00},//DRC6_Power_Meter
       {0x67, 0x00, 0x00, 0x00},//DRC8_Power_Meter
       {0x68, 0x00, 0x00, 0x00},//##Channel_2_DEQ1_A1 
       {0x69, 0x00, 0x00, 0x00},//##Channel_2_DEQ1_A2
       {0x6a, 0x00, 0x00, 0x00},//##Channel_2_DEQ1_B1
       {0x6b, 0x00, 0x00, 0x00},//##Channel_2_DEQ1_B2 
       {0x6c, 0x20, 0x00, 0x00},//##Channel_2_DEQ1_A0
       {0x6d, 0x00, 0x00, 0x00},//##Channel_2_DEQ2_A1 
       {0x6e, 0x00, 0x00, 0x00},//##Channel_2_DEQ2_A2 
       {0x6f, 0x00, 0x00, 0x00},//##Channel_2_DEQ2_B1 
       {0x70, 0x00, 0x00, 0x00},//##Channel_2_DEQ2_B2 
       {0x71, 0x20, 0x00, 0x00},//##Channel_2_DEQ2_A0 
       {0x72, 0x00, 0x00, 0x00},//##Channel_2_DEQ3_A1 
       {0x73, 0x00, 0x00, 0x00},//##Channel_2_DEQ3_A2 
       {0x74, 0x00, 0x00, 0x00},//##Channel_2_DEQ3_B1 
       {0x75, 0x00, 0x00, 0x00},//##Channel_2_DEQ3_B2 
       {0x76, 0x20, 0x00, 0x00},//##Channel_2_DEQ3_A0 
       {0x77, 0x00, 0x00, 0x00},//##Channel_2_DEQ4_A1 
       {0x78, 0x00, 0x00, 0x00},//##Channel_2_DEQ4_A2 
       {0x79, 0x00, 0x00, 0x00},//##Channel_2_DEQ4_B1 
       {0x7a, 0x00, 0x00, 0x00},//##Channel_2_DEQ4_B2 
       {0x7b, 0x20, 0x00, 0x00},//##Channel_2_DEQ4_A0 
       {0x7c, 0x00, 0x00, 0x00},//##Reserve
       {0x7d, 0x00, 0x00, 0x00},//##Reserve
       {0x7e, 0x00, 0x00, 0x00},//##Reserve
       {0x7f, 0x00, 0x00, 0x00},//##Reserve
};

void init_reg_and_ram_tab(void)
{
	int i,j;
	if(10 == g_speaker_power){
		for(i = 0;i < AD82584F_REGISTER_COUNT;i++){
			m_reg_tab[i][0] = spk10w_m_reg_tab[i][0];
			m_reg_tab[i][1] = spk10w_m_reg_tab[i][1];
		}

		for(i = 0;i < AD82584F_RAM_TABLE_COUNT;i++){
			for(j=0;j<4;j++){
				m_ram1_tab[i][j] = spk10w_m_ram1_tab[i][j];
				m_ram2_tab[i][j] = spk10w_m_ram2_tab[i][j];
			}
		}
	}else{
		for(i = 0;i < AD82584F_REGISTER_COUNT;i++){
			m_reg_tab[i][0] = spk3w_m_reg_tab[i][0];
			m_reg_tab[i][1] = spk3w_m_reg_tab[i][1];
		}

		for(i = 0;i < AD82584F_RAM_TABLE_COUNT;i++){
			for(j=0;j<4;j++){
				m_ram1_tab[i][j] = spk3w_m_ram1_tab[i][j];
				m_ram2_tab[i][j] = spk3w_m_ram2_tab[i][j];
			}
		}
	}
}

/* codec private data */
struct ad82584f_priv {
	struct regmap *regmap;
	struct snd_soc_codec *codec;
	struct ad82584f_platform_data *pdata;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif
};

static int ad82584f_set_dai_sysclk(struct snd_soc_dai *codec_dai,
				  int clk_id, unsigned int freq, int dir)
{
	printk("%s\n", __func__);

	return 0;
}

static int ad82584f_set_dai_fmt(struct snd_soc_dai *codec_dai, unsigned int fmt)
{
	printk("%s\n", __func__);

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBS_CFS:
		break;
	default:
		return 0;
	}

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
	case SND_SOC_DAIFMT_RIGHT_J:
	case SND_SOC_DAIFMT_LEFT_J:
		break;
	default:
		return 0;
	}

	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		break;
	case SND_SOC_DAIFMT_NB_IF:
		break;
	default:
		return 0;
	}

	return 0;
}

static int ad82584f_hw_params(struct snd_pcm_substream *substream,
			     struct snd_pcm_hw_params *params,
			     struct snd_soc_dai *dai)
{
	unsigned int rate;

	rate = params_rate(params);
	printk("%s,rate: %u\n", __func__, rate);

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S24_LE:
	case SNDRV_PCM_FORMAT_S24_BE:
		printk("%s,24bit\n", __func__);
		break;
	case SNDRV_PCM_FORMAT_S32_LE:
		printk("%s,32bit\n", __func__);
		break;      
		
	case SNDRV_PCM_FORMAT_S20_3LE:
	case SNDRV_PCM_FORMAT_S20_3BE:
		printk("%s,20bit\n", __func__);
		break;
	case SNDRV_PCM_FORMAT_S16_LE:
	case SNDRV_PCM_FORMAT_S16_BE:
		printk("%s,16bit\n", __func__);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int ad82584f_dai_digital_mute(struct snd_soc_dai *codec_dai,
				  int mute)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	struct ad82584f_priv *ad82584f = snd_soc_codec_get_drvdata(codec);

	dev_info(codec->dev, "%s : %s\n", __func__, mute ? "MUTE" : "UNMUTE");

	if (!mute) {
		snd_soc_write(codec, STATE_CTL_3, 0x00);//--unmute amp
	}else{
		snd_soc_write(codec, STATE_CTL_3, 0x7f);//--mute amp
	}

	return 0;
}

static int ad82584f_set_bias_level(struct snd_soc_codec *codec,
				  enum snd_soc_bias_level level)
{
	printk("%s,level = %d\n", __func__, level);

	switch (level) {
	case SND_SOC_BIAS_ON:
		break;

	case SND_SOC_BIAS_PREPARE:
		/* Full power on */
		break;

	case SND_SOC_BIAS_STANDBY:
		break;

	case SND_SOC_BIAS_OFF:
		/* The chip runs through the power down sequence for us. */
		break;
	}
	//codec->dapm.bias_level = level;         ---rokid
	//codec->component.dapm.bias_level = level;

	return 0;
}

static int ad82584f_startup(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct ad82584f_priv *ad82584f = snd_soc_codec_get_drvdata(codec);
	int ret = 0;

	gpio_direction_output(ad82584f->pdata->reset_pin,1);
	dev_info(codec->dev, "%s reset_pin=%d\n", __func__,
		 gpio_get_value(ad82584f->pdata->reset_pin));

	return ret;
}

static int ad82584f_shutdown(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct ad82584f_priv *ad82584f = snd_soc_codec_get_drvdata(codec);
	int ret = 0;

	gpio_direction_output(ad82584f->pdata->reset_pin,0);
	dev_info(codec->dev, "%s reset_pin=%d\n", __func__,
		 gpio_get_value(ad82584f->pdata->reset_pin));

	return ret;
}

static const struct snd_soc_dai_ops ad82584f_dai_ops = {
	.hw_params = ad82584f_hw_params,
	.set_sysclk = ad82584f_set_dai_sysclk,
	.set_fmt = ad82584f_set_dai_fmt,
	.digital_mute = ad82584f_dai_digital_mute,
	.startup      = ad82584f_startup,
	.shutdown     = ad82584f_shutdown,
};

static struct snd_soc_dai_driver ad82584f_dai = {
	.name = "ad82584f-dai",
	.playback = {
		.stream_name = "Playback",
		.channels_min = 1,
		.channels_max = 2,
		.rates = AD82584F_RATES,
		.formats = AD82584F_FORMATS,
	},
	.capture = {
		.stream_name = "Capture",
		.channels_min = 1,
		.channels_max = 2,
		.rates = AD82584F_RATES,
		.formats = AD82584F_FORMATS,
	},
	.ops = &ad82584f_dai_ops,
};

static int ad82584f_set_eq_drc(struct snd_soc_codec *codec)
{
	u8 i;
	// ch1 ram
	for (i = 0; i < AD82584F_RAM_TABLE_COUNT; i++) {
		snd_soc_write(codec, CFADDR, m_ram1_tab[i][0]);
		snd_soc_write(codec, A1CF1, m_ram1_tab[i][1]);
		snd_soc_write(codec, A1CF2, m_ram1_tab[i][2]);
		snd_soc_write(codec, A1CF3, m_ram1_tab[i][3]);
		snd_soc_write(codec, CFUD, 0x01);
	}
	// ch2 ram
	for (i = 0; i < AD82584F_RAM_TABLE_COUNT; i++) {
		snd_soc_write(codec, CFADDR, m_ram2_tab[i][0]);
		snd_soc_write(codec, A1CF1, m_ram2_tab[i][1]);
		snd_soc_write(codec, A1CF2, m_ram2_tab[i][2]);
		snd_soc_write(codec, A1CF3, m_ram2_tab[i][3]);
		snd_soc_write(codec, CFUD, 0x41);
	}
	return 0;
}

static int ad82584f_reg_init(struct snd_soc_codec *codec)
{
	int i = 0;
	for (i = 0; i < AD82584F_REGISTER_COUNT; i++) {
		snd_soc_write(codec, m_reg_tab[i][0], m_reg_tab[i][1]);
	};
       
	return 0;

}

#ifdef	AD82584F_REG_RAM_CHECK
static int ad82584f_reg_check(struct snd_soc_codec *codec)
{
	int i = 0;
	int reg_data = 0;
	
	for (i = 0; i < AD82584F_REGISTER_COUNT; i++) {
		reg_data = snd_soc_read(codec, m_reg_tab[i][0]);
	};
	return 0;
}

static int ad82584f_eqram1_check(struct snd_soc_codec *codec)
{
	int i = 0;
	int H8_data = 0, M8_data = 0, L8_data = 0;

	for (i = 0; i < AD82584F_RAM_TABLE_COUNT; i++) {
		snd_soc_write(codec, CFADDR, m_ram1_tab[i][0]);			// write ram addr
		snd_soc_write(codec, CFUD, 0x04);						// write read ram cmd
		
		H8_data = snd_soc_read(codec, A1CF1);
		M8_data = snd_soc_read(codec, A1CF2);
		L8_data = snd_soc_read(codec, A1CF3);
		//printk("ad82584f_set_eq_drc ram1  write 0x%x = 0x%x , 0x%x , 0x%x\n", m_ram1_tab[i][0], H8_data,M8_data,L8_data);
	};
	return 0;
}

#endif

static int ad82584f_init(struct snd_soc_codec *codec)
{
	// init AMP 
	dev_info(codec->dev, "%s\n", __func__);

	snd_soc_write(codec, STATE_CTL_3, 0x7f);//--mute amp

	// write amp register
	ad82584f_reg_init(codec);

	snd_soc_write(codec, STATE_CTL_3, 0x7f);//--mute amp
	udelay(100);
	// write amp ram (eq and drc ... ) 
	ad82584f_set_eq_drc(codec);
	udelay(100);

#ifdef	AD82584F_REG_RAM_CHECK
	ad82584f_reg_check(codec);
	ad82584f_eqram1_check(codec);
#endif	

	/*unmute,default power-on is mute.*/
	snd_soc_write(codec, STATE_CTL_3, 0x00);//--unmute amp

	return 0;
}

static int ad82584f_probe(struct snd_soc_codec *codec)
{

	dev_info(codec->dev, "%s\n", __func__);

	codec->control_data = container_of(codec->dev, struct i2c_client, dev);

#ifdef CONFIG_HAS_EARLYSUSPEND
	struct ad82584f_priv *ad82584f = snd_soc_codec_get_drvdata(codec);

	ad82584f->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN;
	ad82584f->early_suspend.suspend = ad82584f_early_suspend;
	ad82584f->early_suspend.resume = ad82584f_late_resume;
	ad82584f->early_suspend.param = codec;
	register_early_suspend(&(ad82584f->early_suspend));
#endif

	ad82584f_init(codec);
	
	return 0;
}

static int ad82584f_remove(struct snd_soc_codec *codec)
{
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct ad82584f_priv *ad82584f = snd_soc_codec_get_drvdata(codec);

	unregister_early_suspend(&(ad82584f->early_suspend));
#endif
	return 0;
}

#ifdef CONFIG_PM
static int ad82584f_suspend(struct snd_soc_codec *codec)
{
	dev_info(codec->dev, "%s\n", __func__);

	return 0;
}

static int ad82584f_resume(struct snd_soc_codec *codec)
{
	dev_info(codec->dev, "%s\n", __func__);

	return 0;
}
#else
#define ad82584f_suspend NULL
#define ad82584f_resume NULL
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
static void ad82584f_early_suspend(struct early_suspend *h)
{
}

static void ad82584f_late_resume(struct early_suspend *h)
{
}
#endif

static const struct snd_soc_dapm_widget ad82584f_dapm_widgets[] = {
	SND_SOC_DAPM_DAC("DAC", "Playback", SND_SOC_NOPM, 0, 0),
};

static const struct snd_soc_codec_driver soc_codec_dev_ad82584f = {
	.probe = ad82584f_probe,
	.remove = ad82584f_remove,
	.suspend = ad82584f_suspend,
	.resume = ad82584f_resume,
	.set_bias_level = ad82584f_set_bias_level,
	.reg_cache_size = ARRAY_SIZE(m_reg_tab),
	.reg_word_size = sizeof(u16),
	.reg_cache_default = m_reg_tab,
	.component_driver = {
		.controls = ad82584f_snd_controls,
		.num_controls = ARRAY_SIZE(ad82584f_snd_controls),
		.dapm_widgets = ad82584f_dapm_widgets,
		.num_dapm_widgets = ARRAY_SIZE(ad82584f_dapm_widgets),
	}
};

static bool ad82584f_readable_register(struct device *dev, unsigned int reg)
{
	if (reg > 0xFF)
		return false;

	switch (reg) {
	case 0x00 ... 0xFF:
		return true;
	default:
		return false;
	}
}

static bool ad82584f_writeable_register(struct device *dev, unsigned int reg)
{
	if (reg > 0xFF)
		return false;

	switch (reg) {
	case 0x00 ... 0xFF:
		return true;
	default:
		return false;
	}
}

static bool ad82584f_volatile_register(struct device *dev, unsigned int reg)
{
	if (reg > 0xFF)
		return false;

	switch (reg) {
	case 0x00 ... 0xFF:
		return true;
	default:
		return false;
	}
}

const struct regmap_config ad82584f_regmap = {
	.reg_bits = 8,
	.val_bits = 8,

	.max_register = 0xFF,
	.readable_reg = ad82584f_readable_register,
	.writeable_reg = ad82584f_writeable_register,
	.volatile_reg = ad82584f_volatile_register,

	.cache_type = REGCACHE_RBTREE,
	//.reg_defaults = ad82584f_reg_def,
	//.num_reg_defaults = ARRAY_SIZE(ad82584f_reg_def),
};

static int ad82584f_i2c_probe(struct i2c_client *i2c,
			     const struct i2c_device_id *id)
{
	struct ad82584f_priv *ad82584f;
	struct ad82584f_platform_data *pdata;
	enum of_gpio_flags flags;
	int ret;
	int value_id;
	const char *dev_name = NULL;
	struct device_node *np = i2c->dev.of_node;

	printk("%s\n", __func__);
	ad82584f = devm_kzalloc(&i2c->dev, sizeof(struct ad82584f_priv), GFP_KERNEL);
	if (!ad82584f){
		printk("%s,%d,Fail to devm_kzalloc\n", __func__,__LINE__);
		return -ENOMEM;
	}

	ad82584f->regmap = devm_regmap_init_i2c(i2c, &ad82584f_regmap);   //---rokid
	if (IS_ERR(ad82584f->regmap)) {
		ret = PTR_ERR(ad82584f->regmap);
		dev_err(&i2c->dev, "Failed to allocate register map: %d\n", ret);
		return ret;
	}

	ret = of_property_read_u32(np, "speaker-power", &g_speaker_power);
	if((ret < 0) || ((g_speaker_power != 3) && (g_speaker_power != 10))){
		dev_err(&i2c->dev, "Failed to get  speaker-power,ret = %d, g_speaker_power=%d\n", ret, g_speaker_power);
		g_speaker_power = 3;
	}
	printk("%s g_speaker_power=%d\n", __func__, g_speaker_power);
	init_reg_and_ram_tab();


	ret = regmap_read(ad82584f->regmap, DEVICE_ID, &value_id);
	if (ret < 0 || (value_id != 0x52)) {
		dev_err(&i2c->dev, "Failed to read device id from the ad82584f: ret=%d,value_id(%d) != 0x52\n", ret, value_id);
		return ret;
	}

	ret = of_property_read_string_index(np, "dev-name", 0, &dev_name);
	if (ret == 0)
		dev_set_name(&i2c->dev, dev_name);

	pdata = devm_kzalloc(&i2c->dev, sizeof(struct ad82584f_platform_data), GFP_KERNEL);
	if (!pdata) {
		dev_err(&i2c->dev, "%s failed to kzalloc for ad82584f pdata\n", __func__);
		return -ENOMEM;
	}
	ad82584f->pdata = pdata;

	ad82584f->pdata->reset_pin = of_get_named_gpio_flags(i2c->dev.of_node, "dsp_reset_pin", 0, &flags);

	if (ad82584f->pdata->reset_pin < 0) {
		dev_err(&i2c->dev, "%s() Can not read property reset_pin\n", __func__);
		ad82584f->pdata->reset_pin = -1;
	} else {
	    ret = gpio_request(ad82584f->pdata->reset_pin, NULL);
	    if (ret != 0) {
		    dev_err(&i2c->dev, "%s request reset_pin error", __func__);
		    return ret;
	    }
	    dev_info(&i2c->dev, "%s set reset_pin high\n", __func__);
	    gpio_direction_output(ad82584f->pdata->reset_pin,1);
	}

	//ad82584f_parse_dt(ad82584f, i2c->dev.of_node);

	i2c_set_clientdata(i2c, ad82584f);

	ret = snd_soc_register_codec(&i2c->dev, &soc_codec_dev_ad82584f, &ad82584f_dai, 1);
	if (ret != 0){
	    dev_err(&i2c->dev, "%s,%d,Failed to register codec\n", __func__,__LINE__);
    }

	return ret;
}

static int ad82584f_i2c_remove(struct i2c_client *client)
{
	pr_info("%s\n", __func__);
	snd_soc_unregister_codec(&client->dev);
	return 0;
}

static int ad82584f_i2c_shutdown(struct i2c_client *client)
{
	pr_info("%s\n", __func__);
	return 0;
}

static const struct i2c_device_id ad82584f_i2c_id[] = {
	{ "ad82584f", 0 },
	{}
};

MODULE_DEVICE_TABLE(i2c, ad82584f_i2c_id);

static const struct of_device_id ad82584f_of_id[] = {
	{ .compatible = "ESMT, ad82584f", },
	{ /* senitel */ }
};
MODULE_DEVICE_TABLE(of, ad82584f_of_id);

#ifdef CONFIG_PM_SLEEP
static int ad82584f_i2c_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ad82584f_priv *ad82584f = i2c_get_clientdata(client);

	pr_info("%s\n", __func__);
	return 0;
}

static int ad82584f_i2c_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ad82584f_priv *ad82584f = i2c_get_clientdata(client);

	pr_info("%s\n", __func__);
	return 0;
}
#endif /* CONFIG_PM_SLEEP */

#ifdef CONFIG_PM
static int ad82584f_i2c_runtime_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ad82584f_priv *ad82584f = i2c_get_clientdata(client);

	pr_info("%s\n", __func__);
	return 0;
}

static int ad82584f_i2c_runtime_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ad82584f_priv *ad82584f = i2c_get_clientdata(client);

	pr_info("%s\n", __func__);
	return 0;
}
#endif /* CONFIG_PM */

static const struct dev_pm_ops ad82584f_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(ad82584f_i2c_suspend,
				ad82584f_i2c_resume)
	SET_RUNTIME_PM_OPS(ad82584f_i2c_runtime_suspend,
			   ad82584f_i2c_runtime_resume,
			   NULL)
};

static struct i2c_driver ad82584f_i2c_driver = {
	.driver = {
		.name = "ad82584f",
		.pm     = &ad82584f_pm_ops,
		.of_match_table = ad82584f_of_id,
		.owner = THIS_MODULE,
	},
	.shutdown = ad82584f_i2c_shutdown,
	.probe = ad82584f_i2c_probe,
	.remove = ad82584f_i2c_remove,
	.id_table = ad82584f_i2c_id,
};

module_i2c_driver(ad82584f_i2c_driver);
MODULE_DESCRIPTION("ASoC ad82584f driver");
MODULE_AUTHOR("AML MM team");
MODULE_LICENSE("GPL");
