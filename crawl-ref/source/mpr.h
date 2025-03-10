/**
 * @file
 * @brief Functions used to print simple messages.
**/

#pragma once

#include "format.h"

// if you mess with this list, you'll need to make changes in initfile.cc
// to message_channel_names, and probably also to message.cc to colour
// everything properly
enum msg_channel_type
{
    MSGCH_PLAIN,            // regular text
    MSGCH_FRIEND_ACTION,    // friendly monsters taking actions
    MSGCH_PROMPT,           // various prompts
    MSGCH_GOD,              // god/religion (param is god)
    MSGCH_DURATION,         // effect down/warnings
    MSGCH_DANGER,           // serious life threats (ie very large HP attacks)
    MSGCH_WARN,             // much less serious threats
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
    MSGCH_EQUIPMENT,        // equipment listing messages
    MSGCH_FLOOR_ITEMS,      // like equipment, but lists of floor items
    MSGCH_MULTITURN_ACTION, // delayed action messages
    MSGCH_EXAMINE,          // messages describing monsters, features, items
    MSGCH_EXAMINE_FILTER,   // "less important" instances of the above
    MSGCH_DIAGNOSTICS,      // various diagnostic messages
    MSGCH_ERROR,            // error messages
    MSGCH_TUTORIAL,         // messages for tutorial
    MSGCH_ORB,              // messages for the orb
    MSGCH_TIMED_PORTAL,     // timed portal entry "tick tick tick" sounds
    MSGCH_HELL_EFFECT,      // hell effects
    MSGCH_MONSTER_WARNING,  // "Foo comes into view", et al
    MSGCH_DGL_MESSAGE,      // dgamelaunch messages
    MSGCH_DECOR_FLAVOUR,    // Flavour messages for walking on some features
    MSGCH_MONSTER_TIMEOUT,  // Messages for summons/temporary monsters expiring.

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

// Be sure to change diag_names in dbg-util.cc to match.
enum diag_type
{
    DIAG_NORMAL,
    DIAG_DNGN,
    DIAG_SKILLS,
    DIAG_COMBAT,
    DIAG_BEAM,
    DIAG_NOISE,
    DIAG_ABYSS,
    DIAG_MONPLACE,
#ifdef DEBUG_MONSPEAK
    DIAG_SPEECH,
#endif
#ifdef DEBUG_MONINDEX
    DIAG_MONINDEX,
#endif
    NUM_DIAGNOSTICS
};

msg_colour_type msg_colour(int colour);

void do_message_print(msg_channel_type channel, int param, bool cap,
                             bool nojoin, const char *format, va_list argp);

void mpr(const string &text);
// static inline void mpr(const char *text) { mpr(string(text)); }
void mpr_nojoin(msg_channel_type channel, string text);

// prevent implicit cast from formatted_string, use formatted_mpr instead
void mpr(const formatted_string &) = delete;

// 4.1-style mpr, currently named mprf for minimal disruption.
void mprf(msg_channel_type channel, int param, PRINTF(2, ));
void mprf(msg_channel_type channel, PRINTF(1, ));
void mprf(PRINTF(0, ));
void mprf_nojoin(msg_channel_type channel, PRINTF(1,));
void mprf_nojoin(PRINTF(0,));

void mprf_nocap(msg_channel_type channel, int param, PRINTF(2, ));
void mprf_nocap(msg_channel_type channel, PRINTF(1, ));
void mprf_nocap(PRINTF(0, ));

#ifdef DEBUG_DIAGNOSTICS
void dprf(PRINTF(0, ));
void dprf(diag_type param, PRINTF(1, ));
#else
# define dprf(...) ((void)0)
#endif
