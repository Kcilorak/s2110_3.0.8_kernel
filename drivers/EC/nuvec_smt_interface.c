/*
 *	Docking EC SMT sysfs
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
#include <linux/delay.h>

#define EC_SMT_PROC_NAME "ec_smt"
#define EC_SMT_CMD_CNT 16
#define IRQ_NUM 17
#define EC_BUS_DVT 10
#define EC_BUS_PVT 9
#define EC_ADDR 0x19

int EC_BUS=EC_BUS_PVT;

static struct proc_dir_entry *ec_smt_dir;
static char *ec_smt_entry_name[] = { "Dock_Connection_status", "Dock_ISN", "EC_version",
                                                             "LED_Dual_BLUE","LED_Dual_ORANGE", "LED_BLUE",
                                                             "Dock_CHG_status","Dock_CHG_current","Dock_BAT_level",
                                                             "USB_Hub_Switch","Dock_CHG_Req","Touch_pad_status",
                                                             "KBD_ID", "TP_ID","Dock_Reset","Pad_Control_LED"};

int set_result(int result, char* cmd_name,char* ret_val, char* buf)
{
    int ret = 0;
    ret = sprintf(buf,"%s,%s,%s\n",(result?"PASS":"FAIL"),cmd_name,(result?ret_val:" "));	
    return ret;
}

#if 1
static int i2c_write(int bytes, u8 *data)
{
	int err;
	struct i2c_msg msg[1];
	struct i2c_adapter *adap;
	adap = i2c_get_adapter( EC_BUS);
	if (!adap){
		printk("(write)get adapter failed\n");
		return -ENODEV;
	}

	msg->addr = EC_ADDR;
	msg->flags = 0;			// 0 write
	msg->len = bytes;
	msg->buf = data;
    
	err = i2c_transfer(adap, msg, 1);

	if( err < 0 ){
		printk("FAIL\n");
	}
    
	i2c_put_adapter(adap);
	if (err >= 0)
		return 0;
	return err;
}
#endif
static int i2c_read( int bytes, u8* cmd, uint8_t* buf)
{
	int err;
       int i;
	struct i2c_msg msg[2];
	struct i2c_adapter *adap;
       u8 data[bytes];
	memset( buf , 0 , bytes);
	memset( data , 0 , bytes );

	adap = i2c_get_adapter( EC_BUS );
	if (!adap){
		printk("(read)get adapter failed\n");
		return -ENODEV;
	}
 
	msg[0].addr = EC_ADDR;
	msg[0].flags = 0;		// 0 write
	msg[0].len = 1; //EC read CMD only has one byte
	msg[0].buf = data;

	for(i=0 ; i < msg[0].len ; i++)				//1116 add
	{
		data[i] = cmd[i];
	}

	msg[1].addr = EC_ADDR;
	msg[1].flags = 1;	// 1 read
	msg[1].len = bytes;
	msg[1].buf = data;

	err = i2c_transfer(adap, msg, 2);
	if( err < 0 ){
		printk("(read)err: %d\n",err);
	}
	i2c_put_adapter(adap);
	/*---*/
       memcpy(&buf[0], &data[0], bytes );
	/*---*/
	if (err >= 0)
		return 0;
	return err;
}
/*Dock Connection 
    echo 1 10 4 0x19 1 0xb0 > /proc/i2c_debug/i2c */
static int dock_connection_read(char *buf, char **start, off_t offset, int count, int *eof, void *data)
{
    int readbytes = 4;
    u8 cmd = 0xb0;
    u8 ret[readbytes];
    char retstr[4];
    int r;
    if(i2c_read(readbytes, &cmd, ret))
        printk("failed to read!\n");
    r = (ret[2] >> 3) & 0x01;
//    printk("result is %d\n",r);
    sprintf(retstr,"%d",r);
    r = set_result(r, "DOCK_connection",(char*)retstr, buf);
    return r;
}

