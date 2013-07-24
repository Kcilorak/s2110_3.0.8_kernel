#include <linux/delay.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/regulator/consumer.h>
#include <linux/leds.h>
#include <linux/leds-lm3559.h>
#include <linux/slab.h>
#include <mach/gpio.h>
#include <mach/gpiomux.h>

// for debug message by Dom
#undef CDBG
#define CDBG(fmt, args...) printk(KERN_DEBUG "[%s: %d] " fmt, __func__, __LINE__, ##args)

#undef CDBGE
#define CDBGE(fmt, args...) printk(KERN_ERR "[%s: %d] " fmt, __func__, __LINE__, ##args)

/*============================================================================
  I2C REGISTER DEFINES
 ============================================================================*/
/* Register read/write macros */
#define LM3559_R_BYTES     1
#define LM3559_W_BYTES     2 /* LM3559_W_BYTES = LM3559_R_BYTES + 1 */
#define LM3559_MAX_RETRIES 5

#define LM3559_WRITE_REG(reg, val) \
	do { \
		rc = lm3559_write_reg(client, reg, val); \
		if (rc) \
			goto err; \
	} while (0)

/* I2C Registers Address */
#define LM3559_ENABLE             0x10
#define LM3559_PRIVACY            0x11
#define LM3559_INDICATOR          0x12
#define LM3559_INDICATOR_BLINKING 0x13
#define LM3559_PRIVACY_PWM        0x14
#define LM3559_GPIO               0x20
#define LM3559_VLED_MONITOR       0x30
#define LM3559_ADC_DELAY          0x31
#define LM3559_VIN_MONITOR        0x80
#define LM3559_LAST_FLASH         0x81
#define LM3559_TORCH_BRIGHTNESS   0xA0
#define LM3559_FLASH_BRIGHTNESS   0xB0
#define LM3559_FLASH_DURATION     0xC0
#define LM3559_FLAGS              0xD0
#define LM3559_CONFIGURATION_1    0xE0
#define LM3559_CONFIGURATION_2    0xF0


/*============================================================================
  DATA DECLARATIONS
 ============================================================================*/
static struct i2c_client *lm3559_client = NULL;

/**
 * struct lm3559_leds_data - internal led data structure
 * @led_dev: led class device
 * @mode: mode of operation - auto, manual, torch, flash
 */
struct lm3559_leds_data {
	struct led_classdev led_dev;
	struct lm3559_leds_platform_data *pdata;
/* BEGIN Dom_Lin@pegatron [2012/04/17] [Pre-flash for focusing] */
	uint8_t torch_on;
/* END Dom_Lin@pegatron [2012/04/17] [Pre-flash for focusing] */
	uint8_t power_on;
	struct regulator *vio;
/* BEGIN Dom_Lin@pegatron [2011/02/02] [Enable back camera flash in torch mode in 1024 codebase] */
	uint8_t control_by_camera;
/* END Dom_Lin@pegatron [2011/02/02] [Enable back camera flash in torch mode in 1024 codebase] */
	uint8_t flash_brightness;
	uint8_t torch_brightness;
/* BEGIN Dom_Lin@pegatron [2012/04/17] [Add delay when taking picture in no light environment] */
	uint8_t flash_duration;
/* END Dom_Lin@pegatron [2012/04/17] [Add delay when taking picture in no light environment] */
/* BEGIN Dom_Lin@pegatron [2012/03/19] [Add camera LED new schematic support after DVT version in 1032 codebase] */
	uint8_t gsbi5;
/* END Dom_Lin@pegatron [2012/03/19] [Add camera LED new schematic support after DVT version in 1032 codebase] */
};

/* BEGIN Dom_Lin@pegatron [2011/02/02] [Init camera strobe flash function in 1024 codebase] */
#ifdef LM3559_DEBUG_NODE
struct lm3559_mode_map {
	const char *mode;
	enum lm3559_mode mode_val;
};

static struct lm3559_mode_map mode_map[] = {
	{ "auto",   LM3559_MODE_AUTO   },
	{ "torch",  LM3559_MODE_TORCH  },
};

struct lm3559_brightness_map {
	const char *str;
	uint8_t val;
	const char* cur;
};

