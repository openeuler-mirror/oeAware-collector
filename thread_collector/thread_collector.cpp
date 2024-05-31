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
#include "interface.h"
#include "thread_info.h"
#include <fstream>
#include <string>
#include <vector>
#include <dirent.h>

const std::string PATH = "/proc";
char thread_name[] = "thread_collector";
const int CYCLE_SIZE = 500;
const std::string STATUS_NAME = "Name:\t";
const int STATUS_NAME_LENGTH = 6;
static DataRingBuf ring_buf;
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
            status_file.close();
        } 
        closedir(task_dir);
        
    }
    closedir(proc_dir);
    return num;
}

const char* get_name() {
    return thread_name;
}

const char* get_version() {
    return nullptr;
}

const char* get_description() {
    return nullptr;
}

int get_period() {
    return CYCLE_SIZE;
}
int get_priority() {
    return 0;
}
bool enable() {
    ring_buf.count = 0;
    ring_buf.index = -1;
    ring_buf.buf_len = 1;
    ring_buf.buf = &data_buf; 
    return true;
}

void disable() {
    
}

const DataRingBuf* get_ring_buf() {
    return &ring_buf;
}

void run(const Param *param) {
    (void)param;
    ring_buf.count++;
    int index = (ring_buf.index + 1) % ring_buf.buf_len;
    int num = get_all_threads();
    data_buf.len = num;
    data_buf.data = threads.data();
    ring_buf.buf[index] = data_buf;
    ring_buf.index = index;
}

struct Interface thread_collect = {
    .get_version = get_version,
    .get_name = get_name,
    .get_description = get_description,
    .get_dep = nullptr,
    .get_priority = get_priority,
    .get_type = nullptr,
    .get_period = get_period,
    .enable = enable,
    .disable = disable,
    .get_ring_buf = get_ring_buf,
    .run = run,
};

extern "C" int get_instance(Interface **ins) {
    *ins = &thread_collect;
    return 1;
}
