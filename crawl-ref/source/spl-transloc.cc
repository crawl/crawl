/*
 *  File:     spl-transloc.cc
 *  Summary:  Translocation spells.
 */

#include "AppHdr.h"

#include "spl-transloc.h"
#include "externs.h"

#include "abyss.h"
#include "branch.h"
#include "cloud.h"
#include "coord.h"
#include "coordit.h"
#include "delay.h"
#include "directn.h"
#include "dungeon.h"
#include "env.h"
#include "fprop.h"
#include "invent.h"
#include "item_use.h"
#include "itemprop.h"
#include "items.h"
#include "message.h"
#include "misc.h"
#include "mon-behv.h"
#include "mon-iter.h"
#include "mon-util.h"
#include "player.h"
#include "random.h"
#include "spl-other.h"
#include "spl-util.h"
#include "stairs.h"
#include "stash.h"
#include "state.h"
#include "stuff.h"
#include "teleport.h"
#include "terrain.h"
#include "transform.h"
#include "traps.h"
#include "travel.h"
#include "view.h"
#include "viewmap.h"
#include "xom.h"

static bool _abyss_blocks_teleport(bool cblink)
{
    // Lugonu worshippers get their perks.
    if (you.religion == GOD_LUGONU)
        return (false);

    // Controlled Blink (the spell) works quite reliably in the Abyss.
    return (cblink ? one_chance_in(3) : !one_chance_in(3));
}

// If wizard_blink is set, all restriction are ignored (except for
// a monster being at the target spot), and the player gains no
// contamination.
int blink(int pow, bool high_level_controlled_blink, bool wizard_blink)
{
    ASSERT(!crawl_state.game_is_arena());

    dist beam;

    if (crawl_state.is_repeating_cmd())
    {
        crawl_state.cant_cmd_repeat("You can't repeat controlled blinks.");
        crawl_state.cancel_cmd_again();
        crawl_state.cancel_cmd_repeat();
        return (1);
    }

    // yes, there is a logic to this ordering {dlb}:
    if (item_blocks_teleport(true, true) && !wizard_blink)
        canned_msg(MSG_STRANGE_STASIS);
    else if (you.level_type == LEVEL_ABYSS
             && _abyss_blocks_teleport(high_level_controlled_blink)
             && !wizard_blink)
    {
        mpr("The power of the Abyss keeps you in your place!");
    }
    else if (you.confused() && !wizard_blink)
        random_blink(false);
    else if (!allow_control_teleport(true) && !wizard_blink)
    {
        mpr("A powerful magic interferes with your control of the blink.");
        if (high_level_controlled_blink)
            return (cast_semi_controlled_blink(pow));
        random_blink(false);
    }
    else
    {
        // query for location {dlb}:
        while (!crawl_state.seen_hups)
        {
            direction_chooser_args args;
            args.restricts = DIR_TARGET;
            args.needs_path = false;
            args.may_target_monster = false;
            args.top_prompt = "Blink to where?";
            direction(beam, args);

            if (!beam.isValid || beam.target == you.pos())
            {
                if (!wizard_blink
                    && !yesno("Are you sure you want to cancel this blink?",
                              false, 'n'))
                {
                    mesclr();
                    continue;
                }
                canned_msg(MSG_OK);
                return (-1);         // early return {dlb}
            }

            monster* beholder = you.get_beholder(beam.target);
            if (!wizard_blink && beholder)
            {
                mprf("You cannot blink away from %s!",
                    beholder->name(DESC_NOCAP_THE, true).c_str());
                continue;
            }

            monster* fearmonger = you.get_fearmonger(beam.target);
            if (!wizard_blink && fearmonger)
            {
                mprf("You cannot blink closer to %s!",
                    fearmonger->name(DESC_NOCAP_THE, true).c_str());
                continue;
            }

            if (grd(beam.target) == DNGN_OPEN_SEA)
            {
                mesclr();
                mpr("You can't blink into the sea!");
            }
            else if (!check_moveto(beam.target, "blink"))
            {
                // try again (messages handled by check_moveto)
            }
            else if (you.see_cell_no_trans(beam.target))
            {
                // Grid in los, no problem.
                break;
            }
            else if (you.trans_wall_blocking(beam.target))
            {
                // Wizard blink can move past translucent walls.
                if (wizard_blink)
                    break;

                mesclr();
                mpr("You can't blink through translucent walls.");
            }
            else
            {
                mesclr();
                mpr("You can only blink to visible locations.");
            }
        }

        // Allow wizard blink to send player into walls, in case the
        // user wants to alter that grid to something else.
        if (wizard_blink && feat_is_solid(grd(beam.target)))
            grd(beam.target) = DNGN_FLOOR;

        if (feat_is_solid(grd(beam.target)) || monster_at(beam.target))
        {
            mpr("Oops! Maybe something was there already.");
            random_blink(false);
        }
        else if (you.level_type == LEVEL_ABYSS && !wizard_blink)
        {
            abyss_teleport(false);
            if (you.pet_target != MHITYOU)
                you.pet_target = MHITNOT;
        }
        else
        {
            // Leave a purple cloud.
            place_cloud(CLOUD_TLOC_ENERGY, you.pos(), 1 + random2(3), KC_YOU);
            move_player_to_grid(beam.target, false, true);

            // Controlling teleport contaminates the player. -- bwr
            if (!wizard_blink)
                contaminate_player(1, true);


            if (!wizard_blink && you.duration[DUR_CONDENSATION_SHIELD] > 0)
                remove_condensation_shield();
        }
    }

    crawl_state.cancel_cmd_again();
    crawl_state.cancel_cmd_repeat();

    return (1);
}

