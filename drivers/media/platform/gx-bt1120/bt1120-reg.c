#include <linux/delay.h>
#include <linux/bitops.h>
#include <linux/io.h>

#include "bt1120-core.h"
#include "bt1120-reg.h"

static unsigned int scale_coef[] = {
	0x00ff0100, 0x01ff0200, 0x02ff0300, 0x03ff0400,
	0x04ff0500, 0x05fe0700, 0x06fe0800, 0x07fd0a00,
	0x07fc0b00, 0x08fc0c00, 0x09fb0e00, 0x09fa1001,
	0x0af91201, 0x0bf81401, 0x0bf71501, 0x0cf61701,
	0x0cf51801, 0x0df41b02, 0x0df31c02, 0x0ef11f02,
	0x0ef02002, 0x0fef2303, 0x0fed2503, 0x10ec2703,
	0x10ea2903, 0x10e92b04, 0x10e72d04, 0x11e53004,
	0x11e33305, 0x11e13505, 0x11df3705, 0x12de3a06,
	0x12dc3c06, 0x12da3e06, 0x12d74106, 0x12d54407,
	0x12d34607, 0x12d14807, 0x12cf4b08, 0x12cd4d08,
	0x12ca5008, 0x12c85309, 0x12c65509, 0x12c35809,
	0x12c15b0a, 0x12bf5d0a, 0x12bc600a, 0x12ba630b,
	0x12b7660b, 0x12b5680b, 0x12b26b0b, 0x12b06e0c,
	0x12ad710c, 0x12aa750d, 0x11a8760d, 0x11a5790d,
	0x11a27d0e, 0x11a07f0e, 0x119d820e, 0x109a840e,
	0x1098870f, 0x10958a0f, 0x10928d0f, 0x10909010,
};

static void *_align_8_bytes(void *addr)
{
	unsigned int buff_addr = (unsigned int)addr;

	if (buff_addr % 8) {
		buff_addr = ((buff_addr / 8) + 1) * 8;
	}

	return (void*)buff_addr;
}

static inline void __set_field(int val, int mask, int offset, volatile long unsigned int *reg)
{
	unsigned int tmp = readl(reg);

	tmp &= ~((mask)<<(offset));
	tmp |= ((val)&(mask))<<(offset);
	writel(tmp, reg);
}

void bt1120_reg_refresh(struct bt1120_reg *reg)
{
	__set_bit(0, &reg->rBT1120_REGS_REFUSH);
}

int bt1120_request_run(struct bt1120_dev *bt1120)
{
	struct bt1120_reg *reg = bt1120->reg;
	__clear_bit(0, &reg->rBT1120_WAIT_MODE);

	return 0;
}

int bt1120_request_stop(struct bt1120_dev *bt1120)
{
	struct bt1120_reg *reg = bt1120->reg;
	unsigned int retry = 0;

	__set_bit(0, &reg->rBT1120_WAIT_MODE);
	while (!test_bit(8, &reg->rBT1120_WAIT_MODE)) {
		retry++;
		mdelay(10);
		if (retry > 50) {
			v4l2_err(&bt1120->v4l2_dev, "bt1120 busy!\n");
			return -1;
		}
	}

	return 0;
}

int bt1120_hw_reset(struct bt1120_dev *bt1120)
{
	struct bt1120_reset_reg *reset_reg = bt1120->reset_reg;

	bt1120_request_stop(bt1120);

	__set_bit(9, &reset_reg->rRESET);
	mdelay(1);
	__clear_bit(9, &reset_reg->rRESET);

	return 0;
}

int bt1120_set_frame_done_hold(struct bt1120_dev *bt1120, bool enable)
{
	struct bt1120_reg *reg = bt1120->reg;

	if (enable)
		__set_bit(29, &reg->rBT1120_CTRL);
	else
		__clear_bit(29, &reg->rBT1120_CTRL);

	return 0;
}

