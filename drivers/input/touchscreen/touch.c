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
#include <asm/unaligned.h>
#include <linux/firmware.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/hrtimer.h>
//[*]--------------------------------------------------------------------------------------------------[*]
#include "touch.h"
#include "touch-i2c.h"
#include "touch-sysfs.h"
#include "touch-update.h"

//[*]--------------------------------------------------------------------------------------------------[*]
#if defined(CONFIG_HAS_EARLYSUSPEND)
	#include <linux/wakelock.h>
	#include <linux/earlysuspend.h>
	#include <linux/suspend.h>
#endif


/*20111216 Jack: Error code define*/
#define NO_ERR 0
#define ERR_POWERSWITCCH 1
/*20120130 Jack: timer to delay*/
struct hrtimer delay_timer;
ktime_t delay_time;
struct touch *lgd_ts;
//[*]--------------------------------------------------------------------------------------------------[*]
#define	DEBUG_TOUCH

//[*]--------------------------------------------------------------------------------------------------[*]
// function prototype define
//[*]--------------------------------------------------------------------------------------------------[*]
static irqreturn_t 	touch_irq		(int irq, void *handle);

void			touch_enable		(struct touch *ts);
void			touch_disable		(struct touch *ts);
int 			touch_remove		(struct device *dev);
void 			touch_hw_reset		(struct touch *ts);
int 			touch_wake_status	(struct touch *ts);
void			touch_wake_control	(struct touch *ts);
int				touch_probe			(struct i2c_client *client);

#ifdef	CONFIG_HAS_EARLYSUSPEND
	static void		touch_late_resume	(struct early_suspend *h);
	static void		touch_early_suspend(struct early_suspend *h);
#else	//#ifdef	CONFIG_HAS_EARLYSUSPEND
	void 			touch_suspend		(struct device *dev);
	void		 	touch_resume		(struct device *dev);
#endif

void			touch_information_display	(struct touch *ts);

static int 		touch_input_open	(struct input_dev *input);
static void		touch_input_close	(struct input_dev *input);

static void 	touch_report		(struct touch *ts);
static void 	touch_work			(struct touch *ts);

/*[BEGIN]20110118 Jack: Delay timer function*/
static enum hrtimer_restart delay_timer_func(struct hrtimer *data)
{
	int ret;
	if(lgd_ts->irq){
		touch_enable(lgd_ts);
	}
	ret=hrtimer_try_to_cancel(&delay_timer);
	return HRTIMER_NORESTART;
}
/*[END]20110118 Jack: Delay timer function*/

//[*]--------------------------------------------------------------------------------------------------[*]
static void 	touch_report(struct touch *ts)
{
	int		id;
	
	for(id = 0; id < ts->pdata->max_fingers; id++)	{
		if(ts->finger[id].event == TS_EVENT_UNKNOWN)		continue;
		
		if(ts->finger[id].event != TS_EVENT_RELEASE)	{

			if(ts->pdata->id_max)
				input_report_abs(ts->input, ABS_MT_TRACKING_ID, ts->pdata->id_min + id);
			else
				input_report_abs(ts->input, ABS_MT_TRACKING_ID, id);
	
			if(ts->pdata->touch_max)	input_report_abs(ts->input, ABS_MT_TOUCH_MAJOR, ts->finger[id].area);
			if(ts->pdata->width_max)	input_report_abs(ts->input, ABS_MT_WIDTH_MAJOR, ts->finger[id].pressure);
	
			input_report_abs(ts->input, ABS_MT_POSITION_X, 	ts->finger[id].x);
			input_report_abs(ts->input, ABS_MT_POSITION_Y,	ts->finger[id].y);
#if defined(DEBUG_TOUCH)
//	printk("%s : id = %d, x = %d, y = %d\n", __func__, id, ts->finger[id].x, ts->finger[id].y);
#endif
		}
		else	{
			ts->finger[id].event = TS_EVENT_UNKNOWN;
#if defined(DEBUG_TOUCH)
//	printk("%s : release id = %d\n", __func__, id);
#endif
		}

		input_mt_sync(ts->input);
	}

	input_sync(ts->input);
}

