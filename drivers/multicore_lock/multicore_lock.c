#include <linux/module.h>
#include <linux/io.h>
#include <linux/mutex.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/multicore_lock.h>

struct mlock {
	struct device_node *np;
	void __iomem *mcu_lock;
	void __iomem *cpu_lock;
};

static DEFINE_MUTEX(multi_lock);

static struct mlock mlock;

static int __init multicore_lock_init(void)
{
	mlock.np = of_find_compatible_node(NULL, NULL, "NationalChip,multicore_lock");
	if (!mlock.np) {
		printk("fail to get multicore lock device node\n");
		return -ENODEV;
	}

	mlock.mcu_lock = of_iomap(mlock.np, 0);
	mlock.cpu_lock = of_iomap(mlock.np, 1);
	if (IS_ERR(mlock.mcu_lock) || IS_ERR(mlock.cpu_lock)) {
		printk("iomap error\n");
		return -EIO;
	}

	return 0;
}

int multicore_lock(MULTICORE_LOCK lock)
{
	unsigned int tmp = 0;

	mutex_lock(&multi_lock);
	tmp = readl(mlock.cpu_lock) | lock;
	writel(tmp, mlock.cpu_lock);

	if (readl(mlock.mcu_lock) & lock) {
		tmp = readl(mlock.cpu_lock) & ~lock;
		writel(tmp, mlock.cpu_lock);

		tmp = readl(mlock.cpu_lock) | lock;
		writel(tmp, mlock.cpu_lock);
	}
	mutex_unlock(&multi_lock);

	return 0;
}

int multicore_unlock(MULTICORE_LOCK lock)
{
	unsigned int tmp = 0;

	mutex_lock(&multi_lock);
	tmp = readl(mlock.cpu_lock) & ~lock;
	writel(tmp, mlock.cpu_lock);
	mutex_unlock(&multi_lock);

	return 0;
}

arch_initcall(multicore_lock_init);
