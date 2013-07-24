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
#include <linux/gpio.h>
#include <linux/i2c.h>

//[*]--------------------------------------------------------------------------------------------------[*]
#include "touch.h"
#include "touch-i2c.h"
#include "touch-sysfs.h"
#include "touch-update.h"

#include "lgd_default.h"
//[*]--------------------------------------------------------------------------------------------------[*]
// function prototype define
//[*]--------------------------------------------------------------------------------------------------[*]
static 	int touch_busy_check			(struct touch *ts);
static 	int touch_i2c_bootmode_read		(struct touch *ts, unsigned char *cmd, unsigned int len, unsigned char *data);
static 	int touch_i2c_bootmode_write	(struct touch *ts, unsigned char *cmd, unsigned int len, unsigned char *data);

//[*]--------------------------------------------------------------------------------------------------[*]
static	int touch_enter_boot_mode		(struct touch *ts);
static	int touch_enter_user_mode		(struct touch *ts);
static	int touch_flash_firmware		(struct device *dev, unsigned int fw_size, unsigned char *data);
static 	int touch_flash_bootmagic		(struct touch *ts);
		int touch_update_fw				(struct device *dev, const char *fw_name);
		int touch_default_fw			(struct device *dev);
		int touch_flash_erase			(struct touch *ts, unsigned int fw_size);
		int touch_flash_write			(struct touch *ts, unsigned int fw_size, unsigned char *data);
		int touch_flash_verify			(struct touch *ts, unsigned int fw_size, unsigned char *data);
		int touch_calibration			(struct touch *ts);
		int	touch_set_mode				(struct touch *ts, unsigned char mode);
		int	touch_get_mode				(struct touch *ts);

//[*]--------------------------------------------------------------------------------------------------[*]
//[*]--------------------------------------------------------------------------------------------------[*]
static int touch_busy_check(struct touch *ts)
{
	int	retry = 3000;	// wait 3 sec
	
	while(retry)	{
		
		mdelay(1);		retry--;
		
		if(touch_wake_status(ts) == 0)		return	0;	// ready
		
	}
	printk("%s error!\n", __func__);
	return	-1;
}

//[*]--------------------------------------------------------------------------------------------------[*]
static int touch_i2c_bootmode_read(struct touch *ts, unsigned char *cmd, unsigned int len, unsigned char *data)
{
#if defined(I2C_RESTART_USED)
	struct i2c_msg	msg[2];
#else
	struct i2c_msg	msg;
#endif	
	int 			ret;

	if((len == 0) || (data == NULL))	{
		dev_err(&ts->client->dev, "I2C read error: Null pointer or length == 0\n");	
		return 	-1;
	}

#if defined(I2C_RESTART_USED)
	memset(msg, 0x00, sizeof(msg));

	msg[0].addr 	= ts->client->addr;
	msg[0].flags 	= 0;
	msg[0].len 		= sizeof(command_t);
	msg[0].buf 		= cmd;

	msg[1].addr 	= ts->client->addr;
	msg[1].flags    = I2C_M_RD;
	msg[1].len 		= len;
	msg[1].buf 		= data;
	
	// busy check error
	if(touch_busy_check(ts))	return	-1;
		
	if ((ret = i2c_transfer(ts->client->adapter, msg, 2)) != 2) {
		dev_err(&ts->client->dev, "I2C read error: (%d) reg: 0x%X len: %d\n", ret, cmd[0], len);
		return -EIO;
	}
#else
	memset(&msg, 0x00, sizeof(msg));

	msg.addr 	= ts->client->addr;		msg.flags 	= 0;
	msg.len		= sizeof(command_t);	msg.buf 	= cmd;

	// busy check error
	if(touch_busy_check(ts))	return	-1;
		
	if ((ret = i2c_transfer(ts->client->adapter, &msg, 1)) != 1) {
		dev_err(&ts->client->dev, "I2C read error: (%d) reg: 0x%X len: %d\n", ret, cmd[0], len);
		return -EIO;
	}

	msg.addr 	= ts->client->addr;		msg.flags  	= I2C_M_RD;
	msg.len 	= len;					msg.buf		= data;
	
	// busy check error
	if(touch_busy_check(ts))	return	-1;
		
	if ((ret = i2c_transfer(ts->client->adapter, &msg, 1)) != 1) {
		dev_err(&ts->client->dev, "I2C read error: (%d) reg: 0x%X len: %d\n", ret, cmd[0], len);
		return -EIO;
	}
#endif
	return len;
}

