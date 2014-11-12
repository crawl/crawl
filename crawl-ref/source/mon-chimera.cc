/**
 * @file
 * @brief Chimeric beasties
**/

#include "AppHdr.h"

#include "mon-chimera.h"

#include <sstream>

#include "ghost.h"
#include "mgen_data.h"
#include "mon-info.h"
#include "mon-pick.h"
#include "monster.h"

static bool is_bad_chimera_part(monster_type part);
static bool is_valid_chimera_part(monster_type part);

void mgen_data::define_chimera(monster_type part1, monster_type part2,
                               monster_type part3)
{
    // Set base_type; some things might still refer to that
    base_type = part1;
    chimera_mons.push_back(part1);
    chimera_mons.push_back(part2);
    chimera_mons.push_back(part3);
}

void ghost_demon::init_chimera(monster* mon, monster_type parts[])
{
    ASSERTPART(0);
    ASSERTPART(1);
    ASSERTPART(2);

    // Set type to the original type to calculate appropriate stats.
    mon->type = parts[0];
    mon->base_monster = MONS_PROGRAM_BUG;
    define_monster(mon);
    mon->type         = MONS_CHIMERA;
    colour = mons_class_colour(MONS_CHIMERA);
    mon->base_monster = parts[0];
    mon->props["chimera_part_2"] = parts[1];
    mon->props["chimera_part_3"] = parts[2];

    resists = 0;
    size_t spellcount = 0;
    for (int i = 0; i < 3; i++)
        if (_apply_chimera_part(mon, parts[i], i+1))
            spellcount++;

    // Scale spell/ability frequencies; if we don't do this, a chimera might
    // end up with a sum of spell frequencies greater than 200, preventing
    // some from being used.
    if (spellcount > 1)
    {
        for (auto &slot : spells)
            slot.freq /= spellcount;
    }

    // If one part has wings, take an average of base speed and the
    // speed of the winged monster.
    monster_type wings = get_chimera_wings(mon);
    if (wings != MONS_NO_MONSTER)
        fly = mons_class_flies(wings);
    monster_type legs = get_chimera_legs(mon);
    if (legs == MONS_NO_MONSTER)
        legs = parts[0];
    speed = mons_class_base_speed(legs);
    if (wings != MONS_NO_MONSTER && wings != legs)
        speed = (speed + mons_class_base_speed(wings)) / 2;
}

// Randomly pick depth-appropriate chimera parts
bool ghost_demon::init_chimera_for_place(monster* mon,
                                         level_id place,
                                         monster_type chimera_type,
                                         coord_def pos)
{
    monster_type parts[3];
    monster_picker picker = positioned_monster_picker(pos);
    for (int n = 0; n < 3; n++)
    {
        monster_type part = pick_monster(place, picker, is_bad_chimera_part);
        if (part == MONS_0)
        {
            part = pick_monster_all_branches(place.absdepth(), picker,
                                             is_bad_chimera_part);
        }
        if (part != MONS_0)
            parts[n] = part;
        else
            return false;
    }
    init_chimera(mon, parts);
    return true;
}

monster_type chimera_part_for_place(level_id place, monster_type chimera_type)
{
    monster_type part = pick_monster(place, is_bad_chimera_part);
    if (part == MONS_0)
        part = pick_monster_all_branches(place.absdepth(), is_bad_chimera_part);
    return part;
}

static bool is_valid_chimera_part(monster_type part)
{
    return !(part == MONS_NO_MONSTER
             || part == MONS_CHIMERA
             || invalid_monster_type(part)
             || mons_class_is_zombified(part)
             || mons_class_flag(part, M_NO_GEN_DERIVED)); // TODO: Chimera zombie
}

