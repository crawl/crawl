/**
 * @file
 * @brief Hierarchical layout system.
**/

#pragma once

#include <array>
#include <functional>

#include "format.h"
#include "KeymapContext.h"
#include "state.h"
#include "tiledef-gui.h"
#include "tilefont.h"
#include "unwind.h"
#include "windowmanager.h"
#ifdef USE_TILE_LOCAL
# include "tilebuf.h"
# include "tiledgnbuf.h"
# include "tiledoll.h"
# include "tilesdl.h"
#endif
#ifdef USE_TILE_WEB
# include "tileweb.h"
#endif

namespace ui {

struct SizeReq
{
    int min, nat;
};

class Margin
{
public:
    constexpr Margin() : top(0), right(0), bottom(0), left(0) {};
    constexpr Margin(int v) : top(v), right(v), bottom(v), left(v) {};
    constexpr Margin(int v, int h) : top(v), right(h), bottom(v), left(h) {};
    constexpr Margin(int t, int lr, int b) : top(t), right(lr), bottom(b), left(lr) {};
    constexpr Margin(int t, int r, int b, int l) : top(t), right(r), bottom(b), left(l) {};

    int top, right, bottom, left;
};

class Region
{
public:
    constexpr Region() : x(0), y(0), width(0), height(0) {};
    constexpr Region(int _x, int _y, int _width, int _height) : x(_x), y(_y), width(_width), height(_height) {};

    constexpr bool operator == (const Region& other) const
    {
        return x == other.x && y == other.y
            && width == other.width && height == other.height;
    }

    constexpr bool empty() const
    {
        return width == 0 || height == 0;
    }

    constexpr int ex() const { return x + width; }
    constexpr int ey() const { return y + height; }

    int x, y, width, height;
};

class Size
{
public:
    constexpr Size() : width(0), height(0) {};
    constexpr Size(int v) : width(v), height(v) {};
    constexpr Size(int w, int h) : width(w), height(h) {};

    constexpr bool operator <= (const Size& other) const
    {
        return width <= other.width && height <= other.height;
    }

    constexpr bool operator == (const Size& other) const
    {
        return width == other.width && height == other.height;
    }

    int width, height;
};

template<typename, typename> class Slot;

template<class Target, class... Args>
class Slot<Target, bool (Args...)>
{
public:
    ~Slot() { alive = false; }
    typedef function<bool (Args...)> HandlerSig;
    typedef multimap<Target*, HandlerSig> HandlerMap;
    bool emit(Target *target, Args&... args)
    {
        auto i = handlers.equal_range(target);
        for (auto it = i.first; it != i.second; ++it)
        {
            HandlerSig func = it->second;
            if (func(forward<Args>(args)...))
                return true;
        }
        return false;
    }
    void on(Target *target, HandlerSig handler)
    {
        auto new_pair = pair<Target*, HandlerSig>(target, handler);
        handlers.insert(new_pair);
    }
    void remove_by_target(Target *target)
    {
        if (alive)
            handlers.erase(target);
    }
protected:
    bool alive {true};
    HandlerMap handlers;
};

class Widget : public enable_shared_from_this<Widget>
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

    virtual ~Widget();

    int flex_grow = 1;
    Align align_self = UNSET;
    bool expand_h = false, expand_v = false;
    bool shrink_h = false, shrink_v = false;
    Region get_region() const { return m_region; }

    Size& min_size() { _invalidate_sizereq(); return m_min_size; }
    Size& max_size() { _invalidate_sizereq(); return m_max_size; }

    virtual void _render() = 0;
    virtual SizeReq _get_preferred_size(Direction dim, int prosp_width);
    virtual void _allocate_region();
    void _set_parent(Widget* p);
    Widget* _get_parent() const { return m_parent; };
    shared_ptr<Widget> get_shared() {
        return shared_from_this();
    };
    void _invalidate_sizereq(bool immediate = true);
    void _queue_allocation(bool immediate = true);
    void set_allocation_needed() { alloc_queued = true; };
    void _expose();

