#include "AppHdr.h"

#ifdef USE_TILE_LOCAL

#include "tilereg-stat.h"

#include "libutil.h"
#include "macro.h"

StatRegion::StatRegion(FontWrapper *font) : TextRegion(font)
{
}

int StatRegion::handle_mouse(MouseEvent &event)
{
    if (mouse_control::current_mode() != MOUSE_MODE_COMMAND)
        return 0;

    if (!inside(event.px, event.py))
        return 0;

    if (event.event != MouseEvent::PRESS || event.button != MouseEvent::LEFT)
        return 0;

#ifdef TOUCH_UI
    // clicking on stats should show all the stats
    return command_to_key(CMD_RESISTS_SCREEN);
#else
    // Resting
    return command_to_key(CMD_REST);
#endif
}

bool StatRegion::update_tip_text(string& tip)
{
    if (mouse_control::current_mode() != MOUSE_MODE_COMMAND)
        return false;

    if (tiles.is_using_small_layout())
        tip = "[L-Click] Show player information";
    else
        tip = "[L-Click] Rest for a while";
    return true;
}

void StatRegion::_clear_buffers()
{
    m_shape_buf.clear();
}

void StatRegion::render()
{
    if (tiles.is_using_small_layout())
    {
        _clear_buffers();
        // black-out part of screen that stats are written on to
        //  - double up area to cover behind where tabs are drawn
        m_shape_buf.add(sx,sy,ex+(ex-sx),ey,VColour(0,0,0,255));
        m_shape_buf.draw();
    }
    TextRegion::render();
}

void StatRegion::clear()
{
    _clear_buffers();
    TextRegion::clear();
}

#endif
