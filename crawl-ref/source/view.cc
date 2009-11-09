/*
 *  File:       view.cc
 *  Summary:    Misc function used to render the dungeon.
 *  Written by: Linley Henzell
 */

#include "AppHdr.h"

#include "view.h"

#include <stdint.h>
#include <string.h>
#include <cmath>
#include <sstream>
#include <algorithm>
#include <memory>

#ifdef TARGET_OS_DOS
#include <conio.h>
#endif

#include "externs.h"

#include "map_knowledge.h"
#include "fprop.h"
#include "viewchar.h"
#include "viewgeom.h"
#include "viewmap.h"
#include "showsymb.h"

#include "attitude-change.h"
#include "branch.h"
#include "command.h"
#include "cio.h"
#include "cloud.h"
#include "clua.h"
#include "colour.h"
#include "database.h"
#include "debug.h"
#include "delay.h"
#include "dgnevent.h"
#include "directn.h"
#include "dungeon.h"
#include "exclude.h"
#include "feature.h"
#include "files.h"
#include "format.h"
#include "ghost.h"
#include "godabil.h"
#include "goditem.h"
#include "itemprop.h"
#include "los.h"
#include "macro.h"
#include "message.h"
#include "misc.h"
#include "mon-behv.h"
#include "monplace.h"
#include "monstuff.h"
#include "mon-util.h"
#include "newgame.h"
#include "options.h"
#include "jobs.h"
#include "notes.h"
#include "output.h"
#include "overmap.h"
#include "place.h"
#include "player.h"
#include "random.h"
#include "religion.h"
#include "skills.h"
#include "stuff.h"
#include "spells3.h"
#include "stash.h"
#include "tiles.h"
#include "travel.h"
#include "state.h"
#include "terrain.h"
#include "tilemcache.h"
#include "tilesdl.h"
#include "travel.h"
#include "tutorial.h"
#include "xom.h"

#define DEBUG_PANE_BOUNDS 0

crawl_view_geometry crawl_view;

extern int stealth;             // defined in acr.cc

bool inside_level_bounds(int x, int y)
{
    return (x > 0 && x < GXM && y > 0 && y < GYM);
}

bool inside_level_bounds(const coord_def &p)
{
    return (inside_level_bounds(p.x, p.y));
}

bool is_notable_terrain(dungeon_feature_type ftype)
{
    return (get_feature_def(ftype).is_notable());
}

void handle_seen_interrupt(monsters* monster)
{
    if (mons_is_unknown_mimic(monster))
        return;

    activity_interrupt_data aid(monster);
    if (!monster->seen_context.empty())
        aid.context = monster->seen_context;
    else if (testbits(monster->flags, MF_WAS_IN_VIEW))
        aid.context = "already seen";
    else
        aid.context = "newly seen";

    if (!mons_is_safe(monster)
        && !mons_class_flag(monster->type, M_NO_EXP_GAIN))
    {
        interrupt_activity(AI_SEE_MONSTER, aid);
    }
    seen_monster( monster );
}

void flush_comes_into_view()
{
    if (!you.turn_is_over
        || (!you_are_delayed() && !crawl_state.is_repeating_cmd()))
    {
        return;
    }

    monsters* mon = crawl_state.which_mon_acting();

    if (!mon || !mon->alive() || (mon->flags & MF_WAS_IN_VIEW)
        || !you.can_see(mon))
    {
        return;
    }

    handle_seen_interrupt(mon);
}

