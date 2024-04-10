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
 * Author: Mr.Li
 * Create: 2024-04-03
 * Description: Provide a complete set of symbolic analysis tools, perform operations such as
 * module records, address analysis and stack conversion.
 ******************************************************************************/
#include <fcntl.h>
#include <unistd.h>
#include <cstdlib>
#include <cstdbool>
#include <cstdio>
#include <algorithm>
#include <fstream>
#include <dwarf++.hh>
#include <elf++.hh>
#include <climits>
#include "name_resolve.h"
#include "pcerr.h"
#include "symbol_resolve.h"

using namespace KUNPENG_SYM;
constexpr __u64 MAX_LINE_LENGTH = 1024;
constexpr __u64 MODULE_NAME_LEN = 1024;
constexpr __u64 MAX_LINUX_FILE_NAME = 1024;
constexpr __u64 MAX_LINUX_SYMBOL_LEN = 8192;
constexpr __u64 MAX_LINUX_MODULE_LEN = 1024;
constexpr int ADDR_LEN = 16;
constexpr int MODE_LEN = 5;
constexpr int MAP_LEN = 128;
constexpr int OFFSET_LEN = 16;
constexpr int DEV_LEN = 16;
constexpr int INODE_LEN = 16;
constexpr __u64 KERNEL_START_ADDR = 0xffff000000000000;
constexpr int BINARY_HALF = 2;
constexpr int KERNEL_NAME_LEN = 8;
constexpr int SHA_BIT_SHIFT_LEN = 8;
constexpr int KERNEL_MODULE_LNE = 128;
constexpr int CODE_LINE_RANGE_LEN = 10;
constexpr int HEX_LEN = 16;
constexpr int TO_TAIL_LEN = 2;

const std::string HUGEPAGE = "/anon_hugepage";
const std::string DEV_ZERO = "/dev/zero";
const std::string ANON = "//anon";
const std::string STACK = "[stack";
const std::string SOCKET = "socket:";
const std::string VSYSCALL = "[vsyscall]";
const std::string HEAP = "[heap]";
const std::string VDSO = "[vdso]";
const std::string SYSV = "/sysv";
const std::string VVAR = "[vvar]";
const std::string R_XP = "r-xp";
const std::string SLASH = "/";
const char DASH = '-';
const char EXE_TYPE = 'x';

namespace {
    static inline void SetFalse(bool& flag)
    {
        flag = false;
    }

    static inline bool CheckIfFile(std::string mapline)
    {
        return (!((mapline.find(HUGEPAGE) != std::string::npos) || (mapline.find(DEV_ZERO) != std::string::npos) ||
                  (mapline.find(ANON) != std::string::npos) || (mapline.find(STACK) != std::string::npos) ||
                  (mapline.find(SOCKET) != std::string::npos) || (mapline.find(VSYSCALL) != std::string::npos) ||
                  (mapline.find(HEAP) != std::string::npos) || (mapline.find(VDSO) != std::string::npos) ||
                  (mapline.find(SYSV) != std::string::npos) || (mapline.find(VVAR) != std::string::npos)) &&
                (mapline.find(R_XP) != std::string::npos))
               ? true
               : false;
    }

    static inline char* InitChar(int len)
    {
        char* str = new char[len + 1];
        memset(str, 0, len + 1);
        if (str == nullptr) {
            return nullptr;
        }
        return str;
    }

    static void ReadProcPidMap(std::ifstream& file, std::vector<std::shared_ptr<ModuleMap>>& modVec)
    {
        char line[MAX_LINUX_SYMBOL_LEN];
        char mode[MODE_LEN];
        char offset[OFFSET_LEN];
        char dev[DEV_LEN];
        char inode[INODE_LEN];
        while (file.getline(line, MAX_LINUX_SYMBOL_LEN)) {
            if (!CheckIfFile(line)) {
                continue;
            }
            std::shared_ptr<ModuleMap> data = std::make_shared<ModuleMap>();
            char modNameChar[MAX_LINUX_MODULE_LEN];
            if (sscanf(line, "%lx-%lx %s %s %s %s %s",
                          &data->start, &data->end, mode, offset,dev,
                          inode, modNameChar) == EOF) {
                continue;
            }
            data->moduleName = modNameChar;
            modVec.emplace_back(data);
        }
    }

    static inline void DwarfInfoRecord(DWARF_DATA_MAP& dwarfTable, const dwarf::line_table& lt,
                                       SymbolVet<std::string>& dwarfFileArray)
    {
        for (const dwarf::line_table::entry& line : lt) {
            if (line.end_sequence) {
                continue;
            }
            std::string fileName = line.file->path();
            if (fileName.empty()) {
                continue;
            }
            DwarfMap data = {0};
            data.lineNum = line.line;
            data.fileIndex = dwarfFileArray.InsertKeyForIndex(fileName);
            dwarfTable.insert({line.address, data});
        }
    }

