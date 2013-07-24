#ifndef NUVEC_API_H
#define NUVEC_API_H
/*EC event*/
/*EC interrupt event*/
#define TP_ERR 0
#define BAT_ERR 1
//---
#define AC_5V_IN    2
#define AC_12V_IN	3
#define USB_5V_IN   4
#define AC_OUT  5
#define INFORMAL_CHG 23
#define INFORMAL_AC_IN 24
//---
#define SD_INSERT  6
#define SD_REMOVE   7
//---
#define BAT_CAP_5   8
#define BAT_CAP_10  9
#define BAT_CAP_15  10
#define BAT_CAP_20  11
#define BAT_CAP_FULL    12
//---
#define BAT_LOW    13
#define CRISIS_BAT_LOW	14
//---
#define BAT_TEMP45    17
//---
#define UNKNOWN  -1
//--wakeup_by_dock
#define DOCK_WKUP 21
#define DOCK_KBD_WKUP 22

#define DO_ECRST 0xe
#define EC_RESET 0xf
#define DOCK_I2C_ABORT 30

/*boost_ctl*/
#define	SYS_5V		0
#define	OTG_5V		1
#define	BOOST_OFF	0
#define	BOOST_ON	1


/*ec_batt_info*/
#define AT_RATE				0x02 /*AtRate*/
#define AT_RATE_TTE 		0x04 /*AtRateTimeToEmpty*/
#define DOCK_BAT_TEMP		0x06 /*TEMP*/
#define DOCK_BAT_VOLT		0x08 /*VOLT*/
#define DOCK_BAT_FLAGS		0x0a /*FLAGS*/
#define NOMINAL_AVAIL_CAP	0x0c /*NominalAvailableCap*/
#define FULL_AVAIL_CAP		0x0e /*FullAvailableCap*/
#define REMANING_CAP		0x10 /*RemaniningCap*/
#define FULL_CHG_CAP		0x12 /*FullChargeCap*/
#define AVG_CURR			0x14 /*AverageCurrent*/
#define TTE					0x16 /*TimeToEmpty*/
#define TTF					0x18 /*TimeToFull*/
#define STANDBY_CURR		0x1a /*StandbyCurrent*/
#define STANDBY_TTE			0x1c /*StandbyTimeToEmpty*/
#define MAX_LD_CURR		0x1e /*MaxLoadCurrent*/
#define MAX_LD_TTE			0x20 /*MaxLoadTimeToEmpty*/
#define AVAIL_ENG			0x22 /*AvailableEnergy*/
#define AVG_POWER			0x24 /*AveragePower*/
#define TTE_CONST_POWER	0x26 /*TTEatConstantPower*/
#define CYCLE_COUNT			0x2a /*CycleCount*/
#define REL_STATE_OF_CHG	0x2c /*RelativeStateOfCharge*/
#define DOCK_BAT_CHG_VOLT	0x30 /*ChargeVoltage*/
#define DOCK_BAT_CHG_CURR	0x32 /*ChargeCurrent*/
#define DEV_ID				0xc2 /*Device type id*/
#define FW_ID				0xc3 /*FW version id*/
#define HW_ID				0xc4 /*HW version id*/
#define DF_ID				0xc5 /*DF version id*/

/*ec_charging_request*/
#define	CHARGING_OFF	0
#define CHARGING_ON	1

/*AC data define*/
#define AC_DATA_INFORMAL		0xe
#define AC_DATA_5V				0x6
#define AC_DATA_USB			0x2
#define AC_DATA_NONE			0x3
#define AC_DATA_12V			0x0

/*AC type*/
#define	AC_NONE	AC_OUT
#define	AC_5V		AC_5V_IN
#define	AC_12V		AC_12V_IN
#define	AC_USB		USB_5V_IN
#define	AC_INFORMAL	INFORMAL_CHG

/*usb bypass en*/
#define 	usb_bypass_high	1
#define	usb_bypass_low		0

/*dock status*/
#define	DOCK_READY	(0)
#define	TOUCHPAD_ERR	(1<<0)
#define	BATTERY_ERR	(1<<1)
#define	PADIN_ERR		(1<<2)
#define	DOCK_I2C_FAIL	(1<<6)
#define	NO_DOCK		(1<<7)

/*ec_charging_setting level*/
#define	EC_DEFCURR_12V	EC_CURR_1536	//default current setting for 12V adapt.
#define	EC_DEFCURR_5V		EC_CURR_2048	//default current setting for 5V adapt.
#define	EC_DEFCURR_USB	EC_CURR_384	//default current setting for usb adapt.

#define	EC_CURR_3840	0xf00	//3840 mA
#define	EC_CURR_2048	0x800	//2048 mA
#define	EC_CURR_1920	0X780	//1920 mA
#define EC_CURR_1536	0x5FE	//1536 mA
#define	EC_CURR_1280	0x500	//1280 mA
#define	EC_CURR_1024	0x400	//1024 mA
#define   EC_CURR_640	0x280	//640 mA
#define	EC_CURR_512	0x200	//512 mA
#define	EC_CURR_384	0x180	//384 mA
#define	EC_CURR_128	0x80	//128 mA
#define	EC_CURR_0		0x0		//0mA
#define	INVALID_VALUE	-100

/*ec_charging_info : function for getting info from chager*/
/*I2C Commands*/
#define	CHG_CONTROL			0x55
#define	CHG_MANUFACTURE_ID	0x56
#define	CHG_DEVICE_ID			0x57
#define	CHG_CHG_CURR			0x50
#define	CHG_MAX_SYS_VOLT		0x52
#define	CHG_MIN_SYS_VOLT		0x53
#define	CHG_INPUT_CURR		0x54

