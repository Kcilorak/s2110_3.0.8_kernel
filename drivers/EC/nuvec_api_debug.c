/*
 *	EC API debug sysfs
 *
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/ctype.h>
#include <asm/io.h>
#include <linux/i2c.h>
#include <linux/nuvec_api.h>
#include <linux/platform_device.h>

#include <linux/rtc.h>
struct rtc_device *rtc = NULL;
int suspend_time_invl = 0;	// 0 as disable


#define EC_API_PROC_NAME "ec_api_debug"
#define IRQ_NUM 17

static int _next_str(char **, char *);

static struct proc_dir_entry *ec_api_dir;
static char* ec_api_entry_name[] = { "get_ac", "boost_ctl", "bat_info","charging_req",
									"usb_used","register_ec_event","usb_bypass",
									"usb_hub_reset","dock_status","ec_charging_setting",
									"charging_info","adc_id","otg_pwr_switch","isn",
									"dock_data","pad_config","dock_reset",
									"touchpad_switch","SD_card_switch","usb_hub_switch",
									"wakeup_flag","wakeup_sleep_interval","ec_bulk_bat_info",
									"set_wakeup_flag", "set_low_bat_cap","set_dual_orange_led",
									"set_dual_blue_led","set_pad_control_led_lv","ssn",
									"bat_manu_info",
#if 0
									"set_sdcard_wakeup_src", "set_dock_chg_timeout",
#endif
#ifdef CONFIG_EC_API_DEBUG
									"rtc_wakeup_time"
#endif
								};
#define API_CNT sizeof(ec_api_entry_name)/sizeof(char*)

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


static int get_ac_read(char *buf, char **start, off_t offset, int count, int *eof, void *data)
{
	int type = 0;
	int ret ;
	ret  = get_ac_type(&type);
	ret = sprintf(buf,"AC_type:%d (%d:AC_none, %d:AC_5V, %d:AC_12V, %d:AC_USB , %d:AC_INFORMAL)\n", 
				type,AC_NONE,AC_5V,AC_12V,AC_USB, AC_INFORMAL);
	return ret;
}
static int boost_ctl_write(struct file *file, const char __user *buffer, unsigned long count, void *data)
{
	char *ep;
	int ret;

	char *buf = (char *)buffer;
	int ref_1,ref_2;

 	ref_1 = simple_strtol(buf, &ep, 16);
	if (_next_str(&buf, ep)) {
		return count;
	}

	 ref_2 = simple_strtol(buf, &ep, 16);
     
	ret = ec_boost_ctl(ref_1, ref_2);
	if(ret == SUCCESS)
		printk("success\n");
        else
              printk("failed\n");
	return count;
}

static int boost_ctl_read(char *buf, char **start, off_t offset, int count, int *eof, void *data)
{
	int ret = 0;
	int sys_5v = 0;
	int otg_5v = 0;

	ret = get_boost_status(&otg_5v, &sys_5v);
	if(ret == SUCCESS)
		printk("success\n");
        else
              printk("failed\n");
	
	printk("otg_5V:%d sys_5v:%d (1:on, 0:off)\n", otg_5v, sys_5v);	
	ret = sprintf(buf,"otg_5V:%d sys_5v:%d\n", otg_5v, sys_5v);
	
	return ret;
}

static int bat_info_write(struct file *file, const char __user *buffer, unsigned long count, void *data)
{
	char *buf = (char *)buffer;
	char *ep;
	int ref_1;
	int ret ;
	int bat_info;
 	ref_1 = simple_strtol(buf, &ep, 16);

ret = get_ec_bat_info(&bat_info, ref_1);
		if(ret == SUCCESS)
		{
			printk("BAT_INFO:0x%x\n", bat_info);
			printk("success\n");
		}        else{
              printk("failed\n");
}
	return count;
}

/*--------------------------------
*
----------------------------------*/
static int bat_manu_info_read(char *buf, char **start, off_t offset, int count, int *eof, void *data)
{
	int ret = 0;
	u8 buff[5] = {0};

	ret = battery_manufacture_info_read(buff);
	if(ret == SUCCESS)
		printk("success\n");
	else
		printk("failed");

	ret = sprintf(buf,"buff[1]:0x%x, buff[2]:0x%x, buff[3]:0x%x, buff[4]:0x%x \n", buff[1], buff[2], buff[3], buff[4]);
	return ret;
}

