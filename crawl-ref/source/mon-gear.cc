/**
 * @file
 * @brief Monsters' starting equipment.
**/

#include "AppHdr.h"

#include "mon-gear.h"

#include <algorithm>

#include "artefact.h"
#include "art-enum.h"
#include "dungeon.h"
#include "ghost.h"
#include "item-name.h"
#include "item-prop.h"
#include "item-status-flag-type.h"
#include "items.h"
#include "libutil.h" // map_find
#include "misc.h" // december_holidays
#include "mon-place.h"
#include "mpr.h"
#include "religion.h" // upgrade_hepliaklqana_weapon
#include "state.h"
#include "tag-version.h"
#include "tilepick.h"
#include "unwind.h"

static void _strip_item_ego(item_def &item)
{
    item.brand = 0; // SP{WPN,ARM,MSL}_NORMAL

    // Remove glow, since that might be because of the ego...
    set_equip_desc(item, ISFLAG_NO_DESC);
    // ...but give it back if it came from curses or plusses.
    item_set_appearance(item);
}

void give_specific_item(monster* mon, int thing)
{
    if (thing == NON_ITEM || thing == -1)
        return;

    item_def &mthing = env.item[thing];
    ASSERT(mthing.defined());

    dprf(DIAG_MONPLACE, "Giving %s to %s...", mthing.name(DESC_PLAIN).c_str(),
         mon->name(DESC_PLAIN, true).c_str());

    mthing.pos.reset();
    mthing.link = NON_ITEM;

    if (mon->undead_or_demonic() || mon->god == GOD_YREDELEMNUL)
    {
        convert2bad(mthing);
        if (get_weapon_brand(mthing) == SPWPN_HOLY_WRATH)
            _strip_item_ego(mthing);
    }

    if (!is_artefact(mthing)
        && (mthing.base_type == OBJ_WEAPONS
         || mthing.base_type == OBJ_ARMOUR
         || mthing.base_type == OBJ_MISSILES))
    {
        bool enchanted = mthing.plus;
        // The item could either lose or gain brand after being generated,
        // adjust the glowing flag.
        if (!mthing.brand && !enchanted)
            set_equip_desc(mthing, 0);
        else if (mthing.brand && !get_equip_desc(mthing))
            set_equip_desc(mthing, ISFLAG_GLOWING);
    }

    unwind_var<int> save_speedinc(mon->speed_increment);
    if (!mon->pickup_item(mthing, false, true))
    {
        dprf(DIAG_MONPLACE, "Destroying %s because %s doesn't want it!",
             mthing.name(DESC_PLAIN, false, true).c_str(),
             mon->name(DESC_PLAIN, true).c_str());
        destroy_item(thing, true);
        return;
    }
    if (!mthing.defined()) // missiles merged into an existing stack
        return;
    ASSERT(mthing.holding_monster() == mon);

    if (!mthing.appearance_initialized())
        item_colour(mthing);
}

void give_specific_item(monster* mon, const item_def& tpl)
{
    int thing = get_mitm_slot();
    if (thing == NON_ITEM)
        return;

    env.item[thing] = tpl;
    give_specific_item(mon, thing);
}

static bool _should_give_unique_item(monster* mon)
{
    // Don't give Natasha an item for dying.
    return mon->type != MONS_NATASHA || !mon->props.exists(FELID_REVIVES_KEY);
}

static talisman_type _talisman_for(monster_type mtyp)
{
    switch (mtyp)
    {
    case MONS_RUPERT:
        if (coinflip())
            return TALISMAN_MAW; // good for shoutin'
        break;
    case MONS_AIZUL:
        return TALISMAN_SERPENT; // late, but so thematic!
    case MONS_ROXANNE:
        return TALISMAN_STATUE;
    case MONS_SOJOBO:
        if (coinflip())
            return TALISMAN_STORM;
        break;
    case MONS_BAI_SUZHEN:
        if (coinflip())
            return TALISMAN_DRAGON;
        break;
    case MONS_JOSEPHINA:
        if (coinflip())
            return TALISMAN_DEATH;
        break;
    case MONS_JORY:
        if (coinflip())
            return TALISMAN_VAMPIRE;
    default:
        break;
    }
    return NUM_TALISMANS;
}

static void _give_talisman(monster* mon, int level)
{
    const talisman_type talisman = _talisman_for(mon->type);
    if (mon->type == MONS_AIZUL)
        level = ISPEC_RANDART; // partially compensate for lateness
    if (talisman != NUM_TALISMANS)
        give_specific_item(mon, items(false, OBJ_TALISMANS, talisman, level));
}

static void _give_wand(monster* mon, int level)
{
    const bool always_wand = mons_class_flag(mon->type, M_ALWAYS_WAND);
    if (!always_wand)
    {
        if (!mons_is_unique(mon->type)
            || mons_class_flag(mon->type, M_NO_WAND)
            || !_should_give_unique_item(mon)
            || !one_chance_in(5))
        {
            return;
        }
    }

    // Don't give top-tier wands before 5 HD, except to Ijyb and not in sprint.
    const bool no_high_tier =
            (mon->get_experience_level() < 5
                || mons_class_flag(mon->type, M_NO_HT_WAND))
            && (mon->type != MONS_IJYB || crawl_state.game_is_sprint());

    const int idx = items(false, OBJ_WANDS, OBJ_RANDOM, level);

    if (idx == NON_ITEM)
        return;

    item_def& wand = env.item[idx];
    // Ugly hack: monsters can't use digging wands, so swap em out.
    while (wand.sub_type == WAND_DIGGING)
    {
        dprf("rerolling");
        generate_wand_item(wand, OBJ_RANDOM, level);
        item_colour(wand);
    }

    const char* rejection_reason =
        (no_high_tier && is_high_tier_wand(wand.sub_type)) ? "high tier" :
                                    !mon->likes_wand(wand) ?      "weak" :
                                                                  nullptr;

    if (rejection_reason && !always_wand)
    {
        dprf(DIAG_MONPLACE,
             "Destroying %s because %s doesn't want a %s wand.",
             wand.name(DESC_A).c_str(),
             mon->name(DESC_THE).c_str(),
             rejection_reason);
        destroy_item(idx, true);
        return;
    }

    wand.flags = 0;
    give_specific_item(mon, idx);
}

static item_def* make_item_for_monster(
    monster* mons,
    object_class_type base,
    int subtype,
    int level,
    int allow_uniques = 0,
    iflags_t flags = 0);

typedef vector<pair<weapon_type, int>> weapon_list;
typedef vector<pair<armour_type, int>> armour_list;
struct plus_range
{
    int odds; ///<1/x chance of getting +[min,max] to weapon plus
    int min, max;
    int nrolls; ///< min 1
};

struct mon_weapon_spec
{
    /// weighted list of weapon types; NUM_WEAPONS -> no weapon
    weapon_list types;
    /// range of possible weapon enchant plusses; if nonzero, sets force_item
    plus_range plusses;
    /// weighted brand list; NUM_BRANDS -> no forced brand
    vector<pair<brand_type, int>> brands;
    /// extra 1/x chance to generate as ISPEC_GOOD_ITEM
    int good_chance;
};

/**
 * Try to apply the given weapon spec to the given base item & generation
 * parameters; randomly choose a weapon type and, possibly, change other fields.
 *
 * @param spec              How to choose weapon type, plusses, brands, etc.
 * @param item[out]         An item to be populated with subtype and other
 *                          fields, before or instead of a call to items().
 * @param force_item[out]   Have we set something (plus, brand) that requires
 *                          us to skip items()?
 * @param level[out]        The item's quality level, if we want to override it.
 * @return                  Did we choose a weapon type?
 */
static bool _apply_weapon_spec(const mon_weapon_spec &spec, item_def &item,
                               bool &force_item, int &level)
{

    const weapon_type *wpn_type
        = random_choose_weighted(spec.types);
    ASSERT(wpn_type);
    if (*wpn_type == NUM_WEAPONS)
        return false;

    item.base_type = OBJ_WEAPONS;
    item.sub_type = *wpn_type;

    if (spec.plusses.odds)
    {
        if (one_chance_in(spec.plusses.odds))
        {
            const int rolls = max(1, spec.plusses.nrolls);
            item.plus += random_range(spec.plusses.min, spec.plusses.max,
                                      rolls);
            force_item = true;
        }
    }
    else // normal plusses (ignored if brand isn't set)
        item.plus = determine_nice_weapon_plusses(level);

    if (spec.good_chance && one_chance_in(spec.good_chance))
        level = ISPEC_GOOD_ITEM;

    const brand_type *brand = random_choose_weighted(spec.brands);
    if (brand)
    {
        if (*brand != NUM_SPECIAL_WEAPONS)
        {
            set_item_ego_type(item, OBJ_WEAPONS, *brand);
            force_item = true;
        }
    }
    else if (force_item) // normal brand
    {
        set_item_ego_type(item, OBJ_WEAPONS,
                          determine_weapon_brand(item, level));
    }

    return true;
}

/**
 * Make a weapon for the given monster.
 *
 * @param mtyp          The type of monster; e.g. MONS_ORC_WARRIOR.
 * @param level         The quality of the weapon to make, usually absdepth.
 * @param melee_only    If this monster can be given both melee and ranged
 *                      weapons, whether to restrict this to melee weapons.
 * @return              The index of the newly-created weapon, or NON_ITEM if
 *                      none was made.
 */
