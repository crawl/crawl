/*
 *  File:       spl-cast.h
 *  Summary:    Spell casting functions.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 */


#ifndef SPL_CAST_H
#define SPL_CAST_H

#include "enum.h"

enum spflag_type
{
    SPFLAG_NONE                 = 0x00000,
    SPFLAG_DIR_OR_TARGET        = 0x00001,      // use DIR_NONE targeting
    SPFLAG_TARGET               = 0x00002,      // use DIR_TARGET targeting
    SPFLAG_GRID                 = 0x00004,      // use DIR_GRID targeting
    SPFLAG_DIR                  = 0x00008,      // use DIR_DIR targeting
    SPFLAG_TARGETING_MASK       = 0x0000f,      // used to test for targeting
    SPFLAG_HELPFUL              = 0x00010,      // TARG_FRIENDS used
    SPFLAG_NEUTRAL              = 0x00020,      // TARG_ANY used
    SPFLAG_NOT_SELF             = 0x00040,      // aborts on isMe
    SPFLAG_UNHOLY               = 0x00080,      // counts at "unholy"
    SPFLAG_MAPPING              = 0x00100,      // a mapping spell of some kind
    SPFLAG_ESCAPE               = 0x00200,      // useful for running away
    SPFLAG_RECOVERY             = 0x00400,      // healing or recovery spell
    SPFLAG_AREA                 = 0x00800,      // area affect
    SPFLAG_BATTLE               = 0x01000,      // a non-Conjuration spell that
                                                // is still a battle spell
    SPFLAG_CARD                 = 0x02000,      // a card effect spell
    SPFLAG_MONSTER              = 0x04000,      // monster-only spell
    SPFLAG_INNATE               = 0x08000,      // an innate spell, even if
                                                // use by a priest/wizard
    SPFLAG_NOISY                = 0x10000,      // makes noise, even if innate
    SPFLAG_TESTING              = 0x20000,      // a testing/debugging spell
    SPFLAG_DEVEL                = 0x40000       // a spell under development
};

enum spret_type
{
    SPRET_ABORT = 0,            // should be left as 0
    SPRET_FAIL,
    SPRET_SUCCESS
};

int list_spells(bool toggle_with_I = true, bool viewing = false);
int spell_fail( spell_type spell );
int calc_spell_power(spell_type spell, bool apply_intel,
                     bool fail_rate_chk = false);
int spell_enhancement( unsigned int typeflags );

// last updaetd 12may2000 {dlb}
/* ***********************************************************************
 * called from: it_use3 - spell
 * *********************************************************************** */
void exercise_spell( spell_type spell_ex, bool spc, bool divide );


// last updaetd 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
bool cast_a_spell( void );

bool maybe_identify_staff( item_def &item, spell_type spell = SPELL_NO_SPELL );

void inspect_spells();


// last updaetd 12may2000 {dlb}
/* ***********************************************************************
 * called from: ability - debug - it_use3 - spell
 * *********************************************************************** */
spret_type your_spells(spell_type spell, int powc = 0, bool allow_fail = true);

const char* failure_rate_to_string( int fail );

int spell_power_colour(spell_type spell);
int spell_power_bars(spell_type spell);
std::string spell_power_string(spell_type spell);
std::string spell_range_string(spell_type spell);
const char* spell_hunger_string( spell_type spell );

#endif