void random_blink(bool allow_partial_control, bool override_abyss)
{
    ASSERT(!crawl_state.game_is_arena());

    bool success = false;
    coord_def target;

    if (item_blocks_teleport(true, true))
        canned_msg(MSG_STRANGE_STASIS);
    else if (you.level_type == LEVEL_ABYSS
             && !override_abyss && !one_chance_in(3))
    {
        mpr("The power of the Abyss keeps you in your place!");
    }
    // First try to find a random square not adjacent to the player,
    // then one adjacent if that fails.
    else if (!random_near_space(you.pos(), target)
             && !random_near_space(you.pos(), target, true))
    {
        mpr("You feel jittery for a moment.");
    }

#ifdef USE_SEMI_CONTROLLED_BLINK
    //jmf: Add back control, but effect is cast_semi_controlled_blink(pow).
    else if (player_control_teleport() && !you.confused()
             && allow_partial_control && allow_control_teleport())
    {
        mpr("You may select the general direction of your translocation.");
        cast_semi_controlled_blink(100);
        maybe_id_ring_TC();
        success = true;
    }
#endif
    else
    {
        canned_msg(MSG_YOU_BLINK);
        coord_def origin = you.pos();
        move_player_to_grid(target, false, true);
        success = true;

        // Leave a purple cloud.
        place_cloud(CLOUD_TLOC_ENERGY, origin, 1 + random2(3), KC_YOU);

        if (you.level_type == LEVEL_ABYSS)
        {
            abyss_teleport(false);
            if (you.pet_target != MHITYOU)
                you.pet_target = MHITNOT;
        }
    }

    if (success && you.duration[DUR_CONDENSATION_SHIELD] > 0)
        remove_condensation_shield();
}

// This function returns true if the player can use controlled teleport
// here.
bool allow_control_teleport(bool quiet)
{
    bool retval = !(testbits(env.level_flags, LFLAG_NO_TELE_CONTROL)
                    || testbits(get_branch_flags(), BFLAG_NO_TELE_CONTROL));

    // Tell the player why if they have teleport control.
    if (!quiet && !retval && player_control_teleport())
        mpr("A powerful magic prevents control of your teleportation.");

    return (retval);
}

void you_teleport(void)
{
    // [Cha] here we block teleportation, which will save the player from
    // death from read-id'ing scrolls (in sprint)
    if (crawl_state.game_is_sprint() || item_blocks_teleport(true, true))
        canned_msg(MSG_STRANGE_STASIS);
    else if (you.duration[DUR_TELEPORT])
    {
        mpr("You feel strangely stable.");
        you.duration[DUR_TELEPORT] = 0;
    }
    else
    {
        mpr("You feel strangely unstable.");

        int teleport_delay = 3 + random2(3);

        if (you.level_type == LEVEL_ABYSS && !one_chance_in(5))
        {
            mpr("You have a feeling this translocation may take a while to kick in...");
            teleport_delay += 5 + random2(10);
        }

        you.set_duration(DUR_TELEPORT, teleport_delay);
    }
}

