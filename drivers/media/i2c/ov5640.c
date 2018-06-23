#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/media.h>
#include <linux/module.h>
#include <linux/ratelimit.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/videodev2.h>

#include <media/media-entity.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-clk.h>
#include <media/v4l2-event.h>
#include <media/v4l2-image-sizes.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-mediabus.h>

#define REG_CHIP_ID_HIGH	0x300a
#define REG_CHIP_ID_LOW		0x300b
#define OV5640_ID_LOW		0x5640

#define WIDTH_720P			1280
#define HEIGHT_720P			720

#define WIDTH_1080P			1920
#define HEIGHT_1080P		1080

#define HTS_QVGA			1896
#define VTS_QVGA			984
#define HTS_VGA				1896
#define VTS_VGA				984
#define HTS_720P			1896
#define VTS_720P			984
#define HTS_1080P			2500
#define VTS_1080P			1120

#define ENDMARKER { 0xff, 0xff }

struct regval_list {
	u16 reg_num;
	u8  value;
};

struct ov5640_framesize {
	u16 width;
	u16 height;
	struct regval_list *regs;
	u16 array_size;
};

struct ov5640_win_size {
	char *name;
	u32  width;
	u32  height;
	u32  hts;
	u32  vts;
	const struct regval_list *regs;
};

enum ov5640_fps {
	OV5640_7FPS,
	OV5640_15FPS,
	OV5640_30FPS,
};

struct ov5640_info {
	struct i2c_client *client;
	struct gpio_desc *gpio_rst;
	struct gpio_desc *gpio_pdn;

	struct v4l2_ctrl_handler hdl;
	struct v4l2_clk *clk;
	struct v4l2_subdev subdev;
	struct v4l2_fract *interval;

	const struct ov5640_win_size *win;
	struct mutex lock;
	enum ov5640_fps fps;

	u32 cfmt_code;
	int power;
	int streaming;
	int contrast_val;
	int saturation_val;
	int brightness_val;
	int sharpness_val;
	int debug;
};

struct v4l2_fract ov5640_interval[] = {
	{1,  7}, //7fps
	{1,  15}, //15fps
	{1,  30}, //30fps
};

#define IMAGEWIN_SET(xst, xend, yst, yend) \
	{0x3800, xst >> 8}, \
	{0x3801, xst & 0xff}, \
	{0x3802, yst >> 8}, \
	{0x3803, yst & 0xff}, \
	{0x3804, xend >> 8}, \
	{0x3805, xend & 0xff}, \
	{0x3806, yend >> 8}, \
	{0x3807, yend & 0xff}

#define OUTSIZE_SET(offx, offy, width, height) \
	{0x3808, width >> 8}, \
	{0x3809, width & 0xff}, \
	{0x380a, height >> 8}, \
	{0x380b, height & 0xff}, \
	{0x3810, offx >> 8}, \
	{0x3811, offx & 0xff}, \
	{0x3812, offy >> 8}, \
	{0x3813, offy & 0xff}

#define HTS_SET(hts) \
	{0x380c, hts >> 8}, \
	{0x380d, hts & 0xff}

#define VTS_SET(vts) \
	{0x380e, vts >> 8}, \
	{0x380f, vts & 0xff}

static const struct regval_list ov5640_qvga_regs[] = {
	{0x3212, 0x03},
	{0x3814, 0x31},
	{0x3815, 0x31},
	IMAGEWIN_SET(0, 1311, 2, 973),
	OUTSIZE_SET(16, 16, QVGA_WIDTH, QVGA_HEIGHT),
	HTS_SET(HTS_QVGA),
	VTS_SET(VTS_QVGA),
	{0x3212, 0x13},
	{0x3212, 0xa3},
	ENDMARKER,
};

static const struct regval_list ov5640_vga_regs[] = {
	{0x3212, 0x03},
	{0x3814, 0x31},
	{0x3815, 0x31},
	IMAGEWIN_SET(0, 1855, 4, 1377),
	OUTSIZE_SET(16, 16, VGA_WIDTH, VGA_HEIGHT),
	HTS_SET(HTS_VGA),
	VTS_SET(HTS_VGA),
	{0x3212, 0x13},
	{0x3212, 0xa3},
	ENDMARKER,
};

static const struct regval_list ov5640_720p_regs[] = {
	{0x3212, 0x03},
	{0x3814, 0x31},
	{0x3815, 0x31},
	IMAGEWIN_SET(0, 2623, 240, 1947),
	OUTSIZE_SET(16, 6, WIDTH_720P, HEIGHT_720P),
	HTS_SET(HTS_720P),
	VTS_SET(HTS_720P),
	{0x3212, 0x13},
	{0x3212, 0xa3},
	ENDMARKER,
};

static const struct regval_list ov5640_1080p_regs[] = {
	{0x3212, 0x03},
	{0x3814, 0x11},
	{0x3815, 0x11},

	IMAGEWIN_SET(336, 2287, 434, 1521),
	OUTSIZE_SET(16, 4, WIDTH_1080P, HEIGHT_1080P),
	HTS_SET(HTS_1080P),
	VTS_SET(VTS_1080P),

	{0x3212, 0x13},
	{0x3212, 0xa3},
	ENDMARKER,
};

#define OV5640_SIZE(n, w, h, hs, vs, r) \
	    {.name = n, .width = w , .height = h, .hts = hs, .vts= vs, .regs = r }

