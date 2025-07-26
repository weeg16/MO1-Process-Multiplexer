/* 
process.h

Declares the Process class, instruction data structures, and function signatures for
process state, instruction execution, and process screen interaction.
*/

#include "paging.h"

#pragma once
#include <string>
#include <atomic>
#include <ctime>
#include <vector>
#include <unordered_map>

enum class InstructionType {
    PRINT,
    DECLARE,
    ADD,
    SUBTRACT,
    SLEEP,
    FOR,
    READ,
    WRITE
};

struct Instruction {
    InstructionType type;
    std::vector<std::string> args;
    std::vector<Instruction> block;
};


class Process {
public:
    std::string name;
    int id;
    int totalInstructions;
    std::atomic<int> executedInstructions;
    int assignedCore;
    int memStart = -1;
    int memEnd = -1;
    bool inMemory = false;

    std::string timestamp;
    std::vector<std::string> logs;
    int tickWaitCounter = 0;

    Process(const std::string& name, int id, int totalIns);
    Process(const std::string& name, int id, int totalIns, int requestedMem);

    bool isFinished() const;
    void logPrint(const std::string& message);

    std::vector<Instruction> instructions;
    size_t instructionPointer = 0;
    std::unordered_map<std::string, uint16_t> variables;
    std::unordered_map<int, uint16_t> memory;  // Simulated memory (address → value)

    bool executeNextInstruction();

    int sleepTicks = 0;
    std::vector<std::tuple<size_t, size_t, int>> forStack; 

    void executeSingleInstruction(const Instruction& ins);

    std::vector<PageTableEntry> pageTable;
    
    int requestedMemory = 0;  // In bytes

    bool memoryViolation = false;
    uint32_t violationAddress = 0;
    std::string violationTime;
    bool isAddressValid(uint32_t addr) const;
    
};

void enterProcessScreen(Process* proc);
void printProcessInfo(const Process* proc);
bool processIsActive(const Process* proc);
