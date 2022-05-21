/*
 * mt8518_afe_common.h  --  Mediatek 8518 audio driver common definitions
 *
 * Copyright (c) 2018 MediaTek Inc.
 * Author: Hidalgo Huang <hidalgo.huang@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _MT8518_AFE_COMMON_H_
#define _MT8518_AFE_COMMON_H_

#define COMMON_CLOCK_FRAMEWORK_API
/* #define DEBUG_AFE_REGISTER_RW */

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/regmap.h>
#include <linux/hrtimer.h>
#include <linux/of_gpio.h>
#include <sound/asound.h>

#define SPDIF_CHSTS_NUM			6
#define SPDIF_USERCODE_NUM		12
#define MT8518_ETDM_MAX_CHANNELS	16
/* #define CALI_RESULT_CHECK */

enum {
	MT8518_AFE_MEMIF_DLM,
	MT8518_AFE_MEMIF_DL2,
	MT8518_AFE_MEMIF_DL3,
	MT8518_AFE_MEMIF_DL6,
	MT8518_AFE_MEMIF_DL7,
	MT8518_AFE_MEMIF_DL8,
	MT8518_AFE_MEMIF_UL1,
	MT8518_AFE_MEMIF_UL2,
	MT8518_AFE_MEMIF_UL3,
	MT8518_AFE_MEMIF_UL4,
	MT8518_AFE_MEMIF_UL5,
	MT8518_AFE_MEMIF_UL8,
	MT8518_AFE_MEMIF_UL9,
	MT8518_AFE_MEMIF_UL10,
	MT8518_AFE_MEMIF_NUM,
	MT8518_AFE_BACKEND_BASE = MT8518_AFE_MEMIF_NUM,
	MT8518_AFE_IO_ETDM1_OUT = MT8518_AFE_BACKEND_BASE,
	MT8518_AFE_IO_ETDM1_IN,
	MT8518_AFE_IO_ETDM2_OUT,
	MT8518_AFE_IO_ETDM2_IN,
	MT8518_AFE_IO_PCM1,
	MT8518_AFE_IO_VIRTUAL_DL_SRC,
	MT8518_AFE_IO_DMIC,
	MT8518_AFE_IO_INT_ADDA,
	MT8518_AFE_IO_GASRC0,
	MT8518_AFE_IO_GASRC1,
	MT8518_AFE_IO_GASRC2,
	MT8518_AFE_IO_GASRC3,
	MT8518_AFE_IO_SPDIF_OUT,
	MT8518_AFE_IO_SPDIF_IN,
	MT8518_AFE_IO_MULTI_IN,
	MT8518_AFE_BACKEND_END,
	MT8518_AFE_BACKEND_NUM = (MT8518_AFE_BACKEND_END -
				  MT8518_AFE_BACKEND_BASE),
};

enum {
	MT8518_AFE_IRQ1, /* SPDIF OUT */
	MT8518_AFE_IRQ2, /* SPDIF IN DETECT */
	MT8518_AFE_IRQ3, /* SPDIF IN DATA */
	MT8518_AFE_IRQ10,
	MT8518_AFE_IRQ11,
	MT8518_AFE_IRQ12,
	MT8518_AFE_IRQ13,
	MT8518_AFE_IRQ14,
	MT8518_AFE_IRQ15,
	MT8518_AFE_IRQ16,
	MT8518_AFE_IRQ17,
	MT8518_AFE_IRQ18,
	MT8518_AFE_IRQ19,
	MT8518_AFE_IRQ20,
	MT8518_AFE_IRQ21,
	MT8518_AFE_IRQ_NUM,
};