static struct lm3559_brightness_map flash_b_map[] = {
	{ "0", (0 << 4 | 0), "112.5"},
	{ "1", (1 << 4 | 0), "168.75"},
	{ "2", (2 << 4 | 0), "225"},
	{ "3", (3 << 4 | 0), "281.25"},
	{ "4", (4 << 4 | 0), "337.5"},
	{ "5", (5 << 4 | 0), "393.75"},
	{ "6", (6 << 4 | 0), "450"},
	{ "7", (7 << 4 | 0), "506.25"},
	{ "8", (8 << 4 | 0), "562.5"},
	{ "9", (9 << 4 | 0), "618.75"},
	{ "10", (10 << 4 | 0), "675"},
	{ "11", (11 << 4 | 0), "731.25"},
	{ "12", (12 << 4 | 0), "787.5"},
	{ "13", (13 << 4 | 0), "843.75"},
	{ "14", (14 << 4 | 0), "900"},
	{ "15", (15 << 4 | 0), "956.25"},
	{ "16", (15 << 4 | 1), "1012.5"},
	{ "17", (15 << 4 | 2), "1068.75"},
	{ "18", (15 << 4 | 3), "1125"},
	{ "19", (15 << 4 | 4), "1181.25"},
	{ "20", (15 << 4 | 5), "1237.5"},
	{ "21", (15 << 4 | 6), "1293.75"},
	{ "22", (15 << 4 | 7), "1350"},
	{ "23", (15 << 4 | 8), "1406.25"},
	{ "24", (15 << 4 | 9), "1462.5"},
	{ "25", (15 << 4 | 10), "1518.75"},
	{ "26", (15 << 4 | 11), "1575"},
	{ "27", (15 << 4 | 12), "1631.25"},
	{ "28", (15 << 4 | 13), "1687.5"},
	{ "29", (15 << 4 | 14), "1743.75"},
	{ "30", (15 << 4 | 15), "1800"},
};

static struct lm3559_brightness_map torch_b_map[] = {
	{ "0", (0x40 | 0 << 3 | 0), "56.25"},
	{ "1", (0x40 | 1 << 3 | 0), "84.375"},
	{ "2", (0x40 | 2 << 3 | 0), "112.5"},
	{ "3", (0x40 | 3 << 3 | 0), "140.625"},
	{ "4", (0x40 | 4 << 3 | 0), "168.75"},
	{ "5", (0x40 | 5 << 3 | 0), "196.875"},
	{ "6", (0x40 | 6 << 3 | 0), "225"},
	{ "7", (0x40 | 7 << 3 | 0), "253.125"},
	{ "8", (0x40 | 7 << 3 | 1), "281.25"},
	{ "9", (0x40 | 7 << 3 | 2), "309.375"},
	{ "10", (0x40 | 7 << 3 | 3), "337.5"},
	{ "11", (0x40 | 7 << 3 | 4), "365.625"},
	{ "12", (0x40 | 7 << 3 | 5), "393.75"},
	{ "13", (0x40 | 7 << 3 | 6), "421.875"},
	{ "14", (0x40 | 7 << 3 | 7), "450"},
};

static int atoi(const char *name)
{
	int val = 0;

	for (;; name++) {
		switch (*name) {
		case '0' ... '9':
			val = 10*val+(*name-'0');
			break;
		default:
			return val;
		}
	}
}
#endif

enum strobe_flash_ctrl_type {
	STROBE_FLASH_CTRL_INIT,
	STROBE_FLASH_CTRL_CHARGE,
	STROBE_FLASH_CTRL_RELEASE
};
/* END Dom_Lin@pegatron [2011/02/02] [Init camera strobe flash function in 1024 codebase] */

#define MSM_CAMERA_LED_OFF     0
#define MSM_CAMERA_LED_LOW     1
#define MSM_CAMERA_LED_HIGH    2
#define MSM_CAMERA_LED_INIT    3
#define MSM_CAMERA_LED_RELEASE 4

/* BEGIN Dom_Lin@pegatron [2012/03/19] [Add camera LED new schematic support after DVT version in 1032 codebase] */
static int lm3559_sda[] = {20, 24};
static int lm3559_scl[] = {21, 25};
/* END Dom_Lin@pegatron [2012/03/19] [Add camera LED new schematic support after DVT version in 1032 codebase] */

