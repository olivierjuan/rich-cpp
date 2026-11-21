#include <rich_core.h>
#include <box.h>
#include <Text.h>
#include <panel/Panel.h>
#include <panel/Table.h>
#include <panel/Columns.h>
#include <panel/Tree.h>
#include <panel/Align.h>
#include <panel/Constrain.h>
#include <panel/Layout.h>
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
    } else if (test_name == "align_center") {
        console.print(
            rich::AnyRenderable(rich::panel::Panel(rich::AnyRenderable(rich::Text{"Hello World", {}}),
                                                   {.width = 30}))
                | rich::align::align(rich::align::Justify::Center)
        );
    } else if (test_name == "align_right") {
        console.print(
            rich::AnyRenderable(rich::panel::Panel(rich::AnyRenderable(rich::Text{"Hello World", {}}),
                                                   {.width = 30}))
                | rich::align::align(rich::align::Justify::Right)
        );
    } else if (test_name == "align_left") {
        console.print(
            rich::AnyRenderable(rich::panel::Panel(rich::AnyRenderable(rich::Text{"Hello World", {}}),
                                                   {.width = 30}))
                | rich::align::align(rich::align::Justify::Left)
        );
    } else if (test_name == "constrain_basic") {
        console.print(
            rich::AnyRenderable(rich::panel::Panel(rich::AnyRenderable(rich::Text{"Hello World", {}}),
                                                   {.width = 30}))
                | rich::constrain::constrain(30)
        );
    } else if (test_name == "layout_row") {
        rich::ConsoleOptions l_opts{.width = 40, .height = 8, .record = true, .legacy_windows = false};
        std::ostringstream l_oss;
        rich::Console l_console(l_oss, l_opts);
        rich::layout::Layout root;
        std::vector<rich::layout::Layout> lr_children;
        lr_children.push_back(rich::layout::Layout(rich::layout::LayoutOptions{.name = "left", .size = 20}));
        lr_children.push_back(rich::layout::Layout(rich::layout::LayoutOptions{.name = "right"}));
        root.split_row(std::move(lr_children));
        root.get("left").update(rich::AnyRenderable(
            rich::panel::Panel(rich::AnyRenderable(rich::Text{"Left", {}}),
                               {.width = 18, .height = 8})));
        root.get("right").update(rich::AnyRenderable(
            rich::panel::Panel(rich::AnyRenderable(rich::Text{"Right", {}}),
                               {.width = 18, .height = 8})));
        l_console.print(rich::AnyRenderable(root));
        std::cout << l_console.export_text();
        return 0;
    } else if (test_name == "layout_column") {
        rich::ConsoleOptions l_opts{.width = 40, .height = 10, .record = true, .legacy_windows = false};
        std::ostringstream l_oss;
        rich::Console l_console(l_oss, l_opts);
        rich::layout::Layout root;
        std::vector<rich::layout::Layout> lc_children;
        lc_children.push_back(rich::layout::Layout(rich::layout::LayoutOptions{.name = "top"}));
        lc_children.push_back(rich::layout::Layout(rich::layout::LayoutOptions{.name = "bottom", .ratio = 2}));
        root.split_column(std::move(lc_children));
        root.get("top").update(rich::AnyRenderable(
            rich::panel::Panel(rich::AnyRenderable(rich::Text{"Top", {}}),
                               {.width = 38, .height = 3})));
        root.get("bottom").update(rich::AnyRenderable(
            rich::panel::Panel(rich::AnyRenderable(rich::Text{"Bottom", {}}),
                               {.width = 38, .height = 7})));
        l_console.print(rich::AnyRenderable(root));
        std::cout << l_console.export_text();
        return 0;
    } else {
        std::cerr << "Unknown test: " << test_name << "\n";
        return 1;
    }

    std::cout << console.export_text();
    return 0;
}
