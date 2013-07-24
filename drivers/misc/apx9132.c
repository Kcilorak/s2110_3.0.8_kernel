/*
 * Driver for keys on GPIO lines capable of generating interrupts.
 *
 * Copyright 2005 Phil Blundell
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>

#include <linux/init.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/sched.h>
#include <linux/pm.h>
#include <linux/slab.h>
#include <linux/sysctl.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/gpio_keys.h>
#include <linux/workqueue.h>
#include <linux/gpio.h>
#include <linux/apx9132.h>

#include <linux/mfd/pm8xxx/gpio.h>

//BEGIN Kite_Yeh@pegatron [20120330][map L11 requlator for hall sensor]
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <asm/setup.h>
//END Kite_Yeh@pegatron [20120330][map L11 requlator for hall sensor]

struct hall_sensor_driver_data {
    int current_state;
    struct work_struct work;
    struct hall_sensor_platform_data *platform_data;
    struct platform_device *pdev;
};  

static struct regulator *hs_vreg_l11;

static ssize_t hall_sensor_show_hall(struct device *dev, struct device_attribute *attr,	char *buf)
{
    struct platform_device *pdev = to_platform_device(dev);
    struct hall_sensor_driver_data *driver_data = platform_get_drvdata(pdev);
    return sprintf(buf, "%d\n", driver_data->current_state);
}

/*
 * ATTRIBUTES:
 *
 * /sys/devices/platform/hall-sensor/state [ro]
 */
static DEVICE_ATTR(state, S_IRUGO, hall_sensor_show_hall, NULL);

static struct attribute *hall_sensor_attrs[] = {
    &dev_attr_state.attr,
    NULL,
};

static struct attribute_group hall_sensor_attr_group = {
    .attrs = hall_sensor_attrs,
};

struct hall_sensor_driver_data *init_driver_data(struct platform_device *pdev)
{
    struct hall_sensor_driver_data *driver_data = 0;
    struct hall_sensor_platform_data *platform_data = pdev->dev.platform_data;

    driver_data = kzalloc(sizeof(struct hall_sensor_driver_data), GFP_KERNEL);

    if (!driver_data) {
	kfree(driver_data);
        driver_data = 0;

        dev_err(&pdev->dev, "failed to allocate state\n");
        printk("[Hall Sensor] init_driver_data: failed to allocate state");

        return 0;
    }
    
    if (platform_data) {
        driver_data->platform_data = platform_data;
    }

    platform_set_drvdata(pdev, driver_data);

    return driver_data;
}

static void hall_sensor_work_func(struct work_struct *work)
{
    int state;
    struct hall_sensor_driver_data *driver_data = container_of(work, struct hall_sensor_driver_data, work);
    /* BEGIN Acme_Wen@pegatroncorp.com [2012/03/05] [uevent for hall sensor] */
    char event_string[20];
    char *envp[] = { event_string, NULL };
    struct device *dev = &driver_data->pdev->dev;
    /* END Acme_Wen@pegatroncorp.com [2012/03/05] [uevent for hall sensor] */

    if (!driver_data) {
        printk("[Hall Sensor] driver_data is NULL\n");
        return;
    }

    if (!driver_data->platform_data) {
        printk("[Hall Sensor] platform_data is NULL\n");
        return;
    }

    state = (gpio_get_value_cansleep(driver_data->platform_data->gpio) ? 0 : 1);

    /* BEGIN Acme_Wen@pegatroncorp.com [2012/03/05] [uevent for hall sensor] */
    if (state == 0)
        sprintf(event_string, "SWITCH_STATE=false");
    else
        sprintf(event_string, "SWITCH_STATE=true");

    kobject_uevent_env(&dev->kobj, KOBJ_CHANGE, envp);
    /* END Acme_Wen@pegatroncorp.com [2012/03/05] [uevent for hall sensor] */

    driver_data->current_state = state;

    printk("[Hall Sensor] state: %d\n", state);
}

