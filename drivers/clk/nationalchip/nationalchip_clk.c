#include "nationalchip_clk.h"
void __iomem *clk_iomap(struct device_node *node, int index,
                    const char *name)
{
    void __iomem *mem;
    u32 start, length;

	if (of_property_read_u32_index(node, "reg", 2 * index, &start)) {
		pr_err("%s must have reg[%d]!\n", node->name, index);
		return IOMEM_ERR_PTR(-EINVAL);
	}
	if (of_property_read_u32_index(node, "reg", 2 * index + 1, &length)) {
		pr_err("%s must have reg[%d]!\n", node->name, index + 1);
		return IOMEM_ERR_PTR(-EINVAL);
	}

    mem = ioremap(start, length);
    if (!mem) {
		printk("%s don't iomap %#x\n", node->name, start);
        return IOMEM_ERR_PTR(-ENOMEM);
    }

    return mem;
}

