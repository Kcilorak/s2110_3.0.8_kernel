/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

/*
 * Toshiba MIPI-DSI-to-LVDS Bridge driver.
 * Device Model TC358764XBG/65XBG.
 * Reference document: TC358764XBG_65XBG_V119.pdf
 *
 * The Host sends a DSI Generic Long Write packet (Data ID = 0x29) over the
 * DSI link for each write access transaction to the chip configuration
 * registers.
 * Payload of this packet is 16-bit register address and 32-bit data.
 * Multiple data values are allowed for sequential addresses.
 *
 * The Host sends a DSI Generic Read packet (Data ID = 0x24) over the DSI
 * link for each read request transaction to the chip configuration
 * registers. Payload of this packet is further defined as follows:
 * 16-bit address followed by a 32-bit value (Generic Long Read Response
 * packet).
 *
 * The bridge supports 5 GPIO lines controlled via the GPC register.
 *
 * The bridge support I2C Master/Slave.
 * The I2C slave can be used for read/write to the bridge register instead of
 * using the DSI interface.
 * I2C slave address is 0x0F (read/write 0x1F/0x1E).
 * The I2C Master can be used for communication with the panel if
 * it has an I2C slave.
 *
 * NOTE: The I2C interface is not used in this driver.
 * Only the DSI interface is used for read/write the bridge registers.
 *
 * Pixel data can be transmitted in non-burst or burst fashion.
 * Non-burst refers to pixel data packet transmission time on DSI link
 * being roughly the same (to account for packet overhead time)
 * as active video line time on LVDS output (i.e. DE = 1).
 * And burst refers to pixel data packet transmission time on DSI link
 * being less than the active video line time on LVDS output.
 * Video mode transmission is further differentiated by the types of
 * timing events being transmitted.
 * Video pulse mode refers to the case where both sync start and sync end
 * events (for frame and line) are transmitted.
 * Video event mode refers to the case where only sync start events
 * are transmitted.
 * This is configured via register bit VPCTRL.EVTMODE.
 *
 */

//#define DEBUG 1

/**
 * Use the I2C master to control the panel.
 */
/* #define TC358764_USE_I2C_MASTER */

#define DRV_NAME "mipi_tc358764"

#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/pwm.h>
#include "msm_fb.h"
#include "mipi_dsi.h"
#include "mipi_tc358764_dsi2lvds.h"
/*+Paul, modify for various panel support*/
#ifdef CONFIG_DSI_TO_BOE_LVDS_PANEL
#include "dsi_to_boe_lvds_panel.h"
#endif
#ifdef CONFIG_DSI_TO_CHIMEI_LVDS_PANEL
#include "dsi_to_chimei_lvds_panel.h"
#endif
#ifdef CONFIG_DSI_TO_LG_LVDS_PANEL
#include "dsi_to_lg_lvds_panel.h"
#endif
/*-Paul, modify for various panel support*/
#include <linux/i2c.h>
/*+Paul, add debugfs interface*/
#include <linux/debugfs.h>
/*-Paul, add debugfs interface*/
#include <linux/gpio.h>
#include <asm/setup.h>
#include <linux/workqueue.h>
#include <asm/atomic.h>

/* Registers definition */

/* DSI D-PHY Layer Registers */
#define D0W_DPHYCONTTX	0x0004	/* Data Lane 0 DPHY Tx Control */
#define CLW_DPHYCONTRX	0x0020	/* Clock Lane DPHY Rx Control */
#define D0W_DPHYCONTRX	0x0024	/* Data Lane 0 DPHY Rx Control */
#define D1W_DPHYCONTRX	0x0028	/* Data Lane 1 DPHY Rx Control */
#define D2W_DPHYCONTRX	0x002C	/* Data Lane 2 DPHY Rx Control */
#define D3W_DPHYCONTRX	0x0030	/* Data Lane 3 DPHY Rx Control */
#define COM_DPHYCONTRX	0x0038	/* DPHY Rx Common Control */
#define CLW_CNTRL	0x0040	/* Clock Lane Control */
#define D0W_CNTRL	0x0044	/* Data Lane 0 Control */
#define D1W_CNTRL	0x0048	/* Data Lane 1 Control */
#define D2W_CNTRL	0x004C	/* Data Lane 2 Control */
#define D3W_CNTRL	0x0050	/* Data Lane 3 Control */
#define DFTMODE_CNTRL	0x0054	/* DFT Mode Control */

/* DSI PPI Layer Registers */
#define PPI_STARTPPI	0x0104	/* START control bit of PPI-TX function. */
#define PPI_BUSYPPI	0x0108
#define PPI_LINEINITCNT	0x0110	/* Line Initialization Wait Counter  */
#define PPI_LPTXTIMECNT	0x0114
#define PPI_LANEENABLE	0x0134	/* Enables each lane at the PPI layer. */
#define PPI_TX_RX_TA	0x013C	/* DSI Bus Turn Around timing parameters */

