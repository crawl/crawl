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

void check_place_cloud( int cl_type, int x, int y, int lifetime );
void place_cloud(unsigned char cl_type, unsigned char ctarget_x, unsigned char ctarget_y, unsigned char cl_range);

void manage_clouds(void);

#endif
