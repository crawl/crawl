/**
 * @file
 * @brief Pseudo spells triggered by gods and various items.
**/

#include "AppHdr.h"

#include "spl-goditem.h"

#include "cleansing-flame-source-type.h"
#include "cloud.h"
#include "coordit.h"
#include "database.h"
#include "directn.h"
#include "env.h"
#include "fight.h"
#include "god-conduct.h"
#include "god-passive.h"
#include "hints.h"
#include "invent.h"
#include "item-prop.h"
#include "items.h"
#include "mapdef.h"
#include "mapmark.h"
#include "message.h"
#include "mon-behv.h"
#include "mon-cast.h"
#include "mon-death.h"
#include "mon-tentacle.h"
#include "religion.h"
#include "spl-util.h"
#include "state.h"
#include "status.h"
#include "stringutil.h"
#include "terrain.h"
#include "tiledef-dngn.h"
#include "traps.h"
#include "view.h"
#include "viewchar.h"

static void _print_holy_pacification_speech(const string &key,
                                            const monster &mon,
                                            msg_channel_type channel)
{
    string full_key = "holy_being_pacification";
    full_key += key;

    string msg = getSpeakString(full_key);

    if (!msg.empty())
    {
        msg = do_mon_str_replacements(msg, mon);
        strip_channel_prefix(msg, channel);
        mprf(channel, "%s", msg.c_str());
    }
}

static bool _mons_hostile(const monster* mon)
{
    // Needs to be done this way because of friendly/neutral enchantments.
    return !mon->wont_attack() && !mon->neutral();
}

/**
 * Is it possible for the player to pacify this monster, independent of their
 * total 'heal power'? If not, why not?
 *
 * @param mon   The monster to be checked for pacifiability.
 * @return      A description of why the monster can't be pacified, if it can't;
 *              e.g. "You cannot pacify this monster while she is sleeping!"
 *              If the monster *can* be pacified, returns the empty string.
 */
string unpacifiable_reason(const monster &mon)
{
    // XXX: be more specific?
    const string generic_reason = "You cannot pacify this monster!";

    // I was thinking of jellies when I wrote this, but maybe we shouldn't
    // exclude zombies and such... (jpeg)
    if (mons_intel(mon) <= I_BRAINLESS // no self-awareness
        || mons_is_tentacle_or_tentacle_segment(mon.type)) // body part
    {
        return generic_reason;
    }

    const mon_holy_type holiness = mon.holiness();

    if (!(holiness & (MH_HOLY | MH_UNDEAD | MH_DEMONIC | MH_NATURAL)))
        return generic_reason;

    if (mon.is_stationary()) // not able to leave the level
        return generic_reason;

    if (mon.asleep()) // not aware of what is happening
    {
        return make_stringf("You cannot pacify this monster while %s is "
                            "sleeping!",
                            mon.pronoun(PRONOUN_SUBJECTIVE).c_str());
    }

    // pacifiable, maybe!
    return "";
}

/**
 * By what factor should a monster of the given holiness have healing divided,
 * when calculating its vulnerability to Elyvilon's pacification?
 * A larger divisor means the monster is less vulnerable.
 *
 * @param holiness  The holiness of the mon to be pacified; e.g. MH_UNDEAD.
 * @return          A value to divide the healing the player does against the
 *                  monster by; e.g. 3.
 */
static int _pacification_heal_div(mon_holy_type holiness)
{
    if (holiness & MH_HOLY)
        return 2;
    if (holiness & MH_UNDEAD)
        return 4;
    if (holiness & MH_DEMONIC)
        return 5;
    return 3;
}

/**
 * What's the highest MHP version of the given monster that the player can
 * potentially pacify, with the given 'healing' roll?
 *
 * @param mon       The monster in question.
 * @param healing   The player's 'healing' roll.
 * @return          How much HP the player can pacify in the best case.
 */
static int _pacifiable_hp(const monster &mon, int healing)
{
    const int heal_mult = (mons_intel(mon) < I_HUMAN) ? 3  // animals
                                                      : 1; // other
    const int heal_div = _pacification_heal_div(mon.holiness());
    // ignoring monster holiness & int
    const int base_hp = you.skill(SK_INVOCATIONS, healing) + healing;
    const int hp = heal_mult * base_hp / heal_div;

    dprf("pacifying %s? factor: %d, Inv: %d, healed: %d, pacify hp pre-roll %d",
         mon.name(DESC_PLAIN).c_str(), heal_mult, you.skill(SK_INVOCATIONS),
         healing, hp);

    return hp;
}