int bt1120_set_scale_coef(struct bt1120_dev *bt1120)
{
	struct bt1120_reg *reg = bt1120->reg;
	unsigned int i = 0;

	writel(9, &reg->rBT1120_SCALE_SIGN);
	for (i = 0; i < 64; i++)
		writel(scale_coef[i], &reg->rBT1120_SCALE_COEF[i]);

	bt1120_reg_refresh(reg);

	return 0;
}

int bt1120_set_zoom_coef(struct bt1120_dev *bt1120)
{
	struct bt1120_reg *reg = bt1120->reg;

	writel(0x00ff0000, &reg->rBT1120_ZOOMOUT_COEF);
	bt1120_reg_refresh(reg);

	return 0;
}

static inline void bt1120_set_sync_mode(struct bt1120_reg *reg,
						enum bt1120_sync_mode mode)
{
	if (mode == SYNC_OUTSIDE)
		__set_field(0x5, 0x7, 9, &reg->rBT1120_CTRL);
	else
		__clear_bit(9, &reg->rBT1120_CTRL);
}

static inline void bt1120_set_bit_width(struct bt1120_reg *reg,
						enum bt1120_bit_width bit_width)
{
	if (bit_width == INPUT_8BIT)
		__clear_bit(8, &reg->rBT1120_CTRL);
	else
		__set_bit(8, &reg->rBT1120_CTRL);
}

static inline void bt1120_set_bit_swap(struct bt1120_reg *reg, enum bt1120_version version)
{
	if (version == BT1120_NRE)
		__set_field(0x1, 0x7, 12, &reg->rBT1120_CTRL);
	else
		__set_field(0x5, 0x7, 12, &reg->rBT1120_CTRL);
}

int bt1120_set_input(struct bt1120_dev *bt1120)
{
	struct bt1120_reg *reg = bt1120->reg;
	struct bt1120_fmt *fmt = bt1120->out_frame.fmt;

	bt1120_set_sync_mode(reg, SYNC_OUTSIDE);
	if (fmt->ybpp == 1)
		bt1120_set_bit_width(reg, INPUT_8BIT);
	else
		bt1120_set_bit_width(reg, INPUT_16BIT);

#ifdef CONFIG_ARCH_LEO
	bt1120_set_bit_swap(reg, BT1120_NRE);
#else
	bt1120_set_bit_swap(reg, BT1120_MPW);
#endif
	bt1120_reg_refresh(reg);

	return 0;
}

static inline void bt1120_set_output_format(struct bt1120_reg *reg, enum bt1120_yuv_fmt yuv_fmt)
{
	if (yuv_fmt == YUV420)
		__clear_bit(16, &reg->rBT1120_CTRL);
	else
		__set_bit(16, &reg->rBT1120_CTRL);
}

static inline void bt1120_set_yuv_channel(struct bt1120_reg *reg, bool enable)
{
	if (enable)
		__set_field(0x0, 0x7, 27, &reg->rBT1120_YUV_ADDR[0].ddr_stat);
	else
		__set_field(0x7, 0x7, 27, &reg->rBT1120_YUV_ADDR[0].ddr_stat);
}

static inline void bt1120_set_syuv_channel(struct bt1120_reg *reg, bool enable)
{
	if (enable)
		__set_field(0x0, 0x7, 24, &reg->rBT1120_YUV_ADDR[0].ddr_stat);
	else
		__set_field(0x7, 0x7, 24, &reg->rBT1120_YUV_ADDR[0].ddr_stat);
}

static inline void bt1120_set_y_size(struct bt1120_reg *reg,
					unsigned int width, unsigned int height)
{
	unsigned int value = width + (height << 16);

	writel(value, &(reg->rBT1120_Y_SIZE));
}

static inline void bt1120_set_fifo_thr(struct bt1120_reg *reg, enum bt1120_fifo_gate gate)
{
	__set_field(gate, 0x3, 20, &reg->rBT1120_FIFO_THR);
	__set_field(gate, 0x3, 16, &reg->rBT1120_FIFO_THR);
	__set_field(gate, 0x3, 12, &reg->rBT1120_FIFO_THR);
}

