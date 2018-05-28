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
#define PWM_DUTY        (0x80)
#define PWM_CYCLE       (0xc0)
#define PWM_CHANNEL_SEL (0x20)
#define PWM_UPDATE      (0xe0)
#define PWM_PORT_EN     (0x30)
#define PWM_CHANNEL_EN  (0xe4)
#define PWM_DIV         (0xf0)
#define GROUP_GPIO_NUM 32
#define PWM_CHANNEL_NUM                 0x8
#define PWM_CHANNEL_SHIFT               0x8
#define PWM_CHANNEL_MASK                0xf
#define PWM_CHANNEL_WIDTH               0x4

#define PWM_SEL_CHANGE(data, shift) (((data >> 3 * shift) & 0x7) << (shift * 4))
#define PWM_SEL_READ(data) (PWM_SEL_CHANGE(data,0) | PWM_SEL_CHANGE(data,1) | PWM_SEL_CHANGE(data,2) | PWM_SEL_CHANGE(data,3) | PWM_SEL_CHANGE(data,4) | PWM_SEL_CHANGE(data,5) | PWM_SEL_CHANGE(data,6) | PWM_SEL_CHANGE(data,7))

typedef struct {
    int cycle;
    int duty;
    int counter;
} PWM_CHANNEL_INFO;

struct gpio_pwm_info{
    int port_pwm_flag;
    int s_gpio_to_channel[GROUP_GPIO_NUM];
    PWM_CHANNEL_INFO s_pwm_channel_info[PWM_CHANNEL_NUM];
};

struct nationalchip_pwm {
	void __iomem *base;
	spinlock_t lock;
	struct clk *clk;
	struct pwm_chip chip;
	unsigned long rate;
	struct gpio_pwm_info gpio_pwm_info;
};

static inline struct nationalchip_pwm *to_nationalchip_pwm(struct pwm_chip *chip)
{
	return container_of(chip, struct nationalchip_pwm, chip);
}

static int nationalchip_pwm_config(struct pwm_chip *chip, struct pwm_device *pwm,
			      int duty_ns, int period_ns)
{
	struct nationalchip_pwm *p = to_nationalchip_pwm(chip);
	unsigned long pc, dc, data;
	u64 rate, tmp;
	int port = pwm->hwpwm;
	struct gpio_pwm_info *gpio_pwm_info = &p->gpio_pwm_info;
	int port_channel = 0;
	int i, ret = -1;

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

	// 寻找有一样频率和占空比的通道
	for (i = 0; i < PWM_CHANNEL_NUM; i++) {
		if (gpio_pwm_info->s_pwm_channel_info[i].cycle == period_ns
				&& gpio_pwm_info->s_pwm_channel_info[i].duty == duty_ns
				&& gpio_pwm_info->s_pwm_channel_info[i].counter != 0) {
			ret = 0;
			break;
		}
	}
	//寻找空的通道
	if (ret != 0){
		for (i = 0; i < PWM_CHANNEL_NUM; i++) {
			if (gpio_pwm_info->s_pwm_channel_info[i].counter == 0){
				ret = 0;
				break;
			}
		}
	}
	// 找到可通道
	if (ret == 0){
		if (gpio_pwm_info->s_gpio_to_channel[port] > 0){
			port_channel = gpio_pwm_info->s_gpio_to_channel[port] -1;
			gpio_pwm_info->s_pwm_channel_info[port_channel].counter -= 1;
		}
		gpio_pwm_info->s_pwm_channel_info[i].counter += 1;
		gpio_pwm_info->s_gpio_to_channel[port] = i + 1;
		gpio_pwm_info->s_pwm_channel_info[i].cycle = period_ns;
		gpio_pwm_info->s_pwm_channel_info[i].duty = duty_ns;

		spin_lock(&p->lock);
		writel(pc - 1, p->base + PWM_CYCLE + i * CHAN_OFFS);
		writel(dc, p->base + PWM_DUTY + i * CHAN_OFFS);
		data = readl(p->base + PWM_CHANNEL_SEL + (port / PWM_CHANNEL_SHIFT) * PWM_CHANNEL_WIDTH);
		data = PWM_SEL_READ(data);
		data &= ~(PWM_CHANNEL_MASK << ((port % PWM_CHANNEL_SHIFT) * PWM_CHANNEL_WIDTH));
		data |= (i << ((port % PWM_CHANNEL_SHIFT) * PWM_CHANNEL_WIDTH));
		writel(data, p->base + PWM_CHANNEL_SEL +  (port / PWM_CHANNEL_SHIFT) * PWM_CHANNEL_WIDTH);
		spin_unlock(&p->lock);
	}

	if (ret != 0){
		gpio_pwm_info->s_pwm_channel_info[port_channel].counter += 1;
		printk("No enough channel!\n");
	}

	return ret;
}

static inline void nationalchip_pwm_enable_set(struct nationalchip_pwm *p,
					  unsigned int channel, bool enable)
{
	unsigned int offset = PWM_PORT_EN;
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
	p->chip.npwm      = 32;
	p->chip.can_sleep = true;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	p->base = ioremap(res->start, resource_size(res));
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
	writel(0xff, p->base + PWM_CHANNEL_EN);
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
