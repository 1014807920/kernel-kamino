#include <linux/clk-provider.h>
#include <linux/clk.h>
#include <linux/module.h>
#include <linux/of.h>
#include "nationalchip_clk.h"

#define PGN40LP25F2000

#ifdef PGN40LP25F2000
#define BS_RANGE 1
#define OD_RANGE 4
#define R_RANGE  32
#define F_RANGE  128
#define FOUT_MIN_LOW 125000000
#define FOUT_MAX_LOW 2000000000
#define FVCO_MIN_LOW 1000000000
#define FVCO_MAX_LOW 2000000000
#define FOUT_MIN_HIG 125000000
#define FOUT_MAX_HIG 2000000000
#define FVCO_MIN_HIG 1000000000
#define FVCO_MAX_HIG 2000000000
#define FREF_MIN     10000000
#define FREF_MAX     50000000

#define CLK_GET(x, mask, shift)       (((x) & ((mask) << (shift))) >> (shift))

#define CLK_SET(x, mask, shift)       (((x) & (mask)) << (shift))

#define CLK_CON_SHIFT 14
#define CLK_OD_SHIFT 12
#define CLK_N_SHIFT  7
#define CLK_M_SHIFT  0

#define CLK_MASK_CON
#define CLK_MASK_OD 0x3
#define CLK_MASK_N  0x1f
#define CLK_MASK_M  0x7f

#endif


int pll_round_rate(int *BS, int *OD, int *R, int *F, unsigned long FIN,  unsigned long *FOUT_NEAR){
	int NR, NF, NO, bs, od, r, f;
	unsigned long FREF, DIFF, RATE;
	unsigned long long FVCO, FOUT;
	RATE = *FOUT_NEAR;
	FVCO = 0;
	FOUT = 0;
	DIFF = 1 << 31;
	FREF = 0;
	for(*BS=0;*BS<BS_RANGE;*BS=*BS+1){
		for(*OD=0;*OD<OD_RANGE;*OD=*OD+1){
			for(*R=0;*R<R_RANGE;*R=*R+1){
				for(*F=0;*F<F_RANGE;*F=*F+1){
					NR = *R + 1 ;
					NO = 1 << *OD;
					NF = 2*(*F + 1) ;
					FOUT = FIN * NF;
					do_div(FOUT, NR * NO);
					/*FOUT = ((FIN*NF) / (NR*NO));*/
					FVCO = FOUT * NO;
					FREF = FIN;
					do_div(FREF, NR);

					/*FREF = FIN / NR;*/
					if((FREF <= FREF_MAX)&&(FREF >= FREF_MIN)){
						if(*BS){
							if((FOUT >=FOUT_MIN_HIG) && (FOUT<=FOUT_MAX_HIG)
									&& (FVCO >= FVCO_MIN_HIG) && (FVCO <= FVCO_MAX_HIG)){
								if ((abs(FOUT - RATE) < DIFF)){
									DIFF = abs(FOUT - RATE);
									*FOUT_NEAR = FOUT;
									bs = *BS;
									od = *OD;
									r  = *R;
									f  = *F;
								}
								if (FOUT == RATE){
									return 0;
								}
							}
						}
						else{
							if((FOUT >=FOUT_MIN_LOW) && (FOUT<=FOUT_MAX_LOW)
									&& (FVCO >= FVCO_MIN_LOW) && (FVCO <= FVCO_MAX_LOW)){
								if ((abs(FOUT - RATE) < DIFF)){
									DIFF = abs(FOUT - RATE);
									*FOUT_NEAR = FOUT;
									bs = *BS;
									od = *OD;
									r  = *R;
									f  = *F;
								}
								if (FOUT == RATE){
									return 0;
								}
							}
						}
					}
				}
			}
		}
	}
	*BS = bs;
	*OD = od;
	*R  = r;
	*F  = f;
	return 0;
}

