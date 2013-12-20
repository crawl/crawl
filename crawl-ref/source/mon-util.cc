/**
 * @file
 * @brief Misc monster related functions.
**/

#include "AppHdr.h"

#include <math.h>

#include "mon-util.h"

#include "act-iter.h"
#include "areas.h"
#include "artefact.h"
#include "beam.h"
#include "colour.h"
#include "coordit.h"
#include "database.h"
#include "dgn-overview.h"
#include "directn.h"
#include "dungeon.h"
#include "env.h"
#include "errors.h"
#include "fight.h"
#include "food.h"
#include "fprop.h"
#include "ghost.h"
#include "goditem.h"
#include "itemname.h"
#include "itemprop.h"
#include "items.h"
#include "libutil.h"
#include "mapmark.h"
#include "mgen_data.h"
#include "misc.h"
#include "mon-abil.h"
#include "mon-behv.h"
#include "mon-chimera.h"
#include "mon-death.h"
#include "mon-place.h"
#include "mon-stuff.h"
#include "notes.h"
#include "options.h"
#include "random.h"
#include "religion.h"
#include "showsymb.h"
#include "species.h"
#include "spl-util.h"
#include "spl-summoning.h"
#include "state.h"
#include "stuff.h"
#include "terrain.h"
#include "tilepick.h"
#include "tileview.h"
#include "traps.h"
#include "unicode.h"
#include "unwind.h"
#include "view.h"

static FixedVector < int, NUM_MONSTERS > mon_entry;

struct mon_display
{
    ucs_t        glyph;
    unsigned     colour;
    monster_type detected; // What a monster of type "type" is detected as.

    mon_display(unsigned gly = 0, unsigned col = 0,
                monster_type d = MONS_NO_MONSTER)
       : glyph(gly), colour(col), detected(d) { }
};

static mon_display monster_symbols[NUM_MONSTERS];

static bool initialised_randmons = false;
static vector<monster_type> monsters_by_habitat[NUM_HABITATS];
static vector<monster_type> species_by_habitat[NUM_HABITATS];

const mon_spellbook mspell_list[] =
{
#include "mon-spll.h"
};

#include "mon-data.h"

#define MONDATASIZE ARRAYSZ(mondata)

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

static habitat_type _grid2habitat(dungeon_feature_type grid)
{
    if (feat_is_watery(grid))
        return HT_WATER;

    if (feat_is_rock(grid) && !feat_is_permarock(grid))
        return HT_ROCK;

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
    case HT_ROCK:
        return DNGN_ROCK_WALL;
    case HT_LAND:
    case HT_AMPHIBIOUS:
    default:
        return DNGN_FLOOR;
    }
}

static void _initialise_randmons()
{
    for (int i = 0; i < NUM_HABITATS; ++i)
    {
        set<monster_type> tmp_species;
        const dungeon_feature_type grid = habitat2grid(habitat_type(i));

        for (monster_type mt = MONS_0; mt < NUM_MONSTERS; ++mt)
        {
            if (invalid_monster_type(mt))
                continue;

            if (monster_habitable_grid(mt, grid))
                monsters_by_habitat[i].push_back(mt);

            const monster_type species = mons_species(mt);
            if (monster_habitable_grid(species, grid))
                tmp_species.insert(species);

        }

        for (set<monster_type>::iterator it = tmp_species.begin();
             it != tmp_species.end(); ++it)
        {
            species_by_habitat[i].push_back(*it);
        }
    }
    initialised_randmons = true;
}

monster_type random_monster_at_grid(const coord_def& p, bool species)
{
    if (!initialised_randmons)
        _initialise_randmons();

    const habitat_type ht = _grid2habitat(grd(p));
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

    for (unsigned i = 0; i < ARRAYSZ(mondata); ++i)
    {
        string name = mondata[i].name;
        lowercase(name);

        const int          mtype = mondata[i].mc;
        const monster_type mon   = monster_type(mtype);

        // Deal sensibly with duplicate entries; refuse or allow the
        // insert, depending on which should take precedence.  Mostly we
        // don't care, except looking up "rakshasa" and getting _FAKE
        // breaks ?/M rakshasa.
        if (Mon_Name_Cache.count(name))
        {
            if (mon == MONS_RAKSHASA_FAKE || mon == MONS_MARA_FAKE
                || mon != MONS_SERPENT_OF_HELL
                   && mons_species(mon) == MONS_SERPENT_OF_HELL)
            {
                // Keep previous entry.
                continue;
            }
            else
                die("Un-handled duplicate monster name: %s", name.c_str());
        }

        Mon_Name_Cache[name] = mon;
    }
}

monster_type get_monster_by_name(string name, bool substring)
{
    if (name.empty())
        return MONS_PROGRAM_BUG;

    lowercase(name);

    if (!substring)
    {
        mon_name_map::iterator i = Mon_Name_Cache.find(name);

        if (i != Mon_Name_Cache.end())
            return i->second;

        return MONS_PROGRAM_BUG;
    }

    int best = 0x7fffffff;

    monster_type mon = MONS_PROGRAM_BUG;
    for (unsigned i = 0; i < ARRAYSZ(mondata); ++i)
    {
        string candidate = mondata[i].name;
        lowercase(candidate);

        const int mtype = mondata[i].mc;

        const string::size_type match = candidate.find(name);
        if (match == string::npos)
            continue;

        int qual = 0x7ffffffe;
        // We prefer prefixes over partial matches.
        if (match == 0)
            qual = candidate.length();;

        if (qual < best)
            best = qual, mon = monster_type(mtype);
    }
    return mon;
}

void init_monsters()
{
    // First, fill static array with dummy values. {dlb}
    mon_entry.init(-1);

    // Next, fill static array with location of entry in mondata[]. {dlb}:
    for (unsigned int i = 0; i < MONDATASIZE; ++i)
        mon_entry[mondata[i].mc] = i;

    // Finally, monsters yet with dummy entries point to TTTSNB(tm). {dlb}:
    for (unsigned int i = 0; i < NUM_MONSTERS; ++i)
        if (mon_entry[i] == -1)
            mon_entry[i] = mon_entry[MONS_PROGRAM_BUG];

    init_monster_symbols();
}

void init_monster_symbols()
{
    map<unsigned, monster_type> base_mons;
    for (monster_type mc = MONS_0; mc < NUM_MONSTERS; ++mc)
    {
        mon_display &md = monster_symbols[mc];
        const monsterentry *me = get_monster_data(mc);
        if (me)
        {
            md.glyph  = me->basechar;
            md.colour = me->colour;
            map<unsigned, monster_type>::iterator it = base_mons.find(md.glyph);
            if (it == base_mons.end() || it->first == MONS_PROGRAM_BUG)
                base_mons[md.glyph] = mc;
            md.detected = base_mons[md.glyph];
        }
    }

    // Let those follow the feature settings, unless specifically overridden.
    monster_symbols[MONS_ANIMATED_TREE].glyph = get_feat_symbol(DNGN_TREE);
    for (monster_type mc = MONS_0; mc < NUM_MONSTERS; ++mc)
        if (mons_genus(mc) == MONS_STATUE)
            monster_symbols[mc].glyph = get_feat_symbol(DNGN_GRANITE_STATUE);

    for (map<monster_type, cglyph_t>::iterator it = Options.mon_glyph_overrides.begin();
         it != Options.mon_glyph_overrides.end(); ++it)
    {
        if (it->first == MONS_PROGRAM_BUG)
            continue;

        const cglyph_t &md = it->second;

        if (md.ch)
            monster_symbols[it->first].glyph = get_glyph_override(md.ch);
        if (md.col)
            monster_symbols[it->first].colour = md.col;
    }

    // Validate all glyphs, even those which didn't come from an override.
    for (monster_type i = MONS_PROGRAM_BUG; i < NUM_MONSTERS; ++i)
        if (wcwidth(monster_symbols[i].glyph) != 1)
            monster_symbols[i].glyph = mons_base_char(i);
}

static bool _get_tentacle_head(const monster*& mon)
{
    // For tentacle segments, find the associated tentacle.
    if (mon->is_child_tentacle_segment())
    {
        if (invalid_monster_index(mon->number))
            return false;
        if (invalid_monster(&menv[mon->number]))
            return false;

        mon = &menv[mon->number];
    }

    // For tentacles, find the associated head.
    if (mon->is_child_tentacle())
    {
        if (invalid_monster_index(mon->number))
            return false;
        if (invalid_monster(&menv[mon->number]))
            return false;

        mon = &menv[mon->number];
    }

    return true;
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
    all = all & ~(res * 7) | res * (lev & 7);
}

resists_t get_mons_class_resists(monster_type mc)
{
    const monsterentry *me = get_monster_data(mc);
    return me ? me->resists : get_monster_data(MONS_PROGRAM_BUG)->resists;
}

resists_t get_mons_resists(const monster* mon)
{
    _get_tentacle_head(mon);

    resists_t resists = get_mons_class_resists(mon->type);

    if (mons_is_ghost_demon(mon->type))
        resists |= mon->ghost->resists;

    if (mons_genus(mon->type) == MONS_DRACONIAN
            && mon->type != MONS_DRACONIAN
        || mon->type == MONS_TIAMAT)
    {
        monster_type draco_species = draco_subspecies(mon);
        if (draco_species != mon->type)
            resists |= get_mons_class_resists(draco_species);
    }

    // Undead get full poison resistance. This is set from here in case
    // they're undead due to the MF_FAKE_UNDEAD flag.
    if (mon->holiness() == MH_UNDEAD)
        resists = resists & ~(MR_RES_POISON * 7) | MR_RES_POISON * 3;

    return resists;
}

int get_mons_resist(const monster* mon, mon_resist_flags res)
{
    return get_resist(get_mons_resists(mon), res);
}

monster* monster_at(const coord_def &pos)
{
    if (!in_bounds(pos))
        return NULL;

    const int mindex = mgrd(pos);
    if (mindex == NON_MONSTER)
        return NULL;

    ASSERT(mindex <= MAX_MONSTERS);
    return &menv[mindex];
}

bool mons_class_flag(monster_type mc, uint64_t bf)
{
    const monsterentry *me = get_monster_data(mc);
    return me ? (me->bitfields & bf) != 0 : false;
}

int monster::wearing(equipment_type slot, int sub_type, bool calc_unid) const
{
    int ret = 0;
    const item_def *item = 0;

    switch (slot)
    {
    case EQ_WEAPON:
    case EQ_STAFF:
        {
            const mon_inv_type end = mons_wields_two_weapons(this)
                                     ? MSLOT_ALT_WEAPON : MSLOT_WEAPON;

            for (int i = MSLOT_WEAPON; i <= end; i = i + 1)
            {
                item = mslot_item((mon_inv_type) i);
                if (item && item->base_type == (slot == EQ_WEAPON ? OBJ_WEAPONS
                                                                  : OBJ_STAVES)
                    && item->sub_type == sub_type
                    // Weapon subtypes are always known, staves not.
                    && (slot == EQ_WEAPON || calc_unid
                        || item_type_known(*item)))
                {
                    ret++;
                }
            }
        }
        break;

    case EQ_ALL_ARMOUR:
    case EQ_CLOAK:
    case EQ_HELMET:
    case EQ_GLOVES:
    case EQ_BOOTS:
    case EQ_SHIELD:
        item = mslot_item(MSLOT_SHIELD);
        if (item && item->base_type == OBJ_ARMOUR && item->sub_type == sub_type)
            ret++;
        // Don't check MSLOT_ARMOUR for EQ_SHIELD
        if (slot == EQ_SHIELD)
            break;
        // intentional fall-through
    case EQ_BODY_ARMOUR:
        item = mslot_item(MSLOT_ARMOUR);
        if (item && item->base_type == OBJ_ARMOUR && item->sub_type == sub_type)
            ret++;
        break;

    case EQ_AMULET:
    case EQ_RINGS:
    case EQ_RINGS_PLUS:
    case EQ_RINGS_PLUS2:
        item = mslot_item(MSLOT_JEWELLERY);
        if (item && item->base_type == OBJ_JEWELLERY
            && item->sub_type == sub_type
            && (calc_unid || item_type_known(*item)))
        {
            if (slot == EQ_RINGS_PLUS)
                ret += item->plus;
            else if (slot == EQ_RINGS_PLUS2)
                ret += item->plus2;
            else
                ret++;
        }
        break;
    default:
        die("invalid slot %d for monster::wearing()", slot);
    }
    return ret;
}

int monster::wearing_ego(equipment_type slot, int special, bool calc_unid) const
{
    int ret = 0;
    const item_def *item = 0;

    switch (slot)
    {
    case EQ_WEAPON:
        {
            const mon_inv_type end = mons_wields_two_weapons(this)
                                     ? MSLOT_ALT_WEAPON : MSLOT_WEAPON;

            for (int i = MSLOT_WEAPON; i <= end; i++)
            {
                item = mslot_item((mon_inv_type) i);
                if (item && item->base_type == OBJ_WEAPONS
                    && get_weapon_brand(*item) == special
                    && (calc_unid || item_type_known(*item)))
                {
                    ret++;
                }
            }
        }
        break;

    case EQ_ALL_ARMOUR:
    case EQ_CLOAK:
    case EQ_HELMET:
    case EQ_GLOVES:
    case EQ_BOOTS:
    case EQ_SHIELD:
        item = mslot_item(MSLOT_SHIELD);
        if (item && item->base_type == OBJ_ARMOUR
            && get_armour_ego_type(*item) == special
            && (calc_unid || item_type_known(*item)))
        {
            ret++;
        }
        // Don't check MSLOT_ARMOUR for EQ_SHIELD
        if (slot == EQ_SHIELD)
            break;
        // intentional fall-through
    case EQ_BODY_ARMOUR:
        item = mslot_item(MSLOT_ARMOUR);
        if (item && item->base_type == OBJ_ARMOUR
            && get_armour_ego_type(*item) == special
            && (calc_unid || item_type_known(*item)))
        {
            ret++;
        }
        break;

    case EQ_AMULET:
    case EQ_STAFF:
    case EQ_RINGS:
    case EQ_RINGS_PLUS:
    case EQ_RINGS_PLUS2:
        // No egos.
        break;

    default:
        die("invalid slot %d for monster::wearing_ego()", slot);
    }
    return ret;
}

int monster::scan_artefacts(artefact_prop_type ra_prop, bool calc_unid) const
{
    int ret = 0;

    // TODO: do we really want to prevent randarts from working for zombies?
    if (mons_itemuse(this) >= MONUSE_STARTING_EQUIPMENT)
    {
        const int weap      = inv[MSLOT_WEAPON];
        const int second    = inv[MSLOT_ALT_WEAPON]; // Two-headed ogres, etc.
        const int armour    = inv[MSLOT_ARMOUR];
        const int shld      = inv[MSLOT_SHIELD];
        const int jewellery = inv[MSLOT_JEWELLERY];

        if (weap != NON_ITEM && mitm[weap].base_type == OBJ_WEAPONS
            && is_artefact(mitm[weap]))
        {
            ret += artefact_wpn_property(mitm[weap], ra_prop);
        }

        if (second != NON_ITEM && mitm[second].base_type == OBJ_WEAPONS
            && is_artefact(mitm[second]) && mons_wields_two_weapons(this))
        {
            ret += artefact_wpn_property(mitm[second], ra_prop);
        }

        if (armour != NON_ITEM && mitm[armour].base_type == OBJ_ARMOUR
            && is_artefact(mitm[armour]))
        {
            ret += artefact_wpn_property(mitm[armour], ra_prop);
        }

        if (shld != NON_ITEM && mitm[shld].base_type == OBJ_ARMOUR
            && is_artefact(mitm[shld]))
        {
            ret += artefact_wpn_property(mitm[shld], ra_prop);
        }

        if (jewellery != NON_ITEM && mitm[jewellery].base_type == OBJ_JEWELLERY
            && is_artefact(mitm[jewellery]))
        {
            ret += artefact_wpn_property(mitm[jewellery], ra_prop);
        }
    }

    return ret;
}

mon_holy_type mons_class_holiness(monster_type mc)
{
    ASSERT_smc();
    return smc->holiness;
}

bool mons_class_is_confusable(monster_type mc)
{
    ASSERT_smc();
    return smc->resist_magic < MAG_IMMUNE
           && mons_class_holiness(mc) != MH_NONLIVING
           && mons_class_holiness(mc) != MH_PLANT;
}

bool mons_class_is_stationary(monster_type mc)
{
    return mons_class_flag(mc, M_STATIONARY);
}

// Monsters that are worthless obstacles: not to
// be attacked by default, but may be cut down to
// get to target even if coaligned.
bool mons_class_is_firewood(monster_type mc)
{
    return mons_class_is_stationary(mc)
           && mons_class_flag(mc, M_NO_EXP_GAIN)
           && !mons_is_tentacle_or_tentacle_segment(mc);
}