static inline void bt1120_set_fifo_urgent_thr(struct bt1120_reg *reg, enum bt1120_fifo_gate gate)
{
	__set_field(gate, 0x3, 22, &reg->rBT1120_FIFO_THR);
	__set_field(gate, 0x3, 18, &reg->rBT1120_FIFO_THR);
	__set_field(gate, 0x3, 14, &reg->rBT1120_FIFO_THR);
}

int bt1120_set_output(struct bt1120_dev *bt1120)
{
	struct bt1120_reg *reg = bt1120->reg;
	struct bt1120_fmt *fmt = bt1120->out_frame.fmt;

	if (fmt->fourcc == V4L2_PIX_FMT_YUV420)
		bt1120_set_output_format(reg, YUV420);
	else
		bt1120_set_output_format(reg, YUV422);

	bt1120_set_yuv_channel(reg, true);
	bt1120_set_syuv_channel(reg, false);
	bt1120_set_y_size(reg, bt1120->sensor.width, bt1120->sensor.height);
	bt1120_set_fifo_thr(reg, NO_GATE);
	bt1120_set_fifo_urgent_thr(reg, NO_GATE);

	bt1120_reg_refresh(reg);

	return 0;
}

static inline void bt1120_set_crop_enable(struct bt1120_reg *reg, bool enable)
{
	if (enable)
		__set_bit(28, &reg->rBT1120_CTRL);
	else
		__clear_bit(28, &reg->rBT1120_CTRL);
}

static void bt1120_calculate_coordinate(struct bt1120_frame *frame, unsigned int *x_bg,
					unsigned int *x_ed, unsigned int *y_bg, unsigned int *y_ed)
{
	if (frame->crop.left % 2)
		*x_bg = frame->crop.left - 1;
	else
		*x_bg = frame->crop.left;

	if (frame->crop.top % 2)
		*y_bg = frame->crop.top - 1;
	else
		*y_bg = frame->crop.top;

	if ((frame->crop.left + frame->crop.width) % 2)
		*x_ed = frame->crop.left + frame->crop.width;
	else
		*x_ed = frame->crop.left + frame->crop.width - 1;

	if ((frame->crop.top + frame->crop.height) % 2)
		*y_ed = frame->crop.top + frame->crop.height;
	else
		*y_ed = frame->crop.top + frame->crop.height - 1;
}

static inline void bt1120_set_crop_cfg_x(struct bt1120_reg *reg, unsigned int x_bg,
					unsigned int x_end)
{
	writel(((x_bg) | (x_end << 16)), &reg->rBT1120_CLIP_CFG_X);
}

static inline void bt1120_set_crop_cfg_y(struct bt1120_reg *reg, unsigned int y_bg,
					unsigned int y_end)
{
	writel(((y_bg) | (y_end << 16)), &reg->rBT1120_CLIP_CFG_Y);
}

int bt1120_set_crop(struct bt1120_dev *bt1120)
{
	struct bt1120_reg *reg = bt1120->reg;
	struct bt1120_frame *frame = &bt1120->out_frame;
	unsigned int x_bg = 0, x_ed = 0;
	unsigned int y_bg = 0, y_ed = 0;

	if (frame->enable_crop) {
		bt1120_calculate_coordinate(frame, &x_bg, &x_ed, &y_bg, &y_ed);
		bt1120_set_crop_cfg_x(reg, x_bg, x_ed);
		bt1120_set_crop_cfg_y(reg, y_bg, y_ed);
	}

	bt1120_set_crop_enable(reg, frame->enable_crop);
	bt1120_reg_refresh(reg);

	return 0;
}

#define BT1120_CALC_STEP(src, dst, downsamp, scale) \
	do { \
		unsigned int tmp = 0; \
		if (src / dst > 4 || (src / dst == 4 && src % dst != 0)) { \
			tmp = src / dst + ((src % dst) ? 1 : 0); \
			downsamp = tmp / 4; \
			src /= (downsamp + 1); \
		} \
		scale = src * 4096 / dst; \
	} while(0);

