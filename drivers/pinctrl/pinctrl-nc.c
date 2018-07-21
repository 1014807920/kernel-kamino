#include <linux/init.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/list.h>

#include <linux/of.h>
#include <linux/of_device.h>

#include <linux/pinctrl/pinctrl.h>
#include <linux/pinctrl/pinmux.h>
#include <linux/pinctrl/pinconf-generic.h>
#include <linux/pinctrl/machine.h>

#define DRIVER_NAME         "nc-pinctrl"
#define NC_MUX_PINS_NAME "pinctrl-nc,pins"

struct nc_func_vals {
	int pin;
	unsigned val;
};

/**
 * struct nc_pingroup - pingroups for a function
 * @np:     pingroup device node pointer
 * @name:   pingroup name
 * @gpins:  array of the pins in the group
 * @ngpins: number of pins in the group
 * @node:   list node
 */
struct nc_pingroup {
    struct device_node *np;
    const char *name;
    int *gpins;
    int ngpins;
    struct list_head node;
};

/**
 * struct nc_function - pinctrl function
 * @name:   pinctrl function name
 * @vals:   register and vals array
 * @nvals:  number of entries in vals array
 * @pgnames:    array of pingroup names the function uses
 * @npgnames:   number of pingroup names the function uses
 * @node:   list node
 */
struct nc_function {
    const char *name;
    struct nc_func_vals *vals;
    unsigned nvals;
    const char **pgnames;
    int npgnames;
    struct nc_conf_vals *conf;
    int nconfs;
    struct list_head node;
};

/**
 * struct nc_gpiofunc_range - pin ranges with same mux value of gpio function
 * @offset: offset base of pins
 * @npins:  number pins with the same mux value of gpio function
 * @gpiofunc:   mux value of gpio function
 * @node:   list node
 */ 
struct nc_gpiofunc_range {
    unsigned offset;
    unsigned npins;
    unsigned gpiofunc;
    struct list_head node;
};

struct nc_device {
    struct resource *res;
    void __iomem *base;
    unsigned size;
    struct device *dev;
    struct pinctrl_dev *pctl;
	char is_ck;

    raw_spinlock_t lock;
    struct mutex mutex;

	struct pinctrl_pin_desc *pins;

    struct radix_tree_root pgtree;
    struct radix_tree_root ftree;
    struct list_head pingroups;
    struct list_head functions;
	struct list_head gpiofuncs;

    unsigned ngroups;
    unsigned nfuncs;
    struct pinctrl_desc desc;
};

struct padmux_sel {
    unsigned char sel[3];
};

