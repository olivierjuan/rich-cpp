#include <rich_core.h>
#include <panel/Panel.h>
#include <panel/Padding.h>
#include <panel/Table.h>
#include <iostream>
int main() {
    using namespace rich;
    using namespace rich::panel;
    using namespace rich::decorator;
    
    Console console;
    
    // Demonstrate pipe composition
    // Text is implicitly convertible to AnyRenderable
    auto content = AnyRenderable(StyledText{"[bold][red]Hello[/red][/bold] from [green]C++ Rich![/green]"});
    
    console.print(content | pad(1) | panel("Demo"));
    
    return 0;
}

int main() {
    using namespace rich;
    using namespace rich::table;
    using namespace rich::decorator;

    Console console;

    // Header banner
    auto banner = AnyRenderable(StyledText{"[bold bright_white]Rich C++[/] - [dim]Terminal formatting for modern C++20[/]"});
    console.print(banner | pad(1) | rich::panel::panel({.title = "Features Showcase", .show_border = true}));

    // 1. Text styles & Colors demo
    console.rule("Text Styling & Colors");
    console.print("[bold]Bold[/], [italic]Italic[/], [underline]Underline[/], [dim]Dimmed[/], [reverse]Reverse[/]");
    console.print("[red]Red[/], [green]Green[/], [yellow]Yellow[/], [blue]Blue[/], [magenta]Magenta[/], [cyan]Cyan[/], [white]White[/]");
    console.print("[bright_red]Bright Red[/], [bright_green]Bright Green[/], [bright_yellow]Bright Yellow[/], [bright_cyan]Bright Cyan[/]");

    // 2. Syntax-like Code Block inside Panel
    console.rule("Code Snippet");
    std::string code =
        "[bright_blue]#include[/] [bright_yellow]<rich/console.hpp>[/]\n"
        "\n"
        "[bright_cyan]int[/] [bright_green]main[/]() {\n"
        "    rich::Console console;\n"
        "    console.print([bright_green]\"[bold red]Hello[/] [blue]World![/]\"[/]);\n"
        "    [bright_magenta]return[/] [bright_yellow]0[/];\n"
        "}";
    console.print(AnyRenderable(StyledText{code}) | rich::panel::panel({.title = "example.cpp", .show_border = true}));

    // 3. Composed Table
    console.rule("Formatted Table");
    Table table("Movies");
    table.add_column("Release", Justify::Left)
         .add_column("Title", Justify::Left)
         .add_column("Box Office", Justify::Right);

    table.add_row({"Dec 20, 2019", "[bold]Star Wars: The Rise of Skywalker[/]", "[bright_green]$952,110,690[/]"})
         .add_row({"May 25, 2018", "[bold]Solo: A Star Wars Story[/]", "[yellow]$393,151,347[/]"})
         .add_row({"Dec 15, 2017", "[bold]Star Wars: The Last Jedi[/]", "[bright_green]$1,332,539,889[/]"})
         .add_row({"May 19, 1999", "[bold]Star Wars: Episode I - The Phantom Menace[/]", "[bright_green]$1,027,044,677[/]"});

    console.print(AnyRenderable(table));

    // 4. Progress / Status representation
    console.rule("Tasks & Progress");
    console.print("[bright_green]✓[/] [bold]Downloading data[/]     [bright_cyan][100%][/] [dim]4.2 MB/s[/]");
    console.print("[bright_green]✓[/] [bold]Parsing JSON[/]         [bright_cyan][100%][/] [dim]0.03s[/]");
    console.print("[bright_yellow]•[/] [bold]Rendering Table[/]      [yellow][ 85%][/] [dim]ETA 00:01[/]");
    console.print("[dim bright_white]•[/] [dim]Writing output[/]       [dim][  0%][/]");

    console.rule();
    return 0;
}
