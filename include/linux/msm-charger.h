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
#ifndef __MSM_CHARGER_H__
#define __MSM_CHARGER_H__

#include <linux/power_supply.h>

#ifdef CONFIG_PEGA_ISL_CHG
// if wants to build local build for SMT version, enable this flag
//#define PEGA_SMT_BUILD	1

/*
Provide 2 solutions for SMT
1. do nothing while usb plug-in, including no charging and no register modifying
2. disable usb charging and set contron register=1 and charging register=128
   ( need to do the same setting in SBL3)
*/
#ifdef PEGA_SMT_BUILD
#define PEGA_SMT_SOLUTION   1
#else
#define PEGA_SMT_SOLUTION   0
#endif

/*
interrupt handler soultion
0. Qualcomm origial design
1. push to work queue and add de-bounce
*/
#define PEGA_INT_HANDLER_SOLUTION   1

#define MSM_CHARGER_RESUME_COUNT 	5
#define SAFETY_RESUME_COUNT 		2
#define AC_OK_STATUS_BIT			0x40
#define ISOLATE_ADAPTER_STATUS_BIT	0x04
#define ISL_REG_CURRENT_BASE		0x80
#define PEGA_CHG_HARD_CODE_12V  	0
/* BEGIN [Jackal_Chen][2012/03/29][Modify the logic of resume_charging()] */
#define PEGA_CHG_RESUME_BATT_LEVEL       90
/* END [Jackal_Chen][2012/03/29][Modify the logic of resume_charging()] */

/*
Adjust charging current according to cell temperature and voltage
1. temperature devided into 4 level
2. voltage devided into 3 level (currently only uses first level)
*/
#define VOLT_MULTI_LEVEL_ENABLE     1

#define CURR_TBL_TEMPE_ITEMS        5
#define CURR_TBL_VOLT_ITEMS         3
#define VOLT_HYSTERESIS             20

#define TEMPE_STOP_CHG_DEGREE_H     450 /*450*/
#define TEMPE_STOP_CHG_DEGREE_M     250 /*250*/
#define TEMPE_STOP_CHG_DEGREE_L     100
#define TEMPE_STOP_CHG_DEGREE_STOP  0

#define VOLT_SLOW_CHG_LEVEL_H       4100
#define VOLT_SLOW_CHG_LEVEL_L       4000

// BEGIN [2012/03/05] Jackal - Remove un-used sanpaolo/santiago define
#define R_MULTIPLE                  1
#define R_MULTIPLE_REV              1

#define BACKLIGHT_DYNAMIC_CURRENT     0

/* charge current depended on backlight level */
#define CHG_CURR_BACKLIGHT_LEVEL_1  	1536    // 1% ~ 30%
#define CHG_CURR_BACKLIGHT_LEVEL_2  	1280    //31% ~ 60%
#define CHG_CURR_BACKLIGHT_LEVEL_3  	1024    //61% ~ 100%
// END [2012/03/05] Jackal - Remove un-used sanpaolo/santiago define

#endif

