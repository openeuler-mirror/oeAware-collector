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
 * Author: Mr.Ye
 * Create: 2024-04-03
 * Description: event configuration query
 ******************************************************************************/
#include <iostream>
#include <cstdlib>
#include <cstdio>
#include <functional>
#include <unordered_map>
#include <cstdarg>
#include <unistd.h>
#include <cstring>
#include <fstream>
#include "trace.h"
#include "common.h"
#include "cpu_map.h"
#include "pfm_event.h"
#include "pmu_event.h"
#include "pmu.h"
#include "pcerr.h"
#include "pfm.h"

#include "uncore.h"
#include "core.h"

using namespace std;
using namespace pcerr;
using namespace KUNPENG_PMU;

static constexpr int MAX_STRING_LEN = 2048;
static CHIP_TYPE g_chipType = UNDEFINED_TYPE;

static struct PmuEvt* ConstructPmuEvtFromCore(KUNPENG_PMU::CoreConfig config, int collectType)
{
    struct PmuEvt* pmuEvtPtr = new PmuEvt;
    pmuEvtPtr->config = config.config;
    pmuEvtPtr->name = config.eventName;
    pmuEvtPtr->type = config.type;
    pmuEvtPtr->collectType = collectType;
    return std::move(pmuEvtPtr);
}

static struct PmuEvt* ConstructPmuEvtFromUncore(KUNPENG_PMU::UncoreConfig config, int collectType)
{
    struct PmuEvt* pmuEvtPtr = new PmuEvt;
    pmuEvtPtr->config = config.config;
    pmuEvtPtr->name = config.eventName;
    pmuEvtPtr->type = config.type;
    pmuEvtPtr->pmuType = config.pmuType;
    pmuEvtPtr->collectType = collectType;
    return std::move(pmuEvtPtr);
}

static struct PmuEvt* GetCoreEvent(const char* pmuName, int collectType)
{
    return KUNPENG_PMU::CORE_EVENT_MAP.at(g_chipType).find(pmuName) !=
           KUNPENG_PMU::CORE_EVENT_MAP.at(g_chipType).end()
           ? ConstructPmuEvtFromCore(
                    KUNPENG_PMU::CORE_EVENT_MAP.at(g_chipType).at(pmuName), collectType)
           : nullptr;
}

static struct PmuEvt* GetUncoreDDRCEvent(const char* pmuName, int collectType)
{
    return KUNPENG_PMU::DDRC_EVENT_MAP.at(g_chipType).find(pmuName) !=
           KUNPENG_PMU::DDRC_EVENT_MAP.at(g_chipType).end()
           ? ConstructPmuEvtFromUncore(KUNPENG_PMU::DDRC_EVENT_MAP.at(g_chipType).at(pmuName), collectType)
           : nullptr;
}

static struct PmuEvt* GetUncoreHHAEvent(const char* pmuName, int collectType)
{
    return KUNPENG_PMU::HHA_EVENT_MAP.at(g_chipType).find(pmuName) !=
           KUNPENG_PMU::HHA_EVENT_MAP.at(g_chipType).end()
           ? ConstructPmuEvtFromUncore(
                    KUNPENG_PMU::HHA_EVENT_MAP.at(g_chipType).at(pmuName), collectType)
           : nullptr;
}

static struct PmuEvt* GetUncoreLLCEvent(const char* pmuName, int collectType)
{
    return KUNPENG_PMU::L3C_EVENT_MAP.at(g_chipType).find(pmuName) !=
           KUNPENG_PMU::L3C_EVENT_MAP.at(g_chipType).end()
           ? ConstructPmuEvtFromUncore(
                    KUNPENG_PMU::L3C_EVENT_MAP.at(g_chipType).at(pmuName), collectType)
           : nullptr;
}

static struct PmuEvt* GetKernelTraceEvent(const char* pmuName, int collectType)
{
    int64_t config = GetTraceEventID(pmuName);
    if (config == -1) {
        return nullptr;
    }
    auto* pmuEvtPtr = new PmuEvt;
    pmuEvtPtr->config = config;
    pmuEvtPtr->name = pmuName;
    pmuEvtPtr->type = PERF_TYPE_TRACEPOINT;
    pmuEvtPtr->collectType = collectType;
    return pmuEvtPtr;
}

static int GetSpeType(void)
{
    constexpr char* speTypePath = "/sys/devices/arm_spe_0/type";
    FILE *fp = fopen(speTypePath, "r");
    int type;

    if (!fp) {
        return -1;
    }

    if (fscanf(fp, "%d", &type) != 1) {
        if (fclose(fp) == EOF) {
            return -1;
        }
        return -1;
    }

    if (fclose(fp) == EOF) {
        return -1;
    }
    return type;
}

using EvtRetriever = std::function<struct PmuEvt*(const char*, int)>;

static const std::unordered_map<int, EvtRetriever> EvtMap{
        {KUNPENG_PMU::CORE_TYPE, GetCoreEvent},
        {KUNPENG_PMU::HHA_TYPE, GetUncoreHHAEvent},
        {KUNPENG_PMU::L3C_TYPE, GetUncoreLLCEvent},
        {KUNPENG_PMU::DDRC_TYPE, GetUncoreDDRCEvent},
        {KUNPENG_PMU::TRACE_TYPE, GetKernelTraceEvent},
};

