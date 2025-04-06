/**
 * @file
 * @brief Functions to handle speaking monsters
**/

#include "AppHdr.h"

#include "mon-speak.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "areas.h"
#include "branch.h"
#include "database.h"
#include "ghost.h"
#include "libutil.h"
#include "message.h"
#include "mon-death.h"
#include "monster.h"
#include "mon-util.h"
#include "religion.h"
#include "skills.h"
#include "state.h"
#include "stringutil.h"
#include "view.h"

// Try the exact key lookup along with the entire prefix list.
// If that fails, start ignoring hostile/religion/branch/silence, in that order,
// first skipping hostile, then hostile *and* religion, then hostile, religion
// *and* branch, then finally all five.
static string __try_exact_string(const vector<string> &prefixes,
                                 const string &key,
                                 bool ignore_hostile  = false,
                                 bool ignore_related  = false,
                                 bool ignore_religion = false,
                                 bool ignore_branch   = false,
                                 bool ignore_silenced = false)
{
    bool hostile  = false;
    bool related  = false;
    bool religion = false;
    bool branch   = false;
    bool silenced = false;

    string prefix = "";
    string msg = "";
    const int size = prefixes.size();
    for (int i = 0; i < size; i++)
    {
        if (prefixes[i] == "hostile")
        {
            if (ignore_hostile)
                continue;
            hostile = true;
        }
        else if (prefixes[i] == "related")
        {
            if (ignore_related)
                continue;
            related = true;
        }
        else if (prefixes[i] == "silenced")
        {
            if (ignore_silenced)
                continue;
            silenced = true;
        }
        else if (prefixes[i] == "Beogh" || prefixes[i] == "good god"
                 || prefixes[i] == "No God"
                 || (str_to_god(prefixes[i]) != GOD_NO_GOD
                     && prefixes[i] != "random"))
        {
            if (ignore_religion)
                continue;
            religion = true;
        }
        else if (branch_by_abbrevname(prefixes[i]) != NUM_BRANCHES)
        {
            if (ignore_branch)
                continue;
            branch = true;
        }
        prefix += prefixes[i];
        prefix += " ";
    }
    msg = getSpeakString(prefix + key);

    if (msg.empty())
    {
        if (hostile) // skip hostile
            msg = __try_exact_string(prefixes, key, true);
        else if (related)
        {
            if (branch) // skip hostile and branch
                msg = __try_exact_string(prefixes, key, true, false, false, true);
            else if (religion) // skip hostile and religion
            {
                msg = __try_exact_string(prefixes, key, true, false, true,
                                         ignore_branch);
            }
            else // skip hostile and related
                msg = __try_exact_string(prefixes, key, true, true);
        }
        else if (religion) // skip hostile, related and religion
        {
            if (branch && coinflip()) // skip hostile, related and branch
                msg = __try_exact_string(prefixes, key, true, true, false, true);
            else // skip hostile, related and religion
                msg = __try_exact_string(prefixes, key, true, true, true);
        }
        else if (branch) // skip hostile, related, religion and branch
            msg = __try_exact_string(prefixes, key, true, true, true, true);
        // 50% use non-verbal monster speech,
        // 50% try for more general silenced monster message instead
        else if (silenced && coinflip()) // skip all
            msg = __try_exact_string(prefixes, key, true, true, true, true, true);
    }
    return msg;
}

static bool _invalid_msg(const string &msg, bool no_player, bool no_foe,
                         bool no_foe_name, bool no_god, bool unseen)
{
    if (no_player
        && (msg.find("@player") != string::npos
            || msg.find("@Player") != string::npos
            || msg.find(":You") != string::npos))
    {
        return true;
    }

    if (no_player)
    {
        for (const string &line : split_string("\n", msg))
            if (starts_with(line, "You") || ends_with(line, "you."))
                return true;
    }

    if (no_foe && (msg.find("@foe") != string::npos
                   || msg.find("@Foe") != string::npos
                   || msg.find("foe@") != string::npos
                   || msg.find("@species") != string::npos))
    {
        return true;
    }

    if (no_god && (msg.find("_god@") != string::npos
                   || msg.find("@god_") != string::npos))
    {
        return true;
    }

    if (no_foe_name && msg.find("@foe_name@") != string::npos)
        return true;

    if (unseen && msg.find("VISUAL") != string::npos)
        return true;

    return false;
}

