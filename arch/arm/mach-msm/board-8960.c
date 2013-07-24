/* Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/i2c.h>
#include <linux/i2c/sx150x.h>
#include <linux/i2c/isl9519.h>
#include <linux/gpio.h>
/*Evan: SAR Porting start*/
#include <linux/platform_device.h>
#include <linux/switch.h>
/*Evan: SAR Porting end*/
#include <linux/msm_ssbi.h>
#include <linux/regulator/gpio-regulator.h>
#include <linux/mfd/pm8xxx/pm8921.h>
#include <linux/mfd/pm8xxx/pm8xxx-adc.h>
#include <linux/regulator/consumer.h>
#include <linux/spi/spi.h>
#include <linux/slimbus/slimbus.h>
#include <linux/bootmem.h>
#include <linux/msm_kgsl.h>
#ifdef CONFIG_ANDROID_PMEM
#include <linux/android_pmem.h>
#endif
#include <linux/dma-mapping.h>
#include <linux/platform_data/qcom_crypto_device.h>
#include <linux/platform_data/qcom_wcnss_device.h>
#include <linux/leds.h>
#include <linux/leds-pm8xxx.h>
/* BEGIN Dom_Lin@pegatron [2011/02/02] [Init camera LED drivers in 1024 codebase] */
#if defined(CONFIG_LEDS_LM3559)
#include <linux/leds-lm3559.h>
#endif
/* END Dom_Lin@pegatron [2011/02/02] [Init camera LED drivers in 1024 codebase] */
#include <linux/i2c/atmel_mxt_ts.h>
#include <linux/msm_tsens.h>
#include <linux/ks8851.h>
#include <linux/i2c/isa1200.h>
#include <linux/memory.h>
#include <linux/memblock.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/setup.h>
#include <asm/hardware/gic.h>
#include <asm/mach/mmc.h>

#include <mach/board.h>
#include <mach/msm_iomap.h>
#include <mach/msm_spi.h>
#ifdef CONFIG_USB_MSM_OTG_72K
#include <mach/msm_hsusb.h>
#else
#include <linux/usb/msm_hsusb.h>
#endif
#include <linux/usb/android.h>
#include <mach/usbdiag.h>
#include <mach/socinfo.h>
#include <mach/rpm.h>
#include <mach/gpio.h>
#include <mach/gpiomux.h>
// [2011/12/22] Jackal - porting westlake
#ifdef CONFIG_MPU_SENSORS_MPU3050
#include <linux/mpu.h>
#define PEGA_MSMPIO_GYRO_INT 69
#define PEGA_MSM_GPIO_ACCEL_INT 10
#endif
// [2011/12/22] Jackal - porting westlake End
#ifdef CONFIG_TOUCHSCREEN_LGD_I2C	/*[BEGIN] CONFIG_TOUCHSCREEN_LGD_I2C */
#include <linux/input/touch_pdata.h>  // mhshin_20120206
#endif	/*[END] CONFIG_TOUCHSCREEN_LGD_I2C */


#include <mach/msm_bus_board.h>
#include <mach/msm_memtypes.h>
#include <mach/dma.h>
#include <mach/msm_dsps.h>
#include <mach/msm_xo.h>
#include <mach/restart.h>

#ifdef CONFIG_WCD9310_CODEC
#include <linux/slimbus/slimbus.h>
#include <linux/mfd/wcd9310/core.h>
#include <linux/mfd/wcd9310/pdata.h>
#endif

#include <linux/ion.h>
#include <mach/ion.h>
#include <mach/mdm2.h>
#include <mach/mdm-peripheral.h>
#include <mach/msm_rtb.h>
#include <mach/msm_cache_dump.h>
#include <mach/scm.h>

#include <linux/fmem.h>

#include "timer.h"
#include "devices.h"
#include "devices-msm8x60.h"
#include "spm.h"
#include "board-8960.h"
#include "pm.h"
#include <mach/cpuidle.h>
#include "rpm_resources.h"
#include "mpm.h"
#include "acpuclock.h"
#include "rpm_log.h"
#include "smd_private.h"
#include "pm-boot.h"
#include "msm_watchdog.h"
/* BEGIN-20111221-paul add h2w for ATS test. */
#if defined (CONFIG_WESTLAKE) && defined (CONFIG_SWITCH_GPIO)
#include "westlake_h2w.h"
#endif
/* END-20111221-paul add for ATS test. */

/* BEGIN-20111214-KenLee-add for westlake charging. */
#ifdef CONFIG_PEGA_ISL_CHG
#include <linux/msm-charger.h>
#endif
/* END-20111214-KenLee-add for westlake charging. */
/*Begin-For EC driver*/
#ifdef CONFIG_EC
#include "westlake_dock.h"
#include <linux/nuvec.h>
#include <linux/nuvec_api.h>
#endif
/*END-For EC driver*/
/*Evan: SAR Porting start*/
#include "westlake_iqs128.h"
/*Evan: SAR Porting end*/
// BEGIN Tim PH, 20111214, add gauge BQ27541 driver.
#include <linux/i2c/bq27520.h>
// END Tim PH, 20111214, add gauge BQ27541 driver.

/* BEGIN Shengjie_Yu, 20111220, Define gpio_keys buttons resource */
#ifdef CONFIG_KEYBOARD_GPIO
#include <linux/gpio_keys.h>
#endif
/* END Shengjie_Yu, 20111220, Define gpio_keys buttons resource  */

/* BEGIN Acme_Wen@pegatroncorp.com [2011/12/21] [for hall sensor] */
#ifdef CONFIG_SENSORS_APX9132
#include <linux/apx9132.h>
#endif 
/* END Acme_Wen@pegatroncorp.com [2011/12/21] [for hall sensor] */
#ifdef CONFIG_TOUCHSCREEN_LGD_I2C
#include <linux/delay.h>  // mhshin_20120221
#endif
static struct platform_device msm_fm_platform_init = {
	.name = "iris_fm",
	.id   = -1,
};

#define KS8851_RST_GPIO		89
#define KS8851_IRQ_GPIO		90
#define HAP_SHIFT_LVL_OE_GPIO	47

#if defined(CONFIG_GPIO_SX150X) || defined(CONFIG_GPIO_SX150X_MODULE)

struct sx150x_platform_data msm8960_sx150x_data[] = {
	[SX150X_CAM] = {
		.gpio_base         = GPIO_CAM_EXPANDER_BASE,
		.oscio_is_gpo      = false,
		.io_pullup_ena     = 0x0,
		.io_pulldn_ena     = 0xc0,
		.io_open_drain_ena = 0x0,
		.irq_summary       = -1,
	},
	[SX150X_LIQUID] = {
		.gpio_base         = GPIO_LIQUID_EXPANDER_BASE,
		.oscio_is_gpo      = false,
		.io_pullup_ena     = 0x0c08,
		.io_pulldn_ena     = 0x4060,
		.io_open_drain_ena = 0x000c,
		.io_polarity       = 0,
		.irq_summary       = -1,
	},
};

#endif

#define MSM_PMEM_ADSP_SIZE         0x7800000
#ifdef CONFIG_I2C
/*20120106_Jack: Add for Touch I2C*/
#define MSM_8960_GSBI3_QUP_I2C_BUS_ID 3	
// [2011/12/22] Jackal - porting for westlake
#define MSM_8960_GSBI12_QUP_I2C_BUS_ID 12
// [2011/12/22] Jackal - porting for westlake End
#endif

#define MSM_PMEM_AUDIO_SIZE        0x2B4000
#define MSM_PMEM_SIZE 0x4000000 /* 64 Mbytes */
#define MSM_LIQUID_PMEM_SIZE 0x4000000 /* 64 Mbytes */
#define MSM_HDMI_PRIM_PMEM_SIZE 0x4000000 /* 64 Mbytes */

#ifdef CONFIG_MSM_MULTIMEDIA_USE_ION
#define MSM_PMEM_KERNEL_EBI1_SIZE  0x280000
#define MSM_ION_SF_SIZE		MSM_PMEM_SIZE
#define MSM_ION_MM_FW_SIZE	0x200000
#define MSM_ION_MM_SIZE		MSM_PMEM_ADSP_SIZE
#define MSM_ION_QSECOM_SIZE	0x600000 /* (6MB) */
#define MSM_ION_MFC_SIZE	SZ_8K
#define MSM_ION_AUDIO_SIZE	MSM_PMEM_AUDIO_SIZE
#define MSM_ION_HEAP_NUM	8
#define MSM_LIQUID_ION_MM_SIZE (MSM_ION_MM_SIZE + 0x600000)
#define MSM_LIQUID_ION_SF_SIZE MSM_LIQUID_PMEM_SIZE
#define MSM_HDMI_PRIM_ION_SF_SIZE MSM_HDMI_PRIM_PMEM_SIZE

#define MSM8960_FIXED_AREA_START 0xb0000000
#define MAX_FIXED_AREA_SIZE	0x10000000
#define MSM_MM_FW_SIZE		0x280000
#define MSM8960_FW_START	(MSM8960_FIXED_AREA_START - MSM_MM_FW_SIZE)

static unsigned msm_ion_sf_size = MSM_ION_SF_SIZE;
#else
#define MSM_PMEM_KERNEL_EBI1_SIZE  0x110C000
#define MSM_ION_HEAP_NUM	1
#endif

static char chip_jtag_id[6], sbl_jtag_id[6];

#ifdef CONFIG_KERNEL_PMEM_EBI_REGION
static unsigned pmem_kernel_ebi1_size = MSM_PMEM_KERNEL_EBI1_SIZE;
static int __init pmem_kernel_ebi1_size_setup(char *p)
{
	pmem_kernel_ebi1_size = memparse(p, NULL);
	return 0;
}
early_param("pmem_kernel_ebi1_size", pmem_kernel_ebi1_size_setup);
#endif

#ifdef CONFIG_ANDROID_PMEM
static unsigned pmem_size = MSM_PMEM_SIZE;
static unsigned pmem_param_set;
static int __init pmem_size_setup(char *p)
{
	pmem_size = memparse(p, NULL);
	pmem_param_set = 1;
	return 0;
}
early_param("pmem_size", pmem_size_setup);

static unsigned pmem_adsp_size = MSM_PMEM_ADSP_SIZE;

static int __init pmem_adsp_size_setup(char *p)
{
	pmem_adsp_size = memparse(p, NULL);
	return 0;
}
early_param("pmem_adsp_size", pmem_adsp_size_setup);

static unsigned pmem_audio_size = MSM_PMEM_AUDIO_SIZE;

static int __init pmem_audio_size_setup(char *p)
{
	pmem_audio_size = memparse(p, NULL);
	return 0;
}
early_param("pmem_audio_size", pmem_audio_size_setup);
#endif

#ifdef CONFIG_ANDROID_PMEM
#ifndef CONFIG_MSM_MULTIMEDIA_USE_ION
static struct android_pmem_platform_data android_pmem_pdata = {
	.name = "pmem",
	.allocator_type = PMEM_ALLOCATORTYPE_ALLORNOTHING,
	.cached = 1,
	.memory_type = MEMTYPE_EBI1,
};

static struct platform_device android_pmem_device = {
	.name = "android_pmem",
	.id = 0,
	.dev = {.platform_data = &android_pmem_pdata},
};

static struct android_pmem_platform_data android_pmem_adsp_pdata = {
	.name = "pmem_adsp",
	.allocator_type = PMEM_ALLOCATORTYPE_BITMAP,
	.cached = 0,
	.memory_type = MEMTYPE_EBI1,
};
static struct platform_device android_pmem_adsp_device = {
	.name = "android_pmem",
	.id = 2,
	.dev = { .platform_data = &android_pmem_adsp_pdata },
};
#endif

static struct android_pmem_platform_data android_pmem_audio_pdata = {
	.name = "pmem_audio",
	.allocator_type = PMEM_ALLOCATORTYPE_BITMAP,
	.cached = 0,
	.memory_type = MEMTYPE_EBI1,
};

static struct platform_device android_pmem_audio_device = {
	.name = "android_pmem",
	.id = 4,
	.dev = { .platform_data = &android_pmem_audio_pdata },
};
#endif

struct fmem_platform_data fmem_pdata = {
};

#define DSP_RAM_BASE_8960 0x8da00000
#define DSP_RAM_SIZE_8960 0x1800000
static int dspcrashd_pdata_8960 = 0xDEADDEAD;

static struct resource resources_dspcrashd_8960[] = {
	{
		.name   = "msm_dspcrashd",
		.start  = DSP_RAM_BASE_8960,
		.end    = DSP_RAM_BASE_8960 + DSP_RAM_SIZE_8960,
		.flags  = IORESOURCE_DMA,
	},
};

static struct platform_device msm_device_dspcrashd_8960 = {
	.name           = "msm_dspcrashd",
	.num_resources  = ARRAY_SIZE(resources_dspcrashd_8960),
	.resource       = resources_dspcrashd_8960,
	.dev = { .platform_data = &dspcrashd_pdata_8960 },
};

static struct memtype_reserve msm8960_reserve_table[] __initdata = {
	[MEMTYPE_SMI] = {
	},
	[MEMTYPE_EBI0] = {
		.flags	=	MEMTYPE_FLAGS_1M_ALIGN,
	},
	[MEMTYPE_EBI1] = {
		.flags	=	MEMTYPE_FLAGS_1M_ALIGN,
	},
};

#if defined(CONFIG_MSM_RTB)
static struct msm_rtb_platform_data msm_rtb_pdata = {
	.size = SZ_1M,
};

static int __init msm_rtb_set_buffer_size(char *p)
{
	int s;

	s = memparse(p, NULL);
	msm_rtb_pdata.size = ALIGN(s, SZ_4K);
	return 0;
}
early_param("msm_rtb_size", msm_rtb_set_buffer_size);


static struct platform_device msm_rtb_device = {
	.name           = "msm_rtb",
	.id             = -1,
	.dev            = {
		.platform_data = &msm_rtb_pdata,
	},
};
#endif

static void __init reserve_rtb_memory(void)
{
#if defined(CONFIG_MSM_RTB)
	msm8960_reserve_table[MEMTYPE_EBI1].size += msm_rtb_pdata.size;
#endif
}

static void __init size_pmem_devices(void)
{
#ifdef CONFIG_ANDROID_PMEM
#ifndef CONFIG_MSM_MULTIMEDIA_USE_ION
	android_pmem_adsp_pdata.size = pmem_adsp_size;

	if (!pmem_param_set) {
		if (machine_is_msm8960_liquid())
			pmem_size = MSM_LIQUID_PMEM_SIZE;
		if (hdmi_is_primary)
			pmem_size = MSM_HDMI_PRIM_PMEM_SIZE;
	}

	android_pmem_pdata.size = pmem_size;
#endif
	android_pmem_audio_pdata.size = MSM_PMEM_AUDIO_SIZE;
#endif
}

static void __init reserve_memory_for(struct android_pmem_platform_data *p)
{
	msm8960_reserve_table[p->memory_type].size += p->size;
}

static void __init reserve_pmem_memory(void)
{
#ifdef CONFIG_ANDROID_PMEM
#ifndef CONFIG_MSM_MULTIMEDIA_USE_ION
	reserve_memory_for(&android_pmem_adsp_pdata);
	reserve_memory_for(&android_pmem_pdata);
#endif
	reserve_memory_for(&android_pmem_audio_pdata);
	msm8960_reserve_table[MEMTYPE_EBI1].size += pmem_kernel_ebi1_size;
#endif
}

static int msm8960_paddr_to_memtype(unsigned int paddr)
{
	return MEMTYPE_EBI1;
}

#define FMEM_ENABLED 0

#ifdef CONFIG_ION_MSM
#ifdef CONFIG_MSM_MULTIMEDIA_USE_ION
static struct ion_cp_heap_pdata cp_mm_ion_pdata = {
	.permission_type = IPT_TYPE_MM_CARVEOUT,
	.align = PAGE_SIZE,
	.reusable = FMEM_ENABLED,
	.mem_is_fmem = FMEM_ENABLED,
	.fixed_position = FIXED_MIDDLE,
};

static struct ion_cp_heap_pdata cp_mfc_ion_pdata = {
	.permission_type = IPT_TYPE_MFC_SHAREDMEM,
	.align = PAGE_SIZE,
	.reusable = 0,
	.mem_is_fmem = FMEM_ENABLED,
	.fixed_position = FIXED_HIGH,
};

static struct ion_co_heap_pdata co_ion_pdata = {
	.adjacent_mem_id = INVALID_HEAP_ID,
	.align = PAGE_SIZE,
	.mem_is_fmem = 0,
};

static struct ion_co_heap_pdata fw_co_ion_pdata = {
	.adjacent_mem_id = ION_CP_MM_HEAP_ID,
	.align = SZ_128K,
	.mem_is_fmem = FMEM_ENABLED,
	.fixed_position = FIXED_LOW,
};
#endif

/**
 * These heaps are listed in the order they will be allocated. Due to
 * video hardware restrictions and content protection the FW heap has to
 * be allocated adjacent (below) the MM heap and the MFC heap has to be
 * allocated after the MM heap to ensure MFC heap is not more than 256MB
 * away from the base address of the FW heap.
 * However, the order of FW heap and MM heap doesn't matter since these
 * two heaps are taken care of by separate code to ensure they are adjacent
 * to each other.
 * Don't swap the order unless you know what you are doing!
 */
