#pragma once

#include "process.h"
#include "config.h"
#include <string>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <random>

class CoreManager {
public:
    CoreManager();
    ~CoreManager();

    void configure(uint32_t coresCount,
                   const std::string& schedType,
                   uint32_t quantum,
                   uint32_t batchFreq,
                   uint32_t minI,
                   uint32_t maxI,
                   uint32_t delay);

    void start();
    void stopScheduler();

    void startSchedulerThread(const Config& config);     // equivalent to scheduler-start
    void stopSchedulerThread();      // equivalent to scheduler-stop

    void addProcess(Process* proc);
    void reportUtil();
    void listProcessStatus();
    void printProcessSummary(std::ostream& out, bool colorize);
    Process* getProcessByName(const std::string& name);
    Process* spawnNewNamedProcess(const std::string& name, int memoryBytes);

    int generateRandomInstructionCount() const;
    void pauseCores();

    uint64_t getCpuTicks() const;
    void printProcessSMI(std::ostream& out, bool colorize);
    void printVMStat(std::ostream& out);

    bool detectDeadlock();

private:
    void tickLoop();
    void coreWorker(int coreId);
    void busyWait(uint32_t milliseconds);

    uint32_t numCores = 1;
    std::string schedulerType = "fcfs";
    uint32_t quantumCycles = 1;
    uint32_t batchProcessFreq = 1;
    uint32_t minIns = 1;
    uint32_t maxIns = 5;
    uint32_t delayPerExec = 0;
    uint32_t processCounter = 0;

    std::vector<std::thread> cores;
    std::thread tickThread;
    std::thread schedulerThread;

    std::vector<bool> coreBusy;
    std::vector<uint32_t> coreInstructions;

    std::queue<Process*> readyQueue;
    std::vector<Process*> allProcesses;
    std::mutex queueMutex;
    std::condition_variable queueCond;

    std::atomic<bool> stop{false};
    std::atomic<bool> generating{false};
    std::atomic<uint64_t> cpuTicks{0};

    std::default_random_engine rng{std::random_device{}()};

    std::condition_variable generatorCond;
    std::mutex generatorMutex;
};