enum {
	MT8518_TOP_CG_AFE,
	MT8518_TOP_CG_APLL,
	MT8518_TOP_CG_APLL2,
	MT8518_TOP_CG_DAC,
	MT8518_TOP_CG_DAC_PREDIS,
	MT8518_TOP_CG_ADC,
	MT8518_TOP_CG_TML,
	MT8518_TOP_CG_UPLINK_TML,
	MT8518_TOP_CG_I2S_IN,
	MT8518_TOP_CG_TDM_IN,
	MT8518_TOP_CG_I2S_OUT,
	MT8518_TOP_CG_TDM_OUT,
	MT8518_TOP_CG_ASRC11,
	MT8518_TOP_CG_ASRC12,
	MT8518_TOP_CG_DL_ASRC,
	MT8518_TOP_CG_A1SYS,
	MT8518_TOP_CG_A2SYS,
	MT8518_TOP_CG_AFE_CONN,
	MT8518_TOP_CG_PCMIF,
	MT8518_TOP_CG_GASRC0,
	MT8518_TOP_CG_GASRC1,
	MT8518_TOP_CG_GASRC2,
	MT8518_TOP_CG_GASRC3,
	MT8518_TOP_CG_DMIC0,
	MT8518_TOP_CG_DMIC1,
	MT8518_TOP_CG_DMIC2,
	MT8518_TOP_CG_DMIC3,
	MT8518_TOP_CG_A1SYS_TIMING,
	MT8518_TOP_CG_A2SYS_TIMING,
	MT8518_TOP_CG_SPDIF_OUT,
	MT8518_TOP_CG_MULTI_IN,
	MT8518_TOP_CG_INTDIR,
	MT8518_TOP_CG_NUM
};

enum {
	MT8518_CLK_TOP_AUD_26M,
	MT8518_CLK_TOP_AUD_BUS,
	MT8518_CLK_FA1SYS,
	MT8518_CLK_FA2SYS,
	MT8518_CLK_HAPLL1,
	MT8518_CLK_HAPLL2,
	MT8518_CLK_AUD1,
	MT8518_CLK_AUD2,
	MT8518_CLK_FASM_L,
	MT8518_CLK_FASM_M,
	MT8518_CLK_FASM_H,
	MT8518_CLK_SPDIF_IN,
	MT8518_CLK_APLL12_DIV0,
	MT8518_CLK_APLL12_DIV3,
	MT8518_CLK_APLL12_DIV4,
	MT8518_CLK_APLL12_DIV6,
	MT8518_CLK_I2S0_M_SEL,
	MT8518_CLK_I2S3_M_SEL,
	MT8518_CLK_I2S4_M_SEL,
	MT8518_CLK_I2S6_M_SEL,
	MT8518_CLK_APLL1,
	MT8518_CLK_APLL2,
	MT8518_CLK_NUM
};

enum {
	MT8518_ETDM1 = 0,
	MT8518_ETDM2,
	MT8518_ETDM_SETS,
};

enum {
	MT8518_ETDM_DATA_ONE_PIN = 0,
	MT8518_ETDM_DATA_MULTI_PIN,
};

enum {
	MT8518_ETDM_SEPARATE_CLOCK = 0,
	MT8518_ETDM_SHARED_CLOCK,
};

enum {
	MT8518_ETDM_SYNC_NONE = 0,
	MT8518_ETDM_SYNC_FROM_IN1,
	MT8518_ETDM_SYNC_FROM_IN2,
	MT8518_ETDM_SYNC_FROM_OUT1,
	MT8518_ETDM_SYNC_FROM_OUT2,
};

enum {
	MT8518_ETDM_FORMAT_I2S = 0,
	MT8518_ETDM_FORMAT_LJ,
	MT8518_ETDM_FORMAT_RJ,
	MT8518_ETDM_FORMAT_EIAJ,
	MT8518_ETDM_FORMAT_DSPA,
	MT8518_ETDM_FORMAT_DSPB,
};

enum {
	MT8518_PCM_FORMAT_I2S = 0,
	MT8518_PCM_FORMAT_EIAJ,
	MT8518_PCM_FORMAT_PCMA,
	MT8518_PCM_FORMAT_PCMB,
};

enum {
	MT8518_MULTI_IN_FORMAT_I2S = 0,
	MT8518_MULTI_IN_FORMAT_LJ,
	MT8518_MULTI_IN_FORMAT_RJ,
};

enum {
	MT8518_FS_8K = 0,
	MT8518_FS_12K,
	MT8518_FS_16K,
	MT8518_FS_24K,
	MT8518_FS_32K,
	MT8518_FS_48K,
	MT8518_FS_96K,
	MT8518_FS_192K,
	MT8518_FS_384K,
	MT8518_FS_ETDMOUT1_1X_EN,
	MT8518_FS_ETDMOUT2_1X_EN,
	MT8518_FS_ETDMIN1_1X_EN = 12,
	MT8518_FS_ETDMIN2_1X_EN,
	MT8518_FS_EXT_PCM_1X_EN = 15,
	MT8518_FS_7D35K,
	MT8518_FS_11D025K,
	MT8518_FS_14D7K,
	MT8518_FS_22D05K,
	MT8518_FS_29D4K,
	MT8518_FS_44D1K,
	MT8518_FS_88D2K,
	MT8518_FS_176D4K,
	MT8518_FS_352D8K,
	MT8518_FS_ETDMIN1_NX_EN,
	MT8518_FS_ETDMIN2_NX_EN,
	MT8518_FS_AMIC_1X_EN_ASYNC = 28,
};

