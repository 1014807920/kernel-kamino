#include <linux/init.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/list.h>
#include <linux/spinlock.h>

#include <linux/irq.h>
#include <linux/irqdomain.h>
#include <linux/platform_device.h>

#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>

#include <linux/basic_mmio_gpio.h>

#define DRIVER_NAME	"nc-gpio"

#define NCGPIO_MAX_PORTS 1

/* include start */
#define NC_GPIO_DIRECT_OFFSET 0
#define NC_GPIO_SET_OFFSET 0x10
#define NC_GPIO_DATA_OFFSET 0x1c

struct ncgpio_port_property {
    struct device_node *node;
    const char  *name;
    unsigned int    idx;
    unsigned int    ngpio;
    unsigned int    gpio_base;
    unsigned int    irq;
    bool        irq_shared;
};

struct ncgpio_platform_data {
    struct ncgpio_port_property *properties;
    unsigned int nports;
};
/* include end */

struct ncgpio_gpio;

struct ncgpio_port {
    struct bgpio_chip   bgc;
    bool            is_registered;
    struct ncgpio_gpio   *gpio;
    unsigned int        idx;
};

struct ncgpio_gpio {
	struct device *dev;
	void __iomem *regs;
	struct ncgpio_port *ports;
	unsigned int nr_ports;
	struct irq_domain *domain;
};

static inline u32 ncgpio_read(struct ncgpio_gpio *gpio, unsigned int offset)
{
    struct bgpio_chip *bgc  = &gpio->ports[0].bgc;
    void __iomem *reg_base  = gpio->regs;

	return bgc->read_reg(reg_base + offset);
}

static inline void ncgpio_write(struct ncgpio_gpio *gpio, unsigned int offset,
                   u32 val)
{
    struct bgpio_chip *bgc  = &gpio->ports[0].bgc;
    void __iomem *reg_base  = gpio->regs;

    bgc->write_reg(reg_base + offset, val);
}

static int ncgpio_gpio_to_irq(struct gpio_chip *gc, unsigned offset)
{
    struct bgpio_chip *bgc = to_bgpio_chip(gc);
    struct ncgpio_port *port = container_of(bgc, struct ncgpio_port, bgc);
    struct ncgpio_gpio *gpio = port->gpio;

    return irq_find_mapping(gpio->domain, offset);
}
/*
 * 中断相关函数
 * */
#define NCGPIO_INTEx 0x40
#define NCGPIO_ACK 0x44			// 写1清楚中断标志
#define NCGPIO_INTSx 0x44		// 读可以获得中断状态
#define NCGPIO_INTMx 0x48		// “置 1 表示不产生中断,也不产生 INTSx 位,相当于 HIINTx、LOINTx、RINTx 和 FINTx 全为 0。”
#define NCGPIO_HIINTx 0x50
#define NCGPIO_LOINTx 0x54
#define NCGPIO_RINTx 0x58
#define NCGPIO_FINTx 0x5C

static u32 ncgpio_do_irq(struct ncgpio_gpio *gpio)
{
    u32 irq_status = readl_relaxed(gpio->regs + NCGPIO_INTSx);
    u32 ret = irq_status;

    while (irq_status) {
        int hwirq = fls(irq_status) - 1;
        int gpio_irq = irq_find_mapping(gpio->domain, hwirq);

        generic_handle_irq(gpio_irq);
        irq_status &= ~BIT(hwirq);
    }

    return ret;
}

