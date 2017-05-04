/**
 * @file
 * @brief Monsters class methods
**/

#include "AppHdr.h"

#include <algorithm>
#include <functional>
#include <queue>

#include "act-iter.h"
#include "areas.h"
#include "artefact.h"
#include "art-enum.h"
#include "attitude-change.h"
#include "bloodspatter.h"
#include "branch.h"
#include "cloud.h"
#include "colour.h"
#include "coordit.h"
#include "database.h"
#include "dgn-event.h"
#include "dgn-overview.h"
#include "directn.h"
#include "english.h"
#include "env.h"
#include "fight.h"
#include "fineff.h"
#include "fprop.h"
#include "ghost.h"
#include "god-abil.h"
#include "god-conduct.h"
#include "god-item.h"
#include "item-name.h"
#include "item-prop.h"
#include "item-status-flag-type.h"
#include "items.h"
#include "libutil.h"
#include "makeitem.h"
#include "message.h"
#include "misc.h"
#include "mon-abil.h"
#include "mon-act.h"
#include "mon-behv.h"
#include "mon-book.h"
#include "mon-cast.h"
#include "mon-clone.h"
#include "mon-death.h"
#include "mon-place.h"
#include "mon-poly.h"
#include "mon-tentacle.h"
#include "mon-transit.h"
#include "religion.h"
#include "rot.h"
#include "spl-monench.h"
#include "spl-summoning.h"
#include "spl-util.h"
#include "state.h"
#include "stringutil.h"
#include "teleport.h"
#include "terrain.h"
#ifdef USE_TILE
#include "tilepick.h"
#include "tileview.h"
#endif
#include "traps.h"
#include "view.h"
#include "xom.h"

monster::monster()
    : hit_points(0), max_hit_points(0),
      speed(0), speed_increment(0), target(), firing_pos(),
      patrol_point(), travel_target(MTRAV_NONE), inv(NON_ITEM), spells(),
      attitude(ATT_HOSTILE), behaviour(BEH_WANDER), foe(MHITYOU),
      enchantments(), flags(), experience(0), base_monster(MONS_NO_MONSTER),
      number(0), colour(COLOUR_INHERIT), foe_memory(0), god(GOD_NO_GOD),
      ghost(), seen_context(SC_NONE), client_id(0), hit_dice(0)

{
    type = MONS_NO_MONSTER;
    travel_path.clear();
    props.clear();
    if (crawl_state.game_is_arena())
        foe = MHITNOT;
    shield_blocks = 0;

    constricting = 0;

    clear_constricted();
    went_unseen_this_turn = false;
    unseen_pos = coord_def(0, 0);
}

// Empty destructor to keep unique_ptr happy with incomplete ghost_demon type.
monster::~monster()
{
}

monster::monster(const monster& mon)
{
    constricting = 0;
    init_with(mon);
}

monster &monster::operator = (const monster& mon)
{
    if (this != &mon)
        init_with(mon);
    return *this;
}

void monster::reset()
{
    mname.clear();
    enchantments.clear();
    ench_cache.reset();
    ench_countdown = 0;
    inv.init(NON_ITEM);
    spells.clear();

    mid             = 0;
    flags           = MF_NO_FLAGS;
    experience      = 0;
    type            = MONS_NO_MONSTER;
    base_monster    = MONS_NO_MONSTER;
    hit_points      = 0;
    max_hit_points  = 0;
    hit_dice        = 0;
    speed_increment = 0;
    attitude        = ATT_HOSTILE;
    behaviour       = BEH_SLEEP;
    foe             = MHITNOT;
    summoner        = 0;
    number          = 0;
    damage_friendly = 0;
    damage_total    = 0;
    shield_blocks   = 0;
    foe_memory      = 0;
    god             = GOD_NO_GOD;
    went_unseen_this_turn = false;
    unseen_pos = coord_def(0, 0);

    mons_remove_from_grid(*this);
    target.reset();
    position.reset();
    firing_pos.reset();
    patrol_point.reset();
    travel_target = MTRAV_NONE;
    travel_path.clear();
    ghost.reset(nullptr);
    seen_context = SC_NONE;
    props.clear();
    clear_constricted();
    // no actual in-game monster should be reset while still constricting
    ASSERT(!constricting);

    client_id = 0;

    // Just for completeness.
    speed           = 0;
    colour         = COLOUR_INHERIT;
}

void monster::init_with(const monster& mon)
{
    reset();

    mid               = mon.mid;
    mname             = mon.mname;
    type              = mon.type;
    base_monster      = mon.base_monster;
    hit_points        = mon.hit_points;
    max_hit_points    = mon.max_hit_points;
    hit_dice          = mon.hit_dice;
    speed             = mon.speed;
    speed_increment   = mon.speed_increment;
    position          = mon.position;
    target            = mon.target;
    firing_pos        = mon.firing_pos;
    patrol_point      = mon.patrol_point;
    travel_target     = mon.travel_target;
    travel_path       = mon.travel_path;
    inv               = mon.inv;
    spells            = mon.spells;
    attitude          = mon.attitude;
    behaviour         = mon.behaviour;
    foe               = mon.foe;
    enchantments      = mon.enchantments;
    ench_cache        = mon.ench_cache;
    flags             = mon.flags;
    experience        = mon.experience;
    number            = mon.number;
    colour            = mon.colour;
    summoner          = mon.summoner;
    foe_memory        = mon.foe_memory;
    god               = mon.god;
    props             = mon.props;
    damage_friendly   = mon.damage_friendly;
    damage_total      = mon.damage_total;

    if (mon.ghost.get())
        ghost.reset(new ghost_demon(*mon.ghost));
    else
        ghost.reset(nullptr);
}

uint32_t monster::last_client_id = 0;

uint32_t monster::get_client_id() const
{
    return client_id;
}

void monster::reset_client_id()
{
    client_id = 0;
}

void monster::ensure_has_client_id()
{
    if (client_id == 0)
        client_id = ++last_client_id;
}

mon_attitude_type monster::temp_attitude() const
{
    if (has_ench(ENCH_HEXED))
    {
        actor *agent = monster_by_mid(get_ench(ENCH_HEXED).source);
        if (agent)
        {
            ASSERT(agent->is_monster());
            return agent->as_monster()->attitude;
        }
        return ATT_HOSTILE; // ???
    }
    if (has_ench(ENCH_CHARM) || has_ench(ENCH_FRIENDLY_BRIBED))
        return ATT_FRIENDLY;
    else if (has_ench(ENCH_NEUTRAL_BRIBED))
        return ATT_GOOD_NEUTRAL; // ???
    else
        return attitude;
}

bool monster::swimming() const
{
    const dungeon_feature_type grid = grd(pos());
    return feat_is_watery(grid) && mons_primary_habitat(*this) == HT_WATER;
}

bool monster::submerged() const
{
    // FIXME, switch to 4.1's MF_SUBMERGED system which is much cleaner.
    // Can't find any reference to MF_SUBMERGED anywhere. Don't know what
    // this means. - abrahamwl
    return has_ench(ENCH_SUBMERGED);
}

bool monster::extra_balanced_at(const coord_def p) const
{
    const dungeon_feature_type grid = grd(p);
    return (mons_genus(type) == MONS_DRACONIAN
            && draco_or_demonspawn_subspecies(*this) == MONS_GREY_DRACONIAN)
                || grid == DNGN_SHALLOW_WATER
                   && (mons_genus(type) == MONS_NAGA // tails, not feet
                       || mons_genus(type) == MONS_SALAMANDER
                       || body_size(PSIZE_BODY) >= SIZE_LARGE);
}

bool monster::extra_balanced() const
{
    return extra_balanced_at(pos());
}

/**
 * Monster floundering conditions.
 *
 * Floundering reduces movement speed and can cause the monster to fumble
 * its attacks. It can be caused by water or by Leda's liquefaction.
 *
 * @param p Coordinates of position to check.
 * @return Whether the monster would be floundering at p.
 */
bool monster::floundering_at(const coord_def p) const
{
    const dungeon_feature_type grid = grd(p);
    return (liquefied(p)
            || (feat_is_water(grid)
                // Can't use monster_habitable_grid() because that'll return
                // true for non-water monsters in shallow water.
                && mons_primary_habitat(*this) != HT_WATER
                // Use real_amphibious to detect giant non-water monsters in
                // deep water, who flounder despite being treated as amphibious.
                && mons_habitat(*this, true) != HT_AMPHIBIOUS
                && !extra_balanced_at(p)))
           && ground_level();
}

bool monster::floundering() const
{
    return floundering_at(pos());
}

bool monster::can_pass_through_feat(dungeon_feature_type grid) const
{
    return mons_class_can_pass(mons_base_type(*this), grid);
}

bool monster::is_habitable_feat(dungeon_feature_type actual_grid) const
{
    return monster_habitable_grid(this, actual_grid);
}

bool monster::can_drown() const
{
    // Presumably a electric eel in lava or a lavafish in deep water could
    // drown, but that should never happen, so this simple check should
    // be enough.
    switch (mons_primary_habitat(*this))
    {
    case HT_WATER:
    case HT_LAVA:
    case HT_AMPHIBIOUS:
        return false;
    default:
        break;
    }

    return !is_unbreathing();
}

size_type monster::body_size(size_part_type /* psize */, bool /* base */) const
{
    monster_info mi(this, MILEV_NAME);
    return mi.body_size();
}

brand_type monster::damage_brand(int which_attack)
{
    const item_def *mweap = weapon(which_attack);

    if (!mweap)
    {
        if (mons_is_ghost_demon(type))
            return ghost->brand;

        return SPWPN_NORMAL;
    }

    return !is_range_weapon(*mweap) ? static_cast<brand_type>(get_weapon_brand(*mweap))
                                    : SPWPN_NORMAL;
}

int monster::damage_type(int which_attack)
{
    const item_def *mweap = weapon(which_attack);

    if (!mweap)
    {
        const mon_attack_def atk = mons_attack_spec(*this, which_attack);
        return (atk.type == AT_CLAW)          ? DVORP_CLAWING :
               (atk.type == AT_TENTACLE_SLAP) ? DVORP_TENTACLE
                                              : DVORP_CRUSHING;
    }

    return get_vorpal_type(*mweap);
}

/**
 * Return the delay caused by attacking with weapon and projectile.
 *
 * @param projectile    The projectile to be fired/thrown, if any.
 * @return              The time taken by an attack with the monster's weapon
 *                      and the given projectile, in aut.
 */
random_var monster::attack_delay(const item_def *projectile,
                                 bool /*rescale*/) const
{
    const item_def* weap = weapon();

    const bool use_unarmed =
        (projectile) ? is_launched(this, weap, *projectile) != LRET_LAUNCHED
                     : !weap;

    if (use_unarmed || !weap)
        return random_var(10);

    random_var delay(property(*weap, PWPN_SPEED));
    if (get_weapon_brand(*weap) == SPWPN_SPEED)
        delay = div_rand_round(delay * 2, 3);
    return (random_var(10) + delay) / 2;
}

int monster::has_claws(bool allow_tran) const
{
    for (int i = 0; i < MAX_NUM_ATTACKS; i++)
    {
        const mon_attack_def atk = mons_attack_spec(*this, i);
        if (atk.type == AT_CLAW)
        {
            // Some better criteria would be better.
            if (body_size() < SIZE_LARGE || atk.damage < 15)
                return 1;
            return 3;
        }
    }

    return 0;
}

item_def *monster::missiles() const
{
    return inv[MSLOT_MISSILE] != NON_ITEM ? &mitm[inv[MSLOT_MISSILE]] : nullptr;
}

item_def *monster::launcher() const
{
    item_def *weap = mslot_item(MSLOT_WEAPON);
    if (weap && is_range_weapon(*weap))
        return weap;

    weap = mslot_item(MSLOT_ALT_WEAPON);
    return weap && is_range_weapon(*weap) ? weap : nullptr;
}

// Does not check whether the monster can dual-wield - that is the
// caller's responsibility.
static int _mons_offhand_weapon_index(const monster* m)
{
    return m->inv[MSLOT_ALT_WEAPON];
}

item_def *monster::weapon(int which_attack) const
{
    const mon_attack_def attk = mons_attack_spec(*this, which_attack);
    if (attk.type != AT_HIT && attk.type != AT_WEAP_ONLY)
        return nullptr;

    // Even/odd attacks use main/offhand weapon.
    if (which_attack > 1)
        which_attack &= 1;

    // This randomly picks one of the wielded weapons for monsters that can use
    // two weapons. Not ideal, but better than nothing. fight.cc does it right,
    // for various values of right.
    int weap = inv[MSLOT_WEAPON];

    if (which_attack && mons_wields_two_weapons(*this))
    {
        const int offhand = _mons_offhand_weapon_index(this);
        if (offhand != NON_ITEM
            && (weap == NON_ITEM || which_attack == 1 || coinflip()))
        {
            weap = offhand;
        }
    }

    return weap == NON_ITEM ? nullptr : &mitm[weap];
}

/**
 * Find a monster's melee weapon, if any.
 *
 * Finds melee weapons carried in the primary or aux slot; if the monster has
 * both (dual-wielding), choose one with a coinflip.
 *
 * @return A melee weapon that the monster is holding, or null.
 */
item_def *monster::melee_weapon() const
{
    item_def* first_weapon = mslot_item(MSLOT_WEAPON);
    item_def* second_weapon = mslot_item(MSLOT_ALT_WEAPON);
    const bool primary_is_melee = first_weapon
                                  && is_melee_weapon(*first_weapon);
    const bool secondary_is_melee = second_weapon
                                    && is_melee_weapon(*second_weapon);
    if (primary_is_melee && secondary_is_melee)
        return random_choose(first_weapon, second_weapon);
    if (primary_is_melee)
        return first_weapon;
    if (secondary_is_melee)
        return second_weapon;
    return nullptr;
}

// Give hands required to wield weapon.
hands_reqd_type monster::hands_reqd(const item_def &item, bool base) const
{
    if (mons_genus(type) == MONS_FORMICID)
        return HANDS_ONE;
    return actor::hands_reqd(item, base);
}

bool monster::can_wield(const item_def& item, bool ignore_curse,
                         bool ignore_brand, bool ignore_shield,
                         bool ignore_transform) const
{
    // Monsters can only wield weapons or go unarmed (OBJ_UNASSIGNED
    // means unarmed).
    if (item.base_type != OBJ_WEAPONS && item.base_type != OBJ_UNASSIGNED)
        return false;

    // These *are* weapons, so they can't wield another weapon or
    // unwield themselves.
    if (mons_class_is_animated_weapon(type))
        return false;

    // MF_HARD_RESET means that all items the monster is carrying will
    // disappear when it does, so it can't accept new items or give up
    // the ones it has.
    if (flags & MF_HARD_RESET)
        return false;

    // Summoned items can only be held by summoned monsters.
    if ((item.flags & ISFLAG_SUMMONED) && !is_summoned())
        return false;

    item_def* weap1 = nullptr;
    if (inv[MSLOT_WEAPON] != NON_ITEM)
        weap1 = &mitm[inv[MSLOT_WEAPON]];

    int       avail_slots = 1;
    item_def* weap2       = nullptr;
    if (mons_wields_two_weapons(*this))
    {
        if (!weap1 || hands_reqd(*weap1) != HANDS_TWO)
            avail_slots = 2;

        const int offhand = _mons_offhand_weapon_index(this);
        if (offhand != NON_ITEM)
            weap2 = &mitm[offhand];
    }

    // If we're already wielding it, then of course we can wield it.
    if (&item == weap1 || &item == weap2)
        return true;

    // Barehanded needs two hands.
    const bool two_handed = item.base_type == OBJ_UNASSIGNED
                            || hands_reqd(item) == HANDS_TWO;

    item_def* _shield = nullptr;
    if (inv[MSLOT_SHIELD] != NON_ITEM)
    {
        ASSERT(!(weap1 && weap2));

        if (two_handed && !ignore_shield)
            return false;

        _shield = &mitm[inv[MSLOT_SHIELD]];
    }

    if (!ignore_curse)
    {
        int num_cursed = 0;
        if (weap1 && weap1->cursed())
            num_cursed++;
        if (weap2 && weap2->cursed())
            num_cursed++;
        if (_shield && _shield->cursed())
            num_cursed++;

        if (two_handed && num_cursed > 0 || num_cursed >= avail_slots)
            return false;
    }

    return could_wield(item, ignore_brand, ignore_transform);
}

/**
 * Checks whether the monster could ever wield the given item, regardless of
 * what they're currently wielding or any other state.
 *
 * @param item              The item to wield.
 * @param ignore_brand      Whether to disregard the item's brand.
 * @return                  Whether the monster could potentially wield the
 *                          item.
 */
bool monster::could_wield(const item_def &item, bool ignore_brand,
                           bool /* ignore_transform */, bool /* quiet */) const
{
    ASSERT(item.defined());

    // These *are* weapons, so they can't wield another weapon.
    if (mons_class_is_animated_weapon(type))
        return false;

    // Monsters can't use unrandarts with special effects.
    if (is_special_unrandom_artefact(item) && !crawl_state.game_is_arena())
        return false;

    // Wimpy monsters (e.g. kobolds, goblins) can't use halberds, etc.
    if (is_weapon(item) && !is_weapon_wieldable(item, body_size()))
        return false;

    if (!ignore_brand)
    {
        // Undead and demonic monsters and monsters that are
        // gifts/worshippers of Yredelemnul won't use holy weapons.
        if ((undead_or_demonic() || god == GOD_YREDELEMNUL)
            && is_holy_item(item))
        {
            return false;
        }

        // Holy monsters that aren't gifts/worshippers of chaotic gods
        // and monsters that are gifts/worshippers of good gods won't
        // use potentially evil weapons.
        if (((is_holy() && !is_chaotic_god(god))
                || is_good_god(god))
            && is_potentially_evil_item(item))
        {
            return false;
        }

        // Holy monsters and monsters that are gifts/worshippers of good
        // gods won't use evil weapons.
        if ((is_holy() || is_good_god(god)) && is_evil_item(item))
            return false;

        // Monsters that are gifts/worshippers of Fedhas won't use
        // corpse-violating weapons.
        if (god == GOD_FEDHAS && is_corpse_violating_item(item))
            return false;

        // Monsters that are gifts/worshippers of Zin won't use unclean
        // weapons.
        if (god == GOD_ZIN && is_unclean_item(item))
            return false;

        // Holy monsters that aren't gifts/worshippers of chaotic gods
        // and monsters that are gifts/worshippers of good gods won't
        // use chaotic weapons.
        if (((is_holy() && !is_chaotic_god(god)) || is_good_god(god))
            && is_chaotic_item(item))
        {
            return false;
        }
    }

    return true;
}

bool monster::can_throw_large_rocks() const
{
    monster_type species = mons_species(false); // zombies can't
    return species == MONS_STONE_GIANT
           || species == MONS_CYCLOPS
           || species == MONS_OGRE;
}

bool monster::can_speak()
{
    if (has_ench(ENCH_MUTE))
        return false;

    // Priest and wizard monsters can always speak.
    if (is_priest() || is_actual_spellcaster())
        return true;

    // Silent or non-sentient monsters can't use the original speech.
    if (mons_intel(*this) < I_HUMAN || !mons_can_shout(type))
        return false;

    // Does it have the proper vocal equipment?
    return mon_shape_is_humanoid(get_mon_shape(*this));
}

bool monster::is_silenced() const
{
    return silenced(pos())
            || has_ench(ENCH_MUTE)
            || has_ench(ENCH_WATER_HOLD)
               && !res_water_drowning();
}

bool monster::search_slots(function<bool (const mon_spell_slot &)> func) const
{
    return any_of(begin(spells), end(spells), func);
}

bool monster::search_spells(function<bool (spell_type)> func) const
{
    return search_slots([&] (const mon_spell_slot &s)
                        { return func(s.spell); });
}

bool monster::has_spell_of_type(spschool_flag_type discipline) const
{
    return search_spells(bind(spell_typematch, placeholders::_1, discipline));
}

void monster::bind_melee_flags()
{
    // Bind fighter / dual-wielder / archer flags from the base type.

    // Alas, we don't know if the mon is zombified at the moment, if it
    // is, the flags (other than dual-wielder) will be removed later.
    if (mons_class_flag(type, M_FIGHTER))
        flags |= MF_FIGHTER;
    if (mons_class_flag(type, M_TWO_WEAPONS))
        flags |= MF_TWO_WEAPONS;
    if (mons_class_flag(type, M_ARCHER))
        flags |= MF_ARCHER;
}

void monster::bind_spell_flags()
{
    if (!mons_is_ghost_demon(type) && mons_has_ranged_spell(*this))
        flags |= MF_SEEN_RANGED;
}

static bool _needs_ranged_attack(const monster* mon)
{
    // Prevent monsters that have conjurations from grabbing missiles.
    if (mon->has_spell_of_type(SPTYP_CONJURATION))
        return false;

    // Same for summonings, but make an exception for friendlies.
    if (!mon->friendly() && mon->has_spell_of_type(SPTYP_SUMMONING))
        return false;

    // Blademasters don't want to throw stuff.
    if (mon->type == MONS_DEEP_ELF_BLADEMASTER)
        return false;

    return true;
}

bool monster::can_use_missile(const item_def &item) const
{
    // Don't allow monsters to pick up missiles without the corresponding
    // launcher. The opposite is okay, and sufficient wandering will
    // hopefully take the monster to a stack of appropriate missiles.

    if (!_needs_ranged_attack(this))
        return false;

    if (item.base_type == OBJ_WEAPONS
        || item.base_type == OBJ_MISSILES && !has_launcher(item))
    {
        return is_throwable(this, item);
    }

    if (item.base_type != OBJ_MISSILES)
        return false;

    // Stones are allowed even without launcher.
    if (item.sub_type == MI_STONE)
        return true;

    item_def *launch;
    for (int i = MSLOT_WEAPON; i <= MSLOT_ALT_WEAPON; ++i)
    {
        launch = mslot_item(static_cast<mon_inv_type>(i));
        if (launch && item.launched_by(*launch))
            return true;
    }

    // No fitting launcher in inventory.
    return false;
}

/**
 * Does this monster have any interest in using the given wand? (Will they
 * pick it up?)
 *
 * Based purely on monster HD & wand type for now. Higher-HD monsters are less
 * inclined to bother with wands, especially the weaker ones.
 *
 * @param item      The wand in question.
 * @return          Whether the monster will bother picking up the wand.
 */
bool monster::likes_wand(const item_def &item) const
{
    ASSERT(item.base_type == OBJ_WANDS);
    // kind of a hack
    // assumptions:
    // bad wands are value 16, so won't be used past hd 4
    // mediocre wands are value 8; won't be used past hd 8
    // other good wands are value 5, won't be used past hd 10
    // better implementations welcome
    return wand_charge_value(item.sub_type) + get_hit_dice() * 2 <= 24;
}

void monster::equip_weapon(item_def &item, bool msg)
{
    if (msg)
    {
        const string str = " wields " +
                           item.name(DESC_A, false, false, true, false,
                                     ISFLAG_CURSED) + ".";
        msg = simple_monster_message(*this, str.c_str());
    }

    const int brand = get_weapon_brand(item);
    if (msg)
    {
        bool message_given = true;
        switch (brand)
        {
        case SPWPN_FLAMING:
            mpr("It bursts into flame!");
            break;
        case SPWPN_FREEZING:
            mpr(is_range_weapon(item) ? "It is covered in frost."
                                      : "It glows with a cold blue light!");
            break;
        case SPWPN_HOLY_WRATH:
            mpr("It softly glows with a divine radiance!");
            break;
        case SPWPN_ELECTROCUTION:
            mprf(MSGCH_SOUND, "You hear the crackle of electricity.");
            break;
        case SPWPN_VENOM:
            mpr("It begins to drip with poison!");
            break;
        case SPWPN_DRAINING:
            mpr("You sense an unholy aura.");
            break;
        case SPWPN_DISTORTION:
            mpr("Its appearance distorts for a moment.");
            break;
        case SPWPN_CHAOS:
            mpr("It is briefly surrounded by a scintillating aura of "
                "random colours.");
            break;
        case SPWPN_PENETRATION:
        {
            bool plural = true;
            string hand = hand_name(true, &plural);
            mprf("%s %s briefly %s through it before %s manages to get a "
                 "firm grip on it.",
                 pronoun(PRONOUN_POSSESSIVE).c_str(),
                 hand.c_str(),
                 // Not conj_verb: the monster isn't the subject.
                 conjugate_verb("pass", plural).c_str(),
                 pronoun(PRONOUN_SUBJECTIVE).c_str());
        }
            break;
        case SPWPN_REAPING:
            mpr("It is briefly surrounded by shifting shadows.");
            break;
        case SPWPN_ACID:
            mprf("It begins to drip corrosive slime!");
            break;

        default:
            // A ranged weapon without special message is known to be unbranded.
            if (brand != SPWPN_NORMAL || !is_range_weapon(item))
                message_given = false;
        }

        if (message_given)
        {
            if (is_artefact(item) && !is_unrandom_artefact(item))
                artefact_learn_prop(item, ARTP_BRAND);
            else
                set_ident_flags(item, ISFLAG_KNOW_TYPE);
        }
    }
}

