#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/jiffies.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/nand_ecc.h>
#include <linux/completion.h>
#include <linux/spi/spi.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include "spinand.h"

#define ID_TABLE_FILL(_id, _name, _info, _ecc, _get_ecc_status, _ecc_strength) {\
	.id		= _id,	\
	.name		= _name,\
	.info_index	= _info,\
	.ecc_index	= _ecc,	\
	.ecc_strength = _ecc_strength,	\
	.get_ecc_status = _get_ecc_status,\
}

enum {
	NAND_1G_PAGE2K_OOB64 = 0,
	NAND_2G_PAGE2K_OOB64,
	NAND_1G_PAGE2K_OOB128,
	NAND_2G_PAGE2K_OOB128,
	NAND_4G_PAGE4K_OOB256,
};

enum {
	ECC_LAYOUT_DEFAULT_OOB64 = 0,
	ECC_LAYOUT_GD_OOB64,
	ECC_LAYOUT_GD_OOB128,
	ECC_LAYOUT_GD_OOB256,
	ECC_LAYOUT_W25ND_OOB64,
	ECC_LAYOUT_TC58CV_OOB64,
	ECC_LAYOUT_MT29F_OOB64,
	ECC_LAYOUT_F50LXX1A_OOB64,
	ECC_LAYOUT_F50LXX41LB_OOB64,
	ECC_LAYOUT_MX35LF_OOB64,
	ECC_LAYOUT_PN26G_OOB64,
	ECC_LAYOUT_XT26G_OOB64,
	ECC_LAYOUT_ZD35X_OOB64,
	ECC_LAYOUT_EM73_OOB64,
	ECC_LAYOUT_FORESEE_D1_OOB64,
	ECC_LAYOUT_FORESEE_S1_OOB64,
	ECC_LAYOUT_DS35Q_OOB64,
	ECC_LAYOUT_HEYANGTEK_OOB64,
	ECC_LAYOUT_HEYANGTEK_OOB128,
};

static struct nand_ecclayout s_ecclayout[] = {
	[ECC_LAYOUT_DEFAULT_OOB64] = {
		.eccbytes = 24,
		.eccpos = {
			1,  2,  3,  4,  5,  6,
			17, 18, 19, 20, 21, 22,
			33, 34, 35, 36, 37, 38,
			49, 50, 51, 52, 53, 54,
		},
		.oobavail = 32,
		.oobfree = {
			{.offset = 8,	.length = 8},
			{.offset = 24,	.length = 8},
			{.offset = 40,	.length = 8},
			{.offset = 56,	.length = 8},
		}
	},
	[ECC_LAYOUT_GD_OOB64] = {
		.eccbytes = 16,
		.eccpos = {
			12, 13, 14, 15,
			28, 29, 30, 31,
			44, 45, 46, 47,
			60, 61, 62, 63,
		},
		.oobavail = 32,
		.oobfree = {
			{.offset = 4,	.length = 8},
			{.offset = 20,	.length = 8},
			{.offset = 36,	.length = 8},
			{.offset = 52,	.length = 8},
		}
	},
	[ECC_LAYOUT_GD_OOB128] = {
		.eccbytes = 64,
		.eccpos = {
			64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
			80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95,
			96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111,
			112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127,
		},
		.oobavail = 48,
		.oobfree = {
			{.offset = 4,	.length = 12},
			{.offset = 20,	.length = 12},
			{.offset = 36,	.length = 12},
			{.offset = 52,	.length = 12},
		}
	},

