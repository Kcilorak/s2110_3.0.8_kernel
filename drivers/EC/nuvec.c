/******************************************************************************
 * Nuvoton EC interface I2C Chip Driver
 *
 * Copyright (C) 2011, Nuvoton Technology Corporation
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see http://www.gnu.org/licenses/>.
 *
 * Contact information for Nuvoton: APC.Support@nuvoton.com
 *****************************************************************************/


#define DRV_VERSION "1.1.2"
#define DEBUGMSG    0  
#define KERNEL_2627   1
#define KERNEL_2625   0
#define USE_GPIO_IRQ  0

#include <linux/module.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/fcntl.h>
#include <linux/init.h>
#include <asm/uaccess.h>
#include <linux/timer.h>

#include <linux/i2c.h>
#include <linux/interrupt.h>
#if USE_GPIO_IRQ 
#include <asm/gpio.h>
#endif

#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/input.h>
#include <linux/nuvec.h>
#include <linux/nuvec_api.h>
#include <linux/platform_device.h>
#include <linux/sysfs.h>
#include <linux/earlysuspend.h>
#include <linux/delay.h>

#include <linux/ctype.h>
MODULE_AUTHOR("Pegatron Corp.");
MODULE_DESCRIPTION("Pegaton Corp. Embedded Controler I2C EC Driver");
MODULE_LICENSE("GPL");

#if defined(PEGA_SMT_BUILD)
#include "nuvec_kbmarco_smt.h"
#else
#include "nuvec_kbmarco_us.h"
#endif

#define NUVEC_MINOR 111
#define EC_BUFF_LEN 256
#define KBC_DATA    0x60

/* EC I2C address */
#define EC_SMB_ADDRESS  0x19

#define	ENOMEM		12	/* Out of Memory */
#define	EINVAL		22	/* Invalid argument */
#define ENOSPC		28	/* No space left on device */

#if KERNEL_2625 
static int ec_i2c_attach_adapter(struct i2c_adapter *adapter);
static int ec_i2c_remove(struct i2c_client *client);
#endif
static int ec_write_byte(u8 *write_data, int size);	
static int ec_read_byte(int cmd, uint8_t *buf);
static int i2c_read_byte(int cmd, uint8_t *buf, int size);
static int i2c_write_byte(u8 *write_data, int size);
static int input_device_create(void);
static int ec_state_change(int ec_sys_state);

/*-----------------------------------------------------------------------------
 * Global variables
 *---------------------------------------------------------------------------*/
/* General structure to hold the driver data */

static struct drv_data {
    struct i2c_client *I2cClient;
    struct delayed_work work;
    struct delayed_work det_work;
    struct delayed_work critical_work;
	/*for mouse*/
    char buf[5];
    unsigned int count; 
	/*for kbd*/
	unsigned int key_scan;
    unsigned int key_down;
    unsigned int escape;
    unsigned int pause_seq;  	
    struct EC_platform_data *pdata;
	struct early_suspend  ec_early_suspend;
} ec;
struct platform_device *ec_platform_dev;

#define Pad_Config_size	4
#define Dock_Data_size	4
#define Reserve_size		4
#define ISN_size	13
#define SSN_size 10
#define ERROR_CHECK		3
int 		error_times	=	0;
int		doing_ec_reset = 0;
int		ec_in_S3 = 0;
int		dock_connected_ok= 0;
int 		bulk_cmd_flag=	0;	//0402
int 		get_ac_cmd_flag = 0;	//0614
int		KBD_enable_flag = 1;

struct Flash_Data{
	u8 ec_command;
	u8 byte_count;
	u8 Pad_Config[Pad_Config_size];
	u8 Dock_Data[Dock_Data_size];
	u8 Reserve[Reserve_size];
	u8 ISN_Data[ISN_size];
} f_data;
	
static struct input_dev *nmouse_dev;
static struct input_dev *nkbd_dev;

/* I2C Addresses to scan */
#if KERNEL_2625 
static unsigned short normal_i2c[] = { EC_SMB_ADDRESS, I2C_CLIENT_END };
#endif
#if KERNEL_2627
static const unsigned short normal_i2c[] = {EC_SMB_ADDRESS, I2C_CLIENT_END};
#endif

#if KERNEL_2625 
static struct i2c_driver chip_driver = {
    .driver = {
        .name	= "nuvec",
    },
    .attach_adapter = ec_i2c_attach_adapter,
    .detach_client   = ec_i2c_remove,
};
#endif
static int _next_str(char **, char *);
static int _next_str(char **buf, char *ep)
{
	*buf = ep;
	while ((**buf != '\0') && (!isdigit(**buf))) {
		(*buf)++;
	}
	if (**buf == '\0') {
		return -1;
	}
	return 0;
}

int TP_status=1; //temp work around
#define TP_SCALING_BASE 10
int TP_scaling=18; //default scaling 1.8x, lv from 0-10
#define DSCALING TP_scaling/TP_SCALING_BASE
int tp_enable(int on)
{
	uint8_t tmpbuf[5] = {0};
	int ret = -1;
	int i = 0;
	uint8_t cmd = 0xff ;
	ECLOG_DBG("%s(%d)\n",__func__,on);
	if(on==1)		//enable TP
	{
		cmd = 0xf4;
		TP_status = 1;
	} else if (on==0){	//disable TP
		cmd = 0xf5;
		TP_status = 0;
	} else {
		ECLOG_ERR("%s(%d)-wrong parameter\n",__func__,on);
		return ret;
	}
	tmpbuf[0] = 0xd4;		//for EC spec,
	tmpbuf[1] = 0x03;	
	tmpbuf[2] = cmd;		//en TP or not
	tmpbuf[3] = 0x00;
	tmpbuf[4] = 0x00;
	while(i<ERROR_CHECK && error_times <=MAX_ERR_TIME && !doing_ec_reset)
	{
		ret = i2c_write_byte(tmpbuf, sizeof(tmpbuf));
		if(ret < 0)
		{
			EC_EI2CXF_MSG;
			i++;
		}
		else
		{
			return SUCCESS;
		}
	}
	return FAIL;
}

/*----------------------------------------------------------------------------
 * i2c_read_byte
 *---------------------------------------------------------------------------*/
 static int i2c_read_byte(int cmd, uint8_t *buf, int size)
//static int i2c_read_byte(int cmd, Read_Data buf, int size)
{
	int ret = 0;

	uint8_t read_cmd = cmd;
	struct i2c_msg msg[2];

	if(ec.pdata) //Avoid driver init timing issue
		goto I2C_RD_DRV_OK;
	else
		return ret;


I2C_RD_DRV_OK:
	if(!DKIN_DET(ec.pdata->docking_det_irq))
	{
		ECLOG_ERR("NotEXIST\n");
		return FAIL;
	}
	msg[0].addr = EC_SMB_ADDRESS;
    	msg[0].flags = 0;
       msg[0].len = 1;
    	msg[0].buf = (char *)(&read_cmd);	//0xb7
	
	msg[1].addr = EC_SMB_ADDRESS;
   	msg[1].flags = 1;
   	msg[1].len = size;
	msg[1].buf = buf;

	if(error_times >= MAX_ERR_TIME)
	{
		ECLOG_ERR("EI2CXF-Recovery fail(%d)! Aborting!\n",error_times);
		return FAIL;
	}

	if(doing_ec_reset)
	{
		ECLOG_INFO("UNDR-RST (err_time = %d)\n",error_times);
		return FAIL;
	}
	
	ret = i2c_transfer(ec.I2cClient->adapter, msg, 2);
	if( ret < 0 ) 
	{
		error_times++;
		if(error_times >= ERROR_CHECK && DKIN_DET(ec.pdata->docking_det_irq) && !doing_ec_reset)
		{
			doing_ec_reset = 1;
			dock_reset_func();
			doing_ec_reset = 0;
			schedule_delayed_work(&ec.det_work, 0);
			if(!gpio_get_value(ec.pdata->EC_irq))
				schedule_delayed_work(&ec.work, 0);
		}
		return ret;
	}
	error_times = 0;

	return SUCCESS;
}

/*----------------------------------------------------------------------------
 * i2c_write_byte
 *---------------------------------------------------------------------------*/
static int i2c_write_byte(u8 *write_data, int size)
{
	
	struct i2c_msg msg[1];
	int ret = 0;

	if(ec.pdata) //Avoid driver init timing issue
		goto I2C_WR_DRV_OK;
	else
		return ret;


I2C_WR_DRV_OK:
	if(!DKIN_DET(ec.pdata->docking_det_irq))
	{
		ECLOG_ERR("NotEXIST\n");
		return -1;
	}

	msg->addr = EC_SMB_ADDRESS;
	msg->flags = 0;			// 0 write
	msg->len = size;
	msg->buf = write_data;

	if(error_times >= MAX_ERR_TIME)
	{
		ECLOG_ERR("EI2CXF-Recovery fail(%d)! Aborting!\n",error_times);
		return FAIL;
	}


	if(doing_ec_reset)
	{
		ECLOG_INFO("UNDR-RST (docking_ec_reset = %d)\n",doing_ec_reset);
		return FAIL;
	}
	
	ret = i2c_transfer(ec.I2cClient->adapter, msg, 1);
	if( ret < 0 )
	{
		error_times++;
		if(error_times >= ERROR_CHECK && DKIN_DET(ec.pdata->docking_det_irq) && !doing_ec_reset)
		{
			doing_ec_reset = 1;
			dock_reset_func();
			doing_ec_reset = 0;
			schedule_delayed_work(&ec.det_work, 0);	
			if(!gpio_get_value(ec.pdata->EC_irq))
				schedule_delayed_work(&ec.work, 0);
		}
		return ret;
	}
	error_times = 0;
	return SUCCESS;
}

/*----------------------------------------------------------------------------
 * ec_event callback function
 *---------------------------------------------------------------------------*/
void (*ec_event)(int type) = NULL;
void register_ec_event_func( void (*pFn)(int ))
{
	if(ec_event)
	{
		ECLOG_ERR("ec_event function is already registered!\n");
		return ;
	}
	ec_event = pFn;
	ECLOG_INFO("ec_event register success!\n");
	if (DKIN_DET(ec.pdata->docking_det_irq))	
	{
		ECLOG_DBG("Dock_in, report the ec_event\n");
		ec_dock_in();
	}
}
EXPORT_SYMBOL(register_ec_event_func);
/*----------------------------------------------------------------------------
 *ec_boost_ctl		
 *---------------------------------------------------------------------------*/
 int ec_boost_ctl(int boost, int on_off)
{
	u8	CMD = 0;
	u8	BOOST_5V = 0;
	int err;
	u8 data[5];

	ECLOG_DBG("%s(%d,%d)\n",__func__,boost,on_off);
	if(boost == SYS_5V)
	{
		CMD = 0x00;
		if(on_off == BOOST_OFF)
		{
			BOOST_5V = 0x00;
		}else if(on_off == BOOST_ON){
			BOOST_5V = 0x01;
		}
	}else if(boost == OTG_5V){
		CMD = 0x01;
		if(on_off == BOOST_OFF)
		{
			BOOST_5V = 0x00;
		}else if(on_off == BOOST_ON){
			BOOST_5V = 0x04;
		}
	}

	data[0] = 0x09;	//for EC spec,
	data[1] = 0x03;	
	data[2] = CMD;		//sys_5V/otg_5v
	data[3] = BOOST_5V;	//5V boost on/off
	data[4] = 0x00;
	
	err = i2c_write_byte(data,sizeof(data));
	if(err < 0)
	{
		EC_EI2CXF_MSG;
		return FAIL;		
	}
	return SUCCESS;
}
EXPORT_SYMBOL(ec_boost_ctl);
/*--------------------------------------------------------------------------
 *get_boost_status	
 *---------------------------------------------------------------------------*/
 int get_boost_status(int *otg_status,int *sys_status)
{
    	#define MAX_READ_BYTE_COUNT	7
    	uint8_t tmpbuf[MAX_READ_BYTE_COUNT] = {0};
	uint8_t cmd = 0xd3;
	int err;
	err = i2c_read_byte(cmd, tmpbuf, sizeof(tmpbuf));
	if( err < 0 ){
		EC_EI2CXF_MSG;
		return FAIL;
	}

	if( (tmpbuf[2]&0x1) == 1 )	//SYS 5v
	{
		*sys_status = BOOST_ON;
	}else{
		*sys_status = BOOST_OFF;
	}

	if( ((tmpbuf[2]>>2)&0x1) ==  1)	//OTG 5v
	{
		*otg_status = BOOST_ON;
	}else{
		*otg_status = BOOST_OFF;
	}

	ECLOG_DBG("%s(sys-%d,otg-%d)\n",__func__,*sys_status,*otg_status);
	
	return SUCCESS;
}
EXPORT_SYMBOL(get_boost_status);
/*----------------------------------------------------------------------------
 *usb _in_used
 *---------------------------------------------------------------------------*/
 
 int usb_in_used(int num)
{
	int err;
	u8 data[5];
	u8 num_of_usb;
	ECLOG_DBG("%s(%d)\n",__func__,num);
	if(num >= 2)
	{
		num_of_usb = 0x02;
	}else if(num == 1){
		num_of_usb = 0x01;
	}else{
		num_of_usb = 0x00;
	}
	data[0] = 0x4e;	//for EC spec,
	data[1] = 0x03;	
	data[2] = 0xd2;
	data[3] = num_of_usb;//number of USB_devices;
	data[4] = 0x00;
	err = i2c_write_byte(data, sizeof(data));
	if( err < 0 ){
		EC_EI2CXF_MSG;
		return FAIL;
	}	
	return SUCCESS;
}
EXPORT_SYMBOL(usb_in_used);
/*-----------------------------------------------------------------------------
 * get_usb_in_use
 *-----------------------------------------------------------------------------*/

