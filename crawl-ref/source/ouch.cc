/**
 * @file
 * @brief Functions used when Bad Things happen to the player.
**/

#include "AppHdr.h"

#include "ouch.h"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#ifdef UNIX
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#include "artefact.h"
#include "art-enum.h"
#include "beam.h"
#include "chardump.h"
#include "colour.h"
#include "delay.h"
#include "describe.h"
#include "dgnevent.h"
#include "end.h"
#include "env.h"
#include "fight.h"
#include "files.h"
#include "fineff.h"
#include "godabil.h"
#include "godpassive.h"
#include "hints.h"
#include "hiscores.h"
#include "invent.h"
#include "itemname.h"
#include "itemprop.h"
#include "items.h"
#include "libutil.h"
#include "macro.h"
#include "message.h"
#include "mgen_data.h"
#include "misc.h"
#include "mon-death.h"
#include "mon-place.h"
#include "mon-util.h"
#include "mutation.h"
#include "notes.h"
#include "options.h"
#include "output.h"
#include "player.h"
#include "player-stats.h"
#include "potion.h"
#include "prompt.h"
#include "random.h"
#include "religion.h"
#include "shopping.h"
#include "shout.h"
#include "skills.h"
#include "spl-clouds.h"
#include "spl-other.h"
#include "spl-selfench.h"
#include "state.h"
#include "stringutil.h"
#include "transform.h"
#include "tutorial.h"
#include "view.h"
#include "xom.h"

void maybe_melt_player_enchantments(beam_type flavour, int damage)
{
    if (flavour == BEAM_FIRE || flavour == BEAM_LAVA
        || flavour == BEAM_HELLFIRE || flavour == BEAM_STICKY_FLAME
        || flavour == BEAM_STEAM)
    {
        if (you.mutation[MUT_ICEMAIL])
        {
            if (!you.duration[DUR_ICEMAIL_DEPLETED])
                mprf(MSGCH_DURATION, "Your icy envelope dissipates!");
            you.duration[DUR_ICEMAIL_DEPLETED] = ICEMAIL_TIME;
            you.redraw_armour_class = true;
        }

        if (you.duration[DUR_ICY_ARMOUR] > 0)
        {
            you.duration[DUR_ICY_ARMOUR] -= damage * BASELINE_DELAY;
            if (you.duration[DUR_ICY_ARMOUR] <= 0)
                remove_ice_armour();
            else
                you.props[MELT_ARMOUR_KEY] = true;
        }
    }
}

