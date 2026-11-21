import io
from rich.console import Console
from rich.progress import Progress

def gen(cases, updates):
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

# Single update at t=105 (start at 100): elapsed 5s, 42 done -> speed 8.4 it/s
print("single update, elapsed 5s:")
print(repr(gen([("Downloading", 100)], [(0, 42, 105.0)])))
print("---")
# Two updates: samples (100, +42), (105, +42). window span 5s, sum=84 -> 16.8 it/s
print("two updates 5s apart:")
print(repr(gen([("Downloading", 100)], [(0, 42, 100.0), (0, 84, 105.0)])))
print("---")
# Speed formatting thresholds
for n, t in [(999, 101.0), (1000, 101.0), (1234, 101.0), (12345, 101.0), (1234567, 101.0)]:
    print(f"speed ~{n}:")
    print(repr(gen([("D", n)], [(0, n, t)])))
    print("---")
# ETA rounding: remaining/speed fractional
print("eta rounding check (42 done of 100 at t=105):")
print(repr(gen([("Downloading", 100)], [(0, 42, 105.0)])))
