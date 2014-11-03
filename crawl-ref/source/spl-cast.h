/**
 * @file
 * @brief Spell casting functions.
**/

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
    // TODO: we need a new targetting flag if we want to target corpses too.
    SPFLAG_TARG_OBJ             = 0x000010,      // use DIR_MOVABLE_OBJECT targ.
    SPFLAG_TARGETING_MASK       = 0x00001f,      // used to test for targeting
    SPFLAG_HELPFUL              = 0x000020,      // TARG_FRIENDS used
    SPFLAG_NEUTRAL              = 0x000040,      // TARG_ANY used
    SPFLAG_NOT_SELF             = 0x000080,      // aborts on isMe
    SPFLAG_UNHOLY               = 0x000100,      // counts as "unholy"
    SPFLAG_UNCLEAN              = 0x000200,      // counts as "unclean"
    SPFLAG_CHAOTIC              = 0x000400,      // counts as "chaotic"
    SPFLAG_HASTY                = 0x000800,      // counts as "hasty"
    SPFLAG_EMERGENCY            = 0x001000,      // monsters use in emergencies
    SPFLAG_ESCAPE               = 0x002000,      // useful for running away
    SPFLAG_RECOVERY             = 0x004000,      // healing or recovery spell
    SPFLAG_AREA                 = 0x008000,      // area affect
    SPFLAG_BATTLE               = 0x010000,      // a non-Conjuration spell that
                                                 // is still a battle spell
    SPFLAG_SELFENCH             = 0x020000,      // monsters use as selfench
    SPFLAG_MONSTER              = 0x040000,      // monster-only spell
    SPFLAG_NEEDS_TRACER         = 0x080000,      // monster casting needs tracer
    SPFLAG_NOISY                = 0x100000,      // makes noise, even if innate
    SPFLAG_TESTING              = 0x200000,      // a testing/debugging spell
    SPFLAG_CORPSE_VIOLATING     = 0x400000,      // Conduct violation for Fedhas
    SPFLAG_ALLOW_SELF           = 0x800000,      // Not helpful, but may want to
                                                 // target self
    SPFLAG_UTILITY             = 0x1000000,      // usable no matter what foe is
    SPFLAG_NO_GHOST            = 0x2000000,      // ghosts can't get this spell
};

enum spret_type
{
    SPRET_ABORT = 0,            // should be left as 0
    SPRET_FAIL,
    SPRET_SUCCESS,
    SPRET_NONE,                 // spell was not handled
};

#define fail_check() if (fail) return SPRET_FAIL

typedef bool (*spell_selector)(spell_type spell);

int list_spells(bool toggle_with_I = true, bool viewing = false,
                bool allow_preselect = true,
                const string &title = "Your Spells",
                spell_selector selector = NULL);
int spell_fail(spell_type spell);
int calc_spell_power(spell_type spell, bool apply_intel,
                     bool fail_rate_chk = false, bool cap_power = true,
                     bool rod = false);
int calc_spell_range(spell_type spell, int power = 0, bool rod = false);

bool cast_a_spell(bool check_range, spell_type spell = SPELL_NO_SPELL);

void inspect_spells();
void do_cast_spell_cmd(bool force);

spret_type your_spells(spell_type spell, int powc = 0, bool allow_fail = true,
    bool evoked = true);

double get_miscast_chance(spell_type spell, int severity = 2);
int fail_severity(spell_type spell);
int failure_rate_colour(spell_type spell);
int failure_rate_to_int(int fail);
char* failure_rate_to_string(int fail);

int power_to_barcount(int power);

string spell_power_string(spell_type spell, bool rod = false);
string spell_range_string(spell_type spell, bool rod = false);
string spell_schools_string(spell_type spell);
string spell_hunger_string(spell_type spell, bool rod = false);
string spell_noise_string(spell_type spell);

bool spell_is_uncastable(spell_type spell, string &message, bool temp = true,
                         bool evoked = false);
void spell_skills(spell_type spell, set<skill_type> &skills);

#endif
