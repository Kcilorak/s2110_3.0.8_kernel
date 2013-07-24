//[*]--------------------------------------------------------------------------------------------------[*]
//
//
// 
//  I2C Touchscreen driver
//  2012.01.17
// 
//
//[*]--------------------------------------------------------------------------------------------------[*]
#include <linux/input.h>	/* BUS_I2C */
#include <linux/i2c.h>
#include <linux/module.h>

//[*]--------------------------------------------------------------------------------------------------[*]
#include "touch.h"
#include "touch-i2c.h"

//[*]--------------------------------------------------------------------------------------------------[*]
//
// function prototype
//
//[*]--------------------------------------------------------------------------------------------------[*]
static 	void __exit		touch_i2c_exit		(void);
static 	int __init 		touch_i2c_init		(void);
static 	int __devexit 	touch_i2c_remove	(struct i2c_client *client);
static 	int __devinit 	touch_i2c_probe		(struct i2c_client *client, const struct i2c_device_id *id);
		int 			touch_i2c_write		(struct i2c_client *client, unsigned char reg, unsigned int len, unsigned char *data);
		int 			touch_i2c_read		(struct i2c_client *client, unsigned char reg, unsigned int len, unsigned char *data);
//[*]--------------------------------------------------------------------------------------------------[*]
#ifdef CONFIG_PM
static int 	touch_i2c_suspend(struct i2c_client *client, pm_message_t message)
{
	#ifndef CONFIG_HAS_EARLYSUSPEND
		touch_suspend(&client->dev);
	#endif

	return 0;
}

//[*]--------------------------------------------------------------------------------------------------[*]
static int 	touch_i2c_resume(struct i2c_client *client)
{
	#ifndef CONFIG_HAS_EARLYSUSPEND
		touch_resume(&client->dev);
	#endif	

	return 0;
}

//[*]--------------------------------------------------------------------------------------------------[*]
#else
	#define touch_i2c_suspend 	NULL
	#define touch_i2c_resume  	NULL
#endif

//[*]--------------------------------------------------------------------------------------------------[*]
int 	touch_i2c_read(struct i2c_client *client, unsigned char reg, unsigned int len, unsigned char *data)
{
#if defined(I2C_RESTART_USED)
	struct i2c_msg	msg[2];
#else
	struct i2c_msg	msg;
#endif	
	int 			ret;

	if((len == 0) || (data == NULL))	{
		dev_err(&client->dev, "I2C read error: Null pointer or length == 0\n");	
		return 	-1;
	}

#if defined(I2C_RESTART_USED)
	memset(msg, 0x00, sizeof(msg));

	msg[0].addr 	= client->addr;
	msg[0].flags 	= 0;
	msg[0].len 		= 1;
	msg[0].buf 		= &reg;

	msg[1].addr 	= client->addr;
	msg[1].flags    = I2C_M_RD;
	msg[1].len 		= len;
	msg[1].buf 		= data;
	
	if ((ret = i2c_transfer(client->adapter, msg, 2)) != 2) {
		dev_err(&client->dev, "I2C read error: (%d) reg: 0x%X len: %d\n", ret, reg, len);
		return -EIO;
	}
#else	
	memset(&msg, 0x00, sizeof(msg));

	msg.addr 	= client->addr;		msg.flags 	= 0;
	msg.len 	= 1;				msg.buf 	= &reg;

	if ((ret = i2c_transfer(client->adapter, &msg, 1)) != 1) {
		dev_err(&client->dev, "I2C read error: (%d) reg: 0x%X len: %d\n", ret, reg, len);
		return -EIO;
	}

	msg.addr 	= client->addr;		msg.flags   = I2C_M_RD;
	msg.len 	= len;				msg.buf 	= data;
	
	if ((ret = i2c_transfer(client->adapter, &msg, 1)) != 1) {
		dev_err(&client->dev, "I2C read error: (%d) reg: 0x%X len: %d\n", ret, reg, len);
		return -EIO;
	}
#endif

	return 	len;
}

//[*]--------------------------------------------------------------------------------------------------[*]
int 	touch_i2c_write(struct i2c_client *client, unsigned char reg, unsigned int len, unsigned char *data)
{
	int 			ret;
	unsigned char	block_data[10];

	if(len >= sizeof(block_data))	{
		dev_err(&client->dev, "I2C write error: wdata overflow reg: 0x%X len: %d\n", reg, len);
		return	-1;
	}

	memset(block_data, 0x00, sizeof(block_data));
	
	block_data[0] = reg;

	if(len)		memcpy(&block_data[1], &data[0], len);

	if ((ret = i2c_master_send(client, block_data, (len + 1)))< 0) {
		dev_err(&client->dev, "I2C write error: (%d) reg: 0x%X len: %d\n", ret, reg, len);
		return ret;
	}
	
	return len;
}

//[*]--------------------------------------------------------------------------------------------------[*]
static int __devinit 	touch_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	return	touch_probe(client);
}

//[*]--------------------------------------------------------------------------------------------------[*]
static int __devexit	touch_i2c_remove(struct i2c_client *client)
{
	return	touch_remove(&client->dev);
}

//[*]--------------------------------------------------------------------------------------------------[*]
static const struct i2c_device_id touch_id[] = {
	{ I2C_TOUCH_NAME, 0 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, touch_id);

//[*]--------------------------------------------------------------------------------------------------[*]
static struct i2c_driver touch_i2c_driver = {
	.driver = {
		.name	= I2C_TOUCH_NAME,
		.owner	= THIS_MODULE,
	},
	.probe		= touch_i2c_probe,
	.remove		= __devexit_p(touch_i2c_remove),
	.suspend	= touch_i2c_suspend,
	.resume		= touch_i2c_resume,
	.id_table	= touch_id,
};

//[*]--------------------------------------------------------------------------------------------------[*]
static int __init 	touch_i2c_init(void)
{
	return i2c_add_driver(&touch_i2c_driver);
}
module_init(touch_i2c_init);

//[*]--------------------------------------------------------------------------------------------------[*]
static void __exit 	touch_i2c_exit(void)
{
	i2c_del_driver(&touch_i2c_driver);
}
module_exit(touch_i2c_exit);

//[*]--------------------------------------------------------------------------------------------------[*]
MODULE_AUTHOR("HardKernel Co., Ltd.");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Touchscreen I2C bus driver");
MODULE_ALIAS("i2c:touch");

//[*]--------------------------------------------------------------------------------------------------[*]
//[*]--------------------------------------------------------------------------------------------------[*]