	[ECC_LAYOUT_GD_OOB256] = {
		.eccbytes = 128,
		.eccpos = {
			128, 129, 130, 131, 132, 133, 134, 135,
			136, 137, 138, 139, 140, 141, 142, 143,
			144, 145, 146, 147, 148, 149, 150, 151,
			152, 153, 154, 155, 156, 157, 158, 159,
			160, 161, 162, 163, 164, 165, 166, 167,
			168, 169, 170, 171, 172, 173, 174, 175,
			176, 177, 178, 179, 180, 181, 182, 183,
			184, 185, 186, 187, 188, 189, 190, 191,
			192, 193, 194, 195, 196, 197, 198, 199,
			200, 201, 202, 203, 204, 205, 206, 207,
			208, 209, 210, 211, 212, 213, 214, 215,
			216, 217, 218, 219, 220, 221, 222, 223,
			224, 225, 226, 227, 228, 229, 230, 231,
			232, 233, 234, 235, 236, 237, 238, 239,
			240, 241, 242, 243, 244, 245, 246, 247,
			248, 249, 250, 251, 252, 253, 254, 255,
		},
		.oobavail = 96,
		.oobfree = {
			{.offset = 4,	.length = 12},
			{.offset = 20,	.length = 12},
			{.offset = 36,	.length = 12},
			{.offset = 52,	.length = 12},
			{.offset = 68,	.length = 12},
			{.offset = 84,	.length = 12},
			{.offset = 100,	.length = 12},
			{.offset = 116,	.length = 12},
		}
	},
	[ECC_LAYOUT_W25ND_OOB64] = {
		.eccbytes = 24,
		.eccpos = {
			8,  9,  10, 11, 12, 13,
			24, 25, 26, 27, 28, 29,
			40, 41, 42, 43, 44, 45,
			56, 57, 58, 59, 60, 61
		},
		.oobavail = 24,
		.oobfree = {
			{.offset = 2,	.length = 6},
			{.offset = 18,	.length = 6},
			{.offset = 34,	.length = 6},
			{.offset = 50,	.length = 6},
		}
	},
	[ECC_LAYOUT_TC58CV_OOB64] = {
		.eccbytes = 0,
		.eccpos = { 0 },
		.oobavail = 60,
		.oobfree = { {.offset = 4, .length = 60}, }
	},
	[ECC_LAYOUT_MT29F_OOB64] = {
		.eccbytes = 32,
		.eccpos = {
			8 , 9 , 10, 11, 12, 13, 14, 15,
			24, 25, 26, 27, 28, 29, 30, 31,
			40, 41, 42, 43, 44, 45, 46, 47,
			56, 57, 58, 59, 60, 61, 62, 63,
		},
		.oobavail = 16,
		.oobfree = {
			{.offset = 4,	.length = 4},
			{.offset = 20,	.length = 4},
			{.offset = 36,	.length = 4},
			{.offset = 52,	.length = 4},
		}
	},
	[ECC_LAYOUT_F50LXX1A_OOB64] = {
		.eccbytes = 28,
		.eccpos = {
			1 , 2 , 3 , 4 , 5 , 6 , 7 ,
			17, 18, 19, 20, 21, 22, 23,
			33, 34, 35, 36, 37, 38, 39,
			49, 50, 51, 52, 53, 54, 55,
		},
		.oobavail = 32,
		.oobfree = {
			{.offset = 8,	.length = 8},
			{.offset = 24,	.length = 8},
			{.offset = 40,	.length = 8},
			{.offset = 56,	.length = 8},
		}
	},
	[ECC_LAYOUT_F50LXX41LB_OOB64] = {
		.eccbytes = 32,
		.eccpos = {
			8 , 9 , 10, 11, 12, 13, 14, 15,
			24, 25, 26, 27, 28, 29, 30, 31,
			40, 41, 42, 43, 44, 45, 46, 47,
			56, 57, 58, 59, 60, 61, 62, 63,
		},
		.oobavail = 16,
		.oobfree = {
			{.offset = 4,	.length = 4},
			{.offset = 20,	.length = 4},
			{.offset = 36,	.length = 4},
			{.offset = 52,	.length = 4},
		}
	},
	[ECC_LAYOUT_MX35LF_OOB64] = {
		.eccbytes = 0,
		.eccpos = { 0 },
		.oobavail = 48,
		.oobfree = {
			{.offset = 4,	.length = 12},
			{.offset = 20,	.length = 12},
			{.offset = 36,	.length = 12},
			{.offset = 52,	.length = 12},
		}
	},
	[ECC_LAYOUT_PN26G_OOB64] = {
		.eccbytes = 52,
		.eccpos = {
			6 , 7 , 8 , 9 , 10, 11, 12, 13, 14, 15, 16, 17, 18,
			21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33,
			36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48,
			51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63
		},
		.oobavail = 8,
		.oobfree = {
			{.offset = 4,	.length = 2},
			{.offset = 19,	.length = 2},
			{.offset = 34,	.length = 2},
			{.offset = 49,	.length = 2},
		}
	},
	[ECC_LAYOUT_XT26G_OOB64] = {
		.eccbytes = 16,
		.eccpos = {
			48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59,
			60, 61, 62, 63
		},
		//oob区域前八个字节不受ecc保护
		.oobavail = 40,
		.oobfree = {
			{.offset = 8,	.length = 40},
		}
	},
	[ECC_LAYOUT_ZD35X_OOB64] = {
		.eccbytes = 0,
		.eccpos = { 0 },
		.oobavail = 16,
		.oobfree = {
			{.offset = 4,	.length = 4},
			{.offset = 20,	.length = 4},
			{.offset = 36,	.length = 4},
			{.offset = 52,	.length = 4},
		}
	},
	[ECC_LAYOUT_EM73_OOB64] = {
		.eccbytes = 32,
		.eccpos = {
			8 , 9 , 10, 11, 12, 13, 14, 15,
			24, 25, 26, 27, 28, 29, 30, 31,
			40, 41, 42, 43, 44, 45, 46, 47,
			56, 57, 58, 59, 60, 61, 62, 63,
		},
		.oobavail = 16,
		.oobfree = {
			{.offset = 4,   .length = 4},
			{.offset = 20,  .length = 4},
			{.offset = 36,  .length = 4},
			{.offset = 52,  .length = 4},
		}
	},
	[ECC_LAYOUT_FORESEE_D1_OOB64] = {
		.eccbytes = 32,
		.eccpos = {
			8 , 9 , 10, 11, 12, 13, 14, 15,
			24, 25, 26, 27, 28, 29, 30, 31,
			40, 41, 42, 43, 44, 45, 46, 47,
			56, 57, 58, 59, 60, 61, 62, 63,
		},
		.oobavail = 30,
		.oobfree = {
			{.offset = 2,	.length = 6},
			{.offset = 16,	.length = 8},
			{.offset = 32,	.length = 8},
			{.offset = 48,	.length = 8},
		}
	},
	[ECC_LAYOUT_FORESEE_S1_OOB64] = {
		.eccbytes = 0,
		.eccpos = {},
		.oobavail = 62,
		.oobfree = {
			{.offset = 2,	.length = 14},
			{.offset = 16,	.length = 16},
			{.offset = 32,	.length = 16},
			{.offset = 48,	.length = 16},
		}
	},
	[ECC_LAYOUT_DS35Q_OOB64] = {
		.eccbytes = 32,
		.eccpos = {
			8 , 9 , 10, 11, 12, 13, 14, 15,
			24, 25, 26, 27, 28, 29, 30, 31,
			40, 41, 42, 43, 44, 45, 46, 47,
			56, 57, 58, 59, 60, 61, 62, 63,
		},
		.oobavail = 16,
		.oobfree = {
			{.offset = 4,	.length = 4},
			{.offset = 20,	.length = 4},
			{.offset = 36,	.length = 4},
			{.offset = 52,	.length = 4},
		}
	},
	[ECC_LAYOUT_HEYANGTEK_OOB64] = {
		.eccbytes = 32,
		.eccpos = {
			8 , 9 , 10, 11, 12, 13, 14, 15,
			24, 25, 26, 27, 28, 29, 30, 31,
			40, 41, 42, 43, 44, 45, 46, 47,
			56, 57, 58, 59, 60, 61, 62, 63,
		},
		.oobavail = 16,
		.oobfree = {
			{.offset = 4,	.length = 4},
			{.offset = 20,	.length = 4},
			{.offset = 36,	.length = 4},
			{.offset = 52,	.length = 4},
		}
	},
	[ECC_LAYOUT_HEYANGTEK_OOB128] = {
		.eccbytes = 96,
		.eccpos = {
			8 , 9 , 10, 11, 12, 13, 14, 15,
			16, 17, 18, 19, 20, 21, 22, 23,
			24, 25, 26, 27, 28, 29, 30, 31,
			40, 41, 42, 43, 44, 45, 46, 47,
			48, 49, 50, 51, 52, 53, 54, 55,
			56, 57, 58, 59, 60, 61, 62, 63,
			72, 73, 74, 75, 76, 77, 78, 79,
			80, 81, 82, 83, 84, 85, 86, 87,
			88, 89, 90, 91, 92, 93, 94, 95,
			104,105,106,107,108,109,110,111,
			112,113,114,115,116,117,118,119,
			120,121,122,123,124,125,126,127,
		},
		.oobavail = 16,
		.oobfree = {
			{.offset = 4,	.length = 4},
			{.offset = 36,	.length = 4},
			{.offset = 68,	.length = 4},
			{.offset = 100,	.length = 4},
		}
	}
};

/* page info */
#define PAGE_MAIN_SIZE_2K          2048
#define PAGE_MAIN_SIZE_4K          4096
#define PAGE_SPARE_SIZE_64B        64
#define PAGE_SPARE_SIZE_128B       128
#define PAGE_SPARE_SIZE_256B       256
#define PAGE_SIZE_2K_64B           (PAGE_MAIN_SIZE_2K + PAGE_SPARE_SIZE_64B)
#define PAGE_SIZE_2K_128B          (PAGE_MAIN_SIZE_2K + PAGE_SPARE_SIZE_128B)
#define PAGE_SIZE_4K_256B          (PAGE_MAIN_SIZE_4K + PAGE_SPARE_SIZE_256B)
#define PAGE_NUM_PER_BLOCK_64      64
/* block info */
#define BLOCK_MAIN_SIZE_128K       (PAGE_MAIN_SIZE_2K * PAGE_NUM_PER_BLOCK_64)
#define BLOCK_MAIN_SIZE_256K       (PAGE_MAIN_SIZE_2K * PAGE_NUM_PER_BLOCK_64)
#define BLOCK_SIZE_128K_4K         (PAGE_SIZE_2K_64B * PAGE_NUM_PER_BLOCK_64)
#define BLOCK_SIZE_128K_8K         (PAGE_SIZE_2K_128B * PAGE_NUM_PER_BLOCK_64)
#define BLOCK_SIZE_256K_16K        (PAGE_SIZE_4K_256B * PAGE_NUM_PER_BLOCK_64)
#define BLOCK_NUM_PER_CHIP_1024    1024
#define BLOCK_NUM_PER_CHIP_2048    2048
/* nand info */
#define NAND_MAIN_SIZE_128M        (BLOCK_MAIN_SIZE_128K * BLOCK_NUM_PER_CHIP_1024)
#define NAND_MAIN_SIZE_256M        (BLOCK_MAIN_SIZE_128K * BLOCK_NUM_PER_CHIP_2048)
#define NAND_MAIN_SIZE_512M        (BLOCK_MAIN_SIZE_256K * BLOCK_NUM_PER_CHIP_2048)
#define NAND_SIZE_128M_4M          (BLOCK_SIZE_128K_4K * BLOCK_NUM_PER_CHIP_1024)
#define NAND_SIZE_128M_8M          (BLOCK_SIZE_128K_8K * BLOCK_NUM_PER_CHIP_1024)
#define NAND_SIZE_256M_8M          (BLOCK_SIZE_128K_4K * BLOCK_NUM_PER_CHIP_2048)
#define NAND_SIZE_256M_16M         (BLOCK_SIZE_128K_8K * BLOCK_NUM_PER_CHIP_2048)
#define NAND_SIZE_512M_32M         (BLOCK_SIZE_256K_16K * BLOCK_NUM_PER_CHIP_2048)
/* other info */
#define BLOCK_SHIFT_128K           (17)
#define BLOCK_SHIFT_256K           (18)
#define BLOCK_MASK_128K            (BLOCK_MAIN_SIZE_128K - 1)
#define BLOCK_MASK_256K            (BLOCK_MAIN_SIZE_256K - 1)
#define PAGE_SHIFT_2K              (11)
#define PAGE_SHIFT_4K              (12)
#define PAGE_MASK_2K               (PAGE_MAIN_SIZE_2K - 1)
#define PAGE_MASK_4K               (PAGE_MAIN_SIZE_4K - 1)

