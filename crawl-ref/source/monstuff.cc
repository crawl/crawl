/*
 *  File:       monstuff.cc
 *  Summary:    Misc monster related functions.
 *  Written by: Linley Henzell
 */

#include "AppHdr.h"
#include "monstuff.h"

//#include <stdlib.h>
//#include <string.h>
//#include <stdio.h>
//#include <algorithm>

#ifdef TARGET_OS_DOS
#include <conio.h>
#endif

#include "arena.h"
#include "artefact.h"
#include "attitude-change.h"
#include "cloud.h"
#include "database.h"
#include "delay.h"
#include "dgnevent.h"
#include "directn.h"
#include "fprop.h"
#include "files.h"
#include "food.h"
#include "godabil.h"
#include "hiscores.h"
#include "items.h"
#include "kills.h"
#include "message.h"
#include "misc.h"
#include "mon-behv.h"
#include "monplace.h"
#include "monspeak.h"
#include "notes.h"
#include "options.h"
#include "player.h"
#include "random.h"
#include "religion.h"
#include "spl-mis.h"
#include "spl-util.h"
#include "state.h"
#include "stuff.h"
#include "terrain.h"
#include "transfor.h"
#include "traps.h"
#include "tutorial.h"
#include "view.h"
#include "shout.h"
#include "viewchar.h"
#include "stash.h"
#include "xom.h"

static bool _wounded_damaged(monster_type mon_type);

// This function creates an artificial item to represent a mimic's
// appearance.  Eventually, mimics could be redone to be more like
// dancing weapons: there'd only be one type and it would look like the
// item it carries. - bwr
void get_mimic_item( const monsters *mimic, item_def &item )
{
    ASSERT(mimic != NULL && mons_is_mimic( mimic->type));

    item.base_type = OBJ_UNASSIGNED;
    item.sub_type  = 0;
    item.special   = 0;
    item.colour    = 0;
    item.flags     = 0;
    item.quantity  = 1;
    item.plus      = 0;
    item.plus2     = 0;
    item.pos       = mimic->pos();
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
        if (!you.inv[i].is_valid())
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
    const bool hostile_grid = feat_destroys_items(grd(monster->pos()));

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
                if (monster->friendly() && mitm[item].is_valid())
                    mitm[item].flags |= ISFLAG_DROPPED_BY_ALLY;

                move_item_to_grid(&item, monster->pos());

                if (mark_item_origins && mitm[item].is_valid())
                    origin_set_monster(mitm[item], monster);
            }

            monster->inv[i] = NON_ITEM;
        }
    }

    if (destroyed)
        mprf(MSGCH_SOUND, feat_item_destruction_message(grd(monster->pos())));
}

monster_type fill_out_corpse(const monsters* monster, item_def& corpse,
                             bool allow_weightless)
{
    ASSERT(!invalid_monster_type(monster->type));
    corpse.clear();

    int summon_type;
    if (monster->is_summoned(NULL, &summon_type)
        || (monster->flags & (MF_BANISHED | MF_HARD_RESET)))
    {
        return (MONS_NO_MONSTER);
    }

    monster_type corpse_class = mons_species(monster->type);

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
        return (MONS_NO_MONSTER);

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
    {
        corpse.props[CORPSE_NAME_KEY] = mons_type_name(monster->type,
                                                       DESC_PLAIN);
    }

    return (corpse_class);
}

bool explode_corpse(item_def& corpse, const coord_def& where)
{
    // Don't want chunks to show up behind the player.
    los_def ld(where, opc_no_actor);

    if (monster_descriptor(corpse.plus, MDSC_LEAVES_HIDE)
            && mons_genus(corpse.plus) == MONS_DRAGON)
    {
        // Uh... dragon hide is tough stuff and it keeps the monster in
        // one piece?  More importantly, it prevents a flavor feature
        // from becoming a trap for the unwary.

        return (false);
    }

    ld.update();

    int nchunks = 1 + random2(mons_weight(corpse.plus) / 150);
    nchunks = stepdown_value(nchunks, 4, 4, 12, 12);

    int ntries = 0;

    corpse.base_type = OBJ_FOOD;
    corpse.sub_type = FOOD_CHUNK;

    int blood = nchunks * 3;

    if (food_is_rotten(corpse))
        blood /= 3;

    blood_spray(where, static_cast<monster_type>(corpse.plus), blood);

    while (nchunks > 0 && ntries < 10000)
    {
        ++ntries;

        coord_def cp = where;
        cp.x += random_range(-LOS_RADIUS, LOS_RADIUS);
        cp.y += random_range(-LOS_RADIUS, LOS_RADIUS);

#ifdef DEBUG_DIAGNOSTICS
        mprf(MSGCH_DIAGNOSTICS, "Trying to scatter chunk to %d, %d...",
             cp.x, cp.y);
#endif

        if (! in_bounds(cp))
            continue;

        if (! ld.see_cell(cp))
            continue;

#ifdef DEBUG_DIAGNOSTICS
        mprf(MSGCH_DIAGNOSTICS, "Cell is visible...");
#endif

        if (feat_is_solid(grd(cp)) || actor_at(cp))
            continue;

        --nchunks;

        if (feat_destroys_items(grd(cp)))
        {
            if (!silenced(cp))
                mprf(MSGCH_SOUND, feat_item_destruction_message(grd(cp)));

            continue;
        }

#ifdef DEBUG_DIAGNOSTICS
        mprf(MSGCH_DIAGNOSTICS, "Success");
#endif

        copy_item_to_grid(corpse, cp);
    }

    return (true);
}

