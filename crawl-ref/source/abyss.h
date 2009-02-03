/*
 *  File:       abyss.h
 *  Summary:    Misc abyss specific functions.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 */


#ifndef ABYSS_H
#define ABYSS_H

void generate_abyss();
void area_shift();
void abyss_teleport(bool new_area);
void save_abyss_uniques();
bool lugonu_corrupt_level(int power);
void run_corruption_effects(int duration);

#endif
