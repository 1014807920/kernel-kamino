#include <linux/clk-provider.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/slab.h>
#include "nationalchip_clk.h"

static void __init nationalchip_leo_mux_setup(struct device_node *node)
{
	struct clk *clk;
	const char *clk_name = node->name;
	const char *parents[NATIONALCHIP_MAX_PARENTS];
	u8 parent_num = 0;
	int mux_offset;
	void __iomem *reg;

	if (of_property_read_u32(node, "mux_offset", &mux_offset)){
		printk("%s don't get mux_offset\n", node->name);
		return ;
	}

	parent_num = of_clk_parent_fill(node, parents, NATIONALCHIP_MAX_PARENTS);
	if (!parent_num){
		printk("%s don't get parents\n", node->name);
		return ;
	}

	if (of_property_read_string(node, "clock-output-names", &clk_name)){
		printk("%s don't get clock-output-names\n", node->name);
		return ;
	}

	reg = clk_iomap(node, 0, of_node_full_name(node));
	if (IS_ERR(reg)){
		pr_err("%s: failed to iomap\n", node->name);
		return;
	}

	clk = clk_register_mux(NULL, clk_name, parents, parent_num,
			       CLK_SET_RATE_PARENT | CLK_SET_RATE_NO_REPARENT, reg,
			       mux_offset, NATIONALCHIP_MUX_WIDTH,
			       0, NULL);

	if (clk) {
		of_clk_add_provider(node, of_clk_src_simple_get, clk);
	}
}

CLK_OF_DECLARE(nationalchip_leo_mux, "nationalchip,leo_mux",
	       nationalchip_leo_mux_setup);
