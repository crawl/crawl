/*
 *  File:       monstuff.cc
 *  Summary:    Misc monster related functions.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 */

#include "AppHdr.h"
REVISION("$Rev$");
#include "monstuff.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <algorithm>

#ifdef DOS
#include <conio.h>
#endif

#include "externs.h"

#include "arena.h"
#include "beam.h"
#include "cloud.h"
#include "database.h"
#include "debug.h"
#include "delay.h"
#include "describe.h"
#include "dgnevent.h"
#include "fight.h"
#include "files.h"
#include "ghost.h"
#include "hiscores.h"
#include "it_use2.h"
#include "itemname.h"
#include "items.h"
#include "itemprop.h"
#include "Kills.h"
#include "makeitem.h"
#include "mapmark.h"
#include "message.h"
#include "misc.h"
#include "monplace.h"
#include "monspeak.h"
#include "mon-pick.h"
#include "mon-util.h"
#include "mstuff2.h"
#include "notes.h"
#include "player.h"
#include "randart.h"
#include "religion.h"
#include "spl-mis.h"
#include "spl-util.h"
#include "spells3.h"
#include "spells4.h"
#include "state.h"
#include "stuff.h"
#include "terrain.h"
#include "traps.h"
#include "tutorial.h"
#include "view.h"
#include "stash.h"
#include "xom.h"

static bool _wounded_damaged(int wound_class);
static bool _handle_special_ability(monsters *monster, bolt & beem);
static bool _handle_pickup(monsters *monster);
static void _handle_behaviour(monsters *monster);
static void _set_nearest_monster_foe(monsters *monster);
static void _mons_in_cloud(monsters *monster);
static bool _mon_can_move_to_pos(const monsters *monster,
                                 const coord_def& delta,
                                 bool just_check = false);
static bool _is_trap_safe(const monsters *monster, const coord_def& where,
                          bool just_check = false);
static bool _monster_move(monsters *monster);
static spell_type _map_wand_to_mspell(int wand_type);

static bool _try_pathfind(monsters *mon, const dungeon_feature_type can_move,
                          bool potentially_blocking);

// [dshaligram] Doesn't need to be extern.
static coord_def mmov;

static const coord_def mon_compass[8] = {
    coord_def(-1,-1), coord_def(0,-1), coord_def(1,-1), coord_def(1,0),
    coord_def( 1, 1), coord_def(0, 1), coord_def(-1,1), coord_def(-1,0)
};

static bool immobile_monster[MAX_MONSTERS];

// A probably needless optimization: convert the C string "just seen" to
// a C++ string just once, instead of twice every time a monster moves.
static const std::string _just_seen("just seen");

#define FAR_AWAY    1000000         // used in monster_move()

#define ENERGY_SUBMERGE(entry) (std::max(entry->energy_usage.swim / 2, 1))

// This function creates an artificial item to represent a mimic's appearance.
// Eventually, mimics could be redone to be more like dancing weapons...
// there'd only be one type and it would look like the item it carries. -- bwr
void get_mimic_item( const monsters *mimic, item_def &item )
{
    ASSERT( mimic != NULL && mons_is_mimic( mimic->type ) );

    item.base_type = OBJ_UNASSIGNED;
    item.sub_type  = 0;
    item.special   = 0;
    item.colour    = 0;
    item.flags     = 0;
    item.quantity  = 1;
    item.plus      = 0;
    item.plus2     = 0;
    item.pos = mimic->pos();
    item.link      = NON_ITEM;

    int prop = 127 * mimic->pos().x + 269 * mimic->pos().y;

    rng_save_excursion exc;
    seed_rng( prop );

    switch (mimic->type)
    {
    case MONS_WEAPON_MIMIC:
        item.base_type = OBJ_WEAPONS;
        item.sub_type = (59 * mimic->pos().x + 79 * mimic->pos().y)
                            % (WPN_MAX_NONBLESSED + 1);

        prop %= 100;

        if (prop < 20)
            make_item_randart(item);
        else if (prop < 50)
            set_equip_desc(item, ISFLAG_GLOWING);
        else if (prop < 80)
            set_equip_desc(item, ISFLAG_RUNED);
        else if (prop < 85)
            set_equip_race(item, ISFLAG_ORCISH);
        else if (prop < 90)
            set_equip_race(item, ISFLAG_DWARVEN);
        else if (prop < 95)
            set_equip_race(item, ISFLAG_ELVEN);
        break;

    case MONS_ARMOUR_MIMIC:
        item.base_type = OBJ_ARMOUR;
        item.sub_type = (59 * mimic->pos().x + 79 * mimic->pos().y)
                            % NUM_ARMOURS;

        prop %= 100;

        if (prop < 20)
            make_item_randart(item);
        else if (prop < 40)
            set_equip_desc(item, ISFLAG_GLOWING);
        else if (prop < 60)
            set_equip_desc(item, ISFLAG_RUNED);
        else if (prop < 80)
            set_equip_desc(item, ISFLAG_EMBROIDERED_SHINY);
        else if (prop < 85)
            set_equip_race(item, ISFLAG_ORCISH);
        else if (prop < 90)
            set_equip_race(item, ISFLAG_DWARVEN);
        else if (prop < 95)
            set_equip_race(item, ISFLAG_ELVEN);
        break;

    case MONS_SCROLL_MIMIC:
        item.base_type = OBJ_SCROLLS;
        item.sub_type = prop % NUM_SCROLLS;
        break;

    case MONS_POTION_MIMIC:
        item.base_type = OBJ_POTIONS;
        item.sub_type = prop % NUM_POTIONS;
        break;

    case MONS_GOLD_MIMIC:
    default:
        item.base_type = OBJ_GOLD;
        item.quantity = 5 + prop % 30;
        break;
    }

    item_colour(item); // also sets special vals for scrolls/potions
}

// Sets the colour of a mimic to match its description... should be called
// whenever a mimic is created or teleported. -- bwr
int get_mimic_colour( const monsters *mimic )
{
    ASSERT( mimic != NULL && mons_is_mimic( mimic->type ) );

    if (mimic->type == MONS_SCROLL_MIMIC)
        return (LIGHTGREY);
    else if (mimic->type == MONS_GOLD_MIMIC)
        return (YELLOW);

    item_def  item;
    get_mimic_item( mimic, item );

    return (item.colour);
}

// Monster curses a random player inventory item.
bool curse_an_item( bool decay_potions, bool quiet )
{
    int count = 0;
    int item  = ENDOFPACK;

    for (int i = 0; i < ENDOFPACK; i++)
    {
        if (!is_valid_item( you.inv[i] ))
            continue;

        if (you.inv[i].base_type == OBJ_WEAPONS
            || you.inv[i].base_type == OBJ_ARMOUR
            || you.inv[i].base_type == OBJ_JEWELLERY
            || you.inv[i].base_type == OBJ_POTIONS)
        {
            if (item_cursed( you.inv[i] ))
                continue;

            if (you.inv[i].base_type != OBJ_POTIONS
                && !you_tran_can_wear(you.inv[i])
                && item_is_equipped(you.inv[i]))
            {
                // Melded items cannot be cursed.
                continue;
            }

            if (you.inv[i].base_type == OBJ_POTIONS
                && (!decay_potions || you.inv[i].sub_type == POT_DECAY))
            {
                continue;
            }

            // Item is valid for cursing, so we'll give it a chance.
            count++;
            if (one_chance_in( count ))
                item = i;
        }
    }

    // Any item to curse?
    if (item == ENDOFPACK)
        return (false);

    // Curse item.
    if (decay_potions && !quiet) // Just for mummies.
        mpr("You feel nervous for a moment...", MSGCH_MONSTER_SPELL);

    if (you.inv[item].base_type == OBJ_POTIONS)
    {
        int amount;
        // Decay at least two of the stack.
        if (you.inv[item].quantity <= 2)
            amount = you.inv[item].quantity;
        else
            amount = 2 + random2(you.inv[item].quantity - 1);

        split_potions_into_decay(item, amount);

        if (item_value(you.inv[item], true) / amount > 2)
            xom_is_stimulated(32 * amount);
    }
    else
    {
        do_curse_item( you.inv[item], false );
    }

    return (true);
}

void monster_drop_ething(monsters *monster, bool mark_item_origins,
                         int owner_id)
{
    const bool hostile_grid = grid_destroys_items(grd(monster->pos()));

    bool destroyed = false;

    // Drop weapons & missiles last (ie on top) so others pick up.
    for (int i = NUM_MONSTER_SLOTS - 1; i >= 0; i--)
    {
        int item = monster->inv[i];

        if (item != NON_ITEM)
        {
            const bool summoned_item =
                testbits(mitm[item].flags, ISFLAG_SUMMONED);
            if (hostile_grid || summoned_item)
            {
                item_was_destroyed(mitm[item], monster->mindex());
                destroy_item( item );
                if (!summoned_item)
                    destroyed = true;
            }
            else
            {
                if (mons_friendly(monster) && is_valid_item(mitm[item]))
                    mitm[item].flags |= ISFLAG_DROPPED_BY_ALLY;

                move_item_to_grid(&item, monster->pos());

                if (mark_item_origins && is_valid_item(mitm[item]))
                    origin_set_monster(mitm[item], monster);
            }

            monster->inv[i] = NON_ITEM;
        }
    }

    if (destroyed)
        mprf(MSGCH_SOUND, grid_item_destruction_message(grd(monster->pos())));
}

int fill_out_corpse(const monsters* monster, item_def& corpse,
                    bool allow_weightless)
{
    ASSERT(monster->type != -1 && monster->type != MONS_PROGRAM_BUG);
    corpse.clear();

    int summon_type;
    if (mons_is_summoned(monster, NULL, &summon_type)
        || (monster->flags & (MF_BANISHED | MF_HARD_RESET)))
    {
        return (-1);
    }

    int corpse_class = mons_species(monster->type);

    // If this was a corpse that was temporarily animated then turn the
    // monster back into a corpse.
    if (mons_class_is_zombified(monster->type)
        && (summon_type == SPELL_ANIMATE_DEAD
            || summon_type == SPELL_ANIMATE_SKELETON
            || summon_type == MON_SUMM_ANIMATE))
    {
        corpse_class = mons_zombie_base(monster);
    }

    if (corpse_class == MONS_DRACONIAN)
    {
        if (monster->type == MONS_TIAMAT)
            corpse_class = MONS_DRACONIAN;
        else
            corpse_class = draco_subspecies(monster);
    }

    if (monster->has_ench(ENCH_SHAPESHIFTER))
        corpse_class = MONS_SHAPESHIFTER;
    else if (monster->has_ench(ENCH_GLOWING_SHAPESHIFTER))
        corpse_class = MONS_GLOWING_SHAPESHIFTER;

    // Doesn't leave a corpse.
    if (mons_weight(corpse_class) == 0 && !allow_weightless)
        return (-1);

    corpse.flags       = 0;
    corpse.base_type   = OBJ_CORPSES;
    corpse.plus        = corpse_class;
    corpse.plus2       = 0;    // butcher work done
    corpse.sub_type    = CORPSE_BODY;
    corpse.special     = FRESHEST_CORPSE;  // rot time
    corpse.quantity    = 1;
    corpse.orig_monnum = monster->type + 1;
    corpse.props[MONSTER_NUMBER] = short(monster->number);

    corpse.colour = mons_class_colour(corpse_class);
    if (corpse.colour == BLACK)
        corpse.colour = monster->colour;

    if (!monster->mname.empty())
        corpse.props[CORPSE_NAME_KEY] = monster->mname;
    else if (mons_is_unique(monster->type))
        corpse.props[CORPSE_NAME_KEY] = mons_type_name(monster->type,
                                                       DESC_PLAIN);

    return (corpse_class);
}

int place_monster_corpse(const monsters *monster, bool silent,
                         bool force)
{
    // The game can attempt to place a corpse for an out-of-bounds monster
    // if a shifter turns into a giant spore and explodes.  In this
    // case we place no corpse since the explosion means anything left
    // over would be scattered, tiny chunks of shifter.
    if (!in_bounds(monster->pos()))
        return (-1);

    item_def corpse;
    const int corpse_class = fill_out_corpse(monster, corpse);

    // Don't place a corpse?  If a zombified monster is somehow capable
    // of leaving a corpse, then always place it.
    if (mons_class_is_zombified(monster->type))
        force = true;

    if (corpse_class == -1 || (!force && coinflip()))
        return (-1);

    if (grid_destroys_items(grd(monster->pos())))
    {
        item_was_destroyed(corpse);
        return (-1);
    }

    int o = get_item_slot();
    if (o == NON_ITEM)
        return (-1);
    mitm[o] = corpse;

    origin_set_monster(mitm[o], monster);

    // Don't care if 'o' is changed, and it shouldn't be (corpses don't
    // stack).
    move_item_to_grid(&o, monster->pos());
    if (see_grid(monster->pos()))
    {
        if (force && !silent)
        {
            if (you.can_see(monster))
                simple_monster_message(monster, " turns back into a corpse!");
            else
            {
                mprf("%s appears out of nowhere!",
                     mitm[o].name(DESC_CAP_A).c_str());
            }
        }
        const bool poison = (mons_corpse_effect(corpse_class) == CE_POISONOUS
                             && player_res_poison() <= 0);
        tutorial_dissection_reminder(!poison);
    }

    return (o);
}

static void _tutorial_inspect_kill()
{
    if (Options.tutorial_events[TUT_KILLED_MONSTER])
        learned_something_new(TUT_KILLED_MONSTER);
}

#ifdef DGL_MILESTONES
static std::string _milestone_kill_verb(killer_type killer)
{
    return (killer == KILL_RESET ? "banished " : "killed ");
}

static void _check_kill_milestone(const monsters *mons,
                                 killer_type killer, int i)
{
    if (mons->type == MONS_PLAYER_GHOST)
    {
        std::string milestone = _milestone_kill_verb(killer) + "the ghost of ";
        milestone += get_ghost_description(*mons, true);
        milestone += ".";
        mark_milestone("ghost", milestone);
    }
    else if (mons_is_unique(mons->type))
    {
        mark_milestone("unique",
                       _milestone_kill_verb(killer)
                       + mons->name(DESC_NOCAP_THE, true)
                       + ".");
    }
}
#endif // DGL_MILESTONES

static void _give_monster_experience(monsters *victim,
                                     int killer_index, int experience,
                                     bool victim_was_born_friendly)
{
    if (invalid_monster_index(killer_index))
        return;

    monsters *mon = &menv[killer_index];
    if (!mon->alive())
        return;

    if ((!victim_was_born_friendly || !mons_friendly(mon))
        && !mons_aligned(killer_index, monster_index(victim)))
    {
        if (mon->gain_exp(experience))
        {
            if (you.religion != GOD_SHINING_ONE && you.religion != GOD_BEOGH
                || player_under_penance()
                || !one_chance_in(3))
            {
                return;
            }

            // Randomly bless the follower who gained experience.
            if (you.religion == GOD_SHINING_ONE
                    && random2(you.piety) >= piety_breakpoint(0)
                || you.religion == GOD_BEOGH
                    && random2(you.piety) >= piety_breakpoint(2))
            {
                bless_follower(mon);
            }
        }
    }
}

static void _give_adjusted_experience(monsters *monster, killer_type killer,
                                     bool pet_kill, int killer_index,
                                     unsigned int *exp_gain,
                                     unsigned int *avail_gain)
{
    const int experience = exper_value(monster);

    const bool created_friendly =
        testbits(monster->flags, MF_CREATED_FRIENDLY);
    const bool was_neutral = testbits(monster->flags, MF_WAS_NEUTRAL);
    const bool no_xp = monster->has_ench(ENCH_ABJ) || !experience;
    const bool already_got_half_xp = testbits(monster->flags, MF_GOT_HALF_XP);

    bool need_xp_msg = false;
    if (created_friendly || was_neutral || no_xp)
        ; // No experience if monster was created friendly or summoned.
    else if (YOU_KILL(killer))
    {
        int old_lev = you.experience_level;
        if (already_got_half_xp)
            gain_exp( experience / 2, exp_gain, avail_gain );
        else
            gain_exp( experience, exp_gain, avail_gain );

        if (old_lev == you.experience_level)
            need_xp_msg = true;
    }
    else if (pet_kill && !already_got_half_xp)
    {
        int old_lev = you.experience_level;
        gain_exp( experience / 2 + 1, exp_gain, avail_gain );

        if (old_lev == you.experience_level)
            need_xp_msg = true;
    }

    // Give a message for monsters dying out of sight.
    if (need_xp_msg && exp_gain > 0
        && (!mons_near(monster) || !you.can_see(monster))
        && !crawl_state.arena)
    {
        mpr("You feel a bit more experienced.");
    }

    if (MON_KILL(killer) && !no_xp)
    {
        _give_monster_experience( monster, killer_index, experience,
                                  created_friendly );
    }
}

static bool _is_pet_kill(killer_type killer, int i)
{
    if (!MON_KILL(killer))
        return (false);

    if (i == ANON_FRIENDLY_MONSTER)
        return (true);

    if (invalid_monster_index(i))
        return (false);

    const monsters *m = &menv[i];
    if (mons_friendly(m)) // This includes enslaved monsters.
        return (true);

    // Check if the monster was confused by you or a friendly, which
    // makes casualties to this monster collateral kills.
    const mon_enchant me = m->get_ench(ENCH_CONFUSION);
    return (me.ench == ENCH_CONFUSION
            && (me.who == KC_YOU || me.who == KC_FRIENDLY));
}

// Elyvilon will occasionally (5% chance) protect the life of one of
// your allies.
static bool _ely_protect_ally(monsters *monster)
{
    if (you.religion != GOD_ELYVILON)
        return (false);

    if (mons_holiness(monster) != MH_NATURAL
            && mons_holiness(monster) != MH_HOLY
        || !mons_friendly(monster)
        || !mons_near(monster)
        || !player_monster_visible(monster) // for simplicity
        || !one_chance_in(20))
    {
        return (false);
    }

    monster->hit_points = 1;

    snprintf(info, INFO_SIZE, " protects %s from harm!%s",
             monster->name(DESC_NOCAP_THE).c_str(),
             coinflip() ? "" : " You feel responsible.");

    simple_god_message(info);
    lose_piety(1);

    return (true);
}

// Elyvilon retribution effect: Heal hostile monsters that were about to
// be killed by you or one of your friends.
static bool _ely_heal_monster(monsters *monster, killer_type killer, int i)
{
    if (you.religion == GOD_ELYVILON)
        return (false);

    god_type god = GOD_ELYVILON;

    if (!you.penance[god] || !god_hates_your_god(god))
        return (false);

    const int ely_penance = you.penance[god];

    if (mons_friendly(monster) || !one_chance_in(10))
        return (false);

    if (MON_KILL(killer) && !invalid_monster_index(i))
    {
        monsters *mon = &menv[i];
        if (!mons_friendly(mon) || !one_chance_in(3))
            return (false);

        if (!mons_near(monster))
            return (false);
    }
    else if (!YOU_KILL(killer))
        return (false);

#ifdef DEBUG_DIAGNOSTICS
    mprf(MSGCH_DIAGNOSTICS, "monster hp: %d, max hp: %d",
         monster->hit_points, monster->max_hit_points);
#endif

    monster->hit_points = std::min(1 + random2(ely_penance/3),
                                   monster->max_hit_points);

#ifdef DEBUG_DIAGNOSTICS
    mprf(MSGCH_DIAGNOSTICS, "new hp: %d, ely penance: %d",
         monster->hit_points, ely_penance);
#endif

    snprintf(info, INFO_SIZE, "%s heals %s%s",
             god_name(god, false).c_str(),
             monster->name(DESC_NOCAP_THE).c_str(),
             monster->hit_points * 2 <= monster->max_hit_points ? "." : "!");

    god_speaks(god, info);
    dec_penance(god, 1 + random2(monster->hit_points/2));

    return (true);
}

static bool _yred_enslave_soul(monsters *monster, killer_type killer)
{
    if (you.religion == GOD_YREDELEMNUL && mons_enslaved_body_and_soul(monster)
        && mons_near(monster) && killer != KILL_RESET
        && killer != KILL_DISMISSED)
    {
        yred_make_enslaved_soul(monster, player_under_penance());
        return (true);
    }

    return (false);
}

static bool _beogh_forcibly_convert_orc(monsters *monster, killer_type killer,
                                        int i)
{
    if (you.religion == GOD_BEOGH
        && mons_species(monster->type) == MONS_ORC
        && !mons_is_summoned(monster) && !mons_is_shapeshifter(monster)
        && !player_under_penance() && you.piety >= piety_breakpoint(2)
        && mons_near(monster))
    {
        bool convert = false;

        if (YOU_KILL(killer))
            convert = true;
        else if (MON_KILL(killer) && !invalid_monster_index(i))
        {
            monsters *mon = &menv[i];
            if (is_follower(mon) && !one_chance_in(3))
                convert = true;
        }

        // Orcs may convert to Beogh under threat of death, either from
        // you or, less often, your followers.  In both cases, the
        // checks are made against your stats.  You're the potential
        // messiah, after all.
        if (convert)
        {
#ifdef DEBUG_DIAGNOSTICS
            mprf(MSGCH_DIAGNOSTICS, "Death convert attempt on %s, HD: %d, "
                 "your xl: %d",
                 monster->name(DESC_PLAIN).c_str(),
                 monster->hit_dice,
                 you.experience_level);
#endif
            if (random2(you.piety) >= piety_breakpoint(0)
                && random2(you.experience_level) >= random2(monster->hit_dice)
                // Bias beaten-up-conversion towards the stronger orcs.
                && random2(monster->hit_dice) > 2)
            {
                beogh_convert_orc(monster, true, MON_KILL(killer));
                return (true);
            }
        }
    }

    return (false);
}

static bool _monster_avoided_death(monsters *monster, killer_type killer, int i)
{
    if (monster->hit_points < -25
        || monster->hit_points < -monster->max_hit_points
        || monster->max_hit_points <= 0
        || monster->hit_dice < 1)
    {
        return (false);
    }

    // Elyvilon specials.
    if (_ely_protect_ally(monster))
        return (true);
    if (_ely_heal_monster(monster, killer, i))
        return (true);

    // Yredelemnul special.
    if (_yred_enslave_soul(monster, killer))
        return (true);

    // Beogh special.
    if (_beogh_forcibly_convert_orc(monster, killer, i))
        return (true);

    return (false);
}

static bool _slime_pit_unlock(bool silent)
{
    unset_level_flags(LFLAG_NO_TELE_CONTROL, silent);

    bool in_los = false;
    if (!silent)
    {
        for (int x = 0; x < GXM && !in_los; ++x)
            for (int y = 0; y < GYM; ++y)
                if (grd[x][y] == DNGN_STONE_WALL
                    && see_grid(x, y))
                {
                    in_los = true;
                    break;
                }
    }

    replace_area_wrapper(DNGN_STONE_WALL, DNGN_CLEAR_ROCK_WALL);

    if (!silent)
    {
        if (in_los)
        {
            mpr("Suddenly, all colour oozes out of the stone walls.",
                MSGCH_MONSTER_ENCHANT);
        }
        else
        {
            mpr("You feel a strange vibration for a moment.",
                MSGCH_MONSTER_ENCHANT);
        }
    }

    return (true);
}

static bool _slime_pit_unlock_offlevel()
{
    return _slime_pit_unlock(true);
}
static bool _slime_pit_unlock_onlevel()
{
    return _slime_pit_unlock(false);
}

static void _fire_monster_death_event(monsters *monster,
                                      killer_type killer,
                                      int i, bool polymorph)
{
    int type = monster->type;

    // Treat whatever the Royal Jelly polymorphed into as if it were still
    // the Royal Jelly (but if a player chooses the character name
    // "shaped Royal Jelly" don't unlock the vaults when the player's
    // ghost is killed).
    if (monster->mname == "shaped Royal Jelly"
        && monster->type != MONS_PLAYER_GHOST)
    {
        type = MONS_ROYAL_JELLY;
    }

    // Banished monsters aren't technically dead, so no death event
    // for them.
    if (killer == KILL_RESET)
    {
        // Give player a hint that banishing the Royal Jelly means the
        // Slime:6 vaults stay locked.
        if (type == MONS_ROYAL_JELLY)
        {
            if (you.can_see(monster))
                mpr("You feel a great sense of loss.");
            else
                mpr("You feel a great sense of loss, and the brush of the "
                    "Abyss.");
        }
        return;
    }

    dungeon_events.fire_event(
        dgn_event(DET_MONSTER_DIED, monster->pos(), 0,
                  monster_index(monster), killer));

    // Don't unlock the Slime:6 vaults if the "death" was actually the
    // Royal Jelly polymorphing into something else; the player still
    // has to kill whatever it polymorphed into.
    if (type == MONS_ROYAL_JELLY && !polymorph)
    {
        const level_id target(BRANCH_SLIME_PITS, 6);
        if (is_existing_level(target))
        {
            apply_to_level(
                target,
                true,
                target == level_id::current() ? _slime_pit_unlock_onlevel
                                              : _slime_pit_unlock_offlevel);
        }
    }
}

static void _mummy_curse(monsters* monster, killer_type killer, int index)
{
    int pow;

    switch(killer)
    {
        // Mummy killed by trap or something other than the player or
        // another monster, so no curse.
        case KILL_MISC:
        // Mummy sent to the Abyss wasn't actually killed, so no curse.
        case KILL_RESET:
        case KILL_DISMISSED:
            return;

        default:
            break;
    }

    switch(monster->type)
    {
        case MONS_MUMMY:          pow = 1; break;
        case MONS_GUARDIAN_MUMMY: pow = 3; break;
        case MONS_MUMMY_PRIEST:   pow = 8; break;
        case MONS_GREATER_MUMMY:  pow = 11; break;

        default:
            mpr("Unknown mummy type.", MSGCH_DIAGNOSTICS);
            return;
    }

    // beam code might give an index of MHITYOU for the player.
    if (YOU_KILL(killer))
        index = NON_MONSTER;

    // Killed by a Zot trap, a god, etc.
    if (index != NON_MONSTER && invalid_monster_index(index))
        return;

    actor* target;
    if (index == NON_MONSTER)
        target = &you;
    else
    {
        // Mummies committing suicide don't cause a death curse.
        if (index == monster->mindex())
           return;
        target = &menv[index];
    }

    // Mummy was killed by a giant spore or ball lightning?
    if (!target->alive())
        return;

    if (monster->type == MONS_MUMMY && YOU_KILL(killer))
        curse_an_item(true);
    else
    {
        if (index == NON_MONSTER)
            mpr("You feel extremely nervous for a moment...",
                MSGCH_MONSTER_SPELL);
        else if (you.can_see(target))
            mprf(MSGCH_MONSTER_SPELL, "A malignant aura surrounds %s.",
                 target->name(DESC_NOCAP_THE).c_str());
        MiscastEffect(target, monster_index(monster), SPTYP_NECROMANCY,
                      pow, random2avg(88, 3), "a mummy death curse");
    }
}

static bool _spore_goes_pop(monsters *monster, killer_type killer,
                            int killer_index, bool pet_kill, bool wizard)
{
    if (monster->hit_points > 0 || monster->hit_points <= -15 || wizard
        || killer == KILL_RESET || killer == KILL_DISMISSED)
    {
        return (false);
    }

    if (killer == KILL_MISC)
        killer = KILL_MON;

    bolt beam;
    const int type = monster->type;

    beam.is_tracer    = false;
    beam.is_explosion = true;
    beam.beam_source  = monster_index(monster);
    beam.type         = dchar_glyph(DCHAR_FIRED_BURST);
    beam.source       = monster->pos();
    beam.target       = monster->pos();
    beam.thrower      = killer;
    beam.aux_source.clear();

    if (YOU_KILL(killer))
        beam.aux_source = "set off by themselves";
    else if (pet_kill)
        beam.aux_source = "set off by their pet";

    const char* msg       = NULL;
    const char* sanct_msg = NULL;
    if (type == MONS_GIANT_SPORE)
    {
        beam.flavour = BEAM_SPORE;
        beam.damage  = dice_def(3, 15);
        beam.name    = "explosion of spores";
        beam.colour  = LIGHTGREY;
        beam.ex_size = 2;
        msg          = "The giant spore explodes!";
        sanct_msg    = "By Zin's power, the giant spore's explosion is "
                       "contained.";
    }
    else if (type == MONS_BALL_LIGHTNING)
    {
        beam.flavour = BEAM_ELECTRICITY;
        beam.damage  = dice_def(3, 20);
        beam.name    = "blast of lightning";
        beam.colour  = LIGHTCYAN;
        beam.ex_size = coinflip() ? 3 : 2;
        msg          = "The ball lightning explodes!";
        sanct_msg    = "By Zin's power, the ball lightning's explosion "
                       "is contained.";
    }
    else
    {
        msg::streams(MSGCH_DIAGNOSTICS) << "Unknown spore type: "
                                        << static_cast<int>(type)
                                        << std::endl;
        return (false);
    }

    bool saw = false;
    if (you.can_see(monster))
    {
        saw = true;
        viewwindow(true, false);
        if (is_sanctuary(monster->pos()))
            mpr(sanct_msg, MSGCH_GOD);
        else
            mprf(MSGCH_MONSTER_DAMAGE, MDAM_DEAD, msg);
    }

    if (is_sanctuary(monster->pos()))
        return (false);

    // Detach monster from the grid first, so it doesn't get hit by
    // its own explosion. (GDL)
    mgrd(monster->pos()) = NON_MONSTER;

    // The explosion might cause a monster to be placed where the spore
    // used to be, so make sure that mgrd() doesn't get cleared a second
    // time (causing the new monster to become floating) when
    // monster->reset() is called.
    monster->pos().reset();

    // Exploding kills the monster a bit earlier than normal.
    monster->hit_points = -16;
    if (saw)
        viewwindow(true, false);

    // FIXME: show_more == mons_near(monster)
    beam.explode();

    // Monster died in explosion, so don't re-attach it to the grid.
    return (true);
}

void _monster_die_cloud(const monsters* monster, bool corpse, bool silent,
                        bool summoned, int summon_type)
{
    // Chaos spawn always leave behind a cloud of chaos.
    if (monster->type == MONS_CHAOS_SPAWN)
    {
        summoned = true;
        corpse   = false;
    }

    if (!summoned)
        return;

    std::string prefix = " ";
    if (corpse)
    {
        if (mons_weight(mons_species(monster->type)) == 0)
            return;

        prefix = "'s corpse ";
    }

    std::string msg = summoned_poof_msg(monster);
    msg += "!";

    cloud_type cloud = CLOUD_NONE;
    if (msg.find("smoke") != std::string::npos)
        cloud = random_smoke_type();
    else if (msg.find("chaos") != std::string::npos)
        cloud = CLOUD_CHAOS;

    if (!silent)
        simple_monster_message(monster, (prefix + msg).c_str());

    if (cloud != CLOUD_NONE)
    {
        place_cloud(cloud, monster->pos(), 1 + random2(3),
                    monster->kill_alignment());
    }
}

