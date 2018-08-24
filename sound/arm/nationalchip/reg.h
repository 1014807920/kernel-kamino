#ifndef __GX8010_REG_H__
#define __GX8010_REG_H__

enum dev_channel_sel {
	R0_CHANNEL_SEL = (1<<0),
	R1_CHANNEL_SEL = (1<<1),
	R2_CHANNEL_SEL = (1<<2),
	W0_CHANNEL_SEL = (1<<3)
};

#define REG_SET_VAL(reg, value) do {                            \
	(*(volatile unsigned int *)reg) = value;                    \
} while(0)

#define REG_GET_VAL(reg)                                        \
	(*(volatile unsigned int *)reg)

#define REG_SET_BIT(reg, bit) do {                              \
	(*(volatile unsigned int *)reg) |= (0x1<<bit);              \
} while(0)

#define REG_CLR_BIT(reg, bit) do {                              \
	(*(volatile unsigned int *)reg) &= ~(0x1<<bit);             \
} while(0)

#define REG_GET_BIT(reg, bit)                                   \
	((*(volatile unsigned int *)reg)&(0x1<<bit))

#define REG_SET_FIELD(reg, val, mask, offset) do {              \
	unsigned int Reg = *(volatile unsigned int *)reg;           \
	Reg &= ~((mask)<<(offset));                                 \
	Reg |= ((val)&(mask))<<(offset);                            \
	(*(volatile unsigned int *)reg) = Reg;                      \
} while(0)

#define REG_GET_FIELD(reg, mask, offset)                        \
	(((*(volatile unsigned int *)reg)>>(offset))&(mask))

struct aout_reg {
	unsigned int CTRL                          ;
	unsigned int I2S_DACINFO                   ;
	unsigned int SPDIF_CL1                     ;
	unsigned int SPDIF_CL2                     ;
	unsigned int SPDIF_CR1                     ;
	unsigned int SPDIF_CR2                     ;
	unsigned int SPDIF_U                       ;
	unsigned int CHANNEL_SEL                   ;
	unsigned int PCM_W_NUM                     ;
	unsigned int PCM_W_BUFFER_SADDR            ;
	unsigned int PCM_W_BUFFER_SIZE             ;
	unsigned int PCM_W_SDC_ADDR                ;
	unsigned int INTEN                         ;
	unsigned int INT                           ;
	unsigned int RESERVED[2]                   ;
	unsigned int R1_INDATA                     ;
	unsigned int R1_VOL_CTRL                   ;
	unsigned int R1_BUFFER_START_ADDR          ;
	unsigned int R1_BUFFER_SIZE                ;
	unsigned int R1_BUFFER_SDC_ADDR            ;
	unsigned int R1_newFRAME_START_ADDR        ;
	unsigned int R1_newFRAME_END_ADDR          ;
	unsigned int R1_playingFRAME_START_ADDR    ;
	unsigned int R1_playingFRAME_END_ADDR      ;
	unsigned int R1_newFrame_pcmLEN            ;
	unsigned int R1_playingFrame_pcmLEN        ;
	unsigned int R1_SET_NEWFRAME_CTRL          ;
	unsigned int R2_INDATA                     ;
	unsigned int R2_VOL_CTRL                   ;
	unsigned int R2_BUFFER_START_ADDR          ;
	unsigned int R2_BUFFER_SIZE                ;
	unsigned int R2_BUFFER_SDC_ADDR            ;
	unsigned int R2_newFRAME_START_ADDR        ;
	unsigned int R2_newFRAME_END_ADDR          ;
	unsigned int R2_playingFRAME_START_ADDR    ;
	unsigned int R2_playingFRAME_END_ADDR      ;
	unsigned int R2_newFrame_pcmLEN            ;
	unsigned int R2_playingFrame_pcmLEN        ;
	unsigned int R2_SET_NEWFRAME_CTRL          ;
	unsigned int R3_INDATA                     ;
	unsigned int R3_VOL_CTRL                   ;
	unsigned int R3_BUFFER_START_ADDR          ;
	unsigned int R3_BUFFER_SIZE                ;
	unsigned int R3_BUFFER_SDC_ADDR            ;
	unsigned int R3_newFRAME_START_ADDR        ;
	unsigned int R3_newFRAME_END_ADDR          ;
	unsigned int R3_playingFRAME_START_ADDR    ;
	unsigned int R3_playingFRAME_END_ADDR      ;
	unsigned int R3_newFrame_pcmLEN            ;
	unsigned int R3_playingFrame_pcmLEN        ;
	unsigned int R3_SET_NEWFRAME_CTRL          ;
	unsigned int SPDIF_CL3                     ;
	unsigned int SPDIF_CL4                     ;
	unsigned int SPDIF_CR3                     ;
	unsigned int SPDIF_CR4                     ;
	unsigned int SPDIF_CL5                     ;
	unsigned int SPDIF_CL6                     ;
	unsigned int SPDIF_CR5                     ;
	unsigned int SPDIF_CR6                     ;
	unsigned int I2S_IN_INFO                   ;
};