static struct ion_platform_data ion_pdata = {
	.nr = MSM_ION_HEAP_NUM,
	.heaps = {
		{
			.id	= ION_SYSTEM_HEAP_ID,
			.type	= ION_HEAP_TYPE_SYSTEM,
			.name	= ION_VMALLOC_HEAP_NAME,
		},
#ifdef CONFIG_MSM_MULTIMEDIA_USE_ION
		{
			.id	= ION_CP_MM_HEAP_ID,
			.type	= ION_HEAP_TYPE_CP,
			.name	= ION_MM_HEAP_NAME,
			.size	= MSM_ION_MM_SIZE,
			.memory_type = ION_EBI_TYPE,
			.extra_data = (void *) &cp_mm_ion_pdata,
		},
		{
			.id	= ION_MM_FIRMWARE_HEAP_ID,
			.type	= ION_HEAP_TYPE_CARVEOUT,
			.name	= ION_MM_FIRMWARE_HEAP_NAME,
			.size	= MSM_ION_MM_FW_SIZE,
			.memory_type = ION_EBI_TYPE,
			.extra_data = (void *) &fw_co_ion_pdata,
		},
		{
			.id	= ION_CP_MFC_HEAP_ID,
			.type	= ION_HEAP_TYPE_CP,
			.name	= ION_MFC_HEAP_NAME,
			.size	= MSM_ION_MFC_SIZE,
			.memory_type = ION_EBI_TYPE,
			.extra_data = (void *) &cp_mfc_ion_pdata,
		},
		{
			.id	= ION_SF_HEAP_ID,
			.type	= ION_HEAP_TYPE_CARVEOUT,
			.name	= ION_SF_HEAP_NAME,
			.size	= MSM_ION_SF_SIZE,
			.memory_type = ION_EBI_TYPE,
			.extra_data = (void *) &co_ion_pdata,
		},
		{
			.id	= ION_IOMMU_HEAP_ID,
			.type	= ION_HEAP_TYPE_IOMMU,
			.name	= ION_IOMMU_HEAP_NAME,
		},
		{
			.id	= ION_QSECOM_HEAP_ID,
			.type	= ION_HEAP_TYPE_CARVEOUT,
			.name	= ION_QSECOM_HEAP_NAME,
			.size	= MSM_ION_QSECOM_SIZE,
			.memory_type = ION_EBI_TYPE,
			.extra_data = (void *) &co_ion_pdata,
		},
		{
			.id	= ION_AUDIO_HEAP_ID,
			.type	= ION_HEAP_TYPE_CARVEOUT,
			.name	= ION_AUDIO_HEAP_NAME,
			.size	= MSM_ION_AUDIO_SIZE,
			.memory_type = ION_EBI_TYPE,
			.extra_data = (void *) &co_ion_pdata,
		},
#endif
	}
};

static struct platform_device ion_dev = {
	.name = "ion-msm",
	.id = 1,
	.dev = { .platform_data = &ion_pdata },
};
#endif

struct platform_device fmem_device = {
	.name = "fmem",
	.id = 1,
	.dev = { .platform_data = &fmem_pdata },
};

static void __init adjust_mem_for_liquid(void)
{
	unsigned int i;

	if (!pmem_param_set) {
		if (machine_is_msm8960_liquid())
			msm_ion_sf_size = MSM_LIQUID_ION_SF_SIZE;

		if (hdmi_is_primary)
			msm_ion_sf_size = MSM_HDMI_PRIM_ION_SF_SIZE;

		if (machine_is_msm8960_liquid() || hdmi_is_primary) {
			for (i = 0; i < ion_pdata.nr; i++) {
				if (ion_pdata.heaps[i].id == ION_SF_HEAP_ID) {
					ion_pdata.heaps[i].size =
						msm_ion_sf_size;
					pr_debug("msm_ion_sf_size 0x%x\n",
						msm_ion_sf_size);
					break;
				}
			}
		}
	}
}

static void __init reserve_mem_for_ion(enum ion_memory_types mem_type,
				      unsigned long size)
{
	msm8960_reserve_table[mem_type].size += size;
}

static void __init msm8960_reserve_fixed_area(unsigned long fixed_area_size)
{
#if defined(CONFIG_ION_MSM) && defined(CONFIG_MSM_MULTIMEDIA_USE_ION)
	int ret;

	if (fixed_area_size > MAX_FIXED_AREA_SIZE)
		panic("fixed area size is larger than %dM\n",
			MAX_FIXED_AREA_SIZE >> 20);

	reserve_info->fixed_area_size = fixed_area_size;
	reserve_info->fixed_area_start = MSM8960_FW_START;

	ret = memblock_remove(reserve_info->fixed_area_start,
		reserve_info->fixed_area_size);
	BUG_ON(ret);
#endif
}

/**
 * Reserve memory for ION and calculate amount of reusable memory for fmem.
 * We only reserve memory for heaps that are not reusable. However, we only
 * support one reusable heap at the moment so we ignore the reusable flag for
 * other than the first heap with reusable flag set. Also handle special case
 * for video heaps (MM,FW, and MFC). Video requires heaps MM and MFC to be
 * at a higher address than FW in addition to not more than 256MB away from the
 * base address of the firmware. This means that if MM is reusable the other
 * two heaps must be allocated in the same region as FW. This is handled by the
 * mem_is_fmem flag in the platform data. In addition the MM heap must be
 * adjacent to the FW heap for content protection purposes.
 */
static void __init reserve_ion_memory(void)
{
#if defined(CONFIG_ION_MSM) && defined(CONFIG_MSM_MULTIMEDIA_USE_ION)
	unsigned int i;
	unsigned int reusable_count = 0;
	unsigned int fixed_size = 0;
	unsigned int fixed_low_size, fixed_middle_size, fixed_high_size;
	unsigned long fixed_low_start, fixed_middle_start, fixed_high_start;

	adjust_mem_for_liquid();
	fmem_pdata.size = 0;
	fmem_pdata.reserved_size_low = 0;
	fmem_pdata.reserved_size_high = 0;
	fixed_low_size = 0;
	fixed_middle_size = 0;
	fixed_high_size = 0;

	/* We only support 1 reusable heap. Check if more than one heap
	 * is specified as reusable and set as non-reusable if found.
	 */
	for (i = 0; i < ion_pdata.nr; ++i) {
		const struct ion_platform_heap *heap = &(ion_pdata.heaps[i]);

		if (heap->type == ION_HEAP_TYPE_CP && heap->extra_data) {
			struct ion_cp_heap_pdata *data = heap->extra_data;

			reusable_count += (data->reusable) ? 1 : 0;

			if (data->reusable && reusable_count > 1) {
				pr_err("%s: Too many heaps specified as "
					"reusable. Heap %s was not configured "
					"as reusable.\n", __func__, heap->name);
				data->reusable = 0;
			}
		}
	}

	for (i = 0; i < ion_pdata.nr; ++i) {
		const struct ion_platform_heap *heap = &(ion_pdata.heaps[i]);

		if (heap->extra_data) {
			int fixed_position = NOT_FIXED;
			int mem_is_fmem = 0;

			switch (heap->type) {
			case ION_HEAP_TYPE_CP:
				mem_is_fmem = ((struct ion_cp_heap_pdata *)
					heap->extra_data)->mem_is_fmem;
				fixed_position = ((struct ion_cp_heap_pdata *)
					heap->extra_data)->fixed_position;
				break;
			case ION_HEAP_TYPE_CARVEOUT:
				mem_is_fmem = ((struct ion_co_heap_pdata *)
					heap->extra_data)->mem_is_fmem;
				fixed_position = ((struct ion_co_heap_pdata *)
					heap->extra_data)->fixed_position;
				break;
			default:
				break;
			}

			if (fixed_position != NOT_FIXED)
				fixed_size += heap->size;
			else
				reserve_mem_for_ion(MEMTYPE_EBI1, heap->size);

			if (fixed_position == FIXED_LOW)
				fixed_low_size += heap->size;
			else if (fixed_position == FIXED_MIDDLE)
				fixed_middle_size += heap->size;
			else if (fixed_position == FIXED_HIGH)
				fixed_high_size += heap->size;

			if (mem_is_fmem)
				fmem_pdata.size += heap->size;
		}
	}

	if (!fixed_size)
		return;

	if (fmem_pdata.size) {
		fmem_pdata.reserved_size_low = fixed_low_size;
		fmem_pdata.reserved_size_high = fixed_high_size;
	}

	msm8960_reserve_fixed_area(fixed_size + MSM_MM_FW_SIZE);

	fixed_low_start = MSM8960_FIXED_AREA_START;
	fixed_middle_start = fixed_low_start + fixed_low_size;
	fixed_high_start = fixed_middle_start + fixed_middle_size;

	for (i = 0; i < ion_pdata.nr; ++i) {
		struct ion_platform_heap *heap = &(ion_pdata.heaps[i]);

		if (heap->extra_data) {
			int fixed_position = NOT_FIXED;

			switch (heap->type) {
			case ION_HEAP_TYPE_CP:
				fixed_position = ((struct ion_cp_heap_pdata *)
					heap->extra_data)->fixed_position;
				break;
			case ION_HEAP_TYPE_CARVEOUT:
				fixed_position = ((struct ion_co_heap_pdata *)
					heap->extra_data)->fixed_position;
				break;
			default:
				break;
			}

			switch (fixed_position) {
			case FIXED_LOW:
				heap->base = fixed_low_start;
				break;
			case FIXED_MIDDLE:
				heap->base = fixed_middle_start;
				break;
			case FIXED_HIGH:
				heap->base = fixed_high_start;
				break;
			default:
				break;
			}
		}
	}
#endif
}

static void __init reserve_mdp_memory(void)
{
	msm8960_mdp_writeback(msm8960_reserve_table);
}

#if defined(CONFIG_MSM_CACHE_DUMP)
static struct msm_cache_dump_platform_data msm_cache_dump_pdata = {
	.l2_size = L2_BUFFER_SIZE,
};

static struct platform_device msm_cache_dump_device = {
	.name           = "msm_cache_dump",
	.id             = -1,
	.dev            = {
		.platform_data = &msm_cache_dump_pdata,
	},
};

#endif

static void reserve_cache_dump_memory(void)
{
#ifdef CONFIG_MSM_CACHE_DUMP
	unsigned int spare;
	unsigned int l1_size;
	unsigned int total;
	int ret;

	ret = scm_call(L1C_SERVICE_ID, L1C_BUFFER_GET_SIZE_COMMAND_ID, &spare,
		sizeof(spare), &l1_size, sizeof(l1_size));

	if (ret)
		/* Fall back to something reasonable here */
		l1_size = L1_BUFFER_SIZE;

	total = l1_size + L2_BUFFER_SIZE;

	msm8960_reserve_table[MEMTYPE_EBI1].size += total;
	msm_cache_dump_pdata.l1_size = l1_size;
#endif
}

static void __init msm8960_calculate_reserve_sizes(void)
{
	size_pmem_devices();
	reserve_pmem_memory();
	reserve_ion_memory();
	reserve_mdp_memory();
	reserve_rtb_memory();
	reserve_cache_dump_memory();
}

static struct reserve_info msm8960_reserve_info __initdata = {
	.memtype_reserve_table = msm8960_reserve_table,
	.calculate_reserve_sizes = msm8960_calculate_reserve_sizes,
	.reserve_fixed_area = msm8960_reserve_fixed_area,
	.paddr_to_memtype = msm8960_paddr_to_memtype,
};

static int msm8960_memory_bank_size(void)
{
	return 1<<29;
}

static void __init locate_unstable_memory(void)
{
	struct membank *mb = &meminfo.bank[meminfo.nr_banks - 1];
	unsigned long bank_size;
	unsigned long low, high;

	bank_size = msm8960_memory_bank_size();
	low = meminfo.bank[0].start;
	high = mb->start + mb->size;

	/* Check if 32 bit overflow occured */
	if (high < mb->start)
		high = ~0UL;

	low &= ~(bank_size - 1);

	if (high - low <= bank_size)
		return;

	msm8960_reserve_info.bank_size = bank_size;
#ifdef CONFIG_ENABLE_DMM
	msm8960_reserve_info.low_unstable_address = mb->start -
					MIN_MEMORY_BLOCK_SIZE + mb->size;
	msm8960_reserve_info.max_unstable_size = MIN_MEMORY_BLOCK_SIZE;
	pr_info("low unstable address %lx max size %lx bank size %lx\n",
		msm8960_reserve_info.low_unstable_address,
		msm8960_reserve_info.max_unstable_size,
		msm8960_reserve_info.bank_size);
#else
	msm8960_reserve_info.low_unstable_address = 0;
	msm8960_reserve_info.max_unstable_size = 0;
#endif
}

static void __init place_movable_zone(void)
{
#ifdef CONFIG_ENABLE_DMM
	movable_reserved_start = msm8960_reserve_info.low_unstable_address;
	movable_reserved_size = msm8960_reserve_info.max_unstable_size;
	pr_info("movable zone start %lx size %lx\n",
		movable_reserved_start, movable_reserved_size);
#endif
}

static void __init msm8960_early_memory(void)
{
	reserve_info = &msm8960_reserve_info;
	locate_unstable_memory();
	place_movable_zone();
}

static char prim_panel_name[PANEL_NAME_MAX_LEN];
static char ext_panel_name[PANEL_NAME_MAX_LEN];
static int __init prim_display_setup(char *param)
{
	if (strnlen(param, PANEL_NAME_MAX_LEN))
		strlcpy(prim_panel_name, param, PANEL_NAME_MAX_LEN);
	return 0;
}
early_param("prim_display", prim_display_setup);

static int __init ext_display_setup(char *param)
{
	if (strnlen(param, PANEL_NAME_MAX_LEN))
		strlcpy(ext_panel_name, param, PANEL_NAME_MAX_LEN);
	return 0;
}
early_param("ext_display", ext_display_setup);
#ifdef CONFIG_ANDROID_RAM_CONSOLE
static struct resource ram_console_resources[] = {
	{
		.flags = IORESOURCE_MEM,
	},
};

static struct platform_device ram_console_device = {
	.name 		= "ram_console",
	.id = -1,
	.num_resources	= ARRAY_SIZE(ram_console_resources),
	.resource	= ram_console_resources,
};

void __init msm8960_ram_console_debug_reserve(unsigned long ram_console_size)
{
	struct resource *res;
  long ret;

  res = platform_get_resource(&ram_console_device, IORESOURCE_MEM, 0);
  if (!res)
		goto fail;
  res->start = memblock_end_of_DRAM() - ram_console_size;
  res->end = res->start + ram_console_size - 1;
  ret = memblock_remove(res->start, ram_console_size);
  if (ret)
   	goto fail;
  return;

fail:
           ram_console_device.resource = NULL;
           ram_console_device.num_resources = 0;
           pr_err("Failed to reserve memory block for ram console\n");
}
#endif

static void __init msm8960_reserve(void)
{
	msm8960_set_display_params(prim_panel_name, ext_panel_name);
	msm_reserve();
	if (fmem_pdata.size) {
#if defined(CONFIG_ION_MSM) && defined(CONFIG_MSM_MULTIMEDIA_USE_ION)
		fmem_pdata.phys = reserve_info->fixed_area_start +
			MSM_MM_FW_SIZE;
		pr_info("mm fw at %lx (fixed) size %x\n",
			reserve_info->fixed_area_start, MSM_MM_FW_SIZE);
		pr_info("fmem start %lx (fixed) size %lx\n",
			fmem_pdata.phys, fmem_pdata.size);
#else
		fmem_pdata.phys = reserve_memory_for_fmem(fmem_pdata.size);
#endif
	}
#ifdef CONFIG_ANDROID_RAM_CONSOLE
	msm8960_ram_console_debug_reserve(SZ_1M);
#endif
}

static int msm8960_change_memory_power(u64 start, u64 size,
	int change_type)
{
	return soc_change_memory_power(start, size, change_type);
}

// [2011/12/22] Jackal - porting for westlake
#ifdef CONFIG_MPU_SENSORS_MPU3050
static struct mpu_platform_data mpu3050_data = {
       .int_config  = 0x10,
       .orientation = {  1,  0,  0,
			   0,  -1,  0,
			   0,  0, -1 },
       .level_shifter = 0,
	  
	/* accel */
	.accel = {
#ifdef CONFIG_MPU_SENSORS_MPU3050_MODULE
		 .get_slave_descr = NULL,
#else
		 .get_slave_descr = get_accel_slave_descr,
#endif
//        .irq = MSM_GPIO_TO_INT(PEGA_MSM_GPIO_ACCEL_INT),  // Jackal add
		 .adapt_num   = MSM_8960_GSBI12_QUP_I2C_BUS_ID,
		 .bus         = EXT_SLAVE_BUS_SECONDARY,
		 //.bus         = EXT_SLAVE_BUS_PRIMARY,
		 .address     = 0x0F,
		 .orientation = {  0,  -1,  0,
				    -1,  0,  0,
				    0,  0, -1 },
	 },
	/* compass */
	.compass = {
#ifdef CONFIG_MPU_SENSORS_MPU3050_MODULE
		 .get_slave_descr = NULL,
#else
		 .get_slave_descr = get_compass_slave_descr,
#endif
		 .adapt_num   = MSM_8960_GSBI12_QUP_I2C_BUS_ID,
		 .bus         = EXT_SLAVE_BUS_PRIMARY,
		 .address     = 0x0C,
		 .orientation = { 0, -1, 0,
				  -1, 0, 0,
				  0, 0, -1 },
	 },
};

static struct i2c_board_info sanpaolo_mpu3050_i2c_boardinfo[] = {

       {
		I2C_BOARD_INFO("mpu3050", 0x68),
//                .irq = PM8058_GPIO_IRQ(PM8058_IRQ_BASE,PEGA_MSMPIO_GYRO_INT - 1),  // Jackal add
	        .platform_data = &mpu3050_data,
	},
};

static struct i2c_board_info sanpaolo_isl29023_i2c_boardinfo[] = {

       {
		I2C_BOARD_INFO("isl29023", 0x44),
       },
};
#endif
// [2011/12/22] Jackal - porting for westlake End

