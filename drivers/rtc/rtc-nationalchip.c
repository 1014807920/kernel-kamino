#include <linux/module.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/rtc.h>
#include <linux/bcd.h>
#include <linux/clk.h>
#include <linux/log2.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/uaccess.h>
#include <linux/io.h>

#include <asm/irq.h>

#define LEO_RTC_REG(x) (x)

#define LEO_RTC_CON         LEO_RTC_REG(0x00)
#define LEO_RTC_INTC        LEO_RTC_REG(0x04)
#define LEO_RTC_INTC_STA    LEO_RTC_REG(0x08)
#define LEO_RTC_DIV         LEO_RTC_REG(0x0c)

#define LEO_RTC_RTC_EN     (1 << 0)
#define LEO_RTC_TICK_EN    (3 << 1)
#define LEO_RTC_ALM_EN     (3 << 3)
#define LEO_RTC_TICK_INTC  (3 << 0)
#define LEO_RTC_ALM_INTC   (3 << 2)
#define LEO_RTC_SNAP_SEC   (1 << 2)
#define LEO_ALM_MASK_WEEK  (1 << 5)

#define LEO_RTC_SEC         LEO_RTC_REG(0x6c)
#define LEO_RTC_MIN         LEO_RTC_REG(0x70)
#define LEO_RTC_HOUR        LEO_RTC_REG(0x74)
#define LEO_RTC_DATE        LEO_RTC_REG(0x7c)
#define LEO_RTC_MON         LEO_RTC_REG(0x80)
#define LEO_RTC_YEAR        LEO_RTC_REG(0x84)
#define LEO_RTC_SNAP        LEO_RTC_REG(0x8c)

#define LEO_RTC_ALM_OFF     LEO_RTC_REG(0x00)

#define LEO_ALM_MASK        LEO_RTC_REG(0x14)
#define LEO_ALM_SEC         LEO_RTC_REG(0x20)
#define LEO_ALM_MIN         LEO_RTC_REG(0x24)
#define LEO_ALM_HOUR        LEO_RTC_REG(0x28)
#define LEO_ALM_DATE        LEO_RTC_REG(0x30)
#define LEO_ALM_MON         LEO_RTC_REG(0x34)
#define LEO_ALM_YEAR        LEO_RTC_REG(0x38)

struct leo_rtc {
	struct device *dev;
	struct rtc_device *rtc;

	void __iomem *base;
	struct clk *rtc_clk;
	struct clk *rtc_src_clk;
	bool clk_disabled;

	int irq_alarm;
	int irq;

	spinlock_t pie_lock;
	spinlock_t alarm_clk_lock;

	int ticnt_save, ticnt_en_save;
};

short bin4bcd(int val)
{
	/*return bin2bcd(val >> 8) * 100 + bin2bcd(val);*/
	int tmp[4];
	tmp[0] = (val / 1000 ) << 12;
	tmp[1] = (val % 1000 / 100) <<  8;
	tmp[2] = (val % 100 / 10) << 4;
	tmp[3] = val % 10;
	/*printf("val : %d\n", );*/
	return tmp[0] + tmp[1] + tmp[2] + tmp[3];
}

short bcd4bin(int val)
{
	/*return bcd2bin(val / 100) * 100 + bcd2bin(val & 0xff);*/
	u8 tmp[4];
	tmp[0] = (val & 0xf000) >> 12;
	tmp[1] = (val & 0x0f00) >> 8;
	tmp[2] = (val & 0x00f0) >> 4;
	tmp[3] = val & 0x000f;
	return tmp[0] * 1000 + tmp[1] * 100 + tmp[2] * 10 + tmp[3];
}

/* IRQ Handlers */
static irqreturn_t leo_rtc_irq(int irq, void *id)
{
	struct leo_rtc *info = (struct leo_rtc *)id;


	writel(0x00, info->base + LEO_RTC_INTC_STA);
	return IRQ_HANDLED;
}


