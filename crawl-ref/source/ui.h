/**
 * @file
 * @brief Hierarchical layout system.
**/

#pragma once

#include <functional>

#include "format.h"
#include "KeymapContext.h"
#include "state.h"
#include "rltiles/tiledef-gui.h"
#include "tilefont.h"
#include "unwind.h"
#include "cio.h"
#ifdef USE_TILE_LOCAL
# include "tilebuf.h"
# include "tiledgnbuf.h"
# include "tiledoll.h"
# include "tilesdl.h"
#endif
#ifdef USE_TILE_WEB
# include "tileweb.h"
# include "json.h"
#endif

struct wm_keyboard_event;

namespace ui {

struct SizeReq
{
    int min, nat;
};

class Margin
{
public:
    constexpr Margin() : top(0), right(0), bottom(0), left(0) {}

    explicit constexpr Margin(int v) : top(v), right(v), bottom(v), left(v) {}
    constexpr Margin(int v, int h) : top(v), right(h), bottom(v), left(h) {}
    constexpr Margin(int t, int lr, int b)
        : top(t), right(lr), bottom(b), left(lr) {}
    constexpr Margin(int t, int r, int b, int l)
        : top(t), right(r), bottom(b), left(l) {}

    int top, right, bottom, left;
};

class Region
{
public:
    constexpr Region() : x(0), y(0), width(0), height(0) {}
    constexpr Region(int _x, int _y, int _width, int _height)
        : x(_x), y(_y), width(_width), height(_height) {}

    constexpr bool operator == (const Region& other) const
    {
        return x == other.x && y == other.y
            && width == other.width && height == other.height;
    }

    constexpr bool operator != (const Region& other) const
    {
        return !(*this == other);
    }

    constexpr bool empty() const
    {
        return width == 0 || height == 0;
    }

    constexpr int ex() const
    {
        return x + width;
    }

    constexpr int ey() const
    {
        return y + height;
    }

    constexpr bool contains_point(int _x, int _y) const
    {
        return _x >= x && _x < ex() && _y >= y && _y < ey();
    }

    Region aabb_intersect(const Region &b) const
    {
        const Region& a = *this;
        Region i = { max(a.x, b.x), max(a.y, b.y), min(a.ex(), b.ex()), min(a.ey(), b.ey()) };
        i.width -= i.x; i.height -= i.y;
        return i;
    }

    Region aabb_union(const Region &b) const
    {
        const Region& a = *this;
        Region i = { min(a.x, b.x), min(a.y, b.y), max(a.ex(), b.ex()), max(a.ey(), b.ey()) };
        i.width -= i.x; i.height -= i.y;
        return i;
    }

    int x, y, width, height;
};

inline ostream& operator << (ostream& ostr, Region const& value)
{
    ostr << "Region(x=" << value.x << ", y=" << value.y << ", ";
    ostr << "w=" << value.width << ", h=" << value.height << ")";
    return ostr;
}

class Size
{
public:
    constexpr Size() : width(0), height(0) {}
    explicit constexpr Size(int v) : width(v), height(v) {}
    constexpr Size(int w, int h) : width(w), height(h) {}

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

class Widget;

class Event
{
public:
    enum Type {
        KeyDown = 0,
        KeyUp,
        MouseMove,
        MouseDown,
        MouseUp,
        MouseEnter,
        MouseLeave,
        MouseWheel,
        FocusIn,
        FocusOut,
        Activate,
    };

    explicit Event(Type type);

    Type type() const { return m_type; }

    shared_ptr<Widget>& target() { return m_target; }
    shared_ptr<Widget> target() const { return m_target; }
    void set_target(shared_ptr<Widget> _target) { m_target = move(_target); }

protected:
    Type m_type;
    shared_ptr<Widget> m_target;
};

class KeyEvent final : public Event
{
public:
    KeyEvent(Type type, const wm_keyboard_event& wm_ev);

    int key() const { return m_key; }

protected:
    int m_key;
};

class MouseEvent final : public Event
{
public:
#ifdef USE_TILE_LOCAL
    MouseEvent(Type type, const wm_mouse_event& wm_ev);
#endif

    enum class Button
    {
        None = 0,
        Left = 1,
        Middle = 2,
        Right = 4,
    };

