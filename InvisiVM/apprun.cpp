#define UNICODE
#define _WIN32_WINNT 0x0600  // Windows Vista or later for GetProcessId
#include <windows.h>
#include <commdlg.h>
#include <string>
#include <shlwapi.h>
#include <thread>
#include <chrono>
#include <shellapi.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <vector>
#include "apprun.h"
#include "constants.h"

#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "Comdlg32.lib")
#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "Psapi.lib")

struct WindowSearchData {
    DWORD targetPid;
    std::wstring filePath;
    std::wstring fileName;
    std::vector<HWND> candidateWindows;
};

// Forward declarations
BOOL CALLBACK EnumWindowsForProcess(HWND hwnd, LPARAM lParam);
BOOL CALLBACK EnumAllWindowsForFile(HWND hwnd, LPARAM lParam);
bool AppRun_FindWindowByPID(HWND parentWindow, AppRunState* state, DWORD processId);
bool AppRun_FindWindowByFileName(HWND parentWindow, AppRunState* state, const wchar_t* filePath);
bool AppRun_EmbedWindow(HWND parentWindow, AppRunState* state);
void AppRun_UpdatePosition(HWND parentWindow, const RECT& clientRect, AppRunState* state);
std::wstring ToLower(const std::wstring& str);
bool IsOurOwnWindow(HWND hwnd);
bool IsValidApplicationWindow(HWND hwnd);
DWORD FindProcessByWindow(HWND hwnd);
std::wstring GetWindowProcessName(HWND hwnd);
DWORD GetProcessIdFromHandle(HANDLE hProcess);

// Helper function for older Windows versions
DWORD GetProcessIdFromHandle(HANDLE hProcess) {
    // Try GetProcessId first (Vista+)
    typedef DWORD (WINAPI *GetProcessIdFunc)(HANDLE);
    HMODULE hKernel32 = GetModuleHandleW(L"kernel32.dll");
    if (hKernel32) {
        GetProcessIdFunc pGetProcessId = (GetProcessIdFunc)GetProcAddress(hKernel32, "GetProcessId");
        if (pGetProcessId) {
            return pGetProcessId(hProcess);
        }
    }
    
    // Fallback: use NtQueryInformationProcess or toolhelp
    // For simplicity, we'll use a different approach
    DWORD processId = 0;
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W pe32;
        pe32.dwSize = sizeof(pe32);
        
        if (Process32FirstW(hSnapshot, &pe32)) {
            do {
                HANDLE hProc = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pe32.th32ProcessID);
                if (hProc) {
                    if (hProc == hProcess) {
                        processId = pe32.th32ProcessID;
                        CloseHandle(hProc);
                        break;
                    }
                    CloseHandle(hProc);
                }
            } while (Process32NextW(hSnapshot, &pe32));
        }
        CloseHandle(hSnapshot);
    }
    
    return processId;
}

// Enumerate windows for a specific process
BOOL CALLBACK EnumWindowsForProcess(HWND hwnd, LPARAM lParam) {
    WindowSearchData* data = (WindowSearchData*)lParam;
    
    if (!IsWindowVisible(hwnd)) {
        return TRUE;
    }
    
    // Get process ID
    DWORD processId;
    GetWindowThreadProcessId(hwnd, &processId);
    
    if (processId != data->targetPid) {
        return TRUE;
    }
    
    // Skip our own windows
    if (IsOurOwnWindow(hwnd)) {
        return TRUE;
    }
    
    // Check if it's a valid application window
    if (IsValidApplicationWindow(hwnd)) {
        data->candidateWindows.push_back(hwnd);
    }
    
    return TRUE;
}

// Enumerate all windows to find by filename
BOOL CALLBACK EnumAllWindowsForFile(HWND hwnd, LPARAM lParam) {
    WindowSearchData* data = (WindowSearchData*)lParam;
    
    if (!IsWindowVisible(hwnd) || IsOurOwnWindow(hwnd)) {
        return TRUE;
    }
    
    wchar_t title[512];
    GetWindowTextW(hwnd, title, 512);
    std::wstring titleLower = ToLower(title);
    
    // Check if title contains filename
    if (!data->fileName.empty() && titleLower.find(data->fileName) != std::wstring::npos) {
        if (IsValidApplicationWindow(hwnd)) {
            data->candidateWindows.push_back(hwnd);
        }
    }
    
    return TRUE;
}

