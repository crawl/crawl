/**
 * @file
 * @brief Tracking permallies granted by Yred and Beogh.
**/

#include "AppHdr.h"

#include "god-companions.h"

#include <algorithm>

#include "ability.h"
#include "act-iter.h"
#include "attitude-change.h"
#include "branch.h"
#include "cloud.h"
#include "coordit.h"
#include "dactions.h"
#include "database.h"
#include "delay.h"
#include "dgn-overview.h"
#include "env.h"
#include "files.h"
#include "fineff.h"
#include "ghost.h"
#include "items.h"
#include "macro.h"
#include "message.h"
#include "mon-death.h"
#include "mon-gear.h"
#include "mon-pathfind.h"
#include "mon-place.h"
#include "mon-speak.h"
#include "mon-util.h"
#include "notes.h"
#include "prompt.h"
#include "religion.h"
#include "spl-other.h"
#include "stringutil.h"
#include "tag-version.h"
#include "terrain.h"
#include "tilepick.h"

enum apostle_death_reason
{
    APOSTLE_DEAD,
    APOSTLE_BANISHED,
    APOSTLE_LEFT
};

map<mid_t, companion> companion_list;

companion::companion(const monster& m)
    : mons(follower(m)), level(level_id::current()), timestamp(you.elapsed_time)
{
}

void init_companions()
{
    companion_list.clear();
}

void add_companion(monster* mons)
{
    ASSERT(mons->alive());
    // Right now this is a special case for Saint Roka, but
    // future orcish uniques should behave in the same way.
    mons->props[NO_ANNOTATE_KEY] = true;
    remove_unique_annotation(mons);
    companion_list[mons->mid] = companion(*mons);
}

void remove_companion(monster* mons)
{
    mons->props[NO_ANNOTATE_KEY] = false;
    set_unique_annotation(mons);
    companion_list.erase(mons->mid);
}

void remove_bound_soul_companion()
{
    for (auto &entry : companion_list)
    {
        monster* mons = monster_by_mid(entry.first);
        if (!mons)
            mons = &entry.second.mons.mons;
        if (mons->type == MONS_BOUND_SOUL)
        {
            remove_companion(mons);
            return;
        }
    }
}

void remove_all_companions(god_type god)
{
    for (auto i = companion_list.begin(); i != companion_list.end();)
    {
        monster* mons = monster_by_mid(i->first);
        if (!mons)
            mons = &i->second.mons.mons;
        if (mons_is_god_gift(*mons, god))
            companion_list.erase(i++);
        else
            ++i;
    }

    // Cleanup apostle data structures on god abandonment
    if (god == GOD_BEOGH)
    {
        CrawlVector& vec = you.props[BEOGH_SAVED_APOSTLES_KEY].get_vector();
        vec.clear();
        you.props.erase(BEOGH_NUM_FOLLOWERS_KEY);
    }
}

void move_companion_to(const monster* mons, const level_id lid)
{
    // If it's taking stairs, that means the player is heading ahead of it,
    // so we shouldn't relocate the monster until it actually arrives
    // (or we can clone things on the other end)
    if (!(mons->flags & MF_TAKING_STAIRS))
    {
        companion_list[mons->mid].level = lid;
        companion_list[mons->mid].mons = follower(*mons);
        companion_list[mons->mid].timestamp = you.elapsed_time;
    }
}

void update_companions()
{
    for (auto &entry : companion_list)
    {
        monster* mons = monster_by_mid(entry.first);
        if (mons)
        {
            if (mons->is_divine_companion())
            {
                ASSERT(mons->alive());
                entry.second.mons = follower(*mons);
                entry.second.timestamp = you.elapsed_time;
            }
        }
    }
}

/**
 * Attempt to recall an ally from offlevel.
 *
 * @param mid   The ID of the monster to be recalled.
 * @return      Whether the monster was successfully recalled onto the level.
 * Note that the monster may not still be alive or onlevel, due to shafts, etc,
 * but they were here at least briefly!
 */
bool recall_offlevel_ally(mid_t mid)
{
    if (!companion_is_elsewhere(mid, true))
        return false;

    companion* comp = &companion_list[mid];
    monster* mons = comp->mons.place(true);
    if (!mons)
        return false;

    // The monster is now on this level
    remove_monster_from_transit(comp->level, mid);
    comp->level = level_id::current();
    simple_monster_message(*mons, " is recalled.");

    // Now that the monster is onlevel, we can safely apply traps to it.
    // old location isn't very meaningful, so use current loc
    mons->apply_location_effects(mons->pos());
    // check if it was killed/shafted by a trap...
    if (!mons->alive())
        return true; // still successfully recalled!

    // Catch up time for off-level monsters
    // (Suppress messages so that we don't get expiry messages for things that
    // supposedly wore off ages ago)
    {
        msg::suppress msg;

        int turns = you.elapsed_time - comp->timestamp;
        // Note: these are auts, not turns, thus healing is 10 times as fast as
        // for other monsters, confusion goes away after a single turn, etc.

        mons->heal(div_rand_round(turns * mons->off_level_regen_rate(), 100));

        if (turns >= 10 && mons->alive())
        {
            // Remove confusion manually (so that the monster
            // doesn't blink after being recalled)
            mons->del_ench(ENCH_CONFUSION, true);
            mons->timeout_enchantments(turns / 10);
        }
    }
    recall_orders(mons);

    return true;
}

