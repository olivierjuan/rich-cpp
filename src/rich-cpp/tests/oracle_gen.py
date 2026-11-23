import io
import sys
from rich.console import Console
from rich.live import Live
from rich.panel import Panel
from rich.screen import Screen
from rich.table import Table
from rich.columns import Columns
from rich.text import Text
from rich.tree import Tree
from rich.align import Align
from rich.constrain import Constrain
from rich import box
from rich.layout import Layout

def generate_python_oracle(test_name: str) -> str:
    sink = io.StringIO()
    c = Console(file=sink, width=60, record=True, legacy_windows=False)

    if test_name == "panel_basic":
        c.print(Panel("Hello World", width=20, title="Test"))
    elif test_name == "panel_multiline":
        c.print(Panel("Hello World\nLine 2", width=24, title="Title"))
    elif test_name == "table_basic":
        t = Table(box=box.ROUNDED)
        t.add_column("Name")
        t.add_column("Age", justify="right")
        t.add_row("Alice", "30")
        t.add_row("Bob", "25")
        c.print(t)
    elif test_name == "rule_basic":
        c.rule("My Rule")
    elif test_name == "columns_basic":
        p1 = Panel("Card 1", width=16, title="A")
        p2 = Panel("Card 2", width=16, title="B")
        c.print(Columns([p1, p2], padding=(0, 2)))
    elif test_name == "box_double":
        c.print(Panel("Double Border", box=box.DOUBLE, width=20, title="Box"))
    elif test_name == "box_heavy":
        c.print(Panel("Heavy Border", box=box.HEAVY, width=20, title="Box"))
    elif test_name == "box_ascii":
        c.print(Panel("ASCII Border", box=box.ASCII, width=20, title="Box"))
    elif test_name == "text_wrap":
        c_small = Console(file=sink, width=20, record=True, legacy_windows=False)
        c_small.print("The quick brown fox jumps over the lazy dog near the riverbank")
        return c_small.export_text()
    elif test_name == "text_wrap_cjk":
        c_small = Console(file=sink, width=20, record=True, legacy_windows=False)
        c_small.print("漢字" * 10 + " abc def")
        return c_small.export_text()
    elif test_name == "text_wrap_emoji":
        # Wide emoji (2 cells) + regional-indicator flags (1 cell each)
        # exercising the generated East-Asian width table.
        c_small = Console(file=sink, width=20, record=True, legacy_windows=False)
        c_small.print("a " + "😀" * 15 + " b " + "🇫🇷" * 4)
        return c_small.export_text()
    elif test_name == "tree_basic":
        t = Tree("Root")
        t.add("Child 1")
        c2 = t.add("Child 2")
        c2.add("Grandchild 2.1")
        t.add("Child 3")
        c.print(t)
    elif test_name == "align_center":
        c.print(Align.center(Panel("Hello World", width=30)))
    elif test_name == "align_right":
        c.print(Align.right(Panel("Hello World", width=30)))
    elif test_name == "align_left":
        c.print(Align.left(Panel("Hello World", width=30)))
    elif test_name == "constrain_basic":
        c.print(Constrain(Panel("Hello World", width=30), 30))
    elif test_name == "layout_row":
        c_l = Console(file=sink, width=40, height=8, record=True, legacy_windows=False)
        root = Layout()
        root.split_row(Layout(name="left", size=20), Layout(name="right"))
        root["left"].update(Panel("Left", width=18, height=8))
        root["right"].update(Panel("Right", width=18, height=8))
        c_l.print(root)
        return c_l.export_text()
    elif test_name == "layout_column":
        c_l = Console(file=sink, width=40, height=10, record=True, legacy_windows=False)
        root = Layout()
        root.split_column(Layout(name="top", ratio=1), Layout(name="bottom", ratio=2))
        root["top"].update(Panel("Top", width=38, height=3))
        root["bottom"].update(Panel("Bottom", width=38, height=7))
        c_l.print(root)
        return c_l.export_text()
    elif test_name == "status_static":
        # Deterministic frame 0 of the "dots" spinner (braille ⠋) with a
        # markup description — equivalent to Status.__rich__() at t=0.
        from rich.spinner import Spinner
        c.print(Spinner("dots", "[bold]Loading[/] data").render(0))
        return c.export_text()
    elif test_name == "live_static":
        # Live's first frame is exactly a plain render of its renderable
        # (LiveRender renders with pad=False; no padding to console width).
        # So the reference is just printing the panel normally.
        p = Panel(Text("Hello World\nLine 2"), width=24, title="Live")
        c.print(p)
        return c.export_text()
    elif test_name == "progress_tracker":
        # Deterministic Progress via get_time=lambda: 100.0 (fixed clock) and
        # auto_refresh=False so nothing renders until stop(). Speed/ETA are
        # unknown ("-:--:--") because no real time elapses between add_task and
        # update under the frozen clock. This validates OUR format contract
        # against Python's actual column layout (description, bar, percentage,
        # ETA) produced by rich.progress.Progress defaults.
        from rich.progress import Progress
        prog = Progress(console=c, auto_refresh=False, get_time=lambda: 100.0)
        t1 = prog.add_task("Downloading", total=100)
        t2 = prog.add_task("Processing", total=200)
        with prog:
            prog.update(t1, completed=42)
            prog.update(t2, completed=137)
        return c.export_text()
    elif test_name == "progress_speed":
        # Rolling-window speed/ETA. Python's Progress binds get_time at
        # construction, so the clock must be advanced explicitly before each
        # update for sample timestamps to differ. Task start at t=100;
        # advance 5s per update:
        #   sample (105, 42) then (110, 42) -> speed = 42 / 5 = 8.4 it/s
        #   (sum skips the first sample); remaining 16 -> ceil(16 / 8.4) = 2
        #   -> ETA "0:00:02". The second task has a single sample, so its ETA
        #   is still "-:--:--" (speed needs >= 2 samples with non-zero span).
        from rich.progress import Progress
        clock = [100.0]
        prog = Progress(console=c, auto_refresh=False, get_time=lambda: clock[0])
        t1 = prog.add_task("Downloading", total=100)
        t2 = prog.add_task("Processing", total=200)
        with prog:
            clock[0] += 5.0
            prog.update(t1, completed=42)
            prog.update(t2, completed=137)
            clock[0] += 5.0
            prog.update(t1, completed=84)
        return c.export_text()
    elif test_name == "export_svg":
        # SVG terminal snapshot: styled markup line + plain line. The default
        # unique_id is derived deterministically (adler32 of segment reprs +
        # title), so output is byte-comparable.
        c2 = Console(file=sink, width=80, record=True, legacy_windows=False)
        c2.print("[bold red]Hello[/] [on blue]colored[/] [reverse]reversed[/] [dim]dim[/] [italic]it[/] [underline]ul[/]")
        c2.print("plain")
        return c2.export_svg(title="Rich")
    elif test_name == "export_html":
        # HTML terminal snapshot: same styled markup + plain content as the
        # export_svg case, exported with default options (class-based styles).
        c2 = Console(file=sink, width=80, record=True, legacy_windows=False)
        c2.print("[bold red]Hello[/] [on blue]colored[/] [reverse]reversed[/] [dim]dim[/] [italic]it[/] [underline]ul[/]")
        c2.print("plain")
        return c2.export_html()
    elif test_name == "emoji_basic":
        # Emoji shortcode replacement: known codes, unknown code left verbatim,
        # variant suffixes, inside markup tags.
        c2 = Console(file=sink, width=80, record=True, legacy_windows=False)
        c2.print("Hello :smile: and :dog: and :notanemoji:")
        c2.print("[bold red]:heart:[/] variant :copyright-emoji: :copyright-text:")
        return c2.export_text()
    elif test_name == "pretty_list":
        c2 = Console(file=sink, width=80, record=True, legacy_windows=False)
        c2.print([1, 2, 3])
        c2.print(["apple", "banana"])
        c2.print([])
        c2.print(list(range(15, 40)))  # exceeds width -> one item per line
        return c2.export_text()
    elif test_name == "pretty_dict":
        c2 = Console(file=sink, width=80, record=True, legacy_windows=False)
        c2.print({"a": 1, "b": 2})
        c2.print({})  # empty
        c2.print({f"key{i:02d}": i for i in range(12)})  # exceeds width (sorted keys)
        c2.print({"nums": [1, 2], "other": [3, 4]})  # nested inline
        c2.print({"nums": list(range(15, 40)), "other": [1]})  # nested expanded
        return c2.export_text()
    elif test_name == "pretty_scalars":
        c2 = Console(file=sink, width=80, record=True, legacy_windows=False)
        c2.print(42)
        c2.print(1.5)
        c2.print(True)
        c2.print(None)
        c2.print((1, "two"))
        c2.print((42,))  # 1-tuple keeps trailing comma
        c2.print([None, 1, None])  # optionals in sequences
        return c2.export_text()
    elif test_name == "pretty_options":
        from rich.pretty import Pretty
        c2 = Console(file=sink, width=40, record=True, legacy_windows=False)
        c2.print(Pretty([1, 2, 3, 4, 5], max_length=2))
        c2.print(Pretty(["a" * 30, "b"], max_string=5))
        c2.print(Pretty([[[1], [2]], [[3]]], max_depth=1))
        c2.print(Pretty([1, 2, 3, 4, 5, 6, 7, 8], expand_all=True))
        c2.print(Pretty([["x" * 20], ["y" * 16]], indent_guides=True))
        return c2.export_text()
    elif test_name == "json_basic":
        from rich.json import JSON
        c2 = Console(file=sink, width=80, record=True, legacy_windows=False)
        c2.print(JSON('{"a": 1, "b": [true, null, "x"], "c": 2.5}'))
        c2.print(JSON('[1, 2, 3]'))
        c2.print(JSON('{}'))
        c2.print(JSON('{"dup": 1, "dup": 2, "s": "hi\\n\\u0041", "f": -0.5, '
                      '"e": 1e2, "wide": "' + "y" * 90 + '"}'))
        c2.print(JSON('{"u": "\\u6f22\\u5b57", "n": NaN, "inf": Infinity}'))
        return c2.export_text()
    elif test_name == "syntax_basic":
        from rich.syntax import Syntax
        py_code = ("def fib(n):\n"
                   '    """Return the nth Fibonacci number."""\n'
                   "    if n <= 1:\n"
                   "        return n\n"
                   "    a, b = 0, 1\n"
                   "    for _ in range(n - 1):\n"
                   "        a, b = b, a + b\n"
                   "    return a + b\n")
        cpp_code = ("int main() {\n"
                    "    // entry point\n"
                    "    int total = 0;\n"
                    "    for (int i = 1; i <= 10; ++i) {\n"
                    "        total += i;\n"
                    "    }\n"
                    "    return total;\n"
                    "}\n")
        c2 = Console(file=sink, width=40, record=True, legacy_windows=False)
        c2.print(Syntax(py_code, "python"))
        c2.print(Syntax(cpp_code, "cpp", line_numbers=True, start_line=10,
                        highlight_lines={12}))
        c2.print(Syntax("plain text\nwith punctuation, and words like 42\n", "text"))
        c2.print(Syntax(py_code, "python", line_numbers=True, word_wrap=True))
        return c2.export_text()
    elif test_name == "markdown_basic":
        from rich.markdown import Markdown
        doc = (
            "# Heading One\n"
            "\n"
            "Paragraph with **bold**, *italic*, `code`, ~~strike~~ and a\n"
            "[link](https://example.com) plus words that wrap across lines\n"
            "at the console width boundary.\n"
            "\n"
            "## Heading Two\n"
            "\n"
            "- alpha one\n"
            "- beta two\n"
            "  - nested one\n"
            "- gamma three\n"
            "\n"
            "3. first item\n"
            "3. second item\n"
            "\n"
            "> quoted line one\n"
            "> quoted line two\n"
            "\n"
            "```python\n"
            "def f(x):\n"
            "    return x * 2\n"
            "```\n"
        )
        c2 = Console(file=sink, width=60, record=True, legacy_windows=False)
        c2.print(Markdown(doc, hyperlinks=False))
        return c2.export_text()
    elif test_name == "markdown_extras":
        from rich.markdown import Markdown
        doc = (
            "Alpha\n"
            "===\n"
            "\n"
            "Beta\n"
            "---\n"
            "\n"
            "See [docs][x] and [y][] then [z]\n"
            "\n"
            "[x]: https://ex.com/a \"T\"\n"
            "[y]: https://ex.com/b\n"
            "[z]: https://ex.com/c\n"
            "\n"
            "After\n"
            "\n"
            "- [ ] open\n"
            "- [x] done\n"
            "* [X] caps\n"
            "\n"
            "Text[^1] more.\n"
            "\n"
            "[^1]: The definition.\n"
            "\n"
            "| A | B | C |\n"
            "|:--|--:|:-:|\n"
            "| 1 | 2 | 3 |\n"
            "\n"
            "A\n"
            "=== x\n"
            "\n"
            "A\n"
            "-\n"
            "B\n"
            "\n"
            "Fwd [fwd] end\n"
            "\n"
            "[fwd]: https://f.io\n"
            "\n"
            "Edges [e1]( b ) [e2](<a b>) [e3](b [c]) [e4]()\n"
            "\n"
            "1. one\n"
            "2) two\n"
            "\n"
            "- loose\n"
            "\n"
            "  cont\n"
            "\n"
            "A | B\n"
            "-- | --\n"
            "1 | 2\n"
            "\n"
            "| a\\|b | c |\n"
            "|---|---|\n"
            "| 1 | 2 |\n"
        )
        c2 = Console(file=sink, width=60, record=True, legacy_windows=False)
        c2.print(Markdown(doc, hyperlinks=False))
        return c2.export_text()
    elif test_name == "progress_bar_basic":
        # Reference rendering of rich::progress::ProgressBar (green fill /
        # dim empty / cyan percentage) via rich markup, at several fill
        # levels including the 0% and 100% edges.
        def bar(completed, total=100, width=30):
            filled = completed * width // total
            empty = width - filled
            pct = str(completed * 100 // total) + "%"
            if len(pct) < 4:
                pct = " " * (4 - len(pct)) + pct
            return ("[bold green]" + "█" * filled + "[/bold green]"
                    + "[dim bright_black]" + "░" * empty + "[/dim bright_black]"
                    + "[bold bright_cyan] " + pct + "[/bold bright_cyan]")
        c2 = Console(file=sink, width=60, record=True, legacy_windows=False)
        for done in (0, 25, 50, 75, 100):
            c2.print(bar(done))
        return c2.export_text()
    elif test_name == "markup_basic":
        c2 = Console(file=sink, width=60, record=True, legacy_windows=False)
        c2.print("[b]B[/b] [i]I[/i] [u red]U[/] [on yellow]Y[/] [b]n[bold]ested[/][/b] [[bracket]]")
        return c2.export_text()
    elif test_name == "padding_basic":
        from rich.padding import Padding
        c2 = Console(file=sink, width=40, record=True, legacy_windows=False)
        c2.print(Padding(Text("line one here\nline two here"), pad=2))
        c2.print(Padding(Text("pipe line\nsecond line"), pad=1))
        return c2.export_text()
    elif test_name == "track_basic":
        from rich.progress import Progress
        prog = Progress(console=c, auto_refresh=False, get_time=lambda: 100.0)
        with prog:
            t = prog.add_task("Processing", total=8)
            for _ in range(8):
                prog.advance(t)
        return c.export_text()
    elif test_name == "style_sgr":
        # Raw SGR stream: forced terminal, truecolor, no_color off. Compare
        # sink bytes directly (not export_text): one print per line.
        sink2 = io.StringIO()
        c2 = Console(file=sink2, width=60, height=25, force_terminal=True,
                     color_system="truecolor", no_color=False,
                     legacy_windows=False)
        c2.print("[red]a[/][bright_red]b[/]")
        c2.print("[color(196)]a[/][on color(27)]b[/]")
        c2.print("[#ff8800]a[/][on #00ff88]b[/]")
        c2.print("[rgb(200,100,50)]a[/][on rgb(10,20,30)]b[/]")
        c2.print("[bold strike reverse]a[/]")
        c2.print("[blink blink2 conceal underline2 frame encircle overline]a[/]")
        c2.print("[default]a[/][on default]b[/]")
        c2.print("[b]alias[/bold]")
        return sink2.getvalue()
    elif test_name == "style_sgr_256":
        # Same raw-SGR compare under the 256-color (EightBit) system: truecolor
        # colors are downgraded (colorsys HLS + cube), color(N) passes, the
        # gray branch hits index 241.
        sink2 = io.StringIO()
        c2 = Console(file=sink2, width=60, force_terminal=True,
                     color_system="256", no_color=False,
                     legacy_windows=False)
        c2.print("[rgb(200,100,50)]a[/]")
        c2.print("[color(196)]b[/]")
        c2.print("[red]c[/]")
        c2.print("[on rgb(200,100,50)]d[/]")
        c2.print("[rgb(100,100,100)]e[/]")
        return sink2.getvalue()
    elif test_name == "log_basic":
        # Console.log with an injected clock (get_datetime) is fully
        # deterministic: time column, omitted repeated time, styled message,
        # fold at the message width, empty log, and the auto-resolved
        # caller path column (oracle_gen.py:<line>).
        from datetime import datetime
        sink2 = io.StringIO()
        c2 = Console(file=sink2, width=60, record=True, legacy_windows=False,
                     get_datetime=lambda: datetime(2026, 7, 22, 21, 53, 33))
        c2.log("hello world", 42)
        c2.log("hello world", 42)
        c2.log("[red]styled[/] message")
        c2.log("a" * 50)
        c2.log()
        return sink2.getvalue()
    elif test_name == "log_sgr":
        # Raw SGR bytes for the log grid: time cell is dim+cyan (two runs:
        # text + pad space), repeated time is blanked but stays styled.
        # log_path=False: with a path, Python wraps the path cell in an OSC 8
        # hyperlink whose id is a non-deterministic counter, so the path is
        # covered by log_basic (export_text strips links) instead.
        from datetime import datetime
        sink2 = io.StringIO()
        c2 = Console(file=sink2, width=60, height=25, force_terminal=True,
                     color_system="truecolor", no_color=False, log_path=False,
                     legacy_windows=False,
                     get_datetime=lambda: datetime(2026, 7, 22, 21, 53, 33))
        c2.log("[red]hello[/] world")
        c2.log("again")
        return sink2.getvalue()
    elif test_name == "theme_push":
        # Theme system: default theme (log.time = dim cyan, rule.line =
        # bright green); push_theme (inherit) overlays names on the stack
        # top; pop_theme restores. get_datetime advances one second per
        # call so omit_repeated_times does not blank the styled time column.
        from datetime import datetime, timedelta
        from rich.theme import Theme
        tick = [datetime(2026, 7, 22, 21, 53, 32)]
        def dt():
            tick[0] += timedelta(seconds=1)
            return tick[0]
        sink2 = io.StringIO()
        c2 = Console(file=sink2, width=60, height=25, force_terminal=True,
                     color_system="truecolor", no_color=False, log_path=False,
                     legacy_windows=False, get_datetime=dt)
        c2.log("default")
        c2.rule("sep")
        c2.push_theme(Theme({"log.time": "bold red", "rule.line": "bright_blue"}))
        c2.log("themed")
        c2.rule("sep")
        c2.pop_theme()
        c2.log("restored")
        c2.rule("sep")
        return sink2.getvalue()
    elif test_name == "capture_basic":
        # Capture: prints inside the scope are diverted to cap.get() — not
        # written to the file and not recorded. Second half proves the
        # captured render carries the console's SGR (force_terminal).
        sink3 = io.StringIO()
        c3 = Console(file=sink3, width=60, record=True, legacy_windows=False)
        with c3.capture() as cap:
            c3.print("[bold]captured[/]")
            c3.print("plain")
        out = cap.get() + "file[" + sink3.getvalue() + "]rec[" + c3.export_text() + "]"
        c3.print("after")
        out += c3.export_text()
        sink4 = io.StringIO()
        c4 = Console(file=sink4, width=60, height=25, force_terminal=True,
                     color_system="truecolor", no_color=False, legacy_windows=False)
        with c4.capture() as cap4:
            c4.print("[bold]sgr[/]")
        return out + "sgr[" + cap4.get() + "]"
    elif test_name == "screen_basic":
        # Screen: shaped to width x console height — short lines padded to
        # width, tall content cropped, no trailing newline.
        sink5 = io.StringIO()
        c5 = Console(file=sink5, width=20, height=5, record=True, legacy_windows=False)
        c5.print(Screen(Panel(Text("Hi"), width=20)))
        c5.print(Screen(Text("l1\nl2\nl3\nl4\nl5\nl6\nl7")))
        return c5.export_text()
    elif test_name == "control_basic":
        # Control codes: bytes land in the file only for terminals
        # (force_terminal); export_text() filters control segments by
        # default, so compare the stream bytes. The Console.control() path
        # is no-op on dumb terminals — override TERM via _environ.
        import os
        from rich.control import Control, escape_control_codes, strip_control_codes
        from rich.segment import ControlType
        sink6 = io.StringIO()
        c6 = Console(file=sink6, width=60, height=25, force_terminal=True,
                     record=True, no_color=False, legacy_windows=False)
        c6.print(Control.bell())
        c6.print(Control.home())
        c6.print(Control.move(2, 3))
        c6.print(Control.move(-1, -2))
        c6.print(Control.move_to(4, 5))
        c6.print(Control.move_to_column(7))
        c6.print(Control.move_to_column(7, 2))
        c6.print(Control.clear())
        c6.print(Control.show_cursor(False))
        c6.print(Control.alt_screen(True))
        c6.print(Control.title("hello"))
        c6.print(Control((ControlType.ERASE_IN_LINE, 2)))
        part1 = sink6.getvalue()
        env = dict(os.environ)
        env["TERM"] = "xterm-256color"
        sink7 = io.StringIO()
        c7 = Console(file=sink7, width=60, height=25, force_terminal=True,
                     record=True, no_color=False, legacy_windows=False, _environ=env)
        c7.control(Control.move(1, 1))
        c7.control(Control.home())
        part2 = sink7.getvalue()
        part3 = (
            "esc:" + escape_control_codes("\x07\x08\x0b\x0c\r")
            + "|st:" + strip_control_codes("a\x07b\x08c") + "|"
        )
        return part1 + "SEP" + part2 + "SEP" + part3
    elif test_name == "colorfile_basic":
        # rich._palettes + rich.color (ANSI_COLOR_NAMES, rich 15.0.0 — no
        # colorfile.py in this release): Palette.match() indices, then raw
        # SGR proving the 235-entry name table ("grey23" -> 237) end-to-end.
        from rich._palettes import (
            EIGHT_BIT_PALETTE,
            STANDARD_PALETTE,
            WINDOWS_PALETTE,
        )
        parts = []
        for r, g, b in [
            (12, 34, 56),
            (237, 1, 2),
            (0, 0, 0),
            (255, 255, 255),
            (127, 127, 127),
        ]:
            parts.append(str(EIGHT_BIT_PALETTE.match((r, g, b))))
        for r, g, b in [(200, 100, 50), (0, 0, 0), (255, 255, 255)]:
            parts.append(str(STANDARD_PALETTE.match((r, g, b))))
        for r, g, b in [(12, 34, 56), (237, 1, 2), (100, 200, 100)]:
            parts.append(str(WINDOWS_PALETTE.match((r, g, b))))
        parts.append("SEP")
        sink2 = io.StringIO()
        c2 = Console(file=sink2, width=60, height=25, force_terminal=True,
                     color_system="truecolor", no_color=False,
                     legacy_windows=False)
        c2.print("[grey23]x[/][on dark_red]y[/]")
        c2.print("[deep_pink3]a[/][bright_red]b[/]")
        return "".join(parts) + sink2.getvalue()
    elif test_name == "fileproxy_basic":
        # rich.file_proxy.FileProxy + rich._null_file.NullFile: partial-line
        # buffering, ANSI-decoded complete lines, raw-string flush.
        from rich._null_file import NULL_FILE
        from rich.file_proxy import FileProxy
        c_fp = Console(file=io.StringIO(), width=60, record=True,
                       legacy_windows=False)
        proxy = FileProxy(c_fp, io.StringIO())
        proxy.write("partial")
        proxy.write(" line1\n")
        proxy.write("\x1b[1mstyled\x1b[0m and \x1b[31mred\x1b[0m line2\n")
        proxy.write("two\nlines\nin one write")
        proxy.flush()
        out = c_fp.export_text()
        out += (
            "NF:"
            + str(NULL_FILE.fileno())
            + ","
            + str(NULL_FILE.write("x"))
            + ","
            + str(len(NULL_FILE.readlines()))
            + ","
            + str(NULL_FILE.tell())
            + ","
            + str(int(NULL_FILE.isatty()))
            + ","
            + str(int(NULL_FILE.writable()))
            + ","
            + str(int(NULL_FILE.readable()))
            + ","
            + str(NULL_FILE.seek(123))
        )
        return out
    elif test_name == "from_ansi_basic":
        # rich.text.Text.from_ansi (AnsiDecoder): SGR state machine,
        # 256/truecolor colors, lenient invalid codes, CR truncation,
        # multi-line join under a base style.
        t = Text.from_ansi(
            "\x1b[1mbold\x1b[0m normal\n"
            "\x1b[38;5;196mred8\x1b[38;2;10;20;30mrgb\x1b[0m\n"
            "\x1b[48;5;237;99mgarbage\x1b[31;33m last\n"
            "over\r written\n"
            "plain",
            style="italic",
        )
        c3 = Console(file=io.StringIO(), width=60, record=True,
                     legacy_windows=False)
        c3.print(t)
        return c3.export_text()
    elif test_name == "highlighter_basic":
        # Raw SGR stream (force_terminal truecolor): the built-in
        # highlighters. print() flattens Text (base style dropped), so the
        # visible output is spans-only — compare sink bytes directly.
        from rich.highlighter import (
            ReprHighlighter,
            JSONHighlighter,
            ISO8601Highlighter,
            NullHighlighter,
        )
        sink2 = io.StringIO()
        c2 = Console(file=sink2, width=60, height=25, force_terminal=True,
                     color_system="truecolor", no_color=False,
                     legacy_windows=False)
        c2.print(ReprHighlighter()(
            "{'name': 'apple', 'count': 1, 'ok': True, 'n': None}"))
        c2.print(ReprHighlighter()("[1, 2.5, -3, 1.5e10, 0x1F, 2+3j]"))
        c2.print(ReprHighlighter()(
            "127.0.0.1 2001:db8::1 /foo/bar.py https://ex.org/x?a=1"))
        c2.print(JSONHighlighter()(
            '{"name": "apple", "count": 1, "flag": true, "z": null, '
            '"nested": {"a": [1, 2]}}'))
        c2.print(ISO8601Highlighter()("2008-08-30 17:21:59"))
        c2.print(ISO8601Highlighter()("2008-08-30T01:45:36.123Z"))
        c2.print(ISO8601Highlighter()("2008-W35-6"))
        c2.print(NullHighlighter()("nothing here 123"))
        return sink2.getvalue()
    elif test_name == "text_extras_basic":
        # Full Text semantics (base style, justify, overflow HONORED):
        # console.print flattens Text through Text(" ").join([t]), so these
        # go through render_lines + _render_buffer instead — the same
        # pipeline a container (Panel/Group) uses.
        def show(t, w):
            nonlocal te_out
            c2 = Console(file=io.StringIO(), width=w, height=25,
                         force_terminal=True, color_system="truecolor",
                         no_color=False, legacy_windows=False)
            lines = c2.render_lines(t, c2.options.update(width=w), pad=False)
            te_out += "".join(c2._render_buffer(line) + "\n" for line in lines)

        te_out = ""
        show(Text("the quick brown fox jumps over the lazy dog",
                  justify="full"), 40)
        t = Text("the quick brown fox jumps over the lazy dog",
                 justify="full")
        t.stylize("bold", 0, 20)
        show(t, 40)
        show(Text("the quick brown fox jumps over the lazy dog",
                  style="red", justify="full"), 40)
        t = Text("hi", justify="center")
        t.stylize("bold", 0, 2)
        show(t, 10)
        t = Text("hi", justify="right")
        t.stylize("italic", 0, 2)
        show(t, 10)
        t = Text("hi", justify="left")
        t.stylize("underline", 1, 2)
        show(t, 10)
        t = Text("hi", style="red")
        t.stylize("bold", 0, 1)
        show(t, 10)
        t = Text("helloxxxxxxxxxxxxx world", overflow="ellipsis")
        t.stylize("red", 0, 5)
        show(t, 12)
        t = Text("helloxxxxxxxxxxxxx world", overflow="crop")
        t.stylize("red", 0, 5)
        show(t, 12)
        t = Text("xxxxxxxxxxxxxxxxxxxxx yyy", overflow="fold")
        t.stylize("blue", 0, 5)
        show(t, 10)
        t = Text("ab\ncd")
        t.stylize("bold", 0, 4)
        show(t, 20)
        t = Text("a\tbb")
        t.stylize("bold", 1, 3)
        show(t, 20)
        show(Text("a\tbb"), 20)
        show(Text.assemble(("he", "bold"), ("llo", "italic"), " world"), 20)
        show(Text.styled("abc", "bold red"), 20)
        t = Text("apple banana")
        t.highlight_regex(r"(?P<f>\w+) (?P<s>\w+)", "green",
                         style_prefix="t.")
        show(t, 20)
        t = Text("abc   \n  def  ")
        t.stylize("bold", 0, 3)
        show(t, 20)
        show(Text("the  quick fox", justify="full"), 20)
        return te_out
    elif test_name == "table_extras":
        # show_footer / caption / row_styles (geometry) / expand /
        # add_section. Box pinned to box.ROUNDED on both sides (Python 15.0.0
        # default is HEAVY_HEAD; the C++ default stays box.ROUNDED by design).
        t = Table(box=box.ROUNDED, show_footer=True)
        t.add_column("Name", footer="nm")
        t.add_column("Age", footer="ag", justify="right")
        t.add_row("Alice", "30")
        t.add_row("Bob", "25")
        c.print(t)
        t = Table(box=box.ROUNDED)
        t.add_column("A")
        t.add_column("B")
        t.add_row("1", "2")
        t.add_row("3", "4")
        t.caption = "My Caption"
        c.print(t)
        t = Table(box=box.ROUNDED, row_styles=["dim", "bold"])
        t.add_column("A")
        t.add_column("B")
        t.add_row("1", "2")
        t.add_row("3", "4")
        t.add_row("5", "6")
        c.print(t)
        t = Table(box=box.ROUNDED, expand=True)
        t.add_column("A")
        t.add_column("B")
        t.add_row("1", "2")
        c.print(t)
        t = Table(box=box.ROUNDED)
        t.add_column("A")
        t.add_column("B")
        t.add_row("1", "2")
        t.add_section()
        t.add_row("3", "4")
        c.print(t)
        t = Table(box=box.SQUARE, expand=True)
        t.add_column("A")
        t.add_column("B")
        t.add_column("C")
        t.add_row("x", "y", "z")
        c.print(t)
        return c.export_text()
    elif test_name == "box_variants":
        # All 19 box variants of rich 15.0.0, one table each: header row
        # (head_* edges), head_row separator, mid data rows, row separator
        # (section), foot_row separator + foot_* footer row, bottom. The
        # MINIMAL*/SIMPLE*/HEAVY_HEAD/EDGE/DOMAIN variants differ across
        # those glyph rows, so this pins every field of every box.
        for n in ["ASCII", "ASCII2", "ASCII_DOUBLE_HEAD", "SQUARE",
                  "SQUARE_DOUBLE_HEAD", "MINIMAL", "MINIMAL_HEAVY_HEAD",
                  "MINIMAL_DOUBLE_HEAD", "SIMPLE", "SIMPLE_HEAD",
                  "SIMPLE_HEAVY", "HORIZONTALS", "ROUNDED", "HEAVY",
                  "HEAVY_EDGE", "HEAVY_HEAD", "DOUBLE", "DOUBLE_EDGE",
                  "MARKDOWN"]:
            t = Table(box=getattr(box, n), show_footer=True)
            t.add_column("Name", footer="nm")
            t.add_column("Age", footer="ag", justify="right")
            t.add_row("Alice", "30")
            t.add_section()
            t.add_row("Bob", "25")
            c.print(t)
        return c.export_text()
    elif test_name == "panel_extras":
        # subtitle / style / border_style / expand / fit / height — raw SGR
        # (styles are byte-visible; geometry covers expand/fit/height).
        sink = io.StringIO()
        c = Console(file=sink, width=60, height=25, record=False,
                    legacy_windows=False, force_terminal=True,
                    color_system="truecolor", no_color=False)
        c.print(Panel("Hello World", style="red"))
        c.print(Panel("Content here", subtitle="Subtitle", style="dim"))
        c.print(Panel.fit("Short"))
        c.print(Panel.fit("Shorter content", title="FitTitle"))
        c.print(Panel("X", width=30, subtitle="Sub"))
        c.print(Panel("Line", height=6, title="H"))
        c.print(Panel("A", style="bold", border_style="red"))
        c.print(Panel("No Style Plain"))
        return sink.getvalue()
    elif test_name == "rule_extras":
        # characters / style / align — raw SGR (dash style is byte-visible).
        from rich.rule import Rule as RuleR
        sink = io.StringIO()
        c = Console(file=sink, width=60, height=25, record=False,
                    legacy_windows=False, force_terminal=True,
                    color_system="truecolor", no_color=False)
        c.print(RuleR("My Rule"))
        c.print(RuleR("X", characters="*"))
        c.print(RuleR("X", characters="\u2501"))
        c.print(RuleR("X", style="red"))
        c.print(RuleR("X", style="bold"))
        c.print(RuleR("X", align="left"))
        c.print(RuleR("X", align="right"))
        c.print(RuleR(characters="*"))
        c.print(RuleR("A" * 50))
        c.print(RuleR("B" * 50, align="left"))
        c.print(RuleR("C" * 50, align="right"))
        c.print(RuleR("[bold]Bold[/]"))
        c.print(RuleR("Mix", characters="--", style="green"))
        return sink.getvalue()
    elif test_name == "tree_extras":
        # guide_style / expanded / hide_root — raw SGR (guide colors are
        # byte-visible; theme tree/tree.line are null on both sides).
        sink = io.StringIO()
        c = Console(file=sink, width=60, height=25, record=False,
                    legacy_windows=False, force_terminal=True,
                    color_system="truecolor", no_color=False)
        t = Tree("root")
        a = t.add("alpha")
        a.add("alpha-1")
        a.add("alpha-2")
        t.add("beta")
        t.add("gamma")
        c.print(t)
        t = Tree("root", guide_style="red")
        t.add("a")
        t.add("b")
        c.print(t)
        t = Tree("root", guide_style="bold")
        t.add("a").add("a-1")
        t.add("b")
        c.print(t)
        t = Tree("root")
        a = t.add("alpha", expanded=False)
        a.add("hidden-1")
        a.add("hidden-2")
        t.add("beta")
        c.print(t)
        t = Tree("root", hide_root=True)
        t.add("a")
        t.add("b")
        c.print(t)
        t = Tree("[bold]root[/]", style="green")
        c1 = t.add("child", guide_style="magenta")
        c1.add("grand")
        t.add("leaf")
        c.print(t)
        t = Tree("only")
        t.add("kid")
        c.print(t)
        return sink.getvalue()
    elif test_name == "columns_extras":
        # expand / equal / column_first / right_to_left / width / title /
        # padding — raw SGR (str items pass through console.render_str, so
        # the 18-digit items are auto-highlighted by the default
        # ReprHighlighter — byte-identical to the C++ ReprHighlighter).
        sink = io.StringIO()
        c = Console(file=sink, width=60, height=25, record=False,
                    legacy_windows=False, force_terminal=True,
                    color_system="truecolor", no_color=False)
        c.print(Columns(["aaaa", "bb", "cccccccc"]))
        c.print(Columns(["aaaa", "bb", "cccccccc"], expand=True))
        c.print(Columns(["aaaa", "bb", "cccccccc"], equal=True))
        c.print(Columns(["i1", "i2", "i3", "i4", "i5", "i6"], column_first=True))
        c.print(Columns(["i1", "i2", "i3", "i4", "i5", "i6"], column_first=True,
                        expand=True))
        c.print(Columns([f"{n}" * 18 for n in range(1, 6)]))
        c.print(Columns(["aaaa", "bb", "cccccccc"], align="right"))
        c.print(Columns(["a" * 12, "b" * 12, "c" * 12, "d" * 12, "e" * 12,
                         "f" * 12, "gg", "hh"], align="right"))
        c.print(Columns(["a" * 12, "b" * 12, "c" * 12, "d" * 12, "e" * 12,
                         "f" * 12, "gg", "hh"], align="center"))
        c.print(Columns(["aaaa", "bb", "cccccccc"], title="Titled"))
        c.print(Columns(["aaaa", "bb", "cccccccc"], width=6))
        c.print(Columns(["aaaa", "bb", "cccccccc"], right_to_left=True))
        c.print(Columns(["aaaa", "bb", "cccccccc"], padding=(1, 2)))
        c.print(Columns(["aaaa", "bb", "cccccccc"], padding=(1, 2), expand=True))
        c.print(Columns([Text("aaaa", style="red"),
                         Text("bb", style="bold green"),
                         Text("cccccccc", style="cyan")]))
        return sink.getvalue()
    elif test_name == "progress_extras":
        # Progress custom columns + pulse + transfer mode. Frozen clocks keep
        # every value deterministic: spinner frame 0, pulse offset fixed,
        # speed samples exact. no_color=False is explicit so the pulse renders
        # all-bar segments (Python: color_system None + no_color False ->
        # " " if no_color else bar => bars on both halves).
        from rich.progress import (
            Progress,
            SpinnerColumn,
            TextColumn,
            BarColumn,
            TaskProgressColumn,
            TimeElapsedColumn,
            TimeRemainingColumn,
            FileSizeColumn,
            TotalFileSizeColumn,
            MofNCompleteColumn,
            DownloadColumn,
            TransferSpeedColumn,
        )
        pc = Console(file=io.StringIO(), width=60, record=True,
                     legacy_windows=False, no_color=False)

        # (a) default columns, frozen clock: no speed/ETA under one sample.
        clock = [100.0]
        prog = Progress(console=pc, auto_refresh=False, get_time=lambda: clock[0])
        t1 = prog.add_task("Downloading", total=100)
        t2 = prog.add_task("Processing", total=200)
        with prog:
            prog.update(t1, completed=42)
            prog.update(t2, completed=137)

        # (b) rolling speed: samples (105, 42), (110, 42) -> 8.4 it/s ->
        # ETA ceil(16 / 8.4) = 2 -> "0:00:02".
        clock = [100.0]
        prog = Progress(console=pc, auto_refresh=False, get_time=lambda: clock[0])
        t1 = prog.add_task("Downloading", total=100)
        t2 = prog.add_task("Processing", total=200)
        with prog:
            clock[0] = 105.0
            prog.update(t1, completed=42)
            prog.update(t2, completed=137)
            clock[0] = 110.0
            prog.update(t1, completed=84)

        # (c) pulse: unstarted task (pulses until started) and total=None.
        clock = [0.0]
        prog = Progress(console=pc, auto_refresh=False, get_time=lambda: clock[0])
        t1 = prog.add_task("Waiting", total=None, start=False)
        t2 = prog.add_task("Started", total=50)
        with prog:
            prog.update(t2, completed=10)
            clock[0] = 3.5

        # (d) transfer columns: spinner + description + bar + percentage +
        # filesize + transfer speed + ETA (single sample -> "?" speed,
        # "-:--:--" ETA).
        clock = [0.0]
        prog = Progress(
            SpinnerColumn(),
            TextColumn("[progress.description]{task.description}"),
            BarColumn(),
            TaskProgressColumn(),
            FileSizeColumn(),
            TransferSpeedColumn(),
            TimeRemainingColumn(),
            console=pc,
            auto_refresh=False,
            get_time=lambda: clock[0],
        )
        t = prog.add_task("Downloading file", total=1500000)
        with prog:
            clock[0] = 60.0
            prog.update(t, completed=750000)

        # (e) remaining column types: str description column, elapsed, total
        # filesize, M-of-N, download, compact ETA with elapsed-when-finished;
        # expand=True. Task 2 finishes via advance (finished_time/speed set).
        clock = [0.0]
        prog = Progress(
            "{task.description}",
            TimeElapsedColumn(),
            TotalFileSizeColumn(),
            MofNCompleteColumn(),
            DownloadColumn(),
            TimeRemainingColumn(compact=True, elapsed_when_finished=True),
            expand=True,
            console=pc,
            auto_refresh=False,
            get_time=lambda: clock[0],
        )
        t1 = prog.add_task("Copy", total=2500000)
        t2 = prog.add_task("Done", total=8)
        with prog:
            clock[0] = 30.0
            prog.update(t1, completed=1250000)
            prog.advance(t2, 8)

        return pc.export_text()
    elif test_name == "spinner_registry":
        # Spinner registry: frame selection math (start_time fix on first
        # render -> frame 0; second render at a known delta), ctor speed,
        # attached text, deferred update(speed), custom frames/interval.
        # Spinner.render is exercised directly (console.get_time() is
        # wall-clock and not oracle-able).
        from rich.spinner import Spinner
        sink2 = io.StringIO()

        def show(sp, t):
            sink2.write(sp.render(t).plain + "\n")

        sp = Spinner("dots")
        sp.render(0.0)
        show(sp, 0.24)
        sp = Spinner("line")
        sp.render(0.0)
        show(sp, 0.39)
        sp = Spinner("dots12")
        sp.render(0.0)
        show(sp, 0.80)
        sp = Spinner("weather")
        sp.render(0.0)
        show(sp, 0.75)
        sp = Spinner("material")
        sp.render(0.0)
        show(sp, 1.68)
        sp = Spinner("point")
        sp.render(0.0)
        show(sp, 0.20)

        sp = Spinner("dots", speed=2.0)
        sp.render(0.0)
        show(sp, 0.24)

        sp = Spinner("line", text="loading...")
        sp.render(0.0)
        show(sp, 0.26)

        sp = Spinner("dots")
        sp.render(0.0)
        sp.update(speed=4.0)
        show(sp, 0.08)
        show(sp, 0.16)

        sp = Spinner("dots")
        sp.frames = ["a", "b", "c"]
        sp.interval = 100
        sp.render(0.0)
        show(sp, 0.1)
        show(sp, 0.2)

        return sink2.getvalue()
    elif test_name == "live_extras":
        # Non-interactive console: Live emits exactly ONE final frame on stop
        # (the render hook appends LiveRender to the stop-time print(Control()),
        # which carries no end of its own -> no trailing newline). transient /
        # screen (screen forces transient) emit nothing. set_alt_screen writes
        # no codes on a non-terminal, so screen=="" output.
        with Live("hello", console=c, auto_refresh=False):
            pass
        c.line()

        with Live("one", console=c, auto_refresh=False) as live:
            live.update("two")
        c.line()

        with Live("gone", console=c, auto_refresh=False, transient=True):
            pass
        c.line()

        with Live("screen", console=c, auto_refresh=False, screen=True):
            pass
        c.line()

        # vertical_overflow via the LiveRender renderable surface (deterministic
        # on a height=25 record console).
        from rich.live_render import LiveRender

        big = "\n".join(f"L{i:02d}" for i in range(30))
        c.print(LiveRender(big, vertical_overflow="crop"))
        c.line()
        c.print(LiveRender(big, vertical_overflow="ellipsis"))
        c.line()
        c.print(LiveRender(big, vertical_overflow="visible"))
        c.line()
        c.print(LiveRender(Text("short"), vertical_overflow="crop"))
        c.line()

        # get_renderable callable (same final-frame path as update())
        state = {"v": "first"}
        with Live(console=c, auto_refresh=False, get_renderable=lambda: state["v"]):
            pass
        state["v"] = "second"
        with Live(console=c, auto_refresh=False, get_renderable=lambda: state["v"]):
            pass
        c.line()
        return c.export_text()
    elif test_name == "pretty_sgr":
        # Raw SGR stream: Pretty's ReprHighlighter coloring — repr.brace bold,
        # repr.number bold+cyan, repr.str green, repr.bool_true/false
        # italic+bright, repr.none italic+magenta, "..." ellipsis yellow,
        # repr.indent green dim guides.
        from rich.pretty import Pretty

        sink2 = io.StringIO()
        c2 = Console(file=sink2, width=60, height=25, force_terminal=True,
                     color_system="truecolor", no_color=False,
                     legacy_windows=False)
        c2.print((1, "two", True, None))
        c2.print({"a": 1, "b": 2})
        c2.print(list(range(12, 27)))
        c2.print(Pretty([1, 2, 3, 4, 5, 6], max_length=3))
        c2.print(Pretty([("a" * 8, 1), ("b" * 8, 2)], indent_guides=True))
        c2.print((0.25, -2.5e3, "end"))
        c2.print(Pretty({"k": {"deep": [0.25, None]}}, expand_all=True))
        return sink2.getvalue()
    elif test_name == "syntax_extras":
        # Raw SGR stream: pygments engine (69 generated lexers + native JSON)
        # with generated themes, truecolor token styles, blended line-number
        # styles, and the full option surface (word_wrap, tab_size,
        # line_range, highlight_lines, code_width, indent_guides, dedent,
        # background_color, unknown-lexer fallback).
        from rich.syntax import Syntax

        sink2 = io.StringIO()
        c2 = Console(file=sink2, width=60, height=25, force_terminal=True,
                     color_system="truecolor", no_color=False,
                     legacy_windows=False)
        py = ("def f(x):\n"
              '    """doc."""\n'
              "    return x + 1\n")
        c2.print(Syntax(py, "python"))
        c2.print(Syntax("const x = 42;\n// c\nconsole.log(x);\n", "javascript"))
        c2.print(Syntax('{"a": [1, 2.5, true, null], "b": "s"}\n', "json"))
        c2.print(Syntax("#!/bin/bash\necho hi\nexport P=1\n", "bash"))
        c2.print(Syntax("SELECT id, name FROM t WHERE id = 5;\n", "sql"))
        c2.print(Syntax("func main() {\n\tfmt.Println(1)\n}\n", "go"))
        c2.print(Syntax("fn main() {\n    let x = 1;\n}\n", "rust"))
        c2.print(Syntax("-- c\nlocal x = 1\nprint(x)\n", "lua"))
        c2.print(Syntax("cmake_minimum_required(VERSION 3.0)\nadd_executable(a m.c)\n", "cmake"))
        c2.print(Syntax("# T\n\nsome **b**\n", "markdown"))
        c2.print(Syntax("[sec]\nkey = value\n", "ini"))
        c2.print(Syntax("# t\n[table]\nx = 1\n", "toml"))
        c2.print(Syntax("\\section{T}\nSome $x^2$ math.\n", "tex"))
        for theme in ("monokai", "dracula", "solarized-dark",
                      "github-dark", "nord", "tango"):
            c2.print(Syntax(py, "python", theme=theme))
        c2.print(Syntax(py, "python", line_numbers=True, start_line=10,
                        highlight_lines={12}))
        c2.print(Syntax("int main() { return very_long_line_value + another + third;\n}\n",
                        "cpp", word_wrap=True))
        c2.print(Syntax("func main() {\n\tfmt.Println(1)\n}\n", "go", tab_size=8))
        c2.print(Syntax(py, "python", line_range=(2, 4)))
        c2.print(Syntax(py, "python", code_width=30))
        c2.print(Syntax(py, "python", indent_guides=True))
        c2.print(Syntax("    a = 1\n    b = 2\n\n    c = 3\n", "python", dedent=True))
        c2.print(Syntax(py, "python", background_color="#1e2e3e"))
        c2.print(Syntax("hello world\n", "nosuchlexer"))
        return sink2.getvalue()
    elif test_name == "number_extras":
        # rich.filesize.decimal (SI), _to_str with the IEC list (binary),
        # and pick_unit_and_suffix — all deterministic string output,
        # including digit grouping, precision/separator variants, and the
        # int64-edge sizes where the break unit exceeds int64 (10^21).
        from rich.filesize import decimal, _to_str, pick_unit_and_suffix
        IEC = ("KiB", "MiB", "GiB", "TiB", "PiB", "EiB", "ZiB", "YiB")
        out = []
        for s in [0, 1, 2, 999, 1000, 1024, 9999, 10000, 123456, 30000,
                  999999999, 10**9, 10**9 + 123456789, 10**12, 10**15,
                  10**18, 10**18 + 123456, 2560 * 1000, 750 * 1000 * 1000,
                  123456789012345, 9223372036854775807]:
            out.append(decimal(s))
        for s in [30000, 1234567, 12345678901, 999999999999999999,
                  9223372036854775807]:
            out.append(decimal(s, precision=0))
            out.append(decimal(s, precision=2, separator=""))
            out.append(decimal(s, precision=3, separator="  "))
        for s in [0, 1, 2, 1023, 1024, 1536, 1048576, 1234567890,
                  999999999999999999, 9223372036854775807]:
            out.append(_to_str(s, IEC, 1024))
        out.append(_to_str(1234567, IEC, 1024, precision=2, separator=""))
        out.append(_to_str(999999999999999999, IEC, 1024, precision=0))
        suf = ["", "kB", "MB", "GB", "TB", "PB", "EB"]
        for s in [0, 1, 999, 1000, 1001, 1000000, 1000005, 1000000000,
                  1000000000001, 10**18]:
            unit, suffix = pick_unit_and_suffix(s, suf, 1000)
            out.append(f"{unit} {suffix}")
        return "\n".join(out) + "\n"
    return c.export_text()

def main():
    if len(sys.argv) < 2:
        print("Usage: python oracle_gen.py <test_name>")
        sys.exit(1)
    test_name = sys.argv[1]
    output = generate_python_oracle(test_name)
    sys.stdout.buffer.write(output.encode("utf-8"))

if __name__ == "__main__":
    main()
