import io
from rich.console import Console
from rich.progress import Progress

def gen(cases, t0, updates):
    # cases: list of (desc, total); updates: list of (id, completed, time)
    sink = io.StringIO()
    c = Console(file=sink, width=60, record=True, legacy_windows=False)
    prog = Progress(console=c, auto_refresh=False, get_time=lambda: 100.0)
    ids = []
    for d, t in cases:
        ids.append(prog.add_task(d, total=t))
    with prog:
        for i, comp, tm in updates:
            prog.get_time = lambda tm=tm: tm
            prog.update(ids[i], completed=comp)
    return c.export_text()

# Two updates on the same task at different times -> rolling window speed
print(repr(gen([("Downloading", 100)], 0, [(0, 42, 100.0), (0, 84, 105.0)])))
print("---")
# Single update, elapsed 5s, 42 done -> speed 8.4 it/s, ETA = ceil(58/8.4)=7
print(repr(gen([("Downloading", 100)], 0, [(0, 42, 105.0)])))
print("---")
# Finished task shows elapsed when finished? default TimeRemainingColumn has
# elapsed_when_finished=False, so finished shows time_remaining=0 -> "0:00:00"
print(repr(gen([("Downloading", 100)], 0, [(0, 100, 103.0)])))
print("---")
# Speed formatting: 1234 it/s -> "1.2×10³ it/s"
print(repr(gen([("Downloading", 10000)], 0, [(0, 1234, 101.0)])))
print("---")
# Unknown total: no bar, no percent, no ETA
prog_sink = io.StringIO()
c = Console(file=prog_sink, width=60, record=True, legacy_windows=False)
prog = Progress(console=c, auto_refresh=False, get_time=lambda: 100.0)
t = prog.add_task("Indeterminate", total=None)
with prog:
    prog.update(t, completed=5)
print(repr(c.export_text()))