bool companion_is_elsewhere(mid_t mid, bool must_exist)
{
    if (companion_list.count(mid))
    {
        return companion_list[mid].level != level_id::current()
               || (player_in_branch(BRANCH_PANDEMONIUM)
                   && companion_list[mid].level.branch == BRANCH_PANDEMONIUM
                   && !monster_by_mid(mid));
    }

    return !must_exist;
}

void wizard_list_companions()
{
    if (companion_list.size() == 0)
    {
        mpr("You have no companions.");
        return;
    }

    for (auto &entry : companion_list)
    {
        companion &comp = entry.second;
        monster &mon = comp.mons.mons;
        mprf("%s (%d)(%s:%d)", mon.name(DESC_PLAIN, true).c_str(), mon.mid,
             branches[comp.level.branch].abbrevname, comp.level.depth);
    }
}

/**
 * Returns the mid of the current ancestor granted by Hepliaklqana, if any. If none
 * exists, returns MID_NOBODY.
 *
 * The ancestor is *not* guaranteed to be on-level, even if it exists; check
 * the companion_list before doing anything rash!
 *
 * @return  The mid_t of the player's ancestor, or MID_NOBODY if none exists.
 */
mid_t hepliaklqana_ancestor()
{
    for (auto &entry : companion_list)
        if (mons_is_hepliaklqana_ancestor(entry.second.mons.mons.type))
            return entry.first;
    return MID_NOBODY;
}

/**
 * Returns the a pointer to the current ancestor granted by Hepliaklqana, if
 * any. If none exists, returns null.
 *
 * The ancestor is *not* guaranteed to be on-level, even if it exists; check
 * the companion_list before doing anything rash!
 *
 * @return  The player's ancestor, or nullptr if none exists.
 */
monster* hepliaklqana_ancestor_mon()
{
    const mid_t ancestor_mid = hepliaklqana_ancestor();
    if (ancestor_mid == MID_NOBODY)
        return nullptr;

    monster* ancestor = monster_by_mid(ancestor_mid);
    if (ancestor)
        return ancestor;

    for (auto &entry : companion_list)
        if (mons_is_hepliaklqana_ancestor(entry.second.mons.mons.type))
            return &entry.second.mons.mons;
    // should never reach this...
    return nullptr;
}

/**
 * @return true if the Hepliaklqana ancestor is at full HP and the player can
 * see this, or if the ancestor is out of sight or does not exist.
 */
bool ancestor_full_hp()
{
    if (you.religion == GOD_HEPLIAKLQANA)
    {
        monster* ancestor = monster_by_mid(hepliaklqana_ancestor());
        if (ancestor == nullptr)
            return true;
        return !you.can_see(*ancestor)
            || ancestor->hit_points == ancestor->max_hit_points;
    }
    return true;
}

#if TAG_MAJOR_VERSION == 34
// A temporary routine to clean up some references to invalid companions and
// prevent crashes on load. Should be unnecessary once the cloning bugs that
// allow the creation of these invalid companions are fully mopped up
void fixup_bad_companions()
{
    for (auto i = companion_list.begin(); i != companion_list.end();)
    {
        if (invalid_monster_type(i->second.mons.mons.type))
            companion_list.erase(i++);
        else
            ++i;
    }
}

bool maybe_bad_priest_monster(monster &mons)
{
    // prior to e6d7efa92cb0, if a follower got polymorphed to a form that
    // satisfied is_priest, its god got cleared. This resulted in Beogh
    // followers potentially getting cloned on level load, resulting in
    // duplicate mids or a corrupted mid cache depending on ordering. This is
    // now fixed up in tag_read_level_load.
    return mons.alive() && mons.attitude == ATT_FRIENDLY
                        && mons.god == GOD_NAMELESS;
}

void fixup_bad_priest_monster(monster &mons)
{
    if (!maybe_bad_priest_monster(mons))
        return;
    mprf(MSGCH_ERROR, "Removing corrupted ex-follower from level: %s.",
                                            mons.full_name(DESC_PLAIN).c_str());
    monster_die(mons, KILL_RESET, -1, true, false);
}
#endif

// Look at the orc stored in a given slot.
// NOTE: It is not safe to place this apostle on the map! This is only for
//       querying apostle properties, like their mid. Use _beogh_restore_apostle()
//       to actually get a placeable apostle.
static monster* _beogh_peek_apostle(int slot)
{
    // Invalid slots
    if (slot < 0 || slot > 3)
        return nullptr;

    if (!you.props.exists(BEOGH_SAVED_APOSTLES_KEY))
        return nullptr;

    CrawlVector& vec = you.props[BEOGH_SAVED_APOSTLES_KEY].get_vector();

    // Uninitialized vector
    if (slot > vec.size())
        return nullptr;

    CrawlVector& save = vec[slot].get_vector();

    if (save.size() < 1)
        return nullptr;

    monster* apostle = &save[0].get_monster();

    return apostle;
}

