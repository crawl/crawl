/*
 *  File:       newgame.h
 *  Summary:    Functions used when starting a new game.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 */


#ifndef NEWGAME_H
#define NEWGAME_H


/* ***********************************************************************
 * called from: initfile
 * *********************************************************************** */
int get_species_index_by_abbrev( const char *abbrev );
int get_species_index_by_name( const char *name );
const char *get_species_abbrev( int which_species );

int get_class_index_by_abbrev( const char *abbrev );
int get_class_index_by_name( const char *name );
const char *get_class_abbrev( int which_job );
const char *get_class_name( int which_job );

/* ***********************************************************************
 * called from: debug and hiscores
 * *********************************************************************** */
int get_species_by_abbrev( const char *abbrev );
int get_species_by_name( const char *name );
int get_class_by_abbrev( const char *abbrev  );
int get_class_by_name( const char *name );

undead_state_type get_undead_state(const species_type sp);

/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
bool new_game();
void initialise_item_descriptions();

int give_first_conjuration_book();
bool choose_race(void);
bool choose_class(void);

/* ***********************************************************************
 * called from: debug newgame
 * *********************************************************************** */
void give_basic_mutations(species_type speci);

bool validate_player_name(const char *name, bool verbose);

#endif
