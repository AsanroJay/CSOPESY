// CSOPESY - Dear ImGui + GLFW + OpenGL3 bootstrap.
//
// This is the minimal "does my toolchain work?" entry point for the desktop
// GUI milestone. It opens a window and shows the ImGui demo. The numbered
// comments map to the five OS-loading phases from the handout (pages 10-12),
// so you can grow this into GUIApplication / UIManager / Desktop later.

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include "Desktop.h"
#include "BootScreen.h"

#include <GLFW/glfw3.h>
#include <cstdio>

static void glfw_error_callback(int error, const char* description) {
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

// Boot/init logging to the attached console (mirrors the lecture demo).
// fflush so lines appear immediately even when stdout is redirected/piped.
static void bootLog(const char* tag, const char* msg) {
    std::printf("[%s] %s\n", tag, msg);
    std::fflush(stdout);
}

int main() {
    // === Phase 1: Bootstrapping - window system + GL context ===============
    bootLog("BOOT", "Bootstrapping CSOPESY...");
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return 1;
    bootLog("GUI", "GLFW initialized");

    // OpenGL 3.0+ / GLSL 130. Good baseline for the ImGui OpenGL3 backend.
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    GLFWwindow* window = glfwCreateWindow(1280, 720, "CSOPESY Desktop OS Emulator", nullptr, nullptr);
    if (window == nullptr)
        return 1;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable VSync
    bootLog("GUI", "OpenGL context created (v3.0)");
    bootLog("GUI", "Window created (1280x720)");
    bootLog("GUI", "VSync enabled");

    // === Phase 2: Kernel Init - create ImGui context, install backends =====
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);
    bootLog("GUI", "ImGui context created");
    bootLog("GUI", "GLFW + OpenGL3 backends attached");
    bootLog("GUI", "Dark theme applied");

    // === Phase 3: System Services - construct the Desktop (Component 1) ====
    Desktop desktop;
    BootScreen boot;
    ImVec4 clear_color = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
    bootLog("GUI", "Desktop environment constructed");
    bootLog("Taskbar", "Taskbar ready (S1, S2, Task Manager)");
    bootLog("BOOT", "System fully initialized and ready!");

    // Variables for tracking active UI Screens
	bool active_screen_1 = false;
	bool active_screen_2 = false;
	bool active_task_manager = false;

    // === Phase 4: Main Loop - poll input, build UI, render frame ===========
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);

        if (!boot.isComplete()) {
            // Boot sequence runs first; the desktop appears once it finishes.
            boot.draw();
        } else {

        // --- Component 1: the Desktop is the base layer, drawn first. ------
        desktop.draw();

		// --- Component 2: buttons for unique UI screens and Task Manager ---
		if (active_screen_1) {
            ImGui::Begin("System Information", &active_screen_1);
            
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "System Information");
            ImGui::Separator();

            auto row = [](const char* label, const char* value) {
                ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("%s", label);
                ImGui::TableSetColumnIndex(1); ImGui::Text("%s", value);
            };

            if (ImGui::BeginTable("sysinfo", 2, ImGuiTableFlags_SizingFixedFit)) {
                ImGui::TableNextRow(); row("Computer Name:",        "CSOPESY-PC");
                ImGui::TableNextRow(); row("Operating System:",     "CSOPESY OS 1.0 64-bit");
                ImGui::TableNextRow(); row("Language:",             "English (Regional Setting: English)");
                ImGui::TableNextRow(); row("System Manufacturer:",  "CSOPESY");
                ImGui::TableNextRow(); row("System Model:",         "Emulator v1.0");
                ImGui::TableNextRow(); row("BIOS:",                 "CSOPESY BIOS v1.0");
                ImGui::TableNextRow(); row("Processor:",            "AMD Ryzen 7 260w/ Radeon 780M Graphics (16 CPUs), ~3.8GHz");
                ImGui::TableNextRow(); row("Memory:",               "16384MB RAM");
                ImGui::TableNextRow(); row("Page file:",            "19556MB used, 6960MB available");
                ImGui::TableNextRow(); row("DirectX Version:",      "DirectX 12");
                ImGui::EndTable();
            }

            ImGui::End();
        }
        if (active_screen_2) {
            ImGui::Begin("System Log", &active_screen_2);
            
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "System Boot Log");
            ImGui::Separator();

            // Phase 1
            ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.0f, 1.0f), "[PHASE 1] Bootstrapping");
            ImGui::TextDisabled("  [ OK ] GLFW initialized");
            ImGui::TextDisabled("  [ OK ] OpenGL context created (v3.0)");
            ImGui::TextDisabled("  [ OK ] Window created (1280x720)");
            ImGui::TextDisabled("  [ OK ] VSync enabled");
            ImGui::Separator();

            // Phase 2
            ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.0f, 1.0f), "[PHASE 2] Kernel Initialization");
            ImGui::TextDisabled("  [ OK ] ImGui context created");
            ImGui::TextDisabled("  [ OK ] Keyboard navigation enabled");
            ImGui::TextDisabled("  [ OK ] GLFW backend attached");
            ImGui::TextDisabled("  [ OK ] OpenGL3 renderer attached");
            ImGui::TextDisabled("  [ OK ] Dark theme applied");
            ImGui::Separator();

            // Phase 3
            ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.0f, 1.0f), "[PHASE 3] System Services");
            ImGui::TextDisabled("  [ OK ] Memory manager initialized (16384MB)");
            ImGui::TextDisabled("  [ OK ] Process scheduler started");
            ImGui::TextDisabled("  [ OK ] CPU core detection complete (16 cores)");
            ImGui::TextDisabled("  [ OK ] File system mounted");
            ImGui::TextDisabled("  [ OK ] Desktop environment constructed");
            ImGui::Separator();

            // Phase 4
            ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.0f, 1.0f), "[PHASE 4] Main Loop");
            ImGui::TextDisabled("  [ OK ] Input polling active");
            ImGui::TextDisabled("  [ OK ] UI render pipeline ready");
            ImGui::TextDisabled("  [ OK ] Taskbar loaded");
            ImGui::TextDisabled("  [ OK ] Task Manager service running");
            ImGui::TextDisabled("  [ OK ] 10 processes registered");
            ImGui::Separator();

            // Phase 5
            ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.0f, 1.0f), "[PHASE 5] Ready");
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "  [ OK ] CSOPESY OS boot complete.");
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "  > System is ready.");

            ImGui::End();
        }

        if (active_task_manager) {
            ImGui::Begin("Task Manager", &active_task_manager, ImGuiWindowFlags_NoCollapse);
            ImGui::Text("Processes running in CSOPESY OS Emulator:");

			if (ImGui::BeginTable("ProcessTable", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
				ImGui::TableSetupColumn("PID");
				ImGui::TableSetupColumn("Name");
				ImGui::TableSetupColumn("CPU");
				ImGui::TableSetupColumn("Memory");
				ImGui::TableHeadersRow();

				struct Process { int pid; const char* name; float cpu; float memory; };
				Process processes[] = {
					{ 1, "process_1", 0.5f, 1.2f },
					{ 2, "process_2", 1.0f, 2.5f },
					{ 3, "process_3", 0.8f, 1.8f },
					{ 4, "process_4", 0.6f, 1.5f },
					{ 5, "process_5", 0.3f, 1.0f },
                    { 6, "process_6", 0.9f, 2.0f },
					{ 7, "process_7", 0.4f, 1.3f },
					{ 8, "process_8", 0.7f, 1.7f },
					{ 9, "process_9", 0.2f, 0.8f },
					{ 10, "process_10", 0.1f, 0.5f }
				};

				for (const auto& process : processes) {
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::Text("%d", process.pid);
					ImGui::TableSetColumnIndex(1);
					ImGui::Text("%s", process.name);
					ImGui::TableSetColumnIndex(2);
					ImGui::Text("%.1f%%", process.cpu);
					ImGui::TableSetColumnIndex(3);
					ImGui::Text("%.1f MB", process.memory);
				}
				ImGui::EndTable();
			}
            ImGui::End();
        }

        // --- Component 3: Window closely resembling the Windows task manager ---
        float panel_h = 50.0f;
		ImGui::SetNextWindowPos(ImVec2(0, (float)display_h - panel_h));
		ImGui::SetNextWindowSize(ImVec2((float)display_w, panel_h));

		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

		ImGuiWindowFlags panel_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize;

		if (ImGui::Begin("Bottom Panel", nullptr, panel_flags)) {
            ImGui::AlignTextToFramePadding();
            ImGui::SameLine();

			if (ImGui::Button("S1"))
				active_screen_1 = !active_screen_1;
            ImGui::SameLine();

            if (ImGui::Button("S2"))
                active_screen_2 = !active_screen_2;
            ImGui::SameLine();

            if (ImGui::Button("Task Manager"))
				active_task_manager = !active_task_manager;

            // --- PWR: relocated into the taskbar so it isn't hidden; the only
            // sanctioned shutdown (the window's X / Alt+F4 is "force exit"). ---
            const float pwr_w = 60.0f;
            ImGui::SameLine(ImGui::GetWindowWidth() - pwr_w - 10.0f);
            ImGui::PushStyleColor(ImGuiCol_Button,        IM_COL32(150, 40, 40, 255));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(200, 60, 60, 255));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  IM_COL32(120, 25, 25, 255));
            if (ImGui::Button("PWR", ImVec2(pwr_w, 0.0f)))
                ImGui::OpenPopup("Shut Down");
            ImGui::PopStyleColor(3);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Shut down CSOPESY");

            // Confirmation modal so a stray click can't kill the session.
            ImVec2 pwr_center = ImGui::GetMainViewport()->GetCenter();
            ImGui::SetNextWindowPos(pwr_center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
            if (ImGui::BeginPopupModal("Shut Down", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::Text("Shut down the CSOPESY emulator?");
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();
                if (ImGui::Button("Shut Down", ImVec2(120.0f, 0.0f)))
                    glfwSetWindowShouldClose(window, true);
                ImGui::SameLine();
                if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f)))
                    ImGui::CloseCurrentPopup();
                ImGui::EndPopup();
            }
            ImGui::End();
		}
        ImGui::PopStyleVar(2);
        } // end else (boot complete)

        // Rendering
        ImGui::Render();
        glViewport(0, 0, display_w, display_h);
        glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    // === Phase 5: Shutdown - tear down in reverse init order ===============
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