/*Start PEGA detect HW ID Type*/
static int pega_hw_board_version = 1;
static int __init pega_hw_board_version_setup(char *cmdstr)
{
	if (strncmp(cmdstr, "0", 1) == 0) {
		pega_hw_board_version = 0;
	}

	if (strncmp(cmdstr, "1", 1) == 0) {
		pega_hw_board_version = 1;
	}

	if (strncmp(cmdstr, "2", 1) == 0) {
		pega_hw_board_version = 2;
	}

	if (strncmp(cmdstr, "3", 1) == 0) {
		pega_hw_board_version = 3;
	}

	if (strncmp(cmdstr, "4", 1) == 0) {
		pega_hw_board_version = 4;
	}

	if (strncmp(cmdstr, "5", 1) == 0) {
		pega_hw_board_version = 5;
	}
	
	printk("Check pega hw board version = %d", pega_hw_board_version);	

	return 1;
}
__setup("androidboot.hw_type_version=", pega_hw_board_version_setup);

/* PEGA.Jeremy1_Wang.120409.Begin[Fuse blown check] */
static int __init pega_chip_jtag_id_setup(char *cmdstr)
{
	strcpy(chip_jtag_id, cmdstr);
	return 1;
}

__setup("androidboot.chip_jtag_id=", pega_chip_jtag_id_setup);

static int __init pega_sbl_jtag_id_setup(char *cmdstr)
{
	strcpy(sbl_jtag_id, cmdstr);
	return 1;
}

__setup("androidboot.sbl_jtag_id=", pega_sbl_jtag_id_setup);

char *chip_id(void)
{
	return chip_jtag_id;
}

char *sbl_id(void)
{
	return sbl_jtag_id;
}
/* PEGA.Jeremy1_Wang.120409.End */

int get_pega_hw_board_version_status(void)
{
	return pega_hw_board_version;
}
/*End PEGA detect HW ID Type*/

/* PEGA.ChiaFeng.120319.Begin[Silent reset] */
static int handle_silent_reset = 0;

static int __init silent_reset_setup(char *cmdstr)
{
	if (strncmp(cmdstr, "1", 1) == 0)
		handle_silent_reset = 1;
	else
		handle_silent_reset = 0;

	//printk("handle_silent_reset = %d", handle_silent_reset);

	return 1;
}
__setup("androidboot.silent_reset=", silent_reset_setup);

int get_silent_reset_status(void)
{
	return handle_silent_reset;
}
/* PEGA.ChiaFeng.120319.End */

static void __init msm8960_allocate_memory_regions(void)
{
	msm8960_allocate_fb_region();
}

#ifdef CONFIG_WCD9310_CODEC

#define TABLA_INTERRUPT_BASE (NR_MSM_IRQS + NR_GPIO_IRQS + NR_PM8921_IRQS)

/* Micbias setting is based on 8660 CDP/MTP/FLUID requirement
 * 4 micbiases are used to power various analog and digital
 * microphones operating at 1800 mV. Technically, all micbiases
 * can source from single cfilter since all microphones operate
 * at the same voltage level. The arrangement below is to make
 * sure all cfilters are exercised. LDO_H regulator ouput level
 * does not need to be as high as 2.85V. It is choosen for
 * microphone sensitivity purpose.
 */
static struct tabla_pdata tabla_platform_data = {
	.slimbus_slave_device = {
		.name = "tabla-slave",
		.e_addr = {0, 0, 0x10, 0, 0x17, 2},
	},
	.irq = MSM_GPIO_TO_INT(62),
	.irq_base = TABLA_INTERRUPT_BASE,
	.num_irqs = NR_TABLA_IRQS,
	.reset_gpio = PM8921_GPIO_PM_TO_SYS(34),
	.micbias = {
		.ldoh_v = TABLA_LDOH_2P85_V,
		.cfilt1_mv = 1800,
		.cfilt2_mv = 2700,
		.cfilt3_mv = 1800,
		.bias1_cfilt_sel = TABLA_CFILT1_SEL,
		.bias2_cfilt_sel = TABLA_CFILT2_SEL,
		.bias3_cfilt_sel = TABLA_CFILT3_SEL,
		.bias4_cfilt_sel = TABLA_CFILT3_SEL,
	},
	.regulator = {
	{
		.name = "CDC_VDD_CP",
		.min_uV = 1800000,
		.max_uV = 1800000,
		.optimum_uA = WCD9XXX_CDC_VDDA_CP_CUR_MAX,
	},
	{
		.name = "CDC_VDDA_RX",
		.min_uV = 1800000,
		.max_uV = 1800000,
		.optimum_uA = WCD9XXX_CDC_VDDA_RX_CUR_MAX,
	},
	{
		.name = "CDC_VDDA_TX",
		.min_uV = 1800000,
		.max_uV = 1800000,
		.optimum_uA = WCD9XXX_CDC_VDDA_TX_CUR_MAX,
	},
	{
		.name = "VDDIO_CDC",
		.min_uV = 1800000,
		.max_uV = 1800000,
		.optimum_uA = WCD9XXX_VDDIO_CDC_CUR_MAX,
	},
	{
		.name = "VDDD_CDC_D",
		.min_uV = 1225000,
		.max_uV = 1225000,
		.optimum_uA = WCD9XXX_VDDD_CDC_D_CUR_MAX,
	},
	{
		.name = "CDC_VDDA_A_1P2V",
		.min_uV = 1225000,
		.max_uV = 1225000,
		.optimum_uA = WCD9XXX_VDDD_CDC_A_CUR_MAX,
	},
	},
};

static struct slim_device msm_slim_tabla = {
	.name = "tabla-slim",
	.e_addr = {0, 1, 0x10, 0, 0x17, 2},
	.dev = {
		.platform_data = &tabla_platform_data,
	},
};

static struct tabla_pdata tabla20_platform_data = {
	.slimbus_slave_device = {
		.name = "tabla-slave",
		.e_addr = {0, 0, 0x60, 0, 0x17, 2},
	},
	.irq = MSM_GPIO_TO_INT(62),
	.irq_base = TABLA_INTERRUPT_BASE,
	.num_irqs = NR_TABLA_IRQS,
	.reset_gpio = PM8921_GPIO_PM_TO_SYS(34),
	.micbias = {
		.ldoh_v = TABLA_LDOH_2P85_V,
		.cfilt1_mv = 1800,
		.cfilt2_mv = 2700,
		.cfilt3_mv = 1800,
		.bias1_cfilt_sel = TABLA_CFILT1_SEL,
		.bias2_cfilt_sel = TABLA_CFILT2_SEL,
		.bias3_cfilt_sel = TABLA_CFILT3_SEL,
		.bias4_cfilt_sel = TABLA_CFILT3_SEL,
	},
	.regulator = {
	{
		.name = "CDC_VDD_CP",
		.min_uV = 1800000,
		.max_uV = 1800000,
		.optimum_uA = WCD9XXX_CDC_VDDA_CP_CUR_MAX,
	},
	{
		.name = "CDC_VDDA_RX",
		.min_uV = 1800000,
		.max_uV = 1800000,
		.optimum_uA = WCD9XXX_CDC_VDDA_RX_CUR_MAX,
	},
	{
		.name = "CDC_VDDA_TX",
		.min_uV = 1800000,
		.max_uV = 1800000,
		.optimum_uA = WCD9XXX_CDC_VDDA_TX_CUR_MAX,
	},
	{
		.name = "VDDIO_CDC",
		.min_uV = 1800000,
		.max_uV = 1800000,
		.optimum_uA = WCD9XXX_VDDIO_CDC_CUR_MAX,
	},
	{
		.name = "VDDD_CDC_D",
		.min_uV = 1225000,
		.max_uV = 1225000,
		.optimum_uA = WCD9XXX_VDDD_CDC_D_CUR_MAX,
	},
	{
		.name = "CDC_VDDA_A_1P2V",
		.min_uV = 1225000,
		.max_uV = 1225000,
		.optimum_uA = WCD9XXX_VDDD_CDC_A_CUR_MAX,
	},
	},
};

static struct slim_device msm_slim_tabla20 = {
	.name = "tabla2x-slim",
	.e_addr = {0, 1, 0x60, 0, 0x17, 2},
	.dev = {
		.platform_data = &tabla20_platform_data,
	},
};
#endif

static struct slim_boardinfo msm_slim_devices[] = {
#ifdef CONFIG_WCD9310_CODEC
	{
		.bus_num = 1,
		.slim_slave = &msm_slim_tabla,
	},
	{
		.bus_num = 1,
		.slim_slave = &msm_slim_tabla20,
	},
#endif
	/* add more slimbus slaves as needed */
};

#define MSM_WCNSS_PHYS	0x03000000
#define MSM_WCNSS_SIZE	0x280000

static struct resource resources_wcnss_wlan[] = {
	{
		.start	= RIVA_APPS_WLAN_RX_DATA_AVAIL_IRQ,
		.end	= RIVA_APPS_WLAN_RX_DATA_AVAIL_IRQ,
		.name	= "wcnss_wlanrx_irq",
		.flags	= IORESOURCE_IRQ,
	},
	{
		.start	= RIVA_APPS_WLAN_DATA_XFER_DONE_IRQ,
		.end	= RIVA_APPS_WLAN_DATA_XFER_DONE_IRQ,
		.name	= "wcnss_wlantx_irq",
		.flags	= IORESOURCE_IRQ,
	},
	{
		.start	= MSM_WCNSS_PHYS,
		.end	= MSM_WCNSS_PHYS + MSM_WCNSS_SIZE - 1,
		.name	= "wcnss_mmio",
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= 84,
		.end	= 88,
		.name	= "wcnss_gpios_5wire",
		.flags	= IORESOURCE_IO,
	},
};

static struct qcom_wcnss_opts qcom_wcnss_pdata = {
//#if defined(PEGA_SMT_BUILD)
	.has_48mhz_xo   = 1,
//#else
//	.has_48mhz_xo	= 0,
//#endif
};

static struct platform_device msm_device_wcnss_wlan = {
	.name		= "wcnss_wlan",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(resources_wcnss_wlan),
	.resource	= resources_wcnss_wlan,
	.dev		= {.platform_data = &qcom_wcnss_pdata},
};

#if defined(CONFIG_CRYPTO_DEV_QCRYPTO) || \
		defined(CONFIG_CRYPTO_DEV_QCRYPTO_MODULE) || \
		defined(CONFIG_CRYPTO_DEV_QCEDEV) || \
		defined(CONFIG_CRYPTO_DEV_QCEDEV_MODULE)

#define QCE_SIZE		0x10000
#define QCE_0_BASE		0x18500000

#define QCE_HW_KEY_SUPPORT	0
#define QCE_SHA_HMAC_SUPPORT	1
#define QCE_SHARE_CE_RESOURCE	1
#define QCE_CE_SHARED		0

/* Begin Bus scaling definitions */
static struct msm_bus_vectors crypto_hw_init_vectors[] = {
	{
		.src = MSM_BUS_MASTER_ADM_PORT0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 0,
		.ib = 0,
	},
	{
		.src = MSM_BUS_MASTER_ADM_PORT1,
		.dst = MSM_BUS_SLAVE_GSBI1_UART,
		.ab = 0,
		.ib = 0,
	},
};

static struct msm_bus_vectors crypto_hw_active_vectors[] = {
	{
		.src = MSM_BUS_MASTER_ADM_PORT0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 70000000UL,
		.ib = 70000000UL,
	},
	{
		.src = MSM_BUS_MASTER_ADM_PORT1,
		.dst = MSM_BUS_SLAVE_GSBI1_UART,
		.ab = 2480000000UL,
		.ib = 2480000000UL,
	},
};

static struct msm_bus_paths crypto_hw_bus_scale_usecases[] = {
	{
		ARRAY_SIZE(crypto_hw_init_vectors),
		crypto_hw_init_vectors,
	},
	{
		ARRAY_SIZE(crypto_hw_active_vectors),
		crypto_hw_active_vectors,
	},
};

static struct msm_bus_scale_pdata crypto_hw_bus_scale_pdata = {
		crypto_hw_bus_scale_usecases,
		ARRAY_SIZE(crypto_hw_bus_scale_usecases),
		.name = "cryptohw",
};
/* End Bus Scaling Definitions*/

static struct resource qcrypto_resources[] = {
	[0] = {
		.start = QCE_0_BASE,
		.end = QCE_0_BASE + QCE_SIZE - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.name = "crypto_channels",
		.start = DMOV_CE_IN_CHAN,
		.end = DMOV_CE_OUT_CHAN,
		.flags = IORESOURCE_DMA,
	},
	[2] = {
		.name = "crypto_crci_in",
		.start = DMOV_CE_IN_CRCI,
		.end = DMOV_CE_IN_CRCI,
		.flags = IORESOURCE_DMA,
	},
	[3] = {
		.name = "crypto_crci_out",
		.start = DMOV_CE_OUT_CRCI,
		.end = DMOV_CE_OUT_CRCI,
		.flags = IORESOURCE_DMA,
	},
};

static struct resource qcedev_resources[] = {
	[0] = {
		.start = QCE_0_BASE,
		.end = QCE_0_BASE + QCE_SIZE - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.name = "crypto_channels",
		.start = DMOV_CE_IN_CHAN,
		.end = DMOV_CE_OUT_CHAN,
		.flags = IORESOURCE_DMA,
	},
	[2] = {
		.name = "crypto_crci_in",
		.start = DMOV_CE_IN_CRCI,
		.end = DMOV_CE_IN_CRCI,
		.flags = IORESOURCE_DMA,
	},
	[3] = {
		.name = "crypto_crci_out",
		.start = DMOV_CE_OUT_CRCI,
		.end = DMOV_CE_OUT_CRCI,
		.flags = IORESOURCE_DMA,
	},
};

#endif

#if defined(CONFIG_CRYPTO_DEV_QCRYPTO) || \
		defined(CONFIG_CRYPTO_DEV_QCRYPTO_MODULE)

static struct msm_ce_hw_support qcrypto_ce_hw_suppport = {
	.ce_shared = QCE_CE_SHARED,
	.shared_ce_resource = QCE_SHARE_CE_RESOURCE,
	.hw_key_support = QCE_HW_KEY_SUPPORT,
	.sha_hmac = QCE_SHA_HMAC_SUPPORT,
	.bus_scale_table = &crypto_hw_bus_scale_pdata,
};

static struct platform_device qcrypto_device = {
	.name		= "qcrypto",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(qcrypto_resources),
	.resource	= qcrypto_resources,
	.dev		= {
		.coherent_dma_mask = DMA_BIT_MASK(32),
		.platform_data = &qcrypto_ce_hw_suppport,
	},
};
#endif

#if defined(CONFIG_CRYPTO_DEV_QCEDEV) || \
		defined(CONFIG_CRYPTO_DEV_QCEDEV_MODULE)

static struct msm_ce_hw_support qcedev_ce_hw_suppport = {
	.ce_shared = QCE_CE_SHARED,
	.shared_ce_resource = QCE_SHARE_CE_RESOURCE,
	.hw_key_support = QCE_HW_KEY_SUPPORT,
	.sha_hmac = QCE_SHA_HMAC_SUPPORT,
	.bus_scale_table = &crypto_hw_bus_scale_pdata,
};

static struct platform_device qcedev_device = {
	.name		= "qce",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(qcedev_resources),
	.resource	= qcedev_resources,
	.dev		= {
		.coherent_dma_mask = DMA_BIT_MASK(32),
		.platform_data = &qcedev_ce_hw_suppport,
	},
};
#endif

#define MDM2AP_ERRFATAL			70
#define AP2MDM_ERRFATAL			95
#define MDM2AP_STATUS			69
#define AP2MDM_STATUS			94
#define AP2MDM_PMIC_RESET_N		80
#define AP2MDM_KPDPWR_N			81

static struct resource mdm_resources[] = {
	{
		.start	= MDM2AP_ERRFATAL,
		.end	= MDM2AP_ERRFATAL,
		.name	= "MDM2AP_ERRFATAL",
		.flags	= IORESOURCE_IO,
	},
	{
		.start	= AP2MDM_ERRFATAL,
		.end	= AP2MDM_ERRFATAL,
		.name	= "AP2MDM_ERRFATAL",
		.flags	= IORESOURCE_IO,
	},
	{
		.start	= MDM2AP_STATUS,
		.end	= MDM2AP_STATUS,
		.name	= "MDM2AP_STATUS",
		.flags	= IORESOURCE_IO,
	},
	{
		.start	= AP2MDM_STATUS,
		.end	= AP2MDM_STATUS,
		.name	= "AP2MDM_STATUS",
		.flags	= IORESOURCE_IO,
	},
	{
		.start	= AP2MDM_PMIC_RESET_N,
		.end	= AP2MDM_PMIC_RESET_N,
		.name	= "AP2MDM_PMIC_RESET_N",
		.flags	= IORESOURCE_IO,
	},
	{
		.start	= AP2MDM_KPDPWR_N,
		.end	= AP2MDM_KPDPWR_N,
		.name	= "AP2MDM_KPDPWR_N",
		.flags	= IORESOURCE_IO,
	},
};

static struct mdm_platform_data mdm_platform_data = {
	.mdm_version = "2.5",
};

static struct platform_device mdm_device = {
	.name		= "mdm2_modem",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(mdm_resources),
	.resource	= mdm_resources,
	.dev		= {
		.platform_data = &mdm_platform_data,
	},
};

static struct platform_device *mdm_devices[] __initdata = {
	&mdm_device,
};

#define MSM_SHARED_RAM_PHYS 0x80000000

static void __init msm8960_map_io(void)
{
	msm_shared_ram_phys = MSM_SHARED_RAM_PHYS;
	msm_map_msm8960_io();

	if (socinfo_init() < 0)
		pr_err("socinfo_init() failed!\n");
}

static void __init msm8960_init_irq(void)
{
	msm_mpm_irq_extn_init();
	gic_init(0, GIC_PPI_START, MSM_QGIC_DIST_BASE,
						(void *)MSM_QGIC_CPU_BASE);

	/* Edge trigger PPIs except AVS_SVICINT and AVS_SVICINTSWDONE */
	writel_relaxed(0xFFFFD7FF, MSM_QGIC_DIST_BASE + GIC_DIST_CONFIG + 4);

	writel_relaxed(0x0000FFFF, MSM_QGIC_DIST_BASE + GIC_DIST_ENABLE_SET);
	mb();
}

