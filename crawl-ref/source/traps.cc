/*
 *  File:       traps.cc
 *  Summary:    Traps related functions.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 */

#include "AppHdr.h"
REVISION("$Rev$");

#include "externs.h"
#include "traps.h"

#include <algorithm>

#include "beam.h"
#include "branch.h"
#include "delay.h"
#include "describe.h"
#include "directn.h"
#include "it_use2.h"
#include "itemname.h"
#include "itemprop.h"
#include "items.h"
#include "makeitem.h"
#include "message.h"
#include "misc.h"
#include "mon-util.h"
#include "monstuff.h"
#include "mtransit.h"
#include "ouch.h"
#include "place.h"
#include "player.h"
#include "randart.h"
#include "skills.h"
#include "spells3.h"
#include "spl-mis.h"
#include "spl-util.h"
#include "state.h"
#include "stuff.h"
#include "terrain.h"
#include "transfor.h"
#include "tutorial.h"
#include "view.h"
#include "xom.h"

bool trap_def::active() const
{
    return (this->type != TRAP_UNASSIGNED);
}

bool trap_def::type_has_ammo() const
{
    switch (this->type)
    {
    case TRAP_DART:   case TRAP_ARROW:  case TRAP_BOLT:
    case TRAP_NEEDLE: case TRAP_SPEAR:  case TRAP_AXE:
        return (true);
    default:
        break;
    }
    return (false);
}

void trap_def::message_trap_entry()
{
    if (this->type == TRAP_TELEPORT)
        mpr("You enter a teleport trap!");
}

void trap_def::disarm()
{
    if (this->type_has_ammo() && this->ammo_qty > 0)
    {
        item_def trap_item = this->generate_trap_item();
        trap_item.quantity = this->ammo_qty;
        copy_item_to_grid(trap_item, this->pos);
    }
    this->destroy();
}

void trap_def::destroy()
{
    if (!in_bounds(this->pos))
        ASSERT("trap position out of bounds!");

    grd(this->pos) = DNGN_FLOOR;
    this->ammo_qty = 0;
    this->pos = coord_def(-1,-1);
    this->type = TRAP_UNASSIGNED;
}

void trap_def::hide()
{
    grd(this->pos) = DNGN_UNDISCOVERED_TRAP;
}

void trap_def::prepare_ammo()
{
    switch (this->type)
    {
    case TRAP_DART:
    case TRAP_ARROW:
    case TRAP_BOLT:
    case TRAP_NEEDLE:
        this->ammo_qty = 3 + random2avg(9, 3);
        break;
    case TRAP_SPEAR:
    case TRAP_AXE:
        this->ammo_qty = 2 + random2avg(6, 3);
        break;
    default:
        this->ammo_qty = 0;
        break;
    }
}

void trap_def::reveal()
{
    const dungeon_feature_type cat = this->category();
    grd(this->pos) = cat;
    set_envmap_obj(this->pos, cat);
}

std::string trap_def::name(description_level_type desc) const
{
    if (this->type >= NUM_TRAPS)
        return ("buggy");

    const char* basename = trap_name(this->type);
    if (desc == DESC_CAP_A || desc == DESC_NOCAP_A)
    {
        std::string prefix = (desc == DESC_CAP_A ? "A" : "a");
        if (is_vowel(basename[0]))
            prefix += 'n';
        prefix += ' ';
        return (prefix + basename);
    }
    else if (desc == DESC_CAP_THE)
        return (std::string("The ") + basename);
    else if (desc == DESC_NOCAP_THE)
        return (std::string("the ") + basename);
    else                        // everything else
        return (basename);
}

bool trap_def::is_known(const actor* act) const
{
    bool rc = false;
    const bool player_knows = (grd(pos) != DNGN_UNDISCOVERED_TRAP);

    if (act == NULL || act->atype() == ACT_PLAYER)
        rc = player_knows;
    else if (act->atype() == ACT_MONSTER)
    {
        const monsters* monster = dynamic_cast<const monsters*>(act);
        const int intel = mons_intel(monster);

        // Smarter trap handling for intelligent monsters
        // * monsters native to a branch can be assumed to know the trap
        //   locations and thus be able to avoid them
        // * friendlies and good neutrals can be assumed to have been warned
        //   by the player about all traps s/he knows about
        // * very intelligent monsters can be assumed to have a high T&D
        //   skill (or have memorised part of the dungeon layout ;) )

        rc = (intel >= I_NORMAL
              && (mons_is_native_in_branch(monster)
                  || mons_wont_attack(monster) && player_knows
                  || intel >= I_HIGH && one_chance_in(3)));
    }
    return (rc);
}


// Returns the number of a net on a given square.
// If trapped only stationary ones are counted
// otherwise the first net found is returned.
int get_trapping_net(const coord_def& where, bool trapped)
{
    for (stack_iterator si(where); si; ++si)
    {
         if (si->base_type == OBJ_MISSILES
             && si->sub_type == MI_THROWING_NET
             && (!trapped || item_is_stationary(*si)))
         {
             return (si->index());
         }
    }
    return (NON_ITEM);
}

