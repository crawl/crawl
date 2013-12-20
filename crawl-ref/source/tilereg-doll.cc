#include "AppHdr.h"

#ifdef USE_TILE_LOCAL

#include "tilereg-doll.h"

#include "libutil.h"
#include "macro.h"
#include "player.h"
#include "tiledef-player.h"
#include "tilefont.h"
#include "tilepick-p.h"

DollEditRegion::DollEditRegion(ImageManager *im, FontWrapper *font) :
    m_font_buf(font),
    m_tile_buf(&im->m_textures[TEX_PLAYER], 17),
    m_cur_buf(&im->m_textures[TEX_PLAYER], 17)
{
    sx = sy = 0;
    dx = dy = 32;
    mx = my = 1;

    m_font = font;

    m_doll_idx = 0;
    m_cat_idx = TILEP_PART_BASE;
    m_copy_valid = false;
}

void DollEditRegion::clear()
{
    m_shape_buf.clear();
    m_tile_buf.clear();
    m_cur_buf.clear();
    m_font_buf.clear();
}

static int _get_next_part(int cat, int part, int inc)
{
    // Can't increment or decrement on show equip.
    if (part == TILEP_SHOW_EQUIP)
        return part;

    // Increment max_part by 1 to include the special value of "none".
    // (Except for the base, for which "none" is disallowed.)
    int max_part = tile_player_part_count[cat] + 1;
    int offset   = tile_player_part_start[cat];

    if (cat == TILEP_PART_BASE)
    {
        offset   = tilep_species_to_base_tile(you.species, you.experience_level);
        max_part = tile_player_count(offset);
    }

    ASSERT(inc > -max_part);

    // Translate the "none" value into something we can do modulo math with.
    if (part == 0)
    {
        part = offset;
        inc--;
    }

    // Valid part numbers are in the range [offset, offset + max_part - 1].
    int ret = (part + max_part + inc - offset) % (max_part);

    if (cat != TILEP_PART_BASE && ret == max_part - 1)
    {
        // "none" value.
        return 0;
    }
    else
    {
        // Otherwise, valid part number.
        return ret + offset;
    }
}

