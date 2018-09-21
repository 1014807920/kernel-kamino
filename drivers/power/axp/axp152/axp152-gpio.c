#include "axp152-gpio.h"

#include <linux/io.h>
#include <linux/clk.h>
#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/pinctrl/consumer.h>
#include <linux/pinctrl/machine.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/pinctrl/pinconf-generic.h>
#include <linux/pinctrl/pinmux.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/slab.h>
#include <linux/mfd/core.h>
#include <linux/seq_file.h>
#include <linux/i2c.h>
#include "../../../pinctrl/core.h"
#include "../axp-core.h"
#include "../axp-gpio.h"

static struct axp_gpio_irqchip *axp152_gpio_irqchip;
static int axp152_pmu_num;

static int axp152_gpio_get_data(struct axp_dev *axp_dev, int gpio)
{
	u8 ret;
	struct axp_regmap *map;

	map = axp_dev->regmap;
	if (NULL == map) {
		pr_err("%s: axp regmap is null\n", __func__);
		return -ENXIO;
	}

	switch (gpio) {
	case 0:
		axp_regmap_read(map, AXP_GPIO_STATE, &ret); ret &= 0x1; break;
	case 1:
		axp_regmap_read(map, AXP_GPIO_STATE, &ret); ret &= 0x2; break;
	case 2:
		axp_regmap_read(map, AXP_GPIO_STATE, &ret); ret &= 0x4; break;
	case 3:
		axp_regmap_read(map, AXP_GPIO_STATE, &ret); ret &= 0x8; break;
	default:
		pr_err("This IO is not an input\n");
		return -ENXIO;
	}

	return ret;
}

static int axp152_gpio_set_data(struct axp_dev *axp_dev, int gpio, int value)
{
	struct axp_regmap *map;

	if ((0 <= gpio) && (3 >= gpio)) {
		map = axp_dev->regmap;
		if (NULL == map) {
			pr_err("%s: %d axp regmap is null\n",
					__func__, __LINE__);
			return -ENXIO;
		}
	} else {
		return -ENXIO;
	}

	if (value) {
		/* high */
		switch (gpio) {
		case 0:
			return axp_regmap_update_sync(map,
				AXP_GPIO0_CFG, 0x01, 0x07);
		case 1:
			return axp_regmap_update_sync(map,
				AXP_GPIO1_CFG, 0x01, 0x07);
		case 2:
			return axp_regmap_update_sync(map,
				AXP_GPIO2_CFG, 0x01, 0x07);
		case 3:
			return axp_regmap_update_sync(map,
				AXP_GPIO3_CFG, 0x01, 0x07);
		default:
			break;
		}
	} else {
		/* low */
		switch (gpio) {
		case 0:
			return axp_regmap_update_sync(map,
				AXP_GPIO0_CFG, 0x0, 0x07);
		case 1:
			return axp_regmap_update_sync(map,
				AXP_GPIO1_CFG, 0x0, 0x07);
		case 2:
			return axp_regmap_update_sync(map,
				AXP_GPIO2_CFG, 0x0, 0x07);
		case 3:
			return axp_regmap_update_sync(map,
				AXP_GPIO3_CFG, 0x0, 0x07);
		default:
			break;
		}
	}

	return -ENXIO;
}

static int axp152_pmx_set(struct axp_dev *axp_dev, int gpio, int mux)
{
	struct axp_regmap *map;

	map = axp_dev->regmap;
	if (NULL == map) {
		pr_err("%s: %d axp regmap is null\n", __func__, __LINE__);
		return -ENXIO;
	}

	if (mux == 1) {
		/* output */
		switch (gpio) {
		case 0:
			return axp_regmap_clr_bits_sync(map,
				AXP_GPIO0_CFG, 0x06);
		case 1:
			return axp_regmap_clr_bits_sync(map,
				AXP_GPIO1_CFG, 0x06);
		case 2:
			return axp_regmap_clr_bits_sync(map,
				AXP_GPIO2_CFG, 0x06);
		case 3:
			return axp_regmap_clr_bits_sync(map,
				AXP_GPIO3_CFG, 0x06);
		default:
			return -ENXIO;
		}
	} else if (mux == 0) {
		/* input */
		switch (gpio) {
		case 0:
			axp_regmap_clr_bits_sync(map,
				AXP_GPIO0_CFG, 0x04);
			return axp_regmap_set_bits_sync(map,
				AXP_GPIO0_CFG, 0x03);
		case 1:
			axp_regmap_clr_bits_sync(map,
				AXP_GPIO1_CFG, 0x04);
			return axp_regmap_set_bits_sync(map,
				AXP_GPIO1_CFG, 0x03);
		case 2:
			axp_regmap_clr_bits_sync(map,
				AXP_GPIO2_CFG, 0x04);
			return axp_regmap_set_bits_sync(map,
				AXP_GPIO2_CFG, 0x03);
		case 3:
			axp_regmap_clr_bits_sync(map,
				AXP_GPIO3_CFG, 0x04);
			return axp_regmap_set_bits_sync(map,
				AXP_GPIO3_CFG, 0x03);
		default:
			pr_err("This IO can not config as input!");
			return -ENXIO;
		}
	}

	return -ENXIO;
}

