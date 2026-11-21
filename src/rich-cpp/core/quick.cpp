#include <rich_core.h>
#include <box.h>
#include <Text.h>
#include <ProgressBar.h>
#include <ProgressTracker.h>
#include <Track.h>
#include <Live.h>
#include <Status.h>
#include <panel/Panel.h>
#include <panel/Padding.h>
#include <panel/Align.h>
#include <panel/Constrain.h>
#include <panel/Table.h>
#include <panel/Columns.h>
#include <panel/Tree.h>
#include <panel/Layout.h>
#include <format>
#include <iostream>
#include <thread>
#include <vector>
#include <chrono>

int main() {
    using namespace rich;
    using namespace rich::table;
    using namespace rich::decorator;
    using namespace rich::tree;
    using namespace rich::progress;

    Console console;

    // Header banner (centered via Align decorator)
    auto banner = AnyRenderable(StyledText{"[bold bright_white]Rich C++[/] - [dim]Terminal formatting for modern C++20[/]"});
    console.print(banner | pad(1) | rich::panel::panel({.title = "Features Showcase", .show_border = true})
                         | rich::align::align(rich::align::Justify::Center));

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
    Table table(TableOptions{.title = "Movies", .box = box::ROUNDED});
    table.add_column("Release", Justify::Left)
         .add_column("Title", Justify::Left)
         .add_column("Box Office", Justify::Right);

    table.add_row({"Dec 20, 2019", "[bold]Star Wars: The Rise of Skywalker[/]", "[bright_green]$952,110,690[/]"})
         .add_row({"May 25, 2018", "[bold]Solo: A Star Wars Story[/]", "[yellow]$393,151,347[/]"})
         .add_row({"Dec 15, 2017", "[bold]Star Wars: The Last Jedi[/]", "[bright_green]$1,332,539,889[/]"})
         .add_row({"May 19, 1999", "[bold]Star Wars: Episode I - The Phantom Menace[/]", "[bright_green]$1,027,044,677[/]"});

    console.print(AnyRenderable(table));

    // 4. Tree View
    console.rule("Hierarchical Tree");
    Tree project_tree("[bold bright_white]rich-cpp[/]");
    auto& src = project_tree.add("[bold bright_blue]core[/]");
    src.add("rich_core.h");
    src.add("box.h");
    src.add("Text.h");
    src.add("ProgressBar.h");
    auto& widgets = project_tree.add("[bold bright_blue]panel[/]");
    widgets.add("Panel.h");
    widgets.add("Table.h");
    widgets.add("Columns.h");
    widgets.add("Tree.h");
    project_tree.add("[bold bright_yellow]CMakeLists.txt[/]");
    console.print(AnyRenderable(project_tree));

    // 6. Layout Dashboard
    console.rule("Layout Dashboard");
    {
        rich::ConsoleOptions dash_opts{.width = 60, .height = 6, .record = true};
        std::ostringstream dash_oss;
        rich::Console dash_console(dash_oss, dash_opts);
        rich::layout::Layout root;
        std::vector<rich::layout::Layout> dash_children;
        dash_children.push_back(rich::layout::Layout(rich::layout::LayoutOptions{.name = "side", .size = 20}));
        dash_children.push_back(rich::layout::Layout(rich::layout::LayoutOptions{.name = "main"}));
        root.split_row(std::move(dash_children));
        root.get("side").update(rich::AnyRenderable(
            rich::panel::Panel(rich::AnyRenderable(rich::Text{"Side panel", {}}),
                               {.title = "Side", .width = 18, .height = 6})));
        root.get("main").update(rich::AnyRenderable(
            rich::panel::Panel(rich::AnyRenderable(rich::Text{"Main area", {}}),
                               {.title = "Main", .width = 38, .height = 6})));
        dash_console.print(rich::AnyRenderable(root));
        std::cout << dash_console.export_text();
    }

    // 5. Progress Bars
    console.rule("Tasks & Progress Bars");
    ProgressBar bar1(100, 28); bar1.update(100);
    ProgressBar bar2(100, 28); bar2.update(85);
    ProgressBar bar3(100, 28); bar3.update(35);

    console.print("[bright_green]✓[/] [bold]Downloading data[/]     " + console.export_text().substr(0, 0));
    console.print(AnyRenderable(bar1));
    console.print("[bright_yellow]•[/] [bold]Processing chunks[/]    " + console.export_text().substr(0, 0));
    console.print(AnyRenderable(bar2));
    console.print("[dim bright_white]•[/] [dim]Compressing archives[/]  " + console.export_text().substr(0, 0));
    console.print(AnyRenderable(bar3));

    // 7. Status Spinner (live animation for ~300ms)
    console.rule("Status Spinner");
    {
        rich::status::Status spinner("Processing [bold]files[/]");
        spinner.start(console);
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        spinner.stop();
        console.print("[bright_green]✓[/] [bold]Done[/] — status line cleared");
    }

    // 8. Live Display (in-place refresh of a multi-row ProgressTracker)
    console.rule("Live Display");
    {
        ProgressTracker tracker(/*show_speed=*/true);
        int r1 = tracker.add_task({.description = "Downloading", .total = 100});
        int r2 = tracker.add_task({.description = "Processing", .total = 200});

        rich::live::Live live(AnyRenderable(tracker), /*refresh_per_second=*/10);
        live.start();
        for (int i = 0; i <= 10; ++i) {
            tracker.tick(r1, 10);   // 100 total over 10 steps
            tracker.tick(r2, 20);   // 200 total over 10 steps
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        live.stop();
        console.print("[bright_green]✓[/] [bold]All tasks complete[/]");
    }

    // 9. Range tracking (track adapter: one task per loop, tick per element)
    console.rule("Range Tracking");
    {
        ProgressTracker tracker;
        std::vector<int> items{1, 2, 3, 4, 5, 6, 7, 8};
        rich::live::Live live(AnyRenderable(tracker), /*refresh_per_second=*/10);
        live.start();
        int sum = 0;
        for (auto& x : rich::progress::track(tracker, "Summing")(items)) {
            sum += x;
            std::this_thread::sleep_for(std::chrono::milliseconds(120));
        }
        live.stop();
        console.print(std::format("[bright_green]✓[/] [bold]Done[/] — sum = {}", sum));
    }

    console.rule();
    return 0;
}
