/*
* Copyright (c) [2025] [Eunoia-62]. All rights reserved.
* This file is part of [InvisiVM].
*/



#define UNICODE           // Enable Unicode support for wide character strings (wchar_t)
#include <windows.h>      // Core Windows API functions and types
#include <windowsx.h>     // Extended Windows macros (GET_X_LPARAM, GET_Y_LPARAM)
#include <vector>         // STL vector container for storing button objects
#include <string>         // STL string class for text handling
#include <algorithm>      // STL algorithms (used for min/max operations)
#include <fstream>        // For reading extracted text file
#include <sstream>        // For string stream operations
#include <direct.h>       // For_getcwd

// These constants define the visual layout and behavior of the application

const int BAR_HEIGHT = 40;          // Height of the bottom bar containing buttons (pixels)
const int CIRCLE_RADIUS = 15;       // Radius of each circular button (pixels) - larger for easier clicking
const int CIRCLE_SPACING = 15;      // Space between buttons and window edge (pixels)
const int WINDOW_WIDTH = 800;       // Default window width (pixels)
const int WINDOW_HEIGHT = 600;      // Default window height (pixels)

// Button states represent different visual and interactive states
enum ButtonState {
    STATE_NORMAL = 0,    // Default state - button is visible but not interacted with
    STATE_HOVERED = 1,   // Mouse is over the button - lighter color
    STATE_CLICKED = 2,   // Button is being pressed - darker color
    STATE_ACTIVE = 3     // Button has been activated - brightest color with border
};

// Virtual key codes for keyboard shortcuts - using character codes for simplicity
const int KEY_RED = '1';      // Press '1' to activate red (close) button
const int KEY_YELLOW = '2';   // Press '2' to activate yellow (minimize) button
const int KEY_GREEN = '3';    // Press '3' to activate green (maximize) button
const int KEY_RESET = 'R';    // Press 'R' to reset all button states

// This class encapsulates all data and behavior for a single Mac-style button
// Each button knows its position, color, state, and can render itself
class WindowControlButton {
public:
    POINT center;           // Screen coordinates of button center (x, y pixels)
    COLORREF baseColor;     // Base color of the button (RGB value)
    ButtonState state;      // Current visual/interactive state
    bool isActive;         // Whether button has been clicked and is "on"
    std::wstring name;     // Display name of the button (e.g., "Close", "Minimize")

    // Initialize a new button with specified color and name
    //   color - RGB color value for the button
    //   buttonName - Wide string name for the button (Unicode support)
    WindowControlButton(COLORREF color, const std::wstring& buttonName) 
        : baseColor(color),           // Set the base color
          state(STATE_NORMAL),        // Start in normal state
          isActive(false),           // Start inactive
          name(buttonName) {         // Store the name
        // Initialize center position to origin - will be set later by UpdateButtonPositions()
        center.x = center.y = 0;
    }
    
    // Determines if a given point (mouse coordinates) is inside this button
    // Uses circular hit detection based on the button's radius
    //   x, y - Screen coordinates to test
    // Returns:
    //   true if point is inside the button circle, false otherwise
    bool ContainsPoint(int x, int y) const {
        // Calculate distance from point to button center using Pythagorean theorem
        int dx = x - center.x;      // Horizontal distance
        int dy = y - center.y;      // Vertical distance
        int distanceSquared = dx * dx + dy * dy;  // Square of actual distance
        int radiusSquared = CIRCLE_RADIUS * CIRCLE_RADIUS;  // Square of radius
        
        // Compare squared distances to avoid expensive sqrt() calculation
        return distanceSquared <= radiusSquared;
    }
    
