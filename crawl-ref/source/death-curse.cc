/**
 * @file
 * @brief Functions for imposing mummy death curses.
 *
 * This is made very complicated because one source of death curses is the
 * scythe (so we have to keep a source around for kill type tracking and
 * messaging) and I (ebering) like the item too much to remove it in the name
 * of code simplification.
 *
 * Curses are currently used by:
 * - Dying mummies
 * - Scythe of curses
 */
#include "AppHdr.h"

#include "death-curse.h"

#include "actor.h"
#include "areas.h"
#include "attack.h"
#include "beam.h"
#include "beam-type.h"
#include "description-level-type.h"
#include "externs.h"
#include "god-passive.h"
#include "killer-type.h"
#include "message.h"
#include "monster.h"
#include "mon-death.h"
#include "mon-enum.h"
#include "mon-util.h"
#include "mpr.h"
#include "ouch.h"
#include "player.h"
#include "player-stats.h"
#include "random.h"
#include "species-type.h"
#include "spl-goditem.h"
#include "stat-type.h"
#include "stringutil.h"

static void _do_msg(actor& target, string player_msg, string mon_seen_msg,
                    string mon_unseen_msg)
{
    if (target.is_player() && !player_msg.empty())
        mpr(player_msg);
    else if (you.can_see(target) && !mon_seen_msg.empty())
    {
        mpr(do_mon_str_replacements(mon_seen_msg,
            *target.as_monster(), S_SILENT));
    }
    else if (!mon_unseen_msg.empty())
    {
        mpr(do_mon_str_replacements(mon_unseen_msg,
            *target.as_monster(), S_SILENT));
    }
}

// Handle applying damage for death-curse effects
static void _ouch(actor& target, const actor * source, int dam,
                  const string cause)
{
    killer_type kt;

    if (source && source->is_player())
        kt = KILL_YOU_MISSILE;
    else if (source && source->is_monster())
        if (source->as_monster()->confused_by_you()
            && !source->as_monster()->friendly())
        {
            kt = KILL_YOU_CONF;
        }
        else
            kt = KILL_MON_MISSILE;
    else
        kt = KILL_NON_ACTOR;

    if (target.is_monster())
    {
        monster* mon_target = target.as_monster();
        mon_target->hurt(source, dam, BEAM_MISSILE, KILLED_BY_BEAM,
                         "", "", false);

        if (!mon_target->alive())
            monster_die(*mon_target, kt, actor_to_death_source(source));
    }
    else
    {
        bool see_source = source && you.can_see(*source);
        ouch(dam, KILLED_BY_DEATH_CURSE, source ? source->mid : MID_NOBODY,
             cause.c_str(), see_source,
             source ? source->name(DESC_A, true).c_str() : nullptr);
    }
}

struct  curse_effect
{
    string name;
    function<void (actor& target, actor* source,
             string cause, int severity)> effect;
    int trivial_weight; // Weight at severity 0
    int severe_weight;  // Weight at severity 15. Linearly interpolated
};

static void _curse_message(actor& target, actor* /*source*/,
                           string /*cause*/, int /*severity*/)
{
    // Avoid message spam
    if (!target.is_player())
        return;

    vector<string> messages = {
        "You feel homesick.",
        "The world around you seems to dim momentarily.",
        "You feel numb.",
        "Strange energies run through your body.",
        "You shiver with cold.",
        "You sense a malignant aura.",
        "You feel very uncomfortable.",
        "Something just walked over your grave. No, really!",
    };

    if (you.can_smell())
        messages.push_back("You smell decay.");

    const string skin = species::skin_name(you.species).c_str();
    if (starts_with(skin, "bandage"))
        messages.push_back("Your bandages flutter.");
    else
    {
        messages.push_back(make_stringf("Your %s prickle%s.",
                        skin.c_str(), ends_with(skin, "s") ? "" : "s"));
    }

    if (!silenced(you.pos()))
        messages.push_back("You hear strange and distant voices.");

    if (you.has_bones())
        messages.push_back("Your bones ache.");

    mpr(*random_iterator(messages));
}

/** Table of possible curse effects
 * Severity scales from 0 up to 27. For mummies this uses their HD:
 * Guardian 7, Priest 10, Greater 15, and Khufu 18.
 * For Kiku wrath this uses player XL, and for the scythe of curses this uses
 * damage done (capped at 27).
 *
 * The severity function in each is linearly interpolating between the
 * following two sets of weights, with the weight of "message" kept at 0 for
 * severities higher than 15.
 *
 * | severity | message | smite | slow | drain | torment |
 * | -------- | ------- | ----- | ---- | ----- | ------- |
 * | 0        | 80      | 20    | 0    | 0     | 0       |
 * | 15       | 0       | 40    | 20   | 20    | 20      |
 *
 * Smite damage, slow duration, and drain effect all scale with severity.
 *
 * Kiku curse protection halves the severity when partially averting the curse,
 * making Khufu's curse a bit weaker than a mummy priest's.
 */
static const vector<curse_effect> curse_effects = {
    {
        "message",
        _curse_message,
        80, 0,
    },
    {
        "smiting",
        [](actor& target, actor* source, string cause, int severity) {
            int dmg = 5 + random2avg(2*severity,2);
            string punct = attack_strength_punctuation(dmg);
            _do_msg(target, "A forgotten god smites you" + punct,
                    "A forgotten god smites @The_monster@" + punct,
                    "A forgotten god smites something" + punct);
            _ouch(target, source, dmg, cause);
        },
        20, 40,
    },
    {
        "slow",
        [](actor& target, actor* source, string /*cause*/, int severity) {
            _do_msg(target,
                    "You feel horribly lethargic.",
                    "@The_monster@ looks incredibly listless.",
                    "");
            target.slow_down(source, severity);
        },
        0, 40,
    },
    {
        "drain",
        [](actor& target, actor* source, string /*cause*/, int severity) {
            _do_msg(target,
                    "You are engulfed in negative energy!",
                    "@The_monster@ is engulfed in negative energy!",
                    "Something is engulfed in negative energy!");
            target.drain(source, false, ( severity * 100 ) / 27);
        },
        0, 40,
    },
    {
        "torment",
        [](actor& target, actor* source,
           string /*cause*/, int /*severity*/) {
            // XXX: Passing a cached copy of a dead mummy will crash here if
            //      another monster is the victim, via behavior_event() since
            //      monster::mindex() is unsafe for copies.
            torment_cell(target.pos(), source->alive() ? source : nullptr, TORMENT_MISCAST);
        },
        0, 40,
    },
};

void death_curse(actor& target, actor* source, string cause, int severity)
{

    if (target.is_player()
        && have_passive(passive_t::miscast_protection_necromancy))
    {
        if (coinflip())
        {
            simple_god_message(" averts the curse.");
            return;
        }
        else
        {
            simple_god_message(" partially averts the curse.");
            severity = severity / 2;
        }
    }

    vector<pair<const curse_effect&, int>> weights;
    for (const curse_effect& curse : curse_effects)
    {
        const int w = ((15 - severity) * curse.trivial_weight
                       + severity * curse.severe_weight) / 15;
        weights.push_back({curse, w});
    }

    random_choose_weighted(weights)->effect(target, source, cause, severity);
}
