/**
 * @file
 * @brief Functions used when picking squares.
**/

#include "AppHdr.h"

#include "directn.h"

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <sstream>

#include "act-iter.h"
#include "areas.h"
#include "artefact.h"
#include "attitude-change.h"
#include "cloud.h"
#include "colour.h"
#include "command.h"
#include "coordit.h"
#include "describe.h"
#include "dungeon.h"
#include "english.h"
#include "externs.h" // INVALID_COORD
#include "fight.h" // melee_confuse_chance
#include "god-abil.h"
#include "hints.h"
#include "invent.h"
#include "item-prop.h"
#include "item-status-flag-type.h"
#include "items.h"
#include "libutil.h"
#include "localise.h"
#include "losglobal.h"
#include "macro.h"
#include "mapmark.h"
#include "message.h"
#include "message-util.h"
#include "misc.h"
#include "mon-death.h"
#include "mon-tentacle.h"
#include "nearby-danger.h"
#include "output.h"
#include "prompt.h"
#include "showsymb.h"
#include "spl-goditem.h"
#include "stash.h"
#include "state.h"
#include "stringutil.h"
#include "tag-version.h"
#include "target.h"
#include "terrain.h"
#ifdef USE_TILE
 #include "tileview.h"
 #include "tile-flags.h"
#endif
#include "traps.h"
#include "travel.h"
#include "viewchar.h"
#include "view.h"
#include "viewmap.h"
#include "wiz-dgn.h"
#include "wiz-mon.h"

enum LOSSelect
{
    LS_ANY      = 0x00,

    // Check only visible squares
    LS_VISIBLE  = 0x01,

    // Check only hidden squares
    LS_HIDDEN   = 0x02,

    LS_VISMASK  = 0x03,

    // Flip from visible to hidden when going forward,
    // or hidden to visible when going backwards.
    LS_FLIPVH   = 0x20,

    // Flip from hidden to visible when going forward,
    // or visible to hidden when going backwards.
    LS_FLIPHV   = 0x40,

    LS_NONE     = 0xFFFF,
};

#ifdef WIZARD
static void _wizard_make_friendly(monster* m);
#endif
static dist _look_around_target(const coord_def &whence);
static void _describe_oos_feature(const coord_def& where);
static void _describe_cell(const coord_def& where, bool in_range = true);
static bool _print_cloud_desc(const coord_def where);
static bool _print_item_desc(const coord_def where);

static bool _blocked_ray(const coord_def &where);
static bool _want_target_monster(const monster *mon, targ_mode_type mode,
                                 targeter* hitfunc);
static bool _find_monster(const coord_def& where, targ_mode_type mode,
                          bool need_path, int range, targeter *hitfunc);
static bool _find_monster_expl(const coord_def& where, targ_mode_type mode,
                               bool need_path, int range, targeter *hitfunc,
                               aff_type mon_aff, aff_type allowed_self_aff);
static bool _find_shadow_step_mons(const coord_def& where, targ_mode_type mode,
                                   bool need_path, int range,
                                   targeter *hitfunc);
static bool _find_object(const coord_def& where, bool need_path, int range,
                         targeter *hitfunc);
static bool _find_autopickup_object(const coord_def& where, bool need_path,
                                    int range, targeter *hitfunc);

typedef function<bool (const coord_def& where)> target_checker;
static bool _find_square_wrapper(coord_def &mfp, int direction,
                                 target_checker find_targ, targeter *hitfunc,
                                 LOSSelect los = LS_ANY);

static int  _targeting_cmd_to_compass(command_type command);
static void _describe_oos_square(const coord_def& where);
static void _extend_move_to_edge(dist &moves);
static vector<string> _get_monster_desc_vector(const monster_info& mi);
static string _get_monster_desc(const monster_info& mi);

#ifdef DEBUG_DIAGNOSTICS
static void _debug_describe_feature_at(const coord_def &where);
#endif

#ifdef WIZARD
static void _wizard_make_friendly(monster* m)
{
    if (m == nullptr)
        return;

    mon_attitude_type att = m->attitude;

    // During arena mode, skip directly from friendly to hostile.
    if (crawl_state.arena_suspended && att == ATT_FRIENDLY)
        att = ATT_NEUTRAL;

    switch (att)
    {
    case ATT_FRIENDLY:
        m->attitude = ATT_GOOD_NEUTRAL;
        m->flags &= ~MF_NO_REWARD;
        m->flags |= MF_WAS_NEUTRAL;
        break;
    case ATT_GOOD_NEUTRAL:
        m->attitude = ATT_STRICT_NEUTRAL;
        break;
    case ATT_STRICT_NEUTRAL:
        m->attitude = ATT_NEUTRAL;
        break;
    case ATT_NEUTRAL:
        m->attitude = ATT_HOSTILE;
        m->flags &= ~MF_WAS_NEUTRAL;
        break;
    case ATT_HOSTILE:
        m->attitude = ATT_FRIENDLY;
        m->flags |= MF_NO_REWARD;
        break;
    }
    mons_att_changed(m);
}
#endif

dist::dist()
    : isValid(false), isTarget(false), isEndpoint(false), isCancel(false),
      choseRay(false), interactive(false), target(), delta(), ray(),
       find_target(false), fire_context(nullptr), cmd_result(CMD_NO_CMD)
{
}

bool dist::isMe() const
{
    // We hack the decision as to whether to use delta or target by
    // assuming that we use delta only if target hasn't been touched.
    return isValid && !isCancel
           && (target == you.pos()
               || (target.origin() && delta.origin()));
}

/**
 * Does this dist object need `target` to be filled in somehow, e.g. with
 * manual/interactive targeting? Affected by the following three dist fields:
 *
 *  `interactive`: force interactive targeting, overrides the other two.
 *  `find_target`: requests to use the direction chooser default target, allows
 *                 non-interactive mode. Overrides a value supplied by `target`.
 *  `target`: supplying a target coord directly allows non-interactive mode.
 */
bool dist::needs_targeting() const
{
    return interactive || !in_bounds(target) && !find_target;
}

static int _targeting_cmd_to_compass(command_type command)
{
    switch (command)
    {
    case CMD_TARGET_UP:         case CMD_TARGET_DIR_UP:
        return 0;
    case CMD_TARGET_UP_RIGHT:   case CMD_TARGET_DIR_UP_RIGHT:
        return 1;
    case CMD_TARGET_RIGHT:      case CMD_TARGET_DIR_RIGHT:
        return 2;
    case CMD_TARGET_DOWN_RIGHT: case CMD_TARGET_DIR_DOWN_RIGHT:
        return 3;
    case CMD_TARGET_DOWN:       case CMD_TARGET_DIR_DOWN:
        return 4;
    case CMD_TARGET_DOWN_LEFT:  case CMD_TARGET_DIR_DOWN_LEFT:
        return 5;
    case CMD_TARGET_LEFT:       case CMD_TARGET_DIR_LEFT:
        return 6;
    case CMD_TARGET_UP_LEFT:    case CMD_TARGET_DIR_UP_LEFT:
        return 7;
    default:
        return -1;
    }
}

static command_type shift_direction(command_type cmd)
{
    switch (cmd)
    {
    case CMD_TARGET_DOWN_LEFT:  return CMD_TARGET_DIR_DOWN_LEFT;
    case CMD_TARGET_LEFT:       return CMD_TARGET_DIR_LEFT;
    case CMD_TARGET_DOWN:       return CMD_TARGET_DIR_DOWN;
    case CMD_TARGET_UP:         return CMD_TARGET_DIR_UP;
    case CMD_TARGET_RIGHT:      return CMD_TARGET_DIR_RIGHT;
    case CMD_TARGET_DOWN_RIGHT: return CMD_TARGET_DIR_DOWN_RIGHT;
    case CMD_TARGET_UP_RIGHT:   return CMD_TARGET_DIR_UP_RIGHT;
    case CMD_TARGET_UP_LEFT:    return CMD_TARGET_DIR_UP_LEFT;
    default: return cmd;
    }
}

actor* direction_chooser::targeted_actor() const
{
    if (target() == you.pos())
        return &you;
    else
        return targeted_monster();
}

monster* direction_chooser::targeted_monster() const
{
    monster* m = monster_at(target());
    if (m && you.can_see(*m))
        return m;
    else
        return nullptr;
}

// Return your target, if it still exists and is visible to you.
static monster* _get_current_target()
{
    if (invalid_monster_index(you.prev_targ))
        return nullptr;

    monster* mon = &env.mons[you.prev_targ];
    ASSERT(mon);
    if (mon->alive() && you.can_see(*mon))
        return mon;
    else
        return nullptr;
}

string direction_chooser::build_targeting_hint_string() const
{
    string hint_string;

    // Hint for 'p' - previous target, and for 'f' - current cell, if
    // applicable.
    // TODO: currently 'f' works for non-actor targets (features, apport) but
    // shows nothing here
    const actor*   f_target = targeted_actor();
    const monster* p_target = _get_current_target();

    if (f_target && f_target == p_target)
        hint_string = localise(", f/p - %s", f_target->name(DESC_PLAIN));
    else
    {
        if (f_target)
            hint_string += localise(", f - %s", f_target->name(DESC_PLAIN));
        if (p_target)
            hint_string += localise(", p - %s", p_target->name(DESC_PLAIN));
    }

    return hint_string;
}

void direction_chooser::print_top_prompt() const
{
    if (!top_prompt.empty())
        mprf(MSGCH_PROMPT, "%s", top_prompt.c_str());
    else if (moves.fire_context)
    {
        // TODO: consolidate top prompt construction more
        mprf(MSGCH_PROMPT, "%s",
            moves.fire_context->get()->quiver_description().tostring().c_str());
    }
}

void direction_chooser::print_key_hints() const
{
    // TODO: build this as a vector and insert ,s and \ns in a smarter way
    string prompt = localise("Press: ? - help");

    if (just_looking)
    {
        if (you.see_cell(target()))
            prompt += localise(", ") + localise("v - describe");
        prompt += localise(", ") + localise(". - travel");
        if (in_bounds(target()) && env.map_knowledge(target()).item())
            prompt += localise(", ") + localise("g - get item");
    }
    else
    {
        if (moves.fire_context)
        {
            const string hint = moves.fire_context->fire_key_hints();
            if (!hint.empty())
                prompt += hint + "\n";
        }
        string direction_hint = "";
        if (!behaviour->targeted())
            direction_hint = localise("Dir - look around, f - activate");
        else
        {
            switch (restricts)
            {
            case DIR_NONE:
                direction_hint = localise("Shift-Dir - straight line");
                break;
            case DIR_TARGET:
            case DIR_SHADOW_STEP:
            case DIR_LEAP:
                direction_hint = localise("Dir - move target");
                break;
            }
        }

        if (direction_hint.size())
        {
            if (prompt[prompt.size() - 1] != '\n')
                prompt += localise(", ");
            prompt += direction_hint;
        }
        if (behaviour->targeted())
            prompt += build_targeting_hint_string();
    }

    // Display the prompt.
    mpr_nolocalise(MSGCH_PROMPT, prompt.c_str());
}

bool direction_chooser::targets_objects() const
{
    return mode == TARG_MOVABLE_OBJECT;
}

/// Are we looking for enemies?
bool direction_chooser::targets_enemies() const
{
    switch (mode)
    {
        case TARG_HOSTILE:
        case TARG_HOSTILE_SUBMERGED:
            return true;
        default:
            return false;
    }
}

void direction_chooser::describe_cell() const
{
    print_top_prompt();
    print_key_hints();

    if (!you.see_cell(target()))
    {
        // FIXME: make this better integrated.
        _describe_oos_square(target());
    }
    else
    {
        bool did_cloud = false;
        print_target_description(did_cloud);
        if (just_looking)
            print_items_description();
        if (just_looking || show_floor_desc)
        {
            print_floor_description(show_boring_feats);
            if (!did_cloud)
                _print_cloud_desc(target());
        }
    }

    flush_prev_message();
}

#ifndef USE_TILE_LOCAL
static cglyph_t _get_ray_glyph(const coord_def& pos, int colour, int glych,
                               int mcol)
{
    if (const monster* mons = monster_at(pos))
    {
        if (mons->alive() && mons->visible_to(&you))
        {
            glych = get_cell_glyph(pos).ch;
            colour = mcol;
        }
    }
    if (pos == you.pos())
    {
        glych = mons_char(you.symbol);
        colour = mcol;
    }
    return {static_cast<char32_t>(glych),
            static_cast<unsigned short>(real_colour(colour))};
}
#endif

// Unseen monsters in shallow water show a "strange disturbance".
// (Unless flying!)
// These should match tests in show.cc's _update_monster
static bool _mon_exposed_in_water(const monster* mon)
{
    return env.grid(mon->pos()) == DNGN_SHALLOW_WATER && !mon->airborne()
           && !mon->submerged() && !cloud_at(mon->pos());
}

static bool _mon_exposed_in_cloud(const monster* mon)
{
    return cloud_at(mon->pos())
           && is_opaque_cloud(cloud_at(mon->pos())->type)
           && !mon->submerged() && !mon->is_insubstantial();
}

static bool _mon_exposed(const monster* mon)
{
    if (!mon || !you.see_cell(mon->pos()) || mon->visible_to(&you))
        return false;

    return _mon_exposed_in_water(mon) || _mon_exposed_in_cloud(mon);
}

static bool _is_target_in_range(const coord_def& where, int range,
                                targeter *hitfunc)
{
    if (hitfunc)
        return hitfunc->valid_aim(where);
    // range == -1 means that range doesn't matter.
    return range == -1 || grid_distance(you.pos(), where) <= range;
}

targeting_behaviour direction_chooser::stock_behaviour;

void direction(dist &moves, const direction_chooser_args& args)
{
    // TODO this might break pre-chosen delta targeting, if that ever happens
    // (currently it looks like isTarget is always set to true)
    moves.interactive = moves.needs_targeting();

    if (moves.interactive)
        direction_chooser(moves, args).choose_direction();
    else
        direction_chooser(moves, args).noninteractive();
}

direction_chooser::direction_chooser(dist& moves_,
                                     const direction_chooser_args& args) :
    moves(moves_),
    restricts(args.restricts),
    mode(args.mode),
    range(args.range),
    just_looking(args.just_looking),
    prefer_farthest(args.prefer_farthest),
    allow_shift_dir(args.allow_shift_dir),
    self(args.self),
    target_prefix(args.target_prefix),
    top_prompt(args.top_prompt),
    behaviour(args.behaviour),
    show_floor_desc(args.show_floor_desc),
    show_boring_feats(args.show_boring_feats),
    hitfunc(args.hitfunc),
    default_place(args.default_place),
    renderer(*this),
    unrestricted(args.unrestricted),
    needs_path(args.needs_path)
{
    if (!behaviour)
        behaviour = &stock_behaviour;

    behaviour->just_looking = just_looking;
    behaviour->get_desc_func = args.get_desc_func;
    if (unrestricted)
    {
        needs_path = false;
        behaviour->needs_path = MB_MAYBE;
    }
    else if (hitfunc)
    {
        needs_path = true;
        behaviour->needs_path = MB_MAYBE; // TODO: can this be relaxed?
    }
    if (behaviour->needs_path != MB_MAYBE)
        needs_path = tobool(behaviour->needs_path, true);

    show_beam = !just_looking && needs_path;
    need_viewport_redraw = show_beam;
    have_beam = false;

    need_text_redraw = true;
    need_cursor_redraw = true;
    need_all_redraw = false;
}