    static inline void ElfInfoRecord(std::vector<ElfMap>& elfVector, const elf::section& sec)
    {
        for (const auto& sym : sec.as_symtab()) {
            auto& data = sym.get_data();
            if (data.type() != elf::stt::func)
                continue;
            ElfMap elfData;
            elfData.start = data.value;
            elfData.end = data.value + data.size;
            elfData.symbolName = sym.get_name();
            elfVector.emplace_back(elfData);
        }
    }

    static StackAsm* FirstLineMatch(const std::string& line)
    {
        if (line[line.size() - TO_TAIL_LEN] == ':') {
            struct StackAsm* stackAsm = CreateNode<struct StackAsm>();
            stackAsm->funcName = InitChar(MAX_LINE_LENGTH);
            stackAsm->asmCode = nullptr;
            stackAsm->next = nullptr;
            int ret = sscanf(line.c_str(), "%llx %s %*s %*s %llx", &stackAsm->funcStartAddr, stackAsm->funcName,
                               &stackAsm->functFileOffset);
            if (ret == EOF) {
                free(stackAsm->funcName);
                stackAsm->funcName = nullptr;
                free(stackAsm);
                stackAsm = nullptr;
                return nullptr;
            }
            return stackAsm;
        }
        return nullptr;
    }

    static bool MatchFileName(const std::string& line, std::string& fileName, unsigned int& lineNum)
    {
        char startStr[MAX_LINE_LENGTH + 1];
        int ret = sscanf(line.c_str(), "%s", startStr);
        if (ret < 0) {
            return false;
        }
        std::string preLine = startStr;
        if (preLine.find(":") + 1 >= preLine.size()) {
            return false;
        }
        std::string lineStr = preLine.substr(preLine.find(":") + 1, preLine.size());
        if (lineStr.empty()) {
            return false;
        }
        if (!SymbolUtils::IsNumber(lineStr)) {
            return false;
        }
        try {
            lineNum = std::atoi(lineStr.c_str());
        } catch (std::exception& err) {
            return false;
        }
        fileName = preLine.substr(0, preLine.find(":"));
        return true;
    }

    static AsmCode* MatchAsmCode(const std::string& line, const std::string& srcFileName, unsigned int lineNum)
    {
        std::string addrStr = line.substr(0, line.find(":"));
        struct AsmCode* asmCode = new AsmCode();
        asmCode->addr = SymbolUtils::SymStoul(addrStr);
        if (asmCode->addr == 0) {
            delete asmCode;
            return nullptr;
        }
        size_t tailLen = line.find("\n") != std::string::npos ? line.find("\n") : strlen(line.c_str());
        char startStr[MAX_LINE_LENGTH + 1];
        int ret = sscanf(line.c_str(), "%*s %s", startStr);
        if (ret == EOF) {
            delete asmCode;
            return nullptr;
        }
        std::string codeStr = line.substr(line.find(startStr) + strlen(startStr), tailLen);
        if (!codeStr.empty()) {
            codeStr.erase(0, codeStr.find_first_not_of(" "));
            codeStr.erase(0, codeStr.find_first_not_of("\t"));
        }
        asmCode->code = InitChar(MAX_LINE_LENGTH);
        strcpy(asmCode->code, codeStr.c_str());
        asmCode->fileName = InitChar(MAX_LINUX_FILE_NAME);
        strcpy(asmCode->fileName, srcFileName.c_str());
        asmCode->lineNum = lineNum;
        return asmCode;
    }

    static inline void FreeSymMap(SYMBOL_MAP& symMap)
    {
        for (auto& item : symMap) {
            for (auto& data : item.second) {
                SymbolUtils::FreeSymbol(data.second);
            }
        }
    }

    static inline void FreeStackMap(STACK_MAP& stackMap)
    {
        for (auto& item : stackMap) {
            for (auto& data : item.second) {
                struct Stack* head = data.second;
                FreeList(&head);
            }
        }
    }

    static inline void FreeKernelVet(std::vector<std::shared_ptr<Symbol>>& kernel)
    {
        for (auto symbol : kernel) {
            if (symbol->module) {
                delete[] symbol->module;
                symbol->module = nullptr;
            }
            if (symbol->fileName) {
                delete[] symbol->fileName;
                symbol->fileName = nullptr;
            }
            if (symbol->symbolName) {
                delete[] symbol->symbolName;
                symbol->symbolName = nullptr;
            }
        }
    }

    static inline size_t HashStr(unsigned long* stack, int nr)
    {
        std::string str = {};
        for (int k = 0; k < nr; k++) {
            str += stack[k];
        }
        std::hash<std::string> h;
        size_t n = h(str);
        return n;
    }