static int charging_req_write(struct file *file, const char __user *buffer, unsigned long count, void *data)
{
	char *ep;
	char *buf = (char *)buffer;
	int ref_1;
	int ret;

	 ref_1 = simple_strtol(buf, &ep, 16);

	ret =ec_charging_req(ref_1);
	if(ret == SUCCESS)
		printk("success\n");
        else
              printk("failed\n");
	
	return count;
}

static int charging_req_read(char *buf, char **start, off_t offset, int count, int *eof, void *data)
{
	int ret = 0;
	int ec_charging_status = 0;

	ret = get_ec_charging_req_status(&ec_charging_status);
	
	if(ret == SUCCESS)
		printk("success\n");
        else
              printk("failed\n");
		
	printk("ec_charging_status:%d (1:charging on, 0:charging off) \n", ec_charging_status);
	ret = sprintf(buf,"ec_charging_status:%d \n", ec_charging_status);
	
	return ret;
}

static int usb_used_write(struct file *file, const char __user *buffer, unsigned long count, void *data)
{
	char *ep;
	char *buf = (char *)buffer;
	int ref_1;
	int ret ;

 	ref_1 = simple_strtol(buf, &ep, 16);

	ret = usb_in_used(ref_1);
	if(ret == SUCCESS)
		printk("success\n");	
        else
              printk("failed\n");
	return count;
}

static int usb_used_read(char *buf, char **start, off_t offset, int count, int *eof, void *data)
{
    int ret =0;
    int usb_dev_count = 0;

    ret = get_usb_in_used(&usb_dev_count);
    printk("usb dev in use ciount : %d\n",usb_dev_count);
    ret = sprintf(buf,"usb dev in use ciount : %d\n",usb_dev_count);
    return ret;
}


void test_func(int type)
{
	printk("[EC API DBG]EC_EVENT is :%d\n",type);
}

static int register_ec_event_write(struct file *file, const char __user *buffer, unsigned long count, void *data)
{
	char *ep;
	char *buf = (char *)buffer;
	int ref_1;
 	ref_1 = simple_strtol(buf, &ep, 10);

	if(ref_1 == 1)
	{
		register_ec_event_func(&test_func);
	}else{
		register_ec_event_func(NULL);
	}
	return count;	
}

//-------------0105
static int usb_bypass_write(struct file *file, const char __user *buffer, unsigned long count, void *data)	
{
	char *ep;
	char *buf = (char *)buffer;
	int ref_1;
	int ret = 0;

 	ref_1 = simple_strtol(buf, &ep, 10);

	ret = set_usb_bypass_en(ref_1);

	if(ret == SUCCESS)
		printk("success\n");
        else
              printk("failed\n");
	
	return count;
}

static int usb_bypass_read(char *buf, char **start, off_t offset, int count, int *eof, void *data)
{
	int ret = 0;
	int usb_bypass_status = 0;

	ret = get_usb_bypass_en(&usb_bypass_status);

	if(ret == SUCCESS)
		printk("success\n");
       else
              printk("failed\n");

	printk("usb_bypass_status:%d (1:usb_bypass_high, 0:usb_bypass_low)\n", usb_bypass_status);
	ret = sprintf(buf,"usb_bypass_status:%d \n", usb_bypass_status);	

	return ret;
}
//--------------
static int usb_hub_reset_write(struct file *file, const char __user *buffer, unsigned long count, void *data)
{
	char *ep;
	char *buf = (char *)buffer;
	int ref_1;
	int ret = 0;

 	ref_1 = simple_strtol(buf, &ep, 16);

	ret = usb_hub_reset(ref_1);

	if(ret == SUCCESS)
		printk("success\n");
        else
              printk("failed\n");
	
	return count;
}