int monster_die(monsters *monster, killer_type killer,
                int killer_index, bool silent, bool wizard)
{
    if (invalid_monster(monster))
        return (-1);

    // If a monster was banished to the Abyss and then killed there,
    // then its death wasn't a banishment.
    if (you.level_type == LEVEL_ABYSS)
        monster->flags &= ~MF_BANISHED;

    if (!silent && _monster_avoided_death(monster, killer, killer_index))
        return (-1);

    crawl_state.inc_mon_acting(monster);

    mons_clear_trapping_net(monster);

    // Update list of monsters beholding player.
    update_beholders(monster, true);

    // Clear auto exclusion now the monster is killed -- if we know about it.
    if (mons_near(monster) || wizard)
        remove_auto_exclude(monster);

          int  summon_type   = 0;
          int  duration      = 0;
    const bool summoned      = mons_is_summoned(monster, &duration,
                                                &summon_type);
    const int monster_killed = monster_index(monster);
    const bool hard_reset    = testbits(monster->flags, MF_HARD_RESET);
    const bool gives_xp      = !summoned;

    const bool drop_items    = !hard_reset;

    const bool mons_reset( killer == KILL_RESET || killer == KILL_DISMISSED );

    const bool submerged     = monster->submerged();

    bool in_transit          = false;

#ifdef DGL_MILESTONES
    _check_kill_milestone(monster, killer, killer_index);
#endif

    // Award experience for suicide if the suicide was caused by the
    // player.
    if (MON_KILL(killer) && monster_killed == killer_index)
    {
        if (monster->confused_by_you())
            killer = KILL_YOU_CONF;
    }
    else if (MON_KILL(killer) && monster->has_ench(ENCH_CHARM))
        killer = KILL_YOU_CONF; // Well, it was confused in a sense... (jpeg)

    // Take note!
    if (!mons_reset && !crawl_state.arena && MONST_INTERESTING(monster))
        take_note(Note(NOTE_KILL_MONSTER,
                       monster->type, mons_friendly(monster),
                       monster->name(DESC_NOCAP_A, true).c_str()));

    // From time to time Trog gives you a little bonus
    if (killer == KILL_YOU && you.duration[DUR_BERSERKER])
    {
        if (you.religion == GOD_TROG
            && !player_under_penance() && you.piety > random2(1000))
        {
            int bonus = 3 + random2avg( 10, 2 );

            you.duration[DUR_BERSERKER] += bonus;
            you.duration[DUR_MIGHT] += bonus;
            haste_player( bonus );

            mpr("You feel the power of Trog in you as your rage grows.",
                MSGCH_GOD, GOD_TROG);
        }
        else if (wearing_amulet( AMU_RAGE ) && one_chance_in(30))
        {
            int bonus = 2 + random2(4);

            you.duration[DUR_BERSERKER] += bonus;
            you.duration[DUR_MIGHT] += bonus;
            haste_player( bonus );

            mpr("Your amulet glows a violent red.");
        }
    }

    if (you.prev_targ == monster_killed)
    {
        you.prev_targ = MHITNOT;
        crawl_state.cancel_cmd_repeat();
    }

    if (killer == KILL_YOU)
        crawl_state.cancel_cmd_repeat();

    const bool pet_kill = _is_pet_kill(killer, killer_index);

    bool did_death_message = false;

    if (monster->type == MONS_GIANT_SPORE
        || monster->type == MONS_BALL_LIGHTNING)
    {
        did_death_message =
            _spore_goes_pop(monster, killer, killer_index, pet_kill, wizard);
    }
    else if (monster->type == MONS_FIRE_VORTEX
             || monster->type == MONS_SPATIAL_VORTEX)
    {
        if (!silent && !mons_reset)
        {
            simple_monster_message( monster, " dissipates!",
                                    MSGCH_MONSTER_DAMAGE, MDAM_DEAD );
            silent = true;
        }

        if (monster->type == MONS_FIRE_VORTEX && !wizard && !mons_reset
            && !submerged)
        {
            place_cloud(CLOUD_FIRE, monster->pos(), 2 + random2(4),
                        monster->kill_alignment());
        }

        if (killer == KILL_RESET)
            killer = KILL_DISMISSED;
    }
    else if (monster->type == MONS_SIMULACRUM_SMALL
             || monster->type == MONS_SIMULACRUM_LARGE)
    {
        if (!silent && !mons_reset)
        {
            simple_monster_message( monster, " vapourises!",
                                    MSGCH_MONSTER_DAMAGE,  MDAM_DEAD );
            silent = true;
        }

        if (!wizard && !mons_reset && !submerged)
            place_cloud(CLOUD_COLD, monster->pos(), 2 + random2(4),
                        monster->kill_alignment());

        if (killer == KILL_RESET)
            killer = KILL_DISMISSED;
    }
    else if (monster->type == MONS_DANCING_WEAPON)
    {
        if (!hard_reset)
        {
            if (killer == KILL_RESET)
                killer = KILL_DISMISSED;
        }

        if (!silent && !hard_reset)
        {
            int w_idx = monster->inv[MSLOT_WEAPON];
            if (w_idx != NON_ITEM && !(mitm[w_idx].flags & ISFLAG_SUMMONED))
            {
                simple_monster_message( monster, " falls from the air.",
                                        MSGCH_MONSTER_DAMAGE, MDAM_DEAD );
                silent = true;
            }
            else
                killer = KILL_RESET;
        }
    }

    bool death_message = !silent && !did_death_message && mons_near(monster)
                         && (player_monster_visible(monster)
                             || crawl_state.arena);

    switch (killer)
    {
        case KILL_YOU:          // You kill in combat.
        case KILL_YOU_MISSILE:  // You kill by missile or beam.
        case KILL_YOU_CONF:     // You kill by confusion.
        {
            const bool bad_kill    = god_hates_killing(you.religion, monster);
            const bool was_neutral = testbits(monster->flags, MF_WAS_NEUTRAL);
            const bool created_friendly =
                testbits(monster->flags, MF_CREATED_FRIENDLY);

            if (death_message)
            {
                bool passive = (killer == KILL_YOU_CONF
                                && (killer_index == ANON_FRIENDLY_MONSTER
                                    || !invalid_monster_index(killer_index)));

                if ( passive )
                {
                    mprf(MSGCH_MONSTER_DAMAGE, MDAM_DEAD, "%s is %s!",
                         monster->name(DESC_CAP_THE).c_str(),
                         _wounded_damaged(monster->type) ?
                         "destroyed" : "killed");
                }
                else
                {
                    mprf(MSGCH_MONSTER_DAMAGE, MDAM_DEAD, "You %s %s!",
                         _wounded_damaged(monster->type) ? "destroy" : "kill",
                         monster->name(DESC_NOCAP_THE).c_str());
                }

                if ((created_friendly || was_neutral) && gives_xp)
                    mpr("That felt strangely unrewarding.");
            }

            if (crawl_state.arena)
                break;

            // killing triggers tutorial lesson
            _tutorial_inspect_kill();

            // Prevent summoned creatures from being good kills.
            if (bad_kill || !created_friendly && gives_xp)
            {
                if (mons_holiness(monster) == MH_NATURAL)
                {
                    did_god_conduct(DID_KILL_LIVING,
                                    monster->hit_dice, true, monster);
                }

                if (mons_holiness(monster) == MH_UNDEAD)
                {
                    did_god_conduct(DID_KILL_UNDEAD,
                                    monster->hit_dice, true, monster);
                }

                if (mons_holiness(monster) == MH_DEMONIC)
                {
                    did_god_conduct(DID_KILL_DEMON,
                                    monster->hit_dice, true, monster);
                }

                if (mons_is_evil(monster)
                    && mons_holiness(monster) == MH_NATURAL)
                {
                    did_god_conduct(DID_KILL_NATURAL_EVIL,
                                    monster->hit_dice, true, monster);
                }

                if (mons_is_chaotic(monster))
                {
                    did_god_conduct(DID_KILL_CHAOTIC,
                                    monster->hit_dice, true, monster);
                }

                // jmf: Trog hates wizards.
                if (mons_is_magic_user(monster))
                {
                    did_god_conduct(DID_KILL_WIZARD,
                                    monster->hit_dice, true, monster);
                }

                // Beogh hates priests of other gods.
                if (mons_class_flag(monster->type, M_PRIEST))
                {
                    did_god_conduct(DID_KILL_PRIEST,
                                    monster->hit_dice, true, monster);
                }

                if (mons_is_holy(monster))
                {
                    did_god_conduct(DID_KILL_HOLY, monster->hit_dice,
                                    true, monster);
                }
            }

            // Divine health and mana restoration doesn't happen when
            // killing born-friendly monsters.  The mutation still
            // applies, however.
            if (player_mutation_level(MUT_DEATH_STRENGTH)
                || (!created_friendly && gives_xp
                    && (you.religion == GOD_MAKHLEB
                        || you.religion == GOD_SHINING_ONE
                           && mons_is_evil_or_unholy(monster))
                    && !player_under_penance()
                    && random2(you.piety) >= piety_breakpoint(0)))
            {
                if (you.hp < you.hp_max)
                {
                    mpr("You feel a little better.");
                    inc_hp(monster->hit_dice + random2(monster->hit_dice),
                           false);
                }
            }

            if (!created_friendly && gives_xp
                && (you.religion == GOD_MAKHLEB
                    || you.religion == GOD_VEHUMET
                    || you.religion == GOD_SHINING_ONE
                       && mons_is_evil_or_unholy(monster))
                && !player_under_penance()
                && random2(you.piety) >= piety_breakpoint(0))
            {
                if (you.magic_points < you.max_magic_points)
                {
                    mpr("You feel your power returning.");
                    inc_mp(1 + random2(monster->hit_dice / 2), false);
                }
            }

            // Randomly bless a follower.
            if (!created_friendly
                && gives_xp
                && (you.religion == GOD_BEOGH
                        && mons_holiness(monster) == MH_NATURAL
                        && random2(you.piety) >= piety_breakpoint(2))
                && !player_under_penance())
            {
                bless_follower();
            }

            if (you.duration[DUR_DEATH_CHANNEL]
                && mons_holiness(monster) == MH_NATURAL
                && mons_can_be_zombified(monster)
                && gives_xp)
            {
                const monster_type spectre_type = mons_species(monster->type);

                // Don't allow 0-headed hydras to become spectral hydras.
                if (spectre_type != MONS_HYDRA || monster->number != 0)
                {
                    const int spectre =
                        create_monster(
                            mgen_data(MONS_SPECTRAL_THING, BEH_FRIENDLY,
                                0, 0, monster->pos(), you.pet_target,
                                0, static_cast<god_type>(you.attribute[ATTR_DIVINE_DEATH_CHANNEL]),
                                spectre_type, monster->number));

                    if (spectre != -1)
                    {
                        if (death_message)
                            mpr("A glowing mist starts to gather...");

                        name_zombie(&menv[spectre], monster);
                    }
                }
            }
            break;
        }

        case KILL_MON:          // Monster kills in combat
        case KILL_MON_MISSILE:  // Monster kills by missile or beam
            if (!silent)
            {
                simple_monster_message(monster,
                                       _wounded_damaged(monster->type) ?
                                       " is destroyed!" : " dies!",
                                       MSGCH_MONSTER_DAMAGE,
                                       MDAM_DEAD);
            }

            if (crawl_state.arena)
                break;

            // No piety loss if god gifts killed by other monsters.
            // Also, dancing weapons aren't really friendlies.
            if (mons_friendly(monster)
                && !mons_is_god_gift(monster)
                && monster->type != MONS_DANCING_WEAPON)
            {
                did_god_conduct(DID_FRIEND_DIED, 1 + (monster->hit_dice / 2),
                                true, monster);
            }

            // Trying to prevent summoning abuse here, so we're trying to
            // prevent summoned creatures from being done_good kills. Only
            // affects creatures which were friendly when summoned.
            if (!testbits(monster->flags, MF_CREATED_FRIENDLY) && gives_xp
                && pet_kill)
            {
                bool notice = false;
                const bool anon = (killer_index == ANON_FRIENDLY_MONSTER);

                const mon_holy_type targ_holy = mons_holiness(monster),
                    attacker_holy = anon ? MH_NATURAL
                                         : mons_holiness(&menv[killer_index]);

                if (targ_holy == MH_NATURAL && attacker_holy == MH_UNDEAD)
                {
                    // Yes, this is a hack, but it makes sure that confused
                    // monsters doing the kill are not referred to as "slave",
                    // and I think it's okay that Yredelemnul ignores kills
                    // done by confused monsters as opposed to enslaved or
                    // friendly ones. (jpeg)
                    if (mons_friendly(&menv[killer_index]))
                    {
                        notice |=
                            did_god_conduct(DID_LIVING_KILLED_BY_UNDEAD_SLAVE,
                                            monster->hit_dice);
                    }
                    else
                    {
                        notice |=
                            did_god_conduct(DID_LIVING_KILLED_BY_SERVANT,
                                            monster->hit_dice);
                    }
                }
                else if (you.religion == GOD_SHINING_ONE
                         || you.religion == GOD_VEHUMET
                         || you.religion == GOD_MAKHLEB
                         || you.religion == GOD_LUGONU
                         || !anon && mons_is_god_gift(&menv[killer_index]))
                {
                    // Yes, we are splitting undead pets from the others
                    // as a way to focus Necromancy vs Summoning (ignoring
                    // Summon Wraith here)... at least we're being nice and
                    // putting the natural creature Summons together with
                    // the Demon ones.  Note that Vehumet gets a free
                    // pass here since those followers are assumed to
                    // come from Summoning spells...  the others are
                    // from invocations (Zin, TSO, Makh, Kiku). -- bwr
                    if (targ_holy == MH_NATURAL)
                    {
                        notice |= did_god_conduct(DID_LIVING_KILLED_BY_SERVANT,
                                                  monster->hit_dice);

                        if (mons_is_evil(monster))
                        {
                            notice |=
                                did_god_conduct(
                                        DID_NATURAL_EVIL_KILLED_BY_SERVANT,
                                        monster->hit_dice);
                        }
                    }
                    else if (targ_holy == MH_DEMONIC)
                    {
                        notice |= did_god_conduct(DID_DEMON_KILLED_BY_SERVANT,
                                                  monster->hit_dice);
                    }
                    else if (targ_holy == MH_UNDEAD)
                    {
                        notice |= did_god_conduct(DID_UNDEAD_KILLED_BY_SERVANT,
                                                  monster->hit_dice);
                    }
                }

                // Holy kills are always noticed.
                if (targ_holy == MH_HOLY)
                {
                    if (attacker_holy == MH_UNDEAD)
                    {
                        // Yes, this is a hack, but it makes sure that confused
                        // monsters doing the kill are not referred to as
                        // "slave", and I think it's okay that Yredelemnul
                        // ignores kills done by confused monsters as opposed
                        // to enslaved or friendly ones. (jpeg)
                        if (mons_friendly(&menv[killer_index]))
                        {
                            notice |=
                                did_god_conduct(DID_HOLY_KILLED_BY_UNDEAD_SLAVE,
                                                monster->hit_dice);
                        }
                        else
                        {
                            notice |=
                                did_god_conduct(DID_HOLY_KILLED_BY_SERVANT,
                                                monster->hit_dice);
                        }
                    }
                    else
                        notice |= did_god_conduct(DID_HOLY_KILLED_BY_SERVANT,
                                                  monster->hit_dice);
                }

                if (you.religion == GOD_VEHUMET
                    && notice
                    && !player_under_penance()
                    && random2(you.piety) >= piety_breakpoint(0))
                {
                    // Vehumet - only for non-undead servants (coding
                    // convenience, no real reason except that Vehumet
                    // prefers demons).
                    if (you.magic_points < you.max_magic_points)
                    {
                        mpr("You feel your power returning.");
                        inc_mp( 1 + random2(monster->hit_dice / 2), false );
                    }
                }

                if (you.religion == GOD_SHINING_ONE
                    && mons_is_evil_or_unholy(monster)
                    && !player_under_penance()
                    && random2(you.piety) >= piety_breakpoint(0)
                    && !invalid_monster_index(killer_index))
                {
                    monsters *mon = &menv[killer_index];

                    // Randomly bless the follower who killed.
                    if (!one_chance_in(3) && mon->alive()
                        && bless_follower(mon))
                    {
                        break;
                    }

                    if (mon->alive() && mon->hit_points < mon->max_hit_points)
                    {
                        simple_monster_message(mon, " looks invigorated.");
                        heal_monster( mon, 1 + random2(monster->hit_dice / 4),
                                      false );
                    }
                }

                // Randomly bless the follower who killed.
                if (you.religion == GOD_BEOGH
                    && mons_holiness(monster) == MH_NATURAL
                    && random2(you.piety) >= piety_breakpoint(2)
                    && !player_under_penance()
                    && !one_chance_in(3)
                    && !invalid_monster_index(killer_index))
                {
                    bless_follower(&menv[killer_index]);
                }

            }
            break;

        // Monster killed by trap/inanimate thing/itself/poison not from you.
        case KILL_MISC:
            if (!silent)
            {
                simple_monster_message(monster,
                                       _wounded_damaged(monster->type) ?
                                       " is destroyed!" : " dies!",
                                       MSGCH_MONSTER_DAMAGE,
                                       MDAM_DEAD);
            }
            break;

        case KILL_RESET:
            // Monster doesn't die, just goes back to wherever it came from
            // This must only be called by monsters running out of time (or
            // abjuration), because it uses the beam variables! Or does it???
            if (!wizard)

            // KILL_RESET monsters no longer lose their whole inventory, only
            // items they were generated with.
            if (!monster->needs_transit())
            {
                // A banished monster that doesn't go on the transit list
                // loses all items.
                if (!mons_is_summoned(monster))
                    monster->destroy_inventory();
                break;
            }

            // Monster goes to the Abyss.
            monster->flags |= MF_BANISHED;
            monster->set_transit( level_id(LEVEL_ABYSS) );
            in_transit = true;
            monster->destroy_inventory();
            // Make monster stop patrolling and/or travelling.
            monster->patrol_point.reset();
            monster->travel_path.clear();
            monster->travel_target = MTRAV_NONE;
            break;

        case KILL_DISMISSED:
            break;

        default:
            monster->destroy_inventory();
            break;
    }

    // Make sure Boris has a foe to address
    if (monster->foe == MHITNOT)
    {
        if (!mons_wont_attack(monster) && !crawl_state.arena)
            monster->foe = MHITYOU;
        else if (!invalid_monster_index(killer_index))
            monster->foe = killer_index;
    }

    if (!silent && !wizard && see_grid(monster->pos()))
    {
        // Make sure that the monster looks dead.
        if (monster->alive() && !in_transit && (!summoned || duration > 0))
            monster->hit_points = -1;
        mons_speaks(monster);
    }

    if (monster->type == MONS_BORIS && !in_transit)
    {
        // XXX: Actual blood curse effect for Boris? -- bwr

        // Now that Boris is dead, he's a valid target for monster
        // creation again. -- bwr
        you.unique_creatures[monster->type] = false;
    }
    else if (!mons_is_summoned(monster))
    {
        if (mons_genus(monster->type) == MONS_MUMMY)
            _mummy_curse(monster, killer, killer_index);
    }

    if (!wizard && !submerged)
        _monster_die_cloud(monster, !mons_reset, silent, summoned, summon_type);

    int corpse = -1;
    if (!mons_reset)
    {
        // Have to add case for disintegration effect here? {dlb}
        if (!summoned)
            corpse = place_monster_corpse(monster, silent);
    }

    if (!mons_reset && !crawl_state.arena)
    {
        you.kills->record_kill(monster, killer, pet_kill);

        kill_category kc =
            (killer == KILL_YOU || killer == KILL_YOU_MISSILE) ? KC_YOU :
            (pet_kill)?                                          KC_FRIENDLY :
                                                                 KC_OTHER;

        unsigned int exp_gain = 0, avail_gain = 0;
        _give_adjusted_experience(monster, killer, pet_kill, killer_index,
                                  &exp_gain, &avail_gain);

        PlaceInfo& curr_PlaceInfo = you.get_place_info();
        PlaceInfo  delta;

        delta.mon_kill_num[kc]++;
        delta.mon_kill_exp       += exp_gain;
        delta.mon_kill_exp_avail += avail_gain;

        you.global_info += delta;
        you.global_info.assert_validity();

        curr_PlaceInfo += delta;
        curr_PlaceInfo.assert_validity();
    }

    _fire_monster_death_event(monster, killer, killer_index, false);

    if (crawl_state.arena)
        arena_monster_died(monster, killer, killer_index, silent, corpse);

    const coord_def mwhere = monster->pos();
    if (drop_items)
        monster_drop_ething(monster, YOU_KILL(killer) || pet_kill);
    else
    {
        // Destroy the items belonging to MF_HARD_RESET monsters so they
        // don't clutter up mitm[]
        monster->destroy_inventory();
    }

    if (!silent && !wizard && !mons_reset
        && !(monster->flags & MF_KNOWN_MIMIC)
        && mons_is_shapeshifter(monster))
    {
        simple_monster_message(monster, "'s shape twists and changes "
                               "as it dies; that was a shifter!");
    }

    crawl_state.dec_mon_acting(monster);
    monster_cleanup(monster);

    // Force redraw for monsters that die.
    if (see_grid(mwhere))
    {
        view_update_at(mwhere);
        update_screen();
    }

    return (corpse);
}

void monster_cleanup(monsters *monster)
{
    crawl_state.mon_gone(monster);

    unsigned int monster_killed = monster_index(monster);
    monster->reset();

    for (int dmi = 0; dmi < MAX_MONSTERS; dmi++)
        if (menv[dmi].foe == monster_killed)
            menv[dmi].foe = MHITNOT;

    if (you.pet_target == monster_killed)
        you.pet_target = MHITNOT;
}

static bool _jelly_divide(monsters *parent)
{
    if (!mons_class_flag(parent->type, M_SPLITS))
        return (false);

    const int reqd = std::max(parent->hit_dice * 8, 50);
    if (parent->hit_points < reqd)
        return (false);

    monsters *child = NULL;
    coord_def child_spot;
    int num_spots = 0;

    // First, find a suitable spot for the child {dlb}:
    for (adjacent_iterator ai(parent->pos()); ai; ++ai)
    {
        if (mgrd(*ai) == NON_MONSTER
            && parent->can_pass_through(*ai)
            && (*ai != you.pos()))
        {
            if ( one_chance_in(++num_spots) )
                child_spot = *ai;
        }
    }

    if ( num_spots == 0 )
        return (false);

    int k = 0;

    // Now that we have a spot, find a monster slot {dlb}:
    for (k = 0; k < MAX_MONSTERS; k++)
    {
        child = &menv[k];

        if (child->type == -1)
            break;
        else if (k == MAX_MONSTERS - 1)
            return (false);
    }

    // Handle impact of split on parent {dlb}:
    parent->max_hit_points /= 2;

    if (parent->hit_points > parent->max_hit_points)
        parent->hit_points = parent->max_hit_points;

    parent->init_experience();
    parent->experience = parent->experience * 3 / 5 + 1;

    // Create child {dlb}:
    // This is terribly partial and really requires
    // more thought as to generation ... {dlb}
    *child = *parent;
    child->max_hit_points  = child->hit_points;
    child->speed_increment = 70 + random2(5);
    child->moveto(child_spot);

    mgrd(child->pos()) = k;

    if (!simple_monster_message(parent, " splits in two!"))
        if (player_can_hear(parent->pos()) || player_can_hear(child->pos()))
            mpr("You hear a squelching noise.", MSGCH_SOUND);

    if (crawl_state.arena)
        arena_placed_monster(child);

    return (true);
}

// If you're invis and throw/zap whatever, alerts menv to your position.
void alert_nearby_monsters(void)
{
    monsters *monster = 0;       // NULL {dlb}

    for (int it = 0; it < MAX_MONSTERS; it++)
    {
        monster = &menv[it];

        // Judging from the above comment, this function isn't
        // intended to wake up monsters, so we're only going to
        // alert monsters that aren't sleeping.  For cases where an
        // event should wake up monsters and alert them, I'd suggest
        // calling noisy() before calling this function. -- bwr
        if (monster->type != -1
            && mons_near(monster)
            && !mons_is_sleeping(monster))
        {
            behaviour_event(monster, ME_ALERT, MHITYOU);
        }
    }
}

static bool _valid_morph( monsters *monster, int new_mclass )
{
    const dungeon_feature_type current_tile = grd(monster->pos());

    // 'morph targets are _always_ "base" classes, not derived ones.
    new_mclass = mons_species(new_mclass);

    // [ds] Non-base draconians are much more trouble than their HD
    // suggests.
    if (mons_genus(new_mclass) == MONS_DRACONIAN
        && new_mclass != MONS_DRACONIAN
        && !player_in_branch(BRANCH_HALL_OF_ZOT)
        && !one_chance_in(10))
    {
        return (false);
    }

    // Various inappropriate polymorph targets.
    if (mons_class_holiness( new_mclass ) != mons_holiness( monster )
        || mons_class_flag( new_mclass, M_UNIQUE)       // no uniques
        || mons_class_flag( new_mclass, M_NO_EXP_GAIN ) // not helpless
        || new_mclass == mons_species( monster->type )  // must be different
        || new_mclass == MONS_PROGRAM_BUG
        || new_mclass == MONS_SHAPESHIFTER
        || new_mclass == MONS_GLOWING_SHAPESHIFTER

        // These require manual setting of mons.base_monster to indicate
        // what they are a skeleton/zombie/simulacrum/spectral thing of,
        // which we currently aren't smart enough to handle.
        || mons_class_is_zombified(new_mclass)

        // These shouldn't happen anyways (demons unaffected + holiness check),
        // but if we ever do have polydemon, these will be needed:
        || new_mclass == MONS_PLAYER_GHOST
        || new_mclass == MONS_PANDEMONIUM_DEMON

        // Only for use by game testers or in the arena.
        || new_mclass == MONS_TEST_SPAWNER

        // Other poly-unsuitable things.
        || new_mclass == MONS_ORB_GUARDIAN
        || new_mclass == MONS_ORANGE_STATUE
        || new_mclass == MONS_SILVER_STATUE
        || new_mclass == MONS_ICE_STATUE)
    {
        return (false);
    }

    // Determine if the monster is happy on current tile.
    return (monster_habitable_grid(new_mclass, current_tile));
}

static bool _is_poly_power_unsuitable( poly_power_type power,
                                       int src_pow, int tgt_pow, int relax )
{
    switch (power)
    {
    case PPT_LESS:
        return (tgt_pow > src_pow - 3 + (relax * 3) / 2)
                || (power == PPT_LESS && (tgt_pow < src_pow - (relax / 2)));
    case PPT_MORE:
        return (tgt_pow < src_pow + 2 - relax)
                || (power == PPT_MORE && (tgt_pow > src_pow + relax));
    default:
    case PPT_SAME:
        return (tgt_pow < src_pow - relax)
                || (tgt_pow > src_pow + (relax * 3) / 2);
    }
}

// If targetc == RANDOM_MONSTER, then relpower indicates the desired
// power of the new monster, relative to the current monster.
// Relaxation still takes effect when needed, no matter what relpower
// says.
bool monster_polymorph(monsters *monster, monster_type targetc,
                       poly_power_type power,
                       bool force_beh)
{
    ASSERT(!(monster->flags & MF_TAKING_STAIRS));
    ASSERT(!(monster->flags & MF_BANISHED) || you.level_type == LEVEL_ABYSS);

    std::string str_polymon;
    int source_power, target_power, relax;
    int tries = 1000;

    // Used to be mons_power, but that just returns hit_dice
    // for the monster class.  By using the current hit dice
    // the player gets the opportunity to use draining more
    // effectively against shapeshifters. -- bwr
    source_power = monster->hit_dice;
    relax = 1;

    if (targetc == RANDOM_MONSTER)
    {
        do
        {
            // Pick a monster that's guaranteed happy at this grid.
            targetc = random_monster_at_grid(monster->pos());

            // Valid targets are always base classes ([ds] which is unfortunate
            // in that well-populated monster classes will dominate polymorphs).
            targetc = mons_species(targetc);

            target_power = mons_power(targetc);

            if (one_chance_in(200))
                relax++;

            if (relax > 50)
                return (simple_monster_message( monster, " shudders."));
        }
        while (tries-- && (!_valid_morph(monster, targetc)
                           || _is_poly_power_unsuitable(power, source_power,
                                                        target_power, relax)));
    }

    if (!_valid_morph(monster, targetc))
        return simple_monster_message(monster, " looks momentarily different.");

    // Messaging.
    bool can_see = you.can_see(monster);

    // If old monster is visible to the player, and is interesting,
    // then note why the interesting monster went away.
    if (can_see && MONST_INTERESTING(monster))
    {
        take_note(Note(NOTE_POLY_MONSTER, monster->type, 0,
                       monster->name(DESC_CAP_A, true).c_str()));
    }

    if (monster->type == MONS_OGRE && targetc == MONS_TWO_HEADED_OGRE)
        str_polymon = " grows a second head!";
    else
    {
        if (mons_is_shapeshifter(monster))
            str_polymon = " changes into ";
        else if (targetc == MONS_PULSATING_LUMP)
            str_polymon = " degenerates into ";
        else
            str_polymon = " evaporates and reforms as ";

        if (!can_see)
            str_polymon += "something you cannot see!";
        else
        {
            str_polymon += mons_type_name(targetc, DESC_NOCAP_A);

            if (targetc == MONS_PULSATING_LUMP)
                str_polymon += " of flesh";

            str_polymon += "!";
        }
    }
    bool player_messaged = can_see
                       && simple_monster_message(monster, str_polymon.c_str());

    // Even if the monster transforms from one type that can behold the
    // player into a different type which can also behold the player,
    // the polymorph disrupts the beholding process.  Do this before
    // changing monster->type, since unbeholding can only happen while
    // the monster is still a mermaid/siren.
    update_beholders(monster, true);

    // Inform listeners that the original monster is gone.
    _fire_monster_death_event(monster, KILL_MISC, NON_MONSTER, true);

    // the actual polymorphing:
    unsigned long flags =
        monster->flags & ~(MF_INTERESTING | MF_SEEN | MF_ATT_CHANGE_ATTEMPT
                           | MF_WAS_IN_VIEW | MF_BAND_MEMBER
                           | MF_HONORARY_UNDEAD | MF_KNOWN_MIMIC);

    std::string name;

    // Preserve the names of uniques and named monsters.
    if (!monster->mname.empty())
        name = monster->mname;
    else if (mons_is_unique(monster->type))
    {
        flags |= MF_INTERESTING;

        name = monster->name(DESC_PLAIN, true);
        if (monster->type == MONS_ROYAL_JELLY)
        {
            name   = "shaped Royal Jelly";
            flags |= MF_NAME_SUFFIX;
        }
        else if (monster->type == MONS_LERNAEAN_HYDRA)
        {
            name   = "shaped Lernaean hydra";
            flags |= MF_NAME_SUFFIX;
        }

        // "Blork the orc" and similar.
        const size_t the_pos = name.find(" the ");
        if (the_pos != std::string::npos)
            name = name.substr(0, the_pos);
    }

    const int  old_hp     = monster->hit_points;
    const int  old_hp_max = monster->max_hit_points;
    const bool old_mon_caught = mons_is_caught(monster);
    const char old_ench_countdown = monster->ench_countdown;

    mon_enchant abj       = monster->get_ench(ENCH_ABJ);
    mon_enchant charm     = monster->get_ench(ENCH_CHARM);
    mon_enchant neutral   = monster->get_ench(ENCH_NEUTRAL);
    mon_enchant shifter   = monster->get_ench(ENCH_GLOWING_SHAPESHIFTER,
                                              ENCH_SHAPESHIFTER);
    mon_enchant summon    = monster->get_ench(ENCH_SUMMON);
    mon_enchant tp        = monster->get_ench(ENCH_TP);

    // deal with mons_sec
    monster->type         = targetc;
    monster->base_monster = MONS_PROGRAM_BUG;
    monster->number       = 0;

    // Note: define_monster() will clear out all enchantments! -- bwr
    define_monster( monster_index(monster) );

    monster->flags = flags;
    monster->mname = name;

    monster->add_ench(abj);
    monster->add_ench(charm);
    monster->add_ench(neutral);
    monster->add_ench(shifter);
    monster->add_ench(summon);
    monster->add_ench(tp);

    monster->ench_countdown = old_ench_countdown;

    if (mons_class_flag(monster->type, M_INVIS))
        monster->add_ench(ENCH_INVIS);

    if (!player_messaged && you.can_see(monster))
    {
        mprf("%s appears out of thin air!", monster->name(DESC_CAP_A).c_str());
        player_messaged = true;
    }

    monster->hit_points = monster->max_hit_points
                                * ((old_hp * 100) / old_hp_max) / 100
                                + random2(monster->max_hit_points);

    monster->hit_points = std::min(monster->max_hit_points,
                                   monster->hit_points);

    monster->speed_increment = 67 + random2(6);

    monster_drop_ething(monster);

    // New monster type might be interesting.
    mark_interesting_monst(monster);

    // If new monster is visible to player, then we've seen it.
    if (player_monster_visible(monster) && mons_near(monster))
    {
        seen_monster(monster);
        // If the player saw both the begining and end results of a shifter
        // changing then he/seh knows it must be a shifter.
        if (can_see && shifter.ench != ENCH_NONE)
            monster->flags |= MF_KNOWN_MIMIC;
    }

    if (old_mon_caught)
        check_net_will_hold_monster(monster);

    if (!force_beh)
        player_angers_monster(monster);

    return (player_messaged);
}

