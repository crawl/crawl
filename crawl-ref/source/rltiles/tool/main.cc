#include <stdio.h>
#include <string.h>
#include "tile_list_processor.h"

#ifdef USE_TILE
  #include <SDL_main.h>
#endif

int main(int argc, char **argv)
{
    if (argc == 3 && !strcmp(argv[1], "-l"))
    {
#ifndef USE_TILE
        fprintf(stderr, "Can't record used tiles in console builds.\n");
        return -1;
#endif

        logfile = fopen("used_tiles.log", "a");
        if (!logfile)
        {
            fprintf(stderr, "Can't write to used_tiles.log\n");
            return -1;
        }
        // Line buffering, for poor man's near-atomicity.  The string is
        // always small enough to fit in one syscall, which keeps them whole
        // in most cases (non-NFS) but is not guaranteed by any standard.
        //
        // This is ok because regular builds don't use "-l".
        setvbuf(logfile, 0, _IOLBF, 0);
        argv++;
    }
    else if (argc != 2)
    {
        fprintf(stderr, "Usage: %s [-l] (tile_list.txt)\n", argv[0]);
        return -1;
    }

    tile_list_processor proc;

    if (!proc.process_list(argv[1]))
    {
        fprintf(stderr, "Error: failed to process '%s'\n", argv[1]);
        return -2;
    }

    if (!proc.write_data())
    {
        fprintf(stderr, "Error: failed to write data for '%s'\n", argv[1]);
        return -3;
    }

    return 0;
}
