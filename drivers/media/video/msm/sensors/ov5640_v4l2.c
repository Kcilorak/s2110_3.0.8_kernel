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

#include "msm_sensor.h"
/* BEGIN Dom_Lin@pegatron [2012/02/04] [Enable back camera auto focus] */
#include <media/msm_isp.h>
/* END Dom_Lin@pegatron [2012/02/04] [Enable back camera auto focus] */

#if !defined(PEGA_SMT_BUILD)
#include <asm/setup.h>
#include <linux/regulator/consumer.h>
#endif

/* BEGIN Dom_Lin@pegatron [2012/04/17] [Add delay when taking picture in no light environment] */
#if defined(CONFIG_LEDS_LM3559) && defined(CONFIG_MSM_CAMERA_FLASH_LM3559)
#include <linux/leds-lm3559.h>
#endif
/* END Dom_Lin@pegatron [2012/04/17] [Add delay when taking picture in no light environment] */

/* BEGIN Dom_Lin@pegatron [2012/02/02] [Enable auto flash function in 1024 codebase] */
#include "ov5640_v4l2.h"
/* END Dom_Lin@pegatron [2012/02/02] [Enable auto flash function in 1024 codebase] */
#define SENSOR_NAME "ov5640"
#define PLATFORM_DRIVER_NAME "msm_camera_ov5640"
#define ov5640_obj ov5640_##obj

#undef CDBG
#define CDBG(fmt, args...) printk(KERN_ERR "[%s: %d] " fmt, __func__, __LINE__, ##args)

#undef CDBGE
#define CDBGE(fmt, args...) printk(KERN_ERR "[%s: %d] " fmt, __func__, __LINE__, ##args)

/* BEGIN Dom_Lin@pegatron [2012/02/20] [Enable back camera touch focus] */
#define OV5640_RC_CHECK(msg) \
	if (rc < 0) { \
		CDBGE(msg" failed\n"); \
		goto done; \
	}
/* END Dom_Lin@pegatron [2012/02/20] [Enable back camera touch focus] */

#define OV5640_AF_ACK
/* BEGIN Dom_Lin@pegatron [2012/04/18] [Update AF firmware to version OV5640-AD5820-ASUS-13011000-20120416140547] */
//#define OV5640_AF_CUSTOM_FOCUS_ZONE
/* END Dom_Lin@pegatron [2012/04/18] [Update AF firmware to version OV5640-AD5820-ASUS-13011000-20120416140547] */
//#define OV5640_SUSPEND_CHANGE

/* BEGIN Dom_Lin@pegatron [2012/02/04] [Enable back camera auto focus] */
#define OV5640_CMD_MAIN  0x3022
#define OV5640_CMD_ACK   0x3023
#define OV5640_CMD_PARA0 0x3024
#define OV5640_CMD_PARA1 0x3025
#define OV5640_CMD_PARA2 0x3026
#define OV5640_CMD_PARA3 0x3027
#define OV5640_CMD_PARA4 0x3028
#define OV5640_FW_STATUS 0x3029

/* BEGIN Dom_Lin@pegatron [2012/04/18] [Update AF firmware to version OV5640-AD5820-ASUS-13011000-20120416140547] */
#define OV5640_AF_TIMEOUT     4200 // approximate 5 sec.
#define OV5640_AF_DELAY         10 // 10 ms
#define OV5640_AF_CMD_TIMEOUT 1700 // approximate 2 sec.
/* END Dom_Lin@pegatron [2012/04/18] [Update AF firmware to version OV5640-AD5820-ASUS-13011000-20120416140547] */

#define OV5640_AUTO_FLASH_GAIN 0x0050 // 0x0040

typedef enum {
	OV5640_FAILED,
	OV5640_SUCCESS,
/* BEGIN Dom_Lin@pegatron [2012/04/26] [Add auto focus error handling and tuning work queue lock timing] */
	OV5640_ABORT,
/* END Dom_Lin@pegatron [2012/04/26] [Add auto focus error handling and tuning work queue lock timing] */
} ov5640_status_t;

typedef enum {
	OV5640_CMD_AF,
/* BEGIN Dom_Lin@pegatron [2012/04/26] [Add auto focus error handling and tuning work queue lock timing] */
	OV5640_CMD_TOUCH_AF,
	OV5640_CMD_AF_ABORT,
/* END Dom_Lin@pegatron [2012/04/26] [Add auto focus error handling and tuning work queue lock timing] */
} ov5640_work_cmd_t;

struct ov5640_work {
	struct delayed_work m_work;
	ov5640_work_cmd_t cmd;
	struct msm_sensor_ctrl_t *s_ctrl;
};

static struct workqueue_struct *ov5640_wq;
static void ov5640_worker(struct work_struct *work);
/* END Dom_Lin@pegatron [2012/02/04] [Enable back camera auto focus] */

DEFINE_MUTEX(ov5640_work_mutex);
/* BEGIN Dom_Lin@pegatron [2012/04/26] [Add auto focus error handling and tuning work queue lock timing] */
DEFINE_MUTEX(ov5640_af_mutex);
/* END Dom_Lin@pegatron [2012/04/26] [Add auto focus error handling and tuning work queue lock timing] */

/* BEGIN Dom_Lin@pegatron [2012/02/02] [Enable auto flash function in 1024 codebase] */
static ov5640_strobe_flash_mode_t ov5640_flash_mode;
/* END Dom_Lin@pegatron [2012/02/02] [Enable auto flash function in 1024 codebase] */

static uint8_t ov5640_read_otp = 1;

#if !defined(PEGA_SMT_BUILD)
static struct regulator* ov5640_reg_af = NULL;
#endif

/* BEGIN Dom_Lin@pegatron [2012/04/12] [Reset VCM position after changing resolution] */
#if defined(OV5640_SUSPEND_CHANGE)
static uint8_t ov5640_reset_vcm = 0;
static uint16_t ov5640_vcm_position = 0;
#endif
/* END Dom_Lin@pegatron [2012/04/12] [Reset VCM position after changing resolution] */
static uint8_t ov5640_enable_MCU = 0;

/* BEGIN Dom_Lin@pegatron [2012/04/17] [Pre-flash for focusing] */
static uint8_t ov5640_pre_flash = 0;
#if defined(OV5640_SUSPEND_CHANGE)
static uint8_t ov5640_flash_delay = 0;
#endif
/* END Dom_Lin@pegatron [2012/04/17] [Pre-flash for focusing] */
typedef enum {
	OV5640_PRE_FLASH_NONE,
	OV5640_PRE_FLASH_ON,
	OV5640_PRE_FLASH_OFF,
} ov5640_pre_flash_status_t;

static ov5640_pre_flash_status_t ov5640_pre_flash_status = OV5640_PRE_FLASH_NONE;

/* BEGIN Dom_Lin@pegatron [2012/04/26] [Add auto focus error handling and tuning work queue lock timing] */
static uint8_t ov5640_streaming = 0;
static uint8_t ov5640_af_stop_waiting = 1;
/* END Dom_Lin@pegatron [2012/04/26] [Add auto focus error handling and tuning work queue lock timing] */

static uint8_t ov5640_strobe = 0;

static uint8_t ov5640_sharpness_value = 6; // default level for preview, Sharpness Auto +1
static ov5640_sensor_mode_t ov5640_sensor_mode = SENSOR_MODE_INVALID;
static int32_t ov5640_sensor_set_sharpness(struct msm_sensor_ctrl_t *s_ctrl, uint8_t val);

static uint8_t ov5640_effect_value = CAMERA_EFFECT_OFF; // default value, Normal
static uint8_t ov5640_saturation_value = 6; // default value
static uint8_t ov5640_next_saturation_value = 6; // default value
static int32_t ov5640_sensor_set_saturation(struct msm_sensor_ctrl_t *s_ctrl, uint8_t val);

DEFINE_MUTEX(ov5640_mut);
static struct msm_sensor_ctrl_t ov5640_s_ctrl;

/*=============================================================
                   SENSOR REGISTER SETTINGS
==============================================================*/
#include "ov5640_v4l2_reg_v1.h"
#include "ov5640_v4l2_reg_v2.h"
#include "ov5640_v4l2_reg_common.h"

#define OV5640_DEF_ARRAY_SIZE(name) static uint32_t name##_size = ARRAY_SIZE(name)
#define OV5640_DEF_2D_ARRAY_SIZE(name) \
	static uint32_t name##_2d_size = ARRAY_SIZE(name[0]); \
	OV5640_DEF_ARRAY_SIZE(name)

#define OV5640_INIT_MAP(name) \
	static uint8_t* name = NULL; \
	static uint32_t name##_size = 0

#define OV5640_INIT_2D_REG_CONF(name, y_size) \
	static struct msm_camera_i2c_reg_conf (*name)[y_size] = NULL; \
	static uint32_t name##_size = 0; \
	static uint32_t name##_2d_size = 0

#define OV5640_INIT_REG_CONF(name) \
	static struct msm_camera_i2c_reg_conf* name = NULL; \
	static uint32_t name##_size = 0

#define OV5640_ASSIGN_ARRAY(name, version) \
	name = name##_##version; \
	name##_size = name##_##version##_size

#define OV5640_ASSIGN_2D_ARRAY(name, version) \
	name = name##_##version; \
	name##_size = name##_##version##_size; \
	name##_2d_size = name##_##version##_2d_size

#define OV5640_ARRAY_SIZE(name) name##_size
#define OV5640_2D_ARRAY_SIZE(name) name##_2d_size

