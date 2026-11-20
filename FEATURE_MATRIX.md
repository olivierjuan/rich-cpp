# Rich Python vs Rich C++ Feature Parity Matrix

This document tracks the feature coverage of **Rich C++** relative to **Rich Python** (upstream `Textualize/rich`).

| Category | Python Rich Feature | Python Module | C++ Rich Equivalent | Status | Verified vs Oracle | Notes |
| :--- | :--- | :--- | :--- | :---: | :---: | :--- |
| **Core Architecture** | `Segment` (text, style) | `rich.segment` | `rich::Segment` | ✅ Done | ✅ Yes | Core rendering unit |
| | `Style` (colors, attributes) | `rich.style` | `rich::Style` | ✅ Done | ✅ Yes | bold, dim, italic, underline, reverse, 16 colors, bright colors, rgb |
| | `Color` (RGB / 256 / 16 / Auto) | `rich.color` | `rich::Color` | ✅ Done | ✅ Yes | ANSI codes + RGB structs |
| | `Console` | `rich.console` | `rich::Console` | ✅ Done | ✅ Yes | print, rule, export_text, record mode, Windows VT init |
| | `Renderable` protocol | `rich.protocol` | `rich::Renderable` (C++20 Concept) | ✅ Done | ✅ Yes | Non-intrusive ADL dispatch (`render_item`, `measure_item`) |
| | Type-erased wrapper | Python Duck Typing | `rich::AnyRenderable` | ✅ Done | ✅ Yes | Value-semantic wrapper (`std::shared_ptr<const Concept>`) |
| | Fluent / Pipe syntax | N/A (Python decorators) | `operator\|` | ✅ Done | ✅ Yes | `content \| pad(1) \| panel({...})` |
| **Markup & Text** | Inline markup `[bold red]...[/]` | `rich.markup` | `rich::parse_markup`, `StyledText` | ✅ Done | ✅ Yes | Tag nesting, `[/]` closing, bracket escaping `[[` |
| | Text formatting & wrapping | `rich.text` | `rich::Text` | 🟡 Partial | ✅ Yes | Basic text and styles implemented; word-wrapping pending |
| | Unicode Cell Width Calculation | `rich.cells` | `cell_width` / `visual_width` | 🟡 Partial | ✅ Yes | Multi-byte UTF-8 width (1 vs 2 cols); full East Asian Width tables pending |
| | Emoji codes `:smile:` | `rich.emoji` | N/A | ❌ Not Started | ❌ No | Emoji shortcode lookup table |
| **Layout & Containers** | `Panel` | `rich.panel` | `rich::panel::Panel` | ✅ Done | ✅ Yes | Centered titles, borders, multiline support, exact byte match |
| | `Padding` | `rich.padding` | `rich::decorator::Padding` | ✅ Done | ✅ Yes | Top/bottom and left/right padding decorator |
| | `Columns` | `rich.columns` | `rich::decorator::Columns` | ✅ Done | ✅ Yes | Multi-row wrapping, varied card widths, left-alignment |
| | `Table` | `rich.table` | `rich::table::Table` | ✅ Done | ✅ Yes | Multi-column, Unicode rounded box, left/right/center justification |
| | `Rule` | `rich.rule` | `rich::Rule`, `Console::rule` | ✅ Done | ✅ Yes | Centered title, dashed box lines, exact byte match |
| | `Layout` (Split grid engine) | `rich.layout` | N/A | ❌ Not Started | ❌ No | Tree-based flexible split-screen layout (rows, cols, ratios) |
| | `Align` (Left/Center/Right/Top/Bottom) | `rich.align` | `rich::table::Justify` (partial) | 🟡 Partial | ❌ No | Standalone Align wrapper for arbitrary renderables |
| | `Constrain` (Max width clamp) | `rich.constrain` | N/A | ❌ Not Started | ❌ No | Constrain renderables to fixed bounding width |
| **Dynamic & Live** | `Live` (dynamic terminal refresh) | `rich.live` | N/A | 🟡 Scaffolded | ❌ No | Refresh loop via `std::jthread` in `Progress.h` |
| | `Progress` bar | `rich.progress` | `rich::live::Progress` | 🟡 Scaffolded | ❌ No | Basic thread loop scaffolded; block progress bars (`█`, `░`) pending |
| | `Status` spinner | `rich.status` | N/A | ❌ Not Started | ❌ No | Braille / Unicode animation frames |
| **Data & Inspection** | `Tree` (hierarchical tree view) | `rich.tree` | N/A | ❌ Not Started | ❌ No | Tree branches (`├──`, `└──`) and nested renderables |
| | `Pretty` print (data structures) | `rich.pretty` | N/A | ❌ Not Started | ❌ No | Pretty printer for `std::vector`, `std::map`, `std::tuple`, structs |
| | `Syntax` highlighting | `rich.syntax` | `rich::highlight::Highlighter` | 🟡 Scaffolded | ❌ No | Interface defined; Tree-sitter or regex highlighter pending |
| | `Markdown` rendering | `rich.markdown` | `rich::markdown::parse` | 🟡 Scaffolded | ❌ No | Header stub in place; md4c integration pending |
| | `Traceback` (pretty stack traces) | `rich.traceback` | `rich::traceback::print_current_trace` | 🟡 Scaffolded | ❌ No | Header stub in place; `cpptrace` integration pending |
| | `JSON` viewer | `rich.json` | N/A | ❌ Not Started | ❌ No | Formatted syntax-colored JSON trees |
| **System & Platform** | Windows VT / UTF-8 auto-init | `rich._windows` | `rich::platform::enable_vt` | ✅ Done | ✅ Yes | Automatic `CP_UTF8` and `ENABLE_VIRTUAL_TERMINAL_PROCESSING` |
| | Terminal size detection | `rich.console` | Hardcoded / ConsoleOptions | 🟡 Partial | ❌ No | Needs `GetConsoleScreenBufferInfo` / `ioctl(TIOCGWINSZ)` |
| | Record / Export (Text) | `rich.console` | `Console::export_text` | ✅ Done | ✅ Yes | Records terminal output to buffer |
| | Export to HTML / SVG | `rich.console` | N/A | ❌ Not Started | ❌ No | Generates SVG/HTML snapshots of terminal output |
| | Automated Oracle test suite | Tests | `tests/verify_oracle.py` | ✅ Done | ✅ Yes | 5/5 exact byte-level parity checks with Python Rich |

---

### Legend
- ✅ **Done**: Fully implemented and validated.
- ✅ **Yes (Oracle)**: Validated against Python Rich byte-for-byte in automated test suite.
- 🟡 **Partial / Scaffolded**: Architecture / interface in place or subset implemented.
- ❌ **Not Started**: Planned in roadmap.
