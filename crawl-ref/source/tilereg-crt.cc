#include "AppHdr.h"

#ifdef USE_TILE_LOCAL

#include "tilereg-crt.h"

#include "cio.h"
#include "menu.h"
#include "tilefont.h"
#include "viewgeom.h"

CRTRegion::CRTRegion(FontWrapper *font_arg) : TextRegion(font_arg)
{
}

CRTRegion::~CRTRegion()
{
    clear();
}

int CRTRegion::handle_mouse(wm_mouse_event &event)
{
    if (event.event == wm_mouse_event::PRESS)
    {
        if (event.button == wm_mouse_event::LEFT)
            return CK_MOUSE_CLICK;
        else if (event.button == wm_mouse_event::RIGHT)
            return CK_MOUSE_CMD;
    }
    return 0;
}

void CRTRegion::on_resize()
{
    TextRegion::on_resize();
    crawl_view.termsz.x = mx;
    crawl_view.termsz.y = my;
}

void CRTRegion::render()
{
    set_transform(true);
    TextRegion::render();
}

#endif
