/**
 * @file
 * @brief Traps related functions.
**/

#include "AppHdr.h"

#include "traps.h"

#include <algorithm>
#include <cmath>

#include "act-iter.h" // buff trap monster finding
#include "areas.h"
#include "art-enum.h"
#include "bloodspatter.h"
#include "branch.h"
#include "cloud.h"
#include "coordit.h"
#include "database.h" // harlequin trap lines
#include "delay.h"
#include "describe.h"
#include "dungeon.h"
#include "dungeon-feature-type.h"
#include "english.h"
#include "god-passive.h" // passive_t::avoid_traps
#include "hints.h"
#include "item-prop.h"
#include "items.h"
#include "libutil.h"
#include "map-knowledge.h"
#include "mapmark.h"
#include "mon-cast.h" // recall for zot traps
#include "mon-enum.h"
#include "mon-tentacle.h"
#include "mon-util.h"
#include "message.h"
#include "mon-place.h"
#include "nearby-danger.h"
#include "orb.h"
#include "player-notices.h"
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
#include "rltiles/tiledef-gui.h"
#include "teleport.h"
#include "terrain.h"
#include "travel.h"
#include "view.h"
#include "xom.h"

static string _net_immune_reason()
{
    if (you.unrand_equipped(UNRAND_SLICK_SLIPPERS))
        return "You slip through the net.";
    else if (you.is_insubstantial() || you.is_amorphous())
        return "The net passes straight through you.";
    return "";
}

bool trap_is_bad_for_player(dungeon_feature_type type)
{
    return type == DNGN_TRAP_ALARM
           || type == DNGN_TRAP_DISPERSAL
           || type == DNGN_TRAP_ZOT
           || type == DNGN_TRAP_NET
           || type == DNGN_TRAP_PLATE
           || type == DNGN_TRAP_TYRANT
           || type == DNGN_TRAP_ARCHMAGE
           || type == DNGN_TRAP_HARLEQUIN
           || type == DNGN_TRAP_DEVOURER;
}

bool trap_is_safe(dungeon_feature_type type, const actor* act)
{
    if (!act)
        act = &you;

    // TODO: For now, just assume they're safe; they don't damage outright,
    // and the messages get old very quickly
    if (type == DNGN_TRAP_WEB) // && act->is_web_immune()
        return true;

    if (!act->is_player())
        return trap_is_bad_for_player(type);

    // No prompt (teleport traps are ineffective if wearing a -Tele item)
    if ((type == DNGN_TRAP_TELEPORT || type == DNGN_TRAP_TELEPORT_PERMANENT)
        && you.no_tele())
    {
        return true;
    }

    if (type == DNGN_PASSAGE_OF_GOLUBRIA || type == DNGN_TRAP_SHAFT)
        return true;

    if (type == DNGN_TRAP_NET && !_net_immune_reason().empty())
        return true;

    // Let players specify traps as safe via lua.
    if (clua.callbooleanfn(false, "c_trap_is_safe", "s", dungeon_feature_name(type)))
        return true;

    return false;
}

// When giving AF_CHAOTIC to monsters, skip holy monsters due to potential vamp
// or draining attacks. They also need attacks that aren't already chaos brand.
bool chaos_lace_criteria (monster* mon) {
    return mons_has_attacks(*mon) && !(mon->is_peripheral())
            && !(mon->is_holy()) && !(is_good_god(mon->god))
            && (mons_attack_spec(*mon, 0, true).flavour != AF_CHAOTIC);
}