void handle_monster_shouts(monsters* monster, bool force)
{
    if (!force && x_chance_in_y(you.skills[SK_STEALTH], 30))
        return;

    // Friendly or neutral monsters don't shout.
    if (!force && (monster->friendly() || monster->neutral()))
        return;

    // Get it once, since monster might be S_RANDOM, in which case
    // mons_shouts() will return a different value every time.
    // Demon lords will insult you as a greeting, but later we'll
    // choose a random verb and loudness for them.
    shout_type  s_type = mons_shouts(monster->type, false);

    // Silent monsters can give noiseless "visual shouts" if the
    // player can see them, in which case silence isn't checked for.
    if (s_type == S_SILENT && !monster->visible_to(&you)
        || s_type != S_SILENT && !player_can_hear(monster->pos()))
    {
        return;
    }

    mon_acting mact(monster);

    std::string default_msg_key = "";

    switch (s_type)
    {
    case S_SILENT:
        // No default message.
        break;
    case S_SHOUT:
        default_msg_key = "__SHOUT";
        break;
    case S_BARK:
        default_msg_key = "__BARK";
        break;
    case S_SHOUT2:
        default_msg_key = "__TWO_SHOUTS";
        break;
    case S_ROAR:
        default_msg_key = "__ROAR";
        break;
    case S_SCREAM:
        default_msg_key = "__SCREAM";
        break;
    case S_BELLOW:
        default_msg_key = "__BELLOW";
        break;
    case S_SCREECH:
        default_msg_key = "__SCREECH";
        break;
    case S_BUZZ:
        default_msg_key = "__BUZZ";
        break;
    case S_MOAN:
        default_msg_key = "__MOAN";
        break;
    case S_GURGLE:
        default_msg_key = "__GURGLE";
        break;
    case S_WHINE:
        default_msg_key = "__WHINE";
        break;
    case S_CROAK:
        default_msg_key = "__CROAK";
        break;
    case S_GROWL:
        default_msg_key = "__GROWL";
        break;
    case S_HISS:
        default_msg_key = "__HISS";
        break;
    case S_DEMON_TAUNT:
        default_msg_key = "__DEMON_TAUNT";
        break;
    default:
        default_msg_key = "__BUGGY"; // S_LOUD, S_VERY_SOFT, etc. (loudness)
    }

    // Now that we have the message key, get a random verb and noise level
    // for pandemonium lords.
    if (s_type == S_DEMON_TAUNT)
        s_type = mons_shouts(monster->type, true);

    std::string msg, suffix;
    std::string key = mons_type_name(monster->type, DESC_PLAIN);

    // Pandemonium demons have random names, so use "pandemonium lord"
    if (monster->type == MONS_PANDEMONIUM_DEMON)
        key = "pandemonium lord";
    // Search for player ghost shout by the ghost's class.
    else if (monster->type == MONS_PLAYER_GHOST)
    {
        const ghost_demon &ghost = *(monster->ghost);
        std::string ghost_class = get_class_name(ghost.job);

        key = ghost_class + " player ghost";

        default_msg_key = "player ghost";
    }

    // Tries to find an entry for "name seen" or "name unseen",
    // and if no such entry exists then looks simply for "name".
    // We don't use "you.can_see(monster)" here since that would return
    // false for submerged monsters, but submerged monsters will be forced
    // to surface before they shout, thus removing that source of
    // non-visibility.
    if (mons_near(monster) && (!monster->invisible() || you.can_see_invisible()))
        suffix = " seen";
    else
        suffix = " unseen";

    msg = getShoutString(key, suffix);

    if (msg == "__DEFAULT" || msg == "__NEXT")
        msg = getShoutString(default_msg_key, suffix);
    else if (msg.empty())
    {
        // NOTE: Use the hardcoded glyph rather than that returned
        // by mons_char(), since the result of mons_char() can be
        // changed by user settings.
        char mchar = get_monster_data(monster->type)->showchar;

        // See if there's a shout for all monsters using the
        // same glyph/symbol
        std::string glyph_key = "'";

        // Database keys are case-insensitve.
        if (isupper(mchar))
            glyph_key += "cap-";

        glyph_key += mchar;
        glyph_key += "'";
        msg = getShoutString(glyph_key, suffix);

        if (msg.empty() || msg == "__DEFAULT")
            msg = getShoutString(default_msg_key, suffix);
    }

    if (default_msg_key == "__BUGGY")
    {
        msg::streams(MSGCH_SOUND) << "You hear something buggy!"
                                  << std::endl;
    }
    else if (s_type == S_SILENT && (msg.empty() || msg == "__NONE"))
    {
        ; // No "visual shout" defined for silent monster, do nothing.
    }
    else if (msg.empty()) // Still nothing found?
    {
        msg::streams(MSGCH_DIAGNOSTICS)
            << "No shout entry for default shout type '"
            << default_msg_key << "'" << std::endl;

        msg::streams(MSGCH_SOUND) << "You hear something buggy!"
                                  << std::endl;
    }
    else if (msg == "__NONE")
    {
        msg::streams(MSGCH_DIAGNOSTICS)
            << "__NONE returned as shout for non-silent monster '"
            << default_msg_key << "'" << std::endl;
        msg::streams(MSGCH_SOUND) << "You hear something buggy!"
                                  << std::endl;
    }
    else
    {
        msg_channel_type channel = MSGCH_TALK;

        std::string param = "";
        std::string::size_type pos = msg.find(":");

        if (pos != std::string::npos)
        {
            param = msg.substr(0, pos);
            msg   = msg.substr(pos + 1);
        }

        if (s_type == S_SILENT || param == "VISUAL")
            channel = MSGCH_TALK_VISUAL;
        else if (param == "SOUND")
            channel = MSGCH_SOUND;

        // Monster must come up from being submerged if it wants to shout.
        if (monster->submerged())
        {
            if (!monster->del_ench(ENCH_SUBMERGED))
            {
                // Couldn't unsubmerge.
                return;
            }

            if (you.can_see(monster))
            {
                if (monster->type == MONS_AIR_ELEMENTAL)
                    monster->seen_context = "thin air";
                else if (monster->type == MONS_TRAPDOOR_SPIDER)
                    monster->seen_context = "leaps out";
                else if (!monster_habitable_grid(monster, DNGN_FLOOR))
                    monster->seen_context = "bursts forth shouting";
                else
                    monster->seen_context = "surfaces";

                // Give interrupt message before shout message.
                handle_seen_interrupt(monster);
            }
        }

        if (channel != MSGCH_TALK_VISUAL || you.can_see(monster))
        {
            msg = do_mon_str_replacements(msg, monster, s_type);
            msg::streams(channel) << msg << std::endl;

            // Otherwise it can move away with no feedback.
            if (you.can_see(monster))
            {
                if (!(monster->flags & MF_WAS_IN_VIEW))
                    handle_seen_interrupt(monster);
                seen_monster(monster);
            }
        }
    }

    const int  noise_level = get_shout_noise_level(s_type);
    const bool heard       = noisy(noise_level, monster->pos(), monster->mindex());

    if (Options.tutorial_left && (heard || you.can_see(monster)))
        learned_something_new(TUT_MONSTER_SHOUT, monster->pos());
}

#ifdef WIZARD
void force_monster_shout(monsters* monster)
{
    handle_monster_shouts(monster, true);
}
#endif

