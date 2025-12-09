/**
 * @file  player-notices.cc
 * @brief Functions for handling the player first encountering monsters, such as
 *        encounter messages and see_monster interrupts.
**/

#include "AppHdr.h"

#include "act-iter.h"
#include "areas.h"
#include "attitude-change.h"
#include "database.h"
#include "delay.h"
#include "describe.h"
#include "directn.h"
#include "dgn-overview.h"
#include "english.h"
#include "env.h"
#include "items.h"
#include "hints.h"
#include "god-conduct.h"
#include "god-passive.h"
#include "god-wrath.h"
#include "level-state-type.h"
#include "message.h"
#include "mutation.h"
#include "mon-act.h"
#include "mon-behv.h"
#include "mon-gear.h"
#include "nearby-danger.h"
#include "notes.h"
#include "output.h"
#include "player-notices.h"
#include "religion.h"
#include "shout.h"
#include "spl-transloc.h"
#include "state.h"
#include "stringutil.h"
#include "transform.h"
#include "view.h"
#include "xom.h"

// Just-generated monsters which are slated to be announced together with
// special messages.
static vector<monster*> to_notice;
seen_context_type special_announce_context;

static bool _try_seen_interrupt(monster& mons, seen_context_type sc)
{
    if (mons_is_safe(&mons))
        return false;

    activity_interrupt_data aid(&mons, sc);
    return interrupt_activity(activity_interrupt::see_monster, aid);
}

static bool _check_monster_alert(const monster& mon)
{
    // Beogh-converted orcs may have left the floor on the turn they were encountered.
    if (!mon.alive())
        return false;

    if ((Options.monster_alert[mon.type]
         || Options.monster_alert_uniques && mons_is_unique(mon.type)
         || Options.monster_alert_min_threat < MTHRT_UNDEF
            && mons_threat_level(mon) >= Options.monster_alert_min_threat
         || Options.monster_alert_unusual && monster_info(&mon).has_unusual_items())
        && !mon.is_firewood() && !mons_att_wont_attack(mon.attitude))
    {
        // If the player encountered this by moving, make sure to actually
        // draw the monster we're warning about.
        update_screen();
        viewwindow();

        // And if it wasn't a monster that would get an encounter warning due to
        // being a summon, make sure to say something.
        if (mon.is_summoned())
            mprf(MSGCH_WARN, "%s comes into view.", mon.name(DESC_A).c_str());

        more(true);
        return true;
    }

    return false;
}

// Observe a monster that may have potentially moved into the player's LoS,
// printing messages and performing other effects if this is the first time the
// player has seen them, and firing activity interrupts even if it isn't.
void maybe_notice_monster(monster& mons, bool stepped)
{
    // Already noticed or unable to be noticed.
    if ((mons.flags & MF_WAS_IN_VIEW) || !you.can_see(mons))
        return;

    const bool already_seen = (bool)(mons.flags & MF_SEEN);
    seen_monster(&mons);

    if (!already_seen)
        _check_monster_alert(mons);

    // If the monster has been seen before (and thus won't print an encounter
    // message), tell interrupt_activity to print something else if needed.
    if (crawl_state.is_repeating_cmd() || you_are_delayed())
    {
        _try_seen_interrupt(mons, !already_seen ? SC_NEWLY_SEEN
                                                : stepped ? SC_NONE
                                                          : SC_ALREADY_IN_VIEW);
    }
}

static monster_type _mons_merge_genus(monster_type mc)
{
    switch (mc)
    {
        case MONS_ZOMBIE:
        case MONS_SIMULACRUM:
        case MONS_DRAUGR:
            return mons_genus(mc);

        default:
            return mc;
    }
}

typedef struct
{
    const monster *mon;
    string name;
    int count;
    bool genus;
} details;

/**
 * Monster list simplification
 *
 * When too many monsters come into view at once, we group the ones with the
 * same genus, starting with the most represented genus.
 *
 * @param types monster types and the number of monster for each type.
 * @param genera monster genera and the number of monster for each genus.
 */