/*============================================================================
  REGISTER R/W
 ============================================================================*/
static int lm3559_write_reg(struct i2c_client* client, uint8_t reg, uint8_t val)
{
	int rc = 0;
	int bytes = 0;
	int i = 0;
	uint8_t buf[LM3559_W_BYTES] = { reg, val };

//	CDBG("addr: %02x, data: %02x\n", reg, val);
	if (client == NULL) {
		CDBGE("i2c client is NULL\n");
		rc = -EUNATCH;
	}
	else {
		do {
			bytes = i2c_master_send(client, buf, LM3559_W_BYTES);
		} while (bytes != LM3559_W_BYTES && (++i) < LM3559_MAX_RETRIES);
		if (bytes != LM3559_W_BYTES) {
			CDBGE("i2c_master_send error\n");
			rc = -EINVAL;
		}
	}
	return rc;
}

static int lm3559_init_registers(struct i2c_client *client)
{
	struct lm3559_leds_data *drvdata = i2c_get_clientdata(client);
	int rc = 0;
	CDBG("+++\n");

	// Default
#if 0 // Use Power On/RESET value
	LM3559_WRITE_REG(LM3559_ENABLE,             0x18);
	LM3559_WRITE_REG(LM3559_PRIVACY,            0x58);
	LM3559_WRITE_REG(LM3559_INDICATOR,          0x00);
	LM3559_WRITE_REG(LM3559_INDICATOR_BLINKING, 0x00);
	LM3559_WRITE_REG(LM3559_PRIVACY_PWM,        0xF0);
	LM3559_WRITE_REG(LM3559_GPIO,               0x80);
	LM3559_WRITE_REG(LM3559_VLED_MONITOR,       0x80);
	LM3559_WRITE_REG(LM3559_ADC_DELAY,          0xC0);
	LM3559_WRITE_REG(LM3559_VIN_MONITOR,        0xC0);
	LM3559_WRITE_REG(LM3559_LAST_FLASH,         0x00);
#endif
	LM3559_WRITE_REG(LM3559_TORCH_BRIGHTNESS,   drvdata->torch_brightness);//0x52);
/* BEGIN Dom_Lin@pegatron [2011/02/02] [Init camera strobe flash function in 1024 codebase] */
	LM3559_WRITE_REG(LM3559_FLASH_BRIGHTNESS,   drvdata->flash_brightness);//0xDD);
/* END Dom_Lin@pegatron [2011/02/02] [Init camera strobe flash function in 1024 codebase] */
#if 0 // Use Power On/RESET value
	LM3559_WRITE_REG(LM3559_FLASH_DURATION,     drvdata->flash_duration);
	LM3559_WRITE_REG(LM3559_FLAGS,              0x00);
	LM3559_WRITE_REG(LM3559_CONFIGURATION_1,    0x68);
	LM3559_WRITE_REG(LM3559_CONFIGURATION_2,    0xF0);
#endif

err:
//	CDBG("---\n");
	return rc;
}

