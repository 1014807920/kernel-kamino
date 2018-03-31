#include <linux/clk-provider.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/slab.h>
#include "nationalchip_clk.h"


static void __init nationalchip_leo_gate_setup(struct device_node *node)
{
	struct clk *clk;
	const char *clk_name = node->name, *parent_name;
	void __iomem *reg = NULL;
	u32 gate_bit;

	if (of_property_read_u32(node, "gate_bit", &gate_bit)){
		printk("%s don't get gate_bit\n", node->name);
		return ;
	}

	reg = clk_iomap(node, 0, of_node_full_name(node));
	if (IS_ERR(reg)){
		pr_err("%s: failed to iomap\n", node->name);
		return;
	}

	of_property_read_string(node, "clock-output-names", &clk_name);
	parent_name = of_clk_get_parent_name(node, 0);

	clk = clk_register_gate(NULL, clk_name, parent_name,
				CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE, reg,
				gate_bit, CLK_GATE_SET_TO_DISABLE, NULL);
	if (!IS_ERR(clk))
		of_clk_add_provider(node, of_clk_src_simple_get, clk);
}

CLK_OF_DECLARE(nationalchip_leo_gate, "nationalchip,leo_gate",
		   nationalchip_leo_gate_setup);
