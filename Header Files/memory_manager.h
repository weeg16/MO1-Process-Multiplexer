#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include <mutex>
#include "process.h"

class MemoryManager {
public:
    static MemoryManager& getInstance();
    void configure(int totalMem, int frameSize);

    struct Frame {
        int frameNo;
        std::string processName;
        int virtualPageNo;
        bool valid;
        uint64_t lastUsed;

        Frame(int fn, const std::string& pn, int vpn, bool v, uint64_t lu)
            : frameNo(fn), processName(pn), virtualPageNo(vpn), valid(v), lastUsed(lu) {}
    };

    std::vector<Frame> frameTable;

    int getFrameCount() const { return frameTable.size(); }
    const Frame& getFrame(int index) const { return frameTable[index]; }

    int getFrameSize() const { return memPerFrame; }
    
    struct Block {
        int start;
        int end;
        bool occupied;
        std::string processName;
    };

    bool allocate(Process* proc);   // first-fit allocation
    void release(Process* proc);    // free memory block
    void dumpSnapshot(uint64_t quantum);  // snapshot file generation

    bool accessPage(Process* proc, int virtualPageNo);

private:
    MemoryManager();
    MemoryManager(const MemoryManager&) = delete;
    MemoryManager& operator=(const MemoryManager&) = delete;

    int totalMemory = 0;
    int memPerFrame = 0;

    std::vector<Block> memoryBlocks;
    std::mutex memMutex;

    void mergeFreeBlocks();
};