/**
 * Try to pacify the given monster. Aborts if that's clearly impossible.
 *
 * @param mon           The monster to be pacified, potentially.
 * @param healed        The amount of healing the pacification attempt uses.
 * @param max_healed    The most healing the player could have rolled.
 * @param fail          Whether the healing invocation has failed (and will
 *                      return SPRET_FAILED after targeting checks finish).
 * @return              Whether the pacification effect was aborted
 *                      (SPRET_ABORT) or the invocation failed (SPRET_FAIL);
 *                      returns SPRET_SUCCESS otherwise, regardless of whether
 *                      the target was actually pacified.
 */
static spret_type _try_to_pacify(monster &mon, int healed, int max_healed,
                                 bool fail)
{
    const string illegal_reason = unpacifiable_reason(mon);
    if (!illegal_reason.empty())
    {
        mpr(illegal_reason);
        return SPRET_ABORT;
    }

    fail_check();

    const int mon_hp = mon.max_hit_points;

    if (_pacifiable_hp(mon, max_healed) < mon_hp)
    {
        // monster mhp too high to ever be pacified with your invo skill.
        dprf("mon hp %d", mon_hp);
        mprf("%s is completely unfazed by your meager offer of peace.",
             mon.name(DESC_THE).c_str());
        return SPRET_SUCCESS;
    }

    const int pacified_hp = random2(_pacifiable_hp(mon, healed));
    dprf("pacified hp: %d, mon hp %d", pacified_hp, mon_hp);
    if (pacified_hp * 23 / 20 < mon_hp)
    {
        // not even close.
        mprf("The light of Elyvilon fails to reach %s.",
             mon.name(DESC_THE).c_str());
        return SPRET_SUCCESS;
    }

    if (pacified_hp < mon_hp)
    {
        // closer! ...but not quite.
        mprf("The light of Elyvilon almost touches upon %s.",
             mon.name(DESC_THE).c_str());
        return SPRET_SUCCESS;
    }

    // we did it!
    // let the player know.
    if (mon.is_holy())
    {
        string key;

        // Quadrupeds can't salute, etc.
        if (mon_shape_is_humanoid(get_mon_shape(mon)))
            key = "_humanoid";

        _print_holy_pacification_speech(key, mon,
                                        MSGCH_FRIEND_ENCHANT);

        if (!one_chance_in(3)
            && mon.can_speak()
            && mon.type != MONS_MENNAS) // Mennas is mute and only has visual speech
        {
            _print_holy_pacification_speech("_speech", mon, MSGCH_TALK);
        }
    }
    else
        simple_monster_message(mon, " turns neutral.");

    record_monster_defeat(&mon, KILL_PACIFIED);
    mons_pacify(mon, ATT_NEUTRAL);

    heal_monster(mon, healed);
    return SPRET_SUCCESS;
}

/**
 * Heal a monster and print an appropriate message.
 *
 * Should only be called if the player is responsible!
 * @param patient the monster to be healed
 * @param amount  how many HP to restore
 * @return whether the monster could be healed.
 */
bool heal_monster(monster& patient, int amount)
{
    if (!patient.heal(amount))
        return false;

    mprf("You heal %s.", patient.name(DESC_THE).c_str());

    if (patient.hit_points == patient.max_hit_points)
        simple_monster_message(patient, " is completely healed.");
    else
        print_wounds(patient);

    return true;
}


static vector<string> _desc_mindless(const monster_info& mi)
{
    if (mi.intel() <= I_BRAINLESS)
        return { "mindless" };
    else
        return {};
}

spret_type cast_healing(int pow, int max_pow, bool fail)
{
    const int healed = pow + roll_dice(2, pow) - 2;
    const int max_healed = (3 * max_pow) - 2;
    ASSERT(healed >= 1);

    dist spd;

    direction_chooser_args args;
    args.restricts = DIR_TARGET;
    args.mode = TARG_INJURED_FRIEND;
    args.needs_path = false;
    args.self = CONFIRM_CANCEL;
    args.target_prefix = "Heal";
    args.get_desc_func = _desc_mindless;
    direction(spd, args);

    if (!spd.isValid)
        return SPRET_ABORT;
    if (cell_is_solid(spd.target))
    {
        canned_msg(MSG_NOTHING_THERE);
        return SPRET_ABORT;
    }

    monster* mons = monster_at(spd.target);
    if (!mons)
    {
        canned_msg(MSG_NOTHING_THERE);
        // This isn't a cancel, to avoid leaking invisible monster
        // locations.
        return SPRET_SUCCESS;
    }

    if (_mons_hostile(mons))
        return _try_to_pacify(*mons, healed, max_healed, fail);

    fail_check();

    if (!heal_monster(*mons, healed))
        canned_msg(MSG_NOTHING_HAPPENS);

    return SPRET_SUCCESS;
}

/// Effects that occur when the player is debuffed.
struct player_debuff_effects
{
    /// Attributes removed by a debuff.
    vector<attribute_type> attributes;
    /// Durations removed by a debuff.
    vector<duration_type> durations;
};