/* Analog timer function enable */
#define PPI_CLS_ATMR	0x0140	/* Delay for Clock Lane in LPRX  */
#define PPI_D0S_ATMR	0x0144	/* Delay for Data Lane 0 in LPRX */
#define PPI_D1S_ATMR	0x0148	/* Delay for Data Lane 1 in LPRX */
#define PPI_D2S_ATMR	0x014C	/* Delay for Data Lane 2 in LPRX */
#define PPI_D3S_ATMR	0x0150	/* Delay for Data Lane 3 in LPRX */
#define PPI_D0S_CLRSIPOCOUNT	0x0164

#define PPI_D1S_CLRSIPOCOUNT	0x0168	/* For lane 1 */
#define PPI_D2S_CLRSIPOCOUNT	0x016C	/* For lane 2 */
#define PPI_D3S_CLRSIPOCOUNT	0x0170	/* For lane 3 */

#define CLS_PRE		0x0180	/* Digital Counter inside of PHY IO */
#define D0S_PRE		0x0184	/* Digital Counter inside of PHY IO */
#define D1S_PRE		0x0188	/* Digital Counter inside of PHY IO */
#define D2S_PRE		0x018C	/* Digital Counter inside of PHY IO */
#define D3S_PRE		0x0190	/* Digital Counter inside of PHY IO */
#define CLS_PREP	0x01A0	/* Digital Counter inside of PHY IO */
#define D0S_PREP	0x01A4	/* Digital Counter inside of PHY IO */
#define D1S_PREP	0x01A8	/* Digital Counter inside of PHY IO */
#define D2S_PREP	0x01AC	/* Digital Counter inside of PHY IO */
#define D3S_PREP	0x01B0	/* Digital Counter inside of PHY IO */
#define CLS_ZERO	0x01C0	/* Digital Counter inside of PHY IO */
#define D0S_ZERO	0x01C4	/* Digital Counter inside of PHY IO */
#define D1S_ZERO	0x01C8	/* Digital Counter inside of PHY IO */
#define D2S_ZERO	0x01CC	/* Digital Counter inside of PHY IO */
#define D3S_ZERO	0x01D0	/* Digital Counter inside of PHY IO */

#define PPI_CLRFLG	0x01E0	/* PRE Counters has reached set values */
#define PPI_CLRSIPO	0x01E4	/* Clear SIPO values, Slave mode use only. */
#define HSTIMEOUT	0x01F0	/* HS Rx Time Out Counter */
#define HSTIMEOUTENABLE	0x01F4	/* Enable HS Rx Time Out Counter */
#define DSI_STARTDSI	0x0204	/* START control bit of DSI-TX function */
#define DSI_BUSYDSI	0x0208
#define DSI_LANEENABLE	0x0210	/* Enables each lane at the Protocol layer. */
#define DSI_LANESTATUS0	0x0214	/* Displays lane is in HS RX mode. */
#define DSI_LANESTATUS1	0x0218	/* Displays lane is in ULPS or STOP state */

#define DSI_INTSTATUS	0x0220	/* Interrupt Status */
#define DSI_INTMASK	0x0224	/* Interrupt Mask */
#define DSI_INTCLR	0x0228	/* Interrupt Clear */
#define DSI_LPTXTO	0x0230	/* Low Power Tx Time Out Counter */

#define DSIERRCNT	0x0300	/* DSI Error Count */
#define APLCTRL		0x0400	/* Application Layer Control */
#define RDPKTLN		0x0404	/* Command Read Packet Length */
#define VPCTRL		0x0450	/* Video Path Control */
#define HTIM1		0x0454	/* Horizontal Timing Control 1 */
#define HTIM2		0x0458	/* Horizontal Timing Control 2 */
#define VTIM1		0x045C	/* Vertical Timing Control 1 */
#define VTIM2		0x0460	/* Vertical Timing Control 2 */
#define VFUEN		0x0464	/* Video Frame Timing Update Enable */

/* Mux Input Select for LVDS LINK Input */
#define LVMX0003	0x0480	/* Bit 0 to 3 */
#define LVMX0407	0x0484	/* Bit 4 to 7 */
#define LVMX0811	0x0488	/* Bit 8 to 11 */
#define LVMX1215	0x048C	/* Bit 12 to 15 */
#define LVMX1619	0x0490	/* Bit 16 to 19 */
#define LVMX2023	0x0494	/* Bit 20 to 23 */
#define LVMX2427	0x0498	/* Bit 24 to 27 */

#define LVCFG		0x049C	/* LVDS Configuration  */
#define LVPHY0		0x04A0	/* LVDS PHY 0 */
#define LVPHY1		0x04A4	/* LVDS PHY 1 */
#define SYSSTAT		0x0500	/* System Status  */
#define SYSRST		0x0504	/* System Reset  */

/* GPIO Registers */
#define GPIOC		0x0520	/* GPIO Control  */
#define GPIOO		0x0524	/* GPIO Output  */
#define GPIOI		0x0528	/* GPIO Input  */

/* I2C Registers */
#define I2CTIMCTRL	0x0540	/* I2C IF Timing and Enable Control */
#define I2CMADDR	0x0544	/* I2C Master Addressing */
#define WDATAQ		0x0548	/* Write Data Queue */
#define RDATAQ		0x054C	/* Read Data Queue */

/* Chip ID and Revision ID Register */
#define IDREG		0x0580

#define TC358764XBG_ID	0x00006500

