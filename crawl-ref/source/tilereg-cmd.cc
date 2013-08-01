#include "AppHdr.h"

#ifdef USE_TILE_LOCAL

#include "tilereg-cmd.h"
#include "process_desc.h"

#include "abl-show.h"
#include "cio.h"
#include "command.h"
#include "process_desc.h"
#include "enum.h"
#include "env.h"
#include "libutil.h"
#include "macro.h"
#include "misc.h"
#include "religion.h"
#include "terrain.h"
#include "tiledef-dngn.h"
#include "tiledef-icons.h"
#include "tilepick.h"
#include "viewgeom.h"
#include "spl-cast.h"
#include "items.h"
#include "areas.h"

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

int CommandRegion::handle_mouse(MouseEvent &event)
{
    unsigned int item_idx;
    if (!place_cursor(event, item_idx))
        return 0;

    if (event.button == MouseEvent::LEFT)
    {
        const command_type cmd = (command_type) m_items[item_idx].idx;
        m_last_clicked_item = item_idx;

        if (tiles.is_using_small_layout())
        {
            // close the tab that we've just successfully used a command from
            tiles.deactivate_tab();
        }

        // this is a really horrid way to preserve the interface in viewmap.cc
        // which expects a keypress rather than a command :(
        if (tiles.get_map_display())
            return command_to_key(cmd);

        process_command(cmd);
        return CK_MOUSE_CMD;
    }
    return 0;
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
        insert_commands(tip, cmd, 0);
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

    alt_desc_proc proc(crawl_view.msgsz.x, crawl_view.msgsz.y);
    process_description<alt_desc_proc>(proc, inf);
    proc.get_string(alt);

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

static bool _command_not_applicable(const command_type cmd, bool safe)
{
    switch (cmd)
    {
    case CMD_REST:
    case CMD_EXPLORE:
    case CMD_INTERLEVEL_TRAVEL:
        return (!safe);
    case CMD_DISPLAY_RELIGION:
        return you_worship(GOD_NO_GOD);
    case CMD_PRAY:
        return (you_worship(GOD_NO_GOD)
                && !feat_is_altar(grd(you.pos())));
    case CMD_USE_ABILITY:
        return (your_talents(false).empty());
    case CMD_BUTCHER:
        // this logic is enormously simplistic compared to food.cc
        for (stack_iterator si(you.pos(), true); si; ++si)
            if (si->base_type == OBJ_CORPSES && si->sub_type == CORPSE_BODY)
                return false;
        if (you.species == SP_VAMPIRE)
            return false;
        return true;
    case CMD_CAST_SPELL:
        return // shamefully copied from _can_cast in spl-cast.cc
            (you.form == TRAN_BAT || you.form == TRAN_PIG) ||
            (you.stat_zero[STAT_INT]) ||
            (you.no_cast()) ||
            (!you.spell_no) ||
            (you.berserk()) ||
            (you.confused()) ||
            (silenced(you.pos()));
    case CMD_DISPLAY_MAP:
        return tiles.get_map_display();
    case CMD_MAP_GOTO_TARGET:
    case CMD_MAP_ADD_WAYPOINT:
    case CMD_MAP_EXCLUDE_AREA:
    case CMD_MAP_CLEAR_EXCLUDES:
    case CMD_MAP_NEXT_LEVEL:
    case CMD_MAP_PREV_LEVEL:
    case CMD_MAP_GOTO_LEVEL:
    case CMD_MAP_FIND_UPSTAIR:
    case CMD_MAP_FIND_DOWNSTAIR:
    case CMD_MAP_FIND_YOU:
    case CMD_MAP_FIND_PORTAL:
    case CMD_MAP_FIND_TRAP:
    case CMD_MAP_FIND_ALTAR:
    case CMD_MAP_FIND_EXCLUDED:
    case CMD_MAP_FIND_WAYPOINT:
    case CMD_MAP_FIND_STASH:
        return !tiles.get_map_display();
    default:
        return false;
    }
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
        const command_type cmd = _common_commands[idx];

        InventoryTile desc;
        desc.tile = tileidx_command(cmd);
        desc.idx  = cmd;

        // Auto-explore while monsters around etc.
        if (_command_not_applicable(cmd, safe))
            desc.flag |= TILEI_FLAG_INVALID;

        m_items.push_back(desc);
    }
}

#endif
