/*
 * File:      shout.cc
 * Summary:   Stealth, noise, shouting.
 */

#include "AppHdr.h"

#include "shout.h"

#include "branch.h"
#include "cluautil.h"
#include "coord.h"
#include "database.h"
#include "dlua.h"
#include "env.h"
#include "ghost.h"
#include "jobs.h"
#include "libutil.h"
#include "message.h"
#include "misc.h"
#include "mon-behv.h"
#include "mon-iter.h"
#include "mon-place.h"
#include "mon-pathfind.h"
#include "mon-stuff.h"
#include "mon-util.h"
#include "monster.h"
#include "player.h"
#include "random.h"
#include "skills.h"
#include "state.h"
#include "stuff.h"
#include "areas.h"
#include "hints.h"
#include "view.h"

#include <sstream>

extern int stealth;             // defined in main.cc

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
    case S_TRUMPET:
        default_msg_key = "__TRUMPET";
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
    // Search for player ghost shout by the ghost's job.
    else if (monster->type == MONS_PLAYER_GHOST)
    {
        const ghost_demon &ghost = *(monster->ghost);
        std::string ghost_job    = get_job_name(ghost.job);

        key = ghost_job + " player ghost";

        default_msg_key = "player ghost";
    }

    // Tries to find an entry for "name seen" or "name unseen",
    // and if no such entry exists then looks simply for "name".
    // We don't use "you.can_see(monster)" here since that would return
    // false for submerged monsters, but submerged monsters will be forced
    // to surface before they shout, thus removing that source of
    // non-visibility.
    if (you.can_see(monster))
        suffix = " seen";
    else
        suffix = " unseen";

    if (monster->props.exists("shout_func"))
    {
        lua_stack_cleaner clean(dlua);

        dlua_chunk &chunk = monster->props["shout_func"];

        if (!chunk.load(dlua))
        {
            push_monster(dlua, monster);
            clua_pushcxxstring(dlua, suffix);
            dlua.callfn(NULL, 2, 1);
            dlua.fnreturns(">s", &msg);

            // __NONE means to be silent, and __NEXT or __DEFAULT means to try
            // the next method of getting a shout message.
            if (msg == "__NONE")
                return;
            if (msg == "__DEFAULT" || msg == "__NEXT")
                msg.clear();
        }
        else
        {
            mprf(MSGCH_ERROR,
                 "Lua shout function for monster '%s' didn't load: %s",
                 monster->full_name(DESC_PLAIN).c_str(),
                 dlua.error.c_str());
        }
    }

    if (msg.empty())
        msg = getShoutString(key, suffix);

    if (msg == "__DEFAULT" || msg == "__NEXT")
        msg = getShoutString(default_msg_key, suffix);
    else if (msg.empty())
    {
        char mchar = mons_base_char(monster->type);

        // See if there's a shout for all monsters using the
        // same glyph/symbol
        std::string glyph_key = "'";

        // Database keys are case-insensitve.
        if (isaupper(mchar))
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

    if (Hints.hints_left && (heard || you.can_see(monster)))
        learned_something_new(HINT_MONSTER_SHOUT, monster->pos());
}

void force_monster_shout(monsters* monster)
{
    handle_monster_shouts(monster, true);
}

