/*
 *  File:       spl-cast.h
 *  Summary:    Spell casting functions.
 *  Written by: Linley Henzell
 */


#ifndef SPL_CAST_H
#define SPL_CAST_H

#include "enum.h"

enum spflag_type
{
    SPFLAG_NONE                 = 0x000000,
    SPFLAG_DIR_OR_TARGET        = 0x000001,      // use DIR_NONE targeting
    SPFLAG_TARGET               = 0x000002,      // use DIR_TARGET targeting
    SPFLAG_GRID                 = 0x000004,      // use DIR_GRID targeting
    SPFLAG_DIR                  = 0x000008,      // use DIR_DIR targeting
    SPFLAG_TARG_OBJ             = 0x000010,      // use DIR_TARGET_OBJECT targ.
    SPFLAG_TARGETING_MASK       = 0x00001f,      // used to test for targeting
    SPFLAG_HELPFUL              = 0x000020,      // TARG_FRIENDS used
    SPFLAG_NEUTRAL              = 0x000040,      // TARG_ANY used
    SPFLAG_NOT_SELF             = 0x000080,      // aborts on isMe
    SPFLAG_UNHOLY               = 0x000100,      // counts as "unholy"
    SPFLAG_UNCLEAN              = 0x000200,      // counts as "unclean"
    SPFLAG_CHAOTIC              = 0x000400,      // counts as "chaotic"
    SPFLAG_HASTY                = 0x000800,      // counts as "hasty"
    SPFLAG_MAPPING              = 0x001000,      // a mapping spell of some kind
    SPFLAG_ESCAPE               = 0x002000,      // useful for running away
    SPFLAG_RECOVERY             = 0x004000,      // healing or recovery spell
    SPFLAG_AREA                 = 0x008000,      // area affect
    SPFLAG_BATTLE               = 0x010000,      // a non-Conjuration spell that
                                                 // is still a battle spell
    SPFLAG_CARD                 = 0x020000,      // a card effect spell
    SPFLAG_MONSTER              = 0x040000,      // monster-only spell
    SPFLAG_INNATE               = 0x080000,      // an innate spell, even if
                                                 // use by a priest/wizard
    SPFLAG_NOISY                = 0x100000,      // makes noise, even if innate
    SPFLAG_TESTING              = 0x200000,      // a testing/debugging spell
    SPFLAG_CORPSE_VIOLATING     = 0x400000,      // Conduct violation for Fedhas
    SPFLAG_ALLOW_SELF           = 0x800000,      // Not helpful, but may want to
                                                 // target self
};

enum spret_type
{
    SPRET_ABORT = 0,            // should be left as 0
    SPRET_FAIL,
    SPRET_SUCCESS,
    SPRET_NONE,                 // spell was not handled
};

typedef bool (*spell_selector)(spell_type spell);

int list_spells(bool toggle_with_I = true, bool viewing = false,
                int minRange = -1, spell_selector selector = NULL);
int spell_fail( spell_type spell );
int calc_spell_power(spell_type spell, bool apply_intel,
                     bool fail_rate_chk = false, bool cap_power = true,
                     bool rod = false);
int calc_spell_range(spell_type spell, int power = 0,
                     bool real_cast = false, bool rod = false);
int spell_enhancement( unsigned int typeflags );

bool cast_a_spell( bool check_range, spell_type spell = SPELL_NO_SPELL );

bool maybe_identify_staff( item_def &item, spell_type spell = SPELL_NO_SPELL );

void inspect_spells();
void do_cast_spell_cmd(bool force);

spret_type your_spells(spell_type spell, int powc = 0, bool allow_fail = true,
                       bool check_range = true, int range_power = 0);

const char* failure_rate_to_string( int fail );

int spell_power_colour(spell_type spell);
int spell_power_bars(spell_type spell, bool rod);
std::string spell_power_string(spell_type spell, bool rod = false);
std::string spell_range_string(spell_type spell, bool rod = false);
std::string spell_schools_string(spell_type spell);
const char* spell_hunger_string(spell_type spell, bool rod = false);
std::string spell_noise_string(spell_type spell);

bool is_prevented_teleport(spell_type spell);

bool spell_is_uncastable(spell_type spell, std::string &message);

#endif
