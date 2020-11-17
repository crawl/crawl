/**
 * @file
 * @brief Self-enchantment spells.
**/

#include "AppHdr.h"

#include "spl-selfench.h"

#include <cmath>

#include "areas.h"
#include "art-enum.h"
#include "coordit.h" // radius_iterator
#include "god-conduct.h"
#include "god-passive.h"
#include "env.h"
#include "hints.h"
#include "item-prop.h"
#include "items.h" // stack_iterator
#include "libutil.h"
#include "macro.h"
#include "message.h"
#include "mutation.h"
#include "output.h"
#include "prompt.h"
#include "player-stats.h"
#include "religion.h"
#include "showsymb.h"
#include "spl-transloc.h"
#include "spl-util.h"
#include "skills.h"
#include "stringutil.h"
#include "terrain.h"
#include "transform.h"
#include "tiledoll.h"
#include "tilepick.h"
#include "view.h"
#include "viewchar.h"

spret cast_deaths_door(int pow, bool fail)
{
    fail_check();
    mpr("You stand defiantly in death's doorway!");
    mprf(MSGCH_SOUND, "You seem to hear sand running through an hourglass...");

    you.set_duration(DUR_DEATHS_DOOR, 10 + random2avg(13, 3)
                                       + (random2(pow) / 10));

    const int hp = max(calc_spell_power(SPELL_DEATHS_DOOR, true) / 10, 1);
    you.attribute[ATTR_DEATHS_DOOR_HP] = hp;
    set_hp(hp);

    if (you.duration[DUR_DEATHS_DOOR] > 25 * BASELINE_DELAY)
        you.duration[DUR_DEATHS_DOOR] = (23 + random2(5)) * BASELINE_DELAY;
    return spret::success;
}

void remove_ice_armour()
{
    mprf(MSGCH_DURATION, "Your icy armour melts away.");
    you.redraw_armour_class = true;
    you.duration[DUR_ICY_ARMOUR] = 0;
}

spret ice_armour(int pow, bool fail)
{
    fail_check();

    if (you.duration[DUR_ICY_ARMOUR])
        mpr("Your icy armour thickens.");
    else if (you.form == transformation::ice_beast)
        mpr("Your icy body feels more resilient.");
    else
        mpr("A film of ice covers your body!");

    you.increase_duration(DUR_ICY_ARMOUR, random_range(40, 50), 50);
    you.props[ICY_ARMOUR_KEY] = pow;
    you.redraw_armour_class = true;

    return spret::success;
}

void remove_missile_prot()
{
    mprf(MSGCH_DURATION, "Your repeling wind fades away.");
    you.attribute[ATTR_REPEL_MISSILES] = 0;
}

spret missile_prot(int, bool fail)
{
    fail_check();
    you.attribute[ATTR_REPEL_MISSILES] = 1;
    mpr("You feel protected from missiles.");
    return spret::success;
}

spret deflection(int /*pow*/, bool fail)
{
    fail_check();
    you.attribute[ATTR_DEFLECT_MISSILES] = 1;
    mpr("You feel very safe from missiles.");
	// Replace RMsl, if active.
    if (you.attribute[ATTR_REPEL_MISSILES])
        you.attribute[ATTR_REPEL_MISSILES] = 0;

    return spret::success;
}

spret cast_regen(int pow, bool fail)
{
    fail_check();
    you.increase_duration(DUR_REGENERATION, 5 + roll_dice(2, pow / 3 + 1), 100,
                          "Your skin crawls.");

    return spret::success;
}

spret cast_revivification(int pow, bool fail)
{
    fail_check();
    mpr("Your body is healed in an amazingly painful way.");

    const int loss = 6 + binomial(9, 8, pow);
    dec_max_hp(loss * you.hp_max / 100);
    set_hp(you.hp_max);

    if (you.duration[DUR_DEATHS_DOOR])
    {
        mprf(MSGCH_DURATION, "Your life is in your own hands once again.");
        // XXX: better cause name?
        paralyse_player("Death's Door abortion");
        you.duration[DUR_DEATHS_DOOR] = 0;
    }
    return spret::success;
}

