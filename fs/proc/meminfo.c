#include <linux/fs.h>
#include <linux/hugetlb.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/mmzone.h>
#include <linux/proc_fs.h>
#include <linux/quicklist.h>
#include <linux/seq_file.h>
#include <linux/swap.h>
#include <linux/vmstat.h>
#include <asm/atomic.h>
#include <asm/page.h>
#include <asm/pgtable.h>
#include "internal.h"

/* 20120710_ChanChung:query ddr manufacture */
#include <linux/io.h>
#include <linux/delay.h>

#define EBI1_CH0_BASE 0x00A00000
#define HWIO_EBI1_CH1_DDR_MR_CNTL_WDATA_ADDR    (EBI1_CH0_BASE + 0x00080014)
#define DDR_MR_RDATA_RANK0 (EBI1_CH0_BASE + 0x00080020)
#define DDR_MANUFACTURE_NUMBERS			19
#define RETRY_TIMES 5

typedef enum
{
	RESERVED_0,				/**< Reserved for future use. */
	SAMSUNG,				/**< Samsung. */
	QIMONDA,				/**< Qimonda. */
	ELPIDA,					/**< Elpida Memory, Inc. */
	ETRON,					/**< Etron Technology, Inc. */
	NANYA,					/**< Nanya Technology Corporation. */
	HYNIX,					/**< Hynix Semiconductor Inc. */
	MOSEL,					/**< Mosel Vitelic Corporation. */
	WINBOND,				/**< Winbond Electronics Corp. */
	ESMT,					/**< Elite Semiconductor Memory Technology Inc. */
	RESERVED_1,				/**< Reserved for future use. */
	SPANSION,				/**< Spansion Inc. */
	SST,					/**< Silicon Storage Technology, Inc. */
	ZMOS,					/**< ZMOS Technology, Inc. */
	INTEL,					/**< Intel Corporation. */
	NUMONYX = 254,				/**< Numonyx, acquired by Micron Technology, Inc. */
	MICRON = 255,				/**< Micron Technology, Inc. */
	ERR,					/** Error */
	DDR_MANUFACTURES_MAX = 0x7FFFFFFF	/**< Forces the enumerator to 32 bits. */
} ddr_manufactureID;

typedef struct
{
	ddr_manufactureID id;
	char	    	  *name;
} ddr_manufactures;

ddr_manufactures ddr_manufactures_list[] =
{
	{ .id = 0,		.name = "RESERVED_0" },
	{ .id = 1,		.name = "SAMSUNG" },
	{ .id = 2,		.name = "QIMONDA" },
	{ .id = 3,		.name = "ELPIDA" },
	{ .id = 4,		.name = "ETRON" },
	{ .id = 5,		.name = "NANYA" },
	{ .id = 6,		.name = "HYNIX" },
	{ .id = 7,		.name = "MOSEL" },
	{ .id = 8,		.name = "WINBOND" },
	{ .id = 9,		.name = "ESMT" },
	{ .id = 10,		.name = "RESERVED_1" },
	{ .id = 11, 		.name = "SPANSION" },
	{ .id = 12,		.name = "SST" },
	{ .id = 13,		.name = "ZMOS" },
	{ .id = 14,		.name = "INTEL" },
	{ .id = 254,		.name = "NUMONYX" },
	{ .id = 255,		.name = "MICRON" },
	{ .id = 256,            .name = "UNKNOWN"},
	{ .id = 0x7FFFFFFF, .name = "DDR_MANUFACTURES_MAX" }
};

