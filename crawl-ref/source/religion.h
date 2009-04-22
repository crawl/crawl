/*
 *  File:       religion.h
 *  Summary:    Misc religion related functions.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 */


#ifndef RELIGION_H
#define RELIGION_H

#include "enum.h"
#include "ouch.h"
#include "externs.h"

#define MAX_PIETY      200
#define HALF_MAX_PIETY (MAX_PIETY / 2)

#define MAX_PENANCE    200

enum harm_protection_type
{
    HPT_NONE = 0,
    HPT_PRAYING,
    HPT_ANYTIME,
    HPT_PRAYING_PLUS_ANYTIME,
    HPT_RELIABLE_PRAYING_PLUS_ANYTIME,
    NUM_HPTS
};

// Calls did_god_conduct() when the object goes out of scope.
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

bool is_evil_god(god_type god);
bool is_good_god(god_type god);
bool is_chaotic_god(god_type god);
bool is_priest_god(god_type god);
void simple_god_message(const char *event, god_type which_deity = you.religion);
int piety_breakpoint(int i);
std::string god_name(god_type which_god, bool long_name = false);
god_type string_to_god(const char *name, bool exact = true);

std::string get_god_powers(god_type which_god);
std::string get_god_likes(god_type which_god, bool verbose = false);
std::string get_god_dislikes(god_type which_god, bool verbose = false);

void dec_penance(int val);
void dec_penance(god_type god, int val);

bool did_god_conduct(conduct_type thing_done, int pgain, bool known = true,
                     const monsters *victim = NULL);
void set_attack_conducts(god_conduct_trigger conduct[3], const monsters *mon,
                         bool known = true);
void enable_attack_conducts(god_conduct_trigger conduct[3]);
void disable_attack_conducts(god_conduct_trigger conduct[3]);

void excommunication(god_type new_god = GOD_NO_GOD);
void gain_piety(int pgn);
void god_speaks(god_type god, const char *mesg);
void lose_piety(int pgn);
void offer_corpse(int corpse);
std::string god_prayer_reaction();
void pray();
void end_prayer();
void handle_god_time(void);
int god_colour(god_type god);
bool player_can_join_god(god_type which_god);
void god_pitch(god_type which_god);
int piety_rank(int piety = -1);
void offer_items();
bool god_hates_your_god(god_type god,
                        god_type your_god = you.religion);
std::string god_hates_your_god_reaction(god_type god,
                                        god_type your_god = you.religion);
bool god_hates_cannibalism(god_type god);
bool god_hates_killing(god_type god, const monsters* mon);
bool god_likes_butchery(god_type god);
bool god_hates_butchery(god_type god);
harm_protection_type god_protects_from_harm(god_type god, bool actual = true);
bool divine_retribution(god_type god);

bool zin_sustenance(bool actual = true);
bool yred_injury_mirror(bool actual = true);
bool beogh_water_walk();
void good_god_holy_attitude_change(monsters *holy);
void good_god_holy_fail_attitude_change(monsters *holy);
void yred_make_enslaved_soul(monsters *mon, bool force_hostile = false,
                             bool quiet = false, bool unlimited = false);
void beogh_convert_orc(monsters *orc, bool emergency,
                       bool converted_by_follower = false);
bool is_holy_item(const item_def& item);
bool is_evil_item(const item_def& item);
bool is_chaotic_item(const item_def& item);
bool is_holy_discipline(int discipline);
bool is_evil_discipline(int discipline);
bool is_holy_spell(spell_type spell, god_type god = GOD_NO_GOD);
bool is_evil_spell(spell_type spell, god_type god = GOD_NO_GOD);
bool is_chaotic_spell(spell_type spell, god_type god = GOD_NO_GOD);
bool is_any_spell(spell_type spell, god_type god = GOD_NO_GOD);
bool is_spellbook_type(const item_def& item, bool book_or_rod,
                       bool (*suitable)(spell_type spell, god_type god) =
                           is_any_spell,
                       god_type god = you.religion);
bool is_holy_spellbook(const item_def& item);
bool is_evil_spellbook(const item_def& item);
bool is_chaotic_spellbook(const item_def& item);
bool god_hates_spellbook(const item_def& item);
bool is_holy_rod(const item_def& item);
bool is_evil_rod(const item_def& item);
bool is_chaotic_rod(const item_def& item);
bool god_hates_rod(const item_def& item);
bool good_god_hates_item_handling(const item_def &item);
bool god_hates_item_handling(const item_def &item);
bool god_hates_spell_type(spell_type spell, god_type god = you.religion);

// NOTE: As of now, these two functions only say if a god won't give a
// spell/school when giving a gift.
bool god_dislikes_spell_type(spell_type spell, god_type god = you.religion);
bool god_dislikes_spell_discipline(int discipline, god_type god = you.religion);

bool trog_burn_spellbooks();
bool ely_destroy_weapons();

bool tso_unchivalric_attack_safe_monster(const monsters *mon);

void mons_make_god_gift(monsters *mon, god_type god = you.religion);
bool mons_is_god_gift(const monsters *mon, god_type god = you.religion);

bool is_undead_slave(const monsters* mon);
bool is_yred_undead_slave(const monsters* mon);
bool is_orcish_follower(const monsters* mon);
bool is_good_lawful_follower(const monsters* mon);
bool is_good_follower(const monsters* mon);
bool is_follower(const monsters* mon);
bool bless_follower(monsters *follower = NULL,
                    god_type god = you.religion,
                    bool (*suitable)(const monsters* mon) = is_follower,
                    bool force = false);

bool god_hates_attacking_friend(god_type god, const actor *fr);
bool god_hates_attacking_friend(god_type god, int species);
bool god_likes_items(god_type god);

void religion_turn_start();
void religion_turn_end();

int get_tension(god_type god = you.religion, bool count_travelling = true);
#endif
