/**
 * @file
 * @brief Translocation spells.
**/

#include "AppHdr.h"

#include "spl-transloc.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "abyss.h"
#include "act-iter.h"
#include "areas.h"
#include "cloud.h"
#include "coordit.h"
#include "delay.h"
#include "directn.h"
#include "dungeon.h"
#include "itemprop.h"
#include "items.h"
#include "libutil.h"
#include "los.h"
#include "losglobal.h"
#include "losparam.h"
#include "message.h"
#include "mgen_data.h"
#include "misc.h"
#include "mon-behv.h"
#include "mon-death.h"
#include "mon-place.h"
#include "mon-util.h"
#include "orb.h"
#include "output.h"
#include "prompt.h"
#include "shout.h"
#include "spl-util.h"
#include "stash.h"
#include "state.h"
#include "stringutil.h"
#include "teleport.h"
#include "terrain.h"
#include "traps.h"
#include "view.h"
#include "viewmap.h"
#include "xom.h"

static void _quadrant_blink(coord_def dir, int pow);

/**
 * Place a cloud of translocational energy at a player's previous location,
 * to make it easier for players to tell what just happened.
 *
 * @param origin    The player's previous location.
 */
static void _place_tloc_cloud(const coord_def &origin)
{
    if (!cell_is_solid(origin))
        place_cloud(CLOUD_TLOC_ENERGY, origin, 1 + random2(3), &you);
}

spret_type cast_disjunction(int pow, bool fail)
{
    fail_check();
    int rand = random_range(35, 45) + random2(pow / 12);
    you.duration[DUR_DISJUNCTION] = min(90 + pow / 12,
        max(you.duration[DUR_DISJUNCTION] + rand,
        30 + rand));
    contaminate_player(1000, true);
    disjunction();
    return SPRET_SUCCESS;
}

void disjunction()
{
    int steps = you.time_taken;
    invalidate_agrid(true);
    for (int step = 0; step < steps; ++step)
    {
        vector<monster*> mvec;
        for (radius_iterator ri(you.pos(), LOS_RADIUS, C_ROUND); ri; ++ri)
        {
            monster* mons = monster_at(*ri);
            if (!mons || !you.see_cell(*ri))
                continue;
            mvec.push_back(mons);
        }
        if (mvec.empty())
            return;
        // blink should be isotropic
        shuffle_array(mvec);
        for (auto mons : mvec)
        {
            if (!mons->alive() || mons->no_tele())
                continue;
            coord_def p = mons->pos();
            if (!disjunction_haloed(p))
                continue;

            int dist = grid_distance(you.pos(), p);
            int decay = max(1, (dist - 1) * (dist + 1));
            int chance = pow(0.8, 1.0 / decay) * 1000;
            if (!x_chance_in_y(chance, 1000))
                blink_away(mons, &you, false);
        }
    }
}

/**
 * Attempt to blink the player to a random nearby tile.
 *
 * @param override_stasis       Whether to blink even if the player is under
 *                              stasis (& thus normally unable to).
 */
void uncontrolled_blink(bool override_stasis)
{
    if (you.no_tele(true, true, true) && !override_stasis)
    {
        canned_msg(MSG_STRANGE_STASIS);
        return;
    }

    coord_def target;
    // First try to find a random square not adjacent to the player,
    // then one adjacent if that fails.
    if (!random_near_space(&you, you.pos(), target)
             && !random_near_space(&you, you.pos(), target, true))
    {
        mpr("You feel jittery for a moment.");
        return;
    }

    if (!you.attempt_escape(2)) // prints its own messages
        return;

    canned_msg(MSG_YOU_BLINK);
    const coord_def origin = you.pos();
    move_player_to_grid(target, false);
    _place_tloc_cloud(origin);
}

/**
 * Attempt to blink the player to a random nearby tile, in a direction of
 * the player's choosing.
 *
 * @param pow           Determines the number of iterations to run for (1-21),
 *                      which increases the odds of actually getting a blink in
 *                      the right direction.
 * @param fail          Whether this came from a miscast spell (& should
 *                      therefore fail after selecting a direction)
 * @param safe_cancel   Whether it's OK to let the player cancel the control
 *                      of the blink (or whether there should be a prompt -
 *                      for e.g. cblink under the ORB, in which a recast could
 *                      turn into random blink instead)
 * @param end_ctele     Whether to end cTele.
 * @return              Whether the blink succeed, aborted, or was miscast.
 */
