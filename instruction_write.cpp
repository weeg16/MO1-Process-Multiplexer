#include "instruction_write.h"
#include <sstream>
#include <iomanip>
#include <cstdlib>

Instruction generateWrite(const std::string& processName) {
    std::stringstream addr;
    addr << "0x" << std::hex << std::uppercase << (rand() % 4096);  // Simulate 0x0000–0x0FFF

    int value = rand() % 65536;  // 0–65535 for uint16
    return {InstructionType::WRITE, {addr.str(), std::to_string(value)}};
}