spret cast_swiftness(int power, bool fail)
{
    fail_check();

    if (you.in_liquid())
    {
        // Hint that the player won't be faster until they leave the liquid.
        mprf("The %s foams!", you.in_water() ? "water"
                             : you.in_lava() ? "lava"
                                             : "liquid ground");
    }

    you.set_duration(DUR_SWIFTNESS, 12 + random2(power)/2, 30,
                     "You feel quick.");
    you.attribute[ATTR_SWIFTNESS] = you.duration[DUR_SWIFTNESS];

    return spret::success;
}

int cast_selective_amnesia(const string &pre_msg)
{
    if (you.spell_no == 0)
    {
        canned_msg(MSG_NO_SPELLS);
        return 0;
    }

    int keyin = 0;
    spell_type spell;
    int slot;

    // Pick a spell to forget.
    keyin = list_spells(false, false, false, "Forget which spell?");
    redraw_screen();

    if (isaalpha(keyin))
    {
        spell = get_spell_by_letter(keyin);
        slot = get_spell_slot_by_letter(keyin);

        if (spell != SPELL_NO_SPELL)
        {
            const string prompt = make_stringf(
                    "Forget %s, freeing %d spell level%s for a total of %d?%s",
                    spell_title(spell), spell_levels_required(spell),
                    spell_levels_required(spell) != 1 ? "s" : "",
                    player_spell_levels() + spell_levels_required(spell),
                    you.spell_library[spell] ? "" :
                    " This spell is not in your library!");

            if (yesno(prompt.c_str(), true, 'n', false))
            {
                if (!pre_msg.empty())
                    mpr(pre_msg);
                del_spell_from_memory_by_slot(slot);
                return 1;
            }
        }
    }

    return -1;
}

spret cast_infusion(int pow, bool fail)
{
    fail_check();
    if (!you.duration[DUR_INFUSION])
        mpr("You begin infusing your attacks with magical energy.");
    else
        mpr("You extend your infusion's duration.");

    you.increase_duration(DUR_INFUSION,  8 + roll_dice(2, pow), 100);
    you.props["infusion_power"] = pow;

    return spret::success;
}

spret cast_song_of_slaying(int pow, bool fail)
{
    fail_check();

    if (you.duration[DUR_SONG_OF_SLAYING])
        mpr("You start a new song!");
    else
        mpr("You start singing a song of slaying.");

    you.set_duration(DUR_SONG_OF_SLAYING, 20 + random2avg(pow, 2));

    you.props[SONG_OF_SLAYING_KEY] = 0;
    return spret::success;
}

spret cast_silence(int pow, bool fail)
{
    fail_check();
    mpr("A profound silence engulfs you.");

    you.increase_duration(DUR_SILENCE, 10 + pow/4 + random2avg(pow/2, 2), 100);
    invalidate_agrid(true);

    if (you.beheld())
        you.update_beholders();

    learned_something_new(HINT_YOU_SILENCE);
    return spret::success;
}

spret cast_liquefaction(int pow, bool fail)
{
    fail_check();
    flash_view_delay(UA_PLAYER, BROWN, 80);
    flash_view_delay(UA_PLAYER, YELLOW, 80);
    flash_view_delay(UA_PLAYER, BROWN, 140);

    mpr("The ground around you becomes liquefied!");

    you.increase_duration(DUR_LIQUEFYING, 10 + random2avg(pow, 2), 100);
    invalidate_agrid(true);
    return spret::success;
}

spret cast_shroud_of_golubria(int pow, bool fail)
{
    fail_check();
    if (you.duration[DUR_SHROUD_OF_GOLUBRIA])
        mpr("You renew your shroud.");
    else
        mpr("Space distorts slightly along a thin shroud covering your body.");

    you.increase_duration(DUR_SHROUD_OF_GOLUBRIA, 7 + roll_dice(2, pow), 50);
    return spret::success;
}

spret cast_transform(int pow, transformation which_trans, bool fail)
{   
    
    if (!transform(pow, which_trans, false, true)
        || !check_form_stat_safety(which_trans))
    {
        return spret::abort;
    }

    fail_check();
    transform(pow, which_trans);
    notify_stat_change();
    return spret::success;
}

spret cast_noxious_bog(int pow, bool fail)
{
    fail_check();
    flash_view_delay(UA_PLAYER, LIGHTGREEN, 100);

    if (!you.duration[DUR_NOXIOUS_BOG])
        mpr("You begin spewing toxic sludge!");
    else
        mpr("Your toxic spew intensifies!");

    you.props[NOXIOUS_BOG_KEY] = pow;
    you.increase_duration(DUR_NOXIOUS_BOG, 5 + random2(pow / 10), 24);
    return spret::success;
}

