/* Copyright (C) 2008 Rodolfo Giometti <giometti@linux.it>
 * Copyright (C) 2008 Eurotech S.p.A. <info@eurotech.it>
 * Based on a previous work by Copyright (C) 2008 Texas Instruments, Inc.
 *
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include <linux/module.h>
#include <linux/param.h>
#include <linux/jiffies.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/idr.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <asm/unaligned.h>
#include <linux/time.h>
#include <linux/mfd/pmic8058.h>
#include <linux/regulator/pmic8058-regulator.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/machine.h>
#include <linux/err.h>
#include <linux/msm-charger.h>
#include <linux/i2c/bq27520.h> /* use the same platform data as bq27520 */
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <mach/msm_iomap.h>
#include <linux/io.h>

#include <linux/msm-charger-log.h>

/*
 * 1.1.0: Initial version from QCom
 * 1.4.0: Add CONFIG_BQ27541_RETRY
 * 1.5.0: Add CONFIG_BQ27541_CACHE
 * 1.6.0: Add CONFIG_BQ27541_CONTROL_REGULATOR
 * 1.7.0: Remove CONFIG_BQ27541_COLUMB_COUNTER_ENABLE
 * 1.8.0: Remove CONFIG_BQ27541_POLLING_ENABLE
 * 1.9.0: Add CONFIG_BQ27541_DUMMY_ENABLE
 */

#define DRIVER_VERSION          "1.9.0"
/* Bq27541 standard data commands */
#define BQ27541_REG_CNTL        0x00
#define BQ27541_REG_AR          0x02
#define BQ27541_REG_ARTTE       0x04
#define BQ27541_REG_TEMP        0x06
#define BQ27541_REG_VOLT        0x08
#define BQ27541_REG_FLAGS       0x0A
#define BQ27541_REG_NAC         0x0C
#define BQ27541_REG_FAC         0x0e
#define BQ27541_REG_RM          0x10
#define BQ27541_REG_FCC         0x12
#define BQ27541_REG_AI          0x14
#define BQ27541_REG_TTE         0x16
#define BQ27541_REG_TTF         0x18
#define BQ27541_REG_SI          0x1a
#define BQ27541_REG_STTE        0x1c
#define BQ27541_REG_MLI         0x1e
#define BQ27541_REG_MLTTE       0x20
#define BQ27541_REG_AE          0x22
#define BQ27541_REG_AP          0x24
#define BQ27541_REG_TTECP       0x26
#define BQ27541_REG_INTTEMP     0x28
#define BQ27541_REG_CC          0x2a
#define BQ27541_REG_SOC         0x2c
#define BQ27541_REG_SOH         0x2e
/*#define BQ27541_REG_NIC         0x2e*/
#define BQ27541_REG_ICR         0x30
#define BQ27541_REG_LOGIDX      0x32
#define BQ27541_REG_LOGBUF      0x34
/* Extended Command */
#define BQ27541_REG_DCAP        0x3C

/* Flags of BQ27541_REG_FLAGS */
#define BQ27541_FLAG_DSC        BIT(0)
#define BQ27541_FLAG_SOCF       BIT(1)
#define BQ27541_FLAG_SOC1       BIT(2)
#define BQ27541_FLAG_HW0        BIT(3)
#define BQ27541_FLAG_HW1        BIT(4)
#define BQ27541_FLAG_TDD        BIT(5)
#define BQ27541_FLAG_ISD        BIT(6)
#define BQ27541_FLAG_OVERTAKEN  BIT(7)
#define BQ27541_FLAG_CHG        BIT(8)
#define BQ27541_FLAG_FC         BIT(9)
#define BQ27541_FLAG_XCHG       BIT(10)
#define BQ27541_FLAG_CHG_INH    BIT(11)
#define BQ27541_FLAG_BATLOW     BIT(12)
#define BQ27541_FLAG_BATHIGH    BIT(13)
#define BQ27541_FLAG_OTD        BIT(14)
#define BQ27541_FLAG_OTC        BIT(15)

/* Control subcommands */
#define BQ27541_SUBCMD_CNTL_STATUS   0x0000
#define BQ27541_SUBCMD_DEVICE_TYPE   0x0001
#define BQ27541_SUBCMD_FW_VER        0x0002
#define BQ27541_SUBCMD_HW_VER        0x0003
#define BQ27541_SUBCMD_DF_CSUM       0x0004
#define BQ27541_SUBCMD_PREV_MACW     0x0007
#define BQ27541_SUBCMD_CHEM_ID       0x0008
#define BQ27541_SUBCMD_BD_OFFSET     0x0009
#define BQ27541_SUBCMD_INT_OFFSET    0x000a
#define BQ27541_SUBCMD_CC_VER        0x000b
#define BQ27541_SUBCMD_DF_VER        0x000c
#define BQ27541_SUBCMD_BAT_INS       0x000d
#define BQ27541_SUBCMD_BAT_REM       0x000e
#define BQ27541_SUBCMD_SET_HIB       0x0011
#define BQ27541_SUBCMD_CLR_HIB       0x0012
#define BQ27541_SUBCMD_SET_SLP       0x0013
#define BQ27541_SUBCMD_CLR_SLP       0x0014
#define BQ27541_SUBCMD_FCT_RES       0x0015
#define BQ27541_SUBCMD_ENABLE_DLOG   0x0018
#define BQ27541_SUBCMD_DISABLE_DLOG  0x0019
#define BQ27541_SUBCMD_SEALED        0x0020
#define BQ27541_SUBCMD_ENABLE_IT     0x0021
#define BQ27541_SUBCMD_DISABLE_IT    0x0023
#define BQ27541_SUBCMD_CAL_MODE      0x0040
#define BQ27541_SUBCMD_RESET         0x0041

/* Flags of BQ27541_SUBCMD_CNTL_STATUS */
/*#define BQ27541_CS_DLOGEN       BIT(15)*/
/*
#define BQ27541_CS_SE           BIT(15)
#define BQ27541_CS_FAS          BIT(14)
#define BQ27541_CS_SS           BIT(13)
#define BQ27541_CS_CSV          BIT(12)
#define BQ27541_CS_CCA          BIT(11)
#define BQ27541_CS_BCA          BIT(10)
#define BQ27541_CS_HDAINTEN     BIT(8)
#define BQ27541_CS_SHUTDOWN     BIT(7)
#define BQ27541_CS_HIBERNATE    BIT(6)
#define BQ27541_CS_FULLSLEEP    BIT(5)
#define BQ27541_CS_SLEEP        BIT(4)
#define BQ27541_CS_LDMD         BIT(3)
#define BQ27541_CS_RUPDIS       BIT(2)
#define BQ27541_CS_VOK          BIT(1)
#define BQ27541_CS_QEN          BIT(0)
*/

/* Set 1 if you want to enable retry mechanism */
#define CONFIG_BQ27541_RETRY               1

/* Set 1 if you want to enable cache mechanism,
   But you have to enable retry mechanism first. */
#define CONFIG_BQ27541_CACHE               1

/* Set 1 if you want to control the regulator. */
#define CONFIG_BQ27541_CONTROL_REGULATOR   1

/* Set 1 if you want to support dummy battery.
   But you have to define PEGA_CHG_FAKE_BATT_ENABLE. */
#ifdef PEGA_CHG_FAKE_BATT_ENABLE
#define CONFIG_BQ27541_DUMMY_ENABLE        1
#endif /* PEGA_CHG_FAKE_BATT_ENABLE */

#ifdef PEGA_SMT_BUILD
/* For interrupt pin, disable interrupt function when SMT version */
#define CONFIG_BQ27541_INTERRUPT_ENABLE    0
#define BATT_FULL_CAPACITY_VAL            50
#else
#define CONFIG_BQ27541_INTERRUPT_ENABLE    1
#define BATT_FULL_CAPACITY_VAL           100
#endif  /* PEGA_SMT_BUILD */

#define ABS(i)          ((i) > 0 ? (i) : -(i))

#define ZERO_DEGREE_CELSIUS_IN_TENTH_KELVIN   (-2731)
#define BQ27541_INIT_DELAY         ((HZ) * 1)

#define BQ27541_SHOW_LOG_INTERVAL   3000 // ms

#if (CONFIG_BQ27541_RETRY == 1)
// If the different between two results larger than acceptible value, let the device crash.
#define BATT_RETRY_TEMP_DIFF         300
#define BATT_RETRY_VOLT_DIFF         300
#define BATT_RETRY_CAP_DIFF          300
#define BATT_RETRY_RSOC_DIFF           5
#define BATT_MAX_RETRY_TIMES           3

#define BATT_VALID_LOW_VOLT         2800
#define BATT_VALID_HIGH_VOLT        4240
#define BATT_VALID_LOW_TEMP         -400
#define BATT_VALID_HIGH_TEMP        1500
#define BATT_VALID_LOW_FCC             0
#define BATT_VALID_HIGH_FCC         6500
#define BATT_VALID_LOW_RM              0
#define BATT_VALID_HIGH_RM          6500
#define BATT_VALID_LOW_RSOC            0
#define BATT_VALID_HIGH_RSOC         100
#endif /* CONFIG_BQ27541_RETRY */

// shut down condition, depends on framework
#define BATT_SHUTDOWN_OVERTEMP       700  // shut down if >= 70 deg C
#define BATT_SHUTDOWN_LOWTEMP       -200  // shut down if <= -20 deg C
#define BATT_SHUTDOWN_LOWVOLT       3450  // shut down if <= 3.45V
#define BATT_SHUTDOWN_LEVEL            0  // shut down if <= 0%.
#define BATT_REMAP_BASE_LEVEL         10  // Remap 10%~94% to 1%~99%

#define BATT_HEALTH_HEAT             450  // stop charging when > 45 deg C
#define BATT_HEALTH_COLD               0  // stop charging when < 0 deg C

#define BATT_CRASH_LOW_VOLT            0
#define BATT_CRASH_HIGH_VOLT        6000
#define BATT_CRASH_LOW_RSOC            0
#define BATT_CRASH_HIGH_RSOC         100
#define BATT_CRASH_LOW_TEMP         -400
#define BATT_CRASH_HIGH_TEMP         850
/*
// Protection function
#define BATT_UNDER_VOLT             2800
#define BATT_OVER_VOLT              4280
#define BATT_CHG_UNDER_TEMP            0
#define BATT_CHG_OVER_TEMP           550
#define BATT_DSG_UNDER_TEMP         -100
#define BATT_DSG_OVER_TEMP           700
*/

// interrupt condition, depends on interrupt pin
#define BATT_INTERRUPT_LOWVOLT      3500  // interrupt if 3.50V
#define BATT_INTERRUPT_LOWCAP        760  // interrupt if 760mAh
#define BATT_INTERRUPT_OVERVOLT     4240  // interrupt if 4.24V
#define BATT_INTERRUPT_OTC           550  // interrupt if 55 deg C
#define BATT_INTERRUPT_OTD           700  // interrupt if 70 deg C