/* Debug Registers */
#define DEBUG00		0x05A0	/* Debug */
#define DEBUG01		0x05A4	/* LVDS Data */

#define CMD_DELAY 100
#define DSI_MAX_LANES 4
#define KHZ 1000
#define MHZ (1000*1000)

#define GP1_LCD_BL_EN 1
#define GP47_TP_5V_SW_EN 47

static struct i2c_client *I2cClient;

/**
 * Command payload for DTYPE_GEN_LWRITE (0x29) / DTYPE_GEN_READ2 (0x24).
 */
struct wr_cmd_payload {
	u16 addr;
	u32 data;
} __packed;

/*
 * Driver state.
 */
static struct msm_panel_common_pdata *d2l_common_pdata;
struct msm_fb_data_type *d2l_mfd;
static struct dsi_buf d2l_tx_buf;
static struct dsi_buf d2l_rx_buf;
static int led_pwm;
static struct pwm_device *bl_pwm;
static int bl_level;

/*+Paul, mark the following codes, they are not suitable for lacoste project */
#if !defined(CONFIG_LACOSTE) &&!defined( CONFIG_WESTLAKE)
static u32 d2l_gpio_out_mask;
static u32 d2l_gpio_out_val;
#endif
/*+Paul, mark the following codes, they are not suitable for lacoste project */

static struct workqueue_struct *lcd_backlight_workqueue;
static struct work_struct lcd_backlight_work;
static atomic_t lcd_backlight_level;

static int mipi_d2l_init(void);

/**
 * Read a bridge register
 *
 * @param mfd
 *
 * @return register data value
 */
//static u32 mipi_d2l_read_reg(struct msm_fb_data_type *mfd, u16 reg)
u32 mipi_d2l_read_reg(struct msm_fb_data_type *mfd, u16 reg)
{
	u32 data;
	int len = 4;
	struct dsi_cmd_desc cmd_read_reg = {
		DTYPE_GEN_READ2, 1, 0, 1, 0, /* cmd 0x24 */
			sizeof(reg), (char *) &reg};

	mipi_dsi_buf_init(&d2l_tx_buf);
	mipi_dsi_buf_init(&d2l_rx_buf);

	/* mutex had been acquried at dsi_on */
	len = mipi_dsi_cmds_rx(mfd, &d2l_tx_buf, &d2l_rx_buf,
			       &cmd_read_reg, len);

	data = *(u32 *)d2l_rx_buf.data;

	if (len != 4)
		pr_err("%s: invalid rlen=%d, expecting 4.\n", __func__, len);

	pr_debug("%s: reg=0x%x.data=0x%08x.\n", __func__, reg, data);


	return data;
}

u32 mipi_d2l_i2c_read_reg(struct msm_fb_data_type *mfd, u16 reg)
{
	#define MAX_READ_BYTE_COUNT 4
	uint8_t addr[2] = {0};
	uint8_t tmpbuf[MAX_READ_BYTE_COUNT] = {0};
	int ret = -1;
	int i =0;
	u32 data;
	struct i2c_msg msg[2];
	
	addr[0] = (reg >> 8 ) & 0xff;
	addr[1] = reg & 0xff;

	msg[0].addr = 0xf;
	msg[0].flags = 0;
	msg[0].len = 2;
	msg[0].buf = addr;
	
	msg[1].addr = 0xf;
	msg[1].flags = I2C_M_RD;
	msg[1].len = MAX_READ_BYTE_COUNT;
	msg[1].buf = tmpbuf;

	ret = i2c_transfer(I2cClient->adapter, msg, 2);
	data = *(u32 *)tmpbuf;;

	pr_info("%s: ret = %d\n reg[0x%04x] = ", __func__, ret, reg);
	for( i = MAX_READ_BYTE_COUNT ; i > 0 ; i--)
		pr_info("0x%02x", tmpbuf[i-1]);
	pr_info("\n");

	return data;
}

u32 mipi_d2l_i2c_write_reg(struct msm_fb_data_type *mfd, u16 reg, u32 data)
{

	#define MAX_WRITE_BYTE_COUNT 6
	struct i2c_msg msg;

	uint8_t tmpbuf[MAX_WRITE_BYTE_COUNT] = {0};
	int ret = -1;

	pr_info("%s: reg=0x%x.data=0x%08x.\n", __func__, reg, data);

	tmpbuf[0] = (reg >> 8 ) & 0xff;
	tmpbuf[1] = reg  & 0xff;
	tmpbuf[5] = (data  >> 24 )  & 0xff;
	tmpbuf[4] = (data  >> 16 ) & 0xff;
	tmpbuf[3] = (data  >> 8 ) & 0xff;	
	tmpbuf[2] = data  & 0xff;

	msg.addr = 0xf;
	msg.flags = 0;
	msg.len = MAX_WRITE_BYTE_COUNT;
	msg.buf = tmpbuf;

	ret = i2c_transfer(I2cClient->adapter, &msg, 1);

	if(ret !=0)
		pr_info("%s: i2c_transfer error:%d\n", __func__, ret);
	
	return 0;
}

/**
 * Write a bridge register
 *
 * @param mfd
 *
 * @return register data value
 */
