/*
 *
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation;
 * either version 2 of the License, or (at your option) any
 * later version.
 */

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/of_irq.h>
#include <linux/prefetch.h>
#include <linux/usb/ch9.h>
#include <linux/usb/gadget.h>
 
#define USB_BASE_ADDR_OFFSET    0x0

#define EP0MAXPACK_REG_ADDR		USB_BASE_ADDR_OFFSET
#define EP0CS_REG_ADDR			(USB_BASE_ADDR_OFFSET+0x02)

#define UDC_EP_OUT1MAXPACK_REG_ADDR	(USB_BASE_ADDR_OFFSET+0x08)
#define UDC_EP_OUT1CON_REG_ADDR		(USB_BASE_ADDR_OFFSET+ 0x0A)
#define UDC_EP_OUT1CS_REG_ADDR		   	(USB_BASE_ADDR_OFFSET+ 0x0B)

#define UDC_EP_IN1MAXPACK_REG_ADDR	(UDC_EP_OUT1MAXPACK_REG_ADDR+0x4)
#define UDC_EP_IN1CON_REG_ADDR		(UDC_EP_OUT1CON_REG_ADDR+0x4)
#define UDC_EP_IN1CS_REG_ADDR		(UDC_EP_OUT1CS_REG_ADDR+0x4)

#define SETUP_PACKET_REG_ADDR  	(USB_BASE_ADDR_OFFSET+0x180)

#define UDC_EP_INIRQ_REG_ADDR		(USB_BASE_ADDR_OFFSET + 0x188)
#define PING_USB_IRQ_REG_ADDR			(USB_BASE_ADDR_OFFSET + 0x18C)

#define UDC_INIEN_REG_ADDR		(USB_BASE_ADDR_OFFSET+0x194)
#define UDC_OUTIEN_REG_ADDR			(UDC_INIEN_REG_ADDR+2)

#define EP0DMAADDR_REG_ADDR		(USB_BASE_ADDR_OFFSET+0x400)

#define UDC_EP_OUT1DMAADDR_REG_ADDR	(USB_BASE_ADDR_OFFSET+0x420)
#define UDC_EP_IN1DMAADDR_REG_ADDR	(USB_BASE_ADDR_OFFSET+0x430)

#define EP0FNADDR_REG_ADDR 	   	(USB_BASE_ADDR_OFFSET+0x200)
#define HC_IN1FNADDR_REG_ADDR 	(USB_BASE_ADDR_OFFSET+0x210)
#define HC_OUT1FNADDR_REG_ADDR 	(USB_BASE_ADDR_OFFSET+0x218)

#define UDC_USB_CTRL_REG_ADDR	(USB_BASE_ADDR_OFFSET+0x1A0)


#define UDC_FNADDR_REG_ADDR		(USB_BASE_ADDR_OFFSET+0x1A6)
#define UDC_FN_CTRL_REG_ADDR	(USB_BASE_ADDR_OFFSET+0x1A4)

#define HC_PORT_CTRL_REG_ADDR	(USB_BASE_ADDR_OFFSET+0x1A8)


#define TAAIDLBDIS		   			(USB_BASE_ADDR_OFFSET+0x1c1) 
#define UDC_DMASTART	  		(USB_BASE_ADDR_OFFSET+0x1CC)
#define UDC_DMASTOP	  				(USB_BASE_ADDR_OFFSET+0x1D0)
#define UDC_DMARESET	  			(USB_BASE_ADDR_OFFSET+0x1D4)

#define USBCS_REG_ADDR   			(USB_BASE_ADDR_OFFSET+0x1A3)
#define HC_PORTCTRL_REG_ADDR   		(USB_BASE_ADDR_OFFSET+0x1AB)

#define UDC_DMA_OUTIRQ_REG_ADDR  	(USB_BASE_ADDR_OFFSET+0x192)
#define UDC_DMA_INIRQ_REG_ADDR 		(USB_BASE_ADDR_OFFSET+0x190)


#define UDC_PING_USB_IEN_REG_ADDR 		(USB_BASE_ADDR_OFFSET+0x198)


#define UDC_DMA_OUTIEN_REG_ADDR  	(USB_BASE_ADDR_OFFSET+0x19E)
#define UDC_DMA_INIEN_REG_ADDR 		(USB_BASE_ADDR_OFFSET+0x19C)

#define UDC_TOGSET_REG_ADDR 		(USB_BASE_ADDR_OFFSET+0x1D8)
#define UDC_TOGRESET_REG_ADDR 		(USB_BASE_ADDR_OFFSET+0x1DC)

//Endpoint FIFO size
#define UDC_OCM_FIFO_SIZE_REG_ADDR 		    (USB_BASE_ADDR_OFFSET+0x300)
#define UDC_OCM_FIFO_ALLOC_REG_ADDR 		(USB_BASE_ADDR_OFFSET+0x304)

#define UDC_OTG_REG_ADDR 		(USB_BASE_ADDR_OFFSET+0x1BC)

#define GXSCPU_REG_BASE_AOUT_IRQ    0x0030a274


/* USB device specific global configuration constants.*/
#define EVAUSB_MAX_ENDPOINTS		16	/* Maximum End Points */
#define EVAUSB_EP_NUMBER_ZERO		0	/* End point Zero */

/* Phase States */
#define SETUP_PHASE			0x0000	/* Setup Phase */
#define DATA_PHASE			0x0001  /* Data Phase */
#define STATUS_PHASE		0x0002  /* Status Phase */

#define EP0_MAX_PACKET		64 /* Endpoint 0 maximum packet length */
#define STATUSBUFF_SIZE		2  /* Buffer size for GET_STATUS command */
#define EPNAME_SIZE			4  /* Buffer size for endpoint name */

/* Endpoint Configuration Status Register */
#define EVAUSB_EP0_CFG_HSNAK_MASK		0x20000 /* Endpoint0 HSNAK bit */
#define EVAUSB_EP0_CFG_STALL_MASK		0x10000 /* Endpoint0 Stall bit */

#define EVAUSB_EP_CFG_VALID_MASK		0x800000 /* Endpoint Valid bit */
#define EVAUSB_EP_CFG_STALL_MASK		0x400000 /* Endpoint Stall bit */

#define EVAUSB_CONTROL_USB_READY_MASK     0x40000000
#define EVAUSB_CONTROL_USB_RMTWAKE_MASK   0x20000000

/* Interrupt register related masks.*/
#define EVAUSB_STATUS_PING_ALL_OUTEPIEIRQ_MASK	0xFFFF0000 /* for test use */
#define EVAUSB_STATUS_OVERFLOWIEIRQ_MASK	    0x00000040
#define EVAUSB_STATUS_HSPEEDIEIRQ_MASK	        0x00000020
#define EVAUSB_STATUS_URESIEIRQ_MASK	        0x00000010
#define EVAUSB_STATUS_SUSPIEIRQ_MASK	        0x00000008 
#define EVAUSB_STATUS_SUTOKEIEIRQ_MASK	        0x00000004 
#define EVAUSB_STATUS_SOFIEIRQ_MASK	            0x00000002 
#define EVAUSB_STATUS_SUDAVIEIRQ_MASK	        0x00000001 

#define EVAUSB_STATUS_EP1_OUT_IRQ_MASK		0x00020000 
#define EVAUSB_STATUS_EP1_IN_IRQ_MASK	    0x00000002 
#define EVAUSB_STATUS_EP0_OUT_IRQ_MASK		0x00010000 
#define EVAUSB_STATUS_EP0_IN_IRQ_MASK	    0x00000001

//register dmactrl
#define REG_DMACTRL_SEND_0_LEN 		0x80000		/*send short packet*/
#define REG_DMACTRL_START_DMA  		0x40000		/*start dma */
#define REG_DMACTRL_STOP_DMA   		0x20000		/*stop dma*/
#define REG_DMACTRL_BIG_ENDIAN 		0x10000  	/*Read/write - Endianess of AHB master interface*/
#define REG_DMACTRL_HSIZE_32   		0x8000000  	/*Read/write - AHB transfer size*/
#define REG_DMACTRL_HPROT_0   		0x10000000 


/* container_of helper macros */
#define to_udc(g)	 container_of((g), struct evausb_udc, gadget)
#define to_evausb_ep(ep)	 container_of((ep), struct evausb_ep, ep_usb)
#define to_evausb_req(req) container_of((req), struct evausb_req, usb_req)

#define bEN_WU_SET_EXTERAN_INT        (4)
#define bEN_USB_SET_EXTERAN_INT        (5)
#define bEN_DMA_SET_EXTERAN_INT        (6)
#define bEN_DUMP_SET_EXTERAN_INT      (7)

/**
 * struct xusb_req - Xilinx USB device request structure
 * @usb_req: Linux usb request structure
 * @queue: usb device request queue
 * @ep: pointer to xusb_endpoint structure
 */
struct evausb_req {
	struct usb_request usb_req;
	struct list_head queue;
	struct evausb_ep *ep;
	void *saved_req_buf;
	u32 real_dma_length;
};

/*	
*	hc_ep - describe one in or out endpoint in host mode
*/
struct eva_ep_dma_struct
{  
	u32		dmaaddr;
	u32		dmatsize;
	u32		dmaringptr;
	u32		dmactrl;
};

/*
*   struct ep_irq - this structur contain pointer at registers tied with interrupt 
*					for endpoints
*/
struct eva_usb_irq
{
	u16 		outirq;
	u16 		inirq;
	u8		usbirq;
	u8		reserved;
	u16     	outpngirq;
	u16 		outdmairq;
	u16 		indmairq;

	u16		outien;
	u16 		inien;
	u8		usbien;
	u8 		reserved2;
	u16     	outpngien;

	u16 		outdmaien;
	u16 		indmaien;
	u8		usbivect;
	u8		dmaivect;
};


/**
 * struct evausb_ep - USB end point structure.
 * @ep_usb: usb endpoint instance
 * @queue: endpoint message queue
 * @udc: xilinx usb peripheral driver instance pointer
 * @desc: pointer to the usb endpoint descriptor
 * @rambase: the endpoint buffer address
 * @offset: the endpoint register offset value
 * @name: name of the endpoint
 * @epnumber: endpoint number
 * @maxpacket: maximum packet size the endpoint can store
 * @buffer0count: the size of the packet recieved in the first buffer
 * @buffer1count: the size of the packet received in the second buffer
 * @curbufnum: current buffer of endpoint that will be processed next
 * @buffer0ready: the busy state of first buffer
 * @buffer1ready: the busy state of second buffer
 * @is_in: endpoint direction (IN or OUT)
 * @is_iso: endpoint type(isochronous or non isochronous)
 */
struct evausb_ep {
	struct usb_ep ep_usb;
	struct list_head queue;
	struct evausb_udc *udc;
	const struct usb_endpoint_descriptor *desc;
	u32  rambase;
	u32  offset;
	char name[8];
	u16  epnumber;
	u16  maxpacket;
	volatile struct eva_ep_dma_struct *dma;
	bool is_in;
	bool is_iso;
	bool is_int;

};

struct general_dma
{
	u16 dmastart_in;
	u16 dmastart_out;

	u16 dmastop_in;
	u16 dmastop_out;

	u16 dmasrst_in;
	u16 dmasrst_out;

	u16 togsetq_in;	
	u16 togsetq_out;

	u16 togrst_in;
	u16 togrst_out;
	
};

struct ck_a7_irq_reg
{
	unsigned int INT_STATUS;
	unsigned int INT_CLEAR;
	unsigned int INT_ENABLE;
};


/**
 * struct evausb_udc -  USB peripheral driver structure
 * @gadget: USB gadget driver instance
 * @ep: an array of endpoint structures
 * @driver: pointer to the usb gadget driver instance
 * @setup: usb_ctrlrequest structure for control requests
 * @req: pointer to dummy request for get status command
 * @dev: pointer to device structure in gadget
 * @usb_state: device in suspended state or not
 * @remote_wkp: remote wakeup enabled by host
 * @setupseqtx: tx status
 * @setupseqrx: rx status
 * @addr: the usb device base address
 * @lock: instance of spinlock
 * @dma_enabled: flag indicating whether the dma is included in the system
 * @read_fn: function pointer to read device registers
 * @write_fn: function pointer to write to device registers
 */
