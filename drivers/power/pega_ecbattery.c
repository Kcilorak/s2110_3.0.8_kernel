
#include <linux/power_supply.h>
#include <linux/err.h>
#include <linux/msm-charger.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/nuvec_api.h>
#include <linux/msm-charger-log.h>

/*
 * 1.1.0: Initial version
 * 1.4.0: Add CONFIG_ECBATT_RETRY
 * 1.5.0: Add CONFIG_ECBATT_CACHE
 * 1.6.0: Add CONFIG_ECBATT_DUMMY_ENABLE
 * 1.7.0: Add CONFIG_ECBATT_GET_BATDATA to get battdata from EC
 */

#define DRIVER_VERSION          "1.6.0"
/* data commands */
//#define ECBATT_REG_AR            AT_RATE
//#define ECBATT_REG_ARTTE         AT_RATE_TTE
#define ECBATT_REG_TEMP          DOCK_BAT_TEMP
#define ECBATT_REG_VOLT          DOCK_BAT_VOLT
#define ECBATT_REG_FLAGS         DOCK_BAT_FLAGS
//#define ECBATT_REG_NAC           NOMINAL_AVAIL_CAP
//#define ECBATT_REG_FAC           FULL_AVAIL_CAP
#define ECBATT_REG_RM            REMANING_CAP
#define ECBATT_REG_FCC           FULL_CHG_CAP
#define ECBATT_REG_AI            AVG_CURR
//#define ECBATT_REG_TTE           TTE
//#define ECBATT_REG_TTF           TTF
//#define ECBATT_REG_SI            STANDBY_CURR
//#define ECBATT_REG_STTE          STANDBY_TTE
//#define ECBATT_REG_MLI           MAX_LD_CURR
//#define ECBATT_REG_MLTTE         MAX_LD_TTE
//#define ECBATT_REG_AE            AVAIL_ENG
//#define ECBATT_REG_AP            AVG_POWER
//#define ECBATT_REG_TTECP         TTE_CONST_POWER
//#define ECBATT_REG_INTTEMP       0x28
//#define ECBATT_REG_CC            CYCLE_COUNT
#define ECBATT_REG_SOC           REL_STATE_OF_CHG
//#define ECBATT_REG_SOH           0x2e
//#define ECBATT_REG_ICR           0x30
//#define ECBATT_REG_LOGIDX        0x32
//#define ECBATT_REG_LOGBUF        0x34
/* Extended Command */
//#define ECBATT_REG_DCAP          0x3C
#define ECBATT_REG_DEVICE_TYPE   DEV_ID
#define ECBATT_REG_FW_VER        FW_ID
#define ECBATT_REG_HW_VER        HW_ID
#define ECBATT_REG_DF_VER        DF_ID

/* Flags of ECBATT_REG_FLAGS */
#define ECBATT_FLAG_DSC          BIT(0)
#define ECBATT_FLAG_SOCF         BIT(1)
#define ECBATT_FLAG_SOC1         BIT(2)
#define ECBATT_FLAG_HW0          BIT(3)
#define ECBATT_FLAG_HW1          BIT(4)
#define ECBATT_FLAG_TDD          BIT(5)
#define ECBATT_FLAG_ISD          BIT(6)
#define ECBATT_FLAG_OVERTAKEN    BIT(7)
#define ECBATT_FLAG_CHG          BIT(8)
#define ECBATT_FLAG_FC           BIT(9)
#define ECBATT_FLAG_XCHG         BIT(10)
#define ECBATT_FLAG_CHG_INH      BIT(11)
#define ECBATT_FLAG_BATLOW       BIT(12)
#define ECBATT_FLAG_BATHIGH      BIT(13)
#define ECBATT_FLAG_OTD          BIT(14)
#define ECBATT_FLAG_OTC          BIT(15)

/* Set 1 if you want to enable retry mechanism */
#define CONFIG_ECBATT_RETRY            1

/* Set 1 if you want to enable cache mechanism,
   But you have to enable retry mechanism first. */
#define CONFIG_ECBATT_CACHE            1

/* Set 1 if you want to get all data once from ec dock. */
#define CONFIG_ECBATT_GET_BATDATA      1

/* Set 1 if you want to support dummy battery.
   But you have to define PEGA_CHG_FAKE_BATT_ENABLE. */
#ifdef PEGA_CHG_FAKE_BATT_ENABLE
#define CONFIG_ECBATT_DUMMY_ENABLE     1
#endif /* PEGA_CHG_FAKE_BATT_ENABLE */

#define ABS(i)          ((i) > 0 ? (i) : -(i))

#define ZERO_DEGREE_CELSIUS_IN_TENTH_KELVIN   (-2731)

#define ECBATT_SHOW_LOG_INTERVAL    3000 // ms

#if (CONFIG_ECBATT_RETRY == 1)
// If the different between two results larger than acceptible value, let the device crash.
#define ECBATT_RETRY_TEMP_DIFF       300
#define ECBATT_RETRY_VOLT_DIFF       300
#define ECBATT_RETRY_CAP_DIFF       1000
#define ECBATT_RETRY_RSOC_DIFF        10
#define ECBATT_MAX_RETRY_TIMES         5

#define ECBATT_VALID_LOW_VOLT       2800
#define ECBATT_VALID_HIGH_VOLT      4240
#define ECBATT_VALID_LOW_TEMP       -400
#define ECBATT_VALID_HIGH_TEMP      1500
#define ECBATT_VALID_LOW_FCC           0
#define ECBATT_VALID_HIGH_FCC       6500
#define ECBATT_VALID_LOW_RM            0
#define ECBATT_VALID_HIGH_RM        6500
#define ECBATT_VALID_LOW_RSOC          0
#define ECBATT_VALID_HIGH_RSOC       100
#endif /* CONFIG_ECBATT_RETRY */

// shut down condition, depends on framework
#define ECBATT_SHUTDOWN_OVERTEMP     700  // shut down if >= 70 deg C
#define ECBATT_SHUTDOWN_LOWTEMP     -200  // shut down if <= -20 deg C
#define ECBATT_SHUTDOWN_LOWVOLT     3000  // low power if <= 3.0V
#define ECBATT_SHUTDOWN_LEVEL          5  // show 0% if <= 5%.
#define ECBATT_REMAP_BASE_LEVEL        6  // Remap 6%~94% to 1%~99%
#define ECBATT_VALID_LOW_VOLT       2800

#define ECBATT_HEALTH_HEAT           450  // stop charging when > 45 deg C
#define ECBATT_HEALTH_COLD             0  // stop charging when < 0 deg C

#define ECBATT_CRASH_LOW_VOLT          0
#define ECBATT_CRASH_HIGH_VOLT      6000
#define ECBATT_CRASH_LOW_RSOC          0
#define ECBATT_CRASH_HIGH_RSOC       100
#define ECBATT_CRASH_LOW_TEMP       -400
#define ECBATT_CRASH_HIGH_TEMP       850

//#ifdef PEGA_SMT_BUILD
#define ECBATT_FULL_CAPACITY_VAL     100
//#endif  /* PEGA_SMT_BUILD */

// interrupt condition, depends on interrupt pin
#define ECBATT_INTERRUPT_LOWVOLT    3500  // interrupt if 3.50V
#define ECBATT_INTERRUPT_LOWCAP      760  // interrupt if 760mAh
#define ECBATT_INTERRUPT_OVERVOLT   4240  // interrupt if 4.24V
#define ECBATT_INTERRUPT_OTC         550  // interrupt if 55 deg C
#define ECBATT_INTERRUPT_OTD         700  // interrupt if 70 deg C

#if (CONFIG_ECBATT_DUMMY_ENABLE == 1)
#define ECBATT_DUMMY_LOWEST_TEMP       0
#define ECBATT_DUMMY_LOWEST_LEVEL     15
#endif /* CONFIG_ECBATT_DUMMY_ENABLE */

static DEFINE_MUTEX(ecbatt_mutex);

struct ecbatt_access_methods {
	int (*read)(int batt_cmd, int *rt_value);
#if (CONFIG_ECBATT_GET_BATDATA == 1)
	int (*read_batdata)(int battdata_enum, int *rt_value);
#endif /* CONFIG_ECBATT_GET_BATDATA */
};

struct ecbatt_device_info {
	struct ecbatt_access_methods	*bus;
	unsigned short          fw_ver;
	unsigned short          df_ver;
	unsigned short          conf;
	int                     is_batt_dead;
#if (CONFIG_ECBATT_DUMMY_ENABLE == 1)
	int                     is_dummy_batt;
#endif /* CONFIG_ECBATT_DUMMY_ENABLE */
};
struct ecbatt_device_info *ecbatt_di;

#if (CONFIG_ECBATT_RETRY == 1)
#if (CONFIG_ECBATT_CACHE == 1)
#define ECBATT_CACHED_RESET_DELAY ((HZ) * 1)
#define ECBATT_CACHED_UPDATE_INTERVAL    1000  // msec
enum {
	ECBATT_CACHED_STATUS,
	ECBATT_CACHED_TEMP,
	ECBATT_CACHED_VOLT,
	ECBATT_CACHED_AI,
	ECBATT_CACHED_RSOC,
	ECBATT_CACHED_HEALTH,
	ECBATT_CACHED_RM,
	ECBATT_CACHED_FCC,
	ECBATT_CACHED_LEVEL,
	NUM_OF_ECBATT_CACHED,
};
struct ecbatt_cached {
	int                  cached_val[NUM_OF_ECBATT_CACHED];
	int                  sspnd_val[NUM_OF_ECBATT_CACHED];
	unsigned long        update_msec[NUM_OF_ECBATT_CACHED];
	spinlock_t           lock;
	int                  reset_delay_on;
	struct delayed_work	 reset_delay;
};
static struct ecbatt_cached batt_cached_info;
#else  /* CONFIG_ECBATT_CACHE */
static int pre_volt, pre_temp, pre_rsoc, pre_level;
#endif /* CONFIG_ECBATT_CACHE */
#endif /* CONFIG_ECBATT_RETRY */

