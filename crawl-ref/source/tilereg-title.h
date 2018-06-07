#ifdef USE_TILE_LOCAL
#pragma once

#include "tilebuf.h"
#include "tilereg.h"

class TitleRegion : public ControlRegion
{
public:
    TitleRegion(int width, int height, FontWrapper *font);

    virtual void render() override;
    virtual void clear() override {};
    virtual void run() override;

    virtual int handle_mouse(MouseEvent &event) override { return 0; }

    void update_message(string message);

protected:
    virtual void on_resize() override {}

    GenericTexture m_img;
    VertBuffer m_buf;
    FontBuffer m_font_buf;
};

#endif
