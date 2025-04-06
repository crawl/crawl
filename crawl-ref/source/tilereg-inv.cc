#include "AppHdr.h"

#ifdef USE_TILE_LOCAL

#include "tilereg-inv.h"

#include "cio.h"
#include "describe.h"
#include "env.h"
#include "evoke.h"
#include "tile-env.h"
#include "invent.h"
#include "item-name.h"
#include "item-prop.h"
#include "items.h"
#include "item-status-flag-type.h"
#include "item-use.h"
#include "libutil.h"
#include "macro.h"
#include "message.h"
#include "options.h"
#include "output.h"
#include "spl-book.h"
#include "stringutil.h"
#include "terrain.h"
#include "tile-inventory-flags.h"
#include "rltiles/tiledef-dngn.h"
#include "rltiles/tiledef-icons.h"
#include "rltiles/tiledef-icons.h"
#include "rltiles/tiledef-main.h"
#include "tag-version.h"
#include "tilepick.h"
#include "unicode.h"

InventoryRegion::InventoryRegion(const TileRegionInit &init) : GridRegion(init)
{
}

void InventoryRegion::pack_buffers()
{
    unsigned int i = 0 + (m_grid_page*mx*my - m_grid_page*2); // this has to match the logic in cursor_index()
    for (int y = 0; y < my; y++)
    {
        for (int x = 0; x < mx; x++)
        {
            if (i >= m_items.size())
                break;

            InventoryTile &item = m_items[i++];

            if (item.flag & TILEI_FLAG_FLOOR)
            {
                if (i > (unsigned int) mx * my * (m_grid_page+1) && item.tile)
                    break;

                int num_floor = tile_dngn_count(tile_env.default_flavour.floor);
                tileidx_t t = tile_env.default_flavour.floor + i % num_floor;
                m_buf.add_dngn_tile(t, x, y);
            }
            else if (!tiles.is_using_small_layout())
                m_buf.add_main_tile(TILE_ITEM_SLOT, x, y);
        }
    }

    i = 0 + (m_grid_page*mx*my - m_grid_page*2); // this also has to match the logic in cursor_index()
    for (int y = 0; y < my; y++)
    {
        for (int x = 0; x < mx; x++)
        {
            if (i >= m_items.size())
                break;

            InventoryTile &item = m_items[i++];

            if (_is_next_button(i-1))
            {
                // continuation to next page icon
                m_buf.add_main_tile(TILE_UNSEEN_ITEM, x, y);
                continue;
            }
            else if (y==0 && x==0 && m_grid_page>0)
            {
                // previous page icon
                m_buf.add_main_tile(TILE_UNSEEN_ITEM, x, y);
                continue;
            }

            if (item.flag & TILEI_FLAG_EQUIP)
            {
                if (item.flag & TILEI_FLAG_CURSE)
                    m_buf.add_main_tile(TILE_ITEM_SLOT_EQUIP_CURSED, x, y);
                else
                    m_buf.add_main_tile(TILE_ITEM_SLOT_EQUIP, x, y);

                if (item.flag & TILEI_FLAG_MELDED)
                    m_buf.add_icons_tile(TILEI_MESH, x, y);
            }
            else if (item.flag & TILEI_FLAG_CURSE)
                m_buf.add_main_tile(TILE_ITEM_SLOT_CURSED, x, y);

            if (item.flag & TILEI_FLAG_SELECT)
                m_buf.add_icons_tile(TILEI_ITEM_SLOT_SELECTED, x, y);

            if (item.flag & TILEI_FLAG_CURSOR)
                m_buf.add_icons_tile(TILEI_CURSOR, x, y);

            if (item.flag & TILEI_FLAG_CHAOTIC)
                m_buf.add_special_tile(TILE_MODIFIER_CHAOTIC, x, y, 0, 0);

            if (item.tile)
                m_buf.add_main_tile(item.tile, x, y);

            if (item.quantity != -1)
                draw_number(x, y, item.quantity);

            if (item.special)
                m_buf.add_special_tile(item.special, x, y, 0, 0);

            if (item.flag & TILEI_FLAG_INVALID && !tiles.is_using_small_layout())
                m_buf.add_icons_tile(TILEI_MESH, x, y);
        }
    }
}

