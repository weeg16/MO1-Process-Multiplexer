#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include <mutex>
#include "process.h"

class MemoryManager {
public:
    static MemoryManager& getInstance();
    void init(uint32_t totalMem, uint32_t perProcMem, uint32_t memPerFrame);
    
    struct Block {
        int start;
        int end;
        bool occupied;
        std::string processName;
    };

    bool allocate(Process* proc);   // first-fit allocation
    void release(Process* proc);    // free memory block
    void dumpSnapshot(uint64_t quantum);  // snapshot file generation

    struct FrameInfo {
        bool occupied = false;
        std::string processName;
        int pageNumber = -1;
        int lastUsedCycle = 0;
    };

    const std::vector<FrameInfo>& getFrameTable() const;

    uint32_t getMemPerFrame() const { return memPerFrame; }
    uint64_t getCurrentCycle() const;  // forward declaration

    bool loadPage(Process* proc, int pageNumber, uint64_t currentCycle);

    Process* findProcessByName(const std::string& name);

    int getDefaultProcessMemory() const { return processMemory; }

    int getTotalMemory() const { return totalMemory; }

    int getPagesIn() const { return pagesIn; }
    int getPagesOut() const { return pagesOut; }

private:
    MemoryManager();
    MemoryManager(const MemoryManager&) = delete;
    MemoryManager& operator=(const MemoryManager&) = delete;

    void mergeFreeBlocks();

    int totalMemory;
    int processMemory;
    int numFrames;
    uint32_t memPerFrame;

    std::vector<FrameInfo> frameTable;
    std::vector<Block> memoryBlocks;
    std::mutex memMutex;

    int pagesIn = 0;
    int pagesOut = 0;
};
