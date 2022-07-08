#include <linux/kernel.h>
#include <linux/delay.h>
#include  "./include/rtk_switch.h"
#include  "./include/vlan.h"
#include  "./include/port.h"
#include  "./include/rate.h"
#include  "./include/rtk_hal.h"
#include  "./include/l2.h"
#include  "./include/stat.h"
#include  "./include/igmp.h"
#include  "./include/rtl8367c_asicdrv_port.h"
#include  "./include/rtl8367c_asicdrv_mib.h"

extern rtk_int32 smi_read(rtk_uint32 mAddrs, rtk_uint32 *rData);
extern rtk_int32 smi_write(rtk_uint32 mAddrs, rtk_uint32 rData);


rtk_api_ret_t rtk_hal_switch_init(void)
{
    rtk_api_ret_t ret;
    unsigned int regValue;
    
    ret = rtk_switch_init();
    printk("rtk_switch_init ret = %d!!!!!!!!!!!!\n", ret);
    mdelay(500);

    regValue = 0;
    smi_write(0x13A0, 0x1234);
    mdelay(100);
    smi_read(0x13A0, &regValue);
    printk("rtk_switch reg = 0x%x !!!!!!!!!!!!\n", regValue);

	/*vlan init*/
	ret = rtk_vlan_init();
	printk("rtk_vlan_init ret = %d!!!!!!!!!!!!\n", ret);

    return ret;
}


int rtk_hal_set_ingress_rate(struct ra_switch_ioctl_data *data)	
{
    rtk_api_ret_t ret;

	if(data->on_off == 1)
		ret = rtk_rate_igrBandwidthCtrlRate_set(data->port, data->bw, 0, 1);
	else
	    ret = rtk_rate_igrBandwidthCtrlRate_set(data->port, 1048568, 0, 1);

	
	return ret;
}

int rtk_hal_set_egress_rate(struct ra_switch_ioctl_data *data)	
{
    rtk_api_ret_t ret;
	
	if(data->on_off == 1)
		ret = rtk_rate_egrBandwidthCtrlRate_set(data->port, data->bw, 1);
	else
	    ret = rtk_rate_egrBandwidthCtrlRate_set(data->port, 1048568, 1);

	
	return ret;
}

int rtk_hal_set_vlan(struct ra_switch_ioctl_data *data)
{
    rtk_vlan_cfg_t vlan;
	int i;

	
    /* clear vlan entry first */
	memset(&vlan, 0x00, sizeof(rtk_vlan_cfg_t));
	RTK_PORTMASK_CLEAR(vlan.mbr);
	RTK_PORTMASK_CLEAR(vlan.untag);
	rtk_vlan_set(data->vid, &vlan);

	memset(&vlan, 0x00, sizeof(rtk_vlan_cfg_t));
	for (i = 0; i < 8; i++) {			
			if (data->port_map & (1 << i)) {
					RTK_PORTMASK_PORT_SET(vlan.mbr, i);
					RTK_PORTMASK_PORT_SET(vlan.untag, i);
					rtk_vlan_portPvid_set(i, data->vid, 0);
			}			
	}
	rtk_vlan_set(data->vid, &vlan);
			
	return 0;
}

int rtk_hal_dump_vlan(void)
{
    rtk_vlan_cfg_t vlan;
	int i;

	printk("vid    fid    portmap\n");
	for (i = 0; i < RTK_SW_VID_RANGE; i++) {
		rtk_vlan_get(i, &vlan);
		printk("RTK_PORTMASK_IS_PORT_SET(vlan.mbr, 0): %d\n", RTK_PORTMASK_IS_PORT_SET(vlan.mbr, 0));
		printk("RTK_PORTMASK_IS_PORT_SET(vlan.mbr, 1): %d\n", RTK_PORTMASK_IS_PORT_SET(vlan.mbr, 1));
		printk("RTK_PORTMASK_IS_PORT_SET(vlan.mbr, 2): %d\n", RTK_PORTMASK_IS_PORT_SET(vlan.mbr, 2));
		printk("RTK_PORTMASK_IS_PORT_SET(vlan.mbr, 3): %d\n", RTK_PORTMASK_IS_PORT_SET(vlan.mbr, 3));
		printk("RTK_PORTMASK_IS_PORT_SET(vlan.mbr, 4): %d\n", RTK_PORTMASK_IS_PORT_SET(vlan.mbr, 4));
	}
	return 0;
}