static const struct ov5640_win_size ov5640_supported_win_sizes[] = {
	OV5640_SIZE("QVGA", QVGA_WIDTH, QVGA_HEIGHT, HTS_QVGA, VTS_QVGA, ov5640_qvga_regs),
	OV5640_SIZE("VGA", VGA_WIDTH, VGA_HEIGHT, HTS_VGA, VTS_VGA, ov5640_vga_regs),
	OV5640_SIZE("720P", WIDTH_720P, HEIGHT_720P, HTS_720P, VTS_720P, ov5640_720p_regs),
	OV5640_SIZE("1080P", WIDTH_1080P, HEIGHT_1080P, HTS_1080P, VTS_1080P, ov5640_1080p_regs),
};

static const struct regval_list ov5640_yuyv_regs[] = {
	{0x4300, 0x30},
	{0x501f, 0x00},
	{0x4713, 0x03},
	{0x4407, 0x03},
	ENDMARKER,
};

static const struct regval_list ov5640_uyvy_regs[] = {
	{0x4300, 0x32},
	{0x501f, 0x00},
	{0x4713, 0x03},
	{0x4407, 0x03},
	ENDMARKER,
};

static const struct regval_list ov5640_rgb565_be_regs[] = {
	{0x4300, 0x6f},
	{0x501f, 0x01},
	{0x4713, 0x03},
	{0x4407, 0x03},
	ENDMARKER,
};

static u32 ov5640_codes[] = {
	MEDIA_BUS_FMT_YUYV8_2X8,
	MEDIA_BUS_FMT_UYVY8_2X8,
	MEDIA_BUS_FMT_RGB565_2X8_BE,
};

struct regval_list ov5640_init_regs[] = {
	{0x3008, 0x42},
	{0x3103, 0x03},
	{0x3017, 0xff},
	{0x3018, 0xff},

	{0x3630, 0x2e},
	{0x3632, 0xe2},
	{0x3633, 0x23},
	{0x3621, 0xe0},
	{0x3704, 0xa0},
	{0x3703, 0x5a},
	{0x3715, 0x78},
	{0x3717, 0x01},
	{0x370b, 0x60},
	{0x3705, 0x1a},
	{0x3905, 0x02},
	{0x3906, 0x10},
	{0x3901, 0x0a},
	{0x3731, 0x12},
	{0x3600, 0x08},
	{0x3601, 0x33},
	{0x302d, 0x60},
	{0x3a18, 0x00},
	{0x3a19, 0xf8},
	{0x3c01, 0x34},
	{0x3c04, 0x28},
	{0x3c05, 0x98},
	{0x3c06, 0x00},
	{0x3c07, 0x07},
	{0x3c08, 0x00},
	{0x3c09, 0x1c},
	{0x3c0a, 0x9c},
	{0x3c0b, 0x40},
	{0x460c, 0x20},

	{0x3C00, 0x04}, //set 50/60Hz
	{0x3C01, 0x34},
	{0x3C04, 0x28},
	{0x3C05, 0x98},
	{0x3C06, 0x00},
	{0x3C07, 0x07},
	{0x3C08, 0x00},
	{0x3C09, 0x1c},
	{0x3C0A, 0x9c},
	{0x3C0B, 0x40},

	{0x3A18, 0x01}, //set aec gain
	{0x3A19, 0xf8},
	{0x3620, 0x52},
	{0x371b, 0x20},
	{0x471c, 0x50},
	{0x3a13, 0x43},
	{0x3635, 0x1c},
	{0x3636, 0x03},
	{0x3634, 0x40},
	{0x3622, 0x01},

	{0x3820, 0x47},
	{0x3821, 0x01},
	{0x3618, 0x00},
	{0x3612, 0x29},
	{0x3708, 0x64},
	{0x3709, 0x52},
	{0x370c, 0x03},

	{0x3A02, 0x03},  //set aec
	{0x3A03, 0xd8},
	{0x3A14, 0x03},
	{0x3A15, 0xd8},
	{0x3A08, 0x01},
	{0x3A09, 0x27},
	{0x3A0A, 0x00},
	{0x3A0B, 0xf6},
	{0x3A0D, 0x04},
	{0x3A0E, 0x03},

	{0x4001, 0x02},  //set blc
	{0x4004, 0x02},
	{0x4005, 0x1a},

	{0x3000, 0x00},
	{0x3002, 0x1c},
	{0x3004, 0xff},
	{0x3006, 0xc3},
	{0x300E, 0x58},

	{0x460c, 0x20},
	{0x3824, 0x02},
	{0x5000, 0xa7},
	{0x5001, 0xa3},

	{0x3212, 0x03},
	{0x3406, 0x00},
	{0x3400, 0x04},
	{0x3401, 0x00},
	{0x3402, 0x04},
	{0x3403, 0x00},
	{0x3404, 0x04},
	{0x3405, 0x00},
	{0x3212, 0x13},
	{0x3212, 0xa3},

