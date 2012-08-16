/**
 * @file
 * @brief Throwing and launching stuff.
**/

#ifndef THROW_H
#define THROW_H

#include <string>
#include "externs.h"
#include "enum.h"

enum fire_type
{
    FIRE_NONE      = 0x0000,
    FIRE_LAUNCHER  = 0x0001,
    FIRE_DART      = 0x0002,
    FIRE_STONE     = 0x0004,
    FIRE_JAVELIN   = 0x0010,
    FIRE_ROCK      = 0x0100,
    FIRE_NET       = 0x0200,
    FIRE_RETURNING = 0x0400,
    FIRE_TOMAHAWK  = 0x0800,
    FIRE_INSCRIBED = 0x1000,   // Only used for _get_fire_order
};

struct bolt;
class dist;

bool item_is_quivered(const item_def &item);
bool fire_warn_if_impossible(bool silent = false);
int get_next_fire_item(int current, int offset);
int get_ammo_to_shoot(int item, dist &target, bool teleport = false);
void fire_thing(int item = -1);
void throw_item_no_quiver(void);

bool throw_it(bolt &pbolt, int throw_2, bool teleport = false,
              int acc_bonus = 0, dist *target = NULL);

bool thrown_object_destroyed(item_def *item, const coord_def& where);
int launcher_final_speed(const item_def &launcher,
                         const item_def *shield, bool scaled = true);

void setup_monster_throw_beam(monster* mons, bolt &beam);
bool mons_throw(monster* mons, bolt &beam, int msl, bool teleport = false);
#endif
