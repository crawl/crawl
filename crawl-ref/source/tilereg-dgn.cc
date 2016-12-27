#include "AppHdr.h"

#ifdef USE_TILE_LOCAL

#include "tilereg-dgn.h"

#include <algorithm> // any_of

#include "cio.h"
#include "cloud.h"
#include "command.h"
#include "coord.h"
#include "directn.h"
#include "dgn-height.h"
#include "env.h"
#include "invent.h"
#include "item-prop.h"
#include "items.h"
#include "jobs.h"
#include "libutil.h"
#include "macro.h"
#include "message.h"
#include "mon-util.h"
#include "nearby-danger.h"
#include "options.h"
#include "output.h"
#include "process-desc.h"
#include "prompt.h"
#include "religion.h"
#include "spl-cast.h"
#include "spl-zap.h"
#include "stash.h"
#include "terrain.h"
#include "tiledef-dngn.h"
#include "tiledef-icons.h"
#include "tiledef-main.h"
#include "tilefont.h"
#include "tilepick.h"
#include "traps.h"
#include "travel.h"
#include "viewgeom.h"

static VColour _flash_colours[NUM_TERM_COLOURS] =
{
    VColour(  0,   0,   0,   0), // BLACK (transparent)
    VColour(  0,   0, 128, 100), // BLUE
    VColour(  0, 128,   0, 100), // GREEN
    VColour(  0, 128, 128, 100), // CYAN
    VColour(128,   0,   0, 100), // RED
    VColour(150,   0, 150, 100), // MAGENTA
    VColour(165,  91,   0, 100), // BROWN
    VColour( 50,  50,  50, 150), // LIGHTGRAY
    VColour(  0,   0,   0, 150), // DARKGRAY
    VColour( 64,  64, 255, 100), // LIGHTBLUE
    VColour( 64, 255,  64, 100), // LIGHTGREEN
    VColour(  0, 255, 255, 100), // LIGHTCYAN
    VColour(255,  64,  64, 100), // LIGHTRED
    VColour(255,  64, 255, 100), // LIGHTMAGENTA
    VColour(150, 150,   0, 100), // YELLOW
    VColour(255, 255, 255, 100), // WHITE
};

DungeonRegion::DungeonRegion(const TileRegionInit &init) :
    TileRegion(init),
    m_cx_to_gx(0),
    m_cy_to_gy(0),
    m_last_clicked_grid(coord_def()),
    m_buf_dngn(init.im)
{
    for (int i = 0; i < CURSOR_MAX; i++)
        m_cursor[i] = NO_CURSOR;
}

DungeonRegion::~DungeonRegion()
{
}

void DungeonRegion::load_dungeon(const crawl_view_buffer &vbuf,
                                 const coord_def &gc)
{
    m_dirty = true;

    m_cx_to_gx = gc.x - mx / 2;
    m_cy_to_gy = gc.y - my / 2;

    m_vbuf = vbuf;

    place_cursor(CURSOR_TUTORIAL, m_cursor[CURSOR_TUTORIAL]);
}

void DungeonRegion::pack_cursor(cursor_type type, unsigned int tile)
{
    const coord_def &gc = m_cursor[type];
    if (gc == NO_CURSOR || !on_screen(gc))
        return;

    const coord_def ep(gc.x - m_cx_to_gx, gc.y - m_cy_to_gy);
    m_buf_dngn.add_icons_tile(tile, ep.x, ep.y);
}

void DungeonRegion::pack_buffers()
{
    m_buf_dngn.clear();
    m_buf_flash.clear();

    if (m_vbuf.empty())
        return;

    screen_cell_t *vbuf_cell = m_vbuf;
    for (int y = 0; y < crawl_view.viewsz.y; ++y)
        for (int x = 0; x < crawl_view.viewsz.x; ++x)
        {
            coord_def gc(x + m_cx_to_gx, y + m_cy_to_gy);

            packed_cell tile_cell = packed_cell(vbuf_cell->tile);
            if (map_bounds(gc))
            {
                tile_cell.flv = env.tile_flv(gc);
                pack_cell_overlays(gc, &tile_cell);
            }
            else
            {
                tile_cell.flv.floor   = 0;
                tile_cell.flv.wall    = 0;
                tile_cell.flv.special = 0;
                tile_cell.flv.feat    = 0;
            }

            m_buf_dngn.add(tile_cell, x, y);

            const int fcol = vbuf_cell->flash_colour;
            if (fcol)
                m_buf_flash.add(x, y, x + 1, y + 1, _flash_colours[fcol]);

            vbuf_cell++;
        }

    pack_cursor(CURSOR_TUTORIAL, TILEI_TUTORIAL_CURSOR);
    const bool mouse_curs_vis = you.see_cell(m_cursor[CURSOR_MOUSE]);
    pack_cursor(CURSOR_MOUSE, mouse_curs_vis ? TILEI_CURSOR : TILEI_CURSOR2);
    pack_cursor(CURSOR_MAP, TILEI_CURSOR);

    if (m_cursor[CURSOR_TUTORIAL] != NO_CURSOR
        && on_screen(m_cursor[CURSOR_TUTORIAL]))
    {
        m_buf_dngn.add_main_tile(TILEI_TUTORIAL_CURSOR,
                                 m_cursor[CURSOR_TUTORIAL].x,
                                 m_cursor[CURSOR_TUTORIAL].y);
    }

    for (const tile_overlay &overlay : m_overlays)
    {
        // overlays must be from the main image and must be in LOS.
        if (!crawl_view.in_los_bounds_g(overlay.gc))
            continue;

        tileidx_t idx = overlay.idx;
        if (idx >= TILE_MAIN_MAX)
            continue;

        const coord_def ep(overlay.gc.x - m_cx_to_gx,
                           overlay.gc.y - m_cy_to_gy);
        m_buf_dngn.add_main_tile(idx, ep.x, ep.y);
    }
}

