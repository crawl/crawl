/*
 *  File:       tilereg-inv.cc
 *
 *  Created by: ennewalker on Sat Jan 5 01:33:53 2008 UTC
 */

#include "AppHdr.h"

#ifdef USE_TILE

#include "tilereg-inv.h"

#include "cio.h"
#include "describe.h"
#include "env.h"
#include "food.h"
#include "itemname.h"
#include "itemprop.h"
#include "items.h"
#include "item_use.h"
#include "libutil.h"
#include "macro.h"
#include "message.h"
#include "misc.h"
#include "mon-util.h"
#include "options.h"
#include "spl-book.h"
#include "stuff.h"
#include "tiledef-dngn.h"
#include "tiledef-main.h"
#include "tiles.h"
#include "viewgeom.h"

InventoryRegion::InventoryRegion(const TileRegionInit &init) : GridRegion(init)
{
}

void InventoryRegion::pack_buffers()
{
    // Pack base separately, as it comes from a different texture...
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

            if (item.flag & TILEI_FLAG_FLOOR)
            {
                if (i >= (unsigned int) mx * my)
                    break;

                int num_floor = tile_dngn_count(env.tile_default.floor);
                int tileidx = env.tile_default.floor + m_flavour[i] % num_floor;
                m_buf.add_dngn_tile(tileidx, x, y);
            }
            else
                m_buf.add_dngn_tile(TILE_ITEM_SLOT, x, y);
        }
    }

    i = 0;
    for (int y = 0; y < my; y++)
    {
        if (i >= m_items.size())
            break;

        for (int x = 0; x < mx; x++)
        {
            if (i >= m_items.size())
                break;

            InventoryTile &item = m_items[i++];

            if (item.flag & TILEI_FLAG_EQUIP)
            {
                if (item.flag & TILEI_FLAG_CURSE)
                    m_buf.add_main_tile(TILE_ITEM_SLOT_EQUIP_CURSED, x, y);
                else
                    m_buf.add_main_tile(TILE_ITEM_SLOT_EQUIP, x, y);

                if (item.flag & TILEI_FLAG_MELDED)
                    m_buf.add_main_tile(TILE_MESH, x, y);
            }
            else if (item.flag & TILEI_FLAG_CURSE)
                m_buf.add_main_tile(TILE_ITEM_SLOT_CURSED, x, y);

            if (item.flag & TILEI_FLAG_SELECT)
                m_buf.add_main_tile(TILE_ITEM_SLOT_SELECTED, x, y);

            if (item.flag & TILEI_FLAG_CURSOR)
                m_buf.add_main_tile(TILE_CURSOR, x, y);

            if (item.tile)
                m_buf.add_main_tile(item.tile, x, y);

            if (item.quantity != -1)
                draw_number(x, y, item.quantity);

            if (item.special)
                m_buf.add_main_tile(item.special, x, y, 0, 0);

            if (item.flag & TILEI_FLAG_TRIED)
                m_buf.add_main_tile(TILE_TRIED, x, y, 0, TILE_Y / 2);

            if (item.flag & TILEI_FLAG_INVALID)
                m_buf.add_main_tile(TILE_MESH, x, y);
        }
    }
}

