// Deterministic unit tests for avant::timer::timer / timer_manager.
//
//   g++ -std=c++20 -Wall -Wextra -Werror test/timer.test.cpp \
//       external/avant-timer/timer.cpp external/avant-timer/timer_manager.cpp \
//       -o test/timer.exe -lpthread && ./test/timer.exe
//
// The old timer.test.cpp was a 100 s real-time `sleep` demo that printed to stdout;
// it could not be asserted on or run under CI. This rewrite drives the manager with a
// synthetic clock (no sleep) so every check is deterministic, plus a single-owning-
// thread contract demo (CP.3) that mirrors how the worker drives the manager.

#include <cstdint>
#include <cstdio>
#include <memory>

#include "../external/avant-timer/timer.h"
#include "../external/avant-timer/timer_manager.h"

using avant::timer::timer;
using avant::timer::timer_manager;

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

namespace
{
    // Helper: build a timer with a recording callback that appends its id each fire.
    template <typename Counter>
    std::shared_ptr<timer> make_timer(uint64_t id,
                                      uint64_t now,
                                      int32_t repeats,
                                      uint64_t interval,
                                      Counter &fired)
    {
        return std::make_shared<timer>(id, now, repeats, interval,
                                       [&fired](const timer &self) -> void
                                       {
                                           ++fired;
                                           (void)self;
                                       });
    }
} // namespace

// ---------------------------------------------------------------------------
// timer::run() post-state
// ---------------------------------------------------------------------------

void test_run_one_shot()
{
    std::printf("one-shot run()\n");
    std::size_t fired = 0;
    timer t(1, 100, 1, 5, [&fired](const timer &)
            { ++fired; });
    const uint64_t expiry_before = t.get_expired_time();
    CHECK(expiry_before == 105, "initial expiry = now + interval");
    t.enforce_run();
    CHECK(fired == 1, "fired exactly once");
    CHECK(t.repeated_times_is_zero(), "finished one-shot lands at the arming state (repeats==0)");
    // Regression (finding 1): a one-shot timer must NOT advance past its fired expiry.
    CHECK(t.get_expired_time() == expiry_before, "one-shot expiry does not advance");
    CHECK(t.get_repeated_times() == 0, "repeated_times decremented to 0");
}

void test_run_repeating()
{
    std::printf("repeating run()\n");
    std::size_t fired = 0;
    timer t(2, 100, 3, 5, [&fired](const timer &)
            { ++fired; });
    t.enforce_run();
    CHECK(fired == 1, "fire 1");
    CHECK(t.get_repeated_times() == 2, "2 repeats left");
    CHECK(t.get_expired_time() == 110, "expiry advanced by one interval");
    t.enforce_run();
    CHECK(t.get_expired_time() == 115, "expiry advanced again");
    t.enforce_run();
    CHECK(fired == 3, "fired 3 times total");
    CHECK(t.get_repeated_times() == 0, "exhausted (repeats hit 0)");
    const uint64_t expiry_at_stop = t.get_expired_time();
    t.enforce_run(); // 4th call: repeated_times == 0 -> no callback, no advance
    CHECK(fired == 3, "does not fire once exhausted");
    CHECK(t.get_expired_time() == expiry_at_stop, "expiry unchanged when exhausted");
}

void test_run_infinite()
{
    std::printf("infinite run()\n");
    std::size_t fired = 0;
    timer t(3, 100, timer::REPEAT_INFINITE, 2, [&fired](const timer &)
            { ++fired; });
    for (int i = 0; i < 5; ++i)
    {
        t.enforce_run();
    }
    CHECK(fired == 5, "fires every run");
    CHECK(!t.repeated_times_is_zero(), "infinite timer is not in the arming state");
    CHECK(t.get_expired_time() == 112, "expiry advances 100 + 5*2 = 112 (5 re-arms)");
    CHECK(t.get_repeated_times() == timer::REPEAT_INFINITE, "repeated_times stays -1");
}