    static inline bool CheckColonSuffix(const std::string& str)
    {
        if (!strstr(str.c_str(), ":")) {
            return false;
        }
        size_t colonIndex = str.find(":");
        if (colonIndex + 1 >= str.size()) {
            return false;
        }
        std::string suffix = str.substr(colonIndex + 1, str.size());
        if (suffix.empty() || suffix.size() == 1) {
            return false;
        }
        return true;
    }

}  // namespace

void SymbolUtils::FreeSymbol(struct Symbol* symbol)
{
    if (symbol->symbolName) {
        delete[] symbol->symbolName;
        symbol->symbolName = nullptr;
    }
    if (symbol->fileName) {
        delete[] symbol->fileName;
        symbol->fileName = nullptr;
    }
    if (symbol->module) {
        delete[] symbol->module;
        symbol->module = nullptr;
    }
    if (symbol) {
        delete symbol;
        symbol = nullptr;
    }
}

void SymbolUtils::FreeStackAsm(struct StackAsm** stackAsm)
{
    struct StackAsm* current = *stackAsm;
    struct StackAsm* next;
    while (current != nullptr) {
        next = current->next;
        if (current->asmCode) {
            delete[] current->asmCode->code;
            delete[] current->asmCode->fileName;
            free(current->asmCode);
        }
        if (current->funcName) {
            delete[] current->funcName;
        }
        free(current);
        current = next;
    }
    *stackAsm = nullptr;
}

bool SymbolUtils::IsFile(const char* fileName)
{
    struct stat st {};
    // 获取文件属性
    if (stat(fileName, &st) != 0) {
        return false;
    }
    return (S_ISREG(st.st_mode)) ? true : false;
}

unsigned long SymbolUtils::SymStoul(const std::string& addrStr)
{
    try {
        return std::stoul(addrStr, nullptr, HEX_LEN);
    } catch (std::invalid_argument& invalidErr) {
        return 0;
    }
}

std::string SymbolUtils::RealPath(const std::string& filePath)
{
    char buff[PATH_MAX] = {0};
    if (realpath(filePath.c_str(), buff)) {
        return std::string{buff};
    } else {
        return std::string{};
    }
}

bool SymbolUtils::IsValidPath(const std::string& filePath)
{
    if (filePath.empty()) {
        return false;
    }
    return true;
}

bool SymbolUtils::IsNumber(const std::string& str)
{
    for (int i = 0; i < str.length(); i++) {
        if (!isdigit(str[i])) {
            return false;
        }
    }
    return true;
}

int SymbolResolve::RecordModule(int pid, RecordModuleType recordModuleType)
{
    if (pid < 0) {
        pcerr::New(LIBSYM_ERR_PARAM_PID_INVALID, "libsym param process ID must be greater than 0");
        return LIBSYM_ERR_PARAM_PID_INVALID;
    }
    SetFalse(this->isCleared);
    moduleSafeHandler.tryLock(pid);
    if (this->moduleMap.find(pid) != this->moduleMap.end()) {
        pcerr::New(0, "success");
        moduleSafeHandler.releaseLock(pid);
        return 0;
    }
    char mapFile[MAP_LEN];
    if (snprintf(mapFile, MAP_LEN, "/proc/%d/maps", pid) < 0) {
        moduleSafeHandler.releaseLock(pid);
        return LIBSYM_ERR_SNPRINF_OPERATE_FAILED;
    }
    std::ifstream file(mapFile);
    if (!file.is_open()) {
        pcerr::New(LIBSYM_ERR_OPEN_FILE_FAILED,
                   "libsym can't open file named " + std::string{mapFile} + " because of " + std::string{strerror(errno)});
        moduleSafeHandler.releaseLock(pid);
        return LIBSYM_ERR_OPEN_FILE_FAILED;
    }
    std::vector<std::shared_ptr<ModuleMap>> modVec;
    ReadProcPidMap(file, modVec);
    for (auto& item : modVec) {
        this->RecordElf(item->moduleName.c_str());
        if (recordModuleType != RecordModuleType::RECORD_NO_DWARF) {
            this->RecordDwarf(item->moduleName.c_str());
        }
    }
    this->moduleMap.insert({pid, modVec});
    pcerr::New(0, "success");
    moduleSafeHandler.releaseLock(pid);
    return 0;
}

