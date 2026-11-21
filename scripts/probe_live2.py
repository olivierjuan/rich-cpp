import io, sys
from rich.console import Console
from rich.live import Live
from rich.panel import Panel
from rich.text import Text

# Force interactive mode so Live emits control codes to the pipe.
sink = io.StringIO()
c = Console(file=sink, width=60, record=False, legacy_windows=False, force_terminal=True)
p = Panel(Text("Hello World\nLine 2"), width=24, title="Live")
live = Live(p, console=c, auto_refresh=True, refresh_per_second=100)
with live:
    pass
out = sink.getvalue()
print(repr(out))
