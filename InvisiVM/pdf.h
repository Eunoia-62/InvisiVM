#ifndef PDF_H
#define PDF_H

#include <windows.h>
#include <string>
#include <vector>
#include "constants.h" // Added: ensure LINE_HEIGHT is defined

// PDF State structure - encapsulates all PDF state for a window
struct PDFState {
    std::string extractedText;
    std::vector<std::string> textLines;
    int scrollPos;
    int maxScrollPos;
    int pageSize;
    int lineHeight;
    std::string filename;
};

// PDF functions - now take PDFState pointer
void PDF_Initialize(PDFState* state);
// FIX: Added the third argument 'const char* expectedType = nullptr'
bool PDF_ProcessFile(const char* pdfPath, PDFState* state, const char* expectedType = nullptr);
void PDF_DrawContent(HDC hdc, const RECT& clientRect, PDFState* state);
void PDF_UpdateScrollInfo(const RECT& clientRect, PDFState* state);
void PDF_HandleScroll(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, PDFState* state);
void PDF_HandleMouseWheel(HWND hwnd, WPARAM wParam, PDFState* state);
bool PDF_IsLoaded(PDFState* state);

#endif