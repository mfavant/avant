#include <iostream>
#include <string>

#include "utility/singleton.h"
#include "system/system.h"

using avant::utility::singleton;

int main(int argc, char **argv)
{
    avant::system::system g_system;
    return g_system.init();
}