/**
 * What AC bonus does the monster get from the given item?
 *
 * @param calc_unid     Whether to include properties unknown (to the player)
 *                      in the calculated result.
 * @return              The AC provided by wearing the given item.
 */
int monster::armour_bonus(const item_def &item, bool calc_unid) const
{
    ASSERT(!is_shield(item));

    int armour_ac = property(item, PARM_AC);
    // For consistency with players, we should multiply this by 1 + (skill/22),
    // where skill may be HD.

    if (!calc_unid && !item_ident(item, ISFLAG_KNOW_PLUSES))
        return armour_ac;

    const int armour_plus = item.plus;
    ASSERT(abs(armour_plus) < 30); // sanity check
    return armour_ac + armour_plus;
}

void monster::equip_armour(item_def &item, bool msg)
{
    if (msg)
    {
        const string str = " wears " +
                           item.name(DESC_A) + ".";
        simple_monster_message(*this, str.c_str());
    }
}

void monster::equip_jewellery(item_def &item, bool msg)
{
    ASSERT(item.base_type == OBJ_JEWELLERY);

    if (msg)
    {
        const string str = " puts on " +
                           item.name(DESC_A) + ".";
        simple_monster_message(*this, str.c_str());
    }
}

void monster::equip(item_def &item, bool msg)
{
    switch (item.base_type)
    {
    case OBJ_WEAPONS:
    case OBJ_STAVES:
        equip_weapon(item, msg);
        break;

    case OBJ_ARMOUR:
        equip_armour(item, msg);
        break;

    case OBJ_JEWELLERY:
        equip_jewellery(item, msg);
    break;

    default:
        break;
    }
}

void monster::unequip_weapon(item_def &item, bool msg)
{
    if (msg)
    {
        const string str = " unwields " +
                           item.name(DESC_A, false, false, true, false,
                                     ISFLAG_CURSED) + ".";
        msg = simple_monster_message(*this, str.c_str());
    }

    const int brand = get_weapon_brand(item);
    if (msg && brand != SPWPN_NORMAL)
    {
        bool message_given = true;
        switch (brand)
        {
        case SPWPN_FLAMING:
            mpr("It stops flaming.");
            break;

        case SPWPN_HOLY_WRATH:
            mpr("It stops glowing.");
            break;

        case SPWPN_ELECTROCUTION:
            mpr("It stops crackling.");
            break;

        case SPWPN_VENOM:
            mpr("It stops dripping with poison.");
            break;

        case SPWPN_DISTORTION:
            mpr("Its appearance distorts for a moment.");
            break;

        default:
            message_given = false;
        }
        if (message_given)
        {
            if (is_artefact(item) && !is_unrandom_artefact(item))
                artefact_learn_prop(item, ARTP_BRAND);
            else
                set_ident_flags(item, ISFLAG_KNOW_TYPE);
        }
    }

    monster *spectral_weapon = find_spectral_weapon(this);
    if (spectral_weapon)
        end_spectral_weapon(spectral_weapon, false);
}

void monster::unequip_armour(item_def &item, bool msg)
{
    if (msg)
    {
        const string str = " takes off " +
                           item.name(DESC_A) + ".";
        simple_monster_message(*this, str.c_str());
    }
}

void monster::unequip_jewellery(item_def &item, bool msg)
{
    ASSERT(item.base_type == OBJ_JEWELLERY);

    if (msg)
    {
        const string str = " takes off " +
                           item.name(DESC_A) + ".";
        simple_monster_message(*this, str.c_str());
    }
}

/**
 * Applies appropriate effects when unequipping an item.
 *
 * Note: this method does NOT modify this->inv to point to NON_ITEM!
 * @param item  the item to be removed.
 * @param msg   whether to give a message
 * @param force whether to remove the item even if cursed.
 * @return whether the item was unequipped successfully.
 */
bool monster::unequip(item_def &item, bool msg, bool force)
{
    if (!force && item.cursed())
        return false;

    if (!force && you.can_see(*this))
        set_ident_flags(item, ISFLAG_KNOW_CURSE);

    switch (item.base_type)
    {
    case OBJ_WEAPONS:
        unequip_weapon(item, msg);
        break;

    case OBJ_ARMOUR:
        unequip_armour(item, msg);
        break;

    case OBJ_JEWELLERY:
        unequip_jewellery(item, msg);
    break;

    default:
        break;
    }

    return true;
}

void monster::lose_pickup_energy()
{
    if (const monsterentry* entry = find_monsterentry())
    {
        const int delta = speed * entry->energy_usage.pickup_percent / 100;
        if (speed_increment > 25 && delta < speed_increment)
            speed_increment -= delta;
    }
}

void monster::pickup_message(const item_def &item)
{
    if (is_range_weapon(item)
        || is_throwable(this, item)
        || item.base_type == OBJ_MISSILES)
    {
        flags |= MF_SEEN_RANGED;
    }

    mprf("%s picks up %s.",
         name(DESC_THE).c_str(),
         item.base_type == OBJ_GOLD ? "some gold"
                                    : item.name(DESC_A).c_str());
}

bool monster::pickup(item_def &item, mon_inv_type slot, bool msg)
{
    ASSERT(item.defined());

    const monster* other_mon = item.holding_monster();

    if (other_mon != nullptr)
    {
        if (other_mon == this)
        {
            if (inv[slot] == item.index())
            {
                mprf(MSGCH_DIAGNOSTICS, "Monster %s already holding item %s.",
                     name(DESC_PLAIN, true).c_str(),
                     item.name(DESC_PLAIN, false, true).c_str());
                return false;
            }
            else
            {
                mprf(MSGCH_DIAGNOSTICS, "Item %s thinks it's already held by "
                                        "monster %s.",
                     item.name(DESC_PLAIN, false, true).c_str(),
                     name(DESC_PLAIN, true).c_str());
            }
        }
        else if (other_mon->type == MONS_NO_MONSTER)
        {
            mprf(MSGCH_DIAGNOSTICS, "Item %s, held by dead monster, being "
                                    "picked up by monster %s.",
                 item.name(DESC_PLAIN, false, true).c_str(),
                 name(DESC_PLAIN, true).c_str());
        }
        else
        {
            mprf(MSGCH_DIAGNOSTICS, "Item %s, held by monster %s, being "
                                    "picked up by monster %s.",
                 item.name(DESC_PLAIN, false, true).c_str(),
                 other_mon->name(DESC_PLAIN, true).c_str(),
                 name(DESC_PLAIN, true).c_str());
        }
    }

    // If a monster chooses a two-handed weapon as main weapon, it will
    // first have to drop any shield it might wear.
    // (Monsters will always favour damage over protection.)
    if ((slot == MSLOT_WEAPON || slot == MSLOT_ALT_WEAPON)
        && inv[MSLOT_SHIELD] != NON_ITEM
        && hands_reqd(item) == HANDS_TWO)
    {
        if (!drop_item(MSLOT_SHIELD, msg))
            return false;
    }

    // Similarly, monsters won't pick up shields if they're
    // wielding (or alt-wielding) a two-handed weapon.
    if (slot == MSLOT_SHIELD)
    {
        const item_def* wpn = mslot_item(MSLOT_WEAPON);
        const item_def* alt = mslot_item(MSLOT_ALT_WEAPON);
        if (wpn && hands_reqd(*wpn) == HANDS_TWO)
            return false;
        if (alt && hands_reqd(*alt) == HANDS_TWO)
            return false;
    }

    if (inv[slot] != NON_ITEM)
    {
        item_def &dest(mitm[inv[slot]]);
        if (items_stack(item, dest))
        {
            dungeon_events.fire_position_event(
                dgn_event(DET_ITEM_PICKUP, pos(), 0, item.index(),
                          mindex()),
                pos());

            if (msg)
                pickup_message(item);
            merge_item_stacks(item, dest);
            inc_mitm_item_quantity(inv[slot], item.quantity);
            destroy_item(item.index());
            equip(item, msg);
            lose_pickup_energy();
            return true;
        }
        return false;
    }

    // don't try to pick up mimics
    if (item.flags & ISFLAG_MIMIC)
        return false;

    dungeon_events.fire_position_event(
        dgn_event(DET_ITEM_PICKUP, pos(), 0, item.index(),
                  mindex()),
        pos());

    const int item_index = item.index();
    unlink_item(item_index);

    inv[slot] = item_index;

    item.set_holding_monster(*this);

    if (msg)
        pickup_message(item);
    equip(item, msg);
    lose_pickup_energy();
    return true;
}

bool monster::drop_item(mon_inv_type eslot, bool msg)
{
    int item_index = inv[eslot];
    if (item_index == NON_ITEM)
        return true;

    item_def& pitem = mitm[item_index];

    // Unequip equipped items before dropping them; unequip() prevents
    // cursed items from being removed.
    bool was_unequipped = false;
    if (eslot == MSLOT_WEAPON
        || eslot == MSLOT_ARMOUR
        || eslot == MSLOT_JEWELLERY
        || eslot == MSLOT_ALT_WEAPON && mons_wields_two_weapons(*this))
    {
        if (!unequip(pitem, msg))
            return false;
        was_unequipped = true;
    }
    const bool was_wand = pitem.base_type == OBJ_WANDS;

    if (pitem.flags & ISFLAG_SUMMONED)
    {
        if (msg)
        {
            mprf("%s %s as %s drops %s!",
                 pitem.name(DESC_THE).c_str(),
                 summoned_poof_msg(this, pitem).c_str(),
                 name(DESC_THE).c_str(),
                 pitem.quantity > 1 ? "them" : "it");
        }

        item_was_destroyed(pitem);
        destroy_item(item_index);
    }
    else
    {
        if (msg)
        {
            mprf("%s drops %s.", name(DESC_THE).c_str(),
                 pitem.name(DESC_A).c_str());
        }
        pitem.props[DROPPER_MID_KEY].get_int() = mid;

        if (!move_item_to_grid(&item_index, pos(), swimming()))
        {
            // Re-equip item if we somehow failed to drop it.
            if (was_unequipped)
                equip(pitem, msg);

            return false;
        }
    }

    if (props.exists("wand_known") && msg && was_wand)
        props.erase("wand_known");

    inv[eslot] = NON_ITEM;
    return true;
}

bool monster::pickup_launcher(item_def &launch, bool msg, bool force)
{
    // Don't allow monsters to pick up launchers that would also
    // refuse to pick up the matching ammo.
    if (!force && !_needs_ranged_attack(this))
        return false;

    // Don't allow monsters to switch to another type of launcher
    // as that would require them to also drop their ammunition
    // and then try to find ammunition for their new launcher.
    // However, they may switch to another launcher if they're
    // out of ammo. (jpeg)
    const int mdam_rating = mons_weapon_damage_rating(launch);
    const missile_type mt = fires_ammo_type(launch);
    mon_inv_type eslot = NUM_MONSTER_SLOTS;
    for (int i = MSLOT_WEAPON; i <= MSLOT_ALT_WEAPON; ++i)
    {
        auto slot = static_cast<mon_inv_type>(i);
        if (const item_def *elaunch = mslot_item(slot))
        {
            if (!is_range_weapon(*elaunch))
                continue;

            return (fires_ammo_type(*elaunch) == mt || !missiles())
                   && (mons_weapon_damage_rating(*elaunch) < mdam_rating
                       || mons_weapon_damage_rating(*elaunch) == mdam_rating
                          && get_weapon_brand(*elaunch) == SPWPN_NORMAL
                          && get_weapon_brand(launch) != SPWPN_NORMAL)
                   && drop_item(slot, msg) && pickup(launch, slot, msg);
        }
        else
            eslot = slot;
    }

    return eslot == NUM_MONSTER_SLOTS ? false : pickup(launch, eslot, msg);
}

static bool _is_signature_weapon(const monster* mons, const item_def &weapon)
{
    // Don't pick up items that would interfere with our special ability
    if (mons->type == MONS_RED_DEVIL)
        return item_attack_skill(weapon) == SK_POLEARMS;

    // Some other uniques have a signature weapon, usually because they
    // always spawn with it, or because it is referenced in their speech
    // and/or descriptions.
    // Upgrading to a similar type is pretty much always allowed, unless
    // we are more interested in the brand, and the brand is *rare*.
    if (mons_is_unique(mons->type))
    {
        weapon_type wtype = (weapon.base_type == OBJ_WEAPONS) ?
            (weapon_type)weapon.sub_type : NUM_WEAPONS;

        // We might allow Sigmund to pick up a better scythe if he finds
        // one...
        if (mons->type == MONS_SIGMUND)
            return wtype == WPN_SCYTHE;

        // Crazy Yiuf's got MONUSE_STARTING_EQUIPMENT right now, but
        // in case that ever changes we don't want him to switch away
        // from his quarterstaff of chaos.
        if (mons->type == MONS_CRAZY_YIUF)
        {
            return wtype == WPN_QUARTERSTAFF
                   && get_weapon_brand(weapon) == SPWPN_CHAOS;
        }

        // Distortion/chaos is immensely flavourful, and we shouldn't
        // allow Psyche to switch away from it.
        if (mons->type == MONS_PSYCHE)
        {
            return get_weapon_brand(weapon) == SPWPN_CHAOS
                   || get_weapon_brand(weapon) == SPWPN_DISTORTION;
        }

        // Don't switch Azrael away from the customary scimitar of
        // flaming.
        if (mons->type == MONS_AZRAEL)
        {
            return wtype == WPN_SCIMITAR
                   && get_weapon_brand(weapon) == SPWPN_FLAMING;
        }

        if (mons->type == MONS_AGNES)
            return wtype == WPN_LAJATANG;

        if (mons->type == MONS_EDMUND)
            return wtype == WPN_FLAIL || wtype == WPN_DIRE_FLAIL;

        // Pikel's got MONUSE_STARTING_EQUIPMENT right now, but,
        // in case that ever changes, we don't want him to switch away
        // from a whip.
        if (mons->type == MONS_PIKEL)
            return get_vorpal_type(weapon) == DVORP_SLASHING;

        if (mons->type == MONS_NIKOLA)
            return get_weapon_brand(weapon) == SPWPN_ELECTROCUTION;

        if (mons->type == MONS_DUVESSA)
        {
            return item_attack_skill(weapon) == SK_SHORT_BLADES
                   || item_attack_skill(weapon) == SK_LONG_BLADES;
        }

        if (mons->type == MONS_IGNACIO)
            return wtype == WPN_EXECUTIONERS_AXE;

        if (mons->type == MONS_MENNAS)
            return get_weapon_brand(weapon) == SPWPN_HOLY_WRATH;

        if (mons->type == MONS_ARACHNE)
        {
            return weapon.is_type(OBJ_STAVES, STAFF_POISON)
                   || is_unrandom_artefact(weapon, UNRAND_OLGREB);
        }

        if (mons->type == MONS_FANNAR)
            return weapon.is_type(OBJ_STAVES, STAFF_COLD);

        // Asterion's demon weapon was a gift from Makhleb.
        if (mons->type == MONS_ASTERION)
        {
            return wtype == WPN_DEMON_BLADE || wtype == WPN_DEMON_WHIP
                || wtype == WPN_DEMON_TRIDENT;
        }

        // Donald kept dropping his shield. I hate that.
        if (mons->type == MONS_DONALD)
            return mons->hands_reqd(weapon) == HANDS_ONE;

        // What kind of assassin would forget her blowgun or dagger somewhere else?
        if (mons->type == MONS_SONJA)
        {
            return item_attack_skill(weapon) == SK_SHORT_BLADES
                   || wtype == WPN_BLOWGUN;
        }

        if (mons->type == MONS_IMPERIAL_MYRMIDON)
            return item_attack_skill(weapon) == SK_LONG_BLADES;
    }

    if (mons->is_holy())
        return is_blessed(weapon) || get_weapon_brand(weapon) == SPWPN_HOLY_WRATH;

    if (is_unrandom_artefact(weapon))
    {
        switch (weapon.unrand_idx)
        {
        case UNRAND_ASMODEUS:
            return mons->type == MONS_ASMODEUS;

        case UNRAND_DISPATER:
            return mons->type == MONS_DISPATER;

        case UNRAND_CEREBOV:
            return mons->type == MONS_CEREBOV;

        case UNRAND_MORG:
            return mons->type == MONS_BORIS;
        }
    }

    return false;
}

static int _ego_damage_bonus(item_def &item)
{
    switch (get_weapon_brand(item))
    {
    case SPWPN_NORMAL:      return 0;
    case SPWPN_VORPAL:      // deliberate fall through
    case SPWPN_PROTECTION:  return 1;
    default:                return 2;
    }
}

bool monster::pickup_melee_weapon(item_def &item, bool msg)
{
    // Draconian monks are masters of unarmed combat.
    // Monstrous demonspawn prefer to RIP AND TEAR with their claws.
    // XXX: this could probably be a monster flag
    if (type == MONS_DRACONIAN_MONK
        || mons_is_demonspawn(type)
           && draco_or_demonspawn_subspecies(*this) == MONS_MONSTROUS_DEMONSPAWN)
    {
        return false;
    }

    const bool dual_wielding = mons_wields_two_weapons(*this);
    if (dual_wielding)
    {
        // If we have either weapon slot free, pick up the weapon.
        if (inv[MSLOT_WEAPON] == NON_ITEM)
            return pickup(item, MSLOT_WEAPON, msg);

        if (inv[MSLOT_ALT_WEAPON] == NON_ITEM)
            return pickup(item, MSLOT_ALT_WEAPON, msg);
    }

    const int new_wpn_dam = mons_weapon_damage_rating(item)
                            + _ego_damage_bonus(item);
    mon_inv_type eslot = NUM_MONSTER_SLOTS;
    item_def *weap;

    // Monsters have two weapon slots, one of which can be a ranged, and
    // the other a melee weapon. (The exception being dual-wielders who can
    // wield two melee weapons). The weapon in MSLOT_WEAPON is the one
    // currently wielded (can be empty).

    for (int i = MSLOT_WEAPON; i <= MSLOT_ALT_WEAPON; ++i)
    {
        auto slot = static_cast<mon_inv_type>(i);
        weap = mslot_item(slot);

        if (!weap)
        {
            // If no weapon in this slot, mark this one.
            if (eslot == NUM_MONSTER_SLOTS)
                eslot = slot;
        }
        else
        {
            if (is_range_weapon(*weap))
                continue;

            // Don't swap from a signature weapon to a non-signature one.
            if (!_is_signature_weapon(this, item)
                && _is_signature_weapon(this, *weap))
            {
                if (dual_wielding)
                    continue;
                else
                    return false;
            }

            // If we get here, the weapon is a melee weapon.
            // If the new weapon is better than the current one and not cursed,
            // replace it. Otherwise, give up.
            const int old_wpn_dam = mons_weapon_damage_rating(*weap)
                                    + _ego_damage_bonus(*weap);

            bool new_wpn_better = new_wpn_dam > old_wpn_dam;
            if (new_wpn_dam == old_wpn_dam)
            {
                // Use shopping value as a crude estimate of resistances etc.
                // XXX: This is not really logical as many properties don't
                //      apply to monsters (e.g. flight, blink, berserk).
                // For simplicity, don't apply this check to secondary weapons
                // for dual wielding monsters.
                int oldval = item_value(*weap, true);
                int newval = item_value(item, true);

                if (newval > oldval)
                    new_wpn_better = true;
            }

            if (new_wpn_better && !weap->cursed())
            {
                if (!dual_wielding
                    || slot == MSLOT_WEAPON
                    || old_wpn_dam
                       < mons_weapon_damage_rating(*mslot_item(MSLOT_WEAPON))
                         + _ego_damage_bonus(*mslot_item(MSLOT_WEAPON)))
                {
                    eslot = slot;
                    if (!dual_wielding)
                        break;
                }
            }
            else if (!dual_wielding)
            {
                // Only dual wielders want two melee weapons.
                return false;
            }
        }
    }

    // No slot found to place this item.
    if (eslot == NUM_MONSTER_SLOTS)
        return false;

    // Current item cannot be dropped.
    if (inv[eslot] != NON_ITEM && !drop_item(eslot, msg))
        return false;

    return pickup(item, eslot, msg);
}

bool monster::wants_weapon(const item_def &weap) const
{
    if (!could_wield(weap))
        return false;

    // Blademasters and master archers like their starting weapon and
    // don't want another, thank you.
    if (type == MONS_DEEP_ELF_BLADEMASTER
        || type == MONS_DEEP_ELF_MASTER_ARCHER)
    {
        return false;
    }

    // Monsters capable of dual-wielding will always prefer two weapons
    // to a single two-handed one, however strong.
    if (mons_wields_two_weapons(*this)
        && hands_reqd(weap) == HANDS_TWO)
    {
        return false;
    }

    // Don't pick up a new weapon if we've been gifted one by the player.
    if (is_range_weapon(weap) && props.exists(BEOGH_RANGE_WPN_GIFT_KEY))
        return false;
    else if (props.exists(BEOGH_MELEE_WPN_GIFT_KEY))
        return false;

    // Arcane spellcasters don't want -Cast.
    if (is_actual_spellcaster()
        && is_artefact(weap)
        && artefact_property(weap, ARTP_PREVENT_SPELLCASTING))
    {
        return false;
    }

    // Nobody picks up giant clubs. Starting equipment is okay, of course.
    if (is_giant_club_type(weap.sub_type))
        return false;

    return true;
}

bool monster::wants_armour(const item_def &item) const
{
    // Monsters that are capable of dual wielding won't pick up shields.
    // Neither will monsters that are already wielding a two-hander.
    if (is_shield(item)
        && (mons_wields_two_weapons(*this)
            || mslot_item(MSLOT_WEAPON)
               && hands_reqd(*mslot_item(MSLOT_WEAPON))
                      == HANDS_TWO))
    {
        return false;
    }

    // Don't pick up new armour if we've been gifted something by the player.
    if (is_shield(item) && props.exists(BEOGH_SH_GIFT_KEY))
        return false;
    else if (props.exists(BEOGH_ARM_GIFT_KEY))
        return false;

    // Spellcasters won't pick up restricting armour, although they can
    // start with one. Applies to arcane spells only, of course.
    if (!pos().origin() && is_actual_spellcaster()
        && (property(item, PARM_EVASION) / 10 < -5
            || is_artefact(item)
               && artefact_property(item, ARTP_PREVENT_SPELLCASTING)))
    {
        return false;
    }

    // Returns whether this armour is the monster's size.
    return check_armour_size(item, body_size());
}

bool monster::wants_jewellery(const item_def &item) const
{
    // Arcane spellcasters don't want -Cast.
    if (is_actual_spellcaster()
        && is_artefact(item)
        && artefact_property(item, ARTP_PREVENT_SPELLCASTING))
    {
        return false;
    }

    if (item.sub_type == AMU_INACCURACY)
        return false;

    // TODO: figure out what monsters actually want rings or amulets
    return true;
}

