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

void delete_cloud( int cloud );
void move_cloud( int cloud, int new_x, int new_y );

void check_place_cloud( int cl_type, int x, int y, int lifetime,
                        kill_category whose );
void place_cloud(int cl_type, int ctarget_x, int ctarget_y, int cl_range,
                 kill_category whose);

void manage_clouds(void);

bool is_opaque_cloud(unsigned char cloud_idx);

#endif
