/**
 * @file
 * @brief Throwing and launching stuff.
**/

#pragma once

#include <string>

#include "enum.h"
#include "quiver.h"
#include "ranged-attack.h"

// TODO: this whole thing is a mess
enum fire_type
{
    FIRE_NONE      = 0x0000,
    FIRE_DART      = 0x0001,
    FIRE_STONE     = 0x0002,
    FIRE_JAVELIN   = 0x0004,
    FIRE_ROCK      = 0x0008,
    FIRE_NET       = 0x0010,
    FIRE_BOOMERANG = 0x0020,
    FIRE_THROWING  = FIRE_DART | FIRE_STONE | FIRE_JAVELIN | FIRE_ROCK
                               | FIRE_NET | FIRE_BOOMERANG,
    FIRE_LAUNCHER  = 0x0040,
    FIRE_INSCRIBED = 0x1000,   // Only used for _get_fire_order
    FIRE_SPELL     = 0x2000, // TODO: more fine-grained
    FIRE_EVOKABLE  = 0x4000,
    FIRE_ABILITY   = 0x8000
};

struct bolt;
class dist;

// A simple wrapper around the beams used to fire ranged attacks.
struct ranged_attack_beam
{
    bolt beam;
    ranged_attack atk;

    ranged_attack_beam(actor& agent, item_def& item);
    ranged_attack_beam(actor& agent, item_def& item, bolt& beam);

    void fire();

private:
    void initialise_beam(actor &agent, item_def &item);
};

bool is_penetrating_attack(const item_def& weapon);
bool fire_warn_if_impossible(bool silent, item_def *weapon);
shared_ptr<quiver::action> get_ammo_to_shoot(int item, dist &target, bool teleport = false);
void untargeted_fire(quiver::action &a);
void fire_item_no_quiver(dist *target=nullptr);

void aim_player_ranged_attack(quiver::action &a);
bool do_player_ranged_attack(const coord_def& targ, item_def* thrown_projectile = nullptr,
                             const ranged_attack* prototype = nullptr,
                             bool no_harm_allies = false, bool allow_salvo = true);

bool mons_throw(monster* mons, ranged_attack_beam& beam, bool teleport = false,
                bool was_redirected = false);

bool do_west_wind_shot();
