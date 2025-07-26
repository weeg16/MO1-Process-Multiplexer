#include "core_manager.h"
#include "memory_manager.h"
#include "process.h"
#include "util.h"

#include <iostream>
#include <random>
#include <thread>
#include <chrono>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <vector>
#include <string>
#include <fstream>

static const char* ORANGE = "\033[38;5;208m";
static const char* BLUE   = "\033[34m";
static const char* YELLOW = "\033[33m";
static const char* GREEN  = "\033[32m"; 
static const char* RED    = "\033[31m";
static const char* RESET  = "\033[0m";

CoreManager::CoreManager() {
    stop.store(false);
    cpuTicks.store(0);
    generating.store(false);
}

CoreManager::~CoreManager() {
    stopScheduler();
    stopSchedulerThread();
    for (auto* proc : allProcesses) {
        delete proc;
    }
}

void CoreManager::configure(uint32_t coresCount, const std::string& schedType, uint32_t quantum,
                            uint32_t batchFreq, uint32_t minI, uint32_t maxI, uint32_t delay) {
    numCores = coresCount;
    schedulerType = schedType;
    quantumCycles = std::max<uint32_t>(1, quantum);
    batchProcessFreq = batchFreq;
    minIns = minI;
    maxIns = maxI;
    delayPerExec = delay;
    coreBusy.assign(numCores, false);
    coreInstructions.assign(numCores, 0);
}

void CoreManager::start() {
    stop = false;
    cpuTicks.store(0);

    for (auto& t : cores) {
        if (t.joinable()) t.join();
    }
    cores.clear();

    for (uint32_t i = 0; i < numCores; ++i) {
        cores.emplace_back(&CoreManager::coreWorker, this, i);
    }

    tickThread = std::thread(&CoreManager::tickLoop, this);
}

void CoreManager::stopScheduler() {
    stop = true;
    queueCond.notify_all();

    for (auto& t : cores) {
        if (t.joinable()) t.join();
    }

    if (tickThread.joinable()) tickThread.join();

    std::cout << "\n[INFO] Scheduler stopped. All cores joined.\n\n";
    std::this_thread::sleep_for(std::chrono::seconds(2));
    clearScreen();
    printHeader();
}

void CoreManager::startSchedulerThread(const Config& config) {
    if (generating.load()) return;

    generating = true;
    schedulerThread = std::thread([this, config] {
        try {
            while (generating) {
                std::string pname = "process" + std::to_string(processCounter);
                std::uniform_int_distribution<uint32_t> dist(config.minIns, config.maxIns);
                int numIns = dist(rng);
                auto* proc = new Process(pname, processCounter++, numIns);
                addProcess(proc);


                std::unique_lock<std::mutex> lock(generatorMutex);
                if (generatorCond.wait_for(lock, std::chrono::seconds(config.batchProcFreq),
                                        [&] { return !generating.load(); })) {
                    break;  // Exit early if generating turned false
}
            }
        } catch (const std::exception& e) {
            std::cerr << "[Scheduler Error] " << e.what() << "\n";
        }
    });
    std::cout << "[INFO] Batch process generation started.\n\n";
}

void CoreManager::stopSchedulerThread() {
    if (!generating.load()) return;
    generating = false;

    generatorCond.notify_all();  // Wakes up the wait

    if (schedulerThread.joinable()) schedulerThread.join();
    std::cout << "[INFO] Batch process generation stopped.\n";
}

void CoreManager::addProcess(Process* proc) {
    std::lock_guard<std::mutex> lock(queueMutex);
    readyQueue.push(proc);
    allProcesses.push_back(proc);
    queueCond.notify_one();
}

void CoreManager::reportUtil() {
    std::cout << "\n=== CPU Utilization Report ===\n";
    for (uint32_t i = 0; i < numCores; ++i) {
        std::cout << "Core " << i << ": " << coreInstructions[i] << " instructions executed.\n";
    }
    std::cout << "===============================\n\n";
}