// NOTE: DOES NOT check for hellfire!!!
int check_your_resists(int hurted, beam_type flavour, string source,
                       bolt *beam, bool doEffects)
{
    int original = hurted;

    dprf("checking resistance: flavour=%d", flavour);

    string kaux = "";
    if (beam)
    {
        source = beam->get_source_name();
        kaux = beam->name;
    }

    if (doEffects)
        maybe_melt_player_enchantments(flavour, hurted);

    switch (flavour)
    {
    case BEAM_WATER:
        hurted = resist_adjust_damage(&you, flavour, hurted);
        if (!hurted && doEffects)
            mpr("You shrug off the wave.");
        else if (hurted > original && doEffects)
        {
            mpr("The water douses you terribly!");
            xom_is_stimulated(200);
        }
        break;

    case BEAM_STEAM:
        hurted = resist_adjust_damage(&you, flavour, hurted);
        if (hurted < original && doEffects)
            canned_msg(MSG_YOU_RESIST);
        else if (hurted > original && doEffects)
        {
            mpr("The steam scalds you terribly!");
            xom_is_stimulated(200);
        }
        break;

    case BEAM_FIRE:
        hurted = resist_adjust_damage(&you, flavour, hurted);
        if (hurted < original && doEffects)
            canned_msg(MSG_YOU_RESIST);
        else if (hurted > original && doEffects)
        {
            mpr("The fire burns you terribly!");
            xom_is_stimulated(200);
        }
        break;

    case BEAM_HELLFIRE:
#if TAG_MAJOR_VERSION == 34
        if (you.species == SP_DJINNI)
        {
            hurted = 0;
            if (doEffects)
                mpr("You resist completely.");
        }
#endif
        break;

    case BEAM_COLD:
        hurted = resist_adjust_damage(&you, flavour, hurted);
        if (hurted < original && doEffects)
            canned_msg(MSG_YOU_RESIST);
        else if (hurted > original && doEffects)
        {
            mpr("You feel a terrible chill!");
            xom_is_stimulated(200);
        }
        break;

    case BEAM_ELECTRICITY:
        hurted = resist_adjust_damage(&you, flavour, hurted);

        if (hurted < original && doEffects)
            canned_msg(MSG_YOU_RESIST);
        else if (hurted > original && doEffects)
        {
            mpr("You are shocked senseless!");
            xom_is_stimulated(200);
        }
        break;

    case BEAM_POISON:
        hurted = resist_adjust_damage(&you, flavour, hurted);

        if (doEffects)
        {
            // Ensure that we received a valid beam object before proceeding.
            // See also melee_attack.cc:_print_resist_messages() which cannot be
            // used with this beam type (as it does not provide a valid beam).
            ASSERT(beam);
            int pois = div_rand_round(beam->damage.num * beam->damage.size, 3);
            pois = 3 + random_range(pois * 2 / 3, pois * 4 / 3);
            poison_player(pois, source, kaux);

            if (player_res_poison() > 0)
                canned_msg(MSG_YOU_RESIST);
        }

        break;

    case BEAM_POISON_ARROW:
        if (doEffects)
        {
            // Ensure that we received a valid beam object before proceeding.
            // See also melee_attack.cc:_print_resist_messages() which cannot be
            // used with this beam type (as it does not provide a valid beam).
            ASSERT(beam);
            int pois = div_rand_round(beam->damage.num * beam->damage.size, 3);
            pois = 3 + random_range(pois * 2 / 3, pois * 4 / 3);

            const int resist = player_res_poison();
            poison_player((resist ? pois / 2 : pois), source, kaux, true);
        }

        hurted = resist_adjust_damage(&you, flavour, hurted);
        if (hurted < original && doEffects)
            canned_msg(MSG_YOU_PARTIALLY_RESIST);
        break;

    case BEAM_NEG:
        hurted = resist_adjust_damage(&you, flavour, hurted);

        if (doEffects)
        {
            // drain_player handles the messaging here
            if (hurted > original)
            {
                mpr("The negative energy saps you greatly!");
                xom_is_stimulated(200);
            }
            drain_player(min(75, 35 + original * 2 / 3), true);
        }
        break;

    case BEAM_ICE:
        hurted = resist_adjust_damage(&you, flavour, hurted);

        if (hurted < original && doEffects)
            canned_msg(MSG_YOU_PARTIALLY_RESIST);
        else if (hurted > original && doEffects)
        {
            mpr("You feel a painful chill!");
            xom_is_stimulated(200);
        }
        break;

    case BEAM_LAVA:
        hurted = resist_adjust_damage(&you, flavour, hurted);

        if (hurted < original && doEffects)
            canned_msg(MSG_YOU_PARTIALLY_RESIST);
        else if (hurted > original && doEffects)
        {
            mpr("The lava burns you terribly!");
            xom_is_stimulated(200);
        }
        break;

    case BEAM_ACID:
        hurted = resist_adjust_damage(&you, flavour, hurted);
        if (hurted < original && doEffects)
            canned_msg(MSG_YOU_RESIST);
        break;

    case BEAM_MIASMA:
        if (you.res_rotting())
        {
            if (doEffects)
                canned_msg(MSG_YOU_RESIST);
            hurted = 0;
        }
        break;

    case BEAM_HOLY:
    {
        // Cleansing flame.
        const int rhe = you.res_holy_energy(nullptr);
        if (rhe > 0)
            hurted = 0;
        else if (rhe == 0)
            hurted /= 2;
        else if (rhe < -1)
            hurted = hurted * 3 / 2;

        if (hurted == 0 && doEffects)
            canned_msg(MSG_YOU_RESIST);
        break;
    }

    case BEAM_AIR:
    {
        // Airstrike.
        if (you.res_wind())
            hurted = 0;
        else if (you.flight_mode())
            hurted += hurted / 2;
        break;
    }

    case BEAM_GHOSTLY_FLAME:
    {
        if (you.holiness() == MH_UNDEAD)
        {
            if (doEffects && hurted > 0)
            {
                you.heal(roll_dice(2, 9));
                mpr("You are bolstered by the flame.");
            }
            hurted = 0;
        }
        else
        {
            hurted = resist_adjust_damage(&you, flavour, hurted);
            if (hurted < original && doEffects)
                canned_msg(MSG_YOU_PARTIALLY_RESIST);
            else if (hurted > original && doEffects)
            {
                mpr("The flames sap you greatly!");
                xom_is_stimulated(200);
            }
        }
    }

    default:
        break;
    }                           // end switch

    return hurted;
}