static inline void bt1120_set_scale_src_size(struct bt1120_reg *reg,
					unsigned int src_w, unsigned int src_h)
{
	__set_field(src_w, 0x7ff, 0, &reg->rBT1120_SCALE_SRC_SIZE);
	__set_field(src_h, 0x7ff, 16, &reg->rBT1120_SCALE_SRC_SIZE);
}

static inline void bt1120_set_scale_dst_size(struct bt1120_reg *reg,
					unsigned int dst_w, unsigned int dst_h)
{
	__set_field(dst_w, 0x7ff, 0, &reg->rBT1120_SCALE_DST_SIZE);
	__set_field(dst_h, 0x7ff, 16, &reg->rBT1120_SCALE_DST_SIZE);
}

static inline void bt1120_set_downsamp_step(struct bt1120_reg *reg,
					unsigned int downsamp_hstep, unsigned int downsamp_vstep)
{
	__set_field(downsamp_hstep, 0x1f, 0, &reg->rBT1120_DOWNSAMP_STEP);
	__set_field(downsamp_vstep, 0x1f, 7, &reg->rBT1120_DOWNSAMP_STEP);
}

static inline void bt1120_set_scale_step(struct bt1120_reg *reg,
					unsigned int scale_hstep, unsigned int scale_vstep)
{
	writel(scale_hstep, &(reg->rBT1120_SCALE_H_STEP));
	writel(scale_vstep, &(reg->rBT1120_SCALE_V_STEP));
}

static inline void bt1120_set_scale_enable(struct bt1120_reg *reg, bool enable)
{
	if (enable)
		__set_bit(24, &reg->rBT1120_CTRL);
	else
		__clear_bit(24, &reg->rBT1120_CTRL);
}

int bt1120_set_scale(struct bt1120_dev *bt1120)
{
	unsigned int src_w = 0;
	unsigned int src_h = 0;
	unsigned int dst_w = bt1120->out_frame.f_width;
	unsigned int dst_h = bt1120->out_frame.f_height;
	unsigned int downsamp_hstep = 0, downsamp_vstep = 0;
	unsigned int scale_hstep = 0, scale_vstep = 0;
	struct bt1120_reg *reg = bt1120->reg;
	int ret = 0;

	if (!bt1120->out_frame.enable_scale) {
		goto disable_scale;
	}

	if (bt1120->out_frame.enable_scale) {
		src_w = bt1120->out_frame.crop.width;
		src_h = bt1120->out_frame.crop.height;
	} else {
		src_w = bt1120->sensor.width;
		src_h = bt1120->sensor.height;
	}

	if (dst_w > src_w || dst_h > src_h) {
		ret = -1;
		goto disable_scale;
	}

	BT1120_CALC_STEP(src_w, dst_w, downsamp_hstep, scale_hstep);
	BT1120_CALC_STEP(src_h, dst_h, downsamp_vstep, scale_vstep);

	bt1120_set_scale_src_size(reg, src_w, src_h);
	bt1120_set_scale_dst_size(reg, dst_w, dst_h);
	bt1120_set_downsamp_step(reg, downsamp_hstep, downsamp_vstep);
	bt1120_set_scale_step(reg, scale_hstep, scale_vstep);

	bt1120_set_scale_enable(reg, true);
	bt1120_reg_refresh(reg);

	return ret;

disable_scale:
	bt1120_set_scale_enable(reg, false);
	return ret;
}

static inline void bt1120_set_hw_buffer_num(struct bt1120_reg *reg,
						enum bt1120_hw_buffer_type type, unsigned int num)
{
	if (type == RAW_FRAME)
		__set_field((num - 1), 0x3, 4, &reg->rBT1120_YUV_ADDR[0].ddr_stat);
	else
		__set_field((num - 1), 0x3, 12, &reg->rBT1120_YUV_ADDR[0].ddr_stat);
}

static inline void bt1120_set_hw_buffer_addr(struct bt1120_reg *reg, enum bt1120_hw_buffer_type type,
						struct bt1120_hw_addr *hw_addr, unsigned int num)
{
	int i = 0;

