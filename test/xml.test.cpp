// g++ -std=c++20 test/xml.test.cpp external/avant-xml/document.cpp external/avant-xml/element.cpp -o test/xml.exe && ./test/xml.exe
#include <cstdio>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

#include <sys/stat.h>
#include <unistd.h>

#include "../external/avant-xml/element.h"
#include "../external/avant-xml/document.h"

using avant::xml::document;
using avant::xml::element;
using avant::xml::to_string;

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

#define CHECK_THROWS_XML(expr, msg)                                                 \
    do                                                                              \
    {                                                                               \
        bool threw = false;                                                         \
        try                                                                         \
        {                                                                           \
            (void)(expr);                                                           \
        }                                                                           \
        catch (const std::logic_error &)                                            \
        {                                                                           \
            threw = true;                                                           \
        }                                                                           \
        catch (const std::exception &ex)                                            \
        {                                                                           \
            std::printf("  [note] %s threw non-logic_error: %s\n", msg, ex.what()); \
        }                                                                           \
        CHECK(threw, msg);                                                          \
    } while (0)

static void test_element_basics()
{
    std::printf("element: construction and accessors\n");

    element el;
    CHECK(el.name().empty(), "default element has empty name");
    CHECK(el.size() == 0, "default element has no children");

    el.name("div");
    el.text("hello world");
    CHECK(to_string(el) == "<div>hello world</div>", "text element serializes");

    element el2("span", "x");
    CHECK(el2.name() == "span" && el2.text() == "x", "two-arg ctor sets name and text");

    // non-mutating attr read (finding 4: no phantom insert)
    element a("a");
    CHECK(a.attr("missing") == "", "missing attr reads as empty");
    CHECK(!a.has_attr("missing"), "missing attr: has_attr false");
    a.attr("k", "v");
    CHECK(a.attr("k") == "v" && a.has_attr("k"), "set attr reads back");
    CHECK(to_string(a) == "<a k=\"v\" />", "present attr serializes, no phantom attrs");
    a.attr("k", ""); // blank value deletes
    CHECK(!a.has_attr("k"), "blank value deletes the attribute");

    // children
    element root("r");
    root.append(element("c1"));
    root.append(element("c2"));
    root.append(element("c3"));
    CHECK(root.size() == 3, "append grows size");
    CHECK(root.child_at(1).name() == "c2", "child_at(1) is c2");
    CHECK(root[2].name() == "c3", "operator[](2) is c3");
    CHECK(root.child_at(99).name().empty(), "child_at out of range yields null element");
    CHECK(root["c2"].name() == "c2", "child lookup by name");
    CHECK(root["nope"].name().empty(), "absent child name yields null element");

    int n = 0;
    for (const auto &c : root)
    {
        (void)c;
        ++n;
    }
    CHECK(n == 3, "non-const range-for visits all children");

    const element &croot = root;
    int nc = 0;
    for (const auto &c : croot)
    {
        (void)c;
        ++nc;
    }
    CHECK(nc == 3, "range-for over a const element (needs end() const)");

    element c1 = root["c1"];
    c1.text("mutated"); // copies out: the tree is value semantics
    CHECK(root["c1"].text().empty(), "child read is a deep copy, tree unmutated");

    root.clear();
    CHECK(root.size() == 0 && root.name().empty() && root.text().empty(), "clear resets everything");
}