void rtk_dump_unicast_table(void)
{
    rtk_uint32 address;
	rtk_l2_ucastAddr_t l2_data;
	rtk_api_ret_t retVal;

	printk("Unicast Table:\n");
	address = 0;
	printk("Port0:");
	while (1) {
		if ((retVal = rtk_l2_addr_next_get(READMETHOD_NEXT_L2UCSPA, UTP_PORT0, &address, &l2_data)) != RT_ERR_OK) {
			if (!address)
				printk("empty!\n");
			break;
			}
		printk("  %02x%02x%02x%02x%02x%02x", l2_data.mac.octet[0], l2_data.mac.octet[1], l2_data.mac.octet[2], l2_data.mac.octet[3], l2_data.mac.octet[4], l2_data.mac.octet[5]);
		address++;
	}
	printk("\n");

    address = 0;
	printk("Port1:");
	while (1) {
		if ((retVal = rtk_l2_addr_next_get(READMETHOD_NEXT_L2UCSPA, UTP_PORT1, &address, &l2_data)) != RT_ERR_OK) {
			if (!address)
				printk("empty!\n");
			break;
			}
		printk("  %02x%02x%02x%02x%02x%02x", l2_data.mac.octet[0], l2_data.mac.octet[1], l2_data.mac.octet[2], l2_data.mac.octet[3], l2_data.mac.octet[4], l2_data.mac.octet[5]);
		address++;
	}
	printk("\n");

    address = 0;
	printk("Port2:");
	while (1) {
		if ((retVal = rtk_l2_addr_next_get(READMETHOD_NEXT_L2UCSPA, UTP_PORT2, &address, &l2_data)) != RT_ERR_OK)  {
			if (!address)
				printk("empty!\n");
			break;
			}
		printk("  %02x%02x%02x%02x%02x%02x", l2_data.mac.octet[0], l2_data.mac.octet[1], l2_data.mac.octet[2], l2_data.mac.octet[3], l2_data.mac.octet[4], l2_data.mac.octet[5]);
		address++;
	}
	printk("\n");

    address = 0;
	printk("Port3:");
	while (1) {
		if ((retVal = rtk_l2_addr_next_get(READMETHOD_NEXT_L2UCSPA, UTP_PORT3, &address, &l2_data)) != RT_ERR_OK) {
			if (!address)
				printk("empty!\n");
			break;
			}
		printk("  %02x%02x%02x%02x%02x%02x", l2_data.mac.octet[0], l2_data.mac.octet[1], l2_data.mac.octet[2], l2_data.mac.octet[3], l2_data.mac.octet[4], l2_data.mac.octet[5]);
		address++;
	}
	printk("\n");

    address = 0;
	printk("Port4:");
	while (1) {
		if ((retVal = rtk_l2_addr_next_get(READMETHOD_NEXT_L2UCSPA, UTP_PORT4, &address, &l2_data)) != RT_ERR_OK) {
			if (!address)
				printk("empty!\n");
			break;
			}
		printk("  %02x%02x%02x%02x%02x%02x", l2_data.mac.octet[0], l2_data.mac.octet[1], l2_data.mac.octet[2], l2_data.mac.octet[3], l2_data.mac.octet[4], l2_data.mac.octet[5]);
		address++;
	}
	printk("\n");
	
}

#if 0
void rtk_dump_multicast_table(void)
{
/*TODO*/
}
#endif

int rtk_hal_dump_table(void)
{
    
	rtk_dump_unicast_table();
	//rtk_dump_multicast_table();	
    return 0;
}



