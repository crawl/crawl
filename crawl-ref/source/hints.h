/**
 * @file
 * @brief Stuff needed for hints mode
**/

#ifndef HINTS_H
#define HINTS_H

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

class formatted_string;
class writer;
class reader;

struct monster_info;

enum hints_types
{
    HINT_BERSERK_CHAR,
    HINT_MAGIC_CHAR,
    HINT_RANGER_CHAR,
    HINT_TYPES_NUM   // 3
};

void save_hints(writer& outf);
void load_hints(reader& inf);
void init_hints_options();

enum hints_event_type
{
    HINT_SEEN_FIRST_OBJECT,
    // seen certain items
    HINT_SEEN_POTION,
    HINT_SEEN_SCROLL,
    HINT_SEEN_WAND,
    HINT_SEEN_SPBOOK,
    HINT_SEEN_JEWELLERY,
    HINT_SEEN_MISC,
    HINT_SEEN_STAFF,
    HINT_SEEN_WEAPON,
    HINT_SEEN_MISSILES,
    HINT_SEEN_ARMOUR,
    HINT_SEEN_RANDART,
    HINT_SEEN_FOOD,
    HINT_SEEN_CARRION,
    HINT_SEEN_GOLD,
    // encountered dungeon features
    HINT_SEEN_STAIRS,
    HINT_SEEN_ESCAPE_HATCH,
    HINT_SEEN_BRANCH,
    HINT_SEEN_PORTAL,
    HINT_SEEN_TRAP,
    HINT_SEEN_ALTAR,
    HINT_SEEN_SHOP,
    HINT_SEEN_DOOR,
    HINT_SEEN_RUNED_DOOR,
    // other 'first events'
    HINT_SEEN_MONSTER,
    HINT_SEEN_ZERO_EXP_MON,
#if TAG_MAJOR_VERSION == 34
    HINT_SEEN_TOADSTOOL,
#endif
    HINT_MONSTER_BRAND,
    HINT_MONSTER_FRIENDLY,
    HINT_MONSTER_SHOUT,
    HINT_MONSTER_LEFT_LOS,
    HINT_KILLED_MONSTER,
    HINT_NEW_LEVEL,
    HINT_SKILL_RAISE,
    HINT_GAINED_MAGICAL_SKILL,
    HINT_GAINED_MELEE_SKILL,
    HINT_GAINED_RANGED_SKILL,
    HINT_CHOOSE_STAT,
    HINT_MAKE_CHUNKS,
    HINT_OFFER_CORPSE,
    HINT_NEW_ABILITY_GOD,
    HINT_NEW_ABILITY_MUT,
    HINT_NEW_ABILITY_ITEM,
    HINT_FLEEING_MONSTER,
    HINT_ROTTEN_FOOD,
    HINT_CONVERT,
    HINT_GOD_DISPLEASED,
    HINT_EXCOMMUNICATE,
    HINT_SPELL_MISCAST,
    HINT_SPELL_HUNGER,
    HINT_GLOWING,
    HINT_YOU_RESIST,
    // status changes
    HINT_YOU_ENCHANTED,
#if TAG_MAJOR_VERSION == 34
    HINT_CONTAMINATED_CHUNK,
#endif
    HINT_YOU_SICK,
    HINT_YOU_POISON,
    HINT_YOU_ROTTING,
    HINT_YOU_CURSED,
    HINT_YOU_HUNGRY,
    HINT_YOU_STARVING,
    HINT_YOU_MUTATED,
    HINT_CAN_BERSERK,
    HINT_POSTBERSERK,
    HINT_CAUGHT_IN_NET,
    HINT_YOU_SILENCE,
    // warning
    HINT_RUN_AWAY,
    HINT_RETREAT_CASTER,
    HINT_WIELD_WEAPON,
    HINT_NEED_HEALING,
    HINT_NEED_POISON_HEALING,
    HINT_INVISIBLE_DANGER,
    HINT_NEED_HEALING_INVIS,
    HINT_ABYSS,
    // interface
    HINT_MULTI_PICKUP,
    HINT_FULL_INVENTORY,
    HINT_SHIFT_RUN,
    HINT_MAP_VIEW,
    HINT_AUTO_EXPLORE,
    HINT_DONE_EXPLORE,
    HINT_AUTO_EXCLUSION,
    HINT_STAIR_BRAND,
    HINT_HEAP_BRAND,
    HINT_TRAP_BRAND,
    HINT_LOAD_SAVED_GAME,
    // for the tutorial
    HINT_AUTOPICKUP_THROWN,
    HINT_TARGET_NO_FOE,
    HINT_REMOVED_CURSE,
    HINT_ITEM_RESISTANCES,
    HINT_FLYING,
    HINT_INACCURACY,
    HINT_HEALING_POTIONS,
    HINT_GAINED_SPELLCASTING,
    HINT_FUMBLING_SHALLOW_WATER,
    HINT_EATING_ROTTEN_FOOD,
    HINT_CLOUD_WARNING,
#if TAG_MAJOR_VERSION == 34
    HINT_MEMORISE_FAILURE,
#endif
    HINT_ANIMATE_CORPSE_SKELETON,
    HINT_SEEN_WEB,
    HINT_SEEN_ROD,
    HINT_EVENTS_NUM
};

struct newgame_def;
void init_hints();
void tutorial_init_hints();
void pick_hints(newgame_def* choice);
void hints_load_game();

void hints_starting_screen();
void hints_new_turn();
void print_hint(string key, const string arg1 = "", const string arg2 = "");
void hints_death_screen();
void hints_finished();

void hints_dissection_reminder(bool healthy);
void hints_healing_check();

void taken_new_item(object_class_type item_type);
void hints_gained_new_skill(skill_type skill);
void hints_monster_seen(const monster& mon);
void hints_first_item(const item_def& item);
void learned_something_new(hints_event_type seen_what,
                           coord_def gc = coord_def());
formatted_string hints_abilities_info();
string hints_skills_info();
string hints_skill_training_info();
string hints_skills_description_info();

// Additional information for tutorial players.
void check_item_hint(const item_def &item, unsigned int num_old_talents);
void hints_describe_item(const item_def &item);
void hints_inscription_info(string prompt);
bool hints_pos_interesting(int x, int y);
void hints_describe_pos(int x, int y);
bool hints_monster_interesting(const monster* mons);
void hints_describe_monster(const monster_info& mi, bool has_stat_desc);

void hints_observe_cell(const coord_def& gc);

struct hints_state
{
    FixedVector<bool, HINT_EVENTS_NUM> hints_events;
    bool hints_explored;
    bool hints_stashes;
    bool hints_travel;
    unsigned int hints_spell_counter;
    unsigned int hints_throw_counter;
    unsigned int hints_berserk_counter;
    unsigned int hints_melee_counter;
    unsigned int hints_last_healed;
    unsigned int hints_seen_invisible;

    bool hints_just_triggered;
    unsigned int hints_type;
};

extern hints_state Hints;

void tutorial_msg(const char *text, bool end = false);
#endif