struct aout_lodec {
	unsigned int RESERVED[1]                   ;
	unsigned int LODEC                         ;
};

#if defined(CONFIG_ARCH_LEO_MPW)
struct aout_irq {
	unsigned int INT_STATUS                    ;
	unsigned int INT_CLEAR                     ;
	unsigned int INT_ENABLE                    ;
};

static inline void aout_irq_set_enable(struct aout_irq *irq, bool enable)
{
	if (enable)
		REG_SET_BIT(&(irq->INT_ENABLE), 12);
	else
		REG_CLR_BIT(&(irq->INT_ENABLE), 12);
}

#endif

static inline void aout_spd_set_enable(struct aout_reg *reg, bool enable)
{
	if (enable)
		REG_SET_BIT(&(reg->CTRL), 10);
	else
		REG_CLR_BIT(&(reg->CTRL), 10);
}

static inline void aout_spd_set_mode(struct aout_reg *reg, unsigned int value)
{
	REG_SET_FIELD(&(reg->CTRL), value, 0x3, 8);
}

static inline void aout_spd_set_mute(struct aout_reg *reg, bool mute)
{
	if (mute)
		REG_SET_BIT(&(reg->CTRL), 30);
	else
		REG_CLR_BIT(&(reg->CTRL), 30);
}

static inline bool aout_spd_get_mute(struct aout_reg *reg)
{
	return (REG_GET_BIT(&(reg->CTRL), 30))?true:false;
}

static inline void aout_i2s_set_mute(struct aout_reg *reg, bool mute)
{
	if (mute)
		REG_SET_BIT(&(reg->CTRL), 29);
	else
		REG_CLR_BIT(&(reg->CTRL), 29);
}

static inline bool aout_i2s_get_mute(struct aout_reg *reg)
{
	return (REG_GET_BIT(&(reg->CTRL), 29))?true:false;
}

static inline void aout_dac_set_format(struct aout_reg *reg, unsigned int value)
{
	REG_SET_FIELD(&(reg->I2S_DACINFO), value, 0x3, 0);
}

static inline unsigned int aout_dac_get_format(struct aout_reg *reg)
{
	return REG_GET_FIELD(&(reg->I2S_DACINFO), 0x3, 0);
}

static inline void aout_dac_set_bits(struct aout_reg *reg, unsigned int value)
{
	REG_SET_FIELD(&(reg->I2S_DACINFO), value, 0x3, 2);
}

static inline unsigned int aout_dac_get_bits(struct aout_reg *reg)
{
	return REG_GET_FIELD(&(reg->I2S_DACINFO), 0x3, 2);
}

static inline void aout_dac_set_mclk(struct aout_reg *reg, unsigned int value)
{
	REG_SET_FIELD(&(reg->I2S_DACINFO), value, 0x3, 4);
}

static inline unsigned int aout_dac_get_mclk(struct aout_reg *reg)
{
	return REG_GET_FIELD(&(reg->I2S_DACINFO), 0x3, 4);
}

static inline void aout_dac_set_l_channel(struct aout_reg *reg, unsigned int value)
{
	REG_SET_FIELD(&(reg->I2S_DACINFO), value, 0x7, 24);
}

static inline void aout_dac_set_r_channel(struct aout_reg *reg, unsigned int value)
{
	REG_SET_FIELD(&(reg->I2S_DACINFO), value, 0x7, 28);
}

static inline void aout_cpu_request_open(struct aout_reg *reg)
{
	REG_SET_BIT(&(reg->INT), 21);
}