class view_desc_proc
{
public:
    view_desc_proc()
    {
        // This thing seems to be starting off 1 line above where it
        // should be. -cao
        nextline();
    }
    int width() { return crawl_view.msgsz.x; }
    int height() { return crawl_view.msgsz.y; }
    void print(const string &str) { cprintf("%s", str.c_str()); }
    void nextline() { cgotoxy(1, wherey() + 1); }
};

static coord_def _full_describe_menu(vector<monster_info> const &list_mons,
                                     vector<item_def> const &list_items,
                                     vector<coord_def> const &list_features,
                                     string selectverb,
                                     bool examine_only = false,
                                     string title = "")
{
    InvMenu desc_menu(MF_SINGLESELECT | MF_ANYPRINTABLE
                        | MF_ALLOW_FORMATTING | MF_SELECT_BY_PAGE);

    string title_secondary;

    if (title.empty())
    {
        if (!list_mons.empty())
            title  = localise("Visible Monsters");
        if (!list_items.empty())
        {
            if (!title.empty())
                title += localise("/Items");
            else
                title += localise("Visible Items");
        }
        if (!list_features.empty())
        {
            if (!title.empty())
                title += localise("/Features");
            else
                title += localise("Visible Features");
        }
        if (examine_only)
            title += localise(" (select to examine)");
        else
        {
            title_secondary = title;
            title_secondary += localise(" (select to examine, '!' to %s):", selectverb);
            title += localise(" (select to %s, '!' to examine):", selectverb);
        }
    }
    else
        title = localise(title);

    desc_menu.set_tag("pickup");
    // necessary for sorting of the item submenu
    desc_menu.set_type(menu_type::pickup);
    desc_menu.set_title(new MenuEntry(title, MEL_TITLE));
    if (examine_only)
    {
        desc_menu.action_cycle = Menu::CYCLE_NONE;
        desc_menu.menu_action = InvMenu::ACT_EXAMINE;
    }
    else
    {
        desc_menu.action_cycle = Menu::CYCLE_TOGGLE;
        desc_menu.menu_action = InvMenu::ACT_EXECUTE;
        desc_menu.set_title(new MenuEntry(title_secondary, MEL_TITLE), false);
    }

    // Start with hotkey 'a' and count from there.
    menu_letter hotkey;
    // Build menu entries for monsters.
    if (!list_mons.empty())
    {
        desc_menu.add_entry(new MenuEntry(localise("Monsters"), MEL_SUBTITLE));
        for (const monster_info &mi : list_mons)
        {
            // List monsters in the form
            // (A) An angel (neutral), wielding a glowing long sword

            ostringstream prefix;
#ifndef USE_TILE_LOCAL
            cglyph_t g = get_mons_glyph(mi);
            const string col_string = colour_to_str(g.col);
            prefix << "(<" << col_string << ">"
                     << (g.ch == '<' ? "<<" : stringize_glyph(g.ch))
                     << "</" << col_string << ">) ";
#endif
            if (Options.monster_item_view_coordinates)
            {
                const coord_def relpos = mi.pos - you.pos();
                prefix << "(" << relpos.x << ", " << -relpos.y << ") ";
            }

            string str = get_monster_equipment_desc(mi, DESC_FULL, DESC_A, true);
            if (mi.dam != MDAM_OKAY)
                str += localise(", ") + localise(mi.damage_desc());

            string consinfo = mi.constriction_description();
            if (!consinfo.empty())
                str += localise(", ") + consinfo;

#ifndef USE_TILE_LOCAL
            // Wraparound if the description is longer than allowed.
            linebreak_string(str, get_number_of_cols() - 9);
#endif
            vector<formatted_string> fss;
            formatted_string::parse_string_to_multiple(str, fss);
            MenuEntry *me = nullptr;
            for (unsigned int j = 0; j < fss.size(); ++j)
            {
                if (j == 0)
                    me = new MonsterMenuEntry(prefix.str() + fss[j].tostring(), &mi, hotkey++);
#ifndef USE_TILE_LOCAL
                else
                {
                    str = "         " + fss[j].tostring();
                    me = new MenuEntry(str, MEL_ITEM, 1);
                }
#endif
                desc_menu.add_entry(me);
            }
        }
    }

    // Build menu entries for items.
    if (!list_items.empty())
    {
        vector<InvEntry*> all_items;
        for (const item_def &item : list_items)
            all_items.push_back(new InvEntry(item));

        const menu_sort_condition *cond = desc_menu.find_menu_sort_condition();
        desc_menu.sort_menu(all_items, cond);

        desc_menu.add_entry(new MenuEntry(localise("Items"), MEL_SUBTITLE));
        for (InvEntry *me : all_items)
        {
#ifndef USE_TILE_LOCAL
            // Show glyphs only for ASCII.
            me->set_show_glyph(true);
#endif
            me->set_show_coordinates(Options.monster_item_view_coordinates);
            me->tag = "pickup";
            me->hotkeys[0] = hotkey;
            me->quantity = 2; // Hack to make items selectable.

            desc_menu.add_entry(me);
            ++hotkey;
        }
    }

    if (!list_features.empty())
    {
        desc_menu.add_entry(new MenuEntry(localise("Features"), MEL_SUBTITLE));
        for (const coord_def &c : list_features)
        {
            ostringstream desc;
#ifndef USE_TILE_LOCAL
            cglyph_t g = get_cell_glyph(c, true);
            const string colour_str = colour_to_str(g.col);
            desc << "(<" << colour_str << ">";
            desc << (g.ch == '<' ? "<<" : stringize_glyph(g.ch));

            desc << "</" << colour_str << ">) ";
#endif
            if (Options.monster_item_view_coordinates)
            {
                const coord_def relpos = c - you.pos();
                desc << "(" << relpos.x << ", " << -relpos.y << ") ";
            }

            desc << localise(feature_description_at(c, false, DESC_A));
            if (is_unknown_stair(c) || is_unknown_transporter(c))
                desc << localise(" (not visited)");
            FeatureMenuEntry *me = new FeatureMenuEntry(desc.str(), c, hotkey);
            me->tag        = "description";
            // Hack to make features selectable.
            me->quantity   = c.x*100 + c.y + 3;
            desc_menu.add_entry(me);
            ++hotkey;
        }
    }

    // Select an item to read its full description, or a monster to read its
    // e'x'amine description. Toggle with '!' to return the coordinates to
    // the caller so that it may perform the promised selectverb

    coord_def target(-1, -1);

    desc_menu.on_single_selection = [&desc_menu, &target](const MenuEntry& sel)
    {
        target = coord_def(-1, -1);
        // HACK: quantity == 1: monsters, quantity == 2: items
        const int quant = sel.quantity;
        if (quant == 1)
        {
            // Get selected monster.
            monster_info* m = static_cast<monster_info* >(sel.data);

#ifdef USE_TILE
            // Highlight selected monster on the screen.
            const coord_def gc(m->pos);
            tiles.place_cursor(CURSOR_TUTORIAL, gc);
            const string &desc = get_terse_square_desc(gc);
            tiles.clear_text_tags(TAG_TUTORIAL);
            tiles.add_text_tag(TAG_TUTORIAL, desc, gc);
#endif

            if (desc_menu.menu_action == InvMenu::ACT_EXAMINE)
            {
                // View database entry.
                describe_monsters(*m);
                redraw_screen();
                update_screen();
                clear_messages();
            }
            else // ACT_EXECUTE -> view/travel
                target = m->pos;
        }
        else if (quant == 2)
        {
            // Get selected item.
            const item_def* i = static_cast<item_def*>(sel.data);
            if (desc_menu.menu_action == InvMenu::ACT_EXAMINE)
                describe_item_popup(*i);
            else // ACT_EXECUTE -> view/travel
                target = i->pos;
        }
        else
        {
            const int num = quant - 3;
            const int y = num % 100;
            const int x = (num - y)/100;
            coord_def c(x,y);

            if (desc_menu.menu_action == InvMenu::ACT_EXAMINE)
                describe_feature_wide(c);
            else // ACT_EXECUTE -> view/travel
                target = c;
        }
        return desc_menu.menu_action == InvMenu::ACT_EXAMINE;
    };
    desc_menu.show();
    redraw_screen();
    update_screen();


#ifndef USE_TILE_LOCAL
    if (!list_items.empty())
    {
        // Unset show_glyph for other menus.
        InvEntry me(list_items[0]);
        me.set_show_glyph(false);
    }
#endif
#ifdef USE_TILE
    // Clear cursor placement.
    tiles.place_cursor(CURSOR_TUTORIAL, NO_CURSOR);
    tiles.clear_text_tags(TAG_TUTORIAL);
#endif
    return target;
}

static void _get_nearby_items(vector<item_def> &list_items,
                                bool need_path, int range, targeter *hitfunc)
{
    // Grab all items known (or thought) to be in the stashes in view.
    for (vision_iterator ri(you); ri; ++ri)
    {
        if (!_is_target_in_range(*ri, range, hitfunc))
            continue;

        if (need_path && _blocked_ray(*ri))
            continue;

        const int oid = you.visible_igrd(*ri);
        if (oid == NON_ITEM)
            continue;

        const vector<const item_def *> items = item_list_on_square(
                                                   you.visible_igrd(*ri));

        for (const item_def * item : items)
            list_items.push_back(*item);
    }
}

static void _get_nearby_features(vector<coord_def> &list_features,
                          bool need_path, int range, targeter *hitfunc)
{
    vector <text_pattern> &filters = Options.monster_item_view_features;
    for (vision_iterator ri(you); ri; ++ri)
    {
        if (!_is_target_in_range(*ri, range, hitfunc))
            continue;

        if (need_path && _blocked_ray(*ri))
            continue;

        // Do we want to aim at this because its the feature, not because
        // of a monster. This is a bit of a hack since hitfunc->valid_aim
        // is monster-aware and monster targets are listed in a different
        // place.
        if (hitfunc && !monster_at(*ri))
            list_features.push_back(*ri);
        // Not using a targeter, list features according to user preferences.
        else if (!hitfunc)
        {
            if (!filters.empty())
            {
                for (const text_pattern &pattern : filters)
                {
                    if (pattern.matches(feature_description(env.grid(*ri)))
                        || feat_stair_direction(env.grid(*ri)) != CMD_NO_CMD
                           && pattern.matches("stair")
                        || feat_is_trap(env.grid(*ri))
                           && pattern.matches("trap"))
                    {
                        list_features.push_back(*ri);
                        break;
                    }
                }
            }
        }
    }
}

// Lists monsters, items, and some interesting features in the player's view.
// TODO: Allow sorting of items lists.
void full_describe_view()
{
    vector<monster_info> list_mons;
    vector<item_def> list_items;
    vector<coord_def> list_features;
    // Get monsters via the monster_info, sorted by difficulty.
    get_monster_info(list_mons);
    _get_nearby_items(list_items, false, get_los_radius(), nullptr);
    _get_nearby_features(list_features, false, get_los_radius(), nullptr);

    if (list_mons.empty() && list_items.empty() && list_features.empty())
    {
        mpr("No monsters, items or features are visible.");
        return;
    }

    coord_def target = _full_describe_menu(list_mons, list_items,
                                           list_features, "target/travel");

    // need to do this after the menu has been closed on console,
    // since do_look_around() runs its own loop
    if (target != coord_def(-1, -1))
        do_look_around(target);
}

void do_look_around(const coord_def &whence)
{
    dist lmove = _look_around_target(you.pos() + whence);
    if (lmove.isValid && lmove.isTarget && !lmove.isCancel
        && !crawl_state.arena_suspended)
    {
        start_travel(lmove.target);
    }
}

bool get_look_position(coord_def *c)
{
    dist lmove = _look_around_target(you.pos());
    if (lmove.isCancel)
        return false;
    *c = lmove.target;
    return true;
}

static dist _look_around_target(const coord_def &whence)
{
    dist lmove;   // Will be initialised by direction().
    direction_chooser_args args;
    args.restricts = DIR_TARGET;
    args.just_looking = true;
    args.needs_path = false;
    args.target_prefix = "Here";
    args.default_place = whence - you.pos();
    direction(lmove, args);
    return lmove;
}

range_view_annotator::range_view_annotator(targeter *range)
{
    if (range && Options.darken_beyond_range)
        crawl_state.darken_range = range;
}

range_view_annotator::~range_view_annotator()
{
    crawl_state.darken_range = nullptr;
}

monster_view_annotator::monster_view_annotator(vector<monster *> *monsters)
{
    if ((Options.use_animations & UA_MONSTER_IN_SIGHT) && monsters->size())
    {
        crawl_state.flash_monsters = monsters;
        viewwindow(false);
        update_screen();
    }
}

monster_view_annotator::~monster_view_annotator()
{
    if ((Options.use_animations & UA_MONSTER_IN_SIGHT)
        && crawl_state.flash_monsters)
    {
        crawl_state.flash_monsters = nullptr;
        viewwindow(false);
        update_screen();
    }
}

bool direction_chooser::move_is_ok() const
{
    if (unrestricted || !behaviour->targeted())
        return true;
    if (!moves.isCancel && moves.isTarget)
    {
        if (!cell_see_cell(you.pos(), target(), LOS_NO_TRANS))
        {
            if (hitfunc && hitfunc->can_affect_unseen())
                return true; // is this too broad?
            if (you.see_cell(target()))
                mprf(MSGCH_EXAMINE_FILTER, "There's something in the way.");
            else
                mprf(MSGCH_EXAMINE_FILTER, "You can't see that place.");
            return false;
        }

        if (looking_at_you())
        {
            if (!targets_objects() && targets_enemies())
            {
                if (self == confirm_prompt_type::cancel
                    || self == confirm_prompt_type::prompt
                       && Options.allow_self_target
                              == confirm_prompt_type::cancel)
                {
                    if (moves.interactive)
                        mprf(MSGCH_EXAMINE_FILTER, "That would be overly suicidal.");
                    return false;
                }
                else if (self != confirm_prompt_type::none
                         && Options.allow_self_target
                                != confirm_prompt_type::none)
                {
                    // if it needs to be asked, simply disallow it when
                    // calling in non-interactive mode
                    if (!moves.interactive)
                        return false;
                    return yesno("Really target yourself?", false, 'n',
                                 true, true, false, nullptr, false);
                }
            }

            if (self == confirm_prompt_type::cancel)
            {
                // avoid printing this message when autotargeting -- it doesn't
                // make much sense
                if (moves.interactive)
                    mprf(MSGCH_EXAMINE_FILTER, "Sorry, you can't target yourself.");
                return false;
            }
        }
    }

    // Some odd cases
    if (!moves.isValid && !moves.isCancel)
        return yesno("Are you sure you want to fizzle?", false, 'n');

    return true;
}

