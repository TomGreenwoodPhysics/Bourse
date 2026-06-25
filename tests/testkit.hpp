#pragma once
#include <cstdio>
#include <string>
#include <vector>

namespace testkit {

struct Stats { int checks = 0; int failures = 0; };
struct Case  { std::string name; void (*fn)(Stats&); };

inline std::vector<Case>& registry() { static std::vector<Case> cases; return cases; }

struct Registrar {
    Registrar(const char* name, void (*fn)(Stats&)) { registry().push_back(Case{name, fn}); }
};

inline int run_all() {
    int total_checks = 0, total_failures = 0, failed_cases = 0;
    for (const auto& c : registry()) {
        Stats s; c.fn(s);
        total_checks += s.checks; total_failures += s.failures;
        const bool ok = (s.failures == 0);
        failed_cases += ok ? 0 : 1;
        std::printf("  [%s] %-46s (%d checks)\n", ok ? "PASS" : "FAIL", c.name.c_str(), s.checks);
    }
    std::printf("\n%d cases, %d checks, %d failures\n",
                static_cast<int>(registry().size()), total_checks, total_failures);
    std::printf("%s\n", failed_cases == 0 ? "ALL GREEN" : "SUITE FAILED");
    return failed_cases == 0 ? 0 : 1;
}

} // namespace testkit

#define TEST_CASE(NAME)                                                         \
    static void NAME(testkit::Stats&);                                          \
    static testkit::Registrar reg_##NAME(#NAME, &NAME);                         \
    static void NAME(testkit::Stats& _stats)

#define CHECK(COND)                                                             \
    do {                                                                        \
        ++_stats.checks;                                                        \
        if (!(COND)) {                                                          \
            ++_stats.failures;                                                  \
            std::printf("      FAIL %s:%d  CHECK(%s)\n",                        \
                        __FILE__, __LINE__, #COND);                             \
        }                                                                       \
    } while (0)

// end of testkit.hpp