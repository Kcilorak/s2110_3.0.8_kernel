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

#include <asm/mach-types.h>
#include <linux/gpio.h>
#include <mach/board.h>
#include <mach/msm_bus_board.h>
#include <mach/gpiomux.h>
#include "devices.h"
#include "board-8960.h"

/* BEGIN Dom_Lin@pegatron [2012/03/09] [Add camera new schematic support after DVT version in 1032 codebase] */
#include <asm/setup.h>
/* END Dom_Lin@pegatron [2012/03/09] [Add camera new schematic support after DVT version in 1032 codebase] */

#if (defined(CONFIG_GPIO_SX150X) || defined(CONFIG_GPIO_SX150X_MODULE)) && \
	defined(CONFIG_I2C)

static struct i2c_board_info cam_expander_i2c_info[] = {
	{
		I2C_BOARD_INFO("sx1508q", 0x22),
		.platform_data = &msm8960_sx150x_data[SX150X_CAM]
	},
};

static struct msm_cam_expander_info cam_expander_info[] = {
	{
		cam_expander_i2c_info,
		MSM_8960_GSBI4_QUP_I2C_BUS_ID,
	},
};
#endif

static struct gpiomux_setting cam_settings[] = {
	{
		.func = GPIOMUX_FUNC_GPIO, /*suspend*/
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_DOWN,
	},

	{
		.func = GPIOMUX_FUNC_1, /*active 1*/
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_NONE,
	},

	{
		.func = GPIOMUX_FUNC_GPIO, /*active 2*/
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_NONE,
	},

	{
		.func = GPIOMUX_FUNC_1, /*active 3*/
		.drv = GPIOMUX_DRV_8MA,
		.pull = GPIOMUX_PULL_NONE,
	},

	{
		.func = GPIOMUX_FUNC_5, /*active 4*/
		.drv = GPIOMUX_DRV_8MA,
		.pull = GPIOMUX_PULL_UP,
	},

	{
		.func = GPIOMUX_FUNC_6, /*active 5*/
		.drv = GPIOMUX_DRV_8MA,
		.pull = GPIOMUX_PULL_UP,
	},

	{
		.func = GPIOMUX_FUNC_2, /*active 6*/
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_UP,
	},

	{
		.func = GPIOMUX_FUNC_3, /*active 7*/
		.drv = GPIOMUX_DRV_8MA,
		.pull = GPIOMUX_PULL_UP,
	},

	{
		.func = GPIOMUX_FUNC_GPIO, /*i2c suspend*/
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_KEEPER,
	},
/* BEGIN Chaoyen_Wu@pegatron [2012.3.13][Add front camera new schematic support after DVT version in 1032 codebase]*/
/* BEGIN Dom_Lin@pegatron [2012/03/09] [Add camera new schematic support after DVT version in 1032 codebase] */
	{
		.func = GPIOMUX_FUNC_2, /*active 9*/
		.drv = GPIOMUX_DRV_8MA,
		.pull = GPIOMUX_PULL_NONE,
	},
/* END Dom_Lin@pegatron [2012/03/09] [Add camera new schematic support after DVT version in 1032 codebase] */
/* END Chaoyen_Wu@pegatron [2012.3.13][Add front camera new schematic support after DVT version in 1032 codebase]*/
};

/* BEGIN Dom_Lin@pegatron [2012/03/03] [Init back camera drivers in 1032 codebase] */
#if 0
static struct msm_gpiomux_config msm8960_cdp_flash_configs[] = {
	{
		.gpio = 3,
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[1],
			[GPIOMUX_SUSPENDED] = &cam_settings[0],
		},
	},
};
#endif

static struct msm_gpiomux_config msm8960_cam_common_configs[] = {
	{ /* GP2_LED_DRV_EN */
		.gpio = 2,
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[2],
			[GPIOMUX_SUSPENDED] = &cam_settings[0],
		},
	},
	{ /* A_CAMIF_MCLK */
		.gpio = 5,
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[3],
			[GPIOMUX_SUSPENDED] = &cam_settings[0],
		},
	},
	{ /* GP54_5M_PWDN */
		.gpio = 54,
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[2],
			[GPIOMUX_SUSPENDED] = &cam_settings[0],
		},
	},
	{ /* GP76_WEBCAM_PWRDN_N */
		.gpio = 76,
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[2],
			[GPIOMUX_SUSPENDED] = &cam_settings[0],
		},
	},
	{ /* GP107_5M_RESET_N */
		.gpio = 107,
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[2],
			[GPIOMUX_SUSPENDED] = &cam_settings[0],
		},
	},
};

static struct msm_gpiomux_config msm8960_cam_2d_configs[] = {
	{ /* GSBI4_1 */
		.gpio = 20,
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[3],
			[GPIOMUX_SUSPENDED] = &cam_settings[8],
		},
	},
	{ /* GSBI4_0 */
		.gpio = 21,
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[3],
			[GPIOMUX_SUSPENDED] = &cam_settings[8],
		},
	},
};
/* END Dom_Lin@pegatron [2012/03/03] [Init back camera drivers in 1032 codebase] */

