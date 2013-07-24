/*
 *  drivers/switch/switch_gpio.c
 *
 * Copyright (C) 2008 Google, Inc.
 * Author: Mike Lockwood <lockwood@android.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/switch.h>
#include <linux/workqueue.h>
#include <linux/gpio.h>

struct gpio_switch_data {
	struct switch_dev sdev;
	unsigned gpio;
	const char *name_on;
	const char *name_off;
	const char *state_on;
	const char *state_off;
	int irq;
	struct work_struct work;

	/*pega-extension*/
	void (*ext_handler)(void); //ext work for irq handler
	int (*ext_int_handler)(void); //ext work for irq handler
	int irq_trigger;
	int pre_state;
};

static void gpio_switch_work(struct work_struct *work)
{
	int state;
    int I2C_state = 0;
    char event_string[20];
    char *envp[] = { event_string, NULL };

	struct gpio_switch_data	*data =
		container_of(work, struct gpio_switch_data, work);
	state = gpio_get_value(data->gpio);
	if(data->irq_trigger ==(IRQF_TRIGGER_FALLING |IRQF_TRIGGER_RISING) && data->pre_state == state)
	{
		printk("%sdebounce:previous state:%d == state:%d\n",__func__, data->pre_state,state);
		return; //avoid the same evnet report repeatly.
	}
	data->pre_state = state;
	if(data->ext_handler) // external work
		data->ext_handler();
	if(data->ext_int_handler) // external work
		I2C_state = data->ext_int_handler();
	if(data->gpio == 14) //for h2w
	{
		/* BEGIN-20111221-paul change the gpio value for headset insertion is 1, headset removal is 0. */
		data->sdev.state = !state;
		/* BEGIN-20111221-paul change the gpio value for headset insertion is 1, headset removal is 0. */
		return;
	}
	if(data->gpio == 7 || data->gpio == 35)
	{

	 	data->sdev.print_state = NULL;
		if (state == 0)
		{
			if(I2C_state) {
				sprintf(event_string, "SWITCH_STATE=1");		//I2C OK
				data->sdev.state = 1;
				}
			else {
				sprintf(event_string, "SWITCH_STATE=10");		//I2C fail
				data->sdev.state = 10;
				}
		} else {
				sprintf(event_string, "SWITCH_STATE=0");
				data->sdev.state = 0;
		}
		kobject_uevent_env(&data->sdev.dev->kobj, KOBJ_CHANGE, envp);
		return;
	}
	switch_set_state(&data->sdev, state);
}

static irqreturn_t gpio_irq_handler(int irq, void *dev_id)
{
	struct gpio_switch_data *switch_data =
	    (struct gpio_switch_data *)dev_id;

	schedule_work(&switch_data->work);
	return IRQ_HANDLED;
}

static ssize_t switch_gpio_print_state(struct switch_dev *sdev, char *buf)
{
	struct gpio_switch_data	*switch_data =
		container_of(sdev, struct gpio_switch_data, sdev);
	const char *state;
	if (switch_get_state(sdev))
		state = switch_data->state_on;
	else
		state = switch_data->state_off;

	if (state)
		return sprintf(buf, "%s\n", state);
	return -1;
}

static int gpio_switch_probe(struct platform_device *pdev)
{
	struct gpio_switch_platform_data *pdata = pdev->dev.platform_data;
	struct gpio_switch_data *switch_data;
	int ret = 0;

	if (!pdata)
		return -EBUSY;

	switch_data = kzalloc(sizeof(struct gpio_switch_data), GFP_KERNEL);
	if (!switch_data)
		return -ENOMEM;

	switch_data->sdev.name = pdata->name;
	switch_data->gpio = pdata->gpio;
	switch_data->name_on = pdata->name_on;
	switch_data->name_off = pdata->name_off;
	switch_data->state_on = pdata->state_on;
	switch_data->state_off = pdata->state_off;
	switch_data->ext_handler = pdata->ext_handler; //external work function point
	switch_data->ext_int_handler = pdata->ext_int_handler;
	switch_data->sdev.print_state = switch_gpio_print_state;
	switch_data->pre_state = -1; //default not 0 or 1

    ret = switch_dev_register(&switch_data->sdev);
	if (ret < 0)
		goto err_switch_dev_register;
	ret = gpio_request(switch_data->gpio, pdev->name);
	if (ret < 0)
		goto err_request_gpio;

	ret = gpio_direction_input(switch_data->gpio);
	if (ret < 0)
		goto err_set_gpio_input;

	INIT_WORK(&switch_data->work, gpio_switch_work);

	switch_data->irq = gpio_to_irq(switch_data->gpio);
	if (switch_data->irq < 0) {
		ret = switch_data->irq;
		goto err_detect_irq_num_failed;
	}
								
#if 0
	/*Oringinal IRQ request as LOW ACTIVE*/
	ret = request_irq(switch_data->irq, gpio_irq_handler,
			  IRQF_TRIGGER_LOW, pdev->name, switch_data);
#endif

	pdata->irq_trigger = (pdata->irq_trigger)?(pdata->irq_trigger):(IRQF_TRIGGER_FALLING);
	switch_data->irq_trigger = pdata->irq_trigger;
	ret = request_irq(switch_data->irq, gpio_irq_handler,
			  pdata->irq_trigger, pdev->name, switch_data);
	if (ret < 0)
		goto err_request_irq;

	/* Perform initial detection */
	gpio_switch_work(&switch_data->work);

	return 0;

err_request_irq:
err_detect_irq_num_failed:
err_set_gpio_input:
	gpio_free(switch_data->gpio);
err_request_gpio:
    switch_dev_unregister(&switch_data->sdev);
err_switch_dev_register:
	kfree(switch_data);

	return ret;
}

static int __devexit gpio_switch_remove(struct platform_device *pdev)
{
	struct gpio_switch_data *switch_data = platform_get_drvdata(pdev);

	cancel_work_sync(&switch_data->work);
	gpio_free(switch_data->gpio);
    switch_dev_unregister(&switch_data->sdev);
	kfree(switch_data);

	return 0;
}

static struct platform_driver gpio_switch_driver = {
	.probe		= gpio_switch_probe,
	.remove		= __devexit_p(gpio_switch_remove),
	.driver		= {
		.name	= "switch-gpio",
		.owner	= THIS_MODULE,
	},
};

static int __init gpio_switch_init(void)
{
	return platform_driver_register(&gpio_switch_driver);
}

static void __exit gpio_switch_exit(void)
{
	platform_driver_unregister(&gpio_switch_driver);
}

module_init(gpio_switch_init);
module_exit(gpio_switch_exit);

MODULE_AUTHOR("Mike Lockwood <lockwood@android.com>");
MODULE_DESCRIPTION("GPIO Switch driver");
MODULE_LICENSE("GPL");
