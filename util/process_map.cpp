/******************************************************************************
 * Copyright (c) Huawei Technologies Co., Ltd. 2024. All rights reserved.
 * gala-gopher licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
 * PURPOSE.
 * See the Mulan PSL v2 for more details.
 * Author: Mr.Wang
 * Create: 2024-04-03
 * Description: Get process and thread information.
 ******************************************************************************/
#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <cstring>
#include <dirent.h>
#include <ctype.h>
#include <memory>
#include "common.h"
#include "process_map.h"

using namespace std;
constexpr int COMM_SIZE = 128;
constexpr int PATH_LEN = 1024;
unsigned int GetNumPid()
{
    DIR *directory = opendir("/proc");
    struct dirent *entry;
    unsigned int count = 0;
    if (directory == nullptr) {
        perror("Error opening /proc directory");
        return -1;
    }

    // Count the number of process directories (pidList)
    while ((entry = readdir(directory))) {
        // Check if the entry is a directory and represents a process ID
        if (entry->d_type == DT_DIR && atoi(entry->d_name) != 0) {
            count++;
        }
    }
    closedir(directory);
    return count;
}

int *GetAllPids(unsigned int *count)
{
    DIR *directory;
    struct dirent *entry;
    int *pidList = nullptr;
    directory = opendir("/proc");

    *count = GetNumPid();

    // Allocate memory for storing pidList
    if ((*count) < SIZE_MAX / sizeof(int)) {
        pidList = static_cast<int *>(malloc((*count) * sizeof(int)));
    }
    if (pidList == nullptr) {
        perror("Memory allocation error");
        closedir(directory);
        return nullptr;
    }

    int index = 0;
    while ((entry = readdir(directory))) {
        if (entry->d_type == DT_DIR && atoi(entry->d_name) != 0) {
            pidList[index++] = atoi(entry->d_name);
        }
    }

    closedir(directory);
    return pidList;
}

void FreeProcTopo(struct ProcTopology *procTopo)
{
    if (procTopo == nullptr) {
        return;
    }
    if (procTopo->childPid != nullptr) {
        free(procTopo->childPid);
        procTopo->childPid = nullptr;
    }
    if (procTopo->comm != nullptr) {
        free(procTopo->comm);
        procTopo->comm = nullptr;
    }
    if (procTopo->exe != nullptr) {
        free(procTopo->exe);
        procTopo->exe = nullptr;
    }
    delete procTopo;
}

static int GetTgid(pid_t pid)
{
    if (pid == -1) {
        // for system sampling.
        return -1;
    }
    // Get tgid from /proc/<pid>/status.
    std::string filePath = "/proc/" + std::to_string(pid) + "/status";
    std::string realPath = GetRealPath(filePath);
    if (!IsValidPath(realPath)) {
        return -1;
    }
    std::ifstream statusFile(realPath);
    if (!statusFile.is_open()) {
        return -1;
    }
    string token;
    bool foundTgid = false;
    while (!statusFile.eof()) {
        if (!statusFile.is_open()) {
            return -1;
        }
        statusFile >> token;
        if (token == "Tgid:") {
            foundTgid = true;
            continue;
        }
        if (foundTgid) {
            return stoi(token);
        }
    }
    return -1;
}

static char *GetComm(pid_t pid)
{
    std::string commName;
    if (pid == -1) {
        commName = "system";
        char *comm = static_cast<char *>(malloc(commName.length() + 1));
        if (comm == nullptr) {
            return nullptr;
        }
        strcpy(comm, commName.c_str());
        return comm;
    }
    std::string filePath = "/proc/" + std::to_string(pid) + "/comm";
    std::string realPath = GetRealPath(filePath);
    if (!IsValidPath(realPath)) {
        return nullptr;
    }
    std::ifstream commFile(realPath);
    if (!commFile.is_open()) {
        return nullptr;
    }
    commFile >> commName;
    char *comm = static_cast<char *>(malloc(commName.length() + 1));
    if (comm == nullptr) {
        return nullptr;
    }
    strcpy(comm, commName.c_str());
    return comm;
}

struct ProcTopology *GetProcTopology(pid_t pid)
{
    unique_ptr<ProcTopology, void (*)(ProcTopology *)> procTopo(new ProcTopology{0}, FreeProcTopo);
    procTopo->tid = pid;
    try {
        // Get tgid, i.e., process id.
        procTopo->pid = GetTgid(pid);
        if (pid != -1 && procTopo->pid == -1) {
            return nullptr;
        }
        // Get command name.
        procTopo->comm = GetComm(procTopo->pid);
        if (procTopo->comm == nullptr) {
            return nullptr;
        }
    } catch (exception&) {
        return nullptr;
    }

    return procTopo.release();
}

// Check if a string represents a valid integer
int IsValidInt(const char *str)
{
    while (*str) {
        if (!isdigit(*str)) {
            return 0;
        }
        str++;
    }
    return 1;
}

void StoreThreadId(int** childTidList, int* count, const char* entryName)
{
    (*count)++;
    int* newChildTidList = new int[(*count)];
    if (newChildTidList != nullptr) {
        if (*childTidList != nullptr) {
            std::copy(*childTidList, *childTidList + (*count) - 1, newChildTidList);
            delete[] *childTidList;
        }
        newChildTidList[(*count) - 1] = atoi(entryName);
        *childTidList = newChildTidList;
    }
}

bool GetChildTidRecursive(const char *dirPath, int **childTidList, int *count)
{
    DIR *dir = opendir(dirPath);
    if (!dir) {
        return false;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_type == DT_DIR) {
            // Skip "." and ".." directories
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }

            // Check if the entry name is a valid thread ID
            if (IsValidInt(entry->d_name)) {
                // Store the thread ID in the array
                StoreThreadId(childTidList, count, entry->d_name);
            }

            char path[PATH_LEN];
            if (snprintf(path, sizeof(path), "%s/%s", dirPath, entry->d_name) < 0) {
                continue;
            }

            // Continue recursively
            GetChildTidRecursive(path, childTidList, count);
        }
    }

    closedir(dir);
    return true;
}

int *GetChildTid(int pid, int *numChild)
{
    int *childTidList = nullptr;
    char dirPath[PATH_LEN];
    if (snprintf(dirPath, sizeof(dirPath), "/proc/%d/task", pid) < 0) {
        return nullptr;
    }
    *numChild = 0;
    if (!GetChildTidRecursive(dirPath, &childTidList, numChild)) {
        if (childTidList) {
            delete[] childTidList;
            childTidList = nullptr;
        }
        *numChild = 0;
    }
    return childTidList;
}