// Monsters magically know the real properties of all items.
static int _get_monster_armour_value(const monster *mon,
                                     const item_def &item)
{
    // Each resistance/property counts as much as 1 point of AC.
    // Steam has been excluded because of its general uselessness.
    int value = item.armour_rating()
              + get_armour_res_fire(item, true)
              + get_armour_res_cold(item, true)
              + get_armour_res_elec(item, true)
              + get_armour_res_corr(item);

    // Give a simple bonus, no matter the size of the MR bonus.
    if (get_armour_res_magic(item, true) > 0)
        value++;

    // Poison becomes much less valuable if the monster is
    // intrinsically resistant.
    if (get_mons_resist(*mon, MR_RES_POISON) <= 0)
        value += get_armour_res_poison(item, true);

    // Same for life protection.
    if (mon->holiness() & MH_NATURAL)
        value += get_armour_life_protection(item, true);

    // See invisible also is only useful if not already intrinsic.
    if (!mons_class_flag(mon->type, M_SEE_INVIS))
        value += get_armour_see_invisible(item, true);

    // Give a sizable bonus for shields of reflection.
    if (get_armour_ego_type(item) == SPARM_REFLECTION)
        value += 3;

    // And an even more sizable bonus for boots/bardings of running.
    if (get_armour_ego_type(item) == SPARM_RUNNING)
        value += 5;

    return value;
}

/**
 * Attempt to have a monster pick up and wear the given armour item.
 * @param item  The item in question.
 * @param msg   Whether to print a message
 * @param force If true, force the monster to pick up and wear the item.
 * @return  True if the monster picks up and wears the item, false otherwise.
 */
bool monster::pickup_armour(item_def &item, bool msg, bool force)
{
    ASSERT(item.base_type == OBJ_ARMOUR);

    if (!force && !wants_armour(item))
        return false;

    const monster_type genus = mons_genus(mons_species(true));
    const monster_type base_type = mons_is_zombified(*this) ? base_monster
                                                            : type;
    equipment_type eq = EQ_NONE;

    // HACK to allow nagas/centaurs to wear bardings. (jpeg)
    switch (item.sub_type)
    {
    case ARM_NAGA_BARDING:
        if (genus == MONS_NAGA || genus == MONS_SALAMANDER)
            eq = EQ_BODY_ARMOUR;
        break;
    case ARM_CENTAUR_BARDING:
        if (genus == MONS_CENTAUR || genus == MONS_YAKTAUR)
            eq = EQ_BODY_ARMOUR;
        break;
    // And another hack or two...
    case ARM_HAT:
        if (base_type == MONS_GASTRONOK || genus == MONS_OCTOPODE)
            eq = EQ_BODY_ARMOUR;
        break;
    case ARM_CLOAK:
        if (base_type == MONS_MAURICE
            || base_type == MONS_NIKOLA
            || base_type == MONS_CRAZY_YIUF
            || genus == MONS_DRACONIAN)
        {
            eq = EQ_BODY_ARMOUR;
        }
        break;
    case ARM_GLOVES:
        if (base_type == MONS_NIKOLA)
            eq = EQ_SHIELD;
        break;
    case ARM_HELMET:
        if (base_type == MONS_ROBIN)
            eq = EQ_SHIELD;
        break;
    default:
        eq = get_armour_slot(item);

        if (eq == EQ_BODY_ARMOUR && genus == MONS_DRACONIAN)
            return false;

        if (eq != EQ_HELMET && base_type == MONS_GASTRONOK)
            return false;

        if (eq != EQ_HELMET && eq != EQ_SHIELD
            && genus == MONS_OCTOPODE)
        {
            return false;
        }
    }

    // Bardings are only wearable by the appropriate monster.
    if (eq == EQ_NONE)
        return false;

    // XXX: Monsters can only equip body armour and shields (as of 0.4).
    if (!force && eq != EQ_BODY_ARMOUR && eq != EQ_SHIELD)
        return false;

    const mon_inv_type mslot = equip_slot_to_mslot(eq);
    if (mslot == NUM_MONSTER_SLOTS)
        return false;

    int value_new = _get_monster_armour_value(this, item);

    // No armour yet -> get this one.
    if (!mslot_item(mslot) && value_new > 0)
        return pickup(item, mslot, msg);

    // Simplistic armour evaluation (comparing AC and resistances).
    if (const item_def *existing_armour = slot_item(eq, false))
    {
        if (!force)
        {
            int value_old = _get_monster_armour_value(this,
                                                      *existing_armour);
            if (value_old > value_new)
                return false;

            if (value_old == value_new)
            {
                // If items are of the same value, use shopping
                // value as a further crude estimate.
                value_old = item_value(*existing_armour, true);
                value_new = item_value(item, true);
            }
            if (value_old >= value_new)
                return false;
        }

        if (!drop_item(mslot, msg))
            return false;
    }

    return pickup(item, mslot, msg);
}

static int _get_monster_jewellery_value(const monster *mon,
                                   const item_def &item)
{
    ASSERT(item.base_type == OBJ_JEWELLERY);

    // Each resistance/property counts as one point.
    int value = 0;

    if (item.sub_type == RING_PROTECTION
        || item.sub_type == RING_EVASION
        || item.sub_type == RING_SLAYING
        || item.sub_type == AMU_REFLECTION)
    {
        value += item.plus;
    }

    if (item.sub_type == AMU_INACCURACY)
        value -= 5;

    value += get_jewellery_res_fire(item, true);
    value += get_jewellery_res_cold(item, true);
    value += get_jewellery_res_elec(item, true);

    // Give a simple bonus, no matter the size of the MR bonus.
    if (get_jewellery_res_magic(item, true) > 0)
        value++;

    // Poison becomes much less valuable if the monster is
    // intrinsically resistant.
    if (get_mons_resist(*mon, MR_RES_POISON) <= 0)
        value += get_jewellery_res_poison(item, true);

    // Same for life protection.
    if (mon->holiness() & MH_NATURAL)
        value += get_jewellery_life_protection(item, true);

    // See invisible also is only useful if not already intrinsic.
    if (!mons_class_flag(mon->type, M_SEE_INVIS))
        value += get_jewellery_see_invisible(item, true);

    // If we're not naturally corrosion-resistant.
    if (item.sub_type == RING_RESIST_CORROSION && !mon->res_corr(false, false))
        value++;

    return value;
}

bool monster::pickup_jewellery(item_def &item, bool msg, bool force)
{
    ASSERT(item.base_type == OBJ_JEWELLERY);

    if (!force && !wants_jewellery(item))
        return false;

    equipment_type eq = EQ_RINGS;

    const mon_inv_type mslot = equip_slot_to_mslot(eq);
    if (mslot == NUM_MONSTER_SLOTS)
        return false;

    int value_new = _get_monster_jewellery_value(this, item);

    // No armour yet -> get this one.
    if (!mslot_item(mslot) && value_new > 0)
        return pickup(item, mslot, msg);

    // Simplistic jewellery evaluation (comparing AC and resistances).
    if (const item_def *existing_jewellery = slot_item(eq, false))
    {
        if (!force)
        {
            int value_old = _get_monster_jewellery_value(this,
                                                         *existing_jewellery);
            if (value_old > value_new)
                return false;

            if (value_old == value_new)
            {
                // If items are of the same value, use shopping
                // value as a further crude estimate.
                value_old = item_value(*existing_jewellery, true);
                value_new = item_value(item, true);
                if (value_old >= value_new)
                    return false;
            }
        }

        if (!drop_item(mslot, msg))
            return false;
    }

    return pickup(item, mslot, msg);
}

bool monster::pickup_weapon(item_def &item, bool msg, bool force)
{
    if (!force && !wants_weapon(item))
        return false;

    // Weapon pickup involves:
    // - If we have no weapons, always pick this up.
    // - If this is a melee weapon and we already have a melee weapon, pick
    //   it up if it is superior to the one we're carrying (and drop the
    //   one we have).
    // - If it is a ranged weapon, and we already have a ranged weapon,
    //   pick it up if it is better than the one we have.

    if (is_range_weapon(item))
        return pickup_launcher(item, msg, force);

    if (pickup_melee_weapon(item, msg))
        return true;

    return false;
}

/**
 * Have a monster pick up a missile item.
 *
 * @param item  The item to pick up.
 * @param msg   Whether to print a message about the pickup.
 * @param force If true, the monster will always try to pick up the item.
 * @return  True if the monster picked up the missile, false otherwise.
*/
bool monster::pickup_missile(item_def &item, bool msg, bool force)
{
    const item_def *miss = missiles();

    if (!force)
    {
        if (item.sub_type == MI_THROWING_NET)
        {
            // Monster may not pick up trapping net.
            if (caught() && item_is_stationary_net(item))
                return false;
        }
        else // None of these exceptions hold for throwing nets.
        {
            // Spellcasters should not waste time with ammunition.
            // Neither summons nor hostile enchantments are counted for
            // this purpose.
            if (!force && mons_has_ranged_spell(*this, true, false))
                return false;

            // Monsters in a fight will only pick up missiles if doing so
            // is worthwhile.
            if (!mons_is_wandering(*this)
                && foe != MHITYOU
                && (item.quantity < 5 || miss && miss->quantity >= 7))
            {
                return false;
            }
        }
    }

    if (miss && items_stack(*miss, item))
        return pickup(item, MSLOT_MISSILE, msg);

    if (!force && !can_use_missile(item))
        return false;

    if (miss)
    {
        item_def *launch;
        for (int i = MSLOT_WEAPON; i <= MSLOT_ALT_WEAPON; ++i)
        {
            launch = mslot_item(static_cast<mon_inv_type>(i));
            if (launch)
            {
                const int item_brand = get_ammo_brand(item);
                // If this ammunition is better, drop the old ones.
                // Don't upgrade to ammunition whose brand cancels the
                // launcher brand or doesn't improve it further.
                // Don't drop huge stacks for tiny stacks.
                if (item.launched_by(*launch)
                    && (!miss->launched_by(*launch)
                        || item.sub_type == MI_SLING_BULLET
                           && miss->sub_type == MI_STONE
                        || get_ammo_brand(*miss) == SPMSL_NORMAL
                           && item_brand != SPMSL_NORMAL)
                    && item.quantity * 2 > miss->quantity)
                {
                    if (!drop_item(MSLOT_MISSILE, msg))
                        return false;
                    break;
                }
            }
        }

        // Allow upgrading throwing weapon brands (XXX: improve this!)
        if (item.sub_type == miss->sub_type
            && (item.sub_type == MI_TOMAHAWK || item.sub_type == MI_JAVELIN)
            && get_ammo_brand(*miss) == SPMSL_NORMAL
            && get_ammo_brand(item) != SPMSL_NORMAL)
        {
            if (!drop_item(MSLOT_MISSILE, msg))
                return false;
        }
    }

    return pickup(item, MSLOT_MISSILE, msg);
}

bool monster::pickup_wand(item_def &item, bool msg, bool force)
{
    if (!force)
    {
        // Don't pick up empty wands.
        if (item.plus == 0)
            return false;

        // Only low-HD monsters bother with wands.
        if (!likes_wand(item))
            return false;

        // Holy monsters and worshippers of good gods won't pick up evil
        // wands.
        if ((is_holy() || is_good_god(god)) && is_evil_item(item))
            return false;
    }

    // If a monster already has a charged wand, don't bother.
    // Otherwise, replace with a charged one.
    if (item_def *wand = mslot_item(MSLOT_WAND))
    {
        if (wand->plus > 0 && !force)
            return false;

        if (!drop_item(MSLOT_WAND, msg))
            return false;
    }

    if (pickup(item, MSLOT_WAND, msg))
    {
        if (msg)
            props["wand_known"] = item_type_known(item);
        return true;
    }
    else
        return false;
}

bool monster::pickup_scroll(item_def &item, bool msg)
{
    return pickup(item, MSLOT_SCROLL, msg);
}

bool monster::pickup_potion(item_def &item, bool msg, bool force)
{
    if (!can_drink() && !force)
        return false;
    return pickup(item, MSLOT_POTION, msg);
}

bool monster::pickup_gold(item_def &item, bool msg)
{
    return pickup(item, MSLOT_GOLD, msg);
}

bool monster::pickup_misc(item_def &item, bool msg, bool force)
{
    // Monsters can't use any miscellaneous items right now, so don't
    // let them pick them up unless explicitly given.
    if (!force)
        return false;
    return pickup(item, MSLOT_MISCELLANY, msg);
}

// Eaten items are handled elsewhere, in _handle_pickup() in mon-act.cc.
bool monster::pickup_item(item_def &item, bool msg, bool force)
{
    // Equipping stuff can be forced when initially equipping monsters.
    if (!force)
    {
        // If a monster isn't otherwise occupied (has a foe, is fleeing, etc.)
        // it is considered wandering.
        bool wandering = mons_is_wandering(*this);
        const int itype = item.base_type;

        // Weak(ened) monsters won't stop to pick up things as long as they
        // feel unsafe.
        if (!wandering && (hit_points * 10 < max_hit_points || hit_points < 10)
            && mon_enemies_around(this))
        {
            return false;
        }
        if (!wandering && !wont_attack())
        {
            // These are not important enough for pickup when
            // seeking, fleeing etc.
            if (itype == OBJ_ARMOUR || itype == OBJ_CORPSES
                || itype == OBJ_JEWELLERY
                || itype == OBJ_MISCELLANY || itype == OBJ_GOLD)
            {
                return false;
            }

            if (itype == OBJ_WEAPONS || itype == OBJ_MISSILES)
            {
                // Fleeing monsters only pick up emergency equipment.
                if (mons_is_fleeing(*this))
                    return false;

                // While occupied, hostile monsters won't pick up items
                // dropped or thrown by you. (You might have done that to
                // distract them.)
                if (testbits(item.flags, ISFLAG_DROPPED)
                    || testbits(item.flags, ISFLAG_THROWN))
                {
                    return false;
                }
            }
        }
    }

    switch (item.base_type)
    {
    // Pickup some stuff only if WANDERING.
    case OBJ_ARMOUR:
        return pickup_armour(item, msg, force);
    case OBJ_GOLD:
        return pickup_gold(item, msg);
    case OBJ_JEWELLERY:
        return pickup_jewellery(item, msg, force);
    // Fleeing monsters won't pick up these.
    // Hostiles won't pick them up if they were ever dropped/thrown by you.
    case OBJ_STAVES:
    case OBJ_WEAPONS:
        return pickup_weapon(item, msg, force);
    case OBJ_MISSILES:
        return pickup_missile(item, msg, force);
    // Other types can always be picked up
    // (barring other checks depending on subtype, of course).
    case OBJ_WANDS:
        return pickup_wand(item, msg, force);
    case OBJ_SCROLLS:
        return pickup_scroll(item, msg);
    case OBJ_POTIONS:
        return pickup_potion(item, msg, force);
    case OBJ_BOOKS:
    case OBJ_MISCELLANY:
        return pickup_misc(item, msg, force);
    default:
        return false;
    }
}

void monster::swap_weapons(maybe_bool maybe_msg)
{
    const bool msg = tobool(maybe_msg, observable());

    item_def *weap = mslot_item(MSLOT_WEAPON);
    item_def *alt  = mslot_item(MSLOT_ALT_WEAPON);

    if (weap && !unequip(*weap, msg))
    {
        // Item was cursed.
        // A centaur may randomly decide to not shoot you, but bashing
        // people with a ranged weapon is a dead giveaway.
        if (weap->cursed() && you.can_see(*this) && is_range_weapon(*weap))
            set_ident_flags(*weap, ISFLAG_KNOW_CURSE);
        return;
    }

    swap(inv[MSLOT_WEAPON], inv[MSLOT_ALT_WEAPON]);

    if (alt)
        equip(*alt, msg);

    // Monsters can swap weapons really fast. :-)
    if ((weap || alt) && speed_increment >= 2)
    {
        if (const monsterentry *entry = find_monsterentry())
            speed_increment -= div_rand_round(entry->energy_usage.attack, 5);
    }
}

void monster::wield_melee_weapon(maybe_bool msg)
{
    const item_def *weap = mslot_item(MSLOT_WEAPON);
    if (!weap || (!weap->cursed() && is_range_weapon(*weap)))
    {
        const item_def *alt = mslot_item(MSLOT_ALT_WEAPON);

        // Switch to the alternate weapon if it's not a ranged weapon, too,
        // or switch away from our main weapon if it's a ranged weapon.
        if (alt && !is_range_weapon(*alt)
            || weap && !alt && type != MONS_STATUE)
        {
            swap_weapons(msg);
        }
    }
}

item_def *monster::slot_item(equipment_type eq, bool include_melded) const
{
    return mslot_item(equip_slot_to_mslot(eq));
}

item_def *monster::mslot_item(mon_inv_type mslot) const
{
    const int mi = (mslot == NUM_MONSTER_SLOTS) ? NON_ITEM : inv[mslot];
    return mi == NON_ITEM ? nullptr : &mitm[mi];
}

item_def *monster::shield() const
{
    return mslot_item(MSLOT_SHIELD);
}

/**
 * Does this monster have a proper name?
 *
 * @return  Whether the monster has a proper name, e.g. "Rupert" or
 *          "Bogric the orc warlord". Should not include 'renamed' vault
 *          monsters, e.g. "decayed bog mummy" or "bag of meat".
 */
bool monster::is_named() const
{
    return mons_is_unique(type)
           || (!mname.empty() && !testbits(flags, MF_NAME_ADJECTIVE |
                                                  MF_NAME_REPLACE));
}

bool monster::has_base_name() const
{
    // Any non-ghost, non-Pandemonium demon that has an explicitly set
    // name has a base name.
    return !mname.empty() && !ghost.get();
}

static string _invalid_monster_str(monster_type type)
{
    string str = "INVALID MONSTER ";

    switch (type)
    {
    case NUM_MONSTERS:
        return str + "NUM_MONSTERS";
    case MONS_NO_MONSTER:
        return str + "MONS_NO_MONSTER";
    case MONS_PLAYER:
        return str + "MONS_PLAYER";
    case RANDOM_DRACONIAN:
        return str + "RANDOM_DRACONIAN";
    case RANDOM_BASE_DRACONIAN:
        return str + "RANDOM_BASE_DRACONIAN";
    case RANDOM_NONBASE_DRACONIAN:
        return str + "RANDOM_NONBASE_DRACONIAN";
    case WANDERING_MONSTER:
        return str + "WANDERING_MONSTER";
    default:
        break;
    }

    str += make_stringf("#%d", (int) type);

    if (type < 0)
        return str;

    if (type > NUM_MONSTERS)
    {
        str += make_stringf(" (NUM_MONSTERS + %d)",
                            int (NUM_MONSTERS - type));
        return str;
    }

    int          i;
    monster_type new_type;
    for (i = 0; true; i++)
    {
        new_type = (monster_type) (((int) type) - i);

        if (invalid_monster_type(new_type))
            continue;
        break;
    }
    str += make_stringf(" (%s + %d)",
                        mons_type_name(new_type, DESC_PLAIN).c_str(),
                        i);

    return str;
}

static string _mon_special_name(const monster& mon, description_level_type desc,
                                bool force_seen)
{
    if (desc == DESC_NONE)
        return "";

    const bool arena_submerged = crawl_state.game_is_arena() && !force_seen
                                     && mon.submerged();

    if (mon.type == MONS_NO_MONSTER)
        return "DEAD MONSTER";
    else if (mon.mid == MID_YOU_FAULTLESS)
        return "INVALID YOU_FAULTLESS";
    else if (invalid_monster_type(mon.type) && mon.type != MONS_PROGRAM_BUG)
        return _invalid_monster_str(mon.type);

    // Handle non-visible case first.
    if (!force_seen && !mon.observable() && !arena_submerged)
    {
        switch (desc)
        {
        case DESC_THE: case DESC_A: case DESC_PLAIN: case DESC_YOUR:
            return "something";
        case DESC_ITS:
            return "something's";
        default:
            return "it (buggy)";
        }
    }

    if (desc == DESC_DBNAME)
    {
        monster_info mi(&mon, MILEV_NAME);
        return mi.db_name();
    }

    return "";
}

string monster::name(description_level_type desc, bool force_vis,
                     bool force_article) const
{
    string s = _mon_special_name(*this, desc, force_vis);
    if (!s.empty() || desc == DESC_NONE)
        return s;

    monster_info mi(this, MILEV_NAME);
    // i.e. to produce "the Maras" instead of just "Maras"
    if (force_article)
        mi.mb.set(MB_NAME_UNQUALIFIED, false);
    return mi.proper_name(desc)
#ifdef DEBUG_MONINDEX
    // This is incredibly spammy, too bad for regular debug builds, but
    // I keep re-adding this over and over during debugging.
           + (Options.quiet_debug_messages[DIAG_MONINDEX]
              ? string()
              : make_stringf("«%d:%d»", mindex(), mid))
#endif
    ;
}

string monster::base_name(description_level_type desc, bool force_vis) const
{
    string s = _mon_special_name(*this, desc, force_vis);
    if (!s.empty() || desc == DESC_NONE)
        return s;

    monster_info mi(this, MILEV_NAME);
    return mi.common_name(desc);
}

string monster::full_name(description_level_type desc) const
{
    string s = _mon_special_name(*this, desc, true);
    if (!s.empty() || desc == DESC_NONE)
        return s;

    monster_info mi(this, MILEV_NAME);
    return mi.full_name(desc);
}

string monster::pronoun(pronoun_type pro, bool force_visible) const
{
    const bool seen = force_visible || you.can_see(*this);
    if (seen && props.exists(MON_GENDER_KEY))
    {
        return decline_pronoun((gender_type)props[MON_GENDER_KEY].get_int(),
                               pro);
    }
    return mons_pronoun(type, pro, seen);
}

string monster::conj_verb(const string &verb) const
{
    return conjugate_verb(verb, false);
}

string monster::hand_name(bool plural, bool *can_plural) const
{
    bool _can_plural;
    if (can_plural == nullptr)
        can_plural = &_can_plural;
    *can_plural = true;

    string str;
    char        ch = mons_base_char(type);

    const bool rand = (type == MONS_CHAOS_SPAWN);

    switch (get_mon_shape(*this))
    {
    case MON_SHAPE_CENTAUR:
    case MON_SHAPE_NAGA:
        // Defaults to "hand"
        break;
    case MON_SHAPE_HUMANOID:
    case MON_SHAPE_HUMANOID_WINGED:
    case MON_SHAPE_HUMANOID_TAILED:
    case MON_SHAPE_HUMANOID_WINGED_TAILED:
        if (ch == 'T' || ch == 'd' || ch == 'n' || mons_is_demon(type))
            str = "claw";
        break;

    case MON_SHAPE_QUADRUPED:
    case MON_SHAPE_QUADRUPED_TAILLESS:
    case MON_SHAPE_QUADRUPED_WINGED:
    case MON_SHAPE_ARACHNID:
        if (mons_genus(type) == MONS_SCORPION || rand && one_chance_in(4))
            str = "pincer";
        else
        {
            str = "front ";
            return str + foot_name(plural, can_plural);
        }
        break;

    case MON_SHAPE_BLOB:
    case MON_SHAPE_SNAKE:
    case MON_SHAPE_FISH:
        return foot_name(plural, can_plural);

    case MON_SHAPE_BAT:
    case MON_SHAPE_BIRD:
        str = "wing";
        break;

    case MON_SHAPE_INSECT:
    case MON_SHAPE_INSECT_WINGED:
    case MON_SHAPE_CENTIPEDE:
        str = "antenna";
        break;

    case MON_SHAPE_SNAIL:
        str = "eye-stalk";
        break;

    case MON_SHAPE_PLANT:
        str = "leaf";
        break;

    case MON_SHAPE_MISC:
        if (ch == 'x' || ch == 'X' || rand)
        {
            str = "tentacle";
            break;
        }
        // Deliberate fallthrough.
    case MON_SHAPE_FUNGUS:
        str         = "body";
        *can_plural = false;
        break;

    case MON_SHAPE_ORB:
        switch (type)
        {
            case MONS_BALLISTOMYCETE_SPORE:
                str = "rhizome";
                break;

            case MONS_FLOATING_EYE:
            case MONS_EYE_OF_DRAINING:
            case MONS_SHINING_EYE:
            case MONS_EYE_OF_DEVASTATION:
            case MONS_GOLDEN_EYE:
                *can_plural = false;
                // Deliberate fallthrough.
            case MONS_GREAT_ORB_OF_EYES:
                str = "pupil";
                break;

            case MONS_GLOWING_ORANGE_BRAIN:
            default:
                if (rand)
                    str = "rhizome";
                else
                {
                    str        = "body";
                    *can_plural = false;
                }
                break;
        }
        break;

    case MON_SHAPE_BUGGY:
        str = "handbug";
        break;
    }

    if (str.empty())
    {
        // Reduce the chance of a random-shaped monster having hands.
        if (rand && coinflip())
            return hand_name(plural, can_plural);

        str = "hand";
    }

    if (plural && *can_plural)
        str = pluralise(str);

    return str;
}

