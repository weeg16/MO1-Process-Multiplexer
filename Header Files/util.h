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