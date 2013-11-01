/**
 * @file
 * @brief Translocation spells.
**/

#include "AppHdr.h"

#include <cmath>
#include <vector>
#include <algorithm>

#include "spl-transloc.h"
#include "externs.h"

#include "abyss.h"
#include "act-iter.h"
#include "areas.h"
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
#include "libutil.h"
#include "losglobal.h"
#include "message.h"
#include "misc.h"
#include "mon-behv.h"
#include "mon-util.h"
#include "mon-stuff.h"
#include "orb.h"
#include "random.h"
#include "shout.h"
#include "spl-util.h"
#include "stash.h"
#include "state.h"
#include "stuff.h"
#include "teleport.h"
#include "terrain.h"
#include "throw.h"
#include "transform.h"
#include "traps.h"
#include "view.h"
#include "viewmap.h"
#include "xom.h"

static bool _abyss_blocks_teleport(bool cblink)
{
    // Controlled Blink (the spell) works more reliably in the Abyss.
    return (cblink ? coinflip() : !one_chance_in(3));
}

// XXX: can miscast before cancelling.
spret_type cast_controlled_blink(int pow, bool fail)
{
    fail_check();
    if (blink(pow, true) == -1)
        return SPRET_ABORT;
    return SPRET_SUCCESS;
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
        for (vector<monster*>::iterator mitr = mvec.begin();
            mitr != mvec.end(); mitr++)
        {
            monster* mons = *mitr;
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

// If wizard_blink is set, all restriction are ignored (except for
// a monster being at the target spot), and the player gains no
// contamination.
int blink(int pow, bool high_level_controlled_blink, bool wizard_blink,
          string *pre_msg)
{
    ASSERT(!crawl_state.game_is_arena());

    dist beam;

    if (crawl_state.is_repeating_cmd())
    {
        crawl_state.cant_cmd_repeat("You can't repeat controlled blinks.");
        crawl_state.cancel_cmd_again();
        crawl_state.cancel_cmd_repeat();
        return -1;
    }

    // yes, there is a logic to this ordering {dlb}:
    if (you.no_tele(true, true, true) && !wizard_blink)
    {
        if (pre_msg)
            mpr(pre_msg->c_str());
        canned_msg(MSG_STRANGE_STASIS);
    }
    else if (player_in_branch(BRANCH_ABYSS)
             && _abyss_blocks_teleport(high_level_controlled_blink)
             && !wizard_blink)
    {
        if (pre_msg)
            mpr(pre_msg->c_str());
        mpr("The power of the Abyss keeps you in your place!");
    }
    else if (you.confused() && !wizard_blink)
    {
        if (pre_msg)
            mpr(pre_msg->c_str());
        random_blink(false);
    }
    // The orb sometimes degrades controlled blinks to completely uncontrolled.
    else if (orb_haloed(you.pos()) && !wizard_blink)
    {
        if (pre_msg)
            mpr(pre_msg->c_str());
        mpr("The orb interferes with your control of the blink!", MSGCH_ORB);
        // abort still wastes the turn
        if (high_level_controlled_blink && coinflip())
            return (cast_semi_controlled_blink(pow, false, false) ? 1 : 0);
        random_blink(false);
    }
    else if (!allow_control_teleport(true) && !wizard_blink)
    {
        if (pre_msg)
            mpr(pre_msg->c_str());
        mpr("A powerful magic interferes with your control of the blink.");
        // FIXME: cancel shouldn't waste a turn here -- need to rework Abyss handling
        if (high_level_controlled_blink)
            return (cast_semi_controlled_blink(pow, false/*true*/, false) ? 1 : -1);
        random_blink(false);
    }
    else
    {
        // query for location {dlb}:
        while (1)
        {
            direction_chooser_args args;
            args.restricts = DIR_TARGET;
            args.needs_path = false;
            args.may_target_monster = false;
            args.top_prompt = "Blink to where?";
            direction(beam, args);

            if (crawl_state.seen_hups)
            {
                mpr("Cancelling blink due to HUP.");
                return -1;
            }

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
                return -1;         // early return {dlb}
            }

            monster* beholder = you.get_beholder(beam.target);
            if (!wizard_blink && beholder)
            {
                mprf("You cannot blink away from %s!",
                    beholder->name(DESC_THE, true).c_str());
                continue;
            }

            monster* fearmonger = you.get_fearmonger(beam.target);
            if (!wizard_blink && fearmonger)
            {
                mprf("You cannot blink closer to %s!",
                    fearmonger->name(DESC_THE, true).c_str());
                continue;
            }

            if (grd(beam.target) == DNGN_OPEN_SEA)
            {
                mesclr();
                mpr("You can't blink into the sea!");
            }
            else if (grd(beam.target) == DNGN_LAVA_SEA)
            {
                mesclr();
                mpr("You can't blink into the sea of lava!");
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
                mpr("There's something in the way!");
            }
            else
            {
                mesclr();
                mpr("You can only blink to visible locations.");
            }
        }

        if (!you.attempt_escape(2))
            return false;

        if (pre_msg)
            mpr(pre_msg->c_str());

        // Allow wizard blink to send player into walls, in case the
        // user wants to alter that grid to something else.
        if (wizard_blink && cell_is_solid(beam.target))
            grd(beam.target) = DNGN_FLOOR;

        if (cell_is_solid(beam.target) || monster_at(beam.target))
        {
            mpr("Oops! Maybe something was there already.");
            random_blink(false);
        }
        else if (player_in_branch(BRANCH_ABYSS) && !wizard_blink)
        {
            abyss_teleport(false);
            if (you.pet_target != MHITYOU)
                you.pet_target = MHITNOT;
        }
        else
        {
            // Leave a purple cloud.
            if (!wizard_blink)
                place_cloud(CLOUD_TLOC_ENERGY, you.pos(), 1 + random2(3), &you);

            move_player_to_grid(beam.target, false, true);

            // Controlling teleport contaminates the player. -- bwr
            if (!wizard_blink)
                contaminate_player(1000, true);
        }
    }

    crawl_state.cancel_cmd_again();
    crawl_state.cancel_cmd_repeat();

    return 1;
}

spret_type cast_blink(bool allow_partial_control, bool fail)
{
    fail_check();
    random_blink(allow_partial_control);
    return SPRET_SUCCESS;
}

void random_blink(bool allow_partial_control, bool override_abyss, bool override_stasis)
{
    ASSERT(!crawl_state.game_is_arena());

    coord_def target;

    if (you.no_tele(true, true, true) && !override_stasis)
        canned_msg(MSG_STRANGE_STASIS);
    else if (player_in_branch(BRANCH_ABYSS)
             && !override_abyss
             && _abyss_blocks_teleport(false))
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

    //jmf: Add back control, but effect is cast_semi_controlled_blink(pow).
    else if (player_control_teleport() && !you.confused() && allow_partial_control
             && allow_control_teleport())
    {
        mpr("You may select the general direction of your translocation.");
        // FIXME: handle aborts here, don't waste the turn
        cast_semi_controlled_blink(100, false, true);
    }
    else if (you.attempt_escape(2))
    {
        canned_msg(MSG_YOU_BLINK);
        coord_def origin = you.pos();
        move_player_to_grid(target, false, true);

        // Leave a purple cloud.
        if (!cell_is_solid(origin))
            place_cloud(CLOUD_TLOC_ENERGY, origin, 1 + random2(3), &you);
    }
}

// This function returns true if the player can use controlled teleport
// here.
bool allow_control_teleport(bool quiet)
{
    const bool retval = !(testbits(env.level_flags, LFLAG_NO_TELE_CONTROL)
                          || orb_haloed(you.pos()) || you.beheld());

    // Tell the player why if they have teleport control.
    if (!quiet && !retval && player_control_teleport())
    {
        if (orb_haloed(you.pos()))
            mpr("The orb prevents control of your teleportation!", MSGCH_ORB);
        else if (you.beheld())
            mpr("It is impossible to concentrate on your destination whilst mesmerised.");
        else
            mpr("A powerful magic prevents control of your teleportation.");
    }

    return retval;
}

spret_type cast_teleport_self(bool fail)
{
    fail_check();
    you_teleport();
    return SPRET_SUCCESS;
}

void you_teleport(void)
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
            mpr("You feel the orb delaying this translocation!", MSGCH_ORB);
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

        handle_interrupted_swap(true);
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

static bool _teleport_player(bool allow_control, bool new_abyss_area,
                             bool wizard_tele, int range)
{
    bool is_controlled = (allow_control && !you.confused()
                          && player_control_teleport()
                          && allow_control_teleport()
                          && !you.berserk());

    // All wizard teleports are automatically controlled.
    if (wizard_tele)
        is_controlled = true;

    // Stasis can't block the Abyss from shifting.
    if (!wizard_tele
        && (crawl_state.game_is_sprint() || you.no_tele(true, true))
            && !new_abyss_area)
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
        abyss_teleport(new_abyss_area);
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
                mpr("Controlled teleport interrupted by HUP signal, "
                    "cancelling teleport.", MSGCH_ERROR);
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
                    mpr("Even you can't go there right now. Sorry!", MSGCH_WARN);
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
                mpr("A strong magical force throws you back!", MSGCH_WARN);
            }
            else
            {
                // Leave a purple cloud.
                if (!wizard_tele && !cell_is_solid(old_pos))
                    place_cloud(CLOUD_TLOC_ENERGY, old_pos, 1 + random2(3), &you);

                move_player_to_grid(pos, false, true);

                // Controlling teleport contaminates the player. - bwr
                if (!wizard_tele)
                    contaminate_player(1000, true);
            }
            // End teleport control.
            if (you.duration[DUR_CONTROL_TELEPORT])
            {
                mpr("You feel uncertain.", MSGCH_DURATION);
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
            newpos = random_in_bounds();
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
        if (!cell_is_solid(old_pos))
            place_cloud(CLOUD_TLOC_ENERGY, old_pos, 1 + random2(3), &you);

        move_player_to_grid(newpos, false, true);
    }

    _handle_teleport_update(large_change, old_pos);
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
    // Leave a purple cloud.
    if (!cell_is_solid(old_pos))
        place_cloud(CLOUD_TLOC_ENERGY, old_pos, 1 + random2(3), &you);

    bool large_change = you.see_cell(where);

    move_player_to_grid(where, false, true);

    _handle_teleport_update(large_change, old_pos);
    return true;
}

