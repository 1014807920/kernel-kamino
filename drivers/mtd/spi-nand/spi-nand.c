#include "spi-nand.h"
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/completion.h>
#include <linux/gpio.h>
#include <linux/spi/spi.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/spinlock.h>
#include <linux/timer.h>
#include <linux/moduleparam.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/spi/spi.h>
#include <linux/scatterlist.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_platform.h>
#include <linux/property.h>
#include <linux/mtd/partitions.h>
#include <linux/spi/flash.h>
#include <linux/device.h>
#include <linux/platform_device.h>

#define DRIVER_NAME    "spi_nand"
#define NAND_SKIP_BAD  0

struct snd_slave{
	struct spi_nand      spi_nand;
	struct spi_device	*spi;
};

#define snd_read_from_cache(_nand, _id, _len, _rbuf) do{                       \
	snd_ops_t *_ops = &(_nand)->snd_ops;                                   \
	snd_cmd_t _cmd = {                                                     \
		.cmd_len  = 4,                                                 \
		.cmd      = {                                                  \
			CMD_READ_RDM,                                          \
			0xFF & (_id >> 8),                                     \
			0xFF & (_id >> 0),                                     \
			~0, 0,                                                 \
		},                                                             \
		.xfer_len = _len,                                              \
		.rx       = _rbuf,                                             \
	};                                                                     \
	_ops->cmdfunc(_nand, &_cmd);                                           \
}while(0)

#define snd_read_page_to_cache(_nand, _page_id) do{                            \
	snd_ops_t *_ops = &(_nand)->snd_ops;                                   \
	snd_cmd_t _cmd = {                                                     \
		.cmd_len  = 4,                                                 \
		.cmd      = {                                                  \
			CMD_READ,                                              \
			0xFF & (_page_id >> 16),                               \
			0xFF & (_page_id >> 8 ),                               \
			0xFF & (_page_id >> 0 ),                               \
		},                                                             \
		.xfer_len = 0,                                                 \
	};                                                                     \
	_ops->cmdfunc(_nand, &_cmd);                                           \
}while(0)

#define snd_get_status(_nand, _REG_STAT, _STATUS) do{                          \
	snd_ops_t *_ops = &(_nand)->snd_ops;                                   \
	snd_cmd_t _cmd = {                                                     \
		.cmd_len  = 2,                                                 \
		.cmd      = { CMD_READ_REG, _REG_STAT },                       \
		.xfer_len = 1,                                                 \
		.rx       = _STATUS,                                           \
	};                                                                     \
	_ops->cmdfunc(_nand, &_cmd);                                           \
}while(0)

#define snd_set_status(_nand, _REG_STAT, _STATUS) do{                          \
	snd_ops_t *_ops = &(_nand)->snd_ops;                                   \
	snd_cmd_t _cmd = {                                                     \
		.cmd_len  = 3,                                                 \
		.cmd      = { CMD_WRITE_REG, _REG_STAT, _STATUS},              \
		.xfer_len = 0,                                                 \
	};                                                                     \
	_ops->cmdfunc(_nand, &_cmd);                                           \
}while(0)

#define snd_write_enable(_nand) do{                                            \
	snd_ops_t *_ops = &(_nand)->snd_ops;                                   \
	snd_cmd_t _cmd = {                                                     \
		.cmd_len  = 1,                                                 \
		.cmd      = { CMD_WR_ENABLE },                                 \
		.xfer_len = 0,                                                 \
	};                                                                     \
	_ops->cmdfunc(_nand, &_cmd);                                           \
}while(0)

#define snd_erase_block_erase(_nand, _blkid) do{                               \
	snd_ops_t *_ops = &(_nand)->snd_ops;                                   \
	uint32_t _row = (_blkid) << 6;                                         \
	snd_cmd_t _cmd = {                                                     \
		.cmd_len  = 4,                                                 \
		.cmd      = {                                                  \
			CMD_ERASE_BLK,                                         \
			0xFF & (_row >> 16),                                   \
			0xFF & (_row >> 8 ),                                   \
			0xFF & (_row >> 0 ),                                   \
		},                                                             \
		.xfer_len = 0,                                                 \
	};                                                                     \
	_ops->cmdfunc(_nand, &_cmd);                                           \
}while(0)