// Should return true if we don't want anyone to teleport here.
static bool _cell_vetoes_teleport (const coord_def cell, bool  check_monsters = true)
{
    // Monsters always veto teleport.
    if (monster_at(cell) && check_monsters)
        return (true);

    // As do all clouds; this may change.
    if (env.cgrid(cell) != EMPTY_CLOUD)
        return (true);

    // But not all features.
    switch (grd(cell))
    {
    case DNGN_FLOOR:
    case DNGN_SHALLOW_WATER:
        return (false);

    case DNGN_DEEP_WATER:
        if (you.species == SP_MERFOLK && (transform_can_swim()
                                          || !you.transform_uncancellable))
        {
            return (false);
        }
        else
            return (true);

    case DNGN_LAVA:
        return (true);

    default:
        // Lava is really the only non-solid glyph above DNGN_MAXSOLID that is
        // not a safe teleport location, and that's handled above.
        if (cell_is_solid(cell))
            return (true);

        return (false);
    }
}

static void _handle_teleport_update (bool large_change, bool check_ring_TC,
                                     const coord_def old_pos)
{
    if (large_change)
    {
        viewwindow();
        for (monster_iterator mi; mi; ++mi)
        {
            const bool see_cell = you.see_cell(mi->pos());

            if (mi->foe == MHITYOU && !see_cell)
            {
                mi->foe_memory = 0;
                behaviour_event(*mi, ME_EVAL);
            }
            else if (see_cell)
                behaviour_event(*mi, ME_EVAL);
        }

        handle_interrupted_swap(true);
    }

    // Might identify unknown ring of teleport control.
    if (check_ring_TC)
        maybe_id_ring_TC();

#ifdef USE_TILE
    if (you.species == SP_MERFOLK)
    {
        const dungeon_feature_type new_grid = grd(you.pos());
        const dungeon_feature_type old_grid = grd(old_pos);
        if (feat_is_water(old_grid) && !feat_is_water(new_grid)
            || !feat_is_water(old_grid) && feat_is_water(new_grid))
        {
            init_player_doll();
        }
    }
#endif
}