string monster::foot_name(bool plural, bool *can_plural) const
{
    bool _can_plural;
    if (can_plural == nullptr)
        can_plural = &_can_plural;
    *can_plural = true;

    string str;
    char        ch = mons_base_char(type);

    const bool rand = (type == MONS_CHAOS_SPAWN);

    switch (get_mon_shape(*this))
    {
    case MON_SHAPE_INSECT:
    case MON_SHAPE_INSECT_WINGED:
    case MON_SHAPE_ARACHNID:
    case MON_SHAPE_CENTIPEDE:
        str = "leg";
        break;

    case MON_SHAPE_HUMANOID:
    case MON_SHAPE_HUMANOID_WINGED:
    case MON_SHAPE_HUMANOID_TAILED:
    case MON_SHAPE_HUMANOID_WINGED_TAILED:
        if (type == MONS_MINOTAUR)
            str = "hoof";
        else if (swimming() && mons_genus(type) == MONS_MERFOLK)
        {
            str         = "tail";
            *can_plural = false;
        }
        break;

    case MON_SHAPE_CENTAUR:
        str = "hoof";
        break;

    case MON_SHAPE_QUADRUPED:
    case MON_SHAPE_QUADRUPED_TAILLESS:
    case MON_SHAPE_QUADRUPED_WINGED:
        if (rand)
        {
            const char* feet[] = {"paw", "talon", "hoof"};
            str = RANDOM_ELEMENT(feet);
        }
        else if (mons_genus(type) == MONS_HOG)
            str = "trotter";
        else if (ch == 'h')
            str = "paw";
        else if (ch == 'l' || ch == 'D')
            str = "talon";
        else if (type == MONS_YAK || type == MONS_DEATH_YAK)
            str = "hoof";
        else if (ch == 'H')
        {
            if (type == MONS_MANTICORE || type == MONS_SPHINX)
                str = "paw";
            else
                str = "talon";
        }
        break;

    case MON_SHAPE_BIRD:
        str = "talon";
        break;

    case MON_SHAPE_BAT:
        str = "claw";
        break;

    case MON_SHAPE_SNAKE:
    case MON_SHAPE_FISH:
        str         = "tail";
        *can_plural = false;
        break;

    case MON_SHAPE_PLANT:
        str = "root";
        break;

    case MON_SHAPE_FUNGUS:
        str         = "stem";
        *can_plural = false;
        break;

    case MON_SHAPE_BLOB:
        str = "pseudopod";
        break;

    case MON_SHAPE_MISC:
        if (ch == 'x' || ch == 'X' || rand)
        {
            str = "tentacle";
            break;
        }
        // Deliberate fallthrough.
    case MON_SHAPE_SNAIL:
    case MON_SHAPE_NAGA:
    case MON_SHAPE_ORB:
        str         = "underside";
        *can_plural = false;
        break;

    case MON_SHAPE_BUGGY:
        str = "footbug";
        break;
    }

    if (str.empty())
    {
        // Reduce the chance of a random-shaped monster having feet.
        if (rand && coinflip())
            return foot_name(plural, can_plural);

        return plural ? "feet" : "foot";
    }

    if (plural && *can_plural)
        str = pluralise(str);

    return str;
}

string monster::arm_name(bool plural, bool *can_plural) const
{
    mon_body_shape shape = get_mon_shape(*this);

    if (!mon_shape_is_humanoid(shape))
        return hand_name(plural, can_plural);

    if (can_plural != nullptr)
        *can_plural = true;

    string adj;
    string str = "arm";

    switch (mons_genus(type))
    {
    case MONS_DRACONIAN:
    case MONS_NAGA:
        adj = "scaled";
        break;

    case MONS_TENGU:
        adj = "feathered";
        break;

    case MONS_MUMMY:
        adj = "bandage-wrapped";
        break;

    case MONS_OCTOPODE:
        str = "tentacle";
        break;

    case MONS_LICH:
    case MONS_SKELETAL_WARRIOR:
    case MONS_ANCIENT_CHAMPION:
    case MONS_REVENANT:
        adj = "bony";
        break;

    default:
        break;
    }

    if (!adj.empty())
        str = adj + " " + str;

    if (plural)
        str = pluralise(str);

    return str;
}

int monster::mindex() const
{
    return this - menv.buffer();
}

/**
 * Sets the monster's "hit dice". Doesn't currently handle adjusting HP, etc.
 *
 * @param new_hit_dice      The new value to set HD to.
 */
void monster::set_hit_dice(int new_hit_dice)
{
    hit_dice = new_hit_dice;

    // XXX: this is unbelievably hacky to preserve old behaviour
    if (type == MONS_OKLOB_PLANT && !spells.empty()
        && spells[0].spell == SPELL_SPIT_ACID)
    {
        spells[0].freq = 200 * hit_dice / 30;
    }
}

void monster::moveto(const coord_def& c, bool clear_net)
{
    if (clear_net && c != pos() && in_bounds(pos()))
        mons_clear_trapping_net(this);

    if (mons_is_projectile(*this))
    {
        // Assume some means of displacement, normal moves will overwrite this.
        props[IOOD_X].get_float() += c.x - pos().x;
        props[IOOD_Y].get_float() += c.y - pos().y;
    }

    clear_constrictions_far_from(c);

    set_position(c);
}

bool monster::fumbles_attack()
{
    if (floundering() && one_chance_in(4))
    {
        if (you.can_see(*this))
        {
            mprf("%s %s", name(DESC_THE).c_str(), liquefied(pos())
                 ? "becomes momentarily stuck in the liquid earth."
                 : "splashes around in the water.");
        }
        else if (player_can_hear(pos(), LOS_RADIUS))
            mprf(MSGCH_SOUND, "You hear a splashing noise.");

        return true;
    }

    if (submerged())
        return true;

    return false;
}

void monster::attacking(actor * /* other */, bool /* ranged */)
{
}

// Sends a monster into a frenzy.
bool monster::go_frenzy(actor *source)
{
    if (!can_go_frenzy())
        return false;

    if (has_ench(ENCH_SLOW))
    {
        del_ench(ENCH_SLOW, true); // Give no additional message.
        simple_monster_message(*this,
            make_stringf(" shakes off %s lethargy.",
                         pronoun(PRONOUN_POSSESSIVE).c_str()).c_str());
    }
    del_ench(ENCH_HASTE, true);
    del_ench(ENCH_FATIGUE, true); // Give no additional message.

    const int duration = 16 + random2avg(13, 2);

    // store the attitude for later retrieval
    props["old_attitude"] = short(attitude);

    attitude = ATT_NEUTRAL;
    add_ench(mon_enchant(ENCH_INSANE, 0, source, duration * BASELINE_DELAY));
    if (holiness() & MH_NATURAL)
    {
        add_ench(mon_enchant(ENCH_HASTE, 0, source, duration * BASELINE_DELAY));
        add_ench(mon_enchant(ENCH_MIGHT, 0, source, duration * BASELINE_DELAY));
    }
    mons_att_changed(this);

    if (simple_monster_message(*this, " flies into a frenzy!"))
        // Xom likes monsters going insane.
        xom_is_stimulated(friendly() ? 25 : 100);

    return true;
}

bool monster::go_berserk(bool intentional, bool /* potion */)
{
    if (!can_go_berserk())
        return false;

    if (stasis())
        return false;

    if (has_ench(ENCH_SLOW))
    {
        del_ench(ENCH_SLOW, true); // Give no additional message.
        simple_monster_message(*this,
            make_stringf(" shakes off %s lethargy.",
                         pronoun(PRONOUN_POSSESSIVE).c_str()).c_str());
    }
    del_ench(ENCH_FATIGUE, true); // Give no additional message.
    del_ench(ENCH_FEAR, true);    // Going berserk breaks fear.
    behaviour = BEH_SEEK;

    // If we're intentionally berserking, use a melee weapon;
    // we won't be able to swap afterwards.
    if (intentional)
        wield_melee_weapon();

    add_ench(ENCH_BERSERK);
    if (simple_monster_message(*this, " goes berserk!"))
        // Xom likes monsters going berserk.
        xom_is_stimulated(friendly() ? 25 : 100);

    if (const item_def* w = weapon())
    {
        if (is_unrandom_artefact(*w, UNRAND_ZEALOT_SWORD))
            for (actor_near_iterator mi(pos(), LOS_NO_TRANS); mi; ++mi)
                if (mons_aligned(this, *mi))
                    mi->go_berserk(false);
    }

    return true;
}

void monster::expose_to_element(beam_type flavour, int strength,
                                bool slow_cold_blood)
{
    switch (flavour)
    {
    case BEAM_COLD:
        if (slow_cold_blood && mons_class_flag(type, M_COLD_BLOOD)
            && res_cold() <= 0 && coinflip())
        {
            do_slow_monster(*this, this, (strength + random2(5)) * BASELINE_DELAY);
        }
        break;
    case BEAM_WATER:
        del_ench(ENCH_STICKY_FLAME);
        break;
    case BEAM_FIRE:
    case BEAM_LAVA:
    case BEAM_STICKY_FLAME:
    case BEAM_STEAM:
        if (has_ench(ENCH_OZOCUBUS_ARMOUR))
        {
            // The 10 here is from expose_player_to_element.
            const int amount = strength ? strength : 10;
            if (!lose_ench_levels(get_ench(ENCH_OZOCUBUS_ARMOUR),
                                  amount * BASELINE_DELAY, true)
                && you.can_see(*this))
            {
                mprf("The heat melts %s icy armour.",
                     apostrophise(name(DESC_THE)).c_str());
            }
        }
        if (has_ench(ENCH_ICEMAIL))
            del_ench(ENCH_ICEMAIL);
        break;
    default:
        break;
    }
}

void monster::banish(actor *agent, const string &, const int, bool force)
{
    coord_def old_pos = pos();

    if (mons_is_projectile(type))
        return;

    if (!force && player_in_branch(BRANCH_ABYSS)
        && x_chance_in_y(you.depth, brdepth[BRANCH_ABYSS]))
    {
        simple_monster_message(*this, " wobbles for a moment.");
        return;
    }

    if (mons_is_tentacle_or_tentacle_segment(type))
    {
        monster* head = monster_by_mid(tentacle_connect);
        if (head)
        {
            head->banish(agent, "", 0, force);
            return;
        }
    }

    simple_monster_message(*this, " is devoured by a tear in reality.",
                           MSGCH_BANISHMENT);
    if (agent && mons_gives_xp(*this, *agent))
    {
        // Double the existing damage blame counts, so the unassigned xp for
        // remaining hp is effectively halved. No need to pass flags this way.
        damage_total *= 2;
        damage_friendly *= 2;
        blame_damage(agent, hit_points);
        // Note: we do not set MF_PACIFIED, the monster is usually not
        // distinguishable from others of the same kind in the Abyss.

        if (agent->is_player())
        {
            did_god_conduct(DID_BANISH, get_experience_level(),
                            true /*possibly wrong*/, this);
        }
    }
    monster_die(*this, KILL_BANISHED, NON_MONSTER);

    if (!cell_is_solid(old_pos))
        place_cloud(CLOUD_TLOC_ENERGY, old_pos, 5 + random2(8), 0);
    for (adjacent_iterator ai(old_pos); ai; ++ai)
        if (!cell_is_solid(*ai) && !cloud_at(*ai) && coinflip())
            place_cloud(CLOUD_TLOC_ENERGY, *ai, 1 + random2(8), 0);
}

bool monster::has_spells() const
{
    return spells.size() > 0;
}

bool monster::has_spell(spell_type spell) const
{
    return search_spells([=] (spell_type sp) { return sp == spell; } );
}

mon_spell_slot_flags monster::spell_slot_flags(spell_type spell) const
{
    mon_spell_slot_flags slot_flags;
    for (const mon_spell_slot &slot : spells)
        if (slot.spell == spell)
            slot_flags |= slot.flags;

    return slot_flags;
}

bool monster::has_unclean_spell() const
{
    return search_spells(is_unclean_spell);
}

bool monster::has_chaotic_spell() const
{
    return search_spells(is_chaotic_spell);
}

bool monster::has_corpse_violating_spell() const
{
    return search_spells(is_corpse_violating_spell);
}

bool monster::has_attack_flavour(int flavour) const
{
    for (int i = 0; i < 4; ++i)
    {
        const int attk_flavour = mons_attack_spec(*this, i).flavour;
        if (attk_flavour == flavour)
            return true;
    }

    return false;
}

bool monster::has_damage_type(int dam_type)
{
    for (int i = 0; i < 4; ++i)
    {
        const int dmg_type = damage_type(i);
        if (dmg_type == dam_type)
            return true;
    }

    return false;
}

int monster::constriction_damage() const
{
    for (int i = 0; i < 4; ++i)
    {
        const mon_attack_def attack = mons_attack_spec(*this, i);
        if (attack.type == AT_CONSTRICT)
            return attack.damage;
    }
    return -1;
}

/** Return true if the monster temporarily confused. False for butterflies, or
    other permanently confused monsters.
*/
bool monster::confused() const
{
    return mons_is_confused(*this);
}

static bool _you_responsible_for_ench(mon_enchant me)
{
    return me.who == KC_YOU
        || me.who == KC_FRIENDLY && !crawl_state.game_is_arena();
}

bool monster::confused_by_you() const
{
    if (mons_class_flag(type, M_CONFUSED))
        return false;

    const mon_enchant me = get_ench(ENCH_CONFUSION);
    const mon_enchant me2 = get_ench(ENCH_MAD);

    return (me.ench == ENCH_CONFUSION && _you_responsible_for_ench(me))
           || (me2.ench == ENCH_MAD && _you_responsible_for_ench(me2));
}

bool monster::paralysed() const
{
    return has_ench(ENCH_PARALYSIS) || has_ench(ENCH_DUMB);
}

bool monster::cannot_act() const
{
    return paralysed() || petrified();
}

bool monster::cannot_move() const
{
    return cannot_act();
}

bool monster::asleep() const
{
    return behaviour == BEH_SLEEP;
}

bool monster::backlit(bool self_halo) const
{
    if (has_ench(ENCH_CORONA) || has_ench(ENCH_STICKY_FLAME)
        || has_ench(ENCH_SILVER_CORONA))
    {
        return true;
    }

    return !umbraed() && haloed() && (self_halo || halo_radius() == -1);
}

bool monster::umbra() const
{
    return umbraed() && !haloed();
}

bool monster::caught() const
{
    return has_ench(ENCH_HELD);
}

bool monster::petrified() const
{
    return has_ench(ENCH_PETRIFIED);
}

bool monster::petrifying() const
{
    return has_ench(ENCH_PETRIFYING);
}

bool monster::liquefied_ground() const
{
    return liquefied(pos())
           && ground_level() && !is_insubstantial()
           && !mons_class_is_stationary(type);
}

// in units of 1/25 hp/turn
int monster::natural_regen_rate() const
{
    // A HD divider ranging from 3 (at 1 HD) to 1 (at 8 HD).
    int divider = max(div_rand_round(15 - get_hit_dice(), 4), 1);

    return max(div_rand_round(get_hit_dice(), divider), 1);
}

// in units of 1/100 hp/turn
int monster::off_level_regen_rate() const
{
    if (!mons_can_regenerate(*this))
        return 0;

    if (mons_class_fast_regen(type) || type == MONS_PLAYER_GHOST)
        return 100;
    // Capped at 0.1 hp/turn.
    return max(natural_regen_rate() * 4, 10);
}

bool monster::friendly() const
{
    return temp_attitude() == ATT_FRIENDLY;
}

bool monster::neutral() const
{
    mon_attitude_type att = temp_attitude();
    return att == ATT_NEUTRAL || att == ATT_GOOD_NEUTRAL
           || att == ATT_STRICT_NEUTRAL;
}

bool monster::good_neutral() const
{
    return temp_attitude() == ATT_GOOD_NEUTRAL;
}

bool monster::strict_neutral() const
{
    return temp_attitude() == ATT_STRICT_NEUTRAL;
}

bool monster::wont_attack() const
{
    return friendly() || good_neutral() || strict_neutral();
}

bool monster::pacified() const
{
    return attitude == ATT_NEUTRAL && testbits(flags, MF_PACIFIED);
}

/**
 * Returns whether the monster currently has any kind of shield.
 */
bool monster::shielded() const
{
    return shield()
           || has_ench(ENCH_BONE_ARMOUR)
           || wearing(EQ_AMULET_PLUS, AMU_REFLECTION) > 0;
}

int monster::shield_bonus() const
{
    if (incapacitated())
        return -100;

    int sh = -100;
    const item_def *shld = shield();
    if (shld && get_armour_slot(*shld) == EQ_SHIELD)
    {

        int shld_c = property(*shld, PARM_AC) + shld->plus * 2;
        shld_c = shld_c * 2 + (body_size(PSIZE_TORSO) - SIZE_MEDIUM)
                            * (shld->sub_type - ARM_LARGE_SHIELD);
        sh = random2avg(shld_c + get_hit_dice() * 4 / 3, 2) / 2;
    }
    if (has_ench(ENCH_BONE_ARMOUR))
    {
        const int bone_armour = 6 + get_hit_dice() / 3;
        sh = max(sh + bone_armour, bone_armour);
    }
    // shielding from jewellery
    const item_def *amulet = mslot_item(MSLOT_JEWELLERY);
    if (amulet && amulet->sub_type == AMU_REFLECTION)
    {
        const int jewellery_plus = amulet->plus;
        ASSERT(abs(jewellery_plus) < 30); // sanity check
        sh += jewellery_plus * 2;
    }

    return sh;
}

/**
 * After being hit or blocking an attack, possibly remove the monster's bone
 * armour (if it has any).
 *
 * Currently a 1/4 chance each time.
 */
void monster::maybe_degrade_bone_armour()
{
    if (has_ench(ENCH_BONE_ARMOUR) && one_chance_in(4))
    {
        del_ench(ENCH_BONE_ARMOUR);
        simple_monster_message(*this, "'s corpse armour sloughs away.");
    }
}

int monster::shield_block_penalty() const
{
    return 4 * shield_blocks * shield_blocks;
}

void monster::shield_block_succeeded(actor *attacker)
{
    actor::shield_block_succeeded(attacker);

    ++shield_blocks;
    maybe_degrade_bone_armour();
}

int monster::shield_bypass_ability(int) const
{
    return 15 + get_hit_dice() * 2 / 3;
}

int monster::missile_deflection() const
{
    if (has_ench(ENCH_DEFLECT_MISSILES))
        return 2;
    else if (has_ench(ENCH_REPEL_MISSILES) || scan_artefacts(ARTP_RMSL))
        return 1;
    else
        return 0;
}

void monster::ablate_deflection()
{
    // TODO: deduplicate this code
    if (has_ench(ENCH_DEFLECT_MISSILES))
    {
        if (one_chance_in(2 + spell_hd(SPELL_DEFLECT_MISSILES)))
            del_ench(ENCH_DEFLECT_MISSILES);
    }
    else if (has_ench(ENCH_REPEL_MISSILES))
    {
        if (one_chance_in(2 + spell_hd(SPELL_REPEL_MISSILES)))
            del_ench(ENCH_REPEL_MISSILES);
    }
}

/**
 * How many weapons of the given brand does this monster currently wield?
 *
 * @param mon           The monster in question.
 * @param brand         The brand in question.
 * @param calc_unid     Whether to include weapons whose brands are unknown (to
 *                      the player).
 * @return              The number of the aforementioned weapons currently
 *                      wielded.
 */
static int _weapons_with_prop(const monster *mon, brand_type brand,
                              bool calc_unid = true)
{
    int wielded = 0;

    const mon_inv_type last_weap_slot = mons_wields_two_weapons(*mon) ?
                                        MSLOT_ALT_WEAPON :
                                        MSLOT_WEAPON;
    for (int i = MSLOT_WEAPON; i <= last_weap_slot; i++)
    {
        const item_def *weap = mon->mslot_item(static_cast<mon_inv_type>(i));
        if (!weap)
            continue;

        if (!calc_unid && !item_ident(*weap, ISFLAG_KNOW_TYPE))
            continue;

        const int weap_brand = get_weapon_brand(*weap);
        if (brand == weap_brand)
            wielded++;
    }

    return wielded;
}

/**
 * What AC bonus or penalty does a given zombie type apply to the base
 * monster type's?
 *
 * @param type      The type of zombie. (Skeleton, simulac, etc)
 * @return          The ac modifier to apply to the base monster's AC.
 */
static int _zombie_ac_modifier(monster_type type)
{
    ASSERT(mons_class_is_zombified(type));

    switch (type)
    {
        case MONS_ZOMBIE:
        case MONS_SIMULACRUM:
            return -2;
        case MONS_SKELETON:
            return -6;
        case MONS_SPECTRAL_THING:
            return 2;
        default:
            die("invalid zombie type %d (%s)", type,
                mons_class_name(type));
    }
}

/**
 * What's the base armour class of this monster?
 *
 * Usually based on type; ghost demons can override this, and draconians/
 * demonspawn are... complicated.
 *
 * @return The base armour class of this monster, before applying item &
 *          status effects.
 */
int monster::base_armour_class() const
{
    ASSERT(!invalid_monster_type(type));

    // ghost demon struct overrides the monster values.
    if (mons_is_ghost_demon(type))
        return ghost->ac;

    // zombie, skeleton, etc ac mods
    if (mons_class_is_zombified(type))
    {
        // handle weird zombies for which type isn't enough to reconstruct ac
        // (e.g. zombies with jobs & demonghost zombies)
        const int base_ac = props.exists(ZOMBIE_BASE_AC_KEY) ?
                                props[ZOMBIE_BASE_AC_KEY].get_int() :
                                get_monster_data(base_monster)->AC;

        return _zombie_ac_modifier(type) + base_ac;
    }

    // abominations are weird.
    if (type == MONS_ABOMINATION_LARGE)
        return min(20, 7 + get_hit_dice() / 2);
    if (type == MONS_ABOMINATION_SMALL)
        return min(10, 3 + get_hit_dice() * 2 / 3);

    // Hepliaklqana ancestors scale with xl.
    if (mons_is_hepliaklqana_ancestor(type))
    {
        if (type == MONS_ANCESTOR_KNIGHT)
            return get_experience_level() + 7;
        return get_experience_level() / 2;
    }

    const int base_ac = get_monster_data(type)->AC;

    // demonspawn & draconians combine base & class ac values.
    if (mons_is_job(type))
    {
        ASSERT(!invalid_monster_type(base_monster));
        return base_ac + get_monster_data(base_monster)->AC;
    }

    // mutant beasts get extra AC for being musk oxen.
    if (has_facet(BF_OX))
        return base_ac + 5;

    return base_ac;
}

/**
 * What's the armour class of this monster?
 *
 * @param calc_unid     Whether to include unknown items/properties in the
 *                      calculated results.
 * @return              The armour class of this monster, including items,
 *                      statuses, etc.
 */
int monster::armour_class(bool calc_unid) const
{
    int ac = base_armour_class();

    // check for protection-brand weapons
    ac += 5 * _weapons_with_prop(this, SPWPN_PROTECTION, calc_unid);

    // armour from ac
    const item_def *armour = mslot_item(MSLOT_ARMOUR);
    if (armour)
        ac += armour_bonus(*armour, calc_unid);

    // armour from jewellery
    const item_def *ring = mslot_item(MSLOT_JEWELLERY);
    if (ring && ring->sub_type == RING_PROTECTION
        && (calc_unid
            || item_ident(*ring, ISFLAG_KNOW_TYPE | ISFLAG_KNOW_PLUSES)))
    {
        const int jewellery_plus = ring->plus;
        ASSERT(abs(jewellery_plus) < 30); // sanity check
        ac += jewellery_plus;
    }

    // various enchantments
    if (has_ench(ENCH_OZOCUBUS_ARMOUR))
        ac += 4 + get_hit_dice() / 3;
    if (has_ench(ENCH_ICEMAIL))
        ac += ICEMAIL_MAX;
    if (has_ench(ENCH_BONE_ARMOUR))
        ac += 6 + get_hit_dice() / 3;
    if (has_ench(ENCH_IDEALISED))
        ac += 4 + get_hit_dice() / 3;

    // Penalty due to bad temp mutations.
    if (has_ench(ENCH_WRETCHED))
        ac -= 8;

    // corrosion hurts.
    if (has_ench(ENCH_CORROSION))
        ac -= 8;

    return max(ac, 0);
}

