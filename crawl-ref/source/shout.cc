/**
 * @file
 * @brief Stealth, noise, shouting.
**/

#include "AppHdr.h"

#include "shout.h"

#include "act-iter.h"
#include "art-enum.h"
#include "artefact.h"
#include "branch.h"
#include "cluautil.h"
#include "coord.h"
#include "database.h"
#include "dlua.h"
#include "env.h"
#include "exercise.h"
#include "ghost.h"
#include "jobs.h"
#include "libutil.h"
#include "los.h"
#include "message.h"
#include "misc.h"
#include "mon-behv.h"
#include "mon-chimera.h"
#include "mon-place.h"
#include "mon-pathfind.h"
#include "mon-stuff.h"
#include "mon-util.h"
#include "monster.h"
#include "noise.h"
#include "player.h"
#include "random.h"
#include "religion.h"
#include "skills.h"
#include "state.h"
#include "terrain.h"
#include "areas.h"
#include "hints.h"
#include "view.h"

#include <sstream>

extern int stealth;             // defined in main.cc

static noise_grid _noise_grid;
static void _actor_apply_noise(actor *act,
                               const coord_def &apparent_source,
                               int noise_intensity_millis,
                               const noise_t &noise,
                               int noise_travel_distance);

void handle_monster_shouts(monster* mons, bool force)
{
    if (!force && one_chance_in(5))
        return;

    // Friendly or neutral monsters don't shout.
    if (!force && (mons->friendly() || mons->neutral()))
        return;

    // Get it once, since monster might be S_RANDOM, in which case
    // mons_shouts() will return a different value every time.
    // Demon lords will insult you as a greeting, but later we'll
    // choose a random verb and loudness for them.
    shout_type  s_type = mons_shouts(mons->type, false);

    // Chimera can take a random shout type from any of their
    // three components
    if (mons->type == MONS_CHIMERA)
    {
        monster_type acting = mons->ghost->acting_part != MONS_0
            ? mons->ghost->acting_part : random_chimera_part(mons);
        s_type = mons_shouts(acting, false);
    }
    // Silent monsters can give noiseless "visual shouts" if the
    // player can see them, in which case silence isn't checked for.
    // Muted monsters can't shout at all.
    if (s_type == S_SILENT && !mons->visible_to(&you)
        || s_type != S_SILENT && !player_can_hear(mons->pos())
        || mons->has_ench(ENCH_MUTE))
    {
        return;
    }

    mon_acting mact(mons);

    string default_msg_key = "";

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
    case S_CAW:
        default_msg_key = "__CAW";
        break;
    case S_CHERUB:
        default_msg_key = "__CHERUB";
        break;
    default:
        default_msg_key = "__BUGGY"; // S_LOUD, S_VERY_SOFT, etc. (loudness)
    }

    // Now that we have the message key, get a random verb and noise level
    // for pandemonium lords.
    if (s_type == S_DEMON_TAUNT)
        s_type = mons_shouts(mons->type, true);

    string msg, suffix;
    string key = mons_type_name(mons->type, DESC_PLAIN);

    // Pandemonium demons have random names, so use "pandemonium lord"
    if (mons->type == MONS_PANDEMONIUM_LORD)
        key = "pandemonium lord";
    // Search for player ghost shout by the ghost's job.
    else if (mons->type == MONS_PLAYER_GHOST)
    {
        const ghost_demon &ghost = *(mons->ghost);
        string ghost_job         = get_job_name(ghost.job);

        key = ghost_job + " player ghost";

        default_msg_key = "player ghost";
    }

    // Tries to find an entry for "name seen" or "name unseen",
    // and if no such entry exists then looks simply for "name".
    // We don't use "you.can_see(mons)" here since that would return
    // false for submerged monsters, but submerged monsters will be forced
    // to surface before they shout, thus removing that source of
    // non-visibility.
    if (you.can_see(mons))
        suffix = " seen";
    else
        suffix = " unseen";

    if (msg.empty())
        msg = getShoutString(key, suffix);

    if (msg == "__DEFAULT" || msg == "__NEXT")
        msg = getShoutString(default_msg_key, suffix);
    else if (msg.empty())
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
        msg = getShoutString(glyph_key, suffix);

        if (msg.empty() || msg == "__DEFAULT")
            msg = getShoutString(default_msg_key, suffix);
    }

    if (default_msg_key == "__BUGGY")
    {
        msg::streams(MSGCH_SOUND) << "You hear something buggy!"
                                  << endl;
    }
    else if (s_type == S_SILENT && (msg.empty() || msg == "__NONE"))
        ; // No "visual shout" defined for silent monster, do nothing.
    else if (msg.empty()) // Still nothing found?
    {
        msg::streams(MSGCH_DIAGNOSTICS)
            << "No shout entry for default shout type '"
            << default_msg_key << "'" << endl;

        msg::streams(MSGCH_SOUND) << "You hear something buggy!"
                                  << endl;
    }
    else if (msg == "__NONE")
    {
        msg::streams(MSGCH_DIAGNOSTICS)
            << "__NONE returned as shout for non-silent monster '"
            << default_msg_key << "'" << endl;
        msg::streams(MSGCH_SOUND) << "You hear something buggy!"
                                  << endl;
    }
    else
    {
        msg_channel_type channel = MSGCH_TALK;
        if (s_type == S_SILENT)
            channel = MSGCH_TALK_VISUAL;

        strip_channel_prefix(msg, channel);

        // Monster must come up from being submerged if it wants to shout.
        if (mons->submerged())
        {
            if (!mons->del_ench(ENCH_SUBMERGED))
            {
                // Couldn't unsubmerge.
                return;
            }

            if (you.can_see(mons))
            {
                if (!monster_habitable_grid(mons, DNGN_FLOOR))
                    mons->seen_context = SC_FISH_SURFACES_SHOUT;
                else
                    mons->seen_context = SC_SURFACES;

                // Give interrupt message before shout message.
                handle_seen_interrupt(mons);
            }
        }

        if (channel != MSGCH_TALK_VISUAL || you.can_see(mons))
        {
            // Otherwise it can move away with no feedback.
            if (you.can_see(mons))
            {
                if (!(mons->flags & MF_WAS_IN_VIEW))
                    handle_seen_interrupt(mons);
                seen_monster(mons);
            }

            msg = do_mon_str_replacements(msg, mons, s_type);
            msg::streams(channel) << msg << endl;
        }
    }

    const int  noise_level = get_shout_noise_level(s_type);
    const bool heard       = noisy(noise_level, mons->pos(), mons->mindex());

    if (crawl_state.game_is_hints() && (heard || you.can_see(mons)))
        learned_something_new(HINT_MONSTER_SHOUT, mons->pos());
}

