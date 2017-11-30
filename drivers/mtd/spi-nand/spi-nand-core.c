#include <linux/err.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/math64.h>
#include <linux/sizes.h>
#include <linux/mtd/mtd.h>
#include <linux/of_platform.h>
#include <linux/spi/flash.h>
#include "spi-nand.h"

#define ID_TABLE_FILL(_id, _info, _name, _wp_info) {                           \
	.id      = _id,                                                        \
	.info    = _info,                                                      \
	.wp_info = _wp_info,                                                   \
	.name    = _name,                                                      \
}

#define WP_TABLE_FILL(_bp, _bp_sh, _ct, ct_sh, _mode, _range) {                \
	.bp_mask    = _bp,                                                     \
	.bp_shift   = _bp_sh,                                                  \
	.ctrl_mask  = _ct,                                                     \
	.ctrl_shift = ct_sh,                                                   \
	.mode       = _mode,                                                   \
	.range      = _range,                                                  \
}

enum {
	NAND_1G_PAGE2_OOB64 = 0,
	NAND_2G_PAGE2_OOB64,
};

enum vendor {
	MT29F1G01ZAC,
	GD5F1G,
	GD5F2G,
	W25NO1G,
	F50L1G,
	TC58CVG053HRA1G,
	MX35LF1GE4AB,
	PN26G01AWS1UG,
};

struct nand_id_index{
	uint16_t    id;
	const char *name;
	struct spi_flash_info *info;
	struct spi_nand_wp    *wp_info;
};

static struct spi_flash_info nand_info[] = {
	/**
	 *  0x112c 0x122c 0x132c 0xc8f1 0xf1c8 0xc8d1
	 *  0xd1c8 0xaaef 0x21C8 0xc298 0x12c2 0xe1a1
	 */
	[NAND_1G_PAGE2_OOB64] = {
		.nand_size          = 1024 * 64 * 2112,
		.usable_size        = 1024 * 64 * 2048,
		.block_size         = 2112*64,
		.block_main_size    = 2048*64,
		.block_num_per_chip = 1024,
		.page_size          = 2112,
		.page_main_size     = 2048,
		.page_spare_size    = 64,
		.page_num_per_block = 64,
		.block_shift        = 17,
		.block_mask         = 0x1ffff,
		.page_shift         = 11,
		.page_mask          = 0x7ff,
	},
	[NAND_2G_PAGE2_OOB64] = { // 0xc8f2
		.nand_size          = (2048 * 64 * 2112),
		.usable_size        = (2048 * 64 * 2048),
		.block_size         = (2112*64),
		.block_main_size    = (2048*64),
		.block_num_per_chip = 2048,
		.page_size          = 2112,
		.page_main_size     = 2048,
		.page_spare_size    = 64,
		.page_num_per_block = 64,
		.block_shift        = 17,
		.block_mask         = 0x1ffff,
		.page_shift         = 11,
		.page_mask          = 0x7ff,
	},
};


static spi_nand_wp_t wp_range_table[] = {
	[GD5F1G]          = WP_TABLE_FILL(0x38, 3, 0x06, 1, WP_BOTTOM, NULL),
	[GD5F2G]          = WP_TABLE_FILL(0x38, 3, 0x06, 1, WP_BOTTOM, NULL),
	[TC58CVG053HRA1G] = WP_TABLE_FILL(0x38, 3, 0x00, 1, WP_BOTTOM, NULL),
};

