#ifndef MIPI_BOE_WXGA_PT
#define MIPI_BOE_WXGA_PT

#include <linux/time.h>

/* PWM */
/* for boe panel 5khz was the minimum freq where flickering wasnt
 * observed as the screen was dimmed
 */
#define PWM_FREQ_HZ 5000
#define PWM_LEVEL 15
#define PWM_PERIOD_USEC (USEC_PER_SEC / PWM_FREQ_HZ)
#define PWM_DUTY_LEVEL (PWM_PERIOD_USEC / PWM_LEVEL)

/********************/
/* DSI PPI Layer Registers*/
/********************/

/*PPI_STARTPPI*/
/*	[0] StartPPI 		*/
/*	0: (Default) Stop function. Writing 0 is invalid and the bit can be set to zero by system reset only.	*/
/*	1: Start function.	*/

#define PPI_STARTPPI_VALUE 0x00000001

/*PPI_TX_RX_TA*/
/*[26:16] TXTAGOCNT*/
/* (5*PPI_LPTXTIMECNT-3)/4 */
/*                             */
/*[10:0] TXTASURECNT*/
/* 1.5* PPI_LPTXTIMECNT */

#define PPI_TX_RX_TA_VALUE 0x00030005

/*PPI_LPTXTIMECNT*/
/* [10:0] PPI_LPTXTIMECNT */
/* (TLPX/(1/Byte_clk)*1000)-1 */

#define PPI_LPTXTIMECNT_VALUE 0x00000003

/*PPI_D0~3S_CLRSIPOCOUNT*/
/* [5:0] PPI_D0~3S_CLRSIPOCOUNT */
/* (THS-PREPARE+THS-ZERO/2)/(1/Byte_clk*1000) */

#define PPI_D0S_CLRSIPOCOUNT_VALUE 0x00000009
#define PPI_D1S_CLRSIPOCOUNT_VALUE 0x00000009
#define PPI_D2S_CLRSIPOCOUNT_VALUE 0x00000009
#define PPI_D3S_CLRSIPOCOUNT_VALUE 0x00000009

/************************/
/* DSI Protocol Layer Registers */
/************************/

/*PPI_STARTPPI*/
/*	[0] StartDSI 			*/
/*	0: DSI-RX is stopped (default). Writing 0 to this bit is invalid. Only system reset can set it to 0.	*/
/*	1: DSI-RX starts up.	*/

#define DSI_STARTDSI_VALUE 0x00000001

/******************/
/* Video Path Registers */
/******************/

/*VPCTRL*/
/*	[31:20] VSDELAY
	[19] VSPOL
		0: Active low
		1; Active high
	[18] DEPOL
		0: Active high
		1; Active low
	[17] HSPOL
		0: Active low
		1; Active high
	[8] OPXLFMT
		0: Selects RGB666 format for output on LVDS link. (default)
		1: Selects RGB888 format for output on LVDS link.
	[5] EVTMODE
		0: Pulse mode. Both V/HSYNC_START and V/HSYNC_END packets are required
		1: Event mode. Only V/HSYNC_START packets are required
	[4] VTGEN
		0: Disabled \u2013 LineSync mode (default)
		1: Enabled \u2013 FrameSync mode
	[0] MSF
		0: Magic Square is disabled. (default)
		1: magic Square is enabled	*/

#define VPCTRL_DSI_NON_BURST_SYNCH_EVENT_VALUE 0x00000120
#define VPCTRL_DSI_NON_BURST_SYNCH_PULSE_VALUE 0x00000100

/*HTIM1*/
/*	[24:16] HBPR
	[8:0] HPW    */

#define HTIM1_VALUE 0x00100010

/*HTIM2*/
/*	[24:16] HFPR
	[10:0]  HDISPR */

#define HTIM2_VALUE 0x00120500

/*VTIM1*/
/*	[24:16] VBPR
	[8:0] VPW    */

#define VTIM1_VALUE 0x00040004

/*VTIM2*/
/*	[24:16] VFPR
	[10:0]  VDISPR */

#define VTIM2_VALUE 0x00040320

/*VFUEN*/
/*	[0] VFUEN		*/
/*	0: No action		*/
/*	1: Upload enable	*/

#define VFUEN_VALUE 0x00000001