static u32 mipi_d2l_write_reg(struct msm_fb_data_type *mfd, u16 reg, u32 data)
{
	struct wr_cmd_payload payload;
	struct dsi_cmd_desc cmd_write_reg = {
		DTYPE_GEN_LWRITE, 1, 0, 0, 0,
			sizeof(payload), (char *)&payload};

	payload.addr = reg;
	payload.data = data;

	/* mutex had been acquried at dsi_on */
	mipi_dsi_cmds_tx(mfd, &d2l_tx_buf, &cmd_write_reg, 1);

	pr_debug("%s: reg=0x%x. data=0x%x.\n", __func__, reg, data);

	return data;
}

/*
 * Init the D2L bridge via the DSI interface for Video.
 *
 *	Register		Addr	Value
 *  ===================================================
 *  PPI_TX_RX_TA		0x013C	0x00040004
 *  PPI_LPTXTIMECNT	        0x0114	0x00000004
 *  PPI_D0S_CLRSIPOCOUNT	0x0164	0x00000003
 *  PPI_D1S_CLRSIPOCOUNT	0x0168	0x00000003
 *  PPI_D2S_CLRSIPOCOUNT	0x016C	0x00000003
 *  PPI_D3S_CLRSIPOCOUNT	0x0170	0x00000003
 *  PPI_LANEENABLE	        0x0134	0x0000001F
 *  DSI_LANEENABLE	        0x0210	0x0000001F
 *  PPI_STARTPPI	        0x0104	0x00000001
 *  DSI_STARTDSI	        0x0204	0x00000001
 *  VPCTRL			0x0450	0x01000120
 *  HTIM1			0x0454	0x002C0028
 *  VTIM1			0x045C	0x001E0008
 *  VFUEN			0x0464	0x00000001
 *  LVCFG			0x049C	0x00000001
 *
 * VPCTRL.EVTMODE (0x20) configuration bit is needed to determine whether
 * video timing information is delivered in pulse mode or event mode.
 * In pulse mode, both Sync Start and End packets are required.
 * In event mode, only Sync Start packets are required.
 *
 * @param mfd
 *
 * @return register data value
 */

#if defined (CONFIG_LACOSTE) || defined( CONFIG_WESTLAKE)

int mipi_d2l_dsi_init_sequence(struct msm_fb_data_type *mfd)
{
	struct mipi_panel_info *mipi = &mfd->panel_info.mipi;
	u32 lanes_enable;
	u32 vpctrl;

	lanes_enable = 0x01; /* clock-lane enable */
	lanes_enable |= (mipi->data_lane0 << 1);
	lanes_enable |= (mipi->data_lane1 << 2);
	lanes_enable |= (mipi->data_lane2 << 3);
	lanes_enable |= (mipi->data_lane3 << 4);

	/*+Paul, modify for various panel support*/
	if (mipi->traffic_mode == DSI_NON_BURST_SYNCH_EVENT)
		vpctrl = 0x01000120;
	else if (mipi->traffic_mode == DSI_NON_BURST_SYNCH_PULSE)
		vpctrl = 0x01000100;
	else {
		pr_err("%s.unsupported traffic_mode %d.\n",
		       __func__, mipi->traffic_mode);
		return -EINVAL;
	}
	/*-Paul, modify for various panel support*/
	
	pr_debug("%s.htime1=0x%x.\n", __func__, HTIM1_VALUE);
	pr_debug("%s.htime2=0x%x.\n", __func__, HTIM2_VALUE);
	pr_debug("%s.vtime1=0x%x.\n", __func__, VTIM1_VALUE);
	pr_debug("%s.vtime2=0x%x.\n", __func__, VTIM2_VALUE);
	pr_debug("%s.vpctrl=0x%x.\n", __func__, vpctrl);
	pr_debug("%s.lanes_enable=0x%x.\n", __func__, lanes_enable);

	mipi_d2l_write_reg(mfd, SYSRST, 0xFF);
	msleep(30);

	mipi_d2l_write_reg(mfd, PPI_TX_RX_TA, PPI_TX_RX_TA_VALUE); /* BTA */
	mipi_d2l_write_reg(mfd, PPI_LPTXTIMECNT, PPI_LPTXTIMECNT_VALUE);
	mipi_d2l_write_reg(mfd, PPI_D0S_CLRSIPOCOUNT, PPI_D0S_CLRSIPOCOUNT_VALUE);
	mipi_d2l_write_reg(mfd, PPI_D1S_CLRSIPOCOUNT, PPI_D1S_CLRSIPOCOUNT_VALUE);
	mipi_d2l_write_reg(mfd, PPI_D2S_CLRSIPOCOUNT, PPI_D2S_CLRSIPOCOUNT_VALUE);
	mipi_d2l_write_reg(mfd, PPI_D3S_CLRSIPOCOUNT, PPI_D3S_CLRSIPOCOUNT_VALUE);
	mipi_d2l_write_reg(mfd, PPI_LANEENABLE, lanes_enable);
	mipi_d2l_write_reg(mfd, DSI_LANEENABLE, lanes_enable);
	mipi_d2l_write_reg(mfd, PPI_STARTPPI, PPI_STARTPPI_VALUE);
	mipi_d2l_write_reg(mfd, DSI_STARTDSI, DSI_STARTDSI_VALUE);

	mipi_d2l_write_reg(mfd, VPCTRL, vpctrl); /* RGB888 + Event mode */
	mipi_d2l_write_reg(mfd, HTIM1, HTIM1_VALUE);
	mipi_d2l_write_reg(mfd, HTIM2, HTIM2_VALUE);
	mipi_d2l_write_reg(mfd, VTIM1, VTIM1_VALUE);
	mipi_d2l_write_reg(mfd, VTIM2, VTIM2_VALUE);
	mipi_d2l_write_reg(mfd, VFUEN, VFUEN_VALUE);
	mipi_d2l_write_reg(mfd, LVPHY0, LVPHY0_RESET_ON_VALUE);
	usleep(150);
	mipi_d2l_write_reg(mfd, LVPHY0, LVPHY0_RESET_OFF_VALUE);	
	mipi_d2l_write_reg(mfd, LVCFG, LVCFG_VALUE); /* Enables LVDS tx */

	/* VESA format instead of JEIDA format for RGB888 */
	mipi_d2l_write_reg(mfd, LVMX0003, 0x03020100);
	mipi_d2l_write_reg(mfd, LVMX0407, 0x08050704);
	mipi_d2l_write_reg(mfd, LVMX0811, 0x0F0E0A09);
	mipi_d2l_write_reg(mfd, LVMX1215, 0x100D0C0B);
	mipi_d2l_write_reg(mfd, LVMX1619, 0x12111716);
	mipi_d2l_write_reg(mfd, LVMX2023, 0x1B151413);
	mipi_d2l_write_reg(mfd, LVMX2427, 0x061A1918);
	
	return 0;
}