    Button button() const { return m_button; }
    int x() const { return m_x; }
    int y() const { return m_y; }
    int wheel_dx() const { return m_wheel_dx; }
    int wheel_dy() const { return m_wheel_dy; }

protected:
    Button m_button;
    int m_x, m_y;
    int m_wheel_dx, m_wheel_dy;
};

class FocusEvent final : public Event
{
public:
    FocusEvent(Type type);
};

class ActivateEvent final : public Event
{
public:
    ActivateEvent();
};

template<typename, typename> class Slot;

template<class Target, class... Args>
class Slot<Target, bool(Args...)>
{
public:
    ~Slot() { alive = false; }
    typedef function<bool (Args...)> HandlerSig;
    typedef multimap<Target*, HandlerSig> HandlerMap;
    template<typename P>
    bool emit_if(P pred, Args&... args)
    {
        for (auto it : handlers)
            if (pred(it.first))
            {
                HandlerSig func = it.second;
                if (func(forward<Args>(args)...))
                    return true;
            }
        return false;
    }
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
    friend struct UIRoot;

public:
    enum Align {
        START = 0,
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
    bool expand_h = false, expand_v = false;
    bool shrink_h = false, shrink_v = false;
    Region get_region() const { return m_region; }

    // FIXME: convert to getter and setter
    Size& min_size()
    {
        _invalidate_sizereq();
        return m_min_size;
    }

    Size& max_size()
    {
        _invalidate_sizereq();
        return m_max_size;
    }

    virtual void _render() = 0;
    virtual SizeReq _get_preferred_size(Direction dim, int prosp_width);
    virtual void _allocate_region();

    void _set_parent(Widget* p);

    Widget* _get_parent() const
    {
        return m_parent;
    }

    shared_ptr<Widget> get_shared()
    {
        return shared_from_this();
    }

    /**
     * Mark this widget as possibly having changed size.
     *
     * _get_preferred_size() will be called before the next allocation/render.
     */
    void _invalidate_sizereq(bool immediate = true);

    /**
     * Mark this widget as needing reallocation.
     *
     * _allocate_region() will be called before the next call to _render(), even
     * if the widget has not resized or moved. This is useful if buffers need to
     * be repacked due to widget state change.
     */
    void _queue_allocation(bool immediate = true);

    void set_allocation_needed()
    {
        alloc_queued = true;
    }

    /**
     * Mark this widget as needing redraw. render() will be called.
     */
    void _expose();

    /**
     * Get/set visibility of this widget only, ignoring the visibility of its
     * ancestors, if there are any, or whether it is in a layout at all.
     */
    bool is_visible() const
    {
        return m_visible;
    }
    void set_visible(bool);

    bool is_ancestor_of(const shared_ptr<Widget>& other);

    virtual void for_each_child(function<void(shared_ptr<Widget>&)>)
    {
    }

    // Wrapper functions which handle common behaviour
    // - margins
    // - caching
    void render();
    SizeReq get_preferred_size(Direction dim, int prosp_width);
    void allocate_region(Region region);

    Margin get_margin() const
    {
        return margin;
    }

    template<class... Args>
    void set_margin_for_crt(Args&&... args)
    {
#ifndef USE_TILE_LOCAL
        margin = Margin(forward<Args>(args)...);
        _invalidate_sizereq();
#else
        UNUSED(args...);
#endif
    }

    template<class... Args>
    void set_margin_for_sdl(Args&&... args)
    {
#ifdef USE_TILE_LOCAL
        margin = Margin(forward<Args>(args)...);
        _invalidate_sizereq();
#else
        UNUSED(args...);
#endif
    }

    virtual bool on_event(const Event& event);

    template<class F>
    void on_any_event(F&& cb)
    {
        slots.event.on(this, forward<F>(cb));
    }

    template<class F>
    void on_hotkey_event(F&& cb)
    {
        slots.hotkey.on(this, [cb](const Event& event){
            return cb(static_cast<const KeyEvent&>(event));
        });
    }