/* BEGIN Dom_Lin@pegatron [2012/03/09] [Add camera new schematic support after DVT version in 1032 codebase] */
static struct msm_gpiomux_config msm8960_cam_common_configs_v2[] = {
/* BEGIN Chaoyen_Wu@pegatron [2012.3.13][Add front camera new schematic support after DVT version in 1032 codebase]*/

	{ /* A_FRONT_MCLK */
		.gpio = 4,
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[9],
			[GPIOMUX_SUSPENDED] = &cam_settings[0],
		},
	},
};

static struct msm_gpiomux_config msm8960_cam_2d_configs_v2_front[] = {
	{ /* GSBI1_1 */
		.gpio = 8,
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[3],
			[GPIOMUX_SUSPENDED] = &cam_settings[8],
		},
	},
	{ /* GSBI1_0 */
		.gpio = 9,
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[3],
			[GPIOMUX_SUSPENDED] = &cam_settings[8],
		},
	},
};
/* END Chaoyen_Wu@pegatron [2012.3.13][Add front camera new schematic support after DVT version in 1032 codebase]*/

static struct msm_gpiomux_config msm8960_cam_2d_configs_v2_back[] = {
	{ /* GSBI4_1 */
		.gpio = 20,
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[3],
			[GPIOMUX_SUSPENDED] = &cam_settings[8],
		},
	},
	{ /* GSBI4_0 */
		.gpio = 21,
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[3],
			[GPIOMUX_SUSPENDED] = &cam_settings[8],
		},
	},
/* BEGIN Dom_Lin@pegatron [2012/03/19] [Add camera LED new schematic support after DVT version in 1032 codebase] */
	{ /* GSBI5_1 */
		.gpio = 24,
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[3],
			[GPIOMUX_SUSPENDED] = &cam_settings[8],
		},
	},
	{ /* GSBI5_0 */
		.gpio = 25,
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[3],
			[GPIOMUX_SUSPENDED] = &cam_settings[8],
		},
	},
/* END Dom_Lin@pegatron [2012/03/19] [Add camera LED new schematic support after DVT version in 1032 codebase] */
};
/* END Dom_Lin@pegatron [2012/03/09] [Add camera new schematic support after DVT version in 1032 codebase] */



#ifdef CONFIG_MSM_CAMERA

#ifdef CONFIG_IMX074
#define VFE_CAMIF_TIMER1_GPIO 2
#define VFE_CAMIF_TIMER2_GPIO 3
#define VFE_CAMIF_TIMER3_GPIO_INT 4
static struct msm_camera_sensor_strobe_flash_data strobe_flash_xenon = {
	.flash_trigger = VFE_CAMIF_TIMER2_GPIO,
	.flash_charge = VFE_CAMIF_TIMER1_GPIO,
	.flash_charge_done = VFE_CAMIF_TIMER3_GPIO_INT,
	.flash_recharge_duration = 50000,
	.irq = MSM_GPIO_TO_INT(VFE_CAMIF_TIMER3_GPIO_INT),
};
#endif

#ifdef CONFIG_MSM_CAMERA_FLASH
static struct msm_camera_sensor_flash_src msm_flash_src = {
	.flash_sr_type = MSM_CAMERA_FLASH_SRC_EXT,
#ifdef CONFIG_IMX074
	._fsrc.ext_driver_src.led_en = VFE_CAMIF_TIMER1_GPIO,
	._fsrc.ext_driver_src.led_flash_en = VFE_CAMIF_TIMER2_GPIO,
#endif
};
#endif

static struct msm_bus_vectors cam_init_vectors[] = {
	{
		.src = MSM_BUS_MASTER_VFE,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 0,
		.ib  = 0,
	},
	{
		.src = MSM_BUS_MASTER_VPE,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 0,
		.ib  = 0,
	},
	{
		.src = MSM_BUS_MASTER_JPEG_ENC,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 0,
		.ib  = 0,
	},
	{
		.src = MSM_BUS_MASTER_JPEG_ENC,
		.dst = MSM_BUS_SLAVE_MM_IMEM,
		.ab  = 0,
		.ib  = 0,
	},
	{
		.src = MSM_BUS_MASTER_VFE,
		.dst = MSM_BUS_SLAVE_MM_IMEM,
		.ab  = 0,
		.ib  = 0,
	},
};

static struct msm_bus_vectors cam_preview_vectors[] = {
	{
		.src = MSM_BUS_MASTER_VFE,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 27648000,
		.ib  = 110592000,
	},
	{
		.src = MSM_BUS_MASTER_VPE,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 0,
		.ib  = 0,
	},
	{
		.src = MSM_BUS_MASTER_JPEG_ENC,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 0,
		.ib  = 0,
	},
	{
		.src = MSM_BUS_MASTER_JPEG_ENC,
		.dst = MSM_BUS_SLAVE_MM_IMEM,
		.ab  = 0,
		.ib  = 0,
	},
	{
		.src = MSM_BUS_MASTER_VFE,
		.dst = MSM_BUS_SLAVE_MM_IMEM,
		.ab  = 0,
		.ib  = 0,
	},
};