int get_usb_in_used(int *count)
{
    	#define MAX_READ_BYTE_COUNT	7
    	uint8_t tmpbuf[MAX_READ_BYTE_COUNT] = {0};
	uint8_t cmd = 0xd2; //Read command for USE_Device_in_use_number stored in EC
	int err;
	err = i2c_read_byte(cmd, tmpbuf, sizeof(tmpbuf));

	if( err < 0 ){
		EC_EI2CXF_MSG;
		return FAIL;
	}

	*count = tmpbuf[2]; 

	ECLOG_DBG("%s(%d)\n",__func__,*count);
	
	return SUCCESS;
}
EXPORT_SYMBOL(get_usb_in_used);
/*-----------------------------------------------------------------------------
 *get_ec_bat_info	
 *----------------------------------------------------------------------------*/
 int get_ec_bat_info(int *info, int cmd)
{
    	#define MAX_READ_BYTE_COUNT	7
    	uint8_t tmpbuf[MAX_READ_BYTE_COUNT] = {0};
    	int err;
	
	err = i2c_read_byte(cmd, tmpbuf, sizeof(tmpbuf));
	if( err < 0 ){
		EC_EI2CXF_MSG;
		return FAIL;
	}

	*info = (tmpbuf[3]<<8)+tmpbuf[2];

	ECLOG_DBG("%s(%d,%d)\n",__func__,*info,cmd);	

	return SUCCESS;
}
EXPORT_SYMBOL(get_ec_bat_info);

/*----------------------------------------------------------------------------
 *ec_charging_req
 *----------------------------------------------------------------------------*/
 int ec_charging_req(int on_off)
{

	int err;
	u8 data[5];
	u8	power_supply = 0;
	ECLOG_DBG("%s(%d)\n",__func__,on_off);
	if(on_off == CHARGING_OFF)
	{
		power_supply = 0x00;
	}else if(on_off == CHARGING_ON)
	{
		power_supply = 0x01;
	}else if (on_off == 0x02)
	{
		power_supply = 0x02;
	}
	data[0] = 0x09;	//for EC spec,
	data[1] = 0x03;	
	data[2] = 0x02;
	data[3] = power_supply;//number of USB_devices;
	data[4] = 0x00;
	err = i2c_write_byte(data, sizeof(data));
	if( err < 0 ){
		EC_EI2CXF_MSG;
		return FAIL;
	}		
	
	return SUCCESS;
}
EXPORT_SYMBOL(ec_charging_req);
/*----------------------------------------------------------------------------
 *get_ec_charging_req
 *----------------------------------------------------------------------------*/
 int get_ec_charging_req_status(int *charging_req_status)
{

    	#define MAX_READ_BYTE_COUNT	7
    	uint8_t tmpbuf[MAX_READ_BYTE_COUNT] = {0};
	uint8_t cmd = 0xd4;
	int err;
	err = i2c_read_byte(cmd, tmpbuf, sizeof(tmpbuf));

	if( err < 0 ){
		EC_EI2CXF_MSG;
		return FAIL;
	}
	
	if( (tmpbuf[2]&0x01) == 1 )		//ec charging
	{
		*charging_req_status = CHARGING_ON;
	}else if ((tmpbuf[2]&0x01) == 0){
		*charging_req_status = CHARGING_OFF;
	}else if (tmpbuf[2] == 2)
	{
		*charging_req_status = 2;
	}

	ECLOG_DBG("%s(%d)\n",__func__,*charging_req_status);
	
	return SUCCESS;
}
EXPORT_SYMBOL(get_ec_charging_req_status);
/*----------------------------------------------------------------------------
 *get_ac_type	
 *----------------------------------------------------------------------------*/
int get_ac_type(int *type)
{
	#define MAX_READ_BYTE_COUNT	7
	uint8_t cmd;
	uint8_t tmpbuf[MAX_READ_BYTE_COUNT] = {0};
	int err;
	int input_curr =0;
	if(get_ac_cmd_flag == 1)
	{
		cmd = 0xd9;
		err = i2c_read_byte(cmd, tmpbuf, sizeof(tmpbuf));
		if(err < 0){
			EC_EI2CXF_MSG;
			return FAIL;
		}
		switch(tmpbuf[2])
		{
			case	AC_DATA_INFORMAL:
				*type = AC_INFORMAL;
				break;
			case	AC_DATA_5V:
				*type = AC_5V;
				break;
			case	AC_DATA_USB:
				*type = AC_USB;
				break;
			case	AC_DATA_NONE:
				*type = AC_NONE;
				break;
			case	AC_DATA_12V:
				*type = AC_12V;
				break;
		}
	}
	else
	{ 
		cmd = 0xb7;
		err = i2c_read_byte(cmd, tmpbuf, sizeof(tmpbuf));
		if( err < 0 ){
			EC_EI2CXF_MSG;
			return FAIL;
		}
		if(tmpbuf[2]&0x2)
		{
			*type = AC_NONE;
		} else 
		{
			cmd = 0xb9;
			err = i2c_read_byte(cmd, tmpbuf, sizeof(tmpbuf));
	
			if( err < 0 ){
				EC_EI2CXF_MSG;
				return FAIL;
			}

			if(!(tmpbuf[2] & 0x2))
			{
				*type = AC_12V;
			} else 
			{
				if (tmpbuf[2]&0x8)
				{
					*type = AC_5V;
				} else 
				{
					*type = AC_USB;
				}
			}
		}
		if(*type == AC_5V)
		{
			get_ec_charging_info(&input_curr, 0x54);
			if(input_curr <= 1024 && input_curr >= 384 )
				*type = AC_INFORMAL;
		}
	}
	
	ECLOG_DBG("%s(%d)\n",__func__,*type);
	return SUCCESS;
	
}
EXPORT_SYMBOL(get_ac_type);
/*----------------------------------------------------------------------------
 *set_usb_bypass_en
 *--------------------------------------------------------------------------*/
 int set_usb_bypass_en(int high_low)
{
	u8	usb_bypass = 0;
	int err;
	u8 data[5];
	ECLOG_DBG("%s(%d)\n",__func__,high_low);
	if(high_low == usb_bypass_high)
	{
		usb_bypass = 0x02;
	}else{
		usb_bypass = 0x00;
	}
	data[0] = 0x20;	//for EC spec,
	data[1] = 0x03;	
	data[2] = 0x00;		//sys_5V/otg_5v
	data[3] = usb_bypass;	//5V boost on/off
	data[4] = 0x00;
	err = i2c_write_byte(data, sizeof(data));

	if( err < 0 ){
		EC_EI2CXF_MSG;
		return FAIL;
	}		
	
	return SUCCESS;
}
EXPORT_SYMBOL(set_usb_bypass_en);
/*-----------------------------------------------------------------------------
 *get_usb_bypass_en
 *---------------------------------------------------------------------------*/
 int get_usb_bypass_en(int *usb_bypass_status)
{
    	#define MAX_READ_BYTE_COUNT	7
    	uint8_t tmpbuf[MAX_READ_BYTE_COUNT] = {0};
	uint8_t cmd = 0xd3;
	int err;
	err  = i2c_read_byte(cmd, tmpbuf, sizeof(tmpbuf));

	if( err < 0 ){
		EC_EI2CXF_MSG;
		return FAIL;
	}

	if( ((tmpbuf[3]>>1)&0x1) == 1 )
	{
		*usb_bypass_status = usb_bypass_high;
	}else{
		*usb_bypass_status = usb_bypass_low;
	}

	ECLOG_DBG("%s(%d)\n",__func__,*usb_bypass_status);

	return SUCCESS;
}
EXPORT_SYMBOL(get_usb_bypass_en);

int usb_hub_reset(int time)
{
	int err;
	u8 data[5];
	
	data[0] = 0x20;	//for EC spec,
	data[1] = 0x03;	
	data[2] = 0xff;		//reset
	data[3] = time & 0xff;           //time low_byte
	data[4] = (time >> 8) & 0xff;//time high_byte
	err =i2c_write_byte(data, sizeof(data));
	ECLOG_DBG("%s(%d)\n",__func__,time);
	if( err < 0 ){
		EC_EI2CXF_MSG;
		return FAIL;
	}		
	
	return SUCCESS;
}
EXPORT_SYMBOL(usb_hub_reset);
/*-----------------------------------------------------------------------------
 * dock_status
 *---------------------------------------------------------------------------*/	
int dock_status(int *status)
{
    	uint8_t tmpbuf[4] = {0};
	uint8_t cmd;
	int err;
	*status = 0;

	if( !DKIN_DET((ec.pdata->docking_det_irq)) || !dock_connected_ok)
	{
		*status = NO_DOCK;
		ECLOG_DBG("%s(%d)\n",__func__,*status);
		return FAIL;
	}
	cmd = 0xb0;
	err  = i2c_read_byte(cmd, tmpbuf, sizeof(tmpbuf));

	if( err < 0 ) {
		EC_EI2CXF_MSG;
		*status = DOCK_I2C_FAIL;
		return SUCCESS;
	}

	if( !(( tmpbuf[2] >> 3 ) & 0x1) )
		*status = ( *status ) | PADIN_ERR;	

	cmd = 0xd2;
	err  = i2c_read_byte(cmd, tmpbuf, sizeof(tmpbuf));	
	if( err < 0 ) {
		EC_EI2CXF_MSG;
		*status = DOCK_I2C_FAIL;
		return SUCCESS;
	}

	*status = ( *status ) | (tmpbuf[3] & (TOUCHPAD_ERR|BATTERY_ERR));

	ECLOG_DBG("%s(%d)\n",__func__,*status);
	
	return SUCCESS;
}
EXPORT_SYMBOL(dock_status);
/*----------------------------------------------------------------------------
 * get_wakeup_flag
 *----------------------------------------------------------------------------*/
int get_wakeup_flag(int *flag)
{
    	uint8_t tmpbuf[4] = {0};
	uint8_t cmd = 0x05;
	int err;
	err = i2c_read_byte(cmd, tmpbuf, sizeof(tmpbuf));
	if(err < 0)
	{
		EC_EI2CXF_MSG;
		return FAIL;		
	}
	*flag = tmpbuf[2] & WAKEUP_FLAG_MASK;

	ECLOG_DBG("%s(%d)\n",__func__,*flag);
	
	return SUCCESS;
}
EXPORT_SYMBOL(get_wakeup_flag);
/*----------------------------------------------------------------------------
 * get_wakeup_interval_time
 *----------------------------------------------------------------------------*/
int get_wakeup_sleep_interval(int *time)
{
    	uint8_t tmpbuf[4] = {0};
	uint8_t cmd = 0x04;
	int err;
	err = i2c_read_byte(cmd, tmpbuf, sizeof(tmpbuf));

	if(err < 0)
	{
		EC_EI2CXF_MSG;
		return FAIL;	
	}

	*time = 	tmpbuf[2] + (tmpbuf[3] << 8);

	ECLOG_DBG("%s(%d)\n",__func__,*time);
	
	return SUCCESS;
}
EXPORT_SYMBOL(get_wakeup_sleep_interval);
/*----------------------------------------------------------------------------
 * bulk_battery_data
 *----------------------------------------------------------------------------*/
