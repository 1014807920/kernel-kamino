/*
 * leds-aw20108.c   aw20108 led module
 *
 * Version: v1.0.0
 *
 * Copyright (c) 2018 AWINIC Technology CO., LTD
 *
 *  Author: Joseph <zhangzetao@awinic.com.cn>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/firmware.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/debugfs.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>
#include <linux/leds.h>
#include <linux/leds-aw20108.h>
#include <linux/leds-aw20108-reg.h>
/******************************************************
 *
 * Marco
 *
 ******************************************************/
#define AW20108_I2C_NAME "aw20108_led"

#define AW20108_VERSION "v1.0.0"

#define AW_I2C_RETRIES 2
#define AW_I2C_RETRY_DELAY 1
#define AW_READ_CHIPID_RETRIES 2
#define AW_READ_CHIPID_RETRY_DELAY 1

struct pinctrl *pinctrl_aw20108;
struct pinctrl_state *aw20108_pins_cfg,*aw20108_rst_output0, *aw20108_rst_output1;
unsigned int aw20108debounce;

const struct of_device_id aw20108_of_match[] = {
	{ .compatible = "awinic,aw20108_led",},
	{},
};
/******************************************************
 *
 * aw20108 led parameter
 *
 ******************************************************/
#define AW20108_CFG_NAME_MAX        64
#if defined(AW20108_BIN_CONFIG)
static char aw20108_cfg_name[][AW20108_CFG_NAME_MAX] = {
    {"aw20108_led_all_on.bin"},
    {"aw20108_led_red_on.bin"},
    {"aw20108_led_green_on.bin"},
    {"aw20108_led_blue_on.bin"},
    {"aw20108_led_breath_forever.bin"},
    {"aw20108_cfg_led_off.bin"},
};
#elif defined(AW20108_ARRAY_CONFIG)
AW20108_CFG aw20108_cfg_array[] = {
    {aw20108_led_all_on,sizeof(aw20108_led_all_on)},
    {aw20108_led_red_on,sizeof(aw20108_led_red_on)},
    {aw20108_led_green_on,sizeof(aw20108_led_green_on)},
    {aw20108_led_blue_on,sizeof(aw20108_led_blue_on)},
    {aw20108_led_breath_forever,sizeof(aw20108_led_breath_forever)},
    {aw20108_cfg_led_off,sizeof(aw20108_cfg_led_off)}
};
#else
	/*Nothing*/
#endif

#define AW20108_IMAX_NAME_MAX       32
static char aw20108_imax_name[][AW20108_IMAX_NAME_MAX] = {
    {"AW20108_IMAX_3P3mA"},
    {"AW20108_IMAX_6P7mA"},
    {"AW20108_IMAX_10mA"},
    {"AW20108_IMAX_13P3mA"},
    {"AW20108_IMAX_20mA"},
    {"AW20108_IMAX_26P7mA"},
    {"AW20108_IMAX_30mA"},
    {"AW20108_IMAX_40mA"},
    {"AW20108_IMAX_53P6mA"},
    {"AW20108_IMAX_60mA"},
    {"AW20108_IMAX_80mA"},
    {"AW20108_IMAX_120mA"},
    {"AW20108_IMAX_160mA"},
};
static char aw20108_imax_code[] = {
    AW20108_IMAX_3P3mA,
    AW20108_IMAX_6P7mA,
    AW20108_IMAX_10mA,
    AW20108_IMAX_13P3mA,
    AW20108_IMAX_20mA,
    AW20108_IMAX_26P7mA,
    AW20108_IMAX_30mA,
    AW20108_IMAX_40mA,
    AW20108_IMAX_53P6mA,
    AW20108_IMAX_60mA,
    AW20108_IMAX_80mA,
    AW20108_IMAX_120mA,
    AW20108_IMAX_160mA,
};
/******************************************************
 *
 * aw20108 i2c write/read
 *
 ******************************************************/
static int aw20108_i2c_write(struct aw20108 *aw20108,
         unsigned char reg_addr, unsigned char reg_data)
{
    int ret = -1;
    unsigned char cnt = 0;
    while(cnt < AW_I2C_RETRIES) {
        ret = i2c_smbus_write_byte_data(aw20108->i2c, reg_addr, reg_data);
        if(ret < 0) {
            pr_err("%s: i2c_write cnt=%d error=%d\n", __func__, cnt, ret);
        } else {
            break;
        }
        cnt ++;
        msleep(AW_I2C_RETRY_DELAY);
    }

    return ret;
}

