/**
 * @file
 * @brief Traps related functions.
**/

#include "AppHdr.h"

#include "traps.h"
#include "trap-def.h"

#include <algorithm>
#include <cmath>

#include "areas.h"
#include "art-enum.h"
#include "bloodspatter.h"
#include "branch.h"
#include "cloud.h"
#include "coordit.h"
#include "delay.h"
#include "describe.h"
#include "dungeon.h"
#include "english.h"
#include "god-passive.h" // passive_t::avoid_traps
#include "hints.h"
#include "item-prop.h"
#include "items.h"
#include "libutil.h"
#include "mapmark.h"
#include "mon-cast.h" // recall for zot traps
#include "mon-enum.h"
#include "mon-tentacle.h"
#include "mon-util.h"
#include "message.h"
#include "mon-place.h"
#include "nearby-danger.h"
#include "orb.h"
#include "player-stats.h" // lose_stat for zot traps
#include "random.h"
#include "religion.h"
#include "shout.h"
#include "spl-damage.h" // cancel_polar_vortex
#include "spl-transloc.h"
#include "spl-summoning.h"
#include "stash.h"
#include "state.h"
#include "stringutil.h"
#include "tag-version.h"
#include "teleport.h"
#include "terrain.h"
#include "travel.h"
#include "xom.h"

static string _net_immune_reason()
{
    if (player_equip_unrand(UNRAND_SLICK_SLIPPERS))
        return "You slip through the net.";
    return "";
}

static const string TRAP_PROJECTILE_KEY = "trap_projectile";

bool trap_def::active() const
{
    return type != TRAP_UNASSIGNED;
}

void trap_def::destroy(bool known)
{
    if (!in_bounds(pos))
        die("Trap position out of bounds!");

    env.grid(pos) = DNGN_FLOOR;
    if (known)
    {
        env.map_knowledge(pos).set_feature(DNGN_FLOOR);
        StashTrack.update_stash(pos);
    }
    env.trap.erase(pos);
}

void trap_def::prepare_ammo(int charges)
{
    if (charges)
    {
        ammo_qty = charges;
        return;
    }
    switch (type)
    {
    case TRAP_GOLUBRIA:
        // really, time until it vanishes
        ammo_qty = 10 + random2(10);
        break;
    case TRAP_TELEPORT:
        ammo_qty = 1;
        break;
    default:
        ammo_qty = 0;
        break;
    }
}

void trap_def::reveal()
{
    env.grid(pos) = feature();
}

string trap_def::name(description_level_type desc) const
{
    if (type >= NUM_TRAPS)
        return "buggy";

    string basename = full_trap_name(type);
    if (desc == DESC_A)
    {
        string prefix = "a";
        if (is_vowel(basename[0]))
            prefix += 'n';
        prefix += ' ';
        return prefix + basename;
    }
    else if (desc == DESC_THE)
        return string("the ") + basename;
    else                        // everything else
        return basename;
}

bool trap_def::is_bad_for_player() const
{
    return type == TRAP_ALARM
           || type == TRAP_DISPERSAL
           || type == TRAP_ZOT
           || type == TRAP_NET
           || type == TRAP_PLATE;
}

bool trap_def::is_safe(actor* act) const
{
    if (!act)
        act = &you;

    // TODO: For now, just assume they're safe; they don't damage outright,
    // and the messages get old very quickly
    if (type == TRAP_WEB) // && act->is_web_immune()
        return true;

#if TAG_MAJOR_VERSION == 34
    if (is_removed_trap(type))
        return true;
#endif

    if (!act->is_player())
        return is_bad_for_player();

    // No prompt (teleport traps are ineffective if wearing a -Tele item)
    if ((type == TRAP_TELEPORT || type == TRAP_TELEPORT_PERMANENT)
        && you.no_tele())
    {
        return true;
    }

    if (type == TRAP_GOLUBRIA || type == TRAP_SHAFT)
        return true;

    if (type == TRAP_NET && !_net_immune_reason().empty())
        return true;

    // Let players specify traps as safe via lua.
    if (clua.callbooleanfn(false, "c_trap_is_safe", "s", trap_name(type).c_str()))
        return true;

    return false;
}

/**
 * Get the item index of the first net on the square.
 *
 * @param where The location.
 * @param trapped If true, the index of the stationary net (trapping a victim)
 *                is returned.
 * @return  The item index of the net.
*/
int get_trapping_net(const coord_def& where, bool trapped)
{
    for (stack_iterator si(where); si; ++si)
    {
        if (si->is_type(OBJ_MISSILES, MI_THROWING_NET)
            && (!trapped || item_is_stationary_net(*si)))
        {
            return si->index();
        }
    }
    return NON_ITEM;
}

/**
 * Return a string describing the reason a given actor is ensnared. (Since nets
 * & webs use the same status.
 *
 * @param actor     The ensnared actor.
 * @return          Either 'held in a net' or 'caught in a web'.
 */
