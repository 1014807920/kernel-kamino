#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/gpio/driver.h>
#include <linux/irqdomain.h>
#include <linux/spinlock.h>
#include <linux/syscore_ops.h>
#include <linux/pinctrl/consumer.h>
#include <linux/pinctrl/machine.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/pinctrl/pinconf-generic.h>
#include <linux/pinctrl/pinmux.h>
#include <linux/interrupt.h>
#include "../core.h"

#define FUNC_INPUT  0x0
#define FUNC_OUTPUT 0x1
#define PIN_NAME_LENGTH 10
#define LONG_BIT (8 * sizeof(long))
/*#define DEBUG 1*/

#ifdef DEBUG
#define PRINTK         printk
#else
#define PRINTK(...)        do{}while(0);
#endif

enum pincfg_type {
    PINCFG_TYPE_DIR,
    PINCFG_TYPE_SET,
    PINCFG_TYPE_DAT,
	PINCFG_TYPE_PWM_SEL,
	PINCFG_TYPE_PWM_EN,
	PINCFG_TYPE_OPEN,

    PINCFG_TYPE_NUM
};

#define INTC_EN					0x40
#define INTC_STATUS				0x44
#define INTC_MASK				0x48
#define INTC_LEVEL_HIGHT		0x50
#define INTC_LEVEL_LOW			0x54
#define INTC_EDGE_RISING		0x58
#define INTC_EDGE_FALLING		0x5c
#define PINCFG_TYPE_MASK        0xFFFF
#define PINCFG_VALUE_SHIFT      16
#define PINCFG_VALUE_MASK       (0xFFFF << PINCFG_VALUE_SHIFT)
#define PINCFG_PACK(type, value)    (((value) << PINCFG_VALUE_SHIFT) | type)
#define PINCFG_UNPACK_TYPE(cfg)     ((cfg) & PINCFG_TYPE_MASK)
#define PINCFG_UNPACK_VALUE(cfg)    (((cfg) & PINCFG_VALUE_MASK) >> \
                        PINCFG_VALUE_SHIFT)

#define PW_SEL_CHANGE(data, shift) (((data >> 3 * shift) & 0x7) << (shift * 4))
#define PWM_SEL_READ(data) (PW_SEL_CHANGE(data,0) | PW_SEL_CHANGE(data,1) | PW_SEL_CHANGE(data,2) | PW_SEL_CHANGE(data,3) | PW_SEL_CHANGE(data,4) | PW_SEL_CHANGE(data,5) | PW_SEL_CHANGE(data,6) | PW_SEL_CHANGE(data,7))



static struct pin_config {
    const char *property;
    enum pincfg_type param;
} cfg_params[] = {

    { "nationalchip,pin-dir",    PINCFG_TYPE_DIR     },
    { "nationalchip,pin-set",    PINCFG_TYPE_SET     },
    { "nationalchip,pin-data",   PINCFG_TYPE_DAT     },
	{ "nationalchip,pin-pwm_sel",PINCFG_TYPE_PWM_SEL },
	{ "nationalchip,pin-pwm_en", PINCFG_TYPE_PWM_EN  },
	{ "nationalchip,pin-open",   PINCFG_TYPE_OPEN    },
};

struct nationalchip_pin_bank_type {
    u32 fld_width[PINCFG_TYPE_NUM];
    u32 reg_offset[PINCFG_TYPE_NUM];
};

enum eint_type {
	EINT_TYPE_NONE,
	EINT_TYPE_GPIO,
};

struct nationalchip_pin_bank {
    struct      nationalchip_pin_bank_type type;
    u32         pctl_offset;
    u32         nr_pins;
    u8          eint_func;
	enum        eint_type  eint_type;
    u32         eint_mask;
    u32         eint_offset;
    const       char  *name;

    u32         pin_base;
    void        *soc_priv;
    struct      device_node *of_node;
    struct      nationalchip_pinctrl_drv_data *drvdata;
    struct      irq_domain *irq_domain;
    struct      gpio_chip gpio_chip;
    struct      pinctrl_gpio_range grange;
    struct      irq_chip irq_chip;
    spinlock_t  slock;

    u32         pm_save[PINCFG_TYPE_NUM + 1]; /* +1 to handle double CON registers*/
};

/**
 * struct nationalchip_pin_group: represent group of pins of a pinmux function.
 * @name: name of the pin group, used to lookup the group.
 * @pins: the pins included in this group.
 * @num_pins: number of pins included in this group.
 * @func: the function number to be programmed when selected.
 */
struct nationalchip_pin_group {
    const char      *name;
    const unsigned int  *pins;
    u8          num_pins;
    u8          func;
};

struct nationalchip_pmx_func {
    const char      *name;
    const char      **groups;
    u8          num_groups;
	u32         val;
};


struct nationalchip_pinctrl_drv_data {
    struct list_head        node;
    void __iomem            *virt_base;
    struct device           *dev;
    int             irq;

    struct pinctrl_desc     pctl;
    struct pinctrl_dev      *pctl_dev;

    const struct nationalchip_pin_group  *pin_groups;
    unsigned int            nr_groups;
    const struct nationalchip_pmx_func   *pmx_functions;
    unsigned int            nr_functions;

    struct nationalchip_pin_bank     *pin_banks;
    u32             nr_banks;
    unsigned int            pin_base;
    unsigned int            nr_pins;
};

static LIST_HEAD(drvdata_list);
static unsigned int pin_base = 0;