static void _genus_factoring(map<const string, details> &types,
                             map<monster_type, int> &genera)
{
    monster_type genus = MONS_NO_MONSTER;
    int num = 0;
    // Find the most represented genus.
    for (const auto &entry : genera)
        if (entry.second > num)
        {
            genus = entry.first;
            num = entry.second;
        }

    // The most represented genus has a single member.
    // No more factoring is possible, we're done.
    if (num == 1)
    {
        genera.clear();
        return;
    }

    genera.erase(genus);

    const monster *mon = nullptr;

    auto it = types.begin();
    do
    {
        if (_mons_merge_genus(it->second.mon->type) != genus)
        {
            ++it;
            continue;
        }

        // This genus has a single monster type. Can't factor.
        if (it->second.count == num)
            return;

        mon = it->second.mon;
        types.erase(it++);
    } while (it != types.end());
    ASSERT(mon); // There is a match as genera contains the monsters in types.

    const auto name = mons_type_name(genus, DESC_PLAIN);
    types[name] = {mon, name, num, true};
}

static bool _is_mon_equipment_worth_listing(const monster_info &mi, bool any_weapon)
{
    for (unsigned int i = 0; i <= MSLOT_LAST_VISIBLE_SLOT; ++i)
    {
        if (!mi.inv[i])
            continue;

        const item_def& item = *mi.inv[i].get();
        if (any_weapon && is_weapon(item)
            || item_is_worth_listing(item)
            || i == MSLOT_ALT_WEAPON && is_range_weapon(item))
        {
            return true;
        }
    }
    return false;
}

/// Return whether or not monster_info::_core_name() describes the inventory
/// for the monster "mon".
static bool _does_core_name_include_inventory(const monster *mon)
{
    return mon->type == MONS_DANCING_WEAPON || mon->type == MONS_SPECTRAL_WEAPON
           || mon->type == MONS_ARMOUR_ECHO;
}

/// Output an equipment warning for newly-seen monsters to 'out'.
static void _monster_headsup(const vector<monster*> &monsters,
                               const unordered_set<const monster*> &single,
                               ostringstream& out)
{
    int listed = 0;
    for (const monster* mon : monsters)
    {
        monster_info mi(mon);
        if (!_is_mon_equipment_worth_listing(mi, monsters.size() == 1))
            continue;

        // Don't repeat inventory. Non-single monsters may be merged, in which
        // case the name is just the name of the monster type.
        if (_does_core_name_include_inventory(mon) && single.count(mon))
            continue;

        // Put equipment messages on a new line if there's more than one monster.
        // (But skip the leading space if nothing else is in this message.)
        if (listed == 0)
            out << (monsters.size() > 1 ? "\n" : out.tellp() > 0 ? " " : "");
        else
            out << " ";

        ++listed;

        string monname;
        if (monsters.size() == 1)
            monname = mon->pronoun(PRONOUN_SUBJECTIVE);
        else if (mon->type == MONS_DANCING_WEAPON)
            monname = "There";
        else if (single.count(mon))
            monname = mon->full_name(DESC_THE);
        else
            monname = mon->full_name(DESC_A);
        out << uppercase_first(monname) << " ";

        if (monsters.size() == 1)
            out << conjugate_verb("are", mon->pronoun_plurality());
        else
            out << "is";

        if (mon->type != MONS_DANCING_WEAPON)
            out << " ";
        out << get_monster_equipment_desc(mi, monsters.size() > 1 ? DESC_NOTEWORTHY
                                                                  : DESC_NOTEWORTHY_AND_WEAPON,
                                          DESC_NONE) << ".";
    }
}

/**
 * Calculate a list of monster types and genera from a list of monsters.
 *
 * @param monsters      A list of monsters (who may have just become visible)
 * @param[out] single   A list of the monsters in "monsters" which are to be
 *                      described separately ("a hog", not one of "2 hogs").
 * @param[out] species  A list of the monsters in "monsters" which are to be
 *                      described. Each element contains a monster, the number
 *                      of monsters to be included, and whether to refer to the
 *                      monster using the genus rather than the monster details.
 */
static void _count_monster_types(const vector<monster*> &monsters,
                                    unordered_set<const monster*> &single,
                                 vector<details> &species)
{
    const unsigned int max_types = 4;

    map<monster_type, int> genera; // This is the plural for genus!
    map<const string, details> species_s; // select which species to show
    for (const monster *mon : monsters)
    {
        const string name = mon->name(DESC_PLAIN);
        auto &det = species_s[name];
        det = {mon, name, det.count+1, false};
        genera[_mons_merge_genus(mon->type)]++;
    }

    // Don't merge named monsters (ghosts and the like). They're exciting!
    while (species_s.size() > max_types && !genera.empty())
        _genus_factoring(species_s, genera);

    map <const monster*, details> species_o; // put species in an order
    for (const auto &sp : species_s)
    {
        const auto det = sp.second;
        species_o[det.mon] = det;
        if (1 == det.count)
            single.insert(det.mon);
    }

    // Build a vector of species/genera sorted by one of the monsters from each.
    for (const auto &sp : species_o)
        species.push_back(sp.second);
}


