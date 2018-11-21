#include <linux/string.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/firmware.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/debugfs.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>
#include <linux/leds.h>

int (*yodabase_led_draw)(char *buf, int length) = NULL;
int (*yodabase_is_led_busy)(void) = NULL;
int (*yodabase_led_brightness_set)(int brightness) = NULL;

#define BREATH_PATTERN 1

struct light {
    struct platform_device *platform_dev;
    struct device *dev;
    int led_num;
    char *led_charbuf;
    int enable_in_probe;
    int led_inter_time;
    int pattern_data;
    int raw_data_length;
    u8 *raw_data;
    int brightness;
    struct delayed_work light_delayed_workqueue;
};

static ssize_t light_pixel_store(struct device *dev, struct device_attribute *attr,
                const char *buf, size_t count)
{
    struct light *light_stage = dev_get_drvdata(dev);

    if(light_stage->enable_in_probe){
        cancel_delayed_work_sync(&light_stage->light_delayed_workqueue);
        light_stage->enable_in_probe = 0;
    }

    if(count != light_stage->led_num){
        dev_err(dev, "light_pixel_store ERR buf,count(%d) != led_num(%d), buf=%s\n", count,light_stage->led_num, buf);
        return count;
    }else{
        memcpy(light_stage->led_charbuf, buf, count);
    }

    if(yodabase_led_draw == NULL){
        dev_err(dev,"%s,yodabase_led_draw is NUll",__func__);
    }else{
        (*yodabase_led_draw)(light_stage->led_charbuf,light_stage->led_num);
    }

    return count;
}

static ssize_t light_pixel_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct light *light_stage = dev_get_drvdata(dev);
    memcpy(buf, light_stage->led_charbuf, light_stage->led_num);
    return 0;
}

static ssize_t light_busy_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    int ret = (*yodabase_is_led_busy)();
    return snprintf(buf, 16, "%u\n", (ret) ? 1 : 0);
}

static ssize_t light_brightness_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct light *light_stage = dev_get_drvdata(dev);
    return snprintf(buf, 16, "%u\n", light_stage->brightness);
}

static ssize_t light_brightness_store(struct device *dev, struct device_attribute *attr,
                const char *buf, size_t count)
{
    struct light *light_stage = dev_get_drvdata(dev);
    unsigned long state;
    int ret = kstrtoul(buf, 10, &state);
    if(ret < 0){
        dev_err(dev,"%s,buf=%s is not right\n",__func__, buf);
        return -1;
    }
    light_stage->brightness = (int)state;
    if(yodabase_led_brightness_set == NULL){
        dev_err(dev,"%s,yodabase_led_brightness_set is NUll",__func__);
    }else{
        (*yodabase_led_brightness_set)(light_stage->brightness);
    }
    return count;
}

static DEVICE_ATTR(pixel, S_IWUSR | S_IRUGO, light_pixel_show, light_pixel_store);
static DEVICE_ATTR(busy, S_IRUGO, light_busy_show, NULL);
static DEVICE_ATTR(brightness, S_IWUSR | S_IRUGO, light_brightness_show, light_brightness_store);

static struct attribute *light_attributes[] = {
    &dev_attr_pixel.attr,
    &dev_attr_busy.attr,
    &dev_attr_brightness.attr,
    NULL
};

static struct attribute_group light_attribute_group = {
    .attrs = light_attributes
};

static uint32_t bgr_cal(uint32_t srcRGB, uint32_t dstRGB, int devision, int seq)
{

    uint8_t tmpR, tmpG, tmpB;

    uint8_t srcR = (uint8_t)((srcRGB >> 16) & 0xFF);
    uint8_t srcG = (uint8_t)((srcRGB >> 8) & 0xFF);
    uint8_t srcB = (uint8_t)((srcRGB >> 0) & 0xFF);

    uint8_t dstR = (uint8_t)((dstRGB >> 16) & 0xFF);
    uint8_t dstG = (uint8_t)((dstRGB >> 8) & 0xFF);
    uint8_t dstB = (uint8_t)((dstRGB >> 0) & 0xFF);

    tmpR = srcR + (dstR - srcR) * seq / devision;
    tmpG = srcG + (dstG - srcG) * seq / devision;
    tmpB = srcB + (dstB - srcB) * seq / devision;

    uint32_t result = tmpR << 16 | tmpG << 8| tmpB;
    return result;
}

