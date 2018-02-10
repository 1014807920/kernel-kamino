#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/smp.h>
#include <linux/cpu.h>
#include <linux/cpu_pm.h>
#include <linux/irq.h>
#include <linux/clockchips.h>
#include <linux/clocksource.h>
#include <linux/interrupt.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/sched_clock.h>

#define NC_VA_COUNTER_1_STATUS		(void *)(timer_reg + 0x00)
#define NC_VA_COUNTER_1_VALUE		(void *)(timer_reg + 0x04)
#define NC_VA_COUNTER_1_CONTROL		(void *)(timer_reg + 0x10)
#define NC_VA_COUNTER_1_CONFIG		(void *)(timer_reg + 0x20)
#define NC_VA_COUNTER_1_PRE		(void *)(timer_reg + 0x24)
#define NC_VA_COUNTER_1_INI		(void *)(timer_reg + 0x28)
#define NC_VA_COUNTER_2_STATUS		(void *)(timer_reg + 0x40)
#define NC_VA_COUNTER_2_VALUE		(void *)(timer_reg + 0x44)
#define NC_VA_COUNTER_2_CONTROL		(void *)(timer_reg + 0x50)
#define NC_VA_COUNTER_2_CONFIG		(void *)(timer_reg + 0x60)
#define NC_VA_COUNTER_2_PRE		(void *)(timer_reg + 0x64)
#define NC_VA_COUNTER_2_INI		(void *)(timer_reg + 0x68)
#define NC_VA_COUNTER_3_STATUS		(void *)(timer_reg + 0x80)
#define NC_VA_COUNTER_3_VALUE		(void *)(timer_reg + 0x84)
#define NC_VA_COUNTER_3_CONTROL		(void *)(timer_reg + 0x90)
#define NC_VA_COUNTER_3_CONFIG		(void *)(timer_reg + 0xa0)
#define NC_VA_COUNTER_3_PRE		(void *)(timer_reg + 0xa4)
#define NC_VA_COUNTER_3_INI		(void *)(timer_reg + 0xa8)

#define COUNTER_CONTROL_RESET   (1 << 0)
#define COUNTER_CONTROL_START   (1 << 1)
#define COUNTER_CONFIG_EN       (1 << 0)
#define COUNTER_CONFIG_INT_EN   (1 << 1)

#define COUNTER_MODULE_ENABLE   (COUNTER_CONFIG_EN | COUNTER_CONFIG_INT_EN)
#define COUNTER_MODULE_DISABLE  ((~COUNTER_CONFIG_EN) & (~COUNTER_CONFIG_INT_EN))
#define COUNTER_MODULE_STOP     ((~COUNTER_CONTROL_RESET) & (~COUNTER_CONTROL_START))

static unsigned int timer_reg;

struct nationalchip_timer{
	struct clocksource *clksource;
	struct clock_event_device *clkevent;
	unsigned int timer_pre;
	u32 cycles_per_tick;
	void (*interrupt_handle)(void);
}nc_timer;

static void enable_timer(void)
{
	__raw_writel(1, NC_VA_COUNTER_1_STATUS);
}

static void disable_timer(void)
{
	__raw_writel(1, NC_VA_COUNTER_1_STATUS);
	__raw_writel(COUNTER_MODULE_DISABLE, NC_VA_COUNTER_1_CONFIG);
}

static cycle_t nc_read_counter2(struct clocksource *cs)
{
    /* Read the timer value */
	return (cycle_t)(readl_relaxed(NC_VA_COUNTER_2_VALUE));
}

static struct clocksource nc_clksource = {
	.name       = "nationalchip-clksource",
	.rating     = 300,
	.read       = nc_read_counter2,
	.mask       = CLOCKSOURCE_MASK(32),
	.flags      = CLOCK_SOURCE_VALID_FOR_HRES,
};

static inline void timer_reset(void)
{
	__raw_writel(COUNTER_CONTROL_RESET,  NC_VA_COUNTER_1_CONTROL);
	__raw_writel(COUNTER_MODULE_STOP, NC_VA_COUNTER_1_CONTROL);
	__raw_writel(COUNTER_MODULE_ENABLE, NC_VA_COUNTER_1_CONFIG);

	__raw_writel(nc_timer.timer_pre, NC_VA_COUNTER_1_PRE);
	__raw_writel(0xffffffff - nc_timer.cycles_per_tick, NC_VA_COUNTER_1_INI);
	__raw_writel(COUNTER_CONTROL_START, NC_VA_COUNTER_1_CONTROL);
}

