#include <cstdio>
#include <string>
#include <vector>
#include "mapdef.h"

// [dshaligram] It may be better to build the web of conditional #ifdefs to
// figure out which header this is in by platform...
extern "C" {
    extern int unlink(const char *);
}

extern map_def map;
extern level_range range;
extern level_range default_depth;
extern const char *autogenheader;
extern const char *outfilename;
extern FILE *outhandle;
extern bool autowarned;
extern bool write_append;