/**
 * What dispellable effects currently exist on the player?
 *
 * @param[out] buffs   The dispellable effects that exist on the player.
 *                     Assumed to be empty when passed in.
 */
static void _dispellable_player_buffs(player_debuff_effects &buffs)
{
    // attributes
    static const attribute_type dispellable_attributes[] = {
#if TAG_MAJOR_VERSION == 34
        ATTR_DELAYED_FIREBALL,
#endif
        ATTR_REPEL_MISSILES,
        ATTR_DEFLECT_MISSILES,
    };

    for (auto attribute : dispellable_attributes)
        if (you.attribute[attribute])
            buffs.attributes.push_back(attribute);

    // durations
    for (unsigned int i = 0; i < NUM_DURATIONS; ++i)
    {
        const int dur = you.duration[i];
        if (dur <= 0 || !duration_dispellable((duration_type) i))
            continue;
        if (i == DUR_TRANSFORMATION && you.form == transformation::shadow)
            continue;
        buffs.durations.push_back((duration_type) i);
        // this includes some buffs that won't be reduced in duration -
        // anything already at 1 aut, or flight/transform while <= 11 aut
        // that's probably not an actual problem
    }
}

/**
 * Does the player have any magical effects that can be removed (by debuff)?
 *
 * @return  Whether there are any effects to be dispelled.
 */
bool player_is_debuffable()
{
    player_debuff_effects buffs;
    _dispellable_player_buffs(buffs);
    return !buffs.durations.empty()
           || !buffs.attributes.empty();
}

/**
 * Remove magical effects from the player.
 *
 * Forms, buffs, debuffs, contamination, probably a few other things.
 * Flight gets an extra 11 aut before going away to minimize instadeaths.
 */
void debuff_player()
{
    bool need_msg = false;

    // find the list of debuffable effects currently active
    player_debuff_effects buffs;
    _dispellable_player_buffs(buffs);

    for (auto attr : buffs.attributes)
    {
        you.attribute[attr] = 0;
#if TAG_MAJOR_VERSION == 34
        if (attr == ATTR_DELAYED_FIREBALL)
            mprf(MSGCH_DURATION, "Your charged fireball dissipates.");
        else
#endif
            need_msg = true;
    }

    for (auto duration : buffs.durations)
    {
        int &len = you.duration[duration];
        if (duration == DUR_TELEPORT)
        {
            len = 0;
            mprf(MSGCH_DURATION, "You feel strangely stable.");
        }
        else if (duration == DUR_PETRIFYING)
        {
            len = 0;
            mprf(MSGCH_DURATION, "You feel limber!");
            you.redraw_evasion = true;
        }
        else if (duration == DUR_FLAYED)
        {
            len = 0;
            heal_flayed_effect(&you);
        }
        else if (len > 1)
        {
            len = 1;
            need_msg = true;
        }
    }

    if (need_msg)
        mprf(MSGCH_WARN, "Your magical effects are unravelling.");
}


/**
  * What dispellable effects currently exist on a given monster?
  *
  * @param[in] mon      The monster in question.
  * @param[out] buffs   The dispellable effects that exist on that monster.
  *                     Assumed to be empty when passed in.
  */
static void _dispellable_monster_buffs(const monster &mon,
                                       vector<enchant_type> &buffs)
{
    // Dispel all magical enchantments...
    for (enchant_type ench : dispellable_enchantments)
    {
        // except for permaconfusion.
        if (ench == ENCH_CONFUSION && mons_class_flag(mon.type, M_CONFUSED))
            continue;

        // Gozag-incited haste is permanent.
        if (ench == ENCH_HASTE && mon.has_ench(ENCH_GOZAG_INCITE))
            continue;

        if (mon.has_ench(ench))
            buffs.push_back(ench);
    }

    // special-case invis, to avoid hitting naturally invis monsters.
    if (mon.has_ench(ENCH_INVIS) && !mons_class_flag(mon.type, M_INVIS))
        buffs.push_back(ENCH_INVIS);
}


/**
 * Does a given monster have any buffs that can be removed?
 *
 * @param mon           The monster in question.
 */
bool monster_is_debuffable(const monster &mon)
{
    vector<enchant_type> buffs;
    _dispellable_monster_buffs(mon, buffs);
    return !buffs.empty();
}

/**
 * Remove magical effects from a given monster.
 *
 * @param mon           The monster to be debuffed.
 */
void debuff_monster(monster &mon)
{
    vector<enchant_type> buffs;
    _dispellable_monster_buffs(mon, buffs);
    if (buffs.empty())
        return;

    for (enchant_type buff : buffs)
        mon.del_ench(buff, true, true);

    simple_monster_message(mon, "'s magical effects unravel!");
}