struct tag_def
{
    tag_def() { text = nullptr; left = right = 0; }

    const char* text;
    char left, right;
    char type;
};

// #define DEBUG_TILES_REDRAW
void DungeonRegion::render()
{
#ifdef DEBUG_TILES_REDRAW
    cprintf("rendering DungeonRegion\n");
#endif
    if (m_dirty)
    {
        pack_buffers();
        m_dirty = false;
    }

    set_transform();
    m_buf_dngn.draw();
    draw_minibars();
    m_buf_flash.draw();

    FixedArray<tag_def, ENV_SHOW_DIAMETER, ENV_SHOW_DIAMETER> tag_show;

    int total_tags = 0;

    for (int t = TAG_MAX - 1; t >= 0; t--)
    {
        for (const TextTag &tag : m_tags[t])
        {
            if (!crawl_view.in_los_bounds_g(tag.gc))
                continue;

            const coord_def ep = grid2show(tag.gc);

            if (tag_show(ep).text)
                continue;

            const char *str = tag.tag.c_str();

            int width    = m_tag_font->string_width(str);
            tag_def &def = tag_show(ep);

            const int buffer = 2;

            def.left  = -width / 2 - buffer;
            def.right =  width / 2 + buffer;
            def.text  = str;
            def.type  = t;

            total_tags++;
        }

        if (total_tags)
            break;
    }

    if (!total_tags)
        return;

    // Draw text tags.
    // TODO enne - be more intelligent about not covering stuff up
    for (int y = 0; y < ENV_SHOW_DIAMETER; y++)
        for (int x = 0; x < ENV_SHOW_DIAMETER; x++)
        {
            coord_def ep(x, y);
            tag_def &def = tag_show(ep);

            if (!def.text)
                continue;

            const coord_def gc = show2grid(ep);
            coord_def pc;
            to_screen_coords(gc, &pc);
            // center this coord, which is at the top left of gc's cell
            pc.x += dx / 2;

            const coord_def min_pos(sx, sy);
            const coord_def max_pos(ex, ey);
            m_tag_font->render_string(pc.x, pc.y, def.text,
                                      min_pos, max_pos, WHITE, false);
        }
}

/**
 * Draws miniature health and magic points bars on top of the player tile.
 *
 * Drawing of either is governed by options tile_show_minihealthbar and
 * tile_show_minimagicbar. By default, both are on.
 *
 * Intended behaviour is to display both if either is not full. (Unless
 * the bar is toggled off by options.) --Eino & felirx
 */
void DungeonRegion::draw_minibars()
{
    if (Options.tile_show_minihealthbar && you.hp < you.hp_max
        || Options.tile_show_minimagicbar
           && you.magic_points < you.max_magic_points)
    {
        // Tiles are 32x32 pixels; 1/32 = 0.03125.
        // The bars are two pixels high each.
        const float bar_height = 0.0625;
        float healthbar_offset = 0.875;

        ShapeBuffer buff;

        if (!you.on_current_level || !on_screen(you.pos()))
            return;

        // FIXME: to_screen_coords could be made into two versions: one
        // that gives coords by pixel (the current one), one that gives
        // them by grid.
        coord_def player_on_screen;
        to_screen_coords(you.pos(), &player_on_screen);
        player_on_screen.x = (player_on_screen.x-sx)/dx;
        player_on_screen.y = (player_on_screen.y-sy)/dy;

        if (Options.tile_show_minimagicbar && you.max_magic_points > 0)
        {
            static const VColour magic(0, 114, 159, 207);      // lightblue
            static const VColour magic_spent(0, 0, 0, 255);  // black

            const float magic_divider = (float) you.magic_points
                                        / (float) you.max_magic_points;

            buff.add(player_on_screen.x,
                     player_on_screen.y + healthbar_offset + bar_height,
                     player_on_screen.x + magic_divider,
                     player_on_screen.y + 1,
                     magic);
            buff.add(player_on_screen.x + magic_divider,
                     player_on_screen.y + healthbar_offset + bar_height,
                     player_on_screen.x + 1,
                     player_on_screen.y + 1,
                     magic_spent);
        }
        else
            healthbar_offset += bar_height;

        if (Options.tile_show_minihealthbar)
        {
            const float min_hp = max(0, you.hp);
            const float health_divider = min_hp / (float) you.hp_max;

            const int hp_percent = (you.hp * 100) / you.hp_max;

            int hp_colour = GREEN;
            for (const auto &entry : Options.hp_colour)
                if (hp_percent <= entry.first)
                    hp_colour = entry.second;

            static const VColour healthy(   0, 255, 0, 255); // green
            static const VColour damaged( 255, 255, 0, 255); // yellow
            static const VColour wounded( 150,   0, 0, 255); // darkred
            static const VColour hp_spent(255,   0, 0, 255); // red

            buff.add(player_on_screen.x,
                     player_on_screen.y + healthbar_offset,
                     player_on_screen.x + health_divider,
                     player_on_screen.y + healthbar_offset + bar_height,
                     hp_colour == RED    ? wounded :
                     hp_colour == YELLOW ? damaged
                                         : healthy);

            buff.add(player_on_screen.x + health_divider,
                     player_on_screen.y + healthbar_offset,
                     player_on_screen.x + 1,
                     player_on_screen.y + healthbar_offset + bar_height,
                     hp_spent);
        }

        buff.draw();
    }
}

