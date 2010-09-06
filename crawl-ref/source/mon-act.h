/*
 *  File:       mon-act.h
 *  Summary:    Monsters doing stuff (monsters acting).
 *  Written by: Linley Henzell
 */

#ifndef MONACT_H
#define MONACT_H

void handle_monsters(void);
void handle_monster_move(monster* mon);

#define ENERGY_SUBMERGE(entry) (std::max(entry->energy_usage.swim / 2, 1))

#endif
