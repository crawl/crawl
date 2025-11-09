#include "AppHdr.h"

#ifdef USE_TILE_LOCAL

#include "tilereg-pause.h"

#include "colour.h"
#include "command.h"
#include "format.h"
#include "glwrapper.h"
#include "libutil.h"
#include "macro.h"
#include "pause-menu.h"
#include "tilefont.h"
#include "ui.h"

PauseButtonRegion::PauseButtonRegion(const TileRegionInit &init)
    : TileRegion(init), m_hover(false)
{
}

void PauseButtonRegion::render()
{
    if (!m_tag_font)
        return;

    set_transform();
    
    // Draw pause text centered
    const string text = m_hover ? "[PAUSE]" : "PAUSE";
    colour_t text_colour = m_hover ? CYAN : WHITE;
    
    int text_width = m_tag_font->char_width() * text.length();
    int text_height = m_tag_font->char_height();
    
    int text_x = ox + (wx - text_width) / 2;
    int text_y = oy + (wy - text_height) / 2;
    
    m_tag_font->render_string(text_x, text_y, formatted_string(text, text_colour));
}

void PauseButtonRegion::clear()
{
    m_hover = false;
}

int PauseButtonRegion::handle_mouse(wm_mouse_event &event)
{
    int cx, cy;
    if (!mouse_pos(event.px, event.py, cx, cy))
    {
        if (m_hover)
        {
            m_hover = false;
            return CK_REDRAW;
        }
        return 0;
    }

    // Handle hover
    if (!m_hover)
    {
        m_hover = true;
        return CK_REDRAW;
    }

    // Handle click
    if (event.event == wm_mouse_event::PRESS)
    {
        show_pause_menu();
        return CK_REDRAW;
    }

    return 0;
}

bool PauseButtonRegion::update_tip_text(string &tip)
{
    tip = "Click to pause the game (F2)";
    return true;
}

void PauseButtonRegion::on_resize()
{
    // Fixed size button based on text
    mx = 1;
    my = 1;
    
    if (m_tag_font)
    {
        const string text = "[PAUSE]"; // Use the longer hover text for sizing
        dx = m_tag_font->char_width() * text.length() + 10; // 10px padding
        dy = m_tag_font->char_height() + 6; // 6px padding
    }
    else
    {
        dx = 60; // Fallback size
        dy = 20;
    }
    
    // Calculate pixel dimensions
    wx = dx;
    wy = dy;
}

void PauseButtonRegion::show_pause_menu()
{
    // Use the same logic as the F2 key press
    PauseMenu menu;
    menu.show();
}

#endif
