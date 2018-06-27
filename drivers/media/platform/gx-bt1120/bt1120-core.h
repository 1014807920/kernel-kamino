#ifndef __BT1120_CORE_H__
#define __BT1120_CORE_H__

#include <linux/io.h>
#include <linux/irq.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/videodev2.h>
#include <linux/kfifo.h>
#include <linux/interrupt.h>

#include <media/media-entity.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-dev.h>
#include <media/v4l2-device.h>
#include <media/v4l2-mediabus.h>
#include <media/videobuf2-v4l2.h>
#include <media/s3c_camif.h>

#define GX_BT1120_DRIVER_NAME "gx-bt1120"
#define BT1120_REQ_BUFS_MIN   2
#define BT1120_REQ_BUFS_MAX   4

struct bt1120_fmt {
	char *description;
	u32  fourcc;
	u8   ybpp;
};

struct bt1120_clk_info {
#define MXA_BT1120_CLK_NUM (2)
	unsigned int  num;
	const char    *name[MXA_BT1120_CLK_NUM];
};

struct bt1120_reg_info {
#define MAX_BT1120_REG_NUM (2)
	unsigned int num;
	char *name[MAX_BT1120_REG_NUM];
	unsigned int length[MAX_BT1120_REG_NUM];
	unsigned int baseAddr[MAX_BT1120_REG_NUM];
};

struct bt1120_irq_info {
#define MAX_BT1120_IRQ_NUM (1)
	unsigned int num;
	char *name[MAX_BT1120_IRQ_NUM];
	unsigned int irq[MAX_BT1120_IRQ_NUM];
	unsigned int irqFlag[MAX_BT1120_IRQ_NUM];
};

struct bt1120_yuv_addr {
	long unsigned int ddr_stat;
	long unsigned int y_buf;
	long unsigned int u_buf;
	long unsigned int v_buf;
};

struct bt1120_reg {
	long unsigned int rBT1120_CTRL;
	long unsigned int rBT1120_INI_EN;
	long unsigned int rBT1120_INI_STAT;
	long unsigned int rBT1120_FIFO_THR;
	long unsigned int rBT1120_DATA_IN;
	long unsigned int rBT1120_FRAME_SIZE;
	long unsigned int rBT1120_Y_SIZE;
	long unsigned int rBT1120_SCALE_SRC_SIZE;
	long unsigned int rBT1120_SCALE_DST_SIZE;
	long unsigned int rBT1120_CLIP_CFG_X;
	long unsigned int rBT1120_CLIP_CFG_Y;
	long unsigned int rBT1120_WAIT_MODE;
	long unsigned int rBT1120_DOWNSAMP_STEP;
	long unsigned int rBT1120_SCALE_H_STEP;
	long unsigned int rBT1120_SCALE_V_STEP;
	long unsigned int rBT1120_SCALE_SIGN;
	struct bt1120_yuv_addr rBT1120_YUV_ADDR[4];
	struct bt1120_yuv_addr rBT1120_SCALE_ADDR[4];
	long unsigned int rBT1120_ZOOMOUT_COEF;
	long unsigned int rBT1120_REGS_REFUSH;
	long unsigned int reserve1[2];
	long unsigned int rBT1120_SCALE_COEF[64];
};

struct bt1120_reset_reg {
	long unsigned int rRESET;
	long unsigned int rRESERVE[5];
	long unsigned int rCLK_CTRL;
};

struct cam_sensor {
	struct v4l2_subdev *sd;
	struct v4l2_fract  timeperframe;
	unsigned int width;
	unsigned int height;
};

struct bt1120_frame {
	unsigned int f_width;
	unsigned int f_height;
	bool         enable_crop;
	bool         enable_scale;
	struct v4l2_rect  crop;
	struct bt1120_fmt *fmt;
	unsigned int sizeimage;
	unsigned int y_size;
	unsigned int uv_size;
};

struct bt1120_phys_addr {
	void *y_addr;
	void *u_addr;
	void *v_addr;
};

struct bt1120_virt_addr {
	void *y_addr;
	void *u_addr;
	void *v_addr;
};

struct bt1120_hw_addr {
	struct bt1120_virt_addr virt_addr;
	struct bt1120_virt_addr align_virt_addr;

	bool y_need_align;
	bool u_need_align;
	bool v_need_align;

	bool full;
	struct list_head list;
	unsigned int index;
};

struct bt1120_buffer {
	struct vb2_v4l2_buffer vb;
	struct list_head list;
	struct bt1120_phys_addr phys_addr;
};

enum bt1120_state {
	BT1120_READY,
	BT1120_RUN,
	BT1120_STOP,
};

struct bt1120_dev {
	struct device           *dev;

	struct v4l2_device      v4l2_dev;
	struct video_device     vdev;
	struct vb2_queue        vb_queue;
	struct v4l2_fh          *owner;
	struct v4l2_clk         *clk[3];
	struct v4l2_ctrl_handler hdl;
	struct vb2_alloc_ctx    *alloc_ctx;

	struct bt1120_reg       *reg;
	struct bt1120_reset_reg *reset_reg;
	struct bt1120_frame     out_frame;
	struct bt1120_clk_info  *clk_info;
	struct bt1120_hw_addr   *hw_addr;
	struct cam_sensor       sensor;
	enum bt1120_state       state;

	struct mutex            lock;
	struct list_head        idle_buf_q; //记录空闲buffer链表
	struct list_head        busy_buf_q; //记录空闲buffer链表
	struct tasklet_struct   tasklet;

	unsigned int            i2c_bus;
	unsigned int            reqbufs_count;
	bool                    is_open;
};

static inline void bt1120_idle_queue_add(struct bt1120_dev *bt1120,
					struct bt1120_buffer *buf)
{
	list_add_tail(&buf->list, &bt1120->idle_buf_q);
}

static inline struct bt1120_buffer *bt1120_idle_queue_pop(
					struct bt1120_dev *bt1120)
{
	struct bt1120_buffer *buf = list_first_entry(&bt1120->idle_buf_q,
							struct bt1120_buffer, list);

	list_del(&buf->list);
	return buf;
}

static inline void bt1120_busy_queue_add(struct bt1120_dev *bt1120,
					struct bt1120_hw_addr *hw_addr)
{
	list_add_tail(&hw_addr->list, &bt1120->busy_buf_q);
}

static inline struct bt1120_hw_addr *bt1120_busy_queue_pop(
					struct bt1120_dev *bt1120)
{
	struct bt1120_hw_addr *hw_addr = list_first_entry(&bt1120->busy_buf_q,
							struct bt1120_hw_addr, list);

	list_del(&hw_addr->list);
	return hw_addr;
}

extern int bt1120_register_video_nodes(struct bt1120_dev *bt1120);
extern irqreturn_t bt1120_interrupt(int irq, void *arg);
extern int bt1120_register_sensor(struct bt1120_dev *bt1120);
extern void bt1120_update_video_buf(unsigned long data);
extern void gx_bt1120_clk_enable(struct bt1120_dev *bt1120);
extern void gx_bt1120_clk_disable(struct bt1120_dev *bt1120);
extern int gx_bt1120_hw_init(struct bt1120_dev *bt1120);

#endif