	if (type == RAW_FRAME) {
		for (i = 0; i < num; i++) {
			writel(virt_to_phys(hw_addr[i].align_virt_addr.y_addr), &reg->rBT1120_YUV_ADDR[i].y_buf);
			writel(virt_to_phys(hw_addr[i].align_virt_addr.u_addr), &reg->rBT1120_YUV_ADDR[i].u_buf);
			writel(virt_to_phys(hw_addr[i].align_virt_addr.v_addr), &reg->rBT1120_YUV_ADDR[i].v_buf);
		}
	} else {
		for (i = 0; i < num; i++) {
			writel(virt_to_phys(hw_addr[i].align_virt_addr.y_addr), &reg->rBT1120_SCALE_ADDR[i].y_buf);
			writel(virt_to_phys(hw_addr[i].align_virt_addr.u_addr), &reg->rBT1120_SCALE_ADDR[i].u_buf);
			writel(virt_to_phys(hw_addr[i].align_virt_addr.v_addr), &reg->rBT1120_SCALE_ADDR[i].v_buf);
		}
	}
}

static inline void bt1120_set_scale_buffer_channel(struct bt1120_reg *reg, bool enable)
{
	if (enable) {
		__clear_bit(24, &reg->rBT1120_YUV_ADDR[0].ddr_stat);
		__clear_bit(25, &reg->rBT1120_YUV_ADDR[0].ddr_stat);
		__clear_bit(26, &reg->rBT1120_YUV_ADDR[0].ddr_stat);
	} else {
		__set_bit(24, &reg->rBT1120_YUV_ADDR[0].ddr_stat);
		__set_bit(25, &reg->rBT1120_YUV_ADDR[0].ddr_stat);
		__set_bit(26, &reg->rBT1120_YUV_ADDR[0].ddr_stat);
	}
}

static inline void bt1120_set_raw_buffer_channel(struct bt1120_reg *reg, bool enable)
{
	if (enable) {
		__clear_bit(27, &reg->rBT1120_YUV_ADDR[0].ddr_stat);
		__clear_bit(28, &reg->rBT1120_YUV_ADDR[0].ddr_stat);
		__clear_bit(29, &reg->rBT1120_YUV_ADDR[0].ddr_stat);
	} else {
		__set_bit(27, &reg->rBT1120_YUV_ADDR[0].ddr_stat);
		__set_bit(28, &reg->rBT1120_YUV_ADDR[0].ddr_stat);
		__set_bit(29, &reg->rBT1120_YUV_ADDR[0].ddr_stat);
	}
}