/**
 * What EV bonus or penalty does a given zombie type apply to the base
 * monster type's?
 *
 * @param type      The type of zombie. (Skeleton, simulac, etc)
 * @return          The ev modifier to apply to the base monster's EV.
 */
static int _zombie_ev_modifier(monster_type type)
{
    ASSERT(mons_class_is_zombified(type));

    switch (type)
    {
        case MONS_ZOMBIE:
        case MONS_SIMULACRUM:
        case MONS_SPECTRAL_THING:
            return -5;
        case MONS_SKELETON:
            return -7;
        default:
            die("invalid zombie type %d (%s)", type,
                mons_class_name(type));
    }
}

/**
 * What's the base evasion of this monster?
 *
 * @return The base evasion of this monster, before applying items & statuses.
 **/
int monster::base_evasion() const
{
    // ghost demon struct overrides the monster values.
    if (mons_is_ghost_demon(type))
        return ghost->ev;

    // zombie, skeleton, etc ac mods
    if (mons_class_is_zombified(type))
    {
        // handle weird zombies for which type isn't enough to reconstruct ev
        // (e.g. zombies with jobs & demonghost zombies)
        const int base_ev = props.exists(ZOMBIE_BASE_EV_KEY) ?
                                props[ZOMBIE_BASE_EV_KEY].get_int() :
                                get_monster_data(base_monster)->ev;

        return _zombie_ev_modifier(type) + base_ev;
    }

    // abominations are weird.
    if (type == MONS_ABOMINATION_LARGE)
        return min(20, 2 * get_hit_dice() / 3);
    if (type == MONS_ABOMINATION_SMALL)
        return min(10, 4 + get_hit_dice());

    const int base_ev = get_monster_data(type)->ev;

    // demonspawn & draconians combine base & class ac values.
    if (mons_is_job(type))
        return base_ev + get_monster_data(base_monster)->ev;

    return base_ev;
}

/**
 * What's the current evasion of this monster?
 *
 * @param evit      A bitfield of ev modifiers to ignore.
 * @return The evasion of this monster, after applying items & statuses.
 **/
int monster::evasion(ev_ignore_type evit, const actor* /*act*/) const
{
    const bool calc_unid = !(evit & EV_IGNORE_UNIDED);

    int ev = base_evasion();

    // account for armour
    for (int slot = MSLOT_ARMOUR; slot <= MSLOT_SHIELD; slot++)
    {
        const item_def* armour = mslot_item(static_cast<mon_inv_type>(slot));
        if (armour)
        {
            ev += property(*armour, PARM_EVASION) / 10
                  / (is_shield(*armour) ? 2 : 6);
        }
    }

    // evasion from jewellery
    const item_def *ring = mslot_item(MSLOT_JEWELLERY);
    if (ring && ring->sub_type == RING_EVASION
        && (calc_unid
            || item_ident(*ring, ISFLAG_KNOW_TYPE | ISFLAG_KNOW_PLUSES)))
    {
        const int jewellery_plus = ring->plus;
        ASSERT(abs(jewellery_plus) < 30); // sanity check
        ev += jewellery_plus;
    }

    if (has_ench(ENCH_AGILE))
        ev += 5;

    if (evit & EV_IGNORE_HELPLESS)
        return max(ev, 0);

    if (paralysed() || petrified() || petrifying() || asleep())
        return 0;

    if (caught() || is_constricted())
        ev /= (body_size(PSIZE_BODY) + 2);
    else if (confused() || has_ench(ENCH_GRASPING_ROOTS))
        ev /= 2;

    return max(ev, 0);
}

bool monster::heal(int amount)
{
    if (mons_is_statue(type))
        return false;

    if (amount < 1)
        return false;
    else if (hit_points == max_hit_points)
        return false;

    hit_points += amount;

    bool success = true;

    if (hit_points > max_hit_points)
        hit_points = max_hit_points;

    if (hit_points == max_hit_points)
    {
        // Clear the damage blame if it goes away completely.
        damage_friendly = 0;
        damage_total = 0;
        props.erase("reaping_damage");
    }

    return success;
}

void monster::blame_damage(const actor* attacker, int amount)
{
    ASSERT(amount >= 0);
    damage_total = min<int>(MAX_DAMAGE_COUNTER, damage_total + amount);
    if (attacker)
    {
        damage_friendly = min<int>(MAX_DAMAGE_COUNTER * 2,
                      damage_friendly + amount * exp_rate(attacker->mindex()));
    }
}

void monster::suicide(int hp_target)
{
    ASSERT(hp_target <= 0);
    const int dam = hit_points - hp_target;
    if (dam > 0)
        blame_damage(nullptr, dam);
    hit_points = hp_target;
}

mon_holy_type monster::holiness(bool /*temp*/) const
{
    // zombie kraken tentacles
    if (testbits(flags, MF_FAKE_UNDEAD))
        return MH_UNDEAD;

    mon_holy_type holi = mons_class_holiness(type);

    // Assume that all unknown gods are not holy.
    if (is_priest() && is_good_god(god))
        holi |= MH_HOLY;

    // Assume that all unknown gods are evil.
    if (is_priest() && (is_evil_god(god) || is_unknown_god(god)))
        holi |= MH_EVIL;

    if (has_attack_flavour(AF_DRAIN_XP)
        || has_attack_flavour(AF_VAMPIRIC))
    {
        holi |= MH_EVIL;
    }

    if (testbits(flags, MF_SPECTRALISED))
        holi |= MH_EVIL;

    return holi;
}

bool monster::undead_or_demonic() const
{
    const mon_holy_type holi = holiness();

    return bool(holi & (MH_UNDEAD | MH_DEMONIC));
}

bool monster::is_holy(bool check_spells) const
{
    return bool(holiness() & MH_HOLY);
}

bool monster::is_nonliving(bool /*temp*/) const
{
    return bool(holiness() & MH_NONLIVING);
}

/** Is the monster considered unclean by Zin?
 *
 *  If not 0, then Zin won't let you have it as an ally, and gives
 *  piety for killing it.
 *  @param check_god whether the monster having a chaotic god matters.
 *  @returns 0 if not hated, a number greater than 0 otherwise.
 */
int monster::how_unclean(bool check_god) const
{
    int uncleanliness = 0;

    if (has_attack_flavour(AF_HUNGER))
        uncleanliness++;
    if (has_attack_flavour(AF_ROT))
        uncleanliness++;
    if (has_attack_flavour(AF_STEAL))
        uncleanliness++;
    if (has_attack_flavour(AF_VAMPIRIC))
        uncleanliness++;

    // Zin considers insanity unclean. And slugs that speak.
    if (type == MONS_CRAZY_YIUF
        || type == MONS_PSYCHE
        || type == MONS_LOUISE
        || type == MONS_GASTRONOK)
    {
        uncleanliness++;
    }

    // A floating mass of disease is nearly the definition of unclean.
    if (type == MONS_ANCIENT_ZYME)
        uncleanliness++;

    // Assume that all unknown gods are not chaotic.
    //
    // Being a worshipper of a chaotic god doesn't yet make you
    // physically/essentially chaotic (so you don't get hurt by silver),
    // but Zin does mind.
    if (is_priest() && is_chaotic_god(god) && check_god)
        uncleanliness++;

    if (has_unclean_spell())
        uncleanliness++;

    if (has_chaotic_spell() && is_actual_spellcaster())
        uncleanliness++;

    corpse_effect_type ce = mons_corpse_effect(type);
    if (ce == CE_MUTAGEN && !how_chaotic())
        uncleanliness++;

    // Corporeal undead are a perversion of natural form.
    if (holiness() & MH_UNDEAD && !is_insubstantial())
        uncleanliness++;

    return uncleanliness;
}

/** How chaotic do you know this monster to be?
 *
 * @param check_spells_god whether to look at its spells and/or
 *        religion; silver damage does not.
 * @returns 0 if not chaotic, a larger number if so.
 */
int monster::known_chaos(bool check_spells_god) const
{
    int chaotic = 0;

    if (type == MONS_UGLY_THING
        || type == MONS_VERY_UGLY_THING
        || type == MONS_ABOMINATION_SMALL
        || type == MONS_ABOMINATION_LARGE
        || type == MONS_WRETCHED_STAR
        || type == MONS_KILLER_KLOWN      // For their random attacks.
        || type == MONS_TIAMAT            // For her colour-changing.
        || type == MONS_BAI_SUZHEN
        || type == MONS_BAI_SUZHEN_DRAGON // For her transformation.
        || mons_is_demonspawn(type))      // Like player demonspawn.
    {
        chaotic++;
    }

    if (is_shapeshifter() && (flags & MF_KNOWN_SHIFTER))
        chaotic++;

    // Knowing chaotic spells is not enough to make you "essentially"
    // chaotic (i.e., silver doesn't hurt you), but it does make you
    // chaotic enough for Zin's chaos recitation. Having chaotic
    // abilities (not actual spells) does mean you're truly changed
    // by chaos.
    if (has_chaotic_spell() && (!is_actual_spellcaster()
                                || check_spells_god))
    {
        chaotic++;
    }

    if (has_attack_flavour(AF_MUTATE)
        || has_attack_flavour(AF_CHAOTIC))
    {
        chaotic++;
    }

    if (is_chaotic_god(god))
        chaotic++;

    if (is_chaotic_god(god) && is_priest())
        chaotic++;

    return chaotic;
}

/** How chaotic is this monster really?
 *
 * @param check_spells_god whether to look at its spells and/or
 *        religion; silver damage does not.
 * @returns 0 if not chaotic, a larger number if so.
 */
int monster::how_chaotic(bool check_spells_god) const
{
    // Don't count known shapeshifters twice.
    if (is_shapeshifter() && (flags & MF_KNOWN_SHIFTER))
        return known_chaos(check_spells_god);
    else
        return is_shapeshifter() + known_chaos(check_spells_god);
}

bool monster::is_unbreathing() const
{
    return mons_is_unbreathing(type);
}

bool monster::is_insubstantial() const
{
    return mons_class_flag(type, M_INSUBSTANTIAL);
}

/// Is this monster completely immune to Damnation-flavoured damage?
bool monster::res_damnation() const
{
    return get_mons_resist(*this, MR_RES_DAMNATION);
}

int monster::res_fire() const
{
    int u = get_mons_resist(*this, MR_RES_FIRE);

    if (mons_itemuse(*this) >= MONUSE_STARTING_EQUIPMENT)
    {
        u += scan_artefacts(ARTP_FIRE);

        const int armour    = inv[MSLOT_ARMOUR];
        const int shld      = inv[MSLOT_SHIELD];
        const int jewellery = inv[MSLOT_JEWELLERY];

        if (armour != NON_ITEM && mitm[armour].base_type == OBJ_ARMOUR)
            u += get_armour_res_fire(mitm[armour], false);

        if (shld != NON_ITEM && mitm[shld].base_type == OBJ_ARMOUR)
            u += get_armour_res_fire(mitm[shld], false);

        if (jewellery != NON_ITEM && mitm[jewellery].base_type == OBJ_JEWELLERY)
            u += get_jewellery_res_fire(mitm[jewellery], false);

        const item_def *w = primary_weapon();
        if (w && w->is_type(OBJ_STAVES, STAFF_FIRE))
            u++;
    }

    if (has_ench(ENCH_FIRE_VULN))
        u--;

    if (has_ench(ENCH_RESISTANCE))
        u++;

    if (u < -3)
        u = -3;
    else if (u > 3)
        u = 3;

    return u;
}

int monster::res_steam() const
{
    int res = get_mons_resist(*this, MR_RES_STEAM);
    if (wearing(EQ_BODY_ARMOUR, ARM_STEAM_DRAGON_ARMOUR))
        res += 3;

    res += (res_fire() + 1) / 2;

    if (res > 3)
        res = 3;

    return res;
}

int monster::res_cold() const
{
    int u = get_mons_resist(*this, MR_RES_COLD);

    if (mons_itemuse(*this) >= MONUSE_STARTING_EQUIPMENT)
    {
        u += scan_artefacts(ARTP_COLD);

        const int armour    = inv[MSLOT_ARMOUR];
        const int shld      = inv[MSLOT_SHIELD];
        const int jewellery = inv[MSLOT_JEWELLERY];

        if (armour != NON_ITEM && mitm[armour].base_type == OBJ_ARMOUR)
            u += get_armour_res_cold(mitm[armour], false);

        if (shld != NON_ITEM && mitm[shld].base_type == OBJ_ARMOUR)
            u += get_armour_res_cold(mitm[shld], false);

        if (jewellery != NON_ITEM && mitm[jewellery].base_type == OBJ_JEWELLERY)
            u += get_jewellery_res_cold(mitm[jewellery], false);

        const item_def *w = primary_weapon();
        if (w && w->is_type(OBJ_STAVES, STAFF_COLD))
            u++;
    }

    if (has_ench(ENCH_RESISTANCE))
        u++;

    if (u < -3)
        u = -3;
    else if (u > 3)
        u = 3;

    return u;
}

int monster::res_elec() const
{
    // This is a variable, not a player_xx() function, so can be above 1.
    int u = 0;

    u += get_mons_resist(*this, MR_RES_ELEC);

    // Don't bother checking equipment if the monster can't use it.
    if (mons_itemuse(*this) >= MONUSE_STARTING_EQUIPMENT)
    {
        u += scan_artefacts(ARTP_ELECTRICITY);

        // No ego armour, but storm dragon.
        // Also no non-artefact rings at present,
        // but it doesn't hurt to be thorough.
        const int armour    = inv[MSLOT_ARMOUR];
        const int jewellery = inv[MSLOT_JEWELLERY];

        if (armour != NON_ITEM && mitm[armour].base_type == OBJ_ARMOUR)
            u += get_armour_res_elec(mitm[armour], false);

        if (jewellery != NON_ITEM && mitm[jewellery].base_type == OBJ_JEWELLERY)
            u += get_jewellery_res_elec(mitm[jewellery], false);

        const item_def *w = primary_weapon();
        if (w && w->is_type(OBJ_STAVES, STAFF_AIR))
            u++;
    }

    if (has_ench(ENCH_RESISTANCE))
        u++;

    // Monsters can legitimately get multiple levels of electricity resistance.

    return u;
}

int monster::res_water_drowning() const
{
    int rw = 0;

    if (is_unbreathing())
        rw++;

    habitat_type hab = mons_habitat(*this);
    if (hab == HT_WATER || hab == HT_AMPHIBIOUS)
        rw++;

    if (get_mons_resist(*this, MR_VUL_WATER))
        rw--;

    return sgn(rw);
}

int monster::res_poison(bool temp) const
{
    int u = get_mons_resist(*this, MR_RES_POISON);

    if (temp && has_ench(ENCH_POISON_VULN))
        u--;

    if (u > 0)
        return u;

    if (mons_itemuse(*this) >= MONUSE_STARTING_EQUIPMENT)
    {
        u += scan_artefacts(ARTP_POISON);

        const int armour    = inv[MSLOT_ARMOUR];
        const int shld      = inv[MSLOT_SHIELD];
        const int jewellery = inv[MSLOT_JEWELLERY];

        if (armour != NON_ITEM && mitm[armour].base_type == OBJ_ARMOUR)
            u += get_armour_res_poison(mitm[armour], false);

        if (shld != NON_ITEM && mitm[shld].base_type == OBJ_ARMOUR)
            u += get_armour_res_poison(mitm[shld], false);

        if (jewellery != NON_ITEM && mitm[jewellery].base_type == OBJ_JEWELLERY)
            u += get_jewellery_res_poison(mitm[jewellery], false);

        const item_def *w = primary_weapon();
        if (w && w->is_type(OBJ_STAVES, STAFF_POISON))
            u++;
    }

    if (has_ench(ENCH_RESISTANCE))
        u++;

    // Monsters can have multiple innate levels of poison resistance, but
    // like players, equipment doesn't stack.
    if (u > 0)
        return 1;
    return u;
}

bool monster::res_sticky_flame() const
{
    return is_insubstantial() || get_mons_resist(*this, MR_RES_STICKY_FLAME) > 0;
}

int monster::res_rotting(bool /*temp*/) const
{
    int res = 0;
    const mon_holy_type holi = holiness();

    // handle undead first so that multi-holiness undead get their due
    if (holi & MH_UNDEAD)
    {
        if (mons_genus(type) == MONS_GHOUL || type == MONS_ZOMBIE)
            res = 1;
        else
            res = 3;
    }
    else if (holi & (MH_NATURAL | MH_PLANT))
        res = 0; // was 1 for plants before. Gardening shows it should be -1
    else if (holi & (MH_HOLY | MH_DEMONIC))
        res = 1;
    else if (is_nonliving())
        res = 3;

    if (is_insubstantial())
        res = 3;
    if (get_mons_resist(*this, MR_RES_ROTTING))
        res += 1;

    return min(3, res);
}

int monster::res_holy_energy() const
{
    if (type == MONS_PROFANE_SERVITOR)
        return 3;

    if (undead_or_demonic())
        return -1;

    if (is_holy()
        || is_good_god(god)
        || is_good_god(you.religion) && is_follower(*this))
    {
        return 3;
    }

    return 0;
}

int monster::res_negative_energy(bool intrinsic_only) const
{
    // If you change this, also change get_mons_resists.
    if (!(holiness() & MH_NATURAL))
        return 3;

    int u = get_mons_resist(*this, MR_RES_NEG);

    if (mons_itemuse(*this) >= MONUSE_STARTING_EQUIPMENT && !intrinsic_only)
    {
        u += scan_artefacts(ARTP_NEGATIVE_ENERGY);

        const int armour    = inv[MSLOT_ARMOUR];
        const int shld      = inv[MSLOT_SHIELD];
        const int jewellery = inv[MSLOT_JEWELLERY];

        if (armour != NON_ITEM && mitm[armour].base_type == OBJ_ARMOUR)
            u += get_armour_life_protection(mitm[armour], false);

        if (shld != NON_ITEM && mitm[shld].base_type == OBJ_ARMOUR)
            u += get_armour_life_protection(mitm[shld], false);

        if (jewellery != NON_ITEM && mitm[jewellery].base_type == OBJ_JEWELLERY)
            u += get_jewellery_life_protection(mitm[jewellery], false);

        const item_def *w = primary_weapon();
        if (w && w->is_type(OBJ_STAVES, STAFF_DEATH))
            u++;
    }

    if (u > 3)
        u = 3;

    return u;
}

bool monster::res_torment() const
{
    const mon_holy_type holy = holiness();
    return holy & (MH_UNDEAD | MH_DEMONIC | MH_PLANT | MH_NONLIVING)
           || get_mons_resist(*this, MR_RES_TORMENT) > 0;
}

bool monster::res_wind() const
{
    return has_ench(ENCH_TORNADO) || get_mons_resist(*this, MR_RES_WIND) > 0;
}

bool monster::res_petrify(bool /*temp*/) const
{
    return is_insubstantial() || get_mons_resist(*this, MR_RES_PETRIFY) > 0;
}

int monster::res_constrict() const
{
    // 3 is immunity, 1 or 2 reduces damage
    if (is_insubstantial())
        return 3;
    if (mons_genus(type) == MONS_JELLY)
        return 3;
    if (is_spiny())
        return 3;

    return 0;
}

bool monster::res_corr(bool calc_unid, bool items) const
{
    if (get_mons_resist(*this, MR_RES_ACID) > 0)
        return true;

    return actor::res_corr(calc_unid, items);
}

int monster::res_acid(bool calc_unid) const
{
    int u = max(get_mons_resist(*this, MR_RES_ACID), (int)actor::res_corr(calc_unid));

    if (has_ench(ENCH_RESISTANCE))
        u++;

    return u;
}

/**
 * What MR (resistance to hexes, etc) does this monster have?
 *
 * @param calc_unid     Whether to include items & effects the player may not
 *                      know about.
 * @return              The monster's magic resistance value.
 */
int monster::res_magic(bool calc_unid) const
{
    if (mons_immune_magic(*this))
        return MAG_IMMUNE;

    const int type_mr = (get_monster_data(type))->resist_magic;
    // Negative values get multiplied with monster hit dice.
    int u = type_mr < 0 ?
                get_hit_dice() * -type_mr * 4 / 3 :
                mons_class_res_magic(type, base_monster);

    // Hepliaklqana ancestors scale with xl.
    if (mons_is_hepliaklqana_ancestor(type))
        u = get_experience_level() * get_experience_level() / 2; // 0-160ish

    // Draining/malmutation reduce monster base MR proportionately.
    const int HD = get_hit_dice();
    if (HD < get_experience_level())
        u = u * HD / get_experience_level();

    // Resistance from artefact properties.
    u += 40 * scan_artefacts(ARTP_MAGIC_RESISTANCE);

    // Ego equipment resistance.
    const int armour    = inv[MSLOT_ARMOUR];
    const int shld      = inv[MSLOT_SHIELD];
    const int jewellery = inv[MSLOT_JEWELLERY];

    // XXX: should also include artefacts mr props
    // (remove ", false" and add appropriate flag checks for calc_unid)

    if (armour != NON_ITEM && mitm[armour].base_type == OBJ_ARMOUR
        && (calc_unid || (mitm[armour].flags & ISFLAG_KNOW_TYPE)))
    {
        u += get_armour_res_magic(mitm[armour], false);
    }

    if (shld != NON_ITEM && mitm[shld].base_type == OBJ_ARMOUR
        && (calc_unid || (mitm[shld].flags & ISFLAG_KNOW_TYPE)))
    {
        u += get_armour_res_magic(mitm[shld], false);
    }

    if (jewellery != NON_ITEM && mitm[jewellery].base_type == OBJ_JEWELLERY
        && calc_unid) // XXX: can you ever see monster jewellery?
    {
        u += get_jewellery_res_magic(mitm[jewellery], false);
    }

    if (has_ench(ENCH_RAISED_MR)) //trog's hand
        u += 80;

    if (has_ench(ENCH_LOWERED_MR))
        u /= 2;

    if (u < 0)
        u = 0;

    return u;
}

bool monster::no_tele(bool calc_unid, bool permit_id, bool blinking) const
{
    // Plants can't survive without roots, so it's either this or auto-kill.
    // Statues have pedestals so moving them is weird.
    if (mons_class_is_stationary(type))
        return true;

    if (mons_is_projectile(type))
        return true;

    // Might be better to teleport the whole kraken instead...
    if (mons_is_tentacle_or_tentacle_segment(type))
        return true;

    if (stasis())
        return true;

    // TODO: permit_id
    if (has_notele_item(calc_unid))
        return true;

    if (has_ench(ENCH_DIMENSION_ANCHOR))
        return true;

    return false;
}

bool monster::antimagic_susceptible() const
{
    return search_slots([] (const mon_spell_slot& slot)
                        { return bool(slot.flags & MON_SPELL_ANTIMAGIC_MASK); });
}

bool monster::airborne() const
{
    // For dancing weapons, this function can get called before their
    // ghost_demon is created, so check for a nullptr ghost. -cao
    return mons_is_ghost_demon(type) && ghost.get() && ghost->flies
           // check both so spectral humans and zombified dragons both fly
           || mons_class_flag(mons_base_type(*this), M_FLIES)
           || mons_class_flag(type, M_FLIES)
           || has_facet(BF_BAT)
           || scan_artefacts(ARTP_FLY) > 0
           || mslot_item(MSLOT_ARMOUR)
              && mslot_item(MSLOT_ARMOUR)->base_type == OBJ_ARMOUR
              && mslot_item(MSLOT_ARMOUR)->brand == SPARM_FLYING
           || mslot_item(MSLOT_JEWELLERY)
              && mslot_item(MSLOT_JEWELLERY)->is_type(OBJ_JEWELLERY, RING_FLIGHT)
           || has_ench(ENCH_FLIGHT);
}

