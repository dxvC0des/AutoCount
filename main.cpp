#include <windows.h>
#include <string>
#include <thread>
#include <vector>
#include <tlhelp32.h>
#include <shellapi.h>
#include <commctrl.h>
#include <setupapi.h>
#include <devguid.h>
#include <regstr.h>
#include <algorithm>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "setupapi.lib")

HWND hLog, hPortCombo, hConnectButton, hKillButton, hRefreshButton, hStatus;
HANDLE hSerial = INVALID_HANDLE_VALUE;
bool running = false;
bool connecting = false;

#define WM_APPEND_LOG (WM_USER + 1)
#define WM_SET_STATUS (WM_USER + 2)
#define WM_SET_CONNECTING (WM_USER + 3)

// ------------------------ COM PORT ENUMERATION ------------------------
std::vector<std::string> getAllComPorts() {
    std::vector<std::string> ports;

    // SetupAPI enumeration
    HDEVINFO hDevInfo = SetupDiGetClassDevsA(&GUID_DEVCLASS_PORTS, nullptr, nullptr, DIGCF_PRESENT);
    if (hDevInfo != INVALID_HANDLE_VALUE) {
        SP_DEVINFO_DATA devInfo; devInfo.cbSize = sizeof(devInfo);
        for (DWORD i = 0; SetupDiEnumDeviceInfo(hDevInfo, i, &devInfo); i++) {
            HKEY hKey = SetupDiOpenDevRegKey(hDevInfo, &devInfo, DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_READ);
            if (hKey != INVALID_HANDLE_VALUE) {
                char portName[256] = {0};
                DWORD size = sizeof(portName), type = 0;
                if (RegQueryValueExA(hKey, "PortName", nullptr, &type, (LPBYTE)portName, &size) == ERROR_SUCCESS) {
                    if (strncmp(portName, "COM", 3) == 0)
                        ports.push_back(portName);
                }
                RegCloseKey(hKey);
            }
        }
        SetupDiDestroyDeviceInfoList(hDevInfo);
    }

    // Fallback scan COM1-COM20
    for (int i = 1; i <= 20; i++) {
        std::string port = "COM" + std::to_string(i);
        std::string path = "\\\\.\\" + port;
        if (GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES &&
            std::find(ports.begin(), ports.end(), port) == ports.end()) {
            ports.push_back(port);
        }
    }

    return ports;
}

void populateComPorts(HWND hCombo) {
    SendMessageA(hCombo, CB_RESETCONTENT, 0, 0);
    auto ports = getAllComPorts();
    for (auto& p : ports)
        SendMessageA(hCombo, CB_ADDSTRING, 0, (LPARAM)p.c_str());

    // Select first port if available
    if (!ports.empty())
        SendMessageA(hCombo, CB_SETCURSEL, 0, 0);
}

// ------------------------ LOG / STATUS ------------------------
void appendLogGUI(const std::string& msg) {
    int len = GetWindowTextLengthA(hLog);
    SendMessageA(hLog, EM_SETSEL, len, len);
    SendMessageA(hLog, EM_REPLACESEL, FALSE, (LPARAM)(msg + "\r\n").c_str());
    SendMessageA(hLog, EM_SCROLLCARET, 0, 0);
}

// ------------------------ CHROME FUNCTIONS ------------------------
void killChrome(HWND hwnd) {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32 entry = {0}; entry.dwSize = sizeof(entry);

    if (Process32First(snapshot, &entry)) {
        do {
            if (_stricmp(entry.szExeFile, "chrome.exe") == 0) {
                HANDLE hProc = OpenProcess(PROCESS_TERMINATE, FALSE, entry.th32ProcessID);
                if (hProc) { TerminateProcess(hProc, 0); CloseHandle(hProc); }
            }
        } while (Process32Next(snapshot, &entry));
    }
    CloseHandle(snapshot);
    PostMessageA(hwnd, WM_APPEND_LOG, 0, (LPARAM)new std::string("[INFO] Killed Chrome"));
}

