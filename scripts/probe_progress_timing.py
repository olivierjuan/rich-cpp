import io
from rich.console import Console
from rich.progress import Progress

def run(times):
    sink = io.StringIO()
    c = Console(file=sink, width=60, record=True, legacy_windows=False)
    def _t():
        for x in times:
            yield x
        while True:
            yield times[-1]
    it = _t()
    prog = Progress(console=c, auto_refresh=False, get_time=lambda: next(it))
    t1 = prog.add_task("Downloading", total=100)
    with prog:
        prog.update(t1, completed=42)
        prog.update(t1, completed=84)
    return c.export_text().splitlines()

print("A: [100, 105]")
for line in run([100.0, 105.0]):
    print(repr(line))
print("B: [100, 100, 105]")
for line in run([100.0, 100.0, 105.0]):
    print(repr(line))
print("C: [100, 105, 105]")
for line in run([100.0, 105.0, 105.0]):
    print(repr(line))
