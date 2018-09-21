#include <linux/dma-mapping.h>
#include <linux/dmaengine.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/spi/spi.h>
#include <linux/types.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include "spi-dw.h"
#include <linux/pci.h>
#include <linux/platform_data/dma-dw.h>

#define RX_BUSY		0
#define TX_BUSY		1

static struct dw_dma_slave spi_dw_dma_tx = {
	.src_id = 0,
	.dst_id = 0,
	.src_master = 0,
	.dst_master = 1,
};

static struct dw_dma_slave spi_dw_dma_rx = {
	.src_id = 1,
	.dst_id = 1,
	.src_master = 1,
	.dst_master = 0,
};

static bool spi_dw_dma_chan_filter(struct dma_chan *chan, void *param)
{
	struct dw_dma_slave *s = param;
	struct device_node *np = chan->device->dev->of_node;

	if(s->dma_ctrl_name){
		if(strcmp(s->dma_ctrl_name, np->name))
			return false;
	}

	if(chan->chan_id != s->src_id)
		return false;

	s->dma_dev = chan->device->dev;
	chan->private = s;
	return true;
}

static int spi_dw_dma_init(struct dw_spi *dws)
{
	struct dw_dma_slave *txs = &spi_dw_dma_tx;
	struct dw_dma_slave *rxs = &spi_dw_dma_rx;
	dma_cap_mask_t mask;

	dma_cap_zero(mask);
	dma_cap_set(DMA_SLAVE, mask);

	if(of_property_read_string(dws->master->dev.of_node,
			"dma_ctrl_name", &txs->dma_ctrl_name))
		txs->dma_ctrl_name = NULL;
	rxs->dma_ctrl_name = txs->dma_ctrl_name;

	/* 1. Init tx channel */
	dws->txchan = dma_request_channel(mask, spi_dw_dma_chan_filter, txs);
	if (!dws->txchan)
		goto err_exit;

	/* 2. Init rx channel */
	dws->rxchan = dma_request_channel(mask, spi_dw_dma_chan_filter, rxs);
	if (!dws->rxchan)
		goto free_txchan;

	dws->master->dma_tx = dws->txchan;
	dws->master->dma_rx = dws->rxchan;
	dws->dma_inited = 1;

	dev_dbg(&dws->master->dev,"tx chan %s, rx chan %s\n",
			dev_name(&dws->txchan->dev->device),
			dev_name(&dws->rxchan->dev->device));

	return 0;

free_txchan:
	dma_release_channel(dws->txchan);
err_exit:
	return -EBUSY;
}

static void spi_dw_dma_exit(struct dw_spi *dws)
{
	if (!dws->dma_inited)
		return;

	dmaengine_terminate_all(dws->txchan);
	dma_release_channel(dws->txchan);

	dmaengine_terminate_all(dws->rxchan);
	dma_release_channel(dws->rxchan);
}

static irqreturn_t dma_transfer(struct dw_spi *dws)
{
	u16 irq_status = dw_readl(dws, DW_SPI_ISR);

	if (!irq_status)
		return IRQ_NONE;

	dw_readl(dws, DW_SPI_ICR);
	spi_reset_chip(dws);

	dev_err(&dws->master->dev, "%s: FIFO overrun/underrun, "
			"irq_status = 0x%x\n", __func__, irq_status);
	dws->master->cur_msg->status = -EIO;
	spi_finalize_current_transfer(dws->master);
	return IRQ_HANDLED;
}

static bool spi_dw_can_dma(struct spi_master *master, struct spi_device *spi,
		struct spi_transfer *xfer)
{
	struct dw_spi *dws = spi_master_get_devdata(master);

	if (!dws->dma_inited)
		return false;

	/* 当xfer->len > fifo_len * 32 才使用dma */
	return xfer->len > (dws->tx_fifo_len << 5);
}

/*
 * dws->dma_chan_busy is set before the dma transfer starts, callback for tx
 * channel will clear a corresponding bit.
 */
static void dw_spi_dma_tx_done(void *arg)
{
	struct dw_spi *dws = arg;

	clear_bit(TX_BUSY, &dws->dma_chan_busy);
	if (test_bit(RX_BUSY, &dws->dma_chan_busy))
		return;
	spi_finalize_current_transfer(dws->master);

}

static struct dma_async_tx_descriptor *dw_spi_dma_prepare_tx(
			struct dw_spi *dws, struct spi_transfer *xfer)
{
	struct dma_slave_config txconf;
	struct dma_async_tx_descriptor *txdesc;
	struct sg_table sg;
	int ret;

	memcpy(&sg, xfer->rx_sg.sgl ? &xfer->rx_sg : &xfer->tx_sg, sizeof(sg));

	txconf.direction = DMA_MEM_TO_DEV;
	txconf.dst_addr = dws->dma_addr;
	txconf.dst_maxburst = 4;
	txconf.src_maxburst = 32;
	txconf.src_addr_width = DMA_SLAVE_BUSWIDTH_1_BYTE;
	txconf.dst_addr_width = DMA_SLAVE_BUSWIDTH_1_BYTE;
	txconf.device_fc = false;

	ret = dmaengine_slave_config(dws->txchan, &txconf);
	if(ret < 0)
		return NULL;

	txdesc = dmaengine_prep_slave_sg(dws->txchan,
				sg.sgl, sg.nents,
				DMA_MEM_TO_DEV,
				DMA_PREP_INTERRUPT | DMA_CTRL_ACK);
	if (!txdesc)
		return NULL;