static string _describe_monsters_from_species(const vector<details> &species)
{
    return comma_separated_fn(species.begin(), species.end(),
        [] (const details &det)
        {
            string name = det.name;
            const monster_type base_type =
                mons_is_zombified(*(det.mon)) ? det.mon->base_monster
                                              : det.mon->type;
            // Treat all monsters with unique base types as named, since zombified
            // uniques keep their names.
            if ((det.mon->is_named() || mons_is_unique(base_type)) && det.count == 1)
            {
                string title = getMiscString(det.mon->name(DESC_DBNAME) + " title");
                if (!title.empty())
                    return title;

                // Use monster_info to get the monster's full name, so we get
                // proper articles for all named monsters as well as proper
                // types for all zombified uniques.
                monster_info mi(det.mon);
                return mi.full_name(DESC_A);
            }
            else if (det.count > 1 && det.genus)
            {
                auto genus = mons_genus(det.mon->type);
                name = " "+pluralise(mons_type_name(genus, DESC_PLAIN));
            }
            else if (det.count > 1)
                name = " "+pluralise(det.name);
            return apply_description(DESC_A, name, det.count);
        });
}

/**
 * Return a string listing monsters in a human readable form.
 * E.g. "a hydra and 2 liches".
 *
 * @param monsters      A list of monsters that just became visible.
 */
string describe_monsters_condensed(const vector<monster*>& monsters)
{
    unordered_set<const monster*> single;
    vector<details> species;
    _count_monster_types(monsters, single, species);

    return _describe_monsters_from_species(species);
}

static string _abyss_monster_creation_message(const monster* mon)
{
    if (mon->type == MONS_DEATH_COB)
    {
        return coinflip() ? " appears in a burst of microwaves!"
                          : " pops from nullspace!";
    }

    // You may ask: "Why these weights?" So would I!
    const vector<pair<string, int>> messages = {
        { " appears in a shower of translocational energy.", 17 },
        { " appears in a shower of sparks.", 34 },
        { " materialises.", 45 },
        { " emerges from chaos.", 13 },
        { " emerges from the beyond.", 26 },
        { make_stringf(" assembles %s!",
                       mon->pronoun(PRONOUN_REFLEXIVE).c_str()), 33 },
        { " erupts from nowhere.", 9 },
        { " bursts from nowhere.", 18 },
        { " is cast out of space.", 7 },
        { " is cast out of reality.", 14 },
        { " coalesces out of pure chaos.", 5 },
        { " coalesces out of seething chaos.", 10 },
        { " punctures the fabric of time!", 2 },
        { " punctures the fabric of the universe.", 7 },
        { make_stringf(" manifests%s!",
                       silenced(you.pos()) ? "" : " with a bang"), 3 },


    };

    return *random_choose_weighted(messages);
}

/**
 * Handle printing "You encounter X" messages for newly appeared monsters.
 * Also let Ash/Zin warn the player about newly-seen monsters, as appropriate.
 *
 * @param monsters      A list of monsters that just became visible.
 * @param sc            Any special context for how these monsters came into
 *                      view (or existence).
 */
static void _handle_encounter_messages(const vector<monster*> monsters,
                                       seen_context_type sc = SC_NONE)
{
    unordered_set<const monster*> single;
    vector<details> species;

    for (monster* mon : monsters)
        if (!(mon->flags & MF_WAS_IN_VIEW))
            view_monster_equipment(mon);

    _count_monster_types(monsters, single, species);

    ostringstream out;

    // Abyss spawn messages should be printed one at a time, regardless of how
    // many things are seen at once.
    if (sc == SC_ABYSS)
    {
        // But make all monsters of the same species use the same flavor message,
        // to condense message spam a little.
        for (details& sp : species)
        {
            string msg = _abyss_monster_creation_message(sp.mon);
            for (int i = 0; i < sp.count; ++i)
            {
                mprf(MSGCH_MONSTER_WARNING, "%s%s", sp.mon->name(DESC_A).c_str(),
                                                    msg.c_str());
            }
        }
    }
    else if (sc == SC_ORBRUN)
    {
        out << _describe_monsters_from_species(species).c_str() << " appear";
        if (monsters.size() == 1)
            out << "s";
        out << " in pursuit of the Orb! ";
    }
    else
        out << "You encounter " << _describe_monsters_from_species(species) << ".";

    _monster_headsup(monsters, single, out);

    string msg = out.str();
    if (!msg.empty())
        mprf(MSGCH_MONSTER_WARNING, "%s", out.str().c_str());
}

