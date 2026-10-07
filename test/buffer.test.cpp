// g++ -std=c++20 buffer.test.cpp ../external/avant-buffer/buffer.cpp -o buffer.exe
#include <cstring>
#include <cstdio>
#include <stdexcept>

#include "../external/avant-buffer/buffer.h"

using avant::buffer::buffer;

static int g_failed = 0;

#define CHECK(cond, msg)                                                   \
    do                                                                     \
    {                                                                      \
        if (cond)                                                          \
        {                                                                  \
            std::printf("  [ok]   %s\n", msg);                             \
        }                                                                  \
        else                                                               \
        {                                                                  \
            ++g_failed;                                                    \
            std::printf("  [FAIL] %s (%s:%d)\n", msg, __FILE__, __LINE__); \
        }                                                                  \
    } while (0)

static const char *k_msg = "hello world, this is a read-me buffer payload!";

void test_write_read_roundtrip()
{
    std::printf("write/read roundtrip\n");
    buffer b;
    const std::size_t n = std::strlen(k_msg);
    const auto got = b.write(k_msg, n);
    CHECK(got == n, "write returns full size");
    CHECK(b.can_readable_size() == n, "readable == written");

    char out[256] = {0};
    const auto rd = b.read(out, n);
    CHECK(rd == n, "read returns full size");
    CHECK(std::memcmp(out, k_msg, n) == 0, "content matches");
    CHECK(b.can_readable_size() == 0, "empty after full read");
}

void test_read_partial_and_short()
{
    std::printf("partial / short reads\n");
    buffer b;
    const std::size_t n = 10;
    char src[16];
    for (std::size_t i = 0; i < n; ++i)
    {
        src[i] = char('a' + i);
    }
    b.write(src, n);

    char out[16] = {0};
    CHECK(b.read(out, 4) == 4, "read 4");
    CHECK(out[0] == 'a' && out[3] == 'd', "first 4 bytes");
    CHECK(b.can_readable_size() == 6, "6 remain");

    CHECK(b.read(out, 100) == 6, "short read clamps to available");
    CHECK(out[0] == 'e' && out[5] == 'j', "remaining bytes");
    CHECK(b.read(out, 1) == 0, "read past end returns 0");
}

void test_copy_all()
{
    std::printf("copy_all\n");
    buffer b;
    const std::size_t n = std::strlen(k_msg);
    b.write(k_msg, n);

    char out[256] = {0};
    CHECK(b.copy_all(out, sizeof(out)) == n, "copy_all returns size");
    CHECK(std::memcmp(out, k_msg, n) == 0, "copy_all content");
    CHECK(b.can_readable_size() == n, "copy_all does not consume");

    char tiny[2] = {0};
    CHECK(b.copy_all(tiny, sizeof(tiny)) == 0, "copy_all too-small returns 0");
    CHECK(b.copy_all(nullptr, sizeof(out)) == 0, "copy_all null returns 0");
}

void test_read_ptr_move_n()
{
    std::printf("read_ptr_move_n\n");
    buffer b;
    char src[8] = {'1', '2', '3', '4', '5', '6', '7', '8'};
    b.write(src, 8);
    CHECK(b.read_ptr_move_n(3), "skip 3");
    CHECK(b.can_readable_size() == 5, "5 remain after skip");
    char out[16] = {0};
    CHECK(b.read(out, 5) == 5, "read 5 after skip");
    CHECK(std::memcmp(out, "45678", 5) == 0, "skipped content");
    CHECK(!b.read_ptr_move_n(99), "skip beyond readable fails");
    CHECK(b.read_ptr_move_n(0), "skip 0 is ok");
}

void test_set_limit_max()
{
    std::printf("set_limit_max\n");
    buffer b;
    CHECK(b.get_limit_max() == 1024, "default limit 1024");
    b.set_limit_max(2048);
    CHECK(b.get_limit_max() == 2048, "limit raised");
    b.set_limit_max(4096);
    CHECK(b.get_limit_max() == 4096, "limit raised again");
    b.set_limit_max(512);
    CHECK(b.get_limit_max() == 4096, "smaller limit ignored");
}