#define OV5640_ADD_REG(version) \
OV5640_DEF_ARRAY_SIZE(ov5640_antibanding_map_##version); \
OV5640_DEF_2D_ARRAY_SIZE(ov5640_antibanding_settings_##version); \
 \
OV5640_DEF_ARRAY_SIZE(ov5640_exposure_map_##version); \
OV5640_DEF_2D_ARRAY_SIZE(ov5640_exposure_settings_##version); \
 \
OV5640_DEF_ARRAY_SIZE(ov5640_contrast_map_##version); \
OV5640_DEF_2D_ARRAY_SIZE(ov5640_contrast_settings_##version); \
 \
OV5640_DEF_ARRAY_SIZE(ov5640_sharpness_map_##version); \
OV5640_DEF_2D_ARRAY_SIZE(ov5640_sharpness_settings_##version); \
 \
OV5640_DEF_ARRAY_SIZE(ov5640_saturation_map_##version); \
OV5640_DEF_2D_ARRAY_SIZE(ov5640_saturation_settings_##version); \
 \
OV5640_DEF_ARRAY_SIZE(ov5640_bestshot_mode_map_##version); \
OV5640_DEF_2D_ARRAY_SIZE(ov5640_bestshot_mode_settings_##version); \
 \
OV5640_DEF_ARRAY_SIZE(ov5640_effect_map_##version); \
OV5640_DEF_2D_ARRAY_SIZE(ov5640_effect_settings_##version); \
 \
OV5640_DEF_ARRAY_SIZE(ov5640_wb_map_##version); \
OV5640_DEF_2D_ARRAY_SIZE(ov5640_wb_settings_##version); \
 \
OV5640_DEF_ARRAY_SIZE(ov5640_recommend_settings_##version); \

OV5640_ADD_REG(v1);
OV5640_ADD_REG(v2);

OV5640_INIT_MAP(ov5640_antibanding_map);
OV5640_INIT_2D_REG_CONF(ov5640_antibanding_settings, 14);

OV5640_INIT_MAP(ov5640_exposure_map);
OV5640_INIT_2D_REG_CONF(ov5640_exposure_settings, 6);

OV5640_INIT_MAP(ov5640_contrast_map);
OV5640_INIT_2D_REG_CONF(ov5640_contrast_settings, 5);

OV5640_INIT_MAP(ov5640_sharpness_map);
OV5640_INIT_2D_REG_CONF(ov5640_sharpness_settings, 9);

OV5640_INIT_MAP(ov5640_saturation_map);
OV5640_INIT_2D_REG_CONF(ov5640_saturation_settings, 5);

OV5640_INIT_MAP(ov5640_bestshot_mode_map);
OV5640_INIT_2D_REG_CONF(ov5640_bestshot_mode_settings, 58);

OV5640_INIT_MAP(ov5640_effect_map);
OV5640_INIT_2D_REG_CONF(ov5640_effect_settings, 9);

OV5640_INIT_MAP(ov5640_wb_map);
OV5640_INIT_2D_REG_CONF(ov5640_wb_settings, 7);

OV5640_INIT_REG_CONF(ov5640_recommend_settings);

/*=============================================================
                       DATA DECLARATIONS
==============================================================*/
static struct v4l2_subdev_info ov5640_subdev_info[] = {
	{
	.code   = V4L2_MBUS_FMT_YUYV8_2X8,
	.colorspace = V4L2_COLORSPACE_JPEG,
	.fmt    = 1,
	.order    = 0,
	},
	/* more can be supported, to be added later */
};

static struct msm_camera_i2c_conf_array ov5640_init_conf[] = {
	{ov5640_init_reset_settings,
	ARRAY_SIZE(ov5640_init_reset_settings), 20, MSM_CAMERA_I2C_BYTE_DATA_AUTO},
	{ov5640_recommend_settings_dummy,
	ARRAY_SIZE(ov5640_recommend_settings_dummy), 0, MSM_CAMERA_I2C_BYTE_DATA_AUTO},
/* BEGIN Dom_Lin@pegatron [2012/02/04] [Enable back camera auto focus] */
	{ov5640_af_settings,
	ARRAY_SIZE(ov5640_af_settings), 0,  MSM_CAMERA_I2C_BYTE_DATA_AUTO},
/* END Dom_Lin@pegatron [2012/02/04] [Enable back camera auto focus] */
#if defined(OV5640_SUSPEND_CHANGE)
	{ov5640_start_stream_settings,
	ARRAY_SIZE(ov5640_start_stream_settings), 0, MSM_CAMERA_I2C_BYTE_DATA_AUTO},
#endif
};

/* BEGIN Dom_Lin@pegatron [2012/02/10] [Enable back camera all supported picture size and preview size] */
static struct msm_camera_i2c_conf_array ov5640_confs[] = {
	/* 2592x1944, 4:3, MSM_SENSOR_RES_FULL */
	{ov5640_preview_2592x1944_settings,
	ARRAY_SIZE(ov5640_preview_2592x1944_settings), 20, MSM_CAMERA_I2C_BYTE_DATA},
	/* 1920x1088, 16:9 (dividable by 16), MSM_SENSOR_RES_QTR */
	{ov5640_preview_1920x1088_settings,
	ARRAY_SIZE(ov5640_preview_1920x1088_settings), 20, MSM_CAMERA_I2C_BYTE_DATA},
	/* 1920x1080, 16:9, MSM_SENSOR_RES_2 */
	{ov5640_preview_1920x1080_settings,
	ARRAY_SIZE(ov5640_preview_1920x1080_settings), 20, MSM_CAMERA_I2C_BYTE_DATA},
	/* 1280x960, 4:3, MSM_SENSOR_RES_3 */
	{ov5640_preview_1280x960_settings,
	ARRAY_SIZE(ov5640_preview_1280x960_settings), 20, MSM_CAMERA_I2C_BYTE_DATA},
	/* 1280x768, 5:3, MSM_SENSOR_RES_4 */
	{ov5640_preview_1280x768_settings,
	ARRAY_SIZE(ov5640_preview_1280x768_settings), 20, MSM_CAMERA_I2C_BYTE_DATA},
	/* 1280x720, 16:9, MSM_SENSOR_RES_5 */
	{ov5640_preview_1280x720_settings,
	ARRAY_SIZE(ov5640_preview_1280x720_settings), 20, MSM_CAMERA_I2C_BYTE_DATA},
	/* 1024x768, 4:3, MSM_SENSOR_RES_6 */
	{ov5640_preview_1024x768_settings,
	ARRAY_SIZE(ov5640_preview_1024x768_settings), 20, MSM_CAMERA_I2C_BYTE_DATA},
};

static struct msm_sensor_output_info_t ov5640_dimensions[] = {
	{ /* 2592x1944, 4:3, MSM_SENSOR_RES_FULL */
		.x_output = 0x0a20, // addr 0x3808, 0x3809
		.y_output = 0x0798, // addr 0x380a, 0x380b
		.line_length_pclk = 0x0b1c, // addr 0x380c, 0x380d
		.frame_length_lines = 0x07b0, // addr 0x380e, 0x380f
		.vt_pixel_clk = 228570000, // unknown
		.op_pixel_clk = 228570000, // unknown
		.binning_factor = 1,
	},
	{ /* 1920x1088, 16:9 (dividable by 16), MSM_SENSOR_RES_QTR */
		.x_output = 0x0780, // addr 0x3808, 0x3809
		.y_output = 0x0440, // addr 0x380a, 0x380b
		.line_length_pclk = 0x09c4, // addr 0x380c, 0x380d
		.frame_length_lines = 0x0460, // addr 0x380e, 0x380f
		.vt_pixel_clk = 228570000, // unknown
		.op_pixel_clk = 228570000, // unknown
		.binning_factor = 1,
	},
	{ /* 1920x1080, 16:9, MSM_SENSOR_RES_2 */
		.x_output = 0x0780, // addr 0x3808, 0x3809
		.y_output = 0x0438, // addr 0x380a, 0x380b
		.line_length_pclk = 0x09c4, // addr 0x380c, 0x380d
		.frame_length_lines = 0x0460, // addr 0x380e, 0x380f
		.vt_pixel_clk = 228570000, // unknown
		.op_pixel_clk = 228570000, // unknown
		.binning_factor = 1,
	},
	{ /* 1280x960, 4:3, MSM_SENSOR_RES_3 */
		.x_output = 0x0500, // addr 0x3808, 0x3809
		.y_output = 0x03c0, // addr 0x380a, 0x380b
		.line_length_pclk = 0x0768, // addr 0x380c, 0x380d
		.frame_length_lines = 0x03d8, // addr 0x380e, 0x380f
		.vt_pixel_clk = 228570000, // unknown
		.op_pixel_clk = 228570000, // unknown
		.binning_factor = 1,
	},
	{ /* 1280x768, 5:3, MSM_SENSOR_RES_4 */
		.x_output = 0x0500, // addr 0x3808, 0x3809
		.y_output = 0x0300, // addr 0x380a, 0x380b
		.line_length_pclk = 0x0768, // addr 0x380c, 0x380d
		.frame_length_lines = 0x03d8, // addr 0x380e, 0x380f
		.vt_pixel_clk = 228570000, // unknown
		.op_pixel_clk = 228570000, // unknown
		.binning_factor = 1,
	},
	{ /* 1280x720, 16:9, MSM_SENSOR_RES_5 */
		.x_output = 0x0500, // addr 0x3808, 0x3809
		.y_output = 0x02D0, // addr 0x380a, 0x380b
		.line_length_pclk = 0x0768, // addr 0x380c, 0x380d
		.frame_length_lines = 0x03d8, // addr 0x380e, 0x380f
		.vt_pixel_clk = 228570000, // unknown
		.op_pixel_clk = 228570000, // unknown
//qcom		.vt_pixel_clk = 192000000, // unknown
//qcom		.op_pixel_clk = 192000000, // unknown
//max		.vt_pixel_clk = 216000000, // unknown
//max		.op_pixel_clk = 216000000, // unknown
		.binning_factor = 1,
	},
	{ /* 1024x768, 4:3, MSM_SENSOR_RES_6 */
		.x_output = 0x0400, // addr 0x3808, 0x3809
		.y_output = 0x0300, // addr 0x380a, 0x380b
		.line_length_pclk = 0x0768, // addr 0x380c, 0x380d
		.frame_length_lines = 0x03d8, // addr 0x380e, 0x380f
		.vt_pixel_clk = 228570000, // unknown
		.op_pixel_clk = 228570000, // unknown
		.binning_factor = 1,
	},
};
/* END Dom_Lin@pegatron [2012/02/10] [Enable back camera all supported picture size and preview size] */

static struct msm_camera_csid_vc_cfg ov5640_cid_cfg[] = {
	{0, CSI_YUV422_8, CSI_DECODE_8BIT},
	{1, CSI_EMBED_DATA, CSI_DECODE_8BIT},
};

static struct msm_camera_csi2_params ov5640_csi_params[] = {
	{ /* 2592x1944, 4:3, MSM_SENSOR_RES_FULL */
		.csid_params = {
			.lane_assign = 0xe4,
			.lane_cnt = 2,
			.lut_params = {
				.num_cid = 2,
				.vc_cfg = ov5640_cid_cfg,
			},
		},
		.csiphy_params = {
			.lane_cnt = 2,
			.settle_cnt = 0x09,
		},
	},
	{ /* 1920x1088, 16:9 (dividable by 16), MSM_SENSOR_RES_QTR */
		.csid_params = {
			.lane_assign = 0xe4,
			.lane_cnt = 2,
			.lut_params = {
				.num_cid = 2,
				.vc_cfg = ov5640_cid_cfg,
			},
		},
		.csiphy_params = {
			.lane_cnt = 2,
			.settle_cnt = 0x09,
		},
	},
	{ /* 1920x1080, 16:9, MSM_SENSOR_RES_2 */
		.csid_params = {
			.lane_assign = 0xe4,
			.lane_cnt = 2,
			.lut_params = {
				.num_cid = 2,
				.vc_cfg = ov5640_cid_cfg,
			},
		},
		.csiphy_params = {
			.lane_cnt = 2,
			.settle_cnt = 0x09,
		},
	},
	{ /* 1280x960, 4:3, MSM_SENSOR_RES_3 */
		.csid_params = {
			.lane_assign = 0xe4,
			.lane_cnt = 2,
			.lut_params = {
				.num_cid = 2,
				.vc_cfg = ov5640_cid_cfg,
			},
		},
		.csiphy_params = {
			.lane_cnt = 2,
			.settle_cnt = 0x09,
		},
	},
	{ /* 1280x768, 5:3, MSM_SENSOR_RES_4 */
		.csid_params = {
			.lane_assign = 0xe4,
			.lane_cnt = 2,
			.lut_params = {
				.num_cid = 2,
				.vc_cfg = ov5640_cid_cfg,
			},
		},
		.csiphy_params = {
			.lane_cnt = 2,
			.settle_cnt = 0x09,
		},
	},
	{ /* 1280x720, 16:9, MSM_SENSOR_RES_5 */
		.csid_params = {
			.lane_assign = 0xe4,
			.lane_cnt = 2,
			.lut_params = {
				.num_cid = 2,
				.vc_cfg = ov5640_cid_cfg,
			},
		},
		.csiphy_params = {
			.lane_cnt = 2,
			.settle_cnt = 0x09,
		},
	},
	{ /* 1024x768, 4:3, MSM_SENSOR_RES_6 */
		.csid_params = {
			.lane_assign = 0xe4,
			.lane_cnt = 2,
			.lut_params = {
				.num_cid = 2,
				.vc_cfg = ov5640_cid_cfg,
			},
		},
		.csiphy_params = {
			.lane_cnt = 2,
			.settle_cnt = 0x09,
		},
	},
};

/* BEGIN Dom_Lin@pegatron [2012/02/10] [Enable back camera all supported picture size and preview size] */
static struct msm_camera_csi2_params *ov5640_csi_params_array[] = {
	&ov5640_csi_params[0], /* 2592x1944, 4:3, MSM_SENSOR_RES_FULL */
	&ov5640_csi_params[1], /* 1920x1088, 16:9 (dividable by 16), MSM_SENSOR_RES_QTR */
	&ov5640_csi_params[2], /* 1920x1080, 16:9, MSM_SENSOR_RES_2 */
	&ov5640_csi_params[3], /* 1280x960, 4:3, MSM_SENSOR_RES_3 */
	&ov5640_csi_params[4], /* 1280x768, 5:3, MSM_SENSOR_RES_4 */
	&ov5640_csi_params[5], /* 1280x720, 16:9, MSM_SENSOR_RES_5 */
	&ov5640_csi_params[6], /* 1024x768, 4:3, MSM_SENSOR_RES_6 */
};
/* END Dom_Lin@pegatron [2012/02/10] [Enable back camera all supported picture size and preview size] */

static struct msm_sensor_output_reg_addr_t ov5640_reg_addr = {
	.x_output = 0x3808,
	.y_output = 0x380a,
	.line_length_pclk = 0x380c,
	.frame_length_lines = 0x380e,
};

static struct msm_sensor_id_info_t ov5640_id_info = {
	.sensor_id_reg_addr = 0x300A,
	.sensor_id = 0x5640,
};

/*=============================================================
                          FUNCTIONS
==============================================================*/
/* BEGIN Dom_Lin@pegatron [2012/03/01] [Read back camera OTP ID] */
int32_t ov5640_otp_read(struct msm_camera_i2c_client *client, uint32_t *data)
{
	int32_t rc = 0;
	uint8_t* pdata = (uint8_t*)data;
	rc = msm_camera_i2c_write_with_mask(client, 0x3000, 0x0000, 0x0010, MSM_CAMERA_I2C_BYTE_DATA);
	if (rc < 0) {
		CDBGE("Reset OTP failed\n");
		goto done;
	}
	rc = msm_camera_i2c_write_with_mask(client, 0x3004, 0x0010, 0x0010, MSM_CAMERA_I2C_BYTE_DATA);
	if (rc < 0) {
		CDBGE("Enable OTP clock failed\n");
		goto done;
	}

#if 0
	rc = msm_camera_i2c_write(client, 0x3D20, 0x00, MSM_CAMERA_I2C_BYTE_DATA);
	if (rc < 0) {
		CDBGE("OTP program ctrl failed\n");
		goto done;
	}
	rc = msm_camera_i2c_write(client, 0x3D21, 0x00, MSM_CAMERA_I2C_BYTE_DATA);
	if (rc < 0) {
		CDBGE("OTP read disable failed\n");
		goto done;
	}
#endif

	rc = msm_camera_i2c_write(client, 0x3D21, 0x01, MSM_CAMERA_I2C_BYTE_DATA);
	if (rc < 0) {
		CDBGE("OTP read enable failed\n");
		goto done;
	}

	rc = msm_camera_i2c_read(client, 0x3D0C, (uint16_t*)(pdata+2), MSM_CAMERA_I2C_WORD_DATA);
	if (rc < 0) {
		CDBGE("OTP read data failed\n");
		goto done;
	}
	rc = msm_camera_i2c_read(client, 0x3D0E, (uint16_t*)pdata, MSM_CAMERA_I2C_WORD_DATA);
	if (rc < 0) {
		CDBGE("OTP read data failed\n");
		goto done;
	}

	usleep_range(10000, 11000);
	rc = msm_camera_i2c_write(client, 0x3D21, 0x00, MSM_CAMERA_I2C_BYTE_DATA);
	if (rc < 0) {
		CDBGE("OTP read disable failed\n");
		goto done;
	}

	rc = msm_camera_i2c_write_with_mask(client, 0x3000, 0x0010, 0x0010, MSM_CAMERA_I2C_BYTE_DATA);
	if (rc < 0) {
		CDBGE("Reset OTP failed\n");
		goto done;
	}
	rc = msm_camera_i2c_write_with_mask(client, 0x3004, 0x0000, 0x0010, MSM_CAMERA_I2C_BYTE_DATA);
	if (rc < 0) {
		CDBGE("Enable OTP clock failed\n");
	}

done:
	return rc;
}
/* END Dom_Lin@pegatron [2012/03/01] [Read back camera OTP ID] */

/* BEGIN Dom_Lin@pegatron [2012/02/02] [Init camera strobe flash function in 1024 codebase] */
static int32_t ov5640_strobe_high(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	CDBG("+++\n");
	if (!ov5640_strobe) {
		rc = msm_camera_i2c_write_tbl(
			s_ctrl->sensor_i2c_client,
			ov5640_strobe_high_settings,
			ARRAY_SIZE(ov5640_strobe_high_settings),
			MSM_CAMERA_I2C_BYTE_DATA_AUTO);
		ov5640_strobe = 1;
	}
	return rc;
}

static int32_t ov5640_strobe_low(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	CDBG("+++\n");
	if (ov5640_strobe) {
		rc = msm_camera_i2c_write_tbl(
			s_ctrl->sensor_i2c_client,
			ov5640_strobe_low_settings,
			ARRAY_SIZE(ov5640_strobe_low_settings),
			MSM_CAMERA_I2C_BYTE_DATA_AUTO);
		ov5640_strobe = 0;
	}
	return rc;
}

/* BEGIN Dom_Lin@pegatron [2012/02/02] [Enable auto flash function in 1024 codebase] */
void ov5640_set_flash_mode(int mode)
{
	CDBG("+++");
	ov5640_flash_mode = (ov5640_strobe_flash_mode_t)mode;
#if defined(OV5640_SUSPEND_CHANGE)
	ov5640_flash_delay = 0;
#endif
	ov5640_pre_flash_status = OV5640_PRE_FLASH_NONE;
}
EXPORT_SYMBOL(ov5640_set_flash_mode);
/* END Dom_Lin@pegatron [2012/02/02] [Enable auto flash function in 1024 codebase] */

/* BEGIN Dom_Lin@pegatron [2012/04/18] [Update AF firmware to version OV5640-AD5820-ASUS-13011000-20120416140547] */
static int32_t ov5640_release_af(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	uint16_t data = 0x0001;
	int32_t timeout = OV5640_AF_CMD_TIMEOUT;

	CDBG("+++\n");

	if (ov5640_enable_MCU) {
		ov5640_enable_MCU = 0;
		rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3004, 0xFF, MSM_CAMERA_I2C_BYTE_DATA);
		OV5640_RC_CHECK("Enable MCU clock");
	}

#ifdef OV5640_AF_ACK
	rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, OV5640_CMD_ACK, 0x01, MSM_CAMERA_I2C_BYTE_DATA);
	OV5640_RC_CHECK("AF CMD_ACK");
#endif
	rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, OV5640_CMD_MAIN, 0x08, MSM_CAMERA_I2C_BYTE_DATA);
	OV5640_RC_CHECK("AF CMD_MAIN");
#ifdef OV5640_AF_ACK
	while (timeout) {
		rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, OV5640_CMD_ACK, &data, MSM_CAMERA_I2C_BYTE_DATA);
		if (rc < 0) {
			CDBGE("AF read CMD_ACK failed\n");
			goto done;
		}
		if (data == 0x0000)
			break;
		usleep_range(OV5640_AF_DELAY*1000, (OV5640_AF_DELAY+1)*1000);
		timeout = timeout - OV5640_AF_DELAY;
//		CDBG("AF read CMD_ACK again, timeout = %d\n", timeout);
	}
	if (data == 0x0001) {
		CDBG("AF timeout\n");
		goto done;
	}
#endif

done:
	return rc;
}

#ifdef OV5640_AF_CUSTOM_FOCUS_ZONE
static int32_t ov5640_launch_custom_focus_zone(struct msm_sensor_ctrl_t *s_ctrl, struct sensor_roi_t roi)
{
	int32_t rc = 0;

	CDBG("+++\n");
	ov5640_af_stop_waiting = 1;
	mutex_lock(&ov5640_af_mutex);

	CDBG("x = %u, y = %u, dx = %u, dy = %u\n", roi.x, roi.y, roi.dx, roi.dy);

	rc = ov5640_release_af(s_ctrl);
	OV5640_RC_CHECK("Release AF");

// Enable Custom Focus Zone Configure
#ifdef OV5640_AF_ACK
#if 0
	rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, OV5640_CMD_ACK, 0x01, MSM_CAMERA_I2C_BYTE_DATA);
	OV5640_RC_CHECK("AF CMD_ACK");
#endif
#endif
	rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, OV5640_CMD_MAIN, 0x8f, MSM_CAMERA_I2C_BYTE_DATA);
	OV5640_RC_CHECK("AF CMD_MAIN");

// Configure Focus Zone
	rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, OV5640_CMD_PARA0, roi.x, MSM_CAMERA_I2C_BYTE_DATA);
	OV5640_RC_CHECK("AF CMD_PARA0");
	rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, OV5640_CMD_PARA1, roi.y, MSM_CAMERA_I2C_BYTE_DATA);
	OV5640_RC_CHECK("AF CMD_PARA1");
	rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, OV5640_CMD_PARA2, roi.dx, MSM_CAMERA_I2C_BYTE_DATA);
	OV5640_RC_CHECK("AF CMD_PARA2");
	rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, OV5640_CMD_PARA3, roi.dy, MSM_CAMERA_I2C_BYTE_DATA);
	OV5640_RC_CHECK("AF CMD_PARA3");
#ifdef OV5640_AF_ACK
#if 0
	rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, OV5640_CMD_ACK, 0x01, MSM_CAMERA_I2C_BYTE_DATA);
	OV5640_RC_CHECK("AF CMD_ACK");
#endif
#endif
	rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, OV5640_CMD_MAIN, 0x90, MSM_CAMERA_I2C_BYTE_DATA);
	OV5640_RC_CHECK("AF CMD_MAIN");

// Launch Custom Focus Zones
#ifdef OV5640_AF_ACK
#if 0
	rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, OV5640_CMD_ACK, 0x01, MSM_CAMERA_I2C_BYTE_DATA);
	OV5640_RC_CHECK("AF CMD_ACK");
#endif
#endif
	rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, OV5640_CMD_MAIN, 0x9f, MSM_CAMERA_I2C_BYTE_DATA);
	OV5640_RC_CHECK("AF_CMD_MAIN");

// Trig Single Auto Focus
#ifdef OV5640_AF_ACK
	rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, OV5640_CMD_ACK, 0x01, MSM_CAMERA_I2C_BYTE_DATA);
	OV5640_RC_CHECK("AF CMD_ACK");
#endif
	rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, OV5640_CMD_MAIN, 0x03, MSM_CAMERA_I2C_BYTE_DATA);
	OV5640_RC_CHECK("AF_CMD_MAIN");

done:
	ov5640_af_stop_waiting = 0;
	mutex_unlock(&ov5640_af_mutex);
	CDBG("---\n");
	return rc;
}
#else // OV5640_AF_CUSTOM_FOCUS_ZONE
static int32_t ov5640_launch_touch_focus_zone(struct msm_sensor_ctrl_t *s_ctrl, struct sensor_roi_t roi)
{
	int32_t rc = 0;
	uint16_t data = 0x0001;
	int32_t timeout = OV5640_AF_CMD_TIMEOUT;

	CDBG("+++\n");
	ov5640_af_stop_waiting = 1;
	mutex_lock(&ov5640_af_mutex);

	CDBG("x = %u, y = %u, dx = %u, dy = %u\n", roi.x, roi.y, roi.dx, roi.dy);

	rc = ov5640_release_af(s_ctrl);
	OV5640_RC_CHECK("Release AF");

// Launch Touch mode Focus Zones
	rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, OV5640_CMD_PARA0, roi.x, MSM_CAMERA_I2C_BYTE_DATA);
	OV5640_RC_CHECK("AF CMD_PARA0");
	rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, OV5640_CMD_PARA1, roi.y, MSM_CAMERA_I2C_BYTE_DATA);
	OV5640_RC_CHECK("AF CMD_PARA1");
