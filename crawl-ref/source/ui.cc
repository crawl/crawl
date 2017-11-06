/**
 * @file
 * @brief Hierarchical layout system.
**/

#include "AppHdr.h"

#include <numeric>

#include "ui.h"
#include "cio.h"

#ifdef USE_TILE_LOCAL
# include "windowmanager.h"
# include "glwrapper.h"
# include "tilebuf.h"
#else
# include "state.h"
# include "output.h"
# include "view.h"
# include "stringutil.h"
#endif

static struct UIRoot
{
public:
    void push_child(shared_ptr<UI> child);
    void pop_child();

    void resize(int w, int h);
    void layout();
    void render();

protected:
    int m_w, m_h;
    UIStack m_root;
    bool m_dirty{false};
} ui_root;

void UI::render()
{
    _render();
}

UISizeReq UI::get_preferred_size(Direction dim, int prosp_width)
{
    ASSERT((dim == HORZ) == (prosp_width == -1));

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

UISizeReq UI::_get_preferred_size(Direction dim, int prosp_width)
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

vector<int> UIBox::layout_cross_axis(vector<UISizeReq>& ch_psz, int cross_sz)
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

UISizeReq UIBox::_get_preferred_size(Direction dim, int prosp_width)
{
    vector<UISizeReq> sr(m_children.size());

    // Get preferred widths
    for (size_t i = 0; i < m_children.size(); i++)
        sr[i] = m_children[i]->get_preferred_size(UI::HORZ, -1);

    if (dim)
    {
        // Get actual widths
        vector<int> cw = horz ? layout_main_axis(sr, prosp_width) : layout_cross_axis(sr, prosp_width);

        // Get preferred heights
        for (size_t i = 0; i < m_children.size(); i++)
            sr[i] = m_children[i]->get_preferred_size(UI::VERT, cw[i]);
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
        sr[i] = m_children[i]->get_preferred_size(UI::HORZ, -1);

    // Get actual widths
    vector<int> cw = horz ? layout_main_axis(sr, m_region[2]) : layout_cross_axis(sr, m_region[2]);

    // Get preferred heights
    for (size_t i = 0; i < m_children.size(); i++)
        sr[i] = m_children[i]->get_preferred_size(UI::VERT, cw[i]);

    // Get actual heights
    vector<int> ch = horz ? layout_cross_axis(sr, m_region[3]) : layout_main_axis(sr, m_region[3]);

    auto const &m = horz ? cw : ch;
    int extra_main_space = m_region[horz ? 2 : 3] - accumulate(m.begin(), m.end(), 0);
    ASSERT(extra_main_space >= 0);

    // main axis offset
    int mo = extra_main_space*(justify_items - UI::START)/2;
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
            case UI::START:   xo = 0; break;
            case UI::CENTER:  xo = extra_cross_space/2; break;
            case UI::END:     xo = extra_cross_space; break;
            case UI::STRETCH: xo = 0; break;
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

void UIImage::set_tile(tile_def tile)
{
#ifdef USE_TILE_LOCAL
    m_tile = tile;
    const tile_info &ti = tiles.get_image_manager()->tile_def_info(m_tile);
    m_tw = ti.width;
    m_th = ti.height;
#endif
}

void UIImage::_render()
{
#ifdef USE_TILE_LOCAL
    glmanager->set_scissor(m_region[0], m_region[1], m_region[2], m_region[3]);
    TileBuffer tb;
    tb.set_tex(&tiles.get_image_manager()->m_textures[m_tile.tex]);

    for (int y = m_region[1]; y < m_region[1]+m_region[3]; y+=m_th)
        for (int x = m_region[0]; x < m_region[0]+m_region[2]; x+=m_tw)
            tb.add(m_tile.tile, x, y, 0, 0, false, m_th, 1.0, 1.0);

    tb.draw();
    tb.clear();
    glmanager->reset_scissor();
#endif
}

UISizeReq UIImage::_get_preferred_size(Direction dim, int prosp_width)
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

void UIStack::pop_child()
{
    if (!m_children.size())
        return;
    m_children.pop_back();
}

void UIStack::_render()
{
    for (auto const& child : m_children)
        child->render();
}

UISizeReq UIStack::_get_preferred_size(Direction dim, int prosp_width)
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
        UISizeReq pw = child->get_preferred_size(UI::HORZ, -1);
        cr[2] = min(max(pw.min, m_region[2]), pw.nat);
        UISizeReq ph = child->get_preferred_size(UI::VERT, cr[2]);
        cr[3] = min(max(ph.min, m_region[3]), ph.nat);
        child->allocate_region(cr);
    }
}

void UIGrid::add_child(shared_ptr<UI> child, int x, int y)
{
    m_child_pos.push_back({x, y});
    m_children.push_back(move(child));
    m_track_info_dirty = true;
}

void UIGrid::init_track_info()
{
    if (!m_track_info_dirty)
        return;
    m_track_info_dirty = false;

    m_row_info.clear();
    m_col_info.clear();
    // calculate the number of rows and columns
    int n_rows = 0, n_cols = 0;
    for (auto pos : m_child_pos)
    {
        n_rows = max(n_rows, pos[1]+1);
        n_cols = max(n_cols, pos[0]+1);
    }
    m_row_info.resize(n_rows);
    m_col_info.resize(n_cols);
}

void UIGrid::_render()
{
    for (auto const& child : m_children)
        child->render();
}

UISizeReq UIGrid::_get_preferred_size(Direction dim, int prosp_width)
{
    init_track_info();

    // get preferred column widths
    for (size_t i = 0; i < m_children.size(); i++)
    {
        auto& s = m_col_info[m_child_pos[i][0]].sr;
        UISizeReq c = m_children[i]->get_preferred_size(UI::HORZ, -1);
        s.min = max(s.min, c.min);
        s.nat = max(s.nat, c.nat);
    }

    // total width min and nat
    UISizeReq w_sr = { 0, 0 };
    for (auto const& col : m_col_info)
    {
        w_sr.min += col.sr.min;
        w_sr.nat += col.sr.nat;
    }

    if (!dim)
        return w_sr;

    layout_track(UI::HORZ, w_sr, prosp_width);

    // get preferred row heights for those widths
    for (size_t i = 0; i < m_children.size(); i++)
    {
        const auto& pos  = m_child_pos[i];
        const int col_w = m_col_info[pos[0]].size;
        auto& s = m_row_info[m_child_pos[i][1]].sr;
        UISizeReq c = m_children[i]->get_preferred_size(UI::VERT, col_w);
        s.min = max(s.min, c.min);
        s.nat = max(s.nat, c.nat);
    }

    // total height min and nat
    UISizeReq h_sr = { 0, 0 };
    for (auto const& row : m_row_info)
    {
        h_sr.min += row.sr.min;
        h_sr.nat += row.sr.nat;
    }

    return h_sr;
}

void UIGrid::layout_track(Direction dim, UISizeReq sr, int size)
{
    int extra = size - sr.min;
    ASSERT(extra >= 0);

    // TODO: distribute extra space properly: currently it
    // all goes to the second column
    auto& infos = dim ? m_row_info : m_col_info;
    for (size_t i = 0; i < infos.size(); ++i)
        infos[i].size = infos[i].sr.min + (i==1)*extra;
}

void UIGrid::_allocate_region()
{
    // Use of _-prefixed member function is necessary here
    UISizeReq h_sr = _get_preferred_size(UI::VERT, m_region[2]);

    layout_track(UI::VERT, h_sr, m_region[3]);

    // calculate the resulting offsets
    int acc = 0;
    for(size_t i = 0; i < m_row_info.size(); ++i)
    {
        m_row_info[i].offset = acc;
        acc += m_row_info[i].size;
    }
    acc = 0;
    for(size_t i = 0; i < m_col_info.size(); ++i)
    {
        m_col_info[i].offset = acc;
        acc += m_col_info[i].size;
    }

    ASSERT(m_children.size() == m_child_pos.size());
    for (size_t i = 0; i < m_children.size(); i++)
    {
        auto const& child = m_children[i];
        i2 pos = m_child_pos[i];

        i4 cell_reg = {
            m_col_info[pos[0]].offset + m_region[0],
            m_row_info[pos[1]].offset + m_region[1],
            m_col_info[pos[0]].size,
            m_row_info[pos[1]].size
        };
        child->allocate_region(cell_reg);
    }
}

void UIRoot::push_child(shared_ptr<UI> ch)
{
    m_root.add_child(move(ch));
    m_dirty = true;
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
    m_dirty = true;
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
    m_dirty = true;
}

void UIRoot::layout()
{
    if (!m_dirty)
        return;
    m_dirty = false;

    // Find preferred size with height-for-width: we never allocate less than
    // the minimum size, but may allocate more than the natural size.
    UISizeReq sr_horz = m_root.get_preferred_size(UI::HORZ, -1);
    int width = max(sr_horz.min, m_w);
    UISizeReq sr_vert = m_root.get_preferred_size(UI::VERT, width);
    int height = max(sr_vert.min, m_h);

    m_root.allocate_region({0, 0, width, height});
}

void UIRoot::render()
{
#ifdef USE_TILE_LOCAL
    glmanager->reset_view_for_redraw(0, 0);
#else
    clrscr();
#endif

#ifdef USE_TILE_LOCAL
    m_root.render();
#else
    // Render only the top of the UI stack on console
    if (m_root.num_children() > 0)
        m_root.get_child(m_root.num_children()-1)->render();
    else
        redraw_screen(false);
#endif

#ifdef USE_TILE_LOCAL
    wm->swap_buffers();
#else
    update_screen();
#endif
}

void ui_push_layout(shared_ptr<UI> root)
{
    ui_root.push_child(move(root));
}

void ui_pop_layout()
{
    ui_root.pop_child();
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
    wm_event event = {0};
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
        clrscr();
        console_shutdown();
        console_startup();
        ui_root.resize(get_number_of_cols(), get_number_of_lines());
    }
#endif
}
