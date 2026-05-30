enable_testing()

include(FetchContent)
FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/heads/main.zip
)
# For Windows environments: prevent overriding the parent compiler's MT/MD settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(unit_tests test_model.cpp identity_proxy_model_test.cpp string_list_model_test.cpp
        sort_filter_proxy_model_test.cpp test_flatten_tree_proxy_model.cpp
        standard_item_model_test.cpp
        unicode_scaler_test.cpp)

target_link_libraries(unit_tests PRIVATE
        ftxmodel
        gtest_main
        gmock
)

include(GoogleTest)
gtest_discover_tests(unit_tests)

function(AddExample Source)
    get_filename_component(base_source "${Source}" NAME_WE)
    add_executable(${base_source} ${Source})
    target_link_libraries(${base_source} PRIVATE ftxmodel)
    target_compile_features(${base_source} PRIVATE cxx_std_20)
    target_compile_options(${base_source} PRIVATE -Wall -Werror -Wextra -Wconversion -Wsign-conversion)
endfunction()

AddExample(list_view_app.cpp)
AddExample(table_view_app.cpp)
AddExample(tree_view_app.cpp)
AddExample(task_view_tree_app.cpp)
AddExample(fs_model_app.cpp)
AddExample(multi_router_delegate_app.cpp)
