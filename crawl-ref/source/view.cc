/**
 * @file
 * @brief Misc function used to render the dungeon.
**/

#include "AppHdr.h"

#include "view.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#include <sstream>

#include "act-iter.h"
#include "artefact.h"
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
#include "english.h"
#include "env.h"
#include "exclude.h"
#include "feature.h"
#include "files.h"
#include "fprop.h"
#include "god-abil.h"
#include "god-conduct.h"
#include "god-passive.h"
#include "god-wrath.h"
#include "hints.h"
#include "item-name.h" // item_type_known
#include "item-prop.h" // get_weapon_brand
#include "libutil.h"
#include "macro.h"
#include "message.h"
#include "misc.h"
#include "mon-behv.h"
#include "mon-death.h"
#include "mon-poly.h"
#include "mon-tentacle.h"
#include "mon-util.h"
#include "nearby-danger.h"
#include "notes.h"
#include "options.h"
#include "output.h"
#include "player.h"
#include "random.h"
#include "religion.h"
#include "shout.h"
#include "show.h"
#include "showsymb.h"
#include "state.h"
#include "stringutil.h"
#include "target.h"
#include "terrain.h"
#include "tilemcache.h"
#ifdef USE_TILE
 #include "tilepick.h"
 #include "tilepick-p.h"
 #include "tileview.h"
#endif
#include "traps.h"
#include "travel.h"
#include "unicode.h"
#include "viewchar.h"
#include "viewmap.h"
#include "xom.h"

//#define DEBUG_PANE_BOUNDS

static layers_type _layers = LAYERS_ALL;
static layers_type _layers_saved = LAYERS_NONE;

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

    if (!mons_is_safe(mons))
        return interrupt_activity(AI_SEE_MONSTER, aid, msgs_buf);

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
        || !you.can_see(*mon))
    {
        return;
    }

    handle_seen_interrupt(mon);
}

void seen_monsters_react(int stealth)
{
    if (you.duration[DUR_TIME_STEP] || crawl_state.game_is_arena())
        return;

    for (monster_near_iterator mi(you.pos()); mi; ++mi)
    {
        if ((mi->asleep() || mons_is_wandering(**mi))
            && check_awaken(*mi, stealth))
        {
            behaviour_event(*mi, ME_ALERT, &you, you.pos(), false);

            // That might have caused a pacified monster to leave the level.
            if (!(*mi)->alive())
                continue;

            monster_consider_shouting(**mi);
        }

        if (!mi->visible_to(&you))
            continue;

        beogh_follower_convert(*mi);
        gozag_check_bribe(*mi);
        slime_convert(*mi);

        if (!mi->has_ench(ENCH_INSANE) && mi->can_see(you))
        {
            // Trigger Duvessa & Dowan upgrades
            if (mi->props.exists(ELVEN_ENERGIZE_KEY))
            {
                mi->props.erase(ELVEN_ENERGIZE_KEY);
                elven_twin_energize(*mi);
            }
#if TAG_MAJOR_VERSION == 34
            else if (mi->props.exists(OLD_DUVESSA_ENERGIZE_KEY))
            {
                mi->props.erase(OLD_DUVESSA_ENERGIZE_KEY);
                elven_twin_energize(*mi);
            }
            else if (mi->props.exists(OLD_DOWAN_ENERGIZE_KEY))
            {
                mi->props.erase(OLD_DOWAN_ENERGIZE_KEY);
                elven_twin_energize(*mi);
            }
#endif
        }
    }
}