    // Returns the appropriate color for the button based on its current state
    // Each state has a different visual appearance to provide user feedback
    // Returns:
    //   COLORREF - RGB color value to use for rendering
    COLORREF GetCurrentColor() const {
        switch (state) {
            case STATE_HOVERED:
                // Mouse is hovering - make color brighter to show interactivity
                return LightenColor(baseColor, 30);
                
            case STATE_CLICKED:
                // Button is being pressed - make color darker for "pressed" effect
                return DarkenColor(baseColor, 30);
                
            case STATE_ACTIVE:
                // Button is active - make very bright to show it's "on"
                return LightenColor(baseColor, 50);
                
            default: // STATE_NORMAL
                // Normal state depends on whether button is logically active
                if (isActive) {
                    return LightenColor(baseColor, 20);  // Slightly bright if active
                } else {
                    return DarkenColor(baseColor, 40);   // Dimmed if inactive
                }
        }
    }

    // Toggle the active state of the button (on/off)
    void Toggle() {
        isActive = !isActive;  // Flip the boolean state
    }
    
    // Reset button to inactive, normal state
    // Used when user presses 'R' to reset all buttons
    void Reset() {
        isActive = false;           // Turn off
        state = STATE_NORMAL;       // Return to normal visual state
    }
    
private:
    // These methods modify colors for different visual states
    // They work with RGB components individually to avoid color overflow
    // Makes a color brighter by adding to each RGB component
    //   color - Original RGB color
    //   amount - How much to brighten (0-255)
    // Returns:
    //   COLORREF - Brightened color with clamping to prevent overflow
    COLORREF LightenColor(COLORREF color, int amount) const {
        // Extract individual color components from RGB value
        int r = GetRValue(color);   // Red component (0-255)
        int g = GetGValue(color);   // Green component (0-255)
        int b = GetBValue(color);   // Blue component (0-255)
        
        // Add brightness amount, but clamp to maximum of 255 to prevent overflow
        r = (r + amount > 255) ? 255 : (r + amount);
        g = (g + amount > 255) ? 255 : (g + amount);
        b = (b + amount > 255) ? 255 : (b + amount);
        
        // Reconstruct RGB value from components
        return RGB(r, g, b);
    }
    
    // Makes a color darker by subtracting from each RGB component
    //   color - Original RGB color
    //   amount - How much to darken (0-255)
    // Returns:
    //   COLORREF - Darkened color with clamping to prevent underflow
    COLORREF DarkenColor(COLORREF color, int amount) const {
        // Extract individual color components
        int r = GetRValue(color);
        int g = GetGValue(color);
        int b = GetBValue(color);
        
        // Subtract darkness amount, but clamp to minimum of 0 to prevent underflow
        r = (r - amount < 0) ? 0 : (r - amount);
        g = (g - amount < 0) ? 0 : (g - amount);
        b = (b - amount < 0) ? 0 : (b - amount);
        
        // Reconstruct RGB value
        return RGB(r, g, b);
    }
};

// These variables maintain the application state across function calls
// In a larger application, these would likely be encapsulated in a class

std::vector<WindowControlButton> g_buttons;  // Container holding all three buttons
int g_hoveredButton = -1;                    // Index of currently hovered button (-1 = none)
int g_clickedButton = -1;                    // Index of button being clicked (-1 = none)
HWND g_hwnd = NULL;                          // Handle to the main window (needed for updates)
std::string g_extractedText = "No PDF loaded. Right-click a PDF file and select 'Open with EmptyApp' to view content.";  // Store extracted PDF text
// Scrolling variables for custom text display
int g_scrollPos = 0;                         // Current scroll position (line number)
int g_maxScrollPos = 0;                      // Maximum scroll position
int g_pageSize = 10;                         // Lines visible per page
int g_lineHeight = 16;                       // Height of each text line in pixels
std::vector<std::string> g_textLines;        // Store text split into individual lines
// These functions handle the business logic of button management

