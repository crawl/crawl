/**
 * @file
 * @brief Misc stuff.
**/

#ifndef EFFECTS_H
#define EFFECTS_H

struct bolt;

class monster;
struct item_def;

void banished(const string &who = "");

bool forget_spell(void);

int mushroom_prob(item_def & corpse);

bool mushroom_spawn_message(int seen_targets, int seen_corpses);

int spawn_corpse_mushrooms(item_def& corpse,
                           int target_count,
                           int& seen_targets,
                           beh_type toadstool_behavior = BEH_HOSTILE,
                           bool distance_as_time = false);

struct mgen_data;
int place_ring(vector<coord_def>& ring_points,
               const coord_def& origin,
               mgen_data prototype,
               int n_arcs,
               int arc_occupancy,
               int& seen_count);

// Collect lists of points that are within LOS, unoccupied, and not solid
// (walls/statues).
void collect_radius_points(vector<vector<coord_def> > &radius_points,
                           const coord_def &origin, los_type los);

void random_uselessness(int scroll_slot = -1);

int recharge_wand(const int item_slot = -1, bool known = true,
                  string *pre_msg = NULL);

void direct_effect(monster* src, spell_type spl, bolt &pbolt, actor *defender);

void yell(bool force = false);

void holy_word(int pow, holy_word_source_type source, const coord_def& where,
               bool silent = false, actor *attacker = NULL);

void holy_word_monsters(coord_def where, int pow, holy_word_source_type source,
                        actor *attacker = NULL);

int torment(actor *attacker, int taux, const coord_def& where);
int torment_player(actor *attacker, int taux);
int torment_monsters(coord_def where, actor *attacker, int taux);

void conduct_electricity(coord_def where, actor *attacker);

void cleansing_flame(int pow, int caster, coord_def where,
                     actor *attacker = NULL);

void change_labyrinth(bool msg = false);

bool vitrify_area(int radius);
void update_level(int elapsedTime);
void handle_time();
void recharge_rods(int aut, bool floor_only);
void recharge_elemental_evokers(int exp);

void slime_wall_damage(actor* act, int delay);

#endif