#if (CONFIG_BQ27541_INTERRUPT_ENABLE == 1)
#define BATT_INT_GPIO                 67
#define GPIO_CONFIG(gpio)    (MSM_TLMM_BASE + 0x1000 + (0x10 * (gpio)))
#endif /* CONFIG_BQ27541_INTERRUPT_ENABLE */

#if (CONFIG_BQ27541_DUMMY_ENABLE == 1)
#define BATT_DUMMY_LOWEST_TEMP         0
#define BATT_DUMMY_LOWEST_LEVEL       15
#endif /* CONFIG_BQ27541_DUMMY_ENABLE */

/* If the system has several batteries we need a different name for each
 * of them...
 */
static DEFINE_IDR(battery_id);
static DEFINE_MUTEX(battery_mutex);

#if (CONFIG_BQ27541_CONTROL_REGULATOR == 1)
static struct regulator *gauge_reg_l11;
#endif /* CONFIG_BQ27541_CONTROL_REGULATOR */

struct bq27541_device_info;
struct bq27541_access_methods {
	int (*read)(u8 reg, int *rt_value, int b_single,
		struct bq27541_device_info *di);
};

struct bq27541_device_info {
	struct device          *dev;
	int                     id;
	struct bq27541_access_methods	*bus;
	struct i2c_client      *client;
	/* 300ms delay is needed after bq27541 is powered up
	 * and before any successful I2C transaction
	 */
	struct delayed_work     hw_config;
	unsigned short          fw_ver;
	unsigned short          df_ver;
	unsigned short          conf;
	int                     is_gauge_ready;
	int                     is_batt_dead;
#if (CONFIG_BQ27541_DUMMY_ENABLE == 1)
	int                     is_dummy_batt;
#endif /* CONFIG_BQ27541_DUMMY_ENABLE */
};
struct bq27541_device_info *bq27541_di;

#if (CONFIG_BQ27541_RETRY == 1)
#if (CONFIG_BQ27541_CACHE == 1)
#define BQ27541_CACHED_RESET_DELAY ((HZ) * 1)
#define BQ27541_CACHED_UPDATE_INTERVAL    1000  // msec
enum {
	BATTERY_CACHED_TEMP,
	BATTERY_CACHED_VOLT,
	BATTERY_CACHED_AI,
	BATTERY_CACHED_RSOC,
	BATTERY_CACHED_RM,
	BATTERY_CACHED_FCC,
	BATTERY_CACHED_LEVEL,
	NUM_OF_BATTERY_CACHED,
};
struct bq27541_cached {
	int                  cached_val[NUM_OF_BATTERY_CACHED];
	int                  sspnd_val[NUM_OF_BATTERY_CACHED];
	unsigned long        update_msec[NUM_OF_BATTERY_CACHED];
	spinlock_t           lock;
	int                  reset_delay_on;
	struct delayed_work	 reset_delay;
};
static struct bq27541_cached batt_cached_info;

#else  /* CONFIG_BQ27541_CACHE */
static int pre_volt, pre_temp, pre_rsoc, pre_level;
#endif /* CONFIG_BQ27541_CACHE */
#endif /* CONFIG_BQ27541_RETRY */

#if (CONFIG_BQ27541_INTERRUPT_ENABLE == 1)
struct bq27541_irq {
	unsigned int         irq_num;
	struct delayed_work  work;
	struct wake_lock     wl;
	int                  irq_on;
};
static struct bq27541_irq gauge_irq;
#endif /* CONFIG_BQ27541_INTERRUPT_ENABLE */

static int bq27541_i2c_txsubcmd(u8 reg, unsigned short subcmd,
		struct bq27541_device_info *di);

static int bq27541_read(u8 reg, int *rt_value, int b_single,
			struct bq27541_device_info *di)
{
	return di->bus->read(reg, rt_value, b_single, di);
}

#if (CONFIG_BQ27541_CACHE == 1 && CONFIG_BQ27541_RETRY == 1)
static void bq27541_cached_info_init(void)
{
	int i = 0;
	unsigned long flag;

	PEGA_DBG_L("[BATT] %s is called.\n", __func__);
	if (batt_cached_info.reset_delay_on)
		cancel_delayed_work(&batt_cached_info.reset_delay);

	spin_lock_irqsave(&batt_cached_info.lock, flag);
	for (i = 0; i < NUM_OF_BATTERY_CACHED; i++)
	{
		batt_cached_info.update_msec[i] = 0;
		batt_cached_info.cached_val[i] = -1;
		batt_cached_info.sspnd_val[i] = -1;
	}
	batt_cached_info.reset_delay_on = 0;
	spin_unlock_irqrestore(&batt_cached_info.lock, flag);
}

static void bq27541_cached_suspend(void)
{
	int i = 0;
	unsigned long flag;

	PEGA_DBG_L("[BATT] %s is called.\n", __func__);
	if (batt_cached_info.reset_delay_on)
		cancel_delayed_work(&batt_cached_info.reset_delay);

	spin_lock_irqsave(&batt_cached_info.lock, flag);
	for (i = 0; i < NUM_OF_BATTERY_CACHED; i++)
	{
		batt_cached_info.update_msec[i] = 0;
		batt_cached_info.cached_val[i] = -1;
	}
	batt_cached_info.reset_delay_on = 0;
	spin_unlock_irqrestore(&batt_cached_info.lock, flag);
}

static void bq27541_cached_reset(struct work_struct *work)
{
	int i = 0;
	unsigned long flag;

	PEGA_DBG_L("[BATT] %s is called.\n", __func__);
	if (batt_cached_info.reset_delay_on)
		cancel_delayed_work(&batt_cached_info.reset_delay);

	spin_lock_irqsave(&batt_cached_info.lock, flag);
	for (i = 0; i < NUM_OF_BATTERY_CACHED; i++)
	{
		batt_cached_info.update_msec[i] = 0;
	}
	batt_cached_info.reset_delay_on = 0;
	spin_unlock_irqrestore(&batt_cached_info.lock, flag);
}

static void bq27541_cached_reset_msec(int batt_cached_type)
{
	unsigned long flag;

	PEGA_DBG_L("[BATT] %s is called.\n", __func__);
	spin_lock_irqsave(&batt_cached_info.lock, flag);
	batt_cached_info.update_msec[batt_cached_type] = 0;
	spin_unlock_irqrestore(&batt_cached_info.lock, flag);
}

static void bq27541_cached_update(int type, int val)
{
	unsigned long flag;

	PEGA_DBG_L("[BATT] %s is called, type: %d, value: %d.\n",
				__func__, type, val);
	if (batt_cached_info.reset_delay_on)
		cancel_delayed_work(&batt_cached_info.reset_delay);
	schedule_delayed_work(&batt_cached_info.reset_delay,
				BQ27541_CACHED_RESET_DELAY);
	spin_lock_irqsave(&batt_cached_info.lock, flag);
	batt_cached_info.update_msec[type] = jiffies_to_msecs(jiffies);
	batt_cached_info.cached_val[type] = val;
	batt_cached_info.sspnd_val[type] = val;
	batt_cached_info.reset_delay_on = 1;
	spin_unlock_irqrestore(&batt_cached_info.lock, flag);
}

static void bq27541_cached_init(void)
{
	PEGA_DBG_L("[BATT] %s is called.\n", __func__);
	spin_lock_init(&batt_cached_info.lock);
	bq27541_cached_info_init();
	INIT_DELAYED_WORK(&batt_cached_info.reset_delay,
			bq27541_cached_reset);
}
#endif // CONFIG_BQ27541_CACHE && CONFIG_BQ27541_RETRY

static int bq27541_avg_level(int level)
{
	static int avg_level[10];
	static int cnt = 0, num = 0;
	int i = 0, total = 0, ret = 0, repeat = 1;

	if (100 == level)
		repeat = 5;
	else if (0 == level)
		repeat = 3;
	else
		repeat = 1;

	for (i = 0; i < repeat; i++)
	{
		if (cnt < 10)
			cnt++;
		avg_level[num % cnt] = level;
		num++;
	}

	for (i = 0; i < cnt; i++)
		total += avg_level[i];

	ret = total / cnt + (int)((total % cnt) > 4);

	PEGA_DBG_L("[BATT] %s, avg_level=%d.(%d, %d, %d, %d, %d, %d, %d, %d, %d, %d).\n",
			__func__, ret, avg_level[0], avg_level[1],
			avg_level[2], avg_level[3], avg_level[4], avg_level[5],
			avg_level[6], avg_level[7], avg_level[8], avg_level[9]);

	return ret;
}

/*
 * Return the firmware version of battery gauge
 * Or "none" if something fails.
 */
static char* bq27541_battery_fw_version(struct bq27541_device_info *di)
{
	static char ver[16];
	int flag = 0;

#if (CONFIG_BQ27541_DUMMY_ENABLE == 1)
	if (di->is_dummy_batt)
		flag = 2;
#endif /* CONFIG_BQ27541_DUMMY_ENABLE */

#if (CONFIG_BQ27541_INTERRUPT_ENABLE == 1)
	flag += gauge_irq.irq_on;
#endif /* CONFIG_BQ27541_INTERRUPT_ENABLE */

	sprintf(ver, "%04x%04X-%04X%x", di->fw_ver, di->df_ver, di->conf, flag);
	return ver;
}

/*
 * Return the battery temperature in tenths of degree Celsius
 * Or 0 if something fails.
 */