bool mons_is_firewood(const monster* mon)
{
    return mons_class_is_firewood(mon->type);
}

// "body" in a purely grammatical sense.
bool mons_has_body(const monster* mon)
{
    if (mon->type == MONS_FLYING_SKULL
        || mon->type == MONS_CURSE_SKULL
        || mon->type == MONS_CURSE_TOE
        || mons_class_is_animated_weapon(mon->type))
    {
        return false;
    }

    switch (mons_base_char(mon->type))
    {
    case 'P':
    case 'v':
    case 'G':
    case '*':
    case '%':
    case 'J':
        return false;
    }

    return true;
}

bool mons_has_flesh(const monster* mon)
{
    if (mon->is_skeletal() || mon->is_insubstantial())
        return false;

    // Dictionary says:
    // 1. (12) flesh -- (the soft tissue of the body of a vertebrate:
    //    mainly muscle tissue and fat)
    // 3. pulp, flesh -- (a soft moist part of a fruit)
    // yet I exclude sense 3 anyway but include arthropods and molluscs.
    return mon->holiness() != MH_PLANT
           && mon->holiness() != MH_NONLIVING
           && mons_base_char(mon->type) != 'G'  // eyes
           && mons_base_char(mon->type) != 'J'  // jellies
           && mons_base_char(mon->type) != '%';// cobs (plant!)
}

// Difference in speed between monster and the player for Cheibriados'
// purposes. This is the speed difference disregarding the player's
// slow status.
int cheibriados_monster_player_speed_delta(const monster* mon)
{
    // Ignore the Slow effect.
    unwind_var<int> ignore_slow(you.duration[DUR_SLOW], 0);
    const int pspeed = 1000 / (player_movement_speed(true) * player_speed());
    dprf("Your delay: %d, your speed: %d, mon speed: %d",
        player_movement_speed(), pspeed, mon->speed);
    return mon->speed - pspeed;
}

bool cheibriados_thinks_mons_is_fast(const monster* mon)
{
    return cheibriados_monster_player_speed_delta(mon) > 0;
}

bool mons_is_projectile(monster_type mc)
{
    return mc == MONS_ORB_OF_DESTRUCTION;
}

bool mons_is_projectile(const monster* mon)
{
    return mon->type == MONS_ORB_OF_DESTRUCTION;
}

bool mons_is_boulder(const monster* mon)
{
    return mon->type == MONS_BOULDER_BEETLE && mon->rolling();
}

static bool _mons_class_is_clingy(monster_type type)
{
    return mons_genus(type) == MONS_SPIDER || type == MONS_GIANT_GECKO
        || type == MONS_GIANT_COCKROACH || type == MONS_GIANT_MITE
        || type == MONS_DEMONIC_CRAWLER;
}

bool mons_can_cling_to_walls(const monster* mon)
{
    return _mons_class_is_clingy(mon->type)
        || mons_class_is_chimeric(mon->type)
           && _mons_class_is_clingy(get_chimera_legs(mon));
}