void monster_grid(bool do_updates)
{
    do_updates = do_updates && !crawl_state.arena;

    for (int s = 0; s < MAX_MONSTERS; ++s)
    {
        monsters *monster = &menv[s];

        if (monster->alive() && mons_near(monster))
        {
            if (do_updates && (monster->asleep()
                               || mons_is_wandering(monster))
                && check_awaken(monster))
            {
                behaviour_event(monster, ME_ALERT, MHITYOU, you.pos(), false);
                handle_monster_shouts(monster);
            }

            // [enne] - It's possible that mgrd and monster->x/y are out of
            // sync because they are updated separately.  If we can see this
            // monster, then make sure that the mgrd is set correctly.
            if (mgrd(monster->pos()) != s)
            {
                // If this mprf triggers for you, please note any special
                // circumstances so we can track down where this is coming
                // from.
                mprf(MSGCH_ERROR, "monster %s (%d) at (%d, %d) was "
                     "improperly placed.  Updating mgrd.",
                     monster->name(DESC_PLAIN, true).c_str(), s,
                     monster->pos().x, monster->pos().y);
                ASSERT(!monster_at(monster->pos()));
                mgrd(monster->pos()) = s;
            }

            if (!env.show.update_monster(monster))
                continue;

#ifdef USE_TILE
            tile_place_monster(monster->pos().x, monster->pos().y, s, true);
#endif

            good_god_follower_attitude_change(monster);
            beogh_follower_convert(monster);
            slime_convert(monster);
            fedhas_neutralise(monster);
        }
    }
}

void update_monsters_in_view()
{
    unsigned int num_hostile = 0;

    for (int s = 0; s < MAX_MONSTERS; s++)
    {
        monsters *monster = &menv[s];

        if (!monster->alive())
            continue;

        if (mons_near(monster))
        {
            if (monster->attitude == ATT_HOSTILE)
                num_hostile++;

            if (mons_is_unknown_mimic(monster))
            {
                // For unknown mimics, don't mark as seen,
                // but do mark it as in view for later messaging.
                // FIXME: is this correct?
                monster->flags |= MF_WAS_IN_VIEW;
            }
            else if (monster->visible_to(&you))
            {
                handle_seen_interrupt(monster);
                seen_monster(monster);
            }
            else
                monster->flags &= ~MF_WAS_IN_VIEW;
        }
        else
            monster->flags &= ~MF_WAS_IN_VIEW;

        // If the monster hasn't been seen by the time that the player
        // gets control back then seen_context is out of date.
        monster->seen_context.clear();
    }

    // Xom thinks it's hilarious the way the player picks up an ever
    // growing entourage of monsters while running through the Abyss.
    // To approximate this, if the number of hostile monsters in view
    // is greater than it ever was for this particular trip to the
    // Abyss, Xom is stimulated in proportion to the number of
    // hostile monsters.  Thus if the entourage doesn't grow, then
    // Xom becomes bored.
    if (you.level_type == LEVEL_ABYSS
        && you.attribute[ATTR_ABYSS_ENTOURAGE] < num_hostile)
    {
        you.attribute[ATTR_ABYSS_ENTOURAGE] = num_hostile;
        xom_is_stimulated(16 * num_hostile);
    }
}

bool check_awaken(monsters* monster)
{
    // Monsters put to sleep by ensorcelled hibernation will sleep
    // at least one turn.
    if (monster->has_ench(ENCH_SLEEPY))
        return (false);

    // Berserkers aren't really concerned about stealth.
    if (you.berserk())
        return (true);

    // I assume that creatures who can sense invisible are very perceptive.
    int mons_perc = 10 + (mons_intel(monster) * 4) + monster->hit_dice
                       + mons_sense_invis(monster) * 5;

    bool unnatural_stealthy = false; // "stealthy" only because of invisibility?

    // Critters that are wandering but still have MHITYOU as their foe are
    // still actively on guard for the player, even if they can't see you.
    // Give them a large bonus -- handle_behaviour() will nuke 'foe' after
    // a while, removing this bonus.
    if (mons_is_wandering(monster) && monster->foe == MHITYOU)
        mons_perc += 15;

    if (!you.visible_to(monster))
    {
        mons_perc -= 75;
        unnatural_stealthy = true;
    }

    if (monster->asleep())
    {
        if (monster->holiness() == MH_NATURAL)
        {
            // Monster is "hibernating"... reduce chance of waking.
            if (monster->has_ench(ENCH_SLEEP_WARY))
                mons_perc -= 10;
        }
        else // unnatural creature
        {
            // Unnatural monsters don't actually "sleep", they just
            // haven't noticed an intruder yet... we'll assume that
            // they're diligently on guard.
            mons_perc += 10;
        }
    }

    // If you've been tagged with Corona or are Glowing, the glow
    // makes you extremely unstealthy.
    if (you.backlit() && you.visible_to(monster))
        mons_perc += 50;

    if (mons_perc < 0)
        mons_perc = 0;

    if (x_chance_in_y(mons_perc + 1, stealth))
        return (true); // Oops, the monster wakes up!

    // You didn't wake the monster!
    if (player_light_armour(true)
        && you.can_see(monster) // to avoid leaking information
        && you.burden_state == BS_UNENCUMBERED
        && !you.attribute[ATTR_SHADOWS]
        && !monster->wont_attack()
        && !mons_class_flag(monster->type, M_NO_EXP_GAIN)
            // If invisible, training happens much more rarely.
        && (!unnatural_stealthy && one_chance_in(25) || one_chance_in(100)))
    {
        exercise(SK_STEALTH, 1);
    }

    return (false);
}

