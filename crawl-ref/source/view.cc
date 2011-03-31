/**
 * @file
 * @brief Misc function used to render the dungeon.
**/

#include "AppHdr.h"

#include "view.h"
#include "shout.h"

#include <string.h>
#include <cmath>
#include <sstream>
#include <algorithm>
#include <memory>

#include "externs.h"

#include "map_knowledge.h"
#include "viewchar.h"
#include "viewgeom.h"
#include "showsymb.h"

#include "attitude-change.h"
#include "branch.h"
#include "cio.h"
#include "cloud.h"
#include "clua.h"
#include "colour.h"
#include "coord.h"
#include "coordit.h"
#include "database.h"
#include "delay.h"
#include "dgn-overview.h"
#include "directn.h"
#include "env.h"
#include "exclude.h"
#include "feature.h"
#include "files.h"
#include "fprop.h"
#include "godconduct.h"
#include "godpassive.h"
#include "hints.h"
#include "libutil.h"
#include "macro.h"
#include "message.h"
#include "misc.h"
#include "mon-behv.h"
#include "mon-iter.h"
#include "mon-stuff.h"
#include "mon-util.h"
#include "options.h"
#include "notes.h"
#include "output.h"
#include "player.h"
#include "random.h"
#include "stash.h"
#include "state.h"
#include "stuff.h"
#include "terrain.h"
#include "tilemcache.h"
#include "tilesdl.h"
#include "travel.h"
#include "xom.h"

#ifdef USE_TILE
 #include "tilepick.h"
 #include "tilepick-p.h"
 #include "tileview.h"
#endif

//#define DEBUG_PANE_BOUNDS

crawl_view_geometry crawl_view;

bool handle_seen_interrupt(monster* mons, std::vector<std::string>* msgs_buf)
{
    if (mons_is_unknown_mimic(mons))
        return false;

    activity_interrupt_data aid(mons);
    if (!mons->seen_context.empty())
        aid.context = mons->seen_context;
    // XXX: Hack to make the 'seen' monster spec flag work.
    else if (testbits(mons->flags, MF_WAS_IN_VIEW)
        || testbits(mons->flags, MF_SEEN))
    {
        aid.context = "already seen";
    }
    else
        aid.context = "newly seen";

    seen_monster(mons);

    if (!mons_is_safe(mons)
        && !mons_class_flag(mons->type, M_NO_EXP_GAIN)
            || mons->type == MONS_BALLISTOMYCETE && mons->number > 0)
    {
        return interrupt_activity(AI_SEE_MONSTER, aid, msgs_buf);
    }

    return false;
}

void flush_comes_into_view()
{
    if (!you.turn_is_over
        || (!you_are_delayed() && !crawl_state.is_repeating_cmd()))
    {
        return;
    }

    monster* mon = crawl_state.which_mon_acting();

    if (!mon || !mon->alive() || (mon->flags & MF_WAS_IN_VIEW)
        || !you.can_see(mon))
    {
        return;
    }

    handle_seen_interrupt(mon);
}

void seen_monsters_react()
{
    if (you.duration[DUR_TIME_STEP] || crawl_state.game_is_arena())
        return;

    for (monster_iterator mi(you.get_los()); mi; ++mi)
    {
        if ((mi->asleep() || mons_is_wandering(*mi))
            && check_awaken(*mi))
        {
            behaviour_event(*mi, ME_ALERT, MHITYOU, you.pos(), false);
            handle_monster_shouts(*mi);
        }

        if (!mi->visible_to(&you))
            continue;

        good_god_follower_attitude_change(*mi);
        beogh_follower_convert(*mi);
        slime_convert(*mi);

        // XXX: Hack for triggering Duvessa's going berserk.
        if (mi->props.exists("duvessa_berserk"))
        {
            mi->props.erase("duvessa_berserk");
            mi->go_berserk(true);
        }

        // XXX: Hack for triggering Dowan's spell changes.
        if (mi->props.exists("dowan_upgrade"))
        {
            mi->add_ench(ENCH_HASTE);
            mi->props.erase("dowan_upgrade");
            simple_monster_message(*mi, " seems to find hidden reserves of power!");
        }
    }
}

