/**
 * @file
 * @brief Misc monster related functions.
**/

#include "AppHdr.h"

#include "mon-util.h"

#include <algorithm>
#include <cmath>
#include <sstream>

#include "act-iter.h"
#include "areas.h"
#include "artefact.h"
#include "attitude-change.h"
#include "beam.h"
#include "cloud.h"
#include "colour.h"
#include "coordit.h"
#include "database.h"
#include "delay.h"
#include "dgn-overview.h"
#include "directn.h"
#include "dungeon.h"
#include "english.h"
#include "env.h"
#include "tile-env.h"
#include "errors.h"
#include "fight.h"
#include "files.h"
#include "fprop.h"
#include "ghost.h"
#include "god-abil.h"
#include "god-companions.h"
#include "god-item.h"
#include "god-passive.h"
#include "item-name.h"
#include "item-prop.h"
#include "items.h"
#include "libutil.h"
#include "mapmark.h"
#include "message.h"
#include "misc.h"
#include "mgen-data.h"
#include "mon-abil.h"
#include "mon-behv.h"
#include "mon-book.h"
#include "mon-cast.h"
#include "mon-death.h"
#include "mon-explode.h"
#include "mon-place.h"
#include "mon-poly.h"
#include "mon-tentacle.h"
#include "mon-util.h"
#include "mutant-beast.h"
#include "notes.h"
#include "options.h"
#include "random.h"
#include "reach-type.h"
#include "religion.h"
#include "showsymb.h"
#include "species.h"
#include "spl-util.h"
#include "state.h"
#include "stringutil.h"
#include "tag-version.h"
#include "terrain.h"
#include "rltiles/tiledef-player.h"
#include "tilepick.h"
#include "tileview.h"
#include "timed-effects.h"
#include "transform.h"
#include "traps.h"
#include "unicode.h"
#include "unwind.h"

static FixedVector < int, NUM_MONSTERS > mon_entry;

struct mon_display
{
    char32_t glyph;
    colour_t colour;

    mon_display(unsigned gly = 0, unsigned col = 0)
       : glyph(gly), colour(col) { }
};

static mon_display monster_symbols[NUM_MONSTERS];

static bool initialised_randmons = false;
static vector<monster_type> monsters_by_habitat[NUM_HABITATS];
static vector<monster_type> species_by_habitat[NUM_HABITATS];

#include "mon-spell.h"
#include "mon-data.h"

#define MONDATASIZE ARRAYSZ(mondata)

static bool _give_apostle_proper_name(monster& mon, apostle_type type);
static int _mons_exp_mod(monster_type mclass);

// Macro that saves some typing, nothing more.
#define smc get_monster_data(mc)
// ASSERT(smc) was getting really old
#define ASSERT_smc()                                                    \
    do {                                                                \
        if (!get_monster_data(mc))                                      \
            die("bogus mc (no monster data): %s (%d)",                  \
                mons_type_name(mc, DESC_PLAIN).c_str(), mc);            \
    } while (false)

/* ******************** BEGIN PUBLIC FUNCTIONS ******************** */

bool monster_class_flies(monster_type mc)
{
    return mons_class_flag(mc, M_FLIES);
}

bool monster_inherently_flies(const monster &mons)
{
    // check both so spectral humans and zombified dragons both fly
    return monster_class_flies(mons.type)
        || monster_class_flies(mons_base_type(mons))
        || mons_is_draconian_job(mons.type)
            && monster_class_flies(draconian_subspecies(mons))
        || mons_is_ghost_demon(mons.type) && mons.ghost && mons.ghost->flies
        || mons.has_facet(BF_BAT);
}

static habitat_type _grid2habitat(dungeon_feature_type grid)
{
    if (feat_is_water(grid))
        return HT_WATER;

    switch (grid)
    {
    case DNGN_LAVA:
        return HT_LAVA;
    case DNGN_FLOOR:
    default:
        return HT_LAND;
    }
}

dungeon_feature_type habitat2grid(habitat_type ht)
{
    switch (ht)
    {
    case HT_WATER:
        return DNGN_DEEP_WATER;
    case HT_LAVA:
        return DNGN_LAVA;
    case HT_LAND:
    case HT_AMPHIBIOUS:
    case HT_AMPHIBIOUS_LAVA:
    default:
        return DNGN_FLOOR;
    }
}

static void _initialise_randmons()
{
    for (int i = 0; i < NUM_HABITATS; ++i)
    {
        set<monster_type> tmp_species;
        const dungeon_feature_type feat = habitat2grid(habitat_type(i));

        for (monster_type mt = MONS_0; mt < NUM_MONSTERS; ++mt)
        {
            if (invalid_monster_type(mt))
                continue;

            if (monster_habitable_feat(mt, feat))
                monsters_by_habitat[i].push_back(mt);

            const monster_type species = mons_species(mt);
            if (monster_habitable_feat(species, feat))
                tmp_species.insert(species);

        }

        for (auto type : tmp_species)
            species_by_habitat[i].push_back(type);
    }
    initialised_randmons = true;
}

monster_type random_monster_at_grid(const coord_def& p, bool species)
{
    if (!initialised_randmons)
        _initialise_randmons();

    const habitat_type ht = _grid2habitat(env.grid(p));
    const vector<monster_type> &valid_mons = species ? species_by_habitat[ht]
                                                     : monsters_by_habitat[ht];

    ASSERT(!valid_mons.empty());
    return valid_mons.empty() ? MONS_PROGRAM_BUG
                              : valid_mons[ random2(valid_mons.size()) ];
}

typedef map<string, monster_type> mon_name_map;
static mon_name_map Mon_Name_Cache;

void init_mon_name_cache()
{
    if (!Mon_Name_Cache.empty())
        return;

    for (const monsterentry &me : mondata)
    {
        const int          mtype = me.mc;
        const monster_type mon   = monster_type(mtype);

        // Deal sensibly with duplicate entries; refuse or allow the
        // insert, depending on which should take precedence. Some
        // uniques of multiple forms can get away with this, though.
        if (mon == MONS_BAI_SUZHEN_DRAGON
            || mon != MONS_SERPENT_OF_HELL
               && mons_species(mon) == MONS_SERPENT_OF_HELL)
        {
            continue;
        }

        string name = me.name;
        lowercase(name);

        if (Mon_Name_Cache.count(name))
            die("Un-handled duplicate monster name: %s", name.c_str());

        Mon_Name_Cache[name] = mon;
    }
}

static const char *_mon_entry_name(size_t idx)
{
    return mondata[idx].name;
}

monster_type get_monster_by_name(string name, bool substring)
{
    if (name.empty())
        return MONS_PROGRAM_BUG;

    lowercase(name);

    if (!substring)
    {
        if (monster_type *mc = map_find(Mon_Name_Cache, name))
            return *mc;
        return MONS_PROGRAM_BUG;
    }

    size_t idx = find_earliest_match(name, (size_t) 0, ARRAYSZ(mondata),
                                     always_true<size_t>, _mon_entry_name);
    return idx == ARRAYSZ(mondata) ? MONS_PROGRAM_BUG
                                   : (monster_type) mondata[idx].mc;
}

void init_monsters()
{
    // First, fill static array with dummy values. {dlb}
    mon_entry.init(-1);

    // Next, fill static array with location of entry in mondata[]. {dlb}:
    for (unsigned int i = 0; i < MONDATASIZE; ++i)
        mon_entry[mondata[i].mc] = i;

    // Finally, monsters yet with dummy entries point to TTTSNB(tm). {dlb}:
    for (int &entry : mon_entry)
        if (entry == -1)
            entry = mon_entry[MONS_PROGRAM_BUG];

    init_monster_symbols();
}

void init_monster_symbols()
{
    map<unsigned, monster_type> base_mons;
    for (monster_type mc = MONS_0; mc < NUM_MONSTERS; ++mc)
    {
        mon_display &md = monster_symbols[mc];
        if (const monsterentry *me = get_monster_data(mc))
        {
            md.glyph  = me->basechar;
            md.colour = me->colour;
            auto it = base_mons.find(md.glyph);
            if (it == base_mons.end() || it->first == MONS_PROGRAM_BUG)
                base_mons[md.glyph] = mc;
        }
    }

    // Let those follow the feature settings, unless specifically overridden.
    monster_symbols[MONS_ANIMATED_TREE].glyph = get_feat_symbol(DNGN_TREE);

    // Validate all glyphs, even those which didn't come from an override.
    for (monster_type i = MONS_PROGRAM_BUG; i < NUM_MONSTERS; ++i)
        if (wcwidth(monster_symbols[i].glyph) != 1)
            monster_symbols[i].glyph = mons_base_char(i);
}

void set_resist(resists_t &all, mon_resist_flags res, int lev)
{
    if (res > MR_LAST_MULTI)
    {
        ASSERT_RANGE(lev, 0, 2);
        if (lev)
            all |= res;
        else
            all &= ~res;
        return;
    }

    ASSERT_RANGE(lev, -3, 5);
    all = (all & ~(res * 7)) | (res * (lev & 7));
}

int get_mons_class_ac(monster_type mc)
{
    const monsterentry *me = get_monster_data(mc);
    return me ? me->AC : get_monster_data(MONS_PROGRAM_BUG)->AC;
}

int get_mons_class_ev(monster_type mc)
{
    const monsterentry *me = get_monster_data(mc);
    return me ? me->ev : get_monster_data(MONS_PROGRAM_BUG)->ev;
}

static resists_t _apply_holiness_resists(resists_t resists, mon_holy_type mh)
{
    // Undead and non-living beings get full poison resistance.
    if (mh & (MH_UNDEAD | MH_NONLIVING))
        resists = (resists & ~(MR_RES_POISON * 7)) | (MR_RES_POISON * 3);

    // Everything but natural creatures and plants have full rNeg. Set here for
    // the benefit of the monster_info constructor. If you change this, also
    // change monster::res_negative_energy.
    if (!(mh & (MH_NATURAL | MH_PLANT)))
        resists = (resists & ~(MR_RES_NEG * 7)) | (MR_RES_NEG * 3);

    if (mh & (MH_UNDEAD | MH_DEMONIC | MH_PLANT | MH_NONLIVING))
        resists |= MR_RES_TORMENT;

    return resists;
}

/**
 * What special resistances does the given mutant beast facet provide?
 *
 * @param facet     The beast_facet in question, e.g. BF_FIRE.
 * @return          A bitfield of resists corresponding to the given facet;
 *                  e.g. MR_RES_FIRE for BF_FIRE.
 */
static resists_t _beast_facet_resists(beast_facet facet)
{
    static const map<beast_facet, resists_t> resists = {
        { BF_STING, MR_RES_POISON },
        { BF_FIRE,  MR_RES_FIRE },
        { BF_SHOCK, MR_RES_ELEC },
        { BF_OX,    MR_RES_COLD },
    };

    return lookup(resists, facet, 0);
}

resists_t get_mons_class_resists(monster_type mc)
{
    const monsterentry *me = get_monster_data(mc);
    const resists_t resists = me ? me->resists
                                 : get_monster_data(MONS_PROGRAM_BUG)->resists;
    // Don't apply fake holiness resists.
    if (mons_is_sensed(mc))
        return resists;

    // Assumes that, when a monster's holiness differs from other monsters
    // of the same type, that only adds resistances, never removes them.
    // Currently the only such case is MF_FAKE_UNDEAD.
    return _apply_holiness_resists(resists, mons_class_holiness(mc));
}

/// All resists intrinsic to a monster, excluding enchants, equip, etc.
resists_t get_mons_resists(const monster& m)
{
    const monster& mon = get_tentacle_head(m);

    resists_t resists = get_mons_class_resists(mon.type);

    if (mons_is_ghost_demon(mon.type))
        resists |= mon.ghost->resists;

    if (mons_genus(mon.type) == MONS_DRACONIAN && mon.type != MONS_DRACONIAN
        || mon.type == MONS_TIAMAT)
    {
        monster_type subspecies = draconian_subspecies(mon);
        if (subspecies != mon.type)
            resists |= get_mons_class_resists(subspecies);
    }

    if (mon.props.exists(MUTANT_BEAST_FACETS))
        for (auto facet : mon.props[MUTANT_BEAST_FACETS].get_vector())
            resists |= _beast_facet_resists((beast_facet)facet.get_int());

    // This is set from here in case they're undead due to the
    // MF_FAKE_UNDEAD flag. See the comment in get_mons_class_resists.
    return _apply_holiness_resists(resists, mon.holiness());
}

int get_mons_resist(const monster& mon, mon_resist_flags res)
{
    return get_resist(get_mons_resists(mon), res);
}

// Returns true if the monster successfully resists this attempt to poison it.
bool monster_resists_this_poison(const monster& mons, bool force)
{
    const int res = mons.res_poison();
    if (res >= 3)
        return true;
    if (!force && res >= 1 && x_chance_in_y(2, 3))
        return true;
    return false;
}

monster* monster_at(const coord_def &pos)
{
    if (!in_bounds(pos))
        return nullptr;

    const int mindex = env.mgrid(pos);
    if (mindex == NON_MONSTER)
        return nullptr;

    ASSERT(mindex <= MAX_MONSTERS);
    return &env.mons[mindex];
}

/// Are any of the bits set?
bool mons_class_flag(monster_type mc, monclass_flags_t bits)
{
    const monsterentry * const me = get_monster_data(mc);
    return me && (me->bitfields & bits);
}

int monster::wearing(object_class_type obj_type, int sub_type,
                     bool count_plus, bool) const
{
    int ret = 0;
    const item_def *item = 0;

    if (!alive())
        return 0;

    switch (obj_type)
    {
    case OBJ_WEAPONS:
    case OBJ_STAVES:
        {
            const mon_inv_type end = mons_wields_two_weapons(*this)
                                     ? MSLOT_ALT_WEAPON : MSLOT_WEAPON;

            for (int i = MSLOT_WEAPON; i <= end; i = i + 1)
            {
                item = mslot_item((mon_inv_type) i);
                if (item && item->base_type == obj_type
                    && item->sub_type == sub_type)
                {
                    ret++;
                }
            }
        }
        break;

    case OBJ_ARMOUR:

        item = mslot_item(MSLOT_SHIELD);
        if (item && item->is_type(OBJ_ARMOUR, sub_type))
            ret++;

        item = mslot_item(MSLOT_ARMOUR);
        if (item && item->is_type(OBJ_ARMOUR, sub_type))
            ret++;
        break;

    case OBJ_JEWELLERY:
        item = mslot_item(MSLOT_JEWELLERY);
        if (item && item->is_type(OBJ_JEWELLERY, sub_type))
        {
            if (count_plus)
                ret += item->plus;
            else
                ret++;
        }
        break;
    default:
        die("invalid object type %d for monster::wearing()", obj_type);
    }
    return ret;
}

int monster::wearing_ego(object_class_type obj_type, int special) const
{
    int ret = 0;
    const item_def *item = 0;

    if (!alive())
        return 0;

    switch (obj_type)
    {
    case OBJ_WEAPONS:
        {
            const mon_inv_type end = mons_wields_two_weapons(*this)
                                     ? MSLOT_ALT_WEAPON : MSLOT_WEAPON;

            for (int i = MSLOT_WEAPON; i <= end; i++)
            {
                item = mslot_item((mon_inv_type) i);
                if (item && item->base_type == OBJ_WEAPONS
                    && get_weapon_brand(*item) == special)
                {
                    ret++;
                }
            }
        }
        break;

    case OBJ_ARMOUR:
        item = mslot_item(MSLOT_SHIELD);
        if (item && item->base_type == OBJ_ARMOUR
            && get_armour_ego_type(*item) == special)
        {
            ret++;
        }

        item = mslot_item(MSLOT_ARMOUR);
        if (item && item->base_type == OBJ_ARMOUR
            && get_armour_ego_type(*item) == special)
        {
            ret++;
        }
        break;

    case OBJ_JEWELLERY:
    case OBJ_STAVES:
        // No egos.
        break;

    default:
        die("invalid object type %d for monster::wearing_ego()", obj_type);
    }
    return ret;
}

int monster::scan_artefacts(artefact_prop_type ra_prop,
                            vector<const item_def *> *matches) const
{
    UNUSED(matches); //TODO: implement this when it will be required somewhere

    if (!alive())
        return 0;

    int ret = 0;

    if (mons_itemuse(*this) >= MONUSE_STARTING_EQUIPMENT)
    {
        const int weap      = inv[MSLOT_WEAPON];
        const int second    = inv[MSLOT_ALT_WEAPON]; // Two-headed ogres, etc.
        const int armour    = inv[MSLOT_ARMOUR];
        const int shld      = inv[MSLOT_SHIELD];
        const int jewellery = inv[MSLOT_JEWELLERY];

        if (weap != NON_ITEM && env.item[weap].base_type == OBJ_WEAPONS
            && is_artefact(env.item[weap]))
        {
            ret += artefact_property(env.item[weap], ra_prop);
        }

        if (second != NON_ITEM && env.item[second].base_type == OBJ_WEAPONS
            && is_artefact(env.item[second]) && mons_wields_two_weapons(*this))
        {
            ret += artefact_property(env.item[second], ra_prop);
        }

        if (armour != NON_ITEM && env.item[armour].base_type == OBJ_ARMOUR
            && is_artefact(env.item[armour]))
        {
            ret += artefact_property(env.item[armour], ra_prop);
        }

        if (shld != NON_ITEM && env.item[shld].base_type == OBJ_ARMOUR
            && is_artefact(env.item[shld]))
        {
            ret += artefact_property(env.item[shld], ra_prop);
        }

        // XXX: Because monster armour slots are awkward, Wiglaf wears his hat
        //      in the jewelry slot. Since it is always an artefact, this should
        //      mostly work out fine, but I'd be happy for a better solution in
        //      future.
        if (jewellery != NON_ITEM && is_artefact(env.item[jewellery]))
            ret += artefact_property(env.item[jewellery], ra_prop);
    }

    return ret;
}

mon_holy_type holiness_by_name(string name)
{
    lowercase(name);
    for (const auto bit : mon_holy_type::range())
    {
        if (name == holiness_name(bit))
            return bit;
    }
    return MH_NONE;
}

const char * holiness_name(mon_holy_type_flags which_holiness)
{
    switch (which_holiness)
    {
    case MH_HOLY:
        return "holy";
    case MH_NATURAL:
        return "natural";
    case MH_UNDEAD:
        return "undead";
    case MH_DEMONIC:
        return "demonic";
    case MH_NONLIVING:
        return "nonliving";
    case MH_PLANT:
        return "plant";
    default:
        return "bug";
    }
}

/// Hack for demonspawn monsters. TODO: de-bitfieldify!
const char * single_holiness_description(mon_holy_type holiness)
{
    for (const auto bit : mon_holy_type::range())
    {
        if (!(holiness & bit))
            continue;
        if (bit == MH_NATURAL && holiness != MH_NATURAL)
            continue;
        return holiness_name(bit);
    }
    return "eggplant";
}

string holiness_description(mon_holy_type holiness)
{
    string description = "";
    for (const auto bit : mon_holy_type::range())
    {
        if (holiness & bit)
        {
            if (!description.empty())
                description += ",";
            description += holiness_name(bit);
        }
    }
    return description;
}

mon_holy_type mons_class_holiness(monster_type mc)
{
    ASSERT_smc();
    return smc->holiness;
}

const char* intelligence_description(mon_intel_type intel)
{
    switch (intel)
    {
    case I_BRAINLESS:   return "Mindless";
    case I_ANIMAL:      return "Animal";
    case I_HUMAN:       return "Human";
    default:            return "Eggplantelligent";
    }
}

bool mons_class_is_stationary(monster_type mc)
{
    return mons_class_flag(mc, M_STATIONARY);
}

bool mons_class_is_draconic(monster_type mc)
{
    switch (mons_genus(mc))
    {
        case MONS_DRAGON:
        case MONS_DRAKE:
        case MONS_DRACONIAN:
        case MONS_WYRMHOLE:
            return true;
        default:
            return false;
    }
}

/**
 * Can killing this class of monster ever reward xp?
 *
 * This answers whether any agent could receive XP for killing a monster of
 * this class. Monsters that fail this have M_NO_EXP_GAIN set.
 * @param mc       The monster type
 * @param indirect If true this will count monsters that are parts of a parent
 *                 monster as xp rewarding even if the parts themselves don't
 *                 reward xp (e.g. tentacles).
 * @returns True if killing a monster of this class could reward xp, false
 * otherwise.
 */
bool mons_class_gives_xp(monster_type mc, bool indirect)
{
    return !mons_class_flag(mc, M_NO_EXP_GAIN)
        || (indirect && mons_is_tentacle_or_tentacle_segment(mc));
}

/**
 * Can killing this monster reward xp to the given actor?
 *
 * This answers whether the player or a monster could ever receive XP for
 * killing the monster, assuming an appropriate kill_type.
 * @param mon      The monster.
 * @param agent    The actor who would be responsible for the kill.
 * @returns True if killing the monster will reward the agent with xp, false
 * otherwise.
 */
bool mons_gives_xp(const monster& victim, const actor& agent)
{
    const bool mon_killed_friend
        = agent.is_monster() && mons_aligned(&victim, &agent);
    return !victim.is_summoned()                        // no summons
            && mons_class_gives_xp(victim.type)         // class must reward xp
            && (!testbits(victim.flags, MF_WAS_NEUTRAL) // no neutral monsters
                || victim.has_ench(ENCH_MAD))           // ...except frenzied ones
            && !testbits(victim.flags, MF_NO_REWARD)    // no reward for no_reward
            && !mon_killed_friend;
}

bool mons_class_is_threatening(monster_type mo)
{
    return !mons_class_flag(mo, M_NO_THREAT);
}

bool mons_is_threatening(const monster& mons)
{
    return mons_class_is_threatening(mons.type) || mons_is_active_ballisto(mons);
}

bool mons_class_is_fragile(monster_type mc)
{
    return mons_class_flag(mc, M_FRAGILE);
}

bool mons_is_fragile(const monster& mons)
{
    return mons_class_is_fragile(mons.type);
}

/**
 * Is this an active ballistomycete?
 *
 * @param mon             The monster
 * @returns True if the monster is an active ballistomycete, false otherwise.
 */
bool mons_is_active_ballisto(const monster& mon)
{
    return mon.type == MONS_BALLISTOMYCETE && mon.ballisto_activity;
}

/**
 * Is this monster class firewood?
 *
 * Firewood monsters are harmless stationary monsters than don't give xp. These
 * are worthless obstacles: not to be attacked by default, but may be cut down
 * to get to target even if coaligned.
 * @param mc The monster type
 * @returns True if the monster class is firewood, false otherwise.
 */
bool mons_class_is_firewood(monster_type mc)
{
    return mons_class_is_stationary(mc)
           && !mons_class_is_test(mc)
           && mons_class_flag(mc, M_NO_THREAT)
           && !mons_is_tentacle_or_tentacle_segment(mc);
}

/**
 * Is this a test monster? Mainly used for exempting these from various checks
 * like firewood, etc.
 */
bool mons_class_is_test(monster_type mc)
{
    return mc == MONS_TEST_SPAWNER || mc == MONS_TEST_STATUE
        || mc == MONS_TEST_BLOB;
}

