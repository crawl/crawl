/*
 *  File:       output.cc
 *  Summary:    Functions used to print player related info.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 *               <1>     -/--/--        LRH             Created
 */


#ifndef OUTPUT_H
#define OUTPUT_H

#include "menu.h"

void update_turn_count();

void print_stats(void);

std::vector<formatted_string> get_full_detail(bool calc_unid);

const char *equip_slot_to_name(int equip);

int equip_name_to_slot(const char *s);

const char *equip_slot_to_name(int equip);

void print_overview_screen(void);
formatted_string get_full_detail2(bool calc_unid);
std::vector<formatted_string> get_stat_info(void);    
std::vector<formatted_string> get_res_info(bool calc_unid);    

#endif