static inline void aout_dev_set_axi_idle_irq_enable(struct aout_reg *reg, bool enable)
{
	if (enable)
		REG_SET_BIT(&(reg->INTEN), 20);
	else
		REG_CLR_BIT(&(reg->INTEN), 20);
}

static inline void aout_dev_clr_axi_idle_irq_status(struct aout_reg *reg)
{
	REG_SET_VAL(&(reg->INT), (0x1<<20));
}

static inline void aout_cpu_request_stop(struct aout_reg *reg)
{
	REG_SET_BIT(&(reg->INT), 22);
}

static inline bool aout_cpu_axi_get_status(struct aout_reg *reg)
{
	return (REG_GET_BIT(&(reg->INT), 20))?true:false;
}

static inline void aout_cold_reset(unsigned int *rst_reg)
{
	REG_SET_BIT(rst_reg, 1);
	REG_CLR_BIT(rst_reg, 1);
}

static inline void aout_cpu_set_reset_enable(struct aout_reg *reg, bool enable)
{
	if (enable)
		REG_SET_BIT(&(reg->CTRL), 31);
	else
		REG_CLR_BIT(&(reg->CTRL), 31);
}

static inline void aout_dev_set_i2s_disable(struct aout_reg *reg, bool enable)
{
	if (enable)
		REG_SET_BIT(&(reg->CTRL), 11);
	else
		REG_CLR_BIT(&(reg->CTRL), 11);
}

static inline void aout_dev_set_mix(struct aout_reg *reg, enum dev_channel_sel r, bool mix)
{
	if (mix) {
		if ((R0_CHANNEL_SEL & r) == R0_CHANNEL_SEL)
			REG_SET_BIT(&(reg->CTRL), 0);
		if ((R1_CHANNEL_SEL & r) == R1_CHANNEL_SEL)
			REG_SET_BIT(&(reg->CTRL), 1);
		if ((R2_CHANNEL_SEL & r) == R2_CHANNEL_SEL)
			REG_SET_BIT(&(reg->CTRL), 2);
	} else {
		if ((R0_CHANNEL_SEL & r) == R0_CHANNEL_SEL)
			REG_CLR_BIT(&(reg->CTRL), 0);
		if ((R1_CHANNEL_SEL & r) == R1_CHANNEL_SEL)
			REG_CLR_BIT(&(reg->CTRL), 1);
		if ((R2_CHANNEL_SEL & r) == R2_CHANNEL_SEL)
			REG_CLR_BIT(&(reg->CTRL), 2);
	}
}

static inline void aout_dev_set_work(struct aout_reg *reg, enum dev_channel_sel r, bool work)
{
	if (work) {
		if ((R0_CHANNEL_SEL & r) == R0_CHANNEL_SEL)
			REG_SET_BIT(&(reg->CTRL), 4);
		if ((R1_CHANNEL_SEL & r) == R1_CHANNEL_SEL)
			REG_SET_BIT(&(reg->CTRL), 5);
		if ((R2_CHANNEL_SEL & r) == R2_CHANNEL_SEL)
			REG_SET_BIT(&(reg->CTRL), 6);
	} else {
		if ((R0_CHANNEL_SEL & r) == R0_CHANNEL_SEL)
			REG_CLR_BIT(&(reg->CTRL), 4);
		if ((R1_CHANNEL_SEL & r) == R1_CHANNEL_SEL)
			REG_CLR_BIT(&(reg->CTRL), 5);
		if ((R2_CHANNEL_SEL & r) == R2_CHANNEL_SEL)
			REG_CLR_BIT(&(reg->CTRL), 6);
	}
}