#ifdef OV5640_AF_ACK
	rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, OV5640_CMD_ACK, 0x01, MSM_CAMERA_I2C_BYTE_DATA);
	OV5640_RC_CHECK("AF CMD_ACK");
#endif
	rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, OV5640_CMD_MAIN, 0x81, MSM_CAMERA_I2C_BYTE_DATA);
	OV5640_RC_CHECK("AF CMD_MAIN");
#ifdef OV5640_AF_ACK
	while (timeout) {
		rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, OV5640_CMD_ACK, &data, MSM_CAMERA_I2C_BYTE_DATA);
		if (rc < 0) {
			CDBGE("AF read CMD_ACK failed\n");
			goto done;
		}
		if (data == 0x0000)
			break;
		usleep_range(OV5640_AF_DELAY*1000, (OV5640_AF_DELAY+1)*1000);
		timeout = timeout - OV5640_AF_DELAY;
//		CDBG("AF read CMD_ACK again, timeout = %d\n", timeout);
	}
	if (data == 0x0001) {
		CDBG("AF tiemout\n");
		goto done;
	}
#endif

// Trig Single Auto Focus
#ifdef OV5640_AF_ACK
	rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, OV5640_CMD_ACK, 0x01, MSM_CAMERA_I2C_BYTE_DATA);
	OV5640_RC_CHECK("AF CMD_ACK");