// Creates and initializes the three control buttons
// Called once during window creation (WM_CREATE message)
void InitializeButtons() {
    try {
        // Clear any existing buttons (safety measure)
        g_buttons.clear();
        
        // Create the three buttons with colors and names
        // Note: Using emplace_back for efficiency - constructs objects directly in vector
        
        // Red button - traditionally the "Close" button in Mac UI
        g_buttons.emplace_back(RGB(255, 95, 87), L"Close");
        
        // Yellow button - traditionally the "Minimize" button in Mac UI  
        g_buttons.emplace_back(RGB(255, 189, 46), L"Minimize");
        
        // Green button - traditionally the "Maximize/Zoom" button in Mac UI
        g_buttons.emplace_back(RGB(40, 201, 64), L"Maximize");
        
    }
    catch (const std::exception& e) {
        // If anything goes wrong during initialization, show error message
        // Using MessageBoxA (ANSI) to avoid Unicode complications in error handling
        MessageBoxA(NULL, "Failed to initialize buttons", "Error", MB_OK | MB_ICONERROR);
    }
}

// Process PDF file through Python script and load extracted text
// Called when app is launched with PDF file path as argument
void ProcessPDFFile(const char* pdfPath) {
    try {
        // Get the directory where the executable is located
        char exeDir[MAX_PATH];
        GetModuleFileNameA(NULL, exeDir, MAX_PATH);
        // Remove the executable name to get just the directory
        char* lastSlash = strrchr(exeDir, '\\');
        if (lastSlash) *lastSlash = '\0';
        
        // Change working directory to where the exe is located
        _chdir(exeDir);
        
        // Now call Python (program.py will be found)
        char command[1000];
        sprintf(command, "python program.py \"%s\"", pdfPath);
        int result = system(command);
        
        if (result == 0) {
            std::ifstream file("text.txt");
            if (file.is_open()) {
                std::stringstream buffer;
                buffer << file.rdbuf();
                g_extractedText = buffer.str();
                file.close();
                
                // Split text into lines for scrolling
                std::stringstream ss(g_extractedText);
                std::string line;
                g_textLines.clear();
                while (std::getline(ss, line)) {
                    g_textLines.push_back(line);
                }
                
                // Update scroll range
                g_maxScrollPos = g_textLines.size() - g_pageSize;
                if (g_maxScrollPos < 0) g_maxScrollPos = 0;
                
            } else {
                g_extractedText = "Error: Text file not found.";
            }
        }
        
        if (g_hwnd) {
            InvalidateRect(g_hwnd, NULL, FALSE);
        }
        
    } catch (const std::exception& e) {
        g_extractedText = "Error: Exception occurred.";
    }
}

// Calculates and sets the screen positions for all buttons based on window size
// Called whenever the window is resized (WM_SIZE message)
// Parameters:
//   clientRect - The current client area rectangle of the window
void UpdateButtonPositions(const RECT& clientRect) {
    // Safety check - don't proceed if buttons aren't initialized
    if (g_buttons.empty()) return;
    
    try {
        // Calculate the vertical position - buttons go in the bottom bar
        int bottomBarTop = clientRect.bottom - BAR_HEIGHT;  // Top of bottom bar
        int centerY = bottomBarTop + BAR_HEIGHT / 2;        // Vertical center of bar
        
        // Position buttons from right to left across the bottom bar
        // We iterate in reverse order to place them: Green, Yellow, Red (right to left)
        for (int i = 0; i < 3; i++) {
            int index = 2 - i;  // Reverse the index: 0->2, 1->1, 2->0
            
            // Calculate horizontal position
            // Start from right edge, subtract spacing, radius, then button spacing
            g_buttons[index].center.x = clientRect.right           // Right edge of window
                                      - CIRCLE_SPACING             // Space from edge
                                      - CIRCLE_RADIUS              // Button radius
                                      - (i * (CIRCLE_RADIUS * 2 + CIRCLE_SPACING)); // Previous buttons
            
            // Set vertical position (same for all buttons)
            g_buttons[index].center.y = centerY;
        }
        
    }
    catch (const std::exception& e) {
        // Report positioning errors
        MessageBoxA(NULL, "Failed to update button positions", "Error", MB_OK | MB_ICONERROR);
    }
}

