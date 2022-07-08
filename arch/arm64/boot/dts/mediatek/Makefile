ifeq ($(CONFIG_SONOS),y)
dtb-$(CONFIG_MACH_MT8518) += bravo_v4.dtb bravo_v3.dtb bravo_v2.dtb bravo_v1.dtb bravo_v0.dtb


else
dtb-$(CONFIG_ARCH_MEDIATEK) += mt6755-evb.dtb
dtb-$(CONFIG_ARCH_MEDIATEK) += mt6795-evb.dtb
dtb-$(CONFIG_ARCH_MEDIATEK) += mt8173-evb.dtb
dtb-$(CONFIG_MACH_MT8518) += aud8518sp2-emmc-uboot.dtb
endif

always		:= $(dtb-y)
subdir-y	:= $(dts-dirs)
clean-files	:= *.dtb
