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
 */

#include "msm_sensor.h"
#include "msm.h"
#include "msm_ispif.h"

#undef CDBG
#define CDBG(fmt, args...) printk(KERN_DEBUG "[%s: %d] " fmt, __func__, __LINE__, ##args)

/*=============================================================*/

int32_t msm_sensor_write_init_settings(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc;
	CDBG("+++\n");
	if(s_ctrl->sensor_i2c_client->client->addr == s_ctrl->sensor_i2c_addr_high){
	CDBG("mt9m114_2nd source init\n");
	rc = msm_sensor_write_all_conf_array(
		s_ctrl->sensor_i2c_client,
		s_ctrl->msm_sensor_reg->init_settings_2,
		s_ctrl->msm_sensor_reg->init_size_2);
	}
	else{
	rc = msm_sensor_write_all_conf_array(
		s_ctrl->sensor_i2c_client,
		s_ctrl->msm_sensor_reg->init_settings,
		s_ctrl->msm_sensor_reg->init_size);
	}
	return rc;
}

int32_t msm_sensor_write_res_settings(struct msm_sensor_ctrl_t *s_ctrl,
	uint16_t res)
{
	int32_t rc;
	CDBG("+++\n");
	rc = msm_sensor_write_conf_array(
		s_ctrl->sensor_i2c_client,
		s_ctrl->msm_sensor_reg->mode_settings, res);
	if (rc < 0)
		return rc;

/* BEGIN Dom_Lin@pegatron [2012/05/04] [Reduce the time of opening back camera] */
	if (s_ctrl->sensordata->sensor_name == NULL || !(0 == strcmp(s_ctrl->sensordata->sensor_name, "ov5640"))) {
		rc = msm_sensor_write_output_settings(s_ctrl, res);
	}
/* END Dom_Lin@pegatron [2012/05/04] [Reduce the time of opening back camera] */
	return rc;
}

int32_t msm_sensor_write_output_settings(struct msm_sensor_ctrl_t *s_ctrl,
	uint16_t res)
{
	int32_t rc = -EFAULT;
	struct msm_camera_i2c_reg_conf dim_settings[] = {
		{s_ctrl->sensor_output_reg_addr->x_output,
			s_ctrl->msm_sensor_reg->
			output_settings[res].x_output},
		{s_ctrl->sensor_output_reg_addr->y_output,
			s_ctrl->msm_sensor_reg->
			output_settings[res].y_output},
		{s_ctrl->sensor_output_reg_addr->line_length_pclk,
			s_ctrl->msm_sensor_reg->
			output_settings[res].line_length_pclk},
		{s_ctrl->sensor_output_reg_addr->frame_length_lines,
			s_ctrl->msm_sensor_reg->
			output_settings[res].frame_length_lines},
	};
	CDBG("+++\n");

	rc = msm_camera_i2c_write_tbl(s_ctrl->sensor_i2c_client, dim_settings,
		ARRAY_SIZE(dim_settings), MSM_CAMERA_I2C_WORD_DATA);
	return rc;
}

void msm_sensor_start_stream(struct msm_sensor_ctrl_t *s_ctrl)
{
	CDBG("+++\n");
	msm_camera_i2c_write_tbl(
		s_ctrl->sensor_i2c_client,
		s_ctrl->msm_sensor_reg->start_stream_conf,
		s_ctrl->msm_sensor_reg->start_stream_conf_size,
		s_ctrl->msm_sensor_reg->default_data_type);
}

void msm_sensor_stop_stream(struct msm_sensor_ctrl_t *s_ctrl)
{
	CDBG("+++\n");
	msm_camera_i2c_write_tbl(
		s_ctrl->sensor_i2c_client,
		s_ctrl->msm_sensor_reg->stop_stream_conf,
		s_ctrl->msm_sensor_reg->stop_stream_conf_size,
		s_ctrl->msm_sensor_reg->default_data_type);
}

void msm_sensor_group_hold_on(struct msm_sensor_ctrl_t *s_ctrl)
{
	CDBG("+++\n");
	msm_camera_i2c_write_tbl(
		s_ctrl->sensor_i2c_client,
		s_ctrl->msm_sensor_reg->group_hold_on_conf,
		s_ctrl->msm_sensor_reg->group_hold_on_conf_size,
		s_ctrl->msm_sensor_reg->default_data_type);
}

void msm_sensor_group_hold_off(struct msm_sensor_ctrl_t *s_ctrl)
{
	CDBG("+++\n");
	msm_camera_i2c_write_tbl(
		s_ctrl->sensor_i2c_client,
		s_ctrl->msm_sensor_reg->group_hold_off_conf,
		s_ctrl->msm_sensor_reg->group_hold_off_conf_size,
		s_ctrl->msm_sensor_reg->default_data_type);
}

int32_t msm_sensor_set_fps(struct msm_sensor_ctrl_t *s_ctrl,
						struct fps_cfg *fps)
{
	uint16_t total_lines_per_frame;
	int32_t rc = 0;
	s_ctrl->fps_divider = fps->fps_div;
	CDBG("+++\n");

	if (s_ctrl->curr_res != MSM_SENSOR_INVALID_RES) {
		total_lines_per_frame = (uint16_t)
			((s_ctrl->curr_frame_length_lines) *
			s_ctrl->fps_divider/Q10);

		rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
			s_ctrl->sensor_output_reg_addr->frame_length_lines,
			total_lines_per_frame, MSM_CAMERA_I2C_WORD_DATA);
	}
	return rc;
}

