/**
 * @file
 * @brief Rendering of map_cell to glyph and colour.
 *
 * This only needs the information within one object of type map_cell.
**/

#include "AppHdr.h"

#include "showsymb.h"

#include "colour.h"
#include "env.h"
#include "item-name.h"
#include "libutil.h" // map_find
#include "options.h"
#include "religion.h"
#include "stash.h"
#include "state.h"
#include "stringutil.h"
#include "terrain.h"
#include "travel.h"
#include "viewchar.h"
#include "mon-tentacle.h"
#include "tilepick.h"
#include "rltiles/tiledef-player.h"

static unsigned short _cell_feat_show_colour(const map_cell& cell,
                                             const coord_def& loc,
                                             bool coloured)
{
    dungeon_feature_type feat = cell.feat();
    unsigned short colour = BLACK;
    const feature_def &fdef = get_feature_def(feat);

    // These do not obey vault recolouring.
    const bool no_vault_recolour = feat_has_dry_floor(feat)
                                   && feat != DNGN_FLOOR
                                   && feat != DNGN_OPEN_DOOR;

    // These aren't shown mossy/bloody/slimy in console.
    const bool norecolour = feat_is_door(feat) || no_vault_recolour;

    if (is_stair_exclusion(loc))
        colour = Options.tc_excluded;
    else if (!coloured)
    {
        if (cell.flags & MAP_EMPHASIZE)
            colour = fdef.seen_em_colour();
        else if (cell.flags & MAP_MAGIC_MAPPED_FLAG)
            colour = fdef.unseen_colour();
        else
            colour = fdef.seen_colour();

        if (colour)
        {
            // Show trails even out of LOS.
            if (Options.show_travel_trail && travel_trail_index(loc) >= 0)
                colour |= COLFLAG_REVERSE;
            return colour;
        }
    }
    else if (!feat_is_solid(feat)
             && (cell.flags & (MAP_SANCTUARY_1 | MAP_SANCTUARY_2)))
    {
        if (cell.flags & MAP_SANCTUARY_1)
            colour = YELLOW;
        else if (cell.flags & MAP_SANCTUARY_2)
        {
            if (!one_chance_in(4))
                colour = WHITE;     // 3/4
            else if (!one_chance_in(3))
                colour = LIGHTCYAN; // 1/6
            else
                colour = LIGHTGREY; // 1/12
        }
    }
    else if (cell.flags & MAP_BLOODY && !norecolour)
        colour = RED;
    else if (cell.flags & MAP_MOLDY && !norecolour)
        colour = (cell.flags & MAP_GLOWING_MOLDY) ? LIGHTRED : LIGHTGREEN;
    else if (cell.flags & MAP_CORRODING && !norecolour
             && !feat_is_wall(feat) && !feat_is_lava(feat)
             && !feat_is_water(feat))
    {
        colour = LIGHTGREEN;
    }
    else if (cell.feat_colour() && !no_vault_recolour)
        colour = cell.feat_colour();
    else
    {
        colour = fdef.colour();

        if (fdef.em_colour() && fdef.em_colour() != fdef.colour()
            && cell.flags & MAP_EMPHASIZE)
        {
            colour = fdef.em_colour();
        }
    }

    if (feat == DNGN_SHALLOW_WATER && player_in_branch(BRANCH_SHOALS))
        colour = ETC_WAVES;

    if (feat_is_tree(feat) && env.forest_awoken_until)
        colour = ETC_AWOKEN_FOREST;

    if (feat == DNGN_FLOOR)
    {
        if (cell.flags & MAP_LIQUEFIED)
            colour = ETC_LIQUEFIED;
        else if (cell.flags & MAP_DISJUNCT)
            colour = ETC_DISJUNCTION;
        else if (cell.flags & MAP_HALOED)
        {
            if (cell.flags & MAP_SILENCED && cell.flags & MAP_UMBRAED)
                colour = CYAN; // Default for silence.
            else if (cell.flags & MAP_SILENCED)
                colour = LIGHTCYAN;
            else if (cell.flags & MAP_UMBRAED)
                colour = fdef.colour(); // Cancels out!
            else
                colour = YELLOW;
        }
        else if (cell.flags & MAP_UMBRAED)
        {
            if (cell.flags & MAP_SILENCED)
                colour = BLUE; // Silence gets darker
            else
                colour = MAGENTA; // If no holy or silence
        }
        else if (cell.flags & MAP_SILENCED)
            colour = CYAN; // Silence but no holy/unholy
        else if (cell.flags & MAP_ORB_HALOED)
            colour = ETC_ORB_GLOW;
        else if (cell.flags & MAP_QUAD_HALOED)
            colour = BLUE;
#if TAG_MAJOR_VERSION == 34
        else if (cell.flags & MAP_HOT)
            colour = ETC_FIRE;
#endif
    }

    if (Options.show_travel_trail && travel_trail_index(loc) >= 0)
        colour |= COLFLAG_REVERSE;

    return colour;
}