#if (CONFIG_ECBATT_GET_BATDATA == 1)
#define ECBATT_DATA_RESET_DELAY ((HZ) * 1)
enum {
	ECBATT_DATA_VOLT,
	ECBATT_DATA_SOC,
	ECBATT_DATA_TEMP,
	ECBATT_DATA_FLAG,
	ECBATT_DATA_RM,
	ECBATT_DATA_FCC,
	ECBATT_DATA_AI,
	NUM_OF_ECBATT_DATA,
};
struct ecbatt_batdata {
	int                  val[NUM_OF_ECBATT_DATA];
	spinlock_t           lock;
	int                  need_update;
	int                  reset_delay_on;
	struct delayed_work	 reset_delay;
	struct mutex         mutex_lock;
};
static struct ecbatt_batdata batt_data;
#endif /* CONFIG_ECBATT_GET_BATDATA */

static int ecbatt_is_battery_present(void);

#if (CONFIG_ECBATT_GET_BATDATA == 1)
static int ecbatt_read_batdata(int battdata_enum, int *rt_value, struct ecbatt_device_info *di)
{
	return di->bus->read_batdata(battdata_enum, rt_value);
}
#endif /* CONFIG_ECBATT_GET_BATDATA */

static int ecbatt_read(int reg, int *rt_value, struct ecbatt_device_info *di)
{
	return di->bus->read(reg, rt_value);
}


#if (CONFIG_ECBATT_CACHE == 1 && CONFIG_ECBATT_RETRY == 1)
static void ecbatt_cached_info_init(void)
{
	int i = 0;
	unsigned long flag;

	PEGA_DBG_L("[ECBATT] %s is called.\n", __func__);
	if (batt_cached_info.reset_delay_on)
		cancel_delayed_work(&batt_cached_info.reset_delay);

	spin_lock_irqsave(&batt_cached_info.lock, flag);
	for (i = 0; i < NUM_OF_ECBATT_CACHED; i++)
	{
		batt_cached_info.update_msec[i] = 0;
		batt_cached_info.cached_val[i] = -1;
		batt_cached_info.sspnd_val[i] = -1;
	}
	batt_cached_info.reset_delay_on = 0;
	spin_unlock_irqrestore(&batt_cached_info.lock, flag);
}

static void ecbatt_cached_suspend(void)
{
	int i = 0;
	unsigned long flag;

	PEGA_DBG_L("[ECBATT] %s is called.\n", __func__);
	if (batt_cached_info.reset_delay_on)
		cancel_delayed_work(&batt_cached_info.reset_delay);

	spin_lock_irqsave(&batt_cached_info.lock, flag);
	for (i = 0; i < NUM_OF_ECBATT_CACHED; i++)
	{
		batt_cached_info.update_msec[i] = 0;
		batt_cached_info.cached_val[i] = -1;
	}
	batt_cached_info.reset_delay_on = 0;
	spin_unlock_irqrestore(&batt_cached_info.lock, flag);
}

static void ecbatt_cached_reset(struct work_struct *work)
{
	int i = 0;
	unsigned long flag;

	PEGA_DBG_L("[ECBATT] %s is called.\n", __func__);
	if (batt_cached_info.reset_delay_on)
		cancel_delayed_work(&batt_cached_info.reset_delay);

	spin_lock_irqsave(&batt_cached_info.lock, flag);
	for (i = 0; i < NUM_OF_ECBATT_CACHED; i++)
	{
		batt_cached_info.update_msec[i] = 0;
	}
	batt_cached_info.reset_delay_on = 0;
	spin_unlock_irqrestore(&batt_cached_info.lock, flag);
}

static void ecbatt_cached_reset_msec(int ecbatt_cached_type)
{
	unsigned long flag;

	PEGA_DBG_L("[ECBATT] %s is called.\n", __func__);
	spin_lock_irqsave(&batt_cached_info.lock, flag);
	batt_cached_info.update_msec[ecbatt_cached_type] = 0;
	spin_unlock_irqrestore(&batt_cached_info.lock, flag);
}

static void ecbatt_cached_update(int type, int val)
{
	unsigned long flag;

	PEGA_DBG_L("[ECBATT] %s is called, type: %d, value: %d.\n",
				__func__, type, val);
	if (batt_cached_info.reset_delay_on)
		cancel_delayed_work(&batt_cached_info.reset_delay);
	schedule_delayed_work(&batt_cached_info.reset_delay,
				ECBATT_CACHED_RESET_DELAY);
	spin_lock_irqsave(&batt_cached_info.lock, flag);
	batt_cached_info.update_msec[type] = jiffies_to_msecs(jiffies);
	batt_cached_info.cached_val[type] = val;
	batt_cached_info.sspnd_val[type] = val;
	batt_cached_info.reset_delay_on = 1;
	spin_unlock_irqrestore(&batt_cached_info.lock, flag);
}

static void ecbatt_cached_init(void)
{
	PEGA_DBG_L("[ECBATT] %s is called.\n", __func__);
	spin_lock_init(&batt_cached_info.lock);
	ecbatt_cached_info_init();
	INIT_DELAYED_WORK(&batt_cached_info.reset_delay,
			ecbatt_cached_reset);
}
#endif // CONFIG_ECBATT_CACHE && CONFIG_ECBATT_RETRY

#if (CONFIG_ECBATT_GET_BATDATA == 1)
static void ecbatt_batdata_info_init(void)
{
	int i = 0;

	PEGA_DBG_L("[ECBATT] %s is called.\n", __func__);
	for (i = 0; i < NUM_OF_ECBATT_DATA; i++)
		batt_data.val[i] = -1;
	batt_data.need_update = 1;
	batt_data.reset_delay_on = 0;
}

static void ecbatt_batdata_reset(struct work_struct *work)
{
	unsigned long flag;

	PEGA_DBG_L("[ECBATT] %s is called.\n", __func__);
	if (batt_data.reset_delay_on)
		cancel_delayed_work(&batt_data.reset_delay);
	spin_lock_irqsave(&batt_data.lock, flag);
	batt_data.need_update = 1;
	batt_data.reset_delay_on = 0;
	spin_unlock_irqrestore(&batt_data.lock, flag);
}

static void ecbatt_batdata_init(void)
{
	PEGA_DBG_L("[ECBATT] %s is called.\n", __func__);
	spin_lock_init(&batt_data.lock);
	mutex_init(&batt_data.mutex_lock);
	ecbatt_batdata_info_init();
	INIT_DELAYED_WORK(&batt_data.reset_delay, ecbatt_batdata_reset);
}
#endif // CONFIG_ECBATT_GET_BATDATA

static int ecbatt_avg_level(int level)
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

	PEGA_DBG_L("[ECBATT] %s, avg_level=%d.(%d, %d, %d, %d, %d, %d, %d, %d, %d, %d).\n",
			__func__, ret, avg_level[0], avg_level[1],
			avg_level[2], avg_level[3], avg_level[4], avg_level[5],
			avg_level[6], avg_level[7], avg_level[8], avg_level[9]);

	return ret;
}

static int ec_is_battery_gauge_ready(void)
{
#if (CONFIG_ECBATT_CACHE == 1 && CONFIG_ECBATT_RETRY == 1)
	static int had_ready = 0;
#endif // CONFIG_ECBATT_CACHE && CONFIG_ECBATT_RETRY
	int ret, status;

	ret = dock_status(&status);
	if ((FAIL == ret) ||
	    (SUCCESS == ret && DOCK_I2C_FAIL & status))
	{
		PEGA_DBG_L("[ECBATT_ERR] %s, dock is not ready, ret= %d, status=%d.\n",
				__func__, ret, status);
#if (CONFIG_ECBATT_CACHE == 1 && CONFIG_ECBATT_RETRY == 1)
		// clear cache
		if (1 == had_ready)
		{
			had_ready = 0;
			ecbatt_cached_info_init();
		}
#endif // CONFIG_ECBATT_CACHE && CONFIG_ECBATT_RETRY
		return 0;
	}
	else
	{
#if (CONFIG_ECBATT_CACHE == 1 && CONFIG_ECBATT_RETRY == 1)
		had_ready = 1;
#endif // CONFIG_ECBATT_CACHE && CONFIG_ECBATT_RETRY
		return 1;
	}
}

/*
 * Return the firmware version of battery gauge
 * Or "none" if something fails.
 */
static char* ec_battery_fw_version(struct ecbatt_device_info *di)
{
	static char ver[16];
	int flag = 0;

#if (CONFIG_ECBATT_DUMMY_ENABLE == 1)
	if (di->is_dummy_batt)
		flag = 2;
#endif /* CONFIG_ECBATT_DUMMY_ENABLE */

	sprintf(ver, "%04x%04X-%04X%x", di->fw_ver, di->df_ver, di->conf, flag);
	return ver;
}

/*
 * Return the battery temperature in tenths of degree Celsius
 * Or 0 if something fails.
 */