//[*]--------------------------------------------------------------------------------------------------[*]
static int touch_i2c_bootmode_write(struct touch *ts, unsigned char *cmd, unsigned int len, unsigned char *data)
{
	int 			ret;
	unsigned char	block_data[I2C_SEND_MAX_SIZE + sizeof(command_t) + 1];

	if(len > I2C_SEND_MAX_SIZE)	{
		dev_err(&ts->client->dev, "I2C write error: wdata overflow reg: 0x%X len: %d\n", cmd[0], len);
	}
	
	memset(block_data, 0x00, sizeof(block_data));
	
	memcpy(&block_data[0], &cmd[0], sizeof(command_t));

	if(len)	
		memcpy(&block_data[sizeof(command_t)], &data[0], len);

	// busy check error
	if(touch_busy_check(ts))	return	-1;

	if ((ret = i2c_master_send(ts->client, block_data, (len + sizeof(command_t))))< 0) {
		dev_err(&ts->client->dev, "I2C write error: (%d) reg: 0x%X len: %d\n", ret, cmd[0], len);
		return ret;
	}
	
	return len;
}

//[*]--------------------------------------------------------------------------------------------------[*]
static int touch_enter_boot_mode(struct touch *ts)
{
	unsigned char	rdata;

	rdata = 0x00;	
	touch_wake_control(ts);
	if(touch_i2c_read(ts->client, REG_TS_UPDATE, sizeof(rdata), &rdata) < 0)		return	-1;
	if(rdata != REG_TS_UPDATE)														return	-1;		
		
	rdata = 0x00;	
	touch_wake_control(ts);
	if(touch_i2c_read(ts->client, REG_TS_UPDATE, sizeof(rdata), &rdata) < 0)		return	-1;
	if(rdata != TOUCH_BOOT_MODE)													return	-1;		

	mdelay(100);
		
	return	0;
}

//[*]--------------------------------------------------------------------------------------------------[*]
static int touch_enter_user_mode(struct touch *ts)
{
	if(touch_i2c_write(ts->client, REG_TS_USERMODE, 0, NULL) < 0)	return	-1;
	mdelay(100);

	touch_wake_control(ts);
	if(touch_get_mode(ts) != TOUCH_USER_MODE)						return	-1;

	return	0;
}

//[*]--------------------------------------------------------------------------------------------------[*]
static int touch_flash_bootmagic(struct touch *ts)
{
	unsigned char	rdata = 0;
	
	// bootmagic write
	touch_busy_check(ts);
	if(touch_i2c_write(ts->client, REG_TS_MAGIC_WR, 0, NULL) < 0)	return	-1;

	touch_busy_check(ts);
	if(touch_i2c_read(ts->client, REG_TS_MAGIC_RD, 1, &rdata) < 0)	return	-1;

	if(rdata != REG_TS_MAGIC_RD)									return	-1;

	return	0;
}

//[*]--------------------------------------------------------------------------------------------------[*]
int touch_flash_erase(struct touch *ts, unsigned int fw_size)
{

	#if 1
		unsigned int	offset = FLASH_USER_CODE_OFFSET;

		erase_cmd_t		cmd;
	
		cmd.reg 	= (CMD_FLASH_ERASE);
		cmd.addr	= (FLASH_START_ADDR + offset);
		cmd.len		= fw_size;
	#else
		command_t		cmd;
	
		cmd.reg 	= (0x20);
		cmd.addr	= (0x08001C00);
		cmd.len		= (fw_size / 1024) + 1;
	
	#endif

	if(touch_i2c_bootmode_write(ts, (unsigned char *)&cmd, 0, NULL) < 0)	return	-1;

	return	0;	
}

