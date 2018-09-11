/*
 * leds-aw9523.c   aw9523 led module
 *
 * Version: 1.0.0
 *
 * Copyright (c) 2017 AWINIC Technology CO., LTD
 *
 *  Author: Nick Li <liweilei@awinic.com.cn>
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
//#include <linux/leds-aw9523.h>
/******************************************************
 *
 * Marco
 *
 ******************************************************/
#define AW9523_I2C_NAME "aw9523_led"

#define AW9523_VERSION "v1.0.0"

#define MAX_I2C_BUFFER_SIZE 65536

#define AW9523_ID 0x23

struct aw9523 {
    struct i2c_client *i2c;
    struct device *dev;
    struct led_classdev cdev;
    struct work_struct brightness_work;
    struct work_struct pixel_work;

    int reset_gpio;

    unsigned char chipid;

    int imax;
};

#define AW_I2C_RETRIES 5
#define AW_I2C_RETRY_DELAY 5
#define AW_READ_CHIPID_RETRIES 5
#define AW_READ_CHIPID_RETRY_DELAY 5

#define REG_INPUT_P0        0x00
#define REG_INPUT_P1        0x01
#define REG_OUTPUT_P0       0x02
#define REG_OUTPUT_P1       0x03
#define REG_CONFIG_P0       0x04
#define REG_CONFIG_P1       0x05
#define REG_INT_P0          0x06
#define REG_INT_P1          0x07
#define REG_ID              0x10
#define REG_CTRL            0x11
#define REG_WORK_MODE_P0    0x12
#define REG_WORK_MODE_P1    0x13
#define REG_DIM00           0x20
#define REG_DIM01           0x21
#define REG_DIM02           0x22
#define REG_DIM03           0x23
#define REG_DIM04           0x24
#define REG_DIM05           0x25
#define REG_DIM06           0x26
#define REG_DIM07           0x27
#define REG_DIM08           0x28
#define REG_DIM09           0x29
#define REG_DIM10           0x2a
#define REG_DIM11           0x2b
#define REG_DIM12           0x2c
#define REG_DIM13           0x2d
#define REG_DIM14           0x2e
#define REG_DIM15           0x2f
#define REG_SWRST           0x7F

const u8 pinnum_to_reg[15][2] = {
    {0, REG_DIM00},//p1_0
    {1, REG_DIM01},//p1_1
    {2, REG_DIM02},//p1_2
    {3, REG_DIM03},//p1_3
    {4, REG_DIM12},//p1_4
    {5, REG_DIM13},//p1_5
    {6, REG_DIM14},//p1_6
    {7, REG_DIM15},//p1_7
    {8, REG_DIM04},//p0_0
    {9, REG_DIM05},//p0_1
    {10,REG_DIM06},//p0_2
    {11,REG_DIM07},//p0_3
    {12,REG_DIM08},//p0_4
    {13,REG_DIM09},//p0_5
    {14,REG_DIM10}//p0_6
};

/* aw9523 register read/write access*/
#define REG_NONE_ACCESS                 0
#define REG_RD_ACCESS                   1 << 0
#define REG_WR_ACCESS                   1 << 1
#define AW9523_REG_MAX                  0xFF

