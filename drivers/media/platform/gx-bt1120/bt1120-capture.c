#include <linux/bug.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/version.h>

#include <media/media-device.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-clk.h>
#include <media/videobuf2-v4l2.h>
#include <media/videobuf2-dma-contig.h>
#include <media/videobuf2-core.h>
#include <media/videobuf2-vmalloc.h>

#include "bt1120-core.h"
#include "bt1120-reg.h"

static int sensor_set_fmt(struct bt1120_dev *bt1120);
void gx_bt1120_clk_enable(struct bt1120_dev *bt1120)
{
	struct bt1120_clk_info *clk_info = bt1120->clk_info;
	unsigned int i = 0;

	for (i = 0; i < clk_info->num; i++)
		v4l2_clk_enable(bt1120->clk[i]);
}

void gx_bt1120_clk_disable(struct bt1120_dev *bt1120)
{
	struct bt1120_clk_info *clk_info = bt1120->clk_info;
	unsigned int i = 0;

	for (i = 0; i < clk_info->num; i++)
		v4l2_clk_disable(bt1120->clk[i]);
}

static int sensor_set_power(struct bt1120_dev *bt1120, int on)
{
	struct v4l2_subdev *sd = bt1120->sensor.sd;

	return v4l2_subdev_call(sd, core, s_power, on);
}

static int gx_bt1120_open(struct file *file)
{
	struct bt1120_dev *bt1120 = video_drvdata(file);
	struct bt1120_frame *frame = &bt1120->out_frame;

	gx_bt1120_clk_enable(bt1120);
	sensor_set_power(bt1120, 1);
	frame->enable_crop = false;
	frame->enable_scale = false;
	bt1120->is_open = true;
	INIT_LIST_HEAD(&bt1120->idle_buf_q);
	INIT_LIST_HEAD(&bt1120->busy_buf_q);

	return v4l2_fh_open(file);
}

static int gx_bt1120_close(struct file *file)
{
	struct bt1120_dev *bt1120 = video_drvdata(file);
	int ret = 0;

	mutex_lock(&bt1120->lock);

	sensor_set_power(bt1120, 0);
	if (bt1120->owner == file->private_data) {
		vb2_queue_release(&bt1120->vb_queue);
		kfree(bt1120->hw_addr);
		bt1120->hw_addr = NULL;
		bt1120->owner = NULL;
	}

	gx_bt1120_clk_disable(bt1120);
	ret = v4l2_fh_release(file);
	bt1120->is_open = false;
	mutex_unlock(&bt1120->lock);

	return ret;
}

static unsigned int gx_bt1120_poll(struct file *file, struct poll_table_struct *wait)
{
	struct bt1120_dev *bt1120 = video_drvdata(file);
	int ret = 0;

	mutex_lock(&bt1120->lock);
	if (bt1120->owner && (bt1120->owner != file->private_data))
		ret = -EBUSY;
	else
		ret = vb2_poll(&bt1120->vb_queue, file, wait);

	mutex_unlock(&bt1120->lock);
	return ret;
}

static int gx_bt1120_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct bt1120_dev *bt1120 = video_drvdata(file);
	int ret;

	if (bt1120->owner && (bt1120->owner != file->private_data))
		ret = -EBUSY;
	else
		ret = vb2_mmap(&bt1120->vb_queue, vma);

	return ret;
}

static const struct v4l2_file_operations gx_bt1120_fops = {
	.owner          = THIS_MODULE,
	.open           = gx_bt1120_open,
	.release        = gx_bt1120_close,
	.poll           = gx_bt1120_poll,
	.unlocked_ioctl = video_ioctl2,
	.mmap           = gx_bt1120_mmap,
};

static int gx_bt1120_vidioc_querycap(struct file *file, void *fh,
					struct v4l2_capability *cap)
{
	struct bt1120_dev *bt1120 = video_drvdata(file);

