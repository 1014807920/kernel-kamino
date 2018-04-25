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

#define CHAN_OFFS       0x04
#define PWM_DUTY        (0x80 - 0x80)
#define PWM_CYCLE       (0xc0 - 0x80)
#define PWM_UPDATE      (0xe0 - 0x80)
#define PWM_EN          (0xe4 - 0x80)
#define PWM_DIV         (0xf0 - 0x80)

struct nationalchip_pwm {
	void __iomem *base;
	spinlock_t lock;
	struct clk *clk;
	struct pwm_chip chip;
	unsigned long rate;
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

	rate = p->rate;

	tmp = period_ns * rate;
	do_div(tmp, NSEC_PER_SEC);
	pc = tmp;

	tmp = duty_ns * rate;
	do_div(tmp, NSEC_PER_SEC);
	dc = tmp;

	if (pc > 0xffff || dc > 0xffff){
		printk("This cycle is too big!");
		return -1;
	}
	spin_lock(&p->lock);
	writel(pc - 1, p->base + PWM_CYCLE + pwm->hwpwm * CHAN_OFFS);
	writel(dc, p->base + PWM_DUTY + pwm->hwpwm *  CHAN_OFFS);
	spin_unlock(&p->lock);

	return 0;
}

static inline void nationalchip_pwm_enable_set(struct nationalchip_pwm *p,
					  unsigned int channel, bool enable)
{
	unsigned int offset = PWM_EN;
	u32 value;

	spin_lock(&p->lock);
	value = readl(p->base + offset);

	if (enable) {
		value |= (1 << channel);
	} else {
		value &= ~(1 << channel);
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
	unsigned int freq, div;
	int ret;

	p = devm_kzalloc(&pdev->dev, sizeof(*p), GFP_KERNEL);
	if (!p)
		return -ENOMEM;

	spin_lock_init(&p->lock);

	if (of_property_read_u32(pdev->dev.of_node, "clock-frequency", &freq))
		return -1;
	p->clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(p->clk)) {
		dev_err(&pdev->dev, "failed to obtain clock\n");
		return PTR_ERR(p->clk);
	}

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

	//分频
	div = clk_get_rate(p->clk) / freq - 1;
	if (div > 0xffff)
		div = 0xffff;
	p->rate = clk_get_rate(p->clk) / (div + 1);
	writel(div, p->base + PWM_DIV);
	//使能
	writel(0xff, p->base + PWM_EN);
	//更新占空比等寄存器
	writel(0x00, p->base + PWM_UPDATE);

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

static struct platform_driver nationalchip_pwm_driver = {
	.probe  = nationalchip_pwm_probe,
	.remove = nationalchip_pwm_remove,
	.driver = {
		.name           = "pwm-nationalchip",
		.of_match_table = nationalchip_pwm_of_match,
	},
};
module_platform_driver(nationalchip_pwm_driver);

MODULE_DESCRIPTION("Nationalchip PWM driver");
MODULE_LICENSE("GPL");