int get_ec_battery_data(struct bat_data *ec_bat )
{
    	uint8_t tmpbuf[16] = {0};
	uint8_t cmd = 0x3f;
	int err;
	if(bulk_cmd_flag == 1)	//support bulk read CLI
	{
		err = i2c_read_byte(cmd, tmpbuf, sizeof(tmpbuf));
		if(err < 0)
		{
			EC_EI2CXF_MSG;
			return FAIL;
		} else 
		{
			ec_bat->voltage = tmpbuf[2] + (tmpbuf[3] << 8);
			ec_bat->relative_state_of_charge = tmpbuf[4] + (tmpbuf[5] << 8);
			ec_bat->tempture = tmpbuf[6] + (tmpbuf[7] << 8);
			ec_bat->flag = tmpbuf[8] + (tmpbuf[9] << 8);
			ec_bat->remain_capacity = tmpbuf[10] + (tmpbuf[11] << 8);
			ec_bat->full_charge_capacity = tmpbuf[12] + (tmpbuf[13]<<8);
			ec_bat->average_current = tmpbuf[14] + (tmpbuf[15] <<8);
		}
	}else
	{
			err = i2c_read_byte(0x08, tmpbuf, sizeof(tmpbuf));
			ec_bat->voltage =  tmpbuf[2] + (tmpbuf[3] << 8);
			if(err< 0)
				return FAIL;
			err = i2c_read_byte(0x2c, tmpbuf, sizeof(tmpbuf));
			ec_bat->relative_state_of_charge =  tmpbuf[2] + (tmpbuf[3] << 8);
			if(err< 0)	
				return FAIL;	
			err = i2c_read_byte(0x06, tmpbuf, sizeof(tmpbuf));
			ec_bat->tempture=  tmpbuf[2] + (tmpbuf[3] << 8);
			if(err< 0)
				return FAIL;		
			err = i2c_read_byte(0x0a, tmpbuf, sizeof(tmpbuf));
			ec_bat->flag=  tmpbuf[2] + (tmpbuf[3] << 8);
			if(err< 0)
				return FAIL;		
			err = i2c_read_byte(0x10, tmpbuf, sizeof(tmpbuf));
			ec_bat->remain_capacity=  tmpbuf[2] + (tmpbuf[3] << 8);
			if(err< 0)
				return FAIL;			
			err = i2c_read_byte(0x12, tmpbuf, sizeof(tmpbuf));
			ec_bat->full_charge_capacity=  tmpbuf[2] + (tmpbuf[3] << 8);
			if(err< 0)
				return FAIL;		
			err = i2c_read_byte(0x14, tmpbuf, sizeof(tmpbuf));
			ec_bat->average_current=  tmpbuf[2] + (tmpbuf[3] << 8);
			if(err< 0)
				return FAIL;
		}

	ECLOG_DBG("%s(vol:%d, rsoc:%d, temp:%d, flag:%d, rm:%d, fcc:%d, avg_current:%d)\n",__func__,
		ec_bat->voltage, ec_bat->relative_state_of_charge, ec_bat->tempture, ec_bat->flag, ec_bat->remain_capacity,
		ec_bat->full_charge_capacity,ec_bat->average_current);
	
	return SUCCESS;
}
EXPORT_SYMBOL(get_ec_battery_data);
/*----------------------------------------------------------------------------
 * ec_charging_setting
 *----------------------------------------------------------------------------*/
 int ec_charging_setting(int input_current, int chg_current)
{
	#define	MASK_L_BYTE	0x03
	uint8_t input_data[5] = {0};
	uint8_t chg_data[5] = {0};
	uint8_t req_data[5] = {0};

	ECLOG_DBG("%s(%d,%d)\n",__func__,input_current, chg_current);
	input_data[0] = 0x09;
	input_data[1] = 0x03;
	input_data[2] = 0x54;
	input_data[3] = 0x00;
	input_data[4] = 0x00;

	chg_data[0] = 0x09;
	chg_data[1] = 0x03;
	chg_data[2] = 0x50;
	chg_data[3] = 0x00;
	chg_data[4] = 0x00;

	req_data[0] = 0x09;
	req_data[1] = 0x03;
	req_data[2] = 0x02;
	req_data[3] = 0x00;		//set ec_charging_req 0
	req_data[4] = 0x00;

	if(input_current == chg_current)
	{
		switch(input_current)
		{
			case EC_DEFCURR_5V:
			case EC_DEFCURR_12V:
			case EC_DEFCURR_USB:
				req_data[3] = CHARGING_ON; //use EC default value
				if( i2c_write_byte(req_data,sizeof(req_data)) < 0 )
				{
					EC_EI2CXF_MSG;
					return FAIL;
				}
				if( i2c_write_byte(input_data, sizeof(input_data)) < 0 )
				{
					EC_EI2CXF_MSG;
					return FAIL;
				}
				if( i2c_write_byte(chg_data, sizeof(chg_data)) < 0 )
				{
					EC_EI2CXF_MSG;
					return FAIL;
				}
				return SUCCESS;
				break;
		}
	}
	else if(input_current < chg_current)
	{
		ECLOG_ERR("%s:error current setting (input(%d) < chg(%d))! \n",__func__,input_current, chg_current);
		return INVALID_VALUE;
	}

	switch(input_current)
	{
		case EC_CURR_3840:
		case EC_CURR_2048:
		case EC_CURR_1536:
		case EC_CURR_1280:
		case EC_CURR_1024:
		case EC_CURR_512:
		case EC_CURR_384:
		case EC_CURR_128:
			input_data[3] = (input_current & MASK_L_BYTE);
			input_data[4] = (input_current >> 8);
			break;
		case EC_CURR_0:
			input_data[3] = EC_CURR_0;
			input_data[4] = EC_CURR_0;
			chg_data[3] = EC_CURR_0;
			chg_data[4] = EC_CURR_0;
			req_data[3] = CHARGING_OFF;
			break;
		default:
			ECLOG_ERR("%s:error input current setting(%d) \n",__func__,input_current);
			return INVALID_VALUE;
			break;
	}

	switch(chg_current)
	{
		case EC_CURR_3840:
		case EC_CURR_2048:
		case EC_CURR_1536:
		case EC_CURR_1280:
		case EC_CURR_1024:
		case EC_CURR_512:
		case EC_CURR_384:
		case EC_CURR_128:
			chg_data[3] = (chg_current & MASK_L_BYTE);
			chg_data[4] = (chg_current >> 8);
			req_data[3] = CHARGING_ON;
			break;
		case EC_CURR_0:
			chg_data[3] = EC_CURR_0;
			chg_data[4] = EC_CURR_0;
			req_data[3] = CHARGING_OFF;
			break;
		default:
			ECLOG_ERR("%s:error charging current setting(%d) \n",__func__,chg_current);
			return INVALID_VALUE;
			break;
	}

	if( i2c_write_byte(req_data, sizeof(req_data)) < 0 )
	{
		EC_EI2CXF_MSG;
		return FAIL;
	}
	if( i2c_write_byte(input_data, sizeof(input_data)) < 0 )
	{
		EC_EI2CXF_MSG;
		return FAIL;
	}
	if( i2c_write_byte(chg_data, sizeof(chg_data)) < 0 )
	{
		EC_EI2CXF_MSG;
		return FAIL;
	}
	return SUCCESS;
}
EXPORT_SYMBOL(ec_charging_setting);
/*-----------------------------------------------------------------------------
 * dock_isn_read
 *---------------------------------------------------------------------------*/
//int dock_isn_read(u8* isn)
char dock_isn_read(char* isn)
{
	 u8 cmd = 0xe0;
	if( i2c_read_byte(cmd, (uint8_t*) (&f_data.byte_count), sizeof(f_data)) < 0)
   	{
		EC_EI2CXF_MSG;
		return FAIL;
   	}

	memcpy(isn, f_data.ISN_Data, ISN_size);
	ECLOG_DBG("%s(%s)\n",__func__,isn);
   return SUCCESS;

}
EXPORT_SYMBOL(dock_isn_read);
/*-----------------------------------------------------------------------------
 * dock_isn_write
 *---------------------------------------------------------------------------*/
int dock_isn_write(char* isn)
{
	u8 cmd = 0xe0;
	u8 init_data[5];
	int i;
	ECLOG_DBG("%s(%s)\n",__func__,isn);
	if( i2c_read_byte(cmd, (uint8_t*) (&f_data.byte_count), sizeof(f_data)) < 0)
   	{
		EC_EI2CXF_MSG;
		return FAIL;
   	}

	for(i=0 ; i<sizeof(f_data.Pad_Config) ;i++)
		ECLOG_DBG("Pad_Config[%d]:0x%x\n",i,f_data.Pad_Config[i]);
	for(i=0 ; i<sizeof(f_data.Dock_Data) ;i++)
		ECLOG_DBG("Dock_Data[%d]:0x%x\n",i,f_data.Dock_Data[i]);
	for(i=0 ; i<sizeof(f_data.Reserve) ;i++)
		ECLOG_DBG("Reserve[%d]:0x%x\n",i,f_data.Reserve[i]);
	
	for(i=0 ; i<sizeof(f_data.ISN_Data) ;i++)
		ECLOG_DBG("ISN_Data[%d]:0x%x\n",i,f_data.ISN_Data[i]);
	
	memcpy(f_data.ISN_Data, isn, ISN_size);
		

	init_data[0] = 0x2f;
	init_data[1] = 0x03;
	init_data[2] = 0xf0;
	init_data[3] = 0x55;
	init_data[4] = 0xaa;
	if(i2c_write_byte(init_data, sizeof(init_data)) < 0)
	{
		EC_EI2CXF_MSG;
		return FAIL;
	}

	f_data.ec_command = 0xe0;
	f_data.byte_count = 0x20;		//byte count
	if(i2c_write_byte((u8*)&f_data, sizeof(f_data)) < 0)
	{
		EC_EI2CXF_MSG;
		return FAIL;
	}
	
	return SUCCESS;
}
EXPORT_SYMBOL(dock_isn_write);
/*-----------------------------------------------------------------------------
 * dock_ssn_read (from 56k)
 *---------------------------------------------------------------------------*/
 char dock_ssn_read(char* ssn)
{
       u8 cmd = 0xe1;
       int ret = 0;
       uint8_t tmpbuf[32] = {0};
       

       ret = i2c_read_byte(cmd, tmpbuf, sizeof(tmpbuf));
       if( ret < 0)
       {
               EC_EI2CXF_MSG;
               return FAIL;            
       }
       memcpy(ssn, tmpbuf+1, SSN_size);
       
//       printk("%s(%s)\n",__func__,ssn);
       return SUCCESS;

}

EXPORT_SYMBOL(dock_ssn_read);
/*-----------------------------------------------------------------------------
 * dock_ssn_write (from 56k)
 *---------------------------------------------------------------------------*/
int dock_ssn_write(char* ssn)
{
       uint8_t ssn_data[34] = {0};
       u8 init_data[5];
       int ret = 0;
       init_data[0] = 0x2f;
       init_data[1] = 0x03;
       init_data[2] = 0xf2;
       init_data[3] = 0x55;
       init_data[4] = 0xaa;

       ret = i2c_write_byte(init_data, sizeof(init_data));
       if(ret  < 0)
       {
               EC_EI2CXF_MSG;
               return FAIL;
       }
       ssn_data[0] = 0x4f;
       ssn_data[1] = 0x20;
       
       memcpy(ssn_data+2, ssn, SSN_size);
       ret = i2c_write_byte(ssn_data, sizeof(ssn_data));
       if(ret < 0)
       {
            	 EC_EI2CXF_MSG;
               return FAIL;            
       }
       return SUCCESS;
       
}
EXPORT_SYMBOL(dock_ssn_write);
/*-----------------------------------------------------------------------------
 * read_battery_manufacture command
 *---------------------------------------------------------------------------*/
 int battery_manufacture_info_read(u8 manu_info[])
{
	u8 cmd = 0x3e;
	u8 ret = 0;
	u8 tmpbuf[5] = {0};
	ret = i2c_read_byte(cmd, tmpbuf, sizeof(tmpbuf));
	if(ret < 0)
	{
		EC_EI2CXF_MSG;
		return FAIL;
	}

	memcpy(manu_info, tmpbuf, sizeof(tmpbuf));

	return SUCCESS;
}
EXPORT_SYMBOL(battery_manufacture_info_read);
/*-----------------------------------------------------------------------------
 * dock_data_read
 *---------------------------------------------------------------------------*/
int dock_data_read(int *dock_data)
{
	 u8 cmd = 0xe0;
	if( i2c_read_byte(cmd, (uint8_t*) (&f_data.byte_count), sizeof(f_data)) < 0)
   	{
		EC_EI2CXF_MSG;
		return FAIL;
   	}

	*dock_data = f_data.Dock_Data[0] + (f_data.Dock_Data[1] << 8) 
		+ (f_data.Dock_Data[2] << 16) + (f_data.Dock_Data[3] << 24);
	
	ECLOG_DBG("%s(0x%x)\n",__func__,*dock_data);
	return SUCCESS;
}
EXPORT_SYMBOL(dock_data_read);
/*-----------------------------------------------------------------------------
 * dock_data_write
 *---------------------------------------------------------------------------*/
int dock_data_write(int dock_data)
{
	u8 cmd = 0xe0;
	u8 init_data[5];
	ECLOG_DBG("%s(%x)\n",__func__,dock_data);
	if( i2c_read_byte(cmd, (uint8_t*) (&f_data.byte_count), sizeof(f_data)) < 0)
   	{
		EC_EI2CXF_MSG;
		return FAIL;
   	}

	f_data.Dock_Data[0] = dock_data & 0xff;
	f_data.Dock_Data[1] = (dock_data >> 8) & 0xff;
	f_data.Dock_Data[2] = (dock_data >> 16) & (0xff);
	f_data.Dock_Data[3] = (dock_data >> 24)& (0xff);
		

	init_data[0] = 0x2f;
	init_data[1] = 0x03;
	init_data[2] = 0xf0;
	init_data[3] = 0x55;
	init_data[4] = 0xaa;
	if(i2c_write_byte(init_data, sizeof(init_data)) < 0)
	{
		EC_EI2CXF_MSG;
		return FAIL;
	}

	f_data.ec_command = 0xe0;
	f_data.byte_count = 0x20;		//byte count
	if(i2c_write_byte((u8*)&f_data, sizeof(f_data)) < 0)
	{
		EC_EI2CXF_MSG;
		return FAIL;
	}
	
	return SUCCESS;
}
EXPORT_SYMBOL(dock_data_write);

/*-----------------------------------------------------------------------------
 * dock_pad_config_read
 *---------------------------------------------------------------------------*/
int dock_pad_config_read(int *pad_config)
{
	 u8 cmd = 0xe0;
	if( i2c_read_byte(cmd, (uint8_t*) (&f_data.byte_count), sizeof(f_data)) < 0)
   	{
		EC_EI2CXF_MSG;
		return FAIL;
   	}

	*pad_config = f_data.Pad_Config[0] + (f_data.Pad_Config[1] << 8) 
		+ (f_data.Pad_Config[2] << 16) + (f_data.Pad_Config[3] << 24);

	ECLOG_DBG("%s(0x%x)\n",__func__,*pad_config);
	return SUCCESS;
}
EXPORT_SYMBOL(dock_pad_config_read);
/*-----------------------------------------------------------------------------
 * dock_pad_config_write
 *---------------------------------------------------------------------------*/
int dock_pad_config_write(int pad_config)
{
	u8 cmd = 0xe0;
	u8 init_data[5];

	ECLOG_DBG("pad_config:0x%x\n",pad_config);
	if( i2c_read_byte(cmd, (uint8_t*) (&f_data.byte_count), sizeof(f_data)) < 0)
   	{
		EC_EI2CXF_MSG;
		return FAIL;
   	}

	f_data.Pad_Config[0] = pad_config & 0xff;
	f_data.Pad_Config[1] = (pad_config >> 8) & 0xff;
	f_data.Pad_Config[2] = (pad_config >> 16) & (0xff);
	f_data.Pad_Config[3] = (pad_config >> 24)& (0xff);
		

	init_data[0] = 0x2f;
	init_data[1] = 0x03;
	init_data[2] = 0xf0;
	init_data[3] = 0x55;
	init_data[4] = 0xaa;
	if(i2c_write_byte(init_data, sizeof(init_data)) < 0)
	{
		EC_EI2CXF_MSG;
		return FAIL;
	}

	f_data.ec_command = 0xe0;
	f_data.byte_count = 0x20;		//byte count
	if(i2c_write_byte((u8*)&f_data, sizeof(f_data)) < 0)
	{
		EC_EI2CXF_MSG;
		return FAIL;
	}
	
	return SUCCESS;
}
EXPORT_SYMBOL(dock_pad_config_write);
#if 1
/*-----------------------------------------------------------------------------
 * dock_SD_card_status_read
 *---------------------------------------------------------------------------*/
 int get_dock_SD_card_status(int *status)
{	

    	uint8_t tmpbuf[4] = {0};
	uint8_t cmd = 0xd7;
	int err;
	err = i2c_read_byte(cmd, tmpbuf, sizeof(tmpbuf));
	if(err < 0)
	{
		EC_EI2CXF_MSG;
		return FAIL;		
	}
	*status = tmpbuf[2] & 0x1;

	ECLOG_DBG("%s(%d)\n",__func__,*status);
	return SUCCESS;
}
#endif
/*-----------------------------------------------------------------------------
 * ec_interrupt
 *---------------------------------------------------------------------------*/
 #define report_event(type,str) if(ec_event) \
                                                    (*ec_event)(type); \
                                                   ECLOG_INFO("EVENT-%d(%s)\n",type,str);
