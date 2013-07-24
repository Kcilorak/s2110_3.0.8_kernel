#include <asm/setup.h>

extern bool dsi_power_on;
/*BEGIN Jack1_Huang 20120409 Modify code for building successfully*/
//#define PANEL_NAME_MAX_LEN	30
/*END Jack1_Huang 20120409 Modify code for building successfully*/
#define HDMI_PANEL_NAME	"hdmi_msm"
#define TVOUT_PANEL_NAME	"tvout_msm"

/*+Paul, 20111108,  add for boe panel*/
#ifdef CONFIG_DSI_TO_BOE_LVDS_PANEL
#define BOE_PANEL_NAME "mipi_boe_wxga"
#endif
/*-Paul, 20111108,  add for boe panel*/
/*+Paul, 20111121,  add for lg panel*/
#ifdef CONFIG_DSI_TO_LG_LVDS_PANEL
#define LG_PANEL_NAME "mipi_lg_wxga"
#endif
/*-Paul, 20111121,  add for lg panel*/

/*+Paul, 20111108, add the following codes to configure panel and backlight to enable gpio for boe panel, they are only for lacoste project */
#define GP0_EN_VDD_PNL	0
#define GP1_LCD_BL_EN 1
#define GP15_EN_VDD_PNL 15
#define GP47_TP_5V_SW_EN 47
#define GP72_LVDS_RST	72
/*-Paul, 20111108, add the following codes to configure panel and backlight to enable gpio for boe panel, they are only for lacoste project */

/*+, 20111108, add the following codes to configure panel and backlight to enable gpio for boe panel, they are only for lacoste project */
static struct gpiomux_setting gpio_display_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};

struct msm_gpiomux_config msm8960_display_configs[] __initdata = {
	{
		.gpio = GP47_TP_5V_SW_EN,
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_display_config,
		}
	},

	{
		.gpio = GP15_EN_VDD_PNL,
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_display_config,
		}
	},
	{
		.gpio = GP1_LCD_BL_EN,
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_display_config,
		}
	},
	{
		.gpio = GP0_EN_VDD_PNL,
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_display_config,
		}
	},
	{
		.gpio = GP72_LVDS_RST,
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_display_config,
		}
	},
	
};
/*-Paul, 20111108, add the following codes to configure panel and backlight to enable gpio for boe panel, they are only for lacoste project */

/*+Paul, 20111108, modify the backlight pwm gpio setting, change the original design to suit lacoste project */
static int westlake_dsi2lvds_gpio[1] = {
	0,/* Backlight PWM-ID=0 for PMIC-GPIO#24 */
	};
/*-Paul, 20111108, modify the backlight pwm gpio setting, change the original design to suit lacoste project */

int enable_panel_power(int on)
{
	int rc;

	if (on) {
		rc = gpio_request(GP15_EN_VDD_PNL, "panel_power_enable");
		if (rc) {
			pr_err("request gpio 15 failed, rc=%d\n", rc);
			return -ENODEV;
		}
		rc = gpio_direction_output(GP15_EN_VDD_PNL, 1);
		if (rc) {
			gpio_free(GP15_EN_VDD_PNL);
			pr_err("%s: unable to set_direction for panel_power_enable gpio [%d]\n",
				   __func__, GP15_EN_VDD_PNL);
			return -ENODEV;
		}
	} else {
		rc = gpio_direction_output(GP15_EN_VDD_PNL, 0);
		if (rc) {
			gpio_free(GP15_EN_VDD_PNL);
			pr_err("%s: unable to set_direction for panel_power_enable gpio [%d]\n",
				   __func__, GP15_EN_VDD_PNL);
			return -ENODEV;
		}
		gpio_free(GP15_EN_VDD_PNL);
	}

	return rc;
}

