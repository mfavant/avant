#include <iostream>
#include <string>

#include "utility/singleton.h"
#include "system/system.h"

using avant::utility::singleton;

int main(int argc, char **argv)
{
    avant::system::system avant_global_system;
    return avant_global_system.init();
}