int bt1120_set_output_addr(struct bt1120_dev *bt1120)
{
	struct bt1120_reg *reg = bt1120->reg;
	struct bt1120_buffer *buffer = NULL;
	int i = 0;

	if (bt1120->hw_addr == NULL) {
		bt1120->hw_addr = kmalloc((sizeof(struct bt1120_hw_addr) * bt1120->reqbufs_count), GFP_KERNEL);

		for (i = 0; i < bt1120->reqbufs_count; i++) {
			buffer = bt1120_idle_queue_pop(bt1120);
			bt1120->hw_addr[i].virt_addr.y_addr = phys_to_virt((phys_addr_t)buffer->phys_addr.y_addr);
			bt1120->hw_addr[i].virt_addr.u_addr = phys_to_virt((phys_addr_t)buffer->phys_addr.u_addr);
			bt1120->hw_addr[i].virt_addr.v_addr = phys_to_virt((phys_addr_t)buffer->phys_addr.v_addr);

			bt1120->hw_addr[i].align_virt_addr.y_addr = _align_8_bytes(bt1120->hw_addr[i].virt_addr.y_addr);
			if (bt1120->hw_addr[i].virt_addr.y_addr != bt1120->hw_addr[i].align_virt_addr.y_addr)
				bt1120->hw_addr[i].y_need_align = true;
			else
				bt1120->hw_addr[i].y_need_align = false;

			bt1120->hw_addr[i].align_virt_addr.u_addr = _align_8_bytes(bt1120->hw_addr[i].virt_addr.u_addr);
			if (bt1120->hw_addr[i].virt_addr.u_addr != bt1120->hw_addr[i].align_virt_addr.u_addr)
				bt1120->hw_addr[i].u_need_align = true;
			else
				bt1120->hw_addr[i].u_need_align = false;

			bt1120->hw_addr[i].align_virt_addr.v_addr = _align_8_bytes(bt1120->hw_addr[i].virt_addr.v_addr);
			if (bt1120->hw_addr[i].virt_addr.v_addr != bt1120->hw_addr[i].align_virt_addr.v_addr)
				bt1120->hw_addr[i].v_need_align = true;
			else
				bt1120->hw_addr[i].v_need_align = false;

			bt1120->hw_addr[i].index = i;
			bt1120->hw_addr[i].full  = false;
			bt1120_idle_queue_add(bt1120, buffer);
		}
	}

	bt1120_set_hw_buffer_num(reg, ZOOM_FRAME, bt1120->reqbufs_count);
	bt1120_set_hw_buffer_num(reg, RAW_FRAME, bt1120->reqbufs_count);
	if (bt1120->out_frame.enable_scale) {
		bt1120_set_hw_buffer_addr(reg, ZOOM_FRAME, bt1120->hw_addr, bt1120->reqbufs_count);
		bt1120_set_scale_buffer_channel(reg, true);
		bt1120_set_raw_buffer_channel(reg, false);
	} else {
		bt1120_set_hw_buffer_addr(reg, RAW_FRAME, bt1120->hw_addr, bt1120->reqbufs_count);
		bt1120_set_scale_buffer_channel(reg, false);
		bt1120_set_raw_buffer_channel(reg, true);
	}

	bt1120_reg_refresh(reg);

	return 0;
}

static inline void bt1120_set_interrupt_frame_done(struct bt1120_reg *reg, bool enable)
{
	if (enable)
		__set_bit(0, &reg->rBT1120_INI_EN);
	else
		__clear_bit(0, &reg->rBT1120_INI_EN);
}

static inline void bt1120_set_interrupt_frame_size_change(struct bt1120_reg *reg, bool enable)
{
	if (enable)
		__set_bit(4, &reg->rBT1120_INI_EN);
	else
		__clear_bit(4, &reg->rBT1120_INI_EN);
}

static inline void bt1120_set_interrupt_crop_error(struct bt1120_reg *reg, bool enable)
{
	if (enable)
		__set_bit(5, &reg->rBT1120_INI_EN);
	else
		__clear_bit(5, &reg->rBT1120_INI_EN);
}

static inline void bt1120_set_interrupt_field_error(struct bt1120_reg *reg, bool enable)
{
	if (enable)
		__set_bit(6, &reg->rBT1120_INI_EN);
	else
		__clear_bit(6, &reg->rBT1120_INI_EN);
}

static inline void bt1120_set_interrupt_sv_fifo_full(struct bt1120_reg *reg, bool enable)
{
	if (enable)
		__set_bit(8, &reg->rBT1120_INI_EN);
	else
		__clear_bit(8, &reg->rBT1120_INI_EN);
}

static inline void bt1120_set_interrupt_su_fifo_full(struct bt1120_reg *reg, bool enable)
{
	if (enable)
		__set_bit(12, &reg->rBT1120_INI_EN);
	else
		__clear_bit(12, &reg->rBT1120_INI_EN);
}

static inline void bt1120_set_interrupt_sy_fifo_full(struct bt1120_reg *reg, bool enable)
{
	if (enable)
		__set_bit(16, &reg->rBT1120_INI_EN);
	else
		__clear_bit(16, &reg->rBT1120_INI_EN);
}

static inline void bt1120_set_interrupt_v_fifo_full(struct bt1120_reg *reg, bool enable)
{
	if (enable)
		__set_bit(20, &reg->rBT1120_INI_EN);
	else
		__clear_bit(20, &reg->rBT1120_INI_EN);
}

static inline void bt1120_set_interrupt_u_fifo_full(struct bt1120_reg *reg, bool enable)
{
	if (enable)
		__set_bit(24, &reg->rBT1120_INI_EN);
	else
		__clear_bit(24, &reg->rBT1120_INI_EN);
}

