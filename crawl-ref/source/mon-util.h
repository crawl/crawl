/*
 *  File:       mon-util.h
 *  Summary:    Misc monster related functions.
 *  Written by: Linley Henzell
 */


#ifndef MONUTIL_H
#define MONUTIL_H

#include "mon-util.h"

#include "externs.h"
#include "enum.h"
#include "mon-enum.h"
#include "mon_resist_def.h"
#include "player.h"
#include "monster.h"

struct bolt;

// ($pellbinder) (c) D.G.S.E. 1998

struct mon_attack_def
{
    mon_attack_type     type;
    mon_attack_flavour  flavour;
    int                 damage;

    static mon_attack_def attk(int dam,
                               mon_attack_type typ = AT_HIT,
                               mon_attack_flavour flav = AF_PLAIN)
    {
        mon_attack_def def = { typ, flav, dam };
        return (def);
    }
};

// Amount of mons->speed_increment used by different actions; defaults
// to 10.
struct mon_energy_usage
{
public:
    int8_t move;
    int8_t swim;
    int8_t attack;
    int8_t missile; // Arrows/crossbows/etc
    int8_t spell;
    int8_t special;
    int8_t item;    // Using an item (i.e., drinking a potion)

    // Percent of mons->speed used when picking up an item; defaults
    // to 100%
    int8_t pickup_percent;

public:
    mon_energy_usage(int mv = 10, int sw = 10, int att = 10, int miss = 10,
                     int spl = 10, int spc = 10, int itm = 10, int pick = 100)
        : move(mv), swim(sw), attack(att), missile(miss),
          spell(spl), special(spc), item(itm), pickup_percent(pick)
    {
    }

    static mon_energy_usage attack_cost(int cost, int sw = 10)
    {
        mon_energy_usage me;
        me.attack = cost;
        me.swim = sw;
        return me;
    }

    static mon_energy_usage missile_cost(int cost)
    {
        mon_energy_usage me;
        me.missile = cost;
        return me;
    }

    static mon_energy_usage swim_cost (int cost)
    {
        mon_energy_usage me;
        me.swim = cost;
        return me;
    }

    static mon_energy_usage move_cost(int mv, int sw = 10)
    {
        const mon_energy_usage me(mv, sw);
        return me;
    }

    mon_energy_usage operator | (const mon_energy_usage &o) const
    {
        return mon_energy_usage(combine(move, o.move),
                                 combine(swim, o.swim),
                                 combine(attack, o.attack),
                                 combine(missile, o.missile),
                                 combine(spell, o.spell),
                                 combine(special, o.special),
                                 combine(item, o.item),
                                 combine(pickup_percent, o.pickup_percent,
                                         100));
    }
private:
    int8_t combine(int8_t a, int8_t b, int8_t def = 10) const {
        return (b != def? b : a);
    }
};

struct monsterentry
{
    short mc;            // monster number

    char showchar;
    uint8_t colour;
    const char *name;

    uint64_t bitfields;
    mon_resist_def resists;

    short weight;
    // [Obsolete] Experience used to be calculated like this:
    // ((((max_hp / 7) + 1) * (mHD * mHD) + 1) * exp_mod) / 10
    //     ^^^^^^ see below at hpdice
    //   Note that this may make draining attacks less attractive (LRH)
    int8_t exp_mod;

    monster_type genus,         // "team" the monster plays for
                 species;       // corpse type of the monster

    mon_holy_type holiness;

    short resist_magic;  // (positive is ??)
    // max damage in a turn is total of these four?

    mon_attack_def attack[4];

    // hpdice[4]: [0]=HD [1]=min_hp [2]=rand_hp [3]=add_hp
    // min hp = [0]*[1]+[3] & max hp = [0]*([1]+[2])+[3])
    // example: the Iron Golem, hpdice={15,7,4,0}
    //      15*7 < hp < 15*(7+4),
    //       105 < hp < 165
    // hp will be around 135 each time.
    unsigned       hpdice[4];

    int8_t AC; // armour class
    int8_t ev; // evasion
    mon_spellbook_type sec;
    corpse_effect_type corpse_thingy;
    zombie_size_type   zombie_size;
    shout_type         shouts;
    mon_intel_type     intel;
    habitat_type     habitat;
    flight_type      fly;
    int8_t           speed;        // How quickly speed_increment increases
    mon_energy_usage energy_usage; // And how quickly it decreases
    mon_itemuse_type gmon_use;
    mon_itemeat_type gmon_eat;
    size_type size;
};

habitat_type grid2habitat(dungeon_feature_type grid);
dungeon_feature_type habitat2grid(habitat_type ht);

monsterentry *get_monster_data(int mc);
const mon_resist_def &get_mons_class_resists(int mc);
mon_resist_def get_mons_resists(const monster* mon);

void init_monsters();
void init_monster_symbols();

monster *monster_at(const coord_def &pos);

// this is the old moname()
std::string mons_type_name(int type, description_level_type desc);

bool give_monster_proper_name(monster* mon, bool orcs_only = true);