static int ec_battery_temperature(struct ecbatt_device_info *di)
{
	int ret = 0, temp = 0;
#if (CONFIG_ECBATT_RETRY == 1)
	static int temp_retry_cnt = 0;
#if (CONFIG_ECBATT_CACHE == 1)
	int pre_temp = batt_cached_info.cached_val[ECBATT_CACHED_TEMP];
	int sspnd_temp = batt_cached_info.sspnd_val[ECBATT_CACHED_TEMP];
	unsigned long curr_msec = 0;
	unsigned long pre_msec = batt_cached_info.update_msec[ECBATT_CACHED_TEMP];
#else  /* CONFIG_ECBATT_CACHE */
	static int sspnd_temp = 0;
#endif /* CONFIG_ECBATT_CACHE */
#endif /* CONFIG_ECBATT_RETRY */

	if (!ec_is_battery_gauge_ready() || di->is_batt_dead)
		return 0;

#if (CONFIG_ECBATT_CACHE == 1 && CONFIG_ECBATT_RETRY == 1)
	curr_msec = jiffies_to_msecs(jiffies);

	if (0 < pre_msec &&
	    ECBATT_CACHED_UPDATE_INTERVAL > ABS(curr_msec - pre_msec))
	{
		PEGA_DBG_L("[ECBATT] use cached temp, curr_msec: %lu, update_msec: %lu, pre_temp: %d.\n",
					curr_msec, pre_msec, pre_temp);
		return pre_temp;
	}
	else
	{
		PEGA_DBG_L("[ECBATT] read temp from gauge, curr_msec: %lu, update_msec: %lu, pre_temp: %d.\n",
					curr_msec, pre_msec, pre_temp);
	}
#endif /* CONFIG_ECBATT_CACHE && CONFIG_ECBATT_RETRY */

#if (CONFIG_ECBATT_GET_BATDATA == 1)
	PEGA_DBG_L("[ECBATT] %s, call ecbatt_read_batdata.\n", __func__);
	ret = ecbatt_read_batdata(ECBATT_DATA_TEMP, &temp, di);
#else  /* CONFIG_ECBATT_GET_BATDATA */
	udelay(66);
	ret = ecbatt_read(ECBATT_REG_TEMP, &temp, di);
#endif /* CONFIG_ECBATT_GET_BATDATA */

	if (ret)
	{
		PEGA_DBG_H("[ECBATT_ERR] error(0x%08x) reading temperature.\n", ret);
#if (CONFIG_ECBATT_RETRY == 1)
		if (-1 == pre_temp)
			pre_temp = (-1 != sspnd_temp) ? sspnd_temp : 0;
		return pre_temp;
#else  /* CONFIG_ECBATT_RETRY */
		return 0;
#endif /* CONFIG_ECBATT_RETRY */
	}

	temp += ZERO_DEGREE_CELSIUS_IN_TENTH_KELVIN;
	PEGA_DBG_L("[ECBATT] %s, temp=%d.\n", __func__, temp);

#if (CONFIG_ECBATT_DUMMY_ENABLE == 1)
	if (1 == di->is_dummy_batt && ECBATT_DUMMY_LOWEST_TEMP > temp)
		temp = ECBATT_DUMMY_LOWEST_TEMP;
#endif /* CONFIG_ECBATT_DUMMY_ENABLE */

#if (CONFIG_ECBATT_RETRY == 0)

	//if (ZERO_DEGREE_CELSIUS_IN_TENTH_KELVIN == temp)
	if (ECBATT_VALID_LOW_TEMP > temp && ECBATT_VALID_HIGH_TEMP < temp)
	{
		temp = 0;
#if (CONFIG_ECBATT_GET_BATDATA == 1)
		PEGA_DBG_L("[ECBATT] %s, call ecbatt_batdata_reset.\n", __func__);
		ecbatt_batdata_reset(NULL);
#endif /* CONFIG_ECBATT_GET_BATDATA */
	}

	return temp;

#else  /* CONFIG_ECBATT_RETRY */
	if (ECBATT_VALID_LOW_TEMP <= temp && ECBATT_VALID_HIGH_TEMP >= temp)
		goto done;
	else
		goto retry;

done:
	if (ABS(temp - pre_temp) <= ECBATT_RETRY_TEMP_DIFF || -1 == pre_temp)
	{
		temp_retry_cnt = ECBATT_MAX_RETRY_TIMES;
	}

retry:
	if (ECBATT_MAX_RETRY_TIMES <= temp_retry_cnt)
	{
		PEGA_DBG_L("[ECBATT_RETRY] %s, max retry count, temp=%d, return.\n",
			__func__, temp);
		temp_retry_cnt = 0;
#if (CONFIG_ECBATT_CACHE == 1)
		ecbatt_cached_update(ECBATT_CACHED_TEMP, temp);
		pre_temp = temp;
#else  /* CONFIG_ECBATT_CACHE */
		sspnd_temp = pre_temp = temp;
#endif /* CONFIG_ECBATT_CACHE */
	}
	else
	{
		PEGA_DBG_H("[ECBATT_RETRY] %s, retry=%d, temp=%d, return pre_temp=%d or sspnd_temp=%d\n",
			__func__, temp_retry_cnt, temp, pre_temp, sspnd_temp);
		temp_retry_cnt++;
#if (CONFIG_ECBATT_GET_BATDATA == 1)
		PEGA_DBG_L("[ECBATT] %s, call ecbatt_batdata_reset.\n", __func__);
		ecbatt_batdata_reset(NULL);
#endif /* CONFIG_ECBATT_GET_BATDATA */
		if (-1 == pre_temp)
			pre_temp = (-1 == sspnd_temp) ? 0 : sspnd_temp;

#if (CONFIG_ECBATT_CACHE == 1)
		ecbatt_cached_reset_msec(ECBATT_CACHED_TEMP);
#endif /* CONFIG_ECBATT_CACHE */
	}
	return pre_temp;
#endif /* CONFIG_ECBATT_RETRY */
}

/*
 * Return the battery Voltage in milivolts
 * Or 0 if something fails.
 */
static int ec_battery_voltage(struct ecbatt_device_info *di)
{
	int ret = 0, volt = 0;
#if (CONFIG_ECBATT_RETRY == 1)
	static int volt_retry_cnt = 0;
#if (CONFIG_ECBATT_CACHE == 1)
	int pre_volt = batt_cached_info.cached_val[ECBATT_CACHED_VOLT];
	int sspnd_volt = batt_cached_info.sspnd_val[ECBATT_CACHED_VOLT];
	unsigned long curr_msec = 0;
	unsigned long pre_msec = batt_cached_info.update_msec[ECBATT_CACHED_VOLT];
#else  /* CONFIG_ECBATT_CACHE */
	static int sspnd_volt = 0;
#endif /* CONFIG_ECBATT_CACHE */
#endif /* CONFIG_ECBATT_RETRY */

	if (!ec_is_battery_gauge_ready() || di->is_batt_dead)
		return 0;

#if (CONFIG_ECBATT_CACHE == 1 && CONFIG_ECBATT_RETRY == 1)
	curr_msec = jiffies_to_msecs(jiffies);

	if (0 < pre_msec &&
	    ECBATT_CACHED_UPDATE_INTERVAL > ABS(curr_msec - pre_msec))
	{
		PEGA_DBG_L("[ECBATT] use cached volt, curr_msec: %lu, update_msec: %lu, pre_volt: %d.\n",
					curr_msec, pre_msec, pre_volt);
		return pre_volt;
	}
	else
	{
		PEGA_DBG_L("[ECBATT] read volt from gauge, curr_msec: %lu, update_msec: %lu, pre_volt: %d.\n",
					curr_msec, pre_msec, pre_volt);
	}
#endif /* CONFIG_ECBATT_CACHE && CONFIG_ECBATT_RETRY */

#if (CONFIG_ECBATT_GET_BATDATA == 1)
	PEGA_DBG_L("[ECBATT] %s, call ecbatt_read_batdata.\n", __func__);
	ret = ecbatt_read_batdata(ECBATT_DATA_VOLT, &volt, di);
#else  /* CONFIG_ECBATT_GET_BATDATA */
	udelay(66);
	ret = ecbatt_read(ECBATT_REG_VOLT, &volt, di);
#endif /* CONFIG_ECBATT_GET_BATDATA */

	if (ret)
	{
		PEGA_DBG_H("[ECBATT_ERR] error(0x%08x) reading voltage.\n", ret);
#if (CONFIG_ECBATT_RETRY == 1)
		if (-1 == pre_volt)
			pre_volt = (-1 != sspnd_volt) ? sspnd_volt : 0;
		return pre_volt;
#else  /* CONFIG_ECBATT_RETRY */
		return 0;
#endif /* CONFIG_ECBATT_RETRY */
	}

	PEGA_DBG_L("[ECBATT] %s, volt=%d.\n", __func__, volt);
#if (CONFIG_ECBATT_RETRY == 0)

#if (CONFIG_ECBATT_GET_BATDATA == 1)
	if (ECBATT_VALID_LOW_VOLT > volt && ECBATT_VALID_HIGH_VOLT < volt)
	{
		PEGA_DBG_L("[ECBATT] %s, call ecbatt_batdata_reset.\n", __func__);
		ecbatt_batdata_reset(NULL);
	}
#endif /* CONFIG_ECBATT_GET_BATDATA */

	return volt;

#else  /* CONFIG_ECBATT_RETRY */
	//if (volt > ECBATT_SHUTDOWN_LOWVOLT && volt < ECBATT_INTERRUPT_OVERVOLT)
	if (ECBATT_VALID_LOW_VOLT <= volt && ECBATT_VALID_HIGH_VOLT >= volt)
		goto done;
	else
		goto retry;

done:
	if (ABS(volt - pre_volt) < ECBATT_RETRY_VOLT_DIFF || -1 == pre_volt)
	{
		volt_retry_cnt = ECBATT_MAX_RETRY_TIMES;
	}

retry:
	if (ECBATT_MAX_RETRY_TIMES <= volt_retry_cnt)
	{
		PEGA_DBG_L("[ECBATT_RETRY] %s, max retry count, volt=%d, return.\n",
			__func__, volt);
		volt_retry_cnt = 0;
#if (CONFIG_ECBATT_CACHE == 1)
		ecbatt_cached_update(ECBATT_CACHED_VOLT, volt);
		pre_volt = volt;
#else  /* CONFIG_ECBATT_CACHE */
		sspnd_volt = pre_volt = volt;
#endif /* CONFIG_ECBATT_CACHE */
	}
	else
	{
		PEGA_DBG_H("[ECBATT_RETRY] %s, retry=%d, volt=%d, return pre_volt=%d or sspnd_volt=%d\n",
			__func__, volt_retry_cnt, volt, pre_volt, sspnd_volt);
		volt_retry_cnt++;
#if (CONFIG_ECBATT_GET_BATDATA == 1)
		PEGA_DBG_L("[ECBATT] %s, call ecbatt_batdata_reset.\n", __func__);
		ecbatt_batdata_reset(NULL);
#endif /* CONFIG_ECBATT_GET_BATDATA */
		if (-1 == pre_volt)
			pre_volt = (-1 == sspnd_volt) ? 0 : sspnd_volt;

#if (CONFIG_ECBATT_CACHE == 1)
		ecbatt_cached_reset_msec(ECBATT_CACHED_VOLT);
#endif /* CONFIG_ECBATT_CACHE */
	}
	return pre_volt;
#endif /* CONFIG_ECBATT_RETRY */
}