int InventoryRegion::handle_mouse(wm_mouse_event &event)
{
    unsigned int item_idx;
    if (!place_cursor(event, item_idx))
        return 0;

    // handle paging
    if (_is_next_button(cursor_index()) && event.button==wm_mouse_event::LEFT)
    {
        // next page
        m_grid_page++;
        update();
        return CK_NO_KEY;
    }
    else if (m_cursor.x==0 && m_cursor.y==0 && m_grid_page>0 && event.button==wm_mouse_event::LEFT)
    {
        // prev page
        m_grid_page--;
        update();
        return CK_NO_KEY;
    }

    if (tiles.get_map_display())
        return CK_NO_KEY;

    int idx = m_items[item_idx].idx;

    bool on_floor = m_items[item_idx].flag & TILEI_FLAG_FLOOR;

    ASSERT(idx >= 0);

    // TODO enne - this is all really only valid for the on-screen inventory
    // Do we subclass InventoryRegion for the onscreen and offscreen versions?
    char key = m_items[item_idx].key;
    if (key)
        return key;

    if (event.button == wm_mouse_event::LEFT)
    {
        m_last_clicked_item = item_idx;
        tiles.set_need_redraw();
        if (on_floor)
            tile_item_pickup(idx, (event.mod & TILES_MOD_CTRL));
        else
        {
            if (event.mod & TILES_MOD_SHIFT)
                tile_item_drop(idx, (event.mod & TILES_MOD_CTRL));
            else
                tile_item_use(idx);
        }
        update();
        return CK_MOUSE_CMD;
    }
    else if (event.button == wm_mouse_event::RIGHT)
    {
        if (on_floor)
        {
            describe_item(env.item[idx]);
            redraw_screen();
            update_screen();
        }
        else // in inventory
        {
            describe_item(you.inv[idx]);
            redraw_screen();
            update_screen();
        }
        return CK_MOUSE_CMD;
    }

    return 0;
}

// Returns whether there's any action you can take with an item in inventory
// apart from dropping it.
static bool _can_use_item(const item_def &item, bool equipped)
{
#if TAG_MAJOR_VERSION == 34
    // There's nothing you can do with an empty box if you can't unwield it.
    if (!equipped && item.sub_type == MISC_BUGGY_EBONY_CASKET)
        return false;
#endif

    if (equipped && item.cursed())
        return false; // stuck!

    if (!you.can_drink())
        return item.base_type != OBJ_POTIONS;

    // In all other cases you can use the item in some way.
    return true;
}

bool InventoryRegion::update_tab_tip_text(string &tip, bool active)
{
    const char *prefix1 = active ? "" : "[L-Click] ";
    const char *prefix2 = active ? "" : "          ";

    tip = make_stringf("%s%s\n%s%s",
                       prefix1, "Display inventory",
                       prefix2, "Use items");

    return true;
}

