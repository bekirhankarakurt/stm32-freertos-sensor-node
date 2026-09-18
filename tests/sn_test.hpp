/**
 * @file sn_test.hpp
 * @brief Minimal dependency-free unit test harness (one executable per suite, run by CTest).
 */
#ifndef SN_TEST_HPP
#define SN_TEST_HPP

#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace sn_test {

/** Registered test case. */
struct Case {
    const char* name;
    void (*fn)();
    Case* next;
};

/** Global registry and failure counter. */
struct Registry {
    Case* head{nullptr};
    int failures{0};
    static Registry& get()
    {
        static Registry r;
        return r;
    }
};

/** Registers a test at static-initialisation time. */
struct Registrar {
    Registrar(Case* c)
    {
        c->next = Registry::get().head;
        Registry::get().head = c;
    }
};

/** Record a failed expectation. */
inline void fail(const char* file, int line, const char* expr)
{
    std::fprintf(stderr, "%s:%d: FAILED: %s\n", file, line, expr);
    ++Registry::get().failures;
}

/** Run all registered tests in declaration order. */
inline int run_all()
{
    Case* list[512];
    int n = 0;
    for (Case* c = Registry::get().head; (c != nullptr) && (n < 512); c = c->next) {
        list[n++] = c;
    }
    for (int i = n - 1; i >= 0; --i) {
        const int before = Registry::get().failures;
        list[i]->fn();
        std::printf("[%s] %s\n", (Registry::get().failures == before) ? " OK " : "FAIL", list[i]->name);
    }
    std::printf("%d test(s), %d failure(s)\n", n, Registry::get().failures);
    return (Registry::get().failures == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

}  // namespace sn_test

#define SN_TEST_CAT2(a, b) a##b
#define SN_TEST_CAT(a, b) SN_TEST_CAT2(a, b)

/** Define a test case. */
#define TEST(name)                                                                              \
    static void SN_TEST_CAT(test_fn_, name)();                                                  \
    static sn_test::Case SN_TEST_CAT(test_case_, name){#name, SN_TEST_CAT(test_fn_, name), nullptr}; \
    static sn_test::Registrar SN_TEST_CAT(test_reg_, name){&SN_TEST_CAT(test_case_, name)};     \
    static void SN_TEST_CAT(test_fn_, name)()

/** Non-fatal expectation. */
#define CHECK(expr)                                  \
    do {                                             \
        if (!(expr)) {                               \
            sn_test::fail(__FILE__, __LINE__, #expr); \
        }                                            \
    } while (0)

/** Fatal expectation: returns from the test case. */
#define REQUIRE(expr)                                \
    do {                                             \
        if (!(expr)) {                               \
            sn_test::fail(__FILE__, __LINE__, #expr); \
            return;                                  \
        }                                            \
    } while (0)

/** Equality expectation printing both values as integers. */
#define CHECK_EQ(a, b)                                                                              \
    do {                                                                                            \
        const long long sn_a = static_cast<long long>(a);                                           \
        const long long sn_b = static_cast<long long>(b);                                           \
        if (sn_a != sn_b) {                                                                         \
            std::fprintf(stderr, "%s:%d: FAILED: %s == %s (%lld vs %lld)\n", __FILE__, __LINE__, #a, \
                         #b, sn_a, sn_b);                                                           \
            ++sn_test::Registry::get().failures;                                                    \
        }                                                                                           \
    } while (0)

/** Entry point for a suite. */
#define SN_TEST_MAIN() \
    int main() { return sn_test::run_all(); }

#endif  // SN_TEST_HPP
