#include <linux/io.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/mutex.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/pwm.h>
#include <linux/gpio/consumer.h>
#include <linux/gpio/driver.h>
#include <linux/gpio/machine.h>

/*#define DEBUG 0*/
#ifdef DEBUG
#define GPIO_PRINTF			printk
#else
#define GPIO_PRINTF(...)		do{}while(0);
#endif

struct uart_control{
	void __iomem *addr;
	u32 val;
	struct gpio_desc *cts_desc;
	struct gpio_desc *rts_desc;
};

static irqreturn_t uart_con_interrupt(int irq, void *dev_id)
{
	int level;
	struct platform_device *pdev= (struct platform_device *)dev_id;
	struct uart_control *uart_con = (struct uart_control *)platform_get_drvdata(pdev);
	level = gpiod_get_value(uart_con->cts_desc);
	if (level == 1){
		writel(0x01, uart_con->addr + 0xa4);
	}else{
		writel(0x00, uart_con->addr + 0xa4);
	}
	GPIO_PRINTF("test get level %d---\n", level);
	return IRQ_HANDLED;
}

/* copy and construct gpio map from SRAM into SDRAM */
static int uart_control_probe(struct platform_device *pdev)
{
	int cts_irq, ret;
	struct uart_control *uart_con;
	struct resource *regs = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	GPIO_PRINTF("\n ------------insmode gpio module---------- \n");

	uart_con = devm_kzalloc(&pdev->dev, sizeof(*uart_con), GFP_KERNEL);
    if (!uart_con) {
        dev_err(&pdev->dev, "failed to allocate memory for driver's "
                "private data\n");
        return -ENOMEM;
    }
	uart_con->addr = ioremap(regs->start, resource_size(regs));
	if (of_property_read_u32(pdev->dev.of_node, "val", &uart_con->val)){
        dev_err(&pdev->dev, "failed to get val\n");
        return -1;
	}

	uart_con->cts_desc = devm_gpiod_get(&pdev->dev, "cts", GPIOD_IN);
	if (IS_ERR(uart_con->cts_desc)){
		printk("fail to gpiod get\n");
		return -1;
	}
	uart_con->rts_desc = devm_gpiod_get(&pdev->dev, "rts", GPIOD_OUT_LOW);
	if (IS_ERR(uart_con->rts_desc)){
		printk("fail to gpiod get\n");
		return -1;
	}
	cts_irq = gpiod_to_irq(uart_con->cts_desc);
	if (cts_irq < 0){
		printk("%s get cts_irq error\n", pdev->name);
	}

	ret = devm_request_irq(&pdev->dev, cts_irq, uart_con_interrupt, IRQF_TRIGGER_FALLING, NULL, pdev);
	if(ret){
		printk("fail to request_irq err:%d\n", ret);
	}
	platform_set_drvdata(pdev, uart_con);

	GPIO_PRINTF("------------------end-----------------------\n");



	return 0;
}

static int uart_control_remove(struct platform_device *pdev)
{
	struct uart_control *uart_con = (struct uart_control *)platform_get_drvdata(pdev);
	devm_gpiod_put(&pdev->dev, uart_con->cts_desc);
	devm_gpiod_put(&pdev->dev, uart_con->rts_desc);
	iounmap(uart_con->addr);
	return 0;
}


static const struct of_device_id nationalchip_uart_control_of_match[] = {
	{ .compatible = "nationalchip,LEO_uart_control",},
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, nationalchip_uart_control_of_match);

static struct platform_driver uart_control_driver = {
	.probe = uart_control_probe,
	.remove = uart_control_remove,
	.driver = {
		.name = "leo_uart_control",
		.of_match_table = nationalchip_uart_control_of_match,
	},
};

module_platform_driver(uart_control_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("uart gpio control driver for leo");
