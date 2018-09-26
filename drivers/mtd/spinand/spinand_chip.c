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
#include <linux/of_gpio.h>
#include <linux/of_platform.h>
#include <linux/property.h>
#include <linux/mtd/partitions.h>
#include <linux/spi/flash.h>
#include <linux/platform_device.h>
#include <linux/jiffies.h>
#include "spinand.h"

#define DRIVER_NAME	"SPINAND"
#define PAGES_PER_BLOCK            64
#define spinand_read_page_to_cache(_chip, _pid) do{\
	struct spinand_cmd _cmd = {\
		.cmd_len = 4,\
		.cmd = {\
			CMD_READ,\
			0xFF & (_pid >> 16),\
			0xFF & (_pid >> 8 ),\
			0xFF & (_pid >> 0 ),\
		},\
		.xfer_len = 0,\
	};\
	(_chip)->cmd_func(_chip, &_cmd);\
}while(0)

#define spinand_get_status(_chip, _reg, _stat) do{\
	struct spinand_cmd _cmd = {\
		.cmd_len  = 2,\
		.cmd      = { CMD_READ_REG, _reg },\
		.xfer_len = 1,\
		.rx       = _stat,\
	};\
	(_chip)->cmd_func(_chip, &_cmd);\
}while(0)

#define spinand_set_status(_chip, _reg, _stat) do{\
	struct spinand_cmd _cmd = {\
		.cmd_len  = 3,\
		.cmd      = { CMD_WRITE_REG, _reg, _stat},\
		.xfer_len = 0,\
	};\
	(_chip)->cmd_func(_chip, &_cmd);\
}while(0)

#define spinand_write_enable(_chip) do{\
	struct spinand_cmd _cmd = {\
		.cmd_len  = 1,\
		.cmd      = { CMD_WR_ENABLE },\
		.xfer_len = 0,\
	};\
	(_chip)->cmd_func(_chip, &_cmd);\
}while(0)

#define spinand_program_data_to_cache(_chip, _id, _len, _wbuf) do{\
	struct spinand_cmd _cmd = {\
		.cmd_len  = 3,\
		.cmd = {\
			CMD_PROG_PAGE_CLRCACHE,\
			0XFF & (_id >> 8),\
			0XFF & (_id >> 0),\
		},\
		.xfer_len = _len,\
		.tx = _wbuf,\
	};\
	(_chip)->cmd_func(_chip, &_cmd);\
}while(0)

#define spinand_program_execute(_chip, _id) do{\
	struct spinand_cmd _cmd = {\
		.cmd_len  = 4,\
		.cmd = {\
			CMD_PROG_PAGE_EXC,\
			0XFF & (_id >> 16),\
			0XFF & (_id >> 8 ),\
			0XFF & (_id >> 0 ),\
		},\
		.xfer_len = 0,\
	};\
	(_chip)->cmd_func(_chip, &_cmd);\
}while(0)

#define spinand_erase_block_erase(_chip, _blkid) do{\
	u32 _row = (_blkid) << 6;\
	struct spinand_cmd _cmd = {\
		.cmd_len  = 4,\
		.cmd      = {\
			CMD_ERASE_BLK,\
			0xFF & (_row >> 16),\
			0xFF & (_row >> 8 ),\
			0xFF & (_row >> 0 ),\
		},\
		.xfer_len = 0,\
	};\
	(_chip)->cmd_func(_chip, &_cmd);\
}while(0)

#define spinand_read_id(_chip, _id) do{\
	struct spinand_cmd cmd = {\
		.cmd_len = 2,\
		.cmd[0] = CMD_READ_ID,\
		.cmd[1] = 0,\
		.xfer_len = 2,\
		.rx = (u8*)_id,\
	};\
	_chip->cmd_func(_chip, &cmd);\
}while(0)

