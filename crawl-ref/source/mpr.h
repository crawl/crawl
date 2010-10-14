/*
 *  File:       mpr.h
 *  Summary:    Functions used to print simple messages.
 *  Written by: Linley Henzell
 */

#ifndef MPR_H
#define MPR_H

// if you mess with this list, you'll need to make changes in initfile.cc
// to message_channel_names, and probably also to message.cc to colour
// everything properly
enum msg_channel_type
{
    MSGCH_PLAIN,            // regular text
    MSGCH_FRIEND_ACTION,    // friendly monsters taking actions
    MSGCH_PROMPT,           // various prompts
    MSGCH_GOD,              // god/religion (param is god)
    MSGCH_PRAY,             // praying messages (param is god)
    MSGCH_DURATION,         // effect down/warnings
    MSGCH_DANGER,           // serious life threats (ie very large HP attacks)
    MSGCH_WARN,             // much less serious threats
    MSGCH_FOOD,             // hunger notices
    MSGCH_RECOVERY,         // recovery from disease/stat/poison condition
    MSGCH_SOUND,            // messages about things the player hears
    MSGCH_TALK,             // monster talk (param is monster type)
    MSGCH_TALK_VISUAL,      // silent monster "talk" (not restricted by silence)
    MSGCH_INTRINSIC_GAIN,   // player level/stat/species-power gains
    MSGCH_MUTATION,         // player gain/lose mutations
    MSGCH_MONSTER_SPELL,    // monsters casting spells
    MSGCH_MONSTER_ENCHANT,  // monsters'*' enchantments up and down
    MSGCH_FRIEND_SPELL,     // allied monsters casting spells
    MSGCH_FRIEND_ENCHANT,   // allied monsters' enchantments up and down
    MSGCH_MONSTER_DAMAGE,   // monster damage reports (param is level)
    MSGCH_MONSTER_TARGET,   // message marking the monster as a target
    MSGCH_BANISHMENT,       // Abyss-related messages
    MSGCH_ROTTEN_MEAT,      // messages about chunks/corpses becoming rotten
    MSGCH_EQUIPMENT,        // equipment listing messages
    MSGCH_FLOOR_ITEMS,      // like equipment, but lists of floor items
    MSGCH_MULTITURN_ACTION, // delayed action messages
    MSGCH_EXAMINE,          // messages describing monsters, features, items
    MSGCH_EXAMINE_FILTER,   // "less important" instances of the above
    MSGCH_DIAGNOSTICS,      // various diagnostic messages
    MSGCH_ERROR,            // error messages
    MSGCH_TUTORIAL,         // messages for tutorial

    NUM_MESSAGE_CHANNELS    // always last
};

enum msg_colour_type
{
    MSGCOL_BLACK        = 0,
    MSGCOL_BLUE,
    MSGCOL_GREEN,
    MSGCOL_CYAN,
    MSGCOL_RED,
    MSGCOL_MAGENTA,
    MSGCOL_BROWN,
    MSGCOL_LIGHTGREY,
    MSGCOL_DARKGREY,
    MSGCOL_LIGHTBLUE,
    MSGCOL_LIGHTGREEN,
    MSGCOL_LIGHTCYAN,
    MSGCOL_LIGHTRED,
    MSGCOL_LIGHTMAGENTA,
    MSGCOL_YELLOW,
    MSGCOL_WHITE,
    MSGCOL_DEFAULT,             // use default colour
    MSGCOL_ALTERNATE,           // use secondary default colour scheme
    MSGCOL_MUTED,               // don't print messages
    MSGCOL_PLAIN,               // same as plain channel
    MSGCOL_NONE,                // parsing failure, etc
};

msg_colour_type msg_colour(int colour);

void mpr(std::string text, msg_channel_type channel=MSGCH_PLAIN, int param=0,
         bool nojoin=false);

inline void mprnojoin(std::string text, msg_channel_type channel=MSGCH_PLAIN,
                      int param=0)
{
    mpr(text, channel, param, true);
}

// 4.1-style mpr, currently named mprf for minimal disruption.
void mprf(msg_channel_type channel, int param, const char *format, ...);
void mprf(msg_channel_type channel, const char *format, ...);
void mprf(const char *format, ...);

// Yay for C89 and lack of variadic #defines...
#ifdef DEBUG_DIAGNOSTICS
void dprf(const char *format, ...);
#else
static inline void dprf(const char *format, ...) {}
#endif

#endif