static bool _beogh_apostle_is_alive(int slot)
{
    monster* apostle = _beogh_peek_apostle(slot);

    if (!apostle)
        return false;

    monster* real = monster_by_mid(apostle->mid);

    // They currently exist on our floor
    if (real && real->alive())
        return true;

    // They aren't on our floor, but are listed as living companions (so they
    // are presumably alive SOMEWHERE)
    if (companion_list.count(apostle->mid))
        return true;

    return false;
}

static void _remove_offlevel_companion(mid_t mid)
{
    level_excursion le;
    le.go_to(companion_list[mid].level);
    monster* dist_real = monster_by_mid(mid);
    remove_companion(dist_real);
    monster_die(*dist_real, KILL_RESET, -1, true);
}


void beogh_do_ostracism()
{
    mprf(MSGCH_GOD, "Beogh sends your followers elsewhere.");
    for (int i = 0; i < you.props[BEOGH_NUM_FOLLOWERS_KEY].get_int(); ++i)
    {
        if (_beogh_apostle_is_alive(i+1))
        {
            mid_t mid = _beogh_peek_apostle(i+1)->mid;

            string key = BEOGH_FOLLOWER_DEATH_PREFIX + std::to_string(mid);
            you.props[key] = APOSTLE_LEFT;

            if (companion_is_elsewhere(mid))
                _remove_offlevel_companion(mid);
            else
            {
                monster* real = monster_by_mid(mid);
                remove_companion(real);
                monster_die(*real, KILL_RESET, -1, true);
            }
        }
    }
}

void beogh_end_ostracism()
{
    mprf(MSGCH_GOD, "Your apostles return to your side.");

    // XXX: It's kind of ugly to wrap this in the resurrection function, but
    // the process of restoring dead apostles is kind of complicated...
    beogh_resurrect_followers(true);
}

// Save the base state of a given apostle into a given slot.
// Slot 0 = recruitable apostle (not yet joined)
// Slot 1-3 = follower apostles
static void _beogh_save_apostle(monster* apostle, int slot)
{
    CrawlVector& vec = you.props[BEOGH_SAVED_APOSTLES_KEY].get_vector();

    // Initialize vector if isn't aren't already
    if (vec.size() == 0)
    {
        for (int i = 0; i < 4; ++i)
            vec.push_back(0);
    }

    // We save a copy of the apostle monster themselves in [0], with their
    // inventory going in subsequent slots.
    CrawlVector save;
    monster saved_mon(*apostle);
    save.push_back(saved_mon);

    // If we save a monster while its hp is 0 (such as when an apostle is first
    // defeated), it will be reset on marshall. So make sure it doesn't look dead!
    saved_mon.hit_points = saved_mon.max_hit_points;
    saved_mon.timeout_enchantments(1000);
    saved_mon.flags &= ~MF_APOSTLE_BAND;

    // Clone item_defs of apostle inventory and save along with them.
    // NOTE: Monster inventories are merely links to entries in the master item
    // list per floor, and monsters stored in props CANNOT safely have items. We
    // need to save the items separately and give them back to them later.
    for (mon_inv_iterator ii(saved_mon); ii; ++ii)
    {
        item_def clone_item = item_def(*ii);
        clone_item.link = NON_ITEM;
        clone_item.pos.reset();
        saved_mon.unequip(env.item[ii->index()], false, true);
        saved_mon.inv[ii.slot()] = NON_ITEM;
        //mprf("Cloned item: %d, %d, %d, %d", clone_item.base_type, clone_item.sub_type, clone_item.plus, clone_item.special);
        save.push_back(clone_item);
    }

    save[0] = saved_mon;
    vec[slot] = save;
}

static monster* _beogh_restore_apostle(int slot)
{
    monster* apostle = _beogh_peek_apostle(slot);

    if (!apostle)
        return nullptr;

    CrawlVector& vec = you.props[BEOGH_SAVED_APOSTLES_KEY].get_vector();
    CrawlVector& save = vec[slot].get_vector();

    // Clone our stored apostle and initialize it into the monster array
    monster* new_mon = get_free_monster();
    *new_mon = monster(*apostle);
    env.mid_cache[new_mon->mid] = new_mon->mindex();

    // Copy saved monster inventory and link to the new monster
    for (unsigned int i = 1; i < save.size(); ++i)
    {
        item_def clone_item = item_def(save[i].get_item());
        // mprf("Restoring item: %d, %d, %d, %d", clone_item.base_type,
        //         clone_item.sub_type, clone_item.plus, clone_item.special);
        give_specific_item(new_mon, clone_item);
    }

    return new_mon;
}

