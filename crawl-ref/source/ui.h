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

namespace ui {

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

struct SizeReq
{
    int min, nat;
};

class Widget
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
    bool shrink_h = false, shrink_v = false;
    const i4 get_region() const { return m_region; }

    virtual void _render() = 0;
    virtual SizeReq _get_preferred_size(Direction dim, int prosp_width);
    virtual void _allocate_region();

    // Wrapper functions which handle common behavior
    // - margins
    void render();
    SizeReq get_preferred_size(Direction dim, int prosp_width);
    void allocate_region(i4 region);

protected:
    i4 m_region;
};

// Box widget: similar to the CSS flexbox (without wrapping)
//  - Lays its children out in either a row or a column
//  - Extra space is allocated according to each child's flex_grow property
//  - align and justify properties work like flexbox's

class Box : public Widget
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

    Box(Direction dir, Expand expand_flags = NONE)
    {
        horz = dir == HORZ;
        expand_h = expand_flags & EXPAND_H;
        expand_v = expand_flags & EXPAND_V;
    };
    void add_child(shared_ptr<Widget> child);
    bool horz;
    Justify justify_items = START;
    Widget::Align align_items = UNSET;

    virtual void _render() override;
    virtual SizeReq _get_preferred_size(Direction dim, int prosp_width) override;
    virtual void _allocate_region() override;

protected:
    vector<int> layout_main_axis(vector<SizeReq>& ch_psz, int main_sz);
    vector<int> layout_cross_axis(vector<SizeReq>& ch_psz, int cross_sz);

    vector<shared_ptr<Widget>> m_children;
};

class Image : public Widget
{
public:
    Image() {};
    Image(tile_def tile) { set_tile(tile); };
    void set_tile(tile_def tile);

    virtual void _render() override;
    virtual SizeReq _get_preferred_size(Direction dim, int prosp_width) override;

protected:
    tile_def m_tile = {TILEG_ERROR, TEX_GUI};
    int m_tw, m_th;

#ifdef USE_TILE_LOCAL
    GenericTexture m_img;
#endif
};

class Stack : public Widget
{
public:
    void add_child(shared_ptr<Widget> child);
    void pop_child();
    size_t num_children() const { return m_children.size(); }
    shared_ptr<Widget> get_child(size_t idx) const { return m_children[idx]; };

    virtual void _render() override;
    virtual SizeReq _get_preferred_size(Direction dim, int prosp_width) override;
    virtual void _allocate_region() override;

protected:
    vector<shared_ptr<Widget>> m_children;
};

class Grid : public Widget
{
public:
    void add_child(shared_ptr<Widget> child, int x, int y);

    virtual void _render() override;
    virtual SizeReq _get_preferred_size(Direction dim, int prosp_width) override;
    virtual void _allocate_region() override;

protected:
    struct track_info {
        int size;
        int offset;
        SizeReq sr;
    };
    vector<track_info> m_col_info;
    vector<track_info> m_row_info;

    vector<i2> m_child_pos;
    vector<shared_ptr<Widget>> m_children;

    void layout_track(Direction dim, SizeReq sr, int size);
    void init_track_info();
    bool m_track_info_dirty = false;
};

void push_layout(shared_ptr<Widget> root);
void pop_layout();
void pump_events();

// XXX: this is a hack used to ensure that when switching to a
// layout-based UI, the starting window size is correct. This is necessary
// because there's no way to query TilesFramework for the current screen size
void resize(int w, int h);

}