static int aw20108_i2c_read(struct aw20108 *aw20108,
        unsigned char reg_addr, unsigned char *reg_data)
{
    int ret = -1;
    unsigned char cnt = 0;

    while(cnt < AW_I2C_RETRIES) {
        ret = i2c_smbus_read_byte_data(aw20108->i2c, reg_addr);
        if(ret < 0) {
            pr_err("%s: i2c_read cnt=%d error=%d\n", __func__, cnt, ret);
        } else {
            *reg_data = ret;
            break;
        }
        cnt ++;
        msleep(AW_I2C_RETRY_DELAY);
    }

    return ret;
}

/*
static int aw20108_i2c_write_bits(struct aw20108 *aw20108,
         unsigned char reg_addr, unsigned char mask, unsigned char reg_data)
{
    unsigned char reg_val;

    aw20108_i2c_read(aw20108, reg_addr, &reg_val);
    reg_val &= mask;
    reg_val |= reg_data;
    aw20108_i2c_write(aw20108, reg_addr, reg_val);

    return 0;
}

*/

/*****************************************************
 *
 * aw20108 led cfg
 *
 *****************************************************/
static int aw20108_reg_page_cfg(struct aw20108 *aw20108, unsigned char page)
{
    aw20108_i2c_write(aw20108, REG_PAGE, page);
    return 0;
}

static int aw20108_sw_reset(struct aw20108 *aw20108)
{
    aw20108_i2c_write(aw20108, REG_SWRST, AW20108_RSTR);
    msleep(2);
    return 0;
}

static int aw20108_chip_enable(struct aw20108 *aw20108, bool flag)
{
    if(flag) {
        aw20108_i2c_write(aw20108, REG_WORK_MODE,
                BIT_GCR_CHIPEN_ENABLE);
    } else {
        aw20108_i2c_write(aw20108, REG_WORK_MODE,
                BIT_GCR_CHIPEN_DISABLE);
    }
    return 0;
}

static int aw20108_imax_cfg(struct aw20108 *aw20108, unsigned char imax)
{
    if(imax>0xF) {
        imax = 0xF;
    }
    aw20108_reg_page_cfg(aw20108, AW20108_REG_PAGE0);
    aw20108_i2c_write(aw20108, REG_GCCR & BIT_IMAX_MASK , imax);

    return 0;
}

static int aw20108_dbgdim_cfg(struct aw20108 *aw20108, unsigned int data)
{
    int i;
    aw20108_i2c_write(aw20108, 0xF0, 0xC1);
    for(i=0; i<AW20108_REG_NUM_PAG1; i++) {
        aw20108->rgbcolor = data;
        aw20108_i2c_write(aw20108, i, aw20108->rgbcolor);
    }
    return 0;
}

static int aw20108_dbgfdad_cfg(struct aw20108 *aw20108, unsigned int data)
{
    int i;
    aw20108_i2c_write(aw20108, 0xF0, 0xC2);
    for(i=0; i<AW20108_REG_NUM_PAG2; i++) {
        aw20108->rgbcolor = data;
        aw20108_i2c_write(aw20108, i, aw20108->rgbcolor);
    }

    return 0;
}

static int aw20108_led_display(struct aw20108 *aw20108)
{
    aw20108_i2c_write(aw20108, 0xF0, 0xC0);
    aw20108_i2c_write(aw20108, 0x02, 0x01);
    aw20108_i2c_write(aw20108, 0x03, 0x18);
    aw20108_i2c_write(aw20108, 0x80, 0x08);
    aw20108_i2c_write(aw20108, 0x01,0X00);
    aw20108_dbgdim_cfg(aw20108, AW20108_DBGCTR_DIM);
    aw20108_dbgfdad_cfg(aw20108, AW20108_DBGCTR_FDAD);

    return 0;
}


static void aw20108_brightness_work(struct work_struct *work)
{
    struct aw20108 *aw20108 = container_of(work, struct aw20108,
          brightness_work);

    pr_info("%s: enter\n", __func__);

    aw20108_chip_enable(aw20108, false);
    if(aw20108->cdev.brightness) {
        aw20108_chip_enable(aw20108, true);
        aw20108_imax_cfg(aw20108, (unsigned char)aw20108->imax);
        aw20108_led_display(aw20108);
    }
}

static void aw20108_set_brightness(struct led_classdev *cdev,
           enum led_brightness brightness)
{
    struct aw20108 *aw20108 = container_of(cdev, struct aw20108, cdev);

    aw20108->cdev.brightness = brightness;

    schedule_work(&aw20108->brightness_work);
}
/*****************************************************
 *
 * firmware/cfg update
 *
 *****************************************************/
#if defined(AW20108_ARRAY_CONFIG)
static void aw20108_update_cfg_array(struct aw20108 *aw20108, unsigned char *p_cfg_data, unsigned int cfg_size)
{
    unsigned int i = 0;

    for(i=0; i<cfg_size; i+=2) {
        aw20108_i2c_write(aw20108, p_cfg_data[i], p_cfg_data[i+1]);
    }
}