/**
 * Handle side-effects for exposure to element other than damage.
 *
 * @param flavour The beam type.
 * @param strength The strength, which is interpreted as a number of player turns.
 * @param slow_cold_blooded If True, the beam_type is BEAM_COLD, and the player
 *                          is cold-blooded and not cold-resistant, slow the
 *                          player 50% of the time.
 */
void expose_player_to_element(beam_type flavour, int strength, bool slow_cold_blooded)
{
    maybe_melt_player_enchantments(flavour, strength ? strength : 10);
    qazlal_element_adapt(flavour, strength);

    if (flavour == BEAM_COLD && slow_cold_blooded
        && player_mutation_level(MUT_COLD_BLOODED)
        && you.res_cold() <= 0 && coinflip())
    {
        you.slow_down(0, strength);
    }

    if (flavour == BEAM_WATER && you.duration[DUR_LIQUID_FLAMES])
    {
        mprf(MSGCH_WARN, "The flames go out!");
        you.duration[DUR_LIQUID_FLAMES] = 0;
        you.props.erase("sticky_flame_source");
        you.props.erase("sticky_flame_aux");
    }
}

static void _lose_level_abilities()
{
    if (you.attribute[ATTR_PERM_FLIGHT]
        && !you.racial_permanent_flight()
        && !you.wearing_ego(EQ_ALL_ARMOUR, SPARM_FLYING))
    {
        you.increase_duration(DUR_FLIGHT, 50, 100);
        you.attribute[ATTR_PERM_FLIGHT] = 0;
        mprf(MSGCH_WARN, "You feel your flight won't last long.");
    }
}

void lose_level()
{
    // Because you.experience is unsigned long, if it's going to be
    // negative, must die straightaway.
    if (you.experience_level == 1)
    {
        ouch(INSTANT_DEATH, KILLED_BY_DRAINING);
        // Return in case death was cancelled via wizard mode
        return;
    }

    you.experience_level--;

    mprf(MSGCH_WARN,
         "You are now level %d!", you.experience_level);

    calc_hp();
    calc_mp();
    _lose_level_abilities();

    char buf[200];
    sprintf(buf, "HP: %d/%d MP: %d/%d",
            you.hp, you.hp_max, you.magic_points, you.max_magic_points);
    take_note(Note(NOTE_XP_LEVEL_CHANGE, you.experience_level, 0, buf));

    you.redraw_title = true;
    you.redraw_experience = true;
#ifdef USE_TILE_LOCAL
    // In case of intrinsic ability changes.
    tiles.layout_statcol();
    redraw_screen();
#endif

    xom_is_stimulated(200);

    // Kill the player if maxhp <= 0.  We can't just move the ouch() call past
    // dec_max_hp() since it would decrease hp twice, so here's another one.
    ouch(0, KILLED_BY_DRAINING);
}

/**
 * Drain the player.
 *
 * @param power             The amount by which to drain the player.
 * @param announce_full     Whether to print messages even when fully resisting
 *                          the drain.
 * @param ignore_protection Whether to ignore the player's rN.
 * @return                  Whether draining occurred.
 */
bool drain_player(int power, bool announce_full, bool ignore_protection)
{
    const int protection = player_prot_life();

    if (protection == 3 && !ignore_protection)
    {
        if (announce_full)
            canned_msg(MSG_YOU_RESIST);

        return false;
    }

    if (protection > 0 && !ignore_protection)
    {
        canned_msg(MSG_YOU_PARTIALLY_RESIST);
        power /= (protection * 2);
    }

    if (power > 0)
    {
        mpr("You feel drained.");
        xom_is_stimulated(15);

        you.attribute[ATTR_XP_DRAIN] += power;
        // Losing skills may affect AC/EV.
        you.redraw_armour_class = true;
        you.redraw_evasion = true;

        dprf("Drained by %d points (%d total)", power, you.attribute[ATTR_XP_DRAIN]);

        return true;
    }

    return false;
}

