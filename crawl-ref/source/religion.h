/*
 *  File:       religion.cc
 *  Summary:    Misc religion related functions.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 *               <1>     -/--/--        LRH             Created
 */


#ifndef RELIGION_H
#define RELIGION_H

#include "enum.h"

void simple_god_message( const char *event, int which_deity = GOD_NO_GOD );
int piety_breakpoint(int i);
char *god_name(int which_god,bool long_name=false); //mv
void dec_penance(int val);
void dec_penance(int god, int val);
void Xom_acts(bool niceness, int sever, bool force_sever);
bool did_god_conduct(int thing_done, int pgain);
void excommunication(void);
void gain_piety(char pgn);
void god_speaks( int god, const char *mesg );
void lose_piety(char pgn);
void offer_corpse(int corpse);
std::string god_prayer_reaction();
void pray();
void handle_god_time(void);
char god_colour(char god);
void god_pitch(unsigned char which_god);
int piety_rank(int piety = -1);
void offer_items();

#endif
