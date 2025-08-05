/* 
util.cpp

Implements miscellaneous utility functions (timestamp generation, color output, 
input handling) used throughout the OS Emulator project.
*/

#include "process.h"
#include "memory_manager.h"
#include "util.h"

#include <sstream>
#include <cctype>
#include <iostream>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif
#else
#include <unistd.h>
#endif

#define ORANGE "\033[38;5;208m"
#define BLUE   "\033[34m"
#define YELLOW "\033[33m"
#define GREEN  "\033[32m"
#define RED    "\033[31m"
#define RESET  "\033[0m"

std::string getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm timeinfo = *std::localtime(&t);
    char buffer[100];
    std::strftime(buffer, sizeof(buffer), "%m/%d/%Y %I:%M:%S %p", &timeinfo);
    return std::string(buffer);
}

void printHeader() {
    #ifdef _WIN32
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD dwMode = 0;
        GetConsoleMode(hOut, &dwMode);
        dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        SetConsoleMode(hOut, dwMode);
    #endif

    std::cout << "_________________________________________________________________\n";
    std::cout << "      __       __       __     ____     _____      __     _     _\n";
    std::cout << "    /    )   /    )   /    )   /    )   /    '   /    )   |    /\n";
    std::cout << "---/---------\\-------/----/---/____/---/__-------\\--------|---/--\n";
    std::cout << "  /           \\     /    /   /        /           \\       |  /\n";
    std::cout << "_(____/___(____/___(____/___/________/____ ___(____/______|_/____\n";
    std::cout << "                                                           /\n";
    std::cout << "                                                       (_ /\n";
    std::cout << "\033[1;32mHello, welcome to CSOPESY Emulator!\033[0m\n";
    std::cout << "\033[1;33mType 'exit' to quit, 'clear' to clear the screen.\033[0m\n\n";
}

void clearScreen() {
#ifdef _WIN32
    system("cls"); // windows
#else
    system("clear"); // macos or linux 
#endif
}

void printColoredTimestamp(std::ostream& out, const std::string& ts) {
    // Works for "06/27/2025 02:18:26 PM" (no comma, single space before AM/PM)

    // Defensive: find "AM" or "PM"
    size_t ampos = ts.find("AM");
    size_t pmpos = ts.find("PM");
    size_t endpos = std::string::npos;

    if (ampos != std::string::npos) endpos = ampos + 2;
    else if (pmpos != std::string::npos) endpos = pmpos + 2;
    else endpos = ts.length();

    std::string t = ts.substr(0, endpos);

    size_t p = 0;
    out << BLUE << "(" << RESET; p++;
    size_t first_slash = t.find('/', p);
    out << ORANGE << t.substr(p, first_slash - p) << RESET; p = first_slash;
    out << BLUE << "/" << RESET; p++;
    size_t second_slash = t.find('/', p);
    out << ORANGE << t.substr(p, second_slash - p) << RESET; p = second_slash;
    out << BLUE << "/" << RESET; p++;
    size_t year_end = t.find(' ', p);
    out << ORANGE << t.substr(p, year_end - p) << RESET; p = year_end;
    out << " "; p++;
    size_t colon1 = t.find(':', p);
    out << ORANGE << t.substr(p, colon1 - p) << RESET; p = colon1;
    out << BLUE << ":" << RESET; p++;
    size_t colon2 = t.find(':', p);
    out << ORANGE << t.substr(p, colon2 - p) << RESET; p = colon2;
    out << BLUE << ":" << RESET; p++;
    size_t space_pm = t.find(' ', p);
    out << ORANGE << t.substr(p, space_pm - p) << RESET; p = space_pm;
    out << " " << ORANGE << t.substr(p + 1, 2) << RESET;
    // advance p to after "AM" or "PM"
    if (t.substr(p + 1, 2) == "AM" || t.substr(p + 1, 2) == "PM") p += 3;
    out << BLUE << ")" << RESET;
}

std::string to_string(InstructionType type) {
    switch (type) {
        case InstructionType::PRINT:    return "PRINT";
        case InstructionType::DECLARE:  return "DECLARE";
        case InstructionType::ADD:      return "ADD";
        case InstructionType::SUBTRACT: return "SUBTRACT";
        case InstructionType::SLEEP:    return "SLEEP";
        case InstructionType::FOR:      return "FOR";
        case InstructionType::READ:     return "READ";
        case InstructionType::WRITE:    return "WRITE";
        default:                        return "UNKNOWN";
    }
}