    template<class F>
    void on_layout_pop(F&& cb)
    {
        slots.layout_pop.on(this, [cb](){
            cb();
            return false;
        });
    }

#define EVENT_HANDLER_HELPER(NAME, ENUM, CLASS) \
    template<class F> \
    void NAME(F&& cb) \
    { \
        slots.event.on(this, [cb](const Event& event){ \
            if (event.type() != Event::Type::ENUM) \
                return false; \
            return cb(static_cast<const CLASS&>(event)); \
        }); \
    }
    EVENT_HANDLER_HELPER(on_keydown_event, KeyDown, KeyEvent)
    EVENT_HANDLER_HELPER(on_keyup_event, KeyUp, KeyEvent)
    EVENT_HANDLER_HELPER(on_mousemove_event, MouseMove, MouseEvent)
    EVENT_HANDLER_HELPER(on_mousedown_event, MouseDown, MouseEvent)
    EVENT_HANDLER_HELPER(on_mouseup_event, MouseUp, MouseEvent)
    EVENT_HANDLER_HELPER(on_mouseenter_event, MouseEnter, MouseEvent)
    EVENT_HANDLER_HELPER(on_mouseleave_event, MouseLeave, MouseEvent)
    EVENT_HANDLER_HELPER(on_mousewheel_event, MouseWheel, MouseEvent)
    EVENT_HANDLER_HELPER(on_focusin_event, FocusIn, FocusEvent)
    EVENT_HANDLER_HELPER(on_focusout_event, FocusOut, FocusEvent)
    EVENT_HANDLER_HELPER(on_activate_event, Activate, ActivateEvent)
#undef EVENT_HANDLER_HELPER

    /**
     * Container widget interface. Must return a pointer to the child widget at
     * the given screen position, or nullptr.
     */
    virtual shared_ptr<Widget> get_child_at_offset(int, int)
    {
        return nullptr;
    }

    const string& sync_id()
    {
        return m_sync_id;
    }

    void set_sync_id(string id);

protected:
    Region m_region;
    Margin margin = Margin{0};

    void _unparent(shared_ptr<Widget>& child);

    vector<shared_ptr<Widget>> m_internal_children;
    void add_internal_child(shared_ptr<Widget> child);

    template<class F>
    void for_each_internal_child(F&& cb)
    {
        for (auto& child : m_internal_children)
            cb(child);
    }

    template<class F>
    void for_each_child_including_internal(F&& cb)
    {
        for_each_internal_child(forward<F>(cb));
        for_each_child(forward<F>(cb));
    }

    /**
     * Whether widgets of this class should be included in the set of focusable
     * widgets. Should return a constant boolean.
     */
    virtual bool can_take_focus() { return false; };

    /**
     * Whether a widget is currently focusable.
     */
    bool focusable()
    {
        // FIXME: does not take into account hierarchical visibility
        // e.g. widget in currently hidden child of switcher
        return is_visible();
    }

#ifdef USE_TILE_WEB
    virtual void sync_save_state();
    virtual void sync_load_state(const JsonNode *json);
    void sync_state_changed();
#endif

    void _emit_layout_pop();

private:
    bool cached_sr_valid[2] = { false, false };
    SizeReq cached_sr[2];
    int cached_sr_pw;
    bool alloc_queued = false;
    bool m_visible = true;
    Widget* m_parent = nullptr;

    Size m_min_size = Size{0};
    Size m_max_size = Size{INT_MAX};

    string m_sync_id;

    static struct slots {
        Slot<Widget, bool(const Event&)> event;
        Slot<Widget, bool(const Event&)> hotkey;
        Slot<Widget, bool()> layout_pop;
    } slots;
};

class Container : public Widget
{
public:
    virtual ~Container() {}
    virtual void for_each_child(function<void(shared_ptr<Widget>&)> f) = 0;
};

class Bin : public Container
{
public:
    virtual ~Bin()
    {
        if (m_child)
            _unparent(m_child);
    }
    void set_child(shared_ptr<Widget> child);
    virtual shared_ptr<Widget> get_child() const
    {
        return m_child;
    }
    shared_ptr<Widget> get_child_at_offset(int x, int y) override;

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

        iterator(value_type& _c, bool _state) : c(_c), state(_state) {}
        void operator++ () { state = true; }
        value_type& operator* () { return c; }
        bool operator== (const iterator& other) { return c == other.c && state == other.state; }
        bool operator!= (const iterator& other) { return !(*this == other); }
    };

public:
    iterator begin() { return iterator(m_child, false); }
    iterator end() { return iterator(m_child, true); }
    void for_each_child(function<void(shared_ptr<Widget>&)> f) override
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
    virtual ~ContainerVec()
    {
        for (auto& child : m_children)
            if (child)
                _unparent(child);
    }