static struct spinand_info s_nand_info[] = {
	[NAND_1G_PAGE2K_OOB64] = {
		.nand_size		= NAND_SIZE_128M_4M,
		.usable_size		= NAND_MAIN_SIZE_128M,
		.block_size		= BLOCK_SIZE_128K_4K,
		.block_main_size	= BLOCK_MAIN_SIZE_128K,
		.block_num_per_chip	= BLOCK_NUM_PER_CHIP_1024,
		.page_size		= PAGE_SIZE_2K_64B,
		.page_main_size		= PAGE_MAIN_SIZE_2K,
		.page_spare_size	= PAGE_SPARE_SIZE_64B,
		.page_num_per_block	= PAGE_NUM_PER_BLOCK_64,
		.block_shift		= BLOCK_SHIFT_128K,
		.block_mask		= BLOCK_MASK_128K,
		.page_shift		= PAGE_SHIFT_2K,
		.page_mask		= PAGE_MASK_2K,
	},
	[NAND_2G_PAGE2K_OOB64] = {
		.nand_size		= NAND_SIZE_256M_8M,
		.usable_size		= NAND_MAIN_SIZE_256M,
		.block_size		= BLOCK_SIZE_128K_4K,
		.block_main_size	= BLOCK_MAIN_SIZE_128K,
		.block_num_per_chip	= BLOCK_NUM_PER_CHIP_2048,
		.page_size		= PAGE_SIZE_2K_64B,
		.page_main_size		= PAGE_MAIN_SIZE_2K,
		.page_spare_size	= PAGE_SPARE_SIZE_64B,
		.page_num_per_block	= PAGE_NUM_PER_BLOCK_64,
		.block_shift		= BLOCK_SHIFT_128K,
		.block_mask		= BLOCK_MASK_128K,
		.page_shift		= PAGE_SHIFT_2K,
		.page_mask		= PAGE_MASK_2K,
	},
	[NAND_1G_PAGE2K_OOB128] = {
		.nand_size		= NAND_SIZE_128M_8M,
		.usable_size		= NAND_MAIN_SIZE_128M,
		.block_size		= BLOCK_SIZE_128K_8K,
		.block_main_size	= BLOCK_MAIN_SIZE_128K,
		.block_num_per_chip	= BLOCK_NUM_PER_CHIP_2048,
		.page_size		= PAGE_SIZE_2K_128B,
		.page_main_size		= PAGE_MAIN_SIZE_2K,
		.page_spare_size	= PAGE_SPARE_SIZE_128B,
		.page_num_per_block	= PAGE_NUM_PER_BLOCK_64,
		.block_shift		= BLOCK_SHIFT_128K,
		.block_mask		= BLOCK_MASK_128K,
		.page_shift		= PAGE_SHIFT_2K,
		.page_mask		= PAGE_MASK_2K,
	},
	[NAND_2G_PAGE2K_OOB128] = {
		.nand_size		= NAND_SIZE_256M_16M,
		.usable_size		= NAND_MAIN_SIZE_256M,
		.block_size		= BLOCK_SIZE_128K_8K,
		.block_main_size	= BLOCK_MAIN_SIZE_128K,
		.block_num_per_chip	= BLOCK_NUM_PER_CHIP_2048,
		.page_size		= PAGE_SIZE_2K_128B,
		.page_main_size		= PAGE_MAIN_SIZE_2K,
		.page_spare_size	= PAGE_SPARE_SIZE_128B,
		.page_num_per_block	= PAGE_NUM_PER_BLOCK_64,
		.block_shift		= BLOCK_SHIFT_128K,
		.block_mask		= BLOCK_MASK_128K,
		.page_shift		= PAGE_SHIFT_2K,
		.page_mask		= PAGE_MASK_2K,
	},
	[NAND_4G_PAGE4K_OOB256] = {
		.nand_size		= NAND_SIZE_512M_32M,
		.usable_size		= NAND_MAIN_SIZE_512M,
		.block_size		= BLOCK_SIZE_256K_16K,
		.block_main_size	= BLOCK_MAIN_SIZE_256K,
		.block_num_per_chip	= PAGE_MAIN_SIZE_2K,
		.page_size		= PAGE_SIZE_4K_256B,
		.page_main_size		= PAGE_MAIN_SIZE_4K,
		.page_spare_size	= PAGE_SPARE_SIZE_256B,
		.page_num_per_block	= PAGE_NUM_PER_BLOCK_64,
		.block_shift		= BLOCK_SHIFT_256K,
		.block_mask		= BLOCK_MASK_256K,
		.page_shift		= PAGE_SHIFT_4K,
		.page_mask		= PAGE_MASK_4K,
	},
};

static int generic_ecc_status(unsigned char status, unsigned int *bitflips)
{
	int ret = 0;
	unsigned int ecc_status = status & STATUS_GENERIC_ECC_MASK;

	switch(ecc_status) {
	case 0x00:
		*bitflips = 0;
		break;
	case 0x10:
		*bitflips = GENERIC_ECC_BITS_MAX - 1;
		break;
	case 0x30:
		*bitflips = GENERIC_ECC_BITS_MAX;
		break;
	case 0x20:
		*bitflips = GENERIC_ECC_BITS_MAX + 1;
		ret = ECC_NOT_CORRECT;
		break;
	default:;
	}
	return ret;
}

static int gd_ecc_status(unsigned char status, unsigned int *bitflips)
{
	int ret = 0;
	unsigned int ecc_status = status & STATUS_GENERIC_ECC_MASK;

	switch(ecc_status) {
	case 0x00:
		*bitflips = 0;
		break;
	case 0x10:
		*bitflips = GD_ECC_BITS_MAX - 1;
		break;
	case 0x30:
		*bitflips = GD_ECC_BITS_MAX;
		break;
	case 0x20:
		*bitflips = GD_ECC_BITS_MAX + 1;
		ret = ECC_NOT_CORRECT;
		break;
	default:;
	}
	return ret;
}

static int toshiba_ecc_status(unsigned char status, unsigned int *bitflips)
{
	int ret = 0;
	unsigned int ecc_status = status & STATUS_GENERIC_ECC_MASK;

	switch(ecc_status) {
	case 0x00:
		*bitflips = 0;
		break;
	case 0x10:
		*bitflips = TOSHIBA_ECC_BITS_MAX - 1;
		break;
	case 0x30:
		*bitflips = TOSHIBA_ECC_BITS_MAX;
		break;
	case 0x20:
		*bitflips = TOSHIBA_ECC_BITS_MAX + 1;
		ret = ECC_NOT_CORRECT;
		break;
	default:;
	}
	return ret;
}

