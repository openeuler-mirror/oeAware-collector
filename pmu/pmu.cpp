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
 * Author: Mr.Zhang
 * Create: 2024-04-03
 * Description: implementations for managing performance monitoring tasks, collecting data,
 * and handling performance counters in the KUNPENG_PMU namespace
 ******************************************************************************/
#include <iostream>
#include <unistd.h>
#include <linux/perf_event.h>
#include "pfm.h"
#include "pfm_event.h"
#include "pmu_event.h"
#include "pmu_list.h"
#include "linked_list.h"
#include "pcerr.h"
#include "safe_handler.h"
#include "pmu.h"

using namespace pcerr;
using namespace KUNPENG_PMU;
using namespace std;

#define MAX_CPU_NUM sysconf(_SC_NPROCESSORS_ONLN)

static unordered_map<unsigned, bool> runningStatus;
static SafeHandler<unsigned> pdMutex;

struct PmuTaskAttr* AssignPmuTaskParam(PmuTaskType collectType, struct PmuAttr *attr);

static int PmuCollectStart(const int pd)
{
    return KUNPENG_PMU::PmuList::GetInstance()->Start(pd);
}

static int PmuCollectPause(const int pd)
{
    return KUNPENG_PMU::PmuList::GetInstance()->Pause(pd);
}

static int CheckCpuList(unsigned numCpu, int* cpuList)
{
    if (numCpu > MAX_CPU_NUM) {
        string errMsg = "Invalid numCpu: " + to_string(numCpu);
        New(LIBPERF_ERR_INVALID_CPULIST, errMsg);
        return LIBPERF_ERR_INVALID_CPULIST;
    }
    if (numCpu > 0 && cpuList == nullptr) {
        New(LIBPERF_ERR_INVALID_CPULIST);
        return LIBPERF_ERR_INVALID_CPULIST;
    }
    for (int i = 0; i < numCpu; i++) {
        if (cpuList[i] < 0 || cpuList[i] >= MAX_CPU_NUM) {
            string errMsg = "Invalid cpu id: " + to_string(cpuList[i]);
            New(LIBPERF_ERR_INVALID_CPULIST, errMsg);
            return LIBPERF_ERR_INVALID_CPULIST;
        }
    }
    return SUCCESS;
}

static int CheckPidList(unsigned numPid, int* pidList)
{
    if (numPid > 0 && pidList == nullptr) {
        New(LIBPERF_ERR_INVALID_PIDLIST);
        return LIBPERF_ERR_INVALID_PIDLIST;
    }
    for (int i = 0; i < numPid; i++) {
        if (pidList[i] < 0) {
            string errMsg = "Invalid pid: " + to_string(pidList[i]);
            New(LIBPERF_ERR_INVALID_PIDLIST, errMsg);
            return LIBPERF_ERR_INVALID_PIDLIST;
        }
    }
    return SUCCESS;
}

static int CheckEvtList(unsigned numEvt, char** evtList)
{
    if (numEvt > 0 && evtList == nullptr) {
        New(LIBPERF_ERR_INVALID_EVTLIST);
        return LIBPERF_ERR_INVALID_EVTLIST;
    }
    return SUCCESS;
}

static int CheckAttr(enum PmuTaskType collectType, struct PmuAttr *attr)
{
    auto err = CheckCpuList(attr->numCpu, attr->cpuList);
    if (err != SUCCESS) {
        return err;
    }
    err = CheckPidList(attr->numPid, attr->pidList);
    if (err != SUCCESS) {
        return err;
    }
    err = CheckEvtList(attr->numEvt, attr->evtList);
    if (err != SUCCESS) {
        return err;
    }
    if (collectType < 0 || collectType >= MAX_TASK_TYPE) {
        New(LIBPERF_ERR_INVALID_TASK_TYPE);
        return LIBPERF_ERR_INVALID_TASK_TYPE;
    }
    if ((collectType == SAMPLING || collectType == COUNTING) && attr->evtList == nullptr) {
        New(LIBPERF_ERR_INVALID_EVTLIST);
        return LIBPERF_ERR_INVALID_EVTLIST;
    }

    return SUCCESS;
}

static bool PdValid(const int &pd)
{
    return PmuList::GetInstance()->IsPdAlive(pd);
}

