import io
from rich.console import Console
from rich.progress import Progress

# Advance the clock by 5s BEFORE each update so sample timestamps differ.
def make_time():
    t = [100.0]
    def fn():
        return t[0]
    def advance(dt):
        t[0] += dt
    return fn, advance

sink = io.StringIO()
c = Console(file=sink, width=60, record=True, legacy_windows=False)
fn, advance = make_time()
prog = Progress(console=c, auto_refresh=False, get_time=fn)
t1 = prog.add_task("Downloading", total=100)   # start_time = 100
with prog:
    advance(5.0)                               # now = 105
    prog.update(t1, completed=42)              # sample (105, 42)
    advance(5.0)                               # now = 110
    prog.update(t1, completed=84)              # sample (110, 42)
task = prog.tasks[0]
print("samples:", list(task._progress))
print("speed:", task.speed)
print("eta:", task.time_remaining)
print(c.export_text(), end="")