static int _apostle_challenge_piety_needed()
{
    // Make the first few trials happen more quickly, so the player can get a
    // minimal number of followers. Slow them down over time.
    return 25
     + (min((short)4, you.num_current_gifts[you.religion]) * 12)
     + (min((short)10, you.num_total_gifts[you.religion]) * 4);
}

// Try to find a suitable spot to place a hostile apostle and their band.
// We consider 'suitable' to be something a little bit out of sight of the player,
// with enough room to place multiple land monsters, and which has land
// connectivity with the player's current position.
//
// This function is deliberately not exhaustive. If the player is near
// disconnected areas, to avoid performing pathological pathfinding many, many
// times, we simply pick a handful of random spots to test. If none are valid,
// we reschedule the apostle creation for a later turn, in the hope that the
// player's surroundings are more agreeable by that point.
static coord_def _find_suitable_apostle_pos()
{
    int tries_left = 5;
    for (distance_iterator di(you.pos(), true, true, 17); di && tries_left > 0; ++di)
    {
        if (grid_distance(you.pos(), *di) < 12
            || !feat_has_solid_floor(env.grid(*di))
            || actor_at(*di))
        {
            continue;
        }

        // We're at least marginally viable, so let's try the more expensive checks
        tries_left -= 1;

        // Test if there's room for our band
        int space_found = 0;
        for (radius_iterator ri(*di, LOS_NO_TRANS, true); ri; ++ri)
        {
            if (feat_has_solid_floor(env.grid(*ri)) && !actor_at(*ri))
                ++space_found;

            if (space_found >=5)
                break;
        }

        // Arbitrary cutoff (some bands are smaller than this, but it should
        // cover 'enough' cases)
        if (space_found < 5)
            continue;

        // Finally, test that we can reach the player from here
        monster_pathfind mp;
        mp.set_range(25);   // Don't search further than this
        if (mp.init_pathfind(*di, you.pos()))
            return *di;
    }

    // No spot found. We can try again later.
    return coord_def(0, 0);
}

static monster* _generate_random_apostle(int pow, int band_pow, coord_def pos)
{
    mgen_data mg(MONS_ORC_APOSTLE, BEH_HOSTILE, pos, MHITYOU, MG_AUTOFOE);
    apostle_type type = random_choose(APOSTLE_WARRIOR,
                                      APOSTLE_WIZARD,
                                      APOSTLE_PRIEST);

    mg.props[APOSTLE_TYPE_KEY] = type;
    mg.props[APOSTLE_POWER_KEY] = pow;

    if (band_pow > 0)
    {
        mg.flags |= MG_PERMIT_BANDS;
        mg.props[APOSTLE_BAND_POWER_KEY] = band_pow;
    }

    return create_monster(mg);
}

// (Maybe) generates a random apostle challenge of the appropriate level,
// spawning it on the current floor (with band), sealing level exits, and
// announcing this to the player.
static bool _try_generate_apostle_challenge(int pow, int band_pow)
{
    coord_def pos = _find_suitable_apostle_pos();

    // If we failed to find a valid spot, try again later
    if (pos.origin())
        return false;

    monster* apostle = _generate_random_apostle(pow, band_pow, pos);

    if (!apostle)
        return false;

    apostle->add_ench(mon_enchant(ENCH_TOUCH_OF_BEOGH, 0, 0, INFINITE_DURATION));
    apostle->flags |= (MF_APOSTLE_BAND | MF_HARD_RESET);
    apostle->props[ALWAYS_CORPSE_KEY] = true;

    // Todo: Make sure not to pick the same name as any existing apostle

    simple_god_message(" speaks to you: \"Beware that a challenger has come"
                       " seeking you in battle. Prove your devotion to me!"
                       " Unite all my children beneath one banner!");

    you.duration[DUR_BEOGH_DIVINE_CHALLENGE] = 100;

    // This prevents getting another challenge back to back, but does not slow
    // the player's progress towards the next one, otherwise.
    you.gift_timeout += random_range(10, 25);

    you.props[BEOGH_CHALLENGE_PROGRESS_KEY].get_int() -= _apostle_challenge_piety_needed();

    // Tracking for piety 'cost' of new challenges incrementing
    you.num_current_gifts[you.religion]++;
    you.num_total_gifts[you.religion]++;

    // Apostles can always track the player, and their followers will eventually
    // follow them, but this helps them reach their destination a little better.
    for (monster_iterator mi; mi; ++mi)
    {
        if (mi->is_band_follower_of(*apostle))
        {
            mi->behaviour = BEH_SEEK;
            mi->foe = MHITYOU;
            mi->target = you.pos();
        }
    }

    return true;
}

static bool _is_invalid_challenge_level()
{
    // No Abyss/Pan/Portals/Zot or on rune floors (banning the player from
    // retreating or stairdancing there seems too mean)
    return !is_connected_branch(level_id::current())
           || player_in_branch(BRANCH_ZOT)
           || branch_has_rune(level_id::current().branch)
              && at_branch_bottom();
}