static int GetEventType(const char *pmuName, string &evtName, string &devName)
{
    auto coreMap = CORE_EVENT_MAP.at(g_chipType);
    auto findCoreEvent = coreMap.find(pmuName);
    if (findCoreEvent != coreMap.end()) {
        evtName = pmuName;
        return CORE_TYPE;
    }
    std::string strName(pmuName);

    // Kernel trace point event name like 'block:block_bio_complete'
    if (strName.find(':') != string::npos) {
        evtName = pmuName;
        return TRACE_TYPE;
    }

    // Parse uncore event name like 'hisi_sccl3_ddrc0/flux_rd/'
    auto findSlash = strName.find('/');
    if (findSlash == string::npos) {
        return -1;
    }
    devName = strName.substr(0, findSlash);
    findSlash = strName.find('/', findSlash);
    if (findSlash == string::npos) {
        return -1;
    }
    evtName = strName.substr(devName.size() + 1, strName.size() - 1 - (devName.size() + 1));
    auto ddrMap = DDRC_EVENT_MAP.at(g_chipType);
    auto findDDrEvent = ddrMap.find(evtName);
    if (findDDrEvent != ddrMap.end()) {
        return DDRC_TYPE;
    }

    return -1;
}

static int GetDeviceType(const string &devName)
{
    string typePath = "/sys/devices/" + devName + "/type";
    std::string realPath = GetRealPath(typePath);
    if (!IsValidPath(realPath)) {
        return -1;
    }
    ifstream typeIn(realPath);
    if (!typeIn.is_open()) {
        return -1;
    }
    string typeStr;
    typeIn >> typeStr;

    return stoi(typeStr);
}

static int GetCpuMask(const string &devName)
{
    string maskPath = "/sys/devices/" + devName + "/cpumask";
    std::string realPath = GetRealPath(maskPath);
    if (!IsValidPath(realPath)) {
        return -1;
    }
    ifstream maskIn(realPath);
    if (!maskIn.is_open()) {
        return -1;
    }
    // Cpumask is a comma-separated list of integers,
    // but now make it simple for ddrc event.
    string maskStr;
    maskIn >> maskStr;

    return stoi(maskStr);
}

static bool GetEvtConfig(const string &devName, const char* pmuName, const string &evtName, __u64 &config)
{
    string evtPath = "/sys/devices/" + devName + "/events/" + evtName;
    std::string realPath = GetRealPath(evtPath);
    if (!IsValidPath(realPath)) {
        return false;
    }
    ifstream evtIn(realPath);
    if (!evtIn.is_open()) {
        return false;
    }
    string configStr;
    evtIn >> configStr;
    auto findEq = configStr.find("=");
    if (findEq == string::npos) {
        return false;
    }
    auto subStr = configStr.substr(findEq + 1, configStr.size() - findEq);
    std::istringstream iss(subStr);
    iss >> std::hex >> config;

    return true;
}

static int FillUncoreFields(const string &devName, const char* pmuName, const string &evtName, PmuEvt *evt)
{
    int devType = GetDeviceType(devName);
    if (devType == -1) {
        return UNKNOWN_ERROR;
    }
    evt->type = devType;
    int cpuMask = GetCpuMask(devName);
    if (cpuMask == -1) {
        return UNKNOWN_ERROR;
    }
    if (!GetEvtConfig(devName, pmuName, evtName, evt->config)) {
        return LIBPERF_ERR_INVALID_EVENT;
    }

    evt->cpumask = cpuMask;
    evt->name = pmuName;
    return SUCCESS;
}

struct PmuEvt* PfmGetPmuEvent(const char* pmuName, int collectType)
{
    if (pmuName == nullptr) {
        auto* evt = new PmuEvt {0};
        evt->collectType = collectType;
        return evt;
    }
    string evtName;
    string devName;
    g_chipType = GetCpuType();
    if (g_chipType == UNDEFINED_TYPE) {
        return nullptr;
    }
    auto type = GetEventType(pmuName, evtName, devName);
    if (type == -1) {
        return nullptr;
    }
    struct PmuEvt* evt = (EvtMap.find(type) != EvtMap.end()) ?
                         EvtMap.at(type)(evtName.c_str(), collectType) : nullptr;
    if (evt == nullptr) {
        return evt;
    }
    if (!devName.empty()) {
        // Fill fields for uncore devices.
        auto err = FillUncoreFields(devName, pmuName, evtName, evt);
        if (err != SUCCESS) {
            return nullptr;
        }
    } else if (evt != nullptr) {
        evt->cpumask = -1;
    }
    return evt;
}

struct PmuEvt* PfmGetSpeEvent(
        unsigned long dataFilter, unsigned long eventFilter, unsigned long minLatency, int collectType)
{
    PmuEvt* evt = new PmuEvt{0};
    evt->collectType = collectType;
    int type = GetSpeType();
    if (type == -1) {
        return nullptr;
    }
    evt->type = static_cast<unsigned long>(type);
    evt->config = dataFilter;
    evt->config1 = eventFilter;
    evt->config2 = minLatency;
    evt->cpumask = -1;

    return evt;
}

void PmuEvtFree(PmuEvt *evt)
{
    if (evt != nullptr) {
        delete evt;
    }
}