void rtk_dump_mib_type(rtk_stat_port_type_t cntr_idx)
{
    rtk_port_t port;
	rtk_stat_counter_t Cntr;

    for (port = 0; port < 5; port++) {
		rtk_stat_port_get(port, cntr_idx, &Cntr);
		printk("%8llu", Cntr);
   	}
	printk("\n");
}

void rtk_hal_dump_mib(void)
{

	printk("  %8s %8s  %8s %8s %8s\n", "Port0", "Port1", "Port2", "Port3", "Port4");
	/* Get TX Unicast Pkts */
	printk("TX Unicast Pkts  :");
	rtk_dump_mib_type(STAT_IfOutUcastPkts);
	/* Get TX Multicast Pkts */
	printk("TX Multicast Pkts:");
	rtk_dump_mib_type(STAT_IfOutMulticastPkts);
	/* Get TX BroadCast Pkts */
	printk("TX BroadCast Pkts:");
	rtk_dump_mib_type(STAT_IfOutBroadcastPkts);
	/* Get TX Collisions */
	/* Get TX Puase Frames */
	printk("TX Pause Frames  :");
	rtk_dump_mib_type(STAT_Dot3OutPauseFrames);
	/* Get TX Drop Events */
	/* Get RX Unicast Pkts */
	printk("RX Unicast Pkts  :");
	rtk_dump_mib_type(STAT_IfInUcastPkts);
	/* Get RX Multicast Pkts */
	printk("RX Multicast Pkts:");
	rtk_dump_mib_type(STAT_IfInMulticastPkts);
	/* Get RX Broadcast Pkts */
	printk("RX Broadcast Pkts:");
	rtk_dump_mib_type(STAT_IfInBroadcastPkts);
	/* Get RX FCS Erros */
	printk("RX FCS Errors    :");
	rtk_dump_mib_type(STAT_Dot3StatsFCSErrors);
	/* Get RX Undersize Pkts */
	printk("RX Undersize Pkts:");
	rtk_dump_mib_type(STAT_EtherStatsUnderSizePkts);
	/* Get RX Fragments */
	printk("RX Fragments     :");
	rtk_dump_mib_type(STAT_EtherStatsFragments);
	/* Get RX Oversize Pkts */
	printk("RX Oversize Pkts :");
	rtk_dump_mib_type(STAT_EtherOversizeStats);
	/* Get RX Jabbers */
	printk("RX Jabbers       :");
	rtk_dump_mib_type(STAT_EtherStatsJabbers);
	/* Get RX Pause Frames */
	printk("RX Pause Frames  :");
	rtk_dump_mib_type(STAT_Dot3InPauseFrames);
	/* clear MIB */
	rtk_stat_global_reset();
}

void rtk_hal_enable_igmpsnoop(struct ra_switch_ioctl_data *data)
{
	rtk_portmask_t portmask;
	int i;

    /* igmpsnoop init */
	/* IGMPv1: HW process */
	/* IGMPv2: HW process */
	/* IGMPv1: Flooding without process */
	/* MLDv1: HW process */
	/* MLDv2: Flooding without process */
	/* Fast Leave: Enabled */
	/* Dynamic router port learnging: Disabled */
	rtk_igmp_init();

    /* enable or disable igmpsnoop */
	rtk_igmp_state_set(1);
	/* set router port */
	for (i = 0; i < 5; i++) {
		if (data->port_map & (1 << i))
				RTK_PORTMASK_PORT_SET(portmask, i);
	}
    rtk_igmp_static_router_port_set(&portmask);		
    /* set query interval */
	rtk_igmp_queryInterval_set(data->igmp_query_interval);
	/* set robustness */
	rtk_igmp_robustness_set(2);
}

void rtk_hal_disable_igmpsnoop(void)
{
    rtk_igmp_state_set(0);
}

void rtk_set_port_mirror(struct ra_switch_ioctl_data * data)
{
    rtk_port_t mirroring_port;
	rtk_portmask_t pMirrored_rx_portmask;
	rtk_portmask_t pMirrored_tx_portmask;


    rtk_mirror_portBased_set(mirroring_port, &pMirrored_rx_portmask, &pMirrored_tx_portmask);
}

