/**
 * @file
 * @brief Hierarchical layout system.
**/

#include "AppHdr.h"

#include <numeric>
#include <stack>
#include <chrono>

#include "ui.h"
#include "cio.h"
#include "macro.h"
# include "state.h"
#include "tileweb.h"

#ifdef USE_TILE_LOCAL
# include "glwrapper.h"
# include "tilebuf.h"
#else
# include <unistd.h>
# include "output.h"
# include "view.h"
# include "stringutil.h"
#endif

namespace ui {

static i4 aabb_intersect(i4 a, i4 b)
{
    a[2] += a[0]; a[3] += a[1];
    b[2] += b[0]; b[3] += b[1];
    i4 i = { max(a[0], b[0]), max(a[1], b[1]), min(a[2], b[2]), min(a[3], b[3]) };
    i[2] -= i[0]; i[3] -= i[1];
    return i;
}

static i4 aabb_union(i4 a, i4 b)
{
    a[2] += a[0]; a[3] += a[1];
    b[2] += b[0]; b[3] += b[1];
    i4 i = { min(a[0], b[0]), min(a[1], b[1]), max(a[2], b[2]), max(a[3], b[3]) };
    i[2] -= i[0]; i[3] -= i[1];
    return i;
}


static inline bool pos_in_rect(i2 pos, i4 rect)
{
    if (pos[0] < rect[0] || pos[0] >= rect[0]+rect[2])
        return false;
    if (pos[1] < rect[1] || pos[1] >= rect[1]+rect[3])
        return false;
    return true;
}

#ifndef USE_TILE_LOCAL
static void clear_text_region(i4 region);
#endif

// must be before ui_root declaration for correct destruction order
vector<Widget*> prev_hover_path;

static struct UIRoot
{
public:
    void push_child(shared_ptr<Widget> child, KeymapContext km);
    void pop_child();
    shared_ptr<Widget> top_child() {
        size_t sz = m_root.num_children();
        return sz > 0 ? m_root.get_child(sz-1) : nullptr;
    };
    size_t num_children() const { return m_root.num_children(); };

    void resize(int w, int h);
    void layout();
    void render();

    bool on_event(const wm_event& event);
    void queue_layout() { m_needs_layout = true; };
    void expose_region(i4 r) {
        if (r[2] == 0 || r[3] == 0)
            return;
        if (m_dirty_region[2] == 0)
            m_dirty_region = r;
        else
            m_dirty_region = aabb_union(m_dirty_region, r);
        needs_paint = true;
    };
    void send_mouse_enter_leave_events(int mx, int my);