static int dock_status_read(char *buf, char **start, off_t offset, int count, int *eof, void *data)
{
	int ret = 0;
	int status = 0;

	ret = dock_status(&status);

	ret = sprintf(buf,"ret = %d \ndock_status:0x%x (bit_0:TP_ERR, bit_1:BATT_ERR, bit_2:PADIN_ERR,bit 7:DOCK_I2C_FAIL, bit 8:NO_DOCK)\n", ret,status);
	
	return ret;
}

static int ec_charging_setting_write(struct file *file, const char __user *buffer, unsigned long count, void *data)
{
	char *ep;
	char *buf = (char *)buffer;
	int ref_1,ref_2;
	int ret;

	 ref_1 = simple_strtol(buf, &ep, 16);

	if (_next_str(&buf, ep)) {
		return count;
	}

	 ref_2 = simple_strtol(buf, &ep, 16); 

	ret =ec_charging_setting(ref_1, ref_2);
	if(ret == SUCCESS)
		printk("success\n");
        else
              printk("failed\n");
	
	return count;	
}

static int charging_info_write(struct file *file, const char __user *buffer, unsigned long count, void *data)
{
	char *buf = (char *)buffer;
	char *ep;
	int ref_1;
	int ret ;
	int bat_info;
 	ref_1 = simple_strtol(buf, &ep, 16);

ret = get_ec_charging_info(&bat_info, ref_1);
		if(ret == SUCCESS)
		{
			printk("CHG_INFO:0x%x\n", bat_info);
			printk("success\n");
		} else {
              	printk("failed\n");
		}
	return count;
}

static int adc_id_read(char *buf, char **start, off_t offset, int count, int *eof, void *data)
{
	int ret = 0;
	int id;

	ret = ec_get_adc_id(&id);
	if(ret == SUCCESS)
		printk("success\n");
        else
              printk("failed\n");
	
	ret = sprintf(buf,"id:0x%x \n", id);
	
	return ret;
}

static int otg_pwr_switch(char *buf, char **start, off_t offset, int count, int *eof, void *data)
{
	int ret = 0;
	otg_power_switch_notifier();
	ret = sprintf(buf,"%s done",__func__);
	return ret;
}

static int isn_read(char *buf, char **start, off_t offset, int count, int *eof, void *data)
{
	int ret = 0;
	char buffer[14] = {0};
	ret = dock_isn_read(buffer);
	printk("%s:%s\n",__func__,buffer);
	if(ret == SUCCESS)
		printk("success\n");
        else
              printk("failed\n");
	ret = sprintf(buf,"isn:%s\n",buffer);
	return ret;
}


static int isn_write(struct file *file, const char __user *buffer, unsigned long count, void *data)
{
	int ret;
	char *buf = (char *)buffer;
	ret = dock_isn_write(buf);
	if(ret == SUCCESS)
		printk("success\n");
        else
              printk("failed\n");
		
	return count;	
}
/*---------------------------
*ssn	read/write
*----------------------------*/
static int ssn_read(char *buf, char **start, off_t offset, int count, int *eof, void *data)
{
	int ret = 0;
	char buffer[14] = {0};
	ret = dock_ssn_read(buffer);
	printk("%s:%s\n",__func__,buffer);
	if(ret == SUCCESS)
		printk("success\n");
        else
              printk("failed\n");
	ret = sprintf(buf,"isn:%s\n",buffer);
	return ret;
}


static int ssn_write(struct file *file, const char __user *buffer, unsigned long count, void *data)
{
	int ret;
	char *buf = (char *)buffer;
	ret = dock_ssn_write(buf);
	if(ret == SUCCESS)
		printk("success\n");
        else
              printk("failed\n");
		
	return count;	
}

