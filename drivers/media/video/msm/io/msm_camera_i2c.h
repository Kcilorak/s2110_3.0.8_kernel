/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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

#ifndef MSM_CAMERA_I2C_H
#define MSM_CAMERA_I2C_H

#include <linux/i2c.h>
#include <linux/delay.h>
#include <mach/camera.h>

#define CONFIG_MSM_CAMERA_I2C_DBG 0

#if CONFIG_MSM_CAMERA_I2C_DBG
#define S_I2C_DBG(fmt, args...) printk(fmt, ##args)
#else
#define S_I2C_DBG(fmt, args...) CDBG(fmt, ##args)
#endif

enum msm_camera_i2c_reg_addr_type {
	MSM_CAMERA_I2C_BYTE_ADDR = 1,
	MSM_CAMERA_I2C_WORD_ADDR,
};

struct msm_camera_i2c_client {
	struct i2c_client *client;
	enum msm_camera_i2c_reg_addr_type addr_type;
};

enum msm_camera_i2c_data_type {
	MSM_CAMERA_I2C_BYTE_DATA = 1,
	MSM_CAMERA_I2C_WORD_DATA,
	MSM_CAMERA_I2C_SET_BYTE_MASK,
	MSM_CAMERA_I2C_UNSET_BYTE_MASK,
	MSM_CAMERA_I2C_SET_WORD_MASK,
	MSM_CAMERA_I2C_UNSET_WORD_MASK,
/* BEGIN Dom_Lin@pegatron [2012/03/14] [Tuning back camera I2C write for reducing open camera time] */
	MSM_CAMERA_I2C_BYTE_DATA_AUTO,
/* END Dom_Lin@pegatron [2012/03/14] [Tuning back camera I2C write for reducing open camera time] */
};

enum msm_camera_i2c_cmd_type {
	MSM_CAMERA_I2C_CMD_WRITE,
	MSM_CAMERA_I2C_CMD_POLL,
/* BEGIN Dom_Lin@pegatron [2011/02/06] [Add I2C delay function] */
	MSM_CAMERA_I2C_CMD_DELAY,
/* END Dom_Lin@pegatron [2011/02/06] [Add I2C delay function] */
/* BEGIN Dom_Lin@pegatron [2012/02/12] [Add I2C skip function] */
	MSM_CAMERA_I2C_CMD_NULL,
/* END Dom_Lin@pegatron [2012/02/12] [Add I2C skip function] */
/* BEGIN Dom_Lin@pegatron [2012/02/13] [Add I2C write with mask function] */
	MSM_CAMERA_I2C_CMD_WRITE_WITH_MASK,
/* END Dom_Lin@pegatron [2012/02/13] [Add I2C write with mask function] */
};

struct msm_camera_i2c_reg_conf {
	uint16_t reg_addr;
	uint16_t reg_data;
	enum msm_camera_i2c_data_type dt;
	enum msm_camera_i2c_cmd_type cmd_type;
/* BEGIN Dom_Lin@pegatron [2012/02/13] [Add I2C write with mask function] */
	uint16_t reg_mask;
/* END Dom_Lin@pegatron [2012/02/13] [Add I2C write with mask function] */
};

struct msm_camera_i2c_conf_array {
	struct msm_camera_i2c_reg_conf *conf;
	uint16_t size;
	uint16_t delay;
	enum msm_camera_i2c_data_type data_type;
};

struct msm_camera_i2c_enum_conf_array {
	struct msm_camera_i2c_conf_array *conf;
	int *conf_enum;
	uint16_t num_enum;
	uint16_t num_index;
	uint16_t num_conf;
	uint16_t delay;
	enum msm_camera_i2c_data_type data_type;
};

int32_t msm_camera_i2c_rxdata(struct msm_camera_i2c_client *client,
	unsigned char *rxdata, int data_length);

int32_t msm_camera_i2c_txdata(struct msm_camera_i2c_client *client,
	unsigned char *txdata, int length);

int32_t msm_camera_i2c_read(struct msm_camera_i2c_client *client,
	uint16_t addr, uint16_t *data,
	enum msm_camera_i2c_data_type data_type);

int32_t msm_camera_i2c_read_seq(struct msm_camera_i2c_client *client,
	uint16_t addr, uint8_t *data, uint16_t num_byte);

int32_t msm_camera_i2c_write(struct msm_camera_i2c_client *client,
	uint16_t addr, uint16_t data,
	enum msm_camera_i2c_data_type data_type);

int32_t msm_camera_i2c_write_seq(struct msm_camera_i2c_client *client,
	uint16_t addr, uint8_t *data, uint16_t num_byte);

int32_t msm_camera_i2c_write_with_mask(struct msm_camera_i2c_client *client,
	uint16_t addr, uint16_t data, uint16_t mask,
	enum msm_camera_i2c_data_type data_type);

int32_t msm_camera_i2c_set_mask(struct msm_camera_i2c_client *client,
	uint16_t addr, uint16_t mask,
	enum msm_camera_i2c_data_type data_type, uint16_t flag);

int32_t msm_camera_i2c_compare(struct msm_camera_i2c_client *client,
	uint16_t addr, uint16_t data,
	enum msm_camera_i2c_data_type data_type);

int32_t msm_camera_i2c_poll(struct msm_camera_i2c_client *client,
	uint16_t addr, uint16_t data,
	enum msm_camera_i2c_data_type data_type);

int32_t msm_camera_i2c_write_tbl(struct msm_camera_i2c_client *client,
	struct msm_camera_i2c_reg_conf *reg_conf_tbl, uint16_t size,
	enum msm_camera_i2c_data_type data_type);

int32_t msm_sensor_write_conf_array(struct msm_camera_i2c_client *client,
	struct msm_camera_i2c_conf_array *array, uint16_t index);

int32_t msm_sensor_write_enum_conf_array(struct msm_camera_i2c_client *client,
	struct msm_camera_i2c_enum_conf_array *conf, uint16_t enum_val);

int32_t msm_sensor_write_all_conf_array(struct msm_camera_i2c_client *client,
	struct msm_camera_i2c_conf_array *array, uint16_t size);
#endif