#if 1
/*dock bat
echo 1 10 4 0x19 1 0x2c > /proc/i2c_debug/i2c */
static int dock_bat_read(char *buf, char **start, off_t offset, int count, int *eof, void *data)
{
    int readbytes = 4;
    u8 cmd = 0x2c;
    u8 ret[readbytes];
    char retstr[readbytes];
    int r;
    int remain_capacity = 0;
    if(i2c_read(readbytes, &cmd, ret))
        printk("failed to read!\n");	

    if(ret[0] == 0x03 && ret[1] == 0x55)
    {
	r = 1;
	remain_capacity = ret[2];
    }else{
	r = 0;
    }
    sprintf(retstr,"%d",remain_capacity);
    r = set_result(r, "DOCK_battery",(char*)retstr,buf);
    return r;
}
#endif

#if 1
/*Dock_CHG_status
echo 1 10 4 0x19 1 0x54 > /proc/i2c_debug/i2c */
static int dock_CHG_status_read(char *buf, char **start, off_t offset, int count, int *eof, void *data)
{
    int readbytes = 4;
    u8 cmd = 0x54;
    u8 ret[readbytes];
    char retstr[readbytes];
    int r;

    if(i2c_read(readbytes, &cmd, ret))
        printk("failed to read!\n");
	
    if(ret[0] == 0x03 && ret[1] == 0x09)
	r = 1;
    else
        r=0;
    sprintf(retstr,"%d",((ret[3]<<8)|ret[2]));
    r = set_result(r,"Dock_CHG_Status",retstr,buf);
    return r;
}

#endif

#if 1
/*Dock_CHG_current
echo 1 10 4 0x19 1 0x50 > /proc/i2c_debug/i2c */
static int dock_CHG_current_read(char *buf, char **start, off_t offset, int count, int *eof, void *data)
{
    int readbytes = 4;
    u8 cmd = 0x50;
    u8 ret[readbytes];
    char retstr[readbytes];
    int r;

    if(i2c_read(readbytes, &cmd, ret))
        printk("failed to read!\n");
	
    if(ret[0] == 0x03 && ret[1] == 0x09)
    {
	r = 1;
    }else{
	r = 0;
    }
    sprintf(retstr,"%d",((ret[3]<<8)|ret[2]));
    r = set_result(r,"Dock_CHG_Current",retstr,buf);
    return r;
}

#endif
/*ISN read/Write TBD...*/
static int ec_ISN_write(struct file *file, const char __user *buffer, unsigned long count, void *data)
{
	char *buf = (char *)buffer;
	if(dock_isn_write(buf))
	{
		printk("FAIL,ISN_write\n");
	}else{
		printk("PASS,ISN_write\n");
	}
	return count;	
}

/*Touch pad status*/
static int ec_touchpad_status_read(char *buf, char **start, off_t offset, int count, int *eof, void *data)
{
    int readbytes = 4;
    u8 cmd = 0xd2;
    u8 ret[readbytes];
    char retstr[readbytes];
    int r;

    if(i2c_read(readbytes, &cmd, ret))
	printk("failed to read!\n");
    r = ret[3]&0x1;
	
    sprintf(retstr, "%d", r);
    r = set_result(!r, "ec touchpad status",(char*)retstr, buf);
    return r;
}

static int ec_ISN_read(char *buf, char **start, off_t offset, int count, int *eof, void *data)
{
    int r = 0;
    char isnbuf[14] = {0};
    r = dock_isn_read(isnbuf);
    r = set_result(!r, "ec ISN read", isnbuf, buf);
    return r;
}

/*EC_version
    echo 1 10 14 0x19 1 0xc0 > /proc/i2c_debug/i2c*/
static int ec_version_read(char *buf, char **start, off_t offset, int count, int *eof, void *data)
{
    int readbytes = 14;
    u8 cmd = 0xc0;
    u8 ret[readbytes];
    char retstr[readbytes];
    int r;
    
    if(i2c_read(readbytes, &cmd, ret))
        printk("failed to read!\n");
    r = (ret[2] == 'W' && ret[3] == 'L')?1:0;
    sprintf(retstr,"%s",ret+2);
    r = set_result(r, "ec version read", retstr, buf);
    return r;
}

