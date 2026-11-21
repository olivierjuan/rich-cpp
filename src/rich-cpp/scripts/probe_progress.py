import io
from rich.console import Console
from rich.progress import Progress

def gen(cases):
    sink = io.StringIO()
    c = Console(file=sink, width=60, record=True, legacy_windows=False)
    prog = Progress(console=c, auto_refresh=False, get_time=lambda: 100.0)
    ids = []
    for d, t, _ in cases:
        ids.append(prog.add_task(d, total=t))
    with prog:
        for i, (d, _, comp) in enumerate(cases):
            prog.update(ids[i], completed=comp)
    return c.export_text()

cases_list = [
    [("Downloading", 100, 42), ("Processing", 200, 137)],
    [("Downloading", 100, 5), ("Processing", 200, 10)],
    [("Downloading", 100, 95), ("Processing", 200, 190)],
    [("Downloading", 100, 100), ("Processing", 200, 200)],
    [("Downloading", 100, 0), ("Processing", 200, 0)],
    [("Downloading", 100, 50), ("Processing", 200, 100)],
    [("Downloading", 100, 45), ("Processing", 200, 145)],
    [("Downloading", 100, 49), ("Processing", 200, 149)],
    [("Downloading", 100, 55), ("Processing", 200, 155)],
]
for cases in cases_list:
    print(repr(gen(cases)))
    print("---")
