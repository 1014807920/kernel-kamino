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

#define VAL_SET(x, mask, rshift, lshift)  \
		((((x) >> rshift) & mask) << lshift)
/*
 * DSP registers
 * register offset for BANK_SEL == BANK_SEL_DSP
 */
#define R_BYPASS    0x05 /* Bypass DSP */
#define   R_BYPASS_DSP_BYPAS    0x01 /* Bypass DSP, sensor out directly */
#define   R_BYPASS_USE_DSP      0x00 /* Use the internal DSP */
#define QS          0x44 /* Quantization Scale Factor */
#define CTRLI       0x50
#define   CTRLI_LP_DP           0x80
#define   CTRLI_ROUND           0x40
#define   CTRLI_V_DIV_SET(x)    VAL_SET(x, 0x3, 0, 3)
#define   CTRLI_H_DIV_SET(x)    VAL_SET(x, 0x3, 0, 0)
#define HSIZE       0x51 /* H_SIZE[7:0] (real/4) */
#define   HSIZE_SET(x)          VAL_SET(x, 0xFF, 2, 0)
#define VSIZE       0x52 /* V_SIZE[7:0] (real/4) */
#define   VSIZE_SET(x)          VAL_SET(x, 0xFF, 2, 0)
#define XOFFL       0x53 /* OFFSET_X[7:0] */
#define   XOFFL_SET(x)          VAL_SET(x, 0xFF, 0, 0)
#define YOFFL       0x54 /* OFFSET_Y[7:0] */
#define   YOFFL_SET(x)          VAL_SET(x, 0xFF, 0, 0)
#define VHYX        0x55 /* Offset and size completion */
#define   VHYX_VSIZE_SET(x)     VAL_SET(x, 0x1, (8+2), 7)
#define   VHYX_HSIZE_SET(x)     VAL_SET(x, 0x1, (8+2), 3)
#define   VHYX_YOFF_SET(x)      VAL_SET(x, 0x3, 8, 4)
#define   VHYX_XOFF_SET(x)      VAL_SET(x, 0x3, 8, 0)
#define DPRP        0x56
#define TEST        0x57 /* Horizontal size completion */
#define   TEST_HSIZE_SET(x)     VAL_SET(x, 0x1, (9+2), 7)
#define ZMOW        0x5A /* Zoom: Out Width  OUTW[7:0] (real/4) */
#define   ZMOW_OUTW_SET(x)      VAL_SET(x, 0xFF, 2, 0)
#define ZMOH        0x5B /* Zoom: Out Height OUTH[7:0] (real/4) */
#define   ZMOH_OUTH_SET(x)      VAL_SET(x, 0xFF, 2, 0)
#define ZMHH        0x5C /* Zoom: Speed and H&W completion */
#define   ZMHH_ZSPEED_SET(x)    VAL_SET(x, 0x0F, 0, 4)
#define   ZMHH_OUTH_SET(x)      VAL_SET(x, 0x1, (8+2), 2)
#define   ZMHH_OUTW_SET(x)      VAL_SET(x, 0x3, (8+2), 0)
#define BPADDR      0x7C /* SDE Indirect Register Access: Address */
#define BPDATA      0x7D /* SDE Indirect Register Access: Data */
#define CTRL2       0x86 /* DSP Module enable 2 */
#define   CTRL2_DCW_EN          0x20
#define   CTRL2_SDE_EN          0x10
#define   CTRL2_UV_ADJ_EN       0x08
#define   CTRL2_UV_AVG_EN       0x04
#define   CTRL2_CMX_EN          0x01
#define CTRL3       0x87 /* DSP Module enable 3 */
#define   CTRL3_BPC_EN          0x80
#define   CTRL3_WPC_EN          0x40
#define SIZEL       0x8C /* Image Size Completion */
#define   SIZEL_HSIZE8_11_SET(x) VAL_SET(x, 0x1, 11, 6)
#define   SIZEL_HSIZE8_SET(x)    VAL_SET(x, 0x7, 0, 3)
#define   SIZEL_VSIZE8_SET(x)    VAL_SET(x, 0x7, 0, 0)
#define HSIZE8      0xC0 /* Image Horizontal Size HSIZE[10:3] */
#define   HSIZE8_SET(x)         VAL_SET(x, 0xFF, 3, 0)
#define VSIZE8      0xC1 /* Image Vertical Size VSIZE[10:3] */
#define   VSIZE8_SET(x)         VAL_SET(x, 0xFF, 3, 0)
#define CTRL0       0xC2 /* DSP Module enable 0 */
#define   CTRL0_AEC_EN       0x80
#define   CTRL0_AEC_SEL      0x40
#define   CTRL0_STAT_SEL     0x20
#define   CTRL0_VFIRST       0x10
#define   CTRL0_YUV422       0x08
#define   CTRL0_YUV_EN       0x04
#define   CTRL0_RGB_EN       0x02
#define   CTRL0_RAW_EN       0x01
#define CTRL1       0xC3 /* DSP Module enable 1 */
#define   CTRL1_CIP          0x80
#define   CTRL1_DMY          0x40
#define   CTRL1_RAW_GMA      0x20
#define   CTRL1_DG           0x10
#define   CTRL1_AWB          0x08
#define   CTRL1_AWB_GAIN     0x04
#define   CTRL1_LENC         0x02
#define   CTRL1_PRE          0x01
#define R_DVP_SP    0xD3 /* DVP output speed control */
#define   R_DVP_SP_AUTO_MODE 0x80
#define   R_DVP_SP_DVP_MASK  0x3F /* DVP PCLK = sysclk (48)/[6:0] (YUV0);
				   *          = sysclk (48)/(2*[6:0]) (RAW);*/
#define IMAGE_MODE  0xDA /* Image Output Format Select */
#define   IMAGE_MODE_Y8_DVP_EN   0x40
#define   IMAGE_MODE_JPEG_EN     0x10
#define   IMAGE_MODE_YUV422      0x00
#define   IMAGE_MODE_RAW10       0x04 /* (DVP) */
#define   IMAGE_MODE_RGB565      0x08
#define   IMAGE_MODE_HREF_VSYNC  0x02 /* HREF timing select in DVP JPEG output
				       * mode (0 for HREF is same as sensor) */