// "body" in a purely grammatical sense.
bool mons_has_body(const monster& mon)
{
    if (mon.type == MONS_WEEPING_SKULL
        || mon.type == MONS_LAUGHING_SKULL
        || mons_species(mon.type) == MONS_CURSE_SKULL // including Murray
        || mon.type == MONS_CURSE_TOE
        || mon.type == MONS_DEATH_COB
        || mon.type == MONS_ARMOUR_ECHO
        || mons_class_is_animated_weapon(mon.type)
        || mons_is_tentacle_or_tentacle_segment(mon.type))
    {
        return false;
    }

    switch (mons_base_char(mon.type))
    {
    case 'P':
    case 'v':
    case 'G':
    case '*':
    case 'J':
        return false;
    }

    return true;
}

// Difference in speed between monster and the player for Cheibriados'
// purposes. This is the speed difference disregarding the player's
// slow status.
int cheibriados_monster_player_speed_delta(const monster& mon)
{
    // Ignore the Slow effect.
    unwind_var<int> ignore_slow(you.duration[DUR_SLOW], 0);
    const int pspeed = 1000 / (player_movement_speed() * player_speed());
    dprf("Your delay: %d, your speed: %d, mon speed: %d",
        player_movement_speed(), pspeed, mon.speed);
    return mon.speed - pspeed;
}

bool cheibriados_thinks_mons_is_fast(const monster& mon)
{
    return mons_base_speed(mon) >= 10;
}

bool mons_is_projectile(monster_type mc)
{
    return mons_class_flag(mc, M_PROJECTILE);
}

bool mons_is_projectile(const monster& mon)
{
    return mons_is_projectile(mon.type);
}

bool mons_has_blood(monster_type mc)
{
    return mons_class_flag(mc, M_COLD_BLOOD)
           || mons_class_flag(mc, M_WARM_BLOOD);
}

bool mons_is_sensed(monster_type mc)
{
    return mc == MONS_SENSED
           || mc == MONS_SENSED_FRIENDLY
           || mc == MONS_SENSED_TRIVIAL
           || mc == MONS_SENSED_EASY
           || mc == MONS_SENSED_TOUGH
           || mc == MONS_SENSED_NASTY;
}

bool mons_offers_beogh_conversion(const monster& mon)
{
    return mons_genus(mon.type) == MONS_ORC
           && mon.is_priest() && mon.god == GOD_BEOGH;
}

bool mons_offers_beogh_conversion_now(const monster& mon)
{
    // Do the expensive LOS check last.
    return mons_offers_beogh_conversion(mon)
                // Only try to convert atheists
                && you.religion == GOD_NO_GOD
                && !you.has_mutation(MUT_FORLORN)
                && you.hp * 3 / 2 <= you.hp_max
                && !mon.is_summoned() && !mon.friendly()
                && !silenced(mon.pos()) && !mon.has_ench(ENCH_MUTE)
                && !mons_is_confused(mon) && mons_is_seeking(mon)
                && mon.foe == MHITYOU && !mons_is_immotile(mon)
                && you.visible_to(&mon) && you.can_see(mon);
}

// Returns true for monsters that obviously (to the player) feel
// "thematically at home" in a branch. Currently used for native
// monsters recognising traps and patrolling branch entrances.
bool mons_is_native_in_branch(const monster& mons,
                              const branch_type branch)
{
    switch (branch)
    {
    case BRANCH_ELF:
        return mons_genus(mons.type) == MONS_ELF;

    case BRANCH_ORC:
        return mons_genus(mons.type) == MONS_ORC;

    case BRANCH_SHOALS:
        return mons_species(mons.type) == MONS_CYCLOPS
               || mons_species(mons.type) == MONS_MERFOLK
               || mons.type == MONS_HARPY;

    case BRANCH_SLIME:
        return mons_is_slime(mons);

    case BRANCH_SNAKE:
        return mons_genus(mons.type) == MONS_NAGA
               || mons_genus(mons.type) == MONS_SALAMANDER
               || mons_genus(mons.type) == MONS_SNAKE;

    case BRANCH_ZOT:
        return mons_genus(mons.type) == MONS_DRACONIAN
               || mons.type == MONS_ORB_GUARDIAN
               || mons.type == MONS_ORB_OF_FIRE
               || mons.type == MONS_DEATH_COB
               || mons.type == MONS_KILLER_KLOWN;

    case BRANCH_VAULTS:
        return mons_genus(mons.type) == MONS_HUMAN;

    case BRANCH_CRYPT:
        return mons.holiness() == MH_UNDEAD;

    case BRANCH_TOMB:
        return mons_genus(mons.type) == MONS_MUMMY
              || mons.type == MONS_USHABTI
              || mons.type == MONS_DEATH_SCARAB;

    case BRANCH_SPIDER:
        return mons_genus(mons.type) == MONS_SPIDER;

    case BRANCH_ABYSS:
        return mons_is_abyssal_only(mons.type)
               || mons.type == MONS_ABOMINATION_LARGE
               || mons.type == MONS_ABOMINATION_SMALL
               || mons.type == MONS_TENTACLED_MONSTROSITY
               || mons.type == MONS_TENTACLED_STARSPAWN
               || mons.type == MONS_THRASHING_HORROR
               || mons.type == MONS_UNSEEN_HORROR
               || mons.type == MONS_WORLDBINDER;

    default:
        return false;
    }
}

bool mons_is_abyssal_only(monster_type mc)
{
    switch (mc)
    {
    case MONS_ANCIENT_ZYME:
    case MONS_ELDRITCH_TENTACLE:
    case MONS_ELDRITCH_TENTACLE_SEGMENT:
    case MONS_LURKING_HORROR:
    case MONS_WRETCHED_STAR:
        return true;
    default:
        return false;
    }
}

// Monsters considered as "slime" for Jiyva.
bool mons_class_is_slime(monster_type mc)
{
    return mons_genus(mc) == MONS_JELLY
           || mons_genus(mc) == MONS_FLOATING_EYE
           || mons_genus(mc) == MONS_GLOWING_ORANGE_BRAIN;
}

bool mons_is_slime(const monster& mon)
{
    return mons_class_is_slime(mon.type);
}

bool herd_monster(const monster& mon)
{
    return mons_class_flag(mon.type, M_HERD);
}

// Plant or fungus or really anything with
// permanent plant holiness
bool mons_class_is_plant(monster_type mc)
{
    return bool(mons_class_holiness(mc) & MH_PLANT);
}

bool mons_is_plant(const monster& mon)
{
    return mons_class_is_plant(mon.type);
}

bool mons_eats_items(const monster& mon)
{
    return mons_is_slime(mon) && have_passive(passive_t::jelly_eating);
}

/* Is the actor susceptible to vampirism?
 *
 * Undead actors and summoned, temporary, or ghostified monsters are all not
 * susceptible.
 * @param act The actor.
 * @param only_known Only include information known to the player.
 * @returns True if the actor is susceptible to vampirism, false otherwise.
 */
bool actor_is_susceptible_to_vampirism(const actor& act, bool only_known)
{
    if (!(act.holiness() & (MH_NATURAL | MH_PLANT)))
        return false;

    if (act.is_player())
        return true;

    const monster *mon = act.as_monster();
    // Don't leak phantom mirror info.
    if (act.is_summoned() && (!only_known
                              || !mon->has_ench(ENCH_PHANTOM_MIRROR)
                              || mon->friendly()))
    {
        return false;
    }

    // Don't allow HP draining from firewood.
    return !mon->is_firewood();
}

bool invalid_monster(const monster* mon)
{
    return !mon || invalid_monster_type(mon->type);
}

bool invalid_monster_type(monster_type mt)
{
    return mt < 0 || mt >= NUM_MONSTERS
           || mon_entry[mt] == mon_entry[MONS_PROGRAM_BUG];
}

bool invalid_monster_index(int i)
{
    return i < 0 || i >= MAX_MONSTERS;
}

bool mons_is_statue(monster_type mc)
{
    return mc == MONS_ORANGE_STATUE
           || mc == MONS_OBSIDIAN_STATUE
           || mc == MONS_ICE_STATUE
           || mc == MONS_ROXANNE;
}

/**
 * The mimic [(cackles|chortles...) and ]vanishes in[ a puff of smoke]!
 *
 * @param pos       The mimic's location.
 * @param name      The mimic's name.
 */
static void _mimic_vanish(const coord_def& pos, const string& name)
{
    const bool can_place_smoke = !cloud_at(pos);
    if (can_place_smoke)
        place_cloud(CLOUD_BLACK_SMOKE, pos, 2 + random2(2), nullptr);
    if (!you.see_cell(pos))
        return;

    const char* const smoke_str = can_place_smoke ? " in a puff of smoke" : "";

    const bool can_cackle = !silenced(pos) && !silenced(you.pos());
    const string cackle = can_cackle ? getSpeakString("_laughs_") + " and " : "";

    mprf("The %s mimic %svanishes%s!",
         name.c_str(), cackle.c_str(), smoke_str);
    interrupt_activity(activity_interrupt::mimic);
}

/**
 * Clean up a "feature" that a mimic was pretending to be.
 *
 * @param pos   The location of the 'feature'.
 */
static void _destroy_mimic_feature(const coord_def &pos)
{
#if TAG_MAJOR_VERSION == 34
    const dungeon_feature_type feat = env.grid(pos);
#endif

    unnotice_feature(level_pos(level_id::current(), pos));
    env.grid(pos) = DNGN_FLOOR;
    env.level_map_mask(pos) &= ~MMT_MIMIC;
    set_terrain_changed(pos);
    remove_markers_and_listeners_at(pos);

#if TAG_MAJOR_VERSION == 34
    if (feat_is_door(feat))
        env.level_map_mask(pos) |= MMT_WAS_DOOR_MIMIC;
#endif
}

void discover_mimic(const coord_def& pos)
{
    item_def* item = item_mimic_at(pos);
    const bool feature_mimic = !item && feature_mimic_at(pos);
    // Is there really a mimic here?
    if (!item && !feature_mimic)
        return;

    const dungeon_feature_type feat = env.grid(pos);

    // If the feature has been destroyed, don't create a floor mimic.
    if (feature_mimic && !feat_is_mimicable(feat, false))
    {
        env.level_map_mask(pos) &= ~MMT_MIMIC;
        return;
    }

    const string name = feature_mimic ? "the " + string(feat_type_name(feat))
                                      : item->name(DESC_THE, false, false,
                                                             false, true);
    const bool plural = feature_mimic ? false : item->quantity > 1;

#ifdef USE_TILE
    tileidx_t tile = tileidx_feature(pos);
    apply_variations(tile_env.flv(pos), &tile, pos);
#endif

    if (you.see_cell(pos))
        mprf("%s %s a mimic!", name.c_str(), plural ? "are" : "is");

    const string shortname = feature_mimic ? feat_type_name(feat)
                                           : item->name(DESC_BASENAME);
    if (item)
        destroy_item(item->index(), true);
    else
        _destroy_mimic_feature(pos);
    _mimic_vanish(pos, shortname);

    // Just in case there's another one.
    if (mimic_at(pos))
        discover_mimic(pos);
}

void discover_shifter(monster& shifter)
{
    shifter.flags |= MF_KNOWN_SHIFTER;
}

bool mons_is_demon(monster_type mc)
{
    return mons_class_holiness(mc) & MH_DEMONIC
             && (mons_demon_tier(mc) != 0 && mc != MONS_ANTAEUS
                 || mons_species(mc) == MONS_RAKSHASA);
}

int mons_demon_tier(monster_type mc)
{
    switch (mons_base_char(mc))
    {
    case 'C':
        if (mc != MONS_ANTAEUS)
            return 0;
        // intentional fall-through for Antaeus
    case '&':
        return -1;
    case '1':
        return 1;
    case '2':
        return 2;
    case '3':
        return 3;
    case '4':
        return 4;
    case '5':
        return 5;
    default:
        return 0;
    }
}

// Beware; returns false for Tiamat!
bool mons_is_draconian(monster_type mc)
{
    return mc >= MONS_FIRST_DRACONIAN && mc <= MONS_LAST_DRACONIAN;
}

bool mons_is_base_draconian(monster_type mc)
{
    return mc >= MONS_FIRST_DRACONIAN && mc <= MONS_LAST_BASE_DRACONIAN;
}

bool mons_class_is_peripheral(monster_type mc)
{
    return mons_is_projectile(mc)
           || mons_is_avatar(mc)
           || mons_class_flag(mc, M_PERIPHERAL)
           || mons_class_is_firewood(mc);
}

size_type mons_class_body_size(monster_type mc)
{
    // Should pass base_type to get the right size for zombies, skeletons &c.
    // For normal monsters, base_type is set to type in the constructor.
    const monsterentry *e = get_monster_data(mc);
    return e ? e->size : SIZE_MEDIUM;
}

int max_corpse_chunks(monster_type mc)
{
    switch (mons_class_body_size(mc))
    {
    case SIZE_TINY:
        return 1;
    case SIZE_LITTLE:
        return 2;
    case SIZE_SMALL:
        return 3;
    case SIZE_MEDIUM:
        return 4;
    case SIZE_LARGE:
        return 9;
    case SIZE_GIANT:
        return 12;
    default:
        return 0;
    }
}

int derived_undead_avg_hp(monster_type mtype, int hd, int scale)
{
    static const map<monster_type, int> hp_per_hd_by_type = {
        { MONS_BOUND_SOUL,     100 },
        { MONS_ZOMBIE,          85 },
        { MONS_SKELETON,        70 },
        { MONS_SPECTRAL_THING,  60 },
        // Simulacra aren't tough, but you can create piles of them. - bwr
        { MONS_SIMULACRUM,      30 },
    };

    const int* hp_per_hd = map_find(hp_per_hd_by_type, mtype);
    ASSERT(hp_per_hd);
    return *hp_per_hd * hd * scale / 10;
}

monster_type mons_genus(monster_type mc)
{
    if (mc == RANDOM_DRACONIAN || mc == RANDOM_BASE_DRACONIAN
        || mc == RANDOM_NONBASE_DRACONIAN
        || (mc == MONS_PLAYER_ILLUSION && species::is_draconian(you.species)))
    {
        return MONS_DRACONIAN;
    }

    if (mc == MONS_NO_MONSTER)
        return MONS_NO_MONSTER;

    ASSERT_smc();
    return smc->genus;
}

monster_type mons_species(monster_type mc)
{
    const monsterentry *me = get_monster_data(mc);
    return me ? me->species : MONS_PROGRAM_BUG;
}

monster_type draconian_subspecies(monster_type type, monster_type base)
{
    const monster_type species = mons_species(type);

    if (species == MONS_DRACONIAN && type != species)
        return base;

    return species;
}

monster_type draconian_subspecies(const monster& mon)
{
    ASSERT(mons_genus(mon.type) == MONS_DRACONIAN);

    if (mon.type == MONS_PLAYER_ILLUSION
        && mons_genus(mon.type) == MONS_DRACONIAN)
    {
        return species::to_mons_species(mon.ghost->species);
    }

    return draconian_subspecies(mon.type, mon.base_monster);
}

monster_type mons_detected_base(monster_type mc)
{
    return mons_genus(mc);
}

/** Does a monster of this type behold opponents like a siren?
 */
bool mons_is_siren_beholder(monster_type mc)
{
    return mc == MONS_MERFOLK_SIREN || mc == MONS_MERFOLK_AVATAR;
}

/** Does this monster behold opponents like a siren?
 *  Ignores any disabilities, checking only the monster's type.
 */
bool mons_is_siren_beholder(const monster& mons)
{
    return mons_is_siren_beholder(mons.type);
}

int get_shout_noise_level(const shout_type shout)
{
    switch (shout)
    {
    case S_SILENT:
        return 0;
    case S_HISS:
    case S_SKITTER:
    case S_FAINT_SKITTER:
    case S_VERY_SOFT:
        return 4;
    case S_SOFT:
        return 6;
    case S_LOUD:
        return 10;
    case S_LOUD_ROAR:
    case S_VERY_LOUD:
        return 12;

    default:
        return 8;
    }
}

// Only beasts and chaos spawns use S_RANDOM for noise type.
// Pandemonium lords can also get here, but this is mostly used for the
// "says" verb used for insults.
static bool _shout_fits_monster(monster_type mc, int shout)
{
    if (shout == NUM_SHOUTS || shout >= NUM_LOUDNESS || shout == S_SILENT)
        return false;

    // Chaos spawns can do anything but demon taunts, since they're
    // not coherent enough to actually say words.
    if (mc == MONS_CHAOS_SPAWN)
        return shout != S_DEMON_TAUNT;

    // For Pandemonium lords, almost everything is fair game. It's only
    // used for the shouting verb ("say", "bellow", "roar", etc.) anyway.
    if (mc != MONS_SIN_BEAST && mc != MONS_MUTANT_BEAST)
        return true;

    switch (shout)
    {
    // Bees, books, cherubs and two-headed ogres never fit.
    case S_SHOUT2:
    case S_BUZZ:
    case S_CHERUB:
    case S_RUSTLE:
    // The beast cannot speak.
    case S_DEMON_TAUNT:
        return false;
    default:
        return true;
    }
}

// If demon_shout is true, we're trying to find a random verb and
// loudness for a Pandemonium lord trying to shout.
shout_type mons_shouts(monster_type mc, bool demon_shout)
{
    ASSERT_smc();
    shout_type u = smc->shouts;

    // Pandemonium lords use this to get the noises.
    if (u == S_RANDOM || demon_shout && u == S_DEMON_TAUNT)
    {
        const int max_shout = (u == S_RANDOM ? NUM_SHOUTS : NUM_LOUDNESS);
        do
        {
            u = static_cast<shout_type>(random2(max_shout));
        }
        while (!_shout_fits_monster(mc, u));
    }

    return u;
}

/// Is the given monster type ever capable of shouting?
bool mons_can_shout(monster_type mc)
{
    // don't use mons_shouts() to avoid S_RANDOM randomization.
    ASSERT_smc();
    return smc->shouts != S_SILENT;
}

bool mons_is_ghost_demon(monster_type mc)
{
    return mons_class_flag(mc, M_GHOST_DEMON);
}

bool mons_is_pghost(monster_type mc)
{
    return mc == MONS_PLAYER_GHOST || mc == MONS_PLAYER_ILLUSION;
}

/**
 * What mutant beast tier does the given XL (base HD) correspond to?
 *
 * If the given value is between tiers, choose the higher possibility.
 *
 * @param xl    The XL (HD) of the mutant beast in question.
 * @return      The corresponding mutant beast tier (e.g. BT_MATURE).
 */
int mutant_beast_tier(int xl)
{
    for (int bt = BT_FIRST; bt < NUM_BEAST_TIERS; ++bt)
        if (xl <= beast_tiers[bt])
            return bt;
    return BT_NONE; // buggy
}

/**
 * Is the provided monster_type a draconian job type? (Not just any draconian,
 * but specifically one with a job! Or the job itself, depending how you think
 * about it.)
 *
 * @param mc    The monster type in question.
 * @return      Whether that monster type is a draconian job.
 **/
bool mons_is_draconian_job(monster_type mc)
{
    return mc >= MONS_FIRST_NONBASE_DRACONIAN
           && mc <= MONS_LAST_NONBASE_DRACONIAN;
}

bool mons_is_unique(monster_type mc)
{
    return mons_class_flag(mc, M_UNIQUE);
}

bool mons_is_or_was_unique(const monster& mon)
{
    return mons_is_unique(mon.type)
           || mon.props.exists(ORIGINAL_TYPE_KEY)
              && mons_is_unique((monster_type) mon.props[ORIGINAL_TYPE_KEY].get_int());
}

/// This monster isn't a unique per se, but it gets a name anyway.
/// E.g., the Hellbinder.
bool mons_is_specially_named(monster_type mc)
{
    return mons_class_flag(mc, M_ALWAYS_NAMED);
}

/**
 * Is the given type one of Hepliaklqana's granted ancestors?
 *
 * @param mc    The type of monster in question.
 * @return      Whether that monster is a player ancestor.
 */
bool mons_is_hepliaklqana_ancestor(monster_type mc)
{
    return mons_class_flag(mc, M_ANCESTOR);
}

/**
 * Can this type of monster be blinded?
 *
 * Certain monsters, e.g. those with a powerful sense of smell, echolocation,
 * or no eyes, are completely immune to blinding.
 *
 * Note that 'dazzling' (from dazzling spray) has additional restrictions above
 * this.
 *
 * @param mc    The class of monster in question.
 * @return      Whether monsters of this type can get ENCH_BLIND.
 */
bool mons_can_be_blinded(monster_type mc)
{
    return !mons_class_flag(mc, M_UNBLINDABLE);
}

/**
 * Can this kind of monster be dazzled?
 *
 * The undead, nonliving, vegetative, or unblindable cannot be dazzled.
 *
 * @param mc    The class of monster in question.
 * @return      Whether monsters of this type can get `ENCH_BLIND` from Dazzling
 *              Spray or wands of light.
 */
bool mons_can_be_dazzled(monster_type mc)
{
    // This was implemented by checking type so that we could use it in
    // monster descriptions (which only have mon_info structs); not sure if
    // that's useful

    const mon_holy_type holiness = mons_class_holiness(mc);
    return !(holiness & (MH_UNDEAD | MH_NONLIVING | MH_PLANT))
        && mons_can_be_blinded(mc);
}

/**
 * Can this type of monster survive in deep water?
 *
 * @param type  The monster type in question.
 * @param base  The base type of the monster. (For e.g. draconians.)
 * @return      Whether monsters of this type can survive falling into deep
 *              water.
 *
 * XXX: Duplicates monster::res_water_drowning().
 */
bool mons_resists_drowning(monster_type type, monster_type base)
{
    const habitat_type ht = mons_habitat_type(type, base, true);

    return mons_is_unbreathing(type) || ht == HT_WATER || ht == HT_AMPHIBIOUS;
}

char32_t mons_char(monster_type mc)
{
    if (Options.mon_glyph_overrides.count(mc)
        && Options.mon_glyph_overrides[mc].ch)
    {
        return Options.mon_glyph_overrides[mc].ch;
    }
    else
        return monster_symbols[mc].glyph;
}

char mons_base_char(monster_type mc)
{
    const monsterentry *me = get_monster_data(mc);
    return me ? me->basechar : 0;
}

mon_itemuse_type mons_class_itemuse(monster_type mc)
{
    ASSERT_smc();
    return smc->gmon_use;
}

mon_itemuse_type mons_itemuse(const monster& mon)
{
    if (mon.type == MONS_BOUND_SOUL)
        return mons_class_itemuse(mons_zombie_base(mon));

    return mons_class_itemuse(mon.type);
}

int mons_class_colour(monster_type mc)
{
    ASSERT_smc();
    // Player monster is a dummy monster used only for display purposes, so
    // it's ok to override it here.
    if (mc == MONS_PLAYER
        && Options.mon_glyph_overrides.count(MONS_PLAYER)
        && Options.mon_glyph_overrides[MONS_PLAYER].col)
    {
        return Options.mon_glyph_overrides[MONS_PLAYER].col;
    }
    else
        return monster_symbols[mc].colour;
}

bool mons_class_can_regenerate(monster_type mc)
{
    return !mons_class_flag(mc, M_NO_REGEN);
}

