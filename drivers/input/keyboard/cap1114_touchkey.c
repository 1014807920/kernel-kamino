#include <linux/module.h>
#include <linux/input.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/bitops.h>
#include <linux/interrupt.h>
#include <linux/of_device.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/spinlock.h>
#include <linux/poll.h>
#include <linux/of_gpio.h>
#include <linux/sysfs.h>

#define CONFIG_TOUCHKEY_KTHREAD
//#undef CONFIG_TOUCHKEY_KTHREAD  /* use interrupt */

#define TOUCHKEY_POOL_INTERVAL 5


#define AML_I2C_BUS_AO          0
#define AML_I2C_BUS_A           1
#define AML_I2C_BUS_B           2
#define AML_I2C_BUS_C           3

/* CAP1XXX has 14 keys */
#define CAP1XXX_MAX_KEY_COUNT       14
/* CAP1XXX chip id */
#define CAPxxxx_CHIP_ID     0x3A
/* Button Status Register_1 */
#define TOUCH_STATUS_REG_1      0x03
/* Masks for button status register1, 6bit are valid */
#define TOUCH_STATU_1_MASK      0x3f
/* Button Status Register_2 */
#define TOUCH_STATUS_REG_2      0x04
/* Masks for touch and release triggers */
#define TOUCH_STATUS_MASK       0x3fff
#define CAP1XXX_I2C_ADDR 0x28


static int debug_enable = 0;

#define TOUCHKEY_DEBUG(format, args...) do{ \
    if(debug_enable) \
    {\
        printk(KERN_EMERG format,##args);\
    }\
}while(0)

struct cap1xxx_touchkey_priv {
    struct input_dev    *input_dev;
    unsigned int        key_val;
    unsigned int        statusbits;
    unsigned int        *keyMap;
    unsigned int        keySize;
#ifdef CONFIG_TOUCHKEY_KTHREAD
    unsigned int        pool_interval;// ms
    struct task_struct  *handler;
#endif
};

struct cap1xxx_init_register {
    u8 addr;
    u8 val;
};

static const struct cap1xxx_init_register init_reg_table[] = {
    {   0x00,       0x00    },      // Main Control
    {   0x1F,       0x4F    },      // Sensitivity Control  default:0x2F
    {   0x20,       0x20    },      // General Configuration
    {   0x21,       0xFF    },      // Sensor Input Enable
    {   0x22,       0xA4    },      // Sensor Input Configuration
    {   0x23,       0x07    },      // Sensor Input Configuration 2
    {   0x24,       0x39    },      // Averaging and Sampling Config
    {   0x26,       0x00    },      // Calibration Activate
    {   0x27,       0xFF    },      // Interrupt Enable
    {   0x28,       0xFF    },      // Repeat Rate Enable
    {   0x2A,       0x00    },      // Multiple Touch Configuration
    {   0x2B,       0x00    },      // Multiple Touch Pattern Config
    {   0x2D,       0xFF    },      // Multiple Touch Pattern
    {   0x2F,       0x8A    },      // Recalibration Configuration
    {   0x30,       0x28    },      // Sensor Input 1 Threshold
    {   0x31,       0x28    },      // Sensor Input 2 Threshold
    {   0x32,       0x28    },      // Sensor Input 3 Threshold
    {   0x33,       0x24    },      // Sensor Input 4 Threshold
    {   0x34,       0x24    },      // Sensor Input 5 Threshold
    {   0x35,       0x24    },      // Sensor Input 6 Threshold
    {   0x36,       0x28    },      // Sensor Input 7 Threshold
    {   0x37,       0x1e    },      // Sensor Input 8 Threshold
    {   0x38,       0xAA    },      // Sensor Input Noise Threshold
    {   0x39,       0xAA    },      // Sensor Input Noise Threshold
    {   0x40,       0x02    },      // Standby Channel
    {   0x41,       0x03    },      // Standby Configuration
    {   0x42,       0x02    },      // Standby Sensitivity
    {   0x43,       0x40    },      // Standby Threshold
    {   0x44,       0x40    },      // Configuration 2
    {   0x4E,       0xFF    },      // Sampling Channel Select
    {   0x4F,       0x00    },      // Sampling Configuration
    {   0x71,       0x00    },      // LED Output Type
    {   0x72,       0x00    },      // LED Sensor Input Linking
    {   0x73,       0x00    },      // LED Polarity
    {   0x74,       0x00    },      // LED Output Control
    {   0x77,       0x00    },      // LED Linked Transition Control
    {   0x79,       0x00    },      // LED Mirror Control
    {   0x81,       0x00    },      // LED Behavior 1
    {   0x82,       0x00    },      // LED Behavior 2
    {   0x84,       0x20    },      // LED Pulse 1 Period
    {   0x85,       0x14    },      // LED Pulse 2 Period
    {   0x86,       0x5D    },      // LED Breathe Period
    {   0x88,       0x04    },      // LED Config
    {   0x90,       0xF0    },      // LED Pulse 1 Duty Cycle
    {   0x91,       0xF0    },      // LED Pulse 2 Duty Cycle
    {   0x92,       0xF0    },      // LED Breathe Duty Cycle
    {   0x93,       0xF0    },      // LED Direct Duty Cycle
    {   0x94,       0x00    },      // LED Direct Ramp Rates
    {   0x95,       0x00    },      // LED Off Delay
};

