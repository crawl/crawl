/**
 * @file
 * @brief Hierarchical layout system.
**/

#include "AppHdr.h"

#include <numeric>
#include <stack>
#include <chrono>
#include <cwctype>

#include "ui.h"
#include "cio.h"
#include "macro.h"
#include "state.h"
#include "tileweb.h"
#include "unicode.h"
#include "libutil.h"
#include "windowmanager.h"
#include "ui-scissor.h"

#ifdef USE_TILE_LOCAL
# include "glwrapper.h"
# include "tilebuf.h"
# include "tilepick-p.h"
# include "tile-player-flag-cut.h"
#else
# if defined(UNIX) || defined(TARGET_COMPILER_MINGW)
#  include <unistd.h>
# endif
# include "output.h"
# include "stringutil.h"
# include "view.h"
#endif

#ifdef USE_TILE_WEB
# include <unordered_map>
#endif

namespace ui {

#ifndef USE_TILE_LOCAL
static void clear_text_region(Region region, COLOURS bg);
#endif

static struct UIRoot
{
public:
    void push_child(shared_ptr<Widget> child, KeymapContext km);
    void pop_child();

    shared_ptr<Widget> top_child()
    {
        const auto sz = m_root.num_children();
        return sz > 0 ? m_root.get_child(sz-1) : nullptr;
    }

    size_t num_children() const
    {
        return m_root.num_children();
    };

    bool widget_is_in_layout(const Widget* w)
    {
        for (; w; w = w->_get_parent())
            if (w == &m_root)
                return true;
        return false;
    }

    void resize(int w, int h);
    void layout();
    void render();
    void swap_buffers();

    bool on_event(wm_event& event);
    bool deliver_event(Event& event);

    void queue_layout()
    {
        m_needs_layout = true;
    }

    void expose_region(Region r)
    {
        if (r.empty())
            return;
        if (m_dirty_region.empty())
            m_dirty_region = r;
        else
            m_dirty_region = m_dirty_region.aabb_union(r);
        needs_paint = true;
    }

    bool needs_paint;
    bool needs_swap;

#ifdef DEBUG
    bool debug_draw = false;
    bool debug_on_event(const wm_event& event);
    void debug_render();
#endif

    struct LayoutInfo
    {
        KeymapContext keymap = KMC_NONE;
        Widget* current_focus = nullptr;
        Widget* default_focus = nullptr;
        int generation_id = 0;
    };

    LayoutInfo state;  // current keymap and focus info
    int next_generation_id = 1;

    vector<int> cutoff_stack;
    vector<Widget*> default_focus_stack;
    vector<Widget*> focus_order;

    void update_focus_order();
    void focus_next();
    void focus_prev();

    struct RestartAllocation {};

#ifdef USE_TILE_LOCAL
    vector<Widget*> hover_path;

    void update_hover_path();
    void update_hover_path_for_widget(Widget* widget);
    void send_mouse_enter_leave_events(
            const vector<Widget*>& old_hover_path,
            const vector<Widget*>& new_hover_path);
#endif

#ifdef USE_TILE_WEB
    void update_synced_widgets();
    unordered_map<string, Widget*> synced_widgets;
    bool receiving_ui_state = false;
    void sync_state();
    void recv_ui_state_change(const JsonNode *state);
#endif

