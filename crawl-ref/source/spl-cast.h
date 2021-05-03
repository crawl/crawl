/**
 * @file
 * @brief Spell casting functions.
**/

#pragma once

#include <vector>
#include <memory>

#include "enum.h"
#include "item-def.h"
#include "skill-type.h"
#include "spell-type.h"

using std::vector;

struct monster_info;
class dist;

enum class spflag
{
    none               = 0x00000000,
    dir_or_target      = 0x00000001,      // use DIR_NONE targeting
    target             = 0x00000002,      // use DIR_TARGET targeting
                     //  0x00000004,
                     //  0x00000008,
                                          // used to test for targeting
    targeting_mask     = spflag::dir_or_target | spflag::target,
    obj                = 0x00000010,      // TARG_MOVABLE_OBJECT used
    helpful            = 0x00000020,      // TARG_FRIEND used
    neutral            = 0x00000040,      // TARG_ANY used
    not_self           = 0x00000080,      // aborts on isMe
    unholy             = 0x00000100,      // counts as "unholy"
    unclean            = 0x00000200,      // counts as "unclean"
    chaotic            = 0x00000400,      // counts as "chaotic"
    hasty              = 0x00000800,      // counts as "hasty"
    emergency          = 0x00001000,      // monsters use in emergencies
    escape             = 0x00002000,      // useful for running away
    recovery           = 0x00004000,      // healing or recovery spell
    area               = 0x00008000,      // area affect
                     //  0x00010000,      // was SPFLAG_BATTLE
    selfench           = 0x00020000,      // monsters use as selfench
    monster            = 0x00040000,      // monster-only spell
    needs_tracer       = 0x00080000,      // monster casting needs tracer
    noisy              = 0x00100000,      // makes noise, even if innate
    testing            = 0x00200000,      // a testing/debugging spell
                     //  0x00400000,      // was spflag::corpse_violating
                     //  0x00800000,      // was SPFLAG_ALLOW_SELF
    utility            = 0x01000000,      // usable no matter what foe is
    no_ghost           = 0x02000000,      // ghosts can't get this spell
    cloud              = 0x04000000,      // makes a cloud
    WL_check           = 0x08000000,      // spell that checks monster WL
    mons_abjure        = 0x10000000,      // monsters can cast abjuration
                                          // instead of this spell
    not_evil           = 0x20000000,      // not considered evil by the
                                          // good gods
    holy               = 0x40000000,      // considered holy (can't be
                                          // used by Yred enslaved souls)
};
DEF_BITFIELD(spell_flags, spflag);

enum class spret
{
    abort = 0,            // should be left as 0
    fail,
    success,
    none,                 // spell was not handled
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

#define INNATE_SPELLS_KEY "innate_spells"

#define fail_check() if (fail) return spret::fail

void surge_power(const int enhanced);
void surge_power_wand(const int mp_cost);

typedef bool (*spell_selector)(spell_type spell);

int list_spells(bool toggle_with_I = true, bool viewing = false,
                bool allow_preselect = true,
                const string &title = "Your Spells",
                spell_selector selector = nullptr);
int raw_spell_fail(spell_type spell);
int stepdown_spellpower(int power, int scale = 1);
int calc_spell_power(spell_type spell, bool apply_intel,
                     bool fail_rate_chk = false, bool cap_power = true,
                     int scale = 1);
int calc_spell_range(spell_type spell, int power = 0, bool allow_bonus = true,
                     bool ignore_shadows = false);

bool cast_a_spell(bool check_range, spell_type spell = SPELL_NO_SPELL, dist *_target = nullptr);

int apply_enhancement(const int initial_power, const int enhancer_levels);

void inspect_spells();
bool can_cast_spells(bool quiet = false, bool exegesis = false);
void do_cast_spell_cmd(bool force);

int hex_success_chance(const int mr, int powc, int scale,
                       bool round_up = false);
class targeter;
unique_ptr<targeter> find_spell_targeter(spell_type spell, int pow, int range);
bool spell_has_targeter(spell_type spell);
vector<string> desc_wl_success_chance(const monster_info& mi, int pow,
                                      bool evoked, targeter* hitfunc);
spret your_spells(spell_type spell, int powc = 0, bool allow_fail = true,
                  const item_def* const evoked_item = nullptr,
                  dist *_target = nullptr);

extern const char *fail_severity_adjs[];

int max_miscast_damage(spell_type spell);
int fail_severity(spell_type spell);
int failure_rate_colour(spell_type spell);
int failure_rate_to_int(int fail);
string failure_rate_to_string(int fail);

int power_to_barcount(int power);

int spell_power_percent(spell_type spell);
string spell_power_string(spell_type spell);
string spell_damage_string(spell_type spell, bool evoked = false);
int spell_acc(spell_type spell);
string spell_range_string(spell_type spell);
string range_string(int range, int maxrange, char32_t caster_char);
string spell_schools_string(spell_type spell);
string spell_failure_rate_string(spell_type spell);
string spell_noise_string(spell_type spell, int chop_wiz_display_width = 0);

void spell_skills(spell_type spell, set<skill_type> &skills);
void do_demonic_magic(int pow, int rank);