// Returns the item slot of a generated corpse, or -1 if no corpse.
int place_monster_corpse(const monsters *monster, bool silent,
                         bool force)
{
    // The game can attempt to place a corpse for an out-of-bounds monster
    // if a shifter turns into a giant spore and explodes.  In this
    // case we place no corpse since the explosion means anything left
    // over would be scattered, tiny chunks of shifter.
    if (!in_bounds(monster->pos()))
        return (-1);

    // Don't attempt to place corpses within walls, either.
    // Currently, this only applies to (shapeshifter) rock worms.
    if (feat_is_wall(grd(monster->pos())))
        return (-1);

    item_def corpse;
    const monster_type corpse_class = fill_out_corpse(monster, corpse);

    // Don't place a corpse?  If a zombified monster is somehow capable
    // of leaving a corpse, then always place it.
    if (mons_class_is_zombified(monster->type))
        force = true;

    if (corpse_class == MONS_NO_MONSTER || (!force && coinflip()))
        return (-1);

    int o = get_item_slot();
    if (o == NON_ITEM)
    {
        item_was_destroyed(corpse);
        return (-1);
    }

    mitm[o] = corpse;

    origin_set_monster(mitm[o], monster);

    if ((monster->flags & MF_EXPLODE_KILL)
            && explode_corpse(corpse, monster->pos()))
    {
        // We already have a spray of chunks
        destroy_item(o);
        return (-1);
    }

    if (feat_destroys_items(grd(monster->pos())))
    {
        item_was_destroyed(corpse);
        destroy_item(o);
        return (-1);
    }

    // Don't care if 'o' is changed, and it shouldn't be (corpses don't
    // stack).
    move_item_to_grid(&o, monster->pos());
    if (observe_cell(monster->pos()))
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

    if ((!victim_was_born_friendly || !mon->friendly())
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

    // FIXME: Since giant spores get detached from mgrd early
    // on, we can't tell by this point if they were visible when
    // they exploded. Rather than bothering to remember this, we
    // just suppress the message.
    if (monster->type == MONS_GIANT_SPORE
        || monster->type == MONS_BALL_LIGHTNING)
    {
        need_xp_msg = false;
    }

    // Give a message for monsters dying out of sight.
    if (need_xp_msg
        && exp_gain > 0
        && !you.can_see(monster)
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
    if (m->friendly()) // This includes enslaved monsters.
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

    if (!monster->is_holy()
            && monster->holiness() != MH_NATURAL
        || !monster->friendly()
        || !you.can_see(monster) // for simplicity
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

    if (monster->friendly() || !one_chance_in(10))
        return (false);

    if (MON_KILL(killer) && !invalid_monster_index(i))
    {
        monsters *mon = &menv[i];
        if (!mon->friendly() || !one_chance_in(3))
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
        && !monster->is_summoned() && !monster->is_shapeshifter()
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

static bool _slime_vault_in_los()
{
    bool in_los = false;

    for (int x = 0; x < GXM && !in_los; ++x)
    {
        for (int y = 0; y < GYM; ++y)
        {
            if ((grd[x][y] == DNGN_STONE_WALL
                    || grd[x][y] == DNGN_CLEAR_STONE_WALL)
                && observe_cell(coord_def(x, y)))
            {
                in_los = true;
                break;
            }
        }
    }

    return (in_los);
}

static bool _slime_vault_to_glass(bool silent)
{
    unset_level_flags(LFLAG_NO_TELE_CONTROL, silent);

    bool in_los = false;

    if (!silent)
        in_los = _slime_vault_in_los();

    replace_area_wrapper(DNGN_STONE_WALL, DNGN_CLEAR_ROCK_WALL);
    // In case it was already vitrified, but then it's less noticeable.
    replace_area_wrapper(DNGN_CLEAR_STONE_WALL, DNGN_CLEAR_ROCK_WALL);

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

    remove_all_jiyva_altars();

    if (silenced(you.pos()))
    {
        god_speaks(GOD_JIYVA, "With an infernal shudder, the power ruling "
                   "this place vanishes!");
    }
    else
    {
        god_speaks(GOD_JIYVA, "With infernal noise, the power ruling this "
                   "place vanishes!");
    }

    return (true);
}

static bool _slime_vault_to_glass_offlevel()
{
    return _slime_vault_to_glass(true);
}

static bool _slime_vault_to_glass_onlevel()
{
    return _slime_vault_to_glass(false);
}

static bool _slime_vault_to_floor(bool silent)
{
    unset_level_flags(LFLAG_NO_TELE_CONTROL, silent);

    bool in_los = false;

    if (!silent)
        in_los = _slime_vault_in_los();

    replace_area_wrapper(DNGN_STONE_WALL, DNGN_FLOOR);
    // In case it was already vitrified, but then it's less noticeable.
    replace_area_wrapper(DNGN_CLEAR_STONE_WALL, DNGN_FLOOR);

    if (silenced(you.pos()))
        mpr("An unexplained breeze blows through the dungeon.", MSGCH_GOD);
    else
        mpr("You hear the sound of toppling stones.", MSGCH_GOD);

    return (true);
}

static bool _slime_vault_to_floor_offlevel()
{
    return _slime_vault_to_floor(true);
}

static bool _slime_vault_to_floor_onlevel()
{
    return _slime_vault_to_floor(false);
}

void slime_vault_change(bool glass)
{
    const level_id target(BRANCH_SLIME_PITS, 6);
    if (is_existing_level(target))
    {
        if (glass)
        {
            apply_to_level(target,
                           target == level_id::current() ?
                               _slime_vault_to_glass_onlevel :
                               _slime_vault_to_glass_offlevel);
        }
        else
        {
            apply_to_level(target,
                           target == level_id::current() ?
                               _slime_vault_to_floor_onlevel :
                               _slime_vault_to_floor_offlevel);
        }
    }
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
        you.royal_jelly_dead = true;

        if (jiyva_is_dead())
            slime_vault_change(true);
    }
}

static void _mummy_curse(monsters* monster, killer_type killer, int index)
{
    int pow;

    switch (killer)
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

    switch (monster->type)
    {
        case MONS_MENKAURE:
        case MONS_MUMMY:          pow = 1; break;
        case MONS_GUARDIAN_MUMMY: pow = 3; break;
        case MONS_MUMMY_PRIEST:   pow = 8; break;
        case MONS_GREATER_MUMMY:  pow = 11; break;
        case MONS_KHUFU:          pow = 15; break;

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

    if ((monster->type == MONS_MUMMY || monster->type == MONS_MENKAURE) && YOU_KILL(killer))
        curse_an_item(true);
    else
    {
        if (index == NON_MONSTER)
        {
            mpr("You feel extremely nervous for a moment...",
                MSGCH_MONSTER_SPELL);
        }
        else if (you.can_see(target))
        {
            mprf(MSGCH_MONSTER_SPELL, "A malignant aura surrounds %s.",
                 target->name(DESC_NOCAP_THE).c_str());
        }
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

    bolt beam;
    const int type = monster->type;

    beam.is_tracer    = false;
    beam.is_explosion = true;
    beam.beam_source  = monster_index(monster);
    beam.type         = dchar_glyph(DCHAR_FIRED_BURST);
    beam.source       = monster->pos();
    beam.target       = monster->pos();
    beam.thrower      = crawl_state.arena ? KILL_MON
      : monster->attitude == ATT_FRIENDLY ? KILL_YOU : KILL_MON;
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
        viewwindow(false);
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
    monster->set_position(coord_def(0,0));

    // Exploding kills the monster a bit earlier than normal.
    monster->hit_points = -16;
    if (saw)
        viewwindow(false);

    // FIXME: show_more == mons_near(monster)
    beam.explode();

    // Monster died in explosion, so don't re-attach it to the grid.
    return (true);
}

void _monster_die_cloud(const monsters* monster, bool corpse, bool silent,
                        bool summoned)
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

// XXX: Another hackish function! May do weird things if multiple copies of
//      the band have been placed using wizard mode. {due}
static void _elven_twin_died(monsters* twin)
{
    bool found_duvessa = false;
    bool found_dowan = false;
    monsters *monster;

    for (int i = 0; i < MAX_MONSTERS; ++i)
    {
        monster = &menv[i];
        if (monster->alive() && monster->type == MONS_DUVESSA)
        {
            found_duvessa = true;
            break;
        }
        else if (monster->alive() && monster->type == MONS_DOWAN)
        {
            found_dowan = true;
            break;
        }
    }

    if ((found_duvessa || found_dowan) && monster->observable())
    {
        // Will generate strings such as 'Duvessa_Duvessa_dies' or, alternately
        // 'Dowan_Dowan_dies', but as neither will match, these can safely be
        // ignored.
        std::string death_message = getSpeakString("_"
            + monster->name(DESC_CAP_THE, true) + "_"
            + twin->name(DESC_CAP_THE) + "_dies_");

        if (!death_message.empty())
            mons_speaks_msg(monster, death_message, MSGCH_TALK, silenced(you.pos()));
    }

    if (found_duvessa && monster->observable())
    {
        // Provides its own flavour message.
        monster->go_berserk(true);
    }
    else if (found_dowan && monster->observable())
    {
        // Doesn't provide any message, so needs one.
        simple_monster_message(monster, " turns to flee.");
        monster->add_ench(mon_enchant(ENCH_FEAR, 0, KC_YOU));
        behaviour_event(monster, ME_SCARE, MHITNOT);
    }
}

void pikel_band_neutralise ()
{
    // XXX: This is a really ugly hack. It should be replaced by something else
    // when band tracking is available. This assumes that the only human monsters
    // with MF_BAND_MEMBER are Pikel's band members.
    bool message_made = false;

    for (int i = 0; i < MAX_MONSTERS; ++i)
    {
        monsters *monster = &menv[i];
        if (monster->alive() && monster->type == MONS_HUMAN
            && testbits(monster->flags, MF_BAND_MEMBER))
        {
            if (monster->observable() && !message_made)
            {
                mpr("Pikel's slaves thank you for their freedom.");
                message_made = true;
            }
            mons_pacify(monster);
        }
    }
}

static void _hogs_to_humans()
{
    // Simplification: if, in a rare event, another hog which was not created
    // as a part of Kirke's band happens to be on the level, the player can't
    // tell them apart anyway.
    // On the other hand, hogs which left the level are too far away to be
    // affected by the magic of Kirke's death.
    // FIXME: If another monster was polymorphed into a hog by Kirke's
    //        porkalator spell, they should be handled specially...
    int any = 0, human = 0;

    for (int i = 0; i < MAX_MONSTERS; ++i)
    {
        monsters *monster = &menv[i];
        if (monster->alive() && monster->type == MONS_HOG)
        {
            const bool could_see = you.can_see(monster);

            // XXX: This resets the size of slime creatures, the number
            // of heads a hydra has, and the number of spikes a manticore
            // has.  Plus it also changes the colour of a draconian which
            // has a sub-type.  And it re-rolls the spellbook the monster
            // has.
            if (monster->number == 0)
                monster->type = MONS_HUMAN;
            else
                monster->type = (monster_type) (monster->number - 1);

            monster->number = 0;
            define_monster(*monster);

            const bool can_see = you.can_see(monster);

            // A monster changing factions while in the arena messes up
            // arena book-keeping.
            if (!crawl_state.arena)
            {
                // * A monster's attitude shouldn't downgrade from friendly
                //   or good-neutral because you helped it.  It'd suck to
                //   lose a permanent ally that way.
                //
                // * A monster has to be smart enough to realize that you
                //   helped it.
                if (monster->attitude == ATT_HOSTILE
                    && mons_intel(monster) >= I_NORMAL)
                {
                    monster->attitude = ATT_GOOD_NEUTRAL;
                    monster->flags |= MF_WAS_NEUTRAL;
                }
            }

            behaviour_event(monster, ME_EVAL);

            if (could_see && can_see)
            {
                any++;
                if (monster->type == MONS_HUMAN)
                    human++;
            }
            else if (could_see && !can_see)
                mpr("The hog vanishes!");
            else if (!could_see && can_see)
                mprf("%s appears from out of thin air!",
                     monster->name(DESC_CAP_A).c_str());
        }
    }

    if (any == 1)
    {
        if (any == human)
            mpr("No longer under Kirke's spell, the hog turns into a human!");
        else
            mpr("No longer under Kirke's spell, the hog returns to its "
                "original form!");
    }
    else if (any > 1)
    {
        if (any == human)
            mpr("No longer under Kirke's spell, all hogs revert to their "
                "human forms!");
        else
            mpr("No longer under Kirke's spell, all hogs revert to their "
                "original forms!");
    }

    // Revert the player as well.
    if (you.attribute[ATTR_TRANSFORMATION] == TRAN_PIG)
        untransform();
}

static int _tentacle_too_far(monsters *head, monsters *tentacle)
{
    // The Shoals produce no disjoint bodies of water.
    // If this ever changes, we'd need to check if the head and tentacle
    // are still in the same pool.
    // XXX: Actually, using Fedhas's Sunlight power you can separate pools...
    return grid_distance(head->pos(), tentacle->pos()) > LOS_RADIUS;
}

void mons_relocated(monsters *monster)
{
    if (monster->type == MONS_KRAKEN)
    {
        int headnum = monster_index(monster);

        if (invalid_monster_index(headnum))
            return;

        for (int i = 0; i < MAX_MONSTERS; ++i)
        {
            monsters *tentacle = &menv[i];
            if (tentacle->type == MONS_KRAKEN_TENTACLE
                && (int)tentacle->number == headnum
                && _tentacle_too_far(monster, tentacle))
            {
                monster_die(tentacle, KILL_RESET, -1, true, false);
            }
        }
    }
    else if (monster->type == MONS_KRAKEN_TENTACLE)
    {
        if (invalid_monster_index(monster->number)
            || menv[monster->number].type != MONS_KRAKEN
            || _tentacle_too_far(&menv[monster->number], monster))
        {
            monster_die(monster, KILL_RESET, -1, true, false);
        }
    }
}

static int _destroy_tentacles(monsters *head)
{
    int tent = 0;
    int headnum = monster_index(head);

    if (invalid_monster_index(headnum))
        return 0;

    for (int i = 0; i < MAX_MONSTERS; ++i)
    {
        monsters *monster = &menv[i];
        if (monster->type == MONS_KRAKEN_TENTACLE
            && (int)monster->number == headnum)
        {
            if (mons_near(monster))
                tent++;
            monster->hurt(monster, INSTANT_DEATH);
        }
    }
    return tent;
}

// Returns the slot of a possibly generated corpse or -1.
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

    ASSERT(!( YOU_KILL(killer) && crawl_state.arena ));

    mons_clear_trapping_net(monster);

    you.remove_beholder(monster);

    // Clear auto exclusion now the monster is killed -- if we know about it.
    if (mons_near(monster) || wizard)
        remove_auto_exclude(monster);

          int  summon_type   = 0;
          int  duration      = 0;
    const bool summoned      = monster->is_summoned(&duration, &summon_type);
    const int monster_killed = monster_index(monster);
    const bool hard_reset    = testbits(monster->flags, MF_HARD_RESET);
    const bool gives_xp      = (!summoned && !mons_class_flag(monster->type,
                                                              M_NO_EXP_GAIN));

    const bool drop_items    = !hard_reset;

    const bool mons_reset(killer == KILL_RESET || killer == KILL_DISMISSED);

    const bool submerged     = monster->submerged();

    bool in_transit          = false;

#ifdef DGL_MILESTONES
    if (!crawl_state.arena)
        _check_kill_milestone(monster, killer, killer_index);
#endif

    // Award experience for suicide if the suicide was caused by the
    // player.
    if (MON_KILL(killer) && monster_killed == killer_index)
    {
        if (monster->confused_by_you())
        {
            ASSERT(!crawl_state.arena);
            killer = KILL_YOU_CONF;
        }
    }
    else if (MON_KILL(killer) && monster->has_ench(ENCH_CHARM))
    {
        ASSERT(!crawl_state.arena);
        killer = KILL_YOU_CONF; // Well, it was confused in a sense... (jpeg)
    }

    // Take note!
    if (!mons_reset && !crawl_state.arena && MONST_INTERESTING(monster))
    {
        take_note(Note(NOTE_KILL_MONSTER,
                       monster->type, monster->friendly(),
                       monster->full_name(DESC_NOCAP_A).c_str()));
    }

    // From time to time Trog gives you a little bonus.
    if (killer == KILL_YOU && you.berserk())
    {
        if (you.religion == GOD_TROG
            && !player_under_penance() && you.piety > random2(1000))
        {
            const int bonus = 3 + random2avg( 10, 2 );

            you.duration[DUR_BERSERKER] += bonus;
            you.duration[DUR_MIGHT] += bonus;
            haste_player(bonus);

            mpr("You feel the power of Trog in you as your rage grows.",
                MSGCH_GOD, GOD_TROG);
        }
        else if (wearing_amulet(AMU_RAGE) && one_chance_in(30))
        {
            const int bonus = 2 + random2(4);

            you.duration[DUR_BERSERKER] += bonus;
            you.duration[DUR_MIGHT] += bonus;
            haste_player(bonus);

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
            simple_monster_message(monster, " dissipates!",
                                   MSGCH_MONSTER_DAMAGE, MDAM_DEAD);
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
            simple_monster_message(monster, " vapourises!",
                                   MSGCH_MONSTER_DAMAGE,  MDAM_DEAD);
            silent = true;
        }

        if (!wizard && !mons_reset && !submerged)
        {
            place_cloud(CLOUD_COLD, monster->pos(), 2 + random2(4),
                        monster->kill_alignment());
        }

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
                simple_monster_message(monster, " falls from the air.",
                                       MSGCH_MONSTER_DAMAGE, MDAM_DEAD);
                silent = true;
            }
            else
                killer = KILL_RESET;
        }
    }

    const bool death_message = !silent && !did_death_message
                               && mons_near(monster)
                               && (monster->visible_to(&you)
                                   || crawl_state.arena);
    const bool exploded      = monster->flags & MF_EXPLODE_KILL;

    const bool created_friendly = testbits(monster->flags, MF_CREATED_FRIENDLY);
          bool anon = (killer_index == ANON_FRIENDLY_MONSTER);
    const mon_holy_type targ_holy = monster->holiness();

    switch (killer)
    {
        case KILL_YOU:          // You kill in combat.
        case KILL_YOU_MISSILE:  // You kill by missile or beam.
        case KILL_YOU_CONF:     // You kill by confusion.
        {
            const bool bad_kill    = god_hates_killing(you.religion, monster);
            const bool was_neutral = testbits(monster->flags, MF_WAS_NEUTRAL);

            if (death_message)
            {
                if (killer == KILL_YOU_CONF
                    && (anon || !invalid_monster_index(killer_index)))
                {
                    mprf(MSGCH_MONSTER_DAMAGE, MDAM_DEAD, "%s is %s!",
                         monster->name(DESC_CAP_THE).c_str(),
                         exploded                        ? "blown up" :
                         _wounded_damaged(monster->type) ? "destroyed"
                                                         : "killed");
                }
                else
                {
                    mprf(MSGCH_MONSTER_DAMAGE, MDAM_DEAD, "You %s %s!",
                         exploded                        ? "blow up" :
                         _wounded_damaged(monster->type) ? "destroy"
                                                         : "kill",
                         monster->name(DESC_NOCAP_THE).c_str());
                }

                if ((created_friendly || was_neutral) && gives_xp)
                    mpr("That felt strangely unrewarding.");
            }

            // Killing triggers tutorial lesson.
            if (gives_xp)
                _tutorial_inspect_kill();

            // Prevent summoned creatures from being good kills.
            if (bad_kill || !created_friendly && gives_xp)
            {
                if (targ_holy == MH_NATURAL)
                {
                    did_god_conduct(DID_KILL_LIVING,
                                    monster->hit_dice, true, monster);

                    if (monster->is_evil())
                    {
                        did_god_conduct(DID_KILL_NATURAL_EVIL,
                                        monster->hit_dice, true, monster);
                    }
                }
                else if (targ_holy == MH_UNDEAD)
                {
                    did_god_conduct(DID_KILL_UNDEAD,
                                    monster->hit_dice, true, monster);
                }
                else if (targ_holy == MH_DEMONIC)
                {
                    did_god_conduct(DID_KILL_DEMON,
                                    monster->hit_dice, true, monster);
                }

                // Zin hates chaotic beings.
                if (monster->is_chaotic())
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
                if (monster->is_priest())
                {
                    did_god_conduct(DID_KILL_PRIEST,
                                    monster->hit_dice, true, monster);
                }

                if (mons_is_slime(monster))
                {
                    did_god_conduct(DID_KILL_SLIME, monster->hit_dice,
                                    true, monster);
                }

                if (fedhas_protects(monster))
                {
                    did_god_conduct(DID_KILL_PLANT, monster->hit_dice,
                                    true, monster);
                }

                // Cheibriados hates fast monsters.
                if (mons_is_fast(monster))
                {
                    did_god_conduct(DID_KILL_FAST, monster->hit_dice,
                                    true, monster);
                }

                // Holy kills are always noticed.
                if (monster->is_holy())
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
                           && monster->is_evil())
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
                       && monster->is_evil())
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
                    && random2(you.piety) >= piety_breakpoint(2))
                && !player_under_penance())
            {
                bless_follower();
            }

            if (you.duration[DUR_DEATH_CHANNEL]
                && monster->holiness() == MH_NATURAL
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
                                0, 0, monster->pos(), MHITYOU,
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

        case KILL_MON:          // Monster kills in combat.
        case KILL_MON_MISSILE:  // Monster kills by missile or beam.
            if (!silent)
            {
                const char* msg =
                    exploded                        ? " is blown up!" :
                    _wounded_damaged(monster->type) ? " is destroyed!"
                                                    : " dies!";
                simple_monster_message(monster, msg, MSGCH_MONSTER_DAMAGE,
                                       MDAM_DEAD);
            }

            if (crawl_state.arena)
                break;

            // No piety loss if god gifts killed by other monsters.
            // Also, dancing weapons aren't really friendlies.
            if (monster->friendly()
                && monster->type != MONS_DANCING_WEAPON)
            {
                did_god_conduct(DID_FRIEND_DIED, 1 + (monster->hit_dice / 2),
                                true, monster);
            }

            if (pet_kill && fedhas_protects(monster))
            {
                did_god_conduct(DID_ALLY_KILLED_PLANT, 1 + (monster->hit_dice / 2),
                                true, monster);
            }

            // Trying to prevent summoning abuse here, so we're trying to
            // prevent summoned creatures from being done_good kills.  Only
            // affects creatures which were friendly when summoned.
            if (!created_friendly && gives_xp && pet_kill
                && (anon || !invalid_monster_index(killer_index)))
            {
                bool notice = false;

                monsters *killer_mon = NULL;
                if (!anon)
                {
                    killer_mon = &menv[killer_index];

                    // If the killer is already dead treat it like an
                    // anonymous monster.
                    if (killer_mon->type == MONS_NO_MONSTER)
                        anon = true;
                }

                const mon_holy_type killer_holy =
                    anon ? MH_NATURAL : killer_mon->holiness();

                if (you.religion == GOD_SHINING_ONE
                    || you.religion == GOD_YREDELEMNUL
                    || you.religion == GOD_KIKUBAAQUDGHA
                    || you.religion == GOD_VEHUMET
                    || you.religion == GOD_MAKHLEB
                    || you.religion == GOD_LUGONU
                    || !anon && mons_is_god_gift(killer_mon))
                {
                    if (killer_holy == MH_UNDEAD)
                    {
                        const bool confused =
                            anon ? false : !killer_mon->friendly();

                        // Yes, these are hacks, but they make sure that
                        // confused monsters doing kills are not
                        // referred to as "slaves", and I think it's
                        // okay that e.g. Yredelemnul ignores kills done
                        // by confused monsters as opposed to enslaved
                        // or friendly ones. (jpeg)
                        if (targ_holy == MH_NATURAL)
                        {
                            notice |= did_god_conduct(
                                          !confused ? DID_LIVING_KILLED_BY_UNDEAD_SLAVE :
                                                      DID_LIVING_KILLED_BY_SERVANT,
                                          monster->hit_dice);
                        }
                        else if (targ_holy == MH_UNDEAD)
                        {
                            notice |= did_god_conduct(
                                          !confused ? DID_UNDEAD_KILLED_BY_UNDEAD_SLAVE :
                                                      DID_UNDEAD_KILLED_BY_SERVANT,
                                          monster->hit_dice);
                        }
                        else if (targ_holy == MH_DEMONIC)
                        {
                            notice |= did_god_conduct(
                                          !confused ? DID_DEMON_KILLED_BY_UNDEAD_SLAVE :
                                                      DID_DEMON_KILLED_BY_SERVANT,
                                          monster->hit_dice);
                        }
                    }
                    // Yes, we are splitting undead pets from the others
                    // as a way to focus Necromancy vs. Summoning
                    // (ignoring Haunt here)... at least we're being
                    // nice and putting the natural creature summons
                    // together with the demonic ones.  Note that
                    // Vehumet gets a free pass here since those
                    // followers are assumed to come from summoning
                    // spells...  the others are from invocations (TSO,
                    // Makhleb, Kiku). - bwr
                    else if (targ_holy == MH_NATURAL)
                    {
                        notice |= did_god_conduct(DID_LIVING_KILLED_BY_SERVANT,
                                                  monster->hit_dice);

                        if (monster->is_evil())
                        {
                            notice |= did_god_conduct(
                                          DID_NATURAL_EVIL_KILLED_BY_SERVANT,
                                          monster->hit_dice);
                        }
                    }
                    else if (targ_holy == MH_UNDEAD)
                    {
                        notice |= did_god_conduct(DID_UNDEAD_KILLED_BY_SERVANT,
                                                  monster->hit_dice);
                    }
                    else if (targ_holy == MH_DEMONIC)
                    {
                        notice |= did_god_conduct(DID_DEMON_KILLED_BY_SERVANT,
                                                  monster->hit_dice);
                    }
                }

                // Holy kills are always noticed.
                if (monster->is_holy())
                {
                    if (killer_holy == MH_UNDEAD)
                    {
                        const bool confused =
                            anon ? false : !killer_mon->friendly();

                        // Yes, this is a hack, but it makes sure that
                        // confused monsters doing kills are not
                        // referred to as "slaves", and I think it's
                        // okay that Yredelemnul ignores kills done by
                        // confused monsters as opposed to enslaved or
                        // friendly ones. (jpeg)
                        notice |= did_god_conduct(
                                      !confused ? DID_HOLY_KILLED_BY_UNDEAD_SLAVE :
                                                  DID_HOLY_KILLED_BY_SERVANT,
                                      monster->hit_dice);
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
                        inc_mp(1 + random2(monster->hit_dice / 2), false);
                    }
                }

                if (you.religion == GOD_SHINING_ONE
                    && monster->is_evil()
                    && !player_under_penance()
                    && random2(you.piety) >= piety_breakpoint(0)
                    && !invalid_monster_index(killer_index))
                {
                    // Randomly bless the follower who killed.
                    if (!one_chance_in(3) && killer_mon->alive()
                        && bless_follower(killer_mon))
                    {
                        break;
                    }

                    if (killer_mon->alive()
                        && killer_mon->hit_points < killer_mon->max_hit_points)
                    {
                        simple_monster_message(killer_mon,
                                               " looks invigorated.");
                        heal_monster(killer_mon,
                                     1 + random2(monster->hit_dice / 4), false);
                    }
                }

                if (you.religion == GOD_BEOGH
                    && random2(you.piety) >= piety_breakpoint(2)
                    && !player_under_penance()
                    && !one_chance_in(3)
                    && !invalid_monster_index(killer_index))
                {
                    // Randomly bless the follower who killed.
                    bless_follower(killer_mon);
                }
            }
            break;

        // Monster killed by trap/inanimate thing/itself/poison not from you.
        case KILL_MISC:
            if (!silent)
            {
                const char* msg =
                    exploded                        ? " is blown up!" :
                    _wounded_damaged(monster->type) ? " is destroyed!"
                                                    : " dies!";
                simple_monster_message(monster, msg, MSGCH_MONSTER_DAMAGE,
                                       MDAM_DEAD);
            }
            break;

        case KILL_RESET:
            // Monster doesn't die, just goes back to wherever it came from.
            // This must only be called by monsters running out of time (or
            // abjuration), because it uses the beam variables!  Or does it???
            // Pacified monsters leave the level when this happens.

            // KILL_RESET monsters no longer lose their whole inventory, only
            // items they were generated with.
            if (monster->pacified() || !monster->needs_transit())
            {
                // A banished monster that doesn't go on the transit list
                // loses all items.
                if (!monster->is_summoned())
                    monster->destroy_inventory();
                break;
            }

            // Monster goes to the Abyss.
            monster->flags |= MF_BANISHED;
            monster->set_transit(level_id(LEVEL_ABYSS));
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

    // Make sure Boris has a foe to address.
    if (monster->foe == MHITNOT)
    {
        if (!monster->wont_attack() && !crawl_state.arena)
            monster->foe = MHITYOU;
        else if (!invalid_monster_index(killer_index))
            monster->foe = killer_index;
    }

    if (!silent && !wizard && observe_cell(monster->pos()))
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
        // And his vault can  be placed again.
        you.uniq_map_names.erase("uniq_boris");
    }
    else if (monster->type == MONS_KIRKE && !in_transit)
    {
        _hogs_to_humans();
    }
    else if (monster->type == MONS_PIKEL)
    {
        // His slaves don't care if he's dead or not, just whether or not
        // he goes away.
        pikel_band_neutralise();
    }
    else if (monster->type == MONS_KRAKEN)
    {
        if (_destroy_tentacles(monster) && !in_transit)
        {
            mpr("The kraken is slain, and its tentacles slide "
                "back into the water like the carrion they now are.");
        }
    }
    else if ((monster->type == MONS_DOWAN || monster->type == MONS_DUVESSA)
              && monster->observable())
    {
        _elven_twin_died(monster);
    }
    else if (!monster->is_summoned())
    {
        if (mons_genus(monster->type) == MONS_MUMMY)
            _mummy_curse(monster, killer, killer_index);
    }

    if (!wizard && !submerged)
        _monster_die_cloud(monster, !mons_reset, silent, summoned);

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
        // don't clutter up mitm[].
        monster->destroy_inventory();
    }

    if (!silent && !wizard && !mons_reset && corpse != -1
        && !(monster->flags & MF_KNOWN_MIMIC)
        && monster->is_shapeshifter())
    {
        simple_monster_message(monster, "'s shape twists and changes "
                               "as it dies.");
    }

    // If we kill an invisible monster reactivate autopickup.
    if (mons_near(monster) && !monster->visible_to(&you))
        autotoggle_autopickup(false);

    crawl_state.dec_mon_acting(monster);
    monster_cleanup(monster);

    // Force redraw for monsters that die.
    if (observe_cell(mwhere))
    {
        view_update_at(mwhere);
        update_screen();
    }

    return (corpse);
}

// Clean up after a dead monster.
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
        if (monster->alive()
            && mons_near(monster)
            && !monster->asleep())
        {
            behaviour_event(monster, ME_ALERT, MHITYOU);
        }
    }
}