static int dockdata_read(char *buf, char **start, off_t offset, int count, int *eof, void *data)
{
	int ret =0;
	int dockdata;
	ret = dock_data_read(&dockdata);
	if(ret == SUCCESS)
		printk("success\n");
        else
              printk("failed\n");	
	ret = sprintf(buf,"%d\n",dockdata);
	return ret;
}

static int dockdata_write(struct file *file, const char __user *buffer, unsigned long count, void *data)
{
	int ret;
	int ref_1;
	char *ep;
	char *buf = (char *)buffer;
 	ref_1 = simple_strtol(buf, &ep, 10);

	printk("dockdata:%d\n",ref_1);
	ret = dock_data_write(ref_1);
	if(ret == SUCCESS)
		printk("success\n");
        else
              printk("failed\n");	
	return count;
}


static int pad_config_read(char *buf, char **start, off_t offset, int count, int *eof, void *data)
{

        int ret;
        int dock_pad_value_r = 0;
        if(dock_pad_config_read(&dock_pad_value_r) == SUCCESS)
        {
                ret = sprintf(buf,"%d\n", ~dock_pad_value_r);
                printk(KERN_ERR "%s sucess: buffer = %s, lval = %d\n",__func__,buf,~dock_pad_value_r);
		return ret;
        }else
        {
		printk(KERN_ERR "%s failed: buffer = %s, lval = %d\n",__func__,buf,~dock_pad_value_r);
                return -EINVAL;
        }


        return ret;
/*
	int ret =0;
	int pad_config;
	ret = dock_pad_config_read(&pad_config);
	if(ret == SUCCESS)
		printk("success\n");
        else
              printk("failed\n");	
	ret = sprintf(buf,"%d\n",pad_config);
	return ret;
	*/
}

static int pad_config_write(struct file *file, const char __user *buffer, unsigned long count, void *data)
{
	int dock_pad_value_w = 0;
	int ref_1;
	char *ep;
	 ref_1 = simple_strtol(buffer, &ep, 10);

	dock_pad_value_w = ref_1;
        if(dock_pad_config_write(~dock_pad_value_w) == SUCCESS)
        {
		printk(KERN_ERR "[%s] write success. lval = %d\n",__func__,~dock_pad_value_w);
                return count;
        }else
        {
		printk(KERN_ERR "[%s] write failed. lval = %d\n",__func__,~dock_pad_value_w);
                return -EINVAL;
        }

        return count;

/*
	int ret;
	int ref_1;
	char *ep;
	char *buf = (char *)buffer;
 	ref_1 = simple_strtol(buf, &ep, 10);

	printk("pad_config:%d\n",ref_1);
	ret = dock_pad_config_write(ref_1);
	if(ret == SUCCESS)
		printk("success\n");
        else
              printk("failed\n");	
	return count;
	*/
}
/*dock_reset*/
static int dock_reset_write(struct file *file, const char __user *buffer, unsigned long count, void *data)
{

    	char *ep;
	char *buf = (char *)buffer;
	int ref;
 	ref = simple_strtol(buf, &ep, 10);
	if(ref == 1)
	{
		ref = dock_reset_func();
       	printk("Dock Reset!\n");
	}

	return count;
	
}
/*touchpad_switch*/
static int touchpad_switch_write(struct file *file, const char __user *buffer, unsigned long count, void *data)
{

    	char *ep;
	char *buf = (char *)buffer;
	int ref,ret;
 	ref = simple_strtol(buf, &ep, 10);
	ret = touchpad_switch(ref);
	if(ret== SUCCESS)
		printk("success\n");
        else
              printk("failed\n");	

	return count;
}
/*sdcard_switch*/
static int sdcard_switch_write(struct file *file, const char __user *buffer, unsigned long count, void *data)
{

    	char *ep;
	char *buf = (char *)buffer;
	int ref,ret;
 	ref = simple_strtol(buf, &ep, 10);
	ret = sdcard_switch(ref);
	if(ret== SUCCESS)
		printk("success\n");
        else
              printk("failed\n");	

	return count;
}
/*usbhub_switch*/
static int usbhub_switch_write(struct file *file, const char __user *buffer, unsigned long count, void *data)
{

    	char *ep;
	char *buf = (char *)buffer;
	int ref,ret;
 	ref = simple_strtol(buf, &ep, 10);
	ret = usb_hub_switch(ref);
	if(ret== SUCCESS)
		printk("success\n");
        else
              printk("failed\n");	

	return count;
}
/*wakeup_flag_write*/
static int wakeup_flag_write(struct file *file, const char __user *buffer, unsigned long count, void *data)
{

    	char *ep;
	char *buf = (char *)buffer;
	int ref,ret;
 	ref = simple_strtol(buf, &ep, 10);
	ret = wakeup_flag(ref);
	if(ret== SUCCESS)
		printk("success\n");
        else
              printk("failed\n");	

	return count;
}
/*wakeup_sleep_interval_write*/
static int wakeup_sleep_interval_write(struct file *file, const char __user *buffer, unsigned long count, void *data)
{
    	char *ep;
	char *buf = (char *)buffer;
	int ref,ret;
 	ref = simple_strtol(buf, &ep, 10);
	ret = wakeup_sleep_interval(ref);
	if(ret== SUCCESS)
		printk("success\n");
        else
              printk("failed\n");	

	return count;
}
static int wakeup_flag_read(char *buf, char **start, off_t offset, int count, int *eof, void *data)
{
	int ret =0;
	int flag;
	ret = get_wakeup_flag(&flag);
	if(ret == SUCCESS)
		printk("success\n");
        else
              printk("failed\n");	
	ret = sprintf(buf,"%d\n",flag);
	return ret;
}

