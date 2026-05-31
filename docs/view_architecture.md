# Core Architecture: Views, Selection, and Delegates

The `ftxmodel` library uses a decoupled Model-View-Delegate architecture. While `AbstractItemModel` handles *what* the data is, the View components handle *how* it is displayed, selected, and styled on the screen.

This document outlines the core components of the presentation layer.

## The Presentation Pipeline

The journey of data from a model to the terminal screen involves several specialized classes working in concert:

1.  **View (`AbstractItemView`)**: The orchestrator. It manages the layout, the scrolling, and listens to the model for updates.
2.  **Selection Model (`ItemSelectionModel`)**: The tracker. It remembers which item the user has currently focused or selected.
3.  **Item Delegate (`ItemDelegate`)**: The painter. It translates a single piece of abstract data into an actual visual FTXUI `Element`.
4.  **Header Delegate (`HeaderDelegate`)**: The border painter. It renders the row and column headers.
5.  **Highlight Style (`SelectionHighlightStyle`)**: The decorator. It applies visual emphasis (like background colors) to selected or focused items.

---

## 1. Views: The Orchestrators

### `AbstractItemView`
This is the foundational class for all visual representations of data. It inherits from `ftxui::ComponentBase` and acts as the bridge between the UI loop and the data source.

**Key Responsibilities:**
*   **Model Binding:** It holds a reference to the `AbstractItemModel`.
*   **Event Subscriptions:** It connects to the model's signals (`dataChanged`, `beginInsertRows`, `beginResetModel`, etc.). When the model changes, the view reacts by updating its internal state and requesting a screen repaint (`update()`).
*   **Selection Coordination:** It automatically provisions and manages an `ItemSelectionModel` bound to the active data model.
*   **Lifecycle Management:** It safely handles edge cases, such as preventing crashes when the currently selected row is deleted from the model.

### `AbstractGridLikeItemView`
An extension of the base view specifically tailored for tabular or 2D grid layouts (like Tables and Lists).

**Key Responsibilities:**
*   **Axis Separation:** It introduces independent delegates and visibility toggles for Horizontal (Columns) and Vertical (Rows) headers.
*   **Styling Engine:** It integrates the `SelectionHighlightStyle` to manage how cursor focus and row selections are visually distinguished from normal data cells.

---

## 2. Tracking Focus: `ItemSelectionModel`

Because UI views in a terminal are often redrawn completely on every frame, and because underlying data can sort or filter dynamically, tracking user selection purely by a row/column index is fragile. 

The `ItemSelectionModel` solves this by tracking the *data identity*, not just the *screen coordinate*.

**Key Responsibilities:**
*   **Persistent Tracking:** When you select an item, the selection model saves the item's `UniqueNodeId`.
*   **Data-First Resolution:** If the view needs to know the current index (e.g., to draw a highlight), the selection model asks the main model to reverse-lookup the `ModelIndex` currently associated with the saved `UniqueNodeId`. If the item moved due to a sort, the selection follows it.
*   **Notification:** It emits a `currentIndexChanged` signal whenever focus shifts, allowing the UI to react to user navigation.

---

## 3. Drawing Cells: `ItemDelegate`

Views know *where* to put cells, but they don't know *how* to draw them. That is the job of the `ItemDelegate`. This separation means you can completely change the look of a table (e.g., rendering a boolean as text vs. a progress bar) without changing the view or the model code.

**Key Responsibilities:**
*   **Widget Generation (`createWidget`)**: Takes a `ModelIndex` and the model, extracts the raw `std::any` data, and builds a styled `ftxui::Element` (like text, a checkbox, or a gauge).
*   **Layout Hints (`sizeHint`)**: Helps the view's layout engine by declaring the preferred dimensions for a specific cell's content.

**Built-in Implementations:**
*   `StyledTextDelegate`: Handles standard text with advanced formatting, alignment, wrapping, and clipping options.
*   `CheckBoxDelegate`: Renders boolean or integer values as interactive `[X]` / `[ ]` brackets.
*   `ProgressBarDelegate`: Converts numeric values into visual FTXUI gauge elements.

---

## 4. Drawing Borders: `HeaderDelegate`

Similar to `ItemDelegate`, but specifically dedicated to rendering the structural labels at the edges of grid views.

**Key Responsibilities:**
*   **Header Generation (`createHeaderWidget`)**: Takes a section index and orientation (Horizontal/Vertical) and generates the label element.

**Built-in Implementations:**
*   `AdvancedHeaderDelegate`: Provides distinct styling pipelines for column labels (centered, custom colors) versus row numbers (right-aligned, padded with grid separator glyphs).

---

## 5. Visual Feedback: `SelectionHighlightStyle`

When an item is selected or focused, it needs to visually pop out from the rest of the grid. Instead of hardcoding colors into the view, `AbstractGridLikeItemView` delegates this responsibility to `SelectionHighlightStyle`.

**Key Responsibilities:**
*   **Behavior Rules:** Defines if highlighting should apply to the entire horizontal row (`SelectRows`) or just the precise focused coordinate (`SelectCells`).
*   **Stateful Decorators:** Applies different `ftxui::Decorator` combinations based on the component's state (e.g., active focus vs. blurred background focus).
*   **Highlight Application (`applyHighlight`)**: Wraps a pre-rendered cell element from the `ItemDelegate` with the appropriate background/foreground colors before the final render step.