int make_mons_weapon(monster_type type, int level, bool melee_only)
{
    rng::subgenerator item_rng;

    static const weapon_list GOBLIN_WEAPONS =
    {   { WPN_DAGGER,           1 },
        { WPN_CLUB,             1 }};
    static const weapon_list GNOLL_WEAPONS = // total 30
    {   { WPN_SPEAR,            8 },
        { WPN_HALBERD,          4 },
        { WPN_CLUB,             6 },
        { WPN_WHIP,             6 },
        { WPN_FLAIL,            6 } };
    static const weapon_list ORC_WEAPONS = // total 525 (!)
    {   { WPN_CLUB,            100 },
        { WPN_DAGGER,           80 },
        { WPN_FLAIL,            60 },
        { WPN_SHORT_SWORD,      60 },
        { WPN_HAND_AXE,         80 },
        { WPN_WHIP,             45 },
        { WPN_MACE,             40 },
        { WPN_TRIDENT,          20 },
        { WPN_FALCHION,         20 },
        { WPN_WAR_AXE,          14 },
        { WPN_MORNINGSTAR,      6 } };
    static const weapon_list DE_KNIGHT_WEAPONS = // total 83 (?)
    {   { WPN_LONG_SWORD,       22 },
        { WPN_SHORT_SWORD,      22 },
        { WPN_SCIMITAR,         17 },
        { WPN_SHORTBOW,         17 },
        { WPN_LONGBOW,          5 }, };
    static const weapon_list DE_MAGE_WEAPONS =
    {   { WPN_LONG_SWORD,       2 },
        { WPN_SHORT_SWORD,      1 },
        { WPN_RAPIER,           1 },
        { WPN_DAGGER,           1 }, };
    static const weapon_list DRAC_MAGE_WEAPONS = // XXX: merge with DE? ^
    {   { WPN_LONG_SWORD,       2 },
        { WPN_SHORT_SWORD,      1 },
        { WPN_RAPIER,           1 },
        { WPN_DAGGER,           1 },
        { WPN_WHIP,             1 }, };
    static const weapon_list NAGA_WEAPONS = // total 124
    {   { WPN_LONG_SWORD,       10 },
        { WPN_SHORT_SWORD,      10 },
        { WPN_SCIMITAR,         10 },
        { WPN_BATTLEAXE,        10 },
        { WPN_HAND_AXE,         10 },
        { WPN_HALBERD,          10 },
        { WPN_GLAIVE,           10 },
        { WPN_MACE,             10 },
        { WPN_DIRE_FLAIL,       10 },
        { WPN_TRIDENT,          10 },
        { WPN_WAR_AXE,          9 },
        { WPN_FLAIL,            9 },
        { WPN_PARTISAN,         2 },
        { WPN_BROAD_AXE,        1 },
        { WPN_MORNINGSTAR,      1 }, };
    static const weapon_list ORC_KNIGHT_WEAPONS =
    {   { WPN_GREAT_SWORD,      8 },
        { WPN_LONG_SWORD,       8 },
        { WPN_BATTLEAXE,        8 },
        { WPN_WAR_AXE,          8 },
        { WPN_GREAT_MACE,       6 },
        { WPN_DIRE_FLAIL,       4 },
        { WPN_BARDICHE,         2 },
        { WPN_GLAIVE,           2 },
        { WPN_BROAD_AXE,        2 },
        { WPN_HALBERD,          2 },
        { WPN_PARTISAN,         1 }, };
    static const mon_weapon_spec ORC_KNIGHT_WSPEC =
        { ORC_KNIGHT_WEAPONS, {4, 1, 3} };
    static const mon_weapon_spec ORC_WARLORD_WSPEC =
        { ORC_KNIGHT_WEAPONS, {4, 1, 3}, {}, 3 };
    static const weapon_list IRON_WEAPONS =
    {   { WPN_GREAT_MACE,       3 },
        { WPN_DIRE_FLAIL,       2 },
        { WPN_FLAIL,            2 },
        { WPN_MORNINGSTAR,      2 },
        { WPN_MACE,             1 }, };
    static const weapon_list OGRE_WEAPONS =
    {   { WPN_GIANT_CLUB,        2 },
        { WPN_GIANT_SPIKED_CLUB, 1 }, };
    static const weapon_list DOUBLE_OGRE_WEAPONS = // total 100
    {   { WPN_GIANT_CLUB,        60 },
        { WPN_GIANT_SPIKED_CLUB, 30 },
        { WPN_DIRE_FLAIL,        9 },
        { WPN_GREAT_MACE,        1 }, };
    static const weapon_list FAUN_WEAPONS =
    {   { WPN_SPEAR,            2 },
        { WPN_CLUB,             1 },
        { WPN_QUARTERSTAFF,     1 }, };
    static const mon_weapon_spec EFREET_WSPEC =
    { { { WPN_SCIMITAR,         1 } },
      { 1, 0, 4 },
      { { SPWPN_FLAMING, 1 } } };
    static const mon_weapon_spec DAEVA_WSPEC =
    { { { WPN_EUDEMON_BLADE,    1 },
        { WPN_SCIMITAR,         2 },
        { WPN_LONG_SWORD,       1 } },
      { 1, 2, 5 },
      { { SPWPN_HOLY_WRATH,     1 } } };
    static const vector<pair<brand_type, int>> HELL_KNIGHT_BRANDS = // sum 45
    {   { SPWPN_FLAMING,        13 },
        { SPWPN_DRAINING,       4 },
        { SPWPN_HEAVY,          4 },
        { SPWPN_DISTORTION,     2 },
        { SPWPN_PAIN,           2 },
        { NUM_SPECIAL_WEAPONS,  20 }, // 5/9 chance of brand
    };
    static const weapon_list URUG_WEAPONS =
    {   { WPN_HALBERD,          5 },
        { WPN_GLAIVE,           5 },
        { WPN_WAR_AXE,          6 },
        { WPN_GREAT_MACE,       6 },
        { WPN_BATTLEAXE,        7 },
        { WPN_LONG_SWORD,       8 },
        { WPN_SCIMITAR,         8 },
        { WPN_GREAT_SWORD,      8 },
        { WPN_BROAD_AXE,        9 },
        { WPN_DOUBLE_SWORD,     10 },
        { WPN_EVENINGSTAR,      13 },
        { WPN_PARTISAN,         15 }, };
    static const weapon_list SP_DEFENDER_WEAPONS =
    {   { WPN_LAJATANG,         1 },
        { WPN_QUICK_BLADE,      1 },
        { WPN_RAPIER,           1 },
        { WPN_DEMON_WHIP,       1 },
        { WPN_FLAIL,            1 } };
    static const weapon_list DS_WEAPONS =
    {   { WPN_GREAT_SWORD,      3 },
        { WPN_DEMON_BLADE,      2 },
        { WPN_DIRE_FLAIL,       3 },
        { WPN_DEMON_WHIP,       2 },
        { WPN_GLAIVE,           3 },
        { WPN_DEMON_TRIDENT,    2 } };
    static const weapon_list GARGOYLE_WEAPONS =
    {   { WPN_MACE,             15 },
        { WPN_FLAIL,            10 },
        { WPN_MORNINGSTAR,      5 },
        { WPN_DIRE_FLAIL,       2 }, };

    static const map<monster_type, mon_weapon_spec> primary_weapon_specs = {
        { MONS_ROBIN,
            { { { WPN_CLUB,             35 },
                { WPN_DAGGER,           30 },
                { WPN_SPEAR,            30 },
                { WPN_SHORT_SWORD,      20 },
                { WPN_MACE,             20 },
                { WPN_WHIP,             15 },
                { WPN_TRIDENT,          10 },
                { WPN_FALCHION,         10 },
        } } },
        { MONS_GOBLIN,                  { GOBLIN_WEAPONS } },
        { MONS_JESSICA,                 { GOBLIN_WEAPONS } },
        { MONS_IJYB,                    { GOBLIN_WEAPONS } },
        { MONS_WIGHT,
            { { { WPN_MORNINGSTAR,      4 },
                { WPN_DIRE_FLAIL,       4 },
                { WPN_WAR_AXE,          4 },
                { WPN_TRIDENT,          4 },
                { WPN_MACE,             7 },
                { WPN_FLAIL,            7 },
                { WPN_FALCHION,         7 },
                { WPN_DAGGER,           7 },
                { WPN_SHORT_SWORD,      7 },
                { WPN_LONG_SWORD,       7 },
                { WPN_SCIMITAR,         7 },
                { WPN_GREAT_SWORD,      7 },
                { WPN_HAND_AXE,         7 },
                { WPN_BATTLEAXE,        7 },
                { WPN_SPEAR,            7 },
                { WPN_HALBERD,          7 },
            } } },
        { MONS_EDMUND,
            { { { WPN_DIRE_FLAIL,       1 },
                { WPN_FLAIL,            2 },
        }, {}, {}, 1 } },
        { MONS_DEATH_KNIGHT,
            { { { WPN_MORNINGSTAR,      5 },
                { WPN_GREAT_MACE,       5 },
                { WPN_HALBERD,          5 },
                { WPN_GREAT_SWORD,      5 },
                { WPN_GLAIVE,           8 },
                { WPN_BROAD_AXE,        10 },
                { WPN_BATTLEAXE,        15 },
        }, {2, 1, 4} } },
        { MONS_GNOLL,                   { GNOLL_WEAPONS } },
        { MONS_OGRE_MAGE,               { GNOLL_WEAPONS } },
        { MONS_NAGA_MAGE,               { GNOLL_WEAPONS } },
        { MONS_NAGARAJA,            { GNOLL_WEAPONS } },
        { MONS_GNOLL_BOUDA,
            { { { WPN_CLUB,             1 },
                { WPN_WHIP,             1 },
        } } },
        { MONS_GNOLL_SERGEANT,
            { { { WPN_SPEAR,            2 },
                { WPN_TRIDENT,          1 },
        }, {}, {}, 3 } },
        { MONS_PIKEL, { { { WPN_WHIP, 1 } }, { 1, 0, 2 }, {
            { SPWPN_FLAMING, 2 },
            { SPWPN_FREEZING, 2 },
            { SPWPN_ELECTROCUTION, 1 },
        } } },
        { MONS_GRUM,
            { { { WPN_SPEAR,            3 },
                { WPN_HALBERD,          1 },
                { WPN_GLAIVE,           1 },
        }, { 1, -2, 1 } } },
        { MONS_GRUNN,
            { { { WPN_GLAIVE,            2 },
                { WPN_BARDICHE,          1 },
        }, { 1, 4, 9 }, { { SPWPN_DRAINING, 1 } } } },
        { MONS_JEREMIAH,
            { { { WPN_TRIDENT,            1 },
        }, { 1, 2, 4 }, {
            { SPWPN_DRAINING,  9 },
            { SPWPN_VAMPIRISM, 1 },
        } } },
        { MONS_CRAZY_YIUF,
            { { { WPN_QUARTERSTAFF, 1 } },
            { 1, 2, 4 },
            { { SPWPN_CHAOS, 1 } } } },
        { MONS_JOSEPH, { { { WPN_QUARTERSTAFF, 1 } } } },
        { MONS_SPRIGGAN_DRUID, { { { WPN_QUARTERSTAFF, 1 } }, {}, {
                { SPWPN_SPECTRAL, 1 },
                { NUM_SPECIAL_WEAPONS, 5 }
        } } },
        { MONS_BAI_SUZHEN, { { { WPN_QUARTERSTAFF, 1 } } } },
        { MONS_ORC,                     { ORC_WEAPONS } },
        { MONS_ORC_PRIEST,              { ORC_WEAPONS } },
        { MONS_DRACONIAN,               { ORC_WEAPONS } },
        { MONS_TERENCE,
            { { { WPN_FLAIL,            30 },
                { WPN_HAND_AXE,         20 },
                { WPN_SHORT_SWORD,      20 },
                { WPN_MACE,             20 },
                { WPN_TRIDENT,          10 },
                { WPN_FALCHION,         10 },
                { WPN_MORNINGSTAR,      3 },
        } } },
        { MONS_DUVESSA,
            { { { WPN_SHORT_SWORD,      3 },
                { WPN_RAPIER,           1 },
        } } },
        { MONS_ASTERION, {
            { { WPN_DEMON_WHIP,         1 },
              { WPN_DEMON_BLADE,        1 },
              { WPN_DEMON_TRIDENT,      1 },
              { WPN_MORNINGSTAR,        1 },
              { WPN_BROAD_AXE,          1 }, },
            {}, { { SPWPN_SPECTRAL, 1 } }, 1,
        } },
        { MONS_AMAEMON, {
            { { WPN_DEMON_WHIP,       1 } },
              { 1, -2, 1 }, { { SPWPN_VENOM, 1 } } } },
        { MONS_MARA,
            { { { WPN_DEMON_WHIP,       1 },
                { WPN_DEMON_TRIDENT,    1 },
                { WPN_DEMON_BLADE,      1 },
        }, {}, {}, 1 } },
        { MONS_RAKSHASA,
            { { { WPN_WHIP,             1 },
                { WPN_TRIDENT,          1 },
                { WPN_LONG_SWORD,       1 },
        } } },
        { MONS_NEKOMATA, {
            { { WPN_DEMON_BLADE, 1 } },
              { 4, 2, 4 }, { { SPWPN_FLAMING, 3 }, { SPWPN_DRAINING, 2 }, }, } },
        { MONS_DEEP_ELF_KNIGHT,         { DE_KNIGHT_WEAPONS } },
        { MONS_DEEP_ELF_HIGH_PRIEST,    { DE_KNIGHT_WEAPONS } },
        { MONS_DEEP_ELF_BLADEMASTER,
            { { { WPN_RAPIER,           20 },
                { WPN_SHORT_SWORD,      5 },
                { WPN_QUICK_BLADE,      1 },
        } } },
        { MONS_DEEP_ELF_ARCHER,
            { { { WPN_SHORT_SWORD,      1 },
                { WPN_DAGGER,           1 },
        } } },
        { MONS_DEEP_ELF_MASTER_ARCHER, { { { WPN_LONGBOW, 1 } } } },
        { MONS_DEEP_ELF_AIR_MAGE,       { DE_MAGE_WEAPONS } },
        { MONS_DEEP_ELF_FIRE_MAGE,      { DE_MAGE_WEAPONS } },
        { MONS_DEEP_ELF_ANNIHILATOR,    { DE_MAGE_WEAPONS } },
        { MONS_DEEP_ELF_DEATH_MAGE,     { DE_MAGE_WEAPONS } },
        { MONS_DEEP_ELF_DEMONOLOGIST,   { DE_MAGE_WEAPONS } },
        { MONS_DEEP_ELF_SORCERER,       { DE_MAGE_WEAPONS } },
        { MONS_DEEP_ELF_ELEMENTALIST,   { DE_MAGE_WEAPONS } },
        { MONS_DRACONIAN_SHIFTER,       { DRAC_MAGE_WEAPONS } },
        { MONS_DRACONIAN_SCORCHER,      { DRAC_MAGE_WEAPONS } },
        { MONS_DRACONIAN_ANNIHILATOR,   { DRAC_MAGE_WEAPONS } },
        { MONS_DRACONIAN_STORMCALLER,   { DRAC_MAGE_WEAPONS } },
        { MONS_RAGGED_HIEROPHANT,       { DRAC_MAGE_WEAPONS } },
        { MONS_VASHNIA,                 { NAGA_WEAPONS, {}, {}, 1 } },
        { MONS_NAGA_SHARPSHOOTER,       { NAGA_WEAPONS } },
        { MONS_NAGA,                    { NAGA_WEAPONS } },
        { MONS_NAGA_WARRIOR,            { NAGA_WEAPONS } },
        { MONS_ORC_WARRIOR,             { NAGA_WEAPONS } },
        { MONS_ORC_HIGH_PRIEST,         { NAGA_WEAPONS } },
        { MONS_BLORKULA_THE_ORCULA,     { NAGA_WEAPONS } },
        { MONS_DANCING_WEAPON,          { NAGA_WEAPONS, {}, {}, 1 } },
        { MONS_SPECTRAL_WEAPON,         { NAGA_WEAPONS } }, // for mspec placement
        { MONS_FRANCES,                 { NAGA_WEAPONS } },
        { MONS_HAROLD,                  { NAGA_WEAPONS } },
        { MONS_SKELETAL_WARRIOR,        { NAGA_WEAPONS } },
        { MONS_PALE_DRACONIAN,          { NAGA_WEAPONS } },
        { MONS_RED_DRACONIAN,           { NAGA_WEAPONS } },
        { MONS_WHITE_DRACONIAN,         { NAGA_WEAPONS } },
        { MONS_GREEN_DRACONIAN,         { NAGA_WEAPONS } },
        { MONS_BLACK_DRACONIAN,         { NAGA_WEAPONS } },
        { MONS_YELLOW_DRACONIAN,        { NAGA_WEAPONS } },
        { MONS_PURPLE_DRACONIAN,        { NAGA_WEAPONS } },
        { MONS_GREY_DRACONIAN,          { NAGA_WEAPONS } },
        { MONS_TENGU,                   { NAGA_WEAPONS } },
        { MONS_NAGA_RITUALIST,          { NAGA_WEAPONS } },
        { MONS_TIAMAT,
            { { { WPN_BARDICHE,         1 },
                { WPN_DEMON_TRIDENT,    1 },
                { WPN_GLAIVE,           1 },
        }, {}, {}, 1 } },
        { MONS_RUPERT,
            // Rupert favours big two-handers with visceral up-close
            // effects, i.e. no polearms.
            { { { WPN_GREAT_MACE,       10 },
                { WPN_GREAT_SWORD,      6 },
                { WPN_TRIPLE_SWORD,     2 },
                { WPN_BATTLEAXE,        8 },
                { WPN_EXECUTIONERS_AXE, 2 },
        }, {}, {}, 1 } },
        { MONS_TENGU_REAVER,            ORC_WARLORD_WSPEC },
        { MONS_VAULT_WARDEN,            ORC_WARLORD_WSPEC },
        { MONS_ORC_WARLORD,             ORC_WARLORD_WSPEC },
        { MONS_SAINT_ROKA,              ORC_WARLORD_WSPEC },
        { MONS_DRACONIAN_KNIGHT,        ORC_WARLORD_WSPEC },
        { MONS_ORC_KNIGHT,              ORC_KNIGHT_WSPEC },
        { MONS_TENGU_WARRIOR,           ORC_KNIGHT_WSPEC },
        { MONS_VAULT_GUARD,             ORC_KNIGHT_WSPEC },
        { MONS_VAMPIRE_KNIGHT,          ORC_KNIGHT_WSPEC },
        { MONS_LOUISE,
            { { { WPN_MORNINGSTAR, 1 },
                { WPN_EVENINGSTAR, 1 },
                { WPN_TRIDENT, 1 },
                { WPN_PARTISAN, 1 },
                { WPN_DEMON_TRIDENT, 1 },
        }, {}, {}, 1 } },
        { MONS_JORY,
            { { { WPN_GREAT_SWORD,      3 },
                { WPN_GLAIVE,           1 },
                { WPN_BATTLEAXE,        1 },
        }, {}, {}, 1 } },
        { MONS_VAULT_SENTINEL,
            { { { WPN_LONG_SWORD,       5 },
                { WPN_FALCHION,         4 },
                { WPN_WAR_AXE,          3 },
                { WPN_MORNINGSTAR,      3 },
        } } },
        { MONS_IRONBOUND_CONVOKER,      { IRON_WEAPONS } },
        { MONS_IRONBOUND_PRESERVER,     { IRON_WEAPONS } },
        { MONS_SIGMUND, { { { WPN_HALBERD, 1 } } } },
        { MONS_REAPER, { { { WPN_HALBERD, 1 } }, {}, {}, 1 } },
        { MONS_BALRUG, { { { WPN_DEMON_WHIP, 1 } } } },
        { MONS_RED_DEVIL,
            { { { WPN_TRIDENT,          4 },
                { WPN_DEMON_TRIDENT,    1 },
        } } },
        { MONS_TWO_HEADED_OGRE,         { DOUBLE_OGRE_WEAPONS } },
        { MONS_IRON_GIANT,              { DOUBLE_OGRE_WEAPONS } },
        { MONS_LODUL,
            { { { WPN_DIRE_FLAIL,       9 },
                { WPN_GREAT_MACE,       1 },
        } } },
        { MONS_IRONBOUND_BEASTMASTER,
            { { { WPN_WHIP,             6 },
                { WPN_TRIDENT,          4 },
                { WPN_DEMON_WHIP,       2 },
                { WPN_PARTISAN,         2 },
        } } },
        { MONS_IRONBOUND_THUNDERHULK,
            { { { WPN_DIRE_FLAIL,       9 },
                { WPN_GREAT_MACE,       1 },
        } } },
        { MONS_ETTIN,
            { { { WPN_DIRE_FLAIL,       9 },
                { WPN_GREAT_MACE,       1 },
        } } },
        { MONS_OGRE,                    { OGRE_WEAPONS } },
        { MONS_EROLCHA,                 { OGRE_WEAPONS } },
        { MONS_ONI_INCARCERATOR, {
            { { WPN_GLAIVE,             3 },
              { WPN_BARDICHE,           1 }, },
            { 1, 1, 3 },
            { { SPWPN_FLAMING, 1 } }
        } },
        { MONS_ILSUIW, {
            { { WPN_TRIDENT,            1 } },
            { 1, -1, 6, 2 },
            { { SPWPN_FREEZING, 1 } }
        } },
        { MONS_MERFOLK_IMPALER,
            { { { WPN_TRIDENT,          20 },
                { WPN_DEMON_TRIDENT,    3 },
        } } },
        { MONS_MERFOLK_AQUAMANCER, { { { WPN_RAPIER, 1 } }, {}, {}, 2 } },
        { MONS_MERFOLK_JAVELINEER, { { { WPN_SPEAR, 1 } } } },
        { MONS_SPRIGGAN_RIDER, { { { WPN_SPEAR, 1 } } } },
        { MONS_MERFOLK, { { { WPN_TRIDENT, 1 } } } },
        { MONS_MERFOLK_SIREN,
            { { { WPN_TRIDENT,          1 },
                { WPN_SPEAR,            2 },
        } } },
        { MONS_KOBOLD_BLASTMINER,
            { { { WPN_ORCBOW,           8 },
                { WPN_ARBALEST,         31 },
                { WPN_HAND_CANNON,      1 },
        } } },
        { MONS_CENTAUR,
            { { { WPN_SHORTBOW,         1 },
                { WPN_ORCBOW,           1 },
        } } },
        { MONS_CENTAUR_WARRIOR,
            { { { WPN_SHORTBOW,         1 },
                { WPN_ORCBOW,           1 },
        } } },
        { MONS_FAUN,                    { FAUN_WEAPONS } },
        { MONS_SATYR,                   { FAUN_WEAPONS } },
        { MONS_SERVANT_OF_WHISPERS,     { FAUN_WEAPONS } },
        { MONS_NESSOS, {
            { { WPN_LONGBOW,            1 } },
            { 1, 1, 3 },
            { { SPWPN_FLAMING, 1 } }
        } },
        { MONS_YAKTAUR,         { { { WPN_ARBALEST, 1 } } } },
        { MONS_YAKTAUR_CAPTAIN, { {
            { WPN_ARBALEST,      19 },
            { WPN_HAND_CANNON, 1  },
        } } },
        { MONS_EFREET,                  EFREET_WSPEC },
        { MONS_ERICA,                   EFREET_WSPEC },
        { MONS_AZRAEL,                  EFREET_WSPEC },
        { MONS_ANGEL, {
            { { WPN_WHIP,               3 },
              { WPN_SACRED_SCOURGE,     1 }, },
            { 1, 1, 3 },
            { { SPWPN_HOLY_WRATH, 1 } },
        } },
        { MONS_CHERUB,
            { { { WPN_FLAIL,            1 },
                { WPN_LONG_SWORD,       1 },
                { WPN_SCIMITAR,         1 },
                { WPN_FALCHION,         1 }, },
            { 1, 0, 4 },
            { { SPWPN_FLAMING, 1 } },
        } },
        { MONS_FRAVASHI,
          // it'd be kinda weird to use trishulas considering they're from
          // a literal opposing faith in the region.
            { { { WPN_HALBERD,       3 },
                { WPN_GLAIVE,        6 },
                { WPN_PARTISAN,      5 },
                { WPN_BARDICHE,      1 }, },
            { 1, 1, 3 },
            { { SPWPN_HOLY_WRATH, 1 } },
        } },
        { MONS_SERAPH, {
            { { WPN_GREAT_SWORD,        1 } },
            { 1, 3, 8 }, // highly enchanted, we're top rank
            { { SPWPN_FLAMING, 1 } },
        } },
        { MONS_DAEVA,                   DAEVA_WSPEC },
        { MONS_PROFANE_SERVITOR,
            { { { WPN_DEMON_WHIP,       1 },
                { WPN_WHIP,             3 }, },
            { 1, 1, 3 },
        } },
        { MONS_MENNAS, {
            { { WPN_TRISHULA,       1},
              { WPN_SACRED_SCOURGE, 1},
              { WPN_EUDEMON_BLADE,  1}, },
            { 1, 0, 5},
            { { SPWPN_HOLY_WRATH, 1}}
        } },
        { MONS_WIGLAF,
            { { { WPN_BROAD_AXE,        14 },
                { WPN_BATTLEAXE,        8 },
                { WPN_EVENINGSTAR,      5 },
                { WPN_MORNINGSTAR,      8 }
        } } },
        { MONS_DONALD,
            { { { WPN_SCIMITAR,         12 },
                { WPN_LONG_SWORD,       10 },
                { WPN_BROAD_AXE,        9 },
                { WPN_PARTISAN,         9 },
                { WPN_EVENINGSTAR,      7 },
                { WPN_DOUBLE_SWORD,     7 },
                { WPN_WAR_AXE,          3 },
        } } },
        { MONS_HELL_KNIGHT,
            { { { WPN_DEMON_WHIP,       1 },
                { WPN_DEMON_BLADE,      1 },
                { WPN_DEMON_TRIDENT,    1 },
                { WPN_HALBERD,          1 },
                { WPN_GLAIVE,           1 },
                { WPN_WAR_AXE,          1 },
                { WPN_GREAT_MACE,       1 },
                { WPN_BATTLEAXE,        1 },
                { WPN_LONG_SWORD,       1 },
                { WPN_SCIMITAR,         1 },
                { WPN_GREAT_SWORD,      1 },
                { WPN_PARTISAN,         1 },
                { WPN_BROAD_AXE,        1 }, },
              { 1, 0, 5 },
              HELL_KNIGHT_BRANDS
        } },
        { MONS_MAGGIE,
            { { { WPN_HALBERD,          1 },
                { WPN_GLAIVE,           1 },
                { WPN_WAR_AXE,          1 },
                { WPN_MORNINGSTAR,      1 },
                { WPN_LONG_SWORD,       1 },
                { WPN_SCIMITAR,         1 },
                { WPN_PARTISAN,         1 },
                { WPN_BROAD_AXE,        1 }, },
              { },
              HELL_KNIGHT_BRANDS
        } },
        { MONS_MARGERY,
            { { { WPN_DEMON_WHIP,       2 },
                { WPN_DEMON_BLADE,      2 },
                { WPN_DEMON_TRIDENT,    2 },
                { WPN_HALBERD,          1 },
                { WPN_GLAIVE,           1 },
                { WPN_WAR_AXE,          1 },
                { WPN_GREAT_MACE,       1 },
                { WPN_BATTLEAXE,        1 },
                { WPN_LONG_SWORD,       1 },
                { WPN_SCIMITAR,         1 },
                { WPN_GREAT_SWORD,      1 },
                { WPN_PARTISAN,         1 },
                { WPN_BROAD_AXE,        1 }, },
              { 1, 0, 5 },
              HELL_KNIGHT_BRANDS
        } },
        { MONS_URUG,                    { URUG_WEAPONS } },
        { MONS_NORRIS,                  { URUG_WEAPONS } },
        { MONS_FREDERICK,
            { { { WPN_DEMON_BLADE,          2 },
                { WPN_DEMON_WHIP,           1 },
                { WPN_DEMON_TRIDENT,        2 },
                { WPN_DOUBLE_SWORD,         1 },
                { WPN_BROAD_AXE,            2 },
                { WPN_PARTISAN,             1 },
                { WPN_EVENINGSTAR,          2 },
        } } },
        { MONS_FIRE_GIANT, {
            { { WPN_GREAT_SWORD,        1 } }, {},
            { { SPWPN_FLAMING, 1 } },
        } },
        { MONS_FROST_GIANT, {
            { { WPN_BATTLEAXE,          1 } }, {},
            { { SPWPN_FREEZING, 1 } },
        } },
        { MONS_CACTUS_GIANT,    { { { WPN_GIANT_SPIKED_CLUB, 1 } } } },
        { MONS_ORC_WIZARD,      { { { WPN_DAGGER, 1 } } } },
        { MONS_ORC_SORCERER,    { { { WPN_DAGGER, 1 } } } },
        { MONS_NERGALLE,        { { { WPN_DAGGER, 1 } } } },
        { MONS_DOWAN,           { { { WPN_DAGGER, 1 } } } },
        { MONS_BURIAL_ACOLYTE,  { { { WPN_DAGGER, 1 } } } },
        { MONS_KOBOLD_DEMONOLOGIST, { { { WPN_DAGGER, 1 } } } },
        { MONS_NECROMANCER,      { { { WPN_DAGGER, 1 } } } },
        { MONS_ARCANIST,         { { { WPN_DAGGER, 1 } } } },
        { MONS_OCCULTIST,        { { { WPN_DAGGER, 1 } } } },
        { MONS_JOSEPHINE,        { { { WPN_DAGGER, 1 } } } },
        { MONS_PSYCHE, {
            { { WPN_DAGGER,             1 }, },
            { 1, 0, 4 },
            { { SPWPN_CHAOS, 3 },
              { SPWPN_DISTORTION, 1 } },
        } },
        { MONS_AGNES,       { { { WPN_LAJATANG, 1 } } } },
        { MONS_SONJA, {
            { { WPN_DAGGER,             1 },
              { WPN_SHORT_SWORD,        1 }, }, {},
            { { SPWPN_DISTORTION,       3 },
              { SPWPN_VENOM,            2 },
              { SPWPN_DRAINING,         1 } },
        } },
        { MONS_MAURICE,
            { { { WPN_DAGGER,           1 },
                { WPN_SHORT_SWORD,      1 },
        } } },
        { MONS_EUSTACHIO,
            { { { WPN_FALCHION,         1 },
                { WPN_RAPIER,           2 },
        } } },
        { MONS_NIKOLA, {
            { { WPN_RAPIER,             1 } },
            { 1, 0, 4 },
            { { SPWPN_ELECTROCUTION, 1 } },
        } },
        { MONS_SALAMANDER_MYSTIC,
            { { { WPN_QUARTERSTAFF,     10 },
                { WPN_DAGGER,           5 },
                { WPN_SCIMITAR,         2 },
        } } },
        { MONS_SPRIGGAN,
            { { { WPN_DAGGER,           1 },
                { WPN_SHORT_SWORD,      1 },
                { WPN_RAPIER,           1 },
        } } },
        { MONS_SPRIGGAN_BERSERKER, {
            { { WPN_QUARTERSTAFF,       10 },
              { WPN_HAND_AXE,           9 },
              { WPN_WAR_AXE,            12 },
              { WPN_BROAD_AXE,          5 },
              { WPN_FLAIL,              5 },
              { WPN_RAPIER,             10 }, }, {},
            { { SPWPN_ANTIMAGIC, 1 },
              { NUM_SPECIAL_WEAPONS, 3 } },
        } },
        { MONS_SPRIGGAN_DEFENDER, { SP_DEFENDER_WEAPONS, {}, {}, 1 } },
        { MONS_ENCHANTRESS, { SP_DEFENDER_WEAPONS, {}, {}, 1 } },
        { MONS_HELLBINDER, { { { WPN_DEMON_BLADE, 1 } } } },
        { MONS_IGNACIO, {
            { { WPN_EXECUTIONERS_AXE, 1 } },
            { 1, 2, 8 },
            { { SPWPN_PAIN, 1 } },
        } },
        { MONS_ANCIENT_CHAMPION, {
            { { WPN_GREAT_MACE,         1 },
              { WPN_BATTLEAXE,          1 },
              { WPN_GREAT_SWORD,        1 }, },
            { 1, 0, 3 },
            { { SPWPN_DRAINING,      13 }, // total 45
              { SPWPN_HEAVY,         7 },
              { SPWPN_FREEZING,      4 },
              { SPWPN_FLAMING,       4 },
              { SPWPN_PAIN,          2 },
              { NUM_SPECIAL_WEAPONS, 15 } }, // 2/3 chance of brand
        } },
        { MONS_SOJOBO, {
            { { WPN_TRIPLE_SWORD,       1 },
              { WPN_GREAT_SWORD,        5 } }, {},
            { { SPWPN_ELECTROCUTION, 2 },
              { NUM_SPECIAL_WEAPONS, 1 } },
            1,
        } },
        { MONS_ZENATA, {
            { { WPN_TRIPLE_SWORD,       1 },
              { WPN_EXECUTIONERS_AXE,   1 },
              { WPN_BARDICHE,           1 },
              { WPN_EVENINGSTAR,        1 },
              { WPN_PARTISAN,           1 },
              { WPN_LAJATANG,           1 },
            }, { 1, 2, 5 },
        } },
        { MONS_DEMONSPAWN_CORRUPTER,   { DS_WEAPONS } },
        { MONS_DEMONSPAWN_SOUL_SCHOLAR,   { DS_WEAPONS } },
        { MONS_DEMONSPAWN_BLOOD_SAINT, {
            { { WPN_DAGGER,             4 },
              { WPN_QUARTERSTAFF,       1 } },
        } },
        { MONS_DEMONSPAWN_WARMONGER, {
            { { WPN_DEMON_BLADE,        10 },
              { WPN_DEMON_WHIP,         10 },
              { WPN_DEMON_TRIDENT,      10 },
              { WPN_BATTLEAXE,          7 },
              { WPN_GREAT_SWORD,        5 },
              { WPN_DOUBLE_SWORD,       2 },
              { WPN_DIRE_FLAIL,         5 },
              { WPN_GREAT_MACE,         2 },
              { WPN_GLAIVE,             5 },
              { WPN_PARTISAN,           4 },
              { WPN_BARDICHE,           2 },
              { WPN_LAJATANG,           1 }, },
           {}, {}, 1,
        } },
        { MONS_GARGOYLE,                { GARGOYLE_WEAPONS } },
        { MONS_MOLTEN_GARGOYLE,         { GARGOYLE_WEAPONS } },
        { MONS_WAR_GARGOYLE, {
            { { WPN_MORNINGSTAR,        10 },
              { WPN_FLAIL,              10 },
              { WPN_DIRE_FLAIL,         5 },
              { WPN_GREAT_MACE,         5 },
              { WPN_LAJATANG,           1 } },
            {}, {}, 4,
        } },
        { MONS_MELIAI, { // labrys
            { { WPN_HAND_AXE,           12 },
              { WPN_WAR_AXE,            7 },
              { WPN_BROAD_AXE,          1 }, },
        } },
        { MONS_IMPERIAL_MYRMIDON, {
            { { WPN_SCIMITAR,           20 },
              { WPN_DEMON_BLADE,        4 },
              { WPN_DOUBLE_SWORD,       1 }, },
        } },
        { MONS_KILLER_KLOWN, {
            { { WPN_CLUB,             10 }, },
            { 1, 8, 12 },
            { { SPWPN_NORMAL,         20 },
              { SPWPN_FLAMING,        20 },
              { SPWPN_FREEZING,       10 },
              { SPWPN_HEAVY,          10 },
              { SPWPN_ELECTROCUTION,  10 },
              { SPWPN_VENOM,          10 },
              { SPWPN_VAMPIRISM,       5 },
              { SPWPN_ANTIMAGIC,       5 },
              { SPWPN_SPECTRAL,        5 },
              { SPWPN_PAIN,            4 },
              { SPWPN_HOLY_WRATH,      3 },
              { SPWPN_DISTORTION,      2 },
              { SPWPN_CHAOS,           1 }, },
        } },
        { MONS_CERULEAN_IMP,  { { { WPN_SPEAR, 1 } }, {}, { { SPWPN_NORMAL, 1 } } } },
    };

    static const weapon_list ORC_KNIGHT_BOWS =
    {   { WPN_ARBALEST,                 1 },
        { NUM_WEAPONS,                  8 }, }; // 1/9 chance of ranged weapon


    static const map<monster_type, mon_weapon_spec> secondary_weapon_specs = {
        { MONS_JOSEPH, { { { WPN_SLING, 1 } } } },
        { MONS_DEEP_ELF_ARCHER,
            { { { WPN_SHORTBOW,         3 },
                { WPN_LONGBOW,          1 },
        } } },
        { MONS_VASHNIA,
            { { { WPN_LONGBOW,          1 },
                { WPN_ARBALEST,         1 },
        } } },
        { MONS_NAGA_SHARPSHOOTER,
            { { { WPN_SHORTBOW,         4 },
                { WPN_ARBALEST,         2 },
                { WPN_LONGBOW,          1 }
        } } },
        { MONS_VAULT_WARDEN,            { { { WPN_ARBALEST, 1 }, { NUM_WEAPONS, 8 }} } },
        { MONS_ORC_WARLORD,             { ORC_KNIGHT_BOWS } },
        { MONS_SAINT_ROKA,              { ORC_KNIGHT_BOWS } },
        { MONS_ORC_KNIGHT,              { ORC_KNIGHT_BOWS } },
        { MONS_ORC_WARRIOR,
            { { { WPN_ORCBOW,           3 },
                { WPN_ARBALEST,         1 },
                { NUM_WEAPONS,          32 }
        } } },
        { MONS_TENGU_WARRIOR,
            { { { WPN_ARBALEST,                 1 },
                { NUM_WEAPONS,                  8 }, // 1/9 chance of weap
        } } },
        { MONS_VAULT_SENTINEL,
            { { { WPN_ARBALEST,                 19 },
                { WPN_HAND_CANNON,            1 },
                { NUM_WEAPONS,                  40 },
        } } },
        { MONS_FAUN, { { { WPN_SLING, 1 } } } },
        { MONS_SATYR, { { { WPN_LONGBOW, 1 } } } },
        { MONS_CHERUB,
            { { { WPN_SLING,                    1 },
                { WPN_SHORTBOW,                 1 },
                { WPN_LONGBOW,                  1 },
        } } },
        // salamanders only have secondary weapons; melee or bow, not both
        { MONS_SALAMANDER, {
            { { WPN_HALBERD,                    5 },
              { WPN_TRIDENT,                    5 },
              { WPN_SPEAR,                      3 },
              { WPN_PARTISAN,                   2 },
              { WPN_SHORTBOW,                   5 }, },
            { 4, 0, 4 },
        } },
        { MONS_NEKOMATA,
            { { { WPN_SLING,                    9 },
                { WPN_ARBALEST,                 9 },
                { WPN_HAND_CANNON,              1 }, },
              { 4, 2, 4 },
              { { SPWPN_FLAMING,                3 },
                { SPWPN_DRAINING,               2 }, },
        } },
        { MONS_DEMONSPAWN_WARMONGER, {
            { { WPN_LONGBOW,                    10 }, // total 60
              { WPN_ARBALEST,                   9 },
              { WPN_TRIPLE_CROSSBOW,            1 },
              { NUM_WEAPONS,                    40 }, }, // 1/3 odds of weap
            {}, {}, 1,
        } },
    };

    bool force_item = false;

    string floor_tile = "";
    string equip_tile = "";
    string custom_name = "";

    item_def item;
    item.base_type = OBJ_UNASSIGNED;
    item.quantity = 1;

    // do we have a secondary weapon to give the monster? (usually ranged)
    const mon_weapon_spec *secondary_spec = map_find(secondary_weapon_specs,
                                                     type);
    if (!secondary_spec || melee_only ||
        !_apply_weapon_spec(*secondary_spec, item, force_item, level))
    {
        // either we're just giving only giving out primary weapons in this
        // call, or we didn't find a secondary weapon to give. either way,
        // try to give the monster a primary weapon. (may be its second!)
        const mon_weapon_spec *primary_spec = map_find(primary_weapon_specs,
                                                       type);
        if (primary_spec)
            _apply_weapon_spec(*primary_spec, item, force_item, level);
    }

    // special cases.
    switch (type)
    {
    case MONS_KOBOLD:
    case MONS_KOBOLD_BRIGAND:
        item.base_type = OBJ_WEAPONS;
        item.sub_type  = random_choose(WPN_DAGGER,      WPN_DAGGER,
                                       WPN_SHORT_SWORD, WPN_SHORT_SWORD,
                                       WPN_CLUB,        WPN_WHIP);
        break;

    case MONS_GOBLIN:
        if (!melee_only && one_chance_in(12) && level)
        {
            item.base_type = OBJ_WEAPONS;
            item.sub_type  = WPN_SLING;
            break;
        }
        break;

    case MONS_GNOLL:
        if (!level && item.is_type(OBJ_WEAPONS, WPN_HALBERD))
            item.sub_type = WPN_CLUB;
        break;

    case MONS_WIGHT:
        if (coinflip())
        {
            force_item = true;
            item.plus += 1 + random2(3);

            if (one_chance_in(5))
                set_item_ego_type(item, OBJ_WEAPONS, SPWPN_FREEZING);
        }
        break;

    case MONS_JORGRUN:
        force_item = true;
        if (one_chance_in(3))
        {
            item.base_type = OBJ_STAVES;
            item.sub_type = STAFF_EARTH;
        }
        else
        {
            item.base_type = OBJ_WEAPONS;
            item.sub_type = WPN_QUARTERSTAFF;
            set_item_ego_type(item, OBJ_WEAPONS, SPWPN_HEAVY);
        }
        break;

    case MONS_CYCLOPS:
    case MONS_STONE_GIANT:
    case MONS_TAINTED_LEVIATHAN:
    case MONS_PARGHIT:
        item.base_type = OBJ_MISSILES;
        item.sub_type  = MI_LARGE_ROCK;
        break;

    case MONS_MERFOLK_IMPALER:
    case MONS_MERFOLK_JAVELINEER:
    case MONS_AGNES:
        if (!one_chance_in(3))
            level = ISPEC_GOOD_ITEM;
        break;

    case MONS_WAR_GARGOYLE:
        if (one_chance_in(3))
            level = ISPEC_GOOD_ITEM;
        break;

    case MONS_MERFOLK:
        if (active_monster_band == BAND_MERFOLK_IMPALER)
        {
            item.base_type = OBJ_WEAPONS;
            item.sub_type  = random_choose_weighted(10, WPN_SPEAR,
                                                    10, WPN_TRIDENT,
                                                    5, WPN_HALBERD,
                                                    5, WPN_GLAIVE);
        }
        break;

    case MONS_DONALD:
    case MONS_FREDERICK:
    case MONS_URUG:
        if (x_chance_in_y(5, 9))
            level = ISPEC_GOOD_ITEM;
        else if (type != MONS_DONALD)
        {
            item.plus += random2(6);
            force_item = true;
        }
        break;

    case MONS_JOSEPHINA:
        item.base_type = OBJ_STAVES;
        item.sub_type = STAFF_COLD;
        break;

    case MONS_FANNAR:
        force_item = true;
        if (one_chance_in(3))
        {
            item.base_type = OBJ_STAVES;
            item.sub_type = STAFF_COLD;
        }
        else
        {
            item.base_type = OBJ_WEAPONS;
            item.sub_type = WPN_QUARTERSTAFF;
            set_item_ego_type(item, OBJ_WEAPONS, SPWPN_FREEZING);
        }
        break;

    case MONS_NIKOLA:
        if (one_chance_in(100) && !get_unique_item_status(UNRAND_ARC_BLADE))
            make_item_unrandart(item, UNRAND_ARC_BLADE);
        break;

    case MONS_AMAEMON:
        if (one_chance_in(100) && !get_unique_item_status(UNRAND_SNAKEBITE))
            make_item_unrandart(item, UNRAND_SNAKEBITE);
        break;

    case MONS_XAKKRIXIS:
        force_item = true;
        item.base_type = OBJ_WEAPONS;
        item.plus += 1 + random2(4);
        item.sub_type = random_choose_weighted(15, WPN_GREAT_MACE,
                                               10, WPN_LAJATANG,
                                               10, WPN_QUARTERSTAFF,
                                                5, WPN_GLAIVE);
        set_item_ego_type(item, OBJ_WEAPONS, SPWPN_VENOM);
        break;

    case MONS_ARACHNE:
        force_item = true;
        item.base_type = OBJ_STAVES;
        item.sub_type = STAFF_ALCHEMY;
        if (one_chance_in(100) && !get_unique_item_status(UNRAND_OLGREB))
            make_item_unrandart(item, UNRAND_OLGREB);
        break;

    case MONS_CEREBOV:
        if (you.props.exists(CEREBOV_DISARMED_KEY))
            break;
        force_item = true;
        make_item_unrandart(item, UNRAND_CEREBOV);
        break;

    case MONS_ASMODEUS:
        force_item = true;
        make_item_unrandart(item, UNRAND_ASMODEUS);
        break;

    case MONS_GERYON:
        // mv: Probably should be moved out of this switch, but it's not
        // worth it, unless we have more monsters with misc. items.
        item.base_type = OBJ_MISCELLANY;
        item.sub_type  = MISC_HORN_OF_GERYON;
        break;

    case MONS_SALAMANDER:
        if (is_range_weapon(item))
        {
            set_item_ego_type(item, OBJ_WEAPONS, SPWPN_FLAMING);
            force_item = true;
        }
        break;

    case MONS_ENCHANTRESS:
        if (one_chance_in(6))
        {
            force_item = true;
            set_item_ego_type(item, OBJ_WEAPONS, SPWPN_DISTORTION);
            item.plus  = random2(5);
        }
        break;

    case MONS_MAGGIE:
        if (one_chance_in(100) && !get_unique_item_status(UNRAND_WYRMBANE))
        {
            make_item_unrandart(item, UNRAND_WYRMBANE);
            item.plus = 9; // Since she's wearing a dragon armour
            force_item = true;
        }
        break;

    case MONS_MARGERY:
        if (one_chance_in(100) && !get_unique_item_status(UNRAND_WYRMBANE))
        {
            make_item_unrandart(item, UNRAND_WYRMBANE);
            item.plus = 10 + random2(2); // Now she's killed at least 2 dragons
            force_item = true;
        }
        break;

        // As a violent thug, Throatcutter suits Terence perfectly.
    case MONS_TERENCE:
        if (one_chance_in(100) && !get_unique_item_status(UNRAND_THROATCUTTER))
        {
            make_item_unrandart(item, UNRAND_THROATCUTTER);
            force_item = true;
        }
        break;
    case MONS_GRUNN:
        if (one_chance_in(100) && !get_unique_item_status(UNRAND_CURSES))
            make_item_unrandart(item, UNRAND_CURSES);
        break;

    case MONS_SIGMUND:
    case MONS_REAPER:
        custom_name = "scythe";
        if (type == MONS_SIGMUND && december_holidays())
        {
            floor_tile  = "wpn_xmas_scythe";
            equip_tile  = "xmas_scythe";
        }
        else
        {
            floor_tile  = "wpn_scythe";
            equip_tile  = "scythe";
        }
        break;

    case MONS_ANCESTOR_HEXER:
    case MONS_ANCESTOR_BATTLEMAGE:
    case MONS_ANCESTOR_KNIGHT:
        force_item = true;
        upgrade_hepliaklqana_weapon(type, item);
        break;

    default:
        break;
    }

    // Only happens if something in above switch doesn't set it. {dlb}
    if (item.base_type == OBJ_UNASSIGNED)
        return NON_ITEM;

    if (!force_item && mons_is_unique(type))
    {
        if (x_chance_in_y(10 + mons_class_hit_dice(type), 100))
            level = ISPEC_GOOD_ITEM;
        else if (level != ISPEC_GOOD_ITEM)
            level += 5;
    }

    const object_class_type xitc = item.base_type;
    const int xitt = item.sub_type;

    // Note this mess, all the work above doesn't mean much unless
    // force_item is set... otherwise we're just going to take the base
    // and subtype and create a new item. - bwr
    const int thing_created =
        ((force_item) ? get_mitm_slot() : items(false, xitc, xitt, level,
                                                item.brand, NO_AGENT,
                                                custom_name));

    if (thing_created == NON_ITEM)
        return NON_ITEM;

    // Copy temporary item into the item array if were forcing it, since
    // items() won't have done it for us.
    if (force_item)
        env.item[thing_created] = item;
    else
        env.item[thing_created].flags |= item.flags;

    item_def &i = env.item[thing_created];
    if (melee_only && (i.base_type != OBJ_WEAPONS || is_range_weapon(i)))
    {
        destroy_item(thing_created);
        return NON_ITEM;
    }

    if (force_item)
        item_set_appearance(i);

    if (!is_artefact(env.item[thing_created]) && !floor_tile.empty())
    {
        ASSERT(!equip_tile.empty());
        env.item[thing_created].props[ITEM_TILE_NAME_KEY] = floor_tile;
        env.item[thing_created].props[WORN_TILE_NAME_KEY] = equip_tile;
        bind_item_tile(env.item[thing_created]);
    }

    return thing_created;
}

