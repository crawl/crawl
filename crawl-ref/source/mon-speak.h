/*
 *  File:       mon-speak.h
 *  Summary:    Functions to handle speaking monsters
 */

#ifndef MONSPEAK_H
#define MONSPEAK_H

#include "externs.h"

void maybe_mons_speaks(monsters* mons);
bool mons_speaks(monsters* mons);
bool mons_speaks_msg(monsters* mons, const std::string &msg,
                     const msg_channel_type def_chan = MSGCH_TALK,
                     const bool silence = false);

#endif
