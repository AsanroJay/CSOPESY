cmake_minimum_required(VERSION 3.21)
project(CSOPESY LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Dependencies -----------------------------------------------------------
# glfw3 is pulled in by vcpkg (see vcpkg.json). OpenGL ships with the OS.
find_package(glfw3 CONFIG REQUIRED)
find_package(OpenGL REQUIRED)

# --- Dear ImGui (vendored source under third_party/imgui) --------------------
# We build ImGui's core + the GLFW platform backend + the OpenGL3 renderer
# backend into a small static library, then link our app against it.
set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui)

add_library(imgui STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_demo.cpp                      # remove later if unused
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

target_include_directories(imgui PUBLIC
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)

# The OpenGL3 backend has its own GL loader, so no GLAD/GLEW is needed.
target_link_libraries(imgui PUBLIC glfw OpenGL::GL)

# --- Application ------------------------------------------------------------
# NOTE: main.cpp (the original console version) is intentionally NOT compiled
# here yet. The GUI milestone builds from main_gui.cpp. Merge your command
# logic in later, then swap/add the source below.
add_executable(csopesy
    main_gui.cpp
    Desktop.cpp
    BootScreen.cpp
)
target_link_libraries(csopesy PRIVATE imgui)