static int lm3559_led_init(struct i2c_client *client)
{
	int rc = 0;
	struct lm3559_leds_data *drvdata = i2c_get_clientdata(client);

	CDBG("+++\n");
	if (drvdata->power_on == 0) {
		drvdata->vio = regulator_get(NULL, "8921_lvs5");
		if (IS_ERR(drvdata->vio)) {
			CDBGE("VFEG 8921 LVS5 get failed\n");
			drvdata->vio = NULL;
			goto err_reg_fail;
		}
		if (regulator_enable(drvdata->vio)) {
			CDBGE("VFEG 8921 LVS5 enable failed\n");
			goto err_reg_enable_fail;
		}
		if (drvdata->pdata->gpiomux_conf_tbl) {
			msm_gpiomux_install(
				(struct msm_gpiomux_config *)drvdata->pdata->gpiomux_conf_tbl,
				drvdata->pdata->gpiomux_conf_tbl_size);
		}

/* BEGIN Dom_Lin@pegatron [2011/02/02] [Enable back camera flash in torch mode in 1024 codebase] */
		if (!drvdata->control_by_camera) {
			rc = gpio_request(lm3559_sda[drvdata->gsbi5], LM3559_LEDS_DEV_NAME);
			if (rc) {
				CDBGE("gpio request SDA fail");
				goto err_req_sda;
			}

			rc = gpio_request(lm3559_scl[drvdata->gsbi5], LM3559_LEDS_DEV_NAME);
			if (rc) {
				CDBGE("gpio request SCL fail");
				goto err_req_scl;
			}
		}
/* END Dom_Lin@pegatron [2011/02/02] [Enable back camera flash in torch mode in 1024 codebase] */

		rc = gpio_request(drvdata->pdata->hw_enable, LM3559_LEDS_DEV_NAME);
		if (rc) {
			CDBGE("gpio request hw_enable fail");
			goto err_req_hwen;
		}
		gpio_direction_output(drvdata->pdata->hw_enable, 1);
		usleep_range(1000, 2000);

		rc = lm3559_init_registers(client);
		if (rc) {
			CDBGE("Init registers failed\n");
			goto err_init_reg;
		}
		drvdata->power_on = 1;
		drvdata->torch_on = 0;
	}
	goto done;

err_init_reg:
	gpio_set_value_cansleep(drvdata->pdata->hw_enable, 0);
	gpio_free(drvdata->pdata->hw_enable);
err_req_hwen:
/* BEGIN Dom_Lin@pegatron [2011/02/02] [Enable back camera flash in torch mode in 1024 codebase] */
	if (!drvdata->control_by_camera)
		gpio_free(lm3559_scl[drvdata->gsbi5]);
err_req_scl:
	if (!drvdata->control_by_camera)
		gpio_free(lm3559_sda[drvdata->gsbi5]);
/* END Dom_Lin@pegatron [2011/02/02] [Enable back camera flash in torch mode in 1024 codebase] */
err_req_sda:
	regulator_disable(drvdata->vio);
err_reg_enable_fail:
	regulator_put(drvdata->vio);
	drvdata->vio = NULL;
err_reg_fail:
done:
//	CDBG("---\n");
	return rc;
}

static int lm3559_led_release(struct i2c_client *client)
{
	int rc = 0;
	struct lm3559_leds_data *drvdata = i2c_get_clientdata(client);

	CDBG("+++\n");
	if (drvdata->power_on) {
		CDBG("release\n");
		gpio_set_value_cansleep(drvdata->pdata->hw_enable, 0);
		gpio_free(drvdata->pdata->hw_enable);
/* BEGIN Dom_Lin@pegatron [2011/02/02] [Enable back camera flash in torch mode in 1024 codebase] */
		if (!drvdata->control_by_camera) {
			gpio_free(lm3559_scl[drvdata->gsbi5]);
			gpio_free(lm3559_sda[drvdata->gsbi5]);
		}
		regulator_disable(drvdata->vio);
		regulator_put(drvdata->vio);
		drvdata->vio = NULL;
		drvdata->power_on = 0;
		drvdata->torch_on = 0;
/* END Dom_Lin@pegatron [2011/02/02] [Enable back camera flash in torch mode in 1024 codebase] */
	}
	drvdata->control_by_camera = 0;
//	CDBG("---\n");
	return rc;
}

static int lm3559_led_off(struct i2c_client *client)
{
	int rc = 0;
	struct lm3559_leds_data *drvdata = i2c_get_clientdata(client);

	CDBG("+++\n");
	LM3559_WRITE_REG(LM3559_ENABLE, 0x18); // Current Sources are Shutdown
	if (!drvdata->torch_on) {
/* BEGIN Dom_Lin@pegatron [2011/02/02] [Init camera strobe flash function in 1024 codebase] */
		LM3559_WRITE_REG(LM3559_CONFIGURATION_1, 0x68); // STROBE pin disabled
/* END Dom_Lin@pegatron [2011/02/02] [Init camera strobe flash function in 1024 codebase] */
	}

	drvdata->torch_on = 0;

err:
//	CDBG("---\n");
	return rc;
}

