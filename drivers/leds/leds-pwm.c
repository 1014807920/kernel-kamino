/*
 * linux/drivers/leds-pwm.c
 *
 * simple PWM based LED control
 *
 * Copyright 2009 Luotao Fu @ Pengutronix (l.fu@pengutronix.de)
 *
 * based on leds-gpio.c by Raphael Assenat <raph@8d.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include <linux/fb.h>
#include <linux/leds.h>
#include <linux/err.h>
#include <linux/pwm.h>
#include <linux/leds_pwm.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/jiffies.h>

struct timer_list prob_timer;
#define DEFAULT_DELAY_MS 6000

struct device *led_dev;

struct led_pwm_data {
	struct led_classdev	cdev;
	struct pwm_device	*pwm;
	struct work_struct	work;
	unsigned int		active_low;
	unsigned int		period;
	int			duty;
	bool			can_sleep;
};

struct led_pwm_priv {
	int num_leds;
#if defined(CONFIG_LEDS_OTA)
    struct work_struct ota_light_work;
#endif
	struct led_pwm_data leds[0];
};

#if defined(CONFIG_LEDS_OTA)
static struct led_pwm_priv *g_priv;
static int ota_light_start = 0;
static int ota_light_end = 0;
static struct kobject *ota_light_kobj;
#endif

static void __led_pwm_set(struct led_pwm_data *led_dat)
{
	int new_duty = led_dat->duty;

	pwm_config(led_dat->pwm, new_duty, led_dat->period);

	if (new_duty == 0)
		pwm_disable(led_dat->pwm);
	else
		pwm_enable(led_dat->pwm);
}

static void led_pwm_work(struct work_struct *work)
{
	struct led_pwm_data *led_dat =
		container_of(work, struct led_pwm_data, work);
	__led_pwm_set(led_dat);
}

static void led_pwm_set(struct led_classdev *led_cdev,
	enum led_brightness brightness)
{
	struct led_pwm_data *led_dat =
		container_of(led_cdev, struct led_pwm_data, cdev);
	unsigned int max = led_dat->cdev.max_brightness;
	unsigned long long duty =  led_dat->period;

	duty *= brightness;
	do_div(duty, max);

	if (led_dat->active_low)
		duty = led_dat->period - duty;

	led_dat->duty = duty;

	if (led_dat->can_sleep)
		schedule_work(&led_dat->work);
	else
		__led_pwm_set(led_dat);
}

static inline size_t sizeof_pwm_leds_priv(int num_leds)
{
	return sizeof(struct led_pwm_priv) +
		      (sizeof(struct led_pwm_data) * num_leds);
}

static void led_pwm_cleanup(struct led_pwm_priv *priv)
{
	while (priv->num_leds--) {
		led_classdev_unregister(&priv->leds[priv->num_leds].cdev);
		if (priv->leds[priv->num_leds].can_sleep)
			cancel_work_sync(&priv->leds[priv->num_leds].work);

		//struct device *dev = container_of(&priv, struct device, driver_data);
		devm_pwm_put(led_dev, priv->leds[priv->num_leds].pwm);
	}
}

static int led_pwm_add(struct device *dev, struct led_pwm_priv *priv,
		       struct led_pwm *led, struct device_node *child)
{
	struct led_pwm_data *led_data = &priv->leds[priv->num_leds];
	int ret;

	led_data->active_low = led->active_low;
	led_data->cdev.name = led->name;
	led_data->cdev.default_trigger = led->default_trigger;
	led_data->cdev.brightness_set = led_pwm_set;
	led_data->cdev.brightness = LED_OFF;
	led_data->cdev.max_brightness = led->max_brightness;
	led_data->cdev.flags = LED_CORE_SUSPENDRESUME;

	if (child)
		led_data->pwm = devm_of_pwm_get(dev, child, NULL);
	else
		led_data->pwm = devm_pwm_get(dev, led->name);
	if (IS_ERR(led_data->pwm)) {
		ret = PTR_ERR(led_data->pwm);
		dev_err(dev, "unable to request PWM for %s: %d\n",
			led->name, ret);
		return ret;
	}

	led_data->can_sleep = pwm_can_sleep(led_data->pwm);
	if (led_data->can_sleep)
		INIT_WORK(&led_data->work, led_pwm_work);

	led_data->period = pwm_get_period(led_data->pwm);
	if (!led_data->period && (led->pwm_period_ns > 0))
		led_data->period = led->pwm_period_ns;

	ret = led_classdev_register(dev, &led_data->cdev);
	if (ret == 0) {
		priv->num_leds++;
	} else {
		dev_err(dev, "failed to register PWM led for %s: %d\n",
			led->name, ret);
	}

	return ret;
}

static int led_pwm_create_of(struct device *dev, struct led_pwm_priv *priv)
{
	struct device_node *child;
	struct led_pwm led;
	int ret = 0;

	memset(&led, 0, sizeof(led));

	for_each_child_of_node(dev->of_node, child) {
		led.name = of_get_property(child, "label", NULL) ? :
			   child->name;

		led.default_trigger = of_get_property(child,
						"linux,default-trigger", NULL);
		led.active_low = of_property_read_bool(child, "active-low");
		of_property_read_u32(child, "max-brightness",
				     &led.max_brightness);

		ret = led_pwm_add(dev, priv, &led, child);
		if (ret) {
			of_node_put(child);
			break;
		}
	}

	return ret;
}

static void prob_timer_fun(unsigned long data)
{
#if !defined(CONFIG_LEDS_OTA)
	led_pwm_cleanup(data);
#endif
}

#if defined(CONFIG_LEDS_OTA)
static void ota_light_workfunc(struct work_struct *work)
{
    int count = 0;
    if(ota_light_start){
        for (count = 0;count < 4;count++)
                led_pwm_set(&g_priv->leds[count].cdev, 100);

        while(ota_light_start) {
            for (count = 4;count > 0;count--) {
                led_pwm_set(&g_priv->leds[count % 4].cdev, 255);
                led_pwm_set(&g_priv->leds[(count - 1) % 4].cdev, 192);
                msleep(220);
                led_pwm_set(&g_priv->leds[count % 4].cdev, 100);
                led_pwm_set(&g_priv->leds[(count - 1) % 4].cdev, 100);
            }
            if (!ota_light_start || ota_light_end)
                break;
        }
    }

    if (ota_light_end) {
        for (count = 0;count < 4;count++)
            led_pwm_set(&g_priv->leds[count].cdev, 255);
        msleep(1000);
        for (count = 0;count < 4;count++)
            led_pwm_set(&g_priv->leds[count].cdev, 0);
    }
}

static ssize_t ota_light_start_show(struct kobject *kobj, struct kobj_attribute *attr,
        char *buf)
{
    return sprintf(buf, "%d\n", ota_light_start);
}

static ssize_t ota_light_start_store(struct kobject *kobj, struct kobj_attribute *attr,
        const char *buf, size_t count)
{
    char *after;
    unsigned long status = simple_strtoul(buf, &after, 10);

    ota_light_start = (int) status;
    if (ota_light_start) {
        ota_light_end = 0;
        schedule_work(&g_priv->ota_light_work);
    }

    return count;
}

static ssize_t ota_light_end_show(struct kobject *kobj, struct kobj_attribute *attr,
        char *buf)
{
    return sprintf(buf, "%d\n", ota_light_end);
}

static ssize_t ota_light_end_store(struct kobject *kobj, struct kobj_attribute *attr,
        const char *buf, size_t count)
{
    char *after;
    unsigned long status = simple_strtoul(buf, &after, 10);

    ota_light_end = (int) status;
    if (ota_light_end) {
        ota_light_start = 0;
        schedule_work(&g_priv->ota_light_work);
    }

    return count;
}

struct kobj_attribute ota_light_start_attr = {
    .attr = {"ota_start", 0660},
    .show = &ota_light_start_show,
    .store = &ota_light_start_store,
};

struct kobj_attribute ota_light_end_attr = {
    .attr = {"ota_end", 0660},
    .show = &ota_light_end_show,
    .store = &ota_light_end_store,
};

static struct attribute *ota_light_attr[] = {
    &ota_light_start_attr.attr,
    &ota_light_end_attr.attr,
    NULL,
};

static struct attribute_group ota_light_attr_group = {
    .attrs = ota_light_attr,
};
#endif

static int led_pwm_probe(struct platform_device *pdev)
{
	struct led_pwm_platform_data *pdata = dev_get_platdata(&pdev->dev);
	struct led_pwm_priv *priv;
	int count, i;
	int ret = 0;

	if (pdata)
		count = pdata->num_leds;
	else
		count = of_get_child_count(pdev->dev.of_node);

	if (!count)
		return -EINVAL;

	priv = devm_kzalloc(&pdev->dev, sizeof_pwm_leds_priv(count),
			    GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	if (pdata) {
		for (i = 0; i < count; i++) {
			ret = led_pwm_add(&pdev->dev, priv, &pdata->leds[i],
					  NULL);
			if (ret)
				break;
		}
	} else {
		ret = led_pwm_create_of(&pdev->dev, priv);
	}

	if (ret) {
		led_pwm_cleanup(priv);
		return ret;
	}

	platform_set_drvdata(pdev, priv);
	led_dev = &pdev->dev;

	unsigned int msecs_tmp;
	if(of_property_read_s32((&pdev->dev)->of_node, "default-keep-ms", &msecs_tmp) != 0){
		msecs_tmp = DEFAULT_DELAY_MS;
	}

#if defined(CONFIG_LEDS_OTA)
    g_priv = priv;
    ota_light_kobj = kobject_create_and_add("ota_light", NULL);
    if (ota_light_kobj) {
        ret = sysfs_create_group(ota_light_kobj, &ota_light_attr_group);
    }
    INIT_WORK(&priv->ota_light_work, ota_light_workfunc);
#endif

	setup_timer(&prob_timer,  prob_timer_fun,  (unsigned long)priv);
	mod_timer(&prob_timer,  jiffies+msecs_to_jiffies(msecs_tmp));

	return 0;
}

static int led_pwm_remove(struct platform_device *pdev)
{
	struct led_pwm_priv *priv = platform_get_drvdata(pdev);

	led_pwm_cleanup(priv);
	del_timer(&prob_timer);

	return 0;
}

static const struct of_device_id of_pwm_leds_match[] = {
	{ .compatible = "pwm-leds", },
	{},
};
MODULE_DEVICE_TABLE(of, of_pwm_leds_match);

static struct platform_driver led_pwm_driver = {
	.probe		= led_pwm_probe,
	.remove		= led_pwm_remove,
	.driver		= {
		.name	= "leds_pwm",
		.of_match_table = of_pwm_leds_match,
	},
};

module_platform_driver(led_pwm_driver);

MODULE_AUTHOR("Luotao Fu <l.fu@pengutronix.de>");
MODULE_DESCRIPTION("PWM LED driver for PXA");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:leds-pwm");
