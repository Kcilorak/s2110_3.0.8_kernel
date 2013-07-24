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

#include <linux/slab.h>
#include <linux/power_supply.h>
#include <linux/err.h>
#include <linux/msm-charger.h>
/* BEGIN [Terry_Tzeng][2011/01/09][Include ec to tablet api and resource] */
#include <linux/nuvec_api.h>
/* END [Terry_Tzeng][2011/01/09][Include ec to tablet api and resource] */
#include <linux/gpio.h>
#include <linux/mfd/pm8xxx/gpio.h>
#include <linux/delay.h>
/* BEGIN [Tim_PH][2012/05/03][For dynamically log mechanism] */
#include <linux/msm-charger-log.h>
/* END [Tim_PH][2012/05/03][For dynamically log mechanism] */


enum echger_batt_cap_level {
	BATT_FULL_LV,
	BATT_NORMAL_LV,
	BATT_LOW_LV,
	BATT_ERR,
};

enum echger_tb_dk_action_st {
	DOCK_ONLY,
	DOCK_AC_5V_IN,
	DOCK_AC_INF_IN,
	DOCK_AC_12V_IN,
	DOCK_USB_5V_IN,
	DOCK_OUT,
	DOCK_RESET,
};

struct echger_mux
{
	struct msm_hardware_charger_priv * hw_chg_priv;
	struct msm_hardware_charger * hw_chg;
	enum echger_batt_cap_level curr_tb_batt_lv;
	enum echger_batt_cap_level curr_dk_batt_lv;
	enum echger_tb_dk_action_st curr_tb_dk_act;
	int tb_bst_pw_st;
	int tb_otg_pw_st;
	int dk_otg_bst_st;
	int dk_sys_bst_st;
	int tb_batt_full_stop;
	struct mutex status_lock;
	int wakelock_flag;
/* BEGIN [Terry_Tzeng][2012/04/24][Drop charging current when dock battery is low statement in AC 5V or USB in]  */
	int drop_curr_flg;
/* END [Terry_Tzeng][2012/04/24][Drop charging current when dock battery is low statement in AC 5V or USB in]  */
/* BEGIN [Terry_Tzeng][2012/05/03][Save system suspend status]  */
	int suspend_status;
/* END [Terry_Tzeng][2012/05/03][Save system suspend status]  */
/* BEGIN [Terry_Tzeng][2012/05/03][Set wakelock error handler] */
	int wakeup_t_retry_flg;
/* END [Terry_Tzeng][2012/05/03][Set wakelock error handler] */
	int *queue_evt;
	int tail;
	int head;
	spinlock_t queue_lock;
	int queue_count;
	struct work_struct queue_work;
	struct workqueue_struct *event_wq_thread;
};

#define T_5BST_PW                       0
#define T_OTG_PW                        1

#define PW_OFF                          1
#define PW_ON                           0

#define TB_BATT_STOP_CHG                1
#define TB_BATT_START_CHG               0

#define TABLET_BATT_TYPE                0
#define DOCK_BATT_TYPE                  1

/* BEGIN [Terry_Tzeng][2012/04/24][Define drop charging current marco when dock battery is low statement in AC 5V or USB in]  */
#define DROP_CURR_EN                    1
#define DROP_CURR_DIS                   0
#define RECOVERY_CHG_CURR_CAP_PERCENT   15
#define DROP_USB_CHG_CURR_CAP_PERCENT   5
/* END [Terry_Tzeng][2012/04/24][Define drop charging current marco when dock battery is low statement in AC 5V or USB in]  */

/* BEGIN [Terry_Tzeng][2012/05/03][Define system suspend status]  */
#define SUSPEND_STATUS_EN               1
#define SUSPEND_STATUS_DIS              0
/* END [Terry_Tzeng][2012/05/03][Define system suspend status]  */


/* BEGIN [Terry_Tzeng][2012/02/28][Charging battery full capacity is 50% for SMT version]  */
#ifdef PEGA_SMT_SOLUTION
#define SMT_BATT_FULL_CAP_PERCENT               50
#endif /* PEGA_SMT_SOLUTION */
/* END [Terry_Tzeng][2012/02/28][Charging battery full capacity is 50% for SMT version]  */
#define BATT_FULL_CAP_PERCENT                   100
#define BATT_LOW_CAP_PERCENT                    0

#define DRIVER_VERSION                          "1.0.0"
#define CHG_MAX_VOLTAGE                         4200

#define CHG_DOCK_IN                             1000
#define CHG_DOCK_OUT                            1001
#define CHG_DOCK_HEARTBEAT_CHK                  1002
/* BEGIN [Terry_Tzeng][2012/03/12][Define received tablet battery full event] */
#define CHG_TABLET_BAT_FULL                     1003
/* BEGIN [Terry_Tzeng][2012/05/03][Handle suspend to late resume status] */
#define CHG_DOCK_SYS_SUSPEND_TO_LATE_RESUME     1004
/* END [Terry_Tzeng][2012/05/03][Handle suspend to late resume status] */
#define CHG_DOCK_SUSPEND_HEARTBEAT_CHK          DOCK_WKUP
#define CHG_DOCK_AC_OUT                         AC_OUT
#define CHG_DOCK_AC_5V_IN                       AC_5V_IN
#define CHG_DOCK_AC_12V_IN                      AC_12V_IN
#define CHG_DOCK_USB_IN                         USB_5V_IN
#define CHG_DOCK_BAT_ERR                        BAT_ERR
#define CHG_DOCK_BAT_LOW                        BAT_LOW
#define CHG_DOCK_BAT_TEMP45                     BAT_TEMP45
#define CHG_DOCK_BAT_CAP_FULL                   BAT_CAP_FULL
#define CHG_DOCK_EC_RESET                       EC_RESET
/* BEGIN [Terry_Tzeng][2012/06/11][Handle informal charger type] */
#define CHG_DOCK_INFORMAL_CHG                   INFORMAL_CHG
/* END [Terry_Tzeng][2012/06/11][Handle informal charger type] */
#if PEGA_SMT_SOLUTION
/* BEGIN [Terry_Tzeng][2012/03/13][Handle tablet and dock charging function by smt version] */
#define CHG_SMT_DOCK_CHG_TO_TABLET              CHG_DOCK_SMT_DK_CHG_TO_TB
#define CHG_SMT_AC_CHG_TO_DOCK                  CHG_DOCK_SMT_AC_CHG_TO_DK
/* END [Terry_Tzeng][2012/03/13][Handle tablet and dock charging function by smt version] */
#endif

#if PEGA_SMT_SOLUTION
#define ECHGER_CTRL_COUNT           15
/* BEGIN [Terry_Tzeng][2012/03/16][Switch 5v adpater charging path from pad to dock] */
#define SWITCH_AC_CHG_PATH_ON       1
#define SWITCH_AC_CHG_PATH_OFF      0
/* END [Terry_Tzeng][2012/03/16][Switch 5v adpater charging path from pad to dock] */
#else
#define ECHGER_CTRL_COUNT           14
#endif

#define CHG_DOCK_UNKNOW_EVT         2000

#define ECHGER_MAX_EVENTS           17

#define NO_DOCK_ST                 -1
#define DOCK_NO_READY_ST            0
#define DOCK_READY_ST               1

#define BATT_READY                  1
#define BATT_NO_READY               0


#define TB_RESTART_CHG_CAP          95

#define DEBUG_DUMP_LOG              1

/* BEGIN [Terry_Tzeng][2012/03/07][Define power switch delay time]  */
#define POWER_SW_DELAY_TIEM_PVT     300
#define POWER_SW_DELAY_TIEM_DVT     2000
/* END [Terry_Tzeng][2012/03/07][Define power switch delay time]  */
/* BEGIN [Terry_Tzeng][2012/05/04][Define watting time] */
#define WAITTING_TIME               1000
#define WAKEUP_TO_SUS_WAIT_TIME     100
/* END [Terry_Tzeng][2012/05/04][Define watting time] */
#define DOCK_OUT_DELAY_TIME         500

#define WAKEUP_EN                   1
#define WAKEUP_DIS                  0

/* BEGIN [Terry_Tzeng][2012/05/03][Set wakelock error handler] */
#define MAX_RETRY_WAKEUP_COUNT      10
/* END [Terry_Tzeng][2012/05/03][Set wakelock error handler] */

#define AC_NONE_SAFETY_TIME         420  // Safety time is 7 hours * 60 = 420 mins for inserted dock
#define AC_5V_SAFETY_TIME           840  // Safety time is 14 hours * 60 = 840 mins for inserted dock 5v adapter
#define AC_USB_SAFETY_TIME          1800  // Safety time is 30 hours * 60 = 1800 mins for inserted dock usb charger

static int wakelock_count = 0;
static struct echger_mux echg_mux_info;
int dock_ver = DOCK_PVT;
/* BEGIN [Terry_Tzeng][2012/03/16][Check entried bypass mode case] */
static int dk_usb_bypass_en;
#if PEGA_SMT_SOLUTION
/* BEGIN [Terry_Tzeng][2012/03/16][Switch 5v adpater charging path from pad to dock] */
static int chg_sw_ac_to_dk = SWITCH_AC_CHG_PATH_OFF;
/* END [Terry_Tzeng][2012/03/16][Switch 5v adpater charging path from pad to dock] */
#endif /* PEGA_SMT_SOLUTION */
/* END [Terry_Tzeng][2012/03/16][Check entried bypass mode case] */

extern void msm_otg_vbus_power(bool on);



/* Check dock status from ec */
static int echger_check_dk_status(void)
{
	int ret, status;

	ret = dock_status(&status);
	if (ret == FAIL)
	{
		return NO_DOCK_ST;
	}
	else
	{
		if (DOCK_I2C_FAIL & status)
		{
			return DOCK_NO_READY_ST;
		}
	}

	return DOCK_READY_ST;

}


/* Update and get last charging action status */
static enum echger_tb_dk_action_st echger_update_tb_dk_act_st(void)
{
	int ac_type;
	int reval;
	enum echger_tb_dk_action_st act_st;

	if (NO_DOCK_ST != echger_check_dk_status())
	{
		reval = get_ac_type(&ac_type);
		if (FAIL == reval)
		{
			PEGA_DBG_H("[CHG_EC_ERR] %s Get dock ac type is fail! \n", __func__);
			act_st = DOCK_ONLY;
		}

		if (AC_5V == ac_type)
		{
			act_st = DOCK_AC_5V_IN;
		}
		else if (AC_INFORMAL == ac_type)
		{
			act_st = DOCK_AC_INF_IN;
		}
		else if (AC_USB == ac_type)
		{
			act_st = DOCK_USB_5V_IN;
		}
		else if (AC_12V == ac_type)
		{
			act_st = DOCK_AC_12V_IN;
		}
		else
		{
			act_st = DOCK_ONLY;
		}
	}
	else
	{
		act_st = DOCK_OUT;
	}

	PEGA_DBG_M("[CHG_EC] %s Get DUT charging action status = %d \n", __func__, act_st);

	return act_st;

}

/* Control tablet power switch to trun on/off */
static int echger_ctrl_tb_pw_on_off(int t_pw_type, int action)
{
	int t_pw_st;
	int reval = SUCCESS;

	PEGA_DBG_M("[CHG_EC] %s \n", __func__);

	if ((PW_OFF != action) &&
	    (PW_ON != action))
	{
		PEGA_DBG_H("[CHG_EC_ERR] Handle tablet power switch action is fail! \n");
		return FAIL;
	}

	if (t_pw_type == T_5BST_PW)
	{
		if (action == PW_ON)
		{
			reval = msm_chg_5vboost_pw_enable();
			if (0 == reval)
			{
				reval = SUCCESS;
				msm_otg_vbus_power(1);
			}
			else
			{
				reval = FAIL;
			}
		}
		else
		{
			reval = msm_chg_5vboost_pw_disable();
			if (0 == reval)
			{
				reval = SUCCESS;
				msm_otg_vbus_power(0);
			}
			else
			{
				reval = FAIL;
			}
		}

		return reval;
	}

	if (t_pw_type != T_OTG_PW)
	{
		PEGA_DBG_H("[CHG_EC_ERR] Set control power switch type is error! \n");
		return FAIL;
	}

	/* Get tablet 5V boost power switch status */
	t_pw_st = gpio_get_value_cansleep(PM8921_GPIO_TO_SYS(PM8921_TB_OTG_PW_GPIO));
	PEGA_DBG_M("[CHG_EC] Get tablet power switch gpio status = %d \n", t_pw_st);

	if (action != t_pw_st)
	{
		struct pm_gpio tb_5vbst_pw_gpio_config = {
			.direction      = PM_GPIO_DIR_OUT,
			.pull           = PM_GPIO_PULL_NO,
			.out_strength   = PM_GPIO_STRENGTH_HIGH,
			.function       = PM_GPIO_FUNC_NORMAL,
			.inv_int_pol    = 0,
			.vin_sel        = 2,
			.output_buffer  = PM_GPIO_OUT_BUF_CMOS,
			.output_value   = 1,
		};

		tb_5vbst_pw_gpio_config.output_value = action;
		reval = pm8xxx_gpio_config(PM8921_GPIO_TO_SYS(PM8921_TB_OTG_PW_GPIO), &tb_5vbst_pw_gpio_config);
		if (reval)
		{
			pr_err("%s: pm8921 gpio %d config failed(%d)\n", __func__, PM8921_GPIO_TO_SYS(PM8921_TB_OTG_PW_GPIO), reval);
		}
	}

	return reval;
}