bool check_awaken(monster* mons)
{
    // Usually redundant because we iterate over player LOS,
    // but e.g. for you.xray_vision.
    if (!mons->see_cell(you.pos()))
        return false;

    // Monsters put to sleep by ensorcelled hibernation will sleep
    // at least one turn.
    if (mons->has_ench(ENCH_SLEEPY))
        return false;

    // Berserkers aren't really concerned about stealth.
    if (you.berserk())
        return true;

    // Vigilant monsters are always alerted
    if (mons_class_flag(mons->type, M_VIGILANT))
        return true;

    // I assume that creatures who can sense invisible are very perceptive.
    int mons_perc = 10 + (mons_intel(mons) * 4) + mons->hit_dice
                       + mons_sense_invis(mons) * 5;

    bool unnatural_stealthy = false; // "stealthy" only because of invisibility?

    // Critters that are wandering but still have MHITYOU as their foe are
    // still actively on guard for the player, even if they can't see you.
    // Give them a large bonus -- handle_behaviour() will nuke 'foe' after
    // a while, removing this bonus.
    if (mons_is_wandering(mons) && mons->foe == MHITYOU)
        mons_perc += 15;

    if (!you.visible_to(mons))
    {
        mons_perc -= 75;
        unnatural_stealthy = true;
    }

    if (mons->asleep())
    {
        if (mons->holiness() == MH_NATURAL)
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

    if (mons_perc < 0)
        mons_perc = 0;

    if (x_chance_in_y(mons_perc + 1, stealth))
        return true; // Oops, the monster wakes up!

    // You didn't wake the monster!
    if (you.can_see(mons) // to avoid leaking information
        && !mons->wont_attack()
        && !mons->neutral() // include pacified monsters
        && !mons_class_flag(mons->type, M_NO_EXP_GAIN))
    {
        practise(unnatural_stealthy ? EX_SNEAK_INVIS : EX_SNEAK);
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

    // Replace weapon references.  Can't use DESC_THE because that includes
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
    msg = replace_all(msg, "@a_player_genus@",
                          article_a(species_name(you.species, true)));

    mpr(msg.c_str(), channel);

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

    msg = maybe_pick_random_substring(msg);
    msg = maybe_capitalise_substring(msg);

    item_noise(*weapon, msg);
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
           const char *msg, int who,
           bool mermaid, bool message_if_unseen, bool fake_noise)
{
    ASSERT_IN_BOUNDS(where);

    if (original_loudness <= 0)
        return false;

    // high ambient noise makes sounds harder to hear
    const int ambient = current_level_ambient_noise();
    const int loudness =
        ambient < 0? original_loudness + random2avg(abs(ambient), 3)
                   : original_loudness - random2avg(abs(ambient), 3);

    dprf("Noise %d (orig: %d; ambient: %d) at pos(%d,%d)",
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
        noise_t(where,
                noise_msg,
                (scaled_loudness + 1) * 1000,
                who,
                0 | (mermaid ? NF_MERMAID : 0)
                | (message_if_unseen ? NF_MESSAGE_IF_UNSEEN : 0)));

    // Some users of noisy() want an immediate answer to whether the
    // player heard the noise. The deferred noise system also means
    // that soft noises can be drowned out by loud noises. For both
    // these reasons, use the simple old noise system to check if the
    // player heard the noise:
    const int dist = loudness * loudness + 1;
    const int player_distance = distance2(you.pos(), where);

    // Message the player.
    if (player_distance <= dist && player_can_hear(where))
    {
        if (msg && !fake_noise)
            mpr(msg, MSGCH_SOUND);
        return true;
    }
    return false;
}

bool noisy(int loudness, const coord_def& where, int who,
           bool mermaid, bool message_if_unseen)
{
    return noisy(loudness, where, NULL, who, mermaid, message_if_unseen);
}

// This fakes noise even through silence.
bool fake_noisy(int loudness, const coord_def& where)
{
    return noisy(loudness, where, NULL, -1, false, false, true);
}

static const char* _player_vampire_smells_blood(int dist)
{
    // non-thirsty vampires get no clear indication of how close the
    // smell is
    if (you.hunger_state >= HS_SATIATED)
        return "";

    if (dist < 16) // 4*4
        return " near-by";

    if (you.hunger_state <= HS_NEAR_STARVING && dist > los_radius2)
        return " in the distance";

    return "";
}

static const char* _player_spider_senses_web(int dist)
{
    if (dist < 4)
        return " near-by";

    if (dist > LOS_RADIUS)
        return " in the distance";

    return "";
}

void check_player_sense(sense_type sense, int range, const coord_def& where)
{
    const int player_distance = distance2(you.pos(), where);

    if (player_distance <= range)
    {
        switch (sense)
        {
        case SENSE_SMELL_BLOOD:
            dprf("Player smells blood, pos: (%d, %d), dist = %d)",
                 you.pos().x, you.pos().y, player_distance);
            you.check_awaken(range - player_distance);
            // Don't message if you can see the square.
            if (!you.see_cell(where))
            {
                mprf("You smell fresh blood%s.",
                     _player_vampire_smells_blood(player_distance));
            }
            break;

        case SENSE_WEB_VIBRATION:
            // Spider form
            if (you.can_cling_to_walls())
            {
                you.check_awaken(range - player_distance);
                // Don't message if you can see the square.
                if (!you.see_cell(where))
                {
                    mprf("You hear a 'twang'%s.",
                         _player_spider_senses_web(player_distance));
                }
            }
            break;
        }
    }
}

void check_monsters_sense(sense_type sense, int range, const coord_def& where)
{
    for (monster_iterator mi; mi; ++mi)
    {
        if (distance2(mi->pos(), where))
            continue;

        switch (sense)
        {
        case SENSE_SMELL_BLOOD:
            if (!mons_class_flag(mi->type, M_BLOOD_SCENT))
                break;

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
                        behaviour_event(*mi, ME_DISTURB, 0, where);
                    }
                    break;
                }
            }
            dprf("alerting %s (%d, %d)",
                            mi->name(DESC_PLAIN).c_str(),
                            mi->pos().x, mi->pos().y);
            behaviour_event(*mi, ME_ALERT, 0, where);

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
                        ench.duration = max(ench.duration, dur);
                        mi->update_ench(ench);
                    }
                    else
                    {
                        mi->add_ench(mon_enchant(ENCH_BATTLE_FRENZY, 1, 0, dur));
                        simple_monster_message(*mi, " is consumed with "
                                                    "blood-lust!");
                    }
                }
            }
            break;

        case SENSE_WEB_VIBRATION:
            if (!mons_class_flag(mi->type, M_WEB_SENSE)
                && !mons_class_flag(get_chimera_legs(*mi), M_WEB_SENSE))
                break;
            if (!one_chance_in(4))
            {
                if (coinflip())
                {
                    dprf("disturbing %s (%d, %d)",
                         mi->name(DESC_PLAIN).c_str(),
                         mi->pos().x, mi->pos().y);
                    behaviour_event(*mi, ME_DISTURB, 0, where);
                }
                else
                {
                    dprf("alerting %s (%d, %d)",
                         mi->name(DESC_PLAIN).c_str(),
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
            check_player_sense(SENSE_SMELL_BLOOD, vamp_range, where);
        }
    }

    check_monsters_sense(SENSE_SMELL_BLOOD, range, where);
}