// pow -1 for passive
int detect_items(int pow)
{
    int items_found = 0;
    int map_radius;
    if (pow >= 0)
        map_radius = 7 + random2(7) + pow;
    else
    {
        if (have_passive(passive_t::detect_items))
        {
            map_radius = min(you.piety / 20 - 1, LOS_RADIUS);
            if (map_radius <= 0)
                return 0;
        }
        else // MUT_JELLY_GROWTH
            map_radius = 5;
    }

    for (radius_iterator ri(you.pos(), map_radius, C_SQUARE); ri; ++ri)
    {
        // Don't expose new dug out areas:
        // Note: assumptions are being made here about how
        // terrain can change (eg it used to be solid, and
        // thus item free).
        if (pow != -1 && env.map_knowledge(*ri).changed())
            continue;

        if (you.visible_igrd(*ri) != NON_ITEM
            && !env.map_knowledge(*ri).item())
        {
            items_found++;
            env.map_knowledge(*ri).set_detected_item();
        }
    }

    return items_found;
}

static void _fuzz_detect_creatures(int pow, int *fuzz_radius, int *fuzz_chance)
{
    dprf("dc_fuzz: Power is %d", pow);

    pow = max(1, pow);

    *fuzz_radius = pow >= 50 ? 1 : 2;

    // Fuzz chance starts off at 100% and declines to a low of 10% for
    // obscenely powerful castings (pow caps around the 60 mark).
    *fuzz_chance = 100 - 90 * (pow - 1) / 59;
    if (*fuzz_chance < 10)
        *fuzz_chance = 10;
}

static bool _mark_detected_creature(coord_def where, monster* mon,
                                    int fuzz_chance, int fuzz_radius)
{
    bool found_good = false;

    if (fuzz_radius && x_chance_in_y(fuzz_chance, 100))
    {
        const int fuzz_diam = 2 * fuzz_radius + 1;

        coord_def place;
        // Try five times to find a valid placement, else we attempt to place
        // the monster where it really is (and may fail).
        for (int itry = 0; itry < 5; ++itry)
        {
            place.set(where.x + random2(fuzz_diam) - fuzz_radius,
                      where.y + random2(fuzz_diam) - fuzz_radius);

            if (!map_bounds(place))
                continue;

            // If the player would be able to see a monster at this location
            // don't place it there.
            if (you.see_cell(place))
                continue;

            // Try not to overwrite another detected monster.
            if (env.map_knowledge(place).detected_monster())
                continue;

            // Don't print monsters on terrain they cannot pass through,
            // not even if said terrain has since changed.
            if (!env.map_knowledge(place).changed()
                && mon->can_pass_through_feat(grd(place)))
            {
                found_good = true;
                break;
            }
        }

        if (found_good)
            where = place;
    }

    env.map_knowledge(where).set_detected_monster(mons_detected_base(mon->type));

    return found_good;
}

int detect_creatures(int pow, bool telepathic)
{
    int fuzz_radius = 0, fuzz_chance = 0;
    if (!telepathic)
        _fuzz_detect_creatures(pow, &fuzz_radius, &fuzz_chance);

    int creatures_found = 0;
    const int map_radius = 9 + random2(7) + pow;

    // Clear the map so detect creatures is more useful and the detection
    // fuzz is harder to analyse by averaging.
    clear_map(false);

    for (radius_iterator ri(you.pos(), map_radius, C_SQUARE); ri; ++ri)
    {
        if (monster* mon = monster_at(*ri))
        {
            // If you can see the monster, don't "detect" it elsewhere.
            if (!you.can_see(*mon))
            {
                creatures_found++;
                _mark_detected_creature(*ri, mon, fuzz_chance, fuzz_radius);
            }
        }
    }

    return creatures_found;
}

static bool _selectively_remove_curse(const string &pre_msg)
{
    bool used = false;

    while (1)
    {
        if (!any_items_of_type(OSEL_CURSED_WORN) && used)
        {
            mpr("You have uncursed all your worn items.");
            return used;
        }

        int item_slot = prompt_invent_item("Uncurse which item?", MT_INVLIST,
                                           OSEL_CURSED_WORN, OPER_ANY,
                                           invprompt_flag::escape_only);
        if (prompt_failed(item_slot))
            return used;

        item_def& item(you.inv[item_slot]);

        if (!item.cursed()
            || !item_is_equipped(item)
            || &item == you.weapon() && !is_weapon(item))
        {
            mpr("Choose a cursed equipped item, or Esc to abort.");
            more();
            continue;
        }

        if (!used && !pre_msg.empty())
            mpr(pre_msg);

        do_uncurse_item(item, false);
        used = true;
    }
}

