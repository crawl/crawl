/*
 *  File:       cloud.h
 *  Summary:    Functions related to clouds.
 *  Written by: Brent Ross 
 *
 *  Change History (most recent first):
 *
 *     <1>    Oct 1/2001      BWR             Created
 */


#ifndef CLOUD_H
#define CLOUD_H

#include "externs.h"

enum fog_machine_type
{
    FM_GEYSER,
    FM_SPREAD,
    FM_BROWNIAN,
    NUM_FOG_MACHINE_TYPES
};

cloud_type random_smoke_type();

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

#endif