int32_t msm_sensor_write_exp_gain1(struct msm_sensor_ctrl_t *s_ctrl,
		uint16_t gain, uint32_t line)
{
	uint32_t fl_lines;
	uint8_t offset;
	CDBG("+++\n");
	fl_lines = s_ctrl->curr_frame_length_lines;
	fl_lines = (fl_lines * s_ctrl->fps_divider) / Q10;
	offset = s_ctrl->sensor_exp_gain_info->vert_offset;
	if (line > (fl_lines - offset))
		fl_lines = line + offset;

	s_ctrl->func_tbl->sensor_group_hold_on(s_ctrl);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_output_reg_addr->frame_length_lines, fl_lines,
		MSM_CAMERA_I2C_WORD_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_exp_gain_info->coarse_int_time_addr, line,
		MSM_CAMERA_I2C_WORD_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_exp_gain_info->global_gain_addr, gain,
		MSM_CAMERA_I2C_WORD_DATA);
	s_ctrl->func_tbl->sensor_group_hold_off(s_ctrl);
	return 0;
}

int32_t msm_sensor_write_exp_gain2(struct msm_sensor_ctrl_t *s_ctrl,
		uint16_t gain, uint32_t line)
{
	uint32_t fl_lines, ll_pclk, ll_ratio;
	uint8_t offset;
	CDBG("+++\n");
	fl_lines = s_ctrl->curr_frame_length_lines * s_ctrl->fps_divider / Q10;
	ll_pclk = s_ctrl->curr_line_length_pclk;
	offset = s_ctrl->sensor_exp_gain_info->vert_offset;
	if (line > (fl_lines - offset)) {
		ll_ratio = (line * Q10) / (fl_lines - offset);
		ll_pclk = ll_pclk * ll_ratio / Q10;
		line = fl_lines - offset;
	}

	s_ctrl->func_tbl->sensor_group_hold_on(s_ctrl);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_output_reg_addr->line_length_pclk, ll_pclk,
		MSM_CAMERA_I2C_WORD_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_exp_gain_info->coarse_int_time_addr, line,
		MSM_CAMERA_I2C_WORD_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_exp_gain_info->global_gain_addr, gain,
		MSM_CAMERA_I2C_WORD_DATA);
	s_ctrl->func_tbl->sensor_group_hold_off(s_ctrl);
	return 0;
}

int32_t msm_sensor_setting1(struct msm_sensor_ctrl_t *s_ctrl,
			int update_type, int res)
{
	int32_t rc = 0;
	static int csi_config;

	s_ctrl->func_tbl->sensor_stop_stream(s_ctrl);
	msleep(30);
	if (update_type == MSM_SENSOR_REG_INIT) {
		CDBG("Register INIT\n");
		s_ctrl->curr_csi_params = NULL;
		msm_sensor_enable_debugfs(s_ctrl);
		msm_sensor_write_init_settings(s_ctrl);
		csi_config = 0;
	} else if (update_type == MSM_SENSOR_UPDATE_PERIODIC) {
		CDBG("PERIODIC : %d\n", res);
		msm_sensor_write_conf_array(
			s_ctrl->sensor_i2c_client,
			s_ctrl->msm_sensor_reg->mode_settings, res);
		msleep(30);
		if (!csi_config) {
			s_ctrl->curr_csic_params = s_ctrl->csic_params[res];
			CDBG("CSI config in progress\n");
			v4l2_subdev_notify(&s_ctrl->sensor_v4l2_subdev,
				NOTIFY_CSIC_CFG,
				s_ctrl->curr_csic_params);
			CDBG("CSI config is done\n");
			mb();
			msleep(30);
			csi_config = 1;
		}
		s_ctrl->func_tbl->sensor_start_stream(s_ctrl);
		msleep(50);
	}
	return rc;
}
int32_t msm_sensor_setting(struct msm_sensor_ctrl_t *s_ctrl,
			int update_type, int res)
{
	int32_t rc = 0;
	CDBG("+++ update_type = %d, res = %d\n", update_type, res);

	v4l2_subdev_notify(&s_ctrl->sensor_v4l2_subdev,
		NOTIFY_ISPIF_STREAM, (void *)ISPIF_STREAM(
		PIX_0, ISPIF_OFF_IMMEDIATELY));
	s_ctrl->func_tbl->sensor_stop_stream(s_ctrl);
	msleep(30);
	if (update_type == MSM_SENSOR_REG_INIT) {
		s_ctrl->curr_csi_params = NULL;
		msm_sensor_enable_debugfs(s_ctrl);
		msm_sensor_write_init_settings(s_ctrl);
	} else if (update_type == MSM_SENSOR_UPDATE_PERIODIC) {
		msm_sensor_write_res_settings(s_ctrl, res);
		if (s_ctrl->curr_csi_params != s_ctrl->csi_params[res]) {
			s_ctrl->curr_csi_params = s_ctrl->csi_params[res];
			v4l2_subdev_notify(&s_ctrl->sensor_v4l2_subdev,
				NOTIFY_CSID_CFG,
				&s_ctrl->curr_csi_params->csid_params);
			v4l2_subdev_notify(&s_ctrl->sensor_v4l2_subdev,
						NOTIFY_CID_CHANGE, NULL);
			mb();
			v4l2_subdev_notify(&s_ctrl->sensor_v4l2_subdev,
				NOTIFY_CSIPHY_CFG,
				&s_ctrl->curr_csi_params->csiphy_params);
			mb();
			msleep(20);
		}

		v4l2_subdev_notify(&s_ctrl->sensor_v4l2_subdev,
			NOTIFY_PCLK_CHANGE, &s_ctrl->msm_sensor_reg->
			output_settings[res].op_pixel_clk);
		v4l2_subdev_notify(&s_ctrl->sensor_v4l2_subdev,
			NOTIFY_ISPIF_STREAM, (void *)ISPIF_STREAM(
			PIX_0, ISPIF_ON_FRAME_BOUNDARY));
		s_ctrl->func_tbl->sensor_start_stream(s_ctrl);
		msleep(30);
	}
	return rc;
}