static int pebble_mini_keyMap[CAP1XXX_MAX_KEY_COUNT] =
{
    KEY_1,  KEY_2,  KEY_3,  KEY_4,
    KEY_5,  KEY_6,  KEY_7,  KEY_8,
    KEY_0,
    KEY_9,  KEY_F1, KEY_F2, KEY_F3, KEY_F4,
};


static struct i2c_client *cap1xxx_i2c_client = NULL;
static struct cap1xxx_touchkey_priv *tHandle;


/****************************************************************************
 * local functions
 ***************************************************************************/
static int  cap1xxx_touchkey_i2c_probe(struct i2c_client *client,
                                  const struct i2c_device_id *id);
static int  cap1xxx_touchkey_i2c_remove(struct i2c_client *client);
static int  cap1xxx_touchkey_i2c_suspend(struct device *dev);
static int  cap1xxx_touchkey_i2c_resume(struct device *dev);

static const struct i2c_device_id cap1xxx_i2c_id[] = {
    {"cap1xxx", 0},
    {}
};

MODULE_DEVICE_TABLE(i2c, cap1xxx_i2c_id);

static struct of_device_id cap1xxx_match_table[] = {
    {.compatible = "nationalchip,cap1xxx_i2c",},
    {}
};

#ifdef CONFIG_PM
static const struct dev_pm_ops cap1xxx_touchkey_pm_ops = {
    .suspend    = cap1xxx_touchkey_i2c_suspend,
    .resume     = cap1xxx_touchkey_i2c_resume,
};
#endif

static struct i2c_driver cap1xxx_i2c_driver = {
    .id_table = cap1xxx_i2c_id,
    .probe = cap1xxx_touchkey_i2c_probe,
    .remove = cap1xxx_touchkey_i2c_remove,
    .driver = {
        .owner = THIS_MODULE,
        .name = "cap1xxx_i2c",
        .of_match_table = cap1xxx_match_table,
#ifdef CONFIG_PM
        .pm = &cap1xxx_touchkey_pm_ops,
#endif
    },
};

static int cap1xxx_read_reg(u8 addr, u8 *data)
{
    struct i2c_msg msg[2];
    int ret;

    if (cap1xxx_i2c_client != NULL) {
        msg[0].addr = cap1xxx_i2c_client->addr;
        msg[0].flags = 0;
        msg[0].buf = &addr;
        msg[0].len = 1;

        msg[1].addr = cap1xxx_i2c_client->addr;
        msg[1].flags = I2C_M_RD;
        msg[1].buf = data;
        msg[1].len = 1;

        ret = i2c_transfer(cap1xxx_i2c_client->adapter, msg, 2);
        if (ret < 0) {
            pr_err("%s client transfer failed, i2cAddr(0x%x) regAddr(0x%x) ret(%d)\n", __func__,
                    cap1xxx_i2c_client->addr, addr, ret);
            return ret;
        }
    } else
        pr_err("%s no client\n", __func__);

    return 0;
}

static int cap1xxx_write_reg(u8 addr, u8 para)
{
    struct i2c_msg msg[1];
    u8 data[2];
    int ret;

    data[0] = addr;
    data[1] = para;

    if (cap1xxx_i2c_client != NULL) {
        msg[0].addr = cap1xxx_i2c_client->addr;
        msg[0].flags = 0;
        msg[0].buf = data;
        msg[0].len = ARRAY_SIZE(data);
        ret = i2c_transfer(cap1xxx_i2c_client->adapter, msg, 1);
        if (ret < 0) {
            pr_err("%s client transfer failed, ret(%d)\n", __func__, ret);
            return ret;
        }
    } else
        pr_err("%s no client\n", __func__);

    return 0;
}