static void _xom_checks_damage(kill_method_type death_type,
                               int dam, mid_t death_source)
{
    if (you_worship(GOD_XOM))
    {
        if (death_type == KILLED_BY_TARGETING
            || death_type == KILLED_BY_BOUNCE
            || death_type == KILLED_BY_REFLECTION
            || death_type == KILLED_BY_SELF_AIMED
               && player_in_a_dangerous_place())
        {
            // Xom thinks the player accidentally hurting him/herself is funny.
            // Deliberate damage is only amusing if it's dangerous.
            int amusement = 200 * dam / (dam + you.hp);
            if (death_type == KILLED_BY_SELF_AIMED)
                amusement /= 5;
            xom_is_stimulated(amusement);
            return;
        }
        else if (death_type == KILLED_BY_FALLING_DOWN_STAIRS
                 || death_type == KILLED_BY_FALLING_THROUGH_GATE)
        {
            // Xom thinks falling down the stairs is hilarious.
            xom_is_stimulated(200);
            return;
        }
        else if (death_type == KILLED_BY_DISINT)
        {
            // flying chunks...
            xom_is_stimulated(100);
            return;
        }
        else if (death_type != KILLED_BY_MONSTER
                    && death_type != KILLED_BY_BEAM
                    && death_type != KILLED_BY_DISINT
                 || !monster_by_mid(death_source))
        {
            return;
        }

        int amusementvalue = 1;
        const monster* mons = monster_by_mid(death_source);

        if (!mons->alive())
            return;

        if (mons->wont_attack())
        {
            // Xom thinks collateral damage is funny.
            xom_is_stimulated(200 * dam / (dam + you.hp));
            return;
        }

        int leveldif = mons->get_experience_level() - you.experience_level;
        if (leveldif == 0)
            leveldif = 1;

        // Note that Xom is amused when you are significantly hurt by a
        // creature of higher level than yourself, as well as by a
        // creature of lower level than yourself.
        amusementvalue += leveldif * leveldif * dam;

        if (!mons->visible_to(&you))
            amusementvalue += 8;

        if (mons->speed < 100/player_movement_speed())
            amusementvalue += 7;

        if (death_type != KILLED_BY_BEAM
            && you.skill(SK_THROWING) <= (you.experience_level / 4))
        {
            amusementvalue += 2;
        }
        else if (you.skill(SK_FIGHTING) <= (you.experience_level / 4))
            amusementvalue += 2;

        if (player_in_a_dangerous_place())
            amusementvalue += 2;

        amusementvalue /= (you.hp > 0) ? you.hp : 1;

        xom_is_stimulated(amusementvalue);
    }
}

static void _yred_mirrors_injury(int dam, mid_t death_source)
{
    if (yred_injury_mirror())
    {
        // Cap damage to what was enough to kill you.  Can matter if
        // Yred saves your life or you have an extra kitty.
        if (you.hp < 0)
            dam += you.hp;

        monster* mons = monster_by_mid(death_source);
        if (dam <= 0 || !mons)
            return;

        mirror_damage_fineff::schedule(mons, &you, dam);
    }
}

static void _maybe_ru_retribution(int dam, mid_t death_source)
{
    if (will_ru_retaliate())
    {
        // Cap damage to what was enough to kill you.  Can matter if
        // you have an extra kitty.
        if (you.hp < 0)
            dam += you.hp;

        monster* mons = monster_by_mid(death_source);
        if (dam <= 0 || !mons)
            return;

        ru_retribution_fineff::schedule(mons, &you, dam);
    }
}