static int  aw20108_cfg_update_array(struct aw20108 *aw20108)
{
	int i;
	i=0;
	pr_info("--%s---%d--aw20108->effect=%d\n",__func__,__LINE__,aw20108->effect);
	pr_info("--%pf ---%d--\n",aw20108_cfg_array[aw20108->effect].p,aw20108_cfg_array[aw20108->effect].count);
	
    aw20108_update_cfg_array(aw20108, (aw20108_cfg_array[aw20108->effect].p), aw20108_cfg_array[aw20108->effect].count );
    return 0;
}
#endif
#if defined(AW20108_BIN_CONFIG)
static void aw20108_cfg_loaded(const struct firmware *cont, void *context)
{
    struct aw20108 *aw20108 = context;
    int i = 0;

    pr_info("%s: enter\n", __func__);

    if (!cont) {
        pr_info("%s: failed to read %s\n", __func__, aw20108_cfg_name[aw20108->effect]);
        release_firmware(cont);
        return;
    }
	mutex_lock(&aw20108->cfg_lock);
    pr_info("%s: loaded %s - size: %zu\n", __func__, aw20108_cfg_name[aw20108->effect],
                    cont ? cont->size : 0);
    for(i=0; i<cont->size; i+=2) {
        aw20108_i2c_write(aw20108, *(cont->data+i), *(cont->data+i+1));
        pr_debug("%s: addr:0x%02x, data:0x%02x\n", __func__, *(cont->data+i), *(cont->data+i+1));
    }

    release_firmware(cont);
	mutex_unlock(&aw20108->cfg_lock);
    pr_info("%s: cfg update complete\n", __func__);
    
}


static int aw20108_cfg_update(struct aw20108 *aw20108)
{	
	int ret;
    pr_info("%s: enter\n", __func__);
    ret = 0;
	
    if(aw20108->effect < (sizeof(aw20108_cfg_name)/AW20108_CFG_NAME_MAX)) {
        pr_info("%s: cfg name=%s\n", __func__, aw20108_cfg_name[aw20108->effect]);
    } else {
        pr_err("%s: effect 0x%02x over s value \n", __func__, aw20108->effect);
        return -1;
    }
    
    return request_firmware_nowait(THIS_MODULE, FW_ACTION_HOTPLUG,
                aw20108_cfg_name[aw20108->effect], aw20108->dev, GFP_KERNEL,
                aw20108, aw20108_cfg_loaded);
}
#endif


static int aw20108_hw_reset(struct aw20108 *aw20108)
{
    pr_info("%s: enter\n", __func__);

    if (aw20108 && gpio_is_valid(aw20108->reset_gpio)) {
        gpio_set_value_cansleep(aw20108->reset_gpio, 0);
        msleep(1);
        gpio_set_value_cansleep(aw20108->reset_gpio, 1);
        msleep(1);
    } else {
        dev_err(aw20108->dev, "%s:  failed\n", __func__);
    }

    pr_info("%s: enter out \n", __func__);
    return 0;
}

static int aw20108_hw_off(struct aw20108 *aw20108)
{
    pr_info("%s: enter\n", __func__);
    if (aw20108 && gpio_is_valid(aw20108->reset_gpio)) {
        gpio_set_value_cansleep(aw20108->reset_gpio, 0);
        msleep(1);
    } else {
        dev_err(aw20108->dev, "%s:  failed\n", __func__);
    }

    return 0;
}


/******************************************************
 *
 * irq 
 *
 ******************************************************/

static irqreturn_t aw20108_irq(int irq, void *data)
{
    struct aw20108 *aw20108 = data;
    unsigned char reg_val;

    pr_info("%s: enter\n", __func__);

	aw20108_i2c_read(aw20108, REG_ISRFLT, &reg_val);
	pr_info("%s: reg INTST=0x%x\n", __func__, reg_val);
    pr_info("%s exit\n", __func__);

    return IRQ_HANDLED;
}

/*****************************************************
 *
 * device tree
 *
 *****************************************************/
static int aw20108_parse_dt(struct device *dev, struct aw20108 *aw20108,
        struct device_node *np)
{

    aw20108->reset_gpio = of_get_named_gpio(np, "reset-gpio", 0);
    if (aw20108->reset_gpio < 0) {
        dev_err(dev, "%s: no reset gpio provided, will not HW reset device\n", __func__);
        return -1;
    } else {
        dev_info(dev, "%s: reset gpio provided ok\n", __func__);
    }
    aw20108->irq_gpio = of_get_named_gpio(np, "irq-gpio", 0);
    if (aw20108->irq_gpio < 0) {
        dev_err(dev, "%s: no irq gpio provided, will not suppport intterupt\n", __func__);
        return -1;
    } else {
        dev_info(dev, "%s: irq gpio provided ok\n", __func__);
    }

