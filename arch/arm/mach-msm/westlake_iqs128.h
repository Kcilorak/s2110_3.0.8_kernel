

#include <linux/switch.h>
#include <linux/delay.h>
 
#define ISQ_128_PIN 52

void Pega_delay_200ms(void);

static struct gpio_switch_platform_data iqs128_switch_data = {
       .name = "iqs128",
	   .gpio = ISQ_128_PIN,
	   .state_off = "CAP_SENSOR_PRESS",
	   .state_on = "CAP_SENSOR_RELEASE",
	   .ext_handler= Pega_delay_200ms,
	   .irq_trigger = IRQF_TRIGGER_FALLING |IRQF_TRIGGER_RISING,
	 };

static struct platform_device iqs128_switch_device = {
	   .name = "switch-gpio",
	   .id= 3,
	   .dev = {
	   .platform_data = &iqs128_switch_data,
			  }
        };

static struct gpiomux_setting gpio_IQS128_switch_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
};


struct msm_gpiomux_config msm8960_IQS128_switch_configs[] __initdata = {
	{
		.gpio = ISQ_128_PIN,
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_IQS128_switch_config,
		}
	},
};

void __init Cap_sensor_iqs52_init(void)
			{
			    
			       //pega_Cap_Sensor_gpio_set();
				   //platform_device_register(&iqs52_switch_device);
				   platform_device_register(&iqs128_switch_device);
			       msm_gpiomux_install(msm8960_IQS128_switch_configs,
			       ARRAY_SIZE(msm8960_IQS128_switch_configs));
}
void Pega_delay_200ms(void)
{
	msleep(200);
}

/*Evan*/