// Finds which button (if any) contains the given mouse coordinates
// Used for hover detection and click handling
//   x, y - Mouse coordinates to test
// Returns:
//   Index of the button containing the point, or -1 if no button contains it
int FindButtonAtPoint(int x, int y) {
    // Check each button in order
    for (size_t i = 0; i < g_buttons.size(); i++) {
        if (g_buttons[i].ContainsPoint(x, y)) {
            return static_cast<int>(i);  // Return index of first button that contains point
        }
    }
    return -1;  // No button contains this point
}

// Updates button states based on mouse movement for hover effects
// Called on every WM_MOUSEMOVE message
//   x, y - Current mouse coordinates
void HandleMouseMove(int x, int y) {
    // Find which button (if any) is under the mouse cursor
    int newHovered = FindButtonAtPoint(x, y);
    
    // Only update if the hovered button has changed (efficiency)
    if (newHovered != g_hoveredButton) {
        
        // Reset the previously hovered button to normal state
        if (g_hoveredButton >= 0 && g_hoveredButton < g_buttons.size()) {
            // Only reset if it's currently in hovered state (avoid overriding other states)
            if (g_buttons[g_hoveredButton].state == STATE_HOVERED) {
                g_buttons[g_hoveredButton].state = STATE_NORMAL;
            }
        }
        
        // Set the new button to hovered state
        if (newHovered >= 0 && newHovered < g_buttons.size()) {
            g_buttons[newHovered].state = STATE_HOVERED;
        }
        
        // Update tracking variable
        g_hoveredButton = newHovered;
        
        // Request a repaint to show the visual changes
        // FALSE parameter means don't erase background (more efficient)
        InvalidateRect(g_hwnd, NULL, FALSE);
    }
}

// Executes the actual functionality when a button is clicked
// This is where the Mac-style buttons perform their actual window operations
//   buttonIndex - Index of the button that was clicked (0=Red, 1=Yellow, 2=Green)
void HandleButtonClick(int buttonIndex) {
    // Validate the button index to prevent crashes
    if (buttonIndex < 0 || buttonIndex >= g_buttons.size()) return;
    
    try {
        // Play system sound for user feedback
        MessageBeep(MB_OK);  // Standard Windows "OK" beep sound
        
        // Execute different actions based on which button was clicked
        switch (buttonIndex) {
            case 0: // Red button - Close window
                // Send WM_CLOSE message to trigger proper shutdown sequence
                // This allows the application to clean up resources before closing
                PostMessage(g_hwnd, WM_CLOSE, 0, 0);
                break;
                
            case 1: // Yellow button - Minimize window
                // Minimize the window to the taskbar
                ShowWindow(g_hwnd, SW_MINIMIZE);
                // Update window title to indicate state (optional feedback)
                SetWindowTextA(g_hwnd, "InvisVM");
                break;
                
            case 2: // Green button - Maximize or Restore window
                // Check current window state to decide action
                if (IsZoomed(g_hwnd)) {
                    // Window is maximized, so restore to normal size
                    ShowWindow(g_hwnd, SW_RESTORE);
                    SetWindowTextA(g_hwnd, "InvisVM");
                } else {
                    // Window is normal size, so maximize it
                    ShowWindow(g_hwnd, SW_MAXIMIZE);
                    SetWindowTextA(g_hwnd, "InvisVM");
                }
                break;
        }
        
    }
    catch (const std::exception& e) {
        // Handle any errors during button click processing
        MessageBoxA(NULL, "Failed to handle button click", "Error", MB_OK | MB_ICONERROR);
    }
}

// Processes keyboard shortcuts for button activation
// Called on WM_KEYDOWN messages
//   key - Virtual key code of the pressed key
void HandleKeyPress(WPARAM key) {
    try {
        // Check which key was pressed and execute corresponding action
        switch (key) {
            case KEY_RED:    // '1' key
                HandleButtonClick(0);  // Activate red (close) button
                break;
                
            case KEY_YELLOW: // '2' key
                HandleButtonClick(1);  // Activate yellow (minimize) button
                break;
                
            case KEY_GREEN:  // '3' key
                HandleButtonClick(2);  // Activate green (maximize) button
                break;
                
            case KEY_RESET:  // 'R' key
                // Reset all buttons to inactive state
                for (auto& button : g_buttons) {
                    button.Reset();  // Call reset method on each button
                }
                // Update window title and request repaint
                SetWindowText(g_hwnd, L"PDF Viewer - Reset");
                InvalidateRect(g_hwnd, NULL, FALSE);
                break;
                
            // Note: No default case needed - we ignore unrecognized keys
        }
        
    }
    catch (const std::exception& e) {
        // Handle keyboard processing errors
        MessageBoxA(NULL, "Failed to handle key press", "Error", MB_OK | MB_ICONERROR);
    }
}