//[*]--------------------------------------------------------------------------------------------------[*]
int touch_flash_write(struct touch *ts, unsigned int fw_size, unsigned char *data)
{
	unsigned int	offset = 0, page_size, remainder;
	command_t		cmd;

	page_size 	= fw_size / I2C_SEND_MAX_SIZE;
	remainder	= fw_size % I2C_SEND_MAX_SIZE;

	while(page_size)	{
		cmd.reg 	= (CMD_FLASH_WRITE);
		cmd.addr	= (FLASH_START_ADDR + FLASH_USER_CODE_OFFSET + offset);
		cmd.len		= (I2C_SEND_MAX_SIZE);

		if(touch_i2c_bootmode_write(ts, (unsigned char *)&cmd, I2C_SEND_MAX_SIZE, &data[offset]) < 0)	return	-1;
		
		offset += I2C_SEND_MAX_SIZE;	page_size--;
	}

	if(remainder)	{
		cmd.reg 	= (CMD_FLASH_WRITE);
		cmd.addr	= (FLASH_START_ADDR + FLASH_USER_CODE_OFFSET + offset);
		cmd.len		= remainder;

		if(touch_i2c_bootmode_write(ts, (unsigned char *)&cmd, remainder, &data[offset]) < 0)	return	-1;
	}

	return	0;
}

//[*]--------------------------------------------------------------------------------------------------[*]
int touch_flash_verify(struct touch *ts, unsigned int fw_size, unsigned char *data)
{
	unsigned int	offset = 0, page_size, remainder;
	unsigned char	rdata[I2C_SEND_MAX_SIZE];
	command_t		cmd;
	int count=0, i;
	
	page_size 	= fw_size / I2C_SEND_MAX_SIZE;
	remainder	= fw_size % I2C_SEND_MAX_SIZE;

	while(page_size)	{
		cmd.reg 	= CMD_FLASH_READ;
		cmd.addr	= (FLASH_START_ADDR + FLASH_USER_CODE_OFFSET + offset);
		cmd.len		= (I2C_SEND_MAX_SIZE);
	
		if(touch_i2c_bootmode_read(ts, (unsigned char *)&cmd, I2C_SEND_MAX_SIZE, &rdata[0]) < 0)		return	-1;
#if 0
		for (i=0;i<I2C_SEND_MAX_SIZE;i++)
		{
			printk( "%d:%c_%c_",i,(unsigned char)(rdata[i]),(unsigned char)(*(data+offset+i)));
		}
#endif		
		if(memcmp(&rdata[0], &data[offset], I2C_SEND_MAX_SIZE) != 0)	{
			printk("%s error1: %d!\n", __func__,count);		

			for(i=0; i<I2C_SEND_MAX_SIZE; i++) {
				if( rdata[i] != data[offset+i] ) {
					printk ("rdata[%d] : %02x data[%d] : %02x\n", i, rdata[i], i, data[offset+i]);
				}
			}
			return	-1;
		}
		
		offset += I2C_SEND_MAX_SIZE;	page_size--;
		count++;
	}
	
	if(remainder)	{
		cmd.reg 	= CMD_FLASH_READ;
		cmd.addr	= (FLASH_START_ADDR + FLASH_USER_CODE_OFFSET + offset);
		cmd.len		= (I2C_SEND_MAX_SIZE);
	
		if(touch_i2c_bootmode_read(ts, (unsigned char *)&cmd, remainder, &rdata[0]) < 0)		return	-1;
		
		if(memcmp(&rdata[0], &data[offset], remainder) != 0)	{
			printk("%s error2!\n", __func__);		return	-1;
		}
	}

	if(touch_flash_bootmagic(ts) < 0)	{
		dev_err(&ts->client->dev, "bootmagic flash failed\n");		return	-1;
	}

	return 0;
}

//[*]--------------------------------------------------------------------------------------------------[*]
static int touch_flash_firmware(struct device *dev, unsigned int fw_size, unsigned char *data)
{
	struct touch 	*ts = dev_get_drvdata(dev);
	int 			ret;

	if ((ret = touch_set_mode(ts, TOUCH_BOOT_MODE)) < 0)	{
		dev_err(dev, "failed to enter boot mode!\n");		goto out_failed;
	}
	printk("Boot mode setup success!\n");

	if ((ret = touch_flash_erase(ts, fw_size)) < 0)	{
		dev_err(dev, "failed to send erase command!\n");	goto out_failed;
	}	
	printk("Flash erase success!\n");

	if ((ret = touch_flash_write(ts, fw_size, data)) < 0)	{
		dev_err(dev, "failed to flash!\n");		goto out_failed;
	}	
	printk("Flash write success!\n");

	if ((ret = touch_flash_verify(ts, fw_size, data)) < 0)	{
		dev_err(dev, "verify flash failed\n");	goto out_failed;
	}
	printk("Flash verify success!\n");

out_failed:

	if (ret < 0)	dev_err(dev, "Firmware update failed!\n");
	else			printk("Firmware update success!\n");

	if (touch_set_mode(ts, TOUCH_USER_MODE) < 0)	dev_err(dev, "Firmware mode exit failed!\n");
	
	mdelay(100);	// reboot delay

	return ret;
}