#else

int mipi_d2l_dsi_init_sequence(struct msm_fb_data_type *mfd)
{
	struct mipi_panel_info *mipi = &mfd->panel_info.mipi;
	u32 lanes_enable;
	u32 vpctrl;
	u32 htime1 = 0x002C0028;
	u32 vtime1 = 0x001E0008;

	lanes_enable = 0x01; /* clock-lane enable */
	lanes_enable |= (mipi->data_lane0 << 1);
	lanes_enable |= (mipi->data_lane1 << 2);
	lanes_enable |= (mipi->data_lane2 << 3);
	lanes_enable |= (mipi->data_lane3 << 4);

	if (mipi->traffic_mode == DSI_NON_BURST_SYNCH_EVENT)
		vpctrl = 0x01000120;
	else if (mipi->traffic_mode == DSI_NON_BURST_SYNCH_PULSE)
		vpctrl = 0x01000100;
	else {
		pr_err("%s.unsupported traffic_mode %d.\n",
		       __func__, mipi->traffic_mode);
		return -EINVAL;
	}

	pr_debug("%s.htime1=0x%x.\n", __func__, htime1);
	pr_debug("%s.vtime1=0x%x.\n", __func__, vtime1);
	pr_debug("%s.vpctrl=0x%x.\n", __func__, vpctrl);
	pr_debug("%s.lanes_enable=0x%x.\n", __func__, lanes_enable);


	mipi_d2l_write_reg(mfd, SYSRST, 0xFF);
	msleep(30);

	mipi_d2l_write_reg(mfd, PPI_TX_RX_TA, 0x00040004); /* BTA */
	mipi_d2l_write_reg(mfd, PPI_LPTXTIMECNT, 0x00000004);
	mipi_d2l_write_reg(mfd, PPI_D0S_CLRSIPOCOUNT, 0x00000003);
	mipi_d2l_write_reg(mfd, PPI_D1S_CLRSIPOCOUNT, 0x00000003);
	mipi_d2l_write_reg(mfd, PPI_D2S_CLRSIPOCOUNT, 0x00000003);
	mipi_d2l_write_reg(mfd, PPI_D3S_CLRSIPOCOUNT, 0x00000003);
	mipi_d2l_write_reg(mfd, PPI_LANEENABLE, lanes_enable);
	mipi_d2l_write_reg(mfd, DSI_LANEENABLE, lanes_enable);
	mipi_d2l_write_reg(mfd, PPI_STARTPPI, 0x00000001);
	mipi_d2l_write_reg(mfd, DSI_STARTDSI, 0x00000001);

	mipi_d2l_write_reg(mfd, VPCTRL, vpctrl); /* RGB888 + Event mode */
	mipi_d2l_write_reg(mfd, HTIM1, htime1);
	mipi_d2l_write_reg(mfd, VTIM1, vtime1);
	mipi_d2l_write_reg(mfd, VFUEN, 0x00000001);
	mipi_d2l_write_reg(mfd, LVCFG, 0x00000001); /* Enables LVDS tx */

	/* VESA format instead of JEIDA format for RGB888 */

	mipi_d2l_write_reg(mfd, LVMX0003, 0x03020100);
	mipi_d2l_write_reg(mfd, LVMX0407, 0x08050704);
	mipi_d2l_write_reg(mfd, LVMX0811, 0x0F0E0A09);
	mipi_d2l_write_reg(mfd, LVMX1215, 0x100D0C0B);
	mipi_d2l_write_reg(mfd, LVMX1619, 0x12111716);
	mipi_d2l_write_reg(mfd, LVMX2023, 0x1B151413);
	mipi_d2l_write_reg(mfd, LVMX2427, 0x061A1918);

	return 0;
}