spret_type semicontrolled_blink(int pow, bool fail, bool safe_cancel,
                                bool end_ctele)
{
    dist bmove;
    direction_chooser_args args;
    args.restricts = DIR_DIR;
    args.mode = TARG_ANY;

    while (true)
    {
        mprf(MSGCH_PROMPT, "Which direction? [ESC to cancel]");
        direction(bmove, args);

        if (crawl_state.seen_hups)
        {
            mpr("Cancelling blink due to HUP.");
            return SPRET_ABORT;
        }

        if (bmove.isValid && !bmove.delta.origin())
            break;

        if (safe_cancel
            || yesno("Are you sure you want to cancel this blink?", false ,'n'))
        {
            canned_msg(MSG_OK);
            return SPRET_ABORT;
        }
    }

    fail_check();

    if (you.no_tele(true, true, true))
    {
        canned_msg(MSG_STRANGE_STASIS);
        return SPRET_SUCCESS; // of a sort
    }

    if (!you.attempt_escape(2)) // prints its own messages
        return SPRET_SUCCESS; // of a sort

    _quadrant_blink(bmove.delta, pow);
    // Controlled blink causes glowing.
    contaminate_player(1000, true);
    // End teleport control if this was a random blink upgraded by cTele.
    if (end_ctele && you.duration[DUR_CONTROL_TELEPORT])
    {
        mprf(MSGCH_DURATION, "You feel uncertain.");
        you.duration[DUR_CONTROL_TELEPORT] = 0;
    }
    return SPRET_SUCCESS;
}


/**
 * Let the player choose a destination for their controlled blink.
 *
 * @param target[out]   The target found, if any.
 * @param safe_cancel   Whether it's OK to let the player cancel the control
 *                      of the blink (or whether there should be a prompt -
 *                      for e.g. ?blink with blurryvis)
 * @return              True if a target was found; false if the player aborted.
 */
static bool _find_cblink_target(coord_def &target, bool safe_cancel)
{
    // query for location {dlb}:
    direction_chooser_args args;
    args.restricts = DIR_TARGET;
    args.needs_path = false;
    args.may_target_monster = false;
    args.top_prompt = "Blink to where?";
    dist beam;
    direction(beam, args);

    if (crawl_state.seen_hups)
    {
        mpr("Cancelling blink due to HUP.");
        return false;
    }

    if (!beam.isValid || beam.target == you.pos())
    {
        if (!safe_cancel
            && !yesno("Are you sure you want to cancel this blink?",
                      false, 'n'))
        {
            clear_messages();
            return _find_cblink_target(target, safe_cancel);
        }

        canned_msg(MSG_OK);
        return false;
    }

    const monster* beholder = you.get_beholder(beam.target);
    if (beholder)
    {
        mprf("You cannot blink away from %s!",
             beholder->name(DESC_THE, true).c_str());
        return _find_cblink_target(target, safe_cancel);
    }

    const monster* fearmonger = you.get_fearmonger(beam.target);
    if (fearmonger)
    {
        mprf("You cannot blink closer to %s!",
             fearmonger->name(DESC_THE, true).c_str());
        return _find_cblink_target(target, safe_cancel);
    }

    if (cell_is_solid(beam.target))
    {
        clear_messages();
        mpr("You can't blink into that!");
        return _find_cblink_target(target, safe_cancel);
    }

    if (!check_moveto(beam.target, "blink"))
    {
        return _find_cblink_target(target, safe_cancel);
        // try again (messages handled by check_moveto)
    }

    if (you.see_cell_no_trans(beam.target))
    {
        target = beam.target; // Grid in los, no problem.
        return true;
    }

    clear_messages();
    if (you.trans_wall_blocking(beam.target))
        mpr("There's something in the way!");
    else
        mpr("You can only blink to visible locations.");
    return _find_cblink_target(target, safe_cancel);
}

void wizard_blink()
{
    // query for location {dlb}:
    direction_chooser_args args;
    args.restricts = DIR_TARGET;
    args.needs_path = false;
    args.may_target_monster = false;
    args.top_prompt = "Blink to where?";
    dist beam;
    direction(beam, args);

    if (!beam.isValid || beam.target == you.pos())
    {
        canned_msg(MSG_OK);
        return;
    }

    if (!in_bounds(beam.target))
    {
        clear_messages();
        mpr("Please don't blink into the map border.");
        return wizard_blink();
    }

    if (monster_at(beam.target))
    {
        clear_messages();
        mpr("Please don't try to blink into monsters.");
        return wizard_blink();
    }

    if (!check_moveto(beam.target, "blink"))
    {
        return wizard_blink();
        // try again (messages handled by check_moveto)
    }

    // Allow wizard blink to send player into walls, in case the
    // user wants to alter that grid to something else.
    if (cell_is_solid(beam.target))
        grd(beam.target) = DNGN_FLOOR;

    move_player_to_grid(beam.target, false);
}