/* Default padmux config DONOT edit */
#define PMUX_INV (0xff)
static const struct padmux_sel padmux_table[] = {
/*    bit0	| bit1	| bit2			function0  | function1 | function2 | function3 | function4 | function5 */
	{ { 0,	PMUX_INV, PMUX_INV } }, // BTDAT00    | PD2PORT00
    { { 1,	PMUX_INV, PMUX_INV } }, // BTDAT01    | PD2PORT01
    { { 2,	PMUX_INV, PMUX_INV } }, // BTDAT02    | PD2PORT02
    { { 3,	PMUX_INV, PMUX_INV } }, // BTDAT03    | PD2PORT03
    { { 4,	PMUX_INV, PMUX_INV } }, // BTCLKIN    | PD2PORT04
    { { 5,	PMUX_INV, PMUX_INV } }, // BTRESET    | PD2PORT05
    { { 6,	PMUX_INV, PMUX_INV } }, // BTCLKVSYNC | PD2PORT06
    { { 7,	PMUX_INV, PMUX_INV } }, // BTCLKHREF  | PD2PORT07
    { { 8,	PMUX_INV, PMUX_INV } }, // BTCLKOUT   | PD2PORT08
    { { 9,	      64, PMUX_INV } }, // BTDAT04    | SDA2      | SPI1SCK   | PD2PORT09
    { {10,	      65, PMUX_INV } }, // BTDAT05    | SCL2      | SPI1MOSI  | PD2PORT10
    { {11,	      66, PMUX_INV } }, // BTDAT06    | UART2RX   | SPI1CSn   | PD2PORT11
    { {12,	      67,       96 } }, // BTDAT07    | UART2TX   | SPI1MISO  | NUARTTX   | AUARTTX   | PD2PORT12
    { {13,	PMUX_INV, PMUX_INV } }, // BTDAT08    | PD2PORT13
    { {14,	PMUX_INV, PMUX_INV } }, // BTDAT09    | PD2PORT14
    { {15,	PMUX_INV, PMUX_INV } }, // BTDAT10    | PD2PORT15
    { {16,	PMUX_INV, PMUX_INV } }, // BTDAT11    | PD2PORT16
    { {17,	PMUX_INV, PMUX_INV } }, // BTDAT12    | PD2PORT17
    { {18,	PMUX_INV, PMUX_INV } }, // BTDAT13    | PD2PORT18
    { {19,	PMUX_INV, PMUX_INV } }, // BTDAT14    | PD2PORT19
    { {20,	PMUX_INV, PMUX_INV } }, // BTDAT15    | PD2PORT20
    { {21,	      68, PMUX_INV } }, // UART3RX    | SD1CDn    | SDA3      | PD2PORT21
    { {22,	      69, PMUX_INV } }, // UART3TX    | SD1DAT1   | SCL3      | PD2PORT22
    { {23,	      70, PMUX_INV } }, // DBGTDI     | SD1DAT0   | SPI2SCK   | PD2PORT23
    { {24,	      71, PMUX_INV } }, // DBGTDO     | SD1CLK    | SPI2MOSI  | PD2PORT24
    { {25,	      72, PMUX_INV } }, // DBGTMS     | SD1CMD    | SPI2CSn   | PD2PORT25
    { {26,	      73, PMUX_INV } }, // DBGTCK     | SD1DAT3   | SPI2MISO  | PD2PORT26
    { {27,	      74, PMUX_INV } }, // DBGTRST    | SD1DAT2   | PD2PORT27
    { {28,	      75, PMUX_INV } }, // UART2RX    | SDA2      | PD2PORT28
    { {29,	      76, PMUX_INV } }, // UART2TX    | SCL2      | PD2PORT29
    { {30,	      77, PMUX_INV } }, // SPI1SCK    | NDBGTDI   | ADBGTDI   | PD2PORT30
    { {31,	      78, PMUX_INV } }, // SPI1MOSI   | NDBGTDO   | ADBGTDO   | PD2PORT31
    { {32,	      79, PMUX_INV } }, // SPI1CSn    | NDBGTMS   | ADBGTMS   | PD2PORT32
    { {33,	      80, PMUX_INV } }, // SPI1MISO   | NDBGTCK   | ADBGTCK   | PD2PORT33
    { {34,	      81, PMUX_INV } }, // SPI2SCK    | NDBGTRST  | ADBGTRST  | PD2PORT34
    { {35,	PMUX_INV, PMUX_INV } }, // SPI2MOSI   | PD2PORT35
    { {36,	PMUX_INV, PMUX_INV } }, // SPI2CSn    | PD2PORT36
    { {37,	PMUX_INV, PMUX_INV } }, // SPI2MISO   | PD2PORT37
    { {38,	PMUX_INV, PMUX_INV } }, // SD0CDn     | PD2PORT38
    { {39,	PMUX_INV, PMUX_INV } }, // SD0DAT1    | PD2PORT39
    { {40,	PMUX_INV, PMUX_INV } }, // SD0DAT0    | PD2PORT40
    { {41,	PMUX_INV, PMUX_INV } }, // SD0CLK     | PD2PORT41
    { {42,	PMUX_INV, PMUX_INV } }, // SD0CMD     | PD2PORT42
    { {43,	PMUX_INV, PMUX_INV } }, // SD0DAT3    | PD2PORT43
    { {44,	PMUX_INV, PMUX_INV } }, // SD0DAT2    | PD2PORT44
    { {45,	PMUX_INV, PMUX_INV } }, // SD1CDn     | PD2PORT45
    { {46,	PMUX_INV, PMUX_INV } }, // SD1DAT1    | PD2PORT46
    { {47,	PMUX_INV, PMUX_INV } }, // SD1DAT0    | PD2PORT47
    { {48,	PMUX_INV, PMUX_INV } }, // SD1CLK     | PD2PORT48
    { {49,	PMUX_INV, PMUX_INV } }, // SD1CMD     | PD2PORT49
    { {50,	PMUX_INV, PMUX_INV } }, // SD1DAT3    | PD2PORT50
    { {51,	PMUX_INV, PMUX_INV } }, // SD1DAT2    | PD2PORT51
};