int32_t msm_sensor_set_sensor_mode(struct msm_sensor_ctrl_t *s_ctrl,
	int mode, int res)
{
	int32_t rc = 0;
	CDBG("+++ mode = %d, res = %d\n", mode, res);
	if (s_ctrl->curr_res != res) {
		s_ctrl->curr_frame_length_lines =
			s_ctrl->msm_sensor_reg->
			output_settings[res].frame_length_lines;

		s_ctrl->curr_line_length_pclk =
			s_ctrl->msm_sensor_reg->
			output_settings[res].line_length_pclk;

		if (s_ctrl->func_tbl->sensor_setting
			(s_ctrl, MSM_SENSOR_UPDATE_PERIODIC, res) < 0)
			return rc;
		s_ctrl->curr_res = res;
	}

	return rc;
}

int32_t msm_sensor_mode_init(struct msm_sensor_ctrl_t *s_ctrl,
			int mode, struct sensor_init_cfg *init_info)
{
	int32_t rc = 0;
	s_ctrl->fps_divider = Q10;
	s_ctrl->cam_mode = MSM_SENSOR_MODE_INVALID;

	CDBG("%s: %d\n", __func__, __LINE__);
	if (mode != s_ctrl->cam_mode) {
		s_ctrl->curr_res = MSM_SENSOR_INVALID_RES;
		s_ctrl->cam_mode = mode;

		rc = s_ctrl->func_tbl->sensor_setting(s_ctrl,
			MSM_SENSOR_REG_INIT, 0);
	}
	return rc;
}

int32_t msm_sensor_get_output_info(struct msm_sensor_ctrl_t *s_ctrl,
		struct sensor_output_info_t *sensor_output_info)
{
	int rc = 0;
	CDBG("+++\n");
	sensor_output_info->num_info = s_ctrl->msm_sensor_reg->num_conf;
	if (copy_to_user((void *)sensor_output_info->output_info,
		s_ctrl->msm_sensor_reg->output_settings,
		sizeof(struct msm_sensor_output_info_t) *
		s_ctrl->msm_sensor_reg->num_conf))
		rc = -EFAULT;

	return rc;
}

long msm_sensor_subdev_ioctl(struct v4l2_subdev *sd,
			unsigned int cmd, void *arg)
{
	struct msm_sensor_ctrl_t *s_ctrl = get_sctrl(sd);
	void __user *argp = (void __user *)arg;
	CDBG("+++\n");
	switch (cmd) {
	case VIDIOC_MSM_SENSOR_CFG:
		return s_ctrl->func_tbl->sensor_config(s_ctrl, argp);
	default:
		return -ENOIOCTLCMD;
	}
}

