/**
 * @file
 * @brief Code for the god menu.
 * @todo The god menu.
 */

#include "AppHdr.h"
#include "godmenu.h"

#include "colour.h"
#include "libutil.h"
#include "religion.h"

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
