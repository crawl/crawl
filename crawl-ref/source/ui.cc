/**
 * @file
 * @brief Hierarchical layout system.
**/

#include "AppHdr.h"

#include <numeric>
#include <stack>

#include "ui.h"

#ifdef USE_TILE_LOCAL
# include "windowmanager.h"
# include "glwrapper.h"
# include "tilebuf.h"
#else
# include "state.h"
# include "view.h"
# include "cio.h"
# include "stringutil.h"
#endif

#define UI_DEBUG_DRAW 0

static i4 aabb_intersect(i4 a, i4 b)
{
    a[2] += a[0]; a[3] += a[1];
    b[2] += b[0]; b[3] += b[1];
    i4 i = { max(a[0], b[0]), max(a[1], b[1]), min(a[2], b[2]), min(a[3], b[3]) };
    i[2] -= i[0]; i[3] -= i[1];
    return i;
}

static void ui_push_scissor(i4 scissor);
static void ui_pop_scissor();

static struct UIRoot
{
public:
    UIRoot() : m_child(nullptr), m_dirty(false) {};
    void set_child(shared_ptr<UI> child);

    void resize(int w, int h);
    void layout();
    void render();

protected:
    int m_w, m_h;
    i4 m_region;
    shared_ptr<UI> m_child;
    bool m_dirty;
} ui_root;

static stack<i4> scissor_stack;

void UI::render()
{
#ifdef USE_TILE_LOCAL
#if UI_DEBUG_DRAW
    LineBuffer buf;
    const VColour vc(160, 80, 80, 255);
    buf.add_square(m_region[0], m_region[1],
            m_region[0] + m_region[2]-1, m_region[1] + m_region[3]-1, vc);
    buf.draw();
#endif
#endif
    _render();
}

UISizeReq UI::get_preferred_size(int dim, int prosp_width)
{
    ASSERT(dim == 0 || dim == 1);
    ASSERT((dim == 0) == (prosp_width == -1));

    // XXX: This needs invalidation on widget/descendant property change!
    if (cached_sr_valid[dim] && (!dim || cached_sr_pw == prosp_width))
        return cached_sr[dim];

    prosp_width = dim ? prosp_width - margin[1] - margin[3] : prosp_width;
    UISizeReq ret = _get_preferred_size(dim, prosp_width);
    ASSERT(ret.min <= ret.nat);
    int m = margin[1-dim] + margin[3-dim];
    ret.min += m;
    ret.nat += m;

    const int ui_expand_sz = 0xffffff;
    if (dim ? expand_v : expand_h)
        ret.nat = ui_expand_sz;
    ret.nat = min(ret.nat, ui_expand_sz);

    cached_sr_valid[dim] = true;
    cached_sr[dim] = ret;
    if (dim)
        cached_sr_pw = prosp_width;

    return ret;
}

void UI::allocate_region(i4 region)
{
    m_region = {
        region[0] + margin[3],
        region[1] + margin[0],
        region[2] - margin[3] - margin[1],
        region[3] - margin[0] - margin[2],
    };

    ASSERT(m_region[2] >= 0);
    ASSERT(m_region[3] >= 0);
    _allocate_region();
}

UISizeReq UI::_get_preferred_size(int dim, int prosp_width)
{
    UISizeReq ret = { 0, 0 };
    return ret;
}

void UI::_allocate_region()
{
}

void UIBox::add_child(shared_ptr<UI> child)
{
    m_children.push_back(move(child));
}

void UIBox::_render()
{
    for (auto const& child : m_children)
        child->render();
}

vector<int> UIBox::layout_main_axis(vector<UISizeReq>& ch_psz, int main_sz)
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
        {
            sum_flex_grow += ch_sz[i] < ch_psz[i].nat ? m_children[i]->flex_grow : 0;
        }
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

vector<int> UIBox::layout_cross_axis(vector<UISizeReq>& ch_psz, int cross_sz)
{
    vector<int> ch_sz(m_children.size());

    for (size_t i = 0; i < m_children.size(); i++)
    {
        auto const& child = m_children[i];
        // find the child's size on the cross axis
        bool stretch = child->align_self == UI_ALIGN_STRETCH ? true
            : align_items == UI_ALIGN_STRETCH;
        ch_sz[i] = stretch ? cross_sz : min(max(ch_psz[i].min, cross_sz), ch_psz[i].nat);
    }

    return ch_sz;
}

