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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/err.h>
//#include <linux/mfd/pmic8058.h>
#include <linux/interrupt.h>
#include <linux/power_supply.h>
#include <linux/delay.h>
#include <linux/bitops.h>
#include <linux/debugfs.h>
#include <linux/msm-charger.h>
#include <linux/time.h>
#include <linux/slab.h>
#include <linux/wakelock.h>
/* BEGIN [Jackal_Chen][2012/01/11][Include gpio.h that we can get gpio status] */
#include <linux/gpio.h>
#include <linux/mfd/pm8xxx/pm8921.h>
/* END [Jackal_Chen][2012/01/11][Include gpio.h that we can get gpio status] */

#include <asm/atomic.h>

/* BEGIN [Terry_Tzeng][2012/02/15][Include regulator interface to control regulator 42 in dock in statement] */
#ifdef MSM_CHARGER_ENABLE_DOCK
#include <linux/regulator/consumer.h>
#include <linux/nuvec_api.h>
/* BEGIN [Terry_Tzeng][2012/03/29][Include early suspend resource] */
#include <linux/earlysuspend.h>
/* END [Terry_Tzeng][2012/03/29][Include early suspend resource] */
#endif /* MSM_CHARGER_ENABLE_DOCK */
/* END [Terry_Tzeng][2012/02/15][Include regulator interface to control regulator 42 in dock in statement] */
/* BEGIN [Jackal_Chen][2012/04/09][Use HW_ID value to distinguish GPIO number of dock_in pin] */
#include <asm/setup.h>
/* END [Jackal_Chen][2012/04/09][Use HW_ID value to distinguish GPIO number of dock_in pin] */

/* BEGIN [Tim_PH][2012/05/03][For dynamically log mechanism] */
#include <linux/msm-charger-log.h>
/* END [Tim_PH][2012/05/03][For dynamically log mechanism] */

//#include <mach/msm_hsusb.h>

#define MSM_CHG_MAX_EVENTS		16
#define CHARGING_TEOC_MS		9000000
#define UPDATE_TIME_MS			60000
#define RESUME_CHECK_PERIOD_MS		60000

#define DEFAULT_BATT_MAX_V		4200
#define DEFAULT_BATT_MIN_V		3200
#define DEFAULT_BATT_RESUME_V		4100

#define MSM_CHARGER_GAUGE_MISSING_VOLTS 4000
#define MSM_CHARGER_GAUGE_MISSING_TEMP  350
#define MSM_CHARGER_GAUGE_MISSING_CURR  0

#define MSM_CHARGER_ENABLE_GAUGE_REMAP  1

/* BEGIN [Terry_Tzeng][2012/01/09][Add dock resource marco] */
#ifdef MSM_CHARGER_ENABLE_DOCK
#define MSM_CHARGER_GAUGE_MISSING_VOLTS 4000
#define MSM_CHARGER_GAUGE_MISSING_TEMP  350
#define MSM_CHARGER_GAUGE_MISSING_CURR  0
#endif /* MSM_CHARGER_ENABLE_DOCK */
/* END [Terry_Tzeng][2012/01/09][Add dock resource marco] */

/**
 * enum msm_battery_status
 * @BATT_STATUS_ABSENT: battery not present
 * @BATT_STATUS_ID_INVALID: battery present but the id is invalid
 * @BATT_STATUS_DISCHARGING: battery is present and is discharging
 * @BATT_STATUS_TRKL_CHARGING: battery is being trickle charged
 * @BATT_STATUS_FAST_CHARGING: battery is being fast charged
 * @BATT_STATUS_JUST_FINISHED_CHARGING: just finished charging,
 *		battery is fully charged. Do not begin charging untill the
 *		voltage falls below a threshold to avoid overcharging
 * @BATT_STATUS_TEMPERATURE_OUT_OF_RANGE: battery present,
					no charging, temp is hot/cold
 */
enum msm_battery_status {
	BATT_STATUS_ABSENT,
	BATT_STATUS_ID_INVALID,
	BATT_STATUS_DISCHARGING,
	BATT_STATUS_TRKL_CHARGING,
	BATT_STATUS_FAST_CHARGING,
	BATT_STATUS_JUST_FINISHED_CHARGING,
	BATT_STATUS_TEMPERATURE_OUT_OF_RANGE,
};

struct msm_charger_event {
	enum msm_hardware_charger_event event;
	struct msm_hardware_charger *hw_chg;
};

struct msm_charger_mux {
	int inited;
	struct list_head msm_hardware_chargers;
	int count_chargers;
	struct mutex msm_hardware_chargers_lock;

	struct device *dev;

	unsigned int max_voltage;
	unsigned int min_voltage;

	unsigned int safety_time;
	/* BEGIN [Jackal_Chen][2012/04/23][Modify safety time of wall charger] */
	unsigned int wall_safety_time;
	/* END [Jackal_Chen][2012/04/23][Modify safety time of wall charger] */
	unsigned long usb_safety_time;
	struct delayed_work teoc_work;

	int safety_stop_charging_count;
	int stop_resume_check;
	int resume_count;
	struct delayed_work resume_work;

	// BEGIN [2012/07/12] Jackal - Modify to fix AC charger unstable bug
#if(MSM_CHARGER_ENABLE_OTG_CHECK)
	struct delayed_work wait_otg_report_work;
#endif
	// END [2012/07/12] Jackal - Modify to fix AC charger unstable bug

	unsigned int update_time;
	int stop_update;
	struct delayed_work update_heartbeat_work;

	struct mutex status_lock;
	enum msm_battery_status batt_status;
/* BEGIN [Terry_Tzeng][2012/01/09][Add dock resource marco] */
#ifdef MSM_CHARGER_ENABLE_DOCK
	enum msm_battery_status ecbatt_status;
	struct regulator *regulator42_ctl_api;		// Control tablet 5V boost power switch for dock in statement
	/* BEGIN [Terry_Tzeng][2012/03/29][Add early suspend function for inserted dock charging ] */
	struct early_suspend  charge_early_suspend;
	/* END [Terry_Tzeng][2012/03/29][Add early suspend function for inserted dock charging ] */
#endif /* MSM_CHARGER_ENABLE_DOCK */
/* END [Terry_Tzeng][2012/01/09][Add dock resource marco] */
	struct msm_hardware_charger_priv *current_chg_priv;
	struct msm_hardware_charger_priv *current_mon_priv;

	unsigned int (*get_batt_capacity_percent) (void);

	struct msm_charger_event *queue;
	int tail;
	int head;
	spinlock_t queue_lock;
	int queue_count;
	struct work_struct queue_work;
	struct workqueue_struct *event_wq_thread;
	struct wake_lock wl;

	struct mutex chg_batt_lock;

};

// [2012/01/03] Jackal - Fix sharging null pointer bug
static spinlock_t usb_chg_lock;
// [2012/01/03] Jackal - Fix sharging null pointer bug End

/* BEGIN [Jackal_Chen][2012/01/16][Add flag of dock in/out] */
static int mDockIn = 0;
/* END [Jackal_Chen][2012/01/16][Add flag of dock in/out] */

/* BEGIN [Jackal_Chen][2012/04/09][Use HW_ID value to distinguish GPIO number of dock_in pin] */
static int dock_in_gpio_number = MSM8960_DOCK_IN_GPIO;    //  Default = 7(for EVT/DVT/PVT)
/* END [Jackal_Chen][2012/04/09][Use HW_ID value to distinguish GPIO number of dock_in pin] */

static struct msm_charger_mux msm_chg;

static struct msm_battery_gauge *msm_batt_gauge;

/* BEGIN [Terry_Tzeng][2012/01/09][Add dock resource marco] */
#ifdef MSM_CHARGER_ENABLE_DOCK
static struct msm_battery_gauge *pega_ecbatt_gauge;
static struct msm_ec_dock *msm_ec_dock;

/* BEGIN [Terry_Tzeng][2012/03/28][Define early suspend function for charging entried suspend funciton] */
void msm_chg_early_suspend(struct early_suspend *h);
void msm_chg_late_resume(struct early_suspend *h);
/* BEGIN [Terry_Tzeng][2012/03/28][Define early suspend function for charging entried suspend funciton] */
#endif /* MSM_CHARGER_ENABLE_DOCK */
/* END [Terry_Tzeng][2012/01/09][Add dock resource marco] */

static int is_status_lock_init = 0;
static int check_charger_type(void);

DEFINE_MUTEX(bus_mutex);

void pega_power_enter_cs()
{
	mutex_lock(&bus_mutex);
}

void pega_power_exit_cs()
{
	mutex_unlock(&bus_mutex);
}

static int is_chg_capable_of_charging(struct msm_hardware_charger_priv *priv)
{
	PEGA_DBG_H("[CHG] %s, hw_chg_state= %d\n", __func__, priv->hw_chg_state);
	if (priv->hw_chg_state == CHG_READY_STATE
	    || priv->hw_chg_state == CHG_CHARGING_STATE)
		return 1;

	return 0;
}

static int is_batt_status_capable_of_charging(void)
{
	PEGA_DBG_L("[CHG] %s, batt_status= %d\n", __func__,msm_chg.batt_status);
	if (msm_chg.batt_status == BATT_STATUS_ABSENT
	    || msm_chg.batt_status == BATT_STATUS_TEMPERATURE_OUT_OF_RANGE
	    || msm_chg.batt_status == BATT_STATUS_ID_INVALID)
	 //   || msm_chg.batt_status == BATT_STATUS_JUST_FINISHED_CHARGING)
		return 0;
	return 1;
}

static int is_batt_status_charging(void)
{
	struct msm_hardware_charger_priv *priv;
	priv = msm_chg.current_chg_priv;

	if (priv)
	{
		if (priv->max_source_current == MAX_CHG_CURR_USB)
			return 0;
	}

	PEGA_DBG_L("[CHG] %s, batt_status= %d\n", __func__,msm_chg.batt_status);
	if (msm_chg.batt_status == BATT_STATUS_TRKL_CHARGING
	    || msm_chg.batt_status == BATT_STATUS_FAST_CHARGING)
		return 1;
	return 0;
}

static int is_battery_gauge_ready(void)
{
	int ret = 0;
	if (msm_batt_gauge && msm_batt_gauge->is_battery_gauge_ready)
	{
		ret = msm_batt_gauge->is_battery_gauge_ready();
	}
	return ret;
	PEGA_DBG_L("[CHG] %s = %d\n", __func__, ret);
}

static int is_battery_present(void)
{
	int ret = 1;

	if (is_battery_gauge_ready())
	{
		ret = msm_batt_gauge->is_battery_present();
		PEGA_DBG_L("[CHG] %s = %d\n", __func__, ret);
	}
	else
	{
		PEGA_DBG_L("[CHG] %s, no gauge assuming batt_present = 1\n", __func__);
	}
	return ret;
}

static int is_battery_temp_within_range(void)
{
	int ret = 1;

	if (is_battery_gauge_ready())
	{
		ret = msm_batt_gauge->is_battery_temp_within_range();
		PEGA_DBG_L("[CHG] %s = %d\n", __func__, ret);
	}
	else
	{
		PEGA_DBG_L("[CHG] %s, no gauge assuming temp_within_range = 1\n", __func__);
	}
	return ret;
}

static int is_battery_id_valid(void)
{
	int ret = 1;

	if (is_battery_gauge_ready())
	{
		ret = msm_batt_gauge->is_battery_id_valid();
		PEGA_DBG_L("[CHG] %s = %d\n", __func__, ret);
	}
	else
	{
		PEGA_DBG_L("[CHG] %s, no gauge assuming id_valid = 1\n", __func__);
	}
	return ret;
}

static char* get_prop_batt_fw_version(void)
{
	char* ret = "invalid";

	if (msm_batt_gauge && msm_batt_gauge->get_battery_fw_ver)
	{
		ret = msm_batt_gauge->get_battery_fw_ver();
		PEGA_DBG_L("[CHG] %s, from gauge: %s\n", __func__, ret);
	}
	else
	{
		PEGA_DBG_L("[CHG] %s, no gauge assuming %s\n", __func__,ret);
	}
	return ret;
}

static int get_prop_batt_mvolts(void)
{
#ifdef PEGA_CHG_FAKE_BATT_ENABLE
	int ret = MSM_CHARGER_GAUGE_MISSING_VOLTS;
#else  /* PEGA_CHG_FAKE_BATT_ENABLE */
	int ret = 0;
#endif /* PEGA_CHG_FAKE_BATT_ENABLE */

	if (msm_batt_gauge && msm_batt_gauge->get_battery_mvolts)
	{
		ret = msm_batt_gauge->get_battery_mvolts();
		PEGA_DBG_L("[CHG] %s, from gauge: %d\n", __func__, ret);
	}
	else
	{
		PEGA_DBG_L("[CHG] %s, no gauge assuming %d mV\n", __func__,ret);
	}
	return ret;
}

static int get_prop_batt_temperature(void)
{
#ifdef PEGA_CHG_FAKE_BATT_ENABLE
	int ret = MSM_CHARGER_GAUGE_MISSING_TEMP;
#else  /* PEGA_CHG_FAKE_BATT_ENABLE */
	int ret = 0;
#endif /* PEGA_CHG_FAKE_BATT_ENABLE */

	if (msm_batt_gauge && msm_batt_gauge->get_battery_temperature)
	{
		ret = msm_batt_gauge->get_battery_temperature();
		PEGA_DBG_L("[CHG] %s, from gauge: %d\n", __func__, ret);
	}
	else
	{
		PEGA_DBG_L("[CHG] %s, no gauge assuming %d deg C\n", __func__, ret);
	}
	return ret;
}

static int get_prop_batt_chg_current(void)
{
	int ret = 0;
	struct msm_hardware_charger_priv *priv;

	if (!msm_chg.current_chg_priv)
		return ret;

	priv = msm_chg.current_chg_priv;
	ret = priv->hw_chg->get_in_current(priv->hw_chg) * 10000 +
	      priv->hw_chg->get_chg_current(priv->hw_chg);

	PEGA_DBG_L("[CHG] %s, name: %s, charging current= %d.\n",
		__func__, priv->hw_chg->name, ret);
	return ret;
}

static int get_prop_batt_current(void)
{
	int ret = 0;

	if (msm_batt_gauge && msm_batt_gauge->get_battery_current)
	{
		ret = msm_batt_gauge->get_battery_current();
		PEGA_DBG_L("[CHG] %s, from gauge: %d\n", __func__, ret);
	}
	else
	{
		PEGA_DBG_L("[CHG] %s, no gauge assuming %d mA\n", __func__, ret);
	}
	return ret;
}

