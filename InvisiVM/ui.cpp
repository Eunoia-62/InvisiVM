#define UNICODE
#include <windows.h>
#include <windowsx.h>
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>
#include <commdlg.h>
#include "ui.h"
#include "pdf.h"
#include "constants.h"

#pragma comment(lib, "Msimg32.lib")
#pragma comment(lib, "Comdlg32.lib")

// Forward declaration
HWND CreatePDFViewerWindow(HINSTANCE hInstance, const char* pdfPath, const char* expectedType);

// File type information
struct FileTypeInfo {
    const wchar_t* label;
    const char* extension;
    const char* filter;
};

static const FileTypeInfo fileTypeInfos[FILE_COUNT] = {
    {L"PDF Files", "pdf", "PDF Files\0*.pdf\0All Files\0*.*\0"},
    {L"Text Files", "txt", "Text Files\0*.txt\0All Files\0*.*\0"},
    {L"CSV Files", "csv", "CSV Files\0*.csv\0All Files\0*.*\0"},
    {L"Word Documents", "docx", "Word Documents\0*.docx\0All Files\0*.*\0"},
    {L"Excel Files", "xlsx", "Excel Spreadsheets\0*.xlsx\0All Files\0*.*\0"},
    {L"Open Application", "exe", "Applications\0*.exe\0All Files\0*.*\0"}
};

void UI_Initialize(UIState* state) {
    if (!state) return;
    
    state->introState = INTRO_BLANK;
    state->introAlpha = 0;
    state->welcomeSize = 30;
    state->introTimer = 0;
    state->skipIntro = false;
    state->holdFrames = 0;
    state->showMainUIBehind = false;
    state->glowPosition = 0.0f;
    state->welcomeGrowFrame = 0;
    state->showHomeUI = false;
    state->hoveredFileButton = -1;
    state->pressedFileButton = -1;
    state->hoveredButton = -1;
    state->clickedButton = -1;
    
    for (int i = 0; i < FILE_COUNT; i++) {
        state->fileTypeButtons[i] = {0, 0, 0, 0};
        state->fileButtonHovered[i] = false;
        state->fileButtonPressed[i] = false;
    }
}

void UI_InitializeButtons(UIState* state) {
    if (!state) return;
    
    state->buttons.clear();
    state->buttons.emplace_back(RGB(255, 95, 87), L"Close");
    state->buttons.emplace_back(RGB(255, 189, 46), L"Minimize");
    state->buttons.emplace_back(RGB(40, 201, 64), L"Maximize");
}

void UI_UpdateButtonPositions(const RECT& clientRect, UIState* state) {
    if (!state || state->buttons.empty()) return;
    
    int bottomBarTop = clientRect.bottom - BAR_HEIGHT;
    int centerY = bottomBarTop + BAR_HEIGHT / 2;
    
    for (size_t i = 0; i < 3 && i < state->buttons.size(); i++) {
        size_t index = 2 - i;
        state->buttons[index].center.x = clientRect.right - CIRCLE_SPACING - CIRCLE_RADIUS - 
                                         (int)(i * (CIRCLE_RADIUS * 2 + CIRCLE_SPACING));
        state->buttons[index].center.y = centerY;
    }
}

int UI_FindButtonAtPoint(int x, int y, UIState* state) {
    if (!state) return -1;
    
    for (size_t i = 0; i < state->buttons.size(); i++) {
        if (state->buttons[i].ContainsPoint(x, y)) {
            return (int)i;
        }
    }
    return -1;
}

void UI_StartIntroTimer(HWND hwnd, UIState* state) {
    if (!state) return;
    state->introTimer = SetTimer(hwnd, TIMER_ID_INTRO, TIMER_INTERVAL, NULL);
}

void UI_StopIntroTimer(HWND hwnd, UIState* state) {
    if (!state) return;
    
    if (state->introTimer) {
        KillTimer(hwnd, state->introTimer);
        state->introTimer = 0;
    }
}

static void NextIntroPhase(UIState* state) {
    if (!state) return;
    
    switch (state->introState) {
        case INTRO_BLANK:
            state->introState = INTRO_LOADING;
            state->introAlpha = 0;
            state->holdFrames = 0;
            break;
        case INTRO_LOADING:
            state->introState = INTRO_LOADING_HOLD;
            state->holdFrames = 0;
            break;
        case INTRO_LOADING_HOLD:
            state->introState = INTRO_LOADING_OUT;
            break;
        case INTRO_LOADING_OUT:
            state->introState = INTRO_WELCOME_IN;
            state->glowPosition = 0.1f;
            break;
        case INTRO_WELCOME_IN:
            state->introState = INTRO_WELCOME_GROW;
            state->welcomeGrowFrame = 0;
            break;
        case INTRO_WELCOME_GROW:
            state->introState = INTRO_COMPLETE;
            break;
        default: break;
    }
}