void DollEditRegion::render()
{
#ifdef DEBUG_TILES_REDRAW
    cprintf("rendering DollEditRegion\n");
#endif
    VColour grey(128, 128, 128, 255);

    m_cur_buf.clear();
    m_tile_buf.clear();
    m_shape_buf.clear();
    m_font_buf.clear();

    // Max items to show at once.
    const int max_show = 9;

    // Layout options (units are in 32x32 squares)
    const int left_gutter    = 2;
    const int item_line      = 2;
    const int edit_doll_line = 5;
    const int doll_line      = 8;
    const int info_offset = left_gutter + max(max_show, (int)NUM_MAX_DOLLS) + 1;

    const int center_x = left_gutter + max_show / 2;

    // Pack current doll separately so it can be drawn repeatedly.
    {
        dolls_data temp = m_dolls[m_doll_idx];
        fill_doll_equipment(temp);
        pack_doll_buf(m_cur_buf, temp, 0, 0, false, false);
    }

    // Draw set of dolls.
    for (int i = 0; i < NUM_MAX_DOLLS; i++)
    {
        int x = left_gutter + i;
        int y = doll_line;

        if (m_mode == TILEP_MODE_LOADING && m_doll_idx == i)
            m_tile_buf.add(TILEP_CURSOR, x, y);

        dolls_data temp = m_dolls[i];
        fill_doll_equipment(temp);
        pack_doll_buf(m_tile_buf, temp, x, y, false, false);

        m_shape_buf.add(x, y, x + 1, y + 1, grey);
    }

    // Draw current category of parts.
    int max_part = tile_player_part_count[m_cat_idx];
    if (m_cat_idx == TILEP_PART_BASE)
    {
        tileidx_t idx = tilep_species_to_base_tile(you.species, you.experience_level);
        max_part = tile_player_count(idx) - 1;
    }

    int show = min(max_show, max_part);
    int half_show = show / 2;
    for (int i = -half_show; i <= show - half_show; i++)
    {
        int x = center_x + i;
        int y = item_line;

        if (i == 0)
            m_tile_buf.add(TILEP_CURSOR, x, y);

        int part = _get_next_part(m_cat_idx, m_part_idx, i);
        ASSERT(part != TILEP_SHOW_EQUIP);
        if (part)
            m_tile_buf.add(part, x, y);

        m_shape_buf.add(x, y, x + 1, y + 1, grey);
    }

    m_shape_buf.add(left_gutter, edit_doll_line, left_gutter + 2, edit_doll_line + 2, grey);
    m_shape_buf.add(left_gutter + 3, edit_doll_line, left_gutter + 4, edit_doll_line + 1, grey);
    m_shape_buf.add(left_gutter + 5, edit_doll_line, left_gutter + 6, edit_doll_line + 1, grey);
    m_shape_buf.add(left_gutter + 7, edit_doll_line, left_gutter + 8, edit_doll_line + 1, grey);
    {
        // Describe the three middle tiles.
        float tile_name_x = (left_gutter + 2.7) * 32.0f;
        float tile_name_y = (edit_doll_line + 1) * 32.0f;
        m_font_buf.add("Custom", VColour::white, tile_name_x, tile_name_y);
        tile_name_x = (left_gutter + 4.7) * 32.0f;
        tile_name_y = (edit_doll_line + 1) * 32.0f;
        m_font_buf.add("Default", VColour::white, tile_name_x, tile_name_y);
        tile_name_x = (left_gutter + 7) * 32.0f;
        tile_name_y = (edit_doll_line + 1) * 32.0f;
        m_font_buf.add("Equip", VColour::white, tile_name_x, tile_name_y);
    }

    set_transform();
    m_shape_buf.draw();
    m_tile_buf.draw();

    {
        GLW_3VF trans(32 * left_gutter, 32 * edit_doll_line, 0);
        GLW_3VF scale(64, 64, 1);
        glmanager->set_transform(trans, scale);
    }

    m_cur_buf.draw();

    {
        dolls_data temp;
        temp = m_job_default;
        fill_doll_equipment(temp);
        pack_doll_buf(m_cur_buf, temp, 2, 0, false, false);

        for (unsigned int i = 0; i < TILEP_PART_MAX; ++i)
            temp.parts[i] = TILEP_SHOW_EQUIP;
        fill_doll_equipment(temp);
        pack_doll_buf(m_cur_buf, temp, 4, 0, false, false);

        if (m_mode == TILEP_MODE_LOADING)
            m_cur_buf.add(TILEP_CURSOR, 0, 0);
        else if (m_mode == TILEP_MODE_DEFAULT)
            m_cur_buf.add(TILEP_CURSOR, 2, 0);
        else if (m_mode == TILEP_MODE_EQUIP)
            m_cur_buf.add(TILEP_CURSOR, 4, 0);
    }

    {
        GLW_3VF trans(32 * (left_gutter + 3), 32 * edit_doll_line, 0);
        GLW_3VF scale(32, 32, 1);
        glmanager->set_transform(trans, scale);
    }

    m_cur_buf.draw();

    // Add text.
    const char *part_name = "(none)";
    if (m_part_idx == TILEP_SHOW_EQUIP)
        part_name = "(show equip)";
    else if (m_part_idx)
        part_name = tile_player_name(m_part_idx);

    glmanager->reset_transform();

    string item_str = part_name;
    float item_name_x = left_gutter * 32.0f;
    float item_name_y = (item_line + 1) * 32.0f;
    m_font_buf.add(item_str, VColour::white, item_name_x, item_name_y);

    string doll_name;
    doll_name = make_stringf("Doll index %d / %d", m_doll_idx, NUM_MAX_DOLLS - 1);
    float doll_name_x = left_gutter * 32.0f;
    float doll_name_y = (doll_line + 1) * 32.0f;
    m_font_buf.add(doll_name, VColour::white, doll_name_x, doll_name_y);

    const char *mode_name[TILEP_MODE_MAX] =
    {
        "Current Equipment",
        "Custom Doll",
        "Job Defaults"
    };
    doll_name = make_stringf("Doll Mode: %s", mode_name[m_mode]);
    doll_name_y += m_font->char_height() * 2.0f;
    m_font_buf.add(doll_name, VColour::white, doll_name_x, doll_name_y);

    // FIXME - this should be generated in rltiles
    const char *cat_name[TILEP_PART_MAX] =
    {
        "Base",
        "Shadow",
        "Halo",
        "Ench",
        "Cloak",
        "Boots",
        "Legs",
        "Body",
        "Gloves",
        "LHand",
        "RHand",
        "Hair",
        "Beard",
        "Helm",
        "DrcWing",
        "DrcHead"
    };

    // Add current doll information:
    string info_str;
    float info_x = info_offset * 32.0f;
    float info_y = 0.0f + m_font->char_height();

    for (int i = 0 ; i < TILEP_PART_MAX; i++)
    {
        int part = m_dolls[m_doll_idx].parts[i];
        int disp = part;
        if (disp)
            disp = disp - tile_player_part_start[i] + 1;
        int maxp = tile_player_part_count[i];

        const char *sel = (m_cat_idx == i) ? "->" : "  ";

        if (part == TILEP_SHOW_EQUIP)
            info_str = make_stringf("%2s%9s: (show equip)", sel, cat_name[i]);
        else if (!part)
            info_str = make_stringf("%2s%9s: (none)", sel, cat_name[i]);
        else
            info_str = make_stringf("%2s%9s: %3d/%3d", sel, cat_name[i], disp, maxp);
        m_font_buf.add(info_str, VColour::white, info_x, info_y);
        info_y += m_font->char_height();
    }

    // List the most important commands. (Hopefully the rest will be
    // self-explanatory.)
    {
        const int height = m_font->char_height();
        const int width  = m_font->char_width();
        const float start_y = doll_name_y + height * 3;
        const float start_x = width * 6;
        m_font_buf.add(
            "Change parts       left/right              Confirm choice      Enter",
            VColour::white, start_x, start_y);
        m_font_buf.add(
            "Change category    up/down                 Copy doll           Ctrl-C",
            VColour::white, start_x, start_y + height * 1);
        m_font_buf.add(
            "Change doll        0-9, Shift + arrows     Paste copied doll   Ctrl-V",
            VColour::white, start_x, start_y + height * 2);
        m_font_buf.add(
            "Change doll mode   m                       Randomise doll      Ctrl-R",
            VColour::white, start_x, start_y + height * 3);
        m_font_buf.add(
            "Save menu          Escape, Ctrl-S          Toggle equipment    *",
            VColour::white, start_x, start_y + height * 4);
        m_font_buf.add(
            "Quit menu          q, Ctrl-Q",
            VColour::white, start_x, start_y + height * 5);
    }

    m_font_buf.draw();
}