int32_t msm_sensor_config(struct msm_sensor_ctrl_t *s_ctrl, void __user *argp)
{
	struct sensor_cfg_data cdata;
	long   rc = 0;
	if (copy_from_user(&cdata,
		(void *)argp,
		sizeof(struct sensor_cfg_data)))
		return -EFAULT;
	mutex_lock(s_ctrl->msm_sensor_mutex);
	CDBG("msm_sensor_config: cfgtype = %d\n",
	cdata.cfgtype);
		switch (cdata.cfgtype) {
		case CFG_SET_FPS:
		case CFG_SET_PICT_FPS:
			if (s_ctrl->func_tbl->
			sensor_set_fps == NULL) {
				rc = -EFAULT;
				break;
			}
			rc = s_ctrl->func_tbl->
				sensor_set_fps(
				s_ctrl,
				&(cdata.cfg.fps));
			break;

		case CFG_SET_EXP_GAIN:
			if (s_ctrl->func_tbl->
			sensor_write_exp_gain == NULL) {
				rc = -EFAULT;
				break;
			}
			rc =
				s_ctrl->func_tbl->
				sensor_write_exp_gain(
					s_ctrl,
					cdata.cfg.exp_gain.gain,
					cdata.cfg.exp_gain.line);
			break;

		case CFG_SET_PICT_EXP_GAIN:
			if (s_ctrl->func_tbl->
			sensor_write_snapshot_exp_gain == NULL) {
				rc = -EFAULT;
				break;
			}
			rc =
				s_ctrl->func_tbl->
				sensor_write_snapshot_exp_gain(
					s_ctrl,
					cdata.cfg.exp_gain.gain,
					cdata.cfg.exp_gain.line);
			break;

		case CFG_SET_MODE:
			if (s_ctrl->func_tbl->
			sensor_set_sensor_mode == NULL) {
				rc = -EFAULT;
				break;
			}
			rc = s_ctrl->func_tbl->
				sensor_set_sensor_mode(
					s_ctrl,
					cdata.mode,
					cdata.rs);
			break;

		case CFG_SET_EFFECT:
/* BEGIN Chaoyen_Wu@pegatron [2012.2.7][enable camera effect]*/			
			CDBG("CFG_SET_EFFECT\n");
			if (s_ctrl->func_tbl->
			sensor_set_effect == NULL) {
				CDBG("sensor_set_effect == NULL\n");
				rc = -EFAULT;
				break;
			}
			rc = s_ctrl->func_tbl->
				sensor_set_effect(
					s_ctrl,
					cdata.cfg.effect);
/* END Chaoyen_Wu@pegatron [2012.2.7][enable camera effect]*/	

			break;
	
		case CFG_SENSOR_INIT:
			if (s_ctrl->func_tbl->
			sensor_mode_init == NULL) {
				rc = -EFAULT;
				break;
			}
			rc = s_ctrl->func_tbl->
				sensor_mode_init(
				s_ctrl,
				cdata.mode,
				&(cdata.cfg.init_info));
			break;

		case CFG_GET_OUTPUT_INFO:
			if (s_ctrl->func_tbl->
			sensor_get_output_info == NULL) {
				rc = -EFAULT;
				break;
			}
			rc = s_ctrl->func_tbl->
				sensor_get_output_info(
				s_ctrl,
				&cdata.cfg.output_info);

			if (copy_to_user((void *)argp,
				&cdata,
				sizeof(struct sensor_cfg_data)))
				rc = -EFAULT;
			break;

		case CFG_GET_EEPROM_DATA:
			if (s_ctrl->sensor_eeprom_client == NULL ||
				s_ctrl->sensor_eeprom_client->
				func_tbl.eeprom_get_data == NULL) {
				rc = -EFAULT;
				break;
			}
			rc = s_ctrl->sensor_eeprom_client->
				func_tbl.eeprom_get_data(
				s_ctrl->sensor_eeprom_client,
				&cdata.cfg.eeprom_data);

			if (copy_to_user((void *)argp,
				&cdata,
				sizeof(struct sensor_eeprom_data_t)))
				rc = -EFAULT;
			break;

/* BEGIN Dom_Lin@pegatron [2011/02/04] [Enable back camera auto focus] */
		case CFG_SET_SENSOR_AUTO_FOCUS:
			CDBG("CFG_SET_SENSOR_AUTO_FOCUS\n");
			if (s_ctrl->func_tbl->
			sensor_set_auto_focus == NULL) {
				CDBG("sensor_set_auto_focus == NULL\n");
				rc = -EFAULT;
				break;
			}
			rc = s_ctrl->func_tbl->
				sensor_set_auto_focus(
					s_ctrl);
			break;
/* END Dom_Lin@pegatron [2011/02/04] [Enable back camera auto focus] */

/* BEGIN Dom_Lin@pegatron [2011/02/06] [Enable back camera white balance] */
		case CFG_SET_WB:
			CDBG("CFG_SET_WB\n");
			if (s_ctrl->func_tbl->
			sensor_set_wb == NULL) {
				CDBG("sensor_set_wb == NULL\n");
				rc = -EFAULT;
				break;
			}
			rc = s_ctrl->func_tbl->
				sensor_set_wb(
					s_ctrl,
					cdata.cfg.wb_val);
			break;
/* END Dom_Lin@pegatron [2011/02/06] [Enable back camera white balance] */

/* BEGIN Dom_Lin@pegatron [2012/02/12] [Enable back camera scene mode] */
		case CFG_SET_BESTSHOT_MODE:
			CDBG("CFG_SET_BESTSHOT_MODE\n");
			if (s_ctrl->func_tbl->
			sensor_set_bestshot_mode == NULL) {
				CDBG("sensor_set_bestshot_mode == NULL\n");
				rc = -EFAULT;
				break;
			}
			rc = s_ctrl->func_tbl->
				sensor_set_bestshot_mode(
					s_ctrl,
					cdata.cfg.bs_mode);
			break;
/* END Dom_Lin@pegatron [2012/02/12] [Enable back camera scene mode] */			
/* BEGIN Chaoyen_Wu@pegatron [2012.2.13][Enable camera saturation]*/ 
		case CFG_SET_SATURATION:
			CDBG("CFG_SET_SATURATION\n");
			if (s_ctrl->func_tbl->sensor_set_saturation == NULL) {
				CDBG("sensor_set_saturation == NULL\n");
				rc = -EFAULT;
				break;
			}
			rc = s_ctrl->func_tbl->sensor_set_saturation(
									s_ctrl,
									cdata.cfg.saturation);	
			break;		
/* END Chaoyen_Wu@pegatron [2012.2.13][Enable camera saturation]*/
/* BEGIN Chaoyen_Wu@pegatron [2012.2.14][Enable camera sharpness]*/ 
		case CFG_SET_SHARPNESS:
			CDBG("CFG_SET_SHARPNESS\n");
			if (s_ctrl->func_tbl->sensor_set_sharpness == NULL) {
				CDBG("sensor_set_sharpness == NULL\n");
				rc = -EFAULT;
				break;
			}
			rc = s_ctrl->func_tbl->sensor_set_sharpness(
									s_ctrl,
									cdata.cfg.sharpness);	
			break;		
/* END Chaoyen_Wu@pegatron [2012.2.14][Enable camera sharpness]*/
/* BEGIN Chaoyen_Wu@pegatron [2012.2.14][Enable camera contrast]*/ 
		case CFG_SET_CONTRAST:
			CDBG("CFG_SET_CONTRAST\n");
			if (s_ctrl->func_tbl->sensor_set_contrast == NULL) {
				CDBG("sensor_set_contrast == NULL\n");
				rc = -EFAULT;
				break;
			}
			rc = s_ctrl->func_tbl->sensor_set_contrast(
									s_ctrl,
									cdata.cfg.contrast);	
			break;		
/* END Chaoyen_Wu@pegatron [2012.2.14][Enable camera contrast]*/
/*BEGIN Chaoyen_Wu@pegatron[2012.2.15][Enable camera exposure compensation]*/
		case CFG_SET_EXP_COMPENSATION:
			CDBG("CFG_SET_EXP_COMPENSATION\n");
			if (s_ctrl->func_tbl->sensor_set_exp_compensation == NULL) {
				CDBG("sensor_set_exp_compensation == NULL\n");
				rc = -EFAULT;
				break;
			}
			rc = s_ctrl->func_tbl->sensor_set_exp_compensation(
									s_ctrl,
									cdata.cfg.exp_compensation);	
			break;			
/*END Chaoyen_Wu@pegatron[2012.2.15][Enable camera exposure compensation]*/
/*BEGIN Chaoyen_Wu@pegatron[2012.2.15][Enable camera anti-banding]*/
	    case CFG_SET_ANTIBANDING:
			CDBG("CFG_SET_ANTIBANDING\n");
			if (s_ctrl->func_tbl->sensor_set_antibanding == NULL) {
				CDBG("sensor_set_antibanding == NULL\n");
				rc = -EFAULT;
				break;
			}
			rc = s_ctrl->func_tbl->sensor_set_antibanding(
									s_ctrl,
									cdata.cfg.antibanding);	
			break;			
/*END Chaoyen_Wu@pegatron[2012.2.15][Enable camera anti-banding]*/	
/* BEGIN Dom_Lin@pegatron [2012/02/20] [Enable back camera touch focus] */
		case CFG_SET_SENSOR_TOUCH_FOCUS:
			CDBG("CFG_SET_SENSOR_TOUCH_FOCUS\n");
			if (s_ctrl->func_tbl->
			sensor_set_touch_focus == NULL) {
				CDBG("sensor_set_touch_focus == NULL\n");
				rc = -EFAULT;
				break;
			}
			rc = s_ctrl->func_tbl->
				sensor_set_touch_focus(
					s_ctrl,
					cdata.cfg.roi);
			break;
/* END Dom_Lin@pegatron [2012/02/20] [Enable back camera touch focus] */
/* BEGIN Dom_Lin@pegatron [2012/04/27] [Release AF when setting focus mode Infinity] */
		case CFG_SET_RELEASE_AF:
			CDBG("CFG_SET_RELEASE_AF\n");
			if (s_ctrl->func_tbl->
			sensor_set_release_af == NULL) {
				CDBG("sensor_set_release_af == NULL\n");
				rc = -EFAULT;
				break;
			}
			rc = s_ctrl->func_tbl->
				sensor_set_release_af(
					s_ctrl);
			break;
/* END Dom_Lin@pegatron [2012/04/27] [Release AF when setting focus mode Infinity] */
		default:
			rc = -EFAULT;
			break;
		}

	mutex_unlock(s_ctrl->msm_sensor_mutex);

	CDBG("--- msm_sensor_config: cfgtype = %d, rc = %ld\n", cdata.cfgtype, rc);
	return rc;
}