// These functions handle drawing the user interface

// Draws a single button with its current visual state
// Called during WM_PAINT for each button
//   hdc - Device context for drawing
//   button - Reference to the button object to draw
void DrawButton(HDC hdc, const WindowControlButton& button) {
    try {
        // Create a brush (fill pattern) with the button's current color
        HBRUSH brush = CreateSolidBrush(button.GetCurrentColor());
        if (!brush) return;  // Safety check - bail if brush creation failed
        
        // Select the brush into the device context and save the old one
        HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, brush);
        
        // Draw the circular button
        // Ellipse() draws a filled circle when width equals height
        Ellipse(hdc,
            button.center.x - CIRCLE_RADIUS,  // Left edge
            button.center.y - CIRCLE_RADIUS,  // Top edge  
            button.center.x + CIRCLE_RADIUS,  // Right edge
            button.center.y + CIRCLE_RADIUS); // Bottom edge
        
        // Draw a white border around active buttons for extra visual feedback
        if (button.isActive) {
            // Create a white pen for the border
            HPEN whitePen = CreatePen(PS_SOLID, 2, RGB(255, 255, 255));
            if (whitePen) {
                // Select pen and null brush (outline only)
                HPEN oldPen = (HPEN)SelectObject(hdc, whitePen);
                SelectObject(hdc, GetStockObject(NULL_BRUSH));
                
                // Draw border circle slightly larger than the button
                Ellipse(hdc,
                    button.center.x - CIRCLE_RADIUS - 2,  // Expand by 2 pixels
                    button.center.y - CIRCLE_RADIUS - 2,
                    button.center.x + CIRCLE_RADIUS + 2,
                    button.center.y + CIRCLE_RADIUS + 2);
                
                // Restore original pen and clean up
                SelectObject(hdc, oldPen);
                DeleteObject(whitePen);
            }
        }
        
        // Restore original brush and clean up resources
        SelectObject(hdc, oldBrush);
        DeleteObject(brush);
        
    }
    catch (const std::exception& e) {
        // Silent fail for rendering errors to avoid disrupting the paint cycle
        // Logging could be added here in a more robust application
    }
}