    return 0;
}

/*****************************************************
 *
 * check chip id
 *
 *****************************************************/
static int aw20108_read_chipid(struct aw20108 *aw20108)
{
    int ret = -1;
    unsigned char cnt = 0;
    unsigned char reg_val = 0;
  
    aw20108_reg_page_cfg(aw20108, AW20108_REG_PAGE0);
    aw20108_sw_reset(aw20108);
  
    while(cnt < AW_READ_CHIPID_RETRIES) {
        ret = aw20108_i2c_read(aw20108, REG_CHIPID, &reg_val);
        if(reg_val== AW20108_CHIPID){
            pr_info("This Chip is  AW20108    REG_ID: 0x%x\n",reg_val);
			return 0;
        }else if (ret < 0) {
            dev_err(aw20108->dev,"%s: failed to AW20108_REG_ID: %d\n",__func__, ret);
            return -EIO;
        }else{
        	cnt ++;
        	pr_info("This Chip    read register   REG_ID: 0x%x\n",reg_val);
        }
        msleep(AW_READ_CHIPID_RETRY_DELAY);
    }
    return -EINVAL;
}


/******************************************************
 *
 * sys group attribute: reg
 *
 ******************************************************/
static ssize_t aw20108_reg_store(struct device *dev, struct device_attribute *attr,
                const char *buf, size_t count)
{
    struct led_classdev *led_cdev = dev_get_drvdata(dev);
    struct aw20108 *aw20108 = container_of(led_cdev, struct aw20108, cdev);

    unsigned int databuf[2] = {0, 0};

    if(2 == sscanf(buf, "%x %x", &databuf[0], &databuf[1])) {
        aw20108_i2c_write(aw20108, databuf[0], databuf[1]);
    }
    return count;
}

static ssize_t aw20108_reg_show(struct device *dev, struct device_attribute *attr,
                char *buf)
{
    struct led_classdev *led_cdev = dev_get_drvdata(dev);
    struct aw20108 *aw20108 = container_of(led_cdev, struct aw20108, cdev);
    ssize_t len = 0;
    unsigned int i = 0;
    unsigned char reg_val = 0;
    unsigned char reg_page = 0;
    aw20108_i2c_read(aw20108, REG_PAGE, &reg_page);
    for(i = 0; i < AW20108_REG_MAX; i ++) {
        if(!reg_page) {
            if(!(aw20108_reg_access[i]&REG_RD_ACCESS))
               continue;
        }
        aw20108_i2c_read(aw20108, i, &reg_val);
        len += snprintf(buf+len, PAGE_SIZE-len, "reg:0x%02x=0x%02x \n", i, reg_val);
    }
    return len;
}

static ssize_t aw20108_hwen_store(struct device *dev, struct device_attribute *attr,
                const char *buf, size_t count)
{
    struct led_classdev *led_cdev = dev_get_drvdata(dev);
    struct aw20108 *aw20108 = container_of(led_cdev, struct aw20108, cdev);

    unsigned int databuf[1] = {0};

    if(1 == sscanf(buf, "%x", &databuf[0])) {
        if(1 == databuf[0]) {
            aw20108_hw_reset(aw20108);
        } else {
            aw20108_hw_off(aw20108);
        }
    }

    return count;
}

static ssize_t aw20108_hwen_show(struct device *dev, struct device_attribute *attr,
                char *buf)
{
    struct led_classdev *led_cdev = dev_get_drvdata(dev);
    struct aw20108 *aw20108 = container_of(led_cdev, struct aw20108, cdev);
    ssize_t len = 0;
    len += snprintf(buf+len, PAGE_SIZE-len, "hwen=%d\n",
            gpio_get_value(aw20108->reset_gpio));

    return len;
}

