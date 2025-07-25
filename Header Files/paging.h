/* 
paging.h

[ description here]
 */

 
#pragma once
#include <vector>


struct PageTableEntry {
    int pageNumber;     // Virtual page number
    int frameNumber;    // Physical frame number (-1 if not in memory)
    bool valid;         // True if currently loaded in memory
    bool dirty;         // True if modified (WRITE instruction)
    int lastUsedCycle;  // For LRU tracking
};

int findLRUPageIndex(const std::vector<PageTableEntry>& pageTable);
