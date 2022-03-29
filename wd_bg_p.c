#include <stdio.h> /* fprintf() TODO: remove after debug */
#include "wd.h"    /* WDStart */

extern int g_i_am_app;

int main(int argc, char **argv)
{
    (void)argc;
    g_i_am_app = 0;

    if (WDStart(argv))
    {
        return 1;
    }

    fprintf(stderr, "WD finished successfully\n");

    return 0;
}