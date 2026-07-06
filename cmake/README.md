# CMake Helpers

Reusable CMake modules live here.

The root `CMakeLists.txt` should stay small and delegate project options,
warnings, sanitizers, dependency wiring, precompiled headers, and install/export
helpers to files in this directory.

`BuildInfo.cmake` owns generation of the core build-information source. It uses
CMake configure-time data and writes generated files into the active build tree.