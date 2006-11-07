/*
 *  File:       maps.cc
 *  Summary:    Functions used to create vaults.
 *  Written by: Linley Henzell
 *
 *  Change History (most recent first):
 *
 *               <1>     -/--/--        LRH             Created
 */


#ifndef MAPS_H
#define MAPS_H

#include "FixVec.h"
#include "dungeon.h"

struct vault_placement
{
    int x, y;
    int width, height;

    vault_placement()
        : x(-1), y(-1), width(0), height(0)
    {
    }
};

int vault_main(map_type vgrid, 
                FixedVector<int, 7>& mons_array, 
                vault_placement &vp,
                int vault_force, 
                int many_many);

int random_map_for_place(const std::string &place, bool mini = false);
int find_map_named(const std::string &name);
int random_map_for_depth(int depth, bool want_minivault = false);
int random_map_for_tag(std::string tag);


#endif
