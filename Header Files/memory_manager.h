#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include <mutex>
#include "process.h"

class MemoryManager {
public:
    static MemoryManager& getInstance();
    
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

    const int totalMemory = 16384;      // bytes
    const int processMemory = 4096;     // bytes
    std::vector<Block> memoryBlocks;
    std::mutex memMutex;

    void mergeFreeBlocks();
};