bool InventoryRegion::update_tip_text(string& tip)
{
    if (m_cursor == NO_CURSOR)
        return false;

    unsigned int item_idx = cursor_index();
    if (item_idx >= m_items.size() || m_items[item_idx].empty())
        return false;

    // page next/prev
    if (_is_next_button(item_idx))
    {
        tip = "Next page\n[L-Click] Show next page of items";
        return true;
    }
    else if (_is_prev_button(item_idx))
    {
        tip = "Previous page\n[L-Click] Show previous page of items";
        return true;
    }

    int idx = m_items[item_idx].idx;

    // TODO enne - consider subclassing this class, rather than depending
    // on "key" to determine if this is the crt inventory or the on screen one.
    bool display_actions = (m_items[item_idx].key == 0
                    && mouse_control::current_mode() == MOUSE_MODE_COMMAND);

    // TODO enne - should the command keys here respect keymaps?
    vector<command_type> cmd;
    if (m_items[item_idx].flag & TILEI_FLAG_FLOOR)
    {
        const item_def &item = env.item[idx];

        if (!item.defined())
            return false;

        tip = "";
        if (m_items[item_idx].key)
        {
            tip = m_items[item_idx].key;
            tip += " - ";
        }

        tip += item.name(DESC_A);

        if (!display_actions)
            return true;

        if (item_is_stationary_net(item))
        {
            actor *trapped = actor_at(item.pos);
            tip += make_stringf(" (holding %s)",
                            trapped ? trapped->name(DESC_A).c_str()
                                    : "nobody"); // buggy net, but don't crash
        }

        if (!item_is_stationary(item))
        {
            tip += "\n[L-Click] Pick up (%)";
            cmd.push_back(CMD_PICKUP);
            if (item.quantity > 1)
            {
                tip += "\n[Ctrl + L-Click] Partial pick up (%)";
                cmd.push_back(CMD_PICKUP_QUANTITY);
            }
        }
    }
    else
    {
        const item_def &item = you.inv[idx];
        if (!item.defined())
            return false;

        tip = item.name(DESC_INVENTORY_EQUIP);

        if (!display_actions)
            return true;

        int type = item.base_type;
        const bool equipped = m_items[item_idx].flag & TILEI_FLAG_EQUIP;
        bool wielded = (&item == you.weapon());

        const int EQUIP_OFFSET = NUM_OBJECT_CLASSES;

        if (_can_use_item(item, equipped))
        {
            string tip_prefix = "\n[L-Click] ";
            string tmp = "";
            if (equipped)
            {
                if (wielded)
                {
                    if (type == OBJ_JEWELLERY || type == OBJ_ARMOUR // ???
                        || is_weapon(item))
                    {
                        type = OBJ_WEAPONS + EQUIP_OFFSET;
                    }
                }
                else
                    type += EQUIP_OFFSET;
            }

            switch (type)
            {
            // first equipable categories
            case OBJ_WEAPONS:
            case OBJ_STAVES:
                if (!you.has_mutation(MUT_NO_GRASPING))
                {
                    tmp += "Wield (%)";
                    cmd.push_back(CMD_WIELD_WEAPON);
                    if (you.has_mutation(MUT_WIELD_OFFHAND)
                        && you.hands_reqd(item) == HANDS_ONE)
                    {
                        tmp += "\n[Ctrl + L-Click] Offhand";
                    }
                }
                break;
            case OBJ_WEAPONS + EQUIP_OFFSET:
                tmp += "Unwield (%-)";
                cmd.push_back(CMD_WIELD_WEAPON);
                break;
            case OBJ_MISCELLANY:
            case OBJ_WANDS:
            case OBJ_TALISMANS:
                tmp += "Evoke (%)";
                cmd.push_back(CMD_EVOKE);
                break;
            case OBJ_ARMOUR:
                if (!you.has_mutation(MUT_NO_ARMOUR))
                {
                    tmp += "Wear (%)";
                    cmd.push_back(CMD_WEAR_ARMOUR);
                }
                break;
            case OBJ_ARMOUR + EQUIP_OFFSET:
                tmp += "Take off (%)";
                cmd.push_back(CMD_REMOVE_ARMOUR);
                break;
            case OBJ_JEWELLERY:
                tmp += "Put on (%)";
                cmd.push_back(CMD_WEAR_JEWELLERY);
                break;
            case OBJ_JEWELLERY + EQUIP_OFFSET:
                tmp += "Remove (%)";
                cmd.push_back(CMD_REMOVE_JEWELLERY);
                break;
            case OBJ_MISSILES:
                if (!you.has_mutation(MUT_NO_GRASPING))
                {
                    tmp += "Fire (%)";
                    cmd.push_back(CMD_FIRE);
                }
                break;
            case OBJ_SCROLLS:
                tmp += "Read (%)";
                cmd.push_back(CMD_READ);
                break;
            case OBJ_POTIONS:
                tmp += "Quaff (%)";
                cmd.push_back(CMD_QUAFF);
                break;
            case OBJ_CORPSES:
            case OBJ_BOOKS:
                break;
            default:
                tmp += "Use";
            }

            if (!tmp.empty())
                tip += tip_prefix + tmp;
        }

        tip += "\n[R-Click] Describe";
        // Has to be non-equipped or non-cursed to drop.
        if (!equipped || !you.inv[idx].cursed())
        {
            tip += "\n[Shift + L-Click] Drop (%)";
            cmd.push_back(CMD_DROP);
            if (you.inv[idx].quantity > 1)
            {
                tip += "\n[Ctrl-Shift + L-Click] Drop quantity (%#)";
                cmd.push_back(CMD_DROP);
            }
        }
    }

    insert_commands(tip, cmd);
    return true;
}

bool InventoryRegion::update_alt_text(string &alt)
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
    const item_def *item;

    if (m_items[item_idx].flag & TILEI_FLAG_FLOOR)
        item = &env.item[idx];
    else
        item = &you.inv[idx];

    if (!item->defined())
        return false;

    describe_info inf;
    if (_is_next_button(item_idx))
    {
        // alt text for next page button
        inf.title = "Next page";
    }
    else if (_is_prev_button(item_idx))
    {
        // alt text for prev page button
        inf.title = "Previous page";
    }
    else
        get_item_desc(*item, inf);

    alt = process_description(inf);
    return true;
}