// If there are more than one net on this square
// split off one of them for checking/setting values.
static void maybe_split_nets(item_def &item, const coord_def& where)
{
    if (item.quantity == 1)
    {
        set_item_stationary(item);
        return;
    }

    item_def it;

    it.base_type = item.base_type;
    it.sub_type  = item.sub_type;
    it.plus      = item.plus;
    it.plus2     = item.plus2;
    it.flags     = item.flags;
    it.special   = item.special;
    it.quantity  = --item.quantity;
    item_colour(it);

    item.quantity = 1;
    set_item_stationary(item);

    copy_item_to_grid( it, where );
}

void mark_net_trapping(const coord_def& where)
{
    int net = get_trapping_net(where);
    if (net == NON_ITEM)
    {
        net = get_trapping_net(where, false);
        if (net != NON_ITEM)
            maybe_split_nets(mitm[net], where);
    }
}

void monster_caught_in_net(monsters *mon, bolt &pbolt)
{
    if (mon->body_size(PSIZE_BODY) >= SIZE_GIANT)
        return;

    if (mons_is_insubstantial(mon->type))
    {
        if (you.can_see(mon))
        {
            mprf("The net passes right through %s!",
                 mon->name(DESC_NOCAP_THE).c_str());
        }
        return;
    }

    bool mon_flies = mon->flight_mode() == FL_FLY;
    if (mon_flies && (!mons_is_confused(mon) || one_chance_in(3)))
    {
        simple_monster_message(mon, " darts out from under the net!");
        return;
    }

    if (mon->type == MONS_OOZE || mon->type == MONS_PULSATING_LUMP)
    {
        simple_monster_message(mon, " oozes right through the net!");
        return;
    }

    if (!mons_is_caught(mon) && mon->add_ench(ENCH_HELD))
    {
        if (mons_near(mon) && !player_monster_visible(mon))
            mpr("Something gets caught in the net!");
        else
            simple_monster_message(mon, " is caught in the net!");

        if (mon_flies)
        {
            simple_monster_message(mon, " falls like a stone!");
            mons_check_pool(mon, mon->pos(), pbolt.killer(), pbolt.beam_source);
        }
    }
}

bool player_caught_in_net()
{
    if (you.body_size(PSIZE_BODY) >= SIZE_GIANT)
        return (false);

    if (you.flight_mode() == FL_FLY && (!you.confused() || one_chance_in(3)))
    {
        mpr("You dart out from under the net!");
        return (false);
    }

    if (!you.attribute[ATTR_HELD])
    {
        you.attribute[ATTR_HELD] = 10;
        mpr("You become entangled in the net!");
        stop_running();

        // I guess levitation works differently, keeping both you
        // and the net hovering above the floor
        if (you.flight_mode() == FL_FLY)
        {
            mpr("You fall like a stone!");
            fall_into_a_pool(you.pos(), false, grd(you.pos()));
        }

        stop_delay(true); // even stair delays
        return (true);
    }
    return (false);
}

void check_net_will_hold_monster(monsters *mons)
{
    if (mons->body_size(PSIZE_BODY) >= SIZE_GIANT)
    {
        int net = get_trapping_net(mons->pos());
        if (net != NON_ITEM)
            destroy_item(net);

        if (see_grid(mons->pos()))
        {
            if (player_monster_visible(mons))
            {
                mprf("The net rips apart, and %s comes free!",
                     mons->name(DESC_NOCAP_THE).c_str());
            }
            else
                mpr("All of a sudden the net rips apart!");
        }
    }
    else if (mons_is_insubstantial(mons->type)
             || mons->type == MONS_OOZE
             || mons->type == MONS_PULSATING_LUMP)
    {
        const int net = get_trapping_net(mons->pos());
        if (net != NON_ITEM)
            remove_item_stationary(mitm[net]);

        if (mons_is_insubstantial(mons->type))
        {
            simple_monster_message(mons,
                                   " drifts right through the net!");
        }
        else
        {
            simple_monster_message(mons,
                                   " oozes right through the net!");
        }
    }
    else
        mons->add_ench(ENCH_HELD);
}

