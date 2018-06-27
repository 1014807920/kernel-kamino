#ifndef __BT1120_RET_H__
#define __BT1120_RET_H__

#define HW_BUFFER_SIZE 4
#define ALIGN_SIZE 8

enum bt1120_sync_mode {
	SYNC_INSIDE,
	SYNC_OUTSIDE,
};

enum bt1120_bit_width {
	INPUT_8BIT,
	INPUT_16BIT,
};

enum bt1120_version {
	BT1120_NRE,
	BT1120_MPW,
};

enum bt1120_yuv_fmt {
	YUV420,
	YUV422,
};

enum bt1120_fifo_gate {
	NO_GATE,
	QUARTER_GATE,
	HALF_GATE,
	THREE_FOURTHS_GATE,
};

enum bt1120_hw_buffer_type {
	RAW_FRAME,
	ZOOM_FRAME,
};

extern int bt1120_hw_reset(struct bt1120_dev *bt1120);
extern int bt1120_set_input(struct bt1120_dev *bt1120);
extern int bt1120_set_zoom_coef(struct bt1120_dev *bt1120);
extern int bt1120_set_scale_coef(struct bt1120_dev *bt1120);
extern int bt1120_set_output(struct bt1120_dev *bt1120);
extern int bt1120_set_frame_done_hold(struct bt1120_dev *bt1120, bool enable);
extern int bt1120_set_output_addr(struct bt1120_dev *bt1120);
extern int bt1120_set_interrupt_enable(struct bt1120_dev *bt1120);
extern int bt1120_run(struct bt1120_dev *bt1120);
extern int bt1120_handle_interrupt(struct bt1120_dev *bt1120);
extern int bt1120_release_buffer(struct bt1120_dev *bt1120, unsigned int index);
extern int bt1120_stop(struct bt1120_dev *bt1120);
extern void bt1120_clr_raw_buffer_state(struct bt1120_dev *bt1120);
extern void bt1120_clr_scale_buffer_state(struct bt1120_dev *bt1120);
extern int bt1120_set_interrupt_disable(struct bt1120_dev *bt1120);
extern int bt1120_set_crop(struct bt1120_dev *bt1120);
extern int bt1120_set_scale(struct bt1120_dev *bt1120);
extern int bt1120_request_run(struct bt1120_dev *bt1120);
extern int bt1120_request_stop(struct bt1120_dev *bt1120);
extern void bt1120_reg_refresh(struct bt1120_reg *reg);

#endif
