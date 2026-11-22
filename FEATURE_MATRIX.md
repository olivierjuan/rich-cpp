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
| | Fluent / Pipe syntax | N/A (Python decorators) | `operator\|` | ✅ Done | ✅ Yes | `content \| pad(1) \| panel({...})`; exercised by `padding_basic` |
| **Markup & Text** | Inline markup `[bold red]...[/]` | `rich.markup` | `rich::parse_markup`, `StyledText` | ✅ Done | ✅ Yes | RE_TAGS grammar port: tag = `[` + `[a-z#/@]` start + `]`; `\[` escape, non-tag brackets literal, unknown tags → empty style, `[/]`/`[/name]` closes (aliases canonicalized), `on <color>` backgrounds; oracle `markup_basic` |
| | Text formatting & wrapping | `rich.text` | `rich::text::Text` | ✅ Done | ✅ Yes | Soft word wrapping matching Python Rich output |
| | Unicode Cell Width Calculation | `rich.cells` | `cell_width` / `visual_width` | ✅ Done | ✅ Yes | Generated East-Asian width table + ZWJ/VS16 handling (`CellTable.h`); oracles `text_wrap_cjk`, `text_wrap_emoji` |
| | Emoji codes `:smile:` | `rich.emoji` | `rich::emoji_replace` | ✅ Done | ✅ Yes | Python-parity `:name:` table (3608 codes, generated `EmojiTable.h`), `-emoji`/`-text` variants, unknown codes verbatim |
| **Layout & Containers** | `Panel` | `rich.panel` | `rich::panel::Panel` | ✅ Done | ✅ Yes | Centered titles, borders, multiline support, custom box styles |
| | `Padding` | `rich.padding` | `rich::decorator::Padding` | ✅ Done | ✅ Yes | Full-width top/bottom blanks, per-line left/right padding, inner width expansion, measure cap; oracle `padding_basic` |
| | `Columns` | `rich.columns` | `rich::decorator::Columns` | ✅ Done | ✅ Yes | Multi-row wrapping, varied card widths, left-alignment |
| | `Table` | `rich.table` | `rich::table::Table` | ✅ Done | ✅ Yes | Multi-column, custom box styles, left/right/center justification |
| | `Rule` | `rich.rule` | `rich::Rule`, `Console::rule` | ✅ Done | ✅ Yes | Centered title, dashed box lines, exact byte match |
| | `Box` styles library | `rich.box` | `rich::box::Box` | ✅ Done | ✅ Yes | `ROUNDED`, `SQUARE`, `HEAVY`, `DOUBLE`, `ASCII` |
| | `Tree` (hierarchical tree view) | `rich.tree` | `rich::tree::Tree` | ✅ Done | ✅ Yes | Recursive branch guides (`├── `, `└── `, `│   `), exact byte match |
| | `Layout` (Split grid engine) | `rich.layout` | `rich::layout::Layout` | ✅ Done | ✅ Yes | split_row/split_column with fixed size + ratio distribution, exact byte match |
| | `Align` (Left/Center/Right) | `rich.align` | `rich::align::Align`, `align(Justify)` | ✅ Done | ✅ Yes | Full-width padding parity (`left`, `center`, `right`), exact byte match |
| | `Constrain` (Max width clamp) | `rich.constrain` | `rich::constrain::Constrain`, `constrain(N)` | ✅ Done | ✅ Yes | Clamps child options width, exact byte match |
| **Dynamic & Live** | `Progress` bar | `rich.progress` | `rich::progress::ProgressBar` | ✅ Done | ✅ Yes | Block characters (`█`, `░`) and percentage indicator; oracle `progress_bar_basic` |
| | `ProgressTracker` (multi-task, speed, ETA) | `rich.progress` | `rich::progress::ProgressTracker` | ✅ Done | ✅ Yes | Half-cell bar, rolling-window speed, ETA; oracle `progress_tracker`, `progress_speed` |
| | `track()` range adapter | `rich.progress` | `rich::progress::track(tracker, desc)(range)` | ✅ Done | ✅ Yes | Ticks task per iteration; exception-safe; demo in `quick.cpp` §9; oracle `track_basic` |
| | `Live` (dynamic terminal refresh) | `rich.live` | `rich::live::Live` | ✅ Done | ✅ Yes | In-place VT redraw, non-TTY degradation, transient clear; oracle `live_static` |
| | `Status` spinner | `rich.status` | `rich::status::Status` | ✅ Done | ✅ Yes | Braille frames, 80 ms interval, deterministic teardown; oracle `status_static` |
| | `Pretty` print (data structures) | `rich.pretty` | `rich::pretty::pretty` | ✅ Done | ✅ Yes | Node/expand layout parity for sequences, maps, pairs/tuples, optionals, scalars; oracles `pretty_list`, `pretty_dict`, `pretty_scalars`, `pretty_options` |
| | `Syntax` highlighting | `rich.syntax` | `rich::highlight::Syntax` | ✅ Done | ✅ Yes | Monokai theme; python/cpp/json/text lexers (regex); `SyntaxOptions.padding`; oracle `syntax_basic` |
| | `Markdown` rendering | `rich.markdown` | `rich::markdown::markdown(doc, hyperlinks)` | ✅ Done | ✅ Yes | Zero-dep CommonMark subset: headings, paragraphs, bullets/ordered lists (nesting), quotes, fenced code, tables, inline bold/italic/code/strike/links/kbd; theme table; oracles `markdown_basic` (byte-exact vs Python) |
| | `Traceback` (pretty stack traces) | `rich.traceback` | `rich::traceback::print_current_trace` / `capture_stack` | ✅ Done | ❌ No | Win32 `CaptureStackBackTrace` + psapi module table + DbgHelp symbol names (local PDBs, explicit `SymLoadModule64`); graceful module+offset fallback without symbols; no byte-exact oracle (addresses nondeterministic) |
| | `JSON` viewer | `rich.json` | `rich::json::json` | ✅ Done | ✅ Yes | Recursive-descent parser + syntax-colored tree rendering; oracle `json_basic` |
| **System & Platform** | Windows VT / UTF-8 auto-init | `rich._windows` | `rich::platform::enable_vt` | ✅ Done | ✅ Yes | Automatic `CP_UTF8` and `ENABLE_VIRTUAL_TERMINAL_PROCESSING` |
| | Auto terminal size detection | `rich.console` | `detect_terminal_width` | ✅ Done | ❌ No | Windows CSBI (`GetConsoleScreenBufferInfo`) + POSIX `ioctl` |
| | `NO_COLOR` standard | `rich.console` | `no_color_enabled` | ✅ Done | ❌ No | Respects `NO_COLOR` environment variable |
| | Record / Export (Text) | `rich.console` | `Console::export_text` | ✅ Done | ✅ Yes | Records terminal output to buffer |
| | Export to SVG | `rich.console` | `Console::export_svg` | ✅ Done | ✅ Yes | SVG terminal snapshot (`CONSOLE_SVG_FORMAT` port, `unique_id` from segment-repr adler32) |
| | Export to HTML | `rich.console` | `Console::export_html` | ✅ Done | ✅ Yes | HTML terminal snapshot (`DEFAULT_TERMINAL_THEME`, class-based CSS); oracle `export_html` |
| | Automated Oracle test suite | Tests | `src/rich-cpp/tests/verify_oracle.py` | ✅ Done | ✅ Yes | 36/36 exact byte-level parity checks with Python Rich |

---

### Legend
- ✅ **Done**: Fully implemented and validated.
- ✅ **Yes (Oracle)**: Validated against Python Rich byte-for-byte in automated test suite.
- 🟡 **Partial / Scaffolded**: Architecture / interface in place or subset implemented.
- ❌ **Not Started**: Planned in roadmap.