    bool needs_paint;
    vector<KeymapContext> keymap_stack;
    vector<int> cutoff_stack;

protected:
    int m_w, m_h;
    i4 m_region;
    i4 m_dirty_region{0, 0, 0, 0};
    Stack m_root;
    bool m_needs_layout{false};
} ui_root;

static stack<i4> scissor_stack;

struct Widget::slots Widget::slots = {};

bool Widget::on_event(const wm_event& event)
{
    if (event.type == WME_KEYDOWN || event.type == WME_KEYUP)
        return Widget::slots.event.emit(this, event);
    else
        return false;
}

static inline bool _maybe_propagate_event(wm_event event, shared_ptr<Widget> &child)
{
#ifdef USE_TILE_LOCAL
    /* WME_MOUSELEAVE is conspicuously absent because it is emitted
     * only on widgets that the cursor just left */
    if (event.type == WME_MOUSEMOTION
     || event.type == WME_MOUSEENTER
     || event.type == WME_MOUSEBUTTONDOWN
     || event.type == WME_MOUSEBUTTONUP)
    {
        i2 pos = {(int)event.mouse_event.px, (int)event.mouse_event.py};
        if (!pos_in_rect(pos, child->get_region()))
            return false;
    }
#endif
    return child->on_event(event);
}

shared_ptr<Widget> ContainerVec::get_child_at_offset(int x, int y)
{
    for (shared_ptr<Widget>& child : m_children)
    {
        const i4 region = child->get_region();
        bool inside = (x >= region[0] && x < region[0] + region[2])
            && (y >= region[1] && y < region[1] + region[3]);
        if (inside)
            return child;
    }
    return nullptr;
}

bool Container::on_event(const wm_event& event)
{
    if (Widget::on_event(event))
        return true;
    for (shared_ptr<Widget> &child : *this)
        if (_maybe_propagate_event(event, child))
            return true;
    return false;
}

bool Bin::on_event(const wm_event& event)
{
    if (Widget::on_event(event))
        return true;
    if (_maybe_propagate_event(event, m_child))
        return true;
    return false;
}

shared_ptr<Widget> Bin::get_child_at_offset(int x, int y)
{
    bool inside = (x > m_region[0] && x < m_region[0] + m_region[2])
        && (y > m_region[1] && y < m_region[1] + m_region[3]);
    return inside ? m_child : nullptr;
}

void Bin::set_child(shared_ptr<Widget> child)
{
    child->_set_parent(this);
    m_child = move(child);
    _invalidate_sizereq();
}

void Widget::render()
{
    _render();
}

SizeReq Widget::get_preferred_size(Direction dim, int prosp_width)
{
    ASSERT((dim == HORZ) == (prosp_width == -1));

    if (cached_sr_valid[dim] && (!dim || cached_sr_pw == prosp_width))
        return cached_sr[dim];

    prosp_width = dim ? prosp_width - margin[1] - margin[3] : prosp_width;
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

    ASSERT(m_min_size[dim] <= m_max_size[dim]);
    ret.min = max(ret.min, m_min_size[dim]);
    ret.nat = min(ret.nat, max(m_max_size[dim], ret.min));
    ret.nat = max(ret.nat, ret.min);
    ASSERT(ret.min <= ret.nat);

    int m = margin[1-dim] + margin[3-dim];
    ret.min += m;
    ret.nat += m;

    ret.nat = min(ret.nat, ui_expand_sz);

    cached_sr_valid[dim] = true;
    cached_sr[dim] = ret;
    if (dim)
        cached_sr_pw = prosp_width;

    return ret;
}

void Widget::allocate_region(i4 region)
{
    i4 new_region = {
        region[0] + margin[3],
        region[1] + margin[0],
        region[2] - margin[3] - margin[1],
        region[3] - margin[0] - margin[2],
    };

    if (m_region == new_region && !alloc_queued)
        return;
    ui_root.expose_region(m_region);
    ui_root.expose_region(new_region);
    m_region = new_region;
    alloc_queued = false;

    ASSERT(m_region[2] >= 0);
    ASSERT(m_region[3] >= 0);
    _allocate_region();
}

SizeReq Widget::_get_preferred_size(Direction dim, int prosp_width)
{
    SizeReq ret = { 0, 0 };
    return ret;
}

void Widget::_allocate_region()
{
}

void Widget::_set_parent(Widget* p)
{
    if (!p)
    {
        for (size_t i = 0; i < prev_hover_path.size(); i++)
            if (prev_hover_path[i] == this)
            {
                prev_hover_path.resize(i);
                break;
            }
    }
    m_parent = p;
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
        auto const& child = m_children[i];
        // find the child's size on the cross axis
        bool stretch = child->align_self == STRETCH ? true
            : align_items == STRETCH;
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
    vector<int> cw = horz ? layout_main_axis(sr, m_region[2]) : layout_cross_axis(sr, m_region[2]);

    // Get preferred heights
    for (size_t i = 0; i < m_children.size(); i++)
        sr[i] = m_children[i]->get_preferred_size(Widget::VERT, cw[i]);

    // Get actual heights
    vector<int> ch = horz ? layout_cross_axis(sr, m_region[3]) : layout_main_axis(sr, m_region[3]);

    auto const &m = horz ? cw : ch;
    int extra_main_space = m_region[horz ? 2 : 3] - accumulate(m.begin(), m.end(), 0);
    ASSERT(extra_main_space >= 0);

    // main axis offset
    int mo = extra_main_space*(justify_items - Box::Justify::START)/2;
    int ho = m_region[0] + (horz ? mo : 0);
    int vo = m_region[1] + (!horz ? mo : 0);

    i4 cr = {ho, vo, 0, 0};
    for (size_t i = 0; i < m_children.size(); i++)
    {
        // cross axis offset
        int extra_cross_space = horz ? m_region[3] - ch[i] : m_region[2] - cw[i];
        int xp = horz ? 1 : 0, xs = xp + 2;

        auto const& child = m_children[i];
        Align child_align = child->align_self ? child->align_self
                : align_items ? align_items
                : Align::START;
        int xo;
        switch (child_align)
        {
            case Widget::START:   xo = 0; break;
            case Widget::CENTER:  xo = extra_cross_space/2; break;
            case Widget::END:     xo = extra_cross_space; break;
            case Widget::STRETCH: xo = 0; break;
            default: ASSERT(0);
        }
        cr[xp] = (horz ? vo : ho) + xo;

        cr[2] = cw[i];
        cr[3] = ch[i];
        if (child_align == STRETCH)
            cr[xs] = (horz ? ch : cw)[i];
        m_children[i]->allocate_region(cr);
        cr[horz ? 0 : 1] += cr[horz ? 2 : 3];
    }
}

void Text::set_text(const formatted_string &fs)
{
    m_text.clear();
    m_text += fs;
    _invalidate_sizereq();
    _expose();
    m_wrapped_size = { -1, -1 };
    _queue_allocation();
}

void Text::set_highlight_pattern(string pattern, bool line)
{
    hl_pat = pattern;
    hl_line = line;
    _expose();
}

void Text::wrap_text_to_size(int width, int height)
{
    i2 wrapped_size = { width, height };
    if (m_wrapped_size == wrapped_size)
        return;
    m_wrapped_size = wrapped_size;

    height = height ? height : 0xfffffff;

#ifdef USE_TILE_LOCAL
    if (wrap_text || ellipsize)
        m_text_wrapped = tiles.get_crt_font()->split(m_text, width, height);
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
        last_line += formatted_string(" ");
        last_line += next_line;
        last_line = last_line.chop(width-2);
        last_line += formatted_string("..");
        m_wrapped_lines.resize(height);
    }
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
    i4 region = m_region;
    if (scissor_stack.size() > 0)
        region = aabb_intersect(region, scissor_stack.top());
    if (region[2] <= 0 || region[3] <= 0)
        return;

#ifdef USE_TILE_LOCAL
    const int line_height = tiles.get_crt_font()->char_height();
    const unsigned line_min = (region[1]-m_region[1]) / line_height;
    const unsigned line_max = (region[1]+region[3]-m_region[1]) / line_height;