#define   IMAGE_MODE_LBYTE_FIRST 0x01 /* Byte swap enable for DVP
				       *    1: Low byte first UYVY (C2[4] =0)
				       *        VYUY (C2[4] =1)
				       *    0: High byte first YUYV (C2[4]=0)
				       *        YVYU (C2[4] = 1) */
#define RESET       0xE0 /* Reset */
#define   RESET_MICROC       0x40
#define   RESET_SCCB         0x20
#define   RESET_JPEG         0x10
#define   RESET_DVP          0x04
#define   RESET_IPU          0x02
#define   RESET_CIF          0x01
#define REGED       0xED /* Register ED */
#define   REGED_CLK_OUT_DIS  0x10
#define MS_SP       0xF0 /* SCCB Master Speed */
#define SS_ID       0xF7 /* SCCB Slave ID */
#define SS_CTRL     0xF8 /* SCCB Slave Control */
#define   SS_CTRL_ADD_AUTO_INC  0x20
#define   SS_CTRL_EN            0x08
#define   SS_CTRL_DELAY_CLK     0x04
#define   SS_CTRL_ACC_EN        0x02
#define   SS_CTRL_SEN_PASS_THR  0x01
#define MC_BIST     0xF9 /* Microcontroller misc register */
#define   MC_BIST_RESET           0x80 /* Microcontroller Reset */
#define   MC_BIST_BOOT_ROM_SEL    0x40
#define   MC_BIST_12KB_SEL        0x20
#define   MC_BIST_12KB_MASK       0x30
#define   MC_BIST_512KB_SEL       0x08
#define   MC_BIST_512KB_MASK      0x0C
#define   MC_BIST_BUSY_BIT_R      0x02
#define   MC_BIST_MC_RES_ONE_SH_W 0x02
#define   MC_BIST_LAUNCH          0x01
#define BANK_SEL    0xFF /* Register Bank Select */
#define   BANK_SEL_DSP     0x00
#define   BANK_SEL_SENS    0x01

/*
 * Sensor registers
 * register offset for BANK_SEL == BANK_SEL_SENS
 */
#define GAIN        0x00 /* AGC - Gain control gain setting */
#define COM1        0x03 /* Common control 1 */
#define   COM1_1_DUMMY_FR          0x40
#define   COM1_3_DUMMY_FR          0x80
#define   COM1_7_DUMMY_FR          0xC0
#define   COM1_VWIN_LSB_UXGA       0x0F
#define   COM1_VWIN_LSB_SVGA       0x0A
#define   COM1_VWIN_LSB_CIF        0x06
#define REG04       0x04 /* Register 04 */
#define   REG04_DEF             0x20 /* Always set */
#define   REG04_HFLIP_IMG       0x80 /* Horizontal mirror image ON/OFF */
#define   REG04_VFLIP_IMG       0x40 /* Vertical flip image ON/OFF */
#define   REG04_VREF_EN         0x10
#define   REG04_HREF_EN         0x08
#define   REG04_AEC_SET(x)      VAL_SET(x, 0x3, 0, 0)
#define REG08       0x08 /* Frame Exposure One-pin Control Pre-charge Row Num */
#define COM2        0x09 /* Common control 2 */
#define   COM2_SOFT_SLEEP_MODE  0x10 /* Soft sleep mode */
				     /* Output drive capability */
#define   COM2_OCAP_Nx_SET(N)   (((N) - 1) & 0x03) /* N = [1x .. 4x] */
#define PID         0x0A /* Product ID Number MSB */
#define VER         0x0B /* Product ID Number LSB */
#define COM3        0x0C /* Common control 3 */
#define   COM3_BAND_50H        0x04 /* 0 For Banding at 60H */
#define   COM3_BAND_AUTO       0x02 /* Auto Banding */
#define   COM3_SING_FR_SNAPSH  0x01 /* 0 For enable live video output after the
				     * snapshot sequence*/
#define AEC         0x10 /* AEC[9:2] Exposure Value */
#define CLKRC       0x11 /* Internal clock */
#define   CLKRC_EN             0x80
#define   CLKRC_DIV_SET(x)     (((x) - 1) & 0x1F) /* CLK = XVCLK/(x) */
#define COM7        0x12 /* Common control 7 */
#define   COM7_SRST            0x80 /* Initiates system reset. All registers are
				     * set to factory default values after which
				     * the chip resumes normal operation */
#define   COM7_RES_UXGA        0x00 /* Resolution selectors for UXGA */
#define   COM7_RES_SVGA        0x40 /* SVGA */
#define   COM7_RES_CIF         0x20 /* CIF */
#define   COM7_ZOOM_EN         0x04 /* Enable Zoom mode */
#define   COM7_COLOR_BAR_TEST  0x02 /* Enable Color Bar Test Pattern */
#define COM8        0x13 /* Common control 8 */
#define   COM8_DEF             0xC0 /* Banding filter ON/OFF */
#define   COM8_BNDF_EN         0x20 /* Banding filter ON/OFF */
#define   COM8_AGC_EN          0x04 /* AGC Auto/Manual control selection */
#define   COM8_AEC_EN          0x01 /* Auto/Manual Exposure control */
#define COM9        0x14 /* Common control 9
			  * Automatic gain ceiling - maximum AGC value [7:5]*/
#define   COM9_AGC_GAIN_2x     0x00 /* 000 :   2x */
#define   COM9_AGC_GAIN_4x     0x20 /* 001 :   4x */
#define   COM9_AGC_GAIN_8x     0x40 /* 010 :   8x */
#define   COM9_AGC_GAIN_16x    0x60 /* 011 :  16x */
#define   COM9_AGC_GAIN_32x    0x80 /* 100 :  32x */
#define   COM9_AGC_GAIN_64x    0xA0 /* 101 :  64x */
#define   COM9_AGC_GAIN_128x   0xC0 /* 110 : 128x */
#define COM10       0x15 /* Common control 10 */
#define   COM10_PCLK_HREF      0x20 /* PCLK output qualified by HREF */
#define   COM10_PCLK_RISE      0x10 /* Data is updated at the rising edge of
				     * PCLK (user can latch data at the next
				     * falling edge of PCLK).
				     * 0 otherwise. */
#define   COM10_HREF_INV       0x08 /* Invert HREF polarity:
				     * HREF negative for valid data*/