/* BEGIN [Jackal_Chen][2012/01/16][Add constant for dock_in GPIO value] */
#define PM8921_GPIO_NUMBER_BASE                   152
#define PM8921_GPIO_TO_SYS(pm_gpio)	(pm_gpio - 1 + PM8921_GPIO_NUMBER_BASE)
#define PM8921_MPP_NUMBER_BASE                    196
#define PM8921_MPP_TO_SYS(pm_gpio)	(pm_gpio - 1 + PM8921_MPP_NUMBER_BASE)
#define MSM8960_DOCK_IN_GPIO        7
/* BEGIN [Jackal_Chen][2012/04/09][Use HW_ID value to distinguish GPIO number of dock_in pin] */
#define MSM8960_DOCK_IN_GPIO_SIT2        35
/* END [Jackal_Chen][2012/04/09][Use HW_ID value to distinguish GPIO number of dock_in pin] */
#define PM8921_MPP12_GPIO                (PM8921_MPP_TO_SYS(12))
#define PM8921_CBL_PWR_IRQ_NUMBER    475
/* END [Jackal_Chen][2012/01/16][Add constant for dock_in GPIO value] */
/* BEGIN [Jackal_Chen][2012/04/05][Non-formal charger support] */
#define PM_USB_IN_VALID_IRQ_HDL_8921    455
#define PM_DC_IN_VALID_IRQ_HDL_8921       482
/* END [Jackal_Chen][2012/04/05][Non-formal charger support] */
/* BEGIN tim_ph@pegatron [2012/01/06][Add docking support] */
#define MSM_CHARGER_ENABLE_DOCK
/* END   tim_ph@pegatron [2012/01/06][Add docking support] */
/* BEGIN [Terry_Tzeng][2012/01/18][Define tablet power switch gpio] */
#ifdef MSM_CHARGER_ENABLE_DOCK
#define PM8921_TB_BOOST_5V_PW_GPIO              42
#define PM8921_TB_OTG_PW_GPIO                   3
/* END [Terry_Tzeng][2012/01/18][Define tablet power switch gpio] */
/* BEGIN [Terry_Tzeng][2012/02/08][Define max current value] */
#define INSERT_DK_MAX_CURR                      2000
/* END [Terry_Tzeng][2012/02/08][Define max current value] */
/* BEGIN [2012/03/05] Jackal - Define max current value */
#define MAX_CHG_CURR_USB                      500
#define MAX_CHG_CURR_USB_WALL       1500
#define MAX_CHG_CURR_AC                         3000
/* END [2012/03/05] Jackal - Define max current value */
#define ECCHG_CHG_ST_IS_CHANGE                  1
#define ECCHE_CHG_ST_NO_CHANGE                  0

/* Define input current and charging current in dock in statement */
#define ECCHG_DOCKIN_IN_CURR                    1152
#define ECCHG_DOCKIN_CHG_CURR                   1152
/* BEGIN [Terry_Tzeng][2012/04/17][Modify input current current default value for PVT and DVT] */
#define ECCHG_USBIN_CHG_CURR_DVT                384
#define ECCHG_USBIN_CHG_CURR_PVT                1152
/* BEGIN [Terry_Tzeng][2012/04/11][Adjust input current for changed charging archtecture] */
#define ECCHG_USBIN_IN_CURR_DVT                 384
#define ECCHG_USBIN_IN_CURR_PVT                 1152
/* END [Terry_Tzeng][2012/04/17][Modify input current current default value for PVT and DVT] */
/* END [Terry_Tzeng][2012/04/11][Adjust input current for changed charging archtecture] */
/* BEGIN [Terry_Tzeng][2012/04/23][Define input current for dock low battery capacity] */
#define ECCHG_USBIN_CHG_CURR_DK_BATT_LOW_PVT    128
#define ECCHG_USBIN_IN_CURR_DK_BATT_LOW_PVT     128
/* END [Terry_Tzeng][2012/04/23][Define input current for dock low battery capacity] */
/* BEGIN [Terry_Tzeng][2012/03/16][Check entried bypass mode case] */
//#if PEGA_SMT_SOLUTION
#define ECCHG_BYPASS_IN_CURR                    128
//#endif /* PEGA_SMT_SOLUTION */
/* END [Terry_Tzeng][2012/03/16][Check entried bypass mode case] */


