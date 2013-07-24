//[*]--------------------------------------------------------------------------------------------------[*]
//
//
// 
//  I2C Touchscreen driver (platform data struct)
//  2012.01.17
// 
//
//[*]--------------------------------------------------------------------------------------------------[*]
#ifndef __TOUCH_PDATA_H__
#define __TOUCH_PDATA_H__

//[*]--------------------------------------------------------------------------------------------------[*]
#ifdef CONFIG_HAS_EARLYSUSPEND
	#include <linux/earlysuspend.h>
#endif

//[*]--------------------------------------------------------------------------------------------------[*]
//[*]--------------------------------------------------------------------------------------------------[*]
#define	I2C_TOUCH_NAME		"lgd-ts"

#define	MAX_PROTOCOL_SIZE	32		// Get data register size
#define	MAX_NUM_FINGERS		10		// max finger count

//#define	I2C_RESTART_USED
//[*]--------------------------------------------------------------------------------------------------[*]
// Touch Event type define
//[*]--------------------------------------------------------------------------------------------------[*]
#define	TS_EVENT_UNKNOWN	0x00
#define	TS_EVENT_RELEASE	0x01
#define	TS_EVENT_MOVE		0x02

//[*]--------------------------------------------------------------------------------------------------[*]
typedef	struct	finger__t	{
	unsigned int	event;
	unsigned int	x;
	unsigned int	y;
	unsigned int	area;
	unsigned int	pressure;
}	__attribute__ ((packed))	finger_t;

//[*]--------------------------------------------------------------------------------------------------[*]
struct touch {
	int									irq;
	struct i2c_client 	*client;
	struct touch_pdata	*pdata;
	struct input_dev		*input;
	char								phys[32];

	// finger data
	finger_t						finger[MAX_NUM_FINGERS];

	// sysfs control flags
	unsigned char				disabled;		// interrupt status
	unsigned char				fw_version		;
	
	unsigned char				*fw_buf;
	unsigned int				fw_size;
	int									fw_status;

#if	defined(CONFIG_HAS_EARLYSUSPEND)
	struct	early_suspend		power;
#endif	
};

//[*]--------------------------------------------------------------------------------------------------[*]
//[*]--------------------------------------------------------------------------------------------------[*]
// Register define (touch_i2c_write, touch_i2c_read function used)
//[*]--------------------------------------------------------------------------------------------------[*]
#define	REG_TS_UPDATE		0x04	// 1 byte write
#define	REG_TS_CALIBRATION	0x05	// 1 byte write

#define	REG_TS_IDLE_WR		0x11	// 1 byte write
#define	REG_TS_IDLE_RD		0x17	// 1 byte write
#define	REG_TS_FIRMWARE_ID	0xD0	// 1 byte read
#define	REG_TS_LENGTH		0xF5	// 1 byte read
#define	REG_TS_DATA			0xFA	// Length read from TS_LENGTH register

#define	REG_TS_FW_STATUS	0x12

#define	REG_TS_USERMODE		0x77

#define	REG_TS_MAGIC_WR		0xF6
#define	REG_TS_MAGIC_RD		0xF8

#define	TOUCH_USER_MODE		0x55
#define	TOUCH_BOOT_MODE		0x88

//[*]--------------------------------------------------------------------------------------------------[*]
// F/W Update control, Boot mode commnd define (touch_i2c_bootmode_read, touch_i2c_bootmode_write function used)
//[*]--------------------------------------------------------------------------------------------------[*]
typedef	struct	command__t	{
	unsigned char	reg;		// flash read/write command
	unsigned int	addr;		// start addr
	unsigned short	len;		// read/write size
}	__attribute__ ((packed))	command_t;

typedef	struct	erase_cmd__t	{
	unsigned char	reg;		// flash read/write command
	unsigned int	addr;		// start addr
	unsigned int	len;		// erase size
}	__attribute__ ((packed))	erase_cmd_t;

#define	FLASH_START_ADDR		0x08000000
#define	FLASH_BOOT_CODE_OFFSET	0x00000000
#define	FLASH_BOOT_CODE_SIZE	0x00002000		// 8 Kbytes
#define	FLASH_USER_CODE_OFFSET	0x00002000
#define	FLASH_USER_CODE_SIZE	0x0000D000		// 52 Kbytes
#define	FLASH_TOTAL_SIZE		0x00010000		// 64 Kbytes

#define	CMD_FLASH_READ			0x03
#define	CMD_FLASH_WRITE			0x06
#define	CMD_FLASH_ERASE			0xF5			// Erase Command (size)

#define	FLASH_PAGE_SIZE			1024

#define	MAX_FW_SIZE				(52 * 1024)		// 52 Kbytes
#define	I2C_SEND_MAX_SIZE		256				// I2C Send/Receive data max size: 512->256

#define	err_status(x)			((x << 16) & 0xFFFF0000)
#define	err_mask(x)				( x        & 0xFFFF0000)

#define	STATUS_USER_MODE		0x00000000
#define	STATUS_BOOT_MODE		0x00000001
#define	STATUS_FW_CHECK			0x00000002
#define	STATUS_FW_ERASE			0x00000004
#define	STATUS_FW_WRITE			0x00000008
#define	STATUS_FW_VERIFY		0x00000010

// Error define
#define	STATUS_NO_MEMORY		0x01000000
#define	STATUS_NO_COMMAND		0x02000000

//[*]--------------------------------------------------------------------------------------------------[*]
// firmware update file name (udev rule used)
//[*]--------------------------------------------------------------------------------------------------[*]
#define FIRMWARE_NAME			"lgd_fw.bin"

//[*]--------------------------------------------------------------------------------------------------[*]
struct touch_pdata	{
	char	name[NAME_MAX];		/* input drv name */
	int 	irq_gpio;					/* irq gpio define */
	int		wake_gpio;				/* wake up gpio define */
	bool 	WakeGPIOInit;
	int 	reset_gpio;				/* reset gpio define */
	int 	reset_level;			/* reset level setting (1 = High reset, 0 = Low reset) */
	int		reset_delay;			/* mdelay value */

	int		(*gpio_init)(void);	/* gpio init function */
	int 	(*gpio_setup)(void);
	int  	(*lgd_power_save)(int);

	
	int 	irq_flags;			/* irq flags setup : Therad irq mode(IRQF_TRIGGER_HIGH | IRQF_ONESHOT) */

	int		abs_max_x, abs_max_y;
	int 	abs_min_x, abs_min_y;
	
	int 	touch_max, touch_min;
	int		width_max, width_min;
	int		id_max, id_min;
	
	int		vendor, product, version;

	int		max_fingers;
};

//[*]--------------------------------------------------------------------------------------------------[*]
#endif /* __TOUCH_PDATA_H__ */
//[*]--------------------------------------------------------------------------------------------------[*]
//[*]--------------------------------------------------------------------------------------------------[*]
