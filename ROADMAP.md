# C++ Rich Library Pragmatic Roadmap

This document structures the roadmap for **Rich C++** by **complexity vs. value tiers**, enabling modular development without forcing heavy third-party dependencies unless explicitly desired.

```mermaid
graph TD
    Tier1["Tier 1: Core Essentials & Zero-Dep Polish (High Value, Zero Deps)"] --> Tier2["Tier 2: Advanced Visual Layout (High Value, Low Complexity)"]
    Tier2 --> Tier3["Tier 3: Dynamic & Live Updates (Interactive, Threading)"]
    Tier3 -.-> Tier4["Tier 4: Heavy Ecosystem Modules (Optional / Heavy Deps)"]
```

---

## Current Status Overview

| Tier | Focus | Dependencies | Status |
| :--- | :--- | :--- | :---: |
| **Tier 0** | Architectural Kernel & Basic Widgets | 0 (Pure C++20) | ✅ **Completed & Validated** |
| **Tier 1** | Core Polish & Text Foundations | 0 (Pure C++20) | 🟡 **Next Up** |
| **Tier 2** | Advanced Visual Layout (Tree, Align, Split Grid) | 0 (Pure C++20) | 📋 **Planned** |
| **Tier 3** | Dynamic & Live Displays (Progress Bars, Spinners) | 0 (`std::jthread` / VT escapes) | 📋 **Planned** |
| **Tier 4** | Heavy Ecosystem Modules (Markdown, Highlighting, Stacktrace) | Optional 3rd-party (`md4c`, `cpptrace`) | ⚠️ **Optional / Deferrable** |

---

## Tier 0: Architectural Kernel & Proof-of-Concept *(Completed)*
*Goal: Prove the C++20 concept, type erasure, and exact layout match against Python Rich.*

- [x] **Renderable Concept & AnyRenderable**: C++20 ADL customization points (`render_item`, `measure_item`) with value-semantic type erasure.
- [x] **Core Primitives**: `Segment`, `Style` (16 ANSI colors, bright variants, bold, dim, italic, underline, reverse, RGB), `Color`.
- [x] **Console**: `Console::print`, `Console::rule`, `Console::export_text`, record mode buffer, automatic Windows `CP_UTF8` and VT processing initialization.
- [x] **Containers & Decorators**:
  - `Panel` with centered title, custom borders, multiline support.
  - `Table` with multi-column grid, Unicode box drawing, column justification.
  - `Columns` with multi-row wrapping and independent left-aligned lines.
  - `Padding` with pipe operator (`|`) composition.
- [x] **Automated Oracle Test Suite**: `tests/verify_oracle.py` running byte-for-byte binary comparison against Python Rich for identical layouts (5/5 tests passing).

---

## Tier 1: Core Essentials & Zero-Dep Polish *(High Value, Zero Dependencies)*
*Goal: Make the library completely robust for standard CLI tool output with zero external dependencies.*

### 1.1 Terminal Dimensions & Stream Detection
- [ ] **Auto Terminal Width**:
  - Windows: Query `GetConsoleScreenBufferInfo` for active window column count.
  - POSIX: Query `ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws)`.
  - Fallback: Default to 80 columns when redirected to a pipe or file (`isatty`).