/*LVPHY0*/
/*
[30:29]LV_DIREN
	00: Normal operation
	b01: PLL X7 Output
	10: '0' Fix
	11: DIRIN Output
[28:24] LV_DIRIN
[22] LV_RST
	0: Normal
	1: Reset
[21] LV_PRBSEN
	0: Normal operation
	1:PRBS Pattern input
[20:16] LV_PRBS_ON
	0: Clock Channel (Input Signal select)
	1: Data Channel (PRBS signal select)
[15:14] LV_IS
	00: X0.76
	01: X1 (Default)
	10: X1.5
	11: X3
[13] LV_MEAS
	0: Normal Mode
	1: measured through the VMID
[12] LV_HIZ
	0: Normal output mode
	1: Hi-Z output
[11] LV_EREN
[10] LV_REN
	0: Normal Range
	1: Reduced Range
[9] LV_PRD
	0: Normal Mode (25Mhz \u2013 85MHz) 1/1 dividing
	1: Hi Frequency TEST mode ½ dividing
[8] LV_BP
	0: Normal operation mode (PLL clock input for p2s)
	1: Bypass mode (outer clock direct input for p2s) \u2013 PLL Power Down Mode
[6:5] LV_FS
	00: 60MHz \u2013 85MHz
	01: 30MHz \u2013 70MHz
	10: 25MHz \u2013 35MHz
	11: Reserved
[4:0] LV_ND
	01101: 85MHz ~
	00110: 60MHz \u2013 85MHz (Default)
	01101: 30Mhz \u2013 70MHz
	11011: 25MHz \u2013 35MHz
*/

#define LVPHY0_RESET_ON_VALUE		0x00448006
#define LVPHY0_RESET_OFF_VALUE	0x00048006

/*LVCFG*/
/*	[0] LVEN		*/
/*	0: LVDS transmitter is disabled. Its outputs are tri-stated. (default)*/
/*	1: LVDS transmitter is enabled.*/

#define LVCFG_VALUE 0x00000001

/*LVDS bit mapping conversion*/
/*		 -------------------------------------------*/
/*LVDS1: | IN07 | IN06 | IN04 | IN03 | IN02 | IN01 | IN00 |*/
/*		 -------------------------------------------*/
/*LVDS2: | IN18 | IN15 | IN14 | IN13 | IN12 | IN09 | IN08 |*/
/*		 -------------------------------------------*/
/*LVDS3: | IN26 | IN25 | IN24 | IN22 | IN21 | IN20 | IN19 |*/
/*		 -------------------------------------------*/
/*LVDS4: | IN23 | IN17 | IN16 | IN11 | IN10 | IN05 | IN27 |*/
/*		 -------------------------------------------*/

/*Assigned meaning 	LVMXi value
		 R0		:		0
		 R1		:		1
		 R2		:		2
		 R3		:		3
		 R4		:		4
		 R5		:		5
		 R6		:		6
		 R7		:		7
		 G0		:		8
		 G1		:		9
		 G2		:		10
		 G3		:		11
		 G4		:		12
		 G5		:		13
		 G6		:		14
		 G7		:		15
		 B0		:		16
		 B1		:		17
		 B2		:		18
		 B3		:		19
		 B4		:		20
		 B5		:		21
		 B6		:		22
		 B7		:		23
		 HYSN	:		24
		 VYSN	:		25
		 DE		:		26
		 0		:		27
 */

/*Boe panel bit mapping */
/*		 --------------------------------------------------------	*/
/*LVDS1: |	G0	|	R5	|	R4	|	R3	|	R2	|	R1	|	R0	|	*/
/*		 --------------------------------------------------------	*/
/*LVDS2: |	B1	|	B0	|	G5	|	G4	|	G3	|	G2	|	G1	|	*/
/*		 --------------------------------------------------------	*/
/*LVDS3: |	DE	|  VYSN	|  HYSN	|	B5	|	B4	|	B3	|	B2	|	*/
/*		 --------------------------------------------------------	*/
/*LVDS4: |	0	|	B7	|	B6	|	G7	|	G6	|	R7	|	R6	|	*/
/*		 --------------------------------------------------------	*/ 

#define LVMX0003_VALUE 0x03020100
#define LVMX0407_VALUE 0x08050704
#define LVMX0811_VALUE 0x0F0E0A09
#define LVMX1215_VALUE 0x100D0C0B
#define LVMX1619_VALUE 0x12111716
#define LVMX2023_VALUE 0x1B151413
#define LVMX2427_VALUE 0x061A1918

#endif /* MIPI_BOE_WXGA_PT */