#ifdef CONFIG_TOUCHKEY_KTHREAD
/* use kthread */
static int cap1xxx_touchkey_thread(void *data)
{
    struct cap1xxx_touchkey_priv *cap1xxx = data;
//  struct i2c_client *client = cap1xxx_i2c_client;;
    struct input_dev *input = cap1xxx->input_dev;
    unsigned int key_num, key_val, pressed;
    int ret;
    u8 tmpVal;
    u32 statusNew, statusOld, statusVal;

    while (!kthread_should_stop()) {
        tmpVal = 0;
        ret = cap1xxx_read_reg(TOUCH_STATUS_REG_2, &tmpVal);
        if (ret < 0) {
            pr_err("i2c read TOUCH_STATUS_REG_2 error. regVal[0x%x]\n", tmpVal);
            goto thread_out;
        }
        statusVal = (u16)(tmpVal << 6);

        ret = cap1xxx_read_reg(TOUCH_STATUS_REG_1, &tmpVal);
        if (ret < 0) {
            pr_err("i2c read TOUCH_STATUS_REG_1 error. regVal[0x%x]\n", tmpVal);
            goto thread_out;
        }
        statusVal = ((tmpVal & TOUCH_STATU_1_MASK) | statusVal ) & TOUCH_STATUS_MASK;

        /* clear INT bit */
        ret = cap1xxx_read_reg(0x00, &tmpVal);
        if (ret < 0) {
            pr_err("i2c read MainStatusControl error. regVal[0x%x]\n", tmpVal);
            goto thread_out;
        }
        tmpVal &= 0xFE;
        ret = cap1xxx_write_reg(0x00, tmpVal);
        if (ret < 0) {
            pr_err("i2c write MainStatusControl error. regVal[0x%x]\n", tmpVal);
            goto thread_out;
        }
    #if 1
        if (statusVal == cap1xxx->statusbits)
            goto thread_out;

        for (key_num = 0; key_num < CAP1XXX_MAX_KEY_COUNT; key_num++) {
            statusNew = ((statusVal >> key_num) & 1);
            statusOld = ((cap1xxx->statusbits >> key_num) & 1);

            if (statusNew != statusOld) {
                pressed = statusVal & (1 << key_num);
                key_val = cap1xxx->keyMap[key_num];
                input_report_key(input, key_val, pressed);
                input_sync(input);
                printk("[TOUCHKEY]%s CS%d KEY:%d %s\n",  __func__, key_num + 1, key_val, pressed ? "pressed" : "released");
            }
        }
        cap1xxx->statusbits = statusVal;
    #else
        if (statusVal == cap1xxx->statusbits && cap1xxx->statusbits ==0)
            goto thread_out;

        for (key_num = 0; key_num < CAP1XXX_MAX_KEY_COUNT; key_num++) {
            statusNew = ((statusVal >> key_num) & 1);
            statusOld = ((cap1xxx->statusbits >> key_num) & 1);

            if ((statusNew != statusOld) || statusNew) {
                pressed = statusVal & (1 << key_num);
                key_val = cap1xxx->keyMap[key_num];
                input_report_key(input, key_val, pressed);
                input_sync(input);
                //TOUCHKEY_DEBUG("[TOUCHKEY]%s CS%d KEY:%d %s\n",  __func__, key_num + 1, key_val, pressed ? "pressed" : "released");
            }
        }
        cap1xxx->statusbits = statusVal;
    #endif
thread_out:
        msleep_interruptible(cap1xxx->pool_interval);
    }

    __set_current_state(TASK_RUNNING);

    return 0;
}