// Assuming the target is in view, is line-of-fire blocked?
static bool _blocked_ray(const coord_def &where)
{
    return !exists_ray(you.pos(), where, opc_solid_see);
}

// Try to find an enemy monster to target
bool direction_chooser::find_default_monster_target(coord_def& result) const
{
    // First try to pick our previous monster target.
    const monster* mons_target = _get_current_target();
    if (mons_target != nullptr
        && _want_target_monster(mons_target, mode, hitfunc)
        && in_range(mons_target->pos()))
    {
        result = mons_target->pos();
        return true;
    }
    // If the previous targetted position is at all useful, use it.
    if (!Options.simple_targeting && hitfunc && !prefer_farthest
        && _find_monster_expl(you.prev_grd_targ, mode, needs_path,
                              range, hitfunc, AFF_YES, AFF_MULTIPLE))
    {
        result = you.prev_grd_targ;
        return true;
    }
    // The previous target is no good. Try to find one from scratch.
    bool success = false;

    if (Options.simple_targeting)
    {
        success = _find_square_wrapper(result, 1,
                                       bind(_find_monster, placeholders::_1,
                                            mode, needs_path, range, hitfunc),
                                       hitfunc);
    }
    else
    {
        success = hitfunc && _find_square_wrapper(result, 1,
                                                  bind(_find_monster_expl,
                                                       placeholders::_1, mode,
                                                       needs_path, range,
                                                       hitfunc,
                                                       // First try to bizap
                                                       AFF_MULTIPLE, AFF_YES),
                                                  hitfunc)
                  || _find_square_wrapper(result, 1,
                                          bind(restricts == DIR_SHADOW_STEP ?
                                               _find_shadow_step_mons :
                                               _find_monster,
                                               placeholders::_1, mode,
                                               needs_path, range, hitfunc),
                                          hitfunc);
    }

    // This is used for three things:
    // * For all LRD targetting
    // * To aim explosions so they try to miss you
    // * To hit monsters in LOS that are outside of normal range, but
    //   inside explosion/cloud range
    if (!Options.simple_targeting && hitfunc
        && hitfunc->can_affect_outside_range()
        && (!hitfunc->set_aim(result)
            || hitfunc->is_affected(result) < AFF_YES
            || hitfunc->is_affected(you.pos()) > AFF_NO))
    {
        coord_def old_result;
        if (success)
            old_result = result;
        for (aff_type mon_aff : { AFF_YES, AFF_MAYBE })
        {
            for (aff_type allowed_self_aff : { AFF_NO, AFF_MAYBE, AFF_YES })
            {
                success = _find_square_wrapper(result, 1,
                                       bind(_find_monster_expl,
                                            placeholders::_1, mode,
                                            needs_path, range, hitfunc,
                                            mon_aff, allowed_self_aff),
                                       hitfunc);
                if (success)
                {
                    // If we're hitting ourselves anyway, just target the
                    // monster's position (this looks less strange).
                    if (allowed_self_aff == AFF_YES && !old_result.origin())
                        result = old_result;
                    break;
                }
            }
            if (success)
                break;
        }
    }
    if (success)
        return true;

    // If we couldn't, maybe it was because of line-of-fire issues.
    // Check if that's happening, and inform the user (because it's
    // pretty confusing.)
    if (needs_path
        && _find_square_wrapper(result, 1,
                                bind(restricts == DIR_SHADOW_STEP ?
                                     _find_shadow_step_mons : _find_monster,
                                     placeholders::_1, mode, false,
                                     range, hitfunc),
                               hitfunc))
    {
        // Special colouring in tutorial or hints mode.
        const bool need_hint = Hints.hints_events[HINT_TARGET_NO_FOE];
        // TODO: this seems to trigger when there are no monsters in range
        // of the hitfunc, regardless of what's in the way, and it shouldn't.
        mprf(need_hint ? MSGCH_TUTORIAL : MSGCH_PROMPT,
            "All monsters which could be auto-targeted are covered by "
            "a wall or statue which interrupts your line of fire, even "
            "though it doesn't interrupt your line of sight.");

        if (need_hint)
        {
            mprf(MSGCH_TUTORIAL, "To return to the main mode, press <w>Escape</w>.");
            Hints.hints_events[HINT_TARGET_NO_FOE] = false;
        }
    }
    return false;
}

// Find a good square to start targeting from.
coord_def direction_chooser::find_default_target() const
{
    coord_def result = you.pos();
    bool success = false;

    if (targets_objects())
    {
        // First, try to find a particularly relevant item (autopickup).
        // Barring that, just try anything.
        success = _find_square_wrapper(result, 1,
                                       bind(_find_autopickup_object,
                                            placeholders::_1,
                                            needs_path, range, hitfunc),
                                       hitfunc,
                                       LS_FLIPVH)
               || _find_square_wrapper(result, 1,
                                       bind(_find_object, placeholders::_1,
                                            needs_path, range, hitfunc),
                                       hitfunc,
                                       LS_FLIPVH);
    }
    else if ((mode != TARG_ANY && mode != TARG_FRIEND)
             || self == confirm_prompt_type::cancel)
    {
        success = find_default_monster_target(result);
    }

    if (!success)
        result = you.pos();

    return result;
}

const coord_def& direction_chooser::target() const
{
    return moves.target;
}

void direction_chooser::set_target(const coord_def& new_target)
{
    moves.target = new_target;
}

#ifdef USE_TILE
static tileidx_t _tileidx_aff_type(aff_type aff)
{
    if (aff < AFF_YES)
        return TILE_RAY_OUT_OF_RANGE;
    else if (aff == AFF_YES)
        return TILE_RAY;
    else if (aff == AFF_LANDING)
        return TILE_LANDING;
    else if (aff == AFF_MULTIPLE)
        return TILE_RAY_MULTI;
    else
        return 0;
}
#endif

#ifndef USE_TILE_LOCAL
static colour_t _colour_aff_type(aff_type aff, bool target)
{
    if (aff < 0)
        return DARKGREY;
    else if (aff < AFF_YES)
        return target ? RED : MAGENTA;
    else if (aff == AFF_YES)
        return target ? LIGHTRED : LIGHTMAGENTA;
    else if (aff == AFF_LANDING)
        return target ? LIGHTGREEN : GREEN;
    else if (aff == AFF_MULTIPLE)
        return target ? LIGHTCYAN : CYAN;
    else
        die("unhandled aff %d", aff);
}
#endif

static void _draw_ray_cell(screen_cell_t& cell, coord_def p, bool on_target,
                           aff_type aff)
{
#ifdef USE_TILE
    UNUSED(on_target, p);
    cell.tile.dngn_overlay[cell.tile.num_dngn_overlay++] =
        _tileidx_aff_type(aff);
#endif
#ifndef USE_TILE_LOCAL
    const auto bcol = _colour_aff_type(aff, on_target);
    const auto mbcol = on_target ? bcol : bcol | COLFLAG_REVERSE;
    const auto cglyph = _get_ray_glyph(p, bcol, '*', mbcol);
    cell.glyph = cglyph.ch;
    cell.colour = cglyph.col;
#endif
}

void direction_chooser_renderer::render(crawl_view_buffer& vbuf)
{
    if (crawl_state.invisible_targeting)
        return;
    m_directn.draw_beam(vbuf);
    m_directn.highlight_summoner(vbuf);
}

void direction_chooser::draw_beam(crawl_view_buffer &vbuf)
{
    if (!show_beam)
        return;

    // Use the new API if implemented.
    if (hitfunc)
    {
        if (behaviour->targeted() && !hitfunc->set_aim(target()))
            return;
        const los_type los = hitfunc->can_affect_unseen()
                                            ? LOS_NONE : LOS_DEFAULT;
        for (radius_iterator ri(you.pos(), los); ri; ++ri)
        {
            aff_type aff = hitfunc->is_affected(*ri);
            if (aff
                && (!feat_is_solid(env.grid(*ri)) || hitfunc->can_affect_walls()))
            {
                auto& cell = vbuf(grid2view(*ri) - 1);
                _draw_ray_cell(cell, *ri, *ri == target(), aff);
            }
        }

        return;
    }

    // If we don't have a new beam to show, we're done.
    if (!have_beam)
        return;

    // We shouldn't ever get a beam to an out-of-LOS target.
    ASSERT(you.see_cell(target()));

    // Work with a copy in order not to mangle anything.
    ray_def ray = beam;

    // Draw the new ray with magenta '*'s, not including your square
    // or the target square. Out-of-range cells get grey '*'s instead.
    for (; ray.pos() != target(); ray.advance())
    {
        const coord_def p = ray.pos();
        ASSERT(you.see_cell(p));

        if (p == you.pos())
            continue;

        const bool inrange = in_range(p);
        auto& cell = vbuf(grid2view(p) - 1);
#ifdef USE_TILE
        cell.tile.dngn_overlay[cell.tile.num_dngn_overlay++] =
            inrange ? TILE_RAY : TILE_RAY_OUT_OF_RANGE;
#endif
#ifndef USE_TILE_LOCAL
        const auto bcol = inrange ? MAGENTA : DARKGREY;
        const auto cglyph = _get_ray_glyph(p, bcol, '*', bcol| COLFLAG_REVERSE);
        cell.glyph = cglyph.ch;
        cell.colour = cglyph.col;
#endif
    }
    textcolour(LIGHTGREY);

    // Only draw the ray over the target on tiles.
#ifdef USE_TILE
    auto& cell = vbuf(grid2view(target()) - 1);
    cell.tile.dngn_overlay[cell.tile.num_dngn_overlay++] =
        in_range(ray.pos()) ? TILE_RAY : TILE_RAY_OUT_OF_RANGE;
#endif
}

bool direction_chooser::in_range(const coord_def& p) const
{
    if (!behaviour->targeted())
        return true;
    if (hitfunc)
        return hitfunc->valid_aim(p);
    return range < 0 || grid_distance(p, you.pos()) <= range;
}

// Cycle to either the next (dir == 1) or previous (dir == -1) object
// and update output accordingly if successful.
void direction_chooser::object_cycle(int dir)
{
    if (_find_square_wrapper(objfind_pos, dir,
                             bind(_find_object, placeholders::_1, needs_path,
                                  range, hitfunc),
                             hitfunc,
                             dir > 0 ? LS_FLIPVH : LS_FLIPHV))
    {
        set_target(objfind_pos);
    }
    else
        flush_input_buffer(FLUSH_ON_FAILURE);
}

void direction_chooser::monster_cycle(int dir)
{
    if (prefer_farthest)
        dir = -dir; // cycle from furthest to closest
    if (_find_square_wrapper(monsfind_pos, dir,
                             bind(_find_monster, placeholders::_1, mode,
                                  needs_path, range, hitfunc),
                             hitfunc))
    {
        set_target(monsfind_pos);
    }
    else
        flush_input_buffer(FLUSH_ON_FAILURE);
}

void direction_chooser::feature_cycle_forward(int feature)
{
    if (_find_square_wrapper(objfind_pos, 1,
                             [feature](const coord_def& where)
                             {
                                 return map_bounds(where)
                                        && (you.see_cell(where)
                                            || env.map_knowledge(where).seen())
                                        && is_feature(feature, where);
                             },
                             hitfunc,
                             LS_FLIPVH))
    {
        set_target(objfind_pos);
    }
    else
        flush_input_buffer(FLUSH_ON_FAILURE);
}

void direction_chooser::update_previous_target() const
{
    you.prev_targ = MHITNOT;
    you.prev_grd_targ.reset();

    // Maybe we should except just_looking here?
    const monster* m = monster_at(target());
    if (m && you.can_see(*m))
        you.prev_targ = m->mindex();
    else if (looking_at_you())
        you.prev_targ = MHITYOU;
    else
        you.prev_grd_targ = target();
}

bool direction_chooser::select(bool allow_out_of_range, bool endpoint)
{
    const monster* mons = monster_at(target());

    if (restricts == DIR_SHADOW_STEP)
    {
        targeter_shadow_step &tgt =
            *static_cast<targeter_shadow_step*>(hitfunc);
        if (!tgt.has_additional_sites(target()))
            return false;
    }

    // leap and shadow step never allow selecting from past the target point
    if ((restricts == DIR_LEAP
         || restricts == DIR_SHADOW_STEP
         || !allow_out_of_range)
        && !in_range(target()))
    {
        return false;
    }
    moves.isEndpoint = endpoint || (mons && _mon_exposed(mons));
    moves.isValid  = true;
    moves.isTarget = true;
    update_previous_target();
    return true;
}

bool direction_chooser::pickup_item()
{
    item_def *ii = nullptr;
    if (in_bounds(target()))
        ii = env.map_knowledge(target()).item();
    if (!ii || !ii->is_valid(true))
    {
        mprf(MSGCH_EXAMINE_FILTER, "You can't see any item there.");
        return false;
    }
    ii->flags |= ISFLAG_THROWN; // make autoexplore greedy

    // From this point, if there's no item, we'll fake one. False info means
    // it's out of bounds and taken, or a mimic.
    item_def *item = 0;
    unsigned short it = env.igrid(target());
    if (it != NON_ITEM)
    {
        item = &env.item[it];
        // Check if it appears to be the same item.
        if (!item->is_valid()
            || ii->base_type != item->base_type
            || ii->sub_type != item->sub_type
               // TODO: check for different unidentified items of the same base type
               && (!item_type_has_unidentified(item->base_type)
                   || ii->sub_type == get_max_subtype(item->base_type))
            || ii->get_colour() != item->get_colour())
        {
            item = 0;
        }
    }
    if (item)
        item->flags |= ISFLAG_THROWN;

    if (!just_looking) // firing/casting prompt
    {
        mprf(MSGCH_EXAMINE_FILTER, "Marked for pickup.");
        return false;
    }

    moves.isValid  = true;
    moves.isTarget = true;
    update_previous_target();
    return true;
}

bool direction_chooser::handle_signals()
{
    // If we've received a HUP signal then the user can't choose a
    // target.
    if (crawl_state.seen_hups)
    {
        moves.isValid  = false;
        moves.isCancel = true;
        moves.cmd_result = CMD_NO_CMD;

        mprf(MSGCH_ERROR, "Targeting interrupted by HUP signal.");
        return true;
    }
    return false;
}

// Print out the initial prompt when targeting starts.
// Prompts might get scrolled off if you have too few lines available;
// we'll live with that.
void direction_chooser::show_initial_prompt()
{
    if (crawl_state.invisible_targeting)
        return;
    behaviour->update_top_prompt(&top_prompt);
    describe_cell();
    const string err = behaviour ? behaviour->get_error() : "";
    if (!err.empty())
        mprf(MSGCH_PROMPT, "%s", err.c_str()); // can this push the prompt one line too tall?
}