static struct msm_cam_clk_info cam_clk_info[] = {
	{"cam_clk", MSM_SENSOR_MCLK_24HZ},
};

int32_t msm_sensor_power_up(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	struct msm_camera_sensor_info *data = s_ctrl->sensordata;
	CDBG("%s: %d\n", __func__, __LINE__);
	s_ctrl->reg_ptr = kzalloc(sizeof(struct regulator *)
			* data->sensor_platform_info->num_vreg, GFP_KERNEL);
	if (!s_ctrl->reg_ptr) {
		pr_err("%s: could not allocate mem for regulators\n",
			__func__);
		return -ENOMEM;
	}

	rc = msm_camera_request_gpio_table(data, 1);
	if (rc < 0) {
		pr_err("%s: request gpio failed\n", __func__);
		goto request_gpio_failed;
	}

	rc = msm_camera_config_vreg(&s_ctrl->sensor_i2c_client->client->dev,
			s_ctrl->sensordata->sensor_platform_info->cam_vreg,
			s_ctrl->sensordata->sensor_platform_info->num_vreg,
			s_ctrl->reg_ptr, 1);
	if (rc < 0) {
		pr_err("%s: regulator on failed\n", __func__);
		goto config_vreg_failed;
	}

	rc = msm_camera_enable_vreg(&s_ctrl->sensor_i2c_client->client->dev,
			s_ctrl->sensordata->sensor_platform_info->cam_vreg,
			s_ctrl->sensordata->sensor_platform_info->num_vreg,
			s_ctrl->reg_ptr, 1);
	if (rc < 0) {
		pr_err("%s: enable regulator failed\n", __func__);
		goto enable_vreg_failed;
	}

	rc = msm_camera_config_gpio_table(data, 1);
	if (rc < 0) {
		pr_err("%s: config gpio failed\n", __func__);
		goto config_gpio_failed;
	}

	if (s_ctrl->clk_rate != 0)
		cam_clk_info->clk_rate = s_ctrl->clk_rate;

	rc = msm_cam_clk_enable(&s_ctrl->sensor_i2c_client->client->dev,
		cam_clk_info, &s_ctrl->cam_clk, ARRAY_SIZE(cam_clk_info), 1);
	if (rc < 0) {
		pr_err("%s: clk enable failed\n", __func__);
		goto enable_clk_failed;
	}

#if defined(CONFIG_S5K3L1YX) || defined(CONFIG_OV5640)
	usleep_range(1000, 2000);
#endif
	if (data->sensor_platform_info->ext_power_ctrl != NULL)
		data->sensor_platform_info->ext_power_ctrl(1);

	return rc;
enable_clk_failed:
		msm_camera_config_gpio_table(data, 0);
config_gpio_failed:
	msm_camera_enable_vreg(&s_ctrl->sensor_i2c_client->client->dev,
			s_ctrl->sensordata->sensor_platform_info->cam_vreg,
			s_ctrl->sensordata->sensor_platform_info->num_vreg,
			s_ctrl->reg_ptr, 0);

enable_vreg_failed:
	msm_camera_config_vreg(&s_ctrl->sensor_i2c_client->client->dev,
		s_ctrl->sensordata->sensor_platform_info->cam_vreg,
		s_ctrl->sensordata->sensor_platform_info->num_vreg,
		s_ctrl->reg_ptr, 0);
config_vreg_failed:
	msm_camera_request_gpio_table(data, 0);
request_gpio_failed:
	kfree(s_ctrl->reg_ptr);
	return rc;
}

