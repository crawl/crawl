/*
 *  File:       tilereg-spl.cc
 *
 *  Created by: ennewalker on Sat Jan 5 01:33:53 2008 UTC
 */

#include "AppHdr.h"

#ifdef USE_TILE

#include "tilereg-spl.h"

#include "cio.h"
#include "libutil.h"
#include "macro.h"
#include "message.h"
#include "spl-cast.h"
#include "spl-util.h"
#include "stuff.h"
#include "tiledef-dngn.h"
#include "tiledef-main.h"
#include "viewgeom.h"

SpellRegion::SpellRegion(const TileRegionInit &init) : GridRegion(init)
{
}

void SpellRegion::activate()
{
    if (!you.spell_no)
    {
        canned_msg(MSG_NO_SPELLS);
        flush_prev_message();
    }
}

void SpellRegion::draw_tag()
{
    if (m_cursor == NO_CURSOR)
        return;

    int curs_index = cursor_index();
    if (curs_index >= (int)m_items.size())
        return;
    int idx = m_items[curs_index].idx;
    if (idx == -1)
        return;

    const spell_type spell = (spell_type) idx;
    std::string desc = make_stringf("%d MP    %s    (%s)",
                                    spell_difficulty(spell),
                                    spell_title(spell),
                                    failure_rate_to_string(spell_fail(spell)));
    draw_desc(desc.c_str());
}

int SpellRegion::handle_mouse(MouseEvent &event)
{
    unsigned int item_idx;
    if (!place_cursor(event, item_idx))
        return (0);

    const spell_type spell = (spell_type) m_items[item_idx].idx;
    if (event.button == MouseEvent::LEFT)
    {
        you.last_clicked_item = item_idx;
        tiles.set_need_redraw();
        if (!cast_a_spell(false, spell))
            flush_input_buffer( FLUSH_ON_FAILURE );
        return CK_MOUSE_CMD;
    }
    else if (spell != NUM_SPELLS && event.button == MouseEvent::RIGHT)
    {
        describe_spell(spell);
        redraw_screen();
        return CK_MOUSE_CMD;
    }
    return (0);
}

bool SpellRegion::update_tab_tip_text(std::string &tip, bool active)
{
    const char *prefix1 = active ? "" : "[L-Click] ";
    const char *prefix2 = active ? "" : "          ";

    tip = make_stringf("%s%s\n%s%s",
                       prefix1, "Display memorised spells",
                       prefix2, "Cast spells");

    return (true);
}

bool SpellRegion::update_tip_text(std::string& tip)
{
    if (m_cursor == NO_CURSOR)
        return (false);

    unsigned int item_idx = cursor_index();
    if (item_idx >= m_items.size() || m_items[item_idx].empty())
        return (false);

    int flag = m_items[item_idx].flag;
    std::vector<command_type> cmd;
    if (flag & TILEI_FLAG_INVALID)
        tip = "You cannot cast this spell right now.";
    else
    {
        tip = "[L-Click] Cast (%)";
        cmd.push_back(CMD_CAST_SPELL);
    }

    tip += "\n[R-Click] Describe (%)";
    cmd.push_back(CMD_DISPLAY_SPELLS);
    insert_commands(tip, cmd);

    return (true);
}

bool SpellRegion::update_alt_text(std::string &alt)
{
    if (m_cursor == NO_CURSOR)
        return (false);

    unsigned int item_idx = cursor_index();
    if (item_idx >= m_items.size() || m_items[item_idx].empty())
        return (false);

    if (you.last_clicked_item >= 0
        && item_idx == (unsigned int) you.last_clicked_item)
    {
        return (false);
    }

    int idx = m_items[item_idx].idx;

    const spell_type spell = (spell_type) idx;

    describe_info inf;
    get_spell_desc(spell, inf);

    alt_desc_proc proc(crawl_view.msgsz.x, crawl_view.msgsz.y);
    process_description<alt_desc_proc>(proc, inf);

    proc.get_string(alt);

    return (true);
}

static int _get_max_spells()
{
    int max_spells = 21;
    if (you.has_spell(SPELL_SELECTIVE_AMNESIA))
        max_spells++;

    return (max_spells);
}

int SpellRegion::get_max_slots()
{
    return (_get_max_spells());
}

void SpellRegion::pack_buffers()
{
    const int max_spells = get_max_slots();

    // Pack base separately, as it comes from a different texture...
    int i = 0;
    for (int y = 0; y < my; y++)
    {
        if (i >= max_spells)
            break;

        for (int x = 0; x < mx; x++)
        {
            if (i++ >= max_spells)
                break;

            m_buf.add_dngn_tile(TILE_ITEM_SLOT, x, y);
        }
    }

    i = 0;
    for (int y = 0; y < my; y++)
    {
        if (i >= (int)m_items.size())
            break;

        for (int x = 0; x < mx; x++)
        {
            if (i >= (int)m_items.size())
                break;

            InventoryTile &item = m_items[i++];
            if (item.flag & TILEI_FLAG_INVALID)
                m_buf.add_main_tile(TILE_MESH, x, y);

            if (item.flag & TILEI_FLAG_CURSOR)
                m_buf.add_main_tile(TILE_CURSOR, x, y);

            if (item.quantity != -1)
                draw_number(x, y, item.quantity);

            if (item.tile)
                m_buf.add_spell_tile(item.tile, x, y);
        }
    }
}

void SpellRegion::update()
{
    m_items.clear();
    m_dirty = true;

    if (mx * my == 0)
        return;

    const unsigned int max_spells = std::min(22, mx*my);

    for (int i = 0; i < 52; ++i)
    {
        const char letter = index_to_letter(i);
        const spell_type spell = get_spell_by_letter(letter);
        if (spell == SPELL_NO_SPELL)
            continue;

        InventoryTile desc;
        desc.tile     = tileidx_spell(spell);
        desc.idx      = (int) spell;
        desc.quantity = spell_difficulty(spell);

        std::string temp;
        if (is_prevented_teleport(spell)
            || spell_is_uncastable(spell, temp)
            || spell_mana(spell) > you.magic_points)
        {
            desc.flag |= TILEI_FLAG_INVALID;
        }

        m_items.push_back(desc);

        if (m_items.size() >= max_spells)
            break;
    }
}

#endif