static int bq27541_battery_temperature(struct bq27541_device_info *di)
{
	int ret = 0, temp = 0;
#if (CONFIG_BQ27541_RETRY == 1)
	static int temp_retry_cnt = 0;
#if (CONFIG_BQ27541_CACHE == 1)
	int pre_temp = batt_cached_info.cached_val[BATTERY_CACHED_TEMP];
	int sspnd_temp = batt_cached_info.sspnd_val[BATTERY_CACHED_TEMP];
	unsigned long curr_msec = 0;
	unsigned long pre_msec = batt_cached_info.update_msec[BATTERY_CACHED_TEMP];
#else  /* CONFIG_BQ27541_CACHE */
	static int sspnd_temp = 0;
#endif /* CONFIG_BQ27541_CACHE */
#endif /* CONFIG_BQ27541_RETRY */

	if (!di->is_gauge_ready || di->is_batt_dead)
		return 0;

#if (CONFIG_BQ27541_CACHE == 1 && CONFIG_BQ27541_RETRY == 1)
	curr_msec = jiffies_to_msecs(jiffies);

	if (0 < pre_msec &&
	    BQ27541_CACHED_UPDATE_INTERVAL > ABS(curr_msec - pre_msec))
	{
		PEGA_DBG_L("[BATT] use cached temp, curr_msec: %lu, update_msec: %lu, pre_temp: %d.\n",
					curr_msec, pre_msec, pre_temp);
		return pre_temp;
	}
	else
	{
		PEGA_DBG_L("[BATT] read temp from gauge, curr_msec: %lu, update_msec: %lu, pre_temp: %d.\n",
					curr_msec, pre_msec, pre_temp);
	}
#endif /* CONFIG_BQ27541_CACHE && CONFIG_BQ27541_RETRY */

	udelay(66);
	ret = bq27541_read(BQ27541_REG_TEMP, &temp, 0, di);
	if (ret)
	{
		PEGA_DBG_H("[BATT_ERR] error(0x%08x) reading temperature.\n", ret);
#if (CONFIG_BQ27541_RETRY == 1)
		if (-1 == pre_temp)
			pre_temp = (-1 != sspnd_temp) ? sspnd_temp : 0;
		return pre_temp;
#else  /* CONFIG_BQ27541_RETRY */
		return 0;
#endif /* CONFIG_BQ27541_RETRY */
	}

	temp += ZERO_DEGREE_CELSIUS_IN_TENTH_KELVIN;
	PEGA_DBG_L("[BATT] %s, temp=%d.\n", __func__, temp);

#if (CONFIG_BQ27541_DUMMY_ENABLE == 1)
	if (1 == di->is_dummy_batt && BATT_DUMMY_LOWEST_TEMP > temp)
		temp = BATT_DUMMY_LOWEST_TEMP;
#endif /* CONFIG_BQ27541_DUMMY_ENABLE */

#if (CONFIG_BQ27541_RETRY == 0)

	if (ZERO_DEGREE_CELSIUS_IN_TENTH_KELVIN == temp)
	{
		temp = 0;
	}

	return temp;

#else  /* CONFIG_BQ27541_RETRY */
	if (BATT_VALID_LOW_TEMP <= temp && BATT_VALID_HIGH_TEMP >= temp)
		goto done;
	else
		goto retry;

done:
	if (ABS(temp - pre_temp) <= BATT_RETRY_TEMP_DIFF || -1 == pre_temp)
	{
		temp_retry_cnt = BATT_MAX_RETRY_TIMES;
	}

retry:
	if (BATT_MAX_RETRY_TIMES <= temp_retry_cnt)
	{
		PEGA_DBG_L("[BATT_RETRY] %s, max retry count, temp=%d, return.\n",
			__func__, temp);
		temp_retry_cnt = 0;
#if (CONFIG_BQ27541_CACHE == 1)
		bq27541_cached_update(BATTERY_CACHED_TEMP, temp);
		pre_temp = temp;
#else  /* CONFIG_BQ27541_CACHE */
		sspnd_temp = pre_temp = temp;
#endif /* CONFIG_BQ27541_CACHE */
	}
	else
	{
		PEGA_DBG_H("[BATT_RETRY] %s, retry=%d, temp=%d, return pre_temp=%d or sspnd_temp=%d\n",
			__func__, temp_retry_cnt, temp, pre_temp, sspnd_temp);
		temp_retry_cnt++;

		if (-1 == pre_temp)
			pre_temp = (-1 == sspnd_temp) ? 0 : sspnd_temp;

#if (CONFIG_BQ27541_CACHE == 1)
		bq27541_cached_reset_msec(BATTERY_CACHED_TEMP);
#endif /* CONFIG_BQ27541_CACHE */
	}
	return pre_temp;
#endif /* CONFIG_BQ27541_RETRY */
}

/*
 * Return the battery Voltage in milivolts
 * Or 0 if something fails.
 */
static int bq27541_battery_voltage(struct bq27541_device_info *di)
{
	int ret = 0, volt = 0;
#if (CONFIG_BQ27541_RETRY == 1)
	static int volt_retry_cnt = 0;
#if (CONFIG_BQ27541_CACHE == 1)
	int pre_volt = batt_cached_info.cached_val[BATTERY_CACHED_VOLT];
	int sspnd_volt = batt_cached_info.sspnd_val[BATTERY_CACHED_VOLT];
	unsigned long curr_msec = 0;
	unsigned long pre_msec = batt_cached_info.update_msec[BATTERY_CACHED_VOLT];
#else  /* CONFIG_BQ27541_CACHE */
	static int sspnd_volt = 0;
#endif /* CONFIG_BQ27541_CACHE */
#endif /* CONFIG_BQ27541_RETRY */

	if (!di->is_gauge_ready || di->is_batt_dead)
		return 0;

#if (CONFIG_BQ27541_CACHE == 1 && CONFIG_BQ27541_RETRY == 1)
	curr_msec = jiffies_to_msecs(jiffies);

	if (0 < pre_msec &&
	    BQ27541_CACHED_UPDATE_INTERVAL > ABS(curr_msec - pre_msec))
	{
		PEGA_DBG_L("[BATT] use cached volt, curr_msec: %lu, update_msec: %lu, pre_volt: %d.\n",
					curr_msec, pre_msec, pre_volt);
		return pre_volt;
	}
	else
	{
		PEGA_DBG_L("[BATT] read volt from gauge, curr_msec: %lu, update_msec: %lu, pre_volt: %d.\n",
					curr_msec, pre_msec, pre_volt);
	}
#endif /* CONFIG_BQ27541_CACHE && CONFIG_BQ27541_RETRY */

	udelay(66);
	ret = bq27541_read(BQ27541_REG_VOLT, &volt, 0, di);
	if (ret)
	{
		PEGA_DBG_H("[BATT_ERR] error(0x%08x) reading voltage.\n", ret);
#if (CONFIG_BQ27541_RETRY == 1)
		if (-1 == pre_volt)
			pre_volt = (-1 != sspnd_volt) ? sspnd_volt : 0;
		return pre_volt;
#else  /* CONFIG_BQ27541_RETRY */
		return 0;
#endif /* CONFIG_BQ27541_RETRY */
	}

	PEGA_DBG_L("[BATT] %s, volt=%d.\n", __func__, volt);

#if (CONFIG_BQ27541_RETRY == 0)
	return volt;

#else  /* CONFIG_BQ27541_RETRY */
	if (BATT_VALID_LOW_VOLT <= volt && BATT_VALID_HIGH_VOLT >= volt)
		goto done;
	else
		goto retry;

done:
	if (ABS(volt - pre_volt) < BATT_RETRY_VOLT_DIFF || -1 == pre_volt)
	{
		volt_retry_cnt = BATT_MAX_RETRY_TIMES;
	}

retry:
	if (BATT_MAX_RETRY_TIMES <= volt_retry_cnt)
	{
		PEGA_DBG_L("[BATT_RETRY] %s, max retry count, volt=%d, return.\n",
			__func__, volt);
		volt_retry_cnt = 0;
#if (CONFIG_BQ27541_CACHE == 1)
		bq27541_cached_update(BATTERY_CACHED_VOLT, volt);
		pre_volt = volt;
#else  /* CONFIG_BQ27541_CACHE */
		sspnd_volt = pre_volt = volt;
#endif /* CONFIG_BQ27541_CACHE */
	}
	else
	{
		PEGA_DBG_H("[BATT_RETRY] %s, retry=%d, volt=%d, return pre_volt=%d or sspnd_volt=%d\n",
			__func__, volt_retry_cnt, volt, pre_volt, sspnd_volt);
		volt_retry_cnt++;

		if (-1 == pre_volt)
			pre_volt = (-1 == sspnd_volt) ? 0 : sspnd_volt;

#if (CONFIG_BQ27541_CACHE == 1)
		bq27541_cached_reset_msec(BATTERY_CACHED_VOLT);
#endif /* CONFIG_BQ27541_CACHE */
	}
	return pre_volt;
#endif /* CONFIG_BQ27541_RETRY */
}

/*
 * Return the battery average current
 * Note that current can be negative signed as well
 * Or 0 if something fails.
 */
static int bq27541_battery_current(struct bq27541_device_info *di)
{
	int ret = 0, curr = 0;
#if (CONFIG_BQ27541_RETRY == 1)
#if (CONFIG_BQ27541_CACHE == 1)
	int pre_curr = batt_cached_info.cached_val[BATTERY_CACHED_AI];
	unsigned long curr_msec = 0;
	unsigned long pre_msec = batt_cached_info.update_msec[BATTERY_CACHED_AI];
#else  /* CONFIG_BQ27541_CACHE */
	static int pre_curr = 0;
#endif /* CONFIG_BQ27541_CACHE */
#endif /* CONFIG_BQ27541_RETRY */

	if (!di->is_gauge_ready || di->is_batt_dead)
		return 0;

#if (CONFIG_BQ27541_CACHE == 1 && CONFIG_BQ27541_RETRY == 1)
	curr_msec = jiffies_to_msecs(jiffies);

	if (0 < pre_msec &&
	    BQ27541_CACHED_UPDATE_INTERVAL > ABS(curr_msec - pre_msec))
	{
		PEGA_DBG_L("[BATT] use cached curr, curr_msec: %lu, update_msec: %lu, pre_curr: %d.\n",
					curr_msec, pre_msec, pre_curr);
		return pre_curr;
	}
	else
	{
		PEGA_DBG_L("[BATT] read curr from gauge, curr_msec: %lu, update_msec: %lu, pre_curr: %d.\n",
					curr_msec, pre_msec, pre_curr);
	}
#endif /* CONFIG_BQ27541_CACHE && CONFIG_BQ27541_RETRY */

	udelay(66);
	ret = bq27541_read(BQ27541_REG_AI, &curr, 0, di);
	if (ret)
	{
		PEGA_DBG_H("[BATT_ERR] error(0x%08x) reading current\n", ret);
#if (CONFIG_BQ27541_RETRY == 1)
		if (-1 == pre_curr)
			pre_curr = 0;
		return pre_curr;
#else  /* CONFIG_BQ27541_RETRY */
		return 0;
#endif /* CONFIG_BQ27541_RETRY */
	}

	curr = (curr > 32768) ? curr - 65536 : curr;
	PEGA_DBG_L("[BATT] %s, curr= %d\n", __func__, curr);

#if (CONFIG_BQ27541_RETRY == 0)
	return curr;
#else  /* CONFIG_BQ27541_RETRY */
#if (CONFIG_BQ27541_CACHE == 1)
	bq27541_cached_update(BATTERY_CACHED_AI, curr);
#endif /* CONFIG_BQ27541_CACHE */
	pre_curr = curr;
	return pre_curr;
#endif /* CONFIG_BQ27541_RETRY */
}

/*
 * Return the compensated capacity of the battery when full charged (mAh)
 * Or 0 if something fails.
 */