static void _maybe_spawn_monsters(int dam, const char* aux,
                                  kill_method_type death_type,
                                  mid_t death_source)
{
    monster* damager = monster_by_mid(death_source);
    // We need to exclude acid damage and similar things or this function
    // will crash later.
    if (!damager)
        return;

    // Exclude torment damage.
    if (aux && strstr(aux, "torment"))
        return;

    monster_type mon;
    int how_many = 0;

    if (you_worship(GOD_JIYVA)
        && you.piety >= piety_breakpoint(5))
    {
        mon = royal_jelly_ejectable_monster();
        if (dam >= you.hp_max * 3 / 4)
            how_many = random2(4) + 2;
        else if (dam >= you.hp_max / 2)
            how_many = random2(2) + 2;
        else if (dam >= you.hp_max / 4)
            how_many = 1;
    }
    else if (you_worship(GOD_XOM)
             && dam >= you.hp_max / 4
             && x_chance_in_y(dam, 3 * you.hp_max))
    {
        mon = MONS_BUTTERFLY;
        how_many = 2 + random2(5);
    }

    if (how_many > 0)
    {
        int count_created = 0;
        for (int i = 0; i < how_many; ++i)
        {
            mgen_data mg(mon, BEH_FRIENDLY, &you, 2, 0, you.pos(),
                         damager->mindex(), 0, you.religion);

            if (create_monster(mg))
                count_created++;
        }

        if (count_created > 0)
        {
            if (mon == MONS_BUTTERFLY)
            {
                mprf(MSGCH_GOD, "A shower of butterflies erupts from you!");
                take_note(Note(NOTE_XOM_EFFECT, you.piety, -1, "butterfly on damage"), true);
            }
            else
            {
                mprf("You shudder from the %s and a %s!",
                     death_type == KILLED_BY_MONSTER ? "blow" : "blast",
                     count_created > 1 ? "flood of jellies pours out from you"
                                       : "jelly pops out");
            }
        }
    }
}

static void _powered_by_pain(int dam)
{
    const int level = player_mutation_level(MUT_POWERED_BY_PAIN);

    if (you.mutation[MUT_POWERED_BY_PAIN]
        && (random2(dam) > 2 + 3 * level
            || dam >= you.hp_max / 2))
    {
        switch (random2(4))
        {
        case 0:
        case 1:
        {
            if (you.magic_points < you.max_magic_points)
            {
                mpr("You focus on the pain.");
                int mp = roll_dice(3, 2 + 3 * level);
                mpr("You feel your power returning.");
                inc_mp(mp);
                break;
            }
            break;
        }
        case 2:
            mpr("You focus on the pain.");
            potionlike_effect(POT_MIGHT, level * 20);
            break;
        case 3:
            mpr("You focus on the pain.");
            potionlike_effect(POT_AGILITY, level * 20);
            break;
        }
    }
}

static void _maybe_fog(int dam)
{
    const int upper_threshold = you.hp_max / 2;
    const int lower_threshold = upper_threshold
                                - upper_threshold
                                  * (you.piety - piety_breakpoint(2))
                                  / (MAX_PIETY - piety_breakpoint(2));
    if (you_worship(GOD_DITHMENOS)
        && you.piety >= piety_breakpoint(2)
        && (dam > 0 && you.form == TRAN_SHADOW
            || dam >= lower_threshold
               && x_chance_in_y(dam - lower_threshold,
                                upper_threshold - lower_threshold)))
    {
        mpr("You emit a cloud of dark smoke.");
        big_cloud(CLOUD_BLACK_SMOKE, &you, you.pos(), 50, 4 + random2(5));
    }
    else if (you_worship(GOD_XOM) && x_chance_in_y(dam, 30 * upper_threshold))
    {
        mprf(MSGCH_GOD, "You emit a cloud of colourful smoke!");
        big_cloud(CLOUD_MAGIC_TRAIL, &you, you.pos(), 50, 4 + random2(5),
                  -1, ETC_RANDOM);
        take_note(Note(NOTE_XOM_EFFECT, you.piety, -1, "smoke on damage"), true);
    }
}