std::wstring ToLower(const std::wstring& str) {
    std::wstring result = str;
    for (size_t i = 0; i < result.length(); i++) {
        result[i] = towlower(result[i]);
    }
    return result;
}

bool IsOurOwnWindow(HWND hwnd) {
    wchar_t className[256];
    wchar_t title[256];
    
    GetClassNameW(hwnd, className, 256);
    GetWindowTextW(hwnd, title, 256);
    
    std::wstring classStr = ToLower(className);
    std::wstring titleStr = ToLower(title);
    
    return (classStr.find(L"pdfviewerapp") != std::wstring::npos ||
            titleStr.find(L"invisvm") != std::wstring::npos);
}

bool IsValidApplicationWindow(HWND hwnd) {
    // Must be visible
    if (!IsWindowVisible(hwnd)) {
        return false;
    }
    
    // Check window styles
    LONG style = GetWindowLong(hwnd, GWL_STYLE);
    LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
    
    // Must have a title bar or be a popup
    if (!(style & WS_CAPTION) && !(style & WS_POPUP)) {
        return false;
    }
    
    // Skip tool windows
    if (exStyle & WS_EX_TOOLWINDOW) {
        return false;
    }
    
    // Skip windows with no title (unless they're large enough)
    wchar_t title[256];
    GetWindowTextW(hwnd, title, 256);
    if (wcslen(title) == 0) {
        RECT rect;
        GetWindowRect(hwnd, &rect);
        int width = rect.right - rect.left;
        int height = rect.bottom - rect.top;
        
        // Allow large untitled windows (like some games/apps)
        if (width < 200 || height < 150) {
            return false;
        }
    }
    
    // Skip desktop and shell windows
    wchar_t className[256];
    GetClassNameW(hwnd, className, 256);
    std::wstring classStr = ToLower(className);
    
    if (classStr == L"progman" || 
        classStr == L"shell_traywnd" ||
        classStr == L"workerw" ||
        classStr.find(L"dde") != std::wstring::npos) {
        return false;
    }
    
    return true;
}

std::wstring GetWindowProcessName(HWND hwnd) {
    DWORD processId;
    GetWindowThreadProcessId(hwnd, &processId);
    
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
    if (!hProcess) {
        return L"";
    }
    
    wchar_t processPath[MAX_PATH];
    // Use GetModuleFileNameEx for broader compatibility
    if (GetModuleFileNameExW(hProcess, NULL, processPath, MAX_PATH)) {
        CloseHandle(hProcess);
        return processPath;
    }
    
    CloseHandle(hProcess);
    return L"";
}

void AppRun_Initialize(AppRunState* state) {
    if (!state) return;
    
    state->embeddedWindow = NULL;
    state->isEmbedded = false;
    state->appPath.clear();
    state->appName.clear();
    ZeroMemory(&state->procInfo, sizeof(state->procInfo));
    state->appRect = {0, 0, 0, 0};
}