/* BEGIN [Terry_Tzeng][2012/03/07][Define input current and charging current for inserted ac 5v in PVT or DVT version ] */
#define ECCHG_AC5V_CHG_CURR_PVT                 1792
#define ECCHG_AC5V_IN_CURR_PVT                  1152
#define ECCHG_AC5V_CHG_CURR_DVT                 1152
#define ECCHG_AC5V_IN_CURR_DVT                  1152
/* BEGIN [Terry_Tzeng][2012/06/11][Handle informal charger type] */
#define ECCHG_INFORMAL_CHG_1024_DROP_CURR       512  //(1024(Dock input current)*5(Informal adapter voltage)*0.9(Dock charger ic loss power)*0.85(Dock 6V system boost loss power)) / 6(Dock system boost voltage)
#define ECCHG_INFORMAL_CHG_640_DROP_CURR        384  //(640(Dock input current)*5(Informal adapter voltage)*0.9(Dock charger ic loss power)*0.85(Dock 6V system boost loss power)) / 6(Dock system boost voltage)
#define ECCHG_INFORMAL_CHG_512_DROP_CURR        256  //(512(Dock input current)*5(Informal adapter voltage)*0.9(Dock charger ic loss power)*0.85(Dock 6V system boost loss power)) / 6(Dock system boost voltage)
#define ECCHG_INFORMAL_CHG_384_DROP_CURR        256  //(384(Dock input current)*5(Informal adapter voltage)*0.9(Dock charger ic loss power)*0.9(Dock 6V system boost loss power)) / 6(Dock system boost voltage)
#define ECCHG_INFORMAL_AC_CHG_CURR_DK_BATT_LOW  256
#define ECCHG_INFORMAL_AC_IN_CURR_DK_BATT_LOW   256
/* END [Terry_Tzeng][2012/06/11][Handle informal charger type] */
/* END [Terry_Tzeng][2012/03/07][Define input current and charging current for inserted ac 5v in PVT or DVT version ] */
/* BEGIN [Terry_Tzeng][2012/04/23][Define input current for dock low battery capacity] */
#define ECCHG_AC5V_CHG_CURR_DK_BATT_LOW_PVT     1152
#define ECCHG_AC5V_IN_CURR_DK_BATT_LOW_PVT      1152
/* END [Terry_Tzeng][2012/04/23][Define input current for dock low battery capacity] */

#define ECCHG_AC12_CHG_CURR                     3840
#define ECCHG_AC12_IN_CURR                      1536

#define WAKEUP_TIME                             150  // unit sec
#define BATT_FULL_WAKEUP_TIME                   1800

/* BEGIN [Jackal_Chen][2012/04/05][Non-formal charger support] */
/* BEGIN [Jackal_Chen][2012/04/17][Remove non-formal charger support from SMT version] */
/* Set to 1 to enable non-formal charger support */
#if (PEGA_SMT_SOLUTION == 1)
#define PEGA_CHG_SUPPORT_NON_FORMAL_CHARGER    0   // SMT version, not support non-formal charger
#else
#define PEGA_CHG_SUPPORT_NON_FORMAL_CHARGER    1  // Developer define
#endif

/* BEGIN [Jackal_Chen][2012/04/23][More non-formal charger type support] */
#if (PEGA_CHG_SUPPORT_NON_FORMAL_CHARGER == 1)
/* Set to 1 to enable more type of non-formal charger support(1500mA/500mA) */
#define PEGA_CHG_SUPPORT_MORE_CHARGER_TYPE        0  // Developer define
#else
#define PEGA_CHG_SUPPORT_MORE_CHARGER_TYPE        0  // Not support
#endif
/* END [Jackal_Chen][2012/04/23][More non-formal charger type support] */

/* Set to 1 to enable non-formal charger logs */
#if (PEGA_CHG_SUPPORT_NON_FORMAL_CHARGER == 0)
#define PEGA_CHG_NON_FORMAL_SUPPORT_DBG_FLAG    0  // Not support non-formal charger, no need dbg logs
#else
#define PEGA_CHG_NON_FORMAL_SUPPORT_DBG_FLAG    0  // Support non-formal charger, developer define
#endif
/* END [Jackal_Chen][2012/04/17][Remove non-formal charger support from SMT version] */