static monster_type _show_mons_type(const monster_info& mi)
{
    if (mi.type == MONS_SLIME_CREATURE && mi.slime_size > 1)
        return MONS_MERGED_SLIME_CREATURE;
    else if (mi.type == MONS_ZOMBIE)
    {
        return mons_zombie_size(mi.base_type) == Z_BIG ?
            MONS_ZOMBIE_LARGE : MONS_ZOMBIE_SMALL;
    }
    else if (mi.type == MONS_SKELETON)
    {
        return mons_zombie_size(mi.base_type) == Z_BIG ?
            MONS_SKELETON_LARGE : MONS_SKELETON_SMALL;
    }
    else if (mi.type == MONS_SIMULACRUM)
    {
        return mons_zombie_size(mi.base_type) == Z_BIG ?
            MONS_SIMULACRUM_LARGE : MONS_SIMULACRUM_SMALL;
    }
    else if (mi.type == MONS_SENSED)
        return mi.base_type;

    return mi.type;
}

static int _get_mons_colour(const monster_info& mi)
{
    // Show hp directly on the monster, except for irrelevant ones.
    // Fedhas worshippers might be interested in their plants however.
    if (crawl_state.viewport_monster_hp
        && (you_worship(GOD_FEDHAS) || !mons_class_is_firewood(mi.type)))
    {
        return dam_colour(mi) | COLFLAG_ITEM_HEAP;
    }

    int col = mi.colour();

    // We really shouldn't store unmodified colour. This hack compares
    // effective type, but really, all redefinitions should work instantly,
    // rather than for newly spawned monsters only.
    monster_type stype = _show_mons_type(mi);
    if (stype != mi.type && mi.type != MONS_SENSED)
        col = mons_class_colour(stype);

    if (mi.is(MB_ROLLING))
        col = ETC_BONE;

    if (mi.is(MB_BERSERK))
        col = RED;

    if (mi.is(MB_MIRROR_DAMAGE))
        col = ETC_DEATH;

    if (mi.is(MB_INNER_FLAME))
        col = ETC_FIRE;

    if (mi.attitude == ATT_FRIENDLY)
        col |= COLFLAG_FRIENDLY_MONSTER;
    else if (mi.attitude != ATT_HOSTILE)
        col |= COLFLAG_NEUTRAL_MONSTER;
    else if (Options.stab_brand != CHATTR_NORMAL
             && mi.is(MB_STABBABLE))
    {
        col |= COLFLAG_WILLSTAB;
    }
    else if (Options.may_stab_brand != CHATTR_NORMAL
             && mi.is(MB_DISTRACTED))
    {
        col |= COLFLAG_MAYSTAB;
    }
    else if (mons_class_is_stationary(mi.type))
    {
        if (Options.feature_item_brand != CHATTR_NORMAL
            && feat_stair_direction(grd(mi.pos)) != CMD_NO_CMD)
        {
            col |= COLFLAG_FEATURE_ITEM;
        }
        else if (Options.heap_brand != CHATTR_NORMAL
                 && you.visible_igrd(mi.pos) != NON_ITEM
                 && !crawl_state.game_is_arena())
        {
            col |= COLFLAG_ITEM_HEAP;
        }
    }

    // Backlit monsters are fuzzy and override colours, but not brands.
    if (!crawl_state.game_is_arena()
        && !you.can_see_invisible()
        && mi.is(MB_INVISIBLE)
        && mi.attitude != ATT_FRIENDLY)
    {
        col = (col & COLFLAG_MASK) | DARKGREY;
    }

    return col;
}

static cglyph_t _get_item_override(const item_def &item)
{
    cglyph_t g;
    g.ch = 0;
    g.col = 0;

    // Skip costly name calculations if not needed.
    if (Options.item_glyph_overrides.empty())
        return g;

    string name = stash_annotate_item(STASH_LUA_SEARCH_ANNOTATE, &item)
                + " {" + item_prefix(item, false) + "} " + item.name(DESC_PLAIN);

    {
        // Check the cache...
        if (cglyph_t *gly = map_find(Options.item_glyph_cache, name))
            return *gly;
    }

    for (auto ir : Options.item_glyph_overrides)
    {
        text_pattern tpat(ir.first);
        if (tpat.matches(name))
        {
            // You may have a rule that sets the glyph but not colour for
            // axes, then another that sets colour only for artefacts
            // (useless items, etc). Thus, apply only parts that apply.
            if (ir.second.ch)
                g.ch = ir.second.ch;
            if (ir.second.col)
                g.col = ir.second.col;
        }
    }

    // Matching against a list of regexps can be costly, save up to 1000
    // last matches.
    if (Options.item_glyph_cache.size() >= 1000)
        Options.item_glyph_cache.clear();
    Options.item_glyph_cache[name] = g;

    return g;
}

