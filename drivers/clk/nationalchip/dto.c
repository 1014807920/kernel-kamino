#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/math64.h>
#include "nationalchip_clk.h"

#define DTO_DIV_SHIFT	30
#define DTO_STEP		0x3fffffff
#define DTO_LOAD		30
#define DTO_RESET		31

static unsigned long clk_dto_recalc_rate(struct clk_hw *hw,
		unsigned long parent_rate)
{
	struct clk_dto *dto = to_clk_dto(hw);
	unsigned long long rate;
	u32 data;

	data = clk_readl(dto->reg) & DTO_STEP;
	rate = parent_rate;
	rate = rate * data;
	rate >>= DTO_DIV_SHIFT;

	return (unsigned long)rate;

}
static long clk_dto_round_rate(struct clk_hw *hw, unsigned long user_rate,
				unsigned long *prate)
{
	unsigned long long rate;
	u32 mult;

	rate = mul_u64_u64_shr((1ULL << DTO_DIV_SHIFT), (u64)user_rate, 0);
	do_div(rate, *prate);
	mult = (rate & DTO_STEP);
	rate = (u64)*prate * mult;
	rate = rate >> DTO_DIV_SHIFT;

	return (unsigned long)rate;

}
static int clk_dto_set_rate(struct clk_hw *hw, unsigned long user_rate,
				unsigned long parent_rate)
{
	struct clk_dto *dto = to_clk_dto(hw);
	unsigned long long int rate;
	u32 mult;

	rate = mul_u64_u64_shr((u64)(1 << DTO_DIV_SHIFT), (u64)user_rate, 0);
	do_div(rate, parent_rate);
	mult = (rate & DTO_STEP);

	clk_writel(1 << DTO_RESET, dto->reg);
	clk_writel(0, dto->reg);
	clk_writel(1 << DTO_RESET, dto->reg);

	clk_writel((1 << DTO_RESET) | mult, dto->reg);
	clk_writel((1 << DTO_RESET) | (1 << DTO_LOAD) | mult, dto->reg);
	clk_writel((1 << DTO_RESET) | (1 << DTO_LOAD) | mult, dto->reg);
	clk_writel((1 << DTO_RESET) | mult, dto->reg);

	return 0;
}

const struct clk_ops dto_ops = {
	.recalc_rate = clk_dto_recalc_rate,
	.round_rate = clk_dto_round_rate,
	.set_rate = clk_dto_set_rate,
};

static struct clk *register_dto(struct device *dev, const char *name,
		const char * const *parent_name, u8 parents_num, unsigned long flags,
		void __iomem *reg, spinlock_t *lock)
{
	struct clk_dto *dto;
	struct clk *clk;
	struct clk_init_data init;

	/* allocate the dto */
	dto = kzalloc(sizeof(*dto), GFP_KERNEL);
	if (!dto)
		return ERR_PTR(-ENOMEM);

	init.name = name;
	init.ops = &dto_ops;
	init.flags = flags | CLK_IS_BASIC;
	init.parent_names = (parent_name ? parent_name: NULL);
	init.num_parents = parents_num;

	/* struct clk_dto assignments */
	dto->reg = reg;
	dto->lock = lock;
	dto->hw.init = &init;

	/* register the clock */
	clk = clk_register(dev, &dto->hw);

	if (IS_ERR(clk))
		kfree(dto);

	return clk;
}

static void __init nationalchip_leo_dto_setup(struct device_node *node)
{
	struct clk_onecell_data *clk_data;
	struct device_node *np = node;
	const char *clk_name = np->name;
	struct property *prop;
	const __be32 *p;
	const char *parents[NATIONALCHIP_MAX_PARENTS];
	void __iomem *reg;
	void __iomem *clk_reg;
	u8 i = 0, parent_num = 0;
	u32 index, clk_num = 0;

	parent_num = of_clk_parent_fill(node, parents, NATIONALCHIP_MAX_PARENTS);
	if (!parent_num){
		printk("%s don't get parents\n", node->name);
		return ;
	}

	reg = clk_iomap(node, 0, of_node_full_name(node));
	if (IS_ERR(reg))
		return;

	clk_num = of_property_count_u32_elems(node, "clock-indices");

	clk_data = kzalloc(sizeof(struct clk_onecell_data), GFP_KERNEL);
	if (!clk_data){
		pr_err("%s: failed to kzalloc\n", node->name);
		goto err_unmap;
	}

	clk_data->clks = kcalloc(clk_num, sizeof(struct clk *), GFP_KERNEL);
	if (!clk_data->clks){
		pr_err("%s: failed to kcalloc\n", node->name);
		goto err_free_data;
	}

	of_property_for_each_u32(node, "clock-indices", prop, p, index) {
		of_property_read_string_index(node, "clock-output-names",
				i, &clk_name);

		clk_reg = reg + 4 * (index - 1);
		clk_data->clks[i] = register_dto(NULL, clk_name,
				parents, parent_num,
				0, clk_reg, NULL);

		i++;
		if (IS_ERR(clk_data->clks[index])) {
			WARN_ON(true);
			continue;
		}
	}

	clk_data->clk_num = clk_num;
	of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);
	return ;

err_free_data:
	kfree(clk_data);
err_unmap:
	iounmap(reg);
}

CLK_OF_DECLARE(nationalchip_leo_dto, "nationalchip,leo_dto",
	       nationalchip_leo_dto_setup);
