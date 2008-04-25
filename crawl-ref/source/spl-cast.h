/*
 *  File:       spl-cast.h
 *  Summary:    Spell casting functions.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 *               <1>     -/--/--        LRH             Created
 */


#ifndef SPL_CAST_H
#define SPL_CAST_H

#include "enum.h"

enum spflag_type
{
    SPFLAG_NONE                 = 0x0000,
    SPFLAG_DIR_OR_TARGET        = 0x0001,       // use DIR_NONE targeting
    SPFLAG_TARGET               = 0x0002,       // use DIR_TARGET targeting
    SPFLAG_GRID                 = 0x0004,       // use DIR_GRID targeting
    SPFLAG_DIR                  = 0x0008,       // use DIR_DIR targeting
    SPFLAG_TARGETING_MASK       = 0x000f,       // used to test for targeting
    SPFLAG_HELPFUL              = 0x0010,       // TARG_FRIENDS used
    SPFLAG_NOT_SELF             = 0x0020,       // aborts on isMe
    SPFLAG_UNHOLY               = 0x0040,       // counts at "unholy"
    SPFLAG_MAPPING              = 0x0080        // a mapping spell of some kind
};

enum spret_type
{
    SPRET_ABORT = 0,            // should be left as 0
    SPRET_FAIL,
    SPRET_SUCCESS
};

int list_spells();
int spell_fail( spell_type spell );
int calc_spell_power(spell_type spell, bool apply_intel,
                     bool fail_rate_chk = false );
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
spret_type your_spells( spell_type spell, int powc = 0,
                        bool allow_fail = true );

// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr - decks - fight - it_use2 - it_use3 - item_use - items -
 *              misc - mstuff2 - religion - spell - spl-book - spells4
 * *********************************************************************** */
void miscast_effect( unsigned int sp_type, int mag_pow, int mag_fail,
                     int force_effect, const char *cause = NULL );

const char* failure_rate_to_string( int fail );

int spell_power_colour(spell_type spell);
int spell_power_bars(spell_type spell);
std::string spell_power_string(spell_type spell);

const char* spell_hunger_string( spell_type spell );

#endif