static int nationalchip_get_group_count(struct pinctrl_dev *pctldev)
{
    struct nationalchip_pinctrl_drv_data *pmx = pinctrl_dev_get_drvdata(pctldev);

    return pmx->nr_groups;
}

static const char *nationalchip_get_group_name(struct pinctrl_dev *pctldev,
                        unsigned group)
{
    struct nationalchip_pinctrl_drv_data *pmx = pinctrl_dev_get_drvdata(pctldev);

    return pmx->pin_groups[group].name;
}

static int nationalchip_get_group_pins(struct pinctrl_dev *pctldev,
                    unsigned group,
                    const unsigned **pins,
                    unsigned *num_pins)
{
    struct nationalchip_pinctrl_drv_data *pmx = pinctrl_dev_get_drvdata(pctldev);

    *pins = pmx->pin_groups[group].pins;
    *num_pins = pmx->pin_groups[group].num_pins;

    return 0;
}

static void nationalchip_dt_free_map(struct pinctrl_dev *pctldev,
                      struct pinctrl_map *map,
                      unsigned num_maps)
{
    int i;

    for (i = 0; i < num_maps; i++)
        if (map[i].type == PIN_MAP_TYPE_CONFIGS_GROUP)
            kfree(map[i].data.configs.configs);

    kfree(map);
}

static int nationalchip_dt_subnode_to_map(struct nationalchip_pinctrl_drv_data *drvdata,
		struct device *dev,
		struct device_node *np,
		struct pinctrl_map **map,
		unsigned *reserved_maps,
		unsigned *num_maps)
{
	int ret, i;
	u32 val;
	unsigned long config;
	unsigned long *configs = NULL;
	unsigned long *cfigs = NULL;
	unsigned num_configs = 0;
	unsigned reserve;
	struct property *prop;
	const char *group;
	bool has_func = false;

	ret = of_property_read_u32(np, cfg_params[PINCFG_TYPE_DIR].property, &val);
	if (!ret)
		has_func = true;
	for (i = 0; i < ARRAY_SIZE(cfg_params); i++) {
		if (!of_find_property(np, cfg_params[i].property, NULL))
			continue;
		++num_configs;
	}
	PRINTK("dt to map num_configs : %d \n",num_configs);

	configs = devm_kzalloc(dev, num_configs * sizeof(config),
			GFP_KERNEL);
	if (!configs){
		dev_err(dev, "kzalloc(configs) failed\n");
		return -EINVAL;
	}

	cfigs = configs;
	for (i = 0; i < ARRAY_SIZE(cfg_params); i++) {
		ret = of_property_read_u32(np, cfg_params[i].property, &val);
		if (!ret) {
			config = PINCFG_PACK(cfg_params[i].param, val);
			*cfigs++ = config;
			PRINTK("get %s : %d\n",cfg_params[i].property, val);
			/* EINVAL=missing, which is fine since it's optional */
		} else if (ret != -EINVAL) {
			dev_err(dev, "could not parse property %s\n",
					cfg_params[i].property);
		}
	}

	reserve = 0;
	if (has_func)
		reserve++;
	if (num_configs)
		reserve++;
	ret = of_property_count_strings(np, "nationalchip,pins");
	if (ret < 0) {
		dev_err(dev, "could not parse property nationalchip,pins\n");
		kfree(configs);
		return -1;
	}
	reserve *= ret;

	*map = devm_kzalloc(dev, reserve * sizeof(struct pinctrl_map), GFP_KERNEL);
	if (!*map)
		return -ENOMEM;

	*num_maps = 0;
	of_property_for_each_string(np, "nationalchip,pins", prop, group) {
		if (has_func) {
			(*map)[*num_maps].type = PIN_MAP_TYPE_MUX_GROUP;
			(*map)[*num_maps].data.mux.group = group;
			(*map)[*num_maps].data.mux.function = np->full_name;
			(*num_maps)++;
		}
		if (num_configs) {
			(*map)[*num_maps].type                      = PIN_MAP_TYPE_CONFIGS_GROUP;
			(*map)[*num_maps].data.configs.group_or_pin = group;
			(*map)[*num_maps].data.configs.configs      = configs;
			(*map)[*num_maps].data.configs.num_configs  = num_configs;
			(*num_maps)++;
		}
	}

	return 0;
}

static int nationalchip_dt_node_to_map(struct pinctrl_dev *pctldev,
                    struct device_node *np_config,
                    struct pinctrl_map **map,
                    unsigned *num_maps)
{
	struct nationalchip_pinctrl_drv_data *drvdata;
	unsigned reserved_maps;
	int ret;

	drvdata = pinctrl_dev_get_drvdata(pctldev);

	reserved_maps = 0;
	*map = NULL;
	*num_maps = 0;

	PRINTK("pinctrl node to map start\n");
	ret = nationalchip_dt_subnode_to_map(drvdata, pctldev->dev, np_config, map,
			&reserved_maps,
			num_maps);
	PRINTK("pinctrl node to map end\n");
	if (ret < 0) {
		nationalchip_dt_free_map(pctldev, *map, *num_maps);
		return ret;
	}

	return 0;
}

static const struct pinctrl_ops nationalchip_pctrl_ops = {
	.get_groups_count	= nationalchip_get_group_count,
	.get_group_name		= nationalchip_get_group_name,
	.get_group_pins		= nationalchip_get_group_pins,
	.dt_node_to_map		= nationalchip_dt_node_to_map,
	.dt_free_map		= nationalchip_dt_free_map,
};

