/**
 * @file
 * @brief Formatted scroller
**/

#pragma once

#include "menu.h"
#include "ui.h"

enum FSFlag {
    FS_START_AT_END = 0x01,
    FS_PREWRAPPED_TEXT = 0x02,
    FS_EASY_EXIT = 0x04,
};

class formatted_scroller
{
public:
    formatted_scroller(int flags, const string& text = "")
    {
        m_flags = flags;
        add_text(text);
    };
    formatted_scroller(const string& text = "") : formatted_scroller(0, text) {};

    virtual void add_formatted_string(const formatted_string& s, bool new_line = false);
    virtual void add_text(const string& s, bool new_line = false);
    virtual void add_raw_text(const string& s, bool new_line = false);

    virtual int show();
    int get_lastch() { return m_lastch; }

    void set_tag(const string& tag) { m_tag = tag; }

    void set_more() { m_more.clear(); }
    void set_more(formatted_string more) { m_more = move(more); }

    void set_title() { m_title.clear(); };
    void set_title(formatted_string title) { m_title = move(title); };

    void set_scroll(int y);

    const formatted_string& get_contents() { return contents; };

    string highlight;

protected:

    formatted_string contents;
    string m_tag;
    formatted_string m_title;
    formatted_string m_more;
    int m_lastch;
    int m_flags;
    int m_scroll;

    bool m_contents_dirty, m_scroll_dirty;

    virtual bool process_key(int keyin);
    shared_ptr<ui::Scroller> m_scroller;
};

void recv_formatted_scroller_scroll(int line);