int SymbolResolve::UpdateModule(int pid)
{
    if (pid < 0) {
        pcerr::New(LIBSYM_ERR_PARAM_PID_INVALID, "libsym param process ID must be greater than 0");
        return LIBSYM_ERR_PARAM_PID_INVALID;
    }
    moduleSafeHandler.tryLock(pid);
    if (this->moduleMap.find(pid) == this->moduleMap.end()) {
        // need to use RecordModule first!
        pcerr::New(SUCCESS);
        moduleSafeHandler.releaseLock(pid);
        return SUCCESS;
    }
    // Get memory maps of pid.
    char mapFile[MAP_LEN];
    if (snprintf(mapFile, MAP_LEN, "/proc/%d/maps", pid) < 0) {
        moduleSafeHandler.releaseLock(pid);
        return LIBSYM_ERR_SNPRINF_OPERATE_FAILED;
    }
    std::ifstream file(mapFile);
    if (!file.is_open()) {
        pcerr::New(LIBSYM_ERR_OPEN_FILE_FAILED,
                   "libsym can't open file named " + std::string{mapFile} + " because of " + std::string{strerror(errno)});
        moduleSafeHandler.releaseLock(pid);
        return LIBSYM_ERR_OPEN_FILE_FAILED;
    }
    std::vector<std::shared_ptr<ModuleMap>> newModVec;
    ReadProcPidMap(file, newModVec);

    // Find new dynamic modules.
    auto &oldModVec = moduleMap[pid];
    if (newModVec.size() <= oldModVec.size()) {
        pcerr::New(SUCCESS);
        moduleSafeHandler.releaseLock(pid);
        return SUCCESS;
    }
    auto diffModVec = FindDiffMaps(oldModVec, newModVec);
    // Load modules.
    for (auto& item : diffModVec) {
        this->RecordElf(item->moduleName.c_str());
        this->RecordDwarf(item->moduleName.c_str());
    }
    for (auto mod : diffModVec) {
        oldModVec.push_back(mod);
    }
    pcerr::New(SUCCESS);
    moduleSafeHandler.releaseLock(pid);
    return SUCCESS;
}

void SymbolResolve::FreeModule(int pid)
{
    if (pid < 0) {
        return;
    }
    moduleSafeHandler.tryLock(pid);
    auto it = this->moduleMap.find(pid);
    if (it != this->moduleMap.end()) {
        this->moduleMap.erase(it);
    }
    moduleSafeHandler.releaseLock(pid);
    return;
}

struct SortElf {
    inline bool operator()(const ElfMap& first, const ElfMap& second)
    {
        return (first.start < second.start);
    }
};

int SymbolResolve::RecordElf(const char* fileName)
{
    SetFalse(this->isCleared);
    std::string file = fileName;
    elfSafeHandler.tryLock(file);

    if (this->elfMap.find(fileName) != this->elfMap.end()) {
        pcerr::New(0, "success");
        elfSafeHandler.releaseLock(file);
        return 0;
    }

    if (!SymbolUtils::IsFile(fileName)) {
        pcerr::New(LIBSYM_ERR_FILE_NOT_RGE, "libsym detects that the input parameter fileName is not a file");
        elfSafeHandler.releaseLock(file);
        return LIBSYM_ERR_FILE_NOT_RGE;
    }

    /** symbol cache logic should be implemented after this line */
    int fd = open(fileName, O_RDONLY);
    if (fd < 0) {
        pcerr::New(LIBSYM_ERR_OPEN_FILE_FAILED,
                   "libsym can't open file named " + file + " because of " + std::string{strerror(errno)});
        elfSafeHandler.releaseLock(file);
        return LIBSYM_ERR_OPEN_FILE_FAILED;
    }

    std::shared_ptr<elf::loader> efLoader = elf::create_mmap_loader(fd);
    elf::elf ef(efLoader);
    std::vector<ElfMap> elfVector;

    try {
        for (const auto& sec : ef.sections()) {
            if (sec.get_hdr().type != elf::sht::symtab && sec.get_hdr().type != elf::sht::dynsym) {
                continue;
            }
            ElfInfoRecord(elfVector, sec);
        }
    } catch (elf::format_error& error) {
        pcerr::New(LIBSYM_ERR_ELFIN_FOMAT_FAILED, "libsym record elf format error: " + std::string{error.what()});
        elfSafeHandler.releaseLock(file);
        return LIBSYM_ERR_ELFIN_FOMAT_FAILED;
    }

    std::sort(elfVector.begin(), elfVector.end(), SortElf());
    this->elfMap.insert({file, elfVector});
    close(fd);
    pcerr::New(0, "success");
    elfSafeHandler.releaseLock(file);
    return 0;
}