static int echger_ctrl_bst_action(int dk_bst_type, int action)
{
	int reval;
	int d_otg_bst, d_sys_bst;

	reval = get_boost_status(&d_otg_bst, &d_sys_bst);

	if (FAIL == reval)
	{
		PEGA_DBG_H("[CHG_EC_ERR] %s Handle get_boost_status return fail \n", __func__);

		reval = ec_boost_ctl(dk_bst_type, action);
		if (FAIL == reval)
		{
			PEGA_DBG_H("[CHG_EC_ERR] %s Call ec_boost_ctl return fail for boost type = %d \n", __func__, dk_bst_type);
			/* Implement fail case */
		}
		return reval;
	}

	if (SYS_5V == dk_bst_type)
	{
		if (d_sys_bst != action)
		{
			reval = ec_boost_ctl(SYS_5V, action);
			if (FAIL == reval)
			{
				PEGA_DBG_H("[CHG_EC_ERR] %s Call ec_boost_ctl return fail for boost type = %d \n", __func__, SYS_5V);
				/* Implement fail case */
			}
		}
	}
	else
	{
		if (d_otg_bst != action)
		{
			reval = ec_boost_ctl(OTG_5V, action);
			if (FAIL == reval)
			{
				PEGA_DBG_H("[CHG_EC_ERR] %s Call ec_boost_ctl return fail for boost type = %d \n", __func__, OTG_5V);
				/* Implement fail case */
			}
		}
	}

	return SUCCESS;
}


static int echger_supply_charging_direction_switch(enum echger_tb_dk_action_st action, enum echger_batt_cap_level dk_batt_level)
{
	int reval = SUCCESS;

	PEGA_DBG_M("[CHG_EC] %s \n", __func__);

	if (DOCK_OUT == action)
	{
		/* Turn on tablet otg disable power switch */
		reval = echger_ctrl_tb_pw_on_off(T_OTG_PW, PW_ON);
		if (SUCCESS == reval)
		{
			echg_mux_info.tb_otg_pw_st = PW_ON;
		}
		else
		{
			PEGA_DBG_H("[CHG_EC_ERR] %s, Turn ON tablet otg boost pw is fail! \n", __func__);
		}

		reval = echger_ctrl_bst_action(SYS_5V, BOOST_OFF);
		if (reval == SUCCESS)
		{
			echg_mux_info.dk_sys_bst_st = BOOST_OFF;
		}
		else
		{
			PEGA_DBG_H("[CHG_EC_ERR] %s, Turn OFF dock system boost is fail! \n", __func__);
		}

		return reval;
	}

	/* Turn off tablet otg disable power switch */
	reval = echger_ctrl_tb_pw_on_off(T_OTG_PW, PW_OFF);
	if (SUCCESS == reval)
		echg_mux_info.tb_otg_pw_st = PW_OFF;
	else
		PEGA_DBG_H("[CHG_EC_ERR] %s, Turn OFF tablet otg boost pw is fail! \n", __func__);

/* BEGIN [Terry_Tzeng][2012/04/26][Modify dock version for PVT and SIT2] */
	if ((DOCK_PVT <= dock_ver) && (DOCK_UNKNOWN > dock_ver))
/* END [Terry_Tzeng][2012/04/26][Modify dock version for PVT and SIT2] */
	{
		if ((DOCK_AC_12V_IN == action) ||
		   ((DOCK_ONLY == action) &&
		   ((BATT_LOW_LV == dk_batt_level) ||
		    (BATT_ERR == dk_batt_level))))
		{
			reval = echger_ctrl_bst_action(SYS_5V, BOOST_OFF);
			if (reval == SUCCESS)
			{
				echg_mux_info.dk_sys_bst_st = BOOST_OFF;
			}
			else
			{
				PEGA_DBG_H("[CHG_EC_ERR] %s, Turn OFF dock system boost is fail! \n", __func__);
			}
		}
		else
		{
			reval = echger_ctrl_bst_action(SYS_5V, BOOST_ON);
			if (reval == SUCCESS)
			{
				echg_mux_info.dk_sys_bst_st = BOOST_ON;
			}
			else
			{
				PEGA_DBG_H("[CHG_EC_ERR] %s, Turn ON dock system boost is fail! \n", __func__);
			}
		}
	}
	else
	{
		if ((DOCK_ONLY == action) &&
		    (BATT_LOW_LV != dk_batt_level) &&
		    (BATT_ERR != dk_batt_level))
		{
			reval = echger_ctrl_bst_action(SYS_5V, BOOST_ON);
			if (reval == SUCCESS)
			{
				echg_mux_info.dk_sys_bst_st = BOOST_ON;
			}
			else
			{
				PEGA_DBG_H("[CHG_EC_ERR] %s, Turn ON dock system boost is fail! \n", __func__);
			}
		}
		else
		{
			reval = echger_ctrl_bst_action(SYS_5V, BOOST_OFF);
			if (reval == SUCCESS)
			{
				echg_mux_info.dk_sys_bst_st = BOOST_OFF;
			}
			else
			{
				PEGA_DBG_H("[CHG_EC_ERR] %s, Turn OFF dock system boost is fail! \n", __func__);
			}
		}
	}

	PEGA_DBG_M("[CHG_EC] %s, TB otg chg disable pw st = %d [PW_ON : 0, PW_OFF : 1] \n", __func__, echg_mux_info.tb_otg_pw_st);
	PEGA_DBG_M("[CHG_EC] %s, DK sys bst st = %d [BOOST_ON : 1, BOOST_OFF : 0]\n", __func__, echg_mux_info.dk_sys_bst_st);

	return reval;
}


// Trun off all power switch and boost in initial statement
static int echger_init_setting_for_changed_charging_status(void)
{
	int reval;
	int g_dk_chg_st;
	int d_otg_bst, d_sys_bst;

	PEGA_DBG_M("[CHG_EC] %s \n", __func__);

	// Turn off tablet otg disable power switch
	reval = echger_ctrl_tb_pw_on_off(T_OTG_PW, PW_OFF);
	echg_mux_info.tb_otg_pw_st = PW_OFF;

	// Default turn on tablet 5V boost power switch to fix instance hub crash issue
	echger_ctrl_tb_pw_on_off(T_5BST_PW, PW_ON);
	echg_mux_info.tb_bst_pw_st = PW_ON;

	// Turn off dock otg and system boost
	if (DOCK_DVT == dock_ver)
		reval = echger_ctrl_bst_action(SYS_5V, BOOST_OFF);

	reval = get_boost_status(&d_otg_bst, &d_sys_bst);
	if (FAIL == reval)
	{
		PEGA_DBG_H("[CHG_EC_ERR] %s, Get dock boost status is fail and check get_boost_status interface \n" ,__func__);
		//echg_mux_info.dk_otg_bst_st = BOOST_OFF;
		echg_mux_info.dk_sys_bst_st = BOOST_OFF;
	}
	else
	{
		//echg_mux_info.dk_otg_bst_st = d_otg_bst;
		echg_mux_info.dk_sys_bst_st = d_sys_bst;
	}

	PEGA_DBG_M("[CHG_EC] In initial statement, turn off all SW and check dock otg boost st = %d, system boost st = %d \n", d_otg_bst, d_sys_bst);

	reval = get_ec_charging_req_status(&g_dk_chg_st);
	if (FAIL == reval)
	{
		PEGA_DBG_H("[CHG_EC_ERR] %s, Handle ge_ec_charging_req_status return fail \n" ,__func__);
		//Implement fail case
	}
	else if (CHARGING_ON == g_dk_chg_st)
	{
		reval = ec_charging_req(CHARGING_OFF);
		if (FAIL == reval)
		{
			PEGA_DBG_H("[CHG_EC_ERR] Handle ge_ec_charging_req_status return fail \n");
			//Implement fail case
		}
	}

	return SUCCESS;
}


static int echger_supply_usb_power_switch(enum echger_tb_dk_action_st action,
                                          enum echger_batt_cap_level dk_batt_level,
                                          enum echger_batt_cap_level tb_batt_level)
{
	int reval = SUCCESS;

	if (DOCK_OUT != action)
	{
		if ((BATT_LOW_LV != dk_batt_level) &&
		    (BATT_ERR != dk_batt_level))
		{
			reval = echger_ctrl_bst_action(OTG_5V, BOOST_ON);
			if (reval == SUCCESS)
			{
				echg_mux_info.dk_otg_bst_st = BOOST_ON;

				reval = echger_ctrl_tb_pw_on_off(T_5BST_PW, PW_OFF);
				if (SUCCESS == reval)
				{
					echg_mux_info.tb_bst_pw_st = PW_OFF;
				}
				else
				{
					PEGA_DBG_H("[CHG_EC_ERR] %s, Turn OFF tablet 5v boost pw is fail! \n", __func__);
				}
			}
			else
			{
				PEGA_DBG_H("[CHG_EC_ERR] %s, Turn ON dock otg boost is fail, So switch to turn on pad otg 5v enable power switch! ! \n", __func__);

				reval = echger_ctrl_tb_pw_on_off(T_5BST_PW, PW_ON);
				if (SUCCESS == reval)
				{
					echg_mux_info.tb_bst_pw_st = PW_ON;
				}
				else
				{
					PEGA_DBG_H("[CHG_EC_ERR] %s, Turn ON tablet 5v boost pw is fail! \n", __func__);
				}
			}
		}
		else
		{
			reval = echger_ctrl_tb_pw_on_off(T_5BST_PW, PW_ON);
			if (SUCCESS == reval)
			{
				echg_mux_info.tb_bst_pw_st = PW_ON;

				reval = echger_ctrl_bst_action(OTG_5V, BOOST_OFF);
				if (reval == SUCCESS)
				{
					echg_mux_info.dk_otg_bst_st = BOOST_OFF;
				}
				else
				{
					PEGA_DBG_H("[CHG_EC_ERR] %s, Turn OFF dock otg boost is fail! \n", __func__);
				}
			}
			else
			{
				PEGA_DBG_H("[CHG_EC_ERR] %s, Turn ON tablet 5v boost pw is fail, So switch to turn on dock otg boost! \n", __func__);

				reval = echger_ctrl_bst_action(OTG_5V, BOOST_ON);
				if (reval == SUCCESS)
				{
					echg_mux_info.dk_otg_bst_st = BOOST_ON;
				}
				else
				{
					PEGA_DBG_H("[CHG_EC_ERR] %s, Turn ON dock otg boost is fail! \n", __func__);
				}
			}
		}

		otg_power_switch_notifier();
	}
	else
	{
		reval = echger_ctrl_tb_pw_on_off(T_5BST_PW, PW_OFF);
		if (SUCCESS == reval)
		{
			echg_mux_info.tb_bst_pw_st = PW_OFF;
		}
		else
		{
			PEGA_DBG_H("[CHG_EC_ERR] %s, Turn OFF tablet 5v boost pw is fail! \n", __func__);
		}

		reval = echger_ctrl_bst_action(OTG_5V, BOOST_OFF);
		if (reval == SUCCESS)
		{
			echg_mux_info.dk_otg_bst_st = BOOST_OFF;
		}
		else
		{
			PEGA_DBG_H("[CHG_EC_ERR] %s, Turn OFF dock otg boost is fail! \n", __func__);
		}
	}

	PEGA_DBG_H("[CHG_EC] %s, TB 5v bst st = %d [PW_ON : 1, PW_OFF : 0] \n", __func__, echg_mux_info.tb_bst_pw_st);
	PEGA_DBG_H("[CHG_EC] %s, DK otg bst st = %d [BOOST_ON : 1, BOOST_OFF : 0]\n", __func__, echg_mux_info.dk_otg_bst_st);

	return reval;
}


static enum echger_batt_cap_level echger_get_batt_level(int get_type)
{
	int gbatt_cap;
	int chk_batt_full_flg;


	PEGA_DBG_M("[CHG_EC] %s Get battery type = %d \n", __func__, get_type);

	if (TABLET_BATT_TYPE == get_type)
	{
		if (BATT_NO_READY == msm_chg_check_padbatt_ready())
		{
			PEGA_DBG_H("[CHG_EC_ERR] %s Check tablet battery gauge is error \n", __func__);
			return BATT_ERR;
		}

		/* Get tablet UI batter capacity */
		//gbatt_cap = msm_chg_get_batt_remain_capacity();
		gbatt_cap = msm_chg_get_ui_batt_capacity();

		PEGA_DBG_H("[CHG_EC] Get tablet UI battery capacity = %d\n", gbatt_cap);

		/* BEGIN [Terry_Tzeng][2012/03/12][Chang defined tablet battery full method] */
		chk_batt_full_flg = msm_chg_get_batt_status();

		if (POWER_SUPPLY_STATUS_FULL == chk_batt_full_flg)
		{
			return BATT_FULL_LV;
		}
		else if ((gbatt_cap > BATT_LOW_CAP_PERCENT) &&
		         (POWER_SUPPLY_STATUS_FULL != chk_batt_full_flg))
		{
			return BATT_NORMAL_LV;
		}
		else if (gbatt_cap <= BATT_LOW_CAP_PERCENT)
		{
			return BATT_LOW_LV;
		}
		/* END [Terry_Tzeng][2012/03/12][Chang defined tablet battery full method] */
	}
	else if (DOCK_BATT_TYPE == get_type)
	{
		if (BATT_NO_READY == msm_chg_check_ecbatt_ready())
		{
			PEGA_DBG_H("[CHG_EC_ERR] %s Check dock battery gauge is error \n", __func__);
			return BATT_ERR;
		}

		/* Get dock batter capacity */
		gbatt_cap = msm_chg_get_ui_ecbatt_capacity();
		PEGA_DBG_H("[CHG_EC] Get dock UI battery capacity = %d\n", gbatt_cap);

		/* BEGIN [Terry_Tzeng][2012/03/12][Chang defined dock battery full method] */
		chk_batt_full_flg = msm_chg_check_ecbatt_full();

		if (POWER_SUPPLY_STATUS_FULL == chk_batt_full_flg)
		{
			return BATT_FULL_LV;
		}
		else if ((gbatt_cap > BATT_LOW_CAP_PERCENT) &&
		         (POWER_SUPPLY_STATUS_FULL != chk_batt_full_flg))
		{
			return BATT_NORMAL_LV;
		}
		else if (gbatt_cap <= BATT_LOW_CAP_PERCENT)
		{
			return BATT_LOW_LV;
		}
		/* END [Terry_Tzeng][2012/03/12][Chang defined dock battery full method] */
	}
	else
	{
		PEGA_DBG_H("[CHG_EC_ERR] %s No set this type(%d) to get battery information \n", __func__, get_type);
		return BATT_ERR;
	}

	PEGA_DBG_H("[CHG_EC_ERR] %s Get battery capacity information is error \n", __func__);

	return BATT_ERR;
}