static inline void bt1120_set_interrupt_y_fifo_full(struct bt1120_reg *reg, bool enable)
{
	if (enable)
		__set_bit(28, &reg->rBT1120_INI_EN);
	else
		__clear_bit(28, &reg->rBT1120_INI_EN);
}

static inline void bt1120_set_interrupt_sy_buffer_full(struct bt1120_reg *reg, bool enable)
{
	if (enable)
		__set_bit(30, &reg->rBT1120_INI_EN);
	else
		__clear_bit(30, &reg->rBT1120_INI_EN);
}

static inline void bt1120_set_interrupt_y_buffer_full(struct bt1120_reg *reg, bool enable)
{
	if (enable)
		__set_bit(31, &reg->rBT1120_INI_EN);
	else
		__clear_bit(31, &reg->rBT1120_INI_EN);
}

int bt1120_set_interrupt_enable(struct bt1120_dev *bt1120)
{
	struct bt1120_reg *reg = bt1120->reg;

	bt1120_set_interrupt_sy_fifo_full(reg, true);
	bt1120_set_interrupt_su_fifo_full(reg, true);
	bt1120_set_interrupt_sv_fifo_full(reg, true);

	bt1120_set_interrupt_y_fifo_full(reg, true);
	bt1120_set_interrupt_u_fifo_full(reg, true);
	bt1120_set_interrupt_v_fifo_full(reg, true);

	bt1120_set_interrupt_y_buffer_full(reg, false);
	bt1120_set_interrupt_sy_buffer_full(reg, true);

	bt1120_set_interrupt_field_error(reg, false);
	bt1120_set_interrupt_crop_error(reg, true);
	bt1120_set_interrupt_frame_done(reg, true);
	bt1120_set_interrupt_frame_size_change(reg, true);

	bt1120_reg_refresh(reg);

	return 0;
}

int bt1120_set_interrupt_disable(struct bt1120_dev *bt1120)
{
	struct bt1120_reg *reg = bt1120->reg;

	bt1120_set_interrupt_sy_fifo_full(reg, false);
	bt1120_set_interrupt_su_fifo_full(reg, false);
	bt1120_set_interrupt_sv_fifo_full(reg, false);

	bt1120_set_interrupt_y_fifo_full(reg, false);
	bt1120_set_interrupt_u_fifo_full(reg, false);
	bt1120_set_interrupt_v_fifo_full(reg, false);

	bt1120_set_interrupt_y_buffer_full(reg, false);
	bt1120_set_interrupt_sy_buffer_full(reg, false);

	bt1120_set_interrupt_field_error(reg, false);
	bt1120_set_interrupt_crop_error(reg, false);
	bt1120_set_interrupt_frame_done(reg, false);
	bt1120_set_interrupt_frame_size_change(reg, false);

	bt1120_reg_refresh(reg);

	return 0;
}

int bt1120_run(struct bt1120_dev *bt1120)
{
	struct bt1120_reg *reg = bt1120->reg;

	__set_bit(0, &reg->rBT1120_CTRL);
	bt1120_reg_refresh(reg);

	return 0;
}

int bt1120_stop(struct bt1120_dev *bt1120)
{
	struct bt1120_reg *reg = bt1120->reg;

	__clear_bit(0, &reg->rBT1120_CTRL);
	bt1120_reg_refresh(reg);

	return 0;
}

static int bt1120_update_hw_buffer_state(struct bt1120_dev *bt1120)
{
	int i = 0;
	struct bt1120_hw_addr *hw_addr = bt1120->hw_addr;
	struct bt1120_reg *reg = bt1120->reg;
	unsigned int ddr_state = readl(&reg->rBT1120_YUV_ADDR[0].ddr_stat);

	if (bt1120->out_frame.enable_scale)
		ddr_state = ddr_state >> 8;

	for (i = 0; i < bt1120->reqbufs_count; i++) {
		if ((ddr_state >> i) & 0x1) {
			if (!hw_addr[i].full) {
				hw_addr[i].full = true;
				bt1120_busy_queue_add(bt1120, &hw_addr[i]);
			}
		}
	}
	return 0;
}

