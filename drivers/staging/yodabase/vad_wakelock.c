/*
 * (C) Copyright 2018
 * Rokid Software Engineering, bin.zhu@rokid.com
 *
 * SPDX-License-Identifier: GPL-2.0+
 */
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

#define VAD_WAKELOCK "vad_wakelock"

extern ssize_t pm_show_wakelocks(char *buf, bool show_active);
extern int pm_wake_lock(const char *buf);
extern int pm_wake_unlock(const char *buf);

static int wakelock_timeout_in_s;
static struct delayed_work wakelock_timeout_delayed_work; 

static void wakelock_timeout_delayed_work_func(struct work_struct *work)
{
    pm_wake_unlock(VAD_WAKELOCK);
}

static int vad_wakelock_probe(struct platform_device *pdev)
{
    int ret;

    printk("vad_wakelock_probe.\n");
    ret = of_property_read_u32(pdev->dev.of_node, "timeout_in_s", &wakelock_timeout_in_s);
    if (ret) {
        dev_err(&pdev->dev, "%s,read no timeout_in_s\n", __func__);
	wakelock_timeout_in_s = 1;
    }
    INIT_DELAYED_WORK(&wakelock_timeout_delayed_work, wakelock_timeout_delayed_work_func);
    return 0;
}

static int vad_wakelock_remove(struct platform_device *pdev)
{
    cancel_delayed_work_sync(&wakelock_timeout_delayed_work);
    return 0;
}

#if defined(CONFIG_PM_SLEEP) && defined(CONFIG_ARM)
static int vad_wakelock_suspend(struct platform_device *dev,  pm_message_t state)
{
    printk("vad_wakelock_suspend.\n");
    return 0;
}

static int vad_wakelock_resume(struct platform_device *dev)
{
    printk("vad_wakelock_resume:pm_wake_lock here.\n");
    pm_wake_lock(VAD_WAKELOCK);
    schedule_delayed_work(&wakelock_timeout_delayed_work, wakelock_timeout_in_s*HZ);
    return 0;
}

static SIMPLE_DEV_PM_OPS(vad_wakelock_pm_ops, vad_wakelock_suspend, vad_wakelock_resume);
#endif

static const struct of_device_id vad_wakelock_dt_match[] = {
    {   .compatible = "rokid,vad-wakelock", },
    {},
};

static struct platform_driver vad_wakelock_driver = {
    .probe = vad_wakelock_probe,
    .remove = vad_wakelock_remove,
    .driver = {
        .name = "vad-wakelock",
        .of_match_table = vad_wakelock_dt_match,
#if defined(CONFIG_PM_SLEEP) && defined(CONFIG_ARM)
        .pm = &vad_wakelock_pm_ops,
#endif
    },
};

static int __init vad_wakelock_init(void)
{
    return platform_driver_register(&vad_wakelock_driver);
}

static void __exit vad_wakelock_exit(void)
{
    platform_driver_unregister(&vad_wakelock_driver);
}

module_init(vad_wakelock_init);
module_exit(vad_wakelock_exit);

MODULE_AUTHOR("bin.zhu");
MODULE_DESCRIPTION("Vad Wakelock Driver");
MODULE_LICENSE("GPL");

