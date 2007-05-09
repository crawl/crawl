#include "AppHdr.h"
#include "levcomp.h"
#include <vector>

std::string lc_desfile;
map_def     lc_map;
level_range lc_range;
depth_ranges lc_default_depths;

extern int yylineno;

void reset_map_parser()
{
    lc_map.init();
    lc_range.reset();
    lc_default_depths.clear();

    yylineno = 1;
}