bool monster::is_banished() const
{
    return !alive() && flags & MF_BANISHED;
}

monster_type monster::mons_species(bool zombie_base) const
{
    if (zombie_base && mons_class_is_zombified(type))
        return ::mons_species(base_monster);
    return ::mons_species(type);
}

bool monster::poison(actor *agent, int amount, bool force)
{
    if (amount <= 0)
        return false;

    // Scale poison down for monsters.
    amount = 1 + amount / 7;

    return poison_monster(this, agent, amount, force);
}

int monster::skill(skill_type sk, int scale, bool real, bool drained) const
{
    // Let spectral weapons have necromancy skill for pain brand.
    if (mons_intel(*this) < I_HUMAN && !mons_is_avatar(type))
        return 0;

    const int hd = scale * get_hit_dice();
    int ret;
    switch (sk)
    {
    case SK_EVOCATIONS:
        return hd;

    case SK_NECROMANCY:
        return (has_spell_of_type(SPTYP_NECROMANCY)) ? hd : hd/2;

    case SK_POISON_MAGIC:
    case SK_FIRE_MAGIC:
    case SK_ICE_MAGIC:
    case SK_EARTH_MAGIC:
    case SK_AIR_MAGIC:
    case SK_SUMMONINGS:
        return is_actual_spellcaster() ? hd : hd / 3;

    // Weapon skills for spectral weapon
    case SK_SHORT_BLADES:
    case SK_LONG_BLADES:
    case SK_AXES:
    case SK_MACES_FLAILS:
    case SK_POLEARMS:
    case SK_STAVES:
        ret = hd;
        if (weapon()
            && sk == item_attack_skill(*weapon())
            && _is_signature_weapon(this, *weapon()))
        {
            // generally slightly skilled if it's a signature weapon
            ret = ret * 5 / 4;
        }
        return ret;

    default:
        return 0;
    }
}

/**
 * Move a monster to an approximate location.
 *
 * @param p the position to end up near.
 * @return whether it tried to move the monster at all.
 */
bool monster::shift(coord_def p)
{
    coord_def result;

    int count = 0;

    if (p.origin())
        p = pos();

    for (adjacent_iterator ai(p); ai; ++ai)
    {
        // Don't drop on anything but vanilla floor right now.
        if (grd(*ai) != DNGN_FLOOR)
            continue;

        if (actor_at(*ai))
            continue;

        if (one_chance_in(++count))
            result = *ai;
    }

    if (count > 0)
        move_to_pos(result);

    return count > 0;
}
void monster::blink()
{
    monster_blink(this);
}

void monster::teleport(bool now, bool)
{
    monster_teleport(this, now, false);
}

bool monster::alive() const
{
    return hit_points > 0 && type != MONS_NO_MONSTER;
}

god_type monster::deity() const
{
    return god;
}

bool monster::drain_exp(actor *agent, bool quiet, int pow)
{
    if (res_negative_energy() >= 3)
        return false;

    if (!quiet && you.can_see(*this))
        mprf("%s is drained!", name(DESC_THE).c_str());

    // If quiet, don't clean up the monster in order to credit properly.
    hurt(agent, 2 + random2(3), BEAM_NEG, KILLED_BY_DRAINING, "", "", !quiet);

    if (alive())
    {
        int dur = min(200 + random2(100),
                      300 - get_ench(ENCH_DRAINED).duration - random2(50));

        if (res_negative_energy())
            dur /= (res_negative_energy() * 2);

        const mon_enchant drain_ench = mon_enchant(ENCH_DRAINED, 1, agent,
                                                   dur);
        add_ench(drain_ench);
    }

    return true;
}

bool monster::rot(actor *agent, int amount, bool quiet, bool no_cleanup)
{
    if (res_rotting() || amount <= 0)
        return false;

    if (!quiet && you.can_see(*this))
        mprf("%s looks less resilient!", name(DESC_THE).c_str());

    // If requested, don't clean up the monster in order to credit properly.
    hurt(agent, amount, BEAM_MISSILE, KILLED_BY_BEAM, "", "", !no_cleanup);

    if (alive())
    {
        max_hit_points -= amount * 2;
        hit_points = min(max_hit_points, hit_points);

        // Clean up the monster if that killed it (unless we shouldn't).
        if (!no_cleanup && max_hit_points <= 0)
            hurt(agent, 0, BEAM_MISSILE, KILLED_BY_BEAM);
    }

    return true;
}

bool monster::corrode_equipment(const char* corrosion_source, int degree)
{
    // Don't corrode spectral weapons or temporary items.
    if (mons_is_avatar(type) || type == MONS_PLAYER_SHADOW)
        return false;

    // rCorr protects against 50% of corrosion.
    // As long as degree is at least 1, we'll apply the status once, because
    // it doesn't look to me like applying it more times does anything.
    // If I'm wrong, we should fix that.
    if (res_corr())
    {
        degree = binomial(degree, 50);
        if (!degree)
        {
            dprf("rCorr protects.");
            return false;
        }
    }

    if (you.see_cell(pos()))
    {
        mprf("%s corrodes %s!",
             corrosion_source,
             name(DESC_THE).c_str());
    }

    add_ench(mon_enchant(ENCH_CORROSION, 0));
    return true;
}

/**
 * Attempts to apply corrosion to a monster.
 */
void monster::splash_with_acid(const actor* evildoer, int /*acid_strength*/,
                               bool /*allow_corrosion*/, const char* /*hurt_msg*/)
{
    const int dam = roll_dice(2, 4);
    const int post_res_dam = resist_adjust_damage(this, BEAM_ACID, dam);

    if (this->observable())
         mprf("%s is splashed with acid.", this->name(DESC_THE).c_str());

    if (!one_chance_in(3))
        corrode_equipment();

    if (post_res_dam > 0)
        hurt(evildoer, post_res_dam, BEAM_ACID, KILLED_BY_ACID);
}

int monster::hurt(const actor *agent, int amount, beam_type flavour,
                   kill_method_type kill_type, string /*source*/,
                   string /*aux*/, bool cleanup_dead, bool attacker_effects)
{
    if (mons_is_projectile(type)
        || mid == MID_ANON_FRIEND
        || type == MONS_PLAYER_SHADOW)
    {
        return 0;
    }

    if (alive())
    {
        if (amount != INSTANT_DEATH
            && mons_species(true) == MONS_DEEP_DWARF)
        {
            // Deep Dwarves get to shave _any_ hp loss. Player version:
            int shave = 1 + random2(2 + random2(1 + get_hit_dice() / 3));
            dprf("(mon) HP shaved: %d.", shave);
            amount -= shave;
            if (amount <= 0)
                return 0;
        }

        if (amount != INSTANT_DEATH)
        {
            if (petrified())
                amount /= 2;
            else if (petrifying())
                amount = amount * 2 / 3;
        }

        if (amount != INSTANT_DEATH && has_ench(ENCH_INJURY_BOND))
        {
            actor* guardian = get_ench(ENCH_INJURY_BOND).agent();
            if (guardian && guardian->alive() && mons_aligned(guardian, this))
            {
                int split = amount / 2;
                if (split > 0)
                {
                    deferred_damage_fineff::schedule(agent, guardian,
                                                     split, false);
                    amount -= split;
                }
            }
        }

        if (amount == INSTANT_DEATH)
            amount = hit_points;
        else if (get_hit_dice() <= 0)
            amount = hit_points;
        else if (amount <= 0 && hit_points <= max_hit_points)
            return 0;

        // Apply damage multipliers for amulet of harm
        if (amount != INSTANT_DEATH)
        {
            // +30% damage when the opponent has harm
            if (agent && agent->extra_harm())
                amount = amount * 13 / 10;
            // +20% damage when self has harm
            else if (extra_harm())
                amount = amount * 6 / 5;
        }

        // Apply damage multipliers for quad damage
        if (attacker_effects && agent && agent->is_player()
            && you.duration[DUR_QUAD_DAMAGE]
            && flavour != BEAM_TORMENT_DAMAGE)
        {
            amount *= 4;
            if (amount > hit_points + 50)
                flags |= MF_EXPLODE_KILL;
        }

        amount = min(amount, hit_points);
        hit_points -= amount;

        if (hit_points > max_hit_points)
        {
            amount    += hit_points - max_hit_points;
            hit_points = max_hit_points;
        }

        if (flavour == BEAM_DEVASTATION || flavour == BEAM_DISINTEGRATION)
        {
            if (can_bleed())
                blood_spray(pos(), type, amount / 5);

            if (!alive())
                flags |= MF_EXPLODE_KILL;
        }

        // Hurt conducts -- pain bond is exempted for balance/gameplay reasons.
        // Damage over time effects are excluded for similar reasons.
        if (agent && agent->is_player() && mons_gives_xp(*this, *agent)
            && flavour != BEAM_SHARED_PAIN
            && flavour != BEAM_STICKY_FLAME
            && kill_type != KILLED_BY_POISON
            && kill_type != KILLED_BY_CLOUD)
        {
           did_hurt_conduct(DID_HURT_FOE, *this, amount);
        }

        // Handle pain bond behavior here. Is technically passive damage.
        // radiate_pain_bond may do additional damage by recursively looping
        // back to the original trigger.
        if (has_ench(ENCH_PAIN_BOND) && flavour != BEAM_SHARED_PAIN)
        {
            int hp_before_pain_bond = hit_points;
            radiate_pain_bond(*this, amount, this);
            amount += hp_before_pain_bond - hit_points;
        }

        // Allow the victim to exhibit passive damage behaviour (e.g.
        // the Royal Jelly or Uskayaw's Pain Bond).
        react_to_damage(agent, amount, flavour);

        // Don't mirror Yredelemnul's effects (in particular don't mirror
        // mirrored damage).
        if (has_ench(ENCH_MIRROR_DAMAGE)
            && crawl_state.which_god_acting() != GOD_YREDELEMNUL)
        {
            // ensure that YOU_FAULTLESS is converted to `you`. this may still
            // fail e.g. when the damage is from a vault-created cloud
            if (auto valid_agent = ensure_valid_actor(agent))
                mirror_damage_fineff::schedule(valid_agent, this, amount * 2 / 3);
        }

        blame_damage(agent, amount);
    }

    if (cleanup_dead && (hit_points <= 0 || get_hit_dice() <= 0)
        && type != MONS_NO_MONSTER)
    {
        if (agent == nullptr)
            monster_die(*this, KILL_MISC, NON_MONSTER);
        else if (agent->is_player())
            monster_die(*this, KILL_YOU, NON_MONSTER);
        else
            monster_die(*this, KILL_MON, agent->mindex());
    }

    return amount;
}

void monster::confuse(actor *atk, int strength)
{
    if (!check_clarity(false))
        enchant_actor_with_flavour(this, atk, BEAM_CONFUSION, strength);
}

void monster::paralyse(actor *atk, int strength, string cause)
{
    enchant_actor_with_flavour(this, atk, BEAM_PARALYSIS, strength);
}

void monster::petrify(actor *atk, bool force)
{
    enchant_actor_with_flavour(this, atk, BEAM_PETRIFY);
}

bool monster::fully_petrify(actor *atk, bool quiet)
{
    bool msg = !quiet && simple_monster_message(*this, mons_is_immotile(*this) ?
                         " turns to stone!" : " stops moving altogether!");

    add_ench(ENCH_PETRIFIED);
    return msg;
}

void monster::slow_down(actor *atk, int strength)
{
    enchant_actor_with_flavour(this, atk, BEAM_SLOW, strength);
}

void monster::set_ghost(const ghost_demon &g)
{
    ghost.reset(new ghost_demon(g));

    if (!ghost->name.empty())
        mname = ghost->name;
}

void monster::set_new_monster_id()
{
    mid = ++you.last_mid;
    // Sorry, if you made 4294901759 monsters over the course of your
    // game you deserve a crash, particularly when the game doesn't
    // even last that many turns.
    ASSERT(mid < MID_FIRST_NON_MONSTER);
    env.mid_cache[mid] = mindex();
}

void monster::ghost_init(bool need_pos)
{
    ghost_demon_init();

    god             = ghost->religion;
    attitude        = ATT_HOSTILE;
    behaviour       = BEH_WANDER;
    flags           = MF_NO_FLAGS;
    foe             = MHITNOT;
    foe_memory      = 0;
    number          = MONS_NO_MONSTER;

    // Ghosts can't worship good gods, but keep the god in the ghost
    // structure so the ghost can comment on it.
    if (is_good_god(god))
        god = GOD_NO_GOD;

    inv.init(NON_ITEM);
    enchantments.clear();
    ench_cache.reset();
    ench_countdown = 0;

    // Summoned player ghosts are already given a position; calling this
    // in those instances will cause a segfault. Instead, check to see
    // if we have a home first. {due}
    if (need_pos && !in_bounds(pos()))
        find_place_to_live();
}

void monster::uglything_init(bool only_mutate)
{
    // If we're mutating an ugly thing, leave its experience level, hit
    // dice and maximum and current hit points as they are.
    if (!only_mutate)
    {
        hit_dice        = ghost->xl;
        max_hit_points  = ghost->max_hp;
        hit_points      = max_hit_points;
    }

    speed           = ghost->speed;
    speed_increment = 70;
    colour          = ghost->colour;
}

void monster::ghost_demon_init()
{
    hit_dice        = ghost->xl;
    max_hit_points  = min<short int>(ghost->max_hp, MAX_MONSTER_HP);
    hit_points      = max_hit_points;
    speed           = ghost->speed;
    speed_increment = 70;
    if (ghost->colour != COLOUR_UNDEF)
        colour = ghost->colour;

    load_ghost_spells();
}

void monster::uglything_mutate(colour_t force_colour)
{
    ghost->init_ugly_thing(type == MONS_VERY_UGLY_THING, true, force_colour);
    uglything_init(true);
}

// Randomise potential damage.
static int _estimated_trap_damage(trap_type trap)
{
    switch (trap)
    {
        case TRAP_BLADE: return 10 + random2(30);
        case TRAP_ARROW: return random2(7);
        case TRAP_SPEAR: return random2(10);
        case TRAP_BOLT:  return random2(13);
        default:         return 0;
    }
}

/**
 * Check whether a given trap (described by trap position) can be
 * regarded as safe. Takes into account monster intelligence and
 * allegiance.
 *
 * @param where       The square to be checked for dangerous traps.
 * @param just_check  Used for intelligent monsters trying to avoid traps.
 * @return            Whether the monster will willingly enter the square.
 */
bool monster::is_trap_safe(const coord_def& where, bool just_check) const
{
    const mon_intel_type intel = mons_intel(*this);

    const trap_def *ptrap = trap_at(where);
    if (!ptrap)
        return true;
    const trap_def& trap = *ptrap;

    const bool player_knows_trap = (trap.is_known(&you));

    // Known shafts are safe. Unknown ones are unknown.
    if (trap.type == TRAP_SHAFT)
        return true;

#if TAG_MAJOR_VERSION == 34
    if (trap.type == TRAP_SHADOW_DORMANT || trap.type == TRAP_SHADOW)
        return true;
#endif

    // No friendly monsters will ever enter a Zot trap you know.
    if (player_knows_trap && friendly() && trap.type == TRAP_ZOT)
        return false;

    // Dumb monsters don't care at all.
    if (intel == I_BRAINLESS)
        return true;

    // Ditto, berserkers & frenzied creatures.
    if (berserk_or_insane())
        return true;

    // Hostile monsters are not afraid of non-mechanical traps.
    // Allies will try to avoid teleportation and zot traps.
    const bool mechanical = (trap.category() == DNGN_TRAP_MECHANICAL);

    if (trap.is_known(this))
    {
        if (just_check)
            return false; // Square is blocked.

        // Test for corridor-like environment.
        const int x = where.x - pos().x;
        const int y = where.y - pos().y;

        // The question is whether the monster (m) can easily reach its
        // presumable destination (x) without stepping on the trap. Traps
        // in corridors do not allow this. See e.g
        //  #x#        ##
        //  #^#   or  m^x
        //   m         ##
        //
        // The same problem occurs if paths are blocked by monsters,
        // hostile terrain or other traps rather than walls.
        // What we do is check whether the squares with the relative
        // positions (-1,0)/(+1,0) or (0,-1)/(0,+1) form a "corridor"
        // (relative to the _trap_ position rather than the monster one).
        // If they don't, the trap square is marked as "unsafe" (because
        // there's a good alternative move for the monster to take),
        // otherwise the decision will be made according to later tests
        // (monster hp, trap type, ...)
        // If a monster still gets stuck in a corridor it will usually be
        // because it has less than half its maximum hp.

        if ((mon_can_move_to_pos(this, coord_def(x-1, y), true)
             || mon_can_move_to_pos(this, coord_def(x+1,y), true))
            && (mon_can_move_to_pos(this, coord_def(x,y-1), true)
                || mon_can_move_to_pos(this, coord_def(x,y+1), true)))
        {
            return false;
        }
    }

    // Friendlies will try not to be parted from you.
    if (intelligent_ally(*this) && (trap.type == TRAP_TELEPORT
                                   || trap.type == TRAP_TELEPORT_PERMANENT)
        && player_knows_trap && can_see(you))
    {
        return false;
    }

    // Healthy monsters don't mind a little pain.
    if (mechanical && hit_points >= max_hit_points / 2
        && (intel < I_HUMAN
            || hit_points > _estimated_trap_damage(trap.type)))
    {
        return true;
    }

    // can't avoid traps you don't know about
    if (!trap.is_known(this))
        return true;

    // we don't think we have enough hp (per above), so avoid mech traps
    if (mechanical)
        return false;

    // Friendly and good neutral monsters don't enjoy Zot trap perks;
    // handle accordingly. In the arena Zot traps affect all monsters.

    // XXX: this logic duplicates the checks for friendly creatures & "traps
    // that are bad for the player " at the top; probably should be merged
    // somehow
    if (wont_attack() || crawl_state.game_is_arena())
        return trap.type != TRAP_ZOT;
    return true;
}

bool monster::is_cloud_safe(const coord_def &place) const
{
    return !mons_avoids_cloud(this, place);
}

bool monster::check_set_valid_home(const coord_def &place,
                                    coord_def &chosen,
                                    int &nvalid) const
{
    if (!in_bounds(place))
        return false;

    if (actor_at(place))
        return false;

    if (!monster_habitable_grid(this, grd(place)))
        return false;

    if (!is_trap_safe(place, true))
        return false;

    if (one_chance_in(++nvalid))
        chosen = place;

    return true;
}


bool monster::is_location_safe(const coord_def &place)
{
    if (!monster_habitable_grid(this, grd(place)))
        return false;

    if (!is_trap_safe(place, true))
        return false;

    if (!is_cloud_safe(place))
        return false;

    return true;
}

bool monster::has_originating_map() const
{
    return props.exists("map");
}

string monster::originating_map() const
{
    if (!has_originating_map())
        return "";
    return props["map"].get_string();
}

void monster::set_originating_map(const string &map_name)
{
    if (!map_name.empty())
        props["map"].get_string() = map_name;
}

#define MAX_PLACE_NEAR_DIST 8

bool monster::find_home_near_place(const coord_def &c)
{
    int last_dist = -1;
    coord_def place(-1, -1);
    int nvalid = 0;
    SquareArray<int, MAX_PLACE_NEAR_DIST> dist(-1);
    queue<coord_def> q;

    q.push(c);
    dist(coord_def()) = 0;
    while (!q.empty())
    {
        coord_def p = q.front();
        q.pop();
        if (dist(p - c) >= last_dist && nvalid)
        {
            bool moved_to_pos = move_to_pos(place);
            ASSERT(moved_to_pos);
            // can't apply location effects here, since the monster might not
            // be on the level yet, which interacts poorly with e.g. shafts
            return true;
        }
        else if (dist(p - c) >= MAX_PLACE_NEAR_DIST)
            break;

        for (adjacent_iterator ai(p); ai; ++ai)
        {
            if (dist(*ai - c) > -1)
                continue;
            dist(*ai - c) = last_dist = dist(p - c) + 1;

            if (!monster_habitable_grid(this, grd(*ai)))
                continue;

            q.push(*ai);
            check_set_valid_home(*ai, place, nvalid);
        }
    }

    return false;
}

bool monster::find_home_near_player()
{
    return find_home_near_place(you.pos());
}

bool monster::find_home_anywhere()
{
    coord_def place(-1, -1);
    int nvalid = 0;
    for (int tries = 0; tries < 600; ++tries)
        if (check_set_valid_home(random_in_bounds(), place, nvalid))
        {
            bool moved_to_pos = move_to_pos(place);
            ASSERT(moved_to_pos);
            // can't apply location effects here, since the monster might not
            // be on the level yet, which interacts poorly with e.g. shafts
            return true;
        }
    return false;
}

bool monster::find_place_to_live(bool near_player)
{
    return near_player && find_home_near_player()
           || find_home_anywhere();
}

void monster::destroy_inventory()
{
    for (mon_inv_iterator ii(*this); ii; ++ii)
        destroy_item(ii->index());
}

bool monster::is_travelling() const
{
    return !travel_path.empty();
}

bool monster::is_patrolling() const
{
    return !patrol_point.origin();
}

bool monster::needs_abyss_transit() const
{
    return (mons_is_unique(type)
            || (flags & MF_BANISHED)
            || get_experience_level() > 8 + random2(25)
            && mons_can_use_stairs(*this))
        && !is_summoned()
        && !mons_is_conjured(type);
}

void monster::set_transit(const level_id &dest)
{
    add_monster_to_transit(dest, *this);
    if (you.can_see(*this))
        remove_unique_annotation(this);
}

void monster::load_ghost_spells()
{
    if (!ghost.get())
    {
        spells.clear();
        return;
    }

    spells = ghost->spells;

#ifdef DEBUG_DIAGNOSTICS
    dprf(DIAG_MONPLACE, "Ghost spells:");
    for (unsigned int i = 0; i < spells.size(); i++)
    {
        dprf(DIAG_MONPLACE, "Spell #%d: %d (%s)",
             i, spells[i].spell, spell_title(spells[i].spell));
    }
#endif
}

bool monster::has_hydra_multi_attack() const
{
    return mons_genus(mons_base_type(*this)) == MONS_HYDRA
        || mons_species(true) == MONS_SERPENT_OF_HELL;
}

int monster::heads() const
{
    if (has_hydra_multi_attack())
        return num_heads;
    else if (mons_shouts(mons_species(true)) == S_SHOUT2)
        return 2;
    // There are lots of things with more or fewer heads, but the return value
    // here doesn't actually matter for non-hydra-type monsters.
    else
        return 1;
}

bool monster::has_multitargeting() const
{
    return has_hydra_multi_attack() && !mons_is_zombified(*this);
}

bool monster::is_priest() const
{
    return search_slots([] (const mon_spell_slot& slot)
                        { return bool(slot.flags & MON_SPELL_PRIEST); });
}

bool monster::is_fighter() const
{
    return bool(flags & MF_FIGHTER);
}

bool monster::is_archer() const
{
    return bool(flags & MF_ARCHER);
}

bool monster::is_actual_spellcaster() const
{
    return search_slots([] (const mon_spell_slot& slot)
                        { return bool(slot.flags & MON_SPELL_WIZARD); } );
}

bool monster::is_shapeshifter() const
{
    return has_ench(ENCH_GLOWING_SHAPESHIFTER, ENCH_SHAPESHIFTER);
}

void monster::scale_hp(int num, int den)
{
    // Without the +1, we lose maxhp on every berserk (the only use) if the
    // maxhp is odd. This version does preserve the value correctly, but only
    // if it is first inflated then deflated.
    hit_points     = (hit_points * num + 1) / den;
    max_hit_points = (max_hit_points * num + 1) / den;

    if (hit_points < 1)
        hit_points = 1;
    if (max_hit_points < 1)
        max_hit_points = 1;
    if (hit_points > max_hit_points)
        hit_points = max_hit_points;
}