static int bq27541_battery_fcc(struct bq27541_device_info *di)
{
	int ret = 0, fcc = 0;
#if (CONFIG_BQ27541_RETRY == 1)
	static int fcc_retry_cnt = 0;
#if (CONFIG_BQ27541_CACHE == 1)
	int pre_fcc = batt_cached_info.cached_val[BATTERY_CACHED_FCC];
	int sspnd_fcc = batt_cached_info.sspnd_val[BATTERY_CACHED_FCC];
	unsigned long curr_msec = 0;
	unsigned long pre_msec = batt_cached_info.update_msec[BATTERY_CACHED_FCC];
#else  /* CONFIG_BQ27541_CACHE */
	static int pre_fcc = 0;
	static int sspnd_fcc = 0;
#endif /* CONFIG_BQ27541_CACHE */
#endif /* CONFIG_BQ27541_RETRY */

	if (!di->is_gauge_ready || di->is_batt_dead)
		return 0;

#if (CONFIG_BQ27541_CACHE == 1 && CONFIG_BQ27541_RETRY == 1)
	curr_msec = jiffies_to_msecs(jiffies);

	if (0 < pre_msec &&
	    BQ27541_CACHED_UPDATE_INTERVAL > ABS(curr_msec - pre_msec))
	{
		PEGA_DBG_L("[BATT] use cached fcc, curr_msec: %lu, update_msec: %lu, pre_fcc: %d.\n",
					curr_msec, pre_msec, pre_fcc);
		return pre_fcc;
	}
	else
	{
		PEGA_DBG_L("[BATT] read fcc from gauge, curr_msec: %lu, update_msec: %lu, pre_fcc: %d.\n",
					curr_msec, pre_msec, pre_fcc);
	}
#endif /* CONFIG_BQ27541_CACHE && CONFIG_BQ27541_RETRY */

	udelay(66);
	ret = bq27541_read(BQ27541_REG_FCC, &fcc, 0, di);
	if (ret)
	{
		PEGA_DBG_H("[BATT_ERR] error(0x%08x) reading fcc\n", ret);
#if (CONFIG_BQ27541_RETRY == 1)
		if (-1 == pre_fcc)
			pre_fcc = 0;
		return pre_fcc;
#else  /* CONFIG_BQ27541_RETRY */
		return 0;
#endif /* CONFIG_BQ27541_RETRY */
	}

	PEGA_DBG_L("[BATT] %s, fcc= %d\n", __func__, fcc);

#if (CONFIG_BQ27541_RETRY == 0)
	return fcc;
#else  /* CONFIG_BQ27541_RETRY */
	if (BATT_VALID_LOW_FCC <= fcc && BATT_VALID_HIGH_FCC >= fcc)
		goto done;
	else
		goto retry;

done:
	if (ABS(fcc - pre_fcc) <= BATT_RETRY_CAP_DIFF || -1 == pre_fcc)
	{
		fcc_retry_cnt = BATT_MAX_RETRY_TIMES;
	}

retry:
	if (BATT_MAX_RETRY_TIMES <= fcc_retry_cnt)
	{
		PEGA_DBG_L("[BATT_RETRY] %s, max retry count, fcc=%d, return.\n",
			__func__, fcc);
		fcc_retry_cnt = 0;

#if (CONFIG_BQ27541_CACHE == 1)
		bq27541_cached_update(BATTERY_CACHED_FCC, fcc);
		pre_fcc = fcc;
#else  /* CONFIG_BQ27541_CACHE */
		sspnd_fcc = pre_fcc = fcc;
#endif /* CONFIG_BQ27541_CACHE */
	}
	else
	{
		PEGA_DBG_H("[BATT_RETRY] %s, retry=%d, fcc=%d, return pre_fcc=%d or sspnd_fcc=%d\n",
			__func__, fcc_retry_cnt, fcc, pre_fcc, sspnd_fcc);
		fcc_retry_cnt++;

		if (-1 == pre_fcc)
			pre_fcc = (-1 == sspnd_fcc) ? 0 : sspnd_fcc;

#if (CONFIG_BQ27541_CACHE == 1)
		bq27541_cached_reset_msec(BATTERY_CACHED_FCC);
#endif /* CONFIG_BQ27541_CACHE */
	}
	return pre_fcc;
#endif /* CONFIG_BQ27541_RETRY */
}

/*
 * Return the compensated capacity remaining (mAh)
 * Or 0 if something fails.
 */
static int bq27541_battery_rm(struct bq27541_device_info *di)
{
	int ret = 0, rm = 0, fcc = 0;
#if (CONFIG_BQ27541_RETRY == 1)
	static int rm_retry_cnt = 0;
#if (CONFIG_BQ27541_CACHE == 1)
	int pre_rm = batt_cached_info.cached_val[BATTERY_CACHED_RM];
	int sspnd_rm = batt_cached_info.sspnd_val[BATTERY_CACHED_RM];
	unsigned long curr_msec = 0;
	unsigned long pre_msec = batt_cached_info.update_msec[BATTERY_CACHED_RM];
#else  /* CONFIG_BQ27541_CACHE */
	static int pre_rm = 0;
	static int sspnd_rm = 0;
#endif /* CONFIG_BQ27541_CACHE */
#endif /* CONFIG_BQ27541_RETRY */

	if (!di->is_gauge_ready || di->is_batt_dead)
		return 0;

#if (CONFIG_BQ27541_CACHE == 1 && CONFIG_BQ27541_RETRY == 1)
	curr_msec = jiffies_to_msecs(jiffies);

	if (0 < pre_msec &&
	    BQ27541_CACHED_UPDATE_INTERVAL > ABS(curr_msec - pre_msec))
	{
		PEGA_DBG_L("[BATT] use cached rm, curr_msec: %lu, update_msec: %lu, pre_rm: %d.\n",
					curr_msec, pre_msec, pre_rm);
		return pre_rm;
	}
	else
	{
		PEGA_DBG_L("[BATT] read rm from gauge, curr_msec: %lu, update_msec: %lu, pre_rm: %d.\n",
					curr_msec, pre_msec, pre_rm);
	}
#endif /* CONFIG_BQ27541_CACHE && CONFIG_BQ27541_RETRY */

	udelay(66);
	ret = bq27541_read(BQ27541_REG_RM, &rm, 0, di);
	if (ret)
	{
		PEGA_DBG_H("[BATT_ERR] error(0x%08x) reading rm\n", ret);
#if (CONFIG_BQ27541_RETRY == 1)
		if (-1 == pre_rm)
			pre_rm = 0;
		return pre_rm;
#else  /* CONFIG_BQ27541_RETRY */
		return 0;
#endif /* CONFIG_BQ27541_RETRY */
	}

	PEGA_DBG_L("[BATT] %s, rm= %d\n", __func__, rm);

#if (CONFIG_BQ27541_RETRY == 0)
	return rm;
#else  /* CONFIG_BQ27541_RETRY */
	fcc = bq27541_battery_fcc(di);
	if (BATT_VALID_LOW_RM <= rm && rm <= fcc)
		goto done;
	else
		goto retry;

done:
	if (ABS(rm - pre_rm) <= BATT_RETRY_CAP_DIFF || -1 == pre_rm)
	{
		rm_retry_cnt = BATT_MAX_RETRY_TIMES;
	}

retry:
	if (BATT_MAX_RETRY_TIMES <= rm_retry_cnt)
	{
		PEGA_DBG_L("[BATT_RETRY] %s, max retry count, rm=%d, return.\n",
			__func__, rm);
		rm_retry_cnt = 0;

#if (CONFIG_BQ27541_CACHE == 1)
		bq27541_cached_update(BATTERY_CACHED_RM, rm);
		pre_rm = rm;
#else  /* CONFIG_BQ27541_CACHE */
		sspnd_rm = pre_rm = rm;
#endif /* CONFIG_BQ27541_CACHE */
	}
	else
	{
		PEGA_DBG_H("[BATT_RETRY] %s, retry=%d, rm=%d, return pre_rm=%d or sspnd_rm=%d\n",
			__func__, rm_retry_cnt, rm, pre_rm, sspnd_rm);
		rm_retry_cnt++;

		if (-1 == pre_rm)
			pre_rm = (-1 == sspnd_rm) ? 0 : sspnd_rm;

#if (CONFIG_BQ27541_CACHE == 1)
		bq27541_cached_reset_msec(BATTERY_CACHED_RM);
#endif /* CONFIG_BQ27541_CACHE */
	}
	return pre_rm;
#endif /* CONFIG_BQ27541_RETRY */
}

/*
 * Return the battery Relative State-of-Charge
 * Or < 0 if something fails.
 */
static int bq27541_battery_rsoc(struct bq27541_device_info *di)
{
	int ret = 0, rsoc = 0;
#if (CONFIG_BQ27541_RETRY == 1)
	static int rsoc_retry_cnt = 0;
#if (CONFIG_BQ27541_CACHE == 1)
	int pre_rsoc = batt_cached_info.cached_val[BATTERY_CACHED_RSOC];
	int sspnd_rsoc = batt_cached_info.sspnd_val[BATTERY_CACHED_RSOC];
	unsigned long curr_msec = 0;
	unsigned long pre_msec = batt_cached_info.update_msec[BATTERY_CACHED_RSOC];
#else  /* CONFIG_BQ27541_CACHE */
	static int sspnd_rsoc = -1;
#endif /* CONFIG_BQ27541_CACHE */
#endif /* CONFIG_BQ27541_RETRY */

	if (!di->is_gauge_ready || di->is_batt_dead)
		return 0;

#if (CONFIG_BQ27541_CACHE == 1 && CONFIG_BQ27541_RETRY == 1)
	curr_msec = jiffies_to_msecs(jiffies);

	if (0 < pre_msec &&
	    BQ27541_CACHED_UPDATE_INTERVAL > ABS(curr_msec - pre_msec))
	{
		PEGA_DBG_L("[BATT] use cached rsoc, curr_msec: %lu, update_msec: %lu, pre_rsoc: %d.\n",
					curr_msec, pre_msec, pre_rsoc);
		return pre_rsoc;
	}
	else
	{
		PEGA_DBG_L("[BATT] read rsoc from gauge, curr_msec: %lu, update_msec: %lu, pre_rsoc: %d.\n",
					curr_msec, pre_msec, pre_rsoc);
	}
#endif /* CONFIG_BQ27541_CACHE && CONFIG_BQ27541_RETRY */

	udelay(66);
	ret = bq27541_read(BQ27541_REG_SOC, &rsoc, 0, di);
	if (ret)
	{
		PEGA_DBG_H("[BATT_ERR] error(0x%08x) reading relative State-of-Charge\n", ret);
#if (CONFIG_BQ27541_RETRY == 1)
		if (-1 == pre_rsoc)
			pre_rsoc = (-1 != sspnd_rsoc) ? sspnd_rsoc : 0;
		return pre_rsoc;
#else  /* CONFIG_BQ27541_RETRY */
		return 0;
#endif /* CONFIG_BQ27541_RETRY */
	}

	PEGA_DBG_L("[BATT] %s, rsoc= %d\n", __func__, rsoc);

#if (CONFIG_BQ27541_RETRY == 0)
	return rsoc;
#else /* CONFIG_BQ27541_RETRY */
	if (BATT_VALID_LOW_RSOC <= rsoc && BATT_VALID_HIGH_RSOC >= rsoc)
		goto done;
	else
		goto retry;

done:
	if (ABS(rsoc - pre_rsoc) < BATT_RETRY_RSOC_DIFF || -1 == pre_rsoc)
	{
		rsoc_retry_cnt = BATT_MAX_RETRY_TIMES;
	}

retry:
	if (BATT_MAX_RETRY_TIMES <= rsoc_retry_cnt)
	{
		PEGA_DBG_L("[BATT_RETRY] %s, max retry count, rsoc=%d, return.\n",
			__func__, rsoc);
		rsoc_retry_cnt = 0;
#if (CONFIG_BQ27541_CACHE == 1)
		bq27541_cached_update(BATTERY_CACHED_RSOC, rsoc);
		pre_rsoc = rsoc;
#else  /* CONFIG_BQ27541_CACHE */
		sspnd_rsoc = pre_rsoc = rsoc;
#endif /* CONFIG_BQ27541_CACHE */
	}
	else
	{
		PEGA_DBG_H("[BATT_RETRY] %s, retry=%d, rsoc=%d, return pre_rsoc=%d or sspnd_rsoc=%d\n",
			__func__, rsoc_retry_cnt, rsoc, pre_rsoc, sspnd_rsoc);
		rsoc_retry_cnt++;

		if (-1 == pre_rsoc)
			pre_rsoc = (-1 == sspnd_rsoc) ? 0 : sspnd_rsoc;

#if (CONFIG_BQ27541_CACHE == 1)
		bq27541_cached_reset_msec(BATTERY_CACHED_RSOC);
#endif /* CONFIG_BQ27541_CACHE */
	}
	return pre_rsoc;
#endif /* CONFIG_BQ27541_RETRY */
}

