/*
 *  File:       tilereg_title.h
 *  Created by: ennewalker on Sat Jan 5 01:33:53 2008 UTC
 */

#ifdef USE_TILE
#ifndef TILEREG_TITLE_H
#define TILEREG_TITLE_H

#include "tilereg.h"

class TitleRegion : public ControlRegion
{
public:
    TitleRegion(int width, int height, FontWrapper *font);

    virtual void render();
    virtual void clear() {};
    virtual void run();

    virtual int handle_mouse(MouseEvent &event) { return 0; }

    void update_message(std::string message);

protected:
    virtual void on_resize() {}

    GenericTexture m_img;
    VertBuffer m_buf;
    FontBuffer m_font_buf;
};

#endif
#endif
