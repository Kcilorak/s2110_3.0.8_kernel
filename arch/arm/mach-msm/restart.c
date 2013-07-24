/* Copyright (c) 2010-2011, Code Aurora Forum. All rights reserved.
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

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/reboot.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/cpu.h>
#include <linux/interrupt.h>
#include <linux/mfd/pmic8058.h>
#include <linux/mfd/pmic8901.h>
#include <linux/mfd/pm8xxx/misc.h>

#include <asm/mach-types.h>

#include <mach/msm_iomap.h>
#include <mach/restart.h>
#include <mach/socinfo.h>
#include <mach/irqs.h>
#include <mach/scm.h>
#include "msm_watchdog.h"
#include "timer.h"
#include <asm/setup.h>

#define WDT0_RST	0x38
#define WDT0_EN		0x40
#define WDT0_BARK_TIME	0x4C
#define WDT0_BITE_TIME	0x5C

#define PSHOLD_CTL_SU (MSM_TLMM_BASE + 0x820)

#define RESTART_REASON_ADDR 0x65C
#define DLOAD_MODE_ADDR     0x0

#define SCM_IO_DISABLE_PMIC_ARBITER	1

/*[20120524] BEGIN Jack1_Huang silent reset */
//#define PANIC_REASON_WATCH_DOG	     0x11223344
#define PANIC_REASON_KERNEL_PANIC   0x55667788
#define SHARED_MEM_BASE             0x2A03F000

static void *panic_reason_addr;
static void *panic_reason_addr2;
/*[20120524] END Jack1_Huang silent reset */
static int restart_mode;
void *restart_reason;

int pmic_reset_irq;
static void __iomem *msm_tmr0_base;

#ifdef CONFIG_MSM_DLOAD_MODE
static int in_panic;
static void *dload_mode_addr;

/* Download mode master kill-switch */
static int dload_set(const char *val, struct kernel_param *kp);
static int download_mode = 1;

module_param_call(download_mode, dload_set, param_get_int,
			&download_mode, 0644);

/*[20120524] BEGIN Jack1_Huang silent reset */
static void msm_set_panic_reason_to_imem(unsigned int reason,void *addr)
{
        if(NULL != addr){
                writel(reason, addr);
                mb();
        }
}

static uint32_t msm_get_panic_reason_from_imem(void* addr)
{
    uint32_t ret = 0;
    ret = readl((uint32_t*)addr);
    return ret;
}
/*[20120524] END Jack1_Huang silent reset */

static int panic_prep_restart(struct notifier_block *this,
                              unsigned long event, void *ptr)
{
        in_panic = 1;
        /*[20120524] BEGIN Jack1_Huang silent reset */
	msm_set_panic_reason_to_imem(PANIC_REASON_KERNEL_PANIC,panic_reason_addr);
        printk(KERN_ERR "[%s] msm_get_panic_reason_from_imem(panic_reason_addr) = 0x%x\n",__func__,msm_get_panic_reason_from_imem(panic_reason_addr));
	msm_set_panic_reason_to_imem(PANIC_REASON_KERNEL_PANIC,panic_reason_addr2);
        printk(KERN_ERR "[%s] msm_get_panic_reason_from_imem(panic_reason_addr2) = 0x%x\n",__func__,msm_get_panic_reason_from_imem(panic_reason_addr2));
	mdelay(1000);
        /*[20120524] END Jack1_Huang silent reset */
        return NOTIFY_DONE;
}

static struct notifier_block panic_blk = {
	.notifier_call	= panic_prep_restart,
};

static void set_dload_mode(int on)
{
	if (dload_mode_addr) {
		__raw_writel(on ? 0xE47B337D : 0, dload_mode_addr);
		__raw_writel(on ? 0xCE14091A : 0,
		       dload_mode_addr + sizeof(unsigned int));
		mb();
	}
}