/* BEGIN [Jackal_Chen][2012/04/18][Modify logic to get usb_in_valid status] */
/* BEGIN [Jackal_Chen][2012/04/23][More non-formal charger type support] */
#if (PEGA_CHG_SUPPORT_MORE_CHARGER_TYPE == 1)
#define PEGA_CHG_WALL_RETRY_COUNT_MAX                      1
#else
#define PEGA_CHG_WALL_RETRY_COUNT_MAX                      2
#endif
/* END [Jackal_Chen][2012/04/23][More non-formal charger type support] */
/* BEGIN [Jackal_Chen][2012/04/27][Since some non-formal charger drop usb_in_valid pin too fast, add check time] */
#define PEGA_CHG_USB_IN_RETRY_COUNT_MAX                  8
/* END [Jackal_Chen][2012/04/27][Since some non-formal charger drop usb_in_valid pin too fast, add check time] */
/* END [Jackal_Chen][2012/04/18][Modify logic to get usb_in_valid status] */
#define PEGA_WALL_CHG_TYPE_1A_INPUT_CURR           1024
#define PEGA_WALL_CHG_TYPE_700MA_INPUT_CURR    640
/* BEGIN [Jackal_Chen][2012/04/23][More non-formal charger type support] */
#if (PEGA_CHG_SUPPORT_MORE_CHARGER_TYPE == 1)
#define PEGA_WALL_CHG_TYPE_1500MA_INPUT_CURR    1536
#define PEGA_WALL_CHG_TYPE_500MA_INPUT_CURR         512
#endif
/* END [Jackal_Chen][2012/04/23][More non-formal charger type support] */

#if(PEGA_CHG_NON_FORMAL_SUPPORT_DBG_FLAG == 1)
	#define PEGA_DBG_NON_FORMAL_SUPPORT(x...)	printk(x)
#else
	#define PEGA_DBG_NON_FORMAL_SUPPORT(x...)	do { } while (0)
#endif
/* END [Jackal_Chen][2012/04/05][Non-formal charger support] */

/* BEGIN [Jackal_Chen][2012/04/10][Detect Y-cable by adc] */
#define PEGA_CHG_DEVICE_ID_MPP_MUNBER                      12
/* END [Jackal_Chen][2012/04/10][Detect Y-cable by adc] */

// BEGIN [2012/07/12] Jackal - Modify to fix AC charger unstable bug
#define MSM_CHARGER_ENABLE_OTG_CHECK  1  // if enable otg check, set this flag to 1, otherwise 0.
#define MSM_CHARGER_OTG_CHECK_MAX  3    // max times to check otg. If you want infinite check until otg report back, set this value to -1.
#define NOTIFY_OTG_CHECK_EVENT  2  // because plugin value is 0 or 1 in notify_usb_of_the_plugin_event(), we give another value for otg check.
// END [2012/07/12] Jackal - Modify to fix AC charger unstable bug

/* BEGIN  */
#endif /* MSM_CHARGER_ENABLE_DOCK */

/* BEGIN [Tim PH][2012/04/10][Enable log dynamically, migrate to msm-charger-log.h] */
#if 0 //
#ifdef CONFIG_PEGA_CHG_DBG

#ifdef PEGA_SMT_BUILD
#define PEGA_CHG_DBG_LEVEL	2   // for SMT release
#else
#define PEGA_CHG_DBG_LEVEL	1   // for formal release
#endif

#if (PEGA_CHG_DBG_LEVEL == 1)
	#define PEGA_DBG_H(x...)	printk(x)
	#define PEGA_DBG_M(x...)	do { } while (0)
	#define PEGA_DBG_L(x...)	do { } while (0)
#elif (PEGA_CHG_DBG_LEVEL == 2)
	#define PEGA_DBG_H(x...)	printk(x)
	#define PEGA_DBG_M(x...)	printk(x)
	#define PEGA_DBG_L(x...)	do { } while (0)
#elif (PEGA_CHG_DBG_LEVEL == 3)
	#define PEGA_DBG_H(x...)	printk(x)
	#define PEGA_DBG_M(x...)	printk(x)
	#define PEGA_DBG_L(x...)	printk(x)
#else
	#define PEGA_DBG_H(x...)	do { } while (0)
	#define PEGA_DBG_M(x...)	do { } while (0)
	#define PEGA_DBG_L(x...)	do { } while (0)