static bool _valid_morph(monsters *monster, monster_type new_mclass)
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
    if (mons_class_holiness(new_mclass) != monster->holiness()
        || mons_class_flag(new_mclass, M_UNIQUE)      // no uniques
        || mons_class_flag(new_mclass, M_NO_EXP_GAIN) // not helpless
        || new_mclass == mons_species(monster->type)  // must be different
        || new_mclass == MONS_PROGRAM_BUG

        // These require manual setting of mons.base_monster to indicate
        // what they are a skeleton/zombie/simulacrum/spectral thing of,
        // which we currently aren't smart enough to handle.
        || mons_class_is_zombified(new_mclass)

        // These require manual setting of the ghost demon struct to
        // indicate their characteristics, which we currently aren't
        // smart enough to handle.
        || mons_is_ghost_demon(new_mclass)

        // Only for use by game testers or in the arena.
        || new_mclass == MONS_TEST_SPAWNER

        // Other poly-unsuitable things.
        || new_mclass == MONS_ORB_GUARDIAN
        || mons_is_statue(new_mclass)

        // The spell on Prince Ribbit can't be broken so easily.
        || (new_mclass == MONS_HUMAN && monster->type == MONS_PRINCE_RIBBIT))
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
                return (simple_monster_message(monster, " shudders."));
        }
        while (tries-- && (!_valid_morph(monster, targetc)
                           || _is_poly_power_unsuitable(power, source_power,
                                                        target_power, relax)));
    }

    if (!_valid_morph(monster, targetc))
    {
        return (simple_monster_message(monster,
                                       " looks momentarily different."));
    }

    // Messaging.
    bool can_see     = you.can_see(monster);
    bool can_see_new = !mons_class_flag(targetc, M_INVIS) || you.can_see_invisible();

    bool need_note = false;
    std::string old_name = monster->full_name(DESC_CAP_A);

    // If old monster is visible to the player, and is interesting,
    // then note why the interesting monster went away.
    if (can_see && MONST_INTERESTING(monster))
        need_note = true;

    std::string new_name = "";
    if (monster->type == MONS_OGRE && targetc == MONS_TWO_HEADED_OGRE)
        str_polymon = " grows a second head";
    else
    {
        if (monster->is_shapeshifter())
            str_polymon = " changes into ";
        else if (targetc == MONS_PULSATING_LUMP)
            str_polymon = " degenerates into ";
        else if (you.religion == GOD_JIYVA
                 && (targetc == MONS_DEATH_OOZE
                     || targetc == MONS_OOZE
                     || targetc == MONS_JELLY
                     || targetc == MONS_BROWN_OOZE
                     || targetc == MONS_SLIME_CREATURE
                     || targetc == MONS_GIANT_AMOEBA
                     || targetc == MONS_ACID_BLOB
                     || targetc == MONS_AZURE_JELLY))
        {
            // Message used for the Slimify ability.
            str_polymon = " quivers uncontrollably and liquefies into ";
        }
        else
            str_polymon = " evaporates and reforms as ";

        if (!can_see_new)
        {
            new_name = "something unseen";
            str_polymon += "something you cannot see";
        }
        else
        {
            str_polymon += mons_type_name(targetc, DESC_NOCAP_A);

            if (targetc == MONS_PULSATING_LUMP)
                str_polymon += " of flesh";
        }
    }
    str_polymon += "!";

    bool player_messaged = can_see
                       && simple_monster_message(monster, str_polymon.c_str());

    // Even if the monster transforms from one type that can behold the
    // player into a different type which can also behold the player,
    // the polymorph disrupts the beholding process.  Do this before
    // changing monster->type, since unbeholding can only happen while
    // the monster is still a mermaid/siren.
    you.remove_beholder(monster);

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

    const monster_type real_targetc =
        (monster->has_ench(ENCH_GLOWING_SHAPESHIFTER)) ? MONS_GLOWING_SHAPESHIFTER :
        (monster->has_ench(ENCH_SHAPESHIFTER))         ? MONS_SHAPESHIFTER
                                                       : targetc;

    const god_type god =
        (player_will_anger_monster(real_targetc)
            || (you.religion == GOD_BEOGH
                && mons_species(real_targetc) != MONS_ORC)) ? GOD_NO_GOD
                                                            : monster->god;

    if (god == GOD_NO_GOD)
        flags &= ~MF_GOD_GIFT;

    const int  old_hp             = monster->hit_points;
    const int  old_hp_max         = monster->max_hit_points;
    const bool old_mon_caught     = monster->caught();
    const char old_ench_countdown = monster->ench_countdown;

    mon_enchant abj       = monster->get_ench(ENCH_ABJ);
    mon_enchant charm     = monster->get_ench(ENCH_CHARM);
    mon_enchant neutral   = monster->get_ench(ENCH_NEUTRAL);
    mon_enchant shifter   = monster->get_ench(ENCH_GLOWING_SHAPESHIFTER,
                                              ENCH_SHAPESHIFTER);
    mon_enchant sub       = monster->get_ench(ENCH_SUBMERGED);
    mon_enchant summon    = monster->get_ench(ENCH_SUMMON);
    mon_enchant tp        = monster->get_ench(ENCH_TP);

    monster_spells spl    = monster->spells;
    const bool need_save_spells
            = (!name.empty()
               && (!monster->can_use_spells()
                   || monster->is_actual_spellcaster()));

    // deal with mons_sec
    monster->type         = targetc;
    monster->base_monster = MONS_NO_MONSTER;
    monster->number       = 0;

    // Note: define_monster() will clear out all enchantments! - bwr
    define_monster(monster_index(monster));

    monster->mname = name;
    monster->flags = flags;
    monster->god   = god;

    // Keep spells for named monsters, but don't override innate ones
    // for dragons and the like. This means that Sigmund polymorphed
    // into a goblin will still cast spells, but if he ends up as a
    // swamp drake he'll breathe fumes and, if polymorphed further,
    // won't remember his spells anymore.
    if (need_save_spells
        && (!monster->can_use_spells() || monster->is_actual_spellcaster()))
    {
        monster->spells = spl;
    }

    monster->add_ench(abj);
    monster->add_ench(charm);
    monster->add_ench(neutral);
    monster->add_ench(shifter);
    monster->add_ench(sub);
    monster->add_ench(summon);
    monster->add_ench(tp);

    // Allows for handling of submerged monsters which polymorph into
    // monsters that can't submerge on this square.
    if (monster->has_ench(ENCH_SUBMERGED)
        && !monster_can_submerge(monster, grd(monster->pos())))
    {
        monster->del_ench(ENCH_SUBMERGED);
    }

    monster->ench_countdown = old_ench_countdown;

    if (mons_class_flag(monster->type, M_INVIS))
        monster->add_ench(ENCH_INVIS);

    if (!player_messaged && you.can_see(monster))
    {
        mprf("%s appears out of thin air!", monster->name(DESC_CAP_A).c_str());
        autotoggle_autopickup(false);
        player_messaged = true;
    }

    monster->hit_points = monster->max_hit_points
                                * ((old_hp * 100) / old_hp_max) / 100
                                + random2(monster->max_hit_points);

    monster->hit_points = std::min(monster->max_hit_points,
                                   monster->hit_points);

    // Don't kill it.
    monster->hit_points = std::max(monster->hit_points, 1);

    monster->speed_increment = 67 + random2(6);

    monster_drop_ething(monster);

    // New monster type might be interesting.
    mark_interesting_monst(monster);
    if (new_name.empty())
        new_name = monster->full_name(DESC_NOCAP_A);

    if (need_note
        || can_see && you.can_see(monster) && MONST_INTERESTING(monster))
    {
        take_note(Note(NOTE_POLY_MONSTER, 0, 0, old_name.c_str(),
                       new_name.c_str()));

        if (you.can_see(monster))
            monster->flags |= MF_SEEN;
    }

    // If new monster is visible to player, then we've seen it.
    if (you.can_see(monster))
    {
        seen_monster(monster);
        // If the player saw both the beginning and end results of a
        // shifter changing, then s/he knows it must be a shifter.
        if (can_see && shifter.ench != ENCH_NONE)
            monster->flags |= MF_KNOWN_MIMIC;
    }

    if (old_mon_caught)
        check_net_will_hold_monster(monster);

    if (!force_beh)
        player_angers_monster(monster);

    // Xom likes watching monsters being polymorphed.
    xom_is_stimulated(monster->is_shapeshifter()               ? 16 :
                      power == PPT_LESS || monster->friendly() ? 32 :
                      power == PPT_SAME                           ? 64 : 128);

    return (player_messaged);
}