UISizeReq UIBox::_get_preferred_size(int dim, int prosp_width)
{
    vector<UISizeReq> sr(m_children.size());

    // Get preferred widths
    for (size_t i = 0; i < m_children.size(); i++)
        sr[i] = m_children[i]->get_preferred_size(0, -1);

    if (dim)
    {
        // Get actual widths
        vector<int> cw = horz ? layout_main_axis(sr, prosp_width) : layout_cross_axis(sr, prosp_width);

        // Get preferred heights
        for (size_t i = 0; i < m_children.size(); i++)
            sr[i] = m_children[i]->get_preferred_size(1, cw[i]);
    }

    // find sum/max of preferred sizes, as appropriate
    bool main_axis = dim == !horz;
    UISizeReq r = { 0, 0 };
    for (auto const& c : sr)
    {
        r.min = main_axis ? r.min + c.min : max(r.min, c.min);
        r.nat = main_axis ? r.nat + c.nat : max(r.nat, c.nat);
    }
    return r;
}

void UIBox::_allocate_region()
{
    vector<UISizeReq> sr(m_children.size());

    // Get preferred widths
    for (size_t i = 0; i < m_children.size(); i++)
        sr[i] = m_children[i]->get_preferred_size(0, -1);

    // Get actual widths
    vector<int> cw = horz ? layout_main_axis(sr, m_region[2]) : layout_cross_axis(sr, m_region[2]);

    // Get preferred heights
    for (size_t i = 0; i < m_children.size(); i++)
        sr[i] = m_children[i]->get_preferred_size(1, cw[i]);

    // Get actual heights
    vector<int> ch = horz ? layout_cross_axis(sr, m_region[3]) : layout_main_axis(sr, m_region[3]);

    auto const &m = horz ? cw : ch;
    int extra_main_space = m_region[horz ? 2 : 3] - accumulate(m.begin(), m.end(), 0);
    ASSERT(extra_main_space >= 0);

    // main axis offset
    int mo = extra_main_space*(justify_items - UI_JUSTIFY_START)/2;
    int ho = m_region[0] + (horz ? mo : 0);
    int vo = m_region[1] + (!horz ? mo : 0);

    i4 cr = {ho, vo, 0, 0};
    for (size_t i = 0; i < m_children.size(); i++)
    {
        // cross axis offset
        int extra_cross_space = horz ? m_region[3] - ch[i] : m_region[2] - cw[i];
        int xp = horz ? 1 : 0, xs = xp + 2;

        auto const& child = m_children[i];
        UIAlign_type child_align = child->align_self ? child->align_self
                : align_items ? align_items
                : UI_ALIGN_START;
        int xo;
        switch (child_align)
        {
            case UI_ALIGN_START:   xo = 0; break;
            case UI_ALIGN_CENTER:  xo = extra_cross_space/2; break;
            case UI_ALIGN_END:     xo = extra_cross_space; break;
            case UI_ALIGN_STRETCH: xo = 0; break;
            default: ASSERT(0);
        }
        cr[xp] = (horz ? vo : ho) + xo;

        cr[2] = cw[i];
        cr[3] = ch[i];
        if (child_align == UI_ALIGN_STRETCH)
            cr[xs] = (horz ? ch : cw)[i];
        m_children[i]->allocate_region(cr);
        cr[horz ? 0 : 1] += cr[horz ? 2 : 3];
    }
}

void UIImage::set_tile(tile_def tile)
{
#ifdef USE_TILE_LOCAL
    m_tile = tile;
    const tile_info &ti = tiles.get_image_manager()->tile_def_info(m_tile);
    m_tw = ti.width;
    m_th = ti.height;
    m_using_tile = true;
#endif
}

void UIImage::set_file(string img_path)
{
#ifdef USE_TILE_LOCAL
    m_using_tile = false;

    if (!m_img.load_texture(img_path.c_str(), MIPMAP_NONE))
        return;

    m_tw = m_img.orig_width();
    m_th = m_img.orig_height();
#endif
}

void UIImage::_render()
{
#ifdef USE_TILE_LOCAL
    ui_push_scissor(m_region);
    if (m_using_tile)
    {
        TileBuffer tb;
        tb.set_tex(&tiles.get_image_manager()->m_textures[m_tile.tex]);

        for (int y = m_region[1]; y < m_region[1]+m_region[3]; y+=m_th)
            for (int x = m_region[0]; x < m_region[0]+m_region[2]; x+=m_tw)
            {
                tb.add(m_tile.tile, x, y, 0, 0, false, m_th, 1.0, 1.0);
            }

        tb.draw();
        tb.clear();
    }
    else
    {
        VertBuffer buf(true, false);
        buf.set_tex(&m_img);

        for (int y = m_region[1]; y < m_region[1]+m_region[3]; y+=m_th)
            for (int x = m_region[0]; x < m_region[0]+m_region[2]; x+=m_tw)
            {
                GLWPrim rect(x, y, x+m_tw, y+m_th);
                rect.set_tex(0, 0, (float)m_img.orig_width()/m_img.width(),
                        (float)m_img.orig_height()/m_img.height());
                buf.add_primitive(rect);
            }

        buf.draw();
    }
    ui_pop_scissor();
#endif
}