struct evausb_udc {
	struct usb_gadget gadget;
	struct evausb_ep ep0;
	struct evausb_ep ep_in[EVAUSB_MAX_ENDPOINTS];
	struct evausb_ep ep_out[EVAUSB_MAX_ENDPOINTS];
	struct usb_gadget_driver *driver;
	struct usb_ctrlrequest setup;
	struct evausb_req *req;
	struct device *dev;
	u32 usb_state;
	u32 remote_wkp;
	u32 setupseqtx;
	u32 setupseqrx;
	void __iomem *addr;
	spinlock_t lock;
	bool dma_enabled;
	volatile u8  * fnaddr;
	volatile u8  * usbcs;
	volatile struct general_dma * dma;
	volatile struct eva_usb_irq * usb_irq;

	unsigned int (*read_fn)(void __iomem *);
	void (*write_fn)(void __iomem *, u32, u32);
};

static const char driver_name[] = "gxeva-udc";
static const char ep0name[] = "ep0";

/* Control endpoint configuration.*/
static const struct usb_endpoint_descriptor config_bulk_out_desc = {
	.bLength		= USB_DT_ENDPOINT_SIZE,
	.bDescriptorType	= USB_DT_ENDPOINT,
	.bEndpointAddress	= USB_DIR_OUT,
	.bmAttributes		= USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize		= cpu_to_le16(EP0_MAX_PACKET),
};

/**
 * evaudc_write32 - little endian write to device registers
 * @addr: base addr of device registers
 * @offset: register offset
 * @val: data to be written
 */
static void evaudc_write32(void __iomem *addr, u32 offset, u32 val)
{
	iowrite32(val, addr + offset);
}

/**
 * evaudc_read32 - little endian read from device registers
 * @addr: addr of device register
 * Return: value at addr
 */
static unsigned int evaudc_read32(void __iomem *addr)
{
	return ioread32(addr);
}

/**
 * evaudc_write32_be - big endian write to device registers
 * @addr: base addr of device registers
 * @offset: register offset
 * @val: data to be written
 */
static void evaudc_write32_be(void __iomem *addr, u32 offset, u32 val)
{
	iowrite32be(val, addr + offset);
}

/**
 * evaudc_read32_be - big endian read from device registers
 * @addr: addr of device register
 * Return: value at addr
 */
static unsigned int evaudc_read32_be(void __iomem *addr)
{
	return ioread32be(addr);
}

/**
 * evaudc_wrstatus - Sets up the usb device status stages.
 * @udc: pointer to the usb device controller structure.
 */
static void evaudc_wrstatus(struct evausb_udc *udc)
{
	/* ep0 only */
	u32 epcfgreg;
	struct evausb_ep *ep0 = &udc->ep0;

	epcfgreg = udc->read_fn(udc->addr + ep0->offset);
	epcfgreg |= EVAUSB_EP0_CFG_HSNAK_MASK;
	udc->write_fn(udc->addr, ep0->offset, epcfgreg);

}

/**
 * evaudc_epconfig - Configures the given endpoint.
 * @ep: pointer to the usb device endpoint structure.
 * @udc: pointer to the usb peripheral controller structure.
 *
 * This function configures a specific endpoint with the given configuration
 * data.
 */
static void evaudc_epconfig(struct evausb_ep *ep, struct evausb_udc *udc)
{
	u32 epcfgreg;
	epcfgreg = 0;

	if (ep->epnumber == 0)
	{
		udc->write_fn(udc->addr, ep->offset, ep->ep_usb.maxpacket);
		epcfgreg = udc->read_fn(udc->addr + ep->offset);
	}
	else
	{
		if (ep->is_iso) {
			epcfgreg = (0x5<<18)|(ep->ep_usb.maxpacket);
		} else if (ep->is_int) {
			epcfgreg = (0x3<<18)|(ep->ep_usb.maxpacket);
		} else { 
			epcfgreg = (0x2<<18)|(ep->ep_usb.maxpacket);
		}

		udc->write_fn(udc->addr, ep->offset, epcfgreg);
	}
}

static void evaudc_handle_unaligned_buf_complete(struct evausb_udc *udc,
				       struct evausb_ep *ep, struct evausb_req *req)
{
	/* If dma is not being used or buffer was aligned */
	if (!udc->dma_enabled|| !req->saved_req_buf)
		return;

	/* Copy data from bounce buffer on successful out transfer */
	if (!ep->is_in && !req->usb_req.status)
	{
		memcpy(req->saved_req_buf, req->usb_req.buf,
							req->usb_req.actual);
	}

	/* Free bounce buffer */
	kfree(req->usb_req.buf);

	req->usb_req.buf = req->saved_req_buf;
	req->saved_req_buf = NULL;
}


static int evaudc_handle_unaligned_buf_start(struct evausb_udc *udc,
				       struct evausb_ep *ep, struct evausb_req *req)
{
	void *req_buf = req->usb_req.buf;

	/* If dma is not being used or buffer is aligned */
	if (!udc->dma_enabled || !((long)req_buf & 3))
		return 0;

	WARN_ON(req->saved_req_buf);

	req->usb_req.buf = kmalloc(req->usb_req.length, GFP_ATOMIC);
	if (!req->usb_req.buf) {
		req->usb_req.buf = req_buf;
		printk("%s: unable to allocate memory for bounce buffer\n",__FUNCTION__);
		return -ENOMEM;
	}

	/* Save actual buffer */
	req->saved_req_buf = req_buf;

	if (ep->is_in)
	{
		memcpy(req->usb_req.buf, req_buf, req->usb_req.length);
	}
	return 0;
}

/**
 * evaudc_start_dma - Starts DMA transfer.
 * @ep: pointer to the usb device endpoint structure.
 * @src: DMA source address.
 * @dst: DMA destination address.
 * @length: number of bytes to transfer.
 *
 * Return: 0 on success, error code on failure
 *
 * This function starts DMA transfer by writing to DMA source,
 * destination and lenth registers.
 */
static int evaudc_start_dma(struct evausb_ep *ep, dma_addr_t addr,u32 length,bool zero)
{
	struct evausb_udc *udc = ep->udc;
	int rc = 0;
	u32 timeout = 500;
	u32 dma_intrstatus;
	//u32 reg_value;

	/*
	 * Set the addresses in the DMA source and
	 * destination registers and then set the length
	 * into the DMA length register.
	 */
	if (ep->is_in)
		udc->dma->dmasrst_in |= 1<<ep->epnumber;
	else
		udc->dma->dmasrst_out |= 1<<ep->epnumber;

	ep->dma->dmaaddr = addr;
	if (ep->is_in)
	{
		ep->dma->dmatsize = length;

		/*If send0len=1, the device sends the short packet.*/
		if (/*zero ||*/ length < ep->ep_usb.maxpacket)
			ep->dma->dmactrl = REG_DMACTRL_SEND_0_LEN;
		
		ep->dma->dmactrl |= REG_DMACTRL_HSIZE_32;
		ep->dma->dmactrl |= REG_DMACTRL_START_DMA;
		udc->dma->dmastart_in |= (0x01<<ep->epnumber);
		return -EINPROGRESS;
	}
	else
	{
		ep->dma->dmatsize = ep->ep_usb.maxpacket;
		ep->dma->dmactrl = (REG_DMACTRL_START_DMA | REG_DMACTRL_HSIZE_32/*REG_DMACTRL_HPROT_0*/);
		udc->dma->dmastart_out |= (0x01<<ep->epnumber);
		return -EINPROGRESS;

	}

	/*
	 * Wait till DMA transaction is complete and
	 * check whether the DMA transaction was
	 * successful.
	 */
	do {
		dma_intrstatus = udc->read_fn(udc->addr + UDC_DMA_INIRQ_REG_ADDR);
		if (ep->is_in)
		{
			if (dma_intrstatus & (1<<ep->epnumber))
			{
				udc->write_fn(udc->addr,UDC_DMA_INIRQ_REG_ADDR,(1<<ep->epnumber));
				evaudc_wrstatus(udc);
				break;
			}
		}
		else 
		{
			if (dma_intrstatus &  (1<<(ep->epnumber+16)))
			{
				udc->write_fn(udc->addr,UDC_DMA_INIRQ_REG_ADDR,(1<<(ep->epnumber+16)));
				break;
			}
		}

		/*
		 * We can't sleep here, because it's also called from
		 * interrupt context.
		 */
		timeout--;
		if (!timeout) {
			dev_err(udc->dev, "DMA timeout ep(%s)\n",ep->name);
			return -ETIMEDOUT;
			//timeout = 5000;
		}
		//printk("evaudc_start_dma arrive here2?\n");
		udelay(1);
	} while (1);
	return rc;
}

/**
 * evaudc_dma_send - Sends IN data using DMA.
 * @ep: pointer to the usb device endpoint structure.
 * @req: pointer to the usb request structure.
 * @buffer: pointer to data to be sent.
 * @length: number of bytes to send.
 *
 * Return: 0 on success, -EAGAIN if no buffer is free and error
 *	   code on failure.
 *
 * This function sends data using DMA.
 */
static int evaudc_dma_send(struct evausb_ep *ep, struct evausb_req *req,
			 u8 *buffer, u32 length)
{
	dma_addr_t src;
	struct evausb_udc *udc = ep->udc;

	src = req->usb_req.dma + req->usb_req.actual;
	if (req->usb_req.length)
	{
		dma_sync_single_for_device(udc->dev, src,
					   length, DMA_TO_DEVICE);
	} else {
		/* None of ping pong buffers are ready currently .*/
		return -EAGAIN;
	}

	return evaudc_start_dma(ep, src, length,(bool)req->usb_req.zero);
}

/**
 * evaudc_dma_receive - Receives OUT data using DMA.
 * @ep: pointer to the usb device endpoint structure.
 * @req: pointer to the usb request structure.
 * @buffer: pointer to storage buffer of received data.
 * @length: number of bytes to receive.
 *
 * Return: 0 on success, -EAGAIN if no buffer is free and error
 *	   code on failure.
 *
 * This function receives data using DMA.
 */
static int evaudc_dma_receive(struct evausb_ep *ep, struct evausb_req *req,
			    u8 *buffer, u32 length)
{
	dma_addr_t dst;
	struct evausb_udc *udc = ep->udc;

	dst = req->usb_req.dma + req->usb_req.actual;
	//dst = req->usb_req.dma;
	if (req->usb_req.length)
	{
		dma_sync_single_for_device(udc->dev, dst,
					   length, DMA_FROM_DEVICE);
	} else {
		/* None of ping pong buffers are ready currently .*/
		return -EAGAIN;
	}

	return evaudc_start_dma(ep, dst, length,req->usb_req.zero);
}

/**
 * evaudc_eptxrx - Transmits or receives data to or from an endpoint.
 * @ep: pointer to the usb endpoint configuration structure.
 * @req: pointer to the usb request structure.
 * @bufferptr: pointer to buffer containing the data to be sent.
 * @bufferlen: The number of data bytes to be sent.
 *
 * Return: 0 on success, -EAGAIN if no buffer is free.
 *
 * This function copies the transmit/receive data to/from the end point buffer
 * and enables the buffer for transmission/reception.
 */
static int evaudc_eptxrx(struct evausb_ep *ep, struct evausb_req *req,
		       u8 *bufferptr, u32 bufferlen)
{
	int rc = 0;
	struct evausb_udc *udc = ep->udc;

	if (udc->dma_enabled) {
		if (ep->is_in)
			rc = evaudc_dma_send(ep, req, bufferptr, bufferlen);
		else
			rc = evaudc_dma_receive(ep, req, bufferptr, bufferlen);
		return rc;
	}
	/*current driver don't support no dma data transfer for evatronix */

	return rc;
}

/**
 * evaudc_done - Exeutes the endpoint data transfer completion tasks.
 * @ep: pointer to the usb device endpoint structure.
 * @req: pointer to the usb request structure.
 * @status: Status of the data transfer.
 *
 * Deletes the message from the queue and updates data transfer completion
 * status.
 */