/**
 * If you give a monster a weapon, he's going to ask for a glass of milk.
 *
 * @param mon               The monster to give a weapon to.
 * @param level             Item level, which might be absdepth.
 * @param second_weapon     Whether this is a recursive call to the function
 *                          to give the monster a melee weapon, after we've
 *                          already given them something else.
 */
static void _give_weapon(monster *mon, int level, bool second_weapon = false)
{
    ASSERT(mon); // TODO: change to monster &mon

    if (mon->type == MONS_DEEP_ELF_BLADEMASTER && mon->weapon())
    {
        const item_def &first_sword = *mon->weapon();
        ASSERT(first_sword.base_type == OBJ_WEAPONS);
        item_def twin_sword = first_sword; // copy
        give_specific_item(mon, twin_sword);
        return;
    }

    const int thing_created = make_mons_weapon(mon->type, level, second_weapon);
    if (thing_created == NON_ITEM)
        return;

    give_specific_item(mon, thing_created);
    if (second_weapon)
        return;

    const item_def &i = env.item[thing_created];

    if ((i.base_type != OBJ_WEAPONS
                && i.base_type != OBJ_STAVES
                && i.base_type != OBJ_MISCELLANY) // don't double-gift geryon horn
               || is_range_weapon(i))
    {
        _give_weapon(mon, level, true);
    }

    if (mon->type == MONS_ERICA && i.is_type(OBJ_WEAPONS, WPN_SCIMITAR))
        make_item_for_monster(mon, OBJ_JEWELLERY, NUM_RINGS, 0, 1);

    if (mon->type == MONS_FANNAR && i.is_type(OBJ_WEAPONS, WPN_QUARTERSTAFF))
        make_item_for_monster(mon, OBJ_JEWELLERY, RING_ICE, 0, 1);

    if (mon->type == MONS_WIGLAF)
    {
        // Always good, and sometimes especially good
        item_def* hat = make_item_for_monster(mon, OBJ_ARMOUR, ARM_HAT, ISPEC_RANDART);
        hat->plus = random_range(2, 4);

        hat->props[ITEM_TILE_NAME_KEY] = "THELM_HAT_WIGLAF";
        hat->props[WORN_TILE_NAME_KEY] = "hat_wiglaf";
        bind_item_tile(*hat);
    }

    if (mon->type == MONS_JOSEPHINA)
        make_item_for_monster(mon, OBJ_JEWELLERY, RING_ICE, ISPEC_RANDART, true);
}