void CoreManager::listProcessStatus() {
    std::cout << "\n--- Process Status ---\n\n";
    for (const auto& proc : allProcesses) {
        std::string status = proc->isFinished() ? "Finished" : (proc->assignedCore == -1 ? "Queued" : "Running");
        std::cout << proc->name << "  | " << status
                  << "  | Core " << proc->assignedCore
                  << "  | " << proc->executedInstructions << " / " << proc->totalInstructions
                  << "  | " << proc->timestamp << "\n";
    }
    std::cout << "\n------------------------\n\n";
}

void CoreManager::tickLoop() {
    while (!stop) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        uint64_t tick = cpuTicks.fetch_add(1);
        if (tick % quantumCycles == 0) {
            MemoryManager::getInstance().dumpSnapshot(tick / quantumCycles);
        }
        queueCond.notify_all();
    }
}

void CoreManager::busyWait(uint32_t milliseconds) {
    using clock = std::chrono::high_resolution_clock;
    auto start = clock::now();
    if (milliseconds == 0) milliseconds = 1;
    while (true) {
        auto now = clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count() >= milliseconds) break;
        for (volatile int i = 0; i < 1000; ++i);
    }
}

void CoreManager::coreWorker(int coreId) {
    while (!stop) {
        Process* proc = nullptr;
        {
            std::unique_lock<std::mutex> lock(queueMutex);

            // std::cout << "[CORE " << coreId << "] Waiting for a process..." << std::endl; // FOR DEBUGGING

            queueCond.wait(lock, [&] { return stop || !readyQueue.empty(); });

            if (stop && readyQueue.empty()) {
                coreBusy[coreId] = false;
                return;
            }

            proc = readyQueue.front();
            readyQueue.pop();
            proc->assignedCore = coreId;
            coreBusy[coreId] = true;

            // std::cout << "[CORE " << coreId << "] Picked process " << proc->name << " (inMemory=" << proc->inMemory << ")" << std::endl; // FOR DEBUGGING

            if (!proc->inMemory) {
                // std::cout << "[CORE " << coreId << "] Attempting to allocate memory for " << proc->name << std::endl; // FOR DEBUGGING

                if (!MemoryManager::getInstance().allocate(proc)) {
                    // std::cout << "[CORE " << coreId << "] Could not allocate memory for " << proc->name << ", requeuing." << std::endl; // FOR DEBUGGING
                    // Cannot allocate memory — requeue and skip this cycle
                    readyQueue.push(proc);
                    queueCond.notify_one();
                    coreBusy[coreId] = false;
                    continue;
                }
            }

            if (proc->timestamp.empty()) {
                proc->timestamp = getCurrentTimestamp();
            }
        }

        int remainingQuantum = quantumCycles;
        while (!proc->isFinished() && (schedulerType == "fcfs" || (schedulerType == "rr" && remainingQuantum-- > 0))) {
            if (stop) return;
            busyWait(delayPerExec);
            proc->executeNextInstruction();
            ++coreInstructions[coreId];
        }

        if (schedulerType == "rr" && !proc->isFinished()) {
            std::lock_guard<std::mutex> lock(queueMutex);
            readyQueue.push(proc);
            queueCond.notify_one();
        }

        if (proc->isFinished()) {
            MemoryManager::getInstance().release(proc);
        }

        coreBusy[coreId] = false;
    }
}

Process* CoreManager::getProcessByName(const std::string& name) {
    for (auto* p : allProcesses) {
        if (p->name == name) return p;
    }
    return nullptr;
}

Process* CoreManager::spawnNewNamedProcess(const std::string& name, int memoryBytes) {
    std::uniform_int_distribution<uint32_t> dist(minIns, maxIns);
    int numIns = dist(rng);
    Process* proc = new Process(name, processCounter++, numIns, memoryBytes);
    addProcess(proc);
    return proc;
}