#define dump_ec_event(buf) ECLOG_INFO(" EVENT 0x%x, 0x%x\n",buf[1],buf[2]);
void ec_events(__u8 *buf)
{
	if(buf[1] == 0x00 && buf[2] == 0x00)
	{
		if(buf[0] == 0xfe)
			ECLOG_INFO("FW update ERROR!!\n");
		return;
	}
    switch(buf[1])
    {
        case 0x4e:
            if(buf[2]==0x02 ) //DEV_STATUS ERROR
            {
                if(buf[3]&0x1) //TP_ERR
                {
                    report_event(TP_ERR,"TP_ERR");
                }
                if(buf[3] & (0x1<<1)) //BAT_ERR
                {
                    report_event(BAT_ERR,"BAT_ERR");
                }    
                else
                {
                    dump_ec_event(buf);
                }
            }
		else if(buf[2] == 0x03)	//wakeup timer time's up
		{
			report_event(DOCK_WKUP,"DOCK_WAKEUP_PAD");
		}
		else if(buf[2] == 0x05)
		{
			report_event(DOCK_KBD_WKUP, "DOCK_KBD_WAKEUP_PAD");
		}
		else if(buf[2] == 0x04)
		{
			report_event(EC_RESET,"DOCK_EC_RESET");
		}
		else {
			dump_ec_event(buf);
		}
            break;
        case 0x30:
            if(buf[2]==0x0) //SD_DET
            {
                if(buf[3] & 0x1)
                {
                    report_event(SD_REMOVE,"SD_REMOVE");
                }
                else
                {
                    report_event(SD_INSERT,"SD_INSERT");
                 }
            }
            else
            {
                dump_ec_event(buf);
            }
            break;
        case 0x09:
	     if(buf[2] == 0x02 && get_ac_cmd_flag == 1)
	     {
	     		report_event(INFORMAL_CHG,"Informal");
			break;
	     }

			
            if(buf[2] == 0x00)
            {
		  if( buf[3] & (0x1<<3))
		  {
			report_event(INFORMAL_AC_IN,"Informal_AC_IN");
			break;
		  }
			
                if(buf[3] & 0x1)    //EC_BALE_DET =1 --> AC_OUT
                {
                    report_event(AC_OUT,"AC_OUT");
                }
                else if(!(buf[3] & (0x1<<1)))  //DEV_ID=0 --> AC_12V
                {
                    report_event(AC_12V_IN,"AC_12V_IN");
                }
                else if(buf[3] & (0x1<<2))  //CHG_5v=1 --> 5V
                {
                    report_event(AC_5V_IN,"AC_5V_IN");
                }
                else    //CHG_5v=0 --> CHG_USB
                {
                    report_event(USB_5V_IN,"AC_USB_IN");
                }
            }
            else
            {
                dump_ec_event(buf);
            }
            break;
        case 0x55:
            if(buf[2]==0x00)//BAT_CAPACITY
            {
                if(buf[3]&0x1)  //CAP 5%
                {
                    report_event(BAT_CAP_5,"BAT_CAP_5");
                }           
                else if(buf[3]&(0x1<<1))    //CAP 10%
                {
                    report_event(BAT_CAP_10,"BAT_CAP_10");
                }                 
                else if(buf[3]&(0x1<<2))    //CAP 15%
                {
                    report_event(BAT_CAP_15,"BAT_CAP_15");
                }
                else if(buf[3]&(0x1<<3))    //CAP 20%
                {
                    report_event(BAT_CAP_20,"BAT_CAP_20");
                }                   
                else if(buf[3]&(0x1<<7))
                {
                    report_event(BAT_CAP_FULL,"BAT_CAP_FULL");
                }
                else
                {
                    dump_ec_event(buf);
                }
            }
            else if(buf[2]==0x01)//BAT_LOW
            {
                if(buf[3]&0x1)
                {
                    report_event(BAT_LOW,"BAT_LOW");
     		  }
#if 0
               if(buf[3]&0x2) 
                {
		      report_event(CRISIS_BAT_LOW,"CRISIS_BAT_LOW");
                }
#endif
                else
                {
                    dump_ec_event(buf);
                }
            }
            else if(buf[2]==0x02)//BAT_TEMP>45
            {
                if(buf[3]&0x1)  //TEMP>45
                {
                    report_event(BAT_TEMP45,"BAT_TEMP>45");
                }
                else
                {
                    dump_ec_event(buf);
                }
            }
            else
            {
                dump_ec_event(buf);
            }
            break;
	case 0x2f:
		if(buf[2] == 0xf0)
		{
			if(buf[3] == 0xfa)
			{
				//report_event(DATA_UPDATE_INIT,"UPDATE_INIT");
			}	
			else
			{
				dump_ec_event(buf);
			}
		}
		else if(buf[2] == 0xe0)
		{
			if(buf[3] == 0xfa)
			{
				//report_event(DATA_UPDATE_DONE,"UPDATE_DONE");
			}
			else
			{
				dump_ec_event(buf);
			}
		}
		else
		{
			dump_ec_event(buf);
		}
        default:
            dump_ec_event(buf);
            break;
    }
}

static irqreturn_t ec_interrupt(int irq, void *dev_id)
{
	if(ec.pdata)
	{
		if(DKIN_DET(ec.pdata->docking_det_irq)) 
			schedule_delayed_work(&ec.work,0);
		else
			ECLOG_ERR("%s,skip! dock was already plugged out\n",__func__);
	}
	else
		ECLOG_ERR("%s,skip! ec driver not ready!\n",__func__);
    return IRQ_HANDLED;
}
static void ec_keydown_force_release(struct input_dev *in_dev, unsigned int kbsc)
{
	if(in_dev)
	{
		input_report_key(in_dev, kbsc, 0);
		input_sync(in_dev);
	}
	else
		ECLOG_ERR("(%s) input device does not exist\n",__func__);
}
/*-----------------------------------------------------------------------------
 * ec_work_handler
 *---------------------------------------------------------------------------*/ 
static void ec_work_handler(struct work_struct *_work)
{
	#define MAX_READ_BYTE_COUNT	7
	uint8_t tmpbuf[MAX_READ_BYTE_COUNT] = {0};
	int ret = -1;
	unsigned int scancode;
	uint8_t cmd = 0xD0;
	int dx, dy;
#if DEBUGMSG
	int i;
#endif
	ret = i2c_read_byte(cmd, tmpbuf, sizeof(tmpbuf));
	if(ret<0)
	{
		(dock_connected_ok == 0 && ec_in_S3 == 1) ? EC_ERR_MSG(ret) : EC_EI2CXF_MSG;
		return;
	}
#if DEBUGMSG
	for(i = 0 ; i < 7 ; i++)
	{
		ECLOG_INFO("buf[%d]:%x\n", i, tmpbuf[i]);
	}	
#endif
	if(tmpbuf[1] == 0xD4)
	{
		if(tmpbuf[2] == 0xFA)
		{
			ECLOG_DBG("mouse ACK (0x%x)\n", tmpbuf[2]);
			return ;
		}
	
		if(tmpbuf[2] == 0x00)
		{
			if(!nmouse_dev)
			{
				ECLOG_ERR("NOTPDEV!\n");
				return;
			}
			if((ec.count == 0) && ((tmpbuf[3]&0x8) == 0)){	
			}else{
				ec.buf[ec.count] = tmpbuf[3];
				ec.count++;		
			}

			if(ec.count <3)	//EC will send 3 interrupt to host
				return;

#if DEBUGMSG
			for(i = 0 ; i < ec.count ; i++)
			{
				ECLOG_INFO( "mouse_BUF[%d]:0x%x\n", i ,ec.buf[i]);
			}
#endif
			ec.count = 0;

			dx = (ec.buf[1] ? (int) ec.buf[1] - (int) ((ec.buf[0] << 4) & 0x100) : 0)*DSCALING;
			dy = (ec.buf[2] ? (int) ((ec.buf[0] << 3) & 0x100) - (int) ec.buf[2] : 0)*DSCALING;

			input_report_rel(nmouse_dev, REL_X, dx);
			input_report_rel(nmouse_dev, REL_Y, dy);
			input_report_key(nmouse_dev, BTN_LEFT, ec.buf[0] & 0x01);
			input_report_key(nmouse_dev, BTN_RIGHT, ec.buf[0] & 0x02);

	
			ECLOG_DBG("naux: dx = %d\n", dx);
			ECLOG_DBG("naux: dy = %d\n", dy);
			ECLOG_DBG("naux: BTN_L=0x%x\n", ec.buf[0] & 0x01);
			ECLOG_DBG("naux: BTN_R=0x%x\n", ec.buf[0] & 0x02);
	
			input_sync(nmouse_dev);
       /*SMT*/
#if defined(PEGA_SMT_BUILD)
			if(KBD_enable_flag ==1 && nkbd_dev)
			{
				ECLOG_INFO("MouseBTN:%d,%d\n", ec.buf[0] & 0x01, ec.buf[0] & 0x02);
				input_report_key(nkbd_dev, KEY_F15, ec.buf[0] & 0x01);
				input_report_key(nkbd_dev, KEY_F16, ec.buf[0] & 0x02);
				input_sync(nkbd_dev);
			}
#endif
       /*SMT*/
		}
	}
	else if(tmpbuf[1]==0x60)
	{
		scancode = tmpbuf[3];
//		ec.key_scan = scancode; // save the scan code in case of the force release necessary.

		switch (scancode)
		{
			case KEY_BREAK_F0:
				ECLOG_DBG("nkbd: scancode (0x%x) KEY_BREAK_F0\n", scancode);
				ec.key_down = 0;
				break;

			case KEY_ESC_E1:
				ECLOG_DBG("nkbd: scancode (0x%x) KEY_ESC_E1\n", scancode);
				ec.pause_seq = 0x1D;

			case KEY_ESC_E0:
				ECLOG_DBG("nkbd: scancode (0x%x) KEY_ESC_E0\n", scancode);
				ec.escape = 0x100;
				ec.key_down   = 1; 
				break;

			default:
				ECLOG_DBG("nkbd: scancode (0x%x)\n", scancode);
				if (scancode & 0x80)
				{
					ec.key_down = 0;
					ECLOG_DBG("nkbd: Key down = 0\n");
				}

				/* Look for special PAUSE KEY scan code sequance: E1,1D,45 E1,9D,C5 */
				if (ec.pause_seq && ((scancode & 0x7F) == ec.pause_seq))
				{
					ECLOG_DBG("nkbd: pause_seq\n");
					return;
				}

				/* Translate scancode to keycode. If the prev key was E0 or E1, use the second half of the keycode tabel */
				scancode = set1_keycode[(scancode & 0x7F) | ec.escape];
				if (scancode != NOKEY)
				{
					ECLOG_DBG("nkbd: keycode = 0x%x, down = %d\n",scancode, ec.key_down);
					if(KBD_enable_flag ==1 && nkbd_dev)
					{
						if(ec.key_down == 0 && (scancode != ec.key_scan))
						{
							input_report_key(nkbd_dev, ec.key_scan, ec.key_down);
							input_sync(nkbd_dev);
						}
						ec.key_scan = scancode;

						input_report_key(nkbd_dev, scancode, ec.key_down);
						input_sync(nkbd_dev);
					}
				}
				else
				{
					ECLOG_DBG("nkbd: keycode = NOKEY\n");
				}

				ec.key_down  = 1;
				ec.escape    = 0;
				ec.pause_seq = 0;
				break;  
    		} /* switch (scancode) */
	}
	else
	{
    		ec_events(tmpbuf);
	}
}
  
/*-------------------------------
 *TouchPad_ID
  *------------------------------*/ 

  static ssize_t TouchPad_ID_show(struct device *dev,
				struct device_attribute *attr, char *buf)
  {
  	int ret,dock_data;
	dock_data_read(&dock_data);
	ret = sprintf(buf, "%d\n",(dock_data&TP_MASK)>>8);

	return ret;
  }
  static DEVICE_ATTR(TouchPad_ID, 0664, TouchPad_ID_show, NULL);

/*-------------------------------
 *TouchPad_enable
  *------------------------------*/
  static ssize_t TouchPad_en_store(struct device *dev,
                                        struct device_attribute *attr,
                                        const char *buf, size_t count)
{
	tp_enable(buf[0] - '0');
	return count;
}
//static DEVICE_ATTR(ec_TP_en, 0664, NULL, ec_TP_en_store);
//-------------------
static ssize_t TouchPad_query_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int ret;

	ret = sprintf(buf, "Touch Pad %s\n",(TP_status?"Enabled":"Disabled"));

	return ret;
}
static DEVICE_ATTR(TouchPad_enable, 0664, TouchPad_query_show, TouchPad_en_store);

/*-------------------------------
 *TouchPad_scaling level control entry
  *------------------------------*/ 
  static ssize_t touchpad_scaling_store(struct device *dev,
                                        struct device_attribute *attr,
                                        const char *buf, size_t count)
{
	char *ep;
	int scaling_lv = simple_strtol(buf, &ep, 10);
	if(scaling_lv > 10)
		TP_scaling= TP_SCALING_BASE+10;
	else if(scaling_lv < 0)
		TP_scaling=TP_SCALING_BASE;
	else
		TP_scaling = scaling_lv+10;
	ECLOG_DBG("%s,scaling_lv:%d|TP_scaling:%d\n",__func__,scaling_lv,TP_scaling);
	return count;
}
  static ssize_t touchpad_scaling_show(struct device *dev,
				struct device_attribute *attr, char *buf)
  {
  	int ret;
	ret = sprintf(buf, "%d\n",TP_scaling - TP_SCALING_BASE);
	return ret;
  }
  static DEVICE_ATTR(TouchPad_Scaling, 0664, touchpad_scaling_show, touchpad_scaling_store);