bool remove_curse(bool alreadyknown, const string &pre_msg)
{
    if (have_passive(passive_t::want_curses) && alreadyknown)
    {
        if (_selectively_remove_curse(pre_msg))
        {
            ash_check_bondage();
            return true;
        }
        else
            return false;
    }

    bool success = false;

    // Players can no longer wield armour and jewellery as weapons, so we do
    // not need to check whether the EQ_WEAPON slot actually contains a weapon:
    // only weapons (and staves) are both wieldable and cursable.
    for (int i = EQ_WEAPON; i < NUM_EQUIP; i++)
    {
        // Melded equipment can also get uncursed this way.
        item_def * const it = you.slot_item(equipment_type(i), true);
        if (it && it->cursed())
        {
            do_uncurse_item(*it);
            success = true;
        }
    }

    if (success)
    {
        if (!pre_msg.empty())
            mpr(pre_msg);
        mpr("You feel as if something is helping you.");
        learned_something_new(HINT_REMOVED_CURSE);
    }
    else if (alreadyknown)
        mprf(MSGCH_PROMPT, "None of your equipped items are cursed.");
    else
    {
        if (!pre_msg.empty())
            mpr(pre_msg);
        mpr("You feel blessed for a moment.");
    }

    return success;
}

#if TAG_MAJOR_VERSION == 34
static bool _selectively_curse_item(bool armour, const string &pre_msg)
{
    while (1)
    {
        int item_slot = prompt_invent_item("Curse which item?", MT_INVLIST,
                                           armour ? OSEL_UNCURSED_WORN_ARMOUR
                                                  : OSEL_UNCURSED_WORN_JEWELLERY,
                                           OPER_ANY, invprompt_flag::escape_only);
        if (prompt_failed(item_slot))
            return false;

        item_def& item(you.inv[item_slot]);

        if (item.cursed()
            || !item_is_equipped(item)
            || armour && item.base_type != OBJ_ARMOUR
            || !armour && item.base_type != OBJ_JEWELLERY)
        {
            mprf("Choose an uncursed equipped piece of %s, or Esc to abort.",
                 armour ? "armour" : "jewellery");
            more();
            continue;
        }

        if (!pre_msg.empty())
            mpr(pre_msg);
        do_curse_item(item, false);
        learned_something_new(HINT_YOU_CURSED);
        return true;
    }
}

bool curse_item(bool armour, const string &pre_msg)
{
    // Make sure there's something to curse first.
    bool found = false;
    int min_type, max_type;
    if (armour)
        min_type = EQ_MIN_ARMOUR, max_type = EQ_MAX_ARMOUR;
    else
        min_type = EQ_LEFT_RING, max_type = EQ_RING_AMULET;
    for (int i = min_type; i <= max_type; i++)
    {
        if (you.equip[i] != -1 && !you.inv[you.equip[i]].cursed())
            found = true;
    }
    if (!found)
    {
        mprf(MSGCH_PROMPT, "You aren't wearing any piece of uncursed %s.",
             armour ? "armour" : "jewellery");
        return false;
    }

    return _selectively_curse_item(armour, pre_msg);
}
#endif

