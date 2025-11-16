#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <windows.h>
#include <shellapi.h>
#include <tlhelp32.h>
#include <vector>
#include <algorithm>
#include <tlhelp32.h>

void killAllChrome() {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return;

    PROCESSENTRY32 entry;
    entry.dwSize = sizeof(PROCESSENTRY32);

    if (Process32First(snapshot, &entry)) {
        do {
            if (_stricmp(entry.szExeFile, "chrome.exe") == 0) {
                HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, entry.th32ProcessID);
                if (hProcess) {
                    TerminateProcess(hProcess, 0);
                    CloseHandle(hProcess);
                }
            }
        } while (Process32Next(snapshot, &entry));
    }
    CloseHandle(snapshot);
}

void openUrlInChromeKiosk(const std::string& url) {
    std::vector<std::string> candidates = {
        "C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe",
        "C:\\Program Files (x86)\\Google\\Chrome\\Application\\chrome.exe"
    };

    std::string chromePath;
    for (const auto& p : candidates) {
        DWORD attrs = GetFileAttributesA(p.c_str());
        if (attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
            chromePath = p;
            break;
        }
    }

    if (chromePath.empty()) {
        std::cout << "[WARN] Chrome not found. Opening with default browser." << std::endl;
        ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        return;
    }

    // Kill all existing Chrome processes before opening new kiosk window
    killAllChrome();

    // Launch new Chrome instance in kiosk mode
    std::string params = "--new-window --kiosk \"" + url + "\"";

    HINSTANCE result = ShellExecuteA(nullptr, "open", chromePath.c_str(), params.c_str(), nullptr, SW_SHOWNORMAL);
    if ((INT_PTR)result <= 32) {
        std::cout << "[ERROR] ShellExecute failed (code " << (INT_PTR)result << "). Falling back to default browser." << std::endl;
        ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    }
}

// Trim helper
static inline std::string trim(const std::string& s) {
    auto start = s.find_first_not_of(" \r\n\t");
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(" \r\n\t");
    return s.substr(start, end - start + 1);
}

int main() {
    std::string portName = "\\\\.\\COM3";  // CHANGE COM PORT HERE
    HANDLE hSerial;

    std::cout << "[INFO] Starting AutoCount Arduino on " << portName << "..." << std::endl;


    while (true) {
        hSerial = CreateFileA(portName.c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, 0, nullptr);
        if (hSerial == INVALID_HANDLE_VALUE) {
            std::cout << "[ERROR] Could not open " << portName << ". Retrying in 5s..." << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(5));
            continue;
        }

        DCB dcbSerialParams = {0};
        dcbSerialParams.DCBlength = sizeof(dcbSerialParams);
        if (!GetCommState(hSerial, &dcbSerialParams)) {
            std::cout << "[ERROR] Failed to get COM state." << std::endl;
            CloseHandle(hSerial);
            std::this_thread::sleep_for(std::chrono::seconds(5));
            continue;
        }

        dcbSerialParams.BaudRate = CBR_9600;
        dcbSerialParams.ByteSize = 8;
        dcbSerialParams.StopBits = ONESTOPBIT;
        dcbSerialParams.Parity = NOPARITY;
        if (!SetCommState(hSerial, &dcbSerialParams)) {
            std::cout << "[ERROR] Failed to set COM parameters." << std::endl;
            CloseHandle(hSerial);
            std::this_thread::sleep_for(std::chrono::seconds(5));
            continue;
        }

        COMMTIMEOUTS timeouts = {0};
        timeouts.ReadIntervalTimeout = 50;
        timeouts.ReadTotalTimeoutConstant = 50;
        timeouts.ReadTotalTimeoutMultiplier = 10;
        SetCommTimeouts(hSerial, &timeouts);

        std::cout << "[INFO] Connected to " << portName << std::endl;

        char buffer[256];
        DWORD bytesRead;
        std::string line = "";

        while (true) {
            if (!ReadFile(hSerial, buffer, sizeof(buffer) - 1, &bytesRead, nullptr)) {
                std::cout << "[WARN] Lost connection. Retrying..." << std::endl;
                break;
            }

            if (bytesRead > 0) {
                buffer[bytesRead] = '\0';
                line += buffer;

                size_t pos;
                while ((pos = line.find('\n')) != std::string::npos) {
                    std::string msg = trim(line.substr(0, pos));
                    line.erase(0, pos + 1);

                    if (!msg.empty()) {
                        if (msg.find("http") != std::string::npos) {
                            std::cout << "[OPEN] " << msg << std::endl;
                            openUrlInChromeKiosk(msg);
                        } else {
                            std::cout << "[SERIAL] " << msg << std::endl;
                        }
                    }
                }
            }
        }

        CloseHandle(hSerial);
        std::this_thread::sleep_for(std::chrono::seconds(3));
    }

    return 0;
}