/* Set RTC frequency */
static int leo_rtc_setfreq(struct leo_rtc *info, int freq)
{
	unsigned int counter_num;

	spin_lock_irq(&info->pie_lock);

	counter_num = clk_get_rate(info->rtc_clk) / freq;
	writel(counter_num, info->base + LEO_RTC_DIV);

	spin_unlock_irq(&info->pie_lock);

	return 0;
}

/* Time read/write */
static int leo_rtc_gettime(struct device *dev, struct rtc_time *rtc_tm)
{
	struct leo_rtc *info = dev_get_drvdata(dev);

	rtc_tm->tm_sec  = readl(info->base + LEO_RTC_SEC);
	rtc_tm->tm_min  = readl(info->base + LEO_RTC_MIN);
	rtc_tm->tm_hour = readl(info->base + LEO_RTC_HOUR);
	rtc_tm->tm_mday = readl(info->base + LEO_RTC_DATE);
	rtc_tm->tm_mon  = readl(info->base + LEO_RTC_MON);
	rtc_tm->tm_year = readl(info->base + LEO_RTC_YEAR);

	rtc_tm->tm_sec = bcd2bin(rtc_tm->tm_sec);
	rtc_tm->tm_min = bcd2bin(rtc_tm->tm_min);
	rtc_tm->tm_hour = bcd2bin(rtc_tm->tm_hour);
	rtc_tm->tm_mday = bcd2bin(rtc_tm->tm_mday);
	rtc_tm->tm_mon = bcd2bin(rtc_tm->tm_mon) - 1;
	rtc_tm->tm_year = bcd4bin(rtc_tm->tm_year) - 1900;

	dev_dbg(dev, "read time %04d.%02d.%02d %02d:%02d:%02d\n",
		 rtc_tm->tm_year, rtc_tm->tm_mon, rtc_tm->tm_mday,
		 rtc_tm->tm_hour, rtc_tm->tm_min, rtc_tm->tm_sec);


	return rtc_valid_tm(rtc_tm);
}

static int leo_rtc_settime(struct device *dev, struct rtc_time *tm)
{
	struct leo_rtc *info = dev_get_drvdata(dev);
	unsigned int rtc_con;
	unsigned int year = tm->tm_year;

	dev_dbg(dev, "set time %04d.%02d.%02d %02d:%02d:%02d\n",
		 tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
		 tm->tm_hour, tm->tm_min, tm->tm_sec);

	rtc_con = readl(info->base + LEO_RTC_CON);
	rtc_con &= ~(LEO_RTC_RTC_EN);
	writel(rtc_con, info->base + LEO_RTC_CON);

	writel(bin2bcd(tm->tm_sec),  info->base + LEO_RTC_SEC);
	writel(bin2bcd(tm->tm_min),  info->base + LEO_RTC_MIN);
	writel(bin2bcd(tm->tm_hour), info->base + LEO_RTC_HOUR);
	writel(bin2bcd(tm->tm_mday), info->base + LEO_RTC_DATE);
	writel(bin2bcd(tm->tm_mon + 1), info->base + LEO_RTC_MON);
	writel(bin4bcd(year + 1900), info->base + LEO_RTC_YEAR);

	rtc_con |= LEO_RTC_RTC_EN;
	writel(rtc_con, info->base + LEO_RTC_CON);

	return 0;
}