#endif

/**
 * Set Backlight level.
 *
 * @param pwm
 * @param level
 *
 * @return int
 */
static int mipi_d2l_set_backlight_level(struct pwm_device *pwm, int level)
{
	int ret = 0;

	pr_debug("%s: level=%d.\n", __func__, level);

	if (level == 0) {
		ret = gpio_direction_output(GP47_TP_5V_SW_EN, 0);
		if (ret) {
			gpio_free(GP47_TP_5V_SW_EN);
			pr_err("%s: unable to set_direction for backlight_power_enable gpio [%d]\n",
				   __func__, GP47_TP_5V_SW_EN);
			return -ENODEV;
		}
		gpio_free(GP47_TP_5V_SW_EN);
		gpio_direction_output(GP1_LCD_BL_EN, 0);
		gpio_free(GP1_LCD_BL_EN);
		msleep(10);
	}

	if ((pwm == NULL) || (level > PWM_LEVEL) || (level < 0)) {
		pr_err("%s.pwm=NULL.\n", __func__);
		return -EINVAL;
	}

	ret = pwm_config(pwm, PWM_DUTY_LEVEL * level, PWM_PERIOD_USEC);
	if (ret) {
		pr_err("%s: pwm_config() failed err=%d.\n", __func__, ret);
		return ret;
	}

	ret = pwm_enable(pwm);
	if (ret) {
		pr_err("%s: pwm_enable() failed err=%d\n",
		       __func__, ret);
		return ret;
	}

	if (level > 0 && bl_level == 0) {
		ret = gpio_request(GP47_TP_5V_SW_EN, "level_shifter_enable");
		if (ret) {
			pr_err("request gpio 1 failed, rc=%d\n", ret);
			return -ENODEV;
		}

		ret = gpio_direction_output(GP47_TP_5V_SW_EN, 1);
		if (ret) {
			gpio_free(GP47_TP_5V_SW_EN);
			pr_err("%s: unable to set_direction for backlight_power_enable gpio [%d]\n",
				   __func__, GP47_TP_5V_SW_EN);
			return -ENODEV;
		}
		ret = gpio_request(GP1_LCD_BL_EN, "Backlight_EN");
		if (ret < 0) {
			printk(KERN_ERR "%s: GPIO %d request failed\n", __func__, GP1_LCD_BL_EN);
			return ret;
		}
		gpio_direction_output(GP1_LCD_BL_EN, 1);
	}

	bl_level = level;

	return 0;
}

/**
 * LCD ON.
 *
 * Set LCD On via MIPI interface or I2C-Slave interface.
 * Set Backlight on.
 *
 * @param pdev
 *
 * @return int
 */
static int mipi_d2l_lcd_on(struct platform_device *pdev)
{
	int ret = 0;

	#if !defined (CONFIG_LACOSTE) &&!defined( CONFIG_WESTLAKE)

	u32 chip_id;

	#endif

	struct msm_fb_data_type *mfd;

	pr_debug("%s.\n", __func__);

	/* wait for valid clock before sending data over DSI or I2C. */
	msleep(30);

	mfd = platform_get_drvdata(pdev);
	d2l_mfd = mfd;

	if (!mfd)
		return -ENODEV;
	if (mfd->key != MFD_KEY)
		return -EINVAL;

	#if !defined (CONFIG_LACOSTE) &&!defined( CONFIG_WESTLAKE)

	chip_id = mipi_d2l_read_reg(mfd, IDREG);
	pr_debug("%s, mipi_d2l_read_reg, chip_id = 0x%x\n", __func__, chip_id);

	if (chip_id != TC358764XBG_ID) {
		pr_err("%s: invalid chip_id=0x%x", __func__, chip_id);
		return -ENODEV;
	}

	#endif

	enable_panel_power(1);

	ret = mipi_d2l_dsi_init_sequence(mfd);

	if (ret)
		return ret;

	/*+Paul, mark the following codes, they are not suitable for lacoste project */
	#if !defined (CONFIG_LACOSTE) &&!defined( CONFIG_WESTLAKE)
	mipi_d2l_write_reg(mfd, GPIOC, d2l_gpio_out_mask);
	/* Set GPIOs: gpio#4=U/D=0 , gpio#3=L/R=1 , gpio#2,1=CABC=0. */
	mipi_d2l_write_reg(mfd, GPIOO, d2l_gpio_out_val);
	#endif
	/*-Paul, mark the following codes, they are not suitable for lacoste project */

	return ret;
}

/**
 * LCD OFF.
 *
 * @param pdev
 *
 * @return int
 */
static int mipi_d2l_lcd_off(struct platform_device *pdev)
{
	int rc = 0;
	struct msm_fb_data_type *mfd;

	pr_debug("%s.\n", __func__);

	mfd = platform_get_drvdata(pdev);
	msleep(200);

	if (!mfd)
		return -ENODEV;
	if (mfd->key != MFD_KEY)
		return -EINVAL;

	return rc;
}