#define   COM10_VSINC_INV      0x02 /* Invert VSYNC polarity */
#define HSTART      0x17 /* Horizontal Window start MSB 8 bit */
#define HEND        0x18 /* Horizontal Window end MSB 8 bit */
#define VSTART      0x19 /* Vertical Window start MSB 8 bit */
#define VEND        0x1A /* Vertical Window end MSB 8 bit */
#define MIDH        0x1C /* Manufacturer ID byte - high */
#define MIDL        0x1D /* Manufacturer ID byte - low  */
#define AEW         0x24 /* AGC/AEC - Stable operating region (upper limit) */
#define AEB         0x25 /* AGC/AEC - Stable operating region (lower limit) */
#define VV          0x26 /* AGC/AEC Fast mode operating region */
#define   VV_HIGH_TH_SET(x)      VAL_SET(x, 0xF, 0, 4)
#define   VV_LOW_TH_SET(x)       VAL_SET(x, 0xF, 0, 0)
#define REG2A       0x2A /* Dummy pixel insert MSB */
#define FRARL       0x2B /* Dummy pixel insert LSB */
#define ADDVFL      0x2D /* LSB of insert dummy lines in Vertical direction */
#define ADDVFH      0x2E /* MSB of insert dummy lines in Vertical direction */
#define YAVG        0x2F /* Y/G Channel Average value */
#define REG32       0x32 /* Common Control 32 */
#define   REG32_PCLK_DIV_2    0x80 /* PCLK freq divided by 2 */
#define   REG32_PCLK_DIV_4    0xC0 /* PCLK freq divided by 4 */
#define ARCOM2      0x34 /* Zoom: Horizontal start point */
#define REG45       0x45 /* Register 45 */
#define FLL         0x46 /* Frame Length Adjustment LSBs */
#define FLH         0x47 /* Frame Length Adjustment MSBs */
#define COM19       0x48 /* Zoom: Vertical start point */
#define ZOOMS       0x49 /* Zoom: Vertical start point */
#define COM22       0x4B /* Flash light control */
#define COM25       0x4E /* For Banding operations */
#define BD50        0x4F /* 50Hz Banding AEC 8 LSBs */
#define BD60        0x50 /* 60Hz Banding AEC 8 LSBs */
#define REG5D       0x5D /* AVGsel[7:0],   16-zone average weight option */
#define REG5E       0x5E /* AVGsel[15:8],  16-zone average weight option */
#define REG5F       0x5F /* AVGsel[23:16], 16-zone average weight option */
#define REG60       0x60 /* AVGsel[31:24], 16-zone average weight option */
#define HISTO_LOW   0x61 /* Histogram Algorithm Low Level */
#define HISTO_HIGH  0x62 /* Histogram Algorithm High Level */

/*
 * ID
 */
#define MANUFACTURER_ID	0x7FA2
#define PID_OV2640	0x2642
#define VERSION(pid, ver) ((pid << 8) | (ver & 0xFF))

#define ENDMARKER { 0xff, 0xff }

struct regval_list {
	u8 reg_num;
	u8  value;
};

struct ov2640_framesize {
	u16 width;
	u16 height;
	struct regval_list *regs;
	u16 array_size;
};

struct ov2640_win_size {
	char *name;
	u32  width;
	u32  height;
	const struct regval_list *regs;
};

enum ov2640_fps {
	OV2640_7FPS,
	OV2640_15FPS,
	OV2640_30FPS,
};

struct ov2640_info {
	struct i2c_client *client;
	struct gpio_desc *gpio_rst;
	struct gpio_desc *gpio_pdn;

	struct v4l2_ctrl_handler hdl;
	struct v4l2_clk *clk;
	struct v4l2_fract *interval;
	struct v4l2_subdev subdev;
	struct mutex lock;
	const struct ov2640_win_size *win;

	enum ov2640_fps fps;
	u32 cfmt_code;
	int power;
	int streaming;
	int contrast_val;
	int saturation_val;
	int brightness_val;
	int exposure_val;
	int debug;
	bool need_power;
	bool need_streaming;
};

struct v4l2_fract ov2640_interval[] = {
	{1,  7}, //7fps
	{1,  15}, //15fps
	{1,  30}, //30fps
};

#define PER_SIZE_REG_SEQ(x, y, v_div, h_div, pclk_div)  \
	{ ZMOW, ZMOW_OUTW_SET(x) },         \
    { ZMOH, ZMOH_OUTH_SET(y) },         \
    { ZMHH, ZMHH_OUTW_SET(x) | ZMHH_OUTH_SET(y) },  \
    { RESET, 0x00}

static const struct regval_list ov2640_qcif_regs[] = {
	PER_SIZE_REG_SEQ(QCIF_WIDTH, QCIF_HEIGHT, 3, 3, 4),
	ENDMARKER,
};

static const struct regval_list ov2640_qvga_regs[] = {
	PER_SIZE_REG_SEQ(QVGA_WIDTH, QVGA_HEIGHT, 2, 2, 4),
	ENDMARKER,
};

static const struct regval_list ov2640_cif_regs[] = {
	PER_SIZE_REG_SEQ(CIF_WIDTH, CIF_HEIGHT, 2, 2, 8),
	ENDMARKER,
};

static const struct regval_list ov2640_vga_regs[] = {
	PER_SIZE_REG_SEQ(VGA_WIDTH, VGA_HEIGHT, 0, 0, 2),
	ENDMARKER,
};

static const struct regval_list ov2640_svga_regs[] = {
	PER_SIZE_REG_SEQ(SVGA_WIDTH, SVGA_HEIGHT, 1, 1, 2),
	ENDMARKER,
};

static const struct regval_list ov2640_xga_regs[] = {
	PER_SIZE_REG_SEQ(XGA_WIDTH, XGA_HEIGHT, 0, 0, 2),
	ENDMARKER,
};

static const struct regval_list ov2640_sxga_regs[] = {
	PER_SIZE_REG_SEQ(SXGA_WIDTH, SXGA_HEIGHT, 0, 0, 2),
	ENDMARKER,
};

static const struct regval_list ov2640_uxga_regs[] = {
	PER_SIZE_REG_SEQ(UXGA_WIDTH, UXGA_HEIGHT, 0, 0, 0),
	ENDMARKER,
};

#define OV2640_SIZE(n, w, h, r) \
	    {.name = n, .width = w , .height = h, .regs = r }