// Noisy now has a messenging service for giving messages to the
// player is appropriate.
//
// Returns true if the PC heard the noise.
bool noisy(int loudness, const coord_def& where, const char *msg, int who,
           bool mermaid)
{
    bool ret = false;

    if (loudness <= 0)
        return (false);

    // If the origin is silenced there is no noise.
    if (silenced(where))
        return (false);

    const int dist = loudness * loudness;
    const int player_distance = distance( you.pos(), where );

    // Message the player.
    if (player_distance <= dist && player_can_hear( where ))
    {
        if (msg)
            mpr( msg, MSGCH_SOUND );

        you.check_awaken(dist - player_distance);

        if (!mermaid)
            you.beholders_check_noise(loudness);

        ret = true;
    }

    for (int p = 0; p < MAX_MONSTERS; p++)
    {
        monsters* monster = &menv[p];

        if (!monster->alive())
            continue;

        // Monsters arent' affected by their own noise.  We don't check
        // where == monster->pos() since it might be caused by the
        // Projected Noise spell.
        if (p == who)
            continue;

        if (distance(monster->pos(), where) <= dist
            && !silenced(monster->pos()))
        {
            // If the noise came from the character, any nearby monster
            // will be jumping on top of them.
            if (where == you.pos())
                behaviour_event( monster, ME_ALERT, MHITYOU, you.pos() );
            else if (mermaid && mons_primary_habitat(monster) == HT_WATER
                     && !monster->friendly())
            {
                // Mermaids/sirens call (hostile) aquatic monsters.
                behaviour_event( monster, ME_ALERT, MHITNOT, where );
            }
            else
                behaviour_event( monster, ME_DISTURB, MHITNOT, where );
        }
    }

    return (ret);
}

bool noisy(int loudness, const coord_def& where, int who,
           bool mermaid)
{
    return noisy(loudness, where, NULL, who, mermaid);
}

static const char* _player_vampire_smells_blood(int dist)
{
    // non-thirsty vampires get no clear indication of how close the
    // smell is
    if (you.hunger_state >= HS_SATIATED)
        return "";

    if (dist < 16) // 4*4
        return " near-by";

    if (you.hunger_state <= HS_NEAR_STARVING && dist > get_los_radius_sq())
        return " in the distance";

    return "";
}

void blood_smell( int strength, const coord_def& where )
{
    monsters *monster = NULL;

    const int range = strength * strength;
#ifdef DEBUG_DIAGNOSTICS
    mprf(MSGCH_DIAGNOSTICS,
         "blood stain at (%d, %d), range of smell = %d",
         where.x, where.y, range);
#endif

    // Of the player species, only Vampires can smell blood.
    if (you.species == SP_VAMPIRE)
    {
        // Whether they actually do so, depends on their hunger state.
        int vamp_strength = strength - 2 * (you.hunger_state - 1);
        if (vamp_strength > 0)
        {
            int vamp_range = vamp_strength * vamp_strength;

            const int player_distance = distance( you.pos(), where );

            if (player_distance <= vamp_range)
            {
#ifdef DEBUG_DIAGNOSTICS
                mprf(MSGCH_DIAGNOSTICS,
                     "Player smells blood, pos: (%d, %d), dist = %d)",
                     you.pos().x, you.pos().y, player_distance);
#endif
                you.check_awaken(range - player_distance);
                // Don't message if you can see the square.
                if (!you.see_cell(where))
                {
                    mprf("You smell fresh blood%s.",
                         _player_vampire_smells_blood(player_distance));
                }
            }
        }
    }

    for (int p = 0; p < MAX_MONSTERS; p++)
    {
        monster = &menv[p];

        if (monster->type < 0)
            continue;

        if (!mons_class_flag(monster->type, M_BLOOD_SCENT))
            continue;

        if (distance(monster->pos(), where) <= range)
        {
            // Let sleeping hounds lie.
            if (monster->asleep()
                && mons_species(monster->type) != MONS_VAMPIRE
                && monster->type != MONS_SHARK)
            {
                // 33% chance of sleeping on
                // 33% of being disturbed (start BEH_WANDER)
                // 33% of being alerted   (start BEH_SEEK)
                if (!one_chance_in(3))
                {
                    if (coinflip())
                    {
#ifdef DEBUG_DIAGNOSTICS
                        mprf(MSGCH_DIAGNOSTICS, "disturbing %s (%d, %d)",
                             monster->name(DESC_PLAIN).c_str(),
                             monster->pos().x, monster->pos().y);
#endif
                        behaviour_event(monster, ME_DISTURB, MHITNOT, where);
                    }
                    continue;
                }
            }
#ifdef DEBUG_DIAGNOSTICS
            mprf(MSGCH_DIAGNOSTICS, "alerting %s (%d, %d)",
                 monster->name(DESC_PLAIN).c_str(),
                 monster->pos().x, monster->pos().y);
#endif
            behaviour_event( monster, ME_ALERT, MHITNOT, where );

            if (monster->type == MONS_SHARK)
            {
                // Sharks go into a battle frenzy if they smell blood.
                monster_pathfind mp;
                if (mp.init_pathfind(monster, where))
                {
                    mon_enchant ench = monster->get_ench(ENCH_BATTLE_FRENZY);
                    const int dist = 15 - (monster->pos() - where).rdist();
                    const int dur  = random_range(dist, dist*2)
                                     * speed_to_duration(monster->speed);

                    if (ench.ench != ENCH_NONE)
                    {
                        int level = ench.degree;
                        if (level < 4 && one_chance_in(2*level))
                            ench.degree++;
                        ench.duration = std::max(ench.duration, dur);
                        monster->update_ench(ench);
                    }
                    else
                    {
                        monster->add_ench(mon_enchant(ENCH_BATTLE_FRENZY, 1,
                                                      KC_OTHER, dur));
                        simple_monster_message(monster, " is consumed with "
                                                        "blood-lust!");
                    }
                }
            }
        }
    }
}


