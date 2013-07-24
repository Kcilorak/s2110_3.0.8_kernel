#ifndef _OV5640_V4L2_H__
#define _OV5640_V4L2_H__

/* Refer vendor/qcom/proprietary/mm-camera/server/hardware/sensor/sensor_interface.h */
typedef enum {
	SENSOR_MODE_SNAPSHOT,
	SENSOR_MODE_RAW_SNAPSHOT,
	SENSOR_MODE_PREVIEW,
	SENSOR_MODE_VIDEO,
	SENSOR_MODE_VIDEO_HD,
	SENSOR_MODE_HFR_60FPS,
	SENSOR_MODE_HFR_90FPS,
	SENSOR_MODE_HFR_120FPS,
	SENSOR_MODE_ZSL,
	SENSOR_MODE_INVALID,
} ov5640_sensor_mode_t;

/* Refer vendor/qcom/proprietary/mm-camera/common/camera.h */
typedef enum {
	STROBE_FLASH_MODE_OFF,
	STROBE_FLASH_MODE_AUTO,
	STROBE_FLASH_MODE_ON, 
	STROBE_FLASH_MODE_MAX,
} ov5640_strobe_flash_mode_t;

void ov5640_set_flash_mode(int mode);

#endif  /* _OV5640_V4L2_H__ */