    coord_def cursor_pos;

protected:
    int m_w, m_h;
    Region m_region;
    Region m_dirty_region;
    Stack m_root;
    bool m_needs_layout{false};
    bool m_changed_layout_since_click = false;
    vector<LayoutInfo> saved_layout_info;
} ui_root;

static ScissorStack scissor_stack;

struct Widget::slots Widget::slots = {};

Event::Event(Event::Type _type) : m_type(_type)
{
}

KeyEvent::KeyEvent(Event::Type _type, const wm_keyboard_event& wm_ev) : Event(_type)
{
    m_key = wm_ev.keysym.sym;
}

#ifdef USE_TILE_LOCAL
MouseEvent::MouseEvent(Event::Type _type, const wm_mouse_event& wm_ev) : Event(_type)
{
    m_button = static_cast<MouseEvent::Button>(wm_ev.button);
    // XXX: is it possible that the cursor has moved since the SDL event fired?
    wm->get_mouse_state(&m_x, &m_y);
    m_wheel_dx = _type == MouseWheel ? wm_ev.px : 0;
    m_wheel_dy = _type == MouseWheel ? wm_ev.py : 0;
}
#endif

FocusEvent::FocusEvent(Event::Type type) : Event(type)
{
}

ActivateEvent::ActivateEvent() : Event(Event::Type::Activate)
{
}

Widget::~Widget()
{
    Widget::slots.event.remove_by_target(this);
    Widget::slots.hotkey.remove_by_target(this);
    if (m_parent && get_focused_widget() == this)
        set_focused_widget(nullptr);
    _set_parent(nullptr);
    erase_val(ui_root.focus_order, this);
#ifdef USE_TILE_WEB
    if (!m_sync_id.empty())
        ui_root.synced_widgets.erase(m_sync_id);
#endif
}

void Widget::_emit_layout_pop()
{
    Widget::slots.layout_pop.emit(this);
    Widget::slots.layout_pop.remove_by_target(this);
}

bool Widget::on_event(const Event& event)
{
    return Widget::slots.event.emit(this, event);
}

shared_ptr<Widget> ContainerVec::get_child_at_offset(int x, int y)
{
    for (shared_ptr<Widget>& child : m_children)
        if (child->get_region().contains_point(x, y))
            return child;
    return nullptr;
}

shared_ptr<Widget> Bin::get_child_at_offset(int x, int y)
{
    if (m_child && m_child->get_region().contains_point(x, y))
        return m_child;
    return nullptr;
}

void Bin::set_child(shared_ptr<Widget> child)
{
    child->_set_parent(this);
    m_child = move(child);
    _invalidate_sizereq();
}

void Widget::render()
{
    if (m_visible)
        _render();
}

SizeReq Widget::get_preferred_size(Direction dim, int prosp_width)
{
    ASSERT((dim == HORZ) == (prosp_width == -1));

    if (!m_visible)
        return { 0, 0 };

    if (cached_sr_valid[dim] && (!dim || cached_sr_pw == prosp_width))
        return cached_sr[dim];

    prosp_width = dim ? prosp_width - margin.right - margin.left : prosp_width;
    SizeReq ret = _get_preferred_size(dim, prosp_width);
    ASSERT(ret.min <= ret.nat);

    // Order is important: max sizes limit expansion, and don't include margins
    const bool expand = dim ? expand_v : expand_h;
    const bool shrink = dim ? shrink_v : shrink_h;
    ASSERT(!(expand && shrink));
    constexpr int ui_expand_sz = 0xffffff;

    if (expand)
        ret.nat = ui_expand_sz;
    else if (shrink)
        ret.nat = ret.min;

    int& _min_size = dim ? m_min_size.height : m_min_size.width;
    int& _max_size = dim ? m_max_size.height : m_max_size.width;

    ASSERT(_min_size <= _max_size);
    ret.min = max(ret.min, _min_size);
    ret.nat = min(ret.nat, max(_max_size, ret.min));
    ret.nat = max(ret.nat, ret.min);
    ASSERT(ret.min <= ret.nat);

    const int m = dim ? margin.top + margin.bottom : margin.left + margin.right;
    ret.min += m;
    ret.nat += m;

    ret.nat = min(ret.nat, ui_expand_sz);

    cached_sr_valid[dim] = true;
    cached_sr[dim] = ret;
    if (dim)
        cached_sr_pw = prosp_width;

    return ret;
}

void Widget::allocate_region(Region region)
{
    if (!m_visible)
        return;

    Region new_region = {
        region.x + margin.left,
        region.y + margin.top,
        region.width - margin.left - margin.right,
        region.height - margin.top - margin.bottom,
    };

    if (m_region == new_region && !alloc_queued)
        return;
    ui_root.expose_region(m_region);
    ui_root.expose_region(new_region);
    m_region = new_region;
    alloc_queued = false;

    ASSERT(m_region.width >= 0);
    ASSERT(m_region.height >= 0);
    _allocate_region();
}

SizeReq Widget::_get_preferred_size(Direction, int)
{
    return { 0, 0xffffff };
}

void Widget::_allocate_region()
{
}

/**
 * Determine whether a widget contains the given widget.
 *
 * @param child   The other widget.
 * @return        True if the other widget is a descendant of this widget.
 */
bool Widget::is_ancestor_of(const shared_ptr<Widget>& other)
{
    for (Widget* w = other.get(); w; w = w->_get_parent())
        if (w == this)
            return true;
    return false;
}

void Widget::_set_parent(Widget* p)
{
    m_parent = p;
#ifdef USE_TILE_LOCAL
    ui_root.update_hover_path_for_widget(this);
#endif
}

/**
 * Unparent a widget.
 *
 * This function verifies that a widget has the correct parent before orphaning.
 * Intended for use in container widget destructors.
 *
 * @param child   The child widget to unparent.
 */
void Widget::_unparent(shared_ptr<Widget>& child)
{
    if (child->m_parent == this)
        child->_set_parent(nullptr);
}

void Widget::_invalidate_sizereq(bool immediate)
{
    for (auto w = this; w; w = w->m_parent)
        fill(begin(w->cached_sr_valid), end(w->cached_sr_valid), false);
    if (immediate)
        ui_root.queue_layout();
}

void Widget::_queue_allocation(bool immediate)
{
    for (auto w = this; w && !w->alloc_queued; w = w->m_parent)
        w->alloc_queued = true;
    if (immediate)
        ui_root.queue_layout();
}

void Widget::_expose()
{
    ui_root.expose_region(m_region);
}

void Widget::set_visible(bool visible)
{
    if (m_visible == visible)
        return;
    m_visible = visible;
    _invalidate_sizereq();
}

void Widget::add_internal_child(shared_ptr<Widget> child)
{
    child->_set_parent(this);
    m_internal_children.emplace_back(move(child));
}

void Widget::set_sync_id(string id)
{
    ASSERT(!_get_parent()); // synced widgets are collected on layout push/pop
    m_sync_id = id;
}

#ifdef USE_TILE_WEB
void Widget::sync_save_state()
{
}

void Widget::sync_load_state(const JsonNode *)
{
}

void Widget::sync_state_changed()
{
    if (m_sync_id.empty())
        return;
    const auto& top = ui_root.top_child();
    if (!top || !top->is_ancestor_of(get_shared()))
        return;
    tiles.json_open_object();
    sync_save_state();
    tiles.json_write_string("msg", "ui-state-sync");
    tiles.json_write_string("widget_id", m_sync_id);
    tiles.json_write_bool("from_webtiles", ui_root.receiving_ui_state);
    tiles.json_write_int("generation_id", ui_root.state.generation_id);
    tiles.json_close_object();
    tiles.finish_message();
}
#endif

void Box::add_child(shared_ptr<Widget> child)
{
    child->_set_parent(this);
    m_children.push_back(move(child));
    _invalidate_sizereq();
}

void Box::_render()
{
    for (auto const& child : m_children)
        child->render();
}

vector<int> Box::layout_main_axis(vector<SizeReq>& ch_psz, int main_sz)
{
    // find the child sizes on the main axis
    vector<int> ch_sz(m_children.size());

    int extra = main_sz;
    for (size_t i = 0; i < m_children.size(); i++)
    {
        ch_sz[i] = ch_psz[i].min;
        extra -= ch_psz[i].min;
        if (align_main == Align::STRETCH)
            ch_psz[i].nat = INT_MAX;
    }
    ASSERT(extra >= 0);

    while (extra > 0)
    {
        int sum_flex_grow = 0, remainder = 0;
        for (size_t i = 0; i < m_children.size(); i++)
            sum_flex_grow += ch_sz[i] < ch_psz[i].nat ? m_children[i]->flex_grow : 0;
        if (!sum_flex_grow)
            break;

        // distribute space to children, based on flex_grow
        for (size_t i = 0; i < m_children.size(); i++)
        {
            float efg = ch_sz[i] < ch_psz[i].nat ? m_children[i]->flex_grow : 0;
            int ch_extra = extra * efg / sum_flex_grow;
            ASSERT(ch_sz[i] <= ch_psz[i].nat);
            int taken = min(ch_extra, ch_psz[i].nat - ch_sz[i]);
            ch_sz[i] += taken;
            remainder += ch_extra - taken;
        }
        extra = remainder;
    }

    return ch_sz;
}

vector<int> Box::layout_cross_axis(vector<SizeReq>& ch_psz, int cross_sz)
{
    vector<int> ch_sz(m_children.size());

    for (size_t i = 0; i < m_children.size(); i++)
    {
        const bool stretch = align_cross == STRETCH;
        ch_sz[i] = stretch ? cross_sz : min(max(ch_psz[i].min, cross_sz), ch_psz[i].nat);
    }

    return ch_sz;
}

SizeReq Box::_get_preferred_size(Direction dim, int prosp_width)
{
    vector<SizeReq> sr(m_children.size());

    // Get preferred widths
    for (size_t i = 0; i < m_children.size(); i++)
        sr[i] = m_children[i]->get_preferred_size(Widget::HORZ, -1);

    if (dim)
    {
        // Get actual widths
        vector<int> cw = horz ? layout_main_axis(sr, prosp_width) : layout_cross_axis(sr, prosp_width);

        // Get preferred heights
        for (size_t i = 0; i < m_children.size(); i++)
            sr[i] = m_children[i]->get_preferred_size(Widget::VERT, cw[i]);
    }

    // find sum/max of preferred sizes, as appropriate
    bool main_axis = dim == !horz;
    SizeReq r = { 0, 0 };
    for (auto const& c : sr)
    {
        r.min = main_axis ? r.min + c.min : max(r.min, c.min);
        r.nat = main_axis ? r.nat + c.nat : max(r.nat, c.nat);
    }
    return r;
}

void Box::_allocate_region()
{
    vector<SizeReq> sr(m_children.size());

    // Get preferred widths
    for (size_t i = 0; i < m_children.size(); i++)
        sr[i] = m_children[i]->get_preferred_size(Widget::HORZ, -1);

    // Get actual widths
    vector<int> cw = horz ? layout_main_axis(sr, m_region.width) : layout_cross_axis(sr, m_region.width);

    // Get preferred heights
    for (size_t i = 0; i < m_children.size(); i++)
        sr[i] = m_children[i]->get_preferred_size(Widget::VERT, cw[i]);

    // Get actual heights
    vector<int> ch = horz ? layout_cross_axis(sr, m_region.height) : layout_main_axis(sr, m_region.height);

    auto const &m = horz ? cw : ch;
    int extra_main_space = (horz ? m_region.width : m_region.height) - accumulate(m.begin(), m.end(), 0);
    ASSERT(extra_main_space >= 0);

    // main axis offset
    int mo;
    switch (align_main)
    {
        case Widget::START:   mo = 0; break;
        case Widget::CENTER:  mo = extra_main_space/2; break;
        case Widget::END:     mo = extra_main_space; break;
        case Widget::STRETCH: mo = 0; break;
        default: ASSERT(0);
    }
    int ho = m_region.x + (horz ? mo : 0);
    int vo = m_region.y + (!horz ? mo : 0);

    Region cr = {ho, vo, 0, 0};
    for (size_t i = 0; i < m_children.size(); i++)
    {
        // cross axis offset
        int extra_cross_space = horz ? m_region.height - ch[i] : m_region.width - cw[i];

        const Align child_align = align_cross;
        int xo;
        switch (child_align)
        {
            case Widget::START:   xo = 0; break;
            case Widget::CENTER:  xo = extra_cross_space/2; break;
            case Widget::END:     xo = extra_cross_space; break;
            case Widget::STRETCH: xo = 0; break;
            default: ASSERT(0);
        }

        int& cr_cross_offset = horz ? cr.y : cr.x;
        int& cr_cross_size = horz ? cr.height : cr.width;

        cr_cross_offset = (horz ? vo : ho) + xo;
        cr.width = cw[i];
        cr.height = ch[i];
        if (child_align == STRETCH)
            cr_cross_size = (horz ? ch : cw)[i];
        m_children[i]->allocate_region(cr);

        int& cr_main_offset = horz ? cr.x : cr.y;
        int& cr_main_size = horz ? cr.width : cr.height;
        cr_main_offset += cr_main_size;
    }
}

Text::Text()
{
#ifdef USE_TILE_LOCAL
    set_font(tiles.get_crt_font());
#endif
}

void Text::set_text(const formatted_string &fs)
{
    if (fs == m_text)
        return;
    m_text.clear();
    m_text += fs;
    _invalidate_sizereq();
    _expose();
    m_wrapped_size = Size(-1);
    _queue_allocation();
}

#ifdef USE_TILE_LOCAL
void Text::set_font(FontWrapper *font)
{
    ASSERT(font);
    m_font = font;
    _queue_allocation();
}
#endif

void Text::set_highlight_pattern(string pattern, bool line)
{
    hl_pat = pattern;
    hl_line = line;
    _expose();
}

void Text::wrap_text_to_size(int width, int height)
{
    Size wrapped_size = { width, height };
    if (m_wrapped_size == wrapped_size)
        return;
    m_wrapped_size = wrapped_size;

    height = height ? height : 0xfffffff;

#ifdef USE_TILE_LOCAL
    if (wrap_text || ellipsize)
        m_text_wrapped = m_font->split(m_text, width, height);
    else
        m_text_wrapped = m_text;

    m_brkpts.clear();
    m_brkpts.emplace_back(brkpt({0, 0}));
    unsigned tally = 0, acc = 0;
    for (unsigned i = 0; i < m_text_wrapped.ops.size(); i++)
    {
        formatted_string::fs_op &op = m_text_wrapped.ops[i];
        if (op.type != FSOP_TEXT)
            continue;
        if (acc > 0)
        {
            m_brkpts.emplace_back(brkpt({i, tally}));
            acc = 0;
        }
        unsigned n = count(op.text.begin(), op.text.end(), '\n');
        acc += n;
        tally += n;
    }
#else
    m_wrapped_lines.clear();
    formatted_string::parse_string_to_multiple(m_text.to_colour_string(), m_wrapped_lines, width);
    // add ellipsis to last line of text if necessary
    if (height < (int)m_wrapped_lines.size())
    {
        auto& last_line = m_wrapped_lines[height-1], next_line = m_wrapped_lines[height];
        last_line += " ";
        last_line += next_line;
        last_line = last_line.chop(width-2);
        last_line += "..";
        m_wrapped_lines.resize(height);
    }
    if (m_wrapped_lines.empty())
        m_wrapped_lines.emplace_back("");
#endif
}

static vector<size_t> _find_highlights(const string& haystack, const string& needle, int a, int b)
{
    vector<size_t> highlights;
    size_t pos = haystack.find(needle, max(a-(int)needle.size()+1, 0));
    while (pos != string::npos && pos < b+needle.size()-1)
    {
        highlights.push_back(pos);
        pos = haystack.find(needle, pos+1);
    }
    return highlights;
}

void Text::_render()
{
    Region region = m_region;
    region = region.aabb_intersect(scissor_stack.top());
    if (region.width <= 0 || region.height <= 0)
        return;

    wrap_text_to_size(m_region.width, m_region.height);

#ifdef USE_TILE_LOCAL
    const int dev_line_height = m_font->char_height(false);
    const int line_min_pos = display_density.logical_to_device(
                                                    region.y - m_region.y);
    const int line_max_pos = display_density.logical_to_device(
                                        region.y + region.height - m_region.y);
    const int line_min = line_min_pos / dev_line_height;
    const int line_max = line_max_pos / dev_line_height;

    // find the earliest and latest ops in the string that could be displayed
    // in the currently visible region, as well as the line offset of the
    // earliest.
    int line_off = 0;
    int ops_min = 0, ops_max = m_text_wrapped.ops.size();
    {
        int i = 1;
        for (; i < (int) m_brkpts.size(); i++)
            if (static_cast<int>(m_brkpts[i].line) >= line_min)
            {
                ops_min = m_brkpts[i - 1].op;
                line_off = m_brkpts[i - 1].line;
                break;
            }
        for (; i < (int)m_brkpts.size(); i++)
            if (static_cast<int>(m_brkpts[i].line) > line_max)
            {
                ops_max = m_brkpts[i].op;
                break;
            }
    }

    // the slice of the contained string that will be displayed
    formatted_string slice;
    slice.ops = vector<formatted_string::fs_op>(
        m_text_wrapped.ops.begin() + ops_min,
        m_text_wrapped.ops.begin() + ops_max);

    // TODO: this is really complicated, can it be refactored? Does it
    // really need to iterate over the formatted_text slices? I'm not sure
    // the formatting ever matters for highlighting locations.
    if (!hl_pat.empty())
    {
        const auto& full_text = m_text.tostring();

        // need to find the byte ranges in full_text that our slice corresponds to
        // note that the indexes are the same in both m_text and m_text_wrapped
        // only because wordwrapping only replaces ' ' with '\n': in other words,
        // this is fairly brittle
        ASSERT(full_text.size() == m_text_wrapped.tostring().size());
        // index of the first and last ops that are being displayed
        int begin_idx = ops_min == 0 ?
                        0 :
                        m_text_wrapped.tostring(0, ops_min - 1).size();
        int end_idx = begin_idx
                        + m_text_wrapped.tostring(ops_min, ops_max-1).size();

        vector<size_t> highlights = _find_highlights(full_text, hl_pat,
                                                     begin_idx, end_idx);

        int ox = m_region.x;
        const int oy = display_density.logical_to_device(m_region.y) +
                                                dev_line_height * line_off;
        size_t lacc = 0; // the start char of the current op relative to region
        size_t line = 0; // the line we are at relative to the region

        bool inside = false;
        // Iterate over formatted_string op slices, looking for highlight
        // sequences. Highlight sequences may span multiple op slices, hence
        // some of the complexity here.
        // All the y math in this section needs to be done in device pixels,
        // in order to handle fractional advances.
        set<size_t> block_lines;
        for (unsigned i = 0; i < slice.ops.size() && !highlights.empty(); i++)
        {
            const auto& op = slice.ops[i];
            if (op.type != FSOP_TEXT)
                continue;
            size_t oplen = op.text.size();

            // dimensions in chars of the pattern relative to current op
            size_t start = highlights[0] - begin_idx - lacc;
            size_t end = highlights[0] - begin_idx - lacc + hl_pat.size();

            size_t op_line_start = begin_idx + lacc > 0
                        ? full_text.rfind('\n', begin_idx + lacc - 1)
                        : string::npos;
            op_line_start = op_line_start == string::npos
                        ? 0
                        : op_line_start + 1;
            string line_before_op = full_text.substr(op_line_start,
                                            begin_idx + lacc - op_line_start);
            // pixel positions for the current op
            size_t op_x = m_font->string_width(line_before_op.c_str());
            const size_t op_y =
                            m_font->string_height(line_before_op.c_str(), false)
                            - dev_line_height;

            // positions in device pixels to highlight relative to current op
            size_t sx = 0, ex = m_font->string_width(op.text.c_str());
            size_t sy = 0, ey = dev_line_height;

            bool started = false; // does the highlight start in the current op?
            bool ended = false;   // does the highlight end in the current op?

            if (start < oplen) // assume start is unsigned and so >=0
            {
                // hacky: reset op x to 0 if we've hit a linebreak in the op
                // before start. This is to handle cases where the op starts
                // midline, but the pattern is after a linebreak in the op.
                const size_t linebreak_in_op = op.text.find("\n");
                if (start > linebreak_in_op)
                    op_x = 0;
                // start position is somewhere in the current op
                const string before = full_text.substr(begin_idx + lacc, start);
                sx = m_font->string_width(before.c_str());
                sy = m_font->string_height(before.c_str(), false)
                                                            - dev_line_height;
                started = true;
            }
            if (end <= oplen) // assume end is unsigned and so >=0
            {
                const string to_end = full_text.substr(begin_idx + lacc, end);
                ex = m_font->string_width(to_end.c_str());
                ey = m_font->string_height(to_end.c_str(), false);
                ended = true;
            }

            if (started || ended || inside)
            {
                m_hl_buf.clear();
                // TODO: as far as I can tell, the above code never produces
                // multi-line spans for this to iterate over...
                for (size_t y = oy + op_y + line + sy;
                            y < oy + op_y + line + ey;
                            y += dev_line_height)
                {
                    if (block_lines.count(y)) // kind of brittle...
                        continue;
                    if (hl_line)
                    {
                        block_lines.insert(y);
                        m_hl_buf.add(region.x,
                            display_density.device_to_logical(y),
                            region.x + region.width,
                            display_density.device_to_logical(y
                                                            + dev_line_height),
                            VColour(255, 255, 0, 50));
                    }
                    else
                    {
                        m_hl_buf.add(ox + op_x + sx,
                            display_density.device_to_logical(y),
                            ox + op_x + ex,
                            display_density.device_to_logical(y
                                                            + dev_line_height),
                            VColour(255, 255, 0, 50));
                    }
                }
                m_hl_buf.draw();
            }
            inside = !ended && (inside || started);

            if (ended)
            {
                highlights.erase(highlights.begin() + 0);
                i--;
            }
            else
            {
                lacc += oplen;
                line += m_font->string_height(op.text.c_str(), false)
                                                            - dev_line_height;
            }
        }
    }

    // XXX: should be moved into a new function render_formatted_string()
    // in FTFontWrapper, that, like render_textblock(), would automatically
    // handle swapping atlas glyphs as necessary.
    FontBuffer m_font_buf(m_font);
    m_font_buf.add(slice, m_region.x, m_region.y +
            display_density.device_to_logical(dev_line_height * line_off));
    m_font_buf.draw();
#else
    const auto& lines = m_wrapped_lines;
    vector<size_t> highlights;
    int begin_idx = 0;

    clear_text_region(m_region, m_bg_colour);

    if (!hl_pat.empty())
    {
        for (int i = 0; i < region.y-m_region.y; i++)
            begin_idx += m_wrapped_lines[i].tostring().size()+1;
        int end_idx = begin_idx;
        for (int i = region.y-m_region.y; i < region.y-m_region.y+region.height; i++)
            end_idx += m_wrapped_lines[i].tostring().size()+1;
        highlights = _find_highlights(m_text.tostring(), hl_pat, begin_idx, end_idx);
    }

    unsigned int hl_idx = 0;
    for (size_t i = 0; i < min(lines.size(), (size_t)region.height); i++)
    {
        cgotoxy(region.x+1, region.y+1+i);
        formatted_string line = lines[i+region.y-m_region.y];
        int end_idx = begin_idx + line.tostring().size();

        // convert highlights on this line to a list of line cuts
        vector<size_t> cuts = {0};
        for (; hl_idx < highlights.size() && (int)highlights[hl_idx] < end_idx; hl_idx++)
        {
            ASSERT(highlights[hl_idx]+hl_pat.size() >= (size_t)begin_idx);
            int la = max((int)highlights[hl_idx] - begin_idx, 0);
            int lb = min(highlights[hl_idx]+hl_pat.size() - begin_idx, (size_t)end_idx - begin_idx);
            ASSERT(la < lb);
            cuts.push_back(la);
            cuts.push_back(lb);
        }
        cuts.push_back(end_idx - begin_idx);

        // keep the last highlight if it extend into the next line
        if (hl_idx && highlights[hl_idx-1]+hl_pat.size() > (size_t)end_idx)
            hl_idx--;

        // cut the line, and highlight alternate segments
        formatted_string out;
        for (size_t j = 0; j+1 < cuts.size(); j++)
        {
            formatted_string slice = line.substr_bytes(cuts[j], cuts[j+1]-cuts[j]);
            if (j%2)
            {
                out.textcolour(WHITE);
                out.cprintf("%s", slice.tostring().c_str());
            }
            else
                out += slice;
        }
        out.chop(region.width).display(0);

        begin_idx = end_idx + 1; // +1 is for the newline
    }
#endif
}

SizeReq Text::_get_preferred_size(Direction dim, int prosp_width)
{
#ifdef USE_TILE_LOCAL
    if (!dim)
    {
        int w = m_font->string_width(m_text);
        // XXX: should be width of '..', unless string itself is shorter than '..'
        static constexpr int min_ellipsized_width = 0;
        static constexpr int min_wrapped_width = 0; // XXX: should be width of longest word
        return { ellipsize ? min_ellipsized_width : wrap_text ? min_wrapped_width : w, w };
    }
    else
    {
        wrap_text_to_size(prosp_width, 0);
        int height = m_font->string_height(m_text_wrapped);
        return { ellipsize ? (int)m_font->char_height() : height, height };
    }
#else
    if (!dim)
    {
        int w = 0, line_w = 0;
        for (auto const& ch : m_text.tostring())
        {
            w = ch == '\n' ? max(w, line_w) : w;
            line_w = ch == '\n' ? 0 : line_w+1;
        }
        w = max(w, line_w);

        // XXX: should be width of '..', unless string itself is shorter than '..'
        static constexpr int min_ellipsized_width = 0;
        static constexpr int min_wrapped_width = 0; // XXX: should be char width of longest word in text
        return { ellipsize ? min_ellipsized_width : wrap_text ? min_wrapped_width : w, w };
    }
    else
    {
        wrap_text_to_size(prosp_width, 0);
        int height = m_wrapped_lines.size();
        return { ellipsize ? 1 : height, height };
    }
#endif
}

void Text::_allocate_region()
{
    wrap_text_to_size(m_region.width, m_region.height);
}

#ifndef USE_TILE_LOCAL
void Text::set_bg_colour(COLOURS colour)
{
    m_bg_colour = colour;
    _expose();
}
#endif

void Image::set_tile(tile_def tile)
{
#ifdef USE_TILE
    m_tile = tile;
#ifdef USE_TILE_LOCAL
    const tile_info &ti = tiles.get_image_manager()->tile_def_info(m_tile);
    m_tw = ti.width;
    m_th = ti.height;
    _invalidate_sizereq();
#endif
#else
    UNUSED(tile);
#endif
}

void Image::_render()
{
#ifdef USE_TILE_LOCAL
    scissor_stack.push(m_region);
    TileBuffer tb;
    tb.set_tex(&tiles.get_image_manager()->m_textures[get_tile_texture(m_tile.tile)]);

    for (int y = m_region.y; y < m_region.y+m_region.height; y+=m_th)
        for (int x = m_region.x; x < m_region.x+m_region.width; x+=m_tw)
            tb.add(m_tile.tile, x, y, 0, 0, false, m_th, 1.0, 1.0);

    tb.draw();
    tb.clear();
    scissor_stack.pop();
#endif
}

SizeReq Image::_get_preferred_size(Direction dim, int /*prosp_width*/)
{
#ifdef USE_TILE_LOCAL
    return {
        // expand takes precedence over shrink for historical reasons
        dim ? (shrink_v ? 0 : m_th) : (shrink_h ? 0 : m_tw),
        dim ? (shrink_v ? 0 : m_th) : (shrink_h ? 0 : m_tw)
    };
#else
    UNUSED(dim);
    return { 0, 0 };
#endif
}

void Stack::add_child(shared_ptr<Widget> child)
{
    child->_set_parent(this);
    m_children.push_back(move(child));
    _invalidate_sizereq();
    _queue_allocation();
}

void Stack::pop_child()
{
    if (!m_children.size())
        return;
    m_children.pop_back();
    _invalidate_sizereq();
    _queue_allocation();
}

shared_ptr<Widget> Stack::get_child_at_offset(int x, int y)
{
    if (m_children.size() == 0)
        return nullptr;
    bool inside = m_children.back()->get_region().contains_point(x, y);
    return inside ? m_children.back() : nullptr;
}

void Stack::_render()
{
    for (auto const& child : m_children)
        child->render();
}

SizeReq Stack::_get_preferred_size(Direction dim, int prosp_width)
{
    SizeReq r = { 0, 0 };
    for (auto const& child : m_children)
    {
        SizeReq c = child->get_preferred_size(dim, prosp_width);
        r.min = max(r.min, c.min);
        r.nat = max(r.nat, c.nat);
    }
    return r;
}

void Stack::_allocate_region()
{
    for (auto const& child : m_children)
    {
        Region cr = m_region;
        SizeReq pw = child->get_preferred_size(Widget::HORZ, -1);
        cr.width = min(max(pw.min, m_region.width), pw.nat);
        SizeReq ph = child->get_preferred_size(Widget::VERT, cr.width);
        cr.height = min(max(ph.min, m_region.height), ph.nat);
        child->allocate_region(cr);
    }
}

void Switcher::add_child(shared_ptr<Widget> child)
{
    // TODO XXX: if there's a focused widget
    // - it must be in the current top child
    // - unfocus it before we
    child->_set_parent(this);
    m_children.push_back(move(child));
    _invalidate_sizereq();
    _queue_allocation();
}

int& Switcher::current()
{
    // TODO XXX: we need to update the focused widget
    // so we need an API change
    _expose();
    return m_current;
}

shared_ptr<Widget> Switcher::current_widget()
{
    if (m_children.size() == 0)
        return nullptr;
    m_current = max(0, min(m_current, (int)m_children.size()-1));
    return m_children[m_current];
}

void Switcher::_render()
{
    if (m_children.size() == 0)
        return;
    m_current = max(0, min(m_current, (int)m_children.size()-1));
    m_children[m_current]->render();
}

SizeReq Switcher::_get_preferred_size(Direction dim, int prosp_width)
{
    SizeReq r = { 0, 0 };
    for (auto const& child : m_children)
    {
        SizeReq c = child->get_preferred_size(dim, prosp_width);
        r.min = max(r.min, c.min);
        r.nat = max(r.nat, c.nat);
    }
    return r;
}

void Switcher::_allocate_region()
{
    for (auto const& child : m_children)
    {
        Region cr = m_region;
        SizeReq pw = child->get_preferred_size(Widget::HORZ, -1);
        cr.width = min(max(pw.min, m_region.width), pw.nat);
        SizeReq ph = child->get_preferred_size(Widget::VERT, cr.width);
        cr.height = min(max(ph.min, m_region.height), ph.nat);
        int xo, yo;
        switch (align_x)
        {
            case Widget::START:   xo = 0; break;
            case Widget::CENTER:  xo = (m_region.width - cr.width)/2; break;
            case Widget::END:     xo = m_region.width - cr.width; break;
            case Widget::STRETCH: xo = 0; break;
            default: ASSERT(0);
        }
        switch (align_y)
        {
            case Widget::START:   yo = 0; break;
            case Widget::CENTER:  yo = (m_region.height - cr.height)/2; break;
            case Widget::END:     yo = m_region.height - cr.height; break;
            case Widget::STRETCH: yo = 0; break;
            default: ASSERT(0);
        }
        cr.width += xo;
        cr.height += yo;
        if (align_x == Widget::STRETCH)
            cr.width = m_region.width;
        if (align_y == Widget::STRETCH)
            cr.height = m_region.height;
        child->allocate_region(cr);
    }
}

shared_ptr<Widget> Switcher::get_child_at_offset(int x, int y)
{
    if (m_children.size() == 0)
        return nullptr;

    int c = max(0, min(m_current, (int)m_children.size()));
    bool inside = m_children[c]->get_region().contains_point(x, y);
    return inside ? m_children[c] : nullptr;
}

shared_ptr<Widget> Grid::get_child_at_offset(int x, int y)
{
    int lx = x - m_region.x;
    int ly = y - m_region.y;
    int row = -1, col = -1;
    for (int i = 0; i < (int)m_col_info.size(); i++)
    {
        const auto& tr = m_col_info[i];
        if (lx >= tr.offset && lx < tr.offset + tr.size)
        {
            col = i;
            break;
        }
    }
    for (int i = 0; i < (int)m_row_info.size(); i++)
    {
        const auto& tr = m_row_info[i];
        if (ly >= tr.offset && ly < tr.offset + tr.size)
        {
            row = i;
            break;
        }
    }
    if (row == -1 || col == -1)
        return nullptr;
    for (auto& child : m_child_info)
    {
        if (child.pos.x <= col && col < child.pos.x + child.span.width)
        if (child.pos.y <= row && row < child.pos.y + child.span.height)
        if (child.widget->get_region().contains_point(x, y))
            return child.widget;
    }
    return nullptr;
}

void Grid::add_child(shared_ptr<Widget> child, int x, int y, int w, int h)
{
    child->_set_parent(this);
    child_info ch = { {x, y}, {w, h}, move(child) };
    m_child_info.push_back(ch);
    m_track_info_dirty = true;
    _invalidate_sizereq();
}

void Grid::init_track_info()
{
    if (!m_track_info_dirty)
        return;
    m_track_info_dirty = false;

    // calculate the number of rows and columns
    int n_rows = 0, n_cols = 0;
    for (auto info : m_child_info)
    {
        n_rows = max(n_rows, info.pos.y+info.span.height);
        n_cols = max(n_cols, info.pos.x+info.span.width);
    }
    m_row_info.resize(n_rows);
    m_col_info.resize(n_cols);

    sort(m_child_info.begin(), m_child_info.end(),
            [](const child_info& a, const child_info& b) {
        return a.pos.y < b.pos.y;
    });
}

void Grid::_render()
{
    // Find the visible rows
    const auto scissor = scissor_stack.top();
    int row_min = 0, row_max = m_row_info.size()-1, i = 0;
    for (; i < (int)m_row_info.size(); i++)
        if (m_row_info[i].offset+m_row_info[i].size+m_region.y >= scissor.y)
        {
            row_min = i;
            break;
        }
    for (; i < (int)m_row_info.size(); i++)
        if (m_row_info[i].offset+m_region.y >= scissor.ey())
        {
            row_max = i-1;
            break;
        }

    for (auto const& child : m_child_info)
    {
        if (child.pos.y < row_min) continue;
        if (child.pos.y > row_max) break;
        child.widget->render();
    }
}

void Grid::compute_track_sizereqs(Direction dim)
{
    auto& track = dim ? m_row_info : m_col_info;
#define DIV_ROUND_UP(n, d) (((n)+(d)-1)/(d))

    for (auto& t : track)
        t.sr = {0, 0};
    for (size_t i = 0; i < m_child_info.size(); i++)
    {
        auto& cp = m_child_info[i].pos;
        auto& cs = m_child_info[i].span;
        // if merging horizontally, need to find (possibly multi-col) width
        int prosp_width = dim ? get_tracks_region(cp.x, cp.y, cs.width, cs.height).width : -1;

        const SizeReq c = m_child_info[i].widget->get_preferred_size(dim, prosp_width);
        // NOTE: items spanning multiple rows/cols don't contribute!
        if (cs.width == 1 && cs.height == 1)
        {
            auto& s = track[dim ? cp.y : cp.x].sr;
            s.min = max(s.min, c.min);
            s.nat = max(s.nat, c.nat);
        }
    }
}

void Grid::set_track_offsets(vector<track_info>& tracks)
{
    int acc = 0;
    for (auto& track : tracks)
    {
        track.offset = acc;
        acc += track.size;
    }
}

SizeReq Grid::_get_preferred_size(Direction dim, int prosp_width)
{
    init_track_info();

    // get preferred column widths
    compute_track_sizereqs(Widget::HORZ);

    // total width min and nat
    SizeReq w_sr = { 0, 0 };
    for (auto const& col : m_col_info)
    {
        w_sr.min += col.sr.min;
        w_sr.nat += col.sr.nat;
    }

    if (!dim)
        return w_sr;

    layout_track(Widget::HORZ, w_sr, prosp_width);
    set_track_offsets(m_col_info);

    // get preferred row heights for those widths
    compute_track_sizereqs(Widget::VERT);

    // total height min and nat
    SizeReq h_sr = { 0, 0 };
    for (auto const& row : m_row_info)
    {
        h_sr.min += row.sr.min;
        h_sr.nat += row.sr.nat;
    }

    return h_sr;
}

void Grid::layout_track(Direction dim, SizeReq sr, int size)
{
    auto& infos = dim ? m_row_info : m_col_info;

    int extra = size - sr.min;
    ASSERT(extra >= 0);

    for (size_t i = 0; i < infos.size(); ++i)
        infos[i].size = infos[i].sr.min;

    const bool stretch = dim ? stretch_v : stretch_h;
    bool stretching = false;

    while (true)
    {
        int sum_flex_grow = 0, sum_taken = 0;
        for (const auto& info : infos)
            sum_flex_grow += info.size < info.sr.nat ? info.flex_grow : 0;
        if (!sum_flex_grow)
        {
            if (!stretch)
                break;
            stretching = true;
            for (const auto& info : infos)
                sum_flex_grow += info.flex_grow;
            if (!sum_flex_grow)
                break;
        }

        for (size_t i = 0; i < infos.size(); ++i)
        {
            float efg = (infos[i].size < infos[i].sr.nat || stretching)
                ? infos[i].flex_grow : 0;
            int tr_extra = extra * efg / sum_flex_grow;
            ASSERT(stretching || infos[i].size <= infos[i].sr.nat);
            int taken = stretching ? tr_extra
                : min(tr_extra, infos[i].sr.nat - infos[i].size);
            infos[i].size += taken;
            sum_taken += taken;
        }
        if (!sum_taken)
            break;
        extra = extra - sum_taken;
    }
}

void Grid::_allocate_region()
{
    // Use of _-prefixed member function is necessary here
    SizeReq h_sr = _get_preferred_size(Widget::VERT, m_region.width);

    layout_track(Widget::VERT, h_sr, m_region.height);
    set_track_offsets(m_row_info);

    for (size_t i = 0; i < m_child_info.size(); i++)
    {
        auto& cp = m_child_info[i].pos;
        auto& cs = m_child_info[i].span;
        Region cell_reg = get_tracks_region(cp.x, cp.y, cs.width, cs.height);
        cell_reg.x += m_region.x;
        cell_reg.y += m_region.y;
        m_child_info[i].widget->allocate_region(cell_reg);
    }
}

void Scroller::set_scroll(int y)
{
    if (m_scroll == y)
        return;
    m_scroll = y;
    _queue_allocation();
#ifdef USE_TILE_WEB
    tiles.json_open_object();
    tiles.json_write_string("msg", "ui-scroller-scroll");
    // XXX: always false, since we do not yet synchronize
    // webtiles client-side scrolls
    tiles.json_write_bool("from_webtiles", false);
    tiles.json_write_int("scroll", y);
    tiles.json_close_object();
    tiles.finish_message();
#endif
}

void Scroller::_render()
{
    if (m_child)
    {
        scissor_stack.push(m_region);
        m_child->render();
#ifdef USE_TILE_LOCAL
        m_shade_buf.draw();
#endif
        scissor_stack.pop();
#ifdef USE_TILE_LOCAL
        m_scrollbar_buf.draw();
#endif
    }
}

SizeReq Scroller::_get_preferred_size(Direction dim, int prosp_width)
{
    if (!m_child)
        return { 0, 0 };

    SizeReq sr = m_child->get_preferred_size(dim, prosp_width);
    if (dim) sr.min = 0; // can shrink to zero height
    return sr;
}

void Scroller::_allocate_region()
{
    SizeReq sr = m_child->get_preferred_size(Widget::VERT, m_region.width);
    m_scroll = max(0, min(m_scroll, sr.nat-m_region.height));
    Region ch_reg = {m_region.x, m_region.y-m_scroll, m_region.width, sr.nat};
    m_child->allocate_region(ch_reg);

#ifdef USE_TILE_LOCAL
    int shade_height = 12, ds = 4;
    int shade_top = min({m_scroll/ds, shade_height, m_region.height/2});
    int shade_bot = min({(sr.nat-m_region.height-m_scroll)/ds, shade_height, m_region.height/2});
    const VColour col_a(4, 2, 4, 0), col_b(4, 2, 4, 200);

    m_shade_buf.clear();
    m_scrollbar_buf.clear();
    {
        GLWPrim rect(m_region.x, m_region.y+shade_top-shade_height,
                m_region.x+m_region.width, m_region.y+shade_top);
        rect.set_col(col_b, col_a);
        m_shade_buf.add_primitive(rect);
    }
    {
        GLWPrim rect(m_region.x, m_region.y+m_region.height-shade_bot,
                m_region.x+m_region.width, m_region.y+m_region.height-shade_bot+shade_height);
        rect.set_col(col_a, col_b);
        m_shade_buf.add_primitive(rect);
    }
    if (ch_reg.height > m_region.height && m_scrolbar_visible) {
        const int x = m_region.x+m_region.width;
        const float h_percent = m_region.height / (float)ch_reg.height;
        const int h = m_region.height*min(max(0.05f, h_percent), 1.0f);
        const float scroll_percent = m_scroll/(float)(ch_reg.height-m_region.height);
        const int y = m_region.y + (m_region.height-h)*scroll_percent;
        GLWPrim bg_rect(x+10, m_region.y, x+12, m_region.y+m_region.height);
        bg_rect.set_col(VColour(41, 41, 41));
        m_scrollbar_buf.add_primitive(bg_rect);
        GLWPrim fg_rect(x+10, y, x+12, y+h);
        fg_rect.set_col(VColour(125, 98, 60));
        m_scrollbar_buf.add_primitive(fg_rect);
    }
#endif
}

bool Scroller::on_event(const Event& event)
{
    if (Bin::on_event(event))
        return true;
#ifdef USE_TILE_LOCAL
    const int line_delta = 20;
#else
    const int line_delta = 1;
#endif
    int delta = 0;
    if (event.type() == Event::Type::KeyDown)
    {
        const auto key = static_cast<const KeyEvent&>(event).key();
        switch (key)
        {
            case ' ': case '+': case CK_PGDN: case '>': case '\'':
                delta = m_region.height;
                break;

            case '-': case CK_PGUP: case '<': case ';':
                delta = -m_region.height;
                break;

            case CK_UP:
                delta = -line_delta;
                break;

            case CK_DOWN:
            case CK_ENTER:
                delta = line_delta;
                break;

            case CK_HOME:
                set_scroll(0);
                return true;

            case CK_END:
                set_scroll(INT_MAX);
                return true;
        }
    }
    else if (event.type() == Event::Type::MouseWheel)
    {
        auto mouse_event = static_cast<const MouseEvent&>(event);
        delta = -1 * mouse_event.wheel_dy() * line_delta;
    }
    else if (event.type() == Event::Type::MouseDown
             && static_cast<const MouseEvent&>(event).button() == MouseEvent::Button::Left)
        delta = line_delta;
    if (delta != 0)
    {
        set_scroll(m_scroll+delta);
        return true;
    }
    return false;
}

Layout::Layout(shared_ptr<Widget> child)
{
#ifdef USE_TILE_LOCAL
    m_depth = ui_root.num_children();
#endif
    child->_set_parent(this);
    m_child = move(child);
    expand_h = expand_v = true;
}

void Layout::_render()
{
    m_child->render();
}

SizeReq Layout::_get_preferred_size(Direction dim, int prosp_width)
{
    return m_child->get_preferred_size(dim, prosp_width);
}

void Layout::_allocate_region()
{
    m_child->allocate_region(m_region);
}

void Popup::_render()
{
#ifdef USE_TILE_LOCAL
    m_buf.draw();
#endif
    m_child->render();
}

SizeReq Popup::_get_preferred_size(Direction dim, int prosp_width)
{
#ifdef USE_TILE_LOCAL
    // can be called with a prosp_width that is narrower than the child's
    // minimum width, since our returned SizeReq has a minimum of 0
    if (dim == VERT)
    {
        SizeReq hsr = m_child->get_preferred_size(HORZ, -1);
        prosp_width = max(prosp_width, hsr.min);
    }
#endif
    SizeReq sr = m_child->get_preferred_size(dim, prosp_width);
#ifdef USE_TILE_LOCAL
    const int pad = base_margin() + m_padding;
    return {
        0, sr.nat + 2*pad + (dim ? m_depth*m_depth_indent*(!m_centred) : 0)
    };
#else
    return { sr.min, sr.nat };
#endif
}

void Popup::_allocate_region()
{
    Region region = m_region;
#ifdef USE_TILE_LOCAL
    m_buf.clear();
    m_buf.add(m_region.x, m_region.y,
            m_region.x + m_region.width, m_region.y + m_region.height,
            VColour(0, 0, 0, 150));
    const int pad = base_margin() + m_padding;
    region.width -= 2*pad;
    region.height -= 2*pad + m_depth*m_depth_indent*(!m_centred);

    SizeReq hsr = m_child->get_preferred_size(HORZ, -1);
    region.width = max(hsr.min, min(region.width, hsr.nat));
    SizeReq vsr = m_child->get_preferred_size(VERT, region.width);
    region.height = max(vsr.min, min(region.height, vsr.nat));

    region.x += pad + (m_region.width-2*pad-region.width)/2;
    region.y += pad + (m_centred ? (m_region.height-2*pad-region.height)/2
            : m_depth*m_depth_indent);

    m_buf.add(region.x - m_padding, region.y - m_padding,
            region.x + region.width + m_padding,
            region.y + region.height + m_padding,
            VColour(125, 98, 60));
    m_buf.add(region.x - m_padding + 2, region.y - m_padding + 2,
            region.x + region.width + m_padding - 2,
            region.y + region.height + m_padding - 2,
            VColour(0, 0, 0));
    m_buf.add(region.x - m_padding + 3, region.y - m_padding + 3,
            region.x + region.width + m_padding - 3,
            region.y + region.height + m_padding - 3,
            VColour(4, 2, 4));
#else
    SizeReq hsr = m_child->get_preferred_size(HORZ, -1);
    region.width = max(hsr.min, min(region.width, hsr.nat));
    SizeReq vsr = m_child->get_preferred_size(VERT, region.width);
    region.height = max(vsr.min, min(region.height, vsr.nat));
#endif
    m_child->allocate_region(region);
}

Size Popup::get_max_child_size()
{
    Size max_child_size = Size(m_region.width, m_region.height);
#ifdef USE_TILE_LOCAL
    const int pad = base_margin() + m_padding;
    max_child_size.width = (max_child_size.width - 2*pad) & ~0x1;
    max_child_size.height = (max_child_size.height - 2*pad - m_depth*m_depth_indent) & ~0x1;
#endif
    return max_child_size;
}

#ifdef USE_TILE_LOCAL
int Popup::base_margin()
{
    const int screen_small = 800, screen_large = 1000;
    const int margin_small = 10, margin_large = 50;
    const int clipped = max(screen_small, min(screen_large, m_region.height));
    return margin_small + (clipped-screen_small)
            *(margin_large-margin_small)/(screen_large-screen_small);
}
#endif

void Checkbox::_render()
{
    if (m_child)
        m_child->render();

    const bool has_focus = ui::get_focused_widget() == this;

#ifdef USE_TILE_LOCAL
    tileidx_t tile = TILEG_CHECKBOX;
    if (m_checked)
        tile += 1;
    if (m_hovered || has_focus)
        tile += 2;

    const int x = m_region.x, y = m_region.y;
    TileBuffer tb;
    tb.set_tex(&tiles.get_image_manager()->m_textures[TEX_GUI]);
    tb.add(tile, x, y, 0, 0, false, check_h, 1.0, 1.0);
    tb.draw();
#else
    cgotoxy(m_region.x+1, m_region.y+1, GOTO_CRT);
    textbackground(has_focus ? LIGHTGREY : BLACK);
    cprintf("[ ]");
    if (m_checked)
    {
        cgotoxy(m_region.x+2, m_region.y+1, GOTO_CRT);
        textcolour(has_focus ? BLACK : WHITE);
        cprintf("X");
    }
#endif
}

const int Checkbox::check_w;
const int Checkbox::check_h;

SizeReq Checkbox::_get_preferred_size(Direction dim, int prosp_width)
{
    SizeReq child_sr = { 0, 0 };
    if (m_child)
        child_sr = m_child->get_preferred_size(dim, prosp_width);

    if (dim == HORZ)
        return { child_sr.min + check_w, child_sr.nat + check_w };
    else
        return { max(child_sr.min, check_h), max(child_sr.nat, check_h) };
}

void Checkbox::_allocate_region()
{
    if (m_child)
    {
        auto child_region = m_region;
        child_region.x += check_w;
        child_region.width -= check_w;
        auto child_sr = m_child->get_preferred_size(VERT, child_region.width);
        child_region.height = min(max(child_sr.min, m_region.height), child_sr.nat);
        child_region.y += (m_region.height - child_region.height)/2;
        m_child->allocate_region(child_region);
    }
}

bool Checkbox::on_event(const Event& event)
{
#ifdef USE_TILE_LOCAL
    if (event.type() == Event::Type::MouseEnter || event.type() == Event::Type::MouseLeave)
    {
        bool new_hovered = event.type() == Event::Type::MouseEnter;
        if (new_hovered != m_hovered)
            _expose();
        m_hovered = new_hovered;
    }
    if (event.type() == Event::Type::MouseDown)
    {
        set_checked(!checked());
        set_focused_widget(this);
        _expose();
        return true;
    }
#endif
    if (event.type() == Event::Type::FocusIn || event.type() == Event::Type::FocusOut)
    {
        _expose();
        return true;
    }
    if (event.type() == Event::Type::KeyDown)
    {
        const auto key = static_cast<const KeyEvent&>(event).key();
        if (key == CK_ENTER || key == ' ')
        {
            set_checked(!checked());
            _expose();
            return true;
        }
    }
    return false;
}

#ifdef USE_TILE_WEB
void Checkbox::sync_save_state()
{
    tiles.json_write_bool("checked", m_checked);
}

void Checkbox::sync_load_state(const JsonNode *json)
{
    if (auto c = json_find_member(json, "checked"))
        if (c->tag == JSON_BOOL)
            set_checked(c->bool_);
}
#endif

TextEntry::TextEntry() : m_line_reader(m_buffer, sizeof(m_buffer))
{
#ifdef USE_TILE_LOCAL
    set_font(tiles.get_crt_font());
#endif
}

void TextEntry::_render()
{
    const bool has_focus = ui::get_focused_widget() == this;

#ifdef USE_TILE_LOCAL
    const int line_height = m_font->char_height();
    const int text_y = m_region.y + (m_region.height - line_height)/2;

    const auto bg = has_focus ? VColour(30, 30, 30, 255)
                              : VColour(29, 27, 21, 255);

    m_buf.clear();
    m_buf.add(m_region.x, m_region.y, m_region.ex(), m_region.ey(), bg);
    m_buf.draw();

    const auto border_bg = has_focus ? VColour(184, 141, 25)
                                     : VColour(125, 98, 60);

    LineBuffer bbuf;
    bbuf.add_square(m_region.x+1, m_region.y+1,
                    m_region.ex(), m_region.ey(), border_bg);
    bbuf.draw();

    const int content_width = m_font->string_width(m_text.c_str());
    const int cursor_x = m_font->string_width(
            m_text.substr(0, m_cursor).c_str());
    constexpr int x_pad = 3;
#else
    const int content_width = strwidth(m_text);
    const int cursor_x = strwidth(m_text.substr(0, m_cursor));
    constexpr int x_pad = 0;
#endif

    const int viewport_width = m_region.width - 2*x_pad;

    // Scroll to keep the cursor in view
    if (cursor_x < m_hscroll)
        m_hscroll = cursor_x;
    else if (cursor_x >= m_hscroll + viewport_width)
        m_hscroll = cursor_x - viewport_width + 1;

    // scroll to keep the textbox full of text, if possible
    m_hscroll = min(m_hscroll, max(0, content_width - viewport_width + 1));

#ifdef USE_TILE_LOCAL
    // XXX: we need to transform the scissor because the skill menu is rendered
    // using the CRT, with an appropriate transform that positions it into the
    // centre of the screen
    GLW_3VF translate;
    glmanager->get_transform(&translate, nullptr);
    const Region scissor_region = {
        m_region.x - static_cast<int>(translate.x),
        m_region.y - static_cast<int>(translate.y),
        m_region.width, m_region.height,
    };
    scissor_stack.push(scissor_region);

    const int text_x = m_region.x - m_hscroll + x_pad;

    FontBuffer m_font_buf(m_font);
    m_font_buf.add(formatted_string(m_text), text_x, text_y);
    m_font_buf.draw();

    scissor_stack.pop();

    if (has_focus)
    {
        m_buf.clear();
        m_buf.add(text_x + cursor_x, text_y,
                  text_x + cursor_x + 1, text_y + line_height,
                  VColour(255, 255, 255, 255));
        m_buf.draw();
    }
#else
    auto prefix_size = chop_string(m_text, m_hscroll, false).size();
    auto remain = chop_string(m_text.substr(prefix_size), m_region.width, true);

    const auto fg_colour = has_focus ? BLACK : WHITE;
    const auto bg_colour = has_focus ? LIGHTGREY : DARKGREY;

    draw_colour draw(fg_colour, bg_colour);
    cgotoxy(m_region.x+1, m_region.y+1, GOTO_CRT);
    cprintf("%s", remain.c_str());

    if (has_focus)
    {
        cgotoxy(m_region.x+cursor_x-m_hscroll+1, m_region.y+1, GOTO_CRT);
        textcolour(DARKGREY);
        cprintf(" ");
        show_cursor_at(m_region.x+cursor_x-m_hscroll+1, m_region.y+1);
    }
#endif
}

SizeReq TextEntry::_get_preferred_size(Direction dim, int /*prosp_width*/)
{
    if (!dim)
        return { 0, 300 };
    else
    {
#ifdef USE_TILE_LOCAL
        const int line_height = m_font->char_height(false);
        const int height = line_height + 2*padding_size();
        return { height, height };
#else
        return { 1, 1 };
#endif
    }
}

#ifdef USE_TILE_LOCAL
int TextEntry::padding_size()
{
    const int line_height = m_font->char_height(false);
    const float pad_amount = 0.2;
    return (static_cast<int>(line_height*pad_amount) + 1)/2;
}
#endif

void TextEntry::_allocate_region()
{
}

bool TextEntry::on_event(const Event& event)
{
    switch (event.type())
    {
    case Event::Type::FocusIn:
    case Event::Type::FocusOut:
        set_cursor_enabled(event.type() == Event::Type::FocusIn);
        _expose();
        return true;
    case Event::Type::MouseDown:
        // TODO: unfocus if the mouse is clicked outside
        // TODO: move the cursor to the clicked position
        if (static_cast<const MouseEvent&>(event).button() == MouseEvent::Button::Left)
            ui::set_focused_widget(this);
        return true;
    case Event::Type::KeyDown:
        {
            const auto key = static_cast<const KeyEvent&>(event).key();
            int ret = m_line_reader.process_key_core(key);
            if (ret == CK_ESCAPE || ret == 0)
                ui::set_focused_widget(nullptr);
            m_text = m_line_reader.get_text();
            m_cursor = m_line_reader.get_cursor_position();
            _expose();
            return key != '\t' && key != CK_SHIFT_TAB;
        }
    default:
        return false;
    }
}

#ifdef USE_TILE_LOCAL
void TextEntry::set_font(FontWrapper *font)
{
    ASSERT(font);
    m_font = font;
    _invalidate_sizereq();
}
#endif

TextEntry::LineReader::LineReader(char *buf, size_t sz)
    : buffer(buf), bufsz(sz), history(nullptr), keyfn(nullptr),
      mode(EDIT_MODE_INSERT),
      cur(nullptr), length(0)
{
    *buffer = 0;
    length = 0;
    cur = buffer;

    if (history)
        history->go_end();
}

TextEntry::LineReader::~LineReader()
{
}

string TextEntry::LineReader::get_text() const
{
    return buffer;
}

void TextEntry::LineReader::set_text(string text)
{
    snprintf(buffer, bufsz, "%s", text.c_str());
    length = min(text.size(), bufsz - 1);
    buffer[length] = 0;
    cur = buffer + length;
}

void TextEntry::LineReader::set_input_history(input_history *i)
{
    history = i;
}

void TextEntry::LineReader::set_keyproc(keyproc fn)
{
    keyfn = fn;
}

void TextEntry::LineReader::set_edit_mode(edit_mode m)
{
    mode = m;
}

void TextEntry::LineReader::set_prompt(string p)
{
    prompt = p;
}

edit_mode TextEntry::LineReader::get_edit_mode()
{
    return mode;
}

#ifdef USE_TILE_WEB
void TextEntry::LineReader::set_tag(const string &id)
{
    tag = id;
}
#endif

int TextEntry::LineReader::process_key_core(int ch)
{
    if (keyfn)
    {
        // if you intercept esc, don't forget to provide another way to
        // exit. Processing esc will safely cancel.
        keyfun_action whattodo = (*keyfn)(ch);
        if (whattodo == KEYFUN_CLEAR)
        {
            buffer[length] = 0;
            if (history && length)
                history->new_input(buffer);
            return 0;
        }
        else if (whattodo == KEYFUN_BREAK)
        {
            buffer[length] = 0;
            return ch;
        }
        else if (whattodo == KEYFUN_IGNORE)
            return -1;
        // else case: KEYFUN_PROCESS
    }

    return process_key(ch);
}

void TextEntry::LineReader::backspace()
{
    char *np = prev_glyph(cur, buffer);
    if (!np)
        return;
    char32_t ch;
    utf8towc(&ch, np);
    buffer[length] = 0;
    length -= cur - np;
    char *c = cur;
    cur = np;
    while (*c)
        *np++ = *c++;
    buffer[length] = 0;
}

void TextEntry::LineReader::delete_char()
{
    // TODO: unify with backspace
    if (*cur)
    {
        const char *np = next_glyph(cur);
        ASSERT(np);
        char32_t ch_at_point;
        utf8towc(&ch_at_point, cur);
        const size_t del_bytes = np - cur;
        const size_t follow_bytes = (buffer + length) - np;
        // Copy the NUL too.
        memmove(cur, np, follow_bytes + 1);
        length -= del_bytes;
    }
}

bool TextEntry::LineReader::is_wordchar(char32_t c)
{
    return iswalnum(c) || c == '_' || c == '-';
}

void TextEntry::LineReader::kill_to_begin()
{
    if (cur == buffer)
        return;

    const int rest = length - (cur - buffer);
    memmove(buffer, cur, rest);
    length = rest;
    buffer[length] = 0;
    cur = buffer;
}

void TextEntry::LineReader::kill_to_end()
{
    if (*cur)
    {
        length = cur - buffer;
        *cur = 0;
    }
}

void TextEntry::LineReader::killword()
{
    if (cur == buffer)
        return;

    bool foundwc = false;
    char *word = cur;
    int ew = 0;
    while (1)
    {
        char *np = prev_glyph(word, buffer);
        if (!np)
            break;

        char32_t c;
        utf8towc(&c, np);
        if (is_wordchar(c))
            foundwc = true;
        else if (foundwc)
            break;

        word = np;
        ew += wcwidth(c);
    }
    memmove(word, cur, strlen(cur) + 1);
    length -= cur - word;
    cur = word;
}

void TextEntry::LineReader::overwrite_char_at_cursor(int ch)
{
    int len = wclen(ch);
    int w = wcwidth(ch);

    if (w >= 0 && cur - buffer + len < static_cast<int>(bufsz))
    {
        bool empty = !*cur;

        wctoutf8(cur, ch);
        cur += len;
        if (empty)
            length += len;
        buffer[length] = 0;
    }
}

void TextEntry::LineReader::insert_char_at_cursor(int ch)
{
    if (wcwidth(ch) >= 0 && length + wclen(ch) < static_cast<int>(bufsz))
    {
        int len = wclen(ch);
        if (*cur)
        {
            char *c = buffer + length - 1;
            while (c >= cur)
            {
                c[len] = *c;
                c--;
            }
        }
        wctoutf8(cur, ch);
        cur += len;
        length += len;
        buffer[length] = 0;
    }
}

#ifdef USE_TILE_LOCAL
void TextEntry::LineReader::clipboard_paste()
{
    if (wm->has_clipboard())
        for (char ch : wm->get_clipboard())
            process_key(ch);
}
#endif

int TextEntry::LineReader::process_key(int ch)
{
    switch (ch)
    {
    CASE_ESCAPE
        return CK_ESCAPE;
    case CK_UP:
    case CONTROL('P'):
    case CK_DOWN:
    case CONTROL('N'):
    {
        if (!history)
            break;

        const string *text = (ch == CK_UP || ch == CONTROL('P'))
                             ? history->prev()
                             : history->next();

        if (text)
            set_text(*text);
        break;
    }
    case CK_ENTER:
        buffer[length] = 0;
        if (history && length)
            history->new_input(buffer);
        return 0;

    case CONTROL('K'):
        kill_to_end();
        break;

    case CK_DELETE:
    case CONTROL('D'):
        delete_char();
        break;

    case CK_BKSP:
        backspace();
        break;

    case CONTROL('W'):
        killword();
        break;

    case CONTROL('U'):
        kill_to_begin();
        break;

    case CK_LEFT:
    case CONTROL('B'):
        if (char *np = prev_glyph(cur, buffer))
            cur = np;
        break;
    case CK_RIGHT:
    case CONTROL('F'):
        if (char *np = next_glyph(cur))
            cur = np;
        break;
    case CK_HOME:
    case CONTROL('A'):
        cur = buffer;
        break;
    case CK_END:
    case CONTROL('E'):
        cur = buffer + length;
        break;
    case CONTROL('V'):
#ifdef USE_TILE_LOCAL
        clipboard_paste();
#endif
        break;
    case CK_REDRAW:
        //redraw_screen();
        return -1;
    default:
        if (mode == EDIT_MODE_OVERWRITE)
            overwrite_char_at_cursor(ch);
        else // mode == EDIT_MODE_INSERT
            insert_char_at_cursor(ch);

        break;
    }

    return -1;
}

#ifdef USE_TILE_WEB
void TextEntry::sync_save_state()
{
    tiles.json_write_string("text", m_text);
    tiles.json_write_int("cursor", m_cursor);
}

void TextEntry::sync_load_state(const JsonNode *json)
{
    if (auto text = json_find_member(json, "text"))
        if (text->tag == JSON_STRING)
            set_text(text->string_);

    // TODO: sync cursor state
}
#endif

#ifdef USE_TILE_LOCAL
void Dungeon::_render()
{
    GLW_3VF t = {(float)m_region.x, (float)m_region.y, 0}, s = {32, 32, 1};
    glmanager->set_transform(t, s);
    m_buf.draw();
    glmanager->reset_transform();
}

SizeReq Dungeon::_get_preferred_size(Direction dim, int /*prosp_width*/)
{
    const int sz = (dim ? height : width)*32;
    return { sz, sz };
}

PlayerDoll::PlayerDoll(dolls_data doll)
{
    m_save_doll = doll;
    for (int i = 0; i < TEX_MAX; i++)
        m_tile_buf[i].set_tex(&tiles.get_image_manager()->m_textures[i]);
    _pack_doll();
}

PlayerDoll::~PlayerDoll()
{
    for (int t = 0; t < TEX_MAX; t++)
        m_tile_buf[t].clear();
}

void PlayerDoll::_pack_doll()
{
    m_tiles.clear();
    // FIXME: Implement this logic in one place in e.g. pack_doll_buf().
    int p_order[TILEP_PART_MAX] =
    {
        TILEP_PART_SHADOW,  //  0
        TILEP_PART_HALO,
        TILEP_PART_ENCH,
        TILEP_PART_DRCWING,
        TILEP_PART_CLOAK,
        TILEP_PART_BASE,    //  5
        TILEP_PART_BOOTS,
        TILEP_PART_LEG,
        TILEP_PART_BODY,
        TILEP_PART_ARM,
        TILEP_PART_HAIR,
        TILEP_PART_BEARD,
        TILEP_PART_DRCHEAD,  // 15
        TILEP_PART_HELM,
        TILEP_PART_HAND1,   // 10
        TILEP_PART_HAND2,
    };

    int flags[TILEP_PART_MAX];
    tilep_calc_flags(m_save_doll, flags);

    // For skirts, boots go under the leg armour. For pants, they go over.
    if (m_save_doll.parts[TILEP_PART_LEG] < TILEP_LEG_SKIRT_OFS)
    {
        p_order[6] = TILEP_PART_BOOTS;
        p_order[7] = TILEP_PART_LEG;
    }

    // Special case bardings from being cut off.
    bool is_naga = (m_save_doll.parts[TILEP_PART_BASE] == TILEP_BASE_NAGA
                    || m_save_doll.parts[TILEP_PART_BASE] == TILEP_BASE_NAGA + 1);
    if (m_save_doll.parts[TILEP_PART_BOOTS] >= TILEP_BOOTS_NAGA_BARDING
        && m_save_doll.parts[TILEP_PART_BOOTS] <= TILEP_BOOTS_NAGA_BARDING_RED)
    {
        flags[TILEP_PART_BOOTS] = is_naga ? TILEP_FLAG_NORMAL : TILEP_FLAG_HIDE;
    }

    bool is_cent = (m_save_doll.parts[TILEP_PART_BASE] == TILEP_BASE_CENTAUR
                    || m_save_doll.parts[TILEP_PART_BASE] == TILEP_BASE_CENTAUR + 1);
    if (m_save_doll.parts[TILEP_PART_BOOTS] >= TILEP_BOOTS_CENTAUR_BARDING
        && m_save_doll.parts[TILEP_PART_BOOTS] <= TILEP_BOOTS_CENTAUR_BARDING_RED)
    {
        flags[TILEP_PART_BOOTS] = is_cent ? TILEP_FLAG_NORMAL : TILEP_FLAG_HIDE;
    }

    for (int i = 0; i < TILEP_PART_MAX; ++i)
    {
        const int p   = p_order[i];
        const tileidx_t idx = m_save_doll.parts[p];
        if (idx == 0 || idx == TILEP_SHOW_EQUIP || flags[p] == TILEP_FLAG_HIDE)
            continue;

        ASSERT_RANGE(idx, TILE_MAIN_MAX, TILEP_PLAYER_MAX);

        int ymax = TILE_Y;

        if (flags[p] == TILEP_FLAG_CUT_CENTAUR
            || flags[p] == TILEP_FLAG_CUT_NAGA)
        {
            ymax = 18;
        }

        m_tiles.emplace_back(idx, ymax);
    }
}

void PlayerDoll::_render()
{
    for (int i = 0; i < TEX_MAX; i++)
        m_tile_buf[i].draw();
}

SizeReq PlayerDoll::_get_preferred_size(Direction /*dim*/, int /*prosp_width*/)
{
    return { TILE_Y, TILE_Y };
}

void PlayerDoll::_allocate_region()
{
    for (int t = 0; t < TEX_MAX; t++)
        m_tile_buf[t].clear();
    for (const tile_def &tdef : m_tiles)
    {
        int tile      = tdef.tile;
        TextureID tex = get_tile_texture(tile);
        m_tile_buf[tex].add_unscaled(tile, m_region.x, m_region.y, tdef.ymax);
    }
}
#endif

void UIRoot::push_child(shared_ptr<Widget> ch, KeymapContext km)
{
    Widget* focus;
    if (auto popup = dynamic_cast<Popup*>(ch.get()))
        focus = popup->get_child().get();
    else
        focus = ch.get();

    saved_layout_info.push_back(state);
    state.keymap = km;
    state.default_focus = state.current_focus = focus;
    state.generation_id = next_generation_id++;

    m_root.add_child(move(ch));
    m_needs_layout = true;
    m_changed_layout_since_click = true;
    update_focus_order();
#ifdef USE_TILE_WEB
    update_synced_widgets();
#endif
#ifndef USE_TILE_LOCAL
    if (m_root.num_children() == 1)
    {
        clrscr();
        ui_root.resize(get_number_of_cols(), get_number_of_lines());
    }
#endif
}

void UIRoot::pop_child()
{
    top_child()->_emit_layout_pop();
    m_root.pop_child();
    m_needs_layout = true;
    state = saved_layout_info.back();
    saved_layout_info.pop_back();
    m_changed_layout_since_click = true;
    update_focus_order();
#ifdef USE_TILE_WEB
    update_synced_widgets();
#endif
#ifndef USE_TILE_LOCAL
    if (m_root.num_children() == 0)
        clrscr();
#endif
}

void UIRoot::resize(int w, int h)
{
    if (w == m_w && h == m_h)
        return;

    m_w = w;
    m_h = h;
    m_needs_layout = true;

    // On console with the window size smaller than the minimum layout,
    // enlarging the window will not cause any size reallocations, and the
    // newly visible region of the terminal will not be filled.
    // Fix: explicitly mark the entire screen as dirty on resize: it won't
    // be strictly necessary for most resizes, but won't hurt.
#ifndef USE_TILE_LOCAL
    expose_region({0, 0, w, h});
#endif
}

void UIRoot::layout()
{
    while (m_needs_layout)
    {
        m_needs_layout = false;

        // Find preferred size with height-for-width: we never allocate less than
        // the minimum size, but may allocate more than the natural size.
        SizeReq sr_horz = m_root.get_preferred_size(Widget::HORZ, -1);
        int width = max(sr_horz.min, m_w);
        SizeReq sr_vert = m_root.get_preferred_size(Widget::VERT, width);
        int height = max(sr_vert.min, m_h);

#ifdef USE_TILE_LOCAL
        m_region = {0, 0, width, height};
#else
        m_region = {0, 0, m_w, m_h};
#endif
        try
        {
            m_root.allocate_region({0, 0, width, height});
        }
        catch (const RestartAllocation &ex)
        {
        }

#ifdef USE_TILE_LOCAL
        update_hover_path();
#endif
    }
}

#ifdef USE_TILE_LOCAL
bool should_render_current_regions = true;
#endif

void UIRoot::render()
{
    if (!needs_paint)
        return;

#ifdef USE_TILE_LOCAL
    glmanager->reset_view_for_redraw();
    tiles.maybe_redraw_screen();
    if (should_render_current_regions)
        tiles.render_current_regions();
    glmanager->reset_transform();
#else
    // On console, clear and redraw only the dirty region of the screen
    m_dirty_region = m_dirty_region.aabb_intersect(m_region);
    textcolour(LIGHTGREY);
    textbackground(BLACK);
    clear_text_region(m_dirty_region, BLACK);
#endif

    scissor_stack.push(m_region);
#ifdef USE_TILE_LOCAL
    int cutoff = cutoff_stack.empty() ? 0 : cutoff_stack.back();
    ASSERT(cutoff <= static_cast<int>(m_root.num_children()));
    for (int i = cutoff; i < static_cast<int>(m_root.num_children()); i++)
        m_root.get_child(i)->render();
#ifdef DEBUG
    debug_render();
#endif
#else
    // Render only the top of the UI stack on console
    if (m_root.num_children() > 0)
        m_root.get_child(m_root.num_children()-1)->render();
    else
    {
        redraw_screen(false);
        update_screen();
    }

    if (is_cursor_enabled() && !cursor_pos.origin())
    {
        cgotoxy(cursor_pos.x, cursor_pos.y, GOTO_CRT);
        cursor_pos.reset();
    }
#endif
    scissor_stack.pop();

    needs_paint = false;
    needs_swap = true;
    m_dirty_region = {0, 0, 0, 0};
}

void UIRoot::swap_buffers()
{
    if (!needs_swap)
        return;
    needs_swap = false;
#ifdef USE_TILE_LOCAL
    wm->swap_buffers();
#else
    update_screen();
#endif
}

#ifdef DEBUG
void UIRoot::debug_render()
{
#ifdef USE_TILE_LOCAL
    if (debug_draw)
    {
        LineBuffer lb;
        ShapeBuffer sb;
        size_t i = 0;
        for (const auto& w : hover_path)
        {
            const auto r = w->get_region();
            i++;
            VColour lc;
            lc = i == hover_path.size() ?
                VColour(255, 100, 0, 100) : VColour(0, 50 + i*40, 0, 100);
            lb.add_square(r.x+1, r.y+1, r.ex(), r.ey(), lc);
        }
        if (!hover_path.empty())
        {
            const auto& hovered_widget = hover_path.back();
            Region r = hovered_widget->get_region();
            const Margin m = hovered_widget->get_margin();

            VColour lc = VColour(0, 0, 100, 100);
            sb.add(r.x, r.y-m.top, r.ex(), r.y, lc);
            sb.add(r.ex(), r.y, r.ex()+m.right, r.ey(), lc);
            sb.add(r.x, r.ey(), r.ex(), r.ey()+m.bottom, lc);
            sb.add(r.x-m.left, r.y, r.x, r.ey(), lc);
        }
        if (auto w = get_focused_widget()) {
            Region r = w->get_region();
            lb.add_square(r.x+1, r.y+1, r.ex(), r.ey(), VColour(128, 31, 239));
        }
        lb.draw();
        sb.draw();
    }
#endif
}
#endif

void UIRoot::update_focus_order()
{
    focus_order.clear();

    function<void(Widget*)> recurse = [&](Widget* widget) {
        if (widget->can_take_focus())
            focus_order.emplace_back(widget);
        widget->for_each_child_including_internal([&](shared_ptr<Widget>& ch) {
            recurse(ch.get());
        });
    };

    int layer_idx = m_root.num_children()-1;
    if (layer_idx >= 0)
    {
        auto layer_root = m_root.get_child(layer_idx).get();
        recurse(layer_root);
    }
}

void UIRoot::focus_next()
{
    if (focus_order.empty())
        return;

    const auto default_focus = state.default_focus;
    const auto current_focus = state.current_focus;

    if (!current_focus || current_focus == default_focus)
    {
        set_focused_widget(focus_order.front());
        return;
    }

    auto it = find(focus_order.begin(), focus_order.end(), current_focus);
    ASSERT(it != focus_order.end());

    do {
        if (*it == focus_order.back())
            it = focus_order.begin();
        else
            ++it;
    } while (*it != current_focus && !(*it)->focusable());

    set_focused_widget(*it);
}

void UIRoot::focus_prev()
{
    if (focus_order.empty())
        return;

    const auto default_focus = state.default_focus;
    const auto current_focus = state.current_focus;

    if (!current_focus || current_focus == default_focus)
    {
        set_focused_widget(focus_order.back());
        return;
    }

    auto it = find(focus_order.begin(), focus_order.end(), current_focus);
    ASSERT(it != focus_order.end());

    do {
        if (*it == focus_order.front())
            it = focus_order.end()-1;
        else
            --it;
    } while (*it != current_focus && !(*it)->focusable());

    set_focused_widget(*it);
}

static function<bool(const wm_event&)> event_filter;

#ifdef USE_TILE_LOCAL
void UIRoot::update_hover_path()
{
    int mx, my;
    wm->get_mouse_state(&mx, &my);

    /* Find current hover path */
    vector<Widget*> new_hover_path;
    shared_ptr<Widget> current = m_root.get_child_at_offset(mx, my);
    while (current)
    {
        new_hover_path.emplace_back(current.get());
        current = current->get_child_at_offset(mx, my);
    }

    size_t new_hover_path_size = new_hover_path.size();
    size_t sz = max(hover_path.size(), new_hover_path.size());
    hover_path.resize(sz, nullptr);
    new_hover_path.resize(sz, nullptr);

    send_mouse_enter_leave_events(hover_path, new_hover_path);

    hover_path = move(new_hover_path);
    hover_path.resize(new_hover_path_size);
}

void UIRoot::send_mouse_enter_leave_events(
        const vector<Widget*>& old_hover_path,
        const vector<Widget*>& new_hover_path)
{
    ASSERT(old_hover_path.size() == new_hover_path.size());

    // event_filter takes a wm_event, and we don't have one, so don't bother to
    // call it; this is fine, since this is a private API and it only checks for
    // keydown events.
    if (event_filter)
        return;

    const size_t size = old_hover_path.size();

    size_t diff;
    for (diff = 0; diff < size; diff++)
        if (new_hover_path[diff] != old_hover_path[diff])
            break;

    if (diff == size)
        return;

    if (old_hover_path[diff])
    {
        const wm_mouse_event dummy = {};
        MouseEvent ev(Event::Type::MouseLeave, dummy);
        for (size_t i = size; i > diff; --i)
            if (old_hover_path[i-1])
                old_hover_path[i-1]->on_event(ev);
    }

    if (new_hover_path[diff])
    {
        const wm_mouse_event dummy = {};
        MouseEvent ev(Event::Type::MouseEnter, dummy);
        for (size_t i = diff; i < size; ++i)
            if (new_hover_path[i])
                new_hover_path[i]->on_event(ev);
    }
}

void UIRoot::update_hover_path_for_widget(Widget *widget)
{
    // truncate the hover path if the widget was previously hovered,
    // but don't deliver any mouseleave events.
    if (!widget->_get_parent())
    {
        for (size_t i = 0; i < hover_path.size(); i++)
            if (hover_path[i] == widget)
            {
                hover_path.resize(i);
                break;
            }
        return;
    }

    auto top = top_layout();
    if (top && top->is_ancestor_of(widget->get_shared()))
        update_hover_path();
}
#endif

static Event::Type convert_event_type(const wm_event& event)
{
    switch (event.type)
    {
        case WME_MOUSEBUTTONDOWN: return Event::Type::MouseDown;
        case WME_MOUSEBUTTONUP: return Event::Type::MouseUp;
        case WME_MOUSEMOTION: return Event::Type::MouseMove;
        case WME_MOUSEWHEEL: return Event::Type::MouseWheel;
        case WME_MOUSEENTER: return Event::Type::MouseEnter;
        case WME_MOUSELEAVE: return Event::Type::MouseLeave;
        case WME_KEYDOWN: return Event::Type::KeyDown;
        case WME_KEYUP: return Event::Type::KeyUp;
        default: abort();
    }
}

bool UIRoot::on_event(wm_event& event)
{
    if (event_filter && event_filter(event))
        return true;

#ifdef DEBUG
    if (debug_on_event(event))
        return true;
#endif

    switch (event.type)
    {
        case WME_MOUSEBUTTONDOWN:
        case WME_MOUSEBUTTONUP:
        case WME_MOUSEMOTION:
        case WME_MOUSEWHEEL:
        {
#ifdef USE_TILE_LOCAL
            auto mouse_event = MouseEvent(convert_event_type(event),
                    event.mouse_event);
            return deliver_event(mouse_event);
#endif
            break;
        }
        case WME_KEYDOWN:
        case WME_KEYUP:
        {
            auto key_event = KeyEvent(convert_event_type(event), event.key);
            return deliver_event(key_event);
        }
        case WME_CUSTOMEVENT:
#ifdef USE_TILE_LOCAL
        {
            auto callback = reinterpret_cast<wm_timer_callback>(event.custom.data1);
            callback(0, nullptr);
            break;
        }
#else
            break;
#endif
        // TODO: maybe stop windowmanager-sdl from returning these?
        case WME_NOEVENT:
            break;
        default:
            die("unreachable, type %d", event.type);
    }

    return false;
}

bool UIRoot::deliver_event(Event& event)
{
    switch (event.type())
    {
    case Event::Type::MouseDown:
    case Event::Type::MouseUp:
#ifdef USE_TILE_LOCAL
        if (event.type() == Event::Type::MouseDown)
            m_changed_layout_since_click = false;
        else if (event.type() == Event::Type::MouseUp)
            if (m_changed_layout_since_click)
                break;
#endif
        // fall through
    case Event::Type::MouseMove:
    case Event::Type::MouseWheel:
    {
#ifdef USE_TILE_LOCAL
        if (!hover_path.empty())
        {
            event.set_target(hover_path.back()->get_shared());
            for (auto w = hover_path.back(); w; w = w->_get_parent())
                if (w->on_event(event))
                    return true;
        }
#endif
        return false;
    }

    case Event::Type::KeyDown:
    case Event::Type::KeyUp:
    {
        const auto top = top_child();
        if (!top)
            return false;

        const auto key = static_cast<const KeyEvent&>(event).key();
        event.set_target(get_focused_widget()->get_shared());

        // give hotkey handlers a chance to intercept this key; they are only
        // called if on a widget within the layout.
        if (event.type() == Event::Type::KeyDown)
        {
            // TODO: only emit if widget is visible
            bool hotkey_handled = Widget::slots.hotkey.emit_if([&](Widget* w){
                return top->is_ancestor_of(w->get_shared());
            }, event);
            if (hotkey_handled)
                return true;
            // TODO: eat the corresponding KeyUp event
        }

        if (auto w = get_focused_widget())
        {
            if (w->on_event(event))
                return true;

            if (w != state.default_focus && event.type() == Event::Type::KeyDown)
            {
                if (key_is_escape(key))
                {
                    set_focused_widget(nullptr);
                    return true;
                }
                if (key == '\t')
                {
                    focus_next();
                    return true;
                }
                if (key == CK_SHIFT_TAB)
                {
                    focus_prev();
                    return true;
                }
            }

            for (w = w->_get_parent(); w; w = w->_get_parent())
                if (w->on_event(event))
                    return true;
        }

        if (event.type() == Event::Type::KeyDown)
        {
            if (key == '\t')
            {
                focus_next();
                return true;
            }
            if (key == CK_SHIFT_TAB)
            {
                focus_prev();
                return true;
            }
        }
    }

    case Event::Type::FocusIn:
    case Event::Type::FocusOut:
        return event.target()->on_event(event);

    default:
        for (auto w = event.target().get(); w; w = w->_get_parent())
            if (w->on_event(event))
                return true;
        return false;
    }
    return false;
}

#ifdef DEBUG
bool UIRoot::debug_on_event(const wm_event& event)
{
    if (event.type == WME_KEYDOWN && event.key.keysym.sym == CK_INSERT)
    {
        ui_root.debug_draw = !ui_root.debug_draw;
        ui_root.queue_layout();
        ui_root.expose_region({0, 0, INT_MAX, INT_MAX});
        return true;
    }

    switch (event.type)
    {
        case WME_MOUSEBUTTONDOWN:
        case WME_MOUSEBUTTONUP:
        case WME_MOUSEMOTION:
            if (ui_root.debug_draw)
            {
                ui_root.queue_layout();
                ui_root.expose_region({0, 0, INT_MAX, INT_MAX});
            }
        default:
            break;
    }

    return false;
}
#endif

#ifdef USE_TILE_WEB
void UIRoot::update_synced_widgets()
{
    synced_widgets.clear();

    function<void(Widget*)> recurse = [&](Widget* widget) {
        const auto id = widget->sync_id();
        if (!id.empty())
        {
            ASSERT(synced_widgets.count(id) == 0);
            synced_widgets[id] = widget;
        }
        widget->for_each_child_including_internal([&](shared_ptr<Widget>& ch) {
            recurse(ch.get());
        });
    };

    if (const auto top = top_child())
        recurse(top.get());
}

void UIRoot::sync_state()
{
    for (const auto& it : synced_widgets)
        it.second->sync_state_changed();
}

void UIRoot::recv_ui_state_change(const JsonNode *json)
{
    const auto generation_id = json_find_member(json, "generation_id");
    if (!generation_id || generation_id->tag != JSON_NUMBER
        || generation_id->number_ != state.generation_id)
    {
        return;
    }

    const auto has_focus = json_find_member(json, "has_focus");
    if (has_focus && (has_focus->tag != JSON_BOOL || !has_focus->bool_))
        return;

    const auto widget_id = json_find_member(json, "widget_id");
    if (!widget_id)
        return;
    if (!(widget_id->tag == JSON_STRING || widget_id->tag == JSON_NULL))
        return;
    const auto widget = widget_id->tag == JSON_STRING ?
        synced_widgets.at(widget_id->string_) : nullptr;

    unwind_bool recv(receiving_ui_state, true);
    if (has_focus)
        ui::set_focused_widget(widget);
    else if (widget)
        widget->sync_load_state(json);
}
#endif

#ifndef USE_TILE_LOCAL
static void clear_text_region(Region region, COLOURS bg)
{
    region = region.aabb_intersect(scissor_stack.top());
    if (region.width <= 0 || region.height <= 0)
        return;
    textcolour(LIGHTGREY);
    textbackground(bg);
    for (int y=region.y; y < region.y+region.height; y++)
    {
        cgotoxy(region.x+1, y+1);
        cprintf("%*s", region.width, "");
    }
}
#endif

#ifdef USE_TILE
void push_cutoff()
{
    int cutoff = static_cast<int>(ui_root.num_children());
    ui_root.cutoff_stack.push_back(cutoff);
#ifdef USE_TILE_WEB
    tiles.push_ui_cutoff();
#endif
}

void pop_cutoff()
{
    ui_root.cutoff_stack.pop_back();
#ifdef USE_TILE_WEB
    tiles.pop_ui_cutoff();
#endif
}
#endif

void push_layout(shared_ptr<Widget> root, KeymapContext km)
{
    ui_root.push_child(move(root), km);
#ifdef USE_TILE_WEB
    ui_root.sync_state();
#endif
}

void pop_layout()
{
    ui_root.pop_child();
#ifdef USE_TILE_LOCAL
    ui_root.update_hover_path();
#else
    if (!has_layout())
        redraw_screen(false);
#endif
}

shared_ptr<Widget> top_layout()
{
    return ui_root.top_child();
}

void resize(int w, int h)
{
    ui_root.resize(w, h);
}

static void remap_key(wm_event &event)
{
    keyseq keys = {event.key.keysym.sym};
    macro_buf_add_with_keymap(keys, ui_root.state.keymap);
    event.key.keysym.sym = macro_buf_get();
    ASSERT(event.key.keysym.sym != -1);
}

void force_render()
{
    ui_root.layout();
    ui_root.needs_paint = true;
    ui_root.render();
    ui_root.swap_buffers();
}

void render()
{
    ui_root.layout();
    ui_root.render();
    ui_root.swap_buffers();
}

void pump_events(int wait_event_timeout)
{
    int macro_key = macro_buf_get();

#ifdef USE_TILE_LOCAL
    // Don't render while there are unhandled mousewheel events,
    // since these can come in faster than crawl can redraw.
    // unlike mousemotion events, we don't drop all but the last event
    // ...but if there are macro keys, we do need to layout (for menu UI)
    if (!wm->next_event_is(WME_MOUSEWHEEL) || macro_key != -1)
#endif
    {
        ui_root.layout();
#ifdef USE_TILE_WEB
        // On webtiles, we can't skip rendering while there are macro keys: a
        // crt screen may be opened and without a render() call, its text won't
        // won't be sent to the client(s). E.g: macro => iai
        ui_root.render();
        if (macro_key == -1)
            ui_root.swap_buffers();
#else
        if (macro_key == -1)
        {
            ui_root.render();
            ui_root.swap_buffers();
        }
#endif
    }

#ifdef USE_TILE_LOCAL
    // These WME_* events are also handled, at different times, by a
    // similar bit of code in tilesdl.cc. Roughly, that handling is used
    // during the main game display, and the this loop is used in the
    // main menu and when there are ui elements on top.
    // TODO: consolidate as much as possible
    wm_event event = {0};
    while (true)
    {
        if (macro_key != -1)
        {
            event.type = WME_KEYDOWN;
            event.key.keysym.sym = macro_key;
            break;
        }

        if (!wm->wait_event(&event, wait_event_timeout))
        {
            if (wait_event_timeout == INT_MAX)
                continue;
            else
                return;
        }

        // For consecutive mouse events, ignore all but the last,
        // since these can come in faster than crawl can redraw.
        if (event.type == WME_MOUSEMOTION && wm->next_event_is(WME_MOUSEMOTION))
            continue;
        if (event.type == WME_KEYDOWN && event.key.keysym.sym == 0)
            continue;

        // translate any key events with the current keymap
        if (event.type == WME_KEYDOWN)
            remap_key(event);
        break;
    }

    switch (event.type)
    {
        case WME_ACTIVEEVENT:
            // When game gains focus back then set mod state clean
            // to get rid of stupid Windows/SDL bug with Alt-Tab.
            if (event.active.gain != 0)
            {
                wm->set_mod_state(TILES_MOD_NONE);
                ui_root.needs_paint = true;
            }
            break;

        case WME_QUIT:
            crawl_state.seen_hups++;
            break;

        case WME_RESIZE:
        {
            // triggers ui::resize:
            tiles.resize_event(event.resize.w, event.resize.h);
            break;
        }

        case WME_MOVE:
            if (tiles.update_dpi())
                ui_root.resize(wm->screen_width(), wm->screen_height());
            ui_root.needs_paint = true;
            break;

        case WME_EXPOSE:
            ui_root.needs_paint = true;
            break;

        case WME_MOUSEMOTION:
            // FIXME: move update_hover_path() into event delivery
            ui_root.update_hover_path();
            ui_root.on_event(event);

        default:
            if (!ui_root.on_event(event) && event.type == WME_MOUSEBUTTONDOWN)
            {
                // If a mouse event wasn't handled, send it through again as a
                // fake key event, for compatibility
                int key;
                if (event.mouse_event.button == wm_mouse_event::LEFT)
                    key = CK_MOUSE_CLICK;
                else if (event.mouse_event.button == wm_mouse_event::RIGHT)
                    key = CK_MOUSE_CMD;
                else break;

                wm_event ev = {0};
                ev.type = WME_KEYDOWN;
                ev.key.keysym.sym = key;
                ui_root.on_event(ev);
            }
            break;
    }
#else
    if (wait_event_timeout <= 0) // resizing probably breaks this case
        return;
    set_getch_returns_resizes(true);
    int k = macro_key != -1 ? macro_key : getch_ck();
    set_getch_returns_resizes(false);

    if (k == CK_RESIZE)
    {
        // This may be superfluous, since the resize handler may have already
        // resized the screen
        clrscr();
        console_shutdown();
        console_startup();
        ui_root.resize(get_number_of_cols(), get_number_of_lines());
    }
    else
    {
        wm_event ev = {0};
        ev.type = WME_KEYDOWN;
        ev.key.keysym.sym = k;
        if (macro_key == -1)
            remap_key(ev);
        ui_root.on_event(ev);
    }
#endif
}

void run_layout(shared_ptr<Widget> root, const bool& done,
        shared_ptr<Widget> initial_focus)
{
    push_layout(root);
    set_focused_widget(initial_focus.get());
    while (!done && !crawl_state.seen_hups)
        pump_events();
    pop_layout();
}

bool has_layout()
{
    return ui_root.num_children() > 0;
}

NORETURN void restart_layout()
{
    throw UIRoot::RestartAllocation();
}

int getch(KeymapContext km)
{
    // getch() can be called when there are no widget layouts, i.e.
    // older layout/rendering code is being used. these parts of code don't
    // set a dirty region, so we should do that now. One example of this
    // is mprf() called from yesno()
    ui_root.needs_paint = true;

    int key;
    bool done = false;
    event_filter = [&](wm_event event) {
        // swallow all events except key presses here; we don't want any other
        // parts of the UI to react to anything while we're blocking for a key
        // press. An example: clicking shopping menu items when asked whether
        // to purchase already-selected items should not do anything.
        if (event.type != WME_KEYDOWN)
            return true;
        key = event.key.keysym.sym;
        done = true;
        return true;
    };
    unwind_var<KeymapContext> temp_keymap(ui_root.state.keymap, km);
    while (!done && !crawl_state.seen_hups)
        pump_events();
    event_filter = nullptr;
    return key;
}

void delay(unsigned int ms)
{
    if (crawl_state.disables[DIS_DELAY])
        ms = 0;

    auto start = std::chrono::high_resolution_clock::now();
#ifdef USE_TILE_LOCAL
    int wait_event_timeout = ms;
    do
    {
        ui_root.expose_region({0, 0, INT_MAX, INT_MAX});
        pump_events(wait_event_timeout);
        auto now = std::chrono::high_resolution_clock::now();
        wait_event_timeout =
            std::chrono::duration_cast<std::chrono::milliseconds>(now - start)
            .count();
    }
    while ((unsigned)wait_event_timeout < ms && !crawl_state.seen_hups);
#else
    constexpr int poll_interval = 10;
    while (!crawl_state.seen_hups)
    {
        auto now = std::chrono::high_resolution_clock::now();
        const int remaining = ms -
            std::chrono::duration_cast<std::chrono::milliseconds>(now - start)
            .count();
        if (remaining < 0)
            break;
        usleep(max(0, min(poll_interval, remaining)));
        if (kbhit())
            pump_events();
    }
#endif
}

/**
 * Is it possible to use UI calls, e.g. push_layout? The answer can be different
 * on different build targets; it is earlier on console than on local tiles.
 */
bool is_available()
{
#ifdef USE_TILE_LOCAL
    // basically whether TilesFramework::initialise() has been called. This
    // isn't precisely right, so (TODO) some more work needs to be
    // done to figure out exactly what is needed to use the UI api minimally
    // without crashing.
    return wm && tiles.fonts_initialized();
#else
    return crawl_state.io_inited;
#endif
}

/**
 * Show the terminal cursor at the given position on the next redraw.
 * The cursor is only shown if the cursor is enabled. 1-indexed.
 */
void show_cursor_at(coord_def pos)
{
    ui_root.cursor_pos = pos;
}

void show_cursor_at(int x, int y)
{
    show_cursor_at(coord_def(x, y));
}

/**
 * A basic progress bar popup. This is meant to be invoked in an RAII style;
 * the caller is responsible for regularly calling `advance_progress` in order
 * to actually trigger UI redraws.
 */
progress_popup::progress_popup(string title, int width)
    : position(0), bar_width(width), no_more(crawl_state.show_more_prompt, false)
{
    auto container = make_shared<Box>(Widget::VERT);
    container->set_cross_alignment(Widget::CENTER);
#ifndef USE_TILE_LOCAL
    // Center the popup in console.
    // if webtiles browser ever uses this property, then this will probably
    // look bad there and need another solution. But right now, webtiles ignores
    // expand_h.
    container->expand_h = true;
#endif
    formatted_string bar_string = get_progress_string(bar_width);
    progress_bar = make_shared<Text>(bar_string);
    auto title_text = make_shared<Text>(title);
    status_text = make_shared<Text>("");
    container->add_child(title_text);
    container->add_child(progress_bar);
    container->add_child(status_text);
    contents = make_shared<Popup>(container);

#ifdef USE_TILE_WEB
    tiles.json_open_object();
    tiles.json_write_string("title", title);
    tiles.json_write_string("bar_text", bar_string.to_colour_string());
    tiles.json_write_string("status", "");
    tiles.push_ui_layout("progress-bar", 1);
    tiles.flush_messages();
    contents->on_layout_pop([](){ tiles.pop_ui_layout(); });
#endif

    push_layout(move(contents));
    pump_events(0);
}

progress_popup::~progress_popup()
{
    pop_layout();

#ifdef USE_TILE_WEB
    tiles.flush_messages();
#endif
}

void progress_popup::force_redraw()
{
#ifdef USE_TILE_WEB
    tiles.json_open_object();
    tiles.json_write_string("status", status_text->get_text());
    tiles.json_write_string("bar_text",
        progress_bar->get_text().to_colour_string());
    tiles.ui_state_change("progress-bar", 0);
    // need to manually flush messages in case the caller doesn't pause for
    // input.
    tiles.flush_messages();
#endif
    pump_events(0);
}

void progress_popup::set_status_text(string status)
{
    status_text->set_text(status);
    force_redraw();
}

void progress_popup::advance_progress()
{
    position++;
    formatted_string new_bar = get_progress_string(bar_width);
    progress_bar->set_text(new_bar);
    force_redraw();
}

formatted_string progress_popup::get_progress_string(unsigned int len)
{
    string bar = string(len, ' ');
    if (len < 3)
        return formatted_string(bar);
    const unsigned int center_pos = len + position % len;
    const bool up = center_pos % 2 == 0;
    const string marker = up ? "/o/" : "\\o\\";
    bar[(center_pos - 1) % len] = marker[0];
    bar[center_pos % len] = marker[1];
    bar[(center_pos + 1) % len] = marker[2];
    bar = string("<lightmagenta>") + bar + "</lightmagenta>";
    return formatted_string::parse_string(bar);
}

void set_focused_widget(Widget* w)
{
    static bool sent_focusout;
    static Widget* new_focus;

    const auto top = top_layout();

    if (!top)
        return;

    if (w && !top->is_ancestor_of(w->get_shared()))
        return;

    if (!w)
        w = ui_root.state.default_focus;

    auto current_focus = ui_root.state.current_focus;

    if (w == current_focus)
        return;

#ifdef USE_TILE_WEB
    tiles.json_open_object();
    tiles.json_write_string("msg", "ui-state-sync");
    tiles.json_write_bool("has_focus", true);
    tiles.json_write_string("widget_id", w->sync_id());  // "" means default
    tiles.json_write_bool("from_webtiles", ui_root.receiving_ui_state);
    tiles.json_write_int("generation_id", ui_root.state.generation_id);
    tiles.json_close_object();
    tiles.finish_message();
#endif

    new_focus = w;

    if (current_focus && !sent_focusout)
    {
        sent_focusout = true;
        auto ev = FocusEvent(Event::Type::FocusOut);
        ev.set_target(current_focus->get_shared());
        ui_root.deliver_event(ev);
    }

    if (new_focus != w)
        return;

    ui_root.state.current_focus = new_focus;

    sent_focusout = false;

    if (new_focus)
    {
        auto ev = FocusEvent(Event::Type::FocusIn);
        ev.set_target(new_focus->get_shared());
        ui_root.deliver_event(ev);
    }
}

Widget* get_focused_widget()
{
    return ui_root.state.current_focus;
}

#ifdef USE_TILE_WEB
void recv_ui_state_change(const JsonNode *state)
{
    ui_root.recv_ui_state_change(state);
}

void sync_ui_state()
{
    ui_root.sync_state();
}

int layout_generation_id()
{
    return ui_root.next_generation_id;
}
#endif

bool raise_event(Event& event)
{
    return ui_root.deliver_event(event);
}

#ifdef USE_TILE_LOCAL
wm_mouse_event to_wm_event(const MouseEvent &ev)
{
    wm_mouse_event mev;
    mev.event = ev.type() == Event::Type::MouseMove ? wm_mouse_event::MOVE :
                ev.type() == Event::Type::MouseDown ? wm_mouse_event::PRESS :
                wm_mouse_event::WHEEL;
    mev.button = static_cast<wm_mouse_event::mouse_event_button>(ev.button());
    mev.mod = wm->get_mod_state();
    int x, y;
    mev.held = wm->get_mouse_state(&x, &y);
    mev.px = x;
    mev.py = y;
    return mev;
}
#endif

}
