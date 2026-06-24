#include "testkit.hpp"

int main() {
    std::printf("Bourse :: order book correctness suite\n\n");
    return testkit::run_all();
}