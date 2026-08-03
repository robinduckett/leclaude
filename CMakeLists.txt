cmake_minimum_required(VERSION 3.26)

project(Leclaude LANGUAGES CXX)

if(NOT MSVC)
  message(FATAL_ERROR "Leclaude is a Windows shell extension. It compiles with MSVC only.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# The DLL loads into Explorer and into other programs. A static C runtime removes the redistributable dependency.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_library(LeclaudeShell SHARED
  src/dllmain.cpp
  src/LeclaudeShell.def
)

target_compile_definitions(LeclaudeShell PRIVATE UNICODE _UNICODE)
target_compile_options(LeclaudeShell PRIVATE /W4 /permissive-)