// We logically associate a difficulty parameter with each tile on each level,
// to make deterministic magic mapping work.  This function returns the
// difficulty parameters for each tile on the current level, whose difficulty
// is less than a certain amount.
//
// Random difficulties are used in the few cases where we want repeated maps
// to give different results; scrolls and cards, since they are a finite
// resource.
static const FixedArray<char, GXM, GYM>& _tile_difficulties(bool random)
{
    // We will often be called with the same level parameter and cutoff, so
    // cache this (DS with passive mapping autoexploring could be 5000 calls
    // in a second or so).
    static FixedArray<char, GXM, GYM> cache;
    static int cache_seed = -1;

    int seed = random ? -1 :
        (static_cast<int>(you.where_are_you) << 8) + you.your_level - 1731813538;

    if (seed == cache_seed && !random)
    {
        return cache;
    }

    if (!random)
    {
        push_rng_state();
        seed_rng(cache_seed);
    }

    cache_seed = seed;

    for (int y = Y_BOUND_1; y <= Y_BOUND_2; ++y)
        for (int x = X_BOUND_1; x <= X_BOUND_2; ++x)
            cache[x][y] = random2(100);

    if (!random)
    {
        pop_rng_state();
    }

    return cache;
}

static std::auto_ptr<FixedArray<bool, GXM, GYM> > _tile_detectability()
{
    std::auto_ptr<FixedArray<bool, GXM, GYM> > map(new FixedArray<bool, GXM, GYM>);

    std::vector<coord_def> flood_from;

    for (int x = X_BOUND_1; x <= X_BOUND_2; ++x)
        for (int y = Y_BOUND_1; y <= Y_BOUND_2; ++y)
        {
            (*map)(coord_def(x,y)) = false;

            if (feat_is_stair(grd[x][y]))
            {
                flood_from.push_back(coord_def(x, y));
            }
        }

    flood_from.push_back(you.pos());

    while (!flood_from.empty())
    {
        coord_def p = flood_from.back();
        flood_from.pop_back();

        if ((*map)(p))
            continue;

        (*map)(p) = true;

        if (grd(p) < DNGN_MINSEE && grd(p) != DNGN_CLOSED_DOOR)
            continue;

        for (int dy = -1; dy <= 1; ++dy)
            for (int dx = -1; dx <= 1; ++dx)
                flood_from.push_back(p + coord_def(dx,dy));
    }

    return map;
}

// Returns true if it succeeded.
bool magic_mapping(int map_radius, int proportion, bool suppress_msg,
                   bool force, bool deterministic, bool circular,
                   coord_def pos)
{
    if (!in_bounds(pos))
        pos = you.pos();

    if (!force
        && (testbits(env.level_flags, LFLAG_NO_MAGIC_MAP)
            || testbits(get_branch_flags(), BFLAG_NO_MAGIC_MAP)))
    {
        if (!suppress_msg)
            mpr("You feel momentarily disoriented.");

        return (false);
    }

    const bool wizard_map = (you.wizard && map_radius == 1000);

    if (!wizard_map)
    {
        if (map_radius > 50)
            map_radius = 50;
        else if (map_radius < 5)
            map_radius = 5;
    }

    // now gradually weaker with distance:
    const int pfar     = (map_radius * 7) / 10;
    const int very_far = (map_radius * 9) / 10;

    bool did_map = false;
    int  num_altars        = 0;
    int  num_shops_portals = 0;

    const FixedArray<char, GXM, GYM>& difficulty =
        _tile_difficulties(!deterministic);

    std::auto_ptr<FixedArray<bool, GXM, GYM> > detectable;

    if (!deterministic)
        detectable = _tile_detectability();

    for (radius_iterator ri(pos, map_radius, !circular, false); ri; ++ri)
    {
        if (!wizard_map)
        {
            int threshold = proportion;

            const int dist = grid_distance( you.pos(), *ri );

            if (dist > very_far)
                threshold = threshold / 3;
            else if (dist > pfar)
                threshold = threshold * 2 / 3;

            if (difficulty(*ri) > threshold)
                continue;
        }

        if (is_terrain_changed(*ri))
            clear_map_knowledge_grid(*ri);

        if (!wizard_map && (is_terrain_seen(*ri) || is_terrain_mapped(*ri)))
            continue;

        if (!wizard_map && !deterministic && !((*detectable)(*ri)))
            continue;

        const dungeon_feature_type feat = grd(*ri);

        bool open = true;

        if (feat_is_solid(feat) && !feat_is_closed_door(feat))
        {
            open = false;
            for (adjacent_iterator ai(*ri); ai; ++ai)
            {
                if (map_bounds(*ai) && (!feat_is_opaque(grd(*ai))
                                        || feat_is_closed_door(grd(*ai))))
                {
                    open = true;
                    break;
                }
            }
        }

        if (open)
        {
            if (wizard_map || !get_map_knowledge_obj(*ri))
                set_map_knowledge_obj(*ri, grd(*ri));

            if (wizard_map)
            {
                if (is_notable_terrain(feat))
                    seen_notable_thing(feat, *ri);

                set_terrain_seen(*ri);
#ifdef USE_TILE
                // Can't use set_map_knowledge_obj because that would
                // overwrite the gmap.
                env.tile_bk_bg(*ri) = tile_idx_unseen_terrain(ri->x, ri->y,
                                                              grd(*ri));
#endif
            }
            else
            {
                set_terrain_mapped(*ri);

                if (get_feature_dchar(feat) == DCHAR_ALTAR)
                    num_altars++;
                else if (get_feature_dchar(feat) == DCHAR_ARCH)
                    num_shops_portals++;
            }

            did_map = true;
        }
    }

    if (!suppress_msg)
    {
        mpr(did_map ? "You feel aware of your surroundings."
                    : "You feel momentarily disoriented.");

        std::vector<std::string> sensed;

        if (num_altars > 0)
            sensed.push_back(make_stringf("%d altar%s", num_altars,
                                          num_altars > 1 ? "s" : ""));

        if (num_shops_portals > 0)
        {
            const char* plur = num_shops_portals > 1 ? "s" : "";
            sensed.push_back(make_stringf("%d shop%s/portal%s",
                                          num_shops_portals, plur, plur));
        }

        if (!sensed.empty())
            mpr_comma_separated_list("You sensed ", sensed);
    }

    return (did_map);
}

