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
#ifndef __PLUGIN_SPE_H__
#define __PLUGIN_SPE_H__

#ifdef __cplusplus
extern "C" {
#endif

char *spe_get_version();
char *spe_get_description();
char *spe_get_type();
char **spe_get_dep(int *len);
void spe_enable();
void spe_disable();
void *spe_get_ring_buf();
void spe_reflash_ring_buf();
char *spe_get_name();
int spe_get_cycle();

#ifdef __cplusplus
}
#endif

#endif