void CoreManager::printProcessSummary(std::ostream& out, bool colorize) {
    int usedCores = 0;
    for (bool b : coreBusy) if (b) ++usedCores;
    int availableCores = numCores - usedCores;
    int percent = (numCores > 0) ? int((usedCores * 100.0) / numCores + 0.5) : 0;

    auto outc = [&](const std::string& s, const char* color = "") {
        if (colorize && color) out << color << s << RESET;
        else out << s;
    };

    out << "\nCPU utilization: ";
    outc(std::to_string(percent) + "%", ORANGE);
    out << "\nCores used: ";
    outc(std::to_string(usedCores), ORANGE); 
    out << "\nCores available: "; 
    outc(std::to_string(availableCores), ORANGE);
    
    out << "\n\n----------------------------------------\n";

    out << "\nRunning processes:\n\n";
    for (const auto& proc : allProcesses) {
        if (!proc->isFinished()) {
            out << proc->name << "  ";
            printColoredTimestamp(out, proc->timestamp);
            out << "  Core: ";
            outc(std::to_string(proc->assignedCore), ORANGE);
            out << "  ";
            outc(std::to_string(proc->executedInstructions), ORANGE);
            out << " / ";
            outc(std::to_string(proc->totalInstructions), ORANGE);
            out << "\n";
        }
    }

    out << "\nFinished processes:\n\n";
    for (const auto& proc : allProcesses) {
        if (proc->isFinished()) {
            out << proc->name << "  ";
            printColoredTimestamp(out, proc->timestamp);
            out << "  Finished  ";
            outc(std::to_string(proc->totalInstructions), ORANGE);
            out << " / ";
            outc(std::to_string(proc->totalInstructions), ORANGE);
            out << "\n";
        }
    }

    out << "\n----------------------------------------\n\n";
}

void CoreManager::printProcessSMI(std::ostream& out, bool colorize) {
    const auto& mem = MemoryManager::getInstance();
    const auto& frames = mem.getFrameTable();

    int totalMem = mem.getTotalMemory();
    int usedFrames = 0;

    auto outc = [&](const std::string& s, const char* color = "") {
        if (colorize && color) out << color << s << RESET;
        else out << s;
    };

    std::unordered_map<std::string, int> processMemoryBytes;

    for (const auto& frame : frames) {
        if (frame.occupied) {
            ++usedFrames;
            processMemoryBytes[frame.processName] += mem.getMemPerFrame();
        }
    }

    int usedMem = usedFrames * mem.getMemPerFrame();
    int cpuUtilPercent = 0;
    uint64_t ticks = getCpuTicks();
    if (ticks > 0) {
        int active = 0;
        for (const auto& count : coreInstructions) active += count;
        cpuUtilPercent = (active * 100) / (ticks * numCores);
        if (cpuUtilPercent > 100) cpuUtilPercent = 100;
    }

    auto toMiB = [](int bytes) {
        return std::max(1, bytes / (1024 * 1024));  // round up to 1MiB minimum if non-zero
    };

    out << "\n--------------------------------------------\n";
    out << "| PROCESS-SMI V01.00 Driver Version: 01.00 |\n";
    out << "--------------------------------------------\n\n";

    out << "CPU-Util:     " << ORANGE << std::to_string(cpuUtilPercent) << "%" << RESET << "\n";

    out << "Memory Usage: " << formatBytes(usedMem)
        << " / " << formatBytes(totalMem) << "\n";

    out << "Memory Util:  " << ORANGE << (100 * usedMem / totalMem) << "%" << RESET << "\n\n";

    out << "--------------------------------------------\n\n";
    out << "Running processes and memory usage:\n\n";

    for (const auto& proc : allProcesses) {
        if (!proc->isFinished() && processMemoryBytes.count(proc->name)) {
            out << proc->name << "  " << formatBytes(processMemoryBytes[proc->name]) << "\n";
        }
    }

    out << "\n--------------------------------------------\n";
}

int CoreManager::generateRandomInstructionCount() const {
    std::uniform_int_distribution<uint32_t> dist(minIns, maxIns);
    return dist(const_cast<std::default_random_engine&>(rng));
}

void CoreManager::pauseCores() {
    stop = true;
    queueCond.notify_all();  // unblock any waiting threads
    if (tickThread.joinable()) tickThread.join();

    for (auto& t : cores) {
        if (t.joinable()) t.join();
    }

    std::cout << "\n[INFO] Cores paused. No further execution or snapshots.\n\n";
    std::this_thread::sleep_for(std::chrono::seconds(2));
    clearScreen();
    printHeader();
}

uint64_t CoreManager::getCpuTicks() const {
    return cpuTicks.load();
}