    bool is_visible() const { return m_visible; }
    void set_visible(bool);

    // Wrapper functions which handle common behavior
    // - margins
    // - caching
    void render();
    SizeReq get_preferred_size(Direction dim, int prosp_width);
    void allocate_region(Region region);

    Margin get_margin() const {
        return margin;
    }

    template<class... Args>
    void set_margin_for_crt(Args&&... args)
    {
#ifndef USE_TILE_LOCAL
        margin = Margin(forward<Args>(args)...);
        _invalidate_sizereq();
#endif
    }

    template<class... Args>
    void set_margin_for_sdl(Args&&... args)
    {
#ifdef USE_TILE_LOCAL
        margin = Margin(forward<Args>(args)...);
        _invalidate_sizereq();
#endif
    }

    virtual bool on_event(const wm_event& event);

    template<class T, class... Args, typename F>
    void on(Slot<T, bool (Args...)>& slot, F&& cb)
    {
        slot.on(this, cb);
    }
    static struct slots {
        Slot<Widget, bool (const wm_event&)> event;
    } slots;

    // XXX: add documentation
    virtual shared_ptr<Widget> get_child_at_offset(int x, int y) {
        return nullptr;
    };

protected:
    Region m_region;
    Margin margin = { 0 };

    void _unparent(shared_ptr<Widget>& child);

private:
    bool cached_sr_valid[2] = { false, false };
    SizeReq cached_sr[2];
    int cached_sr_pw;
    bool alloc_queued = false;
    bool m_visible = true;
    Widget* m_parent = nullptr;

    Size m_min_size = { 0 };
    Size m_max_size = { INT_MAX };
};

class Container : public Widget
{
public:
    virtual ~Container() {}
    virtual void foreach(function<void(shared_ptr<Widget>&)> f) = 0;
};

class Bin : public Container
{
public:
    virtual ~Bin() {
        if (m_child)
            _unparent(m_child);
    };
    void set_child(shared_ptr<Widget> child);
    virtual shared_ptr<Widget> get_child() { return m_child; };
    virtual shared_ptr<Widget> get_child_at_offset(int x, int y) override;

protected:
    class iterator
    {
    public:
        typedef shared_ptr<Widget> value_type;
        typedef ptrdiff_t distance;
        typedef shared_ptr<Widget>* pointer;
        typedef shared_ptr<Widget>& reference;
        typedef input_iterator_tag iterator_category;

        value_type c;
        bool state;

        iterator(value_type& _c, bool _state) : c(_c), state(_state) {};
        void operator++ () { state = true; };
        value_type& operator* () { return c; };
        bool operator== (const iterator& other) { return c == other.c && state == other.state; }
        bool operator!= (const iterator& other) { return !(*this == other); }
    };

public:
    iterator begin() { return iterator(m_child, false); }
    iterator end() { return iterator(m_child, true); }
    virtual void foreach(function<void(shared_ptr<Widget>&)> f) override
    {
        for (auto& child : *this)
            f(child);
    }

protected:
    shared_ptr<Widget> m_child;
};

class ContainerVec : public Container
{
public:
    virtual ~ContainerVec() {
        for (auto& child : m_children)
            if (child)
                _unparent(child);
    }
    virtual shared_ptr<Widget> get_child_at_offset(int x, int y) override;
    size_t num_children() const { return m_children.size(); }
    shared_ptr<Widget>& operator[](size_t pos) { return m_children[pos]; };
    const shared_ptr<Widget>& operator[](size_t pos) const { return m_children[pos]; };

protected:
    class iterator
    {
    public:
        typedef shared_ptr<Widget> value_type;
        typedef ptrdiff_t distance;
        typedef shared_ptr<Widget>* pointer;
        typedef shared_ptr<Widget>& reference;
        typedef input_iterator_tag iterator_category;

        vector<value_type>& c;
        vector<value_type>::iterator it;