/*-------------------------------
 *ec_version_show
  *------------------------------*/  
  static ssize_t ec_version_show(struct device *dev,
				struct device_attribute *attr, char *buf)
  {
    	u8 tmpbuf[15] = {0};
	u8 cmd = 0xc0;
	int err;
       int ret;
	err = i2c_read_byte(cmd, tmpbuf,14);
	if( err < 0 ){
		EC_EI2CXF_MSG;
		return 0;
	}
    
      if(tmpbuf[2] == 'W' && tmpbuf[3] == 'L')
          ret = sprintf(buf, "%s\n",tmpbuf+2);
      else
          ret = sprintf(buf,"wrong ec_version format:%s\n",tmpbuf+2);

	return ret;
  }
  static DEVICE_ATTR(ec_version, 0664, ec_version_show, NULL);

  /*-------------------------------
 *Keyboard_ID
  *------------------------------*/ 
  static ssize_t Keyboard_ID_show(struct device *dev,
				struct device_attribute *attr, char *buf)
  {
  	int ret,dock_data;
	dock_data_read(&dock_data);
	ret = sprintf(buf, "%d\n",dock_data&KBD_MASK);

	return ret;
  }
  static DEVICE_ATTR(Keyboard_ID, 0664, Keyboard_ID_show, NULL);

  /*-------------------------------
 *Board_ID
  *------------------------------*/ 
  static ssize_t Board_ID_show(struct device *dev,
				struct device_attribute *attr, char *buf)
  {
  	int ret,ver;
	ret = ec_dock_ver(&ver);
	if(ret < 0)
		ret= sprintf(buf,"%d\n", 0);
	else
		ret = sprintf(buf,"%d\n", ver);

	return ret;
  }
  static DEVICE_ATTR(Board_ID, 0664, Board_ID_show, NULL);

  /*-------------------------------
 *Charger_Control
  *------------------------------*/ 
  static ssize_t Charger_Control_show(struct device *dev,
				struct device_attribute *attr, char *buf)
  {
  	int ret,info;
	get_ec_charging_info(&info,CHG_CONTROL);
	ret = sprintf(buf,"0x%x \n", info);

	return ret;
  }
  static DEVICE_ATTR(Charger_Control, 0664, Charger_Control_show, NULL);

  /*-------------------------------
 *Charger_Manufacture_ID
  *------------------------------*/ 
  static ssize_t Charger_Manufacture_ID_show(struct device *dev,
				struct device_attribute *attr, char *buf)
  {
  	int ret,info;
	get_ec_charging_info(&info,CHG_MANUFACTURE_ID);
	ret = sprintf(buf,"0x%x \n", info);

	return ret;
  }
  static DEVICE_ATTR(Charger_Manufacture_ID, 0664, Charger_Manufacture_ID_show, NULL);

  /*-------------------------------
 *Charger_Device_ID
  *------------------------------*/ 
  static ssize_t Charger_Device_ID_show(struct device *dev,
				struct device_attribute *attr, char *buf)
  {
  	int ret,info;
	get_ec_charging_info(&info,CHG_DEVICE_ID);
	ret = sprintf(buf,"0x%x \n", info);

	return ret;
  }
  static DEVICE_ATTR(Charger_Device_ID, 0664, Charger_Device_ID_show, NULL);

  /*-------------------------------
 *Battery_Device_Type_ID
  *------------------------------*/ 
  static ssize_t Battery_Device_Type_ID_show(struct device *dev,
				struct device_attribute *attr, char *buf)
  {
  	int ret,info;
	get_ec_bat_info(&info,DEV_ID);
	ret = sprintf(buf,"0x%x \n", info);

	return ret;
  }
  static DEVICE_ATTR(Battery_Device_Type_ID, 0664, Battery_Device_Type_ID_show, NULL);

 /*-------------------------------
 *Battery_FW_Ver_ID
 *------------------------------*/ 
  static ssize_t Battery_FW_Ver_ID_show(struct device *dev,
				struct device_attribute *attr, char *buf)
  {
  	int ret,info;
	get_ec_bat_info(&info,FW_ID);
	ret = sprintf(buf,"0x%x \n", info);

	return ret;
  }
  static DEVICE_ATTR(Battery_FW_Ver_ID, 0664, Battery_FW_Ver_ID_show, NULL);

  
/*-------------------------------
 *Battery_HW_Ver_ID
 *------------------------------*/ 
  static ssize_t Battery_HW_Ver_ID_show(struct device *dev,
				struct device_attribute *attr, char *buf)
  {
  	int ret,info;
	get_ec_bat_info(&info,HW_ID);
	ret = sprintf(buf,"0x%x \n", info);

	return ret;
  }
  static DEVICE_ATTR(Battery_HW_Ver_ID, 0664, Battery_HW_Ver_ID_show, NULL);

  /*-------------------------------
 *Battery_DF_Ver_ID
 *------------------------------*/ 
  static ssize_t Battery_DF_Ver_ID_show(struct device *dev,
				struct device_attribute *attr, char *buf)
  {
  	int ret,info;
	get_ec_bat_info(&info,DF_ID);
	ret = sprintf(buf,"0x%x \n", info);

	return ret;
  }
  static DEVICE_ATTR(Battery_DF_Ver_ID, 0664, Battery_DF_Ver_ID_show, NULL);

/* BEGIN Jack1_Huang [2012/03/16] Add adb api for factory mode */

 /*-------------------------------
 *Read dock pad config
 *------------------------------*/
static ssize_t dock_pad_config_show(struct device *dev, struct device_attribute *attr, char *buf)
{
        int ret;
        int dock_pad_value_r = 0;
        if(dock_pad_config_read(&dock_pad_value_r) == SUCCESS)
        {
                ret = sprintf(buf,"%d\n", ~dock_pad_value_r);
                ECLOG_INFO("%s sucess: buffer = %s, lval = %d\n",__func__,buf,~dock_pad_value_r);
		return ret;
        }else
        {
		ECLOG_ERR("%s failed: buffer = %s, lval = %d\n",__func__,buf,~dock_pad_value_r);
                return -EINVAL;
        }

        return ret;
}


 /*-------------------------------
 *Write dock pad config
 *------------------------------*/
static ssize_t dock_pad_config_store(struct device *dev, struct device_attribute *attr,const char *buf,size_t size)
{
	int dock_pad_value_w = 0;
        unsigned long lval;

        if(strict_strtoul(buf, 10, &lval))return -EINVAL;

        dock_pad_value_w = (int)lval;
        if(dock_pad_config_write(~dock_pad_value_w) == SUCCESS)
        {
		  ECLOG_INFO("[%s] write success. lval = %d\n",__func__,~dock_pad_value_w);
                return size;
        }else
        {
		  ECLOG_ERR( "[%s] write failed. lval = %d\n",__func__,~dock_pad_value_w);
                return -EINVAL;
        }

        return size;
}

static DEVICE_ATTR(dock_pad_enable, 0664, dock_pad_config_show, dock_pad_config_store);
//static DEVICE_ATTR(dock_pad_enable, 0666, dock_pad_config_show, NULL);

/* END Jack1_Huang [2012/03/16] Add adb api for factory mode */

 /*-------------------------------
 *Write dock KBD_matrix_change
 *------------------------------*/
static ssize_t dock_KBD_matrix_change_store(struct device *dev, struct device_attribute *attr,const char *buf,size_t size)
{
	char *ep;
 	int ref_1, ref_2;
	char *buffer = (char*)buf;

 	ref_1 = simple_strtol(buffer, &ep, 10);
	
	if (_next_str(&buffer, ep)) {
		return 0;
	}
	ref_2 = simple_strtol(buffer, &ep, 10);
	
	ECLOG_DBG("index:%d, value:%d\n", ref_1, ref_2);

	if(ref_1 > 511 || ref_1 < 0 )
	{
		ECLOG_ERR("%s: index out of range\n index:%d, value:%d\n", __func__, ref_1, ref_2);
		return size;
	}
		
	set1_keycode[ref_1] = ref_2;
	
	ECLOG_INFO("%s, set1_keycode[%d]:0x%x\n", __func__,ref_1, set1_keycode[ref_1]);
	set_bit(set1_keycode[ref_1], nkbd_dev->keybit);
	return size;
}
 static DEVICE_ATTR(dock_KBD_matrix_change, 0664, NULL, dock_KBD_matrix_change_store);


 /*-------------------------------
 *Read dock KBD_enable_flag
 *------------------------------*/
static ssize_t dock_KBD_enable_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret = 0;

	ret = sprintf(buf,"%d\n", KBD_enable_flag);
	return ret;
}

 /*-------------------------------
 *Write dock KBD_enable_flag
 *------------------------------*/
static ssize_t dock_KBD_enable_store(struct device *dev, struct device_attribute *attr,const char *buf,size_t size)
{
	KBD_enable_flag = (buf[0] - '0');
	return size;
}
 static DEVICE_ATTR(dock_KBD_enable, 0664, dock_KBD_enable_show, dock_KBD_enable_store);
 
 /*-------------------------------
 *Read dock KBD_wakeup_en
 *------------------------------*/
static ssize_t dock_KBD_wakeup_en_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret = 0;
	int flag = 0;

	ret = get_wakeup_flag(&flag);
	if(ret < 0)
		ECLOG_ERR("%s, FAIL\n",__func__);
		
	flag =  (flag  & 0x2) >>1;
	ret = sprintf(buf,"%d\n", flag);
	
	return ret;
}

 /*-------------------------------
 *Write dock KBD_wakeup_en
 *------------------------------*/
static ssize_t dock_KBD_wakeup_en_store(struct device *dev, struct device_attribute *attr,const char *buf,size_t size)
{
	char *ep;
	int flag = 0;
	int ret;
	int ref;

	ref = simple_strtol(buf, &ep, 10);
	
	ret = get_wakeup_flag(&flag);
	if(ret < 0)
		ECLOG_ERR("%s, FAIL\n",__func__);

	if(ref == 1)
		flag = flag | (1<<1);
	else if(ref == 0)
		flag = flag & ~(1<<1);
	
	ret = set_wakeup_flag(flag);
	if(ret < 0)
		ECLOG_ERR("%s, FAIL\n",__func__);
	
	return size;
}
 static DEVICE_ATTR(dock_KBD_wakeup_en, 0664, dock_KBD_wakeup_en_show, dock_KBD_wakeup_en_store);
#if 1
 /*-------------------------------
 *Read dock SD_wakeup_en
 *------------------------------*/
static ssize_t dock_SD_card_wakeup_en_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret = 0;
	int flag = 0;

	ret = get_wakeup_flag(&flag);
	if(ret < 0)
		ECLOG_ERR("%s, FAIL\n",__func__);
		
	flag =  (flag  & 0x4) >>2;
	ret = sprintf(buf,"%d\n", flag);
	
	return ret;
}

 /*-------------------------------
 *Write dock SD_wakeup_en
 *------------------------------*/
static ssize_t dock_SD_card_wakeup_en_store(struct device *dev, struct device_attribute *attr,const char *buf,size_t size)
{
	char *ep;
	int flag = 0;
	int ret;
	int ref;
	
	ref = simple_strtol(buf, &ep, 10);
	
	ret = get_wakeup_flag(&flag);
	if(ret < 0)
		ECLOG_ERR("%s, FAIL\n",__func__);

	if(ref == 1)
		flag = flag | (1<<2);
	else if(ref == 0)
		flag = flag & ~(1<<2);
	
	ret = set_wakeup_flag(flag);
	if(ret < 0)
		ECLOG_ERR("%s, FAIL\n",__func__);
	
	return size;
}
 static DEVICE_ATTR(dock_SD_card_wakeup_en, 0664, dock_SD_card_wakeup_en_show, dock_SD_card_wakeup_en_store);
#endif
 /*-------------------------------
 *Write dock LED_BLUE on/off
 *------------------------------*/ 
static ssize_t dock_LED_blue_store(struct device *dev, struct device_attribute *attr,const char *buf,size_t size)
{
	char *ep;
	u8 W_data[5] = {0};
	int ref;

 	ref = simple_strtol(buf, &ep, 16);
	ECLOG_INFO("%s capslock:(%d) \n",__func__,ref);
/*Disable EC control LED*/
      	W_data[0] = 0x09;
	W_data[1] = 0x03;
	W_data[2] = 0x03;
	W_data[3] = 0x02;
	W_data[4] = 0x00;
	if(i2c_write_byte(W_data,sizeof(W_data)))
       {   
            ECLOG_ERR("FAIL,LED BLUE\n");
       }
/*disable PWM*/

	W_data[0] = 0x70;
	W_data[1] = 0x09;
	W_data[2] = 0xCF;
       W_data[3] = 0x00;
       W_data[4] = 0x00;
       if(i2c_write_byte(W_data, sizeof(W_data)))
       {
            ECLOG_ERR("FAIL,LED BLUE\n");
            return size;
       }

/*Set new LED Status*/
	W_data[0] = 0x09;
       W_data[1] = 0x03;
	W_data[2] = 0x08;
       if(ref)
		W_data[3] = 0x01;
       else
		W_data[3] = 0x00;
       W_data[4] = 0x00;
       if(i2c_write_byte(W_data,sizeof(W_data)))
        {
            ECLOG_ERR("FAIL,LED_BLUE\n");
            return size;
        }
       else {
            ECLOG_DBG("PASS,LED BLUE\n");
       }
     

	return size;
 }
  /*-------------------------------
 *Read dock LED_BLUE status
 *------------------------------*/
 static ssize_t dock_LED_blue_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    u8 cmd = 0xb6; //Read EC GPI66 (LED_B)
    u8 ret[5];
    int r,success;
#if 1
    u8 cmd_2 = 0xd8;
    int err = 0;
    err = i2c_read_byte(cmd_2, ret, sizeof(ret));

    if(err < 0)
    {
	ECLOG_ERR("failed to read!\n");
    }
    else{	
	ECLOG_INFO("%s,sub_id:0x%x, control:0x%x, Caps_status:0x%x\n", __func__, ret[1], ret[2], ret[3]);
    }		

