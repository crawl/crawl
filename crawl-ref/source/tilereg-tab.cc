#include "AppHdr.h"

#ifdef USE_TILE_LOCAL

#include "options.h"
#include "tilereg-tab.h"

#include "command-type.h"
#include "cio.h"
#include "directn.h"
#include "english.h"
#include "libutil.h"
#include "macro.h"
#include "state.h"
#include "stringutil.h"
#include "rltiles/tiledef-gui.h"
#include "tiles-build-specific.h"
#include "viewmap.h"

TabbedRegion::TabbedRegion(const TileRegionInit &init) :
    GridRegion(init),
    m_active(-1),
    m_mouse_tab(-1),
    m_use_small_layout(false),
    m_is_deactivated(false),
    m_buf_gui(&init.im->get_texture(TEX_GUI))
{
}

TabbedRegion::~TabbedRegion()
{
}

void TabbedRegion::set_icon_pos(int idx)
{
    if (!m_tabs[idx].enabled && !m_use_small_layout)
        return;

    int start_y = 0;

    for (int i = 0; i < (int)m_tabs.size(); ++i)
    {
        if (i == idx || (!m_tabs[i].reg && m_tabs[i].cmd==CMD_NO_CMD))
            continue;
        start_y = max(m_tabs[i].max_y + 1, start_y);
    }
    m_tabs[idx].min_y = start_y;
    if (!m_use_small_layout)
        m_tabs[idx].max_y = start_y + m_tabs[idx].height;
    else
        m_tabs[idx].max_y = start_y + m_tabs[idx].height*2; // spread half size tabs
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

int TabbedRegion::push_tab_button(command_type cmd, tileidx_t tile_tab)
{
    return _push_tab(nullptr, cmd, tile_tab);
}
int TabbedRegion::push_tab_region(GridRegion *reg, tileidx_t tile_tab)
{
    return _push_tab(reg, CMD_NO_CMD, tile_tab);
}

int TabbedRegion::_push_tab(GridRegion *reg, command_type cmd, tileidx_t tile_tab)
{
    int idx = m_tabs.size();
    ASSERT(idx >= 0);
    ASSERT(idx >= (int)m_tabs.size() || !m_tabs[idx].reg);
    ASSERT(tile_tab);

    TabInfo inf;
    inf.reg = nullptr;
    inf.cmd = CMD_NO_CMD;
    inf.tile_tab = 0;
    inf.ofs_y = 0;
    inf.min_y = 0;
    inf.max_y = 0;
    inf.height = 0;
    inf.orig_dx = 0;
    inf.orig_dy = 0;
    inf.enabled = true;
    m_tabs.push_back(inf);

    tileidx_t actual_tile_tab = (cmd==CMD_NO_CMD) ? tile_tab
                                                  : tileidx_t{TILEG_TAB_BLANK};
    const tile_info &tinf = tile_gui_info(actual_tile_tab);
    _icon_width = max((int)tinf.width, ox);
    ox = _icon_width;
#ifndef __ANDROID__
    _show_tab_icons = true;
#else
    _show_tab_icons = false;
#endif

    // All tabs should be the same size.
    for (int i = 1; i < TAB_OFS_MAX; ++i)
    {
        const tile_info &inf_other = tile_gui_info(actual_tile_tab + i);
        ASSERT(inf_other.height == tinf.height);
        ASSERT(inf_other.width == tinf.width);
    }

    ASSERT((int)m_tabs.size() > idx);
    m_tabs[idx].reg = reg;
    m_tabs[idx].cmd = cmd;
    m_tabs[idx].tile_tab = tile_tab;
    m_tabs[idx].height = tinf.height;
    m_tabs[idx].orig_dx = reg->dx;
    m_tabs[idx].orig_dy = reg->dy;
    set_icon_pos(idx);

    recalculate();

    return idx;
}

GridRegion *TabbedRegion::get_tab_region(int idx)
{
    if (invalid_index(idx))
        return nullptr;

    return m_tabs[idx].reg;
}

tileidx_t TabbedRegion::get_tab_tile(int idx)
{
    if (invalid_index(idx))
        return 0;

    return m_tabs[idx].tile_tab;
}

void TabbedRegion::activate_tab(int idx)
{
    if (invalid_index(idx))
        return;

    if (!m_tabs[idx].enabled && !m_use_small_layout)
        return;

    if (m_active == idx)
        if (m_use_small_layout && !m_is_deactivated)
            return deactivate_tab();

    m_active = idx;
    m_dirty  = true;
    m_is_deactivated = false;
    tiles.set_need_redraw();

    if (m_tabs[m_active].reg)
    {
        m_tabs[m_active].reg->activate();
        m_tabs[m_active].reg->update();
    }
}

void TabbedRegion::deactivate_tab()
{
    m_is_deactivated = true;

    m_dirty = true;
    tiles.set_need_redraw();

    if (m_tabs[m_active].reg)
        m_tabs[m_active].reg->clear();

    if (m_use_small_layout)
    {
        if (m_tabs[0].reg)
            m_tabs[0].reg->clear();
        m_active = 0;
    }
}

int TabbedRegion::active_tab() const
{
    return m_active;
}

int TabbedRegion::num_tabs() const
{
    return m_tabs.size();
}

bool TabbedRegion::invalid_index(int idx) const
{
    return idx < 0 || (int)m_tabs.size() <= idx;
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
        return false;
    if (!m_tabs[m_active].enabled && !m_use_small_layout)
        return false;
    if (!m_tabs[m_active].reg)
        return false;

    return true;
}

void TabbedRegion::update()
{
    if (!active_is_valid())
        return;
    if (m_is_deactivated)
        return;

    m_tabs[m_active].reg->update();
}

void TabbedRegion::clear()
{
    for (TabInfo &tab : m_tabs)
        if (tab.reg)
            tab.reg->clear();
}

void TabbedRegion::pack_buffers()
{
    m_buf_gui.clear();

    for (int i = 0; i < (int)m_tabs.size(); ++i)
    {
        int ofs;
        if (!m_tabs[i].enabled && !m_use_small_layout)
            continue;
        else if (m_active == i)
            ofs = TAB_OFS_SELECTED;
        else if (m_mouse_tab == i)
            ofs = TAB_OFS_MOUSEOVER;
        else
            ofs = TAB_OFS_UNSELECTED;

        tileidx_t tileidx = (m_tabs[i].cmd==CMD_NO_CMD) ? m_tabs[i].tile_tab + ofs : TILEG_TAB_BLANK + ofs;
        const tile_info &inf = tile_gui_info(tileidx);
        m_buf_gui.add(tileidx, 0, 0, -inf.width, m_tabs[i].min_y, false);
        if (m_tabs[i].cmd != CMD_NO_CMD)
        {
            const tile_info &inf_icon = tile_gui_info(m_tabs[i].tile_tab);
            m_buf_gui.add(m_tabs[i].tile_tab, 0, 0, -inf_icon.width, m_tabs[i].min_y, false, TILE_Y, TILE_X*inf_icon.width/inf.width, TILE_Y*inf_icon.height/inf.height);
        }
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
    if (ox > 0)
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

int TabbedRegion::min_height_for_items() const
{
    for (int i = (int)m_tabs.size()-1; i >= 0; --i)
    {
        if (m_tabs[i].enabled || m_use_small_layout)
            return m_tabs[i].max_y;
    }
    return 0;
}

void TabbedRegion::on_resize()
{
    int reg_sx = sx + ox;
    int reg_sy = sy;

    for (const TabInfo &tab : m_tabs)
    {
        if (!tab.reg)
            continue;

        // on small layout we want to draw tab from top-right corner inwards (down and left)
        if (m_use_small_layout)
        {
            reg_sx = 0;
            tab.reg->dx = dx;
            tab.reg->dy = dy;
        }
        else
        {
            tab.reg->dx = tab.orig_dx;
            tab.reg->dy = tab.orig_dy;
        }

        tab.reg->place(reg_sx, reg_sy);
        tab.reg->resize(mx, my);
    }
}

int TabbedRegion::get_mouseover_tab(wm_mouse_event &event) const
{
    int x = event.px - sx;
    int y = event.py - sy;

    if (m_use_small_layout)
    {
        // scale x and y back to fit
        // x = ... only works because we have offset all the way over to the right and
        //         it's just the margin (ox) that's visible
        x = ((sx+ox)-event.px)*32/dx;
    }

    y = y*32/dy;

    if (x < 0 || x > ox || y < 0 || y > wy)
        return -1;

    for (int i = 0; i < (int)m_tabs.size(); ++i)
        if (y >= m_tabs[i].min_y && y <= m_tabs[i].max_y)
            return i;

    return -1;
}

void TabbedRegion::_update_mouse_tab(wm_mouse_event &event)
{
    int mouse_tab = get_mouseover_tab(event);
    if (mouse_tab != m_mouse_tab)
    {
        m_mouse_tab = mouse_tab;
        m_dirty = true;
        tiles.set_need_redraw();
    }
}

int TabbedRegion::handle_mouse(wm_mouse_event &event)
{
    if (mouse_control::current_mode() != MOUSE_MODE_COMMAND)
        return 0;

    _update_mouse_tab(event);

    if (m_mouse_tab != -1)
    {
        if (event.event == wm_mouse_event::PRESS)
        {
            const command_type cmd = m_tabs[m_mouse_tab].cmd;
            if (cmd == CMD_NO_CMD)
            {
                activate_tab(m_mouse_tab);
                // prevent clicking on tab from 'bubbling up' to other regions
                return CK_NO_KEY;
            }

            if (cmd < CMD_NO_CMD || cmd > CMD_MAX_NORMAL)
                return CK_NO_KEY;
            return encode_command_as_key(m_tabs[m_mouse_tab].cmd);
        }
    }

    // Otherwise, pass input to the active tab.
    if (!active_is_valid())
        return 0;

    return get_tab_region(active_tab())->handle_mouse(event);
}

bool TabbedRegion::handle_mouse_for_map_view(wm_mouse_event &event)
{
    _update_mouse_tab(event);

    if (m_mouse_tab != -1)
    {
        if (event.event == wm_mouse_event::PRESS)
        {
            const command_type cmd = m_tabs[m_mouse_tab].cmd;
            if (cmd == CMD_NO_CMD)
            {
                activate_tab(m_mouse_tab);
                // prevent clicking on tab from 'bubbling up' to other regions
                return true;
            }

            if (cmd < CMD_MIN_OVERMAP || cmd > CMD_MAX_OVERMAP)
                return true;
            process_map_command(cmd);
            return true;
        }
    }

    // Otherwise, pass input to the active tab.
    if (!active_is_valid())
        return false;

    return get_tab_region(active_tab())->handle_mouse_for_map_view(event);
}

bool TabbedRegion::handle_mouse_for_targeting(wm_mouse_event &event)
{
    _update_mouse_tab(event);

    if (m_mouse_tab != -1)
    {
        if (event.event == wm_mouse_event::PRESS)
        {
            const command_type cmd = m_tabs[m_mouse_tab].cmd;
            if (cmd == CMD_NO_CMD)
            {
                activate_tab(m_mouse_tab);
                return true;
            }

            if (cmd < CMD_MIN_TARGET || cmd > CMD_MAX_TARGET)
                return true;
            process_targeting_command(cmd);
            return true;
        }
    }

    // Otherwise, pass input to the active tab.
    if (!active_is_valid())
        return false;

    return get_tab_region(active_tab())->handle_mouse_for_targeting(event);
}

bool TabbedRegion::update_tab_tip_text(string &/*tip*/, bool /*active*/)
{
    return false;
}

bool TabbedRegion::update_tip_text(string &tip)
{
    if (!active_is_valid())
        return false;

    if (m_mouse_tab != -1)
    {
        GridRegion *reg = m_tabs[m_mouse_tab].reg;
        if (reg)
            return reg->update_tab_tip_text(tip, m_mouse_tab == m_active);
    }

    return get_tab_region(active_tab())->update_tip_text(tip);
}

bool TabbedRegion::update_alt_text(string &alt)
{
    if (!active_is_valid())
        return false;

    return get_tab_region(active_tab())->update_alt_text(alt);
}

int TabbedRegion::find_tab(string tab_name) const
{
    lowercase(tab_name);
    string pluralised_name = pluralise(tab_name);
    for (int i = 0, size = m_tabs.size(); i < size; ++i)
    {
        if (m_tabs[i].reg == nullptr)
            continue;

        string reg_name = lowercase_string(m_tabs[i].reg->name());
        if (tab_name == reg_name || pluralised_name == reg_name)
            return i;
    }

    return -1;
}

void TabbedRegion::set_small_layout(bool use_small_layout, const coord_def &windowsz)
{
    if (m_use_small_layout != use_small_layout)
    {
        activate_tab(0);
        if (use_small_layout)
        {
            deactivate_tab();
            if (!_show_tab_icons)
                ox = 0;
        }
        else
            ox = _icon_width;
        m_use_small_layout = use_small_layout;
    }

    if (m_tabs.size() == 0 || !use_small_layout)
    {
        dx = Options.tile_sidebar_pixels;
        dy = Options.tile_sidebar_pixels;
    }
    else
    {
        // original dx (32) * region height (240) / num tabs (7) / height of tab (20)
        int scale = (Options.tile_sidebar_pixels-1) * windowsz.y/num_tabs()/m_tabs[m_active].height -1;
        scale /= 2; // half size fits better
        dx = scale;
        dy = scale;
    }

    reset_icons(0);
    m_dirty = true;
    tiles.set_need_redraw();
}

void TabbedRegion::toggle_tab_icons()
{
    _show_tab_icons = !_show_tab_icons;
    if (_show_tab_icons)
      ox = _icon_width;
    else
      ox = 0;
}

#endif