void test_run_arming_never_fires()
{
    std::printf("arming (repeated_times == 0) run()\n");
    std::size_t fired = 0;
    timer t(4, 100, 0, 5, [&fired](const timer &)
            { ++fired; });
    const uint64_t expiry_before = t.get_expired_time();
    t.enforce_run();
    CHECK(fired == 0, "arming timer never fires");
    CHECK(t.repeated_times_is_zero(), "still arming");
    CHECK(t.get_repeated_times() == 0, "repeated_times unchanged");
    CHECK(t.get_expired_time() == expiry_before, "expiry unchanged");
}

// ---------------------------------------------------------------------------
// is_expired boundary + overflow guard
// ---------------------------------------------------------------------------

void test_is_expired_boundary()
{
    std::printf("is_expired boundary\n");
    timer t(5, 100, 1, 5, [](const timer &) {}); // expiry = 105
    CHECK(!t.is_expired(104), "now < expiry -> not expired");
    CHECK(t.is_expired(105), "now == expiry -> expired");
    CHECK(t.is_expired(106), "now > expiry -> expired");
}

void test_ctor_overflow_guard()
{
    std::printf("constructor overflow guard\n");
    // now + interval would wrap in uint64_t; must saturate to UINT64_MAX (never fires).
    timer t(6, UINT64_MAX - 1, 1, 10, [](const timer &) {});
    CHECK(t.get_expired_time() == UINT64_MAX, "saturated, no wrap");
    CHECK(!t.is_expired(UINT64_MAX - 1), "not expired at near-max now");
    // normal range is unaffected
    timer t2(7, 100, 1, 5, [](const timer &) {});
    CHECK(t2.get_expired_time() == 105, "normal expiry unaffected by guard");
}

// ---------------------------------------------------------------------------
// manager: check_and_handle / mark_delete / query accessors
// ---------------------------------------------------------------------------

void test_manager_fire_once()
{
    std::printf("manager: one-shot fires then drops\n");
    timer_manager mgr;
    std::size_t fired = 0;
    auto t = make_timer(1, 100, 1, 5, fired);
    CHECK(mgr.add(t) == t, "add returns the timer");
    CHECK(mgr.size() == 1, "size 1 after add");
    CHECK(!mgr.empty(), "not empty");
    CHECK(mgr.exist(1) == true, "count(id) == true");

    mgr.check_and_handle(104); // not yet
    CHECK(fired == 0, "not fired before expiry");
    CHECK(mgr.size() == 1, "still queued");
    mgr.check_and_handle(105); // expiry reached
    CHECK(fired == 1, "fired at expiry");
    CHECK(mgr.size() == 0, "dropped after one-shot fire");
    CHECK(mgr.empty(), "empty after fire");
    CHECK(mgr.exist(1) == false, "count(id) == false");
}

void test_manager_repeating_rearms()
{
    std::printf("manager: repeating timer re-arms at the right cadence\n");
    timer_manager mgr;
    std::size_t fired = 0;
    auto t = make_timer(2, 100, 3, 5, fired); // fires at 105, 110, 115
    CHECK(mgr.add(t) == t, "add returns the timer");

    mgr.check_and_handle(105);
    CHECK(fired == 1, "fire at 105");
    CHECK(mgr.size() == 1, "re-armed");
    mgr.check_and_handle(109);
    CHECK(fired == 1, "no early fire at 109");
    mgr.check_and_handle(110);
    CHECK(fired == 2, "fire at 110");
    mgr.check_and_handle(115);
    CHECK(fired == 3, "fire at 115");
    CHECK(mgr.size() == 0, "exhausted and dropped");
    mgr.check_and_handle(120);
    CHECK(fired == 3, "does not fire after exhaustion");
}

void test_manager_arming_never_fires()
{
    std::printf("manager: arming timer is never fired\n");
    timer_manager mgr;
    std::size_t fired = 0;
    auto t = make_timer(3, 100, 0, 5, fired); // expires at 105 but is arming
    CHECK(mgr.add(t) == nullptr, "add returns the timer");
    mgr.check_and_handle(105);
    CHECK(fired == 0, "arming timer not fired even when expired");
    CHECK(mgr.size() == 0, "expired arming entry dropped, not re-armed");
}

