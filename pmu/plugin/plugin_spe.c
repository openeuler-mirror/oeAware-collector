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
#include "plugin_spe.h"

static bool spe_is_open = false;
static int spe_pd = -1;
static struct DataHeader *spe_buf = NULL;
struct PmuData *spe_data = NULL;

static void spe_init()
{
    spe_buf = (struct DataHeader *)malloc(sizeof(struct DataHeader));
    if (!spe_buf) {
        printf("malloc spe_buf failed\n");
        return;
    }
}

static void spe_fini()
{
    if (!spe_buf) {
        return;
    }

    free(spe_buf);
    spe_buf = NULL;
}

static int spe_open()
{
    struct PmuAttr attr;
    int pd;

    attr.evtList = NULL;
    attr.numEvt = 0;
    attr.pidList = NULL;
    attr.numPid = 0;
    attr.cpuList = NULL;
    attr.numCpu = 0;
    attr.period = 2048;
    attr.dataFilter = SPE_DATA_ALL;
    attr.evFilter = SPE_EVENT_RETIRED;
    attr.minLatency = 0x60;

    pd = PmuOpen(SPE_SAMPLING, &attr);
    if (pd == -1) {
        printf("%s\n", Perror());
        return pd;
    }

    spe_is_open = true;
    return pd;
}

static void spe_close()
{
    PmuClose(spe_pd);
    spe_is_open = false;
}

void spe_enable()
{
    if (!spe_buf) {
        spe_init();
    }

    if (!spe_is_open) {
        spe_pd = spe_open();
    }

    PmuEnable(spe_pd);
}

void spe_disable()
{
    PmuDisable(spe_pd);
}

void *spe_get_ring_buf()
{
    return spe_buf;
}

void spe_reflash_ring_buf()
{
    struct DataHeader *data_header;
    int len;

    data_header = (struct DataHeader *)spe_buf;
    if (!data_header) {
        printf("spe_buf has not malloc\n");
        return;
    }

    if (spe_data) {
        PmuDataFree(spe_data);
        spe_data = NULL;
    }

    // while using PMU_SPE, PmuRead internally calls PmuEnable and PmuDisable
    len = PmuRead(spe_pd, &spe_data);

    data_header->len = len;
    data_header->type = PMU_SPE;
    data_header->data = spe_data;
}

char *spe_get_name()
{
    return "collector_spe";
}

int spe_get_cycle()
{
    return 100;
}
