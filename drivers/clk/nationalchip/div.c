#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/of.h>
#include "nationalchip_clk.h"

static unsigned long nationalchip_clk_divider_recalc_rate(struct clk_hw *hw,
		unsigned long parent_rate)
{
	struct nationalchip_clk_divider *divider = to_nationalchip_clk_divider(hw);
	unsigned int val;

	val = clk_readl(divider->reg) >> divider->shift;
	val &= div_mask(divider->width);
	// 芯片设计时0和1都是2分频
	if (val == 0 && divider->enable_reg == NULL)
		val = 1;

	do_div(parent_rate, val + divider->div_base);
	return parent_rate;
}
static long nationalchip_clk_divider_round_rate(struct clk_hw *hw, unsigned long user_rate, unsigned long *parent_rate)
{
	struct nationalchip_clk_divider *divider = to_nationalchip_clk_divider(hw);
	unsigned long long rate = (u64)*parent_rate;
	unsigned long now, max_div, min_div, best = 0;
	int i, bestdiv = 0;

	min_div = 1 + divider->div_base;
	max_div = div_mask(divider->width) + divider->div_base;

	for (i = min_div; i < max_div; i++){
		now = DIV_ROUND_UP_ULL((u64)rate, i);
		if (now <= user_rate && now > best){
			bestdiv = i;
			best = now;
		}
	}

	if (divider->enable_reg != NULL && (user_rate == rate))
		best = user_rate;

	return best;
}

static int nationalchip_clk_divider_set_rate(struct clk_hw *hw, unsigned long user_rate,
				unsigned long parent_rate)
{
	struct nationalchip_clk_divider *divider = to_nationalchip_clk_divider(hw);
	unsigned long long rate = parent_rate;
	unsigned long div;
	unsigned int data;

	do_div(rate, user_rate);
	div = rate - divider->div_base;
	if (div == 0 && divider->enable_reg != NULL){
		data  = clk_readl(divider->enable_reg);
		data &= ~1 << divider->enable_shift;
		clk_writel(data, divider->enable_reg);
	}else if (div != 0 && divider->enable_reg != NULL){
		data  = clk_readl(divider->enable_reg);
		data &= ~1 << divider->enable_shift;
		clk_writel(data, divider->enable_reg);
		data |= (1 << divider->enable_shift);
		clk_writel(data, divider->enable_reg);
	}else if (div == 0 && divider->enable_reg == NULL){
		printk("This frequency is not supported\n");
		return -1;
	}
	data = clk_readl(divider->reg);
	// div 需要先将reset这bit置0置1
	clk_writel(data & ~(1 << (divider->shift + divider->width + 1)), divider->reg);
	clk_writel(data |  (1 << (divider->shift + divider->width + 1)), divider->reg);

	data = clk_readl(divider->reg);
	clk_writel(data & ~(div_mask(divider->width) << divider->shift), divider->reg);
	data = clk_readl(divider->reg);
	clk_writel(data | (div << divider->shift), divider->reg);

	data = clk_readl(divider->reg);
	clk_writel(data & ~(1 << (divider->shift + divider->width)), divider->reg);
	clk_writel(data |  (1 << (divider->shift + divider->width)), divider->reg);

	return 0;
}

const struct clk_ops divider_ops = {
	.recalc_rate = nationalchip_clk_divider_recalc_rate,
	.round_rate = nationalchip_clk_divider_round_rate,
	.set_rate = nationalchip_clk_divider_set_rate,
};

static struct clk *register_divider(struct device *dev, const char *name,
		const char *parent_name, unsigned long flags,
		void __iomem *enable_reg, u8 enable_shift,
		void __iomem *reg, u8 shift, u8 width, u8 div_base,
		u8 clk_divider_flags, spinlock_t *lock)
{
	struct nationalchip_clk_divider *div;
	struct clk *clk;
	struct clk_init_data init;

	if (clk_divider_flags & CLK_DIVIDER_HIWORD_MASK) {
		if (width + shift > 16) {
			pr_warn("divider value exceeds LOWORD field\n");
			return ERR_PTR(-EINVAL);
		}
	}

	/* allocate the divider */
	div = kzalloc(sizeof(*div), GFP_KERNEL);
	if (!div)
		return ERR_PTR(-ENOMEM);

	init.name = name;
	init.ops = &divider_ops;
	init.flags = flags | CLK_IS_BASIC;
	init.parent_names = (parent_name ? &parent_name: NULL);
	init.num_parents = (parent_name ? 1 : 0);

	/* struct nationalchip_clk_divder assignments */
	div->reg        = reg;
	div->enable_reg = enable_reg;
	div->enable_shift = enable_shift;
	div->shift      = shift;
	div->width      = width;
	div->div_base   = div_base;
	div->flags      = clk_divider_flags;
	div->lock       = lock;
	div->hw.init    = &init;

	/* register the clock */
	clk = clk_register(dev, &div->hw);

	if (IS_ERR(clk))
		kfree(div);

	return clk;
}

static void __init nationalchip_leo_divider_setup(struct device_node *node)
{
	struct clk *clk;
	const char *clk_name = node->name;
	const char *clk_parent;
	void __iomem *reg = NULL;
	void __iomem *enable_reg = NULL;
	u32 shift, width, base, enable_shift;
	int reg_num;
	u8 flags = 0;

	clk_parent = of_clk_get_parent_name(node, 0);

	if (of_property_read_string(node, "clock-output-names", &clk_name))
		return ;
	if (of_property_read_u32(node, "div_shift", &shift))
		return ;
	if (of_property_read_u32(node, "div_width", &width))
		return ;
	if (of_property_read_u32(node, "div_base",  &base))
		return ;
	of_property_read_u32(node, "enable_shift",  &enable_shift);

	reg = clk_iomap(node, 0, of_node_full_name(node));
	if (IS_ERR(reg))
		return;
	reg_num = of_property_count_elems_of_size(node, "reg", sizeof(int));
	if (reg_num == 4){
		enable_reg = clk_iomap(node, 1, of_node_full_name(node));
		flags |= CLK_DIVIDER_ALLOW_ZERO;
	}

	clk = register_divider(NULL, clk_name, clk_parent,
							CLK_GET_RATE_NOCACHE, enable_reg, enable_shift,
							reg, shift, width, base,
							0, NULL);

	if (clk) {
		of_clk_add_provider(node, of_clk_src_simple_get, clk);
	}
}

CLK_OF_DECLARE(nationalchip_leo_divider, "nationalchip,leo_divider",
	       nationalchip_leo_divider_setup);
