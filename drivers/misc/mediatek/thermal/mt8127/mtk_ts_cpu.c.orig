/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/dmi.h>
#include <linux/acpi.h>
#include <linux/thermal.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <linux/spinlock.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/reboot.h>
#include <linux/nvmem-consumer.h>
#include "mtk_thermal_typedefs.h"
#include <mt_cpufreq.h>
#include <mt-plat/sync_write.h>
#include "mt-plat/mtk_thermal_monitor.h"
#include "mach/mtk_thermal.h"
#include <linux/time.h>

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#endif

#define __MT_MTK_TS_CPU_C__

#ifdef CONFIG_OF
u32 thermal_irq_number;
void __iomem *thermal_base;
void __iomem *auxadc_ts_base;
void __iomem *apmixed_ts_base;
void __iomem *pericfg_base;

int thermal_phy_base;
int auxadc_ts_phy_base;
int apmixed_phy_base;
int pericfg_phy_base;

struct clk *clk_peri_therm;
struct clk *clk_auxadc;
#endif

#define THERMAL_DEVICE_NAME_2701	"mediatek,mt2701-thermal"

/* 1: turn on adaptive AP cooler; 0: turn off */
#define CPT_ADAPTIVE_AP_COOLER (0)

/* 1: turn on supports to MET logging; 0: turn off */
#define CONFIG_SUPPORT_MET_MTKTSCPU (0)

#define THERMAL_CONTROLLER_HW_FILTER (1) /* 1, 2, 4, 8, 16 */

/* 1: turn on thermal controller HW thermal protection; 0: turn off */
#define THERMAL_CONTROLLER_HW_TP     (1)

/* 1: turn on SW filtering in this sw module; 0: turn off */
#define MTK_TS_CPU_SW_FILTER         (0)

/* 1: thermal driver fast polling, use hrtimer; 0: turn off */
#define THERMAL_DRV_FAST_POLL_HRTIMER          (0)

/* 1: thermal driver update temp to MET directly, use hrtimer; 0: turn off */
#define THERMAL_DRV_UPDATE_TEMP_DIRECT_TO_MET  (0)


#define MIN(_a_, _b_) ((_a_) > (_b_) ? (_b_) : (_a_))
#define MAX(_a_, _b_) ((_a_) > (_b_) ? (_a_) : (_b_))

/*==============*/
/*Variables*/
/*==============*/

#define thermal_readl(addr)		readl(addr)
#define thermal_writel(addr, val)	writel((val), ((void *)addr))
#define thermal_setl(addr, val)		writel(thermal_readl(addr) | (val), ((void *)addr))
#define thermal_clrl(addr, val)		writel(thermal_readl(addr) & ~(val), ((void *)addr))
#define THERMAL_WRAP_WR32(val, addr)	writel((val), ((void *)addr))

static unsigned int interval = 1000;	/* mseconds, 0 : no auto polling */
//static int last_cpu_real_temp;
/* trip_temp[0] must be initialized to the thermal HW protection point. */
static int trip_temp[10] = {
	117000, 100000, 85000, 76000, 70000, 68000, 45000, 35000, 25000, 15000
};

/* atic int gtemp_hot=80000, gtemp_normal=70000, gtemp_low=50000,goffset=5000; */

static struct thermal_zone_device *thz_dev;

static U32 calefuse1;
static U32 calefuse2;
static U32 calefuse3;

static int kernelmode;
static int g_THERMAL_TRIP[10] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

/* atic int RAW_TS2=0, RAW_TS3=0, RAW_TS4=0; */

static int num_trip = 5;
int MA_len_temp;
static int proc_write_flag;

static DEFINE_MUTEX(TS_lock);

static char g_bind0[20] = "mtktscpu-sysrst";
static char g_bind1[20] = "mtk-cl-kshutdown00";
static char g_bind2[20] = "1200";
static char g_bind3[20] = "1400";
static char g_bind4[20] = "1600";
static char g_bind5[20] = "1800";
static char g_bind6[20] = "";
static char g_bind7[20] = "";
static char g_bind8[20] = "";
static char g_bind9[20] = "";

static int read_curr_temp;

#define MTKTSCPU_TEMP_CRIT 120000	/* 120.000 degree Celsius */

static int tc_mid_trip = -275000;

static S32 temperature_to_raw_abb(U32 ret);
/* static int last_cpu_t=0; */
int last_abb_t;
int last_CPU1_t;
int last_CPU2_t;

static int g_tc_resume;		/* default=0,read temp */

static S32 g_adc_ge_t;
static S32 g_adc_oe_t;
static S32 g_o_vtsmcu1;
static S32 g_o_vtsmcu2;
static S32 g_o_vtsmcu3;
static S32 g_o_vtsmcu4;
static S32 g_o_vtsabb;
static S32 g_degc_cali;
static S32 g_adc_cali_en_t;
static S32 g_o_slope;
static S32 g_o_slope_sign;
static S32 g_id;

static S32 g_ge;
static S32 g_oe;
static S32 g_gain;

static S32 g_x_roomt1;
static S32 g_x_roomt2;
static S32 g_x_roomtabb;

#define y_curr_repeat_times 1
#define THERMAL_NAME    "mtk-thermal"
/* #define GPU_Default_POWER     456 */

#define		FIX_ME_IOMAP

int mtktscpu_limited_dmips;

static bool talking_flag;
static int thermal_fast_init(void);

void __attribute__ ((weak)) mt_cpufreq_thermal_protect(unsigned int limited_power)
{
	pr_err("%s is not ready\n", __func__);
}

void set_taklking_flag(bool flag)
{
	talking_flag = flag;
	pr_debug("talking_flag=%d\n", talking_flag);
}

void tscpu_thermal_clock_on(void)
{
	pr_debug("tscpu_thermal_clock_on\n");
	clk_prepare(clk_auxadc);
	clk_enable(clk_auxadc);
	clk_prepare(clk_peri_therm);
	clk_enable(clk_peri_therm);
}

void tscpu_thermal_clock_off(void)
{
	pr_debug("tscpu_thermal_clock_off\n");
	clk_disable(clk_peri_therm);
	clk_unprepare(clk_peri_therm);
	clk_disable(clk_auxadc);
	clk_unprepare(clk_auxadc);
}