const char* held_status(actor *act)
{
    act = act ? act : &you;

    if (get_trapping_net(act->pos(), true) != NON_ITEM)
        return "held in a net";
    else
        return "caught in a web";
}

static void _mark_net_trapping(const coord_def& where)
{
    int net = get_trapping_net(where);
    if (net == NON_ITEM)
    {
        net = get_trapping_net(where, false);
        if (net != NON_ITEM)
            maybe_split_nets(env.item[net], where);
    }
}

/**
 * Attempt to trap a monster in a net.
 *
 * @param mon       The monster being trapped.
 * @return          Whether the monster was successfully trapped.
 */
bool monster_caught_in_net(monster* mon)
{
    if (mon->is_insubstantial() || mon->is_amorphous())
    {
        if (you.can_see(*mon))
        {
            if (mon->is_insubstantial())
            {
                mprf("The net passes right through %s!",
                     mon->name(DESC_THE).c_str());
            }
            else
            {
                mprf("%s effortlessly oozes through the net!",
                     mon->name(DESC_THE).c_str());
            }
        }
        return false;
    }

    monster_info mi(mon);
    if (mi.net_immune())
    {
        if (you.see_cell(mon->pos()))
        {
            if (mon->visible_to(&you))
            {
                mprf("The net is caught on %s!",
                     mon->name(DESC_THE).c_str());
            }
            else
                mpr("The net is caught on something unseen!");
        }
        return false;
    }

    if (!mon->caught() && mon->add_ench(ENCH_HELD))
    {
        if (you.see_cell(mon->pos()))
        {
            if (!mon->visible_to(&you))
                mpr("Something gets caught in the net!");
            else
                simple_monster_message(*mon, " is caught in the net!");
        }
        return true;
    }

    return false;
}

bool player_caught_in_net()
{
    if (!_net_immune_reason().empty())
        return false;

    if (you.attribute[ATTR_HELD])
        return false;

    mpr("You become entangled in the net!");
    stop_running();

    // Set the attribute after the mpr, otherwise the screen updates
    // and we get a glimpse of a web because there isn't a trapping net
    // item yet
    you.attribute[ATTR_HELD] = 1;
    you.redraw_evasion = true;

    stop_delay(true); // even stair delays
    return true;
}

void check_net_will_hold_monster(monster* mons)
{
    ASSERT(mons); // XXX: should be monster &mons
    if (mons->is_insubstantial() || mons->is_amorphous())
    {
        const int net = get_trapping_net(mons->pos());
        if (net != NON_ITEM)
            free_stationary_net(net);

        if (mons->is_insubstantial())
        {
            simple_monster_message(*mons,
                                   " drifts right through the net!");
        }
        else
            simple_monster_message(*mons,
                                   " oozes right through the net!");
    }
    else
        mons->add_ench(ENCH_HELD);
}

static bool _player_caught_in_web()
{
    if (you.attribute[ATTR_HELD])
        return false;

    you.attribute[ATTR_HELD] = 1;

    you.redraw_armour_class = true;
    you.redraw_evasion      = true;
    quiver::set_needs_redraw();

    // No longer stop_running() and stop_delay().
    return true;
}

vector<coord_def> find_golubria_on_level()
{
    vector<coord_def> ret;
    for (rectangle_iterator ri(coord_def(0, 0), coord_def(GXM-1, GYM-1)); ri; ++ri)
    {
        trap_def *trap = trap_at(*ri);
        if (trap && trap->type == TRAP_GOLUBRIA)
            ret.push_back(*ri);
    }
    return ret;
}

enum class passage_type
{
    free,
    blocked,
    none,
};

static passage_type _find_other_passage_side(coord_def& to)
{
    vector<coord_def> clear_passages;
    bool has_blocks = false;
    for (coord_def passage : find_golubria_on_level())
    {
        if (passage != to)
        {
            if (!actor_at(passage))
                clear_passages.push_back(passage);
            else
                has_blocks = true;
        }
    }
    const int choices = clear_passages.size();
    if (choices < 1)
        return has_blocks ? passage_type::blocked : passage_type::none;
    to = clear_passages[random2(choices)];
    return passage_type::free;
}