static struct msm_bus_vectors cam_video_vectors[] = {
	{
		.src = MSM_BUS_MASTER_VFE,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 154275840,
		.ib  = 617103360,
	},
	{
		.src = MSM_BUS_MASTER_VPE,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 206807040,
		.ib  = 488816640,
	},
	{
		.src = MSM_BUS_MASTER_JPEG_ENC,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 0,
		.ib  = 0,
	},
	{
		.src = MSM_BUS_MASTER_JPEG_ENC,
		.dst = MSM_BUS_SLAVE_MM_IMEM,
		.ab  = 0,
		.ib  = 0,
	},
	{
		.src = MSM_BUS_MASTER_VFE,
		.dst = MSM_BUS_SLAVE_MM_IMEM,
		.ab  = 0,
		.ib  = 0,
	},
};

static struct msm_bus_vectors cam_snapshot_vectors[] = {
	{
		.src = MSM_BUS_MASTER_VFE,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 274423680,
		.ib  = 1097694720,
	},
	{
		.src = MSM_BUS_MASTER_VPE,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 0,
		.ib  = 0,
	},
	{
		.src = MSM_BUS_MASTER_JPEG_ENC,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 540000000,
		.ib  = 1350000000,
	},
	{
		.src = MSM_BUS_MASTER_JPEG_ENC,
		.dst = MSM_BUS_SLAVE_MM_IMEM,
		.ab  = 43200000,
		.ib  = 69120000,
	},
	{
		.src = MSM_BUS_MASTER_VFE,
		.dst = MSM_BUS_SLAVE_MM_IMEM,
		.ab  = 43200000,
		.ib  = 69120000,
	},
};

static struct msm_bus_vectors cam_zsl_vectors[] = {
	{
		.src = MSM_BUS_MASTER_VFE,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 302071680,
		.ib  = 1208286720,
	},
	{
		.src = MSM_BUS_MASTER_VPE,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 0,
		.ib  = 0,
	},
	{
		.src = MSM_BUS_MASTER_JPEG_ENC,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 540000000,
		.ib  = 1350000000,
	},
	{
		.src = MSM_BUS_MASTER_JPEG_ENC,
		.dst = MSM_BUS_SLAVE_MM_IMEM,
		.ab  = 43200000,
		.ib  = 69120000,
	},
	{
		.src = MSM_BUS_MASTER_VFE,
		.dst = MSM_BUS_SLAVE_MM_IMEM,
		.ab  = 43200000,
		.ib  = 69120000,
	},
};

static struct msm_bus_paths cam_bus_client_config[] = {
	{
		ARRAY_SIZE(cam_init_vectors),
		cam_init_vectors,
	},
	{
		ARRAY_SIZE(cam_preview_vectors),
		cam_preview_vectors,
	},
	{
		ARRAY_SIZE(cam_video_vectors),
		cam_video_vectors,
	},
	{
		ARRAY_SIZE(cam_snapshot_vectors),
		cam_snapshot_vectors,
	},
	{
		ARRAY_SIZE(cam_zsl_vectors),
		cam_zsl_vectors,
	},
};

static struct msm_bus_scale_pdata cam_bus_client_pdata = {
		cam_bus_client_config,
		ARRAY_SIZE(cam_bus_client_config),
		.name = "msm_camera",
};

static struct msm_camera_device_platform_data msm_camera_csi_device_data[] = {
	{
		.csid_core = 0,
		.is_csiphy = 1,
		.is_csid   = 1,
		.is_ispif  = 1,
		.is_vpe    = 1,
		.cam_bus_scale_table = &cam_bus_client_pdata,
	},
	{
		.csid_core = 1,
		.is_csiphy = 1,
		.is_csid   = 1,
		.is_ispif  = 1,
		.is_vpe    = 1,
		.cam_bus_scale_table = &cam_bus_client_pdata,
	},
};

#ifdef CONFIG_IMX074
static struct camera_vreg_t msm_8960_back_cam_vreg[] = {
	{"cam_vdig", REG_LDO, 1200000, 1200000, 105000},
	{"cam_vio", REG_VS, 0, 0, 0},
	{"cam_vana", REG_LDO, 2800000, 2850000, 85600},
	{"cam_vaf", REG_LDO, 2800000, 2800000, 300000},
};
#endif

#ifdef CONFIG_OV2720
static struct camera_vreg_t msm_8960_front_cam_vreg[] = {
	{"cam_vio", REG_VS, 0, 0, 0},
	{"cam_vana", REG_LDO, 2800000, 2850000, 85600},
	{"cam_vdig", REG_LDO, 1200000, 1200000, 105000},
};
#endif

static struct gpio msm8960_common_cam_gpio[] = {
	{5, GPIOF_DIR_IN, "CAMIF_MCLK"},
	{20, GPIOF_DIR_IN, "CAMIF_I2C_DATA"},
	{21, GPIOF_DIR_IN, "CAMIF_I2C_CLK"},
};
/* BEGIN Chaoyen_Wu@pegatron [2012.3.13][Add front camera new schematic support after DVT version in 1032 codebase]*/
static struct gpio msm8960_common_cam_gpio_v2_front[] = {
	{4, GPIOF_DIR_IN, "CAMIF_MCLK"},
	{8, GPIOF_DIR_IN, "CAMIF_I2C_DATA"},
	{9, GPIOF_DIR_IN, "CAMIF_I2C_CLK"},
};

