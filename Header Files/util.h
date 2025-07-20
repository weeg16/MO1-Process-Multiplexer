/* 
util.h

Declares utility functions for console manipulation, timestamp generation,
and other helper utilities.
*/

#pragma once

#include <cstdint>
#include <string>
#include "screen.h"

std::string getCurrentTimestamp();
uint64_t getCurrentCycle();
void printHeader();
void clearScreen();
void drawScreen(const ConsoleScreen& screen);
void printColoredTimestamp(std::ostream& out, const std::string& ts);