void trap_def::trigger(actor& triggerer, bool flat_footed)
{
    const bool you_know = this->is_known();
    const bool trig_knows = !flat_footed && this->is_known(&triggerer);

    const bool you_trigger = (triggerer.atype() == ACT_PLAYER);
    const bool in_sight = see_grid(this->pos);

    // If set, the trap will be removed at the end of the
    // triggering process.
    bool trap_destroyed = false;

    monsters* m = NULL;
    if (triggerer.atype() == ACT_MONSTER)
        m = dynamic_cast<monsters*>(&triggerer);

    // Anything stepping onto a trap almost always reveals it.
    // (We can rehide it later for the exceptions.)
    if (in_sight)
        this->reveal();

    // Only magical traps affect flying critters.
    if (triggerer.airborne() && this->category() != DNGN_TRAP_MAGICAL)
    {
        if (you_know && m)
            simple_monster_message(m, " flies safely over a trap.");
        return;
    }

    // OK, something is going to happen.
    if (you_trigger)
        this->message_trap_entry();

    // Store the position now in case it gets cleared inbetween.
    const coord_def p(this->pos);

    if (this->type_has_ammo())
        this->shoot_ammo(triggerer, trig_knows);
    else switch (this->type)
    {
    case TRAP_TELEPORT:
        // Never revealed by monsters.
        if (!you_trigger && !you_know)
            this->hide();
        triggerer.teleport(true);
        break;

    case TRAP_ALARM:
        if (silenced(this->pos))
        {
            if (you_know && in_sight)
                mpr("The alarm trap is silent.");

            // If it's silent, you don't know about it.
            if (!you_know)
                this->hide();
        }
        else if (!(m && mons_friendly(m)))
        {
            // Alarm traps aren't set off by hostile monsters, because
            // that would be way too nasty for the player.
            const char* message_here = "An alarm trap emits a blaring wail!";
            const char* message_near = "You hear a blaring wail!";
            const char* message_far  = "You hear a distant blaring wail!";
            noisy(12, this->pos, (you_trigger ? message_here :
                                  (in_sight ? message_near : message_far)));
        }
        break;

    case TRAP_BLADE:
        if (you_trigger)
        {
            if (trig_knows && one_chance_in(3))
                mpr("You avoid triggering a blade trap.");
            else if (random2limit(player_evasion(), 40)
                     + (random2(you.dex) / 3) + (trig_knows ? 3 : 0) > 8)
            {
                mpr("A huge blade swings just past you!");
            }
            else
            {
                mpr("A huge blade swings out and slices into you!");
                const int damage = (you.your_level * 2) + random2avg(29, 2)
                    - random2(1 + player_AC());
                ouch(damage, NON_MONSTER, KILLED_BY_TRAP, "blade");
                bleed_onto_floor(you.pos(), -1, damage, true);
            }
        }
        else if (m)
        {
            if (one_chance_in(5) || (trig_knows && coinflip()))
            {
                // Trap doesn't trigger. Don't reveal it.
                if (you_know)
                {
                    simple_monster_message(m,
                                           " fails to trigger a blade trap.");
                }
                else
                    this->hide();
            }
            else if (random2(m->ev) > 8 || (trig_knows && random2(m->ev) > 8))
            {
                if (in_sight &&
                    !simple_monster_message(m,
                                            " avoids a huge, swinging blade."))
                {
                    mpr("A huge blade swings out!");
                }
            }
            else
            {
                if (in_sight)
                {
                    std::string msg = "A huge blade swings out";
                    if (player_monster_visible( m ))
                    {
                        msg += " and slices into ";
                        msg += m->name(DESC_NOCAP_THE);
                    }
                    msg += "!";
                    mpr(msg.c_str());
                }

                int damage_taken = 10 + random2avg(29, 2) - random2(1 + m->ac);

                if (damage_taken < 0)
                    damage_taken = 0;

                if (!mons_is_summoned(m))
                    bleed_onto_floor(m->pos(), m->type, damage_taken, true);

                m->hurt(NULL, damage_taken);
                if (in_sight && m->alive())
                    print_wounds(m);
            }
        }
        break;

    case TRAP_NET:
        if (you_trigger)
        {
            if (trig_knows && one_chance_in(3))
                mpr("A net swings high above you.");
            else
            {
                if (random2limit(player_evasion(), 40)
                    + (random2(you.dex) / 3) + (trig_knows ? 3 : 0) > 12)
                {
                    mpr("A net drops to the ground!");
                }
                else
                {
                    mpr("A large net falls onto you!");
                    if (player_caught_in_net() && player_in_a_dangerous_place())
                        xom_is_stimulated(64);
                }

                item_def item = this->generate_trap_item();
                copy_item_to_grid(item, triggerer.pos());

                if (you.attribute[ATTR_HELD])
                    mark_net_trapping(you.pos());

                trap_destroyed = true;
            }
        }
        else if (m)
        {
            bool triggered = false;
            if (one_chance_in(3) || (trig_knows && coinflip()))
            {
                // Not triggered, trap stays.
                triggered = false;
                if (you_know)
                    simple_monster_message(m, " fails to trigger a net trap.");
                else
                    this->hide();
            }
            else if (random2(m->ev) > 8 || (trig_knows && random2(m->ev) > 8))
            {
                // Triggered but evaded.
                triggered = true;

                if (in_sight)
                {
                    if (!simple_monster_message(m,
                                                " nimbly jumps out of the way "
                                                "of a falling net."))
                    {
                        mpr("A large net falls down!");
                    }
                }
            }
            else
            {
                // Triggered and hit.
                triggered = true;

                if (in_sight)
                {
                    msg::stream << "A large net falls down";
                    if (player_monster_visible(m))
                        msg::stream << " onto " << m->name(DESC_NOCAP_THE);
                    msg::stream << "!" << std::endl;
                }
                // FIXME: Fake a beam for monster_caught_in_net().
                bolt beam;
                beam.flavour = BEAM_MISSILE;
                beam.thrower = KILL_MISC;
                beam.beam_source = NON_MONSTER;
                monster_caught_in_net(m, beam);
            }

            if (triggered)
            {
                item_def item = this->generate_trap_item();
                copy_item_to_grid(item, triggerer.pos());

                if (mons_is_caught(m))
                    mark_net_trapping(m->pos());

                trap_destroyed = true;
            }
        }
        break;

    case TRAP_ZOT:
        if (you_trigger)
        {
            mpr((trig_knows) ? "You enter the Zot trap."
                             : "Oh no! You have blundered into a Zot trap!");
            if (!trig_knows)
                xom_is_stimulated(32);

            MiscastEffect( &you, ZOT_TRAP_MISCAST, SPTYP_RANDOM,
                           3, "a Zot trap" );
        }
        else if (m)
        {
            // Zot traps are out to get *the player*! Hostile monsters
            // benefit and friendly monsters suffer. Such is life.

            // Preserving original functionality: don't reveal location.
            if (!you_know)
                this->hide();

            if (mons_wont_attack(m) || crawl_state.arena)
            {
                MiscastEffect( m, ZOT_TRAP_MISCAST, SPTYP_RANDOM,
                               3, "the power of Zot" );
            }
            else if (in_sight && one_chance_in(5))
            {
                mpr("The power of Zot is invoked against you!");
                MiscastEffect( &you, ZOT_TRAP_MISCAST, SPTYP_RANDOM,
                               3, "the power of Zot" );
            }
            else if (player_can_hear(this->pos))
            {
                mprf(MSGCH_SOUND, "You hear a %s \"Zot\"!",
                     in_sight ? "loud" : "distant");
            }
        }
        break;

    case TRAP_SHAFT:
        // Paranoia
        if (!is_valid_shaft_level())
        {
            if (you_know && in_sight)
                mpr("The shaft disappears in a puff of logic!");

            trap_destroyed = true;
            break;
        }

        // Known shafts don't trigger as traps.
        if (trig_knows)
        {
            if (you_trigger)
            {
               if (triggerer.airborne())
               {
                   if (you.flight_mode() == FL_LEVITATE)
                       mpr("You float over the shaft.");
                   else
                       mpr("You fly over the shaft.");
               }
               else
                   mpr("You carefully avoid triggering the shaft.");
            }
            break;
        }

        if (!triggerer.will_trigger_shaft())
        {
            if (!you_know)
                this->hide();
            else if (!triggerer.airborne())
            {
                if (you_trigger)
                {
                    mpr("You don't fall through the shaft.");
                }
                else if (m)
                {
                    simple_monster_message(m,
                                           " doesn't fall through the shaft.");
                }
            }
        }

        // Fire away!
        {
            const bool revealed = triggerer.do_shaft();
            if (!revealed && !you_know)
                this->hide();
        }
        break;

    default:
        break;
    }

    if (you_trigger)
    {
        learned_something_new(TUT_SEEN_TRAP, p);

        // Exercise T&D if the trap revealed itself, but not if it ran
        // out of ammo.
        if (!you_know && this->type != TRAP_UNASSIGNED && this->is_known())
            exercise(SK_TRAPS_DOORS, ((coinflip()) ? 2 : 1));
    }

    if (trap_destroyed)
        this->destroy();
}