bool mons_can_regenerate(const monster& m)
{
    const monster& mon = get_tentacle_head(m);

    if (testbits(mon.flags, MF_NO_REGEN))
        return false;

    return mons_class_can_regenerate(mon.type);
}

bool mons_class_fast_regen(monster_type mc)
{
    return mons_class_flag(mc, M_FAST_REGEN);
}

int mons_class_regen_amount(monster_type mc)
{
    switch (mc)
    {
    case MONS_PARGHIT:            return 27;
    case MONS_DEMONIC_CRAWLER:
    case MONS_PROTEAN_PROGENITOR:
    case MONS_ASPIRING_FLESH:
    case MONS_MARTYRED_SHADE:     return 6;
    default:                      return 1;
    }
}

/**
 * Do monsters of the given type ever leave a hide?
 *
 * @param mc      The class of monster in question.
 * @return        Whether the monster has a chance of dropping a hide when
 *                butchered.
 */
bool mons_class_leaves_hide(monster_type mc)
{
    return hide_for_monster(mc) != NUM_ARMOURS;
}

bool mons_class_leaves_wand(monster_type mc)
{
    return mc == MONS_ELEIONOMA || mc == MONS_FENSTRIDER_WITCH;
}

bool mons_class_leaves_organ(monster_type mc)
{
    return mons_class_leaves_hide(mc) || mons_class_leaves_wand(mc);
}

int mons_zombie_size(monster_type mc)
{
    mc = mons_species(mc);
    if (!mons_class_can_be_zombified(mc))
        return Z_NOZOMBIE;

    ASSERT_smc();
    return smc->size > SIZE_MEDIUM ? Z_BIG : Z_SMALL;
}

monster_type mons_zombie_base(const monster& mon)
{
    return mon.base_monster;
}

bool mons_class_is_zombified(monster_type mc)
{
#if TAG_MAJOR_VERSION == 34
    switch (mc)
    {
        case MONS_ZOMBIE_SMALL:     case MONS_ZOMBIE_LARGE:
        case MONS_SKELETON_SMALL:   case MONS_SKELETON_LARGE:
        case MONS_SIMULACRUM_SMALL: case MONS_SIMULACRUM_LARGE:
            return true;
        default:
            break;
    }
#endif

    return mc == MONS_ZOMBIE
        || mc == MONS_SKELETON
        || mc == MONS_SIMULACRUM
        || mc == MONS_SPECTRAL_THING
        || mc == MONS_BOUND_SOUL;
}

bool mons_class_is_animated_weapon(monster_type type)
{
    return type == MONS_DANCING_WEAPON || type == MONS_SPECTRAL_WEAPON;
}

bool mons_class_is_remnant(monster_type mc)
{
    return mons_class_flag(mc, M_REMNANT);
}

bool mons_class_is_animated_object(monster_type type)
{
    return mons_class_is_animated_weapon(type)
        || type == MONS_ARMOUR_ECHO
        || type == MONS_HAUNTED_ARMOUR;
}

bool mons_is_zombified(const monster& mon)
{
    return mons_class_is_zombified(mon.type);
}

monster_type mons_base_type(const monster& mon)
{
    if (mons_class_is_zombified(mon.type))
        return mons_zombie_base(mon);
    return mon.type;
}

bool mons_class_can_leave_corpse(monster_type mc)
{
    ASSERT_smc();
    return smc->leaves_corpse;
}

bool mons_class_can_be_zombified(monster_type mzc)
{
    monster_type mc = mons_species(mzc);
    ASSERT_smc();
    return !invalid_monster_type(mc)
            && !mons_class_flag(mzc, M_NO_ZOMBIE)
            && !mons_class_flag(mzc, M_INSUBSTANTIAL)
            && !mons_is_tentacle_or_tentacle_segment(mzc)
            && (mons_class_holiness(mzc) & MH_NATURAL
                || mons_class_can_leave_corpse(mc))
            && smc->attack[0].damage; // i.e. has_attack
}

bool mons_can_be_zombified(const monster& mon)
{
    return mons_class_can_be_zombified(mon.type)
           && !mon.is_summoned()
           && !mon.has_ench(ENCH_SOUL_RIPE)
           && mons_has_attacks(mon, true);
}

bool mons_class_can_be_spectralised(monster_type mzc, bool divine)
{
    monster_type mc = mons_species(mzc);
    ASSERT_smc();
    return mons_class_holiness(mzc) & (MH_NATURAL | MH_DEMONIC | MH_HOLY)
        && mc != MONS_PANDEMONIUM_LORD
        && mzc != MONS_ORC_APOSTLE
        && (divine || smc->attack[0].type != AT_NONE); // i.e. has_attack
}

// Does this monster have a soul that can be used for necromancy (Death
// Channel, Simulacrum, Yredelemnul's Bind Soul)? For Bind Soul, allow
// monsters with no attacks if they have some spells to use.
// When called from Bind Soul's targeting interface, take into account only
// known effects.
bool mons_can_be_spectralised(const monster& mon, bool divine, bool only_known)
{
    return mons_class_can_be_spectralised(mon.type, divine)
           && (!mon.is_summoned()
               || only_known && mon.has_ench(ENCH_PHANTOM_MIRROR)
                    && mon.mons_species() != MONS_PLAYER_ILLUSION)
           && !mons_is_tentacle_or_tentacle_segment(mon.type)
           && (!testbits(mon.flags, MF_NO_REWARD)
               || mon.props.exists(KIKU_WRETCH_KEY))
           && (mons_has_attacks(mon, true)
               || divine && mon.has_spells());
}

bool mons_class_can_use_stairs(monster_type mc)
{
    return (!mons_class_is_zombified(mc) || mc == MONS_BOUND_SOUL)
           && !mons_is_tentacle_or_tentacle_segment(mc)
           && mc != MONS_SILENT_SPECTRE
           && mc != MONS_GERYON
           && mc != MONS_ROYAL_JELLY
           && mc != MONS_BALL_LIGHTNING
           && mc != MONS_FOXFIRE;
}

bool mons_class_can_use_transporter(monster_type mc)
{
    return !mons_is_tentacle_or_tentacle_segment(mc);
}

bool mons_can_use_stairs(const monster& mon, dungeon_feature_type stair)
{
    if (!mons_class_can_use_stairs(mon.type))
        return false;

    // Summons can't use stairs. (And neither can animated zombies)
    if (mon.is_summoned())
        return false;

    if (mon.has_ench(ENCH_FRIENDLY_BRIBED)
        && (feat_is_branch_entrance(stair) || feat_is_branch_exit(stair)))
    {
        return false;
    }

    // Don't let the pieces of Blorkula individually follow the player between floors
    if (mon.type == MONS_VAMPIRE_BAT
        && mon.props.exists(BLORKULA_REVIVAL_TIMER_KEY))
    {
        return false;
    }

    // If this is the entrance to a portal vault (or another region of Pandemonium)
    // only friendly monsters can traverse this.
    if (!mon.friendly()
        && (feat_is_portal_entrance(stair) || stair == DNGN_TRANSIT_PANDEMONIUM
                                           || stair == DNGN_ENTER_PANDEMONIUM))
    {
        return false;
    }

    // Everything else is fine
    return true;
}

void name_zombie(monster& mon, monster_type mc, const string &mon_name)
{
    mon.mname = mon_name;

    // For the Lernaean hydra: treat Lernaean as an adjective to
    // avoid mentions of "the Lernaean hydra the X-headed hydra zombie".
    if (mc == MONS_LERNAEAN_HYDRA)
    {
        mon.mname = "Lernaean";
        mon.flags |= MF_NAME_ADJECTIVE;
    }
    // Also for the Enchantress: treat Enchantress as an adjective to
    // avoid mentions of "the Enchantress the spriggan zombie".
    else if (mc == MONS_ENCHANTRESS)
    {
        mon.mname = "Enchantress";
        mon.flags |= MF_NAME_ADJECTIVE;
    }
    else if (mons_species(mc) == MONS_SERPENT_OF_HELL)
        mon.mname = "";

    if (starts_with(mon.mname, "shaped "))
        mon.flags |= MF_NAME_SUFFIX;

    // It's unlikely there's a desc for "Duvessa the elf skeleton", but
    // we still want to allow it if overridden.
    if (!mon.props.exists(DBNAME_KEY))
        mon.props[DBNAME_KEY] = mons_class_name(mon.type);
}

void name_zombie(monster& mon, const monster& orig)
{
    if (!mons_is_unique(orig.type) && orig.mname.empty())
        return;

    string name;

    if (!orig.mname.empty())
        name = orig.mname;
    else
        name = mons_type_name(orig.type, DESC_PLAIN);

    name_zombie(mon, orig.type, name);
    mon.flags |= orig.flags & (MF_NAME_SUFFIX
                                 | MF_NAME_ADJECTIVE
                                 | MF_NAME_DESCRIPTOR);
}

// Derived undead deal 80% of the damage of the base form.
static int _downscale_zombie_damage(int damage)
{
    return max(1, 4 * damage / 5);
}

// Do not include AF_PLAIN, we want that to be overwritten for spectrals
// and simulacra
static const set<attack_flavour> allowed_zombie_af = {
    AF_REACH,
    AF_CRUSH,
    AF_TRAMPLE,
    AF_DRAG,
};

static mon_attack_def _downscale_zombie_attack(const monster& mons,
                                               mon_attack_def attk)
{
    switch (attk.type)
    {
    case AT_STING: case AT_SPORE:
    case AT_TOUCH: case AT_ENGULF:
        attk.type = AT_HIT;
        break;
    default:
        break;
    }

    attk.damage = _downscale_zombie_damage(attk.damage);
    if (allowed_zombie_af.count(attk.flavour))
        return attk;

    // overwrite all other AFs
    if (mons.type == MONS_SIMULACRUM)
        attk.flavour = AF_COLD;
    else if (mons.mons_species() == MONS_SPECTRAL_THING)
        attk.flavour = AF_DRAIN;
    else
        attk.flavour = AF_PLAIN;

    return attk;
}

/**
 * What attack does the given mutant beast facet provide?
 *
 * @param facet     The facet in question; e.g. BF_STING.
 * @param tier      The tier of the mutant beast; e.g.
 * @return          The attack corresponding to the given facet; e.g. BT_LARVAL
 *                  { AT_STING, AF_REACH_STING, 10 }. Scales with HD.
 *                  For facets that don't provide an attack, is { }.
 */
static mon_attack_def _mutant_beast_facet_attack(int facet, int tier)
{
    const int dam = tier * 5;
    switch (facet)
    {
        case BF_STING:
            return { AT_STING, AF_REACH_STING, dam };
        case BF_OX:
            return { AT_TRAMPLE, AF_TRAMPLE, dam };
        case BF_WEIRD:
            return { AT_CONSTRICT, AF_CRUSH, dam * 2 / 5};
        default:
            return { };
    }
}

/**
 * Get the attack type, attack flavour and damage for the given attack of a
 * mutant beast.
 *
 * @param mon           The monster in question.
 * @param attk_number   Which attack number to get.
 * @return              A mon_attack_def for the specified attack.
 */
static mon_attack_def _mutant_beast_attack(const monster &mon, int attk_number)
{
    const int tier = mutant_beast_tier(mon.get_experience_level());
    if (attk_number == 0)
        return { AT_HIT, AF_PLAIN, tier * 7 + 5 };

    if (!mon.props.exists(MUTANT_BEAST_FACETS))
        return { };

    int cur_attk = 1;
    for (auto facet : mon.props[MUTANT_BEAST_FACETS].get_vector())
    {
        const mon_attack_def atk = _mutant_beast_facet_attack(facet.get_int(),
                                                              tier);
        if (atk.type == AT_NONE)
            continue;

        if (cur_attk == attk_number)
            return atk;

        ++cur_attk;
    }

    return { };
}

/**
 * Get the attack type, attack flavour and damage for the given attack of an
 * ancestor granted by Hepliaklqana_ancestor_attack.
 *
 * @param mon           The monster in question.
 * @param attk_number   Which attack number to get.
 * @return              A mon_attack_def for the specified attack.
 */
static mon_attack_def _hepliaklqana_ancestor_attack(const monster &mon,
                                                     int attk_number)
{
    if (attk_number != 0)
        return { };

    const int HD = mon.get_experience_level();
    const int dam = HD + 3; // 4 at 1 HD, 21 at 18 HD (max)
    // battlemages do double base melee damage (+25-50% including their weapon)
    const int dam_mult = mon.type == MONS_ANCESTOR_BATTLEMAGE ? 2 : 1;

    return { AT_HIT, AF_PLAIN, dam * dam_mult };
}

/** Get the attack type, attack flavour and damage for a monster attack.
 *
 * @param mon The monster to look at.
 * @param attk_number Which attack number to get.
 * @param base_flavour If true, attack flavours that are randomised on every attack
 *                     will have their base flavour returned instead of one of the
 *                     random flavours.
 * @return  A mon_attack_def for the specified attack.
 */
mon_attack_def mons_attack_spec(const monster& m, int attk_number,
                                bool base_flavour)
{
    monster_type mc = m.type;

    const monster& mon = get_tentacle_head(m);

    const bool zombified = mons_is_zombified(mon);

    if (mon.has_hydra_multi_attack())
        attk_number -= mon.heads() - 1;

    if (attk_number < 0 || attk_number >= MAX_NUM_ATTACKS)
        attk_number = 0;

    if (mons_is_ghost_demon(mc))
    {
        if (attk_number == 0 && mon.ghost)
        {
            return { mon.ghost->att_type, mon.ghost->att_flav,
                     mon.ghost->damage };
        }

        return { AT_NONE, AF_PLAIN, 0 };
    }
    else if (mc == MONS_MUTANT_BEAST)
        return _mutant_beast_attack(mon, attk_number);
    else if (mons_is_hepliaklqana_ancestor(mc))
        return _hepliaklqana_ancestor_attack(mon, attk_number);

    if (zombified && mc != MONS_KRAKEN_TENTACLE)
        mc = mons_zombie_base(mon);

    ASSERT_smc();
    mon_attack_def attk = smc->attack[attk_number];

    if (attk_number == 0)
    {
        if (m.has_ench(ENCH_FIRE_CHAMPION))
            attk.flavour = AF_FIRE;
        else if (m.has_ench(ENCH_CHAOS_LACE))
            attk.flavour = AF_CHAOTIC;

        if (mon.type == MONS_PLAYER_SHADOW)
        {
            if (mon.props.exists(DITH_SHADOW_ATTACK_KEY))
            {
                attk.damage = mon.props[DITH_SHADOW_ATTACK_KEY].get_int();
                if (mon.mslot_item(MSLOT_WEAPON)
                    && mon.mslot_item(MSLOT_WEAPON)->sub_type == WPN_QUICK_BLADE)
                {
                    attk.damage = attk.damage * 3 / 4;
                }
            }
        }

        // summoning miscast monster; hd scaled with miscast severity
        if (mon.type == MONS_NAMELESS)
            attk.damage = mon.get_hit_dice() * 2;

        if (mon.type == MONS_SOUL_WISP)
            attk.damage = 2 + mon.get_hit_dice();

        if (mon.type == MONS_HAUNTED_ARMOUR)
            attk.damage = 5 + (mon.get_hit_dice() * 3 / 4);

        // Boulder beetles get double attack damage and a normal 'hit' attack.
        if (mon.has_ench(ENCH_ROLLING))
        {
            attk.type = AT_HIT;
            attk.damage *= 2;
        }
    }
    // Give Coglin player shadows a second attack for their second weapon
    else if (attk_number == 1 && mon.type == MONS_PLAYER_SHADOW
             && you.has_mutation(MUT_WIELD_OFFHAND))
    {
        attk.type = AT_HIT;
        if (mon.props.exists(DITH_SHADOW_ATTACK_KEY))
        {
            attk.damage = mon.props[DITH_SHADOW_ATTACK_KEY].get_int();
            if (mon.mslot_item(MSLOT_ALT_WEAPON)
                && mon.mslot_item(MSLOT_ALT_WEAPON)->sub_type == WPN_QUICK_BLADE)
            {
                attk.damage = attk.damage * 3 / 4;
            }
        }
    }
    else if (mons_species(mon.type) == MONS_DRACONIAN
             && mon.type != MONS_DRACONIAN
             && attk.type == AT_NONE
             && smc->attack[attk_number - 1].type != AT_NONE)
    {
        // Nonbase draconians inherit aux attacks from their base type.
        // Implicit assumption: base draconian types only get one aux
        // attack, and it's in their second attack slot.
        // If that changes this code will need to be changed.
        const monsterentry* mbase =
            get_monster_data (draconian_subspecies(mon));
        ASSERT(mbase);
        return mbase->attack[1];
    }

    if (mon.type == MONS_ARMOUR_ECHO)
    {
        item_def *def = mon.get_defining_object();
        if (def)
        {
            ASSERT(def->base_type == OBJ_ARMOUR);
            const int typ = def->sub_type;
            const int ac = armour_prop(typ, PARM_AC);
            attk.damage = ac + ac * ac / 2;
        }
    }
    else if (mon.type == MONS_SHADOW_PUPPET)
    {
        if (attk_number == 2)
            attk.damage = 2 + m.get_hit_dice() * 1 / 3;
        else
            attk.damage = 2 + (m.get_hit_dice() * 3 / 2);
    }

    // Vampires get a bite aux in addition to normal attacks.
    if (mon.has_ench(ENCH_VAMPIRE_THRALL)
            && attk.type == AT_NONE
            && smc->attack[attk_number - 1].type != AT_NONE)
    {
        attk.type = AT_BITE;
        attk.flavour = AF_VAMPIRIC;
        attk.damage = 5 + mon.get_experience_level() * 5 / 4;
    }

    if (!base_flavour)
    {
        // TODO: randomization here is not the greatest way of doing any of
        // these...
        if (attk.type == AT_RANDOM)
        {
            attk.type = random_choose(AT_BITE, AT_STING, AT_SPORE, AT_TOUCH,
                                      AT_PECK, AT_HEADBUTT, AT_PUNCH, AT_KICK,
                                      AT_TENTACLE_SLAP, AT_TAIL_SLAP, AT_GORE,
                                      AT_TRUNK_SLAP);
        }

        if (attk.type == AT_CHERUB)
            attk.type = random_choose(AT_HEADBUTT, AT_BITE, AT_PECK, AT_GORE);
    }

    // Slime creature attacks are multiplied by the number merged.
    if (mon.type == MONS_SLIME_CREATURE && mon.blob_size > 1)
        attk.damage *= mon.blob_size;

    return zombified ? _downscale_zombie_attack(mon, attk) : attk;
}

static int _mons_damage(monster_type mc, int rt)
{
    if (rt < 0 || rt > 3)
        rt = 0;
    ASSERT_smc();
    return smc->attack[rt].damage;
}

string mon_attack_name_short(attack_type attack)
{
    switch (attack)
    {
    case AT_SPORE:         return "spore";
    case AT_TENTACLE_SLAP: return "tentacle";
    case AT_TAIL_SLAP:     return "tail";
    case AT_TRUNK_SLAP:    return "trunk";
    case AT_POUNCE:        return "pounce";
    case AT_CHERUB:
    case AT_RANDOM:        return "hit"; // eh
    default:
        return mon_attack_name(attack, false);
    }
}

/**
 * A short description of the given monster attack type.
 *
 * @param attack      The attack to be described; e.g. AT_HIT, AT_SPORE.
 * @param with_object Is the description being used with an object/target?
 *                    True results in e.g. "pounce on"; false, just "pounce".
 *                    Optional parameter, default true.
 * @return            A short description; e.g. "hit", "release spores at".
 */
string mon_attack_name(attack_type attack, bool with_object)
{
    static const char *attack_types[] =
    {
        "hit",         // including weapon attacks
        "bite",
        "sting",

        // spore
        "release spores at",

        "touch",
        "engulf",
        "claw",
        "peck",
        "headbutt",
        "punch",
        "kick",
        "tentacle-slap",
        "tail-slap",
        "gore",
        "constrict",
        "trample",
        "trunk-slap",
#if TAG_MAJOR_VERSION == 34
        "snap closed at",
        "splash",
#endif
        "pounce on",
#if TAG_MAJOR_VERSION == 34
        "sting",
#endif
        "hit, bite, peck, or gore", // AT_CHERUB
#if TAG_MAJOR_VERSION == 34
        "hit", // AT_SHOOT
#endif
        "hit", // AT_WEAP_ONLY,
        "hit or gore", // AT_RANDOM
    };
    COMPILE_CHECK(ARRAYSZ(attack_types) == NUM_ATTACK_TYPES - AT_FIRST_ATTACK);

    const int verb_index = attack - AT_FIRST_ATTACK;
    ASSERT(verb_index < (int)ARRAYSZ(attack_types));

    if (with_object)
        return attack_types[verb_index];
    else
    {
        return replace_all(replace_all(attack_types[verb_index], " at", ""),
                                                                 " on", "");
    }
}

/**
 * Is this attack flavour 'plain'? A plain attack flavour doesn't imply the
 * target will be affected in any particular way beyond damage.
 * Certain attack flavours like AT_TRAMPLE, AT_SPORE, and AT_ENGULF do imply
 * additional effects, and are not considered 'plain'.
 */
bool is_plain_attack_type(attack_type attack)
{
    switch (attack)
    {
        case AT_CONSTRICT:  // constriction
        case AT_ENGULF:     // water hold
        case AT_POUNCE:     // webbing
        case AT_SPORE:      // confusing spores
        case AT_STING:      // poison
        case AT_TRAMPLE:    // trampling
            return false;
        default:
            return true;
    }
}

/**
 * Does this monster attack flavour trigger even if the base attack does no
 * damage?
 *
 * @param flavour   The attack flavour in question; e.g. AF_COLD.
 * @return          Whether the flavour attack triggers on a successful hit
 *                  regardless of damage done.
 */
bool flavour_triggers_damageless(attack_flavour flavour)
{
    return flavour == AF_CRUSH
        || flavour == AF_ENGULF
        || flavour == AF_PURE_FIRE
        || flavour == AF_AIRSTRIKE
        || flavour == AF_SHADOWSTAB
        || flavour == AF_DROWN
        || flavour == AF_CORRODE;
}

/**
 * How much special damage does the given attack flavour do for an attack from
 * a monster of the given hit dice?
 *
 * Various effects (e.g. acid) currently go through more complex codepaths. :(
 *
 * @param flavour       The attack flavour in question; e.g. AF_FIRE.
 * @param HD            The HD to calculate damage for.
 * @param random        Whether to roll damage, or (if false) just return
 *                      the top of the range.
 * @return              The damage that the given attack flavour does, before
 *                      resists and other effects are applied.
 */