static bool _teleport_player(bool allow_control, bool new_abyss_area,
                             bool wizard_tele)
{
    bool is_controlled = (allow_control && !you.confused()
                          && player_control_teleport()
                          && allow_control_teleport()
                          && !you.berserk());

    // All wizard teleports are automatically controlled.
    if (wizard_tele)
        is_controlled = true;

    if (!wizard_tele
        && ((!new_abyss_area && crawl_state.game_is_sprint())
            || item_blocks_teleport(true, true)))
    {
        canned_msg(MSG_STRANGE_STASIS);
        return (false);
    }

    // After this point, we're guaranteed to teleport. Kill the appropriate
    // delays.
    interrupt_activity(AI_TELEPORT);

    // Update what we can see at the current location as well as its stash,
    // in case something happened in the exact turn that we teleported
    // (like picking up/dropping an item).
    viewwindow();
    StashTrack.update_stash(you.pos());

    if (you.duration[DUR_CONDENSATION_SHIELD] > 0)
        remove_condensation_shield();

    if (you.level_type == LEVEL_ABYSS)
    {
        abyss_teleport(new_abyss_area);
        if (you.pet_target != MHITYOU)
            you.pet_target = MHITNOT;

        return (true);
    }

    coord_def pos(1, 0);
    const coord_def old_pos = you.pos();
    bool      large_change  = false;
    bool      check_ring_TC = false;

    if (is_controlled)
    {
        check_ring_TC = true;

        // Only have the messages and the more prompt for non-wizard.
        if (!wizard_tele)
        {
            mpr("You may choose your destination (press '.' or delete to select).");
            mpr("Expect minor deviation.");
            more();
        }

        while (true)
        {
            level_pos lpos;
            bool chose = show_map(lpos, false, true, false);
            pos = lpos.pos;
            redraw_screen();

#if defined(USE_UNIX_SIGNALS) && defined(SIGHUP_SAVE) && defined(USE_CURSES)
            // If we've received a HUP signal then the user can't choose a
            // location, so cancel the teleport.
            if (crawl_state.seen_hups)
            {
                mpr("Controlled teleport interrupted by HUP signal, "
                    "cancelling teleport.", MSGCH_ERROR);
                if (!wizard_tele)
                    contaminate_player(1, true);
                return (false);
            }
#endif

            dprf("Target square (%d,%d)", pos.x, pos.y);

            if (!chose || pos == you.pos())
            {
                if (!wizard_tele)
                {
                    if (!yesno("Are you sure you want to cancel this teleport?",
                               true, 'n'))
                    {
                        continue;
                    }
                }
                if (!wizard_tele)
                    contaminate_player(1, true);
                maybe_id_ring_TC();
                return (false);
            }

            monster* beholder = you.get_beholder(pos);
            if (beholder && !wizard_tele)
            {
                mprf("You cannot teleport away from %s!",
                     beholder->name(DESC_NOCAP_THE, true).c_str());
                mpr("Choose another destination (press '.' or delete to select).");
                more();
                continue;
            }

            monster* fearmonger = you.get_fearmonger(pos);
            if (fearmonger && !wizard_tele)
            {
                mprf("You cannot teleport closer to %s!",
                     fearmonger->name(DESC_NOCAP_THE, true).c_str());
                mpr("Choose another destination (press '.' or delete to select).");
                more();
                continue;
            }
            break;
        }

        // Don't randomly walk wizard teleports.
        if (!wizard_tele)
        {
            pos.x += random2(3) - 1;
            pos.y += random2(3) - 1;

            if (one_chance_in(4))
            {
                pos.x += random2(3) - 1;
                pos.y += random2(3) - 1;
            }
            dprf("Scattered target square (%d, %d)", pos.x, pos.y);
        }

        if (!in_bounds(pos))
        {
            mpr("Nearby solid objects disrupt your rematerialisation!");
            is_controlled = false;
        }

        if (is_controlled)
        {
            if (!you.see_cell(pos))
                large_change = true;

            // Merfolk should be able to control-tele into deep water.
            if (_cell_vetoes_teleport(pos))
            {
                dprf("Target square (%d, %d) vetoed, now random teleport.", pos.x, pos.y);
                is_controlled = false;
                large_change  = false;
            }
            else if (testbits(env.pgrid(pos), FPROP_NO_CTELE_INTO) && !wizard_tele)
            {
                is_controlled = false;
                large_change = false;
                mpr("A strong magical force throws you back!", MSGCH_WARN);
            }
            else
            {
                // Leave a purple cloud.
                place_cloud(CLOUD_TLOC_ENERGY, old_pos, 1 + random2(3), KC_YOU);

                // Controlling teleport contaminates the player. - bwr
                move_player_to_grid(pos, false, true);
                if (!wizard_tele)
                    contaminate_player(1, true);
            }
        }
    }

    if (!is_controlled)
    {
        coord_def newpos;

        // If in a labyrinth, always teleport well away from the centre.
        // (Check done for the straight line, no pathfinding involved.)
        bool need_distance_check = false;
        coord_def centre;
        if (you.level_type == LEVEL_LABYRINTH)
        {
            bool success = false;
            for (int xpos = 0; xpos < GXM; xpos++)
            {
                for (int ypos = 0; ypos < GYM; ypos++)
                {
                    centre = coord_def(xpos, ypos);
                    if (!in_bounds(centre))
                        continue;

                    if (grd(centre) == DNGN_ESCAPE_HATCH_UP)
                    {
                        success = true;
                        break;
                    }
                }
                if (success)
                    break;
            }
            need_distance_check = success;
        }

        do
            newpos = random_in_bounds();
        while (_cell_vetoes_teleport(newpos)
               || need_distance_check && (newpos - centre).abs() < 34*34
               || testbits(env.pgrid(newpos), FPROP_NO_RTELE_INTO));

        if (newpos == old_pos)
            mpr("Your surroundings flicker for a moment.");
        else if (you.see_cell(newpos))
            mpr("Your surroundings seem slightly different.");
        else
        {
            mpr("Your surroundings suddenly seem different.");
            large_change = true;
        }

        // Leave a purple cloud.
        place_cloud(CLOUD_TLOC_ENERGY, old_pos, 1 + random2(3), KC_YOU);

        move_player_to_grid(newpos, false, true);
    }

    _handle_teleport_update(large_change, check_ring_TC, old_pos);
    return (!is_controlled);
}