//[*]--------------------------------------------------------------------------------------------------[*]
int 	touch_default_fw(struct device *dev)
{
	struct touch 	*ts = dev_get_drvdata(dev);
	int 			ret;

	if(ts->fw_buf != NULL)		kfree(ts->fw_buf);

	ts->fw_buf 	= NULL;
	ts->fw_buf 	= (unsigned char *)kzalloc(MAX_FW_SIZE, GFP_KERNEL);

	if(!ts->fw_buf)	{
		dev_err(dev, "Firmware update failed! (No memory!)\n");	return	-1;
	}
	
	ts->fw_size = sizeof(lgd_default_firmware);
	
	if(ts->fw_size > MAX_FW_SIZE)	goto	error;
	printk("%s fw_size: %d!\n", __func__,ts->fw_size);	
	memcpy(ts->fw_buf, &lgd_default_firmware[0], ts->fw_size);

	ret = touch_flash_firmware(dev, ts->fw_size, ts->fw_buf);

error:	
	kfree(ts->fw_buf);
	
	ts->fw_buf = NULL;		ts->fw_size = 0;

	return ret;
}

//[*]--------------------------------------------------------------------------------------------------[*]
int 	touch_update_fw(struct device *dev, const char *fw_name)
{
	const struct firmware 	*fw;
	int 					ret;
	struct touch 			*ts = dev_get_drvdata(dev);

	if ((ret = request_firmware(&fw, fw_name, dev))) {
		dev_err(dev, "request_firmware() failed with %i\n", ret);		return ret;
	}

	if ((fw->size) > MAX_FW_SIZE) {
		dev_err(dev, "invalid firmware size (%d)\n", (int)fw->size);	
		ret = -EFAULT;
		goto out_release_fw;
	}

	touch_disable(ts);

	touch_wake_control(ts);

	if((ret = touch_flash_firmware(dev, fw->size, (unsigned char *)fw->data)) == 0)
		touch_information_display(ts);

	touch_enable(ts);
	
out_release_fw:
	release_firmware(fw);

	return ret;
}

//[*]--------------------------------------------------------------------------------------------------[*]
int 	touch_calibration(struct touch *ts)
{
	unsigned char	wdata = 0x00;
	int				ret;
	
	touch_disable(ts);

	touch_wake_control(ts);

	ret = touch_i2c_write(ts->client, REG_TS_CALIBRATION, sizeof(wdata), &wdata);

	touch_enable(ts);
		
	mdelay(1000);	// 1 sec delay
	
	return	ret;
}

//[*]--------------------------------------------------------------------------------------------------[*]

int 	touch_get_mode(struct touch *ts)
{
	unsigned char	rdata;
	
	touch_wake_control(ts);

	if(touch_i2c_read(ts->client, REG_TS_FW_STATUS, 1, &rdata) < 0)		return	-1;
	
	return	rdata;
}

//[*]--------------------------------------------------------------------------------------------------[*]
int 	touch_set_mode(struct touch *ts, unsigned char mode)
{
	unsigned char	get_mode;

	if((get_mode = touch_get_mode(ts)) < 0)		return	get_mode;

	if(mode == get_mode)			return	0;

	if(mode == TOUCH_BOOT_MODE)		return	(touch_enter_boot_mode(ts));
	else							return	(touch_enter_user_mode(ts));

}

//[*]--------------------------------------------------------------------------------------------------[*]
MODULE_AUTHOR("HardKernel Co., Ltd.");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Touchscreen Driver");

//[*]--------------------------------------------------------------------------------------------------[*]
//[*]--------------------------------------------------------------------------------------------------[*]