static inline void aout_dev_set_l_mute(struct aout_reg *reg, enum dev_channel_sel r, bool mute)
{
	if (mute) {
		if ((R0_CHANNEL_SEL & r) == R0_CHANNEL_SEL)
			REG_SET_BIT(&(reg->CHANNEL_SEL), 3);
		if ((R1_CHANNEL_SEL & r) == R1_CHANNEL_SEL)
			REG_SET_BIT(&(reg->CHANNEL_SEL), 11);
		if ((R2_CHANNEL_SEL & r) == R2_CHANNEL_SEL)
			REG_SET_BIT(&(reg->CHANNEL_SEL), 19);
		if ((R1_CHANNEL_SEL & r) == R1_CHANNEL_SEL)
			REG_SET_BIT(&(reg->CHANNEL_SEL), 27);
	} else {
		if ((R0_CHANNEL_SEL & r) == R0_CHANNEL_SEL)
			REG_CLR_BIT(&(reg->CHANNEL_SEL), 3);
		if ((R1_CHANNEL_SEL & r) == R1_CHANNEL_SEL)
			REG_CLR_BIT(&(reg->CHANNEL_SEL), 11);
		if ((R2_CHANNEL_SEL & r) == R2_CHANNEL_SEL)
			REG_CLR_BIT(&(reg->CHANNEL_SEL), 19);
		if ((R1_CHANNEL_SEL & r) == R1_CHANNEL_SEL)
			REG_CLR_BIT(&(reg->CHANNEL_SEL), 27);
	}
}

static inline void aout_dev_set_r_mute(struct aout_reg *reg, enum dev_channel_sel r, bool mute)
{
	if (mute) {
		if ((R0_CHANNEL_SEL & r) == R0_CHANNEL_SEL)
			REG_SET_BIT(&(reg->CHANNEL_SEL), 7);
		if ((R1_CHANNEL_SEL & r) == R1_CHANNEL_SEL)
			REG_SET_BIT(&(reg->CHANNEL_SEL), 15);
		if ((R2_CHANNEL_SEL & r) == R2_CHANNEL_SEL)
			REG_SET_BIT(&(reg->CHANNEL_SEL), 23);
		if ((R1_CHANNEL_SEL & r) == R1_CHANNEL_SEL)
			REG_SET_BIT(&(reg->CHANNEL_SEL), 31);
	} else {
		if ((R0_CHANNEL_SEL & r) == R0_CHANNEL_SEL)
			REG_CLR_BIT(&(reg->CHANNEL_SEL), 7);
		if ((R1_CHANNEL_SEL & r) == R1_CHANNEL_SEL)
			REG_CLR_BIT(&(reg->CHANNEL_SEL), 15);
		if ((R2_CHANNEL_SEL & r) == R2_CHANNEL_SEL)
			REG_CLR_BIT(&(reg->CHANNEL_SEL), 23);
		if ((R1_CHANNEL_SEL & r) == R1_CHANNEL_SEL)
			REG_CLR_BIT(&(reg->CHANNEL_SEL), 31);
	}
}

static inline void aout_dev_set_l_channel(struct aout_reg *reg, enum dev_channel_sel r, unsigned int value)
{
	if ((R0_CHANNEL_SEL & r) == R0_CHANNEL_SEL)
		REG_SET_FIELD(&(reg->CHANNEL_SEL), value, 0x7, 0);
	if ((R1_CHANNEL_SEL & r) == R1_CHANNEL_SEL)
		REG_SET_FIELD(&(reg->CHANNEL_SEL), value, 0x7, 8);
	if ((R2_CHANNEL_SEL & r) == R2_CHANNEL_SEL)
		REG_SET_FIELD(&(reg->CHANNEL_SEL), value, 0x7, 16);
	if ((W0_CHANNEL_SEL & r) == W0_CHANNEL_SEL)
		REG_SET_FIELD(&(reg->CHANNEL_SEL), value, 0x7, 24);
}

static inline void aout_dev_set_r_channel(struct aout_reg *reg, enum dev_channel_sel r, unsigned int value)
{
	if ((R0_CHANNEL_SEL & r) == R0_CHANNEL_SEL)
		REG_SET_FIELD(&(reg->CHANNEL_SEL), value, 0x7, 4);
	if ((R1_CHANNEL_SEL & r) == R1_CHANNEL_SEL)
		REG_SET_FIELD(&(reg->CHANNEL_SEL), value, 0x7, 12);
	if ((R2_CHANNEL_SEL & r) == R2_CHANNEL_SEL)
		REG_SET_FIELD(&(reg->CHANNEL_SEL), value, 0x7, 20);
	if ((W0_CHANNEL_SEL & r) == W0_CHANNEL_SEL)
		REG_SET_FIELD(&(reg->CHANNEL_SEL), value, 0x7, 28);
}