#endif
	rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, OV5640_CMD_MAIN, 0x03, MSM_CAMERA_I2C_BYTE_DATA);
	OV5640_RC_CHECK("AF_CMD_MAIN");

done:
	ov5640_af_stop_waiting = 0;
	mutex_unlock(&ov5640_af_mutex);
	CDBG("---\n");
	return rc;
}
#endif // OV5640_AF_CUSTOM_FOCUS_ZONE

static int32_t ov5640_trig_af(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;

#if 0 // for Version: OV5640-AD5820-ASUS-13011000-20120416140547.ovt
	struct sensor_roi_t roi;

	CDBG("+++\n");
	ov5640_af_stop_waiting = 1;
	mutex_lock(&ov5640_af_mutex);

	switch (s_ctrl->curr_res) {
	case MSM_SENSOR_RES_QTR: /* 1920x1088, 16:9 (dividable by 16), MSM_SENSOR_RES_QTR */
	case MSM_SENSOR_RES_2:   /* 1920x1080, 16:9, MSM_SENSOR_RES_2 */
	case MSM_SENSOR_RES_5:   /* 1280x720, 16:9, MSM_SENSOR_RES_5 */
		roi.x = 80/2;
		roi.y = 45/2;
		break;
	case MSM_SENSOR_RES_4:   /* 1280x768, 5:3, MSM_SENSOR_RES_4 */
		roi.x = 80/2;
		roi.y = 48/2;
		break;
	default:
		roi.x = 80/2;
		roi.y = 60/2;
	}
	roi.dx = 0;
	roi.dy = 0;

	rc = ov5640_launch_touch_focus_zone(s_ctrl, roi);
	if (rc < 0) {
		CDBGE("ov5640_launch_touch_focus_zone failed\n");
		goto done;
	}

#else // for Version: OV5640-AD5820-ASUS-13011000-20120416140547.ovt
	uint16_t data = 0x0001;
	int32_t timeout = OV5640_AF_CMD_TIMEOUT;

	CDBG("+++\n");
	ov5640_af_stop_waiting = 1;
	mutex_lock(&ov5640_af_mutex);

	rc = ov5640_release_af(s_ctrl);
	OV5640_RC_CHECK("Release AF");

	// for Version: OV5640-AD5820-ASUS-13011000-20120416140547.ovt
	rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, OV5640_CMD_PARA4, 0x00, MSM_CAMERA_I2C_BYTE_DATA);
	OV5640_RC_CHECK("AF CMD_PARA4");

// Re-launch default Focus Zones
#ifdef OV5640_AF_ACK
	rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, OV5640_CMD_ACK, 0x01, MSM_CAMERA_I2C_BYTE_DATA);
	OV5640_RC_CHECK("AF CMD_ACK");
#endif
	rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, OV5640_CMD_MAIN, 0x80, MSM_CAMERA_I2C_BYTE_DATA);
	OV5640_RC_CHECK("AF CMD_MAIN");
#ifdef OV5640_AF_ACK
	while (timeout) {
		rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, OV5640_CMD_ACK, &data, MSM_CAMERA_I2C_BYTE_DATA);
		if (rc < 0) {
			CDBGE("AF read CMD_ACK failed\n");
			goto done;
		}
		if (data == 0x0000)
			break;
		usleep_range(OV5640_AF_DELAY*1000, (OV5640_AF_DELAY+1)*1000);
		timeout = timeout - OV5640_AF_DELAY;
//		CDBG("AF read CMD_ACK again, timeout = %d\n", timeout);
	}
	if (data == 0x0001) {
		CDBG("AF tiemout\n");
		goto done;
	}
#endif

// Trig Single Auto Focus
#ifdef OV5640_AF_ACK
	rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, OV5640_CMD_ACK, 0x01, MSM_CAMERA_I2C_BYTE_DATA);
	OV5640_RC_CHECK("AF CMD_ACK");
#endif
	rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, OV5640_CMD_MAIN, 0x03, MSM_CAMERA_I2C_BYTE_DATA);
	OV5640_RC_CHECK("AF CMD_MAIN");

#endif // for Version: OV5640-AD5820-ASUS-13011000-20120416140547.ovt

done:
	ov5640_af_stop_waiting = 0;
	mutex_unlock(&ov5640_af_mutex);
	CDBG("---\n");
	return rc;
}
/* END Dom_Lin@pegatron [2012/04/18] [Update AF firmware to version OV5640-AD5820-ASUS-13011000-20120416140547] */

/* BEGIN Dom_Lin@pegatron [2012/04/17] [Pre-flash for focusing] */
static int32_t ov5640_pre_flash_on(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	uint16_t gain = 0x0000;
#if defined(OV5640_SUSPEND_CHANGE)
	uint16_t avg = 0x0000;
#endif

#if defined(CONFIG_LEDS_LM3559) && defined(CONFIG_MSM_CAMERA_FLASH_LM3559)
	CDBG("+++\n");

	switch(ov5640_flash_mode) {
	case STROBE_FLASH_MODE_ON:
	case STROBE_FLASH_MODE_AUTO:
		rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, 0x350b, &gain, MSM_CAMERA_I2C_BYTE_DATA);
		if (rc < 0) {
			CDBGE("read real gain failed\n");
			goto done;
		}
		CDBG("Real gain is %u\n", gain);

#if defined(OV5640_SUSPEND_CHANGE)
		if (gain > 0x007F) {
			rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, 0x56a1, &avg, MSM_CAMERA_I2C_BYTE_DATA);
			if (rc < 0) {
				CDBGE("read AVG READOUT failed\n");
				goto done;
			}
			CDBG("AVG READOUT is %u\n", avg);

#if defined(CONFIG_LEDS_LM3559) && defined(CONFIG_MSM_CAMERA_FLASH_LM3559)
			if (gain >= 0x007F && avg <= 0x0004) {
				ov5640_flash_delay = 1;
			}
#endif
		}
#endif // OV5640_SUSPEND_CHANGE

		if (ov5640_flash_mode == STROBE_FLASH_MODE_ON) {
			rc = lm3559_set_led(MSM_CAMERA_LED_LOW);
			ov5640_pre_flash = 1;
			ov5640_pre_flash_status = OV5640_PRE_FLASH_ON;
		}
		else if (gain > OV5640_AUTO_FLASH_GAIN) {
			rc = lm3559_set_led(MSM_CAMERA_LED_LOW);
			ov5640_pre_flash = 1;
			ov5640_pre_flash_status = OV5640_PRE_FLASH_ON;
		}
		else {
			ov5640_pre_flash_status = OV5640_PRE_FLASH_OFF;
		}
		break;
	case STROBE_FLASH_MODE_OFF:
	default:
		break;
	}
done:
#endif
	return rc;
}

static int32_t ov5640_pre_flash_off(void)
{
	int32_t rc = 0;
	CDBG("+++\n");
#if defined(CONFIG_LEDS_LM3559) && defined(CONFIG_MSM_CAMERA_FLASH_LM3559)
	if (ov5640_pre_flash) {
		rc = lm3559_set_led(MSM_CAMERA_LED_OFF);
		ov5640_pre_flash = 0;
	}
#endif
	return rc;
}
/* END Dom_Lin@pegatron [2012/04/17] [Pre-flash for focusing] */

/* BEGIN Dom_Lin@pegatron [2012/04/26] [Add auto focus error handling and tuning work queue lock timing] */
static int32_t ov5640_pre_flash_abort(void)
{
	int32_t rc = 0;
	CDBG("+++\n");
#if defined(CONFIG_LEDS_LM3559) && defined(CONFIG_MSM_CAMERA_FLASH_LM3559)
	rc = lm3559_set_led(MSM_CAMERA_LED_OFF);
#if defined(OV5640_SUSPEND_CHANGE)
	ov5640_flash_delay = 0;
#endif
	ov5640_pre_flash_status = OV5640_PRE_FLASH_NONE;
	ov5640_pre_flash = 0;
#endif
	return rc;
}
/* END Dom_Lin@pegatron [2012/04/26] [Add auto focus error handling and tuning work queue lock timing] */

/* BEGIN Dom_Lin@pegatron [2012/04/27] [Release AF when setting focus mode Infinity] */
static int32_t ov5640_sensor_set_release_af(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;

#if defined(OV5640_SUSPEND_CHANGE)
	ov5640_reset_vcm = 0;
#endif

/* BEGIN Dom_Lin@pegatron [2012/04/26] [Add auto focus error handling and tuning work queue lock timing] */
	ov5640_af_stop_waiting = 1;
	mutex_lock(&ov5640_af_mutex);
	rc = ov5640_release_af(s_ctrl);
	if (rc < 0)
		CDBGE("Release AF failed\n");
	ov5640_af_stop_waiting = 0;
	mutex_unlock(&ov5640_af_mutex);
/* END Dom_Lin@pegatron [2012/04/26] [Add auto focus error handling and tuning work queue lock timing] */

	return rc;
}
/* END Dom_Lin@pegatron [2012/04/27] [Release AF when setting focus mode Infinity] */