    shared_ptr<Widget> get_child_at_offset(int x, int y) override;

    size_t num_children() const
    {
        return m_children.size();
    }

    template<typename T>
    shared_ptr<Widget>& get_child(T pos)
    {
        return m_children[pos];
    }

    template<typename T>
    const shared_ptr<Widget>& get_child(T pos) const
    {
        return m_children[pos];
    }

    template<typename T>
    shared_ptr<Widget>& operator[](T pos)
    {
        return m_children[pos];
    }

    template<typename T>
    const shared_ptr<Widget>& operator[](T pos) const
    {
        return m_children[pos];
    }

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

        iterator(vector<value_type>& _c, vector<value_type>::iterator _it) : c(_c), it(_it) {}
        void operator++ () { ++it; }
        value_type& operator* () { return *it; }
        bool operator== (const iterator& other) { return c == other.c && it == other.it; }
        bool operator!= (const iterator& other) { return !(*this == other); }
    };

public:
    iterator begin()
    {
        return iterator(m_children, m_children.begin());
    }
    iterator end()
    {
        return iterator(m_children, m_children.end());
    }

    void for_each_child(function<void(shared_ptr<Widget>&)> f) override
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

    explicit Box(Direction dir, Expand expand_flags = NONE)
    {
        horz = dir == HORZ;
        expand_h = expand_flags & EXPAND_H;
        expand_v = expand_flags & EXPAND_V;
    }

    virtual ~Box() {}
    void add_child(shared_ptr<Widget> child);

    Widget::Align main_alignment() const { return align_main; }
    Widget::Align cross_alignment() const { return align_cross; }

    void set_main_alignment(Widget::Align align)
    {
        align_main = align;
        _invalidate_sizereq();
    };
    void set_cross_alignment(Widget::Align align)
    {
        align_cross = align;
        _invalidate_sizereq();
    };

    void _render() override;
    SizeReq _get_preferred_size(Direction dim, int prosp_width) override;
    void _allocate_region() override;

protected:
    bool horz;
    Widget::Align align_main = START;
    Widget::Align align_cross = START;

    vector<int> layout_main_axis(vector<SizeReq>& ch_psz, int main_sz);
    vector<int> layout_cross_axis(vector<SizeReq>& ch_psz, int cross_sz);
};

class Text : public Widget
{
public:
    Text();
    virtual ~Text() {}

    template<class... Args>
    explicit Text(Args&&... args) : Text()
    {
        set_text(forward<Args>(args)...);
    }

    void set_text(const formatted_string &fs);
    void set_text(const string& text)
    {
        set_text(formatted_string(text));
    }

    const formatted_string& get_text() const
    {
        return m_text;
    }

    void set_font(FontWrapper *font);
    void set_highlight_pattern(string pattern, bool hl_line = false);

    void _render() override;
    SizeReq _get_preferred_size(Direction dim, int prosp_width) override;
    void _allocate_region() override;

#ifndef USE_TILE_LOCAL
    void set_bg_colour(COLOURS colour);
#endif

    void set_wrap_text(bool _wrap_text)
    {
        if (wrap_text == _wrap_text)
            return;
        wrap_text = _wrap_text;
        _invalidate_sizereq();
    }

    void set_ellipsize(bool _ellipsize)
    {
        if (ellipsize == _ellipsize)
            return;
        ellipsize = _ellipsize;
        _invalidate_sizereq();
    }

protected:
    void wrap_text_to_size(int width, int height);

    bool wrap_text = false;
    bool ellipsize = false;

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
    Size m_wrapped_size = Size{-1};
    string hl_pat;
    bool hl_line;
};

class Image : public Widget
{
public:
    Image() {}
    explicit Image(tile_def tile) : Image()
    {
        set_tile(tile);
    }
    virtual ~Image() {}
    void set_tile(tile_def tile);
    tile_def get_tile() const
    {
        return m_tile;
    }

    void _render() override;
    SizeReq _get_preferred_size(Direction dim, int prosp_width) override;

protected:
    tile_def m_tile = {TILEG_ERROR};
    int m_tw {0}, m_th {0};

#ifdef USE_TILE_LOCAL
    GenericTexture m_img;
#endif
};

class Stack : public ContainerVec
{
public:
    virtual ~Stack() {}
    void add_child(shared_ptr<Widget> child);
    void pop_child();

