#include "memory_manager.h"
#include "process.h"
#include <iostream>
#include <thread>
#include <chrono>

void printPageTable(const Process& p) {
    std::cout << "== Page Table for " << p.name << " ==\n";
    for (int i = 0; i < 5; ++i) {
        auto entry = p.pageTable.at(i);
        std::cout << "VPN " << i 
                  << " -> Frame: " << entry.frameNo
                  << " | Valid: " << entry.valid << "\n";
    }
}

int main() {
    MemoryManager& mm = MemoryManager::getInstance();

    Process p1("P1", 1, 0);
    Process p2("P2", 2, 0);

    mm.accessPage(&p1, 0); // P1 accesses page 0
    mm.accessPage(&p1, 1); // P1 accesses page 1
    mm.accessPage(&p2, 0); // P2 accesses page 0
    mm.accessPage(&p2, 1); // P2 accesses page 1
    mm.accessPage(&p1, 2); // Should trigger LRU

    printPageTable(p1);
    printPageTable(p2);

    return 0;
}