static int32_t ov5640_wait_af(struct msm_sensor_ctrl_t *s_ctrl, uint8_t *status)
{
	int32_t rc = 0;
	uint16_t data = 0x0001;
	int32_t timeout = OV5640_AF_TIMEOUT;

	mutex_lock(&ov5640_af_mutex);

	*status = OV5640_FAILED;

#ifdef OV5640_AF_ACK
	while (timeout && !ov5640_af_stop_waiting) {
		rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, OV5640_CMD_ACK, &data, MSM_CAMERA_I2C_BYTE_DATA);
		if (rc < 0) {
			CDBGE("AF read CMD_ACK failed\n");
			goto done;
		}
		if (data == 0x0000)
			break;
		usleep_range(OV5640_AF_DELAY*1000, (OV5640_AF_DELAY+1)*1000);
		timeout = timeout - OV5640_AF_DELAY;
//		CDBG("AF read CMD_ACK again, timeout = %d\n", timeout);
	}
	if (data == 0x0001 || ov5640_af_stop_waiting) {
		CDBG("AF tiemout\n");
		goto done;
	}
	rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, OV5640_CMD_PARA4, &data, MSM_CAMERA_I2C_BYTE_DATA);
	if (rc < 0) {
		CDBGE("AF read CMD_PARA4 failed\n");
		goto done;
	}
#else
	while (timeout && !ov5640_af_stop_waiting) {
		rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, OV5640_CMD_PARA4, &data, MSM_CAMERA_I2C_BYTE_DATA);
		if (rc < 0) {
			CDBGE("AF read CMD_ACK failed\n");
			goto done;
		}
		if (data != 0x0000)
			break;
		usleep_range(OV5640_AF_DELAY*1000, (OV5640_AF_DELAY+1)*1000);
		timeout = timeout - OV5640_AF_DELAY;
//		CDBG("AF read CMD_PARA4 again, timeout = %d\n", timeout);
	}
#endif

	if (data == 0x0000 || ov5640_af_stop_waiting) {
		CDBG("AF failed\n");
	}
	else {
		CDBG("AF success\n");
		*status = OV5640_SUCCESS;
	}

done:
	if (*status == OV5640_FAILED) {
		// Disable MCU
		rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3004, 0xDF, MSM_CAMERA_I2C_BYTE_DATA);
		if (rc >= 0)
			ov5640_enable_MCU = 1;
	}

#if defined(OV5640_SUSPEND_CHANGE)
/* BEGIN Dom_Lin@pegatron [2012/04/12] [Reset VCM position after changing resolution] */
	rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, 0x3602, &data, MSM_CAMERA_I2C_WORD_DATA);
	if (rc >= 0) {
		ov5640_vcm_position = data;
		ov5640_reset_vcm = 1;
	}
/* END Dom_Lin@pegatron [2012/04/12] [Reset VCM position after changing resolution] */
#endif

/* BEGIN Dom_Lin@pegatron [2012/04/17] [Pre-flash for focusing] */
	rc = ov5640_pre_flash_off();
	if (rc < 0) {
		CDBGE("ov5640_pre_flash_off failed\n");
	}
/* END Dom_Lin@pegatron [2012/04/17] [Pre-flash for focusing] */

	if (ov5640_af_stop_waiting == 1) {
		*status = OV5640_ABORT;
		ov5640_af_stop_waiting = 0;
	}
	mutex_unlock(&ov5640_af_mutex);

	CDBG("---\n");
	return rc;
}

static int32_t ov5640_sensor_set_sensor_mode(struct msm_sensor_ctrl_t *s_ctrl, int mode, int res)
{
	int32_t rc = 0;
/* BEGIN Dom_Lin@pegatron [2012/02/02] [Enable auto flash function in 1024 codebase] */
	uint16_t gain = 0x0000;
/* END Dom_Lin@pegatron [2012/02/02] [Enable auto flash function in 1024 codebase] */
#if defined(OV5640_SUSPEND_CHANGE)
/* BEGIN Dom_Lin@pegatron [2012/04/17] [Add delay when taking picture in no light environment] */
	uint16_t avg = 0x0000;
/* END Dom_Lin@pegatron [2012/04/17] [Add delay when taking picture in no light environment] */
	int curr_res = s_ctrl->curr_res;
#endif
	CDBG("+++\n");

#if !defined(PEGA_SMT_BUILD)
	if (ov5640_reg_af == NULL && get_pega_hw_board_version_status() >= 2) { // Not DVT
		CDBG("enable AF regulator");
		ov5640_reg_af = regulator_get(&s_ctrl->sensor_i2c_client->client->dev, "cam_vaf_v2");
		if (IS_ERR(ov5640_reg_af)) {
			CDBGE("cam_vaf_v2 get failed\n");
			ov5640_reg_af = NULL;
			return -EFAULT;
		}
		rc = regulator_set_voltage(ov5640_reg_af, 2800000, 2800000);
		if (rc < 0) {
			CDBGE("cam_vaf_v2 set voltage failed\n");
			regulator_put(ov5640_reg_af);
			ov5640_reg_af = NULL;
		}
		else {
			rc = regulator_set_optimum_mode(ov5640_reg_af, 100000); // 100mA
			if (rc < 0) {
				CDBGE("cam_vaf_v2 set optimum mode failed\n");
				regulator_set_voltage(ov5640_reg_af, 0, 2800000);
				regulator_put(ov5640_reg_af);
				ov5640_reg_af = NULL;
			}
			else {
				rc = regulator_enable(ov5640_reg_af);
				if (rc < 0) {
					CDBGE("cam_vaf_v2 enable failed\n");
				}
			}
		}
	}
#endif

	rc = msm_sensor_set_sensor_mode(s_ctrl, mode, res);
	if (rc < 0) {
		CDBGE("set sensor mode failed: %d\n", mode);
		goto done;
	}
	ov5640_sensor_mode = mode;

#if defined(OV5640_SUSPEND_CHANGE)
/* BEGIN Dom_Lin@pegatron [2012/04/12] [Reset VCM position after changing resolution] */
	if (mode == SENSOR_MODE_SNAPSHOT && ov5640_reset_vcm == 1) {
		// if use suspend/resume when changing resolution, it needs to re-config VCM position to focused position
		msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3602, ov5640_vcm_position, MSM_CAMERA_I2C_WORD_DATA);
		ov5640_reset_vcm = 0;
	}
/* END Dom_Lin@pegatron [2012/04/12] [Reset VCM position after changing resolution] */
#endif

/* BEGIN Dom_Lin@pegatron [2012/02/06] [Add delay for capturing black image issue] */
#if defined(OV5640_SUSPEND_CHANGE)
	if (curr_res != res)
		msleep(600); // delay for capturing when the frames are not ready issue
#endif
/* END Dom_Lin@pegatron [2012/02/06] [Add delay for capturing black image issue] */

/* BEGIN Dom_Lin@pegatron [2012/02/02] [Enable auto flash function in 1024 codebase] */
	if (mode == SENSOR_MODE_SNAPSHOT) {
		switch(ov5640_flash_mode) {
/* BEGIN Dom_Lin@pegatron [2012/04/17] [Pre-flash for focusing] */
		case STROBE_FLASH_MODE_ON:
		case STROBE_FLASH_MODE_AUTO:
			if (ov5640_pre_flash_status == OV5640_PRE_FLASH_ON) {
#if defined(OV5640_SUSPEND_CHANGE)
				if (ov5640_flash_delay) {
					lm3559_set_duration(0x7F); // 1024 ms
				}
				else {
					lm3559_set_duration(0x6F); // 512 ms
				}
#endif
				rc = ov5640_strobe_high(s_ctrl);
#if defined(OV5640_SUSPEND_CHANGE)
				if (ov5640_flash_delay) {
					msleep(500);
					ov5640_flash_delay = 0;
				}
#endif
				ov5640_pre_flash_status = OV5640_PRE_FLASH_NONE;
			}
			else if (ov5640_pre_flash_status == OV5640_PRE_FLASH_OFF) {
#if defined(OV5640_SUSPEND_CHANGE)
				ov5640_flash_delay = 0;
#endif
				ov5640_pre_flash_status = OV5640_PRE_FLASH_NONE;
			}
			else {
				rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, 0x350b, &gain, MSM_CAMERA_I2C_BYTE_DATA);
				if (rc < 0) {
					CDBGE("read real gain failed\n");
					goto done;
				}
				CDBG("Real gain is %u\n", gain);

#if defined(OV5640_SUSPEND_CHANGE)
				if (gain > 0x007F) {
					rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, 0x56a1, &avg, MSM_CAMERA_I2C_BYTE_DATA);
					if (rc < 0) {
						CDBGE("read AVG READOUT failed\n");
						goto done;
					}
					CDBG("AVG READOUT is %u\n", avg);
				}

#if defined(CONFIG_LEDS_LM3559) && defined(CONFIG_MSM_CAMERA_FLASH_LM3559)
				if (gain >= 0x007F && avg <= 0x0004) {
					lm3559_set_duration(0x7F); // 1024 ms
				}
				else {
					lm3559_set_duration(0x6F); // 512 ms
				}
#endif
#endif // OV5640_SUSPEND_CHANGE

				if (ov5640_flash_mode == STROBE_FLASH_MODE_ON) {
					rc = ov5640_strobe_high(s_ctrl);
				}
				else if (gain > OV5640_AUTO_FLASH_GAIN) {
					rc = ov5640_strobe_high(s_ctrl);
				}

#if defined(OV5640_SUSPEND_CHANGE)
				if (gain >= 0x007F && avg <= 0x0004) {
					msleep(500);
				}
#endif
			}
/* END Dom_Lin@pegatron [2012/04/17] [Pre-flash for focusing] */
			break;
		case STROBE_FLASH_MODE_OFF:
		default:
			rc = ov5640_strobe_low(s_ctrl);
			break;
		}
	}
	else if (ov5640_flash_mode != STROBE_FLASH_MODE_OFF) {
		rc = ov5640_strobe_low(s_ctrl);
	}
/* END Dom_Lin@pegatron [2012/02/02] [Enable auto flash function in 1024 codebase] */
	if (rc < 0) {
		CDBGE("Strobe pin control failed\n");
	}

/* BEGIN Dom_Lin@pegatron [2012/03/28] [Release AF for focusing on infinite object after starting preview] */
	if (mode != SENSOR_MODE_SNAPSHOT) {
/* BEGIN Dom_Lin@pegatron [2012/04/27] [Release AF when setting focus mode Infinity] */
		ov5640_sensor_set_release_af(s_ctrl);
/* END Dom_Lin@pegatron [2012/04/27] [Release AF when setting focus mode Infinity] */
	}
/* END Dom_Lin@pegatron [2012/03/28] [Release AF for focusing on infinite object after starting preview] */

	if (mode != SENSOR_MODE_INVALID) {
		ov5640_sensor_set_sharpness(s_ctrl, ov5640_sharpness_value*5);
	}

done:
	return rc;
}
/* END Dom_Lin@pegatron [2012/02/02] [Init camera strobe flash function in 1024 codebase] */

