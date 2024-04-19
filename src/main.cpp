#include <iostream>
#include <string>

#include "utility/singleton.h"
#include "system/system.h"

using avant::utility::singleton;

int main(int argc, char **argv)
{
    return singleton<avant::system::system>::instance()->init();
}