int InventoryRegion::handle_mouse(MouseEvent &event)
{
    unsigned int item_idx;
    if (!place_cursor(event, item_idx))
        return (0);

    int idx = m_items[item_idx].idx;

    bool on_floor = m_items[item_idx].flag & TILEI_FLAG_FLOOR;

    ASSERT(idx >= 0);

    // TODO enne - this is all really only valid for the on-screen inventory
    // Do we subclass InventoryRegion for the onscreen and offscreen versions?
    char key = m_items[item_idx].key;
    if (key)
        return key;

    if (event.button == MouseEvent::LEFT)
    {
        you.last_clicked_item = item_idx;
        tiles.set_need_redraw();
        if (on_floor)
        {
            if (event.mod & MOD_SHIFT)
                tile_item_use_floor(idx);
            else
                tile_item_pickup(idx);
        }
        else
        {
            if (event.mod & MOD_SHIFT)
                tile_item_drop(idx);
            else if (event.mod & MOD_CTRL)
                tile_item_use_secondary(idx);
            else
                tile_item_use(idx);
        }
        return CK_MOUSE_CMD;
    }
    else if (event.button == MouseEvent::RIGHT)
    {
        if (on_floor)
        {
            if (event.mod & MOD_SHIFT)
            {
                you.last_clicked_item = item_idx;
                tiles.set_need_redraw();
                tile_item_eat_floor(idx);
            }
            else
            {
                describe_item(mitm[idx]);
                redraw_screen();
            }
        }
        else // in inventory
        {
            describe_item(you.inv[idx], true);
            redraw_screen();
        }
        return CK_MOUSE_CMD;
    }

    return 0;
}

// NOTE: Assumes the item is equipped in the first place!
static bool _is_true_equipped_item(item_def item)
{
    // Weapons and staves are only truly equipped if wielded.
    if (item.link == you.equip[EQ_WEAPON])
        return (item.base_type == OBJ_WEAPONS || item.base_type == OBJ_STAVES);

    // Cursed armour and rings are only truly equipped if *not* wielded.
    return (item.link != you.equip[EQ_WEAPON]);
}

// Returns whether there's any action you can take with an item in inventory
// apart from dropping it.
static bool _can_use_item(const item_def &item, bool equipped)
{
    // There's nothing you can do with an empty box if you can't unwield it.
    if (!equipped && item.sub_type == MISC_EMPTY_EBONY_CASKET)
        return (false);

    // Vampires can drain corpses.
    if (item.base_type == OBJ_CORPSES)
    {
        return (you.species == SP_VAMPIRE
                && item.sub_type != CORPSE_SKELETON
                && !food_is_rotten(item)
                && mons_has_blood(item.plus));
    }

    if (equipped && item.cursed())
    {
        // Misc. items/rods can always be evoked, cursed or not.
        if (item.base_type == OBJ_MISCELLANY || item_is_rod(item))
            return (true);

        // You can't unwield/fire a wielded cursed weapon/staff
        // but cursed armour and rings can be unwielded without problems.
        return (!_is_true_equipped_item(item));
    }

    // Mummies can't do anything with food or potions.
    if (you.species == SP_MUMMY)
        return (item.base_type != OBJ_POTIONS && item.base_type != OBJ_FOOD);

    // In all other cases you can use the item in some way.
    return (true);
}

static void _handle_wield_tip(std::string &tip, std::vector<command_type> &cmd,
                              const std::string prefix = "",
                              bool unwield = false)
{
    tip += prefix;
    if (unwield)
        tip += "Unwield (%-)";
    else
        tip += "Wield (%)";
    cmd.push_back(CMD_WIELD_WEAPON);
}

bool InventoryRegion::update_tab_tip_text(std::string &tip, bool active)
{
    const char *prefix1 = active ? "" : "[L-Click] ";
    const char *prefix2 = active ? "" : "          ";

    tip = make_stringf("%s%s\n%s%s",
                       prefix1, "Display inventory",
                       prefix2, "Use items");

    return (true);
}