void test_write_shrink_bug_fixed()
{
    std::printf("shrink bug fixed (regression)\n");
    buffer b; // capacity 1024
    const std::size_t n = std::strlen(k_msg);
    b.write(k_msg, n);
    char out[64] = {0};
    CHECK(b.read(out, n) == n, "consume all -> readable 0");
    CHECK(out[0] == 'h', "consumed first byte");

    char big[200];
    for (std::size_t i = 0; i < 200; ++i)
    {
        big[i] = char('A' + i % 26);
    }
    const auto w = b.write(big, 200); // old code shrank 1024 -> 200 -> heap overflow
    CHECK(w == 200, "200-byte write accepted");
    CHECK(b.can_readable_size() == 200, "200 readable");

    char r[200] = {0};
    CHECK(b.read(r, 200) == 200, "read back 200");
    bool content_ok = true;
    for (std::size_t i = 0; i < 200; ++i)
    {
        if (r[i] != big[i])
        {
            content_ok = false;
            break;
        }
    }
    CHECK(content_ok, "content intact after shrink scenario");
}

void test_expand_across_write()
{
    std::printf("grow across many writes\n");
    buffer b; // 1024
    b.set_limit_max(1024 * 1024);
    char chunk[400];
    for (std::size_t i = 0; i < 400; ++i)
    {
        chunk[i] = char('x' + i % 8);
    }
    std::size_t total = 0;
    for (int i = 0; i < 10; ++i)
    {
        total += b.write(chunk, 400);
    }
    CHECK(total == 4000, "10x400 written");
    CHECK(b.can_readable_size() == 4000, "4000 readable after growth");

    char out[4096] = {0};
    CHECK(b.read(out, 4000) == 4000, "read all back");
    bool content_ok = true;
    for (std::size_t i = 0; i < 4000; ++i)
    {
        if (out[i] != chunk[i % 400])
        {
            content_ok = false;
            break;
        }
    }
    CHECK(content_ok, "content intact after growth");
}

void test_move_semantics()
{
    std::printf("move semantics\n");
    buffer a;
    const std::size_t n = std::strlen(k_msg);
    a.write(k_msg, n);
    CHECK(a.read_ptr_move_n(2), "skip 2"); // skip "he"

    buffer b = std::move(a);
    CHECK(b.can_readable_size() == n - 2, "moved buffer keeps data");
    char out[64] = {0};
    CHECK(b.read(out, n - 2) == n - 2, "read moved data");
    CHECK(std::memcmp(out, k_msg + 2, n - 2) == 0, "moved content correct");
    CHECK(a.can_readable_size() == 0, "source emptied after move");

    // use a fresh source so move-assign has data to transfer
    buffer src;
    src.write(k_msg, n);
    CHECK(src.read_ptr_move_n(2), "skip 2 in src");
    buffer c;
    c = std::move(src);
    CHECK(c.can_readable_size() == n - 2, "move-assign transfers data");
    CHECK(src.can_readable_size() == 0, "source emptied after move-assign");
}

void test_copy_semantics()
{
    std::printf("copy semantics (compact copy)\n");
    buffer a;
    const std::size_t n = std::strlen(k_msg);
    a.write(k_msg, n);
    CHECK(a.read_ptr_move_n(5), "skip 5"); // 5 consumed

    buffer b = a;
    CHECK(b.can_readable_size() == n - 5, "copy has remaining");
    char out[64] = {0};
    CHECK(b.read(out, n - 5) == n - 5, "read copy data");
    CHECK(std::memcmp(out, k_msg + 5, n - 5) == 0, "copy content correct");

    buffer c;
    c.write("zz", 2);
    c = a; // move-assign? no, a is lvalue -> copy
    CHECK(c.can_readable_size() == n - 5, "copy-assign has remaining");
}

