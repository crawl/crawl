/**
 * @file
 * @brief Formatted scroller
**/

#include "AppHdr.h"

#include "scroller.h"
#include "stringutil.h"

using namespace ui;

static vector<formatted_scroller*> open_scrollers;
static bool from_webtiles;

static int _line_height()
{
#ifdef USE_TILE_LOCAL
    return tiles.get_crt_font()->char_height();
#else
    return 1;
#endif
}

void formatted_scroller::add_formatted_string(const formatted_string& fs, bool new_line)
{
    contents += fs;
    if (new_line)
        contents.cprintf("\n");
    m_contents_dirty = true;
}

void formatted_scroller::add_text(const string& s, bool new_line)
{
    add_formatted_string(formatted_string::parse_string(s), new_line);
}

void formatted_scroller::add_raw_text(const string& s, bool new_line)
{
    contents.cprintf("%s%s", s.c_str(), new_line ? "\n" : "");
    m_contents_dirty = true;
}

class UIHookedScroller : public Scroller
{
public:
    UIHookedScroller(formatted_scroller &_fs) : Scroller(), fs(_fs) {};
    virtual void set_scroll(int y) override {
        if (y == m_scroll)
            return;
#ifdef USE_TILE_WEB
        tiles.json_open_object();
        tiles.json_write_bool("from_webtiles", from_webtiles);
        tiles.json_write_int("scroll", y / _line_height());
        tiles.ui_state_change("formatted-scroller", 1);
#endif
        Scroller::set_scroll(y);
    };
protected:
    formatted_scroller &fs;
};

void formatted_scroller::scroll_to_end()
{
    // this needs to match the value in ui-layout.js scroller_scroll_to_line
    // (TODO: why?)
    m_scroll = numeric_limits<int32_t>::max();
    m_scroll_dirty = true;
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

    m_scroller = make_shared<UIHookedScroller>(*this);
#ifndef USE_TILE_LOCAL // ensure CRT scroller uses full height
    m_scroller->expand_v = true;
#endif
    auto text = make_shared<Text>();
    formatted_string c = formatted_string::parse_string(contents.to_colour_string());
    text->set_text(c);
    text->set_highlight_pattern(highlight, true);
    text->wrap_text = !(m_flags & FS_PREWRAPPED_TEXT);
    m_scroller->set_child(text);
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

    m_contents_dirty = false;
    bool done = false;
    popup->on(Widget::slots.event, [&done, &vbox, &text, this](wm_event ev) {
        if (ev.type != WME_KEYDOWN)
            return false; // allow default event handling
        m_lastch = ev.key.keysym.sym;
        done = !process_key(m_lastch);
        if (m_contents_dirty)
        {
            m_contents_dirty = false;
            text->set_text(contents);
#ifdef USE_TILE_WEB
            tiles.json_open_object();
            tiles.json_write_string("text", contents.to_colour_string());
            tiles.json_write_string("highlight", highlight);
            tiles.ui_state_change("formatted-scroller", 0);
#endif
        }
        if (m_scroll_dirty)
        {
            m_scroll_dirty = false;
            m_scroller->set_scroll(m_scroll);
        }
        if (done)
            return true;
        if (vbox->on_event(ev))
            return true;
        if (m_flags & FS_EASY_EXIT)
            return done = true;
        return false;
    });

#ifdef USE_TILE_WEB
    tiles.json_open_object();
    tiles.json_write_string("tag", m_tag);
    tiles.json_write_string("text", contents.to_colour_string());
    tiles.json_write_string("highlight", highlight);
    tiles.json_write_string("more", m_more.to_colour_string());
    tiles.json_write_bool("start_at_end", m_flags & FS_START_AT_END);
    tiles.push_ui_layout("formatted-scroller", 2);
#endif

    if (m_flags & FS_START_AT_END)
        scroll_to_end();

    open_scrollers.push_back(this);
    if (m_scroll_dirty)
    {
        m_scroll_dirty = false;
        m_scroller->set_scroll(m_scroll);
    }

    ui::run_layout(move(popup), done);
    open_scrollers.pop_back();

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

void formatted_scroller::set_scroll(int y)
{
    if (from_webtiles)
        m_scroller->set_scroll(y);
    else
    {
        m_scroll = y;
        m_scroll_dirty = true;
    }
}

void recv_formatted_scroller_scroll(int line)
{
    if (open_scrollers.size() == 0)
        return;
    formatted_scroller *scroller = open_scrollers.back();
    from_webtiles = true;
    scroller->set_scroll(line*_line_height());
    from_webtiles = false;
    // XXX: since the scroll event from webtiles is not delivered by the event
    // pumping loop in ui::pump_events, the UI system won't automatically draw
    // any changes for console spectators, so we need to force a redraw here.
    ui_force_render();
}
