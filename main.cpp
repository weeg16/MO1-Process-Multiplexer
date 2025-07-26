/* 
main.cpp

Entry point and main loop for the OS Emulator. Handles command-line user interface,
menu logic, command parsing, and overall program flow.
*/

#include "config.h"
#include "util.h"
#include "screen.h"
#include "process.h"
#include "core_manager.h"
#include "memory_manager.h"

#include <algorithm>
#include <sstream>
#include <iostream>
#include <string>
#include <thread>
#include <fstream>

CoreManager coreManager;
bool schedulerStarted = false;
bool isInitialized = false;

int main() {
    std::string command;
    Config config;
    bool isRunning = true;

    clearScreen();
    printHeader();

    while (isRunning) {
        std::cout << "Enter a command: ";
        std::getline(std::cin, command);

        if (command == "clear") {
            clearScreen();
            printHeader();
        }
        else if (command == "exit") {
            if (schedulerStarted) {
                coreManager.stopScheduler();  
            }
            std::cout << "\nExiting...\n\n";
            isRunning = false;
        }
        else if (command == "initialize") {
            if (loadConfig("config.txt", config)) {
                coreManager.configure(
                    config.numCPU,
                    config.schedulerType,
                    config.quantumCycles,
                    config.batchProcFreq,
                    config.minIns,
                    config.maxIns,
                    config.delayPerExec
                );

                std::cout << "\n[OK] Configuration loaded.\n\n";

                // Init memory manager
                MemoryManager::getInstance().init(config.maxMemory, config.minMemPerProc, config.frameSize);

                /*
                
                NOTE:

                If you want:
                    (1) More page swaps and faults: Lower mem-per-frame or total memory.

                    (2) More processes at once: Raise max-overall-mem.

                    (3) Stricter memory fit: Raise min-mem-per-proc so that only 1 or 2 processes fit at a time.

                */


                // Create backing store file if not exists
                std::ifstream testFile(config.diskFile);
                if (!testFile.good()) {
                    std::ofstream createFile(config.diskFile);
                    if (createFile.is_open()) {
                        std::cout << "[INFO] Created backing store file: " << config.diskFile << "\n";
                        createFile.close();
                    } else {
                        std::cerr << "[ERROR] Failed to create backing store file.\n";
                    }
                } else {
                    std::cout << "[INFO] Backing store file found: " << config.diskFile << "\n";
                }

                std::this_thread::sleep_for(std::chrono::seconds(2));
                clearScreen();
                printHeader();
                isInitialized = true;
                schedulerStarted = false;
            } else {
                std::cout << "\n[ERROR] Failed to load config.txt.\n\n";
                std::this_thread::sleep_for(std::chrono::seconds(2));
                clearScreen();
                printHeader();
            }
        }
        else if (command == "scheduler-start") {
            if (!isInitialized) {
                std::cout << "\n[WARN] Please run 'initialize' first.\n\n";
                std::this_thread::sleep_for(std::chrono::seconds(2));
                clearScreen();
                printHeader();
                continue;
            }

            /* 
            to see mmore page activity, you can: 
                (1) increase max-ins in config.txt, 
                (2) lower mem-per-frame so fewer instructions fit in one page, OR 
                (3) generate more READ / WRITE instructions with broad address ranges 
            */

            if (!schedulerStarted) {
                std::cout << "\n[INFO] Starting " << config.schedulerType << " Scheduler...\n\n";
                std::this_thread::sleep_for(std::chrono::seconds(2));
                clearScreen();
                printHeader();

                coreManager.start();       
                coreManager.startSchedulerThread(config);     
                schedulerStarted = true;
            } else {
                std::cout << "\n[WARN] Scheduler is already running.\n\n";
                std::this_thread::sleep_for(std::chrono::seconds(2));
                clearScreen();
                printHeader();
            }
        }
        else if (command == "scheduler-stop") {
            if (schedulerStarted) {
                coreManager.stopSchedulerThread();
                coreManager.pauseCores();
                schedulerStarted = false;
            } else {
                std::cout << "\n[WARN] Scheduler is not running.\n";
                std::this_thread::sleep_for(std::chrono::seconds(2));
                clearScreen();
                printHeader();
            }
        }
        else if (command == "report-util") {
            std::ofstream file("csopesy-log.txt");
            coreManager.printProcessSummary(file, false);
            file.close();
            std::cout << "\n[INFO] Report generated at csopesy-log.txt!\n";
            std::this_thread::sleep_for(std::chrono::seconds(2));
            clearScreen();
            printHeader();
        }
        else if (command == "screen -ls") {
            coreManager.printProcessSummary(std::cout, true);
        }
        else if (command.rfind("screen -s ", 0) == 0 && schedulerStarted) {
            std::string remaining = command.substr(10); // Skip "screen -s "
            std::istringstream iss(remaining);
            std::string pname;
            int mem;

            iss >> pname >> mem;

            if (pname.empty() || iss.fail()) {
                std::cout << "\n[ERROR] Invalid syntax. Usage: screen -s <name> <mem>\n";
                std::this_thread::sleep_for(std::chrono::seconds(2));
                clearScreen();
                printHeader();
                continue;
            }

            // Validate memory
            if (mem < 64 || mem > 65536 || (mem & (mem - 1)) != 0) {
                std::cout << "\n[ERROR] Invalid memory size. Must be power of 2 between 64 and 65536 bytes.\n";
                std::this_thread::sleep_for(std::chrono::seconds(2));
                clearScreen();
                printHeader();
                continue;
            }

            if (coreManager.getProcessByName(pname)) {
                std::cout << "\n[ERROR] Process '" << pname << "' already exists.\n";
                std::this_thread::sleep_for(std::chrono::seconds(2));
                clearScreen();
                printHeader();
                continue;
            }

            Process* newProc = coreManager.spawnNewNamedProcess(pname, mem);
            enterProcessScreen(newProc);
        }
        else if (command.rfind("screen -r ", 0) == 0 && schedulerStarted) {
            std::string pname = command.substr(10);
            Process* proc = coreManager.getProcessByName(pname);

            if (!proc) {
                std::cout << "\nProcess " << pname << " not found.\n";
                std::this_thread::sleep_for(std::chrono::seconds(2));
                clearScreen();
                printHeader();
            } else if (proc->memoryViolation) {
                clearScreen();
                printHeader();
                std::cout << "Process " << proc->name
                        << " shut down due to memory access violation error that occurred at "
                        << proc->violationTime << ". 0x"
                        << std::hex << std::uppercase << proc->violationAddress
                        << " invalid.\n";
                std::cout << "\nPress ENTER to return to menu...\n";
                std::string pause;
                std::getline(std::cin, pause);
                clearScreen();
                printHeader();
            } else {
                enterProcessScreen(proc); // normal log view for finished or running process
            }
        }
        else if (command.rfind("screen -c ", 0) == 0 && schedulerStarted) {
            std::string rest = command.substr(10); // Skip "screen -c "

            size_t firstSpace = rest.find(' ');
            size_t secondSpace = rest.find(' ', firstSpace + 1);

            if (firstSpace == std::string::npos || secondSpace == std::string::npos) {
                std::cout << "\n[ERROR] Invalid syntax. Usage: screen -c <name> <mem> \"<instructions>\"\n";
                std::this_thread::sleep_for(std::chrono::seconds(2));
                clearScreen();
                printHeader();
                continue;
            }

            std::string pname = rest.substr(0, firstSpace);
            int mem = std::stoi(rest.substr(firstSpace + 1, secondSpace - firstSpace - 1));
            std::string instrStr = rest.substr(secondSpace + 1);

            if (instrStr.front() == '"' && instrStr.back() == '"') {
                instrStr = instrStr.substr(1, instrStr.size() - 2);
            }

            // std::replace(instrStr.begin(), instrStr.end(), ';', '\n');

            std::string debugInstrStr = instrStr;
            std::replace(debugInstrStr.begin(), debugInstrStr.end(), ';', '\n');
            std::cout << "[DEBUG] Parsed instrStr:\n" << debugInstrStr << "\n";

            if (mem < 64 || mem > 65536 || (mem & (mem - 1)) != 0) {
                std::cout << "\n[ERROR] Invalid memory size. Must be power of 2 between 64 and 65536 bytes.\n";
                std::this_thread::sleep_for(std::chrono::seconds(2));
                clearScreen();
                printHeader();
                continue;
            }

            if (coreManager.getProcessByName(pname)) {
                std::cout << "\n[ERROR] Process '" << pname << "' already exists.\n";
                std::this_thread::sleep_for(std::chrono::seconds(2));
                clearScreen();
                printHeader();
                continue;
            }

            auto parsed = parseInstructions(instrStr);
            if (parsed.empty()) {
                std::cout << "\n[ERROR] Invalid instruction format.\n";
                std::this_thread::sleep_for(std::chrono::seconds(2));
                clearScreen();
                printHeader();
                continue;
            }
            if (parsed.size() > 50) {
                std::cout << "\n[ERROR] Too many instructions (max 50).\n";
                std::this_thread::sleep_for(std::chrono::seconds(2));
                clearScreen();
                printHeader();
                continue;
            }

            Process* proc = new Process(pname, 9999, parsed.size(), mem);  // use dummy ID
            proc->instructions = parsed;
            coreManager.addProcess(proc);
            enterProcessScreen(proc);
        }
        else {
            std::cout << "\nUnrecognized command.\n\n";
            std::this_thread::sleep_for(std::chrono::seconds(1));
            clearScreen();
            printHeader();
        }
    }

    return 0;
}