/*
 * Return the battery level for UI
 * Or < 0 if something fails.
 */
static int bq27541_battery_level(struct bq27541_device_info *di)
{
	int ret = -1;
	int volt = -1, flags = -1, rm = -1, fcc = -1, level = -1, rsoc = -1, re_rsoc = -1, curr = -1, temp = -1;
#if (CONFIG_BQ27541_RETRY == 1)
	int fin = -1;
	static int level_retry_cnt = 0;
	int cached_level = 0;
#if (CONFIG_BQ27541_CACHE == 1)
	int pre_level = batt_cached_info.cached_val[BATTERY_CACHED_LEVEL];
	int sspnd_level = batt_cached_info.sspnd_val[BATTERY_CACHED_LEVEL];
#else  /* CONFIG_BQ27541_CACHE */
	static int sspnd_level = -1;
#endif /* CONFIG_BQ27541_CACHE */
#endif /* CONFIG_BQ27541_RETRY */

	if (!di->is_gauge_ready || di->is_batt_dead)
		return 0;

	udelay(66);
	curr = bq27541_battery_current(di);

	udelay(66);
	rsoc = bq27541_battery_rsoc(di);
	if (BATT_SHUTDOWN_LEVEL < rsoc && 100 >= rsoc)
	{
		re_rsoc = 99 - (int)(98 * (94 - rsoc) / (94 - BATT_REMAP_BASE_LEVEL));
		if (re_rsoc > 99)
			re_rsoc = 99;
		else if (re_rsoc < 1)
			re_rsoc = 1;
	}
	else
		re_rsoc = 0;
	PEGA_DBG_L("[BATT] %s, rsoc=%d, remap rsoc=%d.\n", __func__, rsoc, re_rsoc);

	udelay(66);
	volt = bq27541_battery_voltage(di);

	udelay(66);
	rm = bq27541_battery_rm(di);

	udelay(66);
	fcc = bq27541_battery_fcc(di);

	udelay(66);
	temp = bq27541_battery_temperature(di);

	// Check voltage only.
	if ((volt >= BATT_VALID_LOW_VOLT && volt <= BATT_SHUTDOWN_LOWVOLT) ||
	    (0 == rsoc))
	{
		level = 0;
#if (CONFIG_BQ27541_RETRY == 0)
		goto showlog;
#else  /* CONFIG_BQ27541_RETRY */
		if (-1 != pre_level && curr <= 0)
			fin = 0; //need retry;
		else
			fin = 1; //done;
		goto done;
#endif /* CONFIG_BQ27541_RETRY */
	}

	udelay(66);
	ret = bq27541_read(BQ27541_REG_FLAGS, &flags, 0, di);
	if (ret < 0)
	{
		PEGA_DBG_H("[BATT_ERR] error(0x%08x) reading flags\n", ret);
#if (CONFIG_BQ27541_RETRY == 0)
		goto showlog;
#else  /* CONFIG_BQ27541_RETRY */
		goto done;
#endif /* CONFIG_BQ27541_RETRY */
	}
	else
	{
		if (flags & BQ27541_FLAG_FC)
		{
			if (volt > 3900 &&
			    (rm > 0 && fcc > 0 && rm > fcc * 94 / 100 && rsoc > 94))
			{
				level = 100;
#if (CONFIG_BQ27541_RETRY == 0)
				goto showlog;
#else  /* CONFIG_BQ27541_RETRY */
				if (0 <= pre_level && 99 > pre_level)
					fin = 0; //need retry;
				else
					fin = 1; //done;
				goto done;
#endif /* CONFIG_BQ27541_RETRY */
			}
		}
	}

	if (fcc > 0 && rm > 0)
	{
		level = 99 - (int)(98 * (94 * fcc - 100 * rm) / ((94 - BATT_REMAP_BASE_LEVEL) * fcc));
		if (level > 99)
			level = 99;
		else if (level < 1)
		{
			if (1 < re_rsoc && 99 > re_rsoc)
				level = re_rsoc;
			else
				level = 1;
		}
		PEGA_DBG_L("[BATT] %s, UI level= %d\n", __func__, level);
	}
	else
		level = re_rsoc;

#if (CONFIG_BQ27541_RETRY == 0)
showlog:
#if (CONFIG_BQ27541_DUMMY_ENABLE == 1)
	if (1 == di->is_dummy_batt && BATT_DUMMY_LOWEST_LEVEL > level)
		level = BATT_DUMMY_LOWEST_LEVEL;
#endif /* CONFIG_BQ27541_DUMMY_ENABLE */

	{
		PEGA_DBG_H("[BATT] %s, volt=%d, curr=%d, rsoc=%d, re_rsoc=%d, rm=%d, fcc=%d, level=%d, temp=%d, flags=0x%04x.\n",
				__func__, volt, curr, rsoc, re_rsoc, rm, fcc, level, temp, flags);
	}

	return level;
#else  /* CONFIG_BQ27541_RETRY */
	if ((pre_level >= 0) &&
	    ((curr >= 0 && level < pre_level) ||
	     (curr <= 0 && level > pre_level)))
	{
		PEGA_DBG_L("[BATT] %s, curr= %d, UI level= %d, level_cache= %d is used.\n",
			__func__, curr, level, pre_level);
		level = pre_level;
	}
	else
	{
		PEGA_DBG_L("[BATT] %s, UI level= %d\n", __func__, level);
	}

	fin = 1;

done:
	{
		static int log_temp, log_volt, log_curr, log_rsoc, log_rm, log_fcc, log_level, log_flags;

		if (ABS(log_curr  - curr)  > 100 ||
		    ABS(log_volt  - volt)  > 50  ||
		    ABS(log_temp  - temp)  > 10  ||
		    ABS(log_rm    - rm)    > 100 ||
		    ABS(log_fcc   - fcc)   > 100 ||
		    log_rsoc  != rsoc            ||
		    log_level != level           ||
		    log_flags != flags)
		{
			log_curr  = curr;
			log_volt  = volt;
			log_temp  = temp;
			log_rsoc  = rsoc;
			log_rm    = rm;
			log_fcc   = fcc;
			log_level = level;
			log_flags = flags;
			PEGA_DBG_H("[BATT] %s, volt=%d, curr=%d, rsoc=%d, re_rsoc=%d, rm=%d, fcc=%d, level=%d, level_cache=%d, level_suspend=%d, temp=%d, flags=0x%04x.\n",
					__func__, volt, curr, rsoc, re_rsoc, rm, fcc, level, pre_level, sspnd_level, temp, flags);
		}
	}

	if (0 == fin && rsoc > BATT_SHUTDOWN_LEVEL)
		goto retry;

	if (-1 == pre_level ||
	    (ABS(level - pre_level) < BATT_RETRY_RSOC_DIFF &&
	     ABS(level - re_rsoc) <= BATT_RETRY_RSOC_DIFF) ||
	    (0 == level && ((0 < rm && 0 < fcc) || 0 < rsoc)))
	{
		level_retry_cnt = BATT_MAX_RETRY_TIMES;
	}

retry:
	cached_level = (-1 == pre_level) ? sspnd_level : pre_level;
	if (0 < cached_level &&
	    ABS(level - cached_level) > BATT_RETRY_RSOC_DIFF &&
	    ABS(level - cached_level) > ABS(re_rsoc - cached_level))
	{
		PEGA_DBG_H("[BATT_RETRY] %s, incorrect level=%d found, assigned to %d.\n",
				__func__, level, re_rsoc);
		level = re_rsoc;
	}

	if (BATT_MAX_RETRY_TIMES <= level_retry_cnt)
	{
		if (0 == level)
		{
			if (1 < re_rsoc && volt > BATT_SHUTDOWN_LOWVOLT)
			{
				level = re_rsoc;
				level_retry_cnt = 0;
			}
			else
				level_retry_cnt = BATT_MAX_RETRY_TIMES;
		}
		else if (100 == level && 99 <= re_rsoc)
			level_retry_cnt = BATT_MAX_RETRY_TIMES;
		else
			level_retry_cnt = 0;

#if (CONFIG_BQ27541_CACHE == 1)
		PEGA_DBG_L("[BATT] %s, retry=%d, level=%d.\n",
			__func__, level_retry_cnt, level);
		bq27541_cached_update(BATTERY_CACHED_LEVEL, level);
		pre_level = bq27541_avg_level(level);
#else  /* CONFIG_BQ27541_CACHE */
		sspnd_level = pre_level = bq27541_avg_level(level);
#endif /* CONFIG_BQ27541_CACHE */
	}
	else //if (level_retry_cnt < BATT_MAX_RETRY_TIMES)
	{
		PEGA_DBG_H("[BATT_RETRY] %s, retry=%d, level=%d, return pre_level=%d or sspnd_level=%d\n",
			__func__, level_retry_cnt, level, pre_level, sspnd_level);
		level_retry_cnt++;

		if (-1 == pre_level)
			pre_level = (-1 == sspnd_level) ? re_rsoc : sspnd_level;

#if (CONFIG_BQ27541_CACHE == 1)
		bq27541_cached_reset(NULL);
#endif /* CONFIG_BQ27541_CACHE */
	}
	PEGA_DBG_L("[BATT] %s, return UI level= %d\n", __func__, pre_level);

	if (-1 == pre_level)
		pre_level = 0;

#if (CONFIG_BQ27541_DUMMY_ENABLE == 1)
	if (1 == di->is_dummy_batt && BATT_DUMMY_LOWEST_LEVEL > pre_level)
		pre_level = BATT_DUMMY_LOWEST_LEVEL;
#endif /* CONFIG_BQ27541_DUMMY_ENABLE */

	return pre_level;
#endif /* CONFIG_BQ27541_RETRY */
}