static void test_to_string()
{
    std::printf("element: to_string\n");

    element empty("a");
    CHECK(to_string(empty) == "<a />", "empty element is self-closing");

    element pc("plugin", "");
    pc.attr("name", "libechoplugin.so");
    pc.attr("switch", "on");
    CHECK(to_string(pc) == "<plugin name=\"libechoplugin.so\" switch=\"on\" />",
          "attr-only element is self-closing with attrs");

    element wf("workflow");
    element work("work");
    work.attr("name", "1");
    work.attr("switch", "on");
    work.append(pc);
    wf.append(work);
    CHECK(to_string(wf) ==
              "<workflow><work name=\"1\" switch=\"on\">"
              "<plugin name=\"libechoplugin.so\" switch=\"on\" /></work></workflow>",
          "nested tree serializes");

    element mixed("a");
    mixed.text("text");
    element b("b", "x");
    mixed.append(b);
    CHECK(to_string(mixed) == "<a>text<b>x</b></a>",
          "text is emitted even when children exist (finding 5)");

    // a user-constructed tree with non-name chars still serializes to
    // well-formed xml (names parsed from xml can never contain these)
    element u("a&b");
    u.attr("k\"l", "v");
    CHECK(to_string(u) == "<a&amp;b k&quot;l=\"v\" />", "name and attr key are escaped");

    std::ostringstream os;
    os << empty;
    CHECK(os.str() == "<a />", "operator<< (non-member) serializes");
}

static void test_entities()
{
    std::printf("element: entity escape / document: entity decode\n");

    element t("a");
    t.text("a < b && c > d \"q\"");
    CHECK(to_string(t) == "<a>a &lt; b &amp;&amp; c &gt; d \"q\"</a>",
          "to_string escapes & < > in text (quotes are legal raw in text)");

    element q("a");
    q.attr("k", "x \"y\" & z");
    CHECK(to_string(q) == "<a k=\"x &quot;y&quot; &amp; z\" />",
          "to_string escapes & and quotes in attr values");

    document d;
    d.load_string("<a x=\"&quot;q&quot;\" a=\"&amp;&lt;&gt;\">&lt;b&gt; &amp; c</a>");
    element r = d.parse();
    CHECK(r.attr("x") == "\"q\"", "attr &quot; decodes to quote");
    CHECK(r.attr("a") == "&<>", "attr entities decode");
    CHECK(r.text() == "<b> & c", "text entities decode");
    CHECK(to_string(r) == "<a a=\"&amp;&lt;&gt;\" x=\"&quot;q&quot;\">"
                          "&lt;b&gt; &amp; c</a>",
          "decode then escape is a byte-identical round-trip (attr order = map order)");

    document d2;
    d2.load_string("<a x=\"&am;\">t</a>"); // partial entity
    element r2 = d2.parse();
    CHECK(r2.attr("x") == "&am;", "unrecognized entity is left as-is");

    // numeric character references decode to the char itself, so a
    // parse -> to_string round-trip cannot turn a valid "&#65;" into
    // "&amp;#65;" (the serializer escapes every '&')
    document d3;
    d3.load_string("<a x=\"&#65;&#x42;\">&#72;ello &#x4e2d;</a>");
    element r3 = d3.parse();
    CHECK(r3.attr("x") == "AB", "numeric refs &#dd; / &#xhh; decode in attr values");
    CHECK(r3.text() == std::string("Hello ") + "\xE4\xB8\xAD",
          "decimal and hex refs decode (multi-byte utf-8)");
    CHECK(to_string(r3) == std::string("<a x=\"AB\">Hello ") + "\xE4\xB8\xAD" + "</a>",
          "round-trip of a numeric-ref document is semantically stable");

    document d4;
    d4.load_string("<a x=\"&#x4 &#1114112; &#x110000; &#xD800; &am;\">t</a>");
    element r4 = d4.parse();
    CHECK(r4.attr("x") == "&#x4 &#1114112; &#x110000; &#xD800; &am;",
          "dangling, out-of-range and surrogate refs are left as-is");
}