bool AppRun_SelectAndLaunchApp(HWND parentWindow, AppRunState* state) {
    if (!state) return false;
    
    if (state->isEmbedded) {
        AppRun_CloseApp(state);
    }
    
    // Open file dialog for ANY file type
    wchar_t szFile[MAX_PATH] = {0};
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = parentWindow;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"All Files\0*.*\0Executables\0*.exe\0Documents\0*.pdf;*.doc;*.docx;*.txt;*.xlsx\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
    ofn.lpstrTitle = L"Select Any File or Application to Run";
    
    if (!GetOpenFileNameW(&ofn)) {
        return false;
    }
    
    state->appPath = szFile;
    
    // Extract filename for display
    wchar_t* fileName = PathFindFileNameW(szFile);
    if (fileName) {
        state->appName = fileName;
    }

    // STEP 1: Launch the file/application
    SHELLEXECUTEINFOW sei = {0};
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_FLAG_NO_UI;
    sei.hwnd = parentWindow;
    sei.lpVerb = L"open";
    sei.lpFile = szFile;
    sei.lpParameters = NULL;
    sei.lpDirectory = NULL;
    sei.nShow = SW_SHOWMINNOACTIVE;
    
    if (!ShellExecuteExW(&sei)) {
        DWORD error = GetLastError();
        wchar_t errorMsg[512];
        // Use wsprintfW for maximum compatibility
        wsprintfW(errorMsg, 
                  L"Failed to open file.\nError code: %lu\n\nMake sure you have the appropriate application installed.", 
                  error);
        MessageBoxW(parentWindow, errorMsg, L"Error", MB_OK | MB_ICONERROR);
        return false;
    }
    
    // STEP 2: Handle process
    if (sei.hProcess) {
        // We got a process handle - wait for it to be ready
        state->procInfo.hProcess = sei.hProcess;
        state->procInfo.dwProcessId = GetProcessIdFromHandle(sei.hProcess);
        
        // Wait for the process to finish initializing (max 30 seconds)
        DWORD waitResult = WaitForInputIdle(sei.hProcess, 30000);
        
        if (waitResult == WAIT_TIMEOUT) {
            MessageBoxW(parentWindow, 
                       L"Application is taking too long to start.\nTrying to find window anyway...",
                       L"Timeout", MB_OK | MB_ICONWARNING);
        } else if (waitResult == WAIT_FAILED) {
            // Console app or process ended - try to find window anyway
            Sleep(500);
        }
        
        // STEP 3: Find the window by PID
        if (AppRun_FindWindowByPID(parentWindow, state, state->procInfo.dwProcessId)) {
            return true;
        }
        
        // Fallback: try by filename
        Sleep(1000);
        if (AppRun_FindWindowByFileName(parentWindow, state, szFile)) {
            return true;
        }
        
        MessageBoxW(parentWindow, 
                   L"Application started but no suitable window found.\n\n"
                   L"The file may have opened in an existing application,\n"
                   L"or the application might not have a visible window.",
                   L"No Window Found", MB_OK | MB_ICONINFORMATION);
        AppRun_CloseApp(state);
        return true; // Still return true since we launched successfully
        
    } else {
        // No process handle - file opened in existing application
        // Wait a bit for window to update
        Sleep(1500);
        
        if (AppRun_FindWindowByFileName(parentWindow, state, szFile)) {
            return true;
        }
        
        MessageBoxW(parentWindow, 
                   L"File opened in existing application.\n"
                   L"Could not find a window to embed.\n\n"
                   L"Try opening the application directly (.exe files work best).",
                   L"No Window Found", MB_OK | MB_ICONINFORMATION);
        return true;
    }
}

bool AppRun_FindWindowByPID(HWND parentWindow, AppRunState* state, DWORD processId) {
    if (!state || processId == 0) return false;
    
    WindowSearchData searchData;
    searchData.targetPid = processId;
    searchData.filePath = state->appPath;
    
    // Extract filename without extension
    std::wstring fileName = state->appPath;
    size_t lastSlash = fileName.find_last_of(L"\\/");
    if (lastSlash != std::wstring::npos) {
        fileName = fileName.substr(lastSlash + 1);
    }
    size_t dotPos = fileName.find_last_of(L'.');
    if (dotPos != std::wstring::npos) {
        fileName = fileName.substr(0, dotPos);
    }
    searchData.fileName = ToLower(fileName);
    
    // Enumerate windows for this process
    EnumWindows(EnumWindowsForProcess, (LPARAM)&searchData);
    
    if (searchData.candidateWindows.empty()) {
        return false;
    }
    
    // Pick the best candidate (largest window)
    HWND bestWindow = NULL;
    int maxArea = 0;
    
    for (HWND hwnd : searchData.candidateWindows) {
        RECT rect;
        if (GetWindowRect(hwnd, &rect)) {
            int width = rect.right - rect.left;
            int height = rect.bottom - rect.top;
            int area = width * height;
            
            if (area > maxArea) {
                maxArea = area;
                bestWindow = hwnd;
            }
        }
    }
    
    if (bestWindow) {
        state->embeddedWindow = bestWindow;
        return AppRun_EmbedWindow(parentWindow, state);
    }
    
    return false;
}