static void evaudc_done(struct evausb_ep *ep, struct evausb_req *req, int status)
{
	struct evausb_udc *udc = ep->udc;

	list_del_init(&req->queue);

	if (req->usb_req.status == -EINPROGRESS)
		req->usb_req.status = status;
	else
		status = req->usb_req.status;

	if (status && status != -ESHUTDOWN)
		dev_dbg(udc->dev, "%s done %p, status %d\n",
			ep->ep_usb.name, req, status);
	/* unmap request if DMA is present*/
	if (udc->dma_enabled && ep->epnumber && req->usb_req.length)
		usb_gadget_unmap_request(&udc->gadget, &req->usb_req,
					 ep->is_in);
	if (ep->is_in)
		evaudc_handle_unaligned_buf_complete(udc,ep,req);

	if (req->usb_req.complete) {
		spin_unlock(&udc->lock);
		req->usb_req.complete(&ep->ep_usb, &req->usb_req);
		spin_lock(&udc->lock);
	}
}

/**
 * evaudc_read_fifo - Reads the data from the given endpoint buffer.
 * @ep: pointer to the usb device endpoint structure.
 * @req: pointer to the usb request structure.
 *
 * Return: 0 if request is completed and -EAGAIN if not completed.
 *
 * Pulls OUT packet data from the endpoint buffer.
 */
static int evaudc_read_fifo(struct evausb_ep *ep, struct evausb_req *req)
{
	u8 *buf;
	u32 is_short, count, bufferspace;
	int ret;
	int retval = -EAGAIN;
	struct evausb_udc *udc = ep->udc;

	buf = req->usb_req.buf + req->usb_req.actual;
	prefetchw(buf);
	evaudc_handle_unaligned_buf_start(udc,ep,req);
	
	bufferspace = req->usb_req.length - req->usb_req.actual;
	count = bufferspace;
	if (count > ep->ep_usb.maxpacket)
		count = ep->ep_usb.maxpacket;

	if (unlikely(!bufferspace)) {
		/*
		 * This happens when the driver's buffer
		 * is smaller than what the host sent.
		 * discard the extra data.
		 */
		if (req->usb_req.status != -EOVERFLOW)
			dev_dbg(udc->dev, "%s overflow %d\n",
				ep->ep_usb.name, count);
		req->usb_req.status = -EOVERFLOW;
		evaudc_done(ep, req, -EOVERFLOW);
		return 0;
	}

	ret = evaudc_eptxrx(ep, req, buf, count);
	switch (ret) {
	case 0:
		req->usb_req.actual += min(count, bufferspace);
		dev_dbg(udc->dev, "read %s, %d bytes%s req %p %d/%d\n",
			ep->ep_usb.name, count, is_short ? "/S" : "", req,
			req->usb_req.actual, req->usb_req.length);
		bufferspace -= count;

		/* Completion */
		if ((req->usb_req.actual == req->usb_req.length) || is_short) {
			if (udc->dma_enabled && req->usb_req.length)
				dma_sync_single_for_cpu(udc->dev,
							req->usb_req.dma,
							req->usb_req.actual,
							DMA_FROM_DEVICE);
			evaudc_done(ep, req, 0);
			return 0;
		}
		break;
	case -EAGAIN:
		dev_dbg(udc->dev, "receive busy\n");
		break;
	case -EINPROGRESS:
		dev_dbg(udc->dev, "receive in process\n");
		retval = -EINPROGRESS;
		break;
	case -EINVAL:
	case -ETIMEDOUT:
		/* DMA error, dequeue the request */
		evaudc_done(ep, req, -ECONNRESET);
		retval = 0;
		break;
	}

	return retval;
}

/**
 * evaudc_write_fifo - Writes data into the given endpoint buffer.
 * @ep: pointer to the usb device endpoint structure.
 * @req: pointer to the usb request structure.
 *
 * Return: 0 if request is completed and -EAGAIN if not completed.
 *
 * Loads endpoint buffer for an IN packet.
 */
static int evaudc_write_fifo(struct evausb_ep *ep, struct evausb_req *req)
{
	u32 max;
	u32 length;
	int ret;
	int retval = -EAGAIN;
	struct evausb_udc *udc = ep->udc;
	int is_last, is_short = 0;
	u8 *buf;

	max = le16_to_cpu(ep->desc->wMaxPacketSize);
	buf = req->usb_req.buf + req->usb_req.actual;
	prefetch(buf);
	evaudc_handle_unaligned_buf_start(udc,ep,req);
	length = req->usb_req.length - req->usb_req.actual;
	length = min(length, max);
	req->real_dma_length = length;
	ret = evaudc_eptxrx(ep, req, buf, length);
	switch (ret) {
	case 0:
		req->usb_req.actual += length;
		if (unlikely(length != max)) {
			is_last = is_short = 1;
		} else {
			if (likely(req->usb_req.length !=
				   req->usb_req.actual) || req->usb_req.zero)
				is_last = 0;
			else
				is_last = 1;
		}
		dev_dbg(udc->dev, "%s: wrote %s %d bytes%s%s %d left %p\n",
			__func__, ep->ep_usb.name, length, is_last ? "/L" : "",
			is_short ? "/S" : "",
			req->usb_req.length - req->usb_req.actual, req);
		/* completion */
		if (is_last) {
			evaudc_done(ep, req, 0);
			retval = 0;
		}
		break;
	case -EAGAIN:
		dev_dbg(udc->dev, "Send busy\n");
		break;
	case -EINPROGRESS:
		dev_dbg(udc->dev, "send in process\n");
		retval = -EINPROGRESS;
		break;
	case -EINVAL:
	case -ETIMEDOUT:
		/* DMA error, dequeue the request */
		evaudc_done(ep, req, -ECONNRESET);
		retval = 0;
		break;
	}

	return retval;
}

/**
 * evaudc_nuke - Cleans up the data transfer message list.
 * @ep: pointer to the usb device endpoint structure.
 * @status: Status of the data transfer.
 */
static void evaudc_nuke(struct evausb_ep *ep, int status)
{
	struct evausb_req *req;

	while (!list_empty(&ep->queue)) {
		req = list_first_entry(&ep->queue, struct evausb_req, queue);
		evaudc_done(ep, req, status);
	}
}

/**
 * evaudc_ep_set_halt - Stalls/unstalls the given endpoint.
 * @_ep: pointer to the usb device endpoint structure.
 * @value: value to indicate stall/unstall.
 *
 * Return: 0 for success and error value on failure
 */
static int evaudc_ep_set_halt(struct usb_ep *_ep, int value)
{
	struct evausb_ep *ep = to_evausb_ep(_ep);
	struct evausb_udc *udc;
	unsigned long flags;
	u32 epcfgreg;

	if (!_ep || (!ep->desc && ep->epnumber)) {
		return -EINVAL;
	}
	udc = ep->udc;

	if (ep->is_in && (!list_empty(&ep->queue)) && value) {
		dev_dbg(udc->dev, "requests pending can't halt\n");
		return -EAGAIN;
	}

	spin_lock_irqsave(&udc->lock, flags);

	if (value) {
		/* Stall the device.*/
		epcfgreg = udc->read_fn(udc->addr + ep->offset);
		epcfgreg |= EVAUSB_EP_CFG_STALL_MASK;
		udc->write_fn(udc->addr, ep->offset, epcfgreg);
	} else {
		/* Unstall the device.*/
		epcfgreg = udc->read_fn(udc->addr + ep->offset);
		epcfgreg &= ~EVAUSB_EP_CFG_STALL_MASK;
		udc->write_fn(udc->addr, ep->offset, epcfgreg);
		if (ep->epnumber) {
			/* Reset the toggle bit.*/
			epcfgreg = udc->read_fn(udc->addr + UDC_TOGRESET_REG_ADDR);
			if (ep->is_in)
				epcfgreg |= 1<<(ep->epnumber);
			else
				epcfgreg |= 1<<(ep->epnumber+16);
			udc->write_fn(udc->addr,UDC_TOGRESET_REG_ADDR, epcfgreg);

		}
	}

	spin_unlock_irqrestore(&udc->lock, flags);
	return 0;
}

/**
 * evaudc_ep_enable - Enables the given endpoint.
 * @ep: pointer to the xusb endpoint structure.
 * @desc: pointer to usb endpoint descriptor.
 *
 * Return: 0 for success and error value on failure
 */
static int __evaudc_ep_enable(struct evausb_ep *ep,
			    const struct usb_endpoint_descriptor *desc)
{
	struct evausb_udc *udc = ep->udc;
	u32 tmp;
	u32 epcfg;
	u32 ier;
	u32 dma_ier;
	u16 maxpacket;

	ep->is_in = ((desc->bEndpointAddress & USB_DIR_IN) != 0);
	/* Bit 3...0:endpoint number */
	ep->epnumber = (desc->bEndpointAddress & 0x0f);
	ep->desc = desc;
	ep->ep_usb.desc = desc;
	tmp = desc->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK;
	ep->ep_usb.maxpacket = maxpacket = le16_to_cpu(desc->wMaxPacketSize);

	switch (tmp) {
	case USB_ENDPOINT_XFER_CONTROL:
		dev_dbg(udc->dev, "only one control endpoint\n");
		/* NON- ISO */
		ep->is_iso = false;
		ep->is_int = false;
		return -EINVAL;
	case USB_ENDPOINT_XFER_INT:
		/* NON- ISO */
		ep->is_iso = false;
		ep->is_int = true;
		if (maxpacket > 64) {
			dev_dbg(udc->dev, "bogus maxpacket %d\n", maxpacket);
			return -EINVAL;
		}
		break;
	case USB_ENDPOINT_XFER_BULK:
		/* NON- ISO */
		ep->is_iso = false;
		ep->is_int = false;
		if (!(is_power_of_2(maxpacket) && maxpacket >= 8 &&
				maxpacket <= 512)) {
			dev_info(udc->dev, "bogus maxpacket %d\n", maxpacket);
			return -EINVAL;
		}
		break;
	case USB_ENDPOINT_XFER_ISOC:
		/* ISO */
		ep->is_iso = true;
		ep->is_int = false;
		break;
	}

	evaudc_epconfig(ep, udc);

	dev_dbg(udc->dev, "Enable Endpoint %d max pkt is %d\n",
		ep->epnumber, maxpacket);

	/* Enable the End point.*/
	epcfg = udc->read_fn(udc->addr + ep->offset);
	epcfg |= EVAUSB_EP_CFG_VALID_MASK;
	udc->write_fn(udc->addr, ep->offset, epcfg);

	/* Enable the End point int and dma int.  UDC_INIEN_REG_ADDR*/
	ier = udc->read_fn(udc->addr + UDC_INIEN_REG_ADDR);
	if (!ep->epnumber)
	{
		ier |= ((1<<ep->epnumber)|(1<<(ep->epnumber + 16)));
	}
	else
	{
		if (ep->is_in)
		{
			ier |= (1<<ep->epnumber);
		}
		else
		{
			ier |= ((1<<(ep->epnumber + 16)));
		}
	}
	udc->write_fn(udc->addr, UDC_INIEN_REG_ADDR, ier);
	ier = udc->read_fn(udc->addr + UDC_INIEN_REG_ADDR);
	if (udc->dma_enabled)
	{
		dma_ier = udc->read_fn(udc->addr + UDC_DMA_INIEN_REG_ADDR);
		if (!ep->epnumber)
		{
			dma_ier |= 0X10001;
		}
		else
		{
			if (ep->is_in)
			{
				dma_ier |= (1<<ep->epnumber);
			}
			else
			{
				dma_ier |= ((1<<(ep->epnumber + 16)));
			}
		}
		udc->write_fn(udc->addr, UDC_DMA_INIEN_REG_ADDR, dma_ier);
		ier = udc->read_fn(udc->addr + UDC_DMA_INIEN_REG_ADDR);
	}

	return 0;
}

/**
 * evaudc_ep_enable - Enables the given endpoint.
 * @_ep: pointer to the usb endpoint structure.
 * @desc: pointer to usb endpoint descriptor.
 *
 * Return: 0 for success and error value on failure
 */