// If the returned value is mon.pos(), then nothing was found.
static coord_def _random_monster_nearby_habitable_space(const monsters& mon,
                                                        bool allow_adjacent,
                                                        bool respect_los)
{
    const bool respect_sanctuary = mon.wont_attack();

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

        if (respect_los && !mon.see_cell(target))
            continue;

        // Survived everything, break out (with a good value of target.)
        break;
    }

    if (tries == 150)
        target = mon.pos();

    return (target);
}

bool monster_blink(monsters *monster, bool quiet)
{
    coord_def near = _random_monster_nearby_habitable_space(*monster, false,
                                                            true);
    if (near == monster->pos())
        return (false);

    if (!quiet)
        simple_monster_message(monster, " blinks!");

    if (!(monster->flags & MF_WAS_IN_VIEW))
        monster->seen_context = "thin air";

    const coord_def oldplace = monster->pos();
    if (!monster->move_to_pos(near))
        return (false);

    // Leave a purple cloud.
    place_cloud(CLOUD_TLOC_ENERGY, oldplace, 1 + random2(3),
                monster->kill_alignment());

    monster->check_redraw(oldplace);
    monster->apply_location_effects(oldplace);

    mons_relocated(monster);

    return (true);
}

bool mon_can_be_slimified(monsters *monster)
{
    const mon_holy_type holi = monster->holiness();

    return (holi == MH_UNDEAD
            || holi == MH_NATURAL
                && !mons_is_slime(monster));
}