void noxious_bog_cell(coord_def p)
{
    if (grd(p) == DNGN_DEEP_WATER || grd(p) == DNGN_LAVA)
        return;

    const int turns = 10
                    + random2avg(you.props[NOXIOUS_BOG_KEY].get_int() / 20, 2);
    temp_change_terrain(p, DNGN_TOXIC_BOG, turns * BASELINE_DELAY,
            TERRAIN_CHANGE_BOG, you.as_monster());
}


spret cast_elemental_weapon(int pow, bool fail)
{
    item_def& weapon = *you.weapon();
    const brand_type orig_brand = get_weapon_brand(weapon);

    if (orig_brand != SPWPN_NORMAL)
    {
        mpr("This weapon is already enchanted.");
        return spret::abort;
    }

    fail_check();

    if (!you.duration[DUR_ELEMENTAL_WEAPON])
        mpr("You inject the force of elements into a weapon.");
    else
        mpr("You extend your elemental weapon's duration.");

    you.increase_duration(DUR_ELEMENTAL_WEAPON, 8 + roll_dice(2, pow), 100);
    return spret::success;
}

void end_elemental_weapon(item_def& weapon, bool verbose)
{
    ASSERT(you.duration[DUR_ELEMENTAL_WEAPON]);

    set_item_ego_type(weapon, OBJ_WEAPONS, SPWPN_NORMAL);
    you.duration[DUR_ELEMENTAL_WEAPON] = 0;
    you.props[ELEMENTAL_ENCHANT_KEY] = 0;

    if (verbose)
    {
        mprf(MSGCH_DURATION, "%s has lost its magical powers.",
            weapon.name(DESC_YOUR).c_str());
    }

    you.wield_change = true;
}
void enchant_elemental_weapon(item_def& weapon, spschools_type disciplines, bool verbose)
{
    ASSERT(you.duration[DUR_ELEMENTAL_WEAPON]);

    vector<spschool> schools;
    if (disciplines & spschool::fire) {
        schools.emplace_back(spschool::fire);
    }
    if (disciplines & spschool::ice) {
        schools.emplace_back(spschool::ice);
    }
    if (disciplines & spschool::air) {
        schools.emplace_back(spschool::air);
    }
    if (disciplines & spschool::earth) {
        schools.emplace_back(spschool::earth);
    }
    if (schools.size() == 0) {
        return;
    }
    spschool sel_school = schools[random2(schools.size())];
    you.wield_change = true;
    switch (sel_school) {
    case spschool::fire:
        if (verbose)
        {
            mprf("%s resonated with the elements of fire.", weapon.name(DESC_YOUR).c_str());
        }
        you.props[ELEMENTAL_ENCHANT_KEY] = you.skills[SK_FIRE_MAGIC] / 3;
        set_item_ego_type(weapon, OBJ_WEAPONS, SPWPN_FLAMING);
        break;
    case spschool::ice:
        if (verbose)
        {
            mprf("%s resonated with the elements of ice.", weapon.name(DESC_YOUR).c_str());
        }
        you.props[ELEMENTAL_ENCHANT_KEY] = you.skills[SK_ICE_MAGIC] / 3;
        set_item_ego_type(weapon, OBJ_WEAPONS, SPWPN_FREEZING);
        break;
    case spschool::air:
        if (verbose)
        {
            mprf("%s resonated with the elements of air.", weapon.name(DESC_YOUR).c_str());
        }
        you.props[ELEMENTAL_ENCHANT_KEY] = you.skills[SK_AIR_MAGIC] / 3;
        set_item_ego_type(weapon, OBJ_WEAPONS, SPWPN_ELECTROCUTION);
        break;
    case spschool::earth:
        if (verbose)
        {
            mprf("%s resonated with the elements of earth.", weapon.name(DESC_YOUR).c_str());
        }
        you.props[ELEMENTAL_ENCHANT_KEY] = you.skills[SK_EARTH_MAGIC] / 3;
        set_item_ego_type(weapon, OBJ_WEAPONS, SPWPN_VORPAL);
        break;
    default:
        break;
    }
}


