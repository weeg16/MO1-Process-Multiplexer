#include "memory_manager.h"
#include "core_manager.h"
#include "util.h"

#include <fstream>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <iostream>

#include <sys/stat.h>
#include <sys/types.h>
#include <direct.h> // For Windows

extern CoreManager coreManager;  // Access global coreManager

Process* MemoryManager::findProcessByName(const std::string& name) {
    return coreManager.getProcessByName(name);
}

uint64_t MemoryManager::getCurrentCycle() const {
    return coreManager.getCpuTicks();  // assuming getCpuTicks() exists
}

inline void createFolderIfMissing(const std::string& folderName) {
#ifdef _WIN32
    _mkdir(folderName.c_str());  // Windows
#else
    mkdir(folderName.c_str(), 0777);  // POSIX
#endif
}

MemoryManager& MemoryManager::getInstance() {
    static MemoryManager instance;
    return instance;
}

MemoryManager::MemoryManager() {
    // Initially, all memory is one free block
    memoryBlocks.push_back({0, totalMemory - 1, false, ""});
}

void MemoryManager::init(uint32_t totalMem, uint32_t perProcMem, uint32_t memPerFrame) {
    totalMemory = totalMem;
    processMemory = perProcMem;
    this->memPerFrame = memPerFrame;

    numFrames = totalMemory / memPerFrame;

    frameTable.clear();
    frameTable.resize(numFrames);  // All entries default to unoccupied

    memoryBlocks.clear();
    memoryBlocks.push_back({0, totalMemory - 1, false, ""});

    // std::cout << "[INIT] totalMemory = " << totalMemory << ", processMemory = " << processMemory << std::endl; // FOR DEBUGGING

    // std::cout << "[DEBUG] Initialized " << numFrames << " physical frames (mem-per-frame = " 
    //         << memPerFrame << ")\n";

    // for (int i = 0; i < numFrames; ++i) {
    //     std::cout << "  Frame " << i 
    //             << " | Occupied: " << (frameTable[i].occupied ? "true" : "false")
    //             << " | Owner: " << frameTable[i].processName 
    //             << " | Page: " << frameTable[i].pageNumber << "\n";
    // }
}

bool MemoryManager::allocate(Process* proc) {
    std::lock_guard<std::mutex> lock(memMutex);

    // std::cout << "[ALLOCATE] Trying to allocate memory for process " << proc->name << std::endl; // FOR DEBUGGING

    for (auto& block : memoryBlocks) {
        int blockSize = block.end - block.start + 1;

        if (!block.occupied && blockSize >= proc->requestedMemory) {
            // Allocate from this block
            int allocStart = block.start;
            int allocEnd = allocStart + proc->requestedMemory - 1;

            // Update current block
            block.start = allocEnd + 1;

            // Inserting new allocated block right before updated block
            auto it = std::find_if(memoryBlocks.begin(), memoryBlocks.end(), [&](const Block& b) {
                return b.start == block.start && b.end == block.end && !b.occupied;
            });
            memoryBlocks.insert(it, {allocStart, allocEnd, true, proc->name});

            // If original block is now empty, remove it
            if (block.start > block.end) {
                memoryBlocks.erase(std::find(memoryBlocks.begin(), memoryBlocks.end(), block));
            }

            // Update process
            proc->memStart = allocStart;
            proc->memEnd = allocEnd;
            proc->inMemory = true;

            // std::cout << "[ALLOCATE] Successfully allocated memory for process " << proc->name << std::endl; // FOR DEBUGGING

            return true;
        }
    }

    // std::cout << "[ALLOCATE] Failed to allocate memory for process " << proc->name << std::endl; // FOR DEBUGGING
    return false; // no space
}

void MemoryManager::release(Process* proc) {
    std::lock_guard<std::mutex> lock(memMutex);

    for (auto& block : memoryBlocks) {
        if (block.occupied && block.processName == proc->name) {
            block.occupied = false;
            block.processName = "";
            proc->inMemory = false;
            mergeFreeBlocks();
            return;
        }
    }
}

bool operator==(const MemoryManager::Block& a, const MemoryManager::Block& b) {
    return a.start == b.start && a.end == b.end &&
           a.occupied == b.occupied && a.processName == b.processName;
}

void MemoryManager::mergeFreeBlocks() {
    bool merged = true;
    while (merged) {
        merged = false;
        for (size_t i = 0; i + 1 < memoryBlocks.size(); ++i) {
            Block& a = memoryBlocks[i];
            Block& b = memoryBlocks[i + 1];

            if (!a.occupied && !b.occupied && a.end + 1 == b.start) {
                a.end = b.end;
                memoryBlocks.erase(memoryBlocks.begin() + i + 1);
                merged = true;
                break; // Restart from beginning
            }
        }
    }
}