show_class get_cell_show_class(const map_cell& cell,
                               bool only_stationary_monsters)
{
    if (cell.invisible_monster())
        return SH_INVIS_EXPOSED;

    if (cell.monster() != MONS_NO_MONSTER
        && (!only_stationary_monsters
            || mons_class_is_stationary(cell.monster())))
    {
        return SH_MONSTER;
    }

    if (cell.cloud() != CLOUD_NONE)
        return SH_CLOUD;

    const dungeon_feature_type feat = cell.feat();
    if (feat && feat_is_solid(feat)
        || feat_has_dry_floor(feat)
           && feat != DNGN_FLOOR
           && feat != DNGN_OPEN_DOOR
           && feat != DNGN_ABANDONED_SHOP
           && feat != DNGN_STONE_ARCH
           && feat != DNGN_EXPIRED_PORTAL
           && !feat_is_fountain(feat))
    {
        return SH_FEATURE;
    }

    if (cell.item())
        return SH_ITEM;

    if (cell.feat())
        return SH_FEATURE;

    return SH_NOTHING;
}

static const unsigned short ripple_table[] =
{
     BLUE,          // BLACK        => BLUE (default)
     BLUE,          // BLUE         => BLUE
     GREEN,         // GREEN        => GREEN
     CYAN,          // CYAN         => CYAN
     RED,           // RED          => RED
     MAGENTA,       // MAGENTA      => MAGENTA
     BROWN,         // BROWN        => BROWN
     DARKGREY,      // LIGHTGREY    => DARKGREY
     DARKGREY,      // DARKGREY     => DARKGREY
     BLUE,          // LIGHTBLUE    => BLUE
     GREEN,         // LIGHTGREEN   => GREEN
     BLUE,          // LIGHTCYAN    => BLUE
     RED,           // LIGHTRED     => RED
     MAGENTA,       // LIGHTMAGENTA => MAGENTA
     BROWN,         // YELLOW       => BROWN
     LIGHTGREY,     // WHITE        => LIGHTGREY
};