static bool _monster_needs_warning(const monster& mon)
{
    return !mon.is_firewood() && !mon.is_summoned() && !mons_is_always_safe(&mon);
}

/**
 * Print encounter messages and process all on-first-seen effects for a given
 * set of monsters.
 *
 * @param   monsters      A list of monsters newly seen, for which on-first-seen
 *                        effects should be processed.
 * @param   to_announce   A list of monsters for which encounter messages should
 *                        be printed (which may not include all monsters in the
 *                        previous list).
 */
void notice_new_monsters(vector<monster*>& monsters, vector<monster*>& to_announce,
                         seen_context_type sc)
{
    // We may only wish to announce a subset of monsters seen.
    if (!to_announce.empty())
        _handle_encounter_messages(to_announce, sc);

    // Potentially stop the player for warnings.
    for (monster* mon : monsters)
        if (!(mon->flags & MF_SEEN))
            if (_check_monster_alert(*mon))
                break;

    // But should flag all of them as seen, even if they're harmless.
    for (monster* mon : monsters)
        seen_monster(mon, false);

    if (crawl_state.is_repeating_cmd() || you_are_delayed())
        for (monster* mon : monsters)
            _try_seen_interrupt(*mon, to_announce.empty() ? SC_NONE : SC_NEWLY_SEEN);
}

void queue_monster_announcement(monster& mons, seen_context_type sc)
{
    special_announce_context = sc;
    to_notice.push_back(&mons);
}

void notice_queued_monsters()
{
    // Remove any not still in the player's LoS. (Clouds created when Abyss
    // monsters spawn may conceal some of these between creation and announcement.)
    to_notice.erase(std::remove_if(to_notice.begin(), to_notice.end(),
                    [](const monster * m) { return !you.can_see(*m); }),
                    to_notice.end());

    notice_new_monsters(to_notice, to_notice, special_announce_context);
    to_notice.clear();
}

void update_monsters_in_view()
{
    int num_hostile = 0;

    // Newly-spotted monsters.
    vector<monster*> monsters;
    vector<monster*> to_announce;

    for (monster_iterator mi; mi; ++mi)
    {
        if (you.see_cell(mi->pos()))
        {
            if (mi->attitude == ATT_HOSTILE && !mi->is_firewood())
                num_hostile++;

            if (mi->visible_to(&you))
            {
                if (!(mi->flags & MF_WAS_IN_VIEW))
                {
                    monsters.push_back(*mi);
                    if (_monster_needs_warning(**mi) && !(mi->flags & MF_SEEN))
                        to_announce.push_back(*mi);
                }
            }
            else
                mi->flags &= ~MF_WAS_IN_VIEW;
        }
        else if (!you.turn_is_over)
        {
            if (mi->flags & MF_WAS_IN_VIEW)
            {
                // Reset client id so the player doesn't know (for sure) he
                // has seen this monster before when it reappears.
                mi->reset_client_id();
            }

            mi->flags &= ~MF_WAS_IN_VIEW;
        }
    }

    notice_new_monsters(monsters, to_announce);

    // Xom thinks it's hilarious the way the player picks up an ever
    // growing entourage of monsters while running through the Abyss.
    // To approximate this, if the number of hostile monsters in view
    // is greater than it ever was for this particular trip to the
    // Abyss, Xom is stimulated in proportion to the number of
    // hostile monsters. Thus if the entourage doesn't grow, then
    // Xom becomes bored.
    if (player_in_branch(BRANCH_ABYSS)
        && you.attribute[ATTR_ABYSS_ENTOURAGE] < num_hostile)
    {
        you.attribute[ATTR_ABYSS_ENTOURAGE] = num_hostile;
        xom_is_stimulated(12 * num_hostile);
    }
}

void monster_encounter_message(monster& mon)
{
    if (_monster_needs_warning(mon))
        _handle_encounter_messages({&mon});
}

