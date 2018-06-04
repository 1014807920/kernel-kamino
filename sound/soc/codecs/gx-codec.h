#ifndef __ASOC_CODEC_H__
#define __ASOC_CODEC_H__

#define DAC_MAX_DB_VALUE  (6)
#define DAC_MIN_DB_VALUE  (-100)
#define DAC_INIT_DB_VALUE (-2)
#define DAC_MAX_DB_INDEX  (0x7f)

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

#define REG_GET_FIELD(reg, mask, offset)                        \
	(((*(volatile unsigned int *)reg)>>(offset))&(mask))

struct lodac_reg_des {
#define MAX_AOUT_REG_NUM (1)
	unsigned int num;
	const char   *name   [MAX_AOUT_REG_NUM];
	unsigned int baseAddr[MAX_AOUT_REG_NUM];
	unsigned int length  [MAX_AOUT_REG_NUM];
};

struct aout_lodac {
	unsigned int LODAC_DATA;
	unsigned int LODAC_CTRL;
};

struct lodac_info {
	struct aout_lodac *lodacReg;
	long     int      globalDacMute;
	long     int      globalDacDBValue;
};

#endif