static void lcd_backlight_work_func(struct work_struct *work)
{
	int level;

	level = atomic_read(&lcd_backlight_level);

	if (level > 0 && bl_level == 0)
		msleep(200);

	mipi_d2l_set_backlight_level(bl_pwm, level);
}

static void mipi_d2l_set_backlight(struct msm_fb_data_type *mfd)
{
	int level = mfd->bl_level;

	pr_debug("%s.lvl=%d.\n", __func__, level);

	atomic_set(&lcd_backlight_level, level);
	queue_work(lcd_backlight_workqueue, &lcd_backlight_work);
}

static struct msm_fb_panel_data d2l_panel_data = {
	.on = mipi_d2l_lcd_on,
	.off = mipi_d2l_lcd_off,
	.set_backlight = mipi_d2l_set_backlight,
};

/**
 * Probe for device.
 *
 * Both the "target" and "panel" device use the same probe function.
 * "Target" device has id=0, "Panel" devic has non-zero id.
 * Target device should register first, passing msm_panel_common_pdata.
 * Panel device passing msm_panel_info.
 *
 * @param pdev
 *
 * @return int
 */
static int __devinit mipi_d2l_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct msm_panel_info *pinfo = NULL;

	pr_debug("%s.id=%d.\n", __func__, pdev->id);

	if (pdev->id == 0) {
		/* d2l_common_pdata = platform_get_drvdata(pdev); */
		d2l_common_pdata = pdev->dev.platform_data;

		if (d2l_common_pdata == NULL) {
			pr_err("%s: no PWM gpio specified.\n", __func__);
			return 0;
		}

		led_pwm = d2l_common_pdata->gpio_num[0];

		/*+Paul, mark the following codes, they are not suitable for lacoste project */
		#if !defined (CONFIG_LACOSTE) && !defined(CONFIG_WESTLAKE)
		d2l_gpio_out_mask = d2l_common_pdata->gpio_num[1] >> 8;
		d2l_gpio_out_val = d2l_common_pdata->gpio_num[1] & 0xFF;
		#endif
		/*+Paul, mark the following codes, they are not suitable for lacoste project */

		mipi_dsi_buf_alloc(&d2l_tx_buf, DSI_BUF_SIZE);
		mipi_dsi_buf_alloc(&d2l_rx_buf, DSI_BUF_SIZE);

		return 0;
	}

	if (d2l_common_pdata == NULL) {
		pr_err("%s: d2l_common_pdata is NULL.\n", __func__);
		return -ENODEV;
	}

	bl_pwm = NULL;
	if (led_pwm >= 0) {
		bl_pwm = pwm_request(led_pwm, "lcd-backlight");
		if (bl_pwm == NULL || IS_ERR(bl_pwm)) {
			pr_err("%s pwm_request() failed.id=%d.bl_pwm=%d.\n",
			       __func__, led_pwm, (int) bl_pwm);
			bl_pwm = NULL;
			return -EIO;
		} else {
			pr_debug("%s.pwm_request() ok.pwm-id=%d.\n",
			       __func__, led_pwm);

		}
	} else {
		pr_debug("%s. led_pwm is invalid.\n", __func__);
	}

	/* pinfo = platform_get_drvdata(pdev); */
	pinfo = pdev->dev.platform_data;

	if (pinfo == NULL) {
		pr_err("%s: pinfo is NULL.\n", __func__);
		return -ENODEV;
	}

	d2l_panel_data.panel_info = *pinfo;

	lcd_backlight_workqueue = create_singlethread_workqueue("lcd_backlight_queue");
	INIT_WORK(&lcd_backlight_work, lcd_backlight_work_func);

	pdev->dev.platform_data = &d2l_panel_data;

	msm_fb_add_device(pdev);

	return ret;
}

/**
 * Device removal notification handler.
 *
 * @param pdev
 *
 * @return int
 */
static int __devexit mipi_d2l_remove(struct platform_device *pdev)
{
	/* Note: There are no APIs to remove fb device and free DSI buf. */
	pr_debug("%s.\n", __func__);

	if (bl_pwm) {
		pwm_free(bl_pwm);
		bl_pwm = NULL;
	}

	#if defined (CONFIG_LACOSTE) || defined( CONFIG_WESTLAKE)

	gpio_free(15);

	#endif

	return 0;
}

/**
 * Register the panel device.
 *
 * @param pinfo
 * @param channel_id
 * @param panel_id
 *
 * @return int
 */
int mipi_tc358764_dsi2lvds_register(struct msm_panel_info *pinfo,
					   u32 channel_id, u32 panel_id)
{
	struct platform_device *pdev = NULL;
	int ret;
	/* Use DSI-to-LVDS bridge */
	const char driver_name[] = "mipi_tc358764";

	pr_debug("%s.\n", __func__);
	ret = mipi_d2l_init();
	if (ret) {
		pr_err("mipi_d2l_init() failed with ret %u\n", ret);
		return ret;
	}

	/* Note: the device id should be non-zero */
	pdev = platform_device_alloc(driver_name, (panel_id << 8)|channel_id);
	if (pdev == NULL)
		return -ENOMEM;

	pdev->dev.platform_data = pinfo;

	ret = platform_device_add(pdev);
	if (ret) {
		pr_err("%s: platform_device_register failed!\n", __func__);
		goto err_device_put;
	}

	return 0;

err_device_put:
	platform_device_put(pdev);
	return ret;
}