    int line_off = 0;
    int ops_min = 0, ops_max = m_text_wrapped.ops.size();
    {
        int i = 1;
        for (; i < (int)m_brkpts.size(); i++)
            if (m_brkpts[i].line >= line_min)
            {
                ops_min = m_brkpts[i-1].op;
                line_off = m_brkpts[i-1].line;
                break;
            }
        for (; i < (int)m_brkpts.size(); i++)
            if (m_brkpts[i].line > line_max)
            {
                ops_max = m_brkpts[i].op;
                break;
            }
    }

    formatted_string slice;
    slice.ops = vector<formatted_string::fs_op>(
        m_text_wrapped.ops.begin()+ops_min,
        m_text_wrapped.ops.begin()+ops_max);

    if (!hl_pat.empty())
    {
        const auto& full_text = m_text.tostring();

        // need to find the byte ranges in full_text that our slice corresponds to
        // note that the indexes are the same in both m_text and m_text_wrapped
        // only because wordwrapping only replaces ' ' with '\n': in other words,
        // this is fairly brittle
        ASSERT(full_text.size() == m_text_wrapped.tostring().size());
        int begin_idx = ops_min == 0 ? 0 : m_text_wrapped.tostring(0, ops_min-1).size();
        int end_idx = begin_idx + m_text_wrapped.tostring(ops_min, ops_max-1).size();

        vector<size_t> highlights = _find_highlights(full_text, hl_pat, begin_idx, end_idx);

        const int ox = m_region[0], oy = m_region[1]+line_height*line_off;
        size_t lacc = 0, line = 0;
        FontWrapper *font = tiles.get_crt_font();
        bool inside = false;
        for (unsigned i = 0; i < slice.ops.size() && !highlights.empty(); i++)
        {
            const auto& op = slice.ops[i];
            if (op.type != FSOP_TEXT)
                continue;
            size_t oplen = op.text.size();
            size_t start = highlights[0] - begin_idx - lacc,
                   end = highlights[0] - begin_idx - lacc + hl_pat.size();

            size_t sx = 0, ex = font->string_width(op.text.c_str());
            size_t sy = 0, ey = line_height;
            bool started = false, ended = false;

            if (start < oplen) // assume start is unsigned and so >=0
            {
                {
                    size_t line_start = full_text.rfind('\n', highlights[0]);
                    line_start = line_start == string::npos ? 0 : line_start+1;
                    const string before = full_text.substr(line_start, highlights[0]-line_start);
                    sx = font->string_width(before.c_str());
                }
                {
                    const string before = full_text.substr(lacc+begin_idx, highlights[0]-lacc-begin_idx);
                    sy = font->string_height(before.c_str()) - line_height;
                }
                started = true;
            }
            if (end < oplen) // assume end is unsigned and so >=0
            {
                {
                    size_t line_start = full_text.rfind('\n', highlights[0]+hl_pat.size()-1);
                    line_start = line_start == string::npos ? 0 : line_start+1;
                    const string to_end = full_text.substr(line_start, highlights[0]+hl_pat.size()-line_start);
                    ex = font->string_width(to_end.c_str());
                }
                {
                    const string to_end = full_text.substr(lacc+begin_idx, highlights[0]+hl_pat.size()-1-lacc-begin_idx);
                    ey = font->string_height(to_end.c_str());
                }
                ended = true;
            }

            if (started || ended || inside)
            {
                m_hl_buf.clear();
                for (size_t y = oy+line+sy; y < oy+line+ey; y+=line_height)
                {
                    if (hl_line)
                    {
                        m_hl_buf.add(region[0], y, region[0]+region[2], y + line_height,
                            VColour(255, 255, 0, 50));
                    }
                    else
                        m_hl_buf.add(ox + sx, y, ox + ex, y + line_height,
                            VColour(255, 255, 0, 50));
                }
                m_hl_buf.draw();
            }
            inside = !ended && (inside || started);

            if (ended)
            {
                highlights.erase(highlights.begin()+0);
                i--;
            }
            else
            {
                lacc += oplen;
                line += font->string_height(op.text.c_str()) - line_height;
            }
        }
    }

