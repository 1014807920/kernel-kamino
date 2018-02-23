#include <linux/clk.h>
#include <linux/export.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pwm.h>
#include <linux/spinlock.h>
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/gpio/driver.h>
#include <linux/gpio/machine.h>
#include <config/gpiolib.h>
/*#include "../../gpio/gpiolib.h"*/

#define	CTRL_CHAN_OFFS	0x0c
#define PWM_CYCLE		0x04
#define PWM_DACIN		0x08

struct nationalchip_pwm {
	void __iomem *base;
	spinlock_t lock;
	struct clk *clk;
	struct pwm_chip chip;
};

static inline struct nationalchip_pwm *to_nationalchip_pwm(struct pwm_chip *chip)
{
	return container_of(chip, struct nationalchip_pwm, chip);
}

static int nationalchip_pwm_config(struct pwm_chip *chip, struct pwm_device *pwm,
			      int duty_ns, int period_ns)
{
	struct nationalchip_pwm *p = to_nationalchip_pwm(chip);
	unsigned long pc, dc;
	u64 rate, tmp;
	u32 value;

	rate = (u64)clk_get_rate(p->clk);
	printk("rate : %lld \n",rate);
	printk("hwpwm : %d \n",pwm->hwpwm);

	tmp = period_ns * rate;
	do_div(tmp, NSEC_PER_SEC);
	pc = tmp;

	tmp = duty_ns * rate;
	do_div(tmp, NSEC_PER_SEC);
	dc = tmp;

	spin_lock(&p->lock);
	writel(pc - 1, p->base + pwm->hwpwm * CTRL_CHAN_OFFS + PWM_CYCLE);
	writel(dc, p->base + pwm->hwpwm * CTRL_CHAN_OFFS + PWM_DACIN);
	printk("pc : %ld \n",pc);
	printk("dc : %ld \n",dc);
	spin_unlock(&p->lock);

	printk("pwn config ok\n");


	value = readl(p->base);
	printk("enble : %#x \n",value);

	return 0;
}

static int nationalchip_pwm_set_polarity(struct pwm_chip *chip,
                    struct pwm_device *pwm,
                    enum pwm_polarity polarity)
{
	struct nationalchip_pwm *p = to_nationalchip_pwm(chip);
	unsigned int offset        = pwm->hwpwm * CTRL_CHAN_OFFS;
	u32 value;

	printk("pwn set polarity start\n");
	spin_lock(&p->lock);
	value = readl(p->base + offset);

	if (PWM_POLARITY_NORMAL == polarity) {
		value &= ~(1 << 1);
	} else if (PWM_POLARITY_INVERSED == polarity){
		value |= (1 << 1);
	}

	writel(value, p->base + offset);
	spin_unlock(&p->lock);

	printk("pwn set polarity ok\n");
    return 0;
}

static inline void nationalchip_pwm_enable_set(struct nationalchip_pwm *p,
					  unsigned int channel, bool enable)
{
	unsigned int offset = channel * CTRL_CHAN_OFFS;
	u32 value;

	spin_lock(&p->lock);
	value = readl(p->base + offset);

	printk("enble : %#x \n",value);

	if (enable) {
		value |= 1;
		printk("pwm enable ok\n");
	} else {
		value &= ~1;
		printk("pwm disenable ok\n");
	}

	writel(value, p->base + offset);
	spin_unlock(&p->lock);
}

static int nationalchip_pwm_enable(struct pwm_chip *chip, struct pwm_device *pwm)
{
	struct nationalchip_pwm *p = to_nationalchip_pwm(chip);

	nationalchip_pwm_enable_set(p, pwm->hwpwm, true);

	return 0;
}

static void nationalchip_pwm_disable(struct pwm_chip *chip, struct pwm_device *pwm)
{
	struct nationalchip_pwm *p = to_nationalchip_pwm(chip);

	nationalchip_pwm_enable_set(p, pwm->hwpwm, false);
}

static const struct pwm_ops nationalchip_pwm_ops = {
	.config       = nationalchip_pwm_config,
	.set_polarity = nationalchip_pwm_set_polarity,
	.enable       = nationalchip_pwm_enable,
	.disable      = nationalchip_pwm_disable,
	.owner        = THIS_MODULE,
};

static const struct of_device_id nationalchip_pwm_of_match[] = {
	{ .compatible = "nationalchip,LEO_A7-pwm",},
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, nationalchip_pwm_of_match);

