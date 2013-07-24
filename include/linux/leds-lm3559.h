#ifndef _LINUX_LED_LM3559_H__
#define _LINUX_LED_LM3559_H__

#define LM3559_LEDS_DEV_NAME "lm3559-led"
#define LM3559_LED0_NAME     "led0"

/* BEGIN Dom_Lin@pegatron [2011/02/02] [Init camera strobe flash function in 1024 codebase] */
#define LM3559_DEBUG_NODE

#ifdef LM3559_DEBUG_NODE
enum lm3559_mode {
	LM3559_MODE_AUTO = 0, /* "auto" */
	LM3559_MODE_TORCH,    /* "torch" */
};
#endif
/* END Dom_Lin@pegatron [2011/02/02] [Init camera strobe flash function in 1024 codebase] */

/**
 * struct lm3559_platform_data
 */
struct lm3559_leds_platform_data {
    int hw_enable;
	void *gpiomux_conf_tbl;
	uint8_t gpiomux_conf_tbl_size;
/* BEGIN Dom_Lin@pegatron [2011/02/02] [Init camera strobe flash function in 1024 codebase] */
#ifdef LM3559_DEBUG_NODE
	enum lm3559_mode mode;
#endif
/* END Dom_Lin@pegatron [2011/02/02] [Init camera strobe flash function in 1024 codebase] */
};

int lm3559_set_led(unsigned led_state);
/* BEGIN Dom_Lin@pegatron [2011/02/02] [Init camera strobe flash function in 1024 codebase] */
int lm3559_set_strobe(unsigned type, int charge_en);
/* END Dom_Lin@pegatron [2011/02/02] [Init camera strobe flash function in 1024 codebase] */
/* BEGIN Dom_Lin@pegatron [2012/04/17] [Add delay when taking picture in no light environment] */
int lm3559_set_duration(uint8_t duration);
/* END Dom_Lin@pegatron [2012/04/17] [Add delay when taking picture in no light environment] */

#endif	/* _LINUX_LED_LM3559_H__ */