void UI_UpdateIntroAnimation(HWND hwnd, UIState* state) {
    if (!state) return;
    
    switch (state->introState) {
        case INTRO_BLANK:
            state->holdFrames++;
            if (state->holdFrames >= INTRO_HOLD_FRAMES) {
                NextIntroPhase(state);
            }
            break;
            
        case INTRO_LOADING:
            state->introAlpha += INTRO_FADE_STEP;
            if (state->introAlpha >= 255) {
                state->introAlpha = 255;
                NextIntroPhase(state);
            }
            break;
            
        case INTRO_LOADING_HOLD:
            state->holdFrames++;
            if (state->holdFrames >= INTRO_HOLD_FRAMES) {
                NextIntroPhase(state);
            }
            break;
            
        case INTRO_LOADING_OUT:
            state->introAlpha -= INTRO_FADE_STEP;
            if (state->introAlpha <= 0) {
                state->introAlpha = 0;
                NextIntroPhase(state);
            }
            break;
            
        case INTRO_WELCOME_IN:
            state->introAlpha += INTRO_FADE_STEP_FAST;
            state->glowPosition += 0.02f;
            
            if (state->introAlpha >= 255) {
                state->introAlpha = 255;
                NextIntroPhase(state);
            }
            break;

        case INTRO_WELCOME_GROW: {
            state->welcomeGrowFrame++;
            float progress = (float)state->welcomeGrowFrame / (float)GROW_TOTAL_FRAMES;
            if (progress > 1.0f) progress = 1.0f;

            float eased = (1.0f - cosf(progress * 3.14159f)) / 2.0f;
            state->welcomeSize = (int)(30 + eased * (80 - 30));
            state->glowPosition = progress;

            if (state->welcomeGrowFrame >= GROW_TOTAL_FRAMES) {
                state->welcomeSize = 80;
                NextIntroPhase(state);
            }
            break;
        }

        case INTRO_COMPLETE:
            UI_StopIntroTimer(hwnd, state);
            state->showHomeUI = true;
            break;
        default: break;
    }
    
    InvalidateRect(hwnd, NULL, FALSE);
    UpdateWindow(hwnd);
}

static void DrawTextWithGlow(HDC hdc, const wchar_t* text, int x, int y, HFONT hFont,
                             SIZE textSize, int textColor, float glowPos) {
    HFONT oldFont = (HFONT)SelectObject(hdc, hFont);
    
    SetTextColor(hdc, RGB(textColor, textColor, textColor));
    TextOutW(hdc, x, y, text, (int)wcslen(text));
    
    int glowX = x + (int)(textSize.cx * glowPos);
    int glowRadius = 40;
    
    for (int offset = -glowRadius; offset <= glowRadius; offset += 1) {
        int currentX = glowX + offset;
        if (currentX < x || currentX > x + textSize.cx) continue;
        
        float distance = abs(offset) / (float)glowRadius;
        int intensity = (int)(200 * (1.0f - distance));
        intensity = std::min(intensity, 255);
        
        if (intensity > 0) {
            int r = (int)(intensity * (1.2f + 0.3f * glowPos));
            int g = (int)(intensity * 0.5f);
            int b = (int)(intensity * (1.2f - 0.3f * glowPos));
            
            SetTextColor(hdc, RGB(
                std::min(255, textColor + r),
                std::min(255, textColor + g),
                std::min(255, textColor + b)
            ));
            
            RECT clipRect = {currentX - 1, y, currentX + 1, y + textSize.cy};
            ExtTextOutW(hdc, x, y, ETO_CLIPPED, &clipRect, text, (int)wcslen(text), NULL);
        }
    }
    
    SelectObject(hdc, oldFont);
}

