/* 
util.cpp

Implements miscellaneous utility functions (timestamp generation, color output, 
input handling) used throughout the OS Emulator project.
*/

#include "process.h"
#include "util.h"
#include <iostream>
#include <chrono>
#include <ctime>
#include <iomanip>

#ifdef _WIN32
#include <windows.h>
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif
#else
#include <unistd.h>
#endif

#define ORANGE "\033[38;5;208m"
#define BLUE "\033[34m"
#define RESET "\033[0m"

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