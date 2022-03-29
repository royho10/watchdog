#include <stdio.h>  /* printf() */
#include <unistd.h> /* sleep() TODO: remove after debug  */

#include "wd.h" /* WDStart */

int main(int argc, char **argv)
{
    unsigned int left_to_sleep = 15;
    (void)argc;

    if (WDStart(argv))
    {
        printf("failed\n");

        return 1;
    }

    while (left_to_sleep)
    {
        left_to_sleep = sleep(left_to_sleep);
    }

    WDStop();

    printf("succeeded\n");

    return 0;
}