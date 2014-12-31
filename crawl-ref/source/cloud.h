/**
 * @file
 * @brief Functions related to clouds.
**/

#ifndef CLOUD_H
#define CLOUD_H

cloud_type random_smoke_type();

cloud_type cloud_type_at(const coord_def &pos);
bool cloud_is_yours_at(const coord_def &pos);

void delete_cloud(int cloud);
void delete_cloud_at(coord_def p);
void move_cloud(int cloud, const coord_def& newpos);
void move_cloud_to(coord_def src, coord_def dest);
void swap_clouds(coord_def p1, coord_def p2);

void check_place_cloud(cloud_type cl_type, const coord_def& p, int lifetime,
                       const actor *agent, int spread_rate = -1,
                       int colour = -1,
                       string name = "", string tile = "",
                       int excl_rad = -1);
void place_cloud(cloud_type cl_type, const coord_def& ctarget,
                 int cl_range, const actor *agent,
                 int spread_rate = -1, int colour = -1, string name = "",
                 string tile = "", int excl_rad = -1);

void manage_clouds();

bool is_opaque_cloud_type(cloud_type ctype);
bool is_opaque_cloud(int cloud_idx);

int max_cloud_damage(cloud_type cl_type, int power = -1);
int actor_apply_cloud(actor *act);

string cloud_name_at_index(int cloudno);
string cloud_type_name(cloud_type type, bool terse = true);
cloud_type cloud_name_to_type(const string &name);
colour_t get_cloud_colour(int cloudno);
colour_t get_cloud_colour(const cloud_struct &cloud);
coord_def get_cloud_originator(const coord_def& pos);

bool actor_cloud_immune(const actor *act, const cloud_struct &cloud);
bool is_damaging_cloud(cloud_type type, bool temp = false, bool yours = false);
bool is_harmless_cloud(cloud_type type);

bool mons_avoids_cloud(const monster* mons, int cloud_num,
                       bool placement = false);

void remove_tornado_clouds(mid_t whose);

void run_cloud_spreaders(int dur);

#endif
