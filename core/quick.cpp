#include <rich_core.h>
#include <panel/Panel.h>
#include <panel/Padding.h>
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
    using namespace rich::panel;
    using namespace rich::decorator;
    
    Console console;
    
    // Demonstrate pipe composition
    // Text is implicitly convertible to AnyRenderable
    auto content = AnyRenderable(StyledText{"[bold][red]Hello[/red][/bold] from [green]C++ Rich![/green]"});
    
    console.print(content | pad(1) | panel("Demo"));
    
    return 0;
}