static std::string _desc_mons_type_map(std::map<monster_type, int> types)
{
    std::string message;
    unsigned int count = 1;
    for (std::map<monster_type, int>::iterator it = types.begin();
         it != types.end(); it++)
    {
        std::string name;
        description_level_type desc;
        if (it->second == 1)
        {
            desc = (it == types.begin() ? DESC_CAP_A
                                                : DESC_NOCAP_A);
        }
        else
            desc = DESC_PLAIN;

        name = mons_type_name(it->first, desc);
        if (it->second > 1)
        {
            name = make_stringf("%d %s", it->second,
                                pluralise(name).c_str());
        }

        message += name;
        if (count == types.size() - 1)
            message += " and ";
        else if (count < types.size())
            message += ", ";
        ++count;
    }
    return make_stringf("%s come into view.", message.c_str());
}

void update_monsters_in_view()
{
    const unsigned int max_msgs = 4;
    int num_hostile = 0;
    std::vector<std::string> msgs;
    std::vector<monster*> monsters;

    for (monster_iterator mi; mi; ++mi)
    {
        if (mons_near(*mi))
        {
            if (mi->attitude == ATT_HOSTILE)
                num_hostile++;

            if (mons_is_unknown_mimic(*mi))
            {
                // For unknown mimics, don't mark as seen,
                // but do mark it as in view for later messaging.
                // FIXME: is this correct?
                mi->flags |= MF_WAS_IN_VIEW;
            }
            else if (mi->visible_to(&you))
            {
                if (handle_seen_interrupt(*mi, &msgs))
                    monsters.push_back(*mi);
                seen_monster(*mi);
            }
            else
                mi->flags &= ~MF_WAS_IN_VIEW;
        }
        else
            mi->flags &= ~MF_WAS_IN_VIEW;

        // If the monster hasn't been seen by the time that the player
        // gets control back then seen_context is out of date.
        if (!you.turn_is_over)
            mi->seen_context.clear();
    }

    if (!msgs.empty())
    {
        unsigned int size = monsters.size();
        std::map<monster_type, int> types;
        std::map<monster_type, int> genera; // This is the plural for genus!
        for (unsigned int i = 0; i < size; ++i)
        {
            types[monsters[i]->type]++;
            genera[mons_genus(monsters[i]->type)]++;
        }

        if (size == 1)
            mpr(msgs[0], MSGCH_WARN);
        else if (types.size() <= max_msgs)
            mpr(_desc_mons_type_map(types), MSGCH_WARN);
        else if (genera.size() <= max_msgs)
            mpr(_desc_mons_type_map(genera), MSGCH_WARN);
        else
            mprf(MSGCH_WARN, "%d monsters come into view.", size);

        bool warning = false;
        std::string warning_msg = "Ashenzari warns you: ";
        for (unsigned int i = 0; i < size; ++i)
        {
            const monster* mon = monsters[i];
            if (!mon->props.exists("ash_id"))
                continue;

            if (warning)
                warning_msg += " ";
            else
                warning = true;

            if (size == 1)
                warning_msg += mon->pronoun(PRONOUN_CAP);
            else if (mon->type == MONS_DANCING_WEAPON)
                warning_msg += "There";
            else if (types[mon->type] == 1)
                warning_msg += mon->full_name(DESC_CAP_THE);
            else
                warning_msg += mon->full_name(DESC_CAP_A);

            warning_msg += " is";
            warning_msg += get_monster_equipment_desc(mon, DESC_IDENTIFIED,
                                                      DESC_NONE);
            warning_msg += ".";
        }
        if (warning)
            mpr(warning_msg, MSGCH_GOD);
    }

    // Xom thinks it's hilarious the way the player picks up an ever
    // growing entourage of monsters while running through the Abyss.
    // To approximate this, if the number of hostile monsters in view
    // is greater than it ever was for this particular trip to the
    // Abyss, Xom is stimulated in proportion to the number of
    // hostile monsters.  Thus if the entourage doesn't grow, then
    // Xom becomes bored.
    if (you.level_type == LEVEL_ABYSS
        && you.attribute[ATTR_ABYSS_ENTOURAGE] < num_hostile)
    {
        you.attribute[ATTR_ABYSS_ENTOURAGE] = num_hostile;
        xom_is_stimulated(16 * num_hostile);
    }
}

