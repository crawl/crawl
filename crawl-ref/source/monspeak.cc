/*
 *  File:       monspeak.cc
 *  Summary:    Functions to handle speaking monsters
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 */

#include "AppHdr.h"
REVISION("$Rev$");

#include "monspeak.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <algorithm>

#ifdef DOS
#include <conio.h>
#endif

#include "externs.h"

#include "beam.h"
#include "database.h"
#include "debug.h"
#include "fight.h"
#include "ghost.h"
#include "itemname.h"
#include "message.h"
#include "misc.h"
#include "monplace.h"
#include "monstuff.h"
#include "mon-util.h"
#include "mstuff2.h"
#include "newgame.h"
#include "player.h"
#include "religion.h"
#include "spells2.h"
#include "spells4.h"
#include "state.h"
#include "stuff.h"
#include "view.h"

// Try the exact key lookup along with the entire prefix list.
// If that fails, start ignoring hostile/religion/silence, in that order,
// first skipping hostile, then hostile *and* religion, then all three.
static std::string __try_exact_string(const std::vector<std::string> &prefixes,
                                      const std::string &key,
                                      bool ignore_hostile  = false,
                                      bool ignore_related  = false,
                                      bool ignore_religion = false,
                                      bool ignore_silenced = false)
{
    bool hostile  = false;
    bool related  = false;
    bool religion = false;
    bool silenced = false;

    std::string prefix = "";
    std::string msg = "";
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
         else if (prefixes[i] == "beogh" || prefixes[i] == "good god"
                  || prefixes[i] == "evil god")
         {
             if (ignore_religion)
                 continue;
             religion = true;
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
            if (religion) // skip hostile and religion
                msg = __try_exact_string(prefixes, key, true, false, true);
            else // skip hostile and related
                msg = __try_exact_string(prefixes, key, true, true);
        }
        else if (religion) // skip hostile, related and religion
            msg = __try_exact_string(prefixes, key, true, true, true);
        // 50% use non-verbal monster speech,
        // 50% try for more general silenced monster message instead
        else if (silenced && coinflip()) // skip all
            msg = __try_exact_string(prefixes, key, true, true, true, true);
    }
    return msg;
}

static bool _invalid_msg(const std::string &msg, bool no_player, bool no_foe,
                         bool no_foe_name, bool no_god, bool unseen)
{
    if (no_player
        && (msg.find("@player") != std::string::npos
            || msg.find("@Player") != std::string::npos
            || msg.find(":You") != std::string::npos))
    {
        return (true);
    }

    if (no_player)
    {
        std::vector<std::string> lines = split_string("\n", msg);
        for (unsigned int i = 0; i < lines.size(); i++)
        {
            if (starts_with(lines[i], "You")
                || ends_with(lines[i], "you."))
            {
                return (true);
            }
        }
    }

    if (no_foe && (msg.find("@foe") != std::string::npos
                   || msg.find("@Foe") != std::string::npos
                   || msg.find("foe@") != std::string::npos
                   || msg.find("@species") != std::string::npos))
    {
        return (true);
    }

    if (no_god && (msg.find("_god@") != std::string::npos
                   || msg.find("@god_") != std::string::npos))
    {
        return (true);
    }

    if (no_foe_name && msg.find("@foe_name@") != std::string::npos)
        return (true);

    if (unseen && msg.find("VISUAL") != std::string::npos)
        return (true);

    return (false);
}