/**
 * Attempt to blink the player to a nearby tile of their choosing.
 *
 * @param fail          Whether this came from a miscast spell (& should
 *                      therefore fail after selecting a target)
 * @param safe_cancel   Whether it's OK to let the player cancel the control
 *                      of the blink (or whether there should be a prompt -
 *                      for e.g. ?blink with blurryvis)
 * @return              Whether the blink succeeded, aborted, or was miscast.
 */
spret_type controlled_blink(bool fail, bool safe_cancel)
{
    coord_def target;
    if (!_find_cblink_target(target, safe_cancel))
        return SPRET_ABORT;

    fail_check();

    if (you.no_tele(true, true, true))
    {
        canned_msg(MSG_STRANGE_STASIS);
        return SPRET_SUCCESS; // of a sort
    }

    if (!you.attempt_escape(2))
        return SPRET_SUCCESS; // of a sort

    if (cell_is_solid(target) || monster_at(target))
    {
        mpr("Oops! There was something there already!");
        uncontrolled_blink();
        return SPRET_SUCCESS; // of a sort
    }

    _place_tloc_cloud(you.pos());
    move_player_to_grid(target, false);
    // Controlling teleport contaminates the player. -- bwr
    contaminate_player(1000, true);

    crawl_state.cancel_cmd_again();
    crawl_state.cancel_cmd_repeat();

    return SPRET_SUCCESS;
}

/**
 * Cast the player spell Blink.
 *
 * @param allow_control     Whether cTele can be used to transform the blink
 *                          into a semicontrolled blink. (False for e.g. Xom.)
 * @param fail              Whether the player miscast the spell.
 * @return                  Whether the spell was successfully cast, aborted,
 *                          or miscast.
 */
spret_type cast_blink(bool allow_control, bool fail)
{
    // effects that cast the spell through the player, I guess (e.g. xom)
    if (you.no_tele(false, false, true))
        return fail ? SPRET_FAIL : SPRET_SUCCESS; // probably always SUCCESS

    if (allow_control && player_control_teleport()
        && allow_control_teleport(true))
    {
        if (!you.confused())
            return semicontrolled_blink(100, fail);

        // can't put this in allow_control_teleport(), since that's called for
        // status lights, etc (and we don't want those to flip on and off
        // whenever you're confused... probably?)
        mpr("You're too confused to control your translocation!");
        // anyway, fallthrough to random blink
    }

    fail_check();
    allow_control_teleport(); // print messages only after successfully casting
    uncontrolled_blink();
    return SPRET_SUCCESS;
}

/**
 * Cast the player spell Controlled Blink.
 *
 * @param pow     The power with which the spell is being cast.
 *                Only used when the blink is degraded to semicontrolled.
 * @param fail    Whether the player miscast the spell.
 * @param safe    Whether it's safe to abort (not e.g. unknown ?blink)
 * @return        Whether the spell was successfully cast, aborted, or miscast.
 */
spret_type cast_controlled_blink(int pow, bool fail, bool safe)
{
    // don't prompt if it's useless
    if (you.no_tele(true, true))
    {
        canned_msg(MSG_STRANGE_STASIS);
        return SPRET_ABORT;
    }

    if (crawl_state.is_repeating_cmd())
    {
        crawl_state.cant_cmd_repeat("You can't repeat controlled blinks.");
        crawl_state.cancel_cmd_again();
        crawl_state.cancel_cmd_repeat();
        return SPRET_ABORT;
    }

    if (!allow_control_teleport())
        return semicontrolled_blink(pow, fail, safe, false);

    return controlled_blink(fail, safe);
}

/**
 * Can the player control their teleportation?
 *
 * Doesn't guaranteed that they *can*, just that there aren't any effects
 * preventing them from doing so.
 *
 * @param quiet     Whether to suppress messages.
 * @return          Whether the player can currently control their blinks/
 *                  teleports.
 */
bool allow_control_teleport(bool quiet)
{
    // Attempt to order from most to least permanent.
    if (orb_haloed(you.pos()))
    {
        if (!quiet)
            mprf(MSGCH_ORB, "The orb prevents control of your teleportation!");
        return false;
    }

    if (testbits(env.level_flags, LFLAG_NO_TELE_CONTROL))
    {
        if (!quiet)
            mpr("A powerful magic prevents control of your teleportation.");
        return false;
    }

    if (you.beheld())
    {
        if (!quiet)
        {
            mpr("It is impossible to concentrate on your destination while "
                "mesmerised.");
        }
        return false;
    }

    return true;
}

spret_type cast_teleport_self(bool fail)
{
    fail_check();
    you_teleport();
    return SPRET_SUCCESS;
}

