/*
 *  File:       xom.h
 *  Summary:    Misc Xom related functions.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 */

#ifndef XOM_H
#define XOM_H

#include "ouch.h"

struct item_def;

enum xom_message_type
{
    XM_NORMAL,
    XM_INTRIGUED,
    NUM_XOM_MESSAGE_TYPES
};

void xom_tick();
void xom_is_stimulated(int maxinterestingness,
                       xom_message_type message_type = XM_NORMAL,
                       bool force_message = false);
void xom_is_stimulated(int maxinterestingness, const std::string& message,
                       bool force_message = false);
bool xom_is_nice(int tension = -1);
void xom_acts(bool niceness, int sever, int tension = -1);
const char *describe_xom_favour(bool upper = false);

inline void xom_acts(int sever, int tension = -1)
{
    xom_acts(xom_is_nice(tension), sever, tension);
}

void xom_check_lost_item(const item_def& item);
void xom_check_destroyed_item(const item_def& item, int cause = -1);
void xom_death_message(const kill_method_type killed_by);
#endif
