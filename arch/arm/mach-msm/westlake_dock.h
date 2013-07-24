
#include <linux/switch.h>
#include <linux/interrupt.h>
#include <asm/setup.h>		//0402	WeiHuai add to support HW board ID

/*GP14_JACK_IN_DET*/
#define DOCK_IN_DET_PIN_7	7
#define DOCK_IN_DET_PIN_35	35

int docking_interrupt(void);
static struct gpio_switch_platform_data dock_switch_data = {
	.name= "dock",
	.gpio= DOCK_IN_DET_PIN_7,
	.state_on = "0",
       .state_off = "1",
	.ext_int_handler = docking_interrupt,
	.irq_trigger = IRQF_TRIGGER_FALLING |IRQF_TRIGGER_RISING,
};

static struct platform_device dock_switch_device =
{
	.name = "switch-gpio",
	.id = 2,
	.dev = {
		.platform_data = &dock_switch_data,
	}
};

static struct gpiomux_setting gpio_dock_switch_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_UP,
};

struct msm_gpiomux_config msm8960_dock_switch_configs[] __initdata = {
	{
		.gpio = DOCK_IN_DET_PIN_7,
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_dock_switch_config,
		}
	},
};

void __init msm8960_init_dock_switch_detect(int gpio_pin)
{
/*robert1_chen, 20120409, for Dock GSBI change -- BEGIN*/
	dock_switch_data.gpio = gpio_pin;
	msm8960_dock_switch_configs[0].gpio = gpio_pin;
	printk("%s:dock_det - %d\n",__func__,gpio_pin);
/*robert1_chen, 20120409, for Dock GSBI change -- END*/
	printk("%s:%d\n",__func__,platform_device_register(&dock_switch_device));

	msm_gpiomux_install(msm8960_dock_switch_configs,
			ARRAY_SIZE(msm8960_dock_switch_configs));		
}

