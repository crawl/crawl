/**
 * @file
 * @brief Throwing and launching stuff.
**/

#pragma once

#include <string>

#include "enum.h"
#include "quiver.h"

// TODO: this whole thing is a mess
enum fire_type
{
    FIRE_NONE      = 0x0000,
    FIRE_LAUNCHER  = 0x0001,
    FIRE_DART      = 0x0002,
    FIRE_STONE     = 0x0004,
    FIRE_JAVELIN   = 0x0010,
    FIRE_ROCK      = 0x0100,
    FIRE_NET       = 0x0200, // why is 0x0400 skipped?
    FIRE_BOOMERANG = 0x0800,
    FIRE_THROWING  = FIRE_DART | FIRE_STONE | FIRE_JAVELIN | FIRE_ROCK
                               | FIRE_NET | FIRE_BOOMERANG,
    FIRE_AMMO      = FIRE_LAUNCHER | FIRE_THROWING,
    FIRE_INSCRIBED = 0x1000,   // Only used for _get_fire_order
    FIRE_SPELL     = 0x2000, // TODO: more fine-grained
    FIRE_EVOKABLE  = 0x4000,
    FIRE_ABILITY   = 0x8000
};

struct bolt;
class dist;

bool is_penetrating_attack(const actor& attacker, const item_def* weapon,
                           const item_def& projectile);
bool fire_warn_if_impossible(bool silent, item_def *weapon);
shared_ptr<quiver::action> get_ammo_to_shoot(int item, dist &target, bool teleport = false);
bool is_pproj_active();
void untargeted_fire(shared_ptr<quiver::action> a);
void throw_item_no_quiver(dist *target=nullptr);

bool throw_it(bolt &pbolt, int throw_2, item_def *launcher, dist *target = nullptr);

bool thrown_object_destroyed(item_def *item);

void setup_monster_throw_beam(monster* mons, bolt &beam);
bool mons_throw(monster* mons, bolt &beam, int msl, bool teleport = false);