static int micron_ecc_status(unsigned char status, unsigned int *bitflips)
{
	int ret = 0;
	unsigned int ecc_status = status & STATUS_MICRON_ECC_MASK;

	switch(ecc_status) {
	case 0x00:
		*bitflips = 0;
		break;
	case 0x10:
		*bitflips = 3;
		break;
	case 0x30:
		*bitflips = 6;
		break;
	case 0x50:
		*bitflips = MICRON_ECC_BITS_MAX;
		break;
	case 0x20:
		*bitflips = MICRON_ECC_BITS_MAX + 1;
		ret = ECC_NOT_CORRECT;
		break;
	default:;
	}
	return ret;
}

static int xtx_ecc_status(unsigned char status, unsigned int *bitflips)
{
	int ret = 0;
	unsigned int ecc_status = (status & STATUS_XTX_ECC_MASK) >> 2;

	switch(ecc_status) {
	case 0x00:
		*bitflips = 0;
		break;
	case 0x08:
		*bitflips = XTX_ECC_BITS_MAX + 1;
		ret = ECC_NOT_CORRECT;
		break;
	case 0x0c:
		*bitflips = 8;
		break;
	default:
		*bitflips = ecc_status;
	}
	return ret;
}

static int zetta_ecc_status(unsigned char status, unsigned int *bitflips)
{
	int ret = 0;
	unsigned int ecc_status = status & STATUS_GENERIC_ECC_MASK;

	switch(ecc_status) {
	case 0x00:
		*bitflips = 0;
		break;
	case 0x10:
		*bitflips = ZETTA_ECC_BITS_MAX;
		break;
	case 0x20:
	default:
		*bitflips = ZETTA_ECC_BITS_MAX + 1;
		ret = ECC_NOT_CORRECT;
		break;
	}
	return ret;
}

static int esmt_ecc_status(unsigned char status, unsigned int *bitflips)
{
	int ret = 0;
	unsigned int ecc_status = status & STATUS_GENERIC_ECC_MASK;

	switch(ecc_status) {
	case 0x00:
		*bitflips = 0;
		break;
	case 0x10:
		*bitflips = ESMT_ECC_BITS_MAX;
		break;
	case 0x20:
	default:
		*bitflips = ESMT_ECC_BITS_MAX + 1;
		ret = ECC_NOT_CORRECT;
		break;
	}
	return ret;
}


static int winbond_ecc_status(unsigned char status, unsigned int *bitflips)
{
	int ret = 0;
	unsigned int ecc_status = status & STATUS_GENERIC_ECC_MASK;

	switch(ecc_status) {
	case 0x00:
		*bitflips = 0;
		break;
	case 0x10:
		*bitflips = WINBOND_ECC_BITS_MAX;
		break;
	case 0x30:
	case 0x20:
	default:
		*bitflips = WINBOND_ECC_BITS_MAX + 1;
		ret = ECC_NOT_CORRECT;
		break;
	}
	return ret;
}

static int mxic_ecc_status(unsigned char status, unsigned int *bitflips)
{
	int ret = 0;
	unsigned int ecc_status = status & STATUS_GENERIC_ECC_MASK;

	switch(ecc_status) {
	case 0x00:
		*bitflips = 0;
		break;
	case 0x10:
		*bitflips = MXIC_ECC_BITS_MAX;
		break;
	case 0x30:
	case 0x20:
	default:
		*bitflips = MXIC_ECC_BITS_MAX + 1;
		ret = ECC_NOT_CORRECT;
		break;
	}
	return ret;
}

static int foresse_ecc_status(unsigned char status, unsigned int *bitflips)
{
	int ret = 0;
	unsigned int ecc_status = (status & STATUS_FORESEE_ECC_MASK) >> 4;

	if (likely(ecc_status != 0x07)) {
		*bitflips = ecc_status ;
	} else {
		*bitflips = FORESSE_ECC_BITS_MAX + 1;
		ret = ECC_NOT_CORRECT;
	}

	return ret;
}

static int dosilicon_ecc_status(unsigned char status, unsigned int *bitflips)
{
	int ret = 0;
	unsigned int ecc_status = status & STATUS_GENERIC_ECC_MASK;

	switch(ecc_status) {
	case 0x00:
		*bitflips = 0;
		break;
	case 0x10:
		*bitflips = DOSILICON_ECC_BITS_MAX;
		break;
	case 0x30:
	case 0x20:
	default:
		*bitflips = DOSILICON_ECC_BITS_MAX + 1;
		ret = ECC_NOT_CORRECT;
		break;
	}
	return ret;
}

static int heyang_ecc4_status(unsigned char status, unsigned int *bitflips)
{
	int ret = 0;
	unsigned int ecc_status = status & STATUS_GENERIC_ECC_MASK;

	switch(ecc_status) {
	case 0x00:
		*bitflips = 0;
		break;
	case 0x10:
		*bitflips = HEYANG_ECC_4BITS_MAX;
		break;
	case 0x30:
	case 0x20:
	default:
		*bitflips = HEYANG_ECC_4BITS_MAX + 1;
		ret = ECC_NOT_CORRECT;
		break;
	}
	return ret;
}

static int heyang_ecc14_status(unsigned char status, unsigned int *bitflips)
{
	int ret = 0;
	unsigned int ecc_status = status & STATUS_GENERIC_ECC_MASK;

	switch(ecc_status) {
	case 0x00:
		*bitflips = 0;
		break;
	case 0x10:
		*bitflips = HEYANG_ECC_14BITS_MAX;
		break;
	case 0x30:
	case 0x20:
	default:
		*bitflips = HEYANG_ECC_14BITS_MAX + 1;
		ret = ECC_NOT_CORRECT;
		break;
	}
	return ret;
}