bool InventoryRegion::update_tip_text(std::string& tip)
{
    if (m_cursor == NO_CURSOR)
        return (false);

    unsigned int item_idx = cursor_index();
    if (item_idx >= m_items.size() || m_items[item_idx].empty())
        return (false);

    int idx = m_items[item_idx].idx;

    // TODO enne - consider subclassing this class, rather than depending
    // on "key" to determine if this is the crt inventory or the on screen one.
    bool display_actions = (m_items[item_idx].key == 0
                    && mouse_control::current_mode() == MOUSE_MODE_COMMAND);

    // TODO enne - should the command keys here respect keymaps?
    std::vector<command_type> cmd;
    if (m_items[item_idx].flag & TILEI_FLAG_FLOOR)
    {
        const item_def &item = mitm[idx];

        if (!item.is_valid())
            return (false);

        tip = "";
        if (m_items[item_idx].key)
        {
            tip = m_items[item_idx].key;
            tip += " - ";
        }

        tip += item.name(DESC_NOCAP_A);

        if (!display_actions)
            return (true);

        tip += "\n[L-Click] Pick up (%)";
        cmd.push_back(CMD_PICKUP);
        if (item.base_type == OBJ_CORPSES
            && item.sub_type != CORPSE_SKELETON
            && !food_is_rotten(item))
        {
            tip += "\n[Shift-L-Click] ";
            if (can_bottle_blood_from_corpse(item.plus))
                tip += "Bottle blood";
            else
                tip += "Chop up";
            tip += " (%)";
            cmd.push_back(CMD_BUTCHER);

            if (you.species == SP_VAMPIRE)
            {
                tip += "\n\n[Shift-R-Click] Drink blood (e)";
                cmd.push_back(CMD_EAT);
            }
        }
        else if (item.base_type == OBJ_FOOD
                 && you.is_undead != US_UNDEAD
                 && you.species != SP_VAMPIRE)
        {
            tip += "\n[Shift-R-Click] Eat (e)";
            cmd.push_back(CMD_EAT);
        }
    }
    else
    {
        const item_def &item = you.inv[idx];
        if (!item.is_valid())
            return (false);

        tip = item.name(DESC_INVENTORY_EQUIP);

        if (!display_actions)
            return (true);

        int type = item.base_type;
        const bool equipped = m_items[item_idx].flag & TILEI_FLAG_EQUIP;
        bool wielded = (you.equip[EQ_WEAPON] == idx);

        const int EQUIP_OFFSET = NUM_OBJECT_CLASSES;

        if (_can_use_item(item, equipped))
        {
            tip += "\n[L-Click] ";
            if (equipped)
            {
                if (wielded && type != OBJ_MISCELLANY && !item_is_rod(item))
                {
                    if (type == OBJ_JEWELLERY || type == OBJ_ARMOUR
                        || type == OBJ_WEAPONS || type == OBJ_STAVES)
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
                _handle_wield_tip(tip, cmd);
                if (is_throwable(&you, item))
                {
                    tip += "\n[Ctrl-L-Click] Fire (f)";
                    cmd.push_back(CMD_FIRE);
                }
                break;
            case OBJ_WEAPONS + EQUIP_OFFSET:
                _handle_wield_tip(tip, cmd, "", true);
                if (is_throwable(&you, item))
                {
                    tip += "\n[Ctrl-L-Click] Fire (f)";
                    cmd.push_back(CMD_FIRE);
                }
                break;
            case OBJ_MISCELLANY:
                if (item.sub_type >= MISC_DECK_OF_ESCAPE
                    && item.sub_type <= MISC_DECK_OF_DEFENCE)
                {
                    _handle_wield_tip(tip, cmd);
                    break;
                }
                tip += "Evoke (V)";
                cmd.push_back(CMD_EVOKE);
                break;
            case OBJ_MISCELLANY + EQUIP_OFFSET:
                if (item.sub_type >= MISC_DECK_OF_ESCAPE
                    && item.sub_type <= MISC_DECK_OF_DEFENCE)
                {
                    tip += "Draw a card (%)\n";
                    cmd.push_back(CMD_EVOKE_WIELDED);
                    _handle_wield_tip(tip, cmd, "[Ctrl-L-Click]", true);
                    break;
                }
                // else fall-through
            case OBJ_STAVES + EQUIP_OFFSET: // rods - other staves handled above
                tip += "Evoke (%)\n";
                cmd.push_back(CMD_EVOKE_WIELDED);
                _handle_wield_tip(tip, cmd, "[Ctrl-L-Click]", true);
                break;
            case OBJ_ARMOUR:
                tip += "Wear (%)";
                cmd.push_back(CMD_WEAR_ARMOUR);
                break;
            case OBJ_ARMOUR + EQUIP_OFFSET:
                tip += "Take off (%)";
                cmd.push_back(CMD_REMOVE_ARMOUR);
                break;
            case OBJ_JEWELLERY:
                tip += "Put on (%)";
                cmd.push_back(CMD_WEAR_JEWELLERY);
                break;
            case OBJ_JEWELLERY + EQUIP_OFFSET:
                tip += "Remove (%)";
                cmd.push_back(CMD_REMOVE_JEWELLERY);
                break;
            case OBJ_MISSILES:
                tip += "Fire (%)";
                cmd.push_back(CMD_FIRE);

                if (wielded)
                    _handle_wield_tip(tip, cmd, "\n[Ctrl-L-Click]", true);
                else if (item.sub_type == MI_STONE
                            && you.has_spell(SPELL_SANDBLAST)
                         || item.sub_type == MI_ARROW
                            && you.has_spell(SPELL_STICKS_TO_SNAKES))
                {
                    // For Sandblast and Sticks to Snakes,
                    // respectively.
                    _handle_wield_tip(tip, cmd, "\n[Ctrl-L-Click]");
                }
                break;
            case OBJ_WANDS:
                tip += "Evoke (%)";
                cmd.push_back(CMD_EVOKE);
                if (wielded)
                    _handle_wield_tip(tip, cmd, "\n[Ctrl-L-Click]", true);
                break;
            case OBJ_BOOKS:
                if (item_type_known(item)
                    && item.sub_type != BOOK_MANUAL
                    && item.sub_type != BOOK_DESTRUCTION
                    && can_learn_spell(true))
                {
                    if (player_can_memorise_from_spellbook(item)
                        || has_spells_to_memorise(true))
                    {
                        tip += "Memorise (%)";
                        cmd.push_back(CMD_MEMORISE_SPELL);
                    }
                    if (wielded)
                        _handle_wield_tip(tip, cmd, "\n[Ctrl-L-Click]", true);
                    break;
                }
                // else fall-through
            case OBJ_SCROLLS:
                tip += "Read (%)";
                cmd.push_back(CMD_READ);
                if (wielded)
                    _handle_wield_tip(tip, cmd, "\n[Ctrl-L-Click]", true);
                break;
            case OBJ_POTIONS:
                tip += "Quaff (%)";
                cmd.push_back(CMD_QUAFF);
                // For Sublimation of Blood.
                if (wielded)
                    _handle_wield_tip(tip, cmd, "\n[Ctrl-L-Click]", true);
                else if (item_type_known(item)
                         && is_blood_potion(item)
                         && you.has_spell(SPELL_SUBLIMATION_OF_BLOOD))
                {
                    _handle_wield_tip(tip, cmd, "\n[Ctrl-L-Click]");
                }
                break;
            case OBJ_FOOD:
                tip += "Eat (%)";
                cmd.push_back(CMD_EAT);
                // For Sublimation of Blood.
                if (wielded)
                    _handle_wield_tip(tip, cmd, "\n[Ctrl-L-Click]", true);
                else if (item.sub_type == FOOD_CHUNK
                         && you.has_spell(SPELL_SUBLIMATION_OF_BLOOD))
                {
                    _handle_wield_tip(tip, cmd, "\n[Ctrl-L-Click]");
                }
                break;
            case OBJ_CORPSES:
                if (you.species == SP_VAMPIRE)
                {
                    tip += "Drink blood (%)";
                    cmd.push_back(CMD_EAT);
                }

                if (wielded)
                {
                    if (you.species == SP_VAMPIRE)
                        tip += "\n";
                    _handle_wield_tip(tip, cmd, "\n[Ctrl-L-Click]", true);
                }
                break;
            default:
                tip += "Use";
            }
        }

        // For Boneshards.
        // Special handling since skeletons have no primary action.
        if (item.base_type == OBJ_CORPSES
            && item.sub_type == CORPSE_SKELETON)
        {
            if (wielded)
                _handle_wield_tip(tip, cmd, "\n[Ctrl-L-Click]", true);
            else if (you.has_spell(SPELL_BONE_SHARDS))
                _handle_wield_tip(tip, cmd, "\n[Ctrl-L-Click]");
        }

        tip += "\n[R-Click] Describe";
        // Has to be non-equipped or non-cursed to drop.
        if (!equipped || !_is_true_equipped_item(you.inv[idx])
            || !you.inv[idx].cursed())
        {
            tip += "\n[Shift-L-Click] Drop (%)";
            cmd.push_back(CMD_DROP);
        }
    }

    insert_commands(tip, cmd);
    return (true);
}

bool InventoryRegion::update_alt_text(std::string &alt)
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
    const item_def *item;

    if (m_items[item_idx].flag & TILEI_FLAG_FLOOR)
        item = &mitm[idx];
    else
        item = &you.inv[idx];

    if (!item->is_valid())
        return (false);

    describe_info inf;
    get_item_desc(*item, inf, true);

    alt_desc_proc proc(crawl_view.msgsz.x, crawl_view.msgsz.y);
    process_description<alt_desc_proc>(proc, inf);

    proc.get_string(alt);

    return (true);
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

    if (floor && mitm[idx].is_valid())
        draw_desc(mitm[idx].name(DESC_PLAIN).c_str());
    else if (!floor && you.inv[idx].is_valid())
        draw_desc(you.inv[idx].name(DESC_INVENTORY_EQUIP).c_str());
}

