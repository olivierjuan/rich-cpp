import io
from rich.console import Console
from rich.progress import Progress

def run(times, updates):
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
        for u in updates:
            prog.update(t1, completed=u)
    return c.export_text().splitlines()[0]

# Two samples 5s apart: expect speed 42/5 = 8.4, remaining 58 -> ceil(6.9) = 7
print("A [100,105] upd 42,84:", repr(run([100.0, 105.0], [42, 84])))
# Three samples: (100,42),(105,42),(110,42): sum after first = 84, span 10 -> 8.4
print("B [100,105,110] upd 42,84,96:", repr(run([100.0, 105.0, 110.0], [42, 84, 96])))
# Single update at t=100, render at t=105 (one sample -> no speed)
print("C [100,105] upd 42:", repr(run([100.0, 105.0], [42])))
# Update at same time twice (span 0 -> no speed)
print("D [100,100] upd 42,84:", repr(run([100.0, 100.0], [42, 84])))