static int get_prop_batt_capacity(void)
{
	int ret = 0;

#ifdef MSM_CHARGER_ENABLE_GAUGE_REMAP
	if (msm_batt_gauge && msm_batt_gauge->get_battery_level)
	{
		ret = msm_batt_gauge->get_battery_level();
#else  /* MSM_CHARGER_ENABLE_GAUGE_REMAP */
	if (msm_batt_gauge && msm_batt_gauge->get_batt_remaining_capacity)
	{
		ret = msm_batt_gauge->get_batt_remaining_capacity();
#endif /* MSM_CHARGER_ENABLE_GAUGE_REMAP */
		PEGA_DBG_L("[CHG] %s, from gauge: %d\n", __func__, ret);
	}
	else
	{
		PEGA_DBG_L("[CHG] %s, no gauge assuming %d\n", __func__, ret);
	}

	if (-1 == ret)
	{
		PEGA_DBG_H("[CHG_ERR] %s, level is -1.\n", __func__);
		return 0;
	}
	return ret;
}

static int get_prop_batt_fcc(void)
{
	int ret = 0;

	if (msm_batt_gauge && msm_batt_gauge->get_battery_fcc)
	{
		ret = msm_batt_gauge->get_battery_fcc();
		PEGA_DBG_L("[CHG] %s, from gauge: %d\n", __func__, ret);
	}
	else
	{
		PEGA_DBG_L("[CHG] %s, no gauge assuming %d\n", __func__, ret);
	}
	return ret;
}

static int get_prop_batt_rm(void)
{
	int ret = 0;

	if (msm_batt_gauge && msm_batt_gauge->get_battery_rm)
	{
		ret = msm_batt_gauge->get_battery_rm();
		PEGA_DBG_L("[CHG] %s, from gauge: %d\n", __func__, ret);
	}
	else
	{
		PEGA_DBG_L("[CHG] %s, no gauge assuming %d\n", __func__, ret);
	}
	return ret;
}

static int get_prop_batt_health(void)
{
	int status = 0;

	if (msm_batt_gauge && msm_batt_gauge->get_battery_health)
	{
		status = msm_batt_gauge->get_battery_health();
		PEGA_DBG_L("[CHG] %s, from gauge: %d\n", __func__, status);
	}
	else
	{
		PEGA_DBG_L("[CHG] %s, no gauge assuming %d\n", __func__, status);
	}
	return status;
}

static int get_prop_charge_type(void)
{
	int status = POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;

	if (msm_chg.batt_status == BATT_STATUS_TRKL_CHARGING)
		status = POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
	else if (msm_chg.batt_status == BATT_STATUS_FAST_CHARGING)
		status = POWER_SUPPLY_CHARGE_TYPE_FAST;
	else
		status = POWER_SUPPLY_CHARGE_TYPE_NONE;
	PEGA_DBG_L("[CHG] %s, from msm_chg: %d\n", __func__, msm_chg.batt_status);

	return status;
}

static int get_prop_batt_status(void)
{
	int status = POWER_SUPPLY_STATUS_UNKNOWN;

	PEGA_DBG_L("[CHG] %s, enter\n", __func__);
	if (!is_battery_gauge_ready())
	{
		return POWER_SUPPLY_STATUS_UNKNOWN;
	}

	if (msm_batt_gauge && msm_batt_gauge->get_battery_status)
	{
		struct msm_hardware_charger_priv *priv;
		priv = msm_chg.current_chg_priv;

		if ((NULL == priv) ||
		    (CHG_TYPE_USB == check_charger_type() &&
		     MAX_CHG_CURR_USB_WALL != priv->max_source_current))
		{
			return POWER_SUPPLY_STATUS_DISCHARGING;
		}

		status = msm_batt_gauge->get_battery_status();
		if (POWER_SUPPLY_STATUS_FULL == status)
		{
			if (!is_batt_status_charging() &&
			    BATT_STATUS_JUST_FINISHED_CHARGING != msm_chg.batt_status)
			{
				status = POWER_SUPPLY_STATUS_DISCHARGING;
			}
			PEGA_DBG_L("[CHG] %s, from gauge: %d\n", __func__, status);
		}
		else
		{
			if (is_batt_status_charging() &&
			    priv->hw_chg->get_in_current(priv->hw_chg) > MAX_CHG_CURR_USB)
			{
				status = POWER_SUPPLY_STATUS_CHARGING;
			}
			else
			{
				status = POWER_SUPPLY_STATUS_DISCHARGING;
			}
			PEGA_DBG_L("[CHG] %s, from msm_chg: %d\n", __func__, status);
		}
	}
	else
	{
		PEGA_DBG_L("[CHG] %s, no gauge assuming unknown\n", __func__);
	}
	PEGA_DBG_L("[CHG] %s, status: %d, exit\n", __func__, status);
	return status;
}

/* BEGIN [Terry_Tzeng][2012/01/09][Add dock resource function] */
#ifdef MSM_CHARGER_ENABLE_DOCK
/*static int is_ecbatt_status_charging(void)
{
	struct msm_hardware_charger_priv *priv;
	priv = msm_chg.current_chg_priv;

	if (priv)
	{
		if (priv->max_source_current == MAX_CHG_CURR_USB)
		return 0;
	}

	PEGA_DBG_L("[CHG] %s, batt_status= %d\n", __func__,msm_chg.ecbatt_status);
	if (msm_chg.ecbatt_status == BATT_STATUS_TRKL_CHARGING
	    || msm_chg.ecbatt_status == BATT_STATUS_FAST_CHARGING)
		return 1;
	return 0;
}*/

static int is_ecbatt_ready(void)
{
	int ret = 0;
	if (pega_ecbatt_gauge && pega_ecbatt_gauge->is_battery_gauge_ready)
	{
		ret = pega_ecbatt_gauge->is_battery_gauge_ready();
	}
	return ret;
	PEGA_DBG_L("[CHG] %s = %d\n", __func__, ret);
}

static int is_ecbatt_present(void)
{
	int ret = 0;

	if (!is_ecbatt_ready())
		return 0;

	if (pega_ecbatt_gauge && pega_ecbatt_gauge->is_battery_present)
	{
		ret = pega_ecbatt_gauge->is_battery_present();
		PEGA_DBG_L("[CHG] %s = %d\n", __func__, ret);
	}
	else
	{
		PEGA_DBG_L("[CHG] %s, no dock assuming batt_present = 0.\n", __func__);
	}

	return ret;
}

static int is_ecbatt_temp_within_range(void)
{
	int ret = 1;

	if (!is_ecbatt_ready())
		return 0;

	if (pega_ecbatt_gauge && pega_ecbatt_gauge->is_battery_temp_within_range)
	{
		ret = pega_ecbatt_gauge->is_battery_temp_within_range();
		PEGA_DBG_L("[CHG] %s = %d\n", __func__, ret);
	}
	else
	{
		PEGA_DBG_L("[CHG] %s, no dock assuming temp_within_range = 1\n", __func__);
	}
	return ret;
}

static int is_ecbatt_id_valid(void)
{
	int ret = 1;

	if (!is_ecbatt_ready())
		return 0;

	if (pega_ecbatt_gauge && pega_ecbatt_gauge->is_battery_id_valid)
	{
		ret = pega_ecbatt_gauge->is_battery_id_valid();
		PEGA_DBG_L("[CHG] %s = %d\n", __func__, ret);
	}
	else
	{
		PEGA_DBG_L("[CHG] %s, no dock assuming id_valid = 1\n", __func__);
	}
	return ret;
}

static char* get_prop_ecbatt_fw_version(void)
{
	char* ret = "invalid";

	if (!is_ecbatt_ready())
		return NULL;

	if (pega_ecbatt_gauge && pega_ecbatt_gauge->get_battery_fw_ver)
	{
		ret = pega_ecbatt_gauge->get_battery_fw_ver();
		PEGA_DBG_L("[CHG] %s, from dock: %s\n", __func__, ret);
	}
	else
	{
		PEGA_DBG_L("[CHG] %s, no dock assuming %s\n", __func__,ret);
	}
	return ret;
}

static int get_prop_ecbatt_mvolts(void)
{
	int ret = MSM_CHARGER_GAUGE_MISSING_VOLTS;

	if (!is_ecbatt_ready())
		return 0;

	if (pega_ecbatt_gauge && pega_ecbatt_gauge->get_battery_mvolts)
	{
		ret = pega_ecbatt_gauge->get_battery_mvolts();
		PEGA_DBG_L("[CHG] %s, from dock: %d\n", __func__, ret);
	}
	else
	{
		PEGA_DBG_L("[CHG] %s, no dock assuming %d mV\n", __func__,ret);
	}
	return ret;
}

static int get_prop_ecbatt_temperature(void)
{
	int ret = MSM_CHARGER_GAUGE_MISSING_TEMP;

	if (!is_ecbatt_ready())
		return 0;

	if (pega_ecbatt_gauge && pega_ecbatt_gauge->get_battery_temperature)
	{
		ret = pega_ecbatt_gauge->get_battery_temperature();
		PEGA_DBG_L("[CHG] %s, from dock: %d\n", __func__, ret);
	}
	else
	{
		PEGA_DBG_L("[CHG] %s, no dock assuming %d deg C\n", __func__, ret);
	}
	return ret;
}

static int get_prop_ecbatt_chg_current(void)
{
	int ret = 0;

	if (!is_ecbatt_ready())
		return 0;

	if (pega_ecbatt_gauge &&
	    pega_ecbatt_gauge->get_input_current &&
	    pega_ecbatt_gauge->get_battery_current)
	{
		ret = pega_ecbatt_gauge->get_input_current() * 10000 +
		      pega_ecbatt_gauge->get_charge_current();
		PEGA_DBG_L("[CHG] %s, from dock: %d\n", __func__, ret);
	}
	else
	{
		PEGA_DBG_L("[CHG] %s, no dock assuming %d mA\n", __func__, ret);
	}
	return ret;
}

static int get_prop_ecbatt_current(void)
{
	int ret = 0;

	if (!is_ecbatt_ready())
		return 0;

	if (pega_ecbatt_gauge && pega_ecbatt_gauge->get_battery_current)
	{
		ret = pega_ecbatt_gauge->get_battery_current();
		PEGA_DBG_L("[CHG] %s, from dock: %d\n", __func__, ret);
	}
	else
	{
		PEGA_DBG_L("[CHG] %s, no dock assuming %d mA\n", __func__, ret);
	}
	return ret;
}

static int get_prop_ecbatt_capacity(void)
{
	int ret = 0;

	if (!is_ecbatt_ready())
		return 0;

#ifdef MSM_CHARGER_ENABLE_GAUGE_REMAP
	if (pega_ecbatt_gauge && pega_ecbatt_gauge->get_battery_level)
	{
		ret = pega_ecbatt_gauge->get_battery_level();
#else  /* MSM_CHARGER_ENABLE_GAUGE_REMAP */
	if (pega_ecbatt_gauge && pega_ecbatt_gauge->get_batt_remaining_capacity)
	{
		ret = pega_ecbatt_gauge->get_batt_remaining_capacity();
#endif /* MSM_CHARGER_ENABLE_GAUGE_REMAP */
		PEGA_DBG_L("[CHG] %s, from dock: %d\n", __func__, ret);
	}
	else
	{
		PEGA_DBG_L("[CHG] %s, no dock assuming %d\n", __func__, ret);
	}

	if (-1 == ret)
	{
		PEGA_DBG_H("[CHG_ERR] %s, level is -1.\n", __func__);
		return 0;
	}
	return ret;
}

static int get_prop_ecbatt_fcc(void)
{
	int ret = 0;

	if (!is_ecbatt_ready())
		return 0;

	if (pega_ecbatt_gauge && pega_ecbatt_gauge->get_battery_fcc)
	{
		ret = pega_ecbatt_gauge->get_battery_fcc();
		PEGA_DBG_L("[CHG] %s, from gauge: %d\n", __func__, ret);
	}
	else
	{
		PEGA_DBG_L("[CHG] %s, no gauge assuming %d\n", __func__, ret);
	}
	return ret;
}

static int get_prop_ecbatt_rm(void)
{
	int ret = 0;

	if (!is_ecbatt_ready())
		return 0;

	if (pega_ecbatt_gauge && pega_ecbatt_gauge->get_battery_rm)
	{
		ret = pega_ecbatt_gauge->get_battery_rm();
		PEGA_DBG_L("[CHG] %s, from dock: %d\n", __func__, ret);
	}
	else
	{
		PEGA_DBG_L("[CHG] %s, no function pointer assuming 0.\n", __func__);
	}
	return ret;
}

static int get_prop_ecbatt_health(void)
{
	int status = 0;

	if (!is_ecbatt_ready())
		return POWER_SUPPLY_HEALTH_UNKNOWN;

	if (pega_ecbatt_gauge && pega_ecbatt_gauge->get_battery_health)
	{
		status = pega_ecbatt_gauge->get_battery_health();
		PEGA_DBG_L("[CHG] %s, from dock: %d\n", __func__, status);
	}
	else
	{
		PEGA_DBG_L("[CHG] %s, no dock assuming %d\n", __func__, status);
	}
	return status;
}

static int get_prop_ecbatt_charge_type(void)
{
	int status = 0;

	/*if (msm_chg.ecbatt_status == BATT_STATUS_TRKL_CHARGING)
		status = POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
	else if (msm_chg.ecbatt_status == BATT_STATUS_FAST_CHARGING)
		status = POWER_SUPPLY_CHARGE_TYPE_FAST;
	else
		status = POWER_SUPPLY_CHARGE_TYPE_NONE;
	PEGA_DBG_L("[CHG] %s, from msm_chg: %d\n", __func__, msm_chg.batt_status);

	return status;*/

	if (!is_ecbatt_ready())
		return POWER_SUPPLY_CHARGE_TYPE_NONE;

	if (pega_ecbatt_gauge && pega_ecbatt_gauge->get_charge_type)
	{
		status = pega_ecbatt_gauge->get_charge_type();
	}
	else
	{
		PEGA_DBG_L("[CHG] %s, no dock assuming none.\n", __func__);
	}
	PEGA_DBG_L("[CHG] %s, status: %d, exit\n", __func__, status);
	return status;
}

static int get_prop_ecbatt_status(void)
{
	int status = 0;

	PEGA_DBG_L("[CHG] %s, enter\n", __func__);
	if (!is_ecbatt_ready())
		return POWER_SUPPLY_STATUS_UNKNOWN;

	if (pega_ecbatt_gauge && pega_ecbatt_gauge->get_battery_status)
	{
		//struct msm_hardware_charger_priv *priv;
		//priv = msm_chg.current_chg_priv;

		status = pega_ecbatt_gauge->get_battery_status();
		/*if (POWER_SUPPLY_STATUS_FULL == status)
		{
			if (!is_ecbatt_status_charging())
				status = POWER_SUPPLY_STATUS_DISCHARGING;
			PEGA_DBG_L("[CHG] %s, from dock: %d\n", __func__, status);
		}
		else
		{
			if (is_ecbatt_status_charging())
				status = POWER_SUPPLY_STATUS_CHARGING;
			else
				status = POWER_SUPPLY_STATUS_DISCHARGING;
			PEGA_DBG_L("[CHG] %s, from msm_chg: %d\n", __func__, status);
		}*/
	}
	else
	{
		PEGA_DBG_L("[CHG] %s, no dock assuming unknown\n", __func__);
	}
	PEGA_DBG_L("[CHG] %s, status: %d, exit\n", __func__, status);
	return status;
}
#endif /* MSM_CHARGER_ENABLE_DOCK */
/* END [Terry_Tzeng][2012/01/09][Add dock resource function] */

/* BEGIN [Terry_Tzeng][2012/01/16][Add interface of getting tablet battery information] */
//static int msm_chg_get_batt_mvolts(void)
int msm_chg_get_batt_mvolts(void)
{
	int ret = MSM_CHARGER_GAUGE_MISSING_VOLTS;

	if (is_battery_gauge_ready())
	{
		ret = msm_batt_gauge->get_battery_mvolts();
	}
	else
	{
		PEGA_DBG_L("[CHG] %s, no gauge assuming batt_vol= %d\n", __func__, MSM_CHARGER_GAUGE_MISSING_VOLTS);
	}
	return ret;
}
/* END [Terry_Tzeng][2012/01/16][Add interface of getting tablet battery information] */

/* BEGIN [Jackal_Chen][2012/04/10][Detect Y-cable by adc] */
int pega_get_mpp_adc(int mpp_number, int64_t *val)
{
	struct pm8xxx_adc_chan_result result;
	int rc;

	rc = pm8xxx_adc_mpp_config_read(mpp_number, ADC_MPP_1_AMUX6, &result);
	if (!rc) {
		*val = result.physical;
	} else {
		PEGA_DBG_H("Read ADC MPP value fail, rc = %d\n", rc);
		*val = -1;
	}

	return rc;
}
/* END [Jackal_Chen][2012/04/10][Detect Y-cable by adc] */

int msm_chg_get_batt_status(void)
{
	int status = 0;

	if (is_battery_gauge_ready())
	{
		status = msm_batt_gauge->get_battery_status();
	}
	else
	{
		if (is_batt_status_charging())
			status = POWER_SUPPLY_STATUS_CHARGING;
		else
			status = POWER_SUPPLY_STATUS_DISCHARGING;

		PEGA_DBG_L("[CHG] %s, no gauge, check is_batt_status_charging = %d\n",__func__,status);
	}
	return status;
}

/* BEGIN [2012/04/25] Jackal - User may charge device when battery is full but charge current not less than 300mA yet */
int msm_chg_get_batt_current(void)
{
	int ret = 0;

	if (msm_batt_gauge && msm_batt_gauge->get_battery_current)
	{
		ret = msm_batt_gauge->get_battery_current();
		PEGA_DBG_L("[CHG] %s, from gauge: %d\n", __func__, ret);
	}
	else
	{
		PEGA_DBG_L("[CHG] %s, no gauge assuming %d mA\n", __func__, ret);
	}
	return ret;
}
/* END [2012/04/25] Jackal - User may charge device when battery is full but charge current not less than 300mA yet */

/* BEGIN [Terry_Tzeng][2012/01/16][Create getting tablet battery information] */
int msm_chg_get_batt_temperture(void)
{
	int ret = MSM_CHARGER_GAUGE_MISSING_TEMP;

	if (msm_batt_gauge && msm_batt_gauge->get_battery_temperature)
	{
		ret = msm_batt_gauge->get_battery_temperature();
		PEGA_DBG_L("[CHG] %s, from gauge: %d\n", __func__, ret);
	}
	else
	{
		PEGA_DBG_L("[CHG] %s, no gauge assuming %d deg C\n", __func__, ret);
	}
	return ret;
}

int msm_chg_get_batt_remain_capacity(void)
{
	int ret = 0;

	if(is_battery_gauge_ready())
	{
		if (msm_batt_gauge && msm_batt_gauge->get_batt_remaining_capacity)
		{
			ret = msm_batt_gauge->get_batt_remaining_capacity();
			PEGA_DBG_L("[CHG] %s, from gauge: %d\n", __func__, ret);
		}
		else
		{
			PEGA_DBG_L("[CHG] %s, no gauge assuming %d\n", __func__, ret);
		}
	}

	return ret;

}

int msm_chg_get_ui_batt_capacity(void)
{
	int ret = 0;

	if(is_battery_gauge_ready())
	{
		if (msm_batt_gauge && msm_batt_gauge->get_battery_level)
		{
			ret = msm_batt_gauge->get_battery_level();
			PEGA_DBG_L("[CHG] %s, from gauge: %d\n", __func__, ret);
		}
		else
		{
			PEGA_DBG_L("[CHG] %s, no gauge assuming %d\n", __func__, ret);
		}
	}

	return ret;

}

/* BEGIN [Terry_Tzeng][2012/05/03][Create checked pad battery charging status function] */
int msm_chg_check_padbatt_chg_st(void)
{
	return is_batt_status_charging();
}
/* END [Terry_Tzeng][2012/05/03][Create checked pad battery charging status function] */

/* BEGIN [Terry_Tzeng][2012/04/27][Create checked pad battery ready function] */
int msm_chg_check_padbatt_ready(void)
{
	return is_battery_gauge_ready();
}
/* END [Terry_Tzeng][2012/04/27][Create checked pad battery ready function] */


#ifdef MSM_CHARGER_ENABLE_DOCK

/* BEGIN [Terry_Tzeng][2012/06/11][Handle informal charger type] */
int msm_chg_get_ecbatt_in_curr(void)
{
	int ret = 0;

	if (!is_ecbatt_ready())
		return 0;

	if (pega_ecbatt_gauge &&
	    pega_ecbatt_gauge->get_input_current)
	{
		ret = pega_ecbatt_gauge->get_input_current();
		PEGA_DBG_L("[CHG_EC] %s, get dock input current =  %d\n", __func__, ret);
	}
	else
	{
		PEGA_DBG_H("[CHG_EC_ERR] %s, no dock assuming %d mA\n", __func__, ret);
	}
	return ret;
}
/* END [Terry_Tzeng][2012/06/11][Handle informal charger type] */

/* BEGIN [Terry_Tzeng][2012/03/12][Create checked dock battery full status] */
int msm_chg_check_ecbatt_full(void)
{
	return get_prop_ecbatt_status();
}
/* END [Terry_Tzeng][2012/03/12][Create checked dock battery full status] */

int msm_chg_check_ecbatt_ready(void)
{
	return is_ecbatt_ready();
}

int msm_chg_get_ui_ecbatt_capacity(void)
{
	int ret = 0;

	if (!is_ecbatt_ready())
		return 0;

	if (pega_ecbatt_gauge && pega_ecbatt_gauge->get_battery_level)
	{
		ret = pega_ecbatt_gauge->get_battery_level();
		PEGA_DBG_L("[CHG] %s, from dock: %d\n", __func__, ret);
	}
	else
	{
		PEGA_DBG_L("[CHG] %s, no dock assuming %d\n", __func__, ret);
	}

	if (-1 == ret)
	{
		PEGA_DBG_H("[CHG_ERR] %s, level is -1.\n", __func__);
		return 0;
	}
	return ret;
}

int msm_chg_get_ecbatt_temperature(void)
{
	return get_prop_ecbatt_temperature();
}

int msm_chg_get_ecbatt_mvolt(void)
{
	return get_prop_ecbatt_mvolts();
}

/* BEGIN [Terry_Tzeng][2012/02/15][Create controled regulator 42 interface for dock in statement] */
int msm_chg_check_5vboost_pw_is_enabled(void)
{
	int en;

	if(NULL == msm_chg.regulator42_ctl_api)
	{
		PEGA_DBG_H("[CHG_ERR] %s, msm_chg.regulator42_ctl_api point is null !\n", __func__);
		return -1;
	}

	en = regulator_is_enabled(msm_chg.regulator42_ctl_api);
	PEGA_DBG_H("[CHG] %s, Before handled, regulator_is_enabled = %d\n", __func__, en);

	return en;
}

static bool tb_otg_5V_en_st = false;

int msm_chg_5vboost_pw_enable(void)
{
	int ret;

	if(NULL == msm_chg.regulator42_ctl_api)
	{
		PEGA_DBG_H("[CHG_EC_ERR] %s, msm_chg.regulator42_ctl_api point is null !\n", __func__);
		return -1;
	}

	if(tb_otg_5V_en_st != true)
	{
		ret = regulator_enable(msm_chg.regulator42_ctl_api);

		if (ret) {
			PEGA_DBG_H("[CHG_EC_ERR] %s, Enable regulator 42 is fail !\n", __func__);
			return -1;
		}

		PEGA_DBG_M("[CHG_EC] %s, Enable pad otg 5V enable power switch\n", __func__);
		tb_otg_5V_en_st = true;

	}

	return 0;
}

int msm_chg_5vboost_pw_disable(void)
{
	int ret;

	if(NULL == msm_chg.regulator42_ctl_api)
	{
		PEGA_DBG_H("[CHG_EC_ERR] %s, msm_chg.regulator42_ctl_api point is null !\n", __func__);
		return -1;
	}

	if(tb_otg_5V_en_st != false)
	{
		ret = regulator_disable(msm_chg.regulator42_ctl_api);
		if (ret) {
			PEGA_DBG_H("[CHG_EC_ERR] %s, Disable regulator 42 is fail !\n", __func__);
			return -1;
		}

		PEGA_DBG_M("[CHG_EC] %s, Disable pad otg 5V enable power switch\n", __func__);
		tb_otg_5V_en_st = false;
	}

	return 0;
}

/* END [Terry_Tzeng][2012/02/15][Create controled regulator 42 interface for dock in statement] */
#endif /* MSM_CHARGER_ENABLE_DOCK */
/* END [Terry_Tzeng][2012/01/16][Create getting tablet battery information] */

 /* This function should only be called within handle_event or resume */
static void update_batt_status(void)
{
	PEGA_DBG_L("[CHG] %s, msm_chg.batt_status = %d\n", __func__,msm_chg.batt_status);
	if (is_battery_present()) {
		if (is_battery_id_valid()) {
			if (msm_chg.batt_status == BATT_STATUS_ABSENT
				|| msm_chg.batt_status
					== BATT_STATUS_ID_INVALID) {
				msm_chg.stop_resume_check = 0;
				msm_chg.batt_status = BATT_STATUS_DISCHARGING;
			}
		} else
			msm_chg.batt_status = BATT_STATUS_ID_INVALID;
	 } else
		msm_chg.batt_status = BATT_STATUS_ABSENT;
}

static enum power_supply_property msm_power_props[] = {
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_ONLINE,
};

static char *msm_power_supplied_to[] = {
	"battery",
};

/* BEGIN [Jackal_Chen][2012/01/16][Add "usb_wall_chg" property] */
static int msm_power_get_property(struct power_supply *psy,
                                  enum power_supply_property psp,
                                  union power_supply_propval *val)
{
	struct msm_hardware_charger_priv *priv;
	bool is_usb_psy, is_usb_wall_chg_psy;

	is_usb_psy = !strcmp(psy->name, "usb");
	is_usb_wall_chg_psy = !strcmp(psy->name, "usb_wall_chg");

	if (is_usb_psy) {
		priv = container_of(psy, struct msm_hardware_charger_priv, psy_usb);
	} else {
		if(is_usb_wall_chg_psy) {
			priv = container_of(psy, struct msm_hardware_charger_priv, psy_usb_wall_chg);
		} else {
			priv = container_of(psy, struct msm_hardware_charger_priv, psy_ac);
		}
	}

	// [2012/01/03] Jackal - modify log level for SMT version
	PEGA_DBG_L("[CHG] %s, psp= %d, hw_chg_state= %d\n", __func__, psp, priv->hw_chg_state);
	// [2012/01/03] Jackal - modify log level for SMT version End
	switch (psp) {
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = !(priv->hw_chg_state == CHG_ABSENT_STATE);
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		// BEGIN-20110824-KenLee-add "usb" psy for user space
		if (is_usb_psy) {		/* if  usb power supply query  */
			if (priv->max_source_current == MAX_CHG_CURR_USB)
				val->intval = (priv->hw_chg_state == CHG_READY_STATE) || (priv->hw_chg_state == CHG_CHARGING_STATE);
			else
				val->intval = 0;
			// [2012/01/03] Jackal - modify log level for SMT version
			PEGA_DBG_L("[CHG] .. psy_usb, max_source_current= %d, intval= %d\n", priv->max_source_current, val->intval);
			// [2012/01/03] Jackal - modify log level for SMT version End
		} else if(is_usb_wall_chg_psy) {		/* if  usb wall charger power supply query  */
			if (priv->max_source_current == MAX_CHG_CURR_USB_WALL)
				val->intval = (priv->hw_chg_state == CHG_READY_STATE) || (priv->hw_chg_state == CHG_CHARGING_STATE);
			else
				val->intval = 0;
			// [2012/01/03] Jackal - modify log level for SMT version
			PEGA_DBG_L("[CHG] .. psy_usb_wall_chg, max_source_current= %d, intval= %d\n", priv->max_source_current, val->intval);
			// [2012/01/03] Jackal - modify log level for SMT version End
		} else {   /* if  ac power supply query  */
			if (priv->max_source_current == MAX_CHG_CURR_AC)
				val->intval = (priv->hw_chg_state == CHG_READY_STATE) || (priv->hw_chg_state == CHG_CHARGING_STATE);
			else
				val->intval = 0;
			PEGA_DBG_L("[CHG] .. psy_ac, max_source_current= %d, intval= %d\n", priv->max_source_current, val->intval);
		}
		// END-20110824-KenLee-add "usb" psy for user space
		break;
	default:
		return -EINVAL;
	}
	return 0;
}
/* END [Jackal_Chen][2012/01/16][Add "usb_wall_chg" property] */

static enum power_supply_property msm_batt_power_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	//POWER_SUPPLY_PROP_CHARGE_FULL,
	//POWER_SUPPLY_PROP_CHARGE_NOW,
	//POWER_SUPPLY_PROP_CHARGE_COUNTER,
	//POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_SERIAL_NUMBER,
};

static int msm_batt_power_get_property(struct power_supply *psy,
                                       enum power_supply_property psp,
                                       union power_supply_propval *val)
{
	mutex_lock(&msm_chg.chg_batt_lock);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = get_prop_batt_status();
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		val->intval = get_prop_charge_type();
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = get_prop_batt_health();
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = !(msm_chg.batt_status == BATT_STATUS_ABSENT);
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		/*val->intval = POWER_SUPPLY_TECHNOLOGY_NiMH;*/
		val->intval = POWER_SUPPLY_TECHNOLOGY_LIPO;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = get_prop_batt_mvolts() * 1000;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = get_prop_batt_capacity();
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		val->intval = get_prop_batt_fcc() * 1000;
		break;
	case POWER_SUPPLY_PROP_CHARGE_NOW:
		val->intval = get_prop_batt_rm() * 1000;
		break;
	case POWER_SUPPLY_PROP_CHARGE_COUNTER:
		val->intval = pega_chg_debug_level;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		val->intval = get_prop_batt_temperature();
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		//val->intval = get_prop_batt_chg_current() * 1000;
		val->intval = get_prop_batt_chg_current();
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = get_prop_batt_current() * 1000;
		break;
	case POWER_SUPPLY_PROP_SERIAL_NUMBER:
		val->strval = get_prop_batt_fw_version();
		break;
	default:
		return -EINVAL;
	}
	mutex_unlock(&msm_chg.chg_batt_lock);
	return 0;
}

