/*
 *  File:       tilereg-skl.cc
 *
 *  Created by: jpeg on Sat, Nov 27 2010
 */

#include "AppHdr.h"

#ifdef USE_TILE

#include "tilereg-skl.h"

#include "skills.h"
#include "skills2.h"

#include "cio.h"
#include "libutil.h"
#include "macro.h"
#include "message.h"
#include "spl-cast.h"
#include "spl-util.h"
#include "stuff.h"
#include "tiledef-dngn.h"
#include "tiledef-main.h"
#include "tilepick.h"
#include "viewgeom.h"

SkillRegion::SkillRegion(const TileRegionInit &init) : GridRegion(init)
{
}

void SkillRegion::activate()
{
}

void SkillRegion::draw_tag()
{
    if (m_cursor == NO_CURSOR)
        return;

    int curs_index = cursor_index();
    if (curs_index >= (int)m_items.size())
        return;
    int idx = m_items[curs_index].idx;
    if (idx == -1)
        return;

    const skill_type skill = (skill_type) idx;
    const int apt          = species_apt(skill, you.species);
    std::string desc = make_stringf("%-14s Skill %2d   Aptitude %c%d",
                                    skill_name(skill),
									you.skills[skill],
									apt > 0 ? '+' : ' ',
									apt);

    draw_desc(desc.c_str());
}

int SkillRegion::handle_mouse(MouseEvent &event)
{
    unsigned int item_idx;
    if (!place_cursor(event, item_idx))
        return (0);

    const skill_type skill = (skill_type) m_items[item_idx].idx;
    if (event.button == MouseEvent::LEFT)
    {
        m_last_clicked_item = item_idx;
        if (you.skills[skill] == 0)
            mpr("You cannot toggle a skill you don't have yet.");
        else if (you.skills[skill] >= 27)
            mpr("There's no point to toggling this skill anymore.");
        else
        {
            tiles.set_need_redraw();
            you.practise_skill[skill] = !you.practise_skill[skill];
        }
        return CK_MOUSE_CMD;
    }
    else if (skill != NUM_SKILLS && event.button == MouseEvent::RIGHT)
    {
        describe_skill(skill);
        redraw_screen();
        return CK_MOUSE_CMD;
    }
    return (0);
}

bool SkillRegion::update_tab_tip_text(std::string &tip, bool active)
{
    const char *prefix = active ? "" : "[L-Click] ";

    tip = make_stringf("%s%s",
                       prefix, "Manage skills");

    return (true);
}

bool SkillRegion::update_tip_text(std::string& tip)
{
    if (m_cursor == NO_CURSOR)
        return (false);

    unsigned int item_idx = cursor_index();
    if (item_idx >= m_items.size() || m_items[item_idx].empty())
        return (false);

    const int flag = m_items[item_idx].flag;
    std::vector<command_type> cmd;
    if (flag & TILEI_FLAG_INVALID)
        tip = "You don't have this skill yet.";
    else
    {
        const skill_type skill = (skill_type) m_items[item_idx].idx;

        tip = "[L-Click] ";
        if (you.practise_skill[skill])
            tip += "Lower the rate of training";
        tip += "Increase the rate of training";
    }

    tip += "\n[R-Click] Describe";

    return (true);
}

bool SkillRegion::update_alt_text(std::string &alt)
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

    const skill_type skill = (skill_type) idx;

    describe_info inf;
    // TODO: Nicer display for level, aptitude and crosstraining.
    inf.body << get_skill_description(skill, true);

    alt_desc_proc proc(crawl_view.msgsz.x, crawl_view.msgsz.y);
    process_description<alt_desc_proc>(proc, inf);
    proc.get_string(alt);

    return (true);
}

void SkillRegion::pack_buffers()
{
    int i = 0;
    for (int y = 0; y < my; y++)
    {
        if (i >= 32)
            break;

        for (int x = 0; x < mx; x++)
        {
            if (i++ >= 32)
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

            if (item.quantity > 0)
                draw_number(x, y, item.quantity);

            if (item.tile)
                m_buf.add_skill_tile(item.tile, x, y);

            if (item.flag & TILEI_FLAG_CURSOR)
                m_buf.add_main_tile(TILE_CURSOR, x, y);
        }
    }
}

void SkillRegion::update()
{
    m_items.clear();
    m_dirty = true;

    if (mx * my == 0)
        return;

    for (int idx = 0; idx < NUM_SKILLS; ++idx)
    {
		const skill_type skill = (skill_type) idx;

        if (skill > SK_UNARMED_COMBAT && skill < SK_SPELLCASTING)
            continue;

        InventoryTile desc;
        desc.tile     = tileidx_skill(skill,
                                      you.practise_skill[skill]);
        desc.idx      = idx;
        desc.quantity = you.skills[skill];

        if (you.skills[skill] == 0 || you.skills[skill] >= 27)
            desc.flag |= TILEI_FLAG_INVALID;

        m_items.push_back(desc);
    }
}

#endif