int trap_def::shot_damage(actor& act)
{
    if (act.atype() == ACT_PLAYER)
    {
        switch (this->type)
        {
        case TRAP_NEEDLE: return 0;
        case TRAP_DART:   return random2( 4 + you.your_level/2) + 1;
        case TRAP_ARROW:  return random2( 7 + you.your_level)   + 1;
        case TRAP_SPEAR:  return random2(10 + you.your_level)   + 1;
        case TRAP_BOLT:   return random2(13 + you.your_level)   + 1;
        case TRAP_AXE:    return random2(15 + you.your_level)   + 1;
        default:          return 0;
        }
    }
    else if (act.atype() == ACT_MONSTER)
    {
        // Trap damage to monsters is not a function of level, because
        // they are fairly stupid and tend to have fewer hp than
        // players -- this choice prevents traps from easily killing
        // large monsters fairly deep within the dungeon.
        switch (this->type)
        {
        case TRAP_NEEDLE: return 0;
        case TRAP_DART:   return random2( 4) + 1;
        case TRAP_ARROW:  return random2( 7) + 1;
        case TRAP_SPEAR:  return random2(10) + 1;
        case TRAP_BOLT:   return random2(13) + 1;
        case TRAP_AXE:    return random2(15) + 1;
        default:          return 0;
        }
    }
    return (0);
}

void destroy_trap( const coord_def& pos )
{
    if (trap_def* ptrap = find_trap(pos))
        ptrap->destroy();
}

trap_def* find_trap(const coord_def& pos)
{
    for (int i = 0; i < MAX_TRAPS; ++i)
        if (env.trap[i].pos == pos && env.trap[i].type != TRAP_UNASSIGNED)
            return (&env.trap[i]);
    return (NULL);
}

trap_type get_trap_type(const coord_def& pos)
{
    if (trap_def* ptrap = find_trap(pos))
        return (ptrap->type);
    return (TRAP_UNASSIGNED);
}

// where *must* point to a valid, discovered trap.
void disarm_trap(const coord_def& where)
{
    if (you.duration[DUR_BERSERKER])
    {
        canned_msg(MSG_TOO_BERSERK);
        return;
    }

    trap_def& trap = *find_trap(where);

    switch (trap.category())
    {
    case DNGN_TRAP_MAGICAL:
        mpr("You can't disarm that trap.");
        return;
    case DNGN_TRAP_NATURAL:
        // Only shafts for now.
        mpr("You can't disarm a shaft.");
        return;
    default:
        break;
    }

    // Make the actual attempt
    you.turn_is_over = true;
    if (random2(you.skills[SK_TRAPS_DOORS] + 2) <= random2(you.your_level + 5))
    {
        mpr("You failed to disarm the trap.");
        if (random2(you.dex) > 5 + random2(5 + you.your_level))
            exercise(SK_TRAPS_DOORS, 1 + random2(you.your_level / 5));
        else
        {
            if (trap.type == TRAP_NET && trap.pos != you.pos())
            {
                if (coinflip())
                {
                    mpr("You stumble into the trap!");
                    move_player_to_grid(trap.pos, true, false, true);
                }
            }
            else
                trap.trigger(you, true);

            if (coinflip())
                exercise(SK_TRAPS_DOORS, 1);
        }
    }
    else
    {
        mpr("You have disarmed the trap.");
        trap.disarm();
        exercise(SK_TRAPS_DOORS, 1 + random2(5) + (you.your_level/5));
    }
}

