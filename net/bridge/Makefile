#
# Makefile for the IEEE 802.1d ethernet bridging layer.
#

obj-$(CONFIG_BRIDGE) += bridge.o

bridge-y	:= br.o br_device.o br_fdb.o br_forward.o br_if.o br_input.o \
			br_ioctl.o br_notify.o br_stp.o br_stp_bpdu.o \
			br_stp_if.o br_stp_timer.o br_netlink.o

# If the CONFIG_SONOS flag is active, include Sonos-specific files from
# the sonos subdirectory.
ccflags-$(CONFIG_SONOS) := -I$(src)/sonos \
			-Werror
bridge-$(CONFIG_SONOS)  += sonos/br_direct.o sonos/br_fdb_sonos.o \
			sonos/br_forward_sonos.o sonos/br_mcast.o \
			sonos/br_priority.o sonos/br_proxy.o \
			sonos/br_stp_sonos.o sonos/br_sonos.o \
			sonos/br_tunnel.o sonos/br_uplink.o

bridge-$(CONFIG_SYSFS) += br_sysfs_if.o br_sysfs_br.o

bridge-$(CONFIG_BRIDGE_NETFILTER) += br_netfilter.o

bridge-$(CONFIG_BRIDGE_IGMP_SNOOPING) += br_multicast.o br_mdb.o

bridge-$(CONFIG_BRIDGE_VLAN_FILTERING) += br_vlan.o

obj-$(CONFIG_BRIDGE_NF_EBTABLES) += netfilter/
