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
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "pmu.h"
#include "pcerrc.h"
#include "collector.h"
#include "pmu_plugin.h"
#include "plugin_comm.h"
#include "plugin_sampling.h"

static bool sampling_is_open = false;
static int sampling_pd = -1;
static struct DataHeader *sampling_buf = NULL;
struct PmuData *sampling_data = NULL;

static void sampling_init()
{
    sampling_buf = init_buf(CYCLES_SAMPLING_BUF_SIZE, PMU_CYCLES_SAMPLING);
}

static void sampling_fini()
{
    free_buf(sampling_buf);
}

static int sampling_open()
{
    struct PmuAttr attr;
    char *evtList[1];
    int pd;

    evtList[0] = "cycles";

    attr.evtList = evtList;
    attr.numEvt = 1;
    attr.pidList = NULL;
    attr.numPid = 0;
    attr.cpuList = NULL;
    attr.numCpu = 0;
    attr.freq = 100;
    attr.useFreq = 1;

    pd = PmuOpen(SAMPLING, &attr);
    if (pd == -1) {
        printf("%s\n", Perror());
        return pd;
    }

    sampling_is_open = true;
    return pd;
}

static void sampling_close()
{
    PmuClose(sampling_pd);
    sampling_is_open = false;
}

void sampling_enable()
{
    if (!sampling_buf) {
        sampling_init();
    }

    if (!sampling_is_open) {
        sampling_pd = sampling_open();
    }

    PmuEnable(sampling_pd);
}

void sampling_disable()
{
    PmuDisable(sampling_pd);
}

void *sampling_get_ring_buf()
{
    return sampling_buf;
}

void sampling_reflash_ring_buf()
{
    struct DataHeader *data_header;
    int len;

    data_header = (struct DataHeader *)sampling_buf;
    if (!data_header) {
        printf("sampling_buf has not malloc\n");
        return;
    }

    sampling_disable();
    len = PmuRead(sampling_pd, &sampling_data);
    sampling_enable();

    fill_buf(data_header, sampling_data, len);
}

char *sampling_get_name()
{
    return "collector_pmu_sampling";
}

int sampling_get_cycle()
{
    return 100;
}

char *sampling_get_version()
{
    return NULL;
}

char *sampling_get_description()
{
    return NULL;
}

char *sampling_get_type()
{
    return NULL;
}

char **sampling_get_dep(int *len)
{
    return NULL;
}