/*
 * Return the battery charging status
 * Or < 0 if something fails.
 */
static int bq27541_battery_status(struct bq27541_device_info *di)
{
	int status = POWER_SUPPLY_STATUS_UNKNOWN;
	int ret;

	if (!di->is_gauge_ready || di->is_batt_dead)
		return POWER_SUPPLY_STATUS_UNKNOWN;

	ret = bq27541_battery_level(di);
	if (ret >= BATT_FULL_CAPACITY_VAL)
		status = POWER_SUPPLY_STATUS_FULL;

	PEGA_DBG_L("[BATT] %s, status= %d\n", __func__, status);
	return status;
}

/*
 * Return the battery health status
 * Or < 0 if something fails.
 */
static int bq27541_battery_health(struct bq27541_device_info *di)
{
	int ret;

	if (!di->is_gauge_ready)
		return POWER_SUPPLY_HEALTH_UNKNOWN;

	if (di->is_batt_dead)
		return POWER_SUPPLY_HEALTH_DEAD;

	ret = bq27541_battery_temperature(di);
	if (ret <= BATT_HEALTH_COLD)
		return POWER_SUPPLY_HEALTH_COLD;
	else if (ret >= BATT_HEALTH_HEAT)
		return POWER_SUPPLY_HEALTH_OVERHEAT;

	ret = bq27541_battery_voltage(di);
	if (ret >= BATT_INTERRUPT_OVERVOLT)
		return POWER_SUPPLY_HEALTH_OVERVOLTAGE;

	return POWER_SUPPLY_HEALTH_GOOD;
}

/* Write control subcommand */
static void bq27541_cntl_cmd(struct bq27541_device_info *di,
				int subcmd)
{
	bq27541_i2c_txsubcmd(BQ27541_REG_CNTL, subcmd, di);
}

/*
 * i2c specific code
 */
static int bq27541_i2c_txsubcmd(u8 reg, unsigned short subcmd,
		struct bq27541_device_info *di)
{
	struct i2c_msg msg;
	unsigned char data[3];
	int ret;

	if (!di->client)
		return -ENODEV;

	memset(data, 0, sizeof(data));
	data[0] = reg;
	data[1] = subcmd & 0x00FF;
	data[2] = (subcmd & 0xFF00) >> 8;

	msg.addr = di->client->addr;
	msg.flags = 0; /*Write*/
	msg.len = 3;
	msg.buf = data;

	pega_power_enter_cs();
	ret = i2c_transfer(di->client->adapter, &msg, 1);
	pega_power_exit_cs();

	if (ret < 0)
	{
		PEGA_DBG_H("[BATT_ERR] %s, error(0x%08x) reading subcmd= %d\n",
			__func__, ret, subcmd);
		return -EIO; // fffffffb
	}

	return 0;
}

/* get the config of gauge chip */
static int bq27541_chip_config(struct bq27541_device_info *di)
{
	int flags = 0, ret = 0;

	bq27541_cntl_cmd(di, BQ27541_SUBCMD_CNTL_STATUS);
	udelay(66);
	ret = bq27541_read(BQ27541_REG_CNTL, &flags, 0, di);
	if (ret < 0)
	{
		PEGA_DBG_H("[BATT_ERR] error(0x%08x) reading register %02x\n",
			 ret, BQ27541_REG_CNTL);
		return ret;
	}
	udelay(66);

	return 0;
}

static int bq27541_get_battery_mvolts(void)
{
	return bq27541_battery_voltage(bq27541_di);
}

static int bq27541_get_battery_curr(void)
{
	return bq27541_battery_current(bq27541_di);
}

static int bq27541_get_battery_temperature(void)
{
	return bq27541_battery_temperature(bq27541_di);
}

static int bq27541_get_battery_status(void)
{
	return bq27541_battery_status(bq27541_di);
}

static int bq27541_get_battery_capacity(void)
{
	return bq27541_battery_rsoc(bq27541_di);
}

static int bq27541_get_battery_health(void)
{
	return bq27541_battery_health(bq27541_di);
}

static char* bq27541_get_battery_fw_ver(void)
{
	return bq27541_battery_fw_version(bq27541_di);
}

static int bq27541_get_battery_level(void)
{
	return bq27541_battery_level(bq27541_di);
}

static int bq27541_get_battery_rm(void)
{
	return bq27541_battery_rm(bq27541_di);
}

static int bq27541_get_battery_fcc(void)
{
	return bq27541_battery_fcc(bq27541_di);
}

static int bq27541_is_battery_present(void)
{
#if 0
	int ret = bq27541_battery_voltage(bq27541_di);

	return (ret >= BATT_SHUTDOWN_LOWVOLT) ? 1 : 0;
#else
	return 1;
#endif
}

// this interface is used for charger.
static int bq27541_is_battery_temp_within_range(void)
{
#if 0
	int ret = bq27541_battery_temperature(bq27541_di);

	return (ret > 0 && ret < BATT_INTERRUPT_OTC) ? 1 : 0;
#else
	return 1;
#endif
}

static int bq27541_is_battery_id_valid(void)
{
#if 0
	int type = 0, ret = 0;

	if (bq27541_is_batt_dead)
		return 0;
	else
		return 1;

	/*bq27541_cntl_cmd(bq27541_di, BQ27541_SUBCMD_DEVICE_TYPE);
	udelay(66);
	ret = bq27541_read(BQ27541_REG_CNTL, &type, 0, bq27541_di);

	if (0 == ret && 0x0541 == type)
	{
		PEGA_DBG_L("[BATT] %s, true\n", __func__);
		return 1;
	}
	else
	{
		PEGA_DBG_L("[BATT] %s, false, type=0x%04x, ret=0x%08x.\n",
			__func__, type, ret);
		return 0;
	}*/
#else
	return 1;
#endif
}

static int bq27541_is_battery_gauge_ready(void)
{
	PEGA_DBG_L("%s: %d\n", __func__, bq27541_di->is_gauge_ready);
	return bq27541_di->is_gauge_ready;
}

static struct msm_battery_gauge bq27541_batt_gauge = {
	.get_battery_mvolts       = bq27541_get_battery_mvolts,
	.get_battery_temperature  = bq27541_get_battery_temperature,
	.is_battery_present       = bq27541_is_battery_present,
	.is_battery_temp_within_range  = bq27541_is_battery_temp_within_range,
	.is_battery_id_valid      = bq27541_is_battery_id_valid,
	.is_battery_gauge_ready   = bq27541_is_battery_gauge_ready,
	.get_battery_status       = bq27541_get_battery_status,
	.get_batt_remaining_capacity   = bq27541_get_battery_capacity,
	.get_battery_health       = bq27541_get_battery_health,
	.get_battery_current      = bq27541_get_battery_curr,
	.get_battery_fw_ver       = bq27541_get_battery_fw_ver,
	.get_battery_level        = bq27541_get_battery_level,
	.get_battery_rm           = bq27541_get_battery_rm,
	.get_battery_fcc          = bq27541_get_battery_fcc,
};

static int bq27541_read_i2c(u8 reg, int *rt_value, int b_single,
			struct bq27541_device_info *di)
{
	struct i2c_client *client = di->client;
	struct i2c_msg msg[1];
	unsigned char data[2];
	int err;

	if (!client->adapter)
		return -ENODEV;

	msg->addr = client->addr;
	msg->flags = 0;
	msg->len = 1;
	msg->buf = data;

	data[0] = reg;
	pega_power_enter_cs();
	err = i2c_transfer(client->adapter, msg, 1);
	pega_power_exit_cs();

	if (err >= 0)
	{
		if (!b_single)
			msg->len = 2;
		else
			msg->len = 1;

		msg->flags = I2C_M_RD;
		pega_power_enter_cs();
		err = i2c_transfer(client->adapter, msg, 1);
		pega_power_exit_cs();
		if (err >= 0)
		{
			if (!b_single)
				*rt_value = get_unaligned_le16(data);
			else
				*rt_value = data[0];

			return 0;
		}
	}
	return err;
}

#if (CONFIG_BQ27541_INTERRUPT_ENABLE == 1)
static irqreturn_t irq_handler(int irq, void *dev_id)
{
	int irq_value;
	PEGA_DBG_L("[BATT_IRQ] %s, enter.\n ", __func__);
	irq_value = gpio_get_value(BATT_INT_GPIO);
	if (0 == irq_value)
	{
		PEGA_DBG_L("[BATT_IRQ] %s, irq_value is low, start handle irq.\n ", __func__);
		irq_set_irq_type(gauge_irq.irq_num, IRQF_TRIGGER_HIGH);
		schedule_delayed_work(&gauge_irq.work, 0.5 * (HZ));
		wake_lock(&gauge_irq.wl);
		gauge_irq.irq_on = 1;
	}
	else
	{
		PEGA_DBG_L("[BATT_IRQ] %s, irq_value is high, set the trigger type as low.\n ", __func__);
		irq_set_irq_type(gauge_irq.irq_num, IRQF_TRIGGER_LOW);
		schedule_delayed_work(&gauge_irq.work, 0);
		wake_unlock(&gauge_irq.wl);
		gauge_irq.irq_on = 0;
	}
	PEGA_DBG_L("[BATT_IRQ] %s, irq_value= 0x%08x.\n ", __func__, irq_value);
	PEGA_DBG_L("[BATT_IRQ] %s, exit.\n ", __func__);

	return IRQ_HANDLED;
}

