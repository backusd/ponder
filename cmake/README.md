# CMake Helpers

Reusable CMake modules live here.

The root `CMakeLists.txt` should stay small and delegate project options,
warnings, sanitizers, dependency wiring, precompiled headers, and install/export
helpers to files in this directory.

`BuildInfo.cmake` owns generation of the core build-information source. It uses
CMake configure-time data and writes generated files into the active build tree.
`RenderShaders.cmake` owns build-time render shader generation. It intentionally does not discover
DXC, SPIR-V validator, or reflection tools at configure time; those tools are resolved only when an
explicit render shader target is built or checked. The generator prefers `%VULKAN_SDK%/Bin` and
installed `C:/VulkanSDK/*/Bin` directories before falling back to `PATH`.

`UiBuildVerification.cmake` records the configured target graph after all subdirectories have been
added. Its CTest check always inspects that graph, the CMake cache, and package configuration. On
Ninja and Makefile generators it also requires compile commands and verifies PCH-free header
consumers; generators without compile-database support explicitly skip only those
compile-command checks. The exhaustive clean Windows Debug/Release matrix under `tests/build/ui` is
registered only for MSVC with a Ninja-family generator, drives the render-disabled, generic-render,
and full UI/render topologies, and always requires the compile database for all three modes.