        iterator(vector<value_type>& _c, vector<value_type>::iterator _it) : c(_c), it(_it) {};
        void operator++ () { ++it; };
        value_type& operator* () { return *it; };
        bool operator== (const iterator& other) { return c == other.c && it == other.it; }
        bool operator!= (const iterator& other) { return !(*this == other); }
    };

public:
    iterator begin() { return iterator(m_children, m_children.begin()); }
    iterator end() { return iterator(m_children, m_children.end()); }
    virtual void foreach(function<void(shared_ptr<Widget>&)> f) override
    {
        for (auto& child : *this)
            f(child);
    }

protected:
    vector<shared_ptr<Widget>> m_children;
};

// Box widget: similar to the CSS flexbox (without wrapping)
//  - Lays its children out in either a row or a column
//  - Extra space is allocated according to each child's flex_grow property

class Box : public ContainerVec
{
public:
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
    virtual ~Box() {}
    void add_child(shared_ptr<Widget> child);
    bool horz;
    Widget::Align align_main = START;
    Widget::Align align_cross = UNSET;

    virtual void _render() override;
    virtual SizeReq _get_preferred_size(Direction dim, int prosp_width) override;
    virtual void _allocate_region() override;

protected:
    vector<int> layout_main_axis(vector<SizeReq>& ch_psz, int main_sz);
    vector<int> layout_cross_axis(vector<SizeReq>& ch_psz, int cross_sz);
};

class Text : public Widget
{
public:
    Text();
    Text(string text) : Text()
    {
        set_text(formatted_string(text));
    }
    Text(formatted_string text) : Text()
    {
        set_text(text);
    }
    virtual ~Text() {}

    void set_text(const formatted_string &fs);
    void set_text(string text)
    {
        set_text(formatted_string(text));
    };

    void set_font(FontWrapper *font);

    const formatted_string& get_text() { return m_text; };
    void set_highlight_pattern(string pattern, bool hl_line = false);

    virtual void _render() override;
    virtual SizeReq _get_preferred_size(Direction dim, int prosp_width) override;
    virtual void _allocate_region() override;

    bool wrap_text = false;
    bool ellipsize = false;

#ifndef USE_TILE_LOCAL
    void set_bg_colour(COLOURS colour);
#endif

protected:
    void wrap_text_to_size(int width, int height);

    formatted_string m_text;
#ifdef USE_TILE_LOCAL
    struct brkpt { unsigned int op, line; };
    vector<brkpt> m_brkpts;
    formatted_string m_text_wrapped;
    ShapeBuffer m_hl_buf;
    FontWrapper *m_font;
#else
    vector<formatted_string> m_wrapped_lines;
    COLOURS m_bg_colour = BLACK;
#endif
    Size m_wrapped_size = { -1 };
    string hl_pat;
    bool hl_line;
};

class Image : public Widget
{
public:
    Image() {};
    Image(tile_def tile) { set_tile(tile); };
    virtual ~Image() {}
    void set_tile(tile_def tile);
    tile_def get_tile() const { return m_tile; };

    virtual void _render() override;
    virtual SizeReq _get_preferred_size(Direction dim, int prosp_width) override;

protected:
    tile_def m_tile = {TILEG_ERROR, TEX_GUI};
    int m_tw {0}, m_th {0};

#ifdef USE_TILE_LOCAL
    GenericTexture m_img;
#endif
};

class Stack : public ContainerVec
{
public:
    virtual ~Stack() {};
    void add_child(shared_ptr<Widget> child);
    void pop_child();
    shared_ptr<Widget> get_child(size_t idx) const { return m_children[idx]; };
    virtual shared_ptr<Widget> get_child_at_offset(int x, int y) override;

    virtual void _render() override;
    virtual SizeReq _get_preferred_size(Direction dim, int prosp_width) override;
    virtual void _allocate_region() override;
};

class Switcher : public ContainerVec
{
public:
    virtual ~Switcher() {};
    void add_child(shared_ptr<Widget> child);
    int& current();