static const struct padmux_sel ck_padmux_table[] = {
/*    bit0 | bit1   | bit2   func    function0  | function1 | function2   | function3  | function4 */
	{ { PMUX_INV, PMUX_INV, PMUX_INV } }, // only gpio mode ?
	{ { 1, PMUX_INV, PMUX_INV } },  // POWERDOWN  | PD1PORT01
	{ { 2, PMUX_INV, PMUX_INV } },  // UART0RX    | PD1PORT02
	{ { 4, PMUX_INV, PMUX_INV } },  // UART0TX    | PD1PORT03
	{ { 5, PMUX_INV, PMUX_INV } },  // OTPAVDDEN  | PD1PORT04
	{ { 3,       64, PMUX_INV } },  // SDBGTDI    | DDBGTDI   | SNDBGTDI    | PD1PORT05
	{ { 6,       65, PMUX_INV } },  // SDBGTDO    | DDBGTDO   | SNDBGTDO    | PD1PORT06
	{ { 7,       66,       96 } },  // SDBGTMS    | DDBGTMS   | SNDBGTMS    | PCM1INBCLK | PD1PORT07
	{ { 8,       67,       97 } },  // SDBGTCK    | DDBGTCK   | SNDBGTCK    | PCM1INLRCK | PD1PORT08
	{ { 9,       68,       98 } },  // SDBGTRST   | DDBGTRST  | SNBGTRST    | PCM1INDAT0 | PD1PORT09
	{ {PMUX_INV, PMUX_INV, PMUX_INV } },  //
	{ {10, PMUX_INV, PMUX_INV } },  // PCM1INBCLK | PD1PORT11
	{ {11, PMUX_INV, PMUX_INV } },  // PCM1INLRCK | PD1PORT12
	{ {12, PMUX_INV, PMUX_INV } },  // PCM1INDAT0 | PD1PORT13
	{ {13,       69, PMUX_INV } },  // PCMOUTMCLK | DUARTTX   | SNUARTTX    | PD1PORT14
	{ {14,       70, PMUX_INV } },  // PCMOUTDAT0 | SPDIF     | PD1PORT15
	{ {15, PMUX_INV, PMUX_INV } },  // PCMOUTLRCK | PD1PORT16
	{ {16, PMUX_INV, PMUX_INV } },  // PCMOUTBCLK | PD1PORT17
	{ {17, PMUX_INV, PMUX_INV } },  // UART1RX    | PD1PORT18
	{ {18, PMUX_INV, PMUX_INV } },  // UART1TX    | PD1PORT19
	{ {19,       71, PMUX_INV } },  // DDBGTDI    | SNDBGTDI  | PD1PORT20
	{ {20,       72, PMUX_INV } },  // DDBGTDO    | SNDBGTDO  | PD1PORT21
	{ {21,       73, PMUX_INV } },  // DDBGTMS    | SNDBGTMS  | PD1PORT22
	{ {22,       74, PMUX_INV } },  // DDBGTCK    | SNDBGTCK  | PD1PORT23
	{ {23,       75, PMUX_INV } },  // DDBGTRST   | SNDBGTRST | PD1PORT24
	{ {24,       76, PMUX_INV } },  // DUARTTX    | SNUARTTX  | PD1PORT25
	{ {25, PMUX_INV, PMUX_INV } },  // SDA0       | PD1PORT26
	{ {26, PMUX_INV, PMUX_INV } },  // SCL0       | PD1PORT27
	{ {27, PMUX_INV, PMUX_INV } },  // SDA1       | PD1PORT28
	{ {28, PMUX_INV, PMUX_INV } },  // SCL1       | PD1PORT29
	{ {29,       77, PMUX_INV } },  // PCM0INDAT1 | PDMDAT3   | PD1PORT30
	{ {30,       78, PMUX_INV } },  // PCM0INDAT0 | PDMDAT2   | PD1PORT31
	{ {31,       79, PMUX_INV } },  // PCM0INMCLK | PDMDAT1   | PD1PORT32
	{ {32,       80, PMUX_INV } },  // PCM0INLRCK | PDMDAT0   | PCM0OUTLRCK | PD1PORT33
	{ {33,       81, PMUX_INV } },  // PCM0INBCLK | PDMCLK    | PCM0OUTBCLK | PD1PORT34
	{ {34, PMUX_INV, PMUX_INV } },  // IR         | PD1PORT35
};