void direction_chooser::print_target_description(bool &did_cloud) const
{
    if (targets_objects())
        print_target_object_description();
    else
        print_target_monster_description(did_cloud);

    if (!in_range(target()))
    {
        mprf(MSGCH_EXAMINE_FILTER, "%s",
             hitfunc ? hitfunc->why_not.c_str() : "Out of range.");
    }
}

string direction_chooser::target_interesting_terrain_description() const
{
    const dungeon_feature_type feature = env.grid(target());

    // Only features which can make you lose the item are interesting.
    // FIXME: extract the naming logic from here and use
    // feat_has_solid_floor().
    switch (feature)
    {
    case DNGN_DEEP_WATER: return "water";
    case DNGN_LAVA:       return "lava";
    default:              return "";
    }
}

string direction_chooser::target_cloud_description() const
{
    if (cloud_struct* cloud = cloud_at(target()))
        return cloud->cloud_name(true);
    else
        return "";
}

template<typename C1, typename C2>
static void _append_container(C1& container_base, const C2& container_append)
{
    container_base.insert(container_base.end(),
                          container_append.begin(), container_append.end());
}

string direction_chooser::target_sanctuary_description() const
{
    return is_sanctuary(target()) ? "sanctuary" : "";
}

string direction_chooser::target_silence_description() const
{
    return silenced(target()) ? "silenced" : "";
}

static void _push_back_if_nonempty(const string& str, vector<string>* vec)
{
    if (!str.empty())
        vec->push_back(str);
}

void direction_chooser::print_target_monster_description(bool &did_cloud) const
{
    // Do we see anything?
    const monster* mon = monster_at(target());
    if (!mon)
        return;

    const bool visible = you.can_see(*mon);
    const bool exposed = _mon_exposed(mon);
    if (!visible && !exposed)
        return;

    // OK, now we know that we have something to describe.
    vector<string> suffixes;
    string text;
    // Cell features go first.
    _append_container(suffixes, target_cell_description_suffixes());
    if (visible)
    {
        monster_info mi(mon);
        // Only describe the monster if you can actually see it.
        _append_container(suffixes, monster_description_suffixes(mi));
        text = get_monster_equipment_desc(mi);
    }
    else
        text = localise("Disturbance");

    // Build the final description string.
    if (!suffixes.empty())
    {
        text += localise(" (")
            + localise(comma_separated_line(suffixes.begin(), suffixes.end(), ", "))
            + localise(")");
    }

    mprf(MSGCH_PROMPT, "%s: <lightgrey>%s</lightgrey>", // @noloc
         target_prefix ? target_prefix : !behaviour->targeted() ? "Look" : "Aim",
         text.c_str());

    // If there's a cloud here, it's been described.
    did_cloud = true;
}

// FIXME: this should really take a cell as argument.
vector<string> direction_chooser::target_cell_description_suffixes() const
{
    vector<string> suffixes;
    // Things which describe the cell.
    _push_back_if_nonempty(target_cloud_description(), &suffixes);
    _push_back_if_nonempty(target_sanctuary_description(), &suffixes);
    _push_back_if_nonempty(target_silence_description(), &suffixes);
    _push_back_if_nonempty(target_interesting_terrain_description(), &suffixes);

    return suffixes;
}

vector<string> direction_chooser::monster_description_suffixes(
    const monster_info& mi) const
{
    vector<string> suffixes;

    _push_back_if_nonempty(mi.wounds_description(true), &suffixes);
    _push_back_if_nonempty(mi.constriction_description(), &suffixes);
    _append_container(suffixes, mi.attributes());
    _append_container(suffixes, _get_monster_desc_vector(mi));
    _append_container(suffixes, behaviour->get_monster_desc(mi));

    return suffixes;
}

void direction_chooser::print_target_object_description() const
{
    if (!you.see_cell(target()))
        return;

    const item_def* item = top_item_at(target());
    if (!item)
        return;

    // FIXME: remove the duplication with print_items_description().
    mprf(MSGCH_PROMPT, "%s: %s",
         target_prefix ? target_prefix : "Aim",
         menu_colour_item_name(*item, DESC_A).c_str());
}

void direction_chooser::print_items_description() const
{
    if (!in_bounds(target()))
        return;

    auto items = item_list_on_square(you.visible_igrd(target()));

    if (items.empty())
        return;

    if (items.size() == 1)
    {
        mprf(MSGCH_FLOOR_ITEMS, "<cyan>Item here:</cyan> %s.",
             menu_colour_item_name(*items[0], DESC_A).c_str());
    }
    else
        mprf(MSGCH_FLOOR_ITEMS, "<cyan>Items here: </cyan> %s.", item_message(items).c_str());
}

void direction_chooser::print_floor_description(bool boring_too) const
{
    const dungeon_feature_type feat = env.grid(target());
    if (!boring_too && feat == DNGN_FLOOR)
        return;

#ifdef DEBUG_DIAGNOSTICS
    // [ds] Be more verbose in debug mode.
    if (you.wizard)
        _debug_describe_feature_at(target());
    else
#endif
    mprf(MSGCH_EXAMINE_FILTER, "%s.",
         feature_description_at(target(), true).c_str());
}

void direction_chooser::reinitialize_move_flags()
{
    moves.isValid    = false;
    moves.isTarget   = false;
    moves.isCancel   = false;
    moves.isEndpoint = false;
    moves.choseRay   = false;
}

// Returns true if we've completed targeting.
bool direction_chooser::select_compass_direction(const coord_def& delta)
{
    if (restricts != DIR_TARGET && restricts != DIR_SHADOW_STEP)
    {
        // A direction is allowed, and we've selected it.
        moves.delta    = delta;
        // Needed for now...eventually shouldn't be necessary
        set_target(you.pos() + moves.delta);
        moves.isValid  = true;
        moves.isTarget = false;
        have_beam      = false;
        show_beam      = false;
        moves.choseRay = false;
        return true;
    }
    else
    {
        // Direction not allowed, so just move in that direction.
        // Maybe make this a bigger jump?
        set_target(target() + delta * 3);
        return false;
    }
}

void direction_chooser::toggle_beam()
{
    if (!needs_path)
    {
        mprf(MSGCH_EXAMINE_FILTER, "This spell doesn't need a beam path.");
        return;
    }

    show_beam = !show_beam;
    need_viewport_redraw = true;

    if (show_beam)
    {
        have_beam = find_ray(you.pos(), target(), beam,
                             opc_solid_see, you.current_vision);
    }
}

bool direction_chooser::select_previous_target()
{
    if (const monster* mon_target = _get_current_target())
    {
        // We have all the information we need.
        moves.isValid  = true;
        moves.isTarget = true;
        set_target(mon_target->pos());
        if (!just_looking)
            have_beam = false;

        return !just_looking;
    }
    else
    {
        mprf(MSGCH_EXAMINE_FILTER, "Your target is gone.");
        flush_prev_message();
        return false;
    }
}

bool direction_chooser::looking_at_you() const
{
    return target() == you.pos();
}

void direction_chooser::handle_movement_key(command_type key_command,
                                            bool* loop_done)
{
    const int compass_idx = _targeting_cmd_to_compass(key_command);
    if (compass_idx != -1)
    {
        const coord_def& delta = Compass[compass_idx];
        const bool unshifted = (shift_direction(key_command) != key_command);
        if (unshifted)
            set_target(target() + delta);
        else if (!allow_shift_dir)
            mpr("You can't do that.");
        else
            *loop_done = select_compass_direction(delta);
    }
}

void direction_chooser::handle_wizard_command(command_type key_command,
                                              bool* loop_done)
{
#ifdef WIZARD
    if (!you.wizard)
        return;

    monster* const m = monster_at(target());
    string marker_result = "";

    // These commands do something even if there's no monster there.
    switch (key_command)
    {
    case CMD_TARGET_WIZARD_MOVE:
        wizard_move_player_or_monster(target());
        *loop_done = true;
        return;

    case CMD_TARGET_WIZARD_MISCAST:
        if (m)
            debug_miscast(m->mindex());
        else if (looking_at_you())
            debug_miscast(NON_MONSTER);
        return;

    // Note that this is a wizard-only command.
    case CMD_TARGET_CYCLE_BEAM:
        show_beam = true;
        have_beam = find_ray(you.pos(), target(), beam,
                             opc_solid_see, you.current_vision, show_beam);
        need_viewport_redraw = true;
        return;

    case CMD_TARGET_WIZARD_DEBUG_PORTAL:
        mprf(MSGCH_DIAGNOSTICS, "Trying to run portal debug at %d/%d...",
            target().x, target().y);

        marker_result =
            env.markers.property_at(target(), MAT_ANY, "portal_debug");

        mprf(MSGCH_DIAGNOSTICS, "Got result: %s!",
            marker_result.empty() ? "nothing" : marker_result.c_str());

        return;

    case CMD_TARGET_WIZARD_HURT_MONSTER:
        if (looking_at_you())
        {
            set_hp(1);
            print_stats();
            update_screen();
        }
        break;

    case CMD_TARGET_WIZARD_CREATE_MIMIC:
        if (target() != you.pos())
        {
            wizard_create_feature(target());
            need_viewport_redraw = true;
        }
        return;

    default:
        break;
    }

    // Everything below here doesn't work if there's no monster.
    if (!m)
        return;

    const int mid = m->mindex();

    switch (key_command)
    {
    case CMD_TARGET_WIZARD_PATHFIND:      debug_pathfind(mid);      break;
    case CMD_TARGET_WIZARD_DEBUG_MONSTER: debug_stethoscope(mid);   break;
    case CMD_TARGET_WIZARD_MAKE_SHOUT: debug_make_monster_shout(m); break;
    case CMD_TARGET_WIZARD_MAKE_FRIENDLY:
        _wizard_make_friendly(m);
        need_text_redraw = true;
        break;

    case CMD_TARGET_WIZARD_GIVE_ITEM:  wizard_give_monster_item(m); break;
    case CMD_TARGET_WIZARD_POLYMORPH:  wizard_polymorph_monster(m); break;

    case CMD_TARGET_WIZARD_GAIN_LEVEL:
        wizard_gain_monster_level(m);
        break;

    case CMD_TARGET_WIZARD_BLESS_MONSTER:
        wizard_apply_monster_blessing(m);
        break;

    case CMD_TARGET_WIZARD_MAKE_SUMMONED:
        wizard_make_monster_summoned(m);
        break;

    case CMD_TARGET_WIZARD_HEAL_MONSTER:
        if (m->hit_points < m->max_hit_points)
        {
            m->hit_points = m->max_hit_points;
            need_all_redraw = true;
        }
        break;

    case CMD_TARGET_WIZARD_HURT_MONSTER:
        m->hit_points = 1;
        mpr("Brought monster down to 1 HP.");
        flush_prev_message();
        break;

    case CMD_TARGET_WIZARD_BANISH_MONSTER:
        m->banish(&you, "", 0, true);
        break;

    case CMD_TARGET_WIZARD_KILL_MONSTER:
        monster_die(*m, KILL_YOU, NON_MONSTER);
        break;

    default:
        return;
    }
    redraw_screen();
    update_screen();
#endif
}

void direction_chooser::do_redraws()
{
    if (crawl_state.invisible_targeting)
        return;

    // Check if our targeting behaviour forces a redraw.
    if (behaviour->should_redraw())
    {
        need_all_redraw = true;
        behaviour->clear_redraw();
    }

    if (need_all_redraw)
    {
        need_viewport_redraw = true;
        need_text_redraw = true;
        need_cursor_redraw = true;
        need_all_redraw = false;
    }

    if (need_viewport_redraw)
    {
        viewwindow(false, false, nullptr, &renderer);
        need_viewport_redraw = false;
    }

    if (need_text_redraw)
    {
        msgwin_clear_temporary();
        describe_cell();
        need_text_redraw = false;
    }

    if (need_cursor_redraw || Options.use_fake_cursor)
    {
        cursorxy(crawl_view.grid2screen(target()));
#ifdef USE_TILE_WEB
        // cursorxy doesn't place the cursor in Webtiles, we do it manually here
        // This is by design, since we don't want to use the mouse cursor for
        // the overview map.
        tiles.place_cursor(CURSOR_MOUSE, target());
#endif
        need_cursor_redraw = false;
    }
}

coord_def direction_chooser::find_summoner()
{
    const monster* mon = monster_at(target());

    if (mon && mon->is_summoned()
        // Don't leak information about rakshasa mirrored illusions.
        && !mon->has_ench(ENCH_PHANTOM_MIRROR)
        // Don't leak information about invisible or out-of-los summons.
        && you.can_see(*mon))
    {
        const monster *summ = monster_by_mid(mon->summoner);
        // Don't leak information about invisible summoners.
        if (summ && you.can_see(*summ))
            return summ->pos();
    }
    return INVALID_COORD;
}

void direction_chooser::highlight_summoner(crawl_view_buffer &vbuf)
{
    const coord_def summ_loc = find_summoner();

    if (summ_loc == INVALID_COORD || !you.see_cell(summ_loc))
        return;

    auto& cell = vbuf(grid2view(summ_loc) - 1);
#ifdef USE_TILE
    cell.tile.is_highlighted_summoner = true;
#endif
#if defined(USE_TILE_WEB) || !defined(USE_TILE)
    cell.colour = CYAN | COLFLAG_REVERSE;
#endif
}

bool direction_chooser::tiles_update_target()
{
#ifdef USE_TILE
    const coord_def& gc = tiles.get_cursor();
    if (gc != NO_CURSOR && map_bounds(gc))
    {
        set_target(gc);
        return true;
    }
#endif
    return false;
}

void direction_chooser::move_to_you()
{
    moves.isValid  = true;
    moves.isTarget = true;
    set_target(you.pos());
    moves.delta.reset();
}

void direction_chooser::full_describe()
{
    vector <monster_info> list_mons;
    vector<item_def> list_items;
    vector<coord_def> list_features;

    vector <monster *> nearby_mons = get_nearby_monsters(true, false, false,
                                                         false, true, true,
                                                         range);
    for (auto m : nearby_mons)
        if (_want_target_monster(m, mode, hitfunc))
            list_mons.push_back(monster_info(m));

    if (targets_objects() || just_looking)
        _get_nearby_items(list_items, needs_path, range, hitfunc);

    if (hitfunc && hitfunc->can_affect_walls() || just_looking)
        _get_nearby_features(list_features, needs_path, range, hitfunc);

    if (list_mons.empty() && list_items.empty() && list_features.empty())
    {
        mprf(MSGCH_EXAMINE_FILTER, "There are no valid targets to list.");
        flush_prev_message();
        return;
    }

    const coord_def choice =
        _full_describe_menu(list_mons, list_items, list_features,
                            just_looking ? "target/travel" : "target");

    if (choice != coord_def(-1, -1))
        set_target(choice);
    need_all_redraw = true;
}

void direction_chooser::describe_target()
{
    if (!map_bounds(target()) || !env.map_knowledge(target()).known())
        return;
    full_describe_square(target(), false);
    need_all_redraw = true;
}

void direction_chooser::show_help()
{
    show_targeting_help();
    redraw_screen();
    update_screen();
    clear_messages(true);
    need_all_redraw = true;
}

