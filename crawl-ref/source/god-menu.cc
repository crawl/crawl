/**
 * @file
 * @brief Code for the god menu.
 * @todo The god menu.
 */

#include "AppHdr.h"

#include "god-menu.h"

#include "colour.h"
#include "libutil.h"
#include "religion.h"
#ifdef USE_TILE
#include "terrain.h"
#include "tilepick.h"
#include "tileview.h"
#endif

GodMenuEntry::GodMenuEntry(god_type god_, bool long_name) :
    MenuEntry(god_name(god_, long_name), MEL_ITEM, 1, 0, false),
    god(god_)
{
    if (god == GOD_SHINING_ONE)
        hotkeys.push_back('1');
    else
    {
        hotkeys.push_back(text.at(0));
        hotkeys.push_back(toalower(text.at(0)));
    }
    int c = god_message_altar_colour(god);
    colour_text = colour_to_str(c);
    data = &text;

#ifdef USE_TILE
    const dungeon_feature_type feat = altar_for_god(god_);
    if (feat)
    {
        const tileidx_t idx = tileidx_feature_base(feat);
        add_tile(tile_def(pick_dngn_tile(idx, ui_random(INT_MAX)),
                                         get_dngn_tex(idx)));
        // TODO: randomize tile for jiyva, xom
    }
#endif
}

string GodMenuEntry::get_text(const bool unused) const
{
    if (level == MEL_ITEM && hotkeys.size())
    {
        char buf[300];
        snprintf(buf, sizeof buf, " <%s>%c</%s> %c %s",  colour_text.c_str(),
                 hotkeys[0], colour_text.c_str(), preselected ? '+' : '-', text.c_str());
        return string(buf);
    }
    return text;
}