int SymbolResolve::RecordDwarf(const char* fileName)
{
    SetFalse(this->isCleared);
    std::string file = fileName;
    dwarfSafeHandler.tryLock(file);

    if (this->dwarfMap.find(fileName) != this->dwarfMap.end()) {
        pcerr::New(0, "success");
        dwarfSafeHandler.releaseLock((file));
        return 0;
    }

    if (!SymbolUtils::IsFile(fileName)) {
        pcerr::New(LIBSYM_ERR_FILE_NOT_RGE, "libsym detects that the input parameter fileName is not a file");
        dwarfSafeHandler.releaseLock((file));
        return LIBSYM_ERR_FILE_NOT_RGE;
    }

    int fd = open(fileName, O_RDONLY);
    if (fd < 0) {
        pcerr::New(LIBSYM_ERR_OPEN_FILE_FAILED,
                   "libsym can't open file named " + file + " because of " + std::string{strerror(errno)});
        dwarfSafeHandler.releaseLock((file));
        return LIBSYM_ERR_OPEN_FILE_FAILED;
    }
    /** symbol cache logic should be implemented after this line */

    auto efLoader = elf::create_mmap_loader(fd);
    elf::elf ef(std::move(efLoader));

    try {
        dwarf::dwarf dw(dwarf::elf::create_loader(ef));
        DWARF_DATA_MAP dwarfTable;
        for (const auto& cu : dw.compilation_units()) {
            const dwarf::line_table lt = cu.get_line_table();
            DwarfInfoRecord(dwarfTable, lt, dwarfFileArray);
        }
        std::vector<unsigned long> addrVet;
        for (auto it = dwarfTable.begin(); it != dwarfTable.end(); ++it) {
            addrVet.push_back(it->first);
        }
        this->dwarfMap.insert({file, dwarfTable});
        this->dwarfVetMap.insert({file, addrVet});
    } catch (dwarf::format_error& error) {
        dwarfSafeHandler.releaseLock((file));
        pcerr::New(LIBSYM_ERR_DWARF_FORMAT_FAILED,
                   "libsym record dwarf file named " + file + " format error: " + std::string{error.what()});
        return LIBSYM_ERR_DWARF_FORMAT_FAILED;
    }

    efLoader.reset();
    close(fd);
    pcerr::New(0, "success");
    dwarfSafeHandler.releaseLock((file));
    return 0;
}

void SymbolResolve::Clear()
{
    std::lock_guard<std::mutex> lock(mutex);
    if (!this->instance) {
        return;
    }
    /**
     * free the memory allocated for symbol table
     */
    FreeSymMap(this->symbolMap);

    /**
     * free the memory allocated for stack table
     */
    FreeStackMap(this->stackMap);
    /**
     * free the memory allocated from kernel
     */
    FreeKernelVet(this->ksymArray);
    this->isCleared = true;
    delete this->instance;
    this->instance = nullptr;
}

void SymbolResolve::SearchElfInfo(
        std::vector<ElfMap>& elfVec, unsigned long addr, struct Symbol* symbol, unsigned long* offset)
{
    ssize_t start = 0;
    ssize_t end = elfVec.size() - 1;
    ssize_t mid;
    unsigned long symAddr;

    while (start < end) {
        mid = start + (end - start + 1) / BINARY_HALF;
        symAddr = elfVec[mid].start;
        if (symAddr <= addr) {
            start = mid;
        } else {
            end = mid - 1;
        }
    }

    if (start == end && elfVec[start].start <= addr && elfVec[start].end >= addr) {
        *offset = addr - elfVec[start].start;
        symbol->codeMapEndAddr = elfVec[start].end;
        char* name = CppNamedDemangle(elfVec[start].symbolName.c_str());
        if (name) {
            strcpy(symbol->symbolName, name);
            free(name);
            name = nullptr;
            return;
        }
        strcpy(symbol->symbolName, elfVec[start].symbolName.c_str());
        return;
    }
    strcpy(symbol->symbolName, "UNKNOWN");
    return;
}

void SymbolResolve::SearchDwarfInfo(
        std::vector<unsigned long>& addrVet, DWARF_DATA_MAP& dwarfDataMap, unsigned long addr, struct Symbol* symbol)
{
    DwarfMap dwarfMap = {0};
    bool findLine = false;
    if (dwarfDataMap.find(addr) != dwarfDataMap.end()) {
        dwarfMap = dwarfDataMap.at(addr);
        findLine = true;
    } else {
        auto it = std::upper_bound(addrVet.begin(), addrVet.end(), addr);
        if (it > addrVet.begin() && it < addrVet.end()) {
            --it;
        }
        if (it != addrVet.end()) {
            dwarfMap = dwarfDataMap.at(*it);
            findLine = true;
        }
    }
    if (findLine) {
        std::string fileName = dwarfFileArray.GetKeyByIndex(dwarfMap.fileIndex);
        strcpy(symbol->fileName, fileName.c_str());
        symbol->lineNum = dwarfMap.lineNum;
    } else {
        strcpy(symbol->fileName, "Uknown");
        symbol->lineNum = 0;
    }
}