/* END Chaoyen_Wu@pegatron [2012.3.13][Add front camera new schematic support after DVT version in 1032 codebase]*/

/* BEGIN Dom_Lin@pegatron [2012/03/19] [Add camera LED new schematic support after DVT version in 1032 codebase] */
static struct gpio msm8960_common_cam_gpio_v2_back[] = {
	{5, GPIOF_DIR_IN, "CAMIF_MCLK"},
	{20, GPIOF_DIR_IN, "CAMIF_I2C_DATA"},
	{21, GPIOF_DIR_IN, "CAMIF_I2C_CLK"},
	{24, GPIOF_DIR_IN, "A_I2C_DATA_FLASH"},
	{25, GPIOF_DIR_IN, "A_I2C_CLK_FLASH"},
};
/* END Dom_Lin@pegatron [2012/03/19] [Add camera LED new schematic support after DVT version in 1032 codebase] */

#ifdef CONFIG_MT9M114
static struct gpio msm8960_front_cam_gpio[] = {
	{76, GPIOF_DIR_OUT, "CAM_RESET"},
};
#endif

/* BEGIN Dom_Lin@pegatron [2012/03/03] [Init back camera drivers in 1032 codebase] */
#ifdef CONFIG_OV5640
static struct gpio msm8960_back_cam_gpio[] = {
	{54, GPIOF_DIR_OUT, "GP54_5M_PWDN"},
	{107, GPIOF_DIR_OUT, "GP107_5M_RESET_N"},
};
#endif
/* END Dom_Lin@pegatron [2012/03/03] [Init back camera drivers in 1032 codebase] */

#ifdef CONFIG_MT9M114
static struct msm_gpio_set_tbl msm8960_front_cam_gpio_set_tbl[] = {
	{76, GPIOF_OUT_INIT_LOW, 1000},
	{76, GPIOF_OUT_INIT_HIGH, 4000},
};
#endif

/* BEGIN Dom_Lin@pegatron [2012/03/03] [Init back camera drivers in 1032 codebase] */
#ifdef CONFIG_OV5640
static struct msm_gpio_set_tbl msm8960_back_cam_gpio_set_tbl[] = {
	{54, GPIOF_OUT_INIT_HIGH, 0},
	{107, GPIOF_OUT_INIT_LOW, 5000},
	{54, GPIOF_OUT_INIT_LOW, 1000},
	{107, GPIOF_OUT_INIT_HIGH, 20000},
};
#endif
/* END Dom_Lin@pegatron [2012/03/03] [Init back camera drivers in 1032 codebase] */

#ifdef CONFIG_MT9M114
static struct msm_camera_gpio_conf msm_8960_front_cam_gpio_conf = {
	.cam_gpiomux_conf_tbl = msm8960_cam_2d_configs,
	.cam_gpiomux_conf_tbl_size = ARRAY_SIZE(msm8960_cam_2d_configs),
	.cam_gpio_common_tbl = msm8960_common_cam_gpio,
	.cam_gpio_common_tbl_size = ARRAY_SIZE(msm8960_common_cam_gpio),
	.cam_gpio_req_tbl = msm8960_front_cam_gpio,
	.cam_gpio_req_tbl_size = ARRAY_SIZE(msm8960_front_cam_gpio),
	.cam_gpio_set_tbl = msm8960_front_cam_gpio_set_tbl,
	.cam_gpio_set_tbl_size = ARRAY_SIZE(msm8960_front_cam_gpio_set_tbl),
};
/* BEGIN Chaoyen_Wu@pegatron [2012.3.13][Add front camera new schematic support after DVT version in 1032 codebase]*/
static struct msm_camera_gpio_conf msm_8960_front_cam_gpio_conf_v2 = {
	.cam_gpiomux_conf_tbl = msm8960_cam_2d_configs_v2_front,
	.cam_gpiomux_conf_tbl_size = ARRAY_SIZE(msm8960_cam_2d_configs_v2_front),
	.cam_gpio_common_tbl = msm8960_common_cam_gpio_v2_front,
	.cam_gpio_common_tbl_size = ARRAY_SIZE(msm8960_common_cam_gpio_v2_front),
	.cam_gpio_req_tbl = msm8960_front_cam_gpio,
	.cam_gpio_req_tbl_size = ARRAY_SIZE(msm8960_front_cam_gpio),
	.cam_gpio_set_tbl = msm8960_front_cam_gpio_set_tbl,
	.cam_gpio_set_tbl_size = ARRAY_SIZE(msm8960_front_cam_gpio_set_tbl),
};
/* END Chaoyen_Wu@pegatron [2012.3.13][Add front camera new schematic support after DVT version in 1032 codebase]*/

#endif