/*
 * Return the battery average current
 * Note that current can be negative signed as well
 * Or 0 if something fails.
 */
static int ec_battery_current(struct ecbatt_device_info *di)
{
	int ret = 0, curr = 0;
#if (CONFIG_ECBATT_RETRY == 1)
#if (CONFIG_ECBATT_CACHE == 1)
	int pre_curr = batt_cached_info.cached_val[ECBATT_CACHED_AI];
	unsigned long curr_msec = 0;
	unsigned long pre_msec = batt_cached_info.update_msec[ECBATT_CACHED_AI];
#else  /* CONFIG_ECBATT_CACHE */
	static int pre_curr = 0;
#endif /* CONFIG_ECBATT_CACHE */
#endif /* CONFIG_ECBATT_RETRY */

	if (!ec_is_battery_gauge_ready() || di->is_batt_dead)
		return 0;

#if (CONFIG_ECBATT_CACHE == 1 && CONFIG_ECBATT_RETRY == 1)
	curr_msec = jiffies_to_msecs(jiffies);

	if (0 < pre_msec &&
	    ECBATT_CACHED_UPDATE_INTERVAL > ABS(curr_msec - pre_msec))
	{
		PEGA_DBG_L("[ECBATT] use cached curr, curr_msec: %lu, update_msec: %lu, pre_curr: %d.\n",
					curr_msec, pre_msec, pre_curr);
		return pre_curr;
	}
	else
	{
		PEGA_DBG_L("[ECBATT] read curr from gauge, curr_msec: %lu, update_msec: %lu, pre_curr: %d.\n",
					curr_msec, pre_msec, pre_curr);
	}
#endif /* CONFIG_ECBATT_CACHE && CONFIG_ECBATT_RETRY */

#if (CONFIG_ECBATT_GET_BATDATA == 1)
	PEGA_DBG_L("[ECBATT] %s, call ecbatt_read_batdata.\n", __func__);
	ret = ecbatt_read_batdata(ECBATT_DATA_AI, &curr, di);
#else  /* CONFIG_ECBATT_GET_BATDATA */
	udelay(66);
	ret = ecbatt_read(ECBATT_REG_AI, &curr, di);
#endif /* CONFIG_ECBATT_GET_BATDATA */

	if (ret)
	{
		PEGA_DBG_H("[ECBATT_ERR] error(0x%08x) reading current\n", ret);
#if (CONFIG_ECBATT_RETRY == 1)
		if (-1 == pre_curr)
			pre_curr = 0;
		return pre_curr;
#else  /* CONFIG_ECBATT_RETRY */
		return 0;
#endif /* CONFIG_ECBATT_RETRY */
	}

	curr = (curr > 32768) ? curr - 65536 : curr;
	PEGA_DBG_L("[ECBATT] %s, curr= %d\n", __func__, curr);

#if (CONFIG_ECBATT_GET_BATDATA == 1)
	if (0 == curr && 1 <= ecbatt_is_battery_present())
	{
		PEGA_DBG_L("[ECBATT] %s, call ecbatt_batdata_reset.\n", __func__);
		ecbatt_batdata_reset(NULL);
	}
#endif /* CONFIG_ECBATT_GET_BATDATA */

#if (CONFIG_ECBATT_RETRY == 0)
	return curr;
#else  /* CONFIG_ECBATT_RETRY */
#if (CONFIG_ECBATT_CACHE == 1)
	ecbatt_cached_update(ECBATT_CACHED_AI, curr);
#endif /* CONFIG_ECBATT_CACHE */
	pre_curr = curr;
	return pre_curr;
#endif /* CONFIG_ECBATT_RETRY */
}

/*
 * Return the compensated capacity of the battery when full charged (mAh)
 * Or 0 if something fails.
 */
static int ec_battery_fcc(struct ecbatt_device_info *di)
{
	int ret = 0, fcc = 0;
#if (CONFIG_ECBATT_RETRY == 1)
	static int fcc_retry_cnt = 0;
#if (CONFIG_ECBATT_CACHE == 1)
	int pre_fcc = batt_cached_info.cached_val[ECBATT_CACHED_FCC];
	int sspnd_fcc = batt_cached_info.sspnd_val[ECBATT_CACHED_FCC];
	unsigned long curr_msec = 0;
	unsigned long pre_msec = batt_cached_info.update_msec[ECBATT_CACHED_FCC];
#else  /* CONFIG_ECBATT_CACHE */
	static int pre_fcc = 0;
	static int sspnd_fcc = 0;
#endif /* CONFIG_ECBATT_CACHE */
#endif /* CONFIG_ECBATT_RETRY */

	if (!ec_is_battery_gauge_ready() || di->is_batt_dead)
		return 0;

#if (CONFIG_ECBATT_CACHE == 1 && CONFIG_ECBATT_RETRY == 1)
	curr_msec = jiffies_to_msecs(jiffies);

	if (0 < pre_msec &&
	    ECBATT_CACHED_UPDATE_INTERVAL > ABS(curr_msec - pre_msec))
	{
		PEGA_DBG_L("[ECBATT] use cached fcc, curr_msec: %lu, update_msec: %lu, pre_fcc: %d.\n",
					curr_msec, pre_msec, pre_fcc);
		return pre_fcc;
	}
	else
	{
		PEGA_DBG_L("[ECBATT] read fcc from gauge, curr_msec: %lu, update_msec: %lu, pre_fcc: %d.\n",
					curr_msec, pre_msec, pre_fcc);
	}
#endif /* CONFIG_ECBATT_CACHE && CONFIG_ECBATT_RETRY */

#if (CONFIG_ECBATT_GET_BATDATA == 1)
	PEGA_DBG_L("[ECBATT] %s, call ecbatt_read_batdata.\n", __func__);
	ret = ecbatt_read_batdata(ECBATT_DATA_FCC, &fcc, di);
#else  /* CONFIG_ECBATT_GET_BATDATA */
	udelay(66);
	ret = ecbatt_read(ECBATT_REG_FCC, &fcc, di);
#endif /* CONFIG_ECBATT_GET_BATDATA */

	if (ret)
	{
		PEGA_DBG_H("[ECBATT_ERR] error(0x%08x) reading fcc\n", ret);
#if (CONFIG_ECBATT_RETRY == 1)
		if (-1 == pre_fcc)
			pre_fcc = 0;
		return pre_fcc;
#else  /* CONFIG_ECBATT_RETRY */
		return 0;
#endif /* CONFIG_ECBATT_RETRY */
	}

	PEGA_DBG_L("[ECBATT] %s, fcc= %d\n", __func__, fcc);

#if (CONFIG_ECBATT_GET_BATDATA == 1)
	if (ECBATT_VALID_LOW_FCC > fcc && ECBATT_VALID_HIGH_FCC < fcc)
	{
		PEGA_DBG_L("[ECBATT] %s, call ecbatt_batdata_reset.\n", __func__);
		ecbatt_batdata_reset(NULL);
	}
#endif /* CONFIG_ECBATT_GET_BATDATA */

#if (CONFIG_ECBATT_RETRY == 0)
	return fcc;
#else  /* CONFIG_ECBATT_RETRY */
	if (ECBATT_VALID_LOW_FCC <= fcc && ECBATT_VALID_HIGH_FCC >= fcc)
		goto done;
	else
		goto retry;

done:
	if (ABS(fcc - pre_fcc) <= ECBATT_RETRY_CAP_DIFF || -1 == pre_fcc)
	{
		fcc_retry_cnt = ECBATT_MAX_RETRY_TIMES;
	}

retry:
	if (ECBATT_MAX_RETRY_TIMES <= fcc_retry_cnt)
	{
		PEGA_DBG_L("[ECBATT_RETRY] %s, max retry count, fcc=%d, return.\n",
			__func__, fcc);
		fcc_retry_cnt = 0;

#if (CONFIG_ECBATT_CACHE == 1)
		ecbatt_cached_update(ECBATT_CACHED_FCC, fcc);
		pre_fcc = fcc;
#else  /* CONFIG_ECBATT_CACHE */
		sspnd_fcc = pre_fcc = fcc;
#endif /* CONFIG_ECBATT_CACHE */
	}
	else
	{
		PEGA_DBG_H("[ECBATT_RETRY] %s, retry=%d, fcc=%d, return pre_fcc=%d or sspnd_fcc=%d\n",
			__func__, fcc_retry_cnt, fcc, pre_fcc, sspnd_fcc);
		fcc_retry_cnt++;

		if (-1 == pre_fcc)
			pre_fcc = (-1 == sspnd_fcc) ? 0 : sspnd_fcc;

#if (CONFIG_ECBATT_CACHE == 1)
		ecbatt_cached_reset_msec(ECBATT_CACHED_FCC);
#endif /* CONFIG_ECBATT_CACHE */
	}
	return pre_fcc;
#endif /* CONFIG_ECBATT_RETRY */
}