// Try to avoid locking the player into an apostle challenge if they're already
// in a rough situation. This is very approximate and obviously can't catch
// every combination of bad circumstance, but should at least help.
static bool _is_bad_moment_for_challenge()
{
    if (you.hp < you.hp_max / 2
        || player_stair_delay()
        || get_tension() > 45)
    {
        return true;
    }

    return false;
}

// Check if the player is eligable to receive an apostle challenge, and then
// attempt to generate one if they are. (Called every ~10-30 turns)
bool maybe_generate_apostle_challenge()
{
    if (you.religion != GOD_BEOGH || you.piety < piety_breakpoint(2)
        || you.duration[DUR_BEOGH_DIVINE_CHALLENGE]
        || you.gift_timeout
        || _is_invalid_challenge_level()
        || you.props[BEOGH_CHALLENGE_PROGRESS_KEY].get_int()
           < _apostle_challenge_piety_needed()
        || _is_bad_moment_for_challenge())
    {
        return false;
    }

    // Mostly based on number of apostles you've fought, but capped by xl
    int pow_cap = div_rand_round((max(1, you.experience_level - 5)) * 10, 22) * 10;
    int base_pow = min(pow_cap, 10 + min((int)you.num_total_gifts[GOD_BEOGH], 13) * 5);

    // Guarenteed no band for the first 2 apostles.
    int band_pow = you.num_total_gifts[GOD_BEOGH] < 3 ? 0 : base_pow;

    if (you.num_total_gifts[GOD_BEOGH] > 2)
    {
        // Apostle power will never be lower than base level, but there are
        // several rolls to get higher.
        if (one_chance_in(4))
            base_pow += random_range(5, 10);
        if (one_chance_in(3))
            base_pow += random2avg(div_rand_round(base_pow, 2), 2);
    }

    return _try_generate_apostle_challenge(base_pow, band_pow);
}

void flee_apostle_challenge()
{
    mprf(MSGCH_GOD, "Beogh is disappointed with your cowardice.");
    mprf(MSGCH_GOD, "\"Reflect upon your actions, mortal!\"");
    dock_piety(0, 15, true);

    // Remove the apostle (and its band)
    for (monster_iterator mi; mi; ++mi)
    {
        if (mi->flags & MF_APOSTLE_BAND)
            monster_die(**mi, nullptr, true);
    }

    you.duration[DUR_BEOGH_DIVINE_CHALLENGE] = 0;

    take_note(Note(NOTE_FLED_CHALLENGE));
}

void win_apostle_challenge(monster& apostle)
{
    mons_speaks_msg(&apostle, getSpeakString("orc_apostle_yield"), MSGCH_TALK);
    you.duration[DUR_BEOGH_DIVINE_CHALLENGE] = 0;

    apostle.del_ench(ENCH_TOUCH_OF_BEOGH);

    // Count as having gotten vengeance, even though the target isn't 'dead'
    if (apostle.has_ench(ENCH_VENGEANCE_TARGET))
    {
        apostle.del_ench(ENCH_VENGEANCE_TARGET);
        beogh_progress_vengeance();
    }

    apostle.timeout_enchantments(100);
    apostle.attitude = ATT_GOOD_NEUTRAL;
    mons_att_changed(&apostle);
    apostle.stop_constricting_all();
    apostle.stop_being_constricted();

    you.props[BEOGH_RECRUITABLE_APOSTLE_KEY].get_int() = apostle.mid;

    // Save the recruit's current, healthy state
    _beogh_save_apostle(&apostle, 0);

    mprf(MSGCH_GOD, "Beogh will allow you to induct %s into your service.",
         apostle.name(DESC_THE, true).c_str());

    you.duration[DUR_BEOGH_CAN_RECRUIT] = random_range(30, 45) * BASELINE_DELAY;

    // Remind the player how to do this, if they don't already have an apostle
    if (companion_list.empty())
    {
        mprf("(press <w>%c</w> on the <w>%s</w>bility menu to recruit an apostle)",
                get_talent(ABIL_BEOGH_RECRUIT_APOSTLE, false).hotkey,
                command_to_string(CMD_USE_ABILITY).c_str());
    }

    // Remove the rest of the apostle's band
    for (monster_iterator mi; mi; ++mi)
    {
        if (mi->is_band_follower_of(apostle))
        {
            place_cloud(CLOUD_TLOC_ENERGY, mi->pos(), 1 + random2(3), *mi);
            simple_monster_message(**mi, " is recalled by Beogh.");
            monster_die(**mi, KILL_RESET, -1, true);
        }
    }
}

void end_beogh_recruit_window()
{
    monster* apostle = monster_by_mid(you.props[BEOGH_RECRUITABLE_APOSTLE_KEY].get_int());
    if (apostle && !mons_is_god_gift(*apostle))
    {
        simple_monster_message(*apostle, " is recalled by the power of Beogh.");
        place_cloud(CLOUD_TLOC_ENERGY, apostle->pos(), 1 + random2(3), apostle);
        monster_die(*apostle, KILL_RESET, -1, true);
    }
    you.props.erase(BEOGH_RECRUITABLE_APOSTLE_KEY);
}