// If the returned value is mon.pos(), then nothing was found.
static coord_def _random_monster_nearby_habitable_space(const monsters& mon,
                                                        bool allow_adjacent,
                                                        bool respect_los)
{
    const bool respect_sanctuary = mons_wont_attack(&mon);

    coord_def target;
    int tries;

    for (tries = 0; tries < 150; ++tries)
    {
        const coord_def delta(random2(13) - 6, random2(13) - 6);

        // Check that we don't get something too close to the
        // starting point.
        if (delta.origin())
            continue;

        if (delta.rdist() == 1 && !allow_adjacent)
            continue;

        // Update target.
        target = delta + mon.pos();

        // Check that the target is valid and survivable.
        if (!in_bounds(target))
            continue;

        if (!monster_habitable_grid(&mon, grd(target)))
            continue;

        if (respect_sanctuary && is_sanctuary(target))
            continue;

        if (target == you.pos())
            continue;

        // Check that we didn't go through clear walls.
        if (num_feats_between(mon.pos(), target,
                              DNGN_CLEAR_ROCK_WALL,
                              DNGN_CLEAR_PERMAROCK_WALL,
                              true, true) > 0)
        {
            continue;
        }

        if (respect_los && !mon.mon_see_grid(target))
            continue;

        // Survived everything, break out (with a good value of target.)
        break;
    }

    if (tries == 150)
        target = mon.pos();

    return (target);
}

bool monster_blink(monsters *monster)
{
    coord_def near = _random_monster_nearby_habitable_space(*monster, false,
                                                            true);
    if (near == monster->pos())
        return (false);

    if (!(monster->flags & MF_WAS_IN_VIEW))
        monster->seen_context = "thin air";

    mons_clear_trapping_net(monster);

    const coord_def oldplace = monster->pos();
    if (!monster->move_to_pos(near))
        return (false);

    monster->check_redraw(oldplace);
    monster->apply_location_effects(oldplace);

    return (true);
}

static void _set_random_target(monsters* mon)
{
    mon->target = random_in_bounds(); // If we don't find anything better
    for (int tries = 0; tries < 150; ++tries)
    {
        coord_def delta = coord_def(random2(13), random2(13)) - coord_def(6,6);
        if (delta.origin())
            continue;

        const coord_def newtarget = delta + mon->pos();
        if (!in_bounds(newtarget))
            continue;

        mon->target = newtarget;
        break;
    }
}

// allow_adjacent:  allow target to be adjacent to origin.
// restrict_LOS:    restrict target to be within PLAYER line of sight.
bool random_near_space(const coord_def& origin, coord_def& target,
                       bool allow_adjacent, bool restrict_LOS,
                       bool forbid_sanctuary)
{
    // This might involve ray tracing (via num_feats_between()), so
    // cache results to avoid duplicating ray traces.
    FixedArray<bool, 13, 13> tried;
    tried.init(false);

    // Is the monster on the other side of a transparent wall?
    const bool trans_wall_block  = trans_wall_blocking(origin);
    const bool origin_is_player  = (you.pos() == origin);
    int min_walls_between = 0;

    // Skip ray tracing if possible.
    if (trans_wall_block)
    {
        min_walls_between = num_feats_between(origin, you.pos(),
                                              DNGN_CLEAR_ROCK_WALL,
                                              DNGN_CLEAR_PERMAROCK_WALL);
    }

    int tries = 0;
    while (tries++ < 150)
    {
        coord_def delta(random2(13), random2(13));

        target = origin - coord_def(6, 6) + delta;

        // Origin is not 'near'.
        if (target == origin)
            continue;

        if (tried(delta))
            continue;

        tried(delta) = true;

        if (!in_bounds(target)
            || restrict_LOS && !see_grid(target)
            || grd(target) < DNGN_SHALLOW_WATER
            || mgrd(target) != NON_MONSTER
            || target == you.pos()
            || !allow_adjacent && distance(origin, target) <= 2
            || forbid_sanctuary && is_sanctuary(target))
        {
            continue;
        }

        if (!trans_wall_block && !origin_is_player)
            return (true);

        // If the monster is on a visible square which is on the other
        // side of one or more translucent from the player, then it
        // can only blink through translucent walls if the end point
        // is either not visible to the player, or there are at least
        // as many translucent walls between the player and the end
        // point as between the player and the start point.  However,
        // monsters can still blink through translucent walls to get
        // away from the player, since in the absence of translucent
        // walls monsters can blink to places which are not in either
        // the monster's nor the player's LOS.
        if (!origin_is_player && !see_grid(target))
            return (true);

        // Player can't randomly pass through translucent walls.
        if (origin_is_player)
        {
            if (see_grid_no_trans(target))
                return (true);

            continue;
        }

        int walls_passed = num_feats_between(target, origin,
                                             DNGN_CLEAR_ROCK_WALL,
                                             DNGN_CLEAR_PERMAROCK_WALL,
                                             true, true);
        if (walls_passed == 0)
            return (true);

        // Player can't randomly pass through translucent walls.
        if (origin_is_player)
            continue;

        int walls_between = num_feats_between(target, you.pos(),
                                              DNGN_CLEAR_ROCK_WALL,
                                              DNGN_CLEAR_PERMAROCK_WALL);

        if (walls_between >= min_walls_between)
            return (true);
    }

    return (false);
}

static bool _habitat_okay( const monsters *monster, dungeon_feature_type targ )
{
    return (monster_habitable_grid(monster, targ));
}

// This doesn't really swap places, it just sets the monster's
// position equal to the player (the player has to be moved afterwards).
// It also has a slight problem with the fact that if the player is
// levitating over an inhospitable habitat for the monster the monster
// will be put in a place it normally couldn't go (this could be a
// feature because it prevents insta-killing).  In order to prevent
// that little problem, we go looking for a square for the monster
// to "scatter" to instead... and if we can't find one the monster
// just refuses to be swapped (not a bug, this is intentionally
// avoiding the insta-kill).  Another option is to look a bit
// wider for a vaild square (either by a last attempt blink, or
// by looking at a wider radius)...  insta-killing should be a
// last resort in this function (especially since Tome, Dig, and
// Summoning can be used to set up death traps).  If worse comes
// to worse, at least consider making the Swap spell not work
// when the player is over lava or water (if the player wants to
// swap pets to their death, we can let that go). -- bwr
bool swap_places(monsters *monster)
{
    coord_def loc;
    if (swap_check(monster, loc))
    {
        swap_places(monster, loc);
        return true;
    }
    return false;
}

// Swap monster to this location.  Player is swapped elsewhere.
bool swap_places(monsters *monster, const coord_def &loc)
{
    ASSERT(map_bounds(loc));
    ASSERT(_habitat_okay(monster, grd(loc)));

    if (mgrd(loc) != NON_MONSTER)
    {
        mpr("Something prevents you from swapping places.");
        return (false);
    }

    mpr("You swap places.");

    mgrd(monster->pos()) = NON_MONSTER;

    monster->moveto(loc);

    mgrd(monster->pos()) = monster_index(monster);

    return true;
}

// Returns true if this is a valid swap for this monster.  If true, then
// the valid location is set in loc. (Otherwise loc becomes garbage.)
bool swap_check(monsters *monster, coord_def &loc, bool quiet)
{
    loc = you.pos();

    // Don't move onto dangerous terrain.
    if (is_grid_dangerous(grd(monster->pos())))
    {
        canned_msg(MSG_UNTHINKING_ACT);
        return (false);
    }

    if (mons_is_caught(monster))
    {
        if (!quiet)
            simple_monster_message(monster, " is held in a net!");
        return (false);
    }

    // First try: move monster onto your position.
    bool swap = _habitat_okay( monster, grd(loc) );

    // Choose an appropriate habitat square at random around the target.
    if (!swap)
    {
        int num_found = 0;

        for (adjacent_iterator ai; ai; ++ai)
            if (mgrd(*ai) == NON_MONSTER && _habitat_okay( monster, grd(*ai))
                && one_chance_in(++num_found))
            {
                loc = *ai;
            }

        if (num_found)
            swap = true;
    }

    if (!swap && !quiet)
    {
        // Might not be ideal, but it's better than insta-killing
        // the monster... maybe try for a short blink instead? -- bwr
        simple_monster_message( monster, " resists." );
        // FIXME: AI_HIT_MONSTER isn't ideal.
        interrupt_activity( AI_HIT_MONSTER, monster );
    }

    return (swap);
}

void mons_get_damage_level( const monsters* monster, std::string& desc,
                            mon_dam_level_type& dam_level )
{
    if (monster->hit_points <= monster->max_hit_points / 6)
    {
        desc += "almost ";
        desc += _wounded_damaged(monster->type) ? "destroyed" : "dead";
        dam_level = MDAM_ALMOST_DEAD;
        return;
    }

    if (monster->hit_points <= monster->max_hit_points / 4)
    {
        desc += "severely ";
        dam_level = MDAM_SEVERELY_DAMAGED;
    }
    else if (monster->hit_points <= monster->max_hit_points / 3)
    {
        desc += "heavily ";
        dam_level = MDAM_HEAVILY_DAMAGED;
    }
    else if (monster->hit_points <= (monster->max_hit_points * 3) / 4)
    {
        desc += "moderately ";
        dam_level = MDAM_MODERATELY_DAMAGED;
    }
    else if (monster->hit_points < monster->max_hit_points)
    {
        desc += "lightly ";
        dam_level = MDAM_LIGHTLY_DAMAGED;
    }
    else
    {
        desc += "not ";
        dam_level = MDAM_OKAY;
    }

    desc += _wounded_damaged(monster->type) ? "damaged" : "wounded";
}

void print_wounds(const monsters *monster)
{
    if (!monster->alive() || monster->hit_points == monster->max_hit_points)
        return;

    if (monster_descriptor(monster->type, MDSC_NOMSG_WOUNDS))
        return;

    std::string desc;
    mon_dam_level_type dam_level;
    mons_get_damage_level(monster, desc, dam_level);

    desc.insert(0, " is ");
    desc += ".";
    simple_monster_message(monster, desc.c_str(), MSGCH_MONSTER_DAMAGE,
                           dam_level);
}

// (true == 'damaged') [constructs, undead, etc.]
// and (false == 'wounded') [living creatures, etc.] {dlb}
static bool _wounded_damaged(int monster_type)
{
    // this schema needs to be abstracted into real categories {dlb}:
    const mon_holy_type holy = mons_class_holiness(monster_type);
    if (holy == MH_UNDEAD || holy == MH_NONLIVING || holy == MH_PLANT)
        return (true);

    return (false);
}                               // end _wounded_damaged()

//---------------------------------------------------------------
//
// behaviour_event
//
// 1. Change any of: monster state, foe, and attitude
// 2. Call handle_behaviour to re-evaluate AI state and target x,y
//
//---------------------------------------------------------------
void behaviour_event(monsters *mon, int event, int src,
                     coord_def src_pos)
{
    ASSERT(src >= 0 && src <= MHITYOU);
    ASSERT(!crawl_state.arena || src != MHITYOU);
    ASSERT(in_bounds(src_pos) || src_pos.origin());

    beh_type old_behaviour = mon->behaviour;

    bool isSmart          = (mons_intel(mon) > I_ANIMAL);
    bool wontAttack       = mons_wont_attack_real(mon);
    bool sourceWontAttack = false;
    bool setTarget        = false;
    bool breakCharm       = false;

    if (src == MHITYOU)
        sourceWontAttack = true;
    else if (src != MHITNOT)
        sourceWontAttack = mons_wont_attack_real( &menv[src] );

    switch (event)
    {
    case ME_DISTURB:
        // Assumes disturbed by noise...
        if (mons_is_sleeping(mon))
        {
            mon->behaviour = BEH_WANDER;

            if (mons_near(mon))
                remove_auto_exclude(mon, true);
        }

        // A bit of code to make Project Noise actually do
        // something again.  Basically, dumb monsters and
        // monsters who aren't otherwise occupied will at
        // least consider the (apparent) source of the noise
        // interesting for a moment. -- bwr
        if (!isSmart || mon->foe == MHITNOT || mons_is_wandering(mon))
        {
            if (mon->is_patrolling())
                break;

            ASSERT(!src_pos.origin());
            mon->target = src_pos;
        }
        break;

    case ME_WHACK:
    case ME_ANNOY:
        // Will turn monster against <src>, unless they
        // are BOTH friendly or good neutral AND stupid,
        // or else fleeing anyway.  Hitting someone over
        // the head, of course, always triggers this code.
        if (event == ME_WHACK
            || ((wontAttack != sourceWontAttack || isSmart)
                && !mons_is_fleeing(mon) && !mons_is_panicking(mon)))
        {
            // (Plain) plants and fungi cannot fight back.
            if (mon->type == MONS_FUNGUS || mon->type == MONS_PLANT)
                return;

            mon->foe = src;

            if (mons_is_sleeping(mon) && mons_near(mon))
                remove_auto_exclude(mon, true);

            if (!mons_is_cornered(mon))
                mon->behaviour = BEH_SEEK;

            if (src == MHITYOU)
            {
                mon->attitude = ATT_HOSTILE;
                breakCharm    = true;
            }
        }

        // Now set target x, y so that monster can whack
        // back (once) at an invisible foe.
        if (event == ME_WHACK)
            setTarget = true;
        break;

    case ME_ALERT:
        if (mons_friendly(mon) && mon->is_patrolling())
            break;

        if (mons_is_sleeping(mon) && mons_near(mon))
            remove_auto_exclude(mon, true);

        // Will alert monster to <src> and turn them
        // against them, unless they have a current foe.
        // It won't turn friends hostile either.
        if (!mons_is_fleeing(mon) && !mons_is_panicking(mon)
            && !mons_is_cornered(mon))
        {
            mon->behaviour = BEH_SEEK;
        }

        if (mon->foe == MHITNOT)
            mon->foe = src;

        if (!src_pos.origin()
            && (mon->foe == MHITNOT || mon->foe == src
                || mons_is_wandering(mon)))
        {
            if (mon->is_patrolling())
                break;

            mon->target = src_pos;

            // XXX: Should this be done in _handle_behaviour()?
            if (src == MHITYOU && src_pos == you.pos()
                && !see_grid(mon->pos()))
            {
                const dungeon_feature_type can_move =
                    (mons_amphibious(mon)) ? DNGN_DEEP_WATER
                                           : DNGN_SHALLOW_WATER;

                _try_pathfind(mon, can_move, true);
            }
        }
        break;

    case ME_SCARE:
    {
        const bool flee_sanct = !mons_wont_attack(mon)
                                && is_sanctuary(mon->pos());

        // Stationary monsters can't flee, even from sanctuary.
        if (mons_is_stationary(mon))
        {
            mon->del_ench(ENCH_FEAR, true, true);
            break;
        }

        // Berserking monsters don't flee, unless it's from sanctuary.
        if (mon->has_ench(ENCH_BERSERK) && !flee_sanct)
            break;

        // Neither do plants or nonliving beings, and sanctuary doesn't
        // affect plants.
        if (mons_class_holiness(mon->type) == MH_PLANT
            || (mons_class_holiness(mon->type) == MH_NONLIVING && !flee_sanct))
        {
            mon->del_ench(ENCH_FEAR, true, true);
            break;
        }

        mon->foe = src;
        mon->behaviour = BEH_FLEE;
        // Assume monsters know where to run from, even if player is
        // invisible.
        setTarget = true;
        if (see_grid(mon->pos()))
            learned_something_new(TUT_FLEEING_MONSTER);
        break;
    }

    case ME_CORNERED:
        // Some monsters can't flee.
        if (mon->behaviour != BEH_FLEE && !mon->has_ench(ENCH_FEAR))
            break;

        // Don't stop fleeing from sanctuary.
        if (!mons_wont_attack(mon) && is_sanctuary(mon->pos()))
            break;

        // Pacified monsters shouldn't change their behaviour.
        if (mons_is_pacified(mon))
            break;

        // Just set behaviour... foe doesn't change.
        if (!mons_is_cornered(mon))
            simple_monster_message(mon, " turns to fight!");

        mon->behaviour = BEH_CORNERED;
        break;

    case ME_EVAL:
    default:
        break;
    }

    if (setTarget)
    {
        if (src == MHITYOU)
        {
            mon->target = you.pos();
            mon->attitude = ATT_HOSTILE;
        }
        else if (src != MHITNOT)
        {
            mon->target = menv[src].pos();
        }
    }

    // Now, break charms if appropriate.
    if (breakCharm)
        mon->del_ench(ENCH_CHARM);

    // Do any resultant foe or state changes.
    _handle_behaviour(mon);
    ASSERT(in_bounds(mon->target) || mon->target.origin());

    const bool wasLurking =
        (old_behaviour == BEH_LURK && !mons_is_lurking(mon));
    const bool isPacified = mons_is_pacified(mon);

    if ((wasLurking || isPacified)
        && (event == ME_DISTURB || event == ME_ALERT || event == ME_EVAL))
    {
        // Lurking monsters or pacified monsters leaving the level won't
        // stop doing so just because they noticed something.
        mon->behaviour = old_behaviour;
    }
    else if (wasLurking && mon->has_ench(ENCH_SUBMERGED)
             && !mon->del_ench(ENCH_SUBMERGED))
    {
        // The same goes for lurking submerged monsters, if they can't
        // unsubmerge.
        mon->behaviour = BEH_LURK;
    }

    ASSERT(!crawl_state.arena
           || mon->foe != MHITYOU && mon->target != you.pos());
}

static bool _choose_random_patrol_target_grid(monsters *mon)
{
    const int intel = mons_intel(mon);

    // Zombies will occasionally just stand around.
    // This does not mean that they don't move every second turn. Rather,
    // once they reach their chosen target, there's a 50% chance they'll
    // just remain there until next turn when this function is called
    // again.
    if (intel == I_PLANT && coinflip())
        return (true);

    // If there's no chance we'll find the patrol point, quit right away.
    if (grid_distance(mon->pos(), mon->patrol_point) > 2 * LOS_RADIUS)
        return (false);

    // Can the monster see the patrol point from its current position?
    const bool patrol_seen = mon->mon_see_grid(mon->patrol_point,
                                 habitat2grid(mons_primary_habitat(mon)));

    if (intel == I_PLANT && !patrol_seen)
    {
        // Really stupid monsters won't even try to get back into the
        // patrol zone.
        return (false);
    }

    // While the patrol point is in easy reach, monsters of insect/plant
    // intelligence will only use a range of 5 (distance from the patrol point).
    // Otherwise, try to get back using the full LOS.
    const int  rad      = (intel >= I_ANIMAL || !patrol_seen) ? LOS_RADIUS : 5;
    const bool is_smart = (intel >= I_NORMAL);

    monster_los patrol;
    // Set monster to make monster_los respect habitat restrictions.
    patrol.set_monster(mon);
    patrol.set_los_centre(mon->patrol_point);
    patrol.set_los_range(rad);
    patrol.fill_los_field();

    monster_los lm;
    if (is_smart || !patrol_seen)
    {
        // For stupid monsters, don't bother if the patrol point is in sight.
        lm.set_monster(mon);
        lm.fill_los_field();
    }

    int count_grids = 0;
    for (radius_iterator ri(mon->patrol_point, LOS_RADIUS, true, false);
         ri; ++ri)
    {
        // Don't bother for the current position. If everything fails,
        // we'll stay here anyway.
        if (*ri == mon->pos())
            continue;

        if (!mon->can_pass_through_feat(grd(*ri)))
            continue;

        // Don't bother moving to squares (currently) occupied by a
        // monster. We'll usually be able to find other target squares
        // (and if we're not, we couldn't move anyway), and this avoids
        // monsters trying to move onto a grid occupied by a plant or
        // sleeping monster.
        if (mgrd(*ri) != NON_MONSTER)
            continue;

        if (patrol_seen)
        {
            // If the patrol point can be easily (within LOS) reached
            // from the current position, it suffices if the target is
            // within reach of the patrol point OR the current position:
            // we can easily get there.
            // Only smart monsters will even attempt to move out of the
            // patrol area.
            // NOTE: Either of these can take us into a position where the
            // target cannot be easily reached (e.g. blocked by a wall)
            // and the patrol point is out of sight, too. Such a case
            // will be handled below, though it might take a while until
            // a monster gets out of a deadlock. (5% chance per turn.)
            if (!patrol.in_sight(*ri) && (!is_smart || !lm.in_sight(*ri)))
                continue;
        }
        else
        {
            // If, however, the patrol point is out of reach, we have to
            // make sure the new target brings us into reach of it.
            // This means that the target must be reachable BOTH from
            // the patrol point AND the current position.
            if (!patrol.in_sight(*ri) || !lm.in_sight(*ri))
                continue;

            // If this fails for all surrounding squares (probably because
            // we're too far away), we fall back to heading directly for
            // the patrol point.
        }

        bool set_target = false;
        if (intel == I_PLANT && *ri == mon->patrol_point)
        {
            // Slightly greater chance to simply head for the centre.
            count_grids += 3;
            if (x_chance_in_y(3, count_grids))
                set_target = true;
        }
        else if (one_chance_in(++count_grids))
            set_target = true;

        if (set_target)
            mon->target = *ri;
    }

    return (count_grids);
}

//#define DEBUG_PATHFIND

// Check all grids in LoS and mark lava and/or water as seen if the
// appropriate grids are encountered, so we later only need to do the
// visibility check for monsters that can't pass a feature potentially in
// the way. We don't care about shallow water as most monsters can safely
// cross that, and fire elementals alone aren't really worth the extra
// hassle. :)
static void _check_lava_water_in_sight()
{
    you.lava_in_sight = you.water_in_sight = 0;
    for (radius_iterator ri(you.pos(), LOS_RADIUS); ri; ++ri)
    {
        const coord_def ep = *ri - you.pos() + coord_def(9, 9);
        if (env.show(ep))
        {
            const dungeon_feature_type feat = grd(*ri);
            if (feat == DNGN_LAVA)
            {
                you.lava_in_sight = 1;
                if (you.water_in_sight > 0)
                    break;
            }
            else if (feat == DNGN_DEEP_WATER)
            {
                you.water_in_sight = 1;
                if (you.lava_in_sight > 0)
                    break;
            }
        }
    }
}

// If a monster can see but not directly reach the target, and then fails to
// find a path to get there, mark all surrounding (in a radius of 2) monsters
// of the same (or greater) movement restrictions as also being unable to
// find a path, so we won't need to calculate again.
// Should there be a direct path to the target for a monster thus marked, it
// will still be able to come nearer (and the mark will then be cleared).
static void _mark_neighbours_target_unreachable(monsters *mon)
{
    // Highly intelligent monsters are perfectly capable of pathfinding
    // and don't need their neighbour's advice.
    const mon_intel_type intel = mons_intel(mon);
    if (intel > I_NORMAL)
        return;

    const bool flies         = mons_flies(mon);
    const bool amphibious    = mons_amphibious(mon);
    const habitat_type habit = mons_primary_habitat(mon);

    for (radius_iterator ri(mon->pos(), 2, true, false); ri; ++ri)
    {
        if (*ri == mon->pos())
            continue;

        if (mgrd(*ri) == NON_MONSTER)
            continue;

        // Don't alert monsters out of sight (e.g. on the other side of
        // a wall).
        if (!mon->mon_see_grid(*ri))
            continue;

        monsters* const m = &menv[mgrd(*ri)];

        // Don't restrict smarter monsters as they might find a path
        // a dumber monster wouldn't.
        if (mons_intel(m) > intel)
            continue;

        // Monsters of differing habitats might prefer different routes.
        if (mons_primary_habitat(m) != habit)
            continue;

        // A flying monster has an advantage over a non-flying one.
        if (!flies && mons_flies(m))
            continue;

        // Same for a swimming one, around water.
        if (you.water_in_sight > 0 && !amphibious && mons_amphibious(m))
            continue;

        if (m->travel_target == MTRAV_NONE)
            m->travel_target = MTRAV_UNREACHABLE;
    }
}

static void _find_all_level_exits(std::vector<level_exit> &e)
{
    e.clear();

    for (rectangle_iterator ri(1); ri; ++ri)
    {
        if (!in_bounds(*ri))
            continue;

        const dungeon_feature_type gridc = grd(*ri);

        // All types of stairs.
        if (is_stair(gridc))
            e.push_back(level_exit(*ri, false));

        // Teleportation and shaft traps.
        const trap_type tt = get_trap_type(*ri);
        if (tt == TRAP_TELEPORT || tt == TRAP_SHAFT)
            e.push_back(level_exit(*ri, false));
    }
}

static int _mons_find_nearest_level_exit(const monsters *mon,
                                         std::vector<level_exit> &e,
                                         bool reset = false)
{

    if (e.empty() || reset)
        _find_all_level_exits(e);

    int retval = -1;
    int old_dist = -1;

    for (unsigned int i = 0; i < e.size(); ++i)
    {
        if (e[i].unreachable)
            continue;

        int dist = grid_distance(mon->pos(), e[i].target);

        if (old_dist == -1 || old_dist >= dist)
        {
            // Ignore teleportation and shaft traps that the monster
            // shouldn't know about.
            if (!mons_is_native_in_branch(mon)
                && grd(e[i].target) == DNGN_UNDISCOVERED_TRAP)
            {
                continue;
            }

            retval = i;
            old_dist = dist;
        }
    }

    return (retval);
}

// If _mons_find_level_exits() is ever expanded to handle more grid
// types, this should be expanded along with it.
static void _mons_indicate_level_exit(const monsters *mon)
{
    const dungeon_feature_type gridc = grd(mon->pos());

    // All types of stairs.
    if (is_gate(gridc))
        simple_monster_message(mon, " passes through the gate.");
    else if (is_travelable_stair(gridc))
    {
        command_type dir = grid_stair_direction(gridc);
        simple_monster_message(mon,
            make_stringf(" %s the stairs.",
                dir == CMD_GO_UPSTAIRS   ? "goes up" :
                dir == CMD_GO_DOWNSTAIRS ? "goes down"
                                         : "takes").c_str());
    }
}

void make_mons_leave_level(monsters *mon)
{
    if (mons_is_pacified(mon))
    {
        if (mons_near(mon) && player_monster_visible(mon))
            _mons_indicate_level_exit(mon);

        // Pacified monsters leaving the level take their stuff with
        // them.
        mon->flags |= MF_HARD_RESET;
        monster_die(mon, KILL_DISMISSED, NON_MONSTER);
    }
}

static void _set_no_path_found(monsters *mon)
{
#ifdef DEBUG_PATHFIND
    mpr("No path found!");
#endif

    mon->travel_target = MTRAV_UNREACHABLE;
    // Pass information on to nearby monsters.
    _mark_neighbours_target_unreachable(mon);
}

static bool _target_is_unreachable(monsters *mon)
{
    return (mon->travel_target == MTRAV_UNREACHABLE
            || mon->travel_target == MTRAV_KNOWN_UNREACHABLE);
}

// The monster is trying to get to the player (MHITYOU).
// Check whether there's an unobstructed path to the player (in sight!),
// either by using an existing travel_path or calculating a new one.
// Returns true if no further handling necessary, else false.
static bool _try_pathfind(monsters *mon, const dungeon_feature_type can_move,
                          bool potentially_blocking)
{
    // Just because we can *see* the player, that doesn't mean
    // we can actually get there. To find about that, we first
    // check for transparent walls. If there are transparent
    // walls in the way we'll need pathfinding, no matter what.
    // (Though monsters with a los attack don't need to get any
    // closer to hurt the player.)
    // If no walls are detected, there could still be a river
    // or a pool of lava in the way. So we check whether there
    // is water or lava in LoS (boolean) and if so, try to find
    // a way around it. It's possible that the player can see
    // lava but it actually has no influence on the monster's
    // movement (because it's lying in the opposite direction)
    // but if so, we'll find that out during path finding.
    // In another attempt of optimization, don't bother with
    // path finding if the monster in question has no trouble
    // travelling through water or flying across lava.
    // Also, if no path is found (too far away, perhaps) set a
    // flag, so we don't directly calculate the whole thing again
    // next turn, and even extend that flag to neighbouring
    // monsters of similar movement restrictions.

    // Smart monsters that can fire through walls won't use
    // pathfinding, and it's also not necessary if the monster
    // is already adjacent to you.
    if (potentially_blocking && mons_intel(mon) >= I_NORMAL
           && !mons_friendly(mon) && mons_has_los_ability(mon->type)
        || grid_distance(mon->pos(), you.pos()) == 1)
    {
        potentially_blocking = false;
    }
    else
    {
        // If we don't already know whether there's water or lava
        // in LoS of the player, find out now.
        if (you.lava_in_sight == -1 || you.water_in_sight == -1)
            _check_lava_water_in_sight();

        // Flying monsters don't see water/lava as obstacle.
        // Also don't use pathfinding if the monster can shoot
        // across the blocking terrain, and is smart enough to
        // realize that.
        if (!potentially_blocking && !mons_flies(mon)
            && (mons_intel(mon) < I_NORMAL
                || mons_friendly(mon)
                || (!mons_has_ranged_spell(mon, true)
                    && !mons_has_ranged_attack(mon))))
        {
            const habitat_type habit = mons_primary_habitat(mon);
            if (you.lava_in_sight > 0 && habit != HT_LAVA
                || you.water_in_sight > 0 && habit != HT_WATER
                   && can_move != DNGN_DEEP_WATER)
            {
                potentially_blocking = true;
            }
        }
    }

    if (!potentially_blocking
        || grid_see_grid(mon->pos(), you.pos(), can_move))
    {
        // The player is easily reachable.
        // Clear travel path and target, if necessary.
        if (mon->travel_target != MTRAV_PATROL
            && mon->travel_target != MTRAV_NONE)
        {
            if (mon->is_travelling())
                mon->travel_path.clear();
            mon->travel_target = MTRAV_NONE;
        }
        return (false);
    }

    // Even if the target has been to "unreachable" (the monster already tried,
    // and failed, to find a path) there's a chance of trying again.
    if (!_target_is_unreachable(mon) || one_chance_in(12))
    {
#ifdef DEBUG_PATHFIND
        mprf("%s: Player out of reach! What now?",
             mon->name(DESC_PLAIN).c_str());
#endif
        // If we're already on our way, do nothing.
        if (mon->is_travelling() && mon->travel_target == MTRAV_PLAYER)
        {
            const int len = mon->travel_path.size();
            const coord_def targ = mon->travel_path[len - 1];

            // Current target still valid?
            if (grid_see_grid(targ, you.pos(), can_move))
            {
                // Did we reach the target?
                if (mon->pos() == mon->travel_path[0])
                {
                    // Get next waypoint.
                    mon->travel_path.erase( mon->travel_path.begin() );

                    if (!mon->travel_path.empty())
                    {
                        mon->target = mon->travel_path[0];
                        return (true);
                    }
                }
                else if (grid_see_grid(mon->pos(), mon->travel_path[0],
                                       can_move))
                {
                    mon->target = mon->travel_path[0];
                    return (true);
                }
            }
        }

        // Use pathfinding to find a (new) path to the player.
        const int dist = grid_distance(mon->pos(), you.pos());

#ifdef DEBUG_PATHFIND
        mprf("Need to calculate a path... (dist = %d)", dist);
#endif
        const int range = mons_tracking_range(mon);
        if (range > 0 && dist > range)
        {
            mon->travel_target = MTRAV_UNREACHABLE;
#ifdef DEBUG_PATHFIND
            mprf("Distance too great, don't attempt pathfinding! (%s)",
                 mon->name(DESC_PLAIN).c_str());
#endif
            return (false);
        }

#ifdef DEBUG_PATHFIND
        mprf("Need a path for %s from (%d, %d) to (%d, %d), max. dist = %d",
             mon->name(DESC_PLAIN).c_str(), mon->pos(), you.pos(), range);
#endif
        monster_pathfind mp;
        if (range > 0)
            mp.set_range(range);

        if (mp.init_pathfind(mon, you.pos()))
        {
            mon->travel_path = mp.calc_waypoints();
            if (!mon->travel_path.empty())
            {
                // Okay then, we found a path.  Let's use it!
                mon->target = mon->travel_path[0];
                mon->travel_target = MTRAV_PLAYER;
                return (true);
            }
            else
                _set_no_path_found(mon);
        }
        else
            _set_no_path_found(mon);
    }

    // We didn't find a path.
    return (false);
}

// Returns true if a monster left the level.
static bool _pacified_leave_level(monsters *mon, std::vector<level_exit> e,
                                  int e_index)
{
    // If a pacified monster is leaving the level, and has reached its
    // goal, handle it here.
    // Likewise, if a pacified monster is far enough away from the
    // player, make it leave the level.
    if (e_index != -1 && mon->pos() == e[e_index].target
        || grid_distance(mon->pos(), you.pos()) >= LOS_RADIUS * 4)
    {
        make_mons_leave_level(mon);
        return (true);
    }

    return (false);
}

// Counts deep water twice.
static int _count_water_neighbours(coord_def p)
{
    int water_count = 0;
    for (adjacent_iterator ai(p); ai; ++ai)
    {
        if (grd(*ai) == DNGN_SHALLOW_WATER)
            water_count++;
        else if (grd(*ai) == DNGN_DEEP_WATER)
            water_count += 2;
    }
    return (water_count);
}