// Is the given monster near (in LOS of) the player?
bool mons_near(const monsters *monster)
{
    if (crawl_state.arena || crawl_state.arena_suspended)
        return (true);
    return (you.see_cell(monster->pos()));
}

bool mon_enemies_around(const monsters *monster)
{
    // If the monster has a foe, return true.
    if (monster->foe != MHITNOT && monster->foe != MHITYOU)
        return (true);

    if (crawl_state.arena)
    {
        // If the arena-mode code in _handle_behaviour() hasn't set a foe then
        // we don't have one.
        return (false);
    }
    else if (monster->wont_attack())
    {
        // Additionally, if an ally is nearby and *you* have a foe,
        // consider it as the ally's enemy too.
        return (mons_near(monster) && there_are_monsters_nearby(true));
    }
    else
    {
        // For hostile monsters *you* are the main enemy.
        return (mons_near(monster));
    }
}

// Returns a string containing an ASCII representation of the map. If fullscreen
// is set to false, only the viewable area is returned. Leading and trailing
// spaces are trimmed from each line. Leading and trailing empty lines are also
// snipped.
std::string screenshot(bool fullscreen)
{
    UNUSED(fullscreen);

    // [ds] Screenshots need to be straight ASCII. We will now proceed to force
    // the char and feature tables back to ASCII.
    FixedVector<unsigned, NUM_DCHAR_TYPES> char_table_bk;
    char_table_bk = Options.char_table;

    init_char_table(CSET_ASCII);
    init_show_table();

    int firstnonspace = -1;
    int firstpopline  = -1;
    int lastpopline   = -1;

    std::vector<std::string> lines(crawl_view.viewsz.y);
    for (int count_y = 1; count_y <= crawl_view.viewsz.y; count_y++)
    {
        int lastnonspace = -1;

        for (int count_x = 1; count_x <= crawl_view.viewsz.x; count_x++)
        {
            // in grid coords
            const coord_def gc = view2grid(coord_def(count_x, count_y));

            int ch =
                  (!map_bounds(gc))             ? 0 :
                  (!crawl_view.in_grid_los(gc)) ? get_map_knowledge_char(gc.x, gc.y) :
                  (gc == you.pos())             ? you.symbol
                                                : get_screen_glyph(gc.x, gc.y);

            if (ch && !isprint(ch))
            {
                // [ds] Evil hack time again. Peek at grid, use that character.
                show_type object = show_type(grid_appearance(gc));
                unsigned glych;
                unsigned short glycol = 0;

                get_symbol( gc, object, &glych, &glycol );
                ch = glych;
            }

            // More mangling to accommodate C strings.
            if (!ch)
                ch = ' ';

            if (ch != ' ')
            {
                lastnonspace = count_x;
                lastpopline = count_y;

                if (firstnonspace == -1 || firstnonspace > count_x)
                    firstnonspace = count_x;

                if (firstpopline == -1)
                    firstpopline = count_y;
            }

            lines[count_y - 1] += ch;
        }

        if (lastnonspace < (int) lines[count_y - 1].length())
            lines[count_y - 1].erase(lastnonspace + 1);
    }

    // Restore char and feature tables.
    Options.char_table = char_table_bk;
    init_show_table();

    std::ostringstream ss;
    if (firstpopline != -1 && lastpopline != -1)
    {
        if (firstnonspace == -1)
            firstnonspace = 0;

        for (int i = firstpopline; i <= lastpopline; ++i)
        {
            const std::string &ref = lines[i - 1];
            if (firstnonspace < (int) ref.length())
                ss << ref.substr(firstnonspace);
            ss << EOL;
        }
    }

    return (ss.str());
}

static int _viewmap_flash_colour()
{
    if (you.attribute[ATTR_SHADOWS])
        return (DARKGREY);
    else if (you.berserk())
        return (RED);

    return (BLACK);
}

// Updates one square of the view area. Should only be called for square
// in LOS.
void view_update_at(const coord_def &pos)
{
    if (pos == you.pos())
        return;

    const coord_def vp = grid2view(pos);
    const coord_def ep = view2show(vp);
    env.show.update_at(pos, ep);

    show_type object = env.show(ep);

    if (!object)
        return;

    unsigned short  colour = object.colour;
    unsigned        ch = 0;

    get_symbol( pos, object, &ch, &colour );

    int flash_colour = you.flash_colour;
    if (flash_colour == BLACK)
        flash_colour = _viewmap_flash_colour();

#ifndef USE_TILE
    cgotoxy(vp.x, vp.y);
    put_colour_ch(flash_colour? real_colour(flash_colour) : colour, ch);

    // Force colour back to normal, else clrscr() will flood screen
    // with this colour on DOS.
    textattr(LIGHTGREY);
#endif
}

#ifndef USE_TILE
void flash_monster_colour(const monsters *mon, unsigned char fmc_colour,
                          int fmc_delay)
{
    if (you.can_see(mon))
    {
        unsigned char old_flash_colour = you.flash_colour;
        coord_def c(mon->pos());

        you.flash_colour = fmc_colour;
        view_update_at(c);

        update_screen();
        delay(fmc_delay);

        you.flash_colour = old_flash_colour;
        view_update_at(c);
        update_screen();
    }
}
#endif

bool view_update()
{
    if (you.num_turns > you.last_view_update)
    {
        viewwindow(true, false);
        return (true);
    }
    return (false);
}

