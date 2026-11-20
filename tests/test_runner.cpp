#include <rich_core.h>
#include <box.h>
#include <Text.h>
#include <panel/Panel.h>
#include <panel/Table.h>
#include <panel/Columns.h>
#include <panel/Tree.h>
#include <iostream>
#include <string>
#include <vector>
#ifdef _WIN32
#  include <io.h>
#  include <fcntl.h>
#endif

int main(int argc, char* argv[]) {
#ifdef _WIN32
    _setmode(_fileno(stdout), _O_BINARY);
#endif

    if (argc < 2) {
        std::cerr << "Usage: test_runner <test_name>\n";
        return 1;
    }
    std::string test_name = argv[1];

    rich::ConsoleOptions opts{.width = 60, .record = true, .legacy_windows = false};
    std::ostringstream oss;
    rich::Console console(oss, opts);

    if (test_name == "panel_basic") {
        console.print(rich::AnyRenderable(
            rich::panel::Panel(rich::AnyRenderable(rich::Text{"Hello World", {}}),
                               {.title = "Test", .width = 20})
        ));
    } else if (test_name == "panel_multiline") {
        console.print(rich::AnyRenderable(
            rich::panel::Panel(rich::AnyRenderable(rich::Text{"Hello World\nLine 2", {}}),
                               {.title = "Title", .width = 24})
        ));
    } else if (test_name == "table_basic") {
        rich::table::Table t;
        t.add_column("Name", rich::table::Justify::Left)
         .add_column("Age", rich::table::Justify::Right);
        t.add_row({"Alice", "30"})
         .add_row({"Bob", "25"});
        console.print(rich::AnyRenderable(t));
    } else if (test_name == "rule_basic") {
        console.rule("My Rule");
    } else if (test_name == "columns_basic") {
        std::vector<rich::AnyRenderable> items;
        items.push_back(rich::AnyRenderable(
            rich::panel::Panel(rich::AnyRenderable(rich::Text{"Card 1", {}}), {.title = "A", .width = 16})
        ));
        items.push_back(rich::AnyRenderable(
            rich::panel::Panel(rich::AnyRenderable(rich::Text{"Card 2", {}}), {.title = "B", .width = 16})
        ));
        console.print(rich::decorator::columns(std::move(items), /*spacing=*/2, /*max_columns=*/2));
    } else if (test_name == "box_double") {
        console.print(rich::AnyRenderable(
            rich::panel::Panel(rich::AnyRenderable(rich::Text{"Double Border", {}}),
                               {.title = "Box", .width = 20, .box = rich::box::DOUBLE})
        ));
    } else if (test_name == "box_heavy") {
        console.print(rich::AnyRenderable(
            rich::panel::Panel(rich::AnyRenderable(rich::Text{"Heavy Border", {}}),
                               {.title = "Box", .width = 20, .box = rich::box::HEAVY})
        ));
    } else if (test_name == "box_ascii") {
        console.print(rich::AnyRenderable(
            rich::panel::Panel(rich::AnyRenderable(rich::Text{"ASCII Border", {}}),
                               {.title = "Box", .width = 20, .box = rich::box::ASCII})
        ));
    } else if (test_name == "text_wrap") {
        rich::ConsoleOptions small_opts{.width = 20, .record = true, .legacy_windows = false};
        std::ostringstream small_oss;
        rich::Console small_console(small_oss, small_opts);
        small_console.print(rich::AnyRenderable(
            rich::text::Text("The quick brown fox jumps over the lazy dog near the riverbank")
        ));
        std::cout << small_console.export_text();
        return 0;
    } else if (test_name == "tree_basic") {
        rich::tree::Tree t("Root");
        t.add("Child 1");
        auto& c2 = t.add("Child 2");
        c2.add("Grandchild 2.1");
        t.add("Child 3");
        console.print(rich::AnyRenderable(t));
    } else {
        std::cerr << "Unknown test: " << test_name << "\n";
        return 1;
    }

    std::cout << console.export_text();
    return 0;
}