static void _place_player_corpse(bool explode)
{
    if (!in_bounds(you.pos()))
        return;

    item_def corpse;
    if (fill_out_corpse(0, player_mons(), corpse) == MONS_NO_MONSTER)
        return;

    if (in_good_standing(GOD_GOZAG))
        goldify_corpse(corpse);

    if (explode && explode_corpse(corpse, you.pos()))
        return;

    int o = get_mitm_slot();
    if (o == NON_ITEM)
    {
        item_was_destroyed(corpse);
        return;
    }

    corpse.props[MONSTER_HIT_DICE].get_short() = you.experience_level;
    corpse.props[CORPSE_NAME_KEY] = you.your_name;
    corpse.props[CORPSE_NAME_TYPE_KEY].get_int64() = 0;
    mitm[o] = corpse;

    move_item_to_grid(&o, you.pos(), !you.in_water());
}

#if defined(WIZARD) || defined(DEBUG)
static void _wizard_restore_life()
{
    if (you.hp_max <= 0)
        unrot_hp(9999);
    while (you.hp_max <= 0)
        you.hp_max_adj_perm++, calc_hp();
    if (you.hp <= 0)
        set_hp(you.hp_max);
}
#endif

void reset_damage_counters()
{
    you.turn_damage = 0;
    you.damage_source = NON_MONSTER;
    you.source_damage = 0;
}

bool can_shave_damage()
{
    return you.species == SP_DEEP_DWARF || you.duration[DUR_FORTITUDE];
}

int do_shave_damage(int dam)
{
    if (you.species == SP_DEEP_DWARF)
    {
        // Deep Dwarves get to shave any hp loss.
        int shave = 1 + random2(2 + random2(1 + you.experience_level / 3));
        dprf("HP shaved: %d.", shave);
        dam -= shave;
    }

    if (you.duration[DUR_FORTITUDE])
        dam -= random2(10);

    return dam;
}

// Determine what's threatening for purposes of sacrifice drink and reading.
// the statuses are guaranteed not to happen if the incoming damage is less
// than 4% max hp. Otherwise, they scale up with damage taken and with lower
// health, becoming certain at 20% max health damage or <30% max health
// current hp.
static bool _is_damage_threatening (int damage_fraction_of_hp)
{
    int hp_fraction = you.hp * 100 / you.hp_max;
    return damage_fraction_of_hp > 5
            && hp_fraction <= 85
            && (damage_fraction_of_hp + random2(20) >= 20
                || random2(100) < hp_fraction);
}

/** Hurt the player. Isn't it fun?
 *
 *  @param dam How much damage -- may be INSTANT_DEATH.
 *  @param death_type how did you get hurt?
 *  @param source who could do such a thing?
 *  @param aux what did they do it with?
 *  @param see_source whether the attacker was visible to you
 *  @param death_source_name the attacker's name if it is already dead.
 */
