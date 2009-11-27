/*
 *  File:       effects.h
 *  Summary:    Misc stuff.
 *  Written by: Linley Henzell
 */


#ifndef EFFECTS_H
#define EFFECTS_H

struct bolt;

class monsters;
struct item_def;

void banished(dungeon_feature_type gate_type, const std::string &who = "");

bool forget_spell(void);

bool lose_stat(unsigned char which_stat, unsigned char stat_loss,
               bool force = false, const std::string cause = "",
               bool see_source = true);
bool lose_stat(unsigned char which_stat, unsigned char stat_loss,
               bool force = false, const char* cause = NULL,
               bool see_source = true);
bool lose_stat(unsigned char which_stat, unsigned char stat_loss,
               const monsters* cause, bool force = false);
bool lose_stat(unsigned char which_stat, unsigned char stat_loss,
               const item_def &cause, bool removed, bool force = false);

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

class los_def;
// Collect lists of points that are within LOS (under the given losgrid),
// unoccupied, and not solid (walls/statues).
void collect_radius_points(std::vector<std::vector<coord_def> > &radius_points,
                           const coord_def &origin, const los_def &los);

void random_uselessness(int scroll_slot = -1);

bool acquirement(object_class_type force_class, int agent,
                 bool quiet = false, int *item_index = NULL);

int acquirement_create_item(object_class_type class_wanted,
                            int agent,
                            bool quiet,
                            const coord_def &pos);

bool recharge_wand(const int item_slot = -1);

void direct_effect(monsters *src, spell_type spl, bolt &pbolt, actor *defender);

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
void update_corpses(double elapsedTime);
void update_level(double elapsedTime);
void handle_time(long time_delta);

#endif
