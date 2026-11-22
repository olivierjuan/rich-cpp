# Rich Python vs Rich C++ Feature Parity Matrix

This document tracks the feature coverage of **Rich C++** relative to **Rich Python** (upstream `Textualize/rich`).

| Category | Python Rich Feature | Python Module | C++ Rich Equivalent | Status | Verified vs Oracle | Notes |
| :--- | :--- | :--- | :--- | :---: | :---: | :--- |
| **Core Architecture** | `Segment` (text, style) | `rich.segment` | `rich::Segment` | ✅ Done | ✅ Yes | Core rendering unit |
| | `Style` (colors, attributes) | `rich.style` | `rich::Style` | ✅ Done | ✅ Yes | bold, dim, italic, underline, reverse, 16 colors, bright colors, rgb |
| | `Console` | `rich.console` | `rich::Console` | ✅ Done | ✅ Yes | print, rule, export_text, record mode, Windows VT init |
| | `Color` (RGB / 256 / 16 / Auto) | `rich.color` | `rich::Color` | ✅ Done | ✅ Yes | 16 + bright 16 ANSI + 256-color `color(N)` + RGB truecolor + `default`; `downgrade_color` (256 via colorsys HLS + cube, 16 via BT.601 palette match) is byte-exact Python parity; oracles `style_sgr`, `style_sgr_256` |
| | 256-color palette | `rich.color` | `rich::Color::charcoal(0-255)` | ✅ Done | ✅ Yes | 6×6×6 cube + 24-step grayscale ramp; SGR `38;5;N` / `48;5;N`; `color(N<16)` keeps STANDARD (SGR 30-37/90-97) like Python; oracles `style_sgr`, `style_sgr_256` |
| | Extra `Style` attributes (blink, blink2, conceal, underline2, frame, encircle, overline) | `rich.style` | `rich::Style` | ✅ Done | ✅ Yes | SGR 5 / 6 / 8 / 21 / 51 / 52 / 53 emitted in Python `_make_ansi_codes` order, only when set-and-explicit; markup names + aliases `uu`, `c`, `s`, `o`, `r` (no curves exist in rich 15.0.0); oracles `style_sgr`, `markup_basic` |
| | `Renderable` protocol | `rich.protocol` | `rich::Renderable` (C++20 Concept) | ✅ Done | ✅ Yes | Non-intrusive ADL dispatch (`render_item`, `measure_item`) |
| | Type-erased wrapper | Python Duck Typing | `rich::AnyRenderable` | ✅ Done | ✅ Yes | Value-semantic wrapper (`std::shared_ptr<const Concept>`) |
| | Fluent / Pipe syntax | N/A (Python decorators) | `operator\|` | ✅ Done | ✅ Yes | `content \| pad(1) \| panel({...})`; exercised by `padding_basic` |
| | `Console.log()` (timestamp/path/level log lines) | `rich.console`, `rich._log_render` | `Console::log` + `LogOptions` | ✅ Done | ✅ Yes | `log()` method + log-line grid (time column, message fold, path column); C++ takes path/line explicitly (`LogOptions{show_path, path, line_no}`) where Python walks the stack; injected clock via `LogOptions::time` (`tm`) mirrors Python's `get_datetime` — oracles `log_basic`, `log_sgr`; `log_locals` (scope dump) not ported |
| | Theme system (style-name mapping, `push_theme`/`pop_theme`) | `rich.theme`, `rich.default_styles` | `rich::Theme`, `Console::get_style`/`push_theme`/`pop_theme`/`use_theme` | ✅ Done | ✅ Yes | Full `DEFAULT_STYLES` port (~140 names); `get_style` resolves top theme entry → style-definition parse → default (MissingStyle otherwise); `ThemeScope` RAII for `use_theme`; log time/path and Rule line styles now resolve through the theme — oracle `theme_push` |
| | `Console.input()` (styled prompt input) | `rich.console` | — | ❌ Not Started | 🚫 | Interactive stdin read with prompt markup; not a pure render, not reproducible in CI |
| | `Console.capture()` / `screen()` + `Screen` (alt screen buffer) | `rich.console`, `rich.screen` | — | ❌ Not Started | ❌ No | In-memory render capture; alternate-screen renderable |
| | Control-code API (cursor move/erase/home, show/hide cursor, beep) | `rich.control` | — | ❌ Not Started | ❌ No | Public control-code module; Live/Status emit raw VT escapes inline instead |
| | Color-file / palette loading | `rich.colorfile`, `rich.palette` | — | ❌ Not Started | ❌ No | Color definitions from files; feeds the theme system |
| | File-proxy utilities (`FileProxy`, `FileFilter`, `NullFile`) | `rich.file_proxy`, `rich._null_file` | — | ❌ Not Started | ❌ No | I/O redirect / regex filter / null-stream helpers |
| | Self-diagnostics / object inspection | `rich.diagnose`, `rich._inspect` | — | ❌ Not Started | ❌ No | Niche: `diagnose()` report, `inspect()` renderable |
| | `scope()` (local/instance/global variable dump) | `rich.scope` | — | 🚫 Not portable | 🚫 | C++ has no runtime introspection of local/instance/global variables |
| | Jupyter / IPython integration (`JupyterMixin`, IPython extension, pager) | `rich.jupyter`, `rich._extension`, `rich.pager` | — | 🚫 N/A | 🚫 | Python-IPython ecosystem only |
| | Logging handler (`RichHandler` / `LogHandler`) | `rich.logging` | — | 🚫 N/A | 🚫 | Wraps Python's `logging` module; the portable part (`Console.log()`) is the row above |
| **Markup & Text** | Inline markup `[bold red]...[/]` | `rich.markup` | `rich::parse_markup`, `StyledText` | ✅ Done | ✅ Yes | RE_TAGS grammar port: tag = `[` + `[a-z#/@]` start + `]`; `\[` escape, non-tag brackets literal, unknown tags → empty style, `[/]`/`[/name]` closes (aliases canonicalized), `on <color>` backgrounds; oracle `markup_basic` |
| | Text formatting & wrapping | `rich.text` | `rich::text::Text` | ✅ Done | ✅ Yes | Soft word wrapping matching Python Rich output |
| | Unicode Cell Width Calculation | `rich.cells` | `cell_width` / `visual_width` | ✅ Done | ✅ Yes | Generated East-Asian width table + ZWJ/VS16 handling (`CellTable.h`); oracles `text_wrap_cjk`, `text_wrap_emoji` |
| | Emoji codes `:smile:` | `rich.emoji` | `rich::emoji_replace` | ✅ Done | ✅ Yes | Python-parity `:name:` table (3608 codes, generated `EmojiTable.h`), `-emoji`/`-text` variants, unknown codes verbatim |
| | `Text.from_ansi` (ANSI escape decoder) | `rich.ansi` | — | ❌ Not Started | ❌ No | Parse ANSI escape streams into styled Text (`AnsiDecoder`, `Style.parse`) |
| | Highlighters: `RegexHighlighter`, `ReprHighlighter`, `NumberHighlighter` | `rich.highlighter` | — | ❌ Not Started | ❌ No | User regex→style rules; colored repr output; number formatting rules (JSON + Syntax highlighters do exist) |
| | `Text` extras: `highlight_regex`, `assemble`, `concatenate`, `styled`, overflow modes (`ellipsis`, `crop`) | `rich.text` | — | ❌ Not Started | ❌ No | Cell-wrap engine is ported; these APIs/modes are not |
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
| | `Table` extras: footers, caption, `row_styles`, `expand`, `add_section` | `rich.table` | — | ❌ Not Started | ❌ No | Per-column footer row, table caption, per-row styling, edge expansion |
| | `Panel` extras: `subtitle` (bottom border), `style`, `expand`, `fit` | `rich.panel` | — | ❌ Not Started | ❌ No | Subtitle text on the bottom border; content style; width expansion |
| | `Rule` extras: `characters` (custom glyphs), `style`, `align` | `rich.rule` | — | ❌ Not Started | ❌ No | Custom rule line glyphs, rule styling, title alignment |
| | `Tree` extras: `guide_style`, `expand`/`hide` toggles | `rich.tree` | — | ❌ Not Started | ❌ No | Styled branch guides; per-branch show/hide state |
| | `Columns` extras: `expand`, `equal`, `column_first` | `rich.columns` | — | ❌ Not Started | ❌ No | Layout-option knobs beyond padding + multi-row wrap |
| **Dynamic & Live** | `Progress` bar | `rich.progress` | `rich::progress::ProgressBar` | ✅ Done | ✅ Yes | Block characters (`█`, `░`) and percentage indicator; oracle `progress_bar_basic` |
| | `ProgressTracker` (multi-task, speed, ETA) | `rich.progress` | `rich::progress::ProgressTracker` | ✅ Done | ✅ Yes | Half-cell bar, rolling-window speed, ETA; oracle `progress_tracker`, `progress_speed` |
| | `track()` range adapter | `rich.progress` | `rich::progress::track(tracker, desc)(range)` | ✅ Done | ✅ Yes | Ticks task per iteration; exception-safe; demo in `quick.cpp` §9; oracle `track_basic` |
| | `Live` (dynamic terminal refresh) | `rich.live` | `rich::live::Live` | ✅ Done | ✅ Yes | In-place VT redraw, non-TTY degradation, transient clear; oracle `live_static` |
| | `Status` spinner | `rich.status` | `rich::status::Status` | ✅ Done | ✅ Yes | Braille frames, 80 ms interval, deterministic teardown; oracle `status_static` |
| | `Pretty` print (data structures) | `rich.pretty` | `rich::pretty::pretty` | ✅ Done | ✅ Yes | Node/expand layout parity for sequences, maps, pairs/tuples, optionals, scalars; oracles `pretty_list`, `pretty_dict`, `pretty_scalars`, `pretty_options` |
| | `Syntax` highlighting | `rich.syntax` | `rich::highlight::Syntax` | ✅ Done | ✅ Yes | Monokai theme; python/cpp/json/text lexers (regex); `SyntaxOptions.padding`; oracle `syntax_basic` |
| | `Markdown` rendering | `rich.markdown` | `rich::markdown::markdown(doc, hyperlinks)` | ✅ Done | ✅ Yes | Zero-dep CommonMark subset: headings, paragraphs, bullets/ordered lists (nesting), quotes, fenced code, tables, inline bold/italic/code/strike/links/kbd; theme table; oracles `markdown_basic` (byte-exact vs Python) |
| | `Traceback` (pretty stack traces) | `rich.traceback` | `rich::traceback::print_current_trace` / `capture_stack` | ✅ Done | 🚫 | Win32 `CaptureStackBackTrace` + psapi module table + DbgHelp symbol names (local PDBs, explicit `SymLoadModule64`); graceful module+offset fallback without symbols; no byte-exact oracle (addresses/symbols nondeterministic) |
| | `JSON` viewer | `rich.json` | `rich::json::json` | ✅ Done | ✅ Yes | Recursive-descent parser + syntax-colored tree rendering; oracle `json_basic` |
| | `Progress` custom columns (9 `ProgressColumn` classes), `pulse`, transfer mode | `rich.progress`, `rich.progress_bar` | — | ❌ Not Started | ❌ No | `SpinnerColumn`, `TextColumn`, `BarColumn`, `TimeElapsedColumn`, `TimeRemainingColumn`, `FileSizeColumn`, `TotalFileSizeColumn`, `RenderableColumn`; pulsing indeterminate bar; file-transfer speed display. C++ `ProgressTracker` is a fixed 4-column layout |
| | Spinner registry (73 named animations + custom `Spinner`) | `rich.spinner`, `rich._spinners` | — | ❌ Not Started | ❌ No | `dots2`…`moon`, `tiny`, … + user-defined frames/interval; C++ ships 2 (`dots`, `line`) in `Status.h` |
| | `Live` extras: `screen` mode, `vertical`, `refresh_per_second` | `rich.live` | — | ❌ Not Started | ❌ No | Alternate-screen live region; vertical growth; configurable refresh rate |
| | `Pretty` struct introspection | `rich.pretty` | — | 🚫 Not portable | 🚫 | No `__rich_repr__`-style runtime field introspection in C++; containers/scalars are covered (see Pretty row) |
| | `Pretty` `ReprHighlighter` coloring | `rich.pretty`, `rich.highlighter` | — | ❌ Not Started | ❌ No | Syntax-colored repr output |
| | `Syntax` extended coverage: ~70 Pygments lexers, ~20 themes, `line_numbers`, `start_line`, `word_wrap`, `tab_size`, `elide_lines` | `rich.syntax` | — | ❌ Not Started | ❌ No | C++ has 4 regex lexers (python/cpp/json/text) and the monokai theme |
| | `Markdown` extras: task-list checkboxes, footnotes, reference-style links, table column alignment, setext (`==`) headings | `rich.markdown` | — | ❌ Not Started | ❌ No | Fenced code blocks *do* get `Syntax` highlighting (language detected from the fence info string) |
| | `filesize()` / `Number` renderables | `rich.filesize`, `rich.numbers` | — | ❌ Not Started | ❌ No | Unit-formatted sizes (decimal/binary); numbers with digit grouping + alignment |
| **System & Platform** | Windows VT / UTF-8 auto-init | `rich._windows` | `rich::platform::enable_vt` | ✅ Done | ✅ Yes | Automatic `CP_UTF8` and `ENABLE_VIRTUAL_TERMINAL_PROCESSING` |
| | Auto terminal size detection | `rich.console` | `detect_terminal_width` | ✅ Done | 🚫 | Windows CSBI (`GetConsoleScreenBufferInfo`) + POSIX `ioctl`; value depends on the live terminal, not reproducible in CI |
| | `NO_COLOR` standard | `rich.console` | `no_color_enabled` | ✅ Done | ❌ No | Respects `NO_COLOR` environment variable |
| | Record / Export (Text) | `rich.console` | `Console::export_text` | ✅ Done | ✅ Yes | Records terminal output to buffer |
| | Export to SVG | `rich.console` | `Console::export_svg` | ✅ Done | ✅ Yes | SVG terminal snapshot (`CONSOLE_SVG_FORMAT` port, `unique_id` from segment-repr adler32) |
| | Export to HTML | `rich.console` | `Console::export_html` | ✅ Done | ✅ Yes | HTML terminal snapshot (`DEFAULT_TERMINAL_THEME`, class-based CSS); oracle `export_html` |
| | Automated Oracle test suite | Tests | `src/rich-cpp/tests/verify_oracle.py` | ✅ Done | ✅ Yes | 41/41 exact byte-level parity checks with Python Rich |