// Pick the nearest water grid that is surrounded by the most
// water squares within LoS.
static bool _find_siren_water_target(monsters *mon)
{
    ASSERT(mon->type == MONS_SIREN);

    // Moving away could break the entrancement, so don't do this.
    if ((mon->pos() - you.pos()).rdist() >= 6)
        return (false);

    // Already completely surrounded by deep water.
    if (_count_water_neighbours(mon->pos()) >= 16)
        return (true);

    if (mon->travel_target == MTRAV_SIREN)
    {
        coord_def targ_pos(mon->travel_path[mon->travel_path.size() - 1]);
#ifdef DEBUG_PATHFIND
        mprf("siren target is (%d, %d), dist = %d",
             targ_pos.x, targ_pos.y, (int) (mon->pos() - targ_pos).rdist());
#endif
        if ((mon->pos() - targ_pos).rdist() > 2)
            return (true);
    }

    monster_los lm;
    lm.set_monster(mon);
    lm.set_los_range(LOS_RADIUS);
    lm.fill_los_field();

    int best_water_count = 0;
    coord_def best_target;
    bool first = true;

    while (true)
    {
        int best_num = 0;
        for (radius_iterator ri(mon->pos(), LOS_RADIUS, true, false);
             ri; ++ri)
        {
            if (!grid_is_water(grd(*ri)))
                continue;

            // In the first iteration only count water grids that are
            // not closer to the player than to the siren.
            if (first && (mon->pos() - *ri).rdist() > (you.pos() - *ri).rdist())
                continue;

            // Counts deep water twice.
            const int water_count = _count_water_neighbours(*ri);
            if (water_count < best_water_count)
                continue;

            if (water_count > best_water_count)
            {
                best_water_count = water_count;
                best_target = *ri;
                best_num = 1;
            }
            else // water_count == best_water_count
            {
                const int old_dist = (mon->pos() - best_target).rdist();
                const int new_dist = (mon->pos() - *ri).rdist();
                if (new_dist > old_dist)
                    continue;

                if (new_dist < old_dist)
                {
                    best_target = *ri;
                    best_num = 1;
                }
                else if (one_chance_in(++best_num))
                    best_target = *ri;
            }
        }

        if (!first || best_water_count > 0)
            break;

        // Else start the second iteration.
        first = false;
    }

    if (!best_water_count)
        return (false);

    // We're already optimally placed.
    if (best_target == mon->pos())
        return (true);

    monster_pathfind mp;
#ifdef WIZARD
    // Remove old highlighted areas to make place for the new ones.
    for (rectangle_iterator ri(1); ri; ++ri)
        env.map(*ri).property &= ~(FPROP_HIGHLIGHT);
#endif

    if (mp.init_pathfind(mon, best_target))
    {
        mon->travel_path = mp.calc_waypoints();

        if (!mon->travel_path.empty())
        {
#ifdef WIZARD
            for (unsigned int i = 0; i < mon->travel_path.size(); i++)
                env.map(mon->travel_path[i]).property |= FPROP_HIGHLIGHT;
#endif
#ifdef DEBUG_PATHFIND
            mprf("Found a path to (%d, %d) with %d surrounding water squares",
                 best_target.x, best_target.y, best_water_count);
#endif
            // Okay then, we found a path.  Let's use it!
            mon->target = mon->travel_path[0];
            mon->travel_target = MTRAV_SIREN;
            return (true);
        }
    }

    return (false);
}

static bool _find_wall_target(monsters *mon)
{
    ASSERT(mons_wall_shielded(mon));

    if (mon->travel_target == MTRAV_WALL)
    {
        coord_def targ_pos(mon->travel_path[mon->travel_path.size() - 1]);

        // Target grid might have changed since we started, like if the
        // player destroys the wall the monster wants to hide in.
        if (grid_is_solid(targ_pos)
            && monster_habitable_grid(mon, grd(targ_pos)))
        {
            // Wall is still good.
#ifdef DEBUG_PATHFIND
            mprf("%s target is (%d, %d), dist = %d",
                 mon->name(DESC_PLAIN, true).c_str(),
                 targ_pos.x, targ_pos.y, (int) (mon->pos() - targ_pos).rdist());
#endif
            return (true);
        }

        mon->travel_path.clear();
        mon->travel_target = MTRAV_NONE;
    }

    monster_los lm;
    lm.set_monster(mon);
    lm.set_los_range(LOS_RADIUS);
    lm.fill_los_field();

    int       best_dist             = INT_MAX;
    bool      best_closer_to_player = false;
    coord_def best_target;

    for (radius_iterator ri(mon->pos(), LOS_RADIUS, true, false);
         ri; ++ri)
    {
        if (!grid_is_solid(*ri)
            || !monster_habitable_grid(mon, grd(*ri)))
        {
            continue;
        }

        int  dist = (mon->pos() - *ri).rdist();
        bool closer_to_player = false;
        if (dist > (you.pos() - *ri).rdist())
            closer_to_player = true;

        if (dist < best_dist)
        {
            best_dist             = dist;
            best_closer_to_player = closer_to_player;
            best_target           = *ri;
        }
        else if (best_closer_to_player && !closer_to_player
                 && dist == best_dist)
        {
            best_closer_to_player = false;
            best_target           = *ri;
        }
    }

    if (best_dist == INT_MAX || !in_bounds(best_target))
        return (false);

    monster_pathfind mp;
#ifdef WIZARD
    // Remove old highlighted areas to make place for the new ones.
    for (rectangle_iterator ri(1); ri; ++ri)
        env.map(*ri).property &= ~(FPROP_HIGHLIGHT);
#endif

    if (mp.init_pathfind(mon, best_target))
    {
        mon->travel_path = mp.calc_waypoints();

        if (!mon->travel_path.empty())
        {
#ifdef WIZARD
            for (unsigned int i = 0; i < mon->travel_path.size(); i++)
                env.map(mon->travel_path[i]).property |= FPROP_HIGHLIGHT;
#endif
#ifdef DEBUG_PATHFIND
            mprf("Found a path to (%d, %d)", best_target.x, best_target.y);
#endif
            // Okay then, we found a path.  Let's use it!
            mon->target = mon->travel_path[0];
            mon->travel_target = MTRAV_WALL;
            return (true);
        }
    }
    return (false);
}

// Returns true if further handling neeeded.
static bool _handle_monster_travelling(monsters *mon,
                                       const dungeon_feature_type can_move)
{
#ifdef DEBUG_PATHFIND
    mprf("Monster %s reached target (%d, %d)",
         mon->name(DESC_PLAIN).c_str(), mon->target.x, mon->target.y);
#endif

    // Hey, we reached our first waypoint!
    if (mon->pos() == mon->travel_path[0])
    {
#ifdef DEBUG_PATHFIND
        mpr("Arrived at first waypoint.");
#endif
        mon->travel_path.erase( mon->travel_path.begin() );
        if (mon->travel_path.empty())
        {
#ifdef DEBUG_PATHFIND
            mpr("We reached the end of our path: stop travelling.");
#endif
            mon->travel_target = MTRAV_NONE;
            return (true);
        }
        else
        {
            mon->target = mon->travel_path[0];
#ifdef DEBUG_PATHFIND
            mprf("Next waypoint: (%d, %d)", mon->target.x, mon->target.y);
#endif
            return (false);
        }
    }

    // Can we still see our next waypoint?
    if (!grid_see_grid(mon->pos(), mon->travel_path[0], can_move))
    {
#ifdef DEBUG_PATHFIND
        mpr("Can't see waypoint grid.");
#endif
        // Apparently we got sidetracked a bit.
        // Check the waypoints vector backwards and pick the first waypoint
        // we can see.

        // XXX: Note that this might still not be the best thing to do
        // since another path might be even *closer* to our actual target now.
        // Not by much, though, since the original path was optimal (A*) and
        // the distance between the waypoints is rather small.

        int erase = -1;  // Erase how many waypoints?
        const int size = mon->travel_path.size();
        for (int i = size - 1; i >= 0; --i)
        {
            if (grid_see_grid(mon->pos(), mon->travel_path[i], can_move))
            {
                mon->target = mon->travel_path[i];
                erase = i;
                break;
            }
        }

        if (erase > 0)
        {
#ifdef DEBUG_PATHFIND
            mprf("Need to erase %d of %d waypoints.",
                 erase, size);
#endif
            // Erase all waypoints that came earlier:
            // we don't need them anymore.
            while (0 < erase--)
                mon->travel_path.erase( mon->travel_path.begin() );
        }
        else
        {
            // We can't reach our old path from our current
            // position, so calculate a new path instead.
            monster_pathfind mp;

            // The last coordinate in the path vector is our destination.
            const int len = mon->travel_path.size();
            if (mp.init_pathfind(mon, mon->travel_path[len-1]))
            {
                mon->travel_path = mp.calc_waypoints();
                if (!mon->travel_path.empty())
                {
                    mon->target = mon->travel_path[0];
#ifdef DEBUG_PATHFIND
                    mprf("Next waypoint: (%d, %d)",
                         mon->target.x, mon->target.y);
#endif
                }
                else
                {
                    mon->travel_target = MTRAV_NONE;
                    return (true);
                }
            }
            else
            {
                // Or just forget about the whole thing.
                mon->travel_path.clear();
                mon->travel_target = MTRAV_NONE;
                return (true);
            }
        }
    }

    // Else, we can see the next waypoint and are making good progress.
    // Carry on, then!
    return (false);
}

// Returns true if further handling neeeded.
static bool _handle_monster_patrolling(monsters *mon)
{
    if (!_choose_random_patrol_target_grid(mon))
    {
        // If we couldn't find a target that is within easy reach
        // of the monster and close to the patrol point, depending
        // on monster intelligence, do one of the following:
        //  * set current position as new patrol point
        //  * forget about patrolling
        //  * head back to patrol point

        if (mons_intel(mon) == I_PLANT)
        {
            // Really stupid monsters forget where they're supposed to be.
            if (mons_friendly(mon))
            {
                // Your ally was told to wait, and wait it will!
                // (Though possibly not where you told it to.)
                mon->patrol_point = mon->pos();
            }
            else
            {
                // Stop patrolling.
                mon->patrol_point.reset();
                mon->travel_target = MTRAV_NONE;
                return (true);
            }
        }
        else
        {
            // It's time to head back!
            // Other than for tracking the player, there's currently
            // no distinction between smart and stupid monsters when
            // it comes to travelling back to the patrol point. This
            // is in part due to the flavour of e.g. bees finding
            // their way back to the Hive (and patrolling should
            // really be restricted to cases like this), and for the
            // other part it's not all that important because we
            // calculate the path once and then follow it home, and
            // the player won't ever see the orderly fashion the
            // bees will trudge along.
            // What he will see is them swarming back to the Hive
            // entrance after some time, and that is what matters.
            monster_pathfind mp;
            if (mp.init_pathfind(mon, mon->patrol_point))
            {
                mon->travel_path = mp.calc_waypoints();
                if (!mon->travel_path.empty())
                {
                    mon->target = mon->travel_path[0];
                    mon->travel_target = MTRAV_PATROL;
                }
                else
                {
                    // We're so close we don't even need a path.
                    mon->target = mon->patrol_point;
                }
            }
            else
            {
                // Stop patrolling.
                mon->patrol_point.reset();
                mon->travel_target = MTRAV_NONE;
                return (true);
            }
        }
    }
    else
    {
#ifdef DEBUG_PATHFIND
        mprf("Monster %s (pp: %d, %d) is now patrolling to (%d, %d)",
             mon->name(DESC_PLAIN).c_str(),
             mon->patrol_point.x, mon->patrol_point.y,
             mon->target.x, mon->target.y);
#endif
    }

    return (false);
}

static void _check_wander_target(monsters *mon, bool isPacified = false,
                                 dungeon_feature_type can_move = DNGN_UNSEEN)
{
    // default wander behaviour
    if (mon->pos() == mon->target
        || mons_is_batty(mon) || !isPacified && one_chance_in(20))
    {
        bool need_target = true;

        if (!can_move)
            can_move = (mons_amphibious(mon))
                ? DNGN_DEEP_WATER : DNGN_SHALLOW_WATER;

        if (mon->is_travelling())
            need_target = _handle_monster_travelling(mon, can_move);

        // If we still need a target because we're not travelling
        // (any more), check for patrol routes instead.
        if (need_target && mon->is_patrolling())
            need_target = _handle_monster_patrolling(mon);

        // XXX: This is really dumb wander behaviour... instead of
        // changing the goal square every turn, better would be to
        // have the monster store a direction and have the monster
        // head in that direction for a while, then shift the
        // direction to the left or right.  We're changing this so
        // wandering monsters at least appear to have some sort of
        // attention span.  -- bwr
        if (need_target)
            _set_random_target(mon);
    }
}

static void _arena_set_foe(monsters *mons)
{
    const int mind = monster_index(mons);

    int nearest = -1;
    int best_distance = -1;

    int nearest_unseen = -1;
    int best_unseen_distance = -1;
    for (int i = 0; i < MAX_MONSTERS; ++i)
    {
        if (mind == i)
            continue;

        const monsters *other(&menv[i]);
        if (!other->alive() || mons_aligned(mind, i))
            continue;

        // Don't fight test spawners, since they're only pseduo-monsters
        // placed to spawn real monsters, plus they're impossible to kill.
        // But test spawners can fight each other, to give them a target o
        // spawn against.
        if (other->type == MONS_TEST_SPAWNER
            && mons->type != MONS_TEST_SPAWNER)
        {
            continue;
        }

        const int distance = grid_distance(mons->pos(), other->pos());
        const bool seen = mons->can_see(other);

        if (seen)
        {
            if (best_distance == -1 || distance < best_distance)
            {
                best_distance = distance;
                nearest = i;
            }
        }
        else
        {
            if (best_unseen_distance == -1 || distance < best_unseen_distance)
            {
                best_unseen_distance = distance;
                nearest_unseen = i;
            }
        }

        if ((best_distance == -1 || distance < best_distance)
            && mons->can_see(other))

        {
            best_distance = distance;
            nearest = i;
        }
    }

    if (nearest != -1)
    {
        mons->foe = nearest;
        mons->target = menv[nearest].pos();
        mons->behaviour = BEH_SEEK;
    }
    else if (nearest_unseen != -1)
    {
        mons->target = menv[nearest_unseen].pos();
        if (mons->type == MONS_TEST_SPAWNER)
        {
            mons->foe       = nearest_unseen;
            mons->behaviour = BEH_SEEK;
        }
        else
            mons->behaviour = BEH_WANDER;
    }
    else
    {
        mons->foe       = MHITNOT;
        mons->behaviour = BEH_WANDER;
    }
    if (mons->behaviour == BEH_WANDER)
        _check_wander_target(mons);

    ASSERT(mons->foe == MHITNOT || !mons->target.origin());
}

//---------------------------------------------------------------
//
// handle_behaviour
//
// 1. Evaluates current AI state
// 2. Sets monster target x,y based on current foe
//
// XXX: Monsters of I_NORMAL or above should select a new target
// if their current target is another monster which is sitting in
// a wall and is immune to most attacks while in a wall, unless
// the monster has a spell or special/nearby ability which isn't
// affected by the wall.
//---------------------------------------------------------------
static void _handle_behaviour(monsters *mon)
{
    bool changed = true;
    bool isFriendly = mons_friendly(mon);
    bool isNeutral  = mons_neutral(mon);
    bool wontAttack = mons_wont_attack_real(mon);
    bool proxPlayer = mons_near(mon) && !crawl_state.arena;
    bool trans_wall_block = trans_wall_blocking(mon->pos());

#ifdef WIZARD
    // If stealth is greater than actually possible (wizmode level)
    // pretend the player isn't there, but only for hostile monsters.
    if (proxPlayer && you.skills[SK_STEALTH] > 27 && !mons_wont_attack(mon))
        proxPlayer = false;
#endif
    bool proxFoe;
    bool isHurt     = (mon->hit_points <= mon->max_hit_points / 4 - 1);
    bool isHealthy  = (mon->hit_points > mon->max_hit_points / 2);
    bool isSmart    = (mons_intel(mon) > I_ANIMAL);
    bool isScared   = mon->has_ench(ENCH_FEAR);
    bool isMobile   = !mons_is_stationary(mon);
    bool isPacified = mons_is_pacified(mon);
    bool patrolling = mon->is_patrolling();
    static std::vector<level_exit> e;
    static int                     e_index = -1;

    // Check for confusion -- early out.
    if (mon->has_ench(ENCH_CONFUSION))
    {
        _set_random_target(mon);
        return;
    }

    if (crawl_state.arena)
    {
        if (Options.arena_force_ai)
        {
            if (!mon->get_foe() || mon->target.origin() || one_chance_in(3))
                mon->foe = MHITNOT;
            if (mon->foe == MHITNOT || mon->foe == MHITYOU)
                _arena_set_foe(mon);
            return;
        }
        // If we're not forcing monsters to attack, just make sure they're
        // not targeting the player in arena mode.
        else if (mon->foe == MHITYOU)
            mon->foe = MHITNOT;
    }

    if (mons_wall_shielded(mon) && grid_is_solid(mon->pos()))
    {
        // Monster is safe, so its behaviour can be simplified to fleeing.
        if (mon->behaviour == BEH_CORNERED || mon->behaviour == BEH_PANIC
            || isScared)
        {
            mon->behaviour = BEH_FLEE;
        }
    }

    const dungeon_feature_type can_move =
        (mons_amphibious(mon)) ? DNGN_DEEP_WATER : DNGN_SHALLOW_WATER;


    // Validate current target exists.
    if (mon->foe != MHITNOT && mon->foe != MHITYOU
        && !menv[mon->foe].alive())
    {
        mon->foe = MHITNOT;
    }

    // Change proxPlayer depending on invisibility and standing
    // in shallow water.
    if (proxPlayer && you.invisible())
    {
        if (!mons_player_visible( mon ))
            proxPlayer = false;
        // Must be able to see each other.
        else if (!see_grid(mon->pos()))
            proxPlayer = false;

        const int intel = mons_intel(mon);
        // Now, the corollary to that is that sometimes, if a
        // player is right next to a monster, they will 'see'.
        if (grid_distance( you.pos(), mon->pos() ) == 1
            && one_chance_in(3))
        {
            proxPlayer = true;
        }

        // [dshaligram] Very smart monsters have a chance of clueing in to
        // invisible players in various ways.
        else if (intel == I_NORMAL && one_chance_in(13)
                 || intel == I_HIGH && one_chance_in(6))
        {
            proxPlayer = true;
        }
    }

    // Set friendly target, if they don't already have one.
    // Berserking allies ignore your commands!
    if (isFriendly
        && you.pet_target != MHITNOT
        && (mon->foe == MHITNOT || mon->foe == MHITYOU)
        && !mon->has_ench(ENCH_BERSERK))
    {
        mon->foe = you.pet_target;
    }

    // Instead, berserkers attack nearest monsters.
    if (mon->has_ench(ENCH_BERSERK)
        && (mon->foe == MHITNOT || isFriendly && mon->foe == MHITYOU))
    {
        // Intelligent monsters prefer to attack the player,
        // even when berserking.
        if (!isFriendly && proxPlayer && mons_intel(mon) >= I_NORMAL)
            mon->foe = MHITYOU;
        else
            _set_nearest_monster_foe(mon);
    }

    // Pacified monsters leaving the level prefer not to attack.
    if (isNeutral && !isPacified && mon->foe == MHITNOT)
        _set_nearest_monster_foe(mon);

    // Monsters do not attack themselves. {dlb}
    if (mon->foe == monster_index(mon))
        mon->foe = MHITNOT;

    // Friendly and good neutral monsters do not attack other friendly
    // and good neutral monsters.
    if (mon->foe != MHITNOT && mon->foe != MHITYOU
        && wontAttack && mons_wont_attack_real(&menv[mon->foe]))
    {
        mon->foe = MHITNOT;
    }

    // Neutral monsters prefer not to attack players, or other neutrals.
    if (isNeutral && mon->foe != MHITNOT
        && (mon->foe == MHITYOU || mons_neutral(&menv[mon->foe])))
    {
        mon->foe = MHITNOT;
    }

    // Unfriendly monsters fighting other monsters will usually
    // target the player, if they're healthy.
    if (!isFriendly && !isNeutral
        && mon->foe != MHITYOU && mon->foe != MHITNOT
        && proxPlayer && !(mon->has_ench(ENCH_BERSERK)) && isHealthy
        && !one_chance_in(3))
    {
        mon->foe = MHITYOU;
    }

    // Validate current target again.
    if (mon->foe != MHITNOT && mon->foe != MHITYOU
        && !menv[mon->foe].alive())
    {
        mon->foe = MHITNOT;
    }

    while (changed)
    {
        coord_def foepos = you.pos();

        // Evaluate these each time; they may change.
        if (mon->foe == MHITNOT)
            proxFoe = false;
        else
        {
            if (mon->foe == MHITYOU)
            {
                foepos = you.pos();
                proxFoe = proxPlayer;   // Take invis into account.
            }
            else
            {
                proxFoe = mons_near(mon, mon->foe);

                if (!mon_can_see_monster(mon, &menv[mon->foe]))
                    proxFoe = false;

                foepos = menv[mon->foe].pos();
            }
        }

        // Track changes to state; attitude never changes here.
        beh_type new_beh       = mon->behaviour;
        unsigned short new_foe = mon->foe;

        // Take care of monster state changes.
        switch (mon->behaviour)
        {
        case BEH_SLEEP:
            // default sleep state
            mon->target = mon->pos();
            new_foe = MHITNOT;
            break;

        case BEH_LURK:
        case BEH_SEEK:
            // No foe? Then wander or seek the player.
            if (mon->foe == MHITNOT)
            {
                if (!proxPlayer || isNeutral || patrolling)
                    new_beh = BEH_WANDER;
                else
                {
                    new_foe = MHITYOU;
                    mon->target = you.pos();
                }
                break;
            }

            // Foe gone out of LOS?
            if (!proxFoe)
            {
                if (mon->travel_target == MTRAV_SIREN)
                    mon->travel_target = MTRAV_NONE;

                if (mon->foe == MHITYOU && mon->is_travelling()
                    && mon->travel_target == MTRAV_PLAYER)
                {
                    // We've got a target, so we'll continue on our way.
#ifdef DEBUG_PATHFIND
                    mpr("Player out of LoS... start wandering.");
#endif
                    new_beh = BEH_WANDER;
                    break;
                }

                if (isFriendly)
                {
                    if (patrolling)
                    {
                        new_foe = MHITNOT;
                        new_beh = BEH_WANDER;
                    }
                    else
                    {
                        new_foe = MHITYOU;
                        mon->target = foepos;
                    }
                    break;
                }

                if (mon->foe_memory > 0 && mon->foe != MHITNOT)
                {
                    // If we've arrived at our target x,y
                    // do a stealth check.  If the foe
                    // fails, monster will then start
                    // tracking foe's CURRENT position,
                    // but only for a few moves (smell and
                    // intuition only go so far).

                    if (mon->pos() == mon->target)
                    {
                        if (mon->foe == MHITYOU)
                        {
                            if (one_chance_in(you.skills[SK_STEALTH]/3))
                                mon->target = you.pos();
                            else
                                mon->foe_memory = 1;
                        }
                        else
                        {
                            if (coinflip())     // XXX: cheesy!
                                mon->target = menv[mon->foe].pos();
                            else
                                mon->foe_memory = 1;
                        }
                    }

                    // Either keep chasing, or start wandering.
                    if (mon->foe_memory < 2)
                    {
                        mon->foe_memory = 0;
                        new_beh = BEH_WANDER;
                    }
                    break;
                }

                // Hack: smarter monsters will tend to pursue the player longer.
                int memory = 0;
                switch (mons_intel(mon))
                {
                case I_HIGH:
                    memory = 100 + random2(200);
                    break;
                case I_NORMAL:
                    memory = 50 + random2(100);
                    break;
                case I_ANIMAL:
                case I_INSECT:
                    memory = 25 + random2(75);
                    break;
                case I_PLANT:
                    memory = 10 + random2(50);
                    break;
                }

                mon->foe_memory = memory;
                break;  // switch/case BEH_SEEK
            }

            // Monster can see foe: continue 'tracking'
            // by updating target x,y.
            if (mon->foe == MHITYOU)
            {
                // The foe is the player.
                if (mon->type == MONS_SIREN
                    && player_mesmerised_by(mon)
                    && _find_siren_water_target(mon))
                {
                    break;
                }

                if (_try_pathfind(mon, can_move, trans_wall_block))
                    break;

                // Whew. If we arrived here, path finding didn't yield anything
                // (or wasn't even attempted) and we need to set our target
                // the traditional way.

                // Sometimes, your friends will wander a bit.
                if (isFriendly && one_chance_in(8))
                {
                    _set_random_target(mon);
                    mon->foe = MHITNOT;
                    new_beh  = BEH_WANDER;
                }
                else
                {
                    mon->target = you.pos();
                }
            }
            else
            {
                // We have a foe but it's not the player.
                mon->target = menv[mon->foe].pos();
            }

            // Smart monsters, zombified monsters other than spectral
            // things, plants, and nonliving monsters cannot flee.
            if (isHurt && !isSmart && isMobile
                && (!mons_is_zombified(mon) || mon->type == MONS_SPECTRAL_THING)
                && mons_class_holiness(mon->type) != MH_PLANT
                && mons_class_holiness(mon->type) != MH_NONLIVING)
            {
                new_beh = BEH_FLEE;
            }
            break;

        case BEH_WANDER:
            if (isPacified)
            {
                // If a pacified monster isn't travelling toward
                // someplace from which it can leave the level, make it
                // start doing so.  If there's no such place, either
                // search the level for such a place again, or travel
                // randomly.
                if (mon->travel_target != MTRAV_PATROL)
                {
                    new_foe = MHITNOT;
                    mon->travel_path.clear();

                    e_index = _mons_find_nearest_level_exit(mon, e);

                    if (e_index == -1 && one_chance_in(20))
                        e_index = _mons_find_nearest_level_exit(mon, e, true);

                    if (e_index != -1)
                    {
                        mon->travel_target = MTRAV_PATROL;
                        patrolling = true;
                        mon->patrol_point = e[e_index].target;
                        mon->target = e[e_index].target;
                    }
                    else
                    {
                        mon->travel_target = MTRAV_NONE;
                        patrolling = false;
                        mon->patrol_point.reset();
                        _set_random_target(mon);
                    }
                }

                if (_pacified_leave_level(mon, e, e_index))
                    return;
            }

            // Is our foe in LOS?
            // Batty monsters don't automatically reseek so that
            // they'll flitter away, we'll reset them just before
            // they get movement in handle_monsters() instead. -- bwr
            if (proxFoe && !mons_is_batty(mon))
            {
                new_beh = BEH_SEEK;
                break;
            }

            _check_wander_target(mon, isPacified, can_move);

            // During their wanderings, monsters will eventually relax
            // their guard (stupid ones will do so faster, smart
            // monsters have longer memories).  Pacified monsters will
            // also eventually switch the place from which they want to
            // leave the level, in case their current choice is blocked.
            if (!proxFoe && mon->foe != MHITNOT
                   && one_chance_in(isSmart ? 60 : 20)
                || isPacified && one_chance_in(isSmart ? 40 : 120))
            {
                new_foe = MHITNOT;
                if (mon->is_travelling() && mon->travel_target != MTRAV_PATROL
                    || isPacified)
                {
#ifdef DEBUG_PATHFIND
                    mpr("It's been too long! Stop travelling.");
#endif
                    mon->travel_path.clear();
                    mon->travel_target = MTRAV_NONE;

                    if (isPacified && e_index != -1)
                        e[e_index].unreachable = true;
                }
            }
            break;

        case BEH_FLEE:
            // Check for healed.
            if (isHealthy && !isScared)
                new_beh = BEH_SEEK;

            // Smart monsters flee until they can flee no more...
            // possible to get a 'CORNERED' event, at which point
            // we can jump back to WANDER if the foe isn't present.

            if (isFriendly)
            {
                // Special-cased below so that it will flee *towards* you.
                mon->target = you.pos();
            }
            else if (mons_wall_shielded(mon) && _find_wall_target(mon))
                ; // Wall target found.
            else if (proxFoe)
            {
                // Special-cased below so that it will flee *from* the
                // correct position.
                mon->target = foepos;
            }
            break;

        case BEH_CORNERED:
            // Plants or nonliving monsters cannot fight back.
            if (mons_class_holiness(mon->type) == MH_PLANT
                || mons_class_holiness(mon->type) == MH_NONLIVING)
            {
                break;
            }

            if (isHealthy)
                new_beh = BEH_SEEK;

            // Foe gone out of LOS?
            if (!proxFoe)
            {
                if ((isFriendly || proxPlayer) && !isNeutral && !patrolling)
                    new_foe = MHITYOU;
                else
                    new_beh = BEH_WANDER;
            }
            else
            {
                mon->target = foepos;
            }
            break;

        default:
            return;     // uh oh
        }

        changed = (new_beh != mon->behaviour || new_foe != mon->foe);
        mon->behaviour = new_beh;

        if (mon->foe != new_foe)
            mon->foe_memory = 0;

        mon->foe = new_foe;
    }

    if (mon->travel_target == MTRAV_WALL && grid_is_solid(mon->pos()))
    {
        if (mon->behaviour == BEH_FLEE)
        {
            // Monster is safe, so stay put.
            mon->target = mon->pos();
            mon->foe = MHITNOT;
        }
    }
}

static bool _mons_check_set_foe(monsters *mon, const coord_def& p,
                                bool friendly, bool neutral)
{
    if (!inside_level_bounds(p))
        return (false);

    if (!friendly && !neutral && p == you.pos()
        && mons_player_visible(mon) && !is_sanctuary(p))
    {
        mon->foe = MHITYOU;
        return (true);
    }

    if (mgrd(p) != NON_MONSTER)
    {
        monsters *foe = &menv[mgrd(p)];

        if (foe != mon
            && mon_can_see_monster(mon, foe)
            && (friendly || !is_sanctuary(p))
            && (mons_friendly(foe) != friendly
                || (neutral && !mons_neutral(foe))))
        {
            mon->foe = mgrd(p);
            return (true);
        }
    }
    return (false);
}

// Choose nearest monster as a foe.  (Used for berserking monsters.)
void _set_nearest_monster_foe(monsters *mon)
{
    const bool friendly = mons_friendly_real(mon);
    const bool neutral  = mons_neutral(mon);

    std::vector<coord_def> d;
    d.push_back(coord_def(-1,-1));
    d.push_back(coord_def( 0,-1));
    d.push_back(coord_def( 1,-1));
    d.push_back(coord_def(-1, 0));
    d.push_back(coord_def( 1, 0));
    d.push_back(coord_def(-1, 1));
    d.push_back(coord_def( 0, 1));
    d.push_back(coord_def( 1, 1));

    // Search the eight possible directions in random order, with increasing
    // distance from the monster.
    std::random_shuffle(d.begin(), d.end(), random2);
    for (int k = 1; k <= LOS_RADIUS; ++k)
        for (unsigned int i = 0; i < d.size(); i++)
        {
            const coord_def p = mon->pos() + coord_def(k*d[i].x, k*d[i].y);
            if (_mons_check_set_foe(mon, p, friendly, neutral))
                return;
        }
}

// The default suitable() function for choose_random_nearby_monster().
bool choose_any_monster(const monsters* mon)
{
    return (true);
}

// Find a nearby monster and return its index, including you as a
// possibility with probability weight.  suitable() should return true
// for the type of monster wanted.
// If prefer_named is true, named monsters (including uniques) are twice
// as likely to get chosen compared to non-named ones.
// If prefer_priest is true, priestly monsters (including uniques) are
// twice as likely to get chosen compared to non-priestly ones.
monsters *choose_random_nearby_monster(int weight,
                                       bool (*suitable)(const monsters* mon),
                                       bool in_sight, bool prefer_named,
                                       bool prefer_priest)
{
    return choose_random_monster_on_level(weight, suitable, in_sight, true,
                                          prefer_named, prefer_priest);
}