#ifdef CONFIG_OV5640
static struct msm_camera_gpio_conf msm_8960_back_cam_gpio_conf = {
	.cam_gpiomux_conf_tbl = msm8960_cam_2d_configs,
	.cam_gpiomux_conf_tbl_size = ARRAY_SIZE(msm8960_cam_2d_configs),
	.cam_gpio_common_tbl = msm8960_common_cam_gpio,
	.cam_gpio_common_tbl_size = ARRAY_SIZE(msm8960_common_cam_gpio),
	.cam_gpio_req_tbl = msm8960_back_cam_gpio,
	.cam_gpio_req_tbl_size = ARRAY_SIZE(msm8960_back_cam_gpio),
	.cam_gpio_set_tbl = msm8960_back_cam_gpio_set_tbl,
	.cam_gpio_set_tbl_size = ARRAY_SIZE(msm8960_back_cam_gpio_set_tbl),
};

/* BEGIN Dom_Lin@pegatron [2012/03/09] [Add camera new schematic support after DVT version in 1032 codebase] */
static struct msm_camera_gpio_conf msm_8960_back_cam_gpio_conf_v2 = {
	.cam_gpiomux_conf_tbl = msm8960_cam_2d_configs_v2_back,
	.cam_gpiomux_conf_tbl_size = ARRAY_SIZE(msm8960_cam_2d_configs_v2_back),
	.cam_gpio_common_tbl = msm8960_common_cam_gpio_v2_back,
	.cam_gpio_common_tbl_size = ARRAY_SIZE(msm8960_common_cam_gpio_v2_back),
	.cam_gpio_req_tbl = msm8960_back_cam_gpio,
	.cam_gpio_req_tbl_size = ARRAY_SIZE(msm8960_back_cam_gpio),
	.cam_gpio_set_tbl = msm8960_back_cam_gpio_set_tbl,
	.cam_gpio_set_tbl_size = ARRAY_SIZE(msm8960_back_cam_gpio_set_tbl),
};
/* END Dom_Lin@pegatron [2012/03/09] [Add camera new schematic support after DVT version in 1032 codebase] */
#endif

#ifdef CONFIG_IMX074
static struct i2c_board_info imx074_actuator_i2c_info = {
	I2C_BOARD_INFO("imx074_act", 0x11),
};

static struct msm_actuator_info imx074_actuator_info = {
	.board_info     = &imx074_actuator_i2c_info,
	.bus_id         = MSM_8960_GSBI4_QUP_I2C_BUS_ID,
	.vcm_pwd        = 0,
	.vcm_enable     = 1,
};

static struct msm_camera_sensor_flash_data flash_imx074 = {
	.flash_type	= MSM_CAMERA_FLASH_LED,
#ifdef CONFIG_MSM_CAMERA_FLASH
	.flash_src	= &msm_flash_src
#endif
};

static struct msm_camera_sensor_platform_info sensor_board_info_imx074 = {
	.mount_angle	= 90,
	.cam_vreg = msm_8960_back_cam_vreg,
	.num_vreg = ARRAY_SIZE(msm_8960_back_cam_vreg),
	.gpio_conf = &msm_8960_back_cam_gpio_conf,
};

static struct msm_camera_sensor_info msm_camera_sensor_imx074_data = {
	.sensor_name	= "imx074",
	.pdata	= &msm_camera_csi_device_data[0],
	.flash_data	= &flash_imx074,
	.strobe_flash_data = &strobe_flash_xenon,
	.sensor_platform_info = &sensor_board_info_imx074,
	.csi_if	= 1,
	.camera_type = BACK_CAMERA_2D,
	.actuator_info = &imx074_actuator_info
};
#endif

/* BEGIN Dom_Lin@pegatron [2012/03/03] [Init back camera drivers in 1032 codebase] */
#ifdef CONFIG_OV5640
static struct camera_vreg_t msm_8960_ov5640_vreg[] = {
	{"cam_vio", REG_VS, 0, 0, 0, 0},
	{"cam_vdig", REG_LDO, 1800000, 1800000, 140000, 1}, // delay 1ms, make sure DOVDD becomes stable before AVDD becomes stable
	{"cam_vana", REG_LDO, 2800000, 2800000, 100000, 0}, // analog power use 42mA, vcm power use 100mA
};

/* BEGIN Dom_Lin@pegatron [2012/03/09] [Add camera new schematic support after DVT version in 1032 codebase] */
static struct camera_vreg_t msm_8960_ov5640_vreg_v2[] = {
	{"cam_vdig_v2", REG_VS, 0, 0, 0, 1}, /* common with I2C pull high, delay 1ms, make sure DOVDD becomes stable before AVDD becomes stabl */
	{"cam_vana_v2", REG_LDO, 2800000, 2800000, 42000, 0},
#if defined(PEGA_SMT_BUILD)
	{"cam_vaf_v2", REG_LDO, 2800000, 2800000, 100000, 0},
#endif
};
/* END Dom_Lin@pegatron [2012/03/09] [Add camera new schematic support after DVT version in 1032 codebase] */

/* BEGIN Dom_Lin@pegatron [2012/03/08] [Init camera LED drivers in 1032 codebase] */
static struct msm_camera_sensor_flash_data flash_ov5640 = {
/* BEGIN Dom_Lin@pegatron [2012/03/08] [Enable back camera flash in torch mode in 1032 codebase] */
	.flash_type	= MSM_CAMERA_FLASH_LED,
/* END Dom_Lin@pegatron [2012/03/08] [Enable back camera flash in torch mode in 1032 codebase] */
#ifdef CONFIG_MSM_CAMERA_FLASH
	.flash_src  = &msm_flash_src,
#endif
};
/* END Dom_Lin@pegatron [2012/03/08] [Init camera LED drivers in 1032 codebase] */