static void echger_dk_batt_err_handler(enum echger_tb_dk_action_st chg_act_st, enum echger_batt_cap_level tb_batt_lv)
{
	int reval;
	int tb_5vbst_pw;

	// Control tablet otg boost power switcvh
	reval = echger_ctrl_tb_pw_on_off(T_OTG_PW, PW_OFF);
	if (SUCCESS == reval)
		echg_mux_info.tb_otg_pw_st = PW_OFF;

	// Control tablet system boost power switcvh
	if ((BATT_LOW_LV != tb_batt_lv) ||
	    (BATT_ERR != tb_batt_lv))
	{
		tb_5vbst_pw = PW_ON;
	}
	else
	{
		tb_5vbst_pw = PW_OFF;
	}
	reval = echger_ctrl_tb_pw_on_off(T_5BST_PW, tb_5vbst_pw);
	if (SUCCESS == reval)
		echg_mux_info.tb_bst_pw_st = tb_5vbst_pw;

	// Turn off dock system and otg boost
	if ((chg_act_st == DOCK_ONLY) ||
	    (chg_act_st == DOCK_OUT))
	{
		reval = echger_ctrl_bst_action(SYS_5V, BOOST_OFF);
		if (reval == SUCCESS)
			echg_mux_info.dk_sys_bst_st = BOOST_OFF;
	}
	else
	{
		reval = echger_ctrl_bst_action(SYS_5V, BOOST_ON);
		if (reval == SUCCESS)
			echg_mux_info.dk_sys_bst_st = BOOST_ON;
	}

	reval = echger_ctrl_bst_action(OTG_5V, BOOST_OFF);
	if (reval == SUCCESS)
		echg_mux_info.dk_otg_bst_st = BOOST_OFF;
}



#if DEBUG_DUMP_LOG
static void echger_dump_debug_msg(void)
{
	enum echger_tb_dk_action_st chg_act_st;
	enum echger_batt_cap_level tb_batt_lv;
	enum echger_batt_cap_level dk_batt_lv;
	int d_otg_bst, d_sys_bst;
	int temp;
	int gdk_chg_rq;
	int reval;
	int chk_tb_5vbst_st;

	PEGA_DBG_H("[CHG_EC] ==================  Dump tablet and dock charging controlled debug message ======================= \n\n");

	// Update charging action status
	chg_act_st = echger_update_tb_dk_act_st();
	PEGA_DBG_H("[CHG_EC] Get last tablet and dock status = %d \n", chg_act_st);
	PEGA_DBG_H("[CHG_EC] Record tablet and dock status = %d \n", echg_mux_info.curr_tb_dk_act);

	reval = ec_dock_ver(&dock_ver);
/* BEGIN [Terry_Tzeng][2012/04/26][Modify dock version for PVT and SIT2] */
	if ((DOCK_PVT <= dock_ver) && (DOCK_UNKNOWN > dock_ver))
	{
		PEGA_DBG_H("[CHG_EC] Get dock version is PVT\n");
	}
	else if (DOCK_DVT == dock_ver)
	{
		PEGA_DBG_H("[CHG_EC] Get dock version is DVT\n");
	}
	else
	{
		PEGA_DBG_H("[CHG_EC_ERR] Get dock version is fail!\n");
	}

	PEGA_DBG_H("[CHG_EC] [Tablet data] \n");

	// Get dock battery temperature
	temp = msm_chg_get_batt_temperture();
	PEGA_DBG_M("[CHG_EC] Get tablet battery temperature = %d \n", temp);

	// Get dock battery capacity
	PEGA_DBG_M("[CHG_EC] Get tablet battery mVoltage = %d \n", msm_chg_get_batt_mvolts());

	// Get dock battery capacity
	PEGA_DBG_M("[CHG_EC] Get tablet UI battery capacity = %d \n", msm_chg_get_ui_batt_capacity());

	// Get tablet battery capacity level
	tb_batt_lv = echger_get_batt_level(TABLET_BATT_TYPE);
	PEGA_DBG_H("[CHG_EC] Get tablet battery capacity level = %d \n", tb_batt_lv);

	if (gpio_get_value_cansleep(PM8921_GPIO_TO_SYS(PM8921_TB_OTG_PW_GPIO)))
	{
		PEGA_DBG_H("[CHG_EC] Tablet otg chg disable PW is OFF \n");
	}
	else
	{
		PEGA_DBG_H("[CHG_EC] Tablet otg chg disable PW is ON \n");
	}

	chk_tb_5vbst_st = msm_chg_check_5vboost_pw_is_enabled();
	if (chk_tb_5vbst_st == 1)
	{
		PEGA_DBG_H("[CHG_EC] Tablet 5V boost PW is ON \n");
	}
	else if (chk_tb_5vbst_st == 0)
	{
		PEGA_DBG_H("[CHG_EC] Tablet 5V boost PW is OFF \n");
	}
	else
	{
		PEGA_DBG_H("[CHG_EC] Get tablet 5V boost PW is fail \n");
	}

	PEGA_DBG_M("[CHG_EC] Check wake up count = %d and wake lock status = %d, WAKE LOCK : %d, WAKE UNLOCK : %d \n", wakelock_count, echg_mux_info.wakelock_flag, WAKEUP_EN, WAKEUP_DIS);

	PEGA_DBG_H("[CHG_EC] ------------------------------------------------------------------------ \n");
	PEGA_DBG_H("[CHG_EC] [Dock data] \n");
	// Check dock ready status
	if (DOCK_READY_ST == echger_check_dk_status())
	{
		PEGA_DBG_H("[CHG_EC] Dock is ready\n");
	}
	else
	{
		PEGA_DBG_H("[CHG_EC] Dock is not ready\n");
	}

	// Get dock battery temperature
	temp = msm_chg_get_ecbatt_temperature();
	PEGA_DBG_M("[CHG_EC] Get dock battery temperature = %d \n", temp);

	// Get dock battery capacity
	PEGA_DBG_M("[CHG_EC] Get dock battery mVoltage = %d \n", msm_chg_get_ecbatt_mvolt());

	// Get dock battery capacity
	PEGA_DBG_M("[CHG_EC] Get dock UI battery capacity = %d \n", msm_chg_get_ui_ecbatt_capacity());

	// Get dock battery capacity level
	dk_batt_lv = echger_get_batt_level(DOCK_BATT_TYPE);
	PEGA_DBG_H("[CHG_EC] Get dock battery capacity level = %d \n", dk_batt_lv);

	// Get dock system and otg boost status
	reval = get_boost_status(&d_otg_bst, &d_sys_bst);

	if (FAIL == reval)
	{
		PEGA_DBG_H("[CHG_EC_ERR] Get dock system and otg boost status return fail \n");
	}
	else
	{
		if (BOOST_ON == d_otg_bst)
		{
			PEGA_DBG_H("[CHG_EC] Dock otg boost is ON \n");
		}
		else
		{
			PEGA_DBG_H("[CHG_EC] Dock otg boost is OFF \n");
		}

		if (BOOST_ON == d_sys_bst)
		{

			PEGA_DBG_H("[CHG_EC] Dock system boost PW is ON \n");
		}
		else
		{
			PEGA_DBG_H("[CHG_EC] Dock system boost PW is OFF \n");
		}
	}

	reval = get_ec_charging_req_status(&gdk_chg_rq);
	if (FAIL == reval)
	{
		PEGA_DBG_H("[CHG_EC_ERR] Get dock request status return fail \n");
	}
	else
	{
		if (CHARGING_ON == gdk_chg_rq)
		{
			PEGA_DBG_H("[CHG_EC] Dock is charging ON \n");
		}
		else
		{
			PEGA_DBG_H("[CHG_EC] Dock is charging OFF \n");
		}
	}
	PEGA_DBG_H("\n\n[CHG_EC] =========================================  Dump end ============================================== \n");
}
#endif /* DEBUG_DUMP_LOG */

static void echger_check_dock_version(void)
{
	int ret = 0;

	ret = ec_dock_ver(&dock_ver);
	if (FAIL == ret)
	{
		PEGA_DBG_H("[CHG_EC_ERR] %s Get dock version is fail to switch default is PVT\n", __func__);
		dock_ver = DOCK_PVT;
	}
	else
	{
		if (dock_ver == DOCK_UNKNOWN)
			dock_ver = DOCK_SIT2;
	}
}

/* BEGIN [Terry_Tzeng][2012/03/16][Check entried bypass mode case] */
static int echger_check_bypass_mode(void)
{
	int reval;
	int usb_bypass_status;

	reval = get_usb_bypass_en(&usb_bypass_status);

	if (FAIL == reval)
	{
		PEGA_DBG_H("[CHG_EC_ERR] Use get_usb_bypass_en to check bypass mode is fail \n");
		return reval;
	}

	if (usb_bypass_high == usb_bypass_status)
	{
		dk_usb_bypass_en = usb_bypass_high;
		return usb_bypass_high;
	}

	dk_usb_bypass_en = usb_bypass_low;

	return usb_bypass_low;
}

static void echger_handle_bypass_mode(void)
{
	int reval;

	// Control tablet otg boost power switcvh
	reval = echger_ctrl_tb_pw_on_off(T_OTG_PW, PW_OFF);
	if (SUCCESS == reval)
		echg_mux_info.tb_otg_pw_st = PW_OFF;

	reval = echger_ctrl_tb_pw_on_off(T_5BST_PW, PW_OFF);
	if (SUCCESS == reval)
		echg_mux_info.tb_bst_pw_st = PW_OFF;

	// Turn off dock system and otg boost
	reval = echger_ctrl_bst_action(SYS_5V, BOOST_OFF);
	if (reval == SUCCESS)
		echg_mux_info.dk_sys_bst_st = BOOST_OFF;

	reval = echger_ctrl_bst_action(OTG_5V, BOOST_OFF);
	if (reval == SUCCESS)
		echg_mux_info.dk_otg_bst_st = BOOST_OFF;

	msleep(200);

	reval = echger_ctrl_bst_action(OTG_5V, BOOST_ON);
	if (reval == SUCCESS)
		echg_mux_info.dk_otg_bst_st = BOOST_ON;

	msleep(100);

	reval = echger_ctrl_tb_pw_on_off(T_OTG_PW, PW_ON);
	if (SUCCESS == reval)
		echg_mux_info.tb_otg_pw_st = PW_ON;

#if DEBUG_DUMP_LOG
		echger_dump_debug_msg();
#endif
}


/* END [Terry_Tzeng][2012/03/16][Check entried bypass mode case] */

/* BEGIN [Terry_Tzeng][2012/05/03][Add function of setting suspend status flag] */
void echger_enable_suspend_flag(void)
{
	echg_mux_info.suspend_status = SUSPEND_STATUS_EN;
}

void echger_disable_suspend_flag(void)
{
	echg_mux_info.suspend_status = SUSPEND_STATUS_DIS;
}

void echger_suspend_charging_control(void)
{
	/* Check tablet and dock status */
	int tb_dk_st;
	enum echger_batt_cap_level tb_batt_lv;
	enum echger_batt_cap_level dk_batt_lv;
	int reval;

	tb_dk_st = echger_update_tb_dk_act_st();
	if (DOCK_ONLY != tb_dk_st)
	{
		PEGA_DBG_H("[CHG_EC_ERR] %s Enter suspend charging status is error, charger status = %d\n", __func__, tb_dk_st);
		return;
	}

	tb_batt_lv = echger_get_batt_level(TABLET_BATT_TYPE);

	dk_batt_lv = echger_get_batt_level(DOCK_BATT_TYPE);

	/* Turn off tablet otg disable power switch */
	reval = echger_ctrl_tb_pw_on_off(T_OTG_PW, PW_OFF);
	if (SUCCESS == reval)
	{
		echg_mux_info.tb_otg_pw_st = PW_OFF;
	}
	else
	{
		PEGA_DBG_H("[CHG_EC_ERR] %s, Turn OFF tablet otg boost pw is fail! \n", __func__);
	}

	if (SUSPEND_STATUS_EN == echg_mux_info.suspend_status)
	{
		if ((BATT_FULL_LV == tb_batt_lv) ||
		    (BATT_LOW_LV == dk_batt_lv) ||
		    (BATT_ERR == dk_batt_lv))
		{
			reval = echger_ctrl_bst_action(SYS_5V, BOOST_OFF);
			if (reval == SUCCESS)
			{
				echg_mux_info.dk_sys_bst_st = BOOST_OFF;
			}
			else
			{
				PEGA_DBG_H("[CHG_EC_ERR] %s, Turn OFF dock system boost is fail! \n", __func__);
			}
		}
		else
		{
			reval = echger_ctrl_bst_action(SYS_5V, BOOST_ON);
			if (reval == SUCCESS)
			{
				echg_mux_info.dk_sys_bst_st = BOOST_ON;
			}
			else
			{
				PEGA_DBG_H("[CHG_EC_ERR] %s, Turn OFF dock system boost is fail! \n", __func__);
			}
		}
	}
	else if (SUSPEND_STATUS_DIS == echg_mux_info.suspend_status)
	{
		if ((BATT_LOW_LV == dk_batt_lv) ||
		    (BATT_ERR == dk_batt_lv))
		{
			reval = echger_ctrl_bst_action(SYS_5V, BOOST_OFF);
			if (reval == SUCCESS)
			{
				echg_mux_info.dk_sys_bst_st = BOOST_OFF;
			}
			else
			{
				PEGA_DBG_H("[CHG_EC_ERR] %s, Turn OFF dock system boost is fail! \n", __func__);
			}
		}
		else
		{
			reval = echger_ctrl_bst_action(SYS_5V, BOOST_ON);
			if (reval == SUCCESS)
			{
				echg_mux_info.dk_sys_bst_st = BOOST_ON;
			}
			else
			{
				PEGA_DBG_H("[CHG_EC_ERR] %s, Turn OFF dock system boost is fail! \n", __func__);
			}
		}
	}
}
/* BEGIN [Terry_Tzeng][2012/05/04][Define getting retry count for setting ec wake lock function] */
int  echger_get_retry_flag(int chk_reval)
{
	int ret = 1;

	if (FAIL == chk_reval)
	{
		if (echg_mux_info.wakeup_t_retry_flg < MAX_RETRY_WAKEUP_COUNT)
		{
			echg_mux_info.wakeup_t_retry_flg++;
			ret = 1;
		}
		else
		{
			echg_mux_info.wakeup_t_retry_flg = 0;
			ret = 0;
		}
	}
	else
	{
		ret = 0;
	}

	return ret;
}
/* END [Terry_Tzeng][2012/05/04][Define getting retry count for setting ec wake lock function] */


