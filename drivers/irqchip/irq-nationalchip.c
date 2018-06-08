#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/module.h>
#include <linux/irqdomain.h>
#include <linux/irqchip.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <linux/syscore_ops.h>

#define NC_VA_INTC_NINT31_00		(unsigned int *)(intc_reg + 0x00)
#define NC_VA_INTC_NINT63_32		(unsigned int *)(intc_reg + 0x04)
#define NC_VA_INTC_NPEND31_00		(unsigned int *)(intc_reg + 0x10)
#define NC_VA_INTC_NPEND63_32		(unsigned int *)(intc_reg + 0x14)
#define NC_VA_INTC_NENSET31_00		(unsigned int *)(intc_reg + 0x20)
#define NC_VA_INTC_NENSET63_32		(unsigned int *)(intc_reg + 0x24)
#define NC_VA_INTC_NENCLR31_00		(unsigned int *)(intc_reg + 0x30)
#define NC_VA_INTC_NENCLR63_32		(unsigned int *)(intc_reg + 0x34)
#define NC_VA_INTC_NEN31_00		(unsigned int *)(intc_reg + 0x40)
#define NC_VA_INTC_NEN63_32		(unsigned int *)(intc_reg + 0x44)
#define NC_VA_INTC_NMASK31_00		(unsigned int *)(intc_reg + 0x50)
#define NC_VA_INTC_NMASK63_32		(unsigned int *)(intc_reg + 0x54)
#define NC_VA_INTC_SOURCE		(unsigned int *)(intc_reg + 0x60)

#define NC_NR_IRQS   64 /* allow some CPU external IRQ handling */
static unsigned int irq_num[NC_NR_IRQS];
static unsigned int intc_reg;

static struct irq_domain *root_domain = NULL;

static void nc_irq_mask(struct irq_data *d)
{
	unsigned int mask;
	unsigned int irq_chan;

	irq_chan = irq_num[d->hwirq];
	if (irq_chan == 0xff) return;

	if (irq_chan < 32) {
		mask = __raw_readl(NC_VA_INTC_NMASK31_00);
		mask |= 1 << irq_chan;
		__raw_writel(mask, NC_VA_INTC_NMASK31_00);
	} else {
		mask = __raw_readl(NC_VA_INTC_NMASK63_32);
		mask |= 1 << (irq_chan - 32);
		__raw_writel(mask, NC_VA_INTC_NMASK63_32);
	}
}

static void nc_irq_unmask(struct irq_data *d)
{
	unsigned int mask;
	unsigned int irq_chan;

	irq_chan = irq_num[d->hwirq];
	if (irq_chan == 0xff) return;

	if (irq_chan < 32) {
		mask = __raw_readl(NC_VA_INTC_NMASK31_00);
		mask &= ~( 1 << irq_chan);
		__raw_writel(mask, NC_VA_INTC_NMASK31_00);
	} else {
		mask = __raw_readl( NC_VA_INTC_NMASK63_32);
		mask &= ~(1 << (irq_chan - 32));
		__raw_writel(mask, NC_VA_INTC_NMASK63_32);
	}
}

static void nc_irq_en(unsigned int irq)
{
	unsigned int mask;
	unsigned int irq_chan;

	irq_chan = irq_num[irq];
	if (irq_chan == 0xff) return;

	if (irq_chan < 32) {
		mask = 1 << irq_chan;
		__raw_writel(mask, NC_VA_INTC_NENSET31_00);
	} else {
		mask = 1 << (irq_chan - 32);
		__raw_writel(mask, NC_VA_INTC_NENSET63_32);
	}
}

static void nc_irq_dis(unsigned int irq)
{
	unsigned int mask;
	unsigned int irq_chan;

	irq_chan = irq_num[irq];
	if (irq_chan == 0xff) return;

	if (irq_chan < 32) {
		mask = 1 << irq_chan;
		__raw_writel(mask, NC_VA_INTC_NENCLR31_00);
	} else {
		mask = 1 << (irq_chan - 32);
		__raw_writel(mask, NC_VA_INTC_NENCLR63_32);
	}
}

