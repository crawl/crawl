/*
 *  File:       output.cc
 *  Summary:    Functions used to print player related info.
 *  Written by: Linley Henzell
 *
 *  Change History (most recent first):
 *
 *               <1>     -/--/--        LRH             Created
 */


#ifndef OUTPUT_H
#define OUTPUT_H

// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr - player - stuff
 * *********************************************************************** */
void print_stats(void);

/* ***********************************************************************
 * called from: chardump
 * *********************************************************************** */
void get_full_detail(char* buffer, bool calc_unid);

const char *equip_slot_to_name(int equip);

int equip_name_to_slot(const char *s);

const char *equip_slot_to_name(int equip);

#endif