static int evaudc_ep_enable(struct usb_ep *_ep,
			  const struct usb_endpoint_descriptor *desc)
{
	struct evausb_ep *ep;
	struct evausb_udc *udc;
	unsigned long flags;
	int ret;

	if (!_ep || !desc || desc->bDescriptorType != USB_DT_ENDPOINT) {
		return -EINVAL;
	}

	ep = to_evausb_ep(_ep);
	udc = ep->udc;

	if (!udc->driver || udc->gadget.speed == USB_SPEED_UNKNOWN) {
		dev_dbg(udc->dev, "bogus device state\n");
		return -ESHUTDOWN;
	}

	spin_lock_irqsave(&udc->lock, flags);
	ret = __evaudc_ep_enable(ep, desc);
	spin_unlock_irqrestore(&udc->lock, flags);

	return ret;
}

/**
 * evaudc_ep_disable - Disables the given endpoint.
 * @_ep: pointer to the usb endpoint structure.
 *
 * Return: 0 for success and error value on failure
 */
static int evaudc_ep_disable(struct usb_ep *_ep)
{
	struct evausb_ep *ep;
	unsigned long flags;
	u32 epcfg;
	struct evausb_udc *udc;
	u32 ier;
	u32 dma_ier;

	if (!_ep) {
		return -EINVAL;
	}

	ep = to_evausb_ep(_ep);
	udc = ep->udc;

	spin_lock_irqsave(&udc->lock, flags);

	evaudc_nuke(ep, -ESHUTDOWN);

	/* Restore the endpoint's pristine config */
	ep->desc = NULL;
	ep->ep_usb.desc = NULL;

	dev_dbg(udc->dev, "USB Ep %d disable\n ", ep->epnumber);
	/* Disable the endpoint.*/
	epcfg = udc->read_fn(udc->addr + ep->offset);
	epcfg &= ~EVAUSB_EP_CFG_VALID_MASK;
	udc->write_fn(udc->addr, ep->offset, epcfg);
	
	/* disable the End point int and dma int.  UDC_INIEN_REG_ADDR*/
	ier = udc->read_fn(udc->addr + UDC_INIEN_REG_ADDR);
	if (!ep->epnumber)
	{
		ier &= ~((1<<ep->epnumber)|(1<<(ep->epnumber + 16)));
	}
	else
	{
		if (ep->is_in)
		{
			ier &= ~(1<<ep->epnumber);
		}
		else
		{
			ier &= ~((1<<(ep->epnumber + 16)));
		}
	}
	udc->write_fn(udc->addr, UDC_INIEN_REG_ADDR, ier);
	
	if (udc->dma_enabled)
	{
		dma_ier = udc->read_fn(udc->addr + UDC_DMA_INIEN_REG_ADDR);
		if (!ep->epnumber)
		{
			dma_ier &= ~((1<<ep->epnumber)|(1<<(ep->epnumber + 16)));
		}
		else
		{
			if (ep->is_in)
			{
				dma_ier &= ~(1<<ep->epnumber);
			}
			else
			{
				dma_ier &= ~((1<<(ep->epnumber + 16)));
			}
		}
		udc->write_fn(udc->addr, UDC_DMA_INIEN_REG_ADDR, dma_ier);
	}

	spin_unlock_irqrestore(&udc->lock, flags);
	return 0;
}

/**
 * evaudc_ep_alloc_request - Initializes the request queue.
 * @_ep: pointer to the usb endpoint structure.
 * @gfp_flags: Flags related to the request call.
 *
 * Return: pointer to request structure on success and a NULL on failure.
 */
static struct usb_request *evaudc_ep_alloc_request(struct usb_ep *_ep,
						 gfp_t gfp_flags)
{
	struct evausb_ep *ep = to_evausb_ep(_ep);
	struct evausb_udc *udc;
	struct evausb_req *req;

	udc = ep->udc;
	req = kzalloc(sizeof(*req), gfp_flags);
	if (!req) {
		dev_err(udc->dev, "%s:not enough memory", __func__);
		return NULL;
	}

	req->ep = ep;
	INIT_LIST_HEAD(&req->queue);
	return &req->usb_req;
}

/**
 * evaudc_free_request - Releases the request from queue.
 * @_ep: pointer to the usb device endpoint structure.
 * @_req: pointer to the usb request structure.
 */
static void evaudc_free_request(struct usb_ep *_ep, struct usb_request *_req)
{
	struct evausb_req *req = to_evausb_req(_req);

	if (req->saved_req_buf)
		kfree(req->saved_req_buf);
	kfree(req);
}

/**
 * evaudc_ep0_queue - Adds the request to endpoint 0 queue.
 * @ep0: pointer to the xusb endpoint 0 structure.
 * @req: pointer to the xusb request structure.
 *
 * Return: 0 for success and error value on failure
 */
static int __evaudc_ep0_queue(struct evausb_ep *ep0, struct evausb_req *req)
{
	struct evausb_udc *udc = ep0->udc;
	int ret;
	bool first;

	if (!udc->driver || udc->gadget.speed == USB_SPEED_UNKNOWN) {
		dev_dbg(udc->dev, "%s, bogus device state\n", __func__);
		return -EINVAL;
	}
	if (!list_empty(&ep0->queue)) {
		dev_info(udc->dev, "%s:ep0 busy\n", __func__);
		return -EBUSY;
	}

	req->usb_req.status = -EINPROGRESS;
	req->usb_req.actual = 0;
	first = list_empty(&ep0->queue);

	list_add_tail(&req->queue, &ep0->queue);
	evaudc_handle_unaligned_buf_start(udc,ep0,req);

	if (udc->setup.bRequestType & USB_DIR_IN) {
		if (udc->dma_enabled) {
				ret = usb_gadget_map_request(&udc->gadget, &req->usb_req,1);
				if (ret) {
					dev_dbg(udc->dev, "gadget_map failed ep%d\n",
						ep0->epnumber);
					return -EAGAIN;
				}
		}

	} else {
		if (udc->setup.wLength) {
			if (udc->dma_enabled) {
					ret = usb_gadget_map_request(&udc->gadget, &req->usb_req,0);
					if (ret) {
						dev_dbg(udc->dev, "gadget_map failed ep%d\n",
							ep0->epnumber);
						return -EAGAIN;
					}
			}

		} else {
			evaudc_wrstatus(udc);
			list_del_init(&req->queue);
		}
	}

	if (first && udc->setup.wLength)
	{
		if (udc->setup.bRequestType & USB_DIR_IN)
		{
			ep0->is_in = true;
			ret = evaudc_write_fifo(ep0,req);
		}
		else
		{
			ep0->is_in = false;
			ret = evaudc_read_fifo(ep0,req);
		}
	}

	return 0;
}

/**
 * evaudc_ep0_queue - Adds the request to endpoint 0 queue.
 * @_ep: pointer to the usb endpoint 0 structure.
 * @_req: pointer to the usb request structure.
 * @gfp_flags: Flags related to the request call.
 *
 * Return: 0 for success and error value on failure
 */
static int evaudc_ep0_queue(struct usb_ep *_ep, struct usb_request *_req,
			  gfp_t gfp_flags)
{
	struct evausb_req *req	= to_evausb_req(_req);
	struct evausb_ep	*ep0	= to_evausb_ep(_ep);
	struct evausb_udc *udc	= ep0->udc;
	unsigned long flags;
	int ret;

	spin_lock_irqsave(&udc->lock, flags);
	ret = __evaudc_ep0_queue(ep0, req);
	spin_unlock_irqrestore(&udc->lock, flags);

	return ret;
}

/**
 * evaudc_ep_queue - Adds the request to endpoint queue.
 * @_ep: pointer to the usb endpoint structure.
 * @_req: pointer to the usb request structure.
 * @gfp_flags: Flags related to the request call.
 *
 * Return: 0 for success and error value on failure
 */

volatile bool using_sof = false;
EXPORT_SYMBOL(using_sof);

static int evaudc_ep_queue(struct usb_ep *_ep, struct usb_request *_req,
			 gfp_t gfp_flags)
{
	struct evausb_req *req = to_evausb_req(_req);
	struct evausb_ep	*ep  = to_evausb_ep(_ep);
	struct evausb_udc *udc = ep->udc;
	unsigned long flags;
	int  ret;
	int is_empty;

	if (!ep->desc) {
		dev_dbg(udc->dev, "%s:queing request to disabled %s\n",
			__func__, ep->name);
		return -ESHUTDOWN;
	}

	if (!udc->driver || udc->gadget.speed == USB_SPEED_UNKNOWN) {
		dev_dbg(udc->dev, "%s, bogus device state\n", __func__);
		return -EINVAL;
	}

	spin_lock_irqsave(&udc->lock, flags);

	_req->status = -EINPROGRESS;
	_req->actual = 0;

	if (udc->dma_enabled) {
		ret = usb_gadget_map_request(&udc->gadget, &req->usb_req,
					     ep->is_in);
		if (ret) {
			dev_dbg(udc->dev, "gadget_map failed ep%d\n",
				ep->epnumber);
			spin_unlock_irqrestore(&udc->lock, flags);
			return -EAGAIN;
		}
	}

	//req->saved_dma_paddr = req->usb_req.dma;
	is_empty = list_empty(&ep->queue);
	list_add_tail(&req->queue, &ep->queue);

	if (is_empty) {
		if (ep->is_in) {
			if (ep->is_iso) {
				if(!using_sof) {
					evaudc_write_fifo(ep, req);
				}
			} else
				evaudc_write_fifo(ep, req);
		} else {
			evaudc_read_fifo(ep, req);
		}
	}

	spin_unlock_irqrestore(&udc->lock, flags);
	return 0;
}

/**
 * evaudc_ep_dequeue - Removes the request from the queue.
 * @_ep: pointer to the usb device endpoint structure.
 * @_req: pointer to the usb request structure.
 *
 * Return: 0 for success and error value on failure
 */
static int evaudc_ep_dequeue(struct usb_ep *_ep, struct usb_request *_req)
{
	struct evausb_ep *ep	= to_evausb_ep(_ep);
	struct evausb_req *req	= to_evausb_req(_req);
	struct evausb_udc *udc	= ep->udc;
	unsigned long flags;

	spin_lock_irqsave(&udc->lock, flags);
	/* Make sure it's actually queued on this endpoint */
	list_for_each_entry(req, &ep->queue, queue) {
		if (&req->usb_req == _req)
			break;
	}
	if (&req->usb_req != _req) {
		spin_unlock_irqrestore(&ep->udc->lock, flags);
		return -EINVAL;
	}
	evaudc_done(ep, req, -ECONNRESET);
	spin_unlock_irqrestore(&udc->lock, flags);

	return 0;
}

/**
 * evaudc_ep0_enable - Enables the given endpoint.
 * @ep: pointer to the usb endpoint structure.
 * @desc: pointer to usb endpoint descriptor.
 *
 * Return: error always.
 *
 * endpoint 0 enable should not be called by gadget layer.
 */
static int evaudc_ep0_enable(struct usb_ep *ep,
			   const struct usb_endpoint_descriptor *desc)
{
	return -EINVAL;
}

/**
 * evaudc_ep0_disable - Disables the given endpoint.
 * @ep: pointer to the usb endpoint structure.
 *
 * Return: error always.
 *
 * endpoint 0 disable should not be called by gadget layer.
 */
static int evaudc_ep0_disable(struct usb_ep *ep)
{
	return -EINVAL;
}

static const struct usb_ep_ops evausb_ep0_ops = {
	.enable		= evaudc_ep0_enable,
	.disable	= evaudc_ep0_disable,
	.alloc_request	= evaudc_ep_alloc_request,
	.free_request	= evaudc_free_request,
	.queue		= evaudc_ep0_queue,
	.dequeue	= evaudc_ep_dequeue,
	.set_halt	= evaudc_ep_set_halt,
};

static const struct usb_ep_ops evausb_ep_ops = {
	.enable		= evaudc_ep_enable,
	.disable	= evaudc_ep_disable,
	.alloc_request	= evaudc_ep_alloc_request,
	.free_request	= evaudc_free_request,
	.queue		= evaudc_ep_queue,
	.dequeue	= evaudc_ep_dequeue,
	.set_halt	= evaudc_ep_set_halt,
};

/**
 * evaudc_wakeup - Send remote wakeup signal to host
 * @gadget: pointer to the usb gadget structure.
 *
 * Return: 0 on success and error on failure
 */
