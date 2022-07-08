#
# Copyright (C) 2017 MediaTek Inc.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#

#$(call all-subdir-src-or-makefile)
MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))
subdir-ccflags-y += -Werror

subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
subdir-ccflags-y += -I$(srctree)/drivers/mmc/host/mediatek/$(MTK_PLATFORM)
obj-$(CONFIG_MTK_BASE_POWER) += base/
obj-$(CONFIG_MTK_FREQ_HOPPING) += freqhopping/
obj-$(CONFIG_MTK_DEVAPC_DRIVER) += devapc/
obj-$(CONFIG_MTK_MEM) += mem/
obj-$(CONFIG_MTK_AUXADC)	+= auxadc/
obj-$(CONFIG_MTK_SYSENV)	+= sysenv/
obj-$(CONFIG_MTK_GIC_EXT) += ext_gic/
obj-$(CONFIG_USB_MTK_HDRC) += usb20/
obj-$(CONFIG_MTK_CHIP) += chip/
obj-y += devinfo/
obj-$(CONFIG_MTK_SECURITY_SW_SUPPORT) +=masp/
obj-$(CONFIG_MTK_IN_HOUSE_TEE_SUPPORT)	+= gcpu/
obj-$(CONFIG_MTK_IN_HOUSE_TEE_SUPPORT) += mtee/
obj-$(CONFIG_MTK_SECURE_EFUSE) += secure_efuse/
obj-$(CONFIG_MTK_SOC_THERMAL)   += thermal/
obj-$(CONFIG_USB)	+= usb_boost/
obj-$(CONFIG_MTK_SYS_CIRQ) += cirq/

obj-y += atf/

obj-$(CONFIG_MTK_DFD_INTERNAL_DUMP) += dfd/
obj-$(CONFIG_MTK_SYSTRACKER)   += systracker/

obj-y += spm/

#mem ASRC

obj-$(CONFIG_MTK_AUDIO_ASRC) += asrc/
obj-$(CONFIG_MTK_HWRNG) += hw_random/