static inline int spinand_read_from_cache(struct spinand_chip *chip, u16 id, u16 len, u8* rbuf)
{
	struct spinand_cmd cmd = {
		.cmd_len  = 4,
		.cmd      = {
			CMD_READ_RDM,\
			0xFF & (id >> 8),
			0xFF & (id >> 0),
			~0, 0,
		},
		.xfer_len = len,
		.rx       = rbuf,
	};
	return (chip)->cmd_func(chip, &cmd);
}

static void spinand_hwecc(struct spinand_chip *chip, bool enable)
{
	uint8_t status;

	spinand_get_status(chip, REG_OTP, &status);

	status &= ~REG_ECC_MASK;
	status |= (!!enable) << 4;

	spinand_set_status(chip, REG_OTP, status);
}

static int spinand_reset(struct spinand_chip *chip)
{
	struct spinand_cmd cmd = {
		.cmd_len = 1,
		.cmd = {CMD_RESET, 0},
		.xfer_len = 0,
	};

	return chip->cmd_func(chip, &cmd);
}

static inline int spinand_wait_ready(struct spinand_chip *chip, uint8_t *status)
{
	unsigned long timeo = jiffies;

	timeo += msecs_to_jiffies(100);

	while (1) {
		spinand_get_status(chip, REG_STATUS, status);
		if((*status & STATUS_OIP_MASK) == STATUS_READY)
		    return 0;

		if(time_after(jiffies, timeo))
		    break;

		cond_resched();
	}

	return -ETIMEDOUT;
}

static int spinand_generic_read_page(struct spinand_chip *chip, u32 page_id, u16 offs, u16 len, u8* rbuf, unsigned int *corrected)
{
	uint8_t status;
	int ret, err;

	spinand_read_page_to_cache(chip, page_id);
	ret = spinand_wait_ready(chip, &status);

	if (ret != 0){
	    return ret;
	}

	if(likely(chip->get_ecc_status != NULL)){
		ret = chip->get_ecc_status(status, corrected);
		if (ret != 0){
			dev_dbg(&chip->spi->dev, "spi nand page read error, "
				"status = 0x%02x, page_id = %d\n", status, page_id);
		}
	}

	err = spinand_read_from_cache(chip, offs, len, rbuf);
	if(err != 0)
		return err;

	return ret;
}

static int spinand_generic_program_page(struct spinand_chip *chip, u32 page_id, u16 offs, u16 len, u8* wbuf)
{
	uint8_t status;
	int ret;

	spinand_write_enable(chip);
	spinand_program_data_to_cache(chip, offs, len, wbuf);
	spinand_program_execute(chip, page_id);
	ret = spinand_wait_ready(chip, &status);

	if (ret != 0){
	    return ret;
	}

	if(STATUS_P_FAIL == (status & STATUS_P_FAIL_MASK)){
		dev_dbg(&chip->spi->dev, "spi nand write error, "
			"status = 0x%x, page_id = %d\n", status, page_id);
		return -status;
	}

	return 0;
}

static int spinand_ds25q2g_read_page(struct spinand_chip *chip, u32 page_id, u16 offs, u16 len, u8* rbuf, unsigned int *corrected)
{
	uint8_t status;
	int ret, err;

	//针对Dosilicon的DS35Q2GA做特殊处理,DS35Q2GA只有一个die,每个die有两个plane.
	//当block number为奇数,选择另外一个plane.
	if((page_id / PAGES_PER_BLOCK) % 2){
		offs |= (0x1 << 12);
	}

	spinand_read_page_to_cache(chip, page_id);
	ret = spinand_wait_ready(chip, &status);

	if (ret != 0){
	    return ret;
	}

	if(likely(chip->get_ecc_status != NULL)){
		ret = chip->get_ecc_status(status, corrected);
		if (ret != 0){
			dev_dbg(&chip->spi->dev, "spi nand page read error, "
				"status = 0x%02x, page_id = %d\n", status, page_id);
		}
	}

	err = spinand_read_from_cache(chip, offs, len, rbuf);
	if(err != 0)
		return err;

	return ret;
}