/*Function status return value*/
#define SUCCESS	0
#define FAIL	-1

/*The Mask definition*/
#define KBD_MASK	0xff
#define TP_MASK		0xff << 8
#define TP_GPIO_MASK	1 << 7
#define SD_CARD_GPIO_MASK	1 << 5
#define SD_CARD_RESET_MASK 1 << 7
#define USB_HUB_GPIO_MASK 1 << 1
#define USB_HUB_RESET_MASK 1 << 5
#define WAKEUP_FLAG_MASK	7

/*Dock Board Version*/
#define	DOCK_DVT	1
#define	DOCK_PVT	2
#define 	DOCK_SIT2	3
#define	DOCK_SIT3	4
#define	DOCK_SIT4	5
#define	DOCK_UNKNOWN	100

/*pad_control_led*/
#define		EC_CONTROL_DUAL_LED_ON	2
#define        	PAD_CONTROL_LED_ON      1
#define        	PAD_CONTROL_LED_OFF     0

#define		SET_LED_ON		1
#define		SET_LED_OFF	0

/*TP_switch*/
#define	TP_SWITCH_ON 	1
#define	TP_SWITCH_OFF	0
/*SD_card_switch*/
#define	SD_CARD_ON	1
#define	SD_CARD_OFF	0
/*USB_hub_switch*/
#define	USB_HUB_ON	1
#define	USB_HUB_OFF	0
/*wakeup_flag*/
#define	WAKEUP_ENABLE		1
#define	WAKEUP_DISABLE	0

/*bat bulk structure*/
struct bat_data{
	int voltage;
	int relative_state_of_charge;
	int tempture;
	int flag;
	int remain_capacity;
	int full_charge_capacity;
	int average_current;

};

/*Functions*/
/*EC->Pad interrupt event handler*/
void register_ec_event_func(void (* pFn)(int));



#if defined(PEGA_SMT_BUILD)
#define EC_BUS_DVT 10
#define EC_BUS_PVT 9

int ec_smt_interface_bus_register(int bus);
#endif

/*Pad->EC*/
/*control the EC boosts(otg_5V and sys_5V)*/
int ec_boost_ctl(int boost, int on_off);
/*Get the boost status*/
int get_boost_status(int *otg_status,int *sys_status);
/*get the EC battery info from EC*/
int get_ec_bat_info(int *info, int cmd);
/*regquest the ec charging*/
int ec_charging_req(int on_off);
/*Get the ec charging req status*/
int get_ec_charging_req_status(int *charging_req_status);
/*Get the Adaptor type from EC*/
int get_ac_type(int *type);
/*Send the number of usb in use to EC*/
int usb_in_used(int num);
/*Get the number of usb in use stored in EC*/
int get_usb_in_used(int *count);
/*Set the usb_bypass_en*/
int set_usb_bypass_en(int high_low);
/*Get the usb_bypass_en status*/
int get_usb_bypass_en(int *usb_bypass_status);
/*USB Hub reset*/
int usb_hub_reset(int time);
/*Dock Status*/
int dock_status(int *status);
/*ec_charging_setting*/
//int ec_charging_setting(int level);
int ec_charging_setting(int input_current, int chg_current);
/*ec_get_adc_id : get Dock Board ID*/
int ec_get_adc_id(int *id);
int ec_dock_ver(int *ver);
/*ec_get_charging_info : get charger info*/
int get_ec_charging_info(int *info, int cmd);
/*Notifier for OTG Power Source Change*/
/*This Function Must be call when Pad_OTG_power_on or Dock_OTG_boost_on*/
void otg_power_switch_notifier(void);
//13 byte ISN data read/write
//int dock_isn_read(u8 *isn);
char dock_isn_read(char *isn);
int dock_isn_write(char *isn);
// 4 byte dock data read/write
int dock_data_read(int *dock_data);
int dock_data_write(int dock_data);
// 4 byte pad config data read/wite
int dock_pad_config_read(int* pad_config);
int dock_pad_config_write(int pad_config);
//dock reset
int dock_reset_func(void);
/*TP_switch*/
int touchpad_switch(int on_off);
/*SD_card_switch*/
int sdcard_switch(int on_off);
/*USB_hub_switch*/
int usb_hub_switch(int on_off);
/*wakeup_flag*/
int wakeup_flag(int enable);
/*set wakeup_flag*/
int set_wakeup_flag(int data);
/*wakeup_sleep_interval*/
 int wakeup_sleep_interval(int time);
/*bulk_battery_data*/
int get_ec_battery_data(struct bat_data *ec_bat );
/*get wakeup_flag*/
int get_wakeup_flag(int *flag);
/*get_wakeup_interval_time*/
int get_wakeup_sleep_interval(int *time);
//pad control ld
int pad_control_led(int on_off);
#if 0
//enable/disable sdcard plug and unplug as pad wakeupsource
int set_sdcard_wakeup_src(int enable);
#endif
//low batt capacity setting
int set_low_bat_cap(int low_lv,int critical_low_lv);
#if 0
//Dock charging timeout time setting
int set_dock_chg_timeout(int time);
//get dock SD card status
int get_dock_SD_card_status(int *status);
#endif
int set_dual_orange_led(int on_off);
int set_dual_blue_led(int on_off);

char dock_ssn_read(char *ssn);
int dock_ssn_write(char *ssn);

 int battery_manufacture_info_read(u8 manu_info[]);


#endif