// Table of possible Zot trap effects as pairs with weights.
// 2/3 are "evil magic", 1/3 are "summons"
static const vector<pair<function<void ()>, int>> zot_effects = {
    { [] { lose_stat(STAT_RANDOM, 1 + random2avg(5, 2)); }, 4 },
    { [] { contaminate_player(7000 + random2avg(13000, 2), false); }, 4 },
    { [] { you.paralyse(nullptr, 2 + random2(4), "a Zot trap"); }, 1 },
    { [] { drain_mp(you.magic_points); canned_msg(MSG_MAGIC_DRAIN); }, 2 },
    { [] { you.petrify(nullptr); }, 1 },
    { [] { you.increase_duration(DUR_LOWERED_WL, 5 + random2(15), 20,
                "Your willpower is stripped away!"); }, 4 },
    { [] { mons_word_of_recall(nullptr, 2 + random2(3)); }, 3 },
    { [] {
              mgen_data mg = mgen_data::hostile_at(RANDOM_DEMON_GREATER,
                                                   true, you.pos());
              mg.set_summoned(nullptr, MON_SUMM_ZOT);
              mg.set_non_actor_summoner("a Zot trap");
              mg.extra_flags |= (MF_NO_REWARD | MF_HARD_RESET);
              if (create_monster(mg))
                  mpr("You sense a hostile presence.");
         }, 3 },
    { [] {
             coord_def pt = find_gateway_location(&you);
             if (pt != coord_def(0, 0))
                 create_malign_gateway(pt, BEH_HOSTILE, "a Zot trap", 150);
         }, 1 },
    { [] {
              mgen_data mg = mgen_data::hostile_at(MONS_TWISTER,
                                                   false, you.pos());
              mg.set_summoned(nullptr, MON_SUMM_ZOT, summ_dur(2));
              mg.set_non_actor_summoner("a Zot trap");
              mg.extra_flags |= (MF_NO_REWARD | MF_HARD_RESET);
              if (create_monster(mg))
                  mpr("A huge vortex of air appears!");
         }, 1 },
};

// Zot traps only target the player. This rolls their effect.
static void _zot_trap()
{
    mpr("The power of Zot is invoked against you!");
    (*random_choose_weighted(zot_effects))();
}