static u64 notrace nc_sched_read(void)
{
	return (cycle_t)readl_relaxed(NC_VA_COUNTER_2_VALUE);
}


static irqreturn_t timer_interrupt(int irq, void *dev_id)
{
	struct clock_event_device *dev = (struct clock_event_device *) dev_id;

	nc_timer.interrupt_handle();

	dev->event_handler(dev);

	return IRQ_HANDLED;
}

static int nc_timer_set_periodic(struct clock_event_device *dev)
{
	nc_timer.interrupt_handle = enable_timer;
	timer_reset();
	return 0;
}

static int nc_timer_set_oneshot(struct clock_event_device *dev)
{
	// counter1 开启中断作clockevent, counter2关闭中断作clocksource
	nc_timer.interrupt_handle = disable_timer;
	__raw_writel(COUNTER_CONTROL_RESET,  NC_VA_COUNTER_2_CONTROL);
	__raw_writel(COUNTER_MODULE_STOP, NC_VA_COUNTER_2_CONTROL);
	__raw_writel(COUNTER_CONFIG_EN, NC_VA_COUNTER_2_CONFIG);

	__raw_writel(nc_timer.timer_pre, NC_VA_COUNTER_2_PRE);
	__raw_writel(0, NC_VA_COUNTER_2_INI);

	__raw_writel(COUNTER_CONTROL_START, NC_VA_COUNTER_2_CONTROL);
	__raw_writel(COUNTER_MODULE_DISABLE, NC_VA_COUNTER_1_CONFIG);
	return 0;
}

static int nc_timer_shutdown(struct clock_event_device *dev)
{
	timer_reset();
	return 0;
}


static int nc_set_next_event(unsigned long event,
    struct clock_event_device *evt_dev)
{
	__raw_writel(COUNTER_CONTROL_RESET,  NC_VA_COUNTER_1_CONTROL);
	__raw_writel(COUNTER_MODULE_STOP, NC_VA_COUNTER_1_CONTROL);
	__raw_writel(COUNTER_MODULE_ENABLE, NC_VA_COUNTER_1_CONFIG);

	__raw_writel(0xffffffff - event, NC_VA_COUNTER_1_INI);
	__raw_writel(COUNTER_CONTROL_START, NC_VA_COUNTER_1_CONTROL);
	return 0;
}

static struct clock_event_device nc_ced = {
	.name			= "nationalchip-clkevent",
	.features		= CLOCK_EVT_FEAT_PERIODIC | CLOCK_EVT_FEAT_ONESHOT,
	.rating			= 300,
	.set_state_shutdown	= nc_timer_shutdown,
	.set_state_periodic	= nc_timer_set_periodic,
	.set_state_oneshot  = nc_timer_set_oneshot,
	.set_next_event     = nc_set_next_event,
};

static void __init nc_timer_init(struct device_node *np)
{
	unsigned int irq;
	unsigned int freq, rate;

	/* parse from devicetree */
	timer_reg = (unsigned int) of_iomap(np, 0);
	if (!timer_reg)
		panic("%s, of_iomap err.\n", __func__);

	irq = irq_of_parse_and_map(np, 0);
	if (!irq)
		panic("%s, irq_parse err.\n", __func__);

	if (of_property_read_u32(np, "clock-frequency", &freq))
		panic("%s, clock-frequency error.\n", __func__);

	pr_info("Nationalchip Timer Init, reg: %x, irq: %d, freq: %d.\n",
			timer_reg, irq, freq);

	/* setup irq */
	if (request_irq(irq, timer_interrupt, IRQF_TIMER, np->name, &nc_ced))
		panic("%s timer_interrupt error.\n", __func__);

	nc_timer.clksource = &nc_clksource;
	nc_timer.clkevent  = &nc_ced;

	rate = freq;
	nc_timer.timer_pre = freq / rate - 1;
	nc_timer.cycles_per_tick = DIV_ROUND_UP(rate, HZ);

	sched_clock_register(nc_sched_read, 32, rate);

	clocksource_register_hz(nc_timer.clksource, rate);

	clockevents_config_and_register(nc_timer.clkevent, rate, 0xf, 0xffffffff);

}

CLOCKSOURCE_OF_DECLARE(nationalchip_nc_timer, "nationalchip,timer-v1", nc_timer_init);