static int LED_Dual_Blue_read(char *buf, char **start, off_t offset, int count, int *eof, void *data)
{
    int readbytes = 14;
    u8 cmd = 0xb4; //Read EC GPI40(LED_BH)
    u8 ret[readbytes];
    int r,success;

    if(i2c_read(readbytes, &cmd, ret))
    {
        success =0;
        printk("failed to read!\n");
    }
    else
        success =1;
    r = ret[3] & 0x1;
    r = set_result(success, "LED_Dual_Blue", r?"1":"0", buf);
    return r;
}
/*LED_Dual_Blue
    0 -> off, 1 -> on, 2 -> flash*/
static int LED_Dual_Blue_write(struct file *file, const char __user *buffer, unsigned long count, void *data)
{
	char *ep;
	u8 W_data[11] = {0};
	char *buf = (char *)buffer;
       int readbytes = 4;
       u8 rcmd = 0xb4; //Read Current LED Status
       u8 ret[readbytes];
	int ref;
 	ref = simple_strtol(buf, &ep, 16);
/*Disable EC control LED*/
      	W_data[0] = 0x09;
	W_data[1] = 0x03;
	W_data[2] = 0x03;
	W_data[3] = 0x01;
	W_data[4] = 0x00;
	if(i2c_write(5, W_data))
       {   
            printk("FAIL,LED BLUE\n");
       }
/*disable PWM*/
	W_data[0] = 0x70;
	W_data[1] = 0x09;
	W_data[2] = 0xCF;
       W_data[3] = 0x00;
       W_data[4] = 0x00;
       if(i2c_write(sizeof(W_data),W_data))
       {
            printk("FAIL,LED BLUE\n");
            return count;
       }
/*Get LED Status*/
       if(i2c_read(readbytes,&rcmd,ret))
        {
            printk("failed to  read");
            return count;
        }
/*Set new LED Status*/
       W_data[0] = 0x4e;
       W_data[1] = 0x03;
       W_data[2] = rcmd;
       if(ref)
           W_data[3] = ret[3] | 0x1;
       else
           W_data[3] = ret[3] & ~(0x1);
       W_data[4] = 0x00;
       if(i2c_write(5,W_data))
        {
            printk("FAIL,LED_Dual_BLUE\n");
            return count;
        }
       else 
            printk("PASS,LED_Dual_BLUE\n");

	return count;
}

static int LED_Dual_Orange_read(char *buf, char **start, off_t offset, int count, int *eof, void *data)
{
    int readbytes = 14;
    u8 cmd = 0xb4; //Read EC GPI45(LED_O)
    u8 ret[readbytes];
    int r,success;

    if(i2c_read(readbytes, &cmd, ret))
    {
        success =0;
        printk("failed to read!\n");
    }
    else
        success =1;
    r = (ret[3] >> 5 )& 0x1;
    r = set_result(success, "LED_Dual_Orange", r?"1":"0", buf);
    return r;
}
/*LED_Dual_Orange
    0 -> off, 1 -> on, 2 -> flash*/
static int LED_Dual_Orange_write(struct file *file, const char __user *buffer, unsigned long count, void *data)
{
	char *ep;
	u8 W_data[11] = {0};
	char *buf = (char *)buffer;
       int readbytes = 4;
       u8 rcmd = 0xb4; //Read Current LED Status
       u8 ret[readbytes];
	int ref;
 	ref = simple_strtol(buf, &ep, 16);
/*Disable EC control LED*/
      	W_data[0] = 0x09;
	W_data[1] = 0x03;
	W_data[2] = 0x03;
	W_data[3] = 0x01;
	W_data[4] = 0x00;
	if(i2c_write(5, W_data))
       {   
            printk("FAIL,LED BLUE\n");
       }
/*disable PWM*/
	W_data[0] = 0x70;
	W_data[1] = 0x09;
	W_data[2] = 0xCF;
       W_data[3] = 0x00;
       W_data[4] = 0x00;
       if(i2c_write(sizeof(W_data),W_data))
       {
            printk("FAIL,LED BLUE\n");
            return count;
       }
/*Get LED Status*/
       if(i2c_read(readbytes,&rcmd,ret))
        {
            printk("failed to  read");
            return count;
        }
/*Set new LED Status*/
       W_data[0] = 0x4e;
       W_data[1] = 0x03;
       W_data[2] = rcmd;
       if(ref)
           W_data[3] = ret[3] | (1<<5);
       else
           W_data[3] = ret[3] & ~(1<<5);
       W_data[4] = 0x00;
       if(i2c_write(5,W_data))
        {
            printk("FAIL,LED Dual Orange\n");
            return count;
        }
       else 
            printk("PASS,LED Dual Orange\n");

	return count;
}