static int evaudc_wakeup(struct usb_gadget *gadget)
{
	struct evausb_udc *udc = to_udc(gadget);
	u32 crtlreg;
	int status = -EINVAL;
	unsigned long flags;

	spin_lock_irqsave(&udc->lock, flags);

	/* Remote wake up not enabled by host */
	if (!udc->remote_wkp)
		goto done;

	crtlreg = udc->read_fn(udc->addr + UDC_USB_CTRL_REG_ADDR);
	crtlreg |= EVAUSB_CONTROL_USB_RMTWAKE_MASK;
	/* set remote wake up bit */
	udc->write_fn(udc->addr, UDC_USB_CTRL_REG_ADDR, crtlreg);
	/*
	 * wait for a while and reset remote wake up bit since this bit
	 * is not cleared by HW after sending remote wakeup to host.
	 */
	mdelay(2);

	crtlreg &= ~EVAUSB_CONTROL_USB_RMTWAKE_MASK;
	udc->write_fn(udc->addr, UDC_USB_CTRL_REG_ADDR, crtlreg);
	status = 0;
done:
	spin_unlock_irqrestore(&udc->lock, flags);
	return status;
}

/**
 * evaudc_pullup - start/stop USB traffic
 * @gadget: pointer to the usb gadget structure.
 * @is_on: flag to start or stop
 *    usbcs.6 bit (discon)Peripheral Mode operation:
 *    Software disconnect bit.
 *    The microprocessor can write this bit (a '1' value) to force the "disconnect" state. In the
 *    "disconnect" state, an external USB2.0 transceiver disconnects pull-up resistors from
 *     the D+ and D- lines and the USB host detects device disconnection.
 * Return: 0 always
 *
 * This function starts/stops SIE engine of IP based on is_on.
 */
static int evaudc_pullup(struct usb_gadget *gadget, int is_on)
{
	struct evausb_udc *udc = to_udc(gadget);
	unsigned long flags;
	u32 crtlreg;
	u32 ier;

	spin_lock_irqsave(&udc->lock, flags);

	crtlreg = udc->read_fn(udc->addr + UDC_USB_CTRL_REG_ADDR);
	if (is_on)
	{
		crtlreg &= ~EVAUSB_CONTROL_USB_READY_MASK;
	}
	else
		crtlreg |= EVAUSB_CONTROL_USB_READY_MASK;

	udc->write_fn(udc->addr, UDC_USB_CTRL_REG_ADDR, crtlreg);
	crtlreg = udc->read_fn(udc->addr + UDC_USB_CTRL_REG_ADDR);
	udc->write_fn(udc->addr, UDC_USB_CTRL_REG_ADDR, crtlreg);
	/* Enable the interrupts. */
	ier = /*EVAUSB_STATUS_PING_ALL_OUTEPIEIRQ_MASK|*/
	EVAUSB_STATUS_HSPEEDIEIRQ_MASK|
	EVAUSB_STATUS_URESIEIRQ_MASK|
	EVAUSB_STATUS_SUSPIEIRQ_MASK;
	udc->write_fn(udc->addr,UDC_PING_USB_IEN_REG_ADDR,ier);
	ier = udc->read_fn(udc->addr + UDC_PING_USB_IEN_REG_ADDR);
	ier |= EVAUSB_STATUS_OVERFLOWIEIRQ_MASK|
		EVAUSB_STATUS_SUTOKEIEIRQ_MASK|
		EVAUSB_STATUS_SUDAVIEIRQ_MASK;
	udc->write_fn(udc->addr,UDC_PING_USB_IEN_REG_ADDR,ier);

	spin_unlock_irqrestore(&udc->lock, flags);
printk("evaudc_pullup : is_on %d\n",is_on);
	return 0;
}

/**
 * xudc_eps_init - initialize endpoints.
 * @udc: pointer to the usb device controller structure.
 */
 static void evaudc_initep(struct evausb_udc *udc,
				       struct evausb_ep *ep,
				       int epnum,
				       bool dir_in)
{
	char *dir;

	if (epnum == 0)
		dir = "";
	else if (dir_in)
		dir = "in";
	else
		dir = "out";

	ep->is_in = dir_in;
	ep->epnumber = epnum;
	
	snprintf(ep->name, sizeof(ep->name), "ep%d%s", epnum, dir);

	INIT_LIST_HEAD(&ep->queue);
	INIT_LIST_HEAD(&ep->ep_usb.ep_list);

	/* add to the list of endpoints known by the gadget driver */
	if (epnum)
		list_add_tail(&ep->ep_usb.ep_list, &udc->gadget.ep_list);

	ep->udc = udc;
	ep->ep_usb.name = ep->name;
	usb_ep_set_maxpacket_limit(&ep->ep_usb, epnum ? 1024 : EP0_MAX_PACKET);

	if (epnum == 0) {
		ep->ep_usb.caps.type_control = true;
		ep->ep_usb.ops = &evausb_ep0_ops;
		ep->offset = EP0MAXPACK_REG_ADDR;
		ep->dma = (volatile struct eva_ep_dma_struct *)(udc->addr + EP0DMAADDR_REG_ADDR);
		
	} else {
		ep->ep_usb.caps.type_iso = true;
		ep->ep_usb.caps.type_bulk = true;
		ep->ep_usb.caps.type_int = true;
		ep->ep_usb.ops = &evausb_ep_ops;
		if (dir_in)
		{
			ep->offset = UDC_EP_IN1MAXPACK_REG_ADDR + ((epnum-1) * 0x8);
			ep->dma = (volatile struct eva_ep_dma_struct *)(udc->addr + UDC_EP_IN1DMAADDR_REG_ADDR + (epnum-1) * 0x20);
			//printk("ep(%s) offset=>(0x%x) dma=>(0x%x)\n",ep->name,ep->offset,(u32)ep->dma-(u32)udc->addr);
		}
		else
		{
			ep->offset = UDC_EP_OUT1MAXPACK_REG_ADDR + ((epnum-1) * 0x8);
			ep->dma = (volatile struct eva_ep_dma_struct *)(udc->addr + UDC_EP_OUT1DMAADDR_REG_ADDR + (epnum-1) * 0x20);
		}
	}

	if (dir_in)
		ep->ep_usb.caps.dir_in = true;
	else
		ep->ep_usb.caps.dir_out = true;
	
	
	ep->is_iso = 0;
	evaudc_epconfig(ep, udc);

}
 static void evaudc_eps_init(struct evausb_udc *udc)
{
	u32 epnum = 0;

	evaudc_initep(udc, &udc->ep0,epnum, 0);

	INIT_LIST_HEAD(&udc->gadget.ep_list);
	/* initialise the endpoints now the core has been initialised */
	for (epnum = 1; epnum < EVAUSB_MAX_ENDPOINTS; epnum++) {
		evaudc_initep(udc, &udc->ep_in[epnum],
							epnum, 1);
		evaudc_initep(udc, &udc->ep_out[epnum],
							epnum, 0);
	}
	//printk("im here? last line of evaudc_eps_init\n");
}

/**
 * evaudc_stop_activity - Stops any further activity on the device.
 * @udc: pointer to the usb device controller structure.
 */
static void evaudc_stop_activity(struct evausb_udc *udc)
{
	int i;
	struct evausb_ep *ep;

	for (i = 1; i < EVAUSB_MAX_ENDPOINTS; i++) {
		ep = &udc->ep_in[i];
		evaudc_nuke(ep, -ESHUTDOWN);
		ep = &udc->ep_out[i];
		evaudc_nuke(ep, -ESHUTDOWN);
	}
}

/**
 * xudc_start - Starts the device.
 * @gadget: pointer to the usb gadget structure
 * @driver: pointer to gadget driver structure
 *
 * Return: zero on success and error on failure
 */
static int evaudc_start(struct usb_gadget *gadget,
		      struct usb_gadget_driver *driver)
{
	struct evausb_udc *udc	= to_udc(gadget);
	struct evausb_ep *ep0	= &udc->ep0;
	const struct usb_endpoint_descriptor *desc = &config_bulk_out_desc;
	unsigned long flags;
	int ret = 0;

	spin_lock_irqsave(&udc->lock, flags);

	if (udc->driver) {
		dev_err(udc->dev, "%s is already bound to %s\n",
			udc->gadget.name, udc->driver->driver.name);
		ret = -EBUSY;
		goto err;
	}

	/* hook up the driver */
	udc->driver = driver;
	udc->gadget.speed = driver->max_speed;

	/* Enable the control endpoint. */
	ret = __evaudc_ep_enable(ep0, desc);

	/* Set device address and remote wakeup to 0 */
	*udc->fnaddr = 0x0;
	udc->remote_wkp = 0;
err:
	spin_unlock_irqrestore(&udc->lock, flags);
	return ret;
}

/**
 * xudc_stop - stops the device.
 * @gadget: pointer to the usb gadget structure
 * @driver: pointer to usb gadget driver structure
 *
 * Return: zero always
 */
static int evaudc_stop(struct usb_gadget *gadget)
{
	struct evausb_udc *udc = to_udc(gadget);
	unsigned long flags;

	spin_lock_irqsave(&udc->lock, flags);

	udc->gadget.speed = USB_SPEED_UNKNOWN;
	udc->driver = NULL;

	/* Set device address and remote wakeup to 0 */
	*udc->fnaddr = 0x0;
	udc->remote_wkp = 0;

	evaudc_stop_activity(udc);

	spin_unlock_irqrestore(&udc->lock, flags);

	return 0;
}
static struct usb_ep *g_ep1in = NULL;
static struct usb_ep *evausb_match_ep(struct usb_gadget *g,
		struct usb_endpoint_descriptor *desc,
		struct usb_ss_ep_comp_descriptor *ep_comp)
{
	struct usb_ep *ep = NULL;

	switch (usb_endpoint_type(desc)) {
	case USB_ENDPOINT_XFER_ISOC:
		if (usb_endpoint_dir_in(desc))
		{
			ep = gadget_find_ep_by_name(g, "ep1in");
			g_ep1in = ep;

		}
		else
			ep = gadget_find_ep_by_name(g, "ep1out");
		break;

	default:
		break;
	}

	if (ep && usb_gadget_ep_match_desc(g, ep, desc, ep_comp))
	{
		return ep;
	}

	return NULL;
}


static const struct usb_gadget_ops evausb_udc_ops = {
	//.get_frame	= evaudc_get_frame,
	.wakeup		= evaudc_wakeup,
	.pullup		= evaudc_pullup,
	.udc_start	= evaudc_start,
	.udc_stop	= evaudc_stop,
	.match_ep	= evausb_match_ep,
};

/**
 * xudc_clear_stall_all_ep - clears stall of every endpoint.
 * @udc: pointer to the udc structure.
 */
static void evaudc_clear_stall_all_ep(struct evausb_udc *udc)
{
	struct evausb_ep *ep;
	u32 epcfgreg = 0;
	int i = 0;

	ep = &udc->ep0;
	epcfgreg = udc->read_fn(udc->addr+ep->offset);
	epcfgreg &= ~EVAUSB_EP0_CFG_STALL_MASK;
	udc->write_fn(udc->addr, ep->offset, epcfgreg);

	for (i = 1; i < EVAUSB_MAX_ENDPOINTS; i++) {
		ep = &udc->ep_in[i];
		epcfgreg = udc->read_fn(udc->addr+ep->offset);
		epcfgreg &= ~EVAUSB_EP_CFG_STALL_MASK;
		udc->write_fn(udc->addr, ep->offset, epcfgreg);
		if (ep->epnumber) {
			/* Reset the toggle bit.*/
			epcfgreg = udc->read_fn(udc->addr + UDC_TOGRESET_REG_ADDR);
			epcfgreg |= 1<<ep->epnumber;
			udc->write_fn(udc->addr,UDC_TOGRESET_REG_ADDR, epcfgreg);
		}
		ep = &udc->ep_out[i];
		epcfgreg = udc->read_fn(udc->addr+ep->offset);
		epcfgreg &= ~EVAUSB_EP_CFG_STALL_MASK;
		udc->write_fn(udc->addr, ep->offset, epcfgreg);
		if (ep->epnumber) {
			/* Reset the toggle bit.*/
			epcfgreg = udc->read_fn(udc->addr + UDC_TOGRESET_REG_ADDR);
			epcfgreg |= 1<<(ep->epnumber+16);
			udc->write_fn(udc->addr,UDC_TOGRESET_REG_ADDR, epcfgreg);
		}
	}
}