static struct nand_id_index id_table[] = {
	ID_TABLE_FILL( 0x112C, nand_info + NAND_1G_PAGE2_OOB64, "MT29F1G01ZAC"    ,NULL                            ),
	ID_TABLE_FILL( 0x122C, nand_info + NAND_1G_PAGE2_OOB64, "MT29F1G01ZAC"    ,NULL                            ),
	ID_TABLE_FILL( 0x132C, nand_info + NAND_1G_PAGE2_OOB64, "MT29F1G01ZAC"    ,NULL                            ),
	ID_TABLE_FILL( 0xC8F1, nand_info + NAND_1G_PAGE2_OOB64, "GD5F1G"          ,wp_range_table + GD5F1G         ),
	ID_TABLE_FILL( 0xF1C8, nand_info + NAND_1G_PAGE2_OOB64, "GD5F1G"          ,wp_range_table + GD5F1G         ),
	ID_TABLE_FILL( 0xC8D1, nand_info + NAND_1G_PAGE2_OOB64, "GD5F1G"          ,wp_range_table + GD5F1G         ),
	ID_TABLE_FILL( 0xC8D2, nand_info + NAND_2G_PAGE2_OOB64, "GD5F2G"          ,wp_range_table + GD5F2G         ),
	ID_TABLE_FILL( 0xD1C8, nand_info + NAND_1G_PAGE2_OOB64, "GD5F1G"          ,wp_range_table + GD5F1G         ),
	ID_TABLE_FILL( 0xAAEF, nand_info + NAND_1G_PAGE2_OOB64, "W25NO1G"         ,NULL                            ),
	ID_TABLE_FILL( 0x21C8, nand_info + NAND_1G_PAGE2_OOB64, "F50L1G"          ,NULL                            ),
	ID_TABLE_FILL( 0xc298, nand_info + NAND_1G_PAGE2_OOB64, "TC58CVG053HRA1G" ,wp_range_table + TC58CVG053HRA1G),
	ID_TABLE_FILL( 0x12c2, nand_info + NAND_1G_PAGE2_OOB64, "MX35LF1GE4AB"    ,NULL                            ),
	ID_TABLE_FILL( 0xe1a1, nand_info + NAND_1G_PAGE2_OOB64, "PN26G01AWS1UG"   ,NULL                            ),
	ID_TABLE_FILL( 0x0001, nand_info + NAND_1G_PAGE2_OOB64, "General flash"   ,NULL                            ),
	ID_TABLE_FILL( 0xC8F2, nand_info + NAND_2G_PAGE2_OOB64, "GD5F2G"          ,NULL                            ),
	ID_TABLE_FILL( ~0,     NULL,                            "NULL"            ,NULL                            ),
};

static struct spi_flash_info *snd_get_info(uint16_t id, const char *name)
{
	struct nand_id_index *tb = id_table;

	while(tb->info != NULL){
		if(id > 0){
			if(id == tb->id)
				break;
		}else if(name){
			if(!strcmp(name, tb->name))
				break;
		}
		++tb;
	}

	if(tb->info != NULL){
		tb->info->nand_id = tb->id;
		tb->info->name    = tb->name;
		tb->info->wp_info = tb->wp_info;
		return tb->info;
	}

	printk("warning: unknow flash id = 0x%x, name %s\n", id, name);

	return NULL;
}

static int spi_nand_check(struct spi_nand *nand)
{
	snd_ops_t *ops = &nand->snd_ops;

	if (!nand->dev  || !ops->read  || \
		!ops->write || !ops->erase || !ops->cmdfunc){
		pr_err("spi-nor: please fill all the necessary fields!\n");
		return -EINVAL;
	}

	return 0;
}

static int snd_read_id(struct spi_nand *nand)
{
	uint16_t   id  = 0;
	snd_ops_t *ops = &nand->snd_ops;

	snd_cmd_t cmd = {
		.cmd_len  = 2,
		.cmd      = { CMD_READ_ID, ~0, },
		.xfer_len = 2,
		.rx       = (uint8_t*)&id,
	};
	ops->cmdfunc(nand, &cmd);
	return id;
}

#define MTD_TO_SPI_NAND(_mtd)  ( (_mtd)->priv )

#define SPI_NAND_FUNC(_mtd, _func, args...) do{                                \
	struct spi_nand *_nand = MTD_TO_SPI_NAND(_mtd);                        \
	int _ret;                                                              \
	if(!_nand->snd_ops._func)                                              \
		return -EOPNOTSUPP;                                            \
	mutex_lock(&_nand->lock);                                              \
	_ret = _nand->snd_ops._func(_nand, ##args);                            \
	mutex_unlock(&_nand->lock);                                            \
	return _ret;                                                           \
}while(0)

static int spi_nand_erase(struct mtd_info *mtd, struct erase_info *instr)
{
	struct spi_nand *nand = MTD_TO_SPI_NAND(mtd);
	int ret;

	if(!nand->snd_ops.erase)
		return -EOPNOTSUPP;

	mutex_lock(&nand->lock);
	ret = nand->snd_ops.erase(nand, instr->addr, instr->len);
	instr->state = ret < 0 ? MTD_ERASE_FAILED : MTD_ERASE_DONE;
	mutex_unlock(&nand->lock);

	return ret;
}

static int spi_nand_read(struct mtd_info *mtd, \
	   loff_t from, size_t len, size_t *retlen, u_char *buf)
{
	if(!buf || !retlen){
		dev_dbg(&mtd->dev, "PARAM ERROR!\n");
		return -EINVAL;
	}
	SPI_NAND_FUNC(mtd, read, from, len, retlen, buf);
}

