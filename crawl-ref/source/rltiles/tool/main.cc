#include <stdio.h>
#include "tile_list_processor.h"

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s (tile_list.txt)\n", argv[0]);
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