static const struct ov2640_win_size ov2640_supported_win_sizes[] = {
	OV2640_SIZE("QCIF", QCIF_WIDTH, QCIF_HEIGHT, ov2640_qcif_regs),
	OV2640_SIZE("QVGA", QVGA_WIDTH, QVGA_HEIGHT, ov2640_qvga_regs),
	OV2640_SIZE("CIF", CIF_WIDTH, CIF_HEIGHT, ov2640_cif_regs),
	OV2640_SIZE("VGA", VGA_WIDTH, VGA_HEIGHT, ov2640_vga_regs),
	OV2640_SIZE("SVGA", SVGA_WIDTH, SVGA_HEIGHT, ov2640_svga_regs),
	OV2640_SIZE("SXGA", SXGA_WIDTH, SXGA_HEIGHT, ov2640_sxga_regs),
	OV2640_SIZE("UXGA", UXGA_WIDTH, UXGA_HEIGHT, ov2640_uxga_regs),
};

/*
 * Register settings for pixel formats
 */
static const struct regval_list ov2640_format_change_preamble_regs[] = {
	{ BANK_SEL, BANK_SEL_DSP },
	{ R_BYPASS, R_BYPASS_USE_DSP },
	ENDMARKER,
};

static const struct regval_list ov2640_yuyv_regs[] = {
	{ IMAGE_MODE, IMAGE_MODE_YUV422 },
	{ 0xd7, 0x03 },
	{ 0x33, 0xa0 },
	{ 0xe5, 0x1f },
	{ 0xe1, 0x67 },
	{ RESET,  0x00 },
	{ R_BYPASS, R_BYPASS_USE_DSP },
	ENDMARKER,
};

static const struct regval_list ov2640_uyvy_regs[] = {
	{ IMAGE_MODE, IMAGE_MODE_LBYTE_FIRST | IMAGE_MODE_YUV422 },
	{ 0xd7, 0x01 },
	{ 0x33, 0xa0 },
	{ 0xe1, 0x67 },
	{ RESET,  0x00 },
	{ R_BYPASS, R_BYPASS_USE_DSP },
	ENDMARKER,
};

static const struct regval_list ov2640_rgb565_be_regs[] = {
	{ IMAGE_MODE, IMAGE_MODE_RGB565 },
	{ 0xd7, 0x03 },
	{ RESET,  0x00 },
	{ R_BYPASS, R_BYPASS_USE_DSP },
	ENDMARKER,
};

static const struct regval_list ov2640_rgb565_le_regs[] = {
	{ IMAGE_MODE, IMAGE_MODE_LBYTE_FIRST | IMAGE_MODE_RGB565 },
	{ 0xd7, 0x03 },
	{ RESET,  0x00 },
	{ R_BYPASS, R_BYPASS_USE_DSP },
	ENDMARKER,
};

static u32 ov2640_codes[] = {
	MEDIA_BUS_FMT_YUYV8_2X8,
	MEDIA_BUS_FMT_UYVY8_2X8,
	MEDIA_BUS_FMT_RGB565_2X8_BE,
	MEDIA_BUS_FMT_RGB565_2X8_LE,
};

struct regval_list ov2640_init_regs[] = {
	{BANK_SEL, BANK_SEL_DSP},
	{0x2c,   0xff},
	{0x2e,   0xdf},
	{BANK_SEL, BANK_SEL_SENS},
	{0x3c,   0x32},
	{CLKRC, CLKRC_DIV_SET(1)},
	{COM2, COM2_OCAP_Nx_SET(1)},
	{REG04, REG04_DEF | REG04_HREF_EN},
	{COM8,  COM8_DEF | COM8_BNDF_EN | COM8_AGC_EN | COM8_AEC_EN},
	{COM9, COM9_AGC_GAIN_8x | 0x08},

	{0x2c, 0x0c},
	{0x33, 0x78},
	{0x3a, 0x33},
	{0x3b, 0xfB},

	{0x3e, 0x00},
	{0x43, 0x11},
	{0x16, 0x10},

	{0x39, 0x02},

	{0x35, 0x88},
	{0x22, 0x0a},
	{0x37, 0x40},
	{0x23, 0x00},
	{0x34, 0xa0},
	{0x06, 0x02},
	{0x06, 0x88},
	{0x07, 0xc0},
	{0x0d, 0xb7},
	{0x0e, 0x01},
	{0x4c, 0x00},
	{0x48, 0x00},
	{0x5B, 0x00},
	{0x42, 0x03},

	{0x4a, 0x81},
	{0x21, 0x99},

	{0x04, 0xa8},
	{0x5c, 0x00},
	{0x63, 0x00},
	{0x61, 0x70},
	{0x62, 0x80},
	{0x7c, 0x05},

	{0x20, 0x80},
	{0x28, 0x30},
	{0x6c, 0x00},
	{0x6d, 0x80},
	{0x6e, 0x00},
	{0x70, 0x02},
	{0x71, 0x94},
	{0x73, 0xc1},

	{0x6d, 0x00},

	{0x3d, 0x38},
	{0x2a, 0x00},
	{0x2b, 0x00},

	{0x46, 0x22},      //0x3f to 0x22
	{0x4f, 0xca},      //0x60 to 0xca
	{0x0c, 0x3a},      //0x3c to 0x3a, auto banding
	{0x5a, 0x57},      //add

	{0xff, 0x00},
	{0xe5, 0x7f},
	{0xf9, 0xc0},
	{0x41, 0x24},
	{0xe0, 0x14},
	{0x76, 0xff},
	{0x33, 0xa0},
	{0x42, 0x20},
	{0x43, 0x18},
	{0x4c, 0x00},
	{0x87, 0xd5},
	{0x88, 0x3f},
	{0xd7, 0x03},
	{0xd9, 0x10},
	{0xd3, 0x82},

	{0xc8, 0x08},
	{0xc9, 0x80},

	{0x90, 0x00},
	{0x91, 0x0e},
	{0x91, 0x1a},
	{0x91, 0x31},
	{0x91, 0x5a},
	{0x91, 0x69},
	{0x91, 0x75},
	{0x91, 0x7e},
	{0x91, 0x88},
	{0x91, 0x8f},
	{0x91, 0x96},
	{0x91, 0xa3},
	{0x91, 0xaf},
	{0x91, 0xc4},
	{0x91, 0xd7},
	{0x91, 0xe8},
	{0x91, 0x20},

