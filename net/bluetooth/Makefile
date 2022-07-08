#
# Makefile for the Linux Bluetooth subsystem.
#
export BACKPORT_DIR = $(srctree)/backports/
export backport_srctree = $(BACKPORT_DIR)
NOSTDINC_FLAGS := -I$(BACKPORT_DIR)/include/ -include $(BACKPORT_DIR)/include/backport/backport.h $(NOSTDINC_FLAGS)

obj-$(CONFIG_BT)	+= bluetooth.o

bluetooth-y := af_bluetooth.o hci_core.o hci_conn.o hci_event.o mgmt.o \
	hci_sock.o hci_sysfs.o l2cap_core.o l2cap_sock.o smp.o lib.o \
	ecc.o hci_request.o mgmt_util.o upstream-backports.o

bluetooth-$(CONFIG_BT_BREDR) += sco.o
bluetooth-$(CONFIG_BT_HS) += a2mp.o amp.o
bluetooth-$(CONFIG_BT_DEBUGFS) += hci_debugfs.o
bluetooth-$(CONFIG_BT_SELFTEST) += selftest.o

subdir-ccflags-y += -D__CHECK_ENDIAN__