int32_t msm_sensor_power_down(struct msm_sensor_ctrl_t *s_ctrl)
{
	struct msm_camera_sensor_info *data = s_ctrl->sensordata;
	CDBG("%s\n", __func__);
	if (data->sensor_platform_info->ext_power_ctrl != NULL)
		data->sensor_platform_info->ext_power_ctrl(0);
	msm_cam_clk_enable(&s_ctrl->sensor_i2c_client->client->dev,
		cam_clk_info, &s_ctrl->cam_clk, ARRAY_SIZE(cam_clk_info), 0);
	msm_camera_config_gpio_table(data, 0);
	msm_camera_enable_vreg(&s_ctrl->sensor_i2c_client->client->dev,
		s_ctrl->sensordata->sensor_platform_info->cam_vreg,
		s_ctrl->sensordata->sensor_platform_info->num_vreg,
		s_ctrl->reg_ptr, 0);
	msm_camera_config_vreg(&s_ctrl->sensor_i2c_client->client->dev,
		s_ctrl->sensordata->sensor_platform_info->cam_vreg,
		s_ctrl->sensordata->sensor_platform_info->num_vreg,
		s_ctrl->reg_ptr, 0);
	msm_camera_request_gpio_table(data, 0);
	kfree(s_ctrl->reg_ptr);
	return 0;
}


int32_t msm_sensor_match_id(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	uint16_t chipid = 0;
	rc = msm_camera_i2c_read(
			s_ctrl->sensor_i2c_client,
			s_ctrl->sensor_id_info->sensor_id_reg_addr, &chipid,
			MSM_CAMERA_I2C_WORD_DATA);
	if (rc < 0) {
		CDBG("%s: read id failed\n", __func__);
		return rc;
	}

	CDBG("msm_sensor id: %d = 0x%X\n", chipid, chipid);
	if (chipid != s_ctrl->sensor_id_info->sensor_id) {
		CDBG("msm_sensor_match_id chip id doesnot match\n");
		return -ENODEV;
	}
	return rc;
}

/* BEGIN Dom_Lin@pegatron [2011/02/04] [Enable back camera auto focus] */
void msm_sensor_send_stats_msg(struct msm_sensor_ctrl_t *s_ctrl, uint32_t id, uint8_t status)
{
	struct sensor_msg_stats msgStats;
	msgStats.id = id;
	msgStats.status = status;

	CDBG("+++\n");
	v4l2_subdev_notify(&s_ctrl->sensor_v4l2_subdev, NOTIFY_SENSOR_MSG_STATS, &msgStats);
}
/* END Dom_Lin@pegatron [2011/02/04] [Enable back camera auto focus] */

struct msm_sensor_ctrl_t *get_sctrl(struct v4l2_subdev *sd)
{
	return container_of(sd, struct msm_sensor_ctrl_t, sensor_v4l2_subdev);
}

int32_t msm_sensor_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int rc = 0;
	char front_cam[10] = "mt9m114";
	struct msm_sensor_ctrl_t *s_ctrl;
	CDBG("%s_i2c_probe called\n", client->name);
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		CDBG("i2c_check_functionality failed\n");
		rc = -EFAULT;
		return rc;
	}

	s_ctrl = (struct msm_sensor_ctrl_t *)(id->driver_data);
	if (s_ctrl->sensor_i2c_client != NULL) {
		s_ctrl->sensor_i2c_client->client = client;
		if (s_ctrl->sensor_i2c_addr != 0)
			s_ctrl->sensor_i2c_client->client->addr =
				s_ctrl->sensor_i2c_addr;
	} else {
		rc = -EFAULT;
		return rc;
	}
	
	s_ctrl->sensordata = client->dev.platform_data;
	if (s_ctrl->sensordata == NULL) {
		pr_err("%s: NULL sensor data\n", __func__);
		return -EFAULT;
	}

	rc = s_ctrl->func_tbl->sensor_power_up(s_ctrl);
	if (rc < 0)
		goto probe_fail;

	rc = msm_sensor_match_id(s_ctrl);
/*BEGIN Chaoyen_Wu@pegatron[2012.4.12][front camera 2nd source porting]*/	
	if(rc < 0 && !(strcmp(client->name, front_cam))){
		s_ctrl->sensor_i2c_client->client->addr = s_ctrl->sensor_i2c_addr_high;
		CDBG("2nd source i2c addr: 0x%x\n",s_ctrl->sensor_i2c_client->client->addr);
		rc = msm_sensor_match_id(s_ctrl);
	}	
