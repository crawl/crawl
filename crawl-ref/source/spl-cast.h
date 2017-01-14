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
                                //0x000004,
    SPFLAG_DIR                  = 0x000008,      // use DIR_DIR targeting
    SPFLAG_TARGETING_MASK       = SPFLAG_DIR_OR_TARGET | SPFLAG_TARGET
                                   | SPFLAG_DIR , // used to test for targeting
    // TODO: we need a new flag if we want to target corpses too.
    SPFLAG_OBJ                  = 0x000010,      // TARG_MOVABLE_OBJECT used
    SPFLAG_HELPFUL              = 0x000020,      // TARG_FRIEND used
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
                            //  = 0x010000,      // was SPFLAG_BATTLE
    SPFLAG_SELFENCH             = 0x020000,      // monsters use as selfench
    SPFLAG_MONSTER              = 0x040000,      // monster-only spell
    SPFLAG_NEEDS_TRACER         = 0x080000,      // monster casting needs tracer
    SPFLAG_NOISY                = 0x100000,      // makes noise, even if innate
    SPFLAG_TESTING              = 0x200000,      // a testing/debugging spell
    SPFLAG_CORPSE_VIOLATING     = 0x400000,      // Conduct violation for Fedhas
                               // 0x800000,      // was SPFLAG_ALLOW_SELF
    SPFLAG_UTILITY             = 0x1000000,      // usable no matter what foe is
    SPFLAG_NO_GHOST            = 0x2000000,      // ghosts can't get this spell
    SPFLAG_CLOUD               = 0x4000000,      // makes a cloud
    SPFLAG_MR_CHECK            = 0x8000000,      // spell that checks monster MR
    SPFLAG_MONS_ABJURE        = 0x10000000,      // monsters can cast abjuration
                                                 // instead of this spell
    SPFLAG_NOT_EVIL           = 0x20000000,      // not considered evil by the
                                                 // good gods
    SPFLAG_HOLY               = 0x40000000,      // considered holy (can't be
                                                 // used by Yred enslaved souls)
};

enum spret_type
{
    SPRET_ABORT = 0,            // should be left as 0
    SPRET_FAIL,
    SPRET_SUCCESS,
    SPRET_NONE,                 // spell was not handled
};

#define IOOD_X "iood_x"
#define IOOD_Y "iood_y"
#define IOOD_VX "iood_vx"
#define IOOD_VY "iood_vy"
#define IOOD_KC "iood_kc"
#define IOOD_POW "iood_pow"
#define IOOD_CASTER "iood_caster"
#define IOOD_REFLECTOR "iood_reflector"
#define IOOD_DIST "iood_distance"
#define IOOD_MID "iood_mid"
#define IOOD_FLAWED "iood_flawed"
#define IOOD_TPOS "iood_tpos"

#define fail_check() if (fail) return SPRET_FAIL

void surge_power(const int enhanced);

typedef bool (*spell_selector)(spell_type spell);

int list_spells(bool toggle_with_I = true, bool viewing = false,
                bool allow_preselect = true,
                const string &title = "Your Spells",
                spell_selector selector = nullptr);
int raw_spell_fail(spell_type spell);
int stepdown_spellpower(int power);
int calc_spell_power(spell_type spell, bool apply_intel,
                     bool fail_rate_chk = false, bool cap_power = true);
int calc_spell_range(spell_type spell, int power = 0);

bool cast_a_spell(bool check_range, spell_type spell = SPELL_NO_SPELL);

int apply_enhancement(const int initial_power, const int enhancer_levels);

void inspect_spells();
bool can_cast_spells(bool quiet = false);
void do_cast_spell_cmd(bool force);

int hex_success_chance(const int mr, int powc, int scale,
                       bool round_up = false);
class targeter;
vector<string> desc_success_chance(const monster_info& mi, int pow, bool evoked,
                                   targeter* hitfunc);
spret_type your_spells(spell_type spell, int powc = 0, bool allow_fail = true,
                       const item_def* const evoked_item = nullptr);

extern const char *fail_severity_adjs[];

double get_miscast_chance(spell_type spell, int severity = 2);
int fail_severity(spell_type spell);
int failure_rate_colour(spell_type spell);
int failure_rate_to_int(int fail);
string failure_rate_to_string(int fail);

int power_to_barcount(int power);

string spell_power_string(spell_type spell);
string spell_range_string(spell_type spell);
string range_string(int range, int maxrange, char32_t caster_char);
string spell_schools_string(spell_type spell);
string spell_hunger_string(spell_type spell);
string spell_noise_string(spell_type spell, int chop_wiz_display_width = 0);

void spell_skills(spell_type spell, set<skill_type> &skills);

bool spell_removed(spell_type spell);

#endif