    Widget::Align align_x = START, align_y = START;

    virtual void _render() override;
    virtual SizeReq _get_preferred_size(Direction dim, int prosp_width) override;
    virtual void _allocate_region() override;
    virtual shared_ptr<Widget> get_child_at_offset(int x, int y) override;

protected:
    int m_current;
};

class Grid : public Container
{
public:
    virtual ~Grid() {
        for (auto& child : m_child_info)
            if (child.widget)
                _unparent(child.widget);
    };
    void add_child(shared_ptr<Widget> child, int x, int y, int w = 1, int h = 1);
    const int column_flex_grow(int x) const { return m_col_info.at(x).flex_grow; }
    const int row_flex_grow(int y) const { return m_row_info.at(y).flex_grow; }
    int& column_flex_grow(int x)
    {
        init_track_info();
        return m_col_info.at(x).flex_grow;
    }
    int& row_flex_grow(int y)
    {
        init_track_info();
        return m_row_info.at(y).flex_grow;
    }
    virtual shared_ptr<Widget> get_child_at_offset(int x, int y) override;

    virtual void _render() override;
    virtual SizeReq _get_preferred_size(Direction dim, int prosp_width) override;
    virtual void _allocate_region() override;

    bool stretch_h = false, stretch_v = false;

protected:
    Region get_tracks_region(int x, int y, int w, int h) const
    {
        Region track_region;
        track_region.x = m_col_info[x].offset;
        track_region.y = m_row_info[y].offset;
        track_region.width = m_col_info[x+w-1].size + m_col_info[x+w-1].offset - m_col_info[x].offset;
        track_region.height = m_row_info[y+h-1].size + m_row_info[y+h-1].offset - m_row_info[y].offset;
        return track_region;
    }

    struct track_info {
        int size;
        int offset;
        SizeReq sr;
        int flex_grow = 1;
    };
    vector<track_info> m_col_info;
    vector<track_info> m_row_info;

    struct child_info {
        struct {
            int x, y;
        } pos;
        Size span;
        shared_ptr<Widget> widget;
        inline bool operator==(const child_info& rhs) const { return widget == rhs.widget; }
    };
    vector<child_info> m_child_info;

    void layout_track(Direction dim, SizeReq sr, int size);
    void set_track_offsets(vector<track_info>& tracks);
    void compute_track_sizereqs(Direction dim);
    void init_track_info();
    bool m_track_info_dirty = false;

protected:
    class iterator
    {
    public:
        typedef shared_ptr<Widget> value_type;
        typedef ptrdiff_t distance;
        typedef shared_ptr<Widget>* pointer;
        typedef shared_ptr<Widget>& reference;
        typedef input_iterator_tag iterator_category;

        vector<child_info>& c;
        vector<child_info>::iterator it;

        iterator(vector<child_info>& _c, vector<child_info>::iterator _it) : c(_c), it(_it) {};
        void operator++ () { ++it; };
        value_type& operator* () { return it->widget; };
        bool operator== (const iterator& other) { return c == other.c && it == other.it; }
        bool operator!= (const iterator& other) { return !(*this == other); }
    };

public:
    iterator begin() { return iterator(m_child_info, m_child_info.begin()); }
    iterator end() { return iterator(m_child_info, m_child_info.end()); }
    virtual void foreach(function<void(shared_ptr<Widget>&)> f) override
    {
        for (auto& child : *this)
            f(child);
    }
};

class Scroller : public Bin
{
public:
    virtual ~Scroller() {};

    virtual void set_scroll(int y);
    int get_scroll() const { return m_scroll; };
    void set_scrollbar_visible(bool vis) { m_scrolbar_visible = vis; };
    virtual void _render() override;
    virtual SizeReq _get_preferred_size(Direction dim, int prosp_width) override;
    virtual void _allocate_region() override;
    virtual bool on_event(const wm_event& event) override;
protected:
    int m_scroll = 0;
    bool m_scrolbar_visible = true;
#ifdef USE_TILE_LOCAL
    VertBuffer m_shade_buf = VertBuffer(false, true);
    ShapeBuffer m_scrollbar_buf;
#endif
};