// Used for tyrant's traps, archmage's traps, and harlequin's traps:
// get a buff, look for the two or three highest-HD valid monsters without the
// buff, then give it to each of them. Returns the affected monsters the
// player can see for messaging purposes per-trap afterwards.
static vector<monster*> _find_and_buff_trap_targets(enchant_type enchant,
                                                    coord_def pos, int time)
{
    vector<monster*> options;
    vector<monster*> buffed;

    for (monster_near_iterator mi(pos, LOS_NO_TRANS); mi; ++mi)
    {
        if (!(mi->has_ench(enchant)) && !(mi->wont_attack())
             && !(mi->is_peripheral()))
        {
            if (enchant == ENCH_MIGHT && mons_has_attacks(**mi))
                options.push_back(*mi);
            else if (enchant == ENCH_EMPOWERED_SPELLS && mi->antimagic_susceptible())
                options.push_back(*mi);
            else if (enchant == ENCH_CHAOS_LACE && chaos_lace_criteria(*mi))
                options.push_back(*mi);
        }
    }

    shuffle_array(options);
    sort(options.begin( ), options.end( ), [](actor* a, actor* b)
    { return a->get_hit_dice() > b->get_hit_dice(); });

    if (!(options.empty()))
    {
        int fuzz = random_range(2, 3);
        int cap = (int(options.size()) > fuzz) ? fuzz : options.size();
        int affected = 0;

        for (monster *mons : options)
        {
            if (affected < cap)
            {
                if (enchant == ENCH_CHAOS_LACE && you.see_cell(mons->pos()))
                {
                    flash_tile(mons->pos(), random_choose(RED, BLUE, GREEN,
                               YELLOW, MAGENTA), 120, TILE_BOLT_CHAOS_BUFF);
                }

                mons->add_ench(mon_enchant(enchant, nullptr, time));

                // Traps effects are established as trap-los centric,
                // so don't inform players of monsters out of sight.
                if (you.see_cell(mons->pos()))
                    buffed.push_back(mons);

                affected++;
            }
        }
    }

    return buffed;
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

    if (act->caught_by() >= CAUGHT_NET)
        return "held in a net";
    else
        return "caught in a web";
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

    vector<coord_def> all_passages;
    for (rectangle_iterator ri(coord_def(0, 0), coord_def(GXM-1, GYM-1)); ri; ++ri)
    {
        if (env.grid(*ri) == DNGN_PASSAGE_OF_GOLUBRIA)
        {
            if (*ri != to)
            {
                if (!actor_at(*ri))
                    clear_passages.push_back(*ri);
                else
                    has_blocks = true;
            }
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
    { [] { blind_player(random_range(15, 20)); }, 4 },
    { [] { contaminate_player(1500 + random2avg(1800, 2), false); }, 4 },
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
                 create_malign_gateway(pt, MID_NOBODY, BEH_HOSTILE, "a Zot trap", 150);
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

// Triggers a trap-like feature at a given actor's location.
// (If there isn't one, nothing happens)
void trigger_trap(actor& triggerer)
{
    dungeon_feature_type type = env.grid(triggerer.pos());
    if (!feat_is_trap(type))
        return;

    const bool you_trigger = triggerer.is_player();

    // Store the position now in case it gets cleared in between.
    const coord_def pos(triggerer.pos());

    // Traps require line of sight without blocking translocation effects.
    // Requiring LOS prevents monsters from dispersing out of vaults that have
    // teleport traps available for the player to utilize. Additionally
    // requiring LOS_NO_TRANS prevents vaults that feature monsters with trap
    // behind glass from spamming the message log with irrelevant events.
    if (!you.see_cell_no_trans(pos))
        return;

    monster* m = triggerer.as_monster();

    // Tentacles aren't real monsters, and shouldn't invoke traps.
    if (m && mons_is_tentacle_or_tentacle_segment(m->type))
        return;

    switch (type)
    {
    case DNGN_PASSAGE_OF_GOLUBRIA:
    {
        coord_def to = pos;
        passage_type search_result = _find_other_passage_side(to);
        if (search_result == passage_type::free)
        {
            if (you_trigger)
                mpr("You enter the passage of Golubria.");
            else
                simple_monster_message(*m, " enters the passage of Golubria.");

            // Should always be true.
            bool moved = triggerer.move_to(to, MV_TRANSLOCATION | MV_GOLUBRIA);
            ASSERT(moved);

            place_cloud(CLOUD_TLOC_ENERGY, pos, 1 + random2(3), &triggerer);
            destroy_trap(pos);
        }
        else if (you_trigger)
        {
            mprf("This passage %s!", search_result == passage_type::blocked ?
                 "seems to be blocked by something" : "doesn't lead anywhere");
        }
        break;
    }
    case DNGN_TRAP_DISPERSAL:
    {
        dprf("Triggered dispersal.");
        if (you_trigger)
            mprf("You enter a dispersal trap!");
        else
            mprf("%s enters a dispersal trap!", triggerer.name(DESC_THE).c_str());

        for (monster_near_iterator mi(pos, LOS_NO_TRANS); mi; ++mi)
            if (!mi->no_tele())
                mi->blink();

        you.blink();

        // Make the trap go dormant briefly.
        temp_change_terrain(pos, DNGN_TRAP_DISPERSAL_INACTIVE,
                            random_range(4, 7) * BASELINE_DELAY);
        break;
    }
    case DNGN_TRAP_TELEPORT:
    case DNGN_TRAP_TELEPORT_PERMANENT:
        if (you_trigger)
            mprf("You enter a teleport trap!");
        if (type == DNGN_TRAP_TELEPORT)
        {
            destroy_trap(pos);
            mpr("The teleport trap disappears.");
        }
        if (!triggerer.no_tele())
            triggerer.teleport(true);
        break;

    case DNGN_TRAP_TYRANT:
    {
        if (you_trigger)
            mpr("You enter a tyrant's trap.");
        else
        {
            mprf("%s %s!", triggerer.name(DESC_THE).c_str(),
                 mons_intel(*m) >= I_HUMAN ? "invokes a tyrant's trap upon you" :
                                             "sets off a tyrant's trap");
        }

        int debuff_time = 10 + random2(5);
        you.increase_duration(DUR_WEAK, debuff_time, 50);

        vector<monster*> buffed_mons = _find_and_buff_trap_targets(ENCH_MIGHT, pos,
                                                                   debuff_time * 20);

        // Subject-verb agreement versus plurality is a pain.
        int visible_mons = 0;
        for (monster* mon : buffed_mons)
             if (you.can_see(*mon))
                 visible_mons++;

        if (buffed_mons.size() == 0)
           mpr("Your strength is siphoned away, and you feel your attacks grow feeble!");
        else
        {
            mprf("Your strength is siphoned away as %s grow%s stronger!",
                describe_monsters_condensed(buffed_mons).c_str(),
                visible_mons > 1 ? "" : "s");
        }

        break;
    }

    case DNGN_TRAP_ARCHMAGE:
    {
        if (you_trigger)
            mpr("You enter an archmage's trap.");
        else
        {
            mprf("%s %s!", triggerer.name(DESC_THE).c_str(),
                 mons_intel(*m) >= I_HUMAN ? "invokes an archmage's trap upon you" :
                                             "sets off an archmage's trap");
        }

        int buff_time = 200 + random2(80);
        int drain = min(you.magic_points, max(1, you.magic_points / 3));
        if (drain > 0)
            drain_mp(drain);

        vector<monster*> buffed_mons = _find_and_buff_trap_targets(ENCH_EMPOWERED_SPELLS,
                                                                   pos, buff_time);

        if (buffed_mons.size() == 0)
           mprf(MSGCH_WARN, "Your power is siphoned away!");
        else
        {
            string m_list = describe_monsters_condensed(buffed_mons);

            if (drain == 0)
                mprf("The spells of %s are empowered!", m_list.c_str());
            else
            {
                mprf(MSGCH_WARN, "Your power is siphoned away as the spells of %s are empowered!",
                     m_list.c_str());
            }
        }
        break;
    }

    case DNGN_TRAP_HARLEQUIN:
    {
        if (you_trigger)
            mpr("You enter a harlequin's trap.");

        int buff_time = 200 + random2(80);

        vector<monster*> buffed_mons = _find_and_buff_trap_targets(ENCH_CHAOS_LACE,
                                                                   pos, buff_time);

        if (buffed_mons.size() > 0)
        {
            // With no player-centric effect, don't keep spamming messages about
            // monsters walking on the trap unless it actually does something.
            if (!you_trigger)
            {
                mprf("%s %s!", triggerer.name(DESC_THE).c_str(),
                 mons_intel(*m) >= I_HUMAN ? "invokes a harlequin's trap" :
                                             "sets off a harlequin's trap");
            }

            string m_list = describe_monsters_condensed(buffed_mons);
            mprf(MSGCH_MONSTER_ENCHANT, "%s the attacks of %s are laced with chaos!",
                 getMiscString("harlequin_trap_lines").c_str(),  m_list.c_str());

            // Almost certainly not worth setting up any penance;
            // player-triggered Zot traps summoning demons don't either.
            if (you_trigger && is_good_god(you.religion))
                mprf(MSGCH_GOD, "You feel a twinge of divine disapproval.");
        }
        else if (!you_trigger)
            mprf("%s enters a harlequin's trap.", triggerer.name(DESC_THE).c_str());

        break;
    }

    case DNGN_TRAP_DEVOURER:
    {
        if (you_trigger)
            mpr("You enter a devourer's trap.");

        if (x_chance_in_y(3, 4))
        {
            if (!you_trigger)
            {
                mprf("%s %s!", triggerer.name(DESC_THE).c_str(),
                mons_intel(*m) >= I_HUMAN ? "invokes an devourer's trap upon you" :
                                            "sets off an devourer's trap");
            }
            flash_tile(you.pos(), YELLOW, 60, TILE_BOLT_DEFAULT_YELLOW);
            you.corrode(nullptr, "The trap's stomach acid", 6);
        }
        else if (!you_trigger)
            mprf("%s enters a devourer's trap.", triggerer.name(DESC_THE).c_str());

        break;
    }

    case DNGN_TRAP_ALARM:
        // Alarms always mark the player, but not through glass
        // The trap gets destroyed to prevent the player from abusing an alarm
        // trap found in favourable terrain.
        if (!you.see_cell_no_trans(pos))
            break;
        if (you_trigger)
            mpr("You set off the alarm!");
        else
            mprf("%s %s the alarm!", triggerer.name(DESC_THE).c_str(),
                 mons_intel(*m) >= I_HUMAN ? "pulls" : "sets off");

        if (silenced(pos))
            mpr("The alarm trap vibrates slightly, failing to make a sound.");
        else
            noisy(40, pos, "The alarm trap emits a blaring wail!", triggerer.mid);

        you.sentinel_mark(true);
        destroy_trap(pos);
        break;

    case DNGN_TRAP_NET:
        {
        // Nets need LOF to hit the player, no netting through glass.
        if (!you.see_cell_no_trans(pos))
            break;
        // Don't try to re-net the player when they're already netted/webbed.
        if (you.caught())
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

                if (!simple_monster_message(*m, " drops a net on you."))
                    mpr("Something launches a net on you.");
            }
        }

        if (!triggered)
            break;

        if (you_trigger)
            mpr("You trigger a net trap.");

        if (random2avg(2 * you.evasion(), 2) > 18 + env.absdepth0 / 2)
        {
            mpr("You avoid being caught in a net.");
            break;
        }

        if (!you.trap_in_net(false))
        {
            const string reason = _net_immune_reason();
            if (!reason.empty())
                mpr(reason);
            break;
        }

        if (player_in_a_dangerous_place())
            xom_is_stimulated(50);
        break;
        }

    case DNGN_TRAP_WEB:
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
            else if (you.trap_in_web())
            {
                if (orig_terrain(pos) != DNGN_TRAP_WEB)
                    destroy_trap(pos);
                if (player_in_a_dangerous_place())
                    xom_is_stimulated(50);
            }
        }
        else if (m)
        {
            if (one_chance_in(3))
                simple_monster_message(*m, " evades a web.");
            else
            {
                if (m->trap_in_web())
                {
                    if (!m->visible_to(&you))
                        mpr("A web moves frantically as something is caught in it!");
                    // Don't try to escape the web in the same turn
                    m->props[NEWLY_TRAPPED_KEY] = true;
                    if (orig_terrain(pos) != DNGN_TRAP_WEB)
                        destroy_trap(pos);
                }
            }
        }
        break;

    case DNGN_TRAP_ZOT:
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

    case DNGN_TRAP_SHAFT:
        // Neither the player nor their allies will fall down known shafts.
        if ((m && (m->wont_attack() || !m->will_trigger_shaft())) || you_trigger)
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
            destroy_trap(pos);

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

    case DNGN_TRAP_PLATE:
        dungeon_events.fire_position_event(DET_PRESSURE_PLATE, pos);
        break;

    default:
        break;
    }
}