void get_thermal_all_register(void)
{
	pr_debug("get_thermal_all_register\n");
	pr_debug("TEMPMSR1            = 0x%8x\n", DRV_Reg32(TEMPMSR1));
	pr_debug("TEMPMSR2            = 0x%8x\n", DRV_Reg32(TEMPMSR2));
	pr_debug("TEMPMONCTL0         = 0x%8x\n", DRV_Reg32(TEMPMONCTL0));
	pr_debug("TEMPMONCTL1         = 0x%8x\n", DRV_Reg32(TEMPMONCTL1));
	pr_debug("TEMPMONCTL2         = 0x%8x\n", DRV_Reg32(TEMPMONCTL2));
	pr_debug("TEMPMONINT          = 0x%8x\n", DRV_Reg32(TEMPMONINT));
	pr_debug("TEMPMONINTSTS       = 0x%8x\n", DRV_Reg32(TEMPMONINTSTS));
	pr_debug("TEMPMONIDET0        = 0x%8x\n", DRV_Reg32(TEMPMONIDET0));

	pr_debug("TEMPMONIDET1        = 0x%8x\n", DRV_Reg32(TEMPMONIDET1));
	pr_debug("TEMPMONIDET2        = 0x%8x\n", DRV_Reg32(TEMPMONIDET2));
	pr_debug("TEMPH2NTHRE         = 0x%8x\n", DRV_Reg32(TEMPH2NTHRE));
	pr_debug("TEMPHTHRE           = 0x%8x\n", DRV_Reg32(TEMPHTHRE));
	pr_debug("TEMPCTHRE           = 0x%8x\n", DRV_Reg32(TEMPCTHRE));
	pr_debug("TEMPOFFSETH         = 0x%8x\n", DRV_Reg32(TEMPOFFSETH));

	pr_debug("TEMPOFFSETL         = 0x%8x\n", DRV_Reg32(TEMPOFFSETL));
	pr_debug("TEMPMSRCTL0         = 0x%8x\n", DRV_Reg32(TEMPMSRCTL0));
	pr_debug("TEMPMSRCTL1         = 0x%8x\n", DRV_Reg32(TEMPMSRCTL1));
	pr_debug("TEMPAHBPOLL         = 0x%8x\n", DRV_Reg32(TEMPAHBPOLL));
	pr_debug("TEMPAHBTO           = 0x%8x\n", DRV_Reg32(TEMPAHBTO));
	pr_debug("TEMPADCPNP0         = 0x%8x\n", DRV_Reg32(TEMPADCPNP0));

	pr_debug("TEMPADCPNP1         = 0x%8x\n", DRV_Reg32(TEMPADCPNP1));
	pr_debug("TEMPADCPNP2         = 0x%8x\n", DRV_Reg32(TEMPADCPNP2));
	pr_debug("TEMPADCMUX          = 0x%8x\n", DRV_Reg32(TEMPADCMUX));
	pr_debug("TEMPADCEXT          = 0x%8x\n", DRV_Reg32(TEMPADCEXT));
	pr_debug("TEMPADCEXT1         = 0x%8x\n", DRV_Reg32(TEMPADCEXT1));
	pr_debug("TEMPADCEN           = 0x%8x\n", DRV_Reg32(TEMPADCEN));

	pr_debug("TEMPPNPMUXADDR      = 0x%8x\n", DRV_Reg32(TEMPPNPMUXADDR));
	pr_debug("TEMPADCMUXADDR      = 0x%8x\n", DRV_Reg32(TEMPADCMUXADDR));
	pr_debug("TEMPADCEXTADDR      = 0x%8x\n", DRV_Reg32(TEMPADCEXTADDR));
	pr_debug("TEMPADCEXT1ADDR     = 0x%8x\n", DRV_Reg32(TEMPADCEXT1ADDR));
	pr_debug("TEMPADCENADDR       = 0x%8x\n", DRV_Reg32(TEMPADCENADDR));
	pr_debug("TEMPADCVALIDADDR    = 0x%8x\n", DRV_Reg32(TEMPADCVALIDADDR));

	pr_debug("TEMPADCVOLTADDR     = 0x%8x\n", DRV_Reg32(TEMPADCVOLTADDR));
	pr_debug("TEMPRDCTRL          = 0x%8x\n", DRV_Reg32(TEMPRDCTRL));
	pr_debug("TEMPADCVALIDMASK    = 0x%8x\n", DRV_Reg32(TEMPADCVALIDMASK));
	pr_debug("TEMPADCVOLTAGESHIFT = 0x%8x\n", DRV_Reg32(TEMPADCVOLTAGESHIFT));
	pr_debug("TEMPADCWRITECTRL    = 0x%8x\n", DRV_Reg32(TEMPADCWRITECTRL));
	pr_debug("TEMPMSR0            = 0x%8x\n", DRV_Reg32(TEMPMSR0));


	pr_debug("TEMPIMMD0           = 0x%8x\n", DRV_Reg32(TEMPIMMD0));
	pr_debug("TEMPIMMD1           = 0x%8x\n", DRV_Reg32(TEMPIMMD1));
	pr_debug("TEMPIMMD2           = 0x%8x\n", DRV_Reg32(TEMPIMMD2));
	pr_debug("TEMPPROTCTL         = 0x%8x\n", DRV_Reg32(TEMPPROTCTL));

	pr_debug("TEMPPROTTA          = 0x%8x\n", DRV_Reg32(TEMPPROTTA));
	pr_debug("TEMPPROTTB          = 0x%8x\n", DRV_Reg32(TEMPPROTTB));
	pr_debug("TEMPPROTTC          = 0x%8x\n", DRV_Reg32(TEMPPROTTC));
	pr_debug("TEMPSPARE0          = 0x%8x\n", DRV_Reg32(TEMPSPARE0));
	pr_debug("TEMPSPARE1          = 0x%8x\n", DRV_Reg32(TEMPSPARE1));
	pr_debug("TEMPSPARE2          = 0x%8x\n", DRV_Reg32(TEMPSPARE2));
	pr_debug("TEMPSPARE3          = 0x%8x\n", DRV_Reg32(TEMPSPARE3));
	/* pr_debug("0x11001040          = 0x%8x\n", DRV_Reg32(0xF1001040)); */
}

void get_thermal_slope_intercept(struct TS_PTPOD *ts_info)
{
	unsigned int temp0, temp1, temp2;
	struct TS_PTPOD ts_ptpod;

	pr_debug("get_thermal_slope_intercept\n");

	/* temp0 = (10000*100000/4096/g_gain)*15/18; */
	temp0 = (10000 * 100000 / g_gain) * 15 / 18;
	/* pr_debug("temp0=%d\n", temp0); */
	if (g_o_slope_sign == 0)
		temp1 = temp0 / (165 + g_o_slope);
	else
		temp1 = temp0 / (165 - g_o_slope);

	/* pr_debug("temp1=%d\n", temp1); */
	/* ts_ptpod.ts_MTS = temp1 - (2*temp1) + 2048; */
	ts_ptpod.ts_MTS = temp1;

	temp0 = (g_degc_cali * 10 / 2);
	temp1 = ((10000 * 100000 / 4096 / g_gain) * g_oe + g_x_roomtabb * 10) * 15 / 18;
	/* pr_debug("temp1=%d\n", temp1); */
	if (g_o_slope_sign == 0)
		temp2 = temp1 * 10 / (165 + g_o_slope);
	else
		temp2 = temp1 * 10 / (165 - g_o_slope);

	/* pr_debug("temp2=%d\n", temp2); */
	ts_ptpod.ts_BTS = (temp0 + temp2 - 250) * 4 / 10;

	/* ts_info = &ts_ptpod; */
	ts_info->ts_MTS = ts_ptpod.ts_MTS;
	ts_info->ts_BTS = ts_ptpod.ts_BTS;
	pr_debug("ts_MTS=%d, ts_BTS=%d\n", ts_ptpod.ts_MTS, ts_ptpod.ts_BTS);
}
EXPORT_SYMBOL(get_thermal_slope_intercept);