    shared_ptr<Widget> get_child_at_offset(int x, int y) override;

    void _render() override;
    SizeReq _get_preferred_size(Direction dim, int prosp_width) override;
    void _allocate_region() override;
};

class Switcher : public ContainerVec
{
public:
    virtual ~Switcher() {}
    void add_child(shared_ptr<Widget> child);
    // FIXME: convert to getter and setter
    int& current();
    shared_ptr<Widget> current_widget();

    Widget::Align align_x = START, align_y = START;

    void _render() override;
    SizeReq _get_preferred_size(Direction dim, int prosp_width) override;
    void _allocate_region() override;
    shared_ptr<Widget> get_child_at_offset(int x, int y) override;

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
    }

    void add_child(shared_ptr<Widget> child, int x, int y, int w = 1, int h = 1);

    int column_flex_grow(int x) const
    {
        return m_col_info.at(x).flex_grow;
    }
    int row_flex_grow(int y) const
    {
        return m_row_info.at(y).flex_grow;
    }

    // FIXME: convert to getter and setter
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

    void _render() override;
    SizeReq _get_preferred_size(Direction dim, int prosp_width) override;
    void _allocate_region() override;
    shared_ptr<Widget> get_child_at_offset(int x, int y) override;

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

        iterator(vector<child_info>& _c, vector<child_info>::iterator _it) : c(_c), it(_it) {}
        void operator++ () { ++it; }
        value_type& operator* () { return it->widget; }
        bool operator== (const iterator& other) { return c == other.c && it == other.it; }
        bool operator!= (const iterator& other) { return !(*this == other); }
    };

public:
    iterator begin() { return iterator(m_child_info, m_child_info.begin()); }
    iterator end() { return iterator(m_child_info, m_child_info.end()); }
    void for_each_child(function<void(shared_ptr<Widget>&)> f) override
    {
        for (auto& child : *this)
            f(child);
    }
};

class Scroller : public Bin
{
public:
    virtual ~Scroller() {}

    virtual void set_scroll(int y);

    int get_scroll() const
    {
        return m_scroll;
    }

    void set_scrollbar_visible(bool vis)
    {
        m_scrolbar_visible = vis;
    }

    void _render() override;
    SizeReq _get_preferred_size(Direction dim, int prosp_width) override;
    void _allocate_region() override;
    bool on_event(const Event& event) override;

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
public:
    explicit Layout(shared_ptr<Widget> child);

    void _render() override;
    SizeReq _get_preferred_size(Direction dim, int prosp_width) override;
    void _allocate_region() override;
protected:
#ifdef USE_TILE_LOCAL
    int m_depth;
#endif
};

class Popup : public Layout
{
public:
    explicit Popup(shared_ptr<Widget> child) : Layout(move(child)) {}

    void _render() override;
    SizeReq _get_preferred_size(Direction dim, int prosp_width) override;
    void _allocate_region() override;

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

class Checkbox : public Bin
{
public:
    Checkbox() {};

    virtual void _render() override;
    virtual SizeReq _get_preferred_size(Direction dim, int prosp_width) override;
    virtual void _allocate_region() override;

    virtual bool on_event(const Event& event) override;

    bool checked() const
    {
        return m_checked;
    };

    void set_checked(bool _checked)
    {
        if (m_checked == _checked)
            return;
        m_checked = _checked;
#ifdef USE_TILE_WEB
        sync_state_changed();
#endif
        _expose();
    };

protected:
    bool can_take_focus() override { return true; };

#ifdef USE_TILE_WEB
    void sync_save_state() override;
    void sync_load_state(const JsonNode *json) override;
#endif

    bool m_checked = false;
#ifdef USE_TILE_LOCAL
    bool m_hovered = false;
#endif

#ifdef USE_TILE_LOCAL
    static const int check_w = 30;
    static const int check_h = 20;
#else
    static const int check_w = 4;
    static const int check_h = 1;
#endif
};

class TextEntry : public Widget
{
public:
    TextEntry();
    virtual void _render() override;
    virtual SizeReq _get_preferred_size(Direction dim, int prosp_width) override;
    virtual void _allocate_region() override;
    virtual bool on_event(const Event& event) override;

    void set_font(FontWrapper *font);

