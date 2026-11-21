import io
from rich.console import Console
from rich.progress import Progress

def gen(cases, width=60):
    sink = io.StringIO()
    c = Console(file=sink, width=width, record=True, legacy_windows=False)
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
    [("Downloading", 100, 44), ("Processing", 200, 144)],
    [("Downloading", 100, 46), ("Processing", 200, 146)],
    [("Downloading", 100, 48), ("Processing", 200, 148)],
    [("Downloading", 100, 54), ("Processing", 200, 154)],
    [("Downloading", 100, 99), ("Processing", 200, 199)],
    [("Downloading", 100, 1), ("Processing", 200, 2)],
    [("Downloading", 100, 3), ("Processing", 200, 6)],
    [("Downloading", 100, 4), ("Processing", 200, 8)],
]
for cases in cases_list:
    print(repr(gen(cases)))
    print("---")

# different widths
print("WIDTH 80:")
print(repr(gen([("Downloading", 100, 42), ("Processing", 200, 137)], width=80)))
print("WIDTH 40:")
print(repr(gen([("Downloading", 100, 42), ("Processing", 200, 137)], width=40)))
print("WIDTH 30:")
print(repr(gen([("Downloading", 100, 42), ("Processing", 200, 137)], width=30)))