static unsigned long clk_pll_recalc_rate(struct clk_hw *hw,
		unsigned long parent_rate)
{
	struct clk_pll *pll = to_clk_pll(hw);
	unsigned long long int rate;
	int clkod, clkn, clkm;
	u32 data;

	data  = clk_readl(pll->reg);
	clkm  = CLK_GET(data, CLK_MASK_M, CLK_M_SHIFT);
	clkn  = CLK_GET(data, CLK_MASK_N, CLK_N_SHIFT);
	clkod = CLK_GET(data, CLK_MASK_OD, CLK_OD_SHIFT);

	rate = parent_rate * 2 * (clkm + 1);
	do_div(rate, (clkn + 1) * (1 << clkod));

	return (unsigned long )rate;

}
static long clk_pll_round_rate(struct clk_hw *hw, unsigned long rate,
				unsigned long *prate)
{
	int BS, OD, R, F;
	unsigned long FOUT_NEAR = rate;
	pll_round_rate(&BS, &OD, &R, &F, *prate, &FOUT_NEAR);
	return FOUT_NEAR;

}
static int clk_pll_set_rate(struct clk_hw *hw, unsigned long user_rate,
				unsigned long parent_rate)
{
	struct clk_pll *pll = to_clk_pll(hw);
	int clkbp, clkod, clkn, clkm, j = 10000;
	unsigned long FOUT_NEAR = user_rate;
	u32 data;
	pll_round_rate(&clkbp, &clkod, &clkn, &clkm, parent_rate, &FOUT_NEAR);
	data = (1 << 15) | (1 << 14) | (clkod << 12) | (clkn << 7) | clkm;
	clk_writel(data, pll->reg);
	while(j--);
	j = 10000;
	data = (0 << 15) | (1 << 14) | (clkod << 12) | (clkn << 7) | clkm;
	clk_writel(data, pll->reg);
	while(j--);
	j = 10000;
	data = (0 << 15) | (0 << 14) | (clkod << 12) | (clkn << 7) | clkm;
	clk_writel(data, pll->reg);
	while(j--);
	return 0;
}

const struct clk_ops pll_ops = {
	.recalc_rate = clk_pll_recalc_rate,
	.round_rate = clk_pll_round_rate,
	.set_rate = clk_pll_set_rate,
};

static struct clk *register_pll(struct device *dev, const char *name,
		const char *parent_name, u8 parents_num, unsigned long flags,
		void __iomem *reg,
		u8 clk_pll_flags, spinlock_t *lock)
{
	struct clk_pll *pll;
	struct clk *clk;
	struct clk_init_data init;


	/* allocate the pll */
	pll = kzalloc(sizeof(*pll), GFP_KERNEL);
	if (!pll)
		return ERR_PTR(-ENOMEM);

	init.name = name;
	init.ops = &pll_ops;
	init.flags = flags | CLK_IS_BASIC;
	init.parent_names = (parent_name ? &parent_name: NULL);
	init.num_parents = (parent_name ? 1 : 0);

	/* struct clk_pll assignments */
	pll->reg = reg;
	pll->flags = clk_pll_flags;
	pll->lock = lock;
	pll->hw.init = &init;

	/* register the clock */
	clk = clk_register(dev, &pll->hw);

	if (IS_ERR(clk))
		kfree(pll);

	return clk;
}

static void __init nationalchip_leo_pll_setup(struct device_node *node)
{
	struct clk *clk;
	struct device_node *np = node;
	const char *clk_name = np->name;
	const char *parents;
	void __iomem *reg;
	u8 parent_num = 0;

	parents = of_clk_get_parent_name(node, 0);
	if (!parents){
		printk("%s don't get parents\n", node->name);
		return ;
	}

	reg = clk_iomap(node, 0, of_node_full_name(node));
	if (IS_ERR(reg))
		return;

	clk = register_pll(NULL, clk_name,
			parents, parent_num,
			0,
			reg,
			0, NULL);

	if (!IS_ERR(clk))
		of_clk_add_provider(node, of_clk_src_simple_get, clk);
	return ;
}

CLK_OF_DECLARE(nationalchip_leo_pll, "nationalchip,leo_pll",
	       nationalchip_leo_pll_setup);