	{0x5180, 0xff},
	{0x5181, 0xf2},
	{0x5182, 0x00},
	{0x5183, 0x14},
	{0x5184, 0x25},
	{0x5185, 0x24},
	{0x5186, 0x0b},
	{0x5187, 0x11},
	{0x5188, 0x09},
	{0x5189, 0x75},
	{0x518a, 0x51},
	{0x518b, 0xff},
	{0x518c, 0xff},
	{0x518d, 0x42},
	{0x518e, 0x1c},
	{0x518f, 0x56},
	{0x5190, 0x2c},
	{0x5191, 0xf8},
	{0x5192, 0x04},
	{0x5193, 0x70},
	{0x5194, 0xf0},
	{0x5195, 0xf0},
	{0x5196, 0x03},
	{0x5197, 0x01},
	{0x5198, 0x07},
	{0x5199, 0x30},
	{0x519a, 0x04},
	{0x519b, 0x00},
	{0x519c, 0x05},
	{0x519d, 0x1e},
	{0x519e, 0x38},

	{0x5381, 0x1e},
	{0x5382, 0x5b},
	{0x5383, 0x14},
	{0x5384, 0x05},
	{0x5385, 0x77},
	{0x5386, 0x7c},
	{0x5387, 0x72},
	{0x5388, 0x58},
	{0x5389, 0x1a},
	{0x538A, 0x01},
	{0x538B, 0x98},

	{0x5300, 0x08},
	{0x5301, 0x30},
	{0x5302, 0x10},
	{0x5303, 0x00},
	{0x5304, 0x08},
	{0x5305, 0x30},
	{0x5306, 0x08},
	{0x5307, 0x16},
	{0x5309, 0x08},
	{0x530A, 0x30},
	{0x530B, 0x04},
	{0x530C, 0x06},
	{0x5025, 0x00},

	{0x5480, 0x01},
	{0x5481, 0x08},
	{0x5482, 0x14},
	{0x5483, 0x28},
	{0x5484, 0x51},
	{0x5485, 0x65},
	{0x5486, 0x71},
	{0x5487, 0x7d},
	{0x5488, 0x87},
	{0x5489, 0x91},
	{0x548A, 0x9a},
	{0x548B, 0xaa},
	{0x548C, 0xb8},
	{0x548D, 0xcd},
	{0x548E, 0xdd},
	{0x548F, 0xea},
	{0x5490, 0x1d},

	{0x5580, 0x06},
	{0x5583, 0x40},
	{0x5584, 0x40},
	{0x5589, 0x10},
	{0x558A, 0x00},
	{0x558B, 0xf8},
	{0x501d, 0x40},

	{0x5800, 0x3f},
	{0x5801, 0x23},
	{0x5802, 0x1c},
	{0x5803, 0x1a},
	{0x5804, 0x24},
	{0x5805, 0x38},
	{0x5806, 0x14},
	{0x5807, 0x0c},
	{0x5808, 0x07},
	{0x5809, 0x08},
	{0x580a, 0x0b},
	{0x580b, 0x15},
	{0x580c, 0x0c},
	{0x580d, 0x05},
	{0x580e, 0x00},
	{0x580f, 0x00},
	{0x5810, 0x04},
	{0x5811, 0x0d},
	{0x5812, 0x0b},
	{0x5813, 0x06},
	{0x5814, 0x00},
	{0x5815, 0x00},
	{0x5816, 0x05},
	{0x5817, 0x0c},
	{0x5818, 0x17},
	{0x5819, 0x0d},
	{0x581a, 0x09},
	{0x581b, 0x09},
	{0x581c, 0x0c},
	{0x581d, 0x18},
	{0x581e, 0x30},
	{0x581f, 0x25},
	{0x5820, 0x1a},
	{0x5821, 0x1a},
	{0x5822, 0x23},
	{0x5823, 0x37},
	{0x5824, 0x56},
	{0x5825, 0x55},
	{0x5826, 0x36},
	{0x5827, 0x54},
	{0x5828, 0x28},
	{0x5829, 0x37},
	{0x582a, 0x23},
	{0x582b, 0x22},
	{0x582c, 0x12},
	{0x582d, 0x56},
	{0x582e, 0x06},
	{0x582f, 0x31},
	{0x5830, 0x30},
	{0x5831, 0x31},
	{0x5832, 0x35},
	{0x5833, 0x26},
	{0x5834, 0x32},
	{0x5835, 0x22},
	{0x5836, 0x33},
	{0x5837, 0x24},
	{0x5838, 0x29},
	{0x5839, 0x35},
	{0x583a, 0x27},
	{0x583b, 0x25},
	{0x583c, 0x48},
	{0x583d, 0xdf},

	{0x3A0F, 0x2b},
	{0x3A10, 0x24},
	{0x3A1B, 0x2b},
	{0x3A1E, 0x24},
	{0x3A11, 0x56},
	{0x3A1F, 0x12},

	{0x4715, 0x04},
	{0x4730, 0x69},
	{0x4719, 0x03},

	{0x3503, 0x00},
	{0x3500, 0x00},
	{0x3501, 0x00},
	{0x3502, 0x00},
	{0x350A, 0x00},
	{0x350B, 0x80},

	ENDMARKER,
};

static int ov5640_set_contrast(struct ov5640_info *info, int val);
static int ov5640_set_saturation(struct ov5640_info *info, int val);
static int ov5640_set_brightness(struct ov5640_info *info, int val);
static int ov5640_set_sharpness(struct ov5640_info *info, int val);