    string get_text() const { return m_text; };
    void set_text(string s) {
        m_line_reader.set_text(s);
        m_text = m_line_reader.get_text();
        m_cursor = m_line_reader.get_cursor_position();
#ifdef USE_TILE_WEB
        sync_state_changed();
#endif
        _expose();
    };

    template<typename T>
    void set_input_history(T&& fn) {
        m_line_reader.set_input_history(forward<T>(fn));
    }

    template<typename T>
    void set_keyproc(T&& fn) {
        m_line_reader.set_keyproc(forward<T>(fn));
    }

protected:
    bool can_take_focus() override { return true; };

#ifdef USE_TILE_WEB
    void sync_save_state() override;
    void sync_load_state(const JsonNode *json) override;
#endif

#ifdef USE_TILE_LOCAL
    int padding_size();
#endif

    string m_text;
    int m_cursor = 0;
    int m_hscroll = 0;

    class LineReader
    {
    public:
        LineReader(char *buffer, size_t bufsz);
        virtual ~LineReader();

        typedef keyfun_action (*keyproc)(int &key);

        string get_text() const;
        void set_text(string s);

        void set_input_history(input_history *ih);
        void set_keyproc(keyproc fn);

        void set_edit_mode(edit_mode m);
        edit_mode get_edit_mode();

        void set_prompt(string p);

        void insert_char_at_cursor(int ch);
        void overwrite_char_at_cursor(int ch);
#ifdef USE_TILE_WEB
        void set_tag(const string &tag);
#endif

        int get_cursor_position() {
            return cur - buffer;
        };

        int process_key_core(int ch);
        int process_key(int ch);

    protected:
        void backspace();
        void delete_char();
        void killword();
        void kill_to_begin();
        void kill_to_end();
#ifdef USE_TILE_LOCAL
        void clipboard_paste();
#endif

        bool is_wordchar(char32_t c);

    protected:
        char            *buffer;
        size_t          bufsz;
        input_history   *history;
        keyproc         keyfn;
        edit_mode       mode;
        string          prompt; // currently only used for webtiles input dialogs

#ifdef USE_TILE_WEB
        string          tag; // For identification on the Webtiles client side
#endif

        // These are subject to change during editing.
        char            *cur;
        int             length;
    };

    char m_buffer[1024];
    LineReader m_line_reader;

#ifdef USE_TILE_LOCAL
    ShapeBuffer m_buf;
    FontWrapper *m_font;
#endif
};

#ifdef USE_TILE_LOCAL
class Dungeon : public Widget
{
public:
    Dungeon() : m_buf((ImageManager*)tiles.get_image_manager()) {}
    virtual ~Dungeon() {}

    void _render() override;
    SizeReq _get_preferred_size(Direction dim, int prosp_width) override;

    unsigned width = 0;
    unsigned height = 0;

    // FIXME: convert to getter and setter
    DungeonCellBuffer& buf()
    {
        m_dirty = true;
        return m_buf;
    }

protected:
    DungeonCellBuffer m_buf;
    bool m_dirty = true;
};

class PlayerDoll : public Widget
{
public:
    explicit PlayerDoll(dolls_data doll);
    virtual ~PlayerDoll();

    void _render() override;
    SizeReq _get_preferred_size(Direction dim, int prosp_width) override;
    void _allocate_region() override;

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
void run_layout(shared_ptr<Widget> root, const bool& done,
        shared_ptr<Widget> initial_focus = nullptr);
bool has_layout();
NORETURN void restart_layout();
int getch(KeymapContext km = KMC_DEFAULT);
void force_render();
void render();
void delay(unsigned int ms);

void set_focused_widget(Widget* w);
Widget* get_focused_widget();
bool raise_event(Event& event);

#ifdef USE_TILE_WEB
void recv_ui_state_change(const JsonNode *json);
void sync_ui_state();
int layout_generation_id();
#endif

// XXX: this is a hack used to ensure that when switching to a
// layout-based UI, the starting window size is correct. This is necessary
// because there's no way to query TilesFramework for the current screen size
void resize(int w, int h);

bool is_available();

void show_cursor_at(coord_def pos);
void show_cursor_at(int x, int y);

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

#ifdef USE_TILE_LOCAL
wm_mouse_event to_wm_event(const MouseEvent &);
#endif

#ifdef USE_TILE_LOCAL
extern bool should_render_current_regions;
#endif
}