//////////////////////////////////////////////////////////////////////////////
// noise machinery

// Currently noise attenuation depends solely on the feature in question.
// Permarock walls are assumed to completely kill noise.
static int _noise_attenuation_millis(const coord_def &pos)
{
    const dungeon_feature_type feat = grd(pos);
    switch (feat)
    {
    // Closed doors are excellent at cutting off sound.
    case DNGN_CLOSED_DOOR:
    case DNGN_RUNED_DOOR:
    case DNGN_SEALED_DOOR:
        return BASE_NOISE_ATTENUATION_MILLIS * 8;
    case DNGN_TREE:
    case DNGN_MANGROVE:
        return BASE_NOISE_ATTENUATION_MILLIS * 3;
    default:
        if (feat_is_statue_or_idol(feat))
            return BASE_NOISE_ATTENUATION_MILLIS * 2;
        if (feat_is_permarock(feat))
            return NOISE_ATTENUATION_COMPLETE;
        if (feat_is_wall(feat))
            return BASE_NOISE_ATTENUATION_MILLIS * 12;
        return BASE_NOISE_ATTENUATION_MILLIS;
    }
}

noise_cell::noise_cell()
    : neighbour_delta(0, 0), noise_id(-1), noise_intensity_millis(0),
      noise_travel_distance(0)
{
}

