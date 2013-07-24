//[*]--------------------------------------------------------------------------------------------------[*]
//
//
// 
//  I2C Touchscreen driver
//  2012.01.17
// 
//
//[*]--------------------------------------------------------------------------------------------------[*]
#ifndef _TOUCH_H_
#define _TOUCH_H_

//[*]--------------------------------------------------------------------------------------------------[*]
#include <linux/input/touch_pdata.h>

//[*]--------------------------------------------------------------------------------------------------[*]
// extern function define
//[*]--------------------------------------------------------------------------------------------------[*]
extern	void	touch_enable		(struct touch *ts);
extern	void	touch_disable		(struct touch *ts);
extern	int		touch_remove		(struct device *dev);
extern	int		touch_probe			(struct i2c_client *client);
extern	void 	touch_hw_reset		(struct touch *ts);

#ifndef CONFIG_HAS_EARLYSUSPEND
	extern	void 	touch_suspend	(struct device *dev);
	extern	void	touch_resume	(struct device *dev);
#endif

extern	void	touch_information_display	(struct touch *ts);//jack
extern	int 	touch_wake_status			(struct touch *ts);
extern	void	touch_wake_control			(struct touch *ts);//jack

//[*]--------------------------------------------------------------------------------------------------[*]
#endif /* _TOUCH_H_ */

//[*]--------------------------------------------------------------------------------------------------[*]
//[*]--------------------------------------------------------------------------------------------------[*]