bool direction_chooser::process_command(command_type command)
{
    bool loop_done = false;

    // Move flags are volitile, reset them to defaults before each command
    reinitialize_move_flags();

    switch (command)
    {
    case CMD_TARGET_SHOW_PROMPT: describe_cell(); break;

    case CMD_TARGET_TOGGLE_BEAM:
        if (!just_looking)
            toggle_beam();
        break;

    case CMD_TARGET_EXCLUDE:
        if (!just_looking)
            break;

        if (!is_map_persistent())
            mpr("You cannot set exclusions on this level.");
        else
        {
            const bool was_excluded = is_exclude_root(target());
            cycle_exclude_radius(target());
            need_viewport_redraw   = true;
            const bool is_excluded = is_exclude_root(target());
            if (!was_excluded && is_excluded)
                mpr("Placed new exclusion.");
            else if (was_excluded && !is_excluded)
                mpr("Removed exclusion.");
            else
                mpr("Reduced exclusion size to a single square.");
        }

        need_cursor_redraw = true;
        break;

    case CMD_TARGET_FIND_YOU:       move_to_you(); break;
    case CMD_TARGET_FIND_TRAP:      feature_cycle_forward('^');  break;
    case CMD_TARGET_FIND_PORTAL:    feature_cycle_forward('\\'); break;
    case CMD_TARGET_FIND_ALTAR:     feature_cycle_forward('_');  break;
    case CMD_TARGET_FIND_UPSTAIR:   feature_cycle_forward('<');  break;
    case CMD_TARGET_FIND_DOWNSTAIR: feature_cycle_forward('>');  break;

    case CMD_TARGET_MAYBE_PREV_TARGET:
        loop_done = looking_at_you() ? select_previous_target()
                                     : select(false, false);
        break;

    case CMD_TARGET_PREV_TARGET: loop_done = select_previous_target(); break;

    // some modifiers to the basic selection command
    case CMD_TARGET_SELECT:          loop_done = select(false, false); break;
    case CMD_TARGET_SELECT_FORCE:    loop_done = select(true,  false); break;
    case CMD_TARGET_SELECT_ENDPOINT: loop_done = select(false, true);  break;
    case CMD_TARGET_SELECT_FORCE_ENDPOINT: loop_done = select(true,true); break;

#ifdef USE_TILE
    case CMD_TARGET_MOUSE_SELECT:
        if (tiles_update_target())
            loop_done = select(false, false);
        break;

    case CMD_TARGET_MOUSE_MOVE: tiles_update_target(); break;
#endif

    case CMD_TARGET_GET:             loop_done = pickup_item(); break;

    case CMD_TARGET_CYCLE_BACK:
        if (!targets_objects())
            monster_cycle(-1);
        else
            object_cycle(-1);
        break;

    case CMD_TARGET_OBJ_CYCLE_BACK:
        object_cycle(-1);
        break;

    case CMD_TARGET_CYCLE_FORWARD:
        if (!targets_objects())
            monster_cycle(1);
        else
            object_cycle(1);
        break;

    case CMD_TARGET_OBJ_CYCLE_FORWARD:
        object_cycle(1);
        break;

    case CMD_TARGET_CANCEL:
        loop_done = true;
        moves.isCancel = true;
        break;

    case CMD_TARGET_FULL_DESCRIBE: full_describe(); break;
    case CMD_TARGET_DESCRIBE: describe_target(); break;
    case CMD_TARGET_HELP:     show_help();       break;

    default:
        if (moves.fire_context
            && moves.fire_context->targeter_handles_key(command))
        {
            // because of the somewhat convoluted way in which action selection
            // is handled, some commands can only be handled if the direction
            // chooser has been called via a quiver::action. Otherwise, we
            // ignore these commands.
            moves.isValid = false;
            moves.isCancel = true;
            moves.cmd_result = static_cast<int>(command);
            loop_done = true;
            break;
        }
        // Some blocks of keys with similar handling.
        handle_movement_key(command, &loop_done);
        handle_wizard_command(command, &loop_done);
        break;
    }

    return loop_done;
}

void direction_chooser::finalize_moves()
{
    moves.choseRay = have_beam;
    moves.ray = beam;

    // We need this for directional explosions, otherwise they'll explode one
    // square away from the player.
    _extend_move_to_edge(moves);

#ifdef USE_TILE
    tiles.place_cursor(CURSOR_MOUSE, NO_CURSOR);
#endif
}

class UIDirectionChooserView
#ifdef USE_TILE_LOCAL
    : public ui::Widget
#else
    : public ui::OverlayWidget
#endif
{
public:
    UIDirectionChooserView(direction_chooser& dc) :
        m_dc(dc), old_target(dc.target())
    {
    }
    ~UIDirectionChooserView() {}

    void _render() override
    {
        if (crawl_state.invisible_targeting)
            return;

#ifndef USE_TILE_LOCAL
        // This call ensures that the hud will get redrawn in console any time
        // need_all_redraw is set. Minimally, this needs to happen on the
        // initial call, as well as after any popups on top of this widget.
        if (m_dc.need_all_redraw)
            redraw_screen(false);
#endif

#ifdef USE_TILE_LOCAL
        // We always have to redraw the viewport, because ui::redraw() will call
        // redraw_screen in case the window has been resized.
        m_dc.need_viewport_redraw = true;
#endif
        m_dc.do_redraws();

#ifdef USE_TILE_LOCAL
        tiles.render_current_regions();
        glmanager->reset_transform();
#endif
    }

    void _allocate_region() override
    {
        m_dc.need_all_redraw = true;
        _expose();
    }

    bool on_event(const ui::Event& ev) override
    {
        if (ev.type() == ui::Event::Type::KeyDown)
        {
            auto key = static_cast<const ui::KeyEvent&>(ev).key();
            key = unmangle_direction_keys(key, KMC_TARGETING, false);

            const auto command = m_dc.behaviour->get_command(key);
            // XX a bit ugly to do this here..
            if (m_dc.behaviour->needs_path != MB_MAYBE)
            {
                m_dc.needs_path = tobool(m_dc.behaviour->needs_path, true);
                m_dc.show_beam = !m_dc.just_looking && m_dc.needs_path;
                // XX code duplication
                m_dc.have_beam = m_dc.show_beam
                                 && find_ray(you.pos(), m_dc.target(), m_dc.beam,
                                             opc_solid_see, you.current_vision);
                m_dc.need_text_redraw = true;
                m_dc.need_viewport_redraw = true;
                m_dc.need_cursor_redraw = true;
            }

            string top_prompt = m_dc.top_prompt;
            m_dc.behaviour->update_top_prompt(&top_prompt);

            if (m_dc.top_prompt != top_prompt)
            {
                _expose();
                m_dc.top_prompt = top_prompt;
            }

            process_command(command);

            // Flush the input buffer before the next command.
            if (!crawl_state.is_replaying_keys())
                flush_input_buffer(FLUSH_BEFORE_COMMAND);
            return true;
        }

#ifdef USE_TILE_LOCAL
        if (ev.type() == ui::Event::Type::MouseMove
            || ev.type() == ui::Event::Type::MouseDown)
        {
            auto wm_event = to_wm_event(static_cast<const ui::MouseEvent&>(ev));
            tiles.handle_mouse(wm_event);
            process_command(ev.type() == ui::Event::Type::MouseMove ?
                                CMD_TARGET_MOUSE_MOVE :
                                CMD_TARGET_MOUSE_SELECT);
            return true;
        }
#endif

        return false;
    }

    void process_command(command_type cmd)
    {
        // the chooser needs a cursor, but we need to hide it here
        cursor_control cc(false);
        bool loop_done = m_dc.process_command(cmd);

        // Don't allow going out of bounds.
        if (!crawl_view.in_viewport_g(m_dc.target()))
            m_dc.set_target(old_target);

        if (loop_done)
        {
            m_is_alive = false;

            if (!m_dc.just_looking && !m_dc.move_is_ok())
            {
                m_dc.moves.isCancel = true;
                m_dc.moves.isValid = false;
            }
            return;
        }

        // Redraw whatever is necessary.
        if (old_target != m_dc.target())
        {
            m_dc.have_beam = m_dc.show_beam
                             && find_ray(you.pos(), m_dc.target(), m_dc.beam,
                                         opc_solid_see, you.current_vision);
            m_dc.need_text_redraw = true;
            m_dc.need_viewport_redraw = true;
            m_dc.need_cursor_redraw = true;
        }

        if (m_dc.need_viewport_redraw || m_dc.need_cursor_redraw
            || m_dc.need_text_redraw || m_dc.need_all_redraw)
        {
            _expose();
        }

        old_target = m_dc.target();
    }

    bool is_alive()
    {
        return m_is_alive;
    }

private:
    direction_chooser& m_dc;
    coord_def old_target;
    bool m_is_alive = true;
};

void direction_chooser::update_validity()
{
    if (!select(false, moves.isEndpoint) || !move_is_ok())
    {
        moves.isCancel = true;
        moves.isValid = false;
        return;
    }
    // select() should handle setting bools appropriately on success
}

bool direction_chooser::noninteractive()
{
    // if target is unset, this will find previous or closest target; if
    // target is set this will adjust targeting depending on custom
    // behavior
    if (moves.find_target)
        set_target(find_default_target());

    update_validity();
    finalize_moves();
    moves.cmd_result = moves.isValid && !moves.isCancel ? CMD_FIRE : CMD_NO_CMD;
    return moves.cmd_result == CMD_FIRE;
}

bool direction_chooser::choose_direction()
{
#ifdef USE_TILE
    ui::cutoff_point ui_cutoff_point;
#endif

#ifndef USE_TILE_LOCAL
    cursor_control ccon(!Options.use_fake_cursor);
#endif
#ifdef DGAMELAUNCH
    suppress_dgl_clrscr no_blinking;
#endif

    mouse_control mc(needs_path && !just_looking ? MOUSE_MODE_TARGET_PATH
                                                 : MOUSE_MODE_TARGET);
    targeter_smite legacy_range(&you, range, 0, 0, true);
    range_view_annotator rva(hitfunc ? hitfunc :
                             (range >= 0) ? &legacy_range : nullptr);

    // init
    moves.delta.reset();

    // Find a default target.
    set_target(!default_place.origin() ? default_place
                                       : find_default_target());

    objfind_pos = monsfind_pos = target();
    if (prefer_farthest && moves.target != you.pos())
        monster_cycle(1);

    // If requested, show the beam on startup.
    if (show_beam)
    {
        have_beam = find_ray(you.pos(), target(), beam,
                             opc_solid_see, you.current_vision);
        need_viewport_redraw = have_beam;
    }
    if (hitfunc)
        need_viewport_redraw = true;

    clear_messages();
    msgwin_temporary_mode tmp;

    unwind_bool save_more(crawl_state.show_more_prompt, false);
    show_initial_prompt();
    need_text_redraw = false;

    auto directn_view = make_shared<UIDirectionChooserView>(*this);

#ifdef USE_TILE_LOCAL
    unwind_bool inhibit_rendering(ui::should_render_current_regions, false);
#endif

    // TODO: ideally crawl_state.invisible_targeting would suppress the redraws
    // associated with these ui calls, but I'm not sure of a clean way to make
    // that work
    ui::push_layout(directn_view, KMC_TARGETING);
    directn_view->_queue_allocation();
    while (directn_view->is_alive() && !handle_signals())
        ui::pump_events();
    ui::pop_layout();

    finalize_moves();
    if (moves.isValid && !moves.isCancel)
        moves.cmd_result = CMD_FIRE;
    return moves.isValid;
}

string get_terse_square_desc(const coord_def &gc)
{
    string desc = "";
    const char *unseen_desc = "[unseen terrain]";

    if (gc == you.pos())
        desc = you.your_name;
    else if (!map_bounds(gc))
        desc = localise(unseen_desc);
    else if (!you.see_cell(gc))
    {
        if (env.map_knowledge(gc).seen())
        {
            desc = feature_description_at(gc, false, DESC_PLAIN);
            // Suppress floor description
            if (desc == "floor")
                desc = "";
            else
            {
                // i18n: TODO: Fix this
                desc = "[" + localise(desc) + "]";
            }
        }
        else
            desc = localise(unseen_desc);
    }
    else if (monster_at(gc) && you.can_see(*monster_at(gc)))
    {
            desc = monster_at(gc)->full_name(DESC_PLAIN);
            desc = localise(desc);
    }
    else if (you.visible_igrd(gc) != NON_ITEM)
    {
        if (env.item[you.visible_igrd(gc)].defined())
        {
            desc = env.item[you.visible_igrd(gc)].name(DESC_PLAIN);
            desc = localise(desc);
        }
    }
    else
    {
        desc = feature_description_at(gc, false, DESC_PLAIN);
        // Suppress floor description
        if (desc == "floor")
            desc = "";
        else
        {
            // i18n: TODO: Fix this
            desc = localise(desc);
        }
    }

    return desc;
}

void terse_describe_square(const coord_def &c, bool in_range)
{
    if (!you.see_cell(c))
        _describe_oos_square(c);
    else if (in_bounds(c))
        _describe_cell(c, in_range);
}

// Get description of the "top" thing in a square; for mouseover text.
void get_square_desc(const coord_def &c, describe_info &inf)
{
    const dungeon_feature_type feat = env.map_knowledge(c).feat();
    const cloud_type cloud = env.map_knowledge(c).cloud();

    if (const monster_info *mi = env.map_knowledge(c).monsterinfo())
    {
        // First priority: monsters.
        string desc = uppercase_first(get_monster_equipment_desc(*mi))
                    + localise(".") + "\n";
        const string wounds = mi->wounds_description_sentence();
        if (!wounds.empty())
            desc += uppercase_first(wounds) + "\n";
        const string constrictions = mi->constriction_description();
        if (!constrictions.empty())
        {
            desc += add_punctuation(uppercase_first(constrictions), ".", false);
            desc += "\n";
        }
        desc += _get_monster_desc(*mi);

        inf.title = desc;

        bool temp = false;
        get_monster_db_desc(*mi, inf, temp);
    }
    else if (const item_def *obj = env.map_knowledge(c).item())
    {
        // Second priority: objects.
        get_item_desc(*obj, inf);
    }
    else if (feat != DNGN_UNSEEN && feat != DNGN_FLOOR
             && !feat_is_wall(feat) && !feat_is_tree(feat))
    {
        // Third priority: features.
        get_feature_desc(c, inf);
    }
    else // Fourth priority: clouds.
        inf.body << get_cloud_desc(cloud);
}