static struct spinand_index s_id_table[] = {
	/* GD spi nand flash */
	ID_TABLE_FILL(0xF1C8, "GD5F1GQ4UAYIG",   NAND_1G_PAGE2K_OOB64,  ECC_LAYOUT_GD_OOB64,	     gd_ecc_status,        GD_ECC_BITS_MAX),
	ID_TABLE_FILL(0xF2C8, "GD5F2GQ4RAYIG",   NAND_2G_PAGE2K_OOB64,  ECC_LAYOUT_GD_OOB64,	     gd_ecc_status,        GD_ECC_BITS_MAX),
	ID_TABLE_FILL(0xD1C8, "GD5F1GQ4U",       NAND_1G_PAGE2K_OOB128, ECC_LAYOUT_GD_OOB128,	     gd_ecc_status,        GD_ECC_BITS_MAX),
	ID_TABLE_FILL(0xD2C8, "GD5F2GQ4U",       NAND_2G_PAGE2K_OOB128, ECC_LAYOUT_GD_OOB128,	     gd_ecc_status,        GD_ECC_BITS_MAX),
	ID_TABLE_FILL(0xC1C8, "GD5F1GQ4R",       NAND_1G_PAGE2K_OOB128, ECC_LAYOUT_GD_OOB128,	     gd_ecc_status,        GD_ECC_BITS_MAX),
	ID_TABLE_FILL(0xC2C8, "GD5F2GQ4R",       NAND_2G_PAGE2K_OOB128, ECC_LAYOUT_GD_OOB128,	     gd_ecc_status,        GD_ECC_BITS_MAX),
	ID_TABLE_FILL(0xD4C8, "GD5F4G",          NAND_4G_PAGE4K_OOB256, ECC_LAYOUT_GD_OOB256,	     gd_ecc_status,        GD_ECC_BITS_MAX),
	/* TOSHIBA spi nand flash */
	ID_TABLE_FILL(0xC298, "TC58CVG053HRA1G", NAND_1G_PAGE2K_OOB64,  ECC_LAYOUT_TC58CV_OOB64,     toshiba_ecc_status,   TOSHIBA_ECC_BITS_MAX),
	/* Micron spi nand flash */
	ID_TABLE_FILL(0x122C, "MT29F1G01ZAC",    NAND_1G_PAGE2K_OOB64,  ECC_LAYOUT_MT29F_OOB64,	     micron_ecc_status,    MICRON_ECC_BITS_MAX),
	ID_TABLE_FILL(0x112C, "MT29F1G01ZAC",    NAND_1G_PAGE2K_OOB64,  ECC_LAYOUT_MT29F_OOB64,      micron_ecc_status,    MICRON_ECC_BITS_MAX),
	ID_TABLE_FILL(0x132C, "MT29F1G01ZAC",    NAND_1G_PAGE2K_OOB64,  ECC_LAYOUT_MT29F_OOB64,      micron_ecc_status,    MICRON_ECC_BITS_MAX),
	/* 芯天下 spi nand flash */
	ID_TABLE_FILL(0xE1A1, "PN26G01AWS1UG",   NAND_1G_PAGE2K_OOB64,  ECC_LAYOUT_PN26G_OOB64,      generic_ecc_status,   GENERIC_ECC_BITS_MAX),
	ID_TABLE_FILL(0xE10B, "XT26G01AWS1UG",   NAND_1G_PAGE2K_OOB64,  ECC_LAYOUT_XT26G_OOB64,      xtx_ecc_status,       XTX_ECC_BITS_MAX),
	ID_TABLE_FILL(0xE20B, "XT26G02AWSEGA",   NAND_2G_PAGE2K_OOB64,  ECC_LAYOUT_XT26G_OOB64,      xtx_ecc_status,       XTX_ECC_BITS_MAX),
	/* Zetta Confidentia spi nand flash */
	ID_TABLE_FILL(0x71ba, "ZD35X1GA",        NAND_1G_PAGE2K_OOB64,  ECC_LAYOUT_ZD35X_OOB64,      zetta_ecc_status,     ZETTA_ECC_BITS_MAX),
	ID_TABLE_FILL(0x21ba, "ZD35X1GA",        NAND_1G_PAGE2K_OOB64,  ECC_LAYOUT_ZD35X_OOB64,      zetta_ecc_status,     ZETTA_ECC_BITS_MAX),
	/* ESMT spi nand flash */
	ID_TABLE_FILL(0x21C8, "F50L1G41A",       NAND_1G_PAGE2K_OOB64,  ECC_LAYOUT_F50LXX1A_OOB64,   esmt_ecc_status,      ESMT_ECC_BITS_MAX),
	ID_TABLE_FILL(0x01C8, "F50L1G41LB",      NAND_1G_PAGE2K_OOB64,  ECC_LAYOUT_F50LXX41LB_OOB64, esmt_ecc_status,      ESMT_ECC_BITS_MAX),
	/* winbond spi nand flash */
	ID_TABLE_FILL(0xAAEF, "W25N01GV",        NAND_1G_PAGE2K_OOB64,  ECC_LAYOUT_W25ND_OOB64,	     winbond_ecc_status,   WINBOND_ECC_BITS_MAX),
	/* Mxic spi nand flash */
	ID_TABLE_FILL(0x12C2, "MX35LF1GE4AB",    NAND_1G_PAGE2K_OOB64,  ECC_LAYOUT_MX35LF_OOB64,     mxic_ecc_status,      MXIC_ECC_BITS_MAX),
	/* foresee spi nand flash */
	ID_TABLE_FILL(0xa1cd, "FS35ND01G-D1",    NAND_1G_PAGE2K_OOB64,  ECC_LAYOUT_FORESEE_D1_OOB64, foresse_ecc_status,   FORESSE_ECC_BITS_MAX),
	ID_TABLE_FILL(0xb1cd, "FS35ND01G-S1",    NAND_1G_PAGE2K_OOB64,  ECC_LAYOUT_FORESEE_S1_OOB64, foresse_ecc_status,   FORESSE_ECC_BITS_MAX),
	/* EtronTech spi nand flash */
	ID_TABLE_FILL(0x1cd5, "EM73C044VCD",     NAND_1G_PAGE2K_OOB64,  ECC_LAYOUT_EM73_OOB64,       generic_ecc_status,   GENERIC_ECC_BITS_MAX),
	ID_TABLE_FILL(0x1fd5, "EM73D044VCG",     NAND_2G_PAGE2K_OOB64,  ECC_LAYOUT_EM73_OOB64,       generic_ecc_status,   GENERIC_ECC_BITS_MAX),
	/* dosilicon spi nand flash */
	ID_TABLE_FILL(0x71e5, "DS35Q1GA",        NAND_1G_PAGE2K_OOB64,  ECC_LAYOUT_DS35Q_OOB64,      dosilicon_ecc_status, DOSILICON_ECC_BITS_MAX),
	ID_TABLE_FILL(0x72e5, "DS35Q2GA",        NAND_2G_PAGE2K_OOB64,  ECC_LAYOUT_DS35Q_OOB64,      dosilicon_ecc_status, DOSILICON_ECC_BITS_MAX),
	/* HeYangTek spi nand flash */
	ID_TABLE_FILL(0x21c9, "HYF1GQ4UDACAE",   NAND_1G_PAGE2K_OOB64,  ECC_LAYOUT_HEYANGTEK_OOB64,  heyang_ecc4_status,   HEYANG_ECC_4BITS_MAX),
	ID_TABLE_FILL(0x5ac9, "HYF2GQ4UHCCAE",   NAND_2G_PAGE2K_OOB128, ECC_LAYOUT_HEYANGTEK_OOB128, heyang_ecc14_status,  HEYANG_ECC_14BITS_MAX),
	ID_TABLE_FILL(0x52c9, "HYF2GQ4UAACAE",   NAND_2G_PAGE2K_OOB128, ECC_LAYOUT_HEYANGTEK_OOB128, heyang_ecc14_status,  HEYANG_ECC_14BITS_MAX),

	/* item end */
	ID_TABLE_FILL(0x0001, "General flash",   NAND_1G_PAGE2K_OOB64,  ECC_LAYOUT_DEFAULT_OOB64,    generic_ecc_status,   GENERIC_ECC_BITS_MAX),
};

static struct spinand_info *spinand_id_probe(u16 id, struct spinand_chip *chip)
{
	struct spinand_index *tb = s_id_table;
	struct spinand_info *info;
	int i = ARRAY_SIZE(s_id_table);

	while(i--){
		if(id != tb[i].id)
			continue;

		info = s_nand_info + tb[i].info_index;
		info->id = tb[i].id;
		info->name = tb[i].name;
		info->ecclayout = s_ecclayout + tb[i].ecc_index;
		chip->get_ecc_status = tb[i].get_ecc_status;
		chip->ecc_strength = tb[i].ecc_strength;
		return info;
	}

	pr_warn("Warning: unknow flash id = 0x%04x.\n", id);
	return NULL;
}

static int spinand_unlock(struct mtd_info *mtd, loff_t ofs, uint64_t len)
{
	struct spinand_chip *chip = mtd_to_chip(mtd);
	struct spinand_cmd cmd = {
		.cmd_len = 3,
		.cmd[0] = CMD_WRITE_REG,
		.cmd[1] = REG_BLOCK_LOCK,
		.cmd[2] = 0,
		.xfer_len = 0,
	};

	return chip->cmd_func(chip, &cmd);
}

/**
 * spinand_get_device - [GENERIC] Get chip for selected access
 * @param mtd		MTD device structure
 * @param new_state	the state which is requested
 *
 * Get the device and lock it for exclusive access
 */
