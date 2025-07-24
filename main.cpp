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
                
                MemoryManager::getInstance().configure(config.maxOverallMem, config.memPerFrame);

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
            std::istringstream iss(command.substr(10));
            std::string pname;
            int memSize;
            iss >> pname >> memSize;

            if (memSize < 64 || memSize > 65536 || (memSize & (memSize - 1)) != 0) {
                std::cout << "\n[ERROR] Invalid memory allocation. Must be power of 2 and between 64-65536 bytes.\n";
                std::this_thread::sleep_for(std::chrono::seconds(2));
                clearScreen();
                printHeader();
                continue;
            }

            Process* existing = coreManager.getProcessByName(pname);
            if (existing != nullptr) {
                std::cout << "\n[ERROR] Process '" << pname << "' already exists. Use 'screen -r " << pname << "' to reattach.\n";
                std::this_thread::sleep_for(std::chrono::seconds(2));
                clearScreen();
                printHeader();
            } else {
                Process* newProc = coreManager.spawnNewNamedProcess(pname, memSize);
                enterProcessScreen(newProc);
            }
        }
        else if (command.rfind("screen -r ", 0) == 0 && schedulerStarted) {
            std::string pname = command.substr(10);
            Process* proc = coreManager.getProcessByName(pname);
            if (proc && !proc->isFinished()) {
                enterProcessScreen(proc);
            } else {
                std::cout << "\nProcess " << pname << " not found or has finished.\n";
                std::this_thread::sleep_for(std::chrono::seconds(2));
                clearScreen();
                printHeader();
            }
        }
        else if (command == "process-smi") {
            std::cout << "\n[INFO] process-smi recognized (feature coming soon).\n";
        }
        else if (command == "vmstat") {
            std::cout << "\n[INFO] vmstat recognized (feature coming soon).\n";
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
