/*
* Copyright (C) 2011-2014 MediaTek Inc.
*
* This program is free software: you can redistribute it and/or modify it under the terms of the
* GNU General Public License version 2 as published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
* without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along with this program.
* If not, see <http://www.gnu.org/licenses/>.
*/

/*
 *  ! \file
 *  \brief  Declaration of library functions
 *  Any definitions in this file will be shared among GLUE Layer and internal Driver Stack.
 */

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/
#define WCN_THERMAL_STUB_DBG_LOG                  3
#define WCN_THERMAL_STUB_INFO_LOG                 2
#define WCN_THERMAL_STUB_WARN_LOG                 1
#define WCN_THERMAL_STUB_ERROR_LOG                0

int gWCNThermalStubLogLevel = WCN_THERMAL_STUB_ERROR_LOG;

#define WCN_THERMAL_STUB_LOG_INFO(fmt, arg...) \
do { \
	if (gWCNThermalStubLogLevel >= WCN_THERMAL_STUB_INFO_LOG) \
		pr_warn(fmt, ##arg); \
} while (0)
#define WCN_THERMAL_STUB_LOG_WARN(fmt, arg...) \
do { \
	if (gWCNThermalStubLogLevel >= WCN_THERMAL_STUB_WARN_LOG) \
		pr_warn(fmt, ##arg); \
} while (0)
#define WCN_THERMAL_STUB_LOG_DBG(fmt, arg...) \
do { \
	if (gWCNThermalStubLogLevel >= WCN_THERMAL_STUB_DBG_LOG) \
		pr_debug(fmt, ##arg); \
} while (0)

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/
#include <linux/kernel.h>
#include <mtk_wcn_cmb_stub.h>


/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/

/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/

/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/

static wmt_thermal_query_cb cmb_stub_thermal_ctrl_cb;
/*******************************************************************************
*                  F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/


/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/
/*!
 * \brief A registration function for Wireless driver to register itself to WCN THERMAL-STUB.
 *
 * An MTK-WCN-THERMAL-STUB registration function provided to Wireless driver to register
 * itself and related callback functions when driver being loaded into kernel.
 *
 * \param p_stub_cb a pointer carrying CMB__STUB_CB information
 *
 * \retval 0 operation success
 * \retval -1 invalid parameters
 */
int mtk_wcn_cmb_stub_reg(P_CMB_STUB_CB p_stub_cb)
{
	if ((!p_stub_cb)
	    || (p_stub_cb->size != sizeof(CMB_STUB_CB))) {
		WCN_THERMAL_STUB_LOG_WARN("[wcn_thermal_stub] invalid p_stub_cb:0x%p size(%d)\n",
				  p_stub_cb, (p_stub_cb) ? p_stub_cb->size : 0);
		return -1;
	}
	WCN_THERMAL_STUB_LOG_DBG("[wcn_thermal_stub] registered, p_stub_cb:0x%p size(%d)\n",
		 p_stub_cb, p_stub_cb->size);

	cmb_stub_thermal_ctrl_cb = p_stub_cb->thermal_query_cb;

	return 0;
}
EXPORT_SYMBOL(mtk_wcn_cmb_stub_reg);
/*!
 * \brief A unregistration function for Wireless driver to unregister from WCN-THERMAL-STUB.
 *
 * An MTK-WCN-THERMAL-STUB unregistration function provided to Wireless driver to
 * unregister itself and clear callback function references.
 *
 * \retval 0 operation success
 */
int mtk_wcn_cmb_stub_unreg(void)
{
	cmb_stub_thermal_ctrl_cb = NULL;
	WCN_THERMAL_STUB_LOG_INFO("[wcn_thermal_stub] unregistered\n");	/* KERN_DEBUG */

	return 0;
}
EXPORT_SYMBOL(mtk_wcn_cmb_stub_unreg);


int mtk_wcn_cmb_stub_query_ctrl(void)
{
	signed long temp = 0;

	if (cmb_stub_thermal_ctrl_cb)
		temp = (*cmb_stub_thermal_ctrl_cb) ();
	else
		WCN_THERMAL_STUB_LOG_WARN("[wcn_thermal_stub] thermal_ctrl_cb null\n");

	return temp;
}