	{0x92, 0x00},
	{0x93, 0x06},
	{0x93, 0xe3},
	{0x93, 0x05},
	{0x93, 0x05},
	{0x93, 0x00},
	{0x93, 0x04},
	{0x93, 0x00},
	{0x93, 0x00},
	{0x93, 0x00},
	{0x93, 0x00},
	{0x93, 0x00},
	{0x93, 0x00},
	{0x93, 0x00},

	{0x96, 0x00},
	{0x97, 0x08},
	{0x97, 0x19},
	{0x97, 0x02},
	{0x97, 0x0c},
	{0x97, 0x24},
	{0x97, 0x30},
	{0x97, 0x28},
	{0x97, 0x26},
	{0x97, 0x02},
	{0x97, 0x98},
	{0x97, 0x80},
	{0x97, 0x00},
	{0x97, 0x00},

	{0xc3, 0xed},
	{0xa4, 0x00},
	{0xa8, 0x00},
	{0xc5, 0x11},
	{0xc6, 0x51},
	{0xbf, 0x80},
	{0xc7, 0x10},
	{0xb6, 0x66},
	{0xb8, 0xa5},
	{0xb7, 0x64},
	{0xb9, 0x7c},
	{0xb3, 0xaf},
	{0xb4, 0x97},
	{0xb5, 0xff},
	{0xb0, 0xc5},
	{0xb1, 0x94},
	{0xb2, 0x0f},
	{0xc4, 0x5c},

	{0xc0, 0x64},
	{0xc1, 0x4b},
	{0x8c, 0x00},
	{0x86, 0x3d},

	{0xc3, 0xed},
	{0x7f, 0x00},

	{0xda, 0x01},        //0x00 to 0x01, UYVY

	{0xe5, 0x1f},
	{0xe1, 0x67},
	{0xe0, 0x00},
	{0xdd, 0xff},       //0x7f to 0xff
	{0x05, 0x00},
	{0xff, 0xff},

	ENDMARKER,
};

static struct regval_list ov2640_svga_mode_regs[] = {
	{0xff, 0x01},
	{0x12, 0x40},

	{0x17, 0x11},
	{0x18, 0x43},
	{0x19, 0x00},
	{0x1a, 0x4b},
	{0x03, 0x0a},      //add
	{0x32, 0x09},
	{0x37, 0xc0},

	{0x46, 0x22},
	{0x47, 0x00},
	{0x4f, 0xbb},
	{0x50, 0x9c},
	{0x5a, 0x57},
	{0x6d, 0x00},

	{0x3d, 0x38},

	{0x39, 0x92},
	{0x35, 0xda},
	{0x22, 0x1a},
	{0x37, 0xc3},
	{0x23, 0x00},
	{0x34, 0xc0},
	{0x36, 0x1a},
	{0x06, 0x88},
	{0x07, 0xc0},
	{0x0d, 0x87},
	{0x0e, 0x41},
	{0x4c, 0x00},

	{0xff, 0x00},
	{0xe0, 0x14},
	{0xc0, 0x64},
	{0xc1, 0x4b},
	{0x8c, 0x00},
	{0x86, 0x3d},
	{0x50, 0x00},
	{0x51, 0xc8},
	{0x52, 0x96},
	{0x53, 0x00},
	{0x54, 0x00},
	{0x55, 0x00},
	{0x57, 0x00},
	{0xd3, 0x82},

	ENDMARKER,
};

static struct regval_list ov2640_uxga_mode_regs[] = {
	{0xff, 0x01},
	{0x12, 0x00},

	{0x17, 0x11},
	{0x18, 0x75},
	{0x19, 0x01},
	{0x1a, 0x97},
	{0x03, 0x0f},      //add
	{0x32, 0x36},
	{0x37, 0x40},

	{0x46, 0x36},
	{0x47, 0x00},
	{0x4f, 0xbb},
	{0x50, 0x9c},
	{0x5a, 0x57},
	{0x6d, 0x80},

	{0x3d, 0x34},

	{0x39, 0x02},
	{0x35, 0x88},
	{0x22, 0x0a},
	{0x37, 0x40},
	{0x23, 0x00},
	{0x34, 0xa0},
	{0x36, 0x1a},
	{0x06, 0x02},
	{0x07, 0xc0},
	{0x0d, 0xb7},
	{0x0e, 0x01},
	{0x4c, 0x00},

	{0xff, 0x00},
	{0xe0, 0x14},
	{0xc0, 0xc8},
	{0xc1, 0x96},
	{0x8c, 0x00},
	{0x86, 0x3d},    //0x1d to 0x3d, enable dcw
	{0x50, 0x00},
	{0x51, 0x90},
	{0x52, 0x2c},
	{0x53, 0x00},
	{0x54, 0x00},
	{0x55, 0x88},
	{0x57, 0x00},
	{0xd3, 0x82},

	ENDMARKER,
};

static int ov2640_set_contrast(struct ov2640_info *info, int val);
static int ov2640_set_saturation(struct ov2640_info *info, int val);
static int ov2640_set_brightness(struct ov2640_info *info, int val);
static int ov2640_set_exposure(struct ov2640_info *info, int val);

static inline struct ov2640_info *to_ov2640_info(struct v4l2_subdev *sd)
{
	return container_of(sd, struct ov2640_info, subdev);
}

static inline struct v4l2_subdev *ctrl_to_sd(struct v4l2_ctrl *ctrl)
{
    return &container_of(ctrl->handler, struct ov2640_info, hdl)->subdev;
}
static int ov2640_write_array(struct i2c_client *client,
			const struct regval_list *vals)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ov2640_info *info = to_ov2640_info(sd);
	int ret;

	while ((vals->reg_num != 0xff) || (vals->value != 0xff)) {
		ret = i2c_smbus_write_byte_data(client,
					vals->reg_num, vals->value);
		v4l_dbg(2, info->debug, client, "array: 0x%02x, 0x%02x\n",
					vals->reg_num, vals->value);

		if (ret < 0)
			return ret;
		vals++;
	}
    return 0;
}

static int ov2640_reset(struct i2c_client *client)
{
	int ret;
	const struct regval_list reset_seq[] = {
		{BANK_SEL, BANK_SEL_SENS},
		{COM7, COM7_SRST},
		ENDMARKER,
	};

	ret = ov2640_write_array(client, reset_seq);
	if (ret)
		goto err;

	msleep(5);
err:
	dev_dbg(&client->dev, "%s: (ret %d)", __func__, ret);
	return ret;
}

static void _ov2640_set_power(struct ov2640_info *info, int on)
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