static struct platform_driver d2l_driver = {
	.probe  = mipi_d2l_probe,
	.remove = __devexit_p(mipi_d2l_remove),
	.driver = {
		.name   = DRV_NAME,
	},
};

/***********************************
 *          i2c interface          *
 ***********************************/

#define BRIDGE_IC_ADDRESS  0xf

static int bridge_i2c_prob(struct i2c_client *client, const struct i2c_device_id *id)
{
    printk(KERN_INFO "bridge_i2c_prob \n");

    if (client->addr !=  BRIDGE_IC_ADDRESS) {
        return -1; // Error
    }

    printk(KERN_INFO "bridge_i2c_prob ok \n");

    I2cClient = client;  /* Save a pointer to the i2c client struct we need it during the interrupt */
    return 0;
}

static int bridge_i2c_remove(struct i2c_client *client)
{

    printk(KERN_INFO "bridge_i2c_remove\n");
    kfree(i2c_get_clientdata(client));
    return 0;
}

static const unsigned short normal_i2c[] = {BRIDGE_IC_ADDRESS, I2C_CLIENT_END};

static const struct i2c_device_id bridge_i2c_id[] = {
	{ "bridge_i2c", 0 },
	{ }
};

static struct i2c_driver chip_driver = {
	.driver = {
		.name	= "bridge_i2c",
	},
	.probe  	= bridge_i2c_prob,
	.remove 	= bridge_i2c_remove,
	.id_table	= bridge_i2c_id,
	.class		= I2C_CLASS_HWMON,
	.address_list	= normal_i2c,
};

/***********************************
 *        debugfs interface        *
 ***********************************/

/*+Paul, 20111110, add debugfs interface*/
static struct dentry *d2l_debugfs_dir;

#define MDP_DEBUG_BUF	2048

static char	debug_buf[MDP_DEBUG_BUF];

static int d2l_reg_open(struct inode *inode, struct file *file)
{
	/* non-seekable */
	file->f_mode &= ~(FMODE_LSEEK | FMODE_PREAD | FMODE_PWRITE);
	return 0;
}

static int d2l_reg_release(struct inode *inode, struct file *file)
{
	return 0;
}

static ssize_t d2l_reg_write(
	struct file *file,
	const char __user *buff,
	size_t count,
	loff_t *ppos)
{
	uint32 rw, addr, data;
	u16 u16_addr;
	int cnt;

	pr_debug("%s, count = %d, buff = %s\n", __func__, count, buff);

	if (count >= sizeof(debug_buf))
		return -EFAULT;

	if (copy_from_user(debug_buf, buff, count))
		return -EFAULT;

	debug_buf[count] = 0;	/* end of string */

	/*0: i2c write data
	   1: i2c read data */

	if(debug_buf[0] == '0')
	{
		cnt = sscanf(debug_buf, "%d 0x%x 0x%x", &rw, &addr, &data);
		u16_addr =  (u16) (addr & 0xffff);
		mipi_d2l_i2c_write_reg(NULL, u16_addr, data);
		pr_info("i2c write: addr=0x%04x data=0x%08x\n", addr, data);

	}
	else
	{
		cnt = sscanf(debug_buf, "%d 0x%x", &rw, &addr);
		u16_addr =  (u16) (addr & 0xffff);
		data = mipi_d2l_i2c_read_reg(NULL, u16_addr);
		pr_info("i2c read: addr=0x%04x data=0x%08x\n", addr, data);
	}

	return count;
}

static const struct file_operations d2l_reg_fops = {
	.open = d2l_reg_open,
	.release = d2l_reg_release,
	.write = d2l_reg_write,
};
/*-Paul, 20111110, add debugfs interface*/

/**
 * Module Init
 *
 * @return int
 */
static int mipi_d2l_init(void)
{
	pr_debug("%s.\n", __func__);


	i2c_add_driver(&chip_driver);

	/*+Paul, 20111110, add debugfs interface*/
	d2l_debugfs_dir = debugfs_create_dir("d2l", NULL);
	if (IS_ERR(d2l_debugfs_dir)) {
		int err = PTR_ERR(d2l_debugfs_dir);
		d2l_debugfs_dir = NULL;
		return err;
	}

	debugfs_create_file("reg", S_IRUGO, d2l_debugfs_dir, 0, &d2l_reg_fops);
	/*-Paul, 20111110, add debugfs interface*/

	return platform_driver_register(&d2l_driver);
}

/**
 * Module Exit.
 *
 */
static void __exit mipi_d2l_exit(void)
{
	pr_debug("%s.\n", __func__);

	i2c_del_driver(&chip_driver);
	platform_driver_unregister(&d2l_driver);
}

module_exit(mipi_d2l_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Toshiba MIPI-DSI-to-LVDS bridge driver");
MODULE_AUTHOR("Amir Samuelov <amirs@codeaurora.org>");