static irqreturn_t thermal_interrupt_handler(int irq, void *dev_id)
{
	U32 ret = 0;

	ret = DRV_Reg32(TEMPMONINTSTS);
	/* pr_debug("XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX\n"); */
	pr_debug("thermal_interrupt_handler,ret=0x%08x\n", ret);
	/* pr_debug("XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX\n"); */

	/* ret2 = DRV_Reg32(THERMINTST); */
	/* pr_debug("thermal_interrupt_handler : THERMINTST = 0x%x\n", ret2); */


	/* for SPM reset debug */
	/* dump_spm_reg(); */

	/* pr_debug("thermal_isr: [Interrupt trigger]: status = 0x%x\n", ret); */
	if (ret & THERMAL_MON_CINTSTS0)
		pr_emerg("thermal_isr: thermal sensor point 0 - cold interrupt trigger\n");

	if (ret & THERMAL_MON_HINTSTS0)
		pr_emerg("<<<thermal_isr>>>: thermal sensor point 0 - hot interrupt trigger\n");

	if (ret & THERMAL_MON_HINTSTS1)
		pr_emerg("<<<thermal_isr>>>: thermal sensor point 1 - hot interrupt trigger\n");

	if (ret & THERMAL_MON_HINTSTS2)
		pr_emerg("<<<thermal_isr>>>: thermal sensor point 2 - hot interrupt trigger\n");

	if (ret & THERMAL_tri_SPM_State0)
		pr_emerg("thermal_isr: Thermal state0 to trigger SPM state0\n");

	if (ret & THERMAL_tri_SPM_State1)
		pr_debug("thermal_isr: Thermal state1 to trigger SPM state1\n");

	if (ret & THERMAL_tri_SPM_State2)
		pr_emerg("thermal_isr: Thermal state2 to trigger SPM state2\n");

	return IRQ_HANDLED;
}

static void thermal_reset_and_initial(void)
{
	UINT32 temp;
	int cnt = 0;
	int temp2;

	pr_debug("[Reset and init thermal controller]\n");

	tscpu_thermal_clock_on();

	temp = DRV_Reg32(PERI_GLOBALCON_RST0);
	temp |= 0x00010000;
	THERMAL_WRAP_WR32(temp, PERI_GLOBALCON_RST0);

	temp = DRV_Reg32(PERI_GLOBALCON_RST0);
	temp &= 0xFFFEFFFF;
	THERMAL_WRAP_WR32(temp, PERI_GLOBALCON_RST0);

	thermal_fast_init();
	while (cnt < 30) {
		temp2 = DRV_Reg32(TEMPMSRCTL1);
		pr_debug("TEMPMSRCTL1 = 0x%x\n", temp2);

		if (((temp2 & 0x81) == 0x00) || ((temp2 & 0x81) == 0x81)) {

			DRV_WriteReg32(TEMPMSRCTL1, (temp2 | 0x10E));

			break;
		}
		pr_debug("temp=0x%x, cnt=%d\n", temp2, cnt);
		udelay(10);
		cnt++;
	}
	THERMAL_WRAP_WR32(0x00000000, TEMPMONCTL0);

	/* AuxADC Initialization,ref MT6582_AUXADC.doc */
	temp = DRV_Reg32(AUXADC_CON0_V); /*Auto set enable for CH11*/
	temp &= 0xFFFFF7FF; /*0: Not AUTOSET mode*/
	THERMAL_WRAP_WR32(temp, AUXADC_CON0_V);
	THERMAL_WRAP_WR32(0x800, AUXADC_CON1_CLR_V);
	THERMAL_WRAP_WR32(0x000003FF, TEMPMONCTL1);

#if THERMAL_CONTROLLER_HW_FILTER == 2
	THERMAL_WRAP_WR32(0x03FF03FF, TEMPMONCTL2);
	THERMAL_WRAP_WR32(0x006DEC78, TEMPAHBPOLL);
	THERMAL_WRAP_WR32(0x00000092, TEMPMSRCTL0);
#elif THERMAL_CONTROLLER_HW_FILTER == 4
	THERMAL_WRAP_WR32(0x03FF03FF, TEMPMONCTL2);
	THERMAL_WRAP_WR32(0x0043F459, TEMPAHBPOLL);
	THERMAL_WRAP_WR32(0x000000DB, TEMPMSRCTL0);
#elif THERMAL_CONTROLLER_HW_FILTER == 8
	THERMAL_WRAP_WR32(0x03390339, TEMPMONCTL2);
	THERMAL_WRAP_WR32(0x000C96FA, TEMPAHBPOLL);
	THERMAL_WRAP_WR32(0x00000124, TEMPMSRCTL0);
#elif THERMAL_CONTROLLER_HW_FILTER == 16
	THERMAL_WRAP_WR32(0x01C001C0, TEMPMONCTL2);
	THERMAL_WRAP_WR32(0x0006FE8B, TEMPAHBPOLL);
	THERMAL_WRAP_WR32(0x0000016D, TEMPMSRCTL0);
#else
	THERMAL_WRAP_WR32(0x03FF03FF, TEMPMONCTL2);
	THERMAL_WRAP_WR32(0x00FFFFFF, TEMPAHBPOLL);
	THERMAL_WRAP_WR32(0x00000000, TEMPMSRCTL0);
#endif

	THERMAL_WRAP_WR32(0xFFFFFFFF, TEMPAHBTO);
	THERMAL_WRAP_WR32(0x00000000, TEMPMONIDET0);
	THERMAL_WRAP_WR32(0x00000000, TEMPMONIDET1);

	THERMAL_WRAP_WR32(0x800, AUXADC_CON1_SET_V);
	THERMAL_WRAP_WR32(0x800, TEMPADCMUX);
	THERMAL_WRAP_WR32((UINT32) AUXADC_CON1_CLR_P, TEMPADCMUXADDR);
	THERMAL_WRAP_WR32(0x800, TEMPADCEN);
	THERMAL_WRAP_WR32((UINT32) AUXADC_CON1_SET_P, TEMPADCENADDR);
	THERMAL_WRAP_WR32((UINT32) AUXADC_DAT11_P, TEMPADCVALIDADDR);
	THERMAL_WRAP_WR32((UINT32) AUXADC_DAT11_P, TEMPADCVOLTADDR);
	THERMAL_WRAP_WR32(0x0, TEMPRDCTRL);
	THERMAL_WRAP_WR32(0x0000002C, TEMPADCVALIDMASK);
	THERMAL_WRAP_WR32(0x0, TEMPADCVOLTAGESHIFT);
	THERMAL_WRAP_WR32(0x2, TEMPADCWRITECTRL);
	temp = DRV_Reg32(TS_CON0);
	temp &= ~(0x000000C0);
	THERMAL_WRAP_WR32(temp, TS_CON0);

	/* Add delay time before sensor polling. */
	udelay(200);

	THERMAL_WRAP_WR32(0x0, TEMPADCPNP0);
	THERMAL_WRAP_WR32(0x1, TEMPADCPNP1);
	THERMAL_WRAP_WR32((UINT32) apmixed_phy_base + 0x604, TEMPPNPMUXADDR);
	THERMAL_WRAP_WR32(0x3, TEMPADCWRITECTRL);

	temp = DRV_Reg32(TEMPMSRCTL1);
	DRV_WriteReg32(TEMPMSRCTL1, ((temp & (~0x10E))));

	THERMAL_WRAP_WR32(0x00000003, TEMPMONCTL0);
}