static int dload_set(const char *val, struct kernel_param *kp)
{
	int ret;
	int old_val = download_mode;

	ret = param_set_int(val, kp);
	if (ret)
		return ret;

	/* If download_mode is not zero or one, ignore. */
	if (download_mode >> 1) {
		download_mode = old_val;
		return -EINVAL;
	}

	set_dload_mode(download_mode);
	printk(KERN_ERR "RESTART [%s] set_dload_mode(%d)\n",__func__,download_mode);
	return 0;
}
#else
#define set_dload_mode(x) do {} while (0)
#endif

void msm_set_restart_mode(int mode)
{
	restart_mode = mode;
}
EXPORT_SYMBOL(msm_set_restart_mode);

static void __msm_power_off(int lower_pshold)
{
	printk(KERN_CRIT "Powering off the SoC\n");
#ifdef CONFIG_MSM_DLOAD_MODE
	set_dload_mode(0);
#endif
	pm8xxx_reset_pwr_off(0);

	if (lower_pshold) {
		__raw_writel(0, PSHOLD_CTL_SU);
		mdelay(10000);
		printk(KERN_ERR "Powering off has failed\n");
	}
	return;
}

static void msm_power_off(void)
{
	/* MSM initiated power off, lower ps_hold */
	__msm_power_off(1);
}

static void cpu_power_off(void *data)
{
	int rc;

	pr_err("PMIC Initiated shutdown %s cpu=%d\n", __func__,
						smp_processor_id());
	if (smp_processor_id() == 0) {
		/*
		 * PMIC initiated power off, do not lower ps_hold, pmic will
		 * shut msm down
		 */
		__msm_power_off(0);

		pet_watchdog();
		pr_err("Calling scm to disable arbiter\n");
		/* call secure manager to disable arbiter and never return */
		rc = scm_call_atomic1(SCM_SVC_PWR,
						SCM_IO_DISABLE_PMIC_ARBITER, 1);

		pr_err("SCM returned even when asked to busy loop rc=%d\n", rc);
		pr_err("waiting on pmic to shut msm down\n");
	}

	preempt_disable();
	while (1)
		;
}

static irqreturn_t resout_irq_handler(int irq, void *dev_id)
{
	pr_warn("%s PMIC Initiated shutdown\n", __func__);
	oops_in_progress = 1;
	smp_call_function_many(cpu_online_mask, cpu_power_off, NULL, 0);
	if (smp_processor_id() == 0)
		cpu_power_off(NULL);
	preempt_disable();
	while (1)
		;
	return IRQ_HANDLED;
}

void arch_reset(char mode, const char *cmd)
{
#ifdef CONFIG_MSM_DLOAD_MODE

	/* This looks like a normal reboot at this point. */
	set_dload_mode(0);
        printk(KERN_ERR "RESTART [%s] set_dload_mode(%d)\n",__func__,download_mode);
	/* Write download mode flags if we're panic'ing */
	set_dload_mode(in_panic);
        printk(KERN_ERR "RESTART [%s] set_dload_mode(in_panic) = %d\n",__func__,in_panic);
        /* Kill download mode if master-kill switch is set */
        if (!download_mode)
	{
                set_dload_mode(0);
		printk(KERN_ERR "RESTART [%s] !download_mode set_dload_mode(0)\n",__func__);
	}
        /* Write download mode flags if restart_mode says so */
        if (restart_mode == RESTART_DLOAD)
	{
                set_dload_mode(1);
                printk(KERN_ERR "RESTART [%s] restart_mode == RESTART_DLOAD set_dload_mode(1)\n",__func__);
	}
#endif

	printk(KERN_NOTICE "Going down for restart now\n");

	pm8xxx_reset_pwr_off(1);

	if (cmd != NULL) {
		if (!strncmp(cmd, "bootloader", 10)) {
			__raw_writel(0x77665500, restart_reason);
		} else if (!strncmp(cmd, "recovery", 8)) {
			__raw_writel(0x77665502, restart_reason);
        /* PEGA.Jeremy1_Wang.120409.Begin[Fuse blow command] */
                } else if (!strncmp(cmd, "fuse_blow", 9)) {
                        __raw_writel(0x11223344, restart_reason);
        /* PEGA.Jeremy1_Wang.120409.End[Fuse blow command] */
		} else if (!strncmp(cmd, "oem-", 4)) {
			unsigned long code;
			code = simple_strtoul(cmd + 4, NULL, 16) & 0xff;
			__raw_writel(0x6f656d00 | code, restart_reason);
		} else {
			__raw_writel(0x77665501, restart_reason);
		}
	}

	__raw_writel(0, msm_tmr0_base + WDT0_EN);
	if (!(machine_is_msm8x60_fusion() || machine_is_msm8x60_fusn_ffa())) {
		mb();
		__raw_writel(0, PSHOLD_CTL_SU); /* Actually reset the chip */
		mdelay(5000);
		pr_notice("PS_HOLD didn't work, falling back to watchdog\n");
	}

	__raw_writel(1, msm_tmr0_base + WDT0_RST);
	__raw_writel(5*0x31F3, msm_tmr0_base + WDT0_BARK_TIME);
	__raw_writel(0x31F3, msm_tmr0_base + WDT0_BITE_TIME);
	__raw_writel(1, msm_tmr0_base + WDT0_EN);

	mdelay(10000);
	printk(KERN_ERR "Restarting has failed\n");
}

