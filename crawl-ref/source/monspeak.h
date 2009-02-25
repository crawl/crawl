/*
 *  File:       monspeak.h
 *  Summary:    Functions to handle speaking monsters
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 */

#ifndef MONSPEAK_H
#define MONSPEAK_H

#include "externs.h"

bool mons_speaks(monsters *monster);
bool mons_speaks_msg(monsters *monster, const std::string &msg,
                     const msg_channel_type def_chan = MSGCH_TALK,
                     const bool silence = false);

#endif
