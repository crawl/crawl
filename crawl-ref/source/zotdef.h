/* Zot Defence functions */

#ifndef ZOTDEF_H
#define ZOTDEF_H

int zotdef_spawn(bool boss);
void zotdef_set_wave();
rune_type get_rune(int runenumber);
bool create_trap(trap_type spec_type);
bool create_zotdef_ally(monster_type mtyp, const char *successmsg);
void zotdef_bosses_check();

#endif // ZOTDEF_H
