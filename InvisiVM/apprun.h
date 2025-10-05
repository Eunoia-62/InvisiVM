#ifndef APPRUN_H
#define APPRUN_H

#include <windows.h>
#include <string>

// Application embedding state
struct AppRunState {
    HWND embeddedWindow;           // Handle to the embedded application window
    PROCESS_INFORMATION procInfo;  // Process information
    std::wstring appPath;          // Path to the embedded application
    std::wstring appName;          // Name of the embedded application
    bool isEmbedded;               // Whether an app is currently embedded
    RECT appRect;                  // Rectangle where the app should be displayed
    
    AppRunState() : embeddedWindow(NULL), isEmbedded(false), appPath(L""), appName(L"") {
        ZeroMemory(&procInfo, sizeof(procInfo));
        appRect = {0, 0, 0, 0};
    }
};

// Function declarations
void AppRun_Initialize(AppRunState* state);
bool AppRun_SelectAndLaunchApp(HWND parentWindow, AppRunState* state);
bool AppRun_EmbedWindow(HWND parentWindow, AppRunState* state);
void AppRun_UpdatePosition(HWND parentWindow, const RECT& clientRect, AppRunState* state);
void AppRun_Draw(HDC hdc, const RECT& clientRect, AppRunState* state);
void AppRun_Cleanup(AppRunState* state);
bool AppRun_IsRunning(AppRunState* state);
void AppRun_CloseApp(AppRunState* state);
std::wstring AppRun_GetWindowTitle(AppRunState* state);
BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam);
BOOL CALLBACK FindAnyWindowFromProcess(HWND hwnd, LPARAM lParam);

#endif