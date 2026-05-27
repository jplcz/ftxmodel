option(FTXMODEL_ENABLE_INSTALL "Generate install target" ON)

cmake_minimum_required(VERSION 3.21)
project(ftxmodel LANGUAGES CXX VERSION 1.0.0 DESCRIPTION "Abstract C++ model for FTXUI")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

# Automatically download the sigslot library from GitHub
FetchContent_Declare(
    sigslot
    GIT_REPOSITORY https://github.com/palacaze/sigslot
    GIT_TAG v1.2.3
)
FetchContent_MakeAvailable(sigslot)

include(FetchContent)

FetchContent_Declare(
    ftxui
    GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
    GIT_TAG main
)

set(FTXUI_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(FTXUI_BUILD_TESTS OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(ftxui)

add_library(ftxmodel INTERFACE)
target_include_directories(ftxmodel INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_link_libraries(ftxmodel INTERFACE Pal::Sigslot)
target_compile_features(ftxmodel INTERFACE cxx_std_20)

target_link_libraries(ftxmodel INTERFACE
    ftxui::screen
    ftxui::dom
    ftxui::component
)

if(FTXMODEL_ENABLE_INSTALL)
    include(GNUInstallDirs)

endif()

add_subdirectory(tests)