/**
 * nc_add_function() - adds a new function to the function list
 * @pnc: pnc driver instance
 * @np: device node of the mux entry
 * @name: name of the function
 * @vals: array of mux register value pairs used by the function
 * @nvals: number of mux register value pairs
 * @pgnames: array of pingroup names for the function
 * @npgnames: number of pingroup names
 */
static struct nc_function *nc_add_function(struct nc_device *pnc,
                    struct device_node *np,
                    const char *name,
                    struct nc_func_vals *vals,
                    unsigned nvals,
                    const char **pgnames,
                    unsigned npgnames)
{
    struct nc_function *function;

    function = devm_kzalloc(pnc->dev, sizeof(*function), GFP_KERNEL);
    if (!function)
        return NULL;

    function->name = name;
    function->vals = vals;
    function->nvals = nvals;
    function->pgnames = pgnames;
    function->npgnames = npgnames;

    mutex_lock(&pnc->mutex);
    list_add_tail(&function->node, &pnc->functions);
    radix_tree_insert(&pnc->ftree, pnc->nfuncs, function);
    pnc->nfuncs++;
    mutex_unlock(&pnc->mutex);

    return function;
}

/**
 * nc_add_pingroup() - add a pingroup to the pingroup list
 * @pnc: pnc driver instance
 * @np: device node of the mux entry
 * @name: name of the pingroup
 * @gpins: array of the pins that belong to the group
 * @ngpins: number of pins in the group
 */
static int nc_add_pingroup(struct nc_device *pnc,
                    struct device_node *np,
                    const char *name,
                    int *gpins,
                    int ngpins)
{
    struct nc_pingroup *pingroup;

    pingroup = devm_kzalloc(pnc->dev, sizeof(*pingroup), GFP_KERNEL);
    if (!pingroup)
        return -ENOMEM;

    pingroup->name = name;
    pingroup->np = np;
    pingroup->gpins = gpins;
    pingroup->ngpins = ngpins;

    mutex_lock(&pnc->mutex);
    list_add_tail(&pingroup->node, &pnc->pingroups);
    radix_tree_insert(&pnc->pgtree, pnc->ngroups, pingroup);
    pnc->ngroups++;
    mutex_unlock(&pnc->mutex);

    return 0;
}

