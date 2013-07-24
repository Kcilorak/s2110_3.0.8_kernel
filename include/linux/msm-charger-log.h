#ifndef __MSM_CHARGER_LOG_H__
#define __MSM_CHARGER_LOG_H__

#ifdef CONFIG_PEGA_ISL_CHG

#ifdef CONFIG_PEGA_CHG_DBG

#ifdef PEGA_SMT_BUILD
#define PEGA_CHG_DBG_LEVEL	2   // for SMT release
#else
#define PEGA_CHG_DBG_LEVEL	4   // for formal release
#endif

#if (PEGA_CHG_DBG_LEVEL == 4)
static int pega_chg_debug_level = 1;
#endif /* PEGA_CHG_DBG_LEVEL */

#if (PEGA_CHG_DBG_LEVEL == 1)
	#define PEGA_DBG_H(x...)	printk(x)
	#define PEGA_DBG_M(x...)	do { } while (0)
	#define PEGA_DBG_L(x...)	do { } while (0)
#elif (PEGA_CHG_DBG_LEVEL == 2)
	#define PEGA_DBG_H(x...)	printk(x)
	#define PEGA_DBG_M(x...)	printk(x)
	#define PEGA_DBG_L(x...)	do { } while (0)
#elif (PEGA_CHG_DBG_LEVEL == 3)
	#define PEGA_DBG_H(x...)	printk(x)
	#define PEGA_DBG_M(x...)	printk(x)
	#define PEGA_DBG_L(x...)	printk(x)
#elif (PEGA_CHG_DBG_LEVEL == 4)
	#define PEGA_DBG_H(x...)	{if (pega_chg_debug_level>0) printk(x);}
	#define PEGA_DBG_M(x...)	{if (pega_chg_debug_level>1) printk(x);}
	#define PEGA_DBG_L(x...)	{if (pega_chg_debug_level>2) printk(x);}
#else  /* PEGA_CHG_DBG_LEVEL */
	#define PEGA_DBG_H(x...)	do { } while (0)
	#define PEGA_DBG_M(x...)	do { } while (0)
	#define PEGA_DBG_L(x...)	do { } while (0)
#endif /* PEGA_CHG_DBG_LEVEL */

#else  /* CONFIG_PEGA_CHG_DBG */
	#define PEGA_DBG_H(x...)	do { } while (0)
	#define PEGA_DBG_M(x...)	do { } while (0)
	#define PEGA_DBG_L(x...)	do { } while (0)
#endif /* CONFIG_PEGA_CHG_DBG */

#endif /* CONFIG_PEGA_ISL_CHG */

#endif /* __MSM_CHARGER_LOG_H__ */