#endif

    if(i2c_read_byte(cmd, ret, sizeof(ret)))
    {
        success =0;
        ECLOG_ERR("failed to read!\n");
    }
    else
        success =1;
    r = (ret[3] >>6) & 0x1;
    r = sprintf(buf,"%d\n", r);
   
    return r;
 }
 static DEVICE_ATTR(dock_LED_blue, 0664, dock_LED_blue_show, dock_LED_blue_store);

#if 1
  /*-------------------------------
 *Read dock SD card status
 *------------------------------*/
 static ssize_t dock_SD_card_status_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int ret;
	int SD_status;
	ret = get_dock_SD_card_status(&SD_status);
	ret = sprintf(buf,"%d\n",SD_status);	//0:with SD card, 1:without SD card

	return ret;
}
static DEVICE_ATTR(dock_SD_card_status, 0664, dock_SD_card_status_show, NULL);
#endif

 /*-------------------------------
 *Read dock KBD_enable_flag
 *------------------------------*/
static ssize_t eclog_dbg_lv_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret = 0;
	ret = sprintf(buf,"%d\n", ecloglv);
	return ret;
}

 /*-------------------------------
 *Write dock KBD_enable_flag
 *------------------------------*/
static ssize_t eclog_dbg_lv_store(struct device *dev, struct device_attribute *attr,const char *buf,size_t size)
{
	ecloglv = (buf[0] - '0');
	return size;
}
 static DEVICE_ATTR(eclog_lv, 0664, eclog_dbg_lv_show, eclog_dbg_lv_store);

//------------------
/*0120*/
static struct attribute *ec_attrs[] = {
	&dev_attr_ec_version.attr,
        &dev_attr_TouchPad_ID.attr,
	&dev_attr_TouchPad_enable.attr,
	&dev_attr_TouchPad_Scaling.attr,
	&dev_attr_Keyboard_ID.attr,
	&dev_attr_Board_ID.attr,
	&dev_attr_Charger_Control.attr,
	&dev_attr_Charger_Manufacture_ID.attr,
	&dev_attr_Charger_Device_ID.attr,
	&dev_attr_Battery_Device_Type_ID.attr,
	&dev_attr_Battery_FW_Ver_ID.attr,
	&dev_attr_Battery_HW_Ver_ID.attr,
	&dev_attr_Battery_DF_Ver_ID.attr,
	&dev_attr_dock_pad_enable.attr,
	&dev_attr_dock_KBD_matrix_change.attr,
	&dev_attr_dock_KBD_enable.attr,
	&dev_attr_dock_KBD_wakeup_en.attr,
	&dev_attr_dock_LED_blue.attr,
#if 1
	&dev_attr_dock_SD_card_status.attr,
	&dev_attr_dock_SD_card_wakeup_en.attr,
#endif
	&dev_attr_eclog_lv.attr,
        NULL
};

static const struct attribute_group ec_attr_group = {
        .attrs = ec_attrs,
};
/*0120*/


#ifdef CONFIG_WAKELOCK
        void ec_early_suspend(struct early_suspend *h);
        void ec_late_resume(struct early_suspend *h);
#endif

/*-----------------------------------------------------------------------------
 * check_ec_version
 *---------------------------------------------------------------------------*/
 int check_ec_version(void)
{
    	u8 tmpbuf[15] = {0};
	u8 cmd = 0xc0;
	int err;
	int val = 0;			//current dock firmware version ascII value
	int val_0327 = 0;		//WL120327.T01 for ascII value
	int val_0525 = 0;
	bulk_cmd_flag = 0;
	get_ac_cmd_flag	= 0;
	err = i2c_read_byte(cmd, tmpbuf,sizeof(tmpbuf));
	if( err < 0 ){
		EC_EI2CXF_MSG;
		return 0;
	}

	val_0327 = 48*1000 + 51*100 + 50*10 + 55;
	val_0525 = 48*1000 + 53*100 + 50*10 + 53;
 	val = (tmpbuf[6])*1000 + (tmpbuf[7] )*100 + (tmpbuf[8])*10 + tmpbuf[9];

	if(val > val_0327)	//dock firmware now is latest than 0327
	{
		bulk_cmd_flag = 1;	
	} else {
		bulk_cmd_flag = 0;
	}

	if(val > val_0525)	//dock firmware now is latest than 0525
	{
		get_ac_cmd_flag = 1;
	} else {
		get_ac_cmd_flag = 0;
	}
	return 0;
}

/*-----------------------------------------------------------------------------
 * ec_i2c_prob
 *---------------------------------------------------------------------------*/
static int ec_i2c_prob(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct  EC_platform_data*  ec_data; 
	int ret; 
	int error = 0; 
	int dock_data;
	/*Set the GP12_Dock_SW to L (for Dock Det) -- BEGIN*/
	if(!gpio_request(12,"gpio-12"))
		gpio_direction_output(12,0);
	else
		ECLOG_ERR("set gpio-12 failed\n");
	/*Set the GP12_Dock_SW to L (for Dock Det) -- END*/
	if (client->addr !=  EC_SMB_ADDRESS) {
		return -1; // Error
	}
	 ec.I2cClient = client;  /* Save a pointer to the i2c client struct we need it during the interrupt */
 
	ec_data = client->dev.platform_data;

	if(ec_data->gpio_setup && ec_data->EC_irq)
	{
		ret=ec_data->gpio_setup(ec_data->EC_irq, "EC_int");
		if(ret < 0){
			ECLOG_ERR("nuvec gpio_int setup failed\n");
			goto err;
		}
		ret = request_irq(gpio_to_irq(ec_data->EC_irq), ec_interrupt, IRQF_TRIGGER_FALLING, "NuvEC", NULL);
		if (ret)
		{
			ECLOG_ERR("nuvec: err unable to get IRQ\n");
			goto err;
		}
	}

#ifdef CONFIG_WAKELOCK
	ec.ec_early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	ec.ec_early_suspend.suspend = ec_early_suspend;
	ec.ec_early_suspend.resume =ec_late_resume;
	register_early_suspend(&ec.ec_early_suspend);
#endif //CONFIG_HAS_WAKELOCK

	ec.pdata = ec_data;
	ec_platform_dev =  platform_device_register_simple("Docking", -1, NULL, 0);
	if (IS_ERR(ec_platform_dev)) {
		error = PTR_ERR(ec_platform_dev);
		ECLOG_ERR("unable to register platform device\n");
		platform_device_unregister(ec_platform_dev);
	}
	platform_set_drvdata(ec_platform_dev, &ec);

	error = sysfs_create_group(&ec_platform_dev->dev.kobj, &ec_attr_group);
	if (error) {
		ECLOG_ERR("unable to create sysfs_create_group \n");
	}

	/* init some variables for mouse*/
	ec.count = 0; 
	/*init some variables for kbd*/
	ec.key_down  = 1;
	ec.escape    = 0;
	ec.pause_seq = 0;
	device_init_wakeup(&ec_platform_dev->dev,1);
	ECLOG_INFO("%s ok \n",__func__);

	if(DKIN_DET(ec_data->docking_det_irq))
	{
		dock_data_read(&dock_data);
		ECLOG_INFO("%s,dock_data:0x%x\n", __func__,dock_data);
	}
	
	return 0;

err:
	return ret;
}

/*-----------------------------------------------------------------------------
 * ec_i2c_remove
 *---------------------------------------------------------------------------*/
static int ec_i2c_remove(struct i2c_client *client)
{
    ECLOG_DBG( "nuvec: i2c_remove -\n");
#if KERNEL_2625
    if (i2c_detach_client(client)) {
        return -1;
    }
#endif
	device_init_wakeup(&ec_platform_dev->dev,0);
    kfree(i2c_get_clientdata(client));
    return 0;
}

#if KERNEL_2625
static int ec_i2c_attach_adapter(struct i2c_adapter *adapter){
    return i2c_probe(adapter, &addr_data, ec_detect);
}
#endif //KERNEL_2625 


static int input_device_create(void)
{
	int ret,i;
	if(nkbd_dev){
		return 0;
	}	
  	nkbd_dev = input_allocate_device();
	if(!nkbd_dev)
	{
		ECLOG_ERR("%s,input _dev allocation fails\n",__func__);
		ret = -1;
		goto err;
	}
	nmouse_dev = input_allocate_device();
	if (!nmouse_dev )
	{
		ECLOG_ERR("naux : error allocting memory for input device\n");
		goto err;
	}
	/*KeyBoard*/
	nkbd_dev->name       = "S2 Dock Keyboard";
    	nkbd_dev->phys       = "i2c/input0";
    	nkbd_dev->id.bustype = BUS_I2C;
	nkbd_dev->id.vendor  = 0x1050;  //TBD
    	nkbd_dev->id.product = 0x0006;  //TBD
    	nkbd_dev->id.version = 0x0004; 
	nkbd_dev->evbit[0]   = BIT_MASK(EV_KEY) | BIT_MASK(EV_REP);
	/*TouchPad*/
	nmouse_dev->name       = "S2 Dock TouchPad";
	nmouse_dev->phys       = "i2c/input1";      /* ?? what if nkbc also installed */
	nmouse_dev->id.bustype = BUS_I2C;
	nmouse_dev->id.vendor  = 0x1050;  //TBD
	nmouse_dev->id.product = 0x0006;  //TBD
	nmouse_dev->id.version = 0x0005;  
	nmouse_dev->evbit[0]   = BIT_MASK(EV_KEY) | BIT_MASK(EV_REL);
	nmouse_dev->relbit[0]  = BIT_MASK(REL_X)  | BIT_MASK(REL_Y);
	nmouse_dev->keybit[BIT_WORD(BTN_LEFT)] = BIT_MASK(BTN_LEFT) | BIT_MASK(BTN_MIDDLE)| BIT_MASK(BTN_RIGHT);
	/*KeyCode*/
    	for (i = 1; i < 512 ; i++)
    	{
        	set_bit(set1_keycode[i], nkbd_dev->keybit);
    	}
	ret = input_register_device(nkbd_dev);
	if(ret)
	{
	        ECLOG_ERR("nkbd: err input register device\n");
		goto exit_input_free;
	}
	/*TouchPad*/
	ret = input_register_device(nmouse_dev);
	if (ret)
	{
		ECLOG_ERR("naux: err input register device\n");
		goto exit_input_free;
	}
	return 0;
exit_input_free:
	 input_free_device(nkbd_dev);
	 input_free_device(nmouse_dev);
	 nkbd_dev = NULL;
	 nmouse_dev = NULL;
err:
	return ret;
}

void ec_dock_in(void)
{
	if(ec.pdata)
	{ // avoid timming issue when dock-in power on

	int ret = 0;
	ret = gpio_direction_output(Dock_Reset_Pin, 0);
	if(ret)
		ECLOG_ERR("%s:gpio_direction_output error\n",__func__);
	
	input_device_create();
	ECLOG_DBG("kbd_name:%s\n",nkbd_dev->name);
	ECLOG_DBG("kbd_phys:%s\n",nkbd_dev->phys);
	ECLOG_DBG("tpd_name:%s\n",nmouse_dev->name);
	ECLOG_DBG("tpd_phys:%s\n",nmouse_dev->phys);
	//-------------------------------------------------------
		if(!gpio_get_value(ec.pdata->EC_irq))
			schedule_delayed_work(&ec.work, 0);
		schedule_delayed_work(&ec.det_work, 0);
	}
	dock_connected_ok = 1;
}

void ec_dock_out(void)
{
	ECLOG_DBG("%s\n",__func__);
	if(ec.pdata)
	{
		if(nkbd_dev)
		{
			input_unregister_device(nkbd_dev);	//0424
			nkbd_dev = NULL;	
		}
		if(nmouse_dev)
		{
			input_unregister_device(nmouse_dev);
			nmouse_dev = NULL;
		}
		cancel_delayed_work(&ec.work);	
		cancel_delayed_work(&ec.det_work);
	}
	dock_connected_ok = 0;
	error_times = 0;
	doing_ec_reset = 0;
	TP_status = 1;
	ec_in_S3 = 0;
	KBD_enable_flag = 1;
}
void dock_i2c_fail(void)
{
	report_event(DOCK_I2C_ABORT,"DOCK_I2C_ABORT");
}
/*----------------------------------------------------------------------------
 * ec_state_change
 *----------------------------------------------------------------------------*/
static int ec_state_change(int ec_sys_state)
{
	int err;
	int i = 0;
	u8 data[5];
	ECLOG_INFO("%s(%d)\n",__func__,ec_sys_state);
	
	data[0] = 0x4e;	//for EC spec,sub_id
	data[1] = 0x03;	
	data[2] = 0xd1;		//CMD
	data[3] = ec_sys_state;	//sys_state
	data[4] = 0x00;

	while(i<ERROR_CHECK && error_times <= MAX_ERR_TIME)
	{
		err =i2c_write_byte(data, sizeof(data));
		if( err < 0 )
		{
			if(!doing_ec_reset)
			{
				EC_EI2CXF_MSG;
				i++;
			} else
			{
				return FAIL;
			}
			msleep(ec_init_delay/2);
		}
		else
		{
			return SUCCESS;
		}
	}
	return FAIL;
}

/*----------------------------------------------------------------------------
 * ec_get_charging_info
 *----------------------------------------------------------------------------*/
int get_ec_charging_info(int *info, int cmd)
{
    	uint8_t tmpbuf[4] = {0};
    	int err;

	err = i2c_read_byte(cmd, tmpbuf,sizeof(tmpbuf));

	if( err < 0 ){
		EC_EI2CXF_MSG;
		return FAIL;
	}

	*info = (tmpbuf[3]<<8)+tmpbuf[2];
	ECLOG_DBG("%s(%d,0x%x)\n",__func__,*info,cmd);
	return	SUCCESS;
}
EXPORT_SYMBOL(get_ec_charging_info);
/*----------------------------------------------------------------------------
 * ec_get_adc_id
 *----------------------------------------------------------------------------*/
