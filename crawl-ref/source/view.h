/*
 *  File:       view.cc
 *  Summary:    Misc function used to render the dungeon.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 *     <2>     9/29/99     BCR     Added the BORDER_COLOR define
 *     <1>     -/--/--     LRH     Created
 */


#ifndef VIEW_H
#define VIEW_H


#include "externs.h"

#define BORDER_COLOR BROWN

void init_char_table(char_set_type set);
void init_feature_table( void );

int get_number_of_lines(void);
int get_number_of_cols(void);

/* ***********************************************************************
 * called from: dump_screenshot - chardump
 * *********************************************************************** */
void get_non_ibm_symbol(unsigned int object, unsigned short *ch,
                        unsigned short *color);

// last updated 29may2000 {dlb}
/* ***********************************************************************
 * called from: bang - beam - direct - effects - fight - monstuff -
 *              mstuff2 - spells1 - spells2
 * *********************************************************************** */
bool mons_near(struct monsters *monster, unsigned int foe = MHITYOU);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr - player - stuff
 * *********************************************************************** */
void draw_border(void);

// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr - view
 * *********************************************************************** */
void item(void);

void find_features(const std::vector<coord_def>& features,
	unsigned char feature, std::vector<coord_def> *found);

// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: direct - monstufff - view
 * *********************************************************************** */
void losight(FixedArray<unsigned int, 19, 19>& sh, FixedArray<unsigned char, 80, 70>& gr, int x_p, int y_p);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: ability - acr - it_use3 - item_use - spell
 * *********************************************************************** */
void magic_mapping(int map_radius, int proportion);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr - effects - it_use2 - it_use3 - item_use - spell -
 *              spells - spells3 - spells4
 * *********************************************************************** */
bool noisy( int loudness, int nois_x, int nois_y, const char *msg = NULL );


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr - spells3
 * *********************************************************************** */
void show_map( FixedVector<int, 2>& spec_place, bool travel_mode );


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
void viewwindow2(char draw_it, bool do_updates);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
void viewwindow3(char draw_it, bool do_updates);        // non-IBM graphics

// last updated 19jun2000 (gdl)
/* ***********************************************************************
 * called from: acr view
 * *********************************************************************** */
void setLOSRadius(int newLR);

// last updated 02apr2001 (gdl)
/* ***********************************************************************
 * called from: view monstuff
 * *********************************************************************** */
bool check_awaken(int mons_aw);

void clear_map();

bool is_feature(int feature, int x, int y);

void set_envmap_char( int x, int y, unsigned char chr );
void set_envmap_detected_item(int x, int y, bool detected = true);
void set_envmap_detected_mons(int x, int y, bool detected = true);
bool is_envmap_detected_item(int x, int y);
bool is_envmap_detected_mons(int x, int y);
void set_terrain_mapped( int x, int y );
void set_terrain_seen( int x, int y );
bool is_terrain_known( int x, int y );
bool is_terrain_seen( int x, int y );

void clear_feature_overrides();
void add_feature_override(const std::string &text);
void clear_cset_overrides();
void add_cset_override(char_set_type set, const std::string &overrides);

bool see_grid( int grx, int gry );

std::string screenshot(bool fullscreen = false);

unsigned char get_sightmap_char(int feature);
unsigned char get_magicmap_char(int feature);

int find_ray_path( int sourcex, int sourcey,
                   int targetx, int targety,
                   int xpos[], int ypos[] );

void viewwindow(bool draw_it, bool do_updates);

#if defined(WIN32CONSOLE) || defined(DOS)
unsigned short dos_brand( unsigned short colour,
                          unsigned brand = CHATTR_REVERSE);
#endif

#endif
