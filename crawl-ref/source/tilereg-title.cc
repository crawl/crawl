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
    m_buf(true, false), m_logo_buf(true, false), m_font_buf(font)
{
    sx = sy = 0;
    dx = dy = 1;

    add_tex(m_buf, m_img, _get_title_image().c_str(), width, height);
    add_tex(m_logo_buf, m_logo_img, "logo.png", width, height);
}

void TitleRegion::add_tex(VertBuffer &buf, GenericTexture &img, const char *file,
                          int width, int height)
{
    buf.set_tex(&img);

    if (!img.load_texture(file, MIPMAP_NONE))
        return;

    if ((int)img.orig_width() < width && (int)img.orig_height() < height)
    {
        // Center
        wx = width;
        wy = height;
        ox = (wx - img.orig_width()) / 2;
        oy = (wy - img.orig_height()) / 2;

        GLWPrim rect(0, 0, img.width(), img.height());
        rect.set_tex(0, 0, 1, 1);
        buf.add_primitive(rect);
    }
    else
    {
        // scale to fit
        if (width < height)
        {
            // taller than wide (scale to fit width; leave top/bottom borders)
            ox = 0;
            oy = (height - img.orig_height()*width/img.orig_width())/2;
            wx = img.width()*width/img.orig_width();
            wy = img.height()*width/img.orig_width();
        }
        else
        {
            // wider than tall (so scale to fit height; leave left/right borders)
            oy = 0;
            ox = (width - img.orig_width()*height/img.orig_height())/2;
            wx = img.width()*height/img.orig_height();
            wy = img.height()*height/img.orig_height();
        }

        GLWPrim rect(0, 0, wx, wy);
        rect.set_tex(0, 0, 1, 1);
        buf.add_primitive(rect);
    }
}

void TitleRegion::render()
{
#ifdef DEBUG_TILES_REDRAW
    cprintf("rendering TitleRegion\n");
#endif
    set_transform();
    m_buf.draw();
    m_logo_buf.draw();
    m_font_buf.draw();
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
void TitleRegion::update_message(string message)
{
    m_font_buf.clear();
    m_font_buf.add(message, VColour::white, 0, 0);
}

#endif