// Hands out ammunition fitting the monster's launcher (if any), or else any
// throwable missiles depending on the monster type.
static void _give_ammo(monster* mon, int level, bool mons_summoned)
{
    if (mon->launcher())
        return;

    // Give some monsters throwables.
    int weap_type = -1;
    int qty = 0;
    int brand = SPMSL_NORMAL;

    switch (mon->type)
    {
    case MONS_KOBOLD:
        if (one_chance_in(10) && level > 1)
        {
            weap_type  = MI_DART;
            qty = random_range(2, 8);
            brand = SPMSL_POISONED;
            break;
        }
        if (x_chance_in_y(2, 5))
        {
            weap_type  = MI_STONE;
            qty = 1 + random2(5);
        }
        break;
    case MONS_KOBOLD_BRIGAND:
        weap_type  = MI_DART;
        if (one_chance_in(3))
        {
            // Avoid increasing total amount of generated curare
            // too much.
            brand = SPMSL_CURARE;
            qty = 1;
        }
        else
        {
            qty = random_range(2, 8);
            brand = SPMSL_POISONED;
        }
        break;

    case MONS_SONJA:
        weap_type = MI_DART;
        brand = SPMSL_CURARE;
        qty = random_range(2, 5);
        break;

    case MONS_SPRIGGAN_RIDER:
        if (one_chance_in(15))
        {
            weap_type = MI_DART;
            brand = SPMSL_CURARE;
            qty = random_range(1, 4);
        }
        break;

    case MONS_ORC_WARRIOR:
        if (one_chance_in(
                player_in_branch(BRANCH_ORC)? 9 : 20))
        {
            weap_type = MI_BOOMERANG;
            qty       = random_range(1, 3);
        }
        break;

    case MONS_ORC:
        if (one_chance_in(20))
        {
            weap_type = MI_BOOMERANG;
            qty       = random_range(1, 3);
        }
        break;

    case MONS_URUG:
        weap_type  = MI_JAVELIN;
        qty = random_range(4, 7);
        break;

    // Sprint-only.
    case MONS_CHUCK:
        weap_type  = MI_LARGE_ROCK;
        brand = SPMSL_CHAOS;
        qty = random_range(2, 4);
        break;

    case MONS_POLYPHEMUS:
        weap_type  = MI_LARGE_ROCK;
        qty        = random_range(8, 12);
        break;

    case MONS_MERFOLK_JAVELINEER:
    case MONS_MINOTAUR:
        weap_type  = MI_JAVELIN;
        qty        = random_range(1, 3);
        if (mon->type == MONS_MINOTAUR)
            qty += random_range(1, 5);
        if (one_chance_in(3))
            level = ISPEC_GOOD_ITEM;
        break;

    case MONS_MERFOLK:
        if (one_chance_in(3)
            || active_monster_band == BAND_MERFOLK_JAVELINEER)
        {
            weap_type  = MI_BOOMERANG;
            qty        = random_range(1, 3);
            if (active_monster_band == BAND_MERFOLK_JAVELINEER)
                break;
        }
        if (one_chance_in(8) && !mons_summoned)
        {
            weap_type  = MI_THROWING_NET;
            qty        = 1;
            if (one_chance_in(4))
                qty += random2(3); // up to three nets
        }
        break;

    case MONS_DRACONIAN_KNIGHT:
    case MONS_GNOLL:
        if (!level || !one_chance_in(20))
            break;
        // deliberate fall-through to harold

    case MONS_HAROLD: // bounty hunter, up to 5 nets
        if (mons_summoned)
            break;

        weap_type  = MI_THROWING_NET;
        qty        = 1;
        if (one_chance_in(3))
            qty++;
        if (mon->type == MONS_HAROLD)
            qty += random2(4);

        break;

    default:
        break;
    }

    if (weap_type == -1)
        return;

    const int thing_created = items(false, OBJ_MISSILES, weap_type, level);

    if (thing_created != NON_ITEM)
    {
        item_def& w(env.item[thing_created]);

        if (brand != SPMSL_NORMAL)
            set_item_ego_type(w, OBJ_MISSILES, brand);

        w.quantity = qty;
        give_specific_item(mon, thing_created);
    }
}