bool you_teleport_to(const coord_def where_to, bool move_monsters)
{
    // Attempts to teleport the player from their current location to 'where'.
    // Follows this line of reasoning:
    //   1. Check the location (against _cell_vetoes_teleport), if valid,
    //      teleport the player there.
    //   2. If not because of a monster, and move_monster, teleport that
    //      monster out of the way, then teleport the player there.
    //   3. Otherwise, iterate over adjacent squares. If a sutiable position is
    //      found (or a monster can be moved out of the way, with move_monster)
    //      then teleport the player there.
    //   4. If not, give up and return false.

    bool check_ring_TC = false;
    const coord_def old_pos = you.pos();
    coord_def where = where_to;
    coord_def old_where = where_to;

    // Don't bother to calculate a possible new position if it's out of bounds.
    if (!in_bounds(where))
        return (false);

    if (_cell_vetoes_teleport(where))
    {
        if (monster_at(where) && move_monsters && !_cell_vetoes_teleport(where, false))
        {
            monster* mons = monster_at(where);
            mons->teleport(true);
        }
        else
        {
            for (adjacent_iterator ai(where); ai; ++ai)
            {
                if (!_cell_vetoes_teleport(*ai))
                {
                    where = *ai;
                    break;
                }
                else
                {
                    if (monster_at(*ai) && move_monsters
                            && !_cell_vetoes_teleport(*ai, false))
                    {
                        monster* mons = monster_at(*ai);
                        mons->teleport(true);
                        where = *ai;
                        break;
                    }
                }
            }
            // Give up, we can't find a suitable spot.
            if (where == old_where)
                return (false);
        }
    }

    // If we got this far, we're teleporting the player.
    // Leave a purple cloud.
    place_cloud(CLOUD_TLOC_ENERGY, old_pos, 1 + random2(3), KC_YOU);

    bool large_change = you.see_cell(where);

    move_player_to_grid(where, false, true);

    _handle_teleport_update(large_change, check_ring_TC, old_pos);
    return (true);
}

void you_teleport_now(bool allow_control, bool new_abyss_area, bool wizard_tele)
{
    const bool randtele = _teleport_player(allow_control, new_abyss_area,
                                           wizard_tele);

    // Xom is amused by uncontrolled teleports that land you in a
    // dangerous place, unless the player is in the Abyss and
    // teleported to escape from all the monsters chasing him/her,
    // since in that case the new dangerous area is almost certainly
    // *less* dangerous than the old dangerous area.
    // Teleporting in a labyrinth is also funny, more so for non-minotaurs.
    if (randtele
        && (you.level_type == LEVEL_LABYRINTH
            || you.level_type != LEVEL_ABYSS && player_in_a_dangerous_place()))
    {
        if (you.level_type == LEVEL_LABYRINTH && you.species == SP_MINOTAUR)
            xom_is_stimulated(128);
        else
            xom_is_stimulated(255);
    }
}

