/*
 *	read 
 *	echo 1 b(bus)	n(bytes) 0xCC(dev_addr) m(write command number) 0x00(command) > /proc/i2c_debug/i2c
 *	write
 *	exho 0 b(bus) n(bytes) 0xCC(dev_addr) 0xAA(sub_dev) 0x03(send_bytes) 0x00(command data[0]) data[1] data[2] > /proc/i2c_debug/i2c
 * 	example:
 * 		echo 1 3 4 0x19 1 0xd1 > /proc/i2c_debug/i2c						//read
 * 		echo 0 3 4 0x19 0x4e 0x03 0xd1 0x04 0x00 > /proc/i2c_debug/i2c		//write
 * 		cat /proc/i2c_debug/i2c				//show result
 *		read:
 *		dev	: 0x19
 *		reg	: 0xD1
 *		value	: 0x03 4e 04 00
 *
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/ctype.h>
#include <asm/io.h>
#include <linux/i2c.h>

//#define SPEED_MODE

#define I2C_PROC_NAME "i2c_debug"
#define IRQ_NUM 17

static int _next_str(char **, char *);
static void print_k(uint8_t*);
static int i2c_write_byte(void);
static int i2c_read_byte(uint8_t*);

static struct arg_fmt{
	int rw;
	int bytes;
#ifdef SPEED_MODE
	int speed;
#endif
	int bus;
	u8 dev;
	u8 dev_sft;
	u8 reg;
	int num_cmd;
	u8 r_cmd[10];
	union{
		u8 bits[40];
	}val;
	bool suc;
}arg={0};


static struct proc_dir_entry *i2c_dir;

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

static void print_k(uint8_t* buf)
{
int i;
	if ( arg.rw == 0 ) {
		printk("write:\ndev\t: 0x%02X\nreg\t: 0x%02X\n", arg.dev, arg.reg);
		for(i = 0 ; i < arg.bytes ; i++)
		{
			printk("value[%d]:%x\n", i,buf[i]);
		}
	}else if( arg.rw == 1 ){
		printk("read:\ndev\t: 0x%02X\nreg\t: 0x%02X\n", arg.dev, arg.reg);
		for(i = 0 ; i < arg.bytes ; i++)
		{
			printk("value[%d]:%x\n", i,buf[i]);
		}
	}

}

static int i2c_write_byte(void)
{
	int err,i;
	struct i2c_msg msg[1];
	struct i2c_adapter *adap;
	u8 data[arg.bytes+1];
printk("arg.bus:%x\n",arg.bus);
	adap = i2c_get_adapter( arg.bus );
	if (!adap){
		printk("(write)get adapter failed\n");
		return -ENODEV;
	}
	printk("(write)get adapter\n");

	msg->addr = arg.dev_sft;
	msg->flags = 0;			// 0 write
	msg->len = arg.bytes+1;
	msg->buf = data;
	data[0] = arg.reg;
					
	for( i = 1 ; i < arg.bytes+1 ; ++i ){
		data[i] = arg.val.bits[i-1];
		printk("write_data[%d] :%x\n",i,data[i]);
	}
	err = i2c_transfer(adap, msg, 1);

	if( err < 0 ){
		printk("(write)err: %d\n",err);
	}

	i2c_put_adapter(adap);
	if (err >= 0)
		return 0;
	return err;
}

static int i2c_read_byte(uint8_t* buf)
{
	int err,i;
	struct i2c_msg msg[2];
	struct i2c_adapter *adap;
	u8 data[arg.bytes];
	memset( buf , 0 , arg.bytes);
	memset( data , 0 , arg.bytes );

	adap = i2c_get_adapter( arg.bus );
	if (!adap){
		printk("(read)get adapter failed\n");
		return -ENODEV;
	}
	printk("(read)get adapter\n");
 
	msg[0].addr = arg.dev_sft;
	msg[0].flags = 0;		// 0 write
//	msg[0].len = 1;		//need to change
	msg[0].len = arg.num_cmd;
	msg[0].buf = data;
//	data[0] = arg.reg;		//need to change
	for(i=0 ; i < msg[0].len ; i++)				//1116 add
	{
		data[i] = arg.r_cmd[i];
	}

	msg[1].addr = arg.dev_sft;
	msg[1].flags = I2C_M_RD;	// 1 read
	msg[1].len = arg.bytes;
	msg[1].buf = data;

	err = i2c_transfer(adap, msg, 2);
	if( err < 0 ){
		printk("(read)err: %d\n",err);
	}
	for( i=0 ; i < arg.bytes ; ++i){
		printk("AFTER_read_byte:msg[1].buf[%d] :%x\n",i,msg[1].buf[i]);
	}
	
	for( i=0 ; i < arg.bytes ; ++i){
		arg.val.bits[i] = data[i];
	}
	i2c_put_adapter(adap);
	/*---*/
       memcpy(&buf[0], &data[0], arg.bytes );
	/*---*/
	if (err >= 0)
		return 0;
	return err;
}

