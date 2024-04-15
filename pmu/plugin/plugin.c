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

struct CollectorInterface sampling_collector = {
    .get_name = sampling_get_name,
    .get_cycle = sampling_get_cycle,
    .enable = sampling_enable,
    .disable = sampling_disable,
    .get_ring_buf = sampling_get_ring_buf,
    .reflash_ring_buf = sampling_reflash_ring_buf,
};

struct CollectorInterface counting_collector = {
    .get_name = counting_get_name,
    .get_cycle = counting_get_cycle,
    .enable = counting_enable,
    .disable = counting_disable,
    .get_ring_buf = counting_get_ring_buf,
    .reflash_ring_buf = counting_reflash_ring_buf,
};

int get_instance(struct CollectorInterface **ins)
{
    int ins_count = 0;

    ins[ins_count++] = &sampling_collector;
    ins[ins_count++] = &counting_collector;

    return ins_count;
}