	strlcpy(cap->driver, GX_BT1120_DRIVER_NAME, sizeof(cap->driver));
	strlcpy(cap->card, GX_BT1120_DRIVER_NAME, sizeof(cap->card));
	snprintf(cap->bus_info, sizeof(cap->bus_info), "platform:%s",
				dev_name(bt1120->dev));

	cap->device_caps  = V4L2_CAP_STREAMING | V4L2_CAP_VIDEO_CAPTURE;
	cap->capabilities = cap->device_caps | V4L2_CAP_DEVICE_CAPS;

	return 0;
}

static struct bt1120_fmt bt1120_formats[] = {
	{
		.description = "YUV 4:2:0 planar, Y/Cb/Cr",
		.fourcc      = V4L2_PIX_FMT_YUV420,
		.ybpp        = 1,
	}, {
		.description = "YUV 4:2:2 planar, Y/Cb/Cr",
		.fourcc      = V4L2_PIX_FMT_YUV422P,
		.ybpp        = 1,
	},
};

static int gx_bt1120_vidioc_enum_fmt_vid_cap(struct file *file, void *fh,
					struct v4l2_fmtdesc *f)
{
	if (f->index >= ARRAY_SIZE(bt1120_formats))
		return -EINVAL;

	strlcpy(f->description, bt1120_formats[f->index].description, sizeof(f->description));
	f->pixelformat = bt1120_formats[f->index].fourcc;

	return 0;
}

static int gx_bt1120_vidioc_try_fmt(struct file *file, void *priv,
					struct v4l2_format *f)
{
	int i = 0;
	struct bt1120_fmt *fmt = NULL;

	for (i = 0; i < ARRAY_SIZE(bt1120_formats); i++) {
		if (bt1120_formats[i].fourcc == f->fmt.pix.pixelformat)
			fmt = &bt1120_formats[i];
	}

	if (fmt == NULL)
		return -EINVAL;

	return 0;
}

static int gx_bt1120_vidioc_enum_input(struct file *file, void *priv,
					struct v4l2_input *input)
{
	struct bt1120_dev *bt1120  = video_drvdata(file);
	struct v4l2_subdev *sensor = bt1120->sensor.sd;

	if (input->index || sensor == NULL)
		return -EINVAL;

	input->type = V4L2_INPUT_TYPE_CAMERA;
	strlcpy(input->name, sensor->name, sizeof(input->name));
	return 0;
}

static int gx_bt1120_vidioc_g_input(struct file *file, void *priv,
					unsigned int *i)
{
	*i = 0;
	return 0;
}

static int gx_bt1120_vidioc_s_input(struct file *file, void *priv,
					unsigned int i)
{
   return i == 0 ? 0 : -EINVAL;
}

static int gx_bt1120_vidioc_s_fmt(struct file *file, void *priv,
					struct v4l2_format *f)
{
	int i = 0;
	struct bt1120_fmt *fmt = NULL;
	struct bt1120_dev *bt1120 = video_drvdata(file);
	struct bt1120_frame *frame = &bt1120->out_frame;
	struct v4l2_pix_format *pix = &f->fmt.pix;

	if (vb2_is_busy(&bt1120->vb_queue))
		return -EBUSY;

	if (pix->width > 2047 || pix->width < 16)
		return -EINVAL;

	if (pix->height > 2047 || pix->height < 16)
		return -EINVAL;

	for (i = 0; i < ARRAY_SIZE(bt1120_formats); i++) {
		if (bt1120_formats[i].fourcc == f->fmt.pix.pixelformat)
			fmt = &bt1120_formats[i];
	}

	if (fmt == NULL)
		return -EINVAL;

	frame->f_width   = pix->width;
	frame->f_height  = pix->height;
	frame->fmt       = fmt;
	frame->sizeimage = pix->sizeimage;

	frame->crop.width  = pix->width;
	frame->crop.height = pix->height;
	frame->crop.left   = 0;
	frame->crop.top    = 0;

	if (bt1120->owner == NULL)
		bt1120->owner = priv;

