# CSOPESY — Setup (build & run in VS Code)

This project is **C++ + CMake + vcpkg + Dear ImGui (GLFW + OpenGL3)**. It builds with the
**MSVC** toolchain that ships inside Visual Studio. You do **not** install CMake, Ninja, or
vcpkg separately — Visual Studio provides them.

Everyone does this **once per machine**. After that: `git pull`, then F7 / Shift+F5.

---

## 1. Install the toolchain
1. **Visual Studio 2026** — during install, check the **"Desktop development with C++"** workload.
   This gives you MSVC (the compiler), CMake, Ninja, **and** vcpkg. Any edition (Community is free)
   and any install location works.
2. **VS Code**, plus two extensions (VS Code will prompt you to install them when you open the
   folder — they're listed in `.vscode/extensions.json`):
   - **CMake Tools** (`ms-vscode.cmake-tools`)
   - **C/C++** (`ms-vscode.cpptools`)

## 2. Set `VCPKG_ROOT` (one-time)
The build finds vcpkg through the `VCPKG_ROOT` environment variable, so no machine-specific paths
live in the repo. Point it at the vcpkg bundled with **your** Visual Studio. Open a normal
terminal and run (adjust the path to match your VS edition/location):

```
setx VCPKG_ROOT "C:\Program Files\Microsoft Visual Studio\18\Community\VC\vcpkg"
```

- Professional/Enterprise → replace `Community` accordingly.
- Custom install drive → use that path.
- Prefer a standalone vcpkg? Clone `https://github.com/microsoft/vcpkg`, run
  `bootstrap-vcpkg.bat`, and point `VCPKG_ROOT` at that folder instead.

`setx` affects **new** terminals only — close and reopen your terminal afterward.

## 3. Open VS Code so it can find the compiler
`cmake` / `cl` / `ninja` are not on the system PATH — they live inside Visual Studio. The simplest
reliable way to expose them is to launch VS Code from the VS developer shell:

1. Start menu → **"Developer Command Prompt for VS 2026"**
2. `cd` to the cloned repo, then:
   ```
   code .
   ```

> **Convenience alternative:** install standalone CMake from cmake.org (tick "Add to PATH"), then
> you can open VS Code normally — the `cmake.useVsDeveloperEnvironment: always` setting in this repo
> supplies `cl`/`ninja` automatically.

## 4. Build & run
1. CMake Tools detects `CMakePresets.json` → pick the **default** preset if prompted (the first
   configure runs vcpkg and compiles GLFW — slow once, then cached).
2. In the bottom status bar, set the **launch target** to **`csopesy`**.
3. **Build:** `F7` · **Run:** `Shift+F5` · **Debug:** `F5`.

---

## ⚠️ Do NOT use the green ▶ "Run C/C++ File" button
That button (top-right of the editor) tries to compile a **single file** with `g++` and **cannot**
build this project — it's what causes the *"This app can't run on your PC"* error. Always build/run
through **CMake Tools** (the controls in the bottom status bar, or F7 / Shift+F5).

## Troubleshooting
- **`cmake`/`cl` not found** → you didn't open VS Code from the Developer Command Prompt (step 3),
  or VS lacks the C++ workload (step 1).
- **`$env{VCPKG_ROOT}` / toolchain errors** → `VCPKG_ROOT` isn't set or the terminal predates the
  `setx` (step 2). Reopen the terminal.
- **Stale config after pulling these changes** → Command Palette → `CMake: Delete Cache and Reconfigure`.
