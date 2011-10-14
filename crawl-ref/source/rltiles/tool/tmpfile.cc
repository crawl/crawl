/*
 * Replacement for MSVCRT's broken implementation of tmpfile(): it
 * attempts to place the files in the root directory, which obviously
 * doesn't work for non-admin users...
 */

#include <stdio.h>
#include <malloc.h>

FILE *tmpfile(void)
{
    char *fn = tempnam(NULL, "tileg");

    // T: short-lived
    // D: delete-on-close
    FILE *fp = fopen(fn, "w+TD");

    free(fn);

    return fp;
}
