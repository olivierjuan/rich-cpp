#include <rich_core.h>
#include <box.h>
#include <Text.h>
#include <panel/Panel.h>
#include <panel/Padding.h>
#include <panel/Table.h>
#include <panel/Columns.h>
#include <panel/Tree.h>
#include <panel/Align.h>
#include <panel/Constrain.h>
#include <panel/Layout.h>
#include <Status.h>
#include <Live.h>
#include <ProgressTracker.h>
#include <Track.h>
#include <Pretty.h>
#include <json/Json.h>
#include <ProgressBar.h>
#include <highlight/Syntax.h>
#include <markdown/Markdown.h>
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
    } else if (test_name == "text_wrap_cjk") {
        rich::ConsoleOptions small_opts{.width = 20, .record = true, .legacy_windows = false};
        std::ostringstream small_oss;
        rich::Console small_console(small_oss, small_opts);
        std::string hanzi = "\xE6\xBC\xA2\xE5\xAD\x97"; // 漢字
        std::string content;
        for (int k = 0; k < 10; ++k) content += hanzi;
        content += " abc def";
        small_console.print(rich::AnyRenderable(rich::text::Text(content)));
        std::cout << small_console.export_text();
        return 0;
    } else if (test_name == "text_wrap_emoji") {
        rich::ConsoleOptions small_opts{.width = 20, .record = true, .legacy_windows = false};
        std::ostringstream small_oss;
        rich::Console small_console(small_oss, small_opts);
        std::string smiley = "\xF0\x9F\x98\x80"; // 😀
        std::string flag = "\xF0\x9F\x87\xAB\xF0\x9F\x87\xB7"; // 🇫🇷
        std::string content = "a ";
        for (int k = 0; k < 15; ++k) content += smiley;
        content += " b ";
        for (int k = 0; k < 4; ++k) content += flag;
        small_console.print(rich::AnyRenderable(rich::text::Text(content)));
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
    } else if (test_name == "status_static") {
        // Deterministic frame 0 of the "dots" spinner with markup description.
        console.print(rich::AnyRenderable(rich::status::Status("[bold]Loading[/] data")));
    } else if (test_name == "live_static") {
        // Live's first frame is a plain render of its renderable (no padding to
        // console width), so we render the panel directly — byte-identical to
        // what rich.live.Live would draw on its first tick.
        console.print(rich::AnyRenderable(
            rich::panel::Panel(rich::AnyRenderable(rich::Text{"Hello World\nLine 2", {}}),
                               {.title = "Live", .width = 24})
        ));
    } else if (test_name == "progress_tracker") {
        // Deterministic: fixed injected clock (start at t=0, render at t=0) so
        // elapsed is zero and speed/ETA are unknown ("-:--:--"), matching the
        // Python oracle generated with get_time=lambda: 100.0.
        rich::progress::ProgressTracker tracker;
        auto t0 = std::chrono::nanoseconds{0};
        int r1 = tracker.add_task({.description = "Downloading", .total = 100}, t0);
        int r2 = tracker.add_task({.description = "Processing", .total = 200}, t0);
        tracker.update(r1, 42, 100, t0);
        tracker.update(r2, 137, 200, t0);
        auto segs = tracker.render(console, rich::Options{.width = 60}, t0);
        for (const auto& s : segs) std::cout << s.text;
        return 0;
    } else if (test_name == "progress_speed") {
        // Rolling-window speed/ETA: task starts at t=100s; the clock advances
        // 5s before each update so sample timestamps differ (mirroring the
        // Python oracle's mutable get_time closure).
        //   samples [(105, 42), (110, 42)] -> speed 8.4 it/s, remaining 16
        //   -> ceil(16 / 8.4) = 2 -> ETA "0:00:02". The second task has one
        //   sample, so its ETA stays "-:--:--".
        using namespace std::chrono;
        const auto sec = [](double s) { return nanoseconds{static_cast<long long>(s * 1e9)}; };
        rich::progress::ProgressTracker tracker;
        int r1 = tracker.add_task({.description = "Downloading", .total = 100}, sec(100));
        int r2 = tracker.add_task({.description = "Processing", .total = 200}, sec(100));
        tracker.update(r1, 42, 100, sec(105));
        tracker.update(r2, 137, 200, sec(105));
        tracker.update(r1, 84, 100, sec(110));
        auto segs = tracker.render(console, rich::Options{.width = 60}, sec(110));
        for (const auto& s : segs) std::cout << s.text;
        return 0;
    } else if (test_name == "export_svg") {
        // SVG terminal snapshot: styled markup line + plain line, mirroring the
        // Python oracle (record=True, width=80, title="Rich").
        rich::ConsoleOptions svg_opts{.width = 80, .record = true, .legacy_windows = false};
        rich::Console svg_console(oss, svg_opts);
        svg_console.print("[bold red]Hello[/] [on blue]colored[/] [reverse]reversed[/] [dim]dim[/] [italic]it[/] [underline]ul[/]");
        svg_console.print("plain");
        std::cout << svg_console.export_svg("Rich");
        return 0;
    } else if (test_name == "export_html") {
        // HTML snapshot of the same styled markup + plain content as the
        // export_svg case, default options (class-based styles).
        rich::ConsoleOptions h_opts{.width = 80, .record = true, .legacy_windows = false};
        rich::Console hc(oss, h_opts);
        hc.print("[bold red]Hello[/] [on blue]colored[/] [reverse]reversed[/] [dim]dim[/] [italic]it[/] [underline]ul[/]");
        hc.print("plain");
        std::cout << hc.export_html();
        return 0;
    } else if (test_name == "emoji_basic") {
        rich::ConsoleOptions e_opts{.width = 80, .record = true, .legacy_windows = false};
        rich::Console ec(oss, e_opts);
        ec.print("Hello :smile: and :dog: and :notanemoji:");
        ec.print("[bold red]:heart:[/] variant :copyright-emoji: :copyright-text:");
        std::cout << ec.export_text();
        return 0;
    } else if (test_name == "pretty_list") {
        rich::ConsoleOptions p_opts{.width = 80, .record = true, .legacy_windows = false};
        rich::Console pc(oss, p_opts);
        pc.print(rich::pretty::pretty(std::vector<int>{1, 2, 3}));
        pc.print(rich::pretty::pretty(std::vector<std::string>{"apple", "banana"}));
        pc.print(rich::pretty::pretty(std::vector<int>{}));
        std::vector<int> longv;
        for (int i = 15; i < 40; ++i) longv.push_back(i);
        pc.print(rich::pretty::pretty(longv));
        std::cout << pc.export_text();
        return 0;
    } else if (test_name == "pretty_dict") {
        rich::ConsoleOptions p_opts{.width = 80, .record = true, .legacy_windows = false};
        rich::Console pc(oss, p_opts);
        pc.print(rich::pretty::pretty(std::map<std::string, int>{{"a", 1}, {"b", 2}}));
        pc.print(rich::pretty::pretty(std::map<std::string, int>{}));
        std::map<std::string, int> longd;
        for (int i = 0; i < 12; ++i) longd["key" + std::to_string(i / 10) + std::to_string(i % 10)] = i;
        pc.print(rich::pretty::pretty(longd));
        pc.print(rich::pretty::pretty(std::map<std::string, std::vector<int>>{
            {"nums", {1, 2}}, {"other", {3, 4}}}));
        std::vector<int> longv;
        for (int i = 15; i < 40; ++i) longv.push_back(i);
        pc.print(rich::pretty::pretty(std::map<std::string, std::vector<int>>{
            {"nums", longv}, {"other", {1}}}));
        std::cout << pc.export_text();
        return 0;
    } else if (test_name == "pretty_scalars") {
        rich::ConsoleOptions p_opts{.width = 80, .record = true, .legacy_windows = false};
        rich::Console pc(oss, p_opts);
        pc.print(rich::pretty::pretty(42));
        pc.print(rich::pretty::pretty(1.5));
        pc.print(rich::pretty::pretty(true));
        pc.print(rich::pretty::pretty(nullptr));
        pc.print(rich::pretty::pretty(std::make_tuple(1, "two")));
        pc.print(rich::pretty::pretty(std::tuple<int>(42)));
        pc.print(rich::pretty::pretty(std::vector<std::optional<int>>{
            std::nullopt, 1, std::nullopt}));
        std::cout << pc.export_text();
        return 0;
    } else if (test_name == "pretty_options") {
        rich::ConsoleOptions p2_opts{.width = 40, .record = true, .legacy_windows = false};
        rich::Console pc2(oss, p2_opts);
        {
            rich::pretty::PrettyOptions o;
            o.max_length = 2;
            pc2.print(rich::pretty::pretty(std::vector<int>{1, 2, 3, 4, 5}, o));
        }
        {
            rich::pretty::PrettyOptions o;
            o.max_string = 5;
            pc2.print(rich::pretty::pretty(std::vector<std::string>{std::string(30, 'a'), "b"}, o));
        }
        {
            rich::pretty::PrettyOptions o;
            o.max_depth = 1;
            pc2.print(rich::pretty::pretty(std::vector<std::vector<std::vector<int>>>{
                                                {{1}, {2}}, {{3}}},
                                            o));
        }
        {
            rich::pretty::PrettyOptions o;
            o.expand_all = true;
            pc2.print(rich::pretty::pretty(std::vector<int>{1, 2, 3, 4, 5, 6, 7, 8}, o));
        }
        {
            rich::pretty::PrettyOptions o;
            o.indent_guides = true;
            pc2.print(rich::pretty::pretty(std::vector<std::vector<std::string>>{
                                                {std::string(20, 'x')},
                                                {std::string(16, 'y')}},
                                            o));
        }
        std::cout << pc2.export_text();
        return 0;
    } else if (test_name == "json_basic") {
        rich::ConsoleOptions j_opts{.width = 80, .record = true, .legacy_windows = false};
        rich::Console jc(oss, j_opts);
        jc.print(rich::json_view::json("{\"a\": 1, \"b\": [true, null, \"x\"], \"c\": 2.5}"));
        jc.print(rich::json_view::json("[1, 2, 3]"));
        jc.print(rich::json_view::json("{}"));
        jc.print(rich::json_view::json("{\"dup\": 1, \"dup\": 2, \"s\": \"hi\\n\\u0041\", "
                                       "\"f\": -0.5, \"e\": 1e2, \"wide\": \"" +
                                       std::string(90, 'y') + "\"}"));
        jc.print(rich::json_view::json("{\"u\": \"\\u6f22\\u5b57\", \"n\": NaN, \"inf\": Infinity}"));
        std::cout << jc.export_text();
        return 0;
    } else if (test_name == "syntax_basic") {
        rich::ConsoleOptions s_opts{.width = 40, .record = true, .legacy_windows = false};
        rich::Console sc(oss, s_opts);
        const std::string py_code = "def fib(n):\n"
                                    "    \"\"\"Return the nth Fibonacci number.\"\"\"\n"
                                    "    if n <= 1:\n"
                                    "        return n\n"
                                    "    a, b = 0, 1\n"
                                    "    for _ in range(n - 1):\n"
                                    "        a, b = b, a + b\n"
                                    "    return a + b\n";
        sc.print(rich::highlight::syntax(py_code, "python"));
        const std::string cpp_code = "int main() {\n"
                                     "    // entry point\n"
                                     "    int total = 0;\n"
                                     "    for (int i = 1; i <= 10; ++i) {\n"
                                     "        total += i;\n"
                                     "    }\n"
                                     "    return total;\n"
                                     "}\n";
        rich::highlight::SyntaxOptions so;
        so.line_numbers = true;
        so.start_line = 10;
        so.highlight_lines = {12};
        sc.print(rich::highlight::syntax(cpp_code, "cpp", so));
        sc.print(rich::highlight::syntax("plain text\nwith punctuation, and words like 42\n",
                                         "text"));
        rich::highlight::SyntaxOptions so2;
        so2.line_numbers = true;
        so2.word_wrap = true;
        sc.print(rich::highlight::syntax(py_code, "python", so2));
        std::cout << sc.export_text();
        return 0;
    } else if (test_name == "markdown_basic") {
        rich::ConsoleOptions m_opts{.width = 60, .record = true, .legacy_windows = false};
        rich::Console mc(oss, m_opts);
        const std::string doc = "# Heading One\n"
                                "\n"
                                "Paragraph with **bold**, *italic*, `code`, ~~strike~~ and a\n"
                                "[link](https://example.com) plus words that wrap across lines\n"
                                "at the console width boundary.\n"
                                "\n"
                                "## Heading Two\n"
                                "\n"
                                "- alpha one\n"
                                "- beta two\n"
                                "  - nested one\n"
                                "- gamma three\n"
                                "\n"
                                "3. first item\n"
                                "3. second item\n"
                                "\n"
                                "> quoted line one\n"
                                "> quoted line two\n"
                                "\n"
                                "```python\n"
                                "def f(x):\n"
                                "    return x * 2\n"
                                "```\n";
        mc.print(rich::markdown::markdown(doc, false));
        std::cout << mc.export_text();
        return 0;
    } else if (test_name == "progress_bar_basic") {
        rich::ConsoleOptions p_opts{.width = 60, .record = true, .legacy_windows = false};
        rich::Console pc(oss, p_opts);
        for (int done : {0, 25, 50, 75, 100}) {
            rich::progress::ProgressBar bar(100, 30);
            bar.update(static_cast<size_t>(done));
            pc.print(rich::AnyRenderable(bar));
        }
        std::cout << pc.export_text();
        return 0;
    } else if (test_name == "markup_basic") {
        console.print(std::string(
            "[b]B[/b] [i]I[/i] [u red]U[/] [on yellow]Y[/] "
            "[b]n[bold]ested[/][/b] [[bracket]]"));
        std::cout << console.export_text();
        return 0;
    } else if (test_name == "padding_basic") {
        rich::ConsoleOptions p_opts{.width = 40, .record = true, .legacy_windows = false};
        std::ostringstream p_oss;
        rich::Console pc(p_oss, p_opts);
        // pad() factory ...
        pc.print(rich::decorator::pad(2)(
            rich::AnyRenderable(rich::text::Text("line one here\nline two here"))));
        // ... and pipe syntax
        pc.print(rich::AnyRenderable(rich::text::Text("pipe line\nsecond line")) |
                 rich::decorator::pad(1));
        std::cout << pc.export_text();
        return 0;
    } else if (test_name == "track_basic") {
        // track() ticks one step per element; a fully iterated range leaves
        // the task finished, so the frozen-clock render is byte-stable.
        rich::progress::ProgressTracker tracker;
        const std::chrono::nanoseconds t0{0};
        std::vector<int> items(8);
        for (auto& x : rich::progress::track(tracker, "Processing")(items)) x = 1;
        auto segs = tracker.render(console, rich::Options{.width = 60}, t0);
        for (const auto& s : segs) std::cout << s.text;
        return 0;
    } else if (test_name == "style_sgr") {
        // Raw SGR stream: forced terminal, truecolor, no_color explicitly off
        // (host shells set NO_COLOR / TERM=dumb). Compare sink bytes, one
        // print per line — bypass export_text().
        rich::ConsoleOptions s_opts{.width = 60, .record = false,
                                    .legacy_windows = false,
                                    .force_terminal = true,
                                    .color_system = rich::ColorSystem::Truecolor,
                                    .no_color = false};
        std::ostringstream s_oss;
        rich::Console sc(s_oss, s_opts);
        sc.print(std::string_view("[red]a[/][bright_red]b[/]"));
        sc.print(std::string_view("[color(196)]a[/][on color(27)]b[/]"));
        sc.print(std::string_view("[#ff8800]a[/][on #00ff88]b[/]"));
        sc.print(std::string_view("[rgb(200,100,50)]a[/][on rgb(10,20,30)]b[/]"));
        sc.print(std::string_view("[bold strike reverse]a[/]"));
        sc.print(std::string_view(
            "[blink blink2 conceal underline2 frame encircle overline]a[/]"));
        sc.print(std::string_view("[default]a[/][on default]b[/]"));
        sc.print(std::string_view("[b]alias[/bold]"));
        std::cout << s_oss.str();
        return 0;
    } else if (test_name == "style_sgr_256") {
        rich::ConsoleOptions s_opts{.width = 60, .record = false,
                                    .legacy_windows = false,
                                    .force_terminal = true,
                                    .color_system = rich::ColorSystem::EightBit,
                                    .no_color = false};
        std::ostringstream s_oss;
        rich::Console sc(s_oss, s_opts);
        sc.print(std::string_view("[rgb(200,100,50)]a[/]"));
        sc.print(std::string_view("[color(196)]b[/]"));
        sc.print(std::string_view("[red]c[/]"));
        sc.print(std::string_view("[on rgb(200,100,50)]d[/]"));
        sc.print(std::string_view("[rgb(100,100,100)]e[/]"));
        std::cout << s_oss.str();
        return 0;
    } else if (test_name == "log_basic") {
        // Python resolves the caller path/line from its stack; C++ takes
        // them explicitly — the values mirror oracle_gen.py's log() lines.
        rich::ConsoleOptions l_opts{.width = 60, .record = true, .legacy_windows = false};
        std::ostringstream l_oss;
        rich::Console lc(l_oss, l_opts);
        std::tm t{};
        t.tm_year = 2026 - 1900; t.tm_mon = 6; t.tm_mday = 22;
        t.tm_hour = 21; t.tm_min = 53; t.tm_sec = 33;
        auto lo = [t](int line) {
            rich::LogOptions o;
            o.time = t;
            o.show_path = true;
            o.path = "oracle_gen.py";
            o.line_no = line;
            return o;
        };
        lc.log({std::string_view("hello world"), std::string_view("42")}, lo(330));
        lc.log({std::string_view("hello world"), std::string_view("42")}, lo(331));
        lc.log({std::string_view("[red]styled[/] message")}, lo(332));
        const std::string fifty(50, 'a');
        lc.log({std::string_view(fifty)}, lo(333));
        lc.log({}, lo(334));
        std::cout << lc.export_text();
        return 0;
    } else if (test_name == "log_sgr") {
        rich::ConsoleOptions l_opts{.width = 60, .record = false,
                                    .legacy_windows = false,
                                    .force_terminal = true,
                                    .color_system = rich::ColorSystem::Truecolor,
                                    .no_color = false};
        std::ostringstream l_oss;
        rich::Console lc(l_oss, l_opts);
        std::tm t{};
        t.tm_year = 2026 - 1900; t.tm_mon = 6; t.tm_mday = 22;
        t.tm_hour = 21; t.tm_min = 53; t.tm_sec = 33;
        auto lo = [t] {
            rich::LogOptions o;
            o.time = t;
            return o;
        };
        lc.log({std::string_view("[red]hello[/] world")}, lo());
        lc.log({std::string_view("again")}, lo());
        std::cout << l_oss.str();
        return 0;
    } else {
        std::cerr << "Unknown test: " << test_name << "\n";
        return 1;
    }

    std::cout << console.export_text();
    return 0;
}