int ov5640_read_i2c(struct i2c_client *client, u16 reg, u8 *val)
{
	int ret;
	unsigned char data[2] = { reg >> 8, reg & 0xff };

	ret = i2c_master_send(client, data, 2);
	if (ret < 2) {
		dev_err(&client->dev, "%s: i2c read error, reg: %x\n",
					__func__, reg);
		return ret < 0 ? ret : -EIO;
	}

	ret = i2c_master_recv(client, val, 1);
	if (ret < 1) {
		dev_err(&client->dev, "%s: i2c read error, reg: %x\n",
					__func__, reg);
		return ret < 0 ? ret : -EIO;
	}
	return 0;
}

int ov5640_write_i2c(struct i2c_client *client, u16 reg, u8 val)
{
	int ret;
	unsigned char data[3] = { reg >> 8, reg & 0xff, val };

	ret = i2c_master_send(client, data, 3);
	if (ret < 3) {
		dev_err(&client->dev, "%s: i2c write error, reg: %x\n",
					__func__, reg);
		return ret < 0 ? ret : -EIO;
	}

	return 0;
}

static inline struct ov5640_info *to_ov5640_info(struct v4l2_subdev *sd)
{
	return container_of(sd, struct ov5640_info, subdev);
}

static inline struct v4l2_subdev *ctrl_to_sd(struct v4l2_ctrl *ctrl)
{
    return &container_of(ctrl->handler, struct ov5640_info, hdl)->subdev;
}

static int ov5640_write_array(struct i2c_client *client,
			const struct regval_list *vals)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ov5640_info *info = to_ov5640_info(sd);
	int ret;

	while ((vals->reg_num != 0xff) || (vals->value != 0xff)) {
		ret = ov5640_write_i2c(client, vals->reg_num, vals->value);
		v4l_dbg(2, info->debug, client, "array: 0x%04x, 0x%02x\n",
				vals->reg_num, vals->value);

		if (ret < 0)
			return ret;
		vals++;
	}
    return 0;
}

static int ov5640_reset(struct i2c_client *client)
{
	int ret;
	const struct regval_list ov5640_reset_regs[] = {
		{0x3103, 0x11},
		{0x3008, 0x82},
		ENDMARKER,
	};

	ret = ov5640_write_array(client, ov5640_reset_regs);
	if (ret)
		goto err;

	msleep(5);
err:
	dev_dbg(&client->dev, "%s: (ret %d)", __func__, ret);
	return ret;
}

static void _ov5640_set_power(struct ov5640_info *info, int on)
{
	if (on) {
		v4l2_clk_enable(info->clk);
		msleep(10);
		gpiod_set_value(info->gpio_rst, 0);
		gpiod_set_value(info->gpio_pdn, 1);
		msleep(5);
		gpiod_set_value(info->gpio_pdn, 0);
		msleep(5);
		gpiod_set_value(info->gpio_rst, 1);
		msleep(25);
	} else {
		v4l2_clk_disable(info->clk);
		gpiod_set_value(info->gpio_rst, 0);
		gpiod_set_value(info->gpio_pdn, 1);
	}
	info->streaming = 0;
}

static int ov5640_s_power(struct v4l2_subdev *sd, int on)
{
	struct ov5640_info *info = to_ov5640_info(sd);
	int ret = 0;

	mutex_lock(&info->lock);
	if (info->power != on) {
		_ov5640_set_power(info, on);
		if (on) {
			ov5640_reset(info->client);

			/* initialize the sensor with default data */
			v4l_dbg(1, info->debug, info->client, "%s: Init default", __func__);
			ret = ov5640_write_array(info->client, ov5640_init_regs);
			if (ret < 0)
				return ret;
		}
		info->power = on;
	}
	mutex_unlock(&info->lock);

	return 0;
}

static int ov5640_enum_mbus_code(struct v4l2_subdev *sd,
				struct v4l2_subdev_pad_config *cfg,
				struct v4l2_subdev_mbus_code_enum *code)
{
	if (code->pad || code->index >= ARRAY_SIZE(ov5640_codes))
		return -EINVAL;

	code->code = ov5640_codes[code->index];
	return 0;
}

static const struct ov5640_win_size *ov5640_select_win(u32 *width, u32 *height)
{
	int i, default_size = ARRAY_SIZE(ov5640_supported_win_sizes) - 1;

	for (i = 0; i < ARRAY_SIZE(ov5640_supported_win_sizes); i++) {
		if (ov5640_supported_win_sizes[i].width  >= *width &&
					ov5640_supported_win_sizes[i].height >= *height) {
			*width = ov5640_supported_win_sizes[i].width;
			*height = ov5640_supported_win_sizes[i].height;
			return &ov5640_supported_win_sizes[i];
		}
	}

	*width = ov5640_supported_win_sizes[default_size].width;
	*height = ov5640_supported_win_sizes[default_size].height;
	return &ov5640_supported_win_sizes[default_size];
}