#endif

#else
	#define PEGA_DBG_H(x...)	do { } while (0)
	#define PEGA_DBG_M(x...)	do { } while (0)
	#define PEGA_DBG_L(x...)	do { } while (0)
#endif
#endif //0
/* END [Tim PH][2012/04/10][Enable log dynamically, migrate to msm-charger-log.h] */

#define PEGA_CHG_FAKE_BATT_ENABLE


enum {
	CHG_TYPE_USB,
	CHG_TYPE_AC,
#ifdef MSM_CHARGER_ENABLE_DOCK
	CHG_TYPE_DOCK
#endif /* MSM_CHARGER_ENABLE_DOCK */
};


enum chg_type {
	USB_CHG_TYPE__SDP,
	USB_CHG_TYPE__CARKIT,
	USB_CHG_TYPE__WALLCHARGER,
	USB_CHG_TYPE__INVALID
};

/* BEGIN [Jackal_Chen][2012/04/05][Non-formal charger support] */
/* BEGIN [Jackal_Chen][2012/04/23][More non-formal charger type support] */
#if (PEGA_CHG_SUPPORT_NON_FORMAL_CHARGER == 1)
enum pega_wall_chg_type {
	WALL_CHG_TYPE_OTHER,
	WALL_CHG_TYPE_2A,
	WALL_CHG_TYPE_1A,
	WALL_CHG_TYPE_700MA,
#if (PEGA_CHG_SUPPORT_MORE_CHARGER_TYPE == 1)
	WALL_CHG_TYPE_1500MA,
	WALL_CHG_TYPE_500MA,
#endif
};
#endif
/* END [Jackal_Chen][2012/04/23][More non-formal charger type support] */
/* END [Jackal_Chen][2012/04/05][Non-formal charger support] */

/* BEGIN [Terry_Tzeng][2012/02/08][Add charging event is CHG_DOCK_HEARTBEAT_CHECK in updated heartbeat statement] */
enum msm_hardware_charger_event {
	CHG_INSERTED_EVENT,
	CHG_ENUMERATED_EVENT,
	CHG_REMOVED_EVENT,
	CHG_DONE_EVENT,
	CHG_BATT_BEGIN_FAST_CHARGING,
	CHG_BATT_CHG_RESUME,
	CHG_BATT_TEMP_OUTOFRANGE,
	CHG_BATT_TEMP_INRANGE,
	CHG_BATT_INSERTED,
	CHG_BATT_REMOVED,
	CHG_BATT_STATUS_CHANGE,
	CHG_BATT_NEEDS_RECHARGING,
#ifdef MSM_CHARGER_ENABLE_DOCK
	CHG_DOCK_INSERTED_EVENT,
	CHG_DOCK_REMOVED_EVENT,
	CHG_DOCK_STATUS_CHANGE,
	CHG_DOCK_HEARTBEAT_CHECK,
	CHG_DOCK_RESTART_CHG_EVENT,
	CHG_DOCK_RESTOP_CHG_EVENT,
	CHG_DOCK_SET_PERIOD_CHG_CURR_EVENT,
	CHG_DOCK_OUT_FINISH_EVENT,
#endif /* MSM_CHARGER_ENABLE_DOCK */
#if PEGA_SMT_SOLUTION
	CHG_DOCK_SMT_DK_CHG_TO_TB,
	CHG_DOCK_SMT_AC_CHG_TO_DK,
#endif /* PEGA_SMT_SOLUTION */
};
/* END [Terry_Tzeng][2012/02/08][Add charging event is CHG_DOCK_HEARTBEAT_CHECK in updated heartbeat statement] */

/**
 * enum hardware_charger_state
 * @CHG_ABSENT_STATE: charger cable is unplugged
 * @CHG_PRESENT_STATE: charger cable is plugged but charge current isnt drawn
 * @CHG_READY_STATE: charger cable is plugged and kernel knows how much current
 *			it can draw
 * @CHG_CHARGING_STATE: charger cable is plugged and current is drawn for
 *			charging
 */