enum {
	SPDIF_IN_PORT_NONE = 0,
	SPDIF_IN_PORT_OPT,
	SPDIF_IN_PORT_COAXIAL,
	SPDIF_IN_PORT_ARC,
	SPDIF_IN_PORT_NUM
};

enum {
	SPDIF_IN_MUX_0 = 0,
	SPDIF_IN_MUX_1,
	SPDIF_IN_MUX_2,
};

enum {
	MT8518_ETDM_FORCE_ON_DEFAULT = 0,
	MT8518_ETDM_FORCE_ON_1ST_TRIGGER,
};

enum {
	MT8518_AFE_DEBUGFS_ETDM,
	MT8518_AFE_DEBUGFS_MEMIF,
	MT8518_AFE_DEBUGFS_IRQ,
	MT8518_AFE_DEBUGFS_CONN,
	MT8518_AFE_DEBUGFS_ADDA,
	MT8518_AFE_DEBUGFS_GASRC,
	MT8518_AFE_DEBUGFS_SPDIF,
	MT8518_AFE_DEBUGFS_DBG,
	MT8518_AFE_DEBUGFS_NUM,
};

struct mt8518_fe_dai_data {
	bool slave_mode;
	bool use_sram;
	unsigned int sram_phy_addr;
	void __iomem *sram_vir_addr;
	unsigned int sram_size;
	unsigned int prealloc_size;
	unsigned int pbuf_size_conf;
	unsigned int min_hw_irq_period_us;
};

struct mt8518_be_dai_data {
	bool prepared[SNDRV_PCM_STREAM_LAST + 1];
	unsigned int fmt_mode;
};

struct mt8518_etdm_data {
	int occupied[SNDRV_PCM_STREAM_LAST + 1];
	int active[SNDRV_PCM_STREAM_LAST + 1];
	bool slave_mode[SNDRV_PCM_STREAM_LAST + 1];
	bool lrck_inv[SNDRV_PCM_STREAM_LAST + 1];
	bool bck_inv[SNDRV_PCM_STREAM_LAST + 1];
	bool enable_interlink[SNDRV_PCM_STREAM_LAST + 1];
	unsigned int lrck_width[SNDRV_PCM_STREAM_LAST + 1];
	unsigned int data_mode[SNDRV_PCM_STREAM_LAST + 1];
	unsigned int format[SNDRV_PCM_STREAM_LAST + 1];
	unsigned int mclk_freq[SNDRV_PCM_STREAM_LAST + 1];
	unsigned int clock_mode;
	bool in_disable_ch[MT8518_ETDM_MAX_CHANNELS];
	bool force_on[SNDRV_PCM_STREAM_LAST + 1];
	bool force_on_status[SNDRV_PCM_STREAM_LAST + 1];
	unsigned int force_on_policy[SNDRV_PCM_STREAM_LAST + 1];
	unsigned int force_rate[SNDRV_PCM_STREAM_LAST + 1];
	unsigned int force_channels[SNDRV_PCM_STREAM_LAST + 1];
	unsigned int force_bit_width[SNDRV_PCM_STREAM_LAST + 1];
	unsigned int sync_source[SNDRV_PCM_STREAM_LAST + 1];
	bool int_lrck_inv[SNDRV_PCM_STREAM_LAST + 1];
	bool int_bck_inv[SNDRV_PCM_STREAM_LAST + 1];
	unsigned int enable_seq[SNDRV_PCM_STREAM_LAST + 1];
};

struct mt8518_pcm_intf_data {
	bool slave_mode;
	bool lrck_inv;
	bool bck_inv;
	unsigned int format;
};

struct mt8518_multi_in_data {
	bool lrck_inv;
	bool bck_inv;
	unsigned int format;
	unsigned int period_update_bytes;
	unsigned int notify_irq_count;
	unsigned int current_irq_count;
};

