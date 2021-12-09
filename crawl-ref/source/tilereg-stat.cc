#include "AppHdr.h"

#ifdef USE_TILE_LOCAL

#include "tilereg-stat.h"

#include "libutil.h"
#include "macro.h"
#include "tiles-build-specific.h"

StatRegion::StatRegion(FontWrapper *font_arg) : TextRegion(font_arg)
{
}

int StatRegion::handle_mouse(wm_mouse_event &event)
{
    if (mouse_control::current_mode() != MOUSE_MODE_COMMAND)
        return 0;

    if (!inside(event.px, event.py))
        return 0;

    if (event.event != wm_mouse_event::PRESS || event.button != wm_mouse_event::LEFT)
        return 0;

    if (!tiles.is_using_small_layout())
    {
        // clicking on stats should show all the stats
        return command_to_key(CMD_RESISTS_SCREEN);
    }
    else
    {
        // if small layout, toggle the tab icons instead
        return command_to_key(CMD_TOGGLE_TAB_ICONS);
    }
}

bool StatRegion::update_tip_text(string& tip)
{
    if (mouse_control::current_mode() != MOUSE_MODE_COMMAND)
        return false;

    if (!tiles.is_using_small_layout())
        tip = "[L-Click] Show player information";
    else
        tip = "[L-Click] Toggle tab icons";
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
