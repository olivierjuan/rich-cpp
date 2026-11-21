#include <rich_core.h>
#include <panel/Table.h>
#include <iostream>
#include <fstream>

int main() {
    using namespace rich;
    using namespace rich::table;
    
    // Create a simple table for oracle testing
    Table t({{"Name"}, {"Age"}});
    t.add_row({AnyRenderable(Text{"Alice"}), AnyRenderable(Text{"30"})});
    t.add_row({AnyRenderable(Text{"Bob"}), AnyRenderable(Text{"25"})});
    
    std::ofstream oracle("oracle.txt");
    Console console(oracle);
    console.print(AnyRenderable(t));
    
    return 0;
}
