#define UNICODE
#include <windows.h>
#include <windowsx.h>
#include <string>
#include "ui.h"
#include "pdf.h"
#include "apprun.h"
#include "constants.h"

// Global window counter
static int g_windowCount = 0;

// Window data structure - stores all state for each window
struct WindowData {
    UIState uiState;
    PDFState pdfState;
    bool isPDFViewer;  // true for PDF viewer, false for home window
    bool isAppRunner;  // true when running embedded app
    
    WindowData() : isPDFViewer(false), isAppRunner(false) {}
};

// Forward declarations
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
HWND CreatePDFViewerWindow(HINSTANCE hInstance, const char* pdfPath, const char* expectedType = nullptr);
HWND CreateAppRunnerWindow(HINSTANCE hInstance);
void HandleMouseMove(HWND hwnd, int x, int y, WindowData* data);
void HandleButtonClick(HWND hwnd, int buttonIndex, WindowData* data);
void HandleKeyPress(HWND hwnd, WPARAM key, WindowData* data);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    const wchar_t CLASS_NAME[] = L"PDFViewerApp";

    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);

    if (!RegisterClass(&wc)) {
        MessageBoxA(NULL, "Failed to register window class", "Error", MB_OK | MB_ICONERROR);
        return -1;
    }

    // If launched with PDF, create PDF viewer directly
    if (strlen(lpCmdLine) > 0) {
        char* pdfPath = lpCmdLine;
        if (pdfPath[0] == '"') {
            pdfPath++;
            size_t len = strlen(pdfPath);
            if (len > 0 && pdfPath[len - 1] == '"') pdfPath[len - 1] = '\0';
        }
        
        // Create PDF viewer window directly
        HWND pdfViewer = CreatePDFViewerWindow(hInstance, pdfPath);
        if (pdfViewer) {
            ShowWindow(pdfViewer, nCmdShow);
            UpdateWindow(pdfViewer);
        } else {
            MessageBoxA(NULL, "Failed to create PDF viewer window", "Error", MB_OK | MB_ICONERROR);
            return -1;
        }
    } else {
        // Create main/home window
        WindowData* data = new WindowData();
        UI_Initialize(&data->uiState);
        PDF_Initialize(&data->pdfState);
        AppRun_Initialize(&data->uiState.embeddedApp);
        UI_InitializeButtons(&data->uiState);
        data->isPDFViewer = false;
        data->isAppRunner = false;
        
        HWND hwnd = CreateWindowEx(
            0,
            CLASS_NAME,
            L"InvisVM - Home",
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            WINDOW_WIDTH,
            WINDOW_HEIGHT,
            NULL,
            NULL,
            hInstance,
            data  // Pass WindowData as creation parameter
        );

        if (!hwnd) {
            MessageBoxA(NULL, "Failed to create window", "Error", MB_OK | MB_ICONERROR);
            delete data;
            return -1;
        }

        ShowWindow(hwnd, nCmdShow);
        UpdateWindow(hwnd);

        if (!data->uiState.skipIntro) {
            UI_StartIntroTimer(hwnd, &data->uiState);
        } else {
            data->uiState.showHomeUI = true;
        }
    }

    // Message loop
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}