	txdesc->callback = dw_spi_dma_tx_done;
	txdesc->callback_param = dws;

	return txdesc;
}

/*
 * dws->dma_chan_busy is set before the dma transfer starts, callback for rx
 * channel will clear a corresponding bit.
 */
static void dw_spi_dma_rx_done(void *arg)
{
	struct dw_spi *dws = arg;

	clear_bit(RX_BUSY, &dws->dma_chan_busy);
	if (test_bit(TX_BUSY, &dws->dma_chan_busy))
		return;
	spi_finalize_current_transfer(dws->master);
}

static struct dma_async_tx_descriptor *dw_spi_dma_prepare_rx(struct dw_spi *dws,
		struct spi_transfer *xfer)
{
	struct dma_slave_config rxconf;
	struct dma_async_tx_descriptor *rxdesc;
	struct sg_table sg;
	int ret;

	if (!xfer->rx_buf)
		return NULL;

	memcpy(&sg, xfer->rx_sg.sgl ? &xfer->rx_sg : &xfer->tx_sg, sizeof(sg));


	rxconf.direction = DMA_DEV_TO_MEM;
	rxconf.src_addr = dws->dma_addr;
	rxconf.src_maxburst = 8;
	rxconf.dst_maxburst = 32;
	rxconf.dst_addr_width = DMA_SLAVE_BUSWIDTH_1_BYTE;
	rxconf.src_addr_width = DMA_SLAVE_BUSWIDTH_1_BYTE;
	rxconf.device_fc = false;

	ret = dmaengine_slave_config(dws->rxchan, &rxconf);
	if(ret < 0)
		return NULL;

	rxdesc = dmaengine_prep_slave_sg(dws->rxchan,
				sg.sgl, sg.nents,
				DMA_DEV_TO_MEM,
				DMA_PREP_INTERRUPT | DMA_CTRL_ACK);
	if (!rxdesc)
		return NULL;

	rxdesc->callback = dw_spi_dma_rx_done;
	rxdesc->callback_param = dws;

	return rxdesc;
}

static int spi_dw_dma_setup(struct dw_spi *dws, struct spi_transfer *xfer)
{
	u32 cr0, dma_ctrl = SPI_DMA_TDMAE;

	/* 必须先将spi DMA Control Register 先禁用在开启, 否则传输数据会出错 */
	dw_writel(dws, DW_SPI_DMACR, 0);

	cr0 = dw_readl(dws, DW_SPI_CTRL0) & (~SPI_TMOD_MASK);
	cr0 |= SPI_TMOD_TO << SPI_TMOD_OFFSET;

	dw_writel(dws, DW_SPI_DMARDLR, dws->rx_fifo_len / 2 -1);
	dw_writel(dws, DW_SPI_DMATDLR, dws->tx_fifo_len / 2);

	if(xfer->rx_buf){
		cr0 &= ~SPI_TMOD_MASK;
		dma_ctrl |= SPI_DMA_RDMAE;
	}

	dw_writel(dws, DW_SPI_DMACR, dma_ctrl);
	dw_writel(dws, DW_SPI_CTRL0, cr0);

	/* Set the interrupt mask */
	if(!dws->poll_mode){
		spi_umask_intr(dws, SPI_INT_TXOI | SPI_INT_RXUI | SPI_INT_RXOI);
		dws->transfer_handler = dma_transfer;
	}

	return 0;
}

static int spi_dw_dma_transfer(struct dw_spi *dws, struct spi_transfer *xfer)
{
	struct dma_async_tx_descriptor *txdesc, *rxdesc;

	txdesc = dw_spi_dma_prepare_tx(dws, xfer);
	if(!txdesc)
		return -1;

	rxdesc = dw_spi_dma_prepare_rx(dws, xfer);
	if(!rxdesc && xfer->rx_buf)
		return -1;

	set_bit(TX_BUSY, &dws->dma_chan_busy);
	dmaengine_submit(txdesc);

	if(xfer->rx_buf){
		set_bit(RX_BUSY, &dws->dma_chan_busy);
		dmaengine_submit(rxdesc);
		dma_async_issue_pending(dws->rxchan);
	}

	dma_async_issue_pending(dws->txchan);

	return 0;
}

static void spi_dw_dma_stop(struct dw_spi *dws)
{
	if (test_bit(TX_BUSY, &dws->dma_chan_busy)) {
		dmaengine_terminate_all(dws->txchan);
		clear_bit(TX_BUSY, &dws->dma_chan_busy);
	}
	if (test_bit(RX_BUSY, &dws->dma_chan_busy)) {
		dmaengine_terminate_all(dws->rxchan);
		clear_bit(RX_BUSY, &dws->dma_chan_busy);
	}
}

static struct dw_spi_dma_ops spi_dw_dma_ops = {
	.dma_init = spi_dw_dma_init,
	.dma_exit = spi_dw_dma_exit,
	.dma_setup = spi_dw_dma_setup,
	.can_dma = spi_dw_can_dma,
	.dma_transfer = spi_dw_dma_transfer,
	.dma_stop = spi_dw_dma_stop,
};

int dw_spi_dma_init(struct dw_spi *dws)
{
	dws->dma_tx = &spi_dw_dma_tx;
	dws->dma_rx = &spi_dw_dma_rx;
	dws->dma_ops = &spi_dw_dma_ops;
	return 0;
}
EXPORT_SYMBOL_GPL(dw_spi_dma_init);