// Attempts to take a net off a given monster.
// This doesn't actually have any effect (yet).
// Do not expect gratitude for this!
// ----------------------------------
void remove_net_from(monsters *mon)
{
    you.turn_is_over = true;

    int net = get_trapping_net(mon->pos());

    if (net == NON_ITEM)
    {
        mon->del_ench(ENCH_HELD, true);
        return;
    }

    // factor in whether monster is paralysed or invisible
    int paralys = 0;
    if (mons_is_paralysed(mon)) // makes this easier
        paralys = random2(5);

    int invis = 0;
    if (!player_monster_visible(mon)) // makes this harder
        invis = 3 + random2(5);

    bool net_destroyed = false;
    if ( random2(you.skills[SK_TRAPS_DOORS] + 2) + paralys
           <= random2( 2*mon->body_size(PSIZE_BODY) + 3 ) + invis)
    {
        if (one_chance_in(you.skills[SK_TRAPS_DOORS] + you.dex/2))
        {
            mitm[net].plus--;
            mpr("You tear at the net.");
            if (mitm[net].plus < -7)
            {
                mpr("Whoops! The net comes apart in your hands!");
                mon->del_ench(ENCH_HELD, true);
                destroy_item(net);
                net_destroyed = true;
            }
        }

        if (!net_destroyed)
        {
            if (player_monster_visible(mon))
            {
                mprf("You fail to remove the net from %s.",
                     mon->name(DESC_NOCAP_THE).c_str());
            }
            else
                mpr("You fail to remove the net.");
        }

        if (random2(you.dex) > 5 + random2( 2*mon->body_size(PSIZE_BODY) ))
            exercise(SK_TRAPS_DOORS, 1 + random2(mon->body_size(PSIZE_BODY)/2));
        return;
    }

    mon->del_ench(ENCH_HELD, true);
    remove_item_stationary(mitm[net]);

    if (player_monster_visible(mon))
        mprf("You free %s.", mon->name(DESC_NOCAP_THE).c_str());
    else
        mpr("You loosen the net.");

}

// Decides whether you will try to tear the net (result <= 0)
// or try to slip out of it (result > 0).
// Both damage and escape could be 9 (more likely for damage)
// but are capped at 5 (damage) and 4 (escape).
static int damage_or_escape_net(int hold)
{
    // Spriggan: little (+2)
    // Halfling, Kobold: small (+1)
    // Human, Elf, ...: medium (0)
    // Ogre, Troll, Centaur, Naga: large (-1)
    // transformations: spider, bat: tiny (+3); ice beast: large (-1)
    int escape = SIZE_MEDIUM - you.body_size(PSIZE_BODY);

    int damage = -escape;

    // your weapon may damage the net, max. bonus of 2
    if (you.weapon())
    {
        if (can_cut_meat(*you.weapon()))
            damage++;

        int brand = get_weapon_brand(*you.weapon());
        if (brand == SPWPN_FLAMING || brand == SPWPN_VORPAL)
            damage++;
    }
    else if (you.attribute[ATTR_TRANSFORMATION] == TRAN_BLADE_HANDS)
        damage += 2;
    else if (you.has_usable_claws())
    {
        int level = you.has_claws();
        if (level == 1)
            damage += coinflip();
        else
            damage += level - 1;
    }

    // Berserkers get a fighting bonus.
    if (you.duration[DUR_BERSERKER])
        damage += 2;

    // Check stats.
    if (x_chance_in_y(you.strength, 18))
        damage++;
    if (x_chance_in_y(you.dex, 12))
        escape++;
    if (x_chance_in_y(player_evasion(), 20))
        escape++;

    // Dangerous monsters around you add urgency.
    if (there_are_monsters_nearby(true))
    {
        damage++;
        escape++;
    }

    // Confusion makes the whole thing somewhat harder
    // (less so for trying to escape).
    if (you.confused())
    {
        if (escape > 1)
            escape--;
        else if (damage >= 2)
            damage -= 2;
    }

    // Damaged nets are easier to destroy.
    if (hold < 0)
    {
        damage += random2(-hold/3 + 1);

        // ... and easier to slip out of (but only if escape looks feasible).
        if (you.attribute[ATTR_HELD] < 5 || escape >= damage)
            escape += random2(-hold/2) + 1;
    }

    // If undecided, choose damaging approach (it's quicker).
    if (damage >= escape)
        return (-damage); // negate value

    return (escape);
}

