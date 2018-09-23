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
#include "tilefont.h"
#include "tiledef-gui.h"
#include "windowmanager.h"
#ifdef USE_TILE_LOCAL
# include "tilesdl.h"
# include "tilebuf.h"
# include "tiledgnbuf.h"
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

struct RestartAllocation {};

template<typename, typename> class Slot;

template<class Target, class... Args>
class Slot<Target, bool (Args...)>
{
public:
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
        handlers.erase(target);
    }
protected:
    HandlerMap handlers;
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

    virtual ~Widget() {
        Widget::slots.event.remove_by_target(this);
        _set_parent(nullptr);
    }

    i4 margin = {0,0,0,0};
    int flex_grow = 1;
    Align align_self = UNSET;
    bool expand_h = false, expand_v = false;
    bool shrink_h = false, shrink_v = false;
    const i4 get_region() const { return m_region; }
    i2& min_size() { _invalidate_sizereq(); return m_min_size; }
    i2& max_size() { _invalidate_sizereq(); return m_max_size; }

    virtual void _render() = 0;
    virtual SizeReq _get_preferred_size(Direction dim, int prosp_width);
    virtual void _allocate_region();
    void _set_parent(Widget* p);
    void _invalidate_sizereq(bool immediate = true);
    void _queue_allocation(bool immediate = true);
    void set_allocation_needed() { alloc_queued = true; };
    void _expose();

    // Wrapper functions which handle common behavior
    // - margins
    // - caching
    void render();
    SizeReq get_preferred_size(Direction dim, int prosp_width);
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

    virtual bool on_event(const wm_event& event);

    template<class T, class... Args, typename F>
    void on(Slot<T, bool (Args...)>& slot, F&& cb)
    {
        slot.on(this, cb);
    }
    static struct slots {
        Slot<Widget, bool (const wm_event&)> event;
    } slots;

    virtual shared_ptr<Widget> get_child_at_offset(int x, int y) {
        return nullptr;
    };

protected:
    i4 m_region;

private:
    bool cached_sr_valid[2] = { false, false };
    SizeReq cached_sr[2];
    int cached_sr_pw;
    bool alloc_queued = false;
    Widget* m_parent = nullptr;
    i2 m_min_size = { 0, 0 }, m_max_size = { INT_MAX, INT_MAX };
};

class Container : public Widget
{
protected:
    class iter_impl
    {
    public:
        virtual ~iter_impl() {};
        virtual void operator++() = 0;
        virtual shared_ptr<Widget>& operator*() = 0;
        virtual iter_impl* clone() const = 0;
        virtual bool equal(iter_impl &other) const = 0;
    };

public:
    virtual ~Container() {}
    class iterator : public std::iterator<input_iterator_tag, shared_ptr<Widget>>
    {
    public:
        iterator(iter_impl *_it) : it(_it) {};
        ~iterator() { delete it; it = nullptr; };
        iterator(const iterator& other) : it(other.it->clone()) {};
        iterator& operator=(const iterator& other)
        {
            if (it != other.it)
            {
                delete it;
                it = other.it->clone();
            }
            return *this;
        }
        iterator& operator=(iterator&& other)
        {
            if (it != other.it)
            {
                delete it;
                it = other.it;
                other.it = nullptr;
            }
            return *this;
        }

        void operator++() { ++(*it); };
        bool operator==(const iterator& other) const
        {
            return typeid(it) == typeid(other.it) && it->equal(*other.it);
        };
        bool operator!=(const iterator& other) const { return !(*this == other); }
        shared_ptr<Widget>& operator*() { return **it; };
    protected:
        iter_impl *it;
    };

    virtual bool on_event(const wm_event& event) override;

public:
    virtual iterator begin() = 0;
    virtual iterator end() = 0;
};