static int nc_parse_one_pinctrl_entry(struct nc_device *pnc,
                        struct device_node *np,
                        struct pinctrl_map **map,
                        unsigned *num_maps,
                        const char **pgnames)
{
	struct nc_func_vals *vals;
	const __be32 *mux;
	int size, rows, *pins, index = 0, found = 0, res = -ENOMEM;
	struct nc_function *function;

	mux = of_get_property(np, NC_MUX_PINS_NAME, &size);
    if ((!mux) || (size < sizeof(*mux) * 2)) {
        dev_err(pnc->dev, "bad data for mux %s\n",
            np->name);
        return -EINVAL;
    }

    size /= sizeof(*mux);   /* Number of elements in array */
    rows = size / 2;

    vals = devm_kzalloc(pnc->dev, sizeof(*vals) * rows, GFP_KERNEL);
    if (!vals)
        return -ENOMEM;

    pins = devm_kzalloc(pnc->dev, sizeof(*pins) * rows, GFP_KERNEL);
    if (!pins)
        goto free_vals;

	while(index < size) {
		int pin;
		pin = be32_to_cpup(mux + index++);
		vals[found].pin = pin;
		vals[found].val = be32_to_cpup(mux + index++);
		pins[found++] = pin;
	}

	pgnames[0] = np->name;
	function = nc_add_function(pnc, np, np->name, vals, found, pgnames, 1);
	if (!function)
		goto free_pins;

	res = nc_add_pingroup(pnc, np, np->name, pins, found);
	if (res < 0)
		goto free_function;

	(*map)->type = PIN_MAP_TYPE_MUX_GROUP;
	(*map)->data.mux.group = np->name;
	(*map)->data.mux.function = np->name;
	// HAS_PIN_CONF?
	*num_maps = 1;
	return 0;
free_vals:
free_pins:
free_function:
	return 0;
}
/**
 * nc_dt_node_to_map() - allocates and parses pinctrl maps
 * @pctldev: pinctrl instance
 * @np_config: device tree pinmux entry
 * @map: array of map entries
 * @num_maps: number of maps
 */
static int nc_dt_node_to_map(struct pinctrl_dev *pctldev,
                struct device_node *np_config,
                struct pinctrl_map **map, unsigned *num_maps)
{
    struct nc_device *pnc;
    const char **pgnames;
    int ret;

    pnc = pinctrl_dev_get_drvdata(pctldev);

    /* create 2 maps. One is for pinmux, and the other is for pinconf. */
    *map = devm_kzalloc(pnc->dev, sizeof(**map) * 2, GFP_KERNEL);
    if (!*map)
        return -ENOMEM;

    *num_maps = 0;

    pgnames = devm_kzalloc(pnc->dev, sizeof(*pgnames), GFP_KERNEL);
    if (!pgnames) {
        ret = -ENOMEM;
        goto free_map;
    }

    ret = nc_parse_one_pinctrl_entry(pnc, np_config, map,
            num_maps, pgnames);
    if (ret < 0) {
        dev_err(pnc->dev, "no pins entries for %s\n",
            np_config->name);
        goto free_pgnames;
    }

    return 0;

free_pgnames:
    devm_kfree(pnc->dev, pgnames);
free_map:
    devm_kfree(pnc->dev, *map);

    return ret;
}


static int nc_pin_table_init(struct nc_device *pnc)
{
	int i;
    struct pinctrl_desc *pin_desc = &pnc->desc;

    dev_dbg(pnc->dev, "allocating %i pins\n", pin_desc->npins);
    pnc->pins = devm_kzalloc(pnc->dev,
                sizeof(*pin_desc->pins) * pin_desc->npins,
                GFP_KERNEL);
    if (!pnc->pins)
        return -ENOMEM;

	for(i = 0; i < pin_desc->npins; i++) {
		pnc->pins[i].number = i;
		pnc->pins[i].name = NULL;
		pnc->pins[i].drv_data = NULL;
	}
	pin_desc->pins = pnc->pins;
	return 0;
}

static int nc_get_groups_count(struct pinctrl_dev *pctldev)
{
    struct nc_device *pnc;

    pnc = pinctrl_dev_get_drvdata(pctldev);

    return pnc->ngroups;
}

