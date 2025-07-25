#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include <mutex>
#include "process.h"

class MemoryManager {
public:
    static MemoryManager& getInstance();
    void init(uint32_t totalMem, uint32_t perProcMem);
    
    struct Block {
        int start;
        int end;
        bool occupied;
        std::string processName;
    };

    bool allocate(Process* proc);   // first-fit allocation
    void release(Process* proc);    // free memory block
    void dumpSnapshot(uint64_t quantum);  // snapshot file generation

private:
    MemoryManager();
    MemoryManager(const MemoryManager&) = delete;
    MemoryManager& operator=(const MemoryManager&) = delete;

    int totalMemory;     
    int processMemory;    
    std::vector<Block> memoryBlocks;
    std::mutex memMutex;

    void mergeFreeBlocks();
};