// Function to create a new PDF viewer window
HWND CreatePDFViewerWindow(HINSTANCE hInstance, const char* pdfPath, const char* expectedType) {
    const wchar_t CLASS_NAME[] = L"PDFViewerApp";
    
    // Create window data
    WindowData* data = new WindowData();
    UI_Initialize(&data->uiState);
    PDF_Initialize(&data->pdfState);
    AppRun_Initialize(&data->uiState.embeddedApp);
    UI_InitializeButtons(&data->uiState);
    data->isPDFViewer = true;
    data->isAppRunner = false;
    data->uiState.skipIntro = true;
    data->uiState.showHomeUI = false;
    
    // Process the file
    if (!PDF_ProcessFile(pdfPath, &data->pdfState, expectedType)) {
        MessageBoxA(NULL, "Failed to process file. Check that Python and required libraries are installed.", 
                   "File Error", MB_OK | MB_ICONWARNING);
        // Continue anyway to show error message in window
    }
    
    // Extract filename for window title
    const char* filename = strrchr(pdfPath, '\\');
    if (!filename) filename = strrchr(pdfPath, '/');
    if (!filename) filename = pdfPath;
    else filename++; // Skip the slash
    
    std::wstring windowTitle = L"InvisVM - ";
    // Convert filename to wstring
    int len = MultiByteToWideChar(CP_UTF8, 0, filename, -1, NULL, 0);
    if (len > 0) {
        wchar_t* wFilename = new wchar_t[len];
        MultiByteToWideChar(CP_UTF8, 0, filename, -1, wFilename, len);
        windowTitle += wFilename;
        delete[] wFilename;
    }
    
    HWND hwnd = CreateWindowEx(
        0,
        CLASS_NAME,
        windowTitle.c_str(),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        WINDOW_WIDTH,
        WINDOW_HEIGHT,
        NULL,
        NULL,
        hInstance,
        data  // Pass WindowData as creation parameter
    );
    
    if (!hwnd) {
        delete data;
        return NULL;
    }
    
    return hwnd;
}

// Function to create a new app runner window
HWND CreateAppRunnerWindow(HINSTANCE hInstance) {
    const wchar_t CLASS_NAME[] = L"PDFViewerApp";
    
    // Create window data
    WindowData* data = new WindowData();
    UI_Initialize(&data->uiState);
    PDF_Initialize(&data->pdfState);
    AppRun_Initialize(&data->uiState.embeddedApp);
    UI_InitializeButtons(&data->uiState);
    data->isPDFViewer = false;
    data->isAppRunner = true;
    data->uiState.skipIntro = true;
    data->uiState.showHomeUI = false;
    
    HWND hwnd = CreateWindowEx(
        0,
        CLASS_NAME,
        L"InvisVM - Application",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        WINDOW_WIDTH,
        WINDOW_HEIGHT,
        NULL,
        NULL,
        hInstance,
        data
    );
    
    if (!hwnd) {
        delete data;
        return NULL;
    }
    
    return hwnd;
}

void HandleMouseMove(HWND hwnd, int x, int y, WindowData* data) {
    if (!data) return;
    
    UI_HandleMouseMove(x, y, &data->uiState);
    InvalidateRect(hwnd, NULL, FALSE);
}

void HandleButtonClick(HWND hwnd, int buttonIndex, WindowData* data) {
    if (!data || buttonIndex < 0 || (size_t)buttonIndex >= data->uiState.buttons.size()) return;
    
    MessageBeep(MB_OK);
    
    switch (buttonIndex) {
        case 0:  // Close
            PostMessage(hwnd, WM_CLOSE, 0, 0);
            break;
        case 1:  // Minimize
            ShowWindow(hwnd, SW_MINIMIZE);
            break;
        case 2:  // Maximize/Restore
            if (IsZoomed(hwnd)) {
                ShowWindow(hwnd, SW_RESTORE);
            } else {
                ShowWindow(hwnd, SW_MAXIMIZE);
            }
            break;
    }
}