static int nationalchip_get_functions_count(struct pinctrl_dev *pctldev)
{
    struct nationalchip_pinctrl_drv_data *drvdata = pinctrl_dev_get_drvdata(pctldev);
    return drvdata->nr_functions;
}

/* return the name of the pin function specified */
static const char *nationalchip_pinmux_get_fname(struct pinctrl_dev *pctldev,
                        unsigned selector)
{
    struct nationalchip_pinctrl_drv_data *drvdata = pinctrl_dev_get_drvdata(pctldev);
    return drvdata->pmx_functions[selector].name;
}

/* return the groups associated for the specified function selector */
static int nationalchip_pinmux_get_groups(struct pinctrl_dev *pctldev,
        unsigned selector, const char * const **groups,
        unsigned * const num_groups)
{
    struct nationalchip_pinctrl_drv_data *drvdata = drvdata = pinctrl_dev_get_drvdata(pctldev);
    *groups = drvdata->pmx_functions[selector].groups;
    *num_groups = drvdata->pmx_functions[selector].num_groups;
    return 0;
}

static void pin_to_reg_bank(struct nationalchip_pinctrl_drv_data *drvdata,
            unsigned pin, void __iomem **reg, u32 *offset,
            struct nationalchip_pin_bank **bank)
{
    struct nationalchip_pin_bank *b;

    b = drvdata->pin_banks;

    while ((pin >= b->pin_base) &&
            ((b->pin_base + b->nr_pins - 1) < pin))
        b++;

    *reg = drvdata->virt_base + b->pctl_offset +  b->type.reg_offset[PINCFG_TYPE_DIR];
    *offset = pin - b->pin_base;
    if (bank)
        *bank = b;
}

static int nationalchip_pinmux_set_mux(struct pinctrl_dev *pctldev,
                  unsigned selector,
                  unsigned group)
{
	struct nationalchip_pinctrl_drv_data *drvdata;
	struct nationalchip_pin_bank *bank;
	void __iomem *reg;
	u32 data, pin_offset;
	unsigned long flags;
	const struct nationalchip_pmx_func *func;
	const struct nationalchip_pin_group *grp;

	drvdata = pinctrl_dev_get_drvdata(pctldev);
	func = &drvdata->pmx_functions[selector];
	grp = &drvdata->pin_groups[group];

	pin_to_reg_bank(drvdata, grp->pins[0] - drvdata->pin_base,
			&reg, &pin_offset, &bank);

	spin_lock_irqsave(&bank->slock, flags);

	data = readl(reg);
	data |= func->val << pin_offset;
	writel(data, reg);

	spin_unlock_irqrestore(&bank->slock, flags);

    return 0;
}

static const struct pinmux_ops nationalchip_pinmux_ops = {
	.get_functions_count = nationalchip_get_functions_count,
	.get_function_name   = nationalchip_pinmux_get_fname,
	.get_function_groups = nationalchip_pinmux_get_groups,
	.set_mux             = nationalchip_pinmux_set_mux,
};

static int nationalchip_pinconf_rw(struct pinctrl_dev *pctldev, unsigned int pin,
                unsigned long *config, bool set)
{
    struct nationalchip_pinctrl_drv_data *drvdata;
	const struct nationalchip_pin_bank_type *type;
    struct nationalchip_pin_bank *bank;
    void __iomem *reg_base;
	enum pincfg_type cfg_type = PINCFG_UNPACK_TYPE(*config);
    u32 pin_offset, data, width, mask, shift;
    u32 cfg_value, cfg_reg;
    unsigned long flags;

    drvdata = pinctrl_dev_get_drvdata(pctldev);
    pin_to_reg_bank(drvdata, pin - drvdata->pin_base, &reg_base,
                    &pin_offset, &bank);

	type = &bank->type;
	if (cfg_type >= PINCFG_TYPE_NUM || !type->fld_width[cfg_type])
		return -EINVAL;
	width = type->fld_width[cfg_type];
	cfg_reg = type->reg_offset[cfg_type] + (pin_offset * width / LONG_BIT) * sizeof(long);

	mask = (1 << width) - 1;
	shift = (pin_offset * width % LONG_BIT);
    spin_lock_irqsave(&bank->slock, flags);
	data = readl(reg_base + cfg_reg);
	if (cfg_type == PINCFG_TYPE_PWM_SEL){
		data = PWM_SEL_READ(data);
	}

    if (set) {
        cfg_value = PINCFG_UNPACK_VALUE(*config);
        data &= ~(mask << shift);
        data |= (cfg_value << shift);
        writel(data, reg_base + cfg_reg);
    } else {
        data >>= shift;
        data &= mask;
        *config = PINCFG_PACK(cfg_type, data);
    }
    spin_unlock_irqrestore(&bank->slock, flags);

    return 0;
}

static int nationalchip_pinconf_get(struct pinctrl_dev *pctldev, unsigned int pin,
                    unsigned long *config)
{
	return nationalchip_pinconf_rw(pctldev, pin, config, false);
}

static int nationalchip_pinconf_set(struct pinctrl_dev *pctldev, unsigned int pin,
		unsigned long *configs, unsigned num_configs)
{
	int i, ret;

	for (i = 0; i < num_configs; i++) {
		ret = nationalchip_pinconf_rw(pctldev, pin, &configs[i], true);
		if (ret < 0)
			return ret;
	} /* for each config */

	return 0;

}

