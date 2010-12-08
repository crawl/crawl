/* Zot Defence functions */

#ifndef ZOTDEF_H
#define ZOTDEF_H

int zotdef_spawn(bool boss);
void zotdef_set_wave();
bool create_trap(trap_type spec_type);
bool create_zotdef_ally(monster_type mtyp, const char *successmsg);
void zotdef_bosses_check();

void debug_waves();
std::string zotdef_debug_wave_desc();

#endif // ZOTDEF_H