#define snd_write_enable(_nand) do{                                            \
	snd_ops_t *_ops = &(_nand)->snd_ops;                                   \
	snd_cmd_t _cmd = {                                                     \
		.cmd_len  = 1,                                                 \
		.cmd      = { CMD_WR_ENABLE },                                 \
		.xfer_len = 0,                                                 \
	};                                                                     \
	_ops->cmdfunc(_nand, &_cmd);                                           \
}while(0)

#define snd_program_data_to_cache(_nand, _id, _len, _wbuf) do{                 \
	snd_ops_t *_ops = &(_nand)->snd_ops;                                   \
	snd_cmd_t _cmd = {                                                     \
		.cmd_len  = 3,                                                 \
		.cmd      = {                                                  \
			CMD_PROG_PAGE_CLRCACHE,                                \
			0XFF & (_id >> 8),                                     \
			0XFF & (_id >> 0),                                     \
		},                                                             \
		.xfer_len = _len,                                              \
		.tx       = _wbuf,                                             \
	};                                                                     \
	_ops->cmdfunc(_nand, &_cmd);                                           \
}while(0)

#define snd_program_execute(_nand, _id) do{                                    \
	snd_ops_t *_ops = &(_nand)->snd_ops;                                   \
	snd_cmd_t _cmd = {                                                     \
		.cmd_len  = 4,                                                 \
		.cmd      = {                                                  \
			CMD_PROG_PAGE_EXC,                                     \
			0XFF & (_id >> 16),                                    \
			0XFF & (_id >> 8 ),                                    \
			0XFF & (_id >> 0 ),                                    \
		},                                                             \
		.xfer_len = 0,                                                 \
	};                                                                     \
	_ops->cmdfunc(_nand, &_cmd);                                           \
}while(0)


static void snd_internal_ecc(struct spi_nand *nand, bool enable)
{
	uint8_t status;

	snd_get_status(nand, REG_OTP, &status);

	if(enable){
		status |= REG_ECC_MASK;
	}else{
		status &= ~REG_ECC_MASK;
	}

	snd_set_status(nand, REG_OTP, status);
}

static inline void snd_read_status(struct spi_nand *nand, uint8_t *status)
{
	snd_get_status(nand, REG_STATUS, status);
}

static int snd_page_read(struct spi_nand *nand, uint32_t page_id, \
	   uint16_t ofs, uint16_t len, uint8_t* rbuf)
{
	uint8_t  status = 0;

	snd_read_page_to_cache(nand, page_id);

	while (1){
		snd_read_status(nand, &status);
		if((status & STATUS_OIP_MASK) == STATUS_READY){
			if((status & STATUS_ECC_MASK) == STATUS_ECC_ERROR){
				dev_dbg(nand->dev, "spi nand page "\
					"read error, status = 0x%x\n", status);
				return -status;
			}
			break;
		}
		cpu_relax();
	}
	snd_read_from_cache(nand, ofs, len, rbuf);

	return 0;
}

static int snd_block_isbad(struct spi_nand *nand, loff_t ofs)
{
	snd_info_t *info  = nand->snd_info;
	uint16_t block_id = ofs >> info->block_shift;
	uint8_t  is_bad   = 0;

	snd_internal_ecc(nand, REG_ECC_OFF);       // disable internal ecc

	snd_page_read(nand, \
		block_id*info->page_num_per_block, info->page_main_size, 1, &is_bad);

	snd_internal_ecc(nand, REG_ECC_ON);        // enable internal ecc

	return is_bad == 0XFF ? 0 : 1;
}

