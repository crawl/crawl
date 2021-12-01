#include "AppHdr.h"

#ifdef USE_TILE_LOCAL

#include "tilereg-mem.h"

#include "cio.h"
#include "describe.h"
#include "localise.h"
#include "macro.h"
#include "output.h"
#include "religion.h"
#include "spl-book.h"
#include "spl-cast.h"
#include "spl-util.h"
#include "stringutil.h"
#include "tile-inventory-flags.h"
#include "tilepick.h"
#include "tilereg-cmd.h"
#include "tiles-build-specific.h"
#include "unicode.h"

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
    string desc = localise("%s    (%s)    %d/",
                           spell_title(spell),
                           failure,
                           spell_levels_required(spell));
    if (player_spell_levels() == 1)
        desc += localise("1 spell slot");
    else
        desc += localise("%d spell slots", player_spell_levels());
    draw_desc(desc);
}

int MemoriseRegion::handle_mouse(wm_mouse_event &event)
{
    unsigned int item_idx;
    if (!place_cursor(event, item_idx)
        || tile_command_not_applicable(CMD_MEMORISE_SPELL))
    {
        return 0;
    }

    const spell_type spell = (spell_type) m_items[item_idx].idx;
    if (event.button == wm_mouse_event::LEFT)
    {
        m_last_clicked_item = item_idx;
        tiles.set_need_redraw();
        if (learn_spell(spell))
            tiles.update_tabs();
        else
            flush_input_buffer(FLUSH_ON_FAILURE);
        return CK_MOUSE_CMD;
    }
    else if (event.button == wm_mouse_event::RIGHT)
    {
        describe_spell(spell);
        redraw_screen();
        update_screen();
        return CK_MOUSE_CMD;
    }
    return 0;
}

bool MemoriseRegion::update_tab_tip_text(string &tip, bool active)
{
    const string prefix1 = active ? "" : localise("[L-Click]") + " ";
    const string prefix2 = string(strwidth(prefix1), ' ');

    tip = prefix1 + localise("Display spells in carried books") + "\n";
    tip += prefix2 + localise("Memorise spells");

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
        tip = localise("You cannot memorise this spell now.");
    else
    {
        tip = localise("%s %s (%%)", "[L-Click]", "Memorise");
        cmd.push_back(CMD_MEMORISE_SPELL);
    }

    tip += localise("\n%s %s", "[R-Click]", "Describe");

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

    vector<spell_type> spells = get_sorted_spell_list(); // TODO: should this be silent?
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
            || player_spell_levels() < spell_levels_required(spell)
            || tile_command_not_applicable(CMD_MEMORISE_SPELL))
        {
            desc.flag |= TILEI_FLAG_INVALID;
        }

        if (vehumet_is_offering(spell))
            desc.flag |= TILEI_FLAG_EQUIP;

        m_items.push_back(desc);
    }
}

#endif