void DungeonRegion::clear()
{
    m_vbuf.clear();
}

void DungeonRegion::on_resize()
{
    // TODO enne
}

// FIXME: If the player is targeted, the game asks the player to target
// something with the mouse, then targets the player anyway and treats
// mouse click as if it hadn't come during targeting (moves the player
// to the clicked cell, whatever).
static void _add_targeting_commands(const coord_def& pos)
{
    // Force targeting cursor back onto center to start off on a clean
    // slate.
    macro_buf_add_cmd(CMD_TARGET_FIND_YOU);

    const coord_def delta = pos - you.pos();

    command_type cmd;

    if (delta.x < 0)
        cmd = CMD_TARGET_LEFT;
    else
        cmd = CMD_TARGET_RIGHT;

    for (int i = 0; i < abs(delta.x); i++)
        macro_buf_add_cmd(cmd);

    if (delta.y < 0)
        cmd = CMD_TARGET_UP;
    else
        cmd = CMD_TARGET_DOWN;

    for (int i = 0; i < abs(delta.y); i++)
        macro_buf_add_cmd(cmd);

    macro_buf_add_cmd(CMD_TARGET_MOUSE_SELECT);
}

static bool _is_appropriate_spell(spell_type spell, const actor* target)
{
    ASSERT(is_valid_spell(spell));

    const unsigned int flags    = get_spell_flags(spell);
    const bool         targeted = flags & SPFLAG_TARGETING_MASK;

    // Most spells are blocked by transparent walls.
    // XXX: deduplicate this with the other two? smitey spell lists
    if (targeted && !you.see_cell_no_trans(target->pos()))
    {
        switch (spell)
        {
        case SPELL_CALL_DOWN_DAMNATION:
        case SPELL_SMITING:
        case SPELL_HAUNT:
        case SPELL_FIRE_STORM:
        case SPELL_AIRSTRIKE:
            break;

        default:
            return false;
        }
    }

    const bool helpful = flags & SPFLAG_HELPFUL;

    if (target->is_player())
    {
        if (flags & SPFLAG_NOT_SELF)
            return false;

        return (flags & (SPFLAG_HELPFUL | SPFLAG_ESCAPE | SPFLAG_RECOVERY))
               || !targeted;
    }

    if (!targeted)
        return false;

    if (flags & SPFLAG_NEUTRAL)
        return false;

    bool friendly = target->as_monster()->wont_attack();

    return friendly == helpful;
}

static bool _is_appropriate_evokable(const item_def& item,
                                     const actor* target)
{
    if (!item_is_evokable(item, false, false, true))
        return false;

    // Only wands for now.
    if (item.base_type != OBJ_WANDS)
        return false;

    // Aren't yet any wands that can go through transparent walls.
    if (!you.see_cell_no_trans(target->pos()))
        return false;

    // We don't know what it is, so it *might* be appropriate.
    if (!item_type_known(item))
        return true;

    // Random effects are always (in)appropriate for all targets.
    if (item.sub_type == WAND_RANDOM_EFFECTS)
        return true;

    spell_type spell = zap_to_spell(item.zap());

    return _is_appropriate_spell(spell, target);
}

static bool _have_appropriate_evokable(const actor* target)
{
    return any_of(begin(you.inv), end(you.inv),
                  [target] (const item_def &item) -> bool
                  {
                      return item.defined()
                          && _is_appropriate_evokable(item, target);
                  });
}

static item_def* _get_evokable_item(const actor* target)
{
    vector<const item_def*> list;

    for (const auto &item : you.inv)
        if (item.defined() && _is_appropriate_evokable(item, target))
            list.push_back(&item);

    ASSERT(!list.empty());

    InvMenu menu(MF_SINGLESELECT | MF_ANYPRINTABLE
                 | MF_ALLOW_FORMATTING | MF_SELECT_BY_PAGE);
    menu.set_type(MT_ANY);
    menu.set_title("Wand to zap?");
    menu.load_items(list);
    menu.show();
    vector<SelItem> sel = menu.get_selitems();

    update_screen();
    redraw_screen();

    if (sel.empty())
        return nullptr;

    return const_cast<item_def*>(sel[0].item);
}