static void set_thermal_ctrl_trigger_SPM(int temperature)
{
#if THERMAL_CONTROLLER_HW_TP
	int temp = 0;
	int raw_high, raw_middle, raw_low;

	pr_err("[Set_thermal_ctrl_trigger_SPM]: temperature=%d\n", temperature);

	/*temperature to trigger SPM state2 */
	raw_high   = temperature_to_raw_abb(temperature);
	raw_middle = temperature_to_raw_abb(20000);
	raw_low    = temperature_to_raw_abb(5000);

	temp = DRV_Reg32(TEMPMONINT);
	/* THERMAL_WRAP_WR32(temp & 0x8FFFFFFF, TEMPMONINT);*/	/* enable trigger SPM interrupt  */
	THERMAL_WRAP_WR32(temp & 0x1FFFFFFF, TEMPMONINT);	/* disable trigger SPM interrupt */

	THERMAL_WRAP_WR32(0x20000, TEMPPROTCTL); /* set hot to wakeup event control */
	THERMAL_WRAP_WR32(raw_low, TEMPPROTTA);
	THERMAL_WRAP_WR32(raw_middle, TEMPPROTTB);
	THERMAL_WRAP_WR32(raw_high, TEMPPROTTC); /* set hot to HOT wakeup event */

	/*trigger cold ,normal and hot interrupt*/
	/*remove for temp	THERMAL_WRAP_WR32(temp | 0xE0000000, TEMPMONINT); */
	/* enable trigger SPM interrupt */
	/*Only trigger hot interrupt*/
	THERMAL_WRAP_WR32(temp | 0x80000000, TEMPMONINT);	/* enable trigger SPM interrupt */
#endif
}

void mtkts_dump_cali_info(void)
{
	pr_debug("[calibration] g_adc_ge_t      = 0x%x\n", g_adc_ge_t);
	pr_debug("[calibration] g_adc_oe_t      = 0x%x\n", g_adc_oe_t);
	pr_debug("[calibration] g_degc_cali     = 0x%x\n", g_degc_cali);
	pr_debug("[calibration] g_adc_cali_en_t = 0x%x\n", g_adc_cali_en_t);
	pr_debug("[calibration] g_o_slope       = 0x%x\n", g_o_slope);
	pr_debug("[calibration] g_o_slope_sign  = 0x%x\n", g_o_slope_sign);
	pr_debug("[calibration] g_id            = 0x%x\n", g_id);

	pr_debug("[calibration] g_o_vtsmcu2     = 0x%x\n", g_o_vtsmcu2);
	pr_debug("[calibration] g_o_vtsmcu3     = 0x%x\n", g_o_vtsmcu3);
	pr_debug("[calibration] g_o_vtsmcu4     = 0x%x\n", g_o_vtsmcu4);
}


static void thermal_cal_prepare(struct device *dev)
{
	U32 *buf;
	struct nvmem_cell *cell;
	size_t len;

	g_adc_ge_t = 512;
	g_adc_oe_t = 512;
	g_degc_cali = 40;
	g_o_slope = 0;
	g_o_slope_sign = 0;
	g_o_vtsmcu1 = 287;
	g_o_vtsmcu2 = 287;
	g_o_vtsabb = 287;

	cell = nvmem_cell_get(dev, "calibration-data");
	if (IS_ERR(cell))
		return;

	buf = (u32 *)nvmem_cell_read(cell, &len);

	nvmem_cell_put(cell);

	if (IS_ERR(buf))
		return;

	if (len < 3 * sizeof(u32)) {
		dev_err(dev, "invalid calibration data\n");
		goto out;
	}

	calefuse1 = buf[0];
	calefuse2 = buf[1];
	calefuse3 = buf[2];
	pr_err("[Thermal calibration] buf[0]=0x%x, buf[1]=0x%x, , buf[2]=0x%x\n", buf[0], buf[1], buf[2]);

	g_adc_cali_en_t  = (buf[2] & 0x01000000)>>24;    /* ADC_CALI_EN_T(1b) *(0x1020642c)[24] */

	if (g_adc_cali_en_t) {
		g_adc_ge_t		= (buf[0] & 0x003FF000)>>12;	/*ADC_GE_T [9:0] *(0x10206424)[21:12] */
		g_adc_oe_t		= (buf[0] & 0xFFC00000)>>22;	/*ADC_OE_T [9:0] *(0x10206424)[31:22] */
		g_o_vtsmcu1		= (buf[1] & 0x000001FF);	/* O_VTSMCU1    (9b) *(0x10206428)[8:0] */
		g_o_vtsmcu2		= (buf[1] & 0x0003FE00)>>9;	/* O_VTSMCU2    (9b) *(0x10206428)[17:9] */
		g_o_vtsabb		= (buf[2] & 0x0003FE00)>>9;	/* O_VTSABB     (9b) *(0x1020642c)[17:9] */
		g_degc_cali		= (buf[2] & 0x00FC0000)>>18;	/* DEGC_cali    (6b) *(0x1020642c)[23:18] */
		g_o_slope		= (buf[2] & 0xFC000000)>>26;	/* O_SLOPE      (6b) *(0x1020642c)[31:26] */
		g_o_slope_sign		= (buf[2] & 0x02000000)>>25;	/* O_SLOPE_SIGN (1b) *(0x1020642c)[25] */
		g_id			= (buf[1] & 0x08000000)>>27;
		if (g_id == 0)
			g_o_slope = 0;
	} else {
		dev_info(dev, "Device not calibrated, using default calibration values\n");
	}

	pr_err("[Power/CPU_Thermal] [Thermal calibration] g_adc_ge_t      = 0x%x\n", g_adc_ge_t);
	pr_err("[Power/CPU_Thermal] [Thermal calibration] g_adc_oe_t      = 0x%x\n", g_adc_oe_t);
	pr_err("[Power/CPU_Thermal] [Thermal calibration] g_degc_cali     = 0x%x\n", g_degc_cali);
	pr_err("[Power/CPU_Thermal] [Thermal calibration] g_adc_cali_en_t = 0x%x\n", g_adc_cali_en_t);
	pr_err("[Power/CPU_Thermal] [Thermal calibration] g_o_slope       = 0x%x\n", g_o_slope);
	pr_err("[Power/CPU_Thermal] [Thermal calibration] g_o_slope_sign  = 0x%x\n", g_o_slope_sign);
	pr_err("[Power/CPU_Thermal] [Thermal calibration] g_id            = 0x%x\n", g_id);
	pr_err("[Power/CPU_Thermal] [Thermal calibration] g_o_vtsmcu1     = 0x%x\n", g_o_vtsmcu1);
	pr_err("[Power/CPU_Thermal] [Thermal calibration] g_o_vtsmcu2     = 0x%x\n", g_o_vtsmcu2);
	pr_err("[Power/CPU_Thermal] [Thermal calibration] g_o_vtsabb      = 0x%x\n", g_o_vtsabb);

out:
	kfree(buf);

	return;
}