static inline void aout_dev_set_mono(struct aout_reg *reg, enum dev_channel_sel r, bool mono)
{
	if (mono) {
		if ((W0_CHANNEL_SEL & r) == W0_CHANNEL_SEL)
			REG_SET_BIT(&(reg->CTRL), 19);
	} else {
		if ((W0_CHANNEL_SEL & r) == W0_CHANNEL_SEL)
			REG_CLR_BIT(&(reg->CTRL), 19);
	}
}

static inline void aout_dev_set_irq_enable(struct aout_reg *reg, enum dev_channel_sel r, bool enable)
{
	if (enable) {
		if ((R0_CHANNEL_SEL & r) == R0_CHANNEL_SEL)
			REG_SET_BIT(&(reg->INTEN), 0);
		if ((R1_CHANNEL_SEL & r) == R1_CHANNEL_SEL)
			REG_SET_BIT(&(reg->INTEN), 1);
		if ((R2_CHANNEL_SEL & r) == R2_CHANNEL_SEL)
			REG_SET_BIT(&(reg->INTEN), 2);
		if ((W0_CHANNEL_SEL & r) == W0_CHANNEL_SEL)
			REG_SET_BIT(&(reg->INTEN), 8);
	} else {
		if ((R0_CHANNEL_SEL & r) == R0_CHANNEL_SEL)
			REG_CLR_BIT(&(reg->INTEN), 0);
		if ((R1_CHANNEL_SEL & r) == R1_CHANNEL_SEL)
			REG_CLR_BIT(&(reg->INTEN), 1);
		if ((R2_CHANNEL_SEL & r) == R2_CHANNEL_SEL)
			REG_CLR_BIT(&(reg->INTEN), 2);
		if ((W0_CHANNEL_SEL & r) == W0_CHANNEL_SEL)
			REG_CLR_BIT(&(reg->INTEN), 8);
	}
}

static inline void aout_dev_set_frame_over_irq_enable(struct aout_reg *reg, enum dev_channel_sel r, bool enable)
{
	if (enable) {
		if ((R0_CHANNEL_SEL & r) == R0_CHANNEL_SEL) {
			REG_SET_BIT(&(reg->INTEN), 9);
			REG_SET_BIT(&(reg->CTRL), 13);
		}
		if ((R1_CHANNEL_SEL & r) == R1_CHANNEL_SEL) {
			REG_SET_BIT(&(reg->INTEN), 10);
			REG_SET_BIT(&(reg->CTRL), 14);
		}
		if ((R2_CHANNEL_SEL & r) == R2_CHANNEL_SEL) {
			REG_SET_BIT(&(reg->INTEN), 11);
			REG_SET_BIT(&(reg->CTRL), 15);
		}
	} else {
		if ((R0_CHANNEL_SEL & r) == R0_CHANNEL_SEL) {
			REG_CLR_BIT(&(reg->INTEN), 9);
			REG_CLR_BIT(&(reg->CTRL), 13);
		}
		if ((R1_CHANNEL_SEL & r) == R1_CHANNEL_SEL) {
			REG_CLR_BIT(&(reg->INTEN), 10);
			REG_CLR_BIT(&(reg->CTRL), 14);
		}
		if ((R2_CHANNEL_SEL & r) == R2_CHANNEL_SEL) {
			REG_CLR_BIT(&(reg->INTEN), 11);
			REG_CLR_BIT(&(reg->CTRL), 15);
		}
	}

}

static inline void aout_dev_clr_frame_over_irq_status(struct aout_reg *reg, enum dev_channel_sel r)
{
	if ((R0_CHANNEL_SEL & r) == R0_CHANNEL_SEL)
		REG_SET_VAL(&(reg->INT), (0x1<<9));
	if ((R1_CHANNEL_SEL & r) == R1_CHANNEL_SEL)
		REG_SET_VAL(&(reg->INT), (0x1<<10));
	if ((R2_CHANNEL_SEL & r) == R2_CHANNEL_SEL)
		REG_SET_VAL(&(reg->INT), (0x1<<11));
}

static inline unsigned int aout_dev_get_irq_status(struct aout_reg *reg)
{
	return REG_GET_VAL(&(reg->INT));
}

