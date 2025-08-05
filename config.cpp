/* 
config.cpp

Implements configuration file parsing for loading scheduler and process settings
from config.txt into a Config struct.
*/

#include "config.h"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <iostream>

bool loadConfig(const std::string& filename, Config& config) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "[ERROR] Failed to open config file: " << filename << "\n"; 
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string key;
        iss >> key;

        if (key == "num-cpu") iss >> config.numCPU;
        else if (key == "scheduler") {
            std::string raw;
            iss >> raw;

            // Remove surrounding quotes manually (if any)
            if (!raw.empty() && raw.front() == '"' && raw.back() == '"') {
                raw = raw.substr(1, raw.size() - 2);
            }

            std::transform(raw.begin(), raw.end(), raw.begin(), ::tolower);
            config.schedulerType = raw;

        }
        else if (key == "quantum-cycles") iss >> config.quantumCycles;
        else if (key == "batch-process-freq") iss >> config.batchProcFreq;
        else if (key == "min-ins") iss >> config.minIns;
        else if (key == "max-ins") iss >> config.maxIns;
        else if (key == "delay-per-exec") iss >> config.delayPerExec;

        else if (key == "max-overall-mem") iss >> config.maxMemory;
        else if (key == "mem-per-frame") iss >> config.frameSize;
        else if (key == "min-mem-per-proc") iss >> config.minMemPerProc;
        else if (key == "max-mem-per-proc") iss >> config.maxMemPerProc;
        else if (key == "disk-file") iss >> config.diskFile;

    }

    return true;
}
