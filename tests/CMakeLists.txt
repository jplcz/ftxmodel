enable_testing()

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/heads/main.zip
)
# For Windows environments: prevent overriding the parent compiler's MT/MD settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(unit_tests test_model.cpp)

target_link_libraries(unit_tests PRIVATE 
    ftxmodel
    gtest_main
    gmock
)

include(GoogleTest)
gtest_discover_tests(unit_tests)

add_executable(list_view_app list_view_app.cpp)
target_link_libraries(list_view_app PRIVATE ftxmodel)
target_compile_features(list_view_app PRIVATE cxx_std_20)

add_executable(table_view_app table_view_app.cpp)
target_link_libraries(table_view_app PRIVATE ftxmodel)
target_compile_features(table_view_app PRIVATE cxx_std_20)

