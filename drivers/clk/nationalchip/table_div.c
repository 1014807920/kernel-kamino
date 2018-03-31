#include <linux/clk-provider.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/slab.h>
#include "nationalchip_clk.h"


static void __init nationalchip_leo_table_div_setup(struct device_node *node)
{
	struct clk *clk;
	const char *clk_name = node->name, *parent_name;
	void __iomem *reg = NULL;
	u32 index, ratio_num, div_num, shift, width;
	struct clk_div_table *table;

    if (of_property_read_u32(node, "div_shift", &shift))
        return ;
    if (of_property_read_u32(node, "div_width", &width))
        return ;

	ratio_num = of_property_count_u32_elems(node, "clk_ratio");
	div_num   = of_property_count_u32_elems(node, "clk_div");
	if (ratio_num != div_num){
		pr_err("%s: ratio and div do not match\n", node->name);
	}

	reg = clk_iomap(node, 0, of_node_full_name(node));
	if (IS_ERR(reg)){
		pr_err("%s: failed to iomap\n", node->name);
		return;
	}

	table = kzalloc(sizeof(*table) * (ratio_num + 1), GFP_KERNEL);
	if (!table){
        pr_err("%s: failed to kzalloc\n", node->name);
		return;
		goto err_unmap;
    }

	for (index = 0; index < ratio_num; index++){
		of_property_read_u32_index(node, "clk_div", index, &table[index].div);
		of_property_read_u32_index(node, "clk_ratio", index, &table[index].val);
	}
	table[index].div = 0;
	table[index].val = 0;

	of_property_read_string(node, "clock-output-names", &clk_name);
	parent_name = of_clk_get_parent_name(node, 0);

	clk = clk_register_divider_table(NULL, clk_name, parent_name,
					CLK_GET_RATE_NOCACHE, reg, shift, width, CLK_DIVIDER_ALLOW_ZERO, table,
					NULL);

	if (!IS_ERR(clk))
		of_clk_add_provider(node, of_clk_src_simple_get, clk);
	return;

err_unmap:
    iounmap(reg);

}

CLK_OF_DECLARE(nationalchip_leo_table_div, "nationalchip,leo_table_div",
		   nationalchip_leo_table_div_setup);