/**
 * evaudc_startup_handler - The usb device controller interrupt handler.
 * @udc: pointer to the udc structure.
 * @intrstatus: The mask value containing the interrupt sources.
 *
 * This function handles the RESET,SUSPEND,RESUME and DISCONNECT interrupts.
 */
static void evaudc_startup_handler(struct evausb_udc *udc, u32 intrstatus)
{
	u32 intrreg;
	//u32 otgreg;

	if (intrstatus & EVAUSB_STATUS_URESIEIRQ_MASK) {

		dev_info(udc->dev, "Reset\n");
		printk( "evaudc_startup_handler Reset\n");

		evaudc_stop_activity(udc);
		evaudc_clear_stall_all_ep(udc);
		//clear the interrupt
		udc->write_fn(udc->addr,PING_USB_IRQ_REG_ADDR,EVAUSB_STATUS_URESIEIRQ_MASK);

		if (intrstatus & EVAUSB_STATUS_HSPEEDIEIRQ_MASK)
		{
			udc->write_fn(udc->addr,PING_USB_IRQ_REG_ADDR,EVAUSB_STATUS_HSPEEDIEIRQ_MASK);
			udc->gadget.speed = USB_SPEED_HIGH;
		}
		else
		{
			udc->gadget.speed = USB_SPEED_FULL;
		}

		
		/* Set device address and remote wakeup to 0 */
		*udc->fnaddr = 0x0;
		udc->remote_wkp = 0;

		/* Enable the suspend, resume and disconnect */
		intrreg = udc->read_fn(udc->addr + UDC_PING_USB_IEN_REG_ADDR);
		intrreg |= EVAUSB_STATUS_OVERFLOWIEIRQ_MASK |
		EVAUSB_STATUS_HSPEEDIEIRQ_MASK|EVAUSB_STATUS_URESIEIRQ_MASK |
		EVAUSB_STATUS_SUSPIEIRQ_MASK|EVAUSB_STATUS_SUTOKEIEIRQ_MASK |
		EVAUSB_STATUS_SUDAVIEIRQ_MASK;
		udc->write_fn(udc->addr,UDC_PING_USB_IEN_REG_ADDR,intrreg);
		
	}
	if (intrstatus & EVAUSB_STATUS_SUSPIEIRQ_MASK) {

		dev_info(udc->dev, "Suspend\n");
		printk( "evaudc_startup_handler Suspend\n");

		//clear the interrupt
		udc->write_fn(udc->addr,PING_USB_IRQ_REG_ADDR,EVAUSB_STATUS_SUSPIEIRQ_MASK);

		/* Enable the reset, resume and disconnect */
		intrreg = udc->read_fn(udc->addr + UDC_PING_USB_IEN_REG_ADDR);
		intrreg |= EVAUSB_STATUS_OVERFLOWIEIRQ_MASK |
			EVAUSB_STATUS_HSPEEDIEIRQ_MASK|EVAUSB_STATUS_URESIEIRQ_MASK |
			EVAUSB_STATUS_SUSPIEIRQ_MASK|EVAUSB_STATUS_SUTOKEIEIRQ_MASK |
			EVAUSB_STATUS_SUDAVIEIRQ_MASK;
		udc->write_fn(udc->addr,UDC_PING_USB_IEN_REG_ADDR,intrreg);
		intrreg = udc->read_fn(udc->addr + UDC_PING_USB_IEN_REG_ADDR);

		udc->usb_state = USB_STATE_SUSPENDED;
		if (udc->driver && udc->driver->suspend) {
			spin_unlock(&udc->lock);
			udc->driver->suspend(&udc->gadget);
			spin_lock(&udc->lock);
		}

		/* 让 host 重新枚举 slave */
		//otgreg = udc->read_fn(udc->addr + UDC_OTG_REG_ADDR);
		//otgreg |= (9 << 16);
		//udc->write_fn(udc->addr, UDC_OTG_REG_ADDR, otgreg);
	}

	/*In the Device mode the core can be resumed in three ways:
	Resume signaling from host
	Reset signaling from host
	Wakeup pin (remote wakeup) ,we need to handle the situation...*/

}

/**
 * evaudc_ep0_stall - Stall endpoint zero.
 * @udc: pointer to the udc structure.
 *
 * This function stalls endpoint zero.
 */
static void evaudc_ep0_stall(struct evausb_udc *udc)
{
	u32 epcfgreg;
	struct evausb_ep *ep0 = &udc->ep0;

	epcfgreg = udc->read_fn(udc->addr + ep0->offset);
	epcfgreg |= EVAUSB_EP0_CFG_STALL_MASK;
	udc->write_fn(udc->addr, ep0->offset, epcfgreg);
}

/**
 * xudc_setaddress - executes SET_ADDRESS command
 * @udc: pointer to the udc structure.
 *
 * This function executes USB SET_ADDRESS command
 */
static void evaudc_setaddress(struct evausb_udc *udc)
{
	struct evausb_ep *ep0	= &udc->ep0;
	struct evausb_req *req	= udc->req;
	int ret;

	req->usb_req.length = 0;
	ret = __evaudc_ep0_queue(ep0, req);
	if (ret == 0)
		return;

	dev_err(udc->dev, "Can't respond to SET ADDRESS request\n");
	evaudc_ep0_stall(udc);
}

/**
 * xudc_getstatus - executes GET_STATUS command
 * @udc: pointer to the udc structure.
 *
 * This function executes USB GET_STATUS command
 */
static void evaudc_getstatus(struct evausb_udc *udc)
{
	struct evausb_ep *ep0	= &udc->ep0;
	struct evausb_req *req	= udc->req;
	struct evausb_ep *target_ep;
	u16 status = 0;
	u32 epcfgreg;
	int epnum;
	u32 halt;
	int ret;

	switch (udc->setup.bRequestType & USB_RECIP_MASK) {
	case USB_RECIP_DEVICE:
		/* Get device status */
		status = 1 << USB_DEVICE_SELF_POWERED;
		if (udc->remote_wkp)
			status |= (1 << USB_DEVICE_REMOTE_WAKEUP);
		break;
	case USB_RECIP_INTERFACE:
		break;
	case USB_RECIP_ENDPOINT:
		epnum = udc->setup.wIndex & USB_ENDPOINT_NUMBER_MASK;
		if (!epnum)
		{
			target_ep = &udc->ep0;
			epcfgreg = udc->read_fn(udc->addr + target_ep->offset);
			halt = epcfgreg & EVAUSB_EP0_CFG_STALL_MASK;
		}
		else 
		{
			if (udc->setup.wIndex & USB_DIR_IN)
				target_ep = &udc->ep_in[epnum];
			else
				target_ep = &udc->ep_out[epnum];
			epcfgreg = udc->read_fn(udc->addr + target_ep->offset);
			halt = epcfgreg & EVAUSB_EP_CFG_STALL_MASK;
			if (udc->setup.wIndex & USB_DIR_IN) {
				if (!target_ep->is_in)
					goto stall;
			} else {
				if (target_ep->is_in)
					goto stall;
			}
		}
		if (halt)
			status = 1 << USB_ENDPOINT_HALT;
		break;
	default:
		goto stall;
	}

	req->usb_req.length = 2;
	*(u16 *)req->usb_req.buf = cpu_to_le16(status);
	ret = __evaudc_ep0_queue(ep0, req);
	if (ret == 0)
		return;
stall:
	dev_err(udc->dev, "Can't respond to getstatus request\n");
	evaudc_ep0_stall(udc);
}

/**
 * xudc_set_clear_feature - Executes the set feature and clear feature commands.
 * @udc: pointer to the usb device controller structure.
 *
 * Processes the SET_FEATURE and CLEAR_FEATURE commands.
 */
static void evaudc_set_clear_feature(struct evausb_udc *udc)
{
	struct evausb_ep *ep0	= &udc->ep0;
	struct evausb_req *req	= udc->req;
	struct evausb_ep *target_ep;
	u8 endpoint;
	u8 outinbit;
	u32 epcfgreg;
	int flag = (udc->setup.bRequest == USB_REQ_SET_FEATURE ? 1 : 0);
	int ret;

	switch (udc->setup.bRequestType) {
	case USB_RECIP_DEVICE:
		switch (udc->setup.wValue) {
		case USB_DEVICE_TEST_MODE:
			/*
			 * The Test Mode will be executed
			 * after the status phase.
			 */
			break;
		case USB_DEVICE_REMOTE_WAKEUP:
			if (flag)
				udc->remote_wkp = 1;
			else
				udc->remote_wkp = 0;
			break;
		default:
			evaudc_ep0_stall(udc);
			break;
		}
		break;
	case USB_RECIP_ENDPOINT:
		if (!udc->setup.wValue) {
			endpoint = udc->setup.wIndex & USB_ENDPOINT_NUMBER_MASK;
			outinbit = udc->setup.wIndex & USB_ENDPOINT_DIR_MASK;
			if (!endpoint)
			{
				target_ep = &udc->ep0;
				epcfgreg = udc->read_fn(udc->addr + target_ep->offset);
			}
			else
			{
				if (udc->setup.wIndex & USB_DIR_IN)
					target_ep = &udc->ep_in[endpoint];
				else
					target_ep = &udc->ep_out[endpoint];
			}
			outinbit = outinbit >> 7;

			/* Make sure direction matches.*/
			if (outinbit != target_ep->is_in) {
				evaudc_ep0_stall(udc);
				return;
			}
			epcfgreg = udc->read_fn(udc->addr + target_ep->offset);
			if (!endpoint) {
				/* Clear the stall.*/
				epcfgreg &= ~EVAUSB_EP0_CFG_STALL_MASK;
				udc->write_fn(udc->addr,
					      target_ep->offset, epcfgreg);
			} else {
				if (flag) {
					epcfgreg |= EVAUSB_EP_CFG_STALL_MASK;
					udc->write_fn(udc->addr,
						      target_ep->offset,
						      epcfgreg);
				} else {
					/* Unstall the endpoint.*/
					epcfgreg &= ~(EVAUSB_EP_CFG_STALL_MASK);
					udc->write_fn(udc->addr,
						      target_ep->offset,
						      epcfgreg);
					/* Reset the toggle bit.*/
					epcfgreg = udc->read_fn(udc->addr + UDC_TOGRESET_REG_ADDR);
					if (target_ep->is_in)
						epcfgreg |= 1<<(target_ep->epnumber);
					else
						epcfgreg |= 1<<(target_ep->epnumber+16);
					udc->write_fn(udc->addr,UDC_TOGRESET_REG_ADDR, epcfgreg);
				}
			}
		}
		break;
	default:
		evaudc_ep0_stall(udc);
		return;
	}

	req->usb_req.length = 0;
	ret = __evaudc_ep0_queue(ep0, req);
	if (ret == 0)
		return;

	dev_err(udc->dev, "Can't respond to SET/CLEAR FEATURE\n");
	evaudc_ep0_stall(udc);
}

/**
 * evaudc_handle_setup - Processes the setup packet.
 * @udc: pointer to the usb device controller structure.
 *
 * Process setup packet and delegate to gadget layer.
 */