// Calls the above function to decide on how to get free.
// Note that usually the net will be damaged until trying to slip out
// becomes feasible (for size etc.), so it may take even longer.
void free_self_from_net()
{
    int net = get_trapping_net(you.pos());

    if (net == NON_ITEM) // really shouldn't happen!
    {
        you.attribute[ATTR_HELD] = 0;
        return;
    }

    int hold = mitm[net].plus;
    int do_what = damage_or_escape_net(hold);
#ifdef DEBUG_DIAGNOSTICS
    mprf(MSGCH_DIAGNOSTICS, "net.plus: %d, ATTR_HELD: %d, do_what: %d",
         hold, you.attribute[ATTR_HELD], do_what);
#endif

    if (do_what <= 0) // You try to destroy the net
    {
        // For previously undamaged nets this takes at least 2 and at most
        // 8 turns.
        bool can_slice =
            (you.attribute[ATTR_TRANSFORMATION] == TRAN_BLADE_HANDS)
            || (you.weapon() && can_cut_meat(*you.weapon()));

        int damage = -do_what;

        if (damage < 1)
            damage = 1;

        if (you.duration[DUR_BERSERKER])
            damage *= 2;

        // Medium sized characters are at a disadvantage and sometimes
        // get a bonus.
        if (you.body_size(PSIZE_BODY) == SIZE_MEDIUM)
            damage += coinflip();

        if (damage > 5)
            damage = 5;

        hold -= damage;
        mitm[net].plus = hold;

        if (hold < -7)
        {
            mprf("You %s the net and break free!",
                 can_slice ? (damage >= 4? "slice" : "cut") :
                             (damage >= 4? "shred" : "rip"));

            destroy_item(net);

            you.attribute[ATTR_HELD] = 0;
            return;
        }

        if (damage >= 4)
        {
            mprf("You %s into the net.",
                 can_slice? "slice" : "tear a large gash");
        }
        else
            mpr("You struggle against the net.");

        // Occasionally decrease duration a bit
        // (this is so switching from damage to escape does not hurt as much).
        if (you.attribute[ATTR_HELD] > 1 && coinflip())
        {
            you.attribute[ATTR_HELD]--;

            if (you.attribute[ATTR_HELD] > 1 && hold < -random2(5))
                you.attribute[ATTR_HELD]--;
        }
   }
   else
   {
        // You try to escape (takes at least 3 turns, and at most 10).
        unsigned int escape = do_what;

        if (you.duration[DUR_HASTE]) // extra bonus, also Berserk
            escape++;

        // Medium sized characters are at a disadvantage and sometimes
        // get a bonus.
        if (you.body_size(PSIZE_BODY) == SIZE_MEDIUM)
            escape += coinflip();

        if (escape > 4)
            escape = 4;

        if (escape >= you.attribute[ATTR_HELD])
        {
            if (escape >= 3)
                mpr("You slip out of the net!");
            else
                mpr("You break free from the net!");

            you.attribute[ATTR_HELD] = 0;
            remove_item_stationary(mitm[net]);
            return;
        }

        if (escape >= 3)
            mpr("You try to slip out of the net.");
        else
            mpr("You struggle to escape the net.");

        you.attribute[ATTR_HELD] -= escape;
   }
}

void clear_trapping_net()
{
    if (!you.attribute[ATTR_HELD])
        return;

    if (!in_bounds(you.pos()))
        return;

    const int net = get_trapping_net(you.pos());
    if (net != NON_ITEM)
        remove_item_stationary(mitm[net]);

    you.attribute[ATTR_HELD] = 0;
}

item_def trap_def::generate_trap_item()
{
    item_def item;
    object_class_type base;
    int sub;

    switch (this->type)
    {
    case TRAP_DART:   base = OBJ_MISSILES; sub = MI_DART;         break;
    case TRAP_ARROW:  base = OBJ_MISSILES; sub = MI_ARROW;        break;
    case TRAP_BOLT:   base = OBJ_MISSILES; sub = MI_BOLT;         break;
    case TRAP_SPEAR:  base = OBJ_WEAPONS;  sub = WPN_SPEAR;       break;
    case TRAP_AXE:    base = OBJ_WEAPONS;  sub = WPN_HAND_AXE;    break;
    case TRAP_NEEDLE: base = OBJ_MISSILES; sub = MI_NEEDLE;       break;
    case TRAP_NET:    base = OBJ_MISSILES; sub = MI_THROWING_NET; break;
    default:          return item;
    }

    item.base_type = base;
    item.sub_type  = sub;
    item.quantity  = 1;

    if (base == OBJ_MISSILES)
    {
        set_item_ego_type(item, base,
                          (sub == MI_NEEDLE) ? SPMSL_POISONED : SPMSL_NORMAL);
    }
    else
    {
        set_item_ego_type(item, base, SPWPN_NORMAL);
    }

    // give appropriate racial flag for Orcish Mines and Elven Halls
    // should we ever allow properties of dungeon features, we could use that
    if (you.where_are_you == BRANCH_ORCISH_MINES)
        set_equip_race( item, ISFLAG_ORCISH );
    else if (you.where_are_you == BRANCH_ELVEN_HALLS)
        set_equip_race( item, ISFLAG_ELVEN );

    item_colour(item);
    return item;
}