void you_teleport_now(bool allow_control, bool new_abyss_area,
                      bool wizard_tele, int range)
{
    const bool randtele = _teleport_player(allow_control, new_abyss_area,
                                           wizard_tele, range);

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
    dist target;
    int item = get_ammo_to_shoot(-1, target, true);
    if (item == -1)
        return SPRET_ABORT;

    if (cell_is_solid(target.target))
    {
        mpr("You can't shoot at gazebos.");
        return SPRET_ABORT;
    }

    // Can't use portal through walls. (That'd be just too cheap!)
    if (you.trans_wall_blocking(target.target))
    {
        mpr("There's something in the way!");
        return SPRET_ABORT;
    }

    if (!check_warning_inscriptions(you.inv[item], OPER_FIRE))
        return SPRET_ABORT;

    fail_check();
    bolt beam;
    throw_it(beam, item, true, random2(pow/4), &target);

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

    // Can't apport the Orb in zotdef or sprint
    if (item_is_orb(item)
        && (crawl_state.game_is_zotdef()
            || crawl_state.game_is_sprint()))
    {
        mpr("You cannot apport the Orb!");
        return SPRET_ABORT;
    }

    fail_check();
    // Mass of one unit.
    const int unit_mass = item_mass(item);
    const int max_mass = pow * 30 + random2(pow * 20);

    int max_units = item.quantity;
    if (unit_mass > 0)
        max_units = max_mass / unit_mass;

    if (max_units <= 0)
    {
        if (item_is_orb(item))
            orb_pickup_noise(where, 30);

        mpr("The mass is resisting your pull.");

        return SPRET_SUCCESS;
    }

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
    if (item.base_type == OBJ_MISSILES
        && item.sub_type == MI_THROWING_NET
        && item_is_stationary(item))
    {
        remove_item_stationary(item);
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
    int quantity = item.quantity;
    int apported_mass = unit_mass * min(quantity, max_units);

    int max_dist = max(60 * pow / (apported_mass + 150), 1);

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

    if (max_units < item.quantity)
    {
        if (!copy_item_to_grid(item, new_spot, max_units))
        {
            // Always >1 item.
            mpr("They abruptly stop in place!");
            // Too late to abort.
            return SPRET_SUCCESS;
        }
        item.quantity -= max_units;
    }
    else
        move_top_item(where, new_spot);

    // Mark the item as found now.
    origin_set(new_spot);

    return SPRET_SUCCESS;
}

static bool _quadrant_blink(coord_def dir, int pow)
{
    if (pow > 100)
        pow = 100;

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
        if (!random_near_space(base, target)
            && !random_near_space(base, target, true))
        {
            // Uh oh, WHY should this fail the blink?
            return false;
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
    {
        // We've already succeeded at blinking, so the Abyss shouldn't block it.
        random_blink(false, true);
        return true;
    }

    coord_def origin = you.pos();
    move_player_to_grid(target, false, true);

    // Leave a purple cloud.
    if (!cell_is_solid(origin))
        place_cloud(CLOUD_TLOC_ENERGY, origin, 1 + random2(3), &you);

    return true;
}

spret_type cast_semi_controlled_blink(int pow, bool cheap_cancel, bool end_ctele, bool fail)
{
    dist bmove;
    direction_chooser_args args;
    args.restricts = DIR_DIR;
    args.mode = TARG_ANY;

    while (1)
    {
        mpr("Which direction? [ESC to cancel]", MSGCH_PROMPT);
        direction(bmove, args);

        if (crawl_state.seen_hups)
        {
            mpr("Cancelling blink due to HUP.");
            return SPRET_ABORT;
        }

        if (bmove.isValid && !bmove.delta.origin())
            break;

        if (cheap_cancel
            || yesno("Are you sure you want to cancel this blink?", false ,'n'))
        {
            canned_msg(MSG_OK);
            return SPRET_ABORT;
        }
    }

    fail_check();

    // Note: this can silently fail, eating the blink -- WHY?
    if (you.attempt_escape(2) && _quadrant_blink(bmove.delta, pow))
    {
        // Controlled blink causes glowing.
        contaminate_player(1000, true);
        // End teleport control if this was a random blink upgraded by cTele.
        if (end_ctele && you.duration[DUR_CONTROL_TELEPORT])
        {
            mpr("You feel uncertain.", MSGCH_DURATION);
            you.duration[DUR_CONTROL_TELEPORT] = 0;
        }
    }

    return SPRET_SUCCESS;
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