#define OV5640_SELECT_REG(version) \
		OV5640_ASSIGN_ARRAY(ov5640_antibanding_map, version); \
		OV5640_ASSIGN_2D_ARRAY(ov5640_antibanding_settings, version); \
		OV5640_ASSIGN_ARRAY(ov5640_exposure_map, version); \
		OV5640_ASSIGN_2D_ARRAY(ov5640_exposure_settings, version); \
		OV5640_ASSIGN_ARRAY(ov5640_contrast_map, version); \
		OV5640_ASSIGN_2D_ARRAY(ov5640_contrast_settings, version); \
		OV5640_ASSIGN_ARRAY(ov5640_sharpness_map, version); \
		OV5640_ASSIGN_2D_ARRAY(ov5640_sharpness_settings, version); \
		OV5640_ASSIGN_ARRAY(ov5640_saturation_map, version); \
		OV5640_ASSIGN_2D_ARRAY(ov5640_saturation_settings, version); \
		OV5640_ASSIGN_ARRAY(ov5640_bestshot_mode_map, version); \
		OV5640_ASSIGN_2D_ARRAY(ov5640_bestshot_mode_settings, version); \
		OV5640_ASSIGN_ARRAY(ov5640_effect_map, version); \
		OV5640_ASSIGN_2D_ARRAY(ov5640_effect_settings, version); \
		OV5640_ASSIGN_ARRAY(ov5640_wb_map, version); \
		OV5640_ASSIGN_2D_ARRAY(ov5640_wb_settings, version); \
 \
		OV5640_ASSIGN_ARRAY(ov5640_recommend_settings, version)

static void ov5640_select_settings(uint32_t otp_id)
{
	switch(otp_id) {
	case 0x000AB534: // Chicony
		CDBG("Use Chicony IQ\n");
		OV5640_SELECT_REG(v2);
		break;
	case 0x00000000: // Sunny
	default:
		CDBG("Use Sunny IQ\n");
		OV5640_SELECT_REG(v1);
	}
	// change ov5640_recommend_settings in ov5640_init_conf
	ov5640_init_conf[1].conf = ov5640_recommend_settings;
	ov5640_init_conf[1].size = OV5640_ARRAY_SIZE(ov5640_recommend_settings);
}

static int32_t ov5640_sensor_power_up(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t final_rc = 0;
	int32_t rc = 0;
	uint32_t otp_id = 0x00000000;

	final_rc = msm_sensor_power_up(s_ctrl);
	if (final_rc < 0) {
		CDBGE("msm_sensor_power_up failed\n");
	}
	else {
		if (ov5640_read_otp) {
			// Read OTP data
			rc = ov5640_otp_read(s_ctrl->sensor_i2c_client, &otp_id);
			if (rc < 0) {
				CDBGE("OTP Data read failed\n");
			}
			else {
				CDBG("OTP ID is 0x%08X\n", otp_id);
				ov5640_select_settings(otp_id);
			}
			ov5640_read_otp = 0;
		}
	}

	return final_rc;
}

static int32_t ov5640_sensor_power_down(struct msm_sensor_ctrl_t *s_ctrl)
{
	mutex_lock(&ov5640_work_mutex);
	ov5640_streaming = 0;
	ov5640_af_stop_waiting = 1;
	flush_workqueue(ov5640_wq);
	mutex_unlock(&ov5640_work_mutex);


#if !defined(PEGA_SMT_BUILD)
	if (ov5640_reg_af) {
		CDBG("Disable AF regulator\n");
		regulator_disable(ov5640_reg_af);
		regulator_set_optimum_mode(ov5640_reg_af, 0);
		regulator_set_voltage(ov5640_reg_af, 0, 2800000);
		regulator_put(ov5640_reg_af);
		ov5640_reg_af = NULL;
	}
#endif

/* BEGIN Dom_Lin@pegatron [2012/04/17] [Pre-flash for focusing] */
	ov5640_pre_flash_off();
/* END Dom_Lin@pegatron [2012/04/17] [Pre-flash for focusing] */

	ov5640_strobe = 0;

	// reset to default value
	ov5640_sharpness_value = 6;
	ov5640_sensor_mode = SENSOR_MODE_INVALID;
	ov5640_effect_value = CAMERA_EFFECT_OFF;
	ov5640_saturation_value = 6;
	ov5640_next_saturation_value = 6;

	return msm_sensor_power_down(s_ctrl);
}

/* BEGIN Dom_Lin@pegatron [2012/02/04] [Enable back camera auto focus] */
static int32_t ov5640_sensor_set_auto_focus(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	struct ov5640_work *new_work;
	CDBG("+++\n");

/* BEGIN Dom_Lin@pegatron [2012/04/17] [Pre-flash for focusing] */
	rc = ov5640_pre_flash_on(s_ctrl);
	if (rc < 0) {
		CDBGE("ov5640_pre_flash_on failed\n");
		goto done;
	}
/* END Dom_Lin@pegatron [2012/04/17] [Pre-flash for focusing] */

	rc = ov5640_trig_af(s_ctrl);
	if (rc < 0) {
		CDBGE("ov5640_trig_af failed\n");
		goto done;
	}

done:
	// wait auto focus in work queue
	new_work = kzalloc(sizeof(*new_work), GFP_ATOMIC);
	if (!new_work) {
		CDBGE("kzalloc failed\n");
		return -ENOMEM;
	}

/* BEGIN Dom_Lin@pegatron [2012/04/26] [Add auto focus error handling and tuning work queue lock timing] */
	INIT_DELAYED_WORK(&new_work->m_work, ov5640_worker);
	if (rc < 0) {
		new_work->cmd = OV5640_CMD_AF_ABORT;
	}
	else {
		new_work->cmd = OV5640_CMD_AF;
	}
	new_work->s_ctrl = s_ctrl;

	mutex_lock(&ov5640_work_mutex);
	CDBG("Flush old works\n");
	flush_workqueue(ov5640_wq);
	CDBG("queue_delayed_work\n");
	if (!queue_delayed_work(ov5640_wq, &new_work->m_work, msecs_to_jiffies(0))) {
		mutex_unlock(&ov5640_work_mutex);
		CDBGE("queue_delayed_work failed\n");
		kfree(new_work);
		return -ENOMEM;
	}
	mutex_unlock(&ov5640_work_mutex);
/* END Dom_Lin@pegatron [2012/04/26] [Add auto focus error handling and tuning work queue lock timing] */

	return rc;
}

static void ov5640_worker(struct work_struct *work)
{
	struct delayed_work *delayed = container_of(work, struct delayed_work, work);
	struct ov5640_work *new_work = container_of(delayed, struct ov5640_work, m_work);

	uint8_t status = OV5640_FAILED;

	CDBG("+++ new_work->cmd = %d\n", new_work->cmd);

/* BEGIN Dom_Lin@pegatron [2012/04/26] [Add auto focus error handling and tuning work queue lock timing] */
	if (!ov5640_streaming) {
		kfree(new_work);
		return;
	}
/* END Dom_Lin@pegatron [2012/04/26] [Add auto focus error handling and tuning work queue lock timing] */

	switch(new_work->cmd) {
	case OV5640_CMD_AF:
		ov5640_wait_af(new_work->s_ctrl, &status);
		msm_sensor_send_stats_msg(new_work->s_ctrl, MSG_ID_STATS_AF, status);
		break;
/* BEGIN Dom_Lin@pegatron [2012/04/26] [Add auto focus error handling and tuning work queue lock timing] */
	case OV5640_CMD_TOUCH_AF:
		ov5640_wait_af(new_work->s_ctrl, &status);
		if (status != OV5640_ABORT) {
			msm_sensor_send_stats_msg(new_work->s_ctrl, MSG_ID_STATS_AF, status);
		}
		break;
	case OV5640_CMD_AF_ABORT:
		ov5640_pre_flash_abort();
		msm_sensor_send_stats_msg(new_work->s_ctrl, MSG_ID_STATS_AF, OV5640_FAILED);
		break;
/* END Dom_Lin@pegatron [2012/04/26] [Add auto focus error handling and tuning work queue lock timing] */
	default:
		break;
	}
	CDBG("--- new_work->cmd = %d\n", new_work->cmd);
	kfree(new_work);
}
/* END Dom_Lin@pegatron [2012/02/04] [Enable back camera auto focus] */

static void ov5640_filter_conf(struct msm_camera_i2c_reg_conf* conf, unsigned long x, unsigned long y) {
	int i, j;
	for (i = 0; i < x; ++i) {
		for (j = y-1; j >= 0; --j) {
			if (!conf[i*y+j].reg_addr && !conf[i*y+j].reg_data && !conf[i*y+j].dt && !conf[i*y+j].cmd_type && !conf[i*y+j].reg_mask)
				conf[i*y+j].cmd_type = MSM_CAMERA_I2C_CMD_NULL;
			else
				break;
		}
	}
}

/* BEGIN Dom_Lin@pegatron [2012/02/06] [Enable back camera white balance] */
static int32_t ov5640_sensor_set_wb(struct msm_sensor_ctrl_t *s_ctrl, uint8_t val)
{
	CDBG("+++\n");
	if (val >= OV5640_ARRAY_SIZE(ov5640_wb_map)) {
		CDBGE("Not supported: %u\n", val);
		return 0;
	}
	CDBG("write tbl ov5640_wb_settings[%u]=%u\n", val, ov5640_wb_map[val]);

	msm_sensor_group_hold_on(s_ctrl);
	msm_camera_i2c_write_tbl(
		s_ctrl->sensor_i2c_client,
		ov5640_wb_settings[ov5640_wb_map[val]],
		OV5640_2D_ARRAY_SIZE(ov5640_wb_settings),
		MSM_CAMERA_I2C_BYTE_DATA_AUTO);
	msm_sensor_group_hold_off(s_ctrl);

	return 0;
}
/* END Dom_Lin@pegatron [2012/02/06] [Enable back camera white balance] */

/* BEGIN Dom_Lin@pegatron [2012/02/11] [Enable back camera effect] */
static int32_t ov5640_sensor_set_effect(struct msm_sensor_ctrl_t *s_ctrl, uint8_t val)
{
	CDBG("+++\n");
	if (val >= OV5640_ARRAY_SIZE(ov5640_effect_map)) {
		CDBGE("Not supported: %u\n", val);
		return 0;
	}
	ov5640_effect_value = val;
	CDBG("write tbl ov5640_effect_settings[%u]=%u\n", val, ov5640_effect_map[val]);

	msm_sensor_group_hold_on(s_ctrl);
	msm_camera_i2c_write_tbl(
		s_ctrl->sensor_i2c_client,
		ov5640_effect_settings[ov5640_effect_map[val]],
		OV5640_2D_ARRAY_SIZE(ov5640_effect_settings),
		MSM_CAMERA_I2C_BYTE_DATA_AUTO);
	msm_sensor_group_hold_off(s_ctrl);

	// Exception handling
	if (ov5640_effect_value == CAMERA_EFFECT_OFF && ov5640_saturation_value != ov5640_next_saturation_value) {
		ov5640_sensor_set_saturation(s_ctrl, ov5640_next_saturation_value);
	}
	else if (ov5640_effect_value != CAMERA_EFFECT_OFF) {
		ov5640_saturation_value = 6; // default value
	}

	return 0;
}
/* END Dom_Lin@pegatron [2012/02/11] [Enable back camera effect] */