static void irq_handler_bottom(struct work_struct *work)
{
	//int irq_value;
	//int temp, volt, rsoc, level;
	/*static int bq27541_stop_chg_condition;*/

	PEGA_DBG_L("[BATT_IRQ] %s, enter.\n ", __func__);
	/*
	temp = bq27541_battery_temperature(bq27541_di);
	volt = bq27541_battery_voltage(bq27541_di);
	rsoc = bq27541_battery_rsoc(bq27541_di);
	level = bq27541_battery_level(bq27541_di);
	PEGA_DBG_H("[BATT_IRQ] %s, temp=%d volt=%d rsoc=%d level=%d irq=%d.\n ",
			__func__, temp, volt, rsoc, level, gauge_irq.irq_on);*/

	msm_charger_notify_event(NULL, CHG_BATT_STATUS_CHANGE);

	PEGA_DBG_L("[BATT_IRQ] %s, exit.\n ", __func__);
}
#endif /* CONFIG_BQ27541_INTERRUPT_ENABLE */

static void bq27541_hw_config(struct work_struct *work)
{
	int ret = 0, flags = 0, type = -1, fw_ver = -1, df_ver = -1;
	struct bq27541_device_info *di;
	static int retry_cnt = 0;

	PEGA_DBG_L("[BATT] %s, start\n", __func__);

	PEGA_DBG_L("[BATT] %s, msm_battery_gauge_register\n", __func__);
	/* register batt_gauge function pointer to msm_charger.c */
	msm_battery_gauge_register(&bq27541_batt_gauge);

	di  = container_of(work, struct bq27541_device_info, hw_config.work);
	PEGA_DBG_L("[BATT] %s, bq27541_chip_config\n", __func__);
	ret = bq27541_chip_config(di);
	if (ret)
	{
		if (retry_cnt < 3) // try 3 times
		{
			PEGA_DBG_H("[BATT_ERR] error(0x%08x) failed to config bq27541, retry: %d.\n",
					ret, retry_cnt);
			schedule_delayed_work(&di->hw_config, BQ27541_INIT_DELAY * 10);
			retry_cnt++;
			return;
		}
		else
		{
			bq27541_di->is_batt_dead = 1;
#ifdef PEGA_CHG_FAKE_BATT_ENABLE
			PEGA_DBG_H("[BATT_ERR] failed to config bq27541\nBQ27541_FAKE is enabled.\n");
#else  /* PEGA_CHG_FAKE_BATT_ENABLE */
			bq27541_di->is_gauge_ready = 1;
			PEGA_DBG_H("[BATT_ERR] failed to config bq27541\nThe battery might be depleted\n");
#endif  /* PEGA_CHG_FAKE_BATT_ENABLE */
			return;
		}
	}

	PEGA_DBG_L("[BATT] %s, config\n", __func__);
	udelay(66);
	bq27541_cntl_cmd(di, BQ27541_SUBCMD_CNTL_STATUS);
	udelay(66);
	bq27541_read(BQ27541_REG_CNTL, &flags, 0, di);
	udelay(66);
	bq27541_cntl_cmd(di, BQ27541_SUBCMD_DEVICE_TYPE);
	udelay(66);
	bq27541_read(BQ27541_REG_CNTL, &type, 0, di);
	udelay(66);
	bq27541_cntl_cmd(di, BQ27541_SUBCMD_FW_VER);
	udelay(66);
	bq27541_read(BQ27541_REG_CNTL, &fw_ver, 0, di);
	udelay(66);
	bq27541_cntl_cmd(di, BQ27541_SUBCMD_DF_VER);
	udelay(66);
	bq27541_read(BQ27541_REG_CNTL, &df_ver, 0, di);

	dev_info(di->dev, "DEVICE_TYPE is 0x%04X, FIRMWARE_VERSION is 0x%04X, DATA FLASH VERSION is 0x%04x.\n",
			type, fw_ver, df_ver);
	dev_info(di->dev, "Complete bq27541 configuration 0x%04X.\n", flags);

	di->fw_ver = (unsigned short)fw_ver;
	di->df_ver = (unsigned short)df_ver;
	di->conf   = (unsigned short)flags;

#if (CONFIG_BQ27541_DUMMY_ENABLE == 1)
	if (0 == df_ver)
		di->is_dummy_batt = 1;
	else
		di->is_dummy_batt = 0;

#endif /* CONFIG_BQ27541_DUMMY_ENABLE */
#if 0
	{
		int hw_ver = 0, c;
		char dev_name[8];

		udelay(66);
		bq27541_cntl_cmd(di, BQ27541_SUBCMD_HW_VER);
		udelay(66);
		bq27541_read(BQ27541_REG_CNTL, &hw_ver, 0, di);

		udelay(66);
		bq27541_read(0x63, &c, 0, di);
		dev_name[0] = (char)(c & 0x00ff);
		udelay(66);
		bq27541_read(0x64, &c, 0, di);
		dev_name[1] = (char)(c & 0x00ff);
		udelay(66);
		bq27541_read(0x65, &c, 0, di);
		dev_name[2] = (char)(c & 0x00ff);
		udelay(66);
		bq27541_read(0x66, &c, 0, di);
		dev_name[3] = (char)(c & 0x00ff);
		udelay(66);
		bq27541_read(0x67, &c, 0, di);
		dev_name[4] = (char)(c & 0x00ff);
		udelay(66);
		bq27541_read(0x68, &c, 0, di);
		dev_name[5] = (char)(c & 0x00ff);
		udelay(66);
		bq27541_read(0x69, &c, 0, di);
		dev_name[6] = (char)(c & 0x00ff);
		dev_name[7] = '\0';

		PEGA_DBG_H("[BATT_TEST] HARDWARE VERSION is 0x%04X.\n", hw_ver);
		PEGA_DBG_H("[BATT_TEST] DATA FLASH VERSION is 0x%04X.\n", df_ver);
		PEGA_DBG_H("[BATT_TEST] DEVICE NAME is %s\n", dev_name);
	}

	udelay(66);
	bq27541_cntl_cmd(di, BQ27541_SUBCMD_HW_VER);
	udelay(66);
	ret = bq27541_read(BQ27541_REG_CNTL, &flags, 0, di);
	PEGA_DBG_H("[BATT_TEST] HW Ver= 0x%04x.\n", flags);

	udelay(66);
	bq27541_cntl_cmd(di, BQ27541_SUBCMD_CHEM_ID);
	udelay(66);
	ret = bq27541_read(BQ27541_REG_CNTL, &flags, 0, di);
	PEGA_DBG_H("[BATT_TEST] CHEM ID= 0x%04x.\n", flags);

	udelay(66);
	ret = bq27541_read(BQ27541_REG_DCAP, &flags, 0, di);
	PEGA_DBG_H("[BATT_TEST] DCAP= %d.\n", flags);

	udelay(66);
	ret = bq27541_read(BQ27541_REG_NAC, &flags, 0, di);
	PEGA_DBG_H("[BATT_TEST] NAC= %d.\n", flags);

	udelay(66);
	ret = bq27541_read(BQ27541_REG_FAC, &flags, 0, di);
	PEGA_DBG_H("[BATT_TEST] FAC= %d.\n", flags);

	udelay(66);
	ret = bq27541_read(BQ27541_REG_FCC, &flags, 0, di);
	PEGA_DBG_H("[BATT_TEST] FCC= %d.\n", flags);

	udelay(66);
	ret = bq27541_read(BQ27541_REG_RM, &flags, 0, di);
	PEGA_DBG_H("[BATT_TEST] RM= %d.\n", flags);

	udelay(66);
	ret = bq27541_read(BQ27541_REG_AE, &flags, 0, di);
	PEGA_DBG_H("[BATT_TEST] AE= %d.\n", flags);

	udelay(66);
	ret = bq27541_read(BQ27541_REG_AP, &flags, 0, di);
	PEGA_DBG_H("[BATT_TEST] AP= %d.\n", flags);

	udelay(66);
	ret = bq27541_read(BQ27541_REG_SOH, &flags, 0, di);
	PEGA_DBG_H("[BATT_TEST] SOH= %d.\n", flags);
#endif

#if (CONFIG_BQ27541_INTERRUPT_ENABLE == 1)
	// init interrupt
	INIT_DELAYED_WORK(&gauge_irq.work, irq_handler_bottom);
	PEGA_DBG_L("[BATT] %s, gauge_irq.work= 0x%08x.\n ",
		__func__, (unsigned int)(&gauge_irq.work));

	wake_lock_init(&gauge_irq.wl, WAKE_LOCK_SUSPEND, "gauge_irq_wl");

	ret = gpio_request(BATT_INT_GPIO, "gauge_interrupt");
	if (ret < 0)
		goto batt_failed_1;

	ret = gpio_direction_input(BATT_INT_GPIO);
	if (ret < 0)
		goto batt_failed_2;

	gauge_irq.irq_num = gpio_to_irq(BATT_INT_GPIO);
	if (gauge_irq.irq_num < 0)
	{
		ret = gauge_irq.irq_num;
		goto batt_failed_2;
	}

	ret = request_irq(gauge_irq.irq_num, irq_handler,
			  IRQF_TRIGGER_LOW, "gauge_interrupt", NULL);
	if (ret < 0)
		goto batt_failed_2;

	ret = irq_set_irq_wake(gauge_irq.irq_num, 1);
	if (ret < 0)
		goto batt_failed_3;
#endif /* CONFIG_BQ27541_INTERRUPT_ENABLE */

	bq27541_di->is_gauge_ready = 1;

	return;

#if (CONFIG_BQ27541_INTERRUPT_ENABLE == 1)
batt_failed_3:
	free_irq(gauge_irq.irq_num, 0);
batt_failed_2:
	gpio_free(BATT_INT_GPIO);
batt_failed_1:
	wake_lock_destroy(&gauge_irq.wl);
	cancel_delayed_work(&gauge_irq.work);
	cancel_delayed_work(&di->hw_config);
#endif /* CONFIG_BQ27541_INTERRUPT_ENABLE */
}

