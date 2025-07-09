/* 
process.h

Declares the Process class, instruction data structures, and function signatures for
process state, instruction execution, and process screen interaction.
*/

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
    FOR
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
    bool isFinished() const;
    void logPrint(const std::string& message);

    std::vector<Instruction> instructions;
    size_t instructionPointer = 0;
    std::unordered_map<std::string, uint16_t> variables;

    bool executeNextInstruction();

    int sleepTicks = 0;
    std::vector<std::tuple<size_t, size_t, int>> forStack; 

    void executeSingleInstruction(const Instruction& ins);
};

void enterProcessScreen(Process* proc);
void printProcessInfo(const Process* proc);
bool processIsActive(const Process* proc);