static int lm3559_led_torch(struct i2c_client *client)
{
	int rc = 0;
	struct lm3559_leds_data *drvdata = i2c_get_clientdata(client);
	drvdata->torch_on = 1;

	CDBG("+++\n");
	LM3559_WRITE_REG(LM3559_ENABLE, 0x1A); // Torch Mode

err:
//	CDBG("---\n");
	return rc;
}

static int lm3559_led_flash(struct i2c_client *client)
{
	int rc = 0;
	struct lm3559_leds_data *drvdata = i2c_get_clientdata(client);
	drvdata->torch_on = 0;

	CDBG("+++\n");
/* BEGIN Dom_Lin@pegatron [2011/02/02] [Init camera strobe flash function in 1024 codebase] */
	LM3559_WRITE_REG(LM3559_CONFIGURATION_1, 0x6C); // STROBE pin enabled
/* END Dom_Lin@pegatron [2011/02/02] [Init camera strobe flash function in 1024 codebase] */

err:
//	CDBG("---\n");
	return rc;
}

int lm3559_set_led(unsigned led_state)
{
	int rc = 0;
/* BEGIN Dom_Lin@pegatron [2011/02/02] [Enable back camera flash in torch mode in 1024 codebase] */
	struct lm3559_leds_data *drvdata = i2c_get_clientdata(lm3559_client);
	drvdata->control_by_camera = 1;
/* END Dom_Lin@pegatron [2011/02/02] [Enable back camera flash in torch mode in 1024 codebase] */

	switch (led_state) {

	case MSM_CAMERA_LED_INIT: // i2c_add_driver, GPIO request and control power up
		rc = lm3559_led_init(lm3559_client);
		break;
	case MSM_CAMERA_LED_RELEASE: // GPIO control power down and free
		rc = lm3559_led_release(lm3559_client);
		break;
	case MSM_CAMERA_LED_OFF:
		rc = lm3559_led_off(lm3559_client);
		break;
	case MSM_CAMERA_LED_LOW:
		rc = lm3559_led_torch(lm3559_client);
		break;
	case MSM_CAMERA_LED_HIGH:
		rc = lm3559_led_flash(lm3559_client);
		break;
	default:
		rc = -EFAULT;
		break;
	}
	if (rc != 0)
		drvdata->control_by_camera = 0;
//	CDBG("---\n");
	return rc;
}
EXPORT_SYMBOL(lm3559_set_led);

/* BEGIN Dom_Lin@pegatron [2011/02/02] [Init camera strobe flash function in 1024 codebase] */
int lm3559_set_strobe(unsigned type, int charge_en)
{
	int rc = 0;
	struct lm3559_leds_data *drvdata = i2c_get_clientdata(lm3559_client);
	drvdata->control_by_camera = 1;

	CDBG("type = %u\n", type);
	switch(type) {
	case STROBE_FLASH_CTRL_INIT:
		break;
	case STROBE_FLASH_CTRL_CHARGE:
		if (charge_en)
			rc = lm3559_led_flash(lm3559_client);
		else
			rc = lm3559_led_off(lm3559_client);
		break;
	case STROBE_FLASH_CTRL_RELEASE:
		break;
	default:
		rc = -EFAULT;
		break;
	}
//	CDBG("---\n");
	return rc;
}
EXPORT_SYMBOL(lm3559_set_strobe);

/* BEGIN Dom_Lin@pegatron [2012/04/17] [Add delay when taking picture in no light environment] */
int lm3559_set_duration(uint8_t duration)
{
	int rc = 0;
	struct lm3559_leds_data *drvdata = i2c_get_clientdata(lm3559_client);
    struct i2c_client *client = lm3559_client;
	drvdata->control_by_camera = 1;

	CDBG("duration = 0x%02x\n", duration);
	if (duration != drvdata->flash_duration) {
		drvdata->flash_duration = duration;
		LM3559_WRITE_REG(LM3559_FLASH_DURATION, drvdata->flash_duration);
	}

err:
//	CDBG("---\n");
	return rc;
}
EXPORT_SYMBOL(lm3559_set_duration);
/* END Dom_Lin@pegatron [2012/04/17] [Add delay when taking picture in no light environment] */