static void __init msm8960_init_buses(void)
{
#ifdef CONFIG_MSM_BUS_SCALING
	msm_bus_rpm_set_mt_mask();
	msm_bus_8960_apps_fabric_pdata.rpm_enabled = 1;
	msm_bus_8960_sys_fabric_pdata.rpm_enabled = 1;
	msm_bus_8960_mm_fabric_pdata.rpm_enabled = 1;
	msm_bus_apps_fabric.dev.platform_data =
		&msm_bus_8960_apps_fabric_pdata;
	msm_bus_sys_fabric.dev.platform_data = &msm_bus_8960_sys_fabric_pdata;
	msm_bus_mm_fabric.dev.platform_data = &msm_bus_8960_mm_fabric_pdata;
	msm_bus_sys_fpb.dev.platform_data = &msm_bus_8960_sys_fpb_pdata;
	msm_bus_cpss_fpb.dev.platform_data = &msm_bus_8960_cpss_fpb_pdata;
#endif
}

static struct msm_spi_platform_data msm8960_qup_spi_gsbi1_pdata = {
	.max_clock_speed = 15060000,
};

#ifdef CONFIG_USB_MSM_OTG_72K
static struct msm_otg_platform_data msm_otg_pdata;
#else
static int wr_phy_init_seq[] = {
	0x44, 0x80, /* set VBUS valid threshold
			and disconnect valid threshold */
	/* BEGIN-20120208-Yisheng_Chiu-Fine tune usb driving current */
	0x34, 0x81, /* update DC voltage level */
	0x3e, 0x82, /* set preemphasis and rise/fall time */
	0x33, 0x83, /* set source impedance adjusment */
	/* END-20120208-Yisheng_Chiu-Fine tune usb driving current */
	-1};

static int wr_phy_init_seq_smt[] = {
	0x44, 0x80, /* set VBUS valid threshold
			and disconnect valid threshold */
	0x3f, 0x81, /* update DC voltage level */
	0x3e, 0x82, /* set preemphasis and rise/fall time */
	0x33, 0x83, /* set source impedance adjusment */
	-1};

static int liquid_v1_phy_init_seq[] = {
	0x44, 0x80,/* set VBUS valid threshold
			and disconnect valid threshold */
	0x3C, 0x81,/* update DC voltage level */
	0x18, 0x82,/* set preemphasis and rise/fall time */
	0x23, 0x83,/* set source impedance sdjusment */
	-1};

#define OTG_CHG_DISABLE_GPIO		PM8921_GPIO_PM_TO_SYS(3)

	
static void pmic_id_detect(int val)
{
#if 0
 	pr_info("%s(): val = %d\n", __func__, val);

       if(val==0)
	   	otg_disable_input_current(1);
#else
       int rc;
	   
	struct pm_gpio otg_chg_disable_gpio_config = {
		.direction      = PM_GPIO_DIR_OUT,
		.pull           = PM_GPIO_PULL_NO,
		.out_strength   = PM_GPIO_STRENGTH_HIGH,
		.function       = PM_GPIO_FUNC_NORMAL,
		.inv_int_pol    = 0,
		.vin_sel        = 2,
		.output_buffer  = PM_GPIO_OUT_BUF_CMOS,
		.output_value   = 0,
	};

 	pr_info("%s(): val = %d\n", __func__, val);

 	if (val== 1) {
		otg_chg_disable_gpio_config.output_value=0;
		rc = pm8xxx_gpio_config(OTG_CHG_DISABLE_GPIO, &otg_chg_disable_gpio_config);
		if (rc) {
			pr_err("%s: pm8921 gpio %d config failed(%d)\n",
					__func__, OTG_CHG_DISABLE_GPIO, rc);
			return;
		}
	}else
	{
		otg_chg_disable_gpio_config.output_value=1;
		rc = pm8xxx_gpio_config(OTG_CHG_DISABLE_GPIO, &otg_chg_disable_gpio_config);
		if (rc) {
			pr_err("%s: pm8921 gpio %d config failed(%d)\n",
					__func__, OTG_CHG_DISABLE_GPIO, rc);
			return;
		}	
	}
#endif	

}

#ifdef CONFIG_MSM_BUS_SCALING
/* Bandwidth requests (zero) if no vote placed */
static struct msm_bus_vectors usb_init_vectors[] = {
	{
		.src = MSM_BUS_MASTER_SPS,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 0,
		.ib = 0,
	},
};

/* Bus bandwidth requests in Bytes/sec */
static struct msm_bus_vectors usb_max_vectors[] = {
	{
		.src = MSM_BUS_MASTER_SPS,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 60000000,		/* At least 480Mbps on bus. */
		.ib = 960000000,	/* MAX bursts rate */
	},
};

static struct msm_bus_paths usb_bus_scale_usecases[] = {
	{
		ARRAY_SIZE(usb_init_vectors),
		usb_init_vectors,
	},
	{
		ARRAY_SIZE(usb_max_vectors),
		usb_max_vectors,
	},
};

static struct msm_bus_scale_pdata usb_bus_scale_pdata = {
	usb_bus_scale_usecases,
	ARRAY_SIZE(usb_bus_scale_usecases),
	.name = "usb",
};
#endif

static struct msm_otg_platform_data msm_otg_pdata = {
	.mode			= USB_OTG,
	.otg_control		= OTG_PMIC_CONTROL,
	.phy_type		= SNPS_28NM_INTEGRATED_PHY,
	.pmic_id_irq		= PM8921_USB_ID_IN_IRQ(PM8921_IRQ_BASE),
	.power_budget		= 2000, //750 => 2000 
#ifdef CONFIG_MSM_BUS_SCALING
	.bus_scale_table	= &usb_bus_scale_pdata,
#endif
	.pmic_id                = pmic_id_detect,
};
#endif

#ifdef CONFIG_USB_EHCI_MSM_HSIC
#define HSIC_HUB_RESET_GPIO	91
static struct msm_hsic_host_platform_data msm_hsic_pdata = {
	.strobe		= 150,
	.data		= 151,
};
//#else
//static struct msm_hsic_host_platform_data msm_hsic_pdata;
#endif

#define PID_MAGIC_ID		0x71432909
#define SERIAL_NUM_MAGIC_ID	0x61945374
#define SERIAL_NUMBER_LENGTH	127
#define DLOAD_USB_BASE_ADD	0x2A03F0C8

struct magic_num_struct {
	uint32_t pid;
	uint32_t serial_num;
};

struct dload_struct {
	uint32_t	reserved1;
	uint32_t	reserved2;
	uint32_t	reserved3;
	uint16_t	reserved4;
	uint16_t	pid;
	char		serial_number[SERIAL_NUMBER_LENGTH];
	uint16_t	reserved5;
	struct magic_num_struct magic_struct;
};

static int usb_diag_update_pid_and_serial_num(uint32_t pid, const char *snum)
{
	struct dload_struct __iomem *dload = 0;

	dload = ioremap(DLOAD_USB_BASE_ADD, sizeof(*dload));
	if (!dload) {
		pr_err("%s: cannot remap I/O memory region: %08x\n",
					__func__, DLOAD_USB_BASE_ADD);
		return -ENXIO;
	}

	pr_debug("%s: dload:%p pid:%x serial_num:%s\n",
				__func__, dload, pid, snum);
	/* update pid */
	dload->magic_struct.pid = PID_MAGIC_ID;
	dload->pid = pid;

	/* update serial number */
	dload->magic_struct.serial_num = 0;
	if (!snum) {
		memset(dload->serial_number, 0, SERIAL_NUMBER_LENGTH);
		goto out;
	}

	dload->magic_struct.serial_num = SERIAL_NUM_MAGIC_ID;
	strlcpy(dload->serial_number, snum, SERIAL_NUMBER_LENGTH);
out:
	iounmap(dload);
	return 0;
}

static struct android_usb_platform_data android_usb_pdata = {
	.update_pid_and_serial_num = usb_diag_update_pid_and_serial_num,
};

static struct platform_device android_usb_device = {
	.name	= "android_usb",
	.id	= -1,
	.dev	= {
		.platform_data = &android_usb_pdata,
	},
};

static int is_default_serialno = 0;

static int __init board_serialno_setup(char *serialno)
{

	if (strncmp(serialno, "12345678", 8) == 0)
	{
		is_default_serialno=1;
		msm_otg_pdata.phy_init_seq = wr_phy_init_seq_smt;
       }else
       {
		is_default_serialno=0;
               msm_otg_pdata.phy_init_seq = wr_phy_init_seq;
       }

	return 1;
}
__setup("androidboot.serialno=", board_serialno_setup);


int is_default_serialno_status(void)
{
	return is_default_serialno;
}

static uint8_t spm_wfi_cmd_sequence[] __initdata = {
			0x03, 0x0f,
};

static uint8_t spm_power_collapse_without_rpm[] __initdata = {
			0x00, 0x24, 0x54, 0x10,
			0x09, 0x03, 0x01,
			0x10, 0x54, 0x30, 0x0C,
			0x24, 0x30, 0x0f,
};

static uint8_t spm_power_collapse_with_rpm[] __initdata = {
			0x00, 0x24, 0x54, 0x10,
			0x09, 0x07, 0x01, 0x0B,
			0x10, 0x54, 0x30, 0x0C,
			0x24, 0x30, 0x0f,
};

static struct msm_spm_seq_entry msm_spm_seq_list[] __initdata = {
	[0] = {
		.mode = MSM_SPM_MODE_CLOCK_GATING,
		.notify_rpm = false,
		.cmd = spm_wfi_cmd_sequence,
	},
	[1] = {
		.mode = MSM_SPM_MODE_POWER_COLLAPSE,
		.notify_rpm = false,
		.cmd = spm_power_collapse_without_rpm,
	},
	[2] = {
		.mode = MSM_SPM_MODE_POWER_COLLAPSE,
		.notify_rpm = true,
		.cmd = spm_power_collapse_with_rpm,
	},
};

static struct msm_spm_platform_data msm_spm_data[] __initdata = {
	[0] = {
		.reg_base_addr = MSM_SAW0_BASE,
		.reg_init_values[MSM_SPM_REG_SAW2_CFG] = 0x1F,
#if defined(CONFIG_MSM_AVS_HW)
		.reg_init_values[MSM_SPM_REG_SAW2_AVS_CTL] = 0x00,
		.reg_init_values[MSM_SPM_REG_SAW2_AVS_HYSTERESIS] = 0x00,
#endif
		.reg_init_values[MSM_SPM_REG_SAW2_SPM_CTL] = 0x01,
		.reg_init_values[MSM_SPM_REG_SAW2_PMIC_DLY] = 0x02020204,
		.reg_init_values[MSM_SPM_REG_SAW2_PMIC_DATA_0] = 0x0060009C,
		.reg_init_values[MSM_SPM_REG_SAW2_PMIC_DATA_1] = 0x0000001C,
		.vctl_timeout_us = 50,
		.num_modes = ARRAY_SIZE(msm_spm_seq_list),
		.modes = msm_spm_seq_list,
	},
	[1] = {
		.reg_base_addr = MSM_SAW1_BASE,
		.reg_init_values[MSM_SPM_REG_SAW2_CFG] = 0x1F,
#if defined(CONFIG_MSM_AVS_HW)
		.reg_init_values[MSM_SPM_REG_SAW2_AVS_CTL] = 0x00,
		.reg_init_values[MSM_SPM_REG_SAW2_AVS_HYSTERESIS] = 0x00,
#endif
		.reg_init_values[MSM_SPM_REG_SAW2_SPM_CTL] = 0x01,
		.reg_init_values[MSM_SPM_REG_SAW2_PMIC_DLY] = 0x02020204,
		.reg_init_values[MSM_SPM_REG_SAW2_PMIC_DATA_0] = 0x0060009C,
		.reg_init_values[MSM_SPM_REG_SAW2_PMIC_DATA_1] = 0x0000001C,
		.vctl_timeout_us = 50,
		.num_modes = ARRAY_SIZE(msm_spm_seq_list),
		.modes = msm_spm_seq_list,
	},
};

static uint8_t l2_spm_wfi_cmd_sequence[] __initdata = {
			0x00, 0x20, 0x03, 0x20,
			0x00, 0x0f,
};

static uint8_t l2_spm_gdhs_cmd_sequence[] __initdata = {
			0x00, 0x20, 0x34, 0x64,
			0x48, 0x07, 0x48, 0x20,
			0x50, 0x64, 0x04, 0x34,
			0x50, 0x0f,
};
static uint8_t l2_spm_power_off_cmd_sequence[] __initdata = {
			0x00, 0x10, 0x34, 0x64,
			0x48, 0x07, 0x48, 0x10,
			0x50, 0x64, 0x04, 0x34,
			0x50, 0x0F,
};

static struct msm_spm_seq_entry msm_spm_l2_seq_list[] __initdata = {
	[0] = {
		.mode = MSM_SPM_L2_MODE_RETENTION,
		.notify_rpm = false,
		.cmd = l2_spm_wfi_cmd_sequence,
	},
	[1] = {
		.mode = MSM_SPM_L2_MODE_GDHS,
		.notify_rpm = true,
		.cmd = l2_spm_gdhs_cmd_sequence,
	},
	[2] = {
		.mode = MSM_SPM_L2_MODE_POWER_COLLAPSE,
		.notify_rpm = true,
		.cmd = l2_spm_power_off_cmd_sequence,
	},
};

static struct msm_spm_platform_data msm_spm_l2_data[] __initdata = {
	[0] = {
		.reg_base_addr = MSM_SAW_L2_BASE,
		.reg_init_values[MSM_SPM_REG_SAW2_SPM_CTL] = 0x00,
		.reg_init_values[MSM_SPM_REG_SAW2_PMIC_DLY] = 0x02020204,
		.reg_init_values[MSM_SPM_REG_SAW2_PMIC_DATA_0] = 0x00A000AE,
		.reg_init_values[MSM_SPM_REG_SAW2_PMIC_DATA_1] = 0x00A00020,
		.modes = msm_spm_l2_seq_list,
		.num_modes = ARRAY_SIZE(msm_spm_l2_seq_list),
	},
};

#define PM_HAP_EN_GPIO		PM8921_GPIO_PM_TO_SYS(33)
#define PM_HAP_LEN_GPIO		PM8921_GPIO_PM_TO_SYS(20)

static struct msm_xo_voter *xo_handle_d1;

static int isa1200_power(int on)
{
	int rc = 0;

	gpio_set_value(HAP_SHIFT_LVL_OE_GPIO, !!on);

	rc = on ? msm_xo_mode_vote(xo_handle_d1, MSM_XO_MODE_ON) :
			msm_xo_mode_vote(xo_handle_d1, MSM_XO_MODE_OFF);
	if (rc < 0) {
		pr_err("%s: failed to %svote for TCXO D1 buffer%d\n",
				__func__, on ? "" : "de-", rc);
		goto err_xo_vote;
	}

	return 0;

err_xo_vote:
	gpio_set_value(HAP_SHIFT_LVL_OE_GPIO, !on);
	return rc;
}

static int isa1200_dev_setup(bool enable)
{
	int rc = 0;

	struct pm_gpio hap_gpio_config = {
		.direction      = PM_GPIO_DIR_OUT,
		.pull           = PM_GPIO_PULL_NO,
		.out_strength   = PM_GPIO_STRENGTH_HIGH,
		.function       = PM_GPIO_FUNC_NORMAL,
		.inv_int_pol    = 0,
		.vin_sel        = 2,
		.output_buffer  = PM_GPIO_OUT_BUF_CMOS,
		.output_value   = 0,
	};

	if (enable == true) {
		rc = pm8xxx_gpio_config(PM_HAP_EN_GPIO, &hap_gpio_config);
		if (rc) {
			pr_err("%s: pm8921 gpio %d config failed(%d)\n",
					__func__, PM_HAP_EN_GPIO, rc);
			return rc;
		}

		rc = pm8xxx_gpio_config(PM_HAP_LEN_GPIO, &hap_gpio_config);
		if (rc) {
			pr_err("%s: pm8921 gpio %d config failed(%d)\n",
					__func__, PM_HAP_LEN_GPIO, rc);
			return rc;
		}

		rc = gpio_request(HAP_SHIFT_LVL_OE_GPIO, "hap_shft_lvl_oe");
		if (rc) {
			pr_err("%s: unable to request gpio %d (%d)\n",
					__func__, HAP_SHIFT_LVL_OE_GPIO, rc);
			return rc;
		}

		rc = gpio_direction_output(HAP_SHIFT_LVL_OE_GPIO, 0);
		if (rc) {
			pr_err("%s: Unable to set direction\n", __func__);
			goto free_gpio;
		}

		xo_handle_d1 = msm_xo_get(MSM_XO_TCXO_D1, "isa1200");
		if (IS_ERR(xo_handle_d1)) {
			rc = PTR_ERR(xo_handle_d1);
			pr_err("%s: failed to get the handle for D1(%d)\n",
							__func__, rc);
			goto gpio_set_dir;
		}
	} else {
		gpio_free(HAP_SHIFT_LVL_OE_GPIO);

		msm_xo_put(xo_handle_d1);
	}

	return 0;

gpio_set_dir:
	gpio_set_value(HAP_SHIFT_LVL_OE_GPIO, 0);
free_gpio:
	gpio_free(HAP_SHIFT_LVL_OE_GPIO);
	return rc;
}

static struct isa1200_regulator isa1200_reg_data[] = {
	{
		.name = "vcc_i2c",
		.min_uV = ISA_I2C_VTG_MIN_UV,
		.max_uV = ISA_I2C_VTG_MAX_UV,
		.load_uA = ISA_I2C_CURR_UA,
	},
};

