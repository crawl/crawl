#ifdef USE_TILE_LOCAL
#ifndef TILEREG_TITLE_H
#define TILEREG_TITLE_H

#include "tilebuf.h"
#include "tilereg.h"

class TitleRegion : public ControlRegion
{
public:
    TitleRegion(int width, int height, FontWrapper *font);

    virtual void render();
    virtual void clear() {};
    virtual void run();

    virtual int handle_mouse(MouseEvent &event) { return 0; }

    void update_message(string message);

protected:
    virtual void on_resize() {}

    GenericTexture m_img, m_logo_img;
    VertBuffer m_buf, m_logo_buf;
    FontBuffer m_font_buf;

    void add_tex(VertBuffer &buf, GenericTexture &img, const char *file,
                 int width, int height);
};

#endif
#endif
