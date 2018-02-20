/**
 * @file
 * @brief Hierarchical layout system.
**/

#pragma once

#include <array>
#include <functional>

#include "format.h"
#include "KeymapContext.h"
#include "tilefont.h"
#include "tiledef-gui.h"
#include "windowmanager.h"
#ifdef USE_TILE_LOCAL
# include "tilesdl.h"
# include "tilebuf.h"
# include "tiledgnbuf.h"
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

    virtual ~UI() {
        UI::slots.event.remove_by_target(this);
    }

    i4 margin = {0,0,0,0};
    int flex_grow = 1;
    Align align_self = UNSET;
    bool expand_h = false, expand_v = false;
    const i4 get_region() const { return m_region; }
    i2& min_size() { _invalidate_sizereq(); return m_min_size; }
    i2& max_size() { _invalidate_sizereq(); return m_max_size; }

    virtual void _render() = 0;
    virtual UISizeReq _get_preferred_size(Direction dim, int prosp_width);
    virtual void _allocate_region();
    void _set_parent(UI* p);
    void _invalidate_sizereq();
    void _queue_allocation();

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

    virtual bool on_event(wm_event event);

    template<class T, class... Args, typename F>
    void on(Slot<T, bool (Args...)>& slot, F&& cb)
    {
        slot.on(this, cb);
    }
    static struct slots {
        Slot<UI, bool (wm_event)> event;
    } slots;

protected:
    i4 m_region;

private:
    bool cached_sr_valid[2] = { false, false };
    UISizeReq cached_sr[2];
    int cached_sr_pw;
    bool alloc_queued = false;
    UI* m_parent = nullptr;
    i2 m_min_size = { 0, 0 }, m_max_size = { INT_MAX, INT_MAX };
};

class UIContainer : public UI
{
protected:
    class iter_impl
    {
    public:
        virtual ~iter_impl() {};
        virtual void operator++() = 0;
        virtual shared_ptr<UI>& operator*() = 0;
        virtual bool equal (iter_impl &other) const = 0;
    };

public:
    virtual ~UIContainer() {}
    class iterator
    {
    public:
        iterator(iter_impl *_it) : it(_it) {};
        ~iterator() { delete it; };
        void operator++() { ++(*it); };
        bool operator==(iterator &other) const {
            return typeid(it) == typeid(other.it) && it->equal(*other.it);
        };
        bool operator!=(iterator &other) const { return !(*this == other); }
        shared_ptr<UI>& operator*() { return **it; };
    protected:
        iter_impl *it;
    };

    virtual bool on_event(wm_event event) override;

protected:
    virtual iterator begin() = 0;
    virtual iterator end() = 0;
};

class UIBin : public UI
{
public:
    virtual ~UIBin() {}
    virtual bool on_event(wm_event event) override;
    virtual shared_ptr<UI> get_child() { return m_child; };
protected:
    shared_ptr<UI> m_child;
};

class UIContainerVec : public UIContainer
{
public:
    virtual ~UIContainerVec() {}
private:
    typedef UIContainer::iterator I;

    class iter_impl_vec : public iter_impl
    {
    private:
        typedef vector<shared_ptr<UI>> C;
    public:
        explicit iter_impl_vec (C& _c, C::iterator _it) : c(_c), it(_it) {};
    protected:
        virtual void operator++() override { ++it; };
        virtual shared_ptr<UI>& operator*() override { return *it; };
        virtual bool equal (iter_impl &_other) const override {
            iter_impl_vec &other = static_cast<iter_impl_vec&>(_other);
            return c == other.c && it == other.it;
        };

        C& c;
        C::iterator it;
    };

protected:
    virtual I begin() override { return I(new iter_impl_vec(m_children, m_children.begin())); }
    virtual I end() override { return I(new iter_impl_vec(m_children, m_children.end())); }
    vector<shared_ptr<UI>> m_children;
};

// Box widget: similar to the CSS flexbox (without wrapping)
//  - Lays its children out in either a row or a column
//  - Extra space is allocated according to each child's flex_grow property
//  - align and justify properties work like flexbox's

class UIBox : public UIContainerVec
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
    virtual ~UIBox() {}
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
};