/*
 * Return the compensated capacity remaining (mAh)
 * Or 0 if something fails.
 */
static int ec_battery_rm(struct ecbatt_device_info *di)
{
	int ret = 0, rm = 0, fcc = 0;
#if (CONFIG_ECBATT_RETRY == 1)
	static int rm_retry_cnt = 0;
#if (CONFIG_ECBATT_CACHE == 1)
	int pre_rm = batt_cached_info.cached_val[ECBATT_CACHED_RM];
	int sspnd_rm = batt_cached_info.sspnd_val[ECBATT_CACHED_RM];
	unsigned long curr_msec = 0;
	unsigned long pre_msec = batt_cached_info.update_msec[ECBATT_CACHED_RM];
#else  /* CONFIG_ECBATT_CACHE */
	static int pre_rm = 0;
	static int sspnd_rm = 0;
#endif /* CONFIG_ECBATT_CACHE */
#endif /* CONFIG_ECBATT_RETRY */

	if (!ec_is_battery_gauge_ready() || di->is_batt_dead)
		return 0;

#if (CONFIG_ECBATT_CACHE == 1 && CONFIG_ECBATT_RETRY == 1)
	curr_msec = jiffies_to_msecs(jiffies);

	if (0 < pre_msec &&
	    ECBATT_CACHED_UPDATE_INTERVAL > ABS(curr_msec - pre_msec))
	{
		PEGA_DBG_L("[ECBATT] use cached rm, curr_msec: %lu, update_msec: %lu, pre_rm: %d.\n",
					curr_msec, pre_msec, pre_rm);
		return pre_rm;
	}
	else
	{
		PEGA_DBG_L("[ECBATT] read rm from gauge, curr_msec: %lu, update_msec: %lu, pre_rm: %d.\n",
					curr_msec, pre_msec, pre_rm);
	}
#endif /* CONFIG_ECBATT_CACHE && CONFIG_ECBATT_RETRY */

#if (CONFIG_ECBATT_GET_BATDATA == 1)
	PEGA_DBG_L("[ECBATT] %s, call ecbatt_read_batdata.\n", __func__);
	ret = ecbatt_read_batdata(ECBATT_DATA_RM, &rm, di);
#else  /* CONFIG_ECBATT_GET_BATDATA */
	udelay(66);
	ret = ecbatt_read(ECBATT_REG_RM, &rm, di);
#endif /* CONFIG_ECBATT_GET_BATDATA */

	if (ret)
	{
		PEGA_DBG_H("[ECBATT_ERR] error(0x%08x) reading rm\n", ret);
#if (CONFIG_ECBATT_RETRY == 1)
		if (-1 == pre_rm)
			pre_rm = 0;
		return pre_rm;
#else  /* CONFIG_ECBATT_RETRY */
		return 0;
#endif /* CONFIG_ECBATT_RETRY */
	}

	PEGA_DBG_L("[ECBATT] %s, rm= %d\n", __func__, rm);

	fcc = ec_battery_fcc(di);

#if (CONFIG_ECBATT_GET_BATDATA == 1)
	if (ECBATT_VALID_LOW_RM > rm && fcc < rm)
	{
		PEGA_DBG_L("[ECBATT] %s, call ecbatt_batdata_reset.\n", __func__);
		ecbatt_batdata_reset(NULL);
	}
#endif /* CONFIG_ECBATT_GET_BATDATA */

#if (CONFIG_ECBATT_RETRY == 0)
	return rm;
#else  /* CONFIG_ECBATT_RETRY */
	if (ECBATT_VALID_LOW_RM <= rm && fcc >= rm)
		goto done;
	else
		goto retry;

done:
	if (ABS(rm - pre_rm) <= ECBATT_RETRY_CAP_DIFF || -1 == pre_rm)
	{
		rm_retry_cnt = ECBATT_MAX_RETRY_TIMES;
	}

retry:
	if (ECBATT_MAX_RETRY_TIMES <= rm_retry_cnt)
	{
		PEGA_DBG_L("[ECBATT_RETRY] %s, max retry count, rm=%d, return.\n",
			__func__, rm);
		rm_retry_cnt = 0;

#if (CONFIG_ECBATT_CACHE == 1)
		ecbatt_cached_update(ECBATT_CACHED_RM, rm);
		pre_rm = rm;
#else  /* CONFIG_ECBATT_CACHE */
		sspnd_rm = pre_rm = rm;
#endif /* CONFIG_ECBATT_CACHE */
	}
	else
	{
		PEGA_DBG_H("[ECBATT_RETRY] %s, retry=%d, rm=%d, return pre_rm=%d or sspnd_rm=%d\n",
			__func__, rm_retry_cnt, rm, pre_rm, sspnd_rm);
		rm_retry_cnt++;

		if (-1 == pre_rm)
			pre_rm = (-1 == sspnd_rm) ? 0 : sspnd_rm;

#if (CONFIG_ECBATT_CACHE == 1)
		ecbatt_cached_reset_msec(ECBATT_CACHED_RM);
#endif /* CONFIG_ECBATT_CACHE */
	}
	return pre_rm;
#endif /* CONFIG_ECBATT_RETRY */
}

/*
 * Return the battery Relative State-of-Charge
 * Or < 0 if something fails.
 */