static int msm_batt_power_set_property(struct power_supply *psy,
                                       enum power_supply_property psp,
                                       const union power_supply_propval *val)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_CHARGE_COUNTER:
		pega_chg_debug_level = val->intval;
		break;
	default:
		return -EPERM;
	}

	return 0;
}

static int msm_batt_power_property_is_writeable(struct power_supply *psy,
                                                enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_CHARGE_COUNTER:
		return 1;
	default:
		break;
	}
	return 0;
}

static struct power_supply msm_psy_batt = {
	.name = "battery",
	.type = POWER_SUPPLY_TYPE_BATTERY,
	.properties = msm_batt_power_props,
	.num_properties = ARRAY_SIZE(msm_batt_power_props),
	.get_property = msm_batt_power_get_property,
	.set_property = msm_batt_power_set_property,
	.property_is_writeable = msm_batt_power_property_is_writeable,
};

/* BEGIN [Terry_Tzeng][2012/01/09][Add dock resource marco] */
#ifdef MSM_CHARGER_ENABLE_DOCK
static enum power_supply_property pega_ecbatt_power_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	//POWER_SUPPLY_PROP_CHARGE_FULL,
	//POWER_SUPPLY_PROP_CHARGE_NOW,
	//POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_TEMP,
	//POWER_SUPPLY_PROP_SERIAL_NUMBER,
};

static int pega_ecbatt_power_get_property(struct power_supply *psy,
                                          enum power_supply_property psp,
                                          union power_supply_propval *val)
{
	//mutex_lock(&msm_chg.chg_batt_lock);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = get_prop_ecbatt_status();
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		val->intval = get_prop_ecbatt_charge_type();
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = get_prop_ecbatt_health();
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = is_ecbatt_present();
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		/*val->intval = POWER_SUPPLY_TECHNOLOGY_NiMH;*/
		val->intval = POWER_SUPPLY_TECHNOLOGY_LIPO;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = get_prop_ecbatt_mvolts() * 1000;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = get_prop_ecbatt_capacity();
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		val->intval = get_prop_ecbatt_fcc() * 1000;
		break;
	case POWER_SUPPLY_PROP_CHARGE_NOW:
		val->intval = get_prop_ecbatt_rm() * 1000;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		val->intval = get_prop_ecbatt_temperature();
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		//val->intval = get_prop_ecbatt_chg_current() * 1000;
		val->intval = get_prop_ecbatt_chg_current();
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = get_prop_ecbatt_current() * 1000;
		break;
	case POWER_SUPPLY_PROP_SERIAL_NUMBER:
		val->strval = get_prop_ecbatt_fw_version();
		break;
	default:
		return -EINVAL;
	}
	//mutex_unlock(&msm_chg.chg_batt_lock);
	return 0;
}

static struct power_supply msm_psy_dock = {
	.name = "ec_batt",
	.type = POWER_SUPPLY_TYPE_BATTERY,
	.properties = pega_ecbatt_power_props,
	.num_properties = ARRAY_SIZE(pega_ecbatt_power_props),
	.get_property = pega_ecbatt_power_get_property,
};
#endif /* MSM_CHARGER_ENABLE_DOCK */
// END, Tim PH [2012/01/05][add for docking]
/* END [Terry_Tzeng][2012/01/09][Add dock resource marco] */

static int usb_chg_current;
static struct msm_hardware_charger_priv *usb_hw_chg_priv;
static void (*notify_vbus_state_func_ptr)(int);
static int usb_notified_of_insertion;

// BEGIN [2012/07/12] Jackal - Modify to fix AC charger unstable bug
#if(MSM_CHARGER_ENABLE_OTG_CHECK)
static int mWaitOTGReport = 0;
static int mOTGCheckCount = 0;
#endif
// END [2012/07/12] Jackal - Modify to fix AC charger unstable bug

/* this is passed to the hsusb via platform_data msm_otg_pdata */
int msm_charger_register_vbus_sn(void (*callback)(int))
{
	pr_debug(KERN_INFO "%s\n", __func__);
	PEGA_DBG_L("[CHG] %s set notify_vbus_state_func_ptr! \n", __func__);

	notify_vbus_state_func_ptr = callback;
	return 0;
}

/* this is passed to the hsusb via platform_data msm_otg_pdata */
void msm_charger_unregister_vbus_sn(void (*callback)(int))
{
	pr_debug(KERN_INFO "%s\n", __func__);
	notify_vbus_state_func_ptr = NULL;
}

static void notify_usb_of_the_plugin_event(struct msm_hardware_charger_priv
					   *hw_chg, int plugin)
{
	PEGA_DBG_M("[CHG] %s, hw_chg_name= %s, plugin= %d\n", __func__,hw_chg->hw_chg->name,plugin);

	if (!notify_vbus_state_func_ptr)
	{
		PEGA_DBG_L("[CHG] %s , notify_vbus_state_func_ptr = NULL\n", __func__);
	}
	else
	{
		PEGA_DBG_L("[CHG] %s , notify_vbus_state_func_ptr NOT NULL\n", __func__);
	}

	plugin = !!plugin;
	// BEGIN [2012/07/12] Jackal - Modify to fix AC charger unstable bug
#if(MSM_CHARGER_ENABLE_OTG_CHECK)
	if (plugin == 1 && (usb_notified_of_insertion == 0 || mWaitOTGReport == 1)) {
		usb_notified_of_insertion = 1;
		if (notify_vbus_state_func_ptr) {
			PEGA_DBG_H("[CHG] %s, notifying plugin\n", __func__);
			//(*notify_vbus_state_func_ptr) (plugin);
			if(mWaitOTGReport == 1)
				(*notify_vbus_state_func_ptr) (NOTIFY_OTG_CHECK_EVENT);  // NOTIFY_OTG_CHECK_EVENT = 2
			else
				(*notify_vbus_state_func_ptr) (plugin);
		} else {
			PEGA_DBG_H("[CHG] %s, unable to notify plugin\n", __func__);
		}
		spin_lock(&usb_chg_lock);
		usb_hw_chg_priv = hw_chg;
		spin_unlock(&usb_chg_lock);
	}
#else
	if (plugin == 1 && usb_notified_of_insertion == 0) {
		usb_notified_of_insertion = 1;
		if (notify_vbus_state_func_ptr) {
			dev_dbg(msm_chg.dev, "%s notifying plugin\n", __func__);
			(*notify_vbus_state_func_ptr) (plugin);
		} else
			dev_dbg(msm_chg.dev, "%s unable to notify plugin\n",
				__func__);
		// [2012/01/03] Jackal - Fix sharging null pointer bug
		spin_lock(&usb_chg_lock);
		usb_hw_chg_priv = hw_chg;
		spin_unlock(&usb_chg_lock);
		// [2012/01/03] Jackal - Fix sharging null pointer bug End
	}
#endif
	if (plugin == 0 && usb_notified_of_insertion == 1) {
		if (notify_vbus_state_func_ptr) {
			dev_dbg(msm_chg.dev, "%s notifying unplugin\n",
				__func__);
			(*notify_vbus_state_func_ptr) (plugin);
		} else
			dev_dbg(msm_chg.dev, "%s unable to notify unplugin\n",
				__func__);
		usb_notified_of_insertion = 0;
		// [2012/01/03] Jackal - Fix sharging null pointer bug
		spin_lock(&usb_chg_lock);
		usb_hw_chg_priv = NULL;
		spin_unlock(&usb_chg_lock);
		// [2012/01/03] Jackal - Fix sharging null pointer bug End
	}
	// END [2012/07/12] Jackal - Modify to fix AC charger unstable bug
}

static unsigned int msm_chg_get_batt_capacity_percent(void)
{
	unsigned int current_voltage = msm_chg_get_batt_mvolts();
	unsigned int low_voltage = msm_chg.min_voltage;
	unsigned int high_voltage = msm_chg.max_voltage;

	if (current_voltage <= low_voltage)
		return 0;
	else if (current_voltage >= high_voltage)
		return 100;
	else
		return (current_voltage - low_voltage) * 100
		    / (high_voltage - low_voltage);
}

#ifdef DEBUG
static inline void debug_print(const char *func,
			       struct msm_hardware_charger_priv *hw_chg_priv)
{
	dev_dbg(msm_chg.dev,
		"%s current=(%s)(s=%d)(r=%d) new=(%s)(s=%d)(r=%d) batt=%d En\n",
		func,
		msm_chg.current_chg_priv ? msm_chg.current_chg_priv->
		hw_chg->name : "none",
		msm_chg.current_chg_priv ? msm_chg.
		current_chg_priv->hw_chg_state : -1,
		msm_chg.current_chg_priv ? msm_chg.current_chg_priv->
		hw_chg->rating : -1,
		hw_chg_priv ? hw_chg_priv->hw_chg->name : "none",
		hw_chg_priv ? hw_chg_priv->hw_chg_state : -1,
		hw_chg_priv ? hw_chg_priv->hw_chg->rating : -1,
		msm_chg.batt_status);
}
#else
static inline void debug_print(const char *func,
			       struct msm_hardware_charger_priv *hw_chg_priv)
{
}
#endif

