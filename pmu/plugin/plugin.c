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
#include "interface.h"
#include "plugin_sampling.h"
#include "plugin_counting.h"
#include "plugin_uncore.h"
#include "plugin_spe.h"

#define INS_COLLECTOR_MAX 10

static struct Interface ins_collector[INS_COLLECTOR_MAX] = {0};

struct Interface sampling_collector = {
    .get_version = sampling_get_version,
    .get_description = sampling_get_description,
    .get_priority = sampling_get_priority,
    .get_type = sampling_get_type,
    .get_dep = sampling_get_dep,
    .get_name = sampling_get_name,
    .get_period = sampling_get_period,
    .enable = sampling_enable,
    .disable = sampling_disable,
    .get_ring_buf = sampling_get_ring_buf,
    .run = sampling_run,
};

struct Interface counting_collector = {
    .get_version = counting_get_version,
    .get_description = counting_get_description,
    .get_priority = counting_get_priority,
    .get_type = counting_get_type,
    .get_dep = counting_get_dep,
    .get_name = counting_get_name,
    .get_period = counting_get_period,
    .enable = counting_enable,
    .disable = counting_disable,
    .get_ring_buf = counting_get_ring_buf,
    .run = counting_run,
};

struct Interface uncore_collector = {
    .get_version = uncore_get_version,
    .get_description = uncore_get_description,
    .get_priority = uncore_get_priority,
    .get_type = uncore_get_type,
    .get_dep = uncore_get_dep,
    .get_name = uncore_get_name,
    .get_period = uncore_get_period,
    .enable = uncore_enable,
    .disable = uncore_disable,
    .get_ring_buf = uncore_get_ring_buf,
    .run = uncore_run,
};

struct Interface spe_collector = {
    .get_version = spe_get_version,
    .get_description = spe_get_description,
    .get_priority = spe_get_priority,
    .get_type = spe_get_type,
    .get_dep = spe_get_dep,
    .get_name = spe_get_name,
    .get_period = spe_get_period,
    .enable = spe_enable,
    .disable = spe_disable,
    .get_ring_buf = spe_get_ring_buf,
    .run = spe_run,
};

int get_instance(struct Interface **interface)
{
    int ins_count = 0;

    ins_collector[ins_count++] = sampling_collector;
    ins_collector[ins_count++] = counting_collector;
    ins_collector[ins_count++] = uncore_collector;
    ins_collector[ins_count++] = spe_collector;
    *interface = &ins_collector[0];

    return ins_count;
}