void InventoryRegion::activate()
{
    if (inv_count() < 1)
    {
        canned_msg(MSG_NOTHING_CARRIED);
        flush_prev_message();
    }
}

static void _fill_item_info(InventoryTile &desc, const item_def &item)
{
    desc.tile = tileidx_item(item);

    int type = item.base_type;
    if (type == OBJ_FOOD || type == OBJ_SCROLLS
        || type == OBJ_POTIONS || type == OBJ_MISSILES)
    {
        // -1 specifies don't display anything
        desc.quantity = (item.quantity == 1) ? -1 : item.quantity;
    }
    else if (type == OBJ_WANDS
             && ((item.flags & ISFLAG_KNOW_PLUSES)
                 || item.plus2 == ZAPCOUNT_EMPTY))
    {
        desc.quantity = item.plus;
    }
    else if (item_is_rod(item) && item.flags & ISFLAG_KNOW_PLUSES)
        desc.quantity = item.plus / ROD_CHARGE_MULT;
    else
        desc.quantity = -1;

    if (type == OBJ_WEAPONS || type == OBJ_MISSILES || type == OBJ_ARMOUR)
        desc.special = tile_known_brand(item);
    else if (type == OBJ_CORPSES)
        desc.special = tile_corpse_brand(item);

    desc.flag = 0;
    if (item.cursed() && item_ident(item, ISFLAG_KNOW_CURSE))
        desc.flag |= TILEI_FLAG_CURSE;
    if (item_type_tried(item))
        desc.flag |= TILEI_FLAG_TRIED;
    if (item.pos.x != -1)
        desc.flag |= TILEI_FLAG_FLOOR;
}