// We logically associate a difficulty parameter with each tile on each level,
// to make deterministic magic mapping work.  This function returns the
// difficulty parameters for each tile on the current level, whose difficulty
// is less than a certain amount.
//
// Random difficulties are used in the few cases where we want repeated maps
// to give different results; scrolls and cards, since they are a finite
// resource.
static const FixedArray<uint8_t, GXM, GYM>& _tile_difficulties(bool random)
{
    // We will often be called with the same level parameter and cutoff, so
    // cache this (DS with passive mapping autoexploring could be 5000 calls
    // in a second or so).
    static FixedArray<uint8_t, GXM, GYM> cache;
    static int cache_seed = -1;

    int seed = random ? -1 :
        (static_cast<int>(you.where_are_you) << 8) + you.absdepth0 - 1731813538;

    if (seed == cache_seed && !random)
    {
        return cache;
    }

    if (!random)
    {
        push_rng_state();
        seed_rng(cache_seed);
    }

    cache_seed = seed;

    for (int y = Y_BOUND_1; y <= Y_BOUND_2; ++y)
        for (int x = X_BOUND_1; x <= X_BOUND_2; ++x)
            cache[x][y] = random2(100);

    if (!random)
    {
        pop_rng_state();
    }

    return cache;
}

static std::auto_ptr<FixedArray<bool, GXM, GYM> > _tile_detectability()
{
    std::auto_ptr<FixedArray<bool, GXM, GYM> > map(new FixedArray<bool, GXM, GYM>);

    std::vector<coord_def> flood_from;

    for (int x = X_BOUND_1; x <= X_BOUND_2; ++x)
        for (int y = Y_BOUND_1; y <= Y_BOUND_2; ++y)
        {
            (*map)(coord_def(x,y)) = false;

            if (feat_is_stair(grd[x][y]))
            {
                flood_from.push_back(coord_def(x, y));
            }
        }

    flood_from.push_back(you.pos());

    while (!flood_from.empty())
    {
        coord_def p = flood_from.back();
        flood_from.pop_back();

        if (!in_bounds(p))
            continue;

        if ((*map)(p))
            continue;

        (*map)(p) = true;

        if (grd(p) < DNGN_MINSEE && grd(p) != DNGN_CLOSED_DOOR)
            continue;

        for (int dy = -1; dy <= 1; ++dy)
            for (int dx = -1; dx <= 1; ++dx)
                flood_from.push_back(p + coord_def(dx,dy));
    }

    return map;
}

