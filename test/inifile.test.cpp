// g++ -std=c++20 test/inifile.test.cpp external/avant-inifile/inifile.cpp external/avant-inifile/value.cpp -o test/inifile.exe && ./test/inifile.exe
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>

#include "../external/avant-inifile/inifile.h"

using avant::inifile::inifile;
using avant::inifile::value;

static int g_failed = 0;
static int g_passed = 0;

#define CHECK(cond, msg)                                                   \
    do                                                                     \
    {                                                                      \
        if (cond)                                                          \
        {                                                                  \
            ++g_passed;                                                    \
            std::printf("  [ok]   %s\n", msg);                             \
        }                                                                  \
        else                                                               \
        {                                                                  \
            ++g_failed;                                                    \
            std::printf("  [FAIL] %s (%s:%d)\n", msg, __FILE__, __LINE__); \
        }                                                                  \
    } while (0)

static void test_value_type_coercion()
{
    std::printf("value: type coercion round-trips\n");

    value v = true;
    CHECK(static_cast<bool>(v) == true, "bool true round-trips");
    CHECK(static_cast<std::string>(v) == "true", "bool true stored as 'true'");

    v = false;
    CHECK(static_cast<bool>(v) == false, "bool false round-trips");
    CHECK(static_cast<std::string>(v) == "false", "bool false stored as 'false'");

    v = 42;
    CHECK(static_cast<int>(v) == 42, "int round-trips");
    CHECK(static_cast<std::string>(v) == "42", "int stored as '42'");

    v = -7;
    CHECK(static_cast<int>(v) == -7, "negative int round-trips");

    v = 3.5;
    CHECK(std::abs(static_cast<double>(v) - 3.5) < 1e-6, "double round-trips");
    CHECK(static_cast<int>(v) == 3, "double truncated to int");

    v = "hello";
    CHECK(static_cast<std::string>(v) == "hello", "string round-trips");
    CHECK(static_cast<int>(v) == 0, "non-numeric string -> 0");
    CHECK(static_cast<bool>(v) == false, "non-bool string -> false");
}

static void test_value_bool_parsing()
{
    std::printf("value: bool parsing accepts common truthy forms\n");

    CHECK(static_cast<bool>(value("1")) == true, "'1' is true");
    CHECK(static_cast<bool>(value("on")) == true, "'on' is true");
    CHECK(static_cast<bool>(value("yes")) == true, "'yes' is true");
    CHECK(static_cast<bool>(value("TRUE")) == true, "'TRUE' is true");
    CHECK(static_cast<bool>(value("True")) == true, "'True' is true");
    CHECK(static_cast<bool>(value("2")) == true, "'2' is true");
    CHECK(static_cast<bool>(value("0")) == false, "'0' is false");
    CHECK(static_cast<bool>(value("off")) == false, "'off' is false");
    CHECK(static_cast<bool>(value("no")) == false, "'no' is false");
    CHECK(static_cast<bool>(value("false")) == false, "'false' is false");
    CHECK(static_cast<bool>(value("garbage")) == false, "garbage is false");
    CHECK(static_cast<bool>(value("")) == false, "empty is false");
}

static void test_value_numeric_coercion()
{
    std::printf("value: numeric coercion edge cases\n");

    CHECK(static_cast<int>(value("30x")) == 30, "trailing junk parses leading number (stol)");
    CHECK(static_cast<int>(value("abc")) == 0, "non-numeric -> 0");
    CHECK(static_cast<int>(value("")) == 0, "empty -> 0");
    CHECK(static_cast<int>(value("99999999999999999999")) == INT_MAX, "overflow saturates at INT_MAX");
    CHECK(static_cast<int>(value("-99999999999999999999")) == INT_MIN, "underflow saturates at INT_MIN");
    CHECK(static_cast<int>(value("9999999999")) == INT_MAX, "10-digit value above INT_MAX saturates (boundary)");
    CHECK(static_cast<int>(value("123456789.123456789")) == 123456789, "decimal with >10 total digits is not mistaken for overflow");
    CHECK(std::abs(static_cast<double>(value("1.5e3")) - 1500.0) < 1e-9, "exponent form parses");
    CHECK(static_cast<double>(value("abc")) == 0.0, "non-numeric double -> 0");
}