static int LED_Blue_read(char * buf, char * * start, off_t offset, int count, int * eof, void * data)
{
    int readbytes = 4;
    u8 cmd = 0xb6; //Read EC GPI66 (LED_B)
    u8 ret[readbytes];
    int r,success;

    if(i2c_read(readbytes, &cmd, ret))
    {
        success =0;
        printk("failed to read!\n");
    }
    else
        success =1;
    r = (ret[3] >>6) & 0x1;
    r = set_result(success, "LED_Blue", r?"1":"0", buf);
    return r;
}

/*LED_BLUE
    0 -> off, 1 -> on*/
static int LED_Blue_write(struct file *file, const char __user *buffer, unsigned long count, void *data)
{
	char *ep;
	u8 W_data[11] = {0};
	char *buf = (char *)buffer;
       int readbytes = 4;
       u8 rcmd = 0xb6; //Read Current LED Status
       u8 ret[readbytes];
	int ref;
 	ref = simple_strtol(buf, &ep, 16);
/*Disable EC control LED*/
      	W_data[0] = 0x09;
	W_data[1] = 0x03;
	W_data[2] = 0x03;
	W_data[3] = 0x01;
	W_data[4] = 0x00;
	if(i2c_write(5, W_data))
       {   
            printk("FAIL,LED BLUE\n");
       }
/*disable PWM*/
	W_data[0] = 0x70;
	W_data[1] = 0x09;
	W_data[2] = 0xCF;
       W_data[3] = 0x00;
       W_data[4] = 0x00;
       if(i2c_write(sizeof(W_data),W_data))
       {
            printk("FAIL,LED BLUE\n");
            return count;
       }
/*Get LED Status*/
       if(i2c_read(readbytes,&rcmd,ret))
        {
            printk("failed to  read");
            return count;
        }
/*Set new LED Status*/
       W_data[0] = 0x4e;
       W_data[1] = 0x03;
       W_data[2] = rcmd;
       if(ref)
           W_data[3] = ret[3] | (1<<6);
       else
           W_data[3] = ret[3] & !(1<<6);
       W_data[4] = 0x00;
       if(i2c_write(5,W_data))
        {
            printk("FAIL,LED_BLUE\n");
            return count;
        }
       else 
            printk("PASS,LED BLUE\n");

	return count;
}

/*USB Switch*/
static int usb_hub_switch_write(struct file *file, const char __user *buffer, unsigned long count, void *data)
{
    	char *ep;
	char *buf = (char *)buffer;
	int ref;
 	ref = simple_strtol(buf, &ep, 16);
       if(ref)
       {
            /*Turn off OTG boost 5V*/
            if(ec_boost_ctl(OTG_5V,BOOST_OFF) == FAIL)
            {
                printk("FAIL to control 5V boost!!\n");
                return count;
            }
            /*bypass switch function*/
            if(set_usb_bypass_en(usb_bypass_high) == FAIL)
            {
                printk("FAIL to enable bypass!!\n");
                return count;
            }
            /*reset usb hub*/
            if(usb_hub_reset(0xc00))
            {
                printk("FAIL to reset hub!!\n");
                return count;
            }
       }
       else
       {
            /*bypass switch function*/
            if(set_usb_bypass_en(usb_bypass_high) == FAIL)
            {
                printk("FAIL to enable bypass!!\n");
                return count;
            }
            /*reset usb hub*/
            if(usb_hub_reset(0x64))
            {
                printk("FAIL to reset hub!!\n");
                return count;
            }
	     msleep(200);	
            /*switch to normal mode*/
            if(set_usb_bypass_en(usb_bypass_low) == FAIL)
            {
                printk("FAIL to enable bypass!!\n");
                return count;
            }
            /*Turn on OTG boost 5V*/
            if(ec_boost_ctl(OTG_5V,BOOST_ON) == FAIL)
            {
                printk("FAIL to control 5V boost!!\n");
                return count;
            }
       }
       printk("PASS!\n");
       return count;
       
}