static bool _evoke_item_on_target(actor* target)
{
    item_def* item;
    {
        // Prevent the inventory letter from being recorded twice.
        pause_all_key_recorders pause;

        item = _get_evokable_item(target);
    }

    if (item == nullptr)
        return false;

    if (is_known_empty_wand(*item))
    {
        mpr("That wand is empty.");
        return false;
    }

    macro_buf_add_cmd(CMD_EVOKE);
    macro_buf_add(index_to_letter(item->link)); // Inventory letter.
    _add_targeting_commands(target->pos());
    return true;
}

static bool _spell_in_range(spell_type spell, actor* target)
{
    if (!(get_spell_flags(spell) & SPFLAG_TARGETING_MASK))
        return true;

    int range = calc_spell_range(spell);

    switch (spell)
    {
    case SPELL_MEPHITIC_CLOUD:
    case SPELL_FIREBALL:
    case SPELL_FREEZING_CLOUD:
    case SPELL_POISONOUS_CLOUD:
        // Increase range by one due to cloud radius.
        range++;
        break;
    default:
        break;
    }

    return range >= grid_distance(you.pos(), target->pos());
}

static actor* _spell_target = nullptr;

static bool _spell_selector(spell_type spell)
{
    return _is_appropriate_spell(spell, _spell_target);
}

// TODO: Cast spells which target a particular cell.
static bool _cast_spell_on_target(actor* target)
{
    _spell_target = target;
    spell_type spell;
    int letter;

    if (is_valid_spell(you.last_cast_spell)
        && _is_appropriate_spell(you.last_cast_spell, target))
    {
        spell = you.last_cast_spell;
        letter = get_spell_letter(spell);
    }
    else
    {
        {
            // Prevent the spell letter from being recorded twice.
            pause_all_key_recorders pause;

            letter = list_spells(true, false, true, "Your Spells",
                                 _spell_selector);
        }

        _spell_target = nullptr;

        if (letter == 0)
            return false;

        spell = get_spell_by_letter(letter);
    }

    ASSERT(is_valid_spell(spell));
    ASSERT(_is_appropriate_spell(spell, target));

    if (!_spell_in_range(spell, target))
    {
        mprf("%s is out of range for that spell.",
             target->name(DESC_THE).c_str());
        return true;
    }

    if (spell_mana(spell) > you.magic_points)
    {
        mpr("You don't have enough magic to cast that spell.");
        return true;
    }

    macro_buf_add_cmd(CMD_FORCE_CAST_SPELL);
    macro_buf_add(letter);

    if (get_spell_flags(spell) & SPFLAG_TARGETING_MASK)
        _add_targeting_commands(target->pos());

    return true;
}

static bool _have_appropriate_spell(const actor* target)
{
    for (spell_type spell : you.spells)
    {
        if (!is_valid_spell(spell))
            continue;

        if (_is_appropriate_spell(spell, target))
            return true;
    }
    return false;
}

static bool _can_fire_item()
{
    return you.species != SP_FELID
           && you.m_quiver.get_fire_item() != -1;
}

static bool _handle_distant_monster(monster* mon, unsigned char mod)
{
    const bool shift = (mod & MOD_SHIFT);
    const bool ctrl  = (mod & MOD_CTRL);
    const bool alt   = (shift && ctrl || (mod & MOD_ALT));
    const item_def* weapon = you.weapon();

    // Handle evoking items at monster.
    if (alt && _have_appropriate_evokable(mon))
        return _evoke_item_on_target(mon);

    // Handle firing quivered items.
    if (_can_fire_item() && !ctrl
        && (shift || weapon && is_range_weapon(*weapon)
                     && !mon->wont_attack()))
    {
        macro_buf_add_cmd(CMD_FIRE);
        _add_targeting_commands(mon->pos());
        return true;
    }

    // Handle casting spells at monster.
    if (ctrl && !shift && _have_appropriate_spell(mon))
        return _cast_spell_on_target(mon);

    // Handle weapons of reaching.
    if (!mon->wont_attack() && you.see_cell_no_trans(mon->pos()))
    {
        const int dist = (you.pos() - mon->pos()).rdist();

        if (dist > 1 && weapon && weapon_reach(*weapon) >= dist)
        {
            macro_buf_add_cmd(CMD_EVOKE_WIELDED);
            _add_targeting_commands(mon->pos());
            return true;
        }
    }

    return false;
}

static bool _handle_zap_player(MouseEvent &event)
{
    const bool shift = (event.mod & MOD_SHIFT);
    const bool ctrl  = (event.mod & MOD_CTRL);
    const bool alt   = (shift && ctrl || (event.mod & MOD_ALT));

    if (alt && _have_appropriate_evokable(&you))
        return _evoke_item_on_target(&you);

    if (ctrl && _have_appropriate_spell(&you))
        return _cast_spell_on_target(&you);

    return false;
}