static void _maybe_growl_at(const monster* mons)
{
    // Don't repeatedly growl in place on the same turn.
    if (you.shouted_pos == you.pos())
        return;

    if (you.form == transformation::maw && maw_growl_check(mons))
        you.shouted_pos = you.pos();
}

void seen_monster(monster* mons, bool do_encounter_message)
{
    if (!(mons->flags & MF_WAS_IN_VIEW))
    {
        set_unique_annotation(mons);
        view_monster_equipment(mons);
        mons_set_just_seen(mons);
    }

    // Monster was viewed this turn
    mons->flags |= MF_WAS_IN_VIEW;

    // Don't perform most of these effects for friendly/good neutral monsters.
    if (mons->flags & MF_SEEN || mons_att_wont_attack(mons->attitude))
        return;

    // First time we've seen this particular monster.
    mons->flags |= MF_SEEN;

    if (do_encounter_message)
        monster_encounter_message(*mons);

    if (crawl_state.game_is_hints())
        hints_monster_seen(*mons);

    if (mons_is_notable(*mons))
    {
        string name = mons->name(DESC_A, true);
        if (mons->type == MONS_PLAYER_GHOST)
        {
            name += make_stringf(" (%s)",
                                 short_ghost_description(mons, true).c_str());
        }
        else if (mons->flags & MF_KNOWN_SHIFTER)
        {
            name += make_stringf(" (%sshapeshifter)",
                mons->has_ench(ENCH_GLOWING_SHAPESHIFTER) ? "glowing " : "");
        }
        take_note(Note(NOTE_SEEN_MONSTER, mons->type, 0, name));
    }

    if (you.unrand_equipped(UNRAND_WYRMBANE))
    {
        const item_def *wyrmbane = nullptr;
        const item_def *wpn = you.weapon();
        const item_def *offhand_wpn = you.offhand_weapon();

        if (wpn && wpn->unrand_idx == UNRAND_WYRMBANE)
            wyrmbane = wpn;
        else if (offhand_wpn && offhand_wpn->unrand_idx == UNRAND_WYRMBANE)
            wyrmbane = offhand_wpn;

        if (wyrmbane && mons->dragon_level() > wyrmbane->plus)
            mpr("<green>Wyrmbane glows as a worthy foe approaches.</green>");
    }

    // attempt any god conversions on first sight
    do_conversions(mons);

    if (!(mons->flags & MF_TSO_SEEN))
    {
        if (mons_gives_xp(*mons, you) && !crawl_state.game_is_arena())
        {
            did_god_conduct(DID_SEE_MONSTER, mons->get_experience_level(),
                            true, mons);
        }
        mons->flags |= MF_TSO_SEEN;
    }

    if (mons_offers_beogh_conversion(*mons))
        env.level_state |= LSTATE_BEOGH;

    if (you.form == transformation::sphinx)
        sphinx_notice_riddle_target(mons);

    // If the player has the attractive mutation, maybe attract newly-seen monsters.
    if (should_attract_mons(*mons))
    {
        const int dist = grid_distance(you.pos(), mons->pos());
        attract_monster(*mons, dist - 2); // never attract adjacent

        // Possibly wake the monster (but don't automatically make it find the player)
        if (coinflip())
            behaviour_event(mons, ME_DISTURB, nullptr, you.pos());
    }

    maybe_apply_bane_to_monster(*mons);

    if (player_under_penance(GOD_GOZAG)
        && !mons->wont_attack()
        && !mons->is_stationary()
        && !mons->is_peripheral()
        && coinflip()
        && mons->get_experience_level() >= random2(you.experience_level))
    {
        mprf(MSGCH_GOD, GOD_GOZAG, "Gozag incites %s against you.",
                mons->name(DESC_THE).c_str());
        gozag_incite(mons);
    }

    if (mons->is_shapeshifter()
        && !(mons->flags & MF_KNOWN_SHIFTER)
        && have_passive(passive_t::warn_shapeshifter))
    {
        string msg = make_stringf(" warns you: %s is a foul%s shapeshifter.",
                                    uppercase_first(mons->name(DESC_THE)).c_str(),
                                    mons->has_ench(ENCH_GLOWING_SHAPESHIFTER) ? " glowing" : "");
        simple_god_message(msg.c_str());
        discover_shifter(*mons);
#ifndef USE_TILE_LOCAL
            // XXX: should this really be here...?
            update_monster_pane();
#endif
    }

    _maybe_growl_at(mons);
}