flight_type mons_class_flies(int mc);
flight_type mons_flies(const monster* mon, bool randarts = true);

bool mons_class_amphibious(int mc);
bool mons_class_flattens_trees(int mc);
bool mons_amphibious(const monster* mon);
bool mons_flattens_trees(const monster* mon);
bool mons_class_wall_shielded(int mc);
bool mons_wall_shielded(const monster* mon);

mon_itemuse_type mons_class_itemuse(int mc);
mon_itemuse_type mons_itemuse(const monster* mon);
mon_itemeat_type mons_class_itemeat(int mc);
mon_itemeat_type mons_itemeat(const monster* mon);

bool mons_sense_invis(const monster* mon);

int get_shout_noise_level(const shout_type shout);
shout_type mons_shouts(int mclass, bool demon_shout = false);

bool mons_is_ghost_demon(int mc);
bool mons_is_unique(int mc);
bool mons_is_pghost(int mc);

int mons_difficulty(int mc);
int exper_value(const monster* mon);

int hit_points(int hit_dice, int min_hp, int rand_hp);

int mons_class_hit_dice(int mc);

bool mons_immune_magic(const monster* mon);
const char* mons_resist_string(const monster* mon);

int mons_damage(int mc, int rt);
mon_attack_def mons_attack_spec(const monster* mon, int attk_number);

corpse_effect_type mons_corpse_effect(int mc);

bool mons_class_flag(int mc, uint64_t bf);

int mons_unusable_items(const monster* mon);

mon_holy_type mons_class_holiness(int mc);

bool mons_is_mimic(int mc);
bool mons_is_item_mimic(int mc);
bool mons_is_feat_mimic(int mc);


bool mons_is_statue(int mc, bool allow_disintegrate = false);
bool mons_is_demon(int mc);
bool mons_is_draconian(int mc);
bool mons_is_conjured(int mc);

bool mons_class_wields_two_weapons(int mc);
bool mons_wields_two_weapons(const monster* m);
bool mons_self_destructs(const monster* m);

mon_intel_type mons_class_intel(int mc);
mon_intel_type mons_intel(const monster* mon);

// Use mons_habitat() and mons_primary_habitat() wherever possible,
// since the class variants do not handle zombies correctly.
habitat_type mons_class_habitat(int mc);
habitat_type mons_habitat(const monster* mon);
habitat_type mons_class_primary_habitat(int mc);
habitat_type mons_primary_habitat(const monster* mon);
habitat_type mons_class_secondary_habitat(int mc);
habitat_type mons_secondary_habitat(const monster* mon);

bool intelligent_ally(const monster* mon);

bool mons_skeleton(int mc);

int mons_weight(int mc);
mon_resist_def serpent_of_hell_resists(int flavour);

int mons_class_base_speed(int mc);
int mons_class_zombie_base_speed(int zombie_base_mc);
int mons_base_speed(const monster* mon);
int mons_real_base_speed(int mc);

bool mons_class_can_regenerate(int mc);
bool mons_can_regenerate(const monster* mon);
bool mons_class_can_display_wounds(int mc);
bool mons_can_display_wounds(const monster* mon);
zombie_size_type zombie_class_size(monster_type cs);
int mons_zombie_size(int mc);
monster_type mons_zombie_base(const monster* mon);
bool mons_class_is_zombified(int mc);
monster_type mons_base_type(const monster* mon);
bool mons_class_can_leave_corpse(monster_type mc);
bool mons_is_zombified(const monster* mons);
bool mons_class_can_be_zombified(int mc);
bool mons_can_be_zombified(const monster* mon);
bool mons_class_can_use_stairs(int mc);
bool mons_can_use_stairs(const monster* mon);
bool mons_enslaved_body_and_soul(const monster* mon);
bool mons_enslaved_soul(const monster* mon);
bool name_zombie(monster* mon, int mc, const std::string mon_name);
bool name_zombie(monster* mon, const monster* orig);

int mons_power(int mc);

wchar_t mons_char(int mc);
char mons_base_char(int mc);

int mons_class_colour(int mc);
int mons_colour(const monster* mon);

void mons_load_spells(monster* mon, mon_spellbook_type book);

monster_type royal_jelly_ejectable_monster();
monster_type random_draconian_monster_species();

void define_monster(monster* mons);

void mons_pacify(monster* mon, mon_attitude_type att = ATT_GOOD_NEUTRAL);

bool mons_should_fire(struct bolt &beam);

bool ms_direct_nasty(spell_type monspell);

bool ms_useful_fleeing_out_of_sight(const monster* mon, spell_type monspell);
bool ms_quick_get_away(const monster* mon, spell_type monspell);
bool ms_waste_of_time(const monster* mon, spell_type monspell);
bool ms_low_hitpoint_cast(const monster* mon, spell_type monspell);

bool mons_has_los_ability(monster_type mon_type);
bool mons_has_los_attack(const monster* mon);
bool mons_has_ranged_spell(const monster* mon, bool attack_only = false,
                           bool ench_too = true);
