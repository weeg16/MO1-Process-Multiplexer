#include "process.h"
#include "util.h"
#include "instruction_random.h"
#include "memory_manager.h"

#include <iomanip>
#include <ctime>
#include <iostream>     
#include <thread>  
#include <chrono>     
#include <sstream>
#include <vector>
#include <cmath>
#include <iomanip>

Process::Process(const std::string& name, int id, int totalIns)
    : Process(name, id, totalIns, MemoryManager::getInstance().getDefaultProcessMemory()) {}

Process::Process(const std::string& name, int id, int totalIns, int requestedMem)
    : name(name), id(id), totalInstructions(totalIns), executedInstructions(0),
      assignedCore(-1), requestedMemory(requestedMem) {

    std::time_t now = std::time(nullptr);
    char buf[100];
    std::strftime(buf, sizeof(buf), "%m/%d/%Y %I:%M:%S %p", std::localtime(&now));
    timestamp = buf;

    instructions = generateInstructionSet(name, totalIns);

    int memPerFrame = MemoryManager::getInstance().getMemPerFrame();
    int addressSpace = 4096;
    int numPages = (addressSpace + memPerFrame - 1) / memPerFrame;
    pageTable.reserve(numPages);
    for (int i = 0; i < numPages; ++i) {
        pageTable.push_back({i, -1, false, false, 0});
    }
}

bool Process::isFinished() const {
    return executedInstructions >= totalInstructions;
}

void Process::logPrint(const std::string& message) {
    std::ostringstream oss;
    oss << "(" << getCurrentTimestamp() << ") "
        << "Core:" << assignedCore << " \"" << message << "\"";
    logs.push_back(oss.str());
}

void Process::executeSingleInstruction(const Instruction& ins) {
    switch (ins.type) {
        case InstructionType::PRINT: {
            if (ins.args.size() == 1) {
                logPrint(ins.args[0]);
            } else if (ins.args.size() == 2) {
                if (!ensureSymbolTableMapped(this)) {
                    logPrint("Page fault on PRINT");
                    return;
                }
                const std::string& prefix = ins.args[0];
                const std::string& var = ins.args[1];
                uint16_t value = 0;
                if (variables.count(var)) value = variables[var];
                logPrint(prefix + std::to_string(value));
            }
            break;
        }

        case InstructionType::DECLARE:
            if (!ensureSymbolTableMapped(this)) {
                logPrint("Page fault on DECLARE");
                return;
            }
            variables[ins.args[0]] = static_cast<uint16_t>(std::stoi(ins.args[1]));
            break;

        case InstructionType::ADD: {
            if (!ensureSymbolTableMapped(this)) {
                logPrint("Page fault on ADD");
                return;
            }
            uint16_t v2 = 0, v3 = 0;
            if (variables.count(ins.args[1])) v2 = variables[ins.args[1]];
            else try { v2 = static_cast<uint16_t>(std::stoi(ins.args[1])); } catch (...) {}
            if (variables.count(ins.args[2])) v3 = variables[ins.args[2]];
            else try { v3 = static_cast<uint16_t>(std::stoi(ins.args[2])); } catch (...) {}
            uint32_t sum = v2 + v3;
            if (sum > 65535) sum = 65535;
            variables[ins.args[0]] = static_cast<uint16_t>(sum);
            break;
        }

        case InstructionType::SUBTRACT: {
            if (!ensureSymbolTableMapped(this)) {
                logPrint("Page fault on SUBTRACT");
                return;
            }
            uint16_t v2 = 0, v3 = 0;
            if (variables.count(ins.args[1])) v2 = variables[ins.args[1]];
            else try { v2 = static_cast<uint16_t>(std::stoi(ins.args[1])); } catch (...) {}
            if (variables.count(ins.args[2])) v3 = variables[ins.args[2]];
            else try { v3 = static_cast<uint16_t>(std::stoi(ins.args[2])); } catch (...) {}
            int diff = v2 - v3;
            if (diff < 0) diff = 0;
            variables[ins.args[0]] = static_cast<uint16_t>(diff);
            break;
        }

        case InstructionType::SLEEP:
            sleepTicks = std::stoi(ins.args[0]);
            break;

        case InstructionType::READ: {
            std::string varName = ins.args[0];
            std::string addrStr = ins.args[1];

            int address = 0;
            try {
                address = std::stoi(addrStr, nullptr, 16);
            } catch (std::invalid_argument&) {
                memoryViolation = true;
                violationAddress = 0xFFFFFFFF;
                violationTime = getCurrentTimeStr();
                // logPrint("Memory access error: Invalid address format.");
                return;
            }

            if (!isAddressValid(address)) {
                memoryViolation = true;
                violationAddress = address;
                violationTime = getCurrentTimeStr();
                return;
            }

            int pageSize = MemoryManager::getInstance().getMemPerFrame();
            int pageIndex = address / pageSize;
            auto& pageEntry = pageTable[pageIndex];

            if (!pageEntry.valid) {
                if (!MemoryManager::getInstance().loadPage(this, pageIndex, MemoryManager::getInstance().getCurrentCycle())) {
                    // logPrint("Page fault could not be resolved. Skipping READ.");
                    return;
                }
            }

            uint16_t value = memory.count(address) ? memory[address] : 0;
            variables[varName] = value;

            std::ostringstream oss;
            oss << "READ " << addrStr << " → " << varName << " = " << value;
            // logPrint(oss.str());
            break;
        }

        case InstructionType::WRITE: {
            std::string addrStr = ins.args[0];
            std::string valStr = ins.args[1];

            int address = 0;
            try {
                address = std::stoi(addrStr, nullptr, 16);
            } catch (std::invalid_argument&) {
                memoryViolation = true;
                violationAddress = 0xFFFFFFFF;
                violationTime = getCurrentTimeStr();
                // logPrint("Memory access error: Invalid address format.");
                return;
            }

            // Address validation first!
            if (!isAddressValid(address)) {
                memoryViolation = true;
                violationAddress = address;
                violationTime = getCurrentTimeStr();
                return;
            }

            int value = 0;
            try {
                if (variables.count(valStr)) {
                    value = variables[valStr];
                } else {
                    value = std::stoi(valStr);
                }
            } catch (std::invalid_argument&) {
                memoryViolation = true;
                violationAddress = address;
                violationTime = getCurrentTimeStr();
                // logPrint("Memory access error: Invalid value format.");
                return;
            }

            int pageSize = MemoryManager::getInstance().getMemPerFrame();
            int pageIndex = address / pageSize;
            auto& pageEntry = pageTable[pageIndex];

            if (!pageEntry.valid) {
                if (!MemoryManager::getInstance().loadPage(this, pageIndex, MemoryManager::getInstance().getCurrentCycle())) {
                    // logPrint("Page fault could not be resolved. Skipping WRITE.");
                    return;
                }
            }

            if (value < 0) value = 0;
            if (value > 65535) value = 65535;

            memory[address] = static_cast<uint16_t>(value);
            pageEntry.dirty = true;

            std::ostringstream oss;
            oss << "WRITE " << addrStr << " = " << value;
            // logPrint(oss.str());
            break;
        }

        default: break;
    }
}