static int ov5640_set_pll(struct ov5640_info *info, int xvclk, int pclk)
{
	int tmp_clk = 0, select = 0, save = 0;
	int div = 0, val = 0;
    unsigned int pre_div0[] = {1, 1, 2, 3, 4, 1, 6, 2,
				8, 1, 1, 1, 1, 1, 1, 1};
    unsigned int pclk_div[] = {1, 2, 4, 8};
	static struct regval_list ov5640_pll_regs[] = {
		{0x3108, 0x01},
		{0x3824, 0x01},
		{0x3034, 0x1a},
		{0x3035, 0x21},
		{0x3036, 0x2d},
		{0x3037, 0x10},
		ENDMARKER,
	};

	tmp_clk = pclk * (ov5640_pll_regs[3].value & 0xf);

	select = (ov5640_pll_regs[0].value >> 4) & 0x3;
	tmp_clk = tmp_clk * pclk_div[select];

	if ((ov5640_pll_regs[2].value & 0xf) == 0x08)
		tmp_clk = tmp_clk * 2;
	else if ((ov5640_pll_regs[2].value & 0xf) == 0xa)
		tmp_clk = tmp_clk * 5 / 2;

	if ((ov5640_pll_regs[5].value >> 4) & 0x1)
		tmp_clk = tmp_clk * 2;

	if (tmp_clk > 1000) {
		v4l_err(info->client, "fps is too high\n");
	}

	save = tmp_clk;
	while (1) {
		tmp_clk = save * ((ov5640_pll_regs[3].value >> 4) & 0xf);
		if (tmp_clk < 500)
			ov5640_pll_regs[3].value = ov5640_pll_regs[3].value + 0x10;
		else if (tmp_clk > 1000)
			ov5640_pll_regs[3].value = ov5640_pll_regs[3].value - 0x10;
		else
			break;
	}

	select = ov5640_pll_regs[5].value & 0xf;
	div = xvclk / pre_div0[select];

	val = tmp_clk / div;
	if ((ov5640_pll_regs[5].value >> 7) & 0x1)
		ov5640_pll_regs[4].value = (val / 2) << 1;
	else
		ov5640_pll_regs[4].value = val;

	return ov5640_write_array(info->client, ov5640_pll_regs);
}

static int ov5640_set_timeperframe(struct ov5640_info *info, enum ov5640_fps fps)
{
	int xvclk = 0, hts = 0, vts = 0;
	int pclk = 0, timeperframe = 0;

	xvclk = v4l2_clk_get_rate(info->clk) / 1000000;
	if (xvclk == 0 || !info->win)
		return -1;

	switch (fps) {
		case OV5640_7FPS:
			timeperframe = 7;
			break;
		case OV5640_30FPS:
			timeperframe = 30;
			break;
		default:
			timeperframe = 15;
			break;
	}

	hts = info->win->hts;
	vts = info->win->vts;

	pclk = hts * vts * timeperframe * 2 / 1000 / 1000;

	return ov5640_set_pll(info, xvclk, pclk);
}

static int ov5640_set_params(struct ov5640_info *info, u32 code)
{
	struct i2c_client *client = info->client;
	const struct regval_list *selected_cfmt_regs;
	int ret;

	if (!info->win) {
		v4l_err(client, "ov5640 win is NULL\n");
		return -1;
	}

	/* select format */
	info->cfmt_code = 0;
	switch (code) {
		case MEDIA_BUS_FMT_RGB565_2X8_BE:
			v4l_dbg(1, info->debug, client, "%s: Selected cfmt RGB565 BE", __func__);
			selected_cfmt_regs = ov5640_rgb565_be_regs;
			break;
		case MEDIA_BUS_FMT_YUYV8_2X8:
			v4l_dbg(1, info->debug, client, "%s: Selected cfmt YUYV (YUV422)", __func__);
			selected_cfmt_regs = ov5640_yuyv_regs;
			break;
		default:
		case MEDIA_BUS_FMT_UYVY8_2X8:
			v4l_dbg(1, info->debug, client, "%s: Selected cfmt UYVY", __func__);
			selected_cfmt_regs = ov5640_uyvy_regs;
	}

	ov5640_set_timeperframe(info, info->fps);

	ov5640_set_contrast(info, info->contrast_val);
	ov5640_set_saturation(info, info->saturation_val);
	ov5640_set_brightness(info, info->brightness_val);
	ov5640_set_sharpness(info, info->sharpness_val);

	v4l_dbg(1, info->debug, client, "%s: Set size to %s", __func__, info->win->name);
	/* set size win */
	ret = ov5640_write_array(client, info->win->regs);
	if (ret < 0)
		goto err;

	/* set cfmt */
	ret = ov5640_write_array(client, selected_cfmt_regs);
	if (ret < 0)
		goto err;

	info->cfmt_code = code;
	info->streaming = 1;

	return 0;

err:
	v4l_err(client, "%s: Error %d", __func__, ret);
	ov5640_reset(client);
	info->win = NULL;

	return ret;
}

static int ov5640_get_fmt(struct v4l2_subdev *sd, struct v4l2_subdev_pad_config *cfg,
			struct v4l2_subdev_format *fmt)
{
	struct ov5640_info *info = to_ov5640_info(sd);
	struct v4l2_mbus_framefmt *mf = &fmt->format;

	if (fmt->pad)
		return -EINVAL;

	if (!info->win) {
		u32 width = SVGA_WIDTH, height = SVGA_HEIGHT;
		info->win = ov5640_select_win(&width, &height);
		info->cfmt_code = MEDIA_BUS_FMT_UYVY8_2X8;
	}

	mf->width   = info->win->width;
	mf->height  = info->win->height;
	mf->code    = info->cfmt_code;