static void thermal_cal_prepare_2(U32 ret)
{
	S32 format_1, format_2, format_abb;

	pr_debug("thermal_cal_prepare_2\n");

	g_ge = ((g_adc_ge_t - 512) * 10000) / 4096;
	g_oe =  (g_adc_oe_t - 512);

	g_gain = (10000 + g_ge);

	format_1   = (g_o_vtsmcu1 + 3350 - g_oe);
	format_2   = (g_o_vtsmcu2 + 3350 - g_oe);
	format_abb = (g_o_vtsabb  + 3350 - g_oe);

	g_x_roomt1   = (((format_1   * 10000) / 4096) * 10000) / g_gain;
	g_x_roomt2   = (((format_2   * 10000) / 4096) * 10000) / g_gain;
	g_x_roomtabb = (((format_abb * 10000) / 4096) * 10000) / g_gain;

	pr_debug("[Power/CPU_Thermal] [Thermal calibration] g_ge       = 0x%x\n", g_ge);
	pr_debug("[Power/CPU_Thermal] [Thermal calibration] g_gain     = 0x%x\n", g_gain);
	pr_debug("[Power/CPU_Thermal] [Thermal calibration] g_gain     = 0x%x\n", g_gain);
	pr_debug("[Power/CPU_Thermal] [Thermal calibration] g_x_roomt1 = 0x%x\n", g_x_roomt1);
	pr_debug("[Power/CPU_Thermal] [Thermal calibration] g_x_roomt2 = 0x%x\n", g_x_roomt2);
}

static kal_int32 temperature_to_raw_abb(kal_uint32 ret)
{

	kal_int32 t_curr = ret;
	/* kal_int32 y_curr = 0; */
	kal_int32 format_1 = 0;
	kal_int32 format_2 = 0;
	kal_int32 format_3 = 0;
	kal_int32 format_4 = 0;

	pr_debug("[temperature_to_raw_abb]\n");

	if (g_o_slope_sign == 0) {
		format_1 = t_curr-(g_degc_cali*1000/2);
		format_2 = format_1 * (165+g_o_slope) * 18/15;
		format_2 = format_2 - 2*format_2;
		format_3 = format_2/1000 + g_x_roomt1*10;
		format_4 = (format_3*4096/10000*g_gain)/100000 + g_oe;
	} else {
		format_1 = t_curr-(g_degc_cali*1000/2);
		format_2 = format_1 * (165-g_o_slope) * 18/15;
		format_2 = format_2 - 2*format_2;
		format_3 = format_2/1000 + g_x_roomt1*10;
		format_4 = (format_3*4096/10000*g_gain)/100000 + g_oe;
	}

	pr_debug("[temperature_to_raw_abb] temperature=%d, raw=%d", ret, format_4);
	return format_4;
}

static kal_int32 raw_to_temperature_MCU1(kal_uint32 ret)
{
	S32 t_current = 0;
	S32 y_curr = ret;
	S32 format_1 = 0;
	S32 format_2 = 0;
	S32 format_3 = 0;
	S32 format_4 = 0;

	pr_debug("raw_to_temperature_MCU1\n");

	if (ret == 0)
		return 0;

	format_1 = (g_degc_cali*10 / 2);
	format_2 = (y_curr - g_oe);
	format_3 = (((((format_2) * 10000) / 4096) * 10000) / g_gain) - g_x_roomt1;
	format_3 = format_3 * 15/18;

	if (g_o_slope_sign == 0)
		format_4 = ((format_3 * 100) / (165+g_o_slope)); /* uint = 0.1 deg */
	else
		format_4 = ((format_3 * 100) / (165-g_o_slope)); /* uint = 0.1 deg */

	format_4 = format_4 - (2 * format_4);
	t_current = format_1 + format_4; /* uint = 0.1 deg */

	return t_current;
}

static kal_int32 raw_to_temperature_MCU2(kal_uint32 ret)
{
	S32 t_current = 0;
	S32 y_curr = ret;
	S32 format_1 = 0;
	S32 format_2 = 0;
	S32 format_3 = 0;
	S32 format_4 = 0;

	pr_debug("raw_to_temperature_MCU2\n");
	if (ret == 0)
		return 0;

	format_1 = (g_degc_cali*10 / 2);
	format_2 = (y_curr - g_oe);
	format_3 = (((((format_2) * 10000) / 4096) * 10000) / g_gain) - g_x_roomt2;
	format_3 = format_3 * 15/18;

	if (g_o_slope_sign == 0)
		format_4 = ((format_3 * 100) / (165+g_o_slope)); /* uint = 0.1 deg */
	else
		format_4 = ((format_3 * 100) / (165-g_o_slope)); /* uint = 0.1 deg */
	format_4 = format_4 - (2 * format_4);

	t_current = format_1 + format_4; /* uint = 0.1 deg */

	return t_current;
}



static void thermal_calibration(void)
{
	if (g_adc_cali_en_t == 0)
		pr_debug("#####  Not Calibration  ######\n");

	pr_debug("thermal_calibration\n");
	thermal_cal_prepare_2(0);
}


static int get_immediate_temp1(void)
{
	int curr_raw1, curr_temp1;

	mutex_lock(&TS_lock);
	curr_raw1 = DRV_Reg32(TEMPMSR0);
	curr_raw1 = curr_raw1 & 0x0fff; /* bit0~bit11 */
	curr_temp1 = raw_to_temperature_MCU1(curr_raw1);
	curr_temp1 = curr_temp1*100;
	mutex_unlock(&TS_lock);
	/* pr_debug("[get_immediate_temp1] temp1=%d, raw1=%d\n", curr_temp1, curr_raw1); */
	return curr_temp1;
}

static int get_immediate_temp2(void)
{
	int curr_raw2, curr_temp2;

	mutex_lock(&TS_lock);
	curr_raw2 = DRV_Reg32(TEMPMSR1);
	curr_raw2 = curr_raw2 & 0x0fff; /* bit0~bit11 */
	curr_temp2 = raw_to_temperature_MCU2(curr_raw2);
	curr_temp2 = curr_temp2*100;
	mutex_unlock(&TS_lock);
	/* pr_debug("[get_immediate_temp2] temp2=%d, raw2=%d\n", curr_temp2, curr_raw2); */
	return curr_temp2;
}

int get_immediate_temp2_wrap(void)
{
	int curr_raw;

	curr_raw = get_immediate_temp2();
	pr_debug("[get_immediate_temp2_wrap] curr_raw=%d\n", curr_raw);
	return curr_raw;
}

int CPU_Temp(void)
{
	int curr_temp, curr_temp2;

	curr_temp = get_immediate_temp1();
	curr_temp2 = get_immediate_temp2();

	return curr_temp;
}

static int tscpu_get_temp(struct thermal_zone_device *thermal, int *t)
{
#if MTK_TS_CPU_SW_FILTER == 1
	int ret = 0;
	int curr_temp;
	int temp_temp;

	curr_temp = CPU_Temp();

	pr_debug(" mtktscpu_get_temp  CPU T1=%d\n", curr_temp);

	if ((curr_temp > (trip_temp[0] - 15000)) || (curr_temp < -30000))
		pr_err("[Power/CPU_Thermal] CPU T=%d\n", curr_temp);

	temp_temp = curr_temp;
	/* not resumed from suspensio */
	if (curr_temp != 0) {
		/* invalid range */
		if ((curr_temp > 150000) || (curr_temp < -20000)) {
			pr_err("[Power/CPU_Thermal] CPU temp invalid=%d\n", curr_temp);
			temp_temp = 50000;
			ret = -1;
		} else if (last_cpu_real_temp != 0) {
			if ((curr_temp - last_cpu_real_temp > 20000) || ((last_cpu_real_temp - curr_temp) > 20000)) {
				pr_err("[Power/CPU_Thermal] CPU temp float hugely temp=%d, lasttemp=%d\n",
					curr_temp, last_cpu_real_temp);
				temp_temp = 50000;
				ret = -1;
			}
		}
	}

	last_cpu_real_temp = curr_temp;
	curr_temp = temp_temp;
#else
	int ret = 0;
	static int count = 0;
	int curr_temp;

	curr_temp = CPU_Temp();

	pr_debug(" mtktscpu_get_temp CPU T1=%d\n", curr_temp);

	if ((curr_temp > (trip_temp[0] - 5000)) || (curr_temp < -30000)) {
		if (count++ &0x01) {
			pr_err("[Power/CPU_Thermal] %d: CPU T=%d\n", count, curr_temp);
		}
	}
#endif
	read_curr_temp = curr_temp;
	*t = (unsigned long) curr_temp;
	return ret;
}