static int axp152_pmx_get(struct axp_dev *axp_dev, int gpio)
{
	u8 ret;
	struct axp_regmap *map;

	map = axp_dev->regmap;
	if (NULL == map) {
		pr_err("%s: %d axp regmap is null\n",
				__func__, __LINE__);
		return -ENXIO;
	}

	switch (gpio) {
	case 0:
		axp_regmap_read(map, AXP_GPIO0_CFG, &ret); break;
	case 1:
		axp_regmap_read(map, AXP_GPIO1_CFG, &ret); break;
	case 2:
		axp_regmap_read(map, AXP_GPIO2_CFG, &ret); break;
	case 3:
		axp_regmap_read(map, AXP_GPIO3_CFG, &ret); break;
	default:
		return -ENXIO;
	}

	if (0 == (ret & 0x06))
		return 1;
	else if (0x03 == (ret & 0x07))
		return 0;
	else
		return -ENXIO;
}

static const struct axp_desc_pin axp152_pins[] = {
	AXP_PIN_DESC(AXP_PINCTRL_GPIO(0),
			 AXP_FUNCTION(0x0, "gpio_in"),
			 AXP_FUNCTION(0x1, "gpio_out"),
			 AXP_FUNCTION_IRQ(1)),
	AXP_PIN_DESC(AXP_PINCTRL_GPIO(1),
			 AXP_FUNCTION(0x0, "gpio_in"),
			 AXP_FUNCTION(0x1, "gpio_out"),
			 AXP_FUNCTION_IRQ(1)),
	AXP_PIN_DESC(AXP_PINCTRL_GPIO(2),
			 AXP_FUNCTION(0x0, "gpio_in"),
			 AXP_FUNCTION(0x1, "gpio_out"),
			 AXP_FUNCTION_IRQ(1)),
	AXP_PIN_DESC(AXP_PINCTRL_GPIO(3),
			 AXP_FUNCTION(0x0, "gpio_in"),
			 AXP_FUNCTION(0x1, "gpio_out"),
			 AXP_FUNCTION_IRQ(1)),
};

static struct axp_pinctrl_desc axp152_pinctrl_pins_desc = {
	.pins  = axp152_pins,
	.npins = ARRAY_SIZE(axp152_pins),
};

static struct axp_gpio_ops axp152_gpio_ops = {
	.gpio_get_data = axp152_gpio_get_data,
	.gpio_set_data = axp152_gpio_set_data,
	.pmx_set = axp152_pmx_set,
	.pmx_get = axp152_pmx_get,
};

static int axp152_gpio_irq_request(int gpio_no,
				u32 (*handler)(int, void *), void *data)
{
	int gpio = gpio_no - AXP_PIN_BASE;

	if (!axp_gpio_irq_valid(&axp152_pinctrl_pins_desc, gpio)) {
		pr_err("axp gpio%d irq is not valid\n", gpio);
		return -1;
	}

	axp152_gpio_irqchip[gpio].gpio_no = gpio;
	axp152_gpio_irqchip[gpio].handler = handler;
	axp152_gpio_irqchip[gpio].data = data;

	return 0;
}

static int axp152_gpio_irq_free(int gpio_no)
{
	int gpio = gpio_no - AXP_PIN_BASE;

	if (!axp_gpio_irq_valid(&axp152_pinctrl_pins_desc, gpio)) {
		pr_err("axp gpio%d irq is not valid\n", gpio);
		return -1;
	}

	axp152_gpio_irqchip[gpio].gpio_no = 0;
	axp152_gpio_irqchip[gpio].handler = NULL;
	axp152_gpio_irqchip[gpio].data = NULL;

	return 0;
}

static int axp152_gpio_irq_ack(int gpio_no)
{
	struct axp_dev *cur_axp_dev = get_pmu_cur_dev(axp152_pmu_num);
	struct axp_regmap *map = cur_axp_dev->regmap;
	int gpio = gpio_no - AXP_PIN_BASE;

	if (!axp_gpio_irq_valid(&axp152_pinctrl_pins_desc, gpio)) {
		pr_err("axp gpio%d irq is not valid\n", gpio);
		return -1;
	}

	axp_regmap_write(map, AXP_GPIO0123_INTSTA, 0x1 << gpio);

	return 0;
}

