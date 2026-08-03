cmake_minimum_required(VERSION 3.26)

project(Leclaude LANGUAGES CXX RC)

if(NOT MSVC)
  message(FATAL_ERROR "Leclaude is a Windows shell extension. It compiles with MSVC only.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# The DLL loads into Explorer and into other programs. A static C runtime removes the redistributable dependency.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# The resource compiler does not report the icon as a dependency. This property
# makes sure that a new icon causes a new resource build.
set_source_files_properties(src/resources.rc PROPERTIES
  OBJECT_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/assets/leclaude.ico
)

add_library(LeclaudeShell SHARED
  src/dllmain.cpp
  src/class_factory.cpp
  src/overlay_handler.cpp
  src/project_cache.cpp
  src/project_scan.cpp
  src/registration.cpp
  src/resources.rc
  src/LeclaudeShell.def
)
target_compile_definitions(LeclaudeShell PRIVATE UNICODE _UNICODE)
target_compile_options(LeclaudeShell PRIVATE /W4 /permissive-)
target_link_libraries(LeclaudeShell PRIVATE shell32 ole32 advapi32)

enable_testing()

add_executable(leclaude_tests
  tests/test_main.cpp
  src/project_scan.cpp
)
target_include_directories(leclaude_tests PRIVATE src)
target_compile_definitions(leclaude_tests PRIVATE UNICODE _UNICODE)
target_compile_options(leclaude_tests PRIVATE /W4 /permissive-)
target_link_libraries(leclaude_tests PRIVATE shell32 ole32)

add_test(NAME scan_tests COMMAND leclaude_tests)

add_executable(leclaude_smoke
  tests/smoke_com.cpp
)
target_compile_definitions(leclaude_smoke PRIVATE UNICODE _UNICODE)
target_compile_options(leclaude_smoke PRIVATE /W4 /permissive-)
target_link_libraries(leclaude_smoke PRIVATE shell32 ole32)
add_dependencies(leclaude_smoke LeclaudeShell)

add_test(NAME com_smoke COMMAND leclaude_smoke)