void InventoryRegion::update()
{
    m_items.clear();
    m_dirty = true;

    if (mx * my == 0)
        return;

    // item.base_type <-> char conversion table
    const static char *obj_syms = ")([/%#?=!#+\\0}x";

    int max_pack_row = (ENDOFPACK-1) / mx + 1;
    int max_pack_items = max_pack_row * mx;

    bool inv_shown[ENDOFPACK];
    memset(inv_shown, 0, sizeof(inv_shown));

    int num_ground = 0;
    for (int i = you.visible_igrd(you.pos()); i != NON_ITEM; i = mitm[i].link)
        num_ground++;

    // If the inventory is full, show at least one row of the ground.
    int min_ground = std::min(num_ground, mx);
    max_pack_items = std::min(max_pack_items, mx * my - min_ground);
    max_pack_items = std::min(ENDOFPACK, max_pack_items);

    const size_t show_types_len = strlen(Options.tile_show_items);
    // Special case: show any type if (c == show_types_len).
    for (unsigned int c = 0; c <= show_types_len; c++)
    {
        if ((int)m_items.size() >= max_pack_items)
            break;

        bool show_any = (c == show_types_len);

        object_class_type type = OBJ_UNASSIGNED;
        if (!show_any)
        {
            const char *find = strchr(obj_syms, Options.tile_show_items[c]);
            if (!find)
                continue;
            type = (object_class_type)(find - obj_syms);
        }

        // First, normal inventory
        for (int i = 0; i < ENDOFPACK; ++i)
        {
            if ((int)m_items.size() >= max_pack_items)
                break;

            if (inv_shown[i]
                || !you.inv[i].is_valid()
                || you.inv[i].quantity == 0
                || (!show_any && you.inv[i].base_type != type))
            {
                continue;
            }

            InventoryTile desc;
            _fill_item_info(desc, you.inv[i]);
            desc.idx = i;

            for (int eq = 0; eq < NUM_EQUIP; ++eq)
            {
                if (you.equip[eq] == i)
                {
                    desc.flag |= TILEI_FLAG_EQUIP;
                    if (you.melded[eq])
                        desc.flag |= TILEI_FLAG_MELDED;
                    break;
                }
            }

            inv_shown[i] = true;
            m_items.push_back(desc);
        }
    }

    int remaining = mx*my - m_items.size();
    int empty_on_this_row = mx - m_items.size() % mx;

    // If we're not on the last row...
    if ((int)m_items.size() < mx * (my-1))
    {
        if (num_ground > remaining - empty_on_this_row)
        {
            // Fill out part of this row.
            int fill = remaining - num_ground;
            for (int i = 0; i < fill; ++i)
            {
                InventoryTile desc;
                if ((int)m_items.size() >= max_pack_items)
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
                if ((int)m_items.size() >= max_pack_items)
                    desc.flag |= TILEI_FLAG_INVALID;
                m_items.push_back(desc);
            }

            // Add extra rows, if needed.
            unsigned int ground_rows =
                std::max((num_ground-1) / mx + 1, 1);

            while ((int)(m_items.size() / mx + ground_rows) < my
                   && ((int)m_items.size()) < max_pack_items)
            {
                for (int i = 0; i < mx; i++)
                {
                    InventoryTile desc;
                    if ((int)m_items.size() >= max_pack_items)
                        desc.flag |= TILEI_FLAG_INVALID;
                    m_items.push_back(desc);
                }
            }
        }
    }

    // Then, as many ground items as we can fit.
    bool ground_shown[MAX_ITEMS];
    memset(ground_shown, 0, sizeof(ground_shown));
    for (unsigned int c = 0; c <= show_types_len; c++)
    {
        if ((int)m_items.size() >= mx * my)
            break;

        bool show_any = (c == show_types_len);

        object_class_type type = OBJ_UNASSIGNED;
        if (!show_any)
        {
            const char *find = strchr(obj_syms, Options.tile_show_items[c]);
            if (!find)
                continue;
            type = (object_class_type)(find - obj_syms);
        }

        for (int i = you.visible_igrd(you.pos()); i != NON_ITEM;
             i = mitm[i].link)
        {
            if ((int)m_items.size() >= mx * my)
                break;

            if (ground_shown[i] || !show_any && mitm[i].base_type != type)
                continue;

            InventoryTile desc;
            _fill_item_info(desc, mitm[i]);
            desc.idx = i;
            ground_shown[i] = true;

            m_items.push_back(desc);
        }
    }

    while ((int)m_items.size() < mx * my)
    {
        InventoryTile desc;
        desc.flag = TILEI_FLAG_FLOOR;
        m_items.push_back(desc);
    }
}

#endif