// Show a description of the only thing on a square, or a selection menu with
// visible things on the square if there are many. For x-v and similar contexts.
// Used for both in- and out-of-los cells.
void full_describe_square(const coord_def &c, bool cleanup)
{
    vector<monster_info> list_mons;
    vector<item_def> list_items;
    vector<coord_def> list_features;
    int quantity = 0;

    const monster_info *mi = env.map_knowledge(c).monsterinfo();
    item_def *obj = env.map_knowledge(c).item();
    const dungeon_feature_type feat = env.map_knowledge(c).feat();

    if (mi)
    {
        list_mons.emplace_back(*mi);
        ++quantity;
    }
    if (obj)
    {
        list_items = item_list_in_stash(c);
        quantity += list_items.size();
    }
    // I'm not sure if features should be included. But it seems reasonable to
    // at least include what full_describe_view shows
    if (feat_stair_direction(feat) != CMD_NO_CMD || feat_is_trap(feat))
    {
        list_features.push_back(c);
        ++quantity;
    }

    if (quantity > 1)
    {
        _full_describe_menu(list_mons, list_items, list_features, "", true,
                            you.see_cell(c) ? "What do you want to examine?"
                                            : "What do you want to remember?");
    }
    else if (quantity == 1)
    {
        if (mi)
            describe_monsters(*mi);
        else if (list_items.size())
            describe_item(*obj);
        else
            describe_feature_wide(c);
    }
    else
        describe_feature_wide(c);

    if (cleanup)
    {
        redraw_screen();
        update_screen();
        clear_messages();
    }
}

static void _extend_move_to_edge(dist &moves)
{
    if (moves.delta.origin())
        return;

    // Now the tricky bit - extend the target x,y out to map edge.
    int mx = 0, my = 0;

    if (moves.delta.x > 0)
        mx = (GXM - 1) - you.pos().x;
    if (moves.delta.x < 0)
        mx = you.pos().x;

    if (moves.delta.y > 0)
        my = (GYM - 1) - you.pos().y;
    if (moves.delta.y < 0)
        my = you.pos().y;

    if (mx != 0 && my != 0)
        mx = my = min(mx, my);

    moves.target.x = you.pos().x + moves.delta.x * mx;
    moves.target.y = you.pos().y + moves.delta.y * my;
}

// Attempts to describe a square that's not in line-of-sight. If
// there's a stash on the square, announces the top item and number
// of items, otherwise, if there's a stair that's in the travel
// cache and noted in the Dungeon (O)verview, names the stair.
static void _describe_oos_square(const coord_def& where)
{
    mprf(MSGCH_EXAMINE_FILTER, "You can't see that place.");

    if (!in_bounds(where) || !env.map_knowledge(where).seen())
    {
#ifdef DEBUG_DIAGNOSTICS
        if (!in_bounds(where))
            dprf("(out of bounds)");
        else
            dprf("(map: %x)", env.map_knowledge(where).flags);
#endif
        return;
    }

    describe_stash(where);
    _describe_oos_feature(where);
#ifdef DEBUG_DIAGNOSTICS
    _debug_describe_feature_at(where);
#endif
}

static bool _mons_is_valid_target(const monster* mon, targ_mode_type mode,
                                  int range)
{
    // Monsters that are no threat to you don't count as monsters.
    if (!mons_is_threatening(*mon) && !mons_class_is_test(mon->type))
        return false;

    // Don't target submerged monsters.
    if (mode != TARG_HOSTILE_SUBMERGED && mon->submerged())
        return false;

    // Don't usually target unseen monsters...
    if (!mon->visible_to(&you))
    {
        // ...unless it creates a "disturbance in the water".
        // Since you can't see the monster, assume it's not a friend.
        return mode != TARG_FRIEND
               && _mon_exposed(mon)
               && i_feel_safe(false, false, true, true, range);
    }

    return true;
}

static bool _want_target_monster(const monster *mon, targ_mode_type mode,
                                 targeter* hitfunc)
{
    if (hitfunc && !hitfunc->affects_monster(monster_info(mon)))
        return false;
    switch (mode)
    {
    case TARG_ANY:
        return true;
    case TARG_HOSTILE:
    case TARG_HOSTILE_SUBMERGED:
        return mons_attitude(*mon) == ATT_HOSTILE;
    case TARG_FRIEND:
        return mon->friendly();
    case TARG_INJURED_FRIEND:
        if (mon->friendly() && mons_get_damage_level(*mon) > MDAM_OKAY)
            return true;
        return !mon->wont_attack() && !mon->neutral()
            && unpacifiable_reason(*mon).empty();
    case TARG_BEOGH_GIFTABLE:
        return beogh_can_gift_items_to(mon);
    case TARG_MOVABLE_OBJECT:
        return false;
    case TARG_MOBILE_MONSTER:
        return !(mons_is_tentacle_or_tentacle_segment(mon->type)
                 || mon->is_stationary());
    case TARG_NUM_MODES:
        break;
    // intentionally no default
    }
    die("Unknown targeting mode!");
}

static bool _tobool(maybe_bool mb)
{
    ASSERT(mb != MB_MAYBE);
    return mb == MB_TRUE;
}

static bool _find_monster(const coord_def& where, targ_mode_type mode,
                          bool need_path, int range, targeter *hitfunc)
{
    {
        coord_def dp = grid2player(where);
        // We could pass more info here.
        maybe_bool x = clua.callmbooleanfn("ch_target_monster", "dd",
                                           dp.x, dp.y);
        if (x != MB_MAYBE)
            return _tobool(x);
    }

    // Target the player for friendly and general spells.
    if ((mode == TARG_FRIEND || mode == TARG_ANY) && where == you.pos())
        return true;

    // Don't target out of range
    if (!_is_target_in_range(where, range, hitfunc))
        return false;

    const monster* mon = monster_at(where);

    // No monster or outside LOS.
    if (!mon || !cell_see_cell(you.pos(), where, LOS_DEFAULT))
        return false;

    // Monster in LOS but only via glass walls, so no direct path.
    if (!you.see_cell_no_trans(where))
        return false;

    if (!_mons_is_valid_target(mon, mode, range))
        return false;

    if (need_path && _blocked_ray(mon->pos()))
        return false;

    return _want_target_monster(mon, mode, hitfunc);
}

static bool _find_shadow_step_mons(const coord_def& where, targ_mode_type mode,
                                   bool need_path, int range,
                                   targeter *hitfunc)
{
    {
        coord_def dp = grid2player(where);
        // We could pass more info here.
        maybe_bool x = clua.callmbooleanfn("ch_target_shadow_step", "dd",
                                           dp.x, dp.y);
        if (x != MB_MAYBE)
            return _tobool(x);
    }

    // Need a monster to attack; this checks that the monster is a valid target.
    if (!_find_monster(where, mode, need_path, range, hitfunc))
        return false;
    // Can't step on yourself
    if (where == you.pos())
        return false;

    targeter_shadow_step &tgt = *static_cast<targeter_shadow_step*>(hitfunc);
    return tgt.has_additional_sites(where);
}

static bool _find_monster_expl(const coord_def& where, targ_mode_type mode,
                               bool need_path, int range, targeter *hitfunc,
                               aff_type mon_aff, aff_type allowed_self_aff)
{
    ASSERT(hitfunc);

    {
        coord_def dp = grid2player(where);
        // We could pass more info here.
        maybe_bool x = clua.callmbooleanfn("ch_target_monster_expl", "dd",
                                           dp.x, dp.y);
        if (x != MB_MAYBE)
            return _tobool(x);
    }

    if (!hitfunc->valid_aim(where))
        return false;

    // Target is blocked by something
    if (need_path && _blocked_ray(where))
        return false;

    if (hitfunc->set_aim(where))
    {
        if (hitfunc->is_affected(you.pos()) > allowed_self_aff)
            return false;
        for (monster_near_iterator mi(&you); mi; ++mi)
        {
            if (hitfunc->is_affected(mi->pos()) == mon_aff
                && _mons_is_valid_target(*mi, mode, range)
                && _want_target_monster(*mi, mode, hitfunc))
            {
                    return true;
            }
        }
    }
    return false;
}

static const item_def* _item_at(const coord_def &where)
{
    // XXX: are we ever interacting with unseen items, anyway?
    return you.see_cell(where)
            ? top_item_at(where)
            : env.map_knowledge(where).item();
}

static bool _find_autopickup_object(const coord_def& where, bool need_path,
                                    int range, targeter *hitfunc)
{
    if (!_find_object(where, need_path, range, hitfunc))
        return false;

    const item_def * const item = _item_at(where);
    ASSERT(item);
    return item_needs_autopickup(*item);
}

static bool _find_object(const coord_def& where, bool need_path, int range,
                         targeter *hitfunc)
{
    // Don't target out of range.
    if (!_is_target_in_range(where, range, hitfunc))
        return false;

    if (need_path && (!you.see_cell(where) || _blocked_ray(where)))
        return false;

    const item_def * const item = _item_at(where);
    return item && !item_is_stationary(*item);
}

static int _next_los(int dir, int los, bool wrap)
{
    if (los == LS_ANY)
        return wrap? los : LS_NONE;

    bool vis    = los & LS_VISIBLE;
    bool hidden = los & LS_HIDDEN;
    bool flipvh = los & LS_FLIPVH;
    bool fliphv = los & LS_FLIPHV;

    if (!vis && !hidden)
        vis = true;

    if (wrap)
    {
        if (!flipvh && !fliphv)
            return los;

        // We have to invert flipvh and fliphv if we're wrapping. Here's
        // why:
        //
        //  * Say the cursor is on the last item in LOS, there are no
        //    items outside LOS, and wrap == true. flipvh is true.
        //  * We set wrap false and flip from visible to hidden, but there
        //    are no hidden items. So now we need to flip back to visible
        //    so we can go back to the first item in LOS. Unless we set
        //    fliphv, we can't flip from hidden to visible.
        //
        los = flipvh ? LS_FLIPHV : LS_FLIPVH;
    }
    else
    {
        if (!flipvh && !fliphv)
            return LS_NONE;

        if (flipvh && vis != (dir == 1))
            return LS_NONE;

        if (fliphv && vis == (dir == 1))
            return LS_NONE;
    }

    return (los & ~LS_VISMASK) | (vis ? LS_HIDDEN : LS_VISIBLE);
}

//---------------------------------------------------------------
//
// find_square
//
// Finds the next monster/object/whatever (moving in a spiral
// outwards from the player, so closer targets are chosen first;
// starts to player's left) and puts its coordinates in mfp.
// Returns 1 if it found something, zero otherwise. If direction
// is -1, goes backwards.
//
//---------------------------------------------------------------
static bool _find_square(coord_def &mfp, int direction,
                         target_checker find_targ, targeter *hitfunc,
                         bool wrap, int los)
{
    int temp_xps = mfp.x;
    int temp_yps = mfp.y;
    int x_change = 0;
    int y_change = 0;

    bool onlyVis = false, onlyHidden = false;

    int i, j;

    if (los == LS_NONE)
        return false;

    if (los == LS_FLIPVH || los == LS_FLIPHV)
    {
        if (in_los_bounds_v(mfp))
        {
            // We've been told to flip between visible/hidden, so we
            // need to find what we're currently on.
            const bool vis = you.see_cell(view2grid(mfp));

            if (wrap && (vis != (los == LS_FLIPVH)) == (direction == 1))
            {
                // We've already flipped over into the other direction,
                // so correct the flip direction if we're wrapping.
                los = (los == LS_FLIPHV ? LS_FLIPVH : LS_FLIPHV);
            }

            los = (los & ~LS_VISMASK) | (vis ? LS_VISIBLE : LS_HIDDEN);
        }
        else
        {
            if (wrap)
                los = LS_HIDDEN | (direction > 0 ? LS_FLIPHV : LS_FLIPVH);
            else
                los |= LS_HIDDEN;
        }
    }

    onlyVis     = (los & LS_VISIBLE);
    onlyHidden  = (los & LS_HIDDEN);

    int radius = 0;
    if (crawl_view.viewsz.x > crawl_view.viewsz.y)
        radius = crawl_view.viewsz.x - LOS_RADIUS - 1;
    else
        radius = crawl_view.viewsz.y - LOS_RADIUS - 1;

    const coord_def vyou = grid2view(you.pos());

    const int minx = vyou.x - radius, maxx = vyou.x + radius,
              miny = vyou.y - radius, maxy = vyou.y + radius,
              ctrx = vyou.x, ctry = vyou.y;

    while (temp_xps >= minx - 1 && temp_xps <= maxx
           && temp_yps <= maxy && temp_yps >= miny - 1)
    {
        if (direction == 1 && temp_xps == minx && temp_yps == maxy)
        {
            mfp = vyou;
            if (find_targ(you.pos()))
                return true;
            return _find_square(mfp, direction,
                                find_targ, hitfunc,
                                false, _next_los(direction, los, wrap));
        }
        if (direction == -1 && temp_xps == ctrx && temp_yps == ctry)
        {
            mfp = coord_def(minx, maxy);
            return _find_square(mfp, direction,
                                find_targ, hitfunc,
                                false, _next_los(direction, los, wrap));
        }

        if (direction == 1)
        {
            if (temp_xps == minx - 1)
            {
                x_change = 0;
                y_change = -1;
            }
            else if (temp_xps == ctrx && temp_yps == ctry)
            {
                x_change = -1;
                y_change = 0;
            }
            else if (abs(temp_xps - ctrx) <= abs(temp_yps - ctry))
            {
                if (temp_xps - ctrx >= 0 && temp_yps - ctry <= 0)
                {
                    if (abs(temp_xps - ctrx) > abs(temp_yps - ctry + 1))
                    {
                        x_change = 0;
                        y_change = -1;
                        if (temp_xps - ctrx > 0)
                            y_change = 1;
                        goto finished_spiralling;
                    }
                }
                x_change = -1;
                if (temp_yps - ctry < 0)
                    x_change = 1;
                y_change = 0;
            }
            else
            {
                x_change = 0;
                y_change = -1;
                if (temp_xps - ctrx > 0)
                    y_change = 1;
            }
        }                       // end if (direction == 1)
        else
        {
            // This part checks all eight surrounding squares to find the
            // one that leads on to the present square.
            for (i = -1; i < 2; ++i)
                for (j = -1; j < 2; ++j)
                {
                    if (i == 0 && j == 0)
                        continue;

                    if (temp_xps + i == minx - 1)
                    {
                        x_change = 0;
                        y_change = -1;
                    }
                    else if (temp_xps + i - ctrx == 0
                             && temp_yps + j - ctry == 0)
                    {
                        x_change = -1;
                        y_change = 0;
                    }
                    else if (abs(temp_xps + i - ctrx) <= abs(temp_yps + j - ctry))
                    {
                        const int xi = temp_xps + i - ctrx;
                        const int yj = temp_yps + j - ctry;

                        if (xi >= 0 && yj <= 0
                            && abs(xi) > abs(yj + 1))
                        {
                            x_change = 0;
                            y_change = -1;
                            if (xi > 0)
                                y_change = 1;
                            goto finished_spiralling;
                        }

                        x_change = -1;
                        if (yj < 0)
                            x_change = 1;
                        y_change = 0;
                    }
                    else
                    {
                        x_change = 0;
                        y_change = -1;
                        if (temp_xps + i - ctrx > 0)
                            y_change = 1;
                    }

                    if (temp_xps + i + x_change == temp_xps
                        && temp_yps + j + y_change == temp_yps)
                    {
                        goto finished_spiralling;
                    }
                }
        }                       // end else

      finished_spiralling:
        x_change *= direction;
        y_change *= direction;

        temp_xps += x_change;
        if (temp_yps + y_change <= maxy)  // it can wrap, unfortunately
            temp_yps += y_change;

        const int targ_x = you.pos().x + temp_xps - ctrx;
        const int targ_y = you.pos().y + temp_yps - ctry;
        const coord_def targ(targ_x, targ_y);

        if (!crawl_view.in_viewport_g(targ))
            continue;

        if (!map_bounds(targ))
            continue;

        if ((onlyVis || onlyHidden) && onlyVis != you.see_cell(targ))
            continue;

        if (find_targ(targ))
        {
            mfp.set(temp_xps, temp_yps);
            return true;
        }
    }

    mfp = (direction > 0 ? coord_def(ctrx, ctry) : coord_def(minx, maxy));
    return _find_square(mfp, direction,
                        find_targ, hitfunc,
                        false, _next_los(direction, los, wrap));
}

