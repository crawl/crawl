/**
 * @file
 * @brief Functions used to print information about various game objects.
**/

#pragma once

#include <functional>
#include <sstream>
#include <string>

#include "command-type.h"
#include "enum.h"
#include "mon-util.h"
#include "trap-type.h"

struct monster_info;

// If you add any more description types, remember to also
// change item_description in externs.h
enum item_description_type
{
    IDESC_WANDS = 0,
    IDESC_POTIONS,
    IDESC_SCROLLS,                      // special field (like the others)
    IDESC_RINGS,
#if TAG_MAJOR_VERSION == 34
    IDESC_SCROLLS_II,
#endif
    IDESC_STAVES,
    NUM_IDESC
};

struct describe_info
{
    ostringstream body;
    string title;
    string prefix;
    string suffix;
    string footer;
    string quote;
};

bool is_dumpable_artefact(const item_def &item);

string get_item_description(const item_def &item, bool verbose,
                            bool dump = false, bool lookup = false);

void describe_feature_wide(const coord_def& pos);
void describe_feature_type(dungeon_feature_type feat);
string get_cloud_desc(cloud_type cloud, bool include_title = true);
void get_feature_desc(const coord_def &gc, describe_info &inf, bool include_extra = true);

bool describe_item(item_def &item, function<void (string&)> fixup_desc = nullptr);
void get_item_desc(const item_def &item, describe_info &inf);
void inscribe_item(item_def &item);
void target_item(item_def &item);

int describe_monsters(const monster_info &mi, bool force_seen = false,
                      const string &footer = "");

void get_monster_db_desc(const monster_info &mi, describe_info &inf,
                         bool &has_stat_desc, bool force_seen = false);
branch_type serpent_of_hell_branch(monster_type m);
string serpent_of_hell_flavour(monster_type m);

string player_spell_desc(spell_type spell);
void get_spell_desc(const spell_type spell, describe_info &inf);
void describe_spell(spell_type spelled,
                    const monster_info *mon_owner = nullptr,
                    const item_def* item = nullptr,
                    bool show_booklist = false);

void describe_ability(ability_type ability);

string short_ghost_description(const monster *mon, bool abbrev = false);
string get_ghost_description(const monster_info &mi, bool concise = false);

string get_skill_description(skill_type skill, bool need_title = false);

void describe_skill(skill_type skill);

int hex_chance(const spell_type spell, const int hd);

string get_command_description(const command_type cmd, bool terse);

int show_description(const string &body, const tile_def *tile = nullptr);
int show_description(const describe_info &inf, const tile_def *tile = nullptr);
string process_description(const describe_info &inf, bool include_title = true);

const char* get_size_adj(const size_type size, bool ignore_medium = false);

const char* jewellery_base_ability_string(int subtype);
string artefact_inscription(const item_def& item);
void add_inscription(item_def &item, string inscrip);

string trap_name(trap_type trap);
string full_trap_name(trap_type trap);
int str_to_trap(const string &s);

int count_desc_lines(const string& _desc, const int width);

string extra_cloud_info(cloud_type cloud_type);