static item_def* make_item_for_monster(
    monster* mons,
    object_class_type base,
    int subtype,
    int level,
    int allow_uniques,
    iflags_t flags)
{
    const int bp = get_mitm_slot();
    if (bp == NON_ITEM)
        return 0;

    const int thing_created = items(allow_uniques, base, subtype, level);
    if (thing_created == NON_ITEM)
        return 0;

    env.item[thing_created].flags |= flags;

    give_specific_item(mons, thing_created);
    return &env.item[thing_created];
}

static void _give_shield(monster* mon, int level)
{
    const item_def *main_weap = mon->mslot_item(MSLOT_WEAPON);
    const item_def *alt_weap  = mon->mslot_item(MSLOT_ALT_WEAPON);
    item_def *shield;

    // If the monster is already wielding/carrying a two-handed weapon,
    // it doesn't get a shield. (Monsters always prefer raw damage to
    // protection!)
    if (main_weap && mon->hands_reqd(*main_weap) == HANDS_TWO
        || alt_weap && mon->hands_reqd(*alt_weap) == HANDS_TWO)
    {
        return;
    }

    switch (mon->type)
    {
    case MONS_ASTERION:
    case MONS_XAKKRIXIS:
    case MONS_WIGLAF:
        make_item_for_monster(mon, OBJ_ARMOUR, ARM_KITE_SHIELD,
                              level * 2 + 1, 1);
        break;

    case MONS_DISPATER:
        shield = make_item_for_monster(mon, OBJ_ARMOUR, ARM_ORB, level);
        if (shield)
            make_item_unrandart(*shield, UNRAND_DISPATER);
        break;

    case MONS_JEREMIAH:
        shield = make_item_for_monster(mon, OBJ_ARMOUR, ARM_ORB, level);
        if (shield)
        {
            // Light is good-coded and Wrath is too vicious.
            const auto ego = random_choose(SPARM_MAYHEM, SPARM_ENERGY, SPARM_GUILE);
            set_item_ego_type(*shield, OBJ_ARMOUR, ego);
        }
        break;

    case MONS_FREDERICK:
    {
        // Divinity or conjurer support.
        const auto ego = random_choose(SPARM_LIGHT, SPARM_ENERGY);

        give_specific_item(mon, items(false, OBJ_ARMOUR,
                           ARM_ORB, ISPEC_RANDART, ego));
    }
    break;

    case MONS_DAEVA:
    case MONS_MENNAS:
        make_item_for_monster(mon, OBJ_ARMOUR, ARM_TOWER_SHIELD,
                              level * 2 + 1, 1);
        break;

    case MONS_CHERUB:
        // Big shields interfere with ranged combat, at least theme-wise.
        make_item_for_monster(mon, OBJ_ARMOUR, ARM_BUCKLER, level, 1);
        break;

    case MONS_NAGA_WARRIOR:
        if (coinflip())
            level = ISPEC_GOOD_ITEM;
        // deliberate fall-through
    case MONS_VAULT_GUARD:
    case MONS_VAULT_WARDEN:
    case MONS_ORC_WARLORD:
        if (one_chance_in(3))
        {
            make_item_for_monster(mon, OBJ_ARMOUR,
                                  one_chance_in(3) ? ARM_TOWER_SHIELD
                                                   : ARM_KITE_SHIELD,
                                  level);
        }
        break;

    case MONS_DRACONIAN_KNIGHT:
        if (coinflip())
        {
            make_item_for_monster(mon, OBJ_ARMOUR,
                                  random_choose(ARM_TOWER_SHIELD, ARM_KITE_SHIELD),
                                  level);
        }
        break;
    case MONS_TENGU_WARRIOR:
        if (one_chance_in(3))
            level = ISPEC_GOOD_ITEM;
        // deliberate fall-through
    case MONS_TENGU:
    case MONS_GNOLL_SERGEANT:
        if (mon->type != MONS_TENGU_WARRIOR && !one_chance_in(3))
            break;
        make_item_for_monster(mon, OBJ_ARMOUR,
                              random_choose(ARM_BUCKLER, ARM_KITE_SHIELD),
                              level);
        break;

    case MONS_TENGU_REAVER:
        if (one_chance_in(3))
            level = ISPEC_GOOD_ITEM;
        make_item_for_monster(mon, OBJ_ARMOUR, ARM_BUCKLER, level);
        break;

    case MONS_TENGU_CONJURER:
    case MONS_DEEP_ELF_KNIGHT:
        if (one_chance_in(3))
            make_item_for_monster(mon, OBJ_ARMOUR, ARM_BUCKLER, level);
        break;

    case MONS_SPRIGGAN:
    case MONS_SPRIGGAN_RIDER:
        if (!one_chance_in(4))
            break;
    // else fall-through
    case MONS_SPRIGGAN_DEFENDER:
    case MONS_ENCHANTRESS:
        shield = make_item_for_monster(mon, OBJ_ARMOUR, ARM_BUCKLER,
                      mon->type == MONS_ENCHANTRESS ? ISPEC_GOOD_ITEM :
                      mon->type == MONS_SPRIGGAN_DEFENDER ? level * 2 + 1 :
                      level);
        if (shield && !is_artefact(*shield)) // ineligible...
        {
            shield->props[ITEM_TILE_NAME_KEY] = "buckler_spriggan";
            shield->props[WORN_TILE_NAME_KEY] = "buckler_spriggan";
            bind_item_tile(*shield);
        }
        break;

    case MONS_LOUISE:
        shield = make_item_for_monster(mon, OBJ_ARMOUR, ARM_TOWER_SHIELD,
                                       level * 2 + 1, 1);
        if (shield && !is_artefact(*shield))
        {
            shield->props[ITEM_TILE_NAME_KEY] = "tower shield_louise";
            shield->props[WORN_TILE_NAME_KEY] = "tower shield_louise";
            bind_item_tile(*shield);
        }
        break;

    case MONS_DONALD:
        shield = make_item_for_monster(mon, OBJ_ARMOUR, ARM_KITE_SHIELD,
                                       level * 2 + 1, 1);

        if (shield)
        {
            if (coinflip())
            {
                set_item_ego_type(*shield, OBJ_ARMOUR, SPARM_REFLECTION);
                set_equip_desc(*shield, ISFLAG_GLOWING);
            }
            if (!is_artefact(*shield))
            {
                shield->props[ITEM_TILE_NAME_KEY] = "kite_shield_donald";
                shield->props[WORN_TILE_NAME_KEY] = "kite_shield_donald";
                bind_item_tile(*shield);
            }
        }

        break;

    case MONS_NIKOLA:
        shield = make_item_for_monster(mon, OBJ_ARMOUR, ARM_GLOVES,
                                       level * 2 + 1, 1);
        break;

    case MONS_ROBIN:
        // The Nikola Hack
        make_item_for_monster(mon, OBJ_ARMOUR, ARM_HELMET,
                              level * 2 + 1, 1);
        break;

    case MONS_DEMONSPAWN_CORRUPTER:
    case MONS_DEMONSPAWN_SOUL_SCHOLAR:
        if (one_chance_in(3))
        {
            armour_type shield_type = random_choose(ARM_BUCKLER, ARM_KITE_SHIELD);
            make_item_for_monster(mon, OBJ_ARMOUR, shield_type, level);
        }
        break;

    case MONS_DEMONSPAWN_WARMONGER:
        make_item_for_monster(mon, OBJ_ARMOUR,
                              random_choose(ARM_TOWER_SHIELD, ARM_KITE_SHIELD),
                              ISPEC_GOOD_ITEM);
        break;

    case MONS_ANCESTOR_KNIGHT:
    {
        item_def shld;
        upgrade_hepliaklqana_shield(*mon, shld);
        if (!shld.defined())
            break;

        item_set_appearance(shld);

        const int thing_created = get_mitm_slot();
        if (thing_created == NON_ITEM)
            break;

        env.item[thing_created] = shld;
        give_specific_item(mon, thing_created);
    }
        break;

    default:
        break;
    }
}

