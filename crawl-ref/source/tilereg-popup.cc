#include "AppHdr.h"

#ifdef USE_TILE_LOCAL

#include "tilereg-popup.h"
#include "tilereg-menu.h"

#include "menu.h"
#include "macro.h"
#include "tilebuf.h"
#include "tilefont.h"
#include "cio.h"

PopupRegion::PopupRegion(ImageManager *im, FontWrapper *entry) :
    MenuRegion(im, entry),
    m_retval(0)
{
}

int PopupRegion::handle_mouse(MouseEvent &event)
{
    // if mouse is outside popup box, pretend to handle event (to prevent
    // clash with other regions)
    int retval = MenuRegion::handle_mouse(event);
    return retval == 0 ? CK_NO_KEY : retval;
}

void PopupRegion::render()
{
#ifdef DEBUG_TILES_REDRAW
    cprintf("rendering PopupRegion\n");
#endif
    if (m_dirty)
        place_entries();

    MenuRegion::set_transform();
    m_shape_buf.draw();
    m_line_buf.draw();
    for (int i = 0; i < TEX_MAX; i++)
        m_tile_buf[i].draw();
    m_font_buf.draw();
}

void PopupRegion::place_entries()
{
    _clear_buffers();
    const VColour bgcolour(0, 0, 0, 63);
    const VColour border(255, 255, 255, 255);
    const VColour panel(0, 0, 0, 255);
    m_shape_buf.add(0, 0, ex, ey, bgcolour);
    m_shape_buf.add(ex / 4 - 2, ey / 4 - 2,
                    ex * 3 / 4 + 2, ey * 3 / 4 + 2, border);
    m_shape_buf.add(ex / 4, ey / 4, ex * 3 / 4, ey * 3 / 4, panel);
    _place_entries(ex / 4, ey / 4, ex / 2);
}


void PopupRegion::run()
{
    m_retval = getchm(KMC_CONFIRM);
}

int PopupRegion::get_retval()
{
    return m_retval;
}
#endif
