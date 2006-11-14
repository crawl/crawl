#include "AppHdr.h"
#include "levcomp.h"

std::string lc_desfile;
map_def     lc_map;
level_range lc_range;
level_range lc_default_depth;

void reset_map_parser()
{
    lc_desfile.clear();
    lc_map.init();
    lc_range.reset();
    lc_default_depth.reset();
}
