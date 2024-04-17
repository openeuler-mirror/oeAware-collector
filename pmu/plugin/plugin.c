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
#include "collector.h"
#include "plugin_sampling.h"
#include "plugin_counting.h"
#include "plugin_uncore.h"
#include "plugin_spe.h"

#define INS_COLLECTOR_MAX 10

static struct CollectorInterface ins_collector[INS_COLLECTOR_MAX] = {0};

struct CollectorInterface sampling_collector = {
    .get_version = sampling_get_version,
    .get_description = sampling_get_description,
    .get_type = sampling_get_type,
    .get_dep = sampling_get_dep,
    .get_name = sampling_get_name,
    .get_cycle = sampling_get_cycle,
    .enable = sampling_enable,
    .disable = sampling_disable,
    .get_ring_buf = sampling_get_ring_buf,
    .reflash_ring_buf = sampling_reflash_ring_buf,
};

struct CollectorInterface counting_collector = {
    .get_version = counting_get_version,
    .get_description = counting_get_description,
    .get_type = counting_get_type,
    .get_dep = counting_get_dep,
    .get_name = counting_get_name,
    .get_cycle = counting_get_cycle,
    .enable = counting_enable,
    .disable = counting_disable,
    .get_ring_buf = counting_get_ring_buf,
    .reflash_ring_buf = counting_reflash_ring_buf,
};

struct CollectorInterface uncore_collector = {
    .get_version = uncore_get_version,
    .get_description = uncore_get_description,
    .get_type = uncore_get_type,
    .get_dep = uncore_get_dep,
    .get_name = uncore_get_name,
    .get_cycle = uncore_get_cycle,
    .enable = uncore_enable,
    .disable = uncore_disable,
    .get_ring_buf = uncore_get_ring_buf,
    .reflash_ring_buf = uncore_reflash_ring_buf,
};

struct CollectorInterface spe_collector = {
    .get_version = spe_get_version,
    .get_description = spe_get_description,
    .get_type = spe_get_type,
    .get_dep = spe_get_dep,
    .get_name = spe_get_name,
    .get_cycle = spe_get_cycle,
    .enable = spe_enable,
    .disable = spe_disable,
    .get_ring_buf = spe_get_ring_buf,
    .reflash_ring_buf = spe_reflash_ring_buf,
};

int get_instance(struct CollectorInterface **ins)
{
    int ins_count = 0;

    ins_collector[ins_count++] = sampling_collector;
    ins_collector[ins_count++] = counting_collector;
    ins_collector[ins_count++] = uncore_collector;
    ins_collector[ins_count++] = spe_collector;
    *ins = &ins_collector[0];

    return ins_count;
}