int make_mons_armour(monster_type type, int level)
{
    item_def               item;

    item.base_type = OBJ_UNASSIGNED;
    item.quantity  = 1;

    bool force_item = false;

    switch (type)
    {
    case MONS_DEEP_ELF_ARCHER:
    case MONS_DEEP_ELF_BLADEMASTER:
    case MONS_DEEP_ELF_MASTER_ARCHER:
    case MONS_MERFOLK_JAVELINEER:
    case MONS_EUSTACHIO:
        item.base_type = OBJ_ARMOUR;
        item.sub_type  = ARM_LEATHER_ARMOUR;
        break;

    case MONS_ORC_HIGH_PRIEST:
    case MONS_DEEP_ELF_FIRE_MAGE:
    case MONS_DEEP_ELF_AIR_MAGE:
    case MONS_DEEP_ELF_KNIGHT:
    case MONS_DEEP_ELF_ANNIHILATOR:
    case MONS_DEEP_ELF_DEATH_MAGE:
    case MONS_DEEP_ELF_DEMONOLOGIST:
    case MONS_DEEP_ELF_ELEMENTALIST:
    case MONS_DEEP_ELF_HIGH_PRIEST:
    case MONS_DEEP_ELF_SORCERER:
        item.base_type = OBJ_ARMOUR;
        item.sub_type  = random_choose_weighted(12, ARM_ROBE,
                                                4, ARM_LEATHER_ARMOUR,
                                                3, ARM_RING_MAIL,
                                                1, ARM_SCALE_MAIL);
        break;

    case MONS_ORC:
    case MONS_ORC_PRIEST:
        if (x_chance_in_y(2, 5))
        {
    case MONS_DUVESSA:
    case MONS_IJYB:
            item.base_type = OBJ_ARMOUR;
            item.sub_type  = random_choose_weighted(4, ARM_LEATHER_ARMOUR,
                                                    2, ARM_RING_MAIL,
                                                    1, ARM_SCALE_MAIL,
                                                    1, ARM_CHAIN_MAIL);
        }
        else
            return NON_ITEM; // don't hand out too much armour to early orcs
        break;

    case MONS_GNOLL_BOUDA:
    case MONS_MELIAI:
    case MONS_HALAZID_WARLOCK:
        item.base_type = OBJ_ARMOUR;
        item.sub_type  = random_choose(ARM_ROBE, ARM_LEATHER_ARMOUR);
        break;

    case MONS_SOJOBO:
        level = ISPEC_GOOD_ITEM;
        // deliberate fall-through to ring/scale
    case MONS_TENGU_REAVER:
        if (one_chance_in(3))
            level = ISPEC_GOOD_ITEM;
        // deliberate fall-through to ring/scale
    case MONS_GNOLL_SERGEANT:
    case MONS_VAULT_SENTINEL:
    case MONS_IRONBOUND_CONVOKER:
    case MONS_IRONBOUND_FROSTHEART:
        item.base_type = OBJ_ARMOUR;
        item.sub_type  = random_choose(ARM_RING_MAIL, ARM_SCALE_MAIL);
        break;

    case MONS_JOSEPH:
    case MONS_IMPERIAL_MYRMIDON:
    case MONS_ZENATA:
        item.base_type = OBJ_ARMOUR;
        item.sub_type  = random_choose_weighted(3, ARM_LEATHER_ARMOUR,
                                                2, ARM_RING_MAIL);
        break;

    case MONS_TERENCE:
    case MONS_URUG:
    case MONS_HAROLD:
        item.base_type = OBJ_ARMOUR;
        item.sub_type  = random_choose_weighted(1, ARM_RING_MAIL,
                                                3, ARM_SCALE_MAIL,
                                                2, ARM_CHAIN_MAIL);
        break;

    case MONS_GRUM:
    case MONS_ROBIN:
    case MONS_SPRIGGAN_BERSERKER:
    case MONS_IRONBOUND_THUNDERHULK:
    case MONS_IRONBOUND_BEASTMASTER:
    case MONS_GRUNN:
        item.base_type = OBJ_ARMOUR;
        item.sub_type  = ARM_ANIMAL_SKIN;
        break;

    case MONS_ASTERION:
    case MONS_EDMUND:
    case MONS_FRANCES:
    case MONS_RUPERT:
    {
        item.base_type = OBJ_ARMOUR;
        item.sub_type  = random_choose(ARM_LEATHER_ARMOUR, ARM_RING_MAIL,
                                       ARM_SCALE_MAIL,     ARM_CHAIN_MAIL);
        break;
    }

    case MONS_DONALD:
    case MONS_WIGLAF:
        if (one_chance_in(3))
            level = ISPEC_GOOD_ITEM;
        item.base_type = OBJ_ARMOUR;
        item.sub_type = random_choose_weighted(10, ARM_CHAIN_MAIL,
                                               9, ARM_PLATE_ARMOUR,
                                               1, ARM_CRYSTAL_PLATE_ARMOUR);
        break;

    case MONS_ORC_WARLORD:
    case MONS_SAINT_ROKA:
        // Being at the top has its privileges. :)
        if (one_chance_in(3))
            level = ISPEC_GOOD_ITEM;
        // deliberate fall through to chain/plate
    case MONS_ORC_KNIGHT:
    case MONS_ORC_WARRIOR:
    case MONS_HELL_KNIGHT:
    case MONS_LOUISE:
    case MONS_VAMPIRE_KNIGHT:
    case MONS_JORY:
    case MONS_FREDERICK:
    case MONS_VAULT_GUARD:
    case MONS_VAULT_WARDEN:
    case MONS_ANCIENT_CHAMPION:
    case MONS_MINOTAUR:
        item.base_type = OBJ_ARMOUR;
        item.sub_type  = random_choose(ARM_CHAIN_MAIL, ARM_PLATE_ARMOUR);
        break;

    case MONS_MAGGIE:
        item.base_type = OBJ_ARMOUR;
        item.sub_type = random_choose_weighted(3, ARM_ACID_DRAGON_ARMOUR,
                                               3, ARM_SWAMP_DRAGON_ARMOUR,
                                               6, ARM_STEAM_DRAGON_ARMOUR);
        break;

    case MONS_MARGERY:
        item.base_type = OBJ_ARMOUR;
        item.sub_type = random_choose_weighted(3, ARM_STORM_DRAGON_ARMOUR,
                                               3, ARM_SHADOW_DRAGON_ARMOUR,
                                               6, ARM_FIRE_DRAGON_ARMOUR);
        break;

    case MONS_PARGHIT:
        item.base_type = OBJ_ARMOUR;
        item.sub_type = ARM_GOLDEN_DRAGON_ARMOUR;
        if (one_chance_in(100) && !get_unique_item_status(UNRAND_DRAGON_KING))
            make_item_unrandart(item, UNRAND_DRAGON_KING);
        else
            item.plus = random_range(6, 9);
        force_item = true;
        break;

    case MONS_DEATH_KNIGHT:
        item.base_type = OBJ_ARMOUR;
        item.sub_type  = random_choose_weighted(7, ARM_CHAIN_MAIL,
                                                1, ARM_PLATE_ARMOUR);
        break;

    case MONS_MERFOLK_IMPALER:
        item.base_type = OBJ_ARMOUR;
        item.sub_type = random_choose_weighted(6, ARM_ROBE,
                                               4, ARM_LEATHER_ARMOUR);
        if (one_chance_in(16))
            level = ISPEC_GOOD_ITEM;
        break;

    case MONS_SERAPH:
        level          = ISPEC_GOOD_ITEM;
        item.base_type = OBJ_ARMOUR;
        // obscenely good, don't ever place them randomly
        item.sub_type  = coinflip() ? ARM_PEARL_DRAGON_ARMOUR
                                    : ARM_FIRE_DRAGON_ARMOUR;
        break;

    // Centaurs sometimes wear barding.
    case MONS_CENTAUR:
    case MONS_CENTAUR_WARRIOR:
    case MONS_YAKTAUR:
    case MONS_YAKTAUR_CAPTAIN:
        if (one_chance_in(type == MONS_CENTAUR              ? 1000 :
                          type == MONS_CENTAUR_WARRIOR      ?  500 :
                          type == MONS_YAKTAUR              ?  300
                       /* type == MONS_YAKTAUR_CAPTAIN ? */ :  200))
        {
            item.base_type = OBJ_ARMOUR;
            item.sub_type  = ARM_BARDING;
        }
        else
            return NON_ITEM; // ???
        break;

    case MONS_NAGA:
    case MONS_NAGA_MAGE:
    case MONS_NAGA_RITUALIST:
    case MONS_NAGA_SHARPSHOOTER:
    case MONS_NAGA_WARRIOR:
    case MONS_NAGARAJA:
        if (one_chance_in(type == MONS_NAGA         ?  800 :
                          type == MONS_NAGA_WARRIOR ?  300 :
                          type == MONS_NAGARAJA     ?  100
                                                    :  200))
        {
            item.base_type = OBJ_ARMOUR;
            item.sub_type  = ARM_BARDING;
        }
        else if (type == MONS_NAGARAJA
                 || type == MONS_NAGA_RITUALIST
                 || one_chance_in(3))
        {
            item.base_type = OBJ_ARMOUR;
            item.sub_type  = ARM_ROBE;
        }
        else
            return NON_ITEM; // ???
        break;

    case MONS_VASHNIA:
        item.base_type = OBJ_ARMOUR;
        item.sub_type  = ARM_BARDING;
        level = ISPEC_GOOD_ITEM;
        break;

    case MONS_TENGU_WARRIOR:
    case MONS_IRONBOUND_PRESERVER:
        item.base_type = OBJ_ARMOUR;
        item.sub_type  = random_choose(ARM_LEATHER_ARMOUR, ARM_RING_MAIL);
        break;

    case MONS_GASTRONOK:
        if (one_chance_in(10) && !get_unique_item_status(UNRAND_PONDERING))
        {
            force_item = true;
            make_item_unrandart(item, UNRAND_PONDERING);
        }
        else
        {
            item.base_type = OBJ_ARMOUR;
            item.sub_type  = ARM_HAT;

            // Not as good as it sounds. Still just +0 a lot of the time.
            level          = ISPEC_GOOD_ITEM;
        }
        break;

    case MONS_MAURICE:
    case MONS_CRAZY_YIUF:
        item.base_type = OBJ_ARMOUR;
        item.sub_type  = ARM_CLOAK;
        break;

    case MONS_FANNAR:
    {
        force_item = true;
        item.base_type = OBJ_ARMOUR;
        item.sub_type  = ARM_ROBE;
        item.plus = 1 + coinflip();
        set_item_ego_type(item, OBJ_ARMOUR, SPARM_COLD_RESISTANCE);
        break;
    }

    case MONS_JOSEPHINE:
    case MONS_PSYCHE:
        if (one_chance_in(5))
            level = ISPEC_GOOD_ITEM;
        item.base_type = OBJ_ARMOUR;
        item.sub_type  = ARM_ROBE;
        break;

    case MONS_BORIS:
        force_item = true;
        item.base_type = OBJ_ARMOUR;
        item.sub_type  = ARM_ROBE;
        make_item_randart(item);
        item.plus = random_range(2, 4);
        break;

    case MONS_JOSEPHINA:
    case MONS_VAMPIRE_BLOODPRINCE:
        level = ISPEC_GOOD_ITEM;
    case MONS_ORC_SORCERER:
    case MONS_DEMONSPAWN_BLOOD_SAINT:
    case MONS_JORGRUN:
        if (one_chance_in(3))
            level = ISPEC_GOOD_ITEM;
        // fallthrough to robes
    case MONS_ORC_WIZARD:
    case MONS_BLORKULA_THE_ORCULA:
    case MONS_NERGALLE:
    case MONS_DOWAN:
    case MONS_JESSICA:
    case MONS_KOBOLD_DEMONOLOGIST:
    case MONS_OGRE_MAGE:
    case MONS_EROLCHA:
    case MONS_ARCANIST:
    case MONS_OCCULTIST:
    case MONS_ILSUIW:
    case MONS_MARA:
    case MONS_RAKSHASA:
    case MONS_MERFOLK_AQUAMANCER:
    case MONS_SPRIGGAN:
    case MONS_SPRIGGAN_AIR_MAGE:
    case MONS_SPRIGGAN_DEFENDER:
    case MONS_ANGEL:
    case MONS_CHERUB:
    case MONS_SIGMUND:
    case MONS_WIGHT:
    case MONS_SPRIGGAN_DRUID:
    case MONS_AGNES:
    case MONS_BURIAL_ACOLYTE:
    case MONS_NECROMANCER:
    case MONS_VAMPIRE_MAGE:
    case MONS_PIKEL:
    case MONS_HELLBINDER:
    case MONS_SALAMANDER_MYSTIC:
    case MONS_SERVANT_OF_WHISPERS:
    case MONS_RAGGED_HIEROPHANT:
    case MONS_FENSTRIDER_WITCH:
    case MONS_JEREMIAH:
    case MONS_XAKKRIXIS:
        item.base_type = OBJ_ARMOUR;
        item.sub_type  = ARM_ROBE;
        break;

    case MONS_SPHINX_MARAUDER:
    case MONS_GUARDIAN_SPHINX:
    case MONS_DRACONIAN_SHIFTER:
    case MONS_DRACONIAN_SCORCHER:
    case MONS_DRACONIAN_ANNIHILATOR:
    case MONS_DRACONIAN_STORMCALLER:
    case MONS_DRACONIAN_MONK:
    case MONS_DRACONIAN_KNIGHT:
    case MONS_BAI_SUZHEN:
    case MONS_NIKOLA:
        item.base_type = OBJ_ARMOUR;
        item.sub_type  = ARM_CLOAK;
        break;

    case MONS_ENCHANTRESS:
        force_item = true;
        make_item_unrandart(item, UNRAND_FAERIE);
        break;

    case MONS_TIAMAT:
        force_item = true;
        make_item_unrandart(item, UNRAND_DRAGONSKIN);
        break;

    case MONS_NESSOS:
        item.base_type = OBJ_ARMOUR;
        item.sub_type  = ARM_BARDING;
        break;

    case MONS_DEMONSPAWN_CORRUPTER:
    case MONS_DEMONSPAWN_SOUL_SCHOLAR:
        item.base_type = OBJ_ARMOUR;
        item.sub_type  = random_choose_weighted(2, ARM_LEATHER_ARMOUR,
                                                3, ARM_RING_MAIL,
                                                5, ARM_SCALE_MAIL,
                                                3, ARM_CHAIN_MAIL,
                                                2, ARM_PLATE_ARMOUR);
        break;

    case MONS_DEMONSPAWN_WARMONGER:
        if (coinflip())
            level = ISPEC_GOOD_ITEM;
        item.base_type = OBJ_ARMOUR;
        item.sub_type  = random_choose_weighted( 50, ARM_CHAIN_MAIL,
                                                100, ARM_PLATE_ARMOUR,
                                                  5, ARM_FIRE_DRAGON_ARMOUR,
                                                  5, ARM_ICE_DRAGON_ARMOUR,
                                                  5, ARM_ACID_DRAGON_ARMOUR);
        break;

    case MONS_ONI_INCARCERATOR:
        if (coinflip())
            level = ISPEC_GOOD_ITEM;
        item.base_type = OBJ_ARMOUR;
        item.sub_type  = ARM_FIRE_DRAGON_ARMOUR;
        break;

    default:
        return NON_ITEM;
    }

    // Only happens if something in above switch doesn't set it. {dlb}
    if (item.base_type == OBJ_UNASSIGNED)
        return NON_ITEM;

    const object_class_type xitc = item.base_type;
    const int xitt = item.sub_type;

    if (!force_item && mons_is_unique(type) && level != ISPEC_GOOD_ITEM)
    {
        if (x_chance_in_y(9 + mons_class_hit_dice(type), 100))
            level = ISPEC_GOOD_ITEM;
        else
            level = level * 2 + 5;
    }

    // Note this mess, all the work above doesn't mean much unless
    // force_item is set... otherwise we're just going to take the base
    // and subtype and create a new item. - bwr
    const int thing_created =
        ((force_item) ? get_mitm_slot() : items(false, xitc, xitt, level));

    if (thing_created == NON_ITEM)
        return NON_ITEM;

    // Copy temporary item into the item array if were forcing it, since
    // items() won't have done it for us.
    if (force_item)
        env.item[thing_created] = item;

    item_def &i = env.item[thing_created];

    if (force_item)
        item_set_appearance(i);

    return thing_created;
}

