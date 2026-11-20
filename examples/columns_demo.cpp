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

    std::vector<Country> countries = {
        {"France",         "Paris",      "67.75M",  "643,801 km²", "$2.96T"},
        {"Japan",          "Tokyo",      "125.7M",  "377,975 km²", "$4.94T"},
        {"United States",  "Washington", "331.9M",  "9.834M km²",  "$23.3T"}
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

    console.rule("Countries Info (Columns Demo)");
    console.print(columns(std::move(country_cards), /*spacing=*/3));
    console.rule();

    return 0;
}
