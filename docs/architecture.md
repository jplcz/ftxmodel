# Core Architecture: Models and Indices

The `ftxmodel` library is built around a Model-View architecture, designed to cleanly separate how data is stored from how it is presented on the screen. The foundational pieces of this system are the `AbstractItemModel` and the `ModelIndex`.

## The Transient Navigator: `ModelIndex`

At the heart of the interaction between views and models is the `ModelIndex`. You can think of a `ModelIndex` as a lightweight cursor or coordinate handle. It points to a specific "cell" of data within a model.

### Key Characteristics of a ModelIndex:

*   **Transient by Nature:** A `ModelIndex` is meant to be created, used, and discarded quickly. It is **not** designed to be stored long-term. If a model updates, sorts, or filters its underlying data, any previously stored `ModelIndex` handles may become invalid or point to the wrong data.
*   **Stateless Coordinates:** It simply holds the `row` and `column` location within its parent context.
*   **The Internal Pointer:** For complex models (like hierarchical trees), a `ModelIndex` can carry an opaque `internalPointer()`. This allows the model to map the index directly to an internal data structure (like a specific node in a tree) without having to traverse from the root every time.
*   **Link to Authority:** Every valid index knows which model created it. When you call methods like `index.data()`, the index simply turns around and asks its owning model for that information.

### The Role of `UniqueNodeId`

Because `ModelIndex` is transient, views need a reliable way to remember what the user was looking at or interacting with. For example, if a user has a specific row selected and the background data refreshes, the selection shouldn't arbitrarily jump to a different item that now occupies that row number.

This is where `UniqueNodeId` comes in. While `ModelIndex` is a spatial coordinate (e.g., "Row 5, Column 0"), a `UniqueNodeId` is a semantic identifier (e.g., "Database Record ID 42" or "File path /tmp/foo"). 

Models provide a stable `UniqueNodeId` for every index. When views need to remember a selection across a model reset, they save the `UniqueNodeId`. After the reset, they ask the model to translate that ID back into a new, valid `ModelIndex`.

## The Data Authority: `AbstractItemModel`

The `AbstractItemModel` is the central hub for data access. It doesn't dictate *how* data is stored—it could be a `std::vector`, a database connection, or a parsed JSON document. Instead, it defines a standard contract for how views can query that data.

### The Dimensional Contract

Models describe their shape through a few key methods:

*   **`rowCount(parent)` and `columnCount(parent)`:** These define the grid size. In a flat list, `columnCount` is usually 1. In a table, it's greater than 1. In a tree, these counts are relative to the specified `parent` index.
*   **`index(row, column, parent)`:** This is the factory for creating `ModelIndex` handles. It translates a raw spatial request into an actual coordinate token.
*   **`parent(child)`:** This allows views to walk "up" the hierarchy, which is crucial for rendering trees or determining the full path of an item.

### The Content Contract

When a view needs to draw a cell, it doesn't just ask for "the data". It asks for data tailored to a specific `ItemRole`.

*   **`data(index, role)`:** This is the primary pipeline for information. A view might ask for the `DisplayRole` to get the text to render, or the `EditRole` to get the raw value for an input field.
*   **Polymorphism with `std::any`:** To support any type of data without restrictive templates at the view layer, models wrap all data returns in a `std::any`. It's up to the view (or a delegate) to unwrap it, often using utilities like `AnyToStringTranslator`.

### The Notification System

A model isn't just a static database; it's a live entity. Views need to know when things change so they can redraw. `AbstractItemModel` uses a robust signal system to broadcast these changes:

*   **Granular Updates:** `dataChanged` signals tell views that specific cells have new content, allowing for targeted redraws.
*   **Structural Changes:** When rows or columns are added or removed, models must emit `beginInsertRows`/`endInsertRows` (and their counterparts). This two-step process allows views to freeze their state, update their internal metrics, and then adapt to the new layout without crashing.
*   **Massive Overhauls:** If the entire dataset changes drastically (like a completely new SQL query), `beginResetModel`/`endResetModel` tells views to discard all caches and rebuild from scratch.
