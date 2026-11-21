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