void InventoryRegion::draw_tag()
{
    if (m_cursor == NO_CURSOR)
        return;
    int curs_index = cursor_index();
    if (curs_index >= (int)m_items.size())
        return;
    int idx = m_items[curs_index].idx;
    if (idx == -1)
        return;

    bool floor = m_items[curs_index].flag & TILEI_FLAG_FLOOR;

    if (_is_next_button(curs_index))
        draw_desc("Next page");
    else if (_is_prev_button(curs_index))
        draw_desc("Previous page");
    else if (floor && env.item[idx].defined())
        draw_desc(env.item[idx].name(DESC_PLAIN).c_str());
    else if (!floor && you.inv[idx].defined())
        draw_desc(you.inv[idx].name(DESC_INVENTORY_EQUIP).c_str());
}

void InventoryRegion::activate()
{
    if (inv_count() < 1 && you.num_turns > 0)
    {
        canned_msg(MSG_NOTHING_CARRIED);
        flush_prev_message();
    }

    // switch to first page on activation
    m_grid_page = 0;
}

static void _fill_item_info(InventoryTile &desc, const item_def &item)
{
    desc.tile = tileidx_item(item);

    int type = item.base_type;
    if (is_stackable_item(item))
    {
        // -1 specifies don't display anything
        desc.quantity = (item.quantity == 1) ? -1 : item.quantity;
    }
    else if (type == OBJ_WANDS && item.is_identified())
        desc.quantity = item.charges;
    else
        desc.quantity = -1;

    if (type == OBJ_WEAPONS || type == OBJ_MISSILES
        || type == OBJ_ARMOUR || item.base_type == OBJ_STAVES
#if TAG_MAJOR_VERSION == 34
        || type == OBJ_RODS
#endif
       )
    {
        desc.special = tileidx_known_brand(item);
    }

    desc.flag = 0;
    if (item.cursed())
        desc.flag |= TILEI_FLAG_CURSE;
    if (item.pos.x != -1)
        desc.flag |= TILEI_FLAG_FLOOR;
    if (testbits(item.flags, ISFLAG_CHAOTIC))
        desc.flag |= TILEI_FLAG_CHAOTIC;
}

