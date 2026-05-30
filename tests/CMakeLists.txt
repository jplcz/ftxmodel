enable_testing()

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
CPMAddPackage(
        NAME googletest
        GITHUB_REPOSITORY google/googletest
        GIT_TAG main
        OPTIONS "gtest_force_shared_crt ON"
)

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
        concat_proxy_model_test.cpp
)

target_link_libraries(unit_tests PRIVATE
        ftxmodel
        GTest::gtest_main
)

# Automated CTest test case suite discovery macro pass
include(GoogleTest)
gtest_discover_tests(unit_tests)
