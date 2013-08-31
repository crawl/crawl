#include "AppHdr.h"

#include "map_knowledge.h"

#include "coordit.h"
#include "dgn-overview.h"
#include "dgnevent.h"
#include "directn.h"
#include "env.h"
#include "feature.h"
#include "los.h"
#include "mon-util.h"
#include "notes.h"
#include "options.h"
#include "religion.h"
#include "show.h"
#include "terrain.h"
#ifdef USE_TILE
 #include "tilepick.h"
 #include "tileview.h"
#endif
#include "view.h"

// Used to mark dug out areas, unset when terrain is seen or mapped again.
void set_terrain_changed(int x, int y)
{
    const coord_def p = coord_def(x, y);
    env.map_knowledge[x][y].flags |= MAP_CHANGED_FLAG;

    dungeon_events.fire_position_event(DET_FEAT_CHANGE, p);

    los_terrain_changed(p);

    for (orth_adjacent_iterator ai(p); ai; ++ai)
        if (actor *act = actor_at(*ai))
            act->check_clinging(false, feat_is_door(grd(p)));
}

void set_terrain_mapped(int x, int y)
{
    const coord_def gc(x, y);
    map_cell* cell = &env.map_knowledge(gc);
    cell->flags &= (~MAP_CHANGED_FLAG);
    cell->flags |= MAP_MAGIC_MAPPED_FLAG;
#ifdef USE_TILE
    tiles.update_minimap(gc);
#endif
}

int count_detected_mons()
{
    int count = 0;
    for (rectangle_iterator ri(BOUNDARY_BORDER - 1); ri; ++ri)
    {
        // Don't expose new dug out areas:
        // Note: assumptions are being made here about how
        // terrain can change (eg it used to be solid, and
        // thus monster/item free).
        if (env.map_knowledge(*ri).changed())
            continue;

        if (env.map_knowledge(*ri).detected_monster())
            count++;
    }

    return count;
}

void clear_map(bool clear_items, bool clear_mons)
{
    for (rectangle_iterator ri(BOUNDARY_BORDER - 1); ri; ++ri)
    {
        const coord_def p = *ri;
        map_cell& cell = env.map_knowledge(p);
        if (!cell.known() || cell.visible())
            continue;

        cell.clear_cloud();

        if (clear_items)
            cell.clear_item();

        if (clear_mons && !mons_class_is_stationary(cell.monster()))
            cell.clear_monster();

#ifdef USE_TILE
        tile_reset_fg(p);
#endif
    }
}

static void _automap_from(int x, int y, int mutated)
{
    if (mutated)
    {
        magic_mapping(8 * mutated,
                      you_worship(GOD_ASHENZARI) ? 25 + you.piety / 8 : 25,
                      true, you_worship(GOD_ASHENZARI),
                      true, coord_def(x,y));
    }
}

static int _map_quality()
{
    int passive = player_mutation_level(MUT_PASSIVE_MAPPING);
    // the explanation of this 51 vs max_piety of 200 is left as
    // an exercise to the reader
    if (you_worship(GOD_ASHENZARI) && !player_under_penance())
        passive = max(passive, you.piety / 51);
    return passive;
}

void reautomap_level()
{
    int passive = _map_quality();

    for (int x = X_BOUND_1; x <= X_BOUND_2; ++x)
        for (int y = Y_BOUND_1; y <= Y_BOUND_2; ++y)
            if (env.map_knowledge[x][y].flags & MAP_SEEN_FLAG)
                _automap_from(x, y, passive);
}

void set_terrain_seen(int x, int y)
{
    const dungeon_feature_type feat = grd[x][y];
    map_cell* cell = &env.map_knowledge[x][y];

    // First time we've seen a notable feature.
    if (!(cell->flags & MAP_SEEN_FLAG))
    {
        _automap_from(x, y, _map_quality());

        if (!is_boring_terrain(feat))
        {
            coord_def pos(x, y);
            string desc = feature_description_at(pos, false, DESC_A);
            take_note(Note(NOTE_SEEN_FEAT, 0, 0, desc.c_str()));
        }
    }

    cell->flags &= (~MAP_CHANGED_FLAG);
    cell->flags |= MAP_SEEN_FLAG;

#ifdef USE_TILE
    coord_def pos(x, y);
    tiles.update_minimap(pos);
#endif
}

void set_terrain_visible(const coord_def &c)
{
    map_cell* cell = &env.map_knowledge(c);
    set_terrain_seen(c);
    if (!(cell->flags & MAP_VISIBLE_FLAG))
    {
        cell->flags |= MAP_VISIBLE_FLAG;
        env.visible.insert(c);
    }
    cell->flags &= ~(MAP_DETECTED_MONSTER | MAP_DETECTED_ITEM);
}

void clear_terrain_visibility()
{
    for (set<coord_def>::iterator i = env.visible.begin(); i != env.visible.end(); ++i)
        env.map_knowledge(*i).flags &= ~MAP_VISIBLE_FLAG;
    env.visible.clear();
}

void map_cell::set_detected_item()
{
    clear_item();
    flags |= MAP_DETECTED_ITEM;
    _item = new item_info();
    _item->base_type = OBJ_DETECTED;
    _item->colour    = Options.detected_item_colour;
}

static bool _floor_mf(map_feature mf)
{
    return mf == MF_FLOOR || mf == MF_WATER || mf == MF_LAVA;
}

map_feature get_cell_map_feature(const map_cell& cell)
{
    map_feature mf = MF_SKIP;
    bool mf_mons_no_exp = false;
    if (cell.invisible_monster())
        mf = MF_MONS_HOSTILE;
    else if (cell.monster() != MONS_NO_MONSTER)
    {
        switch (cell.monsterinfo() ? cell.monsterinfo()->attitude : ATT_HOSTILE)
        {
        case ATT_FRIENDLY:
            mf = MF_MONS_FRIENDLY;
            break;
        case ATT_GOOD_NEUTRAL:
            mf = MF_MONS_PEACEFUL;
            break;
        case ATT_NEUTRAL:
        case ATT_STRICT_NEUTRAL:
            mf = MF_MONS_NEUTRAL;
            break;
        case ATT_HOSTILE:
        default:
            if (mons_class_flag(cell.monster(), M_NO_EXP_GAIN))
                mf_mons_no_exp = true;
            else
                mf = MF_MONS_HOSTILE;
            break;
        }
    }
    else if (cell.cloud())
    {
        show_type show;
        show.cls = SH_CLOUD;
        mf = get_feature_def(show).minimap;
    }

    if (mf == MF_SKIP)
        mf = get_feature_def(cell.feat()).minimap;
    if ((mf == MF_SKIP || _floor_mf(mf)) && cell.item())
        mf = get_feature_def(*cell.item()).minimap;
    if ((mf == MF_SKIP || _floor_mf(mf)) && mf_mons_no_exp)
        mf = MF_MONS_NO_EXP;
    if (mf == MF_SKIP)
        mf = MF_UNSEEN;

    if (mf == MF_WALL || mf == MF_FLOOR)
    {
        if (cell.known() && !cell.seen()
            || cell.detected_item()
            || cell.detected_monster())
        {
            mf = (mf == MF_WALL) ? MF_MAP_WALL : MF_MAP_FLOOR;
        }
    }

    return mf;
}
