/*
 *  File:       tilereg_stat.h
 *  Created by: ennewalker on Sat Jan 5 01:33:53 2008 UTC
 */

#ifdef USE_TILE
#ifndef TILEREG_STAT_H
#define TILEREG_STAT_H

#include "tilereg-text.h"

class StatRegion : public TextRegion
{
public:
    StatRegion(FontWrapper *font);

    virtual int handle_mouse(MouseEvent &event);
    virtual bool update_tip_text(std::string &tip);
};

#endif
#endif
