/* 
main.cpp
- Cores start after initialize
- Manual screen commands do not require scheduler-start
- Scheduler-start only handles batch generation
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
                coreManager.stopSchedulerThread();
            }
            coreManager.stopScheduler();  // Stop CPU cores
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

                MemoryManager::getInstance().init(
                    config.maxMemory,
                    config.minMemPerProc,
                    config.frameSize
                );

                std::ofstream truncateFile(config.diskFile, std::ios::trunc);
                if (truncateFile.is_open()) {
                    std::cout << colorizeTag("[INFO] Backing store reset (file truncated).") << "\n";
                    truncateFile.close();
                } else {
                    std::cerr << colorizeTag("[ERROR] Failed to reset backing store.") << "\n";
                }

                std::ifstream testFile(config.diskFile);
                if (!testFile.good()) {
                    std::ofstream createFile(config.diskFile);
                    if (createFile.is_open()) {
                        std::cout << colorizeTag("[INFO] Created backing store file: " + config.diskFile) << "\n";
                        createFile.close();
                    } else {
                        std::cerr << colorizeTag("[ERROR] Failed to create backing store file.") << "\n";
                    }
                } else {
                    std::cout << colorizeTag("[INFO] Backing store file found: " + config.diskFile) << "\n";
                }

                std::cout << "\n[OK] Configuration loaded.\n";
                std::this_thread::sleep_for(std::chrono::seconds(2));

                coreManager.start();  // Immediately start execution cores

                clearScreen();
                printHeader();
                isInitialized = true;
            } else {
                std::cout << "\n" << colorizeTag("[ERROR] Failed to load config.txt.") << "\n\n";
                std::this_thread::sleep_for(std::chrono::seconds(2));
                clearScreen();
                printHeader();
            }
        }
        else if (command == "scheduler-start") {
            if (!isInitialized) {
                std::cout << "\n" << colorizeTag("[WARN] Please run 'initialize' first.") << "\n\n";
                std::this_thread::sleep_for(std::chrono::seconds(2));
                clearScreen();
                printHeader();
                continue;
            }

            if (!schedulerStarted) {
                std::cout << colorizeTag("\n[INFO] Starting batch process generation...") << "\n\n";
                std::this_thread::sleep_for(std::chrono::seconds(2));
                clearScreen();
                printHeader();

                coreManager.startSchedulerThread(config);
                schedulerStarted = true;
            } else {
                std::cout << "\n" << colorizeTag("[WARN] Scheduler already running.") << "\n\n";
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
                std::cout << "\n" << colorizeTag("[WARN] Scheduler is not running.") << "\n";
                std::this_thread::sleep_for(std::chrono::seconds(2));
                clearScreen();
                printHeader();
            }
        }
        else if (command == "report-util") {
            std::ofstream file("csopesy-log.txt");
            coreManager.printProcessSummary(file, false);
            file.close();
            std::cout << colorizeTag("\n[INFO] Report generated at csopesy-log.txt!") << "\n";
            std::this_thread::sleep_for(std::chrono::seconds(2));
            clearScreen();
            printHeader();
        }
        else if (command == "screen -ls") {
            coreManager.printProcessSummary(std::cout, true);
            std::cout << "Press ENTER to return to menu...\n";
            std::string pause;
            std::getline(std::cin, pause);
            clearScreen();
            printHeader();
        }
        else if (command.rfind("screen -s ", 0) == 0 && isInitialized) {
            std::istringstream iss(command.substr(10));
            std::string pname;
            int mem;
            iss >> pname >> mem;

            if (pname.empty() || iss.fail() || mem < 64 || mem > 65536 || (mem & (mem - 1)) != 0) {
                std::cout << "\n" << colorizeTag("[ERROR] Invalid syntax or memory size.") << "\n";
                std::this_thread::sleep_for(std::chrono::seconds(2));
                clearScreen(); printHeader(); continue;
            }

            if (coreManager.getProcessByName(pname)) {
                std::cout << "\n" << colorizeTag("[ERROR] Process already exists.") << "\n";
                std::this_thread::sleep_for(std::chrono::seconds(2));
                clearScreen(); printHeader(); continue;
            }

            Process* newProc = coreManager.spawnNewNamedProcess(pname, mem);
            enterProcessScreen(newProc);
        }
        else if (command.rfind("screen -r ", 0) == 0 && isInitialized) {
            std::string pname = command.substr(10);
            Process* proc = coreManager.getProcessByName(pname);

            if (!proc) {
                std::cout << "\n[ERROR] Process not found.\n";
            } else if (proc->memoryViolation) {
                clearScreen(); printHeader();
                std::cout << "Process " << proc->name << " shut down due to memory violation at "
                          << proc->violationTime << ". Addr: 0x"
                          << std::hex << std::uppercase << proc->violationAddress << "\n";
            } else {
                enterProcessScreen(proc);
            }

            std::cout << "\nPress ENTER to return to menu...\n";
            std::string pause; std::getline(std::cin, pause);
            clearScreen(); printHeader();
        }
        else if (command.rfind("screen -c ", 0) == 0 && isInitialized) {
            std::string rest = command.substr(10);
            size_t firstSpace = rest.find(' ');
            size_t secondSpace = rest.find(' ', firstSpace + 1);
            if (firstSpace == std::string::npos || secondSpace == std::string::npos) {
                std::cout << "\n" << colorizeTag("[ERROR] Usage: screen -c <name> <mem> \"<instr>\"") << "\n";
                std::this_thread::sleep_for(std::chrono::seconds(2));
                clearScreen(); printHeader(); continue;
            }

            std::string pname = rest.substr(0, firstSpace);
            int mem = std::stoi(rest.substr(firstSpace + 1, secondSpace - firstSpace - 1));
            std::string instrStr = rest.substr(secondSpace + 1);

            if (instrStr.front() == '"' && instrStr.back() == '"')
                instrStr = instrStr.substr(1, instrStr.size() - 2);

            auto parsed = parseInstructions(instrStr);
            if (parsed.empty() || parsed.size() > 50) {
                std::cout << "\n" << colorizeTag("[ERROR] Invalid or too many instructions.") << "\n";
                std::this_thread::sleep_for(std::chrono::seconds(2));
                clearScreen(); printHeader(); continue;
            }

            if (coreManager.getProcessByName(pname)) {
                std::cout << "\n" << colorizeTag("[ERROR] Process already exists.") << "\n";
                std::this_thread::sleep_for(std::chrono::seconds(2));
                clearScreen(); printHeader(); continue;
            }

            Process* proc = new Process(pname, 9999, parsed.size(), mem);
            proc->instructions = parsed;
            coreManager.addProcess(proc);
            enterProcessScreen(proc);
        }
        else if (command == "process-smi" && isInitialized) {
            coreManager.printProcessSMI(std::cout, true);
            std::cout << "\nPress ENTER to return to menu...\n";
            std::string pause; std::getline(std::cin, pause);
            clearScreen(); printHeader();
        }
        else if (command == "vmstat" && isInitialized) {
            coreManager.printVMStat(std::cout);
            std::cout << "\nPress ENTER to return to menu...\n";
            std::string pause; std::getline(std::cin, pause);
            clearScreen(); printHeader();
        }
        else {
            std::cout << "\n" << colorizeTag("[ERROR] Unrecognized Command.") << "\n\n";
            std::this_thread::sleep_for(std::chrono::seconds(1));
            clearScreen(); printHeader();
        }
    }

    return 0;
}
