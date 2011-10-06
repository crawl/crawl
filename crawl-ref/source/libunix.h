#ifndef LIBUNIX_H
#define LIBUNIX_H

#ifndef O_BINARY
#define O_BINARY 0
#endif

#ifndef USE_TILE_LOCAL

#include <stdio.h>

#include "libconsole.h"
void fakecursorxy(int x, int y);
#endif
#endif