int flavour_damage(attack_flavour flavour, int HD, bool random)
{
    switch (flavour)
    {
        case AF_FIRE:
            if (random)
                return HD + random2(HD);
            return HD * 2;
        case AF_COLD:
            if (random)
                return HD + random2(HD*2);
            return HD * 3;
        case AF_ELEC:
            if (random)
                return HD + random2(HD/2);
            return HD * 3 / 2;
        case AF_PURE_FIRE:
            if (random)
                return HD * 3 / 2 + random2(HD);
            return HD * 5 / 2;
        case AF_DROWN:
            if (random)
                return HD * 3 / 4 + random2(HD * 3 / 4);
            return HD * 3 / 2;
        // Note: This value is only used for displaying monster damage with xv
        //       and is a lie against non-player targets.
        //       Actual attacks call actor->splash_with_acid() directly.
        case AF_ACID:
        case AF_REACH_TONGUE:
            if (random)
                return roll_dice(4, 3);
            return 12;
        // Just show max damage: this number's only used for display.
        case AF_AIRSTRIKE:
            return pow(HD + 1, 1.2) * 12 / 6;
        default:
            return 0;
    }
}

/**
 * Does a monster attacking with this flavour reach as if using a polearm?
 *
 * @param flavour   The attack flavour in question; e.g. AF_COLD.
 * @return          Whether the flavour grants inherent reach.
 */
bool flavour_has_reach(attack_flavour flavour)
{
    switch (flavour)
    {
        case AF_REACH:
        case AF_REACH_STING:
        case AF_REACH_TONGUE:
        case AF_RIFT:
            return true;
        default:
            return false;
    }
}

bool flavour_has_mobility(attack_flavour flavour)
{
    return flavour == AF_SWOOP || flavour == AF_FLANK;
}

bool mons_invuln_will(const monster& mon)
{
    return get_monster_data(mon.type)->willpower == WILL_INVULN;
}

bool mons_skeleton(monster_type mc)
{
    return !mons_class_flag(mc, M_NO_SKELETON);
}

bool mons_flattens_trees(const monster& mon)
{
    return mons_base_type(mon) == MONS_LERNAEAN_HYDRA;
}

/**
 * Given an average max HP value for a given monster type, what should a given
 * monster have?
 *
 * @param avg_hp    The mean hp.
 * @param scale     A scale that the input avg_hp are multiplied by.
 * @return          A max HP value; no more than +-33% from the given average,
 *                  and within about +-10% of the average 95% of the time.
 *                  This value is not multiplied by the scale - it's an actual
 *                  hp value, regardless of the scale on the input.
 *                  If avg_hp is nonzero, always returns at least 1.
 */
int hit_points(int avg_hp, int scale)
{
    if (!avg_hp)
        return 0;

    const int min_perc = 33;
    const int hp_variance = div_rand_round(avg_hp * min_perc, 100);
    const int min_hp = avg_hp - hp_variance;
    const int hp = min_hp + random2avg(hp_variance * 2, 8);
    return max(1, div_rand_round(hp, scale));
}

// This function returns the standard number of hit dice for a type of
// monster, not a particular monster's current hit dice. - bwr
int mons_class_hit_dice(monster_type mc)
{
    const monsterentry *me = get_monster_data(mc);
    return me ? me->HD : 0;
}

/**
 * What base WL does a monster of the given type have?
 *
 * @param type    The monster type in question.
 * @param base    The base type of the monster. (For e.g. draconians.)
 * @return        The WL of a normal monster of that type.
 */
int mons_class_willpower(monster_type type, monster_type base)
{
    const monster_type base_type =
        base != MONS_NO_MONSTER && mons_is_draconian(type)
            ? draconian_subspecies(type, base)
            : type;

    const int type_wl = (get_monster_data(base_type))->willpower;

    // Negative values get multiplied with monster hit dice.
    if (type_wl >= 0)
        return type_wl;
    return mons_class_hit_dice(base_type) * -type_wl * 4 / 3;
}

/**
 * Can a monster of the given type see invisible creatures?
 *
 * @param type    The monster type in question.
 * @param base    The base type of the monster. (For e.g. draconians.)
 * @return        Whether a normal monster of that type can see invisible
 *                things.
 */
bool mons_class_sees_invis(monster_type type, monster_type base)
{
    if (mons_class_flag(type, M_SEE_INVIS))
        return true;

    if (base != MONS_NO_MONSTER && mons_is_draconian(type)
        && mons_class_flag(draconian_subspecies(type, base), M_SEE_INVIS))
    {
        return true;
    }

    return false;
}

/**
 * What's the average hp for a given type of monster?
 *
 * @param mc        The type of monster in question.
 * @return          The average hp for that monster; rounds down.
 */
int mons_avg_hp(monster_type mc, int scale)
{
    const monsterentry* me = get_monster_data(mc);

    if (!me)
        return 0;

    return me->avg_hp_10x * scale / 10;
}

/**
 * What's the maximum hp for a given type of monster?
 *
 * @param mc        The type of monster in question.
 * @return          The maximum hp for that monster; rounds down.
 */
int mons_max_hp(monster_type mc)
{
    const monsterentry* me = get_monster_data(mc);

    if (!me)
        return 0;

    // TODO: merge the 133% with their use in hit_points()
    return me->avg_hp_10x * 133 / 1000;
}

/**
 * How much XP will the given monster give out by dying?
 *
 * @param mon        The monster in question.
 * @param real      If false, calculate XP for the given monster's type instead of
 *               this monster's specific stats.
 * @param legacy  If set, use higher XP values for high-XP monsters to emulate
 *               historical (pre-2eadbcd) behaviour.
 * @return How much XP this monster is worth.
 */
int exper_value(const monster& mon, bool real, bool legacy)
{
    int x_val = 0;

    // These four are the original arguments.
    const monster_type mc = mon.type;
    int hd                = mon.get_experience_level();
    int maxhp             = mon.max_hit_points;

    // pghosts and pillusions have no reasonable base values, and you can look
    // up the exact value anyway. Especially for pillusions.
    if (real || mon.type == MONS_PLAYER_GHOST || mon.type == MONS_PLAYER_ILLUSION)
    {
        // Monsters with extra health are much harder, but the xp value
        // shouldn't depend on whether it was boosted at the moment of death.
        if (mon.has_ench(ENCH_BERSERK))
            maxhp = (maxhp * 2 + 1) / 3;

        if (mon.has_ench(ENCH_DOUBLED_HEALTH))
            maxhp = maxhp / 2;
    }
    else
    {
        const monsterentry *m = get_monster_data(mons_base_type(mon));
        ASSERT(m);

        // Use real hd, zombies would use the basic species and lose
        // information known to the player ("orc warrior zombie"). Monsters
        // levelling up is visible (although it may happen off-screen), so
        // this is hardly ever a leak. Only Pan lords are unknown in the
        // general.
        if (m->mc == MONS_PANDEMONIUM_LORD)
            hd = m->HD;
        maxhp = mons_max_hp(mc);
    }

    // Hacks to make merged slime creatures not worth so much exp. We
    // will calculate the experience we would get for 1 blob, and then
    // just multiply it so that exp is linear with blobs merged. -cao
    if (mon.type == MONS_SLIME_CREATURE && mon.blob_size > 1)
        maxhp /= mon.blob_size;

    // These are some values we care about.
    const int speed       = mons_base_speed(mon);
    const int modifier    = _mons_exp_mod(mc);
    const int item_usage  = mons_itemuse(mon);

    // XXX: Shapeshifters can qualify here, even though they can't cast.
    const bool spellcaster = mon.has_spells();

    // Early out for no XP monsters.
    if (!mons_class_gives_xp(mc))
        return 0;

    x_val = (16 + maxhp) * hd * hd / 10;

    // Let's calculate a simple difficulty modifier. - bwr
    int diff = 0;

    // Let's look for big spells.
    if (spellcaster)
    {
        for (const mon_spell_slot &slot : mon.spells)
        {
            switch (slot.spell)
            {
            case SPELL_PARALYSE:
            case SPELL_SMITING:
            case SPELL_SUMMON_EYEBALLS:
            case SPELL_CALL_DOWN_DAMNATION:
            case SPELL_HURL_DAMNATION:
            case SPELL_SYMBOL_OF_TORMENT:
            case SPELL_FIRE_STORM:
            case SPELL_GLACIATE:
            case SPELL_POLAR_VORTEX:
            case SPELL_SHATTER:
            case SPELL_ORB_OF_ELECTRICITY:
            case SPELL_CHAIN_LIGHTNING:
            case SPELL_LEGENDARY_DESTRUCTION:
            case SPELL_SUMMON_ILLUSION:
            case SPELL_SPELLSPARK_SERVITOR:
            case SPELL_CONJURE_LIVING_SPELLS:
                diff += 25;
                break;

            case SPELL_SUMMON_GREATER_DEMON:
            case SPELL_HASTE:
            case SPELL_PHANTOM_BLITZ:
            case SPELL_BLINK_RANGE:
            case SPELL_PETRIFY:
            case SPELL_VEX:
                diff += 20;
                break;

            case SPELL_VITRIFY:
            case SPELL_BANISHMENT:
            case SPELL_FAKE_MARA_SUMMON:
            case SPELL_PYRE_ARROW:
            case SPELL_MINDBURST:
            case SPELL_IOOD:
            case SPELL_FIREBALL:
            case SPELL_PLASMA_BEAM:
            case SPELL_IRON_SHOT:
            case SPELL_BOMBARD:
            case SPELL_LEHUDIBS_CRYSTAL_SPEAR:
            case SPELL_LRD:
            case SPELL_LIGHTNING_BOLT:
            case SPELL_CONJURE_BALL_LIGHTNING:
            case SPELL_MARCH_OF_SORROWS:
            case SPELL_AGONY:
            case SPELL_DIG:
                diff += 10;
                break;

            case SPELL_SUMMON_DRAGON:
            case SPELL_SUMMON_HORRIBLE_THINGS:
            case SPELL_HAUNT:
            case SPELL_PLANEREND:
            case SPELL_MALIGN_GATEWAY:
            case SPELL_SUMMON_EMPEROR_SCORPIONS:
                diff += 7;
                break;

            default:
                break;
            }
        }
    }

    // Let's look at regeneration.
    if (mons_class_fast_regen(mc))
        diff += 15;

    // Monsters at normal or fast speed with big melee damage.
    if (speed >= 10)
    {
        int max_melee = 0;
        for (int i = 0; i < 4; ++i)
            max_melee += _mons_damage(mc, i);

        if (max_melee > 30)
            diff += (max_melee / ((speed == 10) ? 2 : 1));
    }

    // Monsters who can use equipment (even if only the equipment
    // they are given) can be considerably enhanced because of
    // the way weapons work for monsters. - bwr
    if (item_usage >= MONUSE_STARTING_EQUIPMENT)
        diff += 30;

    // Set a reasonable range on the difficulty modifier...
    // Currently 70% - 200%. - bwr
    if (diff > 100)
        diff = 100;
    else if (diff < -30)
        diff = -30;

    // Apply difficulty.
    x_val *= (100 + diff);
    x_val /= 100;

    // Basic speed modification.
    if (speed > 0)
    {
        x_val *= speed;
        x_val /= 10;
    }

    // Slow monsters without spells and items often have big HD which
    // cause the experience value to be overly large... this tries
    // to reduce the inappropriate amount of XP that results. - bwr
    if (speed < 10 && !spellcaster && item_usage < MONUSE_STARTING_EQUIPMENT)
        x_val /= 2;

    // Apply the modifier in the monster's definition.
    if (modifier > 0)
    {
        x_val *= modifier;
        x_val /= 10;
    }

    // Scale starcursed mass exp by what percentage of the whole it represents
    if (mon.type == MONS_STARCURSED_MASS)
        x_val = (x_val * mon.blob_size) / 12;

    // Further reduce xp from zombies
    if (mons_is_zombified(mon))
        x_val /= 2;

    // Reductions for big values. - bwr
    if (x_val > 100)
        x_val = 100 + ((x_val - 100) * 3) / 4;
    if (x_val > 750)
        x_val = 750 + (x_val - 750) / (legacy ? 3 : 6);

    // Slime creature exp hack part 2: Scale exp back up by the number
    // of blobs merged. -cao
    // Has to be after the stepdown to prevent issues with 4-5 merged slime
    // creatures. -pf
    if (mon.type == MONS_SLIME_CREATURE && mon.blob_size > 1)
        x_val *= mon.blob_size;

    // Guarantee the value is within limits.
    if (x_val <= 0)
        x_val = 1;

    return x_val;
}

static monster_type _random_mons_between(monster_type min, monster_type max)
{
    monster_type mc = MONS_PROGRAM_BUG;

    do // skip removed monsters
    {
        mc = static_cast<monster_type>(random_range(min, max));
    }
    while (mons_is_removed(mc));

    return mc;
}

monster_type random_draconian_monster_species()
{
    return _random_mons_between(MONS_FIRST_BASE_DRACONIAN,
                                MONS_LAST_SPAWNED_DRACONIAN);
}

monster_type random_draconian_job()
{
    return _random_mons_between(MONS_FIRST_NONBASE_DRACONIAN,
                                MONS_LAST_NONBASE_DRACONIAN);
}

static const pair<monster_type, monster_type> _draconian_combos[] =
{
    { MONS_DRACONIAN_STORMCALLER, MONS_WHITE_DRACONIAN },
    { MONS_DRACONIAN_MONK, MONS_GREEN_DRACONIAN },
    { MONS_DRACONIAN_SHIFTER, MONS_PURPLE_DRACONIAN },
    { MONS_DRACONIAN_ANNIHILATOR, MONS_YELLOW_DRACONIAN },
    { MONS_DRACONIAN_KNIGHT, MONS_BLACK_DRACONIAN },
    { MONS_DRACONIAN_SCORCHER, MONS_RED_DRACONIAN },
#if TAG_MAJOR_VERSION == 34
    { MONS_PROGRAM_BUG, MONS_PROGRAM_BUG }, // MONS_MOTTLED_DRACONIAN
#endif
};

// Should have exactly one job per randomly-spawning draconian colour.
COMPILE_CHECK(ARRAYSZ(_draconian_combos) == MONS_LAST_SPAWNED_DRACONIAN
                                            - MONS_FIRST_BASE_DRACONIAN + 1);

monster_type draconian_colour_for_job(monster_type job)
{
    for (auto &drac : _draconian_combos)
        if (drac.first == job)
            return drac.second;

    return MONS_PROGRAM_BUG;
}

monster_type draconian_job_for_colour(monster_type colour)
{
    for (auto &drac : _draconian_combos)
        if (drac.second == colour)
            return drac.first;

    // Doesn't normally spawn, but should still return a random job.
    if (colour > MONS_LAST_SPAWNED_DRACONIAN
        && colour <= MONS_LAST_BASE_DRACONIAN)
    {
        return random_draconian_job();
    }

    return MONS_PROGRAM_BUG;
}

static mon_spellbook_type _get_mc_spellbook(const monster_type mon_type)
{
    return static_cast<mon_spellbook_type>(get_monster_data(mon_type)->sec);
}

mon_spellbook_type get_spellbook(const monster_info &mon)
{
    // special case for vault monsters: if they have a custom book,
    // treat it as MST_GHOST
    if (mon.props.exists(CUSTOM_SPELLS_KEY))
        return MST_GHOST;

    return _get_mc_spellbook(mon.type);
}

// Get a list of unique spells from a monster's preset spellbooks
// or in the case of ghosts their actual spells.
// If flags is non-zero, it returns only spells that match those flags.
vector<mon_spell_slot> get_unique_spells(const monster_info &mi,
                               mon_spell_slot_flags flags)
{
    // No entry for MST_GHOST
    COMPILE_CHECK(ARRAYSZ(mspell_list) == NUM_MSTYPES - 1);

    const mon_spellbook_type book = get_spellbook(mi);

    // TODO: should we build an index to speed this reverse lookup?
    unsigned int msidx;
    for (msidx = 0; msidx < ARRAYSZ(mspell_list); ++msidx)
        if (mspell_list[msidx].type == book)
            break;

    vector<mon_spell_slot> slots;

    if (mons_genus(mi.type) == MONS_DRACONIAN)
    {
        const mon_spell_slot breath =
            drac_breath(mi.draconian_subspecies());

        if (breath.spell != SPELL_NO_SPELL
            && (flags == MON_SPELL_NO_FLAGS || (breath.flags & flags)))
        {
            slots.push_back(breath);
        }
    }

    // No other spells (e.g. drac and/or wand); quit right away.
    if (book == MST_NO_SPELLS)
        return slots;

    if (book != MST_GHOST)
        ASSERT(msidx < ARRAYSZ(mspell_list));
    for (const mon_spell_slot &slot : (book == MST_GHOST
                                       ? mi.spells
                                       : mspell_list[msidx].spells))
    {
        if (flags != MON_SPELL_NO_FLAGS && !(slot.flags & flags))
            continue;

        if (none_of(slots.begin(), slots.end(),
            [&](const mon_spell_slot& oldslot)
            {
                return oldslot.spell == slot.spell;
            }))
        {
            slots.push_back(slot);
        }
    }

    return slots;
}

mon_spell_slot drac_breath(monster_type drac_type)
{
    spell_type sp;
    switch (drac_type)
    {
    case MONS_BLACK_DRACONIAN:   sp = SPELL_LIGHTNING_BOLT; break;
    case MONS_YELLOW_DRACONIAN:  sp = SPELL_SPIT_ACID; break;
    case MONS_GREEN_DRACONIAN:   sp = SPELL_POISONOUS_CLOUD; break;
    case MONS_PURPLE_DRACONIAN:  sp = SPELL_QUICKSILVER_BOLT; break;
    case MONS_RED_DRACONIAN:     sp = SPELL_SEARING_BREATH; break;
    case MONS_WHITE_DRACONIAN:   sp = SPELL_COLD_BREATH; break;
    case MONS_DRACONIAN:
    case MONS_GREY_DRACONIAN:    sp = SPELL_NO_SPELL; break;
    case MONS_PALE_DRACONIAN:    sp = SPELL_STEAM_BALL; break;

    default:
        die("Invalid draconian subrace: %d", drac_type);
    }

    mon_spell_slot slot;
    slot.spell = sp;
    slot.freq = 62;
    slot.flags = MON_SPELL_NATURAL | MON_SPELL_BREATH;
    return slot;
}

void mons_load_spells(monster& mon)
{
    const mon_spellbook_type book = _get_mc_spellbook(mon.type);

    if (book == MST_GHOST)
        return mon.load_ghost_spells();

    mon.spells.clear();
    if (mons_genus(mon.type) == MONS_DRACONIAN)
    {
        mon_spell_slot breath = drac_breath(draconian_subspecies(mon));
        if (breath.spell != SPELL_NO_SPELL)
            mon.spells.push_back(breath);
    }

    if (book == MST_NO_SPELLS)
        return;

    dprf(DIAG_MONPLACE, "%s: loading spellbook #%d",
         mon.name(DESC_PLAIN, true).c_str(), static_cast<int>(book));

    for (const mon_spellbook &spbook : mspell_list)
        if (spbook.type == book)
        {
            mon.spells.insert(mon.spells.end(),
                                spbook.spells.begin(), spbook.spells.end());
            break;
        }
}

// Never hand out DARKGREY as a monster colour, even if it is randomly
// chosen.
colour_t random_monster_colour()
{
    colour_t col = DARKGREY;
    while (col == DARKGREY)
        col = random_colour();

    return col;
}

// Generate a shiny, new and unscarred monster.
void define_monster(monster& mons, bool friendly)
{
    monster_type mcls         = mons.type;
    ASSERT(!mons_class_is_zombified(mcls)); // should have called define_zombie

    monster_type monbase      = mons.base_monster;
    const monsterentry *m     = get_monster_data(mcls);
    int col                   = mons_class_colour(mcls);
    int hd                    = mons_class_hit_dice(mcls);
    int hp = 0;

    mons.mname.clear();

    // misc
    mons.god = GOD_NO_GOD;

    switch (mcls)
    {
    case MONS_SLIME_CREATURE:
        // Slime creatures start off as only single un-merged blobs.
        mons.blob_size = 1;
        break;

    case MONS_HYDRA:
        // Hydras start off with 4 to 8 heads.
        mons.num_heads = random_range(4, 8);
        break;

    case MONS_LERNAEAN_HYDRA:
        // The Lernaean hydra starts off with 27 heads.
        mons.num_heads = 27;
        break;

    case MONS_TIAMAT:
        // Initialise to a random draconian type.
        draconian_change_colour(&mons);
        monbase = mons.base_monster;
        col = mons.colour;
        break;

    case MONS_STARCURSED_MASS:
        mons.blob_size = 12;
        break;

    // Randomize starting speed burst clock
    case MONS_SIXFIRHY:
    case MONS_JIANGSHI:
        mons.move_spurt = random2(360);
        break;

    case MONS_SHAMBLING_MANGROVE:
        mons.mangrove_pests = x_chance_in_y(3, 5) ? random_range(2, 3) : 0;
        break;

    case MONS_SERPENT_OF_HELL:
    case MONS_SERPENT_OF_HELL_COCYTUS:
    case MONS_SERPENT_OF_HELL_DIS:
    case MONS_SERPENT_OF_HELL_TARTARUS:
        mons.num_heads = 3;
        break;

    default:
        break;
    }

    if (mons_is_draconian_job(mcls))
        monbase = draconian_colour_for_job(mcls);

    if (col == COLOUR_UNDEF) // but never give out darkgrey to monsters
        col = random_monster_colour();

    // Some calculations.
    if (hp == 0)
        hp = hit_points(m->avg_hp_10x);
    const int hp_max = hp;

    // So let it be written, so let it be done.
    mons.set_hit_dice(hd);
    mons.hit_points      = hp;
    mons.max_hit_points  = hp_max;
    mons.speed_increment = 70;

    if (mons.base_monster == MONS_NO_MONSTER
        || mons.base_monster == MONS_PROGRAM_BUG) // latter is zombie gen
    {
        mons.base_monster = monbase;
    }

    mons.flags      = MF_NO_FLAGS;
    mons.colour     = col;

    mons.bind_melee_flags();

    mons_load_spells(mons);

    // Reset monster enchantments.
    mons.enchantments.clear();
    mons.ench_cache.reset();
    mons.ench_countdown = 0;

    switch (mcls)
    {
    case MONS_PANDEMONIUM_LORD:
    {
        ghost_demon ghost;
        ghost.init_pandemonium_lord(friendly);
        mons.set_ghost(ghost);
        mons.ghost_demon_init();
        mons.bind_melee_flags();
        break;
    }

    case MONS_ORC_APOSTLE:
    {
        ghost_demon ghost;

        // Pull type and power from props, if they have been set
        apostle_type type = mons.props.exists(APOSTLE_TYPE_KEY)
                            ? static_cast<apostle_type>(mons.props[APOSTLE_TYPE_KEY].get_int())
                            : APOSTLE_WARRIOR;

        int pow = mons.props.exists(APOSTLE_POWER_KEY)
                    ? mons.props[APOSTLE_POWER_KEY].get_int()
                    : 50;

        ghost.init_orc_apostle(type, pow);
        mons.set_ghost(ghost);
        mons.ghost_demon_init();

        mons.props[MON_GENDER_KEY] = random_choose(GENDER_MALE,
                                                   GENDER_FEMALE,
                                                   GENDER_NEUTRAL);

        if (type == APOSTLE_WIZARD)
            mons.flags |= MF_CAUTIOUS;

        // Choose tile based on apostle class
        if (type == APOSTLE_WIZARD)
            mons.props[TILE_NUM_KEY].get_short() = 0;
        else if (type == APOSTLE_PRIEST)
            mons.props[TILE_NUM_KEY].get_short() = 100;
        else
            mons.props[TILE_NUM_KEY].get_short() = 200;

        _give_apostle_proper_name(mons, type);

        // Reroll our name until it is different from all player apostle names,
        // to try and lessen possible confusion if they end up with two that
        // have identical names.
        while (!apostle_has_unique_name(mons))
            _give_apostle_proper_name(mons, type);

        break;
    }

    case MONS_PLAYER_GHOST:
    {
        if (define_ghost_from_bones(mons))
            break;
        dprf("No ghosts found in bones, falling back on mirrored player");
        // intentional fallthrough -- fall back on mirroring the player
    }
    case MONS_PLAYER_ILLUSION:
    {
        ghost_demon ghost;
        ghost.init_player_ghost();
        if (mcls == MONS_PLAYER_GHOST)
        {
            // still don't allow undead ghosts, even mirrored
            if (you.undead_state(false) != US_ALIVE)
                ghost.species = SP_HUMAN;
            mons.props[MIRRORED_GHOST_KEY] = true;
        }
        mons.set_ghost(ghost);
        mons.ghost_init(!mons.props.exists(FAKE_MON_KEY));
        break;
    }

    case MONS_UGLY_THING:
    case MONS_VERY_UGLY_THING:
    {
        ghost_demon ghost;
        ghost.init_ugly_thing(mcls == MONS_VERY_UGLY_THING);
        mons.set_ghost(ghost);
        mons.uglything_init();
        break;
    }

    // Load with dummy values so certain monster properties can be queried
    // before placement without crashing (proper setup is done later here)
    case MONS_DANCING_WEAPON:
    case MONS_SPECTRAL_WEAPON:
    case MONS_INUGAMI:
    case MONS_PLATINUM_PARAGON:
    {
        ghost_demon ghost;
        ghost.barebones_init();
        mons.set_ghost(ghost);
        break;
    }

    case MONS_NAMELESS_REVENANT:
        initialize_nobody_memories(mons);
        break;

    default:
        break;
    }

    mons.calc_speed();

    // When all is said and done, this monster had better have some hit
    // points, or it will be dead on arrival
    ASSERT(mons.hit_points > 0);
    ASSERT(mons.max_hit_points > 0);
}

