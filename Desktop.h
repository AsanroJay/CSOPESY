#pragma once

// Component 1: The Desktop
//
// The full-screen base layer of the emulator. It is drawn first every frame so
// every other window sits on top of it. Responsibilities:
//   - fill the entire application window with a wallpaper (gradient)
//   - show a real-time clock in a fixed corner
//
// (The PWR / shutdown button lives in the taskbar, in main_gui.cpp.)
class Desktop {
public:
    // Render one frame of the desktop. Call once per frame, before any other
    // UI, so it acts as the background layer.
    void draw();

private:
    void drawWallpaper();
    void drawClock();
};