---

### Legend
- ✅ **Done**: Fully implemented and validated.
- ✅ **Yes (Oracle)**: Validated against Python Rich byte-for-byte in automated test suite.
- 🟡 **Partial / Scaffolded**: Architecture / interface in place or subset implemented.
- ❌ **Not Started**: Not implemented; portable to C++ and a candidate for future work.
- 🚫 **Not portable / N/A**: Not feasible in C++ (no runtime introspection) or specific to the Python ecosystem (IPython, `logging` module).

**"Verified vs Oracle" column**
- ✅ — byte-exact in the automated suite.
- ❌ — oracle-able (deterministic render), but not yet checked.
- 🚫 — oracle impossible: nondeterministic output (Traceback addresses/symbols), live-environment state (terminal size), interactive I/O (`Console.input()`), or not portable to C++. (Wall-clock timestamps are oracle-able via clock injection — see `Console.log()`.)

### Coverage summary
- **41 oracle cases** (41/41 byte-exact) cover every deterministic ✅ row.
- **21 rows ❌ Not Started** — portable but not implemented (largest: Progress custom columns, spinner registry, `Text.from_ansi`, Syntax lexer/theme breadth, Table/Panel decoration).
- **4 rows 🚫** (Status) — `scope()`, Jupyter/IPython, `logging` handler, `Pretty` struct introspection.
- **7 rows 🚫** (Oracle column) — Traceback, terminal-size detection, `Console.input()`, + the 4 not-portable rows: byte-exact verification not possible.