class Bin : public Container
{
public:
    virtual ~Bin() {
        if (m_child)
            m_child->_set_parent(nullptr);
    };
    virtual bool on_event(const wm_event& event) override;
    void set_child(shared_ptr<Widget> child);
    virtual shared_ptr<Widget> get_child() { return m_child; };
    virtual shared_ptr<Widget> get_child_at_offset(int x, int y) override;

private:
    typedef Container::iterator I;

    class iter_impl_bin : public iter_impl
    {
    private:
        typedef shared_ptr<Widget> C;
    public:
        explicit iter_impl_bin (C& _c, bool _state) : c(_c), state(_state) {};
    protected:
        virtual void operator++() override { state = true; };
        virtual shared_ptr<Widget>& operator*() override { return c; };
        virtual iter_impl_bin* clone() const override { return new iter_impl_bin(c, state); };
        virtual bool equal (iter_impl &_other) const override {
            iter_impl_bin &other = static_cast<iter_impl_bin&>(_other);
            return c == other.c && state == other.state;
        };

        C& c;
        bool state;
    };

public:
    virtual I begin() override { return I(new iter_impl_bin(m_child, false)); }
    virtual I end() override { return I(new iter_impl_bin(m_child, true)); }
protected:
    shared_ptr<Widget> m_child;
};

class ContainerVec : public Container
{
public:
    virtual ~ContainerVec() {
        for (auto& child : m_children)
            if (child)
                child->_set_parent(nullptr);
    }
    virtual shared_ptr<Widget> get_child_at_offset(int x, int y) override;
    size_t num_children() const { return m_children.size(); }
private:
    typedef Container::iterator I;

    class iter_impl_vec : public iter_impl
    {
    private:
        typedef vector<shared_ptr<Widget>> C;
    public:
        explicit iter_impl_vec (C& _c, C::iterator _it) : c(_c), it(_it) {};
    protected:
        virtual void operator++() override { ++it; };
        virtual shared_ptr<Widget>& operator*() override { return *it; };
        virtual iter_impl_vec* clone() const override { return new iter_impl_vec(c, it); };
        virtual bool equal (iter_impl &_other) const override {
            iter_impl_vec &other = static_cast<iter_impl_vec&>(_other);
            return c == other.c && it == other.it;
        };

        C& c;
        C::iterator it;
    };

public:
    virtual I begin() override { return I(new iter_impl_vec(m_children, m_children.begin())); }
    virtual I end() override { return I(new iter_impl_vec(m_children, m_children.end())); }
protected:
    vector<shared_ptr<Widget>> m_children;
};

// Box widget: similar to the CSS flexbox (without wrapping)
//  - Lays its children out in either a row or a column
//  - Extra space is allocated according to each child's flex_grow property
//  - align and justify properties work like flexbox's

class Box : public ContainerVec
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
    virtual ~Box() {}
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
};

class Text : public Widget
{
public:
    Text() {}
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

    const formatted_string& get_text() { return m_text; };
    void set_highlight_pattern(string pattern, bool hl_line = false);

    virtual void _render() override;
    virtual SizeReq _get_preferred_size(Direction dim, int prosp_width) override;
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
    ShapeBuffer m_hl_buf;
#else
    vector<formatted_string> m_wrapped_lines;
#endif
    i2 m_wrapped_size = { -1, -1 };
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

    virtual void _render() override;
    virtual SizeReq _get_preferred_size(Direction dim, int prosp_width) override;

protected:
    tile_def m_tile = {TILEG_ERROR, TEX_GUI};
    int m_tw, m_th;

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
    virtual bool on_event(const wm_event& event) override;
};

class Switcher : public ContainerVec
{
public:
    virtual ~Switcher() {};
    void add_child(shared_ptr<Widget> child);
    int& current();

    virtual void _render() override;
    virtual SizeReq _get_preferred_size(Direction dim, int prosp_width) override;
    virtual void _allocate_region() override;
    virtual bool on_event(const wm_event& event) override;

protected:
    int m_current;
};

