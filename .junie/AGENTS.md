# FTXModel Project Overview for AI Agents & Developers

This document provides essential technical details for developing, building, and testing the `ftxmodel` library.

## 1. Build and Configuration

The project uses CMake (minimum version 3.21) and C++20. It relies on `CPM.cmake` for dependency management.

### Key CMake Options
- `FTXMODEL_ENABLE_TESTING` (Default: ON): Controls whether unit tests are built.
- `FTXMODEL_ENABLE_APPS` (Default: ON): Controls whether example applications are built.
- `FTXMODEL_ENABLE_INSTALL` (Default: ON): Enables the generation of the install target.

### Dependencies
Dependencies are automatically fetched via CPM:
- **FTXUI**: Main UI library.
- **PalSigslot**: Used for signals and slots (reactive data updates).
- **GoogleTest**: Required for unit tests.
- **nlohmann_json**: Required for JSON-related models.
- **SQLite3**: Required for SQLite-related models (must be available on the system).

### Building the Project
To build the library and unit tests:
```bash
cmake -B build -S . -DFTXMODEL_ENABLE_TESTING=ON
cmake --build build --target unit_tests
```

## 2. Testing Information

### Running Tests
The main test suite is the `unit_tests` executable. You can run all tests or use GoogleTest filters:
```bash
# Run all tests
./build/tests/unit_tests

# Run specific tests (e.g., only StringListModel tests)
./build/tests/unit_tests --gtest_filter=StringListModelTest.*
```

### Adding New Tests
1. Create a new `.cpp` file in the `tests/` directory (e.g., `tests/my_feature_test.cpp`).
2. Add the file to the `unit_tests` target in `tests/CMakeLists.txt`:
   ```cmake
   add_executable(unit_tests
           ...
           my_feature_test.cpp
   )
   ```
3. Use GoogleTest macros:
   ```cpp
   #include <gtest/gtest.h>
   #include <ftxmodel/some_header.hpp>

   TEST(MyFeatureTest, BehaviorIsCorrect) {
       // ... test logic ...
       EXPECT_TRUE(true);
   }
   ```

## 3. Additional Development Information

### Code Style and Architecture
- **C++20**: Use modern C++ features (concepts, `std::any`, etc.).
- **Model-View Pattern**: The library follows a pattern inspired by Qt's interview (Model/View) architecture, adapted for terminal UIs.
- **Signal/Slots**: Use `PalSigslot` for event communication between models and views.
- **Immutability and Indices**: `ModelIndex` is used to reference data in models. Opaque IDs (`UniqueNodeId`) are preferred for tracking items across layout changes.

### External Integrations
- When working with `SqliteQueryModel`, ensure SQLite3 is linked to the consumer target.
- When working with `JsonPropertyTreeModel`, ensure `nlohmann_json` is linked.
