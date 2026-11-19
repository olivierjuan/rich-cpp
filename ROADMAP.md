# C++ Rich Library Roadmap

This document outlines the phased development of a C++ port of the Rich library, adhering to the architecture and design guidelines established for a consistent, user-friendly, and maintainable library.

## Phase 1: Core Foundation (The Immutable Kernel)
*Goal: Establish the absolute minimal core that validates the architecture (Concept/Type-Erasure/Console).*

- [x] **Project Structure**: CMake setup with shared library core.
- [x] **Basic Types**: `Color`, `Style`, `Segment` (immutable values).
- [x] **Renderable Concept**: C++20 concept for ADL-based extensibility.
- [x] **Type Erasure**: `AnyRenderable` wrapper for composable nesting.
- [x] **Minimal Console**: Interface for writing `Segments` to output.
- [x] **Demonstrator**: Minimal `Panel` widget to validate the composition model.

## Phase 2: Core UX (The "Fluent + Markup" API)
*Goal: Make the library usable for daily styling and text rendering.*

- [x] **Markup Parser**: Integrate with `std::format` to support `[bold red]text[/]`.
- [x] **Text Primitives**: Implement `Text` renderable with integrated markup parsing.
- [ ] **Unicode Handling**: Integrate `utf8proc` for cell-width and grapheme cluster support (critical for layout alignment).
- [ ] **Designated Initializers API**: Add `Options` structs for widgets to improve configurability without fluent verbosity.

## Phase 3: Layout & Widgets (Structural Composition)
*Goal: Support complex layouts.*

- [ ] **Layout Engines**: Implement `Columns` and `Layout` (calculating widths/heights based on `Measure`).
- [ ] **Table Widget**: Port Rich’s width-distribution algorithm (Table/Column/Row).
- [ ] **Border & Padding**: Implement `Panel`, `Rule`, `Padding` as renderable decorators (using the pipe `|` composition model).
- [ ] **Oracle Validation**: Establish a testing suite that compares output bytes against Rich (Python) output for identical inputs.

## Phase 4: Live & Advanced (Dynamic Content)
*Goal: Support dynamic terminal updates.*

- [ ] **Live/Progress/Status**: Implement `std::jthread`-based refresh loop with thread-safe output flushing.
- [ ] **Platform Shim**: Native VT enablement for Windows/Unix.

## Phase 5: Optional Modules (Ecosystem Integration)
*Goal: Separate concerns for minimal core impact.*

- [ ] **Markdown**: Integrate `md4c` as a standalone CMake target.
- [ ] **Tracebacks**: Integrate `cpptrace` or `std::stacktrace` for pretty error handling.
- [ ] **Syntax Highlighting**: Implement pluggable highlighter interface (e.g., regex-based or tree-sitter integration).
## Phase 6: Ecosystem & Adoption (The Polish)
*Goal: Enable reliable downstream adoption and long-term maintainability.*

- [ ] **Documentation**: API reference (e.g., Doxygen) and "Getting Started" tutorials.
- [ ] **Examples**: Comprehensive `examples/` demonstrating real-world usage (e.g., dashboard, log viewer).
- [ ] **CI/CD**: Automated testing pipeline for multiple platforms (Windows/Linux/macOS) and compiler versions.
- [ ] **Package Management**: First-class support for `vcpkg`, `Conan`, and `CPM.cmake` for seamless integration.
- [ ] **Performance**: Micro-benchmarks for `render` and `measure` loops to ensure high-frequency updates remain performant.