static int wakeup_sleep_interval_read(char *buf, char **start, off_t offset, int count, int *eof, void *data)
{
	int ret =0;
	int time;
	ret = get_wakeup_sleep_interval(&time);
	if(ret == SUCCESS)
		printk("success\n");
        else
              printk("failed\n");	
	ret = sprintf(buf,"%d\n",time);
	return ret;
}


static int ec_bulk_bat_info_read(char *buf, char **start, off_t offset, int count, int *eof, void *data)
{
	int ret =0;
	struct bat_data ec_bat ;
	ret = get_ec_battery_data(&ec_bat);
	if(ret == SUCCESS)
		printk("success\n");
        else
              printk("failed\n");	
	ret = sprintf(buf,"%d|%d|%d|%d|%d|%d|%d \n",ec_bat.voltage, ec_bat.relative_state_of_charge,
		ec_bat.tempture,ec_bat.flag, ec_bat.remain_capacity, ec_bat.full_charge_capacity, ec_bat.average_current);
	return ret;
}

static int set_low_bat_cap_write(struct file *file, const char __user *buffer, unsigned long count, void *data)
{
	char *ep;
	char *buf = (char *)buffer;
	int ref1,ref2,ret;
 	ref1 = simple_strtol(buf, &ep, 10);
	if (_next_str(&buf, ep)) {
		return count;
	}
	ref2 = simple_strtol(buf, &ep, 10);
	ret = set_low_bat_cap(ref1,ref2);
	if(ret== SUCCESS)
		printk("success\n");
        else
              printk("failed\n");	
	return count;
}

