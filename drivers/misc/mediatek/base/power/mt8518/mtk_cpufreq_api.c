/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */
#include <linux/cpufreq.h>
#include "mt8518-cpufreq.h"

int _search_available_freq_idx(struct mtk_cpu_dvfs *p, unsigned int target_khz,
				      unsigned int relation)
{
	int new_opp_idx = -1;
	int i;

	if (relation == CPUFREQ_RELATION_L) {
		for (i = 0; i < (signed int)(p->nr_opp_tbl); i++) {
			if (cpu_dvfs_get_freq_by_idx(p, i) >= target_khz) {
				new_opp_idx = i;
				break;
			}
		}
	} else {		/* CPUFREQ_RELATION_H */
		for (i = (signed int)(p->nr_opp_tbl - 1); i >= 0; i--) {
			if (cpu_dvfs_get_freq_by_idx(p, i) <= target_khz) {
				new_opp_idx = i;
				break;
			}
		}
	}

	return new_opp_idx;
}

unsigned int mt_cpufreq_find_close_freq(unsigned int cluster_id,
	unsigned int freq)
{
	enum mt_cpu_dvfs_id id = (enum mt_cpu_dvfs_id) cluster_id;
	struct mtk_cpu_dvfs *p = id_to_cpu_dvfs(id);
	int idx = _search_available_freq_idx(p, freq, CPUFREQ_RELATION_L);
	unsigned int temp;

	if (idx < 0)
		idx = 0;

	temp = cpu_dvfs_get_freq_by_idx(p, idx);

	return temp;
}

unsigned int mt_cpufreq_get_cur_freq(enum mt_cpu_dvfs_id id)
{
#ifdef CPU_DVFS_NOT_READY
	return 0;
#else

	struct mtk_cpu_dvfs *p = id_to_cpu_dvfs(id);
	unsigned int tmp;

	tmp = cpu_dvfs_get_cur_freq(p);

	return tmp;
#endif
}
EXPORT_SYMBOL(mt_cpufreq_get_cur_freq);

unsigned int mt_cpufreq_get_freq_by_idx(enum mt_cpu_dvfs_id id, int idx)
{
#ifdef CPU_DVFS_NOT_READY
	return 0;
#else
	struct mtk_cpu_dvfs *p = id_to_cpu_dvfs(id);

	pr_debug(">> %s()\n", __func__);

	if (!cpu_dvfs_is_available(p)) {
		pr_debug("<< %s():%d\n", __func__, __LINE__);
		return 0;
	}

	pr_debug("<< %s():%d\n", __func__, __LINE__);

	return cpu_dvfs_get_freq_by_idx(p, idx);
#endif
}
EXPORT_SYMBOL(mt_cpufreq_get_freq_by_idx);

unsigned int mt_cpufreq_get_volt_by_idx(enum mt_cpu_dvfs_id id, int idx)
{
#ifdef CPU_DVFS_NOT_READY
	return 0;
#else
	struct mtk_cpu_dvfs *p = id_to_cpu_dvfs(id);
	unsigned int tmp;

	pr_debug(">> %s()\n", __func__);

	if (!cpu_dvfs_is_available(p)) {
		pr_debug("<< %s():%d\n", __func__, __LINE__);
		return 0;
	}

	pr_debug("<< %s():%d\n", __func__, __LINE__);

	 tmp = cpu_dvfs_get_volt_by_idx(p, idx);

	return tmp;
#endif
}
EXPORT_SYMBOL(mt_cpufreq_get_volt_by_idx);

unsigned int mt_cpufreq_get_cpu_level(void)
{
	unsigned int lv = _mt_cpufreq_get_cpu_level();

	return lv;
}
EXPORT_SYMBOL(mt_cpufreq_get_cpu_level);