static string _try_exact_string(const vector<string> &prefixes,
                                const string &key,
                                bool no_player, bool no_foe,
                                bool no_foe_name, bool no_god,
                                bool unseen,
                                bool ignore_hostile  = false,
                                bool ignore_related  = false,
                                bool ignore_religion = false,
                                bool ignore_branch   = false,
                                bool ignore_silenced = false)
{
    string msg;
    for (int tries = 0; tries < 10; tries++)
    {
        msg =
            __try_exact_string(prefixes, key, ignore_hostile, ignore_related,
                               ignore_religion, ignore_branch, ignore_silenced);

        // If the first message was non-empty and discarded then discard
        // all subsequent empty messages, so as to not replace an
        // invalid non-empty message with an empty one.
        if (msg.empty())
        {
            if (tries == 0)
                return msg;
            else
            {
                tries--;
                continue;
            }
        }

        if (_invalid_msg(msg, no_player, no_foe, no_foe_name, no_god, unseen))
        {
            msg = "";
            continue;
        }
        break;
    }

    return msg;
}

static string __get_speak_string(const vector<string> &prefixes,
                                 const string &key,
                                 bool no_player, bool no_foe,
                                 bool no_foe_name, bool no_god,
                                 bool unseen)
{
    string msg = _try_exact_string(prefixes, key, no_player, no_foe,
                                   no_foe_name, no_god, unseen);

    if (!msg.empty())
        return msg;

    // Combinations of prefixes by threes
    const int size = prefixes.size();
    string prefix = "";
    if (size >= 3)
    {
        for (int i = 0; i < (size - 2); i++)
            for (int j = i + 1; j < (size - 1); j++)
                for (int k = j + 1; k < size; k++)
                {
                    prefix  = prefixes[i] + " ";
                    prefix += prefixes[j] + " ";
                    prefix += prefixes[k] + " ";

                    msg = getSpeakString("default " + prefix + key);

                    if (!msg.empty())
                        return msg;
                }
    }

    // Combinations of prefixes by twos
    if (size >= 2)
    {
        for (int i = 0; i < (size - 1); i++)
            for (int j = i + 1; j < size; j++)
            {
                prefix  = prefixes[i] + " ";
                prefix += prefixes[j] + " ";

                msg = getSpeakString("default " + prefix + key);

                if (!msg.empty())
                    return msg;
            }
    }

    // Prefixes singly
    if (size >= 1)
    {
        for (int i = 0; i < size; i++)
        {
            prefix  = prefixes[i] + " ";

            msg = getSpeakString("default " + prefix + key);

            if (!msg.empty())
                return msg;
        }
    }

    // No prefixes
    msg = getSpeakString("default " + key);

    return msg;
}

