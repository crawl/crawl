/*
 *  File:       cloud.h
 *  Summary:    Functions related to clouds.
 *  Written by: Brent Ross 
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 *     <1>    Oct 1/2001      BWR             Created
 */


#ifndef CLOUD_H
#define CLOUD_H

#include "externs.h"
#include "travel.h"

enum fog_machine_type
{
    FM_GEYSER,
    FM_SPREAD,
    FM_BROWNIAN,
    NUM_FOG_MACHINE_TYPES
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
void move_cloud( int cloud, int new_x, int new_y );

void check_place_cloud( cloud_type cl_type, int x, int y, int lifetime,
                        kill_category whose, int spread_rate = -1 );
void place_cloud(cloud_type cl_type, int ctarget_x, int ctarget_y,
                 int cl_range, kill_category whose, int spread_rate = -1 );

void manage_clouds(void);

bool is_opaque_cloud(unsigned char cloud_idx);
int steam_cloud_damage(const cloud_struct &cloud);

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
