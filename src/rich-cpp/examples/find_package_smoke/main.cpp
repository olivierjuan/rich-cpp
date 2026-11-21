// Downstream smoke test: verifies that `find_package(rich CONFIG REQUIRED)`
// resolves and the exported targets compile & link correctly.
#include <rich_core.h>
#include <iostream>

int main() {
    rich::Console console;
    console.print("[bold green]Smoke test OK[/] — rich found via find_package");
    return 0;
}