static int nationalchip_pwm_probe(struct platform_device *pdev)
{
	struct nationalchip_pwm *p;
	struct resource *res;
	int ret;
	unsigned long rate;
	struct clk *pclk;

	p = devm_kzalloc(&pdev->dev, sizeof(*p), GFP_KERNEL);
	if (!p)
		return -ENOMEM;

	spin_lock_init(&p->lock);
#if 1
	p->clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(p->clk)) {
		dev_err(&pdev->dev, "failed to obtain clock\n");
		return PTR_ERR(p->clk);
	}else
		printk("clk addr :%p\n", p->clk);
	/*clk_enable(p->clk);*/
	ret = clk_prepare_enable(p->clk);
	printk("clk_enable ok\n");
	rate = clk_get_rate(p->clk);
	printk("get rate : %ld\n", rate);
	/*rate = clk_round_rate(p->clk,54000000);*/
	rate = clk_round_rate(p->clk, 600000000);
	printk("round rate : %ld\n", rate);
	clk_set_rate(p->clk,rate);
	printk("get rate ok\n");

	printk("change parent osc-------------------\n");
	pclk = clk_get(&pdev->dev, "osc");
	if (IS_ERR(pclk)) {
		dev_err(&pdev->dev, "failed to obtain clock\n");
		return PTR_ERR(p->clk);
	}else
		printk("clk addr :%p\n", pclk);
	ret = clk_set_parent(p->clk, pclk);
	printk("set parent ok %d\n",ret);
	clk_get_parent(p->clk);
	printk("get parent ok\n");
	rate = clk_get_rate(p->clk);
	printk("get rate : %ld\n", rate);
/*#else*/

	printk("set pll_arm-------------------\n");
	pclk = clk_get(&pdev->dev, "pll_arm");
	if (IS_ERR(pclk)) {
		dev_err(&pdev->dev, "failed to obtain clock\n");
		return PTR_ERR(p->clk);
	}else
		printk("clk addr :%p\n", pclk);
	rate = clk_get_rate(pclk);
	printk("get rate : %ld\n", rate);
	/*rate = clk_round_rate(p->clk,54000000);*/
	rate = clk_round_rate(pclk, 216000000);
	printk("round rate : %ld\n", rate);
	clk_set_rate(pclk,rate);
	printk("set rate ok\n");
	printk("change parent pll----------------------------\n");
	ret = clk_set_parent(p->clk, pclk);
	printk("set parent ok %d\n",ret);
	rate = clk_get_rate(p->clk);
	printk("get rate : %ld\n", rate);
#endif

	clk_disable_unprepare(p->clk);
	/*clk_disable(p->clk);*/
	printk("clk_disenable ok\n");

	ret = clk_prepare_enable(p->clk);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to enable clock: %d\n", ret);
		return ret;
	}

	platform_set_drvdata(pdev, p);

	p->chip.dev       = &pdev->dev;
	p->chip.ops       = &nationalchip_pwm_ops;
	p->chip.base      = -1;
	p->chip.npwm      = 8;
	p->chip.can_sleep = true;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	p->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(p->base)) {
		ret = PTR_ERR(p->base);
		goto out_clk;
	}

	ret = pwmchip_add(&p->chip);
	if (ret) {
		dev_err(&pdev->dev, "failed to add PWM chip: %d\n", ret);
		goto out_clk;
	}
	printk("gx pwm chip add ok\n");

	return 0;

out_clk:
	clk_disable_unprepare(p->clk);
	return ret;
}

static int nationalchip_pwm_remove(struct platform_device *pdev)
{
	struct nationalchip_pwm *p = platform_get_drvdata(pdev);
	int ret;

	ret = pwmchip_remove(&p->chip);
	clk_disable_unprepare(p->clk);

	return ret;
}

#ifdef CONFIG_PM_SLEEP
static int nationalchip_pwm_suspend(struct device *dev)
{
	struct nationalchip_pwm *p = dev_get_drvdata(dev);

	clk_disable(p->clk);

	return 0;
}

static int nationalchip_pwm_resume(struct device *dev)
{
	struct nationalchip_pwm *p = dev_get_drvdata(dev);

	clk_enable(p->clk);

	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(nationalchip_pwm_pm_ops, nationalchip_pwm_suspend,
			 nationalchip_pwm_resume);

static struct platform_driver nationalchip_pwm_driver = {
	.probe  = nationalchip_pwm_probe,
	.remove = nationalchip_pwm_remove,
	.driver = {
		.name           = "pwm-nationalchip",
		.of_match_table = nationalchip_pwm_of_match,
		.pm             = &nationalchip_pwm_pm_ops,
	},
};
module_platform_driver(nationalchip_pwm_driver);

MODULE_DESCRIPTION("Nationalchip PWM driver");
MODULE_AUTHOR("Liyj");
MODULE_LICENSE("GPL");