static int nationalchip_pinconf_group_get(struct pinctrl_dev *pctldev,
		unsigned int group, unsigned long *config)
{
	struct nationalchip_pinctrl_drv_data *drvdata;
	const unsigned int *pins;

	drvdata = pinctrl_dev_get_drvdata(pctldev);
	pins = drvdata->pin_groups[group].pins;
	nationalchip_pinconf_get(pctldev, pins[0], config);
	return 0;
}

static int nationalchip_pinconf_group_set(struct pinctrl_dev *pctldev,
            unsigned group, unsigned long *configs,
            unsigned num_configs)
{
	struct nationalchip_pinctrl_drv_data *drvdata;
	const unsigned int *pins;
	unsigned int cnt;

	drvdata = pinctrl_dev_get_drvdata(pctldev);
	pins = drvdata->pin_groups[group].pins;

	for (cnt = 0; cnt < drvdata->pin_groups[group].num_pins; cnt++)
		nationalchip_pinconf_set(pctldev, pins[cnt], configs, num_configs);

    return 0;
}

static const struct pinconf_ops nationalchip_pinconf_ops = {
	.pin_config_get       = nationalchip_pinconf_get,
	.pin_config_set       = nationalchip_pinconf_set,
	.pin_config_group_get = nationalchip_pinconf_group_get,
	.pin_config_group_set = nationalchip_pinconf_group_set,
};

static inline struct nationalchip_pin_bank *gc_to_pin_bank(struct gpio_chip *gc)
{
    return container_of(gc, struct nationalchip_pin_bank, gpio_chip);
}

/* gpiolib gpio_set callback function */
static void nationalchip_gpio_set(struct gpio_chip *gc, unsigned offset, int value)
{
    struct nationalchip_pin_bank *bank = gc_to_pin_bank(gc);
    unsigned long flags;
    void __iomem *reg;
    u32 data;

    reg = bank->drvdata->virt_base + bank->pctl_offset + bank->type.reg_offset[PINCFG_TYPE_SET];

    spin_lock_irqsave(&bank->slock, flags);

    if (value){
		data  = readl(reg);
        data |= 1 << offset;
	}
	else{
		data  = readl(reg);
        data &= ~(1 << offset);
	}

    writel(data, reg);
    spin_unlock_irqrestore(&bank->slock, flags);
}

/* gpiolib gpio_get callback function */
static int nationalchip_gpio_get(struct gpio_chip *gc, unsigned offset)
{
    struct nationalchip_pin_bank *bank = gc_to_pin_bank(gc);
    void __iomem *reg;
    u32 data;

    reg = bank->drvdata->virt_base + bank->pctl_offset + bank->type.reg_offset[PINCFG_TYPE_DAT];

    data = readl(reg);
    data >>= offset;
    data &= 1;

    return data;
}

static int nationalchip_gpio_set_direction(struct gpio_chip *gc,
                         unsigned offset, bool input)
{
	struct nationalchip_pin_bank *bank;
	void __iomem *reg;
	u32 data;
	unsigned long flags;

	bank = gc_to_pin_bank(gc);

	reg = bank->drvdata->virt_base + bank->pctl_offset + bank->type.reg_offset[PINCFG_TYPE_DIR];

	spin_lock_irqsave(&bank->slock, flags);

	data = readl(reg);
	data &= ~(1 << offset);
	if (input)
		data |= FUNC_OUTPUT << offset;
	writel(data, reg);

	spin_unlock_irqrestore(&bank->slock, flags);

	return 0;
}

static int nationalchip_get_direction(struct gpio_chip *chip,
                        unsigned offset)
{
    struct nationalchip_pin_bank *bank;
    void __iomem *reg;
    u32 data;

    bank = gc_to_pin_bank(chip);

    reg = bank->drvdata->virt_base + bank->pctl_offset + bank->type.reg_offset[PINCFG_TYPE_DIR];

    data = readl(reg);
    data >>= offset;
	data &= 1;

    return !data;
}

/* gpiolib gpio_direction_input callback function. */
static int nationalchip_gpio_direction_input(struct gpio_chip *gc, unsigned offset)
{
    return nationalchip_gpio_set_direction(gc, offset, false);
}

/* gpiolib gpio_direction_output callback function. */
static int nationalchip_gpio_direction_output(struct gpio_chip *gc, unsigned offset,
                            int value)
{
    int ret = nationalchip_gpio_set_direction(gc, offset, true);
	return ret;
}

static int nationalchip_gpio_to_irq(struct gpio_chip *gc, unsigned offset)
{
    struct nationalchip_pin_bank *bank = gc_to_pin_bank(gc);
    unsigned int virq;

    if (!bank->irq_domain)
        return -ENXIO;

    virq = irq_create_mapping(bank->irq_domain, offset);
	PRINTK("gpio_to_irq offset : %d virq : %d\n", offset, virq);

    return (virq) ? : -ENXIO;
}

static int nationalchip_gpiolib_register(struct platform_device *pdev,
                    struct nationalchip_pinctrl_drv_data *drvdata)
{
    int i, ret;
    struct nationalchip_pin_bank *bank = drvdata->pin_banks;
    struct gpio_chip *gc;

