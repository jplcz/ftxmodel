enable_testing()

find_package(GTest CONFIG QUIET)

if(NOT GTest_FOUND AND NOT TARGET GTest::gtest_main)
    message(STATUS "GoogleTest not found in package registry. Fetching from GitHub...")

    include(FetchContent)
    FetchContent_Declare(
            googletest
            URL https://github.com/google/googletest/archive/refs/heads/main.zip
    )

    # For Windows environments: prevent overriding the parent compiler's MT/MD settings
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    # Create matching namespaced aliases so your link logic remains unified
    if(TARGET gtest_main AND NOT TARGET GTest::gtest_main)
        add_library(GTest::gtest_main ALIAS gtest_main)
    endif()
else()
    message(STATUS "Found GoogleTest in package registry: ${GTest_DIR}")
endif()

# =============================================================================
# Test Executable Target Definition
# =============================================================================
add_executable(unit_tests
        test_model.cpp
        identity_proxy_model_test.cpp
        string_list_model_test.cpp
        sort_filter_proxy_model_test.cpp
        test_flatten_tree_proxy_model.cpp
        standard_item_model_test.cpp
        unicode_scaler_test.cpp
        test_any_to_string.cpp
)

target_link_libraries(unit_tests PRIVATE
        ftxmodel
        GTest::gtest_main
)

# Automated CTest test case suite discovery macro pass
include(GoogleTest)
gtest_discover_tests(unit_tests)
