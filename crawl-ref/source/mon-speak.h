/**
 * @file
 * @brief Functions to handle speaking monsters
**/

#ifndef MONSPEAK_H
#define MONSPEAK_H

void maybe_mons_speaks(monster* mons);
bool mons_speaks(monster* mons);
bool mons_speaks_msg(monster* mons, const string &msg,
                     const msg_channel_type def_chan = MSGCH_TALK,
                     const bool silence = false);

#endif
