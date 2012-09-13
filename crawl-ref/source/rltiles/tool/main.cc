#include <stdio.h>
#include <string.h>
#include "tile_list_processor.h"

#ifdef USE_TILE
  #include <SDL_main.h>
#endif

static void _usage(const char *fname)
{
    fprintf(stderr, "Usage: %s [-i] [-c] [-l] (tile_list.txt)\n", fname);
}

int main(int argc, char **argv)
{
    int arg = 1;
    bool image = false;
    bool code  = false;
    for (; arg < argc; arg++)
    {
        if (argv[arg][0] != '-')
        {
            if (arg == argc - 1)
                break;
            _usage(argv[0]);
            return -1;
        }
        switch (argv[arg][1])
        {
        case 'l':
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
            break;
        case 'i':
            image = true;
            break;
        case 'c':
            code = true;
            break;
        default:
            _usage(argv[0]);
            return -1;
            break;
        }
    }

    if (arg >= argc)
    {
        _usage(argv[0]);
        return -1;
    }

    if (!image && !code)
        image = code = true; // Backwards compatibility.

    tile_list_processor proc;

    if (!proc.process_list(argv[arg]))
    {
        fprintf(stderr, "Error: failed to process '%s'\n", argv[1]);
        return -2;
    }

    if (!proc.write_data(image, code))
    {
        fprintf(stderr, "Error: failed to write data for '%s'\n", argv[1]);
        return -3;
    }

    return 0;
}
