/**
 * @file
 * @brief Pseudo spells triggered by gods and various items.
**/

#include "AppHdr.h"

#include "spl-goditem.h"

#include "art-enum.h"
#include "attack.h"
#include "cleansing-flame-source-type.h"
#include "colour.h"
#include "coordit.h"
#include "database.h"
#include "directn.h"
#include "english.h"
#include "env.h"
#include "tile-env.h"
#include "fight.h"
#include "god-conduct.h"
#include "god-passive.h"
#include "hints.h"
#include "invent.h"
#include "item-prop.h"
#include "items.h"
#include "level-state-type.h"
#include "los.h"
#include "mapdef.h"
#include "mapmark.h"
#include "map-knowledge.h"
#include "message.h"
#include "mon-behv.h"
#include "mon-cast.h"
#include "mon-death.h"
#include "mon-place.h"
#include "mon-tentacle.h"
#include "notes.h" // NOTE_DREAMSHARD
#include "player.h"
#include "random.h"
#include "religion.h"
#include "spl-clouds.h"
#include "spl-util.h"
#include "state.h"
#include "status.h"
#include "stringutil.h"
#include "tag-version.h"
#include "terrain.h"
#include "rltiles/tiledef-dngn.h"
#include "traps.h"
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

string unpacifiable_reason(const monster& mon)
{
    return unpacifiable_reason(monster_info(&mon));
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
string unpacifiable_reason(const monster_info& mi)
{
    // I was thinking of jellies when I wrote this, but maybe we shouldn't
    // exclude zombies and such... (jpeg)
    if (mi.intel() <= I_BRAINLESS // no self-awareness
        || mons_is_tentacle_or_tentacle_segment(mi.type)) // body part
    {
        return "You cannot pacify mindless monsters!";
    }

    const mon_holy_type holiness = mi.holi;

    if (holiness & MH_NONLIVING)
        return "You cannot pacify nonliving monsters!";

    if (mons_class_is_stationary(mi.type)) // not able to leave the level
        return "You cannot pacify immobile monsters!";

    if (mi.is(MB_SLEEPING) || mi.is(MB_DORMANT)) // unaware of what's happening
    {
        return make_stringf("You cannot pacify this monster while %s %s %s!",
                            mi.pronoun(PRONOUN_SUBJECTIVE),
                            conjugate_verb("are",
                                           mi.pronoun_plurality()).c_str(),
                            mi.is(MB_SLEEPING) ? "asleep" : "dormant");
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
 * The sides for the roll against monster avg hp to determine if the
 * monster can be pacified.
 *
 * The formula is sides = int * ((invo + 1) * power) / holiness)
 * where
 *   power = 30 + invo
 *   int = 3 for animals, 1 for smarter intelligence
 *   holiness = holiness modifier determined in _heal_div
 *
 * @param mc        The type of monster in question.
 * @param pow       The power of the pacification.
 * @return          The dice sides to roll against max hp
 */
static int _pacification_sides(const monster_type mc, int pow)
{
    const int heal_mult = (mons_class_intel(mc) < I_HUMAN) ? 3  // animals
                                                      : 1; // other
    const int heal_div = _pacification_heal_div(mons_class_holiness(mc));
    // ignoring monster holiness & int
    const int base_sides = you.skill(SK_INVOCATIONS, pow) + pow;
    const int sides = heal_mult * base_sides / heal_div;

    return sides;
}

/**
 * Pan lords and player ghosts are beyond Elyvilon's light
 */
static int _pacification_hp(monster_type mc)
{
    return mons_is_pghost(mc) || mc == MONS_PANDEMONIUM_LORD ? 1000
        : mons_avg_hp(mc);
}

/**
 * Try to pacify the given monster. Aborts if that's clearly impossible.
 *
 * @param mon           The monster to be pacified, potentially.
 * @param healed        The amount of healing the pacification attempt uses.
 * @param pow           The healing power.
 * @param fail          Whether the healing invocation has failed (and will
 *                      return spret::failED after targeting checks finish).
 * @return              Whether the pacification effect was aborted
 *                      (spret::abort) or the invocation failed (spret::fail);
 *                      returns spret::success otherwise, regardless of whether
 *                      the target was actually pacified.
 */
static spret _try_to_pacify(monster &mon, int healed, int pow,
                                 bool fail)
{
    const monster_info mi(&mon);
    const string illegal_reason = unpacifiable_reason(mi);
    if (!illegal_reason.empty())
    {
        mpr(illegal_reason);
        return spret::abort;
    }

    fail_check();

    const int mon_hp = _pacification_hp(mon.type);

    if (_pacification_sides(mon.type, pow) < mon_hp)
    {
        // monster avg hp too high to ever be pacified with your invo skill.
        mprf("%s would be completely unfazed by your meagre offer of peace.",
             mon.name(DESC_THE).c_str());
        return spret::abort;
    }

    // Take the min of two rolls of 1d(_pacification_sides)
    const int pacified_roll = biased_random2(_pacification_sides(mon.type, pow) - 1,2);
    dprf("pacified roll: %d, monclass avmhp: %d", pacified_roll, mon_hp);
    if (pacified_roll * 23 / 20 < mon_hp)
    {
        // not even close.
        mprf("The light of Elyvilon fails to reach %s.",
             mon.name(DESC_THE).c_str());
        return spret::success;
    }

    if (pacified_roll < mon_hp)
    {
        // closer! ...but not quite.
        mprf("The light of Elyvilon almost touches upon %s.",
             mon.name(DESC_THE).c_str());
        return spret::success;
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
    return spret::success;
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

/**
 * Compute the success chance of pacification out of scale
 *
 * biased_random2(sides-1,2) is equivalent to picking two integers in
 * [0,sides), re-rolling if they come up the same, and taking a min if they
 * come up distinct. The formula below computes the probability of rolling two
 * numbers that are both large enough, minus the probability they are the same.
 * This probability is the sum of the geometric series with base
 *  a = ((s - t) ^2 - (s - t)) / s^2
 * and ratio
 *  r = 1 / s
 *
 *  (a is the probability of both die being unequal and winning, r is the
 *   probability of both coming up equal.)
 *
 * The reason for the + 1 in the inequality is that if the die is only one
 * larger than monster hp, the min of two distinct rolls is guaranteed to lose.
 */
static int _pacify_chance(const monster_info& mi, const int pow, int scale)
{
    const int sides = _pacification_sides(mi.type, pow);
    const int target = _pacification_hp(mi.type);

    if (sides <= target + 1)
        return 0;

    return (scale * ((sides - target) * (sides - target) - (sides - target)))
         / (sides * sides - sides);
}

static vector<string> _desc_pacify_chance(const monster_info& mi, const int pow)
{
    vector<string> descs;

    if (mi.intel() <= I_BRAINLESS)
        descs.push_back("mindless");
    else if (!unpacifiable_reason(mi).empty()
             || _pacification_sides(mi.type, pow)
                <= _pacification_hp(mi.type) + 1)
        descs.push_back("uninterested");
    else
    {
        const int success = _pacify_chance(mi, pow, 100);
        if (success == 0)
            descs.push_back(make_stringf("chance to pacify: <<1%%"));
        else
            descs.push_back(make_stringf("chance to pacify: %d%%", success));
    }
    return descs;
}

spret cast_healing(int pow, bool fail)
{
    // This arithmetic is to make the healing amount match Greater Healing
    const int base = div_rand_round(pow, 3);
    const int healed = base + roll_dice(2, base) - 2;
    ASSERT(healed >= 1);

    dist spd;

    direction_chooser_args args;
    args.restricts = DIR_TARGET;
    args.mode = TARG_INJURED_FRIEND;
    args.needs_path = false;
    args.self = confirm_prompt_type::cancel;
    args.target_prefix = "Heal";
    args.get_desc_func = bind(_desc_pacify_chance, placeholders::_1, pow);
    direction(spd, args);

    if (!spd.isValid)
        return spret::abort;
    if (cell_is_solid(spd.target))
    {
        canned_msg(MSG_NOTHING_THERE);
        return spret::abort;
    }

    monster* mons = monster_at(spd.target);
    if (!mons)
    {
        fail_check();
        canned_msg(MSG_NOTHING_THERE);
        // This isn't a cancel, to avoid leaking invisible monster
        // locations.
        return spret::success;
    }

    if (_mons_hostile(mons))
        return _try_to_pacify(*mons, healed, pow, fail);

    fail_check();

    if (!heal_monster(*mons, healed))
        canned_msg(MSG_NOTHING_HAPPENS);

    return spret::success;
}

/**
 * What dispellable effects currently exist on the player?
 *
 * @return The dispellable effects that currently exist on the player.
 */
static vector<duration_type> _dispellable_player_buffs()
{
    vector<duration_type> dispellables;
    for (unsigned int i = 0; i < NUM_DURATIONS; ++i)
    {
        const int dur = you.duration[i];
        if (dur <= 0 || !duration_dispellable((duration_type) i))
            continue;
        if (i == DUR_TRANSFORMATION && (you.form == you.default_form
                                        || you.form == transformation::slaughter))
        {
            continue;
        }
        // XXX: Handle special-cases with regard to monster auras.
        //      (It would be nice if this could be handled automatically, but
        //      there aren't yet enough of these effects to bother doing such)
        if (i == DUR_SLOW && aura_is_active_on_player(TORPOR_SLOWED_KEY))
            continue;
        else if (i == DUR_SENTINEL_MARK && aura_is_active_on_player(OPHAN_MARK_KEY))
            continue;

        dispellables.push_back((duration_type) i);
        // this includes some buffs that won't be reduced in duration -
        // anything already at 1 aut, or flight/transform while <= 11 aut
        // that's probably not an actual problem
    }

    return dispellables;
}

/**
 * Does the player have any magical effects that can be removed (by debuff)?
 *
 * @return  Whether there are any effects to be dispelled.
 */
bool player_is_debuffable()
{
    return !_dispellable_player_buffs().empty();
}

/**
 * Does the player have any magical effects that can be removed
 * or any magical contamination?
 *
 * @return Whether cancellation will have any effect on the player.
 */
bool player_is_cancellable()
{
    return get_contamination_level() || player_is_debuffable();
}

/**
 * Lists out the effects that will be removed by cancellation.
 *
 * @param debuffs_only   If true, contamination levels are unaffected.
 */
string describe_player_cancellation(bool debuffs_only)
{
    vector<string> effects;

    // Try to clarify it doesn't remove all contam?
    if (!debuffs_only && get_contamination_level())
        effects.push_back("as magically contaminated");

    vector<duration_type> buffs = _dispellable_player_buffs();
    for (auto duration : buffs)
    {
        if (duration == DUR_TRANSFORMATION)
        {
            effects.push_back("transformed");
            continue;
        }

        status_info inf;
        if (fill_status_info(duration, inf) && !inf.short_text.empty())
        {
            strip_suffix(inf.short_text, " (expiring)");
            effects.push_back(inf.short_text);
        }
    }

    // I hate this, but here are some awkward special cases.
    // (I suspect there are more.)
    static const vector<status_type> dispellable_statuses = {
        STATUS_AIRBORNE,
        STATUS_SPEED,
        STATUS_INVISIBLE,
        STATUS_BEHELD,
    };
    for (auto status : dispellable_statuses)
    {
        status_info inf;
        if (!fill_status_info(status, inf) || inf.short_text.empty())
            continue;
        if (status != STATUS_AIRBORNE)
        {
            strip_suffix(inf.short_text, " (expiring)");
            effects.push_back(inf.short_text);
            continue;
        }
        if (you.attribute[ATTR_PERM_FLIGHT] || you.racial_permanent_flight())
            continue;
        effects.push_back("flying");
    }

    return comma_separated_line(begin(effects), end(effects), " or ");
}

/**
 * Remove magical effects from the player.
 *
 * Forms, buffs, debuffs, contamination, probably a few other things.
 * Flight gets an extra 11 aut before going away to minimize instadeaths.
 */
void debuff_player(bool ignore_resistance)
{
    bool need_msg = false;

    // find the list of debuffable effects currently active
    vector<duration_type> buffs = _dispellable_player_buffs();

    if (!buffs.empty() & !ignore_resistance && you.has_mutation(MUT_INVIOLATE_MAGIC))
    {
        mpr("Your magical effects refuse to unravel!");
        return;
    }

    for (auto duration : buffs)
    {
        int &len = you.duration[duration];
        if (duration == DUR_TELEPORT)
        {
            len = 0;
            mprf(MSGCH_DURATION, "You feel strangely stable.");
            you.props.erase(SJ_TELEPORTITIS_SOURCE);
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
        else if (duration == DUR_STICKY_FLAME)
        {
            mprf(MSGCH_DURATION, "You are no longer on fire.");
            end_sticky_flame_player();
        }
        else if (duration == DUR_FORTRESS_BLAST_TIMER)
        {
            len = 0;
            mprf(MSGCH_DURATION, "Your fortress blast dissipates harmlessly.");
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

bool monster_can_be_unravelled(const monster& mon)
{
    return monster_is_debuffable(mon) || mon.is_abjurable();
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
        mon.del_ench(buff, true, buff != ENCH_PETRIFYING);
    // We generally want to set effect = true so that enchantments get cleaned
    // up - invisible monsters reappear, etc. However, the only thing that
    // effect = true does for PETRIFYING is cause it to turn into
    // ENCH_PETRIFIED. So... let's not do that. (Hacky, sorry!)

    simple_monster_message(mon, " magical effects unravel!", true);
}

// pow -1 for passive
int detect_items(int pow)
{
    int items_found = 0;
    int map_radius = 0;
    if (pow >= 0)
        map_radius = 7 + random2(7) + pow;

    else if (you.has_mutation(MUT_STRONG_NOSE))
        map_radius = get_los_radius();
    else
    {
        if (you.has_mutation(MUT_JELLY_GROWTH))
            map_radius = 5;
        // Check which god may be providing detect_items and set map_radius
        if (have_passive(passive_t::detect_items))
        {
            map_radius = max(map_radius,
                             min(you.piety / 20 - 1, get_los_radius()));

            if (map_radius <= 0)
                return 0;
        }
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

static void _mark_detected_creature(coord_def where, const monster& mon,
                                    int fuzz_chance, int fuzz_radius)
{
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

            // the player believes there is no monster there, and this one could be there
            if (query_map_knowledge(false, place, [&mon](const map_cell& m) {
                  return !m.detected_monster() && mon.can_pass_through_feat(m.feat());
                }) && !you.see_cell(place))
            {
                where = place;
            }
        }
    }

    env.map_knowledge(where).set_detected_monster(mons_detected_base(mon.type));
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
    clear_map();

    for (radius_iterator ri(you.pos(), map_radius, C_SQUARE); ri; ++ri)
    {
        if (monster* mon = monster_at(*ri))
        {
            // If you can see the monster, don't "detect" it elsewhere.
            if (!you.can_see(*mon))
            {
                creatures_found++;
                _mark_detected_creature(*ri, *mon, fuzz_chance, fuzz_radius);
            }
        }
    }

    return creatures_found;
}

spret cast_tomb(int pow, actor* victim, int source, bool fail)
{
    const coord_def& where = victim->pos();
    int number_built = 0;

    // This is a very dubious set. Maybe we should just use !(feat_is_solid)?
    static const set<dungeon_feature_type> safe_tiles =
    {
        DNGN_SHALLOW_WATER, DNGN_DEEP_WATER, DNGN_FLOOR, DNGN_OPEN_DOOR,
        DNGN_OPEN_CLEAR_DOOR, DNGN_BROKEN_DOOR
    };

    bool zin = source == -GOD_ZIN;
    bool proceed;
    string targname;

    vector<coord_def> veto_spots(8);
    for (adjacent_iterator ai(where); ai; ++ai)
        veto_spots.push_back(*ai);
    const vector<coord_def> adj_spots = veto_spots;

    if (zin)
    {
        // We need to get this now because we won't be able to see
        // the monster once the walls go up!
        targname = victim->name(DESC_THE);
        bool success = true;
        bool none_vis = true;

        // Check that any adjacent creatures can be pushed out of the way.
        for (adjacent_iterator ai(where); ai; ++ai)
        {
            // The tile is occupied.
            if (actor *act = actor_at(*ai))
            {
                // Can't push ourselves.
                vector<coord_def> push_targets = get_push_spaces(*ai, true, &veto_spots);
                if (act->is_player() || push_targets.empty())
                {
                    success = false;
                    if (you.can_see(*act))
                        none_vis = false;
                    break;
                }
                else // the new position of the monster is now an additional veto spot for monsters
                    veto_spots.push_back(push_targets.front());
            }

            // don't try to shove the orb of zot into lava and/or crash
            if (env.igrid(*ai) != NON_ITEM)
            {
                if (!has_push_spaces(*ai, false, &adj_spots))
                {
                    success = false;
                    none_vis = false;
                    break;
                }
            }

            // Make sure we have a legitimate tile.
            proceed = false;
            if (cell_is_solid(*ai) && !feat_is_opaque(env.grid(*ai)))
            {
                success = false;
                none_vis = false;
                break;
            }
        }

        if (!success)
        {
            if (none_vis)
            {
                fail_check();
                mprf("You briefly glimpse something next to %s.",
                     targname.c_str());
                return spret::success;
            }

            mprf("You need more space to imprison %s.", targname.c_str());
            return spret::abort;
        }
    }

    fail_check();

    veto_spots = adj_spots;
    for (adjacent_iterator ai(where); ai; ++ai)
    {
        // This is where power comes in.
        if (!zin && one_chance_in(pow / 3))
            continue;

        // The tile is occupied.
        if (zin && actor_at(*ai))
        {
            coord_def newpos = push_actor_from(*ai, &veto_spots, false);
            ASSERT(!newpos.origin());
            veto_spots.push_back(newpos);
        }

        // closed doors are solid, but we don't want a behaviour difference
        // between open and closed doors
        proceed = !cell_is_solid(*ai) || feat_is_door(env.grid(*ai));
        if (!zin && monster_at(*ai))
            proceed = false;

        if (proceed)
        {
            // All items are moved aside for zin, tomb just skips the tile.
            if (env.igrid(*ai) != NON_ITEM && zin)
                push_items_from(*ai, &adj_spots);

            // All traps are destroyed.
            if (trap_def *ptrap = trap_at(*ai))
            {
                ptrap->destroy();
                env.grid(*ai) = DNGN_FLOOR;
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
                tile_env.flv(*ai).feat_idx =
                        store_tilename_get_index("dngn_silver_wall");
                tile_env.flv(*ai).feat = TILE_DNGN_SILVER_WALL;
                if (env.map_knowledge(*ai).seen())
                {
                    env.map_knowledge(*ai).set_feature(DNGN_METAL_WALL);
                    env.map_knowledge(*ai).clear_item();
#ifdef USE_TILE
                    tile_env.bk_bg(*ai) = TILE_DNGN_SILVER_WALL;
                    tile_env.bk_fg(*ai) = 0;
#endif
                }
            }
            // Tomb card
            else
            {
                temp_change_terrain(*ai, DNGN_ROCK_WALL, INFINITE_DURATION,
                                    TERRAIN_CHANGE_TOMB);

                env.grid_colours(*ai) = RED;
                tile_env.flv(*ai).feat_idx =
                        store_tilename_get_index("wall_sandstone");
                tile_env.flv(*ai).feat = TILE_WALL_SANDSTONE;
                if (env.map_knowledge(*ai).seen())
                {
                    env.map_knowledge(*ai).set_feature(DNGN_ROCK_WALL);
                    env.map_knowledge(*ai).clear_item();
#ifdef USE_TILE
                    tile_env.bk_bg(*ai) = TILE_WALL_SANDSTONE;
                    tile_env.bk_fg(*ai) = 0;
#endif
                }
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
        const int tomb_duration = BASELINE_DELAY * pow;
        env.markers.add(new map_tomb_marker(where,
                                            tomb_duration,
                                            source,
                                            victim->mindex()));
        env.markers.clear_need_activate(); // doesn't need activation
    }
    else
        canned_msg(MSG_NOTHING_HAPPENS);

    return spret::success;
}

// Add 6 to this. Damage ranges from 9-12 (avg 10) at 0 invo,
// to 9-78 at 27 invo.
dice_def beogh_smiting_dice(int pow, bool allow_random)
{
    if (allow_random)
        return dice_def(3, div_rand_round(pow, 8));
    else
        return dice_def(3, pow / 8);
}

spret cast_smiting(int pow, monster* mons, bool fail)
{
    if (mons == nullptr)
    {
        fail_check();
        canned_msg(MSG_NOTHING_THERE);
        // Counts as a real cast, due to invisible monsters.
        return spret::success;
    }

    if (mons->friendly())
    {
        mpr("Beogh will not strike down an ally of the cause.");
        return spret::abort;
    }

    // The above should prevent most cases of attack prompts, but just in case...
    if (stop_attack_prompt(mons, false, you.pos()))
        return spret::abort;

    // No direct divine intervention during apostle challenges
    if (mons->has_ench(ENCH_TOUCH_OF_BEOGH))
    {
        simple_god_message(" booms: This is a trial of mortal prowess."
                           " Fight with your own strength!");
        return spret::abort;
    }

    fail_check();

    god_conduct_trigger conducts[3];
    set_attack_conducts(conducts, *mons, you.can_see(*mons));

    // damage at 0 Invo ranges from 9-12 (avg 10), to 9-72 (avg 40) at 27.
    int damage = 6 + beogh_smiting_dice(pow).roll();

    mprf("You smite %s%s",
         mons->name(DESC_THE).c_str(),
         attack_strength_punctuation(damage).c_str());

    behaviour_event(mons, ME_ANNOY, &you);
    mons->hurt(&you, damage, BEAM_MISSILE, KILLED_BY_BEOGH_SMITING);

    if (mons->alive())
    {
        print_wounds(*mons);
        you.pet_target = mons->mindex();
    }

    return spret::success;
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
    if (attacker != nullptr && attacker != mons)
        behaviour_event(mons, ME_ANNOY, attacker);

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

    for (radius_iterator ri(where, LOS_SOLID, true); ri; ++ri)
        holy_word_monsters(*ri, pow, source, attacker);

    // Sequencing so that we don't holy word a demonic guardian reacting to
    // a player reading a holy word scroll on themselves (mantis 12600).
    // (This probably can't happen anymore...?)
    holy_word_monsters(where, pow, source, attacker);
}

int torment_player(const actor *attacker, torment_source_type taux)
{
    ASSERT(!crawl_state.game_is_arena());

    int hploss = 0;

    if (!you.res_torment())
    {
        // Negative energy resistance can alleviate torment.
        hploss = max(0, you.hp * (50 - player_prot_life() * 5) / 100 - 1);
        // Statue form is only partial petrification.
        if (you.form == transformation::statue)
            hploss /= 2;
        if (you.has_mutation(MUT_TORMENT_RESISTANCE))
            hploss /= 2;
#if TAG_MAJOR_VERSION == 34
        // Save compatibility for old demonspawn mutation -- now deterministic
        if (you.has_mutation(MUT_STOCHASTIC_TORMENT_RESISTANCE))
            hploss /= 2;
#endif
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
            // Always give at least partial protection for invoked torment.
            // 24% to 80% chance for other sources.
            else if (random2(250) < you.piety || taux == TORMENT_KIKUBAAQUDGHA)
            {
                hploss -= (1 + random2(hploss - 1));
                simple_god_message(" partially shields you from torment!");
            }
        }
    }

    if (!hploss)
    {
        mpr("You feel a surge of unholy energy.");
        return 0;
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
    case TORMENT_CARD_PAIN:
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
        type = KILLED_BY_DEATH_EXPLOSION;
        aux = "an exploding lurking horror";
        break;

    case TORMENT_MISCAST:
        aux = "by torment";
        break;
    }

    ouch(hploss, type, attacker ? attacker->mid : MID_NOBODY, aux);

    return hploss;
}

int torment_cell(coord_def where, actor *attacker, torment_source_type taux)
{
    int damage = 0;

    if (where == you.pos()
        // The Sceptre of Torment and pain card do not affect the user.
        && !(attacker && attacker->is_player()
            && (taux == TORMENT_SCEPTRE || taux == TORMENT_CARD_PAIN)))
    {
        damage = torment_player(attacker, taux);
    }
    // Don't return, since you could be standing on a monster.

    monster* mons = monster_at(where);
    if (!mons
        || !mons->alive()
        || mons->res_torment()
        || attacker && never_harm_monster(attacker, *mons, true)
        // Monsters can't currently use the sceptre, but just in case.
        || attacker
           && mons == attacker->as_monster()
           && taux == TORMENT_SCEPTRE)
    {
        return damage;
    }

    god_conduct_trigger conducts[3];
    int hploss = max(0, mons->hit_points *
                        (50 - mons->res_negative_energy() * 5) / 100 - 1);

    if (hploss)
    {
        if (mons->observable())
            simple_monster_message(*mons, " convulses!");
        else if (you.see_cell(mons->pos()))
            mpr("Something is bathed in an unholy light!");

        // Currently, torment doesn't annoy the monsters it affects
        // because it can't kill them, and because hostile monsters use
        // it. It does alert them, though.
        // XXX: attacker isn't passed through "int torment()".
        behaviour_event(mons, ME_ALERT, attacker);

        if (attacker && attacker->is_player())
        {
            bool set_conducts = false;
            switch (taux)
            {
                case TORMENT_SCROLL:
                    set_conducts = item_type_known(OBJ_SCROLLS, SCR_TORMENT);
                    break;
                case TORMENT_SCEPTRE:
                    set_conducts = true;
                    break;
                default: break;
            }

            if (set_conducts)
                set_attack_conducts(conducts, *mons, you.can_see(*mons));
        }
    }

    // Player torment annoys the monsters it affects
    // Tolerate unknown scroll, to not annoy ally god users too much.
    if (attacker != nullptr
        && attacker->is_player()
        && (taux != TORMENT_SCROLL
            || item_type_known(OBJ_SCROLLS, SCR_TORMENT)))
    {
        behaviour_event(mons, ME_ANNOY, attacker);
    }

    mons->hurt(attacker, hploss, BEAM_TORMENT_DAMAGE);
    damage += hploss;

    return damage;
}

void torment(actor *attacker, torment_source_type taux, const coord_def& where)
{
    for (radius_iterator ri(where, LOS_NO_TRANS); ri; ++ri)
        torment_cell(*ri, attacker, taux);
}

void setup_cleansing_flame_beam(bolt &beam, int pow,
                                cleansing_flame_source caster,
                                coord_def where, actor *attacker)
{
    beam.flavour      = BEAM_HOLY;
    beam.glyph        = dchar_glyph(DCHAR_FIRED_BURST);
    beam.damage       = dice_def(2, pow);
    beam.target       = where;
    beam.name         = "golden flame";
    beam.colour       = YELLOW;
    beam.aux_source   = (caster == cleansing_flame_source::tso)
                        ? "the Shining One's cleansing flame"
                        : "cleansing flame";
    beam.ex_size      = 2;
    beam.is_explosion = true;

    if (caster == cleansing_flame_source::generic
        || caster == cleansing_flame_source::tso)
    {
        beam.thrower   = KILL_NON_ACTOR;
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

void cleansing_flame(int pow, cleansing_flame_source caster, coord_def where,
                     actor *attacker)
{
    bolt beam;
    setup_cleansing_flame_beam(beam, pow, caster, where, attacker);
    beam.explode(true, caster != cleansing_flame_source::tso);
}

void majin_bo_vampirism(monster &mon, int damage)
{
    if (!you.unrand_equipped(UNRAND_MAJIN) || crawl_state.is_god_acting())
        return;

    dprf("Majin bo might trigger, dam: %d.", damage);

    if (damage < 1 || !actor_is_susceptible_to_vampirism(mon)
        || you.hp == you.hp_max || you.duration[DUR_DEATHS_DOOR])
    {
        return;
    }

    int hp_boost = 1 + random2(damage);
    hp_boost = resist_adjust_damage(&mon, BEAM_NEG, hp_boost);

    if (hp_boost)
    {
        canned_msg(MSG_GAIN_HEALTH);
        inc_hp(hp_boost);
    }
}

/**
 * Handle the dreamshard necklace.
 **/
void dreamshard_shatter()
{
    ASSERT(you.unrand_equipped(UNRAND_DREAMSHARD_NECKLACE));
    for (player_equip_entry& entry: you.equipment.items)
    {
        if (entry.slot != SLOT_AMULET)
            continue;
        item_def& item = entry.get_item();
        if (is_unrandom_artefact(item, UNRAND_DREAMSHARD_NECKLACE))
        {
            you.equipment.remove(item);
            item.unrand_idx = UNRAND_DREAMDUST_NECKLACE;
            you.equipment.add(item, SLOT_AMULET);
            break;
        }
    }
    mpr("Your necklace shatters, unleashing a wave of protective dreams!");
    mark_milestone("dreamshard", "was saved by the dreamshard necklace!");
    take_note(NOTE_DREAMSHARD);

    for (int i = 0; i < 5; i++)
    {
        flash_view(UA_PLAYER, random_uncommon_colour());
        scaled_delay(200);
    }

    // Don't die until your next turn.
    you.duration[DUR_POISONING] = 0;
    set_hp(1);
    you.props[DREAMSHARD_KEY] = true;
    vector<string> dreams = {"life"};

    if (!you.allies_forbidden())
    {
        const int num = 2 + random2(4);
        int created = 0;
        for (int i = 0; i < num; ++i)
        {
            mgen_data mg(RANDOM_COMPATIBLE_MONSTER, BEH_FRIENDLY, you.pos(),
                         MHITYOU, MG_FORCE_BEH | MG_AUTOFOE | MG_NO_OOD);
            mg.set_summoned(&you, MON_SUMM_AID, summ_dur(4));
            if (create_monster(mg))
                ++created;
        }

        if (created)
            dreams.push_back("friendship");
    }

    if (!(env.level_state & LSTATE_STILL_WINDS))
    {
        dreams.push_back("clouds");
        big_cloud(CLOUD_FLUFFY, &you, you.pos(), 50, 8 + random2(8));
    }

    mpr_comma_separated_list("You dream of ", dreams);

    // when dreams spill out into reality it wakes you up
    // put it here after the dream message so that a sleeping player who
    // gets dreamsharded gets a nice message order
    you.wake_up();
}