//[*]--------------------------------------------------------------------------------------------------[*]
static void 	touch_work(struct touch *ts)
{
	int				i, report_id, report_cnt;
	unsigned char	rdata[MAX_PROTOCOL_SIZE +1], length;
	
	touch_i2c_read(ts->client, REG_TS_LENGTH, 1, &length);

#if defined(DEBUG_TOUCH)
//	printk("%s : REG_TS_LENGTH = %d\n", __func__, length);
#endif	
	if(length > MAX_PROTOCOL_SIZE)		return;
	
	if(length)	{
		touch_i2c_read(ts->client, REG_TS_DATA, length, &rdata[0]);
	
		report_id 	= ((rdata[1] << 4) | rdata[0]) & 0x3FF;
		
#if defined(DEBUG_TOUCH)
//	printk("%s : REPORT ID = 0x%04X\n", __func__, report_id);
#endif	
		for(i = 0, report_cnt = 0; i < ts->pdata->max_fingers; report_id >>= 1, i++)	{
			if(report_id & 0x01)	{
				ts->finger[i].event = TS_EVENT_MOVE;
				ts->finger[i].x 	= ((rdata[2 + (report_cnt * 3)] << 4) & 0xF00) | rdata[2 + (report_cnt * 3) + 1];
				ts->finger[i].y 	= ((rdata[2 + (report_cnt * 3)] << 8) & 0xF00) | rdata[2 + (report_cnt * 3) + 2];
				report_cnt++;
			}
			else	{
				if(ts->finger[i].event == TS_EVENT_MOVE)	ts->finger[i].event = TS_EVENT_RELEASE;
				else										ts->finger[i].event = TS_EVENT_UNKNOWN;
			}
		}
		// Touch screen status & data struct
		touch_report(ts);
	}
}

//[*]--------------------------------------------------------------------------------------------------[*]
static irqreturn_t 	touch_irq(int irq, void *handle)
{
	struct touch 	*ts = handle;

	touch_work(ts);
	
	return IRQ_HANDLED;
}

//[*]--------------------------------------------------------------------------------------------------[*]
static int 		touch_input_open(struct input_dev *input)
{
	struct touch 	*ts = input_get_drvdata(input);

	touch_enable(ts);

//	printk("%s\n", __func__);
	
	return 0;
}

//[*]--------------------------------------------------------------------------------------------------[*]
static void 	touch_input_close(struct input_dev *input)
{
	struct touch 	*ts = input_get_drvdata(input);

	touch_disable(ts);

//	printk("%s\n", __func__);
}

//[*]--------------------------------------------------------------------------------------------------[*]
void 	touch_hw_reset(struct touch *ts)
{
	if(gpio_request(ts->pdata->reset_gpio, "touch reset"))	{
		printk("--------------------------------------------------------\n");
		printk("%s : request port error!\n", "touch reset");
		printk("--------------------------------------------------------\n");
	}
	else	{
		gpio_direction_output(ts->pdata->reset_gpio, ts->pdata->reset_level ? 0 : 1);
		mdelay(ts->pdata->reset_delay);

		gpio_direction_output(ts->pdata->reset_gpio, ts->pdata->reset_level ? 1 : 0);
		mdelay(ts->pdata->reset_delay);

		gpio_direction_output(ts->pdata->reset_gpio, ts->pdata->reset_level ? 0 : 1);
		mdelay(ts->pdata->reset_delay);
		
		gpio_free(ts->pdata->reset_gpio);
	}
}

//[*]--------------------------------------------------------------------------------------------------[*]
int 	touch_wake_status(struct touch *ts)
{
	int		ret = -1;
	
	if((ts->pdata->wake_gpio)&&(ts->pdata->WakeGPIOInit)){
			gpio_direction_output(ts->pdata->wake_gpio,1);		udelay(1000);
			ret = gpio_get_value(ts->pdata->wake_gpio);
	}else{
		printk("ERROR : Was not declared wake_up gpio pin in Platform data.!\n");
	}
	return	ret;
}

