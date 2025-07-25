/* 
config.h

Declares the Config struct and function prototypes for config file parsing and validation.
*/

#pragma once

#include <string>
#include <cstdint>

struct Config {
    uint32_t numCPU;
    std::string schedulerType;
    uint32_t quantumCycles;
    uint32_t batchProcFreq;
    uint32_t minIns;
    uint32_t maxIns;
    uint32_t delayPerExec;

    // MO2 additions
    uint32_t maxMemory;
    uint32_t frameSize;
    uint32_t minMemPerProc;
    uint32_t maxMemPerProc;
    std::string diskFile;
};

bool loadConfig(const std::string& filename, Config& config);