void ouch(int dam, kill_method_type death_type, mid_t source, const char *aux,
          bool see_source, const char *death_source_name)
{
    ASSERT(!crawl_state.game_is_arena());
    if (you.duration[DUR_TIME_STEP])
        return;

    if (you.dead) // ... but eligible for revival
        return;

    int drain_amount = 0;

    if (can_shave_damage() && dam != INSTANT_DEATH
        && death_type != KILLED_BY_POISON)
    {
        dam = max(0, do_shave_damage(dam));
    }

    if (dam != INSTANT_DEATH)
    {
        if (you.form == TRAN_SHADOW)
        {
            drain_amount = (dam - (dam / 2));
            dam /= 2;
        }
        if (you.petrified())
            dam /= 2;
        else if (you.petrifying())
            dam = dam * 10 / 15;
    }
    ait_hp_loss hpl(dam, death_type);
    interrupt_activity(AI_HP_LOSS, &hpl);

    if (dam > 0 && death_type != KILLED_BY_POISON)
    {
        you.check_awaken(500);

        int damage_fraction_of_hp = dam * 100 / you.hp_max;

        // Check _is_damage_threatening separately for read and drink so they
        // don't always trigger in unison when you have both.
        if (player_mutation_level(MUT_NO_READ))
        {
            if (_is_damage_threatening(damage_fraction_of_hp))
            {
                if (!you.duration[DUR_NO_SCROLLS])
                    mpr("You feel threatened and lose the ability to read scrolls!");

                you.increase_duration(DUR_NO_SCROLLS, 1 + random2(dam), 30);
            }
        }

        if (player_mutation_level(MUT_NO_DRINK))
        {
            if (_is_damage_threatening(damage_fraction_of_hp))
            {
                if (!you.duration[DUR_NO_POTIONS])
                    mpr("You feel threatened and lose the ability to drink potions!");

                you.increase_duration(DUR_NO_POTIONS, 1 + random2(dam), 30);
            }
        }
    }

    const bool non_death = death_type == KILLED_BY_QUITTING
                        || death_type == KILLED_BY_WINNING
                        || death_type == KILLED_BY_LEAVING;

    // certain effects (e.g. drowned souls) use KILLED_BY_WATER for flavour
    // reasons (morgue messages?), with regrettable consequences if we don't
    // double-check.
    const bool env_death = source == MID_NOBODY
                           && (death_type == KILLED_BY_LAVA
                               || death_type == KILLED_BY_WATER);

    // death's door protects against everything but falling into water/lava,
    // excessive rot, leaving the dungeon, or quitting.
    if (you.duration[DUR_DEATHS_DOOR] && !env_death && !non_death
        && you.hp_max > 0)
    {
        return;
    }

    if (dam != INSTANT_DEATH)
    {
        you.maybe_degrade_bone_armour(BONE_ARMOUR_HIT_RATIO);

        if (you.spirit_shield() && death_type != KILLED_BY_POISON
            && !(aux && strstr(aux, "flay_damage")))
        {
            // round off fairly (important for taking 1 damage at a time)
            int mp = div_rand_round(dam * you.magic_points,
                                    max(you.hp + you.magic_points, 1));
            // but don't kill the player with round-off errors
            mp = max(mp, dam + 1 - you.hp);
            mp = min(mp, you.magic_points);

            dam -= mp;
            dec_mp(mp);
            if (dam <= 0 && you.hp > 0)
                return;
        }

        if (dam >= you.hp && you.hp_max > 0 && god_protects_from_harm())
        {
            simple_god_message(" protects you from harm!");
            return;
        }

        you.turn_damage += dam;
        if (you.damage_source != source)
        {
            you.damage_source = source;
            you.source_damage = 0;
        }
        you.source_damage += dam;

        dec_hp(dam, true);

        // Even if we have low HP messages off, we'll still give a
        // big hit warning (in this case, a hit for half our HPs) -- bwr
        if (dam > 0 && you.hp_max <= dam * 2)
            mprf(MSGCH_DANGER, "Ouch! That really hurt!");

        if (you.hp > 0 && dam > 0)
        {
            if (Options.hp_warning
                && you.hp <= (you.hp_max * Options.hp_warning) / 100
                && (death_type != KILLED_BY_POISON || poison_is_lethal()))
            {
                flash_view_delay(UA_HP, RED, 50);
                mprf(MSGCH_DANGER, "* * * LOW HITPOINT WARNING * * *");
                dungeon_events.fire_event(DET_HP_WARNING);
            }

            hints_healing_check();

            _xom_checks_damage(death_type, dam, source);

            // for note taking
            string damage_desc;
            if (!see_source)
                damage_desc = make_stringf("something (%d)", dam);
            else
            {
                damage_desc = scorefile_entry(dam, source,
                                              death_type, aux, true)
                    .death_description(scorefile_entry::DDV_TERSE);
            }

            take_note(Note(NOTE_HP_CHANGE, you.hp, you.hp_max,
                           damage_desc.c_str()));

            _yred_mirrors_injury(dam, source);
            _maybe_ru_retribution(dam, source);
            _maybe_spawn_monsters(dam, aux, death_type, source);
            _maybe_fog(dam);
            _powered_by_pain(dam);
            if (drain_amount > 0)
                drain_player(drain_amount, true, true);
        }
        if (you.hp > 0)
          return;
    }

    // Is the player being killed by a direct act of Xom?
    if (crawl_state.is_god_acting()
        && crawl_state.which_god_acting() == GOD_XOM
        && crawl_state.other_gods_acting().empty())
    {
        you.escaped_death_cause = death_type;
        you.escaped_death_aux   = aux == nullptr ? "" : aux;

        // Xom should only kill his worshippers if they're under penance
        // or Xom is bored.
        if (you_worship(GOD_XOM) && !you.penance[GOD_XOM]
            && you.gift_timeout > 0)
        {
            return;
        }

        // Also don't kill wizards testing Xom acts.
        if ((crawl_state.repeat_cmd == CMD_WIZARD
                || crawl_state.prev_cmd == CMD_WIZARD)
            && !you_worship(GOD_XOM))
        {
            return;
        }

        // Okay, you *didn't* escape death.
        you.reset_escaped_death();

        // Ensure some minimal information about Xom's involvement.
        if (aux == nullptr || !*aux)
        {
            if (death_type != KILLED_BY_XOM)
                aux = "Xom";
        }
        else if (strstr(aux, "Xom") == nullptr)
            death_type = KILLED_BY_XOM;
    }
    // Xom may still try to save your life.
    else if (xom_saves_your_life(death_type, aux))
        return;

#if defined(WIZARD) || defined(DEBUG)
    if (!non_death && crawl_state.disables[DIS_DEATH])
    {
        _wizard_restore_life();
        return;
    }
#endif

    crawl_state.cancel_cmd_all();

    // Construct scorefile entry.
    scorefile_entry se(dam, source, death_type, aux, false,
                       death_source_name);

#ifdef WIZARD
    if (!non_death)
    {
        if (crawl_state.test || you.wizard || (you.explore && !you.lives))
        {
            const string death_desc
                = se.death_description(scorefile_entry::DDV_VERBOSE);

            dprf("Damage: %d; Hit points: %d", dam, you.hp);

            if (crawl_state.test || !yesno("Die?", false, 'n'))
            {
                mpr("Thought so.");
                take_note(Note(NOTE_DEATH, you.hp, you.hp_max,
                                death_desc.c_str()), true);
                _wizard_restore_life();
                return;
            }
        }
    }
#endif  // WIZARD

    if (crawl_state.game_is_tutorial())
    {
        crawl_state.need_save = false;
        if (!non_death)
            tutorial_death_message();

        screen_end_game("");
    }

    // Okay, so you're dead.
    take_note(Note(NOTE_DEATH, you.hp, you.hp_max,
                    se.death_description(scorefile_entry::DDV_NORMAL).c_str()),
              true);
    if (you.lives && !non_death)
    {
        mark_milestone("death", lowercase_first(se.long_kill_message()).c_str());

        you.deaths++;
        you.lives--;
        you.dead = true;

        stop_delay(true);

        // You wouldn't want to lose this accomplishment to a crash, would you?
        // Especially if you manage to trigger one via lua somehow...
        if (!crawl_state.disables[DIS_SAVE_CHECKPOINTS])
            save_game(false);

        canned_msg(MSG_YOU_DIE);
        xom_death_message((kill_method_type) se.get_death_type());
        more();

        _place_player_corpse(death_type == KILLED_BY_DISINT);
        return;
    }

    // The game's over.
    crawl_state.need_save       = false;
    crawl_state.updating_scores = true;

    // Prevent bogus notes.
    activate_notes(false);

#ifndef SCORE_WIZARD_CHARACTERS
    if (!you.wizard && !you.explore)
#endif
    {
        // Add this highscore to the score file.
        hiscores_new_entry(se);
        logfile_new_entry(se);
    }

    // Never generate bones files of wizard or tutorial characters -- bwr
    if (!non_death && !crawl_state.game_is_tutorial() && !you.wizard)
        save_ghost();

    end_game(se);
}

string morgue_name(string char_name, time_t when_crawl_got_even)
{
    string name = "morgue-" + char_name;

    string time = make_file_time(when_crawl_got_even);
    if (!time.empty())
        name += "-" + time;

    return name;
}

int actor_to_death_source(const actor* agent)
{
    return agent ? agent->mindex() : NON_MONSTER;
}

int timescale_damage(const actor *act, int damage)
{
    if (damage < 0)
        damage = 0;
    // Can we have a uniform player/monster speed system yet?
    if (act->is_player())
        return div_rand_round(damage * you.time_taken, BASELINE_DELAY);
    else
    {
        const monster *mons = act->as_monster();
        const int speed = mons->speed > 0? mons->speed : BASELINE_DELAY;
        return div_rand_round(damage * BASELINE_DELAY, speed);
    }
}