class Layout : public Bin
{
    friend struct UIRoot;
public:
    Layout(shared_ptr<Widget> child);
    virtual void _render() override;
    virtual SizeReq _get_preferred_size(Direction dim, int prosp_width) override;
    virtual void _allocate_region() override;

    void add_event_filter(function<bool (const wm_event&)> handler)
    {
        event_filters.on(this, handler);
    }
protected:
#ifdef USE_TILE_LOCAL
    int m_depth;
#endif
    Slot<Widget, bool (const wm_event&)> event_filters;
};

class Popup : public Layout
{
public:
    Popup(shared_ptr<Widget> child) : Layout(move(child)) {};
    virtual void _render() override;
    virtual SizeReq _get_preferred_size(Direction dim, int prosp_width) override;
    virtual void _allocate_region() override;

    Size get_max_child_size();

protected:
#ifdef USE_TILE_LOCAL
    ShapeBuffer m_buf;
    static constexpr int m_depth_indent = 20;
    static constexpr int m_padding = 23;

    int base_margin();
#endif
    bool m_centred{!crawl_state.need_save};
};

#ifdef USE_TILE_LOCAL
class Dungeon : public Widget
{
public:
    Dungeon() : width(0), height(0), m_buf((ImageManager*)tiles.get_image_manager()), m_dirty(false) {};
    virtual ~Dungeon() {};
    virtual void _render() override;
    virtual SizeReq _get_preferred_size(Direction dim, int prosp_width) override;

    unsigned width, height;
    DungeonCellBuffer& buf() { m_dirty = true; return m_buf; };

protected:
    DungeonCellBuffer m_buf;
    bool m_dirty;
};

class PlayerDoll : public Widget
{
public:
    PlayerDoll(dolls_data doll);
    virtual ~PlayerDoll();

    virtual void _render() override;
    virtual SizeReq _get_preferred_size(Direction dim, int prosp_width) override;
    virtual void _allocate_region() override;
    virtual bool on_event(const wm_event& event) override;

protected:
    void _pack_doll();
    dolls_data m_save_doll;

    vector<tile_def> m_tiles;
    FixedVector<TileBuffer, TEX_MAX> m_tile_buf;
};
#endif

#ifdef USE_TILE
void push_cutoff();
void pop_cutoff();

class cutoff_point
{
public:
    cutoff_point() { push_cutoff(); }
    ~cutoff_point() { pop_cutoff(); }
};
#endif

void push_layout(shared_ptr<Widget> root, KeymapContext km = KMC_DEFAULT);
void pop_layout();
shared_ptr<Widget> top_layout();
void pump_events(int wait_event_timeout = INT_MAX);
void run_layout(shared_ptr<Widget> root, const bool& done);
bool has_layout();
NORETURN void restart_layout();
int getch(KeymapContext km = KMC_DEFAULT);
void ui_force_render();
void ui_render();
void ui_delay(unsigned int ms);

void push_scissor(Region scissor);
void pop_scissor();
Region get_scissor();

void set_focused_widget(Widget* w);
Widget* get_focused_widget();

// XXX: this is a hack used to ensure that when switching to a
// layout-based UI, the starting window size is correct. This is necessary
// because there's no way to query TilesFramework for the current screen size
void resize(int w, int h);

bool is_available();

class progress_popup
{
public:
    progress_popup(string title, int width);
    ~progress_popup();
    void set_status_text(string status);
    void advance_progress();
    void force_redraw();
private:
    shared_ptr<Popup> contents;
    shared_ptr<Text> progress_bar;
    shared_ptr<Text> status_text;
    unsigned int position;
    unsigned int bar_width;
    formatted_string get_progress_string(unsigned int len);
    unwind_bool no_more;
};

}