// Returns true if it succeeded.
bool magic_mapping(int map_radius, int proportion, bool suppress_msg,
                   bool force, bool deterministic, bool circular,
                   coord_def pos)
{
    if (!in_bounds(pos))
        pos = you.pos();

    if (!force
        && (testbits(env.level_flags, LFLAG_NO_MAGIC_MAP)
            || testbits(get_branch_flags(), BFLAG_NO_MAGIC_MAP)))
    {
        if (!suppress_msg)
            mpr("You feel momentarily disoriented.");

        return (false);
    }

    const bool wizard_map = (you.wizard && map_radius == 1000);

    if (!wizard_map)
    {
        if (map_radius > 50)
            map_radius = 50;
        else if (map_radius < 5)
            map_radius = 5;
    }

    // now gradually weaker with distance:
    const int pfar     = dist_range((map_radius * 7) / 10);
    const int very_far = dist_range((map_radius * 9) / 10);

    bool did_map = false;
    int  num_altars        = 0;
    int  num_shops_portals = 0;

    const FixedArray<uint8_t, GXM, GYM>& difficulty =
        _tile_difficulties(!deterministic);

    std::auto_ptr<FixedArray<bool, GXM, GYM> > detectable;

    if (!deterministic)
        detectable = _tile_detectability();

    for (radius_iterator ri(pos, map_radius, circular ? C_ROUND : C_SQUARE);
         ri; ++ri)
    {
        if (!wizard_map)
        {
            int threshold = proportion;

            const int dist = distance(you.pos(), *ri);

            if (dist > very_far)
                threshold = threshold / 3;
            else if (dist > pfar)
                threshold = threshold * 2 / 3;

            if (difficulty(*ri) > threshold)
                continue;
        }

        if (env.map_knowledge(*ri).changed())
            env.map_knowledge(*ri).clear();

        if (!wizard_map && (env.map_knowledge(*ri).seen() || env.map_knowledge(*ri).mapped()))
            continue;

        if (!wizard_map && !deterministic && !((*detectable)(*ri)))
            continue;

        const dungeon_feature_type feat = grd(*ri);

        bool open = true;

        if (feat_is_solid(feat) && !feat_is_closed_door(feat))
        {
            open = false;
            for (adjacent_iterator ai(*ri); ai; ++ai)
            {
                if (map_bounds(*ai) && (!feat_is_opaque(grd(*ai))
                                        || feat_is_closed_door(grd(*ai))))
                {
                    open = true;
                    break;
                }
            }
        }

        if (open)
        {
            if (wizard_map)
                env.map_knowledge(*ri).set_feature(grd(*ri));
            else if (!env.map_knowledge(*ri).feat())
                env.map_knowledge(*ri).set_feature(magic_map_base_feat(grd(*ri)));

            if (wizard_map)
            {
                if (is_notable_terrain(feat))
                    seen_notable_thing(feat, *ri);

                set_terrain_seen(*ri);
#ifdef USE_TILE
                tile_wizmap_terrain(*ri);
#endif
            }
            else
            {
                set_terrain_mapped(*ri);

                if (get_feature_dchar(feat) == DCHAR_ALTAR)
                    num_altars++;
                else if (get_feature_dchar(feat) == DCHAR_ARCH)
                    num_shops_portals++;
            }

            did_map = true;
        }
    }

    if (!suppress_msg)
    {
        mpr(did_map ? "You feel aware of your surroundings."
                    : "You feel momentarily disoriented.");

        std::vector<std::string> sensed;

        if (num_altars > 0)
            sensed.push_back(make_stringf("%d altar%s", num_altars,
                                          num_altars > 1 ? "s" : ""));

        if (num_shops_portals > 0)
        {
            const char* plur = num_shops_portals > 1 ? "s" : "";
            sensed.push_back(make_stringf("%d shop%s/portal%s",
                                          num_shops_portals, plur, plur));
        }

        if (!sensed.empty())
            mpr_comma_separated_list("You sensed ", sensed);
    }

    return (did_map);
}

void fully_map_level()
{
    for (rectangle_iterator ri(1); ri; ++ri)
    {
        bool ok = false;
        for (adjacent_iterator ai(*ri, false); ai; ++ai)
            if (grd(*ai) >= DNGN_MINSEE)
                ok = true;
        if (!ok)
            continue;
        set_terrain_visible(*ri);
        env.map_knowledge(*ri).set_feature(grd(*ri));
        if (igrd(*ri) != NON_ITEM)
            env.map_knowledge(*ri).set_detected_item();
        env.pgrid(*ri) |= FPROP_SEEN_OR_NOEXP;
    }
}

// Is the given monster near (in LOS of) the player?
bool mons_near(const monster* mons)
{
    if (crawl_state.game_is_arena() || crawl_state.arena_suspended)
        return (true);
    return (you.see_cell(mons->pos()));
}

bool mon_enemies_around(const monster* mons)
{
    // If the monster has a foe, return true.
    if (mons->foe != MHITNOT && mons->foe != MHITYOU)
        return (true);

    if (crawl_state.game_is_arena())
    {
        // If the arena-mode code in _handle_behaviour() hasn't set a foe then
        // we don't have one.
        return (false);
    }
    else if (mons->wont_attack())
    {
        // Additionally, if an ally is nearby and *you* have a foe,
        // consider it as the ally's enemy too.
        return (mons_near(mons) && there_are_monsters_nearby(true));
    }
    else
    {
        // For hostile monster* you* are the main enemy.
        return (mons_near(mons));
    }
}