void DungeonRegion::zoom(bool in)
{
    int sign = in ? 1 : -1;
    int amt  = 4;
    const int max_zoom = 64; // this needs to be a proportion, not a fixed amount!
    const bool minimap_zoom = (sx>dx); // i.e. there's a border bigger than a tile (was dx<min_zoom+amt)

    // if we try to zoom out too far, go to minimap instead
    if (!in && minimap_zoom)
        if (tiles.zoom_to_minimap())
            return;

    // if we zoomed in from min zoom, and the map's still up, switch off minimap instead
    if (in && minimap_zoom)
        if (tiles.zoom_from_minimap())
            return;

    // if we zoom out too much, stop
    if (!in && minimap_zoom) //(dx + sign*amt < min_zoom)
        return;
    // if we zoom in too close, stop
    if (dx + sign*amt > max_zoom)
        return;

    dx = dx + sign*amt;
    dy = dy + sign*amt;

    int old_wx = wx; int old_wy = wy;
    recalculate();

    place((old_wx-wx)/2+sx, (old_wy-wy)/2+sy, 0);

    crawl_view.viewsz.x = mx;
    crawl_view.viewsz.y = my;
}

int DungeonRegion::handle_mouse(MouseEvent &event)
{
    tiles.clear_text_tags(TAG_CELL_DESC);

    if (!inside(event.px, event.py))
        return 0;

#ifdef TOUCH_UI
    if (event.event == MouseEvent::PRESS
        && (event.mod & MOD_CTRL)
        && (event.button == MouseEvent::SCROLL_UP || event.button == MouseEvent::SCROLL_DOWN))
    {
        zoom(event.button == MouseEvent::SCROLL_UP);
    }
#endif

    if (mouse_control::current_mode() == MOUSE_MODE_NORMAL
        && event.event == MouseEvent::PRESS
        && event.button == MouseEvent::LEFT)
    {
        m_last_clicked_grid = m_cursor[CURSOR_MOUSE];

        int cx, cy;
        mouse_pos(event.px, event.py, cx, cy);
        const coord_def gc(cx + m_cx_to_gx, cy + m_cy_to_gy);
        tiles.place_cursor(CURSOR_MOUSE, gc);

        return CK_MOUSE_CLICK;
    }

    if (mouse_control::current_mode() == MOUSE_MODE_NORMAL
        || mouse_control::current_mode() == MOUSE_MODE_MACRO
        || mouse_control::current_mode() == MOUSE_MODE_MORE
        || mouse_control::current_mode() == MOUSE_MODE_PROMPT
        || mouse_control::current_mode() == MOUSE_MODE_YESNO)
    {
        return 0;
    }

    int cx;
    int cy;

    bool on_map = mouse_pos(event.px, event.py, cx, cy);

    const coord_def gc(cx + m_cx_to_gx, cy + m_cy_to_gy);
    tiles.place_cursor(CURSOR_MOUSE, gc);

    if (event.event == MouseEvent::MOVE)
    {
        string desc = get_terse_square_desc(gc);
        // Suppress floor description
        if (desc == "floor")
            desc = "";

        if (you.see_cell(gc))
        {
            if (cloud_struct* cloud = cloud_at(gc))
            {
                string terrain_desc = desc;
                desc = cloud->cloud_name(true);

                if (!terrain_desc.empty())
                    desc += "\n" + terrain_desc;
            }
        }

        if (!desc.empty())
            tiles.add_text_tag(TAG_CELL_DESC, desc, gc);
    }

    if (!on_map)
        return 0;

    if (mouse_control::current_mode() == MOUSE_MODE_TARGET
        || mouse_control::current_mode() == MOUSE_MODE_TARGET_PATH
        || mouse_control::current_mode() == MOUSE_MODE_TARGET_DIR)
    {
        if (event.event == MouseEvent::MOVE)
            return CK_MOUSE_MOVE;
        else if (event.event == MouseEvent::PRESS
                 && event.button == MouseEvent::LEFT && on_screen(gc))
        {
            m_last_clicked_grid = m_cursor[CURSOR_MOUSE];
            return CK_MOUSE_CLICK;
        }

        return 0;
    }

    if (event.event != MouseEvent::PRESS)
        return 0;

    m_last_clicked_grid = m_cursor[CURSOR_MOUSE];

    if (you.pos() == gc)
    {
        switch (event.button)
        {
        case MouseEvent::LEFT:
        {
            if ((event.mod & (MOD_CTRL | MOD_ALT)))
            {
                if (_handle_zap_player(event))
                    return 0;
            }

            // if there's an item, pick it up, otherwise wait 1 turn
            if (!(event.mod & MOD_SHIFT))
            {
                const int o = you.visible_igrd(you.pos());
                if (o == NON_ITEM)
                {
                    // if on stairs, travel them
                    const dungeon_feature_type feat = grd(gc);
                    switch (feat_stair_direction(feat))
                    {
                    case CMD_GO_DOWNSTAIRS:
                    case CMD_GO_UPSTAIRS:
                        return command_to_key(feat_stair_direction(feat));
                    default:
                        // otherwise wait
                        return command_to_key(CMD_WAIT);
                    }
                }
                else
                {

                    // pick up menu
                    // More than a single item -> open menu right away.
                    if (count_movable_items(o) > 1)
                    {
                        pickup_menu(o);
                        flush_prev_message();
                        redraw_screen();
                        return CK_MOUSE_CMD;
                    }
                    return command_to_key(CMD_PICKUP);
                }
            }

            const dungeon_feature_type feat = grd(gc);
            switch (feat_stair_direction(feat))
            {
            case CMD_GO_DOWNSTAIRS:
            case CMD_GO_UPSTAIRS:
                return command_to_key(feat_stair_direction(feat));
            default:
                return 0;
            }
        }
        case MouseEvent::RIGHT:
            if (!(event.mod & MOD_SHIFT))
                return command_to_key(CMD_RESISTS_SCREEN); // Character overview.
            if (!you_worship(GOD_NO_GOD))
                return command_to_key(CMD_DISPLAY_RELIGION); // Religion screen.

            // fall through...
        default:
            return 0;
        }

    }
    // else not on player...
    if (event.button == MouseEvent::RIGHT)
    {
        full_describe_square(gc);
        return CK_MOUSE_CMD;
    }

    if (event.button != MouseEvent::LEFT)
        return 0;

    return tile_click_cell(gc, event.mod);
}