int ec_get_adc_id(int *id)
{
	int cmd = 0xc1;
	int err;
	u8 data[4];
	err = i2c_read_byte(cmd,data,sizeof(data));
	if(err < 0)
	{
		EC_EI2CXF_MSG;
		return FAIL;
	}
	*id = (data[3]<<8)+data[2];
	ECLOG_DBG("%s(%d)\n",__func__,*id);
	return SUCCESS;
}
EXPORT_SYMBOL(ec_get_adc_id);
/*----------------------------------------------------------------------------
 * Check_Dock_version
 *----------------------------------------------------------------------------*/
 int ec_dock_ver(int* ver)
{
	int cmd = 0xc6;
	int ret;
	u8 data[4];
	ret = i2c_read_byte(cmd,data,sizeof(data));

	if(ret < 0)
	{
		EC_EI2CXF_MSG;
		return FAIL;	
	}
	if(data[2] & 0x1)
		*ver = DOCK_DVT;
	else if(data[2] & (0x1 << 1))
		*ver = DOCK_PVT;
	else if(data[2] & (0x1 << 2))
		*ver = DOCK_SIT2;
	else if(data[2] & (0x1 <<3))
		*ver = DOCK_SIT3;
	else if(data[2] & (0x1 <<4))
		*ver = DOCK_SIT4;
	else
		*ver = DOCK_UNKNOWN;
	ECLOG_DBG("%s(%d)\n",__func__,*ver);
	return ret;
}
/*----------------------------------------------------------------------------
 * EC reset
 *----------------------------------------------------------------------------*/
int dock_reset_func()
{
	int ret;
	int rst_type;

	ECLOG_INFO("%s, err_time:%d\n",__func__,error_times);
	report_event(DO_ECRST,"EC_RST_START");
	gpio_direction_input(Dock_Reset_Pin);
	if(ec.key_down)
	{
		ec_keydown_force_release(nkbd_dev,ec.key_scan);
		ec.key_down = 0;
	}
	if(ec.buf[0] & 0x01)
	{
		ec_keydown_force_release(nmouse_dev, BTN_LEFT);
		ec.buf[0] = ec.buf[0] & ~0x01;
	}
	if(ec.buf[0] & 0x02)
	{
		ec_keydown_force_release(nmouse_dev, BTN_RIGHT);
		ec.buf[0] = ec.buf[0] & ~0x02;
	}
	rst_type = gpio_get_value(Dock_Reset_Pin);
	ECLOG_DBG("RST-Pin(GP34):%d\n",rst_type);
	
	ret = gpio_direction_output(Dock_Reset_Pin, 0);
	if(ret)
	{
		ECLOG_ERR("%s:gpio_direction_output error\n",__func__);
		goto fail;
	}

	ret = gpio_direction_output(Dock_Reset_Pin, 1);
	if(ret)
	{
		ECLOG_ERR("%s:gpio_direction_output error\n",__func__);
		goto fail;
	}
	msleep(10);
	ret = gpio_direction_output(Dock_Reset_Pin, 0);
	if(ret)
	{
		ECLOG_ERR("%s:gpio_direction_output error\n",__func__);
		goto fail;
	}

	msleep(ec_init_delay);

fail:
	return ret;
}
EXPORT_SYMBOL(dock_reset_func);
/*----------------------------------------------------------------------------
 * touchpad_switch
 *----------------------------------------------------------------------------*/
int touchpad_switch(int on_off)
{
	int ret;
	int cmd = 0xb9;
	u8 R_data[4];
	u8 W_data[5];

	ECLOG_DBG("%s(%d)\n",__func__,on_off);
	W_data[0] = 0x4e;
	W_data[1] = 0x03;
	W_data[2] = 0xb9;
	W_data[3] = 0x00;
	W_data[4] = 0x00;
	ret = i2c_read_byte(cmd, R_data, sizeof(R_data));
	if(ret < 0)
		return FAIL;
	
	if(on_off == TP_SWITCH_ON)
		W_data[3] = R_data[3] | TP_GPIO_MASK;
	else if( on_off == TP_SWITCH_OFF)
		W_data[3] = R_data[3] & (~(TP_GPIO_MASK));
	else 
		ECLOG_ERR("%s(%d):WRONG CMD\n", __func__,on_off);
	
	ret = i2c_write_byte(W_data, sizeof(W_data));
	if(ret < 0)
		return FAIL;
	
	return SUCCESS;
}
EXPORT_SYMBOL(touchpad_switch);
/*----------------------------------------------------------------------------
 * sdcard_switch
 *----------------------------------------------------------------------------*/
int sdcard_switch(int on_off)
{
	int ret;
	int cmd = 0xb1;
	int cmd2 = 0xb7;
	u8 R1_data[4];	//read sd card switch gpio_15
	u8 R2_data[4];	//read sd card reset gpio_77
	u8 W1_data[5];	//write sd card switch gpio_15
	u8 W2_data[5];	//write sd card reset gpio_77

	ECLOG_DBG("%s(%d)\n",__func__,on_off);
	W1_data[0] = 0x4e;
	W1_data[1] = 0x03;
	W1_data[2] = 0xb1;
	W1_data[3] = 0x00;
	W1_data[4] = 0x00;
	
	W2_data[0] = 0x4e;
	W2_data[1] = 0x03;
	W2_data[2] = 0xb7;
	W2_data[3] = 0x00;
	W2_data[4] = 0x00;
	
	ret = i2c_read_byte(cmd, R1_data, sizeof(R1_data));
	if(ret < 0)
		return FAIL;
	ret = i2c_read_byte(cmd2, R2_data, sizeof(R2_data));
	if(ret < 0)
		return FAIL;
	
	if(on_off == SD_CARD_ON) 
	{
		W1_data[3] = R1_data[3] | SD_CARD_GPIO_MASK;			//set gpio15 high
		W2_data[3] = R2_data[3] | SD_CARD_RESET_MASK;		//set gpio77 high

		ret = i2c_write_byte(W1_data, sizeof(W1_data));
		if(ret < 0)
			return FAIL;
		ret = i2c_write_byte(W2_data, sizeof(W2_data));
		if(ret < 0)
			return FAIL;		
	}
	else if(on_off == SD_CARD_OFF) 
	{	
		W1_data[3] = R1_data[3] & (~(SD_CARD_GPIO_MASK));		//set gpio15 low
		W2_data[3] = R2_data[3] & (~(SD_CARD_RESET_MASK));	//set gpio77 low

		ret = i2c_write_byte(W2_data, sizeof(W2_data));
		if(ret < 0)
			return FAIL;
		ret = i2c_write_byte(W1_data, sizeof(W1_data));
		if(ret < 0)
			return FAIL;		
	}
	else  
	{
		ECLOG_ERR("%s(%d):WRONG CMD\n", __func__,on_off);
		return FAIL;
	}	

	return SUCCESS;
}
EXPORT_SYMBOL(sdcard_switch);
/*----------------------------------------------------------------------------
 * usb_hub_switch
 *----------------------------------------------------------------------------*/
int usb_hub_switch(int on_off)
{
	int ret;
	int cmd = 0xb2;
	int cmd2 = 0xb7;
	u8 R1_data[4];		//read usb hub switch gpio_21
	u8 R2_data[4];		//read usb hub reset gpio_75
	u8 W1_data[5];		//write usb hub switch gpio_21
	u8 W2_data[5];		//write usb hub reset gpio_75

	ECLOG_DBG("%s(%d)\n",__func__,on_off);
	W1_data[0] = 0x4e;
	W1_data[1] = 0x03;
	W1_data[2] = 0xb2;
	W1_data[3] = 0x00;
	W1_data[4] = 0x00;

	W2_data[0] = 0x4e;
	W2_data[1] = 0x03;
	W2_data[2] = 0xb7;
	W2_data[3] = 0x00;
	W2_data[4] = 0x00;

	ret = i2c_read_byte(cmd, R1_data, sizeof(R1_data));
	if(ret < 0)
		return FAIL;
	ret = i2c_read_byte(cmd2, R2_data, sizeof(R2_data));
	if(ret < 0)
		return FAIL;
	
	if(on_off == SD_CARD_ON)
	{
		W1_data[3] = R1_data[3] | USB_HUB_GPIO_MASK;		//set gpio_21 high
		W2_data[3] = R2_data[3] | USB_HUB_RESET_MASK;		//set gpio_75 high

		ret = i2c_write_byte(W1_data, sizeof(W1_data));
		if(ret < 0)
			return FAIL;
		ret = i2c_write_byte(W2_data, sizeof(W2_data));
		if(ret < 0)
			return FAIL;		
	}
	else if(on_off == SD_CARD_OFF)
	{
		W1_data[3] = R1_data[3] & (~(USB_HUB_GPIO_MASK));		//set gpio_21 low
		W2_data[3] = R2_data[3] & (~(USB_HUB_RESET_MASK));	//set gpio_75 low
		
		ret = i2c_write_byte(W2_data, sizeof(W2_data));
		if(ret < 0)
			return FAIL;
		ret = i2c_write_byte(W1_data, sizeof(W1_data));
		if(ret < 0)
			return FAIL;			
	}	
	else
	{
		ECLOG_ERR("%s(%d):WRONG CMD\n", __func__,on_off);
		return FAIL;
	}
	
	return SUCCESS;
}
EXPORT_SYMBOL(usb_hub_switch);
/*----------------------------------------------------------------------------
 * wakeup_flag
 *----------------------------------------------------------------------------*/
int set_wakeup_flag(int data)
{
	int ret;
	u8 W_data[5];
	
	W_data[0] = 0x09;
	W_data[1] = 0x03;
	W_data[2] = 0x05;
	W_data[3] = data;//0x00;
	W_data[4] = 0x00;

	ECLOG_DBG("%s(%d)\n",__func__,data);
	ret = i2c_write_byte(W_data, sizeof(W_data));
	if(ret < 0)
		return FAIL;
		
	return SUCCESS;
}
EXPORT_SYMBOL(set_wakeup_flag);
/*----------------------------------------------------------------------------
 * time out wakeup_flag
 *----------------------------------------------------------------------------*/
int wakeup_flag(int enable)
{
	
	int flag = 0;
	int ret;

	ECLOG_DBG("%s(%d)\n",__func__,enable);
	ret = get_wakeup_flag(&flag);
	if(ret < 0)
	{
		ECLOG_ERR("%s, get wakeup flag fail\n",__func__);
		return FAIL;
	}

	if(enable == 1)
		flag = flag | 1;
	else if(enable == 0)
		flag = flag & ~(1);
	else
	{
		ECLOG_ERR("%s(%d):wrong parameter!",__func__,enable);
		return FAIL;
	}
	
	ret = set_wakeup_flag(flag);
	if(ret < 0)
	{
		ECLOG_ERR("%s, FAIL to set wakeup flag\n",__func__);
		return FAIL;
	}
	
	return SUCCESS;

}
EXPORT_SYMBOL(wakeup_flag);
/*----------------------------------------------------------------------------
 * pad_control_led
 *----------------------------------------------------------------------------*/
int pad_control_led(int on_off)
{
	int err;
	u8 data[5];	

	ECLOG_DBG("%s(%d)\n",__func__,on_off);
	data[0] = 0x09;
	data[1] = 0x03;
	data[2] = 0x03;
	data[3] = 0x00;
	data[4] = 0x00;

	if(on_off == PAD_CONTROL_LED_ON) {
		data[3] = 0x01;
	}
	else if (on_off == PAD_CONTROL_LED_OFF) {
		data[3] = 0x00;
	}
	else if(on_off == EC_CONTROL_DUAL_LED_ON){
		data[3] = 0x02;
	}
	else {
		ECLOG_ERR("%s(%d), command fail\n", __func__,on_off);
		return FAIL;
	}

	err = i2c_write_byte(data, sizeof(data));
	if(err < 0)
	{
		EC_EI2CXF_MSG;
		return FAIL;
	}

	return SUCCESS;
}
EXPORT_SYMBOL(pad_control_led);
//------------------------------
int set_dual_orange_led(int on_off)
{
	int	cmd = 0xb4;
	u8	R_data[4] = {0};
	u8	W_data[5] = {0};

/*set PWM to GPIO control*/
	W_data[0] = 0x70;
	W_data[1] = 0x09;
	W_data[2] = 0xCF;
       W_data[3] = 0x00;
       W_data[4] = 0x00;
       if(i2c_write_byte(W_data,sizeof(W_data)))
       {
            ECLOG_ERR("%s,FAIL,LED BLUE\n",__func__);
            return FAIL;
       }

/*Get LED Status*/
       if(i2c_read_byte(cmd, R_data, sizeof(R_data)))
        {
	        ECLOG_ERR("%s,fail to read\n",__func__);
	        return FAIL;
        }
/*Set new LED Status*/
       W_data[0] = 0x4e;
       W_data[1] = 0x03;
       W_data[2] = cmd;
       if(on_off == SET_LED_ON)
           W_data[3] = R_data[3] | (0x1<<5);
       else
           W_data[3] = R_data[3] & ~(0x1<<5);
       W_data[4] = 0x00;
       if(i2c_write_byte(W_data, sizeof(W_data)))
        {
        	ECLOG_ERR("%s,FAIL, LED_Dual_Orange\n",__func__);
           	return FAIL;
        }
       else 
       {
       	ECLOG_DBG("%s,PASS, LED_Dual_Orange\n",__func__);
       }
	return SUCCESS;
}
EXPORT_SYMBOL(set_dual_orange_led);
int set_dual_blue_led(int on_off)
{
	int	cmd = 0xb4;
	u8	R_data[4] = {0};
	u8	W_data[5] = {0};
/*set PWM to GPIO control*/
	W_data[0] = 0x70;
	W_data[1] = 0x09;
	W_data[2] = 0xCF;
       W_data[3] = 0x00;
       W_data[4] = 0x00;
       if(i2c_write_byte(W_data,sizeof(W_data)))
       {
            ECLOG_ERR("%s,FAIL,LED BLUE\n",__func__);
            return FAIL;
       }
	
/*Get LED Status*/
       if(i2c_read_byte(cmd, R_data, sizeof(R_data)))
        {
	        ECLOG_ERR("%s,fail to read\n",__func__);
            return FAIL;
        }
/*Set new LED Status*/
       W_data[0] = 0x4e;
       W_data[1] = 0x03;
       W_data[2] = cmd;
       if(on_off == SET_LED_ON)
           W_data[3] = R_data[3] | (0x1);
       else
           W_data[3] = R_data[3] & ~(0x1);
       W_data[4] = 0x00;
       if(i2c_write_byte(W_data, sizeof(W_data)))
        {
        	ECLOG_ERR("%s,FAIL, LED_Dual_Blue\n",__func__);
            return FAIL;
        }
       else 
       {
       	ECLOG_DBG("%s,PASS, LED_Dual_Blue\n",__func__);
       }
	return SUCCESS;
}
EXPORT_SYMBOL(set_dual_blue_led);
//------------------------------
//low batt capacity setting
int set_low_bat_cap(int low_lv,int critical_low_lv)
{
	uint8_t W_data[5] = {0};
	W_data[0] = 0x09;	//for EC spec,sub_id
	W_data[1] = 0x03;	
	W_data[2] = 0x07;		//CMD
	W_data[3] = low_lv;
	W_data[4] = critical_low_lv;

	ECLOG_DBG("%s(%d,%d)\n",__func__,low_lv,critical_low_lv);
	if(critical_low_lv < low_lv)
	{
		if( i2c_write_byte(W_data,sizeof(W_data)) < 0 )
		{
			
			return FAIL;
		}
	} else {
		ECLOG_ERR("crisis low battery must lower than low battery\n");
		return FAIL;
	}

	return SUCCESS;
}
EXPORT_SYMBOL(set_low_bat_cap);