static void _give_armour(monster* mon, int level)
{
    ASSERT(mon); // TODO: make monster &mon
    give_specific_item(mon, make_mons_armour(mon->type, level));
}

static void _give_gold(monster* mon, int level)
{
    switch (mon->type)
    {
    case MONS_LODUL:
        give_specific_item(mon, items(false, OBJ_GOLD, 0, ISPEC_GIFT));
        break;
    case MONS_MAURICE:
        give_specific_item(mon, items(false, OBJ_GOLD, 0, level));
        break;
    default:
        break;
    }
}

void give_weapon(monster *mons, int level_number)
{
    _give_weapon(mons, level_number);
}

void give_armour(monster *mons, int level_number)
{
    _give_armour(mons, 1 + level_number/2);
}

void give_shield(monster *mons)
{
    _give_shield(mons, -1);
}

void give_item(monster *mons, int level_number, bool mons_summoned)
{
    ASSERT(level_number > -1); // debugging absdepth0 changes

    _give_gold(mons, level_number);
    _give_talisman(mons, level_number);
    _give_wand(mons, level_number);
    _give_weapon(mons, level_number);
    _give_ammo(mons, level_number, mons_summoned);
    _give_armour(mons, 1 + level_number / 2);
    _give_shield(mons, 1 + level_number / 2);

    if (mons->type == MONS_ORC_APOSTLE)
        give_apostle_equipment(mons);
}