void bt1120_clr_raw_buffer_state(struct bt1120_dev *bt1120)
{
	struct bt1120_reg *reg = bt1120->reg;

	__set_field(0xf, 0xf, 0, &reg->rBT1120_YUV_ADDR[0].ddr_stat);
}

void bt1120_clr_scale_buffer_state(struct bt1120_dev *bt1120)
{
	struct bt1120_reg *reg = bt1120->reg;

	__set_field(0xf, 0xf, 8, &reg->rBT1120_YUV_ADDR[0].ddr_stat);
}

int bt1120_handle_interrupt(struct bt1120_dev *bt1120)
{
	struct bt1120_reg *reg = bt1120->reg;
	int interrupt_state = readl(&reg->rBT1120_INI_STAT);

	if (interrupt_state & 0x1) {
		bt1120_update_hw_buffer_state(bt1120);
		__set_bit(0, &reg->rBT1120_INI_STAT);
		tasklet_schedule(&bt1120->tasklet);
	}

	if ((interrupt_state >> 4) & 0x1) {
		unsigned int width = 0, height = 0;
		width = readl(&reg->rBT1120_FRAME_SIZE) & 0x7ff;
		height = (readl(&reg->rBT1120_FRAME_SIZE) >> 16) & 0x7ff;
		v4l2_err(&bt1120->v4l2_dev, "frame size change! size:%d %d\n", width, height);
		__set_bit(4, &reg->rBT1120_INI_STAT);
	}

	if ((interrupt_state >> 5) & 0x1) {
		v4l2_err(&bt1120->v4l2_dev, "crop error\n");
		__set_bit(5, &reg->rBT1120_INI_STAT);
	}

	if ((interrupt_state >> 8) & 0x1) {
		v4l2_err(&bt1120->v4l2_dev, "sv fifo full\n");
		__set_bit(8, &reg->rBT1120_INI_STAT);
	}

	if ((interrupt_state >> 12) & 0x1) {
		v4l2_err(&bt1120->v4l2_dev, "su fifo full\n");
		__set_bit(12, &reg->rBT1120_INI_STAT);
	}

	if ((interrupt_state >> 16) & 0x1) {
		v4l2_err(&bt1120->v4l2_dev, "sy fifo full\n");
		__set_bit(16, &reg->rBT1120_INI_STAT);
	}

	if ((interrupt_state >> 20) & 0x1) {
		v4l2_err(&bt1120->v4l2_dev, "sv fifo full\n");
		__set_bit(20, &reg->rBT1120_INI_STAT);
	}

	if ((interrupt_state >> 24) & 0x1) {
		v4l2_err(&bt1120->v4l2_dev, "su fifo full\n");
		__set_bit(24, &reg->rBT1120_INI_STAT);
	}

	if ((interrupt_state >> 28) & 0x1) {
		v4l2_err(&bt1120->v4l2_dev, "sy fifo full\n");
		__set_bit(28, &reg->rBT1120_INI_STAT);
	}

	if ((interrupt_state >> 30) & 0x1) {
		v4l2_err(&bt1120->v4l2_dev, "scale buffer full\n");
		bt1120_clr_scale_buffer_state(bt1120);
		__set_bit(30, &reg->rBT1120_INI_STAT);
	}

	if ((interrupt_state >> 31) & 0x1) {
		v4l2_warn(&bt1120->v4l2_dev, "raw buffer full\n");
		bt1120_clr_raw_buffer_state(bt1120);
		__set_bit(31, &reg->rBT1120_INI_STAT);
	}

	return 0;
}

int bt1120_release_buffer(struct bt1120_dev *bt1120, unsigned int index)
{
	struct bt1120_reg *reg = bt1120->reg;

	__set_bit(index, &reg->rBT1120_YUV_ADDR[0].ddr_stat);
	__set_bit((index + 8), &reg->rBT1120_YUV_ADDR[0].ddr_stat);

	return 0;
}