static int snd_read_ops(struct spi_nand *nand, ssize_t from, struct mtd_oob_ops *ops)
{
	snd_info_t *info  = nand->snd_info;
	int32_t  page_id  = from >> info->page_shift;
	int32_t  page_num = (ops->len + info->page_main_size - 1) >> info->page_shift;
	int32_t  page_size= info->page_main_size;
	int32_t count     = 0, main_left = ops->len;
	uint8_t *pbuf     = ops->datbuf;
	int32_t  size;

	while (count < page_num){
		size = min(main_left, page_size);

		if(snd_page_read(nand, page_id + count, 0, size, pbuf) < 0){
			dev_dbg(nand->dev, "snd_read_ops: fail, page=%d!\n", page_id);
			return -1;
		}
		main_left -= size;
		pbuf      += size;
		count++;
	}

	ops->retlen = pbuf - ops->datbuf;

	return ops->retlen;
}

static int __snd_read(struct spi_nand *nand, loff_t from, uint8_t *buf, size_t len)
{
	snd_info_t *info = nand->snd_info;
	uint32_t blk_sz  = info->block_main_size;
	uint8_t *pbuf    = buf;
	struct   mtd_oob_ops ops;
	ssize_t  read_left;
	ssize_t  blk_ofs;

	if(len == 0) return 0;
	if(from + len > info->usable_size)
		len = info->usable_size - from;

	if(from & info->page_mask)                               /* 未按页对齐 */
		return -EFAULT;

	read_left = len;

	while(read_left > 0){
		blk_ofs = from & info->block_mask;

	#if NAND_SKIP_BAD                                       /* skip bad block */
		if(snd_block_isbad(nand, from & ~info->block_mask)){
			from += blk_sz - blk_ofs;
			continue;
		}
	#endif

		ops.datbuf = pbuf;
		ops.len    = min(read_left, (ssize_t)blk_sz-blk_ofs);

		if(snd_read_ops(nand, from, &ops) < 0)
			return -1;

		pbuf      += ops.retlen;
		from      += ops.retlen;
		read_left -= ops.retlen;
	}

	return pbuf-(uint8_t*)buf;
}

static int snd_read(struct spi_nand *nand, loff_t from,
				 size_t len, size_t *retlen, uint8_t *read_buf)
{
	snd_info_t *info  = nand->snd_info;
	uint32_t page_sz  = info->page_main_size;
	uint32_t page_off = from & (page_sz - 1);
	uint32_t tmp_len;
	uint8_t  *one_page;
	int32_t  ret = 0;

	if(page_off){
		#if NAND_SKIP_BAD
		while(snd_block_isbad(nand, from & ~info->block_mask))
			from += info->block_main_size;
		#endif

		one_page = kmalloc(page_sz, GFP_KERNEL);
		if(!one_page)
			return -ENOMEM;;

		tmp_len = min(page_sz - page_off, len);

		ret = __snd_read(nand, from-page_off, one_page, page_sz);

		if(ret <= 0) {
			kfree(one_page);
			return ret;
		}

		memcpy(read_buf, one_page + page_off, tmp_len);

		kfree(one_page);

		*retlen  += tmp_len;
		read_buf += tmp_len;
		from     += tmp_len;
		len      -= tmp_len;

	}

	if(len > 0){
		ret = __snd_read(nand, from, read_buf, len);
		if(ret < 0) return ret;
		*retlen += ret;
	}

	return 0;
}

static int32_t  snd_page_program(struct spi_nand *nand, \
	   uint32_t page_id, uint16_t offset, uint16_t len, uint8_t* wbuf)
{
	uint8_t status;

	snd_write_enable(nand);
	snd_program_data_to_cache(nand, offset, len, wbuf);
	snd_program_execute(nand, page_id);

	while (1){
		snd_read_status(nand, &status);
		if(STATUS_READY == (status & STATUS_OIP_MASK)){
			if(STATUS_P_FAIL == (status & STATUS_P_FAIL_MASK)){
				dev_dbg(nand->dev, "spi nand "\
					"write error,nand status = 0x%x\n", status);
				return -status;
			}
			break;
		}
		cpu_relax();
	}

	return 0;
}