#else
/* use interrupt */
static irqreturn_t cap1xxx_touchkey_interrupt(int irq, void *dev_id)
{
    struct cap1xxx_touchkey_priv *cap1xxx = dev_id;
//  struct i2c_client *client = cap1xxx_i2c_client;;
    struct input_dev *input = cap1xxx->input_dev;
    unsigned int key_num, key_val, pressed;
    int ret;
    u8 regVal, tmpVal;
    u32 statusNew, statusOld;

    ret = cap1xxx_read_reg(TOUCH_STATUS_REG_2, &regVal);
    if (ret < 0) {
        pr_err("i2c read TOUCH_STATUS_REG_2 error [%d]\n", regVal);
        goto out;
    }
    regVal <<= 6;

    ret = cap1xxx_read_reg(TOUCH_STATUS_REG_1, &tmpVal);
    if (ret < 0) {
        pr_err("i2c read TOUCH_STATUS_REG_1 error [%d]\n", tmpVal);
        goto out;
    }

    regVal = ((tmpVal & TOUCH_STATU_1_MASK) | regVal ) & TOUCH_STATUS_MASK;

#if 1
    for (key_num = 0; key_num < CAP1XXX_MAX_KEY_COUNT; key_num++) {
        statusNew = ((regVal >> key_num) & 1);
        statusOld = ((cap1xxx->statusbits >> key_num) & 1);

        if (statusNew != statusOld) {
            pressed = regVal & (1 << key_num);
            key_val = cap1xxx->keyMap[key_num];
            input_report_key(input, key_val, pressed);
            input_sync(input);
            printk("[TOUCHKEY]%s key %d %d %s\n",  __func__, key_num, key_val,
                        pressed ? "pressed" : "released");
        }
    }
    cap1xxx->statusbits = regVal;
#else
    /* use old press bit to figure out which bit changed */
    key_num = ffs(regVal ^ cap1xxx->statusbits) - 1;
    pressed = regVal & (1 << key_num);
    cap1xxx->statusbits = regVal;

    key_val = cap1xxx->keyMap[key_num];

    input_event(input, EV_MSC, MSC_SCAN, key_num);
    input_report_key(input, key_val, pressed);
    input_sync(input);

    TOUCHKEY_DEBUG("[TOUCHKEY]%s key %d %d %s\n",  __func__, key_num, key_val,
        pressed ? "pressed" : "released");
#endif

out:
    return IRQ_HANDLED;
}
#endif

static int cap1xxx_check(void)
{
    u8  regVal = 0, timeout = 10;
    int ret = -ENODEV;

    /* check cap1xxx chip */
    do {
        ret = cap1xxx_read_reg(0xFD, &regVal);
        timeout--;
    } while((ret < 0) && (timeout != 0));

    if (!timeout || (regVal != CAPxxxx_CHIP_ID)) {
        pr_err("Touch Key CAP1XXX is not found, regVal[0x%x].\n", regVal);
        return -ENODEV;
    }

    TOUCHKEY_DEBUG("[TOUCHKEY]%s cap1xxx chip id:0x%x, trycount:%d.\n",  __func__, regVal, 5-timeout);

    return 0;
}

static int cap1xxx_touchkey_i2c_probe(struct i2c_client *client,
                                  const struct i2c_device_id *id)
{
    int ret = 0;

    TOUCHKEY_DEBUG("[TOUCHKEY]%s\n", __func__);



    if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
        pr_err("probe: need I2C_FUNC_I2C\n");
        return -ENODEV;
    }

    if (cap1xxx_check()) {
        ret = -ENODEV;
    }

    return ret;
}

static int  cap1xxx_touchkey_i2c_remove(struct i2c_client *client)
{
    TOUCHKEY_DEBUG("[TOUCHKEY]%s\n", __func__);
    return 0;
}

#ifdef CONFIG_PM
static int  cap1xxx_touchkey_i2c_suspend(struct device *dev)
{
    TOUCHKEY_DEBUG("[TOUCHKEY]%s\n", __func__);

    return 0;
}

static int  cap1xxx_touchkey_i2c_resume(struct device *dev)
{
    TOUCHKEY_DEBUG("[TOUCHKEY]%s\n", __func__);

    return 0;
}
#endif


//MODULE_DEVICE_TABLE(of, flex_match_table);