static void echger_wake_lock(void)
{
	if (echg_mux_info.wakelock_flag != WAKEUP_EN)
	{
		struct msm_hardware_charger_priv *priv = NULL;

		if (NULL != echg_mux_info.hw_chg)
		{
			priv = echg_mux_info.hw_chg->charger_private;
		}
		else
		{
			PEGA_DBG_H("[CHG_EC_ERR] %s, echg_mux_info.hw_chg is NULL.\n", __func__);
			return;
		}

		echg_mux_info.wakelock_flag = WAKEUP_EN;
		wake_lock(&priv->wl);
	}

	wakelock_count++;
}


void echger_wake_lock_ext(void)
{
	echger_wake_lock();
}


static void echger_wake_unlock(void)
{
	if (echg_mux_info.wakelock_flag != WAKEUP_DIS)
	{
		struct msm_hardware_charger_priv *priv = NULL;

		if (NULL != echg_mux_info.hw_chg)
		{
			priv = echg_mux_info.hw_chg->charger_private;
		}
		else
		{
			PEGA_DBG_H("[CHG_EC_ERR] %s, echg_mux_info.hw_chg is NULL.\n", __func__);
			return;
		}

		echg_mux_info.wakelock_flag = WAKEUP_DIS;
		wake_unlock(&priv->wl);
	}

	if (wakelock_count > 0)
		wakelock_count--;

}

void echger_wake_unlock_ext(void)
{
	int chk_chger_type = echger_update_tb_dk_act_st();

	if ((DOCK_ONLY == chk_chger_type) ||
	   ((DOCK_OUT == chk_chger_type) &&
	    (NO_DOCK_ST == echger_check_dk_status())))
	{
		msleep(WAITTING_TIME);
		echger_wake_unlock();
	}
}