monsters *choose_random_monster_on_level(int weight,
                                         bool (*suitable)(const monsters* mon),
                                         bool in_sight, bool near_by,
                                         bool prefer_named, bool prefer_priest)
{
    monsters *chosen = NULL;

    // A radius_iterator with radius == max(GXM, GYM) will sweep the
    // whole level.
    radius_iterator ri(you.pos(), near_by ? 9 : std::max(GXM, GYM),
                       true, in_sight);

    for ( ; ri; ++ri )
    {
        if (mgrd(*ri) != NON_MONSTER)
        {
            monsters *mon = &menv[mgrd(*ri)];
            if (suitable(mon))
            {
                // FIXME: if the intent is to favour monsters
                // named by $DEITY, we should set a flag on the
                // monster (something like MF_DEITY_PREFERRED) and
                // use that instead of checking the name, given
                // that other monsters can also have names.

                // True, but it's currently only used for orcs, and
                // Blork and Urug also being preferred to non-named orcs
                // is fine, I think. Once more gods name followers (and
                // prefer them) that should be changed, of course. (jpeg)

                // Named or priestly monsters have doubled chances.
                int mon_weight = 1;

                if ((prefer_named && mon->is_named())
                    || (prefer_priest && mons_class_flag(mon->type, M_PRIEST)))
                {
                    mon_weight++;
                }

                if ( x_chance_in_y(mon_weight, (weight += mon_weight)) )
                    chosen = mon;
            }
        }
    }

    return chosen;
}

// Note that this function *completely* blocks messaging for monsters
// distant or invisible to the player ... look elsewhere for a function
// permitting output of "It" messages for the invisible {dlb}
// Intentionally avoids info and str_pass now. -- bwr
bool simple_monster_message(const monsters *monster, const char *event,
                            msg_channel_type channel,
                            int param,
                            description_level_type descrip)
{

    if ((mons_near(monster) || crawl_state.arena)
        && (channel == MSGCH_MONSTER_SPELL || channel == MSGCH_FRIEND_SPELL
            || player_monster_visible(monster) || crawl_state.arena))
    {
        std::string msg = monster->name(descrip);
        msg += event;
        msg = apostrophise_fixup(msg);

        if (channel == MSGCH_PLAIN && mons_wont_attack_real(monster))
            channel = MSGCH_FRIEND_ACTION;

        mpr(msg.c_str(), channel, param);
        return (true);
    }

    return (false);
}

// Altars as well as branch entrances are considered interesting for
// some monster types.
static bool _mon_on_interesting_grid(monsters *mon)
{
    // Patrolling shouldn't happen all the time.
    if (one_chance_in(4))
        return (false);

    const dungeon_feature_type feat = grd(mon->pos());

    switch (feat)
    {
    // Holy beings will tend to patrol around altars to the good gods.
    case DNGN_ALTAR_ELYVILON:
        if (!one_chance_in(3))
            return (false);
        // else fall through
    case DNGN_ALTAR_ZIN:
    case DNGN_ALTAR_SHINING_ONE:
        return (mons_is_holy(mon));

    // Orcs will tend to patrol around altars to Beogh, and guard the
    // stairway from and to the Orcish Mines.
    case DNGN_ALTAR_BEOGH:
    case DNGN_ENTER_ORCISH_MINES:
    case DNGN_RETURN_FROM_ORCISH_MINES:
        return (mons_is_native_in_branch(mon, BRANCH_ORCISH_MINES));

    // Same for elves and the Elven Halls.
    case DNGN_ENTER_ELVEN_HALLS:
    case DNGN_RETURN_FROM_ELVEN_HALLS:
        return (mons_is_native_in_branch(mon, BRANCH_ELVEN_HALLS));

    // Killer bees always return to their hive.
    case DNGN_ENTER_HIVE:
        return (mons_is_native_in_branch(mon, BRANCH_HIVE));

    default:
        return (false);
    }
}

// If a hostile monster finds itself on a grid of an "interesting" feature,
// while unoccupied, it will remain in that area, and try to return to it
// if it left it for fighting, seeking etc.
static void _maybe_set_patrol_route(monsters *monster)
{
    if (mons_is_wandering(monster)
        && !mons_friendly(monster)
        && !monster->is_patrolling()
        && _mon_on_interesting_grid(monster))
    {
        monster->patrol_point = monster->pos();
    }
}

// Check whether there's a monster of the same alignment adjacent to the
// given monster in at least one of three given directions (relative to
// the monster position).
static bool _allied_monster_at(monsters *mon, coord_def a, coord_def b,
                               coord_def c)
{
    std::vector<coord_def> pos;
    pos.push_back(mon->pos() + a);
    pos.push_back(mon->pos() + b);
    pos.push_back(mon->pos() + c);

    for (unsigned int i = 0; i < pos.size(); i++)
    {
        if (!in_bounds(pos[i]))
            continue;

        if (mgrd(pos[i]) == NON_MONSTER)
            continue;

        if (mons_is_stationary(&menv[mgrd(pos[i])]))
            continue;

        // Hostile monsters of normal intelligence only move aside for
        // monsters of the same type.
        if (mons_intel(mon) <= I_NORMAL && !mons_wont_attack_real(mon)
            && mons_genus(mon->type) != mons_genus((&menv[mgrd(pos[i])])->type))
        {
            continue;
        }

        if (mons_aligned(monster_index(mon), mgrd(pos[i])))
            return (true);
    }

    return (false);
}

// Check up to eight grids in the given direction for whether there's a
// monster of the same alignment as the given monster that happens to
// have a ranged attack. If this is true for the first monster encountered,
// returns true. Otherwise returns false.
static bool _ranged_allied_monster_in_dir(monsters *mon, coord_def p)
{
    coord_def pos = mon->pos();

    for (int i = 1; i <= LOS_RADIUS; i++)
    {
        pos += p;
        if (!in_bounds(pos))
            break;

        if (mgrd(pos) == NON_MONSTER)
            continue;

        if (mons_aligned(monster_index(mon), mgrd(pos)))
        {
            // Hostile monsters of normal intelligence only move aside for
            // monsters of the same type.
            if (mons_intel(mon) <= I_NORMAL && !mons_wont_attack(mon)
                && mons_genus(mon->type) != mons_genus((&menv[mgrd(pos)])->type))
            {
                return (false);
            }

            monsters *m = &menv[mgrd(pos)];
            if (mons_has_ranged_attack(m) || mons_has_ranged_spell(m, true))
                return (true);
        }
        break;
    }
    return (false);
}

//---------------------------------------------------------------
//
// handle_movement
//
// Move the monster closer to its target square.
//
//---------------------------------------------------------------
static void _handle_movement(monsters *monster)
{
    coord_def delta;

    _maybe_set_patrol_route(monster);

    // Monsters will try to flee out of a sanctuary.
    if (is_sanctuary(monster->pos()) && !mons_friendly(monster)
        && !mons_is_fleeing(monster)
        && monster->add_ench(mon_enchant(ENCH_FEAR, 0, KC_YOU)))
    {
        behaviour_event(monster, ME_SCARE, MHITNOT, monster->pos());
    }
    else if (mons_is_fleeing(monster) && inside_level_bounds(env.sanctuary_pos)
             && !is_sanctuary(monster->pos())
             && monster->target == env.sanctuary_pos)
    {
        // Once outside there's a chance they'll regain their courage.
        // Nonliving and berserking monsters always stop immediately,
        // since they're only being forced out rather than actually
        // scared.
        if (monster->holiness() == MH_NONLIVING
            || monster->has_ench(ENCH_BERSERK)
            || random2(5) > 2)
        {
            monster->del_ench(ENCH_FEAR);
        }
    }

    // Some calculations.
    if (mons_class_flag(monster->type, M_BURROWS) && monster->foe == MHITYOU)
    {
        // Boring beetles always move in a straight line in your
        // direction.
        delta = you.pos() - monster->pos();
    }
    else
    {
        delta = monster->target - monster->pos();

        if (crawl_state.arena && Options.arena_force_ai
            && !mons_is_stationary(monster))
        {
            const bool ranged =
                mons_has_ranged_attack(monster)
                || mons_has_ranged_spell(monster);

            // Smiters are happy if they have clear visibility through
            // glass, but other monsters must go around.
            const bool glass_ok = mons_has_smite_attack(monster);

            // Monsters in the arena are smarter than the norm and
            // always pathfind to their targets.
            if (delta.abs() > 2
                && (!ranged ||
                    !monster->mon_see_grid(monster->target, !glass_ok)))
            {
                monster_pathfind mp;
                if (mp.init_pathfind(monster, monster->target))
                    delta = mp.next_pos(monster->pos()) - monster->pos();
            }
        }
    }

    // Move the monster.
    mmov.x = (delta.x > 0) ? 1 : ((delta.x < 0) ? -1 : 0);
    mmov.y = (delta.y > 0) ? 1 : ((delta.y < 0) ? -1 : 0);

    if (mons_is_fleeing(monster) && monster->travel_target != MTRAV_WALL
        && (!mons_friendly(monster)
            || monster->target != you.pos()))
    {
        mmov *= -1;
    }

    // Don't allow monsters to enter a sanctuary or attack you inside a
    // sanctuary, even if you're right next to them.
    if (is_sanctuary(monster->pos() + mmov)
        && (!is_sanctuary(monster->pos())
            || monster->pos() + mmov == you.pos()))
    {
        mmov.reset();
    }

    // Bounds check: don't let fleeing monsters try to run off the grid.
    const coord_def s = monster->target + mmov;
    if (!in_bounds_x(s.x))
        mmov.x = 0;
    if (!in_bounds_y(s.y))
        mmov.y = 0;

    // Now quit if we can't move.
    if (mmov.origin())
        return;

    if (delta.rdist() > 3)
    {
        // Reproduced here is some semi-legacy code that makes monsters
        // move somewhat randomly along oblique paths.  It is an
        // exceedingly good idea, given crawl's unique line of sight
        // properties.
        //
        // Added a check so that oblique movement paths aren't used when
        // close to the target square. -- bwr

        // Sometimes we'll just move parallel the x axis.
        if (abs(delta.x) > abs(delta.y) && coinflip())
            mmov.y = 0;

        // Sometimes we'll just move parallel the y axis.
        if (abs(delta.y) > abs(delta.x) && coinflip())
            mmov.x = 0;
    }

    const coord_def newpos(monster->pos() + mmov);
    FixedArray < bool, 3, 3 > good_move;

    for (int count_x = 0; count_x < 3; count_x++)
        for (int count_y = 0; count_y < 3; count_y++)
        {
            const int targ_x = monster->pos().x + count_x - 1;
            const int targ_y = monster->pos().y + count_y - 1;

            // Bounds check: don't consider moving out of grid!
            if (!in_bounds(targ_x, targ_y))
            {
                good_move[count_x][count_y] = false;
                continue;
            }

            good_move[count_x][count_y] =
                _mon_can_move_to_pos(monster, coord_def(count_x-1, count_y-1));
        }

    if (mons_wall_shielded(monster))
    {
        if (mmov.x != 0 && mmov.y != 0)
        {
            bool updown    = false;
            bool leftright = false;

            coord_def t = monster->pos() + coord_def(mmov.x, 0);
            if (in_bounds(t) && grid_is_rock(grd(t)) && !grid_is_permarock(grd(t)))
                updown = true;

                      t = monster->pos() + coord_def(0, mmov.y);
            if (in_bounds(t) && grid_is_rock(grd(t)) && !grid_is_permarock(grd(t)))
                leftright = true;

            if (updown && (!leftright || coinflip()))
                mmov.y = 0;
            else if (leftright)
                mmov.x = 0;
        }
        else if (mmov.x == 0 && monster->target.x == monster->pos().x)
        {
            bool left  = false;
            bool right = false;
            coord_def t = monster->pos() + coord_def(-1, mmov.y);
            if (in_bounds(t) && grid_is_rock(grd(t)) && !grid_is_permarock(grd(t)))
                left = true;

                      t = monster->pos() + coord_def(1, mmov.y);
            if (in_bounds(t) && grid_is_rock(grd(t)) && !grid_is_permarock(grd(t)))
                right = true;

            if (left && (!right || coinflip()))
                mmov.x = -1;
            else if (right)
                mmov.x = 1;
        }
        else if (mmov.y == 0 && monster->target.y == monster->pos().y)
        {
            bool up   = false;
            bool down = false;
            coord_def t = monster->pos() + coord_def(mmov.x, -1);
            if (in_bounds(t) && grid_is_rock(grd(t)) && !grid_is_permarock(grd(t)))
                up = true;

                      t = monster->pos() + coord_def(mmov.x, 1);
            if (in_bounds(t) && grid_is_rock(grd(t)) && !grid_is_permarock(grd(t)))
                down = true;

            if (up && (!down || coinflip()))
                mmov.y = -1;
            else if (down)
                mmov.y = 1;
        }
    }

    // If the monster is moving in your direction, whether to attack or
    // protect you, or towards a monster it intends to attack, check
    // whether we first need to take a step to the side to make sure the
    // reinforcement can follow through.

    // First, check whether the monster is smart enough to even consider
    // this.
    if ((newpos == you.pos()
         || mgrd(newpos) != NON_MONSTER && monster->foe == mgrd(newpos))
        && mons_intel(monster) >= I_ANIMAL
        && !mons_is_confused(monster) && !mons_is_caught(monster)
        && !monster->has_ench(ENCH_BERSERK))
    {
        // If the monster is moving parallel to the x or y axis, check
        // whether
        //
        // a) the neighbouring grids are blocked
        // b) there are other unblocked grids adjacent to the target
        // c) there's at least one allied monster waiting behind us.
        //
        // (For really smart monsters, also check whether there's a
        // monster farther back in the corridor that has some kind of
        // ranged attack.)
        if (mmov.y == 0)
        {
            if (!good_move[1][0] && !good_move[1][2]
                && (good_move[mmov.x+1][0] || good_move[mmov.x+1][2])
                && (_allied_monster_at(monster, coord_def(-mmov.x, -1),
                                       coord_def(-mmov.x, 0),
                                       coord_def(-mmov.x, 1))
                    || mons_intel(monster) >= I_NORMAL
                       && !mons_wont_attack_real(monster)
                       && _ranged_allied_monster_in_dir(monster,
                                                        coord_def(-mmov.x, 0))))
            {
                if (good_move[mmov.x+1][0])
                    mmov.y = -1;
                if (good_move[mmov.x+1][2] && (mmov.y == 0 || coinflip()))
                    mmov.y = 1;
            }
        }
        else if (mmov.x == 0)
        {
            if (!good_move[0][1] && !good_move[2][1]
                && (good_move[0][mmov.y+1] || good_move[2][mmov.y+1])
                && (_allied_monster_at(monster, coord_def(-1, -mmov.y),
                                       coord_def(0, -mmov.y),
                                       coord_def(1, -mmov.y))
                    || mons_intel(monster) >= I_NORMAL
                       && !mons_wont_attack_real(monster)
                       && _ranged_allied_monster_in_dir(monster,
                                                        coord_def(0, -mmov.y))))
            {
                if (good_move[0][mmov.y+1])
                    mmov.x = -1;
                if (good_move[2][mmov.y+1] && (mmov.x == 0 || coinflip()))
                    mmov.x = 1;
            }
        }
        else // We're moving diagonally.
        {
            if (good_move[mmov.x+1][1])
            {
                if (!good_move[1][mmov.y+1]
                    && (_allied_monster_at(monster, coord_def(-mmov.x, -1),
                                           coord_def(-mmov.x, 0),
                                           coord_def(-mmov.x, 1))
                        || mons_intel(monster) >= I_NORMAL
                           && !mons_wont_attack_real(monster)
                           && _ranged_allied_monster_in_dir(monster,
                                                coord_def(-mmov.x, -mmov.y))))
                {
                    mmov.y = 0;
                }
            }
            else if (good_move[1][mmov.y+1]
                     && (_allied_monster_at(monster, coord_def(-1, -mmov.y),
                                           coord_def(0, -mmov.x),
                                           coord_def(1, -mmov.x))
                         || mons_intel(monster) >= I_NORMAL
                            && !mons_wont_attack_real(monster)
                            && _ranged_allied_monster_in_dir(monster,
                                                coord_def(-mmov.x, -mmov.y))))
            {
                mmov.x = 0;
            }
        }
    }

    // Now quit if we can't move.
    if (mmov.origin())
        return;

    // Try to stay in sight of the player if we're moving towards
    // him/her, in order to avoid the monster coming into view,
    // shouting, and then taking a step in a path to the player which
    // temporarily takes it out of view, which can lead to the player
    // getting "comes into view" and shout messages with no monster in
    // view.

    // Doesn't matter for arena mode.
    if (crawl_state.arena)
        return;

    // Did we just come into view?
    if (monster->seen_context != _just_seen)
        return;

    monster->seen_context.clear();

    // If the player can't see us, it doesn't matter.
    if (!(monster->flags & MF_WAS_IN_VIEW))
        return;

    const coord_def old_pos  = monster->pos();
    const int       old_dist = grid_distance(you.pos(), old_pos);

    // We're not moving towards the player.
    if (grid_distance(you.pos(), old_pos + mmov) >= old_dist)
    {
        // Give a message if we move back out of view.
        monster->seen_context = _just_seen;
        return;
    }

    // We're already staying in the player's LOS.
    if (see_grid(old_pos + mmov))
        return;

    // Try to find a move that brings us closer to the player while
    // keeping us in view.
    int matches = 0;
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
        {
            if (i == 0 && j == 0)
                continue;

            if (!good_move[i][j])
                continue;

            delta.set(i - 1, j - 1);
            coord_def tmp = old_pos + delta;

            if (grid_distance(you.pos(), tmp) < old_dist && see_grid(tmp))
            {
                if (one_chance_in(++matches))
                    mmov = delta;
                break;
            }
        }

    // The only way to get closer to the player is to step out of view;
    // give a message so they player isn't confused about its being
    // announced as coming into view but not being seen.
    monster->seen_context = _just_seen;
}

static void _make_mons_stop_fleeing(monsters *mon)
{
    if (mons_is_fleeing(mon))
        behaviour_event(mon, ME_CORNERED);
}

static bool _is_player_or_mon_sanct(const monsters* monster)
{
    return (is_sanctuary(you.pos())
            || is_sanctuary(monster->pos()));
}

bool mons_avoids_cloud(const monsters *monster, cloud_type cl_type,
                       bool placement, bool extra_careful)
{
    if (placement)
        extra_careful = true;

    switch (cl_type)
    {
    case CLOUD_MIASMA:
        // Even the dumbest monsters will avoid miasma if they can.
        return (mons_res_miasma(monster) <= 0);

    case CLOUD_FIRE:
        if (mons_res_fire(monster) > 1)
            return (false);

        if (extra_careful)
            return (true);

        if (monster->hit_points >= 15 + random2avg(46, 5))
            return (false);
        break;

    case CLOUD_STINK:
        if (mons_res_poison(monster) > 0)
            return (false);
        if (extra_careful)
            return (true);
        if (x_chance_in_y(monster->hit_dice - 1, 5))
            return (false);
        if (monster->hit_points >= random2avg(19, 2))
            return (false);
        break;

    case CLOUD_COLD:
        if (mons_res_cold(monster) > 1)
            return (false);

        if (extra_careful)
            return (true);

        if (monster->hit_points >= 15 + random2avg(46, 5))
            return (false);
        break;

    case CLOUD_POISON:
        if (mons_res_poison(monster) > 0)
            return (false);

        if (extra_careful)
            return (true);

        if (monster->hit_points >= random2avg(37, 4))
            return (false);
        break;

    case CLOUD_GREY_SMOKE:
        if (placement)
            return (false);

        // This isn't harmful, but dumb critters might think so.
        if (mons_intel(monster) > I_ANIMAL || coinflip())
            return (false);

        if (mons_res_fire(monster) > 0)
            return (false);

        if (monster->hit_points >= random2avg(19, 2))
            return (false);
        break;

    default:
        break;
    }

    // Exceedingly dumb creatures will wander into harmful clouds.
    if (is_harmless_cloud(cl_type)
        || mons_intel(monster) == I_PLANT && !extra_careful)
    {
        return (false);
    }

    // If we get here, the cloud is potentially harmful.
    return (true);
}

// Like the above, but prevents monsters from moving into cloud if it
// would anger the player's god, and also allows a monster to move from
// one damaging cloud to another, even if they're of different types.
bool mons_avoids_cloud(const monsters *monster, int cloud_num,
                       cloud_type *cl_type, bool placement)
{
    if (cloud_num == EMPTY_CLOUD)
    {
        if (cl_type != NULL)
            *cl_type = CLOUD_NONE;

        return (false);
    }

    const cloud_struct &cloud = env.cloud[cloud_num];

    if (cl_type != NULL)
        *cl_type = cloud.type;

    const bool careful_friendly
        = YOU_KILL(cloud.killer) && mons_friendly(monster)
       && god_hates_attacking_friend(you.religion, monster);

    // Is the target cloud okay?
    if (!mons_avoids_cloud(monster, cloud.type, placement, careful_friendly))
        return (false);

    // If we're already in a cloud that we'd want to avoid then moving
    // from one to the other is okay.
    if (!in_bounds(monster->pos()) || monster->pos() == cloud.pos)
        return (true);

    const int our_cloud_num = env.cgrid(monster->pos());

    if (our_cloud_num == EMPTY_CLOUD)
        return (true);

    const cloud_struct &our_cloud = env.cloud[our_cloud_num];

    // Don't move monster from a cloud that won't anger their god to one
    // that will.
    if (!YOU_KILL(our_cloud.killer) && careful_friendly)
        return (true);

    return (!mons_avoids_cloud(monster, our_cloud.type, true,
                               careful_friendly));
}

//---------------------------------------------------------------
//
// handle_nearby_ability
//
// Gives monsters a chance to use a special ability when they're
// next to the player.
//
//---------------------------------------------------------------
static void _handle_nearby_ability(monsters *monster)
{
    actor *foe = monster->get_foe();
    if (!foe
        || !mons_near(monster, monster->foe)
        || !monster->can_see(foe)
        || mons_is_sleeping(monster)
        || mons_is_submerged(monster))
    {
        return;
    }

#define MON_SPEAK_CHANCE 21

    if (monster->is_patrolling() || mons_is_wandering(monster)
        || monster->attitude == ATT_NEUTRAL)
    {
        // Very fast wandering/patrolling monsters might, in one monster turn,
        // move into the player's LOS and then back out (or the player
        // might move into their LOS and the monster move back out before
        // the player's view has a chance to update) so prevent them
        // from speaking.
        ;
    }
    else if ((mons_class_flag(monster->type, M_SPEAKS)
                    || !monster->mname.empty())
                && one_chance_in(MON_SPEAK_CHANCE))
    {
        mons_speaks(monster);
    }
    else if (get_mon_shape(monster) >= MON_SHAPE_QUADRUPED)
    {
        // Non-humanoid-ish monsters have a low chance of speaking
        // without the M_SPEAKS flag, to give the dungeon some
        // atmosphere/flavour.
        int chance = MON_SPEAK_CHANCE * 4;

        // Band members are a lot less likely to speak, since there's
        // a lot of them.
        if (testbits(monster->flags, MF_BAND_MEMBER))
            chance *= 10;

        // However, confused and fleeing monsters are more interesting.
        if (mons_is_fleeing(monster))
            chance /= 2;
        if (monster->has_ench(ENCH_CONFUSION))
            chance /= 2;

        if (one_chance_in(chance))
            mons_speaks(monster);
    }
    // Okay then, don't speak.

    if (monster_can_submerge(monster, grd(monster->pos()))
        && !player_mesmerised_by(monster) // No submerging if player entranced.
        && !mons_is_lurking(monster)  // Handled elsewhere.
        && monster->wants_submerge())
    {
        monsterentry* entry = get_monster_data(monster->type);

        monster->add_ench(ENCH_SUBMERGED);
        monster->speed_increment -= ENERGY_SUBMERGE(entry);
        update_beholders(monster);
        return;
    }

    switch (monster->type)
    {
    case MONS_SPATIAL_VORTEX:
    case MONS_KILLER_KLOWN:
        // Choose random colour.
        monster->colour = random_colour();
        break;

    case MONS_GIANT_EYEBALL:
        if (coinflip()
            && !mons_is_wandering(monster)
            && !mons_is_fleeing(monster)
            && !mons_is_pacified(monster)
            && !_is_player_or_mon_sanct(monster))
        {
            if (you.can_see(monster) && you.can_see(foe))
                mprf("%s stares at %s.",
                     monster->name(DESC_CAP_THE).c_str(),
                     foe->name(DESC_NOCAP_THE).c_str());

            // Subtly different from old paralysis behaviour, but
            // it'll do.
            foe->paralyse(monster, 2 + random2(3));
        }
        break;

    case MONS_EYE_OF_DRAINING:
        if (coinflip()
            && foe->atype() == ACT_PLAYER
            && !mons_is_wandering(monster)
            && !mons_is_fleeing(monster)
            && !mons_is_pacified(monster)
            && !_is_player_or_mon_sanct(monster))
        {
            simple_monster_message(monster, " stares at you.");

            dec_mp(5 + random2avg(13, 3));

            heal_monster(monster, 10, true); // heh heh {dlb}
        }
        break;

    case MONS_AIR_ELEMENTAL:
        if (one_chance_in(5))
            monster->add_ench(ENCH_SUBMERGED);
        break;

    case MONS_PANDEMONIUM_DEMON:
        if (monster->ghost->cycle_colours)
            monster->colour = random_colour();
        break;

    default: break;
    }
}

// Returns true if you resist the siren's call.
static bool _siren_movement_effect(const monsters *monster)
{
    bool do_resist = (you.attribute[ATTR_HELD] || you_resist_magic(70));

    if (!do_resist)
    {
        coord_def dir(coord_def(0,0));
        if (monster->pos().x < you.pos().x)
            dir.x = -1;
        else if (monster->pos().x > you.pos().x)
            dir.x = 1;
        if (monster->pos().y < you.pos().y)
            dir.y = -1;
        else if (monster->pos().y > you.pos().y)
            dir.y = 1;

        const coord_def newpos = you.pos() + dir;

        if (!in_bounds(newpos) || is_grid_dangerous(grd(newpos))
            || !you.can_pass_through_feat(grd(newpos)))
        {
            do_resist = true;
        }
        else
        {
            bool swapping = false;
            monsters *mon = NULL;
            if (mgrd(newpos) != NON_MONSTER)
            {
                mon = &menv[mgrd(newpos)];
                if (mons_wont_attack(mon)
                    && !mons_is_stationary(mon)
                    && !mons_cannot_act(mon)
                    && !mons_is_sleeping(mon)
                    && swap_check(mon, you.pos(), true))
                {
                    swapping = true;
                }
                else if (!mons_is_submerged(mon))
                    do_resist = true;
            }

            if (!do_resist)
            {
                const coord_def oldpos = you.pos();
                mprf("The pull of her song draws you forwards.");

                if (swapping)
                {
                    if (mgrd(oldpos) != NON_MONSTER)
                    {
                        mprf("Something prevents you from swapping places "
                             "with %s.",
                             mon->name(DESC_NOCAP_THE).c_str());
                        return (do_resist);
                    }

                    int swap_mon = mgrd(newpos);
                    // Pick the monster up.
                    mgrd(newpos) = NON_MONSTER;
                    mon->moveto(oldpos);

                    // Plunk it down.
                    mgrd(mon->pos()) = swap_mon;

                    mprf("You swap places with %s.",
                         mon->name(DESC_NOCAP_THE).c_str());
                }
                move_player_to_grid(newpos, true, true, true);

                if (swapping)
                    mon->apply_location_effects(newpos);
            }
        }
    }

    return (do_resist);
}

