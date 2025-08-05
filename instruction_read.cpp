#include "instruction_read.h"
#include <sstream>
#include <iomanip>
#include <cstdlib>

Instruction generateRead(const std::string& processName) {
    // Cap address to first 1 or 2 pages worth of memory
    int pageSize = 128;  // match your config.txt mem-per-frame
    int maxAddress = pageSize * 2 - 1;  // allow max 2 pages worth

    std::stringstream addr;
    addr << "0x" << std::hex << std::uppercase << (rand() % maxAddress);

    std::string varName = "r" + std::to_string(rand() % 10);
    return {InstructionType::READ, {varName, addr.str()}};
}
