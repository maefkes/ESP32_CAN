#include "unity_fixture.h"


static void run_all_tests(void)
{
    RUN_TEST_GROUP(BmsCommunication);
}

int main(int argc, const char * argv[])
{
    return UnityMain(argc, argv, run_all_tests);
}

/**************************************************
 * Beispielpfad msys: /c/Users/marvin_schermutzki/Desktop/repos/NUCLEO_F207ZG/test
 * meson setup --wipe build
 * meson test -C build -v
 **************************************************/