static ssize_t aw20108_effect_show(struct device* dev,struct device_attribute *attr, char* buf)
{
    ssize_t len = 0;
    unsigned int i;
    struct led_classdev *led_cdev = dev_get_drvdata(dev);
    struct aw20108 *aw20108 = container_of(led_cdev, struct aw20108, cdev);
#if defined(AW20108_BIN_CONFIG)
    for(i=0; i<sizeof(aw20108_cfg_name)/AW20108_CFG_NAME_MAX; i++) {
        len += snprintf(buf+len, PAGE_SIZE-len, "cfg[%x] = %s\n", i, aw20108_cfg_name[i]);
    }
    len += snprintf(buf+len, PAGE_SIZE-len, "current cfg = %s\n", aw20108_cfg_name[aw20108->effect]);
#elif defined(AW20108_ARRAY_CONFIG)
    for(i=0; i<sizeof(aw20108_cfg_array)/sizeof(struct aw20108_cfg); i++) {
        len += snprintf(buf+len, PAGE_SIZE-len, "cfg[%x] = %pf\n", i, aw20108_cfg_array[i].p);
    }
    len += snprintf(buf+len, PAGE_SIZE-len, "current cfg = %pf\n", aw20108_cfg_array[aw20108->effect].p);
#else
	/*Nothing*/
#endif
    return len;
}

static ssize_t aw20108_effect_store(struct device* dev, struct device_attribute *attr,
                const char* buf, size_t len)
{
    unsigned int databuf[1];
    struct led_classdev *led_cdev = dev_get_drvdata(dev);
    struct aw20108 *aw20108 = container_of(led_cdev, struct aw20108, cdev);

    sscanf(buf,"%x",&databuf[0]);
    aw20108->effect = databuf[0];
#if defined(AW20108_BIN_CONFIG)
	aw20108_cfg_update(aw20108);
#elif defined(AW20108_ARRAY_CONFIG)
	aw20108_cfg_update_array(aw20108);
#else
	/*Nothing*/
#endif

    return len;
}



static ssize_t aw20108_imax_store(struct device* dev, struct device_attribute *attr,
                const char* buf, size_t len)
{
    unsigned int databuf[1];
    struct led_classdev *led_cdev = dev_get_drvdata(dev);
    struct aw20108 *aw20108 = container_of(led_cdev, struct aw20108, cdev);

    sscanf(buf,"%x",&databuf[0]);
    aw20108->imax = databuf[0];
    if(aw20108->imax > (sizeof(aw20108_imax_code)/sizeof(aw20108_imax_code[0])))
        aw20108->imax = sizeof(aw20108_imax_code)/sizeof(aw20108_imax_code[0])-1;

    aw20108_imax_cfg(aw20108, aw20108_imax_code[aw20108->imax]);

    return len;
}

static ssize_t aw20108_imax_show(struct device* dev,struct device_attribute *attr, char* buf)
{
    ssize_t len = 0;
    unsigned int i;
    struct led_classdev *led_cdev = dev_get_drvdata(dev);
    struct aw20108 *aw20108 = container_of(led_cdev, struct aw20108, cdev);

    for(i=0; i<sizeof(aw20108_imax_name)/AW20108_IMAX_NAME_MAX; i++) {
        len += snprintf(buf+len, PAGE_SIZE-len, "imax[%x] = %s\n", i, aw20108_imax_name[i]);
    }
    len += snprintf(buf+len, PAGE_SIZE-len, "current id = 0x%02x, imax = %s\n",
        aw20108->imax, aw20108_imax_name[aw20108->imax]);

    return len;
}

static ssize_t aw20108_rgbcolor_store(struct device* dev, struct device_attribute *attr,
                const char* buf, size_t len)
{
	unsigned int databuf[2] = {0, 0};
    struct led_classdev *led_cdev = dev_get_drvdata(dev);
    struct aw20108 *aw20108 = container_of(led_cdev, struct aw20108, cdev);

    if(2 == sscanf(buf, "%x %x", &databuf[0], &databuf[1])) {
        aw20108_i2c_write(aw20108, 0xF0, 0xC1);

        aw20108->rgbcolor = (databuf[1] & 0x00ff0000) >> 16;
        aw20108->rgbcolor = (aw20108->rgbcolor *64) / 256;
        aw20108_i2c_write(aw20108, databuf[0], aw20108->rgbcolor);

        aw20108->rgbcolor = (databuf[1] & 0x0000ff00)  >> 8;
        aw20108->rgbcolor = (aw20108->rgbcolor *64) / 256;
        aw20108_i2c_write(aw20108, databuf[0]+1, aw20108->rgbcolor);

        aw20108->rgbcolor = (databuf[1] & 0x000000ff)  ;
        aw20108->rgbcolor = (aw20108->rgbcolor *64) / 256;
        aw20108_i2c_write(aw20108, databuf[0]+2, aw20108->rgbcolor);
    }
    return len;
}



