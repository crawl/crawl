/*
 *  File:       tilereg-tab.cc
 *
 *  Created by: ennewalker on Sat Jan 5 01:33:53 2008 UTC
 */

#include "AppHdr.h"

#ifdef USE_TILE

#include "tilereg-tab.h"

#include "libutil.h"
#include "state.h"
#include "tiledef-gui.h"


TabbedRegion::TabbedRegion(const TileRegionInit &init) :
    GridRegion(init),
    m_active(-1),
    m_mouse_tab(-1),
    m_buf_gui(&init.im->m_textures[TEX_GUI])
{

}

TabbedRegion::~TabbedRegion()
{
}

void TabbedRegion::set_icon_pos(int idx)
{
    if (!m_tabs[idx].enabled)
        return;

    int start_y = 0;

    for (int i = 0; i < (int)m_tabs.size(); ++i)
    {
        if (i == idx || !m_tabs[i].reg)
            continue;
        start_y = std::max(m_tabs[i].max_y + 1, start_y);
    }
    m_tabs[idx].min_y = start_y;
    m_tabs[idx].max_y = start_y + m_tabs[idx].height;
}

void TabbedRegion::reset_icons(int from_idx)
{
    for (int i = from_idx; i < (int)m_tabs.size(); ++i)
    {
        m_tabs[i].min_y = 0;
        m_tabs[i].max_y = 0;
    }
    for (int i = from_idx; i < (int)m_tabs.size(); ++i)
        set_icon_pos(i);
}

void TabbedRegion::set_tab_region(int idx, GridRegion *reg, tileidx_t tile_tab)
{
    ASSERT(idx >= 0);
    ASSERT(idx >= (int)m_tabs.size() || !m_tabs[idx].reg);
    ASSERT(tile_tab);

    for (int i = (int)m_tabs.size(); i <= idx; ++i)
    {
        TabInfo inf;
        inf.reg = NULL;
        inf.tile_tab = 0;
        inf.ofs_y = 0;
        inf.min_y = 0;
        inf.max_y = 0;
        inf.height = 0;
        inf.enabled = true;
        m_tabs.push_back(inf);
    }

    const tile_info &inf = tile_gui_info(tile_tab);
    ox = std::max((int)inf.width, ox);

    // All tabs should be the same size.
    for (int i = 1; i < TAB_OFS_MAX; ++i)
    {
        const tile_info &inf_other = tile_gui_info(tile_tab + i);
        ASSERT(inf_other.height == inf.height);
        ASSERT(inf_other.width == inf.width);
    }

    ASSERT((int)m_tabs.size() > idx);
    m_tabs[idx].reg = reg;
    m_tabs[idx].tile_tab = tile_tab;
    m_tabs[idx].height = inf.height;
    set_icon_pos(idx);

    recalculate();
}

GridRegion *TabbedRegion::get_tab_region(int idx)
{
    if (invalid_index(idx))
        return (NULL);

    return (m_tabs[idx].reg);
}

void TabbedRegion::activate_tab(int idx)
{
    if (invalid_index(idx))
        return;

    if (!m_tabs[idx].enabled)
        return;

    if (m_active == idx)
        return;

    m_active = idx;
    m_dirty  = true;
    tiles.set_need_redraw();

    if (m_tabs[m_active].reg)
    {
        m_tabs[m_active].reg->activate();
        m_tabs[m_active].reg->update();
    }
}

int TabbedRegion::active_tab() const
{
    return (m_active);
}

int TabbedRegion::num_tabs() const
{
    return (m_tabs.size());
}

bool TabbedRegion::invalid_index(int idx) const
{
    return (idx < 0 || (int)m_tabs.size() <= idx);
}

void TabbedRegion::enable_tab(int idx)
{
    if (invalid_index(idx) || m_tabs[idx].enabled)
        return;

    m_tabs[idx].enabled = true;

    reset_icons(idx);

    m_dirty = true;
}

void TabbedRegion::disable_tab(int idx)
{
    if (invalid_index(idx) || !m_tabs[idx].enabled)
        return;

    m_tabs[idx].enabled = false;

    if (m_active == idx)
       m_active = (idx + 1) % (int)m_tabs.size();

    if (m_active == idx)
        m_active = -1;

    reset_icons(idx);

    m_dirty = true;
}