kill_category monster::kill_alignment() const
{
    if (mid == MID_YOU_FAULTLESS)
        return KC_YOU;
    return friendly() ? KC_FRIENDLY : KC_OTHER;
}

bool monster::sicken(int amount)
{
    if (res_rotting() || (amount /= 2) < 1)
        return false;

    if (!has_ench(ENCH_SICK) && you.can_see(*this))
    {
        // Yes, could be confused with poisoning.
        mprf("%s looks sick.", name(DESC_THE).c_str());
    }

    add_ench(mon_enchant(ENCH_SICK, 0, 0, amount * BASELINE_DELAY));

    return true;
}

// Recalculate movement speed.
void monster::calc_speed()
{
    speed = mons_base_speed(*this);

    if (has_ench(ENCH_BERSERK))
        speed = berserk_mul(speed);
    else if (has_ench(ENCH_HASTE))
        speed = haste_mul(speed);
    if (has_ench(ENCH_SLOW))
        speed = haste_div(speed);
}

// Check speed and speed_increment sanity.
void monster::check_speed()
{
    // FIXME: If speed is borked, recalculate. Need to figure out how
    // speed is getting borked.
    if (speed < 0 || speed > 130)
    {
        dprf("Bad speed: %s, spd: %d, spi: %d, hd: %d, ench: %s",
             name(DESC_PLAIN).c_str(),
             speed, speed_increment, get_hit_dice(),
             describe_enchantments().c_str());

        calc_speed();

        dprf("Fixed speed for %s to %d", name(DESC_PLAIN).c_str(), speed);
    }

    if (speed_increment < 0)
        speed_increment = 0;

    if (speed_increment > 200)
    {
        dprf("Clamping speed increment on %s: %d",
             name(DESC_PLAIN).c_str(), speed_increment);

        speed_increment = 140;
    }
}

actor *monster::get_foe() const
{
    if (foe == MHITNOT)
        return nullptr;
    else if (foe == MHITYOU)
        return friendly() ? nullptr : &you;

    // Must be a monster!
    monster* my_foe = &menv[foe];
    return my_foe->alive()? my_foe : nullptr;
}

int monster::foe_distance() const
{
    const actor *afoe = get_foe();
    return afoe ? pos().distance_from(afoe->pos())
                : INFINITE_DISTANCE;
}

bool monster::can_go_frenzy() const
{
    if (mons_is_tentacle_or_tentacle_segment(type))
        return false;

    // Brainless natural monsters can still be berserked/frenzied.
    // This could maybe all be replaced by mons_is_object()?
    if (mons_intel(*this) == I_BRAINLESS && !(holiness() & MH_NATURAL))
        return false;

    if (paralysed() || petrified() || petrifying() || asleep())
        return false;

    if (berserk_or_insane() || has_ench(ENCH_FATIGUE))
        return false;

    // If we have no melee attack, going berserk is pointless.
    if (!mons_has_attacks(*this))
        return false;

    return true;
}

bool monster::can_go_berserk() const
{
    return bool(holiness() & MH_NATURAL) && can_go_frenzy();
}

bool monster::berserk() const
{
    return has_ench(ENCH_BERSERK);
}

// XXX: this function could use a better name
bool monster::berserk_or_insane() const
{
    return berserk() || has_ench(ENCH_INSANE);
}

bool monster::needs_berserk(bool check_spells, bool ignore_distance) const
{
    if (!can_go_berserk())
        return false;

    if (has_ench(ENCH_HASTE) || has_ench(ENCH_TP))
        return false;

    if (!ignore_distance && foe_distance() > 3)
        return false;

    if (check_spells)
    {
        for (const mon_spell_slot &slot : spells)
        {
            // Don't count natural abilities for this purpose.
            if (slot.flags & MON_SPELL_NATURAL)
                continue;

            const int spell = slot.spell;
            if (spell != SPELL_BERSERKER_RAGE)
                return false;
        }
    }

    return true;
}

/**
 * Can this monster see invisible creatures?
 *
 * @param calc_unid     Should effects the player doesn't know about be
 *                      considered?
 * @return              Whether the monster can see invisible things.
 */
bool monster::can_see_invisible(bool calc_unid) const
{
    if (mons_is_ghost_demon(type))
        return ghost->see_invis;
    else if (mons_class_sees_invis(type, base_monster))
        return true;
    else if (has_facet(BF_WEIRD))
        return true;

    if (!calc_unid)
        return false;

    if (scan_artefacts(ARTP_SEE_INVISIBLE) > 0)
        return true;
    else if (wearing(EQ_RINGS, RING_SEE_INVISIBLE))
        return true;
    else if (wearing_ego(EQ_ALL_ARMOUR, SPARM_SEE_INVISIBLE))
        return true;

    return false;
}

bool monster::invisible() const
{
    return has_ench(ENCH_INVIS) && !backlit();
}

bool monster::visible_to(const actor *looker) const
{
    bool blind = looker->is_monster()
                 && looker->as_monster()->has_ench(ENCH_BLIND);

    bool vis = looker->is_player() && friendly()
               || (!blind && (!invisible() || looker->can_see_invisible()));

    return vis && (this == looker || !submerged());
}

bool monster::near_foe() const
{
    const actor *afoe = get_foe();
    return afoe && see_cell_no_trans(afoe->pos())
           && summon_can_attack(this, afoe);
}

/**
 * Can the monster be mutated?
 *
 * Nonliving (e.g. statue) monsters & the undead are safe, as are a very few
 * other weird types of monsters.
 *
 * @return Whether the monster can be mutated in any way.
 */
bool monster::can_mutate() const
{
    if (mons_is_tentacle_or_tentacle_segment(type))
        return false;

    // too weird
    if (type == MONS_CHAOS_SPAWN)
        return false;

    // Abominations re-randomize their tile when mutated. They do not gain the
    // malmutate status or experience any other non-cosmetic effect.
    if (type == MONS_ABOMINATION_SMALL || type == MONS_ABOMINATION_LARGE)
        return true;

    const mon_holy_type holi = holiness();

    return !(holi & (MH_UNDEAD | MH_NONLIVING));
}

bool monster::can_safely_mutate(bool temp) const
{
    return can_mutate();
}

bool monster::can_polymorph() const
{
    // can't mutate but can be poly'd
    if (type == MONS_CHAOS_SPAWN)
        return true;

    // Abominations re-randomize their tile when mutated, so can_mutate returns
    // true for them. Like all undead, they can't be polymorphed.
    if (type == MONS_ABOMINATION_SMALL || type == MONS_ABOMINATION_LARGE)
        return false;

    return can_mutate();
}

bool monster::can_bleed(bool /*allow_tran*/) const
{
    return mons_has_blood(type);
}

bool monster::is_stationary() const
{
    return mons_class_is_stationary(type);
}

/**
 * Malmutate the monster.
 *
 * Gives a temporary 'wretched' effect, generally. Some monsters have special
 * interactions.
 *
 * @return Whether the monster was mutated in any way.
 */
bool monster::malmutate(const string &/*reason*/)
{
    if (!can_mutate())
        return false;

    // Abominations re-randomize their tile when mutated. They do not gain the
    // malmutate status or experience any other non-cosmetic effect.
    if (type == MONS_ABOMINATION_SMALL || type == MONS_ABOMINATION_LARGE)
    {
#ifdef USE_TILE
        props[TILE_NUM_KEY].get_short() = ui_random(256);
#endif
        return true;
    }

    // Ugly things merely change colour.
    if (type == MONS_UGLY_THING || type == MONS_VERY_UGLY_THING)
    {
        ugly_thing_mutate(*this);
        return true;
    }

    simple_monster_message(*this, " twists and deforms.");
    add_ench(mon_enchant(ENCH_WRETCHED, 1));
    return true;
}

/**
 * Corrupt the monster's body.
 *
 * Analogous to effects that give the player temp mutations, like wretched star
 * pulses & demonspawn corruptors. Currently identical to malmutate. (Writing
 * this function anyway, since they probably shouldn't be identical.)
 *
 * XXX: adjust duration to differentiate? (make malmut's duration longer, or
 * corrupt's shorter?)
 */
void monster::corrupt()
{
    malmutate("");
}

bool monster::polymorph(int pow)
{
    if (!can_polymorph())
        return false;

    // Polymorphing a (very) ugly thing will mutate it into a different
    // (very) ugly thing.
    if (type == MONS_UGLY_THING || type == MONS_VERY_UGLY_THING)
    {
        ugly_thing_mutate(*this);
        return true;
    }

    // Polymorphing a shapeshifter will make it revert to its original
    // form.
    if (has_ench(ENCH_GLOWING_SHAPESHIFTER))
        return monster_polymorph(this, MONS_GLOWING_SHAPESHIFTER);
    if (has_ench(ENCH_SHAPESHIFTER))
        return monster_polymorph(this, MONS_SHAPESHIFTER);

    // Polymorphing a slime creature will usually split it first
    // and polymorph each part separately.
    if (type == MONS_SLIME_CREATURE)
    {
        slime_creature_polymorph(*this);
        return true;
    }

    return monster_polymorph(this, RANDOM_MONSTER);
}

static bool _mons_is_icy(int mc)
{
    return mc == MONS_ICE_BEAST
           || mc == MONS_SIMULACRUM
           || mc == MONS_ICE_STATUE
           || mc == MONS_BLOCK_OF_ICE;
}

bool monster::is_icy() const
{
    return _mons_is_icy(type);
}

static bool _mons_is_fiery(int mc)
{
    return mc == MONS_FIRE_VORTEX
           || mc == MONS_FIRE_ELEMENTAL
           || mc == MONS_EFREET
           || mc == MONS_AZRAEL
           || mc == MONS_LAVA_SNAKE
           || mc == MONS_SALAMANDER
           || mc == MONS_SALAMANDER_MYSTIC
           || mc == MONS_MOLTEN_GARGOYLE
           || mc == MONS_ORB_OF_FIRE;
}

bool monster::is_fiery() const
{
    return _mons_is_fiery(type);
}

static bool _mons_is_skeletal(int mc)
{
    return mc == MONS_SKELETON
           || mc == MONS_BONE_DRAGON
           || mc == MONS_SKELETAL_WARRIOR
           || mc == MONS_ANCIENT_CHAMPION
           || mc == MONS_REVENANT
           || mc == MONS_FLYING_SKULL
           || mc == MONS_CURSE_SKULL
           || mc == MONS_MURRAY;
}

bool monster::is_skeletal() const
{
    return _mons_is_skeletal(type);
}

/**
 * Does this monster have spines?
 *
 * (If so, it may do damage when attacked in melee, and has rConstrict (!?)
 *
 * @return  Whether this monster has spines.
 */
bool monster::is_spiny() const
{
    return mons_class_flag(mons_is_job(type) ? base_monster : type,
                           M_SPINY);
}

bool monster::has_action_energy() const
{
    return speed_increment >= 80;
}

void monster::check_redraw(const coord_def &old, bool clear_tiles) const
{
    if (!crawl_state.io_inited)
        return;

    const bool see_new = you.see_cell(pos());
    const bool see_old = you.see_cell(old);
    if ((see_new || see_old) && !view_update())
    {
        if (see_new)
            view_update_at(pos());

        // Don't leave a trail if we can see the monster move in.
        if (see_old || (pos() - old).rdist() <= 1)
        {
            view_update_at(old);
#ifdef USE_TILE
            if (clear_tiles && !see_old)
                tile_reset_fg(old);
#endif
        }
        update_screen();
    }
}

void monster::apply_location_effects(const coord_def &oldpos,
                                     killer_type killer,
                                     int killernum)
{
    if (oldpos != pos())
        dungeon_events.fire_position_event(DET_MONSTER_MOVED, pos());

    if (alive()
        && (mons_habitat(*this) == HT_WATER || mons_habitat(*this) == HT_LAVA)
        && !monster_habitable_grid(this, grd(pos()))
        && !has_ench(ENCH_AQUATIC_LAND))
    {
        // Elemental wellsprings always have water beneath them
        if (type == MONS_ELEMENTAL_WELLSPRING)
        {
            temp_change_terrain(pos(), DNGN_SHALLOW_WATER, 100,
                                TERRAIN_CHANGE_FLOOD, this);
        }
        else
            add_ench(ENCH_AQUATIC_LAND);
    }

    if (alive() && has_ench(ENCH_AQUATIC_LAND))
    {
        if (!monster_habitable_grid(this, grd(pos())))
            simple_monster_message(*this, " flops around on dry land!");
        else if (!monster_habitable_grid(this, grd(oldpos)))
        {
            if (you.can_see(*this))
            {
                mprf("%s dives back into the %s!", name(DESC_THE).c_str(),
                                                   feat_type_name(grd(pos())));
            }
            del_ench(ENCH_AQUATIC_LAND);
        }
        // This may have been called via dungeon_terrain_changed instead
        // of by the monster moving move, in that case grd(oldpos) will
        // be the current position that became watery.
        else
            del_ench(ENCH_AQUATIC_LAND);
    }

    // Monsters stepping on traps:
    trap_def* ptrap = trap_at(pos());
    if (ptrap)
        ptrap->trigger(*this);

    if (alive())
        mons_check_pool(this, pos(), killer, killernum);

    if (alive()
        && has_ench(ENCH_SUBMERGED)
        && !monster_can_submerge(this, grd(pos())))
    {
        del_ench(ENCH_SUBMERGED);
    }

    terrain_property_t &prop = env.pgrid(pos());

    if (prop & FPROP_BLOODY)
    {
        monster_type genus = mons_genus(type);

        if (genus == MONS_JELLY || genus == MONS_ELEPHANT_SLUG)
        {
            prop &= ~FPROP_BLOODY;
            if (you.see_cell(pos()) && !visible_to(&you))
            {
                string desc =
                    feature_description_at(pos(), false, DESC_THE, false);
                mprf("The bloodstain on %s disappears!", desc.c_str());
            }
        }
    }
}

void monster::self_destruct()
{
    suicide();
    monster_die(*as_monster(), KILL_MON, mindex());
}

/** A higher-level moving method than moveto().
 *
 *  @param newpos    where to move this monster
 *  @param clear_net whether to clear any trapping nets
 *  @param force     whether to move it even if you're standing there
 *  @returns whether the move took place.
 */
bool monster::move_to_pos(const coord_def &newpos, bool clear_net, bool force)
{
    const actor* a = actor_at(newpos);
    if (a && !(a->is_player() && (fedhas_passthrough(this) || force)))
        return false;

    const int index = mindex();

    // Clear old cell pointer.
    if (in_bounds(pos()) && mgrd(pos()) == index)
        mgrd(pos()) = NON_MONSTER;

    // Set monster x,y to new value.
    moveto(newpos, clear_net);

    // Set new monster grid pointer to this monster.
    mgrd(newpos) = index;

    return true;
}

/** Swap positions with another monster.
 *
 *  move_to_pos can't be used in this case, since it can't move something
 *  to a spot that's occupied. This will abort if either monster can't survive
 *  in the new place.
 *
 *  @param other the monster to swap with
 *  @returns whether they ended up moving.
 */
bool monster::swap_with(monster* other)
{
    const coord_def old_pos = pos();
    const coord_def new_pos = other->pos();

    if (!can_pass_through(new_pos)
        || !other->can_pass_through(old_pos))
    {
        return false;
    }

    if (!monster_habitable_grid(this, grd(new_pos))
         && !can_cling_to(new_pos)
        || !monster_habitable_grid(other, grd(old_pos))
            && !other->can_cling_to(old_pos))
    {
        return false;
    }

    moveto(new_pos);
    other->moveto(old_pos);

    mgrd(old_pos) = other->mindex();
    mgrd(new_pos) = mindex();

    return true;
}

// Returns true if the trap should be revealed to the player.
bool monster::do_shaft()
{
    if (!is_valid_shaft_level())
        return false;

    // Tentacles & player ghosts are immune to shafting
    if (mons_is_tentacle_or_tentacle_segment(type)
        || type == MONS_PLAYER_GHOST)
    {
        return false;
    }

    // Handle instances of do_shaft() being invoked magically when
    // the monster isn't standing over a shaft.
    if (get_trap_type(pos()) != TRAP_SHAFT
        && !feat_is_shaftable(grd(pos())))
    {
        return false;
    }

    level_id lev = shaft_dest(false);

    if (lev == level_id::current())
        return false;

    // If a pacified monster is leaving the level via a shaft trap, and
    // has reached its goal, vaporize it instead of moving it.
    // ditto, non-monsters like battlespheres and prisms.
    if (!pacified() && !mons_is_conjured(type))
        set_transit(lev);

    string msg = make_stringf(" %s a shaft!",
                              !ground_level() ? "is sucked into"
                                              : "falls through");

    const bool reveal = simple_monster_message(*this, msg.c_str());

    place_cloud(CLOUD_DUST, pos(), 1 + random2(3), this);

    // Monster is no longer on this level.
    destroy_inventory();
    monster_cleanup(this);

    return reveal;
}

void monster::put_to_sleep(actor *attacker, int strength, bool hibernate)
{
    const bool valid_target = hibernate ? can_hibernate() : can_sleep();
    if (!valid_target)
        return;

    stop_constricting_all();
    behaviour = BEH_SLEEP;
    flags |= MF_JUST_SLEPT;
    if (hibernate)
        add_ench(ENCH_SLEEP_WARY);
}

void monster::weaken(actor *attacker, int pow)
{
    if (!has_ench(ENCH_WEAK))
        simple_monster_message(*this, " looks weaker.");

    add_ench(mon_enchant(ENCH_WEAK, 1, attacker,
                         (pow + random2(pow + 3)) * BASELINE_DELAY));
}

void monster::check_awaken(int)
{
    // XXX
}

int monster::beam_resists(bolt &beam, int hurted, bool doEffects, string source)
{
    return mons_adjust_flavoured(this, beam, hurted, doEffects);
}

const monsterentry *monster::find_monsterentry() const
{
    return (type == MONS_NO_MONSTER || type == MONS_PROGRAM_BUG) ? nullptr
                                                    : get_monster_data(type);
}

int monster::action_energy(energy_use_type et) const
{
    if (!find_monsterentry())
        return 10;

    const mon_energy_usage &mu = mons_energy(*this);
    int move_cost = 0;
    switch (et)
    {
    case EUT_MOVE:    move_cost = mu.move; break;
    // Amphibious monster speed boni are now dealt with using SWIM_ENERGY,
    // rather than here.
    case EUT_SWIM:    move_cost = mu.swim; break;
    case EUT_MISSILE: return mu.missile;
    case EUT_ITEM:    return mu.item;
    case EUT_SPECIAL: return mu.special;
    case EUT_SPELL:   return mu.spell;
    case EUT_ATTACK:  return mu.attack;
    case EUT_PICKUP:  return mu.pickup_percent;
    }

    if (has_ench(ENCH_SWIFT))
        move_cost -= 3;

    if (wearing_ego(EQ_ALL_ARMOUR, SPARM_PONDEROUSNESS))
        move_cost += 1;

    if (run())
        move_cost -= 1;

    // Shadows move more quickly when blended with the darkness.
    // Change _monster_stat_description in describe.cc if you change this.
    if (type == MONS_SHADOW && invisible())
        move_cost -= 3;

    // Floundering monsters get the same penalty as the player, except that
    // player get penalty on entering water, while monster get the penalty
    // when leaving it.
    if (floundering())
        move_cost += 3 + random2(8);

    // If the monster cast it, it has more control and is there not
    // as slow as when the player casts it.
    if (has_ench(ENCH_LIQUEFYING))
        move_cost -= 2;

    // Never reduce the cost to zero
    return max(move_cost, 1);
}

void monster::lose_energy(energy_use_type et, int div, int mult)
{
    int energy_loss  = div_round_up(mult * action_energy(et), div);
    if (has_ench(ENCH_PETRIFYING))
    {
        energy_loss *= 3;
        energy_loss /= 2;
    }

    if ((et == EUT_MOVE || et == EUT_SWIM) && has_ench(ENCH_GRASPING_ROOTS))
        energy_loss += 5;

    if ((et == EUT_MOVE || et == EUT_SWIM) && has_ench(ENCH_FROZEN))
        energy_loss += 4;

    // Randomize interval between servitor spellcasts
    if ((et == EUT_SPELL && type == MONS_SPELLFORGED_SERVITOR))
        energy_loss += random2(16);

    // Randomize movement cost slightly, to make it less predictable,
    // and make pillar-dancing not entirely safe.
    // No randomization for allies following you to avoid traffic jam
    if ((et == EUT_MOVE || et == EUT_SWIM) && (!friendly() || foe != MHITYOU))
        energy_loss += random2(3) - 1;

    speed_increment -= energy_loss;
}

void monster::gain_energy(energy_use_type et, int div, int mult)
{
    int energy_gain  = div_round_up(mult * action_energy(et), div);
    if (has_ench(ENCH_PETRIFYING))
    {
        energy_gain *= 2;
        energy_gain /= 3;
    }

    speed_increment += energy_gain;
}

/**
 * Can this monster ever drink any potions at all?
 *
 * @return  Whether the monster can drink potions (not a statue, mummy, etc)
 */
bool monster::can_drink() const
{
    if (mons_class_is_stationary(type))
        return false;

    if (mons_itemuse(*this) < MONUSE_STARTING_EQUIPMENT)
        return false;

    // These monsters cannot drink.
    if (is_skeletal() || is_insubstantial()
        || mons_species() == MONS_LICH || mons_genus(type) == MONS_MUMMY
        || mons_species() == MONS_WIGHT || type == MONS_GASTRONOK)
    {
        return false;
    }

    return true;
}

bool monster::can_drink_potion(potion_type ptype) const
{
    if (!can_drink())
        return false;

    switch (ptype)
    {
        case POT_CURING:
        case POT_HEAL_WOUNDS:
            return !(holiness() & (MH_NONLIVING | MH_PLANT));
        case POT_BLOOD:
#if TAG_MAJOR_VERSION == 34
        case POT_BLOOD_COAGULATED:
#endif
            return mons_species() == MONS_VAMPIRE;
        case POT_BERSERK_RAGE:
            return can_go_berserk();
        case POT_HASTE:
        case POT_MIGHT:
        case POT_AGILITY:
        case POT_INVISIBILITY:
        case POT_RESISTANCE:
            // If there are any item using monsters that are permanently
            // invisible, this might have to be restricted.
            return true;
        default:
            break;
    }

    return false;
}

bool monster::should_drink_potion(potion_type ptype) const
{
    switch (ptype)
    {
    case POT_CURING:
        return hit_points <= max_hit_points / 2
               || has_ench(ENCH_POISON)
               || has_ench(ENCH_SICK)
               || has_ench(ENCH_CONFUSION);
    case POT_HEAL_WOUNDS:
        return hit_points <= max_hit_points / 2;
    case POT_BLOOD:
#if TAG_MAJOR_VERSION == 34
    case POT_BLOOD_COAGULATED:
#endif
        return hit_points <= max_hit_points / 2;
    case POT_BERSERK_RAGE:
        // this implies !berserk()
        return !has_ench(ENCH_MIGHT) && !has_ench(ENCH_HASTE)
               && needs_berserk();
    case POT_HASTE:
        return !has_ench(ENCH_HASTE);
    case POT_MIGHT:
        return !has_ench(ENCH_MIGHT) && foe_distance() <= 2;
    case POT_AGILITY:
        return !has_ench(ENCH_AGILE);
    case POT_RESISTANCE:
        return !has_ench(ENCH_RESISTANCE);
    case POT_INVISIBILITY:
        // We're being nice: friendlies won't go invisible if the player
        // won't be able to see them.
        return !has_ench(ENCH_INVIS)
               && (you.can_see_invisible(false) || !friendly());
    default:
        break;
    }

    return false;
}