void view_monster_equipment(monster* mon)
{
    // Don't fully identify player shadow equipment, since it makes messaging worse.
    if (mon->type == MONS_PLAYER_SHADOW)
        return;

    for (unsigned int i = 0; i <= MSLOT_LAST_VISIBLE_SLOT; ++i)
    {
        if (mon->inv[i] == NON_ITEM)
            continue;

        item_def &item = env.item[mon->inv[i]];
        identify_item(item);
        item.flags |= ISFLAG_SEEN;
    }
}

// Reduced weight of axes compared to normal orcs, since they are much stronger
// in player ally hands. (And also stronger against your apostles)
static const weapon_list APOSTLE_WARRIOR_WEAPONS =
    {   { WPN_GREAT_SWORD,      3 },
        { WPN_GREAT_MACE,       3 },
        { WPN_DIRE_FLAIL,       3 },
        { WPN_LONG_SWORD,       2 },
        { WPN_HALBERD,          2 },
        { WPN_WAR_AXE,          2 },
        { WPN_BATTLEAXE,        1 },
        { WPN_GLAIVE,           1 },
        { WPN_TRIDENT,          1 },
        { WPN_PARTISAN,         1 }, };

static const weapon_list APOSTLE_WARRIOR_WEAPONS_GOOD =
    {   { WPN_GREAT_SWORD,      3 },
        { WPN_DOUBLE_SWORD,     3 },
        { WPN_TRIPLE_SWORD,     1 },
        { WPN_EVENINGSTAR,      4 },
        { WPN_BATTLEAXE,        6 },
        { WPN_EXECUTIONERS_AXE, 3 },
        { WPN_BROAD_AXE,        3 },
        { WPN_GREAT_MACE,       2 },
        { WPN_BARDICHE,         2 },
        { WPN_GLAIVE,           2 },
        { WPN_PARTISAN,         2 }, };

static const weapon_list APOSTLE_PRIEST_WEAPONS =
    {   { WPN_FLAIL,            60 },
        { WPN_HAND_AXE,         40 },
        { WPN_MACE,             40 },
        { WPN_TRIDENT,          20 },
        { WPN_HALBERD,          10 },
        { WPN_WAR_AXE,          14 },
        { WPN_MORNINGSTAR,      25 } };

static const armour_list APOSTLE_HEAVY_ARMOUR =
    {   { ARM_SCALE_MAIL,            50 },
        { ARM_CHAIN_MAIL,            40 },
        { ARM_PLATE_ARMOUR,          30 }, };

static const armour_list APOSTLE_LIGHT_ARMOUR =
    {   { ARM_LEATHER_ARMOUR,       50 },
        { ARM_RING_MAIL,            40 },
        { ARM_TROLL_LEATHER_ARMOUR, 7 },
        { ARM_STEAM_DRAGON_ARMOUR,  5 },
        { ARM_ACID_DRAGON_ARMOUR,   4 },
        };

static const armour_list APOSTLE_ELITE_HEAVY_ARMOUR =
    {   { ARM_PLATE_ARMOUR,          45 },
        { ARM_STORM_DRAGON_ARMOUR,   25 },
        { ARM_SHADOW_DRAGON_ARMOUR,  20 },
        { ARM_GOLDEN_DRAGON_ARMOUR,  8 },
        { ARM_CRYSTAL_PLATE_ARMOUR,  12 }, };

void give_apostle_equipment(monster* apostle)
{
    ASSERT(apostle->type == MONS_ORC_APOSTLE);

    int pow = apostle->props[APOSTLE_POWER_KEY].get_int();

    switch (static_cast<apostle_type>(apostle->props[APOSTLE_TYPE_KEY].get_int()))
    {
        default:
        case APOSTLE_WARRIOR:
        {
            weapon_list list = x_chance_in_y(max(0, pow - 50), 100)
                                 ? APOSTLE_WARRIOR_WEAPONS_GOOD
                                 : APOSTLE_WARRIOR_WEAPONS;

            const weapon_type *wpn_type = random_choose_weighted(list);
            const bool randart = x_chance_in_y(pow, 350);

            item_def* weapon =
                make_item_for_monster(apostle, OBJ_WEAPONS, *wpn_type,
                                      randart ? ISPEC_RANDART : ISPEC_GIFT, true);

            // Sometimes give a shield if we rolled a one-handed weapon (with it
            // being more likely at higher apostle power)
            if (apostle->hands_reqd(*weapon) == HANDS_ONE
                && x_chance_in_y(pow, 125))
            {
                make_item_for_monster(apostle, OBJ_ARMOUR,
                                      one_chance_in(3) ? ARM_TOWER_SHIELD
                                                       : ARM_KITE_SHIELD,
                                      pow);
            }

            armour_list alist = x_chance_in_y(max(0, pow - 50), 200)
                                 ? APOSTLE_ELITE_HEAVY_ARMOUR
                                 : APOSTLE_HEAVY_ARMOUR;

            const armour_type *arm_type = random_choose_weighted(alist);

            item_def* armour =
                make_item_for_monster(apostle, OBJ_ARMOUR, *arm_type,
                                      100, true);

            if (!is_unrandom_artefact(*armour))
            {
                armour->plus += roll_dice(2, 2);
                if (x_chance_in_y(max(0, pow - 50), 50))
                    armour->plus += roll_dice(2, 2);
            }

            // If we have no spell besides battlecry, have a chance for
            // boomerangs or javelins.
            if ((apostle->spells.size() == 0
                 || apostle->spells[0].spell == SPELL_BATTLECRY)
                && pow >= 25 && one_chance_in(3))
            {
                if (one_chance_in(3))
                    make_item_for_monster(apostle, OBJ_MISSILES, MI_JAVELIN, 0);
                else
                    make_item_for_monster(apostle, OBJ_MISSILES, MI_BOOMERANG, 0);
            }

            // TODO: Very slim chance of specific unrand weapons?
        }
        break;

        case APOSTLE_WIZARD:
        {
            make_item_for_monster(apostle, OBJ_WEAPONS, WPN_DAGGER,
                                  x_chance_in_y(pow, 250) ? ISPEC_RANDART
                                                          : ISPEC_GIFT,
                                  true);

            make_item_for_monster(apostle, OBJ_ARMOUR, ARM_ROBE,
                                  x_chance_in_y(pow, 550) ? ISPEC_RANDART
                                                          : ISPEC_GIFT,
                                  true);

            // Slim chance at higher levels of a random ring or orb of energy
            if (x_chance_in_y(max(0, pow - 40), 280))
            {
                give_specific_item(apostle, items(false, OBJ_ARMOUR,
                                    ARM_ORB, ISPEC_RANDART, SPARM_ENERGY));
            }
            else if (x_chance_in_y(pow, 150))
            {
                // Many ring types do nothing for monsters, so let's pick through
                // ones that do.
                jewellery_type rtype = random_choose(RING_PROTECTION_FROM_FIRE,
                                                     RING_POISON_RESISTANCE,
                                                     RING_PROTECTION_FROM_COLD,
                                                     RING_EVASION,
                                                     RING_POSITIVE_ENERGY,
                                                     RING_PROTECTION);

                make_item_for_monster(apostle, OBJ_JEWELLERY, rtype, 0,
                                      x_chance_in_y(pow, 125)? ISPEC_RANDART : 0);
            }
        }
        break;

        case APOSTLE_PRIEST:
        {
            const weapon_type *wpn_type = random_choose_weighted(APOSTLE_PRIEST_WEAPONS);
            const bool randart = x_chance_in_y(pow, 500);

            make_item_for_monster(apostle, OBJ_WEAPONS, *wpn_type,
                                    randart ? ISPEC_RANDART : ISPEC_GIFT,
                                    true);

            // No shields for priests

            armour_list alist = x_chance_in_y(max(0, pow - 50), 200)
                                 ? APOSTLE_HEAVY_ARMOUR
                                 : APOSTLE_LIGHT_ARMOUR;

            const armour_type *arm_type = random_choose_weighted(alist);

            item_def* armour =
                make_item_for_monster(apostle, OBJ_ARMOUR, *arm_type, 100, true);

            if (!is_unrandom_artefact(*armour))
                armour->plus += roll_dice(2, 2);
        }
        break;
    }
}