static ssize_t aw20108_rgbbrightness_store(struct device* dev, struct device_attribute *attr,
                const char* buf, size_t len)
{
	unsigned int databuf[2] = {0, 0};
    struct led_classdev *led_cdev = dev_get_drvdata(dev);
    struct aw20108 *aw20108 = container_of(led_cdev, struct aw20108, cdev);

    if(2 == sscanf(buf, "%x %x", &databuf[0], &databuf[1])) {
        aw20108_i2c_write(aw20108, 0xF0, 0xC2);
        aw20108->rgbbrightness = (databuf[1] & 0x00ff0000) >> 16;
        aw20108_i2c_write(aw20108, databuf[0], aw20108->rgbbrightness);

        aw20108->rgbbrightness = (databuf[1] & 0x0000ff00)  >> 8;
        aw20108_i2c_write(aw20108, databuf[0]+1, aw20108->rgbbrightness);

        aw20108->rgbbrightness = (databuf[1] & 0x000000ff)  ;
        aw20108_i2c_write(aw20108, databuf[0]+2, aw20108->rgbbrightness);
    }
    return len;
}

static ssize_t aw20108_allrgbcolor_store(struct device* dev, struct device_attribute *attr,
                const char* buf, size_t len)
{
    unsigned int databuf[1];
    unsigned int i;
    struct led_classdev *led_cdev = dev_get_drvdata(dev);
    struct aw20108 *aw20108 = container_of(led_cdev, struct aw20108, cdev);

    sscanf(buf,"%x",&databuf[0]);
    aw20108->rgbcolor = databuf[0];
    /*Set pag 1 DIM0-DIM35*/
    aw20108_i2c_write(aw20108, 0xF0, 0xC1);
    for(i=0; i<AW20108_REG_NUM_PAG1; i+=3) {
        aw20108->rgbcolor = (databuf[0] & 0x00ff0000) >> 16;
        aw20108->rgbcolor = (aw20108->rgbcolor *64) / 256;
        aw20108_i2c_write(aw20108, i, aw20108->rgbcolor);

        aw20108->rgbcolor = (databuf[0] & 0x0000ff00)  >> 8;
        aw20108->rgbcolor = (aw20108->rgbcolor *64) / 256;
        aw20108_i2c_write(aw20108, i+1, aw20108->rgbcolor);

        aw20108->rgbcolor = (databuf[0] & 0x000000ff)  ;
        aw20108->rgbcolor = (aw20108->rgbcolor *64) / 256;
        aw20108_i2c_write(aw20108, i+2, aw20108->rgbcolor);
        pr_debug("%s: addr:0x%02x, data:0x%02x\n", __func__, i, aw20108->rgbcolor);
	}
    return len;
}

static ssize_t aw20108_allrgbbrightness_store(struct device* dev, struct device_attribute *attr,
                const char* buf, size_t len)
{
    unsigned int databuf[2];
    unsigned int i;
    struct led_classdev *led_cdev = dev_get_drvdata(dev);
    struct aw20108 *aw20108 = container_of(led_cdev, struct aw20108, cdev);

    sscanf(buf,"%x",&databuf[0]);
    /*Set pag 2 PAD0-PAD35*/
    aw20108_i2c_write(aw20108, 0xF0, 0xC2);
    for(i=0; i<AW20108_REG_NUM_PAG2; i+=3) {
        aw20108->rgbbrightness = (databuf[0] & 0x00ff0000) >> 16;
        aw20108_i2c_write(aw20108, i, aw20108->rgbbrightness);

        aw20108->rgbbrightness = (databuf[0] & 0x0000ff00)  >> 8;
        aw20108_i2c_write(aw20108, i+1, aw20108->rgbbrightness);

        aw20108->rgbbrightness = (databuf[0] & 0x000000ff)  ;
        aw20108_i2c_write(aw20108, i+2, aw20108->rgbbrightness);
        pr_debug("%s: addr:0x%02x, data:0x%02x\n", __func__, i, aw20108->rgbbrightness);
	}
    return len;
}

static DEVICE_ATTR(reg, S_IWUSR | S_IRUGO, aw20108_reg_show, aw20108_reg_store);
static DEVICE_ATTR(hwen, S_IWUSR | S_IRUGO, aw20108_hwen_show, aw20108_hwen_store);
static DEVICE_ATTR(effect, S_IWUSR | S_IRUGO, aw20108_effect_show, aw20108_effect_store);
static DEVICE_ATTR(imax, S_IWUSR | S_IRUGO, aw20108_imax_show, aw20108_imax_store);
static DEVICE_ATTR(rgbcolor, S_IWUSR | S_IRUGO, NULL, aw20108_rgbcolor_store);
static DEVICE_ATTR(rgbbrightness, S_IWUSR | S_IRUGO, NULL, aw20108_rgbbrightness_store);
static DEVICE_ATTR(allrgbcolor, S_IWUSR | S_IRUGO, NULL, aw20108_allrgbcolor_store);
static DEVICE_ATTR(allrgbbrightness, S_IWUSR | S_IRUGO, NULL, aw20108_allrgbbrightness_store);