static inline void nc_irq_ack(struct irq_data *d) {}

unsigned int nc_irq_channel_set(struct irq_data *d)
{
	unsigned int status, i;

	i = d->hwirq;
	irq_num[i] = i;
	status = __raw_readl(NC_VA_INTC_SOURCE + i/4);
	status &= ~(0xff << ((i%4)*8));
	status |= (i << ((i%4)*8));
	__raw_writel(status, NC_VA_INTC_SOURCE + i/4);

	nc_irq_en(d->hwirq);
	nc_irq_unmask(d);
	return 0;
}

void nc_irq_channel_clear(struct irq_data *d)
{
	unsigned int i, status;

	i = irq_num[d->hwirq];
	irq_num[d->hwirq] = 0xff;

	nc_irq_mask(d);
	nc_irq_dis(d->hwirq);

	status = __raw_readl(NC_VA_INTC_SOURCE + i/4);
	status |= (0xff << ((i%4)*8));
	__raw_writel(status, NC_VA_INTC_SOURCE + i/4);
}

struct irq_chip nc_irq_chip = {
	.name =		"nationalchip_intc_v1",
	.irq_ack =	nc_irq_ack,
	.irq_mask =	nc_irq_mask,
	.irq_unmask =	nc_irq_unmask,
	.irq_startup =	nc_irq_channel_set,
	.irq_shutdown =	nc_irq_channel_clear,
};

#ifdef CONFIG_ARM
int ff1_64(unsigned int hi, unsigned int lo)
{
	int result;
	__asm__ __volatile__(
			"clz %0,%1"
			:"=r"(hi)
			:"r"(hi)
			:
			);

	__asm__ __volatile__(
			"clz %0,%1"
			:"=r"(lo)
			:"r"(lo)
			:
			);


	if( lo != 32 )
		result = 31-lo;
	else if( hi != 32 )
		result = 31-hi+32;
	else {
		printk("mach_get_auto_irqno error hi:%x, lo:%x.\n", hi, lo);
		result = NC_NR_IRQS;
	}

	return result;
}
#else
unsigned int find_ff1(unsigned int num)
{
	int i;
	for (i=0; i<32; i++)
		if (num&(1<<i))
			break;
	return i;
}

inline int ff1_64(unsigned int hi, unsigned int lo)
{
	int result;
	lo = find_ff1(lo);
	hi = find_ff1(hi);
	if( lo != 32 )
		result = lo;
	else if( hi != 32 )
		result = hi + 32;
	else {
		printk("mach_get_auto_irqno error hi:%x, lo:%x.\n", hi, lo);
		result = NC_NR_IRQS;
	}
	return result;
}
#endif

unsigned int nc_get_irqno(void)
{
	unsigned int nint64hi;
	unsigned int nint64lo;
	int irq_no;
	nint64lo = __raw_readl(NC_VA_INTC_NINT31_00);
	nint64hi = __raw_readl(NC_VA_INTC_NINT63_32);
	irq_no = ff1_64(nint64hi, nint64lo);

	return irq_no;
}

static int irq_map(struct irq_domain *h, unsigned int virq,
		irq_hw_number_t hw_irq_num)
{
	if (virq == 16 || virq == 17 || virq == 18 || virq == 19) { //arm generic timer interrupt
		irq_set_percpu_devid(virq);
		irq_set_chip_and_handler(virq, &nc_irq_chip, handle_percpu_devid_irq);
		irq_clear_status_flags(virq, IRQ_NOAUTOEN);
	}
	else
		irq_set_chip_and_handler(virq, &nc_irq_chip, handle_level_irq);

	irq_num[hw_irq_num] = 0xff;

	return 0;
}

static const struct irq_domain_ops nc_irq_ops = {
	.map	= irq_map,
	.xlate	= irq_domain_xlate_onecell,
};

#ifdef CONFIG_PM
struct nc_irqchip_data {
	unsigned int en_low;
	unsigned int en_hi;
	unsigned int mask_low;
	unsigned int mask_hi;
	unsigned int source[NC_NR_IRQS/4];
};

