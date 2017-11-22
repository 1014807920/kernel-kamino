#include <linux/io.h>
#include <linux/input.h>
#include <linux/input/matrix_keypad.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/timer.h>


struct key_map{
	u32 key_code;
	u32 key_gpio;
};

/* Structure representing various run-time entities */
struct nationalchip_kp {
	void __iomem *base;
	int irq;
	struct input_dev *input_dev;
	unsigned long last_state[2];
	int enable;
	int key_num;
	struct key_map *map;
	struct timer_list timer;
};

struct nationalchip_kp *key_p;

static int nationalchip_get_keycode(struct nationalchip_kp *kp, int bit_nr)
{
	int i = 0;
	int key_code = -1;
	struct key_map *map = kp->map;

	for (; i < kp->key_num; i++, map++)
		if (map->key_gpio == bit_nr){
			key_code = map->key_code;
			break;
		}
	return key_code;
}

static void nationalchip_keypress_timer(unsigned long drv)
{
	struct nationalchip_kp *kp = (struct nationalchip_kp *)drv;
	unsigned long state, bit_nr, key_press, change;

	state = readl(kp->base) & kp->enable;
	change = state ^ kp->last_state[0];
	if (state ^ kp->last_state[0]) {
		kp->last_state[0] = state;

		for_each_set_bit(bit_nr, &change, 32) {
			key_press = state & BIT(bit_nr);
			input_report_key(kp->input_dev, nationalchip_get_keycode(kp, bit_nr), !key_press);
			input_sync(kp->input_dev);
		}
	}

	mod_timer(&kp->timer, jiffies + msecs_to_jiffies(10));
}

static int nationalchip_kp_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct device_node *np;
	struct nationalchip_kp *kp;
	struct input_dev *input_dev;
	struct resource *res;
	struct key_map *map;
	int error;
	u32 data;

	kp = devm_kzalloc(&pdev->dev, sizeof(*kp), GFP_KERNEL);
	if (!kp)
		return -ENOMEM;

	input_dev = devm_input_allocate_device(&pdev->dev);
	if (!input_dev) {
		dev_err(&pdev->dev, "failed to allocate the input device\n");
		return -ENOMEM;
	}

	__set_bit(EV_KEY, input_dev->evbit);

	/* Enable auto repeat feature of Linux input subsystem */
	if (of_property_read_bool(pdev->dev.of_node, "autorepeat"))
		__set_bit(EV_REP, input_dev->evbit);

	input_dev->name = pdev->name;
	input_dev->phys = "keypad/input0";
	input_dev->dev.parent = &pdev->dev;

	input_dev->id.bustype = BUS_HOST;
	input_dev->id.vendor = 0x0001;
	input_dev->id.product = 0x0001;
	input_dev->id.version = 0x0100;

	input_set_drvdata(input_dev, kp);

	kp->input_dev = input_dev;

	platform_set_drvdata(pdev, kp);

	/* Get the KEYPAD base address */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "Missing keypad base address resource\n");
		return -ENODEV;
	}

	kp->base = ioremap(res->start, res->end - res->start + 1);
	if (IS_ERR(kp->base))
		return PTR_ERR(kp->base);

	for_each_child_of_node(node, np) {
		kp->key_num++;
	}

	map = devm_kzalloc(&pdev->dev, kp->key_num * sizeof(struct key_map), GFP_KERNEL);
	if (!map) {
		dev_err(&(pdev->dev), "failed to allocate memory for driver's "
				"kp->map\n");
		return -ENOMEM;
	}
	kp->map = map;
	for_each_child_of_node(node, np) {
		if (of_property_read_u32(np, "key_code", &map->key_code))
			return -1;
		if (of_property_read_u32(np, "key_gpio", &map->key_gpio))
			return -1;
		kp->enable |= 1 << map->key_gpio;
		__set_bit(map->key_code, input_dev->keybit);
		map++;
	}
	data  = readl(kp->base);
	data &= kp->enable;
	kp->last_state[0] = data;

	error = input_register_device(input_dev);
	if (error) {
		dev_err(&pdev->dev, "failed to register input device\n");
		return error;
	}

	setup_timer(&kp->timer, nationalchip_keypress_timer, (unsigned long)kp);
	mod_timer(&kp->timer, jiffies + msecs_to_jiffies(10));

	key_p = kp;

	return 0;
}

static const struct of_device_id nationalchip_kp_of_match[] = {
	{ .compatible = "nationalchip,LEO-keypad" },
	{ },
};
MODULE_DEVICE_TABLE(of, nationalchip_kp_of_match);

static struct platform_driver nationalchip_kp_device_driver = {
	.probe		= nationalchip_kp_probe,
	.driver		= {
		.name	= "nationalchip-keypad",
		.of_match_table = of_match_ptr(nationalchip_kp_of_match),
	}
};

static int __init key_init(void)
{
	printk("Nationalchip key Driver\n");
	return platform_driver_register(&nationalchip_kp_device_driver);
}

static void __exit key_exit(void)
{
	del_timer(&key_p->timer);
	input_unregister_device(key_p->input_dev);
	platform_driver_unregister(&nationalchip_kp_device_driver);
}
module_init(key_init);
module_exit(key_exit);

MODULE_AUTHOR("liyj");
MODULE_DESCRIPTION("NationalChip Keypad Driver");
MODULE_LICENSE("GPL");