// XXX Unbelievably hacky. And to think that my goal was to clean up the code.
// Identical to _find_square, except that mfp is in grid coordinates
// rather than view coordinates.
static bool _find_square_wrapper(coord_def &mfp, int direction,
                                 target_checker find_targ, targeter *hitfunc,
                                 LOSSelect los)
{
    mfp = grid2view(mfp);
    const bool r =  _find_square(mfp, direction, find_targ, hitfunc, true, los);
    mfp = view2grid(mfp);
    return r;
}

static void _describe_oos_feature(const coord_def& where)
{
    if (!env.map_knowledge(where).seen())
        return;

    string desc = feature_description(env.map_knowledge(where).feat());

    if (!desc.empty())
        mprf(MSGCH_EXAMINE_FILTER, "[%s.]", desc.c_str());
}

// Returns a vector of features matching the given pattern.
vector<dungeon_feature_type> features_by_desc(const base_pattern &pattern)
{
    vector<dungeon_feature_type> features;

    if (pattern.valid())
    {
        for (int i = 0; i < NUM_FEATURES; ++i)
        {
            string fdesc =
                feature_description(static_cast<dungeon_feature_type>(i)) + ".";

            if (pattern.matches(fdesc))
                features.push_back(dungeon_feature_type(i));
        }
    }
    return features;
}

void describe_floor()
{
    dungeon_feature_type grid = env.map_knowledge(you.pos()).feat();

    string msg = "There is %s here.";
    string feat;

    switch (grid)
    {
    case DNGN_FLOOR:
        return;

    case DNGN_ENTER_SHOP:
        msg = "There is an entrance to %s here.";
        break;

    default:
        break;
    }

    feat = feature_description_at(you.pos(), true, DESC_A);
    if (feat.empty())
        return;

    msg_channel_type channel = MSGCH_EXAMINE;

    // Messages for water/lava are too spammy use a status light instead.
    if (feat_is_water(grid) || feat_is_lava(grid))
        return;

    mprf(channel, msg.c_str(), feat.c_str());
    if (grid == DNGN_ENTER_GAUNTLET)
        mprf(MSGCH_EXAMINE, "Beware, the minotaur awaits!");
}

static string _base_feature_desc(dungeon_feature_type grid, trap_type trap)
{
    if (feat_is_trap(grid) && trap != NUM_TRAPS)
        return full_trap_name(trap);

    if (grid == DNGN_ROCK_WALL && player_in_branch(BRANCH_PANDEMONIUM))
        return "wall of the weird stuff which makes up Pandemonium";
    else if (!is_valid_feature_type(grid))
        return "";
    else
        return get_feature_def(grid).name;

}

string feature_description(dungeon_feature_type grid, trap_type trap,
                           const string & cover_desc,
                           description_level_type dtype)
{
    string desc = _base_feature_desc(grid, trap);
    desc += cover_desc;

    if (grid == DNGN_FLOOR && dtype == DESC_A)
        dtype = DESC_THE;

    bool ignore_case = false;
    if (grid == DNGN_TRAP_ZOT)
        ignore_case = true;

    return thing_do_grammar(dtype, desc, ignore_case);
}

string raw_feature_description(const coord_def &where)
{
    dungeon_feature_type feat = env.grid(where);

    int mapi = env.level_map_ids(where);
    if (mapi != INVALID_MAP_INDEX)
    {
        const auto &renames = env.level_vaults[mapi]->map.feat_renames;
        if (const string *rename = map_find(renames, feat))
            return *rename;
    }

    return _base_feature_desc(feat, get_trap_type(where));
}

#ifndef DEBUG_DIAGNOSTICS
// Is a feature interesting enough to 'v'iew it, even if a player normally
// doesn't care about descriptions, i.e. does the description hold important
// information? (Yes, this is entirely subjective. --jpeg)
static bool _interesting_feature(dungeon_feature_type feat)
{
    return get_feature_def(feat).flags & FFT_EXAMINE_HINT;
}
#endif

string feature_description_at(const coord_def& where, bool covering,
                              description_level_type dtype)
{
    dungeon_feature_type grid = env.map_knowledge(where).feat();
    trap_type trap = env.map_knowledge(where).trap();

    string marker_desc = env.markers.property_at(where, MAT_ANY,
                                                 "feature_description");

    string covering_description = "";

    if (covering && you.see_cell(where))
    {
        if (feat_is_tree(grid) && env.forest_awoken_until)
            covering_description += ", awoken";

        if (is_icecovered(where))
            covering_description = ", covered with ice";

        if (is_temp_terrain(where))
            covering_description = ", summoned";

        if (is_bloodcovered(where))
            covering_description += ", spattered with blood";
    }

    // FIXME: remove desc markers completely; only Zin walls are left.
    // They suffer, among other problems, from an information leak.
    if (!marker_desc.empty())
    {
        marker_desc += covering_description;

        return thing_do_grammar(dtype, marker_desc);
    }

    if (feat_is_door(grid))
    {
        const string door_desc_prefix =
            env.markers.property_at(where, MAT_ANY,
                                    "door_description_prefix");
        const string door_desc_suffix =
            env.markers.property_at(where, MAT_ANY,
                                    "door_description_suffix");
        const string door_desc_noun =
            env.markers.property_at(where, MAT_ANY,
                                    "door_description_noun");
        const string door_desc_adj  =
            env.markers.property_at(where, MAT_ANY,
                                    "door_description_adjective");
        const string door_desc_veto =
            env.markers.property_at(where, MAT_ANY,
                                    "door_description_veto");

        set<coord_def> all_door;
        find_connected_identical(where, all_door);
        const char *adj, *noun;
        get_door_description(all_door.size(), &adj, &noun);

        string desc;
        if (!door_desc_adj.empty())
            desc += door_desc_adj;
        else
            desc += adj;

        // @noloc section start (adjectives handled as single words)
        if (door_desc_veto.empty() || door_desc_veto != "veto")
        {
            if (grid == DNGN_OPEN_DOOR)
                desc += "open ";
            else if (grid == DNGN_CLOSED_CLEAR_DOOR)
                desc += "closed translucent ";
            else if (grid == DNGN_OPEN_CLEAR_DOOR)
                desc += "open translucent ";
            else if (grid == DNGN_RUNED_DOOR)
                desc += "runed ";
            else if (grid == DNGN_RUNED_CLEAR_DOOR)
                desc += "runed translucent ";
            else if (grid == DNGN_SEALED_DOOR)
                desc += "sealed ";
            else if (grid == DNGN_SEALED_CLEAR_DOOR)
                desc += "sealed translucent ";
            else
                desc += "closed ";
        }
        // @noloc section end

        desc += door_desc_prefix;

        if (!door_desc_noun.empty())
            desc += door_desc_noun;
        else
            desc += noun;

        desc += door_desc_suffix;

        desc += covering_description;

        return thing_do_grammar(dtype, desc);
    }

    bool ignore_case = false;
    if (grid == DNGN_TRAP_ZOT)
        ignore_case = true;

    switch (grid)
    {
#if TAG_MAJOR_VERSION == 34
    case DNGN_TRAP_MECHANICAL:
        return feature_description(grid, trap, covering_description, dtype);

    case DNGN_ENTER_PORTAL_VAULT:
        // Should have been handled at the top of the function.
        return thing_do_grammar(dtype, "UNAMED PORTAL VAULT ENTRY");
#endif
    case DNGN_ENTER_SHOP:
        return shop_name(*shop_at(where));

    case DNGN_FLOOR:
        if (dtype == DESC_A)
            dtype = DESC_THE;
        // fallthrough
    default:
        const string featdesc = grid == env.grid(where)
                              ? raw_feature_description(where)
                              : _base_feature_desc(grid, trap);
        return thing_do_grammar(dtype, featdesc + covering_description,
                ignore_case);
    }
}

static string _describe_monster_weapon(const monster_info& mi, bool ident)
{
    string desc = "";
    string name1, name2;
    const item_def *weap = mi.inv[MSLOT_WEAPON].get();
    const item_def *alt  = mi.inv[MSLOT_ALT_WEAPON].get();

    if (weap && (!ident || item_type_known(*weap)))
        name1 = weap->name(DESC_A, false, false, true, false);
    if (alt && (!ident || item_type_known(*alt)) && mi.wields_two_weapons())
        name2 = alt->name(DESC_A, false, false, true, false);

    if (name1.empty() && !name2.empty())
        name1.swap(name2);

    if (name1 == name2 && weap && !name1.empty())
    {
        item_def dup = *weap;
        ++dup.quantity;
        name1 = dup.name(DESC_A, false, false, true, true);
        name2.clear();
    }

    if (mi.props.exists(SPECIAL_WEAPON_KEY))
    {
        name1 = article_a(ghost_brand_name(
            (brand_type) mi.props[SPECIAL_WEAPON_KEY].get_int(), mi.type));
    }

    if (name1.empty())
        return desc;

    string names = name1;
    if (!name2.empty())
        names += " and " + name2;

    if (mi.type == MONS_PANDEMONIUM_LORD)
        desc = localise("armed with %s", names);
    else if (mons_class_is_animated_weapon(mi.type))
        desc = localise(names);
    else
        desc = localise("wielding %s", names);


    return desc;
}

#ifdef DEBUG_DIAGNOSTICS
static string _stair_destination_description(const coord_def &pos)
{
    if (LevelInfo *linf = travel_cache.find_level_info(level_id::current()))
    {
        const stair_info *si = linf->get_stair(pos);
        if (si)
            return " " + si->describe();
        else if (feat_is_stair(env.grid(pos)))
            return " (unknown stair)";
    }
    return "";
}
#endif

static string _mon_enchantments_string(const monster_info& mi)
{
    const vector<string> enchant_descriptors = mi.attributes();

    if (!enchant_descriptors.empty())
    {
        return localise("This monster is %s.",
                        comma_separated_line(enchant_descriptors.begin(),
                                             enchant_descriptors.end()));
    }
    else
        return "";
}

static vector<string> _get_monster_behaviour_vector(const monster_info& mi)
{
    vector<string> descs;

    if ((mi.is(MB_SLEEPING) || mi.is(MB_DORMANT)))
    {
        if (mi.is(MB_CONFUSED))
            descs.emplace_back(localise("sleepwalking"));
        else if (mons_class_flag(mi.type, M_CONFUSED))
            descs.emplace_back(localise("drifting"));
    }
    else if (mi.attitude == ATT_HOSTILE && (mi.is(MB_UNAWARE) || mi.is(MB_WANDERING)))
        descs.emplace_back(localise("hasn't noticed you"));

    return descs;
}

// FIXME: this duplicates _get_monster_desc(). Unite them.
static vector<string> _get_monster_desc_vector(const monster_info& mi)
{
    vector<string> descs;

    _append_container(descs, _get_monster_behaviour_vector(mi));

    if (you.duration[DUR_CONFUSING_TOUCH])
    {
        const int pow = you.props["confusing touch power"].get_int();
        descs.emplace_back(localise("chance to confuse on hit: %d%%",
                                    hex_success_chance(mi.willpower(),
                                                           pow, 100)));
    }
    else if (you.form == transformation::fungus
             && !mons_is_unbreathing(mi.type))
    {
        descs.emplace_back(localise("chance to confuse on hit: %d%%",
                                    melee_confuse_chance(mi.hd)));
    }

    if (mi.attitude == ATT_FRIENDLY)
        descs.emplace_back(localise("friendly"));
    else if (mi.attitude == ATT_GOOD_NEUTRAL)
        descs.emplace_back(localise("peaceful"));
    else if (mi.attitude != ATT_HOSTILE && !mi.is(MB_INSANE))
    {
        // don't differentiate between permanent or not
        descs.emplace_back(localise("indifferent"));
    }

    if (mi.is(MB_HALOED))
        descs.emplace_back(localise("haloed"));

    if (mi.is(MB_UMBRAED))
        descs.emplace_back(localise("umbra"));

    if (mi.fire_blocker)
    {
        descs.push_back(localise("fire blocked by %s", // FIXME, renamed features
                                 feature_description(mi.fire_blocker, NUM_TRAPS,
                                                     "", DESC_A)));
    }

    return descs;
}

// Returns the description string for a given monster, including attitude
// and enchantments but not equipment or wounds.
static string _get_monster_desc(const monster_info& mi)
{
    string text    = "";
    string pronoun = uppercase_first(mi.pronoun(PRONOUN_SUBJECTIVE));

    if (mi.is(MB_MESMERIZING))
        text += localise("You are mesmerised by this monster's song.") + "\n";

    if (mi.is(MB_SLEEPING) || mi.is(MB_DORMANT))
    {
        if (mi.is(MB_CONFUSED))
            text += localise("This monster appears to be sleepwalking.");
        else
            text += localise("This monster appears to be resting.");
        text += "\n";
    }
    // Applies to both friendlies and hostiles
    else if (mi.is(MB_FLEEING))
        text += localise("This monster is fleeing.") + "\n";
    // hostile with target != you
    else if (mi.attitude == ATT_HOSTILE
             && (mi.is(MB_UNAWARE) || mi.is(MB_WANDERING)))
    {
        text += localise("This monster has not noticed you.") + "\n";
    }

    if (mi.attitude == ATT_FRIENDLY)
        text += localise("This monster is friendly.") + "\n";
    else if (mi.attitude == ATT_GOOD_NEUTRAL)
        text += localise("This monster seems to be peaceful towards you.") + "\n";
    else if (mi.attitude != ATT_HOSTILE && !mi.is(MB_INSANE))
    {
        // don't differentiate between permanent or not
        text += localise("This monster is indifferent to you.") + "\n";
    }

    if (mi.is(MB_SUMMONED) || mi.is(MB_PERM_SUMMON))
    {
        if (mi.is(MB_SUMMONED_CAPPED))
            text += localise("This monster has been summoned and is expiring.");
        else if (mi.is(MB_PERM_SUMMON))
            text += localise("This monster has been summoned but will not time out.");
        else
            text += localise("This monster has been summoned.");

        text += "\n";
    }

    if (mi.is(MB_HALOED))
        text += localise("This monster is illuminated by a divine halo.") + "\n";

    if (mi.is(MB_UMBRAED))
        text += localise("This monster is wreathed by an umbra.") + "\n";

    if (mi.intel() <= I_BRAINLESS)
        text += localise("This monster is mindless.") + "\n";

    if (mi.is(MB_CHAOTIC))
        text += localise("This monster is chaotic.") + "\n";

    if (mi.is(MB_POSSESSABLE))
        text += localise("This monster's soul is ripe for the taking.") + "\n";
    else if (mi.is(MB_ENSLAVED))
        text += localise("This monster is a disembodied soul.") + "\n";

    if (mi.is(MB_MIRROR_DAMAGE))
    {
        text += localise("This monster is reflecting injuries back at attackers.");
        text += "\n";
    }

    if (mi.is(MB_INNER_FLAME))
        text += localise("This monster is filled with an inner flame.") + "\n";

    if (mi.fire_blocker)
    {
        // FIXME: renamed features
        text += localise("Your line of fire to this monster is blocked by %s.",
                         feature_description(mi.fire_blocker, NUM_TRAPS, "",
                                             DESC_A));
        text += "\n";
    }

    text += _mon_enchantments_string(mi);
    if (!text.empty() && text.back() == '\n')
        text.pop_back();
    return text;
}