    // XXX: should be moved into a new function render_formatted_string()
    // in FTFontWrapper, that, like render_textblock(), would automatically
    // handle swapping atlas glyphs as necessary.
    FontBuffer m_font_buf(tiles.get_crt_font());
    m_font_buf.add(slice, m_region[0], m_region[1]+line_height*line_off);
    m_font_buf.draw();
#else
    const auto& lines = m_wrapped_lines;
    vector<size_t> highlights;
    int begin_idx = 0;

    if (!hl_pat.empty())
    {
        for (int i = 0; i < region[1]-m_region[1]; i++)
            begin_idx += m_wrapped_lines[i].tostring().size()+1;
        int end_idx = begin_idx;
        for (int i = region[1]-m_region[1]; i < region[1]-m_region[1]+region[3]; i++)
            end_idx += m_wrapped_lines[i].tostring().size()+1;
        highlights = _find_highlights(m_text.tostring(), hl_pat, begin_idx, end_idx);
    }

    unsigned int hl_idx = 0;
    for (size_t i = 0; i < min(lines.size(), (size_t)region[3]); i++)
    {
        cgotoxy(region[0]+1, region[1]+1+i);
        formatted_string line = lines[i+region[1]-m_region[1]];
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
        out.chop(region[2]).display(0);

        begin_idx = end_idx + 1; // +1 is for the newline
    }
#endif
}

SizeReq Text::_get_preferred_size(Direction dim, int prosp_width)
{
#ifdef USE_TILE_LOCAL
    FontWrapper *font = tiles.get_crt_font();
    if (!dim)
    {
        int w = font->string_width(m_text);
        // XXX: should be width of '..', unless string itself is shorter than '..'
        static constexpr int min_ellipsized_width = 0;
        static constexpr int min_wrapped_width = 0; // XXX: should be width of longest word
        return { ellipsize ? min_ellipsized_width : wrap_text ? min_wrapped_width : w, w };
    }
    else
    {
        wrap_text_to_size(prosp_width, 0);
        int height = font->string_height(m_text_wrapped);
        return { ellipsize ? (int)font->char_height() : height, height };
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
    wrap_text_to_size(m_region[2], m_region[3]);
}

void Image::set_tile(tile_def tile)
{
#ifdef USE_TILE_LOCAL
    m_tile = tile;
    const tile_info &ti = tiles.get_image_manager()->tile_def_info(m_tile);
    m_tw = ti.width;
    m_th = ti.height;
    _invalidate_sizereq();
#endif
}

void Image::_render()
{
#ifdef USE_TILE_LOCAL
    push_scissor(m_region);
    TileBuffer tb;
    tb.set_tex(&tiles.get_image_manager()->m_textures[m_tile.tex]);

    for (int y = m_region[1]; y < m_region[1]+m_region[3]; y+=m_th)
        for (int x = m_region[0]; x < m_region[0]+m_region[2]; x+=m_tw)
            tb.add(m_tile.tile, x, y, 0, 0, false, m_th, 1.0, 1.0);

    tb.draw();
    tb.clear();
    pop_scissor();
#endif
}

SizeReq Image::_get_preferred_size(Direction dim, int prosp_width)
{
#ifdef USE_TILE_LOCAL
    SizeReq ret = {
        // This is a little ad-hoc, but expand taking precedence over shrink when
        // determining the natural size makes the textured dialog box work
        dim ? (shrink_v ? 0 : m_th) : (shrink_h ? 0 : m_tw),
        dim ? (shrink_v ? 0 : m_th) : (shrink_h ? 0 : m_tw)
    };
#else
    SizeReq ret = { 0, 0 };
#endif
    return ret;
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
    const i4 region = m_children.back()->get_region();
    bool inside = (x > region[0] && x < region[0] + region[2])
        && (y > region[1] && y < region[1] + region[3]);
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
        i4 cr = m_region;
        SizeReq pw = child->get_preferred_size(Widget::HORZ, -1);
        cr[2] = min(max(pw.min, m_region[2]), pw.nat);
        SizeReq ph = child->get_preferred_size(Widget::VERT, cr[2]);
        cr[3] = min(max(ph.min, m_region[3]), ph.nat);
        child->allocate_region(cr);
    }
}

bool Stack::on_event(const wm_event& event)
{
    if (Widget::on_event(event))
        return true;
    if (m_children.size() > 0 &&_maybe_propagate_event(event, m_children.back()))
        return true;
    return false;
}