void trap_def::trigger(actor& triggerer)
{
    const bool you_trigger = triggerer.is_player();

    // Traps require line of sight without blocking translocation effects.
    // Requiring LOS prevents monsters from dispersing out of vaults that have
    // teleport traps available for the player to utilize. Additionally
    // requiring LOS_NO_TRANS prevents vaults that feature monsters with trap
    // behind glass from spamming the message log with irrelevant events.
    if (!you.see_cell_no_trans(pos))
        return;

    // If set, the trap will be removed at the end of the
    // triggering process.
    bool trap_destroyed = false, know_trap_destroyed = false;

    monster* m = triggerer.as_monster();

    // Intelligent monsters native to a branch get a bonus avoiding traps
    const bool trig_smart = m
        && mons_is_native_in_branch(*m)
        && mons_intel(*m) >= I_HUMAN;

    // Smarter monsters and those native to the level will simply
    // side-step shafts. Unless they are already looking for
    // an exit, of course.
    if (type == TRAP_SHAFT
        && m
        && (!m->will_trigger_shaft()
            || trig_smart && !mons_is_fleeing(*m) && !m->pacified()))
    {
        return;
    }

    // Tentacles aren't real monsters, and shouldn't invoke traps.
    if (m && mons_is_tentacle_or_tentacle_segment(m->type))
        return;

    // Store the position now in case it gets cleared in between.
    const coord_def p(pos);

    switch (type)
    {
    case TRAP_GOLUBRIA:
    {
        coord_def to = p;
        passage_type search_result = _find_other_passage_side(to);
        if (search_result == passage_type::free)
        {
            if (you_trigger)
            {
                mpr("You enter the passage of Golubria.");
                cancel_polar_vortex();
            }
            else
                simple_monster_message(*m, " enters the passage of Golubria.");

            // Should always be true.
            bool moved = triggerer.move_to_pos(to);
            ASSERT(moved);

            place_cloud(CLOUD_TLOC_ENERGY, p, 1 + random2(3), &triggerer);
            trap_destroyed = true;
            know_trap_destroyed = you_trigger;
            if (you_trigger)
                id_floor_items();
        }
        else if (you_trigger)
        {
            mprf("This passage %s!", search_result == passage_type::blocked ?
                 "seems to be blocked by something" : "doesn't lead anywhere");
        }
        break;
    }
    case TRAP_DISPERSAL:
    {
        dprf("Triggered dispersal.");
        if (you_trigger)
            mprf("You enter %s!", name(DESC_A).c_str());
        else
            mprf("%s enters %s!", triggerer.name(DESC_THE).c_str(),
                    name(DESC_A).c_str());
        mid_t triggerer_mid = triggerer.mid;
        apply_visible_monsters([triggerer_mid] (monster& mons) {
                return (mons.mid != triggerer_mid) && !mons.no_tele() && monster_blink(&mons);
            }, pos);
        if (!you_trigger && you.see_cell_no_trans(pos))
        {
            you.blink();
            if (!you.no_tele(true))
                interrupt_activity(activity_interrupt::teleport);
        }
        // Don't chain disperse
        triggerer.blink();
        break;
    }
    case TRAP_TELEPORT:
    case TRAP_TELEPORT_PERMANENT:
        if (you_trigger)
            mprf("You enter %s!", name(DESC_A).c_str());
        if (ammo_qty > 0 && !--ammo_qty)
        {
            // can't use trap_destroyed, as we might recurse into a shaft
            // or be banished by a Zot trap
            env.map_knowledge(pos).set_feature(DNGN_FLOOR);
            mprf("%s disappears.", name(DESC_THE).c_str());
            destroy();
        }
        if (!triggerer.no_tele())
            triggerer.teleport(true);
        break;

    case TRAP_ALARM:
        // Alarms always mark the player, but not through glass
        // The trap gets destroyed to prevent the player from abusing an alarm
        // trap found in favourable terrain.
        if (!you.see_cell_no_trans(pos))
            break;
        trap_destroyed = true;
        if (you_trigger)
            mpr("You set off the alarm!");
        else
            mprf("%s %s the alarm!", triggerer.name(DESC_THE).c_str(),
                 mons_intel(*m) >= I_HUMAN ? "pulls" : "sets off");

        if (silenced(pos))
        {
            mprf("%s vibrates slightly, failing to make a sound.",
                 name(DESC_THE).c_str());
        }
        else
        {
            string msg = make_stringf("%s emits a blaring wail!",
                               name(DESC_THE).c_str());
            noisy(40, pos, msg.c_str(), triggerer.mid);
        }

        you.sentinel_mark(true);
        break;

    case TRAP_NET:
        {
        // Nets need LOF to hit the player, no netting through glass.
        if (!you.see_cell_no_trans(pos))
            break;
        // Don't try to re-net the player when they're already netted/webbed.
        if (you.attribute[ATTR_HELD])
            break;

        bool triggered = you_trigger;
        if (m)
        {
            if (mons_intel(*m) < I_HUMAN || !one_chance_in(3))
            {
                // Not triggered, trap stays.
                simple_monster_message(*m, " fails to trigger a net trap.");
            }
            else
            {
                // Triggered, net the player.
                triggered = true;

                if (!simple_monster_message(*m,
                                            " drops a net on you."))
                {
                    mpr("Something launches a net on you.");
                }
            }
        }

        if (!triggered)
            break;

        if (random2avg(2 * you.evasion(), 2) > 18 + env.absdepth0 / 2)
        {
            mpr("You avoid being caught in a net.");
            break;
        }

        if (!player_caught_in_net())
        {
            if (!m) // no message already printed
                mpr("You trigger the net trap.");
            const string reason = _net_immune_reason();
            if (!reason.empty())
                mpr(reason);
            break;
        }

        item_def item;
        item.base_type = OBJ_MISSILES;
        item.sub_type  = MI_THROWING_NET;
        item.quantity  = 1;
        set_item_ego_type(item, OBJ_MISSILES, SPMSL_NORMAL);
        item.props[TRAP_PROJECTILE_KEY] = true;
        item_colour(item);

        copy_item_to_grid(item, you.pos());
        if (player_in_a_dangerous_place())
            xom_is_stimulated(50);

        // Mark the item as trapping; after this it's
        // safe to update the view.
        _mark_net_trapping(you.pos());
        break;
        }

    case TRAP_WEB:
        if (triggerer.is_web_immune())
        {
            if (m)
            {
                if (m->is_insubstantial())
                    simple_monster_message(*m, " passes through a web.");
                else if (m->is_amorphous())
                    simple_monster_message(*m, " oozes through a web.");
                // too spammy for spiders, and expected
            }
            break;
        }

        if (you_trigger)
        {
            if (one_chance_in(3))
                mpr("You pick your way through the web.");
            else
            {
                mpr("You are caught in the web!");

                if (_player_caught_in_web() && player_in_a_dangerous_place())
                    xom_is_stimulated(50);
            }
        }
        else if (m)
        {
            if (one_chance_in(3) || (trig_smart && coinflip()))
                simple_monster_message(*m, " evades a web.");
            else
            {
                if (m->visible_to(&you))
                    simple_monster_message(*m, " is caught in a web!");
                else
                    mpr("A web moves frantically as something is caught in it!");

                // If somehow already caught, make it worse.
                m->add_ench(ENCH_HELD);

                // Don't try to escape the web in the same turn
                m->props[NEWLY_TRAPPED_KEY] = true;
            }
        }
        break;

    case TRAP_ZOT:
        if (you_trigger)
        {
            mpr("You enter the Zot trap.");
            _zot_trap();
        }
        else if (m)
        {
            // Zot traps are out to get *the player*! Hostile monsters
            // benefit and friendly monsters bring effects down on
            // the player. Such is life.

            // Give the player a chance to figure out what happened
            if (player_can_hear(pos))
                mprf(MSGCH_SOUND, "You hear a loud \"Zot\"!");

            if (you.see_cell_no_trans(pos) && one_chance_in(5))
                _zot_trap();
        }
        break;

    case TRAP_SHAFT:
        // Known shafts don't trigger as traps.
        // Allies don't fall through shafts (no herding!)
        if (trig_smart || (m && m->wont_attack()) || you_trigger)
            break;

        // A chance to escape.
        if (one_chance_in(4))
            break;

        {
        // keep this for messaging purposes
        const bool triggerer_seen = you.can_see(triggerer);
        const bool triggerer_was_invisible_monster = m && m->has_ench(ENCH_INVIS) && !m->friendly();

        // Fire away!
        triggerer.do_shaft();

        // Player-used shafts are destroyed
        // after one use in down_stairs()
        if (!you_trigger)
        {
            mprf("%s shaft crumbles and collapses.",
                 triggerer_seen ? "The" : "A");
            know_trap_destroyed = true;
            trap_destroyed = true;

            // If we shaft an invisible monster reactivate autopickup.
            // We need to check for actual invisibility rather than
            // whether we can see the monster. There are several edge
            // cases where a monster is visible to the player but we
            // still need to turn autopickup back on, such as
            // TSO's halo or sticky flame.
            if (triggerer_was_invisible_monster)
                autotoggle_autopickup(false);
        }
        }
        break;

    case TRAP_PLATE:
        dungeon_events.fire_position_event(DET_PRESSURE_PLATE, pos);
        break;

    default:
#if TAG_MAJOR_VERSION == 34
        if (is_removed_trap(type))
        {
            mpr("The trap seems to be inoperative.");
            trap_destroyed = true;
        }
#endif
        break;
    }

    if (trap_destroyed)
        destroy(know_trap_destroyed);
}