static int usb_hub_switch_read(char *buf, char **start, off_t offset, int count, int *eof, void *data)
{
    int status;
    int r;
    r = get_usb_bypass_en(&status);
    if(r==SUCCESS)
        r=1;
    else
       r=0;
    r = set_result(r,"HUB_SWITCH",(status?"1":"0"),buf);
    return r;
}

/*Dock Charging Enable*/
static int dock_charging_req_write(struct file *file, const char __user *buffer, unsigned long count, void *data)
{
    	char *ep;
	char *buf = (char *)buffer;
	int ref;
 	ref = simple_strtol(buf, &ep, 16);
       ec_charging_req(ref?CHARGING_ON:CHARGING_OFF);
       printk("PASS!\n");
       return count;
       
}

static int dock_charging_req_read(char *buf, char **start, off_t offset, int count, int *eof, void *data)
{
    int status;
    int r;
    r = get_ec_charging_req_status(&status);
    if(r==SUCCESS)
        r=1;
    else
       r=0;
    r = set_result(r,"Dock_Charging_req",(status?"1":"0"),buf);
    return r;
}

/*KBD_ID*/
static int KBD_ID_read(char *buf, char **start, off_t offset, int count, int *eof, void *data)
{
	int r = 0;
    	int dock_data =0;
	int KBD_data;
	char KBD_id[5];
    	r = dock_data_read(&dock_data);
	KBD_data = (dock_data) & KBD_MASK;
	sprintf(KBD_id, "%d", KBD_data);
    	r = set_result(!r, "ec KBD_ID read", KBD_id, buf);
	return r;
}

static int KBD_ID_write(struct file *file, const char __user *buffer, unsigned long count, void *data)
{
    	char *ep;
	char *buf = (char *)buffer;
	int dock_data =0;
	int ref;
 	ref = simple_strtol(buf, &ep, 10);
	printk("%s:%d\n",__func__, ref);
	dock_data_read(&dock_data);
	ref = (dock_data & TP_MASK) | ref;
	printk("%s:%d\n",__func__, ref);	
	dock_data_write(ref);
       printk("PASS!\n");
	
	return count;
}

/*TP_ID*/
static int TP_ID_read(char *buf, char **start, off_t offset, int count, int *eof, void *data)
{
	int r = 0;
    	int dock_data = 0;
	int TP_data;
	char TP_id[5];
    	r = dock_data_read(&dock_data);
	TP_data = (dock_data & TP_MASK) >> 8;
	sprintf(TP_id, "%d", TP_data);
    	r = set_result(!r, "ec TP_ID read", TP_id, buf);
	return r;
}

static int TP_ID_write(struct file *file, const char __user *buffer, unsigned long count, void *data)
{
    	char *ep;
	char *buf = (char *)buffer;
	int dock_data =0;
	int ref;
 	ref = simple_strtol(buf, &ep, 10);
	printk("%s:%d\n",__func__, ref);
	ref = ref << 8;
	dock_data_read(&dock_data);
	ref = (dock_data  & KBD_MASK) | ref;
	printk("%s:%d\n",__func__, ref);	
	dock_data_write(ref);
       printk("PASS!\n");

	return count;
}