static void evaudc_handle_setup(struct evausb_udc *udc)
{
	struct evausb_ep *ep0 = &udc->ep0;
	struct usb_ctrlrequest setup;
	u32 *setupdat;
	//u32 dcfg;

	/* Load up the chapter 9 command buffer.*/
	setupdat = (u32 __force *) (udc->addr + SETUP_PACKET_REG_ADDR);
	memcpy(&setup, setupdat, 8);
	udc->setup = setup;
	udc->setup.wValue = cpu_to_le16(setup.wValue);
	udc->setup.wIndex = cpu_to_le16(setup.wIndex);
	udc->setup.wLength = cpu_to_le16(setup.wLength);

	/* Clear previous requests */
	evaudc_nuke(ep0, -ECONNRESET);

	if (udc->setup.bRequestType & USB_DIR_IN) {
		/* Execute the get command.*/
		udc->setupseqrx = STATUS_PHASE;
		udc->setupseqtx = DATA_PHASE;
	} else {
		/* Execute the put command.*/
		udc->setupseqrx = DATA_PHASE;
		udc->setupseqtx = STATUS_PHASE;
	}

	switch (udc->setup.bRequest) {
	case USB_REQ_GET_STATUS:
		/* Data+Status phase form udc */
		if ((udc->setup.bRequestType &
				(USB_DIR_IN | USB_TYPE_MASK)) !=
				(USB_DIR_IN | USB_TYPE_STANDARD))
			break;
		evaudc_getstatus(udc);
		return;
	case USB_REQ_SET_ADDRESS:
		/* Status phase from udc */
		if (udc->setup.bRequestType != (USB_DIR_OUT |
				USB_TYPE_STANDARD | USB_RECIP_DEVICE))
			break;
#if 0
		/* Set the address of the device.*/
		dcfg = udc->read_fn(udc->addr+UDC_FN_CTRL_REG_ADDR);
		dcfg |= (udc->setup.wValue & 0x7f) << 16;
		udc->write_fn(udc->addr, UDC_FN_CTRL_REG_ADDR, dcfg);
#else
//		printk ("don't set addr manual\n");
#endif
		evaudc_setaddress(udc);
		return;
	case USB_REQ_CLEAR_FEATURE:
	case USB_REQ_SET_FEATURE:
		/* Requests with no data phase, status phase from udc */
		if ((udc->setup.bRequestType & USB_TYPE_MASK)
				!= USB_TYPE_STANDARD)
			break;
		evaudc_set_clear_feature(udc);
		return;
	default:
		break;
	}

	spin_unlock(&udc->lock);
	if (udc->driver && udc->driver->setup(&udc->gadget, &setup) < 0)
		evaudc_ep0_stall(udc);
	spin_lock(&udc->lock);
}

/**
 * evaudc_ep0_out - Processes the endpoint 0 OUT token.
 * @udc: pointer to the usb device controller structure.
 */
static void evaudc_ep0_out(struct evausb_udc *udc)
{
	struct evausb_ep *ep0 = &udc->ep0;
	struct evausb_req *req;
	u32 dma_cur_sz;
	u32 act_len;

	ep0->is_in = false;

	if (list_empty(&ep0->queue))
		return;

	req = list_first_entry(&ep0->queue, struct evausb_req,queue);
	switch (udc->setupseqrx) {
	case STATUS_PHASE:
		/*
		 * This resets both state machines for the next
		 * Setup packet.
		 */
		udc->setupseqrx = SETUP_PHASE;
		udc->setupseqtx = SETUP_PHASE;
		req->usb_req.actual = req->usb_req.length;
		evaudc_done(ep0, req, 0);
		break;
	case DATA_PHASE:
		evaudc_wrstatus(udc);	
		dma_cur_sz = ep0->dma->dmatsize;
		act_len =  ep0->ep_usb.maxpacket - dma_cur_sz;
		req->usb_req.actual += act_len;
		
			/* Completion */
		if (req->usb_req.actual == req->usb_req.length) {
			if (udc->dma_enabled && req->usb_req.length)
				dma_sync_single_for_cpu(udc->dev,
							req->usb_req.dma,
							req->usb_req.actual,
							DMA_FROM_DEVICE);

			evaudc_done(ep0, req, 0);
			return;
		}
		evaudc_read_fifo(ep0,req);
		break;
	default:
		break;
	}
}

/**
 * evaudc_ep0_in - Processes the endpoint 0 IN token.
 * @udc: pointer to the usb device controller structure.
 */
static void evaudc_ep0_in(struct evausb_udc *udc)
{
	struct evausb_ep *ep0 = &udc->ep0;
	struct evausb_req *req;
	u32 epcfgreg = 0;
	u32 dma_cur_sz;
	u32 act_len;

	
	u8 test_mode = udc->setup.wIndex >> 8;

	ep0->is_in = true;

	if (list_empty(&ep0->queue))
		return ;
	req = list_first_entry(&ep0->queue, struct evausb_req, queue);

	switch (udc->setupseqtx) {
	case STATUS_PHASE:
		switch (udc->setup.bRequest) {
		case USB_REQ_SET_ADDRESS:
			/* Set the address of the device.*/
			epcfgreg = udc->read_fn(udc->addr+UDC_FN_CTRL_REG_ADDR);
			epcfgreg |= (udc->setup.wValue & 0x7f) << 16;
			udc->write_fn(udc->addr, UDC_FN_CTRL_REG_ADDR, epcfgreg);
			break;
		case USB_REQ_SET_FEATURE:
			if (udc->setup.bRequestType ==
					USB_RECIP_DEVICE) {
				if (udc->setup.wValue ==
						USB_DEVICE_TEST_MODE)
					epcfgreg = udc->read_fn(udc->addr + HC_PORT_CTRL_REG_ADDR);
					epcfgreg |= ((test_mode & 0x1f)<<16);
					udc->write_fn(udc->addr,HC_PORT_CTRL_REG_ADDR,epcfgreg);
			}
			break;
		}
		req->usb_req.actual = req->usb_req.length;
		evaudc_done(ep0, req, 0);
		break;
	case DATA_PHASE:
		evaudc_wrstatus(udc);
		dma_cur_sz = ep0->dma->dmatsize;
		act_len = req->real_dma_length - dma_cur_sz;
		req->usb_req.actual += act_len;
			/* Completion */
		if (req->usb_req.actual == req->usb_req.length) {
			if (udc->dma_enabled && req->usb_req.length)
				dma_sync_single_for_cpu(udc->dev,
							req->usb_req.dma,
							req->usb_req.actual,
							DMA_FROM_DEVICE);
			
			evaudc_done(ep0, req, 0);
			
			return;
		}
		evaudc_write_fifo(ep0, req);
		break;
	default:
		break;
	}
}

/**
 * evaudc_ctrl_ep_handler - Endpoint 0 interrupt handler.
 * @udc: pointer to the udc structure.
 * @intrstatus:	It's the mask value for the interrupt sources on endpoint 0.
 *
 * Processes the commands received during enumeration phase.
 */
static void evaudc_ctrl_ep_handler(struct evausb_udc *udc, u32 intrstatus)
{
	if (intrstatus & EVAUSB_STATUS_EP0_IN_IRQ_MASK)
	{
		/*clear interrupt firest*/
		udc->write_fn(udc->addr,UDC_DMA_INIRQ_REG_ADDR,EVAUSB_STATUS_EP0_IN_IRQ_MASK);
		udc->write_fn(udc->addr,UDC_EP_INIRQ_REG_ADDR,EVAUSB_STATUS_EP0_IN_IRQ_MASK);
		evaudc_ep0_in(udc);
	}
	else if (intrstatus & EVAUSB_STATUS_EP0_OUT_IRQ_MASK)
	{
		//clear the interrupt
		udc->write_fn(udc->addr,UDC_DMA_INIRQ_REG_ADDR,EVAUSB_STATUS_EP0_OUT_IRQ_MASK);
		udc->write_fn(udc->addr,UDC_EP_INIRQ_REG_ADDR,EVAUSB_STATUS_EP0_OUT_IRQ_MASK);
		evaudc_ep0_out(udc);
	}
	else
	{
		printk("no possible to arrive here ?\n");
	}
}

/**
 * evaudc_nonctrl_ep_handler - Non control endpoint interrupt handler.
 * @udc: pointer to the udc structure.
 * @epnum: End point number for which the interrupt is to be processed
 * @intrstatus:	mask value for interrupt sources of endpoints other
 *		than endpoint 0.
 *
 * Processes the buffer completion interrupts.
 */
#if 0
static void evaudc_nonctrl_ep_handler(struct evausb_udc *udc, u8 epnum,bool is_in,
				    u32 intrstatus)
{

	struct evausb_req *req;
	struct evausb_ep *ep;
	if (is_in)
		ep = &udc->ep_in[epnum];
	else
		ep = &udc->ep_out[epnum];

	if (list_empty(&ep->queue))
		return;

	req = list_first_entry(&ep->queue, struct evausb_req, queue);

	if (ep->is_in)
		evaudc_write_fifo(ep, req);
}
#endif

static void evaudc_nonctrl_ep_dma_handler(struct evausb_udc *udc, u8 epnum,bool is_in,
				    u32 intrstatus)
{
	struct evausb_req *req;
	struct evausb_ep *ep;
	u32 is_short = 0;
	u32 dma_cur_sz;
	u32 act_len;
	u32 ier;

	if (is_in)
		ep = &udc->ep_in[epnum];
	else
		ep = &udc->ep_out[epnum];

	if (list_empty(&ep->queue))
		return;

	req = list_first_entry(&ep->queue, struct evausb_req, queue);	
	dma_cur_sz = ep->dma->dmatsize;
	if (using_sof && is_in && ep->is_iso) {
		using_sof = false;

		ier = udc->read_fn(udc->addr + UDC_PING_USB_IEN_REG_ADDR);
		ier &= ~EVAUSB_STATUS_SOFIEIRQ_MASK;
		udc->write_fn(udc->addr,UDC_PING_USB_IEN_REG_ADDR,ier);
		ier = udc->read_fn(udc->addr + UDC_PING_USB_IEN_REG_ADDR);
		//printk("evaudc_nonctrl_ep_dma_handler: ier(0x%x)\n",ier);
	}
		
	if (is_in) {
		act_len =  req->real_dma_length - dma_cur_sz;
	}
	else
	{
		act_len =  ep->ep_usb.maxpacket - dma_cur_sz;
		//printk("out :act_len(0x%x) maxpacket(0x%x)\n",act_len,ep->ep_usb.maxpacket);
		if (act_len < ep->ep_usb.maxpacket)
			is_short = 1;
	}
	
	req->usb_req.actual += act_len;
	evaudc_handle_unaligned_buf_complete(ep->udc,ep,req);

	/* Completion */
	if (is_in) {
		if (req->usb_req.actual == req->usb_req.length) {
			if (udc->dma_enabled && req->usb_req.length)
				dma_sync_single_for_cpu(udc->dev,
							req->usb_req.dma,
							req->usb_req.actual,
							DMA_TO_DEVICE);

			evaudc_done(ep, req, 0);
		}
	}
	else
	{
		if (req->usb_req.actual == req->usb_req.length || is_short) {
			if (udc->dma_enabled && req->usb_req.length)
				dma_sync_single_for_cpu(udc->dev,
							req->usb_req.dma,
							req->usb_req.actual,
							DMA_FROM_DEVICE);

			evaudc_done(ep, req, 0);
		}
	}

	if (list_empty(&ep->queue))
		return;

	req = list_first_entry(&ep->queue, struct evausb_req, queue);

	if (is_in)
		evaudc_write_fifo(ep, req);
	else
		evaudc_read_fifo(ep, req);

	return;
}

/**
 * evaudc_irq - The main interrupt handler.
 * @irq: The interrupt number.
 * @_udc: pointer to the usb device controller structure.
 *	After servicing the core\u2019s interrupt, the microprocessor clears the individual interrupt request flag in the core\u2019s
 *	SFRs. If any other USB interrupts are pending, the act of clearing the interrupt request flag causes the core
 *	to generate another pulse for the highest priority pending interrupt. If more than one interrupt is pending,
 *	each is serviced in the priority order shown in Table 2-16.
 *	note: The sequence of clearing interrupt requests is important. The microprocessor should first clear the
 *	main interrupt request flag (the usbintreq request) and then the individual interrupt requests in the core\u2019s
 *	register
 * Return: IRQ_HANDLED after the interrupt is handled.
 */
