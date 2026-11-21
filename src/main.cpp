#include "system/system.h"

int main(int argc, char **argv)
{
    avant::system::system g_system;
    return g_system.init();
}