enum msm_hardware_charger_state {
	CHG_ABSENT_STATE,
	CHG_PRESENT_STATE,
	CHG_READY_STATE,
	CHG_CHARGING_STATE,
};

struct msm_hardware_charger {
	int type;
	int rating;
	const char *name;
	int (*start_charging) (struct msm_hardware_charger *hw_chg,
			       int chg_voltage, int chg_current);
	int (*stop_charging) (struct msm_hardware_charger *hw_chg);
	int (*charging_switched) (struct msm_hardware_charger *hw_chg);
	void (*start_system_current) (struct msm_hardware_charger *hw_chg,
							int chg_current);
	void (*stop_system_current) (struct msm_hardware_charger *hw_chg);
	int (*get_chg_current) (struct msm_hardware_charger *hw_chg);
	int (*get_in_current) (struct msm_hardware_charger *hw_chg);

/* BEGIN [Terry_Tzeng][2012/03/21][Set periodically charging current callback function in suspend status] */
#ifdef MSM_CHARGER_ENABLE_DOCK
	void (*set_period_chg_curr_for_dk)(struct msm_hardware_charger *hw_chg);
#endif /* MSM_CHARGER_ENABLE_DOCK */
/* END [Terry_Tzeng][2012/03/21][Set periodically charging current callback function in suspend status] */
	void *charger_private;	/* used by the msm_charger.c */
};


// BEGIN-20111214-KenLee-remove from pega_msm_charger.c
struct msm_hardware_charger_priv {
	struct list_head list;
	struct msm_hardware_charger *hw_chg;
	enum msm_hardware_charger_state hw_chg_state;
	unsigned int max_source_current;
// BEGIN-20111214-KenLee-add "usb" psy for user space
	struct power_supply psy_ac;
	struct power_supply psy_usb;
// END-20111214-KenLee-add "usb" psy for user space
/* BEGIN [Jackal_Chen][2012/01/16][Add "usb_wall_chg" psy for user space] */
	struct power_supply psy_usb_wall_chg;
/* END [Jackal_Chen][2012/01/16][Add "usb_wall_chg" psy for user space] */
// BEGIN-20111214-KenLee-modify wake lock trigger point
	struct wake_lock wl;
// END-20111214-KenLee-modify wake lock trigger point
// BEGIN-20111214-KenLee-add usb charge type
	enum chg_type usb_chg_type;
// END-20111214-KenLee-add usb charge type
};
// END-20111214-KenLee-remove from pega_msm_charger.c

struct msm_battery_gauge {
	int (*get_battery_mvolts) (void);
	int (*get_battery_temperature) (void);
	int (*is_battery_present) (void);
	int (*is_battery_temp_within_range) (void);
	int (*is_battery_id_valid) (void);
	int (*is_battery_gauge_ready) (void);
	int (*get_battery_status) (void);
	int (*get_batt_remaining_capacity) (void);
	int (*get_battery_health) (void);
	int (*get_battery_current) (void);
	int (*monitor_for_recharging) (void);
	char* (*get_battery_fw_ver) (void);
	int (*get_battery_level) (void);
	int (*get_battery_fcc) (void);
	int (*get_battery_rm) (void);
	int (*get_charge_type) (void);
	int (*get_charge_current) (void);
	int (*get_input_current) (void);
	void (*set_batt_suspend) (void);
};

/* BEGIN [Terry_Tzeng][2012/03/28][Set notification funciotn callback to handle suspend and resume statement in inserted dock] */
/* BEGIN [Terry_Tzeng][2012/01/11][Create dock in process callback function] */
#ifdef MSM_CHARGER_ENABLE_DOCK
struct msm_ec_dock {
	int (*is_tb_batt_change_cb) (struct msm_hardware_charger_priv * hw_chg_priv ,int evt);
	void (* tb_early_suspend_notify) (void);
	void (* tb_late_resume_notify) (void);
	int (*get_safety_time) (void);
};
#endif /* MSM_CHARGER_ENABLE_DOCK */
/* END [Terry_Tzeng][2012/01/11][Create dock in process callback function] */
/* END [Terry_Tzeng][2012/03/28][Set notification funciotn callback to handle suspend and resume statement in inserted dock] */