#if 0
static int set_sdcard_wakeup_src_write(struct file *file, const char __user *buffer, unsigned long count, void *data)
{
    	char *ep;
	char *buf = (char *)buffer;
	int ref,ret;
 	ref = simple_strtol(buf, &ep, 10);
	ret = set_sdcard_wakeup_src(ref);
	if(ret== SUCCESS)
		printk("success\n");
        else
              printk("failed\n");	
	return count;
}
static int set_dock_chg_timeout_write(struct file *file, const char __user *buffer, unsigned long count, void *data)
{
	char *ep;
	char *buf = (char *)buffer;
	int ref,ret;
 	ref = simple_strtol(buf, &ep, 10);
	ret = set_dock_chg_timeout(ref);
	if(ret== SUCCESS)
		printk("success\n");
        else
              printk("failed\n");	
	return count;
}
#endif
static int set_wakeup_flag_read(char *buf, char **start, off_t offset, int count, int *eof, void *data)
{
	int ret =0;
	int flag;
	ret = get_wakeup_flag(&flag);
	if(ret == SUCCESS)
		printk("success\n");
        else
              printk("failed\n");	
	ret = sprintf(buf,"%d\n",flag);
	return ret;
}

static int set_wakeup_flag_write(struct file *file, const char __user *buffer, unsigned long count, void *data)
{
    	char *ep;
	char *buf = (char *)buffer;
	int ref,ret;
 	ref = simple_strtol(buf, &ep, 10);
	ret = set_wakeup_flag(ref);
	if(ret== SUCCESS)
		printk("success\n");
        else
              printk("failed\n");	

	return count;
}
static int set_dual_orange_led_write(struct file *file, const char __user *buffer, unsigned long count, void *data)
{
    	char *ep;
	char *buf = (char *)buffer;
	int ref,ret;
 	ref = simple_strtol(buf, &ep, 10);
	ret = set_dual_orange_led(ref);
	if(ret== SUCCESS)
		printk("success\n");
        else
              printk("failed\n");	
	return count;
}

static int set_dual_blue_led_write(struct file *file, const char __user *buffer, unsigned long count, void *data)
{
    	char *ep;
	char *buf = (char *)buffer;
	int ref,ret;
 	ref = simple_strtol(buf, &ep, 10);
	ret = set_dual_blue_led(ref);
	if(ret== SUCCESS)
		printk("success\n");
        else
              printk("failed\n");	
	return count;
}

static int set_pad_control_led_lv_write(struct file *file, const char __user *buffer, unsigned long count, void *data)
{
    	char *ep;
	char *buf = (char *)buffer;
	int ref,ret;
 	ref = simple_strtol(buf, &ep, 10);
	ret = pad_control_led(ref);
	if(ret== SUCCESS)
		printk("success\n");
        else
              printk("failed\n");	
	return count;
}

#ifdef CONFIG_EC_API_DEBUG
static int rtc_wakeup_time_read(char *buf, char **start, off_t offset, int count, int *eof, void *data)
{
	int ret = 0;
	ret = sprintf(buf,"rtc alarm %s | time=%d\n", suspend_time_invl?"enabled":"disabled",suspend_time_invl);
	return ret;
}

static int rtc_wakeup_time_write(struct file *file, const char __user *buffer, unsigned long count, void *data)
{
    	char *ep;
	char *buf = (char *)buffer;
	int ref;
 	ref = simple_strtol(buf, &ep, 10);
	suspend_time_invl = ref;

	return count;
}

static int ec_debug_probe(struct platform_device *pdev)
{
	printk("%s, %s\n",__func__,pdev->name);
	return 0;
}
#endif