// Conjuration or Hexes.  Summoning and Necromancy make the monster a creature
// at least in some degree, golems have a chem granting them that.
bool mons_is_object(monster_type mc)
{
    return mons_is_conjured(mc)
           || mc == MONS_TWISTER
           // unloading seeds helps the species
           || mc == MONS_GIANT_SPORE
           || mc == MONS_LURKING_HORROR
           || mc == MONS_DANCING_WEAPON;
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

bool mons_allows_beogh(const monster* mon)
{
    if (!player_genus(GENPC_ORCISH) || you_worship(GOD_BEOGH))
        return false; // no one else gives a damn

    return mons_genus(mon->type) == MONS_ORC
           && mon->is_priest() && mon->god == GOD_BEOGH;
}

bool mons_allows_beogh_now(const monster* mon)
{
    // Do the expensive LOS check last.
    return mon && mons_allows_beogh(mon)
               && !silenced(mon->pos()) && !mon->has_ench(ENCH_MUTE)
               && !mons_is_confused(mon) && mons_is_seeking(mon)
               && mon->foe == MHITYOU && !mons_is_immotile(mon)
               && you.visible_to(mon) && you.can_see(mon);
}

// Returns true for monsters that obviously (to the player) feel
// "thematically at home" in a branch.  Currently used for native
// monsters recognising traps and patrolling branch entrances.
bool mons_is_native_in_branch(const monster* mons,
                              const branch_type branch)
{
    switch (branch)
    {
    case BRANCH_ELF:
        return mons_genus(mons->type) == MONS_ELF;

    case BRANCH_ORC:
        return mons_genus(mons->type) == MONS_ORC;

    case BRANCH_SHOALS:
        return mons_species(mons->type) == MONS_CYCLOPS
               || mons_species(mons->type) == MONS_MERFOLK
               || mons_genus(mons->type) == MONS_MERMAID
               || mons->type == MONS_HARPY;

    case BRANCH_SLIME:
        return mons_genus(mons->type) == MONS_JELLY;

    case BRANCH_SNAKE:
        return mons_genus(mons->type) == MONS_NAGA
               || mons_genus(mons->type) == MONS_ADDER;

    case BRANCH_ZOT:
        return mons_genus(mons->type) == MONS_DRACONIAN
               || mons->type == MONS_ORB_GUARDIAN
               || mons->type == MONS_ORB_OF_FIRE
               || mons->type == MONS_DEATH_COB;

    case BRANCH_CRYPT:
        return mons->holiness() == MH_UNDEAD;

    case BRANCH_TOMB:
        return mons_genus(mons->type) == MONS_MUMMY;

    case BRANCH_SPIDER:
        return mons_genus(mons->type) == MONS_SPIDER;

    case BRANCH_FOREST:
        return mons_genus(mons->type) == MONS_SPRIGGAN
               || mons_genus(mons->type) == MONS_TENGU
               || mons_genus(mons->type) == MONS_FAUN
               || mons_genus(mons->type) == MONS_SATYR
               || mons_genus(mons->type) == MONS_DRYAD;

    case BRANCH_BLADE:
        return mons->type == MONS_DANCING_WEAPON;

    case BRANCH_ABYSS:
        return mons_is_abyssal_only(mons->type)
               || mons->type == MONS_ABOMINATION_LARGE
               || mons->type == MONS_ABOMINATION_SMALL
               || mons->type == MONS_TENTACLED_MONSTROSITY
               || mons->type == MONS_TENTACLED_STARSPAWN
               || mons->type == MONS_THRASHING_HORROR
               || mons->type == MONS_UNSEEN_HORROR;

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

bool mons_is_poisoner(const monster* mon)
{
    if (chunk_is_poisonous(mons_corpse_effect(mon->type)))
        return true;

    if (mon->has_attack_flavour(AF_POISON)
        || mon->has_attack_flavour(AF_POISON_NASTY)
        || mon->has_attack_flavour(AF_POISON_MEDIUM)
        || mon->has_attack_flavour(AF_POISON_STRONG)
        || mon->has_attack_flavour(AF_POISON_STR)
        || mon->has_attack_flavour(AF_POISON_INT)
        || mon->has_attack_flavour(AF_POISON_DEX)
        || mon->has_attack_flavour(AF_POISON_STAT))
    {
        return true;
    }

    if (mon->type == MONS_DANCING_WEAPON
        && mon->primary_weapon()
        && get_weapon_brand(*mon->primary_weapon()) == SPWPN_VENOM)
    {
        return true;
    }

    return false;
}

// Monsters considered as "slime" for Jiyva.
bool mons_class_is_slime(monster_type mc)
{
    return mons_genus(mc) == MONS_JELLY
           || mons_genus(mc) == MONS_GIANT_EYEBALL
           || mons_genus(mc) == MONS_GIANT_ORANGE_BRAIN;
}

bool mons_is_slime(const monster* mon)
{
    return mons_class_is_slime(mon->type);
}

bool herd_monster(const monster* mon)
{
    return mons_class_flag(mon->type, M_HERD);
}

// Plant or fungus really
bool mons_class_is_plant(monster_type mc)
{
    return mons_genus(mc) == MONS_PLANT
           || mons_genus(mc) == MONS_FUNGUS
           || mons_species(mc) == MONS_BUSH;
}

bool mons_is_plant(const monster* mon)
{
    return mons_class_is_plant(mon->type);
}

bool mons_eats_items(const monster* mon)
{
    return mons_itemeat(mon) == MONEAT_ITEMS
           || mon->has_ench(ENCH_EAT_ITEMS);
}

bool mons_eats_corpses(const monster* mon)
{
    return mons_itemeat(mon) == MONEAT_CORPSES;
}

bool mons_eats_food(const monster* mon)
{
    return mons_itemeat(mon) == MONEAT_FOOD;
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

bool mons_is_statue(monster_type mc, bool allow_disintegrate)
{
    if (mc == MONS_ORANGE_STATUE || mc == MONS_SILVER_STATUE)
        return true;

    if (!allow_disintegrate)
        return mc == MONS_ICE_STATUE || mc == MONS_ROXANNE;

    return false;
}

bool mons_is_mimic(monster_type mc)
{
    return mons_is_item_mimic(mc) || mons_is_feat_mimic(mc);
}

bool mons_is_item_mimic(monster_type mc)
{
#if TAG_MAJOR_VERSION == 34
    return mc >= MONS_INEPT_ITEM_MIMIC && mc <= MONS_MONSTROUS_ITEM_MIMIC;
#else
    return mc >= MONS_INEPT_ITEM_MIMIC && mc <= MONS_RAVENOUS_ITEM_MIMIC;
#endif
}

bool mons_is_feat_mimic(monster_type mc)
{
    return mc >= MONS_INEPT_FEATURE_MIMIC && mc <= MONS_RAVENOUS_FEATURE_MIMIC;
}

void discover_mimic(const coord_def& pos, bool wake)
{
    item_def* item = item_mimic_at(pos);
    const bool feature_mimic = !item && feature_mimic_at(pos);
    // Is there really a mimic here?
    if (!item && !feature_mimic)
        return;

    const dungeon_feature_type feat = grd(pos);

    // If the feature has been destroyed, don't create a floor mimic.
    if (feature_mimic && feat_cannot_be_mimic(feat))
    {
        env.level_map_mask(pos) &= !MMT_MIMIC;
        return;
    }

    const feature_def feat_d = get_feature_def(feat);
    const string name = feature_mimic ? feat_type_name(feat) :
          item->base_type == OBJ_GOLD ? "pile of gold coins"
                                      : item->name(DESC_BASENAME);

    tileidx_t tile = tileidx_feature(pos);
#ifdef USE_TILE
    apply_variations(env.tile_flv(pos), &tile, pos);
#endif

    // If a monster is standing on top of the mimic, move it out of the way.
    actor* act = actor_at(pos);
    if (act && !act->shove(name.c_str()))
    {
        // Not a single habitable place left on the level.  Possible in a Zig
        // or if a paranoid player covers a small Trove with summons.
        mpr("There is some commotion, and a hidden mimic gets squished!");
        if (item)
            destroy_item(*item, true);
        else
        {
            unnotice_feature(level_pos(level_id::current(), pos));
            grd(pos) = DNGN_FLOOR;
            env.level_map_mask(pos) &= !MMT_MIMIC;
            set_terrain_changed(pos);
            remove_markers_and_listeners_at(pos);
        }
        return;
    }

    if (feature_mimic)
    {
        // If we took a note of this feature, then note that it was a mimic.
        if (!is_boring_terrain(feat))
        {
            string desc = feature_description_at(pos, false, DESC_THE, false);
            take_note(Note(NOTE_FEAT_MIMIC, 0, 0, desc.c_str()));
        }

        // Remove the feature and clear the flag.
        unnotice_feature(level_pos(level_id::current(), pos));
        grd(pos) = DNGN_FLOOR;
        env.level_map_mask(pos) &= !MMT_MIMIC;
        set_terrain_changed(pos);
        remove_markers_and_listeners_at(pos);

        if (feat_is_door(feat))
            env.level_map_mask(pos) |= MMT_WAS_DOOR_MIMIC;
    }

    // Generate and place the monster.
    mgen_data mg;
    mg.behaviour = wake ? BEH_WANDER : BEH_LURK;
    mg.cls = item ? MONS_ITEM_MIMIC : MONS_FEATURE_MIMIC;
    mg.pos = pos;
    if (wake)
        mg.flags |= MG_DONT_COME;

    const int level = env.absdepth0 + 1;

    // Early levels get inept mimics instead
    if (!x_chance_in_y(level - 6, 6))
        mg.cls = item ? MONS_INEPT_ITEM_MIMIC : MONS_INEPT_FEATURE_MIMIC;
    // Deeper, you get ravenous mimics
    else if (x_chance_in_y(level - 15, 6))
        mg.cls = item ? MONS_RAVENOUS_ITEM_MIMIC : MONS_RAVENOUS_FEATURE_MIMIC;


    if (feature_mimic)
    {
        if (feat_is_stone_stair(feat))
            mg.colour = feat_d.em_colour;
        else
            mg.colour = feat_d.colour;

        mg.props["feat_type"] = static_cast<short>(feat);
        mg.props["glyph"] = static_cast<int>(get_feat_symbol(feat));

        mg.props["tile_idx"] = static_cast<int>(tile);
    }
    else
    {
        const cglyph_t glyph = get_item_glyph(item);
        mg.colour = glyph.col;
        mg.props["glyph"] = static_cast<int>(glyph.ch);
    }

    monster *mimic = place_monster(mg, true, true);
    if (!mimic)
    {
        mprf(MSGCH_ERROR, "Too many monsters on level, can't place mimic.");
        if (item)
            destroy_item(*item, true);
        return;
    }

    if (item && !mimic->pickup_misc(*item, 0))
        die("Mimic failed to pickup its item.");

    if (!mimic->move_to_pos(pos))
        die("Moving mimic into position failed.");

    if (wake)
        behaviour_event(mimic, ME_ALERT, &you);

    // Friendly monsters don't appreciate being pushed away.
    if (act && !act->is_player() && act->as_monster()->friendly())
        behaviour_event(act->as_monster(), ME_WHACK, mimic);

    // Announce the mimic.
    if (mons_near(mimic))
    {
        mprf(MSGCH_WARN, "The %s is a mimic!", name.c_str());
        mimic->seen_context = SC_JUST_SEEN;
    }

    // Just in case there's another one.
    if (mimic_at(pos))
        discover_mimic(pos);
}

void discover_shifter(monster* shifter)
{
    shifter->flags |= MF_KNOWN_SHIFTER;
}

bool mons_is_demon(monster_type mc)
{
    const char show_char = mons_base_char(mc);

    // Not every demonic monster is a demon (hell hog, hell hound, etc.)
    if (mons_class_holiness(mc) == MH_DEMONIC
        && (isadigit(show_char) || show_char == '&'))
    {
        return true;
    }

    return false;
}

int mons_demon_tier(monster_type mc)
{
    switch (mons_base_char(mc))
    {
    case 'C':
        if (mc != MONS_ANTAEUS)
            return 0;
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

bool mons_is_draconian(monster_type mc)
{
    return mc >= MONS_FIRST_DRACONIAN && mc <= MONS_LAST_DRACONIAN;
}

// Conjured (as opposed to summoned) monsters are actually here, even
// though they're typically volatile (like, made of real fire). As such,
// they should be immune to Abjuration or Recall. Also, they count as
// things rather than beings.
bool mons_is_conjured(monster_type mc)
{
    return mons_is_projectile(mc)
           || mc == MONS_FIRE_VORTEX
           || mc == MONS_SPATIAL_VORTEX
           || mc == MONS_BALL_LIGHTNING
           || mc == MONS_BATTLESPHERE
           || mc == MONS_SPECTRAL_WEAPON
           || mc == MONS_FULMINANT_PRISM;
}

// Returns true if the given monster's foe is also a monster.
bool mons_foe_is_mons(const monster* mons)
{
    const actor *foe = mons->get_foe();
    return foe && foe->is_monster();
}

int mons_weight(monster_type mc)
{
    ASSERT_smc();
    return smc->weight;
}

corpse_effect_type mons_corpse_effect(monster_type mc)
{
    ASSERT_smc();
    return smc->corpse_thingy;
}

monster_type mons_genus(monster_type mc)
{
    if (mc == RANDOM_DRACONIAN || mc == RANDOM_BASE_DRACONIAN
        || mc == RANDOM_NONBASE_DRACONIAN
        || (mc == MONS_PLAYER_ILLUSION && player_genus(GENPC_DRACONIAN)))
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

monster_type draco_subspecies(const monster* mon)
{
    ASSERT(mons_genus(mon->type) == MONS_DRACONIAN);

    if (mon->type == MONS_PLAYER_ILLUSION)
        return player_species_to_mons_species(mon->ghost->species);

    monster_type retval = mons_species(mon->type);

    if (retval == MONS_DRACONIAN && mon->type != MONS_DRACONIAN)
        retval = mon->base_monster;

    return retval;
}

monster_type mons_detected_base(monster_type mc)
{
    return monster_symbols[mc].detected;
}

int get_shout_noise_level(const shout_type shout)
{
    switch (shout)
    {
    case S_SILENT:
        return 0;
    case S_HISS:
    case S_VERY_SOFT:
        return 4;
    case S_SOFT:
        return 6;
    case S_GURGLE:
    case S_LOUD:
        return 10;
    case S_SHOUT2:
    case S_ROAR:
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

    // For Pandemonium lords, almost everything is fair game.  It's only
    // used for the shouting verb ("say", "bellow", "roar", etc.) anyway.
    if (mc != MONS_HELL_BEAST)
        return shout != S_BUZZ && shout != S_WHINE && shout != S_CROAK;

    switch (shout)
    {
    // 2-headed ogres, bees or mosquitos never fit.
    case S_SHOUT2:
    case S_BUZZ:
    case S_WHINE:
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
            u = static_cast<shout_type>(random2(max_shout));
        while (!_shout_fits_monster(mc, u));
    }

    return u;
}

bool mons_is_ghost_demon(monster_type mc)
{
    return mc == MONS_UGLY_THING
            || mc == MONS_VERY_UGLY_THING
            || mc == MONS_PLAYER_GHOST
            || mc == MONS_PLAYER_ILLUSION
            || mons_class_is_animated_weapon(mc)
            || mc == MONS_PANDEMONIUM_LORD
            || mons_class_is_chimeric(mc);
}

bool mons_is_pghost(monster_type mc)
{
    return mc == MONS_PLAYER_GHOST || mc == MONS_PLAYER_ILLUSION;
}

bool mons_is_unique(monster_type mc)
{
    return mons_class_flag(mc, M_UNIQUE);
}

bool mons_sense_invis(const monster* mon)
{
    return mons_class_flag(mon->type, M_SENSE_INVIS);
}

ucs_t mons_char(monster_type mc)
{
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

mon_itemuse_type mons_itemuse(const monster* mon)
{
    if (mons_enslaved_soul(mon))
        return mons_class_itemuse(mons_zombie_base(mon));

    return mons_class_itemuse(mon->type);
}

static mon_itemeat_type _mons_class_itemeat(monster_type mc)
{
    ASSERT_smc();
    return smc->gmon_eat;
}

mon_itemeat_type mons_itemeat(const monster* mon)
{
    if (mons_enslaved_soul(mon))
        return _mons_class_itemeat(mons_zombie_base(mon));

    if (mon->has_ench(ENCH_EAT_ITEMS))
        return MONEAT_ITEMS;

    return _mons_class_itemeat(mon->type);
}

int mons_class_colour(monster_type mc)
{
    ASSERT_smc();
    return monster_symbols[mc].colour;
}

bool mons_class_can_regenerate(monster_type mc)
{
    return !mons_class_flag(mc, M_NO_REGEN);
}

bool mons_can_regenerate(const monster* mon)
{
    _get_tentacle_head(mon);

    if (testbits(mon->flags, MF_NO_REGEN))
        return false;

    return mons_class_can_regenerate(mon->type);
}

bool mons_class_fast_regen(monster_type mc)
{
    return mons_class_flag(mc, M_FAST_REGEN);
}

bool mons_class_can_display_wounds(monster_type mc)
{
    // Zombified monsters other than spectral things don't show
    // wounds.
    if (mons_class_is_zombified(mc) && mc != MONS_SPECTRAL_THING)
        return false;

    switch (mc)
    {
    case MONS_RAKSHASA:
    case MONS_RAKSHASA_FAKE:
        return false;
    default:
        return true;
    }
}

bool mons_can_display_wounds(const monster* mon)
{
    _get_tentacle_head(mon);

    return mons_class_can_display_wounds(mon->type);
}

bool mons_class_leaves_hide(monster_type mc)
{
    if (mons_genus(mc) == MONS_TROLL)
        return true;
    switch (mc)
    {
    case MONS_FIRE_DRAGON:
    case MONS_ICE_DRAGON:
    case MONS_STEAM_DRAGON:
    case MONS_MOTTLED_DRAGON:
    case MONS_STORM_DRAGON:
    case MONS_GOLDEN_DRAGON:
    case MONS_SWAMP_DRAGON:
    case MONS_PEARL_DRAGON:
        return true;
    default:
        return false;
    }
}

int mons_zombie_size(monster_type mc)
{
    ASSERT_smc();
    return smc->zombie_size;
}

monster_type mons_zombie_base(const monster* mon)
{
    return mon->base_monster;
}

bool mons_class_is_zombified(monster_type mc)
{
    return mc == MONS_ZOMBIE
        || mc == MONS_SKELETON
        || mc == MONS_SIMULACRUM
        || mc == MONS_SPECTRAL_THING;
}

bool mons_class_is_hybrid(monster_type mc)
{
    return mons_class_flag(mc, M_HYBRID);
}

bool mons_class_is_chimeric(monster_type mc)
{
    return mc == MONS_CHIMERA;
}

bool mons_class_is_animated_weapon(monster_type type)
{
    return type == MONS_DANCING_WEAPON || type == MONS_SPECTRAL_WEAPON;
}

bool mons_is_zombified(const monster* mon)
{
    return mons_class_is_zombified(mon->type);
}

monster_type mons_base_type(const monster* mon)
{
    if (mons_class_is_zombified(mon->type) || mons_class_is_chimeric(mon->type))
        return mon->base_monster;
    return mon->type;
}

bool mons_class_can_leave_corpse(monster_type mc)
{
    return mons_weight(mc) > 0;
}

bool mons_class_can_be_zombified(monster_type mc)
{
    monster_type ms = mons_species(mc);
    return !invalid_monster_type(ms)
           && mons_zombie_size(ms) != Z_NOZOMBIE
           && mons_weight(ms) > 0;
}

bool mons_can_be_zombified(const monster* mon)
{
    return mons_class_can_be_zombified(mon->type)
           && !mon->is_summoned()
           && !mons_enslaved_body_and_soul(mon);
}

bool mons_class_can_use_stairs(monster_type mc)
{
    return (!mons_class_is_zombified(mc) || mc == MONS_SPECTRAL_THING)
           && !mons_is_tentacle_or_tentacle_segment(mc)
           && !mons_is_abyssal_only(mc)
           && mc != MONS_SILENT_SPECTRE
           && mc != MONS_PLAYER_GHOST
           && mc != MONS_GERYON
           && mc != MONS_ROYAL_JELLY;
}

bool mons_can_use_stairs(const monster* mon)
{
    if (!mons_class_can_use_stairs(mon->type))
        return false;

    // Check summon status
    int stype = 0;
    // Other permanent summons can always use stairs
    if (mon->is_summoned(0, &stype) && !mon->is_perm_summoned()
        && stype > 0 && stype < NUM_SPELLS)
    {
        // Allow uncapped summons to use stairs. This means creatures
        // from misc evokables, temporary god summons, etc. These tend
        // to be balanced by other means; however this could use a review
        // and perhaps needs a whilelist (or long-duration vs. short-duration).
        return !summons_are_capped(static_cast<spell_type>(stype));
    }
    // Everything else is fine
    return true;
}

bool mons_enslaved_body_and_soul(const monster* mon)
{
    return mon->has_ench(ENCH_SOUL_RIPE);
}

bool mons_enslaved_soul(const monster* mon)
{
    return testbits(mon->flags, MF_ENSLAVED_SOUL);
}

bool name_zombie(monster* mon, monster_type mc, const string &mon_name)
{
    mon->mname = mon_name;

    // Special case for Blork the orc: shorten his name to "Blork" to
    // avoid mentions of "Blork the orc the orc zombie".
    if (mc == MONS_BLORK_THE_ORC)
        mon->mname = "Blork";
    // Also for the Lernaean hydra: treat Lernaean as an adjective to
    // avoid mentions of "the Lernaean hydra the X-headed hydra zombie".
    else if (mc == MONS_LERNAEAN_HYDRA)
    {
        mon->mname = "Lernaean";
        mon->flags |= MF_NAME_ADJECTIVE;
    }
    // Also for the Enchantress: treat Enchantress as an adjective to
    // avoid mentions of "the Enchantress the spriggan zombie".
    else if (mc == MONS_THE_ENCHANTRESS)
    {
        mon->mname = "Enchantress";
        mon->flags |= MF_NAME_ADJECTIVE;
    }

    if (starts_with(mon->mname, "shaped "))
        mon->flags |= MF_NAME_SUFFIX;

    // It's unlikely there's a desc for "Duvessa the elf skeleton", but
    // we still want to allow it if overridden.
    if (!mon->props.exists("dbname"))
        mon->props["dbname"] = mons_class_name(mon->type);

    return true;
}

bool name_zombie(monster* mon, const monster* orig)
{
    if (!mons_is_unique(orig->type) && orig->mname.empty())
        return false;

    string name;

    if (!orig->mname.empty())
        name = orig->mname;
    else
        name = mons_type_name(orig->type, DESC_PLAIN);

    return name_zombie(mon, orig->type, name);
}

static int _downscale_zombie_damage(int damage)
{
    // These are cumulative, of course: {dlb}
    if (damage > 1)
        damage--;
    if (damage > 4)
        damage--;
    if (damage > 11)
        damage--;
    if (damage > 14)
        damage--;

    return damage;
}

static mon_attack_def _downscale_zombie_attack(const monster* mons,
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

    if (mons->type == MONS_SIMULACRUM)
        attk.flavour = AF_COLD;
    else if (mons->type == MONS_SPECTRAL_THING && coinflip())
        attk.flavour = AF_DRAIN_XP;
    else if (attk.flavour != AF_REACH && attk.flavour != AF_CRUSH)
        attk.flavour = AF_PLAIN;

    attk.damage = _downscale_zombie_damage(attk.damage);

    return attk;
}

mon_attack_def mons_attack_spec(const monster* mon, int attk_number)
{
    monster_type mc = mon->type;

    _get_tentacle_head(mon);

    const bool zombified = mons_is_zombified(mon);

    if (attk_number < 0 || attk_number >= MAX_NUM_ATTACKS || mon->has_hydra_multi_attack())
        attk_number = 0;

    if (mons_class_is_chimeric(mc))
    {
        // Chimera get attacks 0, 1 and 2 from their base components. Attack 3 is
        // the secondary attack of the main base type.
        switch (attk_number)
        {
        case 0:
            mc = mon->base_monster;
            break;
        case 1:
            if (mon->props.exists("chimera_part_2"))
            {
                mc = static_cast<monster_type>(mon->props["chimera_part_2"].get_int());
                attk_number = 0;
            }
            break;
        case 2:
            if (mon->props.exists("chimera_part_3"))
            {
                mc = static_cast<monster_type>(mon->props["chimera_part_3"].get_int());
                attk_number = 0;
            }
            break;
        case 3:
            mc = mon->base_monster;
            attk_number = 1;
            break;
        }
    }
    else if (mons_is_ghost_demon(mc))
    {
        if (attk_number == 0)
        {
            return mon_attack_def::attk(mon->ghost->damage,
                                        mon->ghost->att_type,
                                        mon->ghost->att_flav);
        }

        return mon_attack_def::attk(0, AT_NONE);
    }

    if (zombified && mc != MONS_KRAKEN_TENTACLE)
        mc = mons_zombie_base(mon);

    ASSERT_smc();
    mon_attack_def attk = smc->attack[attk_number];

    if (attk.type == AT_RANDOM)
        attk.type = random_choose(AT_HIT, AT_GORE, -1);

    if (attk.type == AT_CHERUB)
        attk.type = random_choose(AT_HIT, AT_BITE, AT_PECK, AT_GORE, -1);

    if (attk.flavour == AF_KLOWN)
    {
        attack_flavour flavours[] =
            {AF_POISON_NASTY, AF_ROT, AF_DRAIN_XP, AF_FIRE, AF_COLD, AF_BLINK,
             AF_ANTIMAGIC};

        attk.flavour = RANDOM_ELEMENT(flavours);
    }

    if (attk.flavour == AF_POISON_STAT)
    {
        attack_flavour flavours[] =
            {AF_POISON_STR, AF_POISON_INT, AF_POISON_DEX};

        attk.flavour = RANDOM_ELEMENT(flavours);
    }
    else if (attk.flavour == AF_DRAIN_STAT)
    {
        attack_flavour flavours[] =
            {AF_DRAIN_STR, AF_DRAIN_INT, AF_DRAIN_DEX};

        attk.flavour = RANDOM_ELEMENT(flavours);
    }

    // Slime creature attacks are multiplied by the number merged.
    if (mon->type == MONS_SLIME_CREATURE && mon->number > 1)
        attk.damage *= mon->number;

    return zombified ? _downscale_zombie_attack(mon, attk) : attk;
}

static int _mons_damage(monster_type mc, int rt)
{
    if (rt < 0 || rt > 3)
        rt = 0;
    ASSERT_smc();
    return smc->attack[rt].damage;
}

const char* resist_margin_phrase(int margin)
{
    return margin >= 30  ? " resists with almost no effort." :
           margin >= 15  ? " easily resists." :
           margin >= 0   ? " resists." :
           margin >= -14 ? " resists with significant effort.":
           margin >= -30 ? " struggles to resist."
                         : " strains under the huge effort it takes to resist.";
}

bool mons_immune_magic(const monster* mon)
{
    return get_monster_data(mon->type)->resist_magic == MAG_IMMUNE;
}

const char* mons_resist_string(const monster* mon, int res_margin)
{
    if (mons_immune_magic(mon))
        return " is unaffected.";
    else
        return resist_margin_phrase(res_margin);
}

bool mons_skeleton(monster_type mc)
{
    return !mons_class_flag(mc, M_NO_SKELETON);
}

bool mons_zombifiable(monster_type mc)
{
    return !mons_class_flag(mc, M_NO_ZOMBIE) && mons_zombie_size(mc);
}

flight_type mons_class_flies(monster_type mc)
{
    const monsterentry *me = get_monster_data(mc);
    return me ? me->fly : FL_NONE;
}

flight_type mons_flies(const monster* mon, bool temp)
{
    flight_type ret;
    // For dancing weapons, this function can get called before their
    // ghost_demon is created, so check for a NULL ghost. -cao
    if (mons_is_ghost_demon(mon->type) && mon->ghost.get())
        ret = mon->ghost->fly;
    else
        ret = mons_class_flies(mons_base_type(mon));

    // Handle the case where the zombified base monster can't fly, but
    // the zombified monster can (e.g. spectral things).
    if (mons_is_zombified(mon))
        ret = max(ret, mons_class_flies(mon->type));

    if (temp && ret < FL_LEVITATE)
    {
        if (mon->scan_artefacts(ARTP_FLY) > 0)
            return FL_LEVITATE;

        const int armour = mon->inv[MSLOT_ARMOUR];
        if (armour != NON_ITEM
            && mitm[armour].base_type == OBJ_ARMOUR
            && mitm[armour].special == SPARM_FLYING)
        {
            return FL_LEVITATE;
        }

        const int jewellery = mon->inv[MSLOT_JEWELLERY];
        if (jewellery != NON_ITEM
            && mitm[jewellery].base_type == OBJ_JEWELLERY
            && mitm[jewellery].sub_type == RING_FLIGHT)
        {
            return FL_LEVITATE;
        }

        if (mon->has_ench(ENCH_FLIGHT))
            return FL_LEVITATE;
    }

    return ret;
}

bool mons_flattens_trees(const monster* mon)
{
    return mons_base_type(mon) == MONS_LERNAEAN_HYDRA;
}

int mons_class_res_wind(monster_type mc)
{
    // Lightning goes well with storms.
    if (mc == MONS_AIR_ELEMENTAL || mc == MONS_BALL_LIGHTNING
        || mc == MONS_TWISTER || mc == MONS_WIND_DRAKE)
    {
        return 1;
    }

    // Flyers are not immune due to buffeting -- and for airstrike, even
    // specially vulnerable.
    // Smoky humanoids may have problems staying together.
    // Insubstantial wisps are a toss-up between being immune and immediately
    // fatally dispersing.
    return 0;
}

// This nice routine we keep in exactly the way it was.
int hit_points(int hit_dice, int min_hp, int rand_hp)
{
    int hrolled = 0;

    for (int hroll = 0; hroll < hit_dice; ++hroll)
    {
        hrolled += random2(1 + rand_hp);
        hrolled += min_hp;
    }

    return hrolled;
}

// This function returns the standard number of hit dice for a type of
// monster, not a particular monster's current hit dice. - bwr
int mons_class_hit_dice(monster_type mc)
{
    const monsterentry *me = get_monster_data(mc);
    return me ? me->hpdice[0] : 0;
}

int mons_avg_hp(monster_type mc)
{
    // Currently, difficulty is defined as "average hp".  Leaks too much info?
    const monsterentry* me = get_monster_data(mc);

    if (!me)
        return 0;

    // [ds] XXX: Use monster experience value as a better indicator of diff.?
    return me->hpdice[0] * (2 * me->hpdice[1] + me->hpdice[2]) / 2
           + me->hpdice[3];
}

int exper_value(const monster* mon, bool real)
{
    int x_val = 0;

    // These four are the original arguments.
    const monster_type mc = mon->type;
    int hd                = mon->hit_dice;
    int maxhp             = mon->max_hit_points;

    // pghosts and pillusions have no reasonable base values, and you can look
    // up the exact value anyway.  Especially for pillusions.
    if (real || mon->type == MONS_PLAYER_GHOST || mon->type == MONS_PLAYER_ILLUSION)
    {
        // A berserking monster is much harder, but the xp value shouldn't
        // depend on whether it was berserk at the moment of death.
        if (mon->has_ench(ENCH_BERSERK))
            maxhp = (maxhp * 2 + 1) / 3;
    }
    else
    {
        const monsterentry *m = get_monster_data(mons_base_type(mon));
        ASSERT(m);

        // Use real hd, zombies would use the basic species and lose
        // information known to the player ("orc warrior zombie").  Monsters
        // levelling up is visible (although it may happen off-screen), so
        // this is hardly ever a leak.  Only Pan lords are unknown in the
        // general.
        if (m->mc == MONS_PANDEMONIUM_LORD)
            hd = m->hpdice[0];
        maxhp = hd * m->hpdice[1] + (hd * (1 + m->hpdice[2])) / 2 + m->hpdice[3];
    }

    // Hacks to make merged slime creatures not worth so much exp.  We
    // will calculate the experience we would get for 1 blob, and then
    // just multiply it so that exp is linear with blobs merged. -cao
    if (mon->type == MONS_SLIME_CREATURE && mon->number > 1)
        maxhp /= mon->number;

    // These are some values we care about.
    const int speed       = mons_base_speed(mon);
    const int modifier    = _mons_exp_mod(mc);
    const int item_usage  = mons_itemuse(mon);

    // XXX: Shapeshifters can qualify here, even though they can't cast.
    const bool spellcaster = mon->can_use_spells();

    // Early out for no XP monsters.
    if (mons_class_flag(mc, M_NO_EXP_GAIN))
        return 0;

    // The beta26 statues have non-spell-like abilities that the experience
    // code can't see, so inflate their XP a bit.  Ice statues and Roxanne
    // get plenty of XP for their spells.
    if (mc == MONS_ORANGE_STATUE || mc == MONS_SILVER_STATUE)
        return hd * 15;

    x_val = (16 + maxhp) * hd * hd / 10;

    // Let's calculate a simple difficulty modifier. - bwr
    int diff = 0;

    // Let's look for big spells.
    if (spellcaster)
    {
        const monster_spells &hspell_pass = mon->spells;

        for (int i = 0; i < 6; ++i)
        {
            switch (hspell_pass[i])
            {
            case SPELL_PARALYSE:
            case SPELL_SMITING:
            case SPELL_SUMMON_GREATER_DEMON:
            case SPELL_SUMMON_EYEBALLS:
            case SPELL_HELLFIRE_BURST:
            case SPELL_HELLFIRE:
            case SPELL_SYMBOL_OF_TORMENT:
            case SPELL_ICE_STORM:
            case SPELL_FIRE_STORM:
            case SPELL_SHATTER:
            case SPELL_CHAIN_LIGHTNING:
            case SPELL_TORNADO:
                diff += 25;
                break;

            case SPELL_LIGHTNING_BOLT:
            case SPELL_STICKY_FLAME_RANGE:
            case SPELL_DISINTEGRATE:
            case SPELL_HAUNT:
            case SPELL_SUMMON_DRAGON:
            case SPELL_SUMMON_HORRIBLE_THINGS:
            case SPELL_BANISHMENT:
            case SPELL_LEHUDIBS_CRYSTAL_SPEAR:
            case SPELL_IRON_SHOT:
            case SPELL_IOOD:
            case SPELL_FIREBALL:
            case SPELL_HASTE:
            case SPELL_AGONY:
            case SPELL_LRD:
                diff += 10;
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

    // Slime creature exp hack part 2: Scale exp back up by the number
    // of blobs merged. -cao
    if (mon->type == MONS_SLIME_CREATURE && mon->number > 1)
        x_val *= mon->number;

    // Scale starcursed mass exp by what percentage of the whole it represents
    if (mon->type == MONS_STARCURSED_MASS)
        x_val = (x_val * mon->number) / 12;

    // Further reduce xp from zombies
    if (mons_is_zombified(mon))
        x_val /= 2;

    // Reductions for big values. - bwr
    if (x_val > 100)
        x_val = 100 + ((x_val - 100) * 3) / 4;
    if (x_val > 750)
        x_val = 750 + (x_val - 750) / 3;

    // Guarantee the value is within limits.
    if (x_val <= 0)
        x_val = 1;
    else if (x_val > 15000)
        x_val = 15000;

    return x_val;
}

monster_type random_draconian_monster_species()
{
    const int num_drac = MONS_PALE_DRACONIAN - MONS_BLACK_DRACONIAN + 1;
    return static_cast<monster_type>(MONS_BLACK_DRACONIAN + random2(num_drac));
}

// Note: For consistent behavior in player_will_anger_monster(), all
// spellbooks a given monster can get here should produce the same
// return values in the following:
//
//     (is_unholy_spell() || is_evil_spell())
//
//     (is_unclean_spell() || is_chaotic_spell())
//
// FIXME: This is not true for one set of spellbooks; MST_WIZARD_IV
// contains the unholy and chaotic Banishment spell, but the other
// MST_WIZARD-type spellbooks contain no unholy, evil, unclean or
// chaotic spells.
//
// If a monster has only one spellbook, it is specified in mon-data.h.
// If it has multiple books, mon-data.h sets the book to MST_NO_SPELLS,
// and the books are accounted for here.
vector<mon_spellbook_type> mons_spellbook_list(monster_type mon_type)
{
    vector<mon_spellbook_type> books;
    const monsterentry *m = get_monster_data(mon_type);
    mon_spellbook_type book = (m->sec);

    switch (mon_type)
    {
    case MONS_HELL_KNIGHT:
        books.push_back(MST_HELL_KNIGHT_I);
        books.push_back(MST_HELL_KNIGHT_II);
        break;

    case MONS_LICH:
    case MONS_ANCIENT_LICH:
        books.push_back(MST_LICH_I);
        books.push_back(MST_LICH_II);
        books.push_back(MST_LICH_III);
        books.push_back(MST_LICH_IV);
        break;

    case MONS_NECROMANCER:
        books.push_back(MST_NECROMANCER_I);
        books.push_back(MST_NECROMANCER_II);
        break;

    case MONS_ORC_WIZARD:
    case MONS_DEEP_ELF_FIGHTER:
    case MONS_DEEP_ELF_KNIGHT:
        books.push_back(MST_ORC_WIZARD_I);
        books.push_back(MST_ORC_WIZARD_II);
        books.push_back(MST_ORC_WIZARD_III);
        break;

    case MONS_WIZARD:
    case MONS_EROLCHA:
        books.push_back(MST_WIZARD_I);
        books.push_back(MST_WIZARD_II);
        books.push_back(MST_WIZARD_III);
        books.push_back(MST_WIZARD_IV);
        books.push_back(MST_WIZARD_V);
        break;

    case MONS_OGRE_MAGE:
        books.push_back(MST_OGRE_MAGE_I);
        books.push_back(MST_OGRE_MAGE_II);
        books.push_back(MST_OGRE_MAGE_III);
        books.push_back(MST_OGRE_MAGE_IV);
        books.push_back(MST_OGRE_MAGE_V);
        break;

    case MONS_DRACONIAN_KNIGHT:
        books.push_back(MST_DEEP_ELF_CONJURER);
        books.push_back(MST_HELL_KNIGHT_I);
        books.push_back(MST_HELL_KNIGHT_II);
        books.push_back(MST_NECROMANCER_I);
        books.push_back(MST_NECROMANCER_II);
        break;

    case MONS_ANCIENT_CHAMPION:
        books.push_back(MST_ANCIENT_CHAMPION_I);
        books.push_back(MST_ANCIENT_CHAMPION_II);
        books.push_back(MST_ANCIENT_CHAMPION_III);
        books.push_back(MST_ANCIENT_CHAMPION_IV);
        break;

    case MONS_TENGU_CONJURER:
        books.push_back(MST_TENGU_CONJURER_I);
        books.push_back(MST_TENGU_CONJURER_II);
        books.push_back(MST_TENGU_CONJURER_III);
        books.push_back(MST_TENGU_CONJURER_IV);
        break;

    case MONS_TENGU_REAVER:
        books.push_back(MST_TENGU_REAVER_I);
        books.push_back(MST_TENGU_REAVER_II);
        books.push_back(MST_TENGU_REAVER_III);
        break;

    case MONS_DEEP_ELF_MAGE:
        books.push_back(MST_DEEP_ELF_MAGE_I);
        books.push_back(MST_DEEP_ELF_MAGE_II);
        books.push_back(MST_DEEP_ELF_MAGE_III);
        books.push_back(MST_DEEP_ELF_MAGE_IV);
        books.push_back(MST_DEEP_ELF_MAGE_V);
        break;

    default:
        books.push_back(book);
        break;
    }

    return books;
}

static void _mons_load_spells(monster* mon)
{
    vector<mon_spellbook_type> books = mons_spellbook_list(mon->type);
    mon_spellbook_type book = books[random2(books.size())];

    if (book == MST_GHOST)
        return mon->load_ghost_spells();

    mon->spells.init(SPELL_NO_SPELL);
    if (book == MST_NO_SPELLS)
        return;

    dprf("%s: loading spellbook #%d", mon->name(DESC_PLAIN, true).c_str(),
         static_cast<int>(book));

    for (unsigned int i = 0; i < ARRAYSZ(mspell_list); ++i)
    {
        if (mspell_list[i].type == book)
        {
            for (int j = 0; j < NUM_MONSTER_SPELL_SLOTS; ++j)
                mon->spells[j] = mspell_list[i].spells[j];
            break;
        }
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

// Butterflies
static colour_t _random_butterfly_colour()
{
    colour_t col;
    // Restricted to 'light' colours.
    do
        col = random_monster_colour();
    while (is_low_colour(col));

    return col;
}

bool init_abomination(monster* mon, int hd)
{
    if (mon->type == MONS_CRAWLING_CORPSE
        || mon->type == MONS_MACABRE_MASS)
    {
        mon->hit_points = mon->max_hit_points = mon->hit_dice = hd;
        return true;
    }
    else if (mon->type != MONS_ABOMINATION_LARGE
             && mon->type != MONS_ABOMINATION_SMALL)
    {
        return false;
    }

    const int max_hd = mon->type == MONS_ABOMINATION_LARGE ? 30 : 15;
    const int max_ac = mon->type == MONS_ABOMINATION_LARGE ? 20 : 10;

    mon->hit_dice = min(max_hd, hd);

    const monsterentry *m = get_monster_data(mon->type);
    int hp = hit_points(hd, m->hpdice[1], m->hpdice[2]) + m->hpdice[3];

    mon->max_hit_points = hp;
    mon->hit_points     = hp;

    if (mon->type == MONS_ABOMINATION_LARGE)
    {
        mon->ac = min(max_ac, 7 + hd / 2);
        mon->ev = min(max_ac, 2 * hd / 3);
    }
    else
    {
        mon->ac = min(max_ac, 3 + hd * 2 / 3);
        mon->ev = min(max_ac, 4 + hd);
    }

    return true;
}

// Generate a shiny, new and unscarred monster.
void define_monster(monster* mons)
{
    monster_type mcls         = mons->type;
    int monnumber             = mons->number;
    monster_type monbase      = mons->base_monster;
    const monsterentry *m     = get_monster_data(mcls);
    int col                   = mons_class_colour(mcls);
    int hd                    = mons_class_hit_dice(mcls);
    int hp, hp_max, ac, ev;

    mons->mname.clear();

    // misc
    ac = m->AC;
    ev = m->ev;

    mons->god = GOD_NO_GOD;

    switch (mcls)
    {
    case MONS_BUTTERFLY:
        col = _random_butterfly_colour();
        break;

    case MONS_ABOMINATION_SMALL:
        init_abomination(mons, 4 + random2(4));
        break;

    case MONS_ABOMINATION_LARGE:
        init_abomination(mons, 8 + random2(4));
        break;

    case MONS_HELL_BEAST:
        hd = 4 + random2(4);
        ac = 2 + random2(5);
        ev = 7 + random2(5);
        break;

    case MONS_MANTICORE:
        // Manticores start off with 8 to 16 spike volleys.
        monnumber = 8 + random2(9);
        break;

    case MONS_SLIME_CREATURE:
        // Slime creatures start off as only single un-merged blobs.
        monnumber = 1;
        break;

    case MONS_HYDRA:
        // Hydras start off with 4 to 8 heads.
        monnumber = random_range(4, 8);
        break;

    case MONS_LERNAEAN_HYDRA:
        // The Lernaean hydra starts off with 27 heads.
        monnumber = 27;
        break;

    case MONS_KRAKEN:
        if (col != BLACK) // May be overwritten by the mon_glyph option.
            break;

        col = element_colour(ETC_KRAKEN);
        break;

    case MONS_DRACONIAN_CALLER:
    case MONS_DRACONIAN_MONK:
    case MONS_DRACONIAN_ZEALOT:
    case MONS_DRACONIAN_SHIFTER:
    case MONS_DRACONIAN_ANNIHILATOR:
    case MONS_DRACONIAN_SCORCHER:
    case MONS_DRACONIAN_KNIGHT:
    {
        // Professional draconians still have a base draconian type.
        // White draconians will never be draconian scorchers, but
        // apart from that, anything goes.
        do
            monbase = random_draconian_monster_species();
        while (drac_colour_incompatible(mcls, monbase));
        break;
    }

    case MONS_TIAMAT:
        // Initialise to a random draconian type.
        draconian_change_colour(mons);
        monbase = mons->base_monster;
        col = mons->colour;
        break;

    case MONS_STARCURSED_MASS:
        monnumber = 12;
        break;

    // Randomize starting speed burst clock
    case MONS_SIXFIRHY:
    case MONS_JIANGSHI:
        monnumber = random2(360);
        break;

    case MONS_TREANT:
        monnumber = x_chance_in_y(3, 5) ? random_range(2, 4) : 0;
        break;

    default:
        break;
    }

    if (col == BLACK) // but never give out darkgrey to monsters
        col = random_monster_colour();

    // Some calculations.
    hp     = hit_points(hd, m->hpdice[1], m->hpdice[2]);
    hp    += m->hpdice[3];
    hp_max = hp;

    // So let it be written, so let it be done.
    mons->hit_dice        = hd;
    mons->hit_points      = hp;
    mons->max_hit_points  = hp_max;
    mons->ac              = ac;
    mons->ev              = ev;
    mons->speed_increment = 70;

    if (mons->base_monster == MONS_NO_MONSTER)
        mons->base_monster = monbase;

    if (mons->number == 0)
        mons->number = monnumber;

    mons->flags      = 0;
    mons->experience = 0;
    mons->colour     = col;

    mons->bind_melee_flags();

    _mons_load_spells(mons);
    mons->bind_spell_flags();

    // Reset monster enchantments.
    mons->enchantments.clear();
    mons->ench_cache.reset();
    mons->ench_countdown = 0;

    // NOTE: For player ghosts and (very) ugly things this just ensures
    // that the monster instance is valid and won't crash when used,
    // though the (very) ugly thing generated should actually work.  The
    // player ghost and (very) ugly thing code is currently only used
    // for generating a monster for MonsterMenuEntry in
    // _find_description() in command.cc.
    switch (mcls)
    {
    case MONS_PANDEMONIUM_LORD:
    {
        ghost_demon ghost;
        ghost.init_random_demon();
        mons->set_ghost(ghost);
        mons->ghost_demon_init();
        mons->flags |= MF_INTERESTING;
        mons->bind_melee_flags();
        mons->bind_spell_flags();
        break;
    }

    case MONS_PLAYER_GHOST:
    case MONS_PLAYER_ILLUSION:
    {
        ghost_demon ghost;
        ghost.init_player_ghost();
        mons->set_ghost(ghost);
        mons->ghost_init(!mons->props.exists("fake"));
        mons->bind_melee_flags();
        mons->bind_spell_flags();
        if (mons->ghost->species == SP_DEEP_DWARF)
            mons->flags |= MF_NO_REGEN;
        break;
    }

    case MONS_UGLY_THING:
    case MONS_VERY_UGLY_THING:
    {
        ghost_demon ghost;
        ghost.init_ugly_thing(mcls == MONS_VERY_UGLY_THING);
        mons->set_ghost(ghost);
        mons->uglything_init();
        break;
    }

    // Load with dummy values so certain monster properties can be queried
    // before placement without crashing (proper setup is done later here)
    case MONS_DANCING_WEAPON:
    case MONS_SPECTRAL_WEAPON:
    {
        ghost_demon ghost;
        mons->set_ghost(ghost);
    }

    default:
        break;
    }

    mons->calc_speed();
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

static const char *drac_colour_names[] =
{
    "black", "mottled", "yellow", "green", "purple", "red", "white", "grey", "pale"
};

string draconian_colour_name(monster_type mon_type)
{
    COMPILE_CHECK(ARRAYSZ(drac_colour_names) ==
                  MONS_PALE_DRACONIAN - MONS_DRACONIAN);

    if (mon_type < MONS_BLACK_DRACONIAN || mon_type > MONS_PALE_DRACONIAN)
        return "buggy";

    return drac_colour_names[mon_type - MONS_BLACK_DRACONIAN];
}

monster_type draconian_colour_by_name(const string &name)
{
    COMPILE_CHECK(ARRAYSZ(drac_colour_names)
                  == (MONS_PALE_DRACONIAN - MONS_DRACONIAN));

    for (unsigned i = 0; i < ARRAYSZ(drac_colour_names); ++i)
    {
        if (name == drac_colour_names[i])
            return static_cast<monster_type>(i + MONS_BLACK_DRACONIAN);
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
    if (me == NULL)
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

static string _get_proper_monster_name(const monster* mon)
{
    const monsterentry *me = mon->find_monsterentry();
    if (!me)
        return "";

    string name = getRandNameString(me->name, " name");
    if (!name.empty())
        return name;

    name = getRandNameString(get_monster_data(mons_genus(mon->type))->name,
                             " name");

    if (!name.empty())
        return name;

    name = getRandNameString("generic_monster_name");
    return name;
}

// Names a previously unnamed monster.
bool give_monster_proper_name(monster* mon, bool orcs_only)
{
    // Already has a unique name.
    if (mon->is_named())
        return false;

    // Since this is called from the various divine blessing routines,
    // don't bless non-orcs, and normally don't bless plain orcs, either.
    if (orcs_only)
    {
        if (mons_genus(mon->type) != MONS_ORC
            || mon->type == MONS_ORC && !one_chance_in(8))
        {
            return false;
        }
    }

    mon->mname = _get_proper_monster_name(mon);

    if (mon->friendly())
        take_note(Note(NOTE_NAMED_ALLY, 0, 0, mon->mname.c_str()));

    return mon->is_named();
}

// See mons_init for initialization of mon_entry array.
monsterentry *get_monster_data(monster_type mc)
{
    if (mc >= 0 && mc < NUM_MONSTERS)
        return &mondata[mon_entry[mc]];
    else
        return NULL;
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

int mons_class_zombie_base_speed(monster_type zombie_base_mc)
{
    return max(3, mons_class_base_speed(zombie_base_mc) - 2);
}

int mons_base_speed(const monster* mon)
{
    if (mon->ghost.get())
        return mon->ghost->speed;

    if (mon->type == MONS_SPECTRAL_THING)
        return mons_class_base_speed(mons_zombie_base(mon));

    return mons_is_zombified(mon) ? mons_class_zombie_base_speed(mons_zombie_base(mon))
                                  : mons_class_base_speed(mon->type);
}

mon_intel_type mons_class_intel(monster_type mc)
{
    ASSERT_smc();
    return smc->intel;
}

mon_intel_type mons_intel(const monster* mon)
{
    _get_tentacle_head(mon);

    if (mons_enslaved_soul(mon))
        return mons_class_intel(mons_zombie_base(mon));

    return mons_class_intel(mon->type);
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
        if (ht == HT_LAND && st >= SIZE_GIANT || mc == MONS_GREY_DRACONIAN)
            ht = HT_AMPHIBIOUS;
    }
    return ht;
}

habitat_type mons_habitat(const monster* mon, bool real_amphibious)
{
    return _mons_class_habitat(mons_base_type(mon), real_amphibious);
}

habitat_type mons_class_primary_habitat(monster_type mc)
{
    habitat_type ht = _mons_class_habitat(mc);
    if (ht == HT_AMPHIBIOUS)
        ht = HT_LAND;
    return ht;
}

habitat_type mons_primary_habitat(const monster* mon)
{
    return mons_class_primary_habitat(mons_base_type(mon));
}

habitat_type mons_class_secondary_habitat(monster_type mc)
{
    habitat_type ht = _mons_class_habitat(mc);
    if (ht == HT_AMPHIBIOUS)
        ht = HT_WATER;
    else if (ht == HT_ROCK)
        ht = HT_LAND;
    else if (ht == HT_INCORPOREAL)
        ht = HT_ROCK;
    return ht;
}

habitat_type mons_secondary_habitat(const monster* mon)
{
    return mons_class_secondary_habitat(mons_base_type(mon));
}

bool intelligent_ally(const monster* mon)
{
    return mon->attitude == ATT_FRIENDLY && mons_intel(mon) >= I_NORMAL;
}

int mons_power(monster_type mc)
{
    // For now, just return monster hit dice.
    ASSERT_smc();
    return mons_class_hit_dice(mc);
}

bool mons_aligned(const actor *m1, const actor *m2)
{
    mon_attitude_type fr1, fr2;
    const monster* mon1, *mon2;

    if (!m1 || !m2)
        return true;

    if (m1->is_player())
        fr1 = ATT_FRIENDLY;
    else
    {
        mon1 = static_cast<const monster* >(m1);
        if (mons_is_projectile(mon1->type))
            return true;
        fr1 = mons_attitude(mon1);
    }

    if (m2->is_player())
        fr2 = ATT_FRIENDLY;
    else
    {
        mon2 = static_cast<const monster* >(m2);
        if (mons_is_projectile(mon2->type))
            return true;
        fr2 = mons_attitude(mon2);
    }

    return mons_atts_aligned(fr1, fr2);
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

bool mons_wields_two_weapons(const monster* mon)
{
    if (testbits(mon->flags, MF_TWO_WEAPONS))
        return true;

    return mons_class_wields_two_weapons(mons_base_type(mon));
}

bool mons_self_destructs(const monster* m)
{
    return m->type == MONS_GIANT_SPORE
        || m->type == MONS_BALL_LIGHTNING
        || m->type == MONS_LURKING_HORROR
        || m->type == MONS_ORB_OF_DESTRUCTION
        || m->type == MONS_FULMINANT_PRISM;
}

bool mons_att_wont_attack(mon_attitude_type fr)
{
    return fr == ATT_FRIENDLY || fr == ATT_GOOD_NEUTRAL
           || fr == ATT_STRICT_NEUTRAL;
}

mon_attitude_type mons_attitude(const monster* m)
{
    if (m->friendly())
        return ATT_FRIENDLY;
    else if (m->good_neutral())
        return ATT_GOOD_NEUTRAL;
    else if (m->strict_neutral())
        return ATT_STRICT_NEUTRAL;
    else if (m->neutral())
        return ATT_NEUTRAL;
    else
        return ATT_HOSTILE;
}

bool mons_is_confused(const monster* m, bool class_too)
{
    return (m->has_ench(ENCH_CONFUSION) || m->has_ench(ENCH_MAD))
           && (class_too || !mons_class_flag(m->type, M_CONFUSED));
}

bool mons_is_wandering(const monster* m)
{
    return m->behaviour == BEH_WANDER;
}

bool mons_is_seeking(const monster* m)
{
    return m->behaviour == BEH_SEEK;
}

bool mons_is_fleeing(const monster* m)
{
    return m->behaviour == BEH_FLEE || mons_class_flag(m->type, M_FLEEING);
}

bool mons_is_retreating(const monster* m)
{
    return m->behaviour == BEH_RETREAT || mons_is_fleeing(m);
}

bool mons_is_panicking(const monster* m)
{
    return m->behaviour == BEH_PANIC;
}

bool mons_is_cornered(const monster* m)
{
    return m->behaviour == BEH_CORNERED;
}

bool mons_is_lurking(const monster* m)
{
    return m->behaviour == BEH_LURK;
}

bool mons_is_influenced_by_sanctuary(const monster* m)
{
    return !m->wont_attack() && !m->is_stationary();
}

bool mons_is_fleeing_sanctuary(const monster* m)
{
    return mons_is_influenced_by_sanctuary(m)
           && in_bounds(env.sanctuary_pos)
           && (m->flags & MF_FLEEING_FROM_SANCTUARY);
}

bool mons_just_slept(const monster* m)
{
    return m->flags & MF_JUST_SLEPT;
}

// Moving body parts, turning oklob flowers and so on counts as motile here.
// So does preparing resurrect, struggling against a net, etc.
bool mons_is_immotile(const monster* mons)
{
    return mons_is_firewood(mons)
        || mons->petrified()
        || mons->asleep()
        || mons->paralysed();
}

bool mons_is_batty(const monster* m)
{
    return mons_class_flag(m->type, M_BATTY)
        || m->type == MONS_CHIMERA && chimera_is_batty(m);
}

bool mons_looks_stabbable(const monster* m)
{
    const unchivalric_attack_type uat = is_unchivalric_attack(&you, m);
    return !m->friendly()
           && (uat == UCAT_PARALYSED || uat == UCAT_SLEEPING);
}

bool mons_looks_distracted(const monster* m)
{
    const unchivalric_attack_type uat = is_unchivalric_attack(&you, m);
    return !m->friendly()
           && uat != UCAT_NO_ATTACK
           && uat != UCAT_PARALYSED
           && uat != UCAT_SLEEPING;
}

void mons_start_fleeing_from_sanctuary(monster* mons)
{
    mons->flags |= MF_FLEEING_FROM_SANCTUARY;
    mons->target = env.sanctuary_pos;
    behaviour_event(mons, ME_SCARE, 0, env.sanctuary_pos);
}

void mons_stop_fleeing_from_sanctuary(monster* mons)
{
    const bool had_flag = (mons->flags & MF_FLEEING_FROM_SANCTUARY);
    mons->flags &= (~MF_FLEEING_FROM_SANCTUARY);
    if (had_flag)
        behaviour_event(mons, ME_EVAL, &you);
}

void mons_pacify(monster* mon, mon_attitude_type att, bool no_xp)
{
    // If the _real_ (non-charmed) attitude is already that or better,
    // don't degrade it.  This can happen, for example, with a high-power
    // Crusade card on Pikel's slaves who would then go down from friendly
    // to good_neutral when you kill Pikel.
    if (mon->attitude >= att)
        return;

    // Must be done before attitude change, so that proper targets are affected
    if (mon->type == MONS_FLAYED_GHOST)
        end_flayed_effect(mon);

    // Make the monster permanently neutral.
    mon->attitude = att;
    mon->flags |= MF_WAS_NEUTRAL;

    if (!testbits(mon->flags, MF_GOT_HALF_XP) && !no_xp
        && !mon->is_summoned()
        && !testbits(mon->flags, MF_NO_REWARD))
    {
        // Give the player half of the monster's XP.
        gain_exp((exper_value(mon) + 1) / 2);
        mon->flags |= MF_GOT_HALF_XP;
    }

    if (mon->type == MONS_GERYON)
    {
        simple_monster_message(mon,
            make_stringf(" discards %s horn.",
                         mon->pronoun(PRONOUN_POSSESSIVE).c_str()).c_str());
        monster_drop_things(mon, false, item_is_horn_of_geryon);
    }

    // End constriction.
    mon->stop_constricting_all(false);
    mon->stop_being_constricted();

    // Cancel fleeing and such.
    mon->behaviour = BEH_WANDER;

    // Remove haunting, which would otherwise cause monster to continue attacking
    mon->del_ench(ENCH_HAUNTING, true, true);

    // Make the monster begin leaving the level.
    behaviour_event(mon, ME_EVAL);

    if (mons_is_pikel(mon))
        pikel_band_neutralise();
    if (mons_is_elven_twin(mon))
        elven_twins_pacify(mon);
    if (mons_is_kirke(mon))
        hogs_to_humans();
    if (mon->type == MONS_VAULT_WARDEN)
        timeout_terrain_changes(0, true);

    mons_att_changed(mon);
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
    case BEAM_INVISIBILITY:
    case BEAM_MIGHT:
        return true;

    default:
        return false;
    }
}

bool mons_should_fire(bolt &beam)
{
    dprf("tracer: foes %d (pow: %d), friends %d (pow: %d), "
         "foe_ratio: %d, smart: %s",
         beam.foe_info.count, beam.foe_info.power,
         beam.friend_info.count, beam.friend_info.power,
         beam.foe_ratio, beam.smart_monster ? "yes" : "no");

    // Use different evaluation criteria if the beam is a beneficial
    // enchantment (haste other).
    if (_beneficial_beam_flavour(beam.flavour))
        return _mons_should_fire_beneficial(beam);

    // Friendly monsters shouldn't be targeting you: this will happen
    // often because the default behaviour for charmed monsters is to
    // have you as a target.  While foe_ratio will handle this, we
    // don't want a situation where a friendly dragon breathes through
    // you to hit other creatures... it should target the other
    // creatures, and coincidentally hit you.
    //
    // FIXME: this can cause problems with reflection, bounces, etc.
    // It would be better to have the monster fire logic never reach
    // this point for friendlies.
    if (!invalid_monster_index(beam.beam_source))
    {
        monster& m = menv[beam.beam_source];
        if (m.alive() && m.friendly() && beam.target == you.pos())
            return false;
    }

    // Use of foeRatio:
    // The higher this number, the more monsters will _avoid_ collateral
    // damage to their friends.
    // Setting this to zero will in fact have all monsters ignore their
    // friends when considering collateral damage.

    // Quick check - did we in fact get any foes?
    if (beam.foe_info.count == 0)
        return false;

    if (is_sanctuary(you.pos()) || is_sanctuary(beam.source))
        return false;

    // If we hit no friends, fire away.
    if (beam.friend_info.count == 0)
        return true;

    // Only fire if they do acceptably low collateral damage.
    return beam.foe_info.power >=
           div_round_up(beam.foe_ratio *
                        (beam.foe_info.power + beam.friend_info.power),
                        100);
}

static bool _ms_los_spell(spell_type monspell)
{
    // True, the tentacles _are_ summoned, but they are restricted to
    // water just like the kraken is, so it makes more sense not to
    // count them here.
    if (monspell == SPELL_CREATE_TENTACLES)
        return false;

    if (monspell == SPELL_SMITING
        || monspell == SPELL_AIRSTRIKE
        || monspell == SPELL_HAUNT
        || monspell == SPELL_SUMMON_SPECTRAL_ORCS
        || spell_typematch(monspell, SPTYP_SUMMONING))
    {
        return true;
    }

    return false;
}

static bool _ms_ranged_spell(spell_type monspell, bool attack_only = false,
                             bool ench_too = true)
{
    // Check for Smiting specially, so it's not filtered along
    // with the summon spells.
    if (attack_only
        && (monspell == SPELL_SMITING || monspell == SPELL_AIRSTRIKE))
    {
        return true;
    }

    // These spells are ranged, but aren't direct attack spells.
    if (_ms_los_spell(monspell))
        return !attack_only;

    switch (monspell)
    {
    case SPELL_NO_SPELL:
    case SPELL_CANTRIP:
    case SPELL_FRENZY:
    case SPELL_HASTE:
    case SPELL_MIGHT:
    case SPELL_MINOR_HEALING:
    case SPELL_MAJOR_HEALING:
    case SPELL_TELEPORT_SELF:
    case SPELL_INVISIBILITY:
    case SPELL_BLINK:
    case SPELL_BLINK_CLOSE:
    case SPELL_BLINK_RANGE:
    case SPELL_BERSERKER_RAGE:
    case SPELL_SWIFTNESS:
        return false;

    // The animation spells don't work through transparent walls and
    // thus are listed here instead of above.
    case SPELL_ANIMATE_DEAD:
    case SPELL_ANIMATE_SKELETON:
        return !attack_only;

    // XXX: can this list not be hard-coded to prevent problems in the future?
    case SPELL_CORONA:
    case SPELL_CONFUSE:
    case SPELL_SLOW:
    case SPELL_PARALYSE:
    case SPELL_SLEEP:
    case SPELL_HIBERNATION:
    case SPELL_CAUSE_FEAR:
    case SPELL_LEDAS_LIQUEFACTION:
    case SPELL_MESMERISE:
    case SPELL_MASS_CONFUSION:
    case SPELL_ENGLACIATION:
    case SPELL_TELEPORT_OTHER:
    case SPELL_BLINK_OTHER_CLOSE:
    case SPELL_BLINK_AWAY:
    case SPELL_BLINK_OTHER:
        return ench_too;

    default:
        // All conjurations count as ranged spells.
        return true;
    }
}

// Returns true if the monster has an ability that only needs LOS to
// affect the target.
bool mons_has_los_ability(monster_type mon_type)
{
    // These eyes only need LOS, as well.  (The other eyes use spells.)
    if (mon_type == MONS_GIANT_EYEBALL
        || mon_type == MONS_EYE_OF_DRAINING
        || mon_type == MONS_GOLDEN_EYE
        || mon_type == MONS_MOTH_OF_WRATH)
    {
        return true;
    }

    // Although not using spells, these are exceedingly dangerous.
    if (mon_type == MONS_SILVER_STATUE || mon_type == MONS_ORANGE_STATUE)
        return true;

    // Beholding just needs LOS.
    if (mons_genus(mon_type) == MONS_MERMAID)
        return true;

    return false;
}

bool mons_has_ranged_spell(const monster* mon, bool attack_only,
                           bool ench_too)
{
    // Monsters may have spell-like abilities.
    if (mons_has_los_ability(mon->type))
        return true;

    if (mon->can_use_spells())
    {
        for (int i = 0; i < NUM_MONSTER_SPELL_SLOTS; ++i)
            if (_ms_ranged_spell(mon->spells[i], attack_only, ench_too))
                return true;
    }

    return false;
}

// Returns whether the monster has a spell which is theoretically capable of
// causing an incapacitation state in the target foe (ie: it can cast sleep
// and the foe is not immune to being put to sleep)
//
// Note that this only current checks for inherent obvious immunity (ie: sleep
// immunity from being undead) and not immunity that might be granted by gear
// (such as an amulet of clarity or stasis)
bool mons_has_incapacitating_spell(const monster* mon, const actor* foe)
{
    if (mon->can_use_spells())
    {
        for (int i = 0; i < NUM_MONSTER_SPELL_SLOTS; ++i)
        {
            switch(mon->spells[i])
            {
                case SPELL_SLEEP:
                    if (foe->can_sleep())
                        return true;
                    break;

                case SPELL_HIBERNATION:
                    if (foe->can_hibernate(false, true))
                        return true;
                    break;

                case SPELL_CONFUSE:
                case SPELL_MASS_CONFUSION:
                case SPELL_PARALYSE:
                    return true;

                case SPELL_PETRIFY:
                    if (foe->res_petrify())
                        return true;
                    break;

                default:
                    break;
            }
        }
    }

    return false;
}

static bool _mons_has_ranged_ability(const monster* mon)
{
    // [ds] FIXME: Get rid of special abilities and remove this.
    switch (mon->type)
    {
    case MONS_ACID_BLOB:
    case MONS_BURNING_BUSH:
    case MONS_DRACONIAN:
    case MONS_FIRE_DRAGON:
    case MONS_ICE_DRAGON:
    case MONS_HELL_HOUND:
    case MONS_LINDWURM:
    case MONS_FIRE_DRAKE:
    case MONS_XTAHUA:
    case MONS_FIRE_CRAB:
    case MONS_ELECTRIC_EEL:
    case MONS_LAVA_SNAKE:
    case MONS_MANTICORE:
    case MONS_OKLOB_PLANT:
    case MONS_OKLOB_SAPLING:
    case MONS_LIGHTNING_SPIRE:
        return true;
    default:
        return false;
    }
}

static bool _mons_has_usable_ranged_weapon(const monster* mon)
{
    // Ugh.
    monster* mnc = const_cast<monster* >(mon);
    const item_def *weapon  = mnc->launcher();
    const item_def *primary = mnc->mslot_item(MSLOT_WEAPON);
    const item_def *missile = mnc->missiles();

    // We don't have a usable ranged weapon if a different cursed weapon
    // is presently equipped.
    if (weapon != primary && primary && primary->cursed())
        return false;

    if (!missile)
        return false;

    return is_launched(mnc, weapon, *missile);
}

bool mons_has_ranged_attack(const monster* mon)
{
    return mons_has_ranged_spell(mon, true) || _mons_has_ranged_ability(mon)
           || _mons_has_usable_ranged_weapon(mon);
}

bool mons_has_incapacitating_ranged_attack(const monster* mon, const actor* foe)
{
    if (_mons_has_usable_ranged_weapon(mon))
    {
        monster* mnc = const_cast<monster* >(mon);
        const item_def *missile = mnc->missiles();

        if (missile && missile->sub_type == MI_THROWING_NET)
            return true;
        else if (missile && missile->sub_type == MI_NEEDLE)
        {
            switch (get_ammo_brand(*missile))
            {
                // Not actually incapacitating, but marked as such so that
                // assassins will prefer using it while ammo remains
                case SPMSL_CURARE:
                    if (!foe->res_poison())
                        return true;

                case SPMSL_SLEEP:
                    if (foe->can_sleep())
                        return true;
                    break;

                case SPMSL_CONFUSION:
                case SPMSL_PARALYSIS:
                    return true;

                default:
                    break;
            }
        }
    }

    return false;
}

static bool _mons_starts_with_ranged_weapon(monster_type mc)
{
    switch (mc)
    {
    case MONS_JOSEPH:
    case MONS_DEEP_ELF_MASTER_ARCHER:
    case MONS_CENTAUR:
    case MONS_CENTAUR_WARRIOR:
    case MONS_NESSOS:
    case MONS_YAKTAUR:
    case MONS_YAKTAUR_CAPTAIN:
    case MONS_CHERUB:
    case MONS_SONJA:
    case MONS_HAROLD:
    case MONS_POLYPHEMUS:
    case MONS_CYCLOPS:
    case MONS_STONE_GIANT:
    case MONS_CHUCK:
    case MONS_MERFOLK_JAVELINEER:
    case MONS_URUG:
    case MONS_FAUN:
    case MONS_SATYR:
        return true;
    default:
        return false;
    }
}

bool mons_has_known_ranged_attack(const monster* mon)
{
    return _mons_has_ranged_ability(mon)
        || mon->flags & MF_SEEN_RANGED
        || _mons_starts_with_ranged_weapon(mon->type)
            && !(mon->flags & MF_KNOWN_SHIFTER);
}

bool mons_can_attack(const monster* mon)
{
    const actor* foe = mon->get_foe();
    if (!foe || !mon->can_see(foe))
        return false;

    if (mons_has_ranged_attack(mon) && mon->see_cell_no_trans(foe->pos()))
        return true;

    return adjacent(mon->pos(), foe->pos());
}

static gender_type _mons_class_gender(monster_type mc)
{
    gender_type gender = GENDER_NEUTER;

    if (mons_genus(mc) == MONS_MERMAID
        || mc == MONS_QUEEN_ANT
        || mc == MONS_QUEEN_BEE
        || mc == MONS_HARPY
        || mc == MONS_SPHINX
        || mc == MONS_WATER_NYMPH)
    {
        gender = GENDER_FEMALE;
    }
    // Mara's fakes aren't unique, but should still be classified as
    // male.
    else if (mc == MONS_MARA_FAKE
             || mc == MONS_HELLBINDER
             || mc == MONS_CLOUD_MAGE)
    {
        gender = GENDER_MALE;
    }
    else if (mons_is_unique(mc) && !mons_is_pghost(mc))
    {
        if (mons_species(mc) == MONS_SERPENT_OF_HELL)
            mc = MONS_SERPENT_OF_HELL;
        switch (mc)
        {
        case MONS_JESSICA:
        case MONS_PSYCHE:
        case MONS_JOSEPHINE:
        case MONS_AGNES:
        case MONS_MAUD:
        case MONS_LOUISE:
        case MONS_FRANCES:
        case MONS_MARGERY:
        case MONS_EROLCHA:
        case MONS_ERICA:
        case MONS_TIAMAT:
        case MONS_ERESHKIGAL:
        case MONS_ROXANNE:
        case MONS_SONJA:
        case MONS_ILSUIW:
        case MONS_NERGALLE:
        case MONS_KIRKE:
        case MONS_DUVESSA:
        case MONS_THE_ENCHANTRESS:
        case MONS_NELLIE:
        case MONS_ARACHNE:
        case MONS_LAMIA:
            gender = GENDER_FEMALE;
            break;
        case MONS_ROYAL_JELLY:
        case MONS_LERNAEAN_HYDRA:
        case MONS_IRON_GIANT:
        case MONS_SERPENT_OF_HELL:
            gender = GENDER_NEUTER;
            break;
        default:
            gender = GENDER_MALE;
            break;
        }
    }

    return gender;
}

// Use of variant (case is irrelevant here):
// PRONOUN_SUBJECTIVE : _She_ is tap dancing.
// PRONOUN_POSSESSIVE : _Her_ sword explodes!
// PRONOUN_REFLEXIVE  : The wizard mumbles to _herself_.
// PRONOUN_OBJECTIVE  : You miss _her_.

const char *mons_pronoun(monster_type mon_type, pronoun_type variant,
                         bool visible)
{
    gender_type gender = !visible ? GENDER_NEUTER
                                  : _mons_class_gender(mon_type);

    switch (variant)
    {
    case PRONOUN_SUBJECTIVE:
        return gender == GENDER_NEUTER ? "it" :
               gender == GENDER_MALE   ? "he" : "she";

    case PRONOUN_POSSESSIVE:
        return gender == GENDER_NEUTER ? "its" :
               gender == GENDER_MALE   ? "his" : "her";

    case PRONOUN_REFLEXIVE:
        return gender == GENDER_NEUTER ? "itself"  :
               gender == GENDER_MALE   ? "himself" : "herself";

    case PRONOUN_OBJECTIVE:
        return gender == GENDER_NEUTER ? "it"  :
               gender == GENDER_MALE   ? "him" : "her";
    }

    return "";
}

// Checks if the monster can use smiting/torment to attack without
// unimpeded LOS to the player.
static bool _mons_has_smite_attack(const monster* mons)
{
    const monster_spells &hspell_pass = mons->spells;
    for (unsigned i = 0; i < hspell_pass.size(); ++i)
        if (hspell_pass[i] == SPELL_SYMBOL_OF_TORMENT
            || hspell_pass[i] == SPELL_SMITING
            || hspell_pass[i] == SPELL_HELLFIRE_BURST
            || hspell_pass[i] == SPELL_FIRE_STORM
            || hspell_pass[i] == SPELL_AIRSTRIKE)
        {
            return true;
        }

    return false;
}

// Determines if a monster is smart and pushy enough to displace other
// monsters.  A shover should not cause damage to the shovee by
// displacing it, so monsters that trail clouds of badness are
// ineligible.  The shover should also benefit from shoving, so monsters
// that can smite/torment are ineligible.
//
// (Smiters would be eligible for shoving when fleeing if the AI allowed
// for smart monsters to flee.)
bool monster_shover(const monster* m)
{
    const monsterentry *me = get_monster_data(m->type);
    if (!me)
        return false;

    // Efreet and fire elementals are disqualified because they leave behind
    // clouds of flame. Rotting devils are disqualified because they trail
    // clouds of miasma.
    if (mons_genus(m->type) == MONS_EFREET || m->type == MONS_FIRE_ELEMENTAL
        || m->type == MONS_ROTTING_DEVIL || m->type == MONS_CURSE_TOE)
    {
        return false;
    }

    // Monsters too stupid to use stairs (e.g. non-spectral zombified undead)
    // are also disqualified.
    if (!mons_can_use_stairs(m))
        return false;

    // Smiters profit from staying back and smiting.
    if (_mons_has_smite_attack(m))
        return false;

    char mchar = me->basechar;

    // Somewhat arbitrary: giants and dragons are too big to get past anything,
    // beetles are too dumb (arguable), dancing weapons can't communicate, eyes
    // aren't pushers and shovers. Worms and elementals are on the list because
    // all 'w' are currently unrelated.
    return mchar != 'C' && mchar != 'B' && mchar != '(' && mchar != 'D'
           && mchar != 'G' && mchar != 'w' && mchar != 'E';
}

// Returns true if m1 and m2 are related, and m1 is higher up the totem pole
// than m2. The criteria for being related are somewhat loose, as you can see
// below.
bool monster_senior(const monster* m1, const monster* m2, bool fleeing)
{
    const monsterentry *me1 = get_monster_data(m1->type),
                       *me2 = get_monster_data(m2->type);

    if (!me1 || !me2)
        return false;

    // Fannar's ice beasts can push past Fannar, who benefits from this.
    // Similarly, he refuses to push past them unless he's fleeing.
    if (m1->type == MONS_FANNAR && m2->type == MONS_ICE_BEAST)
        return fleeing;
    if (m1->type == MONS_ICE_BEAST && m2->type == MONS_FANNAR)
        return true;

    // Band leaders can displace followers regardless of type considerations.
    // -cao
    if (m2->props.exists("band_leader"))
    {
        unsigned leader_mid = m2->props["band_leader"].get_int();
        if (leader_mid == m1->mid)
            return true;
    }
    // And prevent followers to displace the leader to avoid constant swapping.
    else if (m1->props.exists("band_leader"))
    {
        unsigned leader_mid = m1->props["band_leader"].get_int();
        if (leader_mid == m2->mid)
            return false;
    }

    char mchar1 = me1->basechar;
    char mchar2 = me2->basechar;

    // If both are demons, the smaller number is the nastier demon.
    if (isadigit(mchar1) && isadigit(mchar2))
        return fleeing || mchar1 < mchar2;

    // &s are the evillest demons of all, well apart from Geryon, who really
    // profits from *not* pushing past beasts.
    if (mchar1 == '&' && isadigit(mchar2) && m1->type != MONS_GERYON)
        return fleeing || m1->hit_dice > m2->hit_dice;

    // If they're the same holiness, monsters smart enough to use stairs can
    // push past monsters too stupid to use stairs (so that e.g. non-zombified
    // or spectral zombified undead can push past non-spectral zombified
    // undead).
    if (m1->holiness() == m2->holiness() && mons_can_use_stairs(m1)
        && !mons_can_use_stairs(m2))
    {
        return true;
    }

    if (mons_genus(m1->type) == MONS_KILLER_BEE
        && mons_genus(m2->type) == MONS_KILLER_BEE)
    {
        if (fleeing)
            return true;

        if (m1->type == MONS_QUEEN_BEE && m2->type != MONS_QUEEN_BEE)
            return true;
    }

    // Special-case gnolls, so they can't get past (hob)goblins.
    if (mons_species(m1->type) == MONS_GNOLL
        && mons_species(m2->type) != MONS_GNOLL)
    {
        return false;
    }

    // Special-case (non-enslaved soul) spectral things to push past revenants.
    if ((m1->type == MONS_SPECTRAL_THING && !mons_enslaved_soul(m1))
        && m2->type == MONS_REVENANT)
    {
        return true;
    }

    return mchar1 == mchar2 && (fleeing || m1->hit_dice > m2->hit_dice);
}

bool mons_class_can_pass(monster_type mc, const dungeon_feature_type grid)
{
    if (grid == DNGN_MALIGN_GATEWAY)
    {
        return mc == MONS_ELDRITCH_TENTACLE
               || mc == MONS_ELDRITCH_TENTACLE_SEGMENT;
    }

    if (_mons_class_habitat(mc) == HT_INCORPOREAL)
        return !feat_is_permarock(grid);

    if (_mons_class_habitat(mc) == HT_ROCK)
    {
        // Permanent walls can't be passed through.
        return !feat_is_solid(grid)
               || feat_is_rock(grid) && !feat_is_permarock(grid);
    }

    return !feat_is_solid(grid);
}

static bool _mons_can_open_doors(const monster* mon)
{
    return mons_itemuse(mon) >= MONUSE_OPEN_DOORS;
}

// Some functions that check whether a monster can open/eat/pass a
// given door. These all return false if there's no closed door there.
bool mons_can_open_door(const monster* mon, const coord_def& pos)
{
    if (!_mons_can_open_doors(mon))
        return false;

    // Creatures allied with the player can't open sealed doors either
    if (mon->friendly() && grd(pos) == DNGN_SEALED_DOOR)
        return false;

    if (env.markers.property_at(pos, MAT_ANY, "door_restrict") == "veto")
        return false;

    return true;
}

// Monsters that eat items (currently only jellies) also eat doors.
bool mons_can_eat_door(const monster* mon, const coord_def& pos)
{

    if (mons_itemeat(mon) != MONEAT_ITEMS)
        return false;

    if (env.markers.property_at(pos, MAT_ANY, "door_restrict") == "veto")
        return false;

    return true;
}

static bool _mons_can_pass_door(const monster* mon, const coord_def& pos)
{
    return mon->can_pass_through_feat(DNGN_FLOOR)
           && (mons_can_open_door(mon, pos)
               || mons_can_eat_door(mon, pos));
}

bool mons_can_traverse(const monster* mon, const coord_def& p,
                       bool checktraps)
{
    if ((grd(p) == DNGN_CLOSED_DOOR
        || grd(p) == DNGN_SEALED_DOOR)
            && _mons_can_pass_door(mon, p))
    {
        return true;
    }

    if (!mon->is_habitable(p))
        return false;

    const trap_def* ptrap = find_trap(p);
    if (checktraps && ptrap)
    {
        const trap_type tt = ptrap->type;

        // Don't allow allies to pass over known (to them) Zot traps.
        if (tt == TRAP_ZOT
            && ptrap->is_known(mon)
            && mon->friendly())
        {
            return false;
        }

        // Monsters cannot travel over teleport traps.
        if (!can_place_on_trap(mons_base_type(mon), tt))
            return false;
    }

    return true;
}

void mons_remove_from_grid(const monster* mon)
{
    const coord_def pos = mon->pos();
    if (map_bounds(pos) && mgrd(pos) == mon->mindex())
        mgrd(pos) = NON_MONSTER;
}

mon_inv_type equip_slot_to_mslot(equipment_type eq)
{
    switch (eq)
    {
    case EQ_WEAPON:      return MSLOT_WEAPON;
    case EQ_BODY_ARMOUR: return MSLOT_ARMOUR;
    case EQ_SHIELD:      return MSLOT_SHIELD;
    case EQ_RINGS:
    case EQ_AMULET:      return MSLOT_JEWELLERY;
    default: return NUM_MONSTER_SLOTS;
    }
}

mon_inv_type item_to_mslot(const item_def &item)
{
    switch (item.base_type)
    {
    case OBJ_WEAPONS:
    case OBJ_STAVES:
    case OBJ_RODS:
        return MSLOT_WEAPON;
    case OBJ_MISSILES:
        return MSLOT_MISSILE;
    case OBJ_ARMOUR:
        return equip_slot_to_mslot(get_armour_slot(item));
    case OBJ_WANDS:
        return MSLOT_WAND;
    case OBJ_SCROLLS:
        return MSLOT_SCROLL;
    case OBJ_POTIONS:
        return MSLOT_POTION;
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
                      MONS_DEATH_OOZE,
                         -1);
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
        result += (god == GOD_NO_GOD) ? "are" : "is";
    }

    return result;
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

static string _pluralise_player_genus()
{
    string sp = species_name(you.species, true, false);
    if (player_genus(GENPC_ELVEN, you.species)
        || you.species == SP_DEEP_DWARF)
    {
        sp = sp.substr(0, sp.find("f"));
        sp += "ves";
    }
    else if (you.species != SP_DEMONSPAWN)
        sp += "s";

    return sp;
}

// Replaces the "@foo@" strings in monster shout and monster speak
// definitions.
string do_mon_str_replacements(const string &in_msg, const monster* mons,
                               int s_type)
{
    string msg = in_msg;

    const actor*    foe   = (mons->wont_attack()
                             && invalid_monster_index(mons->foe)) ?
                                &you : mons->get_foe();

    if (s_type < 0 || s_type >= NUM_LOUDNESS || s_type == NUM_SHOUTS)
        s_type = mons_shouts(mons->type);

    msg = maybe_pick_random_substring(msg);

    // FIXME: Handle player_genus in case it was not generalised to foe_genus.
    msg = replace_all(msg, "@a_player_genus@",
                      article_a(species_name(you.species, true)));
    msg = replace_all(msg, "@player_genus@", species_name(you.species, true));
    msg = replace_all(msg, "@player_genus_plural@", _pluralise_player_genus());

    string foe_species;

    if (foe == NULL)
        ;
    else if (foe->is_player())
    {
        foe_species = species_name(you.species, true);

        msg = replace_all(msg, " @to_foe@", "");
        msg = replace_all(msg, " @at_foe@", "");

        msg = replace_all(msg, "@player_only@", "");
        msg = replace_all(msg, " @foe,@", ",");

        msg = replace_all(msg, "@player", "@foe");
        msg = replace_all(msg, "@Player", "@Foe");

        msg = replace_all(msg, "@foe_possessive@", "your");
        msg = replace_all(msg, "@foe@", "you");
        msg = replace_all(msg, "@Foe@", "You");

        msg = replace_all(msg, "@foe_name@", you.your_name);
        msg = replace_all(msg, "@foe_species@", species_name(you.species));
        msg = replace_all(msg, "@foe_genus@", foe_species);
        msg = replace_all(msg, "@Foe_genus@", uppercase_first(foe_species));
        msg = replace_all(msg, "@foe_genus_plural@",
                          _pluralise_player_genus());
    }
    else
    {
        string foe_name;
        const monster* m_foe = foe->as_monster();
        if (you.can_see(foe) || crawl_state.game_is_arena())
        {
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
        }
        else
            foe_name = "something";

        string prep = "at";
        if (s_type == S_SILENT || s_type == S_SHOUT || s_type == S_NORMAL)
            prep = "to";
        msg = replace_all(msg, "@says@ @to_foe@", "@says@ " + prep + " @foe@");

        msg = replace_all(msg, " @to_foe@", " to @foe@");
        msg = replace_all(msg, " @at_foe@", " at @foe@");
        msg = replace_all(msg, "@foe,@",    "@foe@,");

        msg = replace_all(msg, "@foe_possessive@", "@foe@'s");
        msg = replace_all(msg, "@foe@", foe_name);
        msg = replace_all(msg, "@Foe@", uppercase_first(foe_name));

        if (m_foe->is_named())
            msg = replace_all(msg, "@foe_name@", foe->name(DESC_PLAIN, true));

        string species = mons_type_name(mons_species(m_foe->type), DESC_PLAIN);

        msg = replace_all(msg, "@foe_species@", species);

        string genus = mons_type_name(mons_genus(m_foe->type), DESC_PLAIN);

        msg = replace_all(msg, "@foe_genus@", genus);
        msg = replace_all(msg, "@Foe_genus@", uppercase_first(genus));
        msg = replace_all(msg, "@foe_genus_plural@", pluralise(genus));

        foe_species = genus;
    }

    description_level_type nocap = DESC_THE, cap = DESC_THE;

    if (mons->is_named() && you.can_see(mons))
    {
        const string name = mons->name(DESC_THE);

        msg = replace_all(msg, "@the_something@", name);
        msg = replace_all(msg, "@The_something@", name);
        msg = replace_all(msg, "@the_monster@",   name);
        msg = replace_all(msg, "@The_monster@",   name);
    }
    else if (mons->attitude == ATT_FRIENDLY
             && !mons_is_unique(mons->type)
             && !crawl_state.game_is_arena()
             && you.can_see(mons))
    {
        nocap = DESC_PLAIN;
        cap   = DESC_PLAIN;

        msg = replace_all(msg, "@the_something@", "your @the_something@");
        msg = replace_all(msg, "@The_something@", "Your @The_something@");
        msg = replace_all(msg, "@the_monster@",   "your @the_monster@");
        msg = replace_all(msg, "@The_monster@",   "Your @the_monster@");
    }

    if (you.see_cell(mons->pos()))
    {
        dungeon_feature_type feat = grd(mons->pos());
        if (feat < DNGN_MINMOVE || feat >= NUM_FEATURES)
            msg = replace_all(msg, "@surface@", "buggy surface");
        else if (feat == DNGN_LAVA)
            msg = replace_all(msg, "@surface@", "lava");
        else if (feat_is_water(feat))
            msg = replace_all(msg, "@surface@", "water");
        else if (feat_is_altar(feat))
            msg = replace_all(msg, "@surface@", "altar");
        else
            msg = replace_all(msg, "@surface@", "ground");

        msg = replace_all(msg, "@feature@", raw_feature_description(mons->pos()));
    }
    else
    {
        msg = replace_all(msg, "@surface@", "buggy unseen surface");
        msg = replace_all(msg, "@feature@", "buggy unseen feature");
    }

    if (you.can_see(mons))
    {
        string something = mons->name(DESC_PLAIN);
        msg = replace_all(msg, "@something@",   something);
        msg = replace_all(msg, "@a_something@", mons->name(DESC_A));
        msg = replace_all(msg, "@the_something@", mons->name(nocap));

        something[0] = toupper(something[0]);
        msg = replace_all(msg, "@Something@",   something);
        msg = replace_all(msg, "@A_something@", mons->name(DESC_A));
        msg = replace_all(msg, "@The_something@", mons->name(cap));
    }
    else
    {
        msg = replace_all(msg, "@something@",     "something");
        msg = replace_all(msg, "@a_something@",   "something");
        msg = replace_all(msg, "@the_something@", "something");

        msg = replace_all(msg, "@Something@",     "Something");
        msg = replace_all(msg, "@A_something@",   "Something");
        msg = replace_all(msg, "@The_something@", "Something");
    }

    // Player name.
    msg = replace_all(msg, "@player_name@", you.your_name);

    string plain = mons->name(DESC_PLAIN);
    msg = replace_all(msg, "@monster@",     plain);
    msg = replace_all(msg, "@a_monster@",   mons->name(DESC_A));
    msg = replace_all(msg, "@the_monster@", mons->name(nocap));

    plain[0] = toupper(plain[0]);
    msg = replace_all(msg, "@Monster@",     plain);
    msg = replace_all(msg, "@A_monster@",   mons->name(DESC_A));
    msg = replace_all(msg, "@The_monster@", mons->name(cap));

    msg = replace_all(msg, "@Subjective@",
                      mons->pronoun(PRONOUN_SUBJECTIVE));
    msg = replace_all(msg, "@subjective@",
                      mons->pronoun(PRONOUN_SUBJECTIVE));
    msg = replace_all(msg, "@Possessive@",
                      mons->pronoun(PRONOUN_POSSESSIVE));
    msg = replace_all(msg, "@possessive@",
                      mons->pronoun(PRONOUN_POSSESSIVE));
    msg = replace_all(msg, "@reflexive@",
                      mons->pronoun(PRONOUN_REFLEXIVE));
    msg = replace_all(msg, "@objective@",
                      mons->pronoun(PRONOUN_OBJECTIVE));

    // Body parts.
    bool   can_plural = false;
    string part_str   = mons->hand_name(false, &can_plural);

    msg = replace_all(msg, "@hand@", part_str);
    msg = replace_all(msg, "@Hand@", uppercase_first(part_str));

    if (!can_plural)
        part_str = "NO PLURAL HANDS";
    else
        part_str = mons->hand_name(true);

    msg = replace_all(msg, "@hands@", part_str);
    msg = replace_all(msg, "@Hands@", uppercase_first(part_str));

    can_plural = false;
    part_str   = mons->arm_name(false, &can_plural);

    msg = replace_all(msg, "@arm@", part_str);
    msg = replace_all(msg, "@Arm@", uppercase_first(part_str));

    if (!can_plural)
        part_str = "NO PLURAL ARMS";
    else
        part_str = mons->arm_name(true);

    msg = replace_all(msg, "@arms@", part_str);
    msg = replace_all(msg, "@Arms@", uppercase_first(part_str));

    can_plural = false;
    part_str   = mons->foot_name(false, &can_plural);

    msg = replace_all(msg, "@foot@", part_str);
    msg = replace_all(msg, "@Foot@", uppercase_first(part_str));

    if (!can_plural)
        part_str = "NO PLURAL FOOT";
    else
        part_str = mons->foot_name(true);

    msg = replace_all(msg, "@feet@", part_str);
    msg = replace_all(msg, "@Feet@", uppercase_first(part_str));

    if (foe != NULL)
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

    // The monster's god, not the player's.  Atheists get
    // "NO GOD"/"NO GOD"/"NO_GOD"/"NO_GOD", and worshippers of nameless
    // gods get "a god"/"its god/my God/My God".
    //
    // XXX: Crawl currently has no first-person possessive pronoun;
    // if it gets one, it should be used for the last two entries.
    if (mons->god == GOD_NO_GOD)
    {
        msg = replace_all(msg, "@a_God@", "NO GOD");
        msg = replace_all(msg, "@A_God@", "NO GOD");
        msg = replace_all(msg, "@possessive_God@", "NO GOD");
        msg = replace_all(msg, "@Possessive_God@", "NO GOD");

        msg = replace_all(msg, "@my_God@", "NO GOD");
        msg = replace_all(msg, "@My_God@", "NO GOD");
    }
    else if (mons->god == GOD_NAMELESS)
    {
        msg = replace_all(msg, "@a_God@", "a god");
        msg = replace_all(msg, "@A_God@", "A god");
        const string possessive = mons->pronoun(PRONOUN_POSSESSIVE) + " god";
        msg = replace_all(msg, "@possessive_God@", possessive);
        msg = replace_all(msg, "@Possessive_God@", uppercase_first(possessive));

        msg = replace_all(msg, "@my_God@", "my God");
        msg = replace_all(msg, "@My_God@", "My God");
    }
    else
    {
        const string godname = god_name(mons->god);
        const string godcap = uppercase_first(godname);
        msg = replace_all(msg, "@a_God@", godname);
        msg = replace_all(msg, "@A_God@", godcap);
        msg = replace_all(msg, "@possessive_God@", godname);
        msg = replace_all(msg, "@Possessive_God@", godcap);

        msg = replace_all(msg, "@my_God@", godname);
        msg = replace_all(msg, "@My_God@", godcap);
    }

    // Replace with species specific insults.
    if (msg.find("@species_insult_") != string::npos)
    {
        msg = replace_all(msg, "@species_insult_adj1@",
                               _get_species_insult(foe_species, "adj1"));
        msg = replace_all(msg, "@species_insult_adj2@",
                               _get_species_insult(foe_species, "adj2"));
        msg = replace_all(msg, "@species_insult_noun@",
                               _get_species_insult(foe_species, "noun"));
    }

    static const char * sound_list[] =
    {
        "says",         // actually S_SILENT
        "shouts",
        "barks",
        "shouts",
        "roars",
        "screams",
        "bellows",
        "trumpets",
        "screeches",
        "buzzes",
        "moans",
        "gurgles",
        "whines",
        "croaks",
        "growls",
        "hisses",
        "sneers",       // S_DEMON_TAUNT
        "caws",
        "says",         // S_CHERUB -- they just speak normally.
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

    msg = apostrophise_fixup(msg);

    msg = maybe_capitalise_substring(msg);

    return msg;
}

static mon_body_shape _get_ghost_shape(const monster* mon)
{
    const ghost_demon &ghost = *(mon->ghost);

    switch (ghost.species)
    {
    case SP_NAGA:
        return MON_SHAPE_NAGA;

    case SP_CENTAUR:
        return MON_SHAPE_CENTAUR;

    case SP_RED_DRACONIAN:
    case SP_WHITE_DRACONIAN:
    case SP_GREEN_DRACONIAN:
    case SP_YELLOW_DRACONIAN:
    case SP_GREY_DRACONIAN:
    case SP_BLACK_DRACONIAN:
    case SP_PURPLE_DRACONIAN:
    case SP_MOTTLED_DRACONIAN:
    case SP_PALE_DRACONIAN:
    case SP_BASE_DRACONIAN:
        return MON_SHAPE_HUMANOID_TAILED;

    default:
        return MON_SHAPE_HUMANOID;
    }
}

mon_body_shape get_mon_shape(const monster* mon)
{
    if (mons_is_pghost(mon->type))
        return _get_ghost_shape(mon);
    else if (mons_is_zombified(mon))
        return get_mon_shape(mon->base_monster);
    else
        return get_mon_shape(mon->type);
}

mon_body_shape get_mon_shape(const monster_type mc)
{
    if (mc == MONS_CHAOS_SPAWN)
        return static_cast<mon_body_shape>(random2(MON_SHAPE_MISC + 1));

    const bool flies = (mons_class_flies(mc) == FL_WINGED);

    switch (mons_base_char(mc))
    {
    case 'a': // ants
        if (mons_genus(mc) == MONS_FORMICID)
            return MON_SHAPE_HUMANOID;
        else
            return MON_SHAPE_INSECT;
    case 'b': // bats and butterflies
        if (mc == MONS_BUTTERFLY)
            return MON_SHAPE_INSECT_WINGED;
        else
            return MON_SHAPE_BAT;
    case 'c': // centaurs
        if (mc == MONS_FAUN || mc == MONS_SATYR)
            return MON_SHAPE_HUMANOID_TAILED;
        return MON_SHAPE_CENTAUR;
    case 'd': // draconions and drakes
        if (mons_genus(mc) == MONS_DRACONIAN)
        {
            if (flies)
                return MON_SHAPE_HUMANOID_WINGED_TAILED;
            else
                return MON_SHAPE_HUMANOID_TAILED;
        }
        else if (flies)
            return MON_SHAPE_QUADRUPED_WINGED;
        else
            return MON_SHAPE_QUADRUPED;
    case 'e': // elves
        return MON_SHAPE_HUMANOID;
    case 'f': // fungi
        return MON_SHAPE_FUNGUS;
    case 'g': // gnolls, goblins and hobgoblins
        return MON_SHAPE_HUMANOID;
    case 'h': // hounds
        return MON_SHAPE_QUADRUPED;
    case 'i': // spriggans
        return MON_SHAPE_HUMANOID;
    case 'j': // snails
        return MON_SHAPE_SNAIL;
    case 'k': // killer bees
        return MON_SHAPE_INSECT_WINGED;
    case 'l': // lizards
        return MON_SHAPE_QUADRUPED;
    case 'm': // merfolk
        return MON_SHAPE_HUMANOID;
    case 'n': // necrophages and ghouls
        return MON_SHAPE_HUMANOID;
    case 'o': // orcs
        return MON_SHAPE_HUMANOID;
    case 'p': // ghosts
        if (mc != MONS_INSUBSTANTIAL_WISP
            && !mons_is_pghost(mc)) // handled elsewhere
        {
            return MON_SHAPE_HUMANOID;
        }
        break;
    case 'q': // dwarves
        return MON_SHAPE_HUMANOID;
    case 'r': // rodents
        return MON_SHAPE_QUADRUPED;
    case 's': // arachnids and centipedes/cockroaches
        if (mc == MONS_GIANT_CENTIPEDE || mc == MONS_DEMONIC_CRAWLER)
            return MON_SHAPE_CENTIPEDE;
        else if (mc == MONS_GIANT_COCKROACH)
            return MON_SHAPE_INSECT;
        else
            return MON_SHAPE_ARACHNID;
    case 'u': // mutated type, not enough info to determine shape
        return MON_SHAPE_MISC;
    case 't': // crocodiles/turtles
        if (mons_genus(mc) == MONS_SNAPPING_TURTLE)
            return MON_SHAPE_QUADRUPED_TAILLESS;
        else
            return MON_SHAPE_QUADRUPED;
    case 'v': // vortices
        return MON_SHAPE_MISC;
    case 'w': // worms
        return MON_SHAPE_SNAKE;
    case 'x': // small abominations
        return MON_SHAPE_MISC;
    case 'y': // winged insects
        return MON_SHAPE_INSECT_WINGED;
    case 'z': // small zombies, etc.
        if (mc == MONS_WIGHT
            || mc == MONS_SKELETAL_WARRIOR
            || mc == MONS_ANCIENT_CHAMPION
            || mc == MONS_FLAMING_CORPSE)
        {
            return MON_SHAPE_HUMANOID;
        }
        else
            // constructed type, not enough info to determine shape
            return MON_SHAPE_MISC;
    case 'A': // angelic beings
        return MON_SHAPE_HUMANOID_WINGED;
    case 'B': // beetles
        return MON_SHAPE_INSECT;
    case 'C': // giants
        return MON_SHAPE_HUMANOID;
    case 'D': // dragons
        if (flies)
            return MON_SHAPE_QUADRUPED_WINGED;
        else
            return MON_SHAPE_QUADRUPED;
    case 'E': // elementals
        return MON_SHAPE_MISC;
    case 'F': // frogs
        return MON_SHAPE_QUADRUPED_TAILLESS;
    case 'G': // floating eyeballs and orbs
        return MON_SHAPE_ORB;
    case 'H': // minotaurs, manticores, hippogriffs and griffins
        if (mc == MONS_MINOTAUR || mons_genus(mc) == MONS_TENGU)
            return MON_SHAPE_HUMANOID;
        else if (mc == MONS_MANTICORE)
            return MON_SHAPE_QUADRUPED;
        else if (mc == MONS_HARPY)
            return MON_SHAPE_HUMANOID_WINGED;
        else
            return MON_SHAPE_QUADRUPED_WINGED;
    case 'I': // ice beasts
        return MON_SHAPE_QUADRUPED;
    case 'J': // jellies and jellyfish
        return MON_SHAPE_BLOB;
    case 'K': // kobolds
        return MON_SHAPE_HUMANOID;
    case 'L': // liches
        return MON_SHAPE_HUMANOID;
    case 'M': // mummies
        return MON_SHAPE_HUMANOID;
    case 'N': // nagas
        if (mons_genus(mc) == MONS_GUARDIAN_SERPENT)
            return MON_SHAPE_SNAKE;
        else
            return MON_SHAPE_NAGA;
    case 'O': // ogres
        return MON_SHAPE_HUMANOID;
    case 'P': // plants
        return MON_SHAPE_PLANT;
    case 'R': // rakshasas and efreet; humanoid?
        return MON_SHAPE_HUMANOID;
    case 'S': // snakes
        return MON_SHAPE_SNAKE;
    case 'T': // trolls
        return MON_SHAPE_HUMANOID;
    case 'U': // bears
        return MON_SHAPE_QUADRUPED_TAILLESS;
    case 'V': // vampires
        return MON_SHAPE_HUMANOID;
    case 'W': // wraiths, humanoid if not a spectral thing
        if (mc == MONS_SPECTRAL_THING)
        {
            // constructed type, not enough info to determine shape
            return MON_SHAPE_MISC;
        }
        else
            return MON_SHAPE_HUMANOID;
    case 'X': // large abominations
        return MON_SHAPE_MISC;
    case 'Y': // yaks, sheep and elephants
        if (mc == MONS_SHEEP)
            return MON_SHAPE_QUADRUPED_TAILLESS;
        else
            return MON_SHAPE_QUADRUPED;
    case 'Z': // large zombies, etc.
        // constructed type, not enough info to determine shape
        return MON_SHAPE_MISC;
    case ';': // water monsters
        if (mc == MONS_ELECTRIC_EEL)
            return MON_SHAPE_SNAKE;
        else
            return MON_SHAPE_FISH;

    // The various demons, plus some golems and statues.  And humanoids.
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '&':
    case '8':
    case '@':
    {
        // Assume that a demon has wings if it can fly, and that it has
        // a tail if it has a sting or tail-slap attack.
        const monsterentry *mon_data = get_monster_data(mc);
        bool tailed = false;
        for (int i = 0; i < 4; ++i)
            if (mon_data->attack[i].type == AT_STING
                || mon_data->attack[i].type == AT_TAIL_SLAP)
            {
                tailed = true;
                break;
            }

        if (flies && tailed)
            return MON_SHAPE_HUMANOID_WINGED_TAILED;
        else if (flies && !tailed)
            return MON_SHAPE_HUMANOID_WINGED;
        else if (!flies && tailed)
            return MON_SHAPE_HUMANOID_TAILED;
        else
            return MON_SHAPE_HUMANOID;
    }

    case '7': // trees and tree-like creatures
        return MON_SHAPE_MISC;
    case '9': // gargoyles
        return MON_SHAPE_HUMANOID_WINGED_TAILED;
    case '*': // orbs
        return MON_SHAPE_ORB;
    }

    return MON_SHAPE_MISC;
}

string get_mon_shape_str(const mon_body_shape shape)
{
    ASSERT_RANGE(shape, MON_SHAPE_HUMANOID, MON_SHAPE_MISC + 1);

    static const char *shape_names[] =
    {
        "humanoid", "winged humanoid", "tailed humanoid",
        "winged tailed humanoid", "centaur", "naga",
        "quadruped", "tailless quadruped", "winged quadruped",
        "bat", "snake", "fish",  "insect", "winged insect",
        "arachnid", "centipede", "snail", "plant", "fungus", "orb",
        "blob", "misc"
    };

    COMPILE_CHECK(ARRAYSZ(shape_names) == MON_SHAPE_MISC + 1);
    return shape_names[shape];
}

bool player_or_mon_in_sanct(const monster* mons)
{
    return is_sanctuary(you.pos()) || is_sanctuary(mons->pos());
}

bool mons_landlubbers_in_reach(const monster* mons)
{
    if (mons_has_ranged_attack(mons))
        return true;

    actor *act;
    for (radius_iterator ai(mons->pos(),
                            mons->reach_range(),
                            C_CIRCLE,
                            true);
                         ai; ++ai)
        if ((act = actor_at(*ai)) && !mons_aligned(mons, act))
            return true;

    return false;
}

int get_dist_to_nearest_monster()
{
    int minRange = LOS_RADIUS_SQ + 1;
    for (radius_iterator ri(you.pos(), LOS_NO_TRANS, true); ri; ++ri)
    {
        const monster* mon = monster_at(*ri);
        if (mon == NULL)
            continue;

        if (!mon->visible_to(&you))
            continue;

        // Plants/fungi don't count.
        if (mons_class_flag(mon->type, M_NO_EXP_GAIN)
            && (mon->type != MONS_BALLISTOMYCETE || mon->number == 0))
        {
            continue;
        }

        if (mon->wont_attack())
            continue;

        int dist = distance2(you.pos(), *ri);
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

int count_mara_fakes()
{
    int count = 0;
    for (monster_iterator mi; mi; ++mi)
    {
        if (mi->type == MONS_MARA_FAKE)
            count++;
    }

    return count;
}

actor *actor_by_mid(mid_t m)
{
    if (m == MID_PLAYER)
        return &you;
    return monster_by_mid(m);
}

monster *monster_by_mid(mid_t m)
{
    if (m == MID_ANON_FRIEND)
        return &menv[ANON_FRIENDLY_MONSTER];
    if (m == MID_YOU_FAULTLESS)
        return &menv[YOU_FAULTLESS];

    map<mid_t, unsigned short>::const_iterator mc = env.mid_cache.find(m);
    if (mc != env.mid_cache.end())
        return &menv[mc->second];
    return 0;
}

bool mons_is_tentacle_head(monster_type mc)
{
    return mc == MONS_KRAKEN || mc == MONS_TENTACLED_STARSPAWN;
}

bool mons_is_child_tentacle(monster_type mc)
{
    return mc == MONS_KRAKEN_TENTACLE
        || mc == MONS_STARSPAWN_TENTACLE
        || mc == MONS_SNAPLASHER_VINE;
}

bool mons_is_child_tentacle_segment(monster_type mc)
{
    return mc == MONS_KRAKEN_TENTACLE_SEGMENT
        || mc == MONS_STARSPAWN_TENTACLE_SEGMENT
        || mc == MONS_SNAPLASHER_VINE_SEGMENT;
}

bool mons_is_tentacle(monster_type mc)
{
    return mc == MONS_ELDRITCH_TENTACLE || mons_is_child_tentacle(mc);
}

bool mons_is_tentacle_segment(monster_type mc)
{
    return mc == MONS_ELDRITCH_TENTACLE_SEGMENT
        || mons_is_child_tentacle_segment(mc);
}

bool mons_is_tentacle_or_tentacle_segment(monster_type mc)
{
    return mons_is_tentacle(mc) || mons_is_tentacle_segment(mc);
}

monster* mons_get_parent_monster(monster* mons)
{
    for (monster_iterator mi; mi; ++mi)
    {
        if (mi->is_parent_monster_of(mons))
            return mi->as_monster();
    }

    return 0;
}

void init_anon()
{
    monster &mon = menv[ANON_FRIENDLY_MONSTER];
    mon.reset();
    mon.type = MONS_PROGRAM_BUG;
    mon.mid = MID_ANON_FRIEND;
    mon.attitude = ATT_FRIENDLY;
    mon.hit_points = mon.max_hit_points = 1000;

    monster &yf = menv[YOU_FAULTLESS];
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
        return &menv[ANON_FRIENDLY_MONSTER];
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

monster_type mons_tentacle_parent_type(const monster* mons)
{
    switch (mons_base_type(mons))
    {
        case MONS_KRAKEN_TENTACLE:
            return MONS_KRAKEN;
        case MONS_KRAKEN_TENTACLE_SEGMENT:
            return MONS_KRAKEN_TENTACLE;
        case MONS_STARSPAWN_TENTACLE:
            return MONS_TENTACLED_STARSPAWN;
        case MONS_STARSPAWN_TENTACLE_SEGMENT:
            return MONS_STARSPAWN_TENTACLE;
        case MONS_ELDRITCH_TENTACLE_SEGMENT:
            return MONS_ELDRITCH_TENTACLE;
        case MONS_SNAPLASHER_VINE_SEGMENT:
            return MONS_SNAPLASHER_VINE;
        default:
            return MONS_PROGRAM_BUG;
    }
}

monster_type mons_tentacle_child_type(const monster* mons)
{
    switch (mons_base_type(mons))
    {
        case MONS_KRAKEN:
            return MONS_KRAKEN_TENTACLE;
        case MONS_KRAKEN_TENTACLE:
            return MONS_KRAKEN_TENTACLE_SEGMENT;
        case MONS_TENTACLED_STARSPAWN:
            return MONS_STARSPAWN_TENTACLE;
        case MONS_STARSPAWN_TENTACLE:
            return MONS_STARSPAWN_TENTACLE_SEGMENT;
        case MONS_ELDRITCH_TENTACLE:
            return MONS_ELDRITCH_TENTACLE_SEGMENT;
        case MONS_SNAPLASHER_VINE:
            return MONS_SNAPLASHER_VINE_SEGMENT;
        default:
            return MONS_PROGRAM_BUG;
    }
}

//Returns whether a given monster is a tentacle segment immediately attached
//to the parent monster
bool mons_tentacle_adjacent(const monster* parent, const monster* child)
{
    return mons_is_tentacle_head(mons_base_type(parent))
           && mons_is_tentacle_segment(child->type)
           && child->props.exists("inwards")
           && child->props["inwards"].get_int() == parent->mindex();
}

mon_threat_level_type mons_threat_level(const monster *mon, bool real)
{
    const double factor = sqrt(exp_needed(you.experience_level) / 30.0);
    const int tension = exper_value(mon, real) / (1 + factor);

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

bool mons_foe_is_marked(const monster* mon)
{
    if (mon->foe == MHITYOU)
        return you.duration[DUR_SENTINEL_MARK];
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
        const monsterentry *md = get_monster_data(mc);

        int MR = md->resist_magic;
        if (MR < 0)
            MR = md->hpdice[9] * -MR * 4 / 3;
        if (md->resist_magic > 200 && md->resist_magic != MAG_IMMUNE)
            fails += make_stringf("%s has MR %d > 200\n", name, MR);

        // Tests below apply only to corpses.
        if (md->species != mc || md->bitfields & M_CANT_SPAWN)
            continue;

        if (md->weight && !md->corpse_thingy)
            fails += make_stringf("%s has a corpse but no corpse_type\n", name);
        if (md->weight && !md->zombie_size)
            fails += make_stringf("%s has a corpse but no zombie_size\n", name);
    }

    if (!fails.empty())
    {
        FILE *f = fopen("mon-data.out", "w");
        if (!f)
            sysfail("can't write test output");
        fprintf(f, "%s", fails.c_str());
        fclose(f);
        fail("mon-data errors (dumped to mon-data.out)");
    }
}

// Used when clearing level data, to ensure any additional reset quirks
// are handled properly.
void reset_all_monsters()
{
    for (int i = 0; i < MAX_MONSTERS; i++)
    {
        // The monsters here have already been saved or discarded, so this
        // is the only place when a constricting monster can legitimately
        // be reset.  Thus, clear constriction manually.
        if (!invalid_monster(&menv[i]))
        {
            delete menv[i].constricting;
            menv[i].constricting = nullptr;
            menv[i].clear_constricted();
        }
        menv[i].reset();
    }

    env.mid_cache.clear();
}

bool mons_is_recallable(actor* caller, monster* targ)
{
    // For player, only recall friendly monsters
    if (caller == &you)
    {
        if (!targ->friendly())
            return false;
    }
    // Monster recall requires same attitude and at least normal intelligence
    else if (mons_intel(targ) < I_NORMAL
             || (!caller && targ->friendly())
             || (caller && !mons_aligned(targ, caller->as_monster())))
    {
        return false;
    }

    return targ->alive()
           && !mons_class_is_stationary(targ->type)
           && !mons_is_conjured(targ->type)
           && monster_habitable_grid(targ, DNGN_FLOOR); //XXX?
}

vector<monster* > get_on_level_followers()
{
    vector<monster* > mon_list;
    for (monster_iterator mi; mi; ++mi)
        if (mons_is_recallable(&you, *mi) && mi->foe == MHITYOU)
            mon_list.push_back(*mi);

    return mon_list;
}

bool mons_stores_tracking_data(const monster* mons)
{
    return mons->type == MONS_THORN_HUNTER;
}

bool mons_is_beast(monster_type mc)
{
    if (mons_class_holiness(mc) != MH_NATURAL
          && mc != MONS_APIS
        || mons_class_intel(mc) < I_INSECT
        || mons_class_intel(mc) > I_ANIMAL)
    {
        return false;
    }
    else if (mons_genus(mc) == MONS_DRAGON
             || mons_genus(mc) == MONS_UGLY_THING
             || mc == MONS_ICE_BEAST
             || mc == MONS_SKY_BEAST
             || mc == MONS_SPIRIT_WOLF
             || mc == MONS_BUTTERFLY)
    {
        return false;
    }
    else
        return true;
}