	switch (mf->code) {
		case MEDIA_BUS_FMT_RGB565_2X8_BE:
		case MEDIA_BUS_FMT_RGB565_2X8_LE:
			mf->colorspace = V4L2_COLORSPACE_SRGB;
			break;
		default:
		case MEDIA_BUS_FMT_YUYV8_2X8:
		case MEDIA_BUS_FMT_UYVY8_2X8:
			mf->colorspace = V4L2_COLORSPACE_JPEG;
	}
	mf->field   = V4L2_FIELD_NONE;
	return 0;
}

static int ov5640_set_fmt(struct v4l2_subdev *sd,
				struct v4l2_subdev_pad_config *cfg,
				struct v4l2_subdev_format *fmt)
{
	struct v4l2_mbus_framefmt *mf = &fmt->format;
	struct ov5640_info *info = to_ov5640_info(sd);

	if (fmt->pad)
		return -EINVAL;

	info->win = ov5640_select_win(&mf->width, &mf->height);
	info->cfmt_code = mf->code;

	mf->field = V4L2_FIELD_NONE;

	switch (mf->code) {
		case MEDIA_BUS_FMT_RGB565_2X8_BE:
		case MEDIA_BUS_FMT_RGB565_2X8_LE:
			mf->colorspace = V4L2_COLORSPACE_SRGB;
			break;
		default:
			mf->code = MEDIA_BUS_FMT_UYVY8_2X8;
		case MEDIA_BUS_FMT_YUYV8_2X8:
		case MEDIA_BUS_FMT_UYVY8_2X8:
			mf->colorspace = V4L2_COLORSPACE_JPEG;
	}

	if (fmt->which == V4L2_SUBDEV_FORMAT_ACTIVE)
		return ov5640_set_params(info, mf->code);

	if (cfg)
		cfg->try_fmt = *mf;

	return 0;
}

static int ov5640_g_frame_interval(struct v4l2_subdev *sd,
			struct v4l2_subdev_frame_interval *fi)
{
	struct ov5640_info *info = to_ov5640_info(sd);

	mutex_lock(&info->lock);
	fi->interval = *info->interval;
	mutex_unlock(&info->lock);

	return 0;
}

static int ov5640_s_frame_interval(struct v4l2_subdev *sd,
			struct v4l2_subdev_frame_interval *fi)
{
	struct ov5640_info *info = to_ov5640_info(sd);
	int fps = 0, min = 0, i = 0, diff = 0;

	v4l_dbg(1, info->debug, info->client, "Setting %d/%d frame interval\n",
			fi->interval.numerator, fi->interval.denominator);

	mutex_lock(&info->lock);
	fps = fi->interval.denominator / fi->interval.numerator;
	min = fps;

	for (i = 0; i < ARRAY_SIZE(ov5640_interval); i++) {
		diff = abs(ov5640_interval[i].denominator /
				ov5640_interval[i].numerator - fps);
		if (diff < min) {
			info->interval = &ov5640_interval[i];
			info->fps = i;
		}
	}
	mutex_unlock(&info->lock);

	return 0;
}

static int ov5640_s_stream(struct v4l2_subdev *sd, int on)
{
	struct ov5640_info *info = to_ov5640_info(sd);
	int ret = 0;
	struct regval_list ov5640_wakeup_regs[] = {
		{0x3008, 0x02},
		ENDMARKER,
	};

	if (info->streaming != on) {
		if (on) {
			if (!info->win) {
				u32 width = SVGA_WIDTH, height = SVGA_HEIGHT;
				info->win = ov5640_select_win(&width, &height);
				info->cfmt_code = MEDIA_BUS_FMT_UYVY8_2X8;
			}

			ret = ov5640_set_params(info, info->cfmt_code);
			if (ret < 0)
				return ret;

			ret = ov5640_write_array(info->client, ov5640_wakeup_regs);
			if (ret < 0)
				return ret;
			msleep(5);
		} else {
			ret = ov5640_reset(info->client);
			if (ret < 0)
				return ret;
		}

		info->streaming = on;
	}

	return 0;
}

static const struct v4l2_subdev_core_ops ov5640_core_ops = {
	.s_power = ov5640_s_power,
};

static const struct v4l2_subdev_pad_ops ov5640_pad_ops = {
	.enum_mbus_code = ov5640_enum_mbus_code,
	.get_fmt = ov5640_get_fmt,
	.set_fmt = ov5640_set_fmt,
};

static const struct v4l2_subdev_video_ops ov5640_video_ops = {
	.s_stream = ov5640_s_stream,
	.g_frame_interval = ov5640_g_frame_interval,
	.s_frame_interval = ov5640_s_frame_interval,
};

static struct v4l2_subdev_ops ov5640_subdev_ops = {
	.pad = &ov5640_pad_ops,
	.core = &ov5640_core_ops,
	.video = &ov5640_video_ops,
};

