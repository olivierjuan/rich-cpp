import io
from rich.console import Console
from rich.progress import Progress

calls = []
def make_time():
    seq = iter([100.0, 105.0])
    def t():
        v = next(seq, 105.0)
        calls.append(v)
        return v
    return t

sink = io.StringIO()
c = Console(file=sink, width=60, record=True, legacy_windows=False)
prog = Progress(console=c, auto_refresh=False, get_time=make_time())
t1 = prog.add_task("Downloading", total=100)
print("after add_task:", calls)
with prog:
    prog.update(t1, completed=42)
    print("after update1:", calls)
    prog.update(t1, completed=84)
    print("after update2:", calls)
task = prog.tasks[0]
print("samples:", list(task._progress))
print("speed:", task.speed)
print("eta:", task.time_remaining)
print(c.export_text(), end="")