static int ov2640_s_power(struct v4l2_subdev *sd, int on)
{
	struct ov2640_info *info = to_ov2640_info(sd);
	int ret = 0;

	mutex_lock(&info->lock);
	if (info->power != on) {
		_ov2640_set_power(info, on);
		if (on) {
			ov2640_reset(info->client);

			/* initialize the sensor with default data */
			v4l_dbg(1, info->debug, info->client, "%s: Init default", __func__);
			ret = ov2640_write_array(info->client, ov2640_init_regs);
			if (ret < 0) {
				v4l_err(info->client, "ov2640 write init regs err\n");
				mutex_unlock(&info->lock);
				return ret;
			}
		}
		info->power = on;
	}
	mutex_unlock(&info->lock);

	return 0;
}

static int ov2640_enum_mbus_code(struct v4l2_subdev *sd,
				struct v4l2_subdev_pad_config *cfg,
				struct v4l2_subdev_mbus_code_enum *code)
{
	if (code->pad || code->index >= ARRAY_SIZE(ov2640_codes))
		return -EINVAL;

	code->code = ov2640_codes[code->index];
	return 0;
}

static const struct ov2640_win_size *ov2640_select_win(u32 *width, u32 *height)
{
	int i, default_size = ARRAY_SIZE(ov2640_supported_win_sizes) - 1;

	for (i = 0; i < ARRAY_SIZE(ov2640_supported_win_sizes); i++) {
		if (ov2640_supported_win_sizes[i].width  >= *width &&
					ov2640_supported_win_sizes[i].height >= *height) {
			*width = ov2640_supported_win_sizes[i].width;
			*height = ov2640_supported_win_sizes[i].height;
			return &ov2640_supported_win_sizes[i];
		}
	}

	*width = ov2640_supported_win_sizes[default_size].width;
	*height = ov2640_supported_win_sizes[default_size].height;
	return &ov2640_supported_win_sizes[default_size];
}

static int ov2640_set_timeperframe(struct ov2640_info *info, enum ov2640_fps fps)
{
	struct regval_list ov2640_clk_regs[] = {
		{0xff, 0x01},
		{0x11, 0x80},
		ENDMARKER,
	};

	if (info->win->width <= SVGA_WIDTH) {
		if (fps == OV2640_7FPS)
			ov2640_clk_regs[1].value = 0x01;
		else if (fps == OV2640_15FPS)
			ov2640_clk_regs[1].value = 0x00;
		else
			ov2640_clk_regs[1].value = 0x80;
	} else {
		if (fps == OV2640_7FPS)
			ov2640_clk_regs[1].value = 0x00;
		else
			ov2640_clk_regs[1].value = 0x80;
	}

	return ov2640_write_array(info->client, ov2640_clk_regs);
}

static int ov2640_set_params(struct ov2640_info *info, u32 code)
{
	struct i2c_client *client = info->client;
	const struct regval_list *selected_cfmt_regs;
	int ret;

	if (!info->win) {
		v4l_err(client, "ov2640 win is NULL\n");
		return -1;
	}

	/* select format */
	info->cfmt_code = 0;
	switch (code) {
		case MEDIA_BUS_FMT_RGB565_2X8_BE:
			v4l_dbg(1, info->debug, client, "%s: Selected cfmt RGB565 BE", __func__);
			selected_cfmt_regs = ov2640_rgb565_be_regs;
			break;
		case MEDIA_BUS_FMT_RGB565_2X8_LE:
			v4l_dbg(1, info->debug, client, "%s: Selected cfmt RGB565 LE", __func__);
			selected_cfmt_regs = ov2640_rgb565_le_regs;
			break;
		case MEDIA_BUS_FMT_YUYV8_2X8:
			v4l_dbg(1, info->debug, client, "%s: Selected cfmt YUYV (YUV422)", __func__);
			selected_cfmt_regs = ov2640_yuyv_regs;
			break;
		default:
		case MEDIA_BUS_FMT_UYVY8_2X8:
			v4l_dbg(1, info->debug, client, "%s: Selected cfmt UYVY", __func__);
			selected_cfmt_regs = ov2640_uyvy_regs;
	}

	ov2640_set_timeperframe(info, info->fps);
	ov2640_set_exposure(info, info->exposure_val);
	if (info->win->width <= SVGA_WIDTH)
		ret = ov2640_write_array(client, ov2640_svga_mode_regs);
	else
		ret = ov2640_write_array(client, ov2640_uxga_mode_regs);

	ov2640_set_contrast(info, info->contrast_val);
	ov2640_set_saturation(info, info->saturation_val);
	ov2640_set_brightness(info, info->brightness_val);

	v4l_dbg(1, info->debug, client, "%s: Set size to %s", __func__, info->win->name);
	/* set size win */
	ret = ov2640_write_array(client, info->win->regs);
	if (ret < 0)
		goto err;

	/* set cfmt */
	ret = ov2640_write_array(client, selected_cfmt_regs);
	if (ret < 0)
		goto err;

	info->cfmt_code = code;
	info->streaming = 1;

	return 0;

err:
	v4l_err(client, "%s: Error %d", __func__, ret);
	ov2640_reset(client);
	info->win = NULL;

	return ret;
}

static int ov2640_get_fmt(struct v4l2_subdev *sd, struct v4l2_subdev_pad_config *cfg,
			struct v4l2_subdev_format *fmt)
{
	struct ov2640_info *info = to_ov2640_info(sd);
	struct v4l2_mbus_framefmt *mf = &fmt->format;

	if (fmt->pad)
		return -EINVAL;