struct mt8518_spdif_in_subdata {
	unsigned int rate;
	unsigned int user_code[SPDIF_USERCODE_NUM];
	unsigned int ch_status[SPDIF_CHSTS_NUM];
	unsigned int stream_type;
};

struct mt8518_spdif_in_data {
	unsigned int port;
	unsigned int ports_mux[SPDIF_IN_PORT_NUM];
	struct mt8518_spdif_in_subdata subdata;
};

struct mt8518_etdm_ctrl_reg {
	unsigned int con0;
	unsigned int con1;
	unsigned int con2;
	unsigned int con3;
	unsigned int con4;
};

struct mt8518_control_data {
	bool bypass_cm0;
	bool bypass_cm1;
	bool spdif_output_iec61937;
};

#define DMIC_MAX_CH (8)

struct mt8518_dmic_data {
	bool two_wire_mode;
	unsigned int clk_phase_sel_ch1;
	unsigned int clk_phase_sel_ch2;
	unsigned int dmic_src_sel[DMIC_MAX_CH];
	bool iir_on;
	unsigned int setup_time_us;
};

enum {
	MT8518_GASRC0 = 0,
	MT8518_GASRC1,
	MT8518_GASRC2,
	MT8518_GASRC3,
	MT8518_GASRC_NUM,
};

struct mt8518_gasrc_ctrl_reg {
	unsigned int con0;
	unsigned int con1;
	unsigned int con2;
	unsigned int con3;
	unsigned int con4;
	unsigned int con6;
	unsigned int con7;
	unsigned int con10;
	unsigned int con11;
	unsigned int con13;
	unsigned int con14;
};

struct mt8518_gasrc_data {
	unsigned int input_mux;
	unsigned int output_mux;
	bool cali_tx;
	bool cali_rx;
	bool one_heart;
	bool iir_on;
	bool duplex;
	bool op_freq_45m;
	unsigned int cali_cycles;
	bool re_enable[SNDRV_PCM_STREAM_LAST + 1];
	atomic_t ref_cnt;
};

enum mt8518_afe_gasrc_mux {
	MUX_GASRC_8CH = 0,
	MUX_GASRC_6CH,
	MUX_GASRC_4CH,
	MUX_GASRC_2CH,
};

struct mt8518_afe_gasrc_mux_map {
	int gasrc_id;
	int idx;
	int mux;
};

enum mt8518_afe_gasrc_lrck_sel_src {
	MT8518_AFE_GASRC_LRCK_SEL_ETDM_IN2 = 0,
	MT8518_AFE_GASRC_LRCK_SEL_ETDM_IN1,
	MT8518_AFE_GASRC_LRCK_SEL_ETDM_OUT2,
	MT8518_AFE_GASRC_LRCK_SEL_ETDM_OUT1,
	MT8518_AFE_GASRC_LRCK_SEL_PCM_IF,
	MT8518_AFE_GASRC_LRCK_SEL_UL_VIRTUAL,
};

enum {
	MT8518_APLL1_RATE = 180633600,
	MT8518_APLL2_RATE = 196608000,
};

enum {
	CLK_TUNE_SPDIF_IN = 0,
	CLK_TUNE_MULTI_IN,
	CLK_TUNE_SOURCE_NUM,
};

enum {
	TUNE_PHASE_INIT = 0,
	TUNE_PHASE_START_CALI_PROC,
	TUNE_PHASE_GET_CALI_RESULT,
	TUNE_PHASE_CHECK_OUTPUT_ACTIVE,
	TUNE_PHASE_ADJUST_APLL_RATE,
	TUNE_PHASE_ADJUST_DONE,
	TUNE_PHASE_ALL_DONE,
	TUNE_PHASE_ABORT,
};

enum {
	PBUF_SIZE_FULL = 0,
	PBUF_SIZE_HALF,
	PBUF_SIZE_QUARTER,
	PBUF_SIZE_EIGHTH,
	PBUF_SIZE_CONF_NUM,
};

enum {
	EN_IN_PREP_DIS_IN_SD = 0,
	EN_DIS_IN_TRIGGER,
};

struct ext_clk_tune_property {
	bool do_tune;
	unsigned int tune_id;
	unsigned int period_ms;
	unsigned int adj_step_ppm;
	unsigned int adj_step_us;
};

