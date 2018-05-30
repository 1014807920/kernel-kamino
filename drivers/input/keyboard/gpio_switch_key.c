/*
 * drivers/amlogic/input/keyboard/gpio_switch_key.c
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/types.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/errno.h>
#include <linux/irq.h>
#include <linux/of_irq.h>

#include <asm/irq.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <uapi/linux/input.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/of.h>
//#include <linux/amlogic/aml_gpio_consumer.h>
//#include <linux/amlogic/gpio-amlogic.h>
#include <linux/switch.h>
#include "../../gpio/gpiolib.h"


struct gpio_switch_key {
	int code;	  /* input switch code */
	const char *name;
	int pin;    /*pin number*/
	int irq;    /*irq number*/
	int enable; /*1 ON ,  0 OFF*/
	int invert;
};

struct gpio_switch_key_plat_data {
	struct gpio_switch_key gpio_switch;
	struct input_dev *input;
	struct work_struct work;
	struct switch_dev sdev;	/* for switch state */
	struct timer_list timer;       /* for polling timer */
};

static void switch_key_detect_work(struct gpio_switch_key_plat_data *pdata)
{
	int enable;

	enable = gpio_get_value(pdata->gpio_switch.pin);
	if (pdata->gpio_switch.invert)
		enable = !enable;

	if (enable != pdata->gpio_switch.enable) {
		if (enable) {
			pr_info("%s switch(%d) ON\n", pdata->gpio_switch.name, pdata->gpio_switch.code);
			switch_set_state(&pdata->sdev, enable);
			input_report_switch(pdata->input,  pdata->gpio_switch.code, enable);
			input_sync(pdata->input);
		} else {
			pr_info("%s switch(%d) OFF\n", pdata->gpio_switch.name, pdata->gpio_switch.code);
			switch_set_state(&pdata->sdev, 0);
			input_report_switch(pdata->input, pdata->gpio_switch.code, 0);
			input_sync(pdata->input);
		}
		pdata->gpio_switch.enable = enable;
	}
}

static void gpio_switch_key_work(struct work_struct *work)
{
	struct gpio_switch_key_plat_data *pdata;

	pdata = container_of(work, struct gpio_switch_key_plat_data, work);
	switch_key_detect_work(pdata);
}

#ifdef GPIO_SWITCH_USE_IRQ
/* irq handler for gpio pin */
static irqreturn_t gpio_switch_key_handler(int irq, void *data)
{
	struct gpio_switch_key_plat_data *pdata = data;

	schedule_work(&pdata->work);

	return IRQ_HANDLED;
}
#else
void gpio_switch_timer_sr(unsigned long data)
{
	struct gpio_switch_key_plat_data *pdata = (struct gpio_switch_key_plat_data *)data;

	schedule_work(&pdata->work);
	mod_timer(&pdata->timer, jiffies+msecs_to_jiffies(50));
}
#endif
static int gpio_switch_key_probe(struct platform_device *pdev)
{
	struct input_dev *input_dev;
	int ret = -EINVAL;
	struct gpio_switch_key_plat_data *pdata = NULL;
//	int pull = GPIOD_PULL_DIS;
	enum of_gpio_flags flags = OF_GPIO_ACTIVE_LOW;

	if (!pdev->dev.of_node) {
		dev_info(&pdev->dev, "gpio_switch_key: pdev->dev.of_node == NULL!\n");
		ret = -EINVAL;
		return ret;
	}

	pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		ret = -EINVAL;
		return ret;
	}

	pdata->gpio_switch.enable = -1;
	pdata->gpio_switch.code = SW_KEYPAD_SLIDE;
	ret = of_property_read_string(pdev->dev.of_node,
			 "switch_name", &(pdata->gpio_switch.name));
	if (ret < 0) {
		dev_info(&pdev->dev,
			"gpio_switch_key: failed to find switch_name\n");
		goto parse_switch_node_fail;
	}

	pdata->gpio_switch.pin = of_get_named_gpio_flags(pdev->dev.of_node,
			"switch_pin",  0,  &flags);
	pdata->gpio_switch.invert	= !!(flags & OF_GPIO_ACTIVE_LOW);
	dev_info(&pdev->dev, "gpio_switch_key: %s pin(%d), invert: %d\n",
			 pdata->gpio_switch.name,  pdata->gpio_switch.pin, pdata->gpio_switch.invert);
	if (!gpio_is_valid(pdata->gpio_switch.pin)) {
		dev_err(&pdev->dev, "gpio_switch_key: Invalid gpio %d\n", pdata->gpio_switch.pin);
		ret = -EINVAL;
		goto parse_switch_node_fail;
	}
	ret = gpio_request(pdata->gpio_switch.pin,  "gpio_switch_key");
	if (ret) {
		dev_err(&pdev->dev, "gpio_switch_key: failed to request gpio %d\n", pdata->gpio_switch.pin);
		goto parse_switch_node_fail;
	}
	gpio_direction_input(pdata->gpio_switch.pin);
