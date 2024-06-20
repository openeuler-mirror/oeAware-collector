/******************************************************************************
 * Copyright (c) 2024 Huawei Technologies Co., Ltd. All rights reserved.
 * oeAware is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 ******************************************************************************/
#ifndef __PMU_PLUGIN_H__
#define __PMU_PLUGIN_H__

#ifdef __cplusplus
extern "C" {
#endif

#define PMU_CYCLES_COUNTING "pmu_cycles_counting"
#define PMU_CYCLES_SAMPLING "pmu_cycles_sampling"
#define PMU_UNCORE "pmu_uncore_counting"
#define PMU_SPE "pmu_spe_sampling"
#define PMU_NETIF_RX "pmu_netif_rx_counting"

#ifdef __cplusplus
}
#endif

#endif