const unsigned char aw9523_reg_access[AW9523_REG_MAX] = {
  [REG_INPUT_P0    ] = REG_RD_ACCESS,
  [REG_INPUT_P1    ] = REG_RD_ACCESS,
  [REG_OUTPUT_P0   ] = REG_RD_ACCESS|REG_WR_ACCESS,
  [REG_OUTPUT_P1   ] = REG_RD_ACCESS|REG_WR_ACCESS,
  [REG_CONFIG_P0   ] = REG_RD_ACCESS|REG_WR_ACCESS,
  [REG_CONFIG_P1   ] = REG_RD_ACCESS|REG_WR_ACCESS,
  [REG_INT_P0      ] = REG_RD_ACCESS|REG_WR_ACCESS,
  [REG_INT_P1      ] = REG_RD_ACCESS|REG_WR_ACCESS,
  [REG_ID          ] = REG_RD_ACCESS,
  [REG_CTRL        ] = REG_RD_ACCESS|REG_WR_ACCESS,
  [REG_WORK_MODE_P0] = REG_RD_ACCESS|REG_WR_ACCESS,
  [REG_WORK_MODE_P1] = REG_RD_ACCESS|REG_WR_ACCESS,
  [REG_DIM00       ] = REG_WR_ACCESS,
  [REG_DIM01       ] = REG_WR_ACCESS,
  [REG_DIM02       ] = REG_WR_ACCESS,
  [REG_DIM03       ] = REG_WR_ACCESS,
  [REG_DIM04       ] = REG_WR_ACCESS,
  [REG_DIM05       ] = REG_WR_ACCESS,
  [REG_DIM06       ] = REG_WR_ACCESS,
  [REG_DIM07       ] = REG_WR_ACCESS,
  [REG_DIM08       ] = REG_WR_ACCESS,
  [REG_DIM09       ] = REG_WR_ACCESS,
  [REG_DIM10       ] = REG_WR_ACCESS,
  [REG_DIM11       ] = REG_WR_ACCESS,
  [REG_DIM12       ] = REG_WR_ACCESS,
  [REG_DIM13       ] = REG_WR_ACCESS,
  [REG_DIM14       ] = REG_WR_ACCESS,
  [REG_DIM15       ] = REG_WR_ACCESS,
  [REG_SWRST       ] = REG_WR_ACCESS,
};

static u8 pixels[15];

/******************************************************
 *
 * aw9523 i2c write/read
 *
 ******************************************************/