static int tscpu_bind(struct thermal_zone_device *thermal, struct thermal_cooling_device *cdev)
{
	int table_val = 0;

	if (!strcmp(cdev->type, g_bind0)) {
		table_val = 0;
		set_thermal_ctrl_trigger_SPM(trip_temp[0]);

		pr_debug("tscpu_bind %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind1)) {
		table_val = 1;
		tc_mid_trip = trip_temp[1];

		pr_debug("tscpu_bind %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind2)) {
		table_val = 2;
		pr_debug("tscpu_bind %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind3)) {
		table_val = 3;
		pr_debug("tscpu_bind %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind4)) {
		table_val = 4;
		pr_debug("tscpu_bind %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind5)) {
		table_val = 5;
		pr_debug("tscpu_bind %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind6)) {
		table_val = 6;
		pr_debug("tscpu_bind %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind7)) {
		table_val = 7;
		pr_debug("tscpu_bind %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind8)) {
		table_val = 8;
		pr_debug("tscpu_bind %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind9)) {
		table_val = 9;
		pr_debug("tscpu_bind %s\n", cdev->type);
	} else {
		return 0;
	}

	pr_debug("tscpu_bind binding OK, %d\n", table_val);

	return 0;
}

static int tscpu_unbind(struct thermal_zone_device *thermal, struct thermal_cooling_device *cdev)
{
	int table_val = 0;

	if (!strcmp(cdev->type, g_bind0)) {
		table_val = 0;
		pr_debug("tscpu_unbind %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind1)) {
		table_val = 1;
		tc_mid_trip = -275000;
		pr_debug("tscpu_unbind %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind2)) {
		table_val = 2;
		pr_debug("tscpu_unbind %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind3)) {
		table_val = 3;
		pr_debug("tscpu_unbind %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind4)) {
		table_val = 4;
		pr_debug("tscpu_unbind %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind5)) {
		table_val = 5;
		pr_debug("tscpu_unbind %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind6)) {
		table_val = 6;
		pr_debug("tscpu_unbind %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind7)) {
		table_val = 7;
		pr_debug("tscpu_unbind %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind8)) {
		table_val = 8;
		pr_debug("tscpu_unbind %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind9)) {
		table_val = 9;
		pr_debug("tscpu_unbind %s\n", cdev->type);
	} else
		return 0;

	pr_debug("tscpu_unbind unbinding OK\n");

	return 0;
}

static int tscpu_get_mode(struct thermal_zone_device *thermal, enum thermal_device_mode *mode)
{
	*mode = (kernelmode) ? THERMAL_DEVICE_ENABLED : THERMAL_DEVICE_DISABLED;
	return 0;
}

static int tscpu_set_mode(struct thermal_zone_device *thermal, enum thermal_device_mode mode)
{
	kernelmode = mode;
	return 0;
}

static int tscpu_get_trip_type(struct thermal_zone_device *thermal, int trip,
			       enum thermal_trip_type *type)
{
	*type = g_THERMAL_TRIP[trip];
	return 0;
}

static int tscpu_get_trip_temp(struct thermal_zone_device *thermal, int trip, int *temp)
{
	*temp = trip_temp[trip];
	return 0;
}

static int tscpu_get_crit_temp(struct thermal_zone_device *thermal, int *temperature)
{
	*temperature = MTKTSCPU_TEMP_CRIT;
	return 0;
}

/* bind callback functions to thermalzone */
static struct thermal_zone_device_ops mtktscpu_dev_ops = {
	.bind = tscpu_bind,
	.unbind = tscpu_unbind,
	.get_temp = tscpu_get_temp,
	.get_mode = tscpu_get_mode,
	.set_mode = tscpu_set_mode,
	.get_trip_type = tscpu_get_trip_type,
	.get_trip_temp = tscpu_get_trip_temp,
	.get_crit_temp = tscpu_get_crit_temp,
};

static int tscpu_read_temperature_info(struct seq_file *m, void *v)
{
	seq_printf(m, "current temp:%d\n", read_curr_temp);
	seq_printf(m, "calefuse1:0x%x\n", calefuse1);
	seq_printf(m, "calefuse2:0x%x\n", calefuse2);
	seq_printf(m, "calefuse3:0x%x\n", calefuse3);
	seq_printf(m, "g_adc_ge_t:%d\n", g_adc_ge_t);
	seq_printf(m, "g_adc_oe_t:%d\n", g_adc_oe_t);
	seq_printf(m, "g_degc_cali:%d\n", g_degc_cali);
	seq_printf(m, "g_adc_cali_en_t:%d\n", g_adc_cali_en_t);
	seq_printf(m, "g_o_slope:%d\n", g_o_slope);
	seq_printf(m, "g_o_slope_sign:%d\n", g_o_slope_sign);
	seq_printf(m, "g_id:%d\n", g_id);
	seq_printf(m, "g_o_vtsmcu1:%d\n", g_o_vtsmcu1);
	seq_printf(m, "g_o_vtsmcu2:%d\n", g_o_vtsmcu2);
	seq_printf(m, "g_o_vtsmcu3:%d\n", g_o_vtsmcu3);
	seq_printf(m, "g_o_vtsmcu4:%d\n", g_o_vtsmcu4);
	seq_printf(m, "g_o_vtsabb :%d\n", g_o_vtsabb);

	return 0;
}

int tscpu_register_thermal(void)
{

	pr_debug("tscpu_register_thermal\n");

	/* trips : trip 0~3 */
	thz_dev = mtk_thermal_zone_device_register("mtktscpu", num_trip, NULL,
						   &mtktscpu_dev_ops, 0, 0, 0, interval);

	return 0;
}

void tscpu_unregister_thermal(void)
{

	pr_debug("tscpu_unregister_thermal\n");
	if (thz_dev) {
		mtk_thermal_zone_device_unregister(thz_dev);
		thz_dev = NULL;
	}
}

static ssize_t tscpu_write(struct file *file, const char __user *buffer, size_t count,
		loff_t *data)
{
	int len = 0, time_msec = 0;
	int trip[10] = { 0 };
	int t_type[10] = { 0 };
	int i;
	char bind0[20], bind1[20], bind2[20], bind3[20], bind4[20];
	char bind5[20], bind6[20], bind7[20], bind8[20], bind9[20];
	char desc[512];

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;
	desc[len] = '\0';

	if (sscanf
			(desc,
			 "%d %d %d %s %d %d %s %d %d %s %d %d %s %d %d %s %d %d %s %d %d %s %d %d %s %d %d %s %d %d %s %d %d",
			 &num_trip, &trip[0], &t_type[0], bind0, &trip[1], &t_type[1], bind1, &trip[2],
			 &t_type[2], bind2, &trip[3], &t_type[3], bind3, &trip[4], &t_type[4], bind4, &trip[5],
			 &t_type[5], bind5, &trip[6], &t_type[6], bind6, &trip[7], &t_type[7], bind7, &trip[8],
			 &t_type[8], bind8, &trip[9], &t_type[9], bind9, &time_msec, &MA_len_temp) == 33) {

		pr_debug("tscpu_write tscpu_unregister_thermal MA_len_temp=%d\n", MA_len_temp);

		tscpu_unregister_thermal();


		for (i = 0; i < num_trip; i++)
			g_THERMAL_TRIP[i] = t_type[i];

		g_bind0[0] = g_bind1[0] = g_bind2[0] = g_bind3[0] = g_bind4[0] = g_bind5[0] =
			g_bind6[0] = g_bind7[0] = g_bind8[0] = g_bind9[0] = '\0';

		for (i = 0; i < 20; i++) {
			g_bind0[i] = bind0[i];
			g_bind1[i] = bind1[i];
			g_bind2[i] = bind2[i];
			g_bind3[i] = bind3[i];
			g_bind4[i] = bind4[i];
			g_bind5[i] = bind5[i];
			g_bind6[i] = bind6[i];
			g_bind7[i] = bind7[i];
			g_bind8[i] = bind8[i];
			g_bind9[i] = bind9[i];
		}

		pr_debug("tscpu_write g_THERMAL_TRIP_0=%d,g_THERMAL_TRIP_1=%d,g_THERMAL_TRIP_2=%d,",
				g_THERMAL_TRIP[0], g_THERMAL_TRIP[1], g_THERMAL_TRIP[2]);
		pr_debug("g_THERMAL_TRIP_3=%d,g_THERMAL_TRIP_4=%d,g_THERMAL_TRIP_5=%d,",
				g_THERMAL_TRIP[3], g_THERMAL_TRIP[4], g_THERMAL_TRIP[5]);
		pr_debug("g_THERMAL_TRIP_6=%d,g_THERMAL_TRIP_7=%d,g_THERMAL_TRIP_8=%d,g_THERMAL_TRIP_9=%d,\n",
				g_THERMAL_TRIP[6], g_THERMAL_TRIP[7], g_THERMAL_TRIP[8],	g_THERMAL_TRIP[9]);
		pr_debug("tscpu_write cooldev0=%s,cooldev1=%s,cooldev2=%s,cooldev3=%s,cooldev4=%s,",
				g_bind0, g_bind1, g_bind2, g_bind3, g_bind4);
		pr_debug("cooldev5=%s,cooldev6=%s,cooldev7=%s,cooldev8=%s,cooldev9=%s\n",
				g_bind5, g_bind6, g_bind7, g_bind8,	g_bind9);

		for (i = 0; i < num_trip; i++)
			trip_temp[i] = trip[i];

		interval = time_msec;

		pr_debug("tscpu_write trip_0_temp=%d,trip_1_temp=%d,trip_2_temp=%d,",
									trip_temp[0], trip_temp[1], trip_temp[2]);
		pr_debug("trip_3_temp=%d,trip_4_temp=%d,trip_5_temp=%d,",
									trip_temp[3], trip_temp[4], trip_temp[5]);
		pr_debug("trip_6_temp=%d,trip_7_temp=%d,trip_8_temp=%d,",
									trip_temp[6],	trip_temp[7], trip_temp[8]);
		pr_debug("trip_9_temp=%d,time_ms=%d, num_trip=%d\n",
									trip_temp[9], interval, num_trip);

		/* get temp, set high low threshold */

		pr_debug("tscpu_write tscpu_register_thermal\n");
		tscpu_register_thermal();

		proc_write_flag = 1;

		return count;
	}
	pr_debug("tscpu_write bad argument\n");

	return -EINVAL;
}

/*tscpu_thermal_suspend spend 1000us~1310us*/
static int tscpu_thermal_suspend(struct platform_device *dev, pm_message_t state)
{
	int cnt = 0;
	int temp = 0;

	pr_debug("tscpu_thermal_suspend\n");

	g_tc_resume = 1;	/* set "1", don't read temp during suspend */

	if (talking_flag == false) {
		pr_debug("tscpu_thermal_suspend no talking\n");

		while (cnt < 30) {
			temp = DRV_Reg32(TEMPMSRCTL1);
			if (((temp & 0x81) == 0x00) || ((temp & 0x81) == 0x81)) {
				DRV_WriteReg32(TEMPMSRCTL1, (temp | 0x0E));
				break;
			}
			udelay(10);
			cnt++;
		}
		THERMAL_WRAP_WR32(0x00000000, TEMPMONCTL0);
		/*TSCON1[5:4]=2'b11, Buffer off */
		THERMAL_WRAP_WR32(DRV_Reg32(TS_CON1) | 0x00000030, TS_CON1);
		tscpu_thermal_clock_off();
	}
	return 0;
}

/*tscpu_thermal_suspend spend 3000us~4000us*/
static int tscpu_thermal_resume(struct platform_device *dev)
{

	pr_debug("tscpu_thermal_resume\n");
	g_tc_resume = 1;	/* set "1", don't read temp during start resume */

	if (talking_flag == false) {
		THERMAL_WRAP_WR32(DRV_Reg32(TS_CON0) | 0x000000C0, TS_CON0);
		thermal_reset_and_initial();
		set_thermal_ctrl_trigger_SPM(trip_temp[0]);
	}

	g_tc_resume = 2;	/* set "2", resume finish,can read temp */

	return 0;
}

static int tscpu_read_temperature_open(struct inode *inode, struct file *file)
{
	return single_open(file, tscpu_read_temperature_info, NULL);
}

static const struct file_operations mtktscpu_read_temperature_fops = {
	.owner = THIS_MODULE,
	.open = tscpu_read_temperature_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = tscpu_write,
	.release = single_release,
};

int thermal_fast_init(void)
{
	UINT32 temp;
	UINT32 cunt = 0;

	pr_debug("thermal_fast_init\n");

	temp = 0xDA1;
	DRV_WriteReg32(TEMPSPARE2, (0x00001000 + temp));
	DRV_WriteReg32(TEMPMONCTL1, 1);
	DRV_WriteReg32(TEMPMONCTL2, 1);
	DRV_WriteReg32(TEMPAHBPOLL, 1);

	DRV_WriteReg32(TEMPAHBTO,    0x000000FF);
	DRV_WriteReg32(TEMPMONIDET0, 0x00000000);
	DRV_WriteReg32(TEMPMONIDET1, 0x00000000);

	DRV_WriteReg32(TEMPMSRCTL0, 0x0000000);

	DRV_WriteReg32(TEMPADCPNP0, 0x1);
	DRV_WriteReg32(TEMPADCPNP1, 0x2);
	DRV_WriteReg32(TEMPADCPNP2, 0x3);

	DRV_WriteReg32(TEMPPNPMUXADDR, thermal_phy_base + 0x0F0);
	DRV_WriteReg32(TEMPADCMUXADDR, thermal_phy_base + 0x0F0);
	DRV_WriteReg32(TEMPADCENADDR,  thermal_phy_base + 0X0F4);
	DRV_WriteReg32(TEMPADCVALIDADDR, thermal_phy_base + 0X0F8);
	DRV_WriteReg32(TEMPADCVOLTADDR, thermal_phy_base + 0X0F8);

	DRV_WriteReg32(TEMPRDCTRL, 0x0);
	DRV_WriteReg32(TEMPADCVALIDMASK, 0x0000002C);
	DRV_WriteReg32(TEMPADCVOLTAGESHIFT, 0x0);
	DRV_WriteReg32(TEMPADCWRITECTRL, 0x3);

	DRV_WriteReg32(TEMPMONINT, 0x00000000);

	DRV_WriteReg32(TEMPMONCTL0, 0x0000000F);

	temp = DRV_Reg32(TEMPMSR0) & 0x0fff;
	while (temp != 0xDA1 && cunt < 20) {
		cunt++;
		pr_debug("[Power/CPU_Thermal]0 temp=%d,cunt=%d\n", temp, cunt);
		temp = DRV_Reg32(TEMPMSR0) & 0x0fff;
	}

	cunt = 0;
	temp = DRV_Reg32(TEMPMSR1) & 0x0fff;
	while (temp != 0xDA1 &&  cunt < 20) {
		cunt++;
		pr_debug("[Power/CPU_Thermal]1 temp=%d,cunt=%d\n", temp, cunt);
		temp = DRV_Reg32(TEMPMSR1) & 0x0fff;
	}

	cunt = 0;
	temp = DRV_Reg32(TEMPMSR2) & 0x0fff;
	while (temp != 0xDA1 &&  cunt < 20) {
		cunt++;
		pr_debug("[Power/CPU_Thermal]2 temp=%d,cunt=%d\n", temp, cunt);
		temp = DRV_Reg32(TEMPMSR2) & 0x0fff;
	}
	return 0;
}

#ifdef CONFIG_OF

static u64 of_get_phys_base(struct device_node *np)
{
	u64 size64;
	const __be32 *regaddr_p;

	regaddr_p = of_get_address(np, 0, &size64, NULL);
	if (!regaddr_p)
		return OF_BAD_ADDR;

	return of_translate_address(np, regaddr_p);
}

static int get_io_reg_base(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;

	if (node) {
		/* Setup IO addresses */
		thermal_base = of_iomap(node, 0);
		pr_debug("[THERM_CTRL] thermal_base=0x%lx\n", (unsigned long)thermal_base);

		/* get thermal phy base */
		thermal_phy_base = of_get_phys_base(node);
		pr_debug("[THERM_CTRL] thermal_phy_base=0x%lx\n", (unsigned long)thermal_phy_base);

		/* get thermal irq num */
		thermal_irq_number = irq_of_parse_and_map(node, 0);
		pr_debug("[THERM_CTRL] thermal_irq_number=0x%lx\n",
			(unsigned long)thermal_irq_number);

		auxadc_ts_base = of_parse_phandle(node, "auxadc", 0);
		pr_debug("[THERM_CTRL] auxadc_ts_base=0x%lx\n", (unsigned long)auxadc_ts_base);

		auxadc_ts_phy_base = of_get_phys_base(auxadc_ts_base);
		pr_debug("[THERM_CTRL] auxadc_ts_phy_base=0x%lx\n",
			(unsigned long)auxadc_ts_phy_base);

		apmixed_ts_base = of_parse_phandle(node, "apmixedsys", 0);
		pr_debug("[THERM_CTRL] apmixed_ts_base=0x%lx\n", (unsigned long)apmixed_ts_base);

		apmixed_phy_base = of_get_phys_base(apmixed_ts_base);
		pr_debug("[THERM_CTRL] apmixed_phy_base=0x%lx\n", (unsigned long)apmixed_phy_base);

		pericfg_base = of_parse_phandle(node, "pericfg", 0);
		pr_debug("[THERM_CTRL] pericfg_base=0x%lx\n", (unsigned long)pericfg_base);
	}

	return 1;
}
#endif

static kuid_t uid = KUIDT_INIT(0);
static kgid_t gid = KGIDT_INIT(1000);

static int tscpu_thermal_probe(struct platform_device *pdev)
{
	int err = 0;
	struct proc_dir_entry *entry = NULL;
	struct proc_dir_entry *mtktscpu_dir = NULL;

	pr_debug("tscpu_thermal_probe\n");

	clk_peri_therm = devm_clk_get(&pdev->dev, "therm");
	WARN_ON(IS_ERR(clk_peri_therm));

	clk_auxadc = devm_clk_get(&pdev->dev, "auxadc");
	WARN_ON(IS_ERR(clk_auxadc));

#ifdef CONFIG_OF
	if (get_io_reg_base(pdev) == 0)
		return 0;
#endif

	thermal_cal_prepare(&pdev->dev);

	thermal_calibration();

	THERMAL_WRAP_WR32(DRV_Reg32(TS_CON0) | 0x000000C0, TS_CON0);

	thermal_reset_and_initial();

	set_thermal_ctrl_trigger_SPM(trip_temp[0]);

	get_thermal_all_register();

#ifdef CONFIG_OF
	err =
	    request_irq(thermal_irq_number, thermal_interrupt_handler, IRQF_TRIGGER_LOW,
			THERMAL_NAME, NULL);
	if (err)
		pr_emerg("tscpu_thermal_probe IRQ register fail\n");
#else
	err =
	    request_irq(THERM_CTRL_IRQ_BIT_ID, thermal_interrupt_handler, IRQF_TRIGGER_LOW,
			THERMAL_NAME, NULL);
	if (err)
		pr_emerg("tscpu_thermal_probe IRQ register fail\n");
#endif

	err = tscpu_register_thermal();
	if (err) {
		pr_debug("tscpu_register_thermal fail\n");
		goto err_unreg;
	}

	mtktscpu_dir = mtk_thermal_get_proc_drv_therm_dir_entry();
	if (!mtktscpu_dir)
		pr_emerg("tscpu_thermal_probe mkdir /proc/driver/thermal failed\n");
	else {
		entry =
		    proc_create("tzcpu_read_temperature", S_IRUGO, mtktscpu_dir,
				&mtktscpu_read_temperature_fops);

		if (entry)
			proc_set_user(entry, uid, gid);

	}
	return 0;

err_unreg:
	return err;
}

#ifdef CONFIG_OF
static const struct of_device_id mt_thermal_of_match[] = {
	{.compatible = THERMAL_DEVICE_NAME_2701,},
	{},
};
#endif

static struct platform_driver mtk_thermal_driver = {
	.remove = NULL,
	.shutdown = NULL,
	.probe = tscpu_thermal_probe,
	.suspend = tscpu_thermal_suspend,
	.resume = tscpu_thermal_resume,
	.driver = {
		   .name = THERMAL_NAME,
#ifdef CONFIG_OF
		   .of_match_table = mt_thermal_of_match,
#endif
		   },
};

static int __init tscpu_init(void)
{
	return platform_driver_register(&mtk_thermal_driver);
}

static void __exit tscpu_exit(void)
{

	pr_debug("tscpu_exit\n");

	tscpu_unregister_thermal();

	tscpu_thermal_clock_off();

	return platform_driver_unregister(&mtk_thermal_driver);
}
module_init(tscpu_init);
module_exit(tscpu_exit);
