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
void hiscores_new_entry( struct scorefile_entry &se );

// last updated 16feb2001 {gdl}
/* ***********************************************************************
 * called from: acr ouch
 * *********************************************************************** */
void hiscores_print_list( int display_count = -1, int format = SCORE_TERSE );

// last updated 16feb2001 {gdl}
/* ***********************************************************************
 * called from: ouch hiscores
 * *********************************************************************** */
void hiscores_format_single( char *buffer, struct scorefile_entry &se );
int  hiscores_format_single_long( char *buffer, struct scorefile_entry &se, 
                                  bool verbose = false );

#endif  // HISCORES_H