static void test_value_equality()
{
    std::printf("value: operator== numeric and boolean-set equality\n");

    CHECK(value(3) == value(3), "3 == 3");
    CHECK(value(3) == value("3"), "int 3 == string '3'");
    CHECK(value("3") == value("3.0"), "'3' == '3.0' (numeric compare)");
    CHECK(value(1) != value(2), "1 != 2");
    CHECK(value(true) == value("on"), "true == 'on' (boolean set)");
    CHECK(value(false) == value("off"), "false == 'off' (boolean set)");
    CHECK(value(true) == value("1"), "true == '1' (boolean set)");
    CHECK(value("TRUE") == value("true"), "bool-set equality is case-insensitive");
    CHECK(value("2") == value("true"), "'2' == 'true' (both truthy in bool set)");
    CHECK(value("hello") == value("hello"), "equal strings");
    CHECK(value("hello") != value("world"), "different strings");
    CHECK(value("") == value(""), "empty == empty");
    CHECK(value("") != value("x"), "empty != 'x'");
}

static void test_set_get_has_remove()
{
    std::printf("inifile: set / get / has / remove\n");

    inifile ini;
    CHECK(!ini.has("server"), "fresh ini has no section");
    CHECK(!ini.has("server", "port"), "fresh ini has no key");

    ini.set("server", "port", "8080");
    ini.set("server", "app_id", "my-app");
    ini.set("server", "use_ssl", "1");
    ini.set("server", "ratio", "0.500000");
    ini.set("server", "daemon", "false");

    CHECK(ini.has("server"), "section exists after set");
    CHECK(ini.has("server", "port"), "int key exists");
    CHECK(ini.has("server", "app_id"), "string key exists");
    CHECK(ini.has("server", "use_ssl"), "bool-ish key exists");
    CHECK(ini.has("server", "ratio"), "double key exists");
    CHECK(ini.has("server", "daemon"), "bool key exists");
    CHECK(!ini.has("server", "missing"), "unset key absent");

    CHECK(static_cast<int>(ini.get("server", "port")) == 8080, "get int");
    CHECK(static_cast<std::string>(ini.get("server", "app_id")) == "my-app", "get string");
    CHECK(static_cast<int>(ini.get("server", "use_ssl")) == 1, "get int-as-bool");
    CHECK(std::abs(static_cast<double>(ini.get("server", "ratio")) - 0.5) < 1e-9, "get double");
    CHECK(static_cast<bool>(ini.get("server", "daemon")) == false, "get bool");

    // set overwrites
    ini.set("server", "port", "9090");
    CHECK(static_cast<int>(ini.get("server", "port")) == 9090, "set overwrites");

    ini.remove("server", "app_id");
    CHECK(!ini.has("server", "app_id"), "removed key absent");
    CHECK(static_cast<int>(ini.get("server", "port")) == 9090, "other keys survive remove");

    ini.remove("server");
    CHECK(!ini.has("server"), "removed section gone");
}

