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
#include "showsymb.h"

#include "attitude-change.h"
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
#include "state.h"
#include "stuff.h"
#include "target.h"
#include "terrain.h"
#include "tilemcache.h"
#include "traps.h"
#include "travel.h"
#include "viewmap.h"
#include "xom.h"

#ifdef USE_TILE
 #include "tilepick.h"
 #include "tilepick-p.h"
 #include "tileview.h"
#endif

//#define DEBUG_PANE_BOUNDS

crawl_view_geometry crawl_view;

bool handle_seen_interrupt(monster* mons, vector<string>* msgs_buf)
{
    activity_interrupt_data aid(mons);
    if (mons->seen_context)
        aid.context = mons->seen_context;
    // XXX: Hack to make the 'seen' monster spec flag work.
    else if (testbits(mons->flags, MF_WAS_IN_VIEW)
             || testbits(mons->flags, MF_SEEN))
    {
        aid.context = SC_ALREADY_SEEN;
    }
    else
        aid.context = SC_NEWLY_SEEN;

    if (!mons_is_safe(mons)
        && !mons_class_flag(mons->type, M_NO_EXP_GAIN)
           || mons->type == MONS_BALLISTOMYCETE && mons->number > 0)
    {
        return interrupt_activity(AI_SEE_MONSTER, aid, msgs_buf);
    }

    seen_monster(mons);

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
            && check_awaken(*mi)
#ifdef EUCLIDEAN
               || you.prev_move.abs() == 2 && x_chance_in_y(2, 5)
                  && check_awaken(*mi)
#endif
           )
        {
            behaviour_event(*mi, ME_ALERT, &you, you.pos(), false);

            // That might have caused a pacified monster to leave the level.
            if (!(*mi)->alive())
                continue;

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

static string _desc_mons_type_map(map<monster_type, int> types)
{
    string message;
    unsigned int count = 1;
    for (map<monster_type, int>::iterator it = types.begin();
         it != types.end(); ++it)
    {
        string name;
        description_level_type desc;
        if (it->second == 1)
            desc = DESC_A;
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

/*
 * Monster list simplification
 *
 * When too many monsters come into view at once, we group the ones with the
 * same genus, starting with the most represented genus.
 *
 * @param types monster types and the number of monster for each type.
 * @param genera monster genera and the number of monster for each genus.
 */
static void _genus_factoring(map<monster_type, int> &types,
                             map<monster_type, int> &genera)
{
    monster_type genus = MONS_NO_MONSTER;
    int num = 0;
    map<monster_type, int>::iterator it;
    // Find the most represented genus.
    for (it = genera.begin(); it != genera.end(); ++it)
        if (it->second > num)
        {
            genus = it->first;
            num = it->second;
        }

    // The most represented genus has a single member.
    // No more factoring is possible, we're done.
    if (num == 1)
    {
        genera.clear();
        return;
    }

    genera.erase(genus);
    it = types.begin();
    do
    {
        if (mons_genus(it->first) != genus)
        {
            ++it;
            continue;
        }

        // This genus has a single monster type. Can't factor.
        if (it->second == num)
            return;

        types.erase(it++);

    } while (it != types.end());

    types[genus] = num;
}

void update_monsters_in_view()
{
    const unsigned int max_msgs = 4;
    int num_hostile = 0;
    vector<string> msgs;
    vector<monster*> monsters;

    for (monster_iterator mi; mi; ++mi)
    {
        if (mons_near(*mi))
        {
            if (mi->attitude == ATT_HOSTILE)
                num_hostile++;

            if (mi->visible_to(&you))
            {
                if (handle_seen_interrupt(*mi, &msgs))
                    monsters.push_back(*mi);
                seen_monster(*mi);
            }
            else
                mi->flags &= ~MF_WAS_IN_VIEW;
        }
        else if (!you.turn_is_over)
        {
            if (mi->flags & MF_WAS_IN_VIEW)
            {
                // Reset client id so the player doesn't know (for sure) he
                // has seen this monster before when it reappears.
                mi->reset_client_id();
            }

            mi->flags &= ~MF_WAS_IN_VIEW;

            // If the monster hasn't been seen by the time that the player
            // gets control back then seen_context is out of date.
            mi->seen_context = SC_NONE;
        }
    }

    if (!msgs.empty())
    {
        unsigned int size = monsters.size();
        map<monster_type, int> types;
        map<monster_type, int> genera; // This is the plural for genus!
        for (unsigned int i = 0; i < size; ++i)
        {
            monster_type type;
            if (monsters[i]->props.exists("mislead_as") && you.misled())
                type = monsters[i]->get_mislead_type();
            else
                type = monsters[i]->type;

            types[type]++;
            genera[mons_genus(type)]++;
        }

        if (size == 1)
            mpr(msgs[0], MSGCH_WARN);
        else
        {
            while (types.size() > max_msgs && !genera.empty())
                _genus_factoring(types, genera);
            mpr(_desc_mons_type_map(types), MSGCH_WARN);
        }

        bool warning = false;
        string warning_msg = "Ashenzari warns you:";
        warning_msg += " ";
        for (unsigned int i = 0; i < size; ++i)
        {
            const monster* mon = monsters[i];
            if (!mon->props.exists("ash_id"))
                continue;

            monster_info mi(mon);

            if (warning)
                warning_msg += " ";
            else
                warning = true;

            string monname;
            if (size == 1)
                monname = mon->pronoun(PRONOUN_SUBJECTIVE);
            else if (mon->type == MONS_DANCING_WEAPON)
                monname = "There";
            else if (types[mon->type] == 1)
                monname = mon->full_name(DESC_THE);
            else
                monname = mon->full_name(DESC_A);
            warning_msg += uppercase_first(monname);

            warning_msg += " is";
            warning_msg += get_monster_equipment_desc(mi, DESC_IDENTIFIED,
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
    if (player_in_branch(BRANCH_ABYSS)
        && you.attribute[ATTR_ABYSS_ENTOURAGE] < num_hostile)
    {
        you.attribute[ATTR_ABYSS_ENTOURAGE] = num_hostile;
        xom_is_stimulated(12 * num_hostile);
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

    if (random)
    {
        cache_seed = -1;
        for (int y = Y_BOUND_1; y <= Y_BOUND_2; ++y)
            for (int x = X_BOUND_1; x <= X_BOUND_2; ++x)
                cache[x][y] = random2(100);
        return cache;
    }

    // must not produce the magic value (-1)
    int seed = (static_cast<int>(you.where_are_you) << 8) + you.depth
             ^ you.game_seeds[SEED_PASSIVE_MAP] & 0x7fffffff;

    if (seed == cache_seed)
        return cache;

    cache_seed = seed;

    for (int y = Y_BOUND_1; y <= Y_BOUND_2; ++y)
        for (int x = X_BOUND_1; x <= X_BOUND_2; ++x)
            cache[x][y] = hash_rand(100, seed, y * GXM + x);

    return cache;
}

// Returns true if it succeeded.
bool magic_mapping(int map_radius, int proportion, bool suppress_msg,
                   bool force, bool deterministic,
                   coord_def pos)
{
    if (!in_bounds(pos))
        pos = you.pos();

    if (!force && testbits(env.level_flags, LFLAG_NO_MAP))
    {
        if (!suppress_msg)
            canned_msg(MSG_DISORIENTED);

        return false;
    }

    const bool wizard_map = (you.wizard && map_radius == 1000);

    if (map_radius < 5)
        map_radius = 5;

    // now gradually weaker with distance:
    const int pfar     = dist_range((map_radius * 7) / 10);
    const int very_far = dist_range((map_radius * 9) / 10);

    bool did_map = false;
    int  num_altars        = 0;
    int  num_shops_portals = 0;

    const FixedArray<uint8_t, GXM, GYM>& difficulty =
        _tile_difficulties(!deterministic);

    for (radius_iterator ri(pos, map_radius, C_ROUND);
         ri; ++ri)
    {
        if (!wizard_map)
        {
            int threshold = proportion;

            const int dist = distance2(you.pos(), *ri);

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
            {
                env.map_knowledge(*ri).set_feature(grd(*ri), 0,
                    feat_is_trap(grd(*ri)) ? get_trap_type(*ri)
                                           : TRAP_UNASSIGNED);
            }
            else if (!env.map_knowledge(*ri).feat())
                env.map_knowledge(*ri).set_feature(magic_map_base_feat(grd(*ri)));
            if (emphasise(*ri))
                env.map_knowledge(*ri).flags |= MAP_EMPHASIZE;

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
        if (did_map)
            mpr("You feel aware of your surroundings.");
        else
            canned_msg(MSG_DISORIENTED);

        vector<string> sensed;

        if (num_altars > 0)
        {
            sensed.push_back(make_stringf("%d altar%s", num_altars,
                                          num_altars > 1 ? "s" : ""));
        }

        if (num_shops_portals > 0)
        {
            const char* plur = num_shops_portals > 1 ? "s" : "";
            sensed.push_back(make_stringf("%d shop%s/portal%s",
                                          num_shops_portals, plur, plur));
        }

        if (!sensed.empty())
            mpr_comma_separated_list("You sensed ", sensed);
    }

    return did_map;
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
        env.map_knowledge(*ri).set_feature(grd(*ri), 0,
            feat_is_trap(grd(*ri)) ? get_trap_type(*ri) : TRAP_UNASSIGNED);
        set_terrain_seen(*ri);
#ifdef USE_TILE
        tile_wizmap_terrain(*ri);
#endif
        if (igrd(*ri) != NON_ITEM)
            env.map_knowledge(*ri).set_detected_item();
        env.pgrid(*ri) |= FPROP_SEEN_OR_NOEXP;
    }
}

// Is the given monster near (in LOS of) the player?
bool mons_near(const monster* mons)
{
    if (crawl_state.game_is_arena() || crawl_state.arena_suspended)
        return true;
    return (you.see_cell(mons->pos()));
}

bool mon_enemies_around(const monster* mons)
{
    // If the monster has a foe, return true.
    if (mons->foe != MHITNOT && mons->foe != MHITYOU)
        return true;

    if (crawl_state.game_is_arena())
    {
        // If the arena-mode code in _handle_behaviour() hasn't set a foe then
        // we don't have one.
        return false;
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
        return mons_near(mons);
    }
}

// Returns a string containing a representation of the map.  Leading and
// trailing spaces are trimmed from each line.  Leading and trailing empty
// lines are also snipped.
string screenshot()
{
    vector<string> lines(crawl_view.viewsz.y);
    unsigned int lsp = GXM;
    for (int y = 0; y < crawl_view.viewsz.y; y++)
    {
        string line;
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
            else
                break;
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

    ostringstream ss;
    unsigned int y = 0;
    for (y = 0; y < lines.size() && lines[y].empty(); y++)
        ;                       // ... and from the top
    for (; y < lines.size(); y++)
        ss << lines[y] << "\n";
    return ss.str();
}

int viewmap_flash_colour()
{
    if (you.attribute[ATTR_SHADOWS])
        return LIGHTGREY;
    else if (you.berserk())
        return RED;

    return BLACK;
}

// Updates one square of the view area. Should only be called for square
// in LOS.
void view_update_at(const coord_def &pos)
{
    if (pos == you.pos())
        return;

    show_update_at(pos);

#ifndef USE_TILE_LOCAL
    if (!env.map_knowledge(pos).visible())
        return;
    cglyph_t g = get_cell_glyph(pos);

    int flash_colour = you.flash_colour == BLACK
        ? viewmap_flash_colour()
        : you.flash_colour;
    monster_type mons = env.map_knowledge(pos).monster();
    int cell_colour =
        flash_colour &&
        (mons == MONS_NO_MONSTER || mons_class_is_firewood(mons))
            ? real_colour(flash_colour)
            : g.col;

    const coord_def vp = grid2view(pos);
    // Don't draw off-screen.
    if (crawl_view.in_viewport_v(vp))
    {
        cgotoxy(vp.x, vp.y, GOTO_DNGN);
        put_colour_ch(cell_colour, g.ch);
    }

    // Force colour back to normal, else clrscr() will flood screen
    // with this colour on DOS.
    textcolor(LIGHTGREY);

#endif
#ifdef USE_TILE_WEB
    tiles.mark_for_redraw(pos);
#endif
}

#ifndef USE_TILE_LOCAL
void flash_monster_colour(const monster* mon, colour_t fmc_colour,
                          int fmc_delay)
{
    if (you.can_see(mon))
    {
        colour_t old_flash_colour = you.flash_colour;
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
        return true;
    }
    return false;
}

void flash_view(colour_t colour, targetter *where)
{
    you.flash_colour = colour;
    you.flash_where = where;
    viewwindow(false);
}

void flash_view_delay(colour_t colour, int flash_delay, targetter *where)
{
    flash_view(colour, where);
    // Scale delay to match change in arena_delay.
    if (crawl_state.game_is_arena())
    {
        flash_delay *= Options.arena_delay;
        flash_delay /= 600;
    }

    delay(flash_delay);
    flash_view(0);
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
        if (!cl.temporary() && is_damaging_cloud(cl.type, false))
        {
            int size;

            // Steam clouds are less dangerous than the other ones,
            // so don't exclude the neighbour cells.
            if (ctype == CLOUD_STEAM && cl.exclusion_radius() == 1)
                size = 0;
            else
                size = cl.exclusion_radius();

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

    // We remove any references to mcache when
    // writing to the background.
    if (Options.clean_map)
        env.tile_bk_fg(gc) = get_clean_map_idx(env.tile_fg(ep));
    else
        env.tile_bk_fg(gc) = env.tile_fg(ep);
    env.tile_bk_bg(gc) = env.tile_bg(ep);
    env.tile_bk_cloud(gc) = env.tile_cloud(ep);
#endif

    return ret;
}

static void player_view_update()
{
    vector<coord_def> update_excludes;
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
    cglyph_t g = get_cell_glyph(gc, Options.clean_map);
    cell->glyph  = g.ch;
    cell->colour = g.col;

#ifdef USE_TILE
    tileidx_out_of_los(&cell->tile.fg, &cell->tile.bg, &cell->tile.cloud, gc);
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

    cell->colour = real_colour(cell->colour);

#ifdef USE_TILE
    cell->tile.fg = env.tile_fg(ep) = tileidx_player();
    cell->tile.bg = env.tile_bg(ep);
    cell->tile.cloud = env.tile_cloud(ep);
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
    cglyph_t g = get_cell_glyph(gc);
    cell->glyph  = g.ch;
    cell->colour = g.col;

#ifdef USE_TILE
    cell->tile.fg = env.tile_fg(ep);
    cell->tile.bg = env.tile_bg(ep);
    cell->tile.cloud = env.tile_cloud(ep);
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
//
// If tiles_only is set, only the tile view will be updated. This
// is only relevant for Webtiles.
//---------------------------------------------------------------
void viewwindow(bool show_updates, bool tiles_only)
{
    // The player could be at (0,0) if we are called during level-gen; this can
    // happen via mpr -> interrupt_activity -> stop_delay -> runrest::stop
    if (you.duration[DUR_TIME_STEP] || you.pos().origin())
        return;

    screen_cell_t *cell(crawl_view.vbuf);

    // The buffer is not initialised when run from 'monster'; abort early.
    if (!cell)
        return;

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
        if (!is_map_persistent())
            ash_detect_portals(false);

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
        flash_colour = viewmap_flash_colour();

    const coord_def tl = coord_def(1, 1);
    const coord_def br = crawl_view.viewsz;
    for (rectangle_iterator ri(tl, br); ri; ++ri)
    {
        // in grid coords
        const coord_def gc = view2grid(*ri);

        if (you.flash_where && you.flash_where->is_affected(gc) <= 0)
            draw_cell(cell, gc, anim_updates, 0);
        else
            draw_cell(cell, gc, anim_updates, flash_colour);

        cell++;
    }

    you.last_view_update = you.num_turns;
#ifndef USE_TILE_LOCAL
#ifdef USE_TILE_WEB
    tiles_crt_control crt(false);
#endif

    if (!tiles_only)
    {
        puttext(crawl_view.viewp.x, crawl_view.viewp.y, crawl_view.vbuf);
        update_monster_pane();
    }
#endif
#ifdef USE_TILE
    tiles.set_need_redraw(you.running ? Options.tile_runrest_rate : 0);
    tiles.load_dungeon(crawl_view.vbuf, crawl_view.vgrdc);
    tiles.update_tabs();
#endif

    // Leaving it this way because short flashes can occur in long ones,
    // and this simply works without requiring a stack.
    you.flash_colour = BLACK;
    you.flash_where = 0;

    // Reset env.show if we munged it.
    if (_show_terrain)
        show_init();

    _debug_pane_bounds();
}

void draw_cell(screen_cell_t *cell, const coord_def &gc,
               bool anim_updates, int flash_colour)
{
#ifdef USE_TILE
    cell->tile.clear();
#endif
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
        if (!you.see_cell(gc))
            cell->colour = DARKGREY;
#ifdef USE_TILE_LOCAL
        else
            cell->colour = real_colour(flash_colour);
#else
        else if (gc != you.pos())
        {
            monster_type mons = env.map_knowledge(gc).monster();
            if (mons == MONS_NO_MONSTER || mons_class_is_firewood(mons))
                cell->colour = real_colour(flash_colour);
        }
#endif
        cell->flash_colour = cell->colour;
    }
    else if (crawl_state.darken_range)
    {
        if (!crawl_state.darken_range->valid_aim(gc))
        {
            cell->colour = DARKGREY;
#ifdef USE_TILE
            if (you.see_cell(gc))
                cell->tile.bg |= TILE_FLAG_OOR;
#endif
        }
    }
#ifdef USE_TILE_LOCAL
    // Grey out grids that cannot be reached due to beholders.
    else if (you.get_beholder(gc))
        cell->tile.bg |= TILE_FLAG_OOR;

    else if (you.get_fearmonger(gc))
        cell->tile.bg |= TILE_FLAG_OOR;

    tile_apply_properties(gc, cell->tile);
#elif defined(USE_TILE_WEB)
    // For webtiles, we only grey out visible tiles
    else if (you.get_beholder(gc) && you.see_cell(gc))
        cell->tile.bg |= TILE_FLAG_OOR;

    else if (you.get_fearmonger(gc) && you.see_cell(gc))
        cell->tile.bg |= TILE_FLAG_OOR;

    tile_apply_properties(gc, cell->tile);
#endif
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