/**
 * struct msm_charger_platform_data
 * @safety_time: max charging time in minutes
 * @update_time: how often the userland be updated of the charging progress
 * @max_voltage: the max voltage the battery should be charged upto
 * @min_voltage: the voltage where charging method switches from trickle to fast
 * @get_batt_capacity_percent: a board specific function to return battery
 *			capacity. Can be null - a default one will be used
 */
/* BEGIN-KenLee-20111214-add for pega charging */
#ifdef CONFIG_PEGA_ISL_CHG
struct msm_charger_platform_data {
	unsigned int safety_time;
	/* BEGIN [Jackal_Chen][2012/04/23][Modify safety time of wall charger] */
	unsigned int wall_safety_time;
	/* END [Jackal_Chen][2012/04/23][Modify safety time of wall charger] */
	unsigned int usb_safety_time;
	unsigned int update_time;
	unsigned int max_voltage;
	unsigned int min_voltage;
	unsigned int (*get_batt_capacity_percent) (void);
};
#else
struct msm_charger_platform_data {
	unsigned int safety_time;
	unsigned int update_time;
	unsigned int max_voltage;
	unsigned int min_voltage;
	unsigned int (*get_batt_capacity_percent) (void);
};
#endif
/* END-KenLee-20111214-add for pega charging */

typedef void (*notify_vbus_state) (int);

#ifdef MSM_CHARGER_ENABLE_DOCK
int msm_dock_ec_init(struct msm_hardware_charger *hw_chg);
void msm_dock_ec_exit(void);
void msm_dock_ec_register(struct msm_ec_dock *dock_gauge);
void msm_dock_ec_unregister(struct msm_ec_dock *dock_gauge);
/* BEGIN [Terry_Tzeng][2012/03/12][Define checked dock battery full status] */
int msm_chg_check_ecbatt_full(void);
/* END [Terry_Tzeng][2012/03/12][Define checked dock battery full status] */
int pega_ecbatt_init(void);
void pega_ecbatt_exit(void);
void pega_ecbatt_gauge_register(struct msm_battery_gauge *ecbatt_gauge);
void pega_ecbatt_gauge_unregister(struct msm_battery_gauge *ecbatt_gauge);
void pega_ecbatt_dock_in(void);
void pega_ecbatt_dock_out(void);
#endif /* MSM_CHARGER_ENABLE_DOCK */

/* BEGIN [Jackal_Chen][2012/04/10][Detect Y-cable by adc] */
int pega_get_mpp_adc(int mpp_number, int64_t *val);
/* END [Jackal_Chen][2012/04/10][Detect Y-cable by adc] */

#if defined(CONFIG_BATTERY_MSM8X60) || defined(CONFIG_BATTERY_MSM8X60_MODULE)
void msm_battery_gauge_register(struct msm_battery_gauge *batt_gauge);
void msm_battery_gauge_unregister(struct msm_battery_gauge *batt_gauge);
int msm_charger_register(struct msm_hardware_charger *hw_chg);
int msm_charger_unregister(struct msm_hardware_charger *hw_chg);
int msm_charger_notify_event(struct msm_hardware_charger *hw_chg,
			     enum msm_hardware_charger_event event);
void msm_charger_vbus_draw(unsigned int mA);

int msm_charger_register_vbus_sn(void (*callback)(int));
void msm_charger_unregister_vbus_sn(void (*callback)(int));

void pega_power_enter_cs(void);
void pega_power_exit_cs(void);

/* BEGIN-KenLee-20111214-add for pega charging */
#ifdef CONFIG_PEGA_ISL_CHG
int msm_chg_get_batt_status(void);
#endif
/* END-KenLee-20111214-add for pega charging */