	sensor_set_fmt(bt1120);

	return 0;
}

static int gx_bt1120_vidioc_g_fmt(struct file *file, void *priv,
					struct v4l2_format *f)
{
	struct bt1120_dev   *bt1120 = video_drvdata(file);
	struct bt1120_frame *frame = &bt1120->out_frame;
	struct bt1120_fmt *fmt = bt1120->out_frame.fmt;
	struct v4l2_pix_format *pix = &f->fmt.pix;

	pix->bytesperline = frame->f_width * fmt->ybpp;
	pix->sizeimage    = frame->sizeimage;

	pix->pixelformat = fmt->fourcc;
	pix->width       = frame->f_width;
	pix->height      = frame->f_height;
	pix->field       = V4L2_FIELD_NONE;
	pix->colorspace  = V4L2_COLORSPACE_JPEG;

	return 0;
}

static int gx_bt1120_reqbufs(struct file *file, void *priv,
					struct v4l2_requestbuffers *rb)
{
	struct bt1120_dev *bt1120 = video_drvdata(file);
	int ret = 0;

	if (bt1120->owner && bt1120->owner != priv)
		return -EBUSY;

	if (rb->count) {
		rb->count = max_t(u32, BT1120_REQ_BUFS_MIN, rb->count);
		rb->count = min_t(u32, BT1120_REQ_BUFS_MAX, rb->count);
	} else
		bt1120->owner = NULL;

	ret = vb2_reqbufs(&bt1120->vb_queue, rb);
	if (ret < 0)
		return ret;

	if (rb->count && rb->count < BT1120_REQ_BUFS_MIN) {
		rb->count = 0;
		vb2_reqbufs(&bt1120->vb_queue, rb);
		ret = -ENOMEM;
	}

	bt1120->reqbufs_count = rb->count;
	if (bt1120->owner == NULL && rb->count > 0)
		bt1120->owner = priv;

	return 0;
}

static int gx_bt1120_querybuf(struct file *file, void *priv,
					struct v4l2_buffer *buf)
{
	struct bt1120_dev *bt1120 = video_drvdata(file);
	return vb2_querybuf(&bt1120->vb_queue, buf);
}

static int gx_bt1120_qbuf(struct file *file, void *priv,
			        struct v4l2_buffer *buf)
{
	struct bt1120_dev *bt1120 = video_drvdata(file);

	if (bt1120->owner && bt1120->owner != priv)
		return -EBUSY;

	return vb2_qbuf(&bt1120->vb_queue, buf);
}

static int gx_bt1120_dqbuf(struct file *file, void *priv,
					struct v4l2_buffer *buf)
{
	struct bt1120_dev *bt1120 = video_drvdata(file);

	if (bt1120->owner && bt1120->owner != priv)
		return -EBUSY;

	return vb2_dqbuf(&bt1120->vb_queue, buf, file->f_flags & O_NONBLOCK);
}

static int gx_bt1120_streamon(struct file *file, void *priv,
					enum v4l2_buf_type type)
{
	struct bt1120_dev *bt1120 = video_drvdata(file);

	if (type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;

	if (bt1120->owner && bt1120->owner != priv)
		return -EBUSY;

	return vb2_streamon(&bt1120->vb_queue, type);
}

static int gx_bt1120_streamoff(struct file *file, void *priv,
					enum v4l2_buf_type type)
{
	struct bt1120_dev *bt1120 = video_drvdata(file);

	if (type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;

	if (bt1120->owner && bt1120->owner != priv)
		return -EBUSY;

	return vb2_streamoff(&bt1120->vb_queue, type);
}

static int gx_bt1120_vidioc_cropcap(struct file *file, void *fh,
					struct v4l2_cropcap *cropcap)
{
	struct bt1120_dev *bt1120 = video_drvdata(file);
	struct bt1120_frame *frame = &bt1120->out_frame;