static const char *ugly_colour_names[] =
{
    "red", "brown", "green", "cyan", "purple", "white"
};

string ugly_thing_colour_name(colour_t colour)
{
    int colour_offset = ugly_thing_colour_offset(colour);

    if (colour_offset == -1)
        return "buggy";

    return ugly_colour_names[colour_offset];
}

static const colour_t ugly_colour_values[] =
{
    RED, BROWN, GREEN, CYAN, MAGENTA, LIGHTGREY
};

colour_t ugly_thing_random_colour()
{
    return RANDOM_ELEMENT(ugly_colour_values);
}

int str_to_ugly_thing_colour(const string &s)
{
    COMPILE_CHECK(ARRAYSZ(ugly_colour_values) == ARRAYSZ(ugly_colour_names));
    for (int i = 0, size = ARRAYSZ(ugly_colour_values); i < size; ++i)
        if (s == ugly_colour_names[i])
            return ugly_colour_values[i];
    return BLACK;
}

int ugly_thing_colour_offset(colour_t colour)
{
    for (unsigned i = 0; i < ARRAYSZ(ugly_colour_values); ++i)
    {
        if (make_low_colour(colour) == ugly_colour_values[i])
            return i;
    }

    return -1;
}

void ugly_thing_apply_uniform_band_colour(mgen_data &mg,
    const monster_type *band_monsters, size_t num_monsters_in_band)
{
    // Verify that the whole band is ugly.
    for (size_t i = 0; i < num_monsters_in_band; i++)
    {
        if (!(band_monsters[i] == MONS_UGLY_THING
            || band_monsters[i] == MONS_VERY_UGLY_THING))
        {
            return;
        }
    }

    // Apply a uniform colour to a fully-ugly band.
    if (ugly_thing_colour_offset(mg.colour) == -1)
        mg.colour = ugly_thing_random_colour();
}

static const char *drac_colour_names[] =
{
    "black",
#if TAG_MAJOR_VERSION == 34
    "",
#endif
    "yellow", "green", "purple", "red", "white", "grey", "pale"
};

string draconian_colour_name(monster_type mon_type)
{
    COMPILE_CHECK(ARRAYSZ(drac_colour_names) ==
                  MONS_LAST_BASE_DRACONIAN - MONS_DRACONIAN);

    if (!mons_is_base_draconian(mon_type) || mon_type == MONS_DRACONIAN)
        return "buggy";

    return drac_colour_names[mon_type - MONS_FIRST_BASE_DRACONIAN];
}

monster_type draconian_colour_by_name(const string &name)
{
    COMPILE_CHECK(ARRAYSZ(drac_colour_names)
                  == (MONS_LAST_BASE_DRACONIAN - MONS_DRACONIAN));

    for (unsigned i = 0; i < ARRAYSZ(drac_colour_names); ++i)
    {
        if (name == drac_colour_names[i])
            return static_cast<monster_type>(i + MONS_FIRST_BASE_DRACONIAN);
    }

    return MONS_PROGRAM_BUG;
}

string mons_type_name(monster_type mc, description_level_type desc)
{
    string result;

    if (!mons_is_unique(mc))
    {
        switch (desc)
        {
        case DESC_THE:       result = "the "; break;
        case DESC_A:         result = "a ";   break;
        case DESC_PLAIN: default:             break;
        }
    }

    switch (mc)
    {
    case RANDOM_MONSTER:
        result += "random monster";
        return result;
    case RANDOM_DRACONIAN:
        result += "random draconian";
        return result;
    case RANDOM_BASE_DRACONIAN:
        result += "random base draconian";
        return result;
    case RANDOM_NONBASE_DRACONIAN:
        result += "random nonbase draconian";
        return result;
    case WANDERING_MONSTER:
        result += "wandering monster";
        return result;
    default: ;
    }

    const monsterentry *me = get_monster_data(mc);
    if (me == nullptr)
    {
        result += make_stringf("invalid monster_type %d", mc);
        return result;
    }

    result += me->name;

    // Vowel fix: Change 'a orc' to 'an orc'..
    if (result.length() >= 3
        && (result[0] == 'a' || result[0] == 'A')
        && result[1] == ' '
        && is_vowel(result[2])
        // XXX: Hack
        && !starts_with(&result[2], "one-"))
    {
        result.insert(1, "n");
    }

    return result;
}

static string _get_proper_monster_name(const monster& mon)
{
    const monsterentry *me = mon.find_monsterentry();
    if (!me)
        return "";

    string name = getRandMonNameString(me->name);
    if (!name.empty())
        return name;

    return getRandMonNameString(get_monster_data(mons_genus(mon.type))->name);
}

// Names a monster (will rename it if it already had a name)
bool give_monster_proper_name(monster& mon)
{
    mon.mname = _get_proper_monster_name(mon);
    if (!mon.props.exists(DBNAME_KEY))
        mon.props[DBNAME_KEY] = mons_class_name(mon.type);

    return mon.is_named();
}

// Names an orc apostle (will rename it if it already had a name)
static bool _give_apostle_proper_name(monster& mon, apostle_type type)
{
    string apostle_key = "orc apostle " + apostle_type_names[type] + " name";
    mon.mname = getRandMonNameString(apostle_key);

    // XXX: The rest of this is duplicated from give_monster_proper_name().
    if (!mon.props.exists(DBNAME_KEY))
        mon.props[DBNAME_KEY] = mons_class_name(mon.type);

    return mon.is_named();
}

// See mons_init for initialization of mon_entry array.
monsterentry *get_monster_data(monster_type mc)
{
    if (mc >= 0 && mc < NUM_MONSTERS)
        return &mondata[mon_entry[mc]];
    else
        return nullptr;
}

static int _mons_exp_mod(monster_type mc)
{
    ASSERT_smc();
    return smc->exp_mod;
}

int mons_class_base_speed(monster_type mc)
{
    ASSERT_smc();
    return smc->speed;
}

mon_energy_usage mons_class_energy(monster_type mc)
{
    ASSERT_smc();
    return smc->energy_usage;
}

mon_energy_usage mons_energy(const monster& mon)
{
    mon_energy_usage meu = mons_class_energy(mons_base_type(mon));
    if (mon.ghost)
        meu.move = meu.swim = mon.ghost->move_energy;
    if (mon.has_ench(ENCH_FIRE_CHAMPION))
        meu.move = meu.move * 2 / 3;
    return meu;
}

int mons_class_zombie_base_speed(monster_type zombie_base_mc)
{
    return max(3, mons_class_base_speed(zombie_base_mc) - 2);
}

/**
 * What's this monster's base speed, before temporary effects are applied?
 *
 * @param mon       The monster in question.
 * @param known     Whether to include only information the player knows about,
 *                  i.e. not the speed of certain monsters with varying speeds
 *                  (abominations, hell beasts)
 * @return          The speed of the monster.
 */
int mons_base_speed(const monster& mon, bool known)
{
    if (mon.ghost)
        return mon.ghost->speed;

    if (mon.props.exists(MON_SPEED_KEY)
        && (!known || mon.type == MONS_MUTANT_BEAST))
    {
        return mon.props[MON_SPEED_KEY];
    }

    if (mon.mons_species() == MONS_SPECTRAL_THING)
        return mons_class_base_speed(mons_zombie_base(mon));

    return mons_is_zombified(mon) ? mons_class_zombie_base_speed(mons_zombie_base(mon))
                                  : mons_class_base_speed(mon.type);
}

mon_intel_type mons_class_intel(monster_type mc)
{
    ASSERT_smc();
    return smc->intel;
}

mon_intel_type mons_intel(const monster& m)
{
    const monster& mon = get_tentacle_head(m);

    if (mon.type == MONS_BOUND_SOUL)
        return mons_class_intel(mons_zombie_base(mon));

    return mons_class_intel(mon.type);
}

static habitat_type _mons_class_habitat(monster_type mc,
                                        bool real_amphibious = false)
{
    const monsterentry *me = get_monster_data(mc);
    habitat_type ht = (me ? me->habitat
                          : get_monster_data(MONS_PROGRAM_BUG)->habitat);
    if (!real_amphibious)
    {
        // XXX: No class equivalent of monster::body_size(PSIZE_BODY)!
        size_type st = (me ? me->size
                           : get_monster_data(MONS_PROGRAM_BUG)->size);
        if (ht == HT_LAND && st >= SIZE_GIANT)
            ht = HT_AMPHIBIOUS;
    }
    return ht;
}

habitat_type mons_habitat_type(monster_type t, monster_type base_t,
                               bool real_amphibious)
{
    return _mons_class_habitat(fixup_zombie_type(t, base_t),
                               real_amphibious);
}

habitat_type mons_habitat(const monster& mon, bool real_amphibious)
{
    const monster_type type = mons_is_draconian_job(mon.type)
        ? draconian_subspecies(mon) : mon.type;

    return mons_habitat_type(type, mons_base_type(mon), real_amphibious);
}

habitat_type mons_class_primary_habitat(monster_type mc)
{
    habitat_type ht = _mons_class_habitat(mc);
    if (ht == HT_AMPHIBIOUS || ht == HT_AMPHIBIOUS_LAVA)
        ht = HT_LAND;
    return ht;
}

habitat_type mons_primary_habitat(const monster& mon)
{
    const monster_type type = mons_is_draconian_job(mon.type)
        ? draconian_subspecies(mon) : mons_base_type(mon);

    return mons_class_primary_habitat(type);
}

habitat_type mons_class_secondary_habitat(monster_type mc)
{
    habitat_type ht = _mons_class_habitat(mc);
    if (ht == HT_AMPHIBIOUS)
        ht = HT_WATER;
    if (ht == HT_AMPHIBIOUS_LAVA)
        ht = HT_LAVA;
    return ht;
}

int mons_power(monster_type mc)
{
    // For now, just return monster hit dice.
    ASSERT_smc();
    return mons_class_hit_dice(mc);
}

/// Are two actors 'aligned'? (Will they refuse to attack each-other?)
bool mons_aligned(const actor *m1, const actor *m2)
{
    if (!m1 || !m2)
        return true;

    if (mons_is_projectile(m1->type) || mons_is_projectile(m2->type))
        return true; // they won't directly attack each-other, anyway

    return mons_atts_aligned(m1->temp_attitude(), m2->temp_attitude());
}

bool mons_atts_aligned(mon_attitude_type fr1, mon_attitude_type fr2)
{
    if (mons_att_wont_attack(fr1) && mons_att_wont_attack(fr2))
        return true;

    return fr1 == fr2;
}

bool mons_class_wields_two_weapons(monster_type mc)
{
    return mons_class_flag(mc, M_TWO_WEAPONS);
}

bool mons_wields_two_weapons(const monster& mon)
{
    if (testbits(mon.flags, MF_TWO_WEAPONS))
        return true;

    return mons_class_wields_two_weapons(mons_base_type(mon));
}

// When this monster reaches its target, does it do impact damage
// and then cease to exist?
bool mons_destroyed_on_impact(const monster& m)
{
    return mons_is_projectile(m) || m.type == MONS_FOXFIRE;
}

// When this monster reaches its target, does it explode and then
// cease to exist?
bool mons_blows_up(const monster& m)
{
    return mon_explodes_on_death(m.type) && m.type != MONS_BENNU;
}

// When this monster reaches its target, does it cease to exist?
bool mons_self_destructs(const monster& m)
{
    return mons_blows_up(m) || mons_destroyed_on_impact(m);
}

/// Does this monster trigger your shoutitis? (Random.)
bool should_shout_at_mons(const monster &m)
{
    return !m.is_peripheral()
        && x_chance_in_y(you.get_mutation_level(MUT_SCREAM) * 6, 100);
}

/// Does this monster trigger your attractitis? (Random.)
bool should_attract_mons(const monster &m)
{
    return you.has_mutation(MUT_INITIALLY_ATTRACTIVE)
        && one_chance_in(3)
        && grid_distance(you.pos(), m.pos()) > 2
        && !mons_is_tentacle_or_tentacle_segment(m.type)
        && !m.is_peripheral()
        && !m.is_summoned() // XXX: unsure about this
        && !m.no_tele();
}

bool mons_att_wont_attack(mon_attitude_type fr)
{
    return fr == ATT_FRIENDLY || fr == ATT_GOOD_NEUTRAL || fr == ATT_MARIONETTE;
}

mon_attitude_type mons_attitude(const monster& m)
{
    return m.temp_attitude();
}

bool mons_is_confused(const monster& m, bool class_too)
{
    return (m.has_ench(ENCH_CONFUSION) || m.has_ench(ENCH_MAD) || m.sleepwalking())
           && (class_too || !mons_class_flag(m.type, M_CONFUSED));
}

bool mons_is_wandering(const monster& m)
{
    return m.behaviour == BEH_WANDER || m.behaviour == BEH_BATTY;
}

bool mons_is_seeking(const monster& m)
{
    return m.behaviour == BEH_SEEK;
}

bool mons_is_unbreathing(monster_type mc)
{
    return bool(mons_class_holiness(mc) & (MH_UNDEAD | MH_NONLIVING
                                           | MH_PLANT));
}

// Either running in fear, or trapped and unable to do so (but still wishing to)
bool mons_is_fleeing(const monster& m)
{
    return m.behaviour == BEH_FLEE || mons_is_cornered(m);
}

// Can be either an orderly withdrawal (from which the monster can stop at will)
// or running in fear (from which they cannot)
bool mons_is_retreating(const monster& m)
{
    return m.behaviour == BEH_RETREAT || mons_is_fleeing(m);
}

bool mons_is_cornered(const monster& m)
{
    return m.behaviour == BEH_CORNERED;
}

bool mons_is_influenced_by_sanctuary(const monster& m)
{
    return !m.wont_attack() && !m.is_stationary();
}

bool mons_is_fleeing_sanctuary(const monster& m)
{
    return sanctuary_exists()
           && (m.flags & MF_FLEEING_FROM_SANCTUARY)
           && mons_is_influenced_by_sanctuary(m);
}

// Monster was just put to sleep and should not awaken for any reason until the
// next player action.
bool mons_just_slept(const monster& m)
{
    return bool(m.flags & MF_JUST_SLEPT);
}

// Monster has an enchanted sleep effect and should not awaken from noise or
// automatic stealth checks until it is over (but directly harming them will
// still do it).
bool mons_is_deep_asleep(const monster& m)
{
    return mons_just_slept(m) || m.has_ench(ENCH_DEEP_SLEEP);
}

// Moving body parts, turning oklob flowers and so on counts as motile here.
// So does preparing resurrect, struggling against a net, etc.
bool mons_is_immotile(const monster& mons)
{
    return mons.is_firewood()
        || mons.petrified()
        || mons.asleep()
        || mons.paralysed();
}

bool mons_is_batty(const monster& m)
{
    return mons_class_flag(m.type, M_BATTY) || m.has_facet(BF_BAT);
}

bool mons_is_removed(monster_type mc)
{
    return mc != MONS_PROGRAM_BUG && mons_species(mc) == MONS_PROGRAM_BUG;
}

bool mons_looks_distracted(const monster& m)
{
    return m.foe != MHITYOU && m.behaviour != BEH_BATTY && !m.friendly();
}

void mons_start_fleeing_from_sanctuary(monster& mons)
{
    mons.flags |= MF_FLEEING_FROM_SANCTUARY;
    mons.target = env.sanctuary_pos;
    behaviour_event(&mons, ME_SCARE, 0, env.sanctuary_pos);
}

void mons_stop_fleeing_from_sanctuary(monster& mons)
{
    const bool had_flag {mons.flags & MF_FLEEING_FROM_SANCTUARY};
    mons.flags &= (~MF_FLEEING_FROM_SANCTUARY);
    if (had_flag)
        behaviour_event(&mons, ME_EVAL, &you);
}

void mons_pacify(monster& mon, mon_attitude_type att, bool no_xp)
{
    // If the _real_ (non-charmed) attitude is already that or better,
    // don't degrade it.
    if (mon.attitude >= att)
        return;

    // Must be done before attitude change, so that proper targets are affected
    if (mon.type == MONS_FLAYED_GHOST)
        end_flayed_effect(&mon);

    // Make the monster permanently neutral.
    mon.attitude = att;
    mon.flags |= MF_WAS_NEUTRAL;

    if (!testbits(mon.flags, MF_PACIFIED) // Don't allow repeatedly pacifying.
        && !no_xp
        && !mon.is_summoned()
        && !testbits(mon.flags, MF_NO_REWARD))
    {
        // Give the player full XP.
        gain_exp(exper_value(mon));
    }
    mon.flags |= MF_PACIFIED;

    if (mon.type == MONS_GERYON)
    {
        simple_monster_message(mon,
            make_stringf(" discards %s horn.",
                         mon.pronoun(PRONOUN_POSSESSIVE).c_str()).c_str());
        monster_drop_things(&mon, false, item_is_horn_of_geryon);
    }

    // End constriction.
    mon.stop_constricting_all();
    mon.stop_being_constricted();

    // Cancel fleeing and such.
    mon.behaviour = BEH_WANDER;

    // Remove haunting, which would otherwise cause monster to continue attacking
    mon.del_ench(ENCH_HAUNTING, true, true);

    // Remove level annotation.
    mon.props[NO_ANNOTATE_KEY] = true;
    remove_unique_annotation(&mon);

    // Make the monster begin leaving the level.
    behaviour_event(&mon, ME_EVAL);

    if (mons_is_mons_class(&mon, MONS_PIKEL))
        pikel_band_neutralise();
    if (mons_is_elven_twin(&mon))
        elven_twins_pacify(&mon);
    if (mons_is_mons_class(&mon, MONS_KIRKE))
        hogs_to_humans();
    if (mon.type == MONS_VAULT_WARDEN)
        timeout_terrain_changes(0, true);

    mons_att_changed(&mon);
}

static bool _mons_should_fire_beneficial(bolt &beam)
{
    // Should monster heal other, haste other or might other be able to
    // target the player? Saying no for now. -cao
    if (beam.target == you.pos())
        return false;

    // Assuming all beneficial beams are enchantments if a foe is in
    // the path the beam will definitely hit them so we shouldn't fire
    // in that case.
    if (beam.friend_info.count == 0
        || beam.foe_info.count != 0)
    {
        return false;
    }

    // Should beneficial monster enchantment beams be allowed in a
    // sanctuary? -cao
    if (is_sanctuary(you.pos()) || is_sanctuary(beam.source))
        return false;

    return true;
}

static bool _beneficial_beam_flavour(beam_type flavour)
{
    switch (flavour)
    {
    case BEAM_HASTE:
    case BEAM_HEALING:
    case BEAM_DOUBLE_HEALTH:
    case BEAM_INVISIBILITY:
    case BEAM_MIGHT:
    case BEAM_AGILITY:
    case BEAM_RESISTANCE:
    case BEAM_CONCENTRATE_VENOM:
        return true;

    default:
        return false;
    }
}

bool mons_should_fire(bolt &beam, bool ignore_good_idea)
{
    dprf("tracer: foes %d (pow: %d), friends %d (pow: %d), "
         "foe_ratio: %d",
         beam.foe_info.count, beam.foe_info.power,
         beam.friend_info.count, beam.friend_info.power,
         beam.foe_ratio);

    // Use different evaluation criteria if the beam is a beneficial
    // enchantment (haste other).
    if (_beneficial_beam_flavour(beam.flavour))
        return _mons_should_fire_beneficial(beam);

    if (is_sanctuary(you.pos()) || is_sanctuary(beam.source))
        return false;

    if (ignore_good_idea)
        return true;
    // After this point, safety/self-interest checks only.


    // Friendly monsters shouldn't be targeting you: this will happen
    // often because the default behaviour for charmed monsters is to
    // have you as a target. While foe_ratio will handle this, we
    // don't want a situation where a friendly dragon breathes through
    // you to hit other creatures... it should target the other
    // creatures, and coincidentally hit you.
    //
    // FIXME: this can cause problems with reflection, bounces, etc.
    // It would be better to have the monster fire logic never reach
    // this point for friendlies.
    if (monster_by_mid(beam.source_id))
    {
        monster* m = monster_by_mid(beam.source_id);
        if (m->alive() && m->friendly() && beam.target == you.pos())
            return false;
    }

    return beam.good_to_fire() >= ai_action::good();
}

/**
 * Can monsters use the given spell effectively from range? (If a monster has
 * the given spell, should it try to keep its distance from its enemies?)
 *
 * @param monspell      The spell in question.
 * @param attack_only   Whether to only count spells which directly harm
 *                      enemies (ie: do damage). Overrides ench_too.
 * @param ench_too      Whether to count temporary debilitating effects (Slow,
 *                      etc).
 * @return              Whether the given spell should be considered 'ranged'.
 */