#ifdef LM3559_DEBUG_NODE
/* END Dom_Lin@pegatron [2011/02/02] [Init camera strobe flash function in 1024 codebase] */
static int lm3559_get_mode_from_str(const char *str)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(mode_map); i++)
		if (sysfs_streq(str, mode_map[i].mode))
			return mode_map[i].mode_val;

	return -1;
}

static ssize_t lm3559_mode_get(struct device *dev, struct device_attribute
                               *attr, char *buf)
{
	struct i2c_client *client = container_of(dev->parent, struct i2c_client, dev);
	struct lm3559_leds_data *drvdata = i2c_get_clientdata(client);

/* BEGIN Dom_Lin@pegatron [2011/02/02] [Init camera strobe flash function in 1024 codebase] */
	return snprintf(buf, PAGE_SIZE, "Current: %s\nSupported: %s, %s\n",
			mode_map[drvdata->pdata->mode].mode,
			mode_map[0].mode, mode_map[1].mode);
/* END Dom_Lin@pegatron [2011/02/02] [Init camera strobe flash function in 1024 codebase] */
}

static ssize_t lm3559_mode_set(struct device *dev, struct device_attribute
                               *attr, const char *buf, size_t size)
{
	struct i2c_client *client = container_of(dev->parent, struct i2c_client, dev);
	struct lm3559_leds_data *drvdata = i2c_get_clientdata(client);
	enum lm3559_mode mode;
	int rc = 0;
	CDBG("+++\n");
/* BEGIN Dom_Lin@pegatron [2011/02/02] [Enable back camera flash in torch mode in 1024 codebase] */
	if (drvdata->control_by_camera) {
		CDBG("Already controlled by camera\n");
		return -EINVAL;
	}
/* END Dom_Lin@pegatron [2011/02/02] [Enable back camera flash in torch mode in 1024 codebase] */

	mode = lm3559_get_mode_from_str(buf);
	if (drvdata->pdata->mode != mode) {
/* BEGIN Dom_Lin@pegatron [2011/02/02] [Enable back camera flash in torch mode in 1024 codebase] */
		switch (mode) {
		case LM3559_MODE_AUTO:
			rc = lm3559_led_off(client);
			if (!rc)
				rc = lm3559_led_release(client);
			break;
		case LM3559_MODE_TORCH:
			rc = lm3559_led_init(client);
			if (!rc)
				rc = lm3559_led_torch(client);
			break;
		default:
			CDBGE("Invalid mode\n");
			return -EINVAL;
		}
/* END Dom_Lin@pegatron [2011/02/02] [Enable back camera flash in torch mode in 1024 codebase] */
		if (rc) {
			CDBGE("Setting %s Mode failed: %d\n", mode_map[mode].mode, rc);
			return -EINVAL;
		}
		drvdata->pdata->mode = mode;
		CDBG("Set to mode: %s\n", buf);
	}

//	CDBG("---\n");
	return size;
}
static DEVICE_ATTR(mode, S_IRUGO | S_IWUSR, lm3559_mode_get, lm3559_mode_set);

static uint8_t lm3559_get_flash_b_from_str(const char *str)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(flash_b_map); i++)
		if (sysfs_streq(str, flash_b_map[i].str))
			return flash_b_map[i].val;

	return ARRAY_SIZE(flash_b_map);
}

static ssize_t lm3559_flash_b_get(struct device *dev, struct device_attribute
                                  *attr, char *buf)
{
	struct i2c_client *client = container_of(dev->parent, struct i2c_client, dev);
	struct lm3559_leds_data *drvdata = i2c_get_clientdata(client);
	int i;
	ssize_t size;
	size = snprintf(buf, PAGE_SIZE, "\nFlash Brightness Support List:\n");
	for(i=0; i<ARRAY_SIZE(flash_b_map); ++i)
		size = snprintf(buf, PAGE_SIZE, "%s[%2d] 0x%02X = %7s mA\n", buf, i, flash_b_map[i].val, flash_b_map[i].cur);

	size = snprintf(buf, PAGE_SIZE, "%s\nUsing 0x%02X ...\n", buf, drvdata->flash_brightness);

	return size;
}