void destroy_trap(const coord_def& pos)
{
    if (trap_def* ptrap = trap_at(pos))
        ptrap->destroy();
}

trap_def* trap_at(const coord_def& pos)
{
    if (!feat_is_trap(env.grid(pos)))
        return nullptr;

    auto it = env.trap.find(pos);
    ASSERT(it != env.trap.end());
    ASSERT(it->second.pos == pos);
    ASSERT(it->second.type != TRAP_UNASSIGNED);

    return &it->second;
}

trap_type get_trap_type(const coord_def& pos)
{
    if (trap_def* ptrap = trap_at(pos))
        return ptrap->type;

    return TRAP_UNASSIGNED;
}

/**
 * End the ATTR_HELD state & redraw appropriate UI.
 *
 * Do NOT call without clearing up nets, webs, etc first!
 */
void stop_being_held()
{
    you.attribute[ATTR_HELD] = 0;
    quiver::set_needs_redraw();
    you.redraw_evasion = true;
}

/**
 * Exit a web that's currently holding you.
 *
 * @param quiet     Whether to squash messages.
 */
void leave_web(bool quiet)
{
    const trap_def *trap = trap_at(you.pos());
    if (!trap || trap->type != TRAP_WEB)
        return;

    if (trap->ammo_qty == 1) // temp web from e.g. jumpspider/spidersack
    {
        if (!quiet)
            mpr("The web tears apart.");
        destroy_trap(you.pos());
    }
    else if (!quiet)
        mpr("You disentangle yourself.");

    stop_being_held();
}

/**
 * Let the player attempt to unstick themself from a web.
 */
static void _free_self_from_web()
{
    // Check if there's actually a web trap in your tile.
    trap_def *trap = trap_at(you.pos());
    if (trap && trap->type == TRAP_WEB)
    {
        // if so, roll a chance to escape the web.
        if (x_chance_in_y(3, 10))
        {
            mpr("You struggle to detach yourself from the web.");
            // but you actually accomplished nothing!
            return;
        }

        leave_web();
    }

    // whether or not there was a web trap there, you're free now.
    stop_being_held();
}

void free_self_from_net()
{
    const int net = get_trapping_net(you.pos());

    if (net == NON_ITEM)
    {
        // If there's no net, it must be a web.
        _free_self_from_web();
        return;
    }

    int hold = env.item[net].net_durability;
    dprf("net.net_durability: %d", hold);

    const int damage = 1 + random2(4);

    hold -= damage;
    env.item[net].net_durability = hold;

    if (hold < NET_MIN_DURABILITY)
    {
        mprf("You %s the net and break free!", damage > 3 ? "shred" : "rip");

        destroy_item(net);
        stop_being_held();
        return;
    }

    if (damage > 3)
        mpr("You tear a large gash into the net.");
    else
        mpr("You struggle against the net.");
}

/**
 * Deals with messaging & cleanup for temporary web traps. Does not actually
 * delete ENCH_HELD!
 *
 * @param mons      The monster leaving a web.
 * @param quiet     Whether to suppress messages.
 */
void monster_web_cleanup(const monster &mons, bool quiet)
{
    trap_def *trap = trap_at(mons.pos());
    if (trap && trap->type == TRAP_WEB)
    {
        if (trap->ammo_qty == 1)
        {
            // temp web from e.g. jumpspider/spidersack
            if (!quiet)
                simple_monster_message(mons, " tears the web.");
            destroy_trap(mons.pos());
        }
        else if (!quiet)
            simple_monster_message(mons, " pulls away from the web.");
    }
}

void mons_clear_trapping_net(monster* mon)
{
    if (!mon->caught())
        return;

    const int net = get_trapping_net(mon->pos());
    if (net != NON_ITEM)
        free_stationary_net(net);

    mon->del_ench(ENCH_HELD, true);
}