/* BEGIN [Terry_Tzeng][2011/01/09][Add register callback fucntion for handled dock charging status from ec] */
static void echger_handle_event(int evt)
{
	mutex_lock(&echg_mux_info.status_lock);

	PEGA_DBG_M("[CHG_EC] %s Receive event = %d\n", __func__, evt);

	switch (evt)
	{
		case CHG_DOCK_EC_RESET:
		{
			enum echger_tb_dk_action_st chg_act_st;

			if (usb_bypass_high == echger_check_bypass_mode())
			{
				PEGA_DBG_H("[CHG_EC] Enter dock usb bypass mode \n");
				break;
			}

			PEGA_DBG_H("[CHG_EC] Receive CHG_DOCK_EC_RESET \n");

			// Update charging action status
			chg_act_st = echger_update_tb_dk_act_st();
			if (DOCK_OUT == chg_act_st)
				break;

			echg_mux_info.curr_tb_dk_act = DOCK_RESET;

			break;
		}

/* BEGIN [Terry_Tzeng][2012/05/03][Handle suspend charging to late resume status for pad insert dock] */
		case CHG_DOCK_SYS_SUSPEND_TO_LATE_RESUME:
		{
			if (DOCK_READY_ST != echger_check_dk_status())
			{
				PEGA_DBG_H("[CHG_EC_ERR] Dock is not ready! \n");
				break;
			}

			if (usb_bypass_high == echger_check_bypass_mode())
			{
				PEGA_DBG_H("[CHG_EC] Enter dock usb bypass mode \n");
				break;
			}

			PEGA_DBG_H("[CHG_EC] Receive CHG_DOCK_SYS_SUSPEND_TO_LATE_RESUME \n");
			echger_disable_suspend_flag();
			echger_suspend_charging_control();
			msm_charger_notify_event(echg_mux_info.hw_chg, CHG_DOCK_SET_PERIOD_CHG_CURR_EVENT);

			break;
		}
/* END [Terry_Tzeng][2012/05/03][Handle suspend charging to late resume status for pad insert dock] */

/* BEGIN [Terry_Tzeng][2012/03/21][Handle heartbeat checking to monitor battery information in suspend mode] */
		case CHG_DOCK_SUSPEND_HEARTBEAT_CHK:
		{
			enum echger_tb_dk_action_st chg_act_st;
			enum echger_batt_cap_level tb_batt_lv;
			enum echger_batt_cap_level dk_batt_lv;
			int before_dk_sys_bst_st;

			if (DOCK_READY_ST != echger_check_dk_status())
			{
				PEGA_DBG_H("[CHG_EC_ERR] Dock is not ready! \n");
				break;
			}

			if (usb_bypass_high == echger_check_bypass_mode())
			{
				PEGA_DBG_H("[CHG_EC] Enter dock usb bypass mode \n");
				break;
			}

			PEGA_DBG_H("[CHG_EC] Receive CHG_DOCK_SUSPEND_HEARTBEAT_CHK \n");

			// Update charging action status
			chg_act_st = echger_update_tb_dk_act_st();

			/* BEGIN [Terry_Tzeng][2012/05/03][Set wakelock error handler] */
			echg_mux_info.wakeup_t_retry_flg = 0;
			/* END [Terry_Tzeng][2012/05/03][Set wakelock error handler] */

			if (DOCK_ONLY != chg_act_st)
			{
				wakeup_flag(WAKEUP_DISABLE);
				break;
			}

			echger_wake_lock();

/* BEGIN [Terry_Tzeng][2012/04/11][Set peroidically charging current to isl9519] */
			msm_charger_notify_event(echg_mux_info.hw_chg, CHG_DOCK_SET_PERIOD_CHG_CURR_EVENT);
/* END [Terry_Tzeng][2012/04/11][Set peroidically charging current to isl9519] */

			// Get tablet battery capacity level
			tb_batt_lv = echger_get_batt_level(TABLET_BATT_TYPE);
			// Get dock battery capacity level
			dk_batt_lv = echger_get_batt_level(DOCK_BATT_TYPE);
			// Check tablet battery charging to be full for charging controled logic
			if ((tb_batt_lv != echg_mux_info.curr_tb_batt_lv) || (dk_batt_lv != echg_mux_info.curr_dk_batt_lv))
			{
				echger_supply_usb_power_switch(chg_act_st, dk_batt_lv, tb_batt_lv);

				before_dk_sys_bst_st = echg_mux_info.dk_sys_bst_st;

				echger_suspend_charging_control();

				echg_mux_info.curr_tb_batt_lv = tb_batt_lv;

				echg_mux_info.curr_dk_batt_lv = dk_batt_lv;

				PEGA_DBG_H("[CHG_EC] Receive CHG_DOCK_SUSPEND_HEARTBEAT_CHK to reset intput current/ charging current \n");
				msm_charger_notify_event(echg_mux_info.hw_chg, CHG_DOCK_RESTART_CHG_EVENT);
#if DEBUG_DUMP_LOG
				echger_dump_debug_msg();
#endif
			}
			else if (chg_act_st == DOCK_ONLY)
			{
				msleep(WAKEUP_TO_SUS_WAIT_TIME);
				echger_wake_unlock();
			}

			break;
		}
/* END [Terry_Tzeng][2012/03/21][Handle heartbeat checking to monitor battery information in suspend mode] */

		case CHG_DOCK_HEARTBEAT_CHK:
		{
			enum echger_tb_dk_action_st chg_act_st;
			enum echger_batt_cap_level tb_batt_lv;
			enum echger_batt_cap_level dk_batt_lv;
			int before_dk_sys_bst_st;
			int gtb_batt_cap;

			PEGA_DBG_H("[CHG_EC] Receive CHG_DOCK_HEARTBEAT_CHK \n");

			if (DOCK_READY_ST != echger_check_dk_status())
			{
				PEGA_DBG_H("[CHG_EC_ERR] Dock is not ready! \n");
				break;
			}

/* BEGIN [Terry_Tzeng][2012/03/16][Check entried bypass mode case] */
			if (usb_bypass_high == echger_check_bypass_mode())
			{
				PEGA_DBG_L("[CHG_EC] Enter dock usb bypass mode \n");
				break;
			}
/* END [Terry_Tzeng][2012/03/16][Check entried bypass mode case] */

			// Update charging action status
			chg_act_st = echger_update_tb_dk_act_st();
			PEGA_DBG_M("[CHG_EC] Update charging action status = %d \n", chg_act_st);
			// Don't handle tablet battery status and dock battery status in different status
			if ((chg_act_st != echg_mux_info.curr_tb_dk_act) ||
			    (NO_DOCK_ST == echger_check_dk_status()))
				break;

			// Get tablet battery capacity level
			tb_batt_lv = echger_get_batt_level(TABLET_BATT_TYPE);
			// Get dock battery capacity level
			dk_batt_lv = echger_get_batt_level(DOCK_BATT_TYPE);

			// Check tablet battery charging to be full for charging controled logic
			if ((DOCK_AC_INF_IN == chg_act_st) ||
			    (DOCK_AC_5V_IN == chg_act_st) ||
			    (DOCK_AC_12V_IN == chg_act_st) ||
			    (DOCK_USB_5V_IN == chg_act_st))
			{
				gtb_batt_cap = msm_chg_get_ui_batt_capacity();
				if (((TB_BATT_STOP_CHG == echg_mux_info.tb_batt_full_stop) &&
				     (gtb_batt_cap <= TB_RESTART_CHG_CAP)) ||
				     (BATT_FULL_LV == dk_batt_lv))
				{
					echg_mux_info.tb_batt_full_stop = TB_BATT_START_CHG;
				}
				else if ((BATT_FULL_LV == tb_batt_lv) && (BATT_FULL_LV != dk_batt_lv))
				{
					echg_mux_info.tb_batt_full_stop = TB_BATT_STOP_CHG;
				}
			}

			if ((tb_batt_lv != echg_mux_info.curr_tb_batt_lv) || (dk_batt_lv != echg_mux_info.curr_dk_batt_lv))
			{
				echger_supply_usb_power_switch(chg_act_st, dk_batt_lv, tb_batt_lv);

				before_dk_sys_bst_st = echg_mux_info.dk_sys_bst_st;

				echger_supply_charging_direction_switch(chg_act_st, dk_batt_lv);

				echg_mux_info.curr_tb_batt_lv = tb_batt_lv;
				echg_mux_info.curr_dk_batt_lv = dk_batt_lv;

				PEGA_DBG_M("[CHG_EC] Dock system boost change status in dock insert status \n");

				/* BEGIN [Terry_Tzeng][2012/04/24][Drop setting charging current for dock battery error] */
				if ((DOCK_USB_5V_IN == chg_act_st) || (DOCK_AC_INF_IN == chg_act_st))
				{
					int dk_ui_batt_cap = msm_chg_get_ui_ecbatt_capacity();

					if ((DROP_USB_CHG_CURR_CAP_PERCENT >= dk_ui_batt_cap) ||
					    (BATT_ERR == dk_batt_lv) ||
					   ((echg_mux_info.drop_curr_flg == DROP_CURR_EN) &&
					    (RECOVERY_CHG_CURR_CAP_PERCENT > dk_ui_batt_cap)))
					{
						echg_mux_info.drop_curr_flg = DROP_CURR_EN;
					}
					else
					{
						echg_mux_info.drop_curr_flg = DROP_CURR_DIS;
					}
				}
				/* END [Terry_Tzeng][2012/04/24][Drop setting charging current for dock battery error] */

				msm_charger_notify_event(echg_mux_info.hw_chg, CHG_DOCK_RESTART_CHG_EVENT);
#if DEBUG_DUMP_LOG
				echger_dump_debug_msg();
#endif
			}
			/* BEGIN [Terry_Tzeng][2012/04/24][Drop setting charging current for dock battery error] */
			else if ((DOCK_USB_5V_IN == chg_act_st) || (DOCK_AC_INF_IN == chg_act_st))
			{
				int tmp_flg;
				int dk_ui_batt_cap = msm_chg_get_ui_ecbatt_capacity();

				if ((DROP_USB_CHG_CURR_CAP_PERCENT >= dk_ui_batt_cap) ||
				    (BATT_ERR == dk_batt_lv) ||
				   ((echg_mux_info.drop_curr_flg == DROP_CURR_EN) &&
				    (RECOVERY_CHG_CURR_CAP_PERCENT > dk_ui_batt_cap)))
				{
					tmp_flg = DROP_CURR_EN;
				}
				else
				{
					tmp_flg = DROP_CURR_DIS;
				}

				if (tmp_flg != echg_mux_info.drop_curr_flg)
				{
					echg_mux_info.drop_curr_flg = tmp_flg;
					msm_charger_notify_event(echg_mux_info.hw_chg, CHG_DOCK_RESTART_CHG_EVENT);
				}
			}
			/* END [Terry_Tzeng][2012/04/24][Drop setting charging current for dock battery error] */

			break;
		}
		/* BEGIN [Terry_Tzeng][2012/05/03][Support power to dock usb output device to avoid dock battery remove] */

		case CHG_DOCK_IN:
		{
			int reval;

			PEGA_DBG_H("[CHG_EC] Receive CHG_DOCK_IN \n");

			if (usb_bypass_high == echger_check_bypass_mode())
			{
				PEGA_DBG_L("[CHG_EC] Enter dock usb bypass mode \n");
				break;
			}

			if (NO_DOCK_ST == echger_check_dk_status())
			{
				PEGA_DBG_H("[CHG_EC_ERR] Dock is no exist! \n");
				break;
			}

			if (DOCK_OUT != echg_mux_info.curr_tb_dk_act)
				break;

			// Get tablet otg boost power switch status
			reval = echger_ctrl_tb_pw_on_off(T_OTG_PW, PW_OFF);
			if (SUCCESS == reval)
				echg_mux_info.tb_otg_pw_st = PW_OFF;

			reval = echger_ctrl_tb_pw_on_off(T_5BST_PW, PW_ON);
			if (SUCCESS == reval)
				echg_mux_info.tb_bst_pw_st = PW_ON;

			break;
		}
		/* END [Terry_Tzeng][2012/05/03][Support power to dock usb output device to avoid dock battery remove] */

		case CHG_DOCK_AC_OUT:
		{
			enum echger_tb_dk_action_st chg_act_st;
			enum echger_batt_cap_level tb_batt_lv;
			enum echger_batt_cap_level dk_batt_lv;

			PEGA_DBG_H("[CHG_EC] Receive CHG_DOCK_AC_OUT \n");

			if (DOCK_READY_ST != echger_check_dk_status())
			{
				PEGA_DBG_H("[CHG_EC_ERR] Dock is not ready! \n");
				break;
			}

/* BEGIN [Terry_Tzeng][2012/03/16][Check entried bypass mode case] */
			if (usb_bypass_high == echger_check_bypass_mode())
			{
				PEGA_DBG_H("[CHG_EC] Enter dock usb bypass mode \n");
				echger_handle_bypass_mode();
				break;
			}

#if PEGA_SMT_SOLUTION
			chg_sw_ac_to_dk = SWITCH_AC_CHG_PATH_OFF;
#endif/* PEGA_SMT_SOLUTION */
/* END [Terry_Tzeng][2012/03/16][Check entried bypass mode case] */
			// Check dock version
			echger_check_dock_version();

			// Set delay time to turn off power switch
/* BEGIN [Terry_Tzeng][2012/04/26][Modify dock version for PVT and SIT2] */
			if ((DOCK_PVT <= dock_ver) && (DOCK_UNKNOWN > dock_ver))
/* END [Terry_Tzeng][2012/04/26][Modify dock version for PVT and SIT2] */
				msleep(POWER_SW_DELAY_TIEM_PVT);
			else
				msleep(POWER_SW_DELAY_TIEM_DVT);

			// Update charging action status
			chg_act_st = echger_update_tb_dk_act_st();

			if (DOCK_ONLY != chg_act_st)
			{
				PEGA_DBG_L("[CHG_EC] Don't handle this event %d, it isn't CHG_DOCK_IN or CHG_DOCK_AC_OUT ! \n", evt);
				break;
			}

			/* Lock process to handle heartbeat checking status */
			echger_wake_lock();

			echger_disable_suspend_flag();

			/* BEGIN [Terry_Tzeng][2012/05/03][Set wakelock error handler] */
			echg_mux_info.wakeup_t_retry_flg = 0;
			/* END [Terry_Tzeng][2012/05/03][Set wakelock error handler] */

			echger_init_setting_for_changed_charging_status();

			echg_mux_info.curr_tb_dk_act = chg_act_st;

			echg_mux_info.tb_batt_full_stop = TB_BATT_START_CHG;

			// Get tablet battery capacity level
			tb_batt_lv = echger_get_batt_level(TABLET_BATT_TYPE);
			echg_mux_info.curr_tb_batt_lv = tb_batt_lv;

			// Get dock battery capacity level
			dk_batt_lv = echger_get_batt_level(DOCK_BATT_TYPE);
			echg_mux_info.curr_dk_batt_lv = dk_batt_lv;

			// Determine to supply power direction from tablet battery or dock battery
			echger_supply_usb_power_switch(chg_act_st, dk_batt_lv, tb_batt_lv);

			// Determine to control power direction for dock system boost trun off/on
			echger_supply_charging_direction_switch(chg_act_st, dk_batt_lv);

			msm_charger_notify_event(echg_mux_info.hw_chg, CHG_DOCK_RESTART_CHG_EVENT);
#if DEBUG_DUMP_LOG
			echger_dump_debug_msg();
#endif

			break;
		}

		case CHG_DOCK_BAT_ERR:
		{
			enum echger_tb_dk_action_st chg_act_st;
			enum echger_batt_cap_level tb_batt_lv;

			PEGA_DBG_H("[CHG_EC] Receive CHG_DOCK_BAT_ERR \n");

			if (DOCK_READY_ST != echger_check_dk_status())
			{
				PEGA_DBG_H("[CHG_EC_ERR] Dock is not ready! \n");
				break;
			}

/* BEGIN [Terry_Tzeng][2012/03/16][Check entried bypass mode case] */
			if (usb_bypass_high == echger_check_bypass_mode())
			{
				PEGA_DBG_L("[CHG_EC] Enter dock usb bypass mode \n");
				break;
			}
/* END [Terry_Tzeng][2012/03/16][Check entried bypass mode case] */

			// Update charging action status
			chg_act_st = echger_update_tb_dk_act_st();

			if (DOCK_ONLY == chg_act_st)
				echger_wake_lock();

			// Get tablet battery capacity level
			tb_batt_lv = echger_get_batt_level(TABLET_BATT_TYPE);
			echg_mux_info.curr_tb_batt_lv = tb_batt_lv;
			echg_mux_info.curr_dk_batt_lv = BATT_ERR;

			// Dock battery error handler
			echger_dk_batt_err_handler(chg_act_st, tb_batt_lv);

/* BEGIN [Terry_Tzeng][2012/04/24][Drop setting charging current for dock battery error] */
			if ((DOCK_USB_5V_IN == chg_act_st) || (DOCK_AC_INF_IN == chg_act_st))
			{
				echg_mux_info.drop_curr_flg = DROP_CURR_EN;
				msm_charger_notify_event(echg_mux_info.hw_chg, CHG_DOCK_RESTART_CHG_EVENT);
			}
			else
			{
				msleep(WAITTING_TIME);

				if (DOCK_ONLY == chg_act_st)
					echger_wake_unlock();
			}
/* END [Terry_Tzeng][2012/04/24][Drop setting charging current for dock battery error] */
			break;
		}

		case CHG_DOCK_OUT:
		{
			int reval;

			PEGA_DBG_H("[CHG_EC] Receive CHG_DOCK_OUT \n");

			if (NO_DOCK_ST != echger_check_dk_status())
			{
				PEGA_DBG_H("[CHG_EC_ERR] Dock is inserted \n");
				break;
			}
/* BEGIN [Terry_Tzeng][2012/03/16][Check entried bypass mode case] */
			dk_usb_bypass_en = usb_bypass_low;
/* END [Terry_Tzeng][2012/03/16][Check entried bypass mode case] */

			/* Lock process to handle heartbeat checking status */
			echger_wake_lock();

			echg_mux_info.curr_tb_dk_act = DOCK_OUT;

			echg_mux_info.tb_batt_full_stop = TB_BATT_START_CHG;

			reval = echger_ctrl_tb_pw_on_off(T_5BST_PW, PW_OFF);
			if (SUCCESS == reval)
				echg_mux_info.tb_bst_pw_st = PW_OFF;

			// Get tablet otg boost power switch status
			reval = echger_ctrl_tb_pw_on_off(T_OTG_PW, PW_ON);
			if (SUCCESS == reval)
				echg_mux_info.tb_otg_pw_st = PW_ON;

			/* BEGIN [Terry_Tzeng][2012/04/24][Drop setting charging current for dock battery error] */
			echg_mux_info.drop_curr_flg = DROP_CURR_DIS;
			/* END [Terry_Tzeng][2012/04/24][Drop setting charging current for dock battery error] */
			msleep(DOCK_OUT_DELAY_TIME);

			msm_charger_notify_event(echg_mux_info.hw_chg, CHG_DOCK_OUT_FINISH_EVENT);
			/* BEGIN [Terry_Tzeng][2012/05/03][Set wakelock error handler] */
			echg_mux_info.wakeup_t_retry_flg = 0;
			/* END [Terry_Tzeng][2012/05/03][Set wakelock error handler] */
			/* Unlock process to finish heartbeat checking status */
			wakeup_flag(WAKEUP_DISABLE);

			break;
		}

		case CHG_DOCK_AC_12V_IN:
		case CHG_DOCK_AC_5V_IN:
		case CHG_DOCK_USB_IN:
		{
			enum echger_tb_dk_action_st chg_act_st;
			enum echger_batt_cap_level tb_batt_lv;
			enum echger_batt_cap_level dk_batt_lv;

			PEGA_DBG_H("[CHG_EC] Receive CHG_DOCK_AC_5V_IN / CHG_DOCK_AC_12V_IN / USB in \n");

			if (DOCK_READY_ST != echger_check_dk_status())
			{
				PEGA_DBG_H("[CHG_EC] Dock is not ready! \n");
				break;
			}

			/* BEGIN [Terry_Tzeng][2012/03/16][Check entried bypass mode case] */
			if (usb_bypass_high == echger_check_bypass_mode())
			{
				PEGA_DBG_H("[CHG_EC] Enter dock usb bypass mode \n");
				if (usb_bypass_high == dk_usb_bypass_en)
					break;
				echger_handle_bypass_mode();
				break;
			}

			#if PEGA_SMT_SOLUTION
			chg_sw_ac_to_dk = SWITCH_AC_CHG_PATH_OFF;
			#endif/* PEGA_SMT_SOLUTION */
			/* END [Terry_Tzeng][2012/03/16][Check entried bypass mode case] */

			// Check dock version
			echger_check_dock_version();

			// Set delay time to turn off power switch
			/* BEGIN [Terry_Tzeng][2012/04/26][Modify dock version for PVT and SIT2] */
			if ((DOCK_PVT <= dock_ver) && (DOCK_UNKNOWN > dock_ver))
			/* END [Terry_Tzeng][2012/04/26][Modify dock version for PVT and SIT2] */
				msleep(POWER_SW_DELAY_TIEM_PVT);
			else
				msleep(POWER_SW_DELAY_TIEM_DVT);

			// Update charging action status
			chg_act_st = echger_update_tb_dk_act_st();

			if ((DOCK_AC_INF_IN != chg_act_st) &&
			    (DOCK_USB_5V_IN != chg_act_st) &&
			    (DOCK_AC_5V_IN != chg_act_st) &&
			    (DOCK_AC_12V_IN != chg_act_st))
			{
				PEGA_DBG_L("[CHG_EC] Don't handle this event %d, it isn't CHG_DOCK_AC_12V_IN/CHG_DOCK_AC_5V_IN/CHG_DOCK_USB_IN ! \n", evt);
				break;
			}

			// Enter wake lock mod
			echger_wake_lock();

			echger_disable_suspend_flag();

			/* BEGIN [Terry_Tzeng][2012/05/03][Set wakelock error handler] */
			echg_mux_info.wakeup_t_retry_flg = 0;
			/* END [Terry_Tzeng][2012/05/03][Set wakelock error handler] */

			echger_init_setting_for_changed_charging_status();

			echg_mux_info.curr_tb_dk_act = chg_act_st;
			//msm_charger_notify_event(NULL, CHG_DOCK_RESTOP_CHG_EVENT);

			// Get tablet battery capacity level
			tb_batt_lv = echger_get_batt_level(TABLET_BATT_TYPE);
			echg_mux_info.curr_tb_batt_lv = tb_batt_lv;

			// Get dock battery capacity level
			dk_batt_lv = echger_get_batt_level(DOCK_BATT_TYPE);
			echg_mux_info.curr_dk_batt_lv = dk_batt_lv;

			// Check Tablet battery is full or not
			if ((BATT_FULL_LV == tb_batt_lv) && (BATT_FULL_LV != dk_batt_lv))
			{
				echg_mux_info.tb_batt_full_stop = TB_BATT_STOP_CHG;
			}
			else
			{
				echg_mux_info.tb_batt_full_stop = TB_BATT_START_CHG;
			}

			// Determine to supply power direction from tablet battery or dock battery
			echger_supply_usb_power_switch(chg_act_st, dk_batt_lv, tb_batt_lv);

			// Determine to control power direction for dock system boost trun off/on
			echger_supply_charging_direction_switch(chg_act_st, dk_batt_lv);

			/* BEGIN [Terry_Tzeng][2012/04/24][Drop setting charging current for dock battery error] */
			if (((DOCK_USB_5V_IN == chg_act_st) ||
			     (DOCK_AC_INF_IN == chg_act_st)) &&
			    ((DROP_USB_CHG_CURR_CAP_PERCENT >= msm_chg_get_ui_ecbatt_capacity()) ||
			     (BATT_ERR == dk_batt_lv)))
			{
				echg_mux_info.drop_curr_flg = DROP_CURR_EN;
			}
			else
			{
				echg_mux_info.drop_curr_flg = DROP_CURR_DIS;
			}
			/* END [Terry_Tzeng][2012/04/24][Drop setting charging current for dock battery error] */

			msm_charger_notify_event(echg_mux_info.hw_chg, CHG_DOCK_RESTART_CHG_EVENT);
#if DEBUG_DUMP_LOG
			echger_dump_debug_msg();
#endif

			break;
		}

		/* BEGIN [Terry_Tzeng][2012/06/12][Handle informal ac adapter ] */
		case CHG_DOCK_INFORMAL_CHG:
		{
			enum echger_tb_dk_action_st chg_act_st;
			enum echger_batt_cap_level tb_batt_lv;
			enum echger_batt_cap_level dk_batt_lv;

			PEGA_DBG_H("[CHG_EC] Receive CHG_DOCK_INFORMAL_CHG \n");

			if (DOCK_READY_ST != echger_check_dk_status())
			{
				PEGA_DBG_H("[CHG_EC] Dock is not ready! \n");
				break;
			}

			/* BEGIN [Terry_Tzeng][2012/03/16][Check entried bypass mode case] */
			if (usb_bypass_high == echger_check_bypass_mode())
			{
				PEGA_DBG_H("[CHG_EC] Enter dock usb bypass mode \n");
				if (usb_bypass_high == dk_usb_bypass_en)
					break;
				echger_handle_bypass_mode();
				break;
			}

			// Update charging action status
			chg_act_st = echger_update_tb_dk_act_st();

			if (DOCK_AC_INF_IN != chg_act_st)
			{
				PEGA_DBG_L("[CHG_EC] Don't handle this event %d, it isn't informal adapter! \n", evt);
				break;
			}

			echger_disable_suspend_flag();

			/* BEGIN [Terry_Tzeng][2012/05/03][Set wakelock error handler] */
			echg_mux_info.wakeup_t_retry_flg = 0;
			/* END [Terry_Tzeng][2012/05/03][Set wakelock error handler] */

			echg_mux_info.curr_tb_dk_act = chg_act_st;
			//msm_charger_notify_event(NULL, CHG_DOCK_RESTOP_CHG_EVENT);

			// Get tablet battery capacity level
			tb_batt_lv = echger_get_batt_level(TABLET_BATT_TYPE);
			echg_mux_info.curr_tb_batt_lv = tb_batt_lv;

			// Get dock battery capacity level
			dk_batt_lv = echger_get_batt_level(DOCK_BATT_TYPE);
			echg_mux_info.curr_dk_batt_lv = dk_batt_lv;

			// Check Tablet battery is full or not
			if ((BATT_FULL_LV == tb_batt_lv) && (BATT_FULL_LV != dk_batt_lv))
			{
				echg_mux_info.tb_batt_full_stop = TB_BATT_STOP_CHG;
			}
			else
			{
				echg_mux_info.tb_batt_full_stop = TB_BATT_START_CHG;
			}

			// Determine to supply power direction from tablet battery or dock battery
			echger_supply_usb_power_switch(chg_act_st, dk_batt_lv, tb_batt_lv);

			// Determine to control power direction for dock system boost trun off/on
			echger_supply_charging_direction_switch(chg_act_st, dk_batt_lv);

			if ((DROP_USB_CHG_CURR_CAP_PERCENT >= msm_chg_get_ui_ecbatt_capacity()) ||
			    (BATT_ERR == dk_batt_lv))
			{
				echg_mux_info.drop_curr_flg = DROP_CURR_EN;
			}
			else
			{
				echg_mux_info.drop_curr_flg = DROP_CURR_DIS;
			}

			msm_charger_notify_event(echg_mux_info.hw_chg, CHG_DOCK_RESTART_CHG_EVENT);
#if DEBUG_DUMP_LOG
			echger_dump_debug_msg();
#endif

			break;
		}
		/* END [Terry_Tzeng][2012/06/12][Handle informal ac adapter ] */

		case CHG_DOCK_BAT_TEMP45:
			PEGA_DBG_H("[CHG_EC] Receive BAT_TEMP45 \n");
			break;

		case CHG_DOCK_BAT_LOW:
		{
			enum echger_tb_dk_action_st chg_act_st;
			enum echger_batt_cap_level tb_batt_lv;

			//msm_charger_notify_event(NULL, CHG_DOCK_RESTOP_CHG_EVENT);
			PEGA_DBG_H("[CHG_EC] Receive BAT_LOW \n");

			if (DOCK_READY_ST != echger_check_dk_status())
			{
				PEGA_DBG_H("[CHG_EC_ERR] Dock is not ready! \n");
				break;
			}

/* BEGIN [Terry_Tzeng][2012/03/16][Check entried bypass mode case] */
			if (usb_bypass_high == echger_check_bypass_mode())
			{
				PEGA_DBG_L("[CHG_EC] Enter dock usb bypass mode \n");
				break;
			}
/* END [Terry_Tzeng][2012/03/16][Check entried bypass mode case] */

			// Update charging action status
			chg_act_st = echger_update_tb_dk_act_st();
			PEGA_DBG_M("[CHG_EC] Update charging action status = %d \n", chg_act_st);

			if (DOCK_ONLY == chg_act_st)
				echger_wake_lock();

			// Get tablet battery capacity level
			tb_batt_lv = echger_get_batt_level(TABLET_BATT_TYPE);
			echg_mux_info.curr_tb_batt_lv = tb_batt_lv;
			echg_mux_info.curr_dk_batt_lv = BATT_LOW_LV;

			// Determine to supply power direction from tablet battery or dock battery
			echger_supply_usb_power_switch(chg_act_st, BATT_LOW_LV, tb_batt_lv);

			// Determine to control power direction for dock system boost trun off/on
			echger_supply_charging_direction_switch(chg_act_st, BATT_LOW_LV);

			/* BEGIN [Terry_Tzeng][2012/04/24][Drop setting charging current for dock battery error] */

			if ((DOCK_USB_5V_IN == chg_act_st) || (DOCK_AC_INF_IN == chg_act_st))
			{
				echg_mux_info.drop_curr_flg = DROP_CURR_EN;
				msm_charger_notify_event(echg_mux_info.hw_chg, CHG_DOCK_RESTART_CHG_EVENT);
			}
			else
			{
				msleep(WAITTING_TIME);

				if (DOCK_ONLY == chg_act_st)
					echger_wake_unlock();
			}
			/* END [Terry_Tzeng][2012/04/24][Drop setting charging current for dock battery error] */
			break;
		}

		case CHG_DOCK_BAT_CAP_FULL:
		{
			enum echger_tb_dk_action_st chg_act_st;
			enum echger_batt_cap_level tb_batt_lv;

			PEGA_DBG_H("[CHG_EC] Receive CHG_DOCK_BAT_CAP_FULL \n");

			if (DOCK_READY_ST != echger_check_dk_status())
			{
				PEGA_DBG_H("[CHG_EC_ERR] Dock is not ready! \n");
				break;
			}

/* BEGIN [Terry_Tzeng][2012/03/16][Check entried bypass mode case] */
			if (usb_bypass_high == echger_check_bypass_mode())
			{
				PEGA_DBG_L("[CHG_EC] Enter dock usb bypass mode \n");
				break;
			}
/* END [Terry_Tzeng][2012/03/16][Check entried bypass mode case] */

			echg_mux_info.tb_batt_full_stop = TB_BATT_START_CHG;

			// Update charging action status
			chg_act_st = echger_update_tb_dk_act_st();
			PEGA_DBG_M("[CHG_EC] Update charging action status = %d \n", chg_act_st);

			if (DOCK_ONLY == chg_act_st)
				echger_wake_lock();

			// Get tablet battery capacity level
			tb_batt_lv = echger_get_batt_level(TABLET_BATT_TYPE);
			echg_mux_info.curr_tb_batt_lv = tb_batt_lv;
			echg_mux_info.curr_dk_batt_lv = BATT_FULL_LV;

			// Determine to supply power direction from tablet battery or dock battery
			echger_supply_usb_power_switch(chg_act_st, BATT_FULL_LV, tb_batt_lv);

			// Determine to control power direction for dock system boost trun off/on
			echger_supply_charging_direction_switch(chg_act_st, BATT_FULL_LV);

			echg_mux_info.drop_curr_flg = DROP_CURR_DIS;

			msm_charger_notify_event(echg_mux_info.hw_chg, CHG_DOCK_RESTART_CHG_EVENT);

#if DEBUG_DUMP_LOG
			echger_dump_debug_msg();
#endif
			break;
		}

		/* BEGIN [Terry_Tzeng][2012/03/12][Handle tablet battery full status] */
		case CHG_TABLET_BAT_FULL:
		{
			enum echger_tb_dk_action_st chg_act_st;
			enum echger_batt_cap_level dk_batt_lv;

			PEGA_DBG_H("[CHG_EC] Receive CHG_TABLET_BAT_FULL \n");

			if (DOCK_READY_ST != echger_check_dk_status())
			{
				PEGA_DBG_H("[CHG_EC_ERR] Dock is not ready! \n");
				break;
			}

/* BEGIN [Terry_Tzeng][2012/03/16][Check entried bypass mode case] */
			if (usb_bypass_high == echger_check_bypass_mode())
			{
				PEGA_DBG_L("[CHG_EC] Enter dock usb bypass mode \n");
				break;
			}
/* END [Terry_Tzeng][2012/03/16][Check entried bypass mode case] */

			if (echg_mux_info.tb_batt_full_stop == TB_BATT_STOP_CHG)
			{
				PEGA_DBG_M("[CHG_EC] Tablet battery has been full not to handle CHG_TABLET_BAT_FULL event \n");
				break;
			}

			echg_mux_info.tb_batt_full_stop = TB_BATT_STOP_CHG;

			// Update charging action status
			chg_act_st = echger_update_tb_dk_act_st();

			if (DOCK_ONLY == chg_act_st)
				echger_wake_lock();

			// Get tablet battery capacity level
			dk_batt_lv = echger_get_batt_level(DOCK_BATT_TYPE);
			echg_mux_info.curr_dk_batt_lv = dk_batt_lv;
			echg_mux_info.curr_tb_batt_lv = BATT_FULL_LV;

			// Determine to supply power direction from tablet battery or dock battery
			echger_supply_usb_power_switch(chg_act_st, dk_batt_lv, BATT_FULL_LV);

			// Determine to control power direction for dock system boost trun off/on
			echger_supply_charging_direction_switch(chg_act_st, dk_batt_lv);

			msm_charger_notify_event(echg_mux_info.hw_chg, CHG_DOCK_RESTART_CHG_EVENT);

#if DEBUG_DUMP_LOG
			echger_dump_debug_msg();
#endif

			break;
		}
		/* END [Terry_Tzeng][2012/03/12][Handle tablet battery full status] */

#if PEGA_SMT_SOLUTION
		/* BEGIN [Terry_Tzeng][2012/03/13][Handle tablet and dock charging function by smt version] */
		case CHG_SMT_AC_CHG_TO_DOCK:
		{
			enum echger_tb_dk_action_st chg_act_st;

			PEGA_DBG_H("[CHG_EC] Receive CHG_SMT_AC_CHG_TO_DOCK \n");

			// Update charging action status
			chg_act_st = echger_update_tb_dk_act_st();

			if ((DOCK_AC_5V_IN != chg_act_st) &&
			    (DOCK_AC_12V_IN != chg_act_st) &&
			    (DOCK_USB_5V_IN != chg_act_st))
			{
				PEGA_DBG_H("[CHG_EC_ERR] Error charging type = %d, it can't switch charging status from pad to dock \n", chg_act_st);
				break;
			}

			chg_sw_ac_to_dk = SWITCH_AC_CHG_PATH_ON;

			msm_charger_notify_event(echg_mux_info.hw_chg, CHG_DOCK_RESTART_CHG_EVENT);

			break;
		}

		case CHG_SMT_DOCK_CHG_TO_TABLET:
		{
			enum echger_tb_dk_action_st chg_act_st;
			enum echger_batt_cap_level tb_batt_lv;
			enum echger_batt_cap_level dk_batt_lv;

			PEGA_DBG_H("[CHG_EC] Receive CHG_DOCK_AC_OUT / CHG_DOCK_IN \n");

			if (usb_bypass_high == echger_check_bypass_mode())
			{
				PEGA_DBG_H("[CHG_EC] Enter dock usb bypass mode \n");
				echger_handle_bypass_mode();
				break;
			}

			chg_sw_ac_to_dk = SWITCH_AC_CHG_PATH_OFF;

			// Update charging action status
			chg_act_st = echger_update_tb_dk_act_st();
			PEGA_DBG_M("[CHG_EC] Update charging action status = %d \n", chg_act_st);

			echger_init_setting_for_changed_charging_status();

			// Check dock version
			echger_check_dock_version();

			// Set delay time to turn off power switch
/* BEGIN [Terry_Tzeng][2012/04/26][Modify dock version for PVT and SIT2] */
			if ((DOCK_PVT <= dock_ver) && (DOCK_UNKNOWN > dock_ver))
/* END [Terry_Tzeng][2012/04/26][Modify dock version for PVT and SIT2] */
				msleep(POWER_SW_DELAY_TIEM_PVT);
			else
				msleep(POWER_SW_DELAY_TIEM_DVT);


			echg_mux_info.curr_tb_dk_act = chg_act_st;

			echg_mux_info.tb_batt_full_stop = TB_BATT_START_CHG;

			// Get tablet battery capacity level
			tb_batt_lv = echger_get_batt_level(TABLET_BATT_TYPE);
			echg_mux_info.curr_tb_batt_lv = tb_batt_lv;

			// Get dock battery capacity level
			dk_batt_lv = echger_get_batt_level(DOCK_BATT_TYPE);
			echg_mux_info.curr_dk_batt_lv = dk_batt_lv;

			// Determine to supply power direction from tablet battery or dock battery
			echger_supply_usb_power_switch(chg_act_st, dk_batt_lv, tb_batt_lv);

			// Determine to control power direction for dock system boost trun off/on
			echger_supply_charging_direction_switch(chg_act_st, dk_batt_lv);

			msm_charger_notify_event(echg_mux_info.hw_chg, CHG_DOCK_RESTART_CHG_EVENT);

#if DEBUG_DUMP_LOG
			echger_dump_debug_msg();
#endif/* DEBUG_DUMP_LOG */

			break;
		}
		/* END [Terry_Tzeng][2012/03/13][Handle tablet and dock charging function by smt version] */

#endif/* PEGA_SMT_SOLUTION */

		default:
			break;
	}

	mutex_unlock(&echg_mux_info.status_lock);
}