static int axp152_gpio_irq_enable(int gpio_no)
{
	struct axp_dev *cur_axp_dev = get_pmu_cur_dev(axp152_pmu_num);
	struct axp_regmap *map = cur_axp_dev->regmap;
	int gpio = gpio_no - AXP_PIN_BASE;
	u8 regval;

	if (!axp_gpio_irq_valid(&axp152_pinctrl_pins_desc, gpio)) {
		pr_err("axp gpio%d irq is not valid\n", gpio);
		return -1;
	}

	axp_regmap_read(map, AXP_GPIO0123_INTEN, &regval);
	regval |= (0x1 << gpio);
	axp_regmap_write(map, AXP_GPIO0123_INTEN, regval);

	return 0;
};

static int axp152_gpio_irq_disable(int gpio_no)
{
	struct axp_dev *cur_axp_dev = get_pmu_cur_dev(axp152_pmu_num);
	struct axp_regmap *map = cur_axp_dev->regmap;
	int gpio = gpio_no - AXP_PIN_BASE;
	u8 regval;

	if (!axp_gpio_irq_valid(&axp152_pinctrl_pins_desc, gpio)) {
		pr_err("axp gpio%d irq is not valid\n", gpio);
		return -1;
	}

	axp_regmap_read(map, AXP_GPIO0123_INTEN, &regval);
	regval &= ~(0x1 << gpio);
	axp_regmap_write(map, AXP_GPIO0123_INTEN, regval);

	return 0;
};

static int axp152_gpio_set_type(int gpio_no, unsigned long type)
{
	struct axp_dev *cur_axp_dev = get_pmu_cur_dev(axp152_pmu_num);
	struct axp_regmap *map = cur_axp_dev->regmap;
	int gpio = gpio_no - AXP_PIN_BASE;
	int reg = 0;
	u8 regval, mode = 0;

	if (!axp_gpio_irq_valid(&axp152_pinctrl_pins_desc, gpio)) {
		pr_err("axp gpio%d irq is not valid\n", gpio);
		return -1;
	}

	switch (type) {
	case AXP_GPIO_IRQF_TRIGGER_RISING:
		mode = AXP_GPIO_IRQ_EDGE_RISING;
		break;
	case AXP_GPIO_IRQF_TRIGGER_FALLING:
		mode = AXP_GPIO_IRQ_EDGE_FALLING;
		break;
	case AXP_GPIO_IRQF_TRIGGER_RISING | AXP_GPIO_IRQF_TRIGGER_FALLING:
		mode = AXP_GPIO_IRQ_EDGE_RISING | AXP_GPIO_IRQ_EDGE_FALLING;
		break;
	default:
		return -EINVAL;
	}

	if (gpio == 0)
		reg = AXP_GPIO0_CFG;
	else if (gpio == 1)
		reg = AXP_GPIO1_CFG;
	else if (gpio == 2)
		reg = AXP_GPIO2_CFG;
	else if (gpio == 3)
		reg = AXP_GPIO3_CFG;

	axp_regmap_read(map, reg, &regval);
	regval &= ~AXP_GPIO_EDGE_TRIG_MASK;
	regval |= mode;
	regval &= ~AXP_GPIO_INPUT_TRIG_MASK;
	regval |= 0x3; /* digital input */
	axp_regmap_write(map, reg, regval);

	return 0;
};

struct axp_gpio_irq_ops axp152_gpio_irq_ops = {
	.irq_request  = axp152_gpio_irq_request,
	.irq_free     = axp152_gpio_irq_free,
	.irq_ack      = axp152_gpio_irq_ack,
	.irq_enable   = axp152_gpio_irq_enable,
	.irq_disable  = axp152_gpio_irq_disable,
	.irq_set_type = axp152_gpio_set_type,
};

/* do nothing, just enabled for standby use */
static irqreturn_t axp152_gpio_isr(int irq, void *data)
{
	int gpio = 0;

	switch (irq) {
	case AXP152_IRQ_GPIO0:
		gpio = 0;
		break;
	case AXP152_IRQ_GPIO1:
		gpio = 1;
		break;
	case AXP152_IRQ_GPIO2:
		gpio = 2;
		break;
	case AXP152_IRQ_GPIO3:
		gpio = 3;
		break;
	default:
		return IRQ_NONE;
	}

	AXP_DEBUG(AXP_INT, axp152_pmu_num, "gpio%d input edge\n", gpio);

	if (axp152_gpio_irqchip[gpio].handler != NULL) {
		axp152_gpio_irqchip[gpio].handler(gpio + AXP_PIN_BASE,
				axp152_gpio_irqchip[gpio].data);
		axp152_gpio_irq_ack(gpio + AXP_PIN_BASE);
	}

	return IRQ_HANDLED;
}