static struct attribute *aw20108_attributes[] = {
    &dev_attr_reg.attr,
    &dev_attr_hwen.attr,
    &dev_attr_effect.attr,
    &dev_attr_imax.attr,
    &dev_attr_rgbcolor.attr,
    &dev_attr_allrgbcolor.attr,
    &dev_attr_rgbbrightness.attr,
    &dev_attr_allrgbbrightness.attr,
    NULL
};

static struct attribute_group aw20108_attribute_group = {
    .attrs = aw20108_attributes
};


/******************************************************
 *
 * led class dev
 *
 ******************************************************/

static int aw20108_parse_led_cdev(struct aw20108 *aw20108,
        struct device_node *np)
{
    struct device_node *temp;
    int ret = -1;
    pr_info("%s: enter\n", __func__);

    for_each_child_of_node(np, temp) {
        ret = of_property_read_string(temp, "aw20108,name",
            &aw20108->cdev.name);
        if (ret < 0) {
            dev_err(aw20108->dev,
                "Failure reading led name, ret = %d\n", ret);
            goto free_pdata;
        }
        ret = of_property_read_u32(temp, "aw20108,imax",
            &aw20108->imax);
        if (ret < 0) {
            dev_err(aw20108->dev,
                "Failure reading imax, ret = %d\n", ret);
            goto free_pdata;
        }
        ret = of_property_read_u32(temp, "aw20108,brightness",
            &aw20108->cdev.brightness);
        if (ret < 0) {
            dev_err(aw20108->dev,
                "Failure reading brightness, ret = %d\n", ret);
            goto free_pdata;
        }
        ret = of_property_read_u32(temp, "aw20108,max_brightness",
            &aw20108->cdev.max_brightness);
        if (ret < 0) {
            dev_err(aw20108->dev,
                "Failure reading max brightness, ret = %d\n", ret);
            goto free_pdata;
        }
    }

    INIT_WORK(&aw20108->brightness_work, aw20108_brightness_work);
    aw20108->cdev.brightness_set = aw20108_set_brightness;
    ret = led_classdev_register(aw20108->dev, &aw20108->cdev);
    if (ret) {
        dev_err(aw20108->dev,"unable to register led ret=%d\n", ret);
        goto free_pdata;
    }
    ret = sysfs_create_group(&aw20108->cdev.dev->kobj,&aw20108_attribute_group);
    if (ret) {
        dev_err(aw20108->dev, "led sysfs ret: %d\n", ret);
        goto free_class;
    }
    return 0;

free_class:
    led_classdev_unregister(&aw20108->cdev);
free_pdata:
    return ret;
}

/******************************************************
 *
 * i2c driver
 *
 ******************************************************/