static bool _do_imprison(int pow, const coord_def& where, bool zin)
{
    // power guidelines:
    // powc is roughly 50 at Evoc 10 with no godly assistance, ranging
    // up to 300 or so with godly assistance or end-level, and 1200
    // as more or less the theoretical maximum.
    int number_built = 0;

    static const set<dungeon_feature_type> safe_tiles =
    {
        DNGN_SHALLOW_WATER, DNGN_DEEP_WATER, DNGN_FLOOR, DNGN_OPEN_DOOR
    };

    bool proceed;
    monster *mon;
    string targname;

    vector<coord_def> veto_spots(8);
    for (adjacent_iterator ai(where); ai; ++ai)
        veto_spots.push_back(*ai);
    const vector<coord_def> adj_spots = veto_spots;

    if (zin)
    {
        // We need to get this now because we won't be able to see
        // the monster once the walls go up!
        mon = monster_at(where);
        targname = mon->name(DESC_THE);
        bool success = true;
        bool none_vis = true;

        // Check that any adjacent creatures can be pushed out of the way.
        for (adjacent_iterator ai(where); ai; ++ai)
        {
            // The tile is occupied.
            if (actor *act = actor_at(*ai))
            {
                // Can't push ourselves.
                coord_def newpos;
                if (act->is_player()
                    || !get_push_space(*ai, newpos, act, true, &veto_spots))
                {
                    success = false;
                    if (you.can_see(*act))
                        none_vis = false;
                    break;
                }
                else
                    veto_spots.push_back(newpos);
            }

            // don't try to shove the orb of zot into lava and/or crash
            if (igrd(*ai) != NON_ITEM)
            {
                coord_def newpos;
                if (!get_push_space(*ai, newpos, nullptr, true, &adj_spots))
                {
                    success = false;
                    none_vis = false;
                    break;
                }
            }

            // Make sure we have a legitimate tile.
            proceed = false;
            if (cell_is_solid(*ai) && !feat_is_opaque(grd(*ai)))
            {
                success = false;
                none_vis = false;
                break;
            }
        }

        if (!success)
        {
            mprf(none_vis ? "You briefly glimpse something next to %s."
                        : "You need more space to imprison %s.",
                targname.c_str());
            return false;
        }
    }

    veto_spots = adj_spots;
    for (adjacent_iterator ai(where); ai; ++ai)
    {
        // This is where power comes in.
        if (!zin && one_chance_in(pow / 3))
            continue;

        // The tile is occupied.
        if (zin)
        {
            if (actor* act = actor_at(*ai))
            {
                coord_def newpos;
                get_push_space(*ai, newpos, act, true, &veto_spots);
                ASSERT(!newpos.origin());
                act->move_to_pos(newpos);
                veto_spots.push_back(newpos);
            }
        }

        // Make sure we have a legitimate tile.
        proceed = false;
        if (!zin && !monster_at(*ai))
        {
            if (feat_is_trap(grd(*ai), true) || feat_is_stone_stair(grd(*ai))
                || safe_tiles.count(grd(*ai)))
            {
                proceed = true;
            }
        }
        else if (zin && !cell_is_solid(*ai))
            proceed = true;

        if (proceed)
        {
            // All items are moved aside.
            if (igrd(*ai) != NON_ITEM)
            {
                coord_def newpos;
                get_push_space(*ai, newpos, nullptr, true);
                if (zin) // zin should've checked for this earlier
                    ASSERT(!newpos.origin());
                else if (newpos.origin())  // tomb just skips the tile
                    continue;
                move_items(*ai, newpos);
            }

            // All traps are destroyed.
            if (trap_def *ptrap = trap_at(*ai))
            {
                ptrap->destroy();
                grd(*ai) = DNGN_FLOOR;
            }

            // Actually place the wall.
            if (zin)
            {
                map_wiz_props_marker *marker = new map_wiz_props_marker(*ai);
                marker->set_property("feature_description", "a gleaming silver wall");
                env.markers.add(marker);

                temp_change_terrain(*ai, DNGN_METAL_WALL, INFINITE_DURATION,
                                    TERRAIN_CHANGE_IMPRISON);

                // Make the walls silver.
                env.grid_colours(*ai) = WHITE;
                env.tile_flv(*ai).feat_idx =
                        store_tilename_get_index("dngn_silver_wall");
                env.tile_flv(*ai).feat = TILE_DNGN_SILVER_WALL;
                if (env.map_knowledge(*ai).seen())
                {
                    env.map_knowledge(*ai).set_feature(DNGN_METAL_WALL);
                    env.map_knowledge(*ai).clear_item();
#ifdef USE_TILE
                    env.tile_bk_bg(*ai) = TILE_DNGN_SILVER_WALL;
                    env.tile_bk_fg(*ai) = 0;
#endif
                }
            }
            // Tomb card
            else
            {
                temp_change_terrain(*ai, DNGN_ROCK_WALL, INFINITE_DURATION,
                                    TERRAIN_CHANGE_TOMB);
            }

            number_built++;
        }
    }

    if (number_built > 0)
    {
        if (zin)
        {
            mprf("Zin imprisons %s with walls of pure silver!",
                 targname.c_str());
        }
        else
            mpr("Walls emerge from the floor!");

        you.update_beholders();
        you.update_fearmongers();
        env.markers.clear_need_activate();
    }
    else
        canned_msg(MSG_NOTHING_HAPPENS);

    return number_built > 0;
}

bool entomb(int pow)
{
    if (_do_imprison(pow, you.pos(), false))
    {
        const int tomb_duration = BASELINE_DELAY * pow;
        env.markers.add(new map_tomb_marker(you.pos(),
                                            tomb_duration,
                                            you.mindex(),
                                            you.mindex()));
        env.markers.clear_need_activate(); // doesn't need activation
        return true;
    }

    return false;
}

bool cast_imprison(int pow, monster* mons, int source)
{
    if (_do_imprison(pow, mons->pos(), true))
    {
        const int tomb_duration = BASELINE_DELAY * pow;
        env.markers.add(new map_tomb_marker(mons->pos(),
                                            tomb_duration,
                                            source,
                                            mons->mindex()));
        env.markers.clear_need_activate(); // doesn't need activation
        return true;
    }

    return false;
}