int DollEditRegion::handle_mouse(MouseEvent &event)
{
    return 0;
}

void DollEditRegion::run()
{
    // Initialise equipment setting.
    dolls_data equip_doll;
    for (unsigned int i = 0; i < TILEP_PART_MAX; ++i)
         equip_doll.parts[i] = TILEP_SHOW_EQUIP;

    // Initialise job default.
    m_job_default = equip_doll;
    tilep_race_default(you.species, you.experience_level, &m_job_default);
    tilep_job_default(you.char_class, &m_job_default);

    // Read predefined dolls from file.
    for (unsigned int i = 0; i < NUM_MAX_DOLLS; ++i)
         m_dolls[i] = equip_doll;

    m_mode = TILEP_MODE_LOADING;
    m_doll_idx = -1;

    if (!load_doll_data("dolls.txt", m_dolls, NUM_MAX_DOLLS,
                        &m_mode, &m_doll_idx))
    {
        m_doll_idx = 0;
    }

    bool update_part_idx = true;

    command_type cmd;
    do
    {
        if (update_part_idx)
        {
            m_part_idx = m_dolls[m_doll_idx].parts[m_cat_idx];
            if (m_part_idx == TILEP_SHOW_EQUIP)
                m_part_idx = 0;
            update_part_idx = false;
        }

        int key = getchm(KMC_DOLL);
        cmd = key_to_command(key, KMC_DOLL);

        switch (cmd)
        {
        case CMD_DOLL_QUIT:
            return;
        case CMD_DOLL_RANDOMIZE:
            create_random_doll(m_dolls[m_doll_idx]);
            break;
        case CMD_DOLL_SELECT_NEXT_DOLL:
        case CMD_DOLL_SELECT_PREV_DOLL:
        {
            const int bonus = (cmd == CMD_DOLL_SELECT_NEXT_DOLL ? 1
                                        : NUM_MAX_DOLLS - 1);

            m_doll_idx = (m_doll_idx + bonus) % NUM_MAX_DOLLS;
            update_part_idx = true;
            if (m_mode != TILEP_MODE_LOADING)
                m_mode = TILEP_MODE_LOADING;
            break;
        }
        case CMD_DOLL_SELECT_NEXT_PART:
        case CMD_DOLL_SELECT_PREV_PART:
        {
            const int bonus = (cmd == CMD_DOLL_SELECT_NEXT_PART ? 1
                                        : TILEP_PART_MAX - 1);

            m_cat_idx = (m_cat_idx + bonus) % TILEP_PART_MAX;
            update_part_idx = true;
            break;
        }
        case CMD_DOLL_CHANGE_PART_NEXT:
        case CMD_DOLL_CHANGE_PART_PREV:
            if (m_part_idx != TILEP_SHOW_EQUIP)
            {
                const int dir = (cmd == CMD_DOLL_CHANGE_PART_NEXT ? 1 : -1);
                m_part_idx = _get_next_part(m_cat_idx, m_part_idx, dir);
            }
            m_dolls[m_doll_idx].parts[m_cat_idx] = m_part_idx;
            break;
        case CMD_DOLL_TOGGLE_EQUIP:
            if (m_dolls[m_doll_idx].parts[m_cat_idx] == TILEP_SHOW_EQUIP)
                m_dolls[m_doll_idx].parts[m_cat_idx] = m_part_idx;
            else
                m_dolls[m_doll_idx].parts[m_cat_idx] = TILEP_SHOW_EQUIP;
            break;
        case CMD_DOLL_CONFIRM_CHOICE:
            if (m_mode != TILEP_MODE_LOADING)
                m_mode = TILEP_MODE_LOADING;
            break;
        case CMD_DOLL_COPY:
            m_doll_copy  = m_dolls[m_doll_idx];
            m_copy_valid = true;
            break;
        case CMD_DOLL_PASTE:
            if (m_copy_valid)
                m_dolls[m_doll_idx] = m_doll_copy;
            break;
        case CMD_DOLL_TAKE_OFF:
            m_part_idx = 0;
            m_dolls[m_doll_idx].parts[m_cat_idx] = 0;
            break;
        case CMD_DOLL_TAKE_OFF_ALL:
            for (int i = 0; i < TILEP_PART_MAX; i++)
            {
                switch (i)
                {
                case TILEP_PART_BASE:
                case TILEP_PART_SHADOW:
                case TILEP_PART_HALO:
                case TILEP_PART_ENCH:
                case TILEP_PART_DRCWING:
                case TILEP_PART_DRCHEAD:
                    break;
                default:
                    m_dolls[m_doll_idx].parts[i] = 0;
                };
            }
            break;
        case CMD_DOLL_TOGGLE_EQUIP_ALL:
            for (int i = 0; i < TILEP_PART_MAX; i++)
                m_dolls[m_doll_idx].parts[i] = TILEP_SHOW_EQUIP;
            break;
        case CMD_DOLL_JOB_DEFAULT:
            m_dolls[m_doll_idx] = m_job_default;
            break;
        case CMD_DOLL_CHANGE_MODE:
            m_mode = (tile_doll_mode)(((int)m_mode + 1) % TILEP_MODE_MAX);
        default:
            if (key == '0')
                m_doll_idx = 0;
            else if (key >= '1' && key <= '9')
                m_doll_idx = key - '1' + 1;
            else
                break;

            if (m_mode != TILEP_MODE_LOADING)
                m_mode = TILEP_MODE_LOADING;
            ASSERT(m_doll_idx < NUM_MAX_DOLLS);
            break;
        }
    }
    while (cmd != CMD_DOLL_SAVE);

    save_doll_data(m_mode, m_doll_idx, &m_dolls[0]);

    // Update player with the current doll.
    switch (m_mode)
    {
    case TILEP_MODE_LOADING:
        player_doll = m_dolls[m_doll_idx];
        break;
    case TILEP_MODE_DEFAULT:
        player_doll = m_job_default;
        break;
    default:
    case TILEP_MODE_EQUIP:
        player_doll = equip_doll;
    }
}

#endif