string get_apostle_name(int slot, bool with_title)
{
    monster* apostle = _beogh_peek_apostle(slot);

    if (!apostle)
        return "Buggy Apostle";

    string name = apostle->name(DESC_PLAIN, true);
    if (with_title)
        name += ", " + apostle_type_names[apostle->props[APOSTLE_TYPE_KEY].get_int()];

    return name;
}

// Attempt to the recall the recruit immediately next to you (slightly better
// for wording of anointing them). If this fails, try to at least place them
// SOMEWHERE.
static bool _try_recall_recruit(monster* mon)
{
    coord_def empty;
    if (find_habitable_spot_near(you.pos(), MONS_ORC_APOSTLE, 1, false, empty))
        return mon->move_to_pos(empty);
    else if (find_habitable_spot_near(you.pos(), MONS_ORC_APOSTLE, 5, false, empty))
        return mon->move_to_pos(empty);

    return false;
}

void beogh_recruit_apostle()
{
    monster* apostle = _beogh_peek_apostle(0);
    ASSERT(apostle);

    monster* real = monster_by_mid(apostle->mid);

    string msg;

    // The apostle we want to recruit is still around on this floor
    if (real && real->alive())
    {
        // Recall them back into our sight, if we can
        if (!you.can_see(*real))
        {
            _try_recall_recruit(real);
            msg += "Beogh recalls " + real->name(DESC_THE, true) + " to your side and ";
        }
    }
    // Apostle died before we could recruit them
    else if (you.props.exists(BEOGH_RECRUITABLE_APOSTLE_DEATH_POS_KEY))
    {
        real = _beogh_restore_apostle(0);
        coord_def pos = you.props[BEOGH_RECRUITABLE_APOSTLE_DEATH_POS_KEY].get_coord();

        if (you.see_cell_no_trans(pos))
        {
            // Revive them in place, if we can see where they died and the space is free
            if (monster_habitable_grid(real, env.grid(pos)) && !actor_at(pos))
                real->move_to_pos(pos);
            else
                _try_recall_recruit(real);

            msg += "Beogh breathes life back into " + real->name(DESC_THE, true) + " and ";
        }
    }
    // Neither dead nor present. They're shaft-immune, so I'm not exactly sure
    // how this happens, but it feels worth accounting for anyway.
    else
    {
        real = _beogh_restore_apostle(0);
        _try_recall_recruit(real);
        msg += "Beogh recalls " + real->name(DESC_THE, true) + " to your side and ";
    }

    if (msg.length() > 0)
        msg += "you anoint them with ash and charcoal and welcome them as a companion.";
    else
        msg += "You anoint " + real->name(DESC_THE, true) + " with ash and charcoal and welcome them as a companion.";

    mpr(msg.c_str());

    // Now atually convert and save the apostle
    int slot = ++you.props[BEOGH_NUM_FOLLOWERS_KEY].get_int();

    real->attitude = ATT_FRIENDLY;
    mons_make_god_gift(*real, GOD_BEOGH);
    add_companion(real);
    mons_att_changed(real);

    // Make the apostle stop wandering around and head back to you.
    real->foe = MHITYOU;
    real->behaviour = BEH_SEEK;
    real->patrol_point.reset();

    _beogh_save_apostle(real, slot);

    you.duration[DUR_BEOGH_CAN_RECRUIT] = 0;
}

static void _cleanup_apostle_corpse(mid_t mid)
{
    const string key = BEOGH_APOSTLE_DEATH_FLOOR_PREFIX + std::to_string(mid);
    level_id floor = you.props[key].get_level_id();

    if (!floor.is_valid())
        return;

    level_excursion le;
    if (floor != level_id::current())
        le.go_to(floor);

    for (int j = 0; j < MAX_ITEMS; ++j)
    {
        if (env.item[j].base_type != OBJ_CORPSES
            || !env.item[j].props.exists(CORPSE_MID_KEY))
        {
            continue;
        }

        if (static_cast<mid_t>(env.item[j].props[CORPSE_MID_KEY].get_int()) == mid)
            destroy_item(j);
    }
}