static int snd_write_ops(struct spi_nand *nand, ssize_t offset, struct mtd_oob_ops *ops)
{
	snd_info_t *info   = nand->snd_info;
	int32_t  page_id   = offset >> info->page_shift;
	int32_t  page_num  = (ops->len + info->page_main_size - 1) >> info->page_shift;
	uint32_t count     = 0, main_left = ops->len;
	uint32_t page_size = info->page_main_size;
	uint8_t *pbuf      = ops->datbuf;
	uint32_t size;
	int32_t  ret;

	while (count < page_num){
		size = min(main_left, page_size);
		ret = snd_page_program(nand, page_id + count, 0, size, pbuf);
		if(ret < 0){
			dev_dbg(nand->dev, "snd_page_program error, errno = %d\n", ret);
			return ret;
		}

		main_left -= size;
		pbuf      += size;
		count++;
	}

	return (ops->retlen = pbuf - ops->datbuf);
}

static int __snd_write(struct spi_nand *nand, ssize_t offset, void *buf, uint32_t len)
{
	struct mtd_oob_ops ops;
	snd_info_t *info  = nand->snd_info;
	uint32_t block_sz = info->block_main_size;
	uint8_t *pbuf = buf;
	uint32_t write_left;
	ssize_t  blk_ofs;
	int ret;

	if(offset & info->page_mask){
		dev_dbg(nand->dev, "The address is not aligned by page!\n");
		return -EPERM;
	}

	if(offset + len > info->usable_size)
		len = info->usable_size - offset;

	write_left = len;
	while(write_left > 0){
		blk_ofs = offset & info->block_mask;

	#if NAND_SKIP_BAD                    /* skip bad block */
		if(snd_block_isbad(nand, offset & ~info->block_mask)){
			offset += block_sz - blk_ofs;
			continue;
		}
	#endif

		ops.datbuf = pbuf;
		ops.len    = min(write_left, (block_sz - blk_ofs));

		ret = snd_write_ops(nand, offset, &ops);

		if(ret < 0){
			dev_dbg(nand->dev, "snd_write_ops error, errno = %d\n",ret);
			return ret;
		}

		pbuf       += ops.retlen;
		offset     += ops.retlen;
		write_left -= ops.retlen;
	}
	return pbuf - (uint8_t*)buf;
}

static int snd_write(struct spi_nand *nand, loff_t to,
				 size_t len, size_t *retlen, const uint8_t *write_buf)
{
	snd_info_t *info  = nand->snd_info;
	uint32_t page_sz  = info->page_main_size;
	uint32_t page_off = to & (page_sz - 1);
	int32_t  ret = 0;
	uint32_t tmp_len;
	uint8_t *one_page;

	if(len == 0) return 0;

	if(page_off){
		#if NAND_SKIP_BAD
		while(snd_block_isbad(nand, to & ~info->block_mask))
			to += info->block_main_size;
		#endif

		tmp_len  = min(page_sz-page_off, len);
		one_page = kmalloc(page_sz, GFP_KERNEL);
		if(!one_page){
			dev_dbg(nand->dev, "kmalloc error!\n");
			return -ENOMEM;
		}

		memset(one_page, 0xFF, page_sz);
		memcpy(one_page + page_off, write_buf, tmp_len);

		ret = __snd_write(nand, to - page_off, one_page, page_sz);

		kfree(one_page);

		if(ret < 0){
			dev_dbg(nand->dev, "__snd_write error, errno = %d, offset = %lld\n", ret, to);
			return ret;
		}

		*retlen   += tmp_len;
		write_buf += tmp_len;
		to        += tmp_len;
		len       -= tmp_len;
	}

	if (len > 0){
		ret = __snd_write(nand, to, (void*)write_buf, len);
		if(ret < 0){
			dev_dbg(nand->dev, "__snd_write error, errno = %d, offset = %lld\n", ret, to);
			return ret;
		}
		*retlen += ret;
	}

	return 0;
}