static int entry_create(void)
{
	int 	i;
	struct proc_dir_entry *ec_api_entry[API_CNT];
	ec_api_dir = proc_mkdir( EC_API_PROC_NAME , NULL);
	if( ec_api_dir == NULL ) {
		printk("proc mkdir failed\n");
		return -1;
	}

	for(i = 0 ; i < API_CNT ; i++)
	{
		ec_api_entry[i]= create_proc_entry(ec_api_entry_name[i] , 0666 , ec_api_dir);

		if (!ec_api_entry[i]) {
			printk("create proc entry failed\n" );
			return -1;
		}
	}

	ec_api_entry[0]->read_proc 	= get_ac_read;
	ec_api_entry[1]->write_proc 	= boost_ctl_write;
	ec_api_entry[1]->read_proc		= boost_ctl_read;
	ec_api_entry[2]->write_proc 	= bat_info_write;
	ec_api_entry[3]->write_proc 	= charging_req_write;
	ec_api_entry[3]->read_proc		= charging_req_read;
	ec_api_entry[4]->write_proc 	= usb_used_write;
	ec_api_entry[4]->read_proc  	= usb_used_read;
	ec_api_entry[5]->write_proc 	= register_ec_event_write;
	ec_api_entry[6]->write_proc		= usb_bypass_write;		
	ec_api_entry[6]->read_proc		= usb_bypass_read;
       ec_api_entry[7]->write_proc       = usb_hub_reset_write;
	ec_api_entry[8]->read_proc		= dock_status_read;
	ec_api_entry[9]->write_proc		= ec_charging_setting_write;
	ec_api_entry[10]->write_proc 	= charging_info_write;	   
	ec_api_entry[11]->read_proc 	= adc_id_read;
	ec_api_entry[12]->read_proc	= otg_pwr_switch;
	ec_api_entry[13]->read_proc	= isn_read;
	ec_api_entry[13]->write_proc	= isn_write;
	ec_api_entry[14]->read_proc	= dockdata_read;
	ec_api_entry[14]->write_proc	= dockdata_write;
	ec_api_entry[15]->read_proc	= pad_config_read;
	ec_api_entry[15]->write_proc	= pad_config_write;
	ec_api_entry[16]->write_proc	= dock_reset_write;
	ec_api_entry[17]->write_proc	= touchpad_switch_write;
	ec_api_entry[18]->write_proc	= sdcard_switch_write;
	ec_api_entry[19]->write_proc	= usbhub_switch_write;
	ec_api_entry[20]->write_proc	= wakeup_flag_write;
	ec_api_entry[20]->read_proc	= wakeup_flag_read;
	ec_api_entry[21]->write_proc	= wakeup_sleep_interval_write;
	ec_api_entry[21]->read_proc	= wakeup_sleep_interval_read;
	ec_api_entry[22]->read_proc	= ec_bulk_bat_info_read;
	ec_api_entry[23]->write_proc	= set_wakeup_flag_write;
	ec_api_entry[23]->read_proc	= set_wakeup_flag_read;
	ec_api_entry[24]-> write_proc	= set_low_bat_cap_write;
	ec_api_entry[25]->write_proc	= set_dual_orange_led_write;
	ec_api_entry[26]->write_proc	= set_dual_blue_led_write;
	ec_api_entry[27]->write_proc	= set_pad_control_led_lv_write;
	ec_api_entry[28]->write_proc	= ssn_write;
	ec_api_entry[28]->read_proc	= ssn_read;
	ec_api_entry[29]->read_proc	= bat_manu_info_read;
#if 0
	ec_api_entry[25]-> write_proc	= set_sdcard_wakeup_src_write;
	ec_api_entry[26]-> write_proc	= set_dock_chg_timeout_write;
#endif
#ifdef CONFIG_EC_API_DEBUG
	ec_api_entry[API_CNT -1]->read_proc	= rtc_wakeup_time_read;
	ec_api_entry[API_CNT -1]->write_proc	= rtc_wakeup_time_write;
#endif

	printk("%s\n",__func__);
	return 0;	
}

#ifdef CONFIG_EC_API_DEBUG
static int has_wakealarm(struct device *dev, void *name_ptr)
{
	struct rtc_device *candidate = to_rtc_device(dev);

	if (!candidate->ops->set_alarm)
		return 0;
	if (!device_may_wakeup(candidate->dev.parent))
		return 0;

	*(const char **)name_ptr = dev_name(dev);
	return 1;
}