/* BEGIN Dom_Lin@pegatron [2012/03/08] [Init camera strobe flash function in 1032 codebase] */
static struct msm_camera_sensor_strobe_flash_data strobe_flash_led = {
};
/* END Dom_Lin@pegatron [2012/03/08] [Init camera strobe flash function in 1032 codebase] */

static struct msm_camera_sensor_platform_info sensor_board_info_ov5640 = {
	.mount_angle	= 0,
	.cam_vreg = msm_8960_ov5640_vreg,
	.num_vreg = ARRAY_SIZE(msm_8960_ov5640_vreg),
	.gpio_conf = &msm_8960_back_cam_gpio_conf,
};

static struct msm_camera_sensor_info msm_camera_sensor_ov5640_data = {
	.sensor_name	= "ov5640",
	.pdata	= &msm_camera_csi_device_data[0],
	.flash_data	= &flash_ov5640,
/* BEGIN Dom_Lin@pegatron [2012/03/08] [Init camera strobe flash function in 1032 codebase] */
	.strobe_flash_data = &strobe_flash_led,
/* END Dom_Lin@pegatron [2012/03/08] [Init camera strobe flash function in 1032 codebase] */
	.sensor_platform_info = &sensor_board_info_ov5640,
	.csi_if	= 1,
	.camera_type = BACK_CAMERA_2D,
};

/* BEGIN Dom_Lin@pegatron [2012/03/09] [Add camera new schematic support after DVT version in 1032 codebase] */
static struct msm_camera_sensor_platform_info sensor_board_info_ov5640_v2 = {
	.mount_angle	= 0,
	.cam_vreg = msm_8960_ov5640_vreg_v2,
	.num_vreg = ARRAY_SIZE(msm_8960_ov5640_vreg_v2),
	.gpio_conf = &msm_8960_back_cam_gpio_conf_v2,
};

static struct msm_camera_sensor_info msm_camera_sensor_ov5640_data_v2 = {
	.sensor_name	= "ov5640",
	.pdata	= &msm_camera_csi_device_data[0],
	.flash_data	= &flash_ov5640,
	.strobe_flash_data = &strobe_flash_led,
	.sensor_platform_info = &sensor_board_info_ov5640_v2,
	.csi_if	= 1,
	.camera_type = BACK_CAMERA_2D,
};
/* END Dom_Lin@pegatron [2012/03/09] [Add camera new schematic support after DVT version in 1032 codebase] */
#endif
/* END Dom_Lin@pegatron [2012/03/03] [Init back camera drivers in 1032 codebase] */

#ifdef CONFIG_MT9M114
/* BEGIN Chaoyen_Wu@pegatron [2012/03/08] [Init front camera drivers in 1032 codebase] */
static struct camera_vreg_t msm_8960_mt9m114_vreg[] = {
	{"cam_vio", REG_VS, 0, 0, 0, 0},
	{"cam_vdig", REG_LDO, 1800000, 1800000, 105000, 0},
	{"cam_vana", REG_LDO, 2800000, 2850000, 85600, 0},
	{"pll_vdd", REG_LDO, 1800000, 1800000, 300000, 0},
};
/* END Chaoyen_Wu@pegatron [2012/03/08] [Init front camera drivers in 1032 codebase] */
/* BEGIN Chaoyen_Wu@pegatron [2012.3.13][Add front camera new schematic support after DVT version in 1032 codebase]*/
static struct camera_vreg_t msm_8960_mt9m114_vreg_v2[] = {
	{"cam_vdig_v2", REG_LDO, 1800000, 1800000, 105000, 0},
	{"cam_vana_v2", REG_LDO, 2800000, 2850000, 85600, 0},
	{"pll_vdd_v2", REG_LDO, 1800000, 1800000, 300000, 0},
};
/* END Chaoyen_Wu@pegatron [2012.3.13][Add front camera new schematic support after DVT version in 1032 codebase]*/

static struct msm_camera_sensor_flash_data flash_mt9m114 = {
	.flash_type = MSM_CAMERA_FLASH_NONE
};

static struct msm_camera_sensor_platform_info sensor_board_info_mt9m114 = {
/* BEGIN Chaoyen_Wu@pegatron [2012/03/08] [Init front camera drivers in 1032 codebase] */	
	.mount_angle = 0,
/* END Chaoyen_Wu@pegatron [2012/03/08] [Init front camera drivers in 1032 codebase] */	
	.cam_vreg = msm_8960_mt9m114_vreg,
	.num_vreg = ARRAY_SIZE(msm_8960_mt9m114_vreg),
	.gpio_conf = &msm_8960_front_cam_gpio_conf,
};

