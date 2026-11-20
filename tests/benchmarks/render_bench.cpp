#include <rich_core.h>
#include <iostream>
#include <chrono>

int main() {
    using namespace rich;
    Console console;
    AnyRenderable content = Text{"Performance Test"};
    
    auto start = std::chrono::high_resolution_clock::now();
    for(int i = 0; i < 10000; ++i) {
        content.render(console, Options{});
    }
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff = end - start;
    std::cout << "Render time: " << diff.count() << "s" << std::endl;
    return 0;
}