void openUrlKiosk(HWND hwnd, const std::string& url) {
    killChrome(hwnd);
    std::vector<std::string> candidates = {
        "C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe",
        "C:\\Program Files (x86)\\Google\\Chrome\\Application\\chrome.exe"
    };
    std::string chromePath;
    for (auto& p : candidates)
        if (GetFileAttributesA(p.c_str()) != INVALID_FILE_ATTRIBUTES) { chromePath = p; break; }

    if (!chromePath.empty()) {
        std::string params = "--new-window --kiosk \"" + url + "\"";
        ShellExecuteA(nullptr, "open", chromePath.c_str(), params.c_str(), nullptr, SW_SHOWNORMAL);
        PostMessageA(hwnd, WM_APPEND_LOG, 0, (LPARAM)new std::string("[OPEN] " + url));
    } else {
        ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        PostMessageA(hwnd, WM_APPEND_LOG, 0, (LPARAM)new std::string("[WARN] Chrome not found, opened default browser"));
    }
}

// ------------------------ SERIAL THREAD ------------------------
void serialThread(HWND hwnd, const std::string portName) {
    PostMessage(hwnd, WM_SET_CONNECTING, TRUE, 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    while (running) {
        hSerial = CreateFileA(portName.c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, 0, nullptr);
        if (hSerial == INVALID_HANDLE_VALUE) {
            PostMessageA(hwnd, WM_APPEND_LOG, 0, (LPARAM)new std::string("[ERROR] Cannot open " + portName));
            PostMessage(hwnd, WM_SET_STATUS, FALSE, 0);
            std::this_thread::sleep_for(std::chrono::seconds(3));
            continue;
        }

        DCB dcb = {0}; dcb.DCBlength = sizeof(dcb);
        GetCommState(hSerial, &dcb);
        dcb.BaudRate = CBR_9600; dcb.ByteSize = 8; dcb.StopBits = ONESTOPBIT; dcb.Parity = NOPARITY;
        SetCommState(hSerial, &dcb);

        COMMTIMEOUTS timeouts = {0};
        timeouts.ReadIntervalTimeout = 50;
        timeouts.ReadTotalTimeoutConstant = 50;
        timeouts.ReadTotalTimeoutMultiplier = 10;
        SetCommTimeouts(hSerial, &timeouts);

        PostMessageA(hwnd, WM_APPEND_LOG, 0, (LPARAM)new std::string("[INFO] Connected to " + portName));
        PostMessage(hwnd, WM_SET_STATUS, TRUE, 0);
        PostMessage(hwnd, WM_SET_CONNECTING, FALSE, 0);

        char buffer[256]; DWORD bytesRead; std::string line;
        while (running && ReadFile(hSerial, buffer, sizeof(buffer)-1, &bytesRead, nullptr)) {
            if (bytesRead > 0) {
                buffer[bytesRead] = '\0';
                line += buffer;
                size_t pos;
                while ((pos = line.find('\n')) != std::string::npos) {
                    std::string msg = line.substr(0, pos);
                    line.erase(0, pos+1);
                    if (!msg.empty()) {
                        if (msg.find("http") != std::string::npos)
                            openUrlKiosk(hwnd, msg);
                        else
                            PostMessageA(hwnd, WM_APPEND_LOG, 0, (LPARAM)new std::string("[SERIAL] " + msg));
                    }
                }
            }
        }

        CloseHandle(hSerial);
        PostMessage(hwnd, WM_SET_STATUS, FALSE, 0);
        PostMessage(hwnd, WM_SET_CONNECTING, FALSE, 0);
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

// ------------------------ WINDOW PROC ------------------------
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static std::thread t;

    switch(msg) {
    case WM_CREATE: {
        HFONT hFont = CreateFontA(16,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, FF_DONTCARE, "Segoe UI");

        hPortCombo = CreateWindowExA(0, "COMBOBOX", nullptr,
            WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | CBS_HASSTRINGS | WS_VSCROLL,
            20,20,150,200, hwnd, nullptr, nullptr, nullptr);
        SendMessageA(hPortCombo, WM_SETFONT, (WPARAM)hFont, TRUE);
        populateComPorts(hPortCombo);

        hRefreshButton = CreateWindowExA(0, "BUTTON", "Refresh COM Ports",
            WS_CHILD | WS_VISIBLE, 180,20,150,30, hwnd, (HMENU)3, nullptr, nullptr);
        SendMessageA(hRefreshButton, WM_SETFONT, (WPARAM)hFont, TRUE);

        hConnectButton = CreateWindowExA(0, "BUTTON", "Connect / Reconnect",
            WS_CHILD | WS_VISIBLE, 340,20,180,30, hwnd, (HMENU)1, nullptr, nullptr);
        SendMessageA(hConnectButton, WM_SETFONT, (WPARAM)hFont, TRUE);

        hKillButton = CreateWindowExA(0, "BUTTON", "Kill Chrome",
            WS_CHILD | WS_VISIBLE, 530,20,120,30, hwnd, (HMENU)2, nullptr, nullptr);
        SendMessageA(hKillButton, WM_SETFONT, (WPARAM)hFont, TRUE);

        hStatus = CreateWindowExA(WS_EX_CLIENTEDGE, "STATIC", "",
            WS_CHILD | WS_VISIBLE | SS_CENTER, 20,60,30,30, hwnd, nullptr, nullptr, nullptr);

        hLog = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
            20,100,630,380, hwnd, nullptr, nullptr, nullptr);
        SendMessageA(hLog, WM_SETFONT, (WPARAM)hFont, TRUE);

    } break;

    case WM_COMMAND:
        if (LOWORD(wParam) == 1) { // Connect/Reconnect
            if (connecting) break;
            LRESULT sel = SendMessageA(hPortCombo, CB_GETCURSEL, 0, 0);
            if (sel == CB_ERR) { appendLogGUI("[ERROR] No COM port selected."); break; }
            char port[10]; SendMessageA(hPortCombo, CB_GETLBTEXT, sel, (LPARAM)port);

            running = false; if (t.joinable()) t.join();

            connecting = true;
            SetWindowTextA(hConnectButton, "Connecting...");
            EnableWindow(hConnectButton, FALSE);

            running = true;
            t = std::thread(serialThread, hwnd, std::string(port));
        }
        else if (LOWORD(wParam) == 2) killChrome(hwnd);
        else if (LOWORD(wParam) == 3) populateComPorts(hPortCombo);
        break;

    case WM_APPEND_LOG: {
        std::string* pMsg = (std::string*)lParam;
        appendLogGUI(*pMsg);
        delete pMsg;
        break;
    }
    case WM_SET_STATUS:
        InvalidateRect(hStatus, nullptr, TRUE);
        break;
    case WM_SET_CONNECTING:
        connecting = (wParam != 0);
        SetWindowTextA(hConnectButton, connecting ? "Connecting..." : "Connect / Reconnect");
        EnableWindow(hConnectButton, !connecting);
        break;

    case WM_PAINT: {
        PAINTSTRUCT ps; HDC hdc = BeginPaint(hStatus, &ps);
        RECT rect; GetClientRect(hStatus, &rect);
        HBRUSH brush = CreateSolidBrush(connecting ? RGB(200,200,0) : (running ? RGB(0,200,0) : RGB(200,0,0)));
        FillRect(hdc, &rect, brush); DeleteObject(brush);
        EndPaint(hStatus, &ps);
    } break;

    case WM_DESTROY:
        running = false; if (t.joinable()) t.join();
        PostQuitMessage(0); break;

    default: return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    return 0;
}

// ------------------------ WINMAIN ------------------------
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nCmdShow) {
    WNDCLASSA wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = "AutoCountWinClass";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassA(&wc);

    HWND hwnd = CreateWindowExA(0, "AutoCountWinClass", "AutoCount GUI",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 680, 520,
        nullptr, nullptr, hInst, nullptr);

    ShowWindow(hwnd, nCmdShow); UpdateWindow(hwnd);

    MSG msg;
    while(GetMessage(&msg,nullptr,0,0)) { TranslateMessage(&msg); DispatchMessage(&msg); }

    return (int)msg.wParam;
}