/* BEGIN Dom_Lin@pegatron [2012/02/12] [Enable back camera scene mode] */
static int32_t ov5640_sensor_set_bestshot_mode(struct msm_sensor_ctrl_t *s_ctrl, uint8_t val)
{
	CDBG("+++\n");
	if (val >= OV5640_ARRAY_SIZE(ov5640_bestshot_mode_map)) {
		CDBGE("Not supported: %u\n", val);
		return 0;
	}
	CDBG("write tbl ov5640_bestshot_mode_settings[%u]=%u\n", val, ov5640_bestshot_mode_map[val]);

	msm_sensor_group_hold_on(s_ctrl);
	msm_camera_i2c_write_tbl(
		s_ctrl->sensor_i2c_client,
		ov5640_bestshot_mode_settings[ov5640_bestshot_mode_map[val]],
		OV5640_2D_ARRAY_SIZE(ov5640_bestshot_mode_settings),
		MSM_CAMERA_I2C_BYTE_DATA_AUTO);
	msm_sensor_group_hold_off(s_ctrl);

	return 0;
}
/* END Dom_Lin@pegatron [2012/02/12] [Enable back camera scene mode] */

/* BEGIN Dom_Lin@pegatron [2012/02/18] [Enable back camera saturation] */
static int32_t ov5640_sensor_set_saturation(struct msm_sensor_ctrl_t *s_ctrl, uint8_t val)
{
	CDBG("+++\n");
	if (val >= OV5640_ARRAY_SIZE(ov5640_saturation_map)) {
		CDBGE("Not supported: %u\n", val);
		return 0;
	}
	// Exception handling
	ov5640_next_saturation_value = val;
	if (ov5640_effect_value != CAMERA_EFFECT_OFF) {
		CDBGE("Can not set staturation while effect is not off\n");
		return 0;
	}
	ov5640_saturation_value = val;
	CDBG("write tbl ov5640_saturation_settings[%u]=%u\n", val, ov5640_saturation_map[val]);

	msm_sensor_group_hold_on(s_ctrl);
	msm_camera_i2c_write_tbl(
		s_ctrl->sensor_i2c_client,
		ov5640_saturation_settings[ov5640_saturation_map[val]],
		OV5640_2D_ARRAY_SIZE(ov5640_saturation_settings),
		MSM_CAMERA_I2C_BYTE_DATA_AUTO);
	msm_sensor_group_hold_off(s_ctrl);

	return 0;
}
/* END Dom_Lin@pegatron [2012/02/18] [Enable back camera saturation] */

/* BEGIN Dom_Lin@pegatron [2012/02/18] [Enable back camera sharpness] */
static int32_t ov5640_sensor_set_sharpness(struct msm_sensor_ctrl_t *s_ctrl, uint8_t val)
{
	CDBG("+++\n");
	val = val / 5;
	if (val >= OV5640_ARRAY_SIZE(ov5640_sharpness_map)) {
		CDBGE("Not supported: %u\n", val);
		return 0;
	}
	if (ov5640_sensor_mode != SENSOR_MODE_SNAPSHOT) {
		ov5640_sharpness_value = val;
		if (val != OV5640_ARRAY_SIZE(ov5640_sharpness_map) - 1)
			val = val + 1;
	}
	CDBG("write tbl ov5640_sharpness_settings[%u]=%u\n", val, ov5640_sharpness_map[val]);

	msm_sensor_group_hold_on(s_ctrl);
	msm_camera_i2c_write_tbl(
		s_ctrl->sensor_i2c_client,
		ov5640_sharpness_settings[ov5640_sharpness_map[val]],
		OV5640_2D_ARRAY_SIZE(ov5640_sharpness_settings),
		MSM_CAMERA_I2C_BYTE_DATA_AUTO);
	msm_sensor_group_hold_off(s_ctrl);

	return 0;
}
/* END Dom_Lin@pegatron [2012/02/18] [Enable back camera sharpness] */

/* BEGIN Dom_Lin@pegatron [2012/02/18] [Enable back camera contrast] */
static int32_t ov5640_sensor_set_contrast(struct msm_sensor_ctrl_t *s_ctrl, uint8_t val)
{
	CDBG("+++\n");
	if (val >= OV5640_ARRAY_SIZE(ov5640_contrast_map)) {
		CDBGE("Not supported: %u\n", val);
		return 0;
	}
	CDBG("write tbl ov5640_contrast_settings[%u]=%u\n", val, ov5640_contrast_map[val]);

	msm_sensor_group_hold_on(s_ctrl);
	msm_camera_i2c_write_tbl(
		s_ctrl->sensor_i2c_client,
		ov5640_contrast_settings[ov5640_contrast_map[val]],
		OV5640_2D_ARRAY_SIZE(ov5640_contrast_settings),
		MSM_CAMERA_I2C_BYTE_DATA_AUTO);
	msm_sensor_group_hold_off(s_ctrl);

	return 0;
}
/* END Dom_Lin@pegatron [2012/02/18] [Enable back camera contrast] */

/* BEGIN Dom_Lin@pegatron [2012/02/18] [Enable back camera exposure compensation] */
static int32_t ov5640_sensor_set_exposure(struct msm_sensor_ctrl_t *s_ctrl, uint32_t exp)
{
	int16_t numerator16 = (exp & 0xffff0000) >> 16;
	uint8_t val = (uint8_t)(numerator16 + 6);
	CDBG("+++\n");
	if (numerator16 < -6 || val >= OV5640_ARRAY_SIZE(ov5640_exposure_map)) {
		CDBGE("Not supported: %d\n", numerator16);
		return 0;
	}
	CDBG("write tbl ov5640_exposure_settings[%u]=%u\n", val, ov5640_exposure_map[val]);

	msm_sensor_group_hold_on(s_ctrl);
	msm_camera_i2c_write_tbl(
		s_ctrl->sensor_i2c_client,
		ov5640_exposure_settings[ov5640_exposure_map[val]],
		OV5640_2D_ARRAY_SIZE(ov5640_exposure_settings),
		MSM_CAMERA_I2C_BYTE_DATA_AUTO);
	msm_sensor_group_hold_off(s_ctrl);

	return 0;
}
/* END Dom_Lin@pegatron [2012/02/18] [Enable back camera exposure compensation] */

/* BEGIN Dom_Lin@pegatron [2012/02/19] [Enable back camera anti-banding] */
static int32_t ov5640_sensor_set_antibanding(struct msm_sensor_ctrl_t *s_ctrl, uint8_t val)
{
	CDBG("+++\n");
	if (val >= OV5640_ARRAY_SIZE(ov5640_antibanding_map)) {
		CDBGE("Not supported: %u\n", val);
		return 0;
	}
	CDBG("write tbl ov5640_antibanding_settings[%u]=%u\n", val, ov5640_antibanding_map[val]);

	msm_sensor_group_hold_on(s_ctrl);
	msm_camera_i2c_write_tbl(
		s_ctrl->sensor_i2c_client,
		ov5640_antibanding_settings[ov5640_antibanding_map[val]],
		OV5640_2D_ARRAY_SIZE(ov5640_antibanding_settings),
		MSM_CAMERA_I2C_BYTE_DATA_AUTO);
	msm_sensor_group_hold_off(s_ctrl);

	return 0;
}
/* END Dom_Lin@pegatron [2012/02/19] [Enable back camera anti-banding] */

/* BEGIN Dom_Lin@pegatron [2012/02/20] [Enable back camera touch focus] */
static int32_t ov5640_sensor_set_touch_focus(struct msm_sensor_ctrl_t *s_ctrl, struct sensor_roi_t roi)
{
	int32_t rc = 0;
	struct ov5640_work *new_work;
	CDBG("+++\n");

/* BEGIN Dom_Lin@pegatron [2012/04/17] [Pre-flash for focusing] */
	rc = ov5640_pre_flash_on(s_ctrl);
	if (rc < 0) {
		CDBGE("ov5640_pre_flash_on failed\n");
		goto done;
	}
/* END Dom_Lin@pegatron [2012/04/17] [Pre-flash for focusing] */

/* BEGIN Dom_Lin@pegatron [2012/04/18] [Update AF firmware to version OV5640-AD5820-ASUS-13011000-20120416140547] */
#ifdef OV5640_AF_CUSTOM_FOCUS_ZONE
	rc = ov5640_launch_custom_focus_zone(s_ctrl, roi);
	if (rc < 0) {
		CDBGE("ov5640_launch_custom_focus_zone failed\n");
		goto done;
	}
#else
	rc = ov5640_launch_touch_focus_zone(s_ctrl, roi);
	if (rc < 0) {
		CDBGE("ov5640_launch_touch_focus_zone failed\n");
		goto done;
	}
#endif
/* END Dom_Lin@pegatron [2012/04/18] [Update AF firmware to version OV5640-AD5820-ASUS-13011000-20120416140547] */

done:
	// wait auto focus in work queue
	new_work = kzalloc(sizeof(*new_work), GFP_ATOMIC);
	if (!new_work) {
		CDBGE("kzalloc failed\n");
		return -ENOMEM;
	}

/* BEGIN Dom_Lin@pegatron [2012/04/26] [Add auto focus error handling and tuning work queue lock timing] */
	INIT_DELAYED_WORK(&new_work->m_work, ov5640_worker);
	if (rc < 0) {
		new_work->cmd = OV5640_CMD_AF_ABORT;
	}
	else {
		new_work->cmd = OV5640_CMD_TOUCH_AF;
	}
	new_work->s_ctrl = s_ctrl;

	mutex_lock(&ov5640_work_mutex);
	CDBG("Flush old works\n");
	flush_workqueue(ov5640_wq);
	CDBG("queue_delayed_work\n");
	if (!queue_delayed_work(ov5640_wq, &new_work->m_work, msecs_to_jiffies(0))) {
		mutex_unlock(&ov5640_work_mutex);
		CDBGE("queue_delayed_work failed\n");
		kfree(new_work);
		return -ENOMEM;
	}
	mutex_unlock(&ov5640_work_mutex);
/* END Dom_Lin@pegatron [2012/04/26] [Add auto focus error handling and tuning work queue lock timing] */

	return rc;
}
/* END Dom_Lin@pegatron [2012/02/20] [Enable back camera touch focus] */

void ov5640_start_stream(struct msm_sensor_ctrl_t *s_ctrl)
{
	CDBG("+++\n");
#if defined(OV5640_SUSPEND_CHANGE)
	msm_sensor_start_stream(s_ctrl);
#else
	if (!ov5640_streaming) {
		msm_sensor_start_stream(s_ctrl);
	}
	else {
		msm_camera_i2c_write_tbl(
			s_ctrl->sensor_i2c_client,
			ov5640_start_frame_settings,
			ARRAY_SIZE(ov5640_start_frame_settings),
			MSM_CAMERA_I2C_BYTE_DATA_AUTO);
	}
#endif
	if (!ov5640_streaming) {
		ov5640_streaming = 1;
	}
}

void ov5640_stop_stream(struct msm_sensor_ctrl_t *s_ctrl)
{
	CDBG("+++\n");
#if defined(OV5640_SUSPEND_CHANGE)
	msm_sensor_stop_stream(s_ctrl);
#else
	if (!ov5640_streaming) {
		msm_sensor_stop_stream(s_ctrl);
	}
	else {
		msm_camera_i2c_write_tbl(
			s_ctrl->sensor_i2c_client,
			ov5640_stop_frame_settings,
			ARRAY_SIZE(ov5640_stop_frame_settings),
			MSM_CAMERA_I2C_BYTE_DATA_AUTO);
	}
#endif
}