static inline void aout_dev_clr_irq_status(struct aout_reg *reg, enum dev_channel_sel r)
{
	if ((R0_CHANNEL_SEL & r) == R0_CHANNEL_SEL)
		REG_SET_VAL(&(reg->INT), (0x1<<0));
	if ((R1_CHANNEL_SEL & r) == R1_CHANNEL_SEL)
		REG_SET_VAL(&(reg->INT), (0x1<<1));
	if ((R2_CHANNEL_SEL & r) == R2_CHANNEL_SEL)
		REG_SET_VAL(&(reg->INT), (0x1<<2));
	if ((W0_CHANNEL_SEL & r) == W0_CHANNEL_SEL)
		REG_SET_VAL(&(reg->INT), (0x1<<8));
}

static inline void aout_dev_set_buffer_s_addr(struct aout_reg *reg, enum dev_channel_sel r, unsigned int value)
{
	if ((R0_CHANNEL_SEL & r) == R0_CHANNEL_SEL)
		REG_SET_VAL(&(reg->R1_BUFFER_START_ADDR), value);
	if ((R1_CHANNEL_SEL & r) == R1_CHANNEL_SEL)
		REG_SET_VAL(&(reg->R2_BUFFER_START_ADDR), value);
	if ((R2_CHANNEL_SEL & r) == R2_CHANNEL_SEL)
		REG_SET_VAL(&(reg->R3_BUFFER_START_ADDR), value);
	if ((W0_CHANNEL_SEL & r) == W0_CHANNEL_SEL)
		REG_SET_VAL(&(reg->PCM_W_BUFFER_SADDR), value);
}

static inline void aout_dev_set_buffer_size(struct aout_reg *reg, enum dev_channel_sel r, unsigned int value)
{
	if ((R0_CHANNEL_SEL & r) == R0_CHANNEL_SEL)
		REG_SET_VAL(&(reg->R1_BUFFER_SIZE), value);
	if ((R1_CHANNEL_SEL & r) == R1_CHANNEL_SEL)
		REG_SET_VAL(&(reg->R2_BUFFER_SIZE), value);
	if ((R2_CHANNEL_SEL & r) == R2_CHANNEL_SEL)
		REG_SET_VAL(&(reg->R3_BUFFER_SIZE), value);
	if ((W0_CHANNEL_SEL & r) == W0_CHANNEL_SEL)
		REG_SET_VAL(&(reg->PCM_W_BUFFER_SIZE), value);
}

static inline void aout_dev_set_frame_s_addr(struct aout_reg *reg, enum dev_channel_sel r, unsigned int value)
{
	if ((R0_CHANNEL_SEL & r) == R0_CHANNEL_SEL)
		REG_SET_VAL(&(reg->R1_newFRAME_START_ADDR), value);
	if ((R1_CHANNEL_SEL & r) == R1_CHANNEL_SEL)
		REG_SET_VAL(&(reg->R2_newFRAME_START_ADDR), value);
	if ((R2_CHANNEL_SEL & r) == R2_CHANNEL_SEL)
		REG_SET_VAL(&(reg->R3_newFRAME_START_ADDR), value);
}

static inline void aout_dev_set_frame_e_addr(struct aout_reg *reg, enum dev_channel_sel r, unsigned int value)
{
	if ((R0_CHANNEL_SEL & r) == R0_CHANNEL_SEL)
		REG_SET_VAL(&(reg->R1_newFRAME_END_ADDR), value);
	if ((R1_CHANNEL_SEL & r) == R1_CHANNEL_SEL)
		REG_SET_VAL(&(reg->R2_newFRAME_END_ADDR), value);
	if ((R2_CHANNEL_SEL & r) == R2_CHANNEL_SEL)
		REG_SET_VAL(&(reg->R3_newFRAME_END_ADDR), value);
}


static inline void aout_dev_set_frame_finish(struct aout_reg *reg, enum dev_channel_sel r, unsigned int value)
{
	if ((R0_CHANNEL_SEL & r) == R0_CHANNEL_SEL)
		REG_SET_FIELD(&(reg->R1_SET_NEWFRAME_CTRL), value, 0x1, 1);
	if ((R1_CHANNEL_SEL & r) == R1_CHANNEL_SEL)
		REG_SET_FIELD(&(reg->R2_SET_NEWFRAME_CTRL), value, 0x1, 1);
	if ((R2_CHANNEL_SEL & r) == R2_CHANNEL_SEL)
		REG_SET_FIELD(&(reg->R2_SET_NEWFRAME_CTRL), value, 0x1, 1);
}