void destroy_trap(const coord_def& pos)
{
    if (!feat_is_trap(env.grid(pos)))
        return;

    dungeon_terrain_changed(pos, DNGN_FLOOR);
    if (you.see_cell(pos))
    {
        update_terrain_knowledge(pos);
        StashTrack.update_stash(pos);
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
    vector<dungeon_feature_type> available_traps = { DNGN_TRAP_TELEPORT };
    // Don't shaft the player when shafts aren't allowed in the location or when
    //  it would be into a dangerous end.
    if (_is_valid_shaft_effect_level() && you.shaftable())
        available_traps.push_back(DNGN_TRAP_SHAFT);
    // No alarms on the first 4 floors
    if (env.absdepth0 > 3)
        available_traps.push_back(DNGN_TRAP_ALARM);

    switch (*random_iterator(available_traps))
    {
        case DNGN_TRAP_SHAFT:
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

        case DNGN_TRAP_ALARM:
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
            apply_noises(); // Otherwise the noise from them won't kick in until the end of the turn.
            break;

        case DNGN_TRAP_TELEPORT:
        {
            // XXX: Approximate old chance of triggering on an average floor.
            if (!one_chance_in(3) || !hostile_teleport_is_possible())
                break;

            string msg = make_stringf("%s and a teleportation trap "
                                      "spontaneously manifests!",
                                      _malev_msg().c_str());
            if (have_passive(passive_t::avoid_traps))
            {
                mpr(msg);
                simple_god_message(" warns you in time for you to avoid it.");
                return;
            }
            mpr(msg);
            hostile_teleport_player();
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

dungeon_feature_type random_trap_for_place(bool dispersal_ok)
{
    // zot traps are Very Special.
    // very common in zot...
    if (player_in_branch(BRANCH_ZOT) && coinflip())
        return DNGN_TRAP_ZOT;

    // and elsewhere, increasingly common with depth
    // possible starting at depth 15 (end of D, late lair, lair branches)
    // XXX: is there a better way to express this?
    if (random2(1 + env.absdepth0) > 14 && one_chance_in(3))
        return DNGN_TRAP_ZOT;

    const bool shaft_ok = is_valid_shaft_level() && !player_in_hell();
    const bool tele_ok = !crawl_state.game_is_sprint();
    const bool alarm_ok = env.absdepth0 > 3;

    // Split lesser theme traps across branches to vary up those branches.
    // (Most Zot and Tomb trap placement is done by vaults.)
    const bool tyrant_ok = player_in_branch(BRANCH_ORC)
                           || player_in_branch(BRANCH_SNAKE)
                           || player_in_branch(BRANCH_VAULTS);
    const bool archmage_ok = player_in_branch(BRANCH_SNAKE)
                             || player_in_branch(BRANCH_ELF)
                             || player_in_branch(BRANCH_DEPTHS);
    const bool harlequin_ok = player_in_branch(BRANCH_DEPTHS)
                              || player_in_branch(BRANCH_ZOT)
                              || player_in_branch(BRANCH_PANDEMONIUM);
    const bool devourer_ok = player_in_branch(BRANCH_SLIME)
                             || player_in_branch(BRANCH_PANDEMONIUM);

    const pair<dungeon_feature_type, int> trap_weights[] =
    {
        { DNGN_TRAP_DISPERSAL, dispersal_ok && tele_ok  ? 4 : 0},
        { DNGN_TRAP_TELEPORT,  tele_ok      ? 5 : 0},
        { DNGN_TRAP_SHAFT,     shaft_ok     ? 4 : 0},
        { DNGN_TRAP_ALARM,     alarm_ok     ? 5 : 0},
        { DNGN_TRAP_TYRANT,    tyrant_ok    ? 3 : 0},
        { DNGN_TRAP_ARCHMAGE,  archmage_ok  ? 3 : 0},
        { DNGN_TRAP_HARLEQUIN, harlequin_ok ? 2 : 0},
        { DNGN_TRAP_DEVOURER,  devourer_ok  ? 2 : 0},
    };

    const dungeon_feature_type *trap = random_choose_weighted(trap_weights);
    return trap ? *trap : DNGN_FLOOR;
}

void place_webs(int num)
{
    for (int j = 0; j < num; j++)
    {
        int tries;
        coord_def pos;
        // this is hardly ever enough to place many webs, most of the time
        // it will fail prematurely. Which is fine.
        for (tries = 0; tries < 200; ++tries)
        {
            pos.x = random2(GXM);
            pos.y = random2(GYM);
            if (in_bounds(pos)
                && env.grid(pos) == DNGN_FLOOR
                && !map_masked(pos, MMT_NO_TRAP))
            {
                // Calculate weight.
                int weight = 0;
                for (adjacent_iterator ai(pos); ai; ++ai)
                {
                    // Solid wall?
                    int solid_weight = 0;
                    // Orthogonals weight three, diagonals 1.
                    if (cell_is_solid(*ai))
                    {
                        solid_weight = (ai->x == pos.x || ai->y == pos.y)
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

        env.grid(pos) = DNGN_TRAP_WEB;
    }
}

bool player::trap_in_web()
{
   if (is_web_immune() || caught())
        return false;

    you.attribute[ATTR_HELD] = 1;
    you.redraw_armour_class = true;
    you.redraw_evasion      = true;
    quiver::set_needs_redraw();

    mpr("You are caught in a web!");

    return true;
}

bool monster::trap_in_web()
{
    if (is_web_immune() || caught())
        return false;

    simple_monster_message(*this, " is caught in a web!");
    add_ench(ENCH_HELD);

    return true;
}

bool player::trap_in_net(bool real, bool quiet)
{
    if (!_net_immune_reason().empty() || caught())
        return false;

    you.attribute[ATTR_HELD] = NET_STARTING_DURABILITY;
    you.props[NET_IS_REAL_KEY].get_bool() = real;
    you.redraw_armour_class = true;
    you.redraw_evasion = true;
    quiver::set_needs_redraw();

    if (!quiet)
        mpr("You become entangled in the net!");

    stop_running();
    stop_delay(true); // even stair delays
    return true;
}

/**
 * Attempt to trap a monster in a net.
 *
 * @param  real  Whether this net should drop on the ground after the monster
 *               escapes (or dies).
 * @param  quiet Whether to silence any messages caused by doing this.
 * @return       Whether the monster was successfully trapped.
 */
bool monster::trap_in_net(bool real, bool quiet)
{
    if (caught())
        return false;

    if (is_insubstantial() || is_amorphous())
    {
        if (!quiet && you.can_see(*this))
        {
            if (is_insubstantial())
                mprf("The net passes right through %s!", name(DESC_THE).c_str());
            else
                mprf("%s effortlessly oozes through the net!", name(DESC_THE).c_str());
        }
        return false;
    }

    if (mons_is_tentacle_or_tentacle_segment(type)
        || is_stationary() && !mons_has_attacks(*this))
    {
        return false;
    }

    add_ench(mon_enchant(ENCH_HELD, nullptr, 0, NET_STARTING_DURABILITY));
    if (!quiet && you.see_cell(pos()))
    {
        if (!visible_to(&you))
            mpr("Something gets caught in the net!");
        else
            simple_monster_message(*this, " is caught in the net!");
    }

    props[NET_IS_REAL_KEY].get_bool() = real;
    return true;
}

void player::struggle_against_net()
{
    if (caught_by() == CAUGHT_WEB)
    {
        // Roll a chance to escape the web.
        if (x_chance_in_y(3, 10))
        {
            mpr("You struggle to detach yourself from the web.");
            return;
        }
        else
        {
            mpr("You disentangle yourself.");
            stop_being_caught();
            return;
        }
    }

    // Handle nets now.
    const int damage = random_range(1, 4);
    if (damage >= you.attribute[ATTR_HELD])
    {
        mprf("You %s the net and break free!", damage > 3 ? "shred" : "rip");
        stop_being_caught();
        return;
    }

    if (damage > 3)
        mpr("You tear a large gash into the net.");
    else
        mpr("You struggle against the net.");

    you.attribute[ATTR_HELD] -= damage;
}

void player::stop_being_caught(bool drop_net)
{
    const caught_type ctype = caught_by();

    if (ctype >= CAUGHT_NET)
    {
        props.erase(NET_IS_REAL_KEY);

        // Make a fresh net to drop on the ground, with chance proportional to
        // how much damage it suffered.
        if (ctype == CAUGHT_NET && drop_net
            && x_chance_in_y(you.attribute[ATTR_HELD], 9))
        {
            // If we're in the middle of moving, drop the net at the player's
            // previous position.
            drop_net_at(last_move_pos.origin() ? pos() : last_move_pos);
        }
    }

    you.attribute[ATTR_HELD] = 0;
    you.redraw_armour_class = true;
    you.redraw_evasion = true;
    quiver::set_needs_redraw();
}