void free_stationary_net(int item_index)
{
    item_def &item = env.item[item_index];
    if (!item.is_type(OBJ_MISSILES, MI_THROWING_NET))
        return;

    const coord_def pos = item.pos;
    // Probabilistically mulch net based on damage done, otherwise
    // reset damage counter (ie: item.net_durability).
    const bool mulch = item.props.exists(TRAP_PROJECTILE_KEY)
                    || x_chance_in_y(-item.net_durability, 9);
    if (mulch)
        destroy_item(item_index);
    else
    {
        item.net_durability = 0;
        item.net_placed = false;
    }

    // Make sure we don't leave a bad trapping net in the stash
    // FIXME: may leak info if a monster escapes an out-of-sight net.
    StashTrack.update_stash(pos);
    StashTrack.unmark_trapping_nets(pos);
}

void clear_trapping_net()
{
    if (!you.attribute[ATTR_HELD])
        return;

    if (!in_bounds(you.pos()))
        return;

    const int net = get_trapping_net(you.pos());
    if (net == NON_ITEM)
        leave_web(true);
    else
        free_stationary_net(net);

    stop_being_held();
}

dungeon_feature_type trap_def::feature() const
{
    return trap_feature(type);
}

dungeon_feature_type trap_feature(trap_type type)
{
    switch (type)
    {
    case TRAP_WEB:
        return DNGN_TRAP_WEB;
    case TRAP_SHAFT:
        return DNGN_TRAP_SHAFT;
    case TRAP_DISPERSAL:
        return DNGN_TRAP_DISPERSAL;
    case TRAP_TELEPORT:
        return DNGN_TRAP_TELEPORT;
    case TRAP_TELEPORT_PERMANENT:
        return DNGN_TRAP_TELEPORT_PERMANENT;
    case TRAP_ALARM:
        return DNGN_TRAP_ALARM;
    case TRAP_ZOT:
        return DNGN_TRAP_ZOT;
    case TRAP_GOLUBRIA:
        return DNGN_PASSAGE_OF_GOLUBRIA;
#if TAG_MAJOR_VERSION == 34
    case TRAP_SHADOW:
        return DNGN_TRAP_SHADOW;
    case TRAP_SHADOW_DORMANT:
        return DNGN_TRAP_SHADOW_DORMANT;
    case TRAP_ARROW:
        return DNGN_TRAP_ARROW;
    case TRAP_SPEAR:
        return DNGN_TRAP_SPEAR;
    case TRAP_BLADE:
        return DNGN_TRAP_BLADE;
    case TRAP_DART:
        return DNGN_TRAP_DART;
    case TRAP_BOLT:
        return DNGN_TRAP_BOLT;
#endif
    case TRAP_NET:
        return DNGN_TRAP_NET;
    case TRAP_PLATE:
        return DNGN_TRAP_PLATE;

#if TAG_MAJOR_VERSION == 34
    case TRAP_NEEDLE:
    case TRAP_GAS:
        return DNGN_TRAP_MECHANICAL;
#endif

    default:
        die("placeholder trap type %d used", type);
    }
}

trap_type trap_type_from_feature(dungeon_feature_type type)
{
    switch (type)
    {
    case DNGN_TRAP_WEB:
        return TRAP_WEB;
    case DNGN_TRAP_SHAFT:
        return TRAP_SHAFT;
    case DNGN_TRAP_DISPERSAL:
        return TRAP_DISPERSAL;
    case DNGN_TRAP_TELEPORT:
        return TRAP_TELEPORT;
    case DNGN_TRAP_TELEPORT_PERMANENT:
        return TRAP_TELEPORT_PERMANENT;
    case DNGN_TRAP_ALARM:
        return TRAP_ALARM;
    case DNGN_TRAP_ZOT:
        return TRAP_ZOT;
    case DNGN_PASSAGE_OF_GOLUBRIA:
        return TRAP_GOLUBRIA;
    case DNGN_TRAP_NET:
        return TRAP_NET;
    case DNGN_TRAP_PLATE:
        return TRAP_PLATE;
    default:
        return TRAP_UNASSIGNED;
    }
}

/***
 * Can a shaft be placed on the current level?
 *
 * @param respect_brflags Whether brflag::no_shafts should be factored in.
 * @returns true if such a shaft can be placed.
 */
bool is_valid_shaft_level(bool respect_brflags)
{
    // Important: We are sometimes called before the level has been loaded
    // or generated, so should not depend on properties of the level itself,
    // but only on its level_id.
    const level_id place = level_id::current();
    if (crawl_state.game_is_sprint())
        return false;

    if (!is_connected_branch(place))
        return false;

    const Branch &branch = branches[place.branch];

    if (respect_brflags && branch.branch_flags & brflag::no_shafts)
        return false;

    // Don't allow shafts from the bottom of a branch.
    return (brdepth[place.branch] - place.depth) >= 1;
}

static bool& _shafted_in(const Branch &branch)
{
    return you.props[make_stringf("shafted_in_%s", branch.abbrevname)].get_bool();
}

/// Mark the player as having been shafted in the current branch.
void set_shafted()
{
    _shafted_in(branches[you.where_are_you]) = true;
}

/**
 * Can we force shaft the player from this level?
 *
 * @returns true if we can.
 */