static bool _ms_ranged_spell(spell_type monspell, bool attack_only = false,
                             bool ench_too = true)
{
    // summoning spells are usable from ranged, but not direct attacks.
    if (spell_typematch(monspell, spschool::summoning))
        return !attack_only;

    const spell_flags flags = get_spell_flags(monspell);

    // buffs & escape spells aren't considered 'ranged'.
    if (testbits(flags, spflag::selfench)
        || testbits(flags, spflag::escape)
        || monspell == SPELL_BLINK_OTHER_CLOSE)
    {
        return false;
    }

    // conjurations are attacks.
    if (spell_typematch(monspell, spschool::conjuration))
        return true;

    // hexes that aren't conjurations or summons are enchantments.
    if (spell_typematch(monspell, spschool::hexes))
        return !attack_only && ench_too;

    switch (monspell)
    {
    case SPELL_NO_SPELL:
    case SPELL_CANTRIP:
    case SPELL_BLINK_CLOSE:
        return false;

    default:
        // Everything else is probably some kind of attack, hopefully.
        return true;
    }
}

// Returns true if the monster has an ability that can affect the target
// anywhere in LOS_DEFAULT; i.e., even through glass.
bool mons_has_los_ability(monster_type mon_type)
{
    return mons_is_siren_beholder(mon_type)
           || mon_type == MONS_STARCURSED_MASS;
}

bool mons_has_ranged_spell(const monster& mon, bool attack_only,
                           bool ench_too)
{
    // Monsters may have spell-like abilities.
    if (mons_has_los_ability(mon.type))
        return true;

    for (const mon_spell_slot &slot : mon.spells)
    {
        if (_ms_ranged_spell(slot.spell, attack_only, ench_too)
            // Assume spells with no defined range are always effective at
            // range.
            && mons_spell_range(mon, slot.spell) != 1)
        {
            return true;
        }
    }

    return false;
}

static bool _mons_has_usable_ranged_weapon(const monster* mon)
{
    return mon->launcher() != nullptr;
}

static bool _mons_has_attack_wand(const monster& mon)
{
    const item_def *wand = mon.mslot_item(MSLOT_WAND);

    return wand && is_offensive_wand(*wand);
}

bool mons_has_ranged_attack(const monster& mon)
{
    return mons_has_ranged_spell(mon, true)
           || _mons_has_usable_ranged_weapon(&mon)
           || mon.missiles()
           || mon.reach_range() != REACH_NONE
           || _mons_has_attack_wand(mon);
}

bool mons_can_attack(const monster& mon)
{
    const actor* foe = mon.get_foe();
    if (!foe || !mon.can_see(*foe))
        return false;

    if (mons_has_ranged_attack(mon) && mon.see_cell_no_trans(foe->pos()))
        return true;

    return adjacent(mon.pos(), foe->pos());
}

/**
 * What gender are monsters of the given class?
 *
 * Used for pronoun selection.
 *
 * @param mc        The type of monster in question
 * @return          GENDER_NEUTER, _NEUTRAL, _FEMALE, or _MALE.
 */
gender_type mons_class_gender(monster_type mc)
{
    const bool female = mons_class_flag(mc, M_FEMALE);
    const bool male = mons_class_flag(mc, M_MALE);
    const bool neutral = mons_class_flag(mc, M_GENDER_NEUTRAL);
    ASSERT(male + female + neutral <= 1);
    return male ? GENDER_MALE :
         female ? GENDER_FEMALE :
        neutral ? GENDER_NEUTRAL :
                  GENDER_NEUTER;
}

// Use of variant (upper-/lowercase is irrelevant here):
// PRONOUN_SUBJECTIVE : _She_ is tap dancing.
// PRONOUN_POSSESSIVE : _Its_ sword explodes!
// PRONOUN_REFLEXIVE  : The wizard mumbles to _herself_.
// PRONOUN_OBJECTIVE  : You miss _him_.
const char *mons_pronoun(monster_type mon_type, pronoun_type variant,
                         bool visible)
{
    const gender_type gender = !visible ? GENDER_NEUTER
                                        : mons_class_gender(mon_type);
    return decline_pronoun(gender, variant);
}

// XXX: this is awful and should not exist
static const spell_type smitey_spells[] = {
    SPELL_SMITING,
    SPELL_AIRSTRIKE,
    SPELL_SYMBOL_OF_TORMENT,
    SPELL_CALL_DOWN_DAMNATION,
    SPELL_CALL_DOWN_LIGHTNING,
    SPELL_FIRE_STORM,
    SPELL_SHATTER,
    SPELL_POLAR_VORTEX,          // dubious
    SPELL_GLACIATE,         // dubious
    SPELL_OZOCUBUS_REFRIGERATION,
    SPELL_MASS_CONFUSION,
    SPELL_ENTROPIC_WEAVE,
    SPELL_GRAVE_CLAW,
};

/**
 * Does the given monster have spells that can damage without requiring LOF?
 *
 * Smitey (smite, airstrike), full-LOS (torment, refrigeration)...
 *
 * @param mon   The monster in question.
 * @return      Whether the given monster has 'smitey' effects.
 */
bool _mons_has_smite_attack(const monster* mons)
{
    return any_of(begin(smitey_spells), end(smitey_spells),
                  [=] (spell_type sp) { return mons->has_spell(sp); });
}

/**
 * Is the given monster smart and pushy enough to displace other
 * monsters?
 *
 * A shover should not cause damage to the shovee by
 * displacing it, so monsters that trail clouds of badness are
 * ineligible. The shover should also benefit from shoving, so monsters
 * that can smite/torment are ineligible.
 *
 * @param m     The monster in question.
 * @return      Whether that monster is ever allowed to 'push' other monsters.
 */
bool monster_shover(const monster& m)
{
    // Efreet and fire elementals are disqualified because they leave behind
    // clouds of flame. Curse toes are disqualified because they trail
    // clouds of miasma.
    if (mons_genus(m.type) == MONS_EFREET || m.type == MONS_FIRE_ELEMENTAL
        || m.type == MONS_CURSE_TOE)
    {
        return false;
    }

    // Monsters too stupid to use stairs (e.g. non-spectral zombified undead)
    // are also disqualified.
    // However, summons *can* push past pals & cause trouble.
    // XXX: redundant with intelligence check?
    if (!mons_can_use_stairs(m) && !m.is_summoned())
        return false;

    // Geryon really profits from *not* pushing past sin beasts.
    if (m.type == MONS_GERYON)
        return false;
    // Likewise, Robin and her mob.
    if (m.type == MONS_ROBIN)
        return false;

    // no mindless creatures pushing, aside from jellies, which just kind of ooze.
    return mons_intel(m) > I_BRAINLESS || mons_genus(m.type) == MONS_JELLY;
}

/**
 * Is the first monster considered 'senior' to the second; that is, can it
 * 'push' (swap with) the latter?
 *
 * Generally, this is true if m1 and m2 are related, and m1 is higher up the
 * totem pole than m2.
 *
 * Not guaranteed to be transitive or symmetric, though it probably should be.
 *
 * @param m1        The potentially senior monster.
 * @param m2        The potentially junior monster.
 * @param fleeing   Whether the first monster is running away; relevant for
 *                  smiters pushing melee monsters out of the way.
 * @return          Whether m1 can push m2.
 */
bool monster_senior(const monster& m1, const monster& m2, bool fleeing)
{
    // non-fleeing smiters won't push past anything.
    if (_mons_has_smite_attack(&m1) && !fleeing)
        return false;

    // Fannar's ice beasts can push past Fannar, who benefits from this.
    if (m1.type == MONS_ICE_BEAST && m2.type == MONS_FANNAR)
        return true;

    // Special-case spectral things to push past things that summon them
    // (revenants, ghost crabs).
    // XXX: unify this logic with Fannar's & Geryon's? (summon-buddies?)
    if (m1.type == MONS_SPECTRAL_THING
        && (m2.type == MONS_REVENANT_SOULMONGER || m2.type == MONS_GHOST_CRAB))
    {
        return true;
    }

    // Let warrior apostles push through other apostles, but (importantly)
    // NOT let wizards displace warriors to move into melee range
    if (m1.type == MONS_ORC_APOSTLE && m2.type == MONS_ORC_APOSTLE)
    {
        if (m1.props[APOSTLE_TYPE_KEY].get_int() == APOSTLE_WARRIOR)
        {
            return m2.props[APOSTLE_TYPE_KEY].get_int() != APOSTLE_WARRIOR
                   || m2.get_hit_dice() < m1.get_hit_dice();
        }
        if (m2.props[APOSTLE_TYPE_KEY].get_int() == APOSTLE_WARRIOR)
        {
            return m1.props[APOSTLE_TYPE_KEY].get_int() == APOSTLE_WARRIOR
                   && m2.get_hit_dice() < m1.get_hit_dice();
        }
    }

    // Band leaders can displace followers regardless of type considerations.
    if (m1.is_band_leader_of(m2))
        return true;
    // And prevent followers to displace the leader to avoid constant swapping.
    // -cao
    else if (m1.is_band_follower_of(m2))
        return false;

    // Monsters smart enough to use stairs can push past monsters too stupid
    // to use stairs (so that e.g. non-zombified or spectral zombified undead
    // can push past non-spectral zombified undead).
    if (mons_class_can_use_stairs(m1.type)
        && !mons_class_can_use_stairs(m2.type))
    {
        return true;
    }
    // This check assumes that demonicness is always carried at the monster type
    // level; this is because a full holiness check in such an often-called
    // function is costly.
    const bool related = mons_genus(m1.type) == mons_genus(m2.type)
                            || (   mons_class_holiness(m1.type) & MH_DEMONIC
                                && mons_class_holiness(m2.type) & MH_DEMONIC);

    // Let all related monsters (all demons are 'related') push past ones that
    // are weaker at all. Unrelated ones have to be quite a bit stronger, to
    // reduce excessive swapping and because HD correlates only weakly with
    // monster strength - but still give a small chance for slightly-higher
    // HD monsters to swap, to discourage ratscumming.
    const int hd1 = m1.get_hit_dice();
    const int hd2 = m2.get_hit_dice();
    return related && fleeing
           || related && hd1 > hd2
           || hd1 > hd2 + min(5, random2(11));
}

bool mons_class_can_pass(monster_type mc, const dungeon_feature_type grid)
{
    if (grid == DNGN_MALIGN_GATEWAY)
    {
        return mc == MONS_ELDRITCH_TENTACLE
               || mc == MONS_ELDRITCH_TENTACLE_SEGMENT;
    }

    return !feat_is_solid(grid);
}

static bool _mons_can_open_doors(const monster* mon)
{
    return mons_itemuse(*mon) >= MONUSE_OPEN_DOORS;
}

// Some functions that check whether a monster can open/eat/pass a
// given door. These all return false if there's no closed door there.
bool mons_can_open_door(const monster& mon, const coord_def& pos)
{
    if (!_mons_can_open_doors(&mon))
        return false;

    // Creatures allied with the player can't open doors.
    // (to prevent sabotaging the player accidentally.)
    //
    // Blood for Blood gets an exception since they filter in continuously over
    // time and otherwise get stuck behind doors in places like Vaults regularly.
    if (mon.friendly()
        && (!mons_is_blood_for_blood_orc(mon)
            || env.grid(pos) == DNGN_SEALED_DOOR ))
    {
        return false;
    }

    if (env.markers.property_at(pos, MAT_ANY, "door_restrict") == "veto")
        return false;

    return true;
}

bool mons_can_eat_door(const monster& mon, const coord_def& pos)
{
    if (env.markers.property_at(pos, MAT_ANY, "door_restrict") == "veto")
        return false;

    return mons_eats_items(mon)
           || mons_class_flag(mons_base_type(mon), M_EAT_DOORS);
}

bool mons_can_destroy_door(const monster& mon, const coord_def& pos)
{
    if (!mons_class_flag(mons_base_type(mon), M_CRASH_DOORS))
        return false;

    if (env.markers.property_at(pos, MAT_ANY, "door_restrict") == "veto")
        return false;

    return true;
}

static bool _mons_can_pass_door(const monster* mon, const coord_def& pos)
{
    return mon->can_pass_through_feat(DNGN_FLOOR)
           && (mons_can_open_door(*mon, pos)
               || mons_can_eat_door(*mon, pos)
               || mons_can_destroy_door(*mon, pos));
}

bool mons_can_traverse(const monster& mon, const coord_def& p,
                       bool only_in_sight, bool checktraps)
{
    // Friendly summons should avoid pathing out of the player's sight
    // (especially as attempting this may make them ignore valid, but longer
    // paths).
    if (only_in_sight && !you.see_cell_no_trans(p))
        return false;

    if (cell_is_runed(p))
        return false;

    // Includes sealed doors.
    if (feat_is_closed_door(env.grid(p)) && _mons_can_pass_door(&mon, p))
        return true;

    if (!mon.is_habitable(p))
        return false;

    return !checktraps || mon.is_trap_safe(p);
}

void mons_remove_from_grid(const monster& mon)
{
    const coord_def pos = mon.pos();
    if (map_bounds(pos) && env.mgrid(pos) == mon.mindex())
        env.mgrid(pos) = NON_MONSTER;
}

mon_inv_type equip_slot_to_mslot(equipment_slot eq)
{
    switch (eq)
    {
    case SLOT_WEAPON:       return MSLOT_WEAPON;
    case SLOT_BODY_ARMOUR:  return MSLOT_ARMOUR;
    case SLOT_OFFHAND:      return MSLOT_SHIELD;
    case SLOT_RING:
    case SLOT_AMULET:       return MSLOT_JEWELLERY;
    default: return NUM_MONSTER_SLOTS;
    }
}

mon_inv_type item_to_mslot(const item_def &item)
{
    switch (item.base_type)
    {
    case OBJ_WEAPONS:
    case OBJ_STAVES:
#if TAG_MAJOR_VERSION == 34
    case OBJ_RODS:
#endif
        return MSLOT_WEAPON;
    case OBJ_MISSILES:
        return MSLOT_MISSILE;
    case OBJ_ARMOUR:
        return equip_slot_to_mslot(get_armour_slot(item));
    case OBJ_JEWELLERY:
        return MSLOT_JEWELLERY;
    case OBJ_WANDS:
        return MSLOT_WAND;
    case OBJ_BOOKS:
    case OBJ_TALISMANS:
    case OBJ_MISCELLANY:
        return MSLOT_MISCELLANY;
    case OBJ_GOLD:
        return MSLOT_GOLD;
    default:
        return NUM_MONSTER_SLOTS;
    }
}

monster_type royal_jelly_ejectable_monster()
{
    return random_choose(MONS_ACID_BLOB,
                         MONS_AZURE_JELLY,
                         MONS_ROCKSLIME,
                         MONS_VOID_OOZE);
}

// Replaces @foe_god@ and @god_is@ with foe's god name.
//
// Atheists get "You"/"you", and worshippers of nameless gods get "Your
// god"/"your god".
static string _replace_god_name(god_type god, bool need_verb = false,
                                bool capital = false)
{
    string result;

    if (god == GOD_NO_GOD)
        result = capital ? "You" : "you";
    else if (god == GOD_NAMELESS)
        result = capital ? "Your god" : "your god";
    else
    {
        const string godname = god_name(god, false);
        result = capital ? uppercase_first(godname) : godname;
    }

    if (need_verb)
    {
        result += ' ';
        result += conjugate_verb("be", god == GOD_NO_GOD);
    }

    return result;
}

static string _random_class_of_god_name(bool (*class_of_god)(god_type god))
{
    string result;
    god_type some_god;

    do
    {
        some_god = random_god();
    }
    while (!class_of_god(some_god));

    const string godname = god_name(some_god, false);
    result = godname;

    return result;
}

// part_class should be a body_part_class_flags value.
string random_body_part_name(bool plural, int part_class)
{
    vector<bool> plural_parts;
    vector<string> body_parts;

    if (part_class & BPART_INTERNAL)
    {
        plural_parts.push_back(false);
        body_parts.push_back("soul");

        plural_parts.push_back(true);
        body_parts.push_back("muscles");

        if (you.has_blood())
        {
            plural_parts.push_back(false);
            body_parts.push_back("blood");
        }

        if (you.has_bones())
        {
            plural_parts.push_back(true);
            body_parts.push_back("bones");
        }
    }

    if (part_class & BPART_EXTERNAL)
    {
        plural_parts.push_back(false);
        body_parts.push_back("head");

        string hands;
        if (you.get_mutation_level(MUT_MISSING_HAND))
        {
            hands = you.hand_name(false);
            plural_parts.push_back(false);
        }
        else
        {
            hands = you.hand_name(true);
            plural_parts.push_back(true);
        }
        body_parts.push_back(hands);

        string arms = you.arm_name(true);
        plural_parts.push_back(true);
        body_parts.push_back(arms);

        if (player_has_feet())
        {
            string feet = you.foot_name(true);
            plural_parts.push_back(true);
            body_parts.push_back(feet);
        }

        if (you.has_tail())
        {
            plural_parts.push_back(false);
            body_parts.push_back("tail");
        }

        if (player_has_ears())
        {
            plural_parts.push_back(true);
            body_parts.push_back("ears");
        }

        string eyes;
        if (you.get_mutation_level(MUT_MISSING_EYE))
        {
            plural_parts.push_back(false);
            eyes = "eye";
        }
        else
        {
            plural_parts.push_back(true);
            eyes = "eyes";
        }
        body_parts.push_back(eyes);

        plural_parts.push_back(false);
        body_parts.push_back("mouth");

        if (player_has_hair())
        {
            plural_parts.push_back(false);
            body_parts.push_back("hair");
        }

        string flesh;
        if (you.petrified())
        {
            plural_parts.push_back(false);
            flesh = "stone";
        }
        else if (!get_form()->flesh_equivalent.empty())
        {
            plural_parts.push_back(false);
            flesh = get_form()->flesh_equivalent;
        }
        else
        {
            string skin = species::skin_name(you.species);
            // Check plurality as the species::skin_name() comment suggests.
            plural_parts.push_back(ends_with(skin, "s"));
            flesh = skin;
        }
        body_parts.push_back(flesh);
    }

    int which_part;

    do
    {
        which_part = random2(body_parts.size());
    }
    while (plural_parts[which_part] != plural);

    return body_parts[which_part];
}

static string _get_species_insult(const string &species, const string &type)
{
    string insult;
    string lookup;

    // Get species genus.
    if (!species.empty())
    {
        lookup  = "insult ";
        lookup += species;
        lookup += " ";
        lookup += type;

        insult  = getSpeakString(lowercase(lookup));
    }

    if (insult.empty()) // Species too specific?
    {
        lookup  = "insult general ";
        lookup += type;

        insult  = getSpeakString(lookup);
    }

    return insult;
}

// From should be of the form "prefix @tag@". Replaces all substrings
// of the form "prefix @tag@" with to, and all strings of the form
// "prefix @tag/alt@" with either to (if nonempty) or "prefix alt".
static string _replace_speech_tag(string msg, string from, const string &to)
{
    if (from.empty())
        return msg;
    msg = replace_all(msg, from, to);

    // Change the @ to a / for the next search
    from[from.size() - 1] = '/';

    // @tag/alternative@
    size_t pos = 0;
    while ((pos = msg.find(from, pos)) != string::npos)
    {
        // beginning of tag
        const size_t at_pos = msg.find('@', pos);
        // beginning of alternative
        const size_t alt_pos = pos + from.size();
        // end of tag (one-past-the-end of alternative)
        const size_t alt_end = msg.find('@', alt_pos);

        // unclosed @tag/alt, or "from" has no @: leave it alone.
        if (alt_end == string::npos || at_pos == string::npos)
            break;

        if (to.empty())
        {
            // Replace only the @...@ part.
            msg.replace(at_pos, alt_end - at_pos + 1,
                        msg.substr(alt_pos, alt_end - alt_pos));
            pos = at_pos + (alt_end - alt_pos);
        }
        else
        {
            // Replace the whole from string, up to the second @
            msg.replace(pos, alt_end - pos + 1, to);
            pos += to.size();
        }
    }
    return msg;
}

