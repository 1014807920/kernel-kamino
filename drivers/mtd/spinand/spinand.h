#ifndef __SPINAND_H__
#define __SPINAND_H__
#include <linux/mtd/mtd.h>

/* cmd */
#define CMD_READ			0x13
#define CMD_READ_RDM			0x03
#define CMD_PROG_PAGE_CLRCACHE		0x02
#define CMD_PROG_PAGE			0x84
#define CMD_PROG_PAGE_EXC		0x10
#define CMD_ERASE_BLK			0xd8
#define CMD_WR_ENABLE			0x06
#define CMD_WR_DISABLE			0x04
#define CMD_READ_ID			0x9f
#define CMD_RESET			0xff
#define CMD_READ_REG			0x0f
#define CMD_WRITE_REG			0x1f

/* feature/ status reg */
#define REG_BLOCK_LOCK			0xa0
#define REG_OTP				0xb0
#define REG_STATUS			0xc0

/* status */
#define STATUS_OIP_MASK			(0x01  )
#define STATUS_READY			(0 << 0)
#define STATUS_BUSY			(1 << 0)
#define STATUS_E_FAIL_MASK		(0x04  )
#define STATUS_E_FAIL			(1 << 2)
#define STATUS_P_FAIL_MASK		(0x08  )
#define STATUS_P_FAIL			(1 << 3)
#define STATUS_COMMON_ECC_MASK		(0x30  )
#define STATUS_FORESEE_ECC_MASK		(0x70  )
#define STATUS_ECC_1BIT_CORRECTED	(1 << 4)
#define STATUS_COMMON_ECC_ERROR		(2 << 4)
#define STATUS_FORESEE_ECC_ERROR	(7 << 4)
#define STATUS_ECC_RESERVED		(3 << 4)

/*ECC enable defines*/
#define REG_ECC_MASK			0x10
#define REG_ECC_OFF			0x00
#define REG_ECC_ON			0x01
#define ECC_DISABLED
#define ECC_IN_NAND
#define ECC_SOFT

/*BUF enable defines*/
#define REG_BUF_MASK			0x08
#define REG_BUF_OFF			0x00
#define REG_BUF_ON			0x01

/* block lock */
#define BL_ALL_LOCKED			0x38
#define BL_1_2_LOCKED			0x30
#define BL_1_4_LOCKED			0x28
#define BL_1_8_LOCKED			0x20
#define BL_1_16_LOCKED			0x18
#define BL_1_32_LOCKED			0x10
#define BL_1_64_LOCKED			0x08
#define BL_ALL_UNLOCKED			0x00

struct spinand_info {
	u16 id;
	const char *name;
	u64 nand_size;
	u64 usable_size;
	u32 block_size;
	u32 block_main_size;
	u32 block_num_per_chip;
	u16 page_size;
	u16 page_main_size;
	u16 page_spare_size;
	u16 page_num_per_block;
	u16 block_shift;
	u16 page_shift;
	u32 block_mask;
	u32 page_mask;
	struct nand_ecclayout *ecclayout;
};

struct spinand_cmd {
	u8  cmd_len;
	u8  cmd[5];
	u32 xfer_len;
	u8 *tx;
	u8 *rx;
};

typedef enum {
	FL_READY,
	FL_READING,
	FL_WRITING,
	FL_ERASING,
	FL_SYNCING,
	FL_LOCKING,
	FL_RESETING,
	FL_OTPING,
	FL_PM_SUSPENDED,
} spinand_state_t;

struct spinand_chip {
	spinlock_t chip_lock;
	wait_queue_head_t wq;
	struct mtd_info mtd;
	struct spi_device *spi;
	struct spinand_info *info;
	spinand_state_t state;
	void *priv;
	void *buf;
	void *oobbuf;
	u8 *bbt;
	int (*reset)(struct spinand_chip *chip);
	int (*read_id)(struct spinand_chip *chip, void* id);
	int (*read_page)(struct spinand_chip *chip, u32 page_id, u16 offset, u16 len, u8* rbuf);
	int (*program_page)(struct spinand_chip *chip, u32 page_id, u16 offset, u16 len, u8* wbuf);
	int (*erase_block)(struct spinand_chip *chip, u32 block_id);
	int (*cmd_func)(struct spinand_chip *chip, struct spinand_cmd *cmd);
	void (*hwecc)(struct spinand_chip *chip, bool en);
};

struct spinand_index {
	u16 id;
	const char *name;
	int ecc_index;
	int info_index;
};

#define mtd_to_chip(_mtd)	((_mtd)->priv)
#define chip_to_mtd(_chip)	(&(_chip)->mtd)

int spinand_mtd_register(struct spinand_chip *chip);
int spinand_mtd_unregister(struct spinand_chip *chip);

#endif	// #ifndef __SPINAND_H__