void Switcher::add_child(shared_ptr<Widget> child)
{
    child->_set_parent(this);
    m_children.push_back(move(child));
    _invalidate_sizereq();
    _queue_allocation();
}

int& Switcher::current()
{
    _expose();
    return m_current;
}

void Switcher::_render()
{
    if (m_children.size() == 0)
        return;
    m_current = max(0, min(m_current, (int)m_children.size()));
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
        i4 cr = m_region;
        SizeReq pw = child->get_preferred_size(Widget::HORZ, -1);
        cr[2] = min(max(pw.min, m_region[2]), pw.nat);
        SizeReq ph = child->get_preferred_size(Widget::VERT, cr[2]);
        cr[3] = min(max(ph.min, m_region[3]), ph.nat);
        child->allocate_region(cr);
    }
}

bool Switcher::on_event(const wm_event& event)
{
    if (Widget::on_event(event))
        return true;
    m_current = max(0, min(m_current, (int)m_children.size()));
    if (m_children.size() > 0 &&_maybe_propagate_event(event, m_children[m_current]))
        return true;
    return false;
}

shared_ptr<Widget> Grid::get_child_at_offset(int x, int y)
{
    int lx = x - m_region[0];
    int ly = y - m_region[1];
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
        if (child.pos[0] <= col && col < child.pos[0] + child.span[0])
        if (child.pos[1] <= row && row < child.pos[1] + child.span[1])
        if (pos_in_rect({x, y}, child.widget->get_region()))
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
        n_rows = max(n_rows, info.pos[1]+info.span[1]);
        n_cols = max(n_cols, info.pos[0]+info.span[0]);
    }
    m_row_info.resize(n_rows);
    m_col_info.resize(n_cols);

    sort(m_child_info.begin(), m_child_info.end(),
            [](const child_info& a, const child_info& b) {
        return a.pos[1] < b.pos[1];
    });
}

void Grid::_render()
{
    // Find the visible rows
    i4 scissor = get_scissor();
    int row_min = 0, row_max = m_row_info.size()-1, i = 0;
    for (; i < (int)m_row_info.size(); i++)
        if (m_row_info[i].offset+m_row_info[i].size+m_region[1] >= scissor[1])
        {
            row_min = i;
            break;
        }
    for (; i < (int)m_row_info.size(); i++)
        if (m_row_info[i].offset+m_region[1] >= scissor[1]+scissor[3])
        {
            row_max = i-1;
            break;
        }

    for (auto const& child : m_child_info)
    {
        if (child.pos[1] < row_min) continue;
        if (child.pos[1] > row_max) break;
        child.widget->render();
    }
}