// Restricted to main dungeon for historical reasons, probably for
// balance: otherwise you have an instant teleport from anywhere.
int portal()
{
    // Disabled completely in zotdef
    if (!player_in_branch(BRANCH_MAIN_DUNGEON) 
	|| crawl_state.game_is_zotdef())
    {
        mpr("This spell doesn't work here.");
        return (-1);
    }
    else if (grd(you.pos()) != DNGN_FLOOR)
    {
        mpr("You must find a clear area in which to cast this spell.");
        return (-1);
    }
    else if (you.char_direction == GDT_ASCENDING)
    {
        // Be evil if you've got the Orb.
        mpr("An empty arch forms before you, then disappears.");
        return (1);
    }

    mpr("Which direction ('<<' for up, '>' for down, 'x' to quit)? ",
        MSGCH_PROMPT);

    level_id lid = level_id::current();
    const int brdepth = branches[lid.branch].depth;

    int dir_sign = 0;
    while (dir_sign == 0)
    {
        const int keyin = getch();
        switch (keyin)
        {
        case '<':
            if (lid.depth == 1)
                mpr("You can't go any further upwards with this spell.");
            else
                dir_sign = -1;
            break;

        case '>':
            if (lid.depth == brdepth)
                mpr("You can't go any further downwards with this spell.");
            else
                dir_sign = 1;
            break;

        case 'x':
            canned_msg(MSG_OK);
            return (-1);

        default:
            break;
        }
    }

    mpr("How many levels (1-9, 'x' to quit)? ", MSGCH_PROMPT);

    int amount = 0;
    while (amount == 0)
    {
        const int keyin = getch();
        if (isadigit(keyin))
            amount = (keyin - '0') * dir_sign;
        else if (keyin == 'x')
        {
            canned_msg(MSG_OK);
            return (-1);
        }
    }

    mpr("You fall through a mystic portal, and materialise at the "
        "foot of a staircase.");
    more();

    lid.depth = std::max(1, std::min(brdepth, lid.depth + amount));
    down_stairs(DNGN_STONE_STAIRS_DOWN_I, EC_UNKNOWN, &lid);

    return (1);
}

bool cast_portal_projectile(int pow)
{
    dist target;
    int item = get_ammo_to_shoot(-1, target, true);
    if (item == -1)
        return (false);

    if (cell_is_solid(target.target))
    {
        mpr("You can't shoot at gazebos.");
        return (false);
    }

    // Can't use portal through walls. (That'd be just too cheap!)
    if (you.trans_wall_blocking(target.target))
    {
        mpr("A translucent wall is in the way.");
        return (false);
    }

    if (!check_warning_inscriptions(you.inv[item], OPER_FIRE))
        return (false);

    bolt beam;
    throw_it(beam, item, true, random2(pow/4), &target);

    return (true);
}

bool cast_apportation(int pow, const coord_def& where)
{
    if (you.trans_wall_blocking(where))
    {
        mpr("Something is in the way.");
        return (false);
    }

    // Letting mostly-melee characters spam apport after every Shoals
    // fight seems like it has too much grinding potential.  We could
    // weaken this for high power.
    if (grd(where) == DNGN_DEEP_WATER || grd(where) == DNGN_LAVA)
    {
        mpr("The density of the terrain blocks your spell.");
        return (false);
    }

    // Let's look at the top item in that square...
    // And don't allow apporting from shop inventories.
    const int item_idx = igrd(where);
    if (item_idx == NON_ITEM || !in_bounds(where))
    {
        // Maybe the player *thought* there was something there (a mimic.)
        if (monster* m = monster_at(where))
        {
            if (mons_is_mimic(m->type) && you.can_see(m))
            {
                mprf("%s twitches.", m->name(DESC_CAP_THE).c_str());
                // Nothing else gives this message, so identify the mimic.
                m->flags |= MF_KNOWN_MIMIC;
                return (true);  // otherwise you get free mimic ID
            }
        }

        mpr("There are no items there.");
        return (false);
    }

    item_def& item = mitm[item_idx];

    // Can't apport the Orb in zotdef
    if (crawl_state.game_is_zotdef() && item_is_orb(item))
    {
        mpr("You cannot apport the sacred Orb!");
	return (false);
    }

    // Protect the player from destroying the item.
    if (feat_destroys_item(grd(you.pos()), item))
    {
        mpr("That would be silly while over this terrain!");
        return (false);
    }

    // Mass of one unit.
    const int unit_mass = item_mass(item);
    const int max_mass = pow * 30 + random2(pow * 20);

    int max_units = item.quantity;
    if (unit_mass > 0)
        max_units = max_mass / unit_mass;

    if (max_units <= 0)
    {
        mpr("The mass is resisting your pull.");
        return (true);
    }

    // We need to modify the item *before* we move it, because
    // move_top_item() might change the location, or merge
    // with something at our position.
    mprf("Yoink! You pull the item%s to yourself.",
         (item.quantity > 1) ? "s" : "");

    if (max_units < item.quantity)
    {
        item.quantity = max_units;
        mpr("You feel that some mass got lost in the cosmic void.");
    }

    // If we apport a net, free the monster under it.
    if (item.base_type == OBJ_MISSILES
        && item.sub_type == MI_THROWING_NET
        && item_is_stationary(item))
    {
        remove_item_stationary(item);
        if (monster* mons = monster_at(where))
            mons->del_ench(ENCH_HELD, true);
    }

    // Actually move the item.
    move_top_item(where, you.pos());
    // Mark the item as found now.
    origin_set(you.pos());

    return (true);
}

