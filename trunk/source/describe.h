
/*
 *  File:       describe.h
 *  Summary:    Functions used to print information about various game objects.
 *  Written by: Linley Henzell
 *
 *  Change History (most recent first):
 *
 *      <2>      5/21/99        BWR             Changed from is_artifact to is_dumpable_artifact
 *      <1>      4/20/99        JDJ             Added get_item_description and is_artifact.
 */

#ifndef DESCRIBE_H
#define DESCRIBE_H

#include <string>
#include "externs.h"

// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: chardump - spells4
 * *********************************************************************** */
bool is_dumpable_artifact( const item_def &item, char verbose );

// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: chardump - describe
 * *********************************************************************** */
std::string get_item_description( const item_def &item, char verbose,
                                  bool dump = false );

// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr - religion
 * *********************************************************************** */
void describe_god( int which_god, bool give_title );


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: item_use - shopping
 * *********************************************************************** */
void describe_item( const item_def &item );

// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: direct
 * *********************************************************************** */
void describe_monsters(int class_described, unsigned char which_mons);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: item_use
 * *********************************************************************** */
void describe_spell(int spelled);

#endif