static inline unsigned int aout_dev_get_frame_e_addr(struct aout_reg *reg, enum dev_channel_sel r)
{
	unsigned int value = 0;

	if ((R0_CHANNEL_SEL & r) == R0_CHANNEL_SEL)
		value = REG_GET_VAL(&(reg->R1_newFRAME_END_ADDR));
	if ((R1_CHANNEL_SEL & r) == R1_CHANNEL_SEL)
		value = REG_GET_VAL(&(reg->R2_newFRAME_END_ADDR));
	if ((R2_CHANNEL_SEL & r) == R2_CHANNEL_SEL)
		value = REG_GET_VAL(&(reg->R3_newFRAME_END_ADDR));

	return value;
}

static inline unsigned int aout_dev_get_frame_r_addr(struct aout_reg *reg, enum dev_channel_sel r)
{
	unsigned int value = 0;

	if ((R0_CHANNEL_SEL & r) == R0_CHANNEL_SEL)
		value = REG_GET_VAL(&(reg->R1_BUFFER_SDC_ADDR));
	if ((R1_CHANNEL_SEL & r) == R1_CHANNEL_SEL)
		value = REG_GET_VAL(&(reg->R2_BUFFER_SDC_ADDR));
	if ((R2_CHANNEL_SEL & r) == R2_CHANNEL_SEL)
		value = REG_GET_VAL(&(reg->R3_BUFFER_SDC_ADDR));

	return value;
}

static inline void aout_dev_set_silent(struct aout_reg *reg, bool silent)
{
	if (silent)
		REG_SET_BIT(&(reg->CTRL), 12);
	else
		REG_CLR_BIT(&(reg->CTRL), 12);
}

static inline void aout_dev_set_samplerate(struct aout_reg *reg, enum dev_channel_sel r, unsigned int value)
{
	if ((R0_CHANNEL_SEL & r) == R0_CHANNEL_SEL)
		REG_SET_FIELD(&(reg->R1_INDATA), value, 0x7, 0);
	if ((R1_CHANNEL_SEL & r) == R1_CHANNEL_SEL)
		REG_SET_FIELD(&(reg->R2_INDATA), value, 0x7, 0);
	if ((R2_CHANNEL_SEL & r) == R2_CHANNEL_SEL)
		REG_SET_FIELD(&(reg->R3_INDATA), value, 0x7, 0);
	if ((W0_CHANNEL_SEL & r) == W0_CHANNEL_SEL)
		REG_SET_FIELD(&(reg->CTRL), value, 0x3, 20);
}

static inline void aout_dev_set_points(struct aout_reg *reg, enum dev_channel_sel r, unsigned int value)
{
	if ((R0_CHANNEL_SEL & r) == R0_CHANNEL_SEL)
		REG_SET_VAL(&(reg->R1_newFrame_pcmLEN), value);
	if ((R1_CHANNEL_SEL & r) == R1_CHANNEL_SEL)
		REG_SET_VAL(&(reg->R2_newFrame_pcmLEN), value);
	if ((R2_CHANNEL_SEL & r) == R2_CHANNEL_SEL)
		REG_SET_VAL(&(reg->R3_newFrame_pcmLEN), value);
	if ((W0_CHANNEL_SEL & r) == W0_CHANNEL_SEL)
		REG_SET_VAL(&(reg->PCM_W_NUM), value);
}

static inline void aout_dev_set_endian(struct aout_reg *reg, enum dev_channel_sel r, unsigned int value)
{
	if ((R0_CHANNEL_SEL & r) == R0_CHANNEL_SEL)
		REG_SET_FIELD(&(reg->R1_INDATA), value, 0x3, 4);
	if ((R1_CHANNEL_SEL & r) == R1_CHANNEL_SEL)
		REG_SET_FIELD(&(reg->R2_INDATA), value, 0x3, 4);
	if ((R2_CHANNEL_SEL & r) == R2_CHANNEL_SEL)
		REG_SET_FIELD(&(reg->R3_INDATA), value, 0x3, 4);
	if ((W0_CHANNEL_SEL & r) == W0_CHANNEL_SEL)
		REG_SET_FIELD(&(reg->CTRL), value, 0x1, 17);
}