bool AppRun_FindWindowByFileName(HWND parentWindow, AppRunState* state, const wchar_t* filePath) {
    if (!state || !filePath) return false;
    
    WindowSearchData searchData;
    searchData.targetPid = 0;
    searchData.filePath = filePath;
    
    // Extract filename without extension
    std::wstring fileName = filePath;
    size_t lastSlash = fileName.find_last_of(L"\\/");
    if (lastSlash != std::wstring::npos) {
        fileName = fileName.substr(lastSlash + 1);
    }
    size_t dotPos = fileName.find_last_of(L'.');
    if (dotPos != std::wstring::npos) {
        fileName = fileName.substr(0, dotPos);
    }
    searchData.fileName = ToLower(fileName);
    
    // Enumerate all windows
    EnumWindows(EnumAllWindowsForFile, (LPARAM)&searchData);
    
    if (searchData.candidateWindows.empty()) {
        return false;
    }
    
    // Pick most recently created window (usually the newest)
    HWND bestWindow = searchData.candidateWindows[searchData.candidateWindows.size() - 1];
    
    state->embeddedWindow = bestWindow;
    return AppRun_EmbedWindow(parentWindow, state);
}

bool AppRun_EmbedWindow(HWND parentWindow, AppRunState* state) {
    if (!state || !state->embeddedWindow) return false;
    
    // Double-check we're not embedding our own window
    if (IsOurOwnWindow(state->embeddedWindow) || state->embeddedWindow == parentWindow) {
        MessageBoxW(parentWindow, L"Cannot embed InvisVM's own window", L"Error", MB_OK | MB_ICONERROR);
        return false;
    }
    
    // Verify window is still valid
    if (!IsWindow(state->embeddedWindow)) {
        MessageBoxW(parentWindow, L"Window is no longer valid", L"Error", MB_OK | MB_ICONERROR);
        return false;
    }
    
    // Restore window if minimized
    if (IsIconic(state->embeddedWindow)) {
        ShowWindow(state->embeddedWindow, SW_RESTORE);
        Sleep(200);
    }
    
    // Set parent to embed
    HWND oldParent = GetParent(state->embeddedWindow);
    SetParent(state->embeddedWindow, parentWindow);
    
    // Remove window decorations and set as child
    LONG style = GetWindowLong(state->embeddedWindow, GWL_STYLE);
    style &= ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU | WS_BORDER);
    style |= WS_CHILD;
    SetWindowLong(state->embeddedWindow, GWL_STYLE, style);
    
    // Update extended style
    LONG exStyle = GetWindowLong(state->embeddedWindow, GWL_EXSTYLE);
    exStyle &= ~(WS_EX_DLGMODALFRAME | WS_EX_WINDOWEDGE | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE);
    SetWindowLong(state->embeddedWindow, GWL_EXSTYLE, exStyle);
    
    // Update position and size
    RECT clientRect;
    GetClientRect(parentWindow, &clientRect);
    AppRun_UpdatePosition(parentWindow, clientRect, state);
    
    // Force window to update its frame
    SetWindowPos(state->embeddedWindow, HWND_TOP, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
    
    // Show the window
    ShowWindow(state->embeddedWindow, SW_SHOW);
    SetForegroundWindow(state->embeddedWindow);
    
    state->isEmbedded = true;
    InvalidateRect(parentWindow, NULL, TRUE);
    
    return true;
}

