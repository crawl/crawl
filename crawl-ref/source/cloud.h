/*
 *  File:       cloud.h
 *  Summary:    Functions related to clouds.
 *  Written by: Brent Ross
 */


#ifndef CLOUD_H
#define CLOUD_H

#include "externs.h"

enum fog_machine_type
{
    FM_GEYSER,
    FM_SPREAD,
    FM_BROWNIAN,
    NUM_FOG_MACHINE_TYPES,
};

struct fog_machine_data
{
    fog_machine_type fm_type;
    cloud_type       cl_type;
    int              size;
    int              power;
};

cloud_type random_smoke_type();

cloud_type cloud_type_at(const coord_def &pos);

void delete_cloud( int cloud );
void delete_cloud_at(coord_def p);
void move_cloud( int cloud, const coord_def& newpos );
void move_cloud_to(coord_def src, coord_def dest);

void check_place_cloud( cloud_type cl_type, const coord_def& p, int lifetime,
                        kill_category whose, int spread_rate = -1,
                        int colour = -1, std::string name = "",
                        std::string tile = "");
void check_place_cloud( cloud_type cl_type, const coord_def& p, int lifetime,
                        killer_type killer, int spread_rate = -1,
                        int colour = -1, std::string name = "",
                        std::string tile = "");
void check_place_cloud( cloud_type cl_type, const coord_def& p, int lifetime,
                        kill_category whose, killer_type killer,
                        int spread_rate = -1,
                        int colour = -1, std::string name = "",
                        std::string tile = "");
void place_cloud( cloud_type cl_type, const coord_def& ctarget,
                  int cl_range, kill_category whose, int spread_rate = -1,
                  int colour = -1, std::string name = "",
                  std::string tile = "");
void place_cloud( cloud_type cl_type, const coord_def& ctarget,
                  int cl_range, killer_type killer, int spread_rate = -1,
                  int colour = -1, std::string name = "",
                  std::string tile = "");
void place_cloud( cloud_type cl_type, const coord_def& ctarget,
                  int cl_range, kill_category whose, killer_type killer,
                  int spread_rate = -1, int colour = -1, std::string name = "",
                  std::string tile = "");

void manage_clouds(void);

bool is_opaque_cloud(int cloud_idx);
int steam_cloud_damage(const cloud_struct &cloud);
int steam_cloud_damage(int decay);

cloud_type beam2cloud(beam_type flavour);
beam_type cloud2beam(cloud_type flavour);

int resist_fraction(int resist, int bonus_res = 0);
int  max_cloud_damage(cloud_type cl_type, int power = -1);
void in_a_cloud(void);

std::string cloud_name_at_index(int cloudno);
std::string cloud_type_name(cloud_type type, bool terse = true);
int get_cloud_colour(int cloudno);

bool is_damaging_cloud(cloud_type type, bool temp = false);
bool is_harmless_cloud(cloud_type type);
bool in_what_cloud (cloud_type type);
cloud_type in_what_cloud ();

// fog generator
void place_fog_machine(fog_machine_type fm_type, cloud_type cl_type,
                       int x, int y, int size, int power);

void place_fog_machine(fog_machine_data data, int x, int y);

bool valid_fog_machine_data(fog_machine_data data);

int              num_fogs_for_place(int level_number = -1,
                                const level_id &place = level_id::current());
fog_machine_data random_fog_for_place(int level_number = -1,
                                const level_id &place = level_id::current());

int              fogs_pan_number(int level_number = -1);
fog_machine_data fogs_pan_type(int level_number = -1);

int              fogs_abyss_number(int level_number = -1);
fog_machine_data fogs_abyss_type(int level_number = -1);

int              fogs_lab_number(int level_number = -1);
fog_machine_data fogs_lab_type(int level_number = -1);

#endif