void Grid::compute_track_sizereqs(Direction dim)
{
    auto& track = dim ? m_row_info : m_col_info;
#define DIV_ROUND_UP(n,d) (((n)+(d)-1)/(d))

    for (auto& t : track)
        t.sr = {0, 0};
    for (size_t i = 0; i < m_child_info.size(); i++)
    {
        auto& cp = m_child_info[i].pos, cs = m_child_info[i].span;
        // if merging horizontally, need to find (possibly multi-col) width
        int prosp_width = dim ? get_tracks_region(cp[0], cp[1], cs[0], cs[1])[2] : -1;

        const SizeReq c = m_child_info[i].widget->get_preferred_size(dim, prosp_width);
        // NOTE: items spanning multiple rows/cols don't contribute!
        if (cs[0] == 1 && cs[1] == 1)
        {
            auto& s = track[cp[dim]].sr;
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

    while (true)
    {
        int sum_flex_grow = 0, sum_taken = 0;
        for (const auto& info : infos)
            sum_flex_grow += info.size < info.sr.nat ? info.flex_grow : 0;
        if (!sum_flex_grow)
            break;

        for (size_t i = 0; i < infos.size(); ++i)
        {
            float efg = infos[i].size < infos[i].sr.nat ? infos[i].flex_grow : 0;
            int tr_extra = extra * efg / sum_flex_grow;
            ASSERT(infos[i].size <= infos[i].sr.nat);
            int taken = min(tr_extra, infos[i].sr.nat - infos[i].size);
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
    SizeReq h_sr = _get_preferred_size(Widget::VERT, m_region[2]);

    layout_track(Widget::VERT, h_sr, m_region[3]);
    set_track_offsets(m_row_info);

    for (size_t i = 0; i < m_child_info.size(); i++)
    {
        auto& cp = m_child_info[i].pos, cs = m_child_info[i].span;
        i4 cell_reg = get_tracks_region(cp[0], cp[1], cs[0], cs[1]);
        cell_reg[0] += m_region[0];
        cell_reg[1] += m_region[1];
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
        push_scissor(m_region);
        m_child->render();
#ifdef USE_TILE_LOCAL
        m_shade_buf.draw();
#endif
        pop_scissor();
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
    SizeReq sr = m_child->get_preferred_size(Widget::VERT, m_region[2]);
    m_scroll = max(0, min(m_scroll, sr.nat-m_region[3]));
    i4 ch_reg = {m_region[0], m_region[1]-m_scroll, m_region[2], sr.nat};
    m_child->allocate_region(ch_reg);

#ifdef USE_TILE_LOCAL
    int shade_height = 12, ds = 4;
    int shade_top = min({m_scroll/ds, shade_height, m_region[3]/2});
    int shade_bot = min({(sr.nat-m_region[3]-m_scroll)/ds, shade_height, m_region[3]/2});
    VColour col_a(4,2,4,0), col_b(4,2,4,200);

    m_shade_buf.clear();
    m_scrollbar_buf.clear();
    {
        GLWPrim rect(m_region[0], m_region[1]+shade_top-shade_height,
                m_region[0]+m_region[2], m_region[1]+shade_top);
        rect.set_col(col_b, col_a);
        m_shade_buf.add_primitive(rect);
    }
    {
        GLWPrim rect(m_region[0], m_region[1]+m_region[3]-shade_bot,
                m_region[0]+m_region[2], m_region[1]+m_region[3]-shade_bot+shade_height);
        rect.set_col(col_a, col_b);
        m_shade_buf.add_primitive(rect);
    }
    if (ch_reg[3] > m_region[3]) {
        const int x = m_region[0]+m_region[2];
        const float h_percent = m_region[3] / (float)ch_reg[3];
        const int h = m_region[3]*min(max(0.05f, h_percent), 1.0f);
        const float scroll_percent = m_scroll/(float)(ch_reg[3]-m_region[3]);
        const int y = m_region[1] + (m_region[3]-h)*scroll_percent;
        GLWPrim rect(x+10, y, x+12, y+h);
        rect.set_col(VColour(158,124,75,200));
        m_scrollbar_buf.add_primitive(rect);
    }
#endif
}

bool Scroller::on_event(const wm_event& event)
{
    if (Bin::on_event(event))
        return true;
#ifdef USE_TILE_LOCAL
    const int line_delta = 20;
#else
    const int line_delta = 1;
#endif
    int delta = 0;
    if (event.type == WME_KEYDOWN)
    {
        switch (event.key.keysym.sym)
        {
            case ' ': case '+': case CK_PGDN: case '>': case '\'':
                delta = m_region[3];
                break;

            case '-': case CK_PGUP: case '<': case ';':
                delta = -m_region[3];
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
    else if (event.type == WME_MOUSEWHEEL)
        delta = -1 * event.mouse_event.py * line_delta;
    else if (event.type == WME_MOUSEBUTTONDOWN && event.mouse_event.button == MouseEvent::LEFT)
        delta = line_delta;
    if (delta != 0)
    {
        set_scroll(m_scroll+delta);
        return true;
    }
    return false;
}

Popup::Popup(shared_ptr<Widget> child)
{
#ifdef USE_TILE_LOCAL
    m_depth = ui_root.num_children();
#endif
    child->_set_parent(this);
    m_child = move(child);
    expand_h = expand_v = true;
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
    SizeReq sr = m_child->get_preferred_size(dim, prosp_width);
#ifdef USE_TILE_LOCAL
    constexpr int pad = m_base_margin + m_padding;
    return {
        0, sr.nat + 2*pad + (dim ? m_depth*m_depth_indent*(!m_centred) : 0)
    };
#else
    return { sr.min, sr.nat };
#endif
}

void Popup::_allocate_region()
{
    i4 region = m_region;
#ifdef USE_TILE_LOCAL
    m_buf.clear();
    m_buf.add(m_region[0], m_region[1],
            m_region[0] + m_region[2], m_region[1] + m_region[3],
            VColour(0, 0, 0, 150));
    constexpr int pad = m_base_margin + m_padding;
    region[2] -= 2*pad;
    region[3] -= 2*pad + m_depth*m_depth_indent*(!m_centred);

    SizeReq hsr = m_child->get_preferred_size(HORZ, -1);
    region[2] = max(hsr.min, min(region[2], hsr.nat));
    SizeReq vsr = m_child->get_preferred_size(VERT, region[2]);
    region[3] = max(vsr.min, min(region[3], vsr.nat));

    region[0] += pad + (m_region[2]-2*pad-region[2])/2;
    region[1] += pad + (m_centred ? (m_region[3]-2*pad-region[3])/2
            : m_depth*m_depth_indent);

    m_buf.add(region[0] - m_padding, region[1] - m_padding,
            region[0] + region[2] + m_padding,
            region[1] + region[3] + m_padding,
            VColour(125, 98, 60));
    m_buf.add(region[0] - m_padding + 2, region[1] - m_padding + 2,
            region[0] + region[2] + m_padding - 2,
            region[1] + region[3] + m_padding - 2,
            VColour(0, 0, 0));
    m_buf.add(region[0] - m_padding + 3, region[1] - m_padding + 3,
            region[0] + region[2] + m_padding - 3,
            region[1] + region[3] + m_padding - 3,
            VColour(4, 2, 4));
#endif
    m_child->allocate_region(region);
}

i2 Popup::get_max_child_size()
{
#ifdef USE_TILE_LOCAL
    constexpr int pad = m_base_margin + m_padding;
    return {
        (m_region[2] - 2*pad) & ~0x1,
        (m_region[3] - 2*pad - m_depth*m_depth_indent) & ~0x1,
    };
#else
    return { m_region[2], m_region[3] };
#endif
}

#ifdef USE_TILE_LOCAL
void Dungeon::_render()
{
#ifdef USE_TILE_LOCAL
    GLW_3VF t = {(float)m_region[0], (float)m_region[1], 0}, s = {32, 32, 1};
    glmanager->set_transform(t, s);
    m_buf.draw();
    glmanager->reset_transform();
#endif
}

SizeReq Dungeon::_get_preferred_size(Direction dim, int prosp_width)
{
    int sz = (dim ? height : width)*32;
    return {sz, sz};
}
#endif

void UIRoot::push_child(shared_ptr<Widget> ch, KeymapContext km)
{
    m_root.add_child(move(ch));
    m_needs_layout = true;
    keymap_stack.push_back(km);
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
    m_root.pop_child();
    m_needs_layout = true;
    keymap_stack.pop_back();
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
        int x, y;
        wm->get_mouse_state(&x, &y);
        ui_root.send_mouse_enter_leave_events(x, y);
#endif
    }
}

void UIRoot::render()
{
    if (!needs_paint)
        return;

#ifdef USE_TILE_LOCAL
    glmanager->reset_view_for_redraw(0, 0);
    tiles.render_current_regions();
    glmanager->reset_transform();
#else
    // On console, clear and redraw only the dirty region of the screen
    m_dirty_region = aabb_intersect(m_dirty_region, m_region);
    textcolour(LIGHTGREY);
    textbackground(BLACK);
    clear_text_region(m_dirty_region);
#endif

    push_scissor(m_region);
#ifdef USE_TILE_LOCAL
    int cutoff = cutoff_stack.empty() ? 0 : cutoff_stack.back();
    ASSERT(cutoff <= static_cast<int>(m_root.num_children()));
    for (int i = cutoff; i < static_cast<int>(m_root.num_children()); i++)
        m_root.get_child(i)->render();
#else
    // Render only the top of the UI stack on console
    if (m_root.num_children() > 0)
        m_root.get_child(m_root.num_children()-1)->render();
    else
        redraw_screen(false);
#endif
    pop_scissor();

#ifdef USE_TILE_LOCAL
    wm->swap_buffers();
#else
    update_screen();
#endif

    needs_paint = false;
    m_dirty_region = {0, 0, 0, 0};
}

static function<bool(const wm_event&)> event_filter;

void UIRoot::send_mouse_enter_leave_events(int mx, int my)
{
    /* Find current hover path */
    vector<Widget*> hover_path;
    shared_ptr<Widget> current = m_root.get_child_at_offset(mx, my);
    while (current)
    {
        hover_path.emplace_back(current.get());
        current = current->get_child_at_offset(mx, my);
    }

    size_t sz = max(prev_hover_path.size(), hover_path.size());
    prev_hover_path.resize(sz, nullptr);
    hover_path.resize(sz, nullptr);

    size_t diff;
    for (diff = 0; diff < sz; diff++)
        if (hover_path[diff] != prev_hover_path[diff])
            break;

    /* send events */
    if (diff < sz)
    {
        wm_event ev = {0};
        ev.mouse_event.px = mx;
        ev.mouse_event.py = my;
        if (prev_hover_path[diff])
        {
            ev.type = WME_MOUSELEAVE;
            if (!event_filter || !event_filter(ev))
                prev_hover_path[diff]->on_event(ev);
        }
        if (hover_path[diff])
        {
            ev.type = WME_MOUSEENTER;
            if (!event_filter || !event_filter(ev))
                hover_path[diff]->on_event(ev);
        }
    }

    prev_hover_path = move(hover_path);
}

bool UIRoot::on_event(const wm_event& event)
{
    if (event_filter && event_filter(event))
        return true;
    return m_root.on_event(event);
}

void push_scissor(i4 scissor)
{
    if (scissor_stack.size() > 0)
        scissor = aabb_intersect(scissor, scissor_stack.top());
    scissor_stack.push(scissor);
#ifdef USE_TILE_LOCAL
    glmanager->set_scissor(scissor[0], scissor[1], scissor[2], scissor[3]);
#endif
}

void pop_scissor()
{
    ASSERT(scissor_stack.size() > 0);
    scissor_stack.pop();
#ifdef USE_TILE_LOCAL
    if (scissor_stack.size() > 0)
    {
        i4 scissor = scissor_stack.top();
        glmanager->set_scissor(scissor[0], scissor[1], scissor[2], scissor[3]);
    }
    else
        glmanager->reset_scissor();
#endif
}

i4 get_scissor()
{
    return scissor_stack.top();
}

#ifndef USE_TILE_LOCAL
static void clear_text_region(i4 region)
{
    if (scissor_stack.size() > 0)
        region = aabb_intersect(region, scissor_stack.top());
    if (region[2] <= 0 || region[3] <= 0)
        return;
    textcolour(LIGHTGREY);
    textbackground(BLACK);
    for (int y=region[1]; y < region[1]+region[3]; y++)
    {
        cgotoxy(region[0]+1, y+1);
        cprintf("%*s", region[2], "");
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
}

void pop_layout()
{
    ui_root.pop_child();
#ifdef USE_TILE_LOCAL
    int x, y;
    wm->get_mouse_state(&x, &y);
    ui_root.send_mouse_enter_leave_events(x, y);
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
    KeymapContext km = ui_root.keymap_stack.size() > 0 ? ui_root.keymap_stack[0] : KMC_NONE;
    macro_buf_add_with_keymap(keys, km);
    event.key.keysym.sym = macro_buf_get();
    ASSERT(event.key.keysym.sym != -1);
}

void ui_force_render()
{
    ui_root.layout();
    ui_root.render();
}

void pump_events(int wait_event_timeout)
{
    int macro_key = macro_buf_get();

#ifdef USE_TILE_LOCAL
    // Don't render while there are unhandled mousewheel events,
    // since these can come in faster than crawl can redraw.
    // unlike mousemotion events, we don't drop all but the last event
    // ...but if there are macro keys, we do need to layout (for menu UI)
    if (!wm->get_event_count(WME_MOUSEWHEEL) || macro_key != -1)
#endif
    {
        ui_root.layout();
#ifndef USE_TILE_WEB
        // On webtiles, we can't skip rendering while there are macro keys: a
        // crt screen may be opened and without a render() call, its text won't
        // won't be sent to the client(s). E.g: macro => iai
        if (macro_key == -1)
#endif
            ui_root.render();
    }

#ifdef USE_TILE_LOCAL
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
            if (wait_event_timeout == INT_MAX)
                continue;
            else
                return;
        if (event.type == WME_MOUSEMOTION)
        {
            // For consecutive mouse events, ignore all but the last,
            // since these can come in faster than crawl can redraw.
            //
            // Note that get_event_count() is misleadingly named and only
            // peeks at the first event, and so will only return 0 or 1.
            if (wm->get_event_count(WME_MOUSEMOTION) > 0)
                continue;
        }
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
            ui_root.resize(event.resize.w, event.resize.h);
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
            ui_root.send_mouse_enter_leave_events(event.mouse_event.px,
                    event.mouse_event.py);
            ui_root.on_event(event);

        default:
            if (!ui_root.on_event(event) && event.type == WME_MOUSEBUTTONDOWN)
            {
                // If a mouse event wasn't handled, send it through again as a
                // fake key event, for compatibility
                int key;
                if (event.mouse_event.button == MouseEvent::LEFT)
                    key = CK_MOUSE_CLICK;
                else if (event.mouse_event.button == MouseEvent::RIGHT)
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

void run_layout(shared_ptr<Widget> root, const bool& done)
{
    push_layout(root);
    while (!done && !crawl_state.seen_hups)
        pump_events();
    pop_layout();
}

bool has_layout()
{
    return ui_root.num_children() > 0;
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
    ui_root.keymap_stack.emplace_back(km);
    while (!done && !crawl_state.seen_hups)
        pump_events();
    ui_root.keymap_stack.pop_back();
    event_filter = nullptr;
    return key;
}

void ui_delay(unsigned int ms)
{
    if (crawl_state.disables[DIS_DELAY])
        ms = 0;

    auto start = std::chrono::high_resolution_clock::now();
#ifdef USE_TILE_LOCAL
    int wait_event_timeout = ms;
    do
    {
        ui_root.expose_region({0,0,INT_MAX,INT_MAX});
        pump_events(wait_event_timeout);
        auto now = std::chrono::high_resolution_clock::now();
        wait_event_timeout =
            std::chrono::duration_cast<std::chrono::milliseconds>(now - start)
            .count();
    }
    while ((unsigned)wait_event_timeout < ms && !crawl_state.seen_hups);
#else
    constexpr long poll_interval = 10;
    while (!crawl_state.seen_hups)
    {
        auto now = std::chrono::high_resolution_clock::now();
        long remaining = ms -
            std::chrono::duration_cast<std::chrono::milliseconds>(now - start)
            .count();
        if (remaining < 0)
            break;
        usleep(max(0l, min(poll_interval, remaining)));
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

}