UISizeReq UIImage::_get_preferred_size(int dim, int prosp_width)
{
#ifdef USE_TILE_LOCAL
    UISizeReq ret = {
        // This is a little ad-hoc, but expand taking precedence over shrink when
        // determining the natural size makes the textured dialog box work
        dim ? (shrink_v ? 0 : m_th) : (shrink_h ? 0 : m_tw),
        dim ? (shrink_v ? 0 : m_th) : (shrink_h ? 0 : m_tw)
    };
#else
    UISizeReq ret = { 0, 0 };
#endif
    return ret;
}

void UIStack::add_child(shared_ptr<UI> child)
{
    m_children.push_back(move(child));
}

void UIStack::_render()
{
    for (auto const& child : m_children)
        child->render();
}

UISizeReq UIStack::_get_preferred_size(int dim, int prosp_width)
{
    UISizeReq r = { 0, 0 };
    for (auto const& child : m_children)
    {
        UISizeReq c = child->get_preferred_size(dim, prosp_width);
        r.min = max(r.min, c.min);
        r.nat = max(r.nat, c.nat);
    }
    return r;
}

void UIStack::_allocate_region()
{
    for (auto const& child : m_children)
    {
        i4 cr = m_region;
        UISizeReq pw = child->get_preferred_size(0, -1);
        cr[2] = min(max(pw.min, m_region[2]), pw.nat);
        UISizeReq ph = child->get_preferred_size(1, cr[2]);
        cr[3] = min(max(ph.min, m_region[3]), ph.nat);
        child->allocate_region(cr);
    }
}

void UIGrid::add_child(shared_ptr<UI> child, int x, int y, int w, int h)
{
    child_info ch = { {x, y}, {w, h} };
    m_child_info.push_back(ch);
    m_children.push_back(child);
    m_track_info_dirty = true;
}

void UIGrid::init_track_info()
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
}

void UIGrid::_render()
{
    for (auto const& child : m_children)
        child->render();
}

void UIGrid::compute_track_sizereqs(int dim)
{
    auto& track = dim ? m_row_info : m_col_info;
#define DIV_ROUND_UP(n,d) (((n)+(d)-1)/(d))

    for (auto& t : track)
        t.sr = {0, 0};
    for (size_t i = 0; i < m_children.size(); i++)
    {
        auto& cp = m_child_info[i].pos, cs = m_child_info[i].span;
        // if merging horizontally, need to find (possibly multi-col) width
        int prosp_width = dim ? get_tracks_region(cp[0], cp[1], cs[0], cs[1])[2] : -1;

        const UISizeReq c = m_children[i]->get_preferred_size(dim, prosp_width);
        // crappy but fast multitrack distribution here: if a child spans n tracks
        // each track gets 1/n-th of its sizereq; good enough for our needs
        for (int ti = cp[dim]; ti < cp[dim]+cs[dim]; ti++)
        {
            auto& s = track[ti].sr;
            s.min = max(s.min, DIV_ROUND_UP(c.min, cs[dim]));
            s.nat = max(s.nat, DIV_ROUND_UP(c.nat, cs[dim]));
        }
    }
}

void UIGrid::set_track_offsets(vector<track_info>& tracks)
{
    int acc = 0;
    for(auto& track : tracks)
    {
        track.offset = acc;
        acc += track.size;
    }
}

UISizeReq UIGrid::_get_preferred_size(int dim, int prosp_width)
{
    init_track_info();

    // get preferred column widths
    compute_track_sizereqs(0);

    // total width min and nat
    UISizeReq w_sr = { 0, 0 };
    for (auto const& col : m_col_info)
    {
        w_sr.min += col.sr.min;
        w_sr.nat += col.sr.nat;
    }

    if (!dim)
        return w_sr;

    layout_track(0, w_sr, prosp_width);
    set_track_offsets(m_col_info);

    // get preferred row heights for those widths
    compute_track_sizereqs(1);

    // total height min and nat
    UISizeReq h_sr = { 0, 0 };
    for (auto const& row : m_row_info)
    {
        h_sr.min += row.sr.min;
        h_sr.nat += row.sr.nat;
    }

    return h_sr;
}