bool mons_has_ranged_attack(const monster* mon);
bool mons_has_ranged_ability(const monster* mon);

const char *mons_pronoun(monster_type mon_type, pronoun_type variant,
                         bool visible = true);

bool mons_aligned(const actor *m1, const actor *m2);
bool mons_atts_aligned(mon_attitude_type fr1, mon_attitude_type fr2);

bool mons_att_wont_attack(mon_attitude_type fr);
mon_attitude_type mons_attitude(const monster* m);

bool mons_foe_is_mons(const monster* mons);

bool mons_behaviour_perceptible(const monster* mon);
bool mons_is_native_in_branch(const monster* mons,
                              const branch_type branch = you.where_are_you);
bool mons_is_poisoner(const monster* mon);

// Whether the monster is temporarily confused (class_too = false)
// or confused at all (class_too = true; temporarily or by class).
bool mons_is_confused(const monster* m, bool class_too = false);

bool mons_is_wandering(const monster* m);
bool mons_is_seeking(const monster* m);
bool mons_is_fleeing(const monster* m);
bool mons_is_panicking(const monster* m);
bool mons_is_cornered(const monster* m);
bool mons_is_lurking(const monster* m);
bool mons_is_batty(const monster* m);
bool mons_is_influenced_by_sanctuary(const monster* m);
bool mons_is_fleeing_sanctuary(const monster* m);
bool mons_was_seen(const monster* m);
bool mons_is_known_mimic(const monster* m);
bool mons_is_unknown_mimic(const monster* m);
bool mons_is_skeletal(int mc);
bool mons_class_is_slime(int mc);
bool mons_is_slime(const monster* mon);
bool mons_class_is_plant(int mc);
bool mons_is_plant(const monster* mon);
bool mons_eats_items(const monster* mon);
bool mons_eats_corpses(const monster* mon);
bool mons_eats_food(const monster* mon);
bool mons_has_lifeforce(const monster* mon);
monster_type mons_genus(int mc);
monster_type mons_detected_base(monster_type mt);
monster_type mons_species(int mc);

bool mons_looks_stabbable(const monster* m);
bool mons_looks_distracted(const monster* m);

void mons_start_fleeing_from_sanctuary(monster* mons);
void mons_stop_fleeing_from_sanctuary(monster* mons);

bool mons_landlubbers_in_reach(const monster* mons);

bool mons_class_is_confusable(int mc);
bool mons_class_is_slowable(int mc);
bool mons_class_is_stationary(int mc);
bool mons_is_stationary(const monster* mon);
bool mons_class_is_firewood(int mc);
bool mons_is_firewood(const monster* mon);
bool mons_has_body(const monster* mon);

int cheibriados_monster_player_speed_delta(const monster* mon);
bool cheibriados_thinks_mons_is_fast(const monster* mon);
bool mons_is_projectile(int mc);
bool mons_has_blood(int mc);

bool invalid_monster(const monster* mon);
bool invalid_monster_type(monster_type mt);
bool invalid_monster_index(int i);

void mons_remove_from_grid(const monster* mon);

bool monster_shover(const monster* m);

bool monster_senior(const monster* first, const monster* second,
                    bool fleeing = false);
monster_type draco_subspecies(const monster* mon);
std::string ugly_thing_colour_name(uint8_t colour);
std::string ugly_thing_colour_name(const monster* mon);
uint8_t ugly_thing_random_colour();
int str_to_ugly_thing_colour(const std::string &s);
uint8_t random_monster_colour();
uint8_t random_large_abomination_colour();
uint8_t random_small_abomination_colour();
int ugly_thing_colour_offset(const uint8_t colour);
std::string  draconian_colour_name(monster_type mon_type);
monster_type draconian_colour_by_name(const std::string &colour);

monster_type random_monster_at_grid(const coord_def& p);
monster_type random_monster_at_grid(dungeon_feature_type grid);

void         init_mon_name_cache();
monster_type get_monster_by_name(std::string name, bool exact = false);

std::string do_mon_str_replacements(const std::string &msg,
                                    const monster* mons, int s_type = -1);

mon_body_shape get_mon_shape(const monster* mon);
mon_body_shape get_mon_shape(const int type);

std::string get_mon_shape_str(const monster* mon);
std::string get_mon_shape_str(const int type);
std::string get_mon_shape_str(const mon_body_shape shape);

bool mons_class_can_pass(int mc, const dungeon_feature_type grid);
bool mons_can_open_door(const monster* mon, const coord_def& pos);
bool mons_can_eat_door(const monster* mon, const coord_def& pos);
bool mons_can_traverse(const monster* mon, const coord_def& pos,
                       bool checktraps = true);

mon_inv_type equip_slot_to_mslot(equipment_type eq);
mon_inv_type item_to_mslot(const item_def &item);

int scan_mon_inv_randarts(const monster* mon,
                          artefact_prop_type ra_prop);

bool player_or_mon_in_sanct(const monster* mons);

int get_dist_to_nearest_monster();

bool mons_is_tentacle(int mc);

#endif
