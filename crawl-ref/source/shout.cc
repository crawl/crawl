/**
 * @file
 * @brief Stealth, noise, shouting.
**/

#include "AppHdr.h"

#include "shout.h"

#include <sstream>

#include "act-iter.h"
#include "areas.h"
#include "artefact.h"
#include "art-enum.h"
#include "branch.h"
#include "database.h"
#include "directn.h"
#include "english.h"
#include "env.h"
#include "exercise.h"
#include "ghost.h"
#include "god-abil.h"
#include "hints.h"
#include "jobs.h"
#include "libutil.h"
#include "macro.h"
#include "message.h"
#include "mon-behv.h"
#include "mon-place.h"
#include "mon-poly.h"
#include "prompt.h"
#include "religion.h"
#include "state.h"
#include "stringutil.h"
#include "terrain.h"
#include "view.h"

static noise_grid _noise_grid;
static void _actor_apply_noise(actor *act,
                               const coord_def &apparent_source,
                               int noise_intensity_millis,
                               const noise_t &noise,
                               int noise_travel_distance);

/// By default, what databse lookup key corresponds to each shout type?
static const map<shout_type, string> default_msg_keys = {
    { S_SILENT,         "" },
    { S_SHOUT,          "__SHOUT" },
    { S_BARK,           "__BARK" },
    { S_HOWL,           "__HOWL" },
    { S_SHOUT2,         "__TWO_SHOUTS" },
    { S_ROAR,           "__ROAR" },
    { S_SCREAM,         "__SCREAM" },
    { S_BELLOW,         "__BELLOW" },
    { S_BLEAT,          "__BLEAT" },
    { S_TRUMPET,        "__TRUMPET" },
    { S_SCREECH,        "__SCREECH" },
    { S_BUZZ,           "__BUZZ" },
    { S_MOAN,           "__MOAN" },
    { S_GURGLE,         "__GURGLE" },
    { S_CROAK,          "__CROAK" },
    { S_GROWL,          "__GROWL" },
    { S_HISS,           "__HISS" },
    { S_DEMON_TAUNT,    "__DEMON_TAUNT" },
    { S_CHERUB,         "__CHERUB" },
    { S_SQUEAL,         "__SQUEAL" },
    { S_LOUD_ROAR,      "__LOUD_ROAR" },
};

/**
 * What's the appropriate DB lookup key for a given monster's shouts?
 *
 * @param mons      The monster in question.
 * @return          A name for the monster; e.g. "orc", "Kirke", "pandemonium
 *                  lord", "Fire Elementalist player ghost".
 */
static string _shout_key(const monster &mons)
{
    // Pandemonium demons have random names, so use "pandemonium lord"
    if (mons.type == MONS_PANDEMONIUM_LORD)
        return "pandemonium lord";

    // Search for player ghost shout by the ghost's job.
    if (mons.type == MONS_PLAYER_GHOST)
    {
        const ghost_demon &ghost = *(mons.ghost);
        const string ghost_job         = get_job_name(ghost.job);
        return ghost_job + " player ghost";
    }

    // everything else just goes by name.
    return mons_type_name(mons.type, DESC_PLAIN);
}

/**
 * Let a monster consider whether or not it wants to shout, and, if so, shout.
 *
 * @param mon       The monster in question.
 */
void monster_consider_shouting(monster &mon)
{
    if (one_chance_in(5))
        return;

    // Friendly or neutral monsters don't shout.
    // XXX: redundant with one of two uses (mon-behv.cc)
    if (mon.friendly() || mon.neutral())
        return;

    monster_attempt_shout(mon);
}

/**
 * If it's at all possible for a monster to shout, have it do so.
 *
 * @param mon       The monster in question.
 * @return          Whether a shout occurred.
 */
bool monster_attempt_shout(monster &mon)
{
    if (mon.cannot_move() || mon.asleep() || mon.has_ench(ENCH_DUMB))
        return false;

    const shout_type shout = mons_shouts(mon.type, false);

    // Silent monsters can give noiseless "visual shouts" if the
    // player can see them, in which case silence isn't checked for.
    // Muted & silenced monsters can't shout at all.
    if (shout == S_SILENT && !mon.visible_to(&you)
        || shout != S_SILENT && mon.is_silenced())
    {
        return false;
    }

    monster_shout(&mon, shout);
    return true;
}


/**
 * Have a monster perform a specific shout.
 *
 * @param mons      The monster in question.
 *                  TODO: use a reference, not a pointer
 * @param shout    The shout_type to use.
 */