static void breath_pattern(struct light *light_stage)
{
    const int div = 64;
    const uint32_t led_ref[3] = {0xFF0000, 0xCC00FF, 0xFFFFFF};//attention it is bgr, not rgb!
    int light_num = sizeof(led_ref)/sizeof(led_ref[0]);

    static uint32_t circle_num = 0,seq = 0;
    int i;

    seq++;
    seq = seq % div;
    if(seq == 0){
        circle_num++;
        circle_num = circle_num % light_num;
    }
    uint32_t led_bgr32 = bgr_cal(led_ref[circle_num], led_ref[(circle_num+1)%light_num], div, seq);
    uint8_t led_bgr8[3];
    led_bgr8[0] = (uint8_t)((led_bgr32 >> 0) & 0xFF);
    led_bgr8[1] = (uint8_t)((led_bgr32 >> 8) & 0xFF);
    led_bgr8[2] = (uint8_t)((led_bgr32 >> 16) & 0xFF);

    for(i = 0; i<light_stage->led_num; i++){
        *((light_stage->led_charbuf) + i) = led_bgr8[i%3];
    }
}

static void raw_data_prepare(struct light *light_stage)
{
    static int tick = 0;
    memcpy(light_stage->led_charbuf, ((light_stage->raw_data)+tick), light_stage->led_num);
    tick = tick + light_stage->led_num;
    if(tick == light_stage->raw_data_length)
        tick = 0;
}

static void prepare_buf(struct light *light_stage)
{

    if((light_stage->pattern_data) > 0){
        switch(light_stage->pattern_data){
            case BREATH_PATTERN:
                breath_pattern(light_stage);
                break;
            default:
                break;
        }
    }else{
        raw_data_prepare(light_stage);
    }
}

static void light_update_work_func(struct delayed_work *work)
{
    struct light *light_stage = container_of(work, struct light, light_delayed_workqueue);
    int ret = 0;
    int i;

    if(yodabase_led_draw != NULL){
        prepare_buf(light_stage);
        ret = (*yodabase_led_draw)(light_stage->led_charbuf,light_stage->led_num);
        if(ret == 0){
            dev_err(light_stage->dev,"light_update_work_func disable yodabase_led_draw\n");
            return;
        }
    }

    schedule_delayed_work(&(light_stage->light_delayed_workqueue), msecs_to_jiffies(light_stage->led_inter_time));
}

