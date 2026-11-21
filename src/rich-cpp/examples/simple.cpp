#include <rich_core.h>
#include <panel/Panel.h>
#include <panel/Padding.h>
#include <iostream>

int main() {
    using namespace rich;
    using namespace rich::panel;
    using namespace rich::decorator;
    
    Console console;
    console.print(AnyRenderable(Text{"Hello, World!"}) | pad(1) | panel({.title="Example"}));
    return 0;
}