// Replaces the "@foo@" strings in monster shout and monster speak
// definitions.
string do_mon_str_replacements(const string &in_msg, const monster& mons,
                               int s_type)
{
    string msg = in_msg;

    const actor* foe = (mons.wont_attack()
                          && invalid_monster_index(mons.foe)) ?
                             &you : mons.get_foe();

    if (s_type < 0 || s_type >= NUM_LOUDNESS || s_type == NUM_SHOUTS)
        s_type = mons_shouts(mons.type);

    msg = maybe_pick_random_substring(msg);

    // FIXME: Handle player_genus in case it was not generalised to foe_genus.
    msg = replace_all(msg, "@a_player_genus@",
                article_a(species::name(you.species, species::SPNAME_GENUS)));
    msg = replace_all(msg, "@player_genus@",
                species::name(you.species, species::SPNAME_GENUS));
    msg = replace_all(msg, "@player_genus_plural@",
                pluralise(species::name(you.species, species::SPNAME_GENUS)));

    string foe_genus;

    if (foe == nullptr)
        ;
    else if (foe->is_player())
    {
        foe_genus = species::name(you.species, species::SPNAME_GENUS);

        msg = _replace_speech_tag(msg, " @to_foe@", "");
        msg = _replace_speech_tag(msg, " @at_foe@", "");

        msg = _replace_speech_tag(msg, " @to_foe@", "");
        msg = _replace_speech_tag(msg, " @at_foe@", "");

        msg = replace_all(msg, "@player_only@", "");
        msg = replace_all(msg, " @foe,@", ",");

        msg = replace_all(msg, "@player", "@foe");
        msg = replace_all(msg, "@Player", "@Foe");

        msg = replace_all(msg, "@foe_possessive@", "your");
        msg = replace_all(msg, "@foe@", "you");
        msg = replace_all(msg, "@Foe@", "You");

        msg = replace_all(msg, "@foe_name@", you.your_name);
        msg = replace_all(msg, "@foe_species@", species::name(you.species));
        msg = replace_all(msg, "@foe_genus@", foe_genus);
        msg = replace_all(msg, "@Foe_genus@", uppercase_first(foe_genus));
        msg = replace_all(msg, "@foe_genus_plural@",
                          pluralise(foe_genus));
    }
    else
    {
        string foe_name;
        const monster* m_foe = foe->as_monster();
        if (m_foe->attitude == ATT_FRIENDLY
            && !mons_is_unique(m_foe->type)
            && !crawl_state.game_is_arena())
        {
            foe_name = foe->name(DESC_YOUR);
            const string::size_type pos = foe_name.find("'");
            if (pos != string::npos)
                foe_name = foe_name.substr(0, pos);
        }
        else
            foe_name = foe->name(DESC_THE);

        string prep = "at";
        if (s_type == S_SILENT || s_type == S_SHOUT || s_type == S_NORMAL)
            prep = "to";
        msg = replace_all(msg, "@says@ @to_foe@", "@says@ " + prep + " @foe@");

        msg = _replace_speech_tag(msg, " @to_foe@", " to @foe@");
        msg = _replace_speech_tag(msg, " @at_foe@", " at @foe@");

        msg = replace_all(msg, "@foe,@", "@foe@,");
        msg = replace_all(msg, "@foe_possessive@", "@foe@'s");
        msg = replace_all(msg, "@foe@", foe_name);
        msg = replace_all(msg, "@Foe@", uppercase_first(foe_name));

        if (m_foe->is_named())
            msg = replace_all(msg, "@foe_name@", foe->name(DESC_PLAIN, true));

        string species = mons_type_name(mons_species(m_foe->type), DESC_PLAIN);

        msg = replace_all(msg, "@foe_species@", species);

        foe_genus = mons_type_name(mons_genus(m_foe->type), DESC_PLAIN);

        msg = replace_all(msg, "@foe_genus@", foe_genus);
        msg = replace_all(msg, "@Foe_genus@", uppercase_first(foe_genus));
        msg = replace_all(msg, "@foe_genus_plural@", pluralise(foe_genus));
    }

    description_level_type nocap = DESC_THE, cap = DESC_THE;

    if (mons.is_named() && you.can_see(mons))
    {
        const string name = mons.name(DESC_THE);

        msg = replace_all(msg, "@the_something@", name);
        msg = replace_all(msg, "@The_something@", name);
        msg = replace_all(msg, "@the_monster@",   name);
        msg = replace_all(msg, "@The_monster@",   name);

        msg = replace_all(msg, "@the_something_possessive@",
                          apostrophise(name));
        msg = replace_all(msg, "@The_something_possessive@",
                          apostrophise(name));
        msg = replace_all(msg, "@the_monster_possessive@",
                          apostrophise(name));
        msg = replace_all(msg, "@The_monster_possessive@",
                          apostrophise(name));
    }
    else if (mons.attitude == ATT_FRIENDLY
             && !mons_is_unique(mons.type)
             && !crawl_state.game_is_arena()
             && you.can_see(mons))
    {
        nocap = DESC_PLAIN;
        cap   = DESC_PLAIN;

        msg = replace_all(msg, "@the_something@", "your @the_something@");
        msg = replace_all(msg, "@The_something@", "Your @the_something@");
        msg = replace_all(msg, "@the_monster@",   "your @the_monster@");
        msg = replace_all(msg, "@The_monster@",   "Your @the_monster@");

        msg = replace_all(msg, "@the_something_possessive@",
                          "your @the_something_possessive@");
        msg = replace_all(msg, "@The_something_possessive@",
                          "Your @the_something_possessive@");
        msg = replace_all(msg, "@the_monster_possessive@",
                          "your @the_monster_possessive@");
        msg = replace_all(msg, "@The_monster_possessive@",
                          "Your @the_monster_possessive@");
    }

    // XXX: Shouldn't be able to see 'fake' monsters
    if (you.see_cell(mons.pos()) && mons.mid != MID_NOBODY)
    {
        dungeon_feature_type feat = env.grid(mons.pos());
        if (feat_is_solid(feat) || feat >= NUM_FEATURES)
            msg = replace_all(msg, "@surface@", "buggy surface");
        else if (feat == DNGN_LAVA)
            msg = replace_all(msg, "@surface@", "lava");
        else if (feat_is_water(feat))
            msg = replace_all(msg, "@surface@", "water");
        else if (feat_is_altar(feat))
            msg = replace_all(msg, "@surface@", "altar");
        else
            msg = replace_all(msg, "@surface@", "ground");

        msg = replace_all(msg, "@feature@", raw_feature_description(mons.pos()));
    }
    else
    {
        msg = replace_all(msg, "@surface@", "buggy unseen surface");
        msg = replace_all(msg, "@feature@", "buggy unseen feature");
    }

    string something = mons.name(DESC_PLAIN);
    msg = replace_all(msg, "@something@",   something);
    msg = replace_all(msg, "@a_something@", mons.name(DESC_A));
    msg = replace_all(msg, "@the_something@", mons.name(nocap));
    msg = replace_all(msg, "@the_something_possessive@",
                      apostrophise(mons.name(nocap)));

    something[0] = toupper_safe(something[0]);
    msg = replace_all(msg, "@Something@",   something);
    msg = replace_all(msg, "@A_something@", mons.name(DESC_A));
    msg = replace_all(msg, "@The_something@", mons.name(cap));
    msg = replace_all(msg, "@The_something_possessive@",
                      apostrophise(mons.name(cap)));

    // Player name.
    msg = replace_all(msg, "@player_name@", you.your_name);
    msg = replace_all(msg, "@player_name_possessive@",
                      apostrophise(you.your_name));

    string plain = mons.name(DESC_PLAIN);
    msg = replace_all(msg, "@monster@",     plain);
    msg = replace_all(msg, "@a_monster@",   mons.name(DESC_A));
    msg = replace_all(msg, "@the_monster@", mons.name(nocap));
    msg = replace_all(msg, "@the_monster_possessive@",
                      apostrophise(mons.name(nocap)));

    plain[0] = toupper_safe(plain[0]);
    msg = replace_all(msg, "@Monster@",     plain);
    msg = replace_all(msg, "@A_monster@",   mons.name(DESC_A));
    msg = replace_all(msg, "@The_monster@", mons.name(cap));
    msg = replace_all(msg, "@The_monster_possessive@",
                      apostrophise(mons.name(cap)));

    string subj_or_poss;

    subj_or_poss = mons.pronoun(PRONOUN_SUBJECTIVE);
    msg = replace_all(msg, "@subjective@", subj_or_poss);
    subj_or_poss[0] = toupper_safe(subj_or_poss[0]);
    msg = replace_all(msg, "@Subjective@", subj_or_poss);

    subj_or_poss = mons.pronoun(PRONOUN_POSSESSIVE);
    msg = replace_all(msg, "@possessive@", subj_or_poss);
    subj_or_poss[0] = toupper_safe(subj_or_poss[0]);
    msg = replace_all(msg, "@Possessive@", subj_or_poss);

    msg = replace_all(msg, "@reflexive@",
                      mons.pronoun(PRONOUN_REFLEXIVE));
    msg = replace_all(msg, "@objective@",
                      mons.pronoun(PRONOUN_OBJECTIVE));

    // Body parts.
    bool   can_plural = false;
    string part_str   = mons.hand_name(false, &can_plural);

    msg = replace_all(msg, "@hand@", part_str);
    msg = replace_all(msg, "@Hand@", uppercase_first(part_str));

    if (!can_plural)
        part_str = "NO PLURAL HANDS";
    else
        part_str = mons.hand_name(true);

    msg = replace_all(msg, "@hands@", part_str);
    msg = replace_all(msg, "@Hands@", uppercase_first(part_str));

    can_plural = false;
    part_str   = mons.arm_name(false, &can_plural);

    msg = replace_all(msg, "@arm@", part_str);
    msg = replace_all(msg, "@Arm@", uppercase_first(part_str));

    if (!can_plural)
        part_str = "NO PLURAL ARMS";
    else
        part_str = mons.arm_name(true);

    msg = replace_all(msg, "@arms@", part_str);
    msg = replace_all(msg, "@Arms@", uppercase_first(part_str));

    can_plural = false;
    part_str   = mons.foot_name(false, &can_plural);

    msg = replace_all(msg, "@foot@", part_str);
    msg = replace_all(msg, "@Foot@", uppercase_first(part_str));

    if (!can_plural)
        part_str = "NO PLURAL FEET";
    else
        part_str = mons.foot_name(true);

    msg = replace_all(msg, "@feet@", part_str);
    msg = replace_all(msg, "@Feet@", uppercase_first(part_str));

    if (foe != nullptr)
    {
        const god_type god = foe->deity();

        // Replace with "you are" for atheists.
        msg = replace_all(msg, "@god_is@",
                          _replace_god_name(god, true, false));
        msg = replace_all(msg, "@God_is@", _replace_god_name(god, true, true));

        // No verb needed.
        msg = replace_all(msg, "@foe_god@",
                          _replace_god_name(god, false, false));
        msg = replace_all(msg, "@Foe_god@",
                          _replace_god_name(god, false, true));
    }

    // The monster's god, not the player's. Atheists get
    // "NO GOD"/"NO GOD"/"NO_GOD"/"NO_GOD", and worshippers of nameless
    // gods get "a god"/"its god/my God/My God".
    //
    // XXX: Crawl currently has no first-person possessive pronoun;
    // if it gets one, it should be used for the last two entries.
    if (mons.god == GOD_NO_GOD)
    {
        msg = replace_all(msg, "@a_God@", "NO GOD");
        msg = replace_all(msg, "@A_God@", "NO GOD");
        msg = replace_all(msg, "@possessive_God@", "NO GOD");
        msg = replace_all(msg, "@Possessive_God@", "NO GOD");

        msg = replace_all(msg, "@my_God@", "NO GOD");
        msg = replace_all(msg, "@My_God@", "NO GOD");
    }
    else if (mons.god == GOD_NAMELESS)
    {
        msg = replace_all(msg, "@a_God@", "a god");
        msg = replace_all(msg, "@A_God@", "A god");
        const string possessive = mons.pronoun(PRONOUN_POSSESSIVE) + " god";
        msg = replace_all(msg, "@possessive_God@", possessive);
        msg = replace_all(msg, "@Possessive_God@", uppercase_first(possessive));

        msg = replace_all(msg, "@my_God@", "my God");
        msg = replace_all(msg, "@My_God@", "My God");
    }
    else
    {
        const string godname = god_name(mons.god);
        const string godcap = uppercase_first(godname);
        msg = replace_all(msg, "@a_God@", godname);
        msg = replace_all(msg, "@A_God@", godcap);
        msg = replace_all(msg, "@possessive_God@", godname);
        msg = replace_all(msg, "@Possessive_God@", godcap);

        msg = replace_all(msg, "@my_God@", godname);
        msg = replace_all(msg, "@My_God@", godcap);
    }

    if (msg.find("@random_god_") != string::npos)
    {
        msg = replace_all(msg, "@random_god_chaotic@",
                          _random_class_of_god_name(is_chaotic_god));
        msg = replace_all(msg, "@random_god_evil@",
                          _random_class_of_god_name(is_evil_god));
        msg = replace_all(msg, "@random_god_good@",
                          _random_class_of_god_name(is_good_god));
    }

    if (msg.find("@random_body_part") != string::npos)
    {
        msg = replace_all(msg, "@random_body_part_any_singular@",
                          random_body_part_name(false, BPART_ANY));
        msg = replace_all(msg, "@random_body_part_internal_singular@",
                          random_body_part_name(false, BPART_INTERNAL));
        msg = replace_all(msg, "@random_body_part_external_singular@",
                          random_body_part_name(false, BPART_EXTERNAL));
        msg = replace_all(msg, "@random_body_part_any_plural@",
                          random_body_part_name(true, BPART_ANY));
        msg = replace_all(msg, "@random_body_part_internal_plural@",
                          random_body_part_name(true, BPART_INTERNAL));
        msg = replace_all(msg, "@random_body_part_external_plural@",
                          random_body_part_name(true, BPART_EXTERNAL));
    }

    // Replace with species specific insults.
    if (msg.find("@species_insult_") != string::npos)
    {
        msg = replace_all(msg, "@species_insult_adj1@",
                               _get_species_insult(foe_genus, "adj1"));
        msg = replace_all(msg, "@species_insult_adj2@",
                               _get_species_insult(foe_genus, "adj2"));
        msg = replace_all(msg, "@species_insult_noun@",
                               _get_species_insult(foe_genus, "noun"));
    }

    static const char * sound_list[] =
    {
        "says",         // actually S_SILENT
        "shouts",
        "barks",
        "howls",
        "shouts",
        "roars",
        "screams",
        "bellows",
        "bleats",
        "trumpets",
        "screeches",
        "buzzes",
        "moans",
        "gurgles",
        "croaks",
        "growls",
        "hisses",
        "skitters",
        "skitters faintly",
        "sneers",       // S_DEMON_TAUNT
        "says",         // S_CHERUB -- they just speak normally.
        "squeals",
        "roars",
        "rustles",      // dubious
        "squeaks",
        "buggily says", // NUM_SHOUTS
        "breathes",     // S_VERY_SOFT
        "whispers",     // S_SOFT
        "says",         // S_NORMAL
        "shouts",       // S_LOUD
        "screams",      // S_VERY_LOUD
    };
    COMPILE_CHECK(ARRAYSZ(sound_list) == NUM_LOUDNESS);

    if (s_type < 0 || s_type >= NUM_LOUDNESS || s_type == NUM_SHOUTS)
    {
        mprf(MSGCH_DIAGNOSTICS, "Invalid @says@ type.");
        msg = replace_all(msg, "@says@", "buggily says");
    }
    else
        msg = replace_all(msg, "@says@", sound_list[s_type]);

    msg = maybe_capitalise_substring(msg);

    return msg;
}

/**
 * Get the monster body shape of the given monster.
 * @param mon  The monster in question.
 * @return     The mon_body_shape type of this monster.
 */
mon_body_shape get_mon_shape(const monster& mon)
{
    monster_type base_type;
    if (mons_is_pghost(mon.type))
        base_type = species::to_mons_species(mon.ghost->species);
    else if (mons_is_zombified(mon))
        base_type = mon.base_monster;
    else
        base_type = mon.type;
    return get_mon_shape(base_type);
}

/**
 * Get the monster body shape of the given monster type.
 * @param mc  The monster type in question.
 * @return     The mon_body_shape type of this monster type.
 */
mon_body_shape get_mon_shape(const monster_type mc)
{
    if (mc == MONS_CHAOS_SPAWN)
    {
        return static_cast<mon_body_shape>(random_range(MON_SHAPE_HUMANOID,
                                                        MON_SHAPE_MISC));
    }

    ASSERT_smc();
    return smc->shape;
}

/**
 * What's the normal tile for a given monster type?
 *
 * @param mc    The monster type in question.
 * @return      The tile for that monster, or TILEP_MONS_PROGRAM_BUG for mons
 *              with variable tiles (e.g. merfolk, hydras, slime creatures).
 */
tileidx_t get_mon_base_tile(monster_type mc)
{
    ASSERT_smc();
    return smc->tile.base;
}

/**
 * How should a given monster type's tile vary?
 *
 * @param mc    The monster type in question.
 * @return      An enum describing how display of the monster should vary
 *              (by individual monster instance, or whether they're in water,
 *              etc)
 */
mon_type_tile_variation get_mon_tile_variation(monster_type mc)
{
    ASSERT_smc();
    return smc->tile.variation;
}

/**
 * What's the normal tile for corpses of a given monster type?
 *
 * @param mc    The monster type in question.
 * @return      The tile for that monster's corpse; may be varied slightly
 *              further in some special cases (ugly things, klowns).
 */
tileidx_t get_mon_base_corpse_tile(monster_type mc)
{
    ASSERT_smc();
    return smc->corpse_tile;
}


/**
 * Get a DB lookup string for the given monster body shape.
 * @param mon  The monster body shape type in question.
 * @return     A DB lookup string for the monster body shape.
 */
string get_mon_shape_str(const mon_body_shape shape)
{
    ASSERT_RANGE(shape, MON_SHAPE_HUMANOID, MON_SHAPE_MISC + 1);

    static const char *shape_names[] =
    {
        "bug", "humanoid", "winged humanoid", "tailed humanoid",
        "winged tailed humanoid", "centaur", "naga",
        "quadruped", "tailless quadruped", "winged quadruped",
        "bat", "bird", "snake", "fish",  "insect", "winged insect",
        "arachnid", "centipede", "snail", "plant", "fungus", "orb",
        "blob", "misc"
    };

    COMPILE_CHECK(ARRAYSZ(shape_names) == MON_SHAPE_MISC + 1);
    return shape_names[shape];
}

/** Is this body shape partially humanoid (i.e. does it at least have a
 *  humanoid upper body)?
 *  @param shape  The body shape in question.
 *  @returns      Whether this body shape is partially humanoid.
 */
bool mon_shape_is_humanoid(mon_body_shape shape)
{
    return shape >= MON_SHAPE_FIRST_HUMANOID
           && shape <= MON_SHAPE_LAST_HUMANOID;
}

bool player_or_mon_in_sanct(const monster& mons)
{
    return is_sanctuary(you.pos()) || is_sanctuary(mons.pos());
}

int get_dist_to_nearest_monster()
{
    int minRange = LOS_RADIUS + 1;
    for (radius_iterator ri(you.pos(), LOS_NO_TRANS, true); ri; ++ri)
    {
        const monster* mon = monster_at(*ri);
        if (mon == nullptr)
            continue;

        if (!mon->visible_to(&you))
            continue;

        // Plants/fungi don't count.
        if ((!mons_is_threatening(*mon) || mon->wont_attack())
            && !mons_class_is_test(mon->type))
        {
            continue;
        }

        int dist = grid_distance(you.pos(), *ri);
        if (dist < minRange)
            minRange = dist;
    }
    return minRange;
}

bool monster_nearby()
{
    for (radius_iterator ri(you.pos(), LOS_DEFAULT); ri; ++ri)
        if (monster_at(*ri))
            return true;
    return false;
}

actor *actor_by_mid(mid_t m, bool require_valid)
{
    if (m == MID_PLAYER)
        return &you;
    return monster_by_mid(m, require_valid);
}

monster *monster_by_mid(mid_t m, bool require_valid)
{
    if (!require_valid)
    {
        if (m == MID_ANON_FRIEND)
            return &env.mons[ANON_FRIENDLY_MONSTER];
        if (m == MID_YOU_FAULTLESS)
            return &env.mons[YOU_FAULTLESS];
    }

    if (unsigned short *mc = map_find(env.mid_cache, m))
        return &env.mons[*mc];
    return 0;
}

monster *cached_monster_copy_by_mid(mid_t m)
{
    for (size_t i = 0; i < env.final_effect_monster_cache.size(); ++i)
    {
        if (env.final_effect_monster_cache[i].mid == m)
            return &env.final_effect_monster_cache[i];
    }

    return nullptr;
}

void init_anon()
{
    monster &mon = env.mons[ANON_FRIENDLY_MONSTER];
    mon.reset();
    mon.type = MONS_PROGRAM_BUG;
    mon.mid = MID_ANON_FRIEND;
    mon.attitude = ATT_FRIENDLY;
    mon.hit_points = mon.max_hit_points = 1000;

    monster &yf = env.mons[YOU_FAULTLESS];
    yf.reset();
    yf.type = MONS_PROGRAM_BUG;
    yf.mid = MID_YOU_FAULTLESS;
    yf.attitude = ATT_FRIENDLY; // higher than this, actually
    yf.hit_points = mon.max_hit_points = 1000;
}

actor *find_agent(mid_t m, kill_category kc)
{
    actor *agent = actor_by_mid(m);
    if (agent)
        return agent;
    switch (kc)
    {
    case KC_YOU:
        // shouldn't happen, there ought to be a valid mid
        return &you;
    case KC_FRIENDLY:
        return &env.mons[ANON_FRIENDLY_MONSTER];
    case KC_OTHER:
        // currently hostile dead/gone monsters are no different from env
        return 0;
    case KC_NCATEGORIES:
    default:
        die("invalid kill category");
    }
}

const char* mons_class_name(monster_type mc)
{
    // Usually, all invalids return "program bug", but since it has value of 0,
    // it's good to tell them apart in error messages.
    if (invalid_monster_type(mc) && mc != MONS_PROGRAM_BUG)
        return "INVALID";

    return get_monster_data(mc)->name;
}

mon_threat_level_type mons_threat_level(const monster &mon, bool real)
{
    const monster& threat = get_tentacle_head(mon);
    if (threat.props.exists(KIKU_WRETCH_KEY))
        return MTHRT_TRIVIAL; // ignores 'real', sorry...

    const double factor = sqrt(exp_needed(you.experience_level) / 30.0);
    const int tension = exper_value(threat, real, true) / (1 + factor);

    if (tension <= 0)
    {
        // Conjurators use melee to conserve mana, MDFis switch plates...
        return MTHRT_TRIVIAL;
    }
    else if (tension <= 5)
    {
        // An easy fight but not ignorable.
        return MTHRT_EASY;
    }
    else if (tension <= 32)
    {
        // Hard but reasonable.
        return MTHRT_TOUGH;
    }
    else
    {
        // Check all wands/jewels several times, wear brown pants...
        return MTHRT_NASTY;
    }
}

bool mons_foe_is_marked(const monster& mon)
{
    if (mon.foe == MHITYOU)
    {
        return (you.duration[DUR_SENTINEL_MARK]
                || testbits(mon.flags, MF_APOSTLE_BAND))
               && in_bounds(you.pos());
    }
    else
        return false;
}

void debug_mondata()
{
    string fails;

    for (monster_type mc = MONS_0; mc < NUM_MONSTERS; ++mc)
    {
        if (invalid_monster_type(mc))
            continue;
        const char* name = mons_class_name(mc);
        if (!name)
        {
            fails += make_stringf("Monster %d has no name\n", mc);
            continue;
        }

        const monsterentry *md = get_monster_data(mc);

        int WL = md->willpower;
        if (WL < 0)
            WL = md->HD * -WL * 4 / 3;
        if (md->willpower > 200 && md->willpower != WILL_INVULN)
            fails += make_stringf("%s has WL %d > 200\n", name, WL);
        if (get_resist(md->resists, MR_RES_POISON) == 2)
            fails += make_stringf("%s has rPois++\n", name);
        if (get_resist(md->resists, MR_RES_ELEC) == 2)
            fails += make_stringf("%s has rElec++\n", name);

        // Tests below apply only to real monsters.
        if (md->bitfields & M_CANT_SPAWN)
            continue;

        if (!md->HD && md->basechar != 'Z') // derived undead...
            fails += make_stringf("%s has 0 HD: %d\n", name, md->HD);
        if (md->avg_hp_10x <= 0 && md->basechar != 'Z')
            fails += make_stringf("%s has <= 0 HP: %d", name, md->avg_hp_10x);

        if (md->basechar == ' ')
            fails += make_stringf("%s has an empty glyph\n", name);

        if (md->AC < 0 && !mons_is_draconian_job(mc))
            fails += make_stringf("%s has negative AC\n", name);
        if (md->ev < 0 && !mons_is_draconian_job(mc))
            fails += make_stringf("%s has negative EV\n", name);
        if (md->exp_mod < 0)
            fails += make_stringf("%s has negative xp mod\n", name);

        if (md->speed < 0)
            fails += make_stringf("%s has 0 speed\n", name);
        else if (md->speed == 0 && !mons_class_is_firewood(mc))
            fails += make_stringf("%s has 0 speed\n", name);

        const bool male = mons_class_flag(mc, M_MALE);
        const bool female = mons_class_flag(mc, M_FEMALE);
        const bool neutral = mons_class_flag(mc, M_GENDER_NEUTRAL);
        if (male + female + neutral > 1)
            fails += make_stringf("%s has too many genders\n", name);

        if (md->shape == MON_SHAPE_BUGGY)
            fails += make_stringf("%s has no defined shape\n", name);

        const bool has_corpse_tile = md->corpse_tile
                                     && md->corpse_tile != TILE_ERROR;
        if (md->species != mc)
        {
            if (has_corpse_tile)
            {
                fails +=
                    make_stringf("%s isn't a species but has a corpse tile\n",
                                 name);
            }
        }
        else if (!md->leaves_corpse)
        {
            if (has_corpse_tile)
            {
                fails += make_stringf("%s has a corpse tile & no corpse\n",
                                      name);
            }
        }
        else if (!has_corpse_tile)
            fails += make_stringf("%s has a corpse but no corpse tile\n", name);
    }

    dump_test_fails(fails, "mon-data");
}