static int aw20108_i2c_probe(struct i2c_client *i2c, const struct i2c_device_id *id)
{
    struct aw20108 *aw20108;
   struct device_node *np = i2c->dev.of_node;
    int ret;
    int irq_flags;
	
    pr_info("%s: enter\n", __func__);

    if (!i2c_check_functionality(i2c->adapter, I2C_FUNC_I2C)) {
        dev_err(&i2c->dev, "check_functionality failed\n");
        return -EIO;
    }

    aw20108 = devm_kzalloc(&i2c->dev, sizeof(struct aw20108), GFP_KERNEL);
    if (aw20108 == NULL)
        return -ENOMEM;

    aw20108->dev = &i2c->dev;
    aw20108->i2c = i2c;

    i2c_set_clientdata(i2c, aw20108);

    mutex_init(&aw20108->cfg_lock);

    /* aw20108 rst & int */
    if (np) {
        ret = aw20108_parse_dt(&i2c->dev, aw20108, np);
        if (ret) {
            dev_err(&i2c->dev, "%s: failed to parse device tree node\n", __func__);
            goto err_parse_dt;
        }
    } else {
        aw20108->reset_gpio = -1;
        aw20108->irq_gpio = -1;
    }

    if (gpio_is_valid(aw20108->reset_gpio)) {
        ret = devm_gpio_request_one(&i2c->dev, aw20108->reset_gpio,
            GPIOF_OUT_INIT_LOW, "aw20108_rst");
        if (ret){
            dev_err(&i2c->dev, "%s: rst request failed\n", __func__);
            goto err_gpio_request;
        }
    }

    if (gpio_is_valid(aw20108->irq_gpio)) {
        ret = devm_gpio_request_one(&i2c->dev, aw20108->irq_gpio,
            GPIOF_DIR_IN, "aw20108_int");
        if (ret){
            dev_err(&i2c->dev, "%s: int request failed\n", __func__);
            goto err_gpio_request;
        }
    }

    /* hardware reset */
    aw20108_hw_reset(aw20108);

    /* aw20108 chip id */
    ret = aw20108_read_chipid(aw20108);
    if (ret < 0) {
        dev_err(&i2c->dev, "%s: aw20108_read_chipid failed ret=%d\n", __func__, ret);
        goto err_id;
    }

    /* aw22xxx irq */
    if (gpio_is_valid(aw20108->irq_gpio) &&
        !(aw20108->flags & AW20108_FLAG_SKIP_INTERRUPTS)) {
        /* register irq handler */
        irq_flags = IRQF_TRIGGER_FALLING | IRQF_ONESHOT;
        ret = devm_request_threaded_irq(&i2c->dev,
                    gpio_to_irq(aw20108->irq_gpio),
                    NULL, aw20108_irq, irq_flags,
                    "aw20108", aw20108);
        if (ret != 0) {
            dev_err(&i2c->dev, "%s: failed to request IRQ %d: %d\n",
                    __func__, gpio_to_irq(aw20108->irq_gpio), ret);
            goto err_irq;
        }
    } else {
        dev_info(&i2c->dev, "%s skipping IRQ registration\n", __func__);
        /* disable feature support if gpio was invalid */
        aw20108->flags |= AW20108_FLAG_SKIP_INTERRUPTS;
    }

    dev_set_drvdata(&i2c->dev, aw20108);

    aw20108_parse_led_cdev(aw20108, np);
    if (ret < 0) {
        dev_err(&i2c->dev, "%s error creating led class dev\n", __func__);
        goto err_sysfs;
    }
    aw20108_i2c_write(aw20108,0xF0,0xC0);
    aw20108_i2c_write(aw20108,0x02,0x01);
    aw20108_i2c_write(aw20108,0x03,0x18);
    aw20108_i2c_write(aw20108,0x80,0x08);
    aw20108_i2c_write(aw20108,0x01,0x00);
   
    pr_info("%s probe completed successfully!\n", __func__);
    return 0;

err_sysfs:
    devm_free_irq(&i2c->dev, gpio_to_irq(aw20108->irq_gpio), aw20108);
err_irq:
err_id:
    devm_gpio_free(&i2c->dev, aw20108->reset_gpio);
    devm_gpio_free(&i2c->dev, aw20108->irq_gpio);
err_gpio_request:
err_parse_dt:
    devm_kfree(&i2c->dev, aw20108);
    aw20108 = NULL;
    return ret;
}

static int aw20108_i2c_remove(struct i2c_client *i2c)
{
    struct aw20108 *aw20108 = i2c_get_clientdata(i2c);

    pr_info("%s: enter\n", __func__);
    sysfs_remove_group(&aw20108->cdev.dev->kobj,
            &aw20108_attribute_group);
    led_classdev_unregister(&aw20108->cdev);

    devm_free_irq(&i2c->dev, gpio_to_irq(aw20108->irq_gpio), aw20108);

    if (gpio_is_valid(aw20108->reset_gpio))
        devm_gpio_free(&i2c->dev, aw20108->reset_gpio);
    if (gpio_is_valid(aw20108->irq_gpio))
        devm_gpio_free(&i2c->dev, aw20108->irq_gpio);

    devm_kfree(&i2c->dev, aw20108);
    aw20108 = NULL;

    return 0;
}

static const struct i2c_device_id aw20108_i2c_id[] = {
    { AW20108_I2C_NAME, 0 },
    { }
};
MODULE_DEVICE_TABLE(i2c, aw20108_i2c_id);

static struct of_device_id aw20108_dt_match[] = {
    { .compatible = "awinic,aw20108_led" },
    { },
};

static struct i2c_driver aw20108_i2c_driver = {
    .driver = {
        .name = AW20108_I2C_NAME,
        .owner = THIS_MODULE,
        .of_match_table = of_match_ptr(aw20108_dt_match),
    },
    .probe = aw20108_i2c_probe,
    .remove = aw20108_i2c_remove,
    .id_table = aw20108_i2c_id,
};

static int __init aw20108_i2c_init(void)
{
    int ret = 0;
    pr_info("aw20108 driver version %s\n", AW20108_VERSION);

    ret = i2c_add_driver(&aw20108_i2c_driver);
    if(ret){
        pr_err("fail to add aw20108 device into i2c\n");
        return ret;
    }
    return 0;
}
module_init(aw20108_i2c_init);


static void __exit aw20108_i2c_exit(void)
{
    i2c_del_driver(&aw20108_i2c_driver);
}
module_exit(aw20108_i2c_exit);


MODULE_DESCRIPTION("AW20108 LED Driver");
MODULE_LICENSE("GPL v2");
