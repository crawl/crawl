/**
 * @file
 * @brief Formatted scroller
**/

#include "AppHdr.h"

#include "scroller.h"
#include "stringutil.h"

using namespace ui;

void formatted_scroller::add_formatted_string(const formatted_string& fs, bool new_line)
{
    contents += fs;
    if (new_line)
        contents.cprintf("\n");
}

void formatted_scroller::add_text(const string& s, bool new_line)
{
    add_formatted_string(formatted_string::parse_string(s), new_line);
}

void formatted_scroller::add_raw_text(const string& s, bool new_line)
{
    contents.cprintf("%s%s", s.c_str(), new_line ? "\n" : "");
}

int formatted_scroller::show()
{
    auto vbox = make_shared<Box>(Widget::VERT);

    if (!m_title.empty())
    {
        shared_ptr<Text> title = make_shared<Text>();
        title->set_text(m_title);
        title->set_margin_for_crt({0, 0, 1, 0});
        title->set_margin_for_sdl({0, 0, 20, 0});
#ifdef USE_TILE_LOCAL
        title->align_self = Widget::Align::CENTER;
#endif
        vbox->add_child(move(title));
    }

#ifdef USE_TILE_LOCAL
    if (!(m_flags & FS_PREWRAPPED_TEXT))
        vbox->max_size()[0] = tiles.get_crt_font()->char_width()*80;
#endif

    m_scroller = make_shared<Scroller>();
#ifndef USE_TILE_LOCAL // ensure CRT scroller uses full height
    m_scroller->expand_v = true;
#endif
    auto text = make_shared<Text>();
    formatted_string c = formatted_string::parse_string(contents.to_colour_string());
    text->set_text(c);
    text->wrap_text = !(m_flags & FS_PREWRAPPED_TEXT);
    m_scroller->set_child(move(text));
    vbox->add_child(m_scroller);

    if (!m_more.empty())
    {
        shared_ptr<Text> more = make_shared<Text>();
        more = make_shared<Text>();
        more->set_text(m_more);
        more->set_margin_for_crt({1, 0, 0, 0});
        more->set_margin_for_sdl({20, 0, 0, 0});
        vbox->add_child(move(more));
    }

    auto popup = make_shared<ui::Popup>(vbox);

    bool done = false;
    popup->on(Widget::slots.event, [&done, &vbox, this](wm_event ev) {
        if (ev.type != WME_KEYDOWN)
            return false; // allow default event handling
        m_lastch = ev.key.keysym.sym;
        if ((done = !process_key(m_lastch)))
            return true;
        if (vbox->on_event(ev))
            return true;
        done = m_flags & FS_EASY_EXIT;
        return done;
    });

#ifdef USE_TILE_WEB
    tiles_crt_control disable_crt(false);
    tiles.json_open_object();
    tiles.json_write_string("text", contents.to_colour_string());
    tiles.json_write_string("more", m_more.to_colour_string());
    tiles.json_write_bool("start_at_end", m_flags & FS_START_AT_END);
    tiles.push_ui_layout("formatted-scroller", 0);
#endif

    // this needs to match the value in ui.js
    if (m_flags & FS_START_AT_END)
        m_scroller->set_scroll(numeric_limits<uint32_t>::max());

    ui::run_layout(move(popup), done);

#ifdef USE_TILE_WEB
    tiles.pop_ui_layout();
#endif

    return m_lastch;
}

bool formatted_scroller::process_key(int ch)
{
    switch (ch)
    {
        case CK_MOUSE_CMD:
        CASE_ESCAPE
            return false;
        default:
            return true;
    }
}
