#include <iostream>
#include "../src/xml/element.h"
#include "../src/xml/document.h"

using namespace std;
using namespace avant::xml;

int main(int argc, char **argv)
{
    element el;
    el.name("div");

    // el.append(element("span"));
    el.text("hello world");

    el.operator<<(cout); //<div>hello world</div>
    cout << endl;

    document m_document;
    m_document.load_file("../bin/config/workflow.xml");
    element root = m_document.parse();
    root.operator<<(cout);
    cout << endl;

    return 0;
}
// g++ ../src/xml/element.cpp ../src/xml/document.cpp xml.test.cpp -o xml.test.exe
// ./xml.test.exe
