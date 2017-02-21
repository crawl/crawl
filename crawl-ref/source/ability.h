/**
 * @file
 * @brief Functions related to special abilities.
**/

#ifndef ABLSHOW_H
#define ABLSHOW_H

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

skill_type invo_skill(god_type god = you.religion);
int get_gold_cost(ability_type ability);
const string make_cost_description(ability_type ability);
unsigned int ability_mp_cost(ability_type abil);
talent get_talent(ability_type ability, bool check_confused);
const char* ability_name(ability_type ability);
vector<const char*> get_ability_names();
string get_ability_desc(const ability_type ability);
int choose_ability_menu(const vector<talent>& talents);
string describe_talent(const talent& tal);
skill_type abil_skill(ability_type abil);
int abil_skill_weight(ability_type abil);

void no_ability_msg();
bool activate_ability();
bool check_ability_possible(const ability_type ability, bool hungerCheck = true,
                            bool quiet = false);
bool activate_talent(const talent& tal);
vector<talent> your_talents(bool check_confused, bool include_unusable = false);
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

#endif