	for (i = 0; i < drvdata->nr_banks; ++i, ++bank) {
		gc                   = &bank->gpio_chip;
		gc->owner            = THIS_MODULE;
		gc->request          = gpiochip_generic_request;
		gc->free             = gpiochip_generic_free;
		gc->set              = nationalchip_gpio_set;
		gc->get              = nationalchip_gpio_get;
		gc->get_direction    = nationalchip_get_direction;
		gc->direction_input  = nationalchip_gpio_direction_input;
		gc->direction_output = nationalchip_gpio_direction_output;
		gc->to_irq           = nationalchip_gpio_to_irq;

		gc->base             = drvdata->pin_base + bank->pin_base;
		gc->ngpio            = bank->nr_pins;
		gc->dev              = &pdev->dev;
		gc->of_node          = bank->of_node;
		gc->label            = bank->name;

        ret = gpiochip_add(gc);
        if (ret) {
            dev_err(&pdev->dev, "failed to register gpio_chip %s, error code: %d\n",
                            gc->label, ret);
            goto fail;
        }
    }

    return 0;

fail:
    for (--i, --bank; i >= 0; --i, --bank)
        gpiochip_remove(&bank->gpio_chip);
    return ret;
}

/* unregister the gpiolib interface with the gpiolib subsystem */
static int nationalchip_gpiolib_unregister(struct platform_device *pdev,
                      struct nationalchip_pinctrl_drv_data *drvdata)
{
    struct nationalchip_pin_bank *bank = drvdata->pin_banks;
    int i;

    for (i = 0; i < drvdata->nr_banks; ++i, ++bank)
        gpiochip_remove(&bank->gpio_chip);

    return 0;
}

static struct nationalchip_pin_group *nationalchip_pinctrl_create_groups(
				struct device *dev,
				struct nationalchip_pinctrl_drv_data *drvdata,
				unsigned int *cnt)
{
	struct pinctrl_desc *ctrldesc = &drvdata->pctl;
	struct nationalchip_pin_group *groups, *grp;
	const struct pinctrl_pin_desc *pdesc;
	int i;

	groups = devm_kzalloc(dev, ctrldesc->npins * sizeof(*groups),
				GFP_KERNEL);
	if (!groups)
		return ERR_PTR(-EINVAL);
	grp = groups;

	pdesc = ctrldesc->pins;
	for (i = 0; i < ctrldesc->npins; ++i, ++pdesc, ++grp) {
		grp->name = pdesc->name;
		grp->pins = &pdesc->number;
		grp->num_pins = 1;
	}

	*cnt = ctrldesc->npins;
	return groups;
}

static int nationalchip_pinctrl_create_function(struct device *dev,
                struct nationalchip_pinctrl_drv_data *drvdata,
                struct device_node *func_np,
                struct nationalchip_pmx_func *func)
{
    int npins, ret, i;

    if (of_property_read_u32(func_np, cfg_params[PINCFG_TYPE_DIR].property, &func->val))
        return 0;

    npins = of_property_count_strings(func_np, "nationalchip,pins");
    if (npins < 1) {
        dev_err(dev, "invalid pin list in %s node", func_np->name);
        return -EINVAL;
    }

    func->name = func_np->full_name;

    func->groups = devm_kzalloc(dev, npins * sizeof(char *), GFP_KERNEL);
    if (!func->groups)
        return -ENOMEM;

    for (i = 0; i < npins; ++i) {
        const char *gname;

        ret = of_property_read_string_index(func_np, "nationalchip,pins",
                            i, &gname);
        if (ret) {
            dev_err(dev,
                "failed to read pin name %d from %s node\n",
                i, func_np->name);
            return ret;
        }

        func->groups[i] = gname;
    }

    func->num_groups = npins;
    return 1;
}


static struct nationalchip_pmx_func *nationalchip_pinctrl_create_functions(
		struct device *dev,
		struct nationalchip_pinctrl_drv_data *drvdata,
		unsigned int *cnt)
{
	struct nationalchip_pmx_func *functions, *func;
	struct device_node *dev_np = dev->of_node;
	struct device_node *cfg_np;
	unsigned int func_cnt = 0;
	int ret;

	for_each_child_of_node(dev_np, cfg_np) {
		if (!of_find_property(cfg_np,"nationalchip,pin-function", NULL))
			continue;
		++func_cnt;
	}

	functions = devm_kzalloc(dev, func_cnt * sizeof(*functions),
			GFP_KERNEL);
	if (!functions) {
		dev_err(dev, "failed to allocate memory for function list\n");
		return ERR_PTR(-EINVAL);
	}
	func = functions;

	func_cnt = 0;
	for_each_child_of_node(dev_np, cfg_np) {
		/*struct device_node *func_np;*/
		ret = nationalchip_pinctrl_create_function(dev, drvdata,
				cfg_np, func);
		if (ret < 0)
			return ERR_PTR(ret);
		if (ret > 0) {
			++func;
			++func_cnt;
		}
	}

	*cnt = func_cnt;
	return functions;
}

static int nationalchip_pinctrl_parse_dt(struct platform_device *pdev,
                    struct nationalchip_pinctrl_drv_data *drvdata)
{
    struct device *dev = &pdev->dev;
    struct nationalchip_pin_group *groups;
    struct nationalchip_pmx_func *functions;
    unsigned int grp_cnt = 0, func_cnt = 0;

    groups = nationalchip_pinctrl_create_groups(dev, drvdata, &grp_cnt);
    if (IS_ERR(groups)) {
        dev_err(dev, "failed to parse pin groups\n");
        return PTR_ERR(groups);
    }

    functions = nationalchip_pinctrl_create_functions(dev, drvdata, &func_cnt);
    if (IS_ERR(functions)) {
        dev_err(dev, "failed to parse pin functions\n");
        return PTR_ERR(functions);
    }

    drvdata->pin_groups = groups;
    drvdata->nr_groups = grp_cnt;
    drvdata->pmx_functions = functions;
    drvdata->nr_functions = func_cnt;