	if (cropcap->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;

	cropcap->bounds.left   = 0;
	cropcap->bounds.top    = 0;
	cropcap->bounds.width  = frame->f_width & ~1;
	cropcap->bounds.height = frame->f_width & ~1;
	cropcap->defrect       = cropcap->bounds;
	cropcap->pixelaspect.numerator   = 15;
	cropcap->pixelaspect.denominator = 1;

	return 0;
}

static int gx_bt1120_vidioc_g_crop(struct file *file, void *prv, struct v4l2_crop *crop)
{
	struct bt1120_dev *bt1120 = video_drvdata(file);
	struct bt1120_frame *frame = &bt1120->out_frame;

	if (crop->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;

	crop->c.left   = frame->crop.left;
	crop->c.top    = frame->crop.top;
	crop->c.width  = frame->crop.width;
	crop->c.height = frame->crop.height;

	return 0;
}

static int gx_bt1120_try_crop(struct bt1120_dev *bt1120, const struct v4l2_crop *crop)
{
	struct bt1120_frame *frame = &bt1120->out_frame;
	unsigned int bounds_width = crop->c.left + crop->c.width;
	unsigned int bounds_height = crop->c.top + crop->c.height;

	if (crop->c.left < 0 || crop->c.top < 0)
		return -EINVAL;

	if (crop->c.width == 0 || crop->c.height == 0)
		return -EINVAL;

	if (bt1120->out_frame.enable_scale) {
		if (bounds_width < frame->f_width || bounds_height < frame->f_height)
			return -EINVAL;
	} else {
		if (bounds_width > frame->f_width || bounds_height > frame->f_height)
			return -EINVAL;
	}

	if ((crop->c.width % 2) || (crop->c.height % 2))
		return -EINVAL;

	return 0;
}

static int gx_bt1120_vidioc_s_crop(struct file *file, void *fh,
					const struct v4l2_crop *crop)
{
	struct bt1120_dev *bt1120  = video_drvdata(file);
	struct bt1120_frame *frame = &bt1120->out_frame;
	int ret = 0;

	if (vb2_is_busy(&bt1120->vb_queue))
		return -EBUSY;

	ret = gx_bt1120_try_crop(bt1120, crop);
	if (ret < 0)
		return -EINVAL;

	frame->crop.left   = crop->c.left;
	frame->crop.top    = crop->c.top;
	frame->crop.width  = crop->c.width;
	frame->crop.height = crop->c.height;
	frame->enable_crop = true;

	return 0;
}

static int gx_bt1120_vidioc_s_parm(struct file *file, void *fh,
					struct v4l2_streamparm *parm)
{
	struct bt1120_dev *bt1120  = video_drvdata(file);

	if (parm->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;

	bt1120->sensor.timeperframe.denominator = parm->parm.capture.timeperframe.denominator;
	bt1120->sensor.timeperframe.numerator = parm->parm.capture.timeperframe.numerator;

	return 0;
}

static int gx_bt1120_vidioc_g_parm(struct file *file, void *fh,
					struct v4l2_streamparm *parm)
{
	struct bt1120_dev *bt1120  = video_drvdata(file);

	if (parm->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;

	parm->parm.capture.timeperframe.denominator = bt1120->sensor.timeperframe.denominator;
	parm->parm.capture.timeperframe.numerator = bt1120->sensor.timeperframe.numerator;
	parm->parm.capture.readbuffers  = 1;

	return 0;
}

static const struct v4l2_ioctl_ops gx_bt1120_ioctl_ops = {
	.vidioc_querycap         = gx_bt1120_vidioc_querycap,
	.vidioc_enum_input       = gx_bt1120_vidioc_enum_input,
	.vidioc_g_input          = gx_bt1120_vidioc_g_input,
	.vidioc_s_input          = gx_bt1120_vidioc_s_input,
	.vidioc_cropcap          = gx_bt1120_vidioc_cropcap,
	.vidioc_s_crop           = gx_bt1120_vidioc_s_crop,
	.vidioc_g_crop           = gx_bt1120_vidioc_g_crop,
	.vidioc_g_parm           = gx_bt1120_vidioc_g_parm,
	.vidioc_s_parm           = gx_bt1120_vidioc_s_parm,
	.vidioc_enum_fmt_vid_cap = gx_bt1120_vidioc_enum_fmt_vid_cap,
	.vidioc_try_fmt_vid_cap  = gx_bt1120_vidioc_try_fmt,
	.vidioc_s_fmt_vid_cap    = gx_bt1120_vidioc_s_fmt,
	.vidioc_g_fmt_vid_cap    = gx_bt1120_vidioc_g_fmt,
	.vidioc_reqbufs          = gx_bt1120_reqbufs,
	.vidioc_querybuf         = gx_bt1120_querybuf,
	.vidioc_qbuf             = gx_bt1120_qbuf,
	.vidioc_dqbuf            = gx_bt1120_dqbuf,
	.vidioc_streamon         = gx_bt1120_streamon,
	.vidioc_streamoff        = gx_bt1120_streamoff,
};

static int gx_bt1120_queue_setup(struct vb2_queue *vq, const void *parg,
					unsigned int *num_buffers, unsigned int *num_planes,
					unsigned int sizes[], void *allocators[])
{
	struct bt1120_dev *bt1120  = vb2_get_drv_priv(vq);
	struct bt1120_frame *frame = &bt1120->out_frame;
	struct bt1120_fmt *fmt     = frame->fmt;

	*num_planes = 1;

	if (frame->enable_crop && !frame->enable_scale)
		frame->y_size  = frame->crop.width * frame->crop.height;
	else
		frame->y_size  = frame->f_width * frame->f_height;

	if (fmt->fourcc == V4L2_PIX_FMT_YUV420)
		frame->uv_size = frame->y_size / 4;
	else
		frame->uv_size = frame->y_size / 2;

	sizes[0]      = frame->y_size + frame->uv_size * 2 + ALIGN_SIZE * 3;
	allocators[0] = bt1120->alloc_ctx;

	return 0;
}

static void gx_bt1120_buffer_queue(struct vb2_buffer *vb)
{
	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);
	struct bt1120_buffer *buf = container_of(vbuf, struct bt1120_buffer, vb);
	struct bt1120_dev *bt1120 = vb2_get_drv_priv(vb->vb2_queue);
	struct bt1120_frame *frame = &bt1120->out_frame;

	buf->phys_addr.y_addr = (void *)vb2_dma_contig_plane_dma_addr(vb, 0);
	buf->phys_addr.u_addr = buf->phys_addr.y_addr + frame->y_size;
	buf->phys_addr.v_addr = buf->phys_addr.u_addr + frame->uv_size;
	bt1120_idle_queue_add(bt1120, buf);
}

int gx_bt1120_hw_init(struct bt1120_dev *bt1120)
{
	int ret = 0;

	ret = bt1120_hw_reset(bt1120);
	bt1120_set_zoom_coef(bt1120);
	bt1120_set_scale_coef(bt1120);
	bt1120_set_input(bt1120);
	bt1120_set_output(bt1120);
	bt1120_set_crop(bt1120);
	bt1120_set_scale(bt1120);
	bt1120_set_output_addr(bt1120);
	bt1120_set_interrupt_enable(bt1120);
	bt1120_set_frame_done_hold(bt1120, true);
	bt1120_run(bt1120);
	bt1120_reg_refresh(bt1120->reg);

	bt1120->state = BT1120_RUN;

	return ret;
}

static int sensor_set_fmt(struct bt1120_dev *bt1120)
{
	struct v4l2_subdev *sd = bt1120->sensor.sd;
	struct v4l2_subdev_format subdev_format = {0};
	struct bt1120_frame *frame = &bt1120->out_frame;
	int ret = 0;

	subdev_format.pad = 0;
	subdev_format.format.width = frame->f_width;
	subdev_format.format.height = frame->f_height;
	subdev_format.format.code = MEDIA_BUS_FMT_UYVY8_2X8;
	subdev_format.which = V4L2_SUBDEV_FORMAT_TRY;

	ret = v4l2_subdev_call(sd, pad, set_fmt, NULL, &subdev_format);
	if (ret < 0)
		return ret;

	bt1120->sensor.width = subdev_format.format.width;
	bt1120->sensor.height = subdev_format.format.height;
	if (bt1120->sensor.width != frame->f_width ||
				bt1120->sensor.height != frame->f_height)
		frame->enable_scale = true;
	else
		frame->enable_scale = false;

	return ret;
}

static int sensor_set_timeperframe(struct bt1120_dev *bt1120)
{
	struct v4l2_subdev *sd = bt1120->sensor.sd;
	struct v4l2_subdev_frame_interval fi = {0};

	if (bt1120->sensor.timeperframe.denominator == 0)
		return 0;

	fi.interval.denominator = bt1120->sensor.timeperframe.denominator;
	fi.interval.numerator = bt1120->sensor.timeperframe.numerator;

	return v4l2_subdev_call(sd, video, s_frame_interval, &fi);
}

static int sensor_set_stream(struct bt1120_dev *bt1120, int on)
{
	struct v4l2_subdev *sd = bt1120->sensor.sd;

	return v4l2_subdev_call(sd, video, s_stream, on);
}

static int gx_bt1120_start_streaming(struct vb2_queue *vq, unsigned int count)
{
	struct bt1120_dev *bt1120 = vb2_get_drv_priv(vq);
	int ret = 0;

	if (bt1120->state == BT1120_RUN)
		return 0;

	mutex_lock(&bt1120->lock);

	sensor_set_timeperframe(bt1120);
	sensor_set_stream(bt1120, 1);
	ret = gx_bt1120_hw_init(bt1120);

	mutex_unlock(&bt1120->lock);

	return ret;
}

static void gx_bt1120_stop_capture(struct bt1120_dev *bt1120)
{
	struct bt1120_buffer *buffer = NULL;

	while (!list_empty(&bt1120->idle_buf_q)) {
		buffer = bt1120_idle_queue_pop(bt1120);
		vb2_buffer_done(&buffer->vb.vb2_buf, VB2_BUF_STATE_ERROR);
	}

	bt1120_set_interrupt_disable(bt1120);
	bt1120_clr_raw_buffer_state(bt1120);
	bt1120_clr_scale_buffer_state(bt1120);
	bt1120_stop(bt1120);
	bt1120_reg_refresh(bt1120->reg);

	bt1120->state = BT1120_STOP;
}

static void gx_bt1120_stop_streaming(struct vb2_queue *vq)
{
	struct bt1120_dev *bt1120 = vb2_get_drv_priv(vq);

	gx_bt1120_stop_capture(bt1120);
}

static const struct vb2_ops gx_bt1120_qops = {
	.queue_setup     = gx_bt1120_queue_setup,
	.buf_queue       = gx_bt1120_buffer_queue,
	.wait_prepare    = vb2_ops_wait_prepare,
	.wait_finish     = vb2_ops_wait_finish,
	.start_streaming = gx_bt1120_start_streaming,
	.stop_streaming  = gx_bt1120_stop_streaming,
};

int bt1120_register_video_nodes(struct bt1120_dev *bt1120)
{
	struct video_device *vfd = &bt1120->vdev;
	struct vb2_queue *q = &bt1120->vb_queue;
	int ret = 0;

	memset(vfd, 0, sizeof(*vfd));
	snprintf(vfd->name, sizeof(vfd->name), "bt1120-cap");
	INIT_LIST_HEAD(&bt1120->idle_buf_q);
	INIT_LIST_HEAD(&bt1120->busy_buf_q);

	vfd->fops         = &gx_bt1120_fops;
	vfd->ioctl_ops    = &gx_bt1120_ioctl_ops;
	vfd->v4l2_dev     = &bt1120->v4l2_dev;
	vfd->release      = video_device_release_empty;

	video_set_drvdata(vfd, bt1120);

	memset(q, 0, sizeof(struct vb2_queue));
	q->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	q->io_modes = VB2_MMAP;
	q->ops = &gx_bt1120_qops;
	q->mem_ops = &vb2_dma_contig_memops;
	q->buf_struct_size = sizeof(struct bt1120_buffer);
	q->drv_priv = bt1120;
	q->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC;
	q->lock = &bt1120->lock;

	ret = vb2_queue_init(q);
	if (ret < 0)
		return ret;

	return video_register_device(vfd, VFL_TYPE_GRABBER, -1);
}

static struct i2c_board_info i2c_info[] = {
	{
		.type = "ov2640",
		.addr = 0x30,
	}, {
		.type = "ov5640",
		.addr = 0x3c,
	},
};

int bt1120_register_sensor(struct bt1120_dev *bt1120)
{
	struct v4l2_device *v4l2_dev = &bt1120->v4l2_dev;
	struct i2c_adapter *adapter = NULL;
	struct v4l2_subdev *sd = NULL;
	unsigned int i = 0;

	adapter = i2c_get_adapter(bt1120->i2c_bus);
	if (adapter == NULL) {
		v4l2_err(v4l2_dev, "failed to get I2C adapter %d\n", 0);
		return -EPROBE_DEFER;
	}

	for (i = 0; i < ARRAY_SIZE(i2c_info); i++) {
		i2c_info[i].of_node = bt1120->dev->of_node;
		sd = v4l2_i2c_new_subdev_board(v4l2_dev, adapter, &i2c_info[i], NULL);
		if (sd == NULL)
			v4l2_warn(v4l2_dev, "failed to acquire subdev %s\n", i2c_info[i].type);
		else
			break;
	}

	if (sd == NULL) {
		i2c_put_adapter(adapter);
		return -EPROBE_DEFER;
	}
	bt1120->sensor.sd = sd;

	v4l2_info(v4l2_dev, "registered sensor subdevice %s\n", sd->name);

	return 0;
}

irqreturn_t bt1120_interrupt(int irq, void *arg)
{
	bt1120_handle_interrupt((struct bt1120_dev*)arg);

	return IRQ_HANDLED;
}

void bt1120_update_video_buf(unsigned long data)
{
	struct bt1120_dev *bt1120 = (struct bt1120_dev *)data;
	struct bt1120_hw_addr *hw_addr = NULL;
	struct bt1120_buffer *buffer = NULL;
	struct bt1120_frame *frame = &bt1120->out_frame;

	if (!list_empty(&bt1120->idle_buf_q))
		buffer = bt1120_idle_queue_pop(bt1120);
	else
		return;

	if (!list_empty(&bt1120->busy_buf_q)) {
		hw_addr = bt1120_busy_queue_pop(bt1120);
		if (!hw_addr->full) {
			v4l2_err(&bt1120->v4l2_dev, "busy queue pop error\n");
			bt1120_idle_queue_add(bt1120, buffer);
			return;
		}
	} else
		return;

	if (hw_addr->y_need_align)
		memcpy(hw_addr->virt_addr.y_addr, hw_addr->align_virt_addr.v_addr, frame->y_size);
	if (hw_addr->u_need_align)
		memcpy(hw_addr->virt_addr.u_addr, hw_addr->align_virt_addr.u_addr, frame->uv_size);
	if (hw_addr->v_need_align)
		memcpy(hw_addr->virt_addr.v_addr, hw_addr->align_virt_addr.v_addr, frame->uv_size);

	hw_addr->full = false;
	bt1120_release_buffer(bt1120, hw_addr->index);
	buffer->vb.vb2_buf.planes[0].bytesused = frame->y_size + frame->uv_size * 2;
	vb2_buffer_done(&buffer->vb.vb2_buf, VB2_BUF_STATE_DONE);
}
