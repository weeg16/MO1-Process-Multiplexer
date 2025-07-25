/* 
paging.cpp

[ definition here]
*/

#include "paging.h"
#include <vector>
#include <limits>

// Finds the index of the least recently used page in the page table
int findLRUPageIndex(const std::vector<PageTableEntry>& pageTable) {
    int minCycle = std::numeric_limits<int>::max();
    int lruIndex = -1;

    for (size_t i = 0; i < pageTable.size(); ++i) {
        if (pageTable[i].valid && pageTable[i].lastUsedCycle < minCycle) {
            minCycle = pageTable[i].lastUsedCycle;
            lruIndex = static_cast<int>(i);
        }
    }

    return lruIndex;
}
