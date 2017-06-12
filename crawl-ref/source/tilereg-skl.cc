#include "AppHdr.h"

#ifdef USE_TILE_LOCAL

#include "tilereg-skl.h"

#include "cio.h"
#include "libutil.h"
#include "options.h"
#include "output.h"
#include "process-desc.h"
#include "skills.h"
#include "tile-inventory-flags.h"
#include "tiledef-icons.h"
#include "tilepick.h"
#include "viewgeom.h"
#ifdef WIZARD
#include "wiz-you.h"
#endif

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

    string progress = "";

    string desc = make_stringf("%-14s Skill %4.1f Aptitude %c%d",
                               skill_name(skill),
                               you.skill(skill, 10) / 10.0,
                               apt > 0 ? '+' : ' ',
                               apt);

    draw_desc(desc.c_str());
}

int SkillRegion::handle_mouse(MouseEvent &event)
{
    unsigned int item_idx;
    if (!place_cursor(event, item_idx))
        return 0;

    const skill_type skill = (skill_type) m_items[item_idx].idx;
    if (event.button == MouseEvent::LEFT)
    {
        // TODO: Handle skill transferral using MOD_SHIFT.
#ifdef WIZARD
        if (you.wizard && (event.mod & MOD_CTRL))
        {
            wizard_set_skill_level(skill);
            return CK_MOUSE_CMD;
        }
#endif
        m_last_clicked_item = item_idx;
        if (!you.can_train[skill])
			mpr("당신은 이 기술을 수련할 수 없다.");
        else if (you.skills[skill] >= 27)
			mpr("이 스킬을 토글하는 것은 이제 아무런 의미가 없다.");
        else
        {
            tiles.set_need_redraw();
            if (Options.skill_focus == SKM_FOCUS_OFF)
                you.train[skill] = (training_status)!you.train[skill];
            else
            {
                you.train[skill] = (training_status)
                    ((you.train[skill] + 1) % NUM_TRAINING_STATUSES);
            }
            reset_training();
        }
        return CK_MOUSE_CMD;
    }
    else if (skill != NUM_SKILLS && event.button == MouseEvent::RIGHT)
    {
        describe_skill(skill);
        redraw_screen();
        return CK_MOUSE_CMD;
    }
    return 0;
}

bool SkillRegion::update_tab_tip_text(string &tip, bool active)
{
    const char *prefix = active ? "" : "[L-Click] ";

    tip = make_stringf("%s%s", prefix, "Manage skills");

    return true;
}

bool SkillRegion::update_tip_text(string& tip)
{
    if (m_cursor == NO_CURSOR)
        return false;

    unsigned int item_idx = cursor_index();
    if (item_idx >= m_items.size() || m_items[item_idx].empty())
        return false;

    const int flag = m_items[item_idx].flag;
    if (flag & TILEI_FLAG_INVALID)
        tip = "You cannot train this skill now.";
    else
    {
        const skill_type skill = (skill_type) m_items[item_idx].idx;

        tip = "[L-Click] ";
        if (you.train[skill])
            tip += "Disable training";
        else
            tip += "Enable training";
    }
#ifdef WIZARD
    if (you.wizard)
        tip += "\n[Ctrl + L-Click] Change skill level (wizmode)";
#endif

    tip += "\n[R-Click] Describe";

    return true;
}

bool SkillRegion::update_alt_text(string &alt)
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

    const skill_type skill = (skill_type) idx;

    describe_info inf;
    // TODO: Nicer display for level, aptitude and crosstraining.
    inf.body << get_skill_description(skill, true);

    alt_desc_proc proc(crawl_view.msgsz.x, crawl_view.msgsz.y);
    process_description<alt_desc_proc>(proc, inf);
    proc.get_string(alt);

    return true;
}

static int _get_aptitude_tile(const int apt)
{
    switch (apt)
    {
    case -5: return TILEI_NUM_MINUS5;
    case -4: return TILEI_NUM_MINUS4;
    case -3: return TILEI_NUM_MINUS3;
    case -2: return TILEI_NUM_MINUS2;
    case -1: return TILEI_NUM_MINUS1;
    case  1: return TILEI_NUM_PLUS1;
    case  2: return TILEI_NUM_PLUS2;
    case  3: return TILEI_NUM_PLUS3;
    case  4: return TILEI_NUM_PLUS4;
    case  5: return TILEI_NUM_PLUS5;
    case 0:
    default: return TILEI_NUM_ZERO;
    }
}

void SkillRegion::pack_buffers()
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

            InventoryTile &item = m_items[i++];
            const skill_type skill = (skill_type) item.idx;

            if (item.flag & TILEI_FLAG_INVALID)
                m_buf.add_icons_tile(TILEI_MESH, x, y);

            if (item.quantity > 0)
                draw_number(x, y, item.quantity);

            const int apt = species_apt(skill, you.species);
            m_buf.add_icons_tile(_get_aptitude_tile(apt), x, y);

            if (item.tile)
                m_buf.add_skill_tile(item.tile, x, y);

            if (item.flag & TILEI_FLAG_CURSOR)
                m_buf.add_icons_tile(TILEI_CURSOR, x, y);
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
        if (is_useless_skill(skill))
            continue;
        InventoryTile desc;
        if (you.skills[skill] >= 27)
            desc.tile = tileidx_skill(skill, TRAINING_MASTERED);
        else if (!you.training[skill])
            desc.tile = tileidx_skill(skill, TRAINING_INACTIVE);
        else
            desc.tile = tileidx_skill(skill, you.train[skill]);
        desc.idx      = idx;
        desc.quantity = you.skills[skill];

        if (!you.can_train[skill] || you.skills[skill] >= 27)
            desc.flag |= TILEI_FLAG_INVALID;

        m_items.push_back(desc);
    }
}

#endif
