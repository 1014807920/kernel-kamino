#ifndef __NATIONALCHIP_CLK_H
#define __NATIONALCHIP_CLK_H

#include <linux/clk-provider.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/slab.h>
#include <linux/spinlock.h>

struct clk_multi_gate {
	struct clk_hw hw;
	void __iomem    *reg;
	u8      bit_idx;
	u8      width;
	u8      flags;
	spinlock_t  *lock;
};


struct clk_dto {
	struct clk_hw   hw;
	void __iomem    *reg;
	u8      shift;
	u8      width;
	spinlock_t  *lock;
};

struct clk_pll {
	struct clk_hw   hw;
	void __iomem    *reg;
	u8      shift;
	u8      width;
	u8      flags;
	spinlock_t  *lock;
};

struct nationalchip_clk_divider {
	struct clk_hw	hw;
	void __iomem	*reg;
	void __iomem	*enable_reg;
	u8		enable_shift;
	u8		shift;
	u8		width;
	u8		flags;
	u8		div_base;
	spinlock_t	*lock;
};


#define NATIONALCHIP_MAX_PARENTS		5
#define NATIONALCHIP_MUX_WIDTH		1

#define to_clk_gate(_hw) container_of(_hw, struct clk_multi_gate, hw)
#define to_clk_dto(_hw) container_of(_hw, struct clk_dto, hw)
#define to_clk_pll(_hw) container_of(_hw, struct clk_pll, hw)
#define to_nationalchip_clk_divider(_hw) container_of(_hw, struct nationalchip_clk_divider, hw)

#define div_mask(width) ((1 << (width)) - 1)


void __iomem *clk_iomap(struct device_node *node, int index,
                    const char *name);
#endif