static void _describe_monster(const monster_info& mi)
{
    // First print type and equipment.
    string text = uppercase_first(get_monster_equipment_desc(mi));
    text = add_punctuation(text, ".", false);
    const string wounds_desc = mi.wounds_description_sentence();
    if (!wounds_desc.empty())
        text += " " + uppercase_first(wounds_desc);
    const string constriction_desc = mi.constriction_description();
    if (!constriction_desc.empty())
        text += localise(" ") + uppercase_first(constriction_desc);
    mpr_nolocalise(MSGCH_EXAMINE, text);

    // Print the rest of the description.
    text = _get_monster_desc(mi);
    if (!text.empty())
        mprf(MSGCH_EXAMINE, "%s", text.c_str());
}

// This method is called in two cases:
// a) Monsters coming into view: "An ogre comes into view. It is wielding ..."
// b) Monster description via 'x': "An ogre, wielding a club, and wearing ..."
string get_monster_equipment_desc(const monster_info& mi,
                                  mons_equip_desc_level_type level,
                                  description_level_type mondtype,
                                  bool print_attitude)
{
    string desc = "";
    if (mondtype != DESC_NONE)
    {
        if (print_attitude && mons_is_pghost(mi.type))
            desc = get_ghost_description(mi);
        else
            desc = mi.full_name(mondtype);
        desc = localise(desc);

        if (print_attitude)
        {
            vector<string> attributes;
            if (mi.attitude == ATT_FRIENDLY)
                attributes.emplace_back("friendly");
            else if (mi.attitude == ATT_GOOD_NEUTRAL)
                attributes.emplace_back("peaceful");
            else if (mi.attitude != ATT_HOSTILE && !mi.is(MB_INSANE))
                attributes.emplace_back("neutral");
            _append_container(attributes, mi.attributes());

            string str = comma_separated_line(attributes.begin(),
                                              attributes.end());

            if (mons_class_is_animated_weapon(mi.type)
                || mi.type == MONS_PANDEMONIUM_LORD
                || mi.type == MONS_PLAYER_GHOST)
            {
                if (!str.empty())
                    str += " ";

                // animated armour is has "animated" in its name already,
                // spectral weapons have "spectral".
                if (mi.type == MONS_DANCING_WEAPON)
                    str += "dancing weapon";
                else if (mi.type == MONS_PANDEMONIUM_LORD)
                    str += "pandemonium lord";
                else if (mi.type == MONS_PLAYER_GHOST)
                    str += "ghost";
            }
            if (!str.empty())
                desc += localise(" (") + localise(str) + localise(")");
        }
    }

    string weap = _describe_monster_weapon(mi, level == DESC_IDENTIFIED);

    // Print the rest of the equipment only for full descriptions.
    if (level == DESC_WEAPON || level == DESC_WEAPON_WARNING)
    {
        if (desc.empty() || weap.empty())
            return desc + weap;
        else
            return desc + " " + weap;
    }

    item_def* mon_arm = mi.inv[MSLOT_ARMOUR].get();
    item_def* mon_shd = mi.inv[MSLOT_SHIELD].get();
    item_def* mon_qvr = mi.inv[MSLOT_MISSILE].get();
    item_def* mon_alt = mi.inv[MSLOT_ALT_WEAPON].get();
    item_def* mon_wnd = mi.inv[MSLOT_WAND].get();
    item_def* mon_rng = mi.inv[MSLOT_JEWELLERY].get();

#define uninteresting(x) (x && !item_is_branded(*x) && !is_artefact(*x))
    // For "comes into view" msgs, only care about branded stuff and artefacts
    if (level == DESC_IDENTIFIED)
    {
        if (uninteresting(mon_arm))
            mon_arm = nullptr;
        if (uninteresting(mon_shd))
            mon_shd = nullptr;
        if (uninteresting(mon_qvr))
            mon_qvr = nullptr;
        if (uninteresting(mon_rng))
            mon_rng = nullptr;
        if (uninteresting(mon_alt) && mon_alt->base_type != OBJ_WANDS)
            mon_alt = nullptr;
    }
#undef uninteresting

    // _describe_monster_weapon already took care of this
    if (mi.wields_two_weapons())
        mon_alt = 0;

    const bool mon_has_wand = mon_wnd;
    const bool mon_carry = mon_alt || mon_has_wand;

    vector<string> item_descriptions;

    // Dancing weapons have all their weapon information in their full_name, so
    // we don't need to add another weapon description here (see Mantis 11887).
    if (!weap.empty() && !mons_class_is_animated_weapon(mi.type))
        item_descriptions.push_back(weap);

    // as with dancing weapons, don't claim animated armours 'wear' their armour
    if (mon_arm && mi.type != MONS_ANIMATED_ARMOUR)
    {
        const string armour_desc = localise("wearing %s",
                                            mon_arm->name(DESC_A).c_str());
        item_descriptions.push_back(armour_desc);
    }

    if (mon_shd)
    {
        const string shield_desc = localise("wearing %s",
                                            mon_shd->name(DESC_A).c_str());
        item_descriptions.push_back(shield_desc);
    }

    if (mon_rng)
    {
        const string rng_desc = localise("wearing %s",
                                          mon_rng->name(DESC_A).c_str());
        item_descriptions.push_back(rng_desc);
    }

    if (mon_qvr)
    {
        const bool net = mon_qvr->sub_type == MI_THROWING_NET;
        const string qvr_desc = net ? mon_qvr->name(DESC_A)
                                    : pluralise(mon_qvr->name(DESC_PLAIN));
        item_descriptions.push_back(localise("quivering %s", qvr_desc));
    }

    if (mon_carry)
    {
        string carried_desc;

        if (mon_alt && mon_has_wand)
        {
            carried_desc = localise("carrying %s and %s", mon_alt->name(DESC_A),
                                    mon_wnd->name(DESC_A));
        }
        else if (mon_alt)
            carried_desc = localise("carrying %s", mon_alt->name(DESC_A));
        else if (mon_has_wand)
            carried_desc = localise("carrying %s", mon_wnd->name(DESC_A));

        item_descriptions.push_back(carried_desc);
    }

    const string item_description = comma_separated_line(
                                                item_descriptions.begin(),
                                                item_descriptions.end(),
                                                localise(" and "),
                                                localise(", "));

    if (!item_description.empty() && !desc.empty())
        desc += localise(", ");
    return desc + item_description;
}

static bool _print_cloud_desc(const coord_def where)
{
    vector<string> areas;
    if (is_sanctuary(where))
        areas.emplace_back("This square lies inside a sanctuary.");
    if (silenced(where))
        areas.emplace_back("This square is shrouded in silence.");
    if (haloed(where) && !umbraed(where))
        areas.emplace_back("This square is lit by a halo.");
    if (umbraed(where) && !haloed(where))
        areas.emplace_back("This square is wreathed by an umbra.");
    if (liquefied(where))
        areas.emplace_back("This square is liquefied.");
    if (orb_haloed(where) || quad_haloed(where))
        areas.emplace_back("This square is covered in a magical glow.");
    if (disjunction_haloed(where))
        areas.emplace_back("This square is bathed in translocational energy.");

    for (string area: areas)
        mpr(area);

    if (cloud_struct* cloud = cloud_at(where))
    {
        mprf(MSGCH_EXAMINE, "There is a cloud of %s here.",
             cloud->cloud_name(true).c_str());
        return true;
    }

    return false;
}

static bool _print_item_desc(const coord_def where)
{
    int targ_item = you.visible_igrd(where);

    if (targ_item == NON_ITEM)
        return false;

    string name = menu_colour_item_name(env.item[targ_item], DESC_A);
    mprf(MSGCH_FLOOR_ITEMS, "You see %s here.", name.c_str());

    if (env.item[ targ_item ].link != NON_ITEM)
        mprf(MSGCH_FLOOR_ITEMS, "There is something else lying underneath.");

    return true;
}

#ifdef DEBUG_DIAGNOSTICS
static void _debug_describe_feature_at(const coord_def &where)
{
    const string feature_desc = feature_description_at(where, true);
    string marker;
    if (map_marker *mark = env.markers.find(where, MAT_ANY))
    {
        string desc = mark->debug_describe();
        if (desc.empty())
            desc = "?";
        marker = " (" + desc + ")";
    }
    const string traveldest = _stair_destination_description(where);
    string height_desc;
    if (env.heightmap)
        height_desc = make_stringf(" (height: %d)", (*env.heightmap)(where));
    const dungeon_feature_type feat = env.grid(where);

    string vault;
    const int map_index = env.level_map_ids(where);
    if (map_index != INVALID_MAP_INDEX)
    {
        const vault_placement &vp(*env.level_vaults[map_index]);
        const coord_def br = vp.pos + vp.size - 1;
        vault = make_stringf(" [Vault: %s (%d,%d)-(%d,%d) (%dx%d)]",
                             vp.map_name_at(where).c_str(),
                             vp.pos.x, vp.pos.y,
                             br.x, br.y,
                             vp.size.x, vp.size.y);
    }

    char32_t ch = get_cell_glyph(where).ch;
    dprf("(%d,%d): %s - %s. (%d/%s)%s%s%s%s map: %x",
         where.x, where.y,
         ch == '<' ? "<<" : stringize_glyph(ch).c_str(),
         feature_desc.c_str(),
         feat,
         dungeon_feature_name(feat),
         marker.c_str(),
         traveldest.c_str(),
         height_desc.c_str(),
         vault.c_str(),
         env.map_knowledge(where).flags);
}
#endif

// Describe a cell, guaranteed to be in view.
static void _describe_cell(const coord_def& where, bool in_range)
{
#ifndef DEBUG_DIAGNOSTICS
    bool monster_described = false;
#endif

    if (where == you.pos() && !crawl_state.arena_suspended)
        mprf(MSGCH_EXAMINE_FILTER, "You.");

    if (const monster* mon = monster_at(where))
    {
#ifdef DEBUG_DIAGNOSTICS
        if (!mon->visible_to(&you))
        {
            mprf(MSGCH_DIAGNOSTICS, "There is a non-visible %smonster here.",
                 _mon_exposed_in_water(mon) ? "exposed by water " :
                 _mon_exposed_in_cloud(mon) ? "exposed by cloud " : "");
        }
#else
        if (!mon->visible_to(&you))
        {
            if (_mon_exposed_in_water(mon))
                mprf(MSGCH_EXAMINE_FILTER, "There is a strange disturbance in the water here.");
            else if (_mon_exposed_in_cloud(mon))
                mprf(MSGCH_EXAMINE_FILTER, "There is a strange disturbance in the cloud here.");

            goto look_clouds;
        }
#endif

        monster_info mi(mon);
        _describe_monster(mi);

        if (!in_range)
            mpr(MSGCH_EXAMINE_FILTER, "This monster is out of range.");
#ifndef DEBUG_DIAGNOSTICS
        monster_described = true;
#endif

#if defined(DEBUG_DIAGNOSTICS) && defined(WIZARD)
        debug_stethoscope(env.mgrid(where));
#endif
        if (crawl_state.game_is_hints() && hints_monster_interesting(mon))
        {
            const char *msg;
#ifdef USE_TILE_LOCAL
            msg = "(<w>Right-click</w> for more information.)";
#else
            msg = "(Press <w>v</w> for more information.)";
#endif
            mpr(msg);
        }
    }

#ifdef DEBUG_DIAGNOSTICS
    _print_cloud_desc(where);
    _print_item_desc(where);
    _debug_describe_feature_at(where);
#else
  // removing warning
  look_clouds:

    bool cloud_described = _print_cloud_desc(where);
    bool item_described = _print_item_desc(where);

    string feature_desc = localise(feature_description_at(where, true));
    const bool bloody = is_bloodcovered(where);
    if (crawl_state.game_is_hints() && hints_pos_interesting(where.x, where.y))
    {
#ifdef USE_TILE_LOCAL
        feature_desc += localise(" (<w>Right-click</w> for more information.)");
#else
        feature_desc += localise(" (Press <w>v</w> for more information.)");
#endif
        mpr_nolocalise(feature_desc);
    }
    else
    {
        dungeon_feature_type feat = env.grid(where);

        if (_interesting_feature(feat))
        {
#ifdef USE_TILE_LOCAL
            feature_desc += localise(" (Right-click for more information.)");
#else
            feature_desc += localise(" (Press 'v' for more information.)");
#endif
        }

        // Suppress "Floor." if there's something on that square that we've
        // already described.
        if (feat == DNGN_FLOOR && !bloody
            && (monster_described || item_described || cloud_described))
        {
            return;
        }

        msg_channel_type channel = MSGCH_EXAMINE;
        if (feat == DNGN_FLOOR || feat_is_water(feat))
            channel = MSGCH_EXAMINE_FILTER;

        mpr_nolocalise(channel, feature_desc);
    }
#endif
}

///////////////////////////////////////////////////////////////////////////
// targeting_behaviour

targeting_behaviour::targeting_behaviour(bool look_around)
    : just_looking(look_around), needs_path(MB_MAYBE)
{
}

targeting_behaviour::~targeting_behaviour()
{
}

command_type targeting_behaviour::get_command(int key)
{
    command_type cmd = key_to_command(key, KMC_TARGETING);
    if (cmd >= CMD_MIN_TARGET && cmd < CMD_TARGET_PREV_TARGET)
        return cmd;

    // XXX: hack
    if (cmd == CMD_TARGET_SELECT && key == ' ' && just_looking)
        cmd = CMD_TARGET_CANCEL;

    return cmd;
}

vector<string> targeting_behaviour::get_monster_desc(const monster_info& mi)
{
    vector<string> descs;
    if (get_desc_func)
        _append_container(descs, (get_desc_func)(mi));
    return descs;
}
