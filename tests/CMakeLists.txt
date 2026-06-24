enable_testing()

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
CPMAddPackage(
        NAME googletest
        GITHUB_REPOSITORY google/googletest
        GIT_TAG main
        OPTIONS "gtest_force_shared_crt ON"
)

CPMAddPackage(
        NAME nlohmann_json
        GITHUB_REPOSITORY nlohmann/json
        VERSION 3.12.0
        OPTIONS
        "JSON_BuildTests OFF" # Skip building up internal JSON framework unit tests
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
        view_coordinate_mapper_test.cpp
        flatten_deep_tree_test.cpp
        dynamic_fetch_test.cpp
        any_type_comparator_test.cpp
        join_proxy_model_test.cpp
        vector_table_model_test.cpp
        tree_table_model_test.cpp
        polymorphic_tree_table_model_test.cpp
        polymorphic_vector_table_model_test.cpp
)

target_link_libraries(unit_tests PRIVATE
        ftxmodel
        GTest::gtest_main
        nlohmann_json::nlohmann_json
)

# Automated CTest test case suite discovery macro pass
include(GoogleTest)
gtest_discover_tests(unit_tests)