static int ec_battery_rsoc(struct ecbatt_device_info *di)
{
	int ret = 0, rsoc = 0;
#if (CONFIG_ECBATT_RETRY == 1)
	static int rsoc_retry_cnt = 0;
#if (CONFIG_ECBATT_CACHE == 1)
	int pre_rsoc = batt_cached_info.cached_val[ECBATT_CACHED_RSOC];
	int sspnd_rsoc = batt_cached_info.sspnd_val[ECBATT_CACHED_RSOC];
	unsigned long curr_msec = 0;
	unsigned long pre_msec = batt_cached_info.update_msec[ECBATT_CACHED_RSOC];
#else  /* CONFIG_ECBATT_CACHE */
	static int sspnd_rsoc = -1;
#endif /* CONFIG_ECBATT_CACHE */
#endif /* CONFIG_ECBATT_RETRY */

	if (!ec_is_battery_gauge_ready() || di->is_batt_dead)
		return 0;

#if (CONFIG_ECBATT_CACHE == 1 && CONFIG_ECBATT_RETRY == 1)
	curr_msec = jiffies_to_msecs(jiffies);

	if (0 < pre_msec &&
	    ECBATT_CACHED_UPDATE_INTERVAL > ABS(curr_msec - pre_msec))
	{
		PEGA_DBG_L("[ECBATT] use cached rsoc, curr_msec: %lu, update_msec: %lu, pre_rsoc: %d.\n",
					curr_msec, pre_msec, pre_rsoc);
		return pre_rsoc;
	}
	else
	{
		PEGA_DBG_L("[ECBATT] read rsoc from gauge, curr_msec: %lu, update_msec: %lu, pre_rsoc: %d.\n",
					curr_msec, pre_msec, pre_rsoc);
	}
#endif /* CONFIG_ECBATT_CACHE && CONFIG_ECBATT_RETRY */

#if (CONFIG_ECBATT_GET_BATDATA == 1)
	PEGA_DBG_L("[ECBATT] %s, call ecbatt_read_batdata.\n", __func__);
	ret = ecbatt_read_batdata(ECBATT_DATA_SOC, &rsoc, di);
#else  /* CONFIG_ECBATT_GET_BATDATA */
	udelay(66);
	ret = ecbatt_read(ECBATT_REG_SOC, &rsoc, di);
#endif /* CONFIG_ECBATT_GET_BATDATA */

	if (ret)
	{
		PEGA_DBG_H("[ECBATT_ERR] error(0x%08x) reading relative State-of-Charge\n", ret);
#if (CONFIG_ECBATT_RETRY == 1)
		if (-1 == pre_rsoc)
			pre_rsoc = (-1 != sspnd_rsoc) ? sspnd_rsoc : 0;
		return pre_rsoc;
#else  /* CONFIG_ECBATT_RETRY */
		return 0;
#endif /* CONFIG_ECBATT_RETRY */
	}

	PEGA_DBG_L("[ECBATT] %s, succeed to get rsoc=%d from gauge.\n", __func__, rsoc);

#if (CONFIG_ECBATT_RETRY == 0)

#if (CONFIG_ECBATT_GET_BATDATA == 1)
	if (ECBATT_VALID_LOW_RSOC > rsoc && ECBATT_VALID_HIGH_RSOC < rsoc)
	{
		PEGA_DBG_L("[ECBATT] %s, call ecbatt_batdata_reset.\n", __func__);
		ecbatt_batdata_reset(NULL);
	}
#endif /* CONFIG_ECBATT_GET_BATDATA */

	return rsoc;
#else /* CONFIG_ECBATT_RETRY */
	if (ECBATT_VALID_LOW_RSOC <= rsoc && ECBATT_VALID_HIGH_RSOC >= rsoc)
		goto done;
	else
		goto retry;

done:
	if (ABS(rsoc - pre_rsoc) < ECBATT_RETRY_RSOC_DIFF || -1 == pre_rsoc)
	{
		rsoc_retry_cnt = ECBATT_MAX_RETRY_TIMES;
	}

retry:
	if (ECBATT_MAX_RETRY_TIMES <= rsoc_retry_cnt)
	{
		PEGA_DBG_L("[ECBATT_RETRY] %s, max retry count, rsoc=%d, return.\n",
			__func__, rsoc);
		rsoc_retry_cnt = 0;
#if (CONFIG_ECBATT_CACHE == 1)
		ecbatt_cached_update(ECBATT_CACHED_RSOC, rsoc);
		pre_rsoc = rsoc;
#else  /* CONFIG_ECBATT_CACHE */
		sspnd_rsoc = pre_rsoc = rsoc;
#endif /* CONFIG_ECBATT_CACHE */
	}
	else
	{
		PEGA_DBG_H("[ECBATT_RETRY] %s, retry=%d, rsoc=%d, return pre_rsoc=%d or sspnd_rsoc=%d\n",
			__func__, rsoc_retry_cnt, rsoc, pre_rsoc, sspnd_rsoc);
		rsoc_retry_cnt++;
#if (CONFIG_ECBATT_GET_BATDATA == 1)
		PEGA_DBG_L("[ECBATT] %s, call ecbatt_batdata_reset.\n", __func__);
		ecbatt_batdata_reset(NULL);
#endif /* CONFIG_ECBATT_GET_BATDATA */
		if (-1 == pre_rsoc)
			pre_rsoc = (-1 == sspnd_rsoc) ? 0 : sspnd_rsoc;

#if (CONFIG_ECBATT_CACHE == 1)
		ecbatt_cached_reset_msec(ECBATT_CACHED_RSOC);
#endif /* CONFIG_ECBATT_CACHE */
	}
	return pre_rsoc;
#endif /* CONFIG_ECBATT_RETRY */
}

static int ec_battery_level(struct ecbatt_device_info *di)
{
	int ret = -1;
	int volt = -1, flags = -1, rm = -1, fcc = -1, level = -1, rsoc = -1, re_rsoc = -1, curr = -1, temp = -1;
#if (CONFIG_ECBATT_RETRY == 1)
	int fin = -1;
	static int level_retry_cnt = 0;
	int cached_level = 0;
#if (CONFIG_ECBATT_CACHE == 1)
	int pre_level = batt_cached_info.cached_val[ECBATT_CACHED_LEVEL];
	int sspnd_level = batt_cached_info.sspnd_val[ECBATT_CACHED_LEVEL];
#else  /* CONFIG_ECBATT_CACHE */
	static int sspnd_level = -1;
#endif /* CONFIG_ECBATT_CACHE */
#endif /* CONFIG_ECBATT_RETRY */

	if (!ec_is_battery_gauge_ready() || di->is_batt_dead)
		return 0;

	udelay(66);
	curr = ec_battery_current(di);

	udelay(66);
	rsoc = ec_battery_rsoc(di);
	if (ECBATT_SHUTDOWN_LEVEL < rsoc && 100 >= rsoc)
	{
		re_rsoc = 99 - (int)(98 * (94 - rsoc) / (94 - ECBATT_REMAP_BASE_LEVEL));
		if (re_rsoc > 99)
			re_rsoc = 99;
		else if (re_rsoc < 1)
			re_rsoc = 1;
	}
	else
		re_rsoc = 0;
	PEGA_DBG_L("[ECBATT] %s, rsoc=%d, remap rsoc = %d.\n", __func__, rsoc, re_rsoc);

	udelay(66);
	volt = ec_battery_voltage(di);

	udelay(66);
	rm = ec_battery_rm(di);

	udelay(66);
	fcc = ec_battery_fcc(di);

	udelay(66);
	temp = ec_battery_temperature(di);

	// Check capacity only.
	if (rsoc <= ECBATT_SHUTDOWN_LEVEL &&
	    rm <= fcc * (ECBATT_SHUTDOWN_LEVEL + 1) / 100)
	{
		level = 0;
#if (CONFIG_ECBATT_RETRY == 0)
		goto showlog;
#else  /* CONFIG_ECBATT_RETRY */
		if (-1 != pre_level && curr <= 0)
			fin = 0; //need retry;
		else
			fin = 1; //done;
		goto done;
#endif /* CONFIG_ECBATT_RETRY */
	}

#if (CONFIG_ECBATT_GET_BATDATA == 1)
	PEGA_DBG_L("[ECBATT] %s, call ecbatt_read_batdata.\n", __func__);
	ret = ecbatt_read_batdata(ECBATT_DATA_FLAG, &flags, di);
#else  /* CONFIG_ECBATT_GET_BATDATA */
	udelay(66);
	ret = ecbatt_read(ECBATT_REG_FLAGS, &flags, di);
#endif /* CONFIG_ECBATT_GET_BATDATA */

	if (ret < 0)
	{
		PEGA_DBG_H("[ECBATT_ERR] error(0x%08x) reading flags.\n", ret);
#if (CONFIG_ECBATT_RETRY == 0)
		goto showlog;
#else  /* CONFIG_ECBATT_RETRY */
		goto done;
#endif /* CONFIG_ECBATT_RETRY */
	}
	else
	{
		if (flags & ECBATT_FLAG_FC)
		{
			if (volt > 3900 &&
			    (rm > 0 && fcc > 0 && rm > fcc * 94 / 100 && rsoc > 94))
			{
				level = 100;
#if (CONFIG_ECBATT_RETRY == 0)
				goto showlog;
#else  /* CONFIG_ECBATT_RETRY */
				if (0 <= pre_level && 99 > pre_level)
					fin = 0; //need retry;
				else
					fin = 1; //done;
				goto done;
#endif /* CONFIG_ECBATT_RETRY */
			}
		}
	}

	if (fcc > 0 && rm > 0)
	{
		level = 99 - (int)(98 * (94 * fcc - 100 * rm) / ((94 - ECBATT_REMAP_BASE_LEVEL) * fcc));
		if (level > 99)
			level = 99;
		else if (level < 1)
		{
			if (1 < re_rsoc && 99 > re_rsoc)
				level = re_rsoc;
			else
				level = 1;
		}
	}
	else
		level = re_rsoc;

#if (CONFIG_ECBATT_RETRY == 0)
showlog:
#if (CONFIG_ECBATT_DUMMY_ENABLE == 1)
	if (1 == di->is_dummy_batt && ECBATT_DUMMY_LOWEST_LEVEL > level)
		level = ECBATT_DUMMY_LOWEST_LEVEL;
#endif /* CONFIG_ECBATT_DUMMY_ENABLE */

	//if (show_log)
	{
		PEGA_DBG_H("[ECBATT] %s, volt=%d, curr=%d, rsoc=%d, re_rsoc=%d, rm=%d, fcc=%d, level=%d, temp=%d, flags=0x%04x.\n",
				__func__, volt, curr, rsoc, re_rsoc, rm, fcc, level, temp, flags);
	}

	return level;
#else  /* CONFIG_ECBATT_RETRY */
	if ((pre_level >= 0) &&
	    ((curr >= 0 && level < pre_level) ||
	     (curr <= 0 && level > pre_level)))
	{
		PEGA_DBG_L("[ECBATT] %s, curr= %d, UI level= %d, level_cache= %d is used.\n",
			__func__, curr, level, pre_level);
		level = pre_level;
	}
	else
	{
		PEGA_DBG_L("[ECBATT] %s, UI level= %d\n", __func__, level);
	}

	fin = 1;

done:
	//if (show_log)
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
			PEGA_DBG_H("[ECBATT] %s, volt=%d, curr=%d, rsoc=%d, re_rsoc=%d, rm=%d, fcc=%d, level=%d, level_cache=%d, level_suspend=%d, temp=%d, flags=0x%04x.\n",
					__func__, volt, curr, rsoc, re_rsoc, rm, fcc, level, pre_level, sspnd_level, temp, flags);
		}
	}

	if (0 == fin && rsoc > ECBATT_SHUTDOWN_LEVEL)
		goto retry;

	if (-1 == pre_level ||
	    (ABS(level - pre_level) < ECBATT_RETRY_RSOC_DIFF &&
	     ABS(level - re_rsoc) <= ECBATT_RETRY_RSOC_DIFF) ||
	    (0 == level && ((0 < rm && 0 < fcc) || 0 < rsoc)))
	{
		level_retry_cnt = ECBATT_MAX_RETRY_TIMES;
	}

