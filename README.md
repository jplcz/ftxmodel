# ftxmodel

> **⚠️ Warning:** This library is currently a **Work In Progress**. Features are incomplete, and some public interfaces and internal architectures are subject to breaking changes without prior notice.

Abstract data model library for [FTXUI](https://github.com/ArthurSonzogni/FTXUI).

This library provides a generic data model abstraction to simplify the management, 
filtering, and presentation of tabular or structured data within FTXUI terminal applications. 
It aims to decouple data handling from the UI layer, making it easier to build 
robust and reactive terminal user interfaces.

## Documentation

*   **[Core Architecture: Models and Indices](docs/architecture.md)**: An overview of how `AbstractItemModel` and `ModelIndex` work together to power the Model-View system.

## Features

- **Generic Data Models**: Define flexible data structures using standard C++ types (e.g., `std::any`, `std::vector`, `std::string`).
- **Data Filtering**: Built-in mechanisms to filter rows based on generic criteria.
- **Type Conversion**: Utilities like `AnyToStringTranslator` to seamlessly convert arbitrary `std::any` data into viewable strings for the UI.
- **Thread Safety**: Most of the UI and model components are not designed for thread safety. Any updates must be performed on the thread which owns the object.  

## Installation with CPM.cmake

You can easily add `ftxmodel` to your CMake project using [CPM.cmake](https://github.com/cpm-cmake/CPM.cmake).

1.  **Add CPM.cmake to your project.**
    You can download `CPM.cmake` and include it in your project's `cmake` directory.

2.  **Add the ftxmodel package.**
    In your `CMakeLists.txt`, add the following:

    ```cmake
    CPMAddPackage(
      NAME ftxmodel
      GIT_REPOSITORY https://gitea.com/jplcz/ftxmodel.git
      GIT_TAG master  # Or a specific release tag/commit
    )
    ```

3.  **Link the library to your target.**
    After defining your executable or library, link `ftxmodel` to it:

    ```cmake
    target_link_libraries(your_target_name PRIVATE ftxmodel)
    ```

## External Library Integrations

The library provides optional components that integrate with external libraries. If you choose to use these components, you must ensure that the respective external libraries are linked in your project.

### SQLite (`SqliteQueryModel`)

To use `ftxmodel::SqliteQueryModel`, your application target must link against the SQLite3 library.

```cmake
# Example linking requirement
target_link_libraries(your_target_name PRIVATE sqlite3)
```

### JSON (`JsonPropertyTreeModel`)

To use `ftxmodel::JsonPropertyTreeModel`, your application target must link against the `nlohmann_json` library.

```cmake
# Example linking requirement
target_link_libraries(your_target_name PRIVATE nlohmann_json::nlohmann_json)
```