static int ov5640_set_contrast(struct ov5640_info *info, int val)
{
	struct regval_list ov5640_contrast_regs[] = {
		{0x3212, 0x03},
		{0x5586, 0x28},
		{0x5585, 0x18},
		{0x3212, 0x13},
		{0x3212, 0xa3},
		ENDMARKER,
	};

	switch (val) {
		case 1:
			ov5640_contrast_regs[1].value = 0x24;
			ov5640_contrast_regs[2].value = 0x10;
			break;
		case 0:
			ov5640_contrast_regs[1].value = 0x20;
			ov5640_contrast_regs[2].value = 0x00;
			break;
		case -1:
			ov5640_contrast_regs[1].value = 0x1c;
			ov5640_contrast_regs[2].value = 0x1c;
			break;
		case -2:
			ov5640_contrast_regs[1].value = 0x18;
			ov5640_contrast_regs[2].value = 0x18;
			break;
		default:
			break;
	}

	return ov5640_write_array(info->client, ov5640_contrast_regs);
}

static int ov5640_set_saturation(struct ov5640_info *info, int val)
{
	struct regval_list ov5640_saturation_regs[] = {
		{0x3212, 0x03},
		{0x5381, 0x1c},
		{0x5382, 0x5a},
		{0x5383, 0x06},

		{0x5384, 0x24},
		{0x5385, 0x8f},
		{0x5386, 0xb3},
		{0x5387, 0xb6},
		{0x5388, 0xb3},
		{0x5389, 0x03},

		{0x538b, 0x98},
		{0x538a, 0x01},
		{0x3212, 0x13},
		{0x3212, 0xa3},
		ENDMARKER,
	};

	switch (val) {
		case 1:
			ov5640_saturation_regs[4].value = 0x1f;
			ov5640_saturation_regs[5].value = 0x7a;
			ov5640_saturation_regs[6].value = 0x9a;
			ov5640_saturation_regs[7].value = 0x9c;
			ov5640_saturation_regs[8].value = 0x9a;
			ov5640_saturation_regs[9].value = 0x02;
			break;
		case 0:
			ov5640_saturation_regs[4].value = 0x1a;
			ov5640_saturation_regs[5].value = 0x66;
			ov5640_saturation_regs[6].value = 0x80;
			ov5640_saturation_regs[7].value = 0x82;
			ov5640_saturation_regs[8].value = 0x80;
			ov5640_saturation_regs[9].value = 0x02;
			break;
		case -1:
			ov5640_saturation_regs[4].value = 0x15;
			ov5640_saturation_regs[5].value = 0x52;
			ov5640_saturation_regs[6].value = 0x66;
			ov5640_saturation_regs[7].value = 0x68;
			ov5640_saturation_regs[8].value = 0x66;
			ov5640_saturation_regs[9].value = 0x02;
			break;
		case -2:
			ov5640_saturation_regs[4].value = 0x10;
			ov5640_saturation_regs[5].value = 0x3d;
			ov5640_saturation_regs[6].value = 0x4d;
			ov5640_saturation_regs[7].value = 0x4e;
			ov5640_saturation_regs[8].value = 0x4d;
			ov5640_saturation_regs[9].value = 0x01;
			break;
		default:
			break;
	}


	return ov5640_write_array(info->client, ov5640_saturation_regs);
}

static int ov5640_set_brightness(struct ov5640_info *info, int val)
{
	struct regval_list ov5640_brightness_regs[] = {
		{0x3212, 0x03},
		{0x5587, 0x30},
		{0x5588, 0x01},
		{0x3212, 0x13},
		{0x3212, 0xa3},
		ENDMARKER,
	};

	switch (val) {
		case 1:
			ov5640_brightness_regs[1].value = 0x10;
			ov5640_brightness_regs[2].value = 0x01;
			break;
		case 0:
			ov5640_brightness_regs[1].value = 0x00;
			ov5640_brightness_regs[2].value = 0x01;
			break;
		case -1:
			ov5640_brightness_regs[1].value = 0x10;
			ov5640_brightness_regs[2].value = 0x09;
			break;
		case -2:
			ov5640_brightness_regs[1].value = 0x30;
			ov5640_brightness_regs[2].value = 0x09;
			break;
		default:
			break;
	}

	return ov5640_write_array(info->client, ov5640_brightness_regs);
}

static int ov5640_set_sharpness(struct ov5640_info *info, int val)
{
	int ret = 0;
	struct regval_list ov5640_sharpness_regs[] = {
		{0x5308, 0x65},
		{0x5302, val},
		ENDMARKER,
	};
	struct regval_list ov5640_aout_sharpness_regs[] = {
		{0x5308, 0x25},
		{0x5300, 0x08},
		{0x5301, 0x30},
		{0x5302, 0x10},
		{0x5303, 0x00},
		{0x5309, 0x08},
		{0x530a, 0x30},
		{0x530b, 0x04},
		{0x530c, 0x06},
		ENDMARKER,
	};

	if (val < 33)
		ret = ov5640_write_array(info->client, ov5640_sharpness_regs);
	else
		ret = ov5640_write_array(info->client, ov5640_aout_sharpness_regs);

	return ret;
}

static int ov5640_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct v4l2_subdev *sd = ctrl_to_sd(ctrl);
	struct ov5640_info *info = to_ov5640_info(sd);

	if (!info->power)
		return -1;

	switch (ctrl->id) {
		case V4L2_CID_CONTRAST:
			info->contrast_val = ctrl->val;
			return ov5640_set_contrast(info, ctrl->val);
		case V4L2_CID_SATURATION:
			info->saturation_val = ctrl->val;
			return ov5640_set_saturation(info, ctrl->val);
		case V4L2_CID_BRIGHTNESS:
			info->brightness_val = ctrl->val;
			return ov5640_set_brightness(info, ctrl->val);
		case V4L2_CID_SHARPNESS:
			info->sharpness_val = ctrl->val;
			return ov5640_set_sharpness(info, ctrl->val);
		default:
			break;
	}

	return -1;
}