retry:
	cached_level = (-1 == pre_level) ? sspnd_level : pre_level;
	if (0 < cached_level &&
	    ABS(level - cached_level) > ECBATT_RETRY_RSOC_DIFF &&
	    ABS(level - cached_level) > ABS(re_rsoc - cached_level))
	{
		PEGA_DBG_H("[ECBATT_RETRY] %s, incorrect level=%d found, assigned to %d.\n",
				__func__, level, re_rsoc);
		level = re_rsoc;
	}

	if (ECBATT_MAX_RETRY_TIMES <= level_retry_cnt)
	{
		if (0 == level)
		{
			if (1 < re_rsoc && volt > ECBATT_SHUTDOWN_LOWVOLT)
			{
				level = re_rsoc;
				level_retry_cnt = 0;
			}
			else
				level_retry_cnt = ECBATT_MAX_RETRY_TIMES;
		}
		else if (100 == level && 99 <= re_rsoc)
			level_retry_cnt = ECBATT_MAX_RETRY_TIMES;
		else
			level_retry_cnt = 0;

#if (CONFIG_ECBATT_CACHE == 1)
		PEGA_DBG_L("[ECBATT] %s, retry=%d, level=%d.\n",
			__func__, level_retry_cnt, level);
		ecbatt_cached_update(ECBATT_CACHED_LEVEL, level);
		pre_level = ecbatt_avg_level(level);
#else  /* CONFIG_ECBATT_CACHE */
		sspnd_level = pre_level = ecbatt_avg_level(level);
#endif /* CONFIG_ECBATT_CACHE */
	}
	else //if (level_retry_cnt < ECBATT_MAX_RETRY_TIMES)
	{
		PEGA_DBG_H("[ECBATT_RETRY] %s, retry=%d, level=%d, return pre_level=%d or sspnd_level=%d\n",
			__func__, level_retry_cnt, level, pre_level, sspnd_level);
		level_retry_cnt++;

#if (CONFIG_ECBATT_GET_BATDATA == 1)
		PEGA_DBG_L("[ECBATT] %s, call ecbatt_batdata_reset.\n", __func__);
		ecbatt_batdata_reset(NULL);
#endif /* CONFIG_ECBATT_GET_BATDATA */
		if (-1 == pre_level)
			pre_level = (-1 == sspnd_level) ? re_rsoc : sspnd_level;

#if (CONFIG_ECBATT_CACHE == 1)
		ecbatt_cached_reset(NULL);
#endif /* CONFIG_ECBATT_CACHE */
	}
	PEGA_DBG_L("[ECBATT] %s, return UI level= %d\n", __func__, pre_level);

	if (-1 == pre_level)
		pre_level = 0;

#if (CONFIG_ECBATT_DUMMY_ENABLE == 1)
	if (1 == di->is_dummy_batt && ECBATT_DUMMY_LOWEST_LEVEL > pre_level)
		pre_level = ECBATT_DUMMY_LOWEST_LEVEL;
#endif /* CONFIG_ECBATT_DUMMY_ENABLE */

	return pre_level;
#endif /* CONFIG_ECBATT_RETRY */
}

/*
 * Return the battery charging status
 * Or < 0 if something fails.
 */
static int ec_battery_status(struct ecbatt_device_info *di)
{
	int status = POWER_SUPPLY_STATUS_UNKNOWN;
	int ret;

	if (!ec_is_battery_gauge_ready() || di->is_batt_dead)
		return POWER_SUPPLY_STATUS_UNKNOWN;

	ret = ec_battery_level(di);
	if (ret >= ECBATT_FULL_CAPACITY_VAL)
		status = POWER_SUPPLY_STATUS_FULL;


	if (SUCCESS == get_ec_charging_req_status(&ret))
	{
		int type = 0;
		get_ac_type(&type);

		if (CHARGING_ON == ret &&
		    (AC_5V_IN == type || AC_12V_IN == type || AC_INFORMAL == type))
		{
			// if the status is full, still keep full status.
			if (POWER_SUPPLY_STATUS_UNKNOWN == status)
				status = POWER_SUPPLY_STATUS_CHARGING;
		}
		else
			status = POWER_SUPPLY_STATUS_DISCHARGING;
	}

	PEGA_DBG_L("[ECBATT] %s, status= %d\n", __func__, status);
	return status;
}

/*
 * Return the battery health status
 * Or < 0 if something fails.
 */
static int ec_battery_health(struct ecbatt_device_info *di)
{
	int ret, status;

	if (!ec_is_battery_gauge_ready())
		return POWER_SUPPLY_HEALTH_UNKNOWN;

	if (SUCCESS == dock_status(&status) && (BATTERY_ERR & status))
		return POWER_SUPPLY_HEALTH_DEAD;

	ret = ec_battery_temperature(di);
	if (ret <= ECBATT_HEALTH_COLD)
		return POWER_SUPPLY_HEALTH_COLD;
	else if (ret >= ECBATT_HEALTH_HEAT)
		return POWER_SUPPLY_HEALTH_OVERHEAT;

	ret = ec_battery_voltage(di);
	if (ret >= ECBATT_INTERRUPT_OVERVOLT)
		return POWER_SUPPLY_HEALTH_OVERVOLTAGE;

	return POWER_SUPPLY_HEALTH_GOOD;
}

/*
 * Return the charging type
 * Or 0 if something fails.
 */
static int ec_battery_charge_type(struct ecbatt_device_info *di)
{
	int flags = 0;
	int status = POWER_SUPPLY_CHARGE_TYPE_NONE;
	int ret;

	if (!ec_is_battery_gauge_ready())
		return POWER_SUPPLY_CHARGE_TYPE_NONE;

	ret = get_ec_charging_req_status(&flags);
	if (SUCCESS == ret)
	{
		if (CHARGING_ON == flags)
		{
			// TODO: Use EC API to check trickle charging mode instead if ready.
			if (ec_battery_voltage(di) <= 3072)
				status = POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
			else
				status = POWER_SUPPLY_CHARGE_TYPE_FAST;
		}
		else
			status = POWER_SUPPLY_CHARGE_TYPE_NONE;
	}

	PEGA_DBG_L("[ECBATT] %s, status= %d\n", __func__, status);
	return status;
}

/*
 * Return the charging current of the battery (mA)
 * Or 0 if something fails.
 */
static int ec_battery_charge_current(struct ecbatt_device_info *di)
{
	int info = 0;
	int ret;

	if (!ec_is_battery_gauge_ready())
		return 0;

	ret = get_ec_charging_info(&info, CHG_CHG_CURR);
	if (FAIL == ret)
		info = 0;

	PEGA_DBG_L("[ECBATT] %s, info= %d\n", __func__, info);
	return info;
}

/*
 * Return the input current of the battery (mA)
 * Or 0 if something fails.
 */
static int ec_battery_input_current(struct ecbatt_device_info *di)
{
	int info = 0;
	int ret;

	if (!ec_is_battery_gauge_ready())
		return 0;

	ret = get_ec_charging_info(&info, CHG_INPUT_CURR);
	if (FAIL == ret)
		info = 0;

	PEGA_DBG_L("[ECBATT] %s, info= %d\n", __func__, info);
	return info;
}

static int ecbatt_get_battery_mvolts(void)
{
	int ret = 0;
	mutex_lock(&ecbatt_mutex);
	ret = ec_battery_voltage(ecbatt_di);
	mutex_unlock(&ecbatt_mutex);
	return ret;
}

static int ecbatt_get_battery_curr(void)
{
	int ret = 0;
	mutex_lock(&ecbatt_mutex);
	ret = ec_battery_current(ecbatt_di);
	mutex_unlock(&ecbatt_mutex);
	return ret;
}

static int ecbatt_get_battery_temperature(void)
{
	int ret = 0;
	mutex_lock(&ecbatt_mutex);
	ret = ec_battery_temperature(ecbatt_di);
	mutex_unlock(&ecbatt_mutex);
	return ret;
}

static int ecbatt_get_battery_status(void)
{
	int ret = 0;
	mutex_lock(&ecbatt_mutex);
	ret = ec_battery_status(ecbatt_di);
	mutex_unlock(&ecbatt_mutex);
	return ret;
}

static int ecbatt_get_battery_capacity(void)
{
	int ret = 0;
	mutex_lock(&ecbatt_mutex);
	ret = ec_battery_rsoc(ecbatt_di);
	mutex_unlock(&ecbatt_mutex);
	return ret;
}

static int ecbatt_get_battery_health(void)
{
	int ret = 0;
	mutex_lock(&ecbatt_mutex);
	ret = ec_battery_health(ecbatt_di);
	mutex_unlock(&ecbatt_mutex);
	return ret;
}

static char* ecbatt_get_battery_fw_ver(void)
{
	return ec_battery_fw_version(ecbatt_di);
}

static int ecbatt_get_battery_level(void)
{
	int ret = 0;
	mutex_lock(&ecbatt_mutex);
	ret = ec_battery_level(ecbatt_di);
	mutex_unlock(&ecbatt_mutex);
	return ret;
}

static int ecbatt_get_battery_rm(void)
{
	int ret = 0;
	mutex_lock(&ecbatt_mutex);
	ret = ec_battery_rm(ecbatt_di);
	mutex_unlock(&ecbatt_mutex);
	return ret;
}

static int ecbatt_get_battery_fcc(void)
{
	int ret = 0;
	mutex_lock(&ecbatt_mutex);
	ret = ec_battery_fcc(ecbatt_di);
	mutex_unlock(&ecbatt_mutex);
	return ret;
}