static void PmuTaskAttrFree(PmuTaskAttr *taskAttr)
{
    auto node = taskAttr;
    while (node) {
        delete[] node->pidList;
        delete[] node->cpuList;
        auto current = node;
        node = node->next;
        current->pmuEvt = nullptr;
        free(current);
    }
}

int PmuOpen(enum PmuTaskType collectType, struct PmuAttr *attr)
{
    try {
        auto err = CheckAttr(collectType, attr);
        if (err != SUCCESS) {
            return -1;
        }

        auto pTaskAttr = AssignPmuTaskParam(collectType, attr);
        if (pTaskAttr == nullptr) {
            return -1;
        }
        unique_ptr<PmuTaskAttr, void (*)(PmuTaskAttr*)> taskAttr(pTaskAttr, PmuTaskAttrFree);

        auto pd = KUNPENG_PMU::PmuList::GetInstance()->NewPd();
        if (pd == -1) {
            New(LIBPERF_ERR_NO_AVAIL_PD);
            return -1;
        }

        err = KUNPENG_PMU::PmuList::GetInstance()->Register(pd, taskAttr.get());
        if (err != SUCCESS) {
            PmuList::GetInstance()->Close(pd);
            pd = -1;
        }
        New(err);
        return pd;
    } catch (std::bad_alloc&) {
        New(COMMON_ERR_NOMEM);
        return -1;
    } catch (exception& ex) {
        New(UNKNOWN_ERROR, ex.what());
        return -1;
    }
}

static int DoCollectCounting(int pd, int milliseconds)
{
    constexpr int collectInterval = 100;
    constexpr int usecPerMilli = 1000;
    // Collect every <collectInterval> milliseconds,
    // and read data from ring buffer.
    int remained = milliseconds;
    bool unlimited = milliseconds == -1;
    PmuCollectStart(pd);
    while (remained > 0 || unlimited) {
        int interval = collectInterval;
        if (!unlimited && remained < collectInterval) {
            interval = remained;
        }
        usleep(usecPerMilli * interval);

        pdMutex.tryLock(pd);
        if (!runningStatus[pd]) {
            pdMutex.releaseLock(pd);
            break;
        }
        pdMutex.releaseLock(pd);

        remained -= interval;
    }
    PmuCollectPause(pd);
    // Read data from ring buffer and store data to somewhere.
    auto err = PmuList::GetInstance()->ReadDataToBuffer(pd);
    if (err != SUCCESS) {
        New(err);
        return err;
    }
    return SUCCESS;
}

static int DoCollectNonCounting(int pd, int milliseconds)
{
    constexpr int collectInterval = 100;
    constexpr int usecPerMilli = 1000;
    // Collect every <collectInterval> milliseconds,
    // and read data from ring buffer.
    int remained = milliseconds;
    bool unlimited = milliseconds == -1;
    while (remained > 0 || unlimited) {
        int interval = collectInterval;
        if (!unlimited && remained < collectInterval) {
            interval = remained;
        }

        PmuCollectStart(pd);
        usleep(usecPerMilli * interval);
        PmuCollectPause(pd);

        // Read data from ring buffer and store data to somewhere.
        auto err = PmuList::GetInstance()->ReadDataToBuffer(pd);
        if (err != SUCCESS) {
            New(err);
            return err;
        }

        // Check if all processes exit.
        if (PmuList::GetInstance()->AllPmuDead(pd)) {
            break;
        }
        pdMutex.tryLock(pd);
        if (!runningStatus[pd]) {
            pdMutex.releaseLock(pd);
            break;
        }
        pdMutex.releaseLock(pd);

        remained -= interval;
    }
    return SUCCESS;
}

static int DoCollect(int pd, int milliseconds)
{
    if (PmuList::GetInstance()->GetTaskType(pd) == COUNTING) {
        return DoCollectCounting(pd, milliseconds);
    }
    return DoCollectNonCounting(pd, milliseconds);
}