static std::string _try_exact_string(const std::vector<std::string> &prefixes,
                                     const std::string &key,
                                     bool no_player, bool no_foe,
                                     bool no_foe_name, bool no_god,
                                     bool unseen,
                                     bool ignore_hostile  = false,
                                     bool ignore_related  = false,
                                     bool ignore_religion = false,
                                     bool ignore_silenced = false)
{
    std::string msg;
    for (int tries = 0; tries < 10; tries++)
    {
        msg =
            __try_exact_string(prefixes, key, ignore_hostile, ignore_related,
                               ignore_religion, ignore_silenced);

        // If the first message was non-empty and discarded then discard
        // all subsequent empty messages, so as to not replace an
        // invalid non-empty message with an empty one.
        if (msg.empty())
        {
            if (tries == 0)
                return (msg);
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

    return (msg);
}

static std::string __get_speak_string(const std::vector<std::string> &prefixes,
                                      const std::string &key,
                                      const monsters *monster,
                                      bool no_player, bool no_foe,
                                      bool no_foe_name, bool no_god,
                                      bool unseen)
{
    std::string msg = _try_exact_string(prefixes, key, no_player, no_foe,
                                        no_foe_name, no_god, unseen);

    if (!msg.empty())
        return msg;

    // Combinations of prefixes by threes
    const int size = prefixes.size();
    std::string prefix = "";
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

static std::string _get_speak_string(const std::vector<std::string> &prefixes,
                                     std::string key,
                                     const monsters *monster,
                                     bool no_player, bool no_foe,
                                     bool no_foe_name, bool no_god,
                                     bool unseen)
{
    int duration = 1;
    if (monster->hit_points <= 0)
        key += " killed";
    else if ((monster->flags & MF_BANISHED) && you.level_type != LEVEL_ABYSS)
        key += " banished";
    else if (monster->is_summoned(&duration) && duration <= 0)
        key += " unsummoned";

    std::string msg;
    for (int tries = 0; tries < 10; tries++)
    {
        msg =
            __get_speak_string(prefixes, key, monster, no_player, no_foe,
                               no_foe_name, no_god, unseen);

        // If the first message was non-empty and discarded then discard
        // all subsequent empty messages, so as to not replace an
        // invalid non-empty message with an empty one.
        if (msg.empty())
        {
            if (tries == 0)
                return (msg);
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

    return (msg);
}

// Player ghosts with different classes can potentially speak different
// things.
static std::string _player_ghost_speak_str(const monsters *monster,
                                     const std::vector<std::string> prefixes)
{
    const ghost_demon &ghost = *(monster->ghost);
    std::string ghost_class = get_class_name(ghost.job);

    std::string prefix = "";
    for (int i = 0, size = prefixes.size(); i < size; i++)
    {
        prefix += prefixes[i];
        prefix += " ";
    }

    // first try together with class name
    std::string msg = getSpeakString(prefix + ghost_class + " player ghost");

    // else try without class name
    if (msg.empty() || msg == "__NEXT")
        msg = getSpeakString(prefix + "player ghost");

    return msg;
}

// If the monster was originally a unique which has been polymorphed into
// a non-unique, is its current monter type capable of using its old
// speech?
static bool _polyd_can_speak(const monsters* monster)
{
    // Wizard and priest monsters can always speak.
    if (mons_class_flag(monster->type, M_ACTUAL_SPELLS | M_PRIEST))
        return (true);

    // Silent or non-sentient monsters can't use the original speech.
    if (mons_intel(monster) < I_NORMAL
        || mons_shouts(monster->type) == S_SILENT)
    {
        return (false);
    }

    // Does it have the proper vocal equipment?
    const mon_body_shape shape = get_mon_shape(monster);
    return (shape >= MON_SHAPE_HUMANOID && shape <= MON_SHAPE_NAGA);
}

// Returns true if something is said.
bool mons_speaks(monsters *monster)
{
    ASSERT(!invalid_monster_class(monster->type));

    // Monsters always talk on death, even if invisible/silenced/etc.
    int duration = 1;
    const bool force_speak = !monster->alive()
        || (monster->flags & MF_BANISHED) && you.level_type != LEVEL_ABYSS
        || (monster->is_summoned(&duration) && duration <= 0)
        || crawl_state.prev_cmd == CMD_LOOK_AROUND; // Wizard testing

    const bool unseen   = !you.can_see(monster);
    const bool confused = monster->confused();

    if (!force_speak)
    {
       // Invisible monster tries to remain unnoticed.  Unless they're
       // confused, since then they're too confused to realize they
       // should stay silent, but only if the player can see them, so as
       // to not have to deal with cases of speaking monsters which the
       // player can't see.
       if (unseen && !confused)
           return (false);

        // Silenced monsters only "speak" 1/3 as often as non-silenced,
        // unless they're normally silent (S_SILENT).  Use
        // get_monster_data(monster->type) to bypass mon_shouts()
        // replacing S_RANDOM with a random value.
        if (silenced(monster->pos())
            && get_monster_data(monster->type)->shouts != S_SILENT)
        {
            if (!one_chance_in(3))
                return (false);
        }

        // Berserk monsters just want your hide.
        if (monster->has_ench(ENCH_BERSERK))
            return (false);

        // Monsters in a battle frenzy are likewise occupied.
        if (monster->has_ench(ENCH_BATTLE_FRENZY) && !one_chance_in(3))
            return (false);

        // Charmed monsters aren't too expressive.
        if (monster->has_ench(ENCH_CHARM) && !one_chance_in(3))
            return (false);
    }

    std::vector<std::string> prefixes;
    if (mons_neutral(monster))
    {
        if (!force_speak && coinflip()) // Neutrals speak half as often.
            return (false);

        prefixes.push_back("neutral");
    }
    else if (mons_friendly(monster) && !crawl_state.arena)
        prefixes.push_back("friendly");
    else
        prefixes.push_back("hostile");

    if (mons_is_fleeing(monster))
        prefixes.push_back("fleeing");

    bool silence = silenced(you.pos());
    if (silenced(monster->pos()))
    {
        silence = true;
        prefixes.push_back("silenced");
    }

    if (confused)
        prefixes.push_back("confused");

    const actor*    foe   = (!crawl_state.arena && mons_wont_attack(monster)
                                && invalid_monster_index(monster->foe)) ?
                                    &you : monster->get_foe();
    const monsters* m_foe = (foe && foe->atype() == ACT_MONSTER) ?
                                dynamic_cast<const monsters*>(foe) : NULL;

    // animals only look at the current player form, smart monsters at the
    // actual player genus
    if (!foe || foe->atype() == ACT_PLAYER)
    {
        if (is_player_same_species(monster->type,
                                   mons_intel(monster) <= I_ANIMAL))
        {
            prefixes.push_back("related"); // maybe overkill for Beogh?
        }
    }
    else
    {
        if (mons_genus(monster->mons_species()) ==
            mons_genus(foe->mons_species()))
        {
            prefixes.push_back("related");
        }
    }

    const god_type god = foe               ? foe->deity() :
                         crawl_state.arena ? GOD_NO_GOD :
                                             you.religion;

    // Add Beogh to list of prefixes for orcs (hostile and friendly) if you
    // worship Beogh. (This assumes your being a Hill Orc, so might have odd
    // results in wizard mode.) Don't count charmed or summoned orcs.
    if (you.religion == GOD_BEOGH && mons_genus(monster->type) == MONS_ORC
        && !monster->has_ench(ENCH_CHARM) && !monster->is_summoned())
    {
        if (monster->god == GOD_BEOGH)
            prefixes.push_back("beogh");
        else
            prefixes.push_back("unbeliever");
    }
    else
    {
        if (is_good_god(god))
            prefixes.push_back("good god");
        else if (is_evil_god(god))
            prefixes.push_back("evil god");
    }

#ifdef DEBUG_MONSPEAK
    {
        std::string prefix;
        const int size = prefixes.size();
        for (int i = 0; i < size; i++)
        {
            prefix += prefixes[i];
            prefix += " ";
        }
        mprf(MSGCH_DIAGNOSTICS, "monster speech lookup for %s: prefix = %s",
             monster->name(DESC_PLAIN).c_str(), prefix.c_str());
    }
#endif

    const bool no_foe      = foe == NULL;
    const bool no_player   = crawl_state.arena
                             || (!mons_wont_attack(monster)
                                 && (!foe || foe->atype() != ACT_PLAYER));
    const bool mon_foe     = m_foe != NULL;
    const bool no_god      = no_foe || (mon_foe && foe->deity() == GOD_NO_GOD);
    const bool named_foe   = !no_foe
        && (!mon_foe || (m_foe->is_named()
                         && m_foe->type != MONS_ROYAL_JELLY));
    const bool no_foe_name = !named_foe
                          || (mon_foe && (m_foe->flags & MF_NAME_MASK));

    std::string msg;

    // First, try its exact name.
    if (monster->type == MONS_PLAYER_GHOST)
    {
        // Player ghosts are treated differently.
        msg = _player_ghost_speak_str(monster, prefixes);
    }
    else if (monster->type == MONS_PANDEMONIUM_DEMON)
    {
        // Pandemonium demons have randomly generated names, so use
        // "pandemonium lord" instead.
        msg = _get_speak_string(prefixes, "pandemonium lord", monster,
                                no_player, no_foe, no_foe_name, no_god,
                                unseen);
    }
    else
    {
        if (!monster->mname.empty() && _polyd_can_speak(monster))
        {
            msg = _get_speak_string(prefixes, monster->name(DESC_PLAIN),
                                    monster, no_player, no_foe, no_foe_name,
                                    no_god, unseen);
        }

        if (msg.empty())
        {
            msg = _get_speak_string(prefixes, monster->base_name(DESC_PLAIN),
                                    monster, no_player, no_foe, no_foe_name,
                                    no_god, unseen);
        }
    }

    // The exact name brought no results, try monster genus.
    if ((msg.empty() || msg == "__NEXT")
        && mons_genus(monster->type) != monster->type)
    {
        msg = _get_speak_string(prefixes,
                       mons_type_name(mons_genus(monster->type), DESC_PLAIN),
                       monster, no_player, no_foe, no_foe_name, no_god,
                       unseen);
    }

    // __NONE means to be silent, and __NEXT means to try the next,
    // less exact method of describing the monster to find a speech
    // string.

    if (msg == "__NONE")
    {
#ifdef DEBUG_MONSPEAK
        mpr("result: \"__NONE\"!", MSGCH_DIAGNOSTICS);
#endif
        return (false);
    }

    // Now that we're not dealing with a specific monster name, include
    // whether or not it can move in the prefix.
    if (mons_is_stationary(monster))
        prefixes.insert(prefixes.begin(), "stationary");

    // Names for the exact monster name and its genus have failed,
    // so try the monster's glyph/symbol.
    if (msg.empty() || msg == "__NEXT")
    {
        std::string key = "'";

        // Database keys are case-insensitve.
        if (isupper(mons_char(monster->type)))
            key += "cap-";

        key += mons_char(monster->type);
        key += "'";
        msg = _get_speak_string(prefixes, key, monster, no_player, no_foe,
                                no_foe_name, no_god, unseen);
    }

    if (msg == "__NONE")
    {
#ifdef DEBUG_MONSPEAK
        mpr("result: \"__NONE\"!", MSGCH_DIAGNOSTICS);
#endif
        return (false);
    }

    // Monster symbol didn't work, try monster shape.  Since we're
    // dealing with just the monster shape, change the prefix to
    // include info on if the monster's intelligence is at odds with
    // its shape.
    mon_body_shape shape = get_mon_shape(monster);
    mon_intel_type intel = mons_intel(monster);
    if (shape >= MON_SHAPE_HUMANOID && shape <= MON_SHAPE_NAGA
        && intel < I_NORMAL)
    {
        prefixes.insert(prefixes.begin(), "stupid");
    }
    else if (shape >= MON_SHAPE_QUADRUPED && shape <= MON_SHAPE_FISH)
    {
        if (mons_char(monster->type) == 'w')
        {
            if (intel > I_INSECT)
                prefixes.insert(prefixes.begin(), "smart");
            else if (intel < I_INSECT)
                prefixes.insert(prefixes.begin(), "stupid");
        }
        else
        {
            if (intel > I_ANIMAL)
                prefixes.insert(prefixes.begin(), "smart");
            else if (intel < I_ANIMAL)
                prefixes.insert(prefixes.begin(), "stupid");
        }
    }
    else if (shape >= MON_SHAPE_INSECT && shape <= MON_SHAPE_SNAIL)
    {
        if (intel > I_INSECT)
            prefixes.insert(prefixes.begin(), "smart");
        else if (intel < I_INSECT)
            prefixes.insert(prefixes.begin(), "stupid");
    }
    else if (shape >= MON_SHAPE_PLANT && shape <= MON_SHAPE_BLOB
             && intel > I_PLANT)
    {
        prefixes.insert(prefixes.begin(), "smart");
    }

    if (msg.empty() || msg == "__NEXT")
    {
        msg = _get_speak_string(prefixes, get_mon_shape_str(shape), monster,
                                no_player, no_foe, no_foe_name, no_god,
                                unseen);
    }

    if (msg == "__NONE")
    {
#ifdef DEBUG_MONSPEAK
        mpr("result: \"__NONE\"!", MSGCH_DIAGNOSTICS);
#endif
        return (false);
    }

    // If we failed to get a message with a winged or tailed humanoid,
    // or a naga or centaur, try moving closer to plain humanoid.
    if ((msg.empty() || msg == "__NEXT") && shape > MON_SHAPE_HUMANOID
        && shape <= MON_SHAPE_NAGA)
    {
        // If a humanoid monster has both wings and a tail, try removing
        // one and then the other to see if we get any results.
        if (shape == MON_SHAPE_HUMANOID_WINGED_TAILED)
        {
            shape = MON_SHAPE_HUMANOID_TAILED;
            msg = _get_speak_string(prefixes, get_mon_shape_str(shape),
                                    monster, no_player, no_foe, no_foe_name,
                                    no_god, unseen);

            // Only be silent if both tailed and winged return __NONE.
            if (msg.empty() || msg == "__NONE" || msg == "__NEXT")
            {
                shape = MON_SHAPE_HUMANOID_WINGED;
                std::string msg2;
                msg2 = _get_speak_string(prefixes, get_mon_shape_str(shape),
                                         monster, no_player, no_foe,
                                         no_foe_name, no_god, unseen);

                if (msg == "__NONE" && msg2 == "__NONE")
                {
#ifdef DEBUG_MONSPEAK
                    mpr("result: \"__NONE\"!", MSGCH_DIAGNOSTICS);
#endif
                    return (false);
                }

                if (msg2 == "__NONE")
                    msg2 = "";

                msg = msg2;
            }
        } // if (shape == MON_SHAPE_HUMANOID_WINGED_TAILED)
        if (msg.empty() || msg == "__NONE" || msg == "__NEXT")
        {
            shape = MON_SHAPE_HUMANOID;
            msg = _get_speak_string(prefixes, get_mon_shape_str(shape),
                                    monster, no_player, no_foe, no_foe_name,
                                    no_god, unseen);
        }
    }
    if (msg.empty() || msg == "__NONE")
    {
#ifdef DEBUG_MONSPEAK
        mprf(MSGCH_DIAGNOSTICS, "final result: %s!",
             (msg.empty() ? "empty" : "\"__NONE\""));
#endif
        return (false);
    }

    if (msg == "__NEXT")
    {
        msg::streams(MSGCH_DIAGNOSTICS)
            << "__NEXT used by shape-based speech string for monster '"
            << monster->name(DESC_PLAIN) << "'" << std::endl;
        return (false);
    }

    if (foe == NULL)
        msg = replace_all(msg, "__YOU_RESIST", "__NOTHING_HAPPENS");
    else if (foe->atype() == ACT_MONSTER)
    {
        if (you.can_see(foe))
            msg = replace_all(msg, "__YOU_RESIST", "@The_monster@ resists.");
        else
            msg = replace_all(msg, "__YOU_RESIST", "__NOTHING_HAPPENS");
    }

    return (mons_speaks_msg(monster, msg, MSGCH_TALK, silence));
}

bool mons_speaks_msg(monsters *monster, const std::string &msg,
                     const msg_channel_type def_chan, const bool silence)
{
    if (!mons_near(monster))
        return (false);

    mon_acting mact(monster);

    // We have a speech string, now parse and act on it.
    const std::string _msg = do_mon_str_replacements(msg, monster);
    const std::vector<std::string> lines = split_string("\n", _msg);

    bool noticed = false;       // Any messages actually printed?

    for (int i = 0, size = lines.size(); i < size; ++i)
    {
        std::string line = lines[i];

        // This function is a little bit of a problem for the message
        // channels since some of the messages it generates are "fake"
        // warning to scare the player.  In order to accomodate this
        // intent, we're falsely categorizing various things in the
        // function as spells and danger warning... everything else
        // just goes into the talk channel -- bwr
        // [jpeg] Added MSGCH_TALK_VISUAL for silent "chatter".
        msg_channel_type msg_type = def_chan;

        std::string param = "";
        std::string::size_type pos = line.find(":");

        if (pos != std::string::npos)
            param = line.substr(0, pos);

        if (!param.empty())
        {
            bool match = true;

            if (param == "DANGER")
                msg_type = MSGCH_DANGER;
            else if (param == "WARN" && !silence || param == "VISUAL WARN")
                msg_type = MSGCH_WARN;
            else if (param == "SOUND")
                msg_type = MSGCH_SOUND;
            else if (param == "VISUAL")
                msg_type = MSGCH_TALK_VISUAL;
            else if (param == "SPELL" && !silence || param == "VISUAL SPELL")
            {
                msg_type = mons_friendly(monster) ? MSGCH_FRIEND_SPELL
                                                  : MSGCH_MONSTER_SPELL;
            }
            else if (param == "ENCHANT" && !silence
                     || param == "VISUAL ENCHANT")
            {
                msg_type = mons_friendly(monster) ? MSGCH_FRIEND_ENCHANT
                                                  : MSGCH_MONSTER_ENCHANT;
            }
            else if (param == "PLAIN")
                msg_type = MSGCH_PLAIN;
            else
                match = false;

            if (match)
                line = line.substr(pos + 1);
        }

        const bool old_noticed = noticed;
        noticed = true;         // Only one case is different.

        // Except for VISUAL, none of the above influence these.
        if (line == "__YOU_RESIST" && (!silence || param == "VISUAL"))
            canned_msg( MSG_YOU_RESIST );
        else if (line == "__NOTHING_HAPPENS" && (!silence || param == "VISUAL"))
            canned_msg( MSG_NOTHING_HAPPENS );
        else if (line == "__MORE" && (!silence || param == "VISUAL"))
            more();
        else if (msg_type == MSGCH_TALK_VISUAL && !you.can_see(monster))
            noticed = old_noticed;
        else
        {
            if (you.can_see(monster))
                handle_seen_interrupt(monster);
            mpr(line.c_str(), msg_type);
        }
    }
    return (noticed);
}
