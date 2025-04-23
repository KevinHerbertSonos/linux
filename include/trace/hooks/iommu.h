/* SPDX-License-Identifier: GPL-2.0 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM iommu

#define TRACE_INCLUDE_PATH trace/hooks

#if !defined(_TRACE_HOOK_IOMMU_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_HOOK_IOMMU_H

#include <linux/types.h>

#include <linux/tracepoint.h>

#define trace_android_vh_iommu_setup_dma_ops(dev, dma_base, size)


#endif /* _TRACE_HOOK_IOMMU_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
