#
# Makefile for the Linux Bluetooth HCI device drivers.
#
export BACKPORT_DIR = $(srctree)/backports/
export backport_srctree = $(BACKPORT_DIR)
NOSTDINC_FLAGS := -I$(BACKPORT_DIR)/include/ -include $(BACKPORT_DIR)/include/backport/backport.h $(NOSTDINC_FLAGS)

obj-$(CONFIG_BT_HCIUART)	+= hci_uart.o

hci_uart-y				:= hci_ldisc.o
hci_uart-$(CONFIG_BT_HCIUART_H4)	+= hci_h4.o
hci_uart-$(CONFIG_BT_HCIUART_BCSP)	+= hci_bcsp.o
hci_uart-$(CONFIG_BT_HCIUART_3WIRE)	+= hci_h5.o
hci_uart-objs				:= $(hci_uart-y)

ccflags-y += -D__CHECK_ENDIAN__