static int mipi_dsi_westlake_panel_power(int on)
{
	static struct regulator *reg_l2, *reg_l11;
	int rc;

	pr_debug("%s: state : %d\n", __func__, on);

	if (!dsi_power_on) {
		reg_l2 = regulator_get(&msm_mipi_dsi1_device.dev,
				"dsi_vdda");
		if (IS_ERR(reg_l2)) {
			pr_err("could not get 8921_l2, rc = %ld\n",
				PTR_ERR(reg_l2));
			return -ENODEV;
		}
		reg_l11 = regulator_get(&msm_mipi_dsi1_device.dev,
				"dsi_bridge_lvds_vdd");
		if (IS_ERR(reg_l11)) {
			pr_err("could not get 8921_l11, rc = %ld\n",
				PTR_ERR(reg_l11));
			return -ENODEV;
		}
		rc = regulator_set_voltage(reg_l2, 1200000, 1200000);
		if (rc) {
			pr_err("set_voltage l2 failed, rc=%d\n", rc);
			return -EINVAL;
		}
		
		rc = regulator_set_voltage(reg_l11, 3300000, 3300000);
		if (rc) {
			pr_err("set_voltage l11 failed, rc=%d\n", rc);
			return -EINVAL;
		}

		dsi_power_on = true;
	}
	if (on) {
		rc = regulator_set_optimum_mode(reg_l2, 100000);
		if (rc < 0) {
			pr_err("set_optimum_mode l2 failed, rc=%d\n", rc);
			return -EINVAL;
		}
		
		rc = regulator_set_optimum_mode(reg_l11, 100000);
		if (rc < 0) {
			pr_err("set_optimum_mode l11 failed, rc=%d\n", rc);
			return -EINVAL;
		}		

		rc = regulator_enable(reg_l2);
		if (rc) {
			pr_err("enable l2 failed, rc=%d\n", rc);
			return -ENODEV;
		}

		rc = regulator_enable(reg_l11);
		if (rc) {
			pr_err("enable l11 failed, rc=%d\n", rc);
			return -ENODEV;
		}

		rc = gpio_request(GP72_LVDS_RST, "bridge_ic_rst_n");
		if (rc) {
			pr_err("request gpio %d failed, rc=%d\n", GP72_LVDS_RST, rc);
			return -ENODEV;
		}

		rc = gpio_direction_output(GP72_LVDS_RST, 1);
		if (rc) {
			gpio_free(GP72_LVDS_RST);
			pr_err("%s: unable to set_direction for backlight_power_enable gpio [%d]\n",
				   __func__, GP72_LVDS_RST);
			return -ENODEV;
		}
	} else {
		rc = gpio_direction_output(GP72_LVDS_RST, 0);
		if (rc) {
			gpio_free(GP72_LVDS_RST);
			pr_err("%s: unable to set_direction for backlight_power_enable gpio [%d]\n",
				   __func__, GP72_LVDS_RST);
			return -ENODEV;
		}

		rc = enable_panel_power(0);
		if (rc) {
			pr_err("%s: unable to set_direction for panel_power_enable gpio [%d]\n",
					__func__, GP15_EN_VDD_PNL);
			return -ENODEV;
		}
		gpio_free(GP72_LVDS_RST);

		rc = regulator_disable(reg_l11);
		if (rc) {
			pr_err("disable reg_l11 failed, rc=%d\n", rc);
			return -ENODEV;
		}

		rc = regulator_disable(reg_l2);
		if (rc) {
			pr_err("disable reg_l2 failed, rc=%d\n", rc);
			return -ENODEV;
		}
		rc = regulator_set_optimum_mode(reg_l2, 100);
		if (rc < 0) {
			pr_err("set_optimum_mode l2 failed, rc=%d\n", rc);
			return -EINVAL;
		}
		
		rc = regulator_set_optimum_mode(reg_l11, 100);
		if (rc < 0) {
			pr_err("set_optimum_mode l11 failed, rc=%d\n", rc);
			return -EINVAL;
		}		

	}
	return 0;
}

static int westlake_msm_fb_detect_panel(const char *name)
{

#ifdef CONFIG_DSI_TO_BOE_LVDS_PANEL
	if (!strncmp(name, BOE_PANEL_NAME, strnlen(BOE_PANEL_NAME,
				PANEL_NAME_MAX_LEN)))
#endif
#ifdef CONFIG_DSI_TO_LG_LVDS_PANEL
	if (!strncmp(name, LG_PANEL_NAME, strnlen(LG_PANEL_NAME,
				PANEL_NAME_MAX_LEN)))
#endif
		return 0;

	if (!strncmp(name, HDMI_PANEL_NAME,
			strnlen(HDMI_PANEL_NAME,
				PANEL_NAME_MAX_LEN)))
		return 0;

	if (!strncmp(name, TVOUT_PANEL_NAME,
			strnlen(TVOUT_PANEL_NAME,
				PANEL_NAME_MAX_LEN)))
		return 0;

	pr_warning("%s: not supported '%s'", __func__, name);
	return -ENODEV;
}

