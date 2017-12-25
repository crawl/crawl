/**
 * @file
 * @brief Hierarchical layout system.
**/

#pragma once

#include <functional>

#include "format.h"
#include "tilefont.h"
#include "tiledef-gui.h"
#ifdef USE_TILE_LOCAL
# include "tilesdl.h"
# include "tilebuf.h"
#endif

struct i4 {
    int xyzw[4];
    template <typename... Ts> i4 (Ts... l) : xyzw{l...} {}
    const int& operator[](int index) const { return xyzw[index]; }
    int& operator[](int index) { return xyzw[index]; }
    inline bool operator==(const i4& rhs) { return equal(begin(xyzw), end(xyzw), begin(rhs.xyzw)); }
};

struct i2 {
    int xy[2];
    template <typename... Ts> i2 (Ts... l) : xy{l...} {}
    const int& operator[](int index) const { return xy[index]; }
    int& operator[](int index) { return xy[index]; }
    inline bool operator==(const i2& rhs) { return equal(begin(xy), end(xy), begin(rhs.xy)); }
};

struct UISizeReq
{
    int min, nat;
};

typedef enum {
    UI_ALIGN_UNSET = 0,
    UI_ALIGN_START,
    UI_ALIGN_END,
    UI_ALIGN_CENTER,
    UI_ALIGN_STRETCH,
} UIAlign_type;

typedef enum {
    UI_JUSTIFY_START = 0,
    UI_JUSTIFY_CENTER,
    UI_JUSTIFY_END,
} UIJustify_type;

class UI
{
public:
    UI() : margin({0,0,0,0}), flex_grow(0), align_self(UI_ALIGN_UNSET), expand_h(false), expand_v(false), cached_sr_valid{false, false} {};

    i4 margin;
    int flex_grow;
    UIAlign_type align_self;
    bool expand_h, expand_v;

    virtual void _render() = 0;
    virtual UISizeReq _get_preferred_size(int dim, int prosp_width);
    virtual void _allocate_region();

    // Wrapper functions which handle common behavior
    // - margins
    // - debug drawing
    // - caching
    void render();
    UISizeReq get_preferred_size(int dim, int prosp_width);
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
    bool cached_sr_valid[2];
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
    UIBox() : horz(false), justify_items(UI_JUSTIFY_START), align_items(UI_ALIGN_UNSET) {
        expand_h = expand_v = true;
    };
    void add_child(shared_ptr<UI> child);
    bool horz;
    UIJustify_type justify_items;
    UIAlign_type align_items;

    virtual void _render() override;
    virtual UISizeReq _get_preferred_size(int dim, int prosp_width) override;
    virtual void _allocate_region() override;

protected:
    vector<int> layout_main_axis(vector<UISizeReq>& ch_psz, int main_sz);
    vector<int> layout_cross_axis(vector<UISizeReq>& ch_psz, int cross_sz);

    vector<shared_ptr<UI>> m_children;
};

class UIText : public UI
{
public:
    UIText() : wrap_text(false), ellipsize(false)
#ifdef USE_TILE_LOCAL
    , m_font_buf(tiles.get_crt_font()), m_wrapped_size{ -1, -1 }
#endif
    {}
    UIText(string text) : UIText()
    {
        set_text(formatted_string::parse_string(text));
    }

    void set_text(const formatted_string &fs);

    virtual void _render() override;
    virtual UISizeReq _get_preferred_size(int dim, int prosp_width) override;
    virtual void _allocate_region() override;

    bool wrap_text, ellipsize;

protected:
    void wrap_text_to_size(int width, int height);

    formatted_string m_text;
#ifdef USE_TILE_LOCAL
    formatted_string m_text_wrapped;
    FontBuffer m_font_buf;
#else
    vector<formatted_string> m_wrapped_lines;
#endif
    i2 m_wrapped_size;
};

class UIImage : public UI
{
public:
    UIImage() : shrink_h(false), shrink_v(false), m_tile(TILEG_ERROR, TEX_GUI) {};
    void set_tile(tile_def tile);
    void set_file(string img_path);

    bool shrink_h, shrink_v;

    virtual void _render() override;
    virtual UISizeReq _get_preferred_size(int dim, int prosp_width) override;

protected:
    bool m_using_tile;

    tile_def m_tile;
    int m_tw, m_th;

#ifdef USE_TILE_LOCAL
    GenericTexture m_img;
#endif
};

class UIStack : public UI
{
public:
    void add_child(shared_ptr<UI> child);

    virtual void _render() override;
    virtual UISizeReq _get_preferred_size(int dim, int prosp_width) override;
    virtual void _allocate_region() override;

protected:
    vector<shared_ptr<UI>> m_children;
};

class UIGrid : public UI
{
public:
    UIGrid() : m_track_info_dirty(false) {};

    void add_child(shared_ptr<UI> child, int x, int y, int w = 1, int h = 1);
    int& track_flex_grow(int x, int y)
    {
        init_track_info();
        ASSERT(x == -1 || y == -1);
        return (x >= 0 ? m_col_info[x].flex_grow : m_row_info[y].flex_grow);
    }

    virtual void _render() override;
    virtual UISizeReq _get_preferred_size(int dim, int prosp_width) override;
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

    void layout_track(int dim, UISizeReq sr, int size);
    void set_track_offsets(vector<track_info>& tracks);
    void compute_track_sizereqs(int dim);
    void init_track_info();
    bool m_track_info_dirty;
};

void ui_push_layout(shared_ptr<UI> root);
void ui_pop_layout();
void ui_pump_events();

// XXX: this is a hack used to ensure that when switching to a
// layout-based UI, the starting window size is correct. This is necessary
// because there's no way to query TilesFramework for the current screen size
void ui_resize(int w, int h);