static void test_reads_are_non_mutating()
{
    std::printf("inifile: reads never create phantom entries\n");

    inifile ini;
    ini.set("real", "k", "1");

    // operator[] on a missing section must not create it
    (void)ini["phantom"]["k"];
    CHECK(!ini.has("phantom"), "operator[] on missing section does not create it");
    // has(key, key) on missing section must stay false
    CHECK(!ini.has("phantom", "k"), "has() on phantom section still false after operator[]");
    // get on missing section/key must not create entries
    (void)ini.get("phantom2", "k");
    (void)ini.get("real", "never-set");
    CHECK(!ini.has("phantom2"), "get on missing section does not create it");
    CHECK(!ini.has("real", "never-set"), "get on missing key does not create it");

    // a read of an existing value is stable and usable
    CHECK(static_cast<int>(ini.get("real", "k")) == 1, "get existing still works");

    // save() of the above must contain only the real section (no phantom headers)
    const std::string path = "/tmp/inifile_test_nonmutating.ini";
    CHECK(ini.save(path), "save() succeeds");
    std::string text;
    {
        std::ifstream f(path);
        std::string line;
        while (std::getline(f, line))
        {
            text += line + "\n";
        }
    }
    CHECK(text.find("phantom") == std::string::npos, "no phantom section in saved file");
    CHECK(text.find("never-set") == std::string::npos, "no phantom key in saved file");
    CHECK(text.find("[real]") != std::string::npos, "real section in saved file");
    CHECK(text.find("k = 1") != std::string::npos, "real key in saved file");

    // writes through a missing section land in a shared stand-in: visible
    // until the next load(), but never in this map
    ini["phantom3"]["k"] = "stale";
    CHECK(!ini.has("phantom3"), "write through missing section does not enter the map");
    CHECK(static_cast<std::string>(ini["phantom3"]["k"]) == "stale", "stand-in write visible before reload");
    ini.load(path);
    CHECK(static_cast<std::string>(ini["phantom3"]["k"]) == "", "stand-in wiped by load()");

    // documented exception: operator[] on an *existing* section still
    // default-constructs missing keys (std::map semantics); get() is the
    // non-mutating read path
    (void)ini["real"]["created_key"];
    CHECK(ini.has("real", "created_key"), "key-level operator[] on existing section creates the key (documented)");

    std::remove(path.c_str());
}

static void test_load_parsing()
{
    std::printf("inifile: load parsing (sections, comments, whitespace, junk lines)\n");

    const std::string path = "/tmp/inifile_test_parse.ini";
    {
        std::ofstream f(path);
        f << "# a comment line\n";
        f << "stray_key = should_be_skipped\n";
        f << "\n";
        f << "  [ server ]  \n";
        f << "app_id =   my-app  \n";
        f << "   port = 8080\n";
        f << "   # inline comment line\n";
        f << "ratio=0.25\n";
        f << "use_ssl = 1\n";
        f << "key_without_value\n";
        f << "= no leading key\n";
        f << "[unclosed section\n";
        f << "daemon = true\n";
        f << "[empty_name]   \n";
        f << "x = 1\n";
        f << "[]\n";
        f << "after_empty = 2\n";
    }

    inifile ini;
    CHECK(ini.load(path), "load() succeeds on messy file");
    CHECK(ini.has("server"), "section name trimmed");
    CHECK(static_cast<std::string>(ini.get("server", "app_id")) == "my-app", "value whitespace-trimmed");
    CHECK(static_cast<int>(ini.get("server", "port")) == 8080, "leading-space key parsed");
    CHECK(std::abs(static_cast<double>(ini.get("server", "ratio")) - 0.25) < 1e-9, "no-space 'key=value' parsed");
    CHECK(static_cast<int>(ini.get("server", "use_ssl")) == 1, "int parsed");
    CHECK(static_cast<bool>(ini.get("server", "daemon")) == true, "bool 'true' parsed");
    CHECK(!ini.has("server", "key_without_value"), "line without '=' skipped");
    CHECK(!ini.has("server", ""), "no phantom empty key from '= ...' line");
    CHECK(!ini.has("stray_key"), "pre-section key skipped, no phantom section");
    CHECK(!ini.has("unclosed section"), "unclosed bracket line skipped");
    CHECK(ini.has("empty_name"), "'[empty_name]' is a real section and parsed");
    CHECK(static_cast<int>(ini.get("empty_name", "x")) == 1, "key under real section parsed");
    CHECK(!ini.has(""), "empty section '[]' skipped, no phantom section");
    CHECK(!ini.has("after_empty"), "key after '[]' does not leak into a new section");
    CHECK(static_cast<int>(ini.get("empty_name", "after_empty")) == 2, "keys after '[]' keep landing in the previous section");
    std::remove(path.c_str());

    // missing file
    inifile bad;
    CHECK(bad.load("/tmp/inifile_test_definitely_missing_12345.ini") == false, "load() fails on missing file");
}