static int echger_dequeue_event(int **event)
{
	unsigned long flags;

	spin_lock_irqsave(&echg_mux_info.queue_lock, flags);

	if (echg_mux_info.queue_count == 0)
	{
		spin_unlock_irqrestore(&echg_mux_info.queue_lock, flags);
		PEGA_DBG_M("[CHG_EC] %s Dequeueing echg_mux_info.queue_count == 0\n", __func__);
		return -EINVAL;
	}
	*event = &echg_mux_info.queue_evt[echg_mux_info.head];
	echg_mux_info.head = (echg_mux_info.head + 1) % ECHGER_MAX_EVENTS;
	//pr_debug("%s dequeueing %d\n", __func__, *event);
	PEGA_DBG_M("[CHG_EC]%s dequeueing %d\n", __func__, **event);
	echg_mux_info.queue_count--;
	spin_unlock_irqrestore(&echg_mux_info.queue_lock, flags);
	return 0;
}

static int echger_enqueue_event(int event)
{
	unsigned long flags;

	spin_lock_irqsave(&echg_mux_info.queue_lock, flags);
	if (echg_mux_info.queue_count == ECHGER_MAX_EVENTS)
	{
		spin_unlock_irqrestore(&echg_mux_info.queue_lock, flags);
		pr_err("%s: queue full cannot enqueue %d\n",
				__func__, event);
		return -EAGAIN;
	}
	pr_debug("%s queueing %d\n", __func__, event);
	echg_mux_info.queue_evt[echg_mux_info.tail] = event;
	echg_mux_info.tail = (echg_mux_info.tail + 1)%ECHGER_MAX_EVENTS;
	echg_mux_info.queue_count++;
	spin_unlock_irqrestore(&echg_mux_info.queue_lock, flags);
	return 0;
}