static int spinand_get_device(struct mtd_info *mtd, int new_state)
{
	struct spinand_chip *chip = mtd_to_chip(mtd);
	DECLARE_WAITQUEUE(wait, current);

	/*
	 * Grab the lock and see if the device is available
	 */
	while(true) {
		spin_lock(&chip->chip_lock);
		if (chip->state == FL_READY) {
			chip->state = new_state;
			spin_unlock(&chip->chip_lock);
			break;
		}
		if (new_state == FL_PM_SUSPENDED) {
			spin_unlock(&chip->chip_lock);
			return (chip->state == FL_PM_SUSPENDED) ? 0 : -EAGAIN;
		}
		set_current_state(TASK_UNINTERRUPTIBLE);
		add_wait_queue(&chip->wq, &wait);
		spin_unlock(&chip->chip_lock);
		schedule();
		remove_wait_queue(&chip->wq, &wait);
	}
	return 0;
}

/**
 * spinand_release_device - [GENERIC] release chip
 * @param mtd		MTD device structure
 *
 * Deselect, release chip lock and wake up anyone waiting on the device
 */
static void spinand_release_device(struct mtd_info *mtd)
{
	struct spinand_chip *chip = mtd_to_chip(mtd);

	/* Release the chip */
	spin_lock(&chip->chip_lock);
	chip->state = FL_READY;
	wake_up(&chip->wq);
	spin_unlock(&chip->chip_lock);
}

#ifdef CONFIG_MTD_SPINAND_SWECC
static void spinand_calculate_ecc(struct mtd_info *mtd)
{
	int i;
	int eccsize = 512;
	int eccbytes = 3;
	int eccsteps = 4;
	int ecctotal = 12;
	struct spinand_chip *chip = mtd->priv;
	struct spinand_info *info = chip->info;
	unsigned char *p = chip->buf;

	for (i = 0; eccsteps; eccsteps--, i += eccbytes, p += eccsize)
		/* must be confirm later. here no ecc size. */
		__nand_calculate_ecc(p, eccsize, &chip->ecc_calc[i]);

	for (i = 0; i < ecctotal; i++) {
		int j = info->page_main_size + info->ecclayout->eccpos[i];
		chip->buf[j] = chip->ecc_calc[i];
	}
}

static int spinand_correct_data(struct mtd_info *mtd)
{
	int i;
	int eccsize = 512;
	int eccbytes = 3;
	int eccsteps = 4;
	int ecctotal = 12;
	struct spinand_chip *chip = mtd->priv;
	struct spinand_info *info = chip->info;
	unsigned char *p = chip->buf;
	int errcode = 0;

	for (i = 0; eccsteps; eccsteps--, i += eccbytes, p += eccsize)
		/* must be confirm later. here no ecc size. */
		__nand_calculate_ecc(p, eccsize, &chip->ecc_calc[i]);

	for (i = 0; i < ecctotal; i++){
		int j = info->page_main_size + info->ecclayout->eccpos[i];
		chip->ecc_code[i] = chip->buf[j];
	}

	for (i = 0; eccsteps; eccsteps--, i += eccbytes, p += eccsize)
	{
		int stat;

		/* must be confirm later */
		stat = __nand_correct_data(p,
			&chip->ecc_code[i], &chip->ecc_calc[i], eccsize);

		if (stat < 0)
			errcode = -1;
		else if (stat == 1)
			errcode = 1;
	}

	return errcode;
}
#endif

static int spinand_read_ops(struct mtd_info *mtd, loff_t from, struct mtd_oob_ops *ops)
{
	struct spinand_chip *chip = mtd_to_chip(mtd);
	struct spinand_info *info = chip->info;
	int page_id, page_offset = 0, page_left = 0, oob_left = 0;
	u32 oobsize = 0, oob_offs = ops->ooboffs;

	int count = 0;
	int main_ok = 0, main_left = 0, main_offset = 0;
	int oob_ok = 0;
	int size, retval;
	unsigned int corrected;

	page_id = from >> info->page_shift;

	/* for main data */
	if(likely(ops->datbuf)){
		page_offset = from & info->page_mask;
		page_left = (page_offset + ops->len + info->page_main_size -1 ) /
							info->page_main_size;
		main_left = ops->len;
		main_offset = page_offset;
	/* for oob */
	}
	if(unlikely(ops->oobbuf)){
		oobsize = ops->mode == MTD_OPS_AUTO_OOB ?
			info->ecclayout->oobavail : info->page_spare_size;
		page_left = max_t(u32, page_left, (oob_offs + ops->ooblen + oobsize -1) / oobsize);
		oob_left = ops->ooblen;

		if(ops->ooboffs >= oobsize)
			return -EINVAL;
	}

#if 0
	pr_notice("page_offset=%04d, oob_offs=%04d, len=%04d, oob_len=%04d\n", page_offset, oob_offs, ops->len, ops->ooblen);
#endif

	while (page_left){
#ifdef CONFIG_MTD_SPINAND_SWECC
		retval = chip->read_page(chip,
			page_id + count, 0, info->page_size, chip->buf, &corrected);
#else
		if(unlikely(main_left && oob_left)){
			retval = chip->read_page(chip, page_id + count, main_offset, \
					info->page_size - main_offset, chip->buf + main_offset , &corrected);
			size = min(main_left, info->page_main_size - main_offset);
			memcpy (ops->datbuf + main_ok, chip->buf + main_offset, size);
		}
		else if(likely(main_left)){
			size = min(main_left, info->page_main_size - main_offset);
#if 1
			retval = chip->read_page(chip, page_id + count, main_offset, \
						size, chip->buf, &corrected);
			memcpy(ops->datbuf + main_ok, chip->buf, size);
#else
			retval = chip->read_page(chip, page_id + count, main_offset, \
						size, ops->datbuf + main_ok, &corrected);
#endif
		}else{
			retval = chip->read_page(chip, page_id + count,
					info->page_main_size,
					info->page_spare_size,
					chip->buf + info->page_main_size, &corrected);
		}

#endif

		if (likely(retval == 0)){
			if (unlikely(corrected != 0))
				 mtd->ecc_stats.corrected += corrected;
		} else if (unlikely(retval == ECC_NOT_CORRECT)) {
			mtd->ecc_stats.failed++;
			pr_notice("read nand page ops ecc not corrected! page id = %d\n", page_id+count);
		} else if (unlikely(retval < 0)){
			pr_err("%s: fail, page=%d!, code = %d\n",__func__, page_id+count, retval);
			return retval;
		}

		if (main_left){
#ifdef CONFIG_MTD_SPINAND_SWECC
			retval = spinand_correct_data(mtd);

			if (retval == -1)
				pr_info("SWECC uncorrectable error! page=%x\n", page_id+count);
			else if (retval == 1)
				pr_info("SWECC 1 bit error, corrected! page=%x\n", page_id+count);
#endif

			main_ok += size;
			main_left -= size;
			main_offset = 0;
			ops->retlen = main_ok;
		}

		if (oob_left){
			int len = 0;

			switch(ops->mode){

			case MTD_OPS_PLACE_OOB:
			case MTD_OPS_RAW:
				if(oob_offs >= info->page_spare_size){
					oob_offs -= info->page_spare_size;
					break;
				}

				len = min_t(u32, oob_left,
					info->page_spare_size - oob_offs);

				memcpy(ops->oobbuf + oob_ok,
					chip->buf + info->page_main_size + oob_offs, len);

				oob_ok += len;
				oob_left -= len;
				oob_offs = 0;
				break;

			case MTD_OPS_AUTO_OOB:{

				struct nand_oobfree *free = mtd->ecclayout->oobfree;
				uint32_t boffs = 0;
				size_t bytes = 0;
				len = min_t(u32, oob_left, mtd->ecclayout->oobavail);

				for (; free->length && len; free++, len -= bytes) {
					/* Read request not from offset 0? */
					if (unlikely(oob_offs)) {
						if (oob_offs >= free->length) {
							oob_offs -= free->length;
							continue;
						}
						boffs = free->offset + oob_offs;
						bytes = min_t(size_t, len, (free->length - oob_offs));
						oob_offs = 0;
					} else {
						bytes = min_t(size_t, len, free->length);
						boffs = free->offset;
					}

					memcpy(ops->oobbuf + oob_ok,
						chip->buf + info->page_main_size + boffs, bytes);
					oob_ok += bytes;
					oob_left -= bytes;
				}
				break;
			}

			default: BUG();
			}

			ops->oobretlen = oob_ok;
		}
		count++;
		page_left--;
	}

	return corrected;
}