static struct msm_hardware_charger_priv *find_best_charger(void)
{
	struct msm_hardware_charger_priv *hw_chg_priv;
	struct msm_hardware_charger_priv *better;
	int rating;

	better = NULL;
	rating = 0;
	PEGA_DBG_H("[CHG] %s \n", __func__);

	list_for_each_entry(hw_chg_priv, &msm_chg.msm_hardware_chargers, list) {
		if (is_chg_capable_of_charging(hw_chg_priv)) {
			if (hw_chg_priv->hw_chg->rating > rating) {
				rating = hw_chg_priv->hw_chg->rating;
				better = hw_chg_priv;
			}
		}
	}

	return better;
}

static int msm_charging_switched(struct msm_hardware_charger_priv *priv)
{
	int ret = 0;

	if (priv->hw_chg->charging_switched)
		ret = priv->hw_chg->charging_switched(priv->hw_chg);
	return ret;
}

static int msm_stop_charging(struct msm_hardware_charger_priv *priv)
{
	int ret;

	PEGA_DBG_H("[CHG] %s, hw_chg_state= %d  \n", __func__, priv->hw_chg_state);
	ret = priv->hw_chg->stop_charging(priv->hw_chg);
	PEGA_DBG_H("[CHG] .. after %s, hw_chg_state= %d  \n", __func__, priv->hw_chg_state);
	return ret;
}

/* the best charger has been selected -start charging from current_chg_priv */
static int msm_start_charging(void)
{
	int ret;
	struct msm_hardware_charger_priv *priv;

	if (msm_chg.current_chg_priv)
	{
		PEGA_DBG_H("[CHG] %s, batt_status= %d, hw_chg_state= %d, max_src_curr= %d\n", __func__,msm_chg.batt_status,msm_chg.current_chg_priv->hw_chg_state,msm_chg.current_chg_priv->max_source_current);
	}
	else
	{
		PEGA_DBG_H("[CHG_ERR] %s fail, current_chg_priv= NULL, batt_status= %d \n", __func__,msm_chg.batt_status);
	}

	priv = msm_chg.current_chg_priv;
	ret = priv->hw_chg->start_charging(priv->hw_chg, msm_chg.max_voltage,
					 priv->max_source_current);
	if (ret) {
		dev_err(msm_chg.dev, "%s couldnt start chg error = %d\n",
			priv->hw_chg->name, ret);
	} else
		priv->hw_chg_state = CHG_CHARGING_STATE;

	PEGA_DBG_H("[CHG] .. after %s, chg= %s, max_src_curr= %d, hw_chg_state= %d \n", __func__,priv->hw_chg->name,priv->max_source_current,priv->hw_chg_state);
	return ret;
}

static void handle_charging_done(struct msm_hardware_charger_priv *priv)
{
	if (msm_chg.current_chg_priv)
	{
		PEGA_DBG_H("[CHG] %s, batt_status= %d, hw_chg_state= %d\n", __func__,msm_chg.batt_status,msm_chg.current_chg_priv->hw_chg_state);
	}
	else
	{
		PEGA_DBG_M("[CHG_ERR] %s fail, current_chg_priv= NULL, batt_status= %d\n", __func__,msm_chg.batt_status);
	}

	if (msm_chg.current_chg_priv == priv) {
		if (msm_chg.current_chg_priv->hw_chg_state ==
		    CHG_CHARGING_STATE)
			if (msm_stop_charging(msm_chg.current_chg_priv)) {
				dev_err(msm_chg.dev, "%s couldnt stop chg\n",
					msm_chg.current_chg_priv->hw_chg->name);
			}
		msm_chg.current_chg_priv->hw_chg_state = CHG_READY_STATE;

		msm_chg.batt_status = BATT_STATUS_JUST_FINISHED_CHARGING;
		dev_info(msm_chg.dev, "%s: stopping safety timer work\n",
				__func__);
		cancel_delayed_work(&msm_chg.teoc_work);

		PEGA_DBG_M("[CHG] %s, starting resume timer work! \n", __func__);
		queue_delayed_work(msm_chg.event_wq_thread,
					&msm_chg.resume_work,
				      round_jiffies_relative(msecs_to_jiffies
						     (RESUME_CHECK_PERIOD_MS)));
	}
	PEGA_DBG_H("[CHG] .. after %s , hw_chg_state= %d\n", __func__,msm_chg.current_chg_priv->hw_chg_state);
}

static void teoc(struct work_struct *work)
{
	/* we have been charging too long - stop charging */

	mutex_lock(&msm_chg.status_lock);
	if (msm_chg.current_chg_priv != NULL
	    && msm_chg.current_chg_priv->hw_chg_state == CHG_CHARGING_STATE)
	{
		msm_chg.safety_stop_charging_count ++;
		/* BEGIN [Jackal_Chen][2012/04/25][Since we always enable re-charge, stop_resume_check should not be set to 1 after safety timer] */
/*		if (msm_chg.safety_stop_charging_count == SAFETY_RESUME_COUNT)
			msm_chg.stop_resume_check = 1;*/
		/* END [Jackal_Chen][2012/04/25][Since we always enable re-charge, stop_resume_check should not be set to 1 after safety timer] */
		handle_charging_done(msm_chg.current_chg_priv);
	}
	PEGA_DBG_H("[CHG] %s, safety timer work expired, count= %d \n", __func__, msm_chg.safety_stop_charging_count);
	/* BEGIN [Jackal_Chen][2012/04/25][Safety yimer expired, notify user space] */
	if(msm_chg.current_chg_priv != NULL)
		power_supply_changed(&msm_chg.current_chg_priv->psy_usb);
	/* END [Jackal_Chen][2012/04/25][Safety yimer expired, notify user space] */
	mutex_unlock(&msm_chg.status_lock);
}

static void handle_battery_inserted(void)
{
	unsigned long safety_time;

	if (msm_chg.current_chg_priv)
	{
		PEGA_DBG_H("[CHG] %s, batt_status= %d, hw_chg_state= %d\n", __func__,msm_chg.batt_status,msm_chg.current_chg_priv->hw_chg_state);
	}
	else
	{
		PEGA_DBG_M("[CHG_ERR] %s fail, current_chg_priv= NULL, batt_status= %d \n", __func__,msm_chg.batt_status);
	}

	msm_chg.stop_resume_check = 0;

	/* if a charger is already present start charging */
	if (msm_chg.current_chg_priv != NULL &&
	    is_batt_status_capable_of_charging() &&
	    !is_batt_status_charging()) {
		if (msm_start_charging()) {
			dev_err(msm_chg.dev, "%s couldnt start chg\n",
				msm_chg.current_chg_priv->hw_chg->name);
			return;
		}
		msm_chg.batt_status = BATT_STATUS_TRKL_CHARGING;

		dev_info(msm_chg.dev, "%s: starting safety timer work\n",
				__func__);

		/* BEGIN [Jackal_Chen][2012/04/23][Modify safety time of wall charger] */
		if (msm_chg.current_chg_priv->max_source_current==MAX_CHG_CURR_USB)
			safety_time = msm_chg.usb_safety_time;
		else if(msm_chg.current_chg_priv->max_source_current==MAX_CHG_CURR_USB_WALL)
			safety_time = msm_chg.wall_safety_time;
		/* BEGIN [Terry_Tzeng][2012/05/28][Set isnerted dock safety time] */
#ifdef MSM_CHARGER_ENABLE_DOCK
		else if(msm_chg.current_chg_priv->max_source_current==INSERT_DK_MAX_CURR)
		{
			if (msm_ec_dock)
				safety_time = msm_ec_dock->get_safety_time() * 60 * MSEC_PER_SEC;
			else
				safety_time = 0;
				
			PEGA_DBG_H("[CHG_EC] Get dock safety time= %ld mins\n", safety_time);
		}
#endif /* MSM_CHARGER_ENABLE_DOCK */
		/* END [Terry_Tzeng][2012/05/28][Set isnerted dock safety time] */
		else
			safety_time = msm_chg.safety_time;
		/* END [Jackal_Chen][2012/04/23][Modify safety time of wall charger] */
		queue_delayed_work(msm_chg.event_wq_thread,
					&msm_chg.teoc_work,
				      round_jiffies_relative(msecs_to_jiffies
							     (safety_time)));
		PEGA_DBG_H("[CHG] .. after %s, hw_chg_state= %d \n", __func__,msm_chg.current_chg_priv->hw_chg_state);
	}
}

/* BEGIN [Jackal_Chen][2012/03/29][Modify the logic of resume_charging()] */
static void resume_charging(struct work_struct *work)
{
	if (msm_chg.stop_resume_check) {
		PEGA_DBG_H("[CHG] %s , stopping resume!\n", __func__);
		return;
	}

	/* BEGIN [Jackal_Chen][2012/04/25][If safety timer has expired, no need to re-charge anymore] */
	if(msm_chg.safety_stop_charging_count > 0) {
		PEGA_DBG_H("[CHG] %s , safety timer has expired, no need to re-charge anymore. \n", __func__);
		return;
	}
	/* END [Jackal_Chen][2012/04/25][If safety timer expired, no need to re-charge anymore] */

	update_batt_status();
	if (msm_chg.batt_status != BATT_STATUS_JUST_FINISHED_CHARGING) {
		PEGA_DBG_M("[CHG] %s called outside JFC state!", __func__);
		return;
	}

	PEGA_DBG_H("[CHG] %s , resume_count= %d\n", __func__, msm_chg.resume_count);
	/* BEGIN [Jackal_Chen][2012/04/11][Modify the logic for SMT => no need to re-charge when battery level <= 90%] */
#if defined PEGA_SMT_BUILD
	if(msm_chg_get_batt_status() != POWER_SUPPLY_STATUS_FULL) {
		msm_chg.resume_count++;
	}
#else
	if(msm_batt_gauge && msm_batt_gauge->get_battery_level() <= PEGA_CHG_RESUME_BATT_LEVEL) {
		msm_chg.resume_count = MSM_CHARGER_RESUME_COUNT + 1;
	}
	else if(msm_chg_get_batt_status() != POWER_SUPPLY_STATUS_FULL) {
		msm_chg.resume_count++;
	}
#endif
	else {
		msm_chg.resume_count = 0;
	}
	/* END [Jackal_Chen][2012/04/11][Modify the logic for SMT => no need to re-charge when battery level <= 90%] */
	/*if (msm_chg_get_batt_status() == POWER_SUPPLY_STATUS_DISCHARGING)
		msm_chg.resume_count++;
	else
		msm_chg.resume_count = 0;*/

#ifndef PEGA_SMT_BUILD
	/*
	 * if we are within 500mV of min voltage range forget the count
	 * force start battery charging by increasing resume count
	 */
	if (msm_chg_get_batt_mvolts() < msm_chg.min_voltage + 500) {
		PEGA_DBG_M("[CHG] %s, batt lost voltage rapidly - force resume charging!\n",__func__);
		msm_chg.resume_count += MSM_CHARGER_RESUME_COUNT + 1;
	}
#endif

	if (msm_chg.resume_count > MSM_CHARGER_RESUME_COUNT) {
		/* the FC flag of battery gauge has rised for 5 mins straight- resume charging */
		/* act as if the battery was just plugged in */
		PEGA_DBG_H("[CHG] %s , need to restart charging.\n", __func__);
		mutex_lock(&msm_chg.status_lock);
		msm_chg.batt_status = BATT_STATUS_DISCHARGING;
		msm_chg.resume_count = 0;
		handle_battery_inserted();
		/* BEGIN [Jackal_Chen][2012/03/27][Modify the order of uevent] */
		if (msm_chg.current_chg_priv != NULL) {
			// BEGIN-20110824-KenLee-add "usb" psy for user space
			//power_supply_changed(&msm_chg.current_chg_priv->psy_ac);
			power_supply_changed(&msm_chg.current_chg_priv->psy_usb);
			// END-20110824-KenLee-add "usb" psy for user space
			/* BEGIN [Jackal_Chen][2012/01/16][Add "usb_wall_chg" psy for user space] */
			//power_supply_changed(&msm_chg.current_chg_priv->psy_usb_wall_chg);
			/* END [Jackal_Chen][2012/01/16][Add "usb_wall_chg" psy for user space] */
		} else if (msm_batt_gauge) {
			power_supply_changed(&msm_psy_batt);
		}
		/* END [Jackal_Chen][2012/03/27][Modify the order of uevent] */
		mutex_unlock(&msm_chg.status_lock);
	} else {
		/* reschedule resume check */
		PEGA_DBG_M("[CHG] %s, rescheduling resume timer work! \n",	__func__);
		queue_delayed_work(msm_chg.event_wq_thread,
					&msm_chg.resume_work,
				      round_jiffies_relative(msecs_to_jiffies
						     (RESUME_CHECK_PERIOD_MS)));
	}
}
/* END [Jackal_Chen][2012/03/29][Modify the logic of resume_charging()] */

// BEGIN [2012/07/12] Jackal - Modify to fix AC charger unstable bug
#if(MSM_CHARGER_ENABLE_OTG_CHECK)
static void wait_otg_report(struct work_struct *work)
{
	PEGA_DBG_H("[CHG] %s().\n", __func__);
#if(MSM_CHARGER_OTG_CHECK_MAX == -1)  // infinite check until otg report back
	if(mWaitOTGReport == 1) {
		if(usb_hw_chg_priv != NULL) {
			notify_usb_of_the_plugin_event(usb_hw_chg_priv, 1);
			// queue next time work
			PEGA_DBG_M("[CHG] %s(), queue next check.\n", __func__);
			queue_delayed_work(msm_chg.event_wq_thread, &msm_chg.wait_otg_report_work, round_jiffies_relative(msecs_to_jiffies(2000)));
		} else {
			PEGA_DBG_H("[CHG] %s(), msm_chg.current_chg_priv is NULL, don't do anything.\n", __func__);
		}
	}
#else  // only check for MSM_CHARGER_OTG_CHECK_MAX times
	if(mWaitOTGReport == 1 && mOTGCheckCount < MSM_CHARGER_OTG_CHECK_MAX) {
		if(usb_hw_chg_priv != NULL) {
			notify_usb_of_the_plugin_event(usb_hw_chg_priv, 1);
			// queue next time work
			PEGA_DBG_M("[CHG] %s(), queue next check.\n", __func__);
			queue_delayed_work(msm_chg.event_wq_thread, &msm_chg.wait_otg_report_work, round_jiffies_relative(msecs_to_jiffies(2000)));
		} else {
			PEGA_DBG_H("[CHG] %s(), msm_chg.current_chg_priv is NULL, don't do anything.\n", __func__);
		}
		mOTGCheckCount++;
	}
#endif  // end of check (MSM_CHARGER_OTG_CHECK_MAX == -1)
	else {
		mOTGCheckCount = 0;
		PEGA_DBG_H("[CHG] %s(), don't queue next check.\n", __func__);
	}
}
#endif
// END [2012/07/12] Jackal - Modify to fix AC charger unstable bug


static void handle_battery_removed(void)
{
	if (msm_chg.current_chg_priv)
	{
		PEGA_DBG_H("[CHG] %s, batt_status= %d, hw_chg_state= %d\n", __func__,msm_chg.batt_status,msm_chg.current_chg_priv->hw_chg_state);
	}
	else
	{
		PEGA_DBG_H("[CHG_ERR] %s fail, current_chg_priv= NULL, batt_status= %d \n", __func__,msm_chg.batt_status);
	}

	/* if a charger is charging the battery stop it */
	if (msm_chg.current_chg_priv != NULL
	    && msm_chg.current_chg_priv->hw_chg_state == CHG_CHARGING_STATE) {
		if (msm_stop_charging(msm_chg.current_chg_priv)) {
			dev_err(msm_chg.dev, "%s couldnt stop chg\n",
				msm_chg.current_chg_priv->hw_chg->name);
		}
		msm_chg.current_chg_priv->hw_chg_state = CHG_READY_STATE;

		dev_info(msm_chg.dev, "%s: stopping safety timer work\n",
				__func__);
		cancel_delayed_work(&msm_chg.teoc_work);
		PEGA_DBG_H("[CHG] .. after %s, hw_chg_state= %d \n",__func__,msm_chg.current_chg_priv->hw_chg_state);
	}
	msm_chg.stop_resume_check = 1;
	cancel_delayed_work(&msm_chg.resume_work);
}



/* BEGIN [Terry_Tzeng][2012/02/15][Handle restarting charging to reset input current and charging current in dock in statement] */
#ifdef MSM_CHARGER_ENABLE_DOCK
static void msm_chg_set_period_chg_curr_for_dk(struct msm_hardware_charger_priv *priv)
{
	if (priv != NULL) {
		priv->hw_chg->set_period_chg_curr_for_dk(priv->hw_chg);
		PEGA_DBG_H("[CHG] .. after %s, chg= %s, max_src_curr= %d, hw_chg_state= %d \n", __func__,priv->hw_chg->name,priv->max_source_current,priv->hw_chg_state);
	}
	else
	{
		PEGA_DBG_H("[CHG_ERR] Get msm_hardware_charger_priv is null point not to set periodically charging current!! \n");
	}

}

static void msm_chg_restop_charging(void)
{
	PEGA_DBG_H("[CHG] %s, Implement restop charging for docking case \n", __func__);
	if (msm_chg.batt_status == BATT_STATUS_ABSENT)
		return;
		/* debounce */
	if (is_battery_present())
		return;

	if(msm_chg.batt_status == BATT_STATUS_TRKL_CHARGING
	    || msm_chg.batt_status == BATT_STATUS_FAST_CHARGING)
	{
		msm_chg.batt_status = BATT_STATUS_DISCHARGING;
	handle_battery_removed();
}
}