static struct isa1200_platform_data isa1200_1_pdata = {
	.name = "vibrator",
	.dev_setup = isa1200_dev_setup,
	.power_on = isa1200_power,
	.hap_en_gpio = PM_HAP_EN_GPIO,
	.hap_len_gpio = PM_HAP_LEN_GPIO,
	.max_timeout = 15000,
	.mode_ctrl = PWM_GEN_MODE,
	.pwm_fd = {
		.pwm_div = 256,
	},
	.is_erm = false,
	.smart_en = true,
	.ext_clk_en = true,
	.chip_en = 1,
	.regulator_info = isa1200_reg_data,
	.num_regulators = ARRAY_SIZE(isa1200_reg_data),
};

static struct i2c_board_info msm_isa1200_board_info[] __initdata = {
	{
		I2C_BOARD_INFO("isa1200_1", 0x90>>1),
	},
};

static struct i2c_board_info sii_device_info[] __initdata = {
	{
		I2C_BOARD_INFO("Sil-9244", 0x39),
		.flags = I2C_CLIENT_WAKE,
		.irq = MSM_GPIO_TO_INT(15),
	},
};
/* BEGIN Chaoyen_Wu@pegatron [2012.3.13][Add front camera new schematic support after DVT version in 1032 codebase]*/
static struct msm_i2c_platform_data msm8960_i2c_qup_gsbi1_pdata = {
	.clk_freq = 100000,
	.src_clk_rate = 24000000,
};

/* END Chaoyen_Wu@pegatron [2012.3.13][Add front camera new schematic support after DVT version in 1032 codebase]*/

static struct msm_i2c_platform_data msm8960_i2c_qup_gsbi4_pdata = {
	.clk_freq = 100000,
	.src_clk_rate = 24000000,
};

/* BEGIN Dom_Lin@pegatron [2012/03/19] [Add camera LED new schematic support after DVT version in 1032 codebase] */
static struct msm_i2c_platform_data msm8960_i2c_qup_gsbi5_pdata = {
	.clk_freq = 100000,
	.src_clk_rate = 24000000,
	.use_gsbi_shared_mode = 1,
};
/* END Dom_Lin@pegatron [2012/03/19] [Add camera LED new schematic support after DVT version in 1032 codebase] */

static struct msm_i2c_platform_data msm8960_i2c_qup_gsbi3_pdata = {
#ifdef CONFIG_TOUCHSCREEN_LGD_I2C
	.clk_freq = 400000,
#else
	.clk_freq = 100000,
#endif
	.src_clk_rate = 24000000,
};

static struct msm_i2c_platform_data msm8960_i2c_qup_gsbi7_pdata = {
	.clk_freq = 100000,	//0216
	.src_clk_rate = 24000000,
};
/*robert1_chen, 20120409, for Dock GSBI change*/
static struct msm_i2c_platform_data msm8960_i2c_qup_gsbi9_pdata = {
	.clk_freq = 100000,	//0313
	.src_clk_rate = 24000000,
};
static struct msm_i2c_platform_data msm8960_i2c_qup_gsbi10_pdata = {
	.clk_freq = 100000,
	.src_clk_rate = 24000000,
};

static struct msm_i2c_platform_data msm8960_i2c_qup_gsbi12_pdata = {
// BEGIN Eric_Tsau@pegatroncorp.com [2012/03/29] [Upgrade i2c clock rate of gsbi12]
	.clk_freq = 400000,
// END Eric_Tsau@pegatroncorp.com [2012/03/29] [Upgrade i2c clock rate of gsbi12]
	.src_clk_rate = 24000000,
};

static struct msm_rpm_platform_data msm_rpm_data = {
	.reg_base_addrs = {
		[MSM_RPM_PAGE_STATUS] = MSM_RPM_BASE,
		[MSM_RPM_PAGE_CTRL] = MSM_RPM_BASE + 0x400,
		[MSM_RPM_PAGE_REQ] = MSM_RPM_BASE + 0x600,
		[MSM_RPM_PAGE_ACK] = MSM_RPM_BASE + 0xa00,
	},

	.irq_ack = RPM_APCC_CPU0_GP_HIGH_IRQ,
	.irq_err = RPM_APCC_CPU0_GP_LOW_IRQ,
	.irq_vmpm = RPM_APCC_CPU0_GP_MEDIUM_IRQ,
	.msm_apps_ipc_rpm_reg = MSM_APCS_GCC_BASE + 0x008,
	.msm_apps_ipc_rpm_val = 4,
};

static struct msm_pm_sleep_status_data msm_pm_slp_sts_data = {
	.base_addr = MSM_ACC0_BASE + 0x08,
	.cpu_offset = MSM_ACC1_BASE - MSM_ACC0_BASE,
	.mask = 1UL << 13,
};

static struct ks8851_pdata spi_eth_pdata = {
	.irq_gpio = KS8851_IRQ_GPIO,
	.rst_gpio = KS8851_RST_GPIO,
};

static struct spi_board_info spi_board_info[] __initdata = {
	{
		.modalias               = "ks8851",
		.irq                    = MSM_GPIO_TO_INT(KS8851_IRQ_GPIO),
		.max_speed_hz           = 19200000,
		.bus_num                = 0,
		.chip_select            = 0,
		.mode                   = SPI_MODE_0,
		.platform_data		= &spi_eth_pdata
	},
	{
		.modalias               = "dsi_novatek_3d_panel_spi",
		.max_speed_hz           = 10800000,
		.bus_num                = 0,
		.chip_select            = 1,
		.mode                   = SPI_MODE_0,
	},
};

static struct platform_device msm_device_saw_core0 = {
	.name          = "saw-regulator",
	.id            = 0,
	.dev	= {
		.platform_data = &msm_saw_regulator_pdata_s5,
	},
};

static struct platform_device msm_device_saw_core1 = {
	.name          = "saw-regulator",
	.id            = 1,
	.dev	= {
		.platform_data = &msm_saw_regulator_pdata_s6,
	},
};

static struct tsens_platform_data msm_tsens_pdata  = {
		.slope			= {910, 910, 910, 910, 910},
		.tsens_factor		= 1000,
		.hw_type		= MSM_8960,
		.tsens_num_sensor	= 5,
};

static struct platform_device msm_tsens_device = {
        .name   = "tsens8960-tm",
        .id     = -1,
        .dev    = {
                .platform_data = &msm_tsens_pdata,
        },
};

#ifdef CONFIG_MSM_FAKE_BATTERY
static struct platform_device fish_battery_device = {
	.name = "fish_battery",
};
#endif

static struct platform_device msm8960_device_ext_5v_vreg __devinitdata = {
	.name	= GPIO_REGULATOR_DEV_NAME,
	.id	= PM8921_MPP_PM_TO_SYS(7),
	.dev	= {
		.platform_data = &msm_gpio_regulator_pdata[GPIO_VREG_ID_EXT_5V],
	},
};

static struct platform_device msm8960_device_ext_l2_vreg __devinitdata = {
	.name	= GPIO_REGULATOR_DEV_NAME,
	.id	= 91,
	.dev	= {
		.platform_data = &msm_gpio_regulator_pdata[GPIO_VREG_ID_EXT_L2],
	},
};

static struct platform_device msm8960_device_ext_3p3v_vreg __devinitdata = {
	.name	= GPIO_REGULATOR_DEV_NAME,
	.id	= PM8921_GPIO_PM_TO_SYS(17),
	.dev	= {
		.platform_data =
			&msm_gpio_regulator_pdata[GPIO_VREG_ID_EXT_3P3V],
	},
};

static struct platform_device msm8960_device_ext_otg_sw_vreg __devinitdata = {
	.name	= GPIO_REGULATOR_DEV_NAME,
	.id	= PM8921_GPIO_PM_TO_SYS(42),
	.dev	= {
		.platform_data =
			&msm_gpio_regulator_pdata[GPIO_VREG_ID_EXT_OTG_SW],
	},
};

static struct platform_device msm8960_device_rpm_regulator __devinitdata = {
	.name	= "rpm-regulator",
	.id	= -1,
	.dev	= {
		.platform_data = &msm_rpm_regulator_pdata,
	},
};

static struct msm_rpm_log_platform_data msm_rpm_log_pdata = {
	.phys_addr_base = 0x0010C000,
	.reg_offsets = {
		[MSM_RPM_LOG_PAGE_INDICES] = 0x00000080,
		[MSM_RPM_LOG_PAGE_BUFFER]  = 0x000000A0,
	},
	.phys_size = SZ_8K,
	.log_len = 4096,		  /* log's buffer length in bytes */
	.log_len_mask = (4096 >> 2) - 1,  /* length mask in units of u32 */
};

static struct platform_device msm_rpm_log_device = {
	.name	= "msm_rpm_log",
	.id	= -1,
	.dev	= {
		.platform_data = &msm_rpm_log_pdata,
	},
};

//BEGIN Shengjie_Yu, 20111220, setup gpio key mapping table
#ifdef CONFIG_KEYBOARD_GPIO

//BEGIN Shengjie_Yu, 20111228, switch gpio setting of Vol up/down to match target placement
#define PMIC_GPIO_02_VLU 2
#define PMIC_GPIO_01_VLD 1

static struct gpio_keys_button gpio_keys_buttons[] = {
        {
                .code           = KEY_VOLUMEDOWN,
                .gpio           = PM8921_GPIO_PM_TO_SYS(PMIC_GPIO_01_VLD),
                .desc           = "key-vl",
                .active_low     = 1,
                .type           = EV_KEY,
                .wakeup         = 0
        },
        {
                .code           = KEY_VOLUMEUP,
                .gpio           = PM8921_GPIO_PM_TO_SYS(PMIC_GPIO_02_VLU),
//END Shengjie_Yu, 20111228, switch gpio setting of Vol up/down to match target placement
                .desc           = "key-vu",
                .active_low     = 1,
                .type           = EV_KEY,
                .wakeup         = 0
        },
};

static struct gpio_keys_platform_data gpio_keys_data = {
        .buttons        = gpio_keys_buttons,
        .nbuttons       = ARRAY_SIZE(gpio_keys_buttons),
        .rep            = 0,
};

static struct platform_device pmic_gpio_keys = {
        .name           = "gpio-keys",
        .id             = -1,
        .dev            = {
                .platform_data  = &gpio_keys_data,
        },
};
#endif
//END Shengjie_Yu, 20111220, setup gpio key mapping table

/* BEGIN Acme_Wen@pegatroncorp.com [2011/12/21] [for hall sensor] */
#ifdef CONFIG_SENSORS_APX9132
static struct hall_sensor_platform_data hsensor_pdata = {
        .state             = 0,
        .gpio              = PM8921_GPIO_PM_TO_SYS(31),
};

static struct platform_device hall_sensor_pdata = {
        .name           = "hall-sensor",
        .id             = -1,
        .dev            = {
                .platform_data  = &hsensor_pdata,
        },
};
#endif
/* END Acme_Wen@pegatroncorp.com [2011/12/21] [for hall sensor] */

#ifdef CONFIG_EC_API_DEBUG
static struct platform_device ec_dbg_pdata = {
        .name           = "ec_api_dbg",
        .id             = -1,
        .dev            = {
        },
};
#endif

static struct platform_device *common_devices[] __initdata = {
	&msm8960_device_dmov,
	&msm_device_smd,
	&msm8960_device_uart_gsbi5,
	&msm_device_uart_dm6,
	&msm_device_saw_core0,
	&msm_device_saw_core1,
	&msm8960_device_ext_5v_vreg,
	&msm8960_device_ssbi_pmic,
	&msm8960_device_ext_otg_sw_vreg,
#if 0
	/* Weiyang, 20120111, we don't have SPI on GSBI1 */
	&msm8960_device_qup_spi_gsbi1,
#endif
	&msm8960_device_qup_i2c_gsbi3,
	&msm8960_device_qup_i2c_gsbi4,
/* BEGIN Dom_Lin@pegatron [2012/03/19] [Add camera LED new schematic support after DVT version in 1032 codebase] */
	&msm8960_device_qup_i2c_gsbi5,
/* END Dom_Lin@pegatron [2012/03/19] [Add camera LED new schematic support after DVT version in 1032 codebase] */
/* BEGIN Chaoyen_Wu@pegatron [2012.3.13][Add front camera new schematic support after DVT version in 1032 codebase]*/
	&msm8960_device_qup_i2c_gsbi1,
/* END Chaoyen_Wu@pegatron [2012.3.13][Add front camera new schematic support after DVT version in 1032 codebase]*/
	&msm8960_device_qup_i2c_gsbi7,
/*robert1_chen, 20120409, for Dock GSBI change -- BEGIN*/
	&msm8960_device_qup_i2c_gsbi9,
/*robert1_chen, 20120409, for Dock GSBI change -- END*/
	&msm8960_device_qup_i2c_gsbi10,
#ifndef CONFIG_MSM_DSPS
	&msm8960_device_qup_i2c_gsbi12,
#endif
	&msm_slim_ctrl,
	&msm_device_wcnss_wlan,
#if defined(CONFIG_CRYPTO_DEV_QCRYPTO) || \
		defined(CONFIG_CRYPTO_DEV_QCRYPTO_MODULE)
	&qcrypto_device,
#endif

#if defined(CONFIG_CRYPTO_DEV_QCEDEV) || \
		defined(CONFIG_CRYPTO_DEV_QCEDEV_MODULE)
	&qcedev_device,
#endif
#ifdef CONFIG_MSM_ROTATOR
	&msm_rotator_device,
#endif
	&msm_device_sps,
#ifdef CONFIG_MSM_FAKE_BATTERY
	&fish_battery_device,
#endif
	&fmem_device,
#ifdef CONFIG_ANDROID_PMEM
#ifndef CONFIG_MSM_MULTIMEDIA_USE_ION
	&android_pmem_device,
	&android_pmem_adsp_device,
#endif
	&android_pmem_audio_device,
#endif
	&msm_device_vidc,
	&msm_device_bam_dmux,
	&msm_fm_platform_init,

#if defined(CONFIG_TSIF) || defined(CONFIG_TSIF_MODULE)
#ifdef CONFIG_MSM_USE_TSIF1
	&msm_device_tsif[1],
#else
	&msm_device_tsif[0],
#endif
#endif

#ifdef CONFIG_HW_RANDOM_MSM
	&msm_device_rng,
#endif
	&msm_rpm_device,
#ifdef CONFIG_ION_MSM
	&ion_dev,
#endif
	&msm_rpm_log_device,
	&msm_rpm_stat_device,
	&msm_device_tz_log,

#ifdef CONFIG_MSM_QDSS
	&msm_etb_device,
	&msm_tpiu_device,
	&msm_funnel_device,
	&msm_etm_device,
#endif
	&msm_device_dspcrashd_8960,
	&msm8960_device_watchdog,
#ifdef CONFIG_MSM_RTB
	&msm_rtb_device,
#endif
	&msm8960_device_cache_erp,
#ifdef CONFIG_MSM_CACHE_DUMP
	&msm_cache_dump_device,
#endif
#ifdef CONFIG_ANDROID_RAM_CONSOLE
	&ram_console_device,
#endif

};

static struct platform_device *sim_devices[] __initdata = {
	&msm8960_device_otg,
	&msm8960_device_gadget_peripheral,
	&msm_device_hsusb_host,
#ifdef CONFIG_USB_EHCI_MSM_HSIC
	&msm_device_hsic_host,
#endif
	&android_usb_device,
	&msm_device_vidc,
	&msm_bus_apps_fabric,
	&msm_bus_sys_fabric,
	&msm_bus_mm_fabric,
	&msm_bus_sys_fpb,
	&msm_bus_cpss_fpb,
	&msm_pcm,
	&msm_multi_ch_pcm,
	&msm_pcm_routing,
	&msm_cpudai0,
	&msm_cpudai1,
	&msm_cpudai_hdmi_rx,
	&msm_cpudai_bt_rx,
	&msm_cpudai_bt_tx,
	&msm_cpudai_fm_rx,
	&msm_cpudai_fm_tx,
	&msm_cpudai_auxpcm_rx,
	&msm_cpudai_auxpcm_tx,
	&msm_cpu_fe,
	&msm_stub_codec,
	&msm_voice,
	&msm_voip,
	&msm_lpa_pcm,
	&msm_compr_dsp,
	&msm_cpudai_incall_music_rx,
	&msm_cpudai_incall_record_rx,
	&msm_cpudai_incall_record_tx,

#if defined(CONFIG_CRYPTO_DEV_QCRYPTO) || \
		defined(CONFIG_CRYPTO_DEV_QCRYPTO_MODULE)
	&qcrypto_device,
#endif

#if defined(CONFIG_CRYPTO_DEV_QCEDEV) || \
		defined(CONFIG_CRYPTO_DEV_QCEDEV_MODULE)
	&qcedev_device,
#endif
};

static struct platform_device *rumi3_devices[] __initdata = {
	&msm_kgsl_3d0,
	&msm_kgsl_2d0,
	&msm_kgsl_2d1,
#ifdef CONFIG_MSM_GEMINI
	&msm8960_gemini_device,
#endif
};