//[*]--------------------------------------------------------------------------------------------------[*]
#if 1
void	touch_wake_control(struct touch *ts)
{
	if((ts->pdata->wake_gpio)&&(ts->pdata->WakeGPIOInit))	{
			gpio_direction_output(ts->pdata->wake_gpio, 0);		udelay(100);
	    gpio_direction_output(ts->pdata->wake_gpio, 1);  mdelay(45); // 0521 modifed delay time from 10 ms to 45 ms
	   	gpio_direction_output(ts->pdata->wake_gpio, 0);  udelay(10);
	}
	else	{
		printk("ERROR : Was not declared wakeup gpio pin in Platform data.!\n");
	}
}


//[*]--------------------------------------------------------------------------------------------------[*]

void	touch_information_display(struct touch *ts)
{
#if 0
	printk("--------------------------------------------------------\n");
	printk("           TOUCH SCREEN INFORMATION\n");
	printk("--------------------------------------------------------\n");
#endif
	touch_wake_control(ts);
	if(touch_i2c_read(ts->client, REG_TS_FIRMWARE_ID, 1, &ts->fw_version) == 1)
		printk("LGD TOUCH F/W Version = %d.%02d\n", ts->fw_version/100, ts->fw_version%100);
#if 0		
	printk("TOUCH FINGRES MAX = %d\n", ts->pdata->max_fingers);
	printk("TOUCH ABS X MAX = %d, TOUCH ABS X MIN = %d\n", ts->pdata->abs_max_x, ts->pdata->abs_min_x);
	printk("TOUCH ABS Y MAX = %d, TOUCH ABS Y MIN = %d\n", ts->pdata->abs_max_y, ts->pdata->abs_min_y);

	if(ts->pdata->touch_max)
		printk("TOUCH MAJOR MAX = %d, TOUCH MAJOR MIN = %d\n", ts->pdata->touch_max, ts->pdata->touch_min);
	if(ts->pdata->width_max)
		printk("TOUCH WIDTH MAX = %d, TOUCH WIDTH MIN = %d\n", ts->pdata->width_max, ts->pdata->width_min);
	if(ts->pdata->id_max)		
		printk("TOUCH ID MAX = %d, TOUCH ID MIN = %d\n", ts->pdata->id_max, ts->pdata->id_min);
	else
		printk("TOUCH ID MAX = %d, TOUCH ID MIN = %d\n", ts->pdata->max_fingers, 0);

	if(ts->pdata->gpio_init)	
		printk("GPIO Early Init Function Implemented\n");
	if(ts->pdata->reset_gpio)	
		printk("H/W Reset Function Implemented\n");
	if(ts->pdata->wake_gpio)	
		printk("H/W Wake-up control Function Implemented\n");

	printk("--------------------------------------------------------\n");
#endif
}
#endif
//[*]--------------------------------------------------------------------------------------------------[*]
int		touch_probe(struct i2c_client *client)
{
	int				rc = -1;
  struct device 	*dev = &client->dev;
	struct touch 	*ts;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_BYTE_DATA)) 	{
		dev_err(&client->dev, "i2c byte data not supported\n");
		return -EIO;
	}

	if (!client->dev.platform_data) {
		dev_err(&client->dev, "Platform data is not available!\n");
		return -EINVAL;
	}

	if(!(ts = kzalloc(sizeof(*ts), GFP_KERNEL)))	return	-ENOMEM;

	ts->client	= client;
	ts->pdata 	= client->dev.platform_data;

	i2c_set_clientdata(client, ts);

	if(ts->pdata->irq_gpio){
		ts->irq = client->irq;
	}
	else{
		dev_err(dev, "Was not declared irq_gpio pin in Platform data.!\n");
		goto err_free_input_mem;		
	}

	/*[BEGIN]20120130 Jack: init timer to delay at early suspend*/
	lgd_ts=ts;
	delay_time = ktime_set(0, 40000000); /* 40 ms */
	hrtimer_init(&delay_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	delay_timer.function =  delay_timer_func;
	/*[END]20120130 Jack: init timer to delay at early suspend*/

	if(ts->pdata->gpio_init)  // wake up gpio setup
	{
		ts->pdata->gpio_init();
		ts->pdata->WakeGPIOInit=true;
	}
	//	if(ts->pdata->reset_gpio)	touch_hw_reset(ts);

	if(ts->pdata->lgd_power_save){		//open power
		rc = ts->pdata->lgd_power_save(1);
		if(rc < 0){
			printk( KERN_INFO "lgd power open on failed\n");
			goto err_free_input_mem;
		}
	}

	if(ts->pdata->gpio_setup){			//setup gpio pin  -> touch irq (CHG)
		rc =ts->pdata->gpio_setup();
		if(rc < 0){
			  printk( KERN_INFO "lgd_ts_probe gpio setup failed\n");
			  goto err_unregister_device;
		}
	}
	dev_set_drvdata(dev, ts);

	if(!(ts->input 	= input_allocate_device())){
		goto err_free_input_mem;
	}
	snprintf(ts->phys, sizeof(ts->phys), "%s/input0", ts->pdata->name);

	ts->input->name 		= ts->pdata->name;
	ts->input->phys 		= ts->phys;
	ts->input->dev.parent 	= dev;
	ts->input->id.bustype 	= BUS_I2C;
	ts->input->open 		= touch_input_open;
	ts->input->close 		= touch_input_close;

	ts->input->id.vendor 	= ts->pdata->vendor;
	ts->input->id.product 	= ts->pdata->product;
	ts->input->id.version 	= ts->pdata->version;

	set_bit(EV_SYN, 			ts->input->evbit);
	set_bit(EV_ABS, 			ts->input->evbit);
	set_bit(ABS_MT_POSITION_X, 	ts->input->absbit);
	set_bit(ABS_MT_POSITION_Y, 	ts->input->absbit);
	set_bit(ABS_MT_TRACKING_ID, ts->input->absbit);

	input_set_drvdata(ts->input, ts);

	/* multi touch */
	input_set_abs_params(ts->input, ABS_MT_POSITION_X, 	ts->pdata->abs_min_x, ts->pdata->abs_max_x,	0, 0);
	input_set_abs_params(ts->input, ABS_MT_POSITION_Y, 	ts->pdata->abs_min_y, ts->pdata->abs_max_y,	0, 0);
		
	if(ts->pdata->touch_max)
		input_set_abs_params(ts->input, ABS_MT_TOUCH_MAJOR, ts->pdata->touch_min, ts->pdata->touch_max,	0, 0);
		
	if(ts->pdata->width_max)
		input_set_abs_params(ts->input, ABS_MT_WIDTH_MAJOR, ts->pdata->width_min, ts->pdata->width_max,	0, 0);

	if(ts->pdata->id_max)
		input_set_abs_params(ts->input, ABS_MT_TRACKING_ID, ts->pdata->id_min, ts->pdata->id_max, 	0, 0);
	else
		input_set_abs_params(ts->input, ABS_MT_TRACKING_ID, 0, ts->pdata->max_fingers, 	0, 0);

	if ((rc = input_register_device(ts->input)))	{
		dev_err(dev, "(%s) input register fail!\n", ts->input->name);
		goto err_free_mem;
	}


#if defined(CONFIG_HAS_EARLYSUSPEND)
	ts->power.suspend	= touch_early_suspend;
	ts->power.resume	= touch_late_resume;
	ts->power.level		= EARLY_SUSPEND_LEVEL_DISABLE_FB-1;
	
	//if, is in USER_SLEEP status and no active auto expiring wake lock
	//if (has_wake_lock(WAKE_LOCK_SUSPEND) == 0 && get_suspend_state() == PM_SUSPEND_ON)
	register_early_suspend(&ts->power);
#endif

	
// IRQF_TRIGGER_RISING
	mdelay(40);

#if 1
	if((rc = request_threaded_irq(ts->irq, NULL, touch_irq, ts->pdata->irq_flags, ts->pdata->name, ts)))	{
		dev_err(dev, "irq %d request fail!\n", ts->irq);
		goto err_free_mem;
	}
#else
	if((rc = request_threaded_irq(ts->irq, NULL, touch_irq, IRQF_TRIGGER_RISING, ts->pdata->name, ts)))	{
		dev_err(dev, "irq %d request fail!\n", ts->irq);
		goto err_free_mem;
	}
#endif
	disable_irq_nosync(ts->irq); //0521 modified

//	mdelay(30);
//	disable_irq_nosync(ts->irq); //0521 modified
	ts->disabled = true;

	if((rc = touch_sysfs_create(dev)) < 0)	
		goto	err_free_irq;


	if(touch_get_mode(ts) == TOUCH_BOOT_MODE)	{
		printk("================= DEFAULT F/W UPDATE =================\n");
		if((rc = touch_default_fw(dev)) < 0)	{
			dev_err(dev, "Can't probe! current touch mode is boot mode! or update failed!\n");
			goto	err_free_irq;
		}
	}

	// Touch screen information display
	touch_information_display(ts);


	return 0;

err_unregister_device :
	input_unregister_device(ts->input);
	ts->input = NULL;
	free_irq(ts->irq, ts);
#if 1
err_free_irq:
	free_irq(ts->irq, ts);
	input_free_device(ts->input);
#endif
err_free_mem:
	kfree(ts->input);
err_free_input_mem:
	kfree(ts);

	return rc;
}