static volatile struct nc_irqchip_data irq_save;

static int nc_irq_suspend(void)
{
	unsigned int i;

	irq_save.en_low = __raw_readl(NC_VA_INTC_NEN31_00);
	irq_save.en_hi = __raw_readl(NC_VA_INTC_NEN63_32);

	irq_save.mask_low = __raw_readl(NC_VA_INTC_NMASK31_00);
	irq_save.mask_hi = __raw_readl(NC_VA_INTC_NMASK63_32);

	for (i = 0; i < (NC_NR_IRQS/4); i++) {
		irq_save.source[i] = __raw_readl(NC_VA_INTC_SOURCE + i);
	}

	/*
	* Disable all interrupts.
	*/
	for (i = 0; i < (NC_NR_IRQS/4); i++) {
		__raw_writel(0xffffffff, NC_VA_INTC_SOURCE + i);
	}

	__raw_writel(0xffffffff, NC_VA_INTC_NMASK31_00);
	__raw_writel(0xffffffff, NC_VA_INTC_NMASK63_32);

	__raw_writel(0xffffffff, NC_VA_INTC_NENCLR31_00);
	__raw_writel(0xffffffff, NC_VA_INTC_NENCLR63_32);

	return 0;
}

static void nc_irq_resume(void)
{
	unsigned int i;

	for (i = 0; i < (NC_NR_IRQS/4); i++) {
		__raw_writel(irq_save.source[i], NC_VA_INTC_SOURCE + i);
	}

	__raw_writel(irq_save.en_low, NC_VA_INTC_NENSET31_00);
	__raw_writel(irq_save.en_hi, NC_VA_INTC_NENSET63_32);

	__raw_writel(irq_save.mask_low, NC_VA_INTC_NMASK31_00);
	__raw_writel(irq_save.mask_hi, NC_VA_INTC_NMASK63_32);
}

static struct syscore_ops ncirq_syscore_ops = {
	.shutdown	= nc_irq_suspend,
	.suspend	= nc_irq_suspend,
	.resume		= nc_irq_resume,
};

#endif

static void nc_intc_irq_handle(struct pt_regs *regs)
{
	u32 irqnr;

	irqnr = nc_get_irqno();
	handle_domain_irq(root_domain, irqnr, regs);
}

static int __init intc_init(struct device_node *intc, struct device_node *parent)
{
	unsigned int i;
	u32 irq_base;

	if (parent)
		panic("DeviceTree incore intc not a root irq controller\n");

	intc_reg = (unsigned int) of_iomap(intc, 0);
	if (!intc_reg)
		panic("Nationalchip Intc Reg: %x.\n", intc_reg);

	__raw_writel(0xffffffff, NC_VA_INTC_NENCLR31_00);
	__raw_writel(0xffffffff, NC_VA_INTC_NENCLR63_32);
	__raw_writel(0xffffffff, NC_VA_INTC_NMASK31_00);
	__raw_writel(0xffffffff, NC_VA_INTC_NMASK63_32);

	for (i = 0; i < (NC_NR_IRQS/4); i++) {
		__raw_writel(0xffffffff, NC_VA_INTC_SOURCE + i);
	}

	irq_base = irq_alloc_descs(-1, 0, NC_NR_IRQS, 0);
	if (irq_base < 0) {
		pr_warn("Couldn't allocate IRQ numbers\n");
		irq_base = 0;
	}

	root_domain = irq_domain_add_legacy(intc, NC_NR_IRQS, 0, 0, &nc_irq_ops, NULL);
	if (!root_domain)
		panic("root irq domain not avail\n");

	irq_set_default_host(root_domain);
	set_handle_irq(nc_intc_irq_handle);

#ifdef CONFIG_PM
	register_syscore_ops(&ncirq_syscore_ops);
#endif

	return 0;
}

IRQCHIP_DECLARE(nationalchip_intc_v1, "nationalchip,intc-v1", intc_init);

