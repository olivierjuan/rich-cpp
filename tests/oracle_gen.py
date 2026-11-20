import io
import sys
from rich.console import Console
from rich.panel import Panel
from rich.table import Table
from rich.columns import Columns
from rich import box

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
