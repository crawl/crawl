/**
 * @file
 * @brief Functions related to special abilities.
**/

#pragma once

#include <string>
#include <vector>

#include "ability-type.h"
#include "enum.h"
#include "player.h"

struct talent
{
    ability_type which;
    int hotkey;
    int fail;
    bool is_invocation;
};

enum class abflag
{
    none                = 0x00000000,
    breath              = 0x00000001, // ability uses DUR_BREATH_WEAPON
    delay               = 0x00000002, // ability has its own delay
    torment             = 0x00000004, // torments the player
    quiet_fail          = 0x00000008, // no message on failure
    exhaustion          = 0x00000010, // fails if you.exhausted
    instant             = 0x00000020, // doesn't take time to use
    conf_ok             = 0x00000040, // can use even if confused
    variable_mp         = 0x00000080, // costs a variable amount of MP
    curse               = 0x00000100, // Destroys a cursed item
    max_hp_drain        = 0x00000200, // drains max hit points
    gold                = 0x00000400, // costs gold
    sacrifice           = 0x00000800, // sacrifice (Ru)
    injury              = 0x00001000, // costs a large proportion of player HP
    berserk_ok          = 0x00002000, // can use even if berserk
    card                = 0x00004000, // deck drawing (Nemelex)
    torchlight          = 0x00008000, // costs torchlight (Yred)
    drac_charges        = 0x00010000, // consumes draconian breath charges

    // targeting flags
    dir_or_target       = 0x10000000, // uses DIR_NONE targeting
    target              = 0x20000000, // uses DIR_TARGET targeting
    targeting_mask      = abflag::dir_or_target | abflag::target,
    not_self            = 0x40000000, // can't be self-targeted
};
DEF_BITFIELD(ability_flags, abflag);

class dist;

vector<ability_type> get_defined_abilities();
skill_type invo_skill(god_type god = you.religion);
int get_gold_cost(ability_type ability);
string nemelex_card_text(ability_type ability);
mutation_type makhleb_ability_to_mutation(ability_type abil);
const string make_cost_description(ability_type ability);
unsigned int ability_mp_cost(ability_type abil);
int ability_range(ability_type abil);
ability_flags get_ability_flags(ability_type ability);
talent get_talent(ability_type ability, bool check_confused);
string ability_name(ability_type ability, bool dbname = false);
vector<string> get_ability_names();
string get_ability_desc(const ability_type ability, bool need_title = true);
int choose_ability_menu(const vector<talent>& talents);
string describe_talent(const talent& tal);
skill_type abil_skill(ability_type abil);
int abil_skill_weight(ability_type abil);

void no_ability_msg();
bool activate_ability();
bool check_ability_possible(const ability_type ability, bool quiet = false);
bool ability_has_targeter(ability_type abil);
unique_ptr<targeter> find_ability_targeter(ability_type ability);
bool activate_talent(const talent& tal, dist *target = nullptr);
bool is_religious_ability(ability_type abil);
bool is_card_ability(ability_type abil);
bool player_has_ability(ability_type abil, bool include_unusable = false);
vector<talent> your_talents(bool check_confused, bool include_unusable = false,
                                        bool ignore_piety = false);
bool string_matches_ability_name(const string& key);
ability_type ability_by_name(const string &name);
string print_abilities();
ability_type fixup_ability(ability_type ability);

int find_ability_slot(ability_type abil, char firstletter = 'f');
int auto_assign_ability_slot(int slot);
vector<ability_type> get_god_abilities(bool ignore_silence = true,
                                       bool ignore_piety = true,
                                       bool ignore_penance = true);
void swap_ability_slots(int index1, int index2, bool silent = false);