static void test_document_parse()
{
    std::printf("document: parse of well-formed input\n");

    document d;
    d.load_string("<a>hello</a>");
    element a = d.parse();
    CHECK(a.name() == "a" && a.text() == "hello" && a.size() == 0, "simple text element");
    CHECK(a[""].name().empty(), "no phantom child in <a>hello</a> (finding 3)");

    element a2 = d.parse(); // re-parse on the same loaded document (finding 8)
    CHECK(a2.name() == "a" && a2.text() == "hello", "parse() resets m_idx and re-parses");

    document decl;
    decl.load_string("<?xml version=\"1.0\"?>\n<a x=\"1\">t</a>");
    element da = decl.parse();
    CHECK(da.name() == "a" && da.attr("x") == "1" && da.text() == "t",
          "declaration + element parses");

    document comments;
    comments.load_string("<!-- one --><!-- two --><a><!-- inner --><b>1</b></a>");
    element ca = comments.parse();
    CHECK(ca.name() == "a" && ca.size() == 1 && ca["b"].text() == "1",
          "repeated leading comments and inner comment parse");

    document single;
    single.load_string("<a x='single-quoted'></a>");
    element sa = single.parse();
    CHECK(sa.attr("x") == "single-quoted", "single-quoted attr value");

    // empty attribute values are preserved during parse (the public attr()
    // blank-deletes semantics does not apply to parsed trees)
    document emptyattr;
    emptyattr.load_string("<a x=\"\" y=\"1\">t</a>");
    element ea = emptyattr.parse();
    CHECK(ea.has_attr("x") && ea.attr("x") == "", "empty attr value is preserved");
    CHECK(ea.attr("y") == "1", "sibling non-empty attr unaffected");
    CHECK(to_string(ea) == "<a x=\"\" y=\"1\">t</a>", "empty attr round-trips byte-identical");

    document formatted;
    formatted.load_string("<a>\n    <b>1</b>\n    <c>2</c>\n</a>");
    element fa = formatted.parse();
    CHECK(fa.name() == "a" && fa.size() == 2 && fa["b"].text() == "1" &&
              fa["c"].text() == "2" && fa.text().empty(),
          "formatted newlines are ignorable whitespace");
    CHECK(to_string(fa) == "<a><b>1</b><c>2</c></a>", "formatted tree round-trips compact");

    document self;
    self.load_string("<a x=\"1\" />");
    element se = self.parse();
    CHECK(se.name() == "a" && se.attr("x") == "1" && se.size() == 0, "self-closing tag");
    CHECK(to_string(se) == "<a x=\"1\" />", "self-closing tag round-trips");

    // legal trailing content after the root: comments and processing instructions
    document trail;
    trail.load_string("<a>1</a><!-- tail -->\n<?xml-stylesheet href=\"x.css\"?>");
    element ta = trail.parse();
    CHECK(ta.name() == "a" && ta.text() == "1", "trailing comment + PI after root parse");

    // UTF-8 BOM is skipped
    document bom;
    bom.load_string(std::string("\xEF\xBB\xBF") + "<a x=\"1\">t</a>");
    element ba = bom.parse();
    CHECK(ba.name() == "a" && ba.attr("x") == "1" && ba.text() == "t", "BOM-prefixed document parses");

    // a processing instruction before the root is legal (as after it)
    document pre_pi;
    pre_pi.load_string("<?config mode=\"test\"?><a>1</a>");
    element pa = pre_pi.parse();
    CHECK(pa.name() == "a" && pa.text() == "1", "pre-root processing instruction parses");

    // a PI whose target starts with "xml" is still a PI, not the declaration
    document xml_style;
    xml_style.load_string("<?xml-stylesheet type=\"text/css\"?><a>1</a>");
    element xa = xml_style.parse();
    CHECK(xa.name() == "a" && xa.text() == "1", "pre-root <?xml-stylesheet?> PI parses");

    // the file the old smoke test used (finding 7: load_file now returns bool)
    {
        std::ifstream probe("bin/config/workflow.xml");
        if (probe)
        {
            document wf;
            CHECK(wf.load_file("bin/config/workflow.xml"), "load_file ok on existing file");
            element w = wf.parse();
            CHECK(w.name() == "workflow" && w.size() == 1, "workflow.xml root");
            element work = w["work"];
            CHECK(work.attr("name") == "1" && work.attr("switch") == "on", "workflow.xml work attrs");
            CHECK(work["plugin"].attr("name") == "libechoplugin.so", "workflow.xml plugin");
        }
        else
        {
            std::printf("  [note] bin/config/workflow.xml not found (cwd?), file checks skipped\n");
        }
        document missing;
        CHECK(!missing.load_file("no/such/file.xml"), "load_file false on missing file");

        // non-regular files must be rejected up front: rdbuf() reads until
        // EOF, and these never have one (old code: /dev/zero hung forever
        // growing the buffer; a directory faked a 0-byte success on Linux)
        document zero;
        CHECK(!zero.load_file("/dev/zero"), "load_file false on /dev/zero (never EOFs)");
        {
            const char tmp_dir[] = "xml.test.tmp.dir";
            ::mkdir(tmp_dir, 0755);
            document dirf;
            CHECK(!dirf.load_file(tmp_dir), "load_file false on a directory");
            ::rmdir(tmp_dir);
        }
        {
            const char tmp_name[] = "xml.test.tmp0.xml";
            std::ofstream out(tmp_name, std::ios::binary); // creates an empty file
            out.close();
            document zb;
            CHECK(zb.load_file(tmp_name), "load_file true on a 0-byte file (read in full)");
            CHECK_THROWS_XML(zb.parse(), "0-byte file: parse throws 'empty document'");
            std::remove(tmp_name);
        }

        // a file the test itself wrote (exercises the read-size guard)
        {
            const char tmp_name[] = "xml.test.tmp.xml";
            {
                std::ofstream out(tmp_name, std::ios::binary);
                out << "<?xml version=\"1.0\"?>\n<a x=\"1\">\n  <b>2</b>\n</a>\n";
            }
            document tf;
            CHECK(tf.load_file(tmp_name), "load_file ok on a file written by the test");
            element tfe = tf.parse();
            CHECK(tfe.name() == "a" && tfe.attr("x") == "1" && tfe["b"].text() == "2" &&
                      tfe.text().empty(),
                  "written file parses (newlines are ignorable whitespace)");
            std::remove(tmp_name);
        }
    }

    // depth limit (finding 9)
    {
        std::string deep = "<a>";
        for (int i = 0; i < 300; ++i)
        {
            deep += "<a>";
        }
        for (int i = 0; i < 300; ++i)
        {
            deep += "</a>";
        }
        deep += "</a>";
        document dd;
        dd.load_string(deep);
        CHECK_THROWS_XML(dd.parse(), "300-deep nesting throws (depth limit)");

        std::string ok = "<a>";
        for (int i = 0; i < 100; ++i)
        {
            ok += "<b>1</b>";
        }
        ok += "</a>"; // 100 wide is fine, depth 1
        document wd;
        wd.load_string(ok);
        element we = wd.parse();
        CHECK(we.size() == 100, "100 siblings parse (limit is on depth)");
    }
}