bool Process::executeNextInstruction() {
    // if (instructionPointer < instructions.size()) {
    //     std::cout << "[TRACE] " << name << " executing instruction " << instructionPointer // FOR DEBUGGING
    //             << " (" << to_string(instructions[instructionPointer].type) << ")\n";
    // }

    if (sleepTicks > 0) {
        --sleepTicks;
        return true;
    }

    if (!forStack.empty()) {
        auto& tup = forStack.back();
        size_t& instrIdx = std::get<0>(tup);
        size_t& blockPtr = std::get<1>(tup);
        int& left = std::get<2>(tup);
        Instruction& forIns = instructions[instrIdx];
        if (blockPtr < forIns.block.size()) {
            Instruction& curr = forIns.block[blockPtr];
            executeSingleInstruction(curr);
            ++blockPtr;
            ++executedInstructions;
        } else if (left > 1) {
            blockPtr = 0;
            --left;
        } else {
            forStack.pop_back();
            ++instructionPointer;
            ++executedInstructions;
        }
        return true;
    }

    if (instructionPointer >= instructions.size()) return false;

    // std::cout << "[DEBUG] Process " << name << " at instruction " << instructionPointer << " of " << instructions.size() << "\n"; // FOR DEBUGGING

    Instruction& ins = instructions[instructionPointer];

    if (ins.type == InstructionType::FOR) {
        int repeats = std::stoi(ins.args[0]);
        forStack.push_back(std::make_tuple(instructionPointer, 0, repeats));
        return true;
    } else {
        executeSingleInstruction(ins);
        ++instructionPointer;
        ++executedInstructions;
        return true;
    }
}

bool Process::isAddressValid(uint32_t addr) const {
    return addr < (uint32_t)requestedMemory;
}