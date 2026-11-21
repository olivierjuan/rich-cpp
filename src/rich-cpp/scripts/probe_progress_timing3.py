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
        task = prog.tasks[0]
        return (task.speed, task.time_remaining, list(task._progress), task.finished_time)

print("A:", run([100.0, 105.0], [42, 84]))
print("B:", run([100.0, 105.0, 110.0], [42, 84, 96]))
print("C:", run([100.0, 105.0], [42]))