void InventoryRegion::update()
{
    m_items.clear();
    m_dirty = true;

    if (mx * my == 0)
        return;

    // XX in principle this should check specific commands in
    // tile_command_not_applicable
    const bool disable_all = tiles.get_map_display();

    const int max_pack_items = ENDOFPACK;

    bool inv_shown[ENDOFPACK];
    memset(inv_shown, 0, sizeof(inv_shown));

    int num_ground = 0;
    for (int i = you.visible_igrd(you.pos()); i != NON_ITEM; i = env.item[i].link)
        num_ground++;

    char32_t c;
    const char *tp = Options.tile_show_items.c_str();
    int s;
    do // Do one last iteration with the 0 char at the end.
    {
        tp += s = utf8towc(&c, tp); // could be better to store this pre-parsed

        if ((int)m_items.size() >= max_pack_items)
            break;

        bool show_any = !c;
        object_class_type type = item_class_by_sym(c);

        // First, normal inventory
        for (int i = 0; i < ENDOFPACK; ++i)
        {
            if ((int)m_items.size() >= max_pack_items)
                break;

            if (inv_shown[i]
                || !you.inv[i].defined()
                || you.inv[i].quantity == 0
                || (!show_any && you.inv[i].base_type != type))
            {
                continue;
            }

            InventoryTile desc;
            _fill_item_info(desc, you.inv[i]);
            desc.idx = i;
            if (disable_all)
                desc.flag |= TILEI_FLAG_INVALID;

            for (const auto& entry : you.equipment.items)
            {
                if (entry.item == i)
                {
                    desc.flag |= TILEI_FLAG_EQUIP;
                    if (entry.melded)
                        desc.flag |= TILEI_FLAG_MELDED;
                    break;
                }
            }

            // Mark our activate talisman as though it were equipped (at least
            // as long as it's in our inventory).
            if (you.inv[i].base_type == OBJ_TALISMANS
                && you.using_talisman(you.inv[i]))
            {
                desc.flag |= TILEI_FLAG_EQUIP;
                if (you.form != you.default_form)
                    desc.flag |= TILEI_FLAG_MELDED;
            }

            inv_shown[i] = true;
            m_items.push_back(desc);
        }
    } while (s);

    // ensure we don't end up stuck on a later page.
    const int total_items = m_items.size() + num_ground;
    // can store mx*my items on first page, but lose 2 for each following page
    // (next & prev buttons)
    const int max_page = max(0, (total_items - 2)) / (mx*my - 2);
    m_grid_page = min(m_grid_page, max_page);

    const int remaining = mx*my - m_items.size();
    const int empty_on_this_row = mx - m_items.size() % mx;

    // If we're not on the last row...
    if ((int)m_items.size() < mx * (my-1))
        // let's deliberately not do this on page 2
    {
        if (num_ground > remaining - empty_on_this_row)
        {
            // Fill out part of this row.
            const int fill = remaining - num_ground;
            for (int i = 0; i < fill; ++i)
            {
                InventoryTile desc;
                if (disable_all || (int)m_items.size() >= max_pack_items)
                    desc.flag |= TILEI_FLAG_INVALID;
                m_items.push_back(desc);
            }
        }
        else
        {
            // Fill out the rest of this row.
            while (m_items.size() % mx != 0)
            {
                InventoryTile desc;
                if (disable_all || (int)m_items.size() >= max_pack_items)
                    desc.flag |= TILEI_FLAG_INVALID;
                m_items.push_back(desc);
            }

            // Add extra rows, if needed.
            unsigned int ground_rows = max((num_ground-1) / mx + 1, 1);

            while ((int)(m_items.size() / mx + ground_rows) < my
                   && ((int)m_items.size()) < max_pack_items)
            {
                for (int i = 0; i < mx; i++)
                {
                    InventoryTile desc;
                    if (disable_all || (int)m_items.size() >= max_pack_items)
                        desc.flag |= TILEI_FLAG_INVALID;
                    m_items.push_back(desc);
                }
            }
        }
        if (tiles.is_using_small_layout())
        {
            // Leave only one row of floor items for small layout
            while ((int)m_items.size() < mx * (my-1))
            {
                InventoryTile desc;
                m_items.push_back(desc);
            }
        }
    }

    // Then, as many ground items as we can fit.
    bool ground_shown[MAX_ITEMS];
    memset(ground_shown, 0, sizeof(ground_shown));

    tp = Options.tile_show_items.c_str();
    do
    {
        tp += s = utf8towc(&c, tp);

        if ((int)m_items.size() >= mx * my * (m_grid_page+1))
            break;

        bool show_any = !c;
        object_class_type type = item_class_by_sym(c);

        for (int i = you.visible_igrd(you.pos()); i != NON_ITEM;
             i = env.item[i].link)
        {
            if ((int)m_items.size() >= mx * my * (m_grid_page+1))
                break;

            if (ground_shown[i] || !show_any && env.item[i].base_type != type)
                continue;

            InventoryTile desc;
            _fill_item_info(desc, env.item[i]);
            desc.idx = i;
            ground_shown[i] = true;
            if (disable_all)
                desc.flag |= TILEI_FLAG_INVALID;

            m_items.push_back(desc);
        }
    } while (s);

    if (!tiles.is_using_small_layout())
    {
        while ((int)m_items.size() < mx * my)
            // let's not do this for p2 either
        {
            InventoryTile desc;
            desc.flag = TILEI_FLAG_FLOOR;
            if (disable_all)
                desc.flag |= TILEI_FLAG_INVALID;
            m_items.push_back(desc);
        }
    }
}

bool InventoryRegion::_is_prev_button(int idx)
{
    // idx is an index in m_items as returned by cursor_index()
    return m_grid_page>0 && idx == mx*my*m_grid_page-2*m_grid_page;
}

/**
 * How many items are we actually looking at (inv+floor), not counting fake
 * padding items inserted on the first page?
 *
 * Only valid for page 1.
 */
int InventoryRegion::_real_item_count()
{
    // xxx: this seems like a classic reduce()...
    int total = 0;
    for (auto desc : m_items)
        if (desc.idx != -1)
            ++total;
    return total;
}

bool InventoryRegion::_is_next_button(int idx)
{
    // idx is an index in m_items as returned by cursor_index()
    return idx == (mx*my - 2)*(m_grid_page+1) - 1 + 2
            && _real_item_count() >= mx*my;
}
#endif
