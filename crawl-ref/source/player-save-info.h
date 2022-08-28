/**
 * @file
 * @brief Player related functions.
**/


#pragma once

#include "game-type.h"
#include "god-type.h"
#include "species.h"
#ifdef USE_TILE
#include "tiledoll.h"
#endif

struct player_save_info
{
    player_save_info()
        : species(SP_UNKNOWN), job(JOB_UNKNOWN),
          experience_level(0), class_name("Ruminator"),
          religion(GOD_NO_GOD), wizard(false), saved_game_type(NUM_GAME_TYPE),
          species_name("Yak"), god_name("Marduk"),
          // non-header stuff
          save_loadable(true)
    { }

    // data from char chunk header
    string name;
    string prev_save_version;
    species_type species;
    job_type job;
    int experience_level;
    string class_name;
    god_type religion;
    string jiyva_second_name;
    bool wizard;
    game_type saved_game_type;
    string species_name;
    string god_name;
    string map;
    bool explore;

#ifdef USE_TILE
    dolls_data doll; // not in header
#endif

    bool save_loadable;
    string filename;

    player_save_info& operator=(const player& rhs);
    bool operator<(const player_save_info& rhs) const;
    string short_desc(bool use_qualifier=true) const;
    string really_short_desc() const;
};
