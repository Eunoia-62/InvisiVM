#ifndef UI_H
#define UI_H

#include <windows.h>
#include <vector>
#include <string>
#include <algorithm>
#include "constants.h"
#include "apprun.h"  // Include AppRunState

// File types supported
enum FileType {
    FILE_PDF = 0,
    FILE_TXT = 1,
    FILE_CSV = 2,
    FILE_DOCX = 3,
    FILE_XLSX = 4,
    FILE_APP = 5,      // New: Application embedding
    FILE_COUNT = 6
};

// Intro sequence states
enum IntroState {
    INTRO_BLANK,
    INTRO_LOADING,
    INTRO_LOADING_HOLD,
    INTRO_LOADING_OUT,
    INTRO_WELCOME_IN,
    INTRO_WELCOME_GROW,
    INTRO_COMPLETE
};

// Button states
enum ButtonState {
    STATE_NORMAL = 0,
    STATE_HOVERED = 1,
    STATE_CLICKED = 2,
    STATE_ACTIVE = 3
};

// Button class
class WindowControlButton {
public:
    POINT center;
    COLORREF baseColor;
    ButtonState state;
    bool isActive;
    std::wstring name;

    WindowControlButton(COLORREF color, const std::wstring& buttonName) 
        : baseColor(color), state(STATE_NORMAL), isActive(false), name(buttonName) {
        center.x = center.y = 0;
    }

    bool ContainsPoint(int x, int y) const {
        int dx = x - center.x;
        int dy = y - center.y;
        return dx * dx + dy * dy <= CIRCLE_RADIUS * CIRCLE_RADIUS;
    }

    COLORREF GetCurrentColor() const {
        switch (state) {
            case STATE_HOVERED: return LightenColor(baseColor, 30);
            case STATE_CLICKED: return DarkenColor(baseColor, 30);
            case STATE_ACTIVE:  return LightenColor(baseColor, 50);
            default:
                return isActive ? LightenColor(baseColor, 20) : DarkenColor(baseColor, 40);
        }
    }

    void Toggle() { isActive = !isActive; }
    void Reset() { isActive = false; state = STATE_NORMAL; }

private:
    COLORREF LightenColor(COLORREF color, int amount) const {
        int r = std::min(GetRValue(color) + amount, 255);
        int g = std::min(GetGValue(color) + amount, 255);
        int b = std::min(GetBValue(color) + amount, 255);
        return RGB(r, g, b);
    }

    COLORREF DarkenColor(COLORREF color, int amount) const {
        int r = std::max(GetRValue(color) - amount, 0);
        int g = std::max(GetGValue(color) - amount, 0);
        int b = std::max(GetBValue(color) - amount, 0);
        return RGB(r, g, b);
    }
};

// UI State structure - encapsulates all UI state for a window
struct UIState {
    // Intro animation state
    IntroState introState;
    int introAlpha;
    int welcomeSize;
    UINT_PTR introTimer;
    bool skipIntro;
    int holdFrames;
    bool showMainUIBehind;
    float glowPosition;
    int welcomeGrowFrame;

    // Home UI state
    bool showHomeUI;
    RECT fileTypeButtons[FILE_COUNT];  // Buttons for each file type
    int hoveredFileButton;
    int pressedFileButton;
    bool fileButtonHovered[FILE_COUNT];
    bool fileButtonPressed[FILE_COUNT];

    // Button state
    std::vector<WindowControlButton> buttons;
    int hoveredButton;
    int clickedButton;

    // Embedded app state
    AppRunState embeddedApp;

    UIState() : introState(INTRO_BLANK), introAlpha(0), welcomeSize(30),
                introTimer(0), skipIntro(false), holdFrames(0),
                showMainUIBehind(false), glowPosition(0.0f), welcomeGrowFrame(0),
                showHomeUI(false), hoveredFileButton(-1), pressedFileButton(-1),
                hoveredButton(-1), clickedButton(-1) {
        for (int i = 0; i < FILE_COUNT; i++) {
            fileTypeButtons[i] = {0, 0, 0, 0};
            fileButtonHovered[i] = false;
            fileButtonPressed[i] = false;
        }
    }
};

// UI functions
void UI_Initialize(UIState* state);
void UI_StartIntroTimer(HWND hwnd, UIState* state);
void UI_StopIntroTimer(HWND hwnd, UIState* state);
void UI_UpdateIntroAnimation(HWND hwnd, UIState* state);
void UI_DrawIntroSequence(HDC hdc, const RECT& clientRect, UIState* state);
void UI_DrawHomeUI(HDC hdc, const RECT& clientRect, UIState* state);
void UI_HandleMouseMove(int x, int y, UIState* state);
bool UI_HandleHomeButtonClick(int x, int y, UIState* state, FileType* selectedType);
bool UI_HandleHomeButtonRelease(HWND hwnd, int x, int y, UIState* state, HINSTANCE hInstance, FileType selectedType);
void UI_DrawButton(HDC hdc, const WindowControlButton& button);
void UI_InitializeButtons(UIState* state);
void UI_UpdateButtonPositions(const RECT& clientRect, UIState* state);
int UI_FindButtonAtPoint(int x, int y, UIState* state);

#endif