static string _get_speak_string(const vector<string> &prefixes,
                                string key,
                                const monster* mons,
                                bool no_player, bool no_foe,
                                bool no_foe_name, bool no_god,
                                bool unseen)
{
    if (you.hp <= 0)
        key += " triumphant";
    else if ((mons->flags & MF_BANISHED) && !player_in_branch(BRANCH_ABYSS))
        key += " banished";
    else if (mons->hit_points <= 0)
    {
        //separate death/permadeath lines for resurrection monsters
        if (mons_is_mons_class(mons, MONS_NATASHA)
               && !mons_felid_can_revive(mons)
            || mons->type == MONS_BENNU
               && !mons_bennu_can_revive(mons))
        {
            key += " permanently";
        }
        key += " killed";
    }
    else if (mons->is_summoned() && mons->get_ench(ENCH_SUMMON).duration <= 0)
        key += " timeout";

    string msg;
    for (int tries = 0; tries < 10; tries++)
    {
        msg =
            __get_speak_string(prefixes, key, no_player, no_foe,
                               no_foe_name, no_god, unseen);

        // If the first message was non-empty and discarded then discard
        // all subsequent empty messages, so as to not replace an
        // invalid non-empty message with an empty one.
        if (msg.empty())
        {
            if (tries == 0)
                return msg;
            else
            {
                tries--;
                continue;
            }
        }

        if (_invalid_msg(msg, no_player, no_foe, no_foe_name, no_god, unseen))
        {
            msg = "";
            continue;
        }

        break;
    }

    return msg;
}

/**
 * Rolls a chance for a monster to speak, and calls mons_speaks as necessary.
 *
 * @param mons The monster in question.
 */
void maybe_mons_speaks(monster* mons)
{
    // Very fast wandering/patrolling monsters might, in one monster turn,
    // move into the player's LOS and then back out (or the player
    // might move into their LOS and the monster move back out before
    // the player's view has a chance to update) so prevent them
    // from speaking.
    if (mons->is_patrolling() || mons_is_wandering(*mons))
        return;

    // per ef44f8a14, this seems to be handled elsewhere?
    if (mons->attitude == ATT_NEUTRAL)
        return;

    // too annoying for a permanent companion without more thought put into it
    if (mons_is_hepliaklqana_ancestor(mons->type))
        return;

    int chance = 21; // this is a very old number; no idea why it was chosen

    // Uniques tend to have more distinctive things to say and are only seen once.
    if (mons_is_unique(mons->type))
        chance -= 9;

    // allies stick around longer, so should probably have longer to say
    // their piece; no need for them to chatter as much.
    if (mons->wont_attack())
        chance *= 15;
    else if (!mons_is_unique(mons->type)
             && testbits(mons->flags, MF_BAND_FOLLOWER))
    {
        // Band members are a lot less likely to speak, since there's
        // a lot of them. Except for uniques.
        chance *= 10;
    }

    // Confused and fleeing monsters are more interesting.
    if (mons_is_fleeing(*mons))
        chance /= 2;
    if (mons->has_ench(ENCH_CONFUSION))
        chance /= 2;

    // Ally orc speech is a little one-note during Blood For Blood and there
    // are often a lot of them, so reduce the chance somewhat.
    if (mons->friendly() && mons_genus(mons->type) == MONS_ORC
        && you.duration[DUR_BLOOD_FOR_BLOOD])
    {
        chance *= 5 / 2;
    }

    if ((mons_class_flag(mons->type, M_SPEAKS)
                    || !mons->mname.empty())
                && one_chance_in(chance))
    {
        mons_speaks(mons);
    }
    else if ((mons->type == MONS_CRAZY_YIUF || mons->type == MONS_DONALD)
        && one_chance_in(9))
    {
        // Yiuf gets an extra chance to speak!
        // So does Donald.
        mons_speaks(mons);
    }
    else if (get_mon_shape(*mons) >= MON_SHAPE_QUADRUPED)
    {
        // Non-humanoid-ish monsters have a low chance of speaking
        // without the M_SPEAKS flag, to give the dungeon some
        // atmosphere/flavour.
        if (one_chance_in(chance * 4))
            mons_speaks(mons);
    }
    // Okay then, don't speak.
}

static actor* _get_foe(const monster &mon)
{
    if (!crawl_state.game_is_arena()
        && mon.wont_attack()
        && invalid_monster_index(mon.foe))
    {
        return &you;
    }
    return mon.get_foe();
}