static void msm_chg_restart_charging(struct msm_hardware_charger_priv *priv)
{
	PEGA_DBG_H("[CHG] %s, Implement restart charging for docking case \n", __func__);

	/*
	if(gpio_get_value_cansleep(dock_in_gpio_number))
		return;
	*/
	if (msm_chg.batt_status == BATT_STATUS_ABSENT)
		return;
		// debounce

	if(msm_chg.batt_status == BATT_STATUS_TRKL_CHARGING
	    || msm_chg.batt_status == BATT_STATUS_FAST_CHARGING)
	{
		msm_chg.batt_status = BATT_STATUS_ABSENT;
		handle_battery_removed();
	}
	if(NULL == msm_chg.current_chg_priv)
	{
		PEGA_DBG_H("[CHG] msm_chg.current_chg_priv = NULL, link point address \n");
		msm_chg.current_chg_priv = priv;
	}

	msm_chg.current_chg_priv->max_source_current = INSERT_DK_MAX_CURR;
	msm_chg.batt_status = BATT_STATUS_DISCHARGING;
	handle_battery_inserted();

}
#endif /* MSM_CHARGER_ENABLE_DOCK */
/* END [Terry_Tzeng][2012/02/15][Handle restarting charging to reset input current and charging current in dock in statement] */


static void update_heartbeat(struct work_struct *work)
{
	int temperature;
	if (msm_chg.current_chg_priv)
	{
		PEGA_DBG_H("\n[CHG] %s, batt_status= %d, hw_chg_state= %d\n", __func__,msm_chg.batt_status,msm_chg.current_chg_priv->hw_chg_state);
	}
	else
	{
		PEGA_DBG_M("\n[CHG] %s, current_chg_priv= NULL, batt_status= %d \n", __func__,msm_chg.batt_status);
	}

	if (msm_chg.batt_status == BATT_STATUS_ABSENT
		|| msm_chg.batt_status == BATT_STATUS_ID_INVALID) {
		if (is_battery_present())
			if (is_battery_id_valid()) {
				msm_chg.batt_status = BATT_STATUS_DISCHARGING;
				handle_battery_inserted();
			}
	} else {
		if (!is_battery_present()) {
			msm_chg.batt_status = BATT_STATUS_ABSENT;
			handle_battery_removed();
		}
		/*
		 * check battery id because a good battery could be removed
		 * and replaced with a invalid battery.
		 */
		if (!is_battery_id_valid()) {
			msm_chg.batt_status = BATT_STATUS_ID_INVALID;
			handle_battery_removed();
		}
	}
	pr_debug("msm-charger %s batt_status= %d\n",
				__func__, msm_chg.batt_status);

	if (msm_chg.current_chg_priv
		&& msm_chg.current_chg_priv->hw_chg_state
			== CHG_CHARGING_STATE) {
		temperature = get_prop_batt_temperature();
		/* TODO implement JEITA SPEC*/
	}

	/* notify that the voltage has changed
	 * the read of the capacity will trigger a
	 * voltage read*/
	/* BEGIN [Jackal_Chen][2012/03/27][Modify the order of uevent] */
	if (msm_chg.current_chg_priv != NULL)
	{
		PEGA_DBG_L("[CHG] %s (%d), power_supply_changed, psy_ac, is called.\n", __func__, __LINE__);
		power_supply_changed(&msm_chg.current_chg_priv->psy_usb);
	}
	else if (msm_batt_gauge)
	{
		PEGA_DBG_L("[CHG] %s, before ps_changed \n", __func__);
		power_supply_changed(&msm_psy_batt);
	}
	/* END [Jackal_Chen][2012/03/27][Modify the order of uevent] */
/* BEGIN [Terry_Tzeng][2012/01/09][Add dock resource marco] */
/* BEGIN [Jackal_Chen][2012/03/22][Send uevent just once] */
#ifdef MSM_CHARGER_ENABLE_DOCK
	else if (msm_ec_dock)
	{
		PEGA_DBG_L("[CHG_EC] %s, before ps_changed \n", __func__);
		power_supply_changed(&msm_psy_dock);
	}
/* END [Jackal_Chen][2012/03/22][Send uevent just once] */

/* BEGIN [Terry_Tzeng][2012/02/08][Check tablet battery and dock battery status in dock in statement] */
	/* BEGIN [Jackal_Chen][2012/04/09][Use HW_ID value to distinguish GPIO number of dock_in pin] */
	if(!gpio_get_value_cansleep(dock_in_gpio_number))
	{
		PEGA_DBG_L("[CHG_EC] %s, Check tablet battery and dock battery st in updated heartbeat \n", __func__);
		if (msm_ec_dock)
			msm_ec_dock->is_tb_batt_change_cb(NULL, CHG_DOCK_HEARTBEAT_CHECK);
	}
	/* END [Jackal_Chen][2012/04/09][Use HW_ID value to distinguish GPIO number of dock_in pin] */
/* END [Terry_Tzeng][2012/02/08][Check tablet battery and dock battery status in dock in statement] */
#endif /* MSM_CHARGER_ENABLE_DOCK */
/* END [Terry_Tzeng][2012/01/09][Add dock resource marco] */
	if (msm_chg.stop_update) {
		msm_chg.stop_update = 0;
		return;
	}
	queue_delayed_work(msm_chg.event_wq_thread,
				&msm_chg.update_heartbeat_work,
			      round_jiffies_relative(msecs_to_jiffies
						     (msm_chg.update_time)));
}

/* set the charger state to READY before calling this */
static void handle_charger_ready(struct msm_hardware_charger_priv *hw_chg_priv)
{
	unsigned long safety_time=0;

	PEGA_DBG_H("[CHG] %s, hw_chg_state= %d \n", __func__,hw_chg_priv->hw_chg_state);

	/* [20110816] KenLee, add for resume charging */
	msm_chg.safety_stop_charging_count = 0;

	if (msm_chg.current_chg_priv != NULL
	    && hw_chg_priv->hw_chg->rating >
	    msm_chg.current_chg_priv->hw_chg->rating) {
		if (msm_chg.current_chg_priv->hw_chg_state ==
		    CHG_CHARGING_STATE) {
			if (msm_stop_charging(msm_chg.current_chg_priv)) {
				dev_err(msm_chg.dev, "%s couldnt stop chg\n",
					msm_chg.current_chg_priv->hw_chg->name);
				return;
			}
			if (msm_charging_switched(msm_chg.current_chg_priv)) {
				dev_err(msm_chg.dev, "%s couldnt stop chg\n",
					msm_chg.current_chg_priv->hw_chg->name);
				return;
			}
		}
		msm_chg.current_chg_priv->hw_chg_state = CHG_READY_STATE;
		msm_chg.current_chg_priv = NULL;
	}

	if (msm_chg.current_chg_priv == NULL) {
		msm_chg.current_chg_priv = hw_chg_priv;
		dev_info(msm_chg.dev,
			 "%s: best charger = %s\n", __func__,
			 msm_chg.current_chg_priv->hw_chg->name);

		if (!is_batt_status_capable_of_charging())
		{
			PEGA_DBG_H("[CHG_ERR] %s, check capable_of_charging= 0, batt_status= %d \n", __func__,msm_chg.batt_status);
			return;
		}
		/* start charging from the new charger */
		if (!msm_start_charging()) {
			/* if we simply switched chg continue with teoc timer
			 * else we update the batt state and set the teoc
			 * timer */
			if (!is_batt_status_charging()) {
				dev_info(msm_chg.dev,
				       "%s: starting safety timer\n", __func__);
				/* BEGIN [Jackal_Chen][2012/04/23][Modify safety time of wall charger] */
				if (msm_chg.current_chg_priv->max_source_current == MAX_CHG_CURR_USB) {
					safety_time = msm_chg.usb_safety_time;
					PEGA_DBG_H("[CHG] %s, safety time = 30 (hours) \n", __func__);
				}
				else if (msm_chg.current_chg_priv->max_source_current == MAX_CHG_CURR_USB_WALL) {
					safety_time = msm_chg.wall_safety_time;
					PEGA_DBG_H("[CHG] %s, safety time = 14 (hours) \n", __func__);
				}
#ifdef MSM_CHARGER_ENABLE_DOCK
				else if (msm_chg.current_chg_priv->max_source_current == INSERT_DK_MAX_CURR) {
					if (msm_ec_dock)
						safety_time = msm_ec_dock->get_safety_time() * 60 * MSEC_PER_SEC;
					else
						safety_time = 0;
					PEGA_DBG_H("[CHG] %s,insert dock safety time = %ld (mins)\n", __func__, safety_time);
				}
#endif /*MSM_CHARGER_ENABLE_DOCK*/
				else {
					safety_time = msm_chg.safety_time;
					PEGA_DBG_H("[CHG] %s, safety time = 6 (hours) \n", __func__);
				}
				/* END [Jackal_Chen][2012/04/23][Modify safety time of wall charger] */
				queue_delayed_work(msm_chg.event_wq_thread,
							&msm_chg.teoc_work,
						      round_jiffies_relative
						      (msecs_to_jiffies
						       (safety_time)));
				msm_chg.batt_status = BATT_STATUS_TRKL_CHARGING;
			}
		} else {
			/* we couldnt start charging from the new readied
			 * charger */
			if (is_batt_status_charging())
				msm_chg.batt_status = BATT_STATUS_DISCHARGING;
		}
	}
	PEGA_DBG_H("[CHG] .. after %s, hw_chg_state= %d \n", __func__,msm_chg.current_chg_priv->hw_chg_state);
}

static void handle_charger_removed(struct msm_hardware_charger_priv
				   *hw_chg_removed, int new_state)
{
	struct msm_hardware_charger_priv *hw_chg_priv;

	debug_print(__func__, hw_chg_removed);
	/* [20110816] KenLee, add for resume charging */
	msm_chg.safety_stop_charging_count = 0;

	if (msm_chg.current_chg_priv == hw_chg_removed) {
		PEGA_DBG_H("[CHG] %s , hw_chg_state= %d \n", __func__, msm_chg.current_chg_priv->hw_chg_state);
		if ((msm_chg.current_chg_priv->hw_chg_state == CHG_CHARGING_STATE) ||
			(msm_chg.current_chg_priv->hw_chg_state == CHG_READY_STATE)) {
			if (msm_stop_charging(hw_chg_removed)) {
				dev_err(msm_chg.dev, "%s couldnt stop chg\n",
					msm_chg.current_chg_priv->hw_chg->name);
			}
		}
		msm_chg.current_chg_priv = NULL;
	}
	else
	{
		PEGA_DBG_H("[CHG_ERR] %s , current_chg_priv != hw_chg_removed\n", __func__);
	}


	hw_chg_removed->hw_chg_state = new_state;

	if (msm_chg.current_chg_priv == NULL) {
		hw_chg_priv = find_best_charger();
		if (hw_chg_priv == NULL) {
			dev_info(msm_chg.dev, "%s: no chargers\n", __func__);
			/* if the battery was Just finished charging
			 * we keep that state as is so that we dont rush
			 * in to charging the battery when a charger is
			 * plugged in shortly. */
			if (is_batt_status_charging())
				msm_chg.batt_status = BATT_STATUS_DISCHARGING;
		} else {
			msm_chg.current_chg_priv = hw_chg_priv;
			dev_info(msm_chg.dev,
				 "%s: best charger = %s\n", __func__,
				 msm_chg.current_chg_priv->hw_chg->name);

			if (!is_batt_status_capable_of_charging())
				return;

			if (msm_start_charging()) {
				/* we couldnt start charging for some reason */
				msm_chg.batt_status = BATT_STATUS_DISCHARGING;
			}
		}
	}

	/* if we arent charging stop the safety timer */
	if (!is_batt_status_charging()) {
		dev_info(msm_chg.dev, "%s: stopping safety timer work\n",
				__func__);
		cancel_delayed_work(&msm_chg.teoc_work);
	}
	PEGA_DBG_H("[CHG] .. after %s, hw_chg_state= %d \n", __func__, hw_chg_removed->hw_chg_state);
}

/* BEGIN [Jackal_Chen][2012/01/16][Modify check logic(add 12V AC type check)] */
static int previous_chg_type = CHG_TYPE_USB;

static int check_charger_type(void)
{
	int chg_type = CHG_TYPE_USB;
	int device_id = 1;  // device_id = 0 -> 12 V AC charger or dock in ; device_id = 1 -> Y-cable or none or audio in
	int dock_in = 0;  // dock_in = 0 -> dock in ; dock_in = 1 -> dock out
	int cbl_pwr = 0;  // cbl_pwr = 1 -> cable plug in ; cbl_pwr = 0 -> cable plug out
	/* BEGIN [Jackal_Chen][2012/04/10][Detect Y-cable by adc] */
	int64_t adc_value;
	int rc;
	/* END [Jackal_Chen][2012/04/10][Detect Y-cable by adc] */

	/* BEGIN [Jackal_Chen][2012/04/10][Detect Y-cable by adc] */
	rc = pega_get_mpp_adc(PEGA_CHG_DEVICE_ID_MPP_MUNBER, &adc_value);
	if(!rc) {
		PEGA_DBG_L("[CHG] device_id adc value  = %lld\n", adc_value);
	}
	if(adc_value <= 100000) {  // voltage < 0.1V, serve as 0V
		device_id = 0;
	}
	//device_id = gpio_get_value_cansleep(PM8921_MPP12_GPIO);
	/* END [Jackal_Chen][2012/04/10][Detect Y-cable by adc] */
	/* BEGIN [Jackal_Chen][2012/04/09][Use HW_ID value to distinguish GPIO number of dock_in pin] */
	dock_in = gpio_get_value_cansleep(dock_in_gpio_number);
	/* END [Jackal_Chen][2012/04/09][Use HW_ID value to distinguish GPIO number of dock_in pin] */
	cbl_pwr = pega_chg_read_irq_stat(PM8921_CBL_PWR_IRQ_NUMBER);

	PEGA_DBG_M("[CHG] %s(), adc_value = %lld, dock_in = %d, cbl_pwr = %d.\n", __func__, adc_value, dock_in, cbl_pwr);


/* BEGIN [Terry_Tzeng][2012/03/16][Check entried bypass mode case] */
	if(!dock_in) {
		int bypass_normal_en;
		int rc;

		rc = get_usb_bypass_en(&bypass_normal_en);

		if((usb_bypass_high == bypass_normal_en) &&
			(FAIL != rc))
		{
			PEGA_DBG_L("[CHG] %s, charger type is dock to switch bypass mode.\n", __func__);
			previous_chg_type = CHG_TYPE_USB;
			chg_type = CHG_TYPE_USB;
		}
		else
		{
			PEGA_DBG_L("[CHG] %s, charger type is dock.\n", __func__);
			previous_chg_type = CHG_TYPE_DOCK;
			chg_type = CHG_TYPE_DOCK;
		}
	}
/* END [Terry_Tzeng][2012/03/16][Check entried bypass mode case] */
	/* BEGIN [Jackal_Chen][2012/04/10][Detect Y-cable by adc] */
	else if(!device_id && dock_in) {
		PEGA_DBG_M("[CHG] %s, charger type is AC.\n", __func__);
		previous_chg_type = CHG_TYPE_AC;
		chg_type = CHG_TYPE_AC;
	}
	/* END [Jackal_Chen][2012/04/10][Detect Y-cable by adc] */
	else {
		/*
		There are 2 conditions of this part:
		1. Plug in USB/USB wall charger
		2. Remove any charger(USB/USB wall charger/AC/Dock)
		*/
		if(cbl_pwr) {
			PEGA_DBG_L("[CHG] %s, Plug in USB/USB wall charger.\n", __func__);
			previous_chg_type = CHG_TYPE_USB;
			chg_type = CHG_TYPE_USB;
		} else {
			if(previous_chg_type == CHG_TYPE_DOCK) {
				PEGA_DBG_L("[CHG] %s, Remove dock, return dock type.\n", __func__);
				previous_chg_type = CHG_TYPE_USB;
				chg_type = CHG_TYPE_DOCK;
			} else if(previous_chg_type == CHG_TYPE_AC) {
				PEGA_DBG_L("[CHG] %s, Remove AC, return AC type.\n", __func__);
				previous_chg_type = CHG_TYPE_USB;
				chg_type = CHG_TYPE_AC;
			} else {
				PEGA_DBG_L("[CHG] %s, Remove USB/USB wall charger, return USB type.\n", __func__);
				previous_chg_type = CHG_TYPE_USB;
				chg_type = CHG_TYPE_USB;
			}
		}
	}


#if 0
#if PEGA_CHG_HARD_CODE_12V
	int chg_type = CHG_TYPE_AC;
#else
	int chg_type = CHG_TYPE_USB;
#endif
#endif
#if 0
	int duck_id;

	PEGA_DBG_M("[CHG] %s \n",__func__);

	duck_id = read_duck_id();
	switch (duck_id)
	{
	case 0:
		chg_type = CHG_TYPE_AC;
	break;
	default:
		chg_type = CHG_TYPE_USB;
	}
#endif
	PEGA_DBG_M("[CHG] %s , chg_type = %d\n", __func__, chg_type);

	return chg_type;
}
/* END [Jackal_Chen][2012/01/16][Modify check logic(add 12V AC type check)] */

