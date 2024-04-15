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

#ifdef __cplusplus
extern "C" {
#endif

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

struct DataHeader {
    int len;
    int type;
    void *data;
};

int get_instance(struct CollectorInterface **ins);

#ifdef __cplusplus
}
#endif

#endif
