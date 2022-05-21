ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include/
ifeq ($(CONFIG_SND_SOC_MT8570),y)
ccflags-y += \
    -I$(srctree)/sound/soc/mediatek/mt8570 \
    -I$(srctree)/drivers/misc/mediatek/hifi4dsp/hifi4dsp_audio_ipi/common/framework \
    -I$(srctree)/drivers/misc/mediatek/hifi4dsp/
endif
ifeq ($(CONFIG_MTK_AUDIO_ASRC),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/asrc \
             -I$(srctree)/drivers/misc/mediatek/asrc/common
endif
snd-soc-mt8518-pcm-objs := \
    mt8518-afe-pcm.o mt8518-afe-utils.o mt8518-snd-utils.o \
    mt8518-afe-controls.o mt8518-afe-debug.o mt8518-afe-procfs.o

# MTK Platform Support
obj-$(CONFIG_SND_SOC_MT8518) += snd-soc-mt8518-pcm.o
# Machine support
obj-$(CONFIG_SND_SOC_MT8518_EVB) += mt8518-evb.o
obj-$(CONFIG_SND_SOC_MT8518A_P1) += mt8518-evb.o
obj-$(CONFIG_SND_SOC_MT8518B_P1) += mt8518-evb.o
obj-$(CONFIG_SND_SOC_MT8518S_P2) += mt8518-evb.o
obj-$(CONFIG_SND_SOC_MT8518S_BRAVO) += mt8518-evb.o