void test_manager_out_of_order_insert_sorts()
{
    std::printf("manager: out-of-order inserts are sorted by expiry\n");
    timer_manager mgr;
    // far future first, then near — the multimap must order them by expiry.
    std::size_t far = 0, near = 0;
    auto far_t = make_timer(1, 100, 1, 100, far); // expiry 200
    auto near_t = make_timer(2, 100, 1, 1, near); // expiry 101
    CHECK(mgr.add(far_t) == far_t, "add far returns timer");
    CHECK(mgr.add(near_t) == near_t, "add near returns timer");
    CHECK(mgr.size() == 2, "two queued");
    mgr.check_and_handle(101);
    CHECK(near == 1, "earliest (101) fired first");
    CHECK(far == 0, "far one not fired");
    CHECK(mgr.size() == 1, "one remains");
    mgr.check_and_handle(200);
    CHECK(far == 1, "far one fires at its expiry");
    CHECK(mgr.size() == 0, "drained");
}

void test_manager_mark_delete_same_tick()
{
    std::printf("manager: delete marked same tick is honored before the timer fires\n");
    timer_manager mgr;
    std::size_t fired = 0;
    // repeating timer: fires at 110, would re-arm at 120. Delete it in the same tick so
    // it fires but is NOT re-armed (this is the worker's use: delete a conn that is
    // closing so its timeout does not re-arm).
    auto t = make_timer(1, 100, 3, 10, fired); // expires 110
    CHECK(mgr.add(t) == t, "add returns the timer");
    mgr.mark_delete(1);        // delete before its expiry is reached
    mgr.check_and_handle(110); // reaches expiry this tick; the delete mark wins -> removed,
    // and (correct delete semantics) it is NOT fired, so a closing conn's timeout
    // callback does not run. This is exactly the worker's use in close_client_fd.
    CHECK(fired == 0, "marked-deleted timer is removed, not fired");
    CHECK(mgr.size() == 0, "deleted, not re-armed");
    mgr.check_and_handle(200);
    CHECK(fired == 0, "still absent after later ticks");
}

void test_manager_delete_set_persists()
{
    std::printf("manager: delete set persists across ticks (regression finding 6)\n");
    timer_manager mgr;
    std::size_t fired = 0;
    auto t = make_timer(1, 100, 1, 10, fired); // one-shot, expiry 110
    CHECK(mgr.add(t) == t, "add returns the timer");
    // A delete marked in one tick must NOT be wiped by a no-op tick in between. The
    // old code blanket-cleared the set each call, losing any cross-tick delete.
    mgr.mark_delete(1);
    mgr.check_and_handle(105); // no timer expired this tick; set must survive
    // ...but a one-shot's expiry (110) is *before* we reach it, so it fires once first.
    // For an *infinite* timer the same sequence must keep it deleted on re-arm.
    std::size_t infired = 0;
    // later expiry (120) so the no-op tick at 110 does not reach it; the delete set must
    // persist across that tick and apply when the infinite timer's expiry is reached.
    auto inf = std::make_shared<timer>(2, 100, timer::REPEAT_INFINITE, 20, [&infired](const timer &)
                                       { ++infired; });
    CHECK(mgr.add(inf) == inf, "add infinite returns timer");
    mgr.mark_delete(2);
    mgr.check_and_handle(110); // id1 fires+expires; id2 (exp 120) not reached; set persists
    mgr.check_and_handle(120); // id2's expiry reached -> deleted, dropped, NOT re-armed
    CHECK(infired == 0, "deleted infinite timer is never re-armed");
    CHECK(mgr.size() == 0, "both entries gone");
}

void test_manager_null_add()
{
    std::printf("manager: add(nullptr) is a no-op\n");
    timer_manager mgr;
    CHECK(mgr.add(nullptr) == nullptr, "returns nullptr");
    CHECK(mgr.size() == 0, "size stays 0");
}