/*END Chaoyen_Wu@pegatron[2012.4.12][front camera 2nd source porting]*/		
	if (rc < 0)
		goto probe_fail;

	if (s_ctrl->sensor_eeprom_client != NULL) {
		struct msm_camera_eeprom_client *eeprom_client =
			s_ctrl->sensor_eeprom_client;
		if (eeprom_client->func_tbl.eeprom_init != NULL &&
			eeprom_client->func_tbl.eeprom_release != NULL) {
			rc = eeprom_client->func_tbl.eeprom_init(
				eeprom_client,
				s_ctrl->sensor_i2c_client->client->adapter);
			if (rc < 0)
				goto probe_fail;

			rc = msm_camera_eeprom_read_tbl(eeprom_client,
			eeprom_client->read_tbl, eeprom_client->read_tbl_size);
			eeprom_client->func_tbl.eeprom_release(eeprom_client);
			if (rc < 0)
				goto probe_fail;
		}
	}

	snprintf(s_ctrl->sensor_v4l2_subdev.name,
		sizeof(s_ctrl->sensor_v4l2_subdev.name), "%s", id->name);
	v4l2_i2c_subdev_init(&s_ctrl->sensor_v4l2_subdev, client,
		s_ctrl->sensor_v4l2_subdev_ops);

	msm_sensor_register(&s_ctrl->sensor_v4l2_subdev);
	goto power_down;
probe_fail:
	CDBG("%s_i2c_probe failed\n", client->name);
power_down:
	if (rc > 0)
		rc = 0;
	s_ctrl->func_tbl->sensor_power_down(s_ctrl);
	return rc;
}

int32_t msm_sensor_power(struct v4l2_subdev *sd, int on)
{
	int rc = 0;
	struct msm_sensor_ctrl_t *s_ctrl = get_sctrl(sd);
	mutex_lock(s_ctrl->msm_sensor_mutex);
	if (on)
		rc = s_ctrl->func_tbl->sensor_power_up(s_ctrl);
	else
		rc = s_ctrl->func_tbl->sensor_power_down(s_ctrl);
	mutex_unlock(s_ctrl->msm_sensor_mutex);
	return rc;
}

int32_t msm_sensor_v4l2_enum_fmt(struct v4l2_subdev *sd, unsigned int index,
			   enum v4l2_mbus_pixelcode *code)
{
	struct msm_sensor_ctrl_t *s_ctrl = get_sctrl(sd);

	if ((unsigned int)index >= s_ctrl->sensor_v4l2_subdev_info_size)
		return -EINVAL;

	*code = s_ctrl->sensor_v4l2_subdev_info[index].code;
	return 0;
}

int32_t msm_sensor_v4l2_s_ctrl(struct v4l2_subdev *sd,
	struct v4l2_control *ctrl)
{
	int rc = -1, i = 0;
	struct msm_sensor_ctrl_t *s_ctrl =
		(struct msm_sensor_ctrl_t *) sd->dev_priv;
	struct msm_sensor_v4l2_ctrl_info_t *v4l2_ctrl =
		s_ctrl->msm_sensor_v4l2_ctrl_info;

	CDBG("%s\n", __func__);
	CDBG("%d\n", ctrl->id);
	if (v4l2_ctrl == NULL)
		return rc;

	for (i = 0; i < s_ctrl->num_v4l2_ctrl; i++) {
		if (v4l2_ctrl[i].ctrl_id == ctrl->id) {
			if (v4l2_ctrl[i].s_v4l2_ctrl != NULL) {
				rc = v4l2_ctrl[i].s_v4l2_ctrl(
					s_ctrl,
					&s_ctrl->msm_sensor_v4l2_ctrl_info[i],
					ctrl->value);
			}
			break;
		}
	}

	return rc;
}

int32_t msm_sensor_v4l2_query_ctrl(
	struct v4l2_subdev *sd, struct v4l2_queryctrl *qctrl)
{
	int rc = -1, i = 0;
	struct msm_sensor_ctrl_t *s_ctrl =
		(struct msm_sensor_ctrl_t *) sd->dev_priv;

	CDBG("%s\n", __func__);
	CDBG("%s id: %d\n", __func__, qctrl->id);

	if (s_ctrl->msm_sensor_v4l2_ctrl_info == NULL)
		return rc;

	for (i = 0; i < s_ctrl->num_v4l2_ctrl; i++) {
		if (s_ctrl->msm_sensor_v4l2_ctrl_info[i].ctrl_id == qctrl->id) {
			qctrl->minimum =
				s_ctrl->msm_sensor_v4l2_ctrl_info[i].min;
			qctrl->maximum =
				s_ctrl->msm_sensor_v4l2_ctrl_info[i].max;
			qctrl->flags = 1;
			rc = 0;
			break;
		}
	}

	return rc;
}

int msm_sensor_s_ctrl_by_enum(struct msm_sensor_ctrl_t *s_ctrl,
		struct msm_sensor_v4l2_ctrl_info_t *ctrl_info, int value)
{
	int rc = 0;
	CDBG("%s enter\n", __func__);
	rc = msm_sensor_write_enum_conf_array(
		s_ctrl->sensor_i2c_client,
		ctrl_info->enum_cfg_settings, value);
	return rc;
}

/* BEGIN Dom_Lin@pegatron [2012/05/04] [Reduce the time of opening back camera] */
static uint8_t msm_sensor_debugfs_enable = 0;
/* END Dom_Lin@pegatron [2012/05/04] [Reduce the time of opening back camera] */

