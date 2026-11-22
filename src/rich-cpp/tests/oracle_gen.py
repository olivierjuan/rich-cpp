import io
import sys
from rich.console import Console
from rich.panel import Panel
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
