//[*]--------------------------------------------------------------------------------------------------[*]
//
//
// 
//  I2C Touchscreen driver
//  2012.01.17
// 
//
//[*]--------------------------------------------------------------------------------------------------[*]
#include <linux/device.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/hrtimer.h>
#include <asm/unaligned.h>
#include <linux/firmware.h>
#include <linux/delay.h>
#include <linux/fs.h>

#include <linux/string.h>

//[*]--------------------------------------------------------------------------------------------------[*]
#include "touch.h"
#include "touch-i2c.h"
#include "touch-sysfs.h"
#include "touch-update.h"

//[*]--------------------------------------------------------------------------------------------------[*]
//
//   sysfs function prototype define
//
//[*]--------------------------------------------------------------------------------------------------[*]
//   disablel (1 -> disable irq, cancel work, 0 -> enable irq), show irq state
//[*]--------------------------------------------------------------------------------------------------[*]
static	ssize_t show_idle			(struct device *dev, struct device_attribute *attr, char *buf);
static	ssize_t store_idle			(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
static	DEVICE_ATTR(idle_reg, 0664, show_idle, store_idle);

//[*]--------------------------------------------------------------------------------------------------[*]
//   disablel (1 -> disable irq, cancel work, 0 -> enable irq), show irq state
//[*]--------------------------------------------------------------------------------------------------[*]
static	ssize_t show_disable			(struct device *dev, struct device_attribute *attr, char *buf);
static	ssize_t store_disable			(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
static	DEVICE_ATTR(disable, 0664, show_disable, store_disable);

//[*]--------------------------------------------------------------------------------------------------[*]
//  fw version display
//[*]--------------------------------------------------------------------------------------------------[*]
static	ssize_t show_fw_version			(struct device *dev, struct device_attribute *attr, char *buf);
static	DEVICE_ATTR(fw_version, 0664, show_fw_version, NULL);

//[*]--------------------------------------------------------------------------------------------------[*]
//   fw data load : fw load status, fw data load
//[*]--------------------------------------------------------------------------------------------------[*]
static	ssize_t show_fw				(struct device *dev, struct device_attribute *attr, char *buf);
static	ssize_t store_fw			(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
static	DEVICE_ATTR(fw, 0664, show_fw, store_fw);

//[*]--------------------------------------------------------------------------------------------------[*]
//   fw status : fw status
//[*]--------------------------------------------------------------------------------------------------[*]
static	ssize_t show_fw_status		(struct device *dev, struct device_attribute *attr, char *buf);
static	ssize_t store_fw_status		(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
static	DEVICE_ATTR(fw_status, 0664, show_fw_status, store_fw_status);

//[*]--------------------------------------------------------------------------------------------------[*]
//   update_fw : 1 -> update fw (load fw)
//[*]--------------------------------------------------------------------------------------------------[*]
static	ssize_t store_update_fw			(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
static	DEVICE_ATTR(update_fw, 0664, NULL, store_update_fw);

//[*]--------------------------------------------------------------------------------------------------[*]
//   calibration (1 -> update calibration, 0 -> nothing), show -> NULL
//[*]--------------------------------------------------------------------------------------------------[*]
static	ssize_t store_calibration		(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
static	DEVICE_ATTR(calibration, 0664, NULL, store_calibration);

//[*]--------------------------------------------------------------------------------------------------[*]
//   hw reset
//[*]--------------------------------------------------------------------------------------------------[*]
static	ssize_t store_reset				(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
static	DEVICE_ATTR(reset, 0664, NULL, store_reset);

//[*]--------------------------------------------------------------------------------------------------[*]
//[*]--------------------------------------------------------------------------------------------------[*]
static struct attribute *touch_sysfs_entries[] = {
	&dev_attr_idle_reg.attr,
	&dev_attr_disable.attr,
	&dev_attr_fw_version.attr,
	&dev_attr_fw.attr,
	&dev_attr_fw_status.attr,
	&dev_attr_update_fw.attr,
	&dev_attr_calibration.attr,
	&dev_attr_reset.attr,
	NULL
};

static struct attribute_group touch_attr_group = {
	.name   = NULL,
	.attrs  = touch_sysfs_entries,
};

//[*]--------------------------------------------------------------------------------------------------[*]
//[*]--------------------------------------------------------------------------------------------------[*]
//   disablel (1 -> disable irq, cancel work, 0 -> enable irq), show irq state
//[*]--------------------------------------------------------------------------------------------------[*]
static	ssize_t show_idle				(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct touch 	*ts = dev_get_drvdata(dev);
	unsigned char	idle = 0;

	if(!err_mask(ts->fw_status) && !(ts->fw_status & STATUS_BOOT_MODE))	{
		touch_disable(ts);
		touch_wake_control(ts);
		if(touch_i2c_read(ts->client, REG_TS_IDLE_RD, 1, &idle) == 1)		{
			touch_enable(ts);
			return sprintf(buf, "%d\n", idle);
		}
		touch_enable(ts);
	}
	return sprintf(buf, "%d\n", -1);
}

//[*]--------------------------------------------------------------------------------------------------[*]
static	ssize_t store_idle			(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct touch 	*ts = dev_get_drvdata(dev);
	unsigned long 	val  = 0;
	unsigned char	idle = 0;
	int 			err;

	if ((err = strict_strtoul(buf, 10, &val)))	return err;

	if (val > 255 && val < 0)	idle = 0;
	else	{
		idle = (unsigned char)val;
		if(!err_mask(ts->fw_status) && !(ts->fw_status & STATUS_BOOT_MODE))	{
			touch_disable(ts);
			touch_wake_control(ts);
			touch_i2c_write(ts->client, REG_TS_IDLE_WR, 1, &idle);
			touch_enable(ts);
		}
	}

	return count;
}

//[*]--------------------------------------------------------------------------------------------------[*]
static	ssize_t store_reset				(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct touch 	*ts = dev_get_drvdata(dev);
	unsigned long 	val;
	int 			err;

	if ((err = strict_strtoul(buf, 10, &val)))	return err;

	if((ts->pdata->reset_gpio) && (val == 1))	touch_hw_reset(ts);

	return count;
}

//[*]--------------------------------------------------------------------------------------------------[*]
//   disablel (1 -> disable irq, cancel work, 0 -> enable irq), show irq state
//[*]--------------------------------------------------------------------------------------------------[*]
static	ssize_t show_disable			(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct touch 	*ts = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", ts->disabled);
}

//[*]--------------------------------------------------------------------------------------------------[*]
static	ssize_t store_disable			(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct touch 	*ts = dev_get_drvdata(dev);
	unsigned long 	val;
	int 			err;

	if ((err = strict_strtoul(buf, 10, &val)))	return err;

	if (val) 	touch_disable(ts);	// interrupt disable
	else		touch_enable(ts);	// interrupt enable

	return count;
}

//[*]--------------------------------------------------------------------------------------------------[*]
// firmware version display
//[*]--------------------------------------------------------------------------------------------------[*]
static	ssize_t show_fw_version			(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct touch 	*ts = dev_get_drvdata(dev);

	if(!err_mask(ts->fw_status) && !(ts->fw_status & STATUS_BOOT_MODE))	{
		touch_disable(ts);
		touch_wake_control(ts);
		touch_i2c_read(ts->client, REG_TS_FIRMWARE_ID, 1, &ts->fw_version);
		touch_enable(ts);
	}

	return sprintf(buf, "%d\n", ts->fw_version);
}

//[*]--------------------------------------------------------------------------------------------------[*]
//   update_fw 
//[*]--------------------------------------------------------------------------------------------------[*]

static	ssize_t store_fw		(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct touch 	*ts = dev_get_drvdata(dev);

	if(ts->fw_buf != NULL)	{
		
		if(ts->fw_size < MAX_FW_SIZE)	memcpy(&ts->fw_buf[ts->fw_size], buf, count);

		ts->fw_size += count;
	}
	
	return count;
}

//[*]--------------------------------------------------------------------------------------------------[*]
static	ssize_t show_fw			(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct touch 	*ts = dev_get_drvdata(dev);

	return	sprintf(buf, "%d\n", ts->fw_size);
}

//[*]--------------------------------------------------------------------------------------------------[*]
//[*]--------------------------------------------------------------------------------------------------[*]
static	ssize_t store_fw_status	(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct touch 	*ts = dev_get_drvdata(dev);
	int 			err;
	unsigned long 	val;

	if ((err = strict_strtoul(buf, 10, &val)))		return err;

	switch(val)	{
		case	STATUS_BOOT_MODE:
			ts->fw_status |= (touch_set_mode(ts, TOUCH_BOOT_MODE) == 0) ? STATUS_BOOT_MODE : err_status(STATUS_BOOT_MODE);
			if(!err_mask(ts->fw_status))	{
				ts->fw_size = 0;
				if(ts->fw_buf != NULL)	memset(ts->fw_buf, 0x00, MAX_FW_SIZE);	
				else					ts->fw_buf = (unsigned char *)kzalloc(MAX_FW_SIZE, GFP_KERNEL);
				
				if(ts->fw_buf == NULL)	ts->fw_status |= err_status(STATUS_NO_MEMORY);
			}
			break;
		case	STATUS_FW_CHECK:
			if(ts->fw_status & STATUS_BOOT_MODE)	{
				ts->fw_status &= (~STATUS_FW_CHECK);
				ts->fw_status |= (ts->fw_size <= MAX_FW_SIZE) ? STATUS_FW_CHECK : err_status(STATUS_FW_CHECK);
			}
			break;
		case	STATUS_FW_ERASE:
			if(ts->fw_status & STATUS_BOOT_MODE)	{
				ts->fw_status &= (~STATUS_FW_ERASE);
				ts->fw_status |= (touch_flash_erase(ts, ts->fw_size) == 0) ? STATUS_FW_ERASE : err_status(STATUS_FW_ERASE);
			}
			break;
		case	STATUS_FW_WRITE:
			if(ts->fw_status & STATUS_BOOT_MODE)	{
				ts->fw_status &= (~STATUS_FW_WRITE);
				ts->fw_status |= (touch_flash_write(ts, ts->fw_size, ts->fw_buf) == 0) ? STATUS_FW_WRITE : err_status(STATUS_FW_WRITE);
			}
			break;
		case	STATUS_FW_VERIFY:
			if(ts->fw_status & STATUS_BOOT_MODE)	{
				ts->fw_status &= (~STATUS_FW_VERIFY);
				ts->fw_status |= (touch_flash_verify(ts, ts->fw_size, ts->fw_buf) == 0) ? STATUS_FW_VERIFY : err_status(STATUS_FW_VERIFY);
			}
			break;
		case	STATUS_USER_MODE:
			if(ts->fw_buf != NULL)	{
				kfree(ts->fw_buf);	ts->fw_buf = NULL;	ts->fw_size = 0;
			}
			ts->fw_status = 
				(touch_set_mode(ts, TOUCH_USER_MODE) == 0) ? STATUS_USER_MODE : (err_status(STATUS_USER_MODE) | STATUS_BOOT_MODE);
			break;
		default	:
			ts->fw_status |= err_status(STATUS_NO_COMMAND);
			break;
	}
	return count;
}

//[*]--------------------------------------------------------------------------------------------------[*]
static	ssize_t show_fw_status	(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct touch 	*ts = dev_get_drvdata(dev);

	return	sprintf(buf, "0x%08X\n", ts->fw_status);
}

//[*]--------------------------------------------------------------------------------------------------[*]
static	ssize_t store_update_fw			(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long 	val;
	int 			err;
	struct touch 	*ts = dev_get_drvdata(dev);

	if ((err = strict_strtoul(buf, 10, &val)))	return 	err;

	if (val == 1)	{
		if(!err_mask(ts->fw_status) && !(ts->fw_status & STATUS_BOOT_MODE))	{
			touch_update_fw(dev, FIRMWARE_NAME);
		}
	}

	return count;
}

//[*]--------------------------------------------------------------------------------------------------[*]
//   calibration (1 -> update calibration, 0 -> nothing), show -> NULL
//[*]--------------------------------------------------------------------------------------------------[*]
static	ssize_t store_calibration		(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct touch 	*ts = dev_get_drvdata(dev);
	unsigned long 	val;
	int 			err;

	if ((err = strict_strtoul(buf, 10, &val)))	return err;

	if (val == 1)	{
		if(!err_mask(ts->fw_status) && !(ts->fw_status & STATUS_BOOT_MODE))	{
			touch_calibration(ts);
		}
	}

	return count;
}

//[*]--------------------------------------------------------------------------------------------------[*]
//[*]--------------------------------------------------------------------------------------------------[*]
int		touch_sysfs_create		(struct device *dev)	
{
	return	sysfs_create_group(&dev->kobj, &touch_attr_group);
}

//[*]--------------------------------------------------------------------------------------------------[*]
void	touch_sysfs_remove		(struct device *dev)	
{
    sysfs_remove_group(&dev->kobj, &touch_attr_group);
}

//[*]--------------------------------------------------------------------------------------------------[*]
MODULE_AUTHOR("HardKernel Co., Ltd.");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Touchscreen Driver");
//[*]--------------------------------------------------------------------------------------------------[*]