static int led_stage_probe(struct platform_device *pdev)
{
    struct light *light_stage;
    int ret;

    dev_info(&pdev->dev, "%s enter\n", __func__);

    light_stage = devm_kzalloc(&pdev->dev, sizeof(struct light), GFP_KERNEL);
    if (!light_stage)
        return -ENOMEM;

    ret = of_property_read_u32(pdev->dev.of_node, "led_num", &light_stage->led_num);
    if (ret) {
        dev_err(&pdev->dev, "%s,read no led_num\n", __func__);
        return -EINVAL;
    }

    light_stage->led_charbuf = kzalloc(light_stage->led_num,  GFP_KERNEL);
    if (light_stage->led_charbuf == NULL)
        return -ENOMEM;

    light_stage->enable_in_probe = 0;
    ret = of_property_read_u32(pdev->dev.of_node, "enable_in_probe", &light_stage->enable_in_probe);
    if (ret) {
        dev_err(&pdev->dev, "%s,read no enable_in_probe\n", __func__);
        goto free_led_charbuf;
    }

    if(light_stage->enable_in_probe){
        ret = of_property_read_u32(pdev->dev.of_node, "led_inter_time", &light_stage->led_inter_time);
        if (ret) {
            dev_err(&pdev->dev, "%s,read no led_inter_time\n", __func__);
            ret = -EINVAL;
            goto free_led_charbuf;
        }

        ret = of_property_read_u32(pdev->dev.of_node, "pattern_data", &light_stage->pattern_data);
        if (ret) {
            dev_err(&pdev->dev, "%s,read no pattern_data\n", __func__);
        }

        ret = of_property_read_u32(pdev->dev.of_node, "raw_data_length", &light_stage->raw_data_length);
        if (!ret) {
            light_stage->raw_data = kzalloc(light_stage->raw_data_length,  GFP_KERNEL);
            if (light_stage->raw_data == NULL){
                dev_err(&pdev->dev, "%s,fail kzalloc raw_data\n", __func__);
                goto free_led_charbuf;
            }
            ret = of_property_read_u8_array(pdev->dev.of_node,  "raw_data", light_stage->raw_data, light_stage->raw_data_length);
            if(ret){
                dev_err(&pdev->dev, "%s,fail to read raw_data\n", __func__);
                goto free_raw_data;
            }
        } else {
            dev_err(&pdev->dev, "%s,read no raw_data_length\n", __func__);
        }

        INIT_DELAYED_WORK(&(light_stage->light_delayed_workqueue),  light_update_work_func);
        schedule_delayed_work(&(light_stage->light_delayed_workqueue), msecs_to_jiffies(100));
    }else{
        dev_info(&pdev->dev, "%s,enable_in_probe is disable\n", __func__);
    }

    light_stage->platform_dev = pdev;
    light_stage->dev = &pdev->dev;
    platform_set_drvdata(pdev, light_stage);

    ret = sysfs_create_group(&light_stage->dev->kobj, &light_attribute_group);
    if (ret) {
        dev_err(&pdev->dev, "%s,sysfs_create_group fail\n", __func__);
        goto free_led_charbuf;
    }

    dev_info(&pdev->dev, "%s probe completed successfully!\n", __func__);

    return 0;

free_raw_data:
    kfree(light_stage->raw_data);
free_led_charbuf:
    kfree(light_stage->led_charbuf);

    return -EINVAL;
}

static int led_stage_remove(struct platform_device *pdev)
{
    struct light *light_stage = platform_get_drvdata(pdev);
    if(light_stage->enable_in_probe){
        cancel_delayed_work_sync(&light_stage->light_delayed_workqueue);
        if((light_stage->led_charbuf))
            kfree(light_stage->led_charbuf);
        if((light_stage->raw_data))
            kfree(light_stage->raw_data);
    }
    sysfs_remove_group(&light_stage->dev->kobj, &light_attribute_group); 
    return 0;
}

static int led_stage_suspend(struct platform_device *dev,  pm_message_t state)
{
    return 0;
}

static int led_stage_resume(struct platform_device *dev)
{
    return 0;
}

static const struct of_device_id led_dt_match[] = {
    {   .compatible = "rokid, led-stage", },
    {},
};

static struct platform_driver led_stage_driver = {
    .probe = led_stage_probe,
    .remove = led_stage_remove,
    .suspend = led_stage_suspend,
    .resume = led_stage_resume,
    .driver = {
        .name = "led-stage",
        .of_match_table = led_dt_match,
    },
};

static int __init led_stage_init(void)
{
    return platform_driver_register(&led_stage_driver);
}

static void __exit led_stage_exit(void)
{
    platform_driver_unregister(&led_stage_driver);
}

module_init(led_stage_init);
module_exit(led_stage_exit);

MODULE_AUTHOR("Eric.yang");
MODULE_DESCRIPTION("Led starge Driver");
MODULE_LICENSE("GPL");