static void ncgpio_irq_handler(struct irq_desc *desc)
{
    struct ncgpio_gpio *gpio = irq_desc_get_handler_data(desc);
    struct irq_chip *chip = irq_desc_get_chip(desc);

    ncgpio_do_irq(gpio);

	if (chip->irq_ack)
        chip->irq_ack(irq_desc_get_irq_data(desc));

}
static int ncgpio_irq_set_type(struct irq_data *d, u32 type)
{
    struct irq_chip_generic *igc = irq_data_get_irq_chip_data(d);
    struct ncgpio_gpio *gpio = igc->private;
    struct bgpio_chip *bgc = &gpio->ports[0].bgc;
	int bit = d->hwirq;
	unsigned long reg, flags;

	if (type & ~(IRQ_TYPE_EDGE_RISING | IRQ_TYPE_EDGE_FALLING |
			IRQ_TYPE_LEVEL_HIGH | IRQ_TYPE_LEVEL_LOW))
	return -EINVAL;

	spin_lock_irqsave(&bgc->lock, flags);

#define NCGPIO_MODREG(reg_offset, condition) \
	reg = ncgpio_read(gpio, (reg_offset)); \
	if (type & (condition)) \
		reg |= BIT(bit); \
	else \
		reg &= ~BIT(bit); \
	ncgpio_write(gpio, (reg_offset), reg);

	NCGPIO_MODREG(NCGPIO_HIINTx, IRQ_TYPE_LEVEL_HIGH);
	NCGPIO_MODREG(NCGPIO_LOINTx, IRQ_TYPE_LEVEL_LOW);
	NCGPIO_MODREG(NCGPIO_RINTx, IRQ_TYPE_EDGE_RISING);
	NCGPIO_MODREG(NCGPIO_FINTx, IRQ_TYPE_EDGE_FALLING);

#undef NCGPIO_MODREG

	irq_setup_alt_chip(d, type);
	spin_unlock_irqrestore(&bgc->lock, flags);
	return 0;
}

static void ncgpio_irq_enable(struct irq_data *d)
{
    struct irq_chip_generic *igc = irq_data_get_irq_chip_data(d);
    struct ncgpio_gpio *gpio = igc->private;
    struct bgpio_chip *bgc = &gpio->ports[0].bgc;
    unsigned long flags;
    u32 val;

    spin_lock_irqsave(&bgc->lock, flags);
    val = ncgpio_read(gpio, NCGPIO_INTEx);
    val |= BIT(d->hwirq);
    ncgpio_write(gpio, NCGPIO_INTEx, val);
    spin_unlock_irqrestore(&bgc->lock, flags);
}

static void ncgpio_irq_disable(struct irq_data *d)
{
    struct irq_chip_generic *igc = irq_data_get_irq_chip_data(d);
    struct ncgpio_gpio *gpio = igc->private;
    struct bgpio_chip *bgc = &gpio->ports[0].bgc;
    unsigned long flags;
    u32 val;

    spin_lock_irqsave(&bgc->lock, flags);
    val = ncgpio_read(gpio, NCGPIO_INTEx);
    val &= ~BIT(d->hwirq);
    ncgpio_write(gpio, NCGPIO_INTEx, val);
    spin_unlock_irqrestore(&bgc->lock, flags);
}

static int ncgpio_irq_reqres(struct irq_data *d)
{
    struct irq_chip_generic *igc = irq_data_get_irq_chip_data(d);
    struct ncgpio_gpio *gpio = igc->private;
    struct bgpio_chip *bgc = &gpio->ports[0].bgc;

    if (gpiochip_lock_as_irq(&bgc->gc, irqd_to_hwirq(d))) {
        dev_err(gpio->dev, "unable to lock HW IRQ %lu for IRQ\n",
            irqd_to_hwirq(d));
        return -EINVAL;
    }
    return 0;
}

static void ncgpio_irq_relres(struct irq_data *d)
{
    struct irq_chip_generic *igc = irq_data_get_irq_chip_data(d);
    struct ncgpio_gpio *gpio = igc->private;
    struct bgpio_chip *bgc = &gpio->ports[0].bgc;

    gpiochip_unlock_as_irq(&bgc->gc, irqd_to_hwirq(d));
}

static void ncgpio_configure_irqs(struct ncgpio_gpio *gpio,
                 struct ncgpio_port *port,
                 struct ncgpio_port_property *pp)
{
    struct gpio_chip *gc = &port->bgc.gc;
    struct device_node *node = pp->node;
    struct irq_chip_generic *irq_gc = NULL;
    unsigned int hwirq, ngpio = gc->ngpio;
    struct irq_chip_type *ct;
    int err, i;

    gpio->domain = irq_domain_add_linear(node, ngpio,
                         &irq_generic_chip_ops, gpio);
    if (!gpio->domain)
        return;

    err = irq_alloc_domain_generic_chips(gpio->domain, ngpio, 2,
                         "gpio-ncgpio", handle_level_irq,
                         IRQ_NOREQUEST, 0,
                         IRQ_GC_INIT_NESTED_LOCK);
    if (err) {
        dev_info(gpio->dev, "irq_alloc_domain_generic_chips failed\n");
        irq_domain_remove(gpio->domain);
        gpio->domain = NULL;
        return;
    }