// Returns a string containing a representation of the map.  Leading and
// trailing spaces are trimmed from each line.  Leading and trailing empty
// lines are also snipped.
std::string screenshot()
{
    std::vector<std::string> lines(crawl_view.viewsz.y);
    unsigned int lsp = GXM;
    for (int y = 0; y < crawl_view.viewsz.y; y++)
    {
        std::string line;
        for (int x = 0; x < crawl_view.viewsz.x; x++)
        {
            // in grid coords
            const coord_def gc = view2grid(crawl_view.viewp +
                                     coord_def(x, y));
            ucs_t ch =
                  (!map_bounds(gc))             ? ' ' :
                  (gc == you.pos())             ? mons_char(you.symbol)
                                                : get_cell_glyph(gc).ch;
            line += stringize_glyph(ch);
        }
        // right-trim the line
        for (int x = line.length() - 1; x >= 0; x--)
            if (line[x] == ' ')
                line.erase(x);
        // see how much it can be left-trimmed
        for (unsigned int x = 0; x < line.length(); x++)
            if (line[x] != ' ')
            {
                if (lsp > x)
                    lsp = x;
                break;
            }
        lines[y] = line;
    }

    for (unsigned int y = 0; y < lines.size(); y++)
        lines[y].erase(0, lsp); // actually trim from the left
    while (!lines.empty() && lines.back().empty())
        lines.pop_back();       // then from the bottom

    std::ostringstream ss;
    unsigned int y = 0;
    for (y = 0; y < lines.size() && lines[y].empty(); y++)
        ;                       // ... and from the top
    for (; y < lines.size(); y++)
        ss << lines[y] << "\n";
    return (ss.str());
}

static int _viewmap_flash_colour()
{
    if (you.attribute[ATTR_SHADOWS])
        return (DARKGREY);
    else if (you.berserk())
        return (RED);

    return (BLACK);
}

// Updates one square of the view area. Should only be called for square
// in LOS.
void view_update_at(const coord_def &pos)
{
    if (pos == you.pos())
        return;

    show_update_at(pos);

#ifndef USE_TILE
    if (!env.map_knowledge(pos).visible())
        return;
    glyph g = get_cell_glyph(pos);

    int flash_colour = you.flash_colour == BLACK
        ? _viewmap_flash_colour()
        : you.flash_colour;
    int mons = env.map_knowledge(pos).monster();
    int cell_colour =
        flash_colour &&
        (mons == MONS_NO_MONSTER || mons_class_is_firewood(mons) ||
         !you.berserk())
            ? real_colour(flash_colour)
            : g.col;

    const coord_def vp = grid2view(pos);
    cgotoxy(vp.x, vp.y, GOTO_DNGN);
    put_colour_ch(cell_colour, g.ch);

    // Force colour back to normal, else clrscr() will flood screen
    // with this colour on DOS.
    textattr(LIGHTGREY);
#endif
}

#ifndef USE_TILE
void flash_monster_colour(const monster* mon, uint8_t fmc_colour,
                          int fmc_delay)
{
    if (you.can_see(mon))
    {
        uint8_t old_flash_colour = you.flash_colour;
        coord_def c(mon->pos());

        you.flash_colour = fmc_colour;
        view_update_at(c);

        update_screen();
        delay(fmc_delay);

        you.flash_colour = old_flash_colour;
        view_update_at(c);
        update_screen();
    }
}
#endif

bool view_update()
{
    if (you.num_turns > you.last_view_update)
    {
        viewwindow();
        return (true);
    }
    return (false);
}

void flash_view(uint8_t colour)
{
    you.flash_colour = colour;
    viewwindow(false);
}