static int mem_type_read_proc(char *page, char **start, off_t off,
					int count, int *eof, void *data)
{
	char *p;
	int len , i=0 ;
	uint32_t mem_value;
	void  *mr5_addr, *tags;
	ddr_manufactureID manufactureID = ELPIDA;

/* Configure DDR_MR_CNTL_WDATA */
	p = page;
	tags = ioremap_nocache(HWIO_EBI1_CH1_DDR_MR_CNTL_WDATA_ADDR, sizeof(uint32_t));
	mem_value = readl(tags);
	mem_value |= 0x05060000;		/* modify those bits
								     [bits 31:24]:MR_ADDR,05 mean read MR5
								     [bits 19:18]:MR_RANK_SEL, 01 mean rand 0 only
								*/
	mem_value &= 0x05F7FFFF;		/* Only bits 31:24 and 19:18 modify*/

	writel(mem_value, tags);		/* Write a command to request read MR5 */

	for ( i = 0 ; i < RETRY_TIMES ; i++ )
	{
		if ( ( readl( tags ) & 0x00020000 ) == 0x00020000 )		//MRR locate at bits 17 
		{
			break;
		}
		manufactureID = ERR;
		msleep(1);
	}
	//if MRR not respone, it will show 256
	if (! ( readl( tags ) & 0x00020000 ) == 0x00020000 )
	{
		p += sprintf( p, "%s\n", ddr_manufactures_list[manufactureID].name );
		return len;
	}

	iounmap(tags);

	mr5_addr = ioremap_nocache(DDR_MR_RDATA_RANK0 , sizeof(uint32_t));
	mem_value = readl(mr5_addr);
	iounmap(mr5_addr);
	manufactureID = (mem_value & 0x000000FF);	/* Manufacturer ID describe in 0:7 of MR5 */

/* Query manufacture name */
	for (i = 0 ; i < DDR_MANUFACTURE_NUMBERS ; i++) {
		if (ddr_manufactures_list[i].id == manufactureID) {
			p += sprintf(p, "DDR SDRAM manufacture name: %s\n", ddr_manufactures_list[i].name);
			break;
		}
	}
	len = (p - page) - off;

	return len;
}
void __attribute__((weak)) arch_report_meminfo(struct seq_file *m)
{
}