static int aw9523_i2c_write(struct aw9523 *aw9523, 
         unsigned char reg_addr, unsigned char reg_data)
{
    int ret = -1;
    unsigned char cnt = 0;

    while(cnt < AW_I2C_RETRIES) {
        ret = i2c_smbus_write_byte_data(aw9523->i2c, reg_addr, reg_data);
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

static int aw9523_i2c_read(struct aw9523 *aw9523, 
        unsigned char reg_addr, unsigned char *reg_data)
{
    int ret = -1;
    unsigned char cnt = 0;

    while(cnt < AW_I2C_RETRIES) {
        ret = i2c_smbus_read_byte_data(aw9523->i2c, reg_addr);
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

static int aw9523_i2c_write_bits(struct aw9523 *aw9523, 
         unsigned char reg_addr, unsigned char mask, unsigned char reg_data)
{
    unsigned char reg_val;

    aw9523_i2c_read(aw9523, reg_addr, &reg_val);
    reg_val &= mask;
    reg_val |= reg_data;
    aw9523_i2c_write(aw9523, reg_addr, reg_val);

    return 0;
}


/******************************************************
 *
 * aw9523 led
 *
 ******************************************************/
static void aw9523_brightness_work(struct work_struct *work)
{
    struct aw9523 *aw9523 = container_of(work, struct aw9523,
          brightness_work);

    unsigned char i;

    if(aw9523->cdev.brightness > aw9523->cdev.max_brightness) {
        aw9523->cdev.brightness = aw9523->cdev.max_brightness;
    }

    for(i=0; i<16; i++) {
        aw9523_i2c_write(aw9523, REG_DIM00+i,
            aw9523->cdev.brightness);                   // dimming
    }
}

static void aw9523_set_brightness(struct led_classdev *cdev,
           enum led_brightness brightness)
{
    struct aw9523 *aw9523 = container_of(cdev, struct aw9523, cdev);

    aw9523->cdev.brightness = brightness;

    schedule_work(&aw9523->brightness_work);
}

/*****************************************************
 *
 * device tree
 *
 *****************************************************/
static int aw9523_parse_dt(struct device *dev, struct aw9523 *aw9523,
        struct device_node *np)
{
    aw9523->reset_gpio = of_get_named_gpio(np, "reset-gpio", 0);
    if (aw9523->reset_gpio < 0) {
        dev_err(dev, "%s: no reset gpio provided, will not HW reset device\n", __func__);
        return -1;
    } else {
        dev_info(dev, "%s: reset gpio provided ok\n", __func__);
    }

    return 0;
}

static int aw9523_hw_reset(struct aw9523 *aw9523)
{
    pr_info("%s enter\n", __func__);

    if (aw9523 && gpio_is_valid(aw9523->reset_gpio)) {
        gpio_set_value_cansleep(aw9523->reset_gpio, 0);
        msleep(1);
        gpio_set_value_cansleep(aw9523->reset_gpio, 1);
        msleep(1);
    } else {
        dev_err(aw9523->dev, "%s:  failed\n", __func__);
    }
    return 0;
}

static int aw9523_hw_off(struct aw9523 *aw9523)
{
    pr_info("%s enter\n", __func__);

    if (aw9523 && gpio_is_valid(aw9523->reset_gpio)) {
        gpio_set_value_cansleep(aw9523->reset_gpio, 0);
        msleep(1);
    } else {
        dev_err(aw9523->dev, "%s:  failed\n", __func__);
    }
    return 0;
}

/*****************************************************
 *
 * check chip id
 *
 *****************************************************/
static int aw9523_read_chipid(struct aw9523 *aw9523)
{
    int ret = -1;
    unsigned char cnt = 0;
    unsigned char reg_val = 0;
  
    while(cnt < AW_READ_CHIPID_RETRIES) {
        ret = aw9523_i2c_read(aw9523, REG_ID, &reg_val);
        if (ret < 0) {
            dev_err(aw9523->dev,
                "%s: failed to read register aw9523_REG_ID: %d\n",
                __func__, ret);
            return -EIO;
        }
        switch (reg_val) {
        case AW9523_ID:
            pr_info("%s aw9523 detected\n", __func__);
            aw9523->chipid = AW9523_ID;
            return 0;
        default:
            pr_info("%s unsupported device revision (0x%x)\n",
                __func__, reg_val );
            break;
        }
        cnt ++;

        msleep(AW_READ_CHIPID_RETRY_DELAY);
    }

    return -EINVAL;
}


/******************************************************
 *
 * sys group attribute: reg
 *
 ******************************************************/
static ssize_t aw9523_reg_store(struct device *dev, struct device_attribute *attr,
                const char *buf, size_t count)
{
    struct led_classdev *led_cdev = dev_get_drvdata(dev);
    struct aw9523 *aw9523 = container_of(led_cdev, struct aw9523, cdev);

    unsigned int databuf[2] = {0, 0};

    if(2 == sscanf(buf, "%x %x", &databuf[0], &databuf[1])) {
        aw9523_i2c_write(aw9523, (unsigned char)databuf[0], (unsigned char)databuf[1]);
    }

    return count;
}

static ssize_t aw9523_reg_show(struct device *dev, struct device_attribute *attr,
                char *buf)
{
    struct led_classdev *led_cdev = dev_get_drvdata(dev);
    struct aw9523 *aw9523 = container_of(led_cdev, struct aw9523, cdev);
    ssize_t len = 0;
    unsigned char i = 0;
    unsigned char reg_val = 0;
    for(i = 0; i < AW9523_REG_MAX; i ++) {
        if(!(aw9523_reg_access[i]&REG_RD_ACCESS))
           continue;
        aw9523_i2c_read(aw9523, i, &reg_val);
        len += snprintf(buf+len, PAGE_SIZE-len, "reg:0x%02x=0x%02x \n", i, reg_val);
    }
    return len;
}

static ssize_t aw9523_hwen_store(struct device *dev, struct device_attribute *attr,
                const char *buf, size_t count)
{
    struct led_classdev *led_cdev = dev_get_drvdata(dev);
    struct aw9523 *aw9523 = container_of(led_cdev, struct aw9523, cdev);

    unsigned int databuf[1] = {0};

    if(1 == sscanf(buf, "%x", &databuf[0])) {
        if(1 == databuf[0]) {
            aw9523_hw_reset(aw9523);
        } else {
            aw9523_hw_off(aw9523);
        }
    }

    return count;
}

static ssize_t aw9523_hwen_show(struct device *dev, struct device_attribute *attr,
                char *buf)
{
    struct led_classdev *led_cdev = dev_get_drvdata(dev);
    struct aw9523 *aw9523 = container_of(led_cdev, struct aw9523, cdev);
    ssize_t len = 0;
    len += snprintf(buf+len, PAGE_SIZE-len, "hwen=%d\n",
            gpio_get_value(aw9523->reset_gpio));

    return len;
}

static void aw9523_pixel_work(struct work_struct *work)
{
    struct aw9523 *aw9523 = container_of(work, struct aw9523,
          pixel_work);

    unsigned char i;

    for(i=0; i<sizeof(pixels); i++) {
        aw9523_i2c_write(aw9523, pinnum_to_reg[i][1], pixels[i]);                   // dimming
    }
}

static ssize_t aw9523_pixel_store(struct device *dev, struct device_attribute *attr,
                const char *buf, size_t count)
{
    struct led_classdev *led_cdev = dev_get_drvdata(dev);
    struct aw9523 *aw9523 = container_of(led_cdev, struct aw9523, cdev);

    if(count < sizeof(pixels)){
        printk("aw9523_pixel_store ERR buf, count=%d, buf=%s\n", count, buf);
        return count;
    }
    cancel_work_sync(&aw9523->pixel_work);
    memcpy(pixels, buf, sizeof(pixels));
    schedule_work(&aw9523->pixel_work);

    return count;
}

static ssize_t led_busy_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct led_classdev *led_cdev = dev_get_drvdata(dev);
    struct aw9523 *aw9523 = container_of(led_cdev, struct aw9523, cdev);
       return snprintf(buf, 16, "%u\n", (work_busy(&aw9523->pixel_work)) ? 1 : 0);
}

static DEVICE_ATTR(reg, S_IWUSR | S_IRUGO, aw9523_reg_show, aw9523_reg_store);
static DEVICE_ATTR(hwen, S_IWUSR | S_IRUGO, aw9523_hwen_show, aw9523_hwen_store);
static DEVICE_ATTR(pixel, S_IWUSR, NULL, aw9523_pixel_store);
static DEVICE_ATTR(busy, S_IRUGO, led_busy_show, NULL);

static struct attribute *aw9523_attributes[] = {
    &dev_attr_reg.attr,
    &dev_attr_hwen.attr,
    &dev_attr_pixel.attr,
    &dev_attr_busy.attr,
    NULL
};

static struct attribute_group aw9523_attribute_group = {
    .attrs = aw9523_attributes
};


/******************************************************
 *
 * led class dev
 *
 ******************************************************/
static int aw9523_parse_led_cdev(struct aw9523 *aw9523,
        struct device_node *np)
{
    struct device_node *temp;
    int ret = -1;

    for_each_child_of_node(np, temp) {
        ret = of_property_read_string(temp, "aw9523,name",
            &aw9523->cdev.name);
        if (ret < 0) {
            dev_err(aw9523->dev,
                "Failure reading led name, ret = %d\n", ret);
            goto free_pdata;
        }
        ret = of_property_read_u32(temp, "aw9523,imax",
            &aw9523->imax);
        if (ret < 0) {
            dev_err(aw9523->dev,
                "Failure reading imax, ret = %d\n", ret);
            goto free_pdata;
        }
        ret = of_property_read_u32(temp, "aw9523,brightness",
            &aw9523->cdev.brightness);
        if (ret < 0) {
            dev_err(aw9523->dev,
                "Failure reading brightness, ret = %d\n", ret);
            goto free_pdata;
        }
        ret = of_property_read_u32(temp, "aw9523,max_brightness",
            &aw9523->cdev.max_brightness);
        if (ret < 0) {
            dev_err(aw9523->dev,
                "Failure reading max brightness, ret = %d\n", ret);
            goto free_pdata;
        }
    }

    aw9523_i2c_write(aw9523, REG_WORK_MODE_P0, 0x00);   // led mode
    aw9523_i2c_write(aw9523, REG_WORK_MODE_P1, 0x00);   // led mode
    aw9523_i2c_write(aw9523, REG_CTRL, 0x03);           // imax

    u8 default_brightness = 128; //max i is 0xff 255
    u8 i;
    for(i=0; i<sizeof(pixels); i++) {
        aw9523_i2c_write(aw9523, pinnum_to_reg[i][1], default_brightness);                   // dimming
    }
    aw9523_i2c_write(aw9523, REG_OUTPUT_P0, 0x0);           // imax
    aw9523_i2c_write(aw9523, REG_OUTPUT_P1, 0x0);           // imax


    INIT_WORK(&aw9523->brightness_work, aw9523_brightness_work);
    INIT_WORK(&aw9523->pixel_work, aw9523_pixel_work);

    aw9523->cdev.brightness_set = aw9523_set_brightness;
    ret = led_classdev_register(aw9523->dev, &aw9523->cdev);
    if (ret) {
        dev_err(aw9523->dev,
            "unable to register led ret=%d\n", ret);
        goto free_pdata;
    }

    ret = sysfs_create_group(&aw9523->cdev.dev->kobj,
            &aw9523_attribute_group);
    if (ret) {
        dev_err(aw9523->dev, "led sysfs ret: %d\n", ret);
        goto free_class;
    }

    return 0;

free_class:
    led_classdev_unregister(&aw9523->cdev);
free_pdata:
    return ret;
}

/******************************************************
 *
 * i2c driver
 *
 ******************************************************/
static int aw9523_i2c_probe(struct i2c_client *i2c, const struct i2c_device_id *id)
{
    struct aw9523 *aw9523;
    struct device_node *np = i2c->dev.of_node;
    int ret;

    pr_info("%s enter\n", __func__);

    if (!i2c_check_functionality(i2c->adapter, I2C_FUNC_I2C)) {
        dev_err(&i2c->dev, "check_functionality failed\n");
        return -EIO;
    }

    aw9523 = devm_kzalloc(&i2c->dev, sizeof(struct aw9523), GFP_KERNEL);
    if (aw9523 == NULL)
        return -ENOMEM;

    aw9523->dev = &i2c->dev;
    aw9523->i2c = i2c;

    i2c_set_clientdata(i2c, aw9523);

    /* aw9523 rst & int */
    if (np) {
        ret = aw9523_parse_dt(&i2c->dev, aw9523, np);
        if (ret) {
            dev_err(&i2c->dev, "%s: failed to parse device tree node\n", __func__);
            goto err;
        }
    } else {
        aw9523->reset_gpio = -1;
    }

    if (gpio_is_valid(aw9523->reset_gpio)) {
        ret = devm_gpio_request_one(&i2c->dev, aw9523->reset_gpio,
            GPIOF_OUT_INIT_LOW, "aw9523_rst");
        if (ret){
            dev_err(&i2c->dev, "%s: rst request failed\n", __func__);
            goto err;
        }
    }

    /* hardware reset */
    aw9523_hw_reset(aw9523);

    /* aw9523 chip id */
    ret = aw9523_read_chipid(aw9523);
    if (ret < 0) {
        dev_err(&i2c->dev, "%s: aw9523_read_chipid failed ret=%d\n", __func__, ret);
        goto err_id;
    }

    dev_set_drvdata(&i2c->dev, aw9523);

    aw9523_parse_led_cdev(aw9523, np);
    if (ret < 0) {
        dev_err(&i2c->dev, "%s error creating led class dev\n", __func__);
        goto err_sysfs;
    }

    pr_info("%s probe completed successfully!\n", __func__);

    return 0;

err_sysfs:
err_id:
err:
    return ret;
}

static int aw9523_i2c_remove(struct i2c_client *i2c)
{
    struct aw9523 *aw9523 = i2c_get_clientdata(i2c);

    pr_info("%s enter\n", __func__);

    if (gpio_is_valid(aw9523->reset_gpio))
        devm_gpio_free(&i2c->dev, aw9523->reset_gpio);

    return 0;
}

static const struct i2c_device_id aw9523_i2c_id[] = {
    { AW9523_I2C_NAME, 0 },
    { }
};
MODULE_DEVICE_TABLE(i2c, aw9523_i2c_id);

static struct of_device_id aw9523_dt_match[] = {
    { .compatible = "awinic,aw9523_led" },
    { },
};

static struct i2c_driver aw9523_i2c_driver = {
    .driver = {
        .name = AW9523_I2C_NAME,
        .owner = THIS_MODULE,
        .of_match_table = of_match_ptr(aw9523_dt_match),
    },
    .probe = aw9523_i2c_probe,
    .remove = aw9523_i2c_remove,
    .id_table = aw9523_i2c_id,
};


static int __init aw9523_i2c_init(void)
{
    int ret = 0;

    pr_info("aw9523 driver version %s\n", AW9523_VERSION);

    ret = i2c_add_driver(&aw9523_i2c_driver);
    if(ret){
        pr_err("fail to add aw9523 device into i2c\n");
        return ret;
    }

    return 0;
}
module_init(aw9523_i2c_init);


static void __exit aw9523_i2c_exit(void)
{
    i2c_del_driver(&aw9523_i2c_driver);
}
module_exit(aw9523_i2c_exit);


MODULE_DESCRIPTION("AW9523 LED Driver");
MODULE_LICENSE("GPL v2");
