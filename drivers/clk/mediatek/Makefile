ubdir-ccflags-y += -Werror

ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/

obj-$(CONFIG_COMMON_CLK_MEDIATEK) += clk-mtk.o clk-pll.o clk-gate.o clk-mux.o clk-apmixed.o
obj-$(CONFIG_RESET_CONTROLLER) += reset.o
obj-$(CONFIG_COMMON_CLK_MT8135) += clk-mt8135.o
obj-$(CONFIG_COMMON_CLK_MT8173) += clk-mt8173.o

obj-$(CONFIG_COMMON_CLK_MT8518) += clk-mt8518.o clkchk-mt8518.o
obj-$(CONFIG_COMMON_CLK_MT8518_MMSYS) += clk-mt8518-mm.o
obj-y += clk-bringup.o

ifeq ($(CONFIG_DEBUG_FS), y)
obj-$(CONFIG_COMMON_CLK_MEDIATEK) += clkdbg.o
obj-$(CONFIG_COMMON_CLK_MT8518) += clkdbg-mt8518.o
endif
