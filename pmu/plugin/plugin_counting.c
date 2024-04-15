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
#include "plugin_counting.h"

static bool counting_is_open = false;
static int counting_pd = -1;
static struct DataHeader *counting_buf = NULL;
struct PmuData *counting_data = NULL;

static void counting_init()
{
    counting_buf = (struct DataHeader *)malloc(sizeof(struct DataHeader));
    if (!counting_buf) {
        printf("malloc counting_buf failed\n");
        return;
    }
}

static void counting_fini()
{
    if (!counting_buf) {
        return;
    }

    free(counting_buf);
    counting_buf = NULL;
}

static int counting_open()
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

    pd = PmuOpen(COUNTING, &attr);
    if (pd == -1) {
        printf("%s\n", Perror());
        return pd;
    }

    counting_is_open = true;
    return pd;
}

static void counting_close()
{
    PmuClose(counting_pd);
    counting_is_open = false;
}

void counting_enable()
{
    if (!counting_buf) {
        counting_init();
    }

    if (!counting_is_open) {
        counting_pd = counting_open();
    }

    PmuEnable(counting_pd);
}

void counting_disable()
{
    PmuDisable(counting_pd);
}

void *counting_get_ring_buf()
{
    return counting_buf;
}

void counting_reflash_ring_buf()
{
    struct DataHeader *data_header;
    struct cycles_counting_buf *buf;
    int len;

    data_header = (struct DataHeader *)counting_buf;
    if (!data_header) {
        printf("counting_buf has not malloc\n");
        return;
    }

    if (counting_data) {
        PmuDataFree(counting_data);
        counting_data = NULL;
    }

    counting_disable();
    len = PmuRead(counting_pd, &counting_data);
    counting_enable();

    data_header->len = len;
    data_header->type = PMU_CYCLES_COUNTING;
    data_header->data = counting_data;
}

char *counting_get_name()
{
    return "collector_pmu_counting";
}

int counting_get_cycle()
{
    return 100;
}
