/*
 *  File:       tilereg-cmd.cc
 *  Created by: jpeg on Sat, Dec 27 2010
 */

#include "AppHdr.h"

#ifdef USE_TILE

#include "tilereg-cmd.h"

#include "abl-show.h"
#include "cio.h"
#include "command.h"
#include "enum.h"
#include "env.h"
#include "libutil.h"
#include "macro.h"
#include "misc.h"
#include "terrain.h"
#include "tiledef-dngn.h"
#include "tiledef-icons.h"
#include "tilepick.h"
#include "viewgeom.h"

CommandRegion::CommandRegion(const TileRegionInit &init) : GridRegion(init)
{
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

int CommandRegion::handle_mouse(MouseEvent &event)
{
    unsigned int item_idx;
    if (!place_cursor(event, item_idx))
        return (0);

    if (event.button == MouseEvent::LEFT)
    {
        const command_type cmd = (command_type) m_items[item_idx].idx;
        m_last_clicked_item = item_idx;
        process_command(cmd);
        return CK_MOUSE_CMD;
    }
    return (0);
}

bool CommandRegion::update_tab_tip_text(std::string &tip, bool active)
{
    const char *prefix = active ? "" : "[L-Click] ";

    tip = make_stringf("%s%s",
                       prefix, "Execute commands");

    return (true);
}

bool CommandRegion::update_tip_text(std::string& tip)
{
    if (m_cursor == NO_CURSOR)
        return (false);

    unsigned int item_idx = cursor_index();
    if (item_idx >= m_items.size() || m_items[item_idx].empty())
        return (false);

    const command_type cmd = (command_type) m_items[item_idx].idx;
    tip = make_stringf("[L-Click] %s",
                       get_command_description(cmd, true).c_str());

    if (command_to_key(cmd) != '\0')
    {
        tip += " (%)";
        insert_commands(tip, cmd, 0);
    }

    // tip += "\n[R-Click] Describe";

    return (true);
}

bool CommandRegion::update_alt_text(std::string &alt)
{
    if (m_cursor == NO_CURSOR)
        return (false);

    unsigned int item_idx = cursor_index();
    if (item_idx >= m_items.size() || m_items[item_idx].empty())
        return (false);

    if (m_last_clicked_item >= 0
        && item_idx == (unsigned int) m_last_clicked_item)
    {
        return (false);
    }

    int idx = m_items[item_idx].idx;

    const command_type cmd = (command_type) idx;

    const std::string desc = get_command_description(cmd, false);
    if (desc.empty())
        return (false);

    describe_info inf;
    inf.body << desc;

    alt_desc_proc proc(crawl_view.msgsz.x, crawl_view.msgsz.y);
    process_description<alt_desc_proc>(proc, inf);
    proc.get_string(alt);

    return (true);
}

void CommandRegion::pack_buffers()
{
    int i = 0;
    for (int y = 0; y < my; y++)
    {
        if (i >= (int)m_items.size())
            break;

        for (int x = 0; x < mx; x++)
        {
            if (i >= (int)m_items.size())
                break;

            m_buf.add_dngn_tile(TILE_ITEM_SLOT, x, y);

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

static bool _command_not_applicable(const command_type cmd)
{
    switch (cmd)
    {
    case CMD_REST:
    case CMD_EXPLORE:
    case CMD_INTERLEVEL_TRAVEL:
        return (!i_feel_safe(false));
    case CMD_DISPLAY_RELIGION:
        return (you.religion == GOD_NO_GOD);
    case CMD_PRAY:
        return (you.religion == GOD_NO_GOD
                && !feat_is_altar(grd(you.pos())));
    case CMD_USE_ABILITY:
        return (your_talents(false).empty());
    default:
        return (false);
    }
}

static const command_type _common_commands[] =
{
    // action commands
    CMD_REST, CMD_EXPLORE, CMD_INTERLEVEL_TRAVEL,
#ifdef CLUA_BINDINGS
    CMD_AUTOFIGHT,
#endif
    CMD_USE_ABILITY, CMD_PRAY, CMD_SEARCH_STASHES,

    // informational commands
    CMD_REPLAY_MESSAGES, CMD_RESISTS_SCREEN, CMD_DISPLAY_OVERMAP,
    CMD_DISPLAY_RELIGION, CMD_DISPLAY_MUTATIONS, CMD_DISPLAY_SKILLS,
    CMD_DISPLAY_CHARACTER_STATUS, CMD_DISPLAY_KNOWN_OBJECTS,

    // meta commands
    CMD_SAVE_GAME_NOW, CMD_EDIT_PLAYER_TILE, CMD_DISPLAY_COMMANDS,
    CMD_CHARACTER_DUMP
};

void CommandRegion::update()
{
    m_items.clear();
    m_dirty = true;

    if (mx * my == 0)
        return;

    const int max_size = ARRAYSZ(_common_commands);
    for (int idx = 0; idx < max_size; ++idx)
    {
        const command_type cmd = _common_commands[idx];

        InventoryTile desc;
        desc.tile = tileidx_command(cmd);
        desc.idx  = cmd;

        // Auto-explore while monsters around etc.
        if (_command_not_applicable(cmd))
            desc.flag |= TILEI_FLAG_INVALID;

        m_items.push_back(desc);
    }
}

#endif