int init_hall_sensor_gpio(struct platform_device *pdev, struct hall_sensor_driver_data *driver_data)
{
    int result;
    struct hall_sensor_platform_data *platform_data = pdev->dev.platform_data;
    const char *desc = "hall-sensor";

    struct pm_gpio hsensor_gpio_config = {
        .direction      = PM_GPIO_DIR_IN,
        .output_buffer  = PM_GPIO_OUT_BUF_CMOS,
        .output_value   = 0,
        .pull           = PM_GPIO_PULL_NO,
        .vin_sel        = PM_GPIO_VIN_BB,
//        .out_strength   = PM_GPIO_STRENGTH_HIGH,
//        .function       = PM_GPIO_FUNC_NORMAL,
        .inv_int_pol    = 0,
    }; 

    result = pm8xxx_gpio_config(platform_data->gpio, &hsensor_gpio_config);

    if (result < 0) {
        pr_err("[Hall Sensor] %s: pm8921 gpio %d config failed(%d)\n", __func__, platform_data->gpio, result);
        printk("[Hall Sensor] init_hall_sensor_gpio: pm8921 gpio %d config failed(%d)\n", platform_data->gpio, result);
        return result;
    }

    INIT_WORK(&driver_data->work, hall_sensor_work_func);

    result = gpio_request(platform_data->gpio, desc);
    if (result < 0) {
        gpio_free(platform_data->gpio);

        dev_err(&pdev->dev, "[Hall Sensor] failed to request GPIO %d, error %d\n", platform_data->gpio, result);
        printk("[Hall Sensor] init_hall_sensor_gpio: failed to request GPIO %d, error %d\n", platform_data->gpio, result);

        return result;
    }

    result = gpio_direction_input(platform_data->gpio);
    if (result < 0) {
        gpio_free(platform_data->gpio);

        dev_err(&pdev->dev, "[Hall Sensor] failed to configure direction for GPIO %d, error %d\n", platform_data->gpio, result);
        printk("[Hall Sensor] init_hall_sensor_gpio: failed to configure direction for GPIO %d, error %d\n", platform_data->gpio, result);

        return result;
    }

    return result;
}

static irqreturn_t hall_sensor_isr(int irq, void *dev_id)
{
    struct hall_sensor_driver_data *driver_data = dev_id;

    schedule_work(&driver_data->work);

    return IRQ_HANDLED;
}

int init_hall_sensor_irq(struct platform_device *pdev, struct hall_sensor_driver_data *driver_data)
{
    int irq, result;
    unsigned long irqflags = IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING;
    struct hall_sensor_platform_data *platform_data = pdev->dev.platform_data;
    const char *desc = "hall-sensor";

    result = gpio_to_irq(platform_data->gpio);

    if (result < 0) {
        // sysfs_remove_group(&pdev->dev.kobj, &gpio_keys_attr_group);

        dev_err(&pdev->dev, "[Hall Sensor] Unable to get irq number for GPIO %d, error %d\n", platform_data->gpio, result);
        printk("[Hall Sensor] init_hall_sensor_irq: Unable to get irq number for GPIO %d, error %d\n", platform_data->gpio, result);

        return result;
    }

    irq = result;

    result = request_any_context_irq(irq, hall_sensor_isr, irqflags, desc, driver_data);

    if (result < 0) {
        gpio_free(platform_data->gpio);

        dev_err(&pdev->dev, "[Hall Sensor] Unable to claim irq %d; error %d\n", irq, result);
        printk("[Hall Sensor] init_hall_sensor_irq: Unable to claim irq %d; error %d\n", irq, result);

        return result;
    }

    return result;
}

static int hs_vreg_enable(char *name, struct regulator *reg)
{
    int result = 0;

    reg = regulator_get(NULL, name);
    if (IS_ERR(reg)) {
		result = PTR_ERR(reg);
  		pr_err("%s: Unable to get apx9132_l11(%d)\n", __func__, result);
		goto fail_hs_vreg_l11;
	}


    result = regulator_set_voltage(reg, 3300000, 3300000);
    if (result) {
		pr_err("%s: set_voltage l11 failed(%d)\n", __func__, result);
		goto fail_hs_vreg_l11;
	}

    result = regulator_enable(reg);
 	if (result) {
		pr_err("%s: vreg_enable failed(%d)\n", __func__, result);
		goto fail_hs_vreg_l11;
	}

    return 0;
    
fail_hs_vreg_l11:
	regulator_put(reg);
    return result;
}