// Draws the instruction text at the top of the window
//   hdc - Device context for drawing
//   clientRect - Current window client rectangle
void DrawStatusBar(HDC hdc, const RECT& clientRect) {
    try {
        // Define rectangle for status bar at top of window
        RECT statusRect = clientRect;
        statusRect.bottom = statusRect.top + 30;  // Make it 30 pixels tall
        
        // Fill status bar with dark background
        HBRUSH darkBrush = CreateSolidBrush(RGB(30, 30, 30));
        if (darkBrush) {
            FillRect(hdc, &statusRect, darkBrush);
            DeleteObject(darkBrush);  // Clean up brush
        }
        
        // Set up text drawing properties
        SetTextColor(hdc, RGB(200, 200, 200));  // Light gray text
        SetBkMode(hdc, TRANSPARENT);            // Don't draw text background
        
        // Draw instruction text centered in the status bar
        const char* instructions = "VM Running";
        DrawTextA(hdc, instructions, -1, &statusRect, 
                 DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        
    }
    catch (const std::exception& e) {
        // Silent fail for rendering errors
    }
}

// Draws the extracted PDF text in the main content area
//   hdc - Device context for drawing  
//   clientRect - Current window client rectangle
void DrawPDFContent(HDC hdc, const RECT& clientRect) {
    try {
        RECT contentRect = clientRect;
        contentRect.top += 35;
        contentRect.bottom -= BAR_HEIGHT + 5;
        contentRect.left += 10;
        contentRect.right -= 30;  // Leave space for scrollbar
        
        SetTextColor(hdc, RGB(240, 240, 240));
        SetBkMode(hdc, TRANSPARENT);
        
        // Split text into lines if not already done
        if (g_textLines.empty() && !g_extractedText.empty()) {
            std::stringstream ss(g_extractedText);
            std::string line;
            while (std::getline(ss, line)) {
                g_textLines.push_back(line);
            }
        }
        
        // Draw visible lines
        int visibleLines = (contentRect.bottom - contentRect.top) / g_lineHeight;
        for (int i = 0; i < visibleLines && (g_scrollPos + i) < g_textLines.size(); i++) {
            int y = contentRect.top + (i * g_lineHeight);
            TextOutA(hdc, contentRect.left, y, 
                    g_textLines[g_scrollPos + i].c_str(), 
                    g_textLines[g_scrollPos + i].length());
        }
        
    } catch (const std::exception& e) {
        // Silent fail
    }
}
// This is the main message handler for the window - it receives and processes
// all Windows messages sent to our window (mouse clicks, key presses, paint requests, etc.)
//   hwnd - Handle to the window receiving the message
//   msg - Message type (WM_PAINT, WM_LBUTTONDOWN, etc.)
//   wParam, lParam - Message-specific data
// Returns:
//   LRESULT - Message-specific return value
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    // Static variable persists between function calls
    static RECT clientRect;  // Stores current window size
    
    // Process different message types
    switch (msg) {
        
        // WINDOW CREATION
        case WM_CREATE:
            g_hwnd = hwnd;
            InitializeButtons();
            
            // Create vertical scrollbar
            SetScrollRange(hwnd, SB_VERT, 0, 100, FALSE);
            SetScrollPos(hwnd, SB_VERT, 0, TRUE);
            ShowScrollBar(hwnd, SB_VERT, TRUE);
            
            return 0;

        // WINDOW RESIZING
        case WM_SIZE:
            // Window size has changed - update button positions
            GetClientRect(hwnd, &clientRect);        // Get new window size
            UpdateButtonPositions(clientRect);       // Recalculate button positions
            InvalidateRect(hwnd, NULL, TRUE);        // Request full repaint
            return 0;

        // BACKGROUND ERASING
        case WM_ERASEBKGND: {
            // Windows wants to erase the background before painting
            HDC hdc = (HDC)wParam;  // Device context for drawing
            if (hdc) {
                // Fill entire client area with black
                FillRect(hdc, &clientRect, (HBRUSH)GetStockObject(BLACK_BRUSH));
                return 1;  // Tell Windows we handled background erasing
            }
            break;
        }

        // WINDOW PAINTING
        case WM_PAINT: {
            // Windows needs us to redraw (part of) the window
            PAINTSTRUCT ps;  // Structure containing paint information
            HDC hdc = BeginPaint(hwnd, &ps);  // Start painting and get device context
            
            if (hdc) {
                try {
                    // Fill background with black (redundant but ensures clean appearance)
                    FillRect(hdc, &clientRect, (HBRUSH)GetStockObject(BLACK_BRUSH));
                    
                    // Draw the instruction text at top
                    DrawStatusBar(hdc, clientRect);
                    
                    // Draw the extracted PDF content in main area
                    DrawPDFContent(hdc, clientRect);
                    
                    // Draw the bottom bar where buttons live
                    RECT bottomBarRect = clientRect;
                    bottomBarRect.top = clientRect.bottom - BAR_HEIGHT;  // Only bottom portion
                    
                    // Fill bottom bar with dark gray
                    HBRUSH greyBrush = CreateSolidBrush(RGB(60, 60, 60));
                    if (greyBrush) {
                        FillRect(hdc, &bottomBarRect, greyBrush);
                        DeleteObject(greyBrush);  // Clean up
                    }
                    
                    // Draw each button
                    for (const auto& button : g_buttons) {
                        DrawButton(hdc, button);
                    }
                    
                }
                catch (const std::exception& e) {
                    // Handle painting errors
                    MessageBoxA(hwnd, "Painting error occurred", "Error", MB_OK | MB_ICONERROR);
                }
            }
            
            EndPaint(hwnd, &ps);  // Finish painting
            return 0;
        }

        // MOUSE MOVEMENT
        case WM_MOUSEMOVE: {
            // Mouse cursor has moved - check for button hovering
            int x = GET_X_LPARAM(lParam);  // Extract X coordinate
            int y = GET_Y_LPARAM(lParam);  // Extract Y coordinate
            HandleMouseMove(x, y);         // Process hover effects
            return 0;
        }

        // MOUSE BUTTON DOWN
        case WM_LBUTTONDOWN: {
            // Left mouse button pressed - start click sequence
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);
            g_clickedButton = FindButtonAtPoint(x, y);  // Which button was clicked?
            
            if (g_clickedButton >= 0) {
                // A button was clicked - show visual feedback
                g_buttons[g_clickedButton].state = STATE_CLICKED;
                SetCapture(hwnd);  // Capture mouse so we get LBUTTONUP even if cursor moves
                InvalidateRect(hwnd, NULL, FALSE);  // Request repaint
            }
            return 0;
        }

        // MOUSE BUTTON UP
        case WM_LBUTTONUP: {
            // Left mouse button released - complete click if still over same button
            if (g_clickedButton >= 0) {
                int x = GET_X_LPARAM(lParam);
                int y = GET_Y_LPARAM(lParam);
                
                // Only execute click if mouse is still over the same button
                // This prevents accidental clicks when user drags off button
                if (FindButtonAtPoint(x, y) == g_clickedButton) {
                    HandleButtonClick(g_clickedButton);  // Execute button function
                }
                
                // Reset click tracking
                g_clickedButton = -1;
                ReleaseCapture();  // Stop capturing mouse
            }
            return 0;
        }

        // KEYBOARD INPUT
        case WM_KEYDOWN:
            // A key was pressed - check for shortcuts
            HandleKeyPress(wParam);  // Process keyboard shortcuts
            return 0;

        // WINDOW DESTRUCTION
        case WM_VSCROLL: {
            int scrollRequest = LOWORD(wParam);
            int newPos = g_scrollPos;
            
            switch (scrollRequest) {
                case SB_LINEUP:   newPos--; break;
                case SB_LINEDOWN: newPos++; break;
                case SB_PAGEUP:   newPos -= g_pageSize; break;
                case SB_PAGEDOWN: newPos += g_pageSize; break;
                case SB_THUMBTRACK: newPos = HIWORD(wParam); break;
            }
            
            if (newPos < 0) newPos = 0;
            if (newPos > g_maxScrollPos) newPos = g_maxScrollPos;
            
            if (newPos != g_scrollPos) {
                g_scrollPos = newPos;
                SetScrollPos(hwnd, SB_VERT, g_scrollPos, TRUE);
                InvalidateRect(hwnd, NULL, FALSE);
            }
            return 0;
        }
        
        case WM_MOUSEWHEEL: {
            int delta = GET_WHEEL_DELTA_WPARAM(wParam);
            int scrollLines = 3;  // Number of lines to scroll per wheel notch
            
            if (delta > 0) {
                // Scroll up
                g_scrollPos -= scrollLines;
            } else {
                // Scroll down  
                g_scrollPos += scrollLines;
            }
            
            // Clamp scroll position to valid range
            if (g_scrollPos < 0) g_scrollPos = 0;
            if (g_scrollPos > g_maxScrollPos) g_scrollPos = g_maxScrollPos;
            
            // Update scrollbar position
            SetScrollPos(hwnd, SB_VERT, g_scrollPos, TRUE);
            
            // Redraw content
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }

        // DEFAULT HANDLING
        default:
            // For all other messages, use Windows default processing
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    
    // Should rarely reach here, but return default processing just in case
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// This is the main function for Windows GUI applications (equivalent to main())
// Windows calls this function when the program starts
//   hInstance - Handle to current application instance
//   hPrevInstance - Legacy parameter (always NULL in modern Windows)
//   lpCmdLine - Command line arguments as string
//   nCmdShow - How to display the window (minimized, maximized, etc.)
// Returns:
//   int - Exit code (0 = success)
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Define the window class name (must be unique)
    const wchar_t CLASS_NAME[] = L"PDFViewerApp";

    try {
        // WINDOW CLASS REGISTRATION
        // Before creating windows, we must register a window class that defines
        // the window's behavior and appearance
        
        WNDCLASS wc = {};  // Initialize structure to zero
        
        // Fill in window class properties
        wc.lpfnWndProc = WndProc;                            // Our message handler function
        wc.hInstance = hInstance;                            // Application instance
        wc.lpszClassName = CLASS_NAME;                       // Unique class name
        wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH); // Default background
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);           // Default cursor
        wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);         // Default icon

        // Register the window class with Windows
        if (!RegisterClass(&wc)) {
            MessageBoxA(NULL, "Failed to register window class", "Error", MB_OK | MB_ICONERROR);
            return -1;  // Exit with error code
        }

        // Check if a PDF file path was passed as command line argument
        if (strlen(lpCmdLine) > 0) {
            // Remove quotes if present in file path
            char* pdfPath = lpCmdLine;
            if (pdfPath[0] == '"') {
                pdfPath++;
                pdfPath[strlen(pdfPath)-1] = '\0';
            }
            
            // Process the PDF file through Python script before creating window
            ProcessPDFFile(pdfPath);
        }

        // WINDOW CREATION
        // Create the actual window using our registered class
        
        HWND hwnd = CreateWindowEx(
            0,                          // Extended window styles (none)
            CLASS_NAME,                 // Window class name
            L"InvisVM",              // Window title
            WS_OVERLAPPEDWINDOW,        // Standard window with title bar, borders, etc.
            CW_USEDEFAULT,              // X position (let Windows choose)
            CW_USEDEFAULT,              // Y position (let Windows choose)  
            WINDOW_WIDTH,               // Width in pixels
            WINDOW_HEIGHT,              // Height in pixels
            NULL,                       // Parent window (none - this is top-level)
            NULL,                       // Menu handle (none)
            hInstance,                  // Application instance
            NULL                        // Additional data (none)
        );

        // Check if window creation succeeded
        if (!hwnd) {
            MessageBoxA(NULL, "Failed to create window", "Error", MB_OK | MB_ICONERROR);
            return -1;  // Exit with error code
        }

        // WINDOW DISPLAY
        // Make the window visible and give it input focus
        
        ShowWindow(hwnd, nCmdShow);  // Show window (normal, minimized, maximized, etc.)
        UpdateWindow(hwnd);          // Force immediate paint by sending WM_PAINT

        // MESSAGE LOOP
        // This is the heart of every Windows application - the message loop
        // It continuously retrieves and processes messages until the program exits
        
        MSG msg;  // Structure to hold message information
        
        // GetMessage() retrieves the next message from the message queue
        // Returns > 0 for normal messages, 0 for WM_QUIT, < 0 for errors
        while (GetMessage(&msg, NULL, 0, 0) > 0) {
            
            // TranslateMessage() converts keyboard messages into character messages
            // For example, converts WM_KEYDOWN into WM_CHAR messages
            TranslateMessage(&msg);
            
            // DispatchMessage() sends the message to the appropriate window procedure
            // In our case, this calls WndProc() with the message
            DispatchMessage(&msg);
        }

        // When we exit the message loop, return the exit code
        // msg.wParam contains the exit code from PostQuitMessage()
        return (int)msg.wParam;
        
    }
    catch (const std::exception& e) {
        // Catch any unexpected errors during program execution
        MessageBoxA(NULL, "Unexpected error occurred", "Fatal Error", MB_OK | MB_ICONERROR);
        return -1;  // Exit with error code
    }
}    
