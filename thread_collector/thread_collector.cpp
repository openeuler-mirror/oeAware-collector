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
#include "thread_info.h"
#include <cstdio>
#include <fstream>
#include <string>
#include <cstring>
#include <cstdlib>
#include <vector>
#include <dirent.h>

const std::string PATH = "/proc";
char thread_name[] = "thread_collector";
const int CYCLE_SIZE = 100;
const std::string STATUS_NAME = "Name:\t";
const int STATUS_NAME_LENGTH = 6;
static DataHeader data_header;
static DataBuf data_buf;
static std::vector<ThreadInfo> threads(THREAD_NUM);

static int get_all_threads() {
    DIR *proc_dir = opendir(PATH.c_str());
    if (proc_dir == nullptr) {
        return 0;
    }
    struct dirent *entry;
    int num = 0;
    while ((entry = readdir(proc_dir)) != nullptr) {
        if (!isdigit(entry->d_name[0])) {
            continue;
        }
        int pid = atoi(entry->d_name);
        DIR *task_dir = opendir(("/proc/" + std::to_string(pid) + "/task").c_str());
        if (task_dir == nullptr) {
            continue;
        }
        struct dirent *task_entry;
        while ((task_entry = readdir(task_dir)) != nullptr) {
            if (!isdigit(task_entry->d_name[0])) {
                continue;
            }
            int tid = atoi(task_entry->d_name);
            std::ifstream status_file("/proc/" + std::to_string(pid) + "/task/" + std::to_string(tid) + "/status");
            if (!status_file.is_open()) {
                continue;
            }
            std::string line;
            std::string name;
            while (getline(status_file, line)) {
                if (line.substr(0, STATUS_NAME_LENGTH) == STATUS_NAME) {
                    name = line.substr(STATUS_NAME_LENGTH);
                    break;
                }
            }
            if (num < THREAD_NUM) {
                threads[num++] = ThreadInfo{pid, tid, name};
            }
        }
        closedir(task_dir);
        
    }
    closedir(proc_dir);
    return num;
}

char* get_name() {
    return thread_name;
}

char* get_version() {
    return nullptr;
}

char* get_description() {
    return nullptr;
}

char* get_type() {
    return nullptr;
}

int get_cycle() {
    return CYCLE_SIZE;
}

void enable() {
    data_header.buf_len = 1;
    data_header.buf = &data_buf; 
}

void disable() {
    
}

void* get_ring_buf() {
    return (void*)&data_header;
}

void reflash_ring_buf() {
    data_header.index++;
    data_header.count++;
    int index = data_header.count % data_header.buf_len;
    int num = get_all_threads();
    data_buf.len = num;
    data_buf.data = threads.data();
    data_header.buf[index] = data_buf;
}

struct CollectorInterface thread_collect = {
    .get_version = get_version,
    .get_name = get_name,
    .get_description = get_description,
    .get_type = get_type,
    .get_cycle = get_cycle,
    .get_dep = nullptr,
    .enable = enable,
    .disable = disable,
    .get_ring_buf = get_ring_buf,
    .reflash_ring_buf = reflash_ring_buf,
};

extern "C" int get_instance(CollectorInterface **ins) {
    *ins = &thread_collect;
    return 1;
}
