#include "Desktop.h"

#include "imgui.h"

#include <chrono>
#include <ctime>

void Desktop::draw() {
    // One borderless, full-viewport window that can never be moved, resized,
    // collapsed, or focused above the apps that live on top of it.
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->Pos);
    ImGui::SetNextWindowSize(vp->Size);

    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_NoBackground;  // we paint our own wallpaper instead

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("##Desktop", nullptr, flags);
    ImGui::PopStyleVar();

    drawWallpaper();
    drawClock();

    ImGui::End();
}

void Desktop::drawWallpaper() {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    const ImVec2 p0 = vp->Pos;
    const ImVec2 p1 = ImVec2(vp->Pos.x + vp->Size.x, vp->Pos.y + vp->Size.y);

    // Vertical gradient: deep indigo at the top down to a cool teal-blue.
    const ImU32 top    = IM_COL32(18, 22, 46, 255);
    const ImU32 bottom = IM_COL32(28, 86, 118, 255);
    dl->AddRectFilledMultiColor(p0, p1, top, top, bottom, bottom);

    // Subtle version label, bottom-left, to match the reference mockups.
    const float margin = 16.0f;
    const char* label = "CSOPESY OS v1.0";
    const ImVec2 sz = ImGui::CalcTextSize(label);
    dl->AddText(ImVec2(p0.x + margin, p1.y - sz.y - margin),
                IM_COL32(120, 220, 160, 200), label);
}

void Desktop::drawClock() {
    // Current local time, recomputed every frame so the seconds tick live.
    const std::time_t now = std::time(nullptr);
    std::tm tm{};
    localtime_s(&tm, &now);

    char timeBuf[32];
    char dateBuf[64];
    std::strftime(timeBuf, sizeof(timeBuf), "%I:%M:%S %p", &tm);
    std::strftime(dateBuf, sizeof(dateBuf), "%A, %b %d, %Y", &tm);

    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    const float pad = 12.0f;
    const float margin = 16.0f;
    const float lineGap = 4.0f;

    const ImVec2 timeSz = ImGui::CalcTextSize(timeBuf);
    const ImVec2 dateSz = ImGui::CalcTextSize(dateBuf);
    const float boxW = (timeSz.x > dateSz.x ? timeSz.x : dateSz.x) + pad * 2.0f;
    const float boxH = timeSz.y + dateSz.y + lineGap + pad * 2.0f;

    // Anchor the panel to the top-right corner.
    const float right = vp->Pos.x + vp->Size.x - margin;
    const float top   = vp->Pos.y + margin;
    const ImVec2 boxMin(right - boxW, top);
    const ImVec2 boxMax(right, top + boxH);

    dl->AddRectFilled(boxMin, boxMax, IM_COL32(0, 0, 0, 90), 6.0f);

    // Both lines right-aligned inside the panel.
    const float timeX = boxMax.x - pad - timeSz.x;
    const float timeY = boxMin.y + pad;
    dl->AddText(ImVec2(timeX, timeY), IM_COL32(255, 255, 255, 255), timeBuf);

    const float dateX = boxMax.x - pad - dateSz.x;
    const float dateY = timeY + timeSz.y + lineGap;
    dl->AddText(ImVec2(dateX, dateY), IM_COL32(200, 210, 225, 255), dateBuf);
}
