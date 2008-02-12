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
#include "ouch.h"
#include "externs.h"

#define MAX_PIETY 200

class actor;
class monsters;

enum harm_protection_type
{
    HPT_NONE = 0,
    HPT_PRAYING,
    HPT_ANYTIME,
    HPT_PRAYING_PLUS_ANYTIME,
    NUM_HPTS
};

bool is_priest_god(god_type god);
void simple_god_message( const char *event, god_type which_deity = GOD_NO_GOD );
int piety_breakpoint(int i);
std::string god_name(god_type which_god, bool long_name = false);
void dec_penance(int val);
void dec_penance(god_type god, int val);

bool did_god_conduct(conduct_type thing_done, int pgain, bool known = true,
                     const monsters *victim = NULL);

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
harm_protection_type god_protects_from_harm(god_type god, bool actual = true);
void god_smites_you(god_type god, kill_method_type death_type,
                    const char *message = NULL);
void divine_retribution(god_type god);

bool beogh_water_walk();
void beogh_idol_revenge();
void good_god_convert_holy(monsters *holy);
void beogh_convert_orc(monsters *orc, bool emergency);
bool is_evil_weapon(const item_def& weap);
bool ely_destroy_weapons();
bool trog_burn_books();
bool tso_stab_safe_monster(const actor *act);

bool god_hates_attacking_friend(god_type god, const actor *fr);

bool is_evil_god(god_type god);
bool is_good_god(god_type god);

// Calls did_god_conduct when the object goes out of scope.
struct god_conduct_trigger
{
    conduct_type conduct;
    int pgain;
    bool known;
    bool enabled;
    std::auto_ptr<monsters> victim;

    god_conduct_trigger(conduct_type c = NUM_CONDUCTS,
                        int pg = 0,
                        bool kn = true,
                        const monsters *vict = NULL);

    void set(conduct_type c = NUM_CONDUCTS,
             int pg = 0,
             bool kn = true,
             const monsters *vict = NULL);

    ~god_conduct_trigger();
};

#endif