static string _desc_mons_type_map(map<monster_type, int> types)
{
    string message;
    unsigned int count = 1;
    for (const auto &entry : types)
    {
        string name;
        description_level_type desc;
        if (entry.second == 1)
            desc = DESC_A;
        else
            desc = DESC_PLAIN;

        name = mons_type_name(entry.first, desc);
        if (entry.second > 1)
        {
            name = make_stringf("%d %s", entry.second,
                                pluralise_monster(name).c_str());
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

static monster_type _mons_genus_keep_uniques(monster_type mc)
{
    return mons_is_unique(mc) ? mc : mons_genus(mc);
}

/**
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
    // Find the most represented genus.
    for (const auto &entry : genera)
        if (entry.second > num)
        {
            genus = entry.first;
            num = entry.second;
        }

    // The most represented genus has a single member.
    // No more factoring is possible, we're done.
    if (num == 1)
    {
        genera.clear();
        return;
    }

    genera.erase(genus);
    auto it = types.begin();
    do
    {
        if (_mons_genus_keep_uniques(it->first) != genus)
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

static bool _is_weapon_worth_listing(const item_def *wpn)
{
    return wpn && (wpn->base_type == OBJ_RODS || wpn->base_type == OBJ_STAVES
                   || is_unrandom_artefact(*wpn)
                   || get_weapon_brand(*wpn) != SPWPN_NORMAL);
}

/// Return a warning for the player about newly-seen monsters, as appropriate.
static string _monster_headsup(const vector<monster*> &monsters,
                               map<monster_type, int> &types,
                               bool divine)
{
    string warning_msg = "";
    for (const monster* mon : monsters)
    {
        const bool ash_ided = mon->props.exists("ash_id");
        const bool zin_ided = mon->props.exists("zin_id");
        const bool has_branded_weapon
            = _is_weapon_worth_listing(mon->weapon())
              || _is_weapon_worth_listing(mon->weapon(1));
        if ((divine && !ash_ided && !zin_ided)
            || (!divine && !has_branded_weapon))
        {
            continue;
        }

        if (!divine && (ash_ided || monsters.size() == 1))
            continue; // don't give redundant warnings for enemies

        monster_info mi(mon);

        if (warning_msg.size())
            warning_msg += " ";

        string monname;
        if (monsters.size() == 1)
            monname = mon->pronoun(PRONOUN_SUBJECTIVE);
        else if (mon->type == MONS_DANCING_WEAPON)
            monname = "There";
        else if (types[mon->type] == 1)
            monname = mon->full_name(DESC_THE);
        else
            monname = mon->full_name(DESC_A);
        warning_msg += uppercase_first(monname);

        warning_msg += " is";
        if (!divine)
        {
            warning_msg += get_monster_equipment_desc(mi, DESC_WEAPON_WARNING,
                                                      DESC_NONE) + ".";
            continue;
        }

        if (you_worship(GOD_ZIN))
        {
            warning_msg += " a foul ";
            if (mon->has_ench(ENCH_GLOWING_SHAPESHIFTER))
                warning_msg += "glowing ";
            warning_msg += "shapeshifter";
        }
        else
        {
            // TODO: deduplicate
            if (mon->type != MONS_DANCING_WEAPON)
                warning_msg += " ";
            warning_msg += get_monster_equipment_desc(mi, DESC_IDENTIFIED,
                                                      DESC_NONE);
        }
        warning_msg += ".";
    }

    return warning_msg;
}

/// Let Ash/Zin warn the player about newly-seen monsters, as appropriate.
static void _divine_headsup(const vector<monster*> &monsters,
                            map<monster_type, int> &types)
{
    const string warnings = _monster_headsup(monsters, types, true);
    if (!warnings.size())
        return;

    const string warning_msg = " warns you: " + warnings;
    simple_god_message(warning_msg.c_str());
#ifndef USE_TILE_LOCAL
    // XXX: should this really be here...?
    if (you_worship(GOD_ZIN))
        update_monster_pane();
#endif
}

static void _secular_headsup(const vector<monster*> &monsters,
                             map<monster_type, int> &types)
{
    const string warnings = _monster_headsup(monsters, types, false);
    if (!warnings.size())
        return;
    mprf(MSGCH_MONSTER_WARNING, "%s", warnings.c_str());
}

/**
 * Handle printing "foo comes into view" messages for newly appeared monsters.
 * Also let Ash/Zin warn the player about newly-seen monsters, as appropriate.
 *
 * @param msgs          A list of individual 'comes into view' messages; e.g.
 *                      "the goblin comes into view.", "Mara opens the gate."
 * @param monsters      A list of monsters that just became visible.
 */
static void _handle_comes_into_view(const vector<string> &msgs,
                                    const vector<monster*> monsters)
{
    const unsigned int max_msgs = 4;

    map<monster_type, int> types;
    map<monster_type, int> genera; // This is the plural for genus!
    for (const monster *mon : monsters)
    {
        const monster_type type = mon->type;
        types[type]++;
        genera[_mons_genus_keep_uniques(type)]++;
    }

    unsigned int size = monsters.size();
    if (size == 1)
        mprf(MSGCH_MONSTER_WARNING, "%s", msgs[0].c_str());
    else
    {
        while (types.size() > max_msgs && !genera.empty())
            _genus_factoring(types, genera);
        mprf(MSGCH_MONSTER_WARNING, "%s",
             _desc_mons_type_map(types).c_str());
    }

    _divine_headsup(monsters, types);
    _secular_headsup(monsters, types);
}

/// If the player has the shout mutation, maybe shout at newly-seen monsters.
static void _maybe_trigger_shoutitis(const vector<monster*> monsters)
{
    if (!player_mutation_level(MUT_SCREAM))
        return;

    for (const monster* mon : monsters)
    {
        if (!mons_is_tentacle_or_tentacle_segment(mon->type)
            && !mons_is_conjured(mon->type)
            && x_chance_in_y(3 + player_mutation_level(MUT_SCREAM) * 3, 100))
        {
            yell(mon);
            return;
        }
    }
}

/// Let Gozag's wrath buff newly-seen hostile monsters, maybe.
static void _maybe_gozag_incite(vector<monster*> monsters)
{
    if (!player_under_penance(GOD_GOZAG))
        return;

    counted_monster_list mon_count;
    vector<monster *> incited;
    for (monster* mon : monsters)
    {
        // XXX: some of this is probably redundant with interrupt_activity
        if (!mon->see_cell(you.pos()) // xray_vision
            || mon->wont_attack()
            || mon->is_stationary()
            || mons_is_object(mon->type)
            || mons_is_tentacle_or_tentacle_segment(mon->type))
        {
            continue;
        }

        if (coinflip()
            && mon->get_experience_level() >= random2(you.experience_level))
        {
            mon_count.add(mon);
            incited.push_back(mon);
        }
    }

    if (incited.empty())
        return;

    string msg = make_stringf("%s incites %s against you.",
                              god_name(GOD_GOZAG).c_str(),
                              mon_count.describe().c_str());
    if (strwidth(msg) >= get_number_of_cols() - 2)
    {
        msg = make_stringf("%s incites your enemies against you.",
                           god_name(GOD_GOZAG).c_str());
    }
    mprf(MSGCH_GOD, GOD_GOZAG, "%s", msg.c_str());

    for (monster *mon : incited)
        gozag_incite(mon);
}

void update_monsters_in_view()
{
    int num_hostile = 0;
    vector<string> msgs;
    vector<monster*> monsters;

    for (monster_iterator mi; mi; ++mi)
    {
        if (you.see_cell(mi->pos()))
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
        _handle_comes_into_view(msgs, monsters);
        // XXX: does interrupt_activity() add 'comes into view' messages to
        // 'msgs' in ALL cases we want shoutitis/gozag wrath to trigger?
        _maybe_trigger_shoutitis(monsters);
        _maybe_gozag_incite(monsters);
    }

    // Xom thinks it's hilarious the way the player picks up an ever
    // growing entourage of monsters while running through the Abyss.
    // To approximate this, if the number of hostile monsters in view
    // is greater than it ever was for this particular trip to the
    // Abyss, Xom is stimulated in proportion to the number of
    // hostile monsters. Thus if the entourage doesn't grow, then
    // Xom becomes bored.
    if (player_in_branch(BRANCH_ABYSS)
        && you.attribute[ATTR_ABYSS_ENTOURAGE] < num_hostile)
    {
        you.attribute[ATTR_ABYSS_ENTOURAGE] = num_hostile;
        xom_is_stimulated(12 * num_hostile);
    }
}

void mark_mon_equipment_seen(const monster *mons)
{
    // mark items as seen.
    for (int slot = MSLOT_WEAPON; slot <= MSLOT_LAST_VISIBLE_SLOT; slot++)
    {
        int item_id = mons->inv[slot];
        if (item_id == NON_ITEM)
            continue;

        item_def &item = mitm[item_id];

        item.flags |= ISFLAG_SEEN;

        // ID brands of weapons held by enemies.
        if (slot == MSLOT_WEAPON
            || slot == MSLOT_ALT_WEAPON && mons_wields_two_weapons(*mons))
        {
            if (is_artefact(item))
                artefact_learn_prop(item, ARTP_BRAND);
            else
                item.flags |= ISFLAG_KNOW_TYPE;
        }
    }
}


// We logically associate a difficulty parameter with each tile on each level,
// to make deterministic magic mapping work. This function returns the
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
    int seed = ((static_cast<int>(you.where_are_you) << 8) + you.depth)
             ^ (you.game_seeds[SEED_PASSIVE_MAP] & 0x7fffffff);

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

    if (!force && !is_map_persistent())
    {
        if (!suppress_msg)
            canned_msg(MSG_DISORIENTED);

        return false;
    }

    const bool wizard_map = (you.wizard && map_radius == 1000);

    if (map_radius < 5)
        map_radius = 5;

    // now gradually weaker with distance:
    const int pfar     = map_radius * 7 / 10;
    const int very_far = map_radius * 9 / 10;

    bool did_map = false;
    int  num_altars        = 0;
    int  num_shops_portals = 0;

    const FixedArray<uint8_t, GXM, GYM>& difficulty =
        _tile_difficulties(!deterministic);

    for (radius_iterator ri(pos, map_radius, C_SQUARE);
         ri; ++ri)
    {
        if (!wizard_map)
        {
            int threshold = proportion;

            const int dist = grid_distance(you.pos(), *ri);

            if (dist > very_far)
                threshold = threshold / 3;
            else if (dist > pfar)
                threshold = threshold * 2 / 3;

            if (difficulty(*ri) > threshold)
                continue;
        }

        if (env.map_knowledge(*ri).changed())
        {
            // If the player has already seen the square, update map
            // knowledge with the new terrain. Otherwise clear what we had
            // before.
            if (env.map_knowledge(*ri).seen())
            {
                dungeon_feature_type newfeat = grd(*ri);
                if (newfeat == DNGN_UNDISCOVERED_TRAP)
                    newfeat = DNGN_FLOOR;
                trap_type tr = feat_is_trap(newfeat) ? get_trap_type(*ri) : TRAP_UNASSIGNED;
                env.map_knowledge(*ri).set_feature(newfeat, env.grid_colours(*ri), tr);
            }
            else
                env.map_knowledge(*ri).clear();
        }

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

                if (get_cell_map_feature(env.map_knowledge(*ri)) == MF_STAIR_BRANCH)
                    seen_notable_thing(feat, *ri);

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
            if (!feat_is_opaque(grd(*ai)))
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
        return you.can_see(*mons) && there_are_monsters_nearby(true);
    }
    else
    {
        // For hostile monster* you* are the main enemy.
        return mons->can_see(you);
    }
}

// Returns a string containing a representation of the map. Leading and
// trailing spaces are trimmed from each line. Leading and trailing empty
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
            char32_t ch =
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

    for (string &line : lines)
        line.erase(0, lsp);     // actually trim from the left
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
    return _layers & LAYERS_ALL && you.berserk() ? RED : BLACK;
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
    textcolour(LIGHTGREY);

#endif
#ifdef USE_TILE_WEB
    tiles.mark_for_redraw(pos);
#endif
}

#ifndef USE_TILE_LOCAL
void flash_monster_colour(const monster* mon, colour_t fmc_colour,
                          int fmc_delay)
{
    ASSERT(mon); // XXX: change to const monster &mon
    if ((Options.use_animations & UA_PLAYER) && you.can_see(*mon))
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

void flash_view(use_animation_type a, colour_t colour, targeter *where)
{
    if (Options.use_animations & a)
    {
        you.flash_colour = colour;
        you.flash_where = where;
        viewwindow(false);
    }
}

void flash_view_delay(use_animation_type a, colour_t colour, int flash_delay,
                      targeter *where)
{
    if (Options.use_animations & a)
    {
        flash_view(a, colour, where);
        scaled_delay(flash_delay);
        flash_view(a, 0);
    }
}

static void _debug_pane_bounds()
{
#ifdef DEBUG_PANE_BOUNDS
    // Doesn't work for HUD because print_stats() overwrites it.

    if (crawl_view.mlistsz.y > 0)
    {
        textcolour(WHITE);
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
    textcolour(LIGHTGREY);
#endif
}

enum class update_flag
{
    AFFECT_EXCLUDES = (1 << 0),
    ADDED_EXCLUDE   = (1 << 1),
};
DEF_BITFIELD(update_flags, update_flag);

// Do various updates when the player sees a cell. Returns whether
// exclusion LOS might have been affected.
static update_flags player_view_update_at(const coord_def &gc)
{
    maybe_remove_autoexclusion(gc);
    update_flags ret;

    // Set excludes in a radius of 1 around harmful clouds genereated
    // by neither monsters nor the player.
    const cloud_struct* cloud = cloud_at(gc);
    if (cloud && !crawl_state.game_is_arena())
    {
        const cloud_struct &cl = *cloud;

        bool did_exclude = false;
        if (!cl.temporary() && is_damaging_cloud(cl.type, false))
        {
            int size = cl.exclusion_radius();

            // Steam clouds are less dangerous than the other ones,
            // so don't exclude the neighbour cells.
            if (cl.type == CLOUD_STEAM && size == 1)
                size = 0;

            bool was_exclusion = is_exclude_root(gc);
            set_exclude(gc, size, false, false, true);
            if (!did_exclude && !was_exclusion)
                ret |= update_flag::ADDED_EXCLUDE;
        }
    }

    // Print hints mode messages for features in LOS.
    if (crawl_state.game_is_hints())
        hints_observe_cell(gc);

    if (env.map_knowledge(gc).changed() || !env.map_knowledge(gc).seen())
        ret |= update_flag::AFFECT_EXCLUDES;

    set_terrain_visible(gc);

    if (!(env.pgrid(gc) & FPROP_SEEN_OR_NOEXP))
    {
        env.pgrid(gc) |= FPROP_SEEN_OR_NOEXP;
        if (!crawl_state.game_is_arena())
        {
            did_god_conduct(DID_EXPLORATION, 2500);
            const int density = env.density ? env.density : 2000;
            you.exploration += div_rand_round(1<<16, density);
        }
    }

#ifdef USE_TILE
    const coord_def ep = grid2show(gc);

    // We remove any references to mcache when
    // writing to the background.
    env.tile_bk_fg(gc) = env.tile_fg(ep);
    env.tile_bk_bg(gc) = env.tile_bg(ep);
    env.tile_bk_cloud(gc) = env.tile_cloud(ep);
#endif

    return ret;
}

static void player_view_update()
{
    if (crawl_state.game_is_arena())
    {
        for (rectangle_iterator ri(crawl_view.vgrdc, LOS_MAX_RANGE); ri; ++ri)
            player_view_update_at(*ri);
        // no need to do excludes on the arena
        return;
    }

    vector<coord_def> update_excludes;
    bool need_update = false;

    for (radius_iterator ri(you.pos(), you.xray_vision ? LOS_NONE : LOS_DEFAULT); ri; ++ri)
    {
        update_flags flags = player_view_update_at(*ri);
        if (flags & update_flag::AFFECT_EXCLUDES)
            update_excludes.push_back(*ri);
        if (flags & update_flag::ADDED_EXCLUDE)
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
    cglyph_t g = get_cell_glyph(gc);
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

class shake_viewport_animation: public animation
{
public:
    shake_viewport_animation() { frames = 5; frame_delay = 40; }

    void init_frame(int frame) override
    {
        offset = coord_def(random2(3) - 1, random2(3) - 1);
    }

    coord_def cell_cb(const coord_def &pos, int &colour) override
    {
        return pos + offset;
    }

private:
    coord_def offset;
};

class checkerboard_animation: public animation
{
public:
    checkerboard_animation() { frame_delay = 100; frames = 5; }
    void init_frame(int frame) override
    {
        current_frame = frame;
    }

    coord_def cell_cb(const coord_def &pos, int &colour) override
    {
        if (current_frame % 2 == (pos.x + pos.y) % 2 && pos != you.pos())
            return coord_def(-1, -1);
        else
            return pos;
    }

    int current_frame;
};

class banish_animation: public animation
{
public:
    banish_animation(): remaining(false) { }

    void init_frame(int frame) override
    {
        current_frame = frame;

        if (!frame)
        {
            frames = 10;
            hidden.clear();
            remaining = true;
        }

        if (remaining)
            frames = frame + 2;
        else
            frames = frame;

        remaining = false;
    }

    coord_def cell_cb(const coord_def &pos, int &colour) override
    {
        if (pos == you.pos())
            return pos;

        if (bool *found = map_find(hidden, pos))
        {
            if (*found)
                return coord_def(-1, -1);
        }

        if (!random2(10 - current_frame))
        {
            hidden.insert(make_pair(pos, true));
            return coord_def(-1, -1);
        }

        remaining = true;
        return pos;
    }

    bool remaining;
    map<coord_def, bool> hidden;
    int current_frame;
};

class slideout_animation: public animation
{
public:
    void init_frame(int frame) override
    {
        current_frame = frame;
    }

    coord_def cell_cb(const coord_def &pos, int &colour) override
    {
        coord_def ret;
        if (pos.y % 2)
            ret = coord_def(pos.x + current_frame * 4, pos.y);
        else
            ret = coord_def(pos.x - current_frame * 4, pos.y);

        coord_def view = grid2view(ret);
        const coord_def max = crawl_view.viewsz;
        if (view.x < 1 || view.y < 1 || view.x > max.x || view.y > max.y)
            return coord_def(-1, -1);
        else
            return ret;
    }

    int current_frame;
};

class orb_animation: public animation
{
public:
    void init_frame(int frame) override
    {
        current_frame = frame;
        range = current_frame > 5
            ? (10 - current_frame)
            : current_frame;
        frame_delay = 3 * (6 - range) * (6 - range);
    }

    coord_def cell_cb(const coord_def &pos, int &colour) override
    {
        const coord_def diff = pos - you.pos();
        const int dist = diff.x * diff.x * 4 / 9 + diff.y * diff.y;
        const int min = range * range;
        const int max = (range + 2) * (range  + 2);
        if (dist >= min && dist < max)
            if (is_tiles())
                colour = MAGENTA;
            else
                colour = DARKGREY;
        else
            colour = 0;

        return pos;
    }

    int range;
    int current_frame;
};

static shake_viewport_animation shake_viewport;
static checkerboard_animation checkerboard;
static banish_animation banish;
static slideout_animation slideout;
static orb_animation orb;

static animation *animations[NUM_ANIMATIONS] = {
    &shake_viewport,
    &checkerboard,
    &banish,
    &slideout,
    &orb
};

void run_animation(animation_type anim, use_animation_type type, bool cleanup)
{
#ifdef USE_TILE_WEB
    // XXX this doesn't work in webtiles yet
    if (is_tiles())
        return;
#endif
    if (Options.use_animations & type)
    {
        animation *a = animations[anim];

        viewwindow();

        for (int i = 0; i < a->frames; ++i)
        {
            a->init_frame(i);
            viewwindow(false, false, a);
            delay(a->frame_delay);
        }

        if (cleanup)
            viewwindow();
    }
}

/**
 * Draws the main window using the character set returned
 * by get_show_glyph().
 *
 * @param show_updates if true, env.show and dependent structures
 *                     are updated. Should be set if anything in
 *                     view has changed.
 * @param tiles_only if true, only the tile view will be updated. This
 *                   is only relevant for Webtiles.
 * @param a[in] the animation to be showing, if any.
 */
void viewwindow(bool show_updates, bool tiles_only, animation *a)
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

    if (show_updates || _layers != LAYERS_ALL)
    {
        if (!is_map_persistent())
            ash_detect_portals(false);

#ifdef USE_TILE
        tile_draw_floor();
        tile_draw_rays(true);
        tiles.clear_overlays();
#endif

        show_init(_layers);
    }

    if (show_updates)
        player_view_update();

    bool run_dont_draw = you.running && Options.travel_delay < 0
                && (!you.running.is_explore() || Options.explore_delay < 0);

    if (run_dont_draw || you.asleep())
    {
        // Reset env.show if we munged it.
        if (_layers != LAYERS_ALL)
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
        const coord_def gc = a
            ? a->cell_cb(view2grid(*ri), flash_colour)
            : view2grid(*ri);

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
    if (_layers != LAYERS_ALL)
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
    else if (gc == you.pos() && you.on_current_level
             && _layers & LAYER_PLAYER
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
    else if (crawl_state.flash_monsters)
    {
        bool found = gc == you.pos();

        if (!found)
            for (auto mon : *crawl_state.flash_monsters)
            {
                if (gc == mon->pos())
                {
                    found = true;
                    break;
                }
            }

        if (!found)
            cell->colour = DARKGREY;
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
#ifndef USE_TILE_LOCAL
    if ((_layers != LAYERS_ALL || Options.always_show_exclusions)
        && you.on_current_level
        && map_bounds(gc)
        && (_layers == LAYERS_NONE
            || gc != you.pos()
               && (env.map_knowledge(gc).monster() == MONS_NO_MONSTER
                   || !you.see_cell(gc)))
        && travel_colour_override(gc))
    {
        if (is_exclude_root(gc))
            cell->colour = Options.tc_excluded;
        else if (is_excluded(gc))
            cell->colour = Options.tc_exclude_circle;
    }
#endif
}

// Hide view layers. The player can toggle certain layers back on
// and the resulting configuration will be remembered for the
// remainder of the game session.
// Pressing | again will return to normal view. Leaving the prompt
// by any other means will give back control of the keys, but the
// view will remain in its altered state until the | key is pressed
// again or the player performs an action.
static void _config_layers_menu()
{
    bool exit = false;

    _layers = _layers_saved;
    crawl_state.viewport_weapons    = !!(_layers & LAYER_MONSTER_WEAPONS);
    crawl_state.viewport_monster_hp = !!(_layers & LAYER_MONSTER_HEALTH);

    msgwin_set_temporary(true);
    while (!exit)
    {
        viewwindow();
        mprf(MSGCH_PROMPT, "Select layers to display:\n"
                           "<%s>(m)onsters</%s>|"
                           "<%s>(p)layer</%s>|"
                           "<%s>(i)tems</%s>|"
                           "<%s>(c)louds</%s>"
#ifndef USE_TILE_LOCAL
                           "|"
                           "<%s>monster (w)eapons</%s>|"
                           "<%s>monster (h)ealth</%s>"
#endif
                           ,
           _layers & LAYER_MONSTERS        ? "lightgrey" : "darkgrey",
           _layers & LAYER_MONSTERS        ? "lightgrey" : "darkgrey",
           _layers & LAYER_PLAYER          ? "lightgrey" : "darkgrey",
           _layers & LAYER_PLAYER          ? "lightgrey" : "darkgrey",
           _layers & LAYER_ITEMS           ? "lightgrey" : "darkgrey",
           _layers & LAYER_ITEMS           ? "lightgrey" : "darkgrey",
           _layers & LAYER_CLOUDS          ? "lightgrey" : "darkgrey",
           _layers & LAYER_CLOUDS          ? "lightgrey" : "darkgrey"
#ifndef USE_TILE_LOCAL
           ,
           _layers & LAYER_MONSTER_WEAPONS ? "lightgrey" : "darkgrey",
           _layers & LAYER_MONSTER_WEAPONS ? "lightgrey" : "darkgrey",
           _layers & LAYER_MONSTER_HEALTH  ? "lightgrey" : "darkgrey",
           _layers & LAYER_MONSTER_HEALTH  ? "lightgrey" : "darkgrey"
#endif
        );
        mprf(MSGCH_PROMPT, "Press <w>%s</w> to return to normal view. "
                           "Press any other key to exit.",
                           command_to_string(CMD_SHOW_TERRAIN).c_str());

        switch (get_ch())
        {
        case 'm': _layers_saved = _layers ^= LAYER_MONSTERS;        break;
        case 'p': _layers_saved = _layers ^= LAYER_PLAYER;          break;
        case 'i': _layers_saved = _layers ^= LAYER_ITEMS;           break;
        case 'c': _layers_saved = _layers ^= LAYER_CLOUDS;          break;
#ifndef USE_TILE_LOCAL
        case 'w': _layers_saved = _layers ^= LAYER_MONSTER_WEAPONS;
                  if (_layers & LAYER_MONSTER_WEAPONS)
                      _layers_saved = _layers |= LAYER_MONSTERS;
                  break;
        case 'h': _layers_saved = _layers ^= LAYER_MONSTER_HEALTH;
                  if (_layers & LAYER_MONSTER_HEALTH)
                      _layers_saved = _layers |= LAYER_MONSTERS;
                  break;
#endif

        // Remaining cases fall through to exit.
        case '|':
            _layers = LAYERS_ALL;
            crawl_state.viewport_weapons    = !!(_layers & LAYER_MONSTER_WEAPONS);
            crawl_state.viewport_monster_hp = !!(_layers & LAYER_MONSTER_HEALTH);
        default:
            exit = true;
            break;
        }

        crawl_state.viewport_weapons    = !!(_layers & LAYER_MONSTER_WEAPONS);
        crawl_state.viewport_monster_hp = !!(_layers & LAYER_MONSTER_HEALTH);

        msgwin_clear_temporary();
    }
    msgwin_set_temporary(false);

    canned_msg(MSG_OK);
    if (_layers != LAYERS_ALL)
    {
        mprf(MSGCH_PLAIN, "Press <w>%s</w> or perform an action "
                          "to restore all view layers.",
                          command_to_string(CMD_SHOW_TERRAIN).c_str());
    }
}

void toggle_show_terrain()
{
    if (_layers == LAYERS_ALL)
        _config_layers_menu();
    else
        reset_show_terrain();
}

void reset_show_terrain()
{
    if (_layers != LAYERS_ALL)
        mprf(MSGCH_PROMPT, "Restoring view layers.");

    _layers = LAYERS_ALL;
    crawl_state.viewport_weapons    = !!(_layers & LAYER_MONSTER_WEAPONS);
    crawl_state.viewport_monster_hp = !!(_layers & LAYER_MONSTER_HEALTH);
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