static void handle_event(struct msm_hardware_charger *hw_chg, int event)
{
	struct msm_hardware_charger_priv *priv = NULL;
	/*
	 * if hw_chg is NULL then this event comes from non-charger
	 * parties like battery gauge
	 */
	/* BEGIN [Jackal_Chen][2012/01/16][Modify logic for AC/dock] */
	int chg_type = CHG_TYPE_USB;
	/* END [Jackal_Chen][2012/01/16][Modify logic for AC/dock] */
	mutex_lock(&msm_chg.chg_batt_lock);
	mutex_lock(&msm_chg.status_lock);

	if (hw_chg)
	{
		priv = hw_chg->charger_private;
		PEGA_DBG_H("[CHG] %s, event %d from %s, chg_type= %d, hw_chg_state= %d\n", __func__, event, hw_chg->name, hw_chg->type,priv->hw_chg_state);
	}
	else
	{
		PEGA_DBG_H("[CHG] %s, event %d from NULL.\n", __func__, event);
		if (event == CHG_ENUMERATED_EVENT)
			return;
	}

	switch (event) {
	case CHG_INSERTED_EVENT:
		if (priv->hw_chg_state != CHG_ABSENT_STATE) {
			dev_info(msm_chg.dev,
				 "%s insertion detected when cbl present",
				 hw_chg->name);
			break;
		}
		//usb_chg_current = 500;
		// BEGIN [2012/03/22] Jackal - Move wake_lock here
		/* BEGIN [Terry_Tzeng][2012/03/23][Switch mode to control wake lock] */
#ifdef MSM_CHARGER_ENABLE_DOCK
		/* BEGIN [Jackal_Chen][2012/04/09][Use HW_ID value to distinguish GPIO number of dock_in pin] */
		if(gpio_get_value_cansleep(dock_in_gpio_number))
		/* END [Jackal_Chen][2012/04/09][Use HW_ID value to distinguish GPIO number of dock_in pin] */
#endif /* MSM_CHARGER_ENABLE_DOCK */
		wake_lock(&priv->wl);
		/* END [Terry_Tzeng][2012/03/23][Switch mode to control wake lock] */
		// END [2012/03/22] Jackal - Move wake_lock here
		update_batt_status();
		priv->max_source_current = 0;
		/* BEGIN [Jackal_Chen][2012/01/16][Modify logic for AC/dock] */
		chg_type = check_charger_type();
		if (chg_type == CHG_TYPE_USB) {
			PEGA_DBG_H("[CHG] %s(), CHG_INSERTED_EVENT, charge type = USB/USB wall charger.\n", __func__);
			priv->hw_chg_state = CHG_PRESENT_STATE;
			notify_usb_of_the_plugin_event(priv, 1);
			// BEGIN [2012/07/12] Jackal - Modify to fix AC charger unstable bug
#if(MSM_CHARGER_ENABLE_OTG_CHECK)
			mWaitOTGReport = 1;
			queue_delayed_work(msm_chg.event_wq_thread, &msm_chg.wait_otg_report_work, round_jiffies_relative(msecs_to_jiffies(2000)));
#endif
			// END [2012/07/12] Jackal - Modify to fix AC charger unstable bug
			if (usb_chg_current) {
				priv->max_source_current = usb_chg_current;
				/* usb has already indicated us to charge */
				priv->hw_chg_state = CHG_READY_STATE;
				handle_charger_ready(priv);
			}
		}
		else if(chg_type == CHG_TYPE_AC) {
			PEGA_DBG_H("[CHG] %s(), CHG_INSERTED_EVENT, charge type = AC.\n", __func__);
			priv->max_source_current = MAX_CHG_CURR_AC;
			priv->hw_chg_state = CHG_READY_STATE;
			handle_charger_ready(priv);
		}

/* BEGIN [Terry_Tzeng][2012/02/08][Handle charger insert action in dock in type] */
#ifdef MSM_CHARGER_ENABLE_DOCK
		else if(chg_type==CHG_TYPE_DOCK) {

			priv->max_source_current = INSERT_DK_MAX_CURR;
			/* usb has already indicated us to charge */
			priv->hw_chg_state = CHG_READY_STATE;
			handle_charger_ready(priv);
		}
#endif /* MSM_CHARGER_ENABLE_DOCK */
/* END [Terry_Tzeng][2012/02/08][Handle charger insert action in dock in type] */
		else {
			priv->hw_chg_state = CHG_READY_STATE;
			handle_charger_ready(priv);
		}
		/* END [Jackal_Chen][2012/01/16][Modify logic for AC/dock] */
		//wake_lock(&priv->wl);
		break;
	case CHG_ENUMERATED_EVENT:	/* only in USB types */
		if (priv->hw_chg_state == CHG_ABSENT_STATE) {
			dev_info(msm_chg.dev, "%s enum withuot presence\n",
				 hw_chg->name);
			break;
		}
		update_batt_status();
		dev_dbg(msm_chg.dev, "%s enum with %dmA to draw\n",
			 hw_chg->name, priv->max_source_current);
		//20111020-kenlee-modify for remove case of vubs_draw
		if (usb_chg_current == 0) {
			/* BEGIN-KenLee-20111028-add for dynamic charging current depended on backlight level */
#if (BACKLIGHT_DYNAMIC_CURRENT == 1)
			priv->usb_chg_type = USB_CHG_TYPE__INVALID;
#endif
			/* BND-KenLee-20111028-add for dynamic charging current depended on backlight level */

			/* usb subsystem doesnt want us to draw
			 * charging current */
			/* act as if the charge is removed */
			if (priv->hw_chg_state != CHG_PRESENT_STATE)
				handle_charger_removed(priv, CHG_PRESENT_STATE);
		} else {
			/* BEGIN-KenLee-20111028-add for dynamic charging current depended on backlight level */
#if (BACKLIGHT_DYNAMIC_CURRENT == 1)
			if (usb_chg_current < 500)
				break;
			// BEGIN [2012/07/12] Jackal - Modify to fix AC charger unstable bug
#if(MSM_CHARGER_ENABLE_OTG_CHECK)
			else if (usb_chg_current == MAX_CHG_CURR_USB) {
				mWaitOTGReport = 0;
				priv->usb_chg_type = USB_CHG_TYPE__SDP;
			} else {
				mWaitOTGReport = 0;
				priv->usb_chg_type = USB_CHG_TYPE__WALLCHARGER;
			}
#endif
			// END [2012/07/12] Jackal - Modify to fix AC charger unstable bug
#endif
			/* END-KenLee-20111028-add for dynamic charging current depended on backlight level */

			// BEGIN [2012/07/12] Jackal - Modify to fix AC charger unstable bug
#if(MSM_CHARGER_ENABLE_OTG_CHECK)
			if(usb_chg_current == MAX_CHG_CURR_USB || usb_chg_current == MAX_CHG_CURR_USB_WALL) {
				mWaitOTGReport = 0;
			}
#endif
			// END [2012/07/12] Jackal - Modify to fix AC charger unstable bug
			if (priv->hw_chg_state != CHG_READY_STATE) {
				priv->max_source_current = usb_chg_current;
				handle_charger_ready(priv);
			}
		}
		break;
	case CHG_REMOVED_EVENT:
		if (priv->hw_chg_state == CHG_ABSENT_STATE) {
			dev_info(msm_chg.dev, "%s cable already removed\n",
				 hw_chg->name);
			break;
		}
		update_batt_status();
		if (check_charger_type()==CHG_TYPE_USB) {
			usb_chg_current = 0;
			// BEGIN [2012/07/12] Jackal - Modify to fix AC charger unstable bug
#if(MSM_CHARGER_ENABLE_OTG_CHECK)
			mWaitOTGReport = 0;
#endif
			// END [2012/07/12] Jackal - Modify to fix AC charger unstable bug
			notify_usb_of_the_plugin_event(priv, 0);
		}
		/* BEGIN-KenLee-20111028-add for dynamic charging current depended on backlight level */
#if (BACKLIGHT_DYNAMIC_CURRENT == 1)
		priv->usb_chg_type = USB_CHG_TYPE__INVALID;
#endif
		/* END-KenLee-20111028-add for dynamic charging current depended on backlight level */
		handle_charger_removed(priv, CHG_ABSENT_STATE);
		//wake_unlock(&priv->wl);
		break;
	case CHG_DONE_EVENT:
		{
		/* BEGIN [Terry_Tzeng][2012/03/12][Notify battery full event when dock inserted] */
		#ifdef MSM_CHARGER_ENABLE_DOCK
			/* BEGIN [Jackal_Chen][2012/04/09][Use HW_ID value to distinguish GPIO number of dock_in pin] */
			if(!gpio_get_value_cansleep(dock_in_gpio_number))
			/* END [Jackal_Chen][2012/04/09][Use HW_ID value to distinguish GPIO number of dock_in pin] */
			{
				if(msm_ec_dock)
					msm_ec_dock->is_tb_batt_change_cb(priv, CHG_DONE_EVENT);
			}
			else
			{
				if (priv->hw_chg_state == CHG_CHARGING_STATE)
					handle_charging_done(priv);
			}
		#else
		if (priv->hw_chg_state == CHG_CHARGING_STATE)
			handle_charging_done(priv);
		#endif
		/* END [Terry_Tzeng][2012/03/12][Notify battery full event when dock inserted] */
		}
		break;
	case CHG_BATT_BEGIN_FAST_CHARGING:
		/* only update if we are TRKL charging */
		if (msm_chg.batt_status == BATT_STATUS_TRKL_CHARGING)
			msm_chg.batt_status = BATT_STATUS_FAST_CHARGING;
		break;
	case CHG_BATT_NEEDS_RECHARGING:
		msm_chg.batt_status = BATT_STATUS_DISCHARGING;
		handle_battery_inserted();
		priv = msm_chg.current_chg_priv;
		break;
	case CHG_BATT_TEMP_OUTOFRANGE:
		/* the batt_temp out of range can trigger
		 * when the battery is absent */
		if (!is_battery_present()
		    && msm_chg.batt_status != BATT_STATUS_ABSENT) {
			msm_chg.batt_status = BATT_STATUS_ABSENT;
			handle_battery_removed();
			break;
		}
		if (msm_chg.batt_status == BATT_STATUS_TEMPERATURE_OUT_OF_RANGE)
			break;
		msm_chg.batt_status = BATT_STATUS_TEMPERATURE_OUT_OF_RANGE;
		handle_battery_removed();
		break;
	case CHG_BATT_CHG_RESUME:
	case CHG_BATT_TEMP_INRANGE:
		if (msm_chg.batt_status != BATT_STATUS_TEMPERATURE_OUT_OF_RANGE)
			break;
		msm_chg.batt_status = BATT_STATUS_ID_INVALID;
		/* check id */
		if (!is_battery_id_valid())
			break;
		/* assume that we are discharging from the battery
		 * and act as if the battery was inserted
		 * if a charger is present charging will be resumed */
		msm_chg.batt_status = BATT_STATUS_DISCHARGING;
		handle_battery_inserted();
		break;
	case CHG_BATT_INSERTED:
		if (msm_chg.batt_status != BATT_STATUS_ABSENT)
			break;
		/* debounce */
		if (!is_battery_present())
			break;
		msm_chg.batt_status = BATT_STATUS_ID_INVALID;
		if (!is_battery_id_valid())
			break;
		/* assume that we are discharging from the battery */
		msm_chg.batt_status = BATT_STATUS_DISCHARGING;
		/* check if a charger is present */
		handle_battery_inserted();
		break;
	case CHG_BATT_REMOVED:
		if (msm_chg.batt_status == BATT_STATUS_ABSENT)
			break;
		/* debounce */
		if (is_battery_present())
			break;
		msm_chg.batt_status = BATT_STATUS_ABSENT;
		handle_battery_removed();
		break;
	case CHG_BATT_STATUS_CHANGE:
		/* TODO  battery SOC like battery-alarm/charging-full features
		can be added here for future improvement */
		break;
/* BEGIN [Terry_Tzeng][2012/01/13][Add] */
#ifdef MSM_CHARGER_ENABLE_DOCK
	/* BEGIN [Jackal_Chen][2012/01/11][Add dock in/out event handler] */
	case CHG_DOCK_INSERTED_EVENT:
		{
			/* BEGIN [Terry_Tzeng][2012/03/20][Don't repeat to handle dock in event] */
			if(msm_ec_dock)
				msm_ec_dock->is_tb_batt_change_cb(priv, CHG_DOCK_INSERTED_EVENT);
			/* END [Terry_Tzeng][2012/03/20][Don't repeat to handle dock in event] */
			PEGA_DBG_M("[CHG] %s, <...Dock in...>\n", __func__);
			mDockIn = 1;
		}
		break;
	case CHG_DOCK_REMOVED_EVENT:
		{
			if(msm_ec_dock)
				msm_ec_dock->is_tb_batt_change_cb(priv, CHG_DOCK_REMOVED_EVENT);
			PEGA_DBG_M("[CHG] %s, <...Dock out...>\n", __func__);
			mDockIn = 0;
		}
		break;
	case CHG_DOCK_OUT_FINISH_EVENT:
		{
			PEGA_DBG_H("[CHG] %s, Receive CHG_DOCK_OUT_FINISH_EVENT\n", __func__);

			if(!gpio_get_value_cansleep(dock_in_gpio_number) || (priv == NULL))
				break;

			if (priv->hw_chg_state == CHG_ABSENT_STATE) {
				dev_info(msm_chg.dev, "%s cable already removed\n",
					 hw_chg->name);
				break;
			}
			
			update_batt_status();
			
			handle_charger_removed(priv, CHG_ABSENT_STATE);
		}
		break;
	/* BEGIN [Terry_Tzeng][2012/02/15][Rework stop charging action in dock in statement ] */
	case CHG_DOCK_RESTOP_CHG_EVENT:
		{
			PEGA_DBG_M("[CHG] %s, Receive CHG_DOCK_RESTOP_CHG_EVENT\n", __func__);
			msm_chg_restop_charging();
		}
		break;
	/* END [Terry_Tzeng][2012/02/15][Rework started charging action in dock in statement ] */
	/* BEGIN [Terry_Tzeng][2012/02/15][Rework started charging action in dock in statement ] */
	case CHG_DOCK_RESTART_CHG_EVENT:
		{
			PEGA_DBG_M("[CHG] %s, Receive CHG_DOCK_RESTART_CHG_EVENT\n", __func__);
			msm_chg_restart_charging(priv);
		}
		break;
	/* END [Terry_Tzeng][2012/02/15][Rework started charging action in dock in statement ] */
	/* BEGIN [Terry_Tzeng][2012/03/21][Handle setting periodically charging current in suspend status ] */
	case CHG_DOCK_SET_PERIOD_CHG_CURR_EVENT:
		{
			PEGA_DBG_M("[CHG] %s, Receive CHG_DOCK_SET_PERIOD_CHG_CURR_EVENT\n", __func__);
			msm_chg_set_period_chg_curr_for_dk(priv);
		}
		break;
	/* END [Terry_Tzeng][2012/03/21][Handle setting periodically charging current in suspend status ] */
#endif /* MSM_CHARGER_ENABLE_DOCK */
	}
	dev_dbg(msm_chg.dev, "%s %d done batt_status=%d\n", __func__,
		event, msm_chg.batt_status);

	/*if (CHG_BATT_STATUS_CHANGE != event)
		msleep(200);*/

	/* update userspace */
/* BEGIN [Jackal_Chen][2012/03/27][Modify the order of uevent] */
	if (priv) {
		// BEGIN-20110824-KenLee-add "usb" psy for user space
		//power_supply_changed(&priv->psy_ac);
		power_supply_changed(&priv->psy_usb);
		// END-20110824-KenLee-add "usb" psy for user space
		/* BEGIN [Jackal_Chen][2012/01/16][Add "usb_wall_chg" psy for user space] */
		//power_supply_changed(&priv->psy_usb_wall_chg);
		/* END [Jackal_Chen][2012/01/16][Add "usb_wall_chg" psy for user space] */
		PEGA_DBG_H("[CHG] .. after %s, hw_chg_state= %d\n", __func__,priv->hw_chg_state);
	}
	else if (msm_batt_gauge)
	{
		PEGA_DBG_L("[CHG] %s, before ps_changed \n", __func__);
		power_supply_changed(&msm_psy_batt);
	}
/* BEGIN [Terry_Tzeng][2012/01/09][Add dock resource marco] */
#ifdef MSM_CHARGER_ENABLE_DOCK
	else if (msm_ec_dock)
	{
		PEGA_DBG_L("[CHG_EC] %s, before ps_changed \n", __func__);
		power_supply_changed(&msm_psy_dock);
	}
#endif /* MSM_CHARGER_ENABLE_DOCK */
/* END [Terry_Tzeng][2012/01/09][Add dock resource marco] */
/* END [Jackal_Chen][2012/03/27][Modify the order of uevent] */
	mutex_unlock(&msm_chg.status_lock);
	mutex_unlock(&msm_chg.chg_batt_lock);
	// BEGIN [2012/03/22] Jackal - Move wake_unlock here after uevent
	if(CHG_REMOVED_EVENT == event) {
/* BEGIN [Terry_Tzeng][2012/03/23][Switch mode to control wake lock] */
#ifdef MSM_CHARGER_ENABLE_DOCK
		/* BEGIN [Jackal_Chen][2012/04/09][Use HW_ID value to distinguish GPIO number of dock_in pin] */
		if(gpio_get_value_cansleep(dock_in_gpio_number))
		/* END [Jackal_Chen][2012/04/09][Use HW_ID value to distinguish GPIO number of dock_in pin] */
#endif /* MSM_CHARGER_ENABLE_DOCK */
/* END [Terry_Tzeng][2012/03/23][Switch mode to control wake lock] */
		wake_unlock(&priv->wl);
	}
	// END [2012/03/22] Jackal - Move wake_unlock here after uevent

/* BEGIN [Terry_Tzeng][2012/05/04][Set dock wake unlock] */
#ifdef MSM_CHARGER_ENABLE_DOCK
	if((CHG_DOCK_OUT_FINISH_EVENT == event) ||
		(CHG_DOCK_RESTART_CHG_EVENT == event))
	{
		PEGA_DBG_L("[CHG_EC] %s, Handle echger wake unlock action \n", __func__);
		echger_wake_unlock_ext();
	}
#endif /* MSM_CHARGER_ENABLE_DOCK */
/* END [Terry_Tzeng][2012/05/04][Set dock wake unlock] */
}