bool noise_cell::can_apply_noise(int _noise_intensity_millis) const
{
    return (noise_intensity_millis < _noise_intensity_millis);
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
    dprf("noise_grid: %u noises to apply", (unsigned int)noises.size());
#endif
    vector<coord_def> noise_perimeter[2];
    int circ_index = 0;

    for (int i = 0, size = noises.size(); i < size; ++i)
        noise_perimeter[circ_index].push_back(noises[i].noise_source);

    int travel_distance = 0;
    while (!noise_perimeter[circ_index].empty())
    {
        const vector<coord_def> &perimeter(noise_perimeter[circ_index]);
        vector<coord_def> &next_perimeter(noise_perimeter[!circ_index]);
        ++travel_distance;
        for (int i = 0, size = perimeter.size(); i < size; ++i)
        {
            const coord_def p(perimeter[i]);
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

    // Diagonals cost more.
    if ((next_pos - current_pos).abs() == 2)
        base_attenuation = base_attenuation * 141 / 100;

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
            return (neighbour_old_distance != travel_distance);
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
            && !mons->has_ench(ENCH_SLEEPY)
            && mons->mindex() != noise.noise_producer_id)
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
    dprf("[NOISE] Noise perceived by %s at (%d,%d) centroid (%d,%d) "
         "source (%d,%d) "
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

#include "options.h"
#include "viewchar.h"
#include <math.h>

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
        fprintf(outf,
                "<span class='i%d'>&#%d;</span>",
                intensity, ch);
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
                write_cell(outf, p, get_feature_def(grd[x][y]).symbol);
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
    dprf("[NOISE] Actor %s (%d,%d) perceives noise (%d) "
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
        if (!(noise.noise_flags & NF_MERMAID))
        {
            you.beholders_check_noise(loudness, player_equip_unrand_effect(UNRAND_DEMON_AXE));
            you.fearmongers_check_noise(loudness, player_equip_unrand_effect(UNRAND_DEMON_AXE));
        }
    }
    else
    {
        monster *mons = act->as_monster();
        // If the noise came from the character, any nearby monster
        // will be jumping on top of them.
        if (grid_distance(apparent_source, you.pos()) <= 3)
            behaviour_event(mons, ME_ALERT, &you, apparent_source);
        else if ((noise.noise_flags & NF_MERMAID)
                 && mons_secondary_habitat(mons) == HT_WATER
                 && !mons->friendly())
        {
            // Mermaids/sirens call (hostile) aquatic monsters.
            behaviour_event(mons, ME_ALERT, 0, apparent_source);
        }
        else
            behaviour_event(mons, ME_DISTURB, 0, apparent_source);
    }
}