/*dock_reset*/
static int dock_reset_write(struct file *file, const char __user *buffer, unsigned long count, void *data)
{

    	char *ep;
	char *buf = (char *)buffer;
	int ref;
 	ref = simple_strtol(buf, &ep, 10);
	if(ref < 2)
	{
		printk("%s Dock start Reset\n",ref?"PVT":"DVT");
		ref = dock_reset_func();
       	printk("Dock Reset!\n");
	}

	return count;
	
}
static int pad_control_led_write(struct file *file, const char __user *buffer, unsigned long count, void *data)
{

    	char *ep;
	char *buf = (char *)buffer;
	int ref;
 	ref = simple_strtol(buf, &ep, 10);	
	ref = pad_control_led(ref);
	if(ref == FAIL)
		printk("FAIL\n");
	else
		printk("PASS\n");
		
	return count;
}

int ec_smt_interface_bus_register(int bus)
{
	EC_BUS = bus;
	return 0;
}

static int __init ec_smt_init(void)
{
	int 	i;
	struct proc_dir_entry *ec_smt_entry[EC_SMT_CMD_CNT];
	ec_smt_dir = proc_mkdir( EC_SMT_PROC_NAME , NULL);
	if( ec_smt_dir == NULL ) {
		printk("proc mkdir failed\n");
		return -1;
	}

	for(i = 0 ; i < EC_SMT_CMD_CNT; i++)
	{
		ec_smt_entry[i]= create_proc_entry(ec_smt_entry_name[i] , 0666 , ec_smt_dir);

		if (!ec_smt_entry[i]) {
			printk("create proc entry failed\n" );
			return -1;
		}
	}

        ec_smt_entry[0] -> read_proc = dock_connection_read;
	 ec_smt_entry[1] -> read_proc = ec_ISN_read;
	 ec_smt_entry[1] -> write_proc = ec_ISN_write;
        ec_smt_entry[2] -> read_proc = ec_version_read;
        ec_smt_entry[3] -> read_proc = LED_Dual_Blue_read;
	 ec_smt_entry[3] -> write_proc = LED_Dual_Blue_write;
        ec_smt_entry[4] -> read_proc = LED_Dual_Orange_read;
	 ec_smt_entry[4] -> write_proc = LED_Dual_Orange_write;
        ec_smt_entry[5] -> read_proc = LED_Blue_read;
	 ec_smt_entry[5] -> write_proc = LED_Blue_write;

	 ec_smt_entry[6] -> read_proc = dock_CHG_status_read;
	 ec_smt_entry[7] -> read_proc = dock_CHG_current_read;
	 ec_smt_entry[8] -> read_proc = dock_bat_read;
        ec_smt_entry[9] -> read_proc = usb_hub_switch_read;
        ec_smt_entry[9] -> write_proc = usb_hub_switch_write;
        ec_smt_entry[10] -> read_proc = dock_charging_req_read;
        ec_smt_entry[10] -> write_proc = dock_charging_req_write;
        ec_smt_entry[11] -> read_proc = ec_touchpad_status_read;
	 ec_smt_entry[12] -> read_proc = KBD_ID_read;
	 ec_smt_entry[12] -> write_proc = KBD_ID_write;
 	 ec_smt_entry[13] -> read_proc = TP_ID_read;
	 ec_smt_entry[13] -> write_proc = TP_ID_write;
	 ec_smt_entry[14] -> write_proc = dock_reset_write;
	 ec_smt_entry[15] -> write_proc = pad_control_led_write;

	printk("%s\n",__func__);
	printk("ec_smt_init. end\n");	
	return 0;	
	
}
static void __exit ec_smt_exit(void)
{
    int i;
    for(i = 0 ; i < EC_SMT_CMD_CNT; i++)
    {
        remove_proc_entry( ec_smt_entry_name[i], ec_smt_dir );
    }
    remove_proc_entry( EC_SMT_PROC_NAME , NULL );
        printk("%s\n",__func__);
}

module_init(ec_smt_init);
module_exit(ec_smt_exit);

MODULE_DESCRIPTION("EC SMT tool");
MODULE_LICENSE("GPL");

