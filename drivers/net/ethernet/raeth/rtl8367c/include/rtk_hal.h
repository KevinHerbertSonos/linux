#ifndef __RTK_HAL_H__
#define __RTK_HAL_H__
#include "./../../ra_ioctl.h"

#define RTK_SW_VID_RANGE        16
extern rtk_api_ret_t rtk_hal_switch_init(void);
extern int rtk_hal_set_ingress_rate(struct ra_switch_ioctl_data *data);
extern int rtk_hal_set_egress_rate(struct ra_switch_ioctl_data *data);
extern int rtk_hal_set_vlan(struct ra_switch_ioctl_data *data);
extern int rtk_hal_dump_vlan(void);
extern int rtk_hal_dump_table(void);
extern void rtk_hal_dump_mib(void);
extern void rtk_hal_enable_igmpsnoop(struct ra_switch_ioctl_data *data);
extern void rtk_hal_disable_igmpsnoop(void);
extern void rtk_set_port_mirror(struct ra_switch_ioctl_data *data);
#endif
