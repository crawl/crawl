/*
 *  File:       hiscores.h
 *  Summary:    Scorefile manipulation functions
 *  Written by: Gordon Lipford
 *
 *  Change History (most recent first):
 *
 *     <1>     16feb2001     GDL     Created
 */


#ifndef HISCORES_H
#define HISCORES_H

// last updated 16feb2001 {gdl}
/* ***********************************************************************
 * called from: ouch
 * *********************************************************************** */
void hiscores_new_entry( const scorefile_entry &se );

// last updated 02dec2006 {smm}
/* ***********************************************************************
 * called from: ouch
 * *********************************************************************** */
void logfile_new_entry( const scorefile_entry &se );

// last updated 16feb2001 {gdl}
/* ***********************************************************************
 * called from: acr ouch
 * *********************************************************************** */
void hiscores_print_list( int display_count = -1, int format = SCORE_TERSE );

// last updated 16feb2001 {gdl}
/* ***********************************************************************
 * called from: ouch hiscores
 * *********************************************************************** */
std::string hiscores_format_single( const scorefile_entry &se );
std::string hiscores_format_single_long( const scorefile_entry &se, 
                                  bool verbose = false );

#endif  // HISCORES_H
