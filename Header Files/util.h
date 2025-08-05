/* 
util.h

Declares utility functions for console manipulation, timestamp generation,
and other helper utilities.
*/

#pragma once

#include <string>
#include "screen.h"
#include "process.h"

std::string getCurrentTimestamp();
void printHeader();
void clearScreen();
void drawScreen(const ConsoleScreen& screen);
void printColoredTimestamp(std::ostream& out, const std::string& ts);
std::string to_string(InstructionType type);
std::vector<Instruction> parseInstructions(const std::string& input);
bool ensureSymbolTableMapped(class Process* proc);
std::string getCurrentTimeStr();
std::string formatBytes(int bytes);
std::string colorizeTag(const std::string& line);