void flash_view_delay(uint8_t colour, int flash_delay)
{
    flash_view(colour);
    // Scale delay to match change in arena_delay.
    if (crawl_state.game_is_arena())
    {
        flash_delay *= Options.arena_delay;
        flash_delay /= 600;
    }

    delay(flash_delay);
}

static void _debug_pane_bounds()
{
#ifdef DEBUG_PANE_BOUNDS
    // Doesn't work for HUD because print_stats() overwrites it.

    if (crawl_view.mlistsz.y > 0)
    {
        textcolor(WHITE);
        cgotoxy(1,1, GOTO_MLIST);
        cprintf("+   L");
        cgotoxy(crawl_view.mlistsz.x-4, crawl_view.mlistsz.y, GOTO_MLIST);
        cprintf("L   +");
    }

    cgotoxy(1,1, GOTO_STAT);
    cprintf("+  H");
    cgotoxy(crawl_view.hudsz.x-3, crawl_view.hudsz.y, GOTO_STAT);
    cprintf("H  +");

    cgotoxy(1,1, GOTO_MSG);
    cprintf("+ M");
    cgotoxy(crawl_view.msgsz.x-2, crawl_view.msgsz.y, GOTO_MSG);
    cprintf("M +");

    cgotoxy(crawl_view.viewp.x, crawl_view.viewp.y);
    cprintf("+V");
    cgotoxy(crawl_view.viewp.x+crawl_view.viewsz.x-2,
            crawl_view.viewp.y+crawl_view.viewsz.y-1);
    cprintf("V+");
    textcolor(LIGHTGREY);
#endif
}

enum update_flags
{
    UF_AFFECT_EXCLUDES = (1 << 0),
    UF_ADDED_EXCLUDE   = (1 << 1),
};

// Do various updates when the player sees a cell. Returns whether
// exclusion LOS might have been affected.
static int player_view_update_at(const coord_def &gc)
{
    maybe_remove_autoexclusion(gc);
    int ret = 0;

    // Set excludes in a radius of 1 around harmful clouds genereated
    // by neither monsters nor the player.
    const int cloudidx = env.cgrid(gc);
    if (cloudidx != EMPTY_CLOUD && !crawl_state.game_is_arena())
    {
        cloud_struct &cl   = env.cloud[cloudidx];
        cloud_type   ctype = cl.type;

        bool did_exclude = false;
        if (cl.whose  == KC_OTHER
            && cl.killer == KILL_MISC
            && is_damaging_cloud(cl.type, false))
        {
            // Steam clouds are less dangerous than the other ones,
            // so don't exclude the neighbour cells.
            const int size = (ctype == CLOUD_STEAM ? 0 : 1);
            bool was_exclusion = is_exclude_root(gc);
            set_exclude(gc, size, false, false, true);
            if (!did_exclude && !was_exclusion)
                ret |= UF_ADDED_EXCLUDE;
        }
    }

    // Print hints mode messages for features in LOS.
    if (crawl_state.game_is_hints())
        hints_observe_cell(gc);

    if (env.map_knowledge(gc).changed() || !env.map_knowledge(gc).seen())
        ret |= UF_AFFECT_EXCLUDES;

    set_terrain_visible(gc);

    if (!(env.pgrid(gc) & FPROP_SEEN_OR_NOEXP))
    {
        env.pgrid(gc) |= FPROP_SEEN_OR_NOEXP;
        if (!crawl_state.game_is_arena())
        {
            const int density = env.density ? env.density : 2000;
            did_god_conduct(DID_EXPLORATION, density);
            you.exploration += div_rand_round(1<<16, density);
        }
    }

#ifdef USE_TILE
    const coord_def ep = grid2show(gc);
    if (!player_in_mappable_area())
        return (ret); // XXX: is this necessary?

    // We remove any references to mcache when
    // writing to the background.
    if (Options.clean_map)
        env.tile_bk_fg(gc) = get_clean_map_idx(env.tile_fg(ep));
    else
        env.tile_bk_fg(gc) = env.tile_fg(ep);
    env.tile_bk_bg(gc) = env.tile_bg(ep);
#endif

    return (ret);
}