bool cast_smiting(int pow, monster* mons)
{
    if (mons == nullptr || mons->submerged())
    {
        canned_msg(MSG_NOTHING_THERE);
        // Counts as a real cast, due to invisible/submerged monsters.
        return true;
    }

    god_conduct_trigger conducts[3];
    disable_attack_conducts(conducts);

    const bool success = !stop_attack_prompt(mons, false, you.pos());

    if (success)
    {
        set_attack_conducts(conducts, mons);

        mprf("You smite %s!", mons->name(DESC_THE).c_str());

        behaviour_event(mons, ME_ANNOY, &you);
    }

    enable_attack_conducts(conducts);

    if (success)
    {
        // damage at 0 Invo ranges from 9-12 (avg 10), to 9-72 (avg 40) at 27.
        int damage_increment = div_rand_round(pow, 8);
        mons->hurt(&you, 6 + roll_dice(3, damage_increment));
        if (mons->alive())
            print_wounds(*mons);
    }

    return success;
}

void holy_word_player(holy_word_source_type source)
{
    if (!you.undead_or_demonic())
        return;

    int hploss = max(0, you.hp / 2 - 1);

    if (!hploss)
        return;

    mpr("You are blasted by holy energy!");

    const char *aux = "holy word";

    kill_method_type type = KILLED_BY_SOMETHING;
    if (crawl_state.is_god_acting())
        type = KILLED_BY_DIVINE_WRATH;

    switch (source)
    {
    case HOLY_WORD_SCROLL:
        aux = "a scroll of holy word";
        break;

    case HOLY_WORD_ZIN:
        aux = "Zin's holy word";
        break;

    case HOLY_WORD_TSO:
        aux = "the Shining One's holy word";
        break;

    case HOLY_WORD_CARD:
        aux = "the Torment card";
        break;
    }

    ouch(hploss, type, MID_NOBODY, aux);

    return;
}

void holy_word_monsters(coord_def where, int pow, holy_word_source_type source,
                        actor *attacker)
{
    pow = min(300, pow);

    // Is the player in this cell?
    if (where == you.pos())
        holy_word_player(source);

    // Is a monster in this cell?
    monster* mons = monster_at(where);
    if (!mons || !mons->alive() || !mons->undead_or_demonic())
        return;

    int hploss = roll_dice(3, 15) + (random2(pow) / 5);

    if (hploss)
    {
        if (source == HOLY_WORD_ZIN)
            simple_monster_message(*mons, " is blasted by Zin's holy word!");
        else
            simple_monster_message(*mons, " convulses!");
    }
    mons->hurt(attacker, hploss, BEAM_MISSILE);

    if (!hploss || !mons->alive())
        return;

    // Currently, holy word annoys the monsters it affects
    // because it can kill them, and because hostile
    // monsters don't use it.
    // Tolerate unknown scroll, to not annoy Yred worshippers too much.
    if (attacker != nullptr
        && attacker != mons
        && (attacker != &you
            || source != HOLY_WORD_SCROLL
            || item_type_known(OBJ_SCROLLS, SCR_HOLY_WORD)))
    {
        behaviour_event(mons, ME_ANNOY, attacker);
    }

    mons->add_ench(mon_enchant(ENCH_DAZED, 0, attacker,
                               (10 + random2(10)) * BASELINE_DELAY));
}

void holy_word(int pow, holy_word_source_type source, const coord_def& where,
               bool silent, actor *attacker)
{
    if (!silent && attacker)
    {
        mprf("%s %s a Word of immense power!",
             attacker->name(DESC_THE).c_str(),
             attacker->conj_verb("speak").c_str());
    }

    for (radius_iterator ri(where, LOS_SOLID); ri; ++ri)
        holy_word_monsters(*ri, pow, source, attacker);
}