void you_teleport()
{
    // [Cha] here we block teleportation, which will save the player from
    // death from read-id'ing scrolls (in sprint)
    if (you.no_tele(true, true))
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

        // Doesn't care whether the cTele will actually work or not.
        if (player_control_teleport())
        {
            mpr("You feel your translocation being delayed.");
            teleport_delay += 1 + random2(3);
        }
        if (player_in_branch(BRANCH_ABYSS) && !one_chance_in(5))
        {
            mpr("You feel the power of the Abyss delaying your translocation.");
            teleport_delay += 5 + random2(10);
        }
        else if (orb_haloed(you.pos()))
        {
            mprf(MSGCH_ORB, "You feel the orb delaying this translocation!");
            teleport_delay += 5 + random2(5);
        }

        you.set_duration(DUR_TELEPORT, teleport_delay);
    }
}

// Should return true if we don't want anyone to teleport here.
static bool _cell_vetoes_teleport(const coord_def cell, bool check_monsters = true,
                                  bool wizard_tele = false)
{
    // Monsters always veto teleport.
    if (monster_at(cell) && check_monsters)
        return true;

    // As do all clouds; this may change.
    if (env.cgrid(cell) != EMPTY_CLOUD && !wizard_tele)
        return true;

    if (cell_is_solid(cell))
        return true;

    return is_feat_dangerous(grd(cell), true) && !wizard_tele;
}

