#include "BootScreen.h"

#include "imgui.h"

namespace {
constexpr double kBootSeconds = 5.0;
}

bool BootScreen::isComplete() const {
    if (m_skipped) return true;
    if (m_startTime < 0.0) return false;
    return (ImGui::GetTime() - m_startTime) >= kBootSeconds;
}

void BootScreen::draw() {
    if (m_startTime < 0.0)
        m_startTime = ImGui::GetTime();

    const double t = ImGui::GetTime() - m_startTime;

    if (ImGui::IsKeyPressed(ImGuiKey_Space) || ImGui::IsKeyPressed(ImGuiKey_Enter))
        m_skipped = true;

    // Full-viewport black POST screen.
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->Pos);
    ImGui::SetNextWindowSize(vp->Size);

    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0, 0, 0, 255));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(28.0f, 24.0f));
    ImGui::Begin("##BootScreen", nullptr, flags);

    const ImVec4 green = ImVec4(0.47f, 0.90f, 0.55f, 1.0f);
    const ImVec4 dim   = ImVec4(0.60f, 0.65f, 0.60f, 1.0f);
    const ImVec4 white = ImVec4(0.90f, 0.92f, 0.90f, 1.0f);

    // Stage 1 (t >= 0): BIOS header.
    ImGui::TextColored(green, "CSOPESY BIOS v1.0");
    ImGui::TextColored(dim,   "(C) 2026 CSOPESY Megatrends Inc.");
    ImGui::TextColored(dim,   "Released: 06/13/2026");
    ImGui::Spacing();
    ImGui::Spacing();

    // Stage 2 (t >= 1.0): RAM check with a filling progress bar.
    if (t >= 1.0) {
        float p = (float)((t - 1.0) / 1.5);
        if (p > 1.0f) p = 1.0f;
        const int counted = (int)(p * 65536.0f);
        ImGui::TextColored(white, "Checking RAM: %d KB", counted);
        ImGui::ProgressBar(p, ImVec2(360.0f, 0.0f));
        if (p >= 1.0f)
            ImGui::TextColored(green, "  [ OK ] 65536 KB memory verified");
        ImGui::Spacing();
    }

    // Stage 3 (t >= 2.5): hardware enumeration, lines appearing one by one.
    if (t >= 2.5) {
        ImGui::TextColored(white, "Detecting hardware...");
        struct Dev { double at; const char* name; };
        static const Dev devs[] = {
            { 2.6, "CPU      : CSOPESY Virtual Core (16 CPUs)" },
            { 2.8, "Memory   : 65536 KB OK" },
            { 3.0, "Display  : OpenGL 3.0 / GLFW backend" },
            { 3.2, "Storage  : CSOPESY-DISK 0 (virtual)" },
        };
        for (const Dev& d : devs)
            if (t >= d.at)
                ImGui::TextColored(dim, "  [ OK ] %s", d.name);
        ImGui::Spacing();
    }

    // Stage 4 (t >= 3.5): loading OS with animated dots.
    if (t >= 3.5) {
        const int dots = (int)((t - 3.5) / 0.3) % 4;
        ImGui::TextColored(green, "Loading CSOPESY OS%.*s", dots, "...");
    }

    // Skip hint, anchored near the bottom-left.
    ImGui::SetCursorPos(ImVec2(28.0f, vp->Size.y - 40.0f));
    ImGui::TextColored(dim, "Press SPACE to skip");

    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}
