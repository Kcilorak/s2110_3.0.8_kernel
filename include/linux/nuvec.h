#ifndef NUVEC_H
#define NUVEC_H

#define	EC_State_S0		0x00
#define	EC_State_S3		0x03
#define	EC_State_S4		0x04
#define MAX_ERR_TIME	10
#define Dock_Reset_Pin	34

#define 	DKIN_DET(det_irq) (!gpio_get_value(det_irq))
/*------------------------------------------
	Westlake Dock EC Log
    -----------------------------------------*/
#define	ECINFO	1
#define	ECDBG	2
static int ecloglv = ECINFO;
#define ECLOG_ERR(x...) printk(KERN_ERR"[EC]"x)
#define ECLOG_INFO(x...) if(ecloglv >= ECINFO)\
							printk(KERN_INFO"[EC]"x)
#define ECLOG_DBG(x...) if(ecloglv >= ECDBG)\
							printk(KERN_DEBUG"[EC]"x)

#define EC_EI2CXF_MSG	ECLOG_ERR("(%s)EI2CXF\n",__func__)
#define EC_ERR_MSG(x...)               printk("[EC]%s:BUS_DRIVER_NOT_READY(%d)\n",__func__,x)

static int ec_init_delay = 650;

struct EC_platform_data {
	unsigned int	EC_irq;
	unsigned int	docking_det_irq;
	int (*gpio_setup)(int , char*);
	irqreturn_t (*docking_interrupt)(int irq, void * dev_id);
};
void ec_dock_in(void);
void ec_dock_out(void);
void dock_i2c_fail(void);

#endif