static void _handle_teleport_update(bool large_change, const coord_def old_pos)
{
    if (large_change)
    {
        viewwindow();
        for (monster_iterator mi; mi; ++mi)
        {
            const bool see_cell = you.see_cell(mi->pos());

            if (mi->foe == MHITYOU && !see_cell && !you.penance[GOD_ASHENZARI])
            {
                mi->foe_memory = 0;
                behaviour_event(*mi, ME_EVAL);
            }
            else if (see_cell)
                behaviour_event(*mi, ME_EVAL);
        }
    }

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

static bool _teleport_player(bool allow_control, bool wizard_tele,
                             int range)
{
    bool is_controlled = (allow_control && !you.confused()
                          && player_control_teleport()
                          && allow_control_teleport()
                          && !you.berserk());

    // All wizard teleports are automatically controlled.
    if (wizard_tele)
        is_controlled = true;

    if (!wizard_tele
        && (crawl_state.game_is_sprint() || you.no_tele(true, true))
            && !player_in_branch(BRANCH_ABYSS))
    {
        canned_msg(MSG_STRANGE_STASIS);
        return false;
    }

    // After this point, we're guaranteed to teleport. Kill the appropriate
    // delays.
    interrupt_activity(AI_TELEPORT);

    // Update what we can see at the current location as well as its stash,
    // in case something happened in the exact turn that we teleported
    // (like picking up/dropping an item).
    viewwindow();
    StashTrack.update_stash(you.pos());

    if (player_in_branch(BRANCH_ABYSS) && !wizard_tele)
    {
        abyss_teleport();
        if (you.pet_target != MHITYOU)
            you.pet_target = MHITNOT;

        return true;
    }

    coord_def pos(1, 0);
    const coord_def old_pos = you.pos();
    bool      large_change  = false;

    if (is_controlled)
    {
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

            // If we've received a HUP signal then the user can't choose a
            // location, so cancel the teleport.
            if (crawl_state.seen_hups)
            {
                mprf(MSGCH_ERROR, "Controlled teleport interrupted by HUP signal, "
                                  "cancelling teleport.");
                if (!wizard_tele)
                    contaminate_player(1000, true);
                return false;
            }

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
                    contaminate_player(1000, true);
                return false;
            }

            monster* beholder = you.get_beholder(pos);
            if (beholder && !wizard_tele)
            {
                mprf("You cannot teleport away from %s!",
                     beholder->name(DESC_THE, true).c_str());
                mpr("Choose another destination (press '.' or delete to select).");
                more();
                continue;
            }

            monster* fearmonger = you.get_fearmonger(pos);
            if (fearmonger && !wizard_tele)
            {
                mprf("You cannot teleport closer to %s!",
                     fearmonger->name(DESC_THE, true).c_str());
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
            if (_cell_vetoes_teleport(pos, true, wizard_tele))
            {
                if (wizard_tele)
                {
                    mprf(MSGCH_WARN, "Even you can't go there right now. Sorry!");
                    return false;
                }

                dprf("Target square (%d, %d) vetoed, now random teleport.", pos.x, pos.y);
                is_controlled = false;
                large_change  = false;
            }
            else if (testbits(env.pgrid(pos), FPROP_NO_CTELE_INTO) && !wizard_tele)
            {
                is_controlled = false;
                large_change = false;
                mprf(MSGCH_WARN, "A strong magical force throws you back!");
            }
            else
            {
                // Leave a purple cloud.
                if (!wizard_tele)
                    _place_tloc_cloud(old_pos);

                move_player_to_grid(pos, false);

                // Controlling teleport contaminates the player. - bwr
                if (!wizard_tele)
                    contaminate_player(1000, true);
            }
            // End teleport control.
            if (you.duration[DUR_CONTROL_TELEPORT])
            {
                mprf(MSGCH_DURATION, "You feel uncertain.");
                you.duration[DUR_CONTROL_TELEPORT] = 0;
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
        if (player_in_branch(BRANCH_LABYRINTH))
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

        int tries = 500;
        do
        {
            newpos = random_in_bounds();
        }
        while (--tries > 0
               && (_cell_vetoes_teleport(newpos)
                   || (newpos - old_pos).abs() > dist_range(range)
                   || need_distance_check && (newpos - centre).abs()
                                              <= dist_range(min(range - 1, 34))
                   || testbits(env.pgrid(newpos), FPROP_NO_RTELE_INTO)));

        // Running out of tries should only happen for limited-range teleports,
        // which are all involuntary; no message.  Return false so it doesn't
        // count as a random teleport for Xom purposes.
        if (tries == 0)
            return false;
        else if (newpos == old_pos)
            mpr("Your surroundings flicker for a moment.");
        else if (you.see_cell(newpos))
            mpr("Your surroundings seem slightly different.");
        else
        {
            mpr("Your surroundings suddenly seem different.");
            large_change = true;
        }

        // Leave a purple cloud.
        _place_tloc_cloud(old_pos);

        move_player_to_grid(newpos, false);
    }

    _handle_teleport_update(large_change, old_pos);
    return !is_controlled;
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

    const coord_def old_pos = you.pos();
    coord_def where = where_to;
    coord_def old_where = where_to;

    // Don't bother to calculate a possible new position if it's out of bounds.
    if (!in_bounds(where))
        return false;

    if (_cell_vetoes_teleport(where))
    {
        if (monster_at(where) && move_monsters && !_cell_vetoes_teleport(where, false))
        {
            // dlua only, don't heed no_tele
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
                return false;
        }
    }

    // If we got this far, we're teleporting the player.
    _place_tloc_cloud(old_pos);

    bool large_change = you.see_cell(where);

    move_player_to_grid(where, false);

    _handle_teleport_update(large_change, old_pos);
    return true;
}

void you_teleport_now(bool allow_control, bool wizard_tele,
                      int range)
{
    const bool randtele = _teleport_player(allow_control, wizard_tele, range);

    // Xom is amused by uncontrolled teleports that land you in a
    // dangerous place, unless the player is in the Abyss and
    // teleported to escape from all the monsters chasing him/her,
    // since in that case the new dangerous area is almost certainly
    // *less* dangerous than the old dangerous area.
    // Teleporting in a labyrinth is also funny, more so for non-minotaurs.
    if (randtele
        && (player_in_branch(BRANCH_LABYRINTH)
            || !player_in_branch(BRANCH_ABYSS) && player_in_a_dangerous_place()))
    {
        xom_is_stimulated(200);
    }
}

spret_type cast_portal_projectile(int pow, bool fail)
{
    fail_check();
    if (!you.duration[DUR_PORTAL_PROJECTILE])
        mpr("You begin teleporting projectiles to their destination.");
    else
        mpr("You renew your portal.");
    // Calculate the accuracy bonus based on current spellpower.
    you.attribute[ATTR_PORTAL_PROJECTILE] = pow;
    you.increase_duration(DUR_PORTAL_PROJECTILE, 3 + random2(pow / 2) + random2(pow / 5), 50);
    return SPRET_SUCCESS;
}

spret_type cast_apportation(int pow, bolt& beam, bool fail)
{
    const coord_def where = beam.target;

    if (!cell_see_cell(you.pos(), where, LOS_SOLID))
    {
        mpr("There's something in the way!");
        return SPRET_ABORT;
    }

    // Letting mostly-melee characters spam apport after every Shoals
    // fight seems like it has too much grinding potential.  We could
    // weaken this for high power.
    if (grd(where) == DNGN_DEEP_WATER || grd(where) == DNGN_LAVA)
    {
        mpr("The density of the terrain blocks your spell.");
        return SPRET_ABORT;
    }

    // Let's look at the top item in that square...
    // And don't allow apporting from shop inventories.
    const int item_idx = igrd(where);
    if (item_idx == NON_ITEM || !in_bounds(where))
    {
        mpr("There are no items there.");
        return SPRET_ABORT;
    }

    item_def& item = mitm[item_idx];

    // Nets can be apported when they have a victim trapped.
    if (item_is_stationary(item) && !item_is_stationary_net(item))
    {
        mpr("You cannot apport that!");
        return SPRET_ABORT;
    }

    // Can't apport the Orb in zotdef or sprint
    if (item_is_orb(item)
        && (crawl_state.game_is_zotdef()
            || crawl_state.game_is_sprint()))
    {
        mpr("You cannot apport the Orb!");
        return SPRET_ABORT;
    }

    fail_check();

    // We need to modify the item *before* we move it, because
    // move_top_item() might change the location, or merge
    // with something at our position.
    if (item_is_orb(item))
    {
        fake_noisy(30, where);

        // There's also a 1-in-3 flat chance of apport failing.
        if (one_chance_in(3))
        {
            orb_pickup_noise(where, 30,
                "The orb shrieks and becomes a dead weight against your magic!",
                "The orb lets out a furious burst of light and becomes "
                    "a dead weight against your magic!");
            return SPRET_SUCCESS;
        }
        else // Otherwise it's just a noisy little shiny thing
        {
            orb_pickup_noise(where, 30,
                "The orb shrieks as your magic touches it!",
                "The orb lets out a furious burst of light as your magic touches it!");
        }
    }

    // If we apport a net, free the monster under it.
    if (item_is_stationary_net(item))
    {
        free_stationary_net(item_idx);
        if (monster* mons = monster_at(where))
            mons->del_ench(ENCH_HELD, true);
    }

    // Heavy items require more power to apport directly to your feet.
    // They might end up just moving a few squares, depending on spell
    // power and item mass.
    beam.is_tracer = true;
    beam.aimed_at_spot = true;
    beam.affects_nothing = true;
    beam.fire();

    // Pop the item's location off the end
    beam.path_taken.pop_back();

    // The actual number of squares it needs to traverse to get to you.
    int dist = beam.path_taken.size();

    // The maximum number of squares the item will actually move, always
    // at least one square.
    int max_dist = max(pow * 2 / 5, 1);

    dprf("Apport dist=%d, max_dist=%d", dist, max_dist);

    int location_on_path = max(-1, dist - max_dist);
    // Don't move mimics under you.
    if ((item.flags & ISFLAG_MIMIC) && location_on_path == -1)
        location_on_path = 0;
    coord_def new_spot;
    if (location_on_path == -1)
        new_spot = you.pos();
    else
        new_spot = beam.path_taken[location_on_path];
    // Try to find safe terrain for the item.
    while (location_on_path < dist)
    {
        if (!feat_virtually_destroys_item(grd(new_spot), item))
            break;
        location_on_path++;
        new_spot = beam.path_taken[location_on_path];
    }
    if (location_on_path == dist)
    {
        mpr("Not with that terrain in the way!");
        return SPRET_SUCCESS;
    }
    dprf("Apport: new spot is %d/%d", new_spot.x, new_spot.y);

    // Actually move the item.
    mprf("Yoink! You pull the item%s towards yourself.",
         (item.quantity > 1) ? "s" : "");

    move_top_item(where, new_spot);

    // Mark the item as found now.
    origin_set(new_spot);

    return SPRET_SUCCESS;
}

/**
 * Attempt to blink in the given direction.
 *
 * @param dir   A direction to blink in.
 * @param pow   Determines number of iterations.
 *              (pow^2 / 500 + 1, where pow is 0-100; so 1-21 iterations)
 */
static void _quadrant_blink(coord_def dir, int pow)
{
    pow = min(100, max(0, pow));

    const int dist = random2(6) + 2;  // 2-7

    // This is where you would *like* to go.
    const coord_def base = you.pos() + dir * dist;

    // This can take a while if pow is high and there's lots of translucent
    // walls nearby.
    coord_def target;
    bool found = false;
    for (int i = 0; i < pow*pow / 500 + 1; ++i)
    {
        // Find a space near our base point...
        // First try to find a random square not adjacent to the basepoint,
        // then one adjacent if that fails.
        if (!random_near_space(&you, base, target)
            && !random_near_space(&you, base, target, true))
        {

            continue; // could probably 'break;' random_near_space uses quite
                      // a lot of iterations...
        }

        // ... which is close enough, but also far enough from us.
        if (distance2(base, target) > 10 || distance2(you.pos(), target) < 8)
            continue;

        if (!you.see_cell_no_trans(target))
            continue;

        found = true;
        break;
    }

    if (!found)
        return uncontrolled_blink();

    coord_def origin = you.pos();
    move_player_to_grid(target, false);
    _place_tloc_cloud(origin);
}

spret_type cast_golubrias_passage(const coord_def& where, bool fail)
{
    // randomize position a bit to make it not as useful to use on monsters
    // chasing you, as well as to not give away hidden trap positions
    int tries = 0;
    int tries2 = 0;
    coord_def randomized_where = where;
    coord_def randomized_here = you.pos();
    do
    {
        tries++;
        randomized_where = where;
        randomized_where.x += random_range(-2, 2);
        randomized_where.y += random_range(-2, 2);
    } while ((!in_bounds(randomized_where) ||
             grd(randomized_where) != DNGN_FLOOR ||
             monster_at(randomized_where) ||
             !you.see_cell(randomized_where) ||
             you.trans_wall_blocking(randomized_where) ||
             randomized_where == you.pos()) &&
            tries < 100);

    do
    {
        tries2++;
        randomized_here = you.pos();
        randomized_here.x += random_range(-2, 2);
        randomized_here.y += random_range(-2, 2);
    } while ((!in_bounds(randomized_here) ||
             grd(randomized_here) != DNGN_FLOOR ||
             monster_at(randomized_here) ||
             !you.see_cell(randomized_here) ||
             you.trans_wall_blocking(randomized_here) ||
             randomized_here == you.pos() ||
             randomized_here == randomized_where) &&
            tries2 < 100);

    if (tries >= 100 || tries2 >= 100)
    {
        if (you.trans_wall_blocking(randomized_where))
            mpr("You cannot create a passage on the other side of the transparent wall.");
        else
            // XXX: bleh, dumb message
            mpr("Creating passages of Golubria requires sufficient empty space.");
        return SPRET_ABORT;
    }

    if (!allow_control_teleport(true) ||
        testbits(env.pgrid(randomized_where), FPROP_NO_CTELE_INTO) ||
        testbits(env.pgrid(randomized_here), FPROP_NO_CTELE_INTO))
    {
        mpr("A powerful magic interferes with the creation of the passage.");
        return SPRET_ABORT;
    }

    fail_check();
    place_specific_trap(randomized_where, TRAP_GOLUBRIA);
    place_specific_trap(randomized_here, TRAP_GOLUBRIA);
    env.level_state |= LSTATE_GOLUBRIA;

    trap_def *trap = find_trap(randomized_where);
    trap_def *trap2 = find_trap(randomized_here);
    if (!trap || !trap2)
    {
        mpr("Something buggy happened.");
        return SPRET_ABORT;
    }

    trap->reveal();
    trap2->reveal();

    return SPRET_SUCCESS;
}

static int _disperse_monster(monster* mon, int pow)
{
    if (!mon)
        return 0;

    if (mon->no_tele())
        return 0;

    if (mon->check_res_magic(pow) > 0)
        monster_blink(mon);
    else
        monster_teleport(mon, true);

    if (mon->check_res_magic(pow) <= 0)
        mon->confuse(&you, 1 + random2avg(pow / 10, 2));

    return 1;
}

spret_type cast_dispersal(int pow, bool fail)
{
    fail_check();
    const int radius = spell_range(SPELL_DISPERSAL, pow);
    if (!apply_monsters_around_square(_disperse_monster, you.pos(), pow,
                                      radius))
    {
        mpr("The air shimmers briefly around you.");
    }
    return SPRET_SUCCESS;
}

int singularity_range(int pow, int strength)
{
    // XXX: unify some of this functionality.
    // A singularity is HD (pow / 10) + 1; its strength is
    // (HD / (range^2)) for a given range, so for a given strength the
    // range is sqrt(pow/10 + 1) / strength.

    return max(0, min(LOS_RADIUS, (int)isqrt((pow/10 + 1) / strength)));
}

spret_type cast_singularity(actor* agent, int pow, const coord_def& where,
                            bool fail)
{
    if (cell_is_solid(where))
    {
        if (agent->is_player())
            mpr("You can't place that within a solid object!");
        return SPRET_ABORT;
    }

    actor* victim = actor_at(where);
    if (victim)
    {
        if (you.can_see(victim))
        {
            if (agent->is_player())
                mpr("You can't place the singularity on a creature.");
            return SPRET_ABORT;
        }

        fail_check();

        if (agent->is_player())
            canned_msg(MSG_GHOSTLY_OUTLINE);
        else if (you.can_see(victim))
        {
            mprf("%s %s for a moment.",
                 victim->name(DESC_THE).c_str(),
                 victim->conj_verb("distort").c_str());
        }
        return SPRET_SUCCESS;
    }

    fail_check();

    for (monster_iterator mi; mi; ++mi)
        if (mi->type == MONS_SINGULARITY && mi->summoner == agent->mid)
        {
            simple_monster_message(*mi, " implodes!");
            monster_die(*mi, KILL_RESET, NON_MONSTER);
        }

    monster* singularity = create_monster(
                                mgen_data(MONS_SINGULARITY,
                                          agent->is_player()
                                          ? BEH_FRIENDLY
                                          : SAME_ATTITUDE(agent->as_monster()),
                                          agent,
                                          // It's summoned, but it uses
                                          // its own mechanic to time out.
                                          0, SPELL_SINGULARITY,
                                          where, MHITNOT, MG_FORCE_PLACE,
                                          GOD_NO_GOD, MONS_NO_MONSTER,
                                          pow / 20, COLOUR_INHERIT,
                                          PROX_ANYWHERE,
                                          level_id::current(),
                                          (pow / 10) + 1));

    if (singularity)
    {
        if (you.can_see(singularity))
        {
            const bool friendly = singularity->wont_attack();
            mprf("Space collapses on itself with a %s crunch%s",
                 friendly ? "satisfying" : "horrifying",
                 friendly ? "." : "!");
        }
        invalidate_agrid(true);
    }
    else
        canned_msg(MSG_NOTHING_HAPPENS);

    return SPRET_SUCCESS;
}

#define GRAVITY "by gravitational forces"

void attract_actor(const actor* agent, actor* victim, const coord_def pos,
                   int pow, int strength)
{
    ray_def ray;
    if (!find_ray(victim->pos(), pos, ray, opc_solid))
    {
        // This probably shouldn't ever happen, but just in case:
        if (you.can_see(victim))
        {
            mprf("%s violently %s moving!",
                 victim->name(DESC_THE).c_str(),
                 victim->conj_verb("stop").c_str());
        }
        victim->hurt(agent, roll_dice(strength / 2, pow / 20),
                     BEAM_MMISSILE, KILLED_BY_BEAM, "", GRAVITY);
        return;
    }

    for (int i = 0; i < strength; i++)
    {
        ray.advance();
        const coord_def newpos = ray.pos();

        if (!victim->can_pass_through_feat(grd(newpos)))
        {
            victim->collide(newpos, agent, pow);
            break;
        }
        else if (actor* act_at_space = actor_at(newpos))
        {
            if (victim != act_at_space
                && act_at_space->type != MONS_SINGULARITY)
            {
                victim->collide(newpos, agent, pow);
            }
            break;
        }
        else
            victim->move_to_pos(newpos, false);
    }
}

void singularity_pull(const monster *singularity)
{
    actor *agent = actor_by_mid(singularity->summoner);

    for (actor_near_iterator ai(singularity->pos(), LOS_NO_TRANS); ai; ++ai)
    {
        if (*ai == singularity
            || agent && mons_aligned(*ai, agent))
        {
            continue;
        }

        const int range = isqrt((singularity->pos() - ai->pos()).abs());
        const int strength =
            min(4, (singularity->get_hit_dice()) / (range*range));
        if (strength <= 0)
            continue;

        static const char *messages[] =
        {
            "%s pulls at %s.",
            "%s crushes %s!",
            "%s violently warps %s!",
            "%s twists %s apart!",
        };

        if (ai->is_monster())
            behaviour_event(ai->as_monster(), ME_ANNOY, singularity);

        if (you.can_see(*ai))
        {
            // Note that we don't care if you see the singularity if
            // you can see its impact on the monster; "Something
            // violently warps Sigmund!" is perfectly acceptable,
            // after all.
            mprf(messages[strength - 1],
                 singularity->name(DESC_THE).c_str(),
                 ai->name(DESC_THE).c_str());
        }
        ai->hurt(singularity, roll_dice(strength, 12), BEAM_MMISSILE,
                 KILLED_BY_BEAM, "", GRAVITY);

        if (ai->alive() && !ai->is_stationary())
        {
            attract_actor(singularity, *ai, singularity->pos(),
                          10 * singularity->get_hit_dice(), strength);
        }
    }
}

bool fatal_attraction(actor *victim, actor *agent, int pow)
{
    bool affected = false;
    for (actor_near_iterator ai(victim->pos(), LOS_NO_TRANS); ai; ++ai)
    {
        if (*ai == victim || *ai == agent || ai->is_stationary())
            continue;

        const int range = isqrt((victim->pos() - ai->pos()).abs());
        const int strength =
            min(4, (pow / 10) / (range*range));
        if (strength <= 0)
            continue;

        affected = true;
        attract_actor(agent, *ai, victim->pos(), pow, strength);

        if (ai->alive() && ai->check_res_magic(pow / 2) <= 0)
            ai->confuse(agent, 1 + random2avg(pow / 10, 2));
    }

    return affected;
}
