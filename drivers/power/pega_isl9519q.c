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

#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/workqueue.h>
#include <linux/interrupt.h>
#include <linux/msm-charger.h>
#include <linux/slab.h>
#include <linux/i2c/isl9519.h>
#include <linux/mfd/pm8xxx/pm8921.h>
/* BEGIN [Jackal_Chen][2012/04/09][Use HW_ID value to distinguish GPIO number of dock_in pin] */
#include <asm/setup.h>
/* END [Jackal_Chen][2012/04/09][Use HW_ID value to distinguish GPIO number of dock_in pin] */
/* BEGIN [Terry_Tzeng][2012/04/17][Include dock charger type defined marco] */
#include <linux/nuvec_api.h>
/* END [Terry_Tzeng][2012/04/17][Include dock charger type defined marco] */

/* BEGIN [Tim_PH][2012/05/03][For dynamically log mechanism] */
#include <linux/msm-charger-log.h>
/* END [Tim_PH][2012/05/03][For dynamically log mechanism] */


#define CHG_CURRENT_REG		0x14
#define MAX_SYS_VOLTAGE_REG	0x15
#define CONTROL_REG		0x3D
#define MIN_SYS_VOLTAGE_REG	0x3E
#define INPUT_CURRENT_REG	0x3F
#define MANUFACTURER_ID_REG	0xFE
#define DEVICE_ID_REG		0xFF

#define TRCKL_CHG_STATUS_BIT	0x80

#define ISL9519_CHG_PERIOD	((HZ) * 150)

#ifdef CONFIG_PEGA_ISL_CHG
#define OTG_REFIRE_INTERVAL	((HZ)*1)
#define OTG_RETRY_COUNT		10
static int charge_current = 0;
static int input_current = 0;
#endif

struct isl9519q_struct {
	struct i2c_client       *client;
	struct delayed_work     charge_work;
	int                     present;
	int                     batt_present;
	bool                    charging;
	int                     usb_current;
	int                     usb_input_current;
	int                     usbcgr_current;
	int                     usbcgr_input_current;
	int                     chgcurrent;
	int                     input_current;
	/* BEGIN [Terry_Tzeng][2012/01/18][Add docking input current and charging current member] */
	int 			dock_chg_current;
	int 			dock_input_current;
	/* END [Terry_Tzeng][2012/01/18][Add docking input current and charging current member] */
	int                     term_current;
	int                     max_system_voltage;
	int                     min_system_voltage;
	int                     valid_n_gpio;
	struct dentry           *dent;
	struct msm_hardware_charger	adapter_hw_chg;

#if (PEGA_INT_HANDLER_SOLUTION == 1)
	struct delayed_work     check_cable_detect_work;
	/* BEGIN [Jackal_Chen][2012/01/16][Add device_id pin config/handler] */
	struct delayed_work     check_device_id_work;
	/* END [Jackal_Chen][2012/01/16][Add device_id pin config/handler] */
#endif
	/* BEGIN [Jackal_Chen][2012/01/11][Add delay work to handle docking interrupt handler in tophalf statement] */
	struct delayed_work docking_interrupt;
	/* END [Jackal_Chen][2012/01/11][Add delay work to handle docking interrupt handler in tophalf statement] */
};

static int prev_volt_idx = -1;

/* BEGIN [Jackal_Chen][2012/04/09][Use HW_ID value to distinguish GPIO number of dock_in pin] */
static int dock_in_gpio_number = MSM8960_DOCK_IN_GPIO;    //  Default = 7(for EVT/DVT/PVT)
/* END [Jackal_Chen][2012/04/09][Use HW_ID value to distinguish GPIO number of dock_in pin] */

/* BEGIN [Jackal_Chen][2012/04/05][Non-formal charger support] */
#if (PEGA_CHG_SUPPORT_NON_FORMAL_CHARGER == 1)
static enum pega_wall_chg_type wall_chg_type = WALL_CHG_TYPE_OTHER;
static int wall_chg_retry_count = PEGA_CHG_WALL_RETRY_COUNT_MAX;
#endif
/* END [Jackal_Chen][2012/04/05][Non-formal charger support] */

/* BEGIN [Jackal_Chen][2012/05/04][Modify logic for non-formal charger to enhance stability] */
static bool only_keep_input_current_flag = false;
/* END [Jackal_Chen][2012/05/04][Modify logic for non-formal charger to enhance stability] */

// Temperature |    ~ 0  |  0 ~ 10 | 10 ~ 25 | 25 ~ 45 | 45 ~
// -----------------------------------------------------------
// 3.0V ~ 4.0V | (0 , 0) | (0 , 1) | (0 , 2) | (0 , 3) | (0 , 4)
// -----------------------------------------------------------
// 4.0V ~ 4.1V | (1 , 0) | (1 , 1) | (1 , 2) | (1 , 3) | (1 , 4)
// -----------------------------------------------------------
// 4.1V ~ 4.2V | (2 , 0) | (2 , 1) | (2 , 2) | (2 , 3) | (2 , 4)

static int curr_tbl_for_usb[CURR_TBL_VOLT_ITEMS][CURR_TBL_TEMPE_ITEMS]=
{
//     0    10    25    45    ==>  Temperature range
  {   0,  384,  384,  384,   0},  //==> 3.0V ~ 4.0V
  {   0,  384,  384,  384,   0},  //==> 4.0V ~ 4.1V
  {   0,  384,  384,  384,   0},  //==> 4.1V ~
};

static int curr_tbl_for_usb_wall[CURR_TBL_VOLT_ITEMS][CURR_TBL_TEMPE_ITEMS] =
{
//     0    10    25    45    ==>  Temperature range
  {   0,  512, 2048, 2048,   0},  //==> 3.0V ~ 4.0V
  {   0,  512, 1152, 1152,   0},  //==> 4.0V ~ 4.1V
  {   0,  512, 1152, 1152,   0},  //==> 4.1V ~
};

static int curr_tbl_for_ac_wall[CURR_TBL_VOLT_ITEMS][CURR_TBL_TEMPE_ITEMS] =
{
//     0    10    25    45    ==>  Temperature range
  {   0,  512, 3072, 3840,   0},  //==> 3.0V ~ 4.0V
  {   0,  512, 1152, 1152,   0},  //==> 4.0V ~ 4.1V
  {   0,  512, 1152, 1152,   0},  //==> 4.1V ~
};

/* BEGIN [Terry_Tzeng][2012/04/17][Add inserted dock charging table for dock charger type] */
#ifdef MSM_CHARGER_ENABLE_DOCK
static int curr_tbl_for_dk_chg[CURR_TBL_VOLT_ITEMS][CURR_TBL_TEMPE_ITEMS] =
{
//     0    10    25    45    ==>  Temperature range
  {   0,  512, 2048, 2048,   0},  //==> 3.0V ~ 4.0V
  {   0,  512, 1152, 1152,   0},  //==> 4.0V ~ 4.1V
  {   0,  512, 1152, 1152,   0},  //==> 4.1V ~
};

#endif /*MSM_CHARGER_ENABLE_DOCK*/
/* END [Terry_Tzeng][2012/04/17][Add inserted dock charging table for dock charger type] */


static int isl9519q_read_reg(struct i2c_client *client, int reg,
	u16 *val)
{
	int ret;
	struct isl9519q_struct *isl_chg;

	isl_chg = i2c_get_clientdata(client);
	pega_power_enter_cs();
	ret = i2c_smbus_read_word_data(isl_chg->client, reg);
	pega_power_exit_cs();

	if (ret < 0) {
		dev_err(&isl_chg->client->dev,
			"i2c read fail: can't read from %02x: %d\n", reg, ret);
		return -EAGAIN;
	} else
		*val = ret;

	return 0;
}

static int isl9519q_write_reg(struct i2c_client *client, int reg,
	u16 val)
{
	int ret;
	struct isl9519q_struct *isl_chg;

	isl_chg = i2c_get_clientdata(client);
	pega_power_enter_cs();
	ret = i2c_smbus_write_word_data(isl_chg->client, reg, val);
	pega_power_exit_cs();

	if (ret < 0) {
		dev_err(&isl_chg->client->dev,
			"i2c write fail: can't write %02x to %02x: %d\n",
			val, reg, ret);
		return -EAGAIN;
	}
	return 0;
}

struct isl9519q_struct *temp_isl_chg = NULL;
static bool is_otg_plugin = false;


static struct delayed_work otg_isl_first_configure;
static struct delayed_work otg_isl_reconfigure;

/* BEGIN-20110816-KenLee-disable charger IC input current when otg plug-in. */
static void reconfigure_isl_control_reg(struct work_struct *work)
{
	int ret = 0;

	ret = isl9519q_write_reg(temp_isl_chg->client, CHG_CURRENT_REG, 128);
	if (ret)
	{
		dev_err(&temp_isl_chg->client->dev,"%s coulnt write to chg_current reg\n", __func__);
		schedule_delayed_work(&otg_isl_reconfigure, (HZ)*10);
	}
	else
	{
		PEGA_DBG_H("[CHG] %s, set CHARGE_REG as 128\n", __func__);
		schedule_delayed_work(&otg_isl_reconfigure, ISL9519_CHG_PERIOD);
	}
}

static int configure_isl_control_reg(bool set_disable)
{
	int ret = 0;
	u16 reg_val;

	if (set_disable)	// set control bit2 to 1
	{
		ret = isl9519q_read_reg(temp_isl_chg->client, CONTROL_REG, &reg_val);
		reg_val = reg_val | ISOLATE_ADAPTER_STATUS_BIT;
		PEGA_DBG_H("[CHG] %s, set CONTROL_REG as 0x%x\n", __func__, reg_val);

		udelay(66);
		ret = isl9519q_write_reg(temp_isl_chg->client, CONTROL_REG, reg_val);
		udelay(66);
		ret = isl9519q_write_reg(temp_isl_chg->client, CHG_CURRENT_REG, 128);
	}
	else				// clear control bit2 to 0
	{
		ret = isl9519q_read_reg(temp_isl_chg->client, CONTROL_REG, &reg_val);
		reg_val = reg_val &(~ISOLATE_ADAPTER_STATUS_BIT);
		PEGA_DBG_H("[CHG] %s, clear CONTROL_REG as 0x%x\n", __func__, reg_val);

		udelay(66);
		ret = isl9519q_write_reg(temp_isl_chg->client, CONTROL_REG, reg_val);
		udelay(66);
		ret = isl9519q_write_reg(temp_isl_chg->client, CHG_CURRENT_REG, 0);
	}
	return ret;
}

static void refire_otg_disable_input_current(struct work_struct *work)
{
	static int retry = 0;
	int ret = 0;

	if (temp_isl_chg)
	{
		PEGA_DBG_H("[CHG] %s, retry = %d\n", __func__, retry);
		ret = configure_isl_control_reg(true);
		if (ret)
			dev_err(&temp_isl_chg->client->dev,"%s coulnt write to chg_current reg\n", __func__);
		else
		{
			schedule_delayed_work(&otg_isl_reconfigure, ISL9519_CHG_PERIOD);
		}
	}
	else
	{
		if (retry < OTG_RETRY_COUNT)
		{
			PEGA_DBG_M("[CHG] %s still NULL, retry =%d\n", __func__, retry);
			schedule_delayed_work(&otg_isl_first_configure, OTG_REFIRE_INTERVAL);
			retry ++;
		}
		else
		{
			PEGA_DBG_H("[CHG_ERR] %s FAIL!!, retry =%d\n", __func__, retry);
			cancel_delayed_work(&otg_isl_first_configure);
		}
	}
}

void otg_disable_input_current(bool disable)
{
	int ret = 0;

	PEGA_DBG_H("[CHG] %s, disable = %d\n", __func__, (int) disable);
	is_otg_plugin = disable;

	if (disable)
	{
		if (temp_isl_chg == NULL)
		{
			INIT_DELAYED_WORK(&otg_isl_first_configure, refire_otg_disable_input_current);
			schedule_delayed_work(&otg_isl_first_configure, OTG_REFIRE_INTERVAL);
		}
		else
		{
			ret = configure_isl_control_reg(true);
			if (ret)
				dev_err(&temp_isl_chg->client->dev,"%s coulnt write to chg_current reg\n", __func__);
			else
			{
				schedule_delayed_work(&otg_isl_reconfigure, ISL9519_CHG_PERIOD);
			}
		}
	}
	else
	{
		ret = configure_isl_control_reg(false);
		if (ret)
			dev_err(&temp_isl_chg->client->dev,"%s coulnt write to control reg\n", __func__);

		cancel_delayed_work(&otg_isl_reconfigure);
		flush_delayed_work(&otg_isl_reconfigure);
	}
}
/* END-20110816-KenLee-disable charger IC input current when otg plug-in. */

#if defined PEGA_SMT_BUILD
#define ATS_CHG_CURRENT	128
static struct delayed_work ats_isl_reconfigure;

#if (PEGA_SMT_SOLUTION == 1)

static int configure_isl_max_vol_reg(void)
{
    int ret = 0;

	ret = isl9519q_write_reg(temp_isl_chg->client, MAX_SYS_VOLTAGE_REG,
    	temp_isl_chg->max_system_voltage);
    if (!ret)
        PEGA_DBG_H("[CHG] %s = %d\n", __func__, temp_isl_chg->max_system_voltage);

    return ret;
}

static int configure_isl_input_reg(int in_curr)
{
    int ret = 0;

    if (in_curr)    // enable input current
    {
        ret = isl9519q_write_reg(temp_isl_chg->client, INPUT_CURRENT_REG, in_curr);
        if (!ret)
            PEGA_DBG_H("[CHG] %s = %d\n", __func__, in_curr);
    }
    else            // disable input current
    {
        ret = isl9519q_write_reg(temp_isl_chg->client, INPUT_CURRENT_REG, 0);
        if (!ret)
            PEGA_DBG_H("[CHG] %s = 0\n", __func__);
    }
    return ret;
}
#endif