    irq_gc = irq_get_domain_generic_chip(gpio->domain, 0);
    if (!irq_gc) {
        irq_domain_remove(gpio->domain);
        gpio->domain = NULL;
        return;
    }

    irq_gc->reg_base = gpio->regs;
    irq_gc->private = gpio;

    for (i = 0; i < 2; i++) {
        ct = &irq_gc->chip_types[i];
        ct->chip.irq_ack = irq_gc_ack_set_bit;
        ct->chip.irq_mask = irq_gc_mask_set_bit;
        ct->chip.irq_unmask = irq_gc_mask_clr_bit;
        ct->chip.irq_set_type = ncgpio_irq_set_type;
        ct->chip.irq_enable = ncgpio_irq_enable;
        ct->chip.irq_disable = ncgpio_irq_disable;
        ct->chip.irq_request_resources = ncgpio_irq_reqres;
        ct->chip.irq_release_resources = ncgpio_irq_relres;
        ct->regs.ack = NCGPIO_ACK;
        ct->regs.mask = NCGPIO_INTMx;
    }

    irq_gc->chip_types[0].type = IRQ_TYPE_LEVEL_MASK;
    irq_gc->chip_types[1].type = IRQ_TYPE_EDGE_BOTH;
    irq_gc->chip_types[1].handler = handle_edge_irq;

    if (!pp->irq_shared) {
        irq_set_chained_handler_and_data(pp->irq, ncgpio_irq_handler,
                         gpio);
    } else {
		pr_err("irq shared not support\n");
		BUG();
	}

    for (hwirq = 0 ; hwirq < ngpio ; hwirq++)
        irq_create_mapping(gpio->domain, hwirq);

    port->bgc.gc.to_irq = ncgpio_gpio_to_irq;
}
/*******************************/

static struct ncgpio_platform_data *
ncgpio_get_pdata_of(struct device *dev)
{
    struct device_node *node, *port_np;
    struct ncgpio_platform_data *pdata;
    struct ncgpio_port_property *pp;
    int nports;
    int i;

    node = dev->of_node;
    if (!IS_ENABLED(CONFIG_OF_GPIO) || !node)
        return ERR_PTR(-ENODEV);

    nports = of_get_child_count(node);
    if (nports == 0)
        return ERR_PTR(-ENODEV);

    pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
    if (!pdata)
        return ERR_PTR(-ENOMEM);

    pdata->properties = devm_kcalloc(dev, nports, sizeof(*pp), GFP_KERNEL);
    if (!pdata->properties)
        return ERR_PTR(-ENOMEM);

    pdata->nports = nports;

    i = 0;
    for_each_child_of_node(node, port_np) {
        pp = &pdata->properties[i++];
        pp->node = port_np;

        if (of_property_read_u32(port_np, "reg", &pp->idx) ||
            pp->idx >= NCGPIO_MAX_PORTS) {
            dev_err(dev, "missing/invalid port index for %s\n",
                port_np->full_name);
            return ERR_PTR(-EINVAL);
        }

        if (of_property_read_u32(port_np, "nc,nr-gpios",
                     &pp->ngpio)) {
            dev_info(dev, "failed to get number of gpios for %s\n",
                 port_np->full_name);
            pp->ngpio = 32;
        }

        if (of_property_read_u32(port_np, "nc,base-gpios",
                     &pp->gpio_base)) {
            dev_info(dev, "failed to get number of gpios for %s\n",
                 port_np->full_name);
			pp->gpio_base   = -1;
        }

		if (of_property_read_bool(port_np, "interrupt-controller")) {
            pp->irq = irq_of_parse_and_map(port_np, 0);
            if (!pp->irq) {
                dev_warn(dev, "no irq for bank %s\n",
                     port_np->full_name);
            }
        }

        pp->irq_shared  = false;
        pp->name    = port_np->full_name;
    }

    return pdata;
}