static inline void aout_dev_set_ch_num(struct aout_reg *reg, enum dev_channel_sel r, unsigned int value)
{
	if ((R0_CHANNEL_SEL & r) == R0_CHANNEL_SEL)
		REG_SET_FIELD(&(reg->R1_INDATA), value, 0x1, 3);
	if ((R1_CHANNEL_SEL & r) == R1_CHANNEL_SEL)
		REG_SET_FIELD(&(reg->R2_INDATA), value, 0x1, 3);
	if ((R2_CHANNEL_SEL & r) == R2_CHANNEL_SEL)
		REG_SET_FIELD(&(reg->R3_INDATA), value, 0x1, 3);
	if ((W0_CHANNEL_SEL & r) == W0_CHANNEL_SEL)
		REG_SET_FIELD(&(reg->CTRL), value, 0x1, 18);
}

static inline void aout_dev_set_bit_size(struct aout_reg *reg, enum dev_channel_sel r, unsigned int value)
{
	if ((R0_CHANNEL_SEL & r) == R0_CHANNEL_SEL)
		REG_SET_FIELD(&(reg->R1_INDATA), value, 0x7, 8);
	if ((R1_CHANNEL_SEL & r) == R1_CHANNEL_SEL)
		REG_SET_FIELD(&(reg->R2_INDATA), value, 0x7, 8);
	if ((R2_CHANNEL_SEL & r) == R2_CHANNEL_SEL)
		REG_SET_FIELD(&(reg->R3_INDATA), value, 0x7, 8);
}

static inline void aout_dev_set_interlace(struct aout_reg *reg, enum dev_channel_sel r, unsigned int value)
{
	if ((R0_CHANNEL_SEL & r) == R0_CHANNEL_SEL)
		REG_SET_FIELD(&(reg->R1_INDATA), value, 0x1, 7);
	if ((R1_CHANNEL_SEL & r) == R1_CHANNEL_SEL)
		REG_SET_FIELD(&(reg->R2_INDATA), value, 0x1, 7);
	if ((R2_CHANNEL_SEL & r) == R2_CHANNEL_SEL)
		REG_SET_FIELD(&(reg->R3_INDATA), value, 0x1, 7);
}

static inline void aout_dev_set_source(struct aout_reg *reg, enum dev_channel_sel r, unsigned int value)
{
	if ((R0_CHANNEL_SEL & r) == R0_CHANNEL_SEL)
		REG_SET_FIELD(&(reg->R1_INDATA), value, 0x3, 12);
	if ((R1_CHANNEL_SEL & r) == R1_CHANNEL_SEL)
		REG_SET_FIELD(&(reg->R2_INDATA), value, 0x3, 12);
	if ((R2_CHANNEL_SEL & r) == R2_CHANNEL_SEL)
		REG_SET_FIELD(&(reg->R3_INDATA), value, 0x3, 12);
	if ((W0_CHANNEL_SEL & r) == W0_CHANNEL_SEL)
		REG_SET_FIELD(&(reg->CTRL), value, 0x1, 22);
}

static inline void aout_dev_set_volume(struct aout_reg *reg, enum dev_channel_sel r, unsigned int volume)
{
	if ((R0_CHANNEL_SEL & r) == R0_CHANNEL_SEL)
		REG_SET_FIELD(&(reg->R1_VOL_CTRL), volume, 0x3FF, 0);
	if ((R1_CHANNEL_SEL & r) == R1_CHANNEL_SEL)
		REG_SET_FIELD(&(reg->R2_VOL_CTRL), volume, 0x3FF, 0);
	if ((R2_CHANNEL_SEL & r) == R2_CHANNEL_SEL)
		REG_SET_FIELD(&(reg->R3_VOL_CTRL), volume, 0x3FF, 0);
}

static inline void aout_src_set_enable(struct aout_reg *reg, bool enable)
{
	aout_dev_set_mix (reg, R0_CHANNEL_SEL, enable);
	aout_dev_set_mix (reg, R1_CHANNEL_SEL, enable);
	aout_dev_set_mix (reg, R2_CHANNEL_SEL, enable);
	aout_dev_set_work(reg, R0_CHANNEL_SEL, enable);
	aout_dev_set_work(reg, R1_CHANNEL_SEL, enable);
	aout_dev_set_work(reg, R2_CHANNEL_SEL, enable);
}

#endif
