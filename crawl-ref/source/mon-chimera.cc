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

    apply_chimera_part(mon,part1,1);
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

    if (mons_is_batty(&dummy))
        mon->props["chimera_batty"].get_int() = partnum;
    else if (mons_flies(&dummy))
        mon->props["chimera_wings"].get_int() = partnum;

    // Apply spells but only for 2nd and 3rd parts since 1st part is
    // already supported by the original define_monster call
    if (partnum == 1)
        return;

    // XXX: It'd be nice to flood fill all available spell slots with spells
    // from parts 2 and 3. But since this would conflict with special
    // slots (emergency, enchantment, etc.) some juggling is needed, until
    // spell slots can be made more sensible.

    // Use misc slots (3+4) for the primary spells of parts 1 & 2
    const int boltslot = partnum + 1;
    // Overwrite the base monster's misc spells if they had any
    if (dummy.spells[0] != SPELL_NO_SPELL)
        mon->spells[boltslot] = dummy.spells[0];

    // Other spell slots overwrite if the base monster(s) didn't have one
    // Enchantment
    if (mon->spells[1] == SPELL_NO_SPELL && dummy.spells[1] != SPELL_NO_SPELL)
        mon->spells[1] = dummy.spells[1];
    // Self-enchantment
    if (mon->spells[2] == SPELL_NO_SPELL && dummy.spells[2] != SPELL_NO_SPELL)
        mon->spells[2] = dummy.spells[2];
    // Emergency
    if (mon->spells[5] == SPELL_NO_SPELL && dummy.spells[5] != SPELL_NO_SPELL)
        mon->spells[5] = dummy.spells[5];
}

monster_type get_chimera_part(const monster* mon, int partnum)
{
    ASSERT_RANGE(partnum,1,4);
    if (partnum == 1) return mon->base_monster;
    if (partnum == 2 && mon->props.exists("chimera_part_2"))
        return static_cast<monster_type>(mon->props["chimera_part_2"].get_int());
    if (partnum == 3 && mon->props.exists("chimera_part_3"))
        return static_cast<monster_type>(mon->props["chimera_part_3"].get_int());
    return MONS_PROGRAM_BUG;
}

monster_type get_chimera_part(const monster_info* mi, int partnum)
{
    ASSERT_RANGE(partnum,1,4);
    if (partnum == 1) return mi->base_type;
    if (partnum == 2 && mi->props.exists("chimera_part_2"))
        return static_cast<monster_type>(mi->props["chimera_part_2"].get_int());
    if (partnum == 3 && mi->props.exists("chimera_part_3"))
        return static_cast<monster_type>(mi->props["chimera_part_3"].get_int());
    return MONS_PROGRAM_BUG;
}

bool chimera_is_batty(const monster* mon)
{
    return mon->props.exists("chimera_batty");
}

monster_type get_chimera_wings(const monster* mon)
{
    if (chimera_is_batty(mon))
        return get_chimera_part(mon, mon->props["chimera_batty"].get_int());
    if (mon->props.exists("chimera_wings"))
        return get_chimera_part(mon, mon->props["chimera_wings"].get_int());
    return MONS_NO_MONSTER;
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