static int ncgpio_add_port(struct ncgpio_gpio *gpio,
                   struct ncgpio_port_property *pp,
                   unsigned int offs)
{
    struct ncgpio_port *port;
    struct bgpio_chip   *bgc;
    void __iomem *dat, *set, *dirout;
    int err;

    port = &gpio->ports[offs];
    port->gpio = gpio;
    port->idx = pp->idx;
	bgc = &port->bgc;

    dat = gpio->regs + NC_GPIO_DATA_OFFSET;
    set = gpio->regs + NC_GPIO_SET_OFFSET;
    dirout = gpio->regs + NC_GPIO_DIRECT_OFFSET;

    err = bgpio_init(bgc, gpio->dev, 4, dat, set, NULL, dirout,
             NULL, false);
    if (err) {
        dev_err(gpio->dev, "failed to init gpio chip for %s\n",
            pp->name);
        return err;
    }

    bgc->gc.request = gpiochip_generic_request;
    bgc->gc.free = gpiochip_generic_free;

#ifdef CONFIG_OF_GPIO
    bgc->gc.of_node = pp->node;
#endif
    bgc->gc.ngpio = pp->ngpio;
    bgc->gc.base = pp->gpio_base;



    if (pp->irq)
        ncgpio_configure_irqs(gpio, port, pp);

    err = gpiochip_add(&bgc->gc);
    if (err)
        dev_err(gpio->dev, "failed to register gpiochip for %s\n",
            pp->name);
    else
        port->is_registered = true;

    return err;
}

static void ncgpio_unregister(struct ncgpio_gpio *gpio)
{
    unsigned int m;

    for (m = 0; m < gpio->nr_ports; ++m)
        if (gpio->ports[m].is_registered)
            gpiochip_remove(&gpio->ports[m].bgc.gc);
}

static void ncgpio_irq_teardown(struct ncgpio_gpio *gpio)
{
    struct ncgpio_port *port = &gpio->ports[0];
    struct gpio_chip *gc = &port->bgc.gc;
    unsigned int ngpio = gc->ngpio;
    irq_hw_number_t hwirq;

    if (!gpio->domain)
        return;

    for (hwirq = 0 ; hwirq < ngpio ; hwirq++)
        irq_dispose_mapping(irq_find_mapping(gpio->domain, hwirq));

    irq_domain_remove(gpio->domain);
    gpio->domain = NULL;
}


static int ncgpio_probe(struct platform_device *pdev)
{
    unsigned int i;
    struct resource *res;
    struct ncgpio_gpio *gpio;
    int err;
    struct device *dev = &pdev->dev;
    struct ncgpio_platform_data *pdata = dev_get_platdata(dev);

    if (!pdata) {
        pdata = ncgpio_get_pdata_of(dev);
        if (IS_ERR(pdata))
            return PTR_ERR(pdata);
    }

    if (!pdata->nports)
		return -ENODEV;

    gpio = devm_kzalloc(&pdev->dev, sizeof(*gpio), GFP_KERNEL);
    if (!gpio)
		return -ENOMEM;

    gpio->dev = &pdev->dev;
    gpio->nr_ports = pdata->nports;

    gpio->ports = devm_kcalloc(&pdev->dev, gpio->nr_ports,
                   sizeof(*gpio->ports), GFP_KERNEL);
    if (!gpio->ports)
		return -ENOMEM;

    res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    gpio->regs = devm_ioremap_resource(&pdev->dev, res);
    if (IS_ERR(gpio->regs))
		return PTR_ERR(gpio->regs);

    for (i = 0; i < gpio->nr_ports; i++) {
		err = ncgpio_add_port(gpio, &pdata->properties[i], i);
		if (err)
			goto out_unregister;
    }
	platform_set_drvdata(pdev, gpio);

	return 0;
out_unregister:
	ncgpio_unregister(gpio);
    ncgpio_irq_teardown(gpio);
	return err;
}

static int ncgpio_remove(struct platform_device *pdev)
{
	struct ncgpio_gpio *gpio = platform_get_drvdata(pdev);

    ncgpio_unregister(gpio);
	ncgpio_irq_teardown(gpio);

	return 0;
}

static const struct of_device_id ncgpio_of_match[] = {
    { .compatible = "NationalChip-gpio" },
    { },
};
MODULE_DEVICE_TABLE(of, ncgpio_of_match);

static struct platform_driver ncgpio_driver = {
    .probe      = ncgpio_probe,
    .remove     = ncgpio_remove,
    .driver = {
        .name       = DRIVER_NAME,
        .of_match_table = ncgpio_of_match,
    },
};

module_platform_driver(ncgpio_driver);

MODULE_AUTHOR("zhangjun");
MODULE_DESCRIPTION("gpio driver for national chip");
MODULE_LICENSE("GPL v2");