void UI_DrawIntroSequence(HDC hdc, const RECT& clientRect, UIState* state) {
    if (!state) return;
    
    HBRUSH whiteBrush = CreateSolidBrush(RGB(255, 255, 255));
    FillRect(hdc, &clientRect, whiteBrush);
    DeleteObject(whiteBrush);

    SetBkMode(hdc, TRANSPARENT);

    const wchar_t* text = (state->introState >= INTRO_WELCOME_IN) ? L"Welcome" : L"Loading...";

    int availableWidth = (state->introState >= INTRO_WELCOME_IN)
                         ? (int)(clientRect.right * 0.50)
                         : (int)(clientRect.right * 0.30);

    int fontSize = 100;
    HFONT hFont = NULL;
    SIZE textSize;

    const wchar_t* fontName = L"Segoe Script";
    int fontWeight = FW_BOLD;

    do {
        if (hFont) DeleteObject(hFont);

        hFont = CreateFont(fontSize, 0, 0, 0, fontWeight,
                          (state->introState < INTRO_WELCOME_IN) ? TRUE : FALSE,
                          FALSE, FALSE,
                          DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                          ANTIALIASED_QUALITY,
                          DEFAULT_PITCH | FF_DONTCARE, fontName);

        if (!hFont) return;

        HFONT oldFont = (HFONT)SelectObject(hdc, hFont);
        GetTextExtentPoint32W(hdc, text, (int)wcslen(text), &textSize);
        SelectObject(hdc, oldFont);

        if (textSize.cx <= availableWidth) break;
        fontSize -= 2;

    } while (fontSize > 10);

    if (state->introState == INTRO_WELCOME_GROW) {
        float growthFactor = 1.0f + ((state->welcomeSize - 30) / 50.0f) * 0.5f;
        int grownSize = (int)(fontSize * growthFactor + 0.01f);

        DeleteObject(hFont);
        hFont = CreateFont(grownSize, 0, 0, 0, fontWeight, FALSE, FALSE, FALSE,
                          DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                          ANTIALIASED_QUALITY,
                          DEFAULT_PITCH | FF_DONTCARE, fontName);
        if (!hFont) return;
    }

    HFONT oldFont = (HFONT)SelectObject(hdc, hFont);
    GetTextExtentPoint32W(hdc, text, (int)wcslen(text), &textSize);
    int x = (clientRect.right - textSize.cx) / 2;
    int y = (clientRect.bottom - textSize.cy) / 2;

    int alpha = std::max(0, std::min(255, state->introAlpha));
    int gray = 255 - alpha;

    if ((state->introState == INTRO_WELCOME_IN || state->introState == INTRO_WELCOME_GROW) && state->glowPosition < 1.0f) {
        DrawTextWithGlow(hdc, text, x, y, hFont, textSize, gray, state->glowPosition);
    } else {
        SetTextColor(hdc, RGB(gray, gray, gray));
        TextOutW(hdc, x, y, text, (int)wcslen(text));
    }

    SelectObject(hdc, oldFont);
    DeleteObject(hFont);
}

