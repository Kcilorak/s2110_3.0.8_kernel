
#include <linux/switch.h>
/* BEGIN-20111221-paul add msleep api. */
#include <linux/delay.h>
/* END-20111221-paul add msleep api. */

/*GP14_JACK_IN_DET*/
#define JACK_IN_DET_PIN 14

void delay_200ms(void);

static struct gpio_switch_platform_data headset_switch_data = {
	.name= "h2w1",
	.gpio= JACK_IN_DET_PIN, 
	.ext_handler= delay_200ms,
	.irq_trigger = IRQF_TRIGGER_FALLING,
};

static struct platform_device headset_switch_device =
{
	.name = "switch-gpio",
	.id = 1,
	.dev = {
		.platform_data = &headset_switch_data,
	}
};

static struct gpiomux_setting gpio_headset_switch_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
};

struct msm_gpiomux_config msm8960_headset_switch_configs[] __initdata = {
	{
		.gpio = JACK_IN_DET_PIN,
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_headset_switch_config,
		}
	},
};

void __init msm8960_init_headset_switch_detect(void)
{
	printk("%s:%d\n",__func__,platform_device_register(&headset_switch_device));

	msm_gpiomux_install(msm8960_headset_switch_configs,
			ARRAY_SIZE(msm8960_headset_switch_configs));		
}

void delay_200ms(void)
{
	msleep(200);
}

