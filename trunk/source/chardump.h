/*
 *  File:       chardump.cc
 *  Summary:    Dumps character info out to the morgue file.
 *  Written by: Linley Henzell
 *
 *  Change History (most recent first):
 *
 *               <1>     4/20/99        JDJ             Reformatted, uses string objects, split out
 *                                                                      7 functions from dump_char, dumps artifact info.
 */


#ifndef CHARDUMP_H
#define CHARDUMP_H


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr - ouch
 * *********************************************************************** */
bool dump_char( const char fname[30], bool show_prices );


#endif