void HandleKeyPress(HWND hwnd, WPARAM key, WindowData* data) {
    if (!data) return;
    
    switch (key) {
        case KEY_RED:
            HandleButtonClick(hwnd, 0, data);
            break;
        case KEY_YELLOW:
            HandleButtonClick(hwnd, 1, data);
            break;
        case KEY_GREEN:
            HandleButtonClick(hwnd, 2, data);
            break;
        case KEY_RESET:
            for (auto& button : data->uiState.buttons) {
                button.Reset();
            }
            InvalidateRect(hwnd, NULL, FALSE);
            break;
    }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    // Get window data
    WindowData* data = NULL;
    
    if (msg == WM_CREATE) {
        // Store the WindowData pointer
        CREATESTRUCT* pCreate = (CREATESTRUCT*)lParam;
        data = (WindowData*)pCreate->lpCreateParams;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)data);
    } else {
        data = (WindowData*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    }
    
    if (!data && msg != WM_CREATE) {
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    
    switch (msg) {
        case WM_CREATE: {
            g_windowCount++;  // Increment window count
            SetScrollRange(hwnd, SB_VERT, 0, 100, FALSE);
            SetScrollPos(hwnd, SB_VERT, 0, TRUE);
            ShowScrollBar(hwnd, SB_VERT, TRUE);
            return 0;
        }

        case WM_SIZE: {
            RECT clientRect;
            GetClientRect(hwnd, &clientRect);
            UI_UpdateButtonPositions(clientRect, &data->uiState);
            PDF_UpdateScrollInfo(clientRect, &data->pdfState);
            
            // Update embedded app position if running
            if (data->isAppRunner && AppRun_IsRunning(&data->uiState.embeddedApp)) {
                AppRun_UpdatePosition(hwnd, clientRect, &data->uiState.embeddedApp);
            }
            
            InvalidateRect(hwnd, NULL, TRUE);
            return 0;
        }

        case WM_ERASEBKGND: {
            HDC hdc = (HDC)wParam;
            if (hdc) {
                RECT clientRect;
                GetClientRect(hwnd, &clientRect);
                FillRect(hdc, &clientRect, (HBRUSH)GetStockObject(BLACK_BRUSH));
                return 1;
            }
            break;
        }

        case WM_TIMER:
            if (wParam == TIMER_ID_INTRO) {
                UI_UpdateIntroAnimation(hwnd, &data->uiState);
            }
            return 0;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

            if (hdc) {
                RECT clientRect;
                GetClientRect(hwnd, &clientRect);
                
                HDC memDC = CreateCompatibleDC(hdc);
                HBITMAP memBmp = CreateCompatibleBitmap(hdc, clientRect.right, clientRect.bottom);
                HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, memBmp);

                // Update button positions
                UI_UpdateButtonPositions(clientRect, &data->uiState);

                if (!data->uiState.skipIntro && data->uiState.introState != INTRO_COMPLETE) {
                    if (data->uiState.showMainUIBehind) {
                        FillRect(memDC, &clientRect, (HBRUSH)GetStockObject(BLACK_BRUSH));
                        PDF_DrawContent(memDC, clientRect, &data->pdfState);
                        
                        RECT bottomBarRect = clientRect;
                        bottomBarRect.top = clientRect.bottom - BAR_HEIGHT;
                        HBRUSH greyBrush = CreateSolidBrush(RGB(60, 60, 60));
                        FillRect(memDC, &bottomBarRect, greyBrush);
                        DeleteObject(greyBrush);

                        for (const auto& button : data->uiState.buttons) {
                            UI_DrawButton(memDC, button);
                        }
                    }
                    UI_DrawIntroSequence(memDC, clientRect, &data->uiState);
                } else {
                    if (data->uiState.showHomeUI) {
                        UI_DrawHomeUI(memDC, clientRect, &data->uiState);
                    } else if (data->isAppRunner && AppRun_IsRunning(&data->uiState.embeddedApp)) {
                        // Draw embedded application view
                        FillRect(memDC, &clientRect, (HBRUSH)GetStockObject(BLACK_BRUSH));
                        
                        // Draw app with borders
                        AppRun_Draw(memDC, clientRect, &data->uiState.embeddedApp);
                        
                        // Draw bottom bar
                        RECT bottomBarRect = clientRect;
                        bottomBarRect.top = clientRect.bottom - BAR_HEIGHT;
                        HBRUSH greyBrush = CreateSolidBrush(RGB(60, 60, 60));
                        FillRect(memDC, &bottomBarRect, greyBrush);
                        DeleteObject(greyBrush);

                        for (const auto& button : data->uiState.buttons) {
                            UI_DrawButton(memDC, button);
                        }
                        
                        // Update window title
                        SetWindowTextW(hwnd, AppRun_GetWindowTitle(&data->uiState.embeddedApp).c_str());
                    } else {
                        // Normal PDF/file view
                        FillRect(memDC, &clientRect, (HBRUSH)GetStockObject(BLACK_BRUSH));
                        PDF_DrawContent(memDC, clientRect, &data->pdfState);

                        RECT bottomBarRect = clientRect;
                        bottomBarRect.top = clientRect.bottom - BAR_HEIGHT;
                        HBRUSH greyBrush = CreateSolidBrush(RGB(60, 60, 60));
                        FillRect(memDC, &bottomBarRect, greyBrush);
                        DeleteObject(greyBrush);

                        for (const auto& button : data->uiState.buttons) {
                            UI_DrawButton(memDC, button);
                        }
                    }
                }

                BitBlt(hdc, 0, 0, clientRect.right, clientRect.bottom, memDC, 0, 0, SRCCOPY);

                SelectObject(memDC, oldBmp);
                DeleteObject(memBmp);
                DeleteDC(memDC);
            }

            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_MOUSEMOVE: {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);
            HandleMouseMove(hwnd, x, y, data);
            return 0;
        }

        case WM_LBUTTONDOWN: {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);

            if (data->uiState.showHomeUI) {
                FileType selectedType;
                if (UI_HandleHomeButtonClick(x, y, &data->uiState, &selectedType)) {
                    SetCapture(hwnd);
                    InvalidateRect(hwnd, NULL, FALSE);
                    return 0;
                }
            }

            data->uiState.clickedButton = UI_FindButtonAtPoint(x, y, &data->uiState);
            
            if (data->uiState.clickedButton >= 0) {
                data->uiState.buttons[data->uiState.clickedButton].state = STATE_CLICKED;
                SetCapture(hwnd);
                InvalidateRect(hwnd, NULL, FALSE);
            }
            return 0;
        }

        case WM_LBUTTONUP: {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);

            if (data->uiState.showHomeUI && data->uiState.pressedFileButton >= 0) {
                FileType selectedType = (FileType)data->uiState.pressedFileButton;
                
                // Handle application embedding
                if (selectedType == FILE_APP) {
                    if (UI_HandleHomeButtonRelease(hwnd, x, y, &data->uiState, 
                                                  (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
                                                  selectedType)) {
                        // Create new app runner window
                        HWND appWindow = CreateAppRunnerWindow((HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE));
                        if (appWindow) {
                            // Get the app runner window's data
                            WindowData* appData = (WindowData*)GetWindowLongPtr(appWindow, GWLP_USERDATA);
                            if (appData) {
                                // Launch app selection dialog
                                if (AppRun_SelectAndLaunchApp(appWindow, &appData->uiState.embeddedApp)) {
                                    ShowWindow(appWindow, SW_SHOW);
                                    UpdateWindow(appWindow);
                                } else {
                                    // User cancelled or error - destroy the window
                                    DestroyWindow(appWindow);
                                }
                            }
                        }
                        ReleaseCapture();
                        InvalidateRect(hwnd, NULL, FALSE);
                        return 0;
                    }
                } else {
                    // Handle other file types normally
                    if (UI_HandleHomeButtonRelease(hwnd, x, y, &data->uiState, 
                                                  (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
                                                  selectedType)) {
                        ReleaseCapture();
                        InvalidateRect(hwnd, NULL, FALSE);
                        return 0;
                    }
                }
            }

            if (data->uiState.clickedButton >= 0) {
                if (UI_FindButtonAtPoint(x, y, &data->uiState) == data->uiState.clickedButton) {
                    HandleButtonClick(hwnd, data->uiState.clickedButton, data);
                }
                
                data->uiState.clickedButton = -1;
                ReleaseCapture();
                InvalidateRect(hwnd, NULL, FALSE);
            }
            return 0;
        }

        case WM_KEYDOWN:
            HandleKeyPress(hwnd, wParam, data);
            return 0;

        case WM_VSCROLL:
            PDF_HandleScroll(hwnd, msg, wParam, lParam, &data->pdfState);
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        
        case WM_MOUSEWHEEL:
            PDF_HandleMouseWheel(hwnd, wParam, &data->pdfState);
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;

        case WM_DESTROY:
            UI_StopIntroTimer(hwnd, &data->uiState);
            
            // Cleanup embedded app if running
            if (data->isAppRunner) {
                AppRun_Cleanup(&data->uiState.embeddedApp);
            }
            
            delete data;  // Clean up window data
            g_windowCount--;  // Decrement window count
            
            // Only quit application when last window closes
            if (g_windowCount == 0) {
                PostQuitMessage(0);
            }
            return 0;

        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    
    return DefWindowProc(hwnd, msg, wParam, lParam);
}