static void player_view_update()
{
    std::vector<coord_def> update_excludes;
    bool need_update = false;
    for (radius_iterator ri(you.get_los()); ri; ++ri)
    {
        int flags = player_view_update_at(*ri);
        if (flags & UF_AFFECT_EXCLUDES)
            update_excludes.push_back(*ri);
        if (flags & UF_ADDED_EXCLUDE)
            need_update = true;
    }
    // Update exclusion LOS for possibly affected excludes.
    update_exclusion_los(update_excludes);
    // Catch up on deferred updates for cloud excludes.
    if (need_update)
        deferred_exclude_update();
}

static void _draw_out_of_bounds(screen_cell_t *cell)
{
    cell->glyph  = ' ';
    cell->colour = DARKGREY;
#ifdef USE_TILE
    cell->tile.fg = 0;
    cell->tile.bg = tileidx_out_of_bounds(you.where_are_you);
#endif
}

static void _draw_outside_los(screen_cell_t *cell, const coord_def &gc)
{
    // Outside the env.show area.
    glyph g = get_cell_glyph(gc, Options.clean_map);
    cell->glyph  = g.ch;
    cell->colour = g.col;

#ifdef USE_TILE
    tileidx_out_of_los(&cell->tile.fg, &cell->tile.bg, gc);
#endif
}

static void _draw_player(screen_cell_t *cell,
                         const coord_def &gc, const coord_def &ep,
                         bool anim_updates)
{
    // Player overrides everything in cell.
    cell->glyph  = mons_char(you.symbol);
    cell->colour = mons_class_colour(you.symbol);
    if (you.swimming())
    {
        if (grd(gc) == DNGN_DEEP_WATER)
            cell->colour = BLUE;
        else
            cell->colour = CYAN;
    }
    if (Options.use_fake_player_cursor)
        cell->colour |= COLFLAG_REVERSE;

#ifdef USE_TILE
    cell->tile.fg = env.tile_fg(ep) = tileidx_player();
    cell->tile.bg = env.tile_bg(ep);
    if (anim_updates)
        tile_apply_animations(cell->tile.bg, &env.tile_flv(gc));
#else
    UNUSED(anim_updates);
#endif
}

static void _draw_los(screen_cell_t *cell,
                      const coord_def &gc, const coord_def &ep,
                      bool anim_updates)
{
    glyph g = get_cell_glyph(gc);
    cell->glyph  = g.ch;
    cell->colour = g.col;

#ifdef USE_TILE
    cell->tile.fg = env.tile_fg(ep);
    cell->tile.bg = env.tile_bg(ep);
    if (anim_updates)
        tile_apply_animations(cell->tile.bg, &env.tile_flv(gc));
#else
    UNUSED(anim_updates);
#endif
}

static bool _show_terrain = false;

