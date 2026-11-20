#include <rich_core.h>
#include <panel/Panel.h>
#include <panel/Columns.h>
#include <iostream>
#include <string>
#include <vector>

int main() {
    using namespace rich;
    using namespace rich::decorator;

    Console console;

    // 5 boxes with intentionally different widths and varied content
    std::vector<AnyRenderable> cards;

    // Card 1: Compact box (width ~18)
    std::string c1 =
        "[dim]Status:[/] [bright_green]Active[/]\n"
        "[dim]Ping:[/]   [bright_cyan]12ms[/]";
    cards.push_back(
        AnyRenderable(StyledText{c1}) |
        rich::panel::panel({.title = "[bold bright_white]Node-01[/]", .show_border = true})
    );

    // Card 2: Medium box (width ~26)
    std::string c2 =
        "[dim]OS:[/]      [bright_white]Ubuntu 24.04[/]\n"
        "[dim]Kernel:[/]  [dim]6.8.0-generic[/]\n"
        "[dim]Uptime:[/]  [bright_yellow]14d 6h[/]";
    cards.push_back(
        AnyRenderable(StyledText{c2}) |
        rich::panel::panel({.title = "[bold bright_white]Production Server[/]", .show_border = true})
    );

    // Card 3: Wide box (width ~38)
    std::string c3 =
        "[dim]Workload:[/] [bright_green]Database Cluster Primary[/]\n"
        "[dim]Throughput:[/] [bright_cyan]142,850 queries/sec[/]\n"
        "[dim]Memory:[/]     [bright_yellow]48.2 GB / 64.0 GB[/]";
    cards.push_back(
        AnyRenderable(StyledText{c3}) |
        rich::panel::panel({.title = "[bold bright_white]PostgreSQL Core Replica[/]", .show_border = true})
    );

    // Card 4: Second line - Extra wide box (width ~42)
    std::string c4 =
        "[dim]Deployment:[/] [bright_white]Kubernetes Control Plane[/]\n"
        "[dim]Pods:[/]       [bright_green]184 Running[/] | [bright_yellow]3 Pending[/]\n"
        "[dim]Ingress:[/]    [bright_cyan]https://api.internal.cluster:443[/]";
    cards.push_back(
        AnyRenderable(StyledText{c4}) |
        rich::panel::panel({.title = "[bold bright_white]Kube-Master-East[/]", .show_border = true})
    );

    // Card 5: Second line - Compact box (width ~22)
    std::string c5 =
        "[dim]Role:[/]   [bright_magenta]Worker[/]\n"
        "[dim]CPU:[/]    [bright_green]18.4%[/]\n"
        "[dim]RAM:[/]    [bright_green]2.1 GB[/]";
    cards.push_back(
        AnyRenderable(StyledText{c5}) |
        rich::panel::panel({.title = "[bold bright_white]Edge-04[/]", .show_border = true})
    );

    console.rule("Intentionally Varied-Width Boxes (Multi-Row Left-Aligned)");
    // max_columns = 3: row 1 gets [compact, medium, wide], row 2 gets [extra wide, compact] flushed left
    console.print(columns(std::move(cards), /*spacing=*/3, /*max_columns=*/3));
    console.rule();

    return 0;
}