static void echger_process_events(struct work_struct *work)
{
	int *event = NULL;
	int rc;

	do {
		rc = echger_dequeue_event(&event);

		if (!rc && (NULL != event))
			echger_handle_event(*event);
	} while (!rc && (NULL != event));
}


static void ec_wakeup_tablet_cb(int evt)
{
	/* BEGIN [Terry_Tzeng][2012/05/21][Switch supported usb power path before started ec reset on time] */
	if (DO_ECRST == evt)
	{
		int reval;

		PEGA_DBG_H("[CHG_EC] %s Handle DO_ECRST \n", __func__);

		if (usb_bypass_high == echger_check_bypass_mode())
		{
			PEGA_DBG_H("[CHG_EC] Enter dock usb bypass mode \n");
			return;
		}

		reval = echger_ctrl_tb_pw_on_off(T_OTG_PW, PW_OFF);
		if (SUCCESS == reval)
			echg_mux_info.tb_otg_pw_st = PW_OFF;

		reval = echger_ctrl_tb_pw_on_off(T_5BST_PW, PW_ON);
		if (SUCCESS == reval)
			echg_mux_info.tb_bst_pw_st = PW_ON;

		reval = echger_ctrl_bst_action(OTG_5V, BOOST_OFF);
		if (reval == SUCCESS)
			echg_mux_info.dk_otg_bst_st = BOOST_OFF;

		return;
	}
	/* END [Terry_Tzeng][2012/05/21][Switch supported usb power path before started ec reset on time] */

	if (((evt < BAT_ERR) ||
	     (evt > AC_OUT)) &&
	     (evt != BAT_CAP_FULL) &&
	     (evt != BAT_LOW) &&
	     (evt != DOCK_WKUP) &&
	     (evt != EC_RESET) &&
	     (evt != INFORMAL_CHG) &&
	     (evt != BAT_TEMP45))
	{
		return;
	}

	echger_enqueue_event(evt);
	queue_work(echg_mux_info.event_wq_thread, &echg_mux_info.queue_work);
}
/* END [Terry_Tzeng][2011/01/09][Add register callback fucntion for handled dock charging status from ec] */


static void echger_set_dk_charging_status(int tb_in_curr, int tb_chg_curr)
{
	int reval;
	int ac_type;
	int get_dk_rq = CHARGING_OFF;
	enum echger_batt_cap_level tb_batt_lv;
	enum echger_batt_cap_level dk_batt_lv;

	PEGA_DBG_M("[CHG_EC] %s \n", __func__);

/* BEGIN [Terry_Tzeng][2012/03/16][Check entried bypass mode case] */
	if (usb_bypass_high == echger_check_bypass_mode())
	{
		PEGA_DBG_H("[CHG_EC] Enter dock usb bypass mode not to charging to dock battery \n");

		// Get dock charger action status
		reval = get_ec_charging_req_status(&get_dk_rq);
		if (FAIL == reval)
		{
			PEGA_DBG_H("[CHG_EC_ERR] %s Get dock charging status is fail \n", __func__);
			return;
		}

		if (CHARGING_OFF != get_dk_rq)
		{
			ec_charging_req(CHARGING_OFF);
		}

		return;
	}

#if PEGA_SMT_SOLUTION
	if (chg_sw_ac_to_dk == SWITCH_AC_CHG_PATH_ON)
	{
		reval = get_ec_charging_req_status(&get_dk_rq);
		if (FAIL == reval)
		{
			PEGA_DBG_H("[CHG_EC_ERR] %s Get dock charging status is fail \n", __func__);
		}

		if (CHARGING_ON != get_dk_rq)
		{
			reval = ec_charging_req(CHARGING_ON);
			if (FAIL == reval)
			{
				PEGA_DBG_H("[CHG_EC_ERR] %s Request dock charing is fail \n", __func__);
			}
		}

		return;
	}
#endif /* PEGA_SMT_SOLUTION */
/* END [Terry_Tzeng][2012/03/16][Check entried bypass mode case] */

	if (DOCK_READY_ST != echger_check_dk_status())
	{
		PEGA_DBG_H("[CHG_EC_ERR] %s Dock is not ready! \n", __func__);
		return;
	}

	reval = get_ac_type(&ac_type);
	if (FAIL == reval)
	{
		PEGA_DBG_H("[CHG_EC_ERR] %s Get ac type is fail \n", __func__);

		return;
	}

	// Get tablet battery capacity level
	tb_batt_lv = echger_get_batt_level(TABLET_BATT_TYPE);
	// Get dock battery capacity level
	dk_batt_lv = echger_get_batt_level(DOCK_BATT_TYPE);

	// Get dock charger action status
	reval = get_ec_charging_req_status(&get_dk_rq);
	if (FAIL == reval)
	{
		PEGA_DBG_H("[CHG_EC_ERR] %s Get dock charging status is fail \n", __func__);
		return;
	}

	PEGA_DBG_L("[CHG_EC] Get dock charging status = %d, tablet_charging_flag = %d \n", get_dk_rq, echg_mux_info.tb_batt_full_stop);
	PEGA_DBG_L("[CHG_EC] Get tablet in curr = %d, chg curr = %d \n", tb_in_curr, tb_chg_curr);

	switch (ac_type)
	{
		case AC_NONE:
		{
			if (CHARGING_ON == get_dk_rq)
				ec_charging_req(CHARGING_OFF);

			break;
		}

		case AC_INFORMAL:
		case AC_5V:
		case AC_USB:
		{
			// Check dock version
			echger_check_dock_version();

			// Set delay time to turn off power switch
			if (DOCK_DVT == dock_ver)
			{
				if ((CHARGING_ON != get_dk_rq) &&
				   (0 == tb_in_curr) &&
				   (0 == tb_chg_curr) &&
				   (TB_BATT_START_CHG !=echg_mux_info.tb_batt_full_stop))
				{
					PEGA_DBG_M("[CHG_EC] %s Request dock starting charging \n", __func__);
					ec_charging_req(CHARGING_ON);
				}
				else if ((CHARGING_ON == get_dk_rq) &&
					     (0 < tb_in_curr))
				{
					ec_charging_req(CHARGING_OFF);
				}
			}
			else
			{
				if (CHARGING_OFF == get_dk_rq)
					ec_charging_req(CHARGING_ON);
			}

			break;
		}

		case AC_12V:
		{
			if ((CHARGING_ON != get_dk_rq) &&
			    (0 == tb_in_curr) &&
			    (0 == tb_chg_curr) &&
			    (TB_BATT_START_CHG !=echg_mux_info.tb_batt_full_stop))
			{
				PEGA_DBG_L("[CHG_EC] %s Request dock starting charging \n", __func__);
				ec_charging_req(CHARGING_ON);
			}
			else if ((CHARGING_ON == get_dk_rq) &&
			         (0 < tb_in_curr))
			{
				ec_charging_req(CHARGING_OFF);
			}

			break;
		}

		default:
			break;
	}
}


int echger_determine_ec_wakeup_timer_en(void)
{
	int ret = 0;

	if ((echg_mux_info.curr_tb_dk_act == DOCK_ONLY) &&
	    (echg_mux_info.curr_dk_batt_lv != BATT_LOW_LV))
	{
		ret = 1;
	}

	PEGA_DBG_H("[CHG_EC] %s, ec wake up timer en = %d, Enable : 1, Disable : 0 \n", __func__, ret);

	return ret;
}

/* BEGIN [Terry_Tzeng][2012/04/17][Add function of get dock hardward version] */
int echger_get_dk_hw_id(void)
{
	int dk_hw_version;

	if (NO_DOCK_ST == echger_check_dk_status())
	{
		PEGA_DBG_H("[CHG_EC_ERR] %s Dock is not ready! \n", __func__);
		return FAIL;
	}

	if (FAIL == ec_dock_ver(&dk_hw_version))
	{
		PEGA_DBG_H("[CHG_EC_ERR] %s Get dock version is fail to switch default is PVT\n", __func__);
		return FAIL;
	}

	return dk_hw_version;
}
/* END [Terry_Tzeng][2012/04/17][Add function of get dock hardward version] */

void echger_notify_tb_start_chg_finish(int tb_in_curr, int tb_chg_curr)
{
	echger_set_dk_charging_status(tb_in_curr, tb_chg_curr);
}