static int spinand_write_ops(struct mtd_info *mtd, loff_t to, struct mtd_oob_ops *ops)
{
	struct spinand_chip *chip = mtd_to_chip(mtd);
	struct spinand_info *info = chip->info;
	int page_id, page_offset = 0, page_num = 0, oob_num = 0;
	int main_ok = 0, main_left = 0, main_offset = 0;
	int oob_ok = 0, oob_left = 0;
	int retval = 0, count = 0;
	int oobsize = 0;
	u32 oob_offs = ops->ooboffs;

	page_id = to >> info->page_shift;

	if(likely(ops->datbuf)){
		page_offset = to & info->page_mask;
		page_num = (page_offset + ops->len + info->page_main_size -1 ) /
							info->page_main_size;
		main_left = ops->len;
		main_offset = page_offset;

	}

	if(unlikely(ops->oobbuf)){
		oobsize = ops->mode == MTD_OPS_AUTO_OOB ?
			info->ecclayout->oobavail : info->page_spare_size;
		oob_num = (ops->ooblen + oobsize -1) / oobsize;
		oob_left = ops->ooblen;

		if((ops->ooblen + ops->ooboffs) > oobsize || ops->ooboffs >= oobsize)
			return -EINVAL;
	}

	while (count < page_num || count < oob_num){

		memset(chip->buf, 0xFF, info->page_size);

		if (count < page_num && ops->datbuf) {
			int size = min_t(u32, main_left,
				info->page_main_size - main_offset);

			memcpy (chip->buf, ops->datbuf + main_ok, size);

			main_ok += size;
			main_left -= size;
			main_offset = 0;

#ifdef CONFIG_MTD_SPINAND_SWECC
			spinand_calculate_ecc(mtd);
#endif
		}

		if (count < oob_num && ops->oobbuf && chip->oobbuf) {
			int size = 0;

			switch(ops->mode){

			case MTD_OPS_PLACE_OOB:
			case MTD_OPS_RAW:
				size = min_t(ssize_t, oob_left, info->page_spare_size);
				memcpy (chip->buf + info->page_main_size, ops->oobbuf, size);
				break;

			case MTD_OPS_AUTO_OOB:{
				struct nand_oobfree *free = mtd->ecclayout->oobfree;
				size_t bytes = 0;
				int tmp = 0, boffs = 0;
				size = min_t(ssize_t, oob_left, info->ecclayout->oobavail);

				memcpy (chip->oobbuf, ops->oobbuf + oob_ok, size);

				for (; free->length && size; free++, size -= bytes) {

					if (unlikely(oob_offs)) {
						if (oob_offs >= free->length) {
							oob_offs -= free->length;
							continue;
						}
						boffs = free->offset + oob_offs;
						bytes = min_t(size_t, size, (free->length - oob_offs));
						oob_offs = 0;
					} else {
						bytes = min_t(size_t, size, free->length);
						boffs = free->offset;
					}

					memcpy (chip->buf + info->page_main_size + boffs,
						chip->oobbuf + tmp, bytes);
					tmp += bytes;
					oob_ok += bytes;
					oob_left -= bytes;
				}
				break;
			}
			default:BUG();
			}

		}

		retval = chip->program_page(chip,
			page_id + count, 0, info->page_size, chip->buf);

		if (retval != 0){
			pr_debug("%s: fail, page=%d!\n",__func__, page_id);
			return retval;
		}

		if (count < page_num && ops->datbuf)
			ops->retlen = main_ok;

		if (count < oob_num && ops->oobbuf && chip->oobbuf)
			ops->oobretlen = oob_ok;

		count++;

	}

	return 0;
}

static int spinand_read(struct mtd_info *mtd, loff_t from, size_t len, size_t *retlen, u_char *buf)
{
	int ret;

	struct mtd_oob_ops ops = {
		.len = len,
		.datbuf = buf,
	};

	/* Do not allow reads past end of device */
	if ((from + len) > mtd->size)
		return -EINVAL;

	if (!len)
		return 0;

	spinand_get_device(mtd, FL_READING);

	ret = spinand_read_ops(mtd, from, &ops);

	spinand_release_device(mtd);

 	*retlen = ops.retlen;
	return ret;
}

static int spinand_write(struct mtd_info *mtd,
	loff_t to, size_t len, size_t *retlen, const u_char *buf)
{
	int ret;
	struct mtd_oob_ops ops = {
		.len = len,
		.datbuf = (u8*)buf,
	};

	/* Do not allow reads past end of device */
	if ((to + len) > mtd->size)
		return -EINVAL;

	if (!len)
		return 0;

	spinand_get_device(mtd, FL_WRITING);

	ret = spinand_write_ops(mtd, to, &ops);

	spinand_release_device(mtd);

	*retlen = ops.retlen;

	return ret;
}

static int spinand_read_oob(struct mtd_info *mtd, loff_t from, struct mtd_oob_ops *ops)
{
	int ret;

	switch(ops->mode){
	case MTD_OPS_PLACE_OOB:
	case MTD_OPS_RAW:
	case MTD_OPS_AUTO_OOB:
		break;
	default:
		return -EINVAL;
	}

	spinand_get_device(mtd, FL_READING);
	ret = spinand_read_ops(mtd, from, ops);
	spinand_release_device(mtd);

	return ret;
}

static int spinand_write_oob(struct mtd_info *mtd, loff_t to, struct mtd_oob_ops *ops)
{
	int ret;

	switch(ops->mode){
	case MTD_OPS_PLACE_OOB:
	case MTD_OPS_RAW:
	case MTD_OPS_AUTO_OOB:
		break;
	default:
		return -EINVAL;
	}

	if(!ops->datbuf)
		ops->len = 0;

	spinand_get_device(mtd, FL_WRITING);
	ret = spinand_write_ops(mtd, to, ops);
	spinand_release_device(mtd);

	return ret;
}

#define bbt_mask_bad(_chip, _blk_id) do{\
	int _block_id = _blk_id;\
	u8 *_bbt = (_chip)->bbt + (_block_id>>3);\
	*_bbt |= 0x01 << (_block_id & 0x7);\
}while(0)

#define bbt_block_isbad(_chip, _blk_id) ({\
	int _block_id = _blk_id;\
	u8 *_bbt = (_chip)->bbt + (_block_id>>3);\
	bool _is_bad = (*_bbt >> (_block_id&0x7)) & 0x01;\
	_is_bad;\
})