- [ ] **NO_COLOR & Color Detection**:
  - Respect `NO_COLOR` environment variable (https://no-color.org).
  - Inspect `COLORTERM` (for truecolor / 24-bit support) and `TERM`.
### 1.2 Text Wrapping & Justification (`rich::Text`)
- [ ] **Word Wrapping Engine**:
  - Soft-wrap text streams at whitespace boundaries to fit within specified console widths.
  - Support `Overflow::Fold`, `Overflow::Crop`, and `Overflow::Ellipsis` (`...`).
- [ ] **Text Alignment**:
  - Add text justification modes: `Justify::Left`, `Justify::Center`, `Justify::Right`, `Justify::Full`.

### 1.3 Box Style Library (`rich::box`)
- [ ] **Box Presets**:
  - `box::ROUNDED` (current default: `╭─╮│╰─╯`)
  - `box::SQUARE` (`┌─┐│└─┘`)
  - `box::HEAVY` (`┏━┓┃┗━┛`)
  - `box::DOUBLE` (`╔═╗║╚═╝`)
  - `box::ASCII` (`+-+|+-+`)
  - `box::MINIMAL` (no vertical outer borders)
- [ ] **Configurable Borders**: Allow passing `{.box = box::DOUBLE}` to `Panel` and `Table`.

### 1.4 Oracle Parity Validation
- [ ] Add Oracle test cases for:
  - `box_styles`: Comparing Table and Panel with each box preset against Python Rich.
  - `text_wrap`: Comparing soft-wrapped paragraphs against Python Rich text wrapping.

---

## Tier 2: Advanced Visual Layout *(High Value, Low Complexity, Zero Dependencies)*
*Goal: Deliver full visual dashboard and structural composition capabilities.*

### 2.1 `Tree` Widget (`rich::tree::Tree`)
- [ ] Hierarchical data visualization with guide characters (`├── `, `└── `, `│   `).
- [ ] Arbitrary renderables as node labels (allowing nested Panels or Tables inside Tree branches).
- [ ] Expandable and collapsible representation.

### 2.2 `Align` & `Constrain` Decorators
- [ ] `Align`: Wrapper to horizontally center, left-align, or right-align any renderable within target width.
  - Syntax: `panel(...) | align(Justify::Center)`
- [ ] `Constrain`: Clamp maximum width of any nested renderable.
  - Syntax: `table | constrain(60)`

### 2.3 `Layout` Engine (Flexible Grid Splits)
- [ ] Split screen into sub-regions:
  - `layout.split_row(...)` (horizontal division).
  - `layout.split_column(...)` (vertical division).
  - Proportional sizing via `ratio` (e.g. 1:2 split).
  - Fixed-size constraints (`size = 10`).

### 2.4 Oracle Parity Validation
- [ ] Add Oracle test cases for `tree_basic` and `layout_split`.
This document structures the roadmap for **Rich C++** by **complexity vs. value tiers**, enabling modular development without forcing heavy third-party dependencies unless explicitly desired.

---
```mermaid
graph TD
    Tier1["Tier 1: Core Essentials & Zero-Dep Polish (High Value, Zero Deps)"] --> Tier2["Tier 2: Advanced Visual Layout (High Value, Low Complexity)"]
    Tier2 --> Tier3["Tier 3: Dynamic & Live Updates (Interactive, Threading)"]
    Tier3 -.-> Tier4["Tier 4: Heavy Ecosystem Modules (Optional / Heavy Deps)"]
```

## Tier 3: Dynamic & Live Displays *(Interactive, Self-Contained)*
*Goal: Support in-place terminal redraws, progress bars, and status spinners.*
---

### 3.1 Smooth Progress Bar (`rich::progress`)
- [ ] Smooth fractional block characters: `█`, `▉`, `▊`, `▋`, `▌`, `▍`, `▎`, `▏`, `░`.
- [ ] Standard progress bar columns:
  - Task description.
  - Progress bar track.
  - Percentage completed (`[ 75%]`).
  - Transfer speed & ETA estimator.
- [ ] Fluent range tracking adapter: `for (auto& item : items | track("Processing")) { ... }`.
## Current Status Overview

### 3.2 Live Display Engine (`rich::live`)
- [ ] In-place screen redraw using VT cursor repositioning escapes (`\x1b[<N>F` move up lines, `\x1b[0J` clear down).
- [ ] Atomic screen flush via mutex to prevent output tearing.
- [ ] Background refresh loop driven by `std::jthread`.
| Tier | Focus | Dependencies | Status |
| :--- | :--- | :--- | :---: |
| **Tier 0** | Architectural Kernel & Basic Widgets | 0 (Pure C++20) | ✅ **Completed & Validated** |
| **Tier 1** | Core Polish & Text Foundations | 0 (Pure C++20) | 🟡 **Next Up** |
| **Tier 2** | Advanced Visual Layout (Tree, Align, Split Grid) | 0 (Pure C++20) | 📋 **Planned** |
| **Tier 3** | Dynamic & Live Displays (Progress Bars, Spinners) | 0 (`std::jthread` / VT escapes) | 📋 **Planned** |
| **Tier 4** | Heavy Ecosystem Modules (Markdown, Highlighting, Stacktrace) | Optional 3rd-party (`md4c`, `cpptrace`) | ⚠️ **Optional / Deferrable** |

### 3.3 Status Spinner (`rich::status`)
- [ ] Indeterminate loading spinners using animated Braille frames (`⠋⠙⠹⠸⠼⠴⠦⠧⠇⠏`).
---

---
## Tier 0: Architectural Kernel & Proof-of-Concept *(Completed)*
*Goal: Prove the C++20 concept, type erasure, and exact layout match against Python Rich.*

## ⚠️ Tier 4: Heavy Ecosystem Modules *(Optional / Deferrable)*
*Goal: Provide specialized document rendering or introspection only if strictly needed.*
*Note: Stop before this tier if zero-dependency and lightweight compilation are priorities.*
- [x] **Renderable Concept & AnyRenderable**: C++20 ADL customization points (`render_item`, `measure_item`) with value-semantic type erasure.
- [x] **Core Primitives**: `Segment`, `Style` (16 ANSI colors, bright variants, bold, dim, italic, underline, reverse, RGB), `Color`.
- [x] **Console**: `Console::print`, `Console::rule`, `Console::export_text`, record mode buffer, automatic Windows `CP_UTF8` and VT processing initialization.
- [x] **Containers & Decorators**:
  - `Panel` with centered title, custom borders, multiline support.
  - `Table` with multi-column grid, Unicode box drawing, column justification.
  - `Columns` with multi-row wrapping and independent left-aligned lines.
  - `Padding` with pipe operator (`|`) composition.
- [x] **Automated Oracle Test Suite**: `tests/verify_oracle.py` running byte-for-byte binary comparison against Python Rich for identical layouts (5/5 tests passing).

| Module | Python Rich Equivalent | Proposed C++ Strategy | Dependency Impact | Recommendation |
| :--- | :--- | :--- | :--- | :--- |
| **Markdown** | `rich.markdown` | Integrate `md4c` CommonMark parser | Lightweight C library | **Optional**: Integrate as separate CMake target `rich::markdown` |
| **Syntax Highlighting** | `rich.syntax` | Regex lexer or Tree-Sitter | Heavy grammar binaries | **Defer**: Provide pluggable `Highlighter` interface instead |
| **Tracebacks** | `rich.traceback` | `cpptrace` or C++23 `<stacktrace>` | Platform symbol resolution | **Nice-to-have**: Standalone CMake target `rich::traceback` |
| **SVG Export** | `Console.save_svg` | Pure C++ SVG string templating | **0 deps** | **Useful**: Web snapshots of terminal sessions |
| **Full Unicode Tables** | `rich.cells` | Generate static C++ table or `utf8proc` | Minimal table or 1 C dep | **Only if needed**: For CJK/emoji-heavy tools |
---

---
## Tier 1: Core Essentials & Zero-Dep Polish *(High Value, Zero Dependencies)*
*Goal: Make the library completely robust for standard CLI tool output with zero external dependencies.*

## Downstream Packaging & CI
- [x] Basic GitHub Actions CI workflow (`.github/workflows/ci.yml`).
- [x] Basic `vcpkg.json` manifest.
- [ ] Matrix CI build across:
  - MSVC (Windows x64)
  - GCC 11+ (Ubuntu Linux x64)
  - Clang 13+ (macOS ARM64/x64)
- [ ] CMake downstream export verification (`find_package(rich CONFIG REQUIRED)`).
### 1.1 Terminal Dimensions & Stream Detection
- [ ] **Auto Terminal Width**:
  - Windows: Query `GetConsoleScreenBufferInfo` for active window column count.
  - POSIX: Query `ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws)`.
  - Fallback: Default to 80 columns when redirected to a pipe or file (`isatty`).
- [ ] **NO_COLOR & Color Detection**:
  - Respect `NO_COLOR` environment variable (https://no-color.org).
  - Inspect `COLORTERM` (for truecolor / 24-bit support) and `TERM`.
### 1.2 Text Wrapping & Justification (`rich::Text`)
- [ ] **Word Wrapping Engine**:
  - Soft-wrap text streams at whitespace boundaries to fit within specified console widths.
  - Support `Overflow::Fold`, `Overflow::Crop`, and `Overflow::Ellipsis` (`...`).
- [ ] **Text Alignment**:
  - Add text justification modes: `Justify::Left`, `Justify::Center`, `Justify::Right`, `Justify::Full`.

### 1.3 Box Style Library (`rich::box`)
- [ ] **Box Presets**:
  - `box::ROUNDED` (current default: `╭─╮│╰─╯`)
  - `box::SQUARE` (`┌─┐│└─┘`)
  - `box::HEAVY` (`┏━┓┃┗━┛`)
  - `box::DOUBLE` (`╔═╗║╚═╝`)
  - `box::ASCII` (`+-+|+-+`)
  - `box::MINIMAL` (no vertical outer borders)
- [ ] **Configurable Borders**: Allow passing `{.box = box::DOUBLE}` to `Panel` and `Table`.

### 1.4 Oracle Parity Validation
- [ ] Add Oracle test cases for:
  - `box_styles`: Comparing Table and Panel with each box preset against Python Rich.
  - `text_wrap`: Comparing soft-wrapped paragraphs against Python Rich text wrapping.

---

## Tier 2: Advanced Visual Layout *(High Value, Low Complexity, Zero Dependencies)*
*Goal: Deliver full visual dashboard and structural composition capabilities.*

### 2.1 `Tree` Widget (`rich::tree::Tree`)
- [ ] Hierarchical data visualization with guide characters (`├── `, `└── `, `│   `).
- [ ] Arbitrary renderables as node labels (allowing nested Panels or Tables inside Tree branches).
- [ ] Expandable and collapsible representation.

### 2.2 `Align` & `Constrain` Decorators
- [ ] `Align`: Wrapper to horizontally center, left-align, or right-align any renderable within target width.
  - Syntax: `panel(...) | align(Justify::Center)`
- [ ] `Constrain`: Clamp maximum width of any nested renderable.
  - Syntax: `table | constrain(60)`

### 2.3 `Layout` Engine (Flexible Grid Splits)
- [ ] Split screen into sub-regions:
  - `layout.split_row(...)` (horizontal division).
  - `layout.split_column(...)` (vertical division).
  - Proportional sizing via `ratio` (e.g. 1:2 split).
  - Fixed-size constraints (`size = 10`).

### 2.4 Oracle Parity Validation
- [ ] Add Oracle test cases for `tree_basic` and `layout_split`.
This document structures the roadmap for **Rich C++** by **complexity vs. value tiers**, enabling modular development without forcing heavy third-party dependencies unless explicitly desired.

---
```mermaid
graph TD
    Tier1["Tier 1: Core Essentials & Zero-Dep Polish (High Value, Zero Deps)"] --> Tier2["Tier 2: Advanced Visual Layout (High Value, Low Complexity)"]
    Tier2 --> Tier3["Tier 3: Dynamic & Live Updates (Interactive, Threading)"]
    Tier3 -.-> Tier4["Tier 4: Heavy Ecosystem Modules (Optional / Heavy Deps)"]
```

## Tier 3: Dynamic & Live Displays *(Interactive, Self-Contained)*
*Goal: Support in-place terminal redraws, progress bars, and status spinners.*
---

### 3.1 Smooth Progress Bar (`rich::progress`)
- [ ] Smooth fractional block characters: `█`, `▉`, `▊`, `▋`, `▌`, `▍`, `▎`, `▏`, `░`.
- [ ] Standard progress bar columns:
  - Task description.
  - Progress bar track.
  - Percentage completed (`[ 75%]`).
  - Transfer speed & ETA estimator.
- [ ] Fluent range tracking adapter: `for (auto& item : items | track("Processing")) { ... }`.
## Current Status Overview

### 3.2 Live Display Engine (`rich::live`)
- [ ] In-place screen redraw using VT cursor repositioning escapes (`\x1b[<N>F` move up lines, `\x1b[0J` clear down).
- [ ] Atomic screen flush via mutex to prevent output tearing.
- [ ] Background refresh loop driven by `std::jthread`.
| Tier | Focus | Dependencies | Status |
| :--- | :--- | :--- | :---: |
| **Tier 0** | Architectural Kernel & Basic Widgets | 0 (Pure C++20) | ✅ **Completed & Validated** |
| **Tier 1** | Core Polish & Text Foundations (Boxes, Wrapping, Terminal Width, NO_COLOR) | 0 (Pure C++20) | ✅ **Completed & Validated (10/10 Oracle)** |
| **Tier 2** | Advanced Visual Layout (Tree, Align, Split Grid) | 0 (Pure C++20) | 🟡 **In Progress (`Tree` Done)** |
| **Tier 3** | Dynamic & Live Displays (Progress Bars, Spinners) | 0 (`std::jthread` / VT escapes) | 🟡 **In Progress (`ProgressBar` Done)** |
| **Tier 4** | Heavy Ecosystem Modules (Markdown, Highlighting, Stacktrace) | Optional 3rd-party (`md4c`, `cpptrace`) | ⚠️ **Optional / Deferrable** |

### 3.3 Status Spinner (`rich::status`)
- [ ] Indeterminate loading spinners using animated Braille frames (`⠋⠙⠹⠸⠼⠴⠦⠧⠇⠏`).
---

---
## Tier 0: Architectural Kernel & Proof-of-Concept *(Completed)*
## ⚠️ Tier 4: Heavy Ecosystem Modules *(Optional / Deferrable)*
*Goal: Provide specialized document rendering or introspection only if strictly needed.*
*Note: Stop before this tier if zero-dependency and lightweight compilation are priorities.*
- [x] **Renderable Concept & AnyRenderable**: C++20 ADL customization points (`render_item`, `measure_item`) with value-semantic type erasure.
- [x] **Core Primitives**: `Segment`, `Style` (16 ANSI colors, bright variants, bold, dim, italic, underline, reverse, RGB), `Color`.
- [x] **Console**: `Console::print`, `Console::rule`, `Console::export_text`, record mode buffer, automatic Windows `CP_UTF8` and VT processing initialization.
- [x] **Containers & Decorators**:
  - `Panel` with centered title, custom borders, multiline support.
  - `Table` with multi-column grid, Unicode box drawing, column justification.
  - `Columns` with multi-row wrapping and independent left-aligned lines.
  - `Padding` with pipe operator (`|`) composition.
- [x] **Initial Oracle Validation**: Exact byte-level matches for basic Panel, Table, Rule, and Columns layouts.

| Module | Python Rich Equivalent | Proposed C++ Strategy | Dependency Impact | Recommendation |
| :--- | :--- | :--- | :--- | :--- |
| **Markdown** | `rich.markdown` | Integrate `md4c` CommonMark parser | Lightweight C library | **Optional**: Integrate as separate CMake target `rich::markdown` |
| **Syntax Highlighting** | `rich.syntax` | Regex lexer or Tree-Sitter | Heavy grammar binaries | **Defer**: Provide pluggable `Highlighter` interface instead |
| **Tracebacks** | `rich.traceback` | `cpptrace` or C++23 `<stacktrace>` | Platform symbol resolution | **Nice-to-have**: Standalone CMake target `rich::traceback` |
| **SVG Export** | `Console.save_svg` | Pure C++ SVG string templating | **0 deps** | **Useful**: Web snapshots of terminal sessions |
| **Full Unicode Tables** | `rich.cells` | Generate static C++ table or `utf8proc` | Minimal table or 1 C dep | **Only if needed**: For CJK/emoji-heavy tools |
---

---
## Tier 1: Core Essentials & Zero-Dep Polish *(Completed)*
*Goal: Make the library completely robust for standard CLI tool output with zero external dependencies.*

## Downstream Packaging & CI
- [x] Basic GitHub Actions CI workflow (`.github/workflows/ci.yml`).
- [x] Basic `vcpkg.json` manifest.
- [ ] Matrix CI build across:
  - MSVC (Windows x64)
  - GCC 11+ (Ubuntu Linux x64)
  - Clang 13+ (macOS ARM64/x64)
- [ ] CMake downstream export verification (`find_package(rich CONFIG REQUIRED)`).
### 1.1 Terminal Dimensions & Stream Detection
- [x] **Auto Terminal Width**:
  - Windows: Query `GetConsoleScreenBufferInfo` for active window column count.
  - POSIX: Query `ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws)`.
  - Fallback: Defaults to 80 columns when redirected or in fixed mode.
- [x] **NO_COLOR Standard**:
  - Respects `NO_COLOR` environment variable (https://no-color.org).
### 1.2 Text Wrapping & Formatting (`rich::text::Text`)
- [x] **Word Wrapping Engine**:
  - Soft-wrap text streams at whitespace boundaries to fit within console width.
  - Exact whitespace and trailing newline parity with Python Rich text wrapping.

### 1.3 Box Style Library (`rich::box`)
- [x] **Box Presets (`core/box.h`)**:
  - `box::ROUNDED` (`╭─╮│╰─╯`)
  - `box::SQUARE` (`┌─┐│└─┘`)
  - `box::HEAVY` (`┏━┓┃┗━┛`)
  - `box::DOUBLE` (`╔═╗║╚═╝`)
  - `box::ASCII` (`+-+|+-+`)
- [x] **Configurable Borders**: Both `Panel` and `Table` accept arbitrary box styles via designated initializers.

### 1.4 Oracle Parity Validation
- [x] Automated byte-level validation for `box_double`, `box_heavy`, `box_ascii`, and `text_wrap` (all passing).

---

## Tier 2: Advanced Visual Layout *(In Progress - Zero Dependencies)*
*Goal: Deliver full visual dashboard and structural composition capabilities.*

### 2.1 `Tree` Widget (`rich::tree::Tree`)
- [x] **Hierarchical Data Visualization (`panel/Tree.h`)**:
  - Nested tree branches with guide glyphs (`├── `, `└── `, `│   `).
  - Arbitrary labels with inline markup parsing.
  - Verified bit-for-bit against Python Rich `Tree` (Oracle test `tree_basic`).

### 2.2 `Align` & `Constrain` Decorators
- [ ] **`Align`**: Wrapper to horizontally center, left-align, or right-align any renderable within target width.
  - Syntax: `panel(...) | align(Justify::Center)`
- [ ] **`Constrain`**: Clamp maximum width of any nested renderable.
  - Syntax: `table | constrain(60)`

### 2.3 `Layout` Engine (Flexible Grid Splits)
- [ ] Split screen into sub-regions:
  - `layout.split_row(...)` (horizontal division).
  - `layout.split_column(...)` (vertical division).
  - Proportional sizing via `ratio` (e.g. 1:2 split).
  - Fixed-size constraints (`size = 10`).
This document structures the roadmap for **Rich C++** by **complexity vs. value tiers**, enabling modular development without forcing heavy third-party dependencies unless explicitly desired.

---
```mermaid
graph TD
    Tier1["Tier 1: Core Essentials & Zero-Dep Polish (High Value, Zero Deps)"] --> Tier2["Tier 2: Advanced Visual Layout (High Value, Low Complexity)"]
    Tier2 --> Tier3["Tier 3: Dynamic & Live Updates (Interactive, Threading)"]
    Tier3 -.-> Tier4["Tier 4: Heavy Ecosystem Modules (Optional / Heavy Deps)"]
```

## Tier 3: Dynamic & Live Displays *(In Progress - Self-Contained)*
*Goal: Support in-place terminal redraws, progress bars, and status spinners.*
---

### 3.1 Smooth Progress Bar (`rich::progress`)
- [x] **Block Progress Bar (`core/ProgressBar.h`)**:
  - Fraction block characters: `█` (filled) and `░` (empty).
  - Percentage indicator with aligned padding.
- [ ] **Multi-Column Progress Tracker**:
  - Progress bar track + transfer speed + ETA estimator.
  - Fluent range tracking adapter: `for (auto& item : items | track("Processing")) { ... }`.
## Current Status Overview

### 3.2 Live Display Engine (`rich::live`)
- [ ] In-place screen redraw using VT cursor repositioning escapes (`\x1b[<N>F` move up lines, `\x1b[0J` clear down).
- [ ] Atomic screen flush via mutex to prevent output tearing.
- [ ] Background refresh loop driven by `std::jthread`.
| Tier | Focus | Dependencies | Status |
| :--- | :--- | :--- | :---: |
| **Tier 0** | Architectural Kernel & Basic Widgets | 0 (Pure C++20) | ✅ **Completed & Validated** |
| **Tier 1** | Core Polish & Text Foundations (Boxes, Wrapping, Terminal Width, NO_COLOR) | 0 (Pure C++20) | ✅ **Completed & Validated (14/14 Oracle)** |
| **Tier 2** | Advanced Visual Layout (Tree, Align, Constrain) | 0 (Pure C++20) | 🟡 **In Progress (`Tree`/`Align`/`Constrain` Done, `Layout` pending)** |
| **Tier 3** | Dynamic & Live Displays (Progress Bars, Spinners) | 0 (`std::jthread` / VT escapes) | 🟡 **In Progress (`ProgressBar` Done)** |
| **Tier 4** | Heavy Ecosystem Modules (Markdown, Highlighting, Stacktrace) | Optional 3rd-party (`md4c`, `cpptrace`) | ⚠️ **Optional / Deferrable** |

### 3.3 Status Spinner (`rich::status`)
- [ ] Indeterminate loading spinners using animated Braille frames (`⠋⠙⠹⠸⠼⠴⠦⠧⠇⠏`).
---

---
## Tier 0: Architectural Kernel & Proof-of-Concept *(Completed)*
## ⚠️ Tier 4: Heavy Ecosystem Modules *(Optional / Deferrable)*
*Goal: Provide specialized document rendering or introspection only if strictly needed.*
*Note: Stop before this tier if zero-dependency and lightweight compilation are priorities.*
- [x] **Renderable Concept & AnyRenderable**: C++20 ADL customization points (`render_item`, `measure_item`) with value-semantic type erasure.
- [x] **Core Primitives**: `Segment`, `Style` (16 ANSI colors, bright variants, bold, dim, italic, underline, reverse, RGB), `Color`.
- [x] **Console**: `Console::print`, `Console::rule`, `Console::export_text`, record mode buffer, automatic Windows `CP_UTF8` and VT processing initialization.
- [x] **Containers & Decorators**:
  - `Panel` with centered title, custom borders, multiline support.
  - `Table` with multi-column grid, Unicode box drawing, column justification.
  - `Columns` with multi-row wrapping and independent left-aligned lines.
  - `Padding` with pipe operator (`|`) composition.
- [x] **Initial Oracle Validation**: Exact byte-level matches for basic Panel, Table, Rule, and Columns layouts.

| Module | Python Rich Equivalent | Proposed C++ Strategy | Dependency Impact | Recommendation |
| :--- | :--- | :--- | :--- | :--- |
| **Markdown** | `rich.markdown` | Integrate `md4c` CommonMark parser | Lightweight C library | **Optional**: Integrate as separate CMake target `rich::markdown` |
| **Syntax Highlighting** | `rich.syntax` | Regex lexer or Tree-Sitter | Heavy grammar binaries | **Defer**: Provide pluggable `Highlighter` interface instead |
| **Tracebacks** | `rich.traceback` | `cpptrace` or C++23 `<stacktrace>` | Platform symbol resolution | **Nice-to-have**: Standalone CMake target `rich::traceback` |
| **SVG Export** | `Console.save_svg` | Pure C++ SVG string templating | **0 deps** | **Useful**: Web snapshots of terminal sessions |
| **Full Unicode Tables** | `rich.cells` | Generate static C++ table or `utf8proc` | Minimal table or 1 C dep | **Only if needed**: For CJK/emoji-heavy tools |
---

---
## Tier 1: Core Essentials & Zero-Dep Polish *(Completed)*
*Goal: Make the library completely robust for standard CLI tool output with zero external dependencies.*

## Downstream Packaging & CI
- [x] Basic GitHub Actions CI workflow (`.github/workflows/ci.yml`).
- [x] Basic `vcpkg.json` manifest.
- [ ] Matrix CI build across:
  - MSVC (Windows x64)
  - GCC 11+ (Ubuntu Linux x64)
  - Clang 13+ (macOS ARM64/x64)
- [ ] CMake downstream export verification (`find_package(rich CONFIG REQUIRED)`).
### 1.1 Terminal Dimensions & Stream Detection
- [x] **Auto Terminal Width**:
  - Windows: Query `GetConsoleScreenBufferInfo` for active window column count.
  - POSIX: Query `ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws)`.
  - Fallback: Defaults to 80 columns when redirected or in fixed mode.
- [x] **NO_COLOR Standard**:
  - Respects `NO_COLOR` environment variable (https://no-color.org).
### 1.2 Text Wrapping & Formatting (`rich::text::Text`)
- [x] **Word Wrapping Engine**:
  - Soft-wrap text streams at whitespace boundaries to fit within console width.
  - Exact whitespace and trailing newline parity with Python Rich text wrapping.

### 1.3 Box Style Library (`rich::box`)
- [x] **Box Presets (`core/box.h`)**:
  - `box::ROUNDED` (`╭─╮│╰─╯`)
  - `box::SQUARE` (`┌─┐│└─┘`)
  - `box::HEAVY` (`┏━┓┃┗━┛`)
  - `box::DOUBLE` (`╔═╗║╚═╝`)
  - `box::ASCII` (`+-+|+-+`)
- [x] **Configurable Borders**: Both `Panel` and `Table` accept arbitrary box styles via designated initializers.

### 1.4 Oracle Parity Validation
- [x] Automated byte-level validation for `box_double`, `box_heavy`, `box_ascii`, and `text_wrap` (all passing).

---

## Tier 2: Advanced Visual Layout *(In Progress - Zero Dependencies)*
*Goal: Deliver full visual dashboard and structural composition capabilities.*

### 2.1 `Tree` Widget (`rich::tree::Tree`)
- [x] **Hierarchical Data Visualization (`panel/Tree.h`)**:
  - Nested tree branches with guide glyphs (`├── `, `└── `, `│   `).
  - Arbitrary labels with inline markup parsing.
  - Verified bit-for-bit against Python Rich `Tree` (Oracle test `tree_basic`).

### 2.2 `Align` & `Constrain` Decorators
- [x] **`Align` (`panel/Align.h`)**: Wrapper to horizontally center, left-align, or right-align any renderable within console width.
  - Syntax: `panel(...) | align(Justify::Center)`
  - Line normalization + padding parity with Python `Align` (Oracle tests `align_center`, `align_right`, `align_left`).
- [x] **`Constrain` (`panel/Constrain.h`)**: Clamp maximum width of any nested renderable.
  - Syntax: `table | constrain(60)`
  - Verified bit-for-bit against Python `Constrain` (Oracle test `constrain_basic`).

### 2.3 `Layout` Engine (Flexible Grid Splits)
- [ ] Split screen into sub-regions:
  - `layout.split_row(...)` (horizontal division).
  - `layout.split_column(...)` (vertical division).
  - Proportional sizing via `ratio` (e.g. 1:2 split).
  - Fixed-size constraints (`size = 10`).

---

## Tier 3: Dynamic & Live Displays *(In Progress - Self-Contained)*
*Goal: Support in-place terminal redraws, progress bars, and status spinners.*

### 3.1 Smooth Progress Bar (`rich::progress`)
- [x] **Block Progress Bar (`core/ProgressBar.h`)**:
  - Fraction block characters: `█` (filled) and `░` (empty).
  - Percentage indicator with aligned padding.
- [ ] **Multi-Column Progress Tracker**:
  - Progress bar track + transfer speed + ETA estimator.
  - Fluent range tracking adapter: `for (auto& item : items | track("Processing")) { ... }`.

### 3.2 Live Display Engine (`rich::live`)
- [ ] In-place screen redraw using VT cursor repositioning escapes (`\x1b[<N>F` move up lines, `\x1b[0J` clear down).
- [ ] Atomic screen flush via mutex to prevent output tearing.
- [ ] Background refresh loop driven by `std::jthread`.

### 3.3 Status Spinner (`rich::status`)
- [ ] Indeterminate loading spinners using animated Braille frames (`⠋⠙⠹⠸⠼⠴⠦⠧⠇⠏`).

---

## ⚠️ Tier 4: Heavy Ecosystem Modules *(Optional / Deferrable)*
*Goal: Provide specialized document rendering or introspection only if strictly needed.*
*Note: Stop before this tier if zero-dependency and lightweight compilation are priorities.*

| Module | Python Rich Equivalent | Proposed C++ Strategy | Dependency Impact | Recommendation |
| :--- | :--- | :--- | :--- | :--- |
| **Markdown** | `rich.markdown` | Integrate `md4c` CommonMark parser | Lightweight C library | **Optional**: Integrate as separate CMake target `rich::markdown` |
| **Syntax Highlighting** | `rich.syntax` | Regex lexer or Tree-Sitter | Heavy grammar binaries | **Defer**: Provide pluggable `Highlighter` interface instead |
| **Tracebacks** | `rich.traceback` | `cpptrace` or C++23 `<stacktrace>` | Platform symbol resolution | **Nice-to-have**: Standalone CMake target `rich::traceback` |
| **SVG Export** | `Console.save_svg` | Pure C++ SVG string templating | **0 deps** | **Useful**: Web snapshots of terminal sessions |
| **Full Unicode Tables** | `rich.cells` | Generate static C++ table or `utf8proc` | Minimal table or 1 C dep | **Only if needed**: For CJK/emoji-heavy tools |

---

## Downstream Packaging & CI
- [x] Basic GitHub Actions CI workflow (`.github/workflows/ci.yml`).
- [x] Basic `vcpkg.json` manifest.
- [ ] Matrix CI build across:
  - MSVC (Windows x64)
  - GCC 11+ (Ubuntu Linux x64)
  - Clang 13+ (macOS ARM64/x64)
- [ ] CMake downstream export verification (`find_package(rich CONFIG REQUIRED)`).