void test_clear()
{
    std::printf("clear\n");
    buffer b;
    const std::size_t n = std::strlen(k_msg);
    b.write(k_msg, n);
    b.clear();
    CHECK(b.can_readable_size() == 0, "cleared empty");
    char out[64] = {0};
    CHECK(b.read(out, 10) == 0, "read after clear returns 0");
    CHECK(b.write(k_msg, n) == n, "write after clear works");
    CHECK(b.can_readable_size() == n, "data present after rewrite");
}

void test_blank_space()
{
    std::printf("blank_space\n");
    buffer b; // capacity 1024, limit 1024
    const std::size_t n = std::strlen(k_msg);
    b.write(k_msg, n);
    CHECK(b.read_ptr_move_n(10), "skip 10"); // 10 consumed
    const auto blank = b.blank_space();
    CHECK(blank == (1024 - (n - 10)), "blank = free tail + capacity headroom");

    buffer g; // grow beyond limit -> headroom
    g.set_limit_max(4096);
    CHECK(g.blank_space() == 4096, "fresh buffer blank == limit");
}

void test_null_and_empty_guards()
{
    std::printf("null / empty guards\n");
    buffer b;
    char out[4] = {0};
    bool threw = false;
    try
    {
        b.write(nullptr, 4);
    }
    catch (const std::exception &)
    {
        threw = true;
    }
    CHECK(threw, "write(nullptr) throws");
    threw = false;
    try
    {
        b.write(out, 0);
    }
    catch (const std::exception &)
    {
        threw = true;
    }
    CHECK(threw, "write size 0 throws");

    CHECK(b.read(out, 0) == 0, "read size 0 returns 0");
    b.write("abcd", 4);
    CHECK(b.read(out, 0) == 0, "read size 0 returns 0 (live)");
    CHECK(b.read(nullptr, 2) == 0, "read nullptr dest returns 0");
}

void test_write_over_limit_throws()
{
    std::printf("write over limit throws\n");
    buffer b; // limit 1024
    // fill the buffer so there is no free space, forcing the size guard
    char fill[1024];
    std::memset(fill, 'f', sizeof(fill));
    b.write(fill, 1024);
    CHECK(b.can_readable_size() == 1024, "buffer full");

    char big[2048];
    std::memset(big, 'q', sizeof(big));
    bool threw = false;
    try
    {
        b.write(big, 2048);
    }
    catch (const std::exception &)
    {
        threw = true;
    }
    CHECK(threw, "write > limit_max while full throws");
    CHECK(b.can_readable_size() == 1024, "state unchanged after throw");
}

void test_force_get_write_ptr()
{
    std::printf("force_get_write_ptr\n");
    buffer b;
    const std::size_t n = std::strlen(k_msg);
    b.write(k_msg, n);
    char *wp = b.force_get_write_ptr();
    const char *wp_const = b.force_get_write_ptr(); // const overload
    CHECK(wp != nullptr, "write ptr non-null");
    CHECK(static_cast<const char *>(wp) == wp_const, "const/overload agree");
    CHECK(static_cast<std::size_t>(wp - b.force_get_read_ptr()) == n, "offset matches readable");
}

int main()
{
    test_write_read_roundtrip();
    test_read_partial_and_short();
    test_copy_all();
    test_read_ptr_move_n();
    test_set_limit_max();
    test_write_shrink_bug_fixed();
    test_expand_across_write();
    test_move_semantics();
    test_copy_semantics();
    test_clear();
    test_blank_space();
    test_null_and_empty_guards();
    test_write_over_limit_throws();
    test_force_get_write_ptr();

    std::printf("\n%s\n", g_failed == 0 ? "ALL PASS" : "FAILURES PRESENT");
    return g_failed == 0 ? 0 : 1;
}