static int cap1xxx_dt_parse(struct platform_device *pdev)
{
    struct device_node *cap1xxx_node = pdev->dev.of_node;
    struct device_node *child;
    struct i2c_board_info board_info;
    struct i2c_adapter *adapter;
    struct i2c_client *client;
    //struct cap1xxx_touchkey_priv *cap1xxx = tHandle;
    int err;
    int addr, irq;
    int bus_type = -1;
    const char *str;

    TOUCHKEY_DEBUG("[TOUCHKEY]%s\n", __func__);

    err = of_property_read_string(cap1xxx_node, "status", &str);
    if (err) {
        pr_info("get touchkey 'status' failed, ret:%d\n", err);
        return -EINVAL;
    }
    if (strcmp(str, "okay") && strcmp(str, "ok")) {
        /* status is not OK, do not probe it */
        pr_info("device %s status is %s, stop probe it\n",
                        cap1xxx_node->name, str);
        return -EINVAL;
    }

    /*err = of_property_read_string(cap1xxx_node, "touch_gpio_en", &str);*/
    /*if (err) {*/
        /*pr_info("get touchkey 'touch_en' failed, ret:%d\n", err);*/
        /*return -EINVAL;*/
    /*} else {*/
        /*touch_gpio_en = of_get_named_gpio_flags(cap1xxx_node, "touch_gpio_en", 0, NULL);*/
        /*pr_debug("touch_gpio_en gpio is %d\n", touch_gpio_en);*/
    /*}*/

    for_each_child_of_node(cap1xxx_node, child) {
        pr_info("%s, child name:%s\n", __func__, child->name);

        err = of_property_read_string(child, "status", &str);
        if (err) {
            pr_info("get 'status' failed, ret:%d\n", err);
            continue;
        }
        if (strcmp(str, "okay") && strcmp(str, "ok")) {
            /* status is not OK, do not probe it */
            pr_info("device %s status is %s, stop probe it\n",
                        child->name, str);
            continue;
        }

        err = of_property_read_u32(child, "i2c_bus", &bus_type);
        if (err) {
            pr_info("get 'i2c_bus' failed, ret:%d\n", err);
            continue;
        }

        memset(&board_info, 0, sizeof(board_info));
        adapter = i2c_get_adapter(bus_type);
        if (!adapter)
            pr_info("wrong i2c adapter:%d\n", bus_type);


        err = of_property_read_u32(child, "reg", &addr);
        if (err)
        {
            pr_info("get 'reg' failed, ret:%d\n", err);
            continue;
        }

        err = of_property_read_string(child, "compatible", &str);
        if (err) {
            pr_info("get 'compatible' failed, ret:%d\n", err);
            continue;
        }

        err = of_property_read_u32(child, "interrupt", &irq);
        if (err)
        {
            pr_info("get 'interrupt' failed, ret:%d\n", err);
            continue;
        }

        strncpy(board_info.type, str, I2C_NAME_SIZE);
        board_info.addr = addr;
        board_info.of_node = child;     /* for device driver */
        board_info.irq = irq;

        client = i2c_new_device(adapter, &board_info);

        if (!client) {
            pr_info("%s, allocate i2c_client failed\n", __func__);
            continue;
        }

        cap1xxx_i2c_client = client;

        pr_info("%s: adapter:%d, addr:0x%x, node name:%s, type:%s\n",
                "Allocate new i2c device",
                bus_type, addr, child->name, str);
    }

    return 0;
}

static int cap1xxx_regs_init(void)
{
    const struct cap1xxx_init_register *reg;
    int ret = -ENODEV, i;

    /* Set up init register */
    for (i = 0; i < ARRAY_SIZE(init_reg_table); i++) {
        reg = &init_reg_table[i];
        ret = cap1xxx_write_reg(reg->addr, reg->val);
        if (ret < 0)
            return ret;
    }

    TOUCHKEY_DEBUG("[TOUCHKEY]%s cap1xxx registers init success.\n",  __func__);

    return 0;
}

static int cap1xxx_event_init(struct cap1xxx_touchkey_priv *cap1xxx)
{
    struct input_dev *input_dev;
    int i, ret = 0;

    input_dev = input_allocate_device();
    if (!input_dev) {
        pr_err("Failed to allocate input_dev\n");
        ret = -ENOMEM;
    }

    cap1xxx->input_dev = input_dev;
    cap1xxx->keyMap = pebble_mini_keyMap;
    cap1xxx->keySize = ARRAY_SIZE(pebble_mini_keyMap);

    input_set_drvdata(input_dev, cap1xxx);
    input_dev->name = "MICROCHIP CAP1XXX Touchkey";
    input_dev->id.bustype = BUS_I2C;
//  input_dev->dev.parent = pdev->dev;
    input_dev->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_REP);
    input_dev->keycode = cap1xxx->keyMap;
    input_dev->keycodesize = sizeof(cap1xxx->keyMap[0]);
    input_dev->keycodemax = cap1xxx->keySize;

    for (i = 0; i < cap1xxx->keySize; i++) {
        input_set_capability(input_dev, EV_KEY, cap1xxx->keyMap[i]);
    }


    ret = input_register_device(input_dev);
    if (ret) {
        input_free_device(input_dev);
    }

    input_dev->rep[REP_DELAY] = 1000;
    input_dev->rep[REP_PERIOD] = 1000;

    return ret;
}