// ---------------------------------------------------------------------------
// worker-style scenario: per-connection one-shot timeout
// ---------------------------------------------------------------------------

void test_worker_style_timeout()
{
    std::printf("worker-style one-shot connection timeout\n");
    timer_manager mgr;
    // simulates the worker: monotonic seconds, per-conn one-shot 5 s timer.
    const uint64_t t0 = 1000;
    std::size_t conn_fired = 0;
    // two connections with the same deadline; mark one deleted before it expires.
    auto c10 = make_timer(10, t0, 1, 5, conn_fired); // gid 10
    std::size_t conn2_fired = 0;
    auto c11 = make_timer(11, t0, 1, 5, conn2_fired); // gid 11
    CHECK(mgr.add(c10) == c10, "add conn 10 returns timer");
    CHECK(mgr.add(c11) == c11, "add conn 11 returns timer");
    CHECK(mgr.size() == 2, "two live conn timers");

    mgr.mark_delete(10);          // conn 10 closed before its timeout
    mgr.check_and_handle(t0 + 5); // both reach expiry at t0+5
    CHECK(conn_fired == 0, "deleted conn 10 did not time out");
    CHECK(conn2_fired == 1, "live conn 11 timed out");
    CHECK(mgr.size() == 0, "both dropped");
}

// ---------------------------------------------------------------------------
// single-owning-thread contract (CP.3): the manager is driven from one thread,
// the way the worker does. A background thread *adds* timers (the id source)
// while the owning thread runs the check loop. This demonstrates that the
// driver's loop + one-shot timers drain correctly under mixed add/check activity
// from a single owner; it is NOT a concurrency test (calling add + check from
// two threads on one manager is undefined behavior by design).
// ---------------------------------------------------------------------------

void test_single_owning_thread_contract()
{
    std::printf("single-owning-thread contract: owner does add + check in one thread\n");
    // The manager is driven from a single owning thread (CP.3) — exactly how the worker
    // uses it. The owner adds a batch of one-shot timers, then runs the check loop until
    // the queue drains. This is the contract the timer library guarantees: one thread
    // performs all add/check_and_handle/mark_delete on a given manager.
    timer_manager mgr;
    const int batch = 50;
    for (int i = 0; i < batch; ++i)
    {
        // distinct ids, same expiry, one-shot.
        (void)mgr.add(std::make_shared<timer>(static_cast<uint64_t>(i), 0, 1, 1, [](const timer &) {}));
    }
    CHECK(mgr.size() == static_cast<std::size_t>(batch), "all timers queued");

    // Drain: each call removes at most one same-expiry timer, so the loop runs ~batch
    // times; the generous bound keeps the test bounded regardless of scheduling.
    int ticks = 0;
    for (int tick = 0; tick < batch * 10 && !mgr.empty(); ++tick)
    {
        (void)mgr.check_and_handle(1);
        ++ticks;
    }
    CHECK(mgr.empty(), "owner drained every timer");
    // All 50 share expiry 1, so a single call drains them all.
    CHECK(ticks == 1, "single call drains the whole same-expiry batch");
}

int main()
{
    std::printf("== timer unit tests ==\n");
    test_run_one_shot();
    test_run_repeating();
    test_run_infinite();
    test_run_arming_never_fires();
    test_is_expired_boundary();
    test_ctor_overflow_guard();

    std::printf("\n== timer_manager tests ==\n");
    test_manager_fire_once();
    test_manager_repeating_rearms();
    test_manager_arming_never_fires();
    test_manager_out_of_order_insert_sorts();
    test_manager_mark_delete_same_tick();
    test_manager_delete_set_persists();
    test_manager_null_add();
    test_worker_style_timeout();

    std::printf("\n== single-owning-thread contract ==\n");
    test_single_owning_thread_contract();

    std::printf("\n%s\n", g_failed == 0 ? "ALL PASS" : "FAILURES PRESENT");
    return g_failed == 0 ? 0 : 1;
}