static int snd_erase_block(struct spi_nand *nand, ssize_t block_id)
{
	uint8_t status= 0;

	snd_write_enable(nand);
	snd_erase_block_erase(nand, block_id);

	while (1){
		snd_read_status(nand, &status);

		if ((status & STATUS_OIP_MASK) == STATUS_READY){
			if ((status & STATUS_E_FAIL_MASK) == STATUS_E_FAIL){
				dev_dbg(nand->dev, "erase error, block=%d, errno=0x%02x\n", block_id, status);
				return -status;
			}
			break;
		}
	}
	return 0;
}

static int snd_erase(struct spi_nand *nand, loff_t offs, uint32_t len)
{
	snd_info_t *info    = nand->snd_info;
	uint32_t block_sz   = info->block_main_size;
	ssize_t  erase_cnt  = (len + info->block_main_size - 1) >> info->block_shift;
	int ret = 0;

	if((offs + len) > info->usable_size)
		len = info->usable_size - offs;

	while(erase_cnt > 0){
		if(!snd_block_isbad(nand, offs & ~info->block_mask)){
			ret = snd_erase_block(nand, offs >> info->block_shift);
			if(ret < 0)
				return ret;
		}

		offs += block_sz;
		--erase_cnt;
	}

	return 0;
}

static int snd_readoob(struct spi_nand *nand, loff_t from, struct mtd_oob_ops *ops)
{
	snd_info_t *info;
	int32_t  page_id;
	int32_t  r_len, ret;
	size_t  oob_size;

	if(!ops || ops->mode !=  MTD_OPS_PLACE_OOB || ops->oobbuf == NULL)
		return -EINVAL;


	info     = nand->snd_info;
	page_id  = from >> info->page_shift;
	oob_size = info->page_spare_size;
	r_len    = min(ops->ooblen, oob_size);

	snd_internal_ecc(nand, REG_ECC_OFF);       // disable internal ecc

	ret = snd_page_read(nand, page_id, \
		  info->page_main_size + ops->ooboffs, r_len, ops->oobbuf);

	snd_internal_ecc(nand, REG_ECC_ON);       // enable internal ecc

	if(ret < 0){
		dev_dbg(nand->dev, "spi nand read oob errno = %d.\n", ret);
		return ret;
	}

	ops->oobretlen = r_len;

	return 0;
}

static int snd_writeoob(struct spi_nand *nand, loff_t to, struct mtd_oob_ops *ops)
{
	snd_info_t *info;
	int32_t  page_id;
	int32_t  w_len, ret;
	size_t  oob_size;

	if(!ops || ops->mode !=  MTD_OPS_PLACE_OOB || ops->oobbuf == NULL)
		return -EINVAL;

	info     = nand->snd_info;
	page_id  = to >> info->page_shift;
	oob_size = info->page_spare_size;
	w_len    = min(ops->ooblen, oob_size);

	snd_internal_ecc(nand, REG_ECC_OFF);       // disable internal ecc

	ret = snd_page_program(nand, page_id, \
		  info->page_main_size + ops->ooboffs, w_len, ops->oobbuf);

	snd_internal_ecc(nand, REG_ECC_ON);       // enable internal ecc

	if(ret < 0){
		dev_dbg(nand->dev, "spi nand write oob errno = %d.\n", ret);
		return ret;
	}

	ops->oobretlen = w_len;

	return 0;
}

