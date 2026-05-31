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

include(CMakeFindDependencyMacro)
include(cmake/CPM.cmake)

find_package(PalSigslot CONFIG QUIET)
set(SIGSLOT_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
CPMAddPackage(
        NAME PalSigslot
        GITHUB_REPOSITORY palacaze/sigslot
        VERSION 1.2.3
        OPTIONS "SIGSLOT_ENABLE_INSTALL OFF"
)

set(FTXUI_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(FTXUI_BUILD_TESTS OFF CACHE BOOL "" FORCE)
CPMAddPackage(
        NAME ftxui
        GITHUB_REPOSITORY ArthurSonzogni/FTXUI
        GIT_TAG main
        OPTIONS
        "FTXUI_BUILD_EXAMPLES OFF"
        "FTXUI_BUILD_TESTS OFF"
)

add_library(ftxmodel INTERFACE
        include/ftxmodel/scrollable_table_view.hpp)
add_library(ftxmodel::ftxmodel ALIAS ftxmodel)
target_compile_features(ftxmodel INTERFACE cxx_std_20)

target_include_directories(ftxmodel INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(ftxmodel INTERFACE
        $<BUILD_INTERFACE:Pal::Sigslot>
        ftxui::screen
        ftxui::dom
        ftxui::component
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