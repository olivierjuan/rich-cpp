#include <rich_core.h>
#include <Palette.h>
#include <FileProxy.h>
#include <box.h>
#include <Text.h>
#include <Highlighter.h>
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
#include <Progress.h>
#include <highlight/Syntax.h>
#include <markdown/Markdown.h>
#include <iostream>
#include <sstream>
#include <cstdio>
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
        lc.log({std::string_view("hello world"), std::string_view("42")}, lo(332));
        lc.log({std::string_view("hello world"), std::string_view("42")}, lo(333));
        lc.log({std::string_view("[red]styled[/] message")}, lo(334));
        const std::string fifty(50, 'a');
        lc.log({std::string_view(fifty)}, lo(335));
        lc.log({}, lo(336));
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
    } else if (test_name == "theme_push") {
        rich::ConsoleOptions l_opts{.width = 60, .height = 25, .record = false,
                                    .legacy_windows = false,
                                    .force_terminal = true,
                                    .color_system = rich::ColorSystem::Truecolor,
                                    .no_color = false};
        std::ostringstream l_oss;
        rich::Console lc(l_oss, l_opts);
        std::tm t{};
        t.tm_hour = 21; t.tm_min = 53; t.tm_sec = 32;
        auto lo = [&t] {
            t.tm_sec += 1;
            rich::LogOptions o;
            o.time = t;
            return o;
        };
        lc.log({std::string_view("default")}, lo());
        lc.rule("sep");
        lc.push_theme(rich::Theme({{"log.time", "bold red"},
                                   {"rule.line", "bright_blue"}}));
        lc.log({std::string_view("themed")}, lo());
        lc.rule("sep");
        lc.pop_theme();
        lc.log({std::string_view("restored")}, lo());
        lc.rule("sep");
        std::cout << l_oss.str();
        return 0;
    } else if (test_name == "input_smoke") {
        // Interactive (not an oracle): piped stdin, prompt recorded, input
        // NOT recorded (Python echoes via the terminal, not the console).
        rich::ConsoleOptions io_opts{.width = 60, .record = true, .legacy_windows = false};
        std::ostringstream io_oss;
        rich::Console ic(io_oss, io_opts);
        std::string name = ic.input(std::string_view("[bold]Name?[/] "));
        std::string secret = ic.input(std::string_view("Password? "), true, true);
        std::cout << "got:[" << name << "][" << secret << "]\n";
        std::cout << ic.export_text();
        return 0;
    } else if (test_name == "capture_basic") {
        // Capture: prints inside the scope go to cap.get() — not to the
        // stream, not to the record buffer. Second half proves the captured
        // render carries SGR (force_terminal color system).
        std::ostringstream c_oss;
        rich::ConsoleOptions co{.width = 60, .record = true, .legacy_windows = false};
        rich::Console cc(c_oss, co);
        auto cap = cc.capture();
        cc.print(std::string_view("[bold]captured[/]"));
        cc.print(std::string_view("plain"));
        std::string out = cap.get() + "file[" + c_oss.str() + "]rec[" + cc.export_text() + "]";
        cc.print(std::string_view("after"));
        out += cc.export_text();
        std::ostringstream s_oss;
        rich::ConsoleOptions fo{.width = 60, .height = 25,
                                .legacy_windows = false,
                                .force_terminal = true,
                                .color_system = rich::ColorSystem::Truecolor,
                                .no_color = false};
        rich::Console cs(s_oss, fo);
        auto cap4 = cs.capture();
        cs.print(std::string_view("[bold]sgr[/]"));
        out += "sgr[" + cap4.get() + "]";
        std::cout << out;
        return 0;
    } else if (test_name == "screen_basic") {
        // Screen: shaped to width x console height — short lines padded to
        // width, tall content cropped, no trailing newline.
        std::ostringstream s_oss;
        rich::ConsoleOptions so{.width = 20, .height = 5, .record = true, .legacy_windows = false};
        rich::Console sc(s_oss, so);
        rich::Screen s1;
        s1.renderables.push_back(rich::AnyRenderable(
            rich::panel::Panel(rich::AnyRenderable(rich::Text{std::string("Hi"), {}}),
                               {.width = 20})));
        sc.print(rich::AnyRenderable(s1));
        rich::Screen s2;
        s2.renderables.push_back(rich::AnyRenderable(
            rich::Text{std::string("l1\nl2\nl3\nl4\nl5\nl6\nl7"), {}}));
        sc.print(rich::AnyRenderable(s2));
        std::cout << sc.export_text();
        return 0;
    } else if (test_name == "control_basic") {
        // Control bytes reach the stream only for terminals (force_terminal);
        // export_text() filters control segments, so compare stream bytes.
        // Console.control() is a no-op on dumb terminals — mirror the Python
        // _environ override by setting TERM for this process.
#ifdef _WIN32
        _putenv_s("TERM", "xterm-256color");
#else
        setenv("TERM", "xterm-256color", 1);
#endif
        rich::ConsoleOptions co{.width = 60, .height = 25, .record = true,
                                .legacy_windows = false,
                                .force_terminal = true,
                                .no_color = false};
        std::ostringstream o1;
        rich::Console c1(o1, co);
        c1.print(rich::AnyRenderable(rich::Control::bell()));
        c1.print(rich::AnyRenderable(rich::Control::home()));
        c1.print(rich::AnyRenderable(rich::Control::move(2, 3)));
        c1.print(rich::AnyRenderable(rich::Control::move(-1, -2)));
        c1.print(rich::AnyRenderable(rich::Control::move_to(4, 5)));
        c1.print(rich::AnyRenderable(rich::Control::move_to_column(7)));
        c1.print(rich::AnyRenderable(rich::Control::move_to_column(7, 2)));
        c1.print(rich::AnyRenderable(rich::Control::clear()));
        c1.print(rich::AnyRenderable(rich::Control::show_cursor(false)));
        c1.print(rich::AnyRenderable(rich::Control::alt_screen(true)));
        c1.print(rich::AnyRenderable(rich::Control::title(std::string("hello"))));
        c1.print(rich::AnyRenderable(
            rich::Control(std::vector<rich::ControlCode>{{rich::ControlType::EraseInLine, 2}})));
        std::string out = o1.str();
        std::ostringstream o2;
        rich::Console c2(o2, co);
        c2.control(rich::Control::move(1, 1));
        c2.control(rich::Control::home());
        out += "SEP" + o2.str();
        out += "SEPesc:" + rich::escape_control_codes(std::string("\a\b\v\f\r")) +
               "|st:" + rich::strip_control_codes(std::string("a\ab\b\fc")) + "|";
        std::cout << out;
        return 0;
    } else if (test_name == "colorfile_basic") {
        // rich._palettes + ANSI_COLOR_NAMES: Palette.match() indices, then raw
        // SGR proving the 235-entry name table end-to-end ("grey23" -> 237).
        auto q = [](const rich::Palette& p, int r, int g, int b) {
            return std::to_string(p.match(r, g, b));
        };
        std::string out;
        const auto& eb = rich::eight_bit_palette();
        const auto& st = rich::standard_palette();
        const auto& wi = rich::windows_palette();
        for (auto [r, g, b] : {std::tuple<int, int, int>{12, 34, 56}, {237, 1, 2},
                               {0, 0, 0}, {255, 255, 255}, {127, 127, 127}})
            out += q(eb, r, g, b);
        for (auto [r, g, b] : {std::tuple<int, int, int>{200, 100, 50}, {0, 0, 0},
                               {255, 255, 255}})
            out += q(st, r, g, b);
        for (auto [r, g, b] : {std::tuple<int, int, int>{12, 34, 56}, {237, 1, 2},
                               {100, 200, 100}})
            out += q(wi, r, g, b);
        out += "SEP";
        rich::ConsoleOptions s_opts{.width = 60, .height = 25, .record = false,
                                    .legacy_windows = false,
                                    .force_terminal = true,
                                    .color_system = rich::ColorSystem::Truecolor,
                                    .no_color = false};
        std::ostringstream s_oss;
        rich::Console sc(s_oss, s_opts);
        sc.print(std::string_view("[grey23]x[/][on dark_red]y[/]"));
        sc.print(std::string_view("[deep_pink3]a[/][bright_red]b[/]"));
        std::cout << out + s_oss.str();
        return 0;
    } else if (test_name == "fileproxy_basic") {
        // rich.file_proxy.FileProxy + rich._null_file.NullFile: partial-line
        // buffering, ANSI-decoded complete lines, raw-string flush.
        rich::ConsoleOptions o{.width = 60, .record = true, .legacy_windows = false};
        std::ostringstream dummy;
        std::ostringstream wrapped;
        rich::Console c(dummy, o);
        rich::FileProxy proxy(c, wrapped);
        proxy.write("partial");
        proxy.write(" line1\n");
        proxy.write("\x1b[1mstyled\x1b[0m and \x1b[31mred\x1b[0m line2\n");
        proxy.write("two\nlines\nin one write");
        proxy.flush();
        std::string out = c.export_text();
        const rich::NullFile nf;
        out += "NF:" + std::to_string(nf.fileno()) + "," +
               std::to_string(nf.write("x")) + "," +
               std::to_string(nf.readlines().size()) + "," +
               std::to_string(nf.tell()) + "," +
               std::to_string(static_cast<int>(nf.isatty())) + "," +
               std::to_string(static_cast<int>(nf.writable())) + "," +
               std::to_string(static_cast<int>(nf.readable())) + "," +
               std::to_string(nf.seek(123));
        std::cout << out;
        return 0;
    } else if (test_name == "from_ansi_basic") {
        // rich.text.Text.from_ansi (AnsiDecoder): SGR state machine,
        // 256/truecolor colors, lenient invalid codes, CR truncation,
        // multi-line join under a base style.
        rich::ConsoleOptions o{.width = 60, .record = true, .legacy_windows = false};
        std::ostringstream dummy;
        rich::Console c(dummy, o);
        const auto base = rich::parse_style_def("italic");
        const rich::Text t = rich::Text::from_ansi(
            std::string("\x1b[1mbold\x1b[0m normal\n"
                        "\x1b[38;5;196mred8\x1b[38;2;10;20;30mrgb\x1b[0m\n"
                        "\x1b[48;5;237;99mgarbage\x1b[31;33m last\n"
                        "over\r written\n"
                        "plain"),
            base);
        c.print(rich::AnyRenderable(t));
        std::cout << c.export_text();
        return 0;
    } else if (test_name == "highlighter_basic") {
        // Raw SGR stream (force_terminal truecolor): built-in highlighters.
        // print() flattens Text (base style dropped) — spans-only output.
        rich::ConsoleOptions s_opts{.width = 60, .height = 25, .record = false,
                                    .legacy_windows = false,
                                    .force_terminal = true,
                                    .color_system = rich::ColorSystem::Truecolor,
                                    .no_color = false};
        std::ostringstream s_oss;
        rich::Console sc(s_oss, s_opts);
        sc.print(rich::AnyRenderable(
            rich::ReprHighlighter()(
                std::string("{'name': 'apple', 'count': 1, 'ok': True, 'n': None}"))));
        sc.print(rich::AnyRenderable(
            rich::ReprHighlighter()(std::string("[1, 2.5, -3, 1.5e10, 0x1F, 2+3j]"))));
        sc.print(rich::AnyRenderable(
            rich::ReprHighlighter()(
                std::string("127.0.0.1 2001:db8::1 /foo/bar.py https://ex.org/x?a=1"))));
        sc.print(rich::AnyRenderable(rich::JSONHighlighter()(
            std::string("{\"name\": \"apple\", \"count\": 1, \"flag\": true, \"z\": null, "
                        "\"nested\": {\"a\": [1, 2]}}"))));
        sc.print(rich::AnyRenderable(
            rich::ISO8601Highlighter()(std::string("2008-08-30 17:21:59"))));
        sc.print(rich::AnyRenderable(
            rich::ISO8601Highlighter()(std::string("2008-08-30T01:45:36.123Z"))));
        sc.print(rich::AnyRenderable(
            rich::ISO8601Highlighter()(std::string("2008-W35-6"))));
        sc.print(rich::AnyRenderable(
            rich::NullHighlighter()(std::string("nothing here 123"))));
        std::cout << s_oss.str();
        return 0;
    } else if (test_name == "text_extras_basic") {
        // Full Text semantics (base style, justify, overflow HONORED):
        // console.print flattens Text, so render through render_lines —
        // the same path a container (Panel/Group) uses.
        std::ostringstream te_out;
        auto show = [&](const rich::text::Text& t, size_t w) {
            rich::ConsoleOptions o{.width = w, .height = 25, .record = false,
                                   .legacy_windows = false,
                                   .force_terminal = true,
                                   .color_system = rich::ColorSystem::Truecolor,
                                   .no_color = false};
            std::ostringstream dummy;
            rich::Console c2(dummy, o);
            auto lines = c2.render_lines({rich::AnyRenderable(t)},
                                         rich::Options{.width = w}, nullptr, false);
            for (auto& line : lines) {
                for (auto& s : line) {
                    std::string sg =
                        rich::style_sgr(s.style, rich::ColorSystem::Truecolor, false);
                    if (sg.empty()) te_out << s.text;
                    else te_out << sg << s.text << "\x1b[0m";
                }
                te_out << "\n";
            }
        };
        {
            auto t = rich::text::Text("the quick brown fox jumps over the lazy dog");
            t.set_justify(rich::text::Justify::Full);
            show(t, 40);
        }
        {
            auto t = rich::text::Text("the quick brown fox jumps over the lazy dog");
            t.set_justify(rich::text::Justify::Full);
            t.stylize(std::string("bold"), 0, 20);
            show(t, 40);
        }
        {
            auto t = rich::text::Text(
                "the quick brown fox jumps over the lazy dog",
                rich::parse_style_def("red"));
            t.set_justify(rich::text::Justify::Full);
            show(t, 40);
        }
        {
            auto t = rich::text::Text("hi");
            t.set_justify(rich::text::Justify::Center);
            t.stylize(std::string("bold"), 0, 2);
            show(t, 10);
        }
        {
            auto t = rich::text::Text("hi");
            t.set_justify(rich::text::Justify::Right);
            t.stylize(std::string("italic"), 0, 2);
            show(t, 10);
        }
        {
            auto t = rich::text::Text("hi");
            t.set_justify(rich::text::Justify::Left);
            t.stylize(std::string("underline"), 1, 2);
            show(t, 10);
        }
        {
            auto t = rich::text::Text("hi", rich::parse_style_def("red"));
            t.stylize(std::string("bold"), 0, 1);
            show(t, 10);
        }
        {
            auto t = rich::text::Text("helloxxxxxxxxxxxxx world");
            t.set_overflow(rich::text::Overflow::Ellipsis);
            t.stylize(std::string("red"), 0, 5);
            show(t, 12);
        }
        {
            auto t = rich::text::Text("helloxxxxxxxxxxxxx world");
            t.set_overflow(rich::text::Overflow::Crop);
            t.stylize(std::string("red"), 0, 5);
            show(t, 12);
        }
        {
            auto t = rich::text::Text("xxxxxxxxxxxxxxxxxxxxx yyy");
            t.set_overflow(rich::text::Overflow::Fold);
            t.stylize(std::string("blue"), 0, 5);
            show(t, 10);
        }
        {
            auto t = rich::text::Text(std::string("ab\ncd"));
            t.stylize(std::string("bold"), 0, 4);
            show(t, 20);
        }
        {
            auto t = rich::text::Text(std::string("a\tbb"));
            t.stylize(std::string("bold"), 1, 3);
            show(t, 20);
        }
        show(rich::text::Text(std::string("a\tbb")), 20);
        show(rich::text::Text::assemble({
                   rich::text::Text::Part("he", rich::parse_style_def("bold")),
                   rich::text::Text::Part("llo", rich::parse_style_def("italic")),
                   rich::text::Text::Part(" world")}),
             20);
        show(rich::text::Text::styled("abc", rich::parse_style_def("bold red")), 20);
        {
            auto t = rich::text::Text("apple banana");
            t.highlight_regex(std::string("(?P<f>\\w+) (?P<s>\\w+)"),
                              std::string("green"), std::string("t."));
            show(t, 20);
        }
        {
            auto t = rich::text::Text(std::string("abc   \n  def  "));
            t.stylize(std::string("bold"), 0, 3);
            show(t, 20);
        }
        {
            auto t = rich::text::Text("the  quick fox");
            t.set_justify(rich::text::Justify::Full);
            show(t, 20);
        }
        std::cout << te_out.str();
        return 0;
    } else if (test_name == "table_extras") {
        // show_footer / caption / row_styles (geometry) / expand /
        // add_section. Box pinned to ROUNDED on both sides (Python 15.0.0
        // default is HEAVY_HEAD; the C++ default stays ROUNDED by design).
        auto show_table = [&](rich::table::Table t) {
            std::ostringstream oss;
            rich::ConsoleOptions o{.width = 60, .record = true,
                                   .legacy_windows = false};
            rich::Console c(oss, o);
            c.print(rich::AnyRenderable(std::move(t)));
            std::cout << c.export_text();
        };
        {
            rich::table::Table t;
            t.add_column("Name", "nm");
            t.add_column("Age", "ag", rich::table::Justify::Right);
            t.add_row({"Alice", "30"});
            t.add_row({"Bob", "25"});
            t.show_footer(true);
            show_table(std::move(t));
        }
        {
            rich::table::Table t;
            t.add_column("A");
            t.add_column("B");
            t.add_row({"1", "2"});
            t.add_row({"3", "4"});
            t.caption("My Caption");
            show_table(std::move(t));
        }
        {
            rich::table::Table t;
            t.row_styles({rich::parse_style_def("dim"),
                          rich::parse_style_def("bold")});
            t.add_column("A");
            t.add_column("B");
            t.add_row({"1", "2"});
            t.add_row({"3", "4"});
            t.add_row({"5", "6"});
            show_table(std::move(t));
        }
        {
            rich::table::Table t(rich::table::TableOptions{.expand = true});
            t.add_column("A");
            t.add_column("B");
            t.add_row({"1", "2"});
            show_table(std::move(t));
        }
        {
            rich::table::Table t;
            t.add_column("A");
            t.add_column("B");
            t.add_row({"1", "2"});
            t.add_section();
            t.add_row({"3", "4"});
            show_table(std::move(t));
        }
        {
            rich::table::Table t(
                rich::table::TableOptions{.expand = true, .box = rich::box::SQUARE});
            t.add_column("A");
            t.add_column("B");
            t.add_column("C");
            t.add_row({"x", "y", "z"});
            show_table(std::move(t));
        }
        return 0;
    } else if (test_name == "panel_extras") {
        // subtitle / style / border_style / expand / fit / height — raw SGR
        // (styles are byte-visible; geometry covers expand/fit/height).
        std::ostringstream p_oss;
        rich::ConsoleOptions p_opts{.width = 60, .height = 25, .record = false,
                                    .legacy_windows = false,
                                    .force_terminal = true,
                                    .color_system = rich::ColorSystem::Truecolor,
                                    .no_color = false};
        rich::Console pc(p_oss, p_opts);
        auto pt = [](const char* s) { return rich::AnyRenderable(rich::Text{std::string(s), {}}); };
        pc.print(rich::AnyRenderable(rich::panel::Panel(pt("Hello World"),
                                                        rich::panel::PanelOptions{.style = "red"})));
        pc.print(rich::AnyRenderable(rich::panel::Panel(pt("Content here"),
                                                        rich::panel::PanelOptions{.subtitle = "Subtitle", .style = "dim"})));
        pc.print(rich::AnyRenderable(rich::panel::Panel::fit(pt("Short"))));
        pc.print(rich::AnyRenderable(rich::panel::Panel::fit(pt("Shorter content"),
                                                             rich::panel::PanelOptions{.title = "FitTitle"})));
        pc.print(rich::AnyRenderable(rich::panel::Panel(pt("X"),
                                                        rich::panel::PanelOptions{.width = 30, .subtitle = "Sub"})));
        pc.print(rich::AnyRenderable(rich::panel::Panel(pt("Line"),
                                                        rich::panel::PanelOptions{.height = 6, .title = "H"})));
        pc.print(rich::AnyRenderable(rich::panel::Panel(pt("A"),
                                                        rich::panel::PanelOptions{.style = "bold", .border_style = "red"})));
        pc.print(rich::AnyRenderable(rich::panel::Panel(pt("No Style Plain"))));
        std::cout << p_oss.str();
        return 0;
    } else if (test_name == "rule_extras") {
        // characters / style / align — raw SGR (dash style is byte-visible).
        std::ostringstream r_oss;
        rich::ConsoleOptions r_opts{.width = 60, .height = 25, .record = false,
                                    .legacy_windows = false,
                                    .force_terminal = true,
                                    .color_system = rich::ColorSystem::Truecolor,
                                    .no_color = false};
        rich::Console rc(r_oss, r_opts);
        rc.print(rich::AnyRenderable(rich::Rule{std::string("My Rule"), 0}));
        rc.print(rich::AnyRenderable(rich::Rule{std::string("X"), 0, "*"}));
        rc.print(rich::AnyRenderable(rich::Rule{std::string("X"), 0, std::string("\xe2\x94\x81")}));
        rc.print(rich::AnyRenderable(rich::Rule{std::string("X"), 0, {}, "red"}));
        rc.print(rich::AnyRenderable(rich::Rule{std::string("X"), 0, {}, "bold"}));
        rc.print(rich::AnyRenderable(rich::Rule{std::string("X"), 0, {}, {}, rich::Rule::Align::Left}));
        rc.print(rich::AnyRenderable(rich::Rule{std::string("X"), 0, {}, {}, rich::Rule::Align::Right}));
        rc.print(rich::AnyRenderable(rich::Rule{std::string(), 0, "*"}));
        rc.print(rich::AnyRenderable(rich::Rule{std::string(50, 'A'), 0}));
        rc.print(rich::AnyRenderable(rich::Rule{std::string(50, 'B'), 0, {}, {}, rich::Rule::Align::Left}));
        rc.print(rich::AnyRenderable(rich::Rule{std::string(50, 'C'), 0, {}, {}, rich::Rule::Align::Right}));
        rc.print(rich::AnyRenderable(rich::Rule{std::string("[bold]Bold[/]"), 0}));
        rc.print(rich::AnyRenderable(rich::Rule{std::string("Mix"), 0, "--", "green"}));
        std::cout << r_oss.str();
        return 0;
    } else if (test_name == "tree_extras") {
        // guide_style / expanded / hide_root — raw SGR (guide colors are
        // byte-visible; theme tree/tree.line are null on both sides).
        std::ostringstream t_oss;
        rich::ConsoleOptions t_opts{.width = 60, .height = 25, .record = false,
                                    .legacy_windows = false,
                                    .force_terminal = true,
                                    .color_system = rich::ColorSystem::Truecolor,
                                    .no_color = false};
        rich::Console tc(t_oss, t_opts);
        using TO = rich::tree::Tree::Options;
        {
            rich::tree::Tree t(std::string("root"));
            auto& a = t.add(std::string("alpha"));
            a.add(std::string("alpha-1"));
            a.add(std::string("alpha-2"));
            t.add(std::string("beta"));
            t.add(std::string("gamma"));
            tc.print(rich::AnyRenderable(t));
        }
        {
            rich::tree::Tree t(std::string("root"), TO{.guide_style = std::string("red")});
            t.add(std::string("a"));
            t.add(std::string("b"));
            tc.print(rich::AnyRenderable(t));
        }
        {
            rich::tree::Tree t(std::string("root"), TO{.guide_style = std::string("bold")});
            t.add(std::string("a")).add(std::string("a-1"));
            t.add(std::string("b"));
            tc.print(rich::AnyRenderable(t));
        }
        {
            rich::tree::Tree t(std::string("root"));
            auto& a = t.add(std::string("alpha"), TO{.expanded = false});
            a.add(std::string("hidden-1"));
            a.add(std::string("hidden-2"));
            t.add(std::string("beta"));
            tc.print(rich::AnyRenderable(t));
        }
        {
            rich::tree::Tree t(std::string("root"), TO{.hide_root = true});
            t.add(std::string("a"));
            t.add(std::string("b"));
            tc.print(rich::AnyRenderable(t));
        }
        {
            rich::tree::Tree t(std::string("[bold]root[/]"), TO{.style = std::string("green")});
            auto& c1 = t.add(std::string("child"), TO{.guide_style = std::string("magenta")});
            c1.add(std::string("grand"));
            t.add(std::string("leaf"));
            tc.print(rich::AnyRenderable(t));
        }
        {
            rich::tree::Tree t(std::string("only"));
            t.add(std::string("kid"));
            tc.print(rich::AnyRenderable(t));
        }
        std::cout << t_oss.str();
        return 0;
    } else if (test_name == "columns_extras") {
        // expand / equal / column_first / right_to_left / width / title /
        // padding — raw SGR (str items pass through console.render_str in
        // Python, so the 18-digit items are auto-highlighted by the default
        // ReprHighlighter; C++ applies the same ReprHighlighter directly).
        std::ostringstream t_oss;
        rich::ConsoleOptions t_opts{.width = 60, .height = 25, .record = false,
                                    .legacy_windows = false,
                                    .force_terminal = true,
                                    .color_system = rich::ColorSystem::Truecolor,
                                    .no_color = false};
        rich::Console tc(t_oss, t_opts);
        using CO = rich::decorator::Columns::Options;
        auto items3 = []() {
            std::vector<rich::AnyRenderable> v;
            for (auto s : {"aaaa", "bb", "cccccccc"})
                v.push_back(rich::AnyRenderable(rich::Text{std::string(s), {}}));
            return v;
        };
        auto items6 = []() {
            std::vector<rich::AnyRenderable> v;
            for (auto s : {"i1", "i2", "i3", "i4", "i5", "i6"})
                v.push_back(rich::AnyRenderable(rich::Text{std::string(s), {}}));
            return v;
        };
        auto items5x18 = []() {
            std::vector<rich::AnyRenderable> v;
            for (int n = 1; n <= 5; ++n)
                v.push_back(rich::AnyRenderable(
                    rich::ReprHighlighter()(std::string(18, '0' + n))));
            return v;
        };
        tc.print(rich::AnyRenderable(rich::decorator::Columns(items3())));
        tc.print(rich::AnyRenderable(rich::decorator::Columns(items3(), CO{.expand = true})));
        tc.print(rich::AnyRenderable(rich::decorator::Columns(items3(), CO{.equal = true})));
        tc.print(
            rich::AnyRenderable(rich::decorator::Columns(items6(), CO{.column_first = true})));
        tc.print(rich::AnyRenderable(
            rich::decorator::Columns(items6(), CO{.expand = true, .column_first = true})));
        tc.print(rich::AnyRenderable(rich::decorator::Columns(items5x18())));
        tc.print(rich::AnyRenderable(rich::decorator::Columns(
            items3(), CO{.align = rich::decorator::Columns::Align::Right})));
        {
            auto itemsMix = []() {
                std::vector<rich::AnyRenderable> v;
                for (auto ch : {'a', 'b', 'c', 'd', 'e', 'f'})
                    v.push_back(rich::AnyRenderable(rich::Text{std::string(12, ch), {}}));
                for (auto s : {"gg", "hh"})
                    v.push_back(rich::AnyRenderable(rich::Text{std::string(s), {}}));
                return v;
            };
            tc.print(rich::AnyRenderable(rich::decorator::Columns(
                itemsMix(), CO{.align = rich::decorator::Columns::Align::Right})));
            tc.print(rich::AnyRenderable(rich::decorator::Columns(
                itemsMix(), CO{.align = rich::decorator::Columns::Align::Center})));
        }
        tc.print(rich::AnyRenderable(rich::decorator::Columns(items3(), CO{.title = "Titled"})));
        tc.print(rich::AnyRenderable(rich::decorator::Columns(items3(), CO{.width = 6})));
        tc.print(rich::AnyRenderable(rich::decorator::Columns(items3(), CO{.right_to_left = true})));
        tc.print(rich::AnyRenderable(rich::decorator::Columns(
            items3(), CO{.pad_top = 1, .pad_right = 2, .pad_bottom = 1, .pad_left = 2})));
        tc.print(rich::AnyRenderable(rich::decorator::Columns(
            items3(),
            CO{.pad_top = 1, .pad_right = 2, .pad_bottom = 1, .pad_left = 2, .expand = true})));
        {
            std::vector<rich::AnyRenderable> styled;
            styled.push_back(rich::AnyRenderable(
                rich::Text{std::string("aaaa"), tc.get_style("red")}));
            styled.push_back(rich::AnyRenderable(
                rich::Text{std::string("bb"), tc.get_style("bold green")}));
            styled.push_back(rich::AnyRenderable(
                rich::Text{std::string("cccccccc"), tc.get_style("cyan")}));
            tc.print(rich::AnyRenderable(rich::decorator::Columns(std::move(styled))));
        }
        std::cout << t_oss.str();
        return 0;
    } else if (test_name == "progress_extras") {
        // Progress custom columns + pulse + transfer mode — mirrors the
        // Python oracle: record console, explicit no_color=false (pulse
        // renders all-bar segments), frozen clocks.
        std::ostringstream t_oss;
        auto render_prog = [&](const rich::progress::Progress& p, double now) {
            rich::ConsoleOptions o{.width = 60, .height = 25, .record = true,
                                   .legacy_windows = false, .no_color = false};
            std::ostringstream pc_oss;
            rich::Console pc(pc_oss, o);
            auto segs = p.render(pc, rich::Options{.width = 60}, now);
            for (auto& s : segs) t_oss << s.text;
        };

        // (a) default columns, frozen clock: single sample -> no speed/ETA.
        {
            rich::progress::Progress p;
            int t1 = p.add_task("Downloading", 100, 0, true, true, 100.0);
            int t2 = p.add_task("Processing", 200, 0, true, true, 100.0);
            rich::progress::Progress::Update u;
            u.completed = 42;
            p.update(t1, u, 100.0);
            u.completed = 137;
            p.update(t2, u, 100.0);
            render_prog(p, 100.0);
        }

        // (b) rolling speed: samples (105, 42), (110, 42) -> 8.4 it/s ->
        // ETA ceil(16 / 8.4) = 2 -> "0:00:02".
        {
            rich::progress::Progress p;
            int t1 = p.add_task("Downloading", 100, 0, true, true, 100.0);
            int t2 = p.add_task("Processing", 200, 0, true, true, 100.0);
            rich::progress::Progress::Update u;
            u.completed = 42;
            p.update(t1, u, 105.0);
            u.completed = 137;
            p.update(t2, u, 105.0);
            u.completed = 84;
            p.update(t1, u, 110.0);
            render_prog(p, 110.0);
        }

        // (c) pulse: unstarted task + total=None task.
        {
            rich::progress::Progress p;
            int t1 = p.add_task("Waiting", std::nullopt, 0, true, false, 3.5);
            int t2 = p.add_task("Started", 50, 0, true, true, 0.0);
            rich::progress::Progress::Update u;
            u.completed = 10;
            p.update(t2, u, 0.0);
            render_prog(p, 3.5);
        }

        // (d) transfer columns.
        {
            std::vector<rich::progress::Progress::AnyColumn> cols = {
                rich::progress::Progress::AnyColumn::of(
                    std::make_shared<rich::progress::SpinnerColumn>(
                        rich::progress::SpinnerColumn())),
                rich::progress::Progress::AnyColumn::of(
                    std::make_shared<rich::progress::TextColumn>(
                        rich::progress::TextColumn(
                            "[progress.description]{task.description}"))),
                rich::progress::Progress::AnyColumn::of(
                    std::make_shared<rich::progress::BarColumn>(
                        rich::progress::BarColumn())),
                rich::progress::Progress::AnyColumn::of(
                    std::make_shared<rich::progress::TaskProgressColumn>(
                        rich::progress::TaskProgressColumn())),
                rich::progress::Progress::AnyColumn::of(
                    std::make_shared<rich::progress::FileSizeColumn>(
                        rich::progress::FileSizeColumn())),
                rich::progress::Progress::AnyColumn::of(
                    std::make_shared<rich::progress::TransferSpeedColumn>(
                        rich::progress::TransferSpeedColumn())),
                rich::progress::Progress::AnyColumn::of(
                    std::make_shared<rich::progress::TimeRemainingColumn>(
                        rich::progress::TimeRemainingColumn())),
            };
            rich::progress::Progress p(cols, false, 30.0);
            int t = p.add_task("Downloading file", 1500000, 0, true, true, 0.0);
            rich::progress::Progress::Update u;
            u.completed = 750000;
            p.update(t, u, 60.0);
            render_prog(p, 60.0);
        }

        // (e) str column + elapsed + total filesize + MofN + download +
        // compact elapsed-when-finished ETA; expand=True.
        {
            std::vector<rich::progress::Progress::AnyColumn> cols = {
                rich::progress::Progress::AnyColumn::of_text("{task.description}"),
                rich::progress::Progress::AnyColumn::of(
                    std::make_shared<rich::progress::TimeElapsedColumn>(
                        rich::progress::TimeElapsedColumn())),
                rich::progress::Progress::AnyColumn::of(
                    std::make_shared<rich::progress::TotalFileSizeColumn>(
                        rich::progress::TotalFileSizeColumn())),
                rich::progress::Progress::AnyColumn::of(
                    std::make_shared<rich::progress::MofNCompleteColumn>(
                        rich::progress::MofNCompleteColumn())),
                rich::progress::Progress::AnyColumn::of(
                    std::make_shared<rich::progress::DownloadColumn>(
                        rich::progress::DownloadColumn())),
                rich::progress::Progress::AnyColumn::of(
                    std::make_shared<rich::progress::TimeRemainingColumn>(
                        rich::progress::TimeRemainingColumn(true, true))),
            };
            rich::progress::Progress p(cols, true, 30.0);
            int t1 = p.add_task("Copy", 2500000, 0, true, true, 0.0);
            int t2 = p.add_task("Done", 8, 0, true, true, 0.0);
            rich::progress::Progress::Update u;
            u.completed = 1250000;
            p.update(t1, u, 30.0);
            p.advance(t2, 8, 30.0);
            render_prog(p, 30.0);
        }
        std::cout << t_oss.str();
        return 0;
    } else if (test_name == "spinner_registry") {
        // Spinner registry: frame selection math, ctor speed, attached text,
        // deferred speed update, custom frames/interval — mirrors the Python
        // oracle (Spinner.render exercised directly).
        std::ostringstream t_oss;
        auto show = [&](rich::status::Spinner& sp, double t) {
            t_oss << sp.render(t) << "\n";
        };

        rich::status::Spinner sp("dots");
        sp.render(0.0);
        show(sp, 0.24);
        sp = rich::status::Spinner("line");
        sp.render(0.0);
        show(sp, 0.39);
        sp = rich::status::Spinner("dots12");
        sp.render(0.0);
        show(sp, 0.80);
        sp = rich::status::Spinner("weather");
        sp.render(0.0);
        show(sp, 0.75);
        sp = rich::status::Spinner("material");
        sp.render(0.0);
        show(sp, 1.68);
        sp = rich::status::Spinner("point");
        sp.render(0.0);
        show(sp, 0.20);

        sp = rich::status::Spinner("dots", std::string(), std::string(), 2.0);
        sp.render(0.0);
        show(sp, 0.24);

        sp = rich::status::Spinner("line", "loading...");
        sp.render(0.0);
        show(sp, 0.26);

        sp = rich::status::Spinner("dots");
        sp.render(0.0);
        sp.set_speed(4.0);
        show(sp, 0.08);
        show(sp, 0.16);

        sp = rich::status::Spinner("dots");
        sp.set_frames({{"a"}, {"b"}, {"c"}}, 100.0);
        sp.render(0.0);
        show(sp, 0.1);
        show(sp, 0.2);

        std::cout << t_oss.str();
        return 0;
    } else if (test_name == "live_extras") {
        // Live extras: non-TTY final frame (no trailing newline), transient /
        // screen suppression, vertical overflow (crop/ellipsis/visible) via the
        // LiveRender-equivalent static helper. Mirrors oracle_gen live_extras.
        std::ostringstream out;

        auto text = [](const std::string& s) {
            return rich::AnyRenderable(rich::Text{s, {}});
        };

        // (a) basic final frame
        {
            rich::live::Live live(text("hello"), 4.0, false, out, 60);
            live.start();
            live.stop();
        }
        std::cout << out.str();
        out.str("");
        std::cout << '\n';

        // (b) update then stop
        {
            rich::live::Live live(text("one"), 4.0, false, out, 60);
            live.update_renderable(text("two"));
            live.start();
            live.stop();
        }
        std::cout << out.str();
        out.str("");
        std::cout << '\n';

        // (c) transient -> nothing
        {
            rich::live::Live live(text("gone"), 4.0, true, out, 60);
            live.start();
            live.stop();
        }
        std::cout << out.str();
        out.str("");
        std::cout << '\n';

        // (d) screen -> transient forced -> nothing
        {
            rich::live::Live live(text("screen"), 4.0, false, out, 60, true, "ellipsis");
            live.start();
            live.stop();
        }
        std::cout << out.str();
        out.str("");
        std::cout << '\n';

        // (e) vertical overflow (height 25, width 60)
        auto big = [] {
            std::vector<std::vector<rich::Segment>> lines;
            for (int i = 0; i < 30; ++i) {
                char buf[8];
                std::snprintf(buf, sizeof buf, "L%02d", i);
                lines.push_back({{std::string(buf), {}}});
            }
            return lines;
        };
        auto emit = [](std::vector<std::vector<rich::Segment>> lines) {
            for (size_t i = 0; i < lines.size(); ++i) {
                for (const auto& s : lines[i]) std::cout << s.text;
                if (i + 1 < lines.size()) std::cout << '\n';
            }
        };
        emit(rich::live::Live::apply_vertical_overflow(big(), 25, "crop", 60));
        std::cout << '\n';
        emit(rich::live::Live::apply_vertical_overflow(big(), 25, "ellipsis", 60));
        std::cout << '\n';
        emit(rich::live::Live::apply_vertical_overflow(big(), 25, "visible", 60));
        std::cout << '\n';
        emit(rich::live::Live::apply_vertical_overflow(
            std::vector<std::vector<rich::Segment>>{{{std::string("short"), {}}}},
            25, "crop", 60));
        std::cout << '\n';

        // (f) get_renderable equivalent: fresh renderable per live instance
        {
            rich::live::Live l1(text("first"), 4.0, false, out, 60);
            l1.start();
            l1.stop();
            rich::live::Live l2(text("second"), 4.0, false, out, 60);
            l2.start();
            l2.stop();
        }
        std::cout << out.str();
        std::cout << '\n';
        return 0;
    } else if (test_name == "pretty_sgr") {
        // Raw SGR: Pretty's ReprHighlighter coloring (repr.brace/number/str/
        // bool/none/ellipsis + repr.indent guides) — mirrors oracle_gen.
        rich::ConsoleOptions p_opts{.width = 60, .force_terminal = true,
                                    .color_system = rich::ColorSystem::Truecolor,
                                    .no_color = false, .legacy_windows = false};
        std::ostringstream p_oss;
        rich::Console pc(p_oss, p_opts);
        pc.print(rich::pretty::pretty(std::make_tuple(1, std::string("two"), true,
                                                           std::nullptr_t{})));
        pc.print(rich::pretty::pretty(std::map<std::string, int>{{"a", 1}, {"b", 2}}));
        std::vector<int> pv;
        for (int i = 12; i < 27; ++i) pv.push_back(i);
        pc.print(rich::pretty::pretty(pv));
        {
            rich::pretty::PrettyOptions po;
            po.max_length = 3;
            pc.print(rich::pretty::pretty(std::vector<int>{1, 2, 3, 4, 5, 6}, po));
        }
        {
            rich::pretty::PrettyOptions po;
            po.indent_guides = true;
            pc.print(rich::pretty::pretty(std::vector<std::tuple<std::string, int>>{
                                                     {std::string(8, 'a'), 1},
                                                     {std::string(8, 'b'), 2}},
                                          po));
        }
        pc.print(rich::pretty::pretty(
            std::make_tuple(0.25, -2.5e3, std::string("end"))));
        {
            rich::pretty::PrettyOptions po;
            po.expand_all = true;
            pc.print(rich::pretty::pretty(std::map<std::string, std::map<std::string, std::vector<std::optional<double>>>>{
                {"k", {{"deep", {0.25, std::nullopt}}}}},
                                         po));
        }
        std::cout << p_oss.str();
        return 0;
    } else {
        std::cerr << "Unknown test: " << test_name << "\n";
        return 1;
    }

    std::cout << console.export_text();
    return 0;
}