static int msm_chg_dequeue_event(struct msm_charger_event **event)
{
	unsigned long flags;

	spin_lock_irqsave(&msm_chg.queue_lock, flags);
	if (msm_chg.queue_count == 0) {
		spin_unlock_irqrestore(&msm_chg.queue_lock, flags);
		return -EINVAL;
	}
	*event = &msm_chg.queue[msm_chg.head];
	msm_chg.head = (msm_chg.head + 1) % MSM_CHG_MAX_EVENTS;
	pr_debug("%s dequeueing %d\n", __func__, (*event)->event);
	msm_chg.queue_count--;
	spin_unlock_irqrestore(&msm_chg.queue_lock, flags);
	return 0;
}

static int msm_chg_enqueue_event(struct msm_hardware_charger *hw_chg,
			enum msm_hardware_charger_event event)
{
	unsigned long flags;

	spin_lock_irqsave(&msm_chg.queue_lock, flags);
	if (msm_chg.queue_count == MSM_CHG_MAX_EVENTS) {
		spin_unlock_irqrestore(&msm_chg.queue_lock, flags);
		pr_err("%s: queue full cannot enqueue %d\n",
				__func__, event);
		return -EAGAIN;
	}
	pr_debug("%s queueing %d\n", __func__, event);
	msm_chg.queue[msm_chg.tail].event = event;
	msm_chg.queue[msm_chg.tail].hw_chg = hw_chg;
	msm_chg.tail = (msm_chg.tail + 1)%MSM_CHG_MAX_EVENTS;
	msm_chg.queue_count++;
	spin_unlock_irqrestore(&msm_chg.queue_lock, flags);
	return 0;
}

static void process_events(struct work_struct *work)
{
	struct msm_charger_event *event;
	int rc;

	do {
		rc = msm_chg_dequeue_event(&event);
		if (!rc)
			handle_event(event->hw_chg, event->event);
	} while (!rc);
}




/* BEGIN [Terry_Tzeng][2012/03/07][Set device attude to check tablet input current, charging current and smt charging function] */
#if PEGA_SMT_SOLUTION
#ifdef MSM_CHARGER_ENABLE_DOCK

static ssize_t msm_chg_input_current_show(struct device *dev,		\
				     struct device_attribute *attr,	\
				     char *buf)
{
	return 0;
}

static DEVICE_ATTR(in_curr, S_IRUGO,
		   msm_chg_input_current_show,
		   NULL);


static ssize_t msm_chg_charging_current_show(struct device *dev,		\
				     struct device_attribute *attr,	\
				     char *buf)
{
	return 0;
}


static DEVICE_ATTR(chg_curr, S_IRUGO,
		   msm_chg_charging_current_show,
		   NULL);



static ssize_t msm_chg_smt_charging_status(struct device *dev,		\
				     struct device_attribute *attr,	\
				     char *buf)
{
	int gdk_chg_st;
	int gtb_chg_st;
	int ret;

	gtb_chg_st = is_batt_status_charging();
	ret = get_ec_charging_req_status(&gdk_chg_st);
	if(FAIL == ret)
	{
		ret = sprintf(buf,"Get pad charging st: %d, dock charging st: FAIL, Charging on : 1, Charing off : 0 \n", gtb_chg_st);
	}
	else
	{
		ret = sprintf(buf,"Get pad charging st: %d, dock charging st: %d, Charging on : 1, Charing off : 0 \n", gtb_chg_st, gdk_chg_st);
	}

	return ret;
}

static ssize_t msm_chg_smt_charging_ctrl(struct device *dev,		\
				      struct device_attribute *attr,	\
				      const char *buf,			\
				      size_t count)
{
	PEGA_DBG_H("[CHG]  count = %d, buf= %s\n", count, buf);

	if(0 == strncmp(buf,"0",1)) // Select 0 -> Dock battery is charging to pad
	{
		PEGA_DBG_H("[CHG]  Control dock batter stop charging, tablet battery start charging \n");
		if(msm_ec_dock)
			msm_ec_dock->is_tb_batt_change_cb(msm_chg.current_chg_priv, CHG_DOCK_SMT_DK_CHG_TO_TB);

	}
	else if (0 == strncmp(buf,"1",1)) // Select 1 -> Dock ac 5v is charging to pad
	{
		PEGA_DBG_H("[CHG]  Control tablet batter stop charging, dock battery start charging \n");
		if(msm_ec_dock)
			msm_ec_dock->is_tb_batt_change_cb(msm_chg.current_chg_priv, CHG_DOCK_SMT_AC_CHG_TO_DK);

	}
	else
	{
		PEGA_DBG_H("[CHG_ERR]  Selected smt error\n");
	}


	return count;
}


static DEVICE_ATTR(smt_ctrl_chg, S_IWUSR | S_IRUGO,
		   msm_chg_smt_charging_status,
		   msm_chg_smt_charging_ctrl);

/* BEGIN [Terry_Tzeng][2012/03/09][Control tablet otg disable pin action by attrude note] */
static ssize_t msm_chg_tb_otg_dis_status(struct device *dev,		\
				     struct device_attribute *attr,	\
				     char *buf)
{
	return gpio_get_value_cansleep(PM8921_GPIO_TO_SYS(PM8921_TB_OTG_PW_GPIO));
}

static ssize_t msm_chg_tb_otg_dis_ctrl(struct device *dev,		\
				      struct device_attribute *attr,	\
				      const char *buf,			\
				      size_t count)
{
	int t_pw_st;
	int cmd_act;
	int reval;

	PEGA_DBG_H("[CHG]  count = %d, buf= %s\n", count, buf);

	if(0 == strncmp(buf,"0",1))
	{
		cmd_act = 0;
	}
	else if(0 == strncmp(buf,"0",1))
	{
		cmd_act = 1;
	}
	else
	{
		PEGA_DBG_H("[CHG_ERR]  Selected otg disablet action number is fail , OTG_DIS open : 0, OTG_DIS close : 1\n");
	}

	PEGA_DBG_H("[CHG]  Selected otg disablet action = %d , OTG_DIS open : 0, OTG_DIS close : 1\n", cmd_act);

	t_pw_st = gpio_get_value_cansleep(PM8921_GPIO_TO_SYS(PM8921_TB_OTG_PW_GPIO));

	if(t_pw_st != cmd_act)
	{
		struct pm_gpio tb_otg_dis_gpio_config = {
				.direction      = PM_GPIO_DIR_OUT,
				.pull           = PM_GPIO_PULL_NO,
				.out_strength   = PM_GPIO_STRENGTH_HIGH,
				.function       = PM_GPIO_FUNC_NORMAL,
				.inv_int_pol    = 0,
				.vin_sel        = 2,
				.output_buffer  = PM_GPIO_OUT_BUF_CMOS,
				.output_value   = 1,
			};

		tb_otg_dis_gpio_config.output_value = cmd_act;
		reval = pm8xxx_gpio_config(PM8921_GPIO_TO_SYS(PM8921_TB_OTG_PW_GPIO), &tb_otg_dis_gpio_config);
		if (reval) {
			PEGA_DBG_H("%s: pm8921 gpio %d config failed(%d)\n",
					__func__, PM8921_GPIO_TO_SYS(PM8921_TB_OTG_PW_GPIO), reval);
		}
	}

	return count;
}



static DEVICE_ATTR(otg_dis_ctrl, S_IWUSR | S_IRUGO ,
		   msm_chg_tb_otg_dis_status,
		   msm_chg_tb_otg_dis_ctrl);
/* END [Terry_Tzeng][2012/03/09][Control tablet otg disable pin action by attrude note] */

static struct attribute *pega_msm_chg_attrs[] = {
	&dev_attr_in_curr.attr,
	&dev_attr_chg_curr.attr,
	&dev_attr_smt_ctrl_chg.attr,
	&dev_attr_otg_dis_ctrl.attr,
	NULL,
};

static struct attribute_group pega_msm_chg_attr_group = {
	.attrs = pega_msm_chg_attrs,
};

#endif /* MSM_CHARGER_ENABLE_DOCK */
#endif /* PEGA_SMT_SOLUTION */
/* END [Terry_Tzeng][2012/03/07][Set device attude to check tablet input current, charging current and smt charging function] */

/* USB calls these to tell us how much charging current we should draw */
void msm_charger_vbus_draw(unsigned int mA)
{

	// BEGIN [2012/08/22] Jackal - Modify to stop retry if OTG report value = 2
	if(2 == mA)
	{
		PEGA_DBG_H("[CHG] %s, mA= 2, stop retry.\n", __func__);
		mWaitOTGReport = 0;
	}
	// END [2012/08/22] Jackal - Modify to stop retry if OTG report value = 2
	if ((mA != 0) && (mA != 100) && (mA != MAX_CHG_CURR_USB) && (mA != MAX_CHG_CURR_USB_WALL))
		return;

	PEGA_DBG_H("[CHG] %s, mA= %d\n", __func__, mA);

//	if (is_status_lock_init)
//		mutex_lock(&msm_chg.status_lock);

	// [2012/01/03] Jackal - Fix sharging null pointer bug
	spin_lock(&usb_chg_lock);
	if (usb_hw_chg_priv) {
		if ((mA == 0) || (mA == MAX_CHG_CURR_USB) || (mA == MAX_CHG_CURR_USB_WALL)) {
			usb_chg_current = mA;
			PEGA_DBG_H("[CHG] %s, issue CHG_ENUMERATED_EVENT \n", __func__);
			msm_charger_notify_event(usb_hw_chg_priv->hw_chg, CHG_ENUMERATED_EVENT);
		}
	} else {
		PEGA_DBG_H("[CHG_ERR] %s called early; charger isnt initialized, mA= %d\n", __func__, mA);
		/* remember the current, to be used when charger is ready */
		if ((mA == 0) || (mA == MAX_CHG_CURR_USB) || (mA == MAX_CHG_CURR_USB_WALL))
			usb_chg_current = mA;
	}
	spin_unlock(&usb_chg_lock);
	// [2012/01/03] Jackal - Fix sharging null pointer bug End
	//if (is_status_lock_init)
	//	mutex_unlock(&msm_chg.status_lock);

	PEGA_DBG_H("[CHG] .. after %s, mA= %d \n", __func__, mA);
}

static int __init determine_initial_batt_status(void)
{
	int rc;
	PEGA_DBG_M("[CHG] %s \n", __func__);

	if (is_battery_present())
		if (is_battery_id_valid())
			if (is_battery_temp_within_range())
			{
				if ((msm_chg.batt_status != BATT_STATUS_TRKL_CHARGING)
					&& (msm_chg.batt_status != BATT_STATUS_FAST_CHARGING))
					msm_chg.batt_status = BATT_STATUS_DISCHARGING;
			}
			else
				msm_chg.batt_status
				    = BATT_STATUS_TEMPERATURE_OUT_OF_RANGE;
		else
			msm_chg.batt_status = BATT_STATUS_ID_INVALID;
	else
		msm_chg.batt_status = BATT_STATUS_ABSENT;

	//if (is_batt_status_capable_of_charging())
	//	handle_battery_inserted();

	rc = power_supply_register(msm_chg.dev, &msm_psy_batt);
	if (rc < 0) {
		dev_err(msm_chg.dev, "%s: power_supply_register failed"
			" rc=%d\n", __func__, rc);
		return rc;
	}

	/* start updaing the battery powersupply every msm_chg.update_time
	 * milliseconds */
	queue_delayed_work(msm_chg.event_wq_thread,
				&msm_chg.update_heartbeat_work,
			      round_jiffies_relative(msecs_to_jiffies
						     (msm_chg.update_time)));

	pr_debug("%s:OK batt_status=%d\n", __func__, msm_chg.batt_status);
	return 0;
}

/* BEGIN [Terry_Tzeng][2012/01/09][Add dock resource marco] */
#ifdef MSM_CHARGER_ENABLE_DOCK
static int __init determine_initial_ecbatt_status(void)
{
	int rc;
	PEGA_DBG_M("[CHG] %s \n", __func__);

	if (is_ecbatt_present())
		if (is_ecbatt_id_valid())
			if (is_ecbatt_temp_within_range())
			{
				if ((msm_chg.ecbatt_status != BATT_STATUS_TRKL_CHARGING)
					&& (msm_chg.ecbatt_status != BATT_STATUS_FAST_CHARGING))
					msm_chg.ecbatt_status = BATT_STATUS_DISCHARGING;
			}
			else
				msm_chg.ecbatt_status
				    = BATT_STATUS_TEMPERATURE_OUT_OF_RANGE;
		else
			msm_chg.ecbatt_status = BATT_STATUS_ID_INVALID;
	else
		msm_chg.ecbatt_status = BATT_STATUS_ABSENT;

	//if (is_batt_status_capable_of_charging())
	//	handle_battery_inserted();

	rc = power_supply_register(msm_chg.dev, &msm_psy_dock);
	if (rc < 0) {
		dev_err(msm_chg.dev, "%s: power_supply_register failed"
			" rc=%d\n", __func__, rc);
		return rc;
	}

	/* start updaing the battery powersupply every msm_chg.update_time
	 * milliseconds */
	/*queue_delayed_work(msm_chg.event_wq_thread,
				&msm_chg.update_heartbeat_work,
			      round_jiffies_relative(msecs_to_jiffies
						     (msm_chg.update_time)));*/

	pr_debug("%s:OK batt_status=%d\n", __func__, msm_chg.batt_status);
	return 0;
}
#endif /* MSM_CHARGER_ENABLE_DOCK */
/* END [Terry_Tzeng][2012/01/09][Add dock resource marco] */

static int __devinit msm_charger_probe(struct platform_device *pdev)
{

	PEGA_DBG_H("[CHG] %s \n", __func__);

	msm_chg.dev = &pdev->dev;
	if (pdev->dev.platform_data) {
		unsigned int milli_secs;

		struct msm_charger_platform_data *pdata
		    = (struct msm_charger_platform_data *)pdev->dev.platform_data;

		milli_secs = pdata->safety_time * 60 * MSEC_PER_SEC;
		if (milli_secs > jiffies_to_msecs(MAX_JIFFY_OFFSET)) {
			dev_warn(&pdev->dev, "%s: safety time too large"
				 "%dms\n", __func__, milli_secs);
			milli_secs = jiffies_to_msecs(MAX_JIFFY_OFFSET);
		}
		msm_chg.safety_time = milli_secs;

		/* BEGIN [Jackal_Chen][2012/04/23][Modify safety time of wall charger] */
		milli_secs = pdata->wall_safety_time * 60 * MSEC_PER_SEC;
		if (milli_secs > jiffies_to_msecs(MAX_JIFFY_OFFSET)) {
			dev_warn(&pdev->dev, "%s: safety time too large"
				 "%dms\n", __func__, milli_secs);
			milli_secs = jiffies_to_msecs(MAX_JIFFY_OFFSET);
		}
		msm_chg.wall_safety_time = milli_secs;
		/* END [Jackal_Chen][2012/04/23][Modify safety time of wall charger] */

		milli_secs = pdata->usb_safety_time * 60 * MSEC_PER_SEC;
		if (milli_secs > jiffies_to_msecs(MAX_JIFFY_OFFSET)) {
			dev_warn(&pdev->dev, "%s: safety time too large"
				 "%dms\n", __func__, milli_secs);
			milli_secs = jiffies_to_msecs(MAX_JIFFY_OFFSET);
		}
		msm_chg.usb_safety_time = milli_secs;

		milli_secs = pdata->update_time * 60 * MSEC_PER_SEC;
		if (milli_secs > jiffies_to_msecs(MAX_JIFFY_OFFSET)) {
			dev_warn(&pdev->dev, "%s: safety time too large"
				 "%dms\n", __func__, milli_secs);
			milli_secs = jiffies_to_msecs(MAX_JIFFY_OFFSET);
		}
		msm_chg.update_time = milli_secs;

		msm_chg.max_voltage = pdata->max_voltage;
		msm_chg.min_voltage = pdata->min_voltage;
		msm_chg.get_batt_capacity_percent = pdata->get_batt_capacity_percent;
	}
	if (msm_chg.safety_time == 0)
		msm_chg.safety_time = CHARGING_TEOC_MS;
	if (msm_chg.update_time == 0)
		msm_chg.update_time = UPDATE_TIME_MS;
	if (msm_chg.max_voltage == 0)
		msm_chg.max_voltage = DEFAULT_BATT_MAX_V;
	if (msm_chg.min_voltage == 0)
		msm_chg.min_voltage = DEFAULT_BATT_MIN_V;
	if (msm_chg.get_batt_capacity_percent == NULL)
		msm_chg.get_batt_capacity_percent = msm_chg_get_batt_capacity_percent;

	mutex_init(&msm_chg.status_lock);
	mutex_init(&msm_chg.chg_batt_lock);
	is_status_lock_init = 1;
	INIT_DELAYED_WORK(&msm_chg.teoc_work, teoc);
	INIT_DELAYED_WORK(&msm_chg.resume_work, resume_charging);
	// BEGIN [2012/07/12] Jackal - Modify to fix AC charger unstable bug
#if(MSM_CHARGER_ENABLE_OTG_CHECK)
	INIT_DELAYED_WORK(&msm_chg.wait_otg_report_work, wait_otg_report);
#endif
	// END [2012/07/12] Jackal - Modify to fix AC charger unstable bug
	INIT_DELAYED_WORK(&msm_chg.update_heartbeat_work, update_heartbeat);
	/* BEGIN [Jackal_Chen][2012/01/16][Add flag of dock in/out] */
	// Wait SW 5 move dock_in pin config to kernel init
#if 0
	mDockIn = !gpio_get_value_cansleep(dock_in_gpio_number);
	PEGA_DBG_H("[CHG] %s, dock_in = %d \n", __func__, mDockIn);
#endif
	/* END [Jackal_Chen][2012/01/16][Add flag of dock in/out] */
	/* BEGIN [Jackal_Chen][2012/04/09][Use HW_ID value to distinguish GPIO number of dock_in pin] */
	if(get_pega_hw_board_version_status() > 2) {
		PEGA_DBG_H("[CHG] %s, device is SIT2 or later \n", __func__);
		dock_in_gpio_number = MSM8960_DOCK_IN_GPIO_SIT2;
	} else {
		PEGA_DBG_H("[CHG] %s, device is EVT/DVT/PVT(SIT1) \n", __func__);
	}
	/* END [Jackal_Chen][2012/04/09][Use HW_ID value to distinguish GPIO number of dock_in pin] */
/* BEGIN [Terry_Tzeng][2012/02/15][Get regulator GPIO 42 interface to control 5V boost power switch of tablet in dock in statement] */
#ifdef MSM_CHARGER_ENABLE_DOCK
	msm_chg.regulator42_ctl_api = regulator_get(msm_chg.dev, "ecchg_tb_5vbst");

/* BEGIN [Terry_Tzeng][2012/03/28][Set early suspend function for charging entried suspend funciton] */
	msm_chg.charge_early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	msm_chg.charge_early_suspend.suspend = msm_chg_early_suspend;
	msm_chg.charge_early_suspend.resume =msm_chg_late_resume;
	register_early_suspend(&msm_chg.charge_early_suspend);
/* END [Terry_Tzeng][2012/03/28][Set early suspend function for charging entried suspend funciton] */
#if PEGA_SMT_SOLUTION
/* BEGIN [Terry_Tzeng][2012/03/07][Set device attude to check tablet input current, charging current and smt charging function] */
	if (sysfs_create_group(&pdev->dev.kobj, &pega_msm_chg_attr_group)) {
		PEGA_DBG_H("[CHG] %s, Unable to export keys/switches \n", __func__);
		sysfs_remove_group(&pdev->dev.kobj, &pega_msm_chg_attr_group);
	}
/* END [Terry_Tzeng][2012/03/07][Set device attude to check tablet input current, charging current and smt charging function] */
#endif /* PEGA_SMT_SOLUTION */
#endif /* MSM_CHARGER_ENABLE_DOCK */

/* END [Terry_Tzeng][2012/02/15][Get regulator GPIO 42 interface to control 5V boost power switch of tablet in dock in statement] */



	return 0;
}