bool TabbedRegion::active_is_valid() const
{
    if (invalid_index(m_active))
        return (false);
    if (!m_tabs[m_active].enabled)
        return (false);
    if (!m_tabs[m_active].reg)
        return (false);

    return (true);
}

void TabbedRegion::update()
{
    if (!active_is_valid())
        return;

    m_tabs[m_active].reg->update();
}

void TabbedRegion::clear()
{
    for (size_t i = 0; i < m_tabs.size(); ++i)
    {
        if (m_tabs[i].reg)
            m_tabs[i].reg->clear();
    }
}

void TabbedRegion::pack_buffers()
{
    m_buf_gui.clear();

    for (int i = 0; i < (int)m_tabs.size(); ++i)
    {
        int ofs;
        if (!m_tabs[i].enabled)
            continue;
        else if (m_active == i)
            ofs = TAB_OFS_SELECTED;
        else if (m_mouse_tab == i)
            ofs = TAB_OFS_MOUSEOVER;
        else
            ofs = TAB_OFS_UNSELECTED;

        tileidx_t tileidx = m_tabs[i].tile_tab + ofs;
        const tile_info &inf = tile_gui_info(tileidx);
        int offset_y = m_tabs[i].min_y;
        m_buf_gui.add(tileidx, 0, 0, -inf.width, offset_y, false);
    }
}

void TabbedRegion::render()
{
    if (crawl_state.game_is_arena())
        return;

    if (!active_is_valid())
        return;

    if (m_dirty)
    {
        pack_buffers();
        m_dirty = false;
    }

#ifdef DEBUG_TILES_REDRAW
    cprintf("rendering TabbedRegion\n");
#endif
    set_transform();
    m_buf_gui.draw();

    m_tabs[m_active].reg->render();

    draw_tag();
}

void TabbedRegion::draw_tag()
{
    if (m_mouse_tab == -1)
        return;

    GridRegion *tab = m_tabs[m_mouse_tab].reg;
    if (!tab)
        return;

    draw_desc(tab->name().c_str());
}

void TabbedRegion::on_resize()
{
    int reg_sx = sx + ox;
    int reg_sy = sy;

    for (size_t i = 0; i < m_tabs.size(); ++i)
    {
        if (!m_tabs[i].reg)
            continue;

        m_tabs[i].reg->place(reg_sx, reg_sy);
        m_tabs[i].reg->resize(mx, my);
    }
}

int TabbedRegion::get_mouseover_tab(MouseEvent &event) const
{
    int x = event.px - sx;
    int y = event.py - sy;

    if (x < 0 || x > ox || y < 0 || y > wy)
        return (-1);

    for (int i = 0; i < (int)m_tabs.size(); ++i)
    {
        if (y >= m_tabs[i].min_y && y <= m_tabs[i].max_y)
            return (i);
    }
    return (-1);
}

int TabbedRegion::handle_mouse(MouseEvent &event)
{
    if (mouse_control::current_mode() != MOUSE_MODE_COMMAND)
        return (0);

    int mouse_tab = get_mouseover_tab(event);
    if (mouse_tab != m_mouse_tab)
    {
        m_mouse_tab = mouse_tab;
        m_dirty = true;
        tiles.set_need_redraw();
    }

    if (m_mouse_tab != -1)
    {
        if (event.event == MouseEvent::PRESS)
        {
            activate_tab(m_mouse_tab);
            return (0);
        }
    }

    // Otherwise, pass input to the active tab.
    if (!active_is_valid())
        return (0);

    return (get_tab_region(active_tab())->handle_mouse(event));
}

bool TabbedRegion::update_tab_tip_text(std::string &tip, bool active)
{
    return (false);
}

bool TabbedRegion::update_tip_text(std::string &tip)
{
    if (!active_is_valid())
        return (false);

    if (m_mouse_tab != -1)
    {
        GridRegion *reg = m_tabs[m_mouse_tab].reg;
        if (reg)
            return (reg->update_tab_tip_text(tip, m_mouse_tab == m_active));
    }

    return (get_tab_region(active_tab())->update_tip_text(tip));
}

bool TabbedRegion::update_alt_text(std::string &alt)
{
    if (!active_is_valid())
        return (false);

    return (get_tab_region(active_tab())->update_alt_text(alt));
}

#endif