static int spi_nand_write(struct mtd_info *mtd, \
	   loff_t to, size_t len, size_t *retlen, const u_char *buf)
{
	if(!buf || !retlen){
		dev_dbg(&mtd->dev, "PARAM ERROR!\n");
		return -EINVAL;
	}
	SPI_NAND_FUNC(mtd, write, to, len, retlen, buf);
}

static int spi_nand_read_oob(struct mtd_info *mtd,
				loff_t from, struct mtd_oob_ops *ops)
{
	SPI_NAND_FUNC(mtd, readoob, from, ops);
}

static int spi_nand_write_oob(struct mtd_info *mtd,\
				loff_t to, struct mtd_oob_ops *ops)
{
	SPI_NAND_FUNC(mtd, writeoob, to, ops);
}

static int spi_nand_lock(struct mtd_info *mtd, loff_t ofs, uint64_t len)
{
	SPI_NAND_FUNC(mtd, lock, ofs, len);
}

static int spi_nand_unlock(struct mtd_info *mtd, loff_t ofs, uint64_t len)
{
	SPI_NAND_FUNC(mtd, unlock, ofs, len);
}

static int spi_nand_is_locked(struct mtd_info *mtd, loff_t ofs, uint64_t len)
{
	SPI_NAND_FUNC(mtd, is_locked, ofs, len);
	return 0;
}

static int spi_nand_block_isbad(struct mtd_info *mtd, loff_t ofs)
{
	SPI_NAND_FUNC(mtd, block_isbad, ofs);
}

static int spi_nand_block_markbad(struct mtd_info *mtd, loff_t ofs)
{
	SPI_NAND_FUNC(mtd, block_markbad, ofs);
}

static void spi_nand_sync(struct mtd_info *mtd)
{
	struct spi_nand *nand = MTD_TO_SPI_NAND(mtd);

	mutex_lock(&nand->lock);

	if(nand->snd_ops.sync)
		nand->snd_ops.sync(nand);

	mutex_unlock(&nand->lock);
}

int spi_nand_scan(struct spi_nand *nand, const char *name)
{

	struct device      *dev = nand->dev;
	struct mtd_info    *mtd = &nand->mtd;
	snd_info_t         *info;

	if(spi_nand_check(nand) < 0)
		return -EINVAL;

	info = snd_get_info(snd_read_id(nand), NULL);

	if (name && !info)
		info = snd_get_info(0, name);

	if (IS_ERR_OR_NULL(info))
		return -ENOENT;
	nand->snd_info = info;
	printk("SPI NAND: %s, size = %d M, page size = %d KB, id = 0x%x\n", \
			info->name, info->usable_size >> 20, info->page_size>>10, info->nand_id);

	mutex_init(&nand->lock);

	if (!mtd->name)
		mtd->name = dev_name(dev);

	mtd->priv            = nand;
	mtd->type            = MTD_NANDFLASH;
	mtd->flags           = MTD_WRITEABLE | MTD_POWERUP_LOCK;
	mtd->size            = info->usable_size;
	mtd->writesize       = info->page_main_size;
	mtd->writebufsize    = info->page_main_size;;
	mtd->erasesize       = info->block_main_size;
	mtd->oobsize         = info->page_spare_size;
	mtd->oobavail        = info->page_spare_size;
	mtd->dev.parent      = dev;
	mtd->erasesize_shift = info->block_shift;
	mtd->writesize_shift = info->page_shift;
	mtd->erasesize_mask  = info->block_mask;
	mtd->writesize_mask  = info->page_mask;
	mtd->numeraseregions = 0;
	mtd->ecclayout       = NULL;

	mtd->_read           = spi_nand_read;
	mtd->_write          = spi_nand_write;
	mtd->_erase          = spi_nand_erase;
	mtd->_block_isbad    = spi_nand_block_isbad;
	mtd->_sync           = spi_nand_sync;
	mtd->_is_locked      = spi_nand_is_locked;
	mtd->_lock           = spi_nand_lock;
	mtd->_unlock         = spi_nand_unlock;
	mtd->_read_oob       = spi_nand_read_oob;
	mtd->_write_oob      = spi_nand_write_oob;
	mtd->_block_markbad  = spi_nand_block_markbad;

	return 0;
}
EXPORT_SYMBOL_GPL(spi_nand_scan);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("zhuzzh");
MODULE_DESCRIPTION("framework for SPI NAND");