static int  cap1xxx_touchkey_probe(struct platform_device *pdev)
{
    struct cap1xxx_touchkey_priv *cap1xxx = NULL;
	struct gpio_desc *desc = NULL;
    int ret = 0;

    TOUCHKEY_DEBUG("[TOUCHKEY]%s\n", __func__);

    cap1xxx = kzalloc(sizeof(struct cap1xxx_touchkey_priv), GFP_KERNEL);
    if (!cap1xxx) {
        pr_err("Failed to allocate memory\n");
        ret = -ENOMEM;
        goto err_free_mem;
    }

    tHandle = cap1xxx;

    if (cap1xxx_dt_parse(pdev)) {
        pr_err("cap1xxx dt parse error %s\n", __func__);
        goto err_dt_parse;
    }

	desc = devm_gpiod_get(&pdev->dev, "ck", GPIOD_OUT_LOW);
    if (IS_ERR(desc)) {
        return PTR_ERR(desc);
    }
    gpiod_set_value(desc, 0);

    if (i2c_add_driver(&cap1xxx_i2c_driver)) {
        pr_err("add i2c driver error %s\n", __func__);
        goto err_i2c_add;
    }

    if (cap1xxx_regs_init()) {
        pr_err("cap1xxx regs init error %s\n", __func__);
        goto err_regs_init;
    }

    if (cap1xxx_event_init(cap1xxx)) {
        pr_err("cap1xxx event init error %s\n", __func__);
        goto err_regs_init;
    }

#ifdef CONFIG_TOUCHKEY_KTHREAD
    cap1xxx->pool_interval = TOUCHKEY_POOL_INTERVAL;
    cap1xxx->handler = kthread_run(cap1xxx_touchkey_thread, cap1xxx, "touchkey_kthread");
#else
    ret = request_threaded_irq(cap1xxx_i2c_client->irq, NULL,
                    cap1xxx_touchkey_interrupt,
                    IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING | IRQF_ONESHOT,
                    cap1xxx_i2c_client->dev.driver->name, cap1xxx);
    if (ret) {
        pr_err("Failed to register interrupt:%d, ret:%d\n", cap1xxx_i2c_client->irq, ret);
        goto err_regs_init;
    }
#endif
    pr_info("Touch Key Chip CAP1XXX init success.\n");

    return 0;

err_regs_init:
    i2c_del_driver(&cap1xxx_i2c_driver);
err_i2c_add:
    i2c_unregister_device(cap1xxx_i2c_client);
err_dt_parse:
    kfree(cap1xxx);
err_free_mem:
    return ret;
}

static int cap1xxx_touchkey_remove(struct platform_device *pdev)
{
    struct cap1xxx_touchkey_priv *cap1xxx = tHandle;

#ifdef CONFIG_TOUCHKEY_KTHREAD
    if (cap1xxx && cap1xxx->handler)
        kthread_stop(cap1xxx->handler);
#else
    if (cap1xxx_i2c_client && cap1xxx)
        free_irq(cap1xxx_i2c_client->irq, cap1xxx);
#endif

    i2c_del_driver(&cap1xxx_i2c_driver);

    if (cap1xxx_i2c_client)
        i2c_unregister_device(cap1xxx_i2c_client);

    if (cap1xxx)
        kfree(cap1xxx);

    return 0;
}

static const struct of_device_id cap1xxx_dt_match[] = {
    {
        .compatible = "nationalchip,touchkey",
    },
    {}
};

static struct platform_driver cap1xxx_driver = {
    .probe = cap1xxx_touchkey_probe,
    .remove = cap1xxx_touchkey_remove,
    .driver = {
        .name = "cap1xxx",
        .owner = THIS_MODULE,
        .of_match_table = cap1xxx_dt_match,
    },
};

static int __init cap1xxx_module_init(void)
{
    TOUCHKEY_DEBUG("### %s,%d\n", __func__, __LINE__);

    if (platform_driver_register(&cap1xxx_driver)) {
        pr_err("failed to register driver\n");
        return -ENODEV;
    }

    return 0;
}

static void __exit cap1xxx_module_exit(void)
{
    TOUCHKEY_DEBUG("### %s,%d\n", __func__, __LINE__);

    platform_driver_unregister(&cap1xxx_driver);
}


module_param(debug_enable, int, 0644);

module_init(cap1xxx_module_init);
module_exit(cap1xxx_module_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Touch Key driver for MICROCHIP CAP1XXX Chip");