// Return true if the potion should be identified.
bool monster::drink_potion_effect(potion_type pot_eff, bool card)
{
    if (!card)
        simple_monster_message(*this, " drinks a potion.");

    switch (pot_eff)
    {
    case POT_CURING:
    {
        if (heal(5 + random2(7)))
            simple_monster_message(*this, " is healed!");

        static const enchant_type cured_enchants[] =
        {
            ENCH_POISON, ENCH_SICK, ENCH_CONFUSION
        };

        for (enchant_type cured : cured_enchants)
            del_ench(cured);
    }
    break;

    case POT_HEAL_WOUNDS:
        if (heal(10 + random2avg(28, 3)))
            simple_monster_message(*this, " is healed!");
        break;

    case POT_BLOOD:
#if TAG_MAJOR_VERSION == 34
    case POT_BLOOD_COAGULATED:
#endif
        if (mons_species() == MONS_VAMPIRE)
        {
            heal(10 + random2avg(28, 3));
            simple_monster_message(*this, " is healed!");
        }
        break;

    case POT_BERSERK_RAGE:
        enchant_actor_with_flavour(this, this, BEAM_BERSERK);
        break;

    case POT_HASTE:
        enchant_actor_with_flavour(this, this, BEAM_HASTE);
        break;

    case POT_MIGHT:
        enchant_actor_with_flavour(this, this, BEAM_MIGHT);
        break;

    case POT_INVISIBILITY:
        enchant_actor_with_flavour(this, this, BEAM_INVISIBILITY);
        break;

    case POT_AGILITY:
        enchant_actor_with_flavour(this, this, BEAM_AGILITY);
        break;

    case POT_RESISTANCE:
        enchant_actor_with_flavour(this, this, BEAM_RESISTANCE);
        break;

    default:
        return false;
    }

    return !card;
}

bool monster::can_evoke_jewellery(jewellery_type jtype) const
{
    if (mons_class_is_stationary(type))
        return false;

    if (mons_itemuse(*this) < MONUSE_STARTING_EQUIPMENT)
        return false;

    switch (jtype)
    {
        case AMU_RAGE:
            return can_go_berserk();
        default:
            break;
    }

    return false;
}

bool monster::should_evoke_jewellery(jewellery_type jtype) const
{
    switch (jtype)
    {
    case AMU_RAGE:
        // this implies !berserk()
        return !has_ench(ENCH_MIGHT) && !has_ench(ENCH_HASTE)
               && needs_berserk();
    default:
        break;
    }

    return false;
}

// Return true if the jewellery should be identified.
bool monster::evoke_jewellery_effect(jewellery_type jtype)
{
    // XXX: this is mostly to prevent a funny message order:
    // "$foo evokes its amulet. $foo wields a great mace. $foo goes berserk!"
    if (jtype == AMU_RAGE)
        wield_melee_weapon();

    mprf("%s evokes %s %s.", name(DESC_THE).c_str(),
         pronoun(PRONOUN_POSSESSIVE).c_str(),
         jewellery_is_amulet(jtype) ? "amulet" : "ring");

    switch (jtype)
    {
    case AMU_RAGE:
        enchant_actor_with_flavour(this, this, BEAM_BERSERK);
        break;

    default:
        return false;
    }

    return true;
}

void monster::react_to_damage(const actor *oppressor, int damage,
                               beam_type flavour)
{
    // Don't discharge on small amounts of damage (this helps avoid
    // continuously shocking when poisoned or sticky flamed)
    // XXX: this might not be necessary anymore?
    if (type == MONS_SHOCK_SERPENT && damage > 4 && oppressor)
    {
        const int pow = div_rand_round(min(damage, hit_points + damage), 9);
        if (pow)
        {
            // we intentionally allow harming the oppressor in this case,
            // so need to cast off its constness
            shock_serpent_discharge_fineff::schedule(this,
                                                     const_cast<actor&>(*oppressor),
                                                     pos(), pow);
        }
    }

    // The (real) royal jelly objects to taking damage and will SULK. :-)
    if (type == MONS_ROYAL_JELLY && !is_summoned())
        trj_spawn_fineff::schedule(oppressor, this, pos(), damage);

    // Damage sharing from the spectral weapon to its owner
    // The damage shared should not be directly lethal, though like the
    // pain spell, it can leave the player at a very dangerous 1hp.
    // XXX: This makes a lot of messages, especially when the spectral weapon
    //      is hit by a monster with multiple attacks and is frozen, burned, etc.
    if (type == MONS_SPECTRAL_WEAPON && oppressor)
    {
        // The owner should not be able to damage itself
        // XXX: the mid check here is intended to treat the player's shadow
        // mimic as the player itself, i.e. the weapon won't share damage
        // the shadow mimic inflicts on it (this causes a crash).
        actor *owner = actor_by_mid(summoner);
        if (owner && owner != oppressor && oppressor->mid != summoner)
        {
            int shared_damage = damage / 2;
            if (shared_damage > 0)
            {
                if (owner->is_player())
                    mpr("Your spectral weapon shares its damage with you!");
                else if (owner->alive() && you.can_see(*owner))
                {
                    string buf = " shares ";
                    buf += owner->pronoun(PRONOUN_POSSESSIVE);
                    buf += " spectral weapon's damage!";
                    simple_monster_message(*owner->as_monster(), buf.c_str());
                }

                // Share damage using a fineff, so that it's non-fatal
                // regardless of processing order in an AoE attack.
                deferred_damage_fineff::schedule(oppressor, owner,
                                                 shared_damage, false, false);
            }
        }
    }
    else if (mons_is_tentacle_or_tentacle_segment(type)
             && !mons_is_solo_tentacle(type)
             && flavour != BEAM_TORMENT_DAMAGE
             && monster_by_mid(tentacle_connect)
             && monster_by_mid(tentacle_connect)->is_parent_monster_of(this))
    {
        deferred_damage_fineff::schedule(oppressor,
                                         monster_by_mid(tentacle_connect),
                                         damage, false);
    }

    if (!alive())
        return;


    if (mons_species() == MONS_BUSH
        && res_fire() < 0 && flavour == BEAM_FIRE
        && damage > 8 && x_chance_in_y(damage, 20))
    {
        place_cloud(CLOUD_FIRE, pos(), 20 + random2(15), oppressor, 5);
    }
    else if (type == MONS_SPRIGGAN_RIDER)
    {
        if (hit_points + damage > max_hit_points / 2)
            damage = max_hit_points / 2 - hit_points;
        if (damage > 0 && x_chance_in_y(damage, damage + hit_points))
        {
            bool fly_died = coinflip();
            int old_hp                = hit_points;
            auto old_flags            = flags;
            mon_enchant_list old_ench = enchantments;
            FixedBitVector<NUM_ENCHANTMENTS> old_ench_cache = ench_cache;
            int8_t old_ench_countdown = ench_countdown;
            string old_name = mname;

            if (!fly_died)
                monster_drop_things(this, mons_aligned(oppressor, &you));

            type = fly_died ? MONS_SPRIGGAN : MONS_HORNET;
            define_monster(*this);
            hit_points = min(old_hp, hit_points);
            flags          = old_flags;
            enchantments   = old_ench;
            ench_cache     = old_ench_cache;
            ench_countdown = old_ench_countdown;
            // Keep the rider's name, if it had one (Mercenary card).
            if (!old_name.empty())
                mname = old_name;

            mounted_kill(this, fly_died ? MONS_HORNET : MONS_SPRIGGAN,
                !oppressor ? KILL_MISC
                : (oppressor->is_player())
                  ? KILL_YOU : KILL_MON,
                (oppressor && oppressor->is_monster())
                  ? oppressor->mindex() : NON_MONSTER);

            // Now clear the name, if the rider just died.
            if (!fly_died)
                mname.clear();

            if (fly_died && !is_habitable(pos()))
            {
                hit_points = 0;
                if (observable())
                {
                    mprf("As %s mount dies, %s plunges down into %s!",
                         pronoun(PRONOUN_POSSESSIVE).c_str(),
                         name(DESC_THE).c_str(),
                         grd(pos()) == DNGN_LAVA ?
                             "lava and is incinerated" :
                             "deep water and drowns");
                }
            }
            else if (fly_died && observable())
            {
                mprf("%s falls from %s now dead mount.",
                     name(DESC_THE).c_str(),
                     pronoun(PRONOUN_POSSESSIVE).c_str());
            }
        }
    }
    else if (type == MONS_STARCURSED_MASS)
        starcursed_merge_fineff::schedule(this);
    else if (type == MONS_RAKSHASA && !has_ench(ENCH_PHANTOM_MIRROR)
             && hit_points < max_hit_points / 2
             && hit_points - damage > 0)
    {
        if (!props.exists("emergency_clone"))
        {
            rakshasa_clone_fineff::schedule(this, pos());
            props["emergency_clone"].get_bool() = true;
        }
    }

    else if (type == MONS_BAI_SUZHEN && hit_points < max_hit_points / 2
                                     && hit_points - damage > 0)
    {
        int old_hp                = hit_points;
        auto old_flags            = flags;
        mon_enchant_list old_ench = enchantments;
        FixedBitVector<NUM_ENCHANTMENTS> old_ench_cache = ench_cache;
        int8_t old_ench_countdown = ench_countdown;
        string old_name = mname;

        monster_drop_things(this, mons_aligned(oppressor, &you));

        type = MONS_BAI_SUZHEN_DRAGON;
        define_monster(*this);
        hit_points = min(old_hp, hit_points);
        flags          = old_flags;
        enchantments   = old_ench;
        ench_cache     = old_ench_cache;
        ench_countdown = old_ench_countdown;

        if (observable())
        {
            mprf(MSGCH_WARN, "%s roars in fury and transforms into a fierce dragon!",
                 name(DESC_THE).c_str());
        }

        add_ench(ENCH_RING_OF_THUNDER);
    }

    if (alive())
        maybe_degrade_bone_armour();
}

reach_type monster::reach_range() const
{
    const mon_attack_def attk(mons_attack_spec(*this, 0));
    if ((attk.flavour == AF_REACH || attk.flavour == AF_REACH_STING)
        && attk.damage)
    {
        return REACH_TWO;
    }

    const item_def *wpn = primary_weapon();
    if (wpn)
        return weapon_reach(*wpn);
    return REACH_NONE;
}

bool monster::can_cling_to_walls() const
{
    return mons_can_cling_to_walls(*this);
}

void monster::steal_item_from_player()
{
    if (confused())
    {
        string msg = getSpeakString("Maurice confused nonstealing");
        if (!msg.empty() && msg != "__NONE")
        {
            msg = replace_all(msg, "@The_monster@", name(DESC_THE));
            mprf(MSGCH_TALK, "%s", msg.c_str());
        }
        return;
    }
    // Theft isn't very peaceful. More importantly, you would risk losing the
    // item forever when the monster flees the level.
    if (pacified())
        return;

    mon_inv_type mslot = NUM_MONSTER_SLOTS;
    int steal_what  = -1;
    int total_value = 0;
    for (int m = 0; m < ENDOFPACK; ++m)
    {
        if (!you.inv[m].defined())
            continue;

        // Cannot unequip player.
        // TODO: Allow stealing of the wielded weapon?
        //       Needs to be unwielded properly and should never lead to
        //       fatal stat loss.
        // 1KB: I'd say no, weapon is being held, it's different from pulling
        //      a wand from your pocket.
        if (item_is_equipped(you.inv[m]))
            continue;

        mon_inv_type monslot = item_to_mslot(you.inv[m]);
        if (monslot == NUM_MONSTER_SLOTS)
            continue;

        // Only try to steal stuff we can still store somewhere.
        if (inv[monslot] != NON_ITEM)
        {
            if (monslot == MSLOT_WEAPON
                && inv[MSLOT_ALT_WEAPON] == NON_ITEM)
            {
                monslot = MSLOT_ALT_WEAPON;
            }
            else
                continue;
        }

        // Candidate for stealing.
        const int value = item_value(you.inv[m], true);
        total_value += value;

        if (x_chance_in_y(value, total_value))
        {
            steal_what = m;
            mslot      = monslot;
        }
    }

    if (steal_what == -1 || you.gold > 0 && one_chance_in(10))
    {
        // Found no item worth stealing, try gold.
        if (you.gold == 0)
        {
            if (silenced(pos()))
                return;

            string complaint = getSpeakString("Maurice nonstealing");
            if (!complaint.empty())
            {
                complaint = replace_all(complaint, "@The_monster@",
                                        name(DESC_THE));
                mprf(MSGCH_TALK, "%s", complaint.c_str());
            }

            bolt beem;
            beem.source    = pos();
            beem.target    = pos();
            beem.source_id = mid;

            // Try to teleport away.
            if (no_tele())
                return;

            if (has_ench(ENCH_TP))
            {
                mons_cast_noise(this, beem, SPELL_BLINK, MON_SPELL_WIZARD);
                // this can kill us, delay the call
                blink_fineff::schedule(this);
            }
            else
                mons_cast(this, beem, SPELL_TELEPORT_SELF, MON_SPELL_WIZARD);

            return;
        }

        const int stolen_amount = min(20 + random2(800), you.gold);
        if (inv[MSLOT_GOLD] != NON_ITEM)
        {
            // If Maurice already's got some gold, simply increase the amount.
            mitm[inv[MSLOT_GOLD]].quantity += stolen_amount;
        }
        else
        {
            // Else create a new item for this pile of gold.
            const int idx = items(false, OBJ_GOLD, OBJ_RANDOM, 0);
            if (idx == NON_ITEM)
                return;

            item_def &new_item = mitm[idx];
            new_item.base_type = OBJ_GOLD;
            new_item.sub_type  = 0;
            new_item.plus      = 0;
            new_item.plus2     = 0;
            new_item.special   = 0;
            new_item.flags     = 0;
            new_item.link      = NON_ITEM;
            new_item.quantity  = stolen_amount;
            new_item.pos.reset();
            item_colour(new_item);

            unlink_item(idx);

            inv[MSLOT_GOLD] = idx;
            new_item.set_holding_monster(*this);
        }
        mitm[inv[MSLOT_GOLD]].flags |= ISFLAG_THROWN;
        mprf("%s steals %s your gold!",
             name(DESC_THE).c_str(),
             stolen_amount == you.gold ? "all" : "some of");

        you.attribute[ATTR_GOLD_FOUND] -= stolen_amount;

        you.del_gold(stolen_amount);
        return;
    }

    ASSERT(steal_what != -1);
    ASSERT(mslot != NUM_MONSTER_SLOTS);
    ASSERT(inv[mslot] == NON_ITEM);

    const int orig_qty = you.inv[steal_what].quantity;

    mprf("%s steals %s!",
         name(DESC_THE).c_str(),
         you.inv[steal_what].name(DESC_YOUR).c_str());

    item_def* tmp = take_item(steal_what, mslot);
    if (!tmp)
        return;
    item_def& new_item = *tmp;

    // You'll want to autopickup it after killing Maurice.
    new_item.flags |= ISFLAG_THROWN;

    // Fix up blood/chunk timers.
    if (is_perishable_stack(new_item))
    {
        // Somehow they always steal the freshest blood.
        for (int i = new_item.quantity; i < orig_qty; ++i)
            remove_oldest_perishable_item(new_item);

        // If the whole stack is gone, it doesn't need to be cleaned up.
        if (you.inv[steal_what].defined())
            remove_newest_perishable_item(you.inv[steal_what]);
    }
}

/**
 * "Give" a monster an item from the player's inventory.
 *
 * @param steal_what The slot in your inventory of the item.
 * @param mslot Which mon_inv_type to put the item in
 *
 * @returns new_item the new item, now in the monster's inventory.
 */
item_def* monster::take_item(int steal_what, mon_inv_type mslot)
{
    // Create new item.
    int index = get_mitm_slot(10);
    if (index == NON_ITEM)
        return nullptr;

    item_def &new_item = mitm[index];

    // Copy item.
    new_item = you.inv[steal_what];

    // Drop the item already in the slot (including the shield
    // if it's a two-hander).
    if ((mslot == MSLOT_WEAPON || mslot == MSLOT_ALT_WEAPON)
        && inv[MSLOT_SHIELD] != NON_ITEM
        && hands_reqd(new_item) == HANDS_TWO)
    {
        drop_item(MSLOT_SHIELD, observable());
    }
    if (inv[mslot] != NON_ITEM)
        drop_item(mslot, observable());

    // Set quantity, and set the item as unlinked.
    new_item.quantity -= random2(new_item.quantity);
    new_item.pos.reset();
    new_item.link = NON_ITEM;

    unlink_item(index);
    inv[mslot] = index;
    new_item.set_holding_monster(*this);

    equip(new_item, true);

    // Item is gone from player's inventory.
    dec_inv_item_quantity(steal_what, new_item.quantity);

    return &new_item;
}

/** Disarm this monster, and preferably pull the weapon into your tile.
 *
 *  @returns a pointer to the weapon disarmed, or nullptr if unsuccessful.
 */
item_def* monster::disarm()
{
    item_def *mons_wpn = mslot_item(MSLOT_WEAPON);

    // is it ok to move the weapon into your tile (w/o destroying it?)
    const bool your_tile_ok = !feat_eliminates_items(grd(you.pos()));

    // It's ok to drop the weapon into deep water if it comes out right away,
    // but if the monster is on lava we just have to abort.
    const bool mon_tile_ok = !feat_destroys_items(grd(pos()))
                             && (your_tile_ok
                                 || !feat_eliminates_items(grd(pos())));

    if (!mons_wpn
        || mons_wpn->cursed()
        || mons_class_is_animated_weapon(type)
        || !adjacent(you.pos(), pos())
        || !you.can_see(*this)
        || !mon_tile_ok
        || mons_wpn->flags & ISFLAG_SUMMONED)
    {
        return nullptr;
    }

    drop_item(MSLOT_WEAPON, false);

    // XXX: assumes nothing's re-ordering items - e.g. gozag gold
    if (your_tile_ok)
        move_top_item(pos(), you.pos());

    if (type == MONS_CEREBOV)
        you.props[CEREBOV_DISARMED_KEY] = true;

    return mons_wpn;
}

/**
 * Checks if the monster can pass through webs freely.
 *
 * Currently: spiders (including Arachne), moths, demonic crawlers,
 * ghosts & other incorporeal monsters, and jelly monsters.
 *
 * @return Whether the monster is immune to webs.
 */
bool monster::is_web_immune() const
{
    return mons_genus(type) == MONS_SPIDER
            || type == MONS_ARACHNE
            || mons_genus(type) == MONS_MOTH
            || mons_genus(type) == MONS_DEMONIC_CRAWLER
            || is_insubstantial()
            || mons_genus(type) == MONS_JELLY;
}

// Followers of Yredelemnul and Dithmenos don't have their accuracy
// reduced by umbra.
bool monster::nightvision() const
{
    return god == GOD_YREDELEMNUL
           || god == GOD_DITHMENOS;
}

bool monster::attempt_escape(int attempts)
{
    int attfactor;
    int randfact;

    if (!is_constricted())
        return true;

    escape_attempts += attempts;
    attfactor = 3 * escape_attempts;

    if (constricted_by != MID_PLAYER)
    {
        randfact = roll_dice(1, 5) + 5;
        const monster* themonst = monster_by_mid(constricted_by);
        ASSERT(themonst);
        randfact += roll_dice(1, themonst->get_hit_dice());
    }
    else
        randfact = roll_dice(1, 3 + you.experience_level);

    if (attfactor > randfact)
    {
        stop_being_constricted(true);
        return true;
    }
    else
        return false;
}

/**
 * Does the monster have a free tentacle to constrict something?
 * Currently only used by octopode monsters.
 * @return  True if it can constrict an additional monster, false otherwise.
 */
bool monster::has_usable_tentacle() const
{
    if (mons_genus(type) != MONS_OCTOPODE)
        return false;

    // ignoring monster octopodes with weapons, for now
    return num_constricting() < 8;
}

// Move the monster to the nearest valid space.
bool monster::shove(const char* feat_name)
{
    for (distance_iterator di(pos()); di; ++di)
        if (monster_space_valid(this, *di, false))
        {
            move_to_pos(*di);
            simple_monster_message(*this,
                make_stringf(" is pushed out of the %s.", feat_name).c_str());
            dprf("Moved to (%d, %d).", pos().x, pos().y);

            return true;
        }

    return false;
}

void monster::id_if_worn(mon_inv_type mslot, object_class_type base_type,
                          int sub_type) const
{
    item_def *item = mslot_item(mslot);

    if (item && item->is_type(base_type, sub_type))
        set_ident_type(*item, true);
}

bool monster::check_clarity(bool silent) const
{
    if (!clarity())
        return false;

    return true;
}

bool monster::stasis() const
{
    return mons_genus(type) == MONS_FORMICID
           || type == MONS_PLAYER_GHOST && ghost->species == SP_FORMICID;
}

bool monster::cloud_immune(bool calc_unid, bool items) const
{
    // Cloud Mage is also checked for in (so stay in sync with)
    // monster_info::monster_info(monster_type, monster_type).
    return type == MONS_CLOUD_MAGE || actor::cloud_immune(calc_unid, items);
}

bool monster::is_illusion() const
{
    return type == MONS_PLAYER_ILLUSION
           || has_ench(ENCH_PHANTOM_MIRROR)
           || props.exists(CLONE_SLAVE_KEY);
}

bool monster::is_divine_companion() const
{
    return attitude == ATT_FRIENDLY
           && !is_summoned()
           && (mons_is_god_gift(*this, GOD_BEOGH)
               || mons_is_god_gift(*this, GOD_YREDELEMNUL)
               || mons_is_god_gift(*this, GOD_HEPLIAKLQANA))
           && mons_can_use_stairs(*this);
}

bool monster::is_jumpy() const
{
    return type == MONS_JUMPING_SPIDER;
}

// HD for spellcasting purposes.
// Currently only used for Aura of Brilliance and Hep ancestors.
int monster::spell_hd(spell_type spell) const
{
    int hd = get_hit_dice();
    if (mons_is_hepliaklqana_ancestor(type))
        hd = max(1, hd * 2 / 3);
    if (has_ench(ENCH_IDEALISED))
        hd *= 2;
    if (has_ench(ENCH_EMPOWERED_SPELLS))
        hd += 5;
    return hd;
}

/**
 * For pandemonium lords & monsters with random spellbooks, track which spells
 * the player has seen this monster cast.
 *
 * @param spell     The spell the player just saw the monster cast.
 */
void monster::note_spell_cast(spell_type spell)
{
    const monster_info mi(this);
    if (type != MONS_PANDEMONIUM_LORD && get_spellbooks(mi).size() <= 1)
        return;

    for (int old_spell : props[SEEN_SPELLS_KEY].get_vector())
        if (old_spell == spell)
            return;

    dprf("tracking seen spell %s for %s",
         spell_title(spell), name(DESC_A, true).c_str());
    props[SEEN_SPELLS_KEY].get_vector().push_back(spell);
}

void monster::align_avatars(bool force_friendly)
{
    mon_attitude_type new_att = (force_friendly ? ATT_FRIENDLY
                                   : attitude);

    // Neutral monsters don't need avatars, and in same cases would attack their
    // own avatars if they had them.
    if (new_att == ATT_NEUTRAL || new_att == ATT_STRICT_NEUTRAL
        || new_att == ATT_GOOD_NEUTRAL)
    {
        remove_avatars();
        return;
    }

    monster* avatar = find_spectral_weapon(this);
    if (avatar)
    {
        avatar->attitude = new_att;
        reset_spectral_weapon(avatar);
    }

    avatar = find_battlesphere(this);
    if (avatar)
    {
        avatar->attitude = new_att;
        reset_battlesphere(avatar);
    }
}

void monster::remove_avatars()
{
    monster* avatar = find_spectral_weapon(this);
    if (avatar)
        end_spectral_weapon(avatar, false, false);

    avatar = find_battlesphere(this);
    if (avatar)
        end_battlesphere(avatar, false);
}

/**
 * Does this monster have the given mutant beast facet?
 *
 * @param facet     The beast_facet in question; e.g. BF_BAT.
 * @return          Whether the given facet is one this monster has.
 */
bool monster::has_facet(int facet) const
{
    if (!props.exists(MUTANT_BEAST_FACETS))
        return false;

    for (auto facet_val : props[MUTANT_BEAST_FACETS].get_vector())
        if (facet_val.get_int() == facet)
            return true;
    return false;
}

/// If the player attacks this monster, will it become hostile?
bool monster::angered_by_attacks() const
{
    return !has_ench(ENCH_INSANE)
            && !mons_is_avatar(type)
            && type != MONS_SPELLFORGED_SERVITOR
            && !testbits(flags, MF_DEMONIC_GUARDIAN)
            && !mons_is_hepliaklqana_ancestor(type);
}