// Shoot a single piece of ammo at the relevant actor.
void trap_def::shoot_ammo(actor& act, bool was_known)
{
    if (this->ammo_qty <= 0)
    {
        if (was_known && act.atype() == ACT_PLAYER)
            mpr("The trap is out of ammunition!");
        else if (player_can_hear(this->pos) && see_grid(this->pos))
            mpr("You hear a soft click.");

        this->disarm();
    }
    else
    {
        // Record position now, in case it's a monster and dies (thus
        // resetting its position) before the ammo can be dropped.
        const coord_def apos = act.pos();

        item_def shot = this->generate_trap_item();
        bool poison = (this->type == TRAP_NEEDLE
                       && !act.res_poison()
                       && x_chance_in_y(50 - (3*act.armour_class()) / 2, 100));

        int damage_taken =
            std::max(this->shot_damage(act) - random2(act.armour_class()+1),0);

        int trap_hit = (20 + (you.your_level*2)) * random2(200) / 100;

        if (act.atype() == ACT_PLAYER)
        {
            if (one_chance_in(5) || (was_known && !one_chance_in(4)))
            {
                mprf( "You avoid triggering %s trap.",
                      this->name(DESC_NOCAP_A).c_str() );

                return;         // no ammo generated either
            }

            // Start constructing the message.
            std::string msg = shot.name(DESC_CAP_A) + " shoots out and ";

            // Check for shield blocking.
            // Exercise only if the trap was unknown (to prevent scumming.)
            if (!was_known && player_shield_class() && coinflip())
                exercise(SK_SHIELDS, 1);

            const int con_block = random2(20 + you.shield_block_penalty());
            const int pro_block = you.shield_bonus();
            if (pro_block >= con_block)
            {
                // Note that we don't call shield_block_succeeded()
                // because that can exercise Shields skill.
                you.shield_blocks++;
                msg += "hits your shield.";
                mpr(msg.c_str());
            }
            else
            {
                // Note that this uses full (not random2limit(foo,40))
                // player_evasion.
                int your_dodge = you.melee_evasion(NULL) - 2
                    + (random2(you.dex) / 3)
                    + (you.duration[DUR_REPEL_MISSILES] * 10);

                // Check if it got past dodging. Deflect Missiles provides
                // immunity to such traps.
                if (trap_hit >= your_dodge
                    && you.duration[DUR_DEFLECT_MISSILES] == 0)
                {
                    // OK, we've been hit.
                    msg += "hits you!";
                    mpr(msg.c_str());

                    // Needle traps can poison.
                    if (poison)
                        you.poison(NULL, 1 + random2(3));

                    ouch(damage_taken, NON_MONSTER, KILLED_BY_TRAP,
                         shot.name(DESC_PLAIN).c_str());
                }
                else            // trap dodged
                {
                    msg += "misses you.";
                    mpr(msg.c_str());
                }

                // Exercise only if the trap was unknown (to prevent scumming.)
                if (!was_known && player_light_armour(true) && coinflip())
                    exercise(SK_DODGING, 1);
            }
        }
        else if (act.atype() == ACT_MONSTER)
        {
            // Determine whether projectile hits.
            bool hit = (trap_hit >= act.melee_evasion(NULL));

            if (see_grid(act.pos()))
            {
                mprf("%s %s %s%s!",
                     shot.name(DESC_CAP_A).c_str(),
                     hit ? "hits" : "misses",
                     act.name(DESC_NOCAP_THE).c_str(),
                     (hit && damage_taken == 0
                         && !poison) ? ", but does no damage" : "");
            }

            // Apply damage.
            if (hit)
            {
                if (poison)
                    act.poison(NULL, 1 + random2(3));
                act.hurt(NULL, damage_taken);
            }
        }

        // Drop the item (sometimes.)
        if (coinflip())
            copy_item_to_grid(shot, apos);

        this->ammo_qty--;
    }
}

// returns appropriate trap symbol
dungeon_feature_type trap_def::category() const
{
    switch (this->type)
    {
    case TRAP_SHAFT:
        return (DNGN_TRAP_NATURAL);

    case TRAP_TELEPORT:
    case TRAP_ALARM:
    case TRAP_ZOT:
        return (DNGN_TRAP_MAGICAL);

    case TRAP_DART:
    case TRAP_ARROW:
    case TRAP_SPEAR:
    case TRAP_AXE:
    case TRAP_BLADE:
    case TRAP_BOLT:
    case TRAP_NEEDLE:
    case TRAP_NET:
    default:                    // what *would* be the default? {dlb}
        return (DNGN_TRAP_MECHANICAL);
    }
}

bool is_valid_shaft_level(const level_id &place)
{
    if (place.level_type != LEVEL_DUNGEON)
        return (false);

    // Disallow shafts on the first two levels.
    if (place == BRANCH_MAIN_DUNGEON
        && you.your_level < 2)
    {
        return (false);
    }

    // Don't generate shafts in branches where teleport control
    // is prevented.  Prevents player from going down levels without
    // reaching stairs, and also keeps player from getting stuck
    // on lower levels with the innability to use teleport control to
    // get back up.
    if (testbits(get_branch_flags(place.branch), LFLAG_NO_TELE_CONTROL))
        return (false);

    const Branch &branch = branches[place.branch];

    // When generating levels, don't place a shaft on the level
    // immediately above the bottom of a branch if that branch is
    // significantly more dangerous than normal.
    int min_delta = 1;
    if (env.turns_on_level == -1 && branch.dangerous_bottom_level)
        min_delta = 2;

    return ((branch.depth - place.depth) >= min_delta);
}

level_id generic_shaft_dest(level_pos lpos)
{
    level_id  lid = lpos.id;
    coord_def pos = lpos.pos;

    if (lid.level_type != LEVEL_DUNGEON)
        return lid;

    int      curr_depth = lid.depth;
    Branch   &branch    = branches[lid.branch];

    // 25% drop one level
    // 50% drop two levels
    // 25% drop three levels
    lid.depth += 2;
    if (pos.x % 2)
        lid.depth--;
    if (pos.y % 2)
        lid.depth++;

    if (lid.depth > branch.depth)
        lid.depth = branch.depth;

    if (lid.depth == curr_depth)
        return lid;

    // Only shafts on the level immediately above a dangerous branch
    // bottom will take you to that dangerous bottom, and shafts can't
    // be created during level generation time.
    // Include level 27 of the main dungeon here, but don't restrict
    // shaft creation (so don't set branch.dangerous_bottom_level).
    if ((branch.dangerous_bottom_level)
        && lid.depth == branch.depth
        && (branch.depth - curr_depth) > 1)
    {
        lid.depth--;
    }

    return lid;
}

