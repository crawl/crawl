/*
 *  File:       monspeak.h
 *  Summary:    Functions to handle speaking monsters
 */

#ifndef MONSPEAK_H
#define MONSPEAK_H

#include "externs.h"

void maybe_mons_speaks(monsters *monster);
bool mons_speaks(monsters *monster);
bool mons_speaks_msg(monsters *monster, const std::string &msg,
                     const msg_channel_type def_chan = MSGCH_TALK,
                     const bool silence = false);

#endif