int PmuCollect(int pd, int milliseconds)
{
    int err = SUCCESS;
    string errMsg = "";
    try {
        if (!PdValid(pd)) {
            New(LIBPERF_ERR_INVALID_PD);
            return -1;
        }
        if (milliseconds != -1 && milliseconds < 0) {
            New(LIBPERF_ERR_INVALID_TIME);
            return -1;
        }

        pdMutex.tryLock(pd);
        runningStatus[pd] = true;
        pdMutex.releaseLock(pd);
        err = DoCollect(pd, milliseconds);
    } catch (std::bad_alloc&) {
        err = COMMON_ERR_NOMEM;
    } catch (exception& ex) {
        err = UNKNOWN_ERROR;
        errMsg = ex.what();
    }
    pdMutex.tryLock(pd);
    runningStatus[pd] = false;
    pdMutex.releaseLock(pd);
    if (!errMsg.empty()) {
        New(err, errMsg);
    } else {
        New(err);
    }
    if (err != SUCCESS) {
        return -1;
    }
    return err;
}

static int InnerCollect(int *pds, unsigned len, size_t collectTime, bool &stop)
{
    for (unsigned i = 0; i < len; ++i) {
        PmuCollectStart(pds[i]);
    }
    usleep(collectTime);
    for (unsigned i = 0; i < len; ++i) {
        PmuCollectPause(pds[i]);
    }

    for (unsigned i = 0; i < len; ++i) {
        // Read data from ring buffer and store data to somewhere.
        auto err = PmuList::GetInstance()->ReadDataToBuffer(pds[i]);
        if (err != SUCCESS) {
            return err;
        }
    }

    // Check if all processes exit.
    bool allDead = true;
    for (unsigned i = 0; i < len; ++i) {
        auto taskType = PmuList::GetInstance()->GetTaskType(pds[i]);
        if (taskType == COUNTING) {
            allDead = false;
            break;
        }
        if (!PmuList::GetInstance()->AllPmuDead(pds[i])) {
            allDead = false;
            break;
        }
    }
    if (allDead) {
        stop = true;
        return SUCCESS;
    }

    // Check if all processes are stopped.
    bool allStopped = true;
    for (unsigned i = 0; i < len; ++i) {
        pdMutex.tryLock(pds[i]);
        if (runningStatus[pds[i]]) {
            allStopped = false;
            pdMutex.releaseLock(pds[i]);
            break;
        }
        pdMutex.releaseLock(pds[i]);
    }
    if (allStopped) {
        stop = true;
    }

    return SUCCESS;
}

int PmuCollectV(int *pds, unsigned len, int milliseconds)
{
    constexpr int collectInterval = 100;
    constexpr int usecPerMilli = 1000;
    // Collect every <collectInterval> milliseconds,
    // and read data from ring buffer.
    int remained = milliseconds;
    bool unlimited = milliseconds == -1;
    for (int i = 0; i < len; ++i) {
        pdMutex.tryLock(pds[i]);
        runningStatus[pds[i]] = true;
        pdMutex.releaseLock(pds[i]);
    }
    while (remained > 0 || unlimited) {
        int interval = collectInterval;
        if (!unlimited && remained < collectInterval) {
            interval = remained;
        }
        bool stop = false;
        auto err = InnerCollect(pds, len, static_cast<size_t>(usecPerMilli * interval), stop);
        if (err != SUCCESS) {
            New(err);
            return err;
        }
        if (stop) {
            break;
        }
        remained -= interval;
    }
    return SUCCESS;
}

void PmuStop(int pd)
{
    if (!PdValid(pd)) {
        New(LIBPERF_ERR_INVALID_PD);
        return;
    }

    pdMutex.tryLock(pd);
    runningStatus[pd] = false;
    pdMutex.releaseLock(pd);
    New(SUCCESS);
}

int PmuRead(int pd, struct PmuData** pmuData)
{
    try {
        if (!PdValid(pd)) {
            New(LIBPERF_ERR_INVALID_PD);
            return LIBPERF_ERR_INVALID_PD;
        }

        auto& retData = KUNPENG_PMU::PmuList::GetInstance()->Read(pd);
        New(SUCCESS);
        if (!retData.empty()) {
            *pmuData = retData.data();
            return retData.size();
        } else {
            *pmuData = nullptr;
            return 0;
        }
    } catch (std::bad_alloc&) {
        New(COMMON_ERR_NOMEM);
        return -1;
    } catch (exception& ex) {
        New(UNKNOWN_ERROR, ex.what());
        return -1;
    }
}

