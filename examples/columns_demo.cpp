#include <rich_core.h>
#include <panel/Panel.h>
#include <panel/Columns.h>
#include <iostream>
#include <string>
#include <vector>

struct Country {
    std::string name;
    std::string capital;
    std::string population;
    std::string area;
    std::string gdp;
};

int main() {
    using namespace rich;
    using namespace rich::decorator;

    Console console;

    // 5 countries of varying widths to demonstrate multi-row left-alignment
    std::vector<Country> countries = {
        {"France",         "Paris",      "67.75M",  "643,801 km²", "$2.96T"},
        {"Japan",          "Tokyo",      "125.7M",  "377,975 km²", "$4.94T"},
        {"United States",  "Washington", "331.9M",  "9.834M km²",  "$23.3T"},
        {"Canada",         "Ottawa",     "38.25M",  "9.985M km²",  "$1.99T"},
        {"United Kingdom", "London",     "67.33M",  "242,495 km²", "$3.13T"}
    };

    std::vector<AnyRenderable> country_cards;

    for (const auto& c : countries) {
        std::string card_content =
            "[dim]Capital:[/]    [bright_white]" + c.capital + "[/]\n"
            "[dim]Population:[/] [bright_green]" + c.population + "[/]\n"
            "[dim]Area:[/]       [bright_cyan]" + c.area + "[/]\n"
            "[dim]GDP:[/]        [bright_yellow]" + c.gdp + "[/]";

        country_cards.push_back(
            AnyRenderable(StyledText{card_content}) |
            rich::panel::panel({.title = "[bold bright_white]" + c.name + "[/]", .show_border = true})
        );
    }

    console.rule("Countries Info (Multi-Row Columns Demo)");
    // max_columns = 3: row 1 gets 3 cards, row 2 gets 2 cards left-aligned
    console.print(columns(std::move(country_cards), /*spacing=*/3, /*max_columns=*/3));
    console.rule();

    return 0;
}