static int ecbatt_get_charge_type(void)
{
	return ec_battery_charge_type(ecbatt_di);
}

static int ecbatt_get_charge_current(void)
{
	return ec_battery_charge_current(ecbatt_di);
}

static int ecbatt_get_input_current(void)
{
	return ec_battery_input_current(ecbatt_di);
}

static int ecbatt_is_battery_present(void)
{
	int ret = 1;
	if (ec_is_battery_gauge_ready())
	{
		int type;
		if (SUCCESS == get_ac_type(&type))
		{
			if (AC_5V_IN == type)
				ret = AC_5V_IN;
			else if (AC_12V_IN == type)
				ret = AC_12V_IN;
			else if (USB_5V_IN == type)
				ret = USB_5V_IN;
		}
	}
	else
		ret = 0;

	PEGA_DBG_H("[ECBATT] %s, ret=%d.\n", __func__, ret);

	return ret;
}

static int ecbatt_is_dock_temp_within_range(void)
{
	return 1;
}

static int ecbatt_is_battery_id_valid(void)
{
	return 1;
}

static int ecbatt_is_battery_gauge_ready(void)
{
	return ec_is_battery_gauge_ready();
}

void ecbatt_suspend(void)
{
	ecbatt_cached_suspend();
}

static struct msm_battery_gauge ecbatt_gauge = {
	.get_battery_mvolts       = ecbatt_get_battery_mvolts,
	.get_battery_temperature  = ecbatt_get_battery_temperature,
	.is_battery_present       = ecbatt_is_battery_present,
	.is_battery_temp_within_range  = ecbatt_is_dock_temp_within_range,
	.is_battery_id_valid      = ecbatt_is_battery_id_valid,
	.is_battery_gauge_ready   = ecbatt_is_battery_gauge_ready,
	.get_battery_status       = ecbatt_get_battery_status,
	.get_batt_remaining_capacity   = ecbatt_get_battery_capacity,
	.get_battery_health       = ecbatt_get_battery_health,
	.get_battery_current      = ecbatt_get_battery_curr,
	.get_battery_fw_ver       = ecbatt_get_battery_fw_ver,
	.get_battery_level        = ecbatt_get_battery_level,
	.get_battery_rm           = ecbatt_get_battery_rm,
	.get_battery_fcc          = ecbatt_get_battery_fcc,
	.get_charge_type          = ecbatt_get_charge_type,
	.get_charge_current       = ecbatt_get_charge_current,
	.get_input_current        = ecbatt_get_input_current,
	.set_batt_suspend         = ecbatt_suspend,
};

static int ecbatt_read_ec(int reg, int *rt_value)
{
	return get_ec_bat_info(rt_value, reg);
}

#if (CONFIG_ECBATT_GET_BATDATA == 1)
static int ecbatt_read_ec_batt_data(int reg, int *rt_value)
{
	int ret = 0;
	if (!ec_is_battery_gauge_ready())
	{
		PEGA_DBG_L("[ECBATT] %s, dock is not ready.\n", __func__);
		return FAIL;
	}

	if (reg < ECBATT_DATA_VOLT && reg >= NUM_OF_ECBATT_DATA)
	{
		*rt_value = 0;
		return FAIL;
	}

	mutex_lock(&batt_data.mutex_lock);
	PEGA_DBG_L("[ECBATT] %s is called, need_update=%d.\n",
			__func__, batt_data.need_update);
	if (batt_data.need_update)
	{
		struct bat_data ec_batdata;
		unsigned long flag;

		msleep(100);
		ret = get_ec_battery_data(&ec_batdata);
		if (SUCCESS == ret)
		{
			unsigned long curr_msec = jiffies_to_msecs(jiffies);
			spin_lock_irqsave(&batt_data.lock, flag);
			batt_data.val[ECBATT_DATA_VOLT] = ec_batdata.voltage;
			batt_data.val[ECBATT_DATA_SOC] = ec_batdata.relative_state_of_charge;
			batt_data.val[ECBATT_DATA_TEMP] = ec_batdata.tempture;
			batt_data.val[ECBATT_DATA_FLAG] = ec_batdata.flag;
			batt_data.val[ECBATT_DATA_RM] = ec_batdata.remain_capacity;
			batt_data.val[ECBATT_DATA_FCC] = ec_batdata.full_charge_capacity;
			batt_data.val[ECBATT_DATA_AI] = ec_batdata.average_current;
			batt_data.need_update = 0;
			batt_data.reset_delay_on = 1;
			spin_unlock_irqrestore(&batt_data.lock, flag);
			schedule_delayed_work(&batt_data.reset_delay,
						ECBATT_DATA_RESET_DELAY);
			PEGA_DBG_M("[ECBATT] %s, batt_data is updated, curr_msec=%lu, volt=%d, rsoc=%d, temp=%d, flag=0x%04x, rm=%d, fcc=%d, ai=%d.\n",
					__func__,
					curr_msec,
					batt_data.val[ECBATT_DATA_VOLT],
					batt_data.val[ECBATT_DATA_SOC],
					batt_data.val[ECBATT_DATA_TEMP],
					batt_data.val[ECBATT_DATA_FLAG],
					batt_data.val[ECBATT_DATA_RM],
					batt_data.val[ECBATT_DATA_FCC],
					batt_data.val[ECBATT_DATA_AI]);
		}
		else
		{
			PEGA_DBG_H("[ECBATT_ERR] %s, fail to get bat_data from ec.\n",
					__func__);
			batt_data.need_update = 1;
		}
	}
	mutex_unlock(&batt_data.mutex_lock);

	*rt_value = batt_data.val[reg];
	return SUCCESS;
}
#endif /* CONFIG_ECBATT_GET_BATDATA */

void pega_ecbatt_dock_in(void)
{
	int ret = 0, val = 0;

	if (!ec_is_battery_gauge_ready())
		return;

	udelay(66);
	ret = ecbatt_read(ECBATT_REG_DF_VER, &val, ecbatt_di);
	if (SUCCESS == ret)
		ecbatt_di->df_ver = val;

	udelay(66);
	ret = ecbatt_read(ECBATT_REG_FW_VER, &val, ecbatt_di);
	if (SUCCESS == ret)
		ecbatt_di->fw_ver = val;

#if (CONFIG_ECBATT_DUMMY_ENABLE == 1)
	if (SUCCESS == ret && 0 == ecbatt_di->df_ver)
		ecbatt_di->is_dummy_batt = 1;
	else
		ecbatt_di->is_dummy_batt = 0;
#endif /* CONFIG_ECBATT_DUMMY_ENABLE */
}

void pega_ecbatt_dock_out(void)
{
	ecbatt_di->fw_ver = -1;
	ecbatt_di->df_ver = -1;
#if (CONFIG_ECBATT_DUMMY_ENABLE == 1)
	ecbatt_di->is_dummy_batt = 0;
#endif /* CONFIG_ECBATT_DUMMY_ENABLE */
}

int pega_ecbatt_init(void)
{
	struct ecbatt_device_info *di;
	struct ecbatt_access_methods *bus;
	int retval = 0;

	PEGA_DBG_L("[ECBATT] %s\n", __func__);

	di = kzalloc(sizeof(*di), GFP_KERNEL);
	if (!di)
	{
		PEGA_DBG_H("[ECBATT_ERR] failed to allocate device info data\n");
		retval = -ENOMEM;
		goto batt_failed_1;
	}

	PEGA_DBG_L("[ECBATT] %s, bus = kzalloc\n", __func__);
	bus = kzalloc(sizeof(*bus), GFP_KERNEL);
	if (!bus)
	{
		PEGA_DBG_H("[ECBATT_ERR] failed to allocate access method data\n");
		retval = -ENOMEM;
		goto batt_failed_2;
	}

	bus->read = &ecbatt_read_ec;
#if (CONFIG_ECBATT_GET_BATDATA == 1)
	bus->read_batdata = &ecbatt_read_ec_batt_data;
#endif /* CONFIG_ECBATT_GET_BATDATA */
	di->bus = bus;
	di->fw_ver = -1;
	di->df_ver = -1;

#if (CONFIG_ECBATT_DUMMY_ENABLE == 1)
	di->is_dummy_batt = 0;
#endif /* CONFIG_ECBATT_DUMMY_ENABLE */

	ecbatt_di = di;
#if (CONFIG_ECBATT_RETRY == 1)
#if (CONFIG_ECBATT_CACHE == 1)
	ecbatt_cached_init();
#else  /* CONFIG_ECBATT_CACHE */
	pre_level = pre_rsoc = pre_volt = pre_temp = -1;
#endif /* CONFIG_ECBATT_CACHE */
#endif /* CONFIG_ECBATT_RETRY */

#if (CONFIG_ECBATT_GET_BATDATA == 1)
	ecbatt_batdata_init();
#endif /* CONFIG_ECBATT_GET_BATDATA */

	pega_ecbatt_gauge_register(&ecbatt_gauge);
	return 0;

/*batt_failed_3:
	kfree(bus);*/
batt_failed_2:
	kfree(di);
batt_failed_1:

	return retval;
}

void pega_ecbatt_exit(void)
{
	PEGA_DBG_L("[ECBATT] %s\n", __func__);
	kfree(ecbatt_di->bus);
	kfree(ecbatt_di);
	pega_ecbatt_gauge_unregister(&ecbatt_gauge);
}

