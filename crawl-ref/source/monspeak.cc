/*
 *  File:       monspeak.cc
 *  Summary:    Functions to handle speaking monsters
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 *      <1>    01/09/00        BWR     Created
 */

#include "AppHdr.h"
#include "monspeak.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

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
#include "spells2.h"
#include "spells4.h"
#include "stuff.h"
#include "view.h"

static std::string get_speak_string(const std::vector<std::string> prefixes,
                                    const std::string key,
                                    const monsters *monster,
                                    bool ignore_silenced = false)
{
    std::string prefix = "";
    bool silenced = false;
    const int size = prefixes.size();
    for (int i = 0; i < size; i++)
    {
        if (prefixes[i] == "silenced")
        {
            if (ignore_silenced)
                continue;
            silenced = true;
        }
        prefix += prefixes[i];
        prefix += " ";
    }

    std::string msg = "";
    // try string of all prefixes
    msg = getSpeakString(prefix + key);
    if (msg != "")
        return msg;
    
    // Combinations of prefixes by threes
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
                    if (msg != "")
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
                if (msg != "")
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
            if (msg != "")
                return msg;
        }
    }

    // No prefixes
    msg = getSpeakString("default " + key);

    // try the same ignoring silence
    if (msg == "" && silenced)
        return get_speak_string(prefixes, key, monster, true);
        
    return msg;
}

// Player ghosts with different classes can potentially speak different
// things.
static std::string player_ghost_speak_str(const monsters *monster,
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

    std::string msg = getSpeakString(prefix + ghost_class + " player ghost");

    if (msg == "__NONE")
        return "";

    if (msg == "" || msg == "__NEXT")
        msg = getSpeakString(prefix + "player ghost");

    return msg;
}

  // returns true if something is said
