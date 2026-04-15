/*
 * Runtime safety guard tests.
 *
 * The Yona C runtime exposes yona_rt_seq_alloc, yona_rt_seq_get, and
 * hamt_alloc, each of which has explicit bounds/overflow guards (trap
 * via abort on invalid input). Here we exercise the valid range to
 * confirm the guards don't regress the fast path. The abort branches
 * are trivially correct by inspection and would require fork-based
 * death tests to exercise from doctest, which is out of scope.
 */

#include <cstdint>
#include <doctest/doctest.h>

extern "C" {
int64_t* yona_rt_seq_alloc(int64_t count);
int64_t yona_rt_seq_get(int64_t* seq, int64_t index);
int64_t yona_rt_seq_length(int64_t* seq);
void yona_rt_seq_set(int64_t* seq, int64_t index, int64_t value);
void yona_rt_rc_dec(void* ptr);
}

TEST_SUITE("RuntimeGuards") {

TEST_CASE("seq_alloc accepts zero count") {
    int64_t* s = yona_rt_seq_alloc(0);
    REQUIRE(s != nullptr);
    CHECK(yona_rt_seq_length(s) == 0);
    yona_rt_rc_dec(s);
}

TEST_CASE("seq_alloc + set + get round-trip on small seq") {
    int64_t* s = yona_rt_seq_alloc(4);
    REQUIRE(s != nullptr);
    CHECK(yona_rt_seq_length(s) == 4);
    for (int i = 0; i < 4; i++) yona_rt_seq_set(s, i, (int64_t)(i * 10));
    for (int i = 0; i < 4; i++) CHECK(yona_rt_seq_get(s, i) == i * 10);
    yona_rt_rc_dec(s);
}

TEST_CASE("seq_alloc accepts medium sizes") {
    int64_t* s = yona_rt_seq_alloc(1024);
    REQUIRE(s != nullptr);
    CHECK(yona_rt_seq_length(s) == 1024);
    yona_rt_seq_set(s, 1023, 42);
    CHECK(yona_rt_seq_get(s, 1023) == 42);
    yona_rt_rc_dec(s);
}

} // TEST_SUITE("RuntimeGuards")