static cglyph_t _get_cell_glyph_with_class(const map_cell& cell,
                                           const coord_def& loc,
                                           const show_class cls,
                                           int colour_mode)
{
    const bool coloured = colour_mode == 0 ? cell.visible() : (colour_mode > 0);
    cglyph_t g;
    show_type show;

    g.ch = 0;
    const cloud_type cell_cloud = cell.cloud();

    switch (cls)
    {
    case SH_INVIS_EXPOSED:
        ASSERT(cell.invisible_monster());

        show.cls = SH_INVIS_EXPOSED;
        if (cell_cloud != CLOUD_NONE)
            g.col = cell.cloud_colour();
        else
            g.col = ripple_table[cell.feat_colour() & 0xf];
        break;

    case SH_MONSTER:
    {
        show = cell.monster();
        const monster_info* mi = cell.monsterinfo();
        ASSERT(mi);

        if (cell.detected_monster())
        {
            ASSERT(mi->type == MONS_SENSED);
            if (mons_is_sensed(mi->base_type))
                g.col = mons_class_colour(mi->base_type);
            else
                g.col = Options.detected_monster_colour;
        }
        else if (!coloured)
            g.col = Options.remembered_monster_colour;
        else
            g.col = _get_mons_colour(*mi);

        monster_type stype = _show_mons_type(*mi);
        if (show.mons == MONS_SENSED)
            g.ch = mons_char(mi->base_type);
        else
            g.ch = mons_char(stype);

        if (mons_is_tentacle_segment(stype))
        {
            switch (tileidx_tentacle(*mi))
            {
            case TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_N_SW:
            case TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_N_SE:
            case TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_S_NW:
            case TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_S_NE:
            case TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_N_S:
                g.ch = dchar_glyph(DCHAR_DRAW_VERT);
                break;
            case TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_W_NE:
            case TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_W_SE:
            case TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_E_NW:
            case TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_E_SW:
            case TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_E_W:
                g.ch = dchar_glyph(DCHAR_DRAW_HORIZ);
                break;
            case TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_NE_SW:
                g.ch = dchar_glyph(DCHAR_DRAW_SLASH);
                break;
            case TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_NW_SE:
                g.ch = dchar_glyph(DCHAR_DRAW_BACKSLASH);
                break;
            case TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_E_N:
                g.ch = dchar_glyph(DCHAR_DRAW_BL);
                break;
            case TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_E_S:
                g.ch = dchar_glyph(DCHAR_DRAW_TL);
                break;
            case TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_S_W:
                g.ch = dchar_glyph(DCHAR_DRAW_TR);
                break;
            case TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_N_W:
                g.ch = dchar_glyph(DCHAR_DRAW_BR);
                break;
            case TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_NE_NW:
                g.ch = dchar_glyph(DCHAR_DRAW_DOWN);
                break;
            case TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_SE_SW:
                g.ch = dchar_glyph(DCHAR_DRAW_UP);
                break;
            case TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_NW_SW:
                g.ch = dchar_glyph(DCHAR_DRAW_RIGHT);
                break;
            case TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_NE_SE:
                g.ch = dchar_glyph(DCHAR_DRAW_LEFT);
                break;
            default: break;
            }
        }

        // If we want to show weapons, overwrite all of that.
        item_def* weapon = mi->inv[MSLOT_WEAPON].get();
        if (crawl_state.viewport_weapons && weapon)
        {
            show = *weapon;
            g = _get_item_override(*weapon);
            if (!g.col)
                g.col = weapon->get_colour();
        }

        break;
    }

    case SH_CLOUD:
        ASSERT(cell_cloud);

        show.cls = SH_CLOUD;
        if (coloured)
            g.col = cell.cloud_colour();
        else
            g.col = DARKGRAY;
        break;

    case SH_FEATURE:
        show = cell.feat();
        ASSERT(show);

        g.col = _cell_feat_show_colour(cell, loc, coloured);

        if (cell.item())
        {
            if (Options.feature_item_brand
                && (feat_is_critical(cell.feat())
                    || feat_is_solid(cell.feat())))
            {
                g.col |= COLFLAG_FEATURE_ITEM;
            }
            else if (Options.trap_item_brand && feat_is_trap(cell.feat()))
                g.col |= COLFLAG_TRAP_ITEM;
        }
        break;

    case SH_ITEM:
    {
        const item_info* eitem = cell.item();
        ASSERT(eitem);
        show = *eitem;

        g = _get_item_override(*eitem);

        if (feat_is_water(cell.feat()))
            g.col = _cell_feat_show_colour(cell, loc, coloured);
        else if (!g.col)
            g.col = eitem->get_colour();

        // monster(mimic)-owned items have link = NON_ITEM+1+midx
        if (cell.flags & MAP_MORE_ITEMS)
            g.col |= COLFLAG_ITEM_HEAP;
        break;
    }

    case SH_NOTHING:
    case NUM_SHOW_CLASSES:
        // blackness
        g.ch = ' ';
        return g;
    }

    if (!g.ch)
    {
        const feature_def &fdef = get_feature_def(show);
        g.ch = cell.seen() ? fdef.symbol() : fdef.magic_symbol();
    }

    if (g.col)
        g.col = real_colour(g.col, loc);

    return g;
}

cglyph_t get_cell_glyph(const coord_def& loc, bool only_stationary_monsters,
                        int colour_mode)
{
    // note: this does NOT determine output of the player glyph;
    // that's handled by itself in _draw_player() in view.cc
    const map_cell& cell = env.map_knowledge(loc);
    const show_class cell_show_class =
        get_cell_show_class(cell, only_stationary_monsters);
    return _get_cell_glyph_with_class(cell, loc, cell_show_class, colour_mode);
}

char32_t get_feat_symbol(dungeon_feature_type feat)
{
    return get_feature_def(feat).symbol();
}

char32_t get_item_symbol(show_item_type it)
{
    return get_feature_def(show_type(it)).symbol();
}

cglyph_t get_item_glyph(const item_def& item)
{
    cglyph_t g = _get_item_override(item);
    if (!g.ch)
        g.ch = get_feature_def(show_type(item)).symbol();
    if (!g.col)
        g.col = item.get_colour();
    return g;
}

cglyph_t get_mons_glyph(const monster_info& mi)
{
    monster_type stype = _show_mons_type(mi);
    cglyph_t g;

    g.ch = mons_char(stype);
    g.col = _get_mons_colour(mi);
    g.col = real_colour(g.col);
    return g;
}

string glyph_to_tagstr(const cglyph_t& g)
{
    string col = colour_to_str(g.col);
    string ch = stringize_glyph(g.ch);
    if (g.ch == '<')
        ch += "<";
    return make_stringf("<%s>%s</%s>", col.c_str(), ch.c_str(), col.c_str());
}
