// aris-ui tests — widget ID hashing.
#include "test_main.hpp"
#include "ui/core/id.hpp"
#include <cstring>

using namespace ui;

TEST(fnv1a_reference_vector) {
    // FNV-1a 64 of empty data is the offset basis.
    CHECK_EQ(HashBytes(14695981039346656037ULL, "", 0), 14695981039346656037ULL);
    // FNV-1a 64 of "a" — published reference value 0xAF63DC4C8601EC8C.
    CHECK_EQ(HashBytes(14695981039346656037ULL, "a", 1), 12638187200555641996ULL);
}

TEST(hash_deterministic_and_separable) {
    WidgetId a = HashString(1, "button");
    WidgetId b = HashString(1, "button");
    WidgetId c = HashString(1, "buttons");
    CHECK_EQ(a, b);
    CHECK(a != c);
    CHECK(HashString(1, "a") != HashString(2, "a"));   // seed matters (ID stack)
}

TEST(hash_combine_pointer) {
    int x = 0;
    WidgetId p1 = HashPointer(1, &x);
    WidgetId p2 = HashPointer(1, &x);
    WidgetId p3 = HashPointer(1, &x + 1);
    CHECK_EQ(p1, p2);
    CHECK(p1 != p3);
    CHECK(p1 != kInvalidId);
}

TEST(hash_combine_value_chain) {
    WidgetId a = HashCombine(HashCombine(1, 10), 20);
    WidgetId b = HashCombine(HashCombine(1, 10), 20);
    WidgetId c = HashCombine(HashCombine(1, 20), 10);
    CHECK_EQ(a, b);
    CHECK(a != c);   // order-dependent, like a scope path
}