//---------------------------------------------------------------
//
// handle_special_ability
//
//---------------------------------------------------------------
static bool _handle_special_ability(monsters *monster, bolt & beem)
{
    bool used = false;

    FixedArray < unsigned int, 19, 19 > show;

    const monster_type mclass = (mons_genus( monster->type ) == MONS_DRACONIAN)
                                  ? draco_subspecies( monster )
                                  : static_cast<monster_type>( monster->type );

    if (!mons_near(monster)
        || mons_is_sleeping(monster)
        || mons_is_submerged(monster))
    {
        return (false);
    }

    const msg_channel_type spl = (mons_friendly(monster) ? MSGCH_FRIEND_SPELL
                                                         : MSGCH_MONSTER_SPELL);

    spell_type spell = SPELL_NO_SPELL;

    switch (mclass)
    {
    case MONS_ORC_KNIGHT:
    case MONS_ORC_WARLORD:
    case MONS_SAINT_ROKA:
        if (is_sanctuary(monster->pos()))
            break;

        used = orc_battle_cry(monster);
        break;

    case MONS_ORANGE_STATUE:
        if (_is_player_or_mon_sanct(monster))
            break;

        used = orange_statue_effects(monster);
        break;

    case MONS_SILVER_STATUE:
        if (_is_player_or_mon_sanct(monster))
        {
            break;
        }
        used = silver_statue_effects(monster);
        break;

    case MONS_BALL_LIGHTNING:
        if (is_sanctuary(monster->pos()))
            break;

        if (monster->attitude == ATT_HOSTILE
            && distance(you.pos(), monster->pos()) <= 5)
        {
            monster->hit_points = -1;
            used = true;
            break;
        }

        for (int i = 0; i < MAX_MONSTERS; i++)
        {
            monsters *targ = &menv[i];

            if (targ->type == -1 || targ->type == MONS_PROGRAM_BUG)
                continue;

            if (distance(monster->pos(), targ->pos()) >= 5)
                continue;

            if (mons_atts_aligned(monster->attitude, targ->attitude))
                continue;

            // Faking LOS by checking the neighbouring square.
            coord_def diff = targ->pos() - monster->pos();
            coord_def sg(sgn(diff.x), sgn(diff.y));
            coord_def t = monster->pos() + sg;

            if (!inside_level_bounds(t))
                continue;

            if (!grid_is_solid(grd(t)))
            {
                monster->hit_points = -1;
                used = true;
                break;
            }
        }
        break;

    case MONS_LAVA_SNAKE:
        if (monster->has_ench(ENCH_CONFUSION))
            break;

        if (!mons_player_visible( monster ))
            break;

        if (coinflip())
            break;

        // Setup tracer.
        beem.name        = "glob of lava";
        beem.aux_source  = "glob of lava";
        beem.range       = 6;
        beem.damage      = dice_def( 3, 10 );
        beem.hit         = 20;
        beem.colour      = RED;
        beem.type        = dchar_glyph(DCHAR_FIRED_ZAP);
        beem.flavour     = BEAM_LAVA;
        beem.beam_source = monster_index(monster);
        beem.thrower     = KILL_MON;

        // Fire tracer.
        fire_tracer(monster, beem);

        // Good idea?
        if (mons_should_fire(beem))
        {
            _make_mons_stop_fleeing(monster);
            simple_monster_message(monster, " spits lava!");
            beem.fire();
            used = true;
        }
        break;

    case MONS_ELECTRICAL_EEL:
        if (monster->has_ench(ENCH_CONFUSION))
            break;

        if (!mons_player_visible( monster ))
            break;

        if (coinflip())
            break;

        // Setup tracer.
        beem.name        = "bolt of electricity";
        beem.aux_source  = "bolt of electricity";
        beem.range       = 8;
        beem.damage      = dice_def( 3, 6 );
        beem.hit         = 50;
        beem.colour      = LIGHTCYAN;
        beem.type        = dchar_glyph(DCHAR_FIRED_ZAP);
        beem.flavour     = BEAM_ELECTRICITY;
        beem.beam_source = monster_index(monster);
        beem.thrower     = KILL_MON;
        beem.is_beam     = true;

        // Fire tracer.
        fire_tracer(monster, beem);

        // Good idea?
        if (mons_should_fire(beem))
        {
            _make_mons_stop_fleeing(monster);
            simple_monster_message(monster,
                                   " shoots out a bolt of electricity!");
            beem.fire();
            used = true;
        }
        break;

    case MONS_ACID_BLOB:
    case MONS_OKLOB_PLANT:
    case MONS_YELLOW_DRACONIAN:
        if (monster->has_ench(ENCH_CONFUSION))
            break;

        if (_is_player_or_mon_sanct(monster))
            break;

        if (one_chance_in(3))
        {
            spell = SPELL_ACID_SPLASH;
            setup_mons_cast(monster, beem, spell);

            // Fire tracer.
            fire_tracer(monster, beem);

            // Good idea?
            if (mons_should_fire(beem))
            {
                _make_mons_stop_fleeing(monster);
                mons_cast(monster, beem, spell);
                mmov.reset();
                used = true;
            }
        }
        break;

    case MONS_MOTH_OF_WRATH:
        if (one_chance_in(3))
            used = moth_incite_monsters(monster);
        break;

    case MONS_SNORG:
        if (monster->has_ench(ENCH_CONFUSION))
            break;

        if (monster->foe == MHITNOT
            || monster->foe == MHITYOU && mons_friendly(monster))
        {
            break;
        }

        // There's a 5% chance of Snorg spontaneously going berserk that
        // increases to 20% once he is wounded.
        if (monster->hit_points == monster->max_hit_points && !one_chance_in(4))
            break;

        if (one_chance_in(5))
            monster->go_berserk(true);
        break;

    case MONS_PIT_FIEND:
        if (one_chance_in(3))
            break;
        // deliberate fall through
    case MONS_FIEND:
        if (monster->has_ench(ENCH_CONFUSION))
            break;

        if (_is_player_or_mon_sanct(monster))
            break;

        // Friendly fiends won't use torment, preferring hellfire
        // (right now there is no way a monster can predict how
        // badly they'll damage the player with torment) -- GDL

        // Well, I guess you could allow it if the player is torment
        // resistant, but there's a very good reason torment resistant
        // players can't cast Torment themselves, and allowing your
        // allies to cast it would just introduce harmless Torment
        // through the backdoor. Thus, shouldn't happen. (jpeg)
        if (one_chance_in(4))
        {
            spell_type spell_cast = SPELL_NO_SPELL;

            switch (random2(4))
            {
            case 0:
                if (!mons_friendly(monster))
                {
                    _make_mons_stop_fleeing(monster);
                    spell_cast = SPELL_SYMBOL_OF_TORMENT;
                    mons_cast(monster, beem, spell_cast);
                    used = true;
                    break;
                }
                // deliberate fallthrough -- see above
            case 1:
            case 2:
            case 3:
                spell_cast = SPELL_HELLFIRE;
                setup_mons_cast(monster, beem, spell_cast);

                // Fire tracer.
                fire_tracer(monster, beem);

                // Good idea?
                if (mons_should_fire(beem))
                {
                    _make_mons_stop_fleeing(monster);

                    mons_cast(monster, beem, spell_cast);
                    used = true;
                }
                break;
            }

            mmov.reset();
        }
        break;

    case MONS_IMP:
    case MONS_PHANTOM:
    case MONS_INSUBSTANTIAL_WISP:
    case MONS_BLINK_FROG:
    case MONS_KILLER_KLOWN:
    case MONS_PRINCE_RIBBIT:
        if (one_chance_in(7) || mons_is_caught(monster) && one_chance_in(3))
        {
            simple_monster_message(monster, " blinks.");
            used = monster_blink(monster);
        }
        break;

    case MONS_MANTICORE:
        if (monster->has_ench(ENCH_CONFUSION))
            break;

        if (!mons_player_visible( monster ))
            break;

        // The fewer spikes the manticore has left, the less
        // likely it will use them.
        if (random2(16) >= static_cast<int>(monster->number))
            break;

        // Do the throwing right here, since the beam is so
        // easy to set up and doesn't involve inventory.

        // Set up the beam.
        beem.name        = "volley of spikes";
        beem.aux_source  = "volley of spikes";
        beem.range       = 6;
        beem.hit         = 14;
        beem.damage      = dice_def( 2, 10 );
        beem.beam_source = monster_index(monster);
        beem.type        = dchar_glyph(DCHAR_FIRED_MISSILE);
        beem.colour      = LIGHTGREY;
        beem.flavour     = BEAM_MISSILE;
        beem.thrower     = KILL_MON;
        beem.is_beam     = false;

        // Fire tracer.
        fire_tracer(monster, beem);

        // Good idea?
        if (mons_should_fire(beem))
        {
            _make_mons_stop_fleeing(monster);
            simple_monster_message(monster, " flicks its tail!");
            beem.fire();
            used = true;
            // Decrement # of volleys left.
            monster->number--;
        }
        break;

    case MONS_PLAYER_GHOST:
    {
        const ghost_demon &ghost = *(monster->ghost);

        if (ghost.species < SP_RED_DRACONIAN
            || ghost.species == SP_GREY_DRACONIAN
            || ghost.species >= SP_BASE_DRACONIAN
            || ghost.xl < 7
            || one_chance_in(ghost.xl - 5))
        {
            break;
        }
    }
    // Intentional fallthrough

    case MONS_WHITE_DRACONIAN:
    case MONS_RED_DRACONIAN:
        spell = SPELL_DRACONIAN_BREATH;
    // Intentional fallthrough

    case MONS_ICE_DRAGON:
        if (spell == SPELL_NO_SPELL)
            spell = SPELL_COLD_BREATH;
    // Intentional fallthrough

    // Dragon breath weapons:
    case MONS_DRAGON:
    case MONS_HELL_HOUND:
    case MONS_LINDWURM:
    case MONS_FIREDRAKE:
    case MONS_XTAHUA:
        if (spell == SPELL_NO_SPELL)
            spell = SPELL_FIRE_BREATH;

        if (monster->has_ench(ENCH_CONFUSION))
            break;

        if (!mons_player_visible( monster ))
            break;

        if (monster->type != MONS_HELL_HOUND && x_chance_in_y(3, 13)
            || one_chance_in(10))
        {
            setup_mons_cast(monster, beem, spell);

            // Fire tracer.
            fire_tracer(monster, beem);

            // Good idea?
            if (mons_should_fire(beem))
            {
                _make_mons_stop_fleeing(monster);
                mons_cast(monster, beem, spell);
                mmov.reset();
                used = true;
            }
        }
        break;

    case MONS_MERMAID:
    case MONS_SIREN:
    {
        // Don't behold observer in the arena.
        if (crawl_state.arena)
            break;

        // Don't behold player already half down or up the stairs.
        if (!you.delay_queue.empty())
        {
            delay_queue_item delay = you.delay_queue.front();

            if (delay.type == DELAY_ASCENDING_STAIRS
                || delay.type == DELAY_DESCENDING_STAIRS)
            {
#ifdef DEBUG_DIAGNOSTICS
                mpr("Taking stairs, don't mesmerise.", MSGCH_DIAGNOSTICS);
#endif
                break;
            }
        }

        // Won't sing if either of you silenced, or it's friendly,
        // confused, fleeing, or leaving the level.
        if (monster->has_ench(ENCH_CONFUSION)
            || mons_is_fleeing(monster)
            || mons_is_pacified(monster)
            || mons_friendly(monster)
            || !player_can_hear(monster->pos()))
        {
            break;
        }

        // Don't even try on berserkers. Mermaids know their limits.
        if (you.duration[DUR_BERSERKER])
            break;

        // Reduce probability because of spamminess.
        if (you.species == SP_MERFOLK && !one_chance_in(4))
            break;

        // A wounded invisible mermaid is less likely to give away her position.
        if (monster->invisible()
            && monster->hit_points <= monster->max_hit_points / 2
            && !one_chance_in(3))
        {
            break;
        }

        bool already_mesmerised = player_mesmerised_by(monster);

        if (one_chance_in(5)
            || monster->foe == MHITYOU && !already_mesmerised && coinflip())
        {
            noisy(12, monster->pos(), NULL, true);

            bool did_resist = false;
            if (player_monster_visible(monster))
            {
                simple_monster_message(monster,
                    make_stringf(" chants %s song.",
                    already_mesmerised ? "her luring" : "a haunting").c_str(),
                    spl);

                if (monster->type == MONS_SIREN)
                {
                    if (_siren_movement_effect(monster))
                    {
                        canned_msg(MSG_YOU_RESIST); // flavour only
                        did_resist = true;
                    }
                }
            }
            else
            {
                // If you're already mesmerised by an invisible mermaid she
                // can still prolong the enchantment; otherwise you "resist".
                if (already_mesmerised)
                    mpr("You hear a luring song.", MSGCH_SOUND);
                else
                {
                    if (one_chance_in(4)) // reduce spamminess
                    {
                        if (coinflip())
                            mpr("You hear a haunting song.", MSGCH_SOUND);
                        else
                            mpr("You hear an eerie melody.", MSGCH_SOUND);

                        canned_msg(MSG_YOU_RESIST); // flavour only
                    }
                    break;
                }
            }

            // Once mesmerised by a particular monster, you cannot resist
            // anymore.
            if (!already_mesmerised
                && (you.species == SP_MERFOLK || you_resist_magic(100)))
            {
                if (!did_resist)
                    canned_msg(MSG_YOU_RESIST);
                break;
            }

            if (!you.duration[DUR_MESMERISED])
            {
                you.duration[DUR_MESMERISED] = 7;
                you.mesmerised_by.push_back(monster_index(monster));
                mprf(MSGCH_WARN, "You are mesmerised by %s!",
                                 monster->name(DESC_NOCAP_THE).c_str());
            }
            else
            {
                you.duration[DUR_MESMERISED] += 5;
                if (!already_mesmerised)
                    you.mesmerised_by.push_back(monster_index(monster));
            }
            used = true;

            if (you.duration[DUR_MESMERISED] > 12)
                you.duration[DUR_MESMERISED] = 12;
        }
        break;
    }
    default:
        break;
    }

    if (used)
        monster->lose_energy(EUT_SPECIAL);

    return (used);
}

//---------------------------------------------------------------
//
// _handle_potion
//
// Give the monster a chance to quaff a potion. Returns true if
// the monster imbibed.
//
//---------------------------------------------------------------
static bool _handle_potion(monsters *monster, bolt & beem)
{
    if (mons_is_sleeping(monster)
        || monster->inv[MSLOT_POTION] == NON_ITEM
        || !one_chance_in(3))
    {
        return (false);
    }

    bool rc = false;

    const int potion_idx = monster->inv[MSLOT_POTION];
    item_def& potion = mitm[potion_idx];
    const potion_type ptype = static_cast<potion_type>(potion.sub_type);

    if (monster->can_drink_potion(ptype) && monster->should_drink_potion(ptype))
    {
        const bool was_visible = you.can_see(monster);

        // Drink the potion.
        const item_type_id_state_type id = monster->drink_potion_effect(ptype);

        // Give ID if necessary.
        if (was_visible && id != ID_UNKNOWN_TYPE)
            set_ident_type(OBJ_POTIONS, ptype, id);

        // Remove it from inventory.
        if (dec_mitm_item_quantity(potion_idx, 1))
            monster->inv[MSLOT_POTION] = NON_ITEM;
        else if (is_blood_potion(potion))
            remove_oldest_blood_potion(potion);

        monster->lose_energy(EUT_ITEM);
        rc = true;
    }

    return (rc);
}

static bool _handle_reaching(monsters *monster)
{
    bool       ret = false;
    const int  wpn = monster->inv[MSLOT_WEAPON];

    if (mons_is_submerged(monster))
        return (false);

    if (mons_aligned(monster_index(monster), monster->foe))
        return (false);

    if (wpn != NON_ITEM && get_weapon_brand(mitm[wpn]) == SPWPN_REACHING)
    {
        if (monster->foe == MHITYOU)
        {
            // This check isn't redundant -- player may be invisible.
            if (monster->target == you.pos()
                && see_grid_no_trans(monster->pos())
                && grid_distance(monster->pos(), you.pos()) == 2)
            {
                ret = true;
                monster_attack(monster, false);
            }
        }
        else if (monster->foe != MHITNOT)
        {
            monsters& mfoe = menv[monster->foe];
            coord_def foepos = mfoe.pos();
            // Same comments as to invisibility as above.
            if (monster->target == foepos
                && monster->mon_see_grid(foepos, true)
                && grid_distance(monster->pos(), foepos) == 2)
            {
                ret = true;
                monsters_fight(monster, &mfoe, false);
            }
        }
    }

    // Player saw the item reach.
    if (ret && !is_artefact(mitm[wpn]) && mons_near(monster)
        && player_monster_visible(monster))
    {
        set_ident_flags(mitm[wpn], ISFLAG_KNOW_TYPE);
    }

    return ret;
}

//---------------------------------------------------------------
//
// handle_scroll
//
// Give the monster a chance to read a scroll. Returns true if
// the monster read something.
//
//---------------------------------------------------------------
static bool _handle_scroll(monsters *monster)
{
    // Yes, there is a logic to this ordering {dlb}:
    if (mons_is_sleeping(monster)
        || mons_is_confused(monster)
        || mons_is_submerged(monster)
        || monster->inv[MSLOT_SCROLL] == NON_ITEM
        || !one_chance_in(3))
    {
        return (false);
    }

    bool                    read        = false;
    item_type_id_state_type ident       = ID_UNKNOWN_TYPE;
    bool                    was_visible =
        mons_near(monster) && player_monster_visible(monster);

    // Notice how few cases are actually accounted for here {dlb}:
    const int scroll_type = mitm[monster->inv[MSLOT_SCROLL]].sub_type;
    switch (scroll_type)
    {
    case SCR_TELEPORTATION:
        if (!monster->has_ench(ENCH_TP))
        {
            if (mons_is_caught(monster) || mons_is_fleeing(monster)
                || mons_is_pacified(monster))
            {
                simple_monster_message(monster, " reads a scroll.");
                monster_teleport(monster, false);
                read  = true;
                ident = ID_KNOWN_TYPE;
            }
        }
        break;

    case SCR_BLINKING:
        if (mons_is_caught(monster) || mons_is_fleeing(monster)
            || mons_is_pacified(monster))
        {
            if (mons_near(monster))
            {
                simple_monster_message(monster, " reads a scroll.");
                simple_monster_message(monster, " blinks!");
                monster_blink(monster);
                read  = true;
                ident = ID_KNOWN_TYPE;
            }
        }
        break;

    case SCR_SUMMONING:
        if (mons_near(monster))
        {
            simple_monster_message(monster, " reads a scroll.");
            const int mon = create_monster(
                mgen_data(MONS_ABOMINATION_SMALL, SAME_ATTITUDE(monster),
                          0, 0, monster->pos(), monster->foe, MG_FORCE_BEH));
            read  = true;
            if (mon != -1)
            {
                if (you.can_see(&menv[mon]))
                {
                    mprf("%s appears!", menv[mon].name(DESC_CAP_A).c_str());
                    ident = ID_KNOWN_TYPE;
                }
                player_angers_monster(&menv[mon]);
            }
            else if (you.can_see(monster))
                canned_msg(MSG_NOTHING_HAPPENS);
        }
        break;
    }

    if (read)
    {
        if (dec_mitm_item_quantity(monster->inv[MSLOT_SCROLL], 1))
            monster->inv[MSLOT_SCROLL] = NON_ITEM;

        if (ident != ID_UNKNOWN_TYPE && was_visible)
            set_ident_type(OBJ_SCROLLS, scroll_type, ident);

        monster->lose_energy(EUT_ITEM);
    }

    return read;
}

//---------------------------------------------------------------
//
// handle_wand
//
// Give the monster a chance to zap a wand. Returns true if the
// monster zapped.
//
//---------------------------------------------------------------
static bool _handle_wand(monsters *monster, bolt &beem)
{
    // Yes, there is a logic to this ordering {dlb}:
    if (!mons_near(monster)
        || mons_is_sleeping(monster)
        || monster->has_ench(ENCH_SUBMERGED)
        || monster->inv[MSLOT_WAND] == NON_ITEM
        || mitm[monster->inv[MSLOT_WAND]].plus <= 0
        || coinflip())
    {
        return (false);
    }

    bool niceWand    = false;
    bool zap         = false;
    bool was_visible = (mons_near(monster) && player_monster_visible(monster));

    item_def &wand(mitm[monster->inv[MSLOT_WAND]]);

    // map wand type to monster spell type
    const spell_type mzap = _map_wand_to_mspell(wand.sub_type);
    if (mzap == SPELL_NO_SPELL)
        return (false);

    // set up the beam
    int power         = 30 + monster->hit_dice;
    bolt theBeam      = mons_spells(monster, mzap, power);

    beem.name         = theBeam.name;
    beem.beam_source  = monster_index(monster);
    beem.source       = monster->pos();
    beem.colour       = theBeam.colour;
    beem.range        = theBeam.range;
    beem.damage       = theBeam.damage;
    beem.ench_power   = theBeam.ench_power;
    beem.hit          = theBeam.hit;
    beem.type         = theBeam.type;
    beem.flavour      = theBeam.flavour;
    beem.thrower      = theBeam.thrower;
    beem.is_beam      = theBeam.is_beam;
    beem.is_explosion = theBeam.is_explosion;

#if HISCORE_WEAPON_DETAIL
    beem.aux_source =
        wand.name(DESC_QUALNAME, false, true, false, false);
#else
    beem.aux_source =
        wand.name(DESC_QUALNAME, false, true, false, false,
                  ISFLAG_KNOW_CURSE | ISFLAG_KNOW_PLUSES);
#endif

    const int wand_type = wand.sub_type;
    switch (wand_type)
    {
    case WAND_DISINTEGRATION:
        // Dial down damage from wands of disintegration, since
        // disintegration beams can do large amounts of damage.
        beem.damage.size = beem.damage.size * 2 / 3;
        break;

    case WAND_ENSLAVEMENT:
    case WAND_DIGGING:
    case WAND_RANDOM_EFFECTS:
        // These have been deemed "too tricky" at this time {dlb}:
        return (false);

    case WAND_POLYMORPH_OTHER:
         // Monsters can be very trigger happy with wands, reduce this
         // for polymorph.
         if (!one_chance_in(5))
             return (false);
         break;

    // These are wands that monsters will aim at themselves {dlb}:
    case WAND_HASTING:
        if (!monster->has_ench(ENCH_HASTE))
        {
            beem.target = monster->pos();
            niceWand = true;
            break;
        }
        return (false);

    case WAND_HEALING:
        if (monster->hit_points <= monster->max_hit_points / 2)
        {
            beem.target = monster->pos();
            niceWand = true;
            break;
        }
        return (false);

    case WAND_INVISIBILITY:
        if (!monster->has_ench(ENCH_INVIS)
            && !monster->has_ench(ENCH_SUBMERGED)
            && (!mons_friendly(monster) || player_see_invis(false)))
        {
            beem.target = monster->pos();
            niceWand = true;
            break;
        }
        return (false);

    case WAND_TELEPORTATION:
        if (monster->hit_points <= monster->max_hit_points / 2
            || mons_is_caught(monster))
        {
            if (!monster->has_ench(ENCH_TP)
                && !one_chance_in(20))
            {
                beem.target = monster->pos();
                niceWand = true;
                break;
            }
            // This break causes the wand to be tried on the player.
            break;
        }
        return (false);
    }

    // Fire tracer, if necessary.
    if (!niceWand)
    {
        fire_tracer( monster, beem );

        // Good idea?
        zap = mons_should_fire(beem);
    }

    if (niceWand || zap)
    {
        if (!niceWand)
            _make_mons_stop_fleeing(monster);

        if (!simple_monster_message(monster, " zaps a wand."))
        {
            if (!silenced(you.pos()))
                mpr("You hear a zap.", MSGCH_SOUND);
        }

        // charge expenditure {dlb}
        wand.plus--;
        beem.is_tracer = false;
        beem.fire();

        if (was_visible)
        {
            if (niceWand || !beem.is_enchantment() || beem.obvious_effect)
                set_ident_type(OBJ_WANDS, wand_type, ID_KNOWN_TYPE);
            else
                set_ident_type(OBJ_WANDS, wand_type, ID_MON_TRIED_TYPE);

            // Increment zap count.
            if (wand.plus2 >= 0)
                wand.plus2++;
        }

        monster->lose_energy(EUT_ITEM);

        return (true);
    }

    return (false);
}

// Returns a suitable breath weapon for the draconian; does not handle all
// draconians, does fire a tracer.
static spell_type _get_draconian_breath_spell( monsters *monster )
{
    spell_type draco_breath = SPELL_NO_SPELL;

    if (mons_genus( monster->type ) == MONS_DRACONIAN)
    {
        switch (draco_subspecies( monster ))
        {
        case MONS_DRACONIAN:
        case MONS_YELLOW_DRACONIAN:     // already handled as ability
            break;
        default:
            draco_breath = SPELL_DRACONIAN_BREATH;
            break;
        }
    }


    if (draco_breath != SPELL_NO_SPELL)
    {
        // [ds] Check line-of-fire here. It won't happen elsewhere.
        bolt beem;
        setup_mons_cast(monster, beem, draco_breath);

        fire_tracer(monster, beem);

        if (!mons_should_fire(beem))
            draco_breath = SPELL_NO_SPELL;
    }

    return (draco_breath);
}

static bool _is_emergency_spell(const monster_spells &msp, int spell)
{
    // If the emergency spell appears early, it's probably not a dedicated
    // escape spell.
    for (int i = 0; i < 5; ++i)
        if (msp[i] == spell)
            return (false);

    return (msp[5] == spell);
}

//---------------------------------------------------------------
//
// handle_spell
//
// Give the monster a chance to cast a spell. Returns true if
// a spell was cast.
//
//---------------------------------------------------------------
static bool _handle_spell(monsters *monster, bolt &beem)
{
    bool monsterNearby = mons_near(monster);
    bool finalAnswer   = false;   // as in: "Is that your...?" {dlb}
    const spell_type draco_breath = _get_draconian_breath_spell(monster);

    if (is_sanctuary(monster->pos()) && !mons_wont_attack(monster))
        return (false);

    // Yes, there is a logic to this ordering {dlb}:
    if (mons_is_sleeping(monster)
        || mons_is_submerged(monster)
        || !mons_class_flag(monster->type, M_SPELLCASTER)
           && draco_breath == SPELL_NO_SPELL)
    {
        return (false);
    }

    if (silenced(monster->pos())
        && mons_class_flag(monster->type,
                           M_PRIEST | M_ACTUAL_SPELLS | M_SPELL_NO_SILENT))
    {
        return (false);
    }

    // Shapeshifters don't get spells.
    if (mons_is_shapeshifter(monster)
        && (mons_class_flag(monster->type, M_ACTUAL_SPELLS)
            || mons_class_flag(monster->type, M_PRIEST)))
    {
        return (false);
    }
    else if (monster->has_ench(ENCH_CONFUSION)
             && !mons_class_flag(monster->type, M_CONFUSED))
    {
        return (false);
    }
    else if (monster->type == MONS_PANDEMONIUM_DEMON
             && !monster->ghost->spellcaster)
    {
        return (false);
    }
    else if (random2(200) > monster->hit_dice + 50
             || monster->type == MONS_BALL_LIGHTNING && coinflip())
    {
        return (false);
    }
    else
    {
        spell_type spell_cast = SPELL_NO_SPELL;
        monster_spells hspell_pass(monster->spells);

        if (!mon_enemies_around(monster))
        {
            // Force the casting of dig when the player is not visible -
            // this is EVIL!
            if (monster->has_spell(SPELL_DIG)
                && mons_is_seeking(monster))
            {
                spell_cast = SPELL_DIG;
                finalAnswer = true;
            }
            else if ((monster->has_spell(SPELL_LESSER_HEALING)
                         || monster->has_spell(SPELL_GREATER_HEALING))
                     && monster->hit_points < monster->max_hit_points)
            {
                // The player's out of sight!
                // Quick, let's take a turn to heal ourselves. -- bwr
                spell_cast = monster->has_spell(SPELL_GREATER_HEALING) ?
                             SPELL_GREATER_HEALING : SPELL_LESSER_HEALING;
                finalAnswer = true;
            }
            else if (mons_is_fleeing(monster) || mons_is_pacified(monster))
            {
                // Since the player isn't around, we'll extend the monster's
                // normal choices to include the self-enchant slot.
                int foundcount = 0;
                for (int i = NUM_MONSTER_SPELL_SLOTS - 1; i >= 0; --i)
                {
                    if (ms_useful_fleeing_out_of_sight(monster, hspell_pass[i])
                        && one_chance_in(++foundcount))
                    {
                        spell_cast = hspell_pass[i];
                        finalAnswer = true;
                    }
                }
            }
            else if (monster->foe == MHITYOU && !monsterNearby)
                return (false);
        }

        // Monsters caught in a net try to get away.
        // This is only urgent if enemies are around.
        if (!finalAnswer && mon_enemies_around(monster)
            && mons_is_caught(monster) && one_chance_in(4))
        {
            for (int i = 0; i < NUM_MONSTER_SPELL_SLOTS; i++)
            {
                if (ms_quick_get_away( monster, hspell_pass[i] ))
                {
                    spell_cast = hspell_pass[i];
                    finalAnswer = true;
                    break;
                }
            }
        }

        // Promote the casting of useful spells for low-HP monsters.
        if (!finalAnswer
            && monster->hit_points < monster->max_hit_points / 4
            && !one_chance_in(4))
        {
            // Note: There should always be at least some chance we don't
            // get here... even if the monster is on its last HP.  That
            // way we don't have to worry about monsters infinitely casting
            // Healing on themselves (e.g. orc high priests).
            if ((mons_is_fleeing(monster) || mons_is_pacified(monster))
                && ms_low_hitpoint_cast(monster, hspell_pass[5]))
            {
                spell_cast = hspell_pass[5];
                finalAnswer = true;
            }

            if (!finalAnswer)
            {
                int found_spell = 0;
                for (int i = 0; i < NUM_MONSTER_SPELL_SLOTS; ++i)
                {
                    if (ms_low_hitpoint_cast( monster, hspell_pass[i] )
                        && one_chance_in(++found_spell))
                    {
                        spell_cast = hspell_pass[i];
                        finalAnswer = true;
                    }
                }
            }
        }

        if (!finalAnswer)
        {
            // If nothing found by now, safe friendlies and good
            // neutrals will rarely cast.
            if (mons_wont_attack(monster) && !mon_enemies_around(monster)
                && !one_chance_in(10))
            {
                return (false);
            }

            // Remove healing/invis/haste if we don't need them.
            int num_no_spell = 0;

            for (int i = 0; i < NUM_MONSTER_SPELL_SLOTS; i++)
            {
                if (hspell_pass[i] == SPELL_NO_SPELL)
                    num_no_spell++;
                else if (ms_waste_of_time( monster, hspell_pass[i] )
                         || hspell_pass[i] == SPELL_DIG)
                {
                    // Should monster not have selected dig by now,
                    // it never will.
                    hspell_pass[i] = SPELL_NO_SPELL;
                    num_no_spell++;
                }
            }

            // If no useful spells... cast no spell.
            if (num_no_spell == NUM_MONSTER_SPELL_SLOTS
                && draco_breath == SPELL_NO_SPELL)
            {
                return (false);
            }

            const bolt orig_beem = beem;
            // Up to four tries to pick a spell.
            for (int loopy = 0; loopy < 4; ++loopy)
            {
                beem = orig_beem;

                bool spellOK = false;

                // Setup spell - monsters that are fleeing or pacified
                // and leaving the level will always try to choose their
                // emergency spell.
                if (mons_is_fleeing(monster) || mons_is_pacified(monster))
                {
                    spell_cast = (one_chance_in(5) ? SPELL_NO_SPELL
                                                   : hspell_pass[5]);

                    // Pacified monsters leaving the level won't choose
                    // emergency spells harmful to the area.
                    if (spell_cast != SPELL_NO_SPELL
                        && mons_is_pacified(monster)
                        && spell_harms_area(spell_cast))
                    {
                        spell_cast = SPELL_NO_SPELL;
                    }
                }
                else
                {
                    // Randomly picking one of the non-emergency spells:
                    spell_cast = hspell_pass[random2(5)];
                }

                if (spell_cast == SPELL_NO_SPELL)
                    continue;

                // Setup the spell.
                setup_mons_cast(monster, beem, spell_cast);

                // beam-type spells requiring tracers
                if (spell_needs_tracer(spell_cast))
                {
                    const bool explode =
                        spell_is_direct_explosion(spell_cast);
                    fire_tracer(monster, beem, explode);
                    // Good idea?
                    if (mons_should_fire(beem))
                        spellOK = true;
                }
                else
                {
                    // All direct-effect/summoning/self-enchantments/etc.
                    spellOK = true;

                    if (ms_direct_nasty(spell_cast)
                        && mons_aligned(monster_index(monster),
                                        monster->foe))
                    {
                        spellOK = false;
                    }
                    else if (monster->foe == MHITYOU || monster->foe == MHITNOT)
                    {
                        // XXX: Note the crude hack so that monsters can
                        // use ME_ALERT to target (we should really have
                        // a measure of time instead of peeking to see
                        // if the player is still there). -- bwr
                        if (!mons_player_visible(monster)
                            && (monster->target != you.pos() || coinflip()))
                        {
                            spellOK = false;
                        }
                    }
                    else if (!mon_can_see_monster(monster,
                                                  &menv[monster->foe]))
                    {
                        spellOK = false;
                    }
                    else if (monster->type == MONS_DAEVA)
                    {
                        const monsters *mon = &menv[monster->foe];

                        // Don't allow daevas to make unchivalric magic
                        // attacks, except against appropriate monsters.
                        if (is_unchivalric_attack(monster, mon)
                            && !tso_unchivalric_attack_safe_monster(mon))
                        {
                            spellOK = false;
                        }
                    }
                }

                // If not okay, then maybe we'll cast a defensive spell.
                if (!spellOK)
                {
                    spell_cast = (coinflip() ? hspell_pass[2]
                                             : SPELL_NO_SPELL);
                }

                if (spell_cast != SPELL_NO_SPELL)
                    break;
            }
        }

        // If there's otherwise no ranged attack use the breath weapon.
        // The breath weapon is also occasionally used.
        if (draco_breath != SPELL_NO_SPELL
            && (spell_cast == SPELL_NO_SPELL
                 || !_is_emergency_spell(hspell_pass, spell_cast)
                    && one_chance_in(4))
            && !_is_player_or_mon_sanct(monster))
        {
            spell_cast = draco_breath;
            finalAnswer = true;
        }

        // Should the monster *still* not have a spell, well, too bad {dlb}:
        if (spell_cast == SPELL_NO_SPELL)
            return (false);

        // Friendly monsters don't use polymorph other, for fear of harming
        // the player.
        if (spell_cast == SPELL_POLYMORPH_OTHER && mons_friendly(monster))
            return (false);

        // Try to animate dead: if nothing rises, pretend we didn't cast it.
        if (spell_cast == SPELL_ANIMATE_DEAD
            && !animate_dead(monster, 100, SAME_ATTITUDE(monster),
                             monster->foe, GOD_NO_GOD, false))
        {
            return (false);
        }

        if (monster->type == MONS_BALL_LIGHTNING)
            monster->hit_points = -1;

        // FINALLY! determine primary spell effects {dlb}:
        if (spell_cast == SPELL_BLINK || spell_cast == SPELL_CONTROLLED_BLINK)
        {
            // Why only cast blink if nearby? {dlb}
            if (monsterNearby)
            {
                mons_cast_noise(monster, beem, spell_cast);

                simple_monster_message(monster, " blinks!");
                monster_blink(monster);

                monster->lose_energy(EUT_SPELL);
            }
            else
                return (false);
        }
        else
        {
            if (spell_needs_foe(spell_cast))
                _make_mons_stop_fleeing(monster);

            mons_cast(monster, beem, spell_cast);
            mmov.reset();
            monster->lose_energy(EUT_SPELL);
        }
    } // end "if mons_class_flag(monster->type, M_SPELLCASTER) ...

    return (true);
}

// Returns a rough estimate of damage from throwing the wielded weapon.
int mons_thrown_weapon_damage(const item_def *weap)
{
    if (!weap || get_weapon_brand(*weap) != SPWPN_RETURNING)
        return (0);

    return std::max(0, (property(*weap, PWPN_DAMAGE) + weap->plus2 / 2));
}

int mons_weapon_damage_rating(const item_def &launcher)
{
    return (property(launcher, PWPN_DAMAGE) + launcher.plus2);
}

// Returns a rough estimate of damage from firing/throwing missile.
int mons_missile_damage(monsters *mons, const item_def *launch,
                        const item_def *missile)
{
    if (!missile || (!launch && !is_throwable(mons, *missile)))
        return (0);

    const int missile_damage = property(*missile, PWPN_DAMAGE) / 2 + 1;
    const int launch_damage  = launch? property(*launch, PWPN_DAMAGE) : 0;
    return std::max(0, launch_damage + missile_damage);
}

// Given the monster's current weapon and alt weapon (either or both of
// which may be NULL), works out whether using missiles or throwing the
// main weapon (with returning brand) is better. If using missiles that
// need a launcher, sets *launcher to the launcher.
//
// If the monster has no ranged weapon attack, returns NON_ITEM.
//
int mons_pick_best_missile(monsters *mons, item_def **launcher,
                           bool ignore_melee)
{
    *launcher = NULL;
    item_def *melee = NULL, *launch = NULL;
    for (int i = MSLOT_WEAPON; i <= MSLOT_ALT_WEAPON; ++i)
    {
        if (item_def *item = mons->mslot_item(static_cast<mon_inv_type>(i)))
        {
            if (is_range_weapon(*item))
                launch = item;
            else if (!ignore_melee)
                melee = item;
        }
    }

    const item_def *missiles = mons->missiles();
    if (launch && missiles && !missiles->launched_by(*launch))
        launch = NULL;

    const int tdam = mons_thrown_weapon_damage(melee);
    const int fdam = mons_missile_damage(mons, launch, missiles);

    if (!tdam && !fdam)
        return (NON_ITEM);
    else if (tdam >= fdam)
        return (melee->index());
    else
    {
        *launcher = launch;
        return (missiles->index());
    }
}