static const struct v4l2_ctrl_ops ov5640_ctrl_ops = {
	.s_ctrl = ov5640_s_ctrl,
};

static int ov5640_video_probe(struct i2c_client *client, struct ov5640_info *info)
{
	int ret = 0;
	u8 id_high, id_low;
	u16 id;

	_ov5640_set_power(info, 1);
	ret = ov5640_read_i2c(client, REG_CHIP_ID_HIGH, &id_high);
	if (ret < 0)
		goto probe_done;

	id = id_high << 8;

	ret = ov5640_read_i2c(client, REG_CHIP_ID_LOW, &id_low);
	if (ret < 0)
		goto probe_done;

	id |= id_low;

	dev_info(&client->dev, "Chip ID 0x%04x detected\n", id);
	if (id != OV5640_ID_LOW)
		ret = -ENODEV;

probe_done:
	_ov5640_set_power(info, 0);
	return ret;
}

static int ov5640_config_gpio(struct ov5640_info *info, struct i2c_client *client)
{
	int ret = 0;

	info->gpio_rst = devm_gpiod_get(&client->dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(info->gpio_rst)) {
		dev_err(&client->dev, "unable to get gpio_rst\n");
		info->gpio_rst = NULL;
		return -1;
	}

	ret = gpiod_direction_output(info->gpio_rst, 0);
	if (ret != 0)
		dev_err(&client->dev, "unable to set direction gpio_rst \n");

	info->gpio_pdn = devm_gpiod_get(&client->dev, "powerdown", GPIOD_OUT_LOW);
	if (IS_ERR(info->gpio_pdn)) {
		dev_err(&client->dev, "unable to get gpio_pdn\n");
		info->gpio_pdn = NULL;
		return -1;
	}

	ret = gpiod_direction_output(info->gpio_pdn, 0);
	if (ret != 0)
		dev_err(&client->dev, "unable to set direction gpio_pdn \n");

	gpiod_set_value(info->gpio_rst, 0);
	gpiod_set_value(info->gpio_pdn, 0);

	return ret;
}

static int ov5640_probe(struct i2c_client *client,
			const struct i2c_device_id *did)
{
	struct i2c_adapter *adapter = client->adapter;
	struct ov5640_info *info = NULL;

	v4l_info(client, "%s\n", __FUNCTION__);
	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA))
	  return -ENODEV;

	info = devm_kzalloc(&client->dev, sizeof(struct ov5640_info), GFP_KERNEL);
	if (info == NULL)
	  return -ENOMEM;

	info->debug  = 1;
	info->client = client;
	mutex_init(&info->lock);
	v4l2_i2c_subdev_init(&info->subdev, client, &ov5640_subdev_ops);

	v4l_info(client, "chip found @ 0x%02x (%s)\n",
				client->addr << 1, client->adapter->name);
	info->clk = v4l2_clk_get(&client->dev, "xvclk");
	if (IS_ERR(info->clk)) {
		v4l_err(client, "ov5640 get clk error");
		return -1;
	}

	ov5640_config_gpio(info, client);
	if (ov5640_video_probe(client, info) < 0) {
		v4l_err(client, "ov5640 video probe error\n");
		goto err_video_probe;
	}

	info->fps = OV5640_15FPS;
	info->contrast_val = 1;
	info->saturation_val = -1;
	info->brightness_val = 0;
	info->sharpness_val = 33;

	v4l2_ctrl_handler_init(&info->hdl, 3);
	v4l2_ctrl_new_std(&info->hdl, &ov5640_ctrl_ops,
				V4L2_CID_CONTRAST, -2, 2, 1, 1);
	v4l2_ctrl_new_std(&info->hdl, &ov5640_ctrl_ops,
				V4L2_CID_SATURATION, -2, 2, 1, -1);
	v4l2_ctrl_new_std(&info->hdl, &ov5640_ctrl_ops,
				V4L2_CID_BRIGHTNESS, -2, 2, 1, 0);
	v4l2_ctrl_new_std(&info->hdl, &ov5640_ctrl_ops,
				V4L2_CID_BRIGHTNESS, 0, 33, 1, 33);

	info->subdev.ctrl_handler = &info->hdl;
	if (info->hdl.error) {
		v4l_err(client, "ov5640 ctrl handler init error\n");
		goto err_video_probe;
	}

	return 0;
err_video_probe:
	kfree(info);
	return -1;
}

static int ov5640_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ov5640_info *info = to_ov5640_info(sd);

	v4l2_ctrl_handler_free(&info->hdl);
	v4l2_device_unregister_subdev(sd);
	v4l_info(client, "%s\n", __FUNCTION__);
	return 0;
}

static const struct i2c_device_id ov5640_id[] = {
	{ "ov5640", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ov5640_id);

static struct i2c_driver ov5640_i2c_driver = {
	.driver = {
		.name = "ov5640",
	},
	.probe      = ov5640_probe,
	.remove     = ov5640_remove,
	.id_table   = ov5640_id,
};

module_i2c_driver(ov5640_i2c_driver);
MODULE_LICENSE("GPL v2");
