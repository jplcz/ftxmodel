cmake_minimum_required(VERSION 3.21)
project(
        ftxmodel
        LANGUAGES CXX
        VERSION 1.0.0
        DESCRIPTION "Abstract C++ model for FTXUI")

option(FTXMODEL_ENABLE_INSTALL "Generate install target" ON)
option(FTXMODEL_ENABLE_TESTING "Enable building of unit tests" ON)
option(FTXMODEL_ENABLE_APPS "Enable example apps" ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)
include(CMakeFindDependencyMacro)

find_package(PalSigslot CONFIG QUIET)

if(NOT PalSigslot_FOUND AND NOT TARGET Pal::Sigslot)
    message(STATUS "sigslot not found in package registry. Fetching from GitHub...")

    set(SIGSLOT_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
            sigslot
            GIT_REPOSITORY https://github.com/palacaze/sigslot
            GIT_TAG v1.2.3)
    FetchContent_MakeAvailable(sigslot)

    # Map alias if the downloaded target name doesn't match the standard package namespace
    if(TARGET sigslot AND NOT TARGET Pal::Sigslot)
        add_library(Pal::Sigslot ALIAS sigslot)
    endif()

    set(FTXMODEL_USING_INTERNAL_SIGSLOT ON)
else()
    message(STATUS "Found sigslot in package registry: ${PalSigslot_DIR}")
endif()

find_package(ftxui CONFIG QUIET)

if(NOT ftxui_FOUND AND NOT TARGET ftxui::screen)
    message(STATUS "ftxui not found in package registry. Fetching from GitHub...")

    set(FTXUI_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(FTXUI_BUILD_TESTS OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(
            ftxui
            GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
            GIT_TAG main)
    FetchContent_MakeAvailable(ftxui)
else()
    message(STATUS "Found ftxui in package registry: ${ftxui_DIR}")
endif()

set(FTXUI_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(FTXUI_BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(ftxui)

add_library(ftxmodel INTERFACE)
add_library(ftxmodel::ftxmodel ALIAS ftxmodel)
target_compile_features(ftxmodel INTERFACE cxx_std_20)

target_include_directories(ftxmodel INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${sigslot_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(ftxmodel INTERFACE
        $<BUILD_INTERFACE:Pal::Sigslot>
        $<BUILD_INTERFACE:ftxui::screen>
        $<BUILD_INTERFACE:ftxui::dom>
        $<BUILD_INTERFACE:ftxui::component>
)

# -----------------------------------------------------------------------------
# Installation & Package Distribution Configuration
# -----------------------------------------------------------------------------
if(FTXMODEL_ENABLE_INSTALL)
    include(GNUInstallDirs)
    include(CMakePackageConfigHelpers)

    install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
    install(TARGETS ftxmodel EXPORT ftxmodelTargets INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

    if(FTXMODEL_USING_INTERNAL_SIGSLOT)
        install(DIRECTORY "${sigslot_SOURCE_DIR}/include/" DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
    endif()

    install(EXPORT ftxmodelTargets
            FILE ftxmodelTargets.cmake
            NAMESPACE ftxmodel::
            DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ftxmodel
    )

    write_basic_package_version_file(
            "${CMAKE_CURRENT_BINARY_DIR}/ftxmodelConfigVersion.cmake"
            VERSION ${PROJECT_VERSION}
            COMPATIBILITY SameMajorVersion
    )

    # Downstream consumers must run the same check when they use find_package(ftxmodel)
    set(CONFIG_CONTENTS [=[
@PACKAGE_INIT@
include(CMakeFindDependencyMacro)

# Downstream: Only try to find sigslot if we didn't bundle it directly into our headers
set(INTERNAL_SIGSLOT_BUNDLED @FTXMODEL_USING_INTERNAL_SIGSLOT@)
if(NOT INTERNAL_SIGSLOT_BUNDLED)
    find_dependency(PalSigslot QUIET)
    if(NOT PalSigslot_FOUND AND NOT TARGET Pal::Sigslot AND TARGET sigslot)
        add_library(Pal::Sigslot ALIAS sigslot)
    endif()
endif()

# Downstream: FTXUI must always be resolved on the host machine
find_dependency(ftxui QUIET)

include("${CMAKE_CURRENT_LIST_DIR}/ftxmodelTargets.cmake")
]=])

    string(CONFIGURE "${CONFIG_CONTENTS}" CONFIG_OUTPUT @ONLY)
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/ftxmodelConfig.cmake" "${CONFIG_OUTPUT}")

    install(FILES
            "${CMAKE_CURRENT_BINARY_DIR}/ftxmodelConfig.cmake"
            "${CMAKE_CURRENT_BINARY_DIR}/ftxmodelConfigVersion.cmake"
            DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ftxmodel
    )
endif()

# -----------------------------------------------------------------------------
# Test Harness Subdirectory
# -----------------------------------------------------------------------------
if (FTXMODEL_ENABLE_TESTING)
    add_subdirectory(tests)
endif ()

if (FTXMODEL_ENABLE_APPS)
    add_subdirectory(examples)
endif ()