static const char *nc_get_group_name(struct pinctrl_dev *pctldev,
                    unsigned gselector)
{
    struct nc_device *pnc;
    struct nc_pingroup *group;

    pnc = pinctrl_dev_get_drvdata(pctldev);
    group = radix_tree_lookup(&pnc->pgtree, gselector);
    if (!group) {
        dev_err(pnc->dev, "%s could not find pingroup%i\n",
            __func__, gselector);
        return NULL;
    }

    return group->name;
}

static int nc_get_group_pins(struct pinctrl_dev *pctldev,
                    unsigned gselector,
                    const unsigned **pins,
                    unsigned *npins)
{
    struct nc_device *pnc;
    struct nc_pingroup *group;

    pnc = pinctrl_dev_get_drvdata(pctldev);
    group = radix_tree_lookup(&pnc->pgtree, gselector);
    if (!group) {
        dev_err(pnc->dev, "%s could not find pingroup%i\n",
            __func__, gselector);
        return -EINVAL;
    }

    *pins = group->gpins;
    *npins = group->ngpins;

    return 0;
}

static void nc_pin_dbg_show(struct pinctrl_dev *pctldev,
                    struct seq_file *s,
                    unsigned pin)
{
    int table_size;
	struct nc_device *pnc;
    unsigned val = 0;
    int i;

	pnc = pinctrl_dev_get_drvdata(pctldev);
	table_size = pnc->is_ck ? ARRAY_SIZE(ck_padmux_table) : ARRAY_SIZE(padmux_table);

    if (pin >= table_size)
        return ;

    for (i = 0; i < 3; i++) {
        unsigned char sel = pnc->is_ck ? ck_padmux_table[pin].sel[i] : padmux_table[pin].sel[i];
        unsigned char f_bit;
        void __iomem *reg;
        u32 mask;

        if (sel == PMUX_INV)
            continue;

		reg = pnc->base + ((sel / 32) * 4);  // sel0,sel1,sel2对应的寄存器
		mask = (u32)(1 << (sel%32));	// selx对应寄存器的哪一位
		f_bit = !!(readl_relaxed(reg) & mask);
		pr_debug("show_dbg read %p, 0x%x: %d\n", reg, mask, f_bit);
		val |= (f_bit) ? (1 << i) : 0;
	}

	seq_printf(s, "%08x %s " , val, DRIVER_NAME);
}

static void nc_dt_free_map(struct pinctrl_dev *pctldev,
                struct pinctrl_map *map, unsigned num_maps)
{
    struct nc_device *pnc;

    pnc = pinctrl_dev_get_drvdata(pctldev);
    devm_kfree(pnc->dev, map);
}

static const struct pinctrl_ops nc_pinctrl_ops = {
    .get_groups_count = nc_get_groups_count,
    .get_group_name = nc_get_group_name,
    .get_group_pins = nc_get_group_pins,
    .pin_dbg_show = nc_pin_dbg_show,
    .dt_node_to_map = nc_dt_node_to_map,
    .dt_free_map = nc_dt_free_map,
};

static int nc_get_functions_count(struct pinctrl_dev *pctldev)
{
    struct nc_device *pnc;

    pnc = pinctrl_dev_get_drvdata(pctldev);

    return pnc->nfuncs;
}

static const char *nc_get_function_name(struct pinctrl_dev *pctldev,
                        unsigned fselector)
{
    struct nc_device *pnc;
    struct nc_function *func;

    pnc = pinctrl_dev_get_drvdata(pctldev);
    func = radix_tree_lookup(&pnc->ftree, fselector);
    if (!func) {
        dev_err(pnc->dev, "%s could not find function%i\n",
            __func__, fselector);
        return NULL;
    }

    return func->name;
}

static int nc_get_function_groups(struct pinctrl_dev *pctldev,
                    unsigned fselector,
                    const char * const **groups,
                    unsigned * const ngroups)
{
    struct nc_device *pnc;
    struct nc_function *func;

    pnc = pinctrl_dev_get_drvdata(pctldev);
    func = radix_tree_lookup(&pnc->ftree, fselector);
    if (!func) {
        dev_err(pnc->dev, "%s could not find function%i\n",
            __func__, fselector);
        return -EINVAL;
    }
    *groups = func->pgnames;
    *ngroups = func->npgnames;

    return 0;
}


