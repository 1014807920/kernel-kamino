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

#define ID_TABLE_FILL(_id, _info, _ecc,_name) {\
	.id		= _id,	\
	.info_index	= _info,\
	.ecc_index	= _ecc,	\
	.name		= _name,\
}

enum {
	NAND_1G_PAGE2K_OOB64 = 0,
	NAND_2G_PAGE2K_OOB64,
	NAND_4G_PAGE4K_OOB256,
};

enum {
	ECC_LAYOUT_DEFAULT_OOB64,
	ECC_LAYOUT_GD_OOB64,
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
		.eccbytes = 28,
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
		.eccbytes = 56,
		.eccpos = {
			8 , 9 , 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
			21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33,
			34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46,
			47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59,
			60, 61, 62, 63
		},
		.oobavail = 7,
		.oobfree = {
			{.offset = 1,	.length = 7},
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
};

static struct spinand_info s_nand_info[] = {
	/**
	 *  0x112c 0x122c 0x132c 0xc8f1 0xf1c8 0xc8d1
	 *  0xd1c8 0xaaef 0x21C8 0xc298 0x12c2 0xe1a1
	 */
	[NAND_1G_PAGE2K_OOB64] = {
		.nand_size		= 1024 * 64 * 2112,
		.usable_size		= 1024 * 64 * 2048,
		.block_size		= 2112*64,
		.block_main_size	= 2048*64,
		.block_num_per_chip	= 1024,
		.page_size		= 2112,
		.page_main_size		= 2048,
		.page_spare_size	= 64,
		.page_num_per_block	= 64,
		.block_shift		= 17,
		.block_mask		= 0x1ffff,
		.page_shift		= 11,
		.page_mask		= 0x7ff,
	},
	[NAND_2G_PAGE2K_OOB64] = { // 0xc8f2
		.nand_size		= (2048 * 64 * 2112),
		.usable_size		= (2048 * 64 * 2048),
		.block_size		= (2112*64),
		.block_main_size	= (2048*64),
		.block_num_per_chip	= 2048,
		.page_size		= 2112,
		.page_main_size		= 2048,
		.page_spare_size	= 64,
		.page_num_per_block	= 64,
		.block_shift		= 17,
		.block_mask		= 0x1ffff,
		.page_shift		= 11,
		.page_mask		= 0x7ff,
	},
	[NAND_4G_PAGE4K_OOB256] = { // 0xd4c8
		.nand_size		= (2048 * 64 * 4352),
		.usable_size		= (2048 * 64 * 4096),
		.block_size		= (4352*64),
		.block_main_size	= (4096*64),
		.block_num_per_chip	= 2048,
		.page_size		= 4352,
		.page_main_size		= 4096,
		.page_spare_size	= 256,
		.page_num_per_block	= 64,
		.block_shift		= 18,
		.block_mask		= 0x3ffff,
		.page_shift		= 12,
		.page_mask		= 0xfff,
	},
};


static struct spinand_index s_id_table[] = {
	/* GD spi nand flash */
	ID_TABLE_FILL(0xF1C8, NAND_1G_PAGE2K_OOB64, ECC_LAYOUT_GD_OOB64,	"GD5F1GQ4UAYIG"),
	ID_TABLE_FILL(0xD1C8, NAND_1G_PAGE2K_OOB64, ECC_LAYOUT_GD_OOB64,	"GD5F1GQ4U"),
	ID_TABLE_FILL(0xC1C8, NAND_1G_PAGE2K_OOB64, ECC_LAYOUT_GD_OOB64,	"GD5F1GQ4R"),
	ID_TABLE_FILL(0xD2C8, NAND_2G_PAGE2K_OOB64, ECC_LAYOUT_GD_OOB64,	"GD5F2GQ4U"),
	ID_TABLE_FILL(0xC2C8, NAND_2G_PAGE2K_OOB64, ECC_LAYOUT_GD_OOB64,	"GD5F2GQ4R"),
	ID_TABLE_FILL(0xF2C8, NAND_2G_PAGE2K_OOB64, ECC_LAYOUT_GD_OOB64,	"GD5F2GQ4RAYIG"),
	ID_TABLE_FILL(0xD4C8, NAND_4G_PAGE4K_OOB256,ECC_LAYOUT_GD_OOB256,	"GD5F4G"),
	/* TOSHIBA spi nand flash */
	ID_TABLE_FILL(0xC298, NAND_1G_PAGE2K_OOB64, ECC_LAYOUT_TC58CV_OOB64,	"TC58CVG053HRA1G"),
	/* Micron spi nand flash */
	ID_TABLE_FILL(0x122C, NAND_1G_PAGE2K_OOB64, ECC_LAYOUT_MT29F_OOB64,	"MT29F1G01ZAC"),
	ID_TABLE_FILL(0x112C, NAND_1G_PAGE2K_OOB64, ECC_LAYOUT_MT29F_OOB64,	"MT29F1G01ZAC"),
	ID_TABLE_FILL(0x132C, NAND_1G_PAGE2K_OOB64, ECC_LAYOUT_MT29F_OOB64,	"MT29F1G01ZAC"),
	/* 芯天下 spi nand flash */
	ID_TABLE_FILL(0xE1A1, NAND_1G_PAGE2K_OOB64, ECC_LAYOUT_PN26G_OOB64,	"PN26G01AWS1UG"),
	ID_TABLE_FILL(0xE10B, NAND_1G_PAGE2K_OOB64, ECC_LAYOUT_XT26G_OOB64,	"XT26G01AWS1UG"),
	ID_TABLE_FILL(0xE20B, NAND_2G_PAGE2K_OOB64, ECC_LAYOUT_XT26G_OOB64,	"XT26G02AWSEGA"),
	/* Zetta Confidentia spi nand flash */
	ID_TABLE_FILL(0x71ba, NAND_1G_PAGE2K_OOB64, ECC_LAYOUT_ZD35X_OOB64,	"ZD35X1GA"),
	ID_TABLE_FILL(0x21ba, NAND_1G_PAGE2K_OOB64, ECC_LAYOUT_ZD35X_OOB64,	"ZD35X1GA"),
	/* ESMT spi nand flash */
	ID_TABLE_FILL(0x21C8, NAND_1G_PAGE2K_OOB64, ECC_LAYOUT_F50LXX1A_OOB64,	"F50L1G41A"),
	ID_TABLE_FILL(0x01C8, NAND_1G_PAGE2K_OOB64, ECC_LAYOUT_F50LXX41LB_OOB64,"F50L1G41LB"),
	/* winbond spi nand flash */
	ID_TABLE_FILL(0xAAEF, NAND_1G_PAGE2K_OOB64, ECC_LAYOUT_W25ND_OOB64,	"W25N01GV"),
	/* Mxic spi nand flash */
	ID_TABLE_FILL(0x12C2, NAND_1G_PAGE2K_OOB64, ECC_LAYOUT_MX35LF_OOB64,	"MX35LF1GE4AB"),
	ID_TABLE_FILL(0x0001, NAND_1G_PAGE2K_OOB64, ECC_LAYOUT_DEFAULT_OOB64,	"General flash"),
};

static int spinand_mtd_read_id(struct spinand_chip *chip)
{
	uint16_t id = 0;
	struct spinand_cmd cmd = {
		.cmd_len = 2,
		.cmd[0] = CMD_READ_ID,
		.cmd[1] = 0,
		.xfer_len = 2,
		.rx = (u8*)&id,
	};

	if(chip->read_id){
		chip->read_id(chip, &id);
		return id;
	}

	chip->cmd_func(chip, &cmd);
	return id;
}

static struct spinand_info *spinand_id_probe(u16 id)
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
		return info;
	}

	pr_warn("Warning: unknow flash id = 0x%x.\n", id);
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
	int page_id, page_offset = 0, page_num = 0, oob_num = 0;
	u32 oobsize = 0, oob_offs = ops->ooboffs;

	int count = 0;
	int main_ok = 0, main_left = 0, main_offset = 0;
	int oob_ok = 0, oob_left = 0;
	int retval;

	page_id = from >> info->page_shift;

	/* for main data */
	if(likely(ops->datbuf)){
		page_offset = from & info->page_mask;
		page_num = (page_offset + ops->len + info->page_main_size -1 ) /
							info->page_main_size;
		main_left = ops->len;
		main_offset = page_offset;
	/* for oob */
	}else if(ops->oobbuf){
		oob_num  = (ops->ooblen + info->ecclayout->oobavail -1) /
						info->ecclayout->oobavail;
		oob_left = ops->ooblen;
		oobsize  = ops->mode == MTD_OPS_AUTO_OOB ?
				info->ecclayout->oobavail : info->page_spare_size;

		if(ops->ooboffs >= oobsize)
			return -EINVAL;
	}else{
		return -EINVAL;
	}

	while (count < page_num || count < oob_num){
#ifdef CONFIG_MTD_SPINAND_SWECC
		retval = chip->read_page(chip,
			page_id + count, 0, info->page_size, chip->buf);
#else
		if(likely(ops->datbuf))
			retval = chip->read_page(chip,
				page_id + count, 0, info->page_size, chip->buf);
		else
			retval = chip->read_page(chip, page_id + count,
					info->page_main_size,
					info->page_spare_size,
					chip->buf + info->page_main_size);
#endif
		if (retval != 0){
			pr_debug("%s: fail, page=%d!\n",__func__, page_id);
			return retval;
		}

		if (count < page_num && ops->datbuf){

			int size = min(main_left, info->page_main_size - main_offset);
#ifdef CONFIG_MTD_SPINAND_SWECC
			retval = spinand_correct_data(mtd);

			if (retval == -1)
				pr_info("SWECC uncorrectable error! page=%x\n", page_id+count);
			else if (retval == 1)
				pr_info("SWECC 1 bit error, corrected! page=%x\n", page_id+count);
#endif

			memcpy (ops->datbuf + main_ok, chip->buf + main_offset, size);

			main_ok += size;
			main_left -= size;
			main_offset = 0;
			ops->retlen = main_ok;
		}

		if (count < oob_num && ops->oobbuf){
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
	}

	return 0;
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

	}else if(ops->oobbuf){
		oob_num = (ops->ooblen + info->ecclayout->oobavail -1) /
						info->ecclayout->oobavail;
		oob_left = ops->ooblen;
		oobsize = ops->mode == MTD_OPS_AUTO_OOB ?
			info->ecclayout->oobavail : info->page_spare_size;

		if((ops->ooblen + ops->ooboffs) > oobsize || ops->ooboffs >= oobsize)
			return -EINVAL;
	}else{
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
		ret = chip->read_page(chip, page_id, info->page_main_size, 1, &is_bad);
		if(ret < 0 || is_bad != 0xff){
			pr_debug("block %d is bad\n", i);
			bbt_mask_bad(chip, i);
		}
	}

	if(chip->hwecc)
		chip->hwecc(chip, 1);

	return 0;
}

int spinand_mtd_register(struct spinand_chip *chip)
{
	struct spinand_info *info;
	struct mtd_info *mtd;
	struct mtd_part_parser_data ofpart;
	u16 spinand_id;

	if(chip_check(chip) < 0)
		return -EINVAL;

	spinand_id = spinand_mtd_read_id(chip);
	info = spinand_id_probe(spinand_id);
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