/* BEGIN [2012/04/25] Jackal - User may charge device when battery is full but charge current not less than 300mA yet */
int msm_chg_get_batt_current(void);
/* END [2012/04/25] Jackal - User may charge device when battery is full but charge current not less than 300mA yet */

/* BEGIN [Terry_Tzeng][2012/01/16][Create getting tablet battery information] */
#ifdef MSM_CHARGER_ENABLE_DOCK
int msm_chg_get_batt_temperture(void);
int msm_chg_get_batt_mvolts(void);
int msm_chg_get_batt_remain_capacity(void);
int msm_chg_get_ui_batt_capacity(void);
/* BEGIN [Terry_Tzeng][2012/04/27][Create checked pad battery ready function] */
int msm_chg_check_padbatt_ready(void);
int msm_chg_check_padbatt_chg_st(void);
/* END [Terry_Tzeng][2012/04/27][Create checked pad battery ready function] */


/* BEGIN [Terry_Tzeng][2012/06/11][Handle informal charger type] */
int msm_chg_get_ecbatt_in_curr(void);
/* END [Terry_Tzeng][2012/06/11][Handle informal charger type] */
int msm_chg_check_ecbatt_ready(void);
int msm_chg_get_ui_ecbatt_capacity(void);
int msm_chg_get_ecbatt_temperature(void);
int msm_chg_get_ecbatt_mvolt(void);

/* BEGIN [Terry_Tzeng][2012/02/15][Declare controled regulator 42 interface for dock in statement] */
int msm_chg_check_5vboost_pw_is_enabled(void);
int msm_chg_5vboost_pw_enable(void);
int msm_chg_5vboost_pw_disable(void);
/* END [Terry_Tzeng][2012/02/15][Declare controled regulator 42 interface for dock in statement] */

int echger_get_tb_in_chg_curr(int * gtb_in_curr ,int * gtb_chg_curr);
void echger_notify_tb_start_chg_finish(int tb_in_curr, int tb_chg_curr);
/* BEGIN [Terry_Tzeng][2012/04/17][Define function of get dock hardward version] */
int echger_get_dk_hw_id(void);
/* END [Terry_Tzeng][2012/04/17][Define function of get dock hardward version] */
/* BEGIN [Terry_Tzeng][2012/05/03][Define function of setting suspend status flag] */
void echger_enable_suspend_flag(void);
void echger_disable_suspend_flag(void);
void echger_suspend_charging_control(void);
void echger_wake_unlock_ext(void);
/* BEGIN [Terry_Tzeng][2012/05/04][Define getting retry count for setting ec wake lock function] */
int  echger_get_retry_flag(int chk_reval);
/* END [Terry_Tzeng][2012/05/03][Define function of setting suspend status flag] */
int echger_determine_ec_wakeup_timer_en(void);

#endif/* MSM_CHARGER_ENABLE_DOCK */
/* END [Terry_Tzeng][2012/01/16][Create getting tablet battery information] */

#else
static inline void msm_battery_gauge_register(struct msm_battery_gauge *gauge)
{
}
static inline void msm_battery_gauge_unregister(struct msm_battery_gauge *gauge)
{
}
static inline int msm_charger_register(struct msm_hardware_charger *hw_chg)
{
	return -ENXIO;
}
static inline int msm_charger_unregister(struct msm_hardware_charger *hw_chg)
{
	return -ENXIO;
}
static inline int msm_charger_notify_event(struct msm_hardware_charger *hw_chg,
			     enum msm_hardware_charger_event event)
{
	return -ENXIO;
}
static inline void msm_charger_vbus_draw(unsigned int mA)
{
}
static inline int msm_charger_register_vbus_sn(void (*callback)(int))
{
	return -ENXIO;
}
static inline void msm_charger_unregister_vbus_sn(void (*callback)(int))
{
}
#endif
#endif /* __MSM_CHARGER_H__ */
