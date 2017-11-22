#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/init.h>
#include <linux/console.h>
#include <linux/sysrq.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/serial_reg.h>
#include <linux/serial_core.h>
#include <linux/serial.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#include <asm/io.h>
#include <asm/irq.h>
#ifndef CONFIG_CSKY
#include <asm/mach/map.h>
#endif

#define GXCHIP_ID_CONFIG_BASE	0x0030a000
#define GXCHIP_ID_CONFIG_OFFSET 0x184

#define GXCHIP_PROBE_CONFIG_BASE	0x0050a000
#define GXCHIP_PROBE_CONFIG_3200	0x184
#define GXCHIP_PROBE_CONFIG_3130	0x1c0
#define GXCHIP_PROBE_CONFIG_3110	0x180

#define GXOS_ID_GX3130			0x3130
#define GXOS_ID_GX3200			0x3200
#define GXOS_ID_GX3110			0x3110

#define HAL_READ_UINT32( _register_, _value_ ) \
	((_value_) = *((volatile unsigned int *)(_register_)))


static unsigned int gx_chipid = 0;
static unsigned char *mapped_addr = 0;

unsigned int gx3200_probe(void)
{
	unsigned int chip_probe;
	
	HAL_READ_UINT32((unsigned int *)(mapped_addr + GXCHIP_PROBE_CONFIG_3200), chip_probe);

	if((0xFFFF & chip_probe >> 14) == GXOS_ID_GX3200)
	{
		return GXOS_ID_GX3200;
	}

	if((0xFFFF & chip_probe >> 14) == 0x320d)
	{
		return GXOS_ID_GX3200;
	}

	return 0;
}

unsigned int gx3130_probe(void)
{
	unsigned int chip_probe;
	
	HAL_READ_UINT32((unsigned int *)(mapped_addr + GXCHIP_PROBE_CONFIG_3130), chip_probe);
	
	if((0xFFFF & chip_probe >> 14) == GXOS_ID_GX3130)
	{
		return GXOS_ID_GX3130;
	}
		
	return 0;
}

unsigned int gx3110_probe(void)
{
	return GXOS_ID_GX3110;
}

unsigned int gx3201_probe(void)
{
	unsigned int chip_id;
	HAL_READ_UINT32((unsigned int *)(mapped_addr + GXCHIP_ID_CONFIG_OFFSET), chip_id);
	return ((chip_id >> 14) & 0xFFFF);
}

unsigned int (*gxchip_probe[])(void) = {
	gx3201_probe,
	gx3200_probe,
	gx3130_probe,
	gx3110_probe,
	NULL,
};

void check_chipid(void)
{
	int i;

	for(i = 0; gxchip_probe[i] != NULL; i++) {
		gx_chipid = (unsigned int)(*gxchip_probe[i])();
		if(gx_chipid != 0) break;
	}
}

static int gxchipid_proc_add(char *name, struct file_operations *ec_ops, struct proc_dir_entry *dir)
{
#if 0
	struct proc_dir_entry *entry;

	entry = create_proc_entry(name, S_IRUGO, dir);
	if (!entry) {
		return -ENODEV;
	} else {
		entry->proc_fops = ec_ops;
		entry->data = NULL;
	}
#endif
	return 0;
}

#if 0
static int gx_chip_id_proc_read(struct seq_file *seq, void *offset)
{
	seq_printf(seq, "%x\n", gx_chipid);
	return 0;
}
#endif

static int gx_chip_id_proc_open(struct inode *inode, struct file *file)
{
	return 0;//single_open(file, gx_chip_id_proc_read, PDE(inode)->data);
}

static struct file_operations gx_chip_id_ops = {
	.open = gx_chip_id_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.owner = THIS_MODULE,
};

#define XTAL_CLOCK 27

static unsigned int __init _freq(unsigned int paddr)
{
	unsigned int CPU_REG;
	int NR,NF,NO;

	unsigned int *addr;

	addr = ioremap(paddr, 0x1);
	CPU_REG = *(volatile unsigned int *)addr;
	iounmap(addr);

	NR = ((CPU_REG>>12)&(0xf))+1;
	NF = ((CPU_REG)&(0x3f))+1;
	NO = ((CPU_REG>>8)&(0xf))+1;

	if (NR == 0 || NO == 0) return 0;

	return (XTAL_CLOCK*NF)/NR/NO;
}

static int __init cpu_dram_freq(void)
{
	printk("CPU DRAM: %dMhz,", _freq(0x0050a0e0));
	printk("SDC: %dMhz\n", _freq(0x0050a0c4));
	return 0;
}

static int gxchipid_init(void)
{

	if (gx_chipid != 0) goto exit;

	if (!request_mem_region(GXCHIP_ID_CONFIG_BASE, 0x1000, "gxchipid_init")) {
		printk(KERN_ERR "request_mem_region failed");
		return -EBUSY;
	}

	mapped_addr = ioremap(GXCHIP_ID_CONFIG_BASE, 0x1000);
	if (!mapped_addr) {
		printk(KERN_ERR "ioremap failed.\n");
		return -ENOMEM;
	}

	check_chipid();

	iounmap(mapped_addr);
	release_mem_region(GXCHIP_ID_CONFIG_BASE, 0x1000);

	printk("gxchip_init: %x.\n", gx_chipid);

	if (gx_chipid == 0x3200) {
		cpu_dram_freq();
	}

exit:
	gxchipid_proc_add("gx_chip_id", &gx_chip_id_ops, NULL);

	return 0;
}

static void gxchipid_exit(void)
{
	remove_proc_entry("gx_chip_id", NULL);
}

unsigned int gx_chip_id_probe(void)
{
	if(gx_chipid != 0) return gx_chipid;

	if (!request_mem_region(GXCHIP_ID_CONFIG_BASE, 0x1000, "gxchipid_init")) {
		printk(KERN_ERR "request_mem_region failed");
		return -EBUSY;
	}

	mapped_addr = ioremap(GXCHIP_ID_CONFIG_BASE, 0x1000);
	if (!mapped_addr) {
		printk(KERN_ERR "ioremap failed.\n");
		return -ENOMEM;
	}

	check_chipid();

	iounmap(mapped_addr);
	release_mem_region(GXCHIP_ID_CONFIG_BASE, 0x1000);

	printk("gxchip_init: %x.\n", gx_chipid);

	return gx_chipid;
}

EXPORT_SYMBOL(gx_chip_id_probe);

arch_initcall(gxchipid_init);
module_exit(gxchipid_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("gx3xxx chip id probe");
module_param(gx_chipid, int, S_IRUGO);