static struct axp_interrupts axp152_gpio_irq[] = {
	{"gpio0", axp152_gpio_isr},
	{"gpio1", axp152_gpio_isr},
	{"gpio2", axp152_gpio_isr},
	{"gpio3", axp152_gpio_isr},
};

static int axp152_gpio_probe(struct platform_device *pdev)
{

	struct axp_dev *axp_dev = dev_get_drvdata(pdev->dev.parent);
	struct axp_pinctrl *axp152_pin;
	int i, irq, err;
    printk("[axp152]Entering %s\n",__func__);
    printk("[axp152] pointer to of_node is %p in line:%d in %s\n",
                       pdev->dev.of_node,__LINE__,__func__);
	axp152_pin = axp_pinctrl_register(&pdev->dev,
			axp_dev, &axp152_pinctrl_pins_desc, &axp152_gpio_ops);
	if (IS_ERR_OR_NULL(axp152_pin))
		goto fail;

	for (i = 0; i < ARRAY_SIZE(axp152_gpio_irq); i++) {
		irq = platform_get_irq_byname(pdev, axp152_gpio_irq[i].name);
		if (irq < 0)
			continue;

		err = axp_gpio_irq_register(axp_dev, irq,
				axp152_gpio_irq[i].isr, axp152_pin);
		if (err != 0) {
			dev_err(&pdev->dev, "failed to request %s IRQ %d: %d\n"
					, axp152_gpio_irq[i].name, irq, err);
			goto out_irq;
		}

		dev_dbg(&pdev->dev, "Requested %s IRQ %d: %d\n",
				axp152_gpio_irq[i].name, irq, err);
	}

	axp152_gpio_irqchip = kzalloc(ARRAY_SIZE(axp152_gpio_irq)
				* sizeof(struct axp_gpio_irqchip), GFP_KERNEL);
	if (IS_ERR_OR_NULL(axp152_gpio_irqchip)) {
		dev_err(&pdev->dev, "axp152_gpio_irqchip: not enough memory\n");
		i = ARRAY_SIZE(axp152_gpio_irq);
		goto out_irq;
	}

	axp152_pmu_num = axp_dev->pmu_num;
	axp_gpio_irq_ops_set(axp152_pmu_num, &axp152_gpio_irq_ops);

	platform_set_drvdata(pdev, axp152_pin);
   printk("[axp152]Quit line: %d func: %s\n",__LINE__,__func__);
	return 0;

out_irq:
	for (i = i - 1; i >= 0; i--) {
		irq = platform_get_irq_byname(pdev, axp152_gpio_irq[i].name);
		if (irq < 0)
			continue;
		axp_free_irq(axp_dev, irq);
	}
fail:
	return -1;
}

static int axp152_gpio_remove(struct platform_device *pdev)
{
	struct axp_dev *axp_dev = dev_get_drvdata(pdev->dev.parent);
	struct axp_pinctrl *axp152_pin = platform_get_drvdata(pdev);
	int i, irq;

	kfree(axp152_gpio_irqchip);
	axp152_gpio_irqchip = NULL;

	for (i = 0; i < ARRAY_SIZE(axp152_gpio_irq); i++) {
		irq = platform_get_irq_byname(pdev, axp152_gpio_irq[i].name);
		if (irq < 0)
			continue;
		axp_free_irq(axp_dev, irq);
	}

	return axp_pinctrl_unregister(axp152_pin);
}

static const struct of_device_id axp152_gpio_dt_ids[] = {
	{ .compatible = "axp1527-gpio", },
	{},
};
MODULE_DEVICE_TABLE(of, axp152_gpio_dt_ids);

static struct platform_driver axp152_gpio_driver = {
	.driver = {
		.name = "axp152-gpio",
		.of_match_table = axp152_gpio_dt_ids,
	},
	.probe  = axp152_gpio_probe,
	.remove = axp152_gpio_remove,
};

static int __init axp152_gpio_initcall(void)
{
	int ret;

	ret = platform_driver_register(&axp152_gpio_driver);
	if (IS_ERR_VALUE(ret)) {
		pr_err("%s: failed, errno %d\n", __func__, ret);
		return -EINVAL;
	}

	return 0;
}
late_initcall(axp152_gpio_initcall);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("gpio Driver for axp152");
MODULE_AUTHOR("Qin <qinyongshen@allwinnertech.com>");