class Grid : public Container
{
public:
    virtual ~Grid() {
        for (auto& child : m_child_info)
            if (child.widget)
                child.widget->_set_parent(nullptr);
    };
    void add_child(shared_ptr<Widget> child, int x, int y, int w = 1, int h = 1);
    const int column_flex_grow(int x) const { return m_col_info[x].flex_grow; }
    const int row_flex_grow(int y) const { return m_row_info[y].flex_grow; }
    int& column_flex_grow(int x)
    {
        init_track_info();
        return m_col_info[x].flex_grow;
    }
    int& row_flex_grow(int y)
    {
        init_track_info();
        return m_row_info[y].flex_grow;
    }
    virtual shared_ptr<Widget> get_child_at_offset(int x, int y) override;

    virtual void _render() override;
    virtual SizeReq _get_preferred_size(Direction dim, int prosp_width) override;
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
        SizeReq sr;
        int flex_grow = 1;
    };
    vector<track_info> m_col_info;
    vector<track_info> m_row_info;

    struct child_info {
        i2 pos;
        i2 span;
        shared_ptr<Widget> widget;
        inline bool operator==(const child_info& rhs) const { return widget == rhs.widget; }
    };
    vector<child_info> m_child_info;

    void layout_track(Direction dim, SizeReq sr, int size);
    void set_track_offsets(vector<track_info>& tracks);
    void compute_track_sizereqs(Direction dim);
    void init_track_info();
    bool m_track_info_dirty = false;

private:
    typedef Container::iterator I;

    class iter_impl_grid : public iter_impl
    {
    private:
        typedef vector<child_info> C;
    public:
        explicit iter_impl_grid (C& _c, C::iterator _it) : c(_c), it(_it) {};
    protected:
        virtual void operator++() override { ++it; };
        virtual shared_ptr<Widget>& operator*() override { return it->widget; };
        virtual iter_impl_grid* clone() const override {
            return new iter_impl_grid(c, it);
        };
        virtual bool equal (iter_impl &_other) const override {
            iter_impl_grid &other = static_cast<iter_impl_grid&>(_other);
            return c == other.c && it == other.it;
        };

        C& c;
        C::iterator it;
    };

public:
    virtual I begin() override { return I(new iter_impl_grid(m_child_info, m_child_info.begin())); }
    virtual I end() override { return I(new iter_impl_grid(m_child_info, m_child_info.end())); }
};

class Scroller : public Bin
{
public:
    virtual ~Scroller() {};

    virtual void set_scroll(int y);
    int get_scroll() const { return m_scroll; };
    virtual void _render() override;
    virtual SizeReq _get_preferred_size(Direction dim, int prosp_width) override;
    virtual void _allocate_region() override;
    virtual bool on_event(const wm_event& event) override;
protected:
    int m_scroll = 0;
#ifdef USE_TILE_LOCAL
    VertBuffer m_shade_buf = VertBuffer(false, true);
    ShapeBuffer m_scrollbar_buf;
#endif
};

class Popup : public Bin
{
public:
    Popup(shared_ptr<Widget> child);
    virtual void _render() override;
    virtual SizeReq _get_preferred_size(Direction dim, int prosp_width) override;
    virtual void _allocate_region() override;

    i2 get_max_child_size();

protected:
#ifdef USE_TILE_LOCAL
    ShapeBuffer m_buf;
    int m_depth;
    static constexpr int m_depth_indent = 20;
    static constexpr int m_base_margin = 50;
    static constexpr int m_padding = 23;
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
int getch(KeymapContext km = KMC_DEFAULT);
void ui_force_render();
void ui_delay(unsigned int ms);

void push_scissor(i4 scissor);
void pop_scissor();
i4 get_scissor();

// XXX: this is a hack used to ensure that when switching to a
// layout-based UI, the starting window size is correct. This is necessary
// because there's no way to query TilesFramework for the current screen size
void resize(int w, int h);

bool is_available();

}