    return 0;
}

static int nationalchip_pinctrl_register(struct platform_device *pdev,
                    struct nationalchip_pinctrl_drv_data *drvdata)
{
    struct pinctrl_desc *ctrldesc = &drvdata->pctl;
    struct pinctrl_pin_desc *pindesc, *pdesc;
    struct nationalchip_pin_bank *pin_bank;
    char *pin_names;
    int pin, bank, ret;

    pindesc = devm_kzalloc(&pdev->dev, sizeof(*pindesc) *
			drvdata->nr_pins, GFP_KERNEL);
    if (!pindesc) {
        dev_err(&pdev->dev, "mem alloc for pin descriptors failed\n");
        return -ENOMEM;
    }

    ctrldesc->name    = "nationalchip-pinctrl";
    ctrldesc->owner   = THIS_MODULE;
    ctrldesc->pins    = pindesc;
    ctrldesc->npins   = drvdata->nr_pins;
	ctrldesc->pctlops = &nationalchip_pctrl_ops;
	ctrldesc->pmxops  = &nationalchip_pinmux_ops;
	ctrldesc->confops = &nationalchip_pinconf_ops;

	pdesc = pindesc;
    /* dynamically populate the pin number and pin name for pindesc */
    for (pin = 0, pdesc = pindesc; pin < ctrldesc->npins; pin++, pdesc++)
        pdesc->number = pin + drvdata->pin_base;

    /*
     * allocate space for storing the dynamically generated names for all
     * the pins which belong to this pin-controller.
     */
    pin_names = devm_kzalloc(&pdev->dev, sizeof(char) * PIN_NAME_LENGTH *
                    drvdata->nr_pins, GFP_KERNEL);
    if (!pin_names) {
        dev_err(&pdev->dev, "mem alloc for pin names failed\n");
        return -ENOMEM;
	}
	/* for each pin, the name of the pin is pin-bank name + pin number */
	for (bank = 0; bank < drvdata->nr_banks; bank++) {
		pin_bank                  = &drvdata->pin_banks[bank];
		for (pin = 0; pin < pin_bank->nr_pins; pin++) {
			sprintf(pin_names, "%s-%d", pin_bank->name, pin);
			pdesc        = pindesc + pin_bank->pin_base + pin;
			pdesc->name  = pin_names;
			pin_names   += PIN_NAME_LENGTH;
		}
	}

	ret = nationalchip_pinctrl_parse_dt(pdev, drvdata);
	if (ret)
		return ret;

	drvdata->pctl_dev = pinctrl_register(ctrldesc, &pdev->dev, drvdata);
	if (IS_ERR(drvdata->pctl_dev)) {
		dev_err(&pdev->dev, "could not register pinctrl driver\n");
		return PTR_ERR(drvdata->pctl_dev);
	}

	for (bank = 0; bank < drvdata->nr_banks; bank++) {
		pin_bank                  = &drvdata->pin_banks[bank];
		pin_bank->grange.name     = pin_bank->name;
		pin_bank->grange.id       = bank;
		pin_bank->grange.base     = pin_bank->gpio_chip.base;
		pin_bank->grange.pin_base = drvdata->pin_base + pin_bank->pin_base;
		pin_bank->grange.npins    = pin_bank->gpio_chip.ngpio;
		pin_bank->grange.gc       = &pin_bank->gpio_chip;
		pinctrl_add_gpio_range(drvdata->pctl_dev, &pin_bank->grange);
	}

	return 0;
}

static int nationalchip_pinctrl_get_banks(struct nationalchip_pinctrl_drv_data *d,
		struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct device_node *np;
	struct nationalchip_pin_bank     *bank;
	u32 i;

	for_each_child_of_node(node, np) {
		if (!of_find_property(np, "gpio-controller", NULL))
			continue;
		d->nr_banks++;
	}

	bank = devm_kzalloc(&pdev->dev, d->nr_banks * sizeof(*bank), GFP_KERNEL);
	if (!bank) {
		dev_err(&(pdev->dev), "failed to allocate memory for driver's "
				"pinctrl_drv_data->pin_banks\n");
		return -ENOMEM;
	}

	d->pin_banks = bank;
	for_each_child_of_node(node, np) {
		if (!of_find_property(np, "gpio-controller", NULL))
			continue;
		if (of_property_read_u32(np, "nationalchip,base_offset", &bank->pctl_offset))
			return -1;
		if (of_property_read_u32(np, "nationalchip,nr_pins", &bank->nr_pins))
			return -1;
		if (!of_find_property(np, "interrupt-controller", NULL))
			bank->eint_type = EINT_TYPE_NONE;
		else
			bank->eint_type = EINT_TYPE_GPIO;

		if (of_property_read_u32_array(np, "nationalchip,reg_offset", bank->type.reg_offset, PINCFG_TYPE_NUM)){
			dev_err(&(pdev->dev), "failed to get reg_offset driver's "
					"pinctrl_drv_data->pin_banks\n");
			return -ENOMEM;
		}
		if (of_property_read_u32_array(np, "nationalchip,cfg_width", bank->type.fld_width, PINCFG_TYPE_NUM)){
			dev_err(&(pdev->dev), "failed to get cfg_width driver's "
					"pinctrl_drv_data->pin_banks\n");
			return -ENOMEM;
		}
		for(i = 0; i < PINCFG_TYPE_NUM; i++){
			PRINTK("bank get %d offset %d\n", i, bank->type.fld_width[i]);
		}
		bank->name = np->name;
		bank->of_node = np;

		spin_lock_init(&bank->slock);
		bank->drvdata = d;
		bank->pin_base = d->nr_pins;
		d->nr_pins += bank->nr_pins;
		bank++;
	}
	d->pin_base = pin_base;
	pin_base += d->nr_pins;

	return 0;
}