void monster_shout(monster* mons, int shout)
{
    shout_type s_type = static_cast<shout_type>(shout);
    mon_acting mact(mons);

    // less specific, more specific.
    const string default_msg_key
        = mons->type == MONS_PLAYER_GHOST ?
                 "player ghost" :
                 lookup(default_msg_keys, s_type, "__BUGGY");
    const string key = _shout_key(*mons);

    // Now that we have the message key, get a random verb and noise level
    // for pandemonium lords.
    if (s_type == S_DEMON_TAUNT)
        s_type = mons_shouts(mons->type, true);

    // Tries to find an entry for "name seen" or "name unseen",
    // and if no such entry exists then looks simply for "name".
    const string suffix = you.can_see(*mons) ? " seen" : " unseen";
    string message = getShoutString(key, suffix);

    if (message == "__DEFAULT" || message == "__NEXT")
        message = getShoutString(default_msg_key, suffix);
    else if (message.empty())
    {
        char mchar = mons_base_char(mons->type);

        // See if there's a shout for all monsters using the
        // same glyph/symbol
        string glyph_key = "'";

        // Database keys are case-insensitve.
        if (isaupper(mchar))
            glyph_key += "cap-";

        glyph_key += mchar;
        glyph_key += "'";
        message = getShoutString(glyph_key, suffix);

        if (message.empty() || message == "__DEFAULT")
            message = getShoutString(default_msg_key, suffix);
    }

    if (default_msg_key == "__BUGGY")
    {
        msg::streams(MSGCH_SOUND) << "You hear something buggy!"
                                  << endl;
    }
    else if (s_type == S_SILENT && (message.empty() || message == "__NONE"))
        ; // No "visual shout" defined for silent monster, do nothing.
    else if (message.empty()) // Still nothing found?
    {
        msg::streams(MSGCH_DIAGNOSTICS)
            << "No shout entry for default shout type '"
            << default_msg_key << "'" << endl;

        msg::streams(MSGCH_SOUND) << "You hear something buggy!"
                                  << endl;
    }
    else if (message == "__NONE")
    {
        msg::streams(MSGCH_DIAGNOSTICS)
            << "__NONE returned as shout for non-silent monster '"
            << default_msg_key << "'" << endl;
        msg::streams(MSGCH_SOUND) << "You hear something buggy!"
                                  << endl;
    }
    else if (s_type == S_SILENT || !silenced(you.pos()))
    {
        msg_channel_type channel = MSGCH_TALK;
        if (s_type == S_SILENT)
            channel = MSGCH_TALK_VISUAL;

        strip_channel_prefix(message, channel);

        // Monster must come up from being submerged if it wants to shout.
        // XXX: this code is probably unreachable now?
        if (mons->submerged())
        {
            if (!mons->del_ench(ENCH_SUBMERGED))
            {
                // Couldn't unsubmerge.
                return;
            }

            if (you.can_see(*mons))
            {
                mons->seen_context = SC_FISH_SURFACES;

                // Give interrupt message before shout message.
                handle_seen_interrupt(mons);
            }
        }

        if (channel != MSGCH_TALK_VISUAL || you.can_see(*mons))
        {
            // Otherwise it can move away with no feedback.
            if (you.can_see(*mons))
            {
                if (!(mons->flags & MF_WAS_IN_VIEW))
                    handle_seen_interrupt(mons);
                seen_monster(mons);
            }

            message = do_mon_str_replacements(message, *mons, s_type);
            msg::streams(channel) << message << endl;
        }
    }

    const int  noise_level = get_shout_noise_level(s_type);
    const bool heard       = noisy(noise_level, mons->pos(), mons->mid);

    if (crawl_state.game_is_hints() && (heard || you.can_see(*mons)))
        learned_something_new(HINT_MONSTER_SHOUT, mons->pos());
}