static int chgcurr_on_set(void *data, u64 val)
{
	int ret = 0;

	if (temp_isl_chg == NULL)
	{
		PEGA_DBG_H("[CHG] %s, pega_hw_chg = NULL!\n", __func__);
		return -EINVAL;
	}

	if (val)  // enable charge current
	{
		ret = isl9519q_write_reg(temp_isl_chg->client, CHG_CURRENT_REG, ATS_CHG_CURRENT);
		if (ret)
			dev_err(&temp_isl_chg->client->dev,"%s coulnt write to current reg\n", __func__);
	}
	else  // disable charge current
	{
		ret = isl9519q_write_reg(temp_isl_chg->client, CHG_CURRENT_REG, 0);
		if (ret)
			dev_err(&temp_isl_chg->client->dev,"%s coulnt write to current reg\n", __func__);
	}
	return 0;
}

static int incurr_on_set(void *data, u64 val)
{
	int ret = 0;
	u16 reg_val;

	if (temp_isl_chg == NULL)
	{
		PEGA_DBG_H("[CHG] %s, pega_hw_chg = NULL!\n", __func__);
		return -EINVAL;
	}

	if (val)  // enable input current
	{
		// clear control bit2 to 0
		ret = isl9519q_read_reg(temp_isl_chg->client, CONTROL_REG, &reg_val);
		reg_val = reg_val &(~ISOLATE_ADAPTER_STATUS_BIT);
		PEGA_DBG_H("[CHG] %s, clear CONTROL_REG as 0x%x\n", __func__, reg_val);
		udelay(66);
		ret = isl9519q_write_reg(temp_isl_chg->client, CONTROL_REG, reg_val);
		if (ret)
			dev_err(&temp_isl_chg->client->dev,"%s coulnt clear control bit=0 \n", __func__);
	}
	else  // disable input current
	{
		// set control bit2 to 1
		ret = isl9519q_read_reg(temp_isl_chg->client, CONTROL_REG, &reg_val);
		reg_val = reg_val | ISOLATE_ADAPTER_STATUS_BIT;
		PEGA_DBG_H("[CHG] %s, set CONTROL_REG as 0x%x\n", __func__, reg_val);
		udelay(66);
		ret = isl9519q_write_reg(temp_isl_chg->client, CONTROL_REG, reg_val);
		if (ret)
			dev_err(&temp_isl_chg->client->dev,"%s coulnt write control bit=1 \n", __func__);
	}
	return 0;
}

