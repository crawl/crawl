/**
 * @file
 * @brief Chimeric beasties
**/

#include "AppHdr.h"
#include "mon-chimera.h"

#include "externs.h"
#include "enum.h"
#include "mon-info.h"
#include "mon-util.h"
#include "monster.h"

#include <sstream>

static void apply_chimera_part(monster* mon, monster_type part, int partnum);

void make_chimera(monster* mon, monster_type part1, monster_type part2, monster_type part3)
{
    ASSERT(part1 != MONS_NO_MONSTER);
    ASSERT(!invalid_monster_type(part1));
    ASSERT(part2 != MONS_NO_MONSTER);
    ASSERT(!invalid_monster_type(part2));
    ASSERT(part3 != MONS_NO_MONSTER);
    ASSERT(!invalid_monster_type(part3));

    // Set type to the original type to calculate appropriate stats.
    mon->type = part1;
    mon->base_monster = MONS_PROGRAM_BUG;
    define_monster(mon);

    mon->type         = MONS_CHIMERA;
    mon->base_monster = part1;
    mon->props["chimera_part_2"] = part2;
    mon->props["chimera_part_3"] = part3;
    apply_chimera_part(mon,part2,2);
    apply_chimera_part(mon,part3,3);
}

static void apply_chimera_part(monster* mon, monster_type part, int partnum)
{
    // TODO: Enforce more rules about the Chimera parts so things
    // can't get broken
    ASSERT(!mons_class_is_zombified(part));

    // Create a temp monster to transfer properties
    monster dummy;
    dummy.type = part;
    define_monster(&dummy);
}

string monster_info::chimera_part_names() const
{
    if (!props.exists("chimera_part_2") || !props.exists("chimera_part_3"))
        return "";

    monster_type chimtype2 = static_cast<monster_type>(props["chimera_part_2"].get_int());
    monster_type chimtype3 = static_cast<monster_type>(props["chimera_part_3"].get_int());
    ASSERT(chimtype2 > MONS_PROGRAM_BUG && chimtype2 < NUM_MONSTERS);
    ASSERT(chimtype3 > MONS_PROGRAM_BUG && chimtype3 < NUM_MONSTERS);

    ostringstream s;
    s << ", " << get_monster_data(chimtype2)->name
      << ", " << get_monster_data(chimtype3)->name;
    return s.str();
}