std::vector<Instruction> parseInstructions(const std::string& input) {
    std::cout << "[DEBUG] Raw input: " << input << "\n";
    std::vector<Instruction> result;
    std::istringstream ss(input);
    std::string line;

    while (std::getline(ss, line, ';')) {  // Split by semicolon
        // Trim whitespace
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);
        if (line.empty()) continue;

        // Extract first word (opcode) by skipping non-space, then stop at first space or '(', whichever comes first
        std::string opcode;
        size_t i = 0;
        while (i < line.size() && !isspace(line[i]) && line[i] != '(') {
            opcode += line[i];
            ++i;
        }
        // Now skip spaces after opcode
        while (i < line.size() && isspace(line[i])) ++i;
        // Rest of line is the args
        std::string args = line.substr(i);
        std::istringstream lineStream(args);

        // Trim opcode (optional)
        opcode.erase(0, opcode.find_first_not_of(" \t\r\n"));
        opcode.erase(opcode.find_last_not_of(" \t\r\n") + 1);

        std::cout << "[TRACE] opcode = '" << opcode << "'\n";

        if (opcode == "DECLARE") {
            std::string var; int val;
            lineStream >> var >> val;
            result.push_back({InstructionType::DECLARE, {var, std::to_string(val)}});
        }
        else if (opcode == "ADD" || opcode == "SUB") {
            std::string dest, a, b;
            lineStream >> dest >> a >> b;
            InstructionType type = (opcode == "ADD") ? InstructionType::ADD : InstructionType::SUBTRACT;
            result.push_back({type, {dest, a, b}});
        }
        else if (opcode == "SLEEP") {
            int ticks;
            lineStream >> ticks;
            result.push_back({InstructionType::SLEEP, {std::to_string(ticks)}});
        }
        else if (opcode == "READ") {
            std::string var, addr;
            lineStream >> var >> addr;
            result.push_back({InstructionType::READ, {var, addr}});
        }
        else if (opcode == "WRITE") {
            std::string addr, val;
            lineStream >> addr >> val;
            result.push_back({InstructionType::WRITE, {addr, val}});
        }
        else if (opcode == "PRINT") {
            std::string rest;
            std::getline(lineStream, rest);

            size_t open = rest.find('(');
            size_t close = rest.rfind(')');
            if (open == std::string::npos || close == std::string::npos || open >= close)
                return {};

            std::string content = rest.substr(open + 1, close - open - 1);
            size_t plusPos = content.find('+');

            if (plusPos == std::string::npos) {
                // Only literal string
                std::string literal = content;
                literal.erase(std::remove(literal.begin(), literal.end(), '\\'), literal.end());
                literal.erase(std::remove_if(literal.begin(), literal.end(), ::isspace), literal.end());

                if (literal.front() == '"' && literal.back() == '"') {
                    literal = literal.substr(1, literal.size() - 2);
                } else {
                    return {}; // invalid string literal
                }

                result.push_back({InstructionType::PRINT, {literal}});
            } else {
                // Literal + variable
                std::string literal = content.substr(0, plusPos);
                std::string var = content.substr(plusPos + 1);

                literal.erase(std::remove(literal.begin(), literal.end(), '\\'), literal.end());
                literal.erase(std::remove_if(literal.begin(), literal.end(), ::isspace), literal.end());
                var.erase(std::remove_if(var.begin(), var.end(), ::isspace), var.end());

                if (literal.front() == '"' && literal.back() == '"') {
                    literal = literal.substr(1, literal.size() - 2);
                } else {
                    return {}; // invalid string literal
                }

                result.push_back({InstructionType::PRINT, {literal, var}});
            }
        }
    }

    std::cout << "[DEBUG] Successfully parsed " << result.size() << " instructions.\n";
    return result;
}

bool ensureSymbolTableMapped(Process* proc) {
    int pageSize = MemoryManager::getInstance().getMemPerFrame();
    int symbolPageIndex = 0; // Assume symbol table is at virtual address 0x0000
    auto& pageEntry = proc->pageTable[symbolPageIndex];

    if (!pageEntry.valid) {
        return MemoryManager::getInstance().loadPage(proc, symbolPageIndex, MemoryManager::getInstance().getCurrentCycle());
    }
    return true;
}

std::string getCurrentTimeStr() {
    std::time_t now = std::time(nullptr);
    std::tm* t = std::localtime(&now);
    char buf[16];
    std::strftime(buf, sizeof(buf), "%H:%M:%S", t);
    return std::string(buf);
}

std::string formatBytes(int bytes) {
    std::ostringstream oss;
    if (bytes >= 1024 * 1024) {
        double mib = bytes / (1024.0 * 1024.0);
        oss << ORANGE << std::fixed << std::setprecision(2) << mib << " MiB" << RESET;
    } else if (bytes >= 1024) {
        double kib = bytes / 1024.0;
        oss << ORANGE << std::fixed << std::setprecision(2) << kib << " KiB" << RESET;
    } else {
        oss << ORANGE << bytes << " B" << RESET;
    }
    return oss.str();
}

std::string colorizeTag(const std::string& line) {
    std::ostringstream out;

    if (line.find("[INFO]") != std::string::npos)
        out << GREEN << line << RESET;
    else if (line.find("[WARN]") != std::string::npos)
        out << YELLOW << line << RESET;
    else if (line.find("[ERROR]") != std::string::npos)
        out << RED << line << RESET;
    else
        out << line;

    return out.str();
}
