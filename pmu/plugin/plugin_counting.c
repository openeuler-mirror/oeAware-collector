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
#include "plugin_counting.h"

static bool counting_is_open = false;
static int counting_pd = -1;
static struct DataHeader *counting_buf = NULL;
struct PmuData *counting_data = NULL;

static void counting_init()
{
    counting_buf = init_buf(CYCLES_COUNTING_BUF_SIZE, PMU_CYCLES_COUNTING);
}

static void counting_fini()
{
    free_buf(counting_buf);
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
    int len;

    data_header = (struct DataHeader *)counting_buf;
    if (!data_header) {
        printf("counting_buf has not malloc\n");
        return;
    }

    counting_disable();
    len = PmuRead(counting_pd, &counting_data);
    counting_enable();

    fill_buf(data_header, counting_data, len);
}

char *counting_get_name()
{
    return "collector_pmu_counting";
}

int counting_get_cycle()
{
    return 100;
}

char *counting_get_version()
{
    return NULL;
}

char *counting_get_description()
{
    return NULL;
}

char *counting_get_type()
{
    return NULL;
}

char **counting_get_dep(int *len)
{
    return NULL;
}