void nationalchip_gpio_set_imask(struct nationalchip_pinctrl_drv_data *drvdata, struct nationalchip_pin_bank *bank, unsigned int offset, bool enable)
{
	unsigned long flags;
    void __iomem    *irq_base;
	u32 data;

	irq_base = drvdata->virt_base + bank->pctl_offset;

	spin_lock_irqsave(&bank->slock, flags);

	data = readl(irq_base + INTC_MASK);
	if (enable)
		data |= (1 << offset);
	else
		data &= ~(1 << offset);
	writel(data, irq_base + INTC_MASK);

	spin_unlock_irqrestore(&bank->slock, flags);
}

static void nationalchip_gpio_irq_mask(struct irq_data *d)
{
    struct gpio_chip *gc = irq_data_get_irq_chip_data(d);
	struct nationalchip_pin_bank *bank = gc_to_pin_bank(gc);
	struct nationalchip_pinctrl_drv_data *drvdata = bank->drvdata;

    nationalchip_gpio_set_imask(drvdata, bank, d->hwirq, true);
}

static void nationalchip_gpio_irq_unmask(struct irq_data *d)
{
    struct gpio_chip *gc = irq_data_get_irq_chip_data(d);
	struct nationalchip_pin_bank *bank = gc_to_pin_bank(gc);
	struct nationalchip_pinctrl_drv_data *drvdata = bank->drvdata;

    nationalchip_gpio_set_imask(drvdata, bank, d->hwirq, false);
}

static int nationalchip_gpio_set_irq_type(struct irq_data *d, unsigned int type)
{
    struct gpio_chip *gc = irq_data_get_irq_chip_data(d);
	struct nationalchip_pin_bank *bank = gc_to_pin_bank(gc);
	struct nationalchip_pinctrl_drv_data *drvdata = bank->drvdata;
	u32 level_high, level_low, edge_rising, edge_falling;
    void __iomem    *irq_base;
	unsigned long flags;

	irq_base = drvdata->virt_base + bank->pctl_offset;

	spin_lock_irqsave(&bank->slock, flags);
	level_high   = readl(irq_base + INTC_LEVEL_HIGHT);
	level_low    = readl(irq_base + INTC_LEVEL_LOW);
	edge_rising  = readl(irq_base + INTC_EDGE_RISING);
	edge_falling = readl(irq_base + INTC_EDGE_FALLING);

	switch (type) {
	case IRQ_TYPE_LEVEL_HIGH:
		level_high   |=   1 << d->hwirq;
		level_low    &= ~(1 << d->hwirq);
		edge_rising  &= ~(1 << d->hwirq);
		edge_falling &= ~(1 << d->hwirq);
		break;
	case IRQ_TYPE_LEVEL_LOW:
		level_high   &= ~(1 << d->hwirq);
		level_low    |=   1 << d->hwirq;
		edge_rising  &= ~(1 << d->hwirq);
		edge_falling &= ~(1 << d->hwirq);
		break;
	case IRQ_TYPE_EDGE_RISING:
		level_high   &= ~(1 << d->hwirq);
		level_low    &= ~(1 << d->hwirq);
		edge_rising  |=   1 << d->hwirq;
		edge_falling &= ~(1 << d->hwirq);
		break;
	case IRQ_TYPE_EDGE_FALLING:
		level_high   &= ~(1 << d->hwirq);
		level_low    &= ~(1 << d->hwirq);
		edge_rising  &= ~(1 << d->hwirq);
		edge_falling |=   1 << d->hwirq;
		break;
	case IRQ_TYPE_EDGE_BOTH:
		level_high   &= ~(1 << d->hwirq);
		level_low    &= ~(1 << d->hwirq);
		edge_rising  |=   1 << d->hwirq;
		edge_falling |=   1 << d->hwirq;
		break;
	default:
		printk("gx gpio don't support the config\n");
		return -EINVAL;
	}

	writel(level_high,   irq_base + INTC_LEVEL_HIGHT);
	writel(level_low,    irq_base + INTC_LEVEL_LOW);
	writel(edge_rising,  irq_base + INTC_EDGE_RISING);
	writel(edge_falling, irq_base + INTC_EDGE_FALLING);

	spin_unlock_irqrestore(&bank->slock, flags);

	return 0;
}

static void nationalchip_gpio_irq_ack(struct irq_data *d)
{
    struct gpio_chip *gc = irq_data_get_irq_chip_data(d);
	struct nationalchip_pin_bank *bank = gc_to_pin_bank(gc);
	struct nationalchip_pinctrl_drv_data *drvdata = bank->drvdata;
    unsigned long reg_pend = bank->pctl_offset + INTC_STATUS;

	writel(1 << d->hwirq, drvdata->virt_base + reg_pend);
}