static const struct i2c_device_id ov5640_i2c_id[] = {
	{SENSOR_NAME, (kernel_ulong_t)&ov5640_s_ctrl},
	{ }
};

static struct i2c_driver ov5640_i2c_driver = {
	.id_table = ov5640_i2c_id,
	.probe  = msm_sensor_i2c_probe,
	.driver = {
		.name = SENSOR_NAME,
	},
};

static struct msm_camera_i2c_client ov5640_sensor_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_WORD_ADDR,
};

static int __init msm_sensor_init_module(void)
{
	CDBG("+++\n");

	ov5640_wq = create_singlethread_workqueue("ov5640");
	if (!ov5640_wq) {
		CDBGE("create_singlethread_workqueue failed\n");
		destroy_workqueue(ov5640_wq);
		ov5640_wq = NULL;
		return -ENOMEM;
	}

	ov5640_filter_conf((struct msm_camera_i2c_reg_conf*)ov5640_wb_settings,
		OV5640_ARRAY_SIZE(ov5640_wb_settings), OV5640_2D_ARRAY_SIZE(ov5640_wb_settings));
	ov5640_filter_conf((struct msm_camera_i2c_reg_conf*)ov5640_effect_settings,
		OV5640_ARRAY_SIZE(ov5640_effect_settings), OV5640_2D_ARRAY_SIZE(ov5640_effect_settings));
	ov5640_filter_conf((struct msm_camera_i2c_reg_conf*)ov5640_bestshot_mode_settings,
		OV5640_ARRAY_SIZE(ov5640_bestshot_mode_settings), OV5640_2D_ARRAY_SIZE(ov5640_bestshot_mode_settings));
/* BEGIN Dom_Lin@pegatron [2012/02/18] [Enable back camera saturation] */
	ov5640_filter_conf((struct msm_camera_i2c_reg_conf*)ov5640_saturation_settings,
		OV5640_ARRAY_SIZE(ov5640_saturation_settings), OV5640_2D_ARRAY_SIZE(ov5640_saturation_settings));
/* END Dom_Lin@pegatron [2012/02/18] [Enable back camera saturation] */
/* BEGIN Dom_Lin@pegatron [2012/02/18] [Enable back camera sharpness] */
	ov5640_filter_conf((struct msm_camera_i2c_reg_conf*)ov5640_sharpness_settings,
		OV5640_ARRAY_SIZE(ov5640_sharpness_settings), OV5640_2D_ARRAY_SIZE(ov5640_sharpness_settings));
/* END Dom_Lin@pegatron [2012/02/18] [Enable back camera sharpness] */
/* BEGIN Dom_Lin@pegatron [2012/02/18] [Enable back camera contrast] */
	ov5640_filter_conf((struct msm_camera_i2c_reg_conf*)ov5640_contrast_settings,
		OV5640_ARRAY_SIZE(ov5640_contrast_settings), OV5640_2D_ARRAY_SIZE(ov5640_contrast_settings));
/* END Dom_Lin@pegatron [2012/02/18] [Enable back camera contrast] */
/* BEGIN Dom_Lin@pegatron [2012/02/18] [Enable back camera exposure compensation] */
	ov5640_filter_conf((struct msm_camera_i2c_reg_conf*)ov5640_exposure_settings,
		OV5640_ARRAY_SIZE(ov5640_exposure_settings), OV5640_2D_ARRAY_SIZE(ov5640_exposure_settings));
/* END Dom_Lin@pegatron [2012/02/18] [Enable back camera exposure compensation] */
/* Dom_Lin@pegatron [2012/02/19] [Enable back camera anti-banding] */
	ov5640_filter_conf((struct msm_camera_i2c_reg_conf*)ov5640_antibanding_settings,
		OV5640_ARRAY_SIZE(ov5640_antibanding_settings), OV5640_2D_ARRAY_SIZE(ov5640_antibanding_settings));
/* Dom_Lin@pegatron [2012/02/19] [Enable back camera anti-banding] */

	return i2c_add_driver(&ov5640_i2c_driver);
}

static void __exit msm_sensor_exit_module(void)
{
	CDBG("+++\n");

/* BEGIN Dom_Lin@pegatron [2012/04/26] [Add auto focus error handling and tuning work queue lock timing] */
	mutex_lock(&ov5640_work_mutex);
	CDBG("destory work queue\n");
	flush_workqueue(ov5640_wq);
	destroy_workqueue(ov5640_wq);
	ov5640_wq = NULL;
	mutex_unlock(&ov5640_work_mutex);
/* END Dom_Lin@pegatron [2012/04/26] [Add auto focus error handling and tuning work queue lock timing] */

	i2c_del_driver(&ov5640_i2c_driver);
}

static struct v4l2_subdev_core_ops ov5640_subdev_core_ops = {
	.s_ctrl = msm_sensor_v4l2_s_ctrl,
	.queryctrl = msm_sensor_v4l2_query_ctrl,
	.ioctl = msm_sensor_subdev_ioctl,
	.s_power = msm_sensor_power,
};

static struct v4l2_subdev_video_ops ov5640_subdev_video_ops = {
	.enum_mbus_fmt = msm_sensor_v4l2_enum_fmt,
};

static struct v4l2_subdev_ops ov5640_subdev_ops = {
	.core = &ov5640_subdev_core_ops,
	.video  = &ov5640_subdev_video_ops,
};

static struct msm_sensor_fn_t ov5640_func_tbl = {
	.sensor_start_stream = ov5640_start_stream,
	.sensor_stop_stream = ov5640_stop_stream,
	.sensor_group_hold_on = msm_sensor_group_hold_on,
	.sensor_group_hold_off = msm_sensor_group_hold_off,
	.sensor_setting = msm_sensor_setting,
/* BEGIN Dom_Lin@pegatron [2012/02/02] [Init camera strobe flash function in 1024 codebase] */
	.sensor_set_sensor_mode = ov5640_sensor_set_sensor_mode,
/* END Dom_Lin@pegatron [2012/02/02] [Init camera strobe flash function in 1024 codebase] */
	.sensor_mode_init = msm_sensor_mode_init,
	.sensor_get_output_info = msm_sensor_get_output_info,
	.sensor_config = msm_sensor_config,
	.sensor_power_up = ov5640_sensor_power_up,
	.sensor_power_down = ov5640_sensor_power_down,
/* BEGIN Dom_Lin@pegatron [2012/02/04] [Enable back camera auto focus] */
	.sensor_set_auto_focus = ov5640_sensor_set_auto_focus,
/* END Dom_Lin@pegatron [2012/02/04] [Enable back camera auto focus] */
/* BEGIN Dom_Lin@pegatron [2012/02/06] [Enable back camera white balance] */
	.sensor_set_wb = ov5640_sensor_set_wb,
/* END Dom_Lin@pegatron [2012/02/06] [Enable back camera white balance] */
/* BEGIN Dom_Lin@pegatron [2012/02/11] [Enable back camera effect] */
	.sensor_set_effect = ov5640_sensor_set_effect,
/* END Dom_Lin@pegatron [2012/02/11] [Enable back camera effect] */
/* BEGIN Dom_Lin@pegatron [2012/02/12] [Enable back camera scene mode] */
	.sensor_set_bestshot_mode = ov5640_sensor_set_bestshot_mode,
/* END Dom_Lin@pegatron [2012/02/12] [Enable back camera scene mode] */
/* BEGIN Dom_Lin@pegatron [2012/02/18] [Enable back camera saturation] */
	.sensor_set_saturation = ov5640_sensor_set_saturation,
/* END Dom_Lin@pegatron [2012/02/18] [Enable back camera saturation] */
/* BEGIN Dom_Lin@pegatron [2012/02/18] [Enable back camera sharpness] */
	.sensor_set_sharpness = ov5640_sensor_set_sharpness,
/* END Dom_Lin@pegatron [2012/02/18] [Enable back camera sharpness] */
/* BEGIN Dom_Lin@pegatron [2012/02/18] [Enable back camera contrast] */
	.sensor_set_contrast = ov5640_sensor_set_contrast,
/* END Dom_Lin@pegatron [2012/02/18] [Enable back camera contrast] */
/* BEGIN Dom_Lin@pegatron [2012/02/18] [Enable back camera exposure compensation] */
	.sensor_set_exp_compensation = ov5640_sensor_set_exposure,
/* END Dom_Lin@pegatron [2012/02/18] [Enable back camera exposure compensation] */
/* BEGIN Dom_Lin@pegatron [2012/02/19] [Enable back camera anti-banding] */
	.sensor_set_antibanding = ov5640_sensor_set_antibanding,
/* END Dom_Lin@pegatron [2012/02/19] [Enable back camera anti-banding] */
/* BEGIN Dom_Lin@pegatron [2012/02/20] [Enable back camera touch focus] */
	.sensor_set_touch_focus = ov5640_sensor_set_touch_focus,
/* END Dom_Lin@pegatron [2012/02/20] [Enable back camera touch focus] */
/* BEGIN Dom_Lin@pegatron [2012/04/27] [Release AF when setting focus mode Infinity] */
	.sensor_set_release_af = ov5640_sensor_set_release_af,
/* END Dom_Lin@pegatron [2012/04/27] [Release AF when setting focus mode Infinity] */
};

static struct msm_sensor_reg_t ov5640_regs = {
	.default_data_type = MSM_CAMERA_I2C_BYTE_DATA_AUTO,
	.start_stream_conf = ov5640_start_stream_settings,
	.start_stream_conf_size = ARRAY_SIZE(ov5640_start_stream_settings),
	.stop_stream_conf = ov5640_stop_stream_settings,
	.stop_stream_conf_size = ARRAY_SIZE(ov5640_stop_stream_settings),
	.group_hold_on_conf = ov5640_groupon_settings,
	.group_hold_on_conf_size = ARRAY_SIZE(ov5640_groupon_settings),
	.group_hold_off_conf = ov5640_groupoff_settings,
	.group_hold_off_conf_size = ARRAY_SIZE(ov5640_groupoff_settings),
	.init_settings = &ov5640_init_conf[0],
	.init_size = ARRAY_SIZE(ov5640_init_conf),
	.mode_settings = &ov5640_confs[0],
	.output_settings = &ov5640_dimensions[0],
	.num_conf = ARRAY_SIZE(ov5640_confs),
};

static struct msm_sensor_ctrl_t ov5640_s_ctrl = {
	.msm_sensor_reg = &ov5640_regs,
	.sensor_i2c_client = &ov5640_sensor_i2c_client,
	.sensor_i2c_addr = 0x78,
	.sensor_output_reg_addr = &ov5640_reg_addr,
	.sensor_id_info = &ov5640_id_info,
	.cam_mode = MSM_SENSOR_MODE_INVALID,
	.csi_params = &ov5640_csi_params_array[0],
	.msm_sensor_mutex = &ov5640_mut,
	.sensor_i2c_driver = &ov5640_i2c_driver,
	.sensor_v4l2_subdev_info = ov5640_subdev_info,
	.sensor_v4l2_subdev_info_size = ARRAY_SIZE(ov5640_subdev_info),
	.sensor_v4l2_subdev_ops = &ov5640_subdev_ops,
	.func_tbl = &ov5640_func_tbl,
	.clk_rate = MSM_SENSOR_MCLK_24HZ,
};

module_init(msm_sensor_init_module);
module_exit(msm_sensor_exit_module);
MODULE_DESCRIPTION("OmniVision 5MP YUV sensor driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Dom Lin <dom_lin@pegatroncorp.com>");