// Returns true if something is said.
bool mons_speaks(monster* mons)
{
    ASSERT(mons); // XXX: change to monster &mons
    ASSERT(!invalid_monster_type(mons->type));

    // Natasha's death lines aren't physical speech.
    if ((mons->asleep() || mons->cannot_act() || mons->flags & MF_EXPLODE_KILL)
        && !(mons->type == MONS_NATASHA && !mons->alive()))
    {
        return false;
    }

    // Monsters talk on death even if invisible/silenced/etc.
    const bool force_speak = !mons->alive()
        || (mons->flags & MF_BANISHED) && !player_in_branch(BRANCH_ABYSS)
        || (mons->is_summoned() && mons->get_ench(ENCH_SUMMON).duration <= 0)
        || you.hp <= 0 // your death counts too
        || crawl_state.prev_cmd == CMD_LOOK_AROUND; // Wizard testing

    const bool unseen   = !you.can_see(*mons);
    const bool confused = mons->confused();

    if (!force_speak)
    {
        // Invisible monster tries to remain unnoticed. Unless they're
        // confused, since then they're too confused to realise they
        // should stay silent, but only if the player can see them, so as
        // to not have to deal with cases of speaking monsters which the
        // player can't see.
        if (unseen && !confused)
            return false;

        // Silenced monsters only "speak" 1/3 as often as non-silenced,
        // unless they're normally silent (S_SILENT).
        if (mons->is_silenced() && mons_can_shout(mons->type)
            && !one_chance_in(3))
        {
            return false;
        }

        // Berserk monsters just want your hide.
        if (mons->berserk_or_frenzied())
            return false;

        // Rolling beetles shouldn't twitch antennae
        if (mons->has_ench(ENCH_ROLLING))
            return false;

        // Charmed monsters aren't too expressive.
        if (mons->has_ench(ENCH_CHARM) && !one_chance_in(3))
            return false;
    }

    vector<string> prefixes;
    if (mons->neutral())
    {
        if (!force_speak && coinflip()) // Neutrals speak half as often.
            return false;

        prefixes.emplace_back("neutral");
    }
    else if (mons->friendly() && !crawl_state.game_is_arena())
        prefixes.emplace_back("friendly");
    else
        prefixes.emplace_back("hostile");

    if (mons_is_fleeing(*mons))
        prefixes.emplace_back("fleeing");

    bool silence = silenced(you.pos());
    if (silenced(mons->pos()) || mons->has_ench(ENCH_MUTE))
    {
        silence = true;
        prefixes.emplace_back("silenced");
    }

    if (confused)
        prefixes.emplace_back("confused");

    // Allows monster speech to be altered slightly on-the-fly.
    if (mons->props.exists(SPEECH_PREFIX_KEY))
        prefixes.push_back(mons->props[SPEECH_PREFIX_KEY].get_string());

    const actor*    foe  = _get_foe(*mons);
    const monster* m_foe = foe ? foe->as_monster() : nullptr;

    if (!foe || foe->is_player() || mons->wont_attack())
    {
        // Animals only look at the current player form, smart monsters at the
        // actual player genus.
        if (is_player_same_genus(mons->type)
            || mons_genus(mons->type) == MONS_ORC && you_worship(GOD_BEOGH))
        {
            prefixes.emplace_back("related");
        }
    }
    else
    {
        if (mons_genus(mons->mons_species()) ==
            mons_genus(foe->mons_species()))
        {
            prefixes.emplace_back("related");
        }
    }

    const god_type god = foe               ? foe->deity() :
                         crawl_state.game_is_arena() ? GOD_NO_GOD
                                           : you.religion;

    // Add Beogh to list of prefixes for orcs (hostile and friendly) if you
    // worship Beogh.
    if (you_worship(GOD_BEOGH) && mons_genus(mons->type) == MONS_ORC)
    {
        // During Blood for Blood, your orcs should be focused on that
        if (mons->friendly() && you.duration[DUR_BLOOD_FOR_BLOOD])
            prefixes.emplace_back("bfb");
        else
        {
            if (mons->god == GOD_BEOGH)
                prefixes.emplace_back("Beogh");
            else
                prefixes.emplace_back("unbeliever");
        }
    }
    else if (mons->type == MONS_PLAYER_GHOST)
    {
        // Use the *ghost's* religion, to get speech about its god. Only
        // sometimes, though, so we can get skill-based messages as well.
        if (coinflip())
            prefixes.push_back(god_name(mons->ghost->religion));
    }
    else
    {
        // Include our current god's name, too. This means that uniques
        // can have speech that is tailored to your specific god.
        if (is_good_god(god) && coinflip())
            prefixes.emplace_back("good god");
        else
            prefixes.push_back(god_name(you.religion));
    }

    // Include our current branch, too. It can make speech vary by branch for
    // uniques and other monsters! Specifically, Donald.
    prefixes.emplace_back(branches[you.where_are_you].abbrevname);

    // Include a prefix for the orb run.
    if (player_has_orb())
        prefixes.emplace_back("orb");

#ifdef DEBUG_MONSPEAK
    {
        string prefix;
        const int size = prefixes.size();
        for (int i = 0; i < size; i++)
        {
            prefix += prefixes[i];
            prefix += " ";
        }
        dprf(DIAG_SPEECH, "monster speech lookup for %s: prefix = %s",
             mons->name(DESC_PLAIN).c_str(), prefix.c_str());
    }
#endif

    const bool no_foe      = (foe == nullptr);
    const bool no_player   = crawl_state.game_is_arena()
                             || (!mons->wont_attack()
                                 && (!foe || !foe->is_player()));
    const bool mon_foe     = (m_foe != nullptr);
    const bool no_god      = no_foe || (mon_foe && foe->deity() == GOD_NO_GOD);
    const bool named_foe   = !no_foe && (!mon_foe || (m_foe->is_named()
                                && m_foe->type != MONS_ROYAL_JELLY));
    const bool no_foe_name = !named_foe
                             || (mon_foe && (m_foe->flags & MF_NAME_MASK));

    string msg;

    // First, try its exact name.
    if (mons->type == MONS_PLAYER_GHOST)
    {
        if (one_chance_in(5))
        {
            const ghost_demon &ghost = *(mons->ghost);
            string ghost_skill  = skill_name(ghost.best_skill);
            vector<string> with_skill = prefixes;
            with_skill.push_back(ghost_skill);
            msg = _get_speak_string(with_skill, "player ghost", mons,
                                    no_player, no_foe, no_foe_name, no_god,
                                    unseen);
        }
        if (msg.empty())
        {
            msg = _get_speak_string(prefixes, "player ghost", mons,
                                    no_player, no_foe, no_foe_name, no_god,
                                    unseen);
        }
    }
    else if (mons->type == MONS_PANDEMONIUM_LORD)
    {
        // Pandemonium demons have randomly generated names, so use
        // "pandemonium lord" instead.
        msg = _get_speak_string(prefixes, "pandemonium lord", mons,
                                no_player, no_foe, no_foe_name, no_god,
                                unseen);
    }
    else
    {
        if (msg.empty() && mons->props.exists(DBNAME_KEY))
        {
            msg = _get_speak_string(prefixes,
                                     mons->props[DBNAME_KEY].get_string(),
                                     mons, no_player, no_foe, no_foe_name,
                                     no_god, unseen);

            if (msg.empty())
            {
                // Try again without the prefixes if the key is empty. Vaults
                // *really* want monsters to use the key specified, rather than
                // the key with prefixes.
                vector<string> faux_prefixes;
                msg = _get_speak_string(faux_prefixes,
                                     mons->props[DBNAME_KEY].get_string(),
                                     mons, no_player, no_foe, no_foe_name,
                                     no_god, unseen);
            }
        }

        // If the monster was originally a unique which has been polymorphed
        // into a non-unique, is its current monster type capable of using its
        // old speech?
        if (!mons->mname.empty() && mons->can_speak() && msg.empty())
        {
            msg = _get_speak_string(prefixes, mons->name(DESC_PLAIN),
                                    mons, no_player, no_foe, no_foe_name,
                                    no_god, unseen);
        }

        if (msg.empty())
        {
            msg = _get_speak_string(prefixes, mons->base_name(DESC_PLAIN),
                                    mons, no_player, no_foe, no_foe_name,
                                    no_god, unseen);
        }
    }

    // The exact name brought no results, try monster genus.
    if ((msg.empty() || msg == "__NEXT")
        && mons_genus(mons->type) != mons->type)
    {
        msg = _get_speak_string(prefixes,
                       mons_type_name(mons_genus(mons->type), DESC_PLAIN),
                       mons, no_player, no_foe, no_foe_name, no_god,
                       unseen);
    }

    // __NONE means to be silent, and __NEXT means to try the next,
    // less exact method of describing the monster to find a speech
    // string.

    if (msg == "__NONE")
    {
#ifdef DEBUG_MONSPEAK
        dprf(DIAG_SPEECH, "result: \"__NONE\"!");
#endif
        return false;
    }

    // Now that we're not dealing with a specific monster name, include
    // whether or not it can move in the prefix.
    if (mons->is_stationary())
        prefixes.insert(prefixes.begin(), "stationary");

    // Names for the exact monster name and its genus have failed,
    // so try the monster's glyph/symbol.
    if (msg.empty() || msg == "__NEXT")
    {
        string key = "'";

        // Database keys are case-insensitive.
        if (isaupper(mons_base_char(mons->type)))
            key += "cap-";

        key += mons_base_char(mons->type);
        key += "'";
        msg = _get_speak_string(prefixes, key, mons, no_player, no_foe,
                                no_foe_name, no_god, unseen);
    }

    if (msg == "__NONE")
    {
#ifdef DEBUG_MONSPEAK
        dprf(DIAG_SPEECH, "result: \"__NONE\"!");
#endif
        return false;
    }

    if (mons_intel(*mons) < I_HUMAN)
        prefixes.insert(prefixes.begin(), "stupid");

    const mon_body_shape shape = get_mon_shape(*mons);
    if (msg.empty() || msg == "__NEXT")
    {
        msg = _get_speak_string(prefixes, get_mon_shape_str(shape), mons,
                                no_player, no_foe, no_foe_name, no_god,
                                unseen);
    }

    if (msg == "__NONE")
    {
#ifdef DEBUG_MONSPEAK
        dprf(DIAG_SPEECH, "result: \"__NONE\"!");
#endif
        return false;
    }

    // If we failed to get a message with a partial/hybrid humanoid, try moving
    // closer to plain humanoid.
    if ((msg.empty() || msg == "__NEXT") && mon_shape_is_humanoid(shape)
        && shape != MON_SHAPE_HUMANOID)
    {
        // If a humanoid monster has both wings and a tail, try removing
        // one and then the other to see if we get any results.
        if (shape == MON_SHAPE_HUMANOID_WINGED_TAILED)
        {
            msg = _get_speak_string(prefixes,
                                    get_mon_shape_str(MON_SHAPE_HUMANOID_TAILED),
                                    mons, no_player, no_foe, no_foe_name,
                                    no_god, unseen);

            // Only be silent if both tailed and winged return __NONE.
            if (msg.empty() || msg == "__NONE" || msg == "__NEXT")
            {
                string msg2;
                msg2 = _get_speak_string(prefixes,
                                         get_mon_shape_str(MON_SHAPE_HUMANOID_WINGED),
                                         mons, no_player, no_foe,
                                         no_foe_name, no_god, unseen);

                if (msg == "__NONE" && msg2 == "__NONE")
                {
#ifdef DEBUG_MONSPEAK
                    dprf(DIAG_SPEECH, "result: \"__NONE\"!");
#endif
                    return false;
                }

                if (msg2 == "__NONE")
                    msg2 = "";

                msg = msg2;
            }
        }

        if (msg.empty() || msg == "__NONE" || msg == "__NEXT")
        {
            msg = _get_speak_string(prefixes,
                                    get_mon_shape_str(MON_SHAPE_HUMANOID),
                                    mons, no_player, no_foe, no_foe_name,
                                    no_god, unseen);
        }
    }
    if (msg.empty() || msg == "__NONE")
    {
#ifdef DEBUG_MONSPEAK
        dprf(DIAG_SPEECH, "final result: %s!",
             (msg.empty() ? "empty" : "\"__NONE\""));
#endif
        return false;
    }

    if (msg == "__NEXT")
    {
        msg::streams(MSGCH_DIAGNOSTICS)
            << "__NEXT used by shape-based speech string for monster '"
            << mons->name(DESC_PLAIN) << "'" << endl;
        return false;
    }

    return mons_speaks_msg(mons, msg, MSGCH_TALK, silence);
}

