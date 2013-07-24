//[*]--------------------------------------------------------------------------------------------------[*]
//
//
// 
//  I2C Touchscreen driver
//  2012.01.17
// 
//
//[*]--------------------------------------------------------------------------------------------------[*]
#ifndef _TOUCH_UPDATE_H_
#define _TOUCH_UPDATE_H_

//[*]--------------------------------------------------------------------------------------------------[*]
// extern function define
//[*]--------------------------------------------------------------------------------------------------[*]
extern	int touch_update_fw				(struct device *dev, const char *fw_name);
extern	int touch_default_fw			(struct device *dev);
extern	int touch_flash_erase			(struct touch *ts, unsigned int fw_size);
extern	int touch_flash_write			(struct touch *ts, unsigned int fw_size, unsigned char *data);
extern	int touch_flash_verify			(struct touch *ts, unsigned int fw_size, unsigned char *data);
extern	int touch_calibration			(struct touch *ts);
extern	int	touch_set_mode				(struct touch *ts, unsigned char mode);
extern	int	touch_get_mode				(struct touch *ts);

//[*]--------------------------------------------------------------------------------------------------[*]
#endif /* _TOUCH_UPDATE_H_ */

//[*]--------------------------------------------------------------------------------------------------[*]
//[*]--------------------------------------------------------------------------------------------------[*]