int tile_click_cell(const coord_def &gc, unsigned char mod)
{
    monster* mon = monster_at(gc);
    if (mon && you.can_see(*mon))
    {
        if (_handle_distant_monster(mon, mod))
            return CK_MOUSE_CMD;
    }

    if ((mod & MOD_CTRL) && adjacent(you.pos(), gc))
    {
        const int cmd = click_travel(gc, mod & MOD_CTRL);
        if (cmd != CK_MOUSE_CMD)
            process_command((command_type) cmd);

        return CK_MOUSE_CMD;
    }

    // Don't move if we've tried to fire/cast/evoke when there's nothing
    // available.
    if (mod & (MOD_SHIFT | MOD_CTRL | MOD_ALT))
        return CK_MOUSE_CMD;

    const int cmd = click_travel(gc, mod & MOD_CTRL);
    if (cmd != CK_MOUSE_CMD)
        process_command((command_type) cmd);

    return CK_MOUSE_CMD;
}

void DungeonRegion::to_screen_coords(const coord_def &gc, coord_def *pc) const
{
    int cx = gc.x - m_cx_to_gx;
    int cy = gc.y - m_cy_to_gy;

    pc->x = sx + ox + cx * dx;
    pc->y = sy + oy + cy * dy;
}

bool DungeonRegion::on_screen(const coord_def &gc) const
{
    int x = gc.x - m_cx_to_gx;
    int y = gc.y - m_cy_to_gy;

    return x >= 0 && x < mx && y >= 0 && y < my;
}

void DungeonRegion::place_cursor(cursor_type type, const coord_def &gc)
{
    coord_def result = gc;

    // If we're only looking for a direction, put the mouse
    // cursor next to the player to let them know that their
    // spell/wand will only go one square.
    if (mouse_control::current_mode() == MOUSE_MODE_TARGET_DIR
        && type == CURSOR_MOUSE && gc != NO_CURSOR)
    {
        coord_def delta = gc - you.pos();

        int ax = abs(delta.x);
        int ay = abs(delta.y);

        result = you.pos();
        if (1000 * ay < 414 * ax)
            result += (delta.x > 0) ? coord_def(1, 0) : coord_def(-1, 0);
        else if (1000 * ax < 414 * ay)
            result += (delta.y > 0) ? coord_def(0, 1) : coord_def(0, -1);
        else if (delta.x > 0)
            result += (delta.y > 0) ? coord_def(1, 1) : coord_def(1, -1);
        else if (delta.x < 0)
            result += (delta.y > 0) ? coord_def(-1, 1) : coord_def(-1, -1);
    }

    if (m_cursor[type] != result)
    {
        m_dirty = true;
        m_cursor[type] = result;
        if (type == CURSOR_MOUSE)
            m_last_clicked_grid = coord_def();
    }
}