//---------------------------------------------------------------
//
// handle_throw
//
// Give the monster a chance to throw something. Returns true if
// the monster hurled.
//
//---------------------------------------------------------------
static bool _handle_throw(monsters *monster, bolt & beem)
{
    // Yes, there is a logic to this ordering {dlb}:
    if (monster->incapacitated()
        || monster->asleep()
        || monster->submerged())
    {
        return (false);
    }

    if (mons_itemuse(monster) < MONUSE_STARTING_EQUIPMENT)
        return (false);

    const bool archer = mons_class_flag(monster->type, M_ARCHER);
    // Highly-specialised archers are more likely to shoot than talk.
    if (one_chance_in(archer? 9 : 5))
        return (false);

    // Don't allow offscreen throwing for now.
    if (monster->foe == MHITYOU && !mons_near(monster))
        return (false);

    // Monsters won't shoot in melee range, largely for balance reasons.
    // Specialist archers are an exception to this rule.
    if (!archer && adjacent(beem.target, monster->pos()))
        return (false);

    // Greatly lowered chances if the monster is fleeing or pacified and
    // leaving the level.
    if ((mons_is_fleeing(monster) || mons_is_pacified(monster))
        && !one_chance_in(8))
    {
        return (false);
    }

    item_def *launcher = NULL;
    const item_def *weapon = NULL;
    const int mon_item = mons_pick_best_missile(monster, &launcher);

    if (mon_item == NON_ITEM || !is_valid_item(mitm[mon_item]))
        return (false);

    if (_is_player_or_mon_sanct(monster))
        return (false);

    item_def *missile = &mitm[mon_item];

    // Throwing a net at a target that is already caught would be
    // completely useless, so bail out.
    if (missile->base_type == OBJ_MISSILES
        && missile->sub_type == MI_THROWING_NET
        && ( beem.target == you.pos() && you.caught()
             || mgrd(beem.target) != NON_MONSTER
             && mons_is_caught(&menv[mgrd(beem.target)])))
    {
        return (false);
    }

    // If the attack needs a launcher that we can't wield, bail out.
    if (launcher)
    {
        weapon = monster->mslot_item(MSLOT_WEAPON);
        if (weapon && weapon != launcher && weapon->cursed())
            return (false);
    }

    // Ok, we'll try it.
    setup_generic_throw( monster, beem );

    // Set fake damage for the tracer.
    beem.damage = dice_def(10, 10);

    // Set item for tracer, even though it probably won't be used
    beem.item = missile;

    // Fire tracer.
    fire_tracer( monster, beem );

    // Clear fake damage (will be set correctly in mons_throw).
    beem.damage = 0;

    // Good idea?
    if (mons_should_fire( beem ))
    {
        // Monsters shouldn't shoot if fleeing, so let them "turn to attack".
        _make_mons_stop_fleeing(monster);

        if (launcher && launcher != weapon)
            monster->swap_weapons();

        beem.name.clear();
        return (mons_throw( monster, beem, mon_item ));
    }

    return (false);
}

static bool _handle_monster_spell(monsters *monster, bolt &beem)
{
    // Shapeshifters don't get spells.
    if (!mons_is_shapeshifter(monster)
        || !mons_class_flag(monster->type, M_ACTUAL_SPELLS))
    {
        if (_handle_spell(monster, beem))
            return (true);
    }

    return (false);
}

// Give the monster its action energy (aka speed_increment).
static void _monster_add_energy(monsters *monster)
{
    if (monster->speed > 0)
    {
        // Randomise to make counting off monster moves harder:
        const int energy_gained =
            std::max(1, div_rand_round(monster->speed * you.time_taken, 10));
        monster->speed_increment += energy_gained;
    }
}

int mons_natural_regen_rate(monsters *monster)
{
    // A HD divider ranging from 3 (at 1 HD) to 1 (at 8 HD).
    int divider =
        std::max(div_rand_round(15 - monster->hit_dice, 4), 1);

    // The undead have a harder time regenerating.
    switch (monster->holiness())
    {
    // The undead don't regenerate easily.
    case MH_UNDEAD:
        divider *= 4;
        break;

    // And golems have it worse.
    case MH_NONLIVING:
        divider *= 5;
        break;

    default:
        break;
    }

    return (std::max(div_rand_round(monster->hit_dice, divider), 1));
}

static inline bool _mons_natural_regen_roll(monsters *monster)
{
    const int regen_rate = mons_natural_regen_rate(monster);
    return (x_chance_in_y(regen_rate, 25));
}

// Do natural regeneration for monster.
static void _monster_regenerate(monsters *monster)
{
    if (monster->has_ench(ENCH_SICK) || !mons_can_regenerate(monster))
        return;

    // Non-land creatures out of their element cannot regenerate.
    if (mons_primary_habitat(monster) != HT_LAND
        && !monster_habitable_grid(monster, grd(monster->pos())))
    {
        return;
    }

    if (monster_descriptor(monster->type, MDSC_REGENERATES)
        || (monster->type == MONS_FIRE_ELEMENTAL
            && (grd(monster->pos()) == DNGN_LAVA
                || cloud_type_at(monster->pos()) == CLOUD_FIRE))

        || (monster->type == MONS_WATER_ELEMENTAL
            && grid_is_watery(grd(monster->pos())))

        || (monster->type == MONS_AIR_ELEMENTAL
            && env.cgrid(monster->pos()) == EMPTY_CLOUD
            && one_chance_in(3))

        || _mons_natural_regen_roll(monster))
    {
        heal_monster(monster, 1, false);
    }
}

static bool _swap_monsters(monsters* mover, monsters* moved)
{
    // Can't swap with a stationary monster.
    if (mons_is_stationary(moved))
        return (false);

    // Swapping is a purposeful action.
    if (mover->confused())
        return (false);

    // Right now just happens in sanctuary.
    if (!is_sanctuary(mover->pos()) || !is_sanctuary(moved->pos()))
        return (false);

    // A friendly or good-neutral monster moving past a fleeing hostile
    // or neutral monster, or vice versa.
    if (mons_wont_attack_real(mover) == mons_wont_attack_real(moved)
        || mons_is_fleeing(mover) == mons_is_fleeing(moved))
    {
        return (false);
    }

    // Don't swap places if the player explicitly ordered their pet to
    // attack monsters.
    if ((mons_friendly(mover) || mons_friendly(moved))
        && you.pet_target != MHITYOU && you.pet_target != MHITNOT)
    {
        return (false);
    }

    if (!mover->can_pass_through(moved->pos())
        || !moved->can_pass_through(mover->pos()))
    {
        return (false);
    }

    if (!monster_habitable_grid(mover, grd(moved->pos()))
        || !monster_habitable_grid(moved, grd(mover->pos())))
    {
        return (false);
    }

    // Okay, we can do the swap.
    const coord_def mover_pos = mover->pos();
    const coord_def moved_pos = moved->pos();

    mover->pos() = moved_pos;
    moved->pos() = mover_pos;

    mgrd(mover->pos()) = mover->mindex();
    mgrd(moved->pos()) = moved->mindex();

    if (you.can_see(mover) && you.can_see(moved))
    {
        mprf("%s and %s swap places.", mover->name(DESC_CAP_THE).c_str(),
             moved->name(DESC_NOCAP_THE).c_str());
    }

    return (true);
}

static void _swim_or_move_energy(monsters *mon)
{
    const dungeon_feature_type feat = grd(mon->pos());

    // FIXME: Replace check with mons_is_swimming()?
    mon->lose_energy( (feat >= DNGN_LAVA && feat <= DNGN_SHALLOW_WATER
                       && !mon->airborne()) ? EUT_SWIM
                                            : EUT_MOVE );
}

#if DEBUG
#    define DEBUG_ENERGY_USE(problem) \
    if (monster->speed_increment == old_energy && monster->alive()) \
             mprf(MSGCH_DIAGNOSTICS, \
                  problem " for monster '%s' consumed no energy", \
                  monster->name(DESC_PLAIN).c_str(), true);
#else
#    define DEBUG_ENERGY_USE(problem) ((void) 0)
#endif

static void _handle_monster_move(monsters *monster)
{
    bool brkk = false;

    monster->hit_points = std::min(monster->max_hit_points,
                                   monster->hit_points);

    // Monster just summoned (or just took stairs), skip this action.
    if (testbits( monster->flags, MF_JUST_SUMMONED ))
    {
        monster->flags &= ~MF_JUST_SUMMONED;
        return;
    }

    mon_acting mact(monster);

    _monster_add_energy(monster);

    // Handle clouds on nonmoving monsters.
    if (monster->speed == 0
        && env.cgrid(monster->pos()) != EMPTY_CLOUD
        && !mons_is_submerged(monster))
    {
        _mons_in_cloud( monster );
    }

    // Apply monster enchantments once for every normal-speed
    // player turn.
    monster->ench_countdown -= you.time_taken;
    while (monster->ench_countdown < 0)
    {
        monster->ench_countdown += 10;
        monster->apply_enchantments();

        // If the monster *merely* died just break from the loop
        // rather than quit altogether, since we have to deal with
        // giant spores and ball lightning exploding at the end of the
        // function, but do return if the monster's data has been
        // reset, since then the monster type is invalid.
        if (monster->type == -1)
            return;
        else if (monster->hit_points < 1)
            break;
    }

    // Memory is decremented here for a reason -- we only want it
    // decrementing once per monster "move".
    if (monster->foe_memory > 0)
        monster->foe_memory--;

    // Otherwise there are potential problems with summonings.
    if (monster->type == MONS_GLOWING_SHAPESHIFTER)
        monster->add_ench(ENCH_GLOWING_SHAPESHIFTER);

    if (monster->type == MONS_SHAPESHIFTER)
        monster->add_ench(ENCH_SHAPESHIFTER);

    // We reset batty monsters from wander to seek here, instead
    // of in handle_behaviour() since that will be called with
    // every single movement, and we want these monsters to
    // hit and run. -- bwr
    if (monster->foe != MHITNOT && mons_is_wandering(monster)
        && mons_is_batty(monster))
    {
        monster->behaviour = BEH_SEEK;
    }

    monster->check_speed();

    monsterentry* entry = get_monster_data(monster->type);
    if (!entry)
        return;

    int old_energy      = INT_MAX;
    int non_move_energy = std::min(entry->energy_usage.move,
                                   entry->energy_usage.swim);

#if DEBUG_MONS_SCAN
    bool monster_was_floating = mgrd(monster->pos()) != monster->mindex();
#endif

    while (monster->has_action_energy())
    {
        // The continues & breaks are WRT this.
        if (!monster->alive())
            break;

#if DEBUG_MONS_SCAN
        if (!monster_was_floating
            && mgrd(monster->pos()) != monster->mindex())
        {
            mprf(MSGCH_ERROR, "Monster %s became detached from mgrd "
                              "in _handle_monster_move() loop",
                 monster->name(DESC_PLAIN, true).c_str());
            mpr("[[[[[[[[[[[[[[[[[[", MSGCH_WARN);
            debug_mons_scan();
            mpr("]]]]]]]]]]]]]]]]]]", MSGCH_WARN);
            monster_was_floating = true;
        }
        else if (monster_was_floating
                 && mgrd(monster->pos()) == monster->mindex())
        {
            mprf(MSGCH_DIAGNOSTICS, "Monster %s re-attached itself to mgrd "
                                    "in _handle_monster_move() loop",
                 monster->name(DESC_PLAIN, true).c_str());
            monster_was_floating = false;
        }
#endif

        if (monster->speed_increment >= old_energy)
        {
#if DEBUG
            if (monster->speed_increment == old_energy)
            {
                mprf(MSGCH_DIAGNOSTICS, "'%s' has same energy as last loop",
                     monster->name(DESC_PLAIN, true).c_str());
            }
            else
            {
                mprf(MSGCH_DIAGNOSTICS, "'%s' has MORE energy than last loop",
                     monster->name(DESC_PLAIN, true).c_str());
            }
#endif
            monster->speed_increment = old_energy - 10;
            old_energy               = monster->speed_increment;
            continue;
        }
        old_energy = monster->speed_increment;

        monster->shield_blocks = 0;

        cloud_type cl_type;
        const int  cloud_num   = env.cgrid(monster->pos());
        const bool avoid_cloud = mons_avoids_cloud(monster, cloud_num,
                                                   &cl_type);
        if (cl_type != CLOUD_NONE)
        {
            if (avoid_cloud)
            {
                if (mons_is_submerged(monster))
                {
                    monster->speed_increment -= entry->energy_usage.swim;
                    break;
                }

                if (monster->type == -1)
                {
                    monster->speed_increment -= entry->energy_usage.move;
                    break;  // problem with vortices
                }
            }

            _mons_in_cloud(monster);

            if (monster->type == -1)
            {
                monster->speed_increment = 1;
                break;
            }
        }

        if (monster->type == MONS_TIAMAT && one_chance_in(3))
        {
            const int cols[] = { RED, WHITE, DARKGREY, GREEN, MAGENTA };
            monster->colour = RANDOM_ELEMENT(cols);
        }

        _monster_regenerate(monster);

        if (mons_cannot_act(monster))
        {
            monster->speed_increment -= non_move_energy;
            continue;
        }

        _handle_behaviour(monster);
        ASSERT(!crawl_state.arena || monster->foe != MHITYOU);
        ASSERT(in_bounds(monster->target) || monster->target.origin());

        // Submerging monsters will hide from clouds.
        if (avoid_cloud
            && monster_can_submerge(monster, grd(monster->pos()))
            && !monster->submerged())
        {
            monster->add_ench(ENCH_SUBMERGED);
            monster->speed_increment -= ENERGY_SUBMERGE(entry);
            continue;
        }

        if (monster->speed >= 100)
        {
            monster->speed_increment -= non_move_energy;
            continue;
        }

        // Harpies may eat food/corpses on the ground.
        if (monster->type == MONS_HARPY && !mons_is_fleeing(monster)
            && (mons_wont_attack(monster)
                || (grid_distance(monster->pos(), you.pos()) > 1))
            && (mons_is_wandering(monster) && one_chance_in(3)
                || one_chance_in(5))
            && expose_items_to_element(BEAM_STEAL_FOOD, monster->pos(), 10))
        {
            if (mons_near(monster) && player_monster_visible(monster))
            {
                simple_monster_message(monster,
                                       " eats something on the ground.");
            }
            monster->speed_increment -= non_move_energy;
            continue;
        }

        if (igrd(monster->pos()) != NON_ITEM
            && (mons_itemuse(monster) == MONUSE_WEAPONS_ARMOUR
                || mons_itemuse(monster) == MONUSE_EATS_ITEMS))
        {
            // Keep neutral and charmed monsters from picking up stuff.
            // Same for friendlies if friendly_pickup is set to "none".
            if (!mons_neutral(monster) && !monster->has_ench(ENCH_CHARM)
                && (!mons_friendly(monster)
                    || you.friendly_pickup > FRIENDLY_PICKUP_NONE))
            {
                if (_handle_pickup(monster))
                {
                    DEBUG_ENERGY_USE("handle_pickup()");
                    continue;
                }
            }
        }

        if (mons_is_lurking(monster) || mons_is_submerged(monster))
        {
            // Lurking monsters only stop lurking if their target is right
            // next to them, otherwise they just sit there.
            if (monster->foe != MHITNOT
                && grid_distance(monster->target, monster->pos()) <= 1)
            {
                if (mons_is_submerged(monster))
                {
                    // Don't unsubmerge if the monster is too damaged or
                    // if the monster is afraid, or if it's avoiding the
                    // cloud on top of the water.
                    if (monster->hit_points <= monster->max_hit_points / 2
                        || monster->has_ench(ENCH_FEAR)
                        || avoid_cloud)
                    {
                        monster->speed_increment -= non_move_energy;
                        continue;
                    }

                    if (!monster->del_ench(ENCH_SUBMERGED))
                    {
                        // Couldn't unsubmerge.
                        monster->speed_increment -= non_move_energy;
                        continue;
                    }
                }
                monster->behaviour = BEH_SEEK;
            }
            else
            {
                monster->speed_increment -= non_move_energy;
                continue;
            }
        }

        if (mons_is_caught(monster))
        {
            // Struggling against the net takes time.
            _swim_or_move_energy(monster);
        }
        else if (!mons_is_petrified(monster))
        {
            // Calculates mmov based on monster target.
            _handle_movement(monster);

            brkk = false;

            if (mons_is_confused(monster)
                || monster->type == MONS_AIR_ELEMENTAL
                   && mons_is_submerged(monster))
            {
                std::vector<coord_def> moves;

                mmov.reset();
                int pfound = 0;
                for (adjacent_iterator ai(monster->pos(), false); ai; ++ai)
                    if (monster->can_pass_through(*ai))
                        if (one_chance_in(++pfound))
                            mmov = *ai - monster->pos();

                // OK, mmov determined.
                const coord_def newcell = mmov + monster->pos();
                monsters* enemy = monster_at(newcell);
                if (enemy
                    && newcell != monster->pos()
                    && !is_sanctuary(monster->pos()))
                {
                    if (monsters_fight(monster, enemy))
                    {
                        brkk = true;
                        mmov.reset();
                        DEBUG_ENERGY_USE("monsters_fight()");
                    }
                    else
                    {
                        // FIXME: None of these work!
                        // Instead run away!
                        if (monster->add_ench(mon_enchant(ENCH_FEAR)))
                        {
                            behaviour_event(monster, ME_SCARE,
                                            MHITNOT, newcell);
                        }
                        break;
                    }
                }
            }

            if (brkk)
                continue;
        }
        _handle_nearby_ability( monster );

        if (!mons_is_sleeping(monster)
            && !mons_is_wandering(monster)

            // Berserking monsters are limited to running up and
            // hitting their foes.
            && !monster->has_ench(ENCH_BERSERK))
        {
            bolt beem;
            
            beem.source      = monster->pos();
            beem.target      = monster->target;
            beem.beam_source = monster->mindex();

            const bool friendly_or_near =
                mons_friendly(monster) || mons_near(monster, monster->foe);
            // Prevents unfriendlies from nuking you from offscreen.
            // How nice!
            if (friendly_or_near || monster->type == MONS_TEST_SPAWNER)
            {
                // [ds] Special abilities shouldn't overwhelm spellcasting
                // in monsters that have both. This aims to give them both
                // roughly the same weight.

                if (coinflip() ? _handle_special_ability(monster, beem)
                                 || _handle_monster_spell(monster, beem)
                               : _handle_monster_spell(monster, beem)
                                 || _handle_special_ability(monster, beem))
                {
                    DEBUG_ENERGY_USE("spell or special");
                    continue;
                }
            }

            if (friendly_or_near)
            {
                if (_handle_potion(monster, beem))
                {
                    DEBUG_ENERGY_USE("_handle_potion()");
                    continue;
                }

                if (_handle_scroll(monster))
                {
                    DEBUG_ENERGY_USE("_handle_scroll()");
                    continue;
                }

                if (_handle_wand(monster, beem))
                {
                    DEBUG_ENERGY_USE("_handle_wand()");
                    continue;
                }

                if (_handle_reaching(monster))
                {
                    DEBUG_ENERGY_USE("_handle_reaching()");
                    continue;
                }
            }

            if (_handle_throw(monster, beem))
            {
                DEBUG_ENERGY_USE("_handle_throw()");
                continue;
            }
        }

        if (!mons_is_caught(monster))
        {
            // See if we move into (and fight) an unfriendly monster.
            monsters* targ = monster_at(monster->pos() + mmov);
            if (targ
                && targ != monster
                && !mons_aligned(monster->mindex(), targ->mindex()))
            {
                // Maybe they can swap places?
                if (_swap_monsters(monster, targ))
                {
                    _swim_or_move_energy(monster);
                    continue;
                }
                // Figure out if they fight.
                else if (monsters_fight(monster, targ))
                {
                    if (mons_is_batty(monster))
                    {
                        monster->behaviour = BEH_WANDER;
                        _set_random_target(monster);
                        // monster->speed_increment -= monster->speed;
                    }

                    mmov.reset();
                    brkk = true;
                    DEBUG_ENERGY_USE("monsters_fight()");
                }
            }

            if (brkk)
                continue;

            if (monster->pos() + mmov == you.pos())
            {
                ASSERT(!crawl_state.arena);

                if (!mons_friendly(monster))
                {
                    monster_attack(monster);

                    if (mons_is_batty(monster))
                    {
                        monster->behaviour = BEH_WANDER;
                        _set_random_target(monster);
                    }
                    DEBUG_ENERGY_USE("monster_attack()");
                    mmov.reset();
                    continue;
                }
            }

            if (invalid_monster(monster) || mons_is_stationary(monster))
            {
                if (monster->speed_increment == old_energy)
                    monster->speed_increment -= non_move_energy;
                continue;
            }

            if (mons_cannot_move(monster) || !_monster_move(monster))
                monster->speed_increment -= non_move_energy;
        }
        update_beholders(monster);

        // Reevaluate behaviour, since the monster's surroundings have
        // changed (it may have moved, or died for that matter).  Don't
        // bother for dead monsters.  :)
        if (monster->alive())
        {
            _handle_behaviour(monster);
            ASSERT(in_bounds(monster->target) || monster->target.origin());
        }

    }

    if (monster->type != -1 && monster->hit_points < 1)
        monster_die(monster, KILL_MISC, NON_MONSTER);
}

//---------------------------------------------------------------
//
// handle_monsters
//
// This is the routine that controls monster AI.
//
//---------------------------------------------------------------
void handle_monsters()
{
    // Keep track of monsters that have already moved and don't allow
    // them to move again.
    memset(immobile_monster, 0, sizeof immobile_monster);

    for (int i = 0; i < MAX_MONSTERS; i++)
    {
        monsters *monster = &menv[i];

        if (monster->type == -1 || immobile_monster[i])
            continue;

        const coord_def oldpos = monster->pos();

        _handle_monster_move(monster);

        if (!invalid_monster(monster) && monster->pos() != oldpos)
            immobile_monster[i] = true;

        // If the player got banished, discard pending monster actions.
        if (you.banished)
        {
            // Clear list of mesmerisinging monsters.
            if (you.duration[DUR_MESMERISED])
            {
                you.mesmerised_by.clear();
                you.duration[DUR_MESMERISED] = 0;
            }
            break;
        }
    }

    // Clear any summoning flags so that lower indiced
    // monsters get their actions in the next round.
    for (int i = 0; i < MAX_MONSTERS; i++)
        menv[i].flags &= ~MF_JUST_SUMMONED;
}

static bool _is_item_jelly_edible(const item_def &item)
{
    // Don't eat artefacts.
    if (is_artefact(item))
        return (false);

    // Shouldn't eat stone things
    //   - but what about wands and rings?
    if (item.base_type == OBJ_MISSILES
        && (item.sub_type == MI_STONE || item.sub_type == MI_LARGE_ROCK))
    {
        return (false);
    }

    // Don't eat special game items.
    if (item.base_type == OBJ_ORBS
        || (item.base_type == OBJ_MISCELLANY
            && (item.sub_type == MISC_RUNE_OF_ZOT
                || item.sub_type == MISC_HORN_OF_GERYON)))
    {
        return (false);
    }

    return (true);
}

//---------------------------------------------------------------
//
// handle_pickup
//
// Returns false if monster doesn't spend any time picking something up.
//
//---------------------------------------------------------------
static bool _handle_pickup(monsters *monster)
{
    if (mons_is_sleeping(monster) || mons_is_submerged(monster))
        return (false);

    const bool monster_nearby = mons_near(monster);

    if (mons_itemuse(monster) == MONUSE_EATS_ITEMS)
    {
        // Friendly jellies won't eat.
        if (monster->attitude != ATT_HOSTILE)
            return (false);

        int  hps_gained = 0;
        int  max_eat    = roll_dice( 1, 10 );
        int  eaten      = 0;
        bool eaten_net  = false;

        for (stack_iterator si(monster->pos());
             si && eaten < max_eat && hps_gained < 50;
             ++si)
        {
            if (!_is_item_jelly_edible(*si))
                continue;

#if DEBUG_DIAGNOSTICS || DEBUG_EATERS
            mprf(MSGCH_DIAGNOSTICS,
                 "%s eating %s", monster->name(DESC_PLAIN, true).c_str(),
                 si->name(DESC_PLAIN).c_str());
#endif

            int quant = si->quantity;

            if (si->base_type != OBJ_GOLD)
            {
                quant = std::min(quant, max_eat - eaten);

                hps_gained += (quant * item_mass(*si)) / 20 + quant;
                eaten += quant;

                if (mons_is_caught(monster)
                    && si->base_type == OBJ_MISSILES
                    && si->sub_type == MI_THROWING_NET
                    && item_is_stationary(*si))
                {
                    monster->del_ench(ENCH_HELD, true);
                    eaten_net = true;
                }
            }
            else
            {
                // shouldn't be much trouble to digest a huge pile of gold!
                if (quant > 500)
                    quant = 500 + roll_dice( 2, (quant - 500) / 2 );

                hps_gained += quant / 10 + 1;
                eaten++;
            }

            if (quant >= si->quantity)
                item_was_destroyed(*si, monster->mindex());

            dec_mitm_item_quantity(si.link(), quant);
        }

        if (eaten)
        {
            hps_gained = std::max(hps_gained, 1);
            hps_gained = std::min(hps_gained, 50);

            // This is done manually instead of using heal_monster(),
            // because that function doesn't work quite this way.  -- bwr
            monster->hit_points += hps_gained;

            if (monster->max_hit_points < monster->hit_points)
                monster->max_hit_points = monster->hit_points;

            if (player_can_hear(monster->pos()))
            {
                mprf(MSGCH_SOUND, "You hear a%s slurping noise.",
                     monster_nearby ? "" : " distant");
            }

            if (eaten_net)
                simple_monster_message(monster, " devours the net!");

            _jelly_divide(monster);
        }

        return (false);
    }

    // Note: Monsters only look at stuff near the top of stacks.
    // XXX: Need to put in something so that monster picks up multiple items
    // (eg ammunition) identical to those it's carrying.
    // Monsters may now pick up several items in the same turn, though with
    // reducing chances. (jpeg)
    bool success = false;
    for (stack_iterator si(monster->pos()); si; ++si)
    {
        if (monster->pickup_item(*si, monster_nearby))
            success = true;
        if (coinflip())
            break;
    }
    return (success);
}

static void _jelly_grows(monsters *monster)
{
    if (player_can_hear(monster->pos()))
    {
        mprf(MSGCH_SOUND, "You hear a%s slurping noise.",
             mons_near(monster) ? "" : " distant");
    }

    monster->hit_points += 5;

    // note here, that this makes jellies "grow" {dlb}:
    if (monster->hit_points > monster->max_hit_points)
        monster->max_hit_points = monster->hit_points;

    _jelly_divide(monster);
}

static bool _mons_can_displace(const monsters *mpusher, const monsters *mpushee)
{
    if (invalid_monster(mpusher) || invalid_monster(mpushee))
        return (false);

    const int ipushee = monster_index(mpushee);
    if (invalid_monster_index(ipushee))
        return (false);

    if (immobile_monster[ipushee])
        return (false);

    // Confused monsters can't be pushed past, sleeping monsters
    // can't push. Note that sleeping monsters can't be pushed
    // past, either, but they may be woken up by a crowd trying to
    // elbow past them, and the wake-up check happens downstream.
    if (mons_is_confused(mpusher)      || mons_is_confused(mpushee)
        || mons_cannot_move(mpusher)   || mons_cannot_move(mpushee)
        || mons_is_stationary(mpusher) || mons_is_stationary(mpushee)
        || mons_is_sleeping(mpusher))
    {
        return (false);
    }

    // Batty monsters are unpushable.
    if (mons_is_batty(mpusher) || mons_is_batty(mpushee))
        return (false);

    if (!monster_shover(mpusher))
        return (false);

    if (!monster_senior(mpusher, mpushee))
        return (false);

    return (true);
}

static bool _monster_swaps_places( monsters *mon, const coord_def& delta )
{
    if (delta.origin())
        return (false);

    monsters* const m2 = monster_at(mon->pos() + delta);

    if (!m2)
        return (false);

    if (!_mons_can_displace(mon, m2))
        return (false);

    if (mons_is_sleeping(m2))
    {
        if (coinflip())
        {
#ifdef DEBUG_DIAGNOSTICS
            mprf(MSGCH_DIAGNOSTICS,
                 "Alerting monster %s at (%d,%d)",
                 m2->name(DESC_PLAIN).c_str(), m2->pos().x, m2->pos().y);
#endif
            behaviour_event(m2, ME_ALERT, MHITNOT);
        }
        return (false);
    }

    // Check that both monsters will be happy at their proposed new locations.
    const coord_def c = mon->pos();
    const coord_def n = mon->pos() + delta;

    if (!_habitat_okay(mon, grd(n)) || !_habitat_okay(m2, grd(c)))
        return (false);

    // Okay, do the swap!
    _swim_or_move_energy(mon);

    mon->pos() = n;
    mgrd(n) = monster_index(mon);
    m2->pos() = c;
    const int m2i = monster_index(m2);
    ASSERT(m2i >= 0 && m2i < MAX_MONSTERS);
    mgrd(c) = m2i;
    immobile_monster[m2i] = true;

    mon->check_redraw(c);
    mon->apply_location_effects(c);
    m2->check_redraw(c);
    m2->apply_location_effects(n);

    // The seen context no longer applies if the monster is moving normally.
    mon->seen_context.clear();
    m2->seen_context.clear();

    return (false);
}

static bool _do_move_monster(monsters *monster, const coord_def& delta)
{
    const coord_def f = monster->pos() + delta;

    if (!in_bounds(f))
        return (false);

    if (f == you.pos())
    {
        monster_attack(monster);
        return (true);
    }

    // This includes the case where the monster attacks itself.
    if (monsters* def = monster_at(f))
    {
        monsters_fight(monster, def);
        return (true);
    }

    // The monster gave a "comes into view" message and then immediately
    // moved back out of view, leaing the player nothing to see, so give
    // this message to avoid confusion.
    if (monster->seen_context == _just_seen && !see_grid(f))
        simple_monster_message(monster, " moves out of view.");

    // The seen context no longer applies if the monster is moving normally.
    monster->seen_context.clear();

    // This appears to be the real one, ie where the movement occurs:
    _swim_or_move_energy(monster);

    if (grd(monster->pos()) == DNGN_DEEP_WATER && grd(f) != DNGN_DEEP_WATER
        && !monster_habitable_grid(monster, DNGN_DEEP_WATER))
    {
        monster->seen_context = "emerges from the water";
    }
    mgrd(monster->pos()) = NON_MONSTER;

    monster->pos() = f;

    mgrd(monster->pos()) = monster_index(monster);

    monster->check_redraw(monster->pos() - delta);
    monster->apply_location_effects(monster->pos() - delta);

    return (true);
}

void mons_check_pool(monsters *monster, const coord_def &oldpos,
                     killer_type killer, int killnum)
{
    // Levitating/flying monsters don't make contact with the terrain.
    if (monster->airborne())
        return;

    dungeon_feature_type grid = grd(monster->pos());
    if ((grid == DNGN_LAVA || grid == DNGN_DEEP_WATER)
        && !monster_habitable_grid(monster, grid))
    {
        const bool message = mons_near(monster);

        // Don't worry about invisibility.  You should be able to see if
        // something has fallen into the lava.
        if (message && (oldpos == monster->pos() || grd(oldpos) != grid))
        {
            mprf("%s falls into the %s!",
                 monster->name(DESC_CAP_THE).c_str(),
                 grid == DNGN_LAVA ? "lava" : "water");
        }

        if (grid == DNGN_LAVA && mons_res_fire(monster) >= 2)
            grid = DNGN_DEEP_WATER;

        // Even fire resistant monsters perish in lava, but inanimate
        // monsters can survive deep water.
        if (grid == DNGN_LAVA || monster->can_drown())
        {
            if (message)
            {
                if (grid == DNGN_LAVA)
                {
                    simple_monster_message(monster, " is incinerated.",
                                           MSGCH_MONSTER_DAMAGE, MDAM_DEAD);
                }
                else if (mons_genus(monster->type) == MONS_MUMMY)
                {
                    simple_monster_message(monster, " falls apart.",
                                           MSGCH_MONSTER_DAMAGE, MDAM_DEAD);
                }
                else
                {
                    simple_monster_message(monster, " drowns.",
                                           MSGCH_MONSTER_DAMAGE, MDAM_DEAD);
                }
            }

            if (killer == KILL_NONE)
            {
                // Self-kill.
                killer  = KILL_MON;
                killnum = monster_index(monster);
            }

            // Yredelemnul special, redux: It's the only one that can
            // work on drowned monsters.
            if (!_yred_enslave_soul(monster, killer))
                monster_die(monster, killer, killnum, true);
        }
    }
}