static int i2c_debug_write(struct file *file, const char __user *buffer, unsigned long count, void *data)
{
	char *ep;
	char *buf = (char *)buffer;	
uint8_t testbuf[64] = {0};
int i;

	arg.rw = simple_strtol(buf, &ep, 10);	//read or write
	if (_next_str(&buf, ep)) {
		return count;
	}
printk("after arg.rw:%x\n",arg.rw);
	arg.bus = simple_strtol(buf, &ep, 10);	//bus number
	if (_next_str(&buf, ep)) {
		return count;
	}
printk("arg.bus:%x\n",arg.bus);	

	arg.bytes = simple_strtol(buf, &ep, 10);	//return bytes number
	if (_next_str(&buf, ep)) {
		return count;
	}
printk("after arg.bytes:%x\n",arg.bytes);

	arg.dev = simple_strtol(buf, &ep, 16);	//slave address
	if (_next_str(&buf, ep)) {
		return count;
	}
	
	arg.dev_sft = arg.dev ;				//do not shift

	if(arg.rw == 1)								//1116 add
	{
		arg.num_cmd = simple_strtol(buf, &ep, 10);
		if (_next_str(&buf, ep)) {
			return count;
		}

		for(i = 0 ; i < arg.num_cmd ; i++)
		{
			if (_next_str(&buf, ep)) {
				printk("count:%lx\n",count);
			}
			arg.r_cmd[i] = simple_strtol(buf, &ep, 16);
			printk("r_cmd[%d]:%x\n",i,arg.r_cmd[i]);
		}
	}else{										//1116 add
		arg.reg = simple_strtol(buf, &ep, 16);	//command or sub_id
	}											//1116 add
printk("==================\n");
	if( arg.rw == 0 ){
		
		for(i = 0 ;i < 40 ; i++)
		{	
			if (_next_str(&buf, ep)) {
				printk("count:%lx\n",count);
//				return count;					
			}
//printk("buf:%c,  ep:%c\n",*buf, *ep);
		arg.val.bits[i] = simple_strtoll(buf, &ep, 16);	//if write,	write command and data bytes
//printk("af1_arg.val.bits[%d]:%x\n",i,arg.val.bits[i]);    //Robert,20111223
		}
//printk("af1_arg.val.val:%x\n",arg.val.val);
	}
/*begin read or write*/	
	if( arg.rw == 1 ){
	
		if( i2c_read_byte( testbuf) >= 0  ){
			print_k(testbuf);
			arg.suc = 1;
		}else{
			printk("i2c read failed\n");
			arg.suc = 0;
		}
	}else if( arg.rw == 0 ){
		if( i2c_write_byte() >= 0 ){
//			print_k(testbuf);
			arg.suc = 1;
		}else{
			printk("i2c write failed\n");
			arg.suc = 0;
		}
	}
	return count;
}

static int i2c_debug_read(char *buf, char **start, off_t offset, int count, int *eof, void *data)
{
	int len = 0;
	int i ;
	if( arg.suc == 0 ){
		len = sprintf(buf, "i2c read(write) failed\n");
		return len;
	}else if( arg.rw == 0 ){
				len = sprintf(buf, "write1:\ndev\t: 0x%02X\nreg\t: 0x%02X\nvalue:\t: 0x", arg.dev, arg.reg);
				for(i = 0 ; i < arg.bytes ; i++)
				{
					len = sprintf(buf, "%s%02x ", buf, arg.val.bits[i]);
				}
				len = sprintf(buf, "%s\n",buf);

	}else if( arg.rw == 1 ){
//				len = sprintf(buf, "write2:\ndev\t: 0x%02X\nreg\t: 0x%02X\nvalue\t: 0x", arg.dev, arg.reg);
				len = sprintf(buf, "write2:\ndev\t: 0x%02X\nreg\t: 0x", arg.dev);
				for(i = 0 ; i < arg.num_cmd ; i++)
				{
					len = sprintf(buf, "%s%02x ",buf, arg.r_cmd[i]);
				}
				len = sprintf(buf, "%s\nvalue\t: 0x",buf);
				for(i = 0 ; i < arg.bytes ; i++)
				{
					len = sprintf(buf, "%s%02x ", buf, arg.val.bits[i]);
				}
				len = sprintf(buf, "%s\n",buf);
	}
	return len;
}
/*	1114	test*/
static int __init i2c_debug_init(void)
{
	struct proc_dir_entry *i2c_entry;
	i2c_dir = proc_mkdir( I2C_PROC_NAME , NULL);
	if( i2c_dir == NULL ) {
		printk("proc mkdir failed\n");
		return -1;
	}
	i2c_entry = create_proc_entry("i2c" , 0666 , i2c_dir);

	if (!i2c_entry) {
		printk("create proc entry failed\n" );
		return -1;
	}
	i2c_entry->read_proc 	= i2c_debug_read;
	i2c_entry->write_proc 	= i2c_debug_write;
	printk("%s\n",__func__);
	printk("i2c_debug_init. end\n");	
	return 0;	
	
}
static void __exit i2c_debug_exit(void)
{
	remove_proc_entry( "i2c", i2c_dir );
	remove_proc_entry( I2C_PROC_NAME , NULL );
	printk("%s\n",__func__);
}

module_init(i2c_debug_init);
module_exit(i2c_debug_exit);

MODULE_DESCRIPTION("I2C debug tool");
MODULE_LICENSE("GPL");