static int snd_is_locked(struct spi_nand *nand, loff_t ofs, uint64_t len)
{
	snd_info_t *info  = nand->snd_info;
	spi_nand_wp_t *wp = info->wp_info;
	uint8_t mode      = wp ? wp->mode : WP_BOTTOM;
	uint8_t bp_shift  = wp ? wp->bp_shift : 3;
	uint8_t bp, status;

	switch(mode){
		case WP_BOTTOM:{
			bp = WP_RANGE_DIV64;
			while(bp <= WP_RANGE_ALL){
				if((info->usable_size >> (WP_RANGE_ALL - bp)) >= ofs)
					break;
				++bp;
			}
			break;
		}
		case WP_TOP:
		case WP_BOTTOM_HIGHT:
		case WP_TOP_HIGHT:
		default:
			printk("unsupport write protect mode %d\n", mode);
			break;
	}

	snd_get_status(nand, REG_BLOCK_LOCK, &status);
	status   = (status >> bp_shift) & 0x07;
	return (bp <= status);
}

static int snd_lock(struct spi_nand *nand, loff_t ofs, uint64_t len)
{
	snd_info_t *info   = nand->snd_info;
	spi_nand_wp_t *wp  = info->wp_info;
	uint64_t nand_size = info->usable_size;
	loff_t  lock_end   = min(ofs + len, nand_size);
	snd_ops_t *ops     = &nand->snd_ops;
	uint8_t bp, mode, status, reg;
	uint8_t bp_shift, ctrl_shift;
	snd_cmd_t  cmd;

	if(wp){
		bp_shift   = wp->bp_shift;
		ctrl_shift = wp->ctrl_shift;
		mode       = wp->mode;
	}else{
		bp_shift   = 3;
		ctrl_shift = 1;
		mode       = WP_BOTTOM;
	}

	switch(mode){
		case WP_BOTTOM:{
			bp = WP_RANGE_DIV64;
			while(bp <= WP_RANGE_ALL){
				if((info->usable_size >> (WP_RANGE_ALL - bp)) >= lock_end)
					break;
				++bp;
			}
			break;
		}
		case WP_TOP:
		case WP_BOTTOM_HIGHT:
		case WP_TOP_HIGHT:
		default:
			printk("unsupport write protect mode %d\n", mode);
			break;
	}

	snd_get_status(nand, REG_BLOCK_LOCK, &reg);
 	if(bp > ((reg >> bp_shift) & 0x07)){
		status = (bp << bp_shift) | (mode << ctrl_shift);
		cmd.cmd_len  = 3;
		cmd.cmd[0]   = CMD_WRITE_REG;
		cmd.cmd[1]   = REG_BLOCK_LOCK;
		cmd.cmd[2]   = status;
		cmd.xfer_len = 0;
		ops->cmdfunc(nand, &cmd);
 	}

	return 0;
}

static int snd_unlock(struct spi_nand *nand, loff_t ofs, uint64_t len)
{
	snd_info_t *info  = nand->snd_info;
	spi_nand_wp_t *wp = info->wp_info;
	snd_ops_t *ops    = &nand->snd_ops;
	uint8_t bp, mode, status, reg;
	uint8_t bp_shift, ctrl_shift;
	snd_cmd_t cmd;

	if(wp){
		bp_shift   = wp->bp_shift;
		ctrl_shift = wp->ctrl_shift;
		mode       = wp->mode;
	}else{
		bp_shift   = 3;
		ctrl_shift = 1;
		mode       = WP_BOTTOM;
	}

	snd_get_status(nand, REG_BLOCK_LOCK, &reg);
	switch(mode){
		case WP_BOTTOM:
			bp = WP_RANGE_DIV64;
			while(bp <= WP_RANGE_ALL){
				if(ofs < (info->usable_size >> (WP_RANGE_ALL - bp))){
					--bp;
					break;
				}
				++bp;
			}
			break;
		case WP_TOP:
		case WP_BOTTOM_HIGHT:
		case WP_TOP_HIGHT:
		default:
			printk("unsupport write protect mode %d\n", mode);
			break;
	}
 	if(bp < ((reg >> bp_shift) & 0x07)){
		status = (bp << bp_shift) | (mode << ctrl_shift);
		cmd.cmd_len  = 3;
		cmd.cmd[0]   = CMD_WRITE_REG;
		cmd.cmd[1]   = REG_BLOCK_LOCK;
		cmd.cmd[2]   = status;
		cmd.xfer_len = 0;
		ops->cmdfunc(nand, &cmd);
 	}

	return 0;
}