static void _debug_pane_bounds()
{
#if DEBUG_PANE_BOUNDS
    // Doesn't work for HUD because print_stats() overwrites it.
    // To debug HUD, add viewwindow(false,false) at end of _prep_input.

    if (crawl_view.mlistsz.y > 0)
    {
        textcolor(WHITE);
        cgotoxy(1,1, GOTO_MLIST);
        cprintf("+   L");
        cgotoxy(crawl_view.mlistsz.x-4, crawl_view.mlistsz.y, GOTO_MLIST);
        cprintf("L   +");
    }

    cgotoxy(1,1, GOTO_STAT);
    cprintf("+  H");
    cgotoxy(crawl_view.hudsz.x-3, crawl_view.hudsz.y, GOTO_STAT);
    cprintf("H  +");

    cgotoxy(1,1, GOTO_MSG);
    cprintf("+ M");
    cgotoxy(crawl_view.msgsz.x-2, crawl_view.msgsz.y, GOTO_MSG);
    cprintf("M +");

    cgotoxy(crawl_view.viewp.x, crawl_view.viewp.y);
    cprintf("+V");
    cgotoxy(crawl_view.viewp.x+crawl_view.viewsz.x-2,
            crawl_view.viewp.y+crawl_view.viewsz.y-1);
    cprintf("V+");
    textcolor(LIGHTGREY);
#endif
}