static int leo_rtc_getalarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	struct leo_rtc *info = dev_get_drvdata(dev);
	struct rtc_time *alm_tm = &alrm->time;
	unsigned int alm_en;

	alm_tm->tm_sec  = readl(info->base + LEO_ALM_SEC);
	alm_tm->tm_min  = readl(info->base + LEO_ALM_MIN);
	alm_tm->tm_hour = readl(info->base + LEO_ALM_HOUR);
	alm_tm->tm_mon  = readl(info->base + LEO_ALM_MON);
	alm_tm->tm_mday = readl(info->base + LEO_ALM_DATE);
	alm_tm->tm_year = readl(info->base + LEO_ALM_YEAR);

	alm_en = readl(info->base + LEO_RTC_CON);

	alrm->enabled = (alm_en & LEO_RTC_ALM_EN) ? 1 : 0;

	/* decode the alarm enable field */
	if (alrm->enabled){
		alm_tm->tm_sec  = bcd2bin(alm_tm->tm_sec);
		alm_tm->tm_min  = bcd2bin(alm_tm->tm_min);
		alm_tm->tm_hour = bcd2bin(alm_tm->tm_hour);
		alm_tm->tm_mday = bcd2bin(alm_tm->tm_mday);
		alm_tm->tm_mon  = bcd2bin(alm_tm->tm_mon) - 1;
		alm_tm->tm_year = bcd4bin(alm_tm->tm_year) - 1900;
	}
	else{
		alm_tm->tm_sec  = -1;
		alm_tm->tm_min  = -1;
		alm_tm->tm_hour = -1;
		alm_tm->tm_mday = -1;
		alm_tm->tm_mon  = -1;
		alm_tm->tm_mon  = -1;
		alm_tm->tm_year = -1;
	}

	dev_dbg(dev, "read alarm %d, %04d.%02d.%02d %02d:%02d:%02d\n",
		 alm_en,
		 alm_tm->tm_year, alm_tm->tm_mon, alm_tm->tm_mday,
		 alm_tm->tm_hour, alm_tm->tm_min, alm_tm->tm_sec);


	return rtc_valid_tm(alm_tm);
}


/* Update control registers */
static int leo_rtc_setaie(struct device *dev, unsigned int enabled)
{
	struct leo_rtc *info = dev_get_drvdata(dev);
	unsigned int rtc_con, rtc_intc;

	dev_dbg(info->dev, "%s: aie=%d\n", __func__, enabled);

	rtc_con   = readl(info->base + LEO_RTC_CON);
	rtc_intc  = readl(info->base + LEO_RTC_INTC);

	if (enabled){
		rtc_con  |= LEO_RTC_ALM_EN;
		rtc_intc |= LEO_RTC_ALM_INTC;
	}else{
		rtc_con  &= ~(LEO_RTC_ALM_EN);
		rtc_intc &= ~(LEO_RTC_ALM_INTC);
	}

	writel(rtc_con, info->base + LEO_RTC_CON);
	writel(rtc_intc, info->base + LEO_RTC_INTC);

	return 0;
}

static int leo_rtc_setalarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	struct leo_rtc *info = dev_get_drvdata(dev);
	struct rtc_time *tm = &alrm->time;
	unsigned int alrm_en, rtc_con;
	int year = tm->tm_year;

	dev_dbg(dev, "leo_rtc_setalarm: %d, %04d.%02d.%02d %02d:%02d:%02d\n",
		 alrm->enabled,
		 tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
		 tm->tm_hour, tm->tm_min, tm->tm_sec);

	rtc_con = readl(info->base + LEO_RTC_CON);
	alrm_en = rtc_con & LEO_RTC_ALM_EN;
	rtc_con |= LEO_RTC_ALM_EN;
	writel(rtc_con, info->base + LEO_RTC_CON);

	if (tm->tm_sec < 60 && tm->tm_sec >= 0) {
		writel(bin2bcd(tm->tm_sec), info->base + LEO_ALM_SEC);
	}

	if (tm->tm_min < 60 && tm->tm_min >= 0) {
		writel(bin2bcd(tm->tm_min), info->base + LEO_ALM_MIN);
	}

	if (tm->tm_hour < 24 && tm->tm_hour >= 0) {
		writel(bin2bcd(tm->tm_hour), info->base + LEO_ALM_HOUR);
	}

	if ((year + 1900) <= 9999 && year >= 0) {
		writel(bin4bcd(year + 1900), info->base + LEO_ALM_YEAR);
	}

	if (tm->tm_mon < 12 && tm->tm_mon >= 0) {
		writel(bin2bcd(tm->tm_mon + 1), info->base + LEO_ALM_MON);
	}

	if (tm->tm_mday <= 31 && tm->tm_mday >= 1) {
		writel(bin2bcd(tm->tm_mday), info->base + LEO_ALM_DATE);
	}

	dev_dbg(dev, "setting LEO_RTCALM to %08x\n", alrm_en);

	leo_rtc_setaie(dev, alrm->enabled);

	return 0;
}