static struct msm_camera_sensor_info msm_camera_sensor_mt9m114_data = {
	.sensor_name = "mt9m114",
	.pdata = &msm_camera_csi_device_data[1],
	.flash_data = &flash_mt9m114,
	.sensor_platform_info = &sensor_board_info_mt9m114,
	.csi_if = 1,
	.camera_type = FRONT_CAMERA_2D,
};
/* BEGIN Chaoyen_Wu@pegatron [2012.3.13][Add front camera new schematic support after DVT version in 1032 codebase]*/
static struct msm_camera_sensor_platform_info sensor_board_info_mt9m114_v2 = {	
	.mount_angle = 0,	
	.cam_vreg = msm_8960_mt9m114_vreg_v2,
	.num_vreg = ARRAY_SIZE(msm_8960_mt9m114_vreg_v2),
	.gpio_conf = &msm_8960_front_cam_gpio_conf_v2,
};

static struct msm_camera_sensor_info msm_camera_sensor_mt9m114_data_v2 = {
	.sensor_name = "mt9m114",
	.pdata = &msm_camera_csi_device_data[1],
	.flash_data = &flash_mt9m114,
	.sensor_platform_info = &sensor_board_info_mt9m114_v2,
	.csi_if = 1,
	.camera_type = FRONT_CAMERA_2D,
};
/* END Chaoyen_Wu@pegatron [2012.3.13][Add front camera new schematic support after DVT version in 1032 codebase]*/
#endif

#ifdef CONFIG_OV2720
static struct msm_camera_sensor_flash_data flash_ov2720 = {
	.flash_type	= MSM_CAMERA_FLASH_NONE,
};

static struct msm_camera_sensor_platform_info sensor_board_info_ov2720 = {
	.mount_angle	= 0,
	.cam_vreg = msm_8960_front_cam_vreg,
	.num_vreg = ARRAY_SIZE(msm_8960_front_cam_vreg),
	.gpio_conf = &msm_8960_front_cam_gpio_conf,
};

static struct msm_camera_sensor_info msm_camera_sensor_ov2720_data = {
	.sensor_name	= "ov2720",
	.pdata	= &msm_camera_csi_device_data[1],
	.flash_data	= &flash_ov2720,
	.sensor_platform_info = &sensor_board_info_ov2720,
	.csi_if	= 1,
	.camera_type = FRONT_CAMERA_2D,
};
#endif

#ifdef CONFIG_S5K3L1YX
static struct camera_vreg_t msm_8960_s5k3l1yx_vreg[] = {
	{"cam_vdig", REG_LDO, 1200000, 1200000, 105000},
	{"cam_vana", REG_LDO, 2800000, 2850000, 85600},
	{"cam_vio", REG_VS, 0, 0, 0},
	{"cam_vaf", REG_LDO, 2800000, 2800000, 300000},
};

static struct msm_camera_sensor_flash_data flash_s5k3l1yx = {
	.flash_type = MSM_CAMERA_FLASH_NONE,
};

static struct msm_camera_sensor_platform_info sensor_board_info_s5k3l1yx = {
	.mount_angle  = 0,
	.cam_vreg = msm_8960_s5k3l1yx_vreg,
	.num_vreg = ARRAY_SIZE(msm_8960_s5k3l1yx_vreg),
	.gpio_conf = &msm_8960_back_cam_gpio_conf,
};

static struct msm_camera_sensor_info msm_camera_sensor_s5k3l1yx_data = {
	.sensor_name          = "s5k3l1yx",
	.pdata                = &msm_camera_csi_device_data[0],
	.flash_data           = &flash_s5k3l1yx,
	.sensor_platform_info = &sensor_board_info_s5k3l1yx,
	.csi_if               = 1,
	.camera_type          = BACK_CAMERA_2D,
};
#endif

#ifdef CONFIG_OV2720
static struct pm8xxx_mpp_config_data privacy_light_on_config = {
	.type		= PM8XXX_MPP_TYPE_SINK,
	.level		= PM8XXX_MPP_CS_OUT_5MA,
	.control	= PM8XXX_MPP_CS_CTRL_MPP_LOW_EN,
};

static struct pm8xxx_mpp_config_data privacy_light_off_config = {
	.type		= PM8XXX_MPP_TYPE_SINK,
	.level		= PM8XXX_MPP_CS_OUT_5MA,
	.control	= PM8XXX_MPP_CS_CTRL_DISABLE,
};

static int32_t msm_camera_8960_ext_power_ctrl(int enable)
{
	int rc = 0;
	if (enable) {
		rc = pm8xxx_mpp_config(PM8921_MPP_PM_TO_SYS(12),
			&privacy_light_on_config);
	} else {
		rc = pm8xxx_mpp_config(PM8921_MPP_PM_TO_SYS(12),
			&privacy_light_off_config);
	}
	return rc;
}
#endif