//[*]--------------------------------------------------------------------------------------------------[*]
void	touch_enable(struct touch *ts)
{
	if(ts->disabled)		{
		enable_irq(ts->irq);
		ts->disabled = false;
	}
}

//[*]--------------------------------------------------------------------------------------------------[*]
void	touch_disable(struct touch *ts)
{
	if(!ts->disabled)	{
		disable_irq(ts->irq);
		ts->disabled = true;
	}
}

//[*]--------------------------------------------------------------------------------------------------[*]
int 	touch_remove(struct device *dev)
{
	struct touch *ts = dev_get_drvdata(dev);

	touch_sysfs_remove(dev);
	
	free_irq(ts->irq, ts);

	if(ts->pdata->reset_gpio)	gpio_free(ts->pdata->reset_gpio);

	input_unregister_device(ts->input);
	
	dev_set_drvdata(dev, NULL);		

	kfree(ts);

	return 0;
}

//[*]--------------------------------------------------------------------------------------------------[*]
#ifdef	CONFIG_HAS_EARLYSUSPEND
		void		touch_early_suspend	(struct early_suspend *h)
#else
				void 	touch_suspend			(struct device *dev)
#endif		
{
	#ifdef	CONFIG_HAS_EARLYSUSPEND
		struct touch *ts = container_of(h, struct touch, power);
	#else	
		struct touch *ts = dev_get_drvdata(dev);
	#endif	

		int ret;
	//printk( KERN_INFO "LDC3001B_suspend().....\n");//Remove debug log
	/*[BEGIN] 2011-12-19 Jack: Disable IRQ Firstly*/
	touch_disable(ts);
	/*[END] 2011-12-19 Jack: Disable IRQ Firstly*/
	/*[BEGIN] 2011-12-19 Jack: Cut Touch VDD when system suspend*/
	ret = ts->pdata->lgd_power_save(0);
	if(ret < 0){
		printk( KERN_INFO "lgd power open off failed\n");
	}
	/*[END] 2011-12-19 Jack: Cut Touch VDD when system suspend*/

}