void torment_player(actor *attacker, torment_source_type taux)
{
    ASSERT(!crawl_state.game_is_arena());

    int hploss = 0;

    if (!player_res_torment())
    {
        // Negative energy resistance can alleviate torment.
        hploss = max(0, you.hp * (50 - player_prot_life() * 5) / 100 - 1);
        // Statue form is only partial petrification.
        if (you.form == transformation::statue || you.species == SP_GARGOYLE)
            hploss /= 2;
    }

    // Kiku protects you from torment to a degree.
    const bool kiku_shielding_player = player_kiku_res_torment();

    if (kiku_shielding_player)
    {
        if (hploss > 0)
        {
            if (random2(600) < you.piety) // 13.33% to 33.33% chance
            {
                hploss = 0;
                simple_god_message(" shields you from torment!");
            }
            else if (random2(250) < you.piety) // 24% to 80% chance
            {
                hploss -= random2(hploss - 1);
                simple_god_message(" partially shields you from torment!");
            }
        }
    }

    if (!hploss)
    {
        mpr("You feel a surge of unholy energy.");
        return;
    }

    mpr("Your body is wracked with pain!");


    kill_method_type type = KILLED_BY_BEAM;
    if (crawl_state.is_god_acting())
        type = KILLED_BY_DIVINE_WRATH;
    else if (taux == TORMENT_MISCAST)
        type = KILLED_BY_WILD_MAGIC;

    const char *aux = "";

    switch (taux)
    {
    case TORMENT_CARDS:
    case TORMENT_SPELL:
        aux = "Symbol of Torment";
        break;

    case TORMENT_AGONY:
        aux = "Agony";
        break;

    case TORMENT_SCEPTRE:
        aux = "sceptre of Torment";
        break;

    case TORMENT_SCROLL:
        aux = "a scroll of torment";
        break;

    case TORMENT_XOM:
        type = KILLED_BY_XOM;
        aux = "Xom's torment";
        break;

    case TORMENT_KIKUBAAQUDGHA:
        aux = "Kikubaaqudgha's torment";
        break;

    case TORMENT_LURKING_HORROR:
        type = KILLED_BY_SPORE;
        aux = "an exploding lurking horror";
        break;

    case TORMENT_MISCAST:
        aux = "by torment";
        break;
    }

    ouch(hploss, type, attacker ? attacker->mid : MID_NOBODY, aux);

    return;
}

void torment_cell(coord_def where, actor *attacker, torment_source_type taux)
{
    // Is the player in this cell?
    if (where == you.pos())
        torment_player(attacker, taux);
    // Don't return, since you could be standing on a monster.

    // Is a monster in this cell?
    monster* mons = monster_at(where);
    if (!mons || !mons->alive() || mons->res_torment())
        return;

    int hploss = max(0, mons->hit_points * (50 - mons->res_negative_energy() * 5) / 100 - 1);

    if (hploss)
    {
        simple_monster_message(*mons, " convulses!");

        // Currently, torment doesn't annoy the monsters it affects
        // because it can't kill them, and because hostile monsters use
        // it. It does alert them, though.
        // XXX: attacker isn't passed through "int torment()".
        behaviour_event(mons, ME_ALERT, attacker);
    }

    mons->hurt(attacker, hploss, BEAM_TORMENT_DAMAGE);
}

void torment(actor *attacker, torment_source_type taux, const coord_def& where)
{
    for (radius_iterator ri(where, LOS_NO_TRANS); ri; ++ri)
        torment_cell(*ri, attacker, taux);
}

void setup_cleansing_flame_beam(bolt &beam, int pow, int caster,
                                coord_def where, actor *attacker)
{
    beam.flavour      = BEAM_HOLY;
    beam.glyph        = dchar_glyph(DCHAR_FIRED_BURST);
    beam.damage       = dice_def(2, pow);
    beam.target       = where;
    beam.name         = "golden flame";
    beam.colour       = YELLOW;
    beam.aux_source   = (caster == CLEANSING_FLAME_TSO)
                        ? "the Shining One's cleansing flame"
                        : "cleansing flame";
    beam.ex_size      = 2;
    beam.is_explosion = true;

    if (caster == CLEANSING_FLAME_GENERIC || caster == CLEANSING_FLAME_TSO)
    {
        beam.thrower   = KILL_MISC;
        beam.source_id = MID_NOBODY;
    }
    else if (attacker->is_player())
    {
        beam.thrower   = KILL_YOU;
        beam.source_id = MID_PLAYER;
    }
    else
    {
        // If there was no attacker, caster should have been
        // CLEANSING_FLAME_{GENERIC,TSO} which we handled above.
        ASSERT(attacker);

        beam.thrower   = KILL_MON;
        beam.source_id = attacker->mid;
    }
}

void cleansing_flame(int pow, int caster, coord_def where,
                     actor *attacker)
{
    bolt beam;
    setup_cleansing_flame_beam(beam, pow, caster, where, attacker);
    beam.explode();
}

spret_type cast_random_effects(int pow, bolt& beam, bool fail)
{
    bolt tracer = beam;
    if (!player_tracer(ZAP_DEBUGGING_RAY, 200, tracer, LOS_RADIUS))
        return SPRET_ABORT;

    fail_check();

    // Extremely arbitrary list of possible effects.
    zap_type zap = random_choose(ZAP_THROW_FLAME,
                                 ZAP_THROW_FROST,
                                 ZAP_SLOW,
                                 ZAP_HASTE,
                                 ZAP_PARALYSE,
                                 ZAP_CONFUSE,
                                 ZAP_TELEPORT_OTHER,
                                 ZAP_INVISIBILITY,
                                 ZAP_ICEBLAST,
                                 ZAP_FIREBALL,
                                 ZAP_BOLT_OF_DRAINING,
                                 ZAP_VENOM_BOLT);

    zapping(zap, pow, beam, false);

    return SPRET_SUCCESS;
}
