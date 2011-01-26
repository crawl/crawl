/*
 *  File:       tutorial.cc
 *  Summary:    Collection of tutorial related functions.
 */

#include "AppHdr.h"

#include "externs.h"
#include "maps.h"
#include "message.h"
#include "mpr.h"
#include "player.h"
#include "state.h"
#include "tutorial.h"

mapref_vector get_tutorial_maps()
{
    return find_maps_for_tag("tutorial_start");
}

// We could save this in crawl_state instead.
// Or choose_game() could save *ng to crawl_state
// entirely, though that'd be redundant with
// you.your_name, you.species, crawl_state.type, ...

static std::string _tutorial_map;

std::string get_tutorial_map()
{
    return _tutorial_map;
}

void set_tutorial_map(const std::string& map)
{
    _tutorial_map = map;
}

void set_tutorial_hunger(int hunger)
{
    if (!crawl_state.game_is_tutorial())
        return;

    you.hunger = hunger;
}

void tutorial_death_message()
{
    mprnojoin("You die...");
    mprnojoin("In Crawl, death is a sad but common occurence. "
              "Note that there's usually something you could have done to "
              "survive, for example by using some kind of item, running away, "
              "resting between fights, or by avoiding combat entirely. "
              "Keep trying, eventually you'll prevail!",
              MSGCH_TUTORIAL);
    more();
}