struct mt8518_afe_ext_clk_tune_data {
	struct ext_clk_tune_property props[CLK_TUNE_SOURCE_NUM];
	struct ext_clk_tune_property *working;
	struct delayed_work clk_tune_work;
	int working_phase;
	bool start;
	unsigned int asrc_id;
	unsigned int cali_cycles;
	unsigned int input_rate;
	u64 cali_retry_time;
	int cali_retry_cnt;
	u64 cali_input_rate_10uhz;
	unsigned long apll1_rate;
	unsigned long apll2_rate;
	unsigned long apll_target_rate;
	unsigned long apll_current_rate;
	unsigned long apll_restore_rate;
	unsigned long apll_step_hz;
	unsigned int outputs;
	unsigned int output_rate;
	void *apll_clk;
	void *afe;
};

enum {
	MT8518_AFE_TDMOUT_CONN_I0 = 0,
	MT8518_AFE_TDMOUT_CONN_I15 = 15,
	MT8518_AFE_TDMOUT_CONN_CFG_NUM = 16,
};

struct mt8518_afe_tdmout_conn_data {
	unsigned int out_cfg[MT8518_AFE_TDMOUT_CONN_CFG_NUM];
};

#ifdef CALI_RESULT_CHECK
struct mt8518_cali_result {
	struct hrtimer cali_hrt;
	int cnt;
	snd_pcm_sframes_t max;
	snd_pcm_sframes_t min;
	snd_pcm_sframes_t avg;
	snd_pcm_sframes_t cur;
	u64 sum;
	void *afe;
};
#endif

enum {
	AFE_PIN_STATE_DEFAULT = 0,
	AFE_PIN_STATE_ETDM1_OUT_ON,
	AFE_PIN_STATE_ETDM1_OUT_OFF,
	AFE_PIN_STATE_ETDM1_IN_ON,
	AFE_PIN_STATE_ETDM1_IN_OFF,
	AFE_PIN_STATE_ETDM2_OUT_ON,
	AFE_PIN_STATE_ETDM2_OUT_OFF,
	AFE_PIN_STATE_ETDM2_IN_ON,
	AFE_PIN_STATE_ETDM2_IN_OFF,
	AFE_PIN_STATE_MAX
};

struct mt8518_afe_private {
	struct clk *clocks[MT8518_CLK_NUM];
	struct mt8518_fe_dai_data fe_data[MT8518_AFE_MEMIF_NUM];
	struct mt8518_be_dai_data be_data[MT8518_AFE_BACKEND_NUM];
	struct mt8518_etdm_data etdm_data[MT8518_ETDM_SETS];
	struct mt8518_pcm_intf_data pcm_intf_data;
	struct mt8518_multi_in_data multi_in_data;
	struct mt8518_spdif_in_data spdif_in_data;
	struct mt8518_control_data ctrl_data;
	struct mt8518_dmic_data dmic_data;
	struct mt8518_afe_tdmout_conn_data tdmo_conn;
	struct mt8518_gasrc_data gasrc_data[MT8518_GASRC_NUM];
	struct mt8518_afe_ext_clk_tune_data clk_tune;
#ifdef CALI_RESULT_CHECK
	struct mt8518_cali_result cali_res;
#endif
	int afe_on_ref_cnt;
	int top_cg_ref_cnt[MT8518_TOP_CG_NUM];
	void __iomem *afe_sram_va;
	u32 afe_sram_pa;
	u32 afe_sram_size;
	bool use_bypass_afe_pinmux;
	u32 be_active_status;
	bool dl8_enable_24ch_output;
	u32 dl8_max_main_channels;
	/* locks */
	spinlock_t afe_ctrl_lock;
	spinlock_t spdifin_ctrl_lock;
	struct regmap *topckgen;
	struct regmap *scpsys;
	int block_dpidle_ref_cnt;
	struct mutex block_dpidle_mutex;
	struct snd_card *card;
	struct pinctrl *pinctrl;
	struct pinctrl_state *pin_states[AFE_PIN_STATE_MAX];
#ifdef CONFIG_DEBUG_FS
	struct dentry *debugfs_dentry[MT8518_AFE_DEBUGFS_NUM];
#endif
};

bool mt8518_afe_rate_supported(unsigned int rate, unsigned int id);
bool mt8518_afe_channel_supported(unsigned int channel, unsigned int id);

#endif
