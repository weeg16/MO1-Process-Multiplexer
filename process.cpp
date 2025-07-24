#include "memory_manager.h"
#include "process.h"
#include "util.h"
#include "instruction_random.h"

#include <iomanip>
#include <ctime>
#include <iostream>     
#include <thread>  
#include <chrono>     
#include <sstream>
#include <vector>

Process::Process(const std::string& name, int id, int totalIns, int memorySize)
    : name(name), id(id), totalInstructions(totalIns), executedInstructions(0), assignedCore(-1), memorySize(memorySize) {
    int memPerFrame = MemoryManager::getInstance().getFrameSize(); // Add getter if needed
    int numPages = (memorySize + memPerFrame - 1) / memPerFrame;
    for (int vpn = 0; vpn < numPages; ++vpn) {
        pageTable[vpn].frameNo = -1;
        pageTable[vpn].valid = false;
    }
    
    // Set timestamp
    std::time_t now = std::time(nullptr);
    char buf[100];
    std::strftime(buf, sizeof(buf), "%m/%d/%Y %I:%M:%S %p", std::localtime(&now));
    timestamp = buf;

    instructions = generateInstructionSet(name, totalIns);
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
    // --- Begin Paging Simulation ---
    // Let's say each instruction accesses the page corresponding to instructionPointer / 2 (for demo)
    int memPerFrame = MemoryManager::getInstance().getFrameSize();
    int virtualPageNo = instructionPointer / 2;

    MemoryManager& memoryManager = MemoryManager::getInstance();
    memoryManager.accessPage(this, virtualPageNo);
    // --- End Paging Simulation ---
    
    std::cout << "[Paging] Process " << name << " accessing VPN " << virtualPageNo << "\n";

    switch (ins.type) {
        case InstructionType::PRINT:
            logPrint(ins.args[0]);
            break;
        case InstructionType::DECLARE:
            variables[ins.args[0]] = static_cast<uint16_t>(std::stoi(ins.args[1]));
            break;
        case InstructionType::ADD: {
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
        default: break;
    }
}

bool Process::executeNextInstruction() {
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