static int chg_on_set(void *data, u64 val)
{
	int ret = 0;
	u16 reg_val;

	if (temp_isl_chg == NULL)
	{
		PEGA_DBG_H("[CHG] %s, pega_hw_chg = NULL!\n", __func__);
		return -EINVAL;
	}

	if (val)  // enable charging
	{
		ret = isl9519q_write_reg(temp_isl_chg->client, CHG_CURRENT_REG, 0);
		if (ret)
			dev_err(&temp_isl_chg->client->dev,"%s coulnt write to current reg\n", __func__);

		// clear control bit2 to 0
		udelay(66);
		ret = isl9519q_read_reg(temp_isl_chg->client, CONTROL_REG, &reg_val);
		reg_val = reg_val &(~ISOLATE_ADAPTER_STATUS_BIT);
		PEGA_DBG_H("[CHG] %s, clear CONTROL_REG as 0x%x\n", __func__, reg_val);

		udelay(66);
		ret = isl9519q_write_reg(temp_isl_chg->client, CONTROL_REG, reg_val);
		if (ret)
			dev_err(&temp_isl_chg->client->dev,"%s coulnt write control reg\n", __func__);

		cancel_delayed_work(&ats_isl_reconfigure);

	}
	else  // disable charging
	{
		// set control bit2 to 1
		ret = isl9519q_read_reg(temp_isl_chg->client, CONTROL_REG, &reg_val);
		reg_val = reg_val | ISOLATE_ADAPTER_STATUS_BIT;
		PEGA_DBG_H("[CHG] %s, set CONTROL_REG as 0x%x\n", __func__, reg_val);

		udelay(66);
		ret = isl9519q_write_reg(temp_isl_chg->client, CONTROL_REG, reg_val);
		if (ret)
			dev_err(&temp_isl_chg->client->dev,"%s coulnt write control reg\n", __func__);

		udelay(66);
		ret = isl9519q_write_reg(temp_isl_chg->client, CHG_CURRENT_REG, ATS_CHG_CURRENT);
		if (ret)
			dev_err(&temp_isl_chg->client->dev,"%s coulnt write to current reg\n", __func__);

		schedule_delayed_work(&ats_isl_reconfigure, ISL9519_CHG_PERIOD);
	}
return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(chgcurr_on_fops, NULL, chgcurr_on_set, "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(incurr_on_fops, NULL, incurr_on_set, "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(chg_on_fops, NULL, chg_on_set, "%llu\n");

static int __init batt_debug_init(void)
{
	struct dentry *chg_debug;

	chg_debug = debugfs_create_dir("isl-dbg", 0);

	if (IS_ERR(chg_debug))
		return PTR_ERR(chg_debug);

	debugfs_create_file("chg_on", 0644, chg_debug, NULL, &chg_on_fops);
	debugfs_create_file("chgcurr_on", 0644, chg_debug, NULL, &chgcurr_on_fops);
	debugfs_create_file("incurr_on", 0644, chg_debug, NULL, &incurr_on_fops);

	PEGA_DBG_H("[CHG] %s succeed\n", __func__);

	return 0;
}

device_initcall(batt_debug_init);
#endif

/* BEGIN-KenLee-20111028-add for dynamic charging current depended on backlight level */
static int isl9519q_calculate_chg_curr(int original_curr)
{
	int chg_cur = 0;

	/* HW ignores lower 7 bits of charging current and input current */
	if ((original_curr * R_MULTIPLE) % ISL_REG_CURRENT_BASE)
		chg_cur = ISL_REG_CURRENT_BASE * ((original_curr * R_MULTIPLE)/ISL_REG_CURRENT_BASE);
	else
		chg_cur = original_curr * R_MULTIPLE;

    PEGA_DBG_M("[CHG] %s, chg_cur after calculated= %d \n",__func__, chg_cur);
    return chg_cur;
}
/* END-KenLee-20111028-add for dynamic charging current depended on backlight level */

/* BEGIN-KenLee-20111028-add for dynamic charging current depended on backlight level */
#if (BACKLIGHT_DYNAMIC_CURRENT == 1)
static int pega_isl9519q_adjust_current_by_backlight(struct msm_hardware_charger *hw_chg)
{
    unsigned int bl_level;
    int ret, chg_cur;
	struct isl9519q_struct *isl_chg;
	struct msm_hardware_charger_priv *priv = NULL;

    priv = hw_chg->charger_private;

    /* BEGIN [Jackal_Chen][2012/03/05][Modify logic for 5V/12V adaptor] */
    if (priv->max_source_current != 1500 && priv->max_source_current != 3000) {
        PEGA_DBG_H("[CHG] %s, chg_type != 5V or 12V adaptor!\n",__func__);
        return 0;
    }
    /*if (priv->usb_chg_type != USB_CHG_TYPE__WALLCHARGER)
    {
        PEGA_DBG_H("[CHG] %s, usb_chg_type != USB_CHG_TYPE__WALLCHARGER \n",__func__);
        return 0;
    }*/
    /* END [Jackal_Chen][2012/03/05][Modify logic for 5V/12V adaptor] */

	isl_chg = container_of(hw_chg, struct isl9519q_struct, adapter_hw_chg);
    bl_level = current_brightness_level();

    if (bl_level == 3)
        chg_cur = CHG_CURR_BACKLIGHT_LEVEL_3;
    else if (bl_level == 2)
        chg_cur = CHG_CURR_BACKLIGHT_LEVEL_2;
    else if (bl_level == 1)
        chg_cur = CHG_CURR_BACKLIGHT_LEVEL_1;
    else    //bl_level == 0 or others
        chg_cur = isl_chg->usbcgr_current;

    PEGA_DBG_H("[CHG] %s, chg_type= %d, bl_level= %d, chg_cur= %d \n",__func__, priv->usb_chg_type,bl_level,chg_cur);

    ret = isl9519q_calculate_chg_curr(chg_cur);

    return ret;
}
#endif
/* END-KenLee-20111028-add for dynamic charging current depended on backlight level */

static int pega_isl9519q_read_volt_index(int volt)
{
    int idx;

    if (prev_volt_idx == -1) // first time after charging start
    {
        // decide index of voltage
        if (volt >= VOLT_SLOW_CHG_LEVEL_H)
            idx = 2;
        else if (volt >= VOLT_SLOW_CHG_LEVEL_L)
            idx = 1;
        else
            idx = 0;
    }
    else
    {
        // decide index of voltage
        if (volt >= (VOLT_SLOW_CHG_LEVEL_H + VOLT_HYSTERESIS))
            idx = 2;
        else if (volt >= (VOLT_SLOW_CHG_LEVEL_H - VOLT_HYSTERESIS))
            idx = prev_volt_idx;
        else if (volt >= (VOLT_SLOW_CHG_LEVEL_L + VOLT_HYSTERESIS))
            idx = 1;
        else if (volt >= (VOLT_SLOW_CHG_LEVEL_L - VOLT_HYSTERESIS))
            idx = prev_volt_idx;
        else
            idx = 0;
    }
    prev_volt_idx = idx;
    return idx;
}

static int pega_isl9519q_read_current_from_table(int max_chg_curr)
{
    int tempe = 0, volt = 0;
    int idx_t = 0, idx_v = 0;
    int ret, chg_cur;

    /* BEGIN [2012/03/12] Jackal - use fixed idx_t for SMT version */
#if (PEGA_SMT_SOLUTION == 1)
    tempe = 300;  // Use 30 'c to let idx_t = 3 (any value between 250 and 450 can be used)
#else
    tempe = msm_chg_get_batt_temperture();
#endif
    /* BEGIN [2012/03/12] Jackal - use fixed idx_t for SMT version */
    volt = msm_chg_get_batt_mvolts();

    // read index of temperature
    if (tempe < TEMPE_STOP_CHG_DEGREE_STOP)
        idx_t = 0;
    else if (tempe < TEMPE_STOP_CHG_DEGREE_L)
        idx_t = 1;
    else if (tempe < TEMPE_STOP_CHG_DEGREE_M)
        idx_t = 2;
    else if (tempe < TEMPE_STOP_CHG_DEGREE_H)
        idx_t = 3;
    else
        idx_t = 4;

#if (VOLT_MULTI_LEVEL_ENABLE == 1)
    idx_v = pega_isl9519q_read_volt_index(volt);
#else
    idx_v = 0;
#endif

    if (max_chg_curr == 500)
    {
        chg_cur = curr_tbl_for_usb[idx_v][idx_t];
        PEGA_DBG_H("[CHG] type=USB, tempe=%d, volt=%d, idx=(%d,%d), curr=%d \n",tempe,volt,idx_v,idx_t,chg_cur);
    }
    else if (max_chg_curr == 1500)
    {
        chg_cur = curr_tbl_for_usb_wall[idx_v][idx_t];
        PEGA_DBG_H("[CHG] type=USB WALL, tempe=%d, volt=%d, idx=(%d,%d), curr=%d \n",tempe,volt,idx_v,idx_t,chg_cur);
    }
/* BEGIN [Terry_Tzeng][2012/04/17][Add inserted dock charging table for dock charger type] */
#ifdef MSM_CHARGER_ENABLE_DOCK
    else if (max_chg_curr == INSERT_DK_MAX_CURR)
    {
	chg_cur = curr_tbl_for_dk_chg[idx_v][idx_t];
        PEGA_DBG_H("[CHG] type= INSERT DK CHG, tempe=%d, volt=%d, idx=(%d,%d), curr=%d \n", tempe,volt, idx_v, idx_t, chg_cur);
    }
#endif /* MSM_CHARGER_ENABLE_DOCK */
/* END [Terry_Tzeng][2012/04/17][Add inserted dock charging table for dock charger type] */
    else if (max_chg_curr == 3000)
    {
        chg_cur = curr_tbl_for_ac_wall[idx_v][idx_t];
        PEGA_DBG_H("[CHG] type=AC, tempe=%d, volt=%d, idx=(%d,%d), curr=%d \n",tempe,volt,idx_v,idx_t,chg_cur);
    }
    else
    {
        chg_cur = 0;
        PEGA_DBG_H("[CHG] type=unknown, tempe=%d, volt=%d, idx=(%d,%d), curr=%d \n",tempe,volt,idx_v,idx_t,chg_cur);
    }

    ret = isl9519q_calculate_chg_curr(chg_cur);

    return ret;
}

/* BEGIN [Terry_Tzeng][2012/03/21][Set periodically charging current callback function in suspend status] */
#ifdef MSM_CHARGER_ENABLE_DOCK
static void isl9519q_set_periodically_chg_curr_for_insert_dk(struct msm_hardware_charger *hw_chg)
{
	u16 temp;
	int in_curr, chg_curr;
	int ret;
	struct isl9519q_struct *isl_chg;

	isl_chg = container_of(hw_chg, struct isl9519q_struct, adapter_hw_chg);

	if (isl_chg->charging) {
		int gdk_ver = echger_get_dk_hw_id();

		ret  = msm_chg_get_batt_status();
		PEGA_DBG_H("[CHG] %s, gauge_batt_status= %d \n",  __func__, ret);

		ret = echger_get_tb_in_chg_curr(&in_curr ,&chg_curr);

		if(DOCK_DVT == gdk_ver)
		{
		if(ECCHE_CHG_ST_NO_CHANGE == ret)
		{
			charge_current = isl9519q_calculate_chg_curr(chg_curr);
		}
		else
		{
			charge_current = 0;
		}
		}
		else if((DOCK_PVT <= gdk_ver) && (DOCK_UNKNOWN > gdk_ver))
		{
			charge_current = pega_isl9519q_read_current_from_table(INSERT_DK_MAX_CURR);
			if(chg_curr == 0)
				charge_current = 0;
		}
		else
		{
			charge_current = 0;
		}

		PEGA_DBG_H("[CHG] %s, Monitor tablet chg_curr = %d, ret_st =%d in dock in statement\n",  __func__, charge_current, ret);

		ret = isl9519q_write_reg(isl_chg->client, INPUT_CURRENT_REG, in_curr);
		if (!ret) {
			PEGA_DBG_H("[CHG] .. write in_cur= %d to INPUT_CURRENT_REG \n",in_curr);
		}
		else
			PEGA_DBG_H("[CHG_ERR] .. couldnt write %d to INPUT_CURRENT_REG , ret= %d \n",in_curr ,ret);

		isl9519q_write_reg(isl_chg->client, CHG_CURRENT_REG, charge_current);
		if (!ret) {
			PEGA_DBG_H("[CHG] .. write charge_current= %d to CHG_CURRENT_REG \n",charge_current);
		}
		else
			PEGA_DBG_H("[CHG_ERR] .. couldnt write %d to CHG_CURRENT_REG , ret= %d \n",charge_current,ret);
		//pega_isl9519q_chg_prepare(hw_chg, in_curr, charge_current);
		//echger_notify_tb_start_chg_finish(in_curr, chg_curr);


		ret = isl9519q_read_reg(isl_chg->client, CONTROL_REG, &temp);
		if (!ret) {
			if (!(temp & TRCKL_CHG_STATUS_BIT))
				msm_charger_notify_event(
						&isl_chg->adapter_hw_chg,
						CHG_BATT_BEGIN_FAST_CHARGING);
		} else {
			dev_err(&isl_chg->client->dev,
				"%s couldnt read cntrl reg\n", __func__);
		}
	}
	else
		PEGA_DBG_H("[CHG_ERR] %s, isl_chg->charging= 0 \n",  __func__);


}
#endif /* MSM_CHARGER_ENABLE_DOCK */
/* END [Terry_Tzeng][2012/03/21][Set periodically charging current callback function in suspend status] */


static void isl9519q_charge(struct work_struct *isl9519_work)
{
	u16 temp;
	int ret;
	struct isl9519q_struct *isl_chg;

	/* BEGIN [2012/03/06] Jackal - add for dynamic charging current adjusting */
#if (PEGA_SMT_SOLUTION != 1)
	int tmp_curr_1;
	int tmp_curr_2 = 9999;
	struct msm_hardware_charger *hw_chg;
	struct msm_hardware_charger_priv *priv = NULL;
	/* BEGIN [Jackal_Chen][2012/04/10][Detect Y-cable by adc] */
	int64_t adc_value;
	int rc;
	/* END [Jackal_Chen][2012/04/10][Detect Y-cable by adc] */
#endif
	/* END [2012/03/06] Jackal - add for dynamic charging current adjusting */
	/* BEGIN [2012/04/25] Jackal - User may charge device when battery is full but charge current not less than 300mA yet */
	int batt_current = 0;
	/* END [2012/04/25] Jackal - User may charge device when battery is full but charge current not less than 300mA yet */

	isl_chg = container_of(isl9519_work, struct isl9519q_struct,
			charge_work.work);

	dev_dbg(&isl_chg->client->dev, "%s\n", __func__);

	if (isl_chg->charging) {
		ret  = msm_chg_get_batt_status();
		PEGA_DBG_H("[CHG] %s, gauge_batt_status= %d \n",  __func__, ret);

		/* BEGIN [2012/03/29] Jackal - If battery full, no need to do other setting below(pad only) */
		/* BEGIN [2012/04/25] Jackal - User may charge device when battery is full but charge current not less than 300mA yet */
		batt_current = msm_chg_get_batt_current();
		PEGA_DBG_H("[CHG] %s, gauge_batt_current= %d \n",  __func__, batt_current);
		if (ret == POWER_SUPPLY_STATUS_FULL && batt_current > 0 && batt_current <= 350) {
		//if (ret == POWER_SUPPLY_STATUS_FULL) {
			msm_charger_notify_event(&isl_chg->adapter_hw_chg, CHG_DONE_EVENT);
			/* BEGIN [Jackal_Chen][2012/04/09][Use HW_ID value to distinguish GPIO number of dock_in pin] */
			if(gpio_get_value_cansleep(dock_in_gpio_number)) {
				PEGA_DBG_H("[CHG] %s, No need to set other settings below, return. \n",  __func__);
				return;
			}
			/* END [Jackal_Chen][2012/04/09][Use HW_ID value to distinguish GPIO number of dock_in pin] */
		}
		/* END [2012/04/25] Jackal - User may charge device when battery is full but charge current not less than 300mA yet */
		/* END [2012/03/29] Jackal - If battery full, no need to do other setting below(pad only) */

#if (PEGA_SMT_SOLUTION == 1)
        ret = configure_isl_max_vol_reg();

/* BEGIN [Terry_Tzeng][2012/03/07][Set notification to check input current and changing current status in dock in statement for SMT version] */
	#ifdef MSM_CHARGER_ENABLE_DOCK
		/* BEGIN [Jackal_Chen][2012/04/09][Use HW_ID value to distinguish GPIO number of dock_in pin] */
		if(!gpio_get_value_cansleep(dock_in_gpio_number))
		/* END [Jackal_Chen][2012/04/09][Use HW_ID value to distinguish GPIO number of dock_in pin] */
		{
		/* BEGIN [Terry_Tzeng][2012/04/17][Add charging table for battery temperture and cell in PVT] */
			int in_curr, chg_curr;
			int ret;

			int gdk_ver = echger_get_dk_hw_id();

			ret = echger_get_tb_in_chg_curr(&in_curr ,&chg_curr);

			if(DOCK_DVT == gdk_ver)
			{
				if(ECCHE_CHG_ST_NO_CHANGE == ret)
				{
					charge_current = isl9519q_calculate_chg_curr(chg_curr);
				}
				else
				{
					charge_current = 0;
				}
			}
			else if((DOCK_PVT <= gdk_ver) && (DOCK_UNKNOWN > gdk_ver))
			{
				charge_current = pega_isl9519q_read_current_from_table(INSERT_DK_MAX_CURR);
				if(chg_curr == 0)
					charge_current = 0;

			}
			else
			{
				charge_current = 0;
			}

		/* END [Terry_Tzeng][2012/04/17][Add charging table for battery temperture and cell in PVT] */
			PEGA_DBG_H("[CHG] %s, Monitor tablet chg_curr = %d, ret_st =%d in dock in statement\n",  __func__, charge_current, ret);

			isl9519q_write_reg(isl_chg->client, CHG_CURRENT_REG, charge_current);
			//pega_isl9519q_chg_prepare(struct msm_hardware_charger *hw_chg, int in_cur, int chg_cur);
			//echger_notify_tb_start_chg_finish(in_curr, chg_curr);
		}
	#endif /* MSM_CHARGER_ENABLE_DOCK */
/* END [Terry_Tzeng][2012/03/07][Set notification to check input current and changing current status in dock in statement for SMT version] */

#else   //PEGA_SMT_SOLUTION == 0 or 2


/* BEGIN [Terry_Tzeng][2012/02/14][Set notification to check input current and changing current status in dock in statement] */
#ifdef MSM_CHARGER_ENABLE_DOCK
		/* BEGIN [Jackal_Chen][2012/04/09][Use HW_ID value to distinguish GPIO number of dock_in pin] */
		if(!gpio_get_value_cansleep(dock_in_gpio_number))
		/* END [Jackal_Chen][2012/04/09][Use HW_ID value to distinguish GPIO number of dock_in pin] */
		{
			int in_curr, chg_curr;
			int ret;

			int gdk_ver = echger_get_dk_hw_id();

			ret = echger_get_tb_in_chg_curr(&in_curr ,&chg_curr);

			if(DOCK_DVT == gdk_ver)
			{
				if(ECCHE_CHG_ST_NO_CHANGE == ret)
				{
					charge_current = isl9519q_calculate_chg_curr(chg_curr);
				}
				else
				{
					charge_current = 0;
				}
			}
			else if((DOCK_PVT <= gdk_ver) && (DOCK_UNKNOWN > gdk_ver))
			{
				charge_current = pega_isl9519q_read_current_from_table(INSERT_DK_MAX_CURR);
				if(chg_curr == 0)
					charge_current = 0;
			}
			else
			{
				charge_current = 0;
			}

			PEGA_DBG_H("[CHG] %s, Monitor tablet chg_curr = %d, ret_st =%d in dock in statement\n",  __func__, charge_current, ret);

			isl9519q_write_reg(isl_chg->client, CHG_CURRENT_REG, charge_current);
			//pega_isl9519q_chg_prepare(struct msm_hardware_charger *hw_chg, int in_cur, int chg_cur);
			//echger_notify_tb_start_chg_finish(in_curr, chg_curr);
		}
		else
		{
#endif /* MSM_CHARGER_ENABLE_DOCK */
/* END [Terry_Tzeng][2012/02/14][Set notification to check input current and changing current status in dock in statement] */

			// BEGIN [2012/03/05] Jackal - Modify the logic of chg_type determine
			hw_chg = &(isl_chg->adapter_hw_chg);
			priv = hw_chg->charger_private;
			if (MAX_CHG_CURR_USB == priv->max_source_current) {  // USB cable
				/* BEGIN [Jackal_Chen][2012/04/10][Detect Y-cable by adc] */
				// BEGIN [2012/03/13] Jackal - Modify chg_curr of USB to 0 for SMT version
#if (PEGA_SMT_SOLUTION == 1)
            			charge_current = 0;
#else
				rc = pega_get_mpp_adc(PEGA_CHG_DEVICE_ID_MPP_MUNBER, &adc_value);
				if(!rc) {
					PEGA_DBG_H("[CHG] %s(), device_id adc value  = %lld\n", __func__, adc_value);
				}
				if(adc_value >= 1100000 && adc_value <1300000) {  // serve as 1.2V => Y-cable
					charge_current = isl_chg->usbcgr_current;
					PEGA_DBG_H("[CHG] %s, chg_type = Y-cable, write chg curr= %d  \n",__func__,charge_current);
				} else {  // serve as USB cable
					charge_current = pega_isl9519q_read_current_from_table(priv->max_source_current);
					PEGA_DBG_H("[CHG] %s, chg_type = USB, write chg curr= %d  \n",__func__,charge_current);
				}
#endif
				// END [2012/03/13] Jackal - Modify chg_curr of USB to 0 for SMT version
				/* END [Jackal_Chen][2012/04/10][Detect Y-cable by adc] */
            			isl9519q_write_reg(isl_chg->client, CHG_CURRENT_REG, charge_current);
        			} else {
#if (BACKLIGHT_DYNAMIC_CURRENT == 1)
            			tmp_curr_2 = pega_isl9519q_adjust_current_by_backlight(isl_chg);
#endif   /* BACKLIGHT_DYNAMIC_CURRENT == 1 */

				if (MAX_CHG_CURR_AC == priv->max_source_current) {
					PEGA_DBG_H("[CHG] %s, chg_type = 12V AC \n",__func__);
				} else if(MAX_CHG_CURR_USB_WALL == priv->max_source_current) {
					PEGA_DBG_H("[CHG] %s, chg_type = WALL CHARGER \n",__func__);
            			} else {
            				PEGA_DBG_H("[CHG] %s, chg_type = unknown \n",__func__);
            			}
				tmp_curr_1 = pega_isl9519q_read_current_from_table(priv->max_source_current);

				if (tmp_curr_1 < tmp_curr_2)
                				charge_current = tmp_curr_1;
            			else
                				charge_current = tmp_curr_2;

				PEGA_DBG_H("[CHG] %s, write chg curr = %d \n",  __func__, charge_current);
				isl9519q_write_reg(isl_chg->client, CHG_CURRENT_REG, charge_current);
				/* BEGIN [2012/04/16] Jackal - Reset in_curr to 1920 if wall_chg_type is 1A or 700mA to correct the mis-action when rapidly plug in/out */
#if (PEGA_CHG_SUPPORT_NON_FORMAL_CHARGER == 1)
				if(MAX_CHG_CURR_USB_WALL == priv->max_source_current) {
					if(wall_chg_type == WALL_CHG_TYPE_1A || wall_chg_type == WALL_CHG_TYPE_700MA) {
						wall_chg_type = WALL_CHG_TYPE_2A;
						isl9519q_write_reg(isl_chg->client, INPUT_CURRENT_REG, isl_chg->usbcgr_input_current);
						PEGA_DBG_H("[CHG] %s, Reset input current to 1920.\n",  __func__);
					}
					/* BEGIN [Jackal_Chen][2012/04/23][More non-formal charger type support] */
#if (PEGA_CHG_SUPPORT_MORE_CHARGER_TYPE == 1)
					else if(wall_chg_type == WALL_CHG_TYPE_1500MA || wall_chg_type == WALL_CHG_TYPE_500MA) {
						wall_chg_type = WALL_CHG_TYPE_2A;
						isl9519q_write_reg(isl_chg->client, INPUT_CURRENT_REG, isl_chg->usbcgr_input_current);
						PEGA_DBG_H("[CHG] %s, Reset input current to 1920.\n",  __func__);
					}
#endif
					/* END [Jackal_Chen][2012/04/23][More non-formal charger type support] */
				}
#endif
				/* END [2012/04/16] Jackal - Reset in_curr to 1920 if wall_chg_type is 1A or 700mA to correct the mis-action when rapidly plug in/out */
        			}
			// END [2012/03/05] Jackal - Modify the logic of chg_type determine

/* BEGIN [Terry_Tzeng][2012/02/14][Set notification to check input current and changing current status in dock in statement] */
#ifdef MSM_CHARGER_ENABLE_DOCK
		}
#endif /* MSM_CHARGER_ENABLE_DOCK */
/* END [Terry_Tzeng][2012/02/14][Set notification to check input current and changing current status in dock in statement] */
#endif  /* PEGA_SMT_SOLUTION == 1 */

		ret = isl9519q_read_reg(isl_chg->client, CONTROL_REG, &temp);
		if (!ret) {
			if (!(temp & TRCKL_CHG_STATUS_BIT))
				msm_charger_notify_event(
						&isl_chg->adapter_hw_chg,
						CHG_BATT_BEGIN_FAST_CHARGING);
		} else {
			dev_err(&isl_chg->client->dev,
				"%s couldnt read cntrl reg\n", __func__);
		}
		schedule_delayed_work(&isl_chg->charge_work,
						ISL9519_CHG_PERIOD);
	}
	else
		PEGA_DBG_H("[CHG_ERR] %s, isl_chg->charging= 0 \n",  __func__);

}


static int pega_isl9519q_chg_prepare(struct msm_hardware_charger *hw_chg,
int in_cur,
int chg_cur)
{
	struct isl9519q_struct *isl_chg;
	int ret = 0;
	int temp;

	isl_chg = container_of(hw_chg, struct isl9519q_struct, adapter_hw_chg);
	PEGA_DBG_M("[CHG] %s \n",__func__);

	/* BEGIN [Jackal_Chen][2012/04/25][Remove check: in_cur != input_current] */
	// Set input current
	ret = isl9519q_write_reg(isl_chg->client, INPUT_CURRENT_REG, in_cur);
	if (!ret) {
		input_current = in_cur;
		PEGA_DBG_H("[CHG] .. write in_cur= %d to INPUT_CURRENT_REG \n",in_cur);
	}
	else
		PEGA_DBG_H("[CHG] .. couldnt write %d to INPUT_CURRENT_REG , ret= %d \n",in_cur,ret);
#if 0
	if (in_cur != input_current)
	{
		ret = isl9519q_write_reg(isl_chg->client, INPUT_CURRENT_REG, in_cur);
/* BEGIN [Terry_Tzeng][2012/02/14][Set log to check input current is writen] */
		if (!ret)
		{
			input_current = in_cur;
			PEGA_DBG_H("[CHG] .. write in_cur= %d to INPUT_CURRENT_REG \n",in_cur);
		}
		else
			PEGA_DBG_H("[CHG] .. couldnt write %d to INPUT_CURRENT_REG , ret= %d \n",in_cur,ret);
/* END [Terry_Tzeng][2012/02/14][Set log to check input current is writen] */

	}
#endif
	/* END [Jackal_Chen][2012/04/25][Remove check: in_cur != input_current] */

	// Step down max system voltage	to 3400
	for (temp=(isl_chg->max_system_voltage/100)*100;temp>=3400;temp=temp-100)
	{
		ret = isl9519q_write_reg(isl_chg->client, MAX_SYS_VOLTAGE_REG, temp);
		if (ret)
		{
			PEGA_DBG_M("[CHG] .. couldnt write %d to MAX_SYS_VOLTAGE_REG , ret= %d \n",temp,ret);
		}
		else
		{
			PEGA_DBG_M("[CHG] .. write max_vol= %d to MAX_SYS_VOLTAGE_REG \n",temp);
		}
	}

	// Set min system voltage to 3328
	ret = isl9519q_write_reg(isl_chg->client, MIN_SYS_VOLTAGE_REG, 3328);
	if (ret)
	{
		PEGA_DBG_M("[CHG] .. couldnt write %d to MIN_SYS_VOLTAGE_REG , ret= %d \n",3328, ret);
	}
	else
	{
		PEGA_DBG_M("[CHG] .. write min_vol= %d to MIN_SYS_VOLTAGE_REG \n", 3328);
	}

	// Set min charge current
	ret = isl9519q_write_reg(isl_chg->client, CHG_CURRENT_REG, 128);
	if (ret)
	{
		PEGA_DBG_M("[CHG] .. couldnt write 128 to CHG_CURRENT_REG , ret= %d \n",ret);
	}
	else
	{
		PEGA_DBG_M("[CHG] .. write chg_cur= 128 to CHG_CURRENT_REG \n");
	}


	// Step up max system voltage to original setting
	for (temp=3500;temp<isl_chg->max_system_voltage;temp=temp+100)
	{
		ret = isl9519q_write_reg(isl_chg->client, MAX_SYS_VOLTAGE_REG, temp);
		if (ret)
		{
			PEGA_DBG_M("[CHG] .. couldnt write %d to MAX_SYS_VOLTAGE_REG , ret= %d \n",temp,ret);
		}
		else
		{
			PEGA_DBG_M("[CHG] .. write max_vol= %d to MAX_SYS_VOLTAGE_REG \n",temp);
		}
	}
	ret = isl9519q_write_reg(isl_chg->client, MAX_SYS_VOLTAGE_REG, isl_chg->max_system_voltage);
	if (ret)
	{
		PEGA_DBG_M("[CHG] .. couldnt write %d to MAX_SYS_VOLTAGE_REG , ret= %d \n",ret,isl_chg->max_system_voltage);
	}
	else
	{
		PEGA_DBG_M("[CHG] .. write max_vol= %d to MAX_SYS_VOLTAGE_REG \n",isl_chg->max_system_voltage);
	}


	// Set min system voltage to original setting (3072)
	ret = isl9519q_write_reg(isl_chg->client, MIN_SYS_VOLTAGE_REG, isl_chg->min_system_voltage);
	if (ret)
	{
		PEGA_DBG_M("[CHG] .. couldnt write %d to MIN_SYS_VOLTAGE_REG , ret= %d \n",isl_chg->min_system_voltage, ret);
	}
	else
	{
		PEGA_DBG_M("[CHG] .. write min_vol= %d to MIN_SYS_VOLTAGE_REG \n", isl_chg->min_system_voltage);
	}


	// Step up charge current to original setting
	for (temp=384;temp<chg_cur;temp=temp+256)
	{
		ret = isl9519q_write_reg(isl_chg->client, CHG_CURRENT_REG, temp);
		if (ret)
		{
			PEGA_DBG_M("[CHG] .. couldnt write %d to CHG_CURRENT_REG , ret= %d \n",temp,ret);
		}
		else
		{
			PEGA_DBG_M("[CHG] .. write chg_cur= %d to CHG_CURRENT_REG \n",temp);
		}
	}

	// Set charge current
	ret = isl9519q_write_reg(isl_chg->client, CHG_CURRENT_REG, chg_cur);
/* BEGIN [Terry_Tzeng][2012/02/14][Set log to check charging current is writen] */
	if (ret)
	{
		PEGA_DBG_H("[CHG] .. couldnt write %d to CHG_CURRENT_REG , ret= %d \n",chg_cur,ret);
	}
	else
	{
		PEGA_DBG_H("[CHG] .. write chg_cur= %d to CHG_CURRENT_REG \n",chg_cur);
		charge_current = chg_cur;
	}
/* END [Terry_Tzeng][2012/02/14][Set log to check charging current is writen] */
	return ret;
}

static int isl9519q_start_charging(struct msm_hardware_charger *hw_chg,
		int chg_voltage, int chg_current)
{
	struct isl9519q_struct *isl_chg;
	int ret = 0;
	int in_cur = 0, chg_cur = 0;
	int tmp_curr_1 = 0, tmp_curr_2 = 9999;
	/* BEGIN [Jackal_Chen][2012/04/10][Detect Y-cable by adc] */
	int64_t adc_value;
	int rc;
	/* END [Jackal_Chen][2012/04/10][Detect Y-cable by adc] */

	isl_chg = container_of(hw_chg, struct isl9519q_struct, adapter_hw_chg);
	PEGA_DBG_H("[CHG] %s, vol= %d, curt= %d, is_charge= %d\n",__func__, chg_voltage, chg_current,isl_chg->charging);

	if (isl_chg->charging)
		/* we are already charging */
		return 0;

	dev_dbg(&isl_chg->client->dev, "%s\n", __func__);

	/* BEGIN [Jackal_Chen][2012/01/16][Modify logic for AC] */
	if (chg_current == MAX_CHG_CURR_AC) {    // AC charger
		/* HW ignores lower 7 bits of charging current and input current */

		/* BEGIN-KenLee-20111028-add for dynamic charging current depended on backlight level */
		chg_cur = isl9519q_calculate_chg_curr(isl_chg->chgcurrent);
		/* END-KenLee-20111028-add for dynamic charging current depended on backlight level */
		/* BEGIN [Jackal_Chen][2012/03/05][Modify chg_curr according to temperature] */
		tmp_curr_1 = pega_isl9519q_read_current_from_table(chg_current);
#if (BACKLIGHT_DYNAMIC_CURRENT == 1)
		tmp_curr_2 = pega_isl9519q_adjust_current_by_backlight(isl_chg);
#endif
		if (tmp_curr_1 < tmp_curr_2) {
			chg_cur = tmp_curr_1;
		} else {
			chg_cur = tmp_curr_2;
		}
		/* END [Jackal_Chen][2012/03/05][Modify chg_curr according to temperature] */
		in_cur = isl_chg->input_current;
		PEGA_DBG_H("[CHG] .. AC_CHG: in_cur= %d , chg_cur= %d, real_cur= %d\n", in_cur, chg_cur, chg_cur*R_MULTIPLE_REV);
	}
	/* BEGIN [Terry_Tzeng][2012/02/09][Write input and charging current in dock in mode] */
#ifdef MSM_CHARGER_ENABLE_DOCK
	else if (chg_current == INSERT_DK_MAX_CURR) {    // Dock
	/* BEGIN [Terry_Tzeng][2012/04/17][Add charging table for battery temperture and cell] */
		int gdk_ver = echger_get_dk_hw_id();

		echger_get_tb_in_chg_curr(&isl_chg->dock_input_current ,&isl_chg->dock_chg_current);

		if(DOCK_DVT == gdk_ver)
			chg_cur = isl9519q_calculate_chg_curr(isl_chg->dock_chg_current);
		else if((DOCK_PVT <= gdk_ver) && (DOCK_UNKNOWN > gdk_ver))
		{
			chg_cur = pega_isl9519q_read_current_from_table(INSERT_DK_MAX_CURR);
			if(isl_chg->dock_chg_current == 0)
				chg_cur = 0;
		}
		else
			chg_cur = 0;
		in_cur = isl_chg->dock_input_current;
		PEGA_DBG_H("[CHG] .. DOCK: chg_type = %d, in_cur= %d , chg_cur= %d, real_cur= %d\n", gdk_ver, in_cur, chg_cur, chg_cur*R_MULTIPLE_REV);
	/* END [Terry_Tzeng][2012/04/17][Add charging table for battery temperture and cell] */
	}
	#endif /* MSM_CHARGER_ENABLE_DOCK */
	/* BEGIN [Terry_Tzeng][2012/02/09][Write input and charging current in dock in mode] */
	else // USB type
	{
		if (chg_current <= MAX_CHG_CURR_USB) {  // SDP
			/* BEGIN [Jackal_Chen][2012/04/10][Detect Y-cable by adc] */
			rc = pega_get_mpp_adc(PEGA_CHG_DEVICE_ID_MPP_MUNBER, &adc_value);
			if(!rc) {
				PEGA_DBG_H("[CHG] %s, device_id adc value  = %lld\n", __func__, adc_value);
			}
			if(adc_value > 1100000 && adc_value <= 1300000) {
				/* BEGIN [Jackal_Chen][2012/06/29][For Y-cable, we still only support input/charge current as USB cable] */
				chg_cur = pega_isl9519q_read_current_from_table(chg_current);
				in_cur = isl_chg->usb_input_current;
				/*chg_cur = isl_chg->usbcgr_current;
				in_cur = isl_chg->usbcgr_input_current;*/
				/* END [Jackal_Chen][2012/06/29][For Y-cable, we still only support input/charge current as USB cable] */
				PEGA_DBG_H("[CHG] .. Y-cable: in_cur= %d , chg_cur= %d, real_cur= %d\n", in_cur, chg_cur, chg_cur*R_MULTIPLE_REV);
			} else {
				chg_cur = pega_isl9519q_read_current_from_table(chg_current);
				in_cur = isl_chg->usb_input_current;
				PEGA_DBG_H("[CHG] .. USB SDP: in_cur= %d , chg_cur= %d, real_cur= %d\n", in_cur, chg_cur, chg_cur*R_MULTIPLE_REV);
			}
			/* END [Jackal_Chen][2012/04/10][Detect Y-cable by adc] */
		} else if(chg_current == MAX_CHG_CURR_USB_WALL) {  // Wall charger
			in_cur = isl_chg->usbcgr_input_current;

			/* BEGIN-KenLee-20111123-add for dynamic charging current depended on backlight level */
#if (BACKLIGHT_DYNAMIC_CURRENT == 1)
			tmp_curr_2 = pega_isl9519q_adjust_current_by_backlight(isl_chg);
#endif
			/* END-KenLee-20111123-add for dynamic charging current depended on backlight level */
			/* BEGIN-KenLee-20111123-add for dynamic charging current depended on cell temperature */
			tmp_curr_1 = pega_isl9519q_read_current_from_table(chg_current);
			/* END-KenLee-20111123-add for dynamic charging current depended on cell temperature */

			if (tmp_curr_1 < tmp_curr_2)
				chg_cur = tmp_curr_1;
			else
				chg_cur = tmp_curr_2;

			/* BEGIN [Jackal_Chen][2012/04/05][Non-formal charger support] */
#if (PEGA_CHG_SUPPORT_NON_FORMAL_CHARGER == 1)
			wall_chg_type = WALL_CHG_TYPE_2A;
#endif
			/* END [Jackal_Chen][2012/04/05][Non-formal charger support] */

			PEGA_DBG_H("[CHG] ...  WALL: in_cur= %d , chg_cur= %d, real_cur= %d\n", in_cur, chg_cur, chg_cur*R_MULTIPLE_REV);
		} else {
			PEGA_DBG_H("[CHG] ...  UNKNOWN: in_cur= %d , chg_cur= %d, real_cur= %d\n", in_cur, chg_cur, chg_cur*R_MULTIPLE_REV);
		}
	}
/* END [Jackal_Chen][2012/01/16][Modify logic for AC] */

#if (PEGA_SMT_SOLUTION == 0)
	ret = pega_isl9519q_chg_prepare(hw_chg, in_cur, chg_cur);

#elif (PEGA_SMT_SOLUTION == 1)
    if (chg_current == 500)     // SDP
    {
        // [2012/01/03] Jackal - Modify input current to 0 for SMT version
        ret = configure_isl_input_reg(0);
        input_current = 0;
/*        ret = configure_isl_input_reg(in_cur);
        input_current = in_cur;*/
        // [2012/01/03] Jackal - Modify input current to 0 for SMT version End
        charge_current = 0;
    }
    /* BEGIN [Jackal_Chen][2012/01/16][Modify logic for AC/dock] */
#ifdef MSM_CHARGER_ENABLE_DOCK
    else if(chg_current == INSERT_DK_MAX_CURR) {
        PEGA_DBG_H("[CHG] SMT version with dock, set input/charge current.\n");
        ret = pega_isl9519q_chg_prepare(hw_chg, in_cur, chg_cur);
    }
#endif /* MSM_CHARGER_ENABLE_DOCK */
    /* END [Jackal_Chen][2012/01/16][Modify logic for AC/dock] */
    else                        // Wall/AC charger
    {
        ret = pega_isl9519q_chg_prepare(hw_chg, in_cur, chg_cur);
    }

#elif (PEGA_SMT_SOLUTION == 2)
	if (chg_current == 500)     // SDP
	{
		ret = configure_isl_control_reg(true);
		input_current = in_cur;
		charge_current = 128;
	}
	else                        // Wall/AC charger
	{
		ret = configure_isl_control_reg(false);
		ret = pega_isl9519q_chg_prepare(hw_chg, in_cur, chg_cur);
	}
#endif

	if (ret) {
		dev_err(&isl_chg->client->dev,
			"%s coulnt write to current_reg\n", __func__);
		goto out;
	}

	dev_dbg(&isl_chg->client->dev, "%s starting timed work\n",
							__func__);

	/* BEGIN [Terry_Tzeng][2012/02/09][Notify starting charging is finished to ec controllor] */
#ifdef MSM_CHARGER_ENABLE_DOCK
        if(chg_current == INSERT_DK_MAX_CURR)
	{
		PEGA_DBG_H("[CHG] Notify table starting charging is finished \n");
		echger_notify_tb_start_chg_finish(in_cur, chg_cur);
	}
#endif /* MSM_CHARGER_ENABLE_DOCK */
	/* END [Terry_Tzeng][2012/02/09][Notify starting charging is finished to ec controllor] */


#if defined PEGA_SMT_BUILD
	schedule_delayed_work(&isl_chg->charge_work, (HZ) * 3);
#else
	schedule_delayed_work(&isl_chg->charge_work,
						ISL9519_CHG_PERIOD);
#endif
	isl_chg->charging = true;
	PEGA_DBG_H("[CHG] .. after %s, is_charge= %d\n",__func__, isl_chg->charging);

out:
	return ret;
}

static int isl9519q_stop_charging(struct msm_hardware_charger *hw_chg)
{
	struct isl9519q_struct *isl_chg;
	int ret = 0;

	isl_chg = container_of(hw_chg, struct isl9519q_struct, adapter_hw_chg);
	PEGA_DBG_H("[CHG] %s, is_charge= %d\n",__func__, isl_chg->charging);

	if (!(isl_chg->charging))
		/* we arent charging */
		return 0;

	dev_dbg(&isl_chg->client->dev, "%s\n", __func__);

#if (PEGA_SMT_SOLUTION == 0)
	ret = isl9519q_write_reg(isl_chg->client, CHG_CURRENT_REG, 0);
#elif (PEGA_SMT_SOLUTION == 1)
    if (input_current > 512)       // Wall/AC charger
    {
        ret = isl9519q_write_reg(isl_chg->client, CHG_CURRENT_REG, 0);
    }
#elif (PEGA_SMT_SOLUTION == 2)
	ret = configure_isl_control_reg(true);
#endif

	if (ret) {
		dev_err(&isl_chg->client->dev,
			"%s coulnt write to current_reg\n", __func__);
		goto out;
	}

	/* BEGIN [Jackal_Chen][2012/03/29][Set input current to supply system if stop_charging() after CHG_DONE] */
	/* BEGIN [Jackal_Chen][2012/03/26][Set input current to 0 when stop_charging] */
	/* BEGIN [Jackal_Chen][2012/04/24][Set input current to supply system if stop_charging() after safety timer] */
	if(pega_chg_read_irq_stat(isl_chg->client->irq)) {
		if(POWER_SUPPLY_STATUS_FULL == msm_chg_get_batt_status())
		{
			PEGA_DBG_H("[CHG] Battery full, keep input current to support system. \n");
		}
		else
		{
			PEGA_DBG_H("[CHG] Battery not full but still has external power, keep input current to support system. \n");
		}

		/* BEGIN [Jackal_Chen][2012/05/14][We just consider Pad only condition, exclude Dock in condition] */
		if(gpio_get_value_cansleep(dock_in_gpio_number)) {  // Pad only condition
			ret = isl9519q_write_reg(isl_chg->client, INPUT_CURRENT_REG, input_current);
			if (!ret) {
				PEGA_DBG_H("[CHG] .. set INPUT_CURRENT_REG = %d. \n", input_current);
			} else {
				PEGA_DBG_H("[CHG_ERR] .. couldnt set INPUT_CURRENT_REG to %d , ret= %d \n", input_current, ret);
			}
		} else {  // Dock in condition, don't set input current
			PEGA_DBG_H("[CHG] Dock in condition, don't set input current. \n");
		}
		/* END [Jackal_Chen][2012/05/14][We just consider Pad only condition, exclude Dock in condition] */
	} else {
		ret = isl9519q_write_reg(isl_chg->client, INPUT_CURRENT_REG, 0);
		if (!ret) {
			PEGA_DBG_H("[CHG] .. set INPUT_CURRENT_REG = 0. \n");
		} else {
			PEGA_DBG_H("[CHG_ERR] .. couldnt set INPUT_CURRENT_REG to 0 , ret= %d \n", ret);
		}
		input_current = 0;
	}
	/* END [Jackal_Chen][2012/04/24][Set input current to supply system if stop_charging() after safety timer] */
	/* END [Jackal_Chen][2012/03/26][Set input current to 0 when stop_charging] */

	charge_current = 0;
	//input_current = 0;
	prev_volt_idx = -1;
	/* END [Jackal_Chen][2012/03/29][Set input current to supply system if stop_charging() after CHG_DONE] */

	/* BEGIN [Jackal_Chen][2012/04/05][Non-formal charger support] */
#if (PEGA_CHG_SUPPORT_NON_FORMAL_CHARGER == 1)
	wall_chg_type = WALL_CHG_TYPE_OTHER;
#endif
	/* END [Jackal_Chen][2012/04/05][Non-formal charger support] */

	isl_chg->charging = false;
	PEGA_DBG_H("[CHG] .. after %s, is_charge= %d\n",__func__, isl_chg->charging);

	cancel_delayed_work(&isl_chg->charge_work);
out:
	return ret;
}

static int isl9519q_charging_switched(struct msm_hardware_charger *hw_chg)
{
	struct isl9519q_struct *isl_chg;

	isl_chg = container_of(hw_chg, struct isl9519q_struct, adapter_hw_chg);
	dev_dbg(&isl_chg->client->dev, "%s\n", __func__);
	return 0;
}

static int isl9519q_get_chg_current(struct msm_hardware_charger *hw_chg)
{
	struct isl9519q_struct *isl_chg;
	u16 curr;
	int ret;

	isl_chg = container_of(hw_chg, struct isl9519q_struct, adapter_hw_chg);
	ret = isl9519q_read_reg(isl_chg->client, CHG_CURRENT_REG, &curr);
	if (ret < 0)
		return 0;
	else
		return curr;
}

static int isl9519q_get_in_current(struct msm_hardware_charger *hw_chg)
{
	struct isl9519q_struct *isl_chg;
	u16 curr;
	int ret;

	isl_chg = container_of(hw_chg, struct isl9519q_struct, adapter_hw_chg);
	ret = isl9519q_read_reg(isl_chg->client, INPUT_CURRENT_REG, &curr);
	if (ret < 0)
		return 0;
	else
		return curr;
}


/* BEGIN [Jackal_Chen][2012/01/11][Add detected dock plug in/out callback function from dock_interrupt function in board_msm8960.c] */
/* dock_in : 0 -> Dock plug out , dock_in : 1 -> Dock plug in */
void Notify_charging_dock_in_out(void)
{
	PEGA_DBG_H("[CHG] %s, Receive dock plug in/out \n", __func__);

	if(temp_isl_chg != NULL) {
		schedule_delayed_work(&temp_isl_chg->docking_interrupt, msecs_to_jiffies(0));
	} else {
		PEGA_DBG_H("[CHG] %s, temp_isl_chg is NULL.\n", __func__);
	}
}

static void docking_in_out_check(struct work_struct *work)
{
	int dock_in = 0;
	PEGA_DBG_H("[CHG] %s, Handle dock plug in/out\n", __func__);
	/* BEGIN [Jackal_Chen][2012/04/09][Use HW_ID value to distinguish GPIO number of dock_in pin] */
	dock_in = !gpio_get_value_cansleep(dock_in_gpio_number);
	/* END [Jackal_Chen][2012/04/09][Use HW_ID value to distinguish GPIO number of dock_in pin] */
	PEGA_DBG_H("[CHG] %s, dock_in = %d\n", __func__, dock_in);
	if(dock_in) {
		// We just notify CHG_DOCK_OUT to pega_msm_charger. CHG_DOCK_IN event will be notified in device_id_check().
		msm_charger_notify_event(NULL, CHG_DOCK_INSERTED_EVENT);
	} else {
		msm_charger_notify_event(NULL, CHG_DOCK_REMOVED_EVENT);
	}
}
/* END [Jackal_Chen][2012/01/11][Add detected dock plug in/out callback function from dock_interrupt function in board_msm8960.c] */

/* BEGIN [Jackal_Chen][2012/04/18][Modify logic to get usb_in_valid status] */
#if (PEGA_CHG_SUPPORT_NON_FORMAL_CHARGER == 1)
/* BEGIN [Jackal_Chen][2012/04/23][Add function to set input current] */
static void set_input_current(struct isl9519q_struct *isl_chg, int in_curr)
{
	int ret;

	ret = isl9519q_write_reg(isl_chg->client, INPUT_CURRENT_REG, in_curr);
	if (!ret) {
		PEGA_DBG_NON_FORMAL_SUPPORT("[CHG] .. set INPUT_CURRENT_REG = %d. \n", in_curr);
	} else {
		PEGA_DBG_H("[CHG_ERR] .. couldnt set INPUT_CURRENT_REG to %d , ret= %d \n", in_curr, ret);
	}
}
/* END [Jackal_Chen][2012/04/23][Add function to set input current] */

static int get_usb_in_valid_status(struct isl9519q_struct *isl_chg, int cbl_pwr_status)
{
	int usb_in_irq_value = 2;
	int usb_in_check_limit = PEGA_CHG_USB_IN_RETRY_COUNT_MAX;
//	int ret;

	if(!cbl_pwr_status) {
		/*ret = isl9519q_write_reg(isl_chg->client, INPUT_CURRENT_REG, 0);
		if (!ret) {
			PEGA_DBG_NON_FORMAL_SUPPORT("[CHG] .. set INPUT_CURRENT_REG = 0. \n");
		} else {
			PEGA_DBG_H("[CHG_ERR] .. couldnt set INPUT_CURRENT_REG to 0 , ret= %d \n", ret);
		}*/
		set_input_current(isl_chg, 0);
		/* BEGIN [Jackal_Chen][2012/04/27][Since some non-formal charger drop usb_in_valid pin too fast, add check time] */
		msleep(300);
		/* END [Jackal_Chen][2012/04/27][Since some non-formal charger drop usb_in_valid pin too fast, add check time] */
		if(!pega_chg_read_irq_stat(PM_USB_IN_VALID_IRQ_HDL_8921)) {
			PEGA_DBG_NON_FORMAL_SUPPORT("[CHG] usb_in_irq_value = 0, cbl_pwr = %d \n", pega_chg_read_irq_stat(isl_chg->client->irq));
			msleep(50);
			while(!pega_chg_read_irq_stat(PM_USB_IN_VALID_IRQ_HDL_8921) && usb_in_check_limit > 0) {
				PEGA_DBG_NON_FORMAL_SUPPORT("[CHG] usb_in_valid = 0, cbl_pwr = %d \n", pega_chg_read_irq_stat(isl_chg->client->irq));
				/*ret = isl9519q_write_reg(isl_chg->client, INPUT_CURRENT_REG, 0);
				if (!ret) {
					PEGA_DBG_NON_FORMAL_SUPPORT("[CHG] .. set INPUT_CURRENT_REG = 0. \n");
				} else {
					PEGA_DBG_H("[CHG_ERR] .. couldnt set INPUT_CURRENT_REG to 0 , ret= %d \n", ret);
				}*/
				PEGA_DBG_NON_FORMAL_SUPPORT("[CHG] usb_in_irq_value = 0, retry count = %d\n", PEGA_CHG_USB_IN_RETRY_COUNT_MAX - usb_in_check_limit + 1);
				if(usb_in_check_limit > 3)
					msleep(50);
				else
					msleep(20);
				usb_in_check_limit--;
			}
			if(usb_in_check_limit <= 0)
				usb_in_irq_value = 0;
			else
				usb_in_irq_value = 1;
		}
		else {
			msleep(50);
			if(pega_chg_read_irq_stat(PM_USB_IN_VALID_IRQ_HDL_8921)) {
				PEGA_DBG_NON_FORMAL_SUPPORT("[CHG] usb_in_valid status = 1, USB may really plug in. \n");
				usb_in_irq_value = 1;
			}
			else {
				PEGA_DBG_NON_FORMAL_SUPPORT("[CHG] usb_in_valid status = 0, USB may plig out. \n");
				usb_in_irq_value = 0;
			}
		}
	}
	else {
		msleep(100);
		if(pega_chg_read_irq_stat(PM_USB_IN_VALID_IRQ_HDL_8921)) {
			usb_in_irq_value = 1;
		}
		else {
			usb_in_irq_value = 0;
		}
	}
	return usb_in_irq_value;
}
#endif
/* END [Jackal_Chen][2012/04/18][Modify logic to get usb_in_valid status] */


#if (PEGA_INT_HANDLER_SOLUTION == 1)    /* start cable detect work*/
/* BEGIN [Jackal_Chen][2012/04/23][More non-formal charger type support] */
static void cable_detect_isl9519q_check(struct work_struct *pega_isl9519_work)
{
	struct isl9519q_struct *isl_chg;
	int	value1, value2;
	int retry_limit = 10;
	/* BEGIN [2012/03/28] Jackal - Set input current to 0 first when insert cable */
	//int ret;
	/* END [2012/03/28] Jackal - Set input current to 0 first when insert cable */
	/* BEGIN [Jackal_Chen][2012/04/18][Modify logic to get usb_in_valid status] */
#if (PEGA_CHG_SUPPORT_NON_FORMAL_CHARGER == 1)
	int usb_in_irq_value = 2;
	//int dc_in_valid_irq_value = 2;
#endif
	/* END [Jackal_Chen][2012/04/18][Modify logic to get usb_in_valid status] */

	isl_chg = container_of(pega_isl9519_work, struct isl9519q_struct,check_cable_detect_work.work);
	isl_chg = i2c_get_clientdata(isl_chg->client);

	do {
		//value1 = gpio_get_value_cansleep(isl_chg->valid_n_gpio);
		value1 = pega_chg_read_irq_stat(isl_chg->client->irq);

		msleep(50);
		//value2 = gpio_get_value_cansleep(isl_chg->valid_n_gpio);
		value2 = pega_chg_read_irq_stat(isl_chg->client->irq);

	} while (value1 != value2 && retry_limit-- > 0);

	PEGA_DBG_NON_FORMAL_SUPPORT("[CHG] cable_detect: GPIO= %d, present= %d, retry_count= %d\n", value1, isl_chg->present, (10-retry_limit));

	/* BEGIN [Jackal_Chen][2012/04/20][Modify logic for non-formal charger stability] */
	/*if (value1 == isl_chg->present) {
		PEGA_DBG_NON_FORMAL_SUPPORT("[CHG] %s, Don't do anything since CBL_PWR value = isl_chg->present = %d \n", __func__, value1);
	}*/
	/* END [Jackal_Chen][2012/04/20][Modify logic for non-formal charger stability] */

	/* BEGIN [Jackal_Chen][2012/04/18][Modify logic to get usb_in_valid status] */
	/* BEGIN [Jackal_Chen][2012/04/05][Non-formal charger support] */
#if (PEGA_CHG_SUPPORT_NON_FORMAL_CHARGER == 1)
	usb_in_irq_value = get_usb_in_valid_status(isl_chg, value1);

	/* BEGIN [Jackal_Chen][2012/04/17][dc_in_valid is not used currently, mask it] */
	//dc_in_valid_irq_value = pega_chg_read_irq_stat(PM_DC_IN_VALID_IRQ_HDL_8921);
	//PEGA_DBG_NON_FORMAL_SUPPORT("[CHG] usb_in_irq_value = %d, dc_in_valid_irq_value = %d\n", usb_in_irq_value, dc_in_valid_irq_value);
	PEGA_DBG_NON_FORMAL_SUPPORT("[CHG] %s, usb_in_irq_value = %d \n", __func__, usb_in_irq_value);
	/* END [Jackal_Chen][2012/04/17][dc_in_valid is not used currently, mask it] */
	/* BEGIN [Jackal_Chen][2012/04/27][Test] */
	if(!usb_in_irq_value) {
		if(pega_chg_read_irq_stat(isl_chg->client->irq)) {
			PEGA_DBG_NON_FORMAL_SUPPORT("[CHG] %s, usb_in_irq_value = 0, but CBL_PWR value = 1, set usb_in_irq_value to 1 \n", __func__);
			usb_in_irq_value = 1;
		}
	}
	/* END [Jackal_Chen][2012/04/27][Test] */
#endif
	/* END [Jackal_Chen][2012/04/05][Non-formal charger support] */
	/* END [Jackal_Chen][2012/04/18][Modify logic to get usb_in_valid status] */

	if (!value1) {
		/* BEGIN [Jackal_Chen][2012/04/05][Non-formal charger support] */
#if (PEGA_CHG_SUPPORT_NON_FORMAL_CHARGER == 1)
		if (!usb_in_irq_value) {
			PEGA_DBG_H("[CHG] %s ... < remove cable > \n", __func__);
			msm_charger_notify_event(&isl_chg->adapter_hw_chg, CHG_REMOVED_EVENT);
			if (is_otg_plugin) {
				otg_disable_input_current(!is_otg_plugin);
			}
			wall_chg_retry_count = PEGA_CHG_WALL_RETRY_COUNT_MAX;
			/* BEGIN [Jackal_Chen][2012/04/20][Modify logic for non-formal charger stability] */
			isl_chg->present = 0;
			/* END [Jackal_Chen][2012/04/20][Modify logic for non-formal charger stability] */
		} else {
			PEGA_DBG_NON_FORMAL_SUPPORT("[CHG] %s ... CBL_PWR drop but cable still insert.\n", __func__);
			if(WALL_CHG_TYPE_2A == wall_chg_type) {
				set_input_current(isl_chg, 0);
#if (PEGA_CHG_SUPPORT_MORE_CHARGER_TYPE == 1)
				PEGA_DBG_H("[CHG] Set wall_chg_type to WALL_CHG_TYPE_1500MA\n");
				wall_chg_type = WALL_CHG_TYPE_1500MA;
#else
				PEGA_DBG_H("[CHG] Set wall_chg_type to WALL_CHG_TYPE_1A\n");
				wall_chg_type = WALL_CHG_TYPE_1A;
#endif
				wall_chg_retry_count = PEGA_CHG_WALL_RETRY_COUNT_MAX;
			}
#if (PEGA_CHG_SUPPORT_MORE_CHARGER_TYPE == 1)
			else if(WALL_CHG_TYPE_1500MA == wall_chg_type) {
				set_input_current(isl_chg, 0);
				PEGA_DBG_H("[CHG] Set wall_chg_type to WALL_CHG_TYPE_1A\n");
				wall_chg_type = WALL_CHG_TYPE_1A;
				wall_chg_retry_count = PEGA_CHG_WALL_RETRY_COUNT_MAX;
			}
#endif
			else if(WALL_CHG_TYPE_1A == wall_chg_type) {
				set_input_current(isl_chg, 0);
				if(wall_chg_retry_count <= 0) {
					PEGA_DBG_H("[CHG] Set wall_chg_type to WALL_CHG_TYPE_700MA\n");
					wall_chg_type = WALL_CHG_TYPE_700MA;
					wall_chg_retry_count = PEGA_CHG_WALL_RETRY_COUNT_MAX;
				} else {
					wall_chg_retry_count--;
					PEGA_DBG_NON_FORMAL_SUPPORT("[CHG] wall_chg_type = WALL_CHG_TYPE_1A, retry count = %d\n", PEGA_CHG_WALL_RETRY_COUNT_MAX - wall_chg_retry_count);
				}
			}
#if (PEGA_CHG_SUPPORT_MORE_CHARGER_TYPE == 1)
			else if(WALL_CHG_TYPE_700MA == wall_chg_type) {
				set_input_current(isl_chg, 0);
				if(wall_chg_retry_count <= 0) {
					PEGA_DBG_H("[CHG] Set wall_chg_type to WALL_CHG_TYPE_500MA\n");
					wall_chg_type = WALL_CHG_TYPE_500MA;
					wall_chg_retry_count = PEGA_CHG_WALL_RETRY_COUNT_MAX;
				} else {
					wall_chg_retry_count--;
					PEGA_DBG_NON_FORMAL_SUPPORT("[CHG] wall_chg_type = WALL_CHG_TYPE_700MA, retry count = %d\n", PEGA_CHG_WALL_RETRY_COUNT_MAX - wall_chg_retry_count);
				}
			}
#endif
			else {
				/* BEGIN [Jackal_Chen][2012/05/04][Modify logic for non-formal charger to enhance stability] */
				/* BEGIN [Jackal_Chen][2012/04/11][Modification for non-formal charger support logic] */
#if (PEGA_CHG_SUPPORT_MORE_CHARGER_TYPE == 1)
				if(WALL_CHG_TYPE_500MA == wall_chg_type)
				{
					PEGA_DBG_H("[CHG_ERR] Already set wall_chg_type to WALL_CHG_TYPE_500MA, but still can't charge.\n");
				}
#else
				if(WALL_CHG_TYPE_700MA == wall_chg_type)
				{
					PEGA_DBG_H("[CHG_ERR] Already set wall_chg_type to WALL_CHG_TYPE_700MA, but still can't charge.\n");
				}
#endif
				else
				{
					/* BEGIN [Jackal_Chen][2012/05/11][only_keep_input_current_flag is set only for Pad, exclude Dock] */
					if(WALL_CHG_TYPE_OTHER == wall_chg_type && POWER_SUPPLY_STATUS_FULL == msm_chg_get_batt_status() && gpio_get_value_cansleep(dock_in_gpio_number)) {
						PEGA_DBG_H("[CHG] Battery full but CBL_PWR drop with cable(wall charger) in, just keep USB cable input current.\n");
						only_keep_input_current_flag = true;
						wall_chg_retry_count = PEGA_CHG_WALL_RETRY_COUNT_MAX;
						return;
					} else {
						only_keep_input_current_flag = false;
						PEGA_DBG_H("[CHG_ERR] Not wall charger type.\n");
					}
					/* END [Jackal_Chen][2012/05/11][only_keep_input_current_flag is set only for Pad, exclude Dock] */
				}
				PEGA_DBG_H("[CHG] %s ... < remove cable > \n", __func__);
				wall_chg_retry_count = PEGA_CHG_WALL_RETRY_COUNT_MAX;
				msm_charger_notify_event(&isl_chg->adapter_hw_chg, CHG_REMOVED_EVENT);
				/* BEGIN [Jackal_Chen][2012/04/20][Modify logic for non-formal charger stability] */
				isl_chg->present = 0;
				/* END [Jackal_Chen][2012/04/20][Modify logic for non-formal charger stability] */
				/* END [Jackal_Chen][2012/04/11][Modification for non-formal charger support logic] */
				/* END [Jackal_Chen][2012/05/04][Modify logic for non-formal charger to enhance stability] */
			}
		}
#else
		if (isl_chg->present == 1) {
			PEGA_DBG_H("[CHG] %s ... < remove cable > \n", __func__);
			msm_charger_notify_event(&isl_chg->adapter_hw_chg,
				CHG_REMOVED_EVENT);
			isl_chg->present = 0;
			if (is_otg_plugin) {
				otg_disable_input_current(!is_otg_plugin);
			}
		}
#endif
		/* END [Jackal_Chen][2012/04/05][Non-formal charger support] */
	} else {
		/* BEGIN [Jackal_Chen][2012/04/05][Non-formal charger support] */
#if (PEGA_CHG_SUPPORT_NON_FORMAL_CHARGER == 1)
		if(usb_in_irq_value) {
			if(WALL_CHG_TYPE_2A == wall_chg_type) {
				PEGA_DBG_NON_FORMAL_SUPPORT("[CHG] .. wall_chg_type = WALL_CHG_TYPE_2A. \n");
				set_input_current(isl_chg, isl_chg->usbcgr_current);
				input_current = isl_chg->usbcgr_input_current;
				msleep(100);
			}
#if (PEGA_CHG_SUPPORT_MORE_CHARGER_TYPE == 1)
			else if(WALL_CHG_TYPE_1500MA == wall_chg_type) {
				PEGA_DBG_NON_FORMAL_SUPPORT("[CHG] .. wall_chg_type = WALL_CHG_TYPE_1500MA. \n");
				set_input_current(isl_chg, PEGA_WALL_CHG_TYPE_1500MA_INPUT_CURR);
				input_current = PEGA_WALL_CHG_TYPE_1500MA_INPUT_CURR;
				msleep(100);
			}
#endif
			else if(WALL_CHG_TYPE_1A == wall_chg_type) {
				PEGA_DBG_NON_FORMAL_SUPPORT("[CHG] .. wall_chg_type = WALL_CHG_TYPE_1A. \n");
				set_input_current(isl_chg, PEGA_WALL_CHG_TYPE_1A_INPUT_CURR);
				input_current = PEGA_WALL_CHG_TYPE_1A_INPUT_CURR;
				msleep(100);
			}
			else if(WALL_CHG_TYPE_700MA == wall_chg_type) {
				PEGA_DBG_NON_FORMAL_SUPPORT("[CHG] .. wall_chg_type = WALL_CHG_TYPE_700MA. \n");
				set_input_current(isl_chg, PEGA_WALL_CHG_TYPE_700MA_INPUT_CURR);
				input_current = PEGA_WALL_CHG_TYPE_700MA_INPUT_CURR;
				msleep(100);
			}
#if (PEGA_CHG_SUPPORT_MORE_CHARGER_TYPE == 1)
			else if(WALL_CHG_TYPE_500MA == wall_chg_type) {
				PEGA_DBG_NON_FORMAL_SUPPORT("[CHG] .. wall_chg_type = WALL_CHG_TYPE_500MA. \n");
				set_input_current(isl_chg, PEGA_WALL_CHG_TYPE_500MA_INPUT_CURR);
				input_current = PEGA_WALL_CHG_TYPE_500MA_INPUT_CURR;
				msleep(100);
			}
#endif
			/* BEGIN [Jackal_Chen][2012/05/11][only_keep_input_current_flag is set only for Pad, exclude Dock] */
			/* BEGIN [Jackal_Chen][2012/05/04][Modify logic for non-formal charger to enhance stability] */
			else if(only_keep_input_current_flag && gpio_get_value_cansleep(dock_in_gpio_number)) {
				PEGA_DBG_H("[CHG] Keep input current = 512 mA(This is for Pad only).\n");
				only_keep_input_current_flag = false;
				set_input_current(isl_chg, 512);
			}
			/* END [Jackal_Chen][2012/05/04][Modify logic for non-formal charger to enhance stability] */
			/* END [Jackal_Chen][2012/05/11][only_keep_input_current_flag is set only for Pad, exclude Dock] */
			else {

				PEGA_DBG_H("[CHG] %s ... < insert cable > \n", __func__);
				if(gpio_get_value_cansleep(dock_in_gpio_number))
					set_input_current(isl_chg, 0);

				msm_charger_notify_event(&isl_chg->adapter_hw_chg, CHG_INSERTED_EVENT);
				/* BEGIN [Jackal_Chen][2012/04/20][Modify logic for non-formal charger stability] */
				isl_chg->present = 1;
				/* END [Jackal_Chen][2012/04/20][Modify logic for non-formal charger stability] */
			}
		} else {
			PEGA_DBG_H("[CHG] %s ... < insert cable > \n", __func__);
			if(gpio_get_value_cansleep(dock_in_gpio_number))
				set_input_current(isl_chg, 0);

			msm_charger_notify_event(&isl_chg->adapter_hw_chg, CHG_INSERTED_EVENT);
			/* BEGIN [Jackal_Chen][2012/04/20][Modify logic for non-formal charger stability] */
			isl_chg->present = 1;
			/* END [Jackal_Chen][2012/04/20][Modify logic for non-formal charger stability] */
		}
#else
		if (isl_chg->present == 0) {
			PEGA_DBG_H("[CHG] %s ... < insert cable > \n", __func__);
			/* BEGIN [2012/03/28] Jackal - Set input current to 0 first when insert cable */
			/*ret = isl9519q_write_reg(isl_chg->client, INPUT_CURRENT_REG, 0);
			if (!ret) {
				PEGA_DBG_H("[CHG] .. set INPUT_CURRENT_REG = 0. \n");
			} else {
				PEGA_DBG_H("[CHG_ERR] .. couldnt set INPUT_CURRENT_REG to 0 , ret= %d \n", ret);
			}*/
			if(gpio_get_value_cansleep(dock_in_gpio_number))
				set_input_current(isl_chg, 0);
			/* END [2012/03/28] Jackal - Set input current to 0 first when insert cable */
			msm_charger_notify_event(&isl_chg->adapter_hw_chg,
				CHG_INSERTED_EVENT);
			isl_chg->present = 1;
		}
#endif
		/* END [Jackal_Chen][2012/04/05][Non-formal charger support] */
	}
}
/* END [Jackal_Chen][2012/04/23][More non-formal charger type support] */
/*end pega isl charge detect*/

static irqreturn_t isl_valid_handler(int irq, void *dev_id)
{
	struct isl9519q_struct *isl_chg;
	struct i2c_client *client = dev_id;

	isl_chg = i2c_get_clientdata(client);

	/* disable irq, this gets enabled in the workqueue */
	/* BEGIN [Jackal_Chen][2012/07/19][Add check for isl_chg to avoid NULL pointer crash] */
	if(isl_chg != NULL && &isl_chg->check_cable_detect_work != NULL) {
		cancel_delayed_work_sync(&isl_chg->check_cable_detect_work);
		schedule_delayed_work(&isl_chg->check_cable_detect_work,msecs_to_jiffies(0));  //debounce time 0ms
	} else {
		PEGA_DBG_H("[CHG] %s, isl_chg is NULL, don't do anything!\n", __func__);
	}
	/* END [Jackal_Chen][2012/07/19][Add check for isl_chg to avoid NULL pointer crash] */
	return IRQ_HANDLED;
}

/* BEGIN [Jackal_Chen][2012/01/16][Add device_id pin config/handler] */
/* BEGIN [Jackal_Chen][2012/04/10][Detect Y-cable by adc] */
#if 0
static void device_id_check(struct work_struct *pega_device_id_work)
{
	struct isl9519q_struct *isl_chg;
	int value1, value2;
	int retry_limit = 10;


	isl_chg = container_of(pega_device_id_work, struct isl9519q_struct,check_device_id_work.work);
	isl_chg = i2c_get_clientdata(isl_chg->client);

	do {
		value1 = gpio_get_value_cansleep(isl_chg->valid_n_gpio);
		//value1 = pega_chg_read_irq_stat(isl_chg->client->irq);

		msleep(50);
		value2 = gpio_get_value_cansleep(isl_chg->valid_n_gpio);
		//value2 = pega_chg_read_irq_stat(isl_chg->client->irq);

	} while (value1 != value2 && retry_limit-- > 0);

	PEGA_DBG_H("[CHG] %s, device_id gpio number = %d.\n", __func__, isl_chg->valid_n_gpio);
	PEGA_DBG_H("[CHG] device_id_check: GPIO= %d, retry_count= %d\n", value1, (10-retry_limit));

	if (!value1) {
		int dock_in = 0;
		PEGA_DBG_H("[CHG] %s, device_id status is low(dock in or 12V AC charger in).\n", __func__);
		msleep(100);  // Wait for a while to get dock_in pin status
		/* BEGIN [Jackal_Chen][2012/04/09][Use HW_ID value to distinguish GPIO number of dock_in pin] */
		dock_in = !gpio_get_value_cansleep(dock_in_gpio_number);
		/* END [Jackal_Chen][2012/04/09][Use HW_ID value to distinguish GPIO number of dock_in pin] */
		if(dock_in) {
			PEGA_DBG_H("[CHG] %s, <  Dock in  >\n", __func__);
			//msm_charger_notify_event(NULL, CHG_DOCK_INSERTED_EVENT);
		} else {
			PEGA_DBG_H("[CHG] %s, <  12V AC in  >\n", __func__);
		}
	} else {
		PEGA_DBG_H("[CHG] %s, device_id status is high.\n", __func__);
	}
}

static irqreturn_t device_id_handler(int irq, void *dev_id)
{
	struct isl9519q_struct *isl_chg;
	struct i2c_client *client = dev_id;
	PEGA_DBG_H("[CHG] %s, detect device_id changed.\n", __func__);

	isl_chg = i2c_get_clientdata(client);

	/* disable irq, this gets enabled in the workqueue */
//	cancel_delayed_work_sync(&isl_chg->check_device_id_work);
	schedule_delayed_work(&isl_chg->check_device_id_work,msecs_to_jiffies(0));  //debounce time 0ms
	return IRQ_HANDLED;
}
#endif
/* END [Jackal_Chen][2012/04/10][Detect Y-cable by adc] */
/* END [Jackal_Chen][2012/01/16][Add device_id pin config/handler] */
#else   //PEGA_INT_HANDLER_SOLUTION == 0
static irqreturn_t isl_valid_handler(int irq, void *dev_id)
{
	//int val;
	int val2;
	struct isl9519q_struct *isl_chg;
	struct i2c_client *client = dev_id;

	isl_chg = i2c_get_clientdata(client);
	//val = gpio_get_value_cansleep(isl_chg->valid_n_gpio);

	val2 = pega_chg_read_irq_stat(client->irq);

	if (val2 < 0) {
		dev_err(&isl_chg->client->dev,
			"%s gpio_get_value failed for %d ret=%d\n", __func__,
			client->irq, val2);
		goto err;
	}
	dev_dbg(&isl_chg->client->dev, "%s val=%d\n", __func__, val2);

	PEGA_DBG_H("[CHG] ... val2= %d, client->irq=%d, present= %d \n", val2, client->irq, isl_chg->present);
	//PEGA_DBG_H("[CHG] ... val= %d, present= %d \n", val, isl_chg->present);
	if (!val2) {
		if (isl_chg->present == 1) {
			PEGA_DBG_H("[CHG] %s ... < remove cable > \n", __func__);
			msm_charger_notify_event(&isl_chg->adapter_hw_chg,
						 CHG_REMOVED_EVENT);
			isl_chg->present = 0;
			if (is_otg_plugin) {
				otg_disable_input_current(!is_otg_plugin);
			}
		}

	} else {
		if (isl_chg->present == 0) {
			PEGA_DBG_H("[CHG] %s ... < insert cable > \n", __func__);
			msm_charger_notify_event(&isl_chg->adapter_hw_chg,
						 CHG_INSERTED_EVENT);
			isl_chg->present = 1;
		}
	}
err:
	return IRQ_HANDLED;
}
#endif

#define MAX_VOLTAGE_REG_MASK  0x3FF0
#define MIN_VOLTAGE_REG_MASK  0x3F00
#define DEFAULT_MAX_VOLTAGE_REG_VALUE	0x1070
#define DEFAULT_MIN_VOLTAGE_REG_VALUE	0x0D00

static int __devinit isl9519q_probe(struct i2c_client *client,
				    const struct i2c_device_id *id)
{
	struct isl_platform_data *pdata;
	struct isl9519q_struct *isl_chg;
	int ret;
	PEGA_DBG_H("[CHG] %s \n", __func__);

	ret = 0;
	pdata = client->dev.platform_data;

	if (pdata == NULL) {
		dev_err(&client->dev, "%s no platform data\n", __func__);
		ret = -EINVAL;
		goto out;
	}

	if (!i2c_check_functionality(client->adapter,
				I2C_FUNC_SMBUS_WORD_DATA)) {
		ret = -EIO;
		goto out;
	}

	isl_chg = kzalloc(sizeof(*isl_chg), GFP_KERNEL);
	if (!isl_chg) {
		ret = -ENOMEM;
		goto out;
	}

#if (PEGA_INT_HANDLER_SOLUTION == 1)
	INIT_DELAYED_WORK(&isl_chg->check_cable_detect_work, cable_detect_isl9519q_check);
	/* BEGIN [Jackal_Chen][2012/01/16][Add device_id pin handler(bottom half)] */
	/* BEGIN [Jackal_Chen][2012/04/10][Detect Y-cable by adc] */
	//INIT_DELAYED_WORK(&isl_chg->check_device_id_work, device_id_check);
	/* END [Jackal_Chen][2012/04/10][Detect Y-cable by adc] */
	/* END [Jackal_Chen][2012/01/16][Add device_id pin handler(bottom half)] */
#endif
/* BEGIN [Jackal_Chen][2012/01/11][Initial delay work to handle docking interrupt handler in tophalf statement] */
	INIT_DELAYED_WORK(&isl_chg->docking_interrupt, docking_in_out_check);
/* END [Jackal_Chen][2012/01/11][Initial delay work to handle docking interrupt handler in tophalf statement] */
	INIT_DELAYED_WORK(&isl_chg->charge_work, isl9519q_charge);
	isl_chg->client = client;
	isl_chg->chgcurrent = pdata->chgcurrent;
	isl_chg->term_current = pdata->term_current;
	isl_chg->input_current = pdata->input_current;
	isl_chg->max_system_voltage = pdata->max_system_voltage;
	isl_chg->min_system_voltage = pdata->min_system_voltage;
	isl_chg->valid_n_gpio = pdata->valid_n_gpio;
	isl_chg->usb_current = pdata->usb_current;
	isl_chg->usb_input_current = pdata->usb_input_current;
	isl_chg->usbcgr_current = pdata->usbcgr_current;
	isl_chg->usbcgr_input_current = pdata->usbcgr_input_current;
	isl_chg->usb_current &= ~0x7F;
	isl_chg->usb_input_current &= ~0x7F;
	isl_chg->usbcgr_current &= ~0x7F;
	isl_chg->usbcgr_input_current &= ~0x7F;

/* BEGIN [Terry_Tzeng][2012/01/18][Set dock initial input current and charging current] */
	isl_chg->dock_chg_current = pdata->dock_chg_current;
	isl_chg->dock_input_current = pdata->dock_input_current;
/* END [Terry_Tzeng][2012/01/18][Set dock initial input current and charging current] */

	/* h/w ignores lower 7 bits of charging current and input current */
	isl_chg->chgcurrent &= ~0x7F;
	isl_chg->input_current &= ~0x7F;

	isl_chg->adapter_hw_chg.type = CHG_TYPE_AC;
	isl_chg->adapter_hw_chg.rating = 2;
	isl_chg->adapter_hw_chg.name = "isl-adapter";
	isl_chg->adapter_hw_chg.start_charging = isl9519q_start_charging;
	isl_chg->adapter_hw_chg.stop_charging = isl9519q_stop_charging;
	isl_chg->adapter_hw_chg.charging_switched = isl9519q_charging_switched;
	isl_chg->adapter_hw_chg.get_chg_current = isl9519q_get_chg_current;
	isl_chg->adapter_hw_chg.get_in_current = isl9519q_get_in_current;
/* BEGIN [Terry_Tzeng][2012/03/21][Set periodically charging current callback function in suspend status] */
#ifdef MSM_CHARGER_ENABLE_DOCK
	isl_chg->adapter_hw_chg.set_period_chg_curr_for_dk = isl9519q_set_periodically_chg_curr_for_insert_dk;
#endif /* MSM_CHARGER_ENABLE_DOCK */
/* END [Terry_Tzeng][2012/03/21][Set periodically charging current callback function in suspend status] */

	/* BEGIN [Jackal_Chen][2012/04/09][Use HW_ID value to distinguish GPIO number of dock_in pin] */
	if(get_pega_hw_board_version_status() > 2) {
		dock_in_gpio_number = MSM8960_DOCK_IN_GPIO_SIT2;
	}
	/* END [Jackal_Chen][2012/04/09][Use HW_ID value to distinguish GPIO number of dock_in pin] */

	if (pdata->chg_detection_config) {
		ret = pdata->chg_detection_config();
		if (ret) {
			dev_err(&client->dev, "%s valid config failed ret=%d\n",
				__func__, ret);
			goto free_isl_chg;
		}
	}
/*
	ret = gpio_request(pdata->valid_n_gpio, "isl_charger_valid");
	if (ret) {
		dev_err(&client->dev, "%s gpio_request failed for %d ret=%d\n",
			__func__, pdata->valid_n_gpio, ret);
		goto free_isl_chg;
	}
*/
	i2c_set_clientdata(client, isl_chg);
	INIT_DELAYED_WORK(&otg_isl_reconfigure, reconfigure_isl_control_reg);
#if defined PEGA_SMT_BUILD
	INIT_DELAYED_WORK(&ats_isl_reconfigure, reconfigure_isl_control_reg);
#endif

	temp_isl_chg = isl_chg;

#if (PEGA_SMT_SOLUTION == 0)
	if (is_otg_plugin)
	{
		ret = configure_isl_control_reg(true);
		if (ret)
			dev_err(&temp_isl_chg->client->dev,"%s coulnt write to control reg\n", __func__);
	}
#if 0       // could be remove if input current has been set in sbl3
	else
	{
		if (isl_chg->usbcgr_input_current) {
			ret = isl9519q_write_reg(isl_chg->client, INPUT_CURRENT_REG, isl_chg->usbcgr_input_current);

			if (ret) {
				dev_err(&client->dev,"%s couldnt write INPUT_CURRENT_REG ret=%d\n",	__func__, ret);
				goto free_irq;
			}
			else
				input_current = isl_chg->usbcgr_input_current;
		}
	}
#endif
#endif

	ret = msm_charger_register(&isl_chg->adapter_hw_chg);
	if (ret) {
		dev_err(&client->dev,
			"%s msm_charger_register failed for ret =%d\n",
			__func__, ret);
		goto free_gpio;
	}

    ret = request_threaded_irq(client->irq, NULL,
        isl_valid_handler,
        IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING,
        "isl_charger_valid", client);

	if (ret) {
		dev_err(&client->dev,
			"%s request_threaded_irq failed for %d ret =%d\n",
			__func__, client->irq, ret);
		goto unregister;
	}
	irq_set_irq_wake(client->irq, 1);

	isl_chg->max_system_voltage &= MAX_VOLTAGE_REG_MASK;
	isl_chg->min_system_voltage &= MIN_VOLTAGE_REG_MASK;
	if (isl_chg->max_system_voltage == 0)
		isl_chg->max_system_voltage = DEFAULT_MAX_VOLTAGE_REG_VALUE;
	if (isl_chg->min_system_voltage == 0)
		isl_chg->min_system_voltage = DEFAULT_MIN_VOLTAGE_REG_VALUE;

	ret = isl9519q_write_reg(isl_chg->client, MAX_SYS_VOLTAGE_REG,
			isl_chg->max_system_voltage);
	if (ret) {
		dev_err(&client->dev,
			"%s couldnt write to MAX_SYS_VOLTAGE_REG ret=%d\n",
			__func__, ret);
		goto free_irq;
	}

	ret = isl9519q_write_reg(isl_chg->client, MIN_SYS_VOLTAGE_REG,
			isl_chg->min_system_voltage);
	if (ret) {
		dev_err(&client->dev,
			"%s couldnt write to MIN_SYS_VOLTAGE_REG ret=%d\n",
			__func__, ret);
		goto free_irq;
	}
/* BEGIN [Jackal_Chen][2012/01/16][Add device_id pin config/handler] */
/* BEGIN [Jackal_Chen][2012/04/10][Detect Y-cable by adc] */
#if 0
	ret = gpio_request(pdata->valid_n_gpio, "device_id");
	if (ret) {
		dev_err(&client->dev, "%s gpio_request failed for %d ret=%d\n", __func__, pdata->valid_n_gpio, ret);
	} else {
		ret = gpio_direction_input(pdata->valid_n_gpio);
		if (ret < 0) {
			dev_err(&client->dev, "%s gpio_direct failed for %d ret=%d\n", __func__, pdata->valid_n_gpio, ret);
		} else {
			ret = request_irq(gpio_to_irq(pdata->valid_n_gpio), device_id_handler, IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING, "device_id", client);
			if (ret < 0) {
				dev_err(&client->dev, "%s irq_request failed for %d ret=%d\n", __func__, pdata->valid_n_gpio, ret);
				goto free_device_id_irq;
			}
		}
	}
#endif
/* END [Jackal_Chen][2012/04/10][Detect Y-cable by adc] */
/* END [Jackal_Chen][2012/01/16][Add device_id pin config/handler] */
#if 0
	ret = gpio_get_value_cansleep(isl_chg->valid_n_gpio);

	PEGA_DBG_H("%s, gpio_get_value_cansleep= %d\n", __func__, ret);
	if (ret < 0) {
		dev_err(&client->dev,
			"%s gpio_get_value failed for %d ret=%d\n", __func__,
			pdata->valid_n_gpio, ret);
		/* assume absent */
		ret = 1;
	}
	if (!ret) {
		msm_charger_notify_event(&isl_chg->adapter_hw_chg,
				CHG_INSERTED_EVENT);
		isl_chg->present = 1;
	}
#else
    ret = pega_chg_read_irq_stat(client->irq);
	PEGA_DBG_H("[CHG] %s, irq = %d, irq_stat = %d \n", __func__, client->irq, ret);

	if (ret < 0) {
		dev_err(&client->dev,
			"%s gpio_get_value failed for %d ret=%d\n", __func__,
			client->irq, ret);
		/* assume absent */
		ret = 0;
	}
	if (ret) {
		msm_charger_notify_event(&isl_chg->adapter_hw_chg,
				CHG_INSERTED_EVENT);
		isl_chg->present = 1;
	}
#endif

#if (PEGA_INT_HANDLER_SOLUTION == 0)
	PEGA_DBG_H("[CHG] %s, INT = 0 , ", __func__);
#elif (PEGA_INT_HANDLER_SOLUTION == 1)
	PEGA_DBG_H("[CHG] %s, INT = 1 , ", __func__);
#endif

#if (PEGA_SMT_SOLUTION == 0)
	PEGA_DBG_H("SMT = 0\n");
#elif (PEGA_SMT_SOLUTION == 1)
	PEGA_DBG_H("SMT = 1\n");
#elif (PEGA_SMT_SOLUTION == 2)
	PEGA_DBG_H("SMT = 2\n");
#endif

	PEGA_DBG_H("%s, done! chg_present= %d\n", __func__, isl_chg->present);

/* BEGIN [Terry_Tzeng][2012/01/09][Add dock resource marco] */
#ifdef MSM_CHARGER_ENABLE_DOCK
	msm_dock_ec_init(&isl_chg->adapter_hw_chg);
	pega_ecbatt_init();
#endif /* MSM_CHARGER_ENABLE_DOCK */
/* END [Terry_Tzeng][2012/01/09][Add dock resource marco] */
	return 0;

/* BEGIN [Jackal_Chen][2012/01/16][Add device_id pin config/handler] */
/* BEGIN [Jackal_Chen][2012/04/10][Detect Y-cable by adc] */
#if 0
free_device_id_irq:
	free_irq(gpio_to_irq(pdata->valid_n_gpio), NULL);
#endif
/* END [Jackal_Chen][2012/04/10][Detect Y-cable by adc] */
/* END [Jackal_Chen][2012/01/16][Add device_id pin config/handler] */
free_irq:
	free_irq(client->irq, NULL);
unregister:
	msm_charger_unregister(&isl_chg->adapter_hw_chg);
free_gpio:
//	gpio_free(pdata->valid_n_gpio);
free_isl_chg:
	kfree(isl_chg);
out:
	return ret;
}

static int __devexit isl9519q_remove(struct i2c_client *client)
{
	struct isl_platform_data *pdata;
	struct isl9519q_struct *isl_chg = i2c_get_clientdata(client);

	pdata = client->dev.platform_data;
	//gpio_free(pdata->valid_n_gpio);
	free_irq(client->irq, client);
	cancel_delayed_work_sync(&isl_chg->charge_work);
	msm_charger_notify_event(&isl_chg->adapter_hw_chg, CHG_REMOVED_EVENT);
	msm_charger_unregister(&isl_chg->adapter_hw_chg);

/* BEGIN [Terry_Tzeng][2012/01/09][Add dock resource marco] */
#ifdef MSM_CHARGER_ENABLE_DOCK
	msm_dock_ec_exit();
	pega_ecbatt_exit();
#endif /* MSM_CHARGER_ENABLE_DOCK */
/* END [Terry_Tzeng][2012/01/09][Add dock resource marco] */

	return 0;
}

static const struct i2c_device_id isl9519q_id[] = {
	{"isl9519q", 0},
	{},
};

#ifdef CONFIG_PM
static int isl9519q_suspend(struct device *dev)
{
	struct isl9519q_struct *isl_chg = dev_get_drvdata(dev);
/* BEGIN [Terry_Tzeng][2012/04/28][Handle suspend charging function for inserted dock charging] */
#ifdef MSM_CHARGER_ENABLE_DOCK
	int dock_in;
#endif /* MSM_CHARGER_ENABLE_DOCK */
/* END [Terry_Tzeng][2012/04/28][Handle suspend charging function for inserted dock charging] */

	dev_dbg(&isl_chg->client->dev, "%s\n", __func__);
	/*
	 * do not suspend while we are charging
	 * because we need to periodically update the register
	 * for charging to proceed
	 */
/* BEGIN [Terry_Tzeng][2012/04/11][If inserted dock charging statement, device will entry suspend mode] */
#ifdef MSM_CHARGER_ENABLE_DOCK
	dock_in = gpio_get_value_cansleep(dock_in_gpio_number);
	if (isl_chg->charging && dock_in)
#else
	if (isl_chg->charging)
#endif /* MSM_CHARGER_ENABLE_DOCK */
/* END [Terry_Tzeng][2012/04/11][If inserted dock charging statement, device will entry suspend mode] */
	{
		PEGA_DBG_H("[CHG_ERR] %s, isl_chg->charging= %d \n", __func__,isl_chg->charging);
		return -EBUSY;
	}

/* BEGIN [Terry_Tzeng][2012/04/28][Handle suspend charging function for inserted dock charging] */
#ifdef MSM_CHARGER_ENABLE_DOCK
	if(!dock_in)
	{
		int ret;
		/* BEGIN [Terry_Tzeng][2012/05/03][Handle suspend status] */
		echger_enable_suspend_flag();
		/* END [Terry_Tzeng][2012/05/03][Handle suspend status] */
		echger_suspend_charging_control();

		/* BEGIN [Terry_Tzeng][2012/05/21][Determine ec wakeup timer is enable or disable] */
		if(echger_determine_ec_wakeup_timer_en())
		{
			isl9519q_set_periodically_chg_curr_for_insert_dk(&isl_chg->adapter_hw_chg);

			/* BEGIN [Terry_Tzeng][2012/05/03][Set wakelock error handler] */
			ret = wakeup_flag(WAKEUP_ENABLE);
			if((FAIL == ret) && (1 == echger_get_retry_flag(ret)))
			{
				PEGA_DBG_H("[CHG_EC_ERR]%s, Set ec wake lock fail ! \n", __func__);
				wakeup_flag(WAKEUP_DISABLE);
				return -EBUSY;
			}
			/* END [Terry_Tzeng][2012/05/03][Set wakelock error handler] */

			if(FAIL != ret)
			{
				int wk_tm;

				if(POWER_SUPPLY_STATUS_FULL == msm_chg_get_batt_status())
				{
					wk_tm = BATT_FULL_WAKEUP_TIME;
				}
				else
				{
					wk_tm = WAKEUP_TIME;
				}
				PEGA_DBG_H("[CHG_EC]%s, Set ec wake up time = %d \n", __func__, wk_tm);

				wakeup_sleep_interval(wk_tm);
			}
		}
		else
		{
			wakeup_flag(WAKEUP_DISABLE);
		}
		/* END [Terry_Tzeng][2012/05/21][Determine ec wakeup timer is enable or disable] */

	}
#endif /* MSM_CHARGER_ENABLE_DOCK */
/* END [Terry_Tzeng][2012/04/28][Handle suspend charging function for inserted dock charging] */

	return 0;
}

static int isl9519q_resume(struct device *dev)
{
	struct isl9519q_struct *isl_chg = dev_get_drvdata(dev);

	dev_dbg(&isl_chg->client->dev, "%s\n", __func__);

	if(!gpio_get_value_cansleep(dock_in_gpio_number))
	{
		PEGA_DBG_H("[CHG_EC] %s, Set wakeup flag disable  \n", __func__);
		isl9519q_set_periodically_chg_curr_for_insert_dk(&isl_chg->adapter_hw_chg);
		wakeup_flag(WAKEUP_DISABLE);
	}

	return 0;
}

static const struct dev_pm_ops isl9519q_pm_ops = {
	.suspend = isl9519q_suspend,
	.resume = isl9519q_resume,
};
#endif

static struct i2c_driver isl9519q_driver = {
	.driver = {
		   .name = "isl9519q",
		   .owner = THIS_MODULE,
#ifdef CONFIG_PM
		   .pm = &isl9519q_pm_ops,
#endif
	},
	.probe = isl9519q_probe,
	.remove = __devexit_p(isl9519q_remove),
	.id_table = isl9519q_id,
};

static int __init isl9519q_init(void)
{
	return i2c_add_driver(&isl9519q_driver);
}

late_initcall(isl9519q_init);
//module_init(isl9519q_init);

static void __exit isl9519q_exit(void)
{
	return i2c_del_driver(&isl9519q_driver);
}

module_exit(isl9519q_exit);

MODULE_AUTHOR("Abhijeet Dharmapurikar <adharmap@codeaurora.org>");
MODULE_DESCRIPTION("Driver for ISL9519Q Charger chip");
MODULE_LICENSE("GPL v2");