static struct platform_device *cdp_devices[] __initdata = {
	&msm_8960_q6_lpass,
	&msm_8960_q6_mss_fw,
	&msm_8960_q6_mss_sw,
	&msm_8960_riva,
	&msm_pil_tzapps,
	&msm8960_device_otg,
	&msm8960_device_gadget_peripheral,
	&msm_device_hsusb_host,
	&android_usb_device,
	&msm_pcm,
	&msm_multi_ch_pcm,
	&msm_pcm_routing,
	&msm_cpudai0,
	&msm_cpudai1,
	&msm_cpudai_hdmi_rx,
	&msm_cpudai_bt_rx,
	&msm_cpudai_bt_tx,
	&msm_cpudai_fm_rx,
	&msm_cpudai_fm_tx,
	&msm_cpudai_auxpcm_rx,
	&msm_cpudai_auxpcm_tx,
	&msm_cpu_fe,
	&msm_stub_codec,
	&msm_kgsl_3d0,
#ifdef CONFIG_MSM_KGSL_2D
	&msm_kgsl_2d0,
	&msm_kgsl_2d1,
#endif
#ifdef CONFIG_MSM_GEMINI
	&msm8960_gemini_device,
#endif
	&msm_voice,
	&msm_voip,
	&msm_lpa_pcm,
	&msm_cpudai_afe_01_rx,
	&msm_cpudai_afe_01_tx,
	&msm_cpudai_afe_02_rx,
	&msm_cpudai_afe_02_tx,
	&msm_pcm_afe,
	&msm_compr_dsp,
	&msm_cpudai_incall_music_rx,
	&msm_cpudai_incall_record_rx,
	&msm_cpudai_incall_record_tx,
	&msm_pcm_hostless,
	&msm_bus_apps_fabric,
	&msm_bus_sys_fabric,
	&msm_bus_mm_fabric,
	&msm_bus_sys_fpb,
	&msm_bus_cpss_fpb,
	&msm_tsens_device,
	/*Shengjie_Yu, 111220, GPIO_KEYS define BEGIN*/
#ifdef CONFIG_KEYBOARD_GPIO
	&pmic_gpio_keys,
#endif
	/*Shengjie_Yu, 111220, GPIO_KEYS define END*/
/* BEGIN Acme_Wen@pegatroncorp.com [2011/12/21] [for hall sensor] */
#ifdef CONFIG_SENSORS_APX9132
        &hall_sensor_pdata,
#endif
/* END Acme_Wen@pegatroncorp.com [2011/12/21] [for hall sensor] */
#ifdef CONFIG_EC_API_DEBUG
	& ec_dbg_pdata
#endif
};

/* BEGIN Dom_Lin@pegatron [2012/03/19] [Add camera LED new schematic support after DVT version in 1032 codebase] */
#define MSM_GSBI5_PHYS 0x16400000
#define GSBI_DUAL_MODE_CODE 0x60
/* END Dom_Lin@pegatron [2012/03/19] [Add camera LED new schematic support after DVT version in 1032 codebase] */

static void __init msm8960_i2c_init(void)
{
/* BEGIN Dom_Lin@pegatron [2012/03/19] [Add camera LED new schematic support after DVT version in 1032 codebase] */
	void *gsbi_mem = ioremap_nocache(MSM_GSBI5_PHYS, 4);
	writel_relaxed(GSBI_DUAL_MODE_CODE, gsbi_mem);
	iounmap(gsbi_mem);
	gsbi_mem = NULL;
	msm8960_device_qup_i2c_gsbi5.dev.platform_data =
					&msm8960_i2c_qup_gsbi5_pdata;
/* END Dom_Lin@pegatron [2012/03/19] [Add camera LED new schematic support after DVT version in 1032 codebase] */

	msm8960_device_qup_i2c_gsbi4.dev.platform_data =
					&msm8960_i2c_qup_gsbi4_pdata;

/* BEGIN Chaoyen_Wu@pegatron [2012.3.13][Add front camera new schematic support after DVT version in 1032 codebase]*/
	msm8960_device_qup_i2c_gsbi1.dev.platform_data =
					&msm8960_i2c_qup_gsbi1_pdata;
/* END Chaoyen_Wu@pegatron [2012.3.13][Add front camera new schematic support after DVT version in 1032 codebase]*/

	msm8960_device_qup_i2c_gsbi3.dev.platform_data =
					&msm8960_i2c_qup_gsbi3_pdata;

	msm8960_device_qup_i2c_gsbi7.dev.platform_data =
					&msm8960_i2c_qup_gsbi7_pdata;
/*robert1_chen, 20120409, for Dock GSBI change -- BEGIN*/
	msm8960_device_qup_i2c_gsbi9.dev.platform_data =
					&msm8960_i2c_qup_gsbi9_pdata;
/*robert1_chen, 20120409, for Dock GSBI change -- END*/
	msm8960_device_qup_i2c_gsbi10.dev.platform_data =
					&msm8960_i2c_qup_gsbi10_pdata;

	msm8960_device_qup_i2c_gsbi12.dev.platform_data =
					&msm8960_i2c_qup_gsbi12_pdata;
}

static void __init msm8960_gfx_init(void)
{
	uint32_t soc_platform_version = socinfo_get_version();
	if (SOCINFO_VERSION_MAJOR(soc_platform_version) == 1) {
		struct kgsl_device_platform_data *kgsl_3d0_pdata =
				msm_kgsl_3d0.dev.platform_data;
		kgsl_3d0_pdata->pwrlevel[0].gpu_freq = 320000000;
		kgsl_3d0_pdata->pwrlevel[1].gpu_freq = 266667000;
	}
}

#ifdef CONFIG_PEGA_ISL_CHG
/* BEGIN-20111214-KenLee-add for westlake charging. */
static struct msm_charger_platform_data msm_charger_data = {
	.safety_time = 360,
	/* BEGIN [Jackal_Chen][2012/04/23][Modify safety time of wall charger] */
	.wall_safety_time = 840,
	/* END [Jackal_Chen][2012/04/23][Modify safety time of wall charger] */
	.usb_safety_time = 1800,
	.update_time = 1,
	.max_voltage = 4200,
	.min_voltage = 3200,
};

static struct platform_device msm_charger_device = {
	.name = "msm-charger",
	.id = -1,
	.dev = {
		.platform_data = &msm_charger_data,
	}
};

/* END-20111214-KenLee-add for sanpaolo charging. */
#endif

static struct msm_cpuidle_state msm_cstates[] __initdata = {
	{0, 0, "C0", "WFI",
		MSM_PM_SLEEP_MODE_WAIT_FOR_INTERRUPT},

	{0, 1, "C2", "POWER_COLLAPSE",
		MSM_PM_SLEEP_MODE_POWER_COLLAPSE},

	{1, 0, "C0", "WFI",
		MSM_PM_SLEEP_MODE_WAIT_FOR_INTERRUPT},
};

static struct msm_pm_platform_data msm_pm_data[MSM_PM_SLEEP_MODE_NR * 2] = {
	[MSM_PM_MODE(0, MSM_PM_SLEEP_MODE_POWER_COLLAPSE)] = {
		.idle_supported = 1,
		.suspend_supported = 1,
		.idle_enabled = 0,
		.suspend_enabled = 0,
	},

	[MSM_PM_MODE(0, MSM_PM_SLEEP_MODE_WAIT_FOR_INTERRUPT)] = {
		.idle_supported = 1,
		.suspend_supported = 1,
		.idle_enabled = 1,
		.suspend_enabled = 1,
	},

	[MSM_PM_MODE(1, MSM_PM_SLEEP_MODE_POWER_COLLAPSE)] = {
		.idle_supported = 0,
		.suspend_supported = 1,
		.idle_enabled = 0,
		.suspend_enabled = 0,
	},

	[MSM_PM_MODE(1, MSM_PM_SLEEP_MODE_WAIT_FOR_INTERRUPT)] = {
		.idle_supported = 1,
		.suspend_supported = 0,
		.idle_enabled = 1,
		.suspend_enabled = 0,
	},
};

static struct msm_rpmrs_level msm_rpmrs_levels[] = {
	{
		MSM_PM_SLEEP_MODE_WAIT_FOR_INTERRUPT,
		MSM_RPMRS_LIMITS(ON, ACTIVE, MAX, ACTIVE),
		true,
		100, 8000, 100000, 1,
	},

	{
		MSM_PM_SLEEP_MODE_POWER_COLLAPSE,
		MSM_RPMRS_LIMITS(ON, GDHS, MAX, ACTIVE),
		false,
		4200, 5000, 60350000, 3500,
	},

	{
		MSM_PM_SLEEP_MODE_POWER_COLLAPSE,
		MSM_RPMRS_LIMITS(ON, HSFS_OPEN, MAX, ACTIVE),
		false,
		6300, 4500, 65350000, 4800,
	},
	{
		MSM_PM_SLEEP_MODE_POWER_COLLAPSE,
		MSM_RPMRS_LIMITS(ON, HSFS_OPEN, ACTIVE, RET_HIGH),
		false,
		7000, 3500, 66600000, 5150,
	},

	{
		MSM_PM_SLEEP_MODE_POWER_COLLAPSE,
		MSM_RPMRS_LIMITS(OFF, GDHS, MAX, ACTIVE),
		false,
		11700, 2500, 67850000, 5500,
	},

	{
		MSM_PM_SLEEP_MODE_POWER_COLLAPSE,
		MSM_RPMRS_LIMITS(OFF, HSFS_OPEN, MAX, ACTIVE),
		false,
		13800, 2000, 71850000, 6800,
	},

	{
		MSM_PM_SLEEP_MODE_POWER_COLLAPSE,
		MSM_RPMRS_LIMITS(OFF, HSFS_OPEN, ACTIVE, RET_HIGH),
		false,
		29700, 500, 75850000, 8800,
	},

	{
		MSM_PM_SLEEP_MODE_POWER_COLLAPSE,
		MSM_RPMRS_LIMITS(OFF, HSFS_OPEN, RET_HIGH, RET_LOW),
		false,
		29700, 0, 76350000, 9800,
	},
};

static struct msm_pm_boot_platform_data msm_pm_boot_pdata __initdata = {
	.mode = MSM_PM_BOOT_CONFIG_TZ,
};

uint32_t msm_rpm_get_swfi_latency(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(msm_rpmrs_levels); i++) {
		if (msm_rpmrs_levels[i].sleep_mode ==
			MSM_PM_SLEEP_MODE_WAIT_FOR_INTERRUPT)
			return msm_rpmrs_levels[i].latency_us;
	}

	return 0;
}

#ifdef CONFIG_I2C
#define I2C_SURF 1
#define I2C_FFA  (1 << 1)
#define I2C_RUMI (1 << 2)
#define I2C_SIM  (1 << 3)
#define I2C_FLUID (1 << 4)
#define I2C_LIQUID (1 << 5)

struct i2c_registry {
	u8                     machs;
	int                    bus;
	struct i2c_board_info *info;
	int                    len;
};

/* Sensors DSPS platform data */
#ifdef CONFIG_MSM_DSPS
#define DSPS_PIL_GENERIC_NAME		"dsps"
#endif /* CONFIG_MSM_DSPS */

static void __init msm8960_init_dsps(void)
{
#ifdef CONFIG_MSM_DSPS
	struct msm_dsps_platform_data *pdata =
		msm_dsps_device.dev.platform_data;
	pdata->pil_name = DSPS_PIL_GENERIC_NAME;
	pdata->gpios = NULL;
	pdata->gpios_num = 0;

	platform_device_register(&msm_dsps_device);
#endif /* CONFIG_MSM_DSPS */
}

static int hsic_peripheral_status = 1;
static DEFINE_MUTEX(hsic_status_lock);

void peripheral_connect()
{
	mutex_lock(&hsic_status_lock);
	if (hsic_peripheral_status)
		goto out;
	platform_device_add(&msm_device_hsic_host);
	hsic_peripheral_status = 1;
out:
	mutex_unlock(&hsic_status_lock);
}
EXPORT_SYMBOL(peripheral_connect);

void peripheral_disconnect()
{
	mutex_lock(&hsic_status_lock);
	if (!hsic_peripheral_status)
		goto out;
	platform_device_del(&msm_device_hsic_host);
	hsic_peripheral_status = 0;
out:
	mutex_unlock(&hsic_status_lock);
}
EXPORT_SYMBOL(peripheral_disconnect);

static void __init msm8960_init_hsic(void)
{
#ifdef CONFIG_USB_EHCI_MSM_HSIC
	uint32_t version = socinfo_get_version();

	if (SOCINFO_VERSION_MAJOR(version) == 1)
		return;

	if (PLATFORM_IS_CHARM25() || machine_is_msm8960_liquid())
		platform_device_register(&msm_device_hsic_host);
#endif
}

#ifdef CONFIG_ISL9519_CHARGER

/* BEGIN-20111214-KenLee-add for westlake charging. */
#ifdef CONFIG_PEGA_ISL_CHG

static struct isl_platform_data isl_data __initdata = {
/* BEGIN [2012/03/05] Jackal - Modify charge/input current of 12V AC */
	.chgcurrent         = 3840,
	.input_current      = 1536,
/* END [2012/03/05] Jackal - Modify charge/input current of 12V AC */
	.usb_current        = 384,
/* BEGIN [2012/05/02] Jackal - Modify input current from 384 mA to 256 mA due to Chilung's request */
	.usb_input_current  = 256 /*384*/,
/* END [2012/05/02] Jackal - Modify input current from 384 mA to 256 mA due to Chilung's request */
	.usbcgr_current     = 2048,
	.usbcgr_input_current = 1920,
	.dock_chg_current   = 2048,
	.dock_input_current = 1920,
	.term_current       = 120,
	.valid_n_gpio       = PM8921_MPP_PM_TO_SYS(12),
	.chg_detection_config = NULL,
	.max_system_voltage   = 4208,
	.min_system_voltage   = 3072,
};

static struct i2c_board_info isl_charger_i2c_info[] __initdata = {
	{
		I2C_BOARD_INFO("isl9519q", 0x9),
		.irq = PM8921_CBLPWR_IRQ(PM8921_IRQ_BASE),
		.platform_data = &isl_data,
	},
};
/* END-20111214-KenLee-add for westlake charging. */
#else

static struct isl_platform_data isl_data __initdata = {
	.valid_n_gpio		= 0,	/* Not required when notify-by-pmic */
	.chg_detection_config	= NULL,	/* Not required when notify-by-pmic */
	.max_system_voltage	= 4200,
	.min_system_voltage	= 3200,
	.chgcurrent		= 1000, /* 1900, */
	.term_current		= 400,	/* Need fine tuning */
	.input_current		= 2048,
};

static struct i2c_board_info isl_charger_i2c_info[] __initdata = {
	{
		I2C_BOARD_INFO("isl9519q", 0x9),
		.irq		= 0,	/* Not required when notify-by-pmic */
		.platform_data	= &isl_data,
	},
};
#endif
#endif /* CONFIG_ISL9519_CHARGER */

// BEGIN Tim PH, 20111214, add gauge BQ27541 driver.
#if defined(CONFIG_BATTERY_BQ27541) || \
		defined(CONFIG_BATTERY_BQ27541_MODULE)
static struct i2c_board_info msm_bq27541_board_info[] = {
	{
		I2C_BOARD_INFO("bq27541", 0xaa>>1),
	},
};
#endif
// END Tim PH, 20111214, add gauge BQ27541C1 driver.

#if defined(CONFIG_GPIO_SX150X) || defined(CONFIG_GPIO_SX150X_MODULE)
static struct i2c_board_info liquid_io_expander_i2c_info[] __initdata = {
	{
		I2C_BOARD_INFO("sx1508q", 0x20),
		.platform_data = &msm8960_sx150x_data[SX150X_LIQUID]
	},
};
#endif

#ifdef CONFIG_EC/*1109, for EC, weihuai*/
#define EC_I2C_ADD 0x19
#define EC_GPIO_IRQ 11

static int pega_EC_gpio_set(int EC_gpio_pin, char* EC_gpio_label)
{
	int ret;
//	printk(KERN_INFO"EC_gpio_pin: %x\n", EC_gpio_pin);

	ret = gpio_request(EC_gpio_pin, EC_gpio_label);
	if(ret < 0)
	{
		 printk(KERN_INFO"nuvec: Failed to gpio_request\n");
		 return ret;
	}

	ret = gpio_direction_input(EC_gpio_pin);

	if (ret) {
		printk(KERN_INFO"nuvec:gpio_direction_iutput set wrong:%d\n",ret);
		return ret;
	}
	return ret;
}

static struct EC_platform_data EC_info = {
 	.gpio_setup = pega_EC_gpio_set,
//	.docking_interrupt = docking_interrupt,
	.EC_irq =EC_GPIO_IRQ,
	.docking_det_irq = DOCK_IN_DET_PIN_7,
};
	
static struct i2c_board_info EC_board_info[] = {
	{
		I2C_BOARD_INFO("nuvec", EC_I2C_ADD),
		.platform_data = &EC_info,
	}
};

/*----------------------------------------------------------------------------
* docking_interrupt
*----------------------------------------------------------------------------*/
int docking_interrupt(void)
{
	int dock_in;
	int dock_in_check;
	int dock_ver;
	int retry=0;
	dock_in_check = DKIN_DET(EC_info.docking_det_irq );
       msleep(2);
       dock_in = DKIN_DET(EC_info.docking_det_irq );
       ECLOG_INFO("%s:%d(%d)\n",__func__,dock_in_check, dock_in);


	/* BEGIN [Terry_Tzeng][2012/05/22][Notify dock in event in any statement]*/
	Notify_charging_dock_in_out();
	/* END [Terry_Tzeng][2012/05/22][Notify dock in event in any statement]*/

	if(!dock_in)
	{
		ec_dock_out();
		goto EC_REMOVE;
	} else {
		ECLOG_INFO("INSRT\n");
	}
	msleep(ec_init_delay);
	
RETRY:
	if(DKIN_DET(EC_info.docking_det_irq ) && ++retry <=MAX_ERR_TIME)
	{
		if( ec_dock_ver(&dock_ver) == -1 )
		{
			ECLOG_ERR("%s, ec_dock_ver EI2CXF :retry-%d\n",__func__,retry);
			msleep(250);
			goto RETRY;
		}else{
			ECLOG_INFO("dock HW version : %d\n",dock_ver);
			usb_hub_reset(0x64); //reset hub when dock in
			ec_dock_in();
			goto EC_OK;
		}
	} else {
		goto EC_I2C_FAIL;
	}
	
EC_I2C_FAIL:
	ECLOG_ERR("%s,EI2CXF\n",__func__);
	dock_i2c_fail();
	return 0;
EC_REMOVE:
	ECLOG_INFO("REMOVE\n");
	return 0;
EC_OK:
	ECLOG_DBG("%s OK\n",__func__);
	return 1;
}
#endif