void AppRun_UpdatePosition(HWND parentWindow, const RECT& clientRect, AppRunState* state) {
    if (!state || !state->isEmbedded || !state->embeddedWindow) return;
    
    const int RED_BORDER = 3;
    const int WHITE_BORDER = 10;
    const int TOTAL_BORDER = RED_BORDER + WHITE_BORDER;
    
    state->appRect.left = TOTAL_BORDER;
    state->appRect.top = TOTAL_BORDER;
    state->appRect.right = clientRect.right - TOTAL_BORDER;
    state->appRect.bottom = clientRect.bottom - BAR_HEIGHT - TOTAL_BORDER;
    
    if (IsWindow(state->embeddedWindow)) {
        SetWindowPos(state->embeddedWindow, HWND_TOP,
                    state->appRect.left,
                    state->appRect.top,
                    state->appRect.right - state->appRect.left,
                    state->appRect.bottom - state->appRect.top,
                    SWP_SHOWWINDOW);
    }
}

void AppRun_Draw(HDC hdc, const RECT& clientRect, AppRunState* state) {
    if (!state || !state->isEmbedded) return;
    
    const int RED_BORDER = 3;
    const int WHITE_BORDER = 10;
    
    // Draw red border (3 pixels thick)
    RECT redBorder = clientRect;
    redBorder.bottom = clientRect.bottom - BAR_HEIGHT;
    HBRUSH redBrush = CreateSolidBrush(RGB(255, 0, 0));
    
    for (int i = 0; i < RED_BORDER; i++) {
        RECT layer = redBorder;
        InflateRect(&layer, -i, -i);
        FrameRect(hdc, &layer, redBrush);
    }
    DeleteObject(redBrush);
    
    // Draw white border (10 pixels thick)
    RECT whiteBorder = clientRect;
    whiteBorder.left = RED_BORDER;
    whiteBorder.top = RED_BORDER;
    whiteBorder.right = clientRect.right - RED_BORDER;
    whiteBorder.bottom = clientRect.bottom - BAR_HEIGHT - RED_BORDER;
    
    HBRUSH whiteBrush = CreateSolidBrush(RGB(255, 255, 255));
    for (int i = 0; i < WHITE_BORDER; i++) {
        RECT layer = whiteBorder;
        InflateRect(&layer, -i, -i);
        FrameRect(hdc, &layer, whiteBrush);
    }
    DeleteObject(whiteBrush);
}

void AppRun_CloseApp(AppRunState* state) {
    if (!state) return;
    
    if (state->embeddedWindow && IsWindow(state->embeddedWindow)) {
        // Restore window style before un-parenting
        LONG style = GetWindowLong(state->embeddedWindow, GWL_STYLE);
        style &= ~WS_CHILD;
        style |= WS_OVERLAPPEDWINDOW;
        SetWindowLong(state->embeddedWindow, GWL_STYLE, style);
        
        // Un-parent the window
        SetParent(state->embeddedWindow, NULL);
        
        // Try graceful close first
        PostMessage(state->embeddedWindow, WM_CLOSE, 0, 0);
        Sleep(500);
        
        // Force close if still running
        if (IsWindow(state->embeddedWindow)) {
            SendMessage(state->embeddedWindow, WM_CLOSE, 0, 0);
            Sleep(300);
        }
    }
    
    // Terminate process if still running
    if (state->procInfo.hProcess) {
        DWORD exitCode;
        if (GetExitCodeProcess(state->procInfo.hProcess, &exitCode) && exitCode == STILL_ACTIVE) {
            TerminateProcess(state->procInfo.hProcess, 0);
            WaitForSingleObject(state->procInfo.hProcess, 2000);
        }
        CloseHandle(state->procInfo.hProcess);
        state->procInfo.hProcess = NULL;
    }
    
    AppRun_Initialize(state);
}

void AppRun_Cleanup(AppRunState* state) {
    if (!state) return;
    AppRun_CloseApp(state);
}

bool AppRun_IsRunning(AppRunState* state) {
    if (!state || !state->isEmbedded) return false;
    
    if (!state->embeddedWindow || !IsWindow(state->embeddedWindow)) {
        return false;
    }
    
    // Check if the embedded window is still visible
    if (!IsWindowVisible(state->embeddedWindow)) {
        return false;
    }
    
    return true;
}

std::wstring AppRun_GetWindowTitle(AppRunState* state) {
    if (!state || !state->isEmbedded) {
        return L"InvisVM";
    }
    
    return state->appName + L" - running in InvisVM";
}