void nc_reg_rmw(void __iomem *reg, u32 mask, bool set)
{
    int l = readl_relaxed(reg);

    if (set)
        l |= mask;
    else
        l &= ~mask;

    writel_relaxed(l, reg);
}

int nc_padmux_set(struct nc_device *pnc, unsigned pin, int function)
{
	int table_size = pnc->is_ck ? ARRAY_SIZE(ck_padmux_table) : ARRAY_SIZE(padmux_table);
	int i;

	dev_dbg(pnc->dev, "write pin:%d, %d\n", pin, function);

	if(pin >= table_size)
		return -1;

	for (i = 0; i < 3; i++) {
        unsigned char sel = pnc->is_ck ? ck_padmux_table[pin].sel[i] : padmux_table[pin].sel[i];
		unsigned char f_bit;
		void __iomem *reg;
		u32 mask;

		if (sel == PMUX_INV)
			continue;

		f_bit = (function >> i) & 0x01;
		reg = pnc->base + ((sel / 32) * 4);	// sel0,sel1,sel2对应的寄存器
		mask = (u32)(1 << (sel%32));
		pr_debug("nc_reg_rmw %p, 0x%x, %d\n", reg, mask, f_bit);
		nc_reg_rmw(reg, mask, f_bit);
	}
	return 0;
}
static int nc_set_mux(struct pinctrl_dev *pctldev, unsigned fselector,
    unsigned group)
{
    struct nc_device *pnc;
    struct nc_function *func;
    int i;

    pnc = pinctrl_dev_get_drvdata(pctldev);

    func = radix_tree_lookup(&pnc->ftree, fselector);
    if (!func)
        return -EINVAL;

    dev_dbg(pnc->dev, "enabling %s function%i\n",
        func->name, fselector);

    for (i = 0; i < func->nvals; i++) {
        struct nc_func_vals *vals;

        vals = &func->vals[i];
		/*写寄存器*/
		nc_padmux_set(pnc, vals->pin, vals->val);
    }

    return 0;
}

static int nc_request_gpio(struct pinctrl_dev *pctldev,
                struct pinctrl_gpio_range *range, unsigned pin)
{
    struct nc_device *pnc = pinctrl_dev_get_drvdata(pctldev);
    struct nc_gpiofunc_range *frange = NULL;
    struct list_head *pos, *tmp;

	printk("zj test \n");
    list_for_each_safe(pos, tmp, &pnc->gpiofuncs) {
        frange = list_entry(pos, struct nc_gpiofunc_range, node);
        if (pin >= frange->offset + frange->npins
            || pin < frange->offset)
            continue;
		nc_padmux_set(pnc, pin, frange->gpiofunc);
        break;
    }
    return 0;
}

static const struct pinmux_ops nc_pinmux_ops = {
    .get_functions_count = nc_get_functions_count,
    .get_function_name = nc_get_function_name,
    .get_function_groups = nc_get_function_groups,
    .set_mux = nc_set_mux,
	.gpio_request_enable = nc_request_gpio,
};

static int nc_add_gpio_func(struct device_node *node, struct nc_device *pnc)
{
    const char *propname = "pinctrl-nc,gpio-range";
    const char *cellname = "#pinctrl-nc,gpio-range-cells";
    struct of_phandle_args gpiospec;
    struct nc_gpiofunc_range *range;
    int ret, i;

    for (i = 0; ; i++) {
        ret = of_parse_phandle_with_args(node, propname, cellname,
                         i, &gpiospec);
        /* Do not treat it as error. Only treat it as end condition. */
        if (ret) {
            ret = 0;
            break;
        }
        range = devm_kzalloc(pnc->dev, sizeof(*range), GFP_KERNEL);
        if (!range) {
            ret = -ENOMEM;
            break;
        }
        range->offset = gpiospec.args[0];
        range->npins = gpiospec.args[1];
        range->gpiofunc = gpiospec.args[2];
        mutex_lock(&pnc->mutex);
        list_add_tail(&range->node, &pnc->gpiofuncs);
        mutex_unlock(&pnc->mutex);
    }
    return ret;
}