/*	ret = of_property_read_u32(pdev->dev.of_node, "switch-gpio-pull", &pull);
	if (!ret) {
		dev_info(&pdev->dev, "switch_key gpio pull %d\n", pull);
		if (pull >= GPIOD_PULL_DIS && pull <= GPIOD_PULL_UP) {
			gpiod_set_pull(gpio_to_desc((pdata->gpio_switch.pin)), pull);
		}
	}
*/
	pdata->sdev.name = pdata->gpio_switch.name;
	ret = switch_dev_register(&pdata->sdev);
	if (ret < 0) {
		dev_err(&pdev->dev, "gpio_switch_key: failed to register switch lock dev\n");
		goto switch_key_register_failed;
	}
	INIT_WORK(&pdata->work, gpio_switch_key_work);
#ifdef GPIO_SWITCH_USE_IRQ// GPIO_AO only support one IRQ ,bus use for HEADPHONE DETECT, so key use polling mode.
	pdata->gpio_switch.irq = gpio_to_irq(pdata->gpio_switch.pin);
	if (pdata->gpio_switch.irq < 0) {
		ret = pdata->gpio_switch.irq;
		goto input_irq_fail;
	}

	ret = request_any_context_irq(pdata->gpio_switch.irq, gpio_switch_key_handler,
			IRQF_TRIGGER_RISING |IRQF_TRIGGER_FALLING,
			pdata->gpio_switch.name, pdata);
	if (ret) {
		dev_err(&pdev->dev, "gpio_switch_key: failed to request irq pin %d\n", pdata->gpio_switch.pin);
		goto input_irq_fail;
	}
#else// polling mode
	dev_info(&pdev->dev, "start setup_timer");
	setup_timer(&pdata->timer,  gpio_switch_timer_sr,  (unsigned long)pdata);
	mod_timer(&pdata->timer,  jiffies+msecs_to_jiffies(100));
#endif
	input_dev = input_allocate_device();
	if ( !input_dev) {
		ret = -ENOMEM;
		goto input_alloc_fail;
	}
	pdata->input = input_dev;
	platform_set_drvdata(pdev,  pdata);

	/* setup input device */
	set_bit(EV_SW,  input_dev->evbit);
	input_set_capability(input_dev, EV_SW, pdata->gpio_switch.code);
	input_dev->name = "switch_key";
	input_dev->phys = "switch_key";
	input_dev->dev.parent = &pdev->dev;

	ret = input_register_device(pdata->input);
	if (ret < 0) {
		dev_err(&pdev->dev,  "Unable to register switch input device.\n");
		goto input_register_failed;
	}
	dev_info(&pdev->dev, "gpio switch_key register input device completed.\n");
	return 0;

input_register_failed:
	input_free_device(input_dev);
input_alloc_fail:
#ifdef GPIO_SWITCH_USE_IRQ
	free_irq(pdata->gpio_switch.irq, pdata);
input_irq_fail:
#endif
	switch_dev_unregister(&pdata->sdev);
switch_key_register_failed:
	gpio_free(pdata->gpio_switch.pin);
parse_switch_node_fail:
	kfree(pdata);
	return ret;
}

static int gpio_switch_key_remove(struct platform_device *pdev)
{
	struct gpio_switch_key_plat_data *pdata = platform_get_drvdata(pdev);

	input_unregister_device(pdata->input);
	input_free_device(pdata->input);
#ifdef GPIO_SWITCH_USE_IRQ
	free_irq(pdata->gpio_switch.irq, pdata);
#endif
	switch_dev_unregister(&pdata->sdev);
	gpio_free(pdata->gpio_switch.pin);
	kfree(pdata);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id switch_key_dt_match[] = {
	{	.compatible = "rokid, switch_key", },
	{},
};
#else
#define switch_key_dt_match NULL
#endif

static struct platform_driver gpio_switch_key_driver = {
	.probe = gpio_switch_key_probe,
	.remove = gpio_switch_key_remove,
	.driver = {
		.name = "switch_key",
		.of_match_table = switch_key_dt_match,
	},
};

static int __init gpio_switch_key_init(void)
{
	return platform_driver_register(&gpio_switch_key_driver);
}

static void __exit gpio_switch_key_exit(void)
{
	platform_driver_unregister(&gpio_switch_key_driver);
}

module_init(gpio_switch_key_init);
module_exit(gpio_switch_key_exit);

MODULE_AUTHOR("Sweet Fan");
MODULE_DESCRIPTION("GPIO Switch Key Driver");
MODULE_LICENSE("GPL");