static int spinand_erase(struct mtd_info *mtd, struct erase_info *instr)
{
	struct spinand_chip *chip = mtd_to_chip(mtd);
	struct spinand_info *info = chip->info;
	u16 block_id, block_num, count;
	int retval = 0, errcode = 0;
	u8 is_bad = 0;

	pr_debug("spinand_erase: start = 0x%llx, len = %llu\n",instr->addr, instr->len);

	/* check address align on block boundary */
	if (instr->addr & info->block_mask) {
		pr_debug("spinand_erase: Unaligned address\n");
		return -EINVAL;
	}

	if (instr->len & info->block_mask) {
		pr_debug("spinand_erase: Length not block aligned\n");
		return -EINVAL;
	}

	/* Do not allow erase past end of device */
	if ((instr->len + instr->addr) > info->usable_size) {
		pr_debug("spinand_erase: Erase past end of device\n");
		return -EINVAL;
	}

	instr->fail_addr = MTD_FAIL_ADDR_UNKNOWN;


	/* Grab the lock and see if the device is available */
	spinand_get_device(mtd, FL_ERASING);

	block_id  = instr->addr >> info->block_shift;
	block_num = instr->len >> info->block_shift;
	count = 0;

	while (count < block_num){
		/* Check if we have a bad block, we do not erase bad blocks! */
		is_bad = bbt_block_isbad(chip, block_id + count);
		if(is_bad){
			pr_warn("%s: attempt to erase a bad block at 0x%x\n",
						__func__, block_id << info->block_shift);
			instr->fail_addr = block_id << info->block_shift;
			errcode = -EPERM;
			break;
		}

		retval = chip->erase_block(chip, block_id+count);
		if (retval < 0){
			retval = chip->erase_block(chip, block_id + count);
			if (retval < 0){
				pr_warn("%s: fail, block=%d!\n",__func__, block_id+count);
				instr->fail_addr = block_id << info->block_shift;
				errcode = -EPERM;
				break;
			}
		}
		count++;
	}

	if(!errcode)
		instr->state = MTD_ERASE_DONE;

	/* Deselect and wake up anyone waiting on the device */
	spinand_release_device(mtd);

	/* Do call back function */
	if(instr->callback)
		instr->callback(instr);

	return errcode;
}

static void spinand_sync(struct mtd_info *mtd)
{
	spinand_get_device(mtd, FL_SYNCING);
	spinand_release_device(mtd);
}

static int spinand_block_isbad(struct mtd_info *mtd, loff_t ofs)
{
	struct spinand_chip *chip = mtd_to_chip(mtd);
	struct spinand_info *info = chip->info;
	int block_id = ofs >> info->block_shift;
	int is_bad;

	spinand_get_device(mtd, FL_READING);

	is_bad = bbt_block_isbad(chip, block_id);

	spinand_release_device(mtd);

	return is_bad;
}

static int spinand_block_markbad(struct mtd_info *mtd, loff_t ofs)
{
	struct spinand_chip *chip = mtd_to_chip(mtd);
	struct spinand_info *info = chip->info;
	int block_id = ofs >> info->block_shift;
	u8 is_bad = 0;
	struct mtd_oob_ops ops = {
		.mode = MTD_OPS_RAW,
		.ooblen = 1,
		.ooboffs = 0,
		.oobbuf = &is_bad,
	};

	bbt_mask_bad(chip, block_id);

	return spinand_write_oob(mtd, ofs&~info->block_mask, &ops);
}

static int spinand_suspend(struct mtd_info *mtd)
{
	return spinand_get_device(mtd, FL_PM_SUSPENDED);
}

static void spinand_resume(struct mtd_info *mtd)
{
	struct spinand_chip *chip = mtd_to_chip(mtd);
if (chip->state == FL_PM_SUSPENDED)
		spinand_release_device(mtd);
	else
		pr_err("resume() called for the chip which is not in suspended state\n");
}

static int chip_check(struct spinand_chip *chip)
{
	if(!chip->read_page || !chip->program_page ||
		!chip->erase_block || !chip->cmd_func)
		return -EINVAL;

	return 0;
}

static int spinand_scan_bad_blocks(struct mtd_info *mtd)
{
	struct spinand_chip *chip = mtd_to_chip(mtd);
	struct spinand_info *info = chip->info;
	int block_count = mtd->size >> info->block_shift;
	int i, ret, page_id;
	unsigned int corrected;
	u8 is_bad;

	pr_debug("%s: total blocks %d\n", __func__, block_count);

	chip->bbt = kzalloc(block_count >> 3, GFP_KERNEL);
	if(!chip->bbt){
		pr_debug("%s: kmalloc error!\n", __func__);
		return -EFAULT;
	}

	if(chip->hwecc)
		chip->hwecc(chip, 0);

	for(i = 0; i < block_count; ++i){
		page_id = i << (info->block_shift - info->page_shift);
		ret = chip->read_page(chip, page_id, info->page_main_size, 1, &is_bad, &corrected);
		if(ret < 0 || is_bad != 0xff){
			pr_debug("block %d is bad\n", i);
			bbt_mask_bad(chip, i);
		}
	}

	if(chip->hwecc)
		chip->hwecc(chip, 1);

	return 0;
}

int spinand_mtd_register(struct spinand_chip *chip, uint16_t spinand_id)
{
	struct spinand_info *info;
	struct mtd_info *mtd;
	struct mtd_part_parser_data ofpart;

	if(chip_check(chip) < 0)
		return -EINVAL;

	info = spinand_id_probe(spinand_id, chip);
	if(!info)
		return -ENODEV;

	mtd = chip_to_mtd(chip);
	chip->info = info;
	chip->state = FL_READY;
	chip->buf = kmalloc(info->page_size + info->page_spare_size, GFP_KERNEL);
	if(!chip->buf){
		pr_err("kmalloc buffer error!\n");
		return -ENOMEM;
	}
	chip->oobbuf =chip->buf + info->page_size;

	init_waitqueue_head(&chip->wq);
	spin_lock_init(&chip->chip_lock);

	mtd->priv		= chip;
	mtd->name		= chip->spi->dev.of_node->name;
	mtd->size		= info->usable_size;
	mtd->erasesize		= info->block_main_size;
	mtd->writesize		= info->page_main_size;
	/* mtd->writebufsize 必须是 mtd->writesize 的整数倍, nand一般为一个page大小 */
	mtd->writebufsize	= mtd->writesize;
	mtd->oobsize		= info->page_spare_size;
	mtd->oobavail		= info->ecclayout->oobavail;
	mtd->owner		= THIS_MODULE;
	mtd->type		= MTD_NANDFLASH;
	mtd->flags		= MTD_WRITEABLE | MTD_POWERUP_LOCK;
	mtd->erasesize_shift	= info->block_shift;
	mtd->writesize_shift	= info->page_shift;
	mtd->erasesize_mask	= info->block_mask;
	mtd->writesize_mask	= info->page_mask;
	mtd->ecclayout		= info->ecclayout;
	mtd->_erase		= spinand_erase;
	mtd->_read		= spinand_read;
	mtd->_write		= spinand_write;
	mtd->_read_oob		= spinand_read_oob;
	mtd->_write_oob		= spinand_write_oob;
	mtd->_sync		= spinand_sync;
	mtd->_unlock		= spinand_unlock;
	mtd->_suspend		= spinand_suspend;
	mtd->_resume		= spinand_resume;
	mtd->_block_isbad	= spinand_block_isbad;
	mtd->_block_markbad	= spinand_block_markbad;
	mtd->bitflip_threshold  = chip->ecc_strength;
	ofpart.of_node		= chip->spi->dev.of_node;

	pr_info("SPINAND: %s, size = %lld M, page size = %d KB, id = 0x%x\n", \
		info->name, info->usable_size >> 20, info->page_size>>10, info->id);

	if(spinand_scan_bad_blocks(mtd) < 0)
		goto err_exit;

	return mtd_device_parse_register(mtd, NULL, &ofpart, NULL, 0);

err_exit:
	if(chip->buf)
		kfree(chip->buf);

	return -EPERM;
}
EXPORT_SYMBOL_GPL(spinand_mtd_register);

int spinand_mtd_unregister(struct spinand_chip *chip)
{
	mtd_device_unregister(&chip->mtd);

	if(chip->bbt)
		kfree(chip->bbt);

	if(chip->buf)
		kfree(chip->buf);

	return 0;
}
EXPORT_SYMBOL_GPL(spinand_mtd_unregister);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("nationalchip");