static void test_malformed()
{
    std::printf("document: malformed input must throw (was: hang / OOB / silent)\n");

    document d;
    d.load_string("");
    CHECK_THROWS_XML(d.parse(), "empty document");

    d.load_string("   \n\t ");
    CHECK_THROWS_XML(d.parse(), "whitespace-only document");

    d.load_string("<");
    CHECK_THROWS_XML(d.parse(), "lone '<' (old: silent empty element)");

    d.load_string("3");
    CHECK_THROWS_XML(d.parse(), "non-element root (old: silent empty element)");

    d.load_string("<a>hello");
    CHECK_THROWS_XML(d.parse(), "unclosed element (old: OOB read + infinite loop)");

    d.load_string("<a x=\"v>");
    CHECK_THROWS_XML(d.parse(), "unterminated attr quote (old: OOB read + infinite loop)");

    d.load_string("<a x=1>");
    CHECK_THROWS_XML(d.parse(), "unquoted attr value");

    d.load_string("<a x >");
    CHECK_THROWS_XML(d.parse(), "attr without '='");

    d.load_string("<a x");
    CHECK_THROWS_XML(d.parse(), "unterminated start tag");

    d.load_string("<a / >");
    CHECK_THROWS_XML(d.parse(), "space between '/' and '>'");

    d.load_string("<a><!DOCTYPE x>text</a>");
    CHECK_THROWS_XML(d.parse(), "DOCTYPE child (old: infinite loop + unbounded growth)");

    d.load_string("<a><3>x</3></a>");
    CHECK_THROWS_XML(d.parse(), "digit-first tag (old: infinite loop + unbounded growth)");

    d.load_string("<a><b></a></b>");
    CHECK_THROWS_XML(d.parse(), "mismatched end tag, match exists later (old: infinite loop)");

    d.load_string("<a></b></a>");
    CHECK_THROWS_XML(d.parse(), "mismatched end tag at close (old: infinite loop)");

    d.load_string("<a></b>");
    CHECK_THROWS_XML(d.parse(), "mismatched end tag, no match at all");

    {
        document t;
        t.load_string("<a></a"); // end tag truncated at end of input
        bool threw = false;
        try
        {
            (void)t.parse();
        }
        catch (const std::logic_error &ex)
        {
            threw = true;
            CHECK(std::string(ex.what()).find("not closed") != std::string::npos,
                  "truncated end tag reports 'not closed' (not a mismatch)");
        }
        CHECK(threw, "truncated end tag");
    }

    d.load_string("<?xml version=\"1.0\"");
    CHECK_THROWS_XML(d.parse(), "unterminated declaration");

    {
        document t;
        t.load_string("<?xml-stylesheet href=\"x.css\"");
        bool threw = false;
        try
        {
            (void)t.parse();
        }
        catch (const std::logic_error &ex)
        {
            threw = true;
            CHECK(std::string(ex.what()).find("processing instruction") != std::string::npos,
                  "unterminated pre-root <?xml- PI reports as PI, not declaration");
        }
        CHECK(threw, "unterminated pre-root <?xml- PI");
    }

    d.load_string("<!-- c");
    CHECK_THROWS_XML(d.parse(), "unterminated comment");

    d.load_string("<!-- c -->");
    CHECK_THROWS_XML(d.parse(), "comment-only document has no root");

    d.load_string("<?foo?>");
    CHECK_THROWS_XML(d.parse(), "processing-instruction-only document has no root");

    d.load_string("<a><!---></a>");
    CHECK_THROWS_XML(d.parse(), "invalid comment <!---> (terminator overlaps the opening)");

    d.load_string("<a>text<b>1</b>tail</a>");
    CHECK_THROWS_XML(d.parse(), "text after child elements (mixed tail)");

    d.load_string("<a></a>trailing");
    CHECK_THROWS_XML(d.parse(), "trailing content after root");

    d.load_string("<a></a><b></b>");
    CHECK_THROWS_XML(d.parse(), "double root");

    d.load_string("<a></a><!-- unterminated");
    CHECK_THROWS_XML(d.parse(), "unterminated trailing comment");

    d.load_string("<a></a><?xml-stylesheet href=\"x.css\"");
    CHECK_THROWS_XML(d.parse(), "unterminated trailing PI");

    d.load_string("<a x=\"1\" x=\"2\">t</a>");
    CHECK_THROWS_XML(d.parse(), "duplicate attribute");

    d.load_string("<a x=\"\" x=\"2\">t</a>");
    CHECK_THROWS_XML(d.parse(), "duplicate attribute even when the first value is empty");

    d.load_string("<a><![CDATA[x < y]]></a>");
    CHECK_THROWS_XML(d.parse(), "CDATA section (not supported, must throw not misparse)");

    {
        document t;
        t.load_string("<a><![CDATA[x < y]]></a>");
        bool threw = false;
        try
        {
            (void)t.parse();
        }
        catch (const std::logic_error &ex)
        {
            threw = true;
            CHECK(std::string(ex.what()).find("CDATA") != std::string::npos,
                  "CDATA error names CDATA (documented in document.h, not a generic 'malformed')");
        }
        CHECK(threw, "CDATA error message check");
    }

    d.load_string("\xEF\xBB\xBFinvalid");
    CHECK_THROWS_XML(d.parse(), "BOM then non-element root still throws");
}

int main()
{
    std::setvbuf(stdout, nullptr, _IOLBF, 0); // keep [ok]/[FAIL] order honest on abort
    test_element_basics();
    test_to_string();
    test_entities();
    test_document_parse();
    test_malformed();

    std::printf("\n%d checks, %d failed\n", g_passed + g_failed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
