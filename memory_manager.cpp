#include "memory_manager.h"
#include "util.h"

#include <fstream>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <iostream>

#include <sys/stat.h>
#include <sys/types.h>
#include <direct.h> // For Windows

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

bool MemoryManager::allocate(Process* proc) {
    std::lock_guard<std::mutex> lock(memMutex);

    for (auto& block : memoryBlocks) {
        int blockSize = block.end - block.start + 1;

        if (!block.occupied && blockSize >= processMemory) {
            // Allocate from this block
            int allocStart = block.start;
            int allocEnd = allocStart + processMemory - 1;

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

            return true;
        }
    }

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
    out.close();
}
