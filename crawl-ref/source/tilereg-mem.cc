#include "AppHdr.h"

#ifdef USE_TILE_LOCAL

#include "tilereg-mem.h"

#include "cio.h"
#include "describe.h"
#include "macro.h"
#include "output.h"
#include "religion.h"
#include "spl-book.h"
#include "spl-cast.h"
#include "spl-util.h"
#include "stringutil.h"
#include "tilepick.h"

MemoriseRegion::MemoriseRegion(const TileRegionInit &init) : SpellRegion(init)
{
}

void MemoriseRegion::activate()
{
    // Print a fitting message if we can't memorise anything.
    has_spells_to_memorise(false);
}

int MemoriseRegion::get_max_slots()
{
    return m_items.size();
}

void MemoriseRegion::draw_tag()
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
    const string failure = failure_rate_to_string(raw_spell_fail(spell));
    string desc = make_stringf("%s    (%s)    %d/%d spell slot%s",
                               spell_title(spell),
                               failure.c_str(),
                               spell_levels_required(spell),
                               player_spell_levels(),
                               spell_levels_required(spell) > 1 ? "s" : "");
    draw_desc(desc.c_str());
}

int MemoriseRegion::handle_mouse(MouseEvent &event)
{
    unsigned int item_idx;
    if (!place_cursor(event, item_idx))
        return 0;

    const spell_type spell = (spell_type) m_items[item_idx].idx;
    if (event.button == MouseEvent::LEFT)
    {
        m_last_clicked_item = item_idx;
        tiles.set_need_redraw();
        if (learn_spell(spell))
            tiles.update_tabs();
        else
            flush_input_buffer(FLUSH_ON_FAILURE);
        return CK_MOUSE_CMD;
    }
    else if (event.button == MouseEvent::RIGHT)
    {
        describe_spell(spell);
        redraw_screen();
        return CK_MOUSE_CMD;
    }
    return 0;
}

bool MemoriseRegion::update_tab_tip_text(string &tip, bool active)
{
    const char *prefix1 = active ? "" : "[L-Click] ";
    const char *prefix2 = active ? "" : "          ";

    tip = make_stringf("%s%s\n%s%s",
                       prefix1, "Display spells in carried books",
                       prefix2, "Memorise spells");

    return true;
}

bool MemoriseRegion::update_tip_text(string& tip)
{
    if (m_cursor == NO_CURSOR)
        return false;

    unsigned int item_idx = cursor_index();
    if (item_idx >= m_items.size() || m_items[item_idx].empty())
        return false;

    int flag = m_items[item_idx].flag;
    vector<command_type> cmd;
    if (flag & TILEI_FLAG_INVALID)
        tip = "You cannot memorise this spell now.";
    else
    {
        tip = "[L-Click] Memorise (%)";
        cmd.push_back(CMD_MEMORISE_SPELL);
    }

    tip += "\n[R-Click] Describe";

    insert_commands(tip, cmd);
    return true;
}

void MemoriseRegion::update()
{
    m_items.clear();
    m_dirty = true;

    if (mx * my == 0)
        return;

    if (!has_spells_to_memorise())
        return;

    const unsigned int max_spells = mx * my;

    vector<spell_type> spells = get_mem_spell_list();
    for (unsigned int i = 0; m_items.size() < max_spells && i < spells.size();
         ++i)
    {
        const spell_type spell = spells[i];

        InventoryTile desc;
        desc.tile     = tileidx_spell(spell);
        desc.idx      = (int) spell;
        desc.quantity = spell_difficulty(spell);

        if (!can_learn_spell(true)
            || spell_difficulty(spell) > you.experience_level
            || player_spell_levels() < spell_levels_required(spell))
        {
            desc.flag |= TILEI_FLAG_INVALID;
        }

        if (vehumet_is_offering(spell))
            desc.flag |= TILEI_FLAG_EQUIP;

        m_items.push_back(desc);
    }
}

#endif
