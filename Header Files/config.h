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

    bool pagingMode;
    std::string replacementPolicy;
    int maxOverallMem;
    int memPerFrame;
    int memPerProc;
};

bool loadConfig(const std::string& filename, Config& config);