#include <windows.h>
#include <fstream>
#include <sstream>
#include <direct.h>
#include <algorithm>
#include "pdf.h"
#include "constants.h"

void PDF_Initialize(PDFState* state) {
    if (!state) return;
    
    state->extractedText = "No PDF loaded. Right-click a PDF file and select 'Open with InvisVM' to view content.";
    state->scrollPos = 0;
    state->maxScrollPos = 0;
    state->pageSize = 10;
    state->lineHeight = LINE_HEIGHT;
    state->textLines.clear();
}

// Process PDF or other files via Python script
bool PDF_ProcessFile(const char* pdfPath, PDFState* state, const char* expectedType) {
    if (!state || !pdfPath || strlen(pdfPath) == 0) return false;

    // Optional type validation
    if (expectedType) {
        std::string ext = pdfPath;
        size_t dotPos = ext.find_last_of('.');
        if (dotPos != std::string::npos) {
            std::string actualExt = ext.substr(dotPos + 1);
            if (_stricmp(actualExt.c_str(), expectedType) != 0) {
                state->extractedText = "Error: Wrong file type.";
                return false;
            }
        }
    }

    // Get executable directory
    char exeDir[MAX_PATH];
    if (GetModuleFileNameA(NULL, exeDir, MAX_PATH) == 0) {
        state->extractedText = "Error: Failed to get executable path.";
        return false;
    }

    char* lastSlash = strrchr(exeDir, '\\');
    if (lastSlash) *lastSlash = '\0';

    if (_chdir(exeDir) != 0) {
        state->extractedText = "Error: Failed to change directory.";
        return false;
    }

    // Check Python script
    std::ifstream scriptCheck("program.py");
    if (!scriptCheck.good()) {
        state->extractedText = "Error: program.py not found in executable directory.";
        return false;
    }
    scriptCheck.close();

    // Build command
    std::string command = "python program.py \"";
    command += pdfPath;
    command += "\" 2>nul"; // suppress stderr

    int result = system(command.c_str());
    if (result != 0) {
        state->extractedText = "Error: Failed to process PDF file. Make sure Python and pypdf are installed.";
        return false;
    }

    // Read extracted text
    std::ifstream file("text.txt");
    if (!file.is_open()) {
        state->extractedText = "Error: Failed to read extracted text file.";
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    state->extractedText = buffer.str();
    file.close();

    // Split into lines
    state->textLines.clear();
    std::stringstream ss(state->extractedText);
    std::string line;
    while (std::getline(ss, line)) state->textLines.push_back(line);

    state->maxScrollPos = std::max(0, (int)state->textLines.size() - state->pageSize);
    state->scrollPos = 0;

    return true;
}

void PDF_DrawContent(HDC hdc, const RECT& clientRect, PDFState* state) {
    if (!state) return;
    
    // Draw status bar
    RECT statusRect = clientRect;
    statusRect.bottom = statusRect.top + 30;
    
    HBRUSH darkBrush = CreateSolidBrush(RGB(30, 30, 30));
    if (darkBrush) {
        FillRect(hdc, &statusRect, darkBrush);
        DeleteObject(darkBrush);
    }
    
    SetTextColor(hdc, RGB(200, 200, 200));
    SetBkMode(hdc, TRANSPARENT);
    
    const char* instructions = "VM Running";
    DrawTextA(hdc, instructions, -1, &statusRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // Draw PDF content
    RECT contentRect = clientRect;
    contentRect.top += 35;
    contentRect.bottom -= BAR_HEIGHT + 5;
    contentRect.left += 10;
    contentRect.right -= 30;
    
    SetTextColor(hdc, RGB(240, 240, 240));
    SetBkMode(hdc, TRANSPARENT);
    
    // Ensure lines are split if needed
    if (state->textLines.empty() && !state->extractedText.empty()) {
        std::stringstream ss(state->extractedText);
        std::string line;
        while (std::getline(ss, line)) {
            state->textLines.push_back(line);
        }
    }
    
    // Draw visible lines
    int visibleLines = (contentRect.bottom - contentRect.top) / state->lineHeight;
    for (int i = 0; i < visibleLines && ((size_t)(state->scrollPos + i) < state->textLines.size()); i++) {
        int y = contentRect.top + (i * state->lineHeight);
        const std::string& line = state->textLines[state->scrollPos + i];
        TextOutA(hdc, contentRect.left, y, line.c_str(), (int)line.length());
    }
}

void PDF_UpdateScrollInfo(const RECT& clientRect, PDFState* state) {
    if (!state || state->textLines.empty()) return;
    
    int visibleLines = (clientRect.bottom - clientRect.top - 35 - BAR_HEIGHT - 5) / state->lineHeight;
    state->pageSize = std::max(1, visibleLines);
    state->maxScrollPos = std::max(0, (int)state->textLines.size() - state->pageSize);
    state->scrollPos = std::min(state->scrollPos, state->maxScrollPos);
}

void PDF_HandleScroll(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, PDFState* state) {
    if (!state) return;
    
    int scrollRequest = LOWORD(wParam);
    int newPos = state->scrollPos;
    
    switch (scrollRequest) {
        case SB_LINEUP:   newPos--; break;
        case SB_LINEDOWN: newPos++; break;
        case SB_PAGEUP:   newPos -= state->pageSize; break;
        case SB_PAGEDOWN: newPos += state->pageSize; break;
        case SB_THUMBTRACK: newPos = HIWORD(wParam); break;
    }
    
    newPos = std::max(0, std::min(newPos, state->maxScrollPos));
    
    if (newPos != state->scrollPos) {
        state->scrollPos = newPos;
        SetScrollPos(hwnd, SB_VERT, state->scrollPos, TRUE);
    }
}

void PDF_HandleMouseWheel(HWND hwnd, WPARAM wParam, PDFState* state) {
    if (!state) return;
    
    int delta = GET_WHEEL_DELTA_WPARAM(wParam);
    int scrollLines = 3;
    
    if (delta > 0) {
        state->scrollPos -= scrollLines;
    } else {
        state->scrollPos += scrollLines;
    }
    
    state->scrollPos = std::max(0, std::min(state->scrollPos, state->maxScrollPos));
    SetScrollPos(hwnd, SB_VERT, state->scrollPos, TRUE);
}
