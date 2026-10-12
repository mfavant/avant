// g++ -std=c++20 test/json.test.cpp external/avant-json/json.cpp external/avant-json/parser.cpp -o test/json.exe && ./test/json.exe
#include <climits>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include "../external/avant-json/json.h"
#include "../external/avant-json/parser.h"

using namespace std;
using avant::json::json;
using avant::json::parser;

static int g_failures = 0;
#define CHECK(expr)                                                              \
    do                                                                           \
    {                                                                            \
        if (!(expr))                                                             \
        {                                                                        \
            std::cerr << "FAIL line " << __LINE__ << ": " << #expr << std::endl; \
            ++g_failures;                                                        \
        }                                                                        \
    } while (0)

int main()
{
    // --- basic parse / to_string round-trip ---
    {
        const std::string str = "{\"int\": 232, \"double\": -434.2, \"arr\":[23,43,1,43.3,{\"n\":{\"arr\":[12,32],\"obj\":{\"in_obj\":{}}}}]}";
        json obj;
        obj.parse(str);
        CHECK(obj.is_object());
        CHECK(obj.get("int").as_integer() == 232);
        CHECK(obj.get("double").as_double() == -434.2);
        CHECK(obj.get("arr").size() == 5);
        CHECK(obj.get("arr").get(static_cast<std::size_t>(0)).as_integer() == 23);
        CHECK(obj.get("arr").get(3).as_double() == 43.3);
        CHECK(obj.get("arr").get(4).get("n").get("arr").size() == 2);
        CHECK(obj.get("arr").get(4).get("n").get("obj").get("in_obj").is_object());
        CHECK(obj.get("arr").get(4).get("n").get("obj").get("in_obj").empty());
        {
            json re;
            re.parse(obj.to_string());
            CHECK(obj == re);
        }
    }

    // --- scalar comparisons ---
    {
        json b = false;
        CHECK(b == false);
        CHECK(b != true);
        json i = 12ll;
        CHECK(b != i); // bool != integer
        CHECK(i == 12ll);
        CHECK(i != 13ll);
    }

    // --- array == with identical content (was broken: returned false) ---
    {
        json a;
        a.parse("[1,2,3]");
        json b;
        b.parse("[1,2,3]");
        CHECK(a == b);
        CHECK(!(a != b));

        json c;
        c.parse("[1,2,4]");
        CHECK(a != c);
    }

    // --- object == with identical content (was broken: returned false) ---
    {
        json a;
        a.parse("{\"k\":1,\"m\":[1,2]}");
        json b;
        b.parse("{\"k\":1,\"m\":[1,2]}");
        CHECK(a == b);
        CHECK(!(a != b));

        json c;
        c.parse("{\"k\":1,\"m\":[1,3]}");
        CHECK(a != c);
    }

    // --- escape decode (\\n, \\t, \\uXXXX) ---
    {
        json s;
        s.parse("\"a\\n\\t\\u0041x\"");
        CHECK(s.is_string());
        CHECK(s.as_string() == "a\n\tAx");
    }

    // --- escape encode / round-trip (was broken: to_string did not escape) ---
    {
        json t("he said \"hi\"\tand\\back");
        const std::string enc = t.to_string();
        CHECK(enc == "\"he said \\\"hi\\\"\\tand\\\\back\"");
        json u;
        u.parse(enc);
        CHECK(u == t);
    }

    // --- number: overflow integer ---
    {
        bool threw = false;
        try
        {
            parser p;
            p.load("99999999999999999999");
            (void)p.parse();
        }
        catch (...)
        {
            threw = true;
        }
        CHECK(threw); // 20 digits > LLONG_MAX
    }

    // --- number: overflow double (1e999 -> inf) ---
    {
        bool threw = false;
        try
        {
            parser p;
            p.load("1e999");
            (void)p.parse();
        }
        catch (...)
        {
            threw = true;
        }
        CHECK(threw);
    }

    // --- number: valid exponent ---
    {
        json e;
        e.parse("1.5e3");
        CHECK(e.is_double());
        CHECK(e.as_double() == 1500.0);
    }

    // --- number: leading zero rejected ---
    {
        bool threw = false;
        try
        {
            parser p;
            p.load("01");
            (void)p.parse();
        }
        catch (...)
        {
            threw = true;
        }
        CHECK(threw);
    }

    // --- trailing content rejected (was accepted) ---
    {
        bool threw = false;
        try
        {
            parser p;
            p.load("1 2");
            (void)p.parse();
        }
        catch (...)
        {
            threw = true;
        }
        CHECK(threw);
    }

    // --- deep nesting: DoS guard (was unbounded recursion -> stack overflow) ---
    {
        // 20000 nested arrays: must throw, not stack-overflow
        std::string deep = "[";
        for (int i = 0; i < 20000; i++)
            deep += "[";
        for (int i = 0; i < 20000; i++)
            deep += "]";
        bool threw = false;
        try
        {
            parser p;
            p.load(deep);
            (void)p.parse();
        }
        catch (...)
        {
            threw = true;
        }
        CHECK(threw);

        // 511 nested: under the 512 cap, must succeed
        std::string ok = "[";
        for (int i = 0; i < 510; i++)
            ok += "[";
        for (int i = 0; i < 510; i++)
            ok += "]";
        ok += "]"; // total 511 levels
        bool success = false;
        try
        {
            parser p;
            p.load(ok);
            json d = p.parse();
            success = d.is_array();
        }
        catch (...)
        {
        }
        CHECK(success);
    }

    // --- nesting depth boundary: exactly kMaxDepth succeeds, one more throws ---
    {
        auto nested = [](std::size_t n)
        {
            std::string s;
            s.reserve(n * 2);
            for (std::size_t i = 0; i < n; i++)
                s += "[";
            for (std::size_t i = 0; i < n; i++)
                s += "]";
            return s;
        };
        bool success = false;
        try
        {
            parser p;
            p.load(nested(parser::kMaxDepth));
            success = p.parse().is_array();
        }
        catch (...)
        {
        }
        CHECK(success);

        bool threw = false;
        try
        {
            parser p;
            p.load(nested(parser::kMaxDepth + 1));
            (void)p.parse();
        }
        catch (...)
        {
            threw = true;
        }
        CHECK(threw);
    }

    // --- move semantics: no double-free, no double-init ---
    {
        json m;
        m.parse("[1,2,3]");
        json n(std::move(m));
        CHECK(n.is_array());
        CHECK(n.size() == 3);
        CHECK(n.get(static_cast<std::size_t>(0)).as_integer() == 1);
        CHECK(m.is_null()); // moved-from source is reset to null
    }

    // --- move assignment ---
    {
        json a("hello");
        json b("world");
        b = std::move(a);
        CHECK(b.is_string());
        CHECK(b.as_string() == "hello");
        CHECK(a.is_null()); // moved-from source is reset to null
    }

    // --- copy: deep copy independence ---
    {
        json a;
        a.parse("{\"k\":[1,2]}");
        json b(a); // copy
        CHECK(a == b);
        b["k"].remove(static_cast<std::size_t>(0)); // mutate b via non-const path
        CHECK(a.get("k").size() == 2);
        CHECK(b.get("k").size() == 1);
    }

    // --- iterator no-op on non-array ---
    {
        json obj("{}");
        size_t cnt = 0;
        for (auto &x : obj)
        {
            (void)x;
            ++cnt;
        }
        CHECK(cnt == 0);
    }

    // --- get on wrong type throws ---
    {
        json x("5");
        bool threw = false;
        try
        {
            (void)x.get("k");
        }
        catch (...)
        {
            threw = true;
        }
        CHECK(threw);

        bool threw2 = false;
        try
        {
            (void)x.get(static_cast<std::size_t>(0));
        }
        catch (...)
        {
            threw2 = true;
        }
        CHECK(threw2);
    }

    // --- has / remove ---
    {
        json arr;
        arr.append(1ll).append(2ll).append(3ll);
        CHECK(arr.has(static_cast<std::size_t>(0)));
        CHECK(arr.has(static_cast<std::size_t>(2)));
        CHECK(!arr.has(static_cast<std::size_t>(3)));
        CHECK(arr.remove(static_cast<std::size_t>(1)));
        CHECK(arr.has(static_cast<std::size_t>(1))); // index 1 now holds 3
        CHECK(arr.size() == 2);
        CHECK(arr.get(static_cast<std::size_t>(1)).as_integer() == 3);

        json obj;
        obj["a"] = 1ll;
        obj["b"] = 2ll;
        CHECK(obj.has("a"));
        CHECK(!obj.has("c"));
        CHECK(obj.remove("a"));
        CHECK(!obj.has("a"));
        CHECK(obj.size() == 1);
    }

    // --- as_* on wrong type throws ---
    {
        json s("hello");
        bool threw = false;
        try
        {
            (void)s.as_integer();
        }
        catch (...)
        {
            threw = true;
        }
        CHECK(threw);

        bool threw2 = false;
        try
        {
            (void)s.as_bool();
        }
        catch (...)
        {
            threw2 = true;
        }
        CHECK(threw2);
    }

    // --- null semantics ---
    {
        json n;
        CHECK(n.is_null());
        CHECK(n.empty());
        CHECK(n == json::null());
    }

    // --- \u surrogate pair reassembles to a non-BMP code point (UTF-8) ---
    {
        json j;
        j.parse("\"\\uD83D\\uDE00\"");              // grinning face, U+1F600
        CHECK(j.as_string() == "\xF0\x9F\x98\x80"); // F0 9F 98 80
    }

    // --- \u lone surrogate is rejected ---
    {
        bool t = false;
        try
        {
            json j;
            j.parse("\"\\uD800\"");
        }
        catch (...)
        {
            t = true;
        }
        CHECK(t); // lone high surrogate
        bool t2 = false;
        try
        {
            json j;
            j.parse("\"\\uDC00\"");
        }
        catch (...)
        {
            t2 = true;
        }
        CHECK(t2); // lone low surrogate
    }

    // --- \u followed by a non-surrogate after a high surrogate is rejected ---
    {
        bool t = false;
        try
        {
            json j;
            j.parse("\"\\uD800x\"");
        }
        catch (...)
        {
            t = true;
        }
        CHECK(t);
    }

    // --- \u BMP character decodes to the right UTF-8 bytes ---
    {
        json j;
        j.parse("\"\\u00E9\"");             // é
        CHECK(j.as_string() == "\xC3\xA9"); // C3 A9
    }

    // --- control char encodes to \u00xx and round-trips ---
    {
        json s;
        s.parse("\"a\\u0001b\"");
        CHECK(s.as_string() == "a"
                               "\x01"
                               "b"); // a, 0x01, b
        const std::string enc = s.to_string();
        json s2;
        s2.parse(enc);
        CHECK(s2 == s);
    }

    // --- as_integer coerces a whole-number double; as_double coerces an integer ---
    {
        json d;
        d.parse("3.0");
        CHECK(d.is_double());
        CHECK(d.as_integer() == 3);

        json i(42ll);
        CHECK(i.is_integer());
        CHECK(i.as_double() == 42.0);
    }

    // --- to_string(double) preserves precision (std::to_string gave 6 digits) ---
    {
        json d;
        d.parse("0.1");
        json r;
        r.parse(d.to_string());
        CHECK(r.as_double() == 0.1);

        json d2;
        d2.parse("10000000.0");
        json r2;
        r2.parse(d2.to_string());
        CHECK(r2.as_double() == 1e7);

        json d3;
        d3.parse("123456.789");
        json r3;
        r3.parse(d3.to_string());
        CHECK(r3.as_double() == 123456.789);
    }

    // --- as_integer: non-whole double throws instead of truncating ---
    {
        json d(3.7);
        bool threw = false;
        try
        {
            (void)d.as_integer();
        }
        catch (...)
        {
            threw = true;
        }
        CHECK(threw); // 3.7 is not a whole number; truncation to 3 would be lossy
    }

    // --- as_integer: double range boundaries ---
    {
        // -2^63 is exactly representable as a double and equals LLONG_MIN.
        json lo;
        lo.parse("-9.223372036854775808e18");
        CHECK(lo.is_double());
        CHECK(lo.as_integer() == LLONG_MIN);

        // 2^63 does not fit in long long: must throw, not cast out of range.
        json hi;
        hi.parse("9.223372036854775808e18");
        bool threw = false;
        try
        {
            (void)hi.as_integer();
        }
        catch (...)
        {
            threw = true;
        }
        CHECK(threw);
    }

    // --- 19-digit integers that fit in long long must parse ---
    {
        json i;
        i.parse("9223372036854775807"); // LLONG_MAX
        CHECK(i.as_integer() == LLONG_MAX);

        json j;
        j.parse("-9223372036854775808"); // LLONG_MIN
        CHECK(j.as_integer() == LLONG_MIN);

        json k;
        k.parse("1000000000000000000"); // 10^18
        CHECK(k.as_integer() == 1000000000000000000ll);

        // 19 digits that overflow long long must still be rejected.
        bool threw = false;
        try
        {
            json x;
            x.parse("9223372036854775808");
        }
        catch (...)
        {
            threw = true;
        }
        CHECK(threw);
    }

    // --- operator[](SIZE_MAX): throws without destroying the array ---
    {
        json arr;
        arr.append(1ll).append(2ll).append(3ll);
        bool threw = false;
        try
        {
            (void)arr[std::numeric_limits<std::size_t>::max()];
        }
        catch (...)
        {
            threw = true;
        }
        CHECK(threw);
        CHECK(arr.size() == 3);
        CHECK(arr.get(static_cast<std::size_t>(0)).as_integer() == 1);
        CHECK(arr.get(static_cast<std::size_t>(2)).as_integer() == 3);
    }

    // --- to_string(double) keeps whole-number doubles re-parsing as doubles ---
    {
        json d(1234.0);
        CHECK(d.to_string() == "1234.0");
        json r;
        r.parse(d.to_string());
        CHECK(r.is_double());
        CHECK(r == d);

        json e;
        e.parse("10000000.0");
        json r2;
        r2.parse(e.to_string());
        CHECK(r2.is_double());
        CHECK(r2 == e);
    }

    // --- non-finite doubles are rejected (JSON has no representation) ---
    {
        bool threw = false;
        try
        {
            json d(std::numeric_limits<double>::infinity());
            (void)d;
        }
        catch (...)
        {
            threw = true;
        }
        CHECK(threw);

        bool threw2 = false;
        try
        {
            json d(1.0);
            d = std::numeric_limits<double>::quiet_NaN();
            (void)d;
        }
        catch (...)
        {
            threw2 = true;
        }
        CHECK(threw2);

        bool threw3 = false;
        try
        {
            (void)json(1.0).to_string();
        }
        catch (...)
        {
            threw3 = true;
        } // sanity: finite double serializes fine
        CHECK(!threw3);
    }

    // --- unescaped control characters in strings are rejected (RFC 8259) ---
    {
        bool threw = false;
        try
        {
            json s;
            s.parse("\"a\x01b\"");
        }
        catch (...)
        {
            threw = true;
        }
        CHECK(threw);
    }

    // --- duplicate object keys: last value wins (std::map storage) ---
    {
        json j;
        j.parse("{\"k\":1,\"k\":2}");
        CHECK(j.size() == 1);
        CHECK(j.get("k").as_integer() == 2);
    }

    // --- object == ignores key order; to_string emits sorted keys ---
    {
        json a;
        a.parse("{\"b\":1,\"a\":2}");
        json b;
        b.parse("{\"a\":2,\"b\":1}");
        CHECK(a == b);
        CHECK(a.to_string() == b.to_string());
    }

    // --- raw UTF-8 bytes pass through parsing and serialization untouched ---
    {
        json j;
        j.parse("\"h\xC3\xA9llo\""); // "héllo" in UTF-8
        CHECK(j.as_string() == "h\xC3\xA9llo");
        const std::string enc = j.to_string();
        CHECK(enc == "\"h\xC3\xA9llo\"");
        json j2;
        j2.parse(enc);
        CHECK(j2 == j);
    }

    // --- "/" escape decodes; re-serialization round-trips ---
    {
        json j;
        j.parse("\"a\\/b\"");
        CHECK(j.as_string() == "a/b");
        json j2;
        j2.parse(j.to_string());
        CHECK(j2 == j);
    }

    // --- object keys are escaped on serialization and decode back ---
    {
        json obj;
        obj["a\"b"] = 1ll;
        CHECK(obj.to_string() == "{\"a\\\"b\":1}");
        json r;
        r.parse(obj.to_string());
        CHECK(r == obj);
        CHECK(r.get("a\"b").as_integer() == 1);
    }

    // --- big integer part with a fraction parses as a double via strtod ---
    {
        json d;
        d.parse("1234567890123456789.5");
        CHECK(d.is_double());
        CHECK(d.as_double() == std::strtod("1234567890123456789.5", nullptr));
        json r;
        r.parse(d.to_string());
        CHECK(r.is_double());
        CHECK(r.as_double() == d.as_double());
    }

    // --- operator<< agrees with to_string ---
    {
        json j;
        j.parse("{\"s\":\"a\\nb\",\"n\":1}");
        std::ostringstream os;
        os << j;
        CHECK(os.str() == j.to_string());
    }

    if (g_failures == 0)
    {
        std::cout << "ALL PASS" << std::endl;
    }
    else
    {
        std::cout << g_failures << " FAILURES" << std::endl;
    }
    return g_failures ? 1 : 0;
}