static int bq27541_battery_probe(struct i2c_client *client,
				 const struct i2c_device_id *id)
{
	/*char *name;*/
	struct bq27541_device_info *di;
	struct bq27541_access_methods *bus;
	int num;
	int retval = 0;

	PEGA_DBG_L("[BATT] %s, start\n", __func__);

	PEGA_DBG_L("[BATT] %s, i2c_check_functionality\n", __func__);
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
		return -ENODEV;

	PEGA_DBG_L("[BATT] %s, idr_pre_get\n", __func__);
	/* Get new ID for the new battery device */
	retval = idr_pre_get(&battery_id, GFP_KERNEL);
	if (retval == 0)
		return -ENOMEM;
	PEGA_DBG_L("[BATT] %s, idr_get_new\n", __func__);
	mutex_lock(&battery_mutex);
	retval = idr_get_new(&battery_id, client, &num);
	mutex_unlock(&battery_mutex);
	if (retval < 0)
		return retval;

	/*name = kasprintf(GFP_KERNEL, "%s-%d", id->name, num);
	if (!name)
	{
		PEGA_DBG_H("[BATT_ERR] failed to allocate device name\n");
		retval = -ENOMEM;
		goto batt_failed_1;
	}*/

	PEGA_DBG_L("[BATT] %s, di = kzalloc\n", __func__);
	di = kzalloc(sizeof(*di), GFP_KERNEL);
	if (!di)
	{
		PEGA_DBG_H("[BATT_ERR] failed to allocate device info data\n");
		retval = -ENOMEM;
		goto batt_failed_2;
	}
	di->id = num;

	PEGA_DBG_L("[BATT] %s, bus = kzalloc\n", __func__);
	bus = kzalloc(sizeof(*bus), GFP_KERNEL);
	if (!bus)
	{
		PEGA_DBG_H("[BATT_ERR] failed to allocate access method data\n");
		retval = -ENOMEM;
		goto batt_failed_3;
	}

	i2c_set_clientdata(client, di);
	di->dev = &client->dev;
	bus->read = &bq27541_read_i2c;
	di->bus = bus;
	di->client = client;
	di->fw_ver = -1;
	di->df_ver = -1;
	di->is_gauge_ready = 0;
	di->is_batt_dead = 0;
#if (CONFIG_BQ27541_DUMMY_ENABLE == 1)
	di->is_dummy_batt = 0;
#endif /* CONFIG_BQ27541_DUMMY_ENABLE */

#if (CONFIG_BQ27541_CONTROL_REGULATOR == 1)
	gauge_reg_l11 = regulator_get(di->dev, "gauge_vdd");
	PEGA_DBG_L("[BATT] %s, gauge_reg_l11: %ld, dev: %ld.\n",
			__func__, (long)gauge_reg_l11, (long)di->dev);
	if (IS_ERR(gauge_reg_l11)) {
		PEGA_DBG_H("[BATT_ERR] could not get 8921_l11, retval = %ld\n",
			PTR_ERR(gauge_reg_l11));
		goto batt_failed_4;
	}

	retval = regulator_set_voltage(gauge_reg_l11, 3300000, 3300000);
	PEGA_DBG_L("[BATT] %s, regulator_set_voltage: %d\n", __func__, retval);
	if (retval) {
		PEGA_DBG_H("[BATT_ERR] set_voltage l11 failed, retval=%d\n", retval);
		goto batt_failed_4;
	}

	retval = regulator_enable(gauge_reg_l11);
	PEGA_DBG_L("[BATT] %s, regulator_enable: %d\n", __func__, retval);
	if (retval) {
		PEGA_DBG_H("[BATT_ERR] enable l11 failed, retval=%d\n", retval);
		goto batt_failed_4;
	}
#endif /* CONFIG_BQ27541_CONTROL_REGULATOR */

	/*if (retval)
	{
		PEGA_DBG_H("[BATT_ERR] failed to setup bq27541\n");
		goto batt_failed_4;
	}

	if (retval)
	{
		PEGA_DBG_H("[BATT_ERR] failed to powerup bq27541\n");
		goto batt_failed_4;
	}*/

	bq27541_di = di;

#if (CONFIG_BQ27541_RETRY == 1)
#if (CONFIG_BQ27541_CACHE == 1)
	bq27541_cached_init();
#else  /* CONFIG_BQ27541_CACHE */
	pre_level = -1;
	pre_rsoc = -1;
#endif /* CONFIG_BQ27541_CACHE */
#endif /* CONFIG_BQ27541_RETRY */

	/* register batt_gauge function pointer to msm_charger.c */
	/*msm_battery_gauge_register(&bq27541_batt_gauge);*/

	INIT_DELAYED_WORK(&di->hw_config, bq27541_hw_config);
	schedule_delayed_work(&di->hw_config, BQ27541_INIT_DELAY);

#if (CONFIG_BQ27541_INTERRUPT_ENABLE == 1)
	/* configure the GPIO to INPUT/NO PULL */
	/*writel(0x00000000, GPIO_CONFIG(BATT_INT_GPIO));*/
	/* config the GPIO to PULL HIGH */
	writel(0x03, GPIO_CONFIG(BATT_INT_GPIO));
#endif /* CONFIG_BQ27541_INTERRUPT_ENABLE */

	PEGA_DBG_H("[BATT_CONF]");

#if (CONFIG_BQ27541_RETRY == 1)
	PEGA_DBG_H(" ENABLE RETRY,");
#endif /* CONFIG_BQ27541_RETRY */

#if (CONFIG_BQ27541_CACHE == 1)
	PEGA_DBG_H(" ENABLE CACHE,");
#endif /* CONFIG_BQ27541_RETRY */

#if (CONFIG_BQ27541_DUMMY_ENABLE == 1)
	PEGA_DBG_H(" SUPPORT DUMMY BATTERY,");
#endif /* CONFIG_BQ27541_DUMMY_ENABLE */

#if (CONFIG_BQ27541_INTERRUPT_ENABLE == 1)
	PEGA_DBG_H(" ENABLE INTERRUPT");
#endif /* CONFIG_BQ27541_INTERRUPT_ENABLE */

	PEGA_DBG_H(".\n");

	return 0;

#if (CONFIG_BQ27541_CONTROL_REGULATOR == 1)
batt_failed_4:
	kfree(bus);
#endif /* CONFIG_BQ27541_CONTROL_REGULATOR */
batt_failed_3:
	kfree(di);
batt_failed_2:
	/*kfree(name);*/
/*batt_failed_1:*/
	mutex_lock(&battery_mutex);
	idr_remove(&battery_id, num);
	mutex_unlock(&battery_mutex);

	return retval;
}

static int bq27541_battery_remove(struct i2c_client *client)
{
	struct bq27541_device_info *di = i2c_get_clientdata(client);

	msm_battery_gauge_unregister(&bq27541_batt_gauge);
	//bq27541_cntl_cmd(di, BQ27541_SUBCMD_DISABLE_DLOG);
	//udelay(66);
	//bq27541_cntl_cmd(di, BQ27541_SUBCMD_DISABLE_IT);
	cancel_delayed_work(&di->hw_config);
	kfree(di->bus);

	mutex_lock(&battery_mutex);
	idr_remove(&battery_id, di->id);
	mutex_unlock(&battery_mutex);

	kfree(di);
#if (CONFIG_BQ27541_INTERRUPT_ENABLE == 1)
	wake_lock_destroy(&gauge_irq.wl);
	cancel_delayed_work(&gauge_irq.work);
	free_irq(gauge_irq.irq_num, 0);
	gpio_free(BATT_INT_GPIO);
#endif /* CONFIG_BQ27541_INTERRUPT_ENABLE */

#if (CONFIG_BQ27541_CACHE == 1 && CONFIG_BQ27541_RETRY == 1)
	batt_cached_info.reset_delay_on = 0;
	cancel_delayed_work(&batt_cached_info.reset_delay);
#endif /* CONFIG_BQ27541_CACHE && CONFIG_BQ27541_RETRY*/
	return 0;
}

static int bq27541_battery_suspend(struct device *dev)
{
#if (CONFIG_BQ27541_CONTROL_REGULATOR == 1)
	int rc;

	PEGA_DBG_L("[BATT] %s, disable regulator L11.\n", __func__);
	rc = regulator_disable(gauge_reg_l11);
	if (rc) {
		PEGA_DBG_H("[BATT_ERR] %s, disable gauge_reg_l11 failed, rc=%d\n",
			__func__, rc);
		return -ENODEV;
	}
#endif /* CONFIG_BQ27541_CONTROL_REGULATOR */

#if (CONFIG_BQ27541_RETRY == 1)
#if (CONFIG_BQ27541_CACHE == 1)
	bq27541_cached_suspend();
#else /* CONFIG_BQ27541_CACHE */
	pre_volt = pre_temp = pre_rsoc = pre_level = -1;
#endif /* CONFIG_BQ27541_CACHE */
#endif /* CONFIG_BQ27541_RETRY */
	return 0;
}

static int bq27541_battery_resume(struct device *dev)
{
#if (CONFIG_BQ27541_CONTROL_REGULATOR == 1)
	int rc;

	PEGA_DBG_L("[BATT] %s, enable regulator L11.\n", __func__);
	rc = regulator_enable(gauge_reg_l11);
	if (rc) {
		PEGA_DBG_H("[BATT_ERR] fail to enable L11, rc=%d\n", rc);
		return -ENODEV;
	}
#endif /* CONFIG_BQ27541_CONTROL_REGULATOR */

#if (CONFIG_BQ27541_RETRY == 1 && CONFIG_BQ27541_CACHE == 1)
		PEGA_DBG_L("[BATT] %s, cached data before suspend, volt=%d, temp=%d, rsoc=%d, level=%d\n",
				__func__,
				batt_cached_info.sspnd_val[BATTERY_CACHED_VOLT],
				batt_cached_info.sspnd_val[BATTERY_CACHED_TEMP],
				batt_cached_info.sspnd_val[BATTERY_CACHED_RSOC],
				batt_cached_info.sspnd_val[BATTERY_CACHED_LEVEL] );
#endif /* CONFIG_BQ27541_RETRY && CONFIG_BQ27541_CACHE */

#if (CONFIG_BQ27541_INTERRUPT_ENABLE == 1)
	schedule_delayed_work(&gauge_irq.work, 0.1 * (HZ));
#else  /* CONFIG_BQ27541_INTERRUPT_ENABLE */
	msleep(100);
	msm_charger_notify_event(NULL, CHG_BATT_STATUS_CHANGE);
#endif /* CONFIG_BQ27541_INTERRUPT_ENABLE */
	return 0;
}

static const struct dev_pm_ops bq27541_pm_ops = {
	.suspend = bq27541_battery_suspend,
	.resume = bq27541_battery_resume,
};

static const struct i2c_device_id bq27541_id[] = {
	{ "bq27541", 1 },
	{},
};
MODULE_DEVICE_TABLE(i2c, BQ27541_id);

static struct i2c_driver bq27541_battery_driver = {
	.driver		= {
			.name = "bq27541-battery",
			.pm = &bq27541_pm_ops,
	},
	.probe		= bq27541_battery_probe,
	.remove		= bq27541_battery_remove,
	.id_table	= bq27541_id,
};

static int __init bq27541_battery_init(void)
{
	int ret;

	ret = i2c_add_driver(&bq27541_battery_driver);
	if (ret)
		PEGA_DBG_H("[BATT_ERR] Unable to register BQ27541 driver\n");

	return ret;
}
module_init(bq27541_battery_init);

static void __exit bq27541_battery_exit(void)
{
	i2c_del_driver(&bq27541_battery_driver);
}
module_exit(bq27541_battery_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Qualcomm Innovation Center, Inc.");
MODULE_DESCRIPTION("BQ27541 battery monitor driver");
