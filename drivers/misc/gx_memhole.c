#include <linux/module.h>
#include <linux/slab.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/sysrq.h>
#include <linux/mm.h>
#include <linux/bootmem.h>

#ifndef CONFIG_CSKY
#define PHY_DRAM_ADDR 0xc0000000
#else
#define PHY_DRAM_ADDR 0x10000000
#endif

static unsigned int mem_size;

struct gx_membank {
	unsigned int start;
	unsigned int size;
	int node;
};

static struct gx_meminfo {
	int nr_banks;
	struct gx_membank bank[4];
} gx_meminfo_local = {
	.nr_banks = 0,
};

static struct gx_meminfo_fb {
	unsigned int start;
	unsigned int size;
	unsigned int surface_size;
} gx_meminfo_fb_local = {.start = 0, .size = 0, .surface_size = 0 };


static unsigned int fb_totle_size = 0;
struct gx_meminfo_fb *gx_fb_info_get(void)
{
#if 0
	gx_meminfo_fb_local.start = PHY_DRAM_ADDR + mem_size +
		gx_meminfo_local.bank[0].size;
#else
	gx_meminfo_fb_local.start = PHY_DRAM_ADDR + mem_size;
	if(fb_totle_size == 0)
		fb_totle_size = gx_meminfo_fb_local.size + gx_meminfo_local.bank[0].size;
	gx_meminfo_fb_local.size = fb_totle_size;
#endif
	printk("frambuffer size:%x, at:%x.\n", gx_meminfo_fb_local.size, gx_meminfo_fb_local.start);
	return &gx_meminfo_fb_local;
}

struct gx_meminfo *gx_hole_info_get(void)
{
	int i;
	for (i = 0; i < gx_meminfo_local.nr_banks && i < 3; i++){
		if (gx_meminfo_local.bank[i].start == 0)
			gx_meminfo_local.bank[i].start = PHY_DRAM_ADDR + mem_size;
		printk("video buffer size:%x, at:%x\n", gx_meminfo_local.bank[i].size, gx_meminfo_local.bank[i].start);
	}

	return &gx_meminfo_local;
}

static void add_sys_mem(unsigned int start, unsigned int size)
{
	int i;

	for (i = 0; i < gx_meminfo_local.nr_banks && i < 3; i++){
		// insert sort
		if (gx_meminfo_local.bank[i].start > start) {
			memmove(&gx_meminfo_local.bank[i+1], &gx_meminfo_local.bank[i],
					sizeof(struct gx_membank) * (gx_meminfo_local.nr_banks - i));
			break;
		}
	}

	if (i < 4) {
		gx_meminfo_local.bank[i].start = start;
		gx_meminfo_local.bank[i].size = size;
		gx_meminfo_local.nr_banks++;
	}

	for (i = 0; i < gx_meminfo_local.nr_banks; i++)
		gx_meminfo_local.bank[i].node = i;
}

static int __init surface_mem(char *p)
{
	char **pp;

	pp = &p;
	*pp = *pp - 1;

	gx_meminfo_fb_local.surface_size = memparse(*pp + 1, pp);
	return 0;
}
__setup("surface=", surface_mem);

static int __init video_framebuffer_mem(char *p)
{
	unsigned int start, size;
	char **pp;

	pp = &p;
	*pp = *pp - 1;

	do {
		size = memparse(*pp + 1, pp);
		if (*p == '@') {
			start = memparse((*pp) + 1, pp);
		} else {
			start = 0;
		}
		add_sys_mem(start, size);
	} while(**pp == ',');

	return 0;
}
__setup("videomem=", video_framebuffer_mem);

static unsigned int av_extend_array[256];

unsigned int *gx_av_extend(void)
{
	return &av_extend_array[0];
}

static int __init av_extend_parse(char *p)
{
	char **pp;
	int i = 1;

	av_extend_array[0] = 0;
	pp = &p;
	*pp = *pp - 1;

	do {
		av_extend_array[i] = memparse(*pp + 1, pp);
		if (*p == '@') {
			av_extend_array[i+1] = memparse((*pp) + 1, pp);
		} else {
			return -1;
		}
		i=i+2;
		if (i >= 256) return -1;
		av_extend_array[0] = av_extend_array[0] + 1;
	} while(**pp == ',');

	return 0;
}
__setup("av_extend=", av_extend_parse);

static int __init fb_mem(char *p)
{
	char **pp;

	pp = &p;
	*pp = *pp - 1;

	gx_meminfo_fb_local.size = memparse(*pp + 1, pp);

	return 0;
}
__setup("fbmem=", fb_mem);

static int __init sys_mem(char *p)
{
	char **pp;

	pp = &p;
	*pp = *pp - 1;

	mem_size = memparse(*pp + 1, pp);

	return 0;
}
__setup("mem=", sys_mem);


EXPORT_SYMBOL(gx_hole_info_get);
EXPORT_SYMBOL(gx_fb_info_get);
EXPORT_SYMBOL(gx_av_extend);