bool DungeonRegion::update_tip_text(string &tip)
{
    // TODO enne - it would be really nice to use the tutorial
    // descriptions here for features, monsters, etc...
    // Unfortunately, that would require quite a bit of rewriting
    // and some parsing of formatting to get that to work.

    if (mouse_control::current_mode() != MOUSE_MODE_COMMAND)
        return false;

    if (m_cursor[CURSOR_MOUSE] == NO_CURSOR)
        return false;
    if (!map_bounds(m_cursor[CURSOR_MOUSE]))
        return false;

    const coord_def gc = m_cursor[CURSOR_MOUSE];
    bool ret = (tile_dungeon_tip(gc, tip));

#ifdef WIZARD
    if (you.wizard)
    {
        if (ret)
            tip += "\n\n";

        if (you.see_cell(gc))
        {
            const coord_def ep = grid2show(gc);

            tip += make_stringf("GC(%d, %d) EP(%d, %d)\n",
                                gc.x, gc.y, ep.x, ep.y);

            if (env.heightmap.get())
                tip += make_stringf("HEIGHT(%d)\n", dgn_height_at(gc));

            tip += "\n";
            tip += tile_debug_string(env.tile_fg(ep), env.tile_bg(ep), env.tile_cloud(ep), ' ');
        }
        else
        {
            tip += make_stringf("GC(%d, %d) [out of sight]\n", gc.x, gc.y);
            if (env.heightmap.get())
                tip += make_stringf("HEIGHT(%d)\n", dgn_height_at(gc));
            tip += "\n";
        }

        tip += tile_debug_string(env.tile_bk_fg(gc), env.tile_bk_bg(gc), env.tile_bk_bg(gc), 'B');

        if (!m_vbuf.empty())
        {
            const screen_cell_t *vbuf = m_vbuf;
            const coord_def vc(gc.x - m_cx_to_gx, gc.y - m_cy_to_gy);
            const screen_cell_t &cell = vbuf[crawl_view.viewsz.x * vc.y + vc.x];
            tip += tile_debug_string(cell.tile.fg, cell.tile.bg, cell.tile.cloud, 'V');
        }

        tip += make_stringf("\nFLV: floor: %d (%s) (%d)"
                            "\n     wall:  %d (%s) (%d)"
                            "\n     feat:  %d (%s) (%d)"
                            "\n  special:  %d\n",
                            env.tile_flv(gc).floor,
                            tile_dngn_name(env.tile_flv(gc).floor),
                            env.tile_flv(gc).floor_idx,
                            env.tile_flv(gc).wall,
                            tile_dngn_name(env.tile_flv(gc).wall),
                            env.tile_flv(gc).wall_idx,
                            env.tile_flv(gc).feat,
                            tile_dngn_name(env.tile_flv(gc).feat),
                            env.tile_flv(gc).feat_idx,
                            env.tile_flv(gc).special);

        ret = true;
    }
#endif

    return ret;
}

static string _check_spell_evokable(const actor* target,
                                    vector<command_type> &cmd)
{
    string str = "";
    if (_have_appropriate_spell(target))
    {
        str += "\n[Ctrl + L-Click] Cast spell (%)";
        cmd.push_back(CMD_CAST_SPELL);
    }

    if (_have_appropriate_evokable(target))
    {
        string key = "Alt";
#ifdef UNIX
        // On Unix systems the Alt key is already hogged by
        // the application window, at least when we're not
        // in fullscreen mode, so we use Ctrl-Shift instead.
        if (!tiles.is_fullscreen())
            key = "Ctrl-Shift";
#endif
        str += "\n[" + key + " + L-Click] Zap wand (%)";
        cmd.push_back(CMD_EVOKE);
    }

    return str;
}

static void _add_tip(string &tip, string text)
{
    if (!tip.empty())
        tip += "\n";
    tip += text;
}