bool invalid_msg(const monster &mon, string msg)
{
    const actor*    foe    = _get_foe(mon);
    const monster* m_foe   = foe ? foe->as_monster() : nullptr;
    // TODO: dedup with mons_speaks()
    const bool no_foe      = (foe == nullptr);
    const bool no_player   = crawl_state.game_is_arena()
                             || (!mon.wont_attack()
                                 && (!foe || !foe->is_player()));
    const bool mon_foe     = (m_foe != nullptr);
    const bool no_god      = no_foe || (mon_foe && foe->deity() == GOD_NO_GOD);
    const bool named_foe   = !no_foe && (!mon_foe || (m_foe->is_named()
                                && m_foe->type != MONS_ROYAL_JELLY));
    const bool no_foe_name = !named_foe
                             || (mon_foe && (m_foe->flags & MF_NAME_MASK));
    const bool unseen = !you.can_see(mon);
    return _invalid_msg(msg, no_player, no_foe, no_foe_name, no_god, unseen);

}

bool mons_speaks_msg(monster* mons, const string &msg,
                     const msg_channel_type def_chan, bool silence)
{
    if (!you.see_cell(mons->pos()))
        return false;

    mon_acting mact(mons);

    // We have a speech string, now parse and act on it.
    const string _msg = do_mon_str_replacements(msg, *mons);
    const vector<string> lines = split_string("\n", _msg);

    bool noticed = false;       // Any messages actually printed?

    if (mons->has_ench(ENCH_MUTE))
        silence = true;

    for (string line : lines)
    {
        // This function is a little bit of a problem for the message
        // channels since some of the messages it generates are "fake"
        // warnings to scare the player. In order to accommodate this
        // intent, we're falsely categorizing various things in the
        // function as spells and danger warning... everything else
        // just goes into the talk channel -- bwr
        // [jpeg] Added MSGCH_TALK_VISUAL for silent "chatter".
        msg_channel_type msg_type = def_chan;

        if (strip_channel_prefix(line, msg_type, silence))
        {
            if (msg_type == MSGCH_MONSTER_SPELL && mons->friendly())
                msg_type = MSGCH_FRIEND_SPELL;
            if (msg_type == MSGCH_MONSTER_ENCHANT && mons->friendly())
                msg_type = MSGCH_FRIEND_ENCHANT;
            if (line.empty())
                continue;
        }

        const bool old_noticed = noticed;
        noticed = true;         // Only one case is different.

        // Except for VISUAL, none of the above influence these.
        if (msg_type == MSGCH_TALK_VISUAL)
            silence = false;

        if (msg_type == MSGCH_TALK_VISUAL && !you.can_see(*mons))
            noticed = old_noticed;
        else
        {
            if (you.can_see(*mons))
                handle_seen_interrupt(mons);
            mprf(msg_type, "%s", line.c_str());
        }
    }
    return noticed;
}