static ssize_t lm3559_flash_b_set(struct device *dev, struct device_attribute
                                  *attr, const char *buf, size_t size)
{
	struct i2c_client *client = container_of(dev->parent, struct i2c_client, dev);
	struct lm3559_leds_data *drvdata = i2c_get_clientdata(client);
	uint8_t val;
	CDBG("+++\n");
#if 0
	if (drvdata->control_by_camera) {
		CDBG("Already controlled by camera\n");
		return -EINVAL;
	}
#endif

	val = lm3559_get_flash_b_from_str(buf);
	if (val != ARRAY_SIZE(flash_b_map)) {
		CDBG("val: 0x%02X = %s mA \n", val, flash_b_map[atoi(buf)].cur);
		drvdata->flash_brightness = val;
	}
//	CDBG("---\n");
	return size;
}
static DEVICE_ATTR(flash_b, S_IRUGO | S_IWUSR, lm3559_flash_b_get, lm3559_flash_b_set);

static uint8_t lm3559_get_torch_b_from_str(const char *str)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(torch_b_map); i++)
		if (sysfs_streq(str, torch_b_map[i].str))
			return torch_b_map[i].val;

	return ARRAY_SIZE(torch_b_map);
}

static ssize_t lm3559_torch_b_get(struct device *dev, struct device_attribute
                                  *attr, char *buf)
{
	struct i2c_client *client = container_of(dev->parent, struct i2c_client, dev);
	struct lm3559_leds_data *drvdata = i2c_get_clientdata(client);
	int i;
	ssize_t size;
	size = snprintf(buf, PAGE_SIZE, "\nTorch Brightness Support List:\n");
	for(i=0; i<ARRAY_SIZE(torch_b_map); ++i)
		size = snprintf(buf, PAGE_SIZE, "%s[%2d] 0x%02X = %7s mA\n", buf, i, torch_b_map[i].val, torch_b_map[i].cur);

	size = snprintf(buf, PAGE_SIZE, "%s\nUsing 0x%02X ...\n", buf, drvdata->torch_brightness);

	return size;
}

static ssize_t lm3559_torch_b_set(struct device *dev, struct device_attribute
                                  *attr, const char *buf, size_t size)
{
	struct i2c_client *client = container_of(dev->parent, struct i2c_client, dev);
	struct lm3559_leds_data *drvdata = i2c_get_clientdata(client);
	uint8_t val;
	CDBG("+++\n");
#if 0
	if (drvdata->control_by_camera) {
		CDBG("Already controlled by camera\n");
		return -EINVAL;
	}
#endif

	val = lm3559_get_torch_b_from_str(buf);
	if (val != ARRAY_SIZE(torch_b_map)) {
		CDBG("val: 0x%02X = %s mA \n", val, torch_b_map[atoi(buf)].cur);
		drvdata->torch_brightness = val;
	}
//	CDBG("---\n");
	return size;
}
static DEVICE_ATTR(torch_b, S_IRUGO | S_IWUSR, lm3559_torch_b_get, lm3559_torch_b_set);

/* BEGIN Dom_Lin@pegatron [2011/02/02] [Init camera strobe flash function in 1024 codebase] */
#endif
/* END Dom_Lin@pegatron [2011/02/02] [Init camera strobe flash function in 1024 codebase] */