static void nationalchip_gpio_irq_handler(struct irq_desc *desc)
{
	struct gpio_chip *gc = irq_desc_get_handler_data(desc);
	struct irq_chip *chip = irq_desc_get_chip(desc);
	struct nationalchip_pin_bank *bank = gc_to_pin_bank(gc);
	struct nationalchip_pinctrl_drv_data *drvdata = bank->drvdata;
    void __iomem    *irq_base;
	u32 data, offset;

	chained_irq_enter(chip, desc);
	irq_base = drvdata->virt_base + bank->pctl_offset;

	data = readl(irq_base + INTC_STATUS);

	for_each_set_bit(offset, (unsigned long *)&data, 32) {
		unsigned int gpio_irq;

		gpio_irq = irq_find_mapping(gc->irqdomain, offset);
		PRINTK("gpio %d irq %d\n", offset, gpio_irq);
		generic_handle_irq(gpio_irq);
	}

	chained_irq_exit(chip, desc);
}


static int nationalchip_gpio_irq_init(struct platform_device *pdev,
		struct nationalchip_pinctrl_drv_data *drvdata)
{
	struct nationalchip_pin_bank *pin_bank;
	struct irq_chip *irq_chip;
	int bank, ret;
    void __iomem    *irq_base;

	irq_base = drvdata->virt_base;

	for (bank = 0; bank < drvdata->nr_banks; bank++) {
		pin_bank               = &drvdata->pin_banks[bank];
		irq_chip               = &pin_bank->irq_chip;
		irq_chip->name         = "nationalchip_gpio_irq";
		irq_chip->irq_mask     = nationalchip_gpio_irq_mask;
		irq_chip->irq_unmask   = nationalchip_gpio_irq_unmask;
		irq_chip->irq_set_type = nationalchip_gpio_set_irq_type;
		irq_chip->irq_ack      = nationalchip_gpio_irq_ack;


		ret = gpiochip_irqchip_add(&pin_bank->gpio_chip, &pin_bank->irq_chip, 0,
				handle_edge_irq, IRQ_TYPE_NONE);
		if (ret) {
			dev_err(&pdev->dev, "Failed to add irq chip\n");
		}
		gpiochip_set_chained_irqchip(&pin_bank->gpio_chip, &pin_bank->irq_chip, drvdata->irq,
				nationalchip_gpio_irq_handler);
	}

	//中断使能
	writel(0xffffffff, drvdata->virt_base + INTC_EN);

	return 0;
}

static int nationalchip_pinctrl_probe(struct platform_device *pdev)
{
	struct nationalchip_pinctrl_drv_data *drvdata;
	struct device *dev = &pdev->dev;
	struct resource *res;
	int ret;

	if (!dev->of_node) {
		dev_err(dev, "device tree node not found\n");
		return -ENODEV;
	}

	drvdata = devm_kzalloc(dev, sizeof(*drvdata), GFP_KERNEL);
	if (!drvdata) {
		dev_err(dev, "failed to allocate memory for driver's "
				"private data\n");
		return -ENOMEM;
	}

	ret = nationalchip_pinctrl_get_banks(drvdata,pdev);
	if (0 != ret) {
		dev_err(&pdev->dev, "driver data not available\n");
		return -ENOMEM;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (of_property_read_bool(pdev->dev.of_node, "interrupts")){
		drvdata->irq = platform_get_irq(pdev,0);
		if (drvdata->irq == -ENXIO)
		{
			dev_err(&pdev->dev, "driver not get irq\n");
			return -ENXIO;
		}
	}

	PRINTK("res start:%#x\n", res->start);
	PRINTK("pinctrl get irq : %d\n", drvdata->irq);
	drvdata->virt_base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(drvdata->virt_base))
		return PTR_ERR(drvdata->virt_base);

	ret = nationalchip_gpiolib_register(pdev, drvdata);
	if (ret)
		return ret;
	PRINTK("----- gx register gpiolib ok\n");

	ret = nationalchip_pinctrl_register(pdev, drvdata);
	if (ret) {
		nationalchip_gpiolib_unregister(pdev, drvdata);
		return ret;
	}
	PRINTK("---- gx register pinctrl ok\n");

	if (of_property_read_bool(pdev->dev.of_node, "interrupts"))
		nationalchip_gpio_irq_init(pdev, drvdata);

	drvdata->dev = dev;
	platform_set_drvdata(pdev, drvdata);
	list_add_tail(&drvdata->node, &drvdata_list);

	return 0;
}


static const struct of_device_id nationalchip_pinctrl_dt_match[] = {
	{ .compatible = "nationalchip,LEO_A7-pinctrl",},
	{},
};
MODULE_DEVICE_TABLE(of, nationalchip_pinctrl_dt_match);

static struct platform_driver nationalchip_pinctrl_driver = {
	.probe  = nationalchip_pinctrl_probe,
	.driver = {
		.name           = "nationalchip-pinctrl",
		.of_match_table = nationalchip_pinctrl_dt_match,
	},
};

static int __init nationalchip_pinctrl_drv_register(void)
{
	printk("gx pinctrl init\n");

	return platform_driver_register(&nationalchip_pinctrl_driver);
}
postcore_initcall(nationalchip_pinctrl_drv_register);

static void __exit nationalchip_pinctrl_drv_unregister(void)
{
	struct nationalchip_pinctrl_drv_data *drvdata;
    struct nationalchip_pin_bank *bank;
    int i;

	list_for_each_entry(drvdata, &drvdata_list, node) {
		pinctrl_unregister(drvdata->pctl_dev);
		bank = drvdata->pin_banks;
		for (i = 0; i < drvdata->nr_banks; ++i, ++bank)
			gpiochip_remove(&bank->gpio_chip);
	}
	platform_driver_unregister(&nationalchip_pinctrl_driver);
}

module_exit(nationalchip_pinctrl_drv_unregister);

MODULE_DESCRIPTION("nationalchip pinctrl driver");
MODULE_LICENSE("GPL v2");

