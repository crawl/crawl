/**
 * @file
 * @brief Hierarchical layout system.
**/

#pragma once

#include <array>
#include <functional>

#include "format.h"
#include "tilefont.h"
#include "tiledef-gui.h"
#ifdef USE_TILE_LOCAL
# include "tilesdl.h"
# include "tilebuf.h"
#endif

// This is used instead of std::array because travis uses older versions of GCC
// and Clang that demand braces around initializer lists everywhere.
template <size_t N>
struct vec {
    int items[N];
    template <typename... Ts> vec<N> (Ts... l) : items{l...} {}
    const int& operator[](int index) const { return items[index]; }
    int& operator[](int index) { return items[index]; }
    inline bool operator==(const vec<N>& rhs) {
        return equal(begin(items), end(items), begin(rhs.items));
    }
    inline bool operator!=(const vec<N>& rhs) { return !(*this == rhs); }
};
using i2 = vec<2>;
using i4 = vec<4>;

struct UISizeReq
{
    int min, nat;
};

class UI
{
public:
    enum Align {
        UNSET = 0,
        START,
        END,
        CENTER,
        STRETCH,
    };

    enum Direction {
        HORZ = 0,
        VERT,
    };

    i4 margin = {0,0,0,0};
    int flex_grow = 1;
    Align align_self = UNSET;
    bool expand_h = false, expand_v = false;
    const i4 get_region() const { return m_region; }

    virtual void _render() = 0;
    virtual UISizeReq _get_preferred_size(Direction dim, int prosp_width);
    virtual void _allocate_region();

    // Wrapper functions which handle common behavior
    // - margins
    // - caching
    void render();
    UISizeReq get_preferred_size(Direction dim, int prosp_width);
    void allocate_region(i4 region);

    void set_margin_for_crt(i4 _margin)
    {
#ifndef USE_TILE_LOCAL
        margin = _margin;
#endif
    };
    void set_margin_for_sdl(i4 _margin)
    {
#ifdef USE_TILE_LOCAL
        margin = _margin;
#endif
    };

protected:
    i4 m_region;

private:
    bool cached_sr_valid[2] = { false, false };
    UISizeReq cached_sr[2];
    int cached_sr_pw;
};

// Box widget: similar to the CSS flexbox (without wrapping)
//  - Lays its children out in either a row or a column
//  - Extra space is allocated according to each child's flex_grow property
//  - align and justify properties work like flexbox's

class UIBox : public UI
{
public:
    enum Justify {
        START = 0,
        CENTER,
        END,
    };

    enum Expand {
        NONE = 0x0,
        EXPAND_H = 0x1,
        EXPAND_V = 0x2,
        SHRINK_H = 0x4,
        SHRINK_V = 0x8,
    };

    UIBox(Direction dir, Expand expand_flags = NONE)
    {
        horz = dir == HORZ;
        expand_h = expand_flags & EXPAND_H;
        expand_v = expand_flags & EXPAND_V;
    };
    void add_child(shared_ptr<UI> child);
    bool horz;
    Justify justify_items = START;
    UI::Align align_items = UNSET;

    virtual void _render() override;
    virtual UISizeReq _get_preferred_size(Direction dim, int prosp_width) override;
    virtual void _allocate_region() override;

protected:
    vector<int> layout_main_axis(vector<UISizeReq>& ch_psz, int main_sz);
    vector<int> layout_cross_axis(vector<UISizeReq>& ch_psz, int cross_sz);

    vector<shared_ptr<UI>> m_children;
};

class UIText : public UI
{
public:
    UIText() {}
    UIText(string text) : UIText()
    {
        set_text(formatted_string::parse_string(text));
    }

    void set_text(const formatted_string &fs);

    virtual void _render() override;
    virtual UISizeReq _get_preferred_size(Direction dim, int prosp_width) override;
    virtual void _allocate_region() override;

    bool wrap_text = false;
    bool ellipsize = false;

protected:
    void wrap_text_to_size(int width, int height);

    formatted_string m_text;
#ifdef USE_TILE_LOCAL
    struct brkpt { unsigned int op, line; };
    vector<brkpt> m_brkpts;
    formatted_string m_text_wrapped;
#else
    vector<formatted_string> m_wrapped_lines;
#endif
    i2 m_wrapped_size = { -1, -1 };
};

class UIImage : public UI
{
public:
    void set_tile(tile_def tile);

    bool shrink_h = false, shrink_v = false;

    virtual void _render() override;
    virtual UISizeReq _get_preferred_size(Direction dim, int prosp_width) override;

protected:
    tile_def m_tile = {TILEG_ERROR, TEX_GUI};
    int m_tw, m_th;

#ifdef USE_TILE_LOCAL
    GenericTexture m_img;
#endif
};

class UIStack : public UI
{
public:
    void add_child(shared_ptr<UI> child);
    void pop_child();
    size_t num_children() const { return m_children.size(); }
    shared_ptr<UI> get_child(size_t idx) const { return m_children[idx]; };

    virtual void _render() override;
    virtual UISizeReq _get_preferred_size(Direction dim, int prosp_width) override;
    virtual void _allocate_region() override;

protected:
    vector<shared_ptr<UI>> m_children;
};

class UIGrid : public UI
{
public:
    void add_child(shared_ptr<UI> child, int x, int y, int w = 1, int h = 1);
    int& track_flex_grow(int x, int y)
    {
        init_track_info();
        ASSERT(x == -1 || y == -1);
        return x >= 0 ? m_col_info[x].flex_grow : m_row_info[y].flex_grow;
    }

    virtual void _render() override;
    virtual UISizeReq _get_preferred_size(Direction dim, int prosp_width) override;
    virtual void _allocate_region() override;

protected:
    i4 get_tracks_region(int x, int y, int w, int h) const
    {
        return {
            m_col_info[x].offset, m_row_info[y].offset,
            m_col_info[x+w-1].size + m_col_info[x+w-1].offset - m_col_info[x].offset,
            m_row_info[y+h-1].size + m_row_info[y+h-1].offset - m_row_info[y].offset,
        };
    }

    struct track_info {
        int size;
        int offset;
        UISizeReq sr;
        int flex_grow;
    };
    vector<track_info> m_col_info;
    vector<track_info> m_row_info;

    struct child_info {
        i2 pos;
        i2 span;
    };
    vector<child_info> m_child_info;
    vector<shared_ptr<UI>> m_children;

    void layout_track(Direction dim, UISizeReq sr, int size);
    void set_track_offsets(vector<track_info>& tracks);
    void compute_track_sizereqs(Direction dim);
    void init_track_info();
    bool m_track_info_dirty = false;
};

void ui_push_layout(shared_ptr<UI> root);
void ui_pop_layout();
void ui_pump_events();

void ui_push_scissor(i4 scissor);
void ui_pop_scissor();

// XXX: this is a hack used to ensure that when switching to a
// layout-based UI, the starting window size is correct. This is necessary
// because there's no way to query TilesFramework for the current screen size
void ui_resize(int w, int h);
