/* Zot Defence functions */

#ifndef ZOTDEF_H
#define ZOTDEF_H
// TODO: this should be replaced with pragma later or something?

#include "traps.h"


monster* zotdef_spawn(bool boss);
void zotdef_set_wave();
bool create_trap(trap_type spec_type);


bool create_zotdef_ally(monster_type mtyp, const char *successmsg);


bool zotdef_create_altar();
void zotdef_create_pond(const coord_def& center, int radius);
void zotdef_bosses_check();

string zotdef_debug_wave_desc();


int _zotdef_count_allies();

#endif // ZOTDEF_H
