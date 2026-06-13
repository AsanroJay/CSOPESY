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

        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
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
