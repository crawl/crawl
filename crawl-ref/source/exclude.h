#ifndef EXCLUDE_H
#define EXCLUDE_H

bool need_auto_exclude(const monsters *mon, bool sleepy = false);
void set_auto_exclude(const monsters *mon);
void remove_auto_exclude(const monsters *mon, bool sleepy = false);

#endif

