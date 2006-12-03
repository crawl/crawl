/*
 *  File:       overmap.cc
 *  Summary:    "Overmap" functionality
 *  Written by: Linley Henzell
 *
 *  Change History (most recent first):
 *
 *               <1>    --/--/--        LRH             Created
 */


#ifndef OVERMAP_H
#define OVERMAP_H

void seen_notable_thing( int which_thing, int x, int y );
void display_overmap();
void unnotice_labyrinth_portal();
std::string overview_description_string();

#endif