level_id generic_shaft_dest(coord_def pos)
{
    return generic_shaft_dest(level_pos(level_id::current(), pos));
}

void handle_items_on_shaft(const coord_def& pos, bool open_shaft)
{
    if (!is_valid_shaft_level())
        return;

    level_id  dest = generic_shaft_dest(pos);

    if (dest == level_id::current())
        return;

    int o = igrd(pos);

    if (o == NON_ITEM)
        return;

    igrd(pos) = NON_ITEM;

    if (is_terrain_seen(pos) && open_shaft)
    {
        mpr("A shaft opens up in the floor!");
        grd(pos) = DNGN_TRAP_NATURAL;
    }

    while (o != NON_ITEM)
    {
        int next = mitm[o].link;

        if (is_valid_item( mitm[o] ))
        {
            if (is_terrain_seen(pos))
            {
                mprf("%s falls through the shaft.",
                     mitm[o].name(DESC_INVENTORY).c_str());
            }
            add_item_to_transit(dest, mitm[o]);

            mitm[o].base_type = OBJ_UNASSIGNED;
            mitm[o].quantity = 0;
            mitm[o].props.clear();
        }

        o = next;
    }
}

static int num_traps_default(int level_number, const level_id &place)
{
    return random2avg(9, 2);
}

int num_traps_for_place(int level_number, const level_id &place)
{
    if (level_number == -1)
        level_number = place.absdepth();

    switch (place.level_type)
    {
    case LEVEL_DUNGEON:
        if (branches[place.branch].num_traps_function != NULL)
            return branches[place.branch].num_traps_function(level_number);
        else
            return num_traps_default(level_number, place);
    case LEVEL_ABYSS:
        return traps_abyss_number(level_number);
    case LEVEL_PANDEMONIUM:
        return traps_pan_number(level_number);
    case LEVEL_LABYRINTH:
    case LEVEL_PORTAL_VAULT:
        ASSERT(false);
        break;
    default:
        return 0;
    }

    return 0;
}

static trap_type random_trap_default(int level_number, const level_id &place)
{
    trap_type type = TRAP_DART;

    if ((random2(1 + level_number) > 1) && one_chance_in(4))
        type = TRAP_NEEDLE;
    if (random2(1 + level_number) > 3)
        type = TRAP_SPEAR;
    if (random2(1 + level_number) > 5)
        type = TRAP_AXE;

    // Note we're boosting arrow trap numbers by moving it
    // down the list, and making spear and axe traps rarer.
    if (type == TRAP_DART ? random2(1 + level_number) > 2
                          : one_chance_in(7))
    {
        type = TRAP_ARROW;
    }

    if ((type == TRAP_DART || type == TRAP_ARROW) && one_chance_in(15))
        type = TRAP_NET;

    if (random2(1 + level_number) > 7)
        type = TRAP_BOLT;
    if (random2(1 + level_number) > 11)
        type = TRAP_BLADE;

    if (random2(1 + level_number) > 14 && one_chance_in(3)
        || (place == BRANCH_HALL_OF_ZOT && coinflip()))
    {
        type = TRAP_ZOT;
    }

    if (one_chance_in(50) && is_valid_shaft_level(place))
        type = TRAP_SHAFT;
    if (one_chance_in(20))
        type = TRAP_TELEPORT;
    if (one_chance_in(40))
        type = TRAP_ALARM;

    return (type);
}

trap_type random_trap_for_place(int level_number, const level_id &place)
{
    if (level_number == -1)
        level_number = place.absdepth();

    switch (place.level_type)
    {
    case LEVEL_DUNGEON:
        if (branches[place.branch].rand_trap_function != NULL)
            return branches[place.branch].rand_trap_function(level_number);
        else
            return random_trap_default(level_number, place);
    case LEVEL_ABYSS:
        return traps_abyss_type(level_number);
    case LEVEL_PANDEMONIUM:
        return traps_pan_type(level_number);
    default:
        return random_trap_default(level_number, place);
    }
    return NUM_TRAPS;
}

int traps_zero_number(int level_number)
{
    return 0;
}

int traps_pan_number(int level_number)
{
    return num_traps_default(level_number, level_id(LEVEL_PANDEMONIUM));
}

trap_type traps_pan_type(int level_number)
{
    return random_trap_default(level_number, level_id(LEVEL_PANDEMONIUM));
}

int traps_abyss_number(int level_number)
{
    return num_traps_default(level_number, level_id(LEVEL_ABYSS));
}

trap_type traps_abyss_type(int level_number)
{
    return random_trap_default(level_number, level_id(LEVEL_ABYSS));
}

int traps_lab_number(int level_number)
{
    return num_traps_default(level_number, level_id(LEVEL_LABYRINTH));
}

trap_type traps_lab_type(int level_number)
{
    return random_trap_default(level_number, level_id(LEVEL_LABYRINTH));
}