static int hs_vreg_disable(struct regulator *reg)
{
    int rc = 0;
    if (NULL == reg){
        printk("[Hall Sensor] Disable regulator cancel, pointer is NULL!");
        return 0;
    }
    
    rc = regulator_disable(reg);
    if (rc) {
		pr_err("[Hall Sensor]%s: Disable regulator failed(%d)\n", __func__, rc);
	}
	regulator_put(reg);
    return rc;
}

static int __devinit hall_sensor_probe(struct platform_device *pdev)
{
    int result = 0; //, wakeup = 0;
    bool wakeup = true;
    struct hall_sensor_driver_data *driver_data = 0;

    printk("[Hall Sensor] hall_sensor_probe...\n");

    // init driver data
    driver_data = init_driver_data(pdev);
    if (!driver_data) return -ENOMEM;
    
    // init attribute (node)
    result = sysfs_create_group(&pdev->dev.kobj, &hall_sensor_attr_group);
    if (result) {
        dev_err(&pdev->dev, "Unable to export state, error: %d\n", result);
        goto err0;
    }

    //BEGIN Kite_Yeh@pegatron [20120330][map L11 requlator for hall sensor]
    result = hs_vreg_enable("apx9132_l11", hs_vreg_l11);
    if (result){
        goto err1;
    }
    //END Kite_Yeh@pegatron [20120330][map L11 requlator for hall sensor]

    
    // init gpio
    result = init_hall_sensor_gpio(pdev, driver_data);
    if (result < 0) goto err1;
    
    // init irq
    result = init_hall_sensor_irq(pdev, driver_data);
    if (result < 0) goto err1;

    driver_data->pdev = pdev;

    device_init_wakeup(&pdev->dev, wakeup);

    return 0;

err1:
    sysfs_remove_group(&pdev->dev.kobj, &hall_sensor_attr_group);
err0:
    kfree(driver_data);
   
    return result;
}

static int __devexit hall_sensor_remove(struct platform_device *pdev)
{
    sysfs_remove_group(&pdev->dev.kobj, &hall_sensor_attr_group);
    return 0;
}


#ifdef CONFIG_PM
static int hall_sensor_suspend(struct device *dev)
{
    int irq = -1;
   	struct hall_sensor_platform_data *platform_data = dev->platform_data;
    printk("[Hall Sensor] hall_sensor_suspend...\n");
    irq = gpio_to_irq(platform_data->gpio);
    
	if (device_may_wakeup(dev))
		enable_irq_wake(irq);
    return 0;
}

static int hall_sensor_resume(struct device *dev)
{
    int irq = -1;
   	struct hall_sensor_platform_data *platform_data = dev->platform_data;
    printk("[Hall Sensor] hall_sensor_resume...\n");
    irq = gpio_to_irq(platform_data->gpio);
    
	if (device_may_wakeup(dev))
		disable_irq_wake(irq);
    return 0;
}

static const struct dev_pm_ops hall_sensor_pm_ops = {
    .suspend    = hall_sensor_suspend,
    .resume     = hall_sensor_resume,
};
#endif

static struct platform_driver hall_sensor_device_driver = {
    .probe    = hall_sensor_probe,
    .remove   = __devexit_p(hall_sensor_remove),
    .driver   = {
        .name   = "hall-sensor",
        .owner  = THIS_MODULE,
        #ifdef CONFIG_PM
        .pm    = &hall_sensor_pm_ops,
        #endif
    }
};

static int __init hall_sensor_init(void)
{
    return platform_driver_register(&hall_sensor_device_driver);
}

static void __exit hall_sensor_exit(void)
{
    hs_vreg_disable(hs_vreg_l11);
    platform_driver_unregister(&hall_sensor_device_driver);
}

module_init(hall_sensor_init);
module_exit(hall_sensor_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Phil Blundell <pb@handhelds.org>");
MODULE_DESCRIPTION("Hall sensor driver for pm8921 GPIOs");
MODULE_ALIAS("platform:hall-sensor");