bool tile_dungeon_tip(const coord_def &gc, string &tip)
{
    const int attack_dist = you.weapon() ?
        weapon_reach(*you.weapon()) : 1;

    vector<command_type> cmd;
    tip = "";
    bool has_monster = false;

    // Left-click first.
    if (gc == you.pos())
    {
        tip = you.your_name;
        tip += " (";
        tip += get_species_abbrev(you.species);
        tip += get_job_abbrev(you.char_class);
        tip += ")";

        tip += _check_spell_evokable(&you, cmd);
    }
    else // non-player squares
    {
        const actor* target = actor_at(gc);
        if (target && you.can_see(*target))
        {
            has_monster = true;
            if ((gc - you.pos()).rdist() <= attack_dist)
            {
                if (!cell_is_solid(gc))
                {
                    const monster* mon = monster_at(gc);
                    if (!mon || mon->friendly() || !mon->visible_to(&you))
                        _add_tip(tip, "[L-Click] Move");
                    else if (mon)
                    {
                        tip = mon->name(DESC_A);
                        _add_tip(tip, "[L-Click] Attack");
                    }
                }
            }

            if (you.species != SP_FELID
                && you.see_cell_no_trans(target->pos())
                && you.m_quiver.get_fire_item() != -1)
            {
                _add_tip(tip, "[Shift + L-Click] Fire (%)");
                cmd.push_back(CMD_FIRE);
            }

            tip += _check_spell_evokable(target, cmd);
        }
        else if (!cell_is_solid(gc)) // no monster or player
        {
            if (adjacent(gc, you.pos()))
                _add_tip(tip, "[L-Click] Move");
            else if (env.map_knowledge(gc).feat() != DNGN_UNSEEN
                     && i_feel_safe())
            {
                _add_tip(tip, "[L-Click] Travel");
            }
        }
        else if (feat_is_closed_door(grd(gc)))
        {
            if (!adjacent(gc, you.pos()) && i_feel_safe())
                _add_tip(tip, "[L-Click] Travel");

            _add_tip(tip, "[L-Click] Open door (%)");
            cmd.push_back(CMD_OPEN_DOOR);
        }
    }

    // These apply both on the same square as the player's and elsewhere.
    if (!has_monster)
    {
        if (you.see_cell(gc) && env.map_knowledge(gc).item())
        {
            const item_info * const item = env.map_knowledge(gc).item();
            if (item && !item_is_stationary(*item))
            {
                _add_tip(tip, "[L-Click] Pick up items (%)");
                cmd.push_back(CMD_PICKUP);
            }
        }

        const dungeon_feature_type feat = env.map_knowledge(gc).feat();
        const command_type dir = feat_stair_direction(feat);
        if (dir != CMD_NO_CMD)
        {
            _add_tip(tip, "[Shift + L-Click] ");
            if (feat == DNGN_ENTER_SHOP)
                tip += "enter shop";
            else if (feat_is_altar(feat)
                     && player_can_join_god(feat_altar_god(feat)))
            {
                tip += "pray at altar";
            }
            else if (feat_is_gate(feat))
                tip += "enter gate";
            else
                tip += "use stairs";

            tip += " (%)";
            cmd.push_back(dir);
        }
    }

    // Right-click.
    if (gc == you.pos())
    {
        const int o = you.visible_igrd(you.pos());
        if (o == NON_ITEM)
        {
            // if on stairs, travel them
            const dungeon_feature_type feat = grd(gc);
            if (feat_stair_direction(feat) == CMD_GO_DOWNSTAIRS
                || feat_stair_direction(feat) == CMD_GO_UPSTAIRS)
            {
                // XXX: wrong for golubria, shops?
                _add_tip(tip, "[L-Click] Use stairs (%)");
                cmd.push_back(feat_stair_direction(feat));
            }
            else if (feat_is_altar(feat)
                     && player_can_join_god(feat_altar_god(feat)))
            {
                _add_tip(tip, "[L-Click] Pray at altar (%)");
                cmd.push_back(feat_stair_direction(feat));
            }
            else
            {
                // otherwise wait
                _add_tip(tip, "[L-Click] Wait one turn (%)");
                cmd.push_back(CMD_WAIT);
            }
        }
        else
        {
            // pick up menu
            // this is already added by the code above
        }

        // Character overview.
        _add_tip(tip, "[R-Click] Overview (%)");
        cmd.push_back(CMD_RESISTS_SCREEN);

        // Religion.
        if (!you_worship(GOD_NO_GOD))
        {
            _add_tip(tip, "[Shift + R-Click] Religion (%)");
            cmd.push_back(CMD_DISPLAY_RELIGION);
        }
    }
    else if (you.see_cell(gc)
             && env.map_knowledge(gc).feat() != DNGN_UNSEEN)
    {
        _add_tip(tip, "[R-Click] Describe");
    }

    if (!tip.empty())
        insert_commands(tip, cmd, false);

    return true;
}

bool DungeonRegion::update_alt_text(string &alt)
{
    if (mouse_control::current_mode() != MOUSE_MODE_COMMAND)
        return false;

    const coord_def &gc = m_cursor[CURSOR_MOUSE];

    if (gc == NO_CURSOR)
        return false;
    if (!map_bounds(gc))
        return false;
    if (!env.map_knowledge(gc).seen())
        return false;
    if (m_last_clicked_grid == gc)
        return false;

    describe_info inf;
    dungeon_feature_type feat = env.map_knowledge(gc).feat();
    if (you.see_cell(gc))
        get_square_desc(gc, inf);
    else if (feat != DNGN_FLOOR && !feat_is_wall(feat) && !feat_is_tree(feat))
        get_feature_desc(gc, inf);
    else
    {
        // For plain floor, output the stash description.
        const string stash = get_stash_desc(gc);
        if (!stash.empty())
            inf.body << "\n" << stash;
    }

    alt_desc_proc proc(crawl_view.msgsz.x, crawl_view.msgsz.y);
    process_description<alt_desc_proc>(proc, inf);

    proc.get_string(alt);

    // Suppress floor description
    if (alt == "Floor.")
    {
        alt.clear();
        return false;
    }
    return true;
}

void DungeonRegion::clear_text_tags(text_tag_type type)
{
    m_tags[type].clear();
}

void DungeonRegion::add_text_tag(text_tag_type type, const string &tag,
                                 const coord_def &gc)
{
    TextTag t;
    t.tag = tag;
    t.gc  = gc;

    m_tags[type].push_back(t);
}

void DungeonRegion::add_overlay(const coord_def &gc, int idx)
{
    tile_overlay over;
    over.gc  = gc;
    over.idx = idx;

    m_overlays.push_back(over);
    m_dirty = true;
}

void DungeonRegion::clear_overlays()
{
    m_overlays.clear();
    m_dirty = true;
}

#endif