spret cast_flame_strike(int pow, bool fail)
{
    if (you.duration[DUR_OVERHEAT]) {
        mpr("You're overheated.");
        return spret::abort;
    }
    fail_check();

    if (!you.duration[DUR_FLAME_STRIKE])
        mpr("Flames are enveloped in your attack.");
    else
        mpr("You extend your flame strike's duration.");

    if (you.species == SP_LAVA_ORC)
    {
        you.temperature = TEMP_MAX;
    }

    you.increase_duration(DUR_FLAME_STRIKE, 8 + roll_dice(2, pow), 100);
    you.props["flame_power"] = pow;

    return spret::success;
}

spret cast_insulation(int power, bool fail)
{
    fail_check();
    you.increase_duration(DUR_INSULATION, 10 + random2(power), 100,
                          "You feel insulated.");
    return spret::success;
}

spret change_lesser_lich(int , bool fail)
{
    ASSERT(you.species == SP_LESSER_LICH);

    string prompt = "Are you really going to be a permanent lich?";
    if (!yesno(prompt.c_str(), false, 'n'))
    {
        canned_msg(MSG_OK);
        return spret::abort;
    }
    fail_check();

    you.species = SP_LICH;

    uint8_t saved_skills[NUM_SKILLS];
    for (skill_type sk = SK_FIRST_SKILL; sk < NUM_SKILLS; ++sk)
    {
        saved_skills[sk] = you.skills[sk];
        check_skill_level_change(sk, false);
    }
    // The player symbol depends on species.
    update_player_symbol();
#ifdef USE_TILE
    init_player_doll();
#endif
    mprf(MSGCH_INTRINSIC_GAIN,
        "Now you are a complete lich!");

    for (skill_type sk = SK_FIRST_SKILL; sk < NUM_SKILLS; ++sk)
    {
        const int oldapt = species_apt(sk, SP_LESSER_LICH);
        const int newapt = species_apt(sk, you.species);
        if (oldapt != newapt)
        {
            mprf(MSGCH_INTRINSIC_GAIN, "You learn %s %s%s.",
                skill_name(sk),
                abs(oldapt - newapt) > 1 ? "much " : "",
                oldapt > newapt ? "slower" : "quicker");
        }

        you.skills[sk] = saved_skills[sk];
        check_skill_level_change(sk);
    }

    you.innate_mutation[MUT_STOCHASTIC_TORMENT_RESISTANCE]--;
    delete_mutation(MUT_STOCHASTIC_TORMENT_RESISTANCE, "necromutation", false, true, false, false);
    you.innate_mutation[MUT_NO_DEVICE_HEAL]--;
    delete_mutation(MUT_NO_DEVICE_HEAL, "necromutation", false, true, false, false);

    if (you.duration[DUR_REGENERATION])
    {
        mprf(MSGCH_DURATION, "You stop regenerating.");
        you.duration[DUR_REGENERATION] = 0;
    }

    you.hunger_state = HS_SATIATED;  // no hunger effects while transformed
    you.redraw_status_lights = true;
    give_level_mutations(you.species, 1);

    check_training_targets();

    gain_and_note_hp_mp();

    redraw_screen();

    return spret::success;
}

spret cast_shrapnel_curtain(int pow, bool fail)
{
    fail_check();
    if (!you.duration[DUR_SHRAPNEL])
        mpr("Spiny pebbles and gravels are ready to react.");
    else
        mpr("You reinforce your curtain of shrapnel.");

    you.increase_duration(DUR_SHRAPNEL, 8 + roll_dice(2, pow), 100);
    you.props["shrapnel_power"] = pow;

    return spret::success;
}

spret cast_barrier(int pow, bool fail)
{
    fail_check();
    if (you.duration[DUR_BARRIER] && you.attribute[ATTR_BARRIER] >= 0)
    {
        mpr("You've already activated your barrier.");
        return spret::abort;
    }
    if (you.duration[DUR_BARRIER_BROKEN])
    {
        mpr("You've still unable to barrier due to aftershock.");
        return spret::abort;
    }
    you.attribute[ATTR_BARRIER] = 1 + get_real_hp(true, false)/2;
    you.set_duration(DUR_BARRIER, 15 + random2avg(pow/2, 2) + (random2(pow) / 10));
    you.redraw_status_lights = true;
    flash_view_delay(UA_PLAYER, YELLOW, 300);
    mpr("Hermetic barrier starts to protect you from the outside!");
    return spret::success;
}