static int msm_sensor_debugfs_stream_s(void *data, u64 val)
{
	struct msm_sensor_ctrl_t *s_ctrl = (struct msm_sensor_ctrl_t *) data;
	CDBG("+++\n");
	if (val)
		s_ctrl->func_tbl->sensor_start_stream(s_ctrl);
	else
		s_ctrl->func_tbl->sensor_stop_stream(s_ctrl);
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(sensor_debugfs_stream, NULL,
			msm_sensor_debugfs_stream_s, "%llu\n");

static int msm_sensor_debugfs_test_s(void *data, u64 val)
{
	CDBG("val: %llu\n", val);
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(sensor_debugfs_test, NULL,
			msm_sensor_debugfs_test_s, "%llu\n");

/* BEGIN Dom_Lin@pegatron [2012/03/09] [Add camera debug node for read I2C data by adb] */
static int msm_sensor_debugfs_read_s(void *data, u64 val)
{
	int32_t rc = 0;
	struct msm_sensor_ctrl_t *s_ctrl = (struct msm_sensor_ctrl_t *) data;
	uint16_t addr = (uint16_t)val;
	uint16_t s_data = 0;

	rc = msm_camera_i2c_read(
			s_ctrl->sensor_i2c_client,
			addr, &s_data,
			MSM_CAMERA_I2C_BYTE_DATA);
	if (rc < 0) {
		CDBG("read %04x failed\n", addr);
	}
	else {
		CDBG("0x%04x = 0x%02X\n", addr, s_data);
	}
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(sensor_debugfs_read, NULL,
			msm_sensor_debugfs_read_s, "%04llx\n");
/* END Dom_Lin@pegatron [2012/03/09] [Add camera debug node for read I2C data by adb] */

/* BEGIN Dom_Lin@pegatron [2012/04/18] [Add camera debug node for write I2C data by adb] */
static uint16_t msm_sensor_debug_addr = 0x0000;
static int msm_sensor_debugfs_addr_s(void *data, u64 val)
{
	uint16_t addr = (uint16_t)val;
//	CDBG("set write address %04x\n", addr);
	msm_sensor_debug_addr = addr;
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(sensor_debugfs_addr, NULL,
			msm_sensor_debugfs_addr_s, "%04llx\n");

static int msm_sensor_debugfs_write_s(void *data, u64 val)
{
	int32_t rc = 0;
	struct msm_sensor_ctrl_t *s_ctrl = (struct msm_sensor_ctrl_t *) data;
	uint16_t s_data = (uint8_t)val;

	rc = msm_camera_i2c_write(
			s_ctrl->sensor_i2c_client,
			msm_sensor_debug_addr, s_data,
			MSM_CAMERA_I2C_BYTE_DATA);
	if (rc < 0) {
		CDBG("write %04x failed\n", msm_sensor_debug_addr);
	}
	else {
		CDBG("write 0x%04x: 0x%02X\n", msm_sensor_debug_addr, s_data);
	}

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(sensor_debugfs_write, NULL,
			msm_sensor_debugfs_write_s, "%02llx\n");
/* END Dom_Lin@pegatron [2012/04/18] [Add camera debug node for write I2C data by adb] */

int msm_sensor_enable_debugfs(struct msm_sensor_ctrl_t *s_ctrl)
{
	struct dentry *debugfs_base, *sensor_dir;

/* BEGIN Dom_Lin@pegatron [2012/05/04] [Reduce the time of opening back camera] */
	if (msm_sensor_debugfs_enable)
		return 0;
/* END Dom_Lin@pegatron [2012/05/04] [Reduce the time of opening back camera] */

	CDBG("+++\n");

	debugfs_base = debugfs_create_dir("msm_sensor", NULL);
	if (!debugfs_base)
		return -ENOMEM;

	sensor_dir = debugfs_create_dir
		(s_ctrl->sensordata->sensor_name, debugfs_base);
	if (!sensor_dir)
		return -ENOMEM;

	if (!debugfs_create_file("stream", S_IRUGO | S_IWUSR, sensor_dir,
			(void *) s_ctrl, &sensor_debugfs_stream))
		return -ENOMEM;

	if (!debugfs_create_file("test", S_IRUGO | S_IWUSR, sensor_dir,
			(void *) s_ctrl, &sensor_debugfs_test))
		return -ENOMEM;

/* BEGIN Dom_Lin@pegatron [2012/03/09] [Add camera debug node for read I2C data by adb] */
	if (!debugfs_create_file("read", S_IRUGO | S_IWUSR, sensor_dir,
			(void *) s_ctrl, &sensor_debugfs_read))
		return -ENOMEM;
/* END Dom_Lin@pegatron [2012/03/09] [Add camera debug node for read I2C data by adb] */

/* BEGIN Dom_Lin@pegatron [2012/04/18] [Add camera debug node for write I2C data by adb] */
	if (!debugfs_create_file("addr", S_IRUGO | S_IWUSR, sensor_dir,
			(void *) s_ctrl, &sensor_debugfs_addr))
		return -ENOMEM;

	if (!debugfs_create_file("write", S_IRUGO | S_IWUSR, sensor_dir,
			(void *) s_ctrl, &sensor_debugfs_write))
		return -ENOMEM;
/* END Dom_Lin@pegatron [2012/04/18] [Add camera debug node for write I2C data by adb] */

/* BEGIN Dom_Lin@pegatron [2012/05/04] [Reduce the time of opening back camera] */
	msm_sensor_debugfs_enable = 1;
/* END Dom_Lin@pegatron [2012/05/04] [Reduce the time of opening back camera] */
	return 0;
}