#ifdef CONFIG_TOUCHSCREEN_LGD_I2C	/*[BEGIN] CONFIG_TOUCHSCREEN_LGD_I2C */
#define LGD_I2C_ADD (0x30>>1)
#define GP47_DB_5V_EN 47
#define LGD_TOUCH_CHG_GPIO 49
#define LGD_WAKEUP_GPIO	58
#define LGD_Touch_ID	3		//PVT Run
#define LGD_RESET_GPIO	46


static bool lgd_touch_init_state = false;

static int pega_lgd_touch_power_5v_en(int on_off)
{
	int rc;
	printk("%s:%d\n", __func__,on_off);

	if (lgd_touch_init_state == false) 
	{
		/*request touch GPIO*/
		rc = gpio_request(GP47_DB_5V_EN, "db_5v_en");
		if (rc) {
			pr_err("%s: gpio_request failed for GP47_DB_5V_EN\n",
				__func__);
			goto fail;
		}

		lgd_touch_init_state = true;
	}

	/*turn on or turn off 5V power*/
	if (on_off)
	{
		rc = gpio_direction_output(GP47_DB_5V_EN, 1);
		if (rc) {
			pr_err("%s gpio_direction_output %d failed rc = %d\n",
				__func__, GP47_DB_5V_EN, rc);
			goto fail;
		}
	}
	else
	{
		rc = gpio_direction_output(GP47_DB_5V_EN, 0);
		if (rc) {
			pr_err("%s gpio_direction_output %d failed rc = %d\n",
				__func__, GP47_DB_5V_EN, rc);
			goto fail;
		}
	}
	return rc;

fail:
	gpio_free(GP47_DB_5V_EN);
	return rc;
}


int pega_lgd_touch_gpio_set(void)
{
	int ret;

	//printk("%s: request gpio...\n", __func__);
	/*Request GPIO of CHG*/
	ret = gpio_request(LGD_TOUCH_CHG_GPIO,"lgd_int");
	if(ret < 0)
	{
		 printk(KERN_INFO"[Touch]: Failed to gpio_request LGD_TOUCH_CHG_GPIO\n");
		 return ret;
	}

	/*Config GPIO of CHG as input*/
	ret = gpio_direction_input(LGD_TOUCH_CHG_GPIO);

	if (ret) {
		printk(KERN_INFO"[Touch]:LGD_TOUCH_CHG_GPIO gpio_direction_iutput set wrong:%d\n",ret);
		return ret;
	}

	/*Request GPIO of RESET*/
	ret = gpio_request(LGD_RESET_GPIO,"lgd_rst");
	if(ret < 0)
	{
		 printk(KERN_INFO"[Touch]: Failed to gpio_request LGD_RESET_GPIO\n");
		 return ret;
	}

	/*Config GPIO of RESET as input*/
	ret = gpio_direction_input(LGD_RESET_GPIO);

	if (ret) {
		printk(KERN_INFO"[Touch]:LGD_RESET_GPIO gpio_direction_iutput set wrong:%d\n",ret);
		return ret;
	}
	/*Request GPIO of Touch ID Pin*/
	ret = gpio_request(LGD_Touch_ID,"lgd_ID");
	if(ret < 0)
	{
		 printk(KERN_INFO"[Touch]: Failed to gpio_request LGD_Touch_ID\n");
		 return ret;
	}
	/*Config GPIO of CHG as input*/
	ret = gpio_direction_input(LGD_Touch_ID);

	if (ret) {
		printk(KERN_INFO"[Touch]: LGD_Touch_ID gpio_direction_iutput set wrong:%d\n",ret);
		return ret;
	}

	return 0;
}

static struct regulator *reg_l17;
static struct regulator *reg_l11;
static struct regulator *reg_lvs4;
static bool reg_init = false;
static bool lgd_ts_poweron  = false;

int pega_lgd_touch_power(int on)
{
	int rc =0;

//	printk(KERN_INFO"%s: set power...\n", __func__);

	if(reg_init == false)
	{
//		printk(KERN_INFO"%s: get regulator...\n", __func__);

		/*Get regulator 8921_l17*/
		reg_l17 = regulator_get(NULL, "8921_l17");
		if (IS_ERR(reg_l17)) {
			pr_err("%s: could not get 8921_l17, rc = %ld\n",__func__,PTR_ERR(reg_l17));
			return -ENODEV;
		}
		/*Set Voltage of L17: 3.3v*/
		rc = regulator_set_voltage(reg_l17, 3300000, 3300000);
		if (rc) {
			pr_err("%s: set_voltage l17 failed, rc=%d\n", __func__,rc);
			return -EINVAL;
		}
		if (get_pega_hw_board_version_status() == 0) {//EVT
			/*Get regulator lvs4*/
//			printk(KERN_INFO"%s: regulator_get lvs4...\n", __func__);
			reg_lvs4 = regulator_get(NULL, "8921_lvs4");

			if (IS_ERR(reg_lvs4)) {
				pr_err("%s: could not get 8921_lvs4, rc = %ld\n",__func__,PTR_ERR(reg_lvs4));
				return -ENODEV;
			}
		}
		/*Get regulator reg_l11*/
		reg_l11 = regulator_get(NULL, "8921_l11");

		if (IS_ERR(reg_l11)) {
			pr_err("%s: could not get 8921_l11, rc = %ld\n",__func__,PTR_ERR(reg_l11));
			return -ENODEV;
		}
		/*Set Voltage of L11: 3.3v*/
		rc = regulator_set_voltage(reg_l11, 3300000, 3300000);
		if (rc) {
			pr_err("%s: set_voltage l11 failed, rc=%d\n", __func__,rc);
			return -EINVAL;
		}

		reg_init = true;
	}

  	if (on) {

//		printk("%s: regulator on\n", __func__);

		if(lgd_ts_poweron == true)
		{
			printk("%s: regulator already on, return...", __func__);
			return 0;
		}
		/*[BEGIN]20120119 Jack: Change power on sequence enable lvs4 ->  L/S L11 regulator -> touch power -> L/S enable*/
		if (get_pega_hw_board_version_status() == 0) {	//EVT
			//Enable Regulator reg_lvs4
//			printk(KERN_INFO"%s: regulator_enable lvs4...\n", __func__);
			rc = regulator_enable(reg_lvs4);
			if (rc) {
				pr_err("%s: enable lvs4 failed, rc=%d\n", __func__,rc);
				return -ENODEV;
			}
		}
		//enable pm8921 L11
		rc = regulator_enable(reg_l11);
		if (rc) {
			pr_err("%s: enable l11 failed, rc=%d\n", __func__,rc);
			return -ENODEV;
		}

		/*enable pm8921 L17*/
		 rc = regulator_set_optimum_mode(reg_l17, 100000);
			if (rc < 0) {
			pr_err("set_optimum_mode l17 failed, rc=%d\n", rc);
			return -EINVAL;
			}

		rc = regulator_enable(reg_l17);
		if (rc) {
			pr_err("%s: enable l17 failed, rc=%d\n", __func__,rc);
			return -ENODEV;
		}
		if (get_pega_hw_board_version_status() == 0) {	//EVT
			//enable enable GP47_sw_en
			rc =pega_lgd_touch_power_5v_en(1);
			if(rc < 0){
					printk( KERN_INFO "%s:lgd_ts_probe 5v_en power on failed\n", __func__);
					return rc;
			}
		}
		/*[END]20120119 Jack: Change power on sequence enable lvs4 regulator ->  L/S L11 regulator -> touch power -> L/S enable*/
		lgd_ts_poweron = true;

  	}
	else
	{

//		printk("%s: regulator off\n", __func__);

		if(lgd_ts_poweron == false)
		{
			printk("%s: regulator already off, return...\n", __func__);
			return 0;
		}
		/*[BEGIN]20120119 Jack: Change power sequence disable L/S enable -> touch power -> L11 -> LVS4*/
		if (get_pega_hw_board_version_status() == 0) {	//EVT
			/*Disable enable GP47_sw_en*/
			rc =pega_lgd_touch_power_5v_en(0);
			if(rc < 0){
					printk( KERN_INFO "%s:lgd_ts_probe 5v_en power on failed\n", __func__);
					return rc;
			}
		}
		/*Disable pm8921 L17*/
		rc = regulator_disable(reg_l17);
		if (rc) {
			pr_err("%s: disable l17 failed, rc=%d\n", __func__,rc);
			return -ENODEV;
		}

		/*Disable pm8921 L11*/
		rc = regulator_disable(reg_l11);
		if (rc) {
			pr_err("disable reg_l11 failed, rc=%d\n", rc);
			return -ENODEV;
		}
		if (get_pega_hw_board_version_status() == 0) {
			/*Disable Regulator reg_lvs4*/
//			printk(KERN_INFO"%s: regulator_disable lvs4...\n", __func__);
			rc = regulator_disable(reg_lvs4);
			if (rc) {
				pr_err("%s: disable lvs4 failed, rc=%d\n", __func__,rc);
				return -ENODEV;
			}
		}
		/*[END]20120119 Jack: Change power sequence disable L/S enable -> touch power -> I2C pull high voltage*/
		lgd_ts_poweron = false;

	}

	
	return 0;
}


int touch_wakeup_gpio_init(void)
{
	int ret;

//	printk("%s: request gpio...\n", __func__);
	/*Request GPIO of WAKE-UP*/
	ret = gpio_request(LGD_WAKEUP_GPIO,"wakeup gpio");
	if(ret != 0)
	{
		 printk(KERN_INFO"[Touch]: Failed to gpio_request LGD_WAKEUP_GPIO\n");
		 return ret;
	}
	else {
	  // mhshin_20120221_debug_start
	  gpio_direction_output(LGD_WAKEUP_GPIO, 0);   // Default output is Low.
	  udelay(10);
	  // mhshin_20120221_debug_end
		//printk("%s : GPIO pull-up/down disable for wake-up gpio !\n", __func__);
		return 0;
	}
}

static struct  touch_pdata lgd_info = {
	.name		= "lgd-ts",
	.irq_gpio		= MSM_GPIO_TO_INT(LGD_TOUCH_CHG_GPIO),
	.wake_gpio	= LGD_WAKEUP_GPIO, 
	.WakeGPIOInit = false,
	.irq_flags		= (IRQF_TRIGGER_RISING),
	.abs_max_x	= 1280,
	.abs_max_y 	= 800,
	.abs_min_x	= 0,
	.abs_min_y	= 0,
	.max_fingers	= 10,
	.gpio_init		= touch_wakeup_gpio_init,	
	.gpio_setup = pega_lgd_touch_gpio_set,
	.lgd_power_save = pega_lgd_touch_power,
	
};

static struct i2c_board_info lgd_board_info[] = {  // mhshin_2012_0208
	{
		I2C_BOARD_INFO("lgd-ts", LGD_I2C_ADD),
		.platform_data = &lgd_info,
		.irq = MSM_GPIO_TO_INT(LGD_TOUCH_CHG_GPIO),
	}
};


#endif /* CONFIG_TOUCHSCREEN_LGD_I2C */

/* BEGIN Dom_Lin@pegatron [2011/02/02] [Init camera LED drivers in 1024 codebase] */
#if defined(CONFIG_LEDS_LM3559)
static struct lm3559_leds_platform_data lm3559_leds_data = {
	.hw_enable = 2,
/* BEGIN Dom_Lin@pegatron [2011/02/02] [Init camera strobe flash function in 1024 codebase] */
#ifdef LM3559_DEBUG_NODE
	.mode = LM3559_MODE_AUTO,
#endif
/* END Dom_Lin@pegatron [2011/02/02] [Init camera strobe flash function in 1024 codebase] */
};

static struct i2c_board_info lm3559_leds_i2c_info[] __initdata = {
	{
		I2C_BOARD_INFO(LM3559_LEDS_DEV_NAME, 0x53),
		.platform_data	= &lm3559_leds_data,
	},
};
#endif
/* END Dom_Lin@pegatron [2011/02/02] [Init camera LED drivers in 1024 codebase] */

static struct i2c_registry msm8960_i2c_devices[] __initdata = {
/* BEGIN-20111214-KenLee-add for westlake charging. */

#ifdef CONFIG_ISL9519_CHARGER
	{
		I2C_SURF | I2C_FFA | I2C_FLUID | I2C_LIQUID,
		MSM_8960_GSBI10_QUP_I2C_BUS_ID,
		isl_charger_i2c_info,
		ARRAY_SIZE(isl_charger_i2c_info),
	},
#endif /* CONFIG_ISL9519_CHARGER */
/* END-20111214-KenLee-add for westlake charging. */
/* BEGIN Tim PH, 20111214, add gauge BQ27541 driver. */
#ifdef CONFIG_BATTERY_BQ27541
	{
		I2C_SURF | I2C_FFA | I2C_FLUID | I2C_LIQUID,
		MSM_8960_GSBI10_QUP_I2C_BUS_ID,
		msm_bq27541_board_info,
		ARRAY_SIZE(msm_bq27541_board_info),
	},
#endif /* CONFIG_BATTERY_BQ27541C1 */
/* END Tim_PH, 20111214, add gauge BQ27541 driver. */
	{
		I2C_FFA | I2C_LIQUID,
		MSM_8960_GSBI10_QUP_I2C_BUS_ID,
		sii_device_info,
		ARRAY_SIZE(sii_device_info),
	},
	{
		I2C_LIQUID,
		MSM_8960_GSBI10_QUP_I2C_BUS_ID,
		msm_isa1200_board_info,
		ARRAY_SIZE(msm_isa1200_board_info),
	},
// [2011/12/22] Jackal - porting for westlake
#ifdef CONFIG_MPU_SENSORS_MPU3050
      {
    I2C_SURF | I2C_FFA | I2C_FLUID,
		MSM_8960_GSBI12_QUP_I2C_BUS_ID,
		sanpaolo_mpu3050_i2c_boardinfo,
		ARRAY_SIZE(sanpaolo_mpu3050_i2c_boardinfo),
      },
      {
    I2C_SURF | I2C_FFA | I2C_FLUID,
		MSM_8960_GSBI12_QUP_I2C_BUS_ID,
		sanpaolo_isl29023_i2c_boardinfo,
		ARRAY_SIZE(sanpaolo_isl29023_i2c_boardinfo),
      },
#endif
// [2011/12/22] Jackal - porting for westlake End

#if defined(CONFIG_GPIO_SX150X) || defined(CONFIG_GPIO_SX150X_MODULE)
	{
		I2C_LIQUID,
		MSM_8960_GSBI10_QUP_I2C_BUS_ID,
		liquid_io_expander_i2c_info,
		ARRAY_SIZE(liquid_io_expander_i2c_info),
	},
#endif
#ifdef CONFIG_TOUCHSCREEN_LGD_I2C
	{
		I2C_SURF | I2C_FFA | I2C_FLUID,
		MSM_8960_GSBI3_QUP_I2C_BUS_ID,
		lgd_board_info,
		ARRAY_SIZE(lgd_board_info),
	},
#endif	/*CONFIG_TOUCHSCREEN_LGD_I2C*/
};
#endif /* CONFIG_I2C */

