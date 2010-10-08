/*
 *  File:       effects.h
 *  Summary:    Misc stuff.
 *  Written by: Linley Henzell
 */


#ifndef EFFECTS_H
#define EFFECTS_H

struct bolt;

class monster;
struct item_def;

void banished(dungeon_feature_type gate_type, const std::string &who = "");

bool forget_spell(void);

int mushroom_prob(item_def & corpse);

bool mushroom_spawn_message(int seen_targets, int seen_corpses);

int spawn_corpse_mushrooms(item_def &corpse,
                           int target_count,
                           int & seen_targets,
                           beh_type toadstool_behavior = BEH_HOSTILE,
                           bool distance_as_time = false);

struct mgen_data;
int place_ring(std::vector<coord_def>& ring_points,
               const coord_def& origin,
               mgen_data prototype,
               int n_arcs,
               int arc_occupancy,
               int& seen_count);

class los_base;
// Collect lists of points that are within LOS (under the given losgrid),
// unoccupied, and not solid (walls/statues).
void collect_radius_points(std::vector<std::vector<coord_def> > &radius_points,
                           const coord_def &origin, const los_base* los);

void random_uselessness(int scroll_slot = -1);

bool recharge_wand(const int item_slot = -1, bool known = true);

void direct_effect(monster* src, spell_type spl, bolt &pbolt, actor *defender);

void yell(bool force = false);

int holy_word(int pow, int caster, const coord_def& where, bool silent = false,
              actor *attacker = NULL);

int holy_word_player(int pow, int caster, actor *attacker = NULL);
int holy_word_monsters(coord_def where, int pow, int caster,
                       actor *attacker = NULL);

int torment(int caster, const coord_def& where);

int torment_player(int pow, int caster);
int torment_monsters(coord_def where, int pow, int caster,
                     actor *attacker = NULL);

void immolation(int pow, int caster, coord_def where, bool known = false,
                actor *attacker = NULL);

void conduct_electricity(coord_def where, actor *attacker);

void cleansing_flame(int pow, int caster, coord_def where,
                     actor *attacker = NULL);

void change_labyrinth(bool msg = false);

bool forget_inventory(bool quiet = false);
bool vitrify_area(int radius);
void update_corpses(int elapsedTime);
void update_level(int elapsedTime);
void handle_time();
void recharge_rods(int aut, bool floor_only);

void slime_wall_damage(actor* act, int delay);

#endif
