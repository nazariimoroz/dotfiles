cmake_minimum_required(VERSION 3.16)
project(easyhub LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Prefer a system FTXUI (e.g. the `ftxui-git` package); otherwise fetch it.
find_package(ftxui QUIET)
if(NOT ftxui_FOUND)
  message(STATUS "system ftxui not found — fetching via FetchContent")
  include(FetchContent)
  FetchContent_Declare(
    ftxui
    GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
    GIT_TAG v5.0.0
  )
  FetchContent_MakeAvailable(ftxui)
endif()

add_executable(easyhub src/main.cpp src/tray.cpp)
target_link_libraries(easyhub
  PRIVATE ftxui::screen ftxui::dom ftxui::component)

find_package(Threads REQUIRED)
target_link_libraries(easyhub PRIVATE Threads::Threads)

# GIO/GDBus for the system-tray (StatusNotifier) support.
find_package(PkgConfig REQUIRED)
pkg_check_modules(GIO REQUIRED IMPORTED_TARGET gio-2.0 gio-unix-2.0)
target_link_libraries(easyhub PRIVATE PkgConfig::GIO)