void beogh_dismiss_apostle(int slot)
{
    ASSERT(slot > 0 && slot < 4);

    monster* apostle = _beogh_peek_apostle(slot);
    ASSERT(apostle);

    string name = apostle->name(DESC_THE, true);
    if (!yesno(make_stringf("Really dismiss %s?", name.c_str()).c_str(), false, 'n'))
    {
        canned_msg(MSG_OK);
        return;
    }

    mprf("You release %s from your service.", name.c_str());

    // Remove our follower monster (if they are elsewhere, use an excursion to
    // remove them immediately.)
    if (companion_is_elsewhere(apostle->mid, true))
    {
        level_excursion le;
        le.go_to(companion_list[apostle->mid].level);
        monster* dist_real = monster_by_mid(apostle->mid);

        // While this should almost always find the original, sometimes it may
        // not (eg: if you left them in the Abyss). Let's double-check.
        if (dist_real)
        {
            remove_companion(dist_real);
            monster_die(*dist_real, KILL_RESET, -1, true);
        }
    }
    else
    {
        monster* real = monster_by_mid(apostle->mid);
        if (real)
        {
            if (you.can_see(*real))
            {
                mons_speaks_msg(real, getSpeakString("orc_apostle_dismissed"), MSGCH_TALK);
                place_cloud(CLOUD_TLOC_ENERGY, real->pos(), 1 + random2(3), apostle);
            }
            remove_companion(real);
            monster_die(*real, KILL_RESET, -1, true);
        }
    }

    _cleanup_apostle_corpse(apostle->mid);

    // Then remove their stored copies
    CrawlVector& vec = you.props[BEOGH_SAVED_APOSTLES_KEY].get_vector();
    vec.erase(slot);
    vec.push_back(0);

    you.props[BEOGH_NUM_FOLLOWERS_KEY].get_int() -= 1;

    //mprf("You now have %d followers.", you.props[BEOGH_NUM_FOLLOWERS_KEY].get_int());
}

void beogh_swear_vegeance(monster& apostle)
{
    bool already_avenging = you.duration[DUR_BEOGH_SEEKING_VENGEANCE];
    if (!already_avenging)
        you.props[BEOGH_VENGEANCE_NUM_KEY].get_int() += 1;

    bool new_targets = false;

    // To keep track of which monsters correspond to which period of avenging
    // (so that off-level things can be cleaned up properly when starting and
    // ending vengeance periods repeatedly)
    int vengenance_num = you.props[BEOGH_VENGEANCE_NUM_KEY].get_int();
    for (radius_iterator ri(apostle.pos(), LOS_NO_TRANS, true); ri; ++ri)
    {
        monster* mon = monster_at(*ri);
        if (mon && !mon->wont_attack() && !mons_is_firewood(*mon)
            && !mon->is_summoned() && !mons_is_conjured(mon->type)
            && !mon->has_ench(ENCH_VENGEANCE_TARGET))
        {
            you.duration[DUR_BEOGH_SEEKING_VENGEANCE] += 1;
            mon->add_ench(mon_enchant(ENCH_VENGEANCE_TARGET, vengenance_num, &you, INFINITE_DURATION));
            mon->patrol_point = apostle.pos();
            new_targets = true;
        }
    }

    if (new_targets)
        mprf(MSGCH_DURATION, "You swear to avenge %s death!", apostle.name(DESC_ITS, true).c_str());

    // Calculate how much additional piety it should cost to resurrect this follower.
    //
    // Stronger apostles take more piety to resurrect (helps a little with making
    // early ones not feel like they're actively slowing everybody else down)
    you.props[BEOGH_RES_PIETY_NEEDED_KEY].get_int() +=
        65 + (apostle.props[APOSTLE_POWER_KEY].get_int() / 2);
}

void beogh_follower_banished(monster& apostle)
{
    // Don't swear vengeance when this happens, and set a very small piety
    // revival counter instead, additionally marking this follower as banished
    you.props[BEOGH_RES_PIETY_NEEDED_KEY].get_int() += random_range(10, 30);

    string key = BEOGH_FOLLOWER_DEATH_PREFIX + std::to_string(apostle.mid);
    you.props[key] = APOSTLE_BANISHED;

    remove_companion(&apostle);
}

void beogh_progress_vengeance()
{
    ASSERT(you.duration[DUR_BEOGH_SEEKING_VENGEANCE]);
    you.duration[DUR_BEOGH_SEEKING_VENGEANCE] -= 1;
    if (you.duration[DUR_BEOGH_SEEKING_VENGEANCE] == 0)
    {
        mprf(MSGCH_DURATION, "You feel as though your fallen companions have been avenged.");

        // This cleanup should usually be unneccessary, since we are only supposed
        // to end when we've killed EVERY marked target, but slime creature
        // splitting (and probably some other methods of cloning) can result in
        // more monsters being marked in total than were marked originally,
        // and subsequently killing one of them will assert.
        for (monster_iterator mi; mi; ++mi)
            mi->del_ench(ENCH_VENGEANCE_TARGET);
        add_daction(DACT_BEOGH_VENGEANCE_CLEANUP);

        you.props[BEOGH_RES_PIETY_NEEDED_KEY].get_int() /= 4;
        beogh_progress_resurrection(0);
    }
}

// Increment our progress to the next follower resurrection (if any are dead).
// Called whenever piety is gained and also when revenge is complete
void beogh_progress_resurrection(int amount)
{
    // Nobody's dead
    if (you.props[BEOGH_RES_PIETY_NEEDED_KEY].get_int() == 0)
        return;

    you.props[BEOGH_RES_PIETY_GAINED_KEY].get_int() += amount;
    if (you.props[BEOGH_RES_PIETY_NEEDED_KEY].get_int()
        <= you.props[BEOGH_RES_PIETY_GAINED_KEY].get_int())
    {
        // This is often triggered via killing monsters, and can cause an
        // excursion before that monster is finished dying. This is needed to
        // avoid the dying monster entering an invalid state.
        beogh_resurrection_fineff::schedule();
    }
}

