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

#define MAX_PIETY 200

bool is_priest_god(god_type god);
void simple_god_message( const char *event, god_type which_deity = GOD_NO_GOD );
int piety_breakpoint(int i);
const char *god_name(god_type which_god, bool long_name = false); //mv
void dec_penance(int val);
void dec_penance(god_type god, int val);
bool did_god_conduct(conduct_type thing_done, int pgain);
void excommunication(void);
void gain_piety(int pgn);
void god_speaks(god_type god, const char *mesg );
void lose_piety(int pgn);
void offer_corpse(int corpse);
std::string god_prayer_reaction();
void pray();
void handle_god_time(void);
int god_colour(god_type god);
void god_pitch(god_type which_god);
int piety_rank(int piety = -1);
void offer_items();
bool god_likes_butchery(god_type god);
bool god_hates_butchery(god_type god);
void divine_retribution(god_type god);

bool xom_is_nice();
void xom_is_stimulated(int maxinterestingness);
void xom_acts(bool niceness, int sever);
const char *describe_xom_favour();

bool beogh_water_walk();
void beogh_idol_revenge();
bool ely_destroy_weapons();
void trog_burn_books();

inline void xom_acts(int sever)
{
    xom_acts(xom_is_nice(), sever);
}

#endif