void UIGrid::layout_track(int dim, UISizeReq sr, int size)
{
    auto& infos = dim ? m_row_info : m_col_info;

    float extra = size - sr.min;
    ASSERT(extra >= 0);
    int sum_flex_grow = 0;
    for (const auto& info : infos)
        sum_flex_grow += info.flex_grow;
    extra = sum_flex_grow > 0 ? extra/sum_flex_grow : 0;

    for(size_t i = 0; i < infos.size(); ++i)
        infos[i].size = infos[i].sr.min + extra*infos[i].flex_grow;
}

void UIGrid::_allocate_region()
{
    // Use of _-prefixed member function is necessary here
    UISizeReq h_sr = _get_preferred_size(1, m_region[2]);

    layout_track(1, h_sr, m_region[3]);
    set_track_offsets(m_row_info);

    ASSERT(m_children.size() == m_child_info.size());
    for (size_t i = 0; i < m_children.size(); i++)
    {
        auto& cp = m_child_info[i].pos, cs = m_child_info[i].span;
        i4 cell_reg = get_tracks_region(cp[0], cp[1], cs[0], cs[1]);
        cell_reg[0] += m_region[0];
        cell_reg[1] += m_region[1];
        m_children[i]->allocate_region(cell_reg);
    }
}

void UIRoot::set_child(shared_ptr<UI> ch)
{
    m_child = move(ch);
    m_dirty = true;
}

void UIRoot::resize(int w, int h)
{
    if (w == m_w && h == m_h)
        return;

    m_w = w;
    m_h = h;
    m_dirty = true;
}

void UIRoot::layout()
{
    if (!m_dirty)
        return;
    m_dirty = false;

    // Find preferred size with height-for-width
    UISizeReq sr_horz = m_child->get_preferred_size(0, -1);
    int width = min(max(sr_horz.min, m_w), sr_horz.nat);
    UISizeReq sr_vert = m_child->get_preferred_size(1, width);
    int height = min(max(sr_vert.min, m_h), sr_vert.nat);

#ifdef USE_TILE_LOCAL
    m_region = {0, 0, width, height};
#else
    m_region = {0, 0, m_w, m_h};
#endif
    m_child->allocate_region({0, 0, width, height});
}

void UIRoot::render()
{
    if (!m_child)
        return;

#ifdef USE_TILE_LOCAL
    glmanager->reset_view_for_redraw(0, 0);
#else
    clrscr();
#endif

    ui_push_scissor(m_region);
    m_child->render();
    ui_pop_scissor();

#ifdef USE_TILE_LOCAL
    wm->swap_buffers();
#else
    update_screen();
#endif
}

static void ui_push_scissor(i4 scissor)
{
    if (scissor_stack.size() > 0)
        scissor = aabb_intersect(scissor, scissor_stack.top());
    scissor_stack.push(scissor);
#ifdef USE_TILE_LOCAL
    glmanager->set_scissor(scissor[0], scissor[1], scissor[2], scissor[3]);
#endif
}

static void ui_pop_scissor()
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

void ui_push_layout(shared_ptr<UI> root)
{
    ui_root.set_child(move(root));

    // Ensure we start the event loop with a valid layout size
    // on tiles, this is done by ui_resize() for now
#ifndef USE_TILE_LOCAL
    ui_root.resize(get_number_of_cols(), get_number_of_lines());
#endif
}

void ui_pop_layout()
{
    ui_root.set_child(nullptr);
}

void ui_resize(int w, int h)
{
    ui_root.resize(w, h);
}

void ui_pump_events()
{
    ui_root.layout();
    ui_root.render();

#ifdef USE_TILE_LOCAL
    wm_event event;
    while (!wm->wait_event(&event));

    switch (event.type)
    {
        case WME_RESIZE:
        {
            ui_root.resize(event.resize.w, event.resize.h);
            coord_def ws(event.resize.w, event.resize.h);
            wm->resize(ws);
            break;
        }

        default:
            break;
    }
#else
    set_getch_returns_resizes(true);
    int k = getch_ck();
    set_getch_returns_resizes(false);

    if (k == CK_RESIZE)
    {
        // This may be superfluous, since the resize handler may have already
        // resized the screen
        console_shutdown();
        console_startup();
        ui_root.resize(get_number_of_cols(), get_number_of_lines());
    }
#endif
}