static int spinand_ds25q2g_program_page(struct spinand_chip *chip, u32 page_id, u16 offs, u16 len, u8* wbuf)
{
	uint8_t status;
	int ret;

	//针对Dosilicon的DS35Q2GA做特殊处理,DS35Q2GA只有一个die,每个die有两个plane.
	//当block number为奇数,选择另外一个plane.
	if((page_id / PAGES_PER_BLOCK) % 2){
		offs |= (0x1 << 12);
	}

	spinand_write_enable(chip);
	spinand_program_data_to_cache(chip, offs, len, wbuf);
	spinand_program_execute(chip, page_id);
	ret = spinand_wait_ready(chip, &status);

	if (ret != 0){
	    return ret;
	}

	if(STATUS_P_FAIL == (status & STATUS_P_FAIL_MASK)){
		dev_dbg(&chip->spi->dev, "spi nand write error, "
			"status = 0x%x, page_id = %d\n", status, page_id);
		return -status;
	}

	return 0;
}

static int spinand_erase_block(struct spinand_chip *chip, u32 block_id)
{
	uint8_t status;
	int ret;

	spinand_write_enable(chip);
	spinand_erase_block_erase(chip, block_id);
	ret = spinand_wait_ready(chip, &status);

	if (ret != 0){
	    return ret;
	}

	if ((status & STATUS_E_FAIL_MASK) == STATUS_E_FAIL){
		dev_dbg(&chip->spi->dev, "erase error, "
			"block=%d, status = 0x%02x\n", block_id, status);
		return -status;
	}

	return 0;
}

static int spinand_cmd_func(struct spinand_chip *chip, struct spinand_cmd *cmd)
{
	struct spi_message message;
	struct spi_transfer x[2] = {0};

	spi_message_init(&message);

	x[0].len = cmd->cmd_len;
	x[0].tx_buf = cmd->cmd;
	spi_message_add_tail(x, &message);

	if(cmd->xfer_len > 0){
		x[1].len = cmd->xfer_len;
		x[1].tx_buf = cmd->tx;
		x[1].rx_buf = cmd->rx;
		spi_message_add_tail(x+1, &message);
	}

	return spi_sync(chip->spi, &message);
}

static int spinand_probe(struct spi_device *spi)
{
	struct spinand_chip *chip;
	uint16_t id;

	chip = devm_kzalloc(&spi->dev, sizeof(*chip), GFP_KERNEL);
	if(!chip)
		return -ENOMEM;

	chip->spi = spi;
	chip->reset = spinand_reset;
	chip->erase_block = spinand_erase_block;
	chip->cmd_func = spinand_cmd_func;
	chip->hwecc = spinand_hwecc;

	spinand_read_id(chip, &id);

	if(id == 0x72e5){
		chip->read_page    = spinand_ds25q2g_read_page;
		chip->program_page = spinand_ds25q2g_program_page;
	}else{
		chip->read_page    = spinand_generic_read_page;
		chip->program_page = spinand_generic_program_page;
	}

	spi_set_drvdata(spi, chip);
	return spinand_mtd_register(chip, id);
}

static int spinand_remove(struct spi_device *spi)
{
	struct spinand_chip *chip;

	chip = spi_get_drvdata(spi);

	spinand_mtd_unregister(chip);
	devm_kfree(&spi->dev, chip);
	return 0;
}

static const struct of_device_id spinand_of_table[] = {
	{ .compatible = "spinand" }, { }
};
MODULE_DEVICE_TABLE(of, spinand_of_table);

static struct spi_driver spi_nand_driver = {
	.probe = spinand_probe,
	.remove = spinand_remove,
	.driver = {
		.name = DRIVER_NAME,
		.of_match_table = spinand_of_table,
	},
};

module_spi_driver(spi_nand_driver);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("nationalchip");
MODULE_ALIAS("spi:"DRIVER_NAME);