class UIText : public UI
{
public:
    UIText() {}
    UIText(string text) : UIText()
    {
        set_text(formatted_string::parse_string(text));
    }
    virtual ~UIText() {}

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
    virtual ~UIImage() {}
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

class UIStack : public UIContainerVec
{
public:
    virtual ~UIStack() {};
    void add_child(shared_ptr<UI> child);
    void pop_child();
    size_t num_children() const { return m_children.size(); }
    shared_ptr<UI> get_child(size_t idx) const { return m_children[idx]; };

    virtual void _render() override;
    virtual UISizeReq _get_preferred_size(Direction dim, int prosp_width) override;
    virtual void _allocate_region() override;
    virtual bool on_event(wm_event event) override;
};

class UISwitcher : public UIContainerVec
{
public:
    virtual ~UISwitcher() {};
    void add_child(shared_ptr<UI> child);
    size_t num_children() const { return m_children.size(); }
    int& current();

    virtual void _render() override;
    virtual UISizeReq _get_preferred_size(Direction dim, int prosp_width) override;
    virtual void _allocate_region() override;
    virtual bool on_event(wm_event event) override;

protected:
    int m_current;
};

class UIGrid : public UIContainer
{
public:
    virtual ~UIGrid() {};
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
        shared_ptr<UI> widget;
        inline bool operator==(const child_info& rhs) const { return widget == rhs.widget; }
    };
    vector<child_info> m_child_info;

    void layout_track(Direction dim, UISizeReq sr, int size);
    void set_track_offsets(vector<track_info>& tracks);
    void compute_track_sizereqs(Direction dim);
    void init_track_info();
    bool m_track_info_dirty = false;

private:
    typedef UIContainer::iterator I;

    class iter_impl_grid : public iter_impl
    {
    private:
        typedef vector<child_info> C;
    public:
        explicit iter_impl_grid (C& _c, C::iterator _it) : c(_c), it(_it) {};
    protected:
        virtual void operator++() override { ++it; };
        virtual shared_ptr<UI>& operator*() override { return it->widget; };
        virtual bool equal (iter_impl &_other) const override {
            iter_impl_grid &other = static_cast<iter_impl_grid&>(_other);
            return c == other.c && it == other.it;
        };

        C c;
        C::iterator it;
    };

public:
    virtual I begin() override { return I(new iter_impl_grid(m_child_info, m_child_info.begin())); }
    virtual I end() override { return I(new iter_impl_grid(m_child_info, m_child_info.end())); }
};

class UIScroller : public UIBin
{
public:
    virtual ~UIScroller() {};

    void set_child(shared_ptr<UI> child);
    virtual void set_scroll(int y);
    int get_scroll() const { return m_scroll; };
    virtual void _render() override;
    virtual UISizeReq _get_preferred_size(Direction dim, int prosp_width) override;
    virtual void _allocate_region() override;
    virtual bool on_event(wm_event event) override;
protected:
    int m_scroll = 0;
};

class UIPopup : public UIBin
{
public:
    UIPopup(shared_ptr<UI> child);
    virtual void _render() override;
    virtual UISizeReq _get_preferred_size(Direction dim, int prosp_width) override;
    virtual void _allocate_region() override;

protected:
    shared_ptr<UI> m_root;
#ifdef USE_TILE_LOCAL
    ShapeBuffer m_buf;
#endif
};

#ifdef USE_TILE_LOCAL
class UIDungeon : public UI
{
public:
    UIDungeon() : width(0), height(0), m_buf((ImageManager*)tiles.get_image_manager()), m_dirty(false) {};
    virtual ~UIDungeon() {};
    virtual void _render() override;
    virtual UISizeReq _get_preferred_size(Direction dim, int prosp_width) override;

    unsigned width, height;
    DungeonCellBuffer& buf() { m_dirty = true; return m_buf; };

protected:
    DungeonCellBuffer m_buf;
    bool m_dirty;
};
#endif

void ui_push_layout(shared_ptr<UI> root, KeymapContext km = KMC_DEFAULT);
void ui_pop_layout();
void ui_pump_events();
int ui_getch(KeymapContext km = KMC_DEFAULT);

void ui_push_scissor(i4 scissor);
void ui_pop_scissor();
i4 ui_get_scissor();

// XXX: this is a hack used to ensure that when switching to a
// layout-based UI, the starting window size is correct. This is necessary
// because there's no way to query TilesFramework for the current screen size
void ui_resize(int w, int h);
