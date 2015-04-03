#include "AppHdr.h"

#ifdef USE_TILE_LOCAL

#include "tilereg-title.h"

#include "files.h"
#include "libutil.h"
#include "macro.h"
#include "random.h"

static const string _get_title_image()
{
    vector<string> files = get_title_files();
    return files[random2(files.size())];
}

TitleRegion::TitleRegion(int width, int height, FontWrapper* font) :
    m_buf(true, false), m_font_buf(font)
{
    // set the texture for the title image
    m_buf.set_tex(&m_img);

    sx = sy = 0;
    dx = dy = 1;

    if (!m_img.load_texture(_get_title_image().c_str(), MIPMAP_NONE))
        return;

    if ((int)m_img.orig_width() < width && (int)m_img.orig_height() < height)
    {
        // Center
        wx = width;
        wy = height;
        ox = (wx - m_img.orig_width()) / 2;
        oy = (wy - m_img.orig_height()) / 2;

        GLWPrim rect(0, 0, m_img.width(), m_img.height());
        rect.set_tex(0, 0, 1, 1);
        m_buf.add_primitive(rect);
    }
    else
    {
        // scale to fit
        if (width < height)
        {
            // taller than wide (scale to fit width; leave top/bottom borders)
            ox = 0;
            oy = (height - m_img.orig_height()*width/m_img.orig_width())/2;
            wx = m_img.width()*width/m_img.orig_width();
            wy = m_img.height()*width/m_img.orig_width();
        }
        else
        {
            // wider than tall (so scale to fit height; leave left/right borders)
            oy = 0;
            ox = (width - m_img.orig_width()*height/m_img.orig_height())/2;
            wx = m_img.width()*height/m_img.orig_height();
            wy = m_img.height()*height/m_img.orig_height();
        }

        GLWPrim rect(0, 0, wx, wy);
        rect.set_tex(0, 0, 1, 1);
        m_buf.add_primitive(rect);
    }
}

void TitleRegion::render()
{
#ifdef DEBUG_TILES_REDRAW
    cprintf("rendering TitleRegion\n");
#endif
    set_transform();
    m_buf.draw();
    m_font_buf.draw();
}

void TitleRegion::run()
{
    mouse_control mc(MOUSE_MODE_MORE);
    getchm();
    // XXX Figure out why, during the title, the first call to
    // TilesFramework::getch_ck() through getchm() is returning -10000 before
    // any real keyboard input.
    getchm();
}

/**
 * We only want to show one line of message by default so clear the
 * font buffer before adding the new message.
 */
void TitleRegion::update_message(string message)
{
    m_font_buf.clear();
    m_font_buf.add(message, VColour::white, 0, 0);
}

#endif
