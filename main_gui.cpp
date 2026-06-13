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

#include <GLFW/glfw3.h>
#include <cstdio>

static void glfw_error_callback(int error, const char* description) {
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

int main() {
    // === Phase 1: Bootstrapping - window system + GL context ===============
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return 1;

    // OpenGL 3.0+ / GLSL 130. Good baseline for the ImGui OpenGL3 backend.
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    GLFWwindow* window = glfwCreateWindow(1280, 720, "CSOPESY Desktop OS Emulator", nullptr, nullptr);
    if (window == nullptr)
        return 1;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable VSync

    // === Phase 2: Kernel Init - create ImGui context, install backends =====
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // === Phase 3: System Services - construct the Desktop (Component 1) ====
    Desktop desktop;
    ImVec4 clear_color = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);

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

        // --- Component 1: the Desktop is the base layer, drawn first. ------
        desktop.draw();
        if (desktop.shutdownRequested())
            glfwSetWindowShouldClose(window, true);

		// --- Component 2: buttons for unique UI screens and Task Manager ---
		if (active_screen_1) {
			ImGui::Begin("Screen 1");
			ImGui::Text("This is Screen 1");
			if (ImGui::Button("Close Screen 1"))
                active_screen_1 = false;
			ImGui::End();
		}

        if (active_screen_2) {
			ImGui::Begin("Screen 2");
			ImGui::Text("This is Screen 2");
			if (ImGui::Button("Close Screen 2"))
                active_screen_2 = false;
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
        int display_w, display_h;
		glfwGetFramebufferSize(window, &display_w, &display_h);

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
            ImGui::SameLine();
            ImGui::End();
		}
        ImGui::PopStyleVar(2);

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