static irqreturn_t evaudc_irq(int irq, void *_udc)
{
	struct evausb_udc *udc = _udc;
	u32 intrstatus;
	u8 index;
	u32 epx_dmairq_in,epx_dmairq_out;
	u32 epx_irq_in,epx_irq_out;
	unsigned long flags;
	u32 dma_intrstatus;
	u32 crtlreg;
	u32 ier;
	static volatile u32 read_data = 0;
	struct evausb_ep *eva_ep;
	struct evausb_req *req;

	spin_lock_irqsave(&udc->lock, flags);
	/* Read the ping&usb  Interrupt Request Register.*/
	intrstatus = udc->read_fn(udc->addr + PING_USB_IRQ_REG_ADDR);
	if (intrstatus & (EVAUSB_STATUS_URESIEIRQ_MASK | EVAUSB_STATUS_SUSPIEIRQ_MASK)) {
		evaudc_startup_handler(udc, intrstatus);
	}
	if (intrstatus & EVAUSB_STATUS_SUDAVIEIRQ_MASK) {
		//clear the interrupt
		udc->write_fn(udc->addr,PING_USB_IRQ_REG_ADDR,EVAUSB_STATUS_SUDAVIEIRQ_MASK);
		evaudc_handle_setup(udc);
	}
	
	if (intrstatus & EVAUSB_STATUS_SUTOKEIEIRQ_MASK)
	{
		//clear the interrupt
		udc->write_fn(udc->addr,PING_USB_IRQ_REG_ADDR,EVAUSB_STATUS_SUTOKEIEIRQ_MASK);
	}
	if (intrstatus & EVAUSB_STATUS_OVERFLOWIEIRQ_MASK)
	{
		/*the core sends a NAK handshake when it works as a USB peripheral device, and
		When a FIFO overflow error occurs, the overflowir bit is set in the usbirq register.
		USB transfer for all OUT endpoints is stopped.Transfer is resumed when the overflowir flag is cleared by the CPU*/
		udelay(10);
		//clear the interrupt
		udc->write_fn(udc->addr,PING_USB_IRQ_REG_ADDR,EVAUSB_STATUS_OVERFLOWIEIRQ_MASK);
	}

	if (intrstatus & EVAUSB_STATUS_HSPEEDIEIRQ_MASK)
	{
		udc->write_fn(udc->addr,PING_USB_IRQ_REG_ADDR,EVAUSB_STATUS_HSPEEDIEIRQ_MASK);
		udc->gadget.speed = USB_SPEED_HIGH;
	}

	/* Read the EP Interrupt Request Register.*/
	intrstatus = udc->read_fn(udc->addr + UDC_EP_INIRQ_REG_ADDR);
	dma_intrstatus = udc->read_fn(udc->addr + UDC_DMA_INIRQ_REG_ADDR);
	crtlreg = udc->read_fn(udc->addr + UDC_USB_CTRL_REG_ADDR);
	//printk("ep :intrstatus 0x%x dma_intrstatus 0x%x dma&usb ivec:0x%x\n",intrstatus,dma_intrstatus,crtlreg&0xff);
	if ((dma_intrstatus & 0xffffff))
	{
		for (index = 1; index < EVAUSB_MAX_ENDPOINTS; index++) {
			epx_dmairq_out = (dma_intrstatus &(EVAUSB_STATUS_EP1_OUT_IRQ_MASK <<(index - 1)));
			epx_dmairq_in = (dma_intrstatus &(EVAUSB_STATUS_EP1_IN_IRQ_MASK <<(index - 1)));

			if(epx_dmairq_out) {
				//clear the interrupt first
				//printk("ep :intrstatus 0x%x dma_intrstatus 0x%x dma&usb ivec:0x%x \n",intrstatus,dma_intrstatus,crtlreg);
				udc->write_fn(udc->addr,UDC_DMA_INIRQ_REG_ADDR,epx_dmairq_out);
				evaudc_nonctrl_ep_dma_handler(udc, index,false,
							dma_intrstatus);
			}
			if(epx_dmairq_in) {
				//clear the interrupt first
				//printk("ep :intrstatus 0x%x dma_intrstatus 0x%x dma&usb ivec:0x%x \n",intrstatus,dma_intrstatus,crtlreg);
				udc->write_fn(udc->addr,UDC_DMA_INIRQ_REG_ADDR,epx_dmairq_in);
				evaudc_nonctrl_ep_dma_handler(udc, index,true,
							dma_intrstatus);
			}
		}
	}
	if ((intrstatus & 0xffffff))
	{
		if (intrstatus & (EVAUSB_STATUS_EP0_IN_IRQ_MASK | EVAUSB_STATUS_EP0_OUT_IRQ_MASK))
		{	//clear and handle interrupts
			evaudc_ctrl_ep_handler(udc, intrstatus);
		}
		for (index = 1; index < EVAUSB_MAX_ENDPOINTS; index++) {
			epx_irq_in = (intrstatus &(EVAUSB_STATUS_EP1_IN_IRQ_MASK <<(index - 1))) ;
			epx_irq_out = (intrstatus &(EVAUSB_STATUS_EP1_OUT_IRQ_MASK <<(index - 1)));
			if (epx_irq_in ) {
				//clear the interrupt first
				udc->write_fn(udc->addr,UDC_EP_INIRQ_REG_ADDR,epx_irq_in);
#if 0
				evaudc_nonctrl_ep_handler(udc, index,true,
							intrstatus);
#endif
			}
			else if(epx_irq_out ) {
				//clear the interrupt first
				udc->write_fn(udc->addr,UDC_EP_INIRQ_REG_ADDR,epx_irq_out);
#if 0
				evaudc_nonctrl_ep_handler(udc, index,false,
							intrstatus);
#endif
			}
		}
	}
	intrstatus = udc->read_fn(udc->addr + PING_USB_IRQ_REG_ADDR);
	if (intrstatus & EVAUSB_STATUS_SOFIEIRQ_MASK)
	{
		//clear the interrupt
		udc->write_fn(udc->addr,PING_USB_IRQ_REG_ADDR,EVAUSB_STATUS_SOFIEIRQ_MASK);

		if (using_sof)
		{
			//struct evausb_ep *eva_ep;
			//struct evausb_req *req;
			ier = udc->read_fn(udc->addr + UDC_PING_USB_IEN_REG_ADDR);
			if ((ier & EVAUSB_STATUS_SOFIEIRQ_MASK) == 0) {
				ier |= EVAUSB_STATUS_SOFIEIRQ_MASK;
				udc->write_fn(udc->addr,UDC_PING_USB_IEN_REG_ADDR,ier);
			}

			if (g_ep1in)
			{
				eva_ep = to_evausb_ep(g_ep1in);
				if (!list_empty(&eva_ep->queue)){
	
					req = list_first_entry(&eva_ep->queue, struct evausb_req, queue);
					read_data ++;
					if (eva_ep->is_in && (read_data%8) == 0)
						{
							evaudc_write_fifo(eva_ep, req);
						}
				}
			}
		}
	}

	spin_unlock_irqrestore(&udc->lock, flags);
	return IRQ_HANDLED;
}

/**
 * evaudc_probe - The device probe function for driver initialization.
 * @pdev: pointer to the platform device structure.
 *
 * Return: 0 for success and error value on failure
 */
static int evaudc_probe(struct platform_device *pdev)
{
#define IRQ_NUMBER 3
	struct device_node *np = pdev->dev.of_node;
	struct resource *res;
	struct evausb_udc *udc;
	int irq[IRQ_NUMBER];
	int ret;
//	u32 crtlreg;
	u8 *buff;
	int i = 100;
	void *config_addr;

#define CONFIG_ADDR 0x0030a000
	config_addr = ioremap(CONFIG_ADDR, 0x10);

    *(volatile unsigned int*)config_addr |=  (1<<10);
    while(i--) __asm__ __volatile__( "nop" );
    *(volatile unsigned int*)config_addr &= ~(1<<10);

	iounmap(config_addr);

	udc = devm_kzalloc(&pdev->dev, sizeof(*udc), GFP_KERNEL);
	if (!udc)
		return -ENOMEM;

	/* Create a dummy request for GET_STATUS, SET_ADDRESS */
	udc->req = devm_kzalloc(&pdev->dev, sizeof(struct evausb_req),
				GFP_KERNEL);
	if (!udc->req)
		return -ENOMEM;

	buff = devm_kzalloc(&pdev->dev, STATUSBUFF_SIZE, GFP_KERNEL);
	if (!buff)
		return -ENOMEM;

	udc->req->usb_req.buf = buff;

	/* Map the registers */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	udc->addr = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(udc->addr))
	{
		dev_err(&pdev->dev,"error occur : udc->addr(%p) is error\n",udc->addr);
		return PTR_ERR(udc->addr);
	}

	for (i = 0; i < IRQ_NUMBER; i++) {
		irq[i] = platform_get_irq(pdev, i);
		if (irq[i] < 0) {
			dev_err(&pdev->dev, "unable to get irq\n");
			return irq[i];
		}
	}

	for (i = 0; i < IRQ_NUMBER; i++) {
		ret = devm_request_irq(&pdev->dev, irq[i], evaudc_irq, 0,
				dev_name(&pdev->dev), udc);
		if (ret < 0) {
			dev_dbg(&pdev->dev, "unable to request irq[%d] %d", i, irq[i]);
			goto fail;
		}
	}

	udc->dma_enabled = of_property_read_bool(np, "eva,has-builtin-dma");

	/* Setup gadget structure */
	udc->gadget.ops = &evausb_udc_ops;
	udc->gadget.max_speed = USB_SPEED_HIGH;
	udc->gadget.speed = USB_SPEED_UNKNOWN;
	udc->gadget.ep0 = &udc->ep0.ep_usb;
	udc->gadget.name = driver_name;

	spin_lock_init(&udc->lock);

	/* Check for IP endianness */
	udc->write_fn = evaudc_write32_be;
	udc->read_fn = evaudc_read32_be;

	udc->write_fn(udc->addr, HC_PORT_CTRL_REG_ADDR, TEST_J);
	if ((udc->read_fn(udc->addr + HC_PORT_CTRL_REG_ADDR))
			== TEST_J) {
		udc->write_fn = evaudc_write32;
		udc->read_fn = evaudc_read32;
	}
	udc->write_fn(udc->addr, HC_PORT_CTRL_REG_ADDR, 0x40);

	evaudc_eps_init(udc);
	udc->dma = (volatile struct general_dma*)(udc->addr + UDC_DMASTART);
	udc->usb_irq = (volatile struct eva_usb_irq*)(udc->addr + UDC_EP_INIRQ_REG_ADDR);
	udc->fnaddr = (volatile u8*)(udc->addr + UDC_FNADDR_REG_ADDR);
	udc->usbcs = (volatile u8*)(udc->addr + USBCS_REG_ADDR);

	/* Set device address to 0.*/
	*udc->fnaddr = 0x0;

#if 0
	crtlreg = udc->read_fn(udc->addr + UDC_USB_CTRL_REG_ADDR);
	crtlreg |= EVAUSB_CONTROL_USB_READY_MASK;
	udc->write_fn(udc->addr, UDC_USB_CTRL_REG_ADDR, crtlreg);
#endif

	ret = usb_add_gadget_udc(&pdev->dev, &udc->gadget);
	if (ret)
		goto fail;

	udc->dev = &udc->gadget.dev;
	platform_set_drvdata(pdev, udc);

	dev_vdbg(&pdev->dev, "%s at 0x%08X mapped to %p %s\n",
		 driver_name, (u32)res->start, udc->addr,
		 udc->dma_enabled ? "with DMA" : "without DMA");

	//eva_ctrl_dev = &pdev->dev;
	return 0;
fail:
	dev_err(&pdev->dev, "probe failed, %d\n", ret);
	return ret;
}

/**
 * evaudc_remove - Releases the resources allocated during the initialization.
 * @pdev: pointer to the platform device structure.
 *
 * Return: 0 always
 */
static int evaudc_remove(struct platform_device *pdev)
{
	struct evausb_udc *udc = platform_get_drvdata(pdev);
	usb_del_gadget_udc(&udc->gadget);

	return 0;
}

/* Match table for of_platform binding */
static const struct of_device_id usb_of_match[] = {
	{ .compatible = "gx-eva,gx-eva-usb2-device", },
	{ /* end of list */ },
};
MODULE_DEVICE_TABLE(of, usb_of_match);

static struct platform_driver evaudc_driver = {
	.driver = {
		.name = driver_name,
		.of_match_table = usb_of_match,
	},
	.probe = evaudc_probe,
	.remove = evaudc_remove,
};

module_platform_driver(evaudc_driver);

MODULE_DESCRIPTION("eva udc driver");
MODULE_AUTHOR("gx, Inc");
MODULE_LICENSE("GPL");
