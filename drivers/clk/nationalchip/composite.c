
#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/math64.h>
#include "nationalchip_clk.h"

static void clk_gate_endisable(struct clk_hw *hw, int enable)
{
	struct clk_multi_gate *gate = to_clk_gate(hw);
	u32 mask = BIT(gate->width) - 1;
	unsigned long uninitialized_var(flags);
	u32 reg;

	if (gate->lock)
		spin_lock_irqsave(gate->lock, flags);
	else
		__acquire(gate->lock);

	reg = clk_readl(gate->reg);

	if (enable)
		reg &= ~(mask << gate->bit_idx);
	else
		reg |= mask << gate->bit_idx;

	clk_writel(reg, gate->reg);

	if (gate->lock)
		spin_unlock_irqrestore(gate->lock, flags);
	else
		__release(gate->lock);
}

static int clk_gate_enable(struct clk_hw *hw)
{
	clk_gate_endisable(hw, 1);

	return 0;
}

static void clk_gate_disable(struct clk_hw *hw)
{
	clk_gate_endisable(hw, 0);
}

static int clk_gate_is_enabled(struct clk_hw *hw)
{
	u32 reg;
	struct clk_multi_gate *gate = to_clk_gate(hw);
	u32 mask = BIT(gate->width) - 1;

	reg = (clk_readl(gate->reg) >> gate->bit_idx) & mask;

	return reg == mask ? 0 : 1;
}

const struct clk_ops clk_multi_gate_ops = {
	.enable = clk_gate_enable,
	.disable = clk_gate_disable,
	.is_enabled = clk_gate_is_enabled,
};


/**
 * Register a clock branch.
 * Most clock branches have a form like
 *
 * src1 --|--\
 *        |M |--[DIV]-[GATE]-
 * src2 --|--/
 *
 * sometimes without one of those components.
 */
static struct clk *nationalchip_clk_register_branch(const char *name,
		const char *const *parent_names, u8 num_parents,
		void __iomem *mux_reg, u8 mux_shift, u8 mux_width, u8 mux_flags,
		u8 fix_div, u8 fix_mult,
		void __iomem *gate_reg, u8 gate_offset, u8 gate_width, u8 gate_flags,
		unsigned long flags, spinlock_t *lock)
{
	struct clk *clk;
	struct clk_mux *mux = NULL;
	struct clk_gate *gate = NULL;
	struct clk_multi_gate *multi_gate = NULL;
	struct clk_fixed_factor *fix = NULL;
	const struct clk_ops *mux_ops = NULL, *fix_ops = NULL,
			     *gate_ops = NULL;

	if (num_parents > 1) {
		mux = kzalloc(sizeof(*mux), GFP_KERNEL);
		if (!mux)
			return ERR_PTR(-ENOMEM);

		mux->reg = mux_reg;
		mux->shift = mux_shift;
		mux->mask = BIT(mux_width) - 1;
		mux->flags = mux_flags;
		mux->lock = lock;
		mux_ops = (mux_flags & CLK_MUX_READ_ONLY) ? &clk_mux_ro_ops
							: &clk_mux_ops;
	}

	if (fix_mult != 1 || fix_div != 1) {
		fix = kzalloc(sizeof(*fix), GFP_KERNEL);
		if (!fix)
			goto err_div;

		fix->mult = fix_mult;
		fix->div = fix_div;
		fix_ops = &clk_fixed_factor_ops;
	}

	if (gate_offset >= 0) {
		if (gate_width == 1){
			gate = kzalloc(sizeof(struct clk_gate), GFP_KERNEL);
			if (!gate)
				goto err_gate;

			gate->flags = gate_flags;
			gate->reg = gate_reg;
			gate->bit_idx = gate_offset;
			gate->lock = lock;
			gate_ops = &clk_gate_ops;
		}
		else{
			multi_gate = kzalloc(sizeof(struct clk_multi_gate), GFP_KERNEL);
			if (!multi_gate)
				goto err_gate;
			multi_gate->flags = gate_flags;
			multi_gate->reg = gate_reg;
			multi_gate->bit_idx = gate_offset;
			multi_gate->lock = lock;
			gate_ops = &clk_multi_gate_ops;
			multi_gate->width = gate_width;
		}
	}

	if (gate_width == 1){
		clk = clk_register_composite(NULL, name, parent_names, num_parents,
						mux ? &mux->hw : NULL, mux_ops,
						fix ? &fix->hw : NULL, fix_ops,
						gate ? &gate->hw : NULL, gate_ops,
						flags);
	}
	else{
		clk = clk_register_composite(NULL, name, parent_names, num_parents,
						mux ? &mux->hw : NULL, mux_ops,
						fix ? &fix->hw : NULL, fix_ops,
						multi_gate ? &multi_gate->hw : NULL, gate_ops,
						flags);
	}

	return clk;
err_gate:
	kfree(fix);
err_div:
	kfree(mux);
	return ERR_PTR(-ENOMEM);
}

static void __init nationalchip_leo_composite_setup(struct device_node *node)
{
	struct clk *clk;
	const char *clk_name = node->name;
	const char *parents[NATIONALCHIP_MAX_PARENTS];
	void __iomem *mux_reg;
	void __iomem *gate_reg;
	u32 mux_offset, clock_div, clock_mult, gate_bit, gate_width, parent_num = 0;

	parent_num = of_clk_parent_fill(node, parents, NATIONALCHIP_MAX_PARENTS);
	if (!parent_num){
		printk("%s don't get parents\n", node->name);
		return ;
	}

	mux_reg = clk_iomap(node, 0, of_node_full_name(node));
	if (IS_ERR(mux_reg))
		return;

	gate_reg = clk_iomap(node, 1, of_node_full_name(node));
	if (IS_ERR(gate_reg))
		goto gate_reg;

	if (of_property_read_string(node, "clock-output-names", &clk_name)){
		printk("%s don't get clock-output-names\n", node->name);
		goto free;
	}
	if(of_property_read_u32(node, "mux_offset", &mux_offset))
		goto free;
	if(of_property_read_u32(node, "clock_div", &clock_div))
		goto free;
	if(of_property_read_u32(node, "clock_mult", &clock_mult))
		goto free;
	if(of_property_read_u32(node, "gate_bit", &gate_bit))
		goto free;
	if(of_property_read_u32(node, "gate_width", &gate_width))
		gate_width = 1;

	clk = nationalchip_clk_register_branch(clk_name, parents, parent_num,
		mux_reg, mux_offset, NATIONALCHIP_MUX_WIDTH, 0,
		clock_div, clock_mult,
		gate_reg, gate_bit, gate_width, CLK_GATE_SET_TO_DISABLE,
		CLK_SET_RATE_PARENT | CLK_SET_RATE_NO_REPARENT, NULL);

	if (!IS_ERR(clk)){
		of_clk_add_provider(node, of_clk_src_simple_get, clk);
	}else
		goto free;

	return ;

free:
	iounmap(mux_reg);
gate_reg:
	iounmap(gate_reg);
}

CLK_OF_DECLARE(nationalchip_leo_composite, "nationalchip,leo_composite",
	       nationalchip_leo_composite_setup);

