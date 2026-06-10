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
AddExample(format_text_app.cpp)
AddExample(flatten_model.cpp)
AddExample(contact_proxy_model_app.cpp)
AddExample(async_string_list.cpp)
AddExample(fetch_more_list.cpp)
AddExample(shortcut_bar_app.cpp)

find_package(SQLite3)

if(SQLite3_FOUND)
    message(STATUS "SQLite3 found successfully for examples")

    AddExample(sqlite_example_app.cpp)
    target_link_libraries(sqlite_example_app PRIVATE SQLite::SQLite3)

    AddExample(scrollable_table_view_app.cpp)
    target_link_libraries(scrollable_table_view_app PRIVATE SQLite::SQLite3)
else()
    message(WARNING "SQLite3 was requested for examples but could not be located on the system.")
endif()

CPMAddPackage(
        NAME nlohmann_json
        GITHUB_REPOSITORY nlohmann/json
        VERSION 3.12.0
        OPTIONS
        "JSON_BuildTests OFF" # Skip building up internal JSON framework unit tests
)

AddExample(json_property_tree.cpp)
target_link_libraries(json_property_tree PRIVATE nlohmann_json::nlohmann_json)

AddExample(scrollable_json_property_tree.cpp)
target_link_libraries(scrollable_json_property_tree PRIVATE nlohmann_json::nlohmann_json)