void __init msm8960_init_cam(void)
{
	msm_gpiomux_install(msm8960_cam_common_configs,
			ARRAY_SIZE(msm8960_cam_common_configs));

/* BEGIN Dom_Lin@pegatron [2012/03/09] [Add camera new schematic support after DVT version in 1032 codebase] */
	if (get_pega_hw_board_version_status() >= 2) { // Not DVT
		msm_gpiomux_install(msm8960_cam_common_configs_v2,
				ARRAY_SIZE(msm8960_cam_common_configs_v2));
	}
/* END Dom_Lin@pegatron [2012/03/09] [Add camera new schematic support after DVT version in 1032 codebase] */

	if (machine_is_msm8960_cdp()) {
/* BEGIN Dom_Lin@pegatron [2012/03/03] [Init back camera drivers in 1032 codebase] */
#if 0
		msm_gpiomux_install(msm8960_cdp_flash_configs,
			ARRAY_SIZE(msm8960_cdp_flash_configs));
#endif
/* END Dom_Lin@pegatron [2012/03/03] [Init back camera drivers in 1032 codebase] */
		#if defined(CONFIG_I2C) && (defined(CONFIG_GPIO_SX150X) || \
		defined(CONFIG_GPIO_SX150X_MODULE))
		msm_flash_src._fsrc.ext_driver_src.led_en =
			GPIO_CAM_GP_LED_EN1;
		msm_flash_src._fsrc.ext_driver_src.led_flash_en =
			GPIO_CAM_GP_LED_EN2;
		msm_flash_src._fsrc.ext_driver_src.expander_info =
			cam_expander_info;
		#endif
	}

#if defined(CONFIG_IMX074) && defined(CONFIG_OV2720)
	if (machine_is_msm8960_liquid()) {
		struct msm_camera_sensor_info *s_info;
		s_info = &msm_camera_sensor_imx074_data;
		s_info->sensor_platform_info->mount_angle = 180;
		s_info = &msm_camera_sensor_ov2720_data;
		s_info->sensor_platform_info->ext_power_ctrl =
			msm_camera_8960_ext_power_ctrl;
	}
#endif

	platform_device_register(&msm8960_device_csiphy0);
	platform_device_register(&msm8960_device_csiphy1);
	platform_device_register(&msm8960_device_csid0);
	platform_device_register(&msm8960_device_csid1);
	platform_device_register(&msm8960_device_ispif);
	platform_device_register(&msm8960_device_vfe);
	platform_device_register(&msm8960_device_vpe);
}

#ifdef CONFIG_I2C
static struct i2c_board_info msm8960_camera_i2c_boardinfo[] = {
#ifdef CONFIG_IMX074
	{
	I2C_BOARD_INFO("imx074", 0x1A),
	.platform_data = &msm_camera_sensor_imx074_data,
	},
#endif
#ifdef CONFIG_OV2720
	{
	I2C_BOARD_INFO("ov2720", 0x6C),
	.platform_data = &msm_camera_sensor_ov2720_data,
	},
#endif
#ifdef CONFIG_OV5640
	{
	I2C_BOARD_INFO("ov5640", 0x78),
	.platform_data = &msm_camera_sensor_ov5640_data,
	},
#endif
#ifdef CONFIG_MT9M114
	{
	I2C_BOARD_INFO("mt9m114", 0x48),
	.platform_data = &msm_camera_sensor_mt9m114_data,
	},
#endif
#ifdef CONFIG_S5K3L1YX
	{
	I2C_BOARD_INFO("s5k3l1yx", 0x20),
	.platform_data = &msm_camera_sensor_s5k3l1yx_data,
	},
#endif
#ifdef CONFIG_MSM_CAMERA_FLASH_SC628A
	{
	I2C_BOARD_INFO("sc628a", 0x6E),
	},
#endif
};

struct msm_camera_board_info msm8960_camera_board_info = {
	.board_info = msm8960_camera_i2c_boardinfo,
	.num_i2c_board_info = ARRAY_SIZE(msm8960_camera_i2c_boardinfo),
};

/* BEGIN Dom_Lin@pegatron [2012/03/09] [Add camera new schematic support after DVT version in 1032 codebase] */
static struct i2c_board_info msm8960_camera_i2c_boardinfo_v2_back[] = {
#ifdef CONFIG_OV5640
	{
	I2C_BOARD_INFO("ov5640", 0x78),
	.platform_data = &msm_camera_sensor_ov5640_data_v2,
	},
#endif
};

struct msm_camera_board_info msm8960_camera_board_info_v2_back = {
	.board_info = msm8960_camera_i2c_boardinfo_v2_back,
	.num_i2c_board_info = ARRAY_SIZE(msm8960_camera_i2c_boardinfo_v2_back),
};
/* BEGIN Chaoyen_Wu@pegatron [2012.3.13][Add front camera new schematic support after DVT version in 1032 codebase]*/

static struct i2c_board_info msm8960_camera_i2c_boardinfo_v2_front[] = {
#ifdef CONFIG_MT9M114
	{
	I2C_BOARD_INFO("mt9m114", 0x48),
	.platform_data = &msm_camera_sensor_mt9m114_data_v2,
	},
#endif
};

struct msm_camera_board_info msm8960_camera_board_info_v2_front = {
	.board_info = msm8960_camera_i2c_boardinfo_v2_front,
	.num_i2c_board_info = ARRAY_SIZE(msm8960_camera_i2c_boardinfo_v2_front),
};
/* END Chaoyen_Wu@pegatron [2012.3.13][Add front camera new schematic support after DVT version in 1032 codebase]*/
/* END Dom_Lin@pegatron [2012/03/09] [Add camera new schematic support after DVT version in 1032 codebase] */
#endif
#endif