static bool _is_valid_shaft_effect_level()
{
    const level_id place = level_id::current();
    const Branch &branch = branches[place.branch];

    // Don't shaft the player when we can't, or when we already did once this game
    // in this branch, or when it would be into a dangerous end.
    return is_valid_shaft_level()
           && !_shafted_in(branch)
           && !(branch.branch_flags & brflag::dangerous_end
                && brdepth[place.branch] - place.depth == 1);
}

/***
 * The player rolled a new tile, see if they deserve to be trapped.
 */
void roll_trap_effects()
{
    int trap_rate = trap_rate_for_place();

    you.trapped = you.num_turns
        && env.density > 0 // can happen with builder in debug state
        && (you.trapped || x_chance_in_y(trap_rate, 9 * env.density));
}

static string _malev_msg()
{
    return make_stringf("A malevolent force fills %s...",
                        branches[you.where_are_you].longname);
}

static void _print_malev()
{
    mpr(_malev_msg());
}

/**
 * Separate from roll_trap_effects so the trap triggers when crawl is in an
 * appropriate state
 */
void do_trap_effects()
{
    if (crawl_state.game_is_descent())
        return;

    // Try to shaft, teleport, or alarm the player.

    // We figure out which possibilities are allowed before picking which happens
    // so that the overall chance of being trapped doesn't depend on which
    // possibilities are allowed.

    // Teleport effects are allowed everywhere, no need to check
    vector<trap_type> available_traps = { TRAP_TELEPORT };
    // Don't shaft the player when shafts aren't allowed in the location or when
    //  it would be into a dangerous end.
    if (_is_valid_shaft_effect_level() && you.shaftable())
        available_traps.push_back(TRAP_SHAFT);
    // No alarms on the first 3 floors
    if (env.absdepth0 > 3)
        available_traps.push_back(TRAP_ALARM);

    switch (*random_iterator(available_traps))
    {
        case TRAP_SHAFT:
            dprf("Attempting to shaft player.");
            _print_malev();
            if (have_passive(passive_t::avoid_traps))
            {
                simple_god_message(" reveals a hidden shaft just before you would have fallen in.");
                return;
            }
            if (you.do_shaft())
                set_shafted();
            break;

        case TRAP_ALARM:
            // Alarm effect alarms are always noisy, even if the player is
            // silenced, to avoid "travel only while silenced" behaviour.
            // XXX: improve messaging to make it clear there's a wail outside of the
            // player's silence
            _print_malev();
            if (have_passive(passive_t::avoid_traps))
            {
                simple_god_message(" reveals an alarm trap just before you would have tripped it.");
                return;
            }
            mpr("With a horrendous wail, an alarm goes off!");
            fake_noisy(40, you.pos());
            you.sentinel_mark(true);
            break;

        case TRAP_TELEPORT:
        {
            string msg = make_stringf("%s and a teleportation trap "
                                      "spontaneously manifests!",
                                      _malev_msg().c_str());
            if (have_passive(passive_t::avoid_traps))
            {
                mpr(msg);
                simple_god_message(" warns you in time for you to avoid it.");
                return;
            }
            you_teleport_now(false, true, msg);
            break;
        }

        // Other cases shouldn't be possible, but having a default here quiets
        // compiler warnings
        default:
            break;
    }

    learned_something_new(HINT_MALEVOLENCE);
}

level_id generic_shaft_dest(level_id place)
{
    if (!is_connected_branch(place))
        return place;

    int curr_depth = place.depth;
    int max_depth = brdepth[place.branch];

    // Shafts drop you 1/2/3 levels with equal chance.
    // 33.3% for 1, 2, 3 from D:3, less before
    place.depth += 1 + random2(min(place.depth, 3));

    // In descent, instead always drop one floor. Too brutal otherwise.
    if (crawl_state.game_is_descent())
        place.depth = curr_depth + 1;

    if (place.depth > max_depth)
        place.depth = max_depth;

    if (place.depth == curr_depth)
        return place;

    // Only shafts on the level immediately above a dangerous branch
    // bottom will take you to that dangerous bottom.
    if (branches[place.branch].branch_flags & brflag::dangerous_end
        && place.depth == max_depth
        && (max_depth - curr_depth) > 1)
    {
        place.depth--;
    }

    return place;
}

/**
 * Get the trap effect rate for the current level.
 *
 * No traps effects occur in either Temple or disconnected branches other than
 * Pandemonium. For other branches, this value starts at 1. It is increased for
 * deeper levels; by one for every 10 levels of absdepth,
 * capping out at max 9.
 *
 * No traps in tutorial, sprint, and arena.
 *
 * @return  The trap rate for the current level.
*/
int trap_rate_for_place()
{
    if (player_in_branch(BRANCH_TEMPLE)
        || (!player_in_connected_branch()
            && !player_in_branch(BRANCH_PANDEMONIUM))
        || crawl_state.game_is_sprint()
        || crawl_state.game_is_tutorial()
        || crawl_state.game_is_arena())
    {
        return 0;
    }

    return 1 + env.absdepth0 / 10;
}

