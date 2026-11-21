import io, sys
from rich.console import Console
from rich.live import Live
from rich.panel import Panel
from rich.text import Text

# Capture raw bytes from a pipe (non-TTY) to see what Live emits.
sink = io.StringIO()
c = Console(file=sink, width=60, record=False, legacy_windows=False)
p = Panel(Text("Hello World\nLine 2"), width=24, title="Live")
live = Live(p, console=c, auto_refresh=True, refresh_per_second=100)
with live:
    pass
out = sink.getvalue()
print(repr(out))