static void __init register_i2c_devices(void)
{
#ifdef CONFIG_I2C
	u8 mach_mask = 0;
	int i;
#ifdef CONFIG_MSM_CAMERA
	struct i2c_registry msm8960_camera_i2c_devices = {
		I2C_SURF | I2C_FFA | I2C_FLUID | I2C_LIQUID | I2C_RUMI,
		MSM_8960_GSBI4_QUP_I2C_BUS_ID,
		msm8960_camera_board_info.board_info,
		msm8960_camera_board_info.num_i2c_board_info,
	};
/* BEGIN Dom_Lin@pegatron [2012/03/09] [Add camera new schematic support after DVT version in 1032 codebase] */
	struct i2c_registry msm8960_camera_i2c_devices_v2_back = {
		I2C_SURF | I2C_FFA | I2C_FLUID | I2C_LIQUID | I2C_RUMI,
		MSM_8960_GSBI4_QUP_I2C_BUS_ID,
		msm8960_camera_board_info_v2_back.board_info,
		msm8960_camera_board_info_v2_back.num_i2c_board_info,
	};
/* BEGIN Chaoyen_Wu@pegatron [2012.3.13][Add front camera new schematic support after DVT version in 1032 codebase]*/
	struct i2c_registry msm8960_camera_i2c_devices_v2_front = {
		I2C_SURF | I2C_FFA | I2C_FLUID | I2C_LIQUID | I2C_RUMI,
		MSM_8960_GSBI1_QUP_I2C_BUS_ID,
		msm8960_camera_board_info_v2_front.board_info,
		msm8960_camera_board_info_v2_front.num_i2c_board_info,
	};
/* END Chaoyen_Wu@pegatron [2012.3.13][Add front camera new schematic support after DVT version in 1032 codebase]*/
/* END Dom_Lin@pegatron [2012/03/09] [Add camera new schematic support after DVT version in 1032 codebase] */
#endif
#if 1	/*0402---start*/
#ifdef CONFIG_EC
/*WeiHuai--dock i2c base on gsbi10 for DVT*/
	struct i2c_registry msm8960_ec_i2c_devices_v1 = {
		I2C_SURF | I2C_FFA | I2C_FLUID,
		MSM_8960_GSBI10_QUP_I2C_BUS_ID,
		EC_board_info,
		ARRAY_SIZE(EC_board_info),
	};
/*WeiHuai--dock i2c base on gsbi9 for SIT2*/
	struct i2c_registry msm8960_ec_i2c_devices_v2 = {
		I2C_SURF | I2C_FFA | I2C_FLUID,
		MSM_8960_GSBI9_QUP_I2C_BUS_ID,
		EC_board_info,
		ARRAY_SIZE(EC_board_info),
	};
#endif
#endif	/*0402--end*/
/* BEGIN Dom_Lin@pegatron [2012/03/19] [Add camera LED new schematic support after DVT version in 1032 codebase] */
#if defined(CONFIG_LEDS_LM3559)
	static struct i2c_registry lm3559_leds_devices = {
		I2C_SURF | I2C_FFA | I2C_FLUID,
		MSM_8960_GSBI4_QUP_I2C_BUS_ID,
		lm3559_leds_i2c_info,
		ARRAY_SIZE(lm3559_leds_i2c_info),
	};
	static struct i2c_registry lm3559_leds_devices_v2 = {
		I2C_SURF | I2C_FFA | I2C_FLUID,
		MSM_8960_GSBI5_QUP_I2C_BUS_ID,
		lm3559_leds_i2c_info,
		ARRAY_SIZE(lm3559_leds_i2c_info),
	};
#endif
/* END Dom_Lin@pegatron [2012/03/19] [Add camera LED new schematic support after DVT version in 1032 codebase] */

	/* Build the matching 'supported_machs' bitmask */
	if (machine_is_msm8960_cdp())
		mach_mask = I2C_SURF;
	else if (machine_is_msm8960_rumi3())
		mach_mask = I2C_RUMI;
	else if (machine_is_msm8960_sim())
		mach_mask = I2C_SIM;
	else if (machine_is_msm8960_fluid())
		mach_mask = I2C_FLUID;
	else if (machine_is_msm8960_liquid())
		mach_mask = I2C_LIQUID;
	else if (machine_is_msm8960_mtp())
		mach_mask = I2C_FFA;
	else
		pr_err("unmatched machine ID in register_i2c_devices\n");

	/* Run the array and install devices as appropriate */
	for (i = 0; i < ARRAY_SIZE(msm8960_i2c_devices); ++i) {
		if (msm8960_i2c_devices[i].machs & mach_mask)
			i2c_register_board_info(msm8960_i2c_devices[i].bus,
						msm8960_i2c_devices[i].info,
						msm8960_i2c_devices[i].len);
	}
#ifdef CONFIG_MSM_CAMERA
/* BEGIN Dom_Lin@pegatron [2012/03/09] [Add camera new schematic support after DVT version in 1032 codebase] */
	if (get_pega_hw_board_version_status() >= 2) { // Not DVT
		if (msm8960_camera_i2c_devices_v2_back.machs & mach_mask)
			i2c_register_board_info(msm8960_camera_i2c_devices_v2_back.bus,
				msm8960_camera_i2c_devices_v2_back.info,
				msm8960_camera_i2c_devices_v2_back.len);
/* BEGIN Chaoyen_Wu@pegatron [2012.3.13][Add front camera new schematic support after DVT version in 1032 codebase]*/
		if (msm8960_camera_i2c_devices_v2_front.machs & mach_mask)
			i2c_register_board_info(msm8960_camera_i2c_devices_v2_front.bus,
				msm8960_camera_i2c_devices_v2_front.info,
				msm8960_camera_i2c_devices_v2_front.len);		
/* END Chaoyen_Wu@pegatron [2012.3.13][Add front camera new schematic support after DVT version in 1032 codebase]*/
	}
	else {
		printk(KERN_DEBUG "[%s: %d] camera use DVT schematic\n", __func__, __LINE__);
		if (msm8960_camera_i2c_devices.machs & mach_mask)
			i2c_register_board_info(msm8960_camera_i2c_devices.bus,
				msm8960_camera_i2c_devices.info,
				msm8960_camera_i2c_devices.len);
	}
/* END Dom_Lin@pegatron [2012/03/09] [Add camera new schematic support after DVT version in 1032 codebase] */
#endif

#ifdef CONFIG_EC	/*WeiHuai start--0402*/
	if(get_pega_hw_board_version_status() >=3) {	//HW after SIT2
		EC_info.docking_det_irq = DOCK_IN_DET_PIN_35;
		if(msm8960_ec_i2c_devices_v2.machs & mach_mask)
			i2c_register_board_info(msm8960_ec_i2c_devices_v2.bus,
			msm8960_ec_i2c_devices_v2.info,
			msm8960_ec_i2c_devices_v2.len);
	}else {
		EC_info.docking_det_irq = DOCK_IN_DET_PIN_7;
		if(msm8960_ec_i2c_devices_v1.machs & mach_mask)
			i2c_register_board_info(msm8960_ec_i2c_devices_v1.bus,
			msm8960_ec_i2c_devices_v1.info,
			msm8960_ec_i2c_devices_v1.len);
	}
#if defined(PEGA_SMT_BUILD)
	ec_smt_interface_bus_register((EC_info.docking_det_irq==DOCK_IN_DET_PIN_35)?EC_BUS_PVT:EC_BUS_DVT);
#endif
#endif	/*WeiHuai end--0402*/
/* BEGIN Dom_Lin@pegatron [2012/03/19] [Add camera LED new schematic support after DVT version in 1032 codebase] */
#if defined(CONFIG_LEDS_LM3559)
	if (get_pega_hw_board_version_status() >= 2) { // Not DVT
		if (lm3559_leds_devices_v2.machs & mach_mask)
			i2c_register_board_info(lm3559_leds_devices_v2.bus,
				lm3559_leds_devices_v2.info,
				lm3559_leds_devices_v2.len);
	}
	else {
		if (lm3559_leds_devices.machs & mach_mask)
			i2c_register_board_info(lm3559_leds_devices.bus,
				lm3559_leds_devices.info,
				lm3559_leds_devices.len);
	}
#endif
/* END Dom_Lin@pegatron [2012/03/19] [Add camera LED new schematic support after DVT version in 1032 codebase] */

#endif
}

static void __init msm8960_sim_init(void)
{
	struct msm_watchdog_pdata *wdog_pdata = (struct msm_watchdog_pdata *)
		&msm8960_device_watchdog.dev.platform_data;

	wdog_pdata->bark_time = 15000;
	msm_tsens_early_init(&msm_tsens_pdata);
	BUG_ON(msm_rpm_init(&msm_rpm_data));
	BUG_ON(msm_rpmrs_levels_init(msm_rpmrs_levels,
				ARRAY_SIZE(msm_rpmrs_levels)));
	regulator_suppress_info_printing();
	platform_device_register(&msm8960_device_rpm_regulator);
	msm_clock_init(&msm8960_clock_init_data);
	msm8960_init_pmic();

	msm8960_device_otg.dev.platform_data = &msm_otg_pdata;
	msm8960_init_gpiomux();
	msm8960_i2c_init();
	msm_spm_init(msm_spm_data, ARRAY_SIZE(msm_spm_data));
	msm_spm_l2_init(msm_spm_l2_data);
	msm8960_init_buses();
	platform_add_devices(common_devices, ARRAY_SIZE(common_devices));
	msm8960_pm8921_gpio_mpp_init();
	platform_add_devices(sim_devices, ARRAY_SIZE(sim_devices));
	acpuclk_init(&acpuclk_8960_soc_data);

	msm8960_device_qup_spi_gsbi1.dev.platform_data =
				&msm8960_qup_spi_gsbi1_pdata;
	spi_register_board_info(spi_board_info, ARRAY_SIZE(spi_board_info));

	msm8960_init_mmc();
	msm8960_init_fb();
	slim_register_board_info(msm_slim_devices,
		ARRAY_SIZE(msm_slim_devices));
	msm_pm_set_platform_data(msm_pm_data, ARRAY_SIZE(msm_pm_data));
	msm_pm_set_rpm_wakeup_irq(RPM_APCC_CPU0_WAKE_UP_IRQ);
	msm_cpuidle_set_states(msm_cstates, ARRAY_SIZE(msm_cstates),
				msm_pm_data);
	BUG_ON(msm_pm_boot_init(&msm_pm_boot_pdata));
	msm_pm_init_sleep_status_data(&msm_pm_slp_sts_data);
}

static void __init msm8960_rumi3_init(void)
{
	msm_tsens_early_init(&msm_tsens_pdata);
	BUG_ON(msm_rpm_init(&msm_rpm_data));
	BUG_ON(msm_rpmrs_levels_init(msm_rpmrs_levels,
				ARRAY_SIZE(msm_rpmrs_levels)));
	regulator_suppress_info_printing();
	platform_device_register(&msm8960_device_rpm_regulator);
	msm_clock_init(&msm8960_dummy_clock_init_data);
	msm8960_init_gpiomux();
	msm8960_init_pmic();
	msm8960_device_qup_spi_gsbi1.dev.platform_data =
				&msm8960_qup_spi_gsbi1_pdata;
	spi_register_board_info(spi_board_info, ARRAY_SIZE(spi_board_info));
	msm8960_i2c_init();
	msm_spm_init(msm_spm_data, ARRAY_SIZE(msm_spm_data));
	msm_spm_l2_init(msm_spm_l2_data);
	platform_add_devices(common_devices, ARRAY_SIZE(common_devices));
	msm8960_pm8921_gpio_mpp_init();
	platform_add_devices(rumi3_devices, ARRAY_SIZE(rumi3_devices));
	msm8960_init_mmc();
	register_i2c_devices();
	msm8960_init_fb();
	slim_register_board_info(msm_slim_devices,
		ARRAY_SIZE(msm_slim_devices));
	msm_pm_set_platform_data(msm_pm_data, ARRAY_SIZE(msm_pm_data));
	msm_pm_set_rpm_wakeup_irq(RPM_APCC_CPU0_WAKE_UP_IRQ);
	msm_cpuidle_set_states(msm_cstates, ARRAY_SIZE(msm_cstates),
				msm_pm_data);
	BUG_ON(msm_pm_boot_init(&msm_pm_boot_pdata));
	msm_pm_init_sleep_status_data(&msm_pm_slp_sts_data);
}

static void __init msm8960_cdp_init(void)
{
	if (meminfo_init(SYS_MEMORY, SZ_256M) < 0)
		pr_err("meminfo_init() failed!\n");

	msm_tsens_early_init(&msm_tsens_pdata);
	BUG_ON(msm_rpm_init(&msm_rpm_data));
	BUG_ON(msm_rpmrs_levels_init(msm_rpmrs_levels,
				ARRAY_SIZE(msm_rpmrs_levels)));

	regulator_suppress_info_printing();
	if (msm_xo_init())
		pr_err("Failed to initialize XO votes\n");
	platform_device_register(&msm8960_device_rpm_regulator);
	msm_clock_init(&msm8960_clock_init_data);
	if (machine_is_msm8960_liquid())
		msm_otg_pdata.mhl_enable = true;
	msm8960_device_otg.dev.platform_data = &msm_otg_pdata;
	if (machine_is_msm8960_mtp() || machine_is_msm8960_fluid() ||
		machine_is_msm8960_cdp()) {
		//chilung 20120629, default value move to board_serialno_setup
		//msm_otg_pdata.phy_init_seq = wr_phy_init_seq;
	} else if (machine_is_msm8960_liquid()) {
			msm_otg_pdata.phy_init_seq =
				liquid_v1_phy_init_seq;
	}
	msm_otg_pdata.swfi_latency =
		msm_rpmrs_levels[0].latency_us;
#ifdef CONFIG_USB_EHCI_MSM_HSIC
	if (machine_is_msm8960_liquid()) {
		if (SOCINFO_VERSION_MAJOR(socinfo_get_version()) >= 2)
			msm_hsic_pdata.hub_reset = HSIC_HUB_RESET_GPIO;
	}
	msm_device_hsic_host.dev.platform_data = &msm_hsic_pdata;
#endif
	msm8960_init_gpiomux();
	msm8960_init_pmic();
	if ((SOCINFO_VERSION_MAJOR(socinfo_get_version()) >= 2 &&
		(machine_is_msm8960_mtp())) || machine_is_msm8960_liquid())
		msm_isa1200_board_info[0].platform_data = &isa1200_1_pdata;
	msm8960_i2c_init();
	msm8960_gfx_init();
	msm_spm_init(msm_spm_data, ARRAY_SIZE(msm_spm_data));
	msm_spm_l2_init(msm_spm_l2_data);
	msm8960_init_buses();
	platform_add_devices(msm_footswitch_devices,
		msm_num_footswitch_devices);
	if (machine_is_msm8960_liquid())
		platform_device_register(&msm8960_device_ext_3p3v_vreg);

/* BEGIN-20111214-KenLee-add for westlake charging. */
#ifdef CONFIG_PEGA_ISL_CHG
	platform_device_register(&msm_charger_device);
#endif
/* END-20111214-KenLee-add for westlake charging. */

	if (machine_is_msm8960_cdp())
		platform_device_register(&msm8960_device_ext_l2_vreg);
	platform_add_devices(common_devices, ARRAY_SIZE(common_devices));
	msm8960_pm8921_gpio_mpp_init();
	platform_add_devices(cdp_devices, ARRAY_SIZE(cdp_devices));
	msm8960_init_hsic();
	msm8960_init_cam();
	msm8960_init_mmc();
	acpuclk_init(&acpuclk_8960_soc_data);
	register_i2c_devices();
	msm8960_init_fb();
	slim_register_board_info(msm_slim_devices,
		ARRAY_SIZE(msm_slim_devices));
	msm8960_init_dsps();
	msm_pm_set_platform_data(msm_pm_data, ARRAY_SIZE(msm_pm_data));
	msm_pm_set_rpm_wakeup_irq(RPM_APCC_CPU0_WAKE_UP_IRQ);
	msm_cpuidle_set_states(msm_cstates, ARRAY_SIZE(msm_cstates),
				msm_pm_data);
	change_memory_power = &msm8960_change_memory_power;
	BUG_ON(msm_pm_boot_init(&msm_pm_boot_pdata));
	msm_pm_init_sleep_status_data(&msm_pm_slp_sts_data);
	if (PLATFORM_IS_CHARM25())
		platform_add_devices(mdm_devices, ARRAY_SIZE(mdm_devices));
/*robert1_chen, 20120409, for Dock GSBI change -- BEGIN*/
    #if !defined(CONFIG_EC) 
	msm8960_device_qup_spi_gsbi1.dev.platform_data =
				&msm8960_qup_spi_gsbi1_pdata;
	spi_register_board_info(spi_board_info, ARRAY_SIZE(spi_board_info));
    #else
	msm8960_init_dock_switch_detect(EC_info.docking_det_irq );
	//ENABLE GPIO11 mpm interrupt
	__raw_writel((__raw_readl(MSM_TLMM_BASE+0x840)|0x40),(MSM_TLMM_BASE+0x840));
    #endif
/*robert1_chen, 20120409, for Dock GSBI change -- END*/
/* BEGIN-20111221-paul add h2w for ATS test. */
#if defined (CONFIG_WESTLAKE) && defined (CONFIG_SWITCH_GPIO)
	msm8960_init_headset_switch_detect();
#endif
/* END-20111221-paul add h2w for ATS test. */
/*Evan: SAR porting start*/	
   //pega_Cap_Sensor_gpio_set();
   Cap_sensor_iqs52_init();
/*Evan: SAR proting end*/
}

MACHINE_START(MSM8960_SIM, "QCT MSM8960 SIMULATOR")
	.map_io = msm8960_map_io,
	.reserve = msm8960_reserve,
	.init_irq = msm8960_init_irq,
	.handle_irq = gic_handle_irq,
	.timer = &msm_timer,
	.init_machine = msm8960_sim_init,
	.init_early = msm8960_allocate_memory_regions,
	.init_very_early = msm8960_early_memory,
MACHINE_END

MACHINE_START(MSM8960_RUMI3, "QCT MSM8960 RUMI3")
	.map_io = msm8960_map_io,
	.reserve = msm8960_reserve,
	.init_irq = msm8960_init_irq,
	.handle_irq = gic_handle_irq,
	.timer = &msm_timer,
	.init_machine = msm8960_rumi3_init,
	.init_early = msm8960_allocate_memory_regions,
	.init_very_early = msm8960_early_memory,
MACHINE_END

MACHINE_START(MSM8960_CDP, "QCT MSM8960 CDP")
	.map_io = msm8960_map_io,
	.reserve = msm8960_reserve,
	.init_irq = msm8960_init_irq,
	.handle_irq = gic_handle_irq,
	.timer = &msm_timer,
	.init_machine = msm8960_cdp_init,
	.init_early = msm8960_allocate_memory_regions,
	.init_very_early = msm8960_early_memory,
MACHINE_END

MACHINE_START(MSM8960_MTP, "QCT MSM8960 MTP")
	.map_io = msm8960_map_io,
	.reserve = msm8960_reserve,
	.init_irq = msm8960_init_irq,
	.handle_irq = gic_handle_irq,
	.timer = &msm_timer,
	.init_machine = msm8960_cdp_init,
	.init_early = msm8960_allocate_memory_regions,
	.init_very_early = msm8960_early_memory,
MACHINE_END

MACHINE_START(MSM8960_FLUID, "QCT MSM8960 FLUID")
	.map_io = msm8960_map_io,
	.reserve = msm8960_reserve,
	.init_irq = msm8960_init_irq,
	.handle_irq = gic_handle_irq,
	.timer = &msm_timer,
	.init_machine = msm8960_cdp_init,
	.init_early = msm8960_allocate_memory_regions,
	.init_very_early = msm8960_early_memory,
MACHINE_END

MACHINE_START(MSM8960_LIQUID, "QCT MSM8960 LIQUID")
	.map_io = msm8960_map_io,
	.reserve = msm8960_reserve,
	.init_irq = msm8960_init_irq,
	.handle_irq = gic_handle_irq,
	.timer = &msm_timer,
	.init_machine = msm8960_cdp_init,
	.init_early = msm8960_allocate_memory_regions,
	.init_very_early = msm8960_early_memory,
MACHINE_END