	if (!info->win) {
		u32 width = SVGA_WIDTH, height = SVGA_HEIGHT;
		info->win = ov2640_select_win(&width, &height);
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

static int ov2640_set_fmt(struct v4l2_subdev *sd,
				struct v4l2_subdev_pad_config *cfg,
				struct v4l2_subdev_format *fmt)
{
	struct v4l2_mbus_framefmt *mf = &fmt->format;
	struct ov2640_info *info = to_ov2640_info(sd);

	if (fmt->pad)
		return -EINVAL;

	info->win = ov2640_select_win(&mf->width, &mf->height);
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
		return ov2640_set_params(info, mf->code);

	if (cfg)
		cfg->try_fmt = *mf;

	return 0;
}

static int ov2640_g_frame_interval(struct v4l2_subdev *sd,
			struct v4l2_subdev_frame_interval *fi)
{
	struct ov2640_info *info = to_ov2640_info(sd);

	mutex_lock(&info->lock);
	fi->interval = *info->interval;
	mutex_unlock(&info->lock);

	return 0;
}

static int ov2640_s_frame_interval(struct v4l2_subdev *sd,
			struct v4l2_subdev_frame_interval *fi)
{
	struct ov2640_info *info = to_ov2640_info(sd);
	int fps = 0, min = 0, i = 0, diff = 0;

	v4l_dbg(1, info->debug, info->client, "Setting %d/%d frame interval\n",
			fi->interval.numerator, fi->interval.denominator);

	mutex_lock(&info->lock);
	fps = fi->interval.denominator / fi->interval.numerator;
	min = fps;

	for (i = 0; i < ARRAY_SIZE(ov2640_interval); i++) {
		diff = abs(ov2640_interval[i].denominator /
				ov2640_interval[i].numerator - fps);
		if (diff < min) {
			info->interval = &ov2640_interval[i];
			info->fps = i;
		}
	}
	mutex_unlock(&info->lock);

	return 0;
}

static int ov2640_s_stream(struct v4l2_subdev *sd, int on)
{
	struct ov2640_info *info = to_ov2640_info(sd);
	int ret = 0;

	if (info->streaming != on) {
		if (on) {
			if (!info->win) {
				u32 width = SVGA_WIDTH, height = SVGA_HEIGHT;
				info->win = ov2640_select_win(&width, &height);
				info->cfmt_code = MEDIA_BUS_FMT_UYVY8_2X8;
			}

			ret = ov2640_set_params(info, info->cfmt_code);
			if (ret < 0)
				return ret;
		} else {
			ret = ov2640_reset(info->client);
			if (ret < 0)
				return ret;
		}

		info->streaming = on;
	}

	return 0;
}

static const struct v4l2_subdev_core_ops ov2640_core_ops = {
	.s_power = ov2640_s_power,
};

static const struct v4l2_subdev_pad_ops ov2640_pad_ops = {
	.enum_mbus_code = ov2640_enum_mbus_code,
	.get_fmt = ov2640_get_fmt,
	.set_fmt = ov2640_set_fmt,
};

static const struct v4l2_subdev_video_ops ov2640_video_ops = {
	.s_stream = ov2640_s_stream,
	.g_frame_interval = ov2640_g_frame_interval,
	.s_frame_interval = ov2640_s_frame_interval,
};

static struct v4l2_subdev_ops ov2640_subdev_ops = {
	.pad = &ov2640_pad_ops,
	.core = &ov2640_core_ops,
	.video = &ov2640_video_ops,
};

static int ov2640_set_contrast(struct ov2640_info *info, int val)
{
	struct regval_list ov2640_contrast_regs[] = {
		{0xff, 0x00},
		{0x7c, 0x00},
		{0x7d, 0x04},
		{0x7c, 0x07},
		{0x7d, 0x20},
		{0x7d, 0x14},
		{0x7d, 0x3e},
		{0x7d, 0x06},
		ENDMARKER,
	};

	ov2640_contrast_regs[5].value = 32 + 4 * val;
	ov2640_contrast_regs[6].value = 32 - 10 * val;

	return ov2640_write_array(info->client, ov2640_contrast_regs);
}

static int ov2640_set_saturation(struct ov2640_info *info, int val)
{
	struct regval_list ov2640_saturation_regs[] = {
		{0xff, 0x00},
		{0x7c, 0x00},
		{0x7d, 0x02},
		{0x7c, 0x03},
		{0x7d, 0x28},
		{0x7d, 0x28},
		ENDMARKER,
	};

	ov2640_saturation_regs[4].value = 72 + 16 * val;
	ov2640_saturation_regs[5].value = 72 + 16 * val;

	return ov2640_write_array(info->client, ov2640_saturation_regs);
}

static int ov2640_set_brightness(struct ov2640_info *info, int val)
{
	struct regval_list ov2640_brightness_regs[] = {
		{0xff, 0x00},
		{0x7c, 0x00},
		{0x7d, 0x04},
		{0x7c, 0x09},
		{0x7d, 0x00},
		{0x7d, 0x00},
		ENDMARKER,
	};

	ov2640_brightness_regs[4].value = val << 4;

	return ov2640_write_array(info->client, ov2640_brightness_regs);
}

static int ov2640_set_exposure(struct ov2640_info *info, int val)
{
	struct regval_list ov2640_exposure_regs[] = {
		{0xff, 0x01},
		{0x24, 0x3e},
		{0x25, 0x38},
		{0x26, 0x81},
		ENDMARKER,
	};

	switch (val) {
		case -2:
			ov2640_exposure_regs[1].value = 0x20;
			ov2640_exposure_regs[2].value = 0x18;
			ov2640_exposure_regs[3].value = 0x60;
			break;
		case -1:
			ov2640_exposure_regs[1].value = 0x34;
			ov2640_exposure_regs[2].value = 0x1c;
			ov2640_exposure_regs[3].value = 0x00;
			break;
		case 1:
			ov2640_exposure_regs[1].value = 0x48;
			ov2640_exposure_regs[2].value = 0x40;
			ov2640_exposure_regs[3].value = 0x81;
			break;
		case 2:
			ov2640_exposure_regs[1].value = 0x58;
			ov2640_exposure_regs[2].value = 0x50;
			ov2640_exposure_regs[3].value = 0x92;
			break;
	}

	return ov2640_write_array(info->client, ov2640_exposure_regs);
}

static int ov2640_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct v4l2_subdev *sd = ctrl_to_sd(ctrl);
	struct ov2640_info *info = to_ov2640_info(sd);
	int ret = -1;

	if (!info->power)
		return ret;

	ret = i2c_smbus_write_byte_data(info->client, BANK_SEL, BANK_SEL_SENS);
	if (ret < 0)
		return ret;

	switch (ctrl->id) {
		case V4L2_CID_CONTRAST:
			info->contrast_val = ctrl->val;
			return ov2640_set_contrast(info, ctrl->val);
		case V4L2_CID_SATURATION:
			info->saturation_val = ctrl->val;
			return ov2640_set_saturation(info, ctrl->val);
		case V4L2_CID_BRIGHTNESS:
			info->brightness_val = ctrl->val;
			return ov2640_set_brightness(info, ctrl->val);
		case V4L2_CID_EXPOSURE:
			info->exposure_val = ctrl->val;
			return ov2640_set_exposure(info, ctrl->val);
	}

	return -1;
}

static const struct v4l2_ctrl_ops ov2640_ctrl_ops = {
	.s_ctrl = ov2640_s_ctrl,
};

static int ov2640_video_probe(struct i2c_client *client, struct ov2640_info *info)
{
	u8 pid, ver;
	u16 id;
	int ret = 0;

	_ov2640_set_power(info, 1);
	/*
	 * check and show product ID and manufacturer ID
	 */
	i2c_smbus_write_byte_data(client, BANK_SEL, BANK_SEL_SENS);
	pid = i2c_smbus_read_byte_data(client, PID);
	id = pid << 8;
	ver = i2c_smbus_read_byte_data(client, VER);
	id |= ver;

	if (id != PID_OV2640) {
		v4l_err(client, "Product ID error %#x\n", id);
		ret = -ENODEV;
		goto probe_done;
	}

	v4l_info(client, "ov2640 ID %#x\n", id);

probe_done:
	_ov2640_set_power(info, 0);
	return ret;
}

static int ov2640_config_gpio(struct ov2640_info *info, struct i2c_client *client)
{
	int ret = 0;

	info->gpio_rst = devm_gpiod_get(&client->dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(info->gpio_rst)) {
		v4l_err(info->client, "unable to get gpio_rst\n");
		info->gpio_rst = NULL;
		return -1;
	}

	ret = gpiod_direction_output(info->gpio_rst, 0);
	if (ret != 0)
		v4l_err(info->client, "unable to set direction gpio_rst \n");

	info->gpio_pdn = devm_gpiod_get(&client->dev, "powerdown", GPIOD_OUT_LOW);
	if (IS_ERR(info->gpio_pdn)) {
		v4l_err(info->client, "unable to get gpio_pdn\n");
		info->gpio_pdn = NULL;
		return -1;
	}

	ret = gpiod_direction_output(info->gpio_pdn, 0);
	if (ret != 0)
		v4l_err(info->client, "unable to set direction gpio_pdn \n");

	gpiod_set_value(info->gpio_rst, 0);
	gpiod_set_value(info->gpio_pdn, 0);

	return ret;
}

static int ov2640_probe(struct i2c_client *client,
			const struct i2c_device_id *did)
{
	struct i2c_adapter *adapter = client->adapter;
	struct ov2640_info *info = NULL;

	v4l_info(client, "%s\n", __FUNCTION__);
	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA))
		return -ENODEV;

	info = devm_kzalloc(&client->dev, sizeof(struct ov2640_info), GFP_KERNEL);
	if (info == NULL)
		return -ENOMEM;

	info->debug  = 1;
	info->client = client;
	mutex_init(&info->lock);
	v4l2_i2c_subdev_init(&info->subdev, client, &ov2640_subdev_ops);

	v4l_info(client, "chip found @ 0x%02x (%s)\n",
				client->addr << 1, client->adapter->name);
	info->clk = v4l2_clk_get(&client->dev, "xvclk");
	if (IS_ERR(info->clk)) {
		v4l_err(client, "ov2640 get clk error");
		return -1;
	}

	ov2640_config_gpio(info, client);
	if (ov2640_video_probe(client, info) < 0) {
		v4l_err(client, "ov2640 video probe error\n");
		goto err_video_probe;
	}

	info->fps = OV2640_15FPS;
	info->contrast_val = 1;
	info->saturation_val = -1;
	info->brightness_val = 0;
	info->exposure_val = 0;

	v4l2_ctrl_handler_init(&info->hdl, 4);
	v4l2_ctrl_new_std(&info->hdl, &ov2640_ctrl_ops,
				V4L2_CID_CONTRAST, -2, 2, 1, 1);
	v4l2_ctrl_new_std(&info->hdl, &ov2640_ctrl_ops,
				V4L2_CID_SATURATION, -2, 2, 1, -1);
	v4l2_ctrl_new_std(&info->hdl, &ov2640_ctrl_ops,
				V4L2_CID_BRIGHTNESS, -2, 2, 1, 0);
	v4l2_ctrl_new_std(&info->hdl, &ov2640_ctrl_ops,
				V4L2_CID_EXPOSURE, -2, 2, 1, 0);

	info->subdev.ctrl_handler = &info->hdl;
	if (info->hdl.error) {
		v4l_err(client, "ov2640 ctrl handler init error\n");
		goto err_video_probe;
	}

	return 0;