// Randomise potential damage.
static int _estimated_trap_damage(trap_type trap)
{
    switch (trap)
    {
        case TRAP_BLADE:
           return (10 + random2(30));
        case TRAP_DART:
           return (random2(4));
        case TRAP_ARROW:
           return (random2(7));
        case TRAP_SPEAR:
           return (random2(10));
        case TRAP_BOLT:
           return (random2(13));
        case TRAP_AXE:
           return (random2(15));
        default:
           return (0);
    }
}

// Check whether a given trap (described by trap position) can be
// regarded as safe.  Takes into account monster intelligence and
// allegiance.
// (just_check is used for intelligent monsters trying to avoid traps.)
static bool _is_trap_safe(const monsters *monster, const coord_def& where,
                          bool just_check)
{
    const int intel = mons_intel(monster);

    const trap_def *ptrap = find_trap(where);
    if (!ptrap)
        return (true);
    const trap_def& trap = *ptrap;

    const bool player_knows_trap = (trap.is_known(&you));

    // No friendly monsters will ever enter a Zot trap you know.
    if (player_knows_trap && mons_friendly(monster) && trap.type == TRAP_ZOT)
        return (false);

    // Dumb monsters don't care at all.
    if (intel == I_PLANT)
        return (true);

    if (trap.type == TRAP_SHAFT && monster->will_trigger_shaft())
    {
        if ((mons_is_fleeing(monster) || mons_is_pacified(monster))
            && intel >= I_NORMAL)
        {
            return (true);
        }
        return (false);
    }

    // Hostile monsters are not afraid of non-mechanical traps.
    // Allies will try to avoid teleportation and zot traps.
    const bool mechanical = (trap.category() == DNGN_TRAP_MECHANICAL);

    if (trap.is_known(monster))
    {
        if (just_check)
            return (false); // Square is blocked.
        else
        {
            // Test for corridor-like environment.
            const int x = where.x - monster->pos().x;
            const int y = where.y - monster->pos().y;

            // The question is whether the monster (m) can easily reach its
            // presumable destination (x) without stepping on the trap. Traps
            // in corridors do not allow this. See e.g
            //  #x#        ##
            //  #^#   or  m^x
            //   m         ##
            //
            // The same problem occurs if paths are blocked by monsters,
            // hostile terrain or other traps rather than walls.
            // What we do is check whether the squares with the relative
            // positions (-1,0)/(+1,0) or (0,-1)/(0,+1) form a "corridor"
            // (relative to the _trap_ position rather than the monster one).
            // If they don't, the trap square is marked as "unsafe" (because
            // there's a good alternative move for the monster to take),
            // otherwise the decision will be made according to later tests
            // (monster hp, trap type, ...)
            // If a monster still gets stuck in a corridor it will usually be
            // because it has less than half its maximum hp.

            if ((_mon_can_move_to_pos(monster, coord_def(x-1, y), true)
                 || _mon_can_move_to_pos(monster, coord_def(x+1,y), true))
                && (_mon_can_move_to_pos(monster, coord_def(x,y-1), true)
                    || _mon_can_move_to_pos(monster, coord_def(x,y+1), true)))
            {
                return (false);
            }
        }
    }

    // Friendlies will try not to be parted from you.
    if (intelligent_ally(monster) && trap.type == TRAP_TELEPORT
        && player_knows_trap && mons_near(monster))
    {
        return (false);
    }

    // Healthy monsters don't mind a little pain.
    if (mechanical && monster->hit_points >= monster->max_hit_points / 2
        && (intel == I_ANIMAL
            || monster->hit_points > _estimated_trap_damage(trap.type)))
    {
        return (true);
    }

    // Friendly and good neutral monsters don't enjoy Zot trap perks;
    // handle accordingly.  In the arena Zot traps affect all monsters.
    if (mons_wont_attack(monster) || crawl_state.arena)
    {
        return (mechanical ? mons_flies(monster)
                           : !trap.is_known(monster) || trap.type != TRAP_ZOT);
    }
    else
        return (!mechanical || mons_flies(monster));
}

static void _mons_open_door(monsters* monster, const coord_def &pos)
{
    dungeon_feature_type grid = grd(pos);
    const char *adj = "", *noun = "door";

    bool was_secret = false;
    bool was_seen   = false;

    std::set<coord_def> all_door;
    find_connected_range(pos, DNGN_CLOSED_DOOR, DNGN_SECRET_DOOR,
                         all_door);
    get_door_description(all_door.size(), &adj, &noun);

    for (std::set<coord_def>::iterator i = all_door.begin();
         i != all_door.end(); ++i)
    {
        const coord_def& dc = *i;
        if (grd(dc) == DNGN_SECRET_DOOR && see_grid(dc))
        {
            grid = grid_secret_door_appearance(dc);
            was_secret = true;
        }

        if (see_grid(dc))
            was_seen = true;
        else
            set_terrain_changed(dc);

        grd[dc.x][dc.y] = DNGN_OPEN_DOOR;
    }

    if (was_seen)
    {
        viewwindow(true, false);

        if (was_secret)
        {
            mprf("%s was actually a secret door!",
                 feature_description(grid, NUM_TRAPS, false,
                                     DESC_CAP_THE, false).c_str());
            learned_something_new(TUT_SEEN_SECRET_DOOR, pos);
        }

        std::string open_str = "opens the ";
        open_str += adj;
        open_str += noun;
        open_str += ".";

        monster->seen_context = open_str;

        if (!you.can_see(monster))
        {
            mprf("Something unseen %s", open_str.c_str());
            interrupt_activity(AI_FORCE_INTERRUPT);
        }
        else if (!you_are_delayed())
        {
            mprf("%s %s", monster->name(DESC_CAP_A).c_str(),
                 open_str.c_str());
        }
    }

    monster->lose_energy(EUT_MOVE);
}

static bool _no_habitable_adjacent_grids(const monsters *mon)
{
    for (adjacent_iterator ai(mon->pos()); ai; ++ai)
        if (_habitat_okay(mon, grd(*ai)))
            return (false);

    return (true);
}

// Check whether a monster can move to given square (described by its relative
// coordinates to the current monster position). just_check is true only for
// calls from is_trap_safe when checking the surrounding squares of a trap.
static bool _mon_can_move_to_pos(const monsters *monster,
                                 const coord_def& delta, bool just_check)
{
    const coord_def targ = monster->pos() + delta;

    // Bounds check: don't consider moving out of grid!
    if (!in_bounds(targ))
        return (false);

    // Non-friendly and non-good neutral monsters won't enter
    // sanctuaries.
    if (!mons_wont_attack(monster)
        && is_sanctuary(targ)
        && !is_sanctuary(monster->pos()))
    {
        return (false);
    }

    // Inside a sanctuary don't attack anything!
    if (is_sanctuary(monster->pos())
        && (targ == you.pos() || mgrd(targ) != NON_MONSTER))
    {
        return (false);
    }

    const dungeon_feature_type target_grid = grd(targ);
    const habitat_type habitat = mons_primary_habitat(monster);

    // Effectively slows down monster movement across water.
    // Fire elementals can't cross at all.
    bool no_water = false;
    if (monster->type == MONS_FIRE_ELEMENTAL || one_chance_in(5))
        no_water = true;

    cloud_type targ_cloud_type;
    const int  targ_cloud_num = env.cgrid(targ);

    if (mons_avoids_cloud(monster, targ_cloud_num, &targ_cloud_type))
        return (false);

    if (mons_class_flag(monster->type, M_BURROWS)
        && (target_grid == DNGN_ROCK_WALL
            || target_grid == DNGN_CLEAR_ROCK_WALL))
    {
        // Don't burrow out of bounds.
        if (!in_bounds(targ))
            return (false);

        // Don't burrow at an angle (legacy behaviour).
        if (delta.x != 0 && delta.y != 0)
            return (false);
    }
    else if (!monster->can_pass_through_feat(target_grid)
             || no_water && grid_is_water(target_grid))
    {
        return (false);
    }
    else if (!_habitat_okay(monster, target_grid))
    {
        // If the monster somehow ended up in this habitat (and is
        // not dead by now), give it a chance to get out again.
        if (grd(monster->pos()) == target_grid
            && _no_habitable_adjacent_grids(monster))
        {
            return (true);
        }

        return (false);
    }

    // Wandering mushrooms don't move while you are looking.
    if (monster->type == MONS_WANDERING_MUSHROOM && see_grid(targ))
        return (false);

    // Water elementals avoid fire and heat.
    if (monster->type == MONS_WATER_ELEMENTAL
        && (target_grid == DNGN_LAVA
            || targ_cloud_type == CLOUD_FIRE
            || targ_cloud_type == CLOUD_STEAM))
    {
        return (false);
    }

    // Fire elementals avoid water and cold.
    if (monster->type == MONS_FIRE_ELEMENTAL
        && (grid_is_watery(target_grid)
            || targ_cloud_type == CLOUD_COLD))
    {
        return (false);
    }

    // Submerged water creatures avoid the shallows where
    // they would be forced to surface. -- bwr
    // [dshaligram] Monsters now prefer to head for deep water only if
    // they're low on hitpoints. No point in hiding if they want a
    // fight.
    if (habitat == HT_WATER
        && targ != you.pos()
        && target_grid != DNGN_DEEP_WATER
        && grd(monster->pos()) == DNGN_DEEP_WATER
        && monster->hit_points < (monster->max_hit_points * 3) / 4)
    {
        return (false);
    }

    // Smacking the player is always a good move if we're
    // hostile (even if we're heading somewhere else).
    // Also friendlies want to keep close to the player
    // so it's okay as well.

    // Smacking another monster is good, if the monsters
    // are aligned differently.
    if (mgrd(targ) != NON_MONSTER)
    {
        if (just_check)
        {
            if (targ == monster->pos())
                return (true);

            return (false); // blocks square
        }

        const int thismonster = monster_index(monster),
                  targmonster = mgrd(targ);

        if (mons_aligned(thismonster, targmonster)
            && targmonster != MHITNOT
            && targmonster != MHITYOU
            && !_mons_can_displace(monster, &menv[targmonster]))
        {
            return (false);
        }
    }

    // Friendlies shouldn't try to move onto the player's
    // location, if they are aiming for some other target.
    if (mons_wont_attack(monster)
        && monster->foe != MHITYOU
        && (monster->foe != MHITNOT || monster->is_patrolling())
        && targ == you.pos())
    {
        return (false);
    }

    // Wandering through a trap is OK if we're pretty healthy,
    // really stupid, or immune to the trap.
    if (!_is_trap_safe(monster, targ, just_check))
        return (false);

    // If we end up here the monster can safely move.
    return (true);
}

static bool _monster_move(monsters *monster)
{
    FixedArray < bool, 3, 3 > good_move;
    int count_x, count_y, count;

    const habitat_type habitat = mons_primary_habitat(monster);
    bool deep_water_available = false;

    if (monster->type == MONS_TRAPDOOR_SPIDER)
    {
        if (mons_is_submerged(monster))
           return (false);

        // Trapdoor spiders hide if they can't see their target.
        bool can_see;

        if (monster->foe == MHITNOT)
            can_see = false;
        else if (monster->foe == MHITYOU)
            can_see = monster->can_see(&you);
        else
            can_see = monster->can_see(&menv[monster->foe]);

        if (monster_can_submerge(monster, grd(monster->pos()))
            && !can_see && !mons_is_confused(monster)
            && !monster->has_ench(ENCH_BERSERK))
        {
            monster->add_ench(ENCH_SUBMERGED);
            monster->behaviour = BEH_LURK;
            return (false);
        }
    }

    // Berserking monsters make a lot of racket.
    if (monster->has_ench(ENCH_BERSERK))
    {
        int noise_level = get_shout_noise_level(mons_shouts(monster->type));
        if (noise_level > 0)
        {
            if (mons_near(monster) && player_monster_visible(monster))
            {
                if (one_chance_in(10))
                {
                    mprf(MSGCH_TALK_VISUAL, "%s rages.",
                         monster->name(DESC_CAP_THE).c_str());
                }
                noisy(noise_level, monster->pos());
            }
            else if (one_chance_in(5))
                handle_monster_shouts(monster, true);
            else
            {
                // Just be noisy without messaging the player.
                noisy(noise_level, monster->pos());
            }
        }
    }

    if (monster->confused())
    {
        if (mmov.x || mmov.y || one_chance_in(15))
        {
            const coord_def newpos = monster->pos() + mmov;
            if (in_bounds(newpos)
                && (habitat == HT_LAND
                    || monster_habitable_grid(monster, grd(newpos))))
            {
                return _do_move_monster(monster, mmov);
            }
        }
        return (false);
    }

    // Let's not even bother with this if mmov.x and mmov.y are zero.
    if (mmov.origin())
        return (false);

    for (count_x = 0; count_x < 3; count_x++)
        for (count_y = 0; count_y < 3; count_y++)
        {
            const int targ_x = monster->pos().x + count_x - 1;
            const int targ_y = monster->pos().y + count_y - 1;

            // Bounds check: don't consider moving out of grid!
            if (!in_bounds(targ_x, targ_y))
            {
                good_move[count_x][count_y] = false;
                continue;
            }
            dungeon_feature_type target_grid = grd[targ_x][targ_y];

            if (target_grid == DNGN_DEEP_WATER)
                deep_water_available = true;

            good_move[count_x][count_y] =
                _mon_can_move_to_pos(monster, coord_def(count_x-1, count_y-1));
        }

    // Now we know where we _can_ move.

    const coord_def newpos = monster->pos() + mmov;
    // Normal/smart monsters know about secret doors
    // (they _live_ in the dungeon!)
    if (grd(newpos) == DNGN_CLOSED_DOOR
        || grd(newpos) == DNGN_SECRET_DOOR && mons_intel(monster) >= I_NORMAL)
    {
        if (mons_is_zombified(monster))
        {
            // For zombies, monster type is kept in mon->base_monster.
            if (mons_class_itemuse(monster->base_monster) >= MONUSE_OPEN_DOORS)
            {
                _mons_open_door(monster, newpos);
                return (true);
            }
        }
        else if (mons_itemuse(monster) >= MONUSE_OPEN_DOORS)
        {
            _mons_open_door(monster, newpos);
            return (true);
        }
    } // endif - secret/closed doors

    // Jellies eat doors.  Yum!
    if ((grd(newpos) == DNGN_CLOSED_DOOR || grd(newpos) == DNGN_OPEN_DOOR)
        && mons_itemuse(monster) == MONUSE_EATS_ITEMS
        // Doors with permarock marker cannot be eaten.
        && !feature_marker_at(newpos, DNGN_PERMAROCK_WALL))
    {
        grd(newpos) = DNGN_FLOOR;

        _jelly_grows(monster);

        if (see_grid(newpos))
        {
            viewwindow(true, false);

            if (!you.can_see(monster))
            {
                mpr("The door mysteriously vanishes.");
                interrupt_activity( AI_FORCE_INTERRUPT );
            }
        }
    } // done door-eating jellies

    // Water creatures have a preference for water they can hide in -- bwr
    // [ds] Weakened the powerful attraction to deep water if the monster
    // is in good health.
    if (habitat == HT_WATER
        && deep_water_available
        && grd(monster->pos()) != DNGN_DEEP_WATER
        && grd(newpos) != DNGN_DEEP_WATER
        && newpos != you.pos()
        && (one_chance_in(3)
            || monster->hit_points <= (monster->max_hit_points * 3) / 4))
    {
        count = 0;

        for (count_x = 0; count_x < 3; count_x++)
            for (count_y = 0; count_y < 3; count_y++)
            {
                if (good_move[count_x][count_y]
                    && grd[monster->pos().x + count_x - 1][monster->pos().y + count_y - 1]
                            == DNGN_DEEP_WATER)
                {
                    count++;

                    if (one_chance_in( count ))
                    {
                        mmov.x = count_x - 1;
                        mmov.y = count_y - 1;
                    }
                }
            }
    }

    // Now, if a monster can't move in its intended direction, try
    // either side.  If they're both good, move in whichever dir
    // gets it closer (farther for fleeing monsters) to its target.
    // If neither does, do nothing.
    if (good_move[mmov.x + 1][mmov.y + 1] == false)
    {
        int current_distance = distance(monster->pos(), monster->target);

        int dir = -1;

        for (int i = 0; i < 8; i++)
        {
            if (mon_compass[i] == mmov)
            {
                dir = i;
                break;
            }
        }

        if (dir < 0)
            goto forget_it;

        int dist[2];

        // First 1 away, then 2 (3 is silly).
        for (int j = 1; j <= 2; j++)
        {
            int sdir, inc;

            if (coinflip())
            {
                sdir = -j;
                inc = 2*j;
            }
            else
            {
                sdir = j;
                inc = -2*j;
            }

            // Try both directions.
            for (int mod = sdir, i = 0; i < 2; mod += inc, i++)
            {
                int newdir = (dir + 8 + mod) % 8;
                if (good_move[mon_compass[newdir].x+1][mon_compass[newdir].y+1])
                {
                    dist[i] = distance(monster->pos()+mon_compass[newdir],
                                       monster->target);
                }
                else
                {
                    dist[i] = (mons_is_fleeing(monster)) ? (-FAR_AWAY)
                                                         : FAR_AWAY;
                }
            }

            // Now choose.
            if (dist[0] == dist[1] && abs(dist[0]) == FAR_AWAY)
                continue;

            // Which one was better? -- depends on FLEEING or not.
            if (mons_is_fleeing(monster))
            {
                if (dist[0] >= dist[1] && dist[0] >= current_distance)
                {
                    mmov = mon_compass[((dir+8)+sdir)%8];
                    break;
                }
                if (dist[1] >= dist[0] && dist[1] >= current_distance)
                {
                    mmov = mon_compass[((dir+8)-sdir)%8];
                    break;
                }
            }
            else
            {
                if (dist[0] <= dist[1] && dist[0] <= current_distance)
                {
                    mmov = mon_compass[((dir+8)+sdir)%8];
                    break;
                }
                if (dist[1] <= dist[0] && dist[1] <= current_distance)
                {
                    mmov = mon_compass[((dir+8)-sdir)%8];
                    break;
                }
            }
        }
    } // end - try to find good alternate move

forget_it:

    // ------------------------------------------------------------------
    // If we haven't found a good move by this point, we're not going to.
    // ------------------------------------------------------------------

    // Take care of beetle burrowing.
    if (mons_class_flag(monster->type, M_BURROWS))
    {
        const dungeon_feature_type feat = grd(monster->pos() + mmov);
        if ((feat == DNGN_ROCK_WALL || feat == DNGN_CLEAR_ROCK_WALL)
            && good_move[mmov.x + 1][mmov.y + 1] == true)
        {
            grd(monster->pos() + mmov) = DNGN_FLOOR;
            set_terrain_changed(monster->pos() + mmov);

            if (!silenced(you.pos()))
            {
                // Message depends on whether caused by boring beetle or
                // acid (Dissolution).
                mpr((monster->type == MONS_BORING_BEETLE) ?
                    "You hear a grinding noise." :
                    "You hear a sizzling sound.", MSGCH_SOUND);
            }
        }
    }

    bool ret = false;
    if (good_move[mmov.x + 1][mmov.y + 1] && !(mmov.x == 0 && mmov.y == 0))
    {
        // Check for attacking player.
        if (monster->pos() + mmov == you.pos())
        {
            ret = monster_attack(monster);
            mmov.reset();
        }

        // If we're following the player through stairs, the only valid
        // movement is towards the player. -- bwr
        if (testbits( monster->flags, MF_TAKING_STAIRS ))
        {
            const delay_type delay = current_delay_action();
            if (delay != DELAY_ASCENDING_STAIRS
                && delay != DELAY_DESCENDING_STAIRS)
            {
                monster->flags &= ~MF_TAKING_STAIRS;

#ifdef DEBUG_DIAGNOSTICS
                mprf(MSGCH_DIAGNOSTICS,
                     "BUG: %s was marked as follower when not following!",
                     monster->name(DESC_PLAIN).c_str(), true);
#endif
            }
            else
            {
                ret    = true;
                mmov.reset();

#if DEBUG_DIAGNOSTICS
                mprf(MSGCH_DIAGNOSTICS,
                     "%s is skipping movement in order to follow.",
                     monster->name(DESC_CAP_THE).c_str(), true );
#endif
            }
        }

        // Check for attacking another monster.
        if (monsters* targ = monster_at(monster->pos() + mmov))
        {
            if (mons_aligned(monster->mindex(), targ->mindex()))
                ret = _monster_swaps_places(monster, mmov);
            else
            {
                monsters_fight(monster, targ);
                ret = true;
            }

            // If the monster swapped places, the work's already done.
            mmov.reset();
        }

        if (monster->type == MONS_EFREET
            || monster->type == MONS_FIRE_ELEMENTAL)
        {
            place_cloud( CLOUD_FIRE, monster->pos(),
                         2 + random2(4), monster->kill_alignment() );
        }

        if (monster->type == MONS_ROTTING_DEVIL
            || monster->type == MONS_CURSE_TOE)
        {
            place_cloud( CLOUD_MIASMA, monster->pos(),
                         2 + random2(3), monster->kill_alignment() );
        }
    }
    else
    {
        mmov.reset();

        // Fleeing monsters that can't move will panic and possibly
        // turn to face their attacker.
        _make_mons_stop_fleeing(monster);
    }

    if (mmov.x || mmov.y || (monster->confused() && one_chance_in(6)))
        return _do_move_monster(monster, mmov);

    return ret;
}                               // end monster_move()

static void _mons_in_cloud(monsters *monster)
{
    int wc = env.cgrid(monster->pos());
    int hurted = 0;
    bolt beam;

    const int speed = ((monster->speed > 0) ? monster->speed : 10);
    bool wake = false;

    if (mons_is_mimic( monster->type ))
    {
        mimic_alert(monster);
        return;
    }

    const cloud_struct &cloud(env.cloud[wc]);
    switch (cloud.type)
    {
    case CLOUD_DEBUGGING:
        end(1, false, "Fatal error: monster steps on nonexistent cloud!");
        return;

    case CLOUD_FIRE:
        if (monster->type == MONS_FIRE_VORTEX
            || monster->type == MONS_EFREET
            || monster->type == MONS_FIRE_ELEMENTAL)
        {
            return;
        }

        simple_monster_message(monster, " is engulfed in flames!");

        hurted +=
            resist_adjust_damage( monster,
                                  BEAM_FIRE,
                                  monster->res_fire(),
                                  ((random2avg(16, 3) + 6) * 10) / speed );

        hurted -= random2(1 + monster->ac);
        break;

    case CLOUD_STINK:
        simple_monster_message(monster, " is engulfed in noxious gasses!");

        if (mons_res_poison(monster) > 0)
            return;

        beam.flavour = BEAM_CONFUSION;
        beam.thrower = cloud.killer;

        if (cloud.whose == KC_FRIENDLY)
            beam.beam_source = ANON_FRIENDLY_MONSTER;

        if (mons_class_is_confusable(monster->type)
            && 1 + random2(27) >= monster->hit_dice)
        {
            beam.apply_enchantment_to_monster(monster);
        }

        hurted += (random2(3) * 10) / speed;
        break;

    case CLOUD_COLD:
        simple_monster_message(monster, " is engulfed in freezing vapours!");

        hurted +=
            resist_adjust_damage( monster,
                                  BEAM_COLD,
                                  monster->res_cold(),
                                  ((6 + random2avg(16, 3)) * 10) / speed );

        hurted -= random2(1 + monster->ac);
        break;

    case CLOUD_POISON:
        simple_monster_message(monster, " is engulfed in a cloud of poison!");

        if (mons_res_poison(monster) > 0)
            return;

        poison_monster(monster, cloud.whose);
        // If the monster got poisoned, wake it up.
        wake = true;

        hurted += (random2(8) * 10) / speed;

        if (mons_res_poison(monster) < 0)
            hurted += (random2(4) * 10) / speed;
        break;

    case CLOUD_STEAM:
    {
        // couldn't be bothered coding for armour of res fire

        simple_monster_message(monster, " is engulfed in steam!");

        const int steam_base_damage = steam_cloud_damage(cloud);
        hurted +=
            resist_adjust_damage(
                monster,
                BEAM_STEAM,
                monster->res_steam(),
                (random2avg(steam_base_damage, 2) * 10) / speed);

        hurted -= random2(1 + monster->ac);
        break;
    }

    case CLOUD_MIASMA:
        simple_monster_message(monster, " is engulfed in a dark miasma!");

        if (mons_res_miasma(monster) > 0)
            return;

        poison_monster(monster, cloud.whose);

        if (monster->max_hit_points > 4 && coinflip())
            monster->max_hit_points--;

        beam.flavour = BEAM_SLOW;

        if (one_chance_in(3))
            beam.apply_enchantment_to_monster(monster);

        hurted += (10 * random2avg(12, 3)) / speed;    // 3
        break;

    default:                // 'harmless' clouds -- colored smoke, etc {dlb}.
        return;
    }

    // A sleeping monster that sustains damage will wake up.
    if ((wake || hurted > 0) && mons_is_sleeping(monster))
    {
        // We have no good coords to give the monster as the source of the
        // disturbance other than the cloud itself.
        behaviour_event(monster, ME_DISTURB, MHITNOT, monster->pos());
    }

    if (hurted > 0)
    {
#ifdef DEBUG_DIAGNOSTICS
        mprf(MSGCH_DIAGNOSTICS, "%s takes %d damage from cloud.",
             monster->name(DESC_CAP_THE).c_str(), hurted);
#endif
        monster->hurt(NULL, hurted, BEAM_MISSILE, false);

        if (monster->hit_points < 1)
        {
            mon_enchant death_ench(ENCH_NONE, 0, cloud.whose);
            monster_die(monster, cloud.killer, death_ench.kill_agent());
        }
    }
}

bool monster_descriptor(int which_class, unsigned char which_descriptor)
{
    if (which_descriptor == MDSC_LEAVES_HIDE)
    {
        switch (which_class)
        {
        case MONS_DRAGON:
        case MONS_TROLL:
        case MONS_ICE_DRAGON:
        case MONS_STEAM_DRAGON:
        case MONS_MOTTLED_DRAGON:
        case MONS_STORM_DRAGON:
        case MONS_GOLDEN_DRAGON:
        case MONS_SWAMP_DRAGON:
        case MONS_YAK:
        case MONS_SHEEP:
            return (true);
        default:
            return (false);
        }
    }

    if (which_descriptor == MDSC_REGENERATES)
    {
        switch (which_class)
        {
        case MONS_CACODEMON:
        case MONS_DEEP_TROLL:
        case MONS_HELLWING:
        case MONS_IMP:
        case MONS_IRON_TROLL:
        case MONS_LEMURE:
        case MONS_ROCK_TROLL:
        case MONS_SLIME_CREATURE:
        case MONS_SNORG:
        case MONS_TROLL:
        case MONS_HYDRA:
        case MONS_KILLER_KLOWN:
        case MONS_LERNAEAN_HYDRA:
        case MONS_DISSOLUTION:
            return (true);
        default:
            return (false);
        }
    }

    if (which_descriptor == MDSC_NOMSG_WOUNDS)
    {
        // Zombified monsters other than spectral things don't show
        // wounds.
        if (mons_class_is_zombified(which_class)
            && which_class != MONS_SPECTRAL_THING)
        {
            return (true);
        }

        switch (which_class)
        {
        case MONS_RAKSHASA:
        case MONS_RAKSHASA_FAKE:
            return (true);
        default:
            return (false);
        }
    }

    return (false);
}

bool message_current_target()
{
    if (crawl_state.is_replaying_keys())
    {
        if (you.prev_targ == MHITNOT || you.prev_targ == MHITYOU)
            return (false);

        const monsters *montarget = &menv[you.prev_targ];
        return (you.prev_targ != MHITNOT && you.prev_targ != MHITYOU
                && mons_near(montarget) && player_monster_visible(montarget));
    }

    if (you.prev_targ != MHITNOT && you.prev_targ != MHITYOU)
    {
        const monsters *montarget = &menv[you.prev_targ];

        if (mons_near(montarget) && player_monster_visible(montarget))
        {
            mprf( MSGCH_PROMPT, "Current target: %s "
                  "(use p or f to fire at it again.)",
                  montarget->name(DESC_PLAIN).c_str() );
            return (true);
        }

        mpr("You have no current target.");
    }

    return (false);
}

// aaah, the simple joys of pointer arithmetic! {dlb}:
unsigned int monster_index(const monsters *monster)
{
    return (monster - menv.buffer());
}

bool heal_monster(monsters * patient, int health_boost,
                  bool permit_growth)
{
    if (mons_is_statue(patient->type))
        return (false);

    if (health_boost < 1)
        return (false);
    else if (!permit_growth && patient->hit_points == patient->max_hit_points)
        return (false);

    patient->hit_points += health_boost;

    bool success = true;

    if (patient->hit_points > patient->max_hit_points)
    {
        if (permit_growth)
        {
            const monsterentry* m = get_monster_data(patient->type);
            const int maxhp =
                m->hpdice[0] * (m->hpdice[1] + m->hpdice[2]) + m->hpdice[3];

            // Limit HP growth.
            if (random2(3 * maxhp) > 2 * patient->max_hit_points)
                patient->max_hit_points++;
            else
                success = false;
        }

        patient->hit_points = patient->max_hit_points;
    }

    return (success);
}                               // end heal_monster()

static spell_type _map_wand_to_mspell(int wand_type)
{
    switch (wand_type)
    {
    case WAND_FLAME:
        return SPELL_THROW_FLAME;
    case WAND_FROST:
        return SPELL_THROW_FROST;
    case WAND_SLOWING:
        return SPELL_SLOW;
    case WAND_HASTING:
        return SPELL_HASTE;
    case WAND_MAGIC_DARTS:
        return SPELL_MAGIC_DART;
    case WAND_HEALING:
        return SPELL_LESSER_HEALING;
    case WAND_PARALYSIS:
        return SPELL_PARALYSE;
    case WAND_FIRE:
        return SPELL_BOLT_OF_FIRE;
    case WAND_COLD:
        return SPELL_BOLT_OF_COLD;
    case WAND_CONFUSION:
        return SPELL_CONFUSE;
    case WAND_INVISIBILITY:
        return SPELL_INVISIBILITY;
    case WAND_TELEPORTATION:
        return SPELL_TELEPORT_OTHER;
    case WAND_LIGHTNING:
        return SPELL_LIGHTNING_BOLT;
    case WAND_DRAINING:
        return SPELL_BOLT_OF_DRAINING;
    case WAND_DISINTEGRATION:
        return SPELL_DISINTEGRATE;
    case WAND_POLYMORPH_OTHER:
        return SPELL_POLYMORPH_OTHER;
    default:
        return SPELL_NO_SPELL;
    }
}

void seen_monster(monsters *monster)
{
    // If the monster is in the auto_exclude list, automatically
    // set an exclusion.
    if (need_auto_exclude(monster) && !is_exclude_root(monster->pos()))
        toggle_exclude(monster->pos());

    // Monster was viewed this turn
    monster->flags |= MF_WAS_IN_VIEW;

    if (monster->flags & MF_SEEN)
        return;

    // First time we've seen this particular monster.
    monster->flags |= MF_SEEN;

    if (!mons_is_mimic(monster->type) && MONST_INTERESTING(monster))
    {
        take_note(
            Note(NOTE_SEEN_MONSTER, monster->type, 0,
                 monster->name(DESC_NOCAP_A).c_str()));
    }
}

//---------------------------------------------------------------
//
// shift_monster
//
// Moves a monster to approximately p and returns true if
// the monster was moved.
//
//---------------------------------------------------------------
bool shift_monster(monsters *mon, coord_def p)
{
    coord_def result;

    int count = 0;

    if (p.origin())
        p = mon->pos();

    for (adjacent_iterator ai(p); ai; ++ai)
    {
        // Don't drop on anything but vanilla floor right now.
        if (grd(*ai) != DNGN_FLOOR)
            continue;

        if (mgrd(*ai) != NON_MONSTER)
            continue;

        if (*ai == you.pos())
            continue;

        if (one_chance_in(++count))
            result = *ai;
    }

    if (count > 0)
    {
        const int mon_index = mgrd(mon->pos());
        mgrd(mon->pos()) = NON_MONSTER;
        mgrd(result) = mon_index;
        mon->moveto(result);
    }

    return (count > 0);
}