static bool _apostle_was_banished(const monster& apostle)
{
    const string key = BEOGH_FOLLOWER_DEATH_PREFIX + std::to_string(apostle.mid);
    return you.props.exists(key) && you.props[key].get_int() == APOSTLE_BANISHED;
}

static bool _apostle_left_you(const monster& apostle)
{
    const string key = BEOGH_FOLLOWER_DEATH_PREFIX + std::to_string(apostle.mid);
    return you.props.exists(key) && you.props[key].get_int() == APOSTLE_LEFT;
}

void beogh_resurrect_followers(bool end_ostracism_only)
{
    vector<monster*> dead_apostles;
    vector<string> revived_names;
    vector<bool> was_banished;

    for (int i = 0; i < you.props[BEOGH_NUM_FOLLOWERS_KEY].get_int(); ++i)
    {
        if (!_beogh_apostle_is_alive(i+1))
        {
            // If we are merely returning followers after penance, skip any
            // followers who are dead for 'real'.
            if (end_ostracism_only && !_apostle_left_you(*_beogh_peek_apostle(i+1)))
                continue;

            monster* apostle = _beogh_restore_apostle(i+1);
            dead_apostles.push_back(apostle);

            if (_apostle_was_banished(*apostle))
            {
                // Rough up our poor Abyss escapeee
                apostle->hit_points -= random2avg(apostle->max_hit_points - 1, 3);
                if (coinflip())
                    apostle->add_ench(ENCH_WRETCHED);

                was_banished.push_back(true);
            }
            else
            {
                revived_names.push_back(apostle->name(DESC_THE, true));
                was_banished.push_back(false);
            }

            const string key = BEOGH_FOLLOWER_DEATH_PREFIX + std::to_string(apostle->mid);
            you.props.erase(key);
        }
    }

    for (unsigned int i = 0; i < dead_apostles.size(); ++i)
    {
        bool placed = false;

        coord_def empty;
        if (find_habitable_spot_near(you.pos(), MONS_ORC_APOSTLE, 2, false, empty))
            placed = dead_apostles[i]->move_to_pos(empty);
        else if (find_habitable_spot_near(you.pos(), MONS_ORC_APOSTLE, LOS_RADIUS, false, empty))
            placed = dead_apostles[i]->move_to_pos(empty);

        // If there was somehow nowhere to place them, put them back in 'limbo',
        // but as a companion so they can be recalled later.
        if (!placed)
        {
            add_companion(dead_apostles[i]);
            monster_die(*dead_apostles[i], KILL_RESET, -1, true, false);
        }
        else
        {
            add_companion(dead_apostles[i]);

            if (was_banished[i])
            {
                simple_monster_message(*dead_apostles[i],
                                " has fought their way back out of the Abyss!");
                mons_speaks_msg(dead_apostles[i],
                    getSpeakString("orc_apostle_unbanished"), MSGCH_TALK);
            }
        }
    }

    // The rest of the bookkeeping only applies to real resurrections
    if (end_ostracism_only)
        return;

    // Now clean up the corpses
    for (unsigned int i = 0; i < dead_apostles.size(); ++i)
    {
        // No corpses to clean up in this case
        if (was_banished[i])
            continue;

        _cleanup_apostle_corpse(dead_apostles[i]->mid);
    }

    if (!revived_names.empty())
    {
        mpr_comma_separated_list("Beogh breathes life back into ", revived_names,
                                " and ", ", ", MSGCH_GOD);
    }

    you.props.erase(BEOGH_RES_PIETY_GAINED_KEY);
    you.props.erase(BEOGH_RES_PIETY_NEEDED_KEY);

    // End vengeance statuses (in case we revived companions without finishing them)
    you.duration[DUR_BEOGH_SEEKING_VENGEANCE] = 0;
    add_daction(DACT_BEOGH_VENGEANCE_CLEANUP);

    // Increment how many times vengeance has been declared (so that the daction
    // will only clean up past marks and not future ones)
    you.props[BEOGH_VENGEANCE_NUM_KEY].get_int() += 1;
}

// Save where the given follower died, so that we can clean up their corpse later
void beogh_note_follower_death(const monster& apostle)
{
    string key = BEOGH_APOSTLE_DEATH_FLOOR_PREFIX + std::to_string(apostle.mid);
    you.props[key] = level_id::current();
}

bool tile_has_valid_bfb_corpse(const coord_def pos)
{
    for (stack_iterator si(pos); si; ++si)
    {
        if (si->is_type(OBJ_CORPSES, CORPSE_BODY)
            && si->props.exists(BEOGH_BFB_VALID_KEY))
        {
            return true;
        }
    }

    return false;
}