//---------------------------------------------------------------
//
// Draws the main window using the character set returned
// by get_show_glyph().
//
// If show_updates is set, env.show and dependent structures
// are updated. Should be set if anything in view has changed.
//---------------------------------------------------------------
void viewwindow(bool show_updates)
{
    if (you.duration[DUR_TIME_STEP])
        return;

    screen_cell_t *cell(crawl_view.vbuf);

    // Update the animation of cells only once per turn.
    const bool anim_updates = (you.last_view_update != you.num_turns);
    // Except for elemental colours, which should be updated every refresh.
    you.frame_no++;

#ifdef USE_TILE
    tiles.clear_text_tags(TAG_NAMED_MONSTER);

    if (show_updates)
        mcache.clear_nonref();
#endif

    if (show_updates || _show_terrain)
    {
        if (!player_in_mappable_area())
        {
            env.map_knowledge.init(map_cell());
            ash_detect_portals(false);
        }

#ifdef USE_TILE
        tile_draw_floor();
        tile_draw_rays(true);
        tiles.clear_overlays();
#endif

        show_init(_show_terrain);
    }

    if (show_updates)
        player_view_update();

    bool run_dont_draw = you.running && Options.travel_delay < 0
                && (!you.running.is_explore() || Options.explore_delay < 0);

    if (run_dont_draw || you.asleep())
    {
        // Reset env.show if we munged it.
        if (_show_terrain)
            show_init();
        return;
    }

    cursor_control cs(false);

    int flash_colour = you.flash_colour;
    if (flash_colour == BLACK)
        flash_colour = _viewmap_flash_colour();

    const coord_def tl = coord_def(1, 1);
    const coord_def br = crawl_view.viewsz;
    for (rectangle_iterator ri(tl, br); ri; ++ri)
    {
#ifdef USE_TILE
        cell->tile.clear();
#endif

        // in grid coords
        const coord_def gc = view2grid(*ri);
        const coord_def ep = grid2show(gc);

        if (!map_bounds(gc))
            _draw_out_of_bounds(cell);
        else if (!crawl_view.in_los_bounds_g(gc))
            _draw_outside_los(cell, gc);
        else if (gc == you.pos() && you.on_current_level && !_show_terrain
                 && !crawl_state.game_is_arena()
                 && !crawl_state.arena_suspended)
        {
            _draw_player(cell, gc, ep, anim_updates);
        }
        else if (you.see_cell(gc) && you.on_current_level)
            _draw_los(cell, gc, ep, anim_updates);
        else
            _draw_outside_los(cell, gc);

        cell->flash_colour = BLACK;

        // Alter colour if flashing the characters vision.
        if (flash_colour)
        {
            if (you.see_cell(gc))
            {
#ifdef USE_TILE
                cell->colour = real_colour(flash_colour);
#else
                monster_type mons = env.map_knowledge(gc).monster();
                if (mons == MONS_NO_MONSTER || mons_class_is_firewood(mons) ||
                    !you.berserk())
                {
                    cell->colour = real_colour(flash_colour);
                }
#endif
            }
            else
            {
                cell->colour = DARKGREY;
            }
            cell->flash_colour = cell->colour;
        }
        else if (crawl_state.darken_range >= 0)
        {
            const int rsq = (crawl_state.darken_range
                             * crawl_state.darken_range) + 1;
            bool out_of_range = distance(you.pos(), gc) > rsq
                                || !you.see_cell(gc);
            if (out_of_range)
            {
                cell->colour = DARKGREY;
#ifdef USE_TILE
                if (you.see_cell(gc))
                    cell->tile.bg |= TILE_FLAG_OOR;
#endif
            }
        }
#ifdef USE_TILE
        // Grey out grids that cannot be reached due to beholders.
        else if (you.get_beholder(gc))
            cell->tile.bg |= TILE_FLAG_OOR;

        else if (you.get_fearmonger(gc))
            cell->tile.bg |= TILE_FLAG_OOR;

        tile_apply_properties(gc, cell->tile);
#endif

        cell++;
    }

    // Leaving it this way because short flashes can occur in long ones,
    // and this simply works without requiring a stack.
    you.flash_colour = BLACK;
    you.last_view_update = you.num_turns;
#ifndef USE_TILE
    puttext(crawl_view.viewp.x, crawl_view.viewp.y, crawl_view.vbuf);
    update_monster_pane();
#else
    tiles.set_need_redraw(you.running ? Options.tile_runrest_rate : 0);
    tiles.load_dungeon(crawl_view.vbuf, crawl_view.vgrdc);
    tiles.update_tabs();
#endif

    // Reset env.show if we munged it.
    if (_show_terrain)
        show_init();

    _debug_pane_bounds();
}

void toggle_show_terrain()
{
    _show_terrain = !_show_terrain;
    if (_show_terrain)
        mpr("Showing terrain only.");
    else
        mpr("Returning to normal view.");
}

void reset_show_terrain()
{
    _show_terrain = false;
}

////////////////////////////////////////////////////////////////////////////
// Term resize handling (generic).

void handle_terminal_resize(bool redraw)
{
    crawl_state.terminal_resized = false;

    if (crawl_state.terminal_resize_handler)
        (*crawl_state.terminal_resize_handler)();
    else
        crawl_view.init_geometry();

    if (redraw)
        redraw_screen();
}