bool check_awaken(monster* mons, int stealth)
{
    // Usually redundant because we iterate over player LOS,
    // but e.g. for you.xray_vision.
    if (!mons->see_cell(you.pos()))
        return false;

    // Monsters put to sleep by ensorcelled hibernation will sleep
    // at least one turn.
    if (mons_just_slept(*mons))
        return false;

    // Berserkers aren't really concerned about stealth.
    if (you.berserk())
        return true;

    // If you've sacrificed stealth, you always alert monsters.
    if (player_mutation_level(MUT_NO_STEALTH))
        return true;


    int mons_perc = 10 + (mons_intel(*mons) * 4) + mons->get_hit_dice();

    bool unnatural_stealthy = false; // "stealthy" only because of invisibility?

    // Critters that are wandering but still have MHITYOU as their foe are
    // still actively on guard for the player, even if they can't see you.
    // Give them a large bonus -- handle_behaviour() will nuke 'foe' after
    // a while, removing this bonus.
    if (mons_is_wandering(*mons) && mons->foe == MHITYOU)
        mons_perc += 15;

    if (!you.visible_to(mons))
    {
        mons_perc -= 75;
        unnatural_stealthy = true;
    }

    if (mons->asleep())
    {
        if (mons->holiness() & MH_NATURAL)
        {
            // Monster is "hibernating"... reduce chance of waking.
            if (mons->has_ench(ENCH_SLEEP_WARY))
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

    if (mons_perc < 4)
        mons_perc = 4;

    if (x_chance_in_y(mons_perc + 1, stealth))
        return true; // Oops, the monster wakes up!

    // You didn't wake the monster!
    if (you.can_see(*mons) // to avoid leaking information
        && !mons->wont_attack()
        && !mons->neutral() // include pacified monsters
        && mons_class_gives_xp(mons->type))
    {
        practise_sneaking(unnatural_stealthy);
    }

    return false;
}

void item_noise(const item_def &item, string msg, int loudness)
{
    if (is_unrandom_artefact(item))
    {
        // "Your Singing Sword" sounds disrespectful
        // (as if there could be more than one!)
        msg = replace_all(msg, "@Your_weapon@", "@The_weapon@");
        msg = replace_all(msg, "@your_weapon@", "@the_weapon@");
    }

    // Set appropriate channel (will usually be TALK).
    msg_channel_type channel = MSGCH_TALK;

    string param;
    const string::size_type pos = msg.find(":");

    if (pos != string::npos)
        param = msg.substr(0, pos);

    if (!param.empty())
    {
        bool match = true;

        if (param == "DANGER")
            channel = MSGCH_DANGER;
        else if (param == "WARN")
            channel = MSGCH_WARN;
        else if (param == "SOUND")
            channel = MSGCH_SOUND;
        else if (param == "PLAIN")
            channel = MSGCH_PLAIN;
        else if (param == "SPELL")
            channel = MSGCH_FRIEND_SPELL;
        else if (param == "ENCHANT")
            channel = MSGCH_FRIEND_ENCHANT;
        else if (param == "VISUAL")
            channel = MSGCH_TALK_VISUAL;
        else if (param != "TALK")
            match = false;

        if (match)
            msg = msg.substr(pos + 1);
    }

    if (msg.empty()) // give default noises
    {
        channel = MSGCH_SOUND;
        msg = "You hear a strange noise.";
    }

    // Replace weapon references. Can't use DESC_THE because that includes
    // pluses etc. and we want just the basename.
    msg = replace_all(msg, "@The_weapon@", "The @weapon@");
    msg = replace_all(msg, "@the_weapon@", "the @weapon@");
    msg = replace_all(msg, "@Your_weapon@", "Your @weapon@");
    msg = replace_all(msg, "@your_weapon@", "your @weapon@");
    msg = replace_all(msg, "@weapon@", item.name(DESC_BASENAME));

    // replace references to player name and god
    msg = replace_all(msg, "@player_name@", you.your_name);
    msg = replace_all(msg, "@player_god@",
                      you_worship(GOD_NO_GOD) ? "atheism"
                      : god_name(you.religion, coinflip()));
    msg = replace_all(msg, "@player_genus@", species_name(you.species, SPNAME_GENUS));
    msg = replace_all(msg, "@a_player_genus@",
                          article_a(species_name(you.species, SPNAME_GENUS)));
    msg = replace_all(msg, "@player_genus_plural@",
                      pluralise(species_name(you.species, SPNAME_GENUS)));

    msg = maybe_pick_random_substring(msg);
    msg = maybe_capitalise_substring(msg);

    mprf(channel, "%s", msg.c_str());

    if (channel != MSGCH_TALK_VISUAL)
        noisy(loudness, you.pos());
}

// TODO: Let artefacts besides weapons generate noise.
void noisy_equipment()
{
    if (silenced(you.pos()) || !one_chance_in(20))
        return;

    string msg;

    const item_def* weapon = you.weapon();
    if (!weapon)
        return;

    if (is_unrandom_artefact(*weapon))
    {
        string name = weapon->name(DESC_PLAIN, false, true, false, false,
                                   ISFLAG_IDENT_MASK);
        msg = getSpeakString(name);
        if (msg == "NONE")
            return;
    }

    if (msg.empty())
        msg = getSpeakString("noisy weapon");

    item_noise(*weapon, msg, 20);
}

// Berserking monsters cannot be ordered around.
static bool _follows_orders(monster* mon)
{
    return mon->friendly()
           && mon->type != MONS_BALLISTOMYCETE_SPORE
           && !mon->berserk_or_insane()
           && !mons_is_conjured(mon->type)
           && !mon->has_ench(ENCH_HAUNTING);
}

// Sets foe target of friendly monsters.
// If allow_patrol is true, patrolling monsters get MHITNOT instead.
static void _set_friendly_foes(bool allow_patrol = false)
{
    for (monster_near_iterator mi(you.pos()); mi; ++mi)
    {
        if (!_follows_orders(*mi))
            continue;
        if (you.pet_target != MHITNOT && mi->behaviour == BEH_WITHDRAW)
        {
            mi->behaviour = BEH_SEEK;
            mi->patrol_point = coord_def(0, 0);
        }
        mi->foe = (allow_patrol && mi->is_patrolling() ? MHITNOT
                                                         : you.pet_target);
    }
}

static void _set_allies_patrol_point(bool clear = false)
{
    for (monster_near_iterator mi(you.pos()); mi; ++mi)
    {
        if (!_follows_orders(*mi))
            continue;
        mi->patrol_point = (clear ? coord_def(0, 0) : mi->pos());
        if (!clear)
            mi->behaviour = BEH_WANDER;
        else
            mi->behaviour = BEH_SEEK;
    }
}

static void _set_allies_withdraw(const coord_def &target)
{
    coord_def delta = target - you.pos();
    float mult = float(LOS_RADIUS * 2) / (float)max(abs(delta.x), abs(delta.y));
    coord_def rally_point = clamp_in_bounds(coord_def(delta.x * mult, delta.y * mult) + you.pos());

    for (monster_near_iterator mi(you.pos()); mi; ++mi)
    {
        if (!_follows_orders(*mi))
            continue;
        if (mons_class_flag(mi->type, M_STATIONARY))
            continue;
        mi->behaviour = BEH_WITHDRAW;
        mi->target = clamp_in_bounds(target);
        mi->patrol_point = rally_point;
        mi->foe = MHITNOT;

        mi->props.erase("last_pos");
        mi->props.erase("idle_point");
        mi->props.erase("idle_deadline");
        mi->props.erase("blocked_deadline");
    }
}

/// Does the player have a 'previous target' to issue targeting orders at?
static bool _can_target_prev()
{
    return !(you.prev_targ == MHITNOT || you.prev_targ == MHITYOU);
}

/// Prompt the player to issue orders. Returns the key pressed.
static int _issue_orders_prompt()
{
    mprf(MSGCH_PROMPT, "What are your orders?");
    if (!you.cannot_speak())
    {
        string cap_shout = you.shout_verb(false);
        cap_shout[0] = toupper(cap_shout[0]);
        mprf(" t - %s!", cap_shout.c_str());
    }

    if (!you.berserk())
    {
        string previous;
        if (_can_target_prev())
        {
            const monster* target = &menv[you.prev_targ];
            if (target->alive() && you.can_see(*target))
                previous = "   p - Attack previous target.";
        }

        mprf("Orders for allies: a - Attack new target.%s", previous.c_str());
        mpr("                   r - Retreat!             s - Stop attacking.");
        mpr("                   g - Guard the area.      f - Follow me.");
    }
    mpr(" Anything else - Cancel.");

    if (you.berserk())
        flush_prev_message(); // buffer doesn't get flushed otherwise

    const int keyn = get_ch();
    clear_messages();
    return keyn;
}

/**
 * Issue the order specified by the given key.
 *
 * @param keyn              The key the player just pressed.
 * @param mons_targd[out]   Who the player's allies should be targetting as a
 *                          result of this command.
 * @return                  Whether a command actually executed (and the value
 *                          of mons_targd should be used).
 */
static bool _issue_order(int keyn, int &mons_targd)
{
    if (you.berserk())
    {
        canned_msg(MSG_TOO_BERSERK);
        return false;
    }

    switch (keyn)
    {
        case 'f':
        case 's':
            mons_targd = MHITYOU;
            if (keyn == 'f')
            {
                // Don't reset patrol points for 'Stop fighting!'
                _set_allies_patrol_point(true);
                mpr("Follow me!");
            }
            else
                mpr("Stop fighting!");
            break;

        case 'w':
        case 'g':
            mpr("Guard this area!");
            mons_targd = MHITNOT;
            _set_allies_patrol_point();
            break;

        case 'p':

            if (_can_target_prev())
            {
                mons_targd = you.prev_targ;
                break;
            }

            // fall through
        case 'a':
            if (env.sanctuary_time > 0)
            {
                if (!yesno("An ally attacking under your orders might violate "
                           "sanctuary; order anyway?", false, 'n'))
                {
                    canned_msg(MSG_OK);
                    return false;
                }
            }

        {
            direction_chooser_args args;
            args.restricts = DIR_TARGET;
            args.mode = TARG_HOSTILE;
            args.needs_path = false;
            args.top_prompt = "Gang up on whom?";
            dist targ;
            direction(targ, args);

            if (targ.isCancel)
            {
                canned_msg(MSG_OK);
                return false;
            }

            bool cancel = !targ.isValid;
            if (!cancel)
            {
                const monster* m = monster_at(targ.target);
                cancel = (m == nullptr || !you.can_see(*m));
                if (!cancel)
                    mons_targd = m->mindex();
            }

            if (cancel)
            {
                canned_msg(MSG_NOTHING_THERE);
                return false;
            }
        }
            break;

        case 'r':
        {
            direction_chooser_args args;
            args.restricts = DIR_TARGET;
            args.mode = TARG_ANY;
            args.needs_path = false;
            args.top_prompt = "Retreat in which direction?";
            dist targ;
            direction(targ, args);

            if (targ.isCancel)
            {
                canned_msg(MSG_OK);
                return false;
            }

            if (targ.isValid)
            {
                mpr("Fall back!");
                mons_targd = MHITNOT;
            }

            _set_allies_withdraw(targ.target);
        }
            break;

        default:
            canned_msg(MSG_OK);
            return false;
    }

    return true;
}

/**
 * Prompt the player to either change their allies' orders or to shout.
 *
 * XXX: it'd be nice if shouting was a separate command.
 * XXX: this should maybe be in another file.
 */
void issue_orders()
{
    ASSERT(!crawl_state.game_is_arena());

    if (you.cannot_speak() && you.berserk())
    {
        mpr("You're too berserk to give orders, and you can't shout!");
        return;
    }

    const int keyn = _issue_orders_prompt();
    if (keyn == '!' || keyn == 't') // '!' for [very] old keyset
    {
        yell();
        you.turn_is_over = true;
        return;
    }

    int mons_targd = MHITNOT; // XXX: just use you.pet_target directly?
    if (!_issue_order(keyn, mons_targd))
        return;

    you.turn_is_over = true;
    you.pet_target = mons_targd;
    // Allow patrolling for "Stop fighting!" and "Wait here!"
    _set_friendly_foes(keyn == 's' || keyn == 'w');

    if (mons_targd != MHITNOT && mons_targd != MHITYOU)
        mpr("Attack!");
}

/**
 * Make the player yell, either at a monster or at nothing in particular.
 *
 * @mon     The monster to yell at; may be null.
 */
void yell(const actor* mon)
{
    ASSERT(!crawl_state.game_is_arena());

    const string shout_verb = you.shout_verb(mon != nullptr);
    const int noise_level = you.shout_volume();

    if (you.cannot_speak())
    {
        if (mon)
        {
            if (you.paralysed() || you.duration[DUR_WATER_HOLD])
            {
                mprf("You feel a strong urge to %s, but "
                     "you are unable to make a sound!",
                     shout_verb.c_str());
            }
            else
            {
                mprf("You feel a %s rip itself from your throat, "
                     "but you make no sound!",
                     shout_verb.c_str());
            }
        }
        else
            mpr("You are unable to make a sound!");

        return;
    }

    if (mon)
    {
        mprf("You %s%s at %s!",
             shout_verb.c_str(),
             you.duration[DUR_RECITE] ? " your recitation" : "",
             mon->name(DESC_THE).c_str());
    }
    else
    {
        mprf(MSGCH_SOUND, "You %s%s!",
             shout_verb.c_str(),
             you.berserk() ? " wildly" : " for attention");
    }

    noisy(noise_level, you.pos());
}

void apply_noises()
{
    // [ds] This copying isn't awesome, but we cannot otherwise handle
    // the case where one set of noises wakes up monsters who then let
    // out yips of their own, modifying _noise_grid while it is in the
    // middle of propagate_noise().
    if (_noise_grid.dirty())
    {
        noise_grid copy = _noise_grid;
        // Reset the main grid.
        _noise_grid.reset();
        copy.propagate_noise();
    }
}

// noisy() has a messaging service for giving messages to the player
// as appropriate.
bool noisy(int original_loudness, const coord_def& where,
           const char *msg, mid_t who, bool fake_noise)
{
    ASSERT_IN_BOUNDS(where);

    if (original_loudness <= 0)
        return false;

    // high ambient noise makes sounds harder to hear
    const int ambient = ambient_noise();
    const int loudness =
        ambient < 0 ? original_loudness + random2avg(abs(ambient), 3)
                    : original_loudness - random2avg(abs(ambient), 3);

    dprf(DIAG_NOISE, "Noise %d (orig: %d; ambient: %d) at pos(%d,%d)",
         loudness, original_loudness, ambient, where.x, where.y);

    if (loudness <= 0)
        return false;

    // If the origin is silenced there is no noise, unless we're
    // faking it.
    if (silenced(where) && !fake_noise)
        return false;

    // [ds] Reduce noise propagation for Sprint.
    const int scaled_loudness =
        crawl_state.game_is_sprint()? max(1, div_rand_round(loudness, 2))
                                    : loudness;

    // Add +1 to scaled_loudness so that all squares adjacent to a
    // sound of loudness 1 will hear the sound.
    const string noise_msg(msg? msg : "");
    _noise_grid.register_noise(
        noise_t(where, noise_msg, (scaled_loudness + 1) * 1000, who));

    // Some users of noisy() want an immediate answer to whether the
    // player heard the noise. The deferred noise system also means
    // that soft noises can be drowned out by loud noises. For both
    // these reasons, use the simple old noise system to check if the
    // player heard the noise:
    const int dist = loudness;
    const int player_distance = grid_distance(you.pos(), where);

    // Message the player.
    if (player_distance <= dist && player_can_hear(where))
    {
        if (msg && !fake_noise)
            mprf(MSGCH_SOUND, "%s", msg);
        return true;
    }
    return false;
}

bool noisy(int loudness, const coord_def& where, mid_t who)
{
    return noisy(loudness, where, nullptr, who);
}

// This fakes noise even through silence.
bool fake_noisy(int loudness, const coord_def& where)
{
    return noisy(loudness, where, nullptr, MID_NOBODY, true);
}

void check_monsters_sense(sense_type sense, int range, const coord_def& where)
{
    for (monster_iterator mi; mi; ++mi)
    {
        if (grid_distance(mi->pos(), where) > range)
            continue;

        switch (sense)
        {
        case SENSE_SMELL_BLOOD:
            if (!mons_class_flag(mi->type, M_BLOOD_SCENT))
                break;

            // Let sleeping hounds lie.
            if (mi->asleep()
                && mons_species(mi->type) != MONS_VAMPIRE)
            {
                // 33% chance of sleeping on
                // 33% of being disturbed (start BEH_WANDER)
                // 33% of being alerted   (start BEH_SEEK)
                if (!one_chance_in(3))
                {
                    if (coinflip())
                    {
                        dprf(DIAG_NOISE, "disturbing %s (%d, %d)",
                             mi->name(DESC_A, true).c_str(),
                             mi->pos().x, mi->pos().y);
                        behaviour_event(*mi, ME_DISTURB, 0, where);
                    }
                    break;
                }
            }
            dprf(DIAG_NOISE, "alerting %s (%d, %d)",
                            mi->name(DESC_A, true).c_str(),
                            mi->pos().x, mi->pos().y);
            behaviour_event(*mi, ME_ALERT, 0, where);
            break;

        case SENSE_WEB_VIBRATION:
            if (!mons_class_flag(mi->type, M_WEB_SENSE))
                break;

            if (!one_chance_in(4))
            {
                if (coinflip())
                {
                    dprf(DIAG_NOISE, "disturbing %s (%d, %d)",
                         mi->name(DESC_A, true).c_str(),
                         mi->pos().x, mi->pos().y);
                    behaviour_event(*mi, ME_DISTURB, 0, where);
                }
                else
                {
                    dprf(DIAG_NOISE, "alerting %s (%d, %d)",
                         mi->name(DESC_A, true).c_str(),
                         mi->pos().x, mi->pos().y);
                    behaviour_event(*mi, ME_ALERT, 0, where);
                }
            }
            break;
        }
    }
}

void blood_smell(int strength, const coord_def& where)
{
    const int range = strength;
    dprf("blood stain at (%d, %d), range of smell = %d",
         where.x, where.y, range);

    check_monsters_sense(SENSE_SMELL_BLOOD, range, where);
}

//////////////////////////////////////////////////////////////////////////////
// noise machinery

// Currently noise attenuation depends solely on the feature in question.
// Permarock walls are assumed to completely kill noise.
static int _noise_attenuation_millis(const coord_def &pos)
{
    const dungeon_feature_type feat = grd(pos);

    if (feat_is_permarock(feat))
        return NOISE_ATTENUATION_COMPLETE;

    return BASE_NOISE_ATTENUATION_MILLIS *
            (feat_is_wall(feat)        ? 12 :
             feat_is_closed_door(feat) ?  8 :
             feat_is_tree(feat)        ?  3 :
             feat_is_statuelike(feat)  ?  2 :
                                          1);
}

noise_cell::noise_cell()
    : neighbour_delta(0, 0), noise_id(-1), noise_intensity_millis(0),
      noise_travel_distance(0)
{
}

bool noise_cell::can_apply_noise(int _noise_intensity_millis) const
{
    return noise_intensity_millis < _noise_intensity_millis;
}

bool noise_cell::apply_noise(int _noise_intensity_millis,
                             int _noise_id,
                             int _noise_travel_distance,
                             const coord_def &_neighbour_delta)
{
    if (can_apply_noise(_noise_intensity_millis))
    {
        noise_id = _noise_id;
        noise_intensity_millis = _noise_intensity_millis;
        noise_travel_distance = _noise_travel_distance;
        neighbour_delta = _neighbour_delta;
        return true;
    }
    return false;
}

int noise_cell::turn_angle(const coord_def &next_delta) const
{
    if (neighbour_delta.origin())
        return 0;

    // Going in reverse?
    if (next_delta.x == -neighbour_delta.x
        && next_delta.y == -neighbour_delta.y)
    {
        return 4;
    }

    const int xdiff = abs(neighbour_delta.x - next_delta.x);
    const int ydiff = abs(neighbour_delta.y - next_delta.y);
    return xdiff + ydiff;
}

noise_grid::noise_grid()
    : cells(), noises(), affected_actor_count(0)
{
}

void noise_grid::reset()
{
    cells.init(noise_cell());
    noises.clear();
    affected_actor_count = 0;
}

void noise_grid::register_noise(const noise_t &noise)
{
    noise_cell &target_cell(cells(noise.noise_source));
    if (target_cell.can_apply_noise(noise.noise_intensity_millis))
    {
        const int noise_index = noises.size();
        noises.push_back(noise);
        noises[noise_index].noise_id = noise_index;
        cells(noise.noise_source).apply_noise(noise.noise_intensity_millis,
                                              noise_index,
                                              0,
                                              coord_def(0, 0));
    }
}

void noise_grid::propagate_noise()
{
    if (noises.empty())
        return;

#ifdef DEBUG_NOISE_PROPAGATION
    dprf(DIAG_NOISE, "noise_grid: %u noises to apply",
         (unsigned int)noises.size());
#endif
    vector<coord_def> noise_perimeter[2];
    int circ_index = 0;

    for (const noise_t &noise : noises)
        noise_perimeter[circ_index].push_back(noise.noise_source);

    int travel_distance = 0;
    while (!noise_perimeter[circ_index].empty())
    {
        const vector<coord_def> &perimeter(noise_perimeter[circ_index]);
        vector<coord_def> &next_perimeter(noise_perimeter[!circ_index]);
        ++travel_distance;
        for (const coord_def p : perimeter)
        {
            const noise_cell &cell(cells(p));

            if (!cell.silent())
            {
                apply_noise_effects(p,
                                    cell.noise_intensity_millis,
                                    noises[cell.noise_id],
                                    travel_distance - 1);

                const int attenuation = _noise_attenuation_millis(p);
                // If the base noise attenuation kills the noise, go no farther:
                if (noise_is_audible(cell.noise_intensity_millis - attenuation))
                {
                    // [ds] Not using adjacent iterator which has
                    // unnecessary overhead for the tight loop here.
                    for (int xi = -1; xi <= 1; ++xi)
                    {
                        for (int yi = -1; yi <= 1; ++yi)
                        {
                            if (xi || yi)
                            {
                                const coord_def next_position(p.x + xi,
                                                              p.y + yi);
                                if (in_bounds(next_position)
                                    && !silenced(next_position))
                                {
                                    if (propagate_noise_to_neighbour(
                                            attenuation,
                                            travel_distance,
                                            cell, p,
                                            next_position))
                                    {
                                        next_perimeter.push_back(next_position);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        noise_perimeter[circ_index].clear();
        circ_index = !circ_index;
    }

#ifdef DEBUG_NOISE_PROPAGATION
    if (affected_actor_count)
    {
        mprf(MSGCH_WARN, "Writing noise grid with %d noise sources",
             noises.size());
        dump_noise_grid("noise-grid.html");
    }
#endif
}

bool noise_grid::propagate_noise_to_neighbour(int base_attenuation,
                                              int travel_distance,
                                              const noise_cell &cell,
                                              const coord_def &current_pos,
                                              const coord_def &next_pos)
{
    noise_cell &neighbour(cells(next_pos));
    if (!neighbour.can_apply_noise(cell.noise_intensity_millis
                                   - base_attenuation))
    {
        return false;
    }

    const int noise_turn_angle = cell.turn_angle(next_pos - current_pos);
    const int turn_attenuation =
        noise_turn_angle? (base_attenuation * (100 + noise_turn_angle * 25)
                           / 100)
        : base_attenuation;
    const int attenuated_noise_intensity =
        cell.noise_intensity_millis - turn_attenuation;
    if (noise_is_audible(attenuated_noise_intensity))
    {
        const int neighbour_old_distance = neighbour.noise_travel_distance;
        if (neighbour.apply_noise(attenuated_noise_intensity,
                                  cell.noise_id,
                                  travel_distance,
                                  next_pos - current_pos))
            // Return true only if we hadn't already registered this
            // cell as a neighbour (presumably with a lower volume).
            return neighbour_old_distance != travel_distance;
    }
    return false;
}

void noise_grid::apply_noise_effects(const coord_def &pos,
                                     int noise_intensity_millis,
                                     const noise_t &noise,
                                     int noise_travel_distance)
{
    if (you.pos() == pos)
    {
        _actor_apply_noise(&you, noise.noise_source,
                           noise_intensity_millis, noise,
                           noise_travel_distance);
        ++affected_actor_count;
    }

    if (monster *mons = monster_at(pos))
    {
        if (mons->alive()
            && !mons_just_slept(*mons)
            && mons->mid != noise.noise_producer_mid)
        {
            const coord_def perceived_position =
                noise_perceived_position(mons, pos, noise);
            _actor_apply_noise(mons, perceived_position,
                               noise_intensity_millis, noise,
                               noise_travel_distance);
            ++affected_actor_count;
        }
    }
}

// Given an actor at affected_pos and a given noise, calculates where
// the actor might think the noise originated.
//
// [ds] Let's keep this brutally simple, since the player will
// probably not notice even if we get very clever:
//
//  - If the cells can see each other, return the actual source position.
//
//  - If the cells cannot see each other, calculate a noise source as follows:
//
//    Calculate a noise centroid between the noise source and the observer,
//    weighted to the noise source if the noise has traveled in a straight line,
//    weighted toward the observer the more the noise has deviated from a
//    straight line.
//
//    Fuzz the centroid by the extra distance the noise has traveled over
//    the straight line distance. This is where the observer will think the
//    noise originated.
//
//    Thus, if the noise has traveled in a straight line, the observer
//    will know the exact origin, 100% of the time, even if the
//    observer is all the way across the level.
coord_def noise_grid::noise_perceived_position(actor *act,
                                               const coord_def &affected_pos,
                                               const noise_t &noise) const
{
    const int noise_travel_distance = cells(affected_pos).noise_travel_distance;
    if (!noise_travel_distance)
        return noise.noise_source;

    const int cell_grid_distance =
        grid_distance(affected_pos, noise.noise_source);

    if (cell_grid_distance <= LOS_RADIUS)
    {
        if (act->see_cell(noise.noise_source))
            return noise.noise_source;
    }

    const int extra_distance_covered =
        noise_travel_distance - cell_grid_distance;

    const int source_weight = 200;
    const int target_weight =
        extra_distance_covered?
        75 * extra_distance_covered / cell_grid_distance
        : 0;

    const coord_def noise_centroid =
        target_weight?
        (noise.noise_source * source_weight + affected_pos * target_weight)
        / (source_weight + target_weight)
        : noise.noise_source;

    const int fuzz = extra_distance_covered;
    const coord_def perceived_point =
        fuzz?
        noise_centroid + coord_def(random_range(-fuzz, fuzz, 2),
                                   random_range(-fuzz, fuzz, 2))
        : noise_centroid;

    const coord_def final_perceived_point =
        !in_bounds(perceived_point)?
        clamp_in_bounds(perceived_point)
        : perceived_point;

#ifdef DEBUG_NOISE_PROPAGATION
    dprf(DIAG_NOISE, "[NOISE] Noise perceived by %s at (%d,%d) "
         "centroid (%d,%d) source (%d,%d) "
         "heard at (%d,%d), distance: %d (traveled %d)",
         act->name(DESC_PLAIN, true).c_str(),
         final_perceived_point.x, final_perceived_point.y,
         noise_centroid.x, noise_centroid.y,
         noise.noise_source.x, noise.noise_source.y,
         affected_pos.x, affected_pos.y,
         cell_grid_distance, noise_travel_distance);
#endif
    return final_perceived_point;
}

#ifdef DEBUG_NOISE_PROPAGATION

#include <cmath>

// Return HTML RGB triple given a hue and assuming chroma of 0.86 (220)
static string _hue_rgb(int hue)
{
    const int chroma = 220;
    const double hue2 = hue / 60.0;
    const int x = chroma * (1.0 - fabs(hue2 - floor(hue2 / 2) * 2 - 1));
    int red = 0, green = 0, blue = 0;
    if (hue2 < 1)
        red = chroma, green = x;
    else if (hue2 < 2)
        red = x, green = chroma;
    else if (hue2 < 3)
        green = chroma, blue = x;
    else if (hue2 < 4)
        green = x, blue = chroma;
    else
        red = x, blue = chroma;
    // Other hues are not generated, so skip them.
    return make_stringf("%02x%02x%02x", red, green, blue);
}

static string _noise_intensity_styles()
{
    // Hi-intensity sound will be red (HSV 0), low intensity blue (HSV 240).
    const int hi_hue = 0;
    const int lo_hue = 240;
    const int huespan = lo_hue - hi_hue;

    const int max_intensity = 25;
    string styles;
    for (int intensity = 1; intensity <= max_intensity; ++intensity)
    {
        const int hue = lo_hue - intensity * huespan / max_intensity;
        styles += make_stringf(".i%d { background: #%s }\n",
                               intensity, _hue_rgb(hue).c_str());

    }
    return styles;
}

static void _write_noise_grid_css(FILE *outf)
{
    fprintf(outf,
            "<style type='text/css'>\n"
            "body { font-family: monospace; padding: 0; margin: 0; "
            "line-height: 100%% }\n"
            "%s\n"
            "</style>",
            _noise_intensity_styles().c_str());
}

void noise_grid::write_cell(FILE *outf, coord_def p, int ch) const
{
    const int intensity = min(25, cells(p).noise_intensity_millis / 1000);
    if (intensity)
        fprintf(outf, "<span class='i%d'>&#%d;</span>", intensity, ch);
    else
        fprintf(outf, "<span>&#%d;</span>", ch);
}

void noise_grid::write_noise_grid(FILE *outf) const
{
    // Duplicate the screenshot() trick.
    FixedVector<unsigned, NUM_DCHAR_TYPES> char_table_bk;
    char_table_bk = Options.char_table;

    init_char_table(CSET_ASCII);
    init_show_table();

    fprintf(outf, "<div>\n");
    // Write the whole map out without checking for mappedness. Handy
    // for debugging level-generation issues.
    for (int y = 0; y < GYM; ++y)
    {
        for (int x = 0; x < GXM; ++x)
        {
            coord_def p(x, y);
            if (you.pos() == coord_def(x, y))
                write_cell(outf, p, '@');
            else
                write_cell(outf, p, get_feature_def(grd[x][y]).symbol());
        }
        fprintf(outf, "<br>\n");
    }
    fprintf(outf, "</div>\n");
}

void noise_grid::dump_noise_grid(const string &filename) const
{
    FILE *outf = fopen(filename.c_str(), "w");
    fprintf(outf, "<!DOCTYPE html><html><head>");
    _write_noise_grid_css(outf);
    fprintf(outf, "</head>\n<body>\n");
    write_noise_grid(outf);
    fprintf(outf, "</body></html>\n");
    fclose(outf);
}
#endif

static void _actor_apply_noise(actor *act,
                               const coord_def &apparent_source,
                               int noise_intensity_millis,
                               const noise_t &noise,
                               int noise_travel_distance)
{
#ifdef DEBUG_NOISE_PROPAGATION
    dprf(DIAG_NOISE, "[NOISE] Actor %s (%d,%d) perceives noise (%d) "
         "from (%d,%d), real source (%d,%d), distance: %d, noise traveled: %d",
         act->name(DESC_PLAIN, true).c_str(),
         act->pos().x, act->pos().y,
         noise_intensity_millis,
         apparent_source.x, apparent_source.y,
         noise.noise_source.x, noise.noise_source.y,
         grid_distance(act->pos(), noise.noise_source),
         noise_travel_distance);
#endif

    const bool player = act->is_player();
    if (player)
    {
        const int loudness = div_rand_round(noise_intensity_millis, 1000);
        act->check_awaken(loudness);
    }
    else
    {
        monster *mons = act->as_monster();
        // If the noise came from the character, any nearby monster
        // will be jumping on top of them.
        if (grid_distance(apparent_source, you.pos()) <= 3)
            behaviour_event(mons, ME_ALERT, &you, apparent_source);
        else
            behaviour_event(mons, ME_DISTURB, 0, apparent_source);
    }
}