//---------------------------------------------------------------
//
// viewwindow -- now unified and rolled into a single pass
//
// Draws the main window using the character set returned
// by get_symbol().
//
// This function should not interfere with the game condition,
// unless do_updates is set (ie. stealth checks for visible
// monsters).
//
//---------------------------------------------------------------
void viewwindow(bool draw_it, bool do_updates)
{
    if (you.duration[DUR_TIME_STEP])
        return;
    flush_prev_message();

#ifdef USE_TILE
    std::vector<unsigned int> tileb(
        crawl_view.viewsz.y * crawl_view.viewsz.x * 2);
    tiles.clear_text_tags(TAG_NAMED_MONSTER);
#endif
    screen_buffer_t *buffy(crawl_view.vbuf);

    int count_x, count_y;

    calc_show_los();

#ifdef USE_TILE
    tile_draw_floor();
    mcache.clear_nonref();
#endif

    env.show.init();

    monster_grid(do_updates);

#ifdef USE_TILE
    tile_draw_rays(true);
    tiles.clear_overlays();
#endif

    if (draw_it)
    {
        cursor_control cs(false);

        const bool map = player_in_mappable_area();
        const bool draw =
#ifdef USE_TILE
            !is_resting() &&
#endif
            (!you.running || Options.travel_delay > -1
             || you.running.is_explore() && Options.explore_delay > -1)
            && !you.asleep();

        int bufcount = 0;

        int flash_colour = you.flash_colour;
        if (flash_colour == BLACK)
            flash_colour = _viewmap_flash_colour();

        std::vector<coord_def> update_excludes;
        for (count_y = crawl_view.viewp.y;
             count_y < crawl_view.viewp.y + crawl_view.viewsz.y; count_y++)
        {
            for (count_x = crawl_view.viewp.x;
                 count_x < crawl_view.viewp.x + crawl_view.viewsz.x; count_x++)
            {
                // in grid coords
                const coord_def gc(view2grid(coord_def(count_x, count_y)));
                const coord_def ep = view2show(grid2view(gc));

                if (in_bounds(gc) && you.see_cell(gc))
                    maybe_remove_autoexclusion(gc);

                // Print tutorial messages for features in LOS.
                if (Options.tutorial_left
                    && in_bounds(gc)
                    && crawl_view.in_grid_los(gc)
                    && env.show(ep))
                {
                    tutorial_observe_cell(gc);
                }

                // Order is important here.
                if (!map_bounds(gc))
                {
                    // off the map
                    buffy[bufcount]     = 0;
                    buffy[bufcount + 1] = DARKGREY;
#ifdef USE_TILE
                    tileidx_unseen(tileb[bufcount], tileb[bufcount+1], ' ', gc);
#endif
                }
                else if (!crawl_view.in_grid_los(gc))
                {
                    // Outside the env.show area.
                    buffy[bufcount]     = get_map_knowledge_char(gc);
                    buffy[bufcount + 1] = DARKGREY;

                    if (Options.colour_map)
                    {
                        buffy[bufcount + 1] =
                            colour_code_map(gc, Options.item_colour);
                    }

#ifdef USE_TILE
                    unsigned int bg = env.tile_bk_bg(gc);
                    unsigned int fg = env.tile_bk_fg(gc);
                    if (bg == 0 && fg == 0)
                        tileidx_unseen(fg, bg, get_map_knowledge_char(gc), gc);

                    tileb[bufcount]     = fg;
                    tileb[bufcount + 1] = bg | tile_unseen_flag(gc);
#endif
                }
                else if (gc == you.pos() && !crawl_state.arena
                         && !crawl_state.arena_suspended)
                {
                    show_type       object = env.show(ep);
                    unsigned short  colour = object.colour;
                    unsigned        ch;
                    get_symbol(gc, object, &ch, &colour);

                    if (map)
                    {
                        set_map_knowledge_glyph(gc, object, colour);
                        if (is_terrain_changed(gc) || !is_terrain_seen(gc))
                            update_excludes.push_back(gc);

                        set_terrain_seen(gc);
                        set_map_knowledge_detected_mons(gc, false);
                        set_map_knowledge_detected_item(gc, false);
                    }
#ifdef USE_TILE
                    if (map)
                    {
                        env.tile_bk_bg(gc) = env.tile_bg(ep);
                        env.tile_bk_fg(gc) = env.tile_fg(ep);
                    }

                    tileb[bufcount] = env.tile_fg(ep) =
                        tileidx_player(you.char_class);
                    tileb[bufcount+1] = env.tile_bg(ep);
#endif

                    // Player overrides everything in cell.
                    buffy[bufcount]     = you.symbol;
                    buffy[bufcount + 1] = you.colour;

                    if (you.swimming())
                    {
                        if (grd(gc) == DNGN_DEEP_WATER)
                            buffy[bufcount + 1] = BLUE;
                        else
                            buffy[bufcount + 1] = CYAN;
                    }
                }
                else
                {
                    show_type  object = env.show(ep);
                    unsigned short colour = object.colour;
                    unsigned        ch;

                    get_symbol( gc, object, &ch, &colour );

                    buffy[bufcount]     = ch;
                    buffy[bufcount + 1] = colour;
#ifdef USE_TILE
                    tileb[bufcount]   = env.tile_fg(ep);
                    tileb[bufcount+1] = env.tile_bg(ep);
#endif

                    if (map)
                    {
                        // This section is very tricky because it
                        // duplicates the old code (which was horrid).

                        // If the grid is in LoS env.show was set and
                        // we set the buffer already, so...
                        if (buffy[bufcount] != 0)
                        {
                            // ... map that we've seen this
                            if (is_terrain_changed(gc) || !is_terrain_seen(gc))
                                update_excludes.push_back(gc);

                            set_terrain_seen(gc);
                            set_map_knowledge_glyph(gc, object, colour );
                            set_map_knowledge_detected_mons(gc, false);
                            set_map_knowledge_detected_item(gc, false);
#ifdef USE_TILE
                            // We remove any references to mcache when
                            // writing to the background.
                            if (Options.clean_map)
                            {
                                env.tile_bk_fg(gc) =
                                    get_clean_map_idx(env.tile_fg(ep));
                            }
                            else
                            {
                                env.tile_bk_fg(gc) = env.tile_fg(ep);
                            }
                            env.tile_bk_bg(gc) = env.tile_bg(ep);
#endif
                        }

                        // Check if we're looking to clean_map...
                        // but don't touch the buffer to clean it,
                        // instead we modify the env.map_knowledge itself so
                        // that the map stays clean as it moves out
                        // of the env.show radius.
                        //
                        // Note: show_backup is 0 on every square which
                        // is inside the env.show radius and doesn't
                        // have a monster or cloud on it, and is equal
                        // to the grid before monsters and clouds were
                        // added otherwise.
                        if (Options.clean_map
                            && env.show.get_backup(ep)
                            && is_terrain_seen(gc))
                        {
                            get_symbol(gc, env.show.get_backup(ep), &ch, &colour);
                            set_map_knowledge_glyph(gc, env.show.get_backup(ep), colour);
                        }

                        // Now we get to filling in both the unseen
                        // grids in the env.show radius area as
                        // well doing the clean_map.  The clean_map
                        // is done by having the env.map_knowledge set to the
                        // backup character above, and down here we
                        // procede to override that character if it's
                        // out of LoS!  If it wasn't, buffy would have
                        // already been set (but we'd still have
                        // clobbered env.map_knowledge... which is important
                        // to do for when we move away from the area!)
                        if (buffy[bufcount] == 0)
                        {
                            // Show map.
                            buffy[bufcount]     = get_map_knowledge_char(gc);
                            buffy[bufcount + 1] = DARKGREY;

                            if (Options.colour_map)
                            {
                                buffy[bufcount + 1] =
                                    colour_code_map(gc, Options.item_colour);
                            }
#ifdef USE_TILE
                            if (env.tile_bk_fg(gc) != 0
                                || env.tile_bk_bg(gc) != 0)
                            {
                                tileb[bufcount] = env.tile_bk_fg(gc);

                                tileb[bufcount + 1] =
                                    env.tile_bk_bg(gc) | tile_unseen_flag(gc);
                            }
                            else
                            {
                                tileidx_unseen(tileb[bufcount],
                                               tileb[bufcount+1],
                                               get_map_knowledge_char(gc),
                                               gc);
                            }
#endif
                        }
                    }
                }

                // Alter colour if flashing the characters vision.
                if (flash_colour && buffy[bufcount])
                {
                    buffy[bufcount + 1] =
                        observe_cell(gc) ? real_colour(flash_colour)
                                         : DARKGREY;
                }
                else if (Options.target_range > 0 && buffy[bufcount]
                         && (grid_distance(you.pos(), gc) > Options.target_range
                             || !observe_cell(gc)))
                {
                    buffy[bufcount + 1] = DARKGREY;
#ifdef USE_TILE
                    if (observe_cell(gc))
                        tileb[bufcount + 1] |= TILE_FLAG_OOR;
#endif
                }

                bufcount += 2;
            }
        }

        update_exclusion_los(update_excludes);

        // Leaving it this way because short flashes can occur in long ones,
        // and this simply works without requiring a stack.
        you.flash_colour = BLACK;

        // Avoiding unneeded draws when running.
        if (draw)
        {
#ifdef USE_TILE
            tiles.set_need_redraw();
            tiles.load_dungeon(&tileb[0], crawl_view.vgrdc);
            tiles.update_inventory();
#else
            you.last_view_update = you.num_turns;
            puttext(crawl_view.viewp.x, crawl_view.viewp.y,
                    crawl_view.viewp.x + crawl_view.viewsz.x - 1,
                    crawl_view.viewp.y + crawl_view.viewsz.y - 1,
                    buffy);

            update_monster_pane();
#endif
        }
    }

    _debug_pane_bounds();
}

////////////////////////////////////////////////////////////////////////////
// Term resize handling (generic).

void handle_terminal_resize(bool redraw)
{
    crawl_state.terminal_resized = false;

    if (crawl_state.terminal_resize_handler)
        (*crawl_state.terminal_resize_handler)();
    else
        crawl_view.init_geometry();

    if (redraw)
        redraw_screen();
}