void UI_DrawHomeUI(HDC hdc, const RECT& clientRect, UIState* state) {
    if (!state) return;
    
    // Background white area
    HBRUSH bgBrush = CreateSolidBrush(RGB(255, 255, 255));
    FillRect(hdc, &clientRect, bgBrush);
    DeleteObject(bgBrush);

    // Left sidebar (grey)
    RECT sidebar = clientRect;
    sidebar.right = sidebar.left + 60;
    HBRUSH sbBrush = CreateSolidBrush(RGB(210, 210, 210));
    FillRect(hdc, &sidebar, sbBrush);
    DeleteObject(sbBrush);

    // Top header rectangle
    RECT header = clientRect;
    header.left = sidebar.right;
    header.bottom = header.top + 90;
    HBRUSH headerBrush = CreateSolidBrush(RGB(255, 255, 255));
    FillRect(hdc, &header, headerBrush);
    DeleteObject(headerBrush);

    // Header text "Welcome To InvisVM"
    const wchar_t* headerFontName = L"Segoe Script";
    HFONT hHeaderFont = CreateFont(28, 0, 0, 0, FW_BOLD, TRUE, FALSE, FALSE,
                                   DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                   ANTIALIASED_QUALITY,
                                   DEFAULT_PITCH | FF_DONTCARE, headerFontName);
    HFONT oldF = (HFONT)SelectObject(hdc, hHeaderFont);
    SetTextColor(hdc, RGB(30, 30, 30));
    SetBkMode(hdc, TRANSPARENT);
    RECT hdrTextRect = header;
    hdrTextRect.left += 12;
    hdrTextRect.top += 8;
    DrawTextW(hdc, L"Welcome To\nInvisVM", -1, &hdrTextRect, DT_LEFT | DT_TOP);
    SelectObject(hdc, oldF);
    DeleteObject(hHeaderFont);

    // Divider line under header
    HPEN divPen = CreatePen(PS_SOLID, 3, RGB(0, 0, 0));
    HPEN oldDiv = (HPEN)SelectObject(hdc, divPen);
    MoveToEx(hdc, sidebar.right, header.bottom, NULL);
    LineTo(hdc, clientRect.right, header.bottom);
    SelectObject(hdc, oldDiv);
    DeleteObject(divPen);

    // Main content area
    RECT content = clientRect;
    content.top = header.bottom + 8;
    content.left = sidebar.right + 12;
    content.right -= 12;
    content.bottom -= BAR_HEIGHT + 10;

    // Label "Select File Type"
    const wchar_t* labelFontName = L"Segoe UI";
    HFONT labFont = CreateFont(22, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                               DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                               ANTIALIASED_QUALITY,
                               DEFAULT_PITCH | FF_DONTCARE, labelFontName);
    HFONT oldLabelFont = (HFONT)SelectObject(hdc, labFont);
    SetTextColor(hdc, RGB(20, 20, 20));
    RECT lblRect = content;
    lblRect.top += 10;
    DrawTextW(hdc, L"Select File Type", -1, &lblRect, DT_LEFT | DT_TOP);
    SelectObject(hdc, oldLabelFont);
    DeleteObject(labFont);

    // Draw file type buttons
    int btnW = 180;
    int btnH = 36;
    int btnSpacing = 10;
    int startY = content.top + 50;
    
    for (int i = 0; i < FILE_COUNT; i++) {
        int btnX = content.left + 10;
        int btnY = startY + (i * (btnH + btnSpacing));
        
        // Shadow
        RECT shadow = {btnX + 4, btnY + 4, btnX + 4 + btnW, btnY + 4 + btnH};
        HBRUSH shadowBrush = CreateSolidBrush(RGB(200, 200, 200));
        FillRect(hdc, &shadow, shadowBrush);
        DeleteObject(shadowBrush);
        
        // Button rect
        RECT btnRect = {btnX, btnY, btnX + btnW, btnY + btnH};
        state->fileTypeButtons[i] = btnRect;
        
        // Button fill (special color for Application button)
        HBRUSH btnBrush;
        if (state->fileButtonPressed[i]) {
            btnBrush = CreateSolidBrush(RGB(34, 34, 34));
        } else if (state->fileButtonHovered[i]) {
            if (i == FILE_APP) {
                btnBrush = CreateSolidBrush(RGB(220, 240, 255));  // Light blue for app
            } else {
                btnBrush = CreateSolidBrush(RGB(240, 240, 240));
            }
        } else {
            if (i == FILE_APP) {
                btnBrush = CreateSolidBrush(RGB(230, 245, 255));  // Slightly blue tint
            } else {
                btnBrush = CreateSolidBrush(RGB(245, 245, 245));
            }
        }
        FillRect(hdc, &btnRect, btnBrush);
        DeleteObject(btnBrush);
        
        // Button border
        HPEN borderPen = CreatePen(PS_SOLID, 2, RGB(0, 0, 0));
        HPEN oldBorder = (HPEN)SelectObject(hdc, borderPen);
        SelectObject(hdc, GetStockObject(NULL_BRUSH));
        Rectangle(hdc, btnRect.left, btnRect.top, btnRect.right, btnRect.bottom);
        SelectObject(hdc, oldBorder);
        DeleteObject(borderPen);
        
        // Button label
        HFONT btnFont = CreateFont(15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                   DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                   ANTIALIASED_QUALITY,
                                   DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        HFONT oldBtnF = (HFONT)SelectObject(hdc, btnFont);
        SetTextColor(hdc, RGB(20, 20, 20));
        SetBkMode(hdc, TRANSPARENT);
        DrawTextW(hdc, fileTypeInfos[i].label, -1, &btnRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(hdc, oldBtnF);
        DeleteObject(btnFont);
    }

    // Bottom bar (grey background)
    RECT bottomBarRect = clientRect;
    bottomBarRect.top = clientRect.bottom - BAR_HEIGHT;
    HBRUSH greyBrush = CreateSolidBrush(RGB(60, 60, 60));
    FillRect(hdc, &bottomBarRect, greyBrush);
    DeleteObject(greyBrush);

    // Draw the circular buttons on the bottom bar
    for (const auto& button : state->buttons) {
        UI_DrawButton(hdc, button);
    }
}

void UI_DrawButton(HDC hdc, const WindowControlButton& button) {
    HBRUSH brush = CreateSolidBrush(button.GetCurrentColor());
    if (!brush) return;
    
    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, brush);
    
    Ellipse(hdc,
           button.center.x - CIRCLE_RADIUS,
           button.center.y - CIRCLE_RADIUS,
           button.center.x + CIRCLE_RADIUS,
           button.center.y + CIRCLE_RADIUS);
    
    if (button.isActive) {
        HPEN whitePen = CreatePen(PS_SOLID, 2, RGB(255, 255, 255));
        if (whitePen) {
            HPEN oldPen = (HPEN)SelectObject(hdc, whitePen);
            SelectObject(hdc, GetStockObject(NULL_BRUSH));
            
            Ellipse(hdc,
                   button.center.x - CIRCLE_RADIUS - 2,
                   button.center.y - CIRCLE_RADIUS - 2,
                   button.center.x + CIRCLE_RADIUS + 2,
                   button.center.y + CIRCLE_RADIUS + 2);
            
            SelectObject(hdc, oldPen);
            DeleteObject(whitePen);
        }
    }
    
    SelectObject(hdc, oldBrush);
    DeleteObject(brush);
}

void UI_HandleMouseMove(int x, int y, UIState* state) {
    if (!state) return;
    
    if (state->showHomeUI) {
        // Check file type buttons
        for (int i = 0; i < FILE_COUNT; i++) {
            bool overBtn = (x >= state->fileTypeButtons[i].left && x <= state->fileTypeButtons[i].right &&
                           y >= state->fileTypeButtons[i].top && y <= state->fileTypeButtons[i].bottom);
            if (overBtn != state->fileButtonHovered[i]) {
                state->fileButtonHovered[i] = overBtn;
            }
        }
    }
    
    // Handle control button hover
    int newHovered = UI_FindButtonAtPoint(x, y, state);
    
    if (newHovered != state->hoveredButton) {
        if (state->hoveredButton >= 0 && (size_t)state->hoveredButton < state->buttons.size()) {
            if (state->buttons[state->hoveredButton].state == STATE_HOVERED) {
                state->buttons[state->hoveredButton].state = STATE_NORMAL;
            }
        }
        
        if (newHovered >= 0 && (size_t)newHovered < state->buttons.size()) {
            state->buttons[newHovered].state = STATE_HOVERED;
        }
        
        state->hoveredButton = newHovered;
    }
}

bool UI_HandleHomeButtonClick(int x, int y, UIState* state, FileType* selectedType) {
    if (!state || !selectedType) return false;
    
    if (state->showHomeUI) {
        for (int i = 0; i < FILE_COUNT; i++) {
            if (x >= state->fileTypeButtons[i].left && x <= state->fileTypeButtons[i].right &&
                y >= state->fileTypeButtons[i].top && y <= state->fileTypeButtons[i].bottom) {
                state->fileButtonPressed[i] = true;
                state->pressedFileButton = i;
                *selectedType = (FileType)i;
                return true;
            }
        }
    }
    return false;
}

bool UI_HandleHomeButtonRelease(HWND hwnd, int x, int y, UIState* state, HINSTANCE hInstance, FileType selectedType) {
    if (!state) return false;
    
    if (state->showHomeUI && state->pressedFileButton >= 0) {
        int btn = state->pressedFileButton;
        state->fileButtonPressed[btn] = false;
        state->pressedFileButton = -1;

        if (x >= state->fileTypeButtons[btn].left && x <= state->fileTypeButtons[btn].right &&
            y >= state->fileTypeButtons[btn].top && y <= state->fileTypeButtons[btn].bottom) {
            
            // Skip file dialog for application type - handled in main.cpp
            if (selectedType == FILE_APP) {
                return true;
            }
            
            char szFile[MAX_PATH] = {0};
            OPENFILENAMEA ofn = {};
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = hwnd;
            ofn.lpstrFile = szFile;
            ofn.nMaxFile = MAX_PATH;
            ofn.lpstrFilter = fileTypeInfos[selectedType].filter;
            ofn.nFilterIndex = 1;
            ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
            
            if (GetOpenFileNameA(&ofn)) {
                // Create new viewer window with expected type
                HWND viewerWindow = CreatePDFViewerWindow(hInstance, szFile, fileTypeInfos[selectedType].extension);
                if (viewerWindow) {
                    ShowWindow(viewerWindow, SW_SHOW);
                    UpdateWindow(viewerWindow);
                }
            }
        }
        return true;
    }
    return false;
}