std::shared_ptr<ModuleMap> SymbolResolve::AddrToModule(
        std::vector<std::shared_ptr<ModuleMap>>& processModule, unsigned long addr)
{
    ssize_t start = 0;
    ssize_t end = processModule.size() - 1;
    ssize_t mid;
    unsigned long symAddr;

    while (start < end) {
        mid = start + (end - start + 1) / BINARY_HALF;
        symAddr = processModule[mid]->start;
        if (symAddr <= addr) {
            start = mid;
        } else {
            end = mid - 1;
        }
    }

    if (start == end && processModule[start]->start <= addr) {
        return processModule[start];
    }

    pcerr::New(LIBSYM_ERR_MAP_ADDR_MODULE_FAILED, "libsym addr can't find module");
    return nullptr;
}

struct Stack* SymbolResolve::StackToHash(int pid, unsigned long* stack, int nr)
{
    if (this->stackMap.find(pid) == this->stackMap.end()) {
        this->stackMap[pid] = {};
    }
    size_t stackId = HashStr(stack, nr);
    if (this->stackMap.at(pid).find(stackId) != this->stackMap.at(pid).end()) {
        return this->stackMap.at(pid).at(stackId);
    }

    struct Stack* head = nullptr;
    for (int i = nr - 1; i >= 0; i--) {
        struct Stack* current = CreateNode<struct Stack>();
        auto symbol = this->MapAddr(pid, stack[i]);
        if (symbol != nullptr) {
            current->symbol = symbol;
        } else {
            current->symbol = nullptr;
        }
        AddTail<struct Stack>(&head, &current);
    }

    if (this->stackMap.at(pid).find(stackId) == this->stackMap.at(pid).end()) {
        this->stackMap.at(pid)[stackId] = head;
    }
    pcerr::New(0, "success");
    return head;
}

struct Symbol* SymbolResolve::MapKernelAddr(unsigned long addr)
{
    ssize_t start = 0;
    ssize_t end = this->ksymArray.size() - 1;
    ssize_t mid;
    __u64 symAddr;

    while (start < end) {
        mid = start + (end - start + 1) / BINARY_HALF;
        symAddr = this->ksymArray[mid]->addr;
        if (symAddr <= addr) {
            start = mid;
        } else {
            end = mid - 1;
        }
    }

    if (start == end && this->ksymArray[start]->addr <= addr) {
        return this->ksymArray[start].get();
    }
    pcerr::New(LIBSYM_ERR_MAP_KERNAL_ADDR_FAILED, "libsym cannot find the corresponding kernel address");
    return nullptr;
}

struct Symbol* SymbolResolve::MapUserAddr(int pid, unsigned long addr)
{
    if (this->moduleMap.find(pid) == this->moduleMap.end()) {
        pcerr::New(LIBSYM_ERR_NOT_FIND_PID, "The libsym process ID " + std::to_string(pid) + " cannot be found.");
        return nullptr;
    }

    std::shared_ptr<ModuleMap> module = this->AddrToModule(this->moduleMap.at(pid), addr);
    if (!module) {
        return nullptr;
    }
    /**
     * Try to search elf data first
     */
    symSafeHandler.tryLock(pid);
    if (this->symbolMap.find(pid) == this->symbolMap.end()) {
        this->symbolMap[pid] = {};
    }
    symSafeHandler.releaseLock(pid);
    auto it = this->symbolMap.at(pid).find(addr);
    if (it != this->symbolMap.at(pid).end()) {
        return it->second;
    }
    struct Symbol* symbol = new struct Symbol();
    symbol->module = InitChar(MAX_LINUX_MODULE_LEN);
    symbol->symbolName = InitChar(MAX_LINUX_SYMBOL_LEN);
    symbol->fileName = InitChar(MAX_LINUX_FILE_NAME);
    symbol->addr = addr;
    symbol->offset = 0;
    strcpy(symbol->module, module->moduleName.c_str());
    unsigned long addrToSearch = addr;
    if (this->elfMap.find(module->moduleName) != this->elfMap.end()) {
        // If the largest symbol in the elf symbol table is detected to be smaller than the searched symbol, subtraction
        // is performed.
        std::vector<ElfMap> elfVet = this->elfMap.at(module->moduleName);
        if (!elfVet.empty()) {
            if (elfVet.back().end < addrToSearch && addrToSearch > module->start) {
                addrToSearch = addrToSearch - module->start;
            }
            this->SearchElfInfo(elfVet, addrToSearch, symbol, &symbol->offset);
        }
    }

    if (this->dwarfMap.find(module->moduleName) != this->dwarfMap.end()) {
        this->SearchDwarfInfo(
                this->dwarfVetMap.at(module->moduleName), this->dwarfMap.at(module->moduleName), addrToSearch, symbol);
    }
    symbol->codeMapAddr = addrToSearch;
    this->symbolMap.at(pid).insert({addr, symbol});
    pcerr::New(0, "success");
    return symbol;
}

struct Symbol* SymbolResolve::MapAddr(int pid, unsigned long addr)
{
    struct Symbol* data = nullptr;
    if (addr > KERNEL_START_ADDR) {
        data = this->MapKernelAddr(addr);
        if (data == nullptr) {
            return nullptr;
        }
        data->offset = addr - data->addr;
    } else {
        data = this->MapUserAddr(pid, addr);
    }
    return data;
}

int SymbolResolve::RecordKernel()
{
    SetFalse(this->isCleared);
    if (!this->ksymArray.empty()) {
        pcerr::New(0, "success");
        return 0;
    }
    //  Prevent multiple threads from processing kernel data at the same time.
    std::lock_guard<std::mutex> guard(kernelMutex);

    FILE* kallsyms = fopen("/proc/kallsyms", "r");
    if (__glibc_unlikely(kallsyms == nullptr)) {
        pcerr::New(LIBSYM_ERR_KALLSYMS_INVALID,
                   "libsym failed to open /proc/kallsyms, found that file /proc/kallsyms " + std::string{strerror(errno)});
        return LIBSYM_ERR_KALLSYMS_INVALID;
    }

    char line[MAX_LINE_LENGTH];
    char mode;
    __u64 addr;
    char name[KERNEL_MODULE_LNE];

    while (fgets(line, sizeof(line), kallsyms)) {
        if (sscanf(line, "%llx %c %s%*[^\n]\n", &addr, &mode, name) == EOF) {
            continue;
        }
        ssize_t nameLen = strlen(name);
        std::shared_ptr<Symbol> data = std::make_shared<Symbol>();
        data->symbolName = InitChar(nameLen);
        strcpy(data->symbolName, name);
        data->addr = addr;
        data->fileName = InitChar(KERNEL_NAME_LEN);
        strcpy(data->fileName, "KERNEL");
        data->module = InitChar(KERNEL_NAME_LEN);
        strcpy(data->module, "KERNEL");
        data->lineNum = 0;
        this->ksymArray.emplace_back(data);
    }

    fclose(kallsyms);
    pcerr::New(0, "success");
    return 0;
}

int SymbolResolve::UpdateModule(int pid, const char* moduleName, unsigned long startAddr)
{
    if (pid < 0) {
        pcerr::New(LIBSYM_ERR_PARAM_PID_INVALID, "libsym param process ID must be greater than 0");
        return LIBSYM_ERR_PARAM_PID_INVALID;
    }
    SetFalse(this->isCleared);
    std::shared_ptr<ModuleMap> data = std::make_shared<ModuleMap>();
    data->moduleName = moduleName;
    data->start = startAddr;
    int ret = this->RecordElf(data->moduleName.c_str());
    if (ret == LIBSYM_ERR_OPEN_FILE_FAILED || ret == LIBSYM_ERR_FILE_NOT_RGE) {
        return ret;
    }

    this->RecordDwarf(data->moduleName.c_str());
    if (ret == LIBSYM_ERR_OPEN_FILE_FAILED || ret == LIBSYM_ERR_FILE_NOT_RGE) {
        return ret;
    }

    if (this->moduleMap.find(pid) == this->moduleMap.end()) {
        std::vector<std::shared_ptr<ModuleMap>> modVec;
        modVec.emplace_back(data);
        this->moduleMap[pid] = modVec;
    } else {
        this->moduleMap[pid].emplace_back(data);
    }
    pcerr::New(0, "success");
    return 0;
}

struct Symbol* SymbolResolve::MapCodeElfAddr(const std::string& moduleName, unsigned long addr)
{
    struct Symbol* data = nullptr;
    if (addr > KERNEL_START_ADDR) {
        pcerr::New(LIBSYM_ERR_MAP_CODE_KERNEL_NOT_SUPPORT,
                   "libsym The current version does not support kernel source code matching.");
        return nullptr;
    } else {
        int ret = RecordElf(moduleName.c_str());
        if (ret != 0) {
            return nullptr;
        }
        data = this->MapUserCodeAddr(moduleName, addr);
    }
    return data;
}

struct Symbol* SymbolResolve::MapUserCodeAddr(const std::string& moduleName, unsigned long startAddr)
{
    struct Symbol* symbol = new Symbol();
    symbol->symbolName = InitChar(MAX_LINUX_SYMBOL_LEN);
    symbol->fileName = InitChar(MAX_LINUX_SYMBOL_LEN);
    symbol->module = InitChar(MAX_LINUX_MODULE_LEN);
    symbol->addr = startAddr;
    unsigned long addrToSearch = startAddr;
    symbol->codeMapAddr = addrToSearch;
    strcpy(symbol->module, moduleName.c_str());
    if (this->elfMap.find(moduleName) != this->elfMap.end()) {
        this->SearchElfInfo(this->elfMap.at(moduleName), addrToSearch, symbol, &symbol->offset);
    }
    pcerr::New(0, "success");
    return symbol;
}

struct StackAsm* SymbolResolve::MapAsmCode(const char* moduleName, unsigned long startAddr, unsigned long endAddr)
{
    struct StackAsm* stackAsm = MapAsmCodeStack(moduleName, startAddr, endAddr);
    return stackAsm;
}

struct Symbol* SymbolResolve::MapCodeAddr(const char* moduleName, unsigned long startAddr)
{
    Symbol* symbol = MapCodeElfAddr(moduleName, startAddr);
    if (!symbol) {
        return nullptr;
    }
    int ret = RecordDwarf(moduleName);
    if (ret == 0) {
        this->SearchDwarfInfo(
                this->dwarfVetMap.at(moduleName), this->dwarfMap.at(moduleName), symbol->codeMapAddr, symbol);
    } else {
        symbol->fileName = nullptr;
    }
    return symbol;
}

struct StackAsm* ReadAsmCodeFromPipe(FILE* pipe)
{
    struct StackAsm* head = nullptr;
    struct StackAsm* last = nullptr;
    int lineLen = MAX_LINE_LENGTH;
    char line[lineLen];
    std::string srcFileName = "unknown";
    unsigned int srcLineNum = 0;
    while (!feof(pipe)) {
        memset(line, 0, lineLen);
        if (!fgets(line, lineLen, pipe)) {
            break;
        }
        if (!line || (line && line[0] == '\0') || (line && line[0] == '\n')) {
            continue;
        }
        struct StackAsm* stackAsm = nullptr;
        if (strstr(line, "File Offset") != nullptr) {
            stackAsm = FirstLineMatch(line);
        }
        if (!stackAsm && head) {
            if (!CheckColonSuffix(line)) {
                continue;
            }
            if (MatchFileName(line, srcFileName, srcLineNum)) {
                continue;
            }
            AsmCode* asmCode = MatchAsmCode(line, srcFileName, srcLineNum);
            if (!asmCode) {
                continue;
            }
            struct StackAsm* current = CreateNode<struct StackAsm>();
            current->funcName = nullptr;
            current->asmCode = asmCode;
            last->next = current;
            last = current;
            continue;
        }
        if (!head && stackAsm) {
            AddTail<struct StackAsm>(&head, &stackAsm);
            last = head;
        }
        if (stackAsm && head) {
            last->next = stackAsm;
            last = stackAsm;
        }
    }
    pclose(pipe);
    pipe = nullptr;
    return head;
}

struct StackAsm* SymbolResolve::MapAsmCodeStack(
        const std::string& moduleName, unsigned long startAddr, unsigned long endAddr)
{
    char startAddrStr[ADDR_LEN];
    char endAddrStr[ADDR_LEN];
    if (startAddr >= endAddr) {
        pcerr::New(LIBSYM_ERR_START_SMALLER_END, "libysm the end address must be greater than the start address");
        return nullptr;
    }
    if (snprintf(startAddrStr, ADDR_LEN, "0x%lx", startAddr) < 0) {
        pcerr::New(LIBSYM_ERR_SNPRINF_OPERATE_FAILED, "libsym fails to execute snprintf");
        return nullptr;
    }

    if (snprintf(endAddrStr, ADDR_LEN, "0x%lx", endAddr) < 0) {
        pcerr::New(LIBSYM_ERR_SNPRINF_OPERATE_FAILED, "libsym fails to execute snprintf");
        return nullptr;
    }
    std::string cmd = "objdump -Fld " + moduleName + " --start-address=" + std::string{startAddrStr} +
                      " --stop-address=" + std::string{endAddrStr};
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        pcerr::New(LIBSYM_ERR_CMD_OPERATE_FAILED,
                   "libsym fails to obtain the assembly instruction" + std::string{strerror(errno)});
        return nullptr;
    }
    struct StackAsm* head = ReadAsmCodeFromPipe(pipe);
    pcerr::New(0, "success");
    return head;
}

std::vector<std::shared_ptr<ModuleMap>> SymbolResolve::FindDiffMaps(
        const std::vector<std::shared_ptr<ModuleMap>>& oldMaps,
        const std::vector<std::shared_ptr<ModuleMap>>& newMaps) const
{
    std::vector<std::shared_ptr<ModuleMap>> diffMaps;
    for (auto newMod : newMaps) {
        for (auto oldMod : oldMaps) {
            if (newMod->start != oldMod->start) {
                diffMaps.push_back(newMod);
            }
        }
    }

    return diffMaps;
}

SymbolResolve* SymbolResolve::instance = nullptr;
std::mutex SymbolResolve::mutex;
std::mutex SymbolResolve::kernelMutex;