static void test_save_roundtrip()
{
    std::printf("inifile: save / reload round-trip\n");

    inifile ini;
    ini.set("server", "app_id", "rt-app");
    ini.set("server", "port", "12345");
    ini.set("server", "daemon", "false");
    ini.set("server", "ratio", "2.500000");
    ini.set("other", "x", "y");

    const std::string path = "/tmp/inifile_test_roundtrip.ini";
    CHECK(ini.save(path), "save() succeeds");

    inifile re;
    CHECK(re.load(path), "reload() succeeds");
    CHECK(static_cast<std::string>(re.get("server", "app_id")) == "rt-app", "string round-trips");
    CHECK(static_cast<int>(re.get("server", "port")) == 12345, "int round-trips");
    CHECK(static_cast<bool>(re.get("server", "daemon")) == false, "bool round-trips");
    CHECK(std::abs(static_cast<double>(re.get("server", "ratio")) - 2.5) < 1e-6, "double round-trips");
    CHECK(static_cast<std::string>(re.get("other", "x")) == "y", "second section round-trips");

    // save failure detection
    CHECK(ini.save("/nonexistent_dir_xyz/inifile.ini") == false, "save() reports failure on bad path");
    std::remove(path.c_str());
}

static void test_clear()
{
    std::printf("inifile: clear\n");

    inifile ini;
    ini.set("a", "k", "1");
    ini.set("b", "k", "2");
    CHECK(ini.has("a") && ini.has("b"), "two sections present");
    ini.clear();
    CHECK(!ini.has("a") && !ini.has("b"), "clear removes all sections");
}

static void test_real_main_ini()
{
    std::printf("inifile: parse actual bin/config/main.ini (config_mgr key set)\n");

    inifile ini;
    if (!ini.load("bin/config/main.ini"))
    {
        // not runnable from the repo root in every environment; skip rather than fail
        std::printf("  [skip] bin/config/main.ini not found from cwd\n");
        return;
    }
    CHECK(ini.has("server"), "[server] section present");
    CHECK(static_cast<std::string>(ini.get("server", "app_id")).size() > 0, "app_id non-empty");
    CHECK(static_cast<int>(ini.get("server", "port")) > 0, "port positive");
    CHECK(static_cast<int>(ini.get("server", "worker_cnt")) > 0, "worker_cnt positive");
    CHECK(static_cast<int>(ini.get("server", "max_client_cnt")) > 0, "max_client_cnt positive");
    CHECK(static_cast<int>(ini.get("server", "epoll_wait_time")) > 0, "epoll_wait_time positive");
    CHECK(static_cast<int>(ini.get("server", "accept_per_tick")) > 0, "accept_per_tick positive");
    CHECK(static_cast<std::string>(ini.get("server", "task_type")).size() > 0, "task_type non-empty");
    const int use_ssl = ini.get("server", "use_ssl");
    CHECK(use_ssl == 0 || use_ssl == 1, "use_ssl is 0 or 1");
    CHECK(ini.has("ipc"), "[ipc] section present");
    CHECK(static_cast<int>(ini.get("ipc", "max_ipc_conn_num")) > 0, "max_ipc_conn_num positive");
}

int main()
{
    test_value_type_coercion();
    test_value_bool_parsing();
    test_value_numeric_coercion();
    test_value_equality();
    test_set_get_has_remove();
    test_reads_are_non_mutating();
    test_load_parsing();
    test_save_roundtrip();
    test_clear();
    test_real_main_ini();

    std::printf("\n%d checks passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
