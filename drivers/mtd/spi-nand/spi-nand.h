#ifndef __SIMPLE_SPI_NAND_H__
#define __SIMPLE_SPI_NAND_H__
#include <linux/mtd/mtd.h>

#define CMD_READ                   0x13
#define CMD_READ_RDM               0x03
#define CMD_PROG_PAGE_CLRCACHE     0x02
#define CMD_PROG_PAGE              0x84
#define CMD_PROG_PAGE_EXC          0x10
#define CMD_ERASE_BLK              0xd8
#define CMD_WR_ENABLE              0x06
#define CMD_WR_DISABLE             0x04
#define CMD_READ_ID                0x9f
#define CMD_RESET                  0xff
#define CMD_READ_REG               0x0f
#define CMD_WRITE_REG              0x1f

/* feature/ status reg */
#define REG_BLOCK_LOCK             0xa0
#define REG_OTP                    0xb0
#define REG_STATUS                 0xc0

/* status */
#define STATUS_OIP_MASK            (0x01  )
#define STATUS_READY               (0 << 0)
#define STATUS_BUSY                (1 << 0)
#define STATUS_E_FAIL_MASK         (0x04  )
#define STATUS_E_FAIL              (1 << 2)
#define STATUS_P_FAIL_MASK         (0x08  )
#define STATUS_P_FAIL              (1 << 3)
#define STATUS_ECC_MASK            (0x30  )
#define STATUS_ECC_1BIT_CORRECTED  (1 << 4)
#define STATUS_ECC_ERROR           (2 << 4)
#define STATUS_ECC_RESERVED        (3 << 4)


/*ECC enable defines*/
#define REG_ECC_MASK               0x10
#define REG_ECC_OFF                0x00
#define REG_ECC_ON                 0x01
#define ECC_DISABLED
#define ECC_IN_NAND
#define ECC_SOFT

/*BUF enable defines*/
#define REG_BUF_MASK               0x08
#define REG_BUF_OFF                0x00
#define REG_BUF_ON                 0x01

/* block lock */
#define BL_ALL_LOCKED              0x38
#define BL_1_2_LOCKED              0x30
#define BL_1_4_LOCKED              0x28
#define BL_1_8_LOCKED              0x20
#define BL_1_16_LOCKED             0x18
#define BL_1_32_LOCKED             0x10
#define BL_1_64_LOCKED             0x08
#define BL_ALL_UNLOCKED            0x00

typedef struct{
	uint8_t  cmd_len;
	uint8_t  cmd[5];
	uint32_t xfer_len;
	uint8_t  *tx;
	uint8_t  *rx;
} snd_cmd_t;

typedef enum WP_MODE {
	WP_TOP = 0,
	WP_BOTTOM_HIGHT,
	WP_BOTTOM,
	WP_TOP_HIGHT,
	WP_UNIFORM,
} wp_mode_t;

enum {
	WP_RANGE_NONE = 0,
	WP_RANGE_DIV64,
	WP_RANGE_DIV32,
	WP_RANGE_DIV16,
	WP_RANGE_DIV08,
	WP_RANGE_DIV04,
	WP_RANGE_DIV02,
	WP_RANGE_ALL,
};

struct wp_range {
	uint8_t  wp_bp;
	uint64_t wp_start;
	uint64_t wp_end;
};

typedef struct spi_nand_wp {
	uint8_t   bp_mask, bp_shift;
	uint8_t   ctrl_mask, ctrl_shift;
	wp_mode_t mode;
	struct wp_range *range;
} spi_nand_wp_t;

typedef struct spi_flash_info {
	uint16_t    nand_id;
	const char *name;
	uint32_t    nand_size;
	uint32_t    usable_size;
	uint32_t    block_size;
	uint32_t    block_main_size;
	uint32_t    block_num_per_chip;
	uint16_t    page_size;
	uint16_t    page_main_size;
	uint16_t    page_spare_size;
	uint16_t    page_num_per_block;
	uint16_t    block_shift;
	uint16_t    page_shift;
	uint32_t    block_mask;
	uint32_t    page_mask;
	spi_nand_wp_t *wp_info;
} snd_info_t;

struct spi_nand;
typedef struct spi_flash_ops {
	int  (*read)    (struct spi_nand *nand, loff_t from,
					 size_t len, size_t *retlen, uint8_t *read_buf);
	int  (*write)   (struct spi_nand *nand, loff_t to,
					 size_t len, size_t *retlen, const uint8_t *write_buf);
	int  (*erase)   (struct spi_nand *nand, loff_t offs, uint32_t len);
	int  (*readoob) (struct spi_nand *nand, loff_t from, struct mtd_oob_ops *ops);
	int  (*writeoob)(struct spi_nand *nand, loff_t to, struct mtd_oob_ops *ops);
	int  (*lock)    (struct spi_nand *nand, loff_t ofs, uint64_t len);
	int  (*unlock)  (struct spi_nand *nand, loff_t ofs, uint64_t len);
	void (*cmdfunc) (struct spi_nand *nand, snd_cmd_t *cmd);
	void (*sync)    (struct spi_nand *nand);
	int  (*is_locked)    (struct spi_nand *nand, loff_t ofs, uint64_t len);
	int  (*block_isbad)  (struct spi_nand *nand, loff_t ofs);
	int  (*block_markbad)(struct spi_nand *nand, loff_t ofs);
} snd_ops_t;


struct spi_nand {
	struct mtd_info     mtd;
	struct mutex        lock;
	struct device      *dev;
	struct device_node *snd_node;
	snd_info_t         *snd_info;
	snd_ops_t           snd_ops;
	void               *priv;
};



extern int spi_nand_scan(struct spi_nand *nand, const char *name);

#endif