static int meminfo_proc_show(struct seq_file *m, void *v)
{
	struct sysinfo i;
	unsigned long committed;
	unsigned long allowed;
	struct vmalloc_info vmi;
	long cached;
	unsigned long pages[NR_LRU_LISTS];
	int lru;

/*
 * display in kilobytes.
 */
#define K(x) ((x) << (PAGE_SHIFT - 10))
	si_meminfo(&i);
	si_swapinfo(&i);
	committed = percpu_counter_read_positive(&vm_committed_as);
	allowed = ((totalram_pages - hugetlb_total_pages())
		* sysctl_overcommit_ratio / 100) + total_swap_pages;

	cached = global_page_state(NR_FILE_PAGES) -
			total_swapcache_pages - i.bufferram;
	if (cached < 0)
		cached = 0;

	get_vmalloc_info(&vmi);

	for (lru = LRU_BASE; lru < NR_LRU_LISTS; lru++)
		pages[lru] = global_page_state(NR_LRU_BASE + lru);

	/*
	 * Tagged format, for easy grepping and expansion.
	 */
	seq_printf(m,
		"MemTotal:       %8lu kB\n"
		"MemFree:        %8lu kB\n"
		"Buffers:        %8lu kB\n"
		"Cached:         %8lu kB\n"
		"SwapCached:     %8lu kB\n"
		"Active:         %8lu kB\n"
		"Inactive:       %8lu kB\n"
		"Active(anon):   %8lu kB\n"
		"Inactive(anon): %8lu kB\n"
		"Active(file):   %8lu kB\n"
		"Inactive(file): %8lu kB\n"
		"Unevictable:    %8lu kB\n"
		"Mlocked:        %8lu kB\n"
#ifdef CONFIG_HIGHMEM
		"HighTotal:      %8lu kB\n"
		"HighFree:       %8lu kB\n"
		"LowTotal:       %8lu kB\n"
		"LowFree:        %8lu kB\n"
#endif
#ifndef CONFIG_MMU
		"MmapCopy:       %8lu kB\n"
#endif
		"SwapTotal:      %8lu kB\n"
		"SwapFree:       %8lu kB\n"
		"Dirty:          %8lu kB\n"
		"Writeback:      %8lu kB\n"
		"AnonPages:      %8lu kB\n"
		"Mapped:         %8lu kB\n"
		"Shmem:          %8lu kB\n"
		"Slab:           %8lu kB\n"
		"SReclaimable:   %8lu kB\n"
		"SUnreclaim:     %8lu kB\n"
		"KernelStack:    %8lu kB\n"
		"PageTables:     %8lu kB\n"
#ifdef CONFIG_QUICKLIST
		"Quicklists:     %8lu kB\n"
#endif
		"NFS_Unstable:   %8lu kB\n"
		"Bounce:         %8lu kB\n"
		"WritebackTmp:   %8lu kB\n"
		"CommitLimit:    %8lu kB\n"
		"Committed_AS:   %8lu kB\n"
		"VmallocTotal:   %8lu kB\n"
		"VmallocUsed:    %8lu kB\n"
		"VmallocChunk:   %8lu kB\n"
#ifdef CONFIG_MEMORY_FAILURE
		"HardwareCorrupted: %5lu kB\n"
#endif
#ifdef CONFIG_TRANSPARENT_HUGEPAGE
		"AnonHugePages:  %8lu kB\n"
#endif
		,
		K(i.totalram),
		K(i.freeram),
		K(i.bufferram),
		K(cached),
		K(total_swapcache_pages),
		K(pages[LRU_ACTIVE_ANON]   + pages[LRU_ACTIVE_FILE]),
		K(pages[LRU_INACTIVE_ANON] + pages[LRU_INACTIVE_FILE]),
		K(pages[LRU_ACTIVE_ANON]),
		K(pages[LRU_INACTIVE_ANON]),
		K(pages[LRU_ACTIVE_FILE]),
		K(pages[LRU_INACTIVE_FILE]),
		K(pages[LRU_UNEVICTABLE]),
		K(global_page_state(NR_MLOCK)),
#ifdef CONFIG_HIGHMEM
		K(i.totalhigh),
		K(i.freehigh),
		K(i.totalram-i.totalhigh),
		K(i.freeram-i.freehigh),
#endif
#ifndef CONFIG_MMU
		K((unsigned long) atomic_long_read(&mmap_pages_allocated)),
#endif
		K(i.totalswap),
		K(i.freeswap),
		K(global_page_state(NR_FILE_DIRTY)),
		K(global_page_state(NR_WRITEBACK)),
		K(global_page_state(NR_ANON_PAGES)
#ifdef CONFIG_TRANSPARENT_HUGEPAGE
		  + global_page_state(NR_ANON_TRANSPARENT_HUGEPAGES) *
		  HPAGE_PMD_NR
#endif
		  ),
		K(global_page_state(NR_FILE_MAPPED)),
		K(global_page_state(NR_SHMEM)),
		K(global_page_state(NR_SLAB_RECLAIMABLE) +
				global_page_state(NR_SLAB_UNRECLAIMABLE)),
		K(global_page_state(NR_SLAB_RECLAIMABLE)),
		K(global_page_state(NR_SLAB_UNRECLAIMABLE)),
		global_page_state(NR_KERNEL_STACK) * THREAD_SIZE / 1024,
		K(global_page_state(NR_PAGETABLE)),
#ifdef CONFIG_QUICKLIST
		K(quicklist_total_size()),
#endif
		K(global_page_state(NR_UNSTABLE_NFS)),
		K(global_page_state(NR_BOUNCE)),
		K(global_page_state(NR_WRITEBACK_TEMP)),
		K(allowed),
		K(committed),
		(unsigned long)VMALLOC_TOTAL >> 10,
		vmi.used >> 10,
		vmi.largest_chunk >> 10
#ifdef CONFIG_MEMORY_FAILURE
		,atomic_long_read(&mce_bad_pages) << (PAGE_SHIFT - 10)
#endif
#ifdef CONFIG_TRANSPARENT_HUGEPAGE
		,K(global_page_state(NR_ANON_TRANSPARENT_HUGEPAGES) *
		   HPAGE_PMD_NR)
#endif
		);

	hugetlb_report_meminfo(m);

	arch_report_meminfo(m);

	return 0;
#undef K
}

static int meminfo_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, meminfo_proc_show, NULL);
}

static const struct file_operations meminfo_proc_fops = {
	.open		= meminfo_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __init proc_meminfo_init(void)
{
	proc_create("meminfo", 0, NULL, &meminfo_proc_fops);
	return 0;
}
static int __init proc_memtype_init(void)
{
	create_proc_read_entry("mem_type", 0, 0, mem_type_read_proc, NULL);
	return 0;
}
module_init(proc_meminfo_init);
module_init(proc_memtype_init);