void PmuClose(int pd)
{
    if (!PdValid(pd)) {
        New(LIBPERF_ERR_INVALID_PD);
        return;
    }
    try {
        KUNPENG_PMU::PmuList::GetInstance()->Close(pd);
        New(SUCCESS);
    } catch (std::bad_alloc&) {
        New(COMMON_ERR_NOMEM);
    } catch (exception& ex) {
        New(UNKNOWN_ERROR, ex.what());
    }
}

static struct PmuEvt* GetPmuEvent(const char* pmuName, int collectType)
{
    return PfmGetPmuEvent(pmuName, collectType);
}

static void PrepareCpuList(PmuAttr *attr, PmuTaskAttr *taskParam, PmuEvt* pmuEvt)
{
    if (pmuEvt->cpumask >= 0) {
        taskParam->numCpu = 1;
        taskParam->cpuList = new int[1];
        taskParam->cpuList[0] = pmuEvt->cpumask;
    } else if (attr->cpuList == nullptr && attr->pidList != nullptr && pmuEvt->collectType == COUNTING) {
        // For counting with pid list for system wide, open fd with cpu -1 and specific pid.
        taskParam->numCpu = 1;
        taskParam->cpuList = new int[taskParam->numCpu];
        taskParam->cpuList[0] = -1;
    } else if (attr->cpuList == nullptr) {
        // For null cpulist, open fd with cpu 0,1,2...max_cpu
        taskParam->numCpu = MAX_CPU_NUM;
        taskParam->cpuList = new int[taskParam->numCpu];
        for (int i = 0; i < taskParam->numCpu; i++) {
            taskParam->cpuList[i] = i;
        }
    } else {
        taskParam->numCpu = attr->numCpu;
        taskParam->cpuList = new int[attr->numCpu];
        for (int i = 0; i < attr->numCpu; i++) {
            taskParam->cpuList[i] = attr->cpuList[i];
        }
    }
}

static struct PmuTaskAttr* AssignTaskParam(PmuTaskType collectType, PmuAttr *attr, const char* name)
{
    unique_ptr<PmuTaskAttr, void (*)(PmuTaskAttr*)> taskParam(CreateNode<struct PmuTaskAttr>(), PmuTaskAttrFree);
    /**
     * Assign pids to collect
     */
    taskParam->numPid = attr->numPid;
    taskParam->pidList = new int[attr->numPid];
    for (int i = 0; i < attr->numPid; i++) {
        taskParam->pidList[i] = attr->pidList[i];
    }
    PmuEvt* pmuEvt = nullptr;
    if (collectType == SPE_SAMPLING) {
        pmuEvt = PfmGetSpeEvent(attr->dataFilter, attr->evFilter, attr->minLatency, collectType);
        if (pmuEvt == nullptr) {
            New(LIBPERF_ERR_SPE_UNAVAIL);
            return nullptr;
        }
    } else {
        pmuEvt = GetPmuEvent(name, collectType);
        if (pmuEvt == nullptr) {
            New(LIBPERF_ERR_INVALID_EVENT, "Invalid event: " + string(name));
            return nullptr;
        }
    }
    /**
     * Assign cpus to collect
     */
    PrepareCpuList(attr, taskParam.get(), pmuEvt);

    taskParam->pmuEvt = shared_ptr<PmuEvt>(pmuEvt, PmuEvtFree);
    taskParam->pmuEvt->useFreq = attr->useFreq;
    taskParam->pmuEvt->period = attr->period;
    return taskParam.release();
}

struct PmuTaskAttr* AssignPmuTaskParam(enum PmuTaskType collectType, struct PmuAttr *attr)
{
    struct PmuTaskAttr* taskParam = nullptr;
    if (collectType == SPE_SAMPLING) {
        // evtList is nullptr, cannot loop over evtList.
        taskParam = AssignTaskParam(collectType, attr, nullptr);
        return taskParam;
    }
    for (int i = 0; i < attr->numEvt; i++) {
        struct PmuTaskAttr* current = AssignTaskParam(collectType, attr, attr->evtList[i]);
        if (current == nullptr) {
            return nullptr;
        }
        AddTail(&taskParam, &current);
    }
    return taskParam;
}

void PmuDataFree(struct PmuData* pmuData)
{
    PmuList::GetInstance()->FreeData(pmuData);
    New(SUCCESS);
}
