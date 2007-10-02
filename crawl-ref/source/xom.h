/*
 *  File:       xom.h
 *  Summary:    Misc Xom related functions.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 *               <1>     09/28/07        MPC             Split from religion.h
 */

#ifndef XOM_H
#define XOM_H

class item_def;

enum xom_message_type
{
    XM_NORMAL,
    XM_INTRIGUED,
    NUM_XOM_MESSAGE_TYPES
};

void xom_is_stimulated(int maxinterestingness,
                       xom_message_type message_type = XM_NORMAL,
                       bool force_message = false);
void xom_is_stimulated(int maxinterestingness, std::string message,
                       bool force_message = false);
bool xom_is_nice();
void xom_acts(bool niceness, int sever);
const char *describe_xom_favour();

inline void xom_acts(int sever)
{
    xom_acts(xom_is_nice(), sever);
}

void xom_check_lost_item(const item_def& item);
void xom_check_destroyed_item(const item_def& item, int cause = -1);

#endif