int echger_get_tb_in_chg_curr(int * gtb_in_curr ,int * gtb_chg_curr)
{
	int reval;
	int ac_type;
	int in_curr = 0, chg_curr = 0;
	enum echger_batt_cap_level tb_batt_lv;
	enum echger_batt_cap_level dk_batt_lv;
	enum echger_tb_dk_action_st tb_dk_st;

	PEGA_DBG_M("[CHG_EC] %s \n", __func__);

/* BEGIN [Terry_Tzeng][2012/03/16][Check entried bypass mode case] */
	if (usb_bypass_high == echger_check_bypass_mode())
	{
		PEGA_DBG_H("[CHG_EC] Enter dock usb bypass mode not to charging to dock battery \n");
		*gtb_in_curr = ECCHG_BYPASS_IN_CURR;
		*gtb_chg_curr = 0;

		return ECCHG_CHG_ST_IS_CHANGE;
	}

#if PEGA_SMT_SOLUTION
	if (chg_sw_ac_to_dk == SWITCH_AC_CHG_PATH_ON)
	{
		PEGA_DBG_H("[CHG_EC] Enter dock usb bypass mode not to charging to dock battery \n");
		*gtb_in_curr = 0;
		*gtb_chg_curr = 0;

		return ECCHG_CHG_ST_IS_CHANGE;
	}
#endif /* PEGA_SMT_SOLUTION */
/* END [Terry_Tzeng][2012/03/16][Check entried bypass mode case] */

	/* Check tablet and dock status */
	tb_dk_st = echger_update_tb_dk_act_st();

	if (NO_DOCK_ST == echger_check_dk_status())
	{
		PEGA_DBG_H("[CHG_EC_ERR] %s Dock is not ready! \n", __func__);
		*gtb_in_curr = 0;
		*gtb_chg_curr = 0;

		return ECCHG_CHG_ST_IS_CHANGE;
	}

	reval = get_ac_type(&ac_type);
	if (FAIL == reval)
	{
		PEGA_DBG_H("[CHG_EC_ERR] %s Get ac type is fail \n", __func__);
		*gtb_in_curr = 0;
		*gtb_chg_curr = 0;

		return ECCHG_CHG_ST_IS_CHANGE;
	}

	// Get tablet battery capacity level
	tb_batt_lv = echger_get_batt_level(TABLET_BATT_TYPE);
	// Get dock battery capacity level
	dk_batt_lv = echger_get_batt_level(DOCK_BATT_TYPE);

	PEGA_DBG_L("[CHG_EC] Get tb_batt_lv = %d, dk_batt_lv = %d, ac_type = %d \n", tb_batt_lv, dk_batt_lv, ac_type);
	PEGA_DBG_L("[CHG_EC] Get echg_mux_info.tb_batt_full_stop = %d \n", echg_mux_info.tb_batt_full_stop);

	if (AC_NONE == ac_type)
	{
		if ((BATT_FULL_LV != tb_batt_lv) &&
		    (BATT_ERR != tb_batt_lv) &&
		    (BATT_LOW_LV != dk_batt_lv) &&
		    (BATT_ERR != dk_batt_lv))
		{
			in_curr = ECCHG_DOCKIN_IN_CURR;
			chg_curr = ECCHG_DOCKIN_CHG_CURR;
		}
		else if ((BATT_FULL_LV == tb_batt_lv) &&
		         (BATT_LOW_LV != dk_batt_lv) &&
		         (BATT_ERR != dk_batt_lv))
		{
			if (echg_mux_info.suspend_status == SUSPEND_STATUS_DIS)
			{
				in_curr = ECCHG_DOCKIN_IN_CURR;
				chg_curr = 0;
			}
			else
			{
				in_curr = 0;
				chg_curr = 0;
			}
		}
		else
		{
			in_curr = 0;
			chg_curr = 0;
		}
	}
	else if (AC_5V == ac_type)
	{
		if ((BATT_FULL_LV != tb_batt_lv) &&
		    (BATT_ERR != tb_batt_lv) &&
		    (TB_BATT_STOP_CHG != echg_mux_info.tb_batt_full_stop))
		{
			/* BEGIN [Terry_Tzeng][2012/04/26][Modify dock version for PVT and SIT2] */
			if ((DOCK_PVT <= dock_ver) && (DOCK_UNKNOWN > dock_ver))
			/* END [Terry_Tzeng][2012/04/26][Modify dock version for PVT and SIT2] */
			{
				in_curr = ECCHG_AC5V_IN_CURR_PVT;
				chg_curr = ECCHG_AC5V_CHG_CURR_PVT;
			}
			else
			{
				in_curr = ECCHG_AC5V_IN_CURR_DVT;
				chg_curr = ECCHG_AC5V_CHG_CURR_DVT;
			}
		}
		else if (BATT_FULL_LV == tb_batt_lv)
		{
			/* BEGIN [Terry_Tzeng][2012/04/26][Modify dock version for PVT and SIT2] */
			if ((DOCK_PVT <= dock_ver) && (DOCK_UNKNOWN > dock_ver))
			/* END [Terry_Tzeng][2012/04/26][Modify dock version for PVT and SIT2] */
				in_curr = ECCHG_AC5V_IN_CURR_PVT;
			else
				in_curr = ECCHG_AC5V_IN_CURR_DVT;

			chg_curr = 0;
		}
		else
		{
			in_curr = 0;
			chg_curr = 0;
		}
	}
	else if (AC_INFORMAL == ac_type)
	{
		if ((BATT_FULL_LV != tb_batt_lv) &&
		    (BATT_ERR != tb_batt_lv) &&
		    (TB_BATT_STOP_CHG != echg_mux_info.tb_batt_full_stop))
		{
			if ((DOCK_PVT <= dock_ver) && (DOCK_UNKNOWN > dock_ver))
			{
				in_curr = ECCHG_AC5V_IN_CURR_PVT;
				chg_curr = ECCHG_AC5V_CHG_CURR_PVT;
			}
			else
			{
				in_curr = ECCHG_AC5V_IN_CURR_DVT;
				chg_curr = ECCHG_AC5V_CHG_CURR_DVT;
			}

			if (DROP_CURR_EN == echg_mux_info.drop_curr_flg)
			{
				int temp_curr = msm_chg_get_ecbatt_in_curr();

				if ((0 < temp_curr) && (temp_curr < EC_CURR_1920))
				{
					if (temp_curr == EC_CURR_1024)
						in_curr = ECCHG_INFORMAL_CHG_1024_DROP_CURR;
					else if (temp_curr == EC_CURR_640)
						in_curr = ECCHG_INFORMAL_CHG_640_DROP_CURR;
					else if (temp_curr == EC_CURR_512)
						in_curr = ECCHG_INFORMAL_CHG_512_DROP_CURR;
					else if (temp_curr == EC_CURR_384)
						in_curr = ECCHG_INFORMAL_CHG_384_DROP_CURR;
					else
						in_curr = ECCHG_INFORMAL_AC_IN_CURR_DK_BATT_LOW;
				}

				PEGA_DBG_L("[CHG_EC] Get INFORMAL AC type to change input current = %d, dock input current = %d \n", in_curr, temp_curr);
			}
		}
		else if (BATT_FULL_LV == tb_batt_lv)
		{
			if ((DOCK_PVT <= dock_ver) && (DOCK_UNKNOWN > dock_ver))
				in_curr = ECCHG_AC5V_IN_CURR_PVT;
			else
				in_curr = ECCHG_AC5V_IN_CURR_DVT;

			chg_curr = 0;

			if (DROP_CURR_EN == echg_mux_info.drop_curr_flg)
			{
				int temp_curr = msm_chg_get_ecbatt_in_curr();

				if ((0 < temp_curr) && (temp_curr < EC_CURR_1920))
				{
					if (temp_curr == EC_CURR_1024)
						in_curr = ECCHG_INFORMAL_CHG_1024_DROP_CURR;
					else if (temp_curr == EC_CURR_640)
						in_curr = ECCHG_INFORMAL_CHG_640_DROP_CURR;
					else if (temp_curr == EC_CURR_512)
						in_curr = ECCHG_INFORMAL_CHG_512_DROP_CURR;
					else if (temp_curr == EC_CURR_384)
						in_curr = ECCHG_INFORMAL_CHG_384_DROP_CURR;
					else
						in_curr = ECCHG_INFORMAL_AC_IN_CURR_DK_BATT_LOW;
				}

				PEGA_DBG_L("[CHG_EC] Get INFORMAL AC type to change input current = %d, dock input current = %d \n", in_curr, temp_curr);
			}
		}
		else
		{
			in_curr = 0;
			chg_curr = 0;
		}
	}
	else if (AC_USB == ac_type)
	{
/* BEGIN [Terry_Tzeng][2012/04/17][Modify determined charging and input current condition for PVT] */
		if ((BATT_FULL_LV != tb_batt_lv) &&
		    (BATT_ERR != tb_batt_lv) &&
		    (TB_BATT_STOP_CHG != echg_mux_info.tb_batt_full_stop))
/* END [Terry_Tzeng][2012/04/17][Modify determined charging and input current condition for PVT] */
		{
/* BEGIN [Terry_Tzeng][2012/04/17][Modify input current current default value for PVT and DVT] */
			/* BEGIN [Terry_Tzeng][2012/04/26][Modify dock version for PVT and SIT2] */
			if ((DOCK_PVT <= dock_ver) && (DOCK_UNKNOWN > dock_ver))
			/* END [Terry_Tzeng][2012/04/26][Modify dock version for PVT and SIT2] */
			{
				if (DROP_CURR_EN == echg_mux_info.drop_curr_flg)
				{
					in_curr = ECCHG_USBIN_IN_CURR_DK_BATT_LOW_PVT;
					chg_curr = ECCHG_USBIN_CHG_CURR_DK_BATT_LOW_PVT;
				}
				else
				{
					in_curr = ECCHG_USBIN_IN_CURR_PVT;
					chg_curr = ECCHG_USBIN_CHG_CURR_PVT;
				}
			}
			else
			{
				in_curr = ECCHG_USBIN_IN_CURR_DVT;
				chg_curr = ECCHG_USBIN_CHG_CURR_DVT;
			}
/* END [Terry_Tzeng][2012/04/17][Modify input current current default value for PVT and DVT] */
		}
		else if (BATT_FULL_LV == tb_batt_lv)
		{
/* BEGIN [Terry_Tzeng][2012/04/17][Modify input current current default value for PVT and DVT] */
			if ((DOCK_PVT <= dock_ver) && (DOCK_UNKNOWN > dock_ver))
			{
				if (DROP_CURR_EN == echg_mux_info.drop_curr_flg)
				{
					in_curr = ECCHG_USBIN_IN_CURR_DK_BATT_LOW_PVT;
				}
				else
				{
					in_curr = ECCHG_USBIN_IN_CURR_PVT;
				}
			}
			else
				in_curr = ECCHG_USBIN_IN_CURR_DVT;
/* END [Terry_Tzeng][2012/04/17][Modify input current current default value for PVT and DVT] */

			chg_curr = 0;
		}
		else
		{
			in_curr = 0;
			chg_curr = 0;
		}
	}
	else if (AC_12V == ac_type)
	{
		if ((BATT_FULL_LV != tb_batt_lv) &&
		    (BATT_ERR != tb_batt_lv) &&
		    (TB_BATT_STOP_CHG != echg_mux_info.tb_batt_full_stop))
		{
			in_curr = ECCHG_AC12_IN_CURR;
			chg_curr = ECCHG_AC12_CHG_CURR;
		}
		else if (BATT_FULL_LV == tb_batt_lv)
		{
			in_curr = ECCHG_AC12_IN_CURR;
			chg_curr = 0;
		}
		else
		{
			in_curr = 0;
			chg_curr = 0;
		}
	}
	else
	{
			in_curr = 0;
			chg_curr = 0;
	}

	*gtb_in_curr = in_curr;
	*gtb_chg_curr = chg_curr;

	if (tb_dk_st != echg_mux_info.curr_tb_dk_act)
	{
		return ECCHG_CHG_ST_IS_CHANGE;
	}
	else
	{
		return ECCHE_CHG_ST_NO_CHANGE;
	}

}


/* BEGIN [Terry_Tzeng][2012/03/28][Add function of notification tablet entried suspend mode] */
static void echger_tb_early_suspend_notify(void)
{
	PEGA_DBG_M("[CHG_EC] %s\n", __func__);

	//msm_charger_notify_event(echg_mux_info.hw_chg, CHG_DOCK_SET_PERIOD_CHG_CURR_EVENT);

}

static void echger_tb_late_resume_notify(void)
{
	PEGA_DBG_M("[CHG_EC] %s\n", __func__);

	if ((DOCK_ONLY != echger_update_tb_dk_act_st()) ||
	    (echg_mux_info.suspend_status != SUSPEND_STATUS_EN))
		return;

	echger_enqueue_event(CHG_DOCK_SYS_SUSPEND_TO_LATE_RESUME);
	queue_work(echg_mux_info.event_wq_thread, &echg_mux_info.queue_work);
}
/* END [Terry_Tzeng][2012/03/28][Add function of notification tablet entried suspend mode] */

static int echger_get_safety_time(void)
{
	int safe_time;
	int ac_type;

	if (NO_DOCK_ST == echger_check_dk_status())
	{
		PEGA_DBG_H("[CHG_EC_ERR] %s Dock is not ready! \n", __func__);
		return 0;
	}

	if (FAIL == get_ac_type(&ac_type))
	{
		PEGA_DBG_H("[CHG_EC_ERR] %s Get ac type is fail \n", __func__);
		return 0;
	}

	if (AC_NONE == ac_type)
	{
		safe_time = AC_NONE_SAFETY_TIME;
	}
	else if (AC_USB == ac_type)
	{
		safe_time = AC_USB_SAFETY_TIME;
	}
	else
	{
		safe_time = AC_5V_SAFETY_TIME;
	}

	return safe_time;
}


/* BEGIN [Terry_Tzeng][2012/01/11][Add tablet to dock processing callback function] */
static int echger_is_tb_batt_change_cb(struct msm_hardware_charger_priv * hw_chg_priv ,int evt)
{

	PEGA_DBG_M("[CHG_EC] %s Receive event = %d\n", __func__, evt);

	echg_mux_info.hw_chg_priv = hw_chg_priv;

	if (CHG_DOCK_HEARTBEAT_CHECK == evt)
	{
		evt = CHG_DOCK_HEARTBEAT_CHK;
	}
	else if (CHG_DOCK_INSERTED_EVENT == evt)
	{
		evt = CHG_DOCK_IN;
	}
	else if (CHG_DOCK_REMOVED_EVENT == evt)
	{
		evt = CHG_DOCK_OUT;
	}
	/* BEGIN [Terry_Tzeng][2012/03/12][Handle tablet battery full event] */
	else if (CHG_DONE_EVENT == evt)
	{
		evt = CHG_TABLET_BAT_FULL;
	}
	/* END [Terry_Tzeng][2012/03/12][Handle tablet battery full event] */
/* BEGIN [Terry_Tzeng][2012/03/13][Handle tablet and dock charging function by smt version] */
#if PEGA_SMT_SOLUTION
	else if (CHG_DOCK_SMT_DK_CHG_TO_TB == evt)
	{
		evt = CHG_SMT_DOCK_CHG_TO_TABLET;
	}
	else if (CHG_DOCK_SMT_AC_CHG_TO_DK == evt)
	{
		evt = CHG_SMT_AC_CHG_TO_DOCK;
	}
#endif
/* END [Terry_Tzeng][2012/03/13][Handle tablet and dock charging function by smt version] */
	else
	{
		return 0;
	}

	echger_enqueue_event(evt);
	queue_work(echg_mux_info.event_wq_thread, &echg_mux_info.queue_work);

	return 0;
}
/* END [Terry_Tzeng][2012/01/11][Add tablet to dock processing callback function] */


static struct msm_ec_dock ec_dock = {
	.is_tb_batt_change_cb = echger_is_tb_batt_change_cb,
	.tb_early_suspend_notify = echger_tb_early_suspend_notify,
	.tb_late_resume_notify = echger_tb_late_resume_notify,
	.get_safety_time = echger_get_safety_time,
};
/* END [Terry_Tzeng][2012/01/11][Set dock in process callback function - echger_is_dock_hdlr_cb] */


int msm_dock_ec_init(struct msm_hardware_charger *hw_chg)
{
	int rc;

	PEGA_DBG_H("[CHG_EC] %s\n", __func__);

	if (NULL == hw_chg)
	{
		PEGA_DBG_H("[CHG_EC_ERR] %s Get msm_hardware_charger structure is NULL\n", __func__);
	}
	echg_mux_info.hw_chg = hw_chg;

	echg_mux_info.curr_tb_dk_act = DOCK_OUT;
	echg_mux_info.dk_otg_bst_st = BOOST_OFF;
	echg_mux_info.dk_sys_bst_st = BOOST_OFF;
	echg_mux_info.tb_otg_pw_st = PW_OFF;
	echg_mux_info.tb_bst_pw_st = PW_OFF;
	echg_mux_info.tb_batt_full_stop = TB_BATT_START_CHG;
	echg_mux_info.queue_evt = kzalloc(sizeof(int)* ECHGER_CTRL_COUNT* ECHGER_MAX_EVENTS, GFP_KERNEL);

	if (!echg_mux_info.queue_evt)
	{
		rc = -ENOMEM;
		PEGA_DBG_H("[CHG_EC_ERR] %s create echg_mux_info.queue_evt is fail\n", __func__);
		goto out;
	}

	mutex_init(&echg_mux_info.status_lock);
	spin_lock_init(&echg_mux_info.queue_lock);
	echg_mux_info.queue_count = 0;
	echg_mux_info.tail = 0;
	echg_mux_info.head = 0;
	echg_mux_info.wakelock_flag = WAKEUP_DIS;

/* BEGIN [Terry_Tzeng][2012/04/24][Drop charging current when dock battery is low statement in AC 5V or USB in]  */
	echg_mux_info.drop_curr_flg = DROP_CURR_DIS;
/* END [Terry_Tzeng][2012/04/24][Drop charging current when dock battery is low statement in AC 5V or USB in]  */
/* BEGIN [Terry_Tzeng][2012/05/03][Initial system suspend status]  */
	echg_mux_info.suspend_status = SUSPEND_STATUS_DIS;
/* END [Terry_Tzeng][2012/05/03][Initial system suspend status]  */
/* BEGIN [Terry_Tzeng][2012/05/03][Set wakelock error handler] */
	echg_mux_info.wakeup_t_retry_flg = 0;
/* END [Terry_Tzeng][2012/05/03][Set wakelock error handler] */

	INIT_WORK(&echg_mux_info.queue_work, echger_process_events);
	echg_mux_info.event_wq_thread = create_workqueue("ecchg_ctrl_eventd");
	if (!echg_mux_info.event_wq_thread)
	{
		rc = -ENOMEM;
		goto free_queue;
	}

/* BEGIN [Terry_Tzeng][2011/01/09][Register callback fucntion to ec for AC-IN/OUT or Low batter from dock] */
	register_ec_event_func(ec_wakeup_tablet_cb);
/* END [Terry_Tzeng][2011/01/09][Register callback fucntion to ec for AC-IN/OUT or Low batter from dock] */

	msm_dock_ec_register(&ec_dock);

/* BEGIN [Terry_Tzeng][2012/03/16][Check entried bypass mode case] */
	dk_usb_bypass_en = usb_bypass_low;
/* END [Terry_Tzeng][2012/03/16][Check entried bypass mode case] */

	return 0;

free_queue:
	kfree(echg_mux_info.queue_evt);
out:
	return rc;
}


void msm_dock_ec_exit(void)
{
	echg_mux_info.hw_chg = NULL;
	mutex_destroy(&echg_mux_info.status_lock);
	PEGA_DBG_H("[CHG_EC] %s\n", __func__);
	msm_dock_ec_unregister(&ec_dock);
	flush_workqueue(echg_mux_info.event_wq_thread);
	destroy_workqueue(echg_mux_info.event_wq_thread);
	kfree(echg_mux_info.queue_evt);
}