static const struct rtc_class_ops leo_rtcops = {
	.read_time	= leo_rtc_gettime,
	.set_time	= leo_rtc_settime,
	.read_alarm	= leo_rtc_getalarm,
	.set_alarm	= leo_rtc_setalarm,
	.alarm_irq_enable = leo_rtc_setaie,
};


static int leo_rtc_remove(struct platform_device *pdev)
{
	struct leo_rtc *info = platform_get_drvdata(pdev);

	leo_rtc_setaie(info->dev, 0);

	clk_unprepare(info->rtc_clk);

	return 0;
}

static const struct of_device_id leo_rtc_dt_match[];

static int leo_rtc_probe(struct platform_device *pdev)
{
	struct leo_rtc *info = NULL;
	struct rtc_time rtc_tm;
	struct resource *res;
	int ret;

	info = devm_kzalloc(&pdev->dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	/* find the IRQs */
	info->irq = platform_get_irq(pdev, 0);
	if (info->irq < 0) {
		dev_err(&pdev->dev, "no irq for rtc\n");
		return info->irq;
	}

	info->dev = &pdev->dev;
	spin_lock_init(&info->pie_lock);
	spin_lock_init(&info->alarm_clk_lock);

	platform_set_drvdata(pdev, info);

	dev_dbg(&pdev->dev, "leo_rtc:irq %d\n",
		 info->irq);

	/* get the memory region */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	info->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(info->base))
		return PTR_ERR(info->base);

	info->rtc_clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(info->rtc_clk)) {
		dev_err(&pdev->dev, "failed to find rtc clock\n");
		return PTR_ERR(info->rtc_clk);
	}
	clk_prepare_enable(info->rtc_clk);


	dev_dbg(&pdev->dev, "leo_rtc: RTCCON=%02x\n",
		 readw(info->base + LEO_RTC_CON));

	/* Check RTC Time */
	if (leo_rtc_gettime(&pdev->dev, &rtc_tm)) {
		rtc_tm.tm_year	= 70;
		rtc_tm.tm_mon	= 0;
		rtc_tm.tm_mday	= 1;
		rtc_tm.tm_hour	= 0;
		rtc_tm.tm_min	= 0;
		rtc_tm.tm_sec	= 0;

		leo_rtc_settime(&pdev->dev, &rtc_tm);

		dev_warn(&pdev->dev, "warning: invalid RTC value so initializing it\n");
	}

	/* register RTC and exit */
	info->rtc = devm_rtc_device_register(&pdev->dev, "leo", &leo_rtcops,
				  THIS_MODULE);
	if (IS_ERR(info->rtc)) {
		dev_err(&pdev->dev, "cannot attach rtc\n");
		ret = PTR_ERR(info->rtc);
		goto err_nortc;
	}

	ret = devm_request_irq(&pdev->dev, info->irq, leo_rtc_irq,
			  0,  "leo-rtc", info);
	if (ret) {
		dev_err(&pdev->dev, "IRQ%d error %d\n", info->irq, ret);
		goto err_nortc;
	}

	writel(LEO_ALM_MASK_WEEK, info->base + LEO_ALM_MASK);
	writel(LEO_RTC_SNAP_SEC, info->base + LEO_RTC_SNAP);
	// 硬件设计是一次加10us
	leo_rtc_setfreq(info, 100000);

	return 0;

 err_nortc:

	clk_disable_unprepare(info->rtc_clk);

	return ret;
}

static const struct of_device_id leo_rtc_dt_match[] = {
	{
		.compatible = "nationalchip,leo-rtc",
	},
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, leo_rtc_dt_match);

static struct platform_driver leo_rtc_driver = {
	.probe		= leo_rtc_probe,
	.remove		= leo_rtc_remove,
	.driver		= {
		.name	= "leo-rtc",
		.of_match_table	= of_match_ptr(leo_rtc_dt_match),
	},
};
module_platform_driver(leo_rtc_driver);

MODULE_DESCRIPTION("NATIONALCHIP LEO RTC Driver");
MODULE_AUTHOR("liyj");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:leo-rtc");