static int __devexit msm_charger_remove(struct platform_device *pdev)
{
	mutex_destroy(&msm_chg.status_lock);
	power_supply_unregister(&msm_psy_batt);
/* BEGIN [Terry_Tzeng][2012/01/09][Add dock resource marco] */
#ifdef MSM_CHARGER_ENABLE_DOCK
	power_supply_unregister(&msm_psy_dock);
#endif /* MSM_CHARGER_ENABLE_DOCK */
/* END [Terry_Tzeng][2012/01/09][Add dock resource marco] */
	return 0;
}

int msm_charger_notify_event(struct msm_hardware_charger *hw_chg,
			     enum msm_hardware_charger_event event)
{
	msm_chg_enqueue_event(hw_chg, event);
	queue_work(msm_chg.event_wq_thread, &msm_chg.queue_work);
	return 0;
}
EXPORT_SYMBOL(msm_charger_notify_event);

int msm_charger_register(struct msm_hardware_charger *hw_chg)
{
	struct msm_hardware_charger_priv *priv;
	int rc = 0;

	if (!msm_chg.inited) {
		PEGA_DBG_H("[CHG_ERR] %s, msm_chg is NULL, Too early to register!\n", __func__);
		return -EAGAIN;
	}
	PEGA_DBG_H("[CHG] %s\n", __func__);

	if (hw_chg->start_charging == NULL
		|| hw_chg->stop_charging == NULL
		|| hw_chg->name == NULL
		|| hw_chg->rating == 0) {
		PEGA_DBG_H("[CHG_ERR] %s, invalid hw_chg!\n", __func__);
		return -EINVAL;
	}

	priv = kzalloc(sizeof *priv, GFP_KERNEL);
	if (priv == NULL) {
		PEGA_DBG_H("[CHG_ERR] %s, kzalloc failed!\n", __func__);
		return -ENOMEM;
	}

	/* BEGIN [Jackal_Chen][2012/01/16][Add "usb_wall_chg" psy for user space] */
	// BEGIN-20110824-KenLee-add "usb" psy for user space
	priv->psy_ac.name = "ac";
	priv->psy_ac.type = POWER_SUPPLY_TYPE_MAINS;
	priv->psy_ac.supplied_to = msm_power_supplied_to;
	priv->psy_ac.num_supplicants = ARRAY_SIZE(msm_power_supplied_to);
	priv->psy_ac.properties = msm_power_props;
	priv->psy_ac.num_properties = ARRAY_SIZE(msm_power_props);
	priv->psy_ac.get_property = msm_power_get_property;

	priv->psy_usb.name = "usb";
	priv->psy_usb.type = POWER_SUPPLY_TYPE_USB;
	priv->psy_usb.supplied_to = msm_power_supplied_to;
	priv->psy_usb.num_supplicants = ARRAY_SIZE(msm_power_supplied_to);
	priv->psy_usb.properties = msm_power_props;
	priv->psy_usb.num_properties = ARRAY_SIZE(msm_power_props);
	priv->psy_usb.get_property = msm_power_get_property;

	priv->psy_usb_wall_chg.name = "usb_wall_chg";
	priv->psy_usb_wall_chg.type = POWER_SUPPLY_TYPE_MAINS;
	priv->psy_usb_wall_chg.supplied_to = msm_power_supplied_to;
	priv->psy_usb_wall_chg.num_supplicants = ARRAY_SIZE(msm_power_supplied_to);
	priv->psy_usb_wall_chg.properties = msm_power_props;
	priv->psy_usb_wall_chg.num_properties = ARRAY_SIZE(msm_power_props);
	priv->psy_usb_wall_chg.get_property = msm_power_get_property;

	wake_lock_init(&priv->wl, WAKE_LOCK_SUSPEND, priv->psy_ac.name);

	rc = power_supply_register(NULL, &priv->psy_ac);
	if (rc) {
		PEGA_DBG_H("[CHG_ERR] %s, power_supply_register - psy_ac failed!\n", __func__);
		goto out;
	}
	rc = power_supply_register(NULL, &priv->psy_usb);
	if (rc) {
		PEGA_DBG_H("[CHG_ERR] %s, power_supply_register - psy_usb failed\n",
			__func__);
		goto out;
	}
	rc = power_supply_register(NULL, &priv->psy_usb_wall_chg);
	if (rc) {
		PEGA_DBG_H("[CHG_ERR] %s, power_supply_register - psy_usb_wall_chg failed\n",
			__func__);
		goto out;
	}
	// END-20110824-KenLee-add "usb" psy for user space
	/* END [Jackal_Chen][2012/01/16][Add "usb_wall_chg" psy for user space] */


	priv->hw_chg = hw_chg;
	priv->hw_chg_state = CHG_ABSENT_STATE;
	/* BEGIN-KenLee-20111028-add for dynamic charging current depended on backlight level */
#if (BACKLIGHT_DYNAMIC_CURRENT == 1)
	priv->usb_chg_type = USB_CHG_TYPE__INVALID;
#endif
	/* END-KenLee-20111028-add for dynamic charging current depended on backlight level */
	INIT_LIST_HEAD(&priv->list);
	mutex_lock(&msm_chg.msm_hardware_chargers_lock);
	list_add_tail(&priv->list, &msm_chg.msm_hardware_chargers);
	mutex_unlock(&msm_chg.msm_hardware_chargers_lock);
	hw_chg->charger_private = (void *)priv;
	PEGA_DBG_M("[CHG] .. after %s, hw_chg_state= %d \n", __func__, priv->hw_chg_state);
	return 0;

out:
	wake_lock_destroy(&priv->wl);
	kfree(priv);
	return rc;
}
EXPORT_SYMBOL(msm_charger_register);

void msm_battery_gauge_register(struct msm_battery_gauge *batt_gauge)
{
	PEGA_DBG_M("[CHG] %s \n", __func__);
	if (msm_batt_gauge) {
		msm_batt_gauge = batt_gauge;
		pr_err("msm-charger %s multiple battery gauge called\n",
								__func__);
	} else {
		//20110823-KenLee-need to register msm_psy_batt to power supply before msm_batt_gauge assigment.
		determine_initial_batt_status();
		msm_batt_gauge = batt_gauge;
	}
}
EXPORT_SYMBOL(msm_battery_gauge_register);

void msm_battery_gauge_unregister(struct msm_battery_gauge *batt_gauge)
{
	msm_batt_gauge = NULL;
}
EXPORT_SYMBOL(msm_battery_gauge_unregister);

/* BEGIN [Terry_Tzeng][2012/01/09][Add dock resource marco] */
#ifdef MSM_CHARGER_ENABLE_DOCK
void msm_dock_ec_register(struct msm_ec_dock *dock_gauge)
{
	PEGA_DBG_M("[CHG] %s \n", __func__);
	msm_ec_dock = dock_gauge;
}
EXPORT_SYMBOL(msm_dock_ec_register);

void msm_dock_ec_unregister(struct msm_ec_dock *dock_gauge)
{
	msm_ec_dock = NULL;
}
EXPORT_SYMBOL(msm_dock_ec_unregister);

void pega_ecbatt_gauge_register(struct msm_battery_gauge *ecbatt_gauge)
{
	PEGA_DBG_H("[CHG] %s \n", __func__);
	if (pega_ecbatt_gauge) {
		pega_ecbatt_gauge = ecbatt_gauge;
		pr_err("msm-charger %s multiple ec battery gauge is called.\n",
								__func__);
	} else {
		determine_initial_ecbatt_status();
		pega_ecbatt_gauge = ecbatt_gauge;
		//pega_ecbatt_init();
	}
}
EXPORT_SYMBOL(pega_ecbatt_gauge_register);

void pega_ecbatt_gauge_unregister(struct msm_battery_gauge *ecbatt_gauge)
{
	pega_ecbatt_exit();
	pega_ecbatt_gauge = NULL;
}
EXPORT_SYMBOL(pega_ecbatt_gauge_unregister);
#endif /* MSM_CHARGER_ENABLE_DOCK */
/* END [Terry_Tzeng][2012/01/09][Add dock resource marco] */

int msm_charger_unregister(struct msm_hardware_charger *hw_chg)
{
	struct msm_hardware_charger_priv *priv;

	priv = (struct msm_hardware_charger_priv *)(hw_chg->charger_private);
	mutex_lock(&msm_chg.msm_hardware_chargers_lock);
	list_del(&priv->list);
	mutex_unlock(&msm_chg.msm_hardware_chargers_lock);
	wake_lock_destroy(&priv->wl);
// BEGIN-20110824-KenLee-add "usb" psy for user space
	power_supply_unregister(&priv->psy_ac);
	power_supply_unregister(&priv->psy_usb);
// END-20110824-KenLee-add "usb" psy for user space
/* BEGIN [Jackal_Chen][2012/01/16][Add "usb_wall_chg" property] */
	power_supply_unregister(&priv->psy_usb_wall_chg);
/* END [Jackal_Chen][2012/01/16][Add "usb_wall_chg" property] */
	kfree(priv);
	return 0;
}
EXPORT_SYMBOL(msm_charger_unregister);

#ifdef MSM_CHARGER_ENABLE_DOCK
/* BEGIN [Terry_Tzeng][2012/03/28][Set early suspend function for charging entried suspend funciton] */
void msm_chg_early_suspend(struct early_suspend *h)
{
	PEGA_DBG_H("[CHG] %s \n", __func__);
	if(!gpio_get_value_cansleep(MSM8960_DOCK_IN_GPIO))
	{
		if(NULL == msm_ec_dock)
		{
			PEGA_DBG_H("[CHG_ERR] %s, msm_ec_dock is null point \n", __func__);
			return;
		}

		msm_ec_dock->tb_early_suspend_notify();
	}
}

void msm_chg_late_resume(struct early_suspend *h)
{
	PEGA_DBG_H("[CHG] %s \n", __func__);
	if(!gpio_get_value_cansleep(MSM8960_DOCK_IN_GPIO))
	{
		if(NULL == msm_ec_dock)
		{
			PEGA_DBG_H("[CHG_ERR] %s, msm_ec_dock is null point \n", __func__);
			return;
		}

		msm_ec_dock->tb_late_resume_notify();
	}
}
#endif /* MSM_CHARGER_ENABLE_DOCK */

static int msm_charger_suspend(struct device *dev)
{
	PEGA_DBG_L("[CHG] %s \n", __func__);
	dev_dbg(msm_chg.dev, "%s suspended\n", __func__);
	msm_chg.stop_update = 1;
	cancel_delayed_work(&msm_chg.update_heartbeat_work);
	mutex_lock(&msm_chg.status_lock);
/* BEGIN [Terry_Tzeng][2012/03/23][Switch mode to control wake lock] */
#ifdef MSM_CHARGER_ENABLE_DOCK
	// BEGIN [Tim_PH][2012/04/20][clear cache of ecbattery when suspense]
	if (pega_ecbatt_gauge && pega_ecbatt_gauge->set_batt_suspend)
		pega_ecbatt_gauge->set_batt_suspend();
	// END [Tim_PH][2012/04/20][clear cache of ecbattery when suspense]
	/* BEGIN [Jackal_Chen][2012/04/09][Use HW_ID value to distinguish GPIO number of dock_in pin] */
	if(gpio_get_value_cansleep(dock_in_gpio_number))
	/* END [Jackal_Chen][2012/04/09][Use HW_ID value to distinguish GPIO number of dock_in pin] */
#endif /* MSM_CHARGER_ENABLE_DOCK */
/* END [Terry_Tzeng][2012/03/23][Switch mode to control wake lock] */
	handle_battery_removed();
	mutex_unlock(&msm_chg.status_lock);
	return 0;
}

static int msm_charger_resume(struct device *dev)
{
	PEGA_DBG_L("[CHG] %s \n", __func__);
	dev_dbg(msm_chg.dev, "%s resumed\n", __func__);
	msm_chg.stop_update = 0;
	/* start updaing the battery powersupply every msm_chg.update_time
	 * milliseconds */
	queue_delayed_work(msm_chg.event_wq_thread,
				&msm_chg.update_heartbeat_work,
			      round_jiffies_relative(msecs_to_jiffies
						     (msm_chg.update_time)));
	mutex_lock(&msm_chg.status_lock);
/* BEGIN [Terry_Tzeng][2012/03/23][Switch mode to control wake lock] */
#ifdef MSM_CHARGER_ENABLE_DOCK
	/* BEGIN [Jackal_Chen][2012/04/09][Use HW_ID value to distinguish GPIO number of dock_in pin] */
	if(gpio_get_value_cansleep(dock_in_gpio_number))
	/* END [Jackal_Chen][2012/04/09][Use HW_ID value to distinguish GPIO number of dock_in pin] */
#endif /* MSM_CHARGER_ENABLE_DOCK */
/* END [Terry_Tzeng][2012/03/23][Switch mode to control wake lock] */
	handle_battery_inserted();
	mutex_unlock(&msm_chg.status_lock);
	return 0;
}

static SIMPLE_DEV_PM_OPS(msm_charger_pm_ops,
		msm_charger_suspend, msm_charger_resume);

static struct platform_driver msm_charger_driver = {
	.probe = msm_charger_probe,
	.remove = __devexit_p(msm_charger_remove),
	.driver = {
		   .name = "msm-charger",
		   .owner = THIS_MODULE,
		   .pm = &msm_charger_pm_ops,
	},
};

static int __init msm_charger_init(void)
{
	int rc;

	INIT_LIST_HEAD(&msm_chg.msm_hardware_chargers);
	msm_chg.count_chargers = 0;
	mutex_init(&msm_chg.msm_hardware_chargers_lock);

	msm_chg.queue = kzalloc(sizeof(struct msm_charger_event)
				* MSM_CHG_MAX_EVENTS,
				GFP_KERNEL);
	if (!msm_chg.queue) {
		rc = -ENOMEM;
		goto out;
	}
	msm_chg.tail = 0;
	msm_chg.head = 0;
	spin_lock_init(&msm_chg.queue_lock);
	msm_chg.queue_count = 0;
	INIT_WORK(&msm_chg.queue_work, process_events);
	msm_chg.event_wq_thread = create_workqueue("msm_charger_eventd");
	if (!msm_chg.event_wq_thread) {
		rc = -ENOMEM;
		goto free_queue;
	}
	rc = platform_driver_register(&msm_charger_driver);
	if (rc < 0) {
		pr_err("%s: FAIL: platform_driver_register. rc = %d\n",
		       __func__, rc);
		goto destroy_wq_thread;
	}
	msm_chg.inited = 1;
	// [2012/01/03] Jackal - Fix sharging null pointer bug
	spin_lock_init(&usb_chg_lock);
	// [2012/01/03] Jackal - Fix sharging null pointer bug End
	return 0;

destroy_wq_thread:
	destroy_workqueue(msm_chg.event_wq_thread);
free_queue:
	kfree(msm_chg.queue);
out:
	return rc;
}

static void __exit msm_charger_exit(void)
{
	flush_workqueue(msm_chg.event_wq_thread);
	destroy_workqueue(msm_chg.event_wq_thread);
	kfree(msm_chg.queue);
	platform_driver_unregister(&msm_charger_driver);
/* BEGIN [Terry_Tzeng][2012/02/15][Release regulator GPIO 42 interface to control 5V boost power switch of tablet in dock in statement] */
#ifdef MSM_CHARGER_ENABLE_DOCK
	if (msm_chg.regulator42_ctl_api)
		regulator_put(msm_chg.regulator42_ctl_api);
#endif /* MSM_CHARGER_ENABLE_DOCK */
/* END [Terry_Tzeng][2012/02/15][Release regulator GPIO 42 interface to control 5V boost power switch of tablet in dock in statement] */

}

module_init(msm_charger_init);
module_exit(msm_charger_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Abhijeet Dharmapurikar <adharmap@codeaurora.org>");
MODULE_DESCRIPTION("Battery driver for Qualcomm MSM chipsets.");
MODULE_VERSION("1.0");