//[*]--------------------------------------------------------------------------------------------------[*]
#ifdef	CONFIG_HAS_EARLYSUSPEND
	void touch_late_resume	(struct early_suspend *h)
#else
			void 	touch_resume		(struct device *dev)
#endif
{

	int ret;
	
	#ifdef	CONFIG_HAS_EARLYSUSPEND
		struct touch *ts = container_of(h, struct touch, power);
	#else	
		struct touch *ts = dev_get_drvdata(dev);
	#endif	

//	printk("%s\n", __func__);	Remove debug log
	/*[BEGIN] 2011-12-19 Jack: RePowerOn Touch moudlue when system suspend*/
	ret = ts->pdata->lgd_power_save(1);
	if(ret < 0){
		printk( KERN_INFO "lgd power open on failed\n");
	}
	/*[END] 2011-12-19 Jack: RePowerOn Touch moudlue when system suspend*/
	/*[BEGIN] 2012-01-17 Jack:Delay 20 ms to prevent unexpected interrupts*/
	hrtimer_start(&delay_timer, delay_time, HRTIMER_MODE_REL);
	/*[END] 2012-01-17 Jack:Delay 20 ms to prevent unexpected interrupts*/


}

//[*]--------------------------------------------------------------------------------------------------[*]
MODULE_AUTHOR("HardKernel Co., Ltd.");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Touchscreen Driver");

//[*]--------------------------------------------------------------------------------------------------[*]
//[*]--------------------------------------------------------------------------------------------------[*]
