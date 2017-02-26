#ifdef USE_TILE
#pragma once

#include "tile-player-flags.h"

struct dolls_data
{
    dolls_data();
    dolls_data(const dolls_data& orig);
    const dolls_data& operator=(const dolls_data& other);
    ~dolls_data();

    bool operator ==(const dolls_data &other) const;
    bool operator !=(const dolls_data &other) const { return !(*this == other); }

    tileidx_t *parts;
};

enum tile_doll_mode
{
    TILEP_MODE_EQUIP   = 0,  // draw doll based on equipment
    TILEP_MODE_LOADING = 1,  // draw doll based on dolls.txt definitions
    TILEP_MODE_DEFAULT = 2,  // draw doll based on job specific definitions
    TILEP_MODE_MAX
};

extern dolls_data player_doll;
extern int doll_gender;

void init_player_doll();
void fill_doll_equipment(dolls_data &result);
void create_random_doll(dolls_data &result);
void save_doll_file(writer &dollf);

// Saves player doll definitions into dolls.txt.
// Returns true if successful, else false.
bool save_doll_data(int mode, int num, const dolls_data* dolls);

// Loads player doll definitions from (by default) dolls.txt.
// Returns true if file found, else false.
bool load_doll_data(const char *fn, dolls_data *dolls, int max,
                    tile_doll_mode *mode, int *cur);

class SubmergedTileBuffer;
void pack_doll_buf(SubmergedTileBuffer& buf, const dolls_data &doll, int x, int y, bool submerged, bool ghost);

#endif