static int __devinit lm3559_probe(struct i2c_client *client,
                                  const struct i2c_device_id *id)
{
	int rc = 0;
	struct lm3559_leds_platform_data *pdata = client->dev.platform_data;
	struct lm3559_leds_data *drvdata;

	CDBG("+++\n");
	if (pdata == NULL) {
		CDBGE("platform data required\n");
		return -ENODEV;
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		CDBGE("client not i2c capable\n");
		return -ENODEV;
	}

/* BEGIN Dom_Lin@pegatron [2011/02/02] [Add exception handing when probing led driver IC] */
	if(!i2c_get_clientdata(client)) {
		drvdata = kzalloc(sizeof(struct lm3559_leds_data), GFP_KERNEL);
		if (drvdata == NULL) {
			CDBGE("failed to alloc memory\n");
			return -ENOMEM;
		}

		drvdata->led_dev.name = LM3559_LED0_NAME;
		drvdata->pdata = pdata;
		drvdata->torch_on = 0;
		drvdata->power_on = 0;
		drvdata->control_by_camera = 0;
		drvdata->flash_brightness = 0x40; // 337.5 mA, 0x20; // 225mA
		drvdata->torch_brightness = 0x7B; // 337.5 mA, 0x60; // 168.75mA
/* BEGIN Dom_Lin@pegatron [2012/04/17] [Add delay when taking picture in no light environment] */
		drvdata->flash_duration = 0x6F; // 512 ms
/* END Dom_Lin@pegatron [2012/04/17] [Add delay when taking picture in no light environment] */
		if (!strcmp(dev_name(&(client->dev)), "5-0053")) {
			CDBG("Use GSBI5 after DVT version\n");
			drvdata->gsbi5 = 1;
		}
		i2c_set_clientdata(client, drvdata);

		rc = led_classdev_register(&client->dev, &drvdata->led_dev);
		if (rc) {
			CDBGE("unable to register led, rc=%d\n", rc);
			goto err_class_register;
		}

/* BEGIN Dom_Lin@pegatron [2011/02/02] [Init camera strobe flash function in 1024 codebase] */
#ifdef LM3559_DEBUG_NODE
		rc = device_create_file(drvdata->led_dev.dev, &dev_attr_mode);
		if (rc) {
			CDBGE("File device creation failed: %d\n", rc);
			goto err_create_file;
		}
		rc = device_create_file(drvdata->led_dev.dev, &dev_attr_flash_b);
		if (rc) {
			CDBGE("File device creation failed: %d\n", rc);
			goto err_create_flash_b;
		}
		rc = device_create_file(drvdata->led_dev.dev, &dev_attr_torch_b);
		if (rc) {
			CDBGE("File device creation failed: %d\n", rc);
			goto err_create_torch_b;
		}
#endif
	}
	lm3559_client = client;
/* END Dom_Lin@pegatron [2011/02/02] [Add exception handing when probing led driver IC] */
	goto done;

#ifdef LM3559_DEBUG_NODE
err_create_torch_b:
	device_remove_file(drvdata->led_dev.dev, &dev_attr_flash_b);
err_create_flash_b:
	device_remove_file(drvdata->led_dev.dev, &dev_attr_mode);
err_create_file:
	led_classdev_unregister(&drvdata->led_dev);
#endif
/* END Dom_Lin@pegatron [2011/02/02] [Init camera strobe flash function in 1024 codebase] */
err_class_register:
	kfree(drvdata);
done:
//	CDBG("---\n");
	return rc;
}

static int __devexit lm3559_remove(struct i2c_client *client)
{
	struct lm3559_leds_data *drvdata = i2c_get_clientdata(client);
	CDBG("+++\n");
	if (drvdata) {
		lm3559_set_led(MSM_CAMERA_LED_OFF);
		lm3559_set_led(MSM_CAMERA_LED_RELEASE);

#ifdef LM3559_DEBUG_NODE
		device_remove_file(drvdata->led_dev.dev, &dev_attr_torch_b);
		device_remove_file(drvdata->led_dev.dev, &dev_attr_flash_b);
		device_remove_file(drvdata->led_dev.dev, &dev_attr_mode);
#endif
		led_classdev_unregister(&drvdata->led_dev);
		kfree(drvdata);
		i2c_set_clientdata(client, NULL);
		lm3559_client = NULL;
	}
//	CDBG("---\n");
	return 0;
}

static const struct i2c_device_id lm3559_id[] = {
	{LM3559_LEDS_DEV_NAME, 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, lm3559_id);

static struct i2c_driver lm3559_i2c_driver = {
	.probe = lm3559_probe,
	.remove = lm3559_remove,
	.id_table = lm3559_id,
	.driver = {
		.name = LM3559_LEDS_DEV_NAME,
		.owner = THIS_MODULE,
	},
};

static int __init lm3559_init(void)
{
	return i2c_add_driver(&lm3559_i2c_driver);
}

static void __exit lm3559_exit(void)
{
	i2c_del_driver(&lm3559_i2c_driver);
}

module_init(lm3559_init);
module_exit(lm3559_exit);

MODULE_DESCRIPTION("LED driver for LM3559");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("0.0");
MODULE_AUTHOR("Dom Lin <dom_lin@pegatroncorp.com>");
