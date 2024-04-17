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
#ifndef __COLLECTOR_H__
#define __COLLECTOR_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DATA_HEADER_TYPE_SIZE 64

struct CollectorInterface {
    char* (*get_version)();
    char* (*get_name)();
    char* (*get_description)();
    char* (*get_type)();
    int (*get_cycle)();
    char** (*get_dep)(int *len);
    void (*enable)();
    void (*disable)();
    void* (*get_ring_buf)();
    void (*reflash_ring_buf)();
};

struct DataBuf {
    int len;
    void *data;
};

struct DataHeader {
    char type[DATA_HEADER_TYPE_SIZE];              // collector type
    int index;                                     // buf write index, initial value is -1
    uint64_t count;                                // collector times
    struct DataBuf *buf;
    int buf_len;
};

int get_instance(struct CollectorInterface **ins);

#ifdef __cplusplus
}
#endif

#endif