static int _quadrant_blink(coord_def where, int pow, int, actor *)
{
    if (where == you.pos())
        return (0);

    if (you.level_type == LEVEL_ABYSS)
    {
        abyss_teleport(false);
        if (you.pet_target != MHITYOU)
            you.pet_target = MHITNOT;
        return (1);
    }

    if (pow > 100)
        pow = 100;

    const int dist = random2(6) + 2;  // 2-7

    // This is where you would *like* to go.
    const coord_def base = you.pos() + (where - you.pos()) * dist;

    // This can take a while if pow is high and there's lots of translucent
    // walls nearby.
    coord_def target;
    bool found = false;
    for (int i = 0; i < (pow*pow) / 500 + 1; ++i)
    {
        // Find a space near our base point...
        // First try to find a random square not adjacent to the basepoint,
        // then one adjacent if that fails.
        if (!random_near_space(base, target)
            && !random_near_space(base, target, true))
        {
            return 0;
        }

        // ... which is close enough, but also far enough from us.
        if (distance(base, target) > 10 || distance(you.pos(), target) < 8)
            continue;

        if (!you.see_cell_no_trans(target))
            continue;

        found = true;
        break;
    }

    if (!found)
        return(0);

    coord_def origin = you.pos();
    move_player_to_grid(target, false, true);

    // Leave a purple cloud.
    place_cloud(CLOUD_TLOC_ENERGY, origin, 1 + random2(3), KC_YOU);

    return (1);
}

int cast_semi_controlled_blink(int pow)
{
    int result = apply_one_neighbouring_square(_quadrant_blink, pow);

    // Controlled blink causes glowing.
    if (result)
        contaminate_player(1, true);

    return (result);
}

bool can_cast_golubrias_passage()
{
    return find_golubria_on_level().size() < 2;
}

bool cast_golubrias_passage(const coord_def& where)
{
    // randomize position a bit to make it not as useful to use on monsters
    // chasing you, as well as to not give away hidden trap positions
    int tries = 0;
    coord_def randomized_where = where;
    do
    {
        tries++;
        randomized_where = where;
        randomized_where.x += random_range(-2, 2);
        randomized_where.y += random_range(-2, 2);
    } while((!in_bounds(randomized_where) ||
             grd(randomized_where) != DNGN_FLOOR ||
             monster_at(randomized_where) ||
             !you.see_cell(randomized_where) ||
             you.trans_wall_blocking(randomized_where) ||
             randomized_where == you.pos()) &&
            tries < 100);

    if (tries >= 100)
    {
        if (you.trans_wall_blocking(randomized_where))
            mpr("You cannot create a passage on the other side of the transparent wall.");
        else
            // XXX: bleh, dumb message
            mpr("Creating a passage of Golubria requires sufficient empty space.");
        return false;
    }

    if (!allow_control_teleport(true) ||
        testbits(env.pgrid(randomized_where), FPROP_NO_CTELE_INTO))
    {
        // lose a turn
        mpr("A powerful magic interferes with the creation of the passage.");
        place_cloud(CLOUD_TLOC_ENERGY, randomized_where, 3 + random2(3),
                    KC_YOU);
        return true;
    }

    place_specific_trap(randomized_where, TRAP_GOLUBRIA);

    trap_def *trap = find_trap(randomized_where);
    if (!trap)
    {
        mpr("Something buggy happened.");
        return false;
    }

    trap->reveal();

    return true;
}