static int nc_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct nc_device *pnc;
	struct resource *res;
	int ret;

    pnc = devm_kzalloc(&pdev->dev, sizeof(*pnc), GFP_KERNEL);
    if (!pnc) {
        dev_err(&pdev->dev, "could not allocate\n");
        return -ENOMEM;
    }
    pnc->dev = &pdev->dev;
	raw_spin_lock_init(&pnc->lock);
    mutex_init(&pnc->mutex);
    INIT_LIST_HEAD(&pnc->pingroups);
    INIT_LIST_HEAD(&pnc->functions);
	INIT_LIST_HEAD(&pnc->gpiofuncs);

    res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    if (!res) {
        dev_err(pnc->dev, "could not get resource\n");
        return -ENODEV;
    }

    pnc->res = devm_request_mem_region(pnc->dev, res->start,
            resource_size(res), DRIVER_NAME);
    if (!pnc->res) {
        dev_err(pnc->dev, "could not get mem_region\n");
        return -EBUSY;
    }

    pnc->size = resource_size(pnc->res);
    pnc->base = devm_ioremap(pnc->dev, pnc->res->start, pnc->size);
    if (!pnc->base) {
        dev_err(pnc->dev, "could not ioremap\n");
        return -ENODEV;
    }

	if (of_find_property(np, "nc,in-csky-domain", NULL))
		pnc->is_ck = 1;
	else
		pnc->is_ck = 0;

    INIT_RADIX_TREE(&pnc->pgtree, GFP_KERNEL);
    INIT_RADIX_TREE(&pnc->ftree, GFP_KERNEL);
	platform_set_drvdata(pdev, pnc);

    pnc->desc.name = DRIVER_NAME;
    pnc->desc.pctlops = &nc_pinctrl_ops;
    pnc->desc.pmxops = &nc_pinmux_ops;
    pnc->desc.owner = THIS_MODULE;
	pnc->desc.npins = pnc->is_ck ? ARRAY_SIZE(ck_padmux_table) : ARRAY_SIZE(padmux_table);

    ret = nc_pin_table_init(pnc);
    if (ret < 0)
		return -EINVAL;

    pnc->pctl = pinctrl_register(&pnc->desc, pnc->dev, pnc);
    if (IS_ERR(pnc->pctl)) {
        dev_err(pnc->dev, "could not register single pinctrl driver\n");
        ret = PTR_ERR(pnc->pctl);
		return -EINVAL;
    }

	ret = nc_add_gpio_func(np, pnc);
	if (ret < 0) {
		pinctrl_unregister(pnc->pctl);
		return ret;
	}

	dev_info(pnc->dev, "%i pins at pa %p size %u\n",
		pnc->desc.npins, pnc->base, pnc->size);

	return 0;
}
static int nc_remove(struct platform_device *pdev)
{
	struct nc_device *pnc = platform_get_drvdata(pdev);
	if (!pnc)
		return 0;

	pinctrl_unregister(pnc->pctl);
	return 0;
}

static const struct of_device_id nc_of_match[] = {
    { .compatible =  "NationalChip-pinctrl" },
    { },
};
MODULE_DEVICE_TABLE(of, nc_of_match);

static struct platform_driver nc_driver = {
    .probe      = nc_probe,
    .remove     = nc_remove,
    .driver = {
        .name       = DRIVER_NAME,
        .of_match_table = nc_of_match,
    },
};

module_platform_driver(nc_driver);

MODULE_AUTHOR("zhangjun");
MODULE_DESCRIPTION("pinctrl driver for national chip");
MODULE_LICENSE("GPL v2");

