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
#include <string.h>
#include "pmu.h"
#include "collector.h"

struct DataHeader *init_buf(int buf_len, const char *type)
{
    struct DataHeader *data_header;
    int ret;

    data_header = (struct DataHeader *)malloc(sizeof(struct DataHeader));
    if (!data_header) {
        printf("malloc data_header failed\n");
        return NULL;
    }

    (void)memset_s(data_header, sizeof(struct DataHeader), 0, sizeof(struct DataHeader));
    
    ret = strcpy_s(data_header->type, DATA_HEADER_TYPE_SIZE, type);
    if (ret != 0) {
        printf("strcpy_s data_header type failed\n");
        free(data_header);
        data_header = NULL;
        return NULL;
    }

    data_header->index = -1;

    data_header->buf = (struct DataBuf *)malloc(sizeof(struct DataBuf) * buf_len);
    if (!data_header->buf) {
        printf("malloc data_header buf failed\n");
        free(data_header);
        data_header = NULL;
        return NULL;
    }

    (void)memset_s(data_header->buf, sizeof(struct DataBuf) * buf_len, 0, sizeof(struct DataBuf) * buf_len);
    data_header->buf_len = buf_len;

    return data_header;
}

void free_buf(struct DataHeader *data_header)
{
    if (!data_header) {
        return;
    }

    if (!data_header->buf) {
        goto out;
    }

    free(data_header->buf);
    data_header->buf = NULL;

out:
    free(data_header);
    data_header = NULL;
}

void fill_buf(struct DataHeader *data_header, struct PmuData *pmu_data, int len)
{
    struct DataBuf *buf;
    int index;

    index = (data_header->index + 1) % data_header->buf_len;
    data_header->index = index;
    data_header->count++;
    buf = &data_header->buf[index];

    if (buf->data != NULL) {
        PmuDataFree(buf->data);
        buf->data = NULL;
        buf->len = 0;
    }

    buf->len = len;
    buf->data = (void *)pmu_data;
}