static int set_wakeup_alarm(struct rtc_device *rtc_dev,int sleeptime)
{
	/*for set alarm*/
	int ret;
	struct rtc_wkalrm   alm;
	unsigned long		now,curr_alm_time;
	static char err_wakealarm []  =
		KERN_ERR "PM: can't set %s wakealarm, err %d\n";
	if(!rtc_dev)
	{
		printk("%s:rtc pointer is NULL\n",__func__);
		return -1;
	}
	printk("%s\nset alarm\n",__func__);
	rtc_read_time(rtc_dev, &alm.time);
	rtc_tm_to_time(&alm.time, &now);
	memset(&alm, 0, sizeof alm);
	ret = rtc_read_alarm(rtc_dev,&alm);
	rtc_tm_to_time(&alm.time,&curr_alm_time);
	printk("================================================================\n");
	printk("  ret = %d, alarm %s \n",ret,alm.enabled?"enabled":"disabled");
	printk(" now= %lu | curr_alm_time=%lu | our_alm_time=%lu \n",now,curr_alm_time,(now+sleeptime));
	printk("================================================================\n");
	if(ret || (alm.enabled == false) || ((now+sleeptime) < curr_alm_time))
	{
		printk("%s\n+-set alarm\n",__func__);
		rtc_time_to_tm(now + sleeptime, &alm.time);
		alm.enabled = true;
	}
	ret = rtc_set_alarm(rtc_dev, &alm);
	if (ret < 0) {
		printk(err_wakealarm, dev_name(&rtc_dev->dev), ret);
		return ret;
	}
	return 0;
}
#if 0
static int disable_wakeup_alarm(struct rtc_device *rtc_dev)
{
	struct rtc_wkalrm   alm;
	int ret;
	if(!rtc_dev)
	{
		printk("%s:rtc pointer is NULL",__func__);
		return -1;
	}
	printk("%s\n--set alarm",__func__);
	alm.enabled = false;
	ret = rtc_set_alarm(rtc_dev, &alm);
	return ret;
}
#endif

static int ec_debug_suspend(struct platform_device *dev, pm_message_t state)
{
	int ret;
	printk("%s\n",__func__);
	if(suspend_time_invl != 0)
	{
		ret = set_wakeup_alarm(rtc,suspend_time_invl);
		printk("[%s]set_wakeup_alarm %d",__func__,ret);
	}
	return 0;
}

static int ec_debug_resume(struct platform_device *dev)
{
	printk("%s\n",__func__);
	return 0;
}

static int ec_debug_remove(struct platform_device *pdev)
{
	return 0;
}

#if 0
static const struct dev_pm_ops ec_dbg_pm_ops = {
	.suspend	= ec_debug_suspend,
	.resume	= ec_debug_resume,
};
#endif

static struct platform_driver ec_api_dbg_driver = {
	.probe = ec_debug_probe,
	.remove = ec_debug_remove,
	.suspend	= ec_debug_suspend,
	.resume	= ec_debug_resume,
	.driver = { .name = "ec_api_dbg",
			  .owner = THIS_MODULE,
			}
};
#endif
static int __init ec_debug_init(void)
{
#ifdef CONFIG_EC_API_DEBUG
	/*Test RTC*/
	static char             warn_no_rtc[] =
                KERN_WARNING "PM: no wakealarm-capable RTC driver is ready\n";
	char	*pony = NULL;
	printk("=== find RTC ===\n");
	/* RTCs have initialized by now too ... can we use one? */
	class_find_device(rtc_class, NULL, &pony, has_wakealarm);
	if (pony)
		rtc = rtc_class_open(pony);
	if (!rtc) {
		printk(warn_no_rtc);
	}
	platform_driver_register(&ec_api_dbg_driver);
#endif
	entry_create();
	/*Test RTC*/
//	platform_device_register(&ec_dbg_pdata);
	return 0;
}

static void __exit ec_debug_exit(void)
{
int i;
for(i = 0 ; i < API_CNT; i++)
{
	remove_proc_entry( ec_api_entry_name[i], ec_api_dir );
}
remove_proc_entry( EC_API_PROC_NAME , NULL );
	printk("%s\n",__func__);
}

module_init(ec_debug_init);
module_exit(ec_debug_exit);

MODULE_DESCRIPTION("EC API debug tool");
MODULE_LICENSE("GPL");