void MemoryManager::dumpSnapshot(uint64_t quantum) {
    std::lock_guard<std::mutex> lock(memMutex);

    // Create folder if it doesn't exist
    std::string folderName = "memory_snapshots";
    createFolderIfMissing(folderName);  // Only creates if missing

    std::ostringstream filename;
    filename << folderName << "/memory_stamp_"
             << std::setw(2) << std::setfill('0') << quantum << ".txt";

    std::ofstream out(filename.str());
    if (!out.is_open()) return;

    // Header
    out << "Timestamp: (" << getCurrentTimestamp() << ")\n";

    int inMemCount = 0;
    int fragmentation = 0;

    for (const auto& block : memoryBlocks) {
        if (block.occupied) {
            inMemCount++;
        } else if ((block.end - block.start + 1) < processMemory) {
            fragmentation += (block.end - block.start + 1);
        }
    }

    out << "Number of processes in memory: " << inMemCount << "\n";
    out << "Total external fragmentation in KB: " << fragmentation << "\n\n";

    out << "----end---- = 16384\n\n";

    for (auto it = memoryBlocks.rbegin(); it != memoryBlocks.rend(); ++it) {
        const auto& block = *it;
        if (block.occupied) {
            out << block.end + 1 << "\n";
            out << block.processName << "\n";
            out << block.start << "\n\n";
        }
    }

    out << "----start---- = 0\n";

    out << "\n--- Frame Table ---\n";
    for (int i = 0; i < frameTable.size(); ++i) {
        const auto& frame = frameTable[i];
        if (frame.occupied)
            out << "Frame " << i << ": " << frame.processName << ", page " << frame.pageNumber << " (valid)\n";
        else
            out << "Frame " << i << ": [free]\n";
    }
    out << "\n";

    out.close();
}

const std::vector<MemoryManager::FrameInfo>& MemoryManager::getFrameTable() const {
    return frameTable;
}

bool MemoryManager::loadPage(Process* proc, int pageNumber, uint64_t currentCycle) {
    std::lock_guard<std::mutex> lock(memMutex);

    // Step 1: Try to find a free frame
    int frameIndex = -1;
    for (int i = 0; i < numFrames; ++i) {
        if (!frameTable[i].occupied) {
            frameIndex = i;
            break;
        }
    }

    // Step 2: If no free frame, use LRU
    if (frameIndex == -1) {
        int oldestCycle = INT32_MAX;
        for (int i = 0; i < numFrames; ++i) {
            if (frameTable[i].lastUsedCycle < oldestCycle) {
                oldestCycle = frameTable[i].lastUsedCycle;
                frameIndex = i;
            }
        }

        std::string victimProc = frameTable[frameIndex].processName;
        int victimPage = frameTable[frameIndex].pageNumber;

        Process* victim = MemoryManager::getInstance().findProcessByName(victimProc);
        if (victim) {
            // if (victim->pageTable[victimPage].dirty) {
            //     std::cout << "[SWAP-OUT] Writing dirty page " << victimPage << " of process " << victimProc << " to backing store.\n"; // FOR DEBUGGING
            //     // (Optional: simulate writing to backing store file)
            // }

            if (victim->pageTable[victimPage].dirty) {
                std::ofstream backingStore("csopesy-backing-store.txt", std::ios::app);
                if (backingStore.is_open()) {
                    // You can replace "dummydata" with your page content if available
                    backingStore << victimProc << "," << victimPage << "," << "dummydata" << std::endl;
                    backingStore.close();
                }
            }

            victim->logPrint("Evicted page " + std::to_string(victimPage) + " from frame " + std::to_string(frameIndex));
            victim->pageTable[victimPage].valid = false;
            victim->pageTable[victimPage].frameNumber = -1;
            victim->pageTable[victimPage].dirty = false;
        }

        // std::cout << "[EVICT] Replacing page " << victimPage << " from process " << victimProc << " (frame " << frameIndex << ")\n";
    }

    // Step 3: Load new page
    frameTable[frameIndex].occupied = true;
    frameTable[frameIndex].processName = proc->name;
    frameTable[frameIndex].pageNumber = pageNumber;
    frameTable[frameIndex].lastUsedCycle = currentCycle;

    auto& pageEntry = proc->pageTable[pageNumber];
    pageEntry.frameNumber = frameIndex;
    pageEntry.valid = true;
    pageEntry.lastUsedCycle = currentCycle;

    // Simulate swap-in: check if this page is in the backing store file
    std::ifstream backingStore("csopesy-backing-store.txt");
    std::string line;
    while (std::getline(backingStore, line)) {
        std::istringstream iss(line);
        std::string procName, pageStr, data;
        std::getline(iss, procName, ',');
        std::getline(iss, pageStr, ',');
        std::getline(iss, data); // data can contain commas

        // if (procName == proc->name && std::stoi(pageStr) == pageNumber) {
        //     std::cout << "[SWAP-IN] Loaded page " << pageNumber << " for process " << proc->name
        //             << " from backing store (data=" << data << ")\n";
        //     break;
        // }
    }
    backingStore.close();


    // std::cout << "[PAGE LOAD] Process " << proc->name << " loaded page " << pageNumber
    //           << " into frame " << frameIndex << "\n";

    return true;
}