// Indicates preferred chimera parts
// TODO: Should maybe check any of:
// mons_is_object / mons_is_conjured / some of mons_has_flesh
// is_stationary / mons_class_is_firewood / mons_is_mimic / mons_class_holiness
static bool is_bad_chimera_part(monster_type part)
{
    return (!is_valid_chimera_part(part))
           || mons_class_is_hybrid(part)
           || mons_class_is_zombified(part)
           || mons_species(part) != part
           || mons_class_intel(part) > I_NORMAL
           || mons_class_intel(part) < I_INSECT
           || mons_is_unique(part);
}

/**
 * Apply the characteristics of a chimera part to the chimera.
 *
 * @param  chimera    The chimera.
 * @param  part       The type of monster this part is.
 * @param  partnum    Which part of the chimera this is.
 * @return            Whether this part of the chimera had abilities/spells.
 */
bool ghost_demon::_apply_chimera_part(monster* mon, monster_type part,
                                      int partnum)
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

    // Check for a legs part. Jumpy behaviour (jumping spiders) should
    // override normal clinging.
    if (dummy.is_jumpy()
        || (dummy.can_cling_to_walls() && !mon->props.exists("chimera_legs")))
    {
        ev = dummy.evasion();
        mon->props["chimera_legs"].get_int() = partnum;
    }

    if (dummy.can_see_invisible())
        see_invis = true;

    // Transfer all resists in this list
    const static mon_resist_flags resist_list[] =
        { MR_RES_FIRE, MR_RES_COLD, MR_RES_ELEC, MR_RES_POISON, MR_RES_NEG,
          MR_RES_ACID, MR_RES_STEAM, MR_RES_STICKY_FLAME, MR_RES_ASPHYX,
          MR_RES_ROTTING, MR_RES_PETRIFY, MR_RES_WIND, MR_RES_TORMENT };

    for (unsigned int n = 0; n < ARRAYSZ(resist_list); ++n)
    {
        const mon_resist_flags res_flag = resist_list[n];
        const int part_resist = get_mons_resist(&dummy, res_flag);
        const int cur_resist = get_resist(resists, res_flag);
        const int new_resist = res_flag > MR_LAST_MULTI
            ? (part_resist | cur_resist) // Boolean resists
            : (part == 1 ? part_resist
                         : min(4, max(-3, part_resist + cur_resist))); // Additive resists

        set_resist(resists, res_flag, new_resist);
    }

    // Apply spells but only for 2nd and 3rd parts since 1st part is
    // already supported by the original define_monster call
    if (partnum == 1)
    {
        // Always AC/EV on the first part
        ac = dummy.armour_class();
        ev = dummy.evasion();
        max_hp = dummy.max_hit_points;
        xl = dummy.get_hit_dice();
        // Copy all spells from first part
        spells = dummy.spells;
        return spells.size() > 0;
    }

    // Add spells and abilities from the current part.
    for (const auto &slot : dummy.spells)
    {
        if (slot.spell != SPELL_NO_SPELL)
            spells.push_back(slot);
    }

    return dummy.spells.size() > 0;
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

monster_type random_chimera_part(const monster* mon)
{
    ASSERT(mon->type == MONS_CHIMERA);
    return get_chimera_part(mon, random2(3) + 1);
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

monster_type get_chimera_legs(const monster* mon)
{
    if (mon->props.exists("chimera_legs"))
        return get_chimera_part(mon, mon->props["chimera_legs"].get_int());
    return get_chimera_part(mon, 1);
}

string monster_info::chimera_part_names() const
{
    if (!props.exists("chimera_part_2") || !props.exists("chimera_part_3"))
        return "";

    monster_type chimtype2 = static_cast<monster_type>(props["chimera_part_2"].get_int());
    monster_type chimtype3 = static_cast<monster_type>(props["chimera_part_3"].get_int());
    ASSERT_RANGE(chimtype2, MONS_PROGRAM_BUG + 1, NUM_MONSTERS);
    ASSERT_RANGE(chimtype3, MONS_PROGRAM_BUG + 1, NUM_MONSTERS);

    ostringstream s;
    s << ", " << get_monster_data(chimtype2)->name
      << ", " << get_monster_data(chimtype3)->name;
    return s.str();
}
