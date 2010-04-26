/*
 *  File:       tilereg-title.cc
 *
 *  Created by: ennewalker on Sat Jan 5 01:33:53 2008 UTC
 */

#include "AppHdr.h"

#ifdef USE_TILE

#include "tilereg-title.h"

#include <SDL_opengl.h>

#include "libutil.h"
#include "macro.h"

TitleRegion::TitleRegion(int width, int height, FontWrapper* font) :
    m_buf(true, false), m_font_buf(font)
{
    // set the texture for the title image
    m_buf.set_tex(&m_img);

    sx = sy = 0;
    dx = dy = 1;

    if (!m_img.load_texture("title.png", MIPMAP_NONE))
        return;

    // Center
    wx = width;
    wy = height;
    ox = (wx - m_img.orig_width()) / 2;
    oy = (wy - m_img.orig_height()) / 2;

    GLWRect rect(0, 0, m_img.width(), m_img.height());
    rect.set_tex(0, 0, 1, 1);

    // Push it
    m_buf.push(rect);
}

void TitleRegion::render()
{
#ifdef DEBUG_TILES_REDRAW
    cprintf("rendering TitleRegion\n");
#endif
    set_transform();
    m_buf.draw(NULL, NULL);
    m_font_buf.draw(NULL, NULL);
}

void TitleRegion::run()
{
    mouse_control mc(MOUSE_MODE_MORE);
    getchm();
}

/**
 * We only want to show one line of message by default so clear the
 * font buffer before adding the new message.
 */
void TitleRegion::update_message(std::string message)
{
    m_font_buf.clear();
    m_font_buf.add(message, VColour::white, 0, 0);
}

#endif