bool mons_speaks(const monsters *monster)
{
    // Invisible monster tries to remain unnoticed.  Unless they're
    // confused, since then they're too confused to realize they
    // should stay silent, but only if the player can see them, so as
    // to not have to deal with cases of speaking monsters which the
    // player can't see.
    if (monster->invisible() && !(player_monster_visible(monster)
                                  && monster->has_ench(ENCH_CONFUSION)))
        return false;

    // Silenced monsters only "speak" 1/3 as often as non-silenced,
    // unless they're normally silent (S_SILENT).  Use
    // get_monster_data(monster->type) to bypass mon_shouts()
    // replacing S_RANDOM with a random value.
    if (silenced(monster->x, monster->y)
        && get_monster_data(monster->type)->shouts != S_SILENT)
    {
        if (!one_chance_in(3))
            return false;
    }

    // charmed monsters aren't too expressive
    if (monster->has_ench(ENCH_CHARM))
        return false;

    // berserk monsters just want your hide.
    if (monster->has_ench(ENCH_BERSERK))
        return false;

    std::vector<std::string> prefixes;
    if (monster->attitude == ATT_FRIENDLY)
        prefixes.push_back("friendly");

    if (monster->behaviour == BEH_FLEE)
        prefixes.push_back("fleeing");

    bool silence = silenced(you.x_pos, you.y_pos);
    if (silenced(monster->x, monster->y))
    {
        silence = true;
        prefixes.push_back("silenced");
    }

    if (monster->has_ench(ENCH_CONFUSION))
        prefixes.push_back("confused");
        
    // Add Beogh to list of prefixes for orcs (hostile and friendly) if you
    // worship Beogh. (This assumes you being a Hill Orc, so might have odd
    // results in wizard mode.)
    if (you.religion == GOD_BEOGH && mons_genus(monster->type) == MONS_ORC)
        prefixes.push_back("beogh");

    std::string msg;

    // __NONE means to be silent, and __NEXT means to try the next,
    // less exact method of describing the monster to find a speech
    // string.

    // First, try its exact name
    if (monster->type == MONS_PLAYER_GHOST)
        // Player ghosts are treated differently.
        msg = player_ghost_speak_str(monster, prefixes);
    else if (monster->type == MONS_PANDEMONIUM_DEMON)
        // Pandemonium demons have randomly generated names,
        // so use "pandemonium lord" instead.
        msg = get_speak_string(prefixes, "pandemonium lord", monster);
    else
        msg = get_speak_string(prefixes, monster->name(DESC_PLAIN), monster);

    if (msg == "__NONE")
        return false;

    // The exact name brought no results, try species.
    if (msg.empty() || msg == "__NEXT")
    {
        msg = get_speak_string(prefixes,
                       mons_type_name(mons_species(monster->type), DESC_PLAIN),
                       monster);

        // Still nothing found? Try monster genus!
        if (msg.empty() || msg == "__NEXT")
        {
            msg = get_speak_string(prefixes,
                           mons_type_name(mons_genus(monster->type), DESC_PLAIN),
                           monster);
        }
    }

    // Now that we're not dealing with a specific monster name, include
    // whether or not it can move in the prefix
    if (mons_is_stationary(monster))
        prefixes.insert(prefixes.begin(), "stationary");

    // Names for the monster, its species and its genus all failed,
    // so try the monster's glyph/symbol.
    if (msg == "" || msg == "__NEXT")
    {
        std::string key = "'";

        // Database keys are case-insensitve.
        if (isupper(mons_char(monster->type)))
            key += "cap-";

        key += mons_char(monster->type);
        key += "'";
        msg = get_speak_string(prefixes, key, monster);
    }
    if (msg == "__NONE")
        return false;

    // Monster symbol didn't work, try monster shape.  Since we're
    // dealing with just the monster shape, change the prefix to
    // include info on if the monster's intelligence is at odds with
    // its shape.
    mon_body_shape shape = get_mon_shape(monster);
    mon_intel_type intel = mons_intel(monster->type);
    if (shape >= MON_SHAPE_HUMANOID && shape <= MON_SHAPE_NAGA
        && intel < I_NORMAL)
        prefixes.insert(prefixes.begin(), "stupid");
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
        prefixes.insert(prefixes.begin(), "smart");

    if (msg == "" || msg == "__NEXT")
        msg = get_speak_string(prefixes, get_mon_shape_str(shape), monster);
    if (msg == "__NONE")
        return false;

    // If we failed to get a message with a winged or tailed humanoid,
    // or a naga or centaur, try moving closer to plain humanoid
    if ((msg == "" || msg == "__NEXT") && shape > MON_SHAPE_HUMANOID
        && shape <= MON_SHAPE_NAGA)
    {
        // If a humanoid monster has both wings and a tail, try
        // removing one and then the other to see if we get any
        // results.
        if (shape == MON_SHAPE_HUMANOID_WINGED_TAILED)
        {
            shape = MON_SHAPE_HUMANOID_TAILED;
            msg = get_speak_string(prefixes,
                                   get_mon_shape_str(shape),
                                   monster);

            // Only be silent if both tailed and winged return __NONE
            if (msg == "" || msg == "__NONE" || msg == "__NEXT")
            {
                shape = MON_SHAPE_HUMANOID_WINGED;
                std::string msg2;
                msg2 = get_speak_string(prefixes,
                                        get_mon_shape_str(shape),
                                        monster);
                if (msg == "__NONE" && msg2 == "__NONE")
                    return false;

                if (msg2 == "__NONE")
                    msg2 = "";

                msg = msg2;
            }
        } // if (shape == MON_SHAPE_HUMANOID_WINGED_TAILED)
        if (msg == "" || msg == "__NONE" || msg == "__NEXT") 
        {
            shape = MON_SHAPE_HUMANOID;
            msg = get_speak_string(prefixes,
                                   get_mon_shape_str(shape),
                                   monster);
        }
    }
    if (msg == "__NONE" || msg == "")
        return false;

    if (msg == "__NEXT")
    {
        msg::streams(MSGCH_DIAGNOSTICS)
            << "__NEXT used by shape-based speech string for monster '"
            << monster->name(DESC_PLAIN) << "'" << std::endl;
        return false;
    }

    // We have a speech string, now parse and act on it.
    msg = do_mon_str_replacements(msg, monster);

    std::vector<std::string> lines = split_string("\n", msg);

    for (int i = 0, size = lines.size(); i < size; i++)
    {
        std::string line = lines[i];
        
        // This function is a little bit of a problem for the message
        // channels since some of the messages it generates are "fake"
        // warning to scare the player.  In order to accomidate this
        // intent, we're falsely categorizing various things in the
        // function as spells and danger warning... everything else
        // just goes into the talk channel -- bwr
        // [jpeg] Added MSGCH_TALK_VISUAL for silent "chatter"
        msg_channel_type msg_type = MSGCH_TALK;

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
                msg_type = MSGCH_MONSTER_SPELL;
            else if (param == "ENCHANT" && !silence || param == "VISUAL ENCHANT")
                msg_type = MSGCH_MONSTER_ENCHANT;
            else if (param == "PLAIN")
                msg_type = MSGCH_PLAIN;
            else
                match = false;

            if (match)
                line = line.substr(pos + 1);
        }

        // except for VISUAL none of the above influence these
        if (line == "__YOU_RESIST" && (!silence || param == "VISUAL"))
        {
            canned_msg( MSG_YOU_RESIST );
            continue;
        }
        else if (line == "__NOTHING_HAPPENS" && (!silence || param == "VISUAL"))
        {
            canned_msg( MSG_NOTHING_HAPPENS );
            continue;
        }
        else if (line == "__MORE" && (!silence || param == "VISUAL"))
        {
            more();
            continue;
        }

        mpr(line.c_str(), msg_type);
    }

    return true;
}                               // end mons_speaks = end of routine
