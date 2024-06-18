#include "AppHdr.h"

#ifdef USE_TILE_LOCAL

#include "tilereg-cmd.h"

#include "ability.h"
#include "cio.h"
#include "command.h"
#include "describe.h"
#include "directn.h"
#include "env.h"
#include "items.h"
#include "libutil.h"
#include "macro.h"
#include "nearby-danger.h"
#include "religion.h"
#include "spl-cast.h"
#include "stringutil.h"
#include "rltiles/tiledef-icons.h"
#include "tilepick.h"
#include "tiles-build-specific.h"
#include "viewmap.h"

CommandRegion::CommandRegion(const TileRegionInit &init,
                             const command_type commands[],
                             const int n_commands, const string _name,
                             const string help) :
    GridRegion(init),
    _common_commands( commands, commands + n_commands ),
    m_name(_name),
    m_help(help)
{
    n_common_commands = n_commands;
}

void CommandRegion::activate()
{
}

void CommandRegion::draw_tag()
{
    if (m_cursor == NO_CURSOR)
        return;

    int curs_index = cursor_index();
    if (curs_index >= (int)m_items.size())
        return;
    int idx = m_items[curs_index].idx;
    if (idx == -1)
        return;

    const command_type cmd = (command_type) idx;

    draw_desc(get_command_description(cmd, true).c_str());
}

int CommandRegion::handle_mouse(wm_mouse_event &event)
{
    unsigned int item_idx;
    if (!place_cursor(event, item_idx))
        return 0;

    if (event.button == wm_mouse_event::LEFT)
    {
        const command_type cmd = (command_type) m_items[item_idx].idx;
        m_last_clicked_item = item_idx;

        if (cmd >= CMD_NO_CMD && cmd <= CMD_MAX_NORMAL)
            return encode_command_as_key(cmd);
    }
    return 0;
}

bool CommandRegion::handle_mouse_for_map_view(wm_mouse_event &event)
{
    unsigned int item_idx;
    if (!place_cursor(event, item_idx))
        return false;

    if (event.button == wm_mouse_event::LEFT)
    {
        const command_type cmd = (command_type)m_items[item_idx].idx;
        m_last_clicked_item = item_idx;

        if (cmd >= CMD_MIN_OVERMAP && cmd <= CMD_MAX_OVERMAP)
        {
            process_map_command(cmd);
            return true;
        }
    }
    return false;
}

bool CommandRegion::handle_mouse_for_targeting(wm_mouse_event &event)
{
    unsigned int item_idx;
    if (!place_cursor(event, item_idx))
        return false;

    if (event.button == wm_mouse_event::LEFT)
    {
        const command_type cmd = (command_type)m_items[item_idx].idx;
        m_last_clicked_item = item_idx;

        if (cmd >= CMD_MIN_TARGET && cmd <= CMD_MAX_TARGET)
        {
            process_targeting_command(cmd);
            return true;
        }
    }
    return false;
}

bool CommandRegion::update_tab_tip_text(string &tip, bool active)
{
    const char *prefix = active ? "" : "[L-Click] ";

    tip = make_stringf("%s%s", prefix, m_help.c_str());
    return true;
}

bool CommandRegion::update_tip_text(string& tip)
{
    if (m_cursor == NO_CURSOR)
        return false;

    unsigned int item_idx = cursor_index();
    if (item_idx >= m_items.size() || m_items[item_idx].empty())
        return false;

    const command_type cmd = (command_type) m_items[item_idx].idx;
    tip = make_stringf("[L-Click] %s",
                       get_command_description(cmd, true).c_str());

    if (command_to_key(cmd) != '\0')
    {
        tip += " (%)";
        insert_commands(tip, { cmd });
    }

    // tip += "\n[R-Click] Describe";

    return true;
}

bool CommandRegion::update_alt_text(string &alt)
{
    if (m_cursor == NO_CURSOR)
        return false;

    unsigned int item_idx = cursor_index();
    if (item_idx >= m_items.size() || m_items[item_idx].empty())
        return false;

    if (m_last_clicked_item >= 0
        && item_idx == (unsigned int) m_last_clicked_item)
    {
        return false;
    }

    int idx = m_items[item_idx].idx;

    const command_type cmd = (command_type) idx;

    const string desc = get_command_description(cmd, false);
    if (desc.empty())
        return false;

    describe_info inf;
    inf.body << desc;

    alt = process_description(inf);
    return true;
}

void CommandRegion::pack_buffers()
{
    unsigned int i = 0;
    for (int y = 0; y < my; y++)
    {
        if (i >= m_items.size())
            break;

        for (int x = 0; x < mx; x++)
        {
            if (i >= m_items.size())
                break;

            InventoryTile &item = m_items[i++];
            if (item.flag & TILEI_FLAG_INVALID)
                m_buf.add_icons_tile(TILEI_MESH, x, y);

            if (item.tile)
                m_buf.add_command_tile(item.tile, x, y);

            if (item.flag & TILEI_FLAG_CURSOR)
                m_buf.add_icons_tile(TILEI_CURSOR, x, y);
        }
    }
}

bool tile_command_not_applicable(const command_type cmd, bool safe)
{
    // in the level map, only the map pan commands work
    const bool map_cmd = context_for_command(cmd) == KMC_LEVELMAP;
    if (tiles.get_map_display() != map_cmd)
        return true;

    switch (cmd)
    {
    case CMD_REST:
        return !safe || !can_rest_here();
    case CMD_EXPLORE:
    case CMD_INTERLEVEL_TRAVEL:
    case CMD_MEMORISE_SPELL:
        return !safe;
    case CMD_DISPLAY_RELIGION:
        return you_worship(GOD_NO_GOD);
    case CMD_USE_ABILITY:
        return your_talents(false).empty();
    case CMD_CAST_SPELL:
        return !you.spell_no || !can_cast_spells(true);
    default:
        return false;
    }
}

bool tile_command_not_applicable(const command_type cmd)
{
    return tile_command_not_applicable(cmd, i_feel_safe(false));
}

void CommandRegion::update()
{
    m_items.clear();
    m_dirty = true;

    if (mx * my == 0)
        return;

    const bool safe = i_feel_safe(false);

    for (int idx = 0; idx < n_common_commands; ++idx)
    {
        command_type cmd = _common_commands[idx];

        // hackily toggle between display map and exit map display depending
        // on whether we are in map mode
        if (cmd == CMD_DISPLAY_MAP && tiles.get_map_display())
            cmd = CMD_MAP_EXIT_MAP;

        InventoryTile desc;
        desc.tile = tileidx_command(cmd);
        desc.idx  = cmd;

        // Auto-explore while monsters around etc.
        if (tile_command_not_applicable(cmd, safe))
            desc.flag |= TILEI_FLAG_INVALID;

        m_items.push_back(desc);
    }
}

#endif