static int __init msm_restart_init(void)
{
	int rc;
        /* BEGIN Jack1_Huang 20120521 silent reset */
        void *imem = ioremap_nocache(SHARED_MEM_BASE, SZ_4K);
        /* END Jack1_Huang 20120521 silent reset */

#ifdef CONFIG_MSM_DLOAD_MODE
	atomic_notifier_chain_register(&panic_notifier_list, &panic_blk);
	dload_mode_addr = MSM_IMEM_BASE + DLOAD_MODE_ADDR;
	/* Reset detection is switched on below.*/
	//set_dload_mode(1);
	/* BEGIN Jack1_Huang 20120521 set the init dload_mode to 0 restart */
	if(download_mode)
	{
		printk(KERN_ERR "RESTART [%s], set_dload_mode(1), download_mode = %d\n",__func__,download_mode);
		set_dload_mode(1);
	}else
	{
                printk(KERN_ERR "RESTART [%s], set_dload_mode(0), download_mode = %d\n",__func__,download_mode);
		set_dload_mode(0);
	}
	/* END Jack1_Huang 20120521 set the init dload_mode to 0 restart */
#endif
	/*[20120524] BEGIN Jack1_Huang silent reset */
	//panic_reason_addr = SHARED_MEM_BASE + (5 * sizeof(unsigned int));
	panic_reason_addr = imem + 5*sizeof(unsigned int);
	panic_reason_addr2 = imem + 9*sizeof(unsigned int);
	printk(KERN_ERR "[%s] msm_get_panic_reason_from_imem(panic_reason_addr) = 0x%x\n",__func__,msm_get_panic_reason_from_imem(panic_reason_addr));
	msm_set_panic_reason_to_imem(0xFFFFFFFF,panic_reason_addr);
	//=========================backup mechanism=====================================//
        printk(KERN_ERR "[%s] msm_get_panic_reason_from_imem(panic_reason_addr2) = 0x%x\n",__func__,msm_get_panic_reason_from_imem(panic_reason_addr2));
        msm_set_panic_reason_to_imem(0xFFFFFFFF,panic_reason_addr2);
	/*[20120524] END Jack1_Huang silent reset */
	msm_tmr0_base = msm_timer_get_timer0_base();
	restart_reason = MSM_IMEM_BASE + RESTART_REASON_ADDR;
	pm_power_off = msm_power_off;

	if (pmic_reset_irq != 0) {
		rc = request_any_context_irq(pmic_reset_irq,
					resout_irq_handler, IRQF_TRIGGER_HIGH,
					"restart_from_pmic", NULL);
		if (rc < 0)
			pr_err("pmic restart irq fail rc = %d\n", rc);
	} else {
		pr_warn("no pmic restart interrupt specified\n");
	}
	return 0;
}

late_initcall(msm_restart_init);