/**
 * Choose a weighted random trap type for the currently-generated level.
 *
 * Odds of generating zot traps vary by depth (and are depth-limited). Alarm
 * traps also can't be placed before D:4. All other traps are depth-agnostic.
 *
 * @return                    A random trap type.
 *                            May be NUM_TRAPS, if no traps were valid.
 */

trap_type random_trap_for_place(bool dispersal_ok)
{
    // zot traps are Very Special.
    // very common in zot...
    if (player_in_branch(BRANCH_ZOT) && coinflip())
        return TRAP_ZOT;

    // and elsewhere, increasingly common with depth
    // possible starting at depth 15 (end of D, late lair, lair branches)
    // XXX: is there a better way to express this?
    if (random2(1 + env.absdepth0) > 14 && one_chance_in(3))
        return TRAP_ZOT;

    const bool shaft_ok = is_valid_shaft_level() && !player_in_hell();
    const bool tele_ok = !crawl_state.game_is_sprint();
    const bool alarm_ok = env.absdepth0 > 3;

    const pair<trap_type, int> trap_weights[] =
    {
        { TRAP_DISPERSAL, dispersal_ok && tele_ok  ? 1 : 0},
        { TRAP_TELEPORT,  tele_ok  ? 1 : 0},
        { TRAP_SHAFT,    shaft_ok  ? 1 : 0},
        { TRAP_ALARM,    alarm_ok  ? 1 : 0},
    };

    const trap_type *trap = random_choose_weighted(trap_weights);
    return trap ? *trap : NUM_TRAPS;
}

void place_webs(int num)
{
    trap_def ts;
    for (int j = 0; j < num; j++)
    {
        int tries;
        // this is hardly ever enough to place many webs, most of the time
        // it will fail prematurely. Which is fine.
        for (tries = 0; tries < 200; ++tries)
        {
            ts.pos.x = random2(GXM);
            ts.pos.y = random2(GYM);
            if (in_bounds(ts.pos)
                && env.grid(ts.pos) == DNGN_FLOOR
                && !map_masked(ts.pos, MMT_NO_TRAP))
            {
                // Calculate weight.
                int weight = 0;
                for (adjacent_iterator ai(ts.pos); ai; ++ai)
                {
                    // Solid wall?
                    int solid_weight = 0;
                    // Orthogonals weight three, diagonals 1.
                    if (cell_is_solid(*ai))
                    {
                        solid_weight = (ai->x == ts.pos.x || ai->y == ts.pos.y)
                                        ? 3 : 1;
                    }
                    weight += solid_weight;
                }

                // Maximum weight is 4*3+4*1 = 16
                // *But* that would imply completely surrounded by rock (no point there)
                if (weight <= 16 && x_chance_in_y(weight + 2, 34))
                    break;
            }
        }

        if (tries >= 200)
            break;

        ts.type = TRAP_WEB;
        ts.prepare_ammo();
        ts.reveal();
        env.trap[ts.pos] = ts;
    }
}

bool ensnare(actor *fly)
{
    ASSERT(fly); // XXX: change to actor &fly
    if (fly->is_web_immune())
        return false;

    if (fly->caught())
    {
        // currently webs are stateless so except for flavour it's a no-op
        if (fly->is_player())
            mpr("You are even more entangled.");
        return false;
    }

    // If we're over water, an open door, shop, portal, etc, the web will
    // fail to attach and you'll be released after a single turn.
    if (env.grid(fly->pos()) == DNGN_FLOOR)
    {
        place_specific_trap(fly->pos(), TRAP_WEB, 1); // 1 ammo = destroyed on exit (hackish)
        if (you.see_cell(fly->pos()))
            env.grid(fly->pos()) = DNGN_TRAP_WEB;
    }

    if (fly->is_player())
    {
        if (_player_caught_in_web()) // no fail, returns false if already held
            mpr("You are caught in a web!");
    }
    else
    {
        simple_monster_message(*fly->as_monster(), " is caught in a web!");
        fly->as_monster()->add_ench(ENCH_HELD);
    }

    // Drowned?
    if (!fly->alive())
        return true;

    return true;
}

// Whether this trap type can be placed in vaults by the ^ glyph
bool is_regular_trap(trap_type trap)
{
#if TAG_MAJOR_VERSION == 34
    return !is_removed_trap(trap)
           && (trap <= TRAP_MAX_REGULAR || trap == TRAP_DISPERSAL);
#else
    return trap <= TRAP_MAX_REGULAR;
#endif
}

#if TAG_MAJOR_VERSION == 34
bool is_removed_trap(trap_type trap)
{
    switch (trap)
    {
    case TRAP_ARROW:
    case TRAP_DART:
    case TRAP_SPEAR:
    case TRAP_BLADE:
    case TRAP_BOLT:
    case TRAP_NEEDLE:
    case TRAP_GAS:
    case TRAP_SHADOW:
    case TRAP_SHADOW_DORMANT:
        return true;
    default:
        return false;
    }
}
#endif