#if 1
//enable/disable sdcard plug and unplug as pad wakeupsource
int set_sdcard_wakeup_src(int enable)
{
	int flag = 0;
	int ret;

	ECLOG_DBG("%s(%d)\n",__func__,enable);
	ret = get_wakeup_flag(&flag);
	if(ret < 0)
	{
		ECLOG_ERR("%s, get wakeup flag  FAIL\n",__func__);
		return FAIL;
	}

	if(enable == 1)
		flag = flag | (1<<2);
	else if(enable == 0)
		flag = flag & ~(1<<2);
	else
	{
		ECLOG_ERR("%s(%d),wrong parameter!",__func__,enable);
		return FAIL;
	}
	
	ret = set_wakeup_flag(flag);
	if(ret < 0)
		ECLOG_ERR("%s, set wakeup flag FAIL\n",__func__);
	
	return SUCCESS;
}
EXPORT_SYMBOL(set_sdcard_wakeup_src);

//Dock charging timeout time setting
int set_dock_chg_timeout(int time)
{
	u8 w_data[5];
	ECLOG_DBG("%s(%d)\n",__func__,time);
	w_data[0] = 0x09;
	w_data[1] = 0x03;
	w_data[2] = 0x06;
	w_data[3] = time & 0xff;
	w_data[4] = (time >> 8) & 0xff;
	if(i2c_write_byte(w_data, sizeof(w_data)) < 0)
	{
		EC_EI2CXF_MSG;
		return FAIL;
	}

	return SUCCESS;
}
EXPORT_SYMBOL(set_dock_chg_timeout);
#endif

/*----------------------------------------------------------------------------
 * det_work_handler
 *----------------------------------------------------------------------------*/
static void det_work_handler(struct work_struct *_work)
{
	int dock_ac_type;
	int err;
	ECLOG_DBG("%s\n",__func__);
	if(ec_in_S3)
		err = ec_state_change(EC_State_S3);
	else
		err = ec_state_change(EC_State_S0);

	if(err == FAIL)
		return;
	if(get_ac_type(&dock_ac_type)<0)
		ECLOG_ERR("%s, get_ac_type failed",__func__);
	else
	{
		report_event(dock_ac_type,"AC_TYPE");
	}
	tp_enable(TP_status);
	check_ec_version();
}
/*----------------------------------------------------------------------------
 * critical_work_handler
 *----------------------------------------------------------------------------*/
static void critical_work_handler(struct work_struct *_work)
{
	ECLOG_DBG("%s",__func__);
	if(ec_in_S3)
	{
		ECLOG_DBG("s3:%d\n",ec_in_S3);
		ec_state_change(EC_State_S3);
	}
	else
	{
		ECLOG_DBG("s3:%d\n",ec_in_S3);
		ec_state_change(EC_State_S0);
		tp_enable(TP_status);
	}
}

void otg_power_switch_notifier(void)
{
	ECLOG_INFO("%s:OTG PWR_SRC Switch! Reset Touch Pad.\n",__func__);
	msleep(500);
	tp_enable(TP_status);
}
EXPORT_SYMBOL(otg_power_switch_notifier);
/*-------------------
 *wakeup/sleep interval
 *------------------*/
 int wakeup_sleep_interval(int time)
{
	uint8_t W_data[5] = {0};

	ECLOG_DBG("%s(%d)\n",__func__,time);
	W_data[0] = 0x09;	//for EC spec,sub_id
	W_data[1] = 0x03;	
	W_data[2] = 0x04;		//CMD
	W_data[3] = time & 0xff;	
	W_data[4] = (time >> 8) & 0xff;

	ECLOG_DBG("w_data[3]:0x%x\n",W_data[3]);
	ECLOG_DBG("w_data[4]:0x%x\n",W_data[4]);

	if( i2c_write_byte(W_data,sizeof(W_data)) < 0 )
	{
		EC_EI2CXF_MSG;
		return FAIL;
	}	
	
	return SUCCESS;
}
EXPORT_SYMBOL(wakeup_sleep_interval);
/*----------------------------------------------------------------------------
 * ec_early_suspend/ec_late_resume
 *----------------------------------------------------------------------------*/
#ifdef CONFIG_WAKELOCK
void ec_early_suspend(struct early_suspend *h)
{
	struct  EC_platform_data*  ec_data;
	ec_data = ec.pdata;
	
	ECLOG_INFO("enter %s\n",__func__);
	if(DKIN_DET(ec_data->docking_det_irq))
	{		
		if(ec_state_change(EC_State_S3))
			ECLOG_ERR("%s:dock_ec_state_change failed!\n",__func__);
		ec_in_S3 = 1;
	}
}

void ec_late_resume(struct early_suspend *h)
{
	struct  EC_platform_data*  ec_data;
	ec_data = ec.pdata;

	ECLOG_INFO("enter %s\n",__func__);
	if(DKIN_DET(ec_data->docking_det_irq))
	{
		if(!gpio_get_value(ec_data->EC_irq))
			schedule_delayed_work(&ec.work, 0);
		ec_in_S3 = 0;
		schedule_delayed_work(&ec.critical_work,0);
	}
}
#endif


/*----------------------------------------------------------------------------
 * ec_suspend
 *----------------------------------------------------------------------------*/
static int ec_suspend(struct i2c_client *client, pm_message_t mesg)
{
	int ret = 0;
	struct  EC_platform_data*  ec_data;
	ec_data = client->dev.platform_data;
	
	if(DKIN_DET(ec_data->docking_det_irq))
	{
		dock_connected_ok = 0;
		if (device_may_wakeup(&ec_platform_dev->dev)) {
			enable_irq_wake(gpio_to_irq(ec.pdata->EC_irq));
		}
	}
	if (device_may_wakeup(&ec_platform_dev->dev)) {
			enable_irq_wake(gpio_to_irq(ec_data->docking_det_irq));
	}

	return ret;
}
/*----------------------------------------------------------------------------
 * ec_resume
 *----------------------------------------------------------------------------*/
static int ec_resume(struct i2c_client *client)
{
	int ret = 0;
	struct  EC_platform_data*  ec_data;
	ec_data = client->dev.platform_data;

	if(DKIN_DET(ec_data->docking_det_irq))
	{
		dock_connected_ok = 1;
		if(!gpio_get_value(ec.pdata->EC_irq))
			schedule_delayed_work(&ec.work, 0);
		if (device_may_wakeup(&ec_platform_dev->dev)) {
			disable_irq_wake(gpio_to_irq(ec.pdata->EC_irq));
		}
	}
	if (device_may_wakeup(&ec_platform_dev->dev)) {
			disable_irq_wake(gpio_to_irq(ec_data->docking_det_irq));
	}
	return ret;
}
/*----------------------------------------------------------------------------
 * ec_shutdown
 *----------------------------------------------------------------------------*/
 static void ec_shutdown(struct i2c_client *client)
{
	int ret = 0;
    
	struct  EC_platform_data*  ec_data;
	ec_data = client->dev.platform_data;	

	if(DKIN_DET(ec_data->docking_det_irq))
	{
		ret  = ec_state_change(EC_State_S4);
		ECLOG_INFO("%s\n",__func__);
	}
}
#if KERNEL_2627 
/*-----------------------------------------------------------------------------
 * ec_i2c_id
 *---------------------------------------------------------------------------*/
static const struct i2c_device_id ec_i2c_id[] = {
    { "nuvec", 0 },
    { }
};

/*-----------------------------------------------------------------------------
 * This is the driver that will be inserted.
 *---------------------------------------------------------------------------*/
static struct i2c_driver chip_driver = {
    .driver = {
        .name	= "nuvec",
    },
    .probe  	= ec_i2c_prob,
    .remove 	= ec_i2c_remove,
    .id_table	= ec_i2c_id,
    .class		= I2C_CLASS_HWMON,
    .address_list	= normal_i2c,
    .suspend	= ec_suspend,
    .resume	= ec_resume,
    .shutdown	= ec_shutdown,
  
};
#endif

/*----------------------------------------------------------------------------
 *ec_ioctl
 *---------------------------------------------------------------------------*/
#define EC_IOCTL_BASE 77
#define FW_read _IOWR( EC_IOCTL_BASE, 0, int * )
#define FW_write _IOW( EC_IOCTL_BASE, 1, int *)
#define KBD_matrix_write _IOR(EC_IOCTL_BASE,2, int * )

static int ec_read_byte(int cmd, uint8_t *buf)
{
    	#define MAX_BYTE_COUNT	7
	int ret = 0;

	uint8_t read_cmd = cmd;
	struct i2c_msg msg[2];

	msg[0].addr = EC_SMB_ADDRESS;
    	msg[0].flags = 0;
       msg[0].len = 1;
    	msg[0].buf = (char *)(&read_cmd);	//0xb7
	
	msg[1].addr = EC_SMB_ADDRESS;
   	msg[1].flags = 1;
   	msg[1].len = MAX_BYTE_COUNT;
	msg[1].buf = buf;

	ret = i2c_transfer(ec.I2cClient->adapter, msg, 2);
	if( ret < 0 ){
		return ret;
	}

	return ret;	//0116
	
}

static int ec_write_byte(u8 *write_data, int size)
{
	
	struct i2c_msg msg[1];
	int ret = 0;


	msg->addr = EC_SMB_ADDRESS;
	msg->flags = 0;			// 0 write
	msg->len = size;
	msg->buf = write_data;
	
	ret = i2c_transfer(ec.I2cClient->adapter, msg, 1);
	return ret;
}

static  long ec_ioctl( struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	u8 data[100];
	unsigned char* KBD_matrix;
	memset(data,0,100);
	switch(cmd)
	{
		case FW_read:	
			if( copy_from_user( data, (void __user*) arg, sizeof(data)) )
				return -EFAULT;
			ret = ec_read_byte(data[0], data);

			if( copy_to_user( (void __user*) arg, data, sizeof(data) ))
				return -EFAULT;
			return ret;
		case FW_write:
			if( copy_from_user( data, (void __user*) arg, sizeof(data)) )
				return -EFAULT;
			ret = ec_write_byte(data, sizeof(data));	/*34*/
			return ret;
		case KBD_matrix_write:
			KBD_matrix = set1_keycode;
			if(copy_to_user((void __user*) arg, KBD_matrix, 512))
				return -EFAULT;
			break;		
		default:
			break;
	}

	return ret;
}

/*-----------------------------------------------------------------------------
 * nuvec_fops
 *---------------------------------------------------------------------------*/
const struct file_operations nuvec_fops = {
    .owner  = THIS_MODULE,
    //--------
    .unlocked_ioctl = ec_ioctl,
    //--------
};

/*-----------------------------------------------------------------------------
 * nuvec_dev
 *---------------------------------------------------------------------------*/
static struct miscdevice nuvec_dev = {
    NUVEC_MINOR,
    "nuvec",
    &nuvec_fops
};

/*-----------------------------------------------------------------------------
 * nuvec_init
 *---------------------------------------------------------------------------*/
static int __init nuvec_init(void)
{
     
	int err = 0;

	INIT_DELAYED_WORK(&ec.work, ec_work_handler);
	INIT_DELAYED_WORK(&ec.det_work, det_work_handler);	
	INIT_DELAYED_WORK(&ec.critical_work, critical_work_handler);
	err = misc_register(&nuvec_dev);
	if (err) {
		ECLOG_ERR("misc register fail\n");
	goto fail;
	}

	err = gpio_request(Dock_Reset_Pin, "Dock_reset_en");
	if(err < 0)
	{
		ECLOG_ERR("%s:gpio_request error\n",__func__);
		goto fail;
	}

	ECLOG_DBG("naux: init\n");

	err = i2c_add_driver(&chip_driver);
	if (err)
	{
		ECLOG_ERR( "naux: err i2c add driver\n");
		goto fail;
	}

	return 0;

fail:
	gpio_free(Dock_Reset_Pin);
	i2c_del_driver(&chip_driver);	
	return err;
}


static void __exit nuvec_exit(void)
{
    ECLOG_DBG("naux: exit\n");
    misc_deregister( &nuvec_dev );
    i2c_del_driver(&chip_driver);
    input_unregister_device(nmouse_dev);
    input_unregister_device(nkbd_dev);
}


module_init(nuvec_init);
module_exit(nuvec_exit);