bool check_awaken(monsters* monster)
{
    // Usually redundant because we iterate over player LOS,
    // but e.g. for you.xray_vision.
    if (!monster->see_cell(you.pos()))
        return (false);

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

    const item_def *body_armour = you.slot_item(EQ_BODY_ARMOUR, false);
    const int armour_mass = body_armour? item_mass(*body_armour) : 0;
    // You didn't wake the monster!
    if (!x_chance_in_y(armour_mass, 1000)
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

    // high ambient noise makes sounds harder to hear
    int ambient = current_level_ambient_noise();
    if (ambient < 0) {
        loudness += random2avg(abs(ambient), 3);
    }
    else {
        loudness -= random2avg(abs(ambient), 3);
    }

    if (loudness <= 0)
        return (false);

    // [ds] Reduce noise propagation for Sprint.
    if (crawl_state.game_is_sprint())
        loudness = std::max(1, div_rand_round(loudness, 3));

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

    for (monster_iterator mi; mi; ++mi)
    {
        if (!mi->alive())
            continue;

        // Monsters put to sleep by ensorcelled hibernation will sleep
        // at least one turn.
        if (mi->has_ench(ENCH_SLEEPY))
            return (false);

        // Monsters arent' affected by their own noise.  We don't check
        // where == mi->pos() since it might be caused by the
        // Projected Noise spell.
        if (mi->mindex() == who)
            continue;

        if (distance(mi->pos(), where) <= dist
            && !silenced(mi->pos()))
        {
            // If the noise came from the character, any nearby monster
            // will be jumping on top of them.
            if (where == you.pos())
                behaviour_event(*mi, ME_ALERT, MHITYOU, you.pos());
            else if (mermaid && mons_primary_habitat(*mi) == HT_WATER
                     && !mi->friendly())
            {
                // Mermaids/sirens call (hostile) aquatic monsters.
                behaviour_event(*mi, ME_ALERT, MHITNOT, where);
            }
            else
                behaviour_event(*mi, ME_DISTURB, MHITNOT, where);
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

void blood_smell(int strength, const coord_def& where)
{
    const int range = strength * strength;
    dprf("blood stain at (%d, %d), range of smell = %d",
         where.x, where.y, range);

    // Of the player species, only Vampires can smell blood.
    if (you.species == SP_VAMPIRE)
    {
        // Whether they actually do so, depends on their hunger state.
        int vamp_strength = strength - 2 * (you.hunger_state - 1);
        if (vamp_strength > 0)
        {
            int vamp_range = vamp_strength * vamp_strength;

            const int player_distance = distance(you.pos(), where);

            if (player_distance <= vamp_range)
            {
                dprf("Player smells blood, pos: (%d, %d), dist = %d)",
                     you.pos().x, you.pos().y, player_distance);
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

    circle_def c(where, range, C_CIRCLE);
    for (monster_iterator mi(&c); mi; ++mi)
    {
        if (!mons_class_flag(mi->type, M_BLOOD_SCENT))
            continue;

        // Let sleeping hounds lie.
        if (mi->asleep()
            && mons_species(mi->type) != MONS_VAMPIRE
            && mi->type != MONS_SHARK)
        {
            // 33% chance of sleeping on
            // 33% of being disturbed (start BEH_WANDER)
            // 33% of being alerted   (start BEH_SEEK)
            if (!one_chance_in(3))
            {
                if (coinflip())
                {
                    dprf("disturbing %s (%d, %d)",
                         mi->name(DESC_PLAIN).c_str(),
                         mi->pos().x, mi->pos().y);
                    behaviour_event(*mi, ME_DISTURB, MHITNOT, where);
                }
                continue;
            }
        }
        dprf("alerting %s (%d, %d)",
             mi->name(DESC_PLAIN).c_str(),
             mi->pos().x, mi->pos().y);
        behaviour_event(*mi, ME_ALERT, MHITNOT, where);

        if (mi->type == MONS_SHARK)
        {
            // Sharks go into a battle frenzy if they smell blood.
            monster_pathfind mp;
            if (mp.init_pathfind(*mi, where))
            {
                mon_enchant ench = mi->get_ench(ENCH_BATTLE_FRENZY);
                const int dist = 15 - (mi->pos() - where).rdist();
                const int dur  = random_range(dist, dist*2)
                                 * speed_to_duration(mi->speed);

                if (ench.ench != ENCH_NONE)
                {
                    int level = ench.degree;
                    if (level < 4 && one_chance_in(2*level))
                        ench.degree++;
                    ench.duration = std::max(ench.duration, dur);
                    mi->update_ench(ench);
                }
                else
                {
                    mi->add_ench(mon_enchant(ENCH_BATTLE_FRENZY, 1,
                                             KC_OTHER, dur));
                    simple_monster_message(*mi, " is consumed with "
                                                "blood-lust!");
                }
            }
        }
    }
}