void slimify_monster(monsters *mon, bool hostile)
{
    if (mon->holiness() == MH_UNDEAD)
        monster_polymorph(mon, MONS_DEATH_OOZE);
    else
    {
        const int x = mon->hit_dice + (coinflip() ? 1 : -1) * random2(5);

        if (x < 3)
            monster_polymorph(mon, MONS_OOZE);
        else if (x >= 3 && x < 5)
            monster_polymorph(mon, MONS_JELLY);
        else if (x >= 5 && x < 7)
            monster_polymorph(mon, MONS_BROWN_OOZE);
        else if (x >= 7 && x <= 11)
        {
            if (coinflip())
                monster_polymorph(mon, MONS_SLIME_CREATURE);
            else
                monster_polymorph(mon, MONS_GIANT_AMOEBA);
        }
        else
        {
            if (coinflip())
                monster_polymorph(mon, MONS_ACID_BLOB);
            else
                monster_polymorph(mon, MONS_AZURE_JELLY);
        }
    }

    if (!mons_eats_items(mon))
        mon->add_ench(ENCH_EAT_ITEMS);

    if (!hostile)
        mon->attitude = ATT_STRICT_NEUTRAL;
    else
        mon->attitude = ATT_HOSTILE;

    mons_make_god_gift(mon, GOD_JIYVA);
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

    if (monster_at(loc))
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
    if (is_feat_dangerous(grd(monster->pos())))
    {
        canned_msg(MSG_UNTHINKING_ACT);
        return (false);
    }

    if (monster->caught())
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

        for (adjacent_iterator ai(you.pos()); ai; ++ai)
            if (!monster_at(*ai) && _habitat_okay(monster, grd(*ai))
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

// Given an adjacent monster, returns true if the monster can hit it
// (the monster should not be submerged, be submerged in shallow water
// if the monster has a polearm, or be submerged in anything if the
// monster has tentacles).
bool monster_can_hit_monster(monsters *monster, const monsters *targ)
{
    if (!targ->submerged() || monster->has_damage_type(DVORP_TENTACLE))
        return (true);

    if (grd(targ->pos()) != DNGN_SHALLOW_WATER)
        return (false);

    const item_def *weapon = monster->weapon();
    return (weapon && weapon_skill(*weapon) == SK_POLEARMS);
}

void mons_get_damage_level(const monsters* monster, std::string& desc,
                           mon_dam_level_type& dam_level)
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
    else if (monster->hit_points <= monster->max_hit_points * 3 / 4)
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

std::string get_wounds_description(const monsters *monster)
{
    if (!monster->alive() || monster->hit_points == monster->max_hit_points)
        return "";

    if (monster_descriptor(monster->type, MDSC_NOMSG_WOUNDS))
        return "";

    std::string desc;
    mon_dam_level_type dam_level;
    mons_get_damage_level(monster, desc, dam_level);

    desc.insert(0, " is ");
    desc += ".";

    return desc;
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
static bool _wounded_damaged(monster_type mon_type)
{
    // this schema needs to be abstracted into real categories {dlb}:
    const mon_holy_type holi = mons_class_holiness(mon_type);

    return (holi == MH_UNDEAD || holi == MH_NONLIVING || holi == MH_PLANT);
}

// If _mons_find_level_exits() is ever expanded to handle more grid
// types, this should be expanded along with it.
static void _mons_indicate_level_exit(const monsters *mon)
{
    const dungeon_feature_type feat = grd(mon->pos());
    const bool is_shaft = (get_trap_type(mon->pos()) == TRAP_SHAFT);

    if (feat_is_gate(feat))
        simple_monster_message(mon, " passes through the gate.");
    else if (feat_is_travelable_stair(feat))
    {
        command_type dir = feat_stair_direction(feat);
        simple_monster_message(mon,
            make_stringf(" %s the %s.",
                dir == CMD_GO_UPSTAIRS     ? "goes up" :
                dir == CMD_GO_DOWNSTAIRS   ? "goes down"
                                           : "takes",
                feat_is_escape_hatch(feat) ? "escape hatch"
                                           : "stairs").c_str());
    }
    else if (is_shaft)
    {
        simple_monster_message(mon,
            make_stringf(" %s the shaft.",
                mons_flies(mon) ? "goes down"
                                : "jumps into").c_str());
    }
}

void make_mons_leave_level(monsters *mon)
{
    if (mon->pacified())
    {
        if (you.can_see(mon))
            _mons_indicate_level_exit(mon);

        // Pacified monsters leaving the level take their stuff with
        // them.
        mon->flags |= MF_HARD_RESET;
        monster_die(mon, KILL_DISMISSED, NON_MONSTER);
    }
}

// Checks whether there is a straight path from p1 to p2 that passes
// through features >= allowed.
// If it exists, such a path may be missed; on the other hand, it
// is not guaranteed that p2 is visible from p1 according to LOS rules.
// Not symmetric.
bool can_go_straight(const coord_def& p1, const coord_def& p2,
                     dungeon_feature_type allowed)
{
    if (distance(p1, p2) > get_los_radius_sq())
        return (false);

    dungeon_feature_type max_disallowed = DNGN_MAXOPAQUE;
    if (allowed != DNGN_UNSEEN)
        max_disallowed = static_cast<dungeon_feature_type>(allowed - 1);

    return (!num_feats_between(p1, p2, DNGN_UNSEEN, max_disallowed,
                               true, true));
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

    for (; ri; ++ri)
    {
        if (monsters *mon = monster_at(*ri))
        {
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

                if (prefer_named && mon->is_named())
                    mon_weight++;

                if (prefer_priest && mon->is_priest())
                    mon_weight++;

                if (x_chance_in_y(mon_weight, (weight += mon_weight)))
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
            || monster->visible_to(&you) || crawl_state.arena))
    {
        std::string msg = monster->name(descrip);
        msg += event;
        msg = apostrophise_fixup(msg);

        if (channel == MSGCH_PLAIN && monster->wont_attack())
            channel = MSGCH_FRIEND_ACTION;

        mpr(msg.c_str(), channel, param);
        return (true);
    }

    return (false);
}

bool mons_avoids_cloud(const monsters *monster, cloud_struct cloud,
                       bool placement)
{
    bool extra_careful = placement;
    cloud_type cl_type = cloud.type;

    if (placement)
        extra_careful = true;

    switch (cl_type)
    {
    case CLOUD_MIASMA:
        // Even the dumbest monsters will avoid miasma if they can.
        return (!monster->res_rotting());

    case CLOUD_FIRE:
    case CLOUD_FOREST_FIRE:
        if (monster->res_fire() > 1)
            return (false);

        if (extra_careful)
            return (true);

        if (mons_intel(monster) >= I_ANIMAL && monster->res_fire() < 0)
            return (true);

        if (monster->hit_points >= 15 + random2avg(46, 5))
            return (false);
        break;

    case CLOUD_STINK:
        if (monster->res_poison() > 0)
            return (false);

        if (extra_careful)
            return (true);

        if (mons_intel(monster) >= I_ANIMAL && monster->res_poison() < 0)
            return (true);

        if (x_chance_in_y(monster->hit_dice - 1, 5))
            return (false);

        if (monster->hit_points >= random2avg(19, 2))
            return (false);
        break;

    case CLOUD_COLD:
        if (monster->res_cold() > 1)
            return (false);

        if (extra_careful)
            return (true);

        if (mons_intel(monster) >= I_ANIMAL && monster->res_cold() < 0)
            return (true);

        if (monster->hit_points >= 15 + random2avg(46, 5))
            return (false);
        break;

    case CLOUD_POISON:
        if (monster->res_poison() > 0)
            return (false);

        if (extra_careful)
            return (true);

        if (mons_intel(monster) >= I_ANIMAL && monster->res_poison() < 0)
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

        if (monster->res_fire() > 0)
            return (false);

        if (monster->hit_points >= random2avg(19, 2))
            return (false);
        break;

    case CLOUD_RAIN:
        // Fiery monsters dislike the rain.
        if (monster->is_fiery() && extra_careful)
            return (true);

        // We don't care about what's underneath the rain cloud if we can fly.
        if (monster->flight_mode() != FL_NONE)
            return (false);

        // These don't care about deep water.
        if (monster_habitable_grid(monster, DNGN_DEEP_WATER))
            return (false);

        // This position could become deep water, and they might drown.
        if (grd(cloud.pos) == DNGN_SHALLOW_WATER)
            return (true);

        // Otherwise, it's safe for everyone else.
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

// Like the above, but allow a monster to move from one damaging cloud
// to another, even if they're of different types.
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

    // Is the target cloud okay?
    if (!mons_avoids_cloud(monster, cloud, placement))
        return (false);

    // If we're already in a cloud that we'd want to avoid then moving
    // from one to the other is okay.
    if (!in_bounds(monster->pos()) || monster->pos() == cloud.pos)
        return (true);

    const int our_cloud_num = env.cgrid(monster->pos());

    if (our_cloud_num == EMPTY_CLOUD)
        return (true);

    const cloud_struct &our_cloud = env.cloud[our_cloud_num];

    return (!mons_avoids_cloud(monster, our_cloud, true));
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

int mons_natural_regen_rate(monsters *monster)
{
    // A HD divider ranging from 3 (at 1 HD) to 1 (at 8 HD).
    int divider =
        std::max(div_rand_round(15 - monster->hit_dice, 4), 1);

    // The undead have a harder time regenerating.  Golems have it worse.
    switch (monster->holiness())
    {
    case MH_UNDEAD:
        divider *= (mons_enslaved_soul(monster)) ? 2 : 4;
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

        if (grid == DNGN_LAVA && monster->res_fire() >= 2)
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

bool monster_descriptor(int which_class, mon_desc_type which_descriptor)
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
        case MONS_PURGY:
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

        return (you.can_see(&menv[you.prev_targ]));
    }

    if (you.prev_targ != MHITNOT && you.prev_targ != MHITYOU)
    {
        const monsters *montarget = &menv[you.prev_targ];

        if (you.can_see(montarget))
        {
            mprf(MSGCH_PROMPT, "Current target: %s "
                 "(use p or f to fire at it again.)",
                 montarget->name(DESC_PLAIN).c_str());
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
}

void seen_monster(monsters *monster)
{
    // If the monster is in the auto_exclude list, automatically
    // set an exclusion.
    set_auto_exclude(monster);

    // Monster was viewed this turn
    monster->flags |= MF_WAS_IN_VIEW;

    if (monster->flags & MF_SEEN)
        return;

    // First time we've seen this particular monster.
    monster->flags |= MF_SEEN;

    if (!mons_is_mimic(monster->type))
    {
        if (Options.tutorial_left)
            tutorial_first_monster(*monster);

        if (MONST_INTERESTING(monster))
        {
            take_note(
                      Note(NOTE_SEEN_MONSTER, monster->type, 0,
                           monster->name(DESC_NOCAP_A, true).c_str()));
        }
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

        if (actor_at(*ai))
            continue;

        if (one_chance_in(++count))
            result = *ai;
    }

    if (count > 0)
    {
        mgrd(mon->pos()) = NON_MONSTER;
        mon->moveto(result);
        mgrd(result) = mon->mindex();
    }

    return (count > 0);
}

// Make all of the monster's original equipment disappear, unless it's a fixed
// artefact or unrand artefact.
static void _vanish_orig_eq(monsters* mons)
{
    for (int i = 0; i < NUM_MONSTER_SLOTS; ++i)
    {
        if (mons->inv[i] == NON_ITEM)
            continue;

        item_def &item(mitm[mons->inv[i]]);

        if (!item.is_valid())
            continue;

        if (item.orig_place != 0 || item.orig_monnum != 0
            || !item.inscription.empty()
            || is_unrandom_artefact(item)
            || (item.flags & (ISFLAG_DROPPED | ISFLAG_THROWN | ISFLAG_NOTED_GET
                              | ISFLAG_BEEN_IN_INV) ) )
        {
            continue;
        }
        item.flags |= ISFLAG_SUMMONED;
    }
}

int dismiss_monsters(std::string pattern) {
    // Make all of the monsters' original equipment disappear unless "keepitem"
    // is found in the regex (except for fixed arts and unrand arts).
    const bool keep_item = strip_tag(pattern, "keepitem");

    // Dismiss by regex
    text_pattern tpat(pattern);
    int ndismissed = 0;
    for (int mon = 0; mon < MAX_MONSTERS; mon++)
    {
        monsters *monster = &menv[mon];
        if (monster->alive() &&
            (tpat.empty() || tpat.matches(monster->name(DESC_PLAIN, true))))
        {
            if (!keep_item)
                _vanish_orig_eq(monster);
            monster_die(monster, KILL_DISMISSED, NON_MONSTER, false, true);
            ++ndismissed;
        }
    }
    return (ndismissed);
}

bool is_item_jelly_edible(const item_def &item)
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

bool monster_random_space(const monsters *monster, coord_def& target,
                          bool forbid_sanctuary)
{
    int tries = 0;
    while (tries++ < 1000)
    {
        target = random_in_bounds();

        // Don't land on top of another monster.
        if (actor_at(target))
            continue;

        if (is_sanctuary(target) && forbid_sanctuary)
            continue;

        if (monster_habitable_grid(monster, grd(target)))
            return (true);
    }

    return (false);
}

bool monster_random_space(monster_type mon, coord_def& target,
                          bool forbid_sanctuary)
{
    monsters dummy;
    dummy.type = mon;

    return monster_random_space(&dummy, target, forbid_sanctuary);
}

void monster_teleport(monsters *monster, bool instan, bool silent)
{
    if (!instan)
    {
        if (monster->del_ench(ENCH_TP))
        {
            if (!silent)
                simple_monster_message(monster, " seems more stable.");
        }
        else
        {
            if (!silent)
                simple_monster_message(monster, " looks slightly unstable.");

            monster->add_ench( mon_enchant(ENCH_TP, 0, KC_OTHER,
                                           random_range(20, 30)) );
        }

        return;
    }

    bool was_seen = you.can_see(monster) && !mons_is_lurking(monster);

    if (!silent)
        simple_monster_message(monster, " disappears!");

    const coord_def oldplace = monster->pos();

    // Pick the monster up.
    mgrd(oldplace) = NON_MONSTER;

    coord_def newpos;
    if (monster_random_space(monster, newpos, !monster->wont_attack()))
        monster->moveto(newpos);

    mgrd(monster->pos()) = monster_index(monster);

    // Mimics change form/colour when teleported.
    if (mons_is_mimic(monster->type))
    {
        monster_type old_type = monster->type;
        monster->type   = static_cast<monster_type>(
                                         MONS_GOLD_MIMIC + random2(5));
        monster->colour = get_mimic_colour(monster);

        // If it's changed form, you won't recognise it.
        // This assumes that a non-gold mimic turning into another item of
        // the same description is really, really unlikely.
        if (old_type != MONS_GOLD_MIMIC || monster->type != MONS_GOLD_MIMIC)
            was_seen = false;
    }

    const bool now_visible = mons_near(monster);
    if (!silent && now_visible)
    {
        if (was_seen)
            simple_monster_message(monster, " reappears nearby!");
        else
        {
            // Even if it doesn't interrupt an activity (the player isn't
            // delayed, the monster isn't hostile) we still want to give
            // a message.
            activity_interrupt_data ai(monster, "thin air");
            if (!interrupt_activity(AI_SEE_MONSTER, ai))
                simple_monster_message(monster, " appears out of thin air!");
        }
    }

    if (monster->visible_to(&you) && now_visible)
        handle_seen_interrupt(monster);

    // Leave a purple cloud.
    place_cloud(CLOUD_TLOC_ENERGY, oldplace, 1 + random2(3),
                monster->kill_alignment());

    monster->check_redraw(oldplace);
    monster->apply_location_effects(oldplace);

    mons_relocated(monster);

    // Teleporting mimics change form - if they reappear out of LOS, they are
    // no longer known.
    if (mons_is_mimic(monster->type))
    {
        if (now_visible)
            monster->flags |= MF_KNOWN_MIMIC;
        else
            monster->flags &= ~MF_KNOWN_MIMIC;
    }
}

void mons_clear_trapping_net(monsters *mon)
{
    if (!mon->caught())
        return;

    const int net = get_trapping_net(mon->pos());
    if (net != NON_ITEM)
        remove_item_stationary(mitm[net]);

    mon->del_ench(ENCH_HELD, true);
}

bool mons_clonable(const monsters* mon, bool needs_adjacent)
{
    // No uniques or ghost demon monsters.  Also, figuring out the name
    // for the clone of a named monster isn't worth it.
    if (mons_is_unique(mon->type)
        || mons_is_ghost_demon(mon->type)
        || mon->is_named())
    {
        return (false);
    }

    if (needs_adjacent)
    {
        // Is there space for the clone?
        bool square_found = false;
        for (int i = 0; i < 8; i++)
        {
            const coord_def p = mon->pos() + Compass[i];

            if (in_bounds(p)
                && !actor_at(p)
                && monster_habitable_grid(mon, grd(p)))
            {
                square_found = true;
                break;
            }
        }
        if (!square_found)
            return (false);
    }

    // Is the monster carrying an artefact?
    for (int i = 0; i < NUM_MONSTER_SLOTS; i++)
    {
        const int index = mon->inv[i];

        if (index == NON_ITEM)
            continue;

        if (is_artefact(mitm[index]))
            return (false);
    }

    return (true);
}

int clone_mons(const monsters* orig, bool quiet, bool* obvious,
               coord_def pos)
{
    // Is there an open slot in menv?
    int midx = NON_MONSTER;
    for (int i = 0; i < MAX_MONSTERS; i++)
        if (menv[i].type == MONS_NO_MONSTER)
        {
            midx = i;
            break;
        }

    if (midx == NON_MONSTER)
        return (NON_MONSTER);

    if (!in_bounds(pos))
    {
        // Find an adjacent square.
        int squares = 0;
        for (int i = 0; i < 8; i++)
        {
            const coord_def p = orig->pos() + Compass[i];

            if (in_bounds(p)
                && !actor_at(p)
                && monster_habitable_grid(orig, grd(p)))
            {
                if (one_chance_in(++squares))
                    pos = p;
            }
        }

        if (squares == 0)
            return (NON_MONSTER);
    }

    ASSERT( !actor_at(pos) );

    monsters &mon(menv[midx]);

    mon          = *orig;
    mon.set_position(pos);
    mgrd(pos)    = midx;

    // Duplicate objects, or unequip them if they can't be duplicated.
    for (int i = 0; i < NUM_MONSTER_SLOTS; i++)
    {
        const int old_index = orig->inv[i];

        if (old_index == NON_ITEM)
            continue;

        const int new_index = get_item_slot(0);
        if (new_index == NON_ITEM)
        {
            mon.unequip(mitm[old_index], i, 0, true);
            mon.inv[i] = NON_ITEM;
            continue;
        }

        mon.inv[i]      = new_index;
        mitm[new_index] = mitm[old_index];
        mitm[new_index].set_holding_monster(midx);
    }

    bool _obvious;
    if (obvious == NULL)
        obvious = &_obvious;
    *obvious = false;

    if (you.can_see(orig) && you.can_see(&mon))
    {
        if (!quiet)
            simple_monster_message(orig, " is duplicated!");
        *obvious = true;
    }

    mark_interesting_monst(&mon, mon.behaviour);
    if (you.can_see(&mon))
    {
        handle_seen_interrupt(&mon);
        viewwindow(false);
    }

    if (crawl_state.arena)
        arena_placed_monster(&mon);

    return (midx);
}

std::string summoned_poof_msg(const monsters* monster, bool plural)
{
    int  summon_type = 0;
    bool valid_mon   = false;
    if (monster != NULL && !invalid_monster(monster))
    {
        (void) monster->is_summoned(NULL, &summon_type);
        valid_mon = true;
    }

    std::string msg      = "disappear%s in a puff of smoke";
    bool        no_chaos = false;

    switch (summon_type)
    {
    case SPELL_SHADOW_CREATURES:
        msg      = "dissolve%s into shadows";
        no_chaos = true;
        break;

    case MON_SUMM_CHAOS:
        msg = "degenerate%s into a cloud of primal chaos";
        break;

    case MON_SUMM_WRATH:
    case MON_SUMM_AID:
        if (valid_mon && is_good_god(monster->god))
        {
            msg      = "dissolve%s into sparkling lights";
            no_chaos = true;
        }
        break;
    }

    if (valid_mon)
    {
        if (monster->god == GOD_XOM && !no_chaos && one_chance_in(10)
            || monster->type == MONS_CHAOS_SPAWN)
        {
            msg = "degenerate%s into a cloud of primal chaos";
        }

        if (monster->is_holy()
            && summon_type != SPELL_SHADOW_CREATURES
            && summon_type != MON_SUMM_CHAOS)
        {
            msg = "dissolve%s into sparkling lights";
        }
    }

    // Conjugate.
    msg = make_stringf(msg.c_str(), plural ? "" : "s");

    return (msg);
}

std::string summoned_poof_msg(const int midx, const item_def &item)
{
    if (midx == NON_MONSTER)
        return summoned_poof_msg(static_cast<const monsters*>(NULL), item);
    else
        return summoned_poof_msg(&menv[midx], item);
}

std::string summoned_poof_msg(const monsters* monster, const item_def &item)
{
    ASSERT(item.flags & ISFLAG_SUMMONED);

    return summoned_poof_msg(monster, item.quantity > 1);
}