err_video_probe:
	kfree(info);
	return -1;
}

static int ov2640_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ov2640_info *info = to_ov2640_info(sd);

	v4l2_ctrl_handler_free(&info->hdl);
	v4l2_device_unregister_subdev(sd);
	v4l_info(client, "%s\n", __FUNCTION__);
	return 0;
}

static int ov2640_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ov2640_info *info = to_ov2640_info(sd);

	if (info->streaming) {
		info->streaming = 0;
		info->need_streaming = true;
	} else {
		info->need_streaming = false;
	}

	if (info->power) {
		ov2640_s_power(sd, 0);
		info->need_power = true;
	} else {
		info->need_power = false;
	}

	return 0;
}

static int ov2640_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ov2640_info *info = to_ov2640_info(sd);
	int ret = 0;

	ret = gpiod_direction_output(info->gpio_rst, 0);
	if (ret != 0)
		v4l_err(info->client, "unable to set direction gpio_rst \n");

	ret = gpiod_direction_output(info->gpio_pdn, 0);
	if (ret != 0)
		v4l_err(info->client, "unable to set direction gpio_rst \n");

	if (info->need_power)
		ov2640_s_power(sd, 1);

	if (info->need_streaming)
		ov2640_s_stream(sd, 1);

	return 0;
}

static const struct i2c_device_id ov2640_id[] = {
	{ "ov2640", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ov2640_id);

static const struct dev_pm_ops ov2640_pm_ops = {
	.suspend = ov2640_suspend,
	.resume = ov2640_resume,
};

static struct i2c_driver ov2640_i2c_driver = {
	.driver = {
		.name = "ov2640",
		.pm   = &ov2640_pm_ops,
	},
	.probe      = ov2640_probe,
	.remove     = ov2640_remove,
	.id_table   = ov2640_id,
};

module_i2c_driver(ov2640_i2c_driver);
MODULE_LICENSE("GPL v2");