/**
 * Iterate over mspell_list (mon-spell.h) and look for anything that seems
 * incorrect. Dump the output to a text file & print its location to the
 * console.
 */
void debug_monspells()
{
    string fails;

    // first, build a map from spellbooks to the first monster that uses them
    // (zero-initialised, where 0 == MONS_PROGRAM_BUG).
    monster_type mon_book_map[NUM_MSTYPES] = { };
    for (monster_type mc = MONS_0; mc < NUM_MONSTERS; ++mc)
    {
        if (!invalid_monster_type(mc))
        {
            mon_spellbook_type mon_book = _get_mc_spellbook(mc);
            if (mon_book < ARRAYSZ(mon_book_map) && !mon_book_map[mon_book])
                mon_book_map[mon_book] = mc;
        }
    }

    // then, check every spellbook for errors.

    for (const mon_spellbook &spbook : mspell_list)
    {
        string book_name;
        const monster_type sample_mons = mon_book_map[spbook.type];
        if (!sample_mons)
        {
            string spells;
            if (spbook.spells.empty())
                spells = "no spells";
            else
                for (const mon_spell_slot &spslot : spbook.spells)
                    if (is_valid_spell(spslot.spell))
                        spells += make_stringf(",%s", spell_title(spslot.spell));

            fails += make_stringf("Book #%d is unused (%s)\n", spbook.type,
                                  spells.c_str());
            book_name = make_stringf("#%d", spbook.type);
        }
        else
        {
            const char * const mons_name = get_monster_data(sample_mons)->name;
            book_name = mons_name;
        }

        const char * const bknm = book_name.c_str();

        if (!spbook.spells.size())
            fails += make_stringf("Empty book %s\n", bknm);

        for (const mon_spell_slot &spslot : spbook.spells)
        {
            string spell_name;
            if (!is_valid_spell(spslot.spell))
            {
                fails += make_stringf("Book %s contains invalid spell %d\n",
                                      bknm, spslot.spell);
                spell_name = to_string(spslot.spell);
            }
            else
                spell_name = spell_title(spslot.spell);

            // TODO: export this value somewhere
            const int max_freq = 200;
            if (spslot.freq > max_freq)
            {
                fails += make_stringf("Spellbook %s has spell %s at freq %d "
                                      "(greater than max freq %d)\n",
                                      bknm, spell_name.c_str(),
                                      spslot.freq, max_freq);
            }

            mon_spell_slot_flag category = MON_SPELL_NO_FLAGS;
            for (const auto flag : mon_spell_slot_flags::range())
            {
                if (!(spslot.flags & flag))
                    continue;

                if (flag >= MON_SPELL_FIRST_CATEGORY
                    && flag <= MON_SPELL_LAST_CATEGORY)
                {
                    if (category == MON_SPELL_NO_FLAGS)
                        category = flag;
                    else
                    {
                        fails += make_stringf("Spellbook %s has spell %s in "
                                              "multiple categories (%d and %d)\n",
                                              bknm, spell_name.c_str(),
                                              category, flag);
                    }
                }

                COMPILE_CHECK(MON_SPELL_NOISY > MON_SPELL_LAST_CATEGORY);
                if (flag == MON_SPELL_NOISY
                    && category && !(category & MON_SPELL_INNATE_MASK))
                {
                    fails += make_stringf("Spellbook %s has spell %s marked "
                                          "MON_SPELL_NOISY redundantly\n",
                                          bknm, spell_name.c_str());
                }
            }

            if (category == MON_SPELL_NO_FLAGS)
            {
                fails += make_stringf("Spellbook %s has spell %s with no "
                                      "category\n", bknm, spell_name.c_str());
            }
        }
    }

    dump_test_fails(fails, "mon-spell");
}

// Used when clearing level data, to ensure any additional reset quirks
// are handled properly.
void reset_all_monsters()
{
    for (auto &mons : menv_real)
    {
        // The monsters here have already been saved or discarded, so this
        // is the only place when a constricting monster can legitimately
        // be reset. Thus, clear constriction manually.
        if (!invalid_monster(&mons))
        {
            delete mons.constricting;
            mons.constricting = nullptr;
        }
        mons.reset();
    }

    env.mid_cache.clear();
}

bool mons_is_recallable(const actor* caller, const monster& targ)
{
    // For player, only recall friendly monsters
    if (caller == &you)
    {
        if (!targ.friendly())
            return false;
    }
    // Monster recall requires same attitude and at least normal intelligence
    else if (mons_intel(targ) < I_HUMAN
             || (!caller && targ.friendly())
             || (caller && !mons_aligned(&targ, caller->as_monster()))
             || targ.type == MONS_PLAYER_GHOST)
    {
        return false;
    }

    return targ.alive()
           && !mons_class_is_stationary(targ.type)
           && !targ.is_peripheral()
           && mons_class_is_threatening(targ.type);
}

bool mons_stores_tracking_data(const monster& mons)
{
    return mons.type == MONS_THORN_HUNTER
           || mons.type == MONS_MERFOLK_AVATAR
           || mons.type == MONS_BOULDER_BEETLE;
}

bool mons_is_beast(monster_type mc)
{
    if (!(mons_class_holiness(mc) & MH_NATURAL)
        || mons_class_intel(mc) != I_ANIMAL)
    {
        return false;
    }
    else if (mons_genus(mc) == MONS_UGLY_THING
             || mc == MONS_ICE_BEAST
             || mc == MONS_SKY_BEAST
             || mc == MONS_BUTTERFLY)
    {
        return false;
    }
    else
        return true;
}

bool mons_is_avatar(monster_type mc)
{
    return mons_class_flag(mc, M_AVATAR);
}

bool mons_is_wrath_avatar(const monster &mon)
{
    return mon.type == MONS_GOD_WRATH_AVATAR;
}

bool mons_is_player_shadow(const monster& mon)
{
    return mon.type == MONS_PLAYER_SHADOW
        && mon.attitude == ATT_FRIENDLY; // hostile shadows are god wrath
}

// Zero-damage attacks with special effects (constriction, drowning, pure fire,
// etc.) aren't counted by default, since this is used to decide whether the
// monster can go berserk or be weakened, both of which require an attack with
// non-zero base damage.
bool mons_has_attacks(const monster& mon, bool allow_damageless)
{
    const mon_attack_def attk = mons_attack_spec(mon, 0);
    return attk.type != AT_NONE
           && (attk.type != AT_WEAP_ONLY || mon.weapon(0))
           && (allow_damageless
               || attk.damage > 0
               || (attk.type == AT_WEAP_ONLY && mon.weapon(0)));
}

// The default suitable() function for choose_random_nearby_monster().
bool choose_any_monster(const monster& mon)
{
    return !mons_is_projectile(mon.type);
}

// Pick a random monster within line of sight of the player which matches a
// given criteria and return it.
//
// suitable() should return true for any monsters we want to be eligable.
monster* choose_random_nearby_monster(bool (*suitable)(const monster& mon))
{
    int weight = 0;
    monster* chosen = nullptr;
    for (radius_iterator ri(you.pos(), LOS_NO_TRANS); ri; ++ri)
    {
        monster* mon = monster_at(*ri);
        if (!mon || !suitable(*mon))
            continue;

        if (one_chance_in(++weight))
            chosen = mon;
    }

    return chosen;
}

// Pick a random monster on the current floor which matches a given criteria and
// return it.
//
// suitable() should return true for any monsters we want to be eligable.
monster* choose_random_monster_on_level(bool (*suitable)(const monster& mon))
{
    int weight = 0;
    monster* chosen = nullptr;
    for (monster_iterator mi; mi; ++mi)
    {
        if (!suitable(**mi))
            continue;

        if (one_chance_in(++weight))
            chosen = *mi;
    }

    return chosen;
}

int spell_freq_for_hd(int hd)
{
    return hd + 50;
}

void normalize_spell_freq(monster_spells &spells, int total_freq)
{
    // Not using std::accumulate because slot.freq is only a uint8_t.
    unsigned int total_given_freq = 0;
    for (const auto &slot : spells)
        total_given_freq += slot.freq;

    if (total_given_freq > 0)
    {
        for (auto &slot : spells)
            slot.freq = total_freq * slot.freq / total_given_freq;
    }
    else
    {
        const size_t numspells = spells.size();
        for (auto &slot : spells)
            slot.freq = total_freq / numspells;
    }
}

/// Rounded to player-visible approximations, how hurt is this monster?
mon_dam_level_type mons_get_damage_level(const monster& mons)
{
    if (mons.hit_points <= mons.max_hit_points / 5)
        return MDAM_ALMOST_DEAD;
    else if (mons.hit_points <= mons.max_hit_points * 2 / 5)
        return MDAM_SEVERELY_DAMAGED;
    else if (mons.hit_points <= mons.max_hit_points * 3 / 5)
        return MDAM_HEAVILY_DAMAGED;
    else if (mons.hit_points <= mons.max_hit_points * 4 / 5)
        return MDAM_MODERATELY_DAMAGED;
    else if (mons.hit_points < mons.max_hit_points)
        return MDAM_LIGHTLY_DAMAGED;
    else
        return MDAM_OKAY;
}

string get_damage_level_string(mon_holy_type holi, mon_dam_level_type mdam)
{
    ostringstream ss;
    switch (mdam)
    {
    case MDAM_ALMOST_DEAD:
        ss << "almost";
        ss << (wounded_damaged(holi) ? " destroyed" : " dead");
        return ss.str();
    case MDAM_SEVERELY_DAMAGED:
        ss << "severely";
        break;
    case MDAM_HEAVILY_DAMAGED:
        ss << "heavily";
        break;
    case MDAM_MODERATELY_DAMAGED:
        ss << "moderately";
        break;
    case MDAM_LIGHTLY_DAMAGED:
        ss << "lightly";
        break;
    case MDAM_OKAY:
    default:
        ss << "not";
        break;
    }
    ss << (wounded_damaged(holi) ? " damaged" : " wounded");
    return ss.str();
}

void print_wounds(const monster& mons)
{
    if (!mons.alive() || mons.hit_points == mons.max_hit_points)
        return;

    mon_dam_level_type dam_level = mons_get_damage_level(mons);
    string desc = get_damage_level_string(mons.holiness(), dam_level);

    desc.insert(0, " is ");
    desc += ".";
    simple_monster_message(mons, desc.c_str(), false, MSGCH_MONSTER_DAMAGE,
                           dam_level);
}

// (true == 'damaged') [constructs, undead, etc.]
// and (false == 'wounded') [living creatures, etc.] {dlb}
bool wounded_damaged(mon_holy_type holi)
{
    // this schema needs to be abstracted into real categories {dlb}:
    return bool(holi & (MH_UNDEAD | MH_NONLIVING | MH_PLANT));
}

// Is this monster interesting enough to make notes about?
bool mons_is_notable(const monster& mons)
{
    if (crawl_state.game_is_arena())
        return false;

    // (Ex-)Unique monsters are always interesting
    if (mons_is_or_was_unique(mons))
        return true;
    // If it's never going to attack us, then not interesting
    if (mons.friendly())
        return false;
    // tentacles aren't real monsters.
    if (mons_is_tentacle_or_tentacle_segment(mons.type))
        return false;
    // Hostile ghosts and illusions are always interesting.
    if (mons.type == MONS_PLAYER_GHOST
        || mons.type == MONS_PLAYER_ILLUSION
        || mons.type == MONS_PANDEMONIUM_LORD)
    {
        return true;
    }
    // Jellies are never interesting to Jiyva.
    if (mons.type == MONS_JELLY && have_passive(passive_t::jellies_army))
        return false;
    if (mons_threat_level(mons) == MTHRT_NASTY)
        return true;
    const auto &nm = Options.note_monsters;
    // Don't waste time on moname() if user isn't using this option
    if (!nm.empty())
    {
        const string iname = mons_type_name(mons.type, DESC_A);
        return any_of(begin(nm), end(nm), [&](const text_pattern &pat) -> bool
                                          { return pat.matches(iname); });
    }

    return false;
}

/**
 * Set up fields for mutant beasts that vary by tier & facets (that is, that
 * vary between individual beasts).
 *
 * @param mons          The beast to be initialized.
 * @param HD            The beast's HD. If 0, default to mon-data's version.
 * @param beast_facets  The beast's facets (e.g. fire, bat).
 *                      If empty, chooses two distinct facets at random.
 */
void init_mutant_beast(monster &mons, short HD, vector<int> beast_facets)
{
    if (!HD)
        HD = mons.get_experience_level();

    // MUTANT_BEAST_TIER is to make it visible for mon-info
    mons.props[MUTANT_BEAST_TIER] = HD;

    if (beast_facets.empty())
    {
        vector<int> available_facets;
        for (int f = BF_FIRST; f <= BF_LAST; ++f)
            available_facets.insert(available_facets.end(), f);

        ASSERT(available_facets.size() >= 2);

        shuffle_array(available_facets);
        beast_facets.push_back(available_facets[0]);
        beast_facets.push_back(available_facets[1]);

        ASSERT(beast_facets.size() == 2);
        ASSERT(beast_facets[0] != beast_facets[1]);
    }

    for (auto facet : beast_facets)
    {
        mons.props[MUTANT_BEAST_FACETS].get_vector().push_back(facet);

        switch (facet)
        {
            case BF_BAT:
                mons.props[MON_SPEED_KEY] = mons.speed * 2;
                mons.calc_speed();
                break;
            case BF_FIRE:
                mons.spells.emplace_back(SPELL_FIRE_BREATH, 60,
                                         MON_SPELL_NATURAL | MON_SPELL_BREATH);
                mons.props[CUSTOM_SPELLS_KEY] = true;
                break;
            case BF_SHOCK:
                mons.spells.emplace_back(SPELL_BLINKBOLT, 60,
                                         MON_SPELL_NATURAL);
                mons.props[CUSTOM_SPELLS_KEY] = true;
                break;
            default:
                break;
        }
    }
}

/**
 * If a monster needs to charge up to cast a spell (by 'casting' the spell
 * repeatedly), how many charges does it need until it can actually set the
 * spell off?
 *
 * @param m     The type of monster in question.
 * @return      The required number of charges.
 */
int max_mons_charge(monster_type m)
{
    switch (m)
    {
        case MONS_ORB_SPIDER:
            return 1;
        default:
            return 0;
    }
}

// Deal out damage to nearby pain-bonded monsters based on the distance between them.
void radiate_pain_bond(const monster& mon, int damage, const monster* original_target)
{
    for (actor_near_iterator ai(mon.pos(), LOS_NO_TRANS); ai; ++ai)
    {
        if (!ai->is_monster())
            continue;

        monster* target = ai->as_monster();

        if (&mon == target) // no self-sharing
            continue;

        // Only other pain-bonded monsters are affected.
        if (!target->has_ench(ENCH_PAIN_BOND))
            continue;

        int distance = target->pos().distance_from(mon.pos());
        if (distance > 3)
            continue;

        damage = max(0, div_rand_round(damage * (4 - distance), 5));

        if (damage > 0)
        {
            behaviour_event(target, ME_ANNOY, &you, you.pos());

            // save any potential cleanup of the original target for later
            // (in `monster::hurt`).
            if (target == original_target)
                damage = target->hurt(&you, damage, BEAM_SHARED_PAIN, KILLED_BY_MONSTER, "", "", false);
            else
                damage = target->hurt(&you, damage, BEAM_SHARED_PAIN);

            if (damage > 0)
                radiate_pain_bond(*target, damage, original_target);
        }
    }
}

// When a monster explodes violently, add some spice
void throw_monster_bits(const monster& mon)
{
    for (actor_near_iterator ai(mon.pos(), LOS_NO_TRANS); ai; ++ai)
    {
        if (!ai->is_monster())
            continue;

        monster* target = ai->as_monster();

        if (&mon == target) // can't throw chunks of something at itself.
            continue;

        int distance = target->pos().distance_from(mon.pos());
        if (!one_chance_in(distance + 4)) // generally gonna miss
            continue;

        int damage = 1 + random2(mon.get_hit_dice());

        mprf("%s is hit by a flying piece of %s!",
                target->name(DESC_THE, false).c_str(),
                mon.name(DESC_THE, false).c_str());

        // Because someone will get a kick out of this some day.
        if (mons_class_flag(mons_base_type(mon), M_ACID_SPLASH))
            target->corrode(&you, "a flying bit");

        behaviour_event(target, ME_ANNOY, &you, you.pos());
        target->hurt(&you, damage);
    }
}

/// Add an ancestor spell to the given list.
static void _add_ancestor_spell(monster_spells &spells, spell_type spell)
{
    spells.emplace_back(spell, 25, MON_SPELL_WIZARD);
}

/**
 * Set the correct spells for a given ancestor, corresponding to their HD and
 * type.
 *
 * @param ancestor      The ancestor in question.
 * @param notify        Whether to print messages if anything changes.
 */
void set_ancestor_spells(monster &ancestor, bool notify)
{
    ASSERT(mons_is_hepliaklqana_ancestor(ancestor.type));

    vector<spell_type> old_spells;
    for (auto spellslot : ancestor.spells)
        old_spells.emplace_back(spellslot.spell);

    ancestor.spells = {};
    const int HD = ancestor.get_experience_level();
    switch (ancestor.type)
    {
    case MONS_ANCESTOR_BATTLEMAGE:
        _add_ancestor_spell(ancestor.spells, HD >= 10 ?
                                             SPELL_BOLT_OF_MAGMA :
                                             SPELL_THROW_FROST);
        _add_ancestor_spell(ancestor.spells, HD >= 16 ?
                                             SPELL_LEHUDIBS_CRYSTAL_SPEAR :
                                             SPELL_STONE_ARROW);
        break;
    case MONS_ANCESTOR_HEXER:
        _add_ancestor_spell(ancestor.spells, HD >= 10 ? SPELL_PARALYSE
                                                      : SPELL_SLOW);
        _add_ancestor_spell(ancestor.spells, HD >= 13 ? SPELL_MASS_CONFUSION
                                                      : SPELL_CONFUSE);
        break;
    default:
        break;
    }

    if (HD >= 13)
        ancestor.spells.emplace_back(SPELL_HASTE, 25, MON_SPELL_WIZARD);

    if (ancestor.spells.size())
        ancestor.props[CUSTOM_SPELLS_KEY] = true;

    if (!notify)
        return;

    for (auto spellslot : ancestor.spells)
    {
        if (find(old_spells.begin(), old_spells.end(), spellslot.spell)
            == old_spells.end())
        {
            mprf("%s regains %s memory of %s.",
                 ancestor.name(DESC_YOUR, true).c_str(),
                 ancestor.pronoun(PRONOUN_POSSESSIVE, true).c_str(),
                 spell_title(spellslot.spell));
        }
    }
}

static bool _apply_to_monsters(monster_func f, radius_iterator&& ri)
{
    bool affected_any = false;
    for (; ri; ri++)
    {
        monster* mons = monster_at(*ri);
        if (!invalid_monster(mons))
            affected_any = f(*mons) || affected_any;
    }

    return affected_any;
}

bool apply_monsters_around_square(monster_func f, const coord_def& where,
                                  int radius)
{
    return _apply_to_monsters(f, radius_iterator(where, radius, C_SQUARE,
                                                 LOS_NO_TRANS, true));
}

bool apply_visible_monsters(monster_func f, const coord_def& where, los_type los)
{
    return _apply_to_monsters(f, radius_iterator(where, los, true));
}

int touch_of_beogh_hp_mult(const monster& mon)
{
    const int pow = mon.props.exists(APOSTLE_POWER_KEY)
                    ? mon.props[APOSTLE_POWER_KEY].get_int() : 0;

    return 100 + (min(50, pow * 2 / 3));
}

/**
 * Should all actions beams originating from a particular ignore a given
 * monster completely?
 *
 *  @param agent       The source of the beam. If nullptr, the beam has no source.
 *  @param mon         A monster that is the target of the beam.
 *  @param do_message  If true, prints a message stating how the monster
 *                        avoided the beam.
 *  @return            Whether the given agent can shoot through this monster.
 */
bool shoot_through_monster(const actor* agent, const monster& mon, bool do_message)
{
    // Projectiles can be shot through by anyone.
    if (mons_is_projectile(mon))
        return true;

    // But all the other checks require an aligned agent.
    if (!agent || !mons_aligned(agent, &mon))
        return false;

    if (mons_is_avatar(mon.type))
        return true;

    if ((agent->is_player() && have_passive(passive_t::shoot_through_plants)
         || agent->is_monster() && agent->deity() == GOD_FEDHAS)
         && mons_class_is_plant(mon.type))
    {
        if (do_message && you.can_see(mon))
        {
            simple_god_message(
                        make_stringf(" protects %s plant from harm.",
                            agent->is_player() ? "your" : "a").c_str(),
                        false, GOD_FEDHAS);
        }
        return true;
    }

    if (agent->is_player() && mons_is_hepliaklqana_ancestor(mon.type))
    {
        // TODO: this message does not work very well for all sorts of attacks
        // should this be a god message?
        if (do_message && you.can_see(mon))
            mprf("%s avoids your attack.", mon.name(DESC_THE).c_str());
        return true;
    }

    if (agent->is_player()
        && will_have_passive(passive_t::shadow_attacks)
        && mons_is_player_shadow(mon))
    {
        return true;
    }

    if (agent->is_player() && testbits(mon.flags, MF_DEMONIC_GUARDIAN))
    {
        if (do_message && you.can_see(mon))
            mpr("Your demonic guardian avoids your attack.");
        return true;
    }

    if (agent->is_player() && mon.type == MONS_HAUNTED_ARMOUR)
        return true;

    return false;
}

bool shoot_through_monster(const actor* agent, const monster* mon, bool do_message)
{
    return mon && shoot_through_monster(agent, *mon, do_message);
}

/**
 * Should all actions (beams, explosions, and otherwise) caused by a particular
 * agent ignore a given monster?
 *
 * This is *mostly* the same as shoot_through_monster(), but Jiyva jellies
 * are specifically impossible to harm, but cannot be shot through.
 *
 *  @param agent       The source of the action. If nullptr, the action has no source.
 *  @param mon         A monster that is the target of the action.
 *  @param do_message  If true, prints a message stating how the monster avoided harm.
 *  @return            Whether the monster cannot be harmed by this agent.
 */
bool never_harm_monster(const actor* agent, const monster& mon, bool do_message)
{
    if (shoot_through_monster(agent, mon, do_message))
        return true;

    if (agent && agent->is_player()
        && have_passive(passive_t::neutral_slimes)
        && mons_is_slime(mon))
    {
        if (do_message && you.can_see(mon))
            simple_god_message(" protects your slime from harm.", false, GOD_JIYVA);
        return true;
    }

    return false;
}

bool never_harm_monster(const actor* agent, const monster* mon, bool do_message)
{
    return mon && never_harm_monster(agent, *mon, do_message);
}

/**
 * Returns the maximum distance this type of monster is allowed to be from its
 * creator. It will refuse to move further away than this under its own power,
 * and if circumstances cause it to exceed this, it will abandon other plans to
 * return to this range instead.
 */
int mons_leash_range(monster_type mc)
{
    switch (mc)
    {
        case MONS_PHALANX_BEETLE:   return 1;
        case MONS_HAUNTED_ARMOUR:   return 2;
        default:                    return 0; // No leashing
    }
}