static int snd_block_markbad(struct spi_nand *nand, loff_t ofs)
{
	snd_info_t *info = nand->snd_info;
	struct mtd_oob_ops ops;
	uint8_t mask = 0;

	ops.mode    = MTD_OPS_PLACE_OOB;
	ops.ooblen  = 1;
	ops.ooboffs = 0;
	ops.datbuf  = NULL;
	ops.oobbuf  = &mask;

	return snd_writeoob(nand, ofs & ~info->block_mask, &ops);
}

static void snd_cmdfunc(struct spi_nand *nand, snd_cmd_t *c)
{
	struct  snd_slave   *slave = nand->priv;
	struct  spi_message  message;
	struct  spi_transfer x[2] = {0};

	spi_message_init(&message);

	x[0].len    = c->cmd_len;
	x[0].tx_buf = c->cmd;
	spi_message_add_tail(x, &message);

	if(c->xfer_len > 0){
		x[1].len    = c->xfer_len;
		x[1].tx_buf = c->tx;
		x[1].rx_buf = c->rx;
		spi_message_add_tail(x+1, &message);
	}

	spi_sync(slave->spi, &message);
}

static int snd_probe(struct spi_device *spi)
{
	struct flash_platform_data	*data;
	struct snd_slave            *flash;
	struct spi_nand             *nand;
	struct mtd_part_parser_data	 ppdata;
	int   ret;

	data = dev_get_platdata(&spi->dev);

	flash = devm_kzalloc(&spi->dev, sizeof(*flash), GFP_KERNEL);
	if (!flash)
		return -ENOMEM;

	nand = &flash->spi_nand;
	memset(&nand->snd_ops, 0, sizeof(nand->snd_ops));
	/* install the hooks */
	nand->snd_ops.read          = snd_read;
	nand->snd_ops.write         = snd_write;
	nand->snd_ops.erase         = snd_erase;
	nand->snd_ops.lock          = snd_lock;
	nand->snd_ops.unlock        = snd_unlock,
	nand->snd_ops.readoob       = snd_readoob;
	nand->snd_ops.writeoob      = snd_writeoob;
	nand->snd_ops.is_locked     = snd_is_locked;
	nand->snd_ops.block_isbad   = snd_block_isbad;
	nand->snd_ops.block_markbad = snd_block_markbad;
	nand->snd_ops.cmdfunc       = snd_cmdfunc;
	nand->dev      = &spi->dev;
	nand->snd_node = spi->dev.of_node;
	nand->priv     = flash;

	spi_set_drvdata(spi, flash);
	flash->spi = spi;

	ret = spi_nand_scan(nand, NULL);
	if (ret) return ret;

	ppdata.of_node = spi->dev.of_node;

	snd_unlock(nand, 0, 128*1024*1024);

	return mtd_device_parse_register(&nand->mtd, NULL, \
		   &ppdata, data ? data->parts : NULL, data ? data->nr_parts : 0);
}

static int snd_remove(struct spi_device *spi)
{
	struct snd_slave *flash;
	struct spi_nand  *nand;

	flash = spi_get_drvdata(spi);
	if(!flash) return -1;

	nand = &flash->spi_nand;

	return mtd_device_unregister(&nand->mtd);
}

static const struct of_device_id snd_of_table[] = {
	{ .compatible = "spi-nand" }, { }
};
MODULE_DEVICE_TABLE(of, snd_of_table);

static struct spi_driver spi_nand_driver = {
	.probe	= snd_probe,
	.remove	= snd_remove,
	.driver = {
		.name	= DRIVER_NAME,
		.of_match_table = snd_of_table,
	},
};

module_spi_driver(spi_nand_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("zhuzzh");
MODULE_ALIAS("spi:"DRIVER_NAME);
