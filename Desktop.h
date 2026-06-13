#pragma once

// Component 1: The Desktop
//
// The full-screen base layer of the emulator. It is drawn first every frame so
// every other window sits on top of it. Responsibilities:
//   - fill the entire application window with a wallpaper (gradient)
//   - show a real-time clock in a fixed corner
//   - host the PWR button, the *only* sanctioned way to shut the app down
//
// The Desktop does not talk to GLFW directly. When the user confirms shutdown
// it raises a flag; the main loop reads shutdownRequested() and translates it
// into glfwSetWindowShouldClose(). This keeps the UI decoupled from the host.
class Desktop {
public:
    // Render one frame of the desktop. Call once per frame, before any other
    // UI, so it acts as the background layer.
    void draw();

    // True once the user has confirmed shutdown via the PWR button.
    bool shutdownRequested() const { return m_shutdownRequested; }

private:
    void drawWallpaper();
    void drawClock();
    void drawPowerButton();

    bool m_shutdownRequested = false;
};
