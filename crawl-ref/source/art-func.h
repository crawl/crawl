/**
 * @file
 * @brief Functions non-standard unrandarts uses.
**/

/*
 * util/art-data.pl scans through this file to grab the functions
 * non-standard unrandarts use and put them into the unranddata structs
 * in art-data.h, so the function names must have the form of
 * _UNRAND_ENUM_func_name() in order to be recognised.
 */

#ifdef ART_FUNC_H
#error "art-func.h included twice!"
#endif

#ifdef ART_DATA_H
#error "art-func.h must be included before art-data.h"
#endif

#define ART_FUNC_H

#include "cloud.h"         // For storm bow's and robe of clouds' rain
#include "effects.h"       // For Sceptre of Torment tormenting
#include "env.h"           // For storm bow env.cgrid
#include "fight.h"
#include "food.h"          // For evokes
#include "godconduct.h"    // did_god_conduct
#include "misc.h"
#include "mgen_data.h"     // For Sceptre of Asmodeus evoke
#include "mon-place.h"     // For Sceptre of Asmodeus evoke
#include "mon-stuff.h"     // For Scythe of Curses cursing items
#include "player.h"
#include "spl-cast.h"      // For evokes
#include "spl-damage.h"    // For the Singing Sword.
#include "spl-miscast.h"   // For Staff of Wucad Mu and Scythe of Curses miscasts
#include "spl-summoning.h" // For Zonguldrok animating dead
#include "terrain.h"       // For storm bow

/*******************
 * Helper functions.
 *******************/

static void _equip_mpr(bool* show_msgs, const char* msg,
                       msg_channel_type chan = MSGCH_PLAIN)
{
    bool def_show = true;

    if (show_msgs == NULL)
        show_msgs = &def_show;

    if (*show_msgs)
        mprf(chan, "%s", msg);

    // Caller shouldn't give any more messages.
    *show_msgs = false;
}

/*******************
 * Unrand functions.
 *******************/

static void _ASMODEUS_melee_effects(item_def* weapon, actor* attacker,
                                    actor* defender, bool mondied, int dam)
{
    if (attacker->is_player())
        did_god_conduct(DID_UNHOLY, 3);
}

static bool _evoke_sceptre_of_asmodeus()
{
    if (!x_chance_in_y(you.skill(SK_EVOCATIONS, 100), 3000))
        return false;

    const monster_type mon = random_choose_weighted(
                                   3, MONS_EFREET,
                                   3, MONS_SUN_DEMON,
                                   3, MONS_BALRUG,
                                   2, MONS_HELLION,
                                   1, MONS_BRIMSTONE_FIEND,
                                   0);

    mgen_data mg(mon, BEH_CHARMED, &you,
                 0, 0, you.pos(), MHITYOU,
                 MG_FORCE_BEH, you.religion);
    mg.extra_flags |= (MF_NO_REWARD | MF_HARD_RESET);

    monster *m = create_monster(mg);

    if (m)
    {
        mpr("The Sceptre summons one of its servants.");
        did_god_conduct(DID_UNHOLY, 3);

        m->add_ench(mon_enchant(ENCH_FAKE_ABJURATION, 6));

        if (!player_angers_monster(m))
            mpr("You don't feel so good about this...");
    }
    else
        mpr("The air shimmers briefly.");

    return true;
}

static bool _ASMODEUS_evoke(item_def *item, int* pract, bool* did_work,
                            bool* unevokable)
{
    if (_evoke_sceptre_of_asmodeus())
    {
        make_hungry(200, false, true);
        *did_work = true;
        *pract    = 1;
    }

    return false;
}

////////////////////////////////////////////////////
static void _CEREBOV_melee_effects(item_def* weapon, actor* attacker,
                                   actor* defender, bool mondied, int dam)
{
    if (attacker->is_player())
        did_god_conduct(DID_UNHOLY, 3);

    if (dam)
    {
        if (defender->is_player()
            && defender->res_fire() <= 3
            && !you.duration[DUR_FIRE_VULN])
        {
            mpr("The Sword of Cerebov burns away your fire resistance.");
            you.increase_duration(DUR_FIRE_VULN, 3 + random2(dam), 50);
        }
        if (defender->is_monster()
            && !mondied
            && !defender->as_monster()->res_hellfire()
            && !defender->as_monster()->has_ench(ENCH_FIRE_VULN))
        {
            mprf("The Sword of Cerebov burns away %s fire resistance.",
                 defender->name(DESC_ITS).c_str());
            defender->as_monster()->add_ench(
                mon_enchant(ENCH_FIRE_VULN, 1, attacker,
                            (3 + random2(dam)) * BASELINE_DELAY));
        }
    }
}

////////////////////////////////////////////////////
static void _curses_miscast(actor* victim, int power, int fail)
{
    MiscastEffect(victim, WIELD_MISCAST, SPTYP_NECROMANCY, power, fail,
                  "the Scythe of Curses", NH_NEVER);
}

static void _CURSES_equip(item_def *item, bool *show_msgs, bool unmeld)
{
    _equip_mpr(show_msgs, "A shiver runs down your spine.");
    if (!unmeld)
        _curses_miscast(&you, random2(9), random2(70));
}

static void _CURSES_world_reacts(item_def *item)
{
    if (one_chance_in(30))
        curse_an_item();
}

static void _CURSES_melee_effects(item_def* weapon, actor* attacker,
                                  actor* defender, bool mondied, int dam)
{
    if (attacker->is_player())
        did_god_conduct(DID_NECROMANCY, 3);
    if (!mondied && defender->has_lifeforce())
        _curses_miscast(defender, random2(9), random2(70));
}

/////////////////////////////////////////////////////

static void _DISPATER_melee_effects(item_def* weapon, actor* attacker,
                                    actor* defender, bool mondied, int dam)
{
    if (attacker->is_player())
        did_god_conduct(DID_UNHOLY, 3);
}

static bool _DISPATER_evoke(item_def *item, int* pract, bool* did_work,
                            bool* unevokable)
{
    if (!enough_hp(11, true))
    {
        mpr("You're too close to death to use this item.");
        *unevokable = true;
        return true;
    }

    if (!enough_mp(5, false))
    {
        *unevokable = true;
        return true;
    }

    *did_work = true;
    int power = you.skill(SK_EVOCATIONS, 8);

    if (your_spells(SPELL_HELLFIRE, power, false) == SPRET_ABORT)
    {
        *unevokable = true;
        return false;
    }

    mpr("You feel the staff feeding on your energy!");
    dec_hp(5 + random2avg(19, 2), false);
    dec_mp(2 + random2avg(5, 2));
    make_hungry(100, false, true);
    *pract    = (coinflip() ? 2 : 1);

    return false;
}

////////////////////////////////////////////////////

// XXX: Staff giving a boost to poison spells is hardcoded in
// player_spec_poison()

static void _olgreb_pluses(item_def *item)
{
    // Giving Olgreb's staff a little lift since staves of poison have
    // been made better. -- bwr
    item->plus  = you.skill(SK_POISON_MAGIC) / 3;
    item->plus2 = item->plus;
}

static void _OLGREB_equip(item_def *item, bool *show_msgs, bool unmeld)
{
    if (you.can_smell())
        _equip_mpr(show_msgs, "You smell chlorine.");
    else
        _equip_mpr(show_msgs, "The staff glows a sickly green.");

    _olgreb_pluses(item);
}

static void _OLGREB_unequip(item_def *item, bool *show_msgs)
{
    if (you.can_smell())
        _equip_mpr(show_msgs, "The smell of chlorine vanishes.");
    else
        _equip_mpr(show_msgs, "The staff's sickly green glow vanishes.");
}

static void _OLGREB_world_reacts(item_def *item)
{
    _olgreb_pluses(item);
}

static bool _OLGREB_evoke(item_def *item, int* pract, bool* did_work,
                          bool* unevokable)
{
    if (!enough_mp(4, false))
    {
        *unevokable = true;
        return true;
    }

    if (!x_chance_in_y(you.skill(SK_EVOCATIONS, 100) + 100, 600))
        return false;

    *did_work = true;

    int power = div_rand_round(20 + you.skill(SK_EVOCATIONS, 20), 4);

    // Allow aborting (for example if friendlies are nearby).
    if (your_spells(SPELL_OLGREBS_TOXIC_RADIANCE, power, false) == SPRET_ABORT)
    {
        *unevokable = true;
        return false;
    }

    if (x_chance_in_y(you.skill(SK_EVOCATIONS, 100) + 100, 2000))
        your_spells(SPELL_VENOM_BOLT, power, false);

    dec_mp(4);
    make_hungry(50, false, true);
    *pract    = 1;

    return false;
}

static void _OLGREB_melee_effects(item_def* weapon, actor* attacker,
                                  actor* defender, bool mondied, int dam)
{
    int skill = attacker->skill(SK_POISON_MAGIC, 100);
    if (defender->alive()
        && (coinflip() || x_chance_in_y(skill, 800)))
    {
        defender->poison(attacker, 2, defender->has_lifeforce()
                                      && x_chance_in_y(skill, 800));
        if (attacker->is_player())
            did_god_conduct(DID_POISON, 3);
    }
}

////////////////////////////////////////////////////

static void _power_pluses(item_def *item)
{
    item->plus  = min(you.hp / 10, 27);
    item->plus2 = item->plus;
}

static void _POWER_equip(item_def *item, bool *show_msgs, bool unmeld)
{
    _equip_mpr(show_msgs, "You sense an aura of extreme power.");
    _power_pluses(item);
}

static void _POWER_world_reacts(item_def *item)
{
    _power_pluses(item);
}

////////////////////////////////////////////////////

static void _SINGING_SWORD_equip(item_def *item, bool *show_msgs, bool unmeld)
{
    bool def_show = true;

    if (show_msgs == NULL)
        show_msgs = &def_show;

    if (!*show_msgs)
        return;

    if (!item->props.exists("ss_welcome"))
    {
        mprf(MSGCH_TALK, "The sword says, \"Hi! I'm the Singing Sword!\"");
        item->props["ss_welcome"].get_bool() = true;
    }
    else
        mprf(MSGCH_TALK, "The Singing Sword hums in delight!");

    *show_msgs = false;
}

static void _SINGING_SWORD_unequip(item_def *item, bool *show_msgs)
{
    set_artefact_name(*item, "Singing Sword");
    _equip_mpr(show_msgs, "The Singing Sword sighs.", MSGCH_TALK);
}

static void _SINGING_SWORD_world_reacts(item_def *item)
{
    int tension = get_tension(GOD_NO_GOD);
    int tier = (tension <= 0) ? 0 : (tension < 40) ? 1 : 2;
    bool silent = silenced(you.pos());

    string old_name = get_artefact_name(*item);
    string new_name;
    if (silent)
        new_name = "Sulking Sword";
    else if (tier < 2)
        new_name = "Singing Sword";
    else
        new_name = "Screaming Sword";
    if (old_name != new_name)
    {
        set_artefact_name(*item, new_name);
        you.wield_change = true;
    }

    // not as spammy at low tension
    if (!x_chance_in_y(7, (tier == 0) ? 1000 : (tier == 1) ? 100 : 10))
        return;

    // it will still struggle more with higher tension
    if (silent)
        tier = 0;

    if (tier == 2 && one_chance_in(10))
        tier++; // SCREAM -- double damage

    const char *tenname[] =  {"silenced", "no_tension", "low_tension",
                              "high_tension", "SCREAM"};
    const string key = tenname[tier];
    string msg = getSpeakString("singing sword " + key);

    msg = maybe_pick_random_substring(msg);
    msg = maybe_capitalise_substring(msg);

    const int loudness[] = {0, 15, 25, 35};
    item_noise(*item, msg, loudness[tier]);

    if (tier < 2)
        return; // no damage on low tiers

    sonic_damage(tier == 3);
}

////////////////////////////////////////////////////

static void _PRUNE_equip(item_def *item, bool *show_msgs, bool unmeld)
{
    _equip_mpr(show_msgs, "You feel pruney.");
}

static void _PRUNE_world_reacts(item_def *item)
{
    if (one_chance_in(10))
        did_god_conduct(DID_CHAOS, 1);
}

////////////////////////////////////////////////////

static void _TORMENT_equip(item_def *item, bool *show_msgs, bool unmeld)
{
    _equip_mpr(show_msgs, "A terribly searing pain shoots up your arm!");
}

static void _TORMENT_world_reacts(item_def *item)
{
    if (one_chance_in(200))
    {
        torment(&you, TORMENT_SPWLD, you.pos());
        did_god_conduct(DID_UNHOLY, 1);
    }
}

static void _TORMENT_melee_effects(item_def* weapon, actor* attacker,
                                   actor* defender, bool mondied, int dam)
{
    if (!coinflip())
        return;
    torment(attacker, TORMENT_SPWLD, attacker->pos());
    if (attacker->is_player())
        did_god_conduct(DID_UNHOLY, 5);
}

/////////////////////////////////////////////////////

static void _TROG_equip(item_def *item, bool *show_msgs, bool unmeld)
{
    _equip_mpr(show_msgs, "You feel bloodthirsty!");
}

static void _TROG_unequip(item_def *item, bool *show_msgs)
{
    _equip_mpr(show_msgs, "You feel less violent.");
}

////////////////////////////////////////////////////

static void _wucad_miscast(actor* victim, int power,int fail)
{
    MiscastEffect(victim, WIELD_MISCAST, SPTYP_DIVINATION, power, fail,
                  "the Staff of Wucad Mu", NH_NEVER);
}

static bool _WUCAD_MU_evoke(item_def *item, int* pract, bool* did_work,
                            bool* unevokable)
{
    if (you.species == SP_DJINNI)
    {
        mpr("The staff is unable to affect your essence.");
        *unevokable = true;
        return true;
    }

    if (you.magic_points == you.max_magic_points)
    {
        mpr("Your reserves of magic are full.");
        *unevokable = true;
        return true;
    }

    if (!x_chance_in_y(you.skill(SK_EVOCATIONS, 100) + 100, 2500))
        return false;

    if (one_chance_in(4))
    {
        _wucad_miscast(&you, random2(9), random2(70));
        return false;
    }

    mpr("Magical energy flows into your mind!");

    inc_mp(3 + random2(5) + you.skill_rdiv(SK_EVOCATIONS, 1, 3));
    make_hungry(50, false, true);

    *pract    = 1;
    *did_work = true;

    return false;
}

///////////////////////////////////////////////////

// XXX: Always getting maximal vampiric drain is hardcoded in
// melee_attack::apply_damage_brand()

static void _VAMPIRES_TOOTH_equip(item_def *item, bool *show_msgs, bool unmeld)
{
    if (you.is_undead == US_ALIVE)
    {
        _equip_mpr(show_msgs,
                   "You feel a strange hunger, and smell blood in the air...");
        if (!unmeld)
            make_hungry(4500, false, false);
    }
    else if (you.species == SP_VAMPIRE)
        _equip_mpr(show_msgs, "You feel a bloodthirsty glee!");
    else
        _equip_mpr(show_msgs, "You feel strangely empty.");
}

///////////////////////////////////////////////////

// XXX: Pluses at creation time are hardcoded in make_item_unrandart()

static void _VARIABILITY_world_reacts(item_def *item)
{
    do_uncurse_item(*item);

    if (x_chance_in_y(2, 5))
        item->plus  += (coinflip() ? +1 : -1);

    if (x_chance_in_y(2, 5))
        item->plus2 += (coinflip() ? +1 : -1);

    if (item->plus < -4)
        item->plus = -4;
    else if (item->plus > 16)
        item->plus = 16;

    if (item->plus2 < -4)
        item->plus2 = -4;
    else if (item->plus2 > 16)
        item->plus2 = 16;
}

///////////////////////////////////////////////////

static void _ZONGULDROK_equip(item_def *item, bool *show_msgs, bool unmeld)
{
    _equip_mpr(show_msgs, "You sense an extremely unholy aura.");
}

static void _ZONGULDROK_world_reacts(item_def *item)
{
    animate_dead(&you, 1 + random2(3), BEH_HOSTILE, MHITYOU, 0,
                 "the Sword of Zonguldrok");
    did_god_conduct(DID_NECROMANCY, 1);
    did_god_conduct(DID_CORPSE_VIOLATION, 1);
}

static void _ZONGULDROK_melee_effects(item_def* weapon, actor* attacker,
                                      actor* defender, bool mondied, int dam)
{
    if (attacker->is_player())
    {
        did_god_conduct(DID_NECROMANCY, 3);
        did_god_conduct(DID_CORPSE_VIOLATION, 3);
    }
}

///////////////////////////////////////////////////

static void _STORM_BOW_world_reacts(item_def *item)
{
    if (!one_chance_in(300))
        return;

    for (radius_iterator ri(you.pos(), 2, C_ROUND, LOS_SOLID); ri; ++ri)
        if (!cell_is_solid(*ri) && env.cgrid(*ri) == EMPTY_CLOUD && one_chance_in(5))
            place_cloud(CLOUD_RAIN, *ri, random2(20), &you, 3);
}

///////////////////////////////////////////////////

static void _GONG_melee_effects(item_def* item, actor* wearer,
                                actor* attacker, bool dummy, int dam)
{
    if (silenced(wearer->pos()))
        return;

    string msg = getSpeakString("shield of the gong");
    if (msg.empty())
        msg = "You hear a strange loud sound.";
    mprf(MSGCH_SOUND, "%s", msg.c_str());

    noisy(40, wearer->pos());
}

///////////////////////////////////////////////////

static void _RCLOUDS_world_reacts(item_def *item)
{
    cloud_type cloud;
    if (one_chance_in(4))
        cloud = CLOUD_RAIN;
    else
        cloud = CLOUD_MIST;

    for (radius_iterator ri(you.pos(), 2, C_ROUND, LOS_SOLID); ri; ++ri)
        if (!cell_is_solid(*ri) && env.cgrid(*ri) == EMPTY_CLOUD
                && one_chance_in(20))
        {
            place_cloud(cloud, *ri, random2(10), &you, 1);
        }
}

static void _RCLOUDS_equip(item_def *item, bool *show_msgs, bool unmeld)
{
    _equip_mpr(show_msgs, "A thin mist springs up around you!");
}

///////////////////////////////////////////////////

static void _DEMON_AXE_melee_effects(item_def* item, actor* attacker,
                                     actor* defender, bool mondied, int dam)
{
    if (one_chance_in(10))
        cast_summon_demon(50+random2(100));

    if (attacker->is_player())
        did_god_conduct(DID_UNHOLY, 3);
}

static void _DEMON_AXE_world_reacts(item_def *item)
{
    monster* closest = NULL;

    for (distance_iterator di(you.pos(), true, true, LOS_RADIUS); di; ++di)
    {
        monster *mon = monster_at(*di);
        if (mon && you.can_see(mon)
            && you.possible_beholder(mon)
            && !mons_class_flag(mon->type, M_NO_EXP_GAIN))
        {
            closest = mon;
            goto found;
        }
    }
    return;
found:

    if (!you.beheld_by(closest))
    {
        mprf("Visions of slaying %s flood into your mind.",
             closest->name(DESC_THE).c_str());

        // The monsters (if any) currently mesmerising the player do not include
        // this monster. To avoid trapping the player, all other beholders
        // are removed.

        you.clear_beholders();
    }

    if (you.confused())
    {
        mpr("Your confusion fades away as the thirst for blood takes over your mind.");
        you.duration[DUR_CONF] = 0;
    }

    you.add_beholder(closest, true);
}

static void _DEMON_AXE_unequip(item_def *item, bool *show_msgs)
{
    if (you.beheld())
    {
        // This shouldn't clear mermaids and sirens, but we lack the information
        // why they behold us -- usually, it's due to the axe.  Since unwielding
        // it costs scrolls of rem curse, we might say getting the demon away is
        // enough of a shock to get you back to senses.
        you.clear_beholders();
        mpr("Your thirst for blood fades away.");
    }
}

///////////////////////////////////////////////////

static void _WYRMBANE_equip(item_def *item, bool *show_msgs, bool unmeld)
{
    _equip_mpr(show_msgs, player_genus(GENPC_DRACONIAN) || you.form == TRAN_DRAGON
                            ? "You feel an overwhelming desire to commit suicide."
                            : "You feel an overwhelming desire to slay dragons!");
}

static void _WYRMBANE_melee_effects(item_def* weapon, actor* attacker,
                                    actor* defender, bool mondied, int dam)
{
    if (!mondied || !defender || !is_dragonkind(defender)
        || defender->is_summoned()
        || defender->is_monster()
           && testbits(defender->as_monster()->flags, MF_NO_REWARD))
    {
        return;
    }
    if (defender->is_player())
    {
        // can't currently happen even on a death blow
        mpr("<green>You see the lance glow as it kills you.</green>");
        return;
    }
    // The cap can be reached by:
    // * iron dragon, golden dragon, pearl dragon (18)
    // * Xtahua (19)
    // * bone dragon, Serpent of Hell (20)
    // * Tiamat (22)
    // * pghosts (up to 27)
    int hd = min(defender->as_monster()->hit_dice, 18);
    dprf("Killed a drac with hd %d.", hd);
    bool boosted = false;
    if (weapon->plus < hd)
        weapon->plus++, boosted = true;
    if (weapon->plus2 < hd)
        weapon->plus2++, boosted = true;
    if (boosted)
    {
        mprf("<green>The lance glows as it skewers %s.</green>",
              defender->name(DESC_THE).c_str());
        you.wield_change = true;
    }
}

///////////////////////////////////////////////////

static void _UNDEADHUNTER_melee_effects(item_def* item, actor* attacker,
                                        actor* defender, bool mondied, int dam)
{
    if (defender->holiness() == MH_UNDEAD && !one_chance_in(3) && !mondied)
    {
        mprf("%s %s blasted by disruptive energy!",
              defender->name(DESC_THE).c_str(),
              defender->is_player() ? "are" : "is");
        defender->hurt(attacker, random2avg((1 + (dam * 3)), 3));
    }
}

///////////////////////////////////////////////////
static void _BRILLIANCE_equip(item_def *item, bool *show_msgs, bool unmeld)
{
    invalidate_agrid(true);
}

static void _BRILLIANCE_unequip(item_def *item, bool *show_msgs)
{
    invalidate_agrid(true);
}

///////////////////////////////////////////////////
static void _DEVASTATOR_equip(item_def *item, bool *show_msgs, bool unmeld)
{
    _equip_mpr(show_msgs, "Time to lay down the shillelagh law.");
}

static void _DEVASTATOR_melee_effects(item_def* item, actor* attacker,
                                      actor* defender, bool mondied, int dam)
{
    if (dam)
        shillelagh(attacker, defender->pos(), dam);
}

///////////////////////////////////////////////////
static void _DRAGONSKIN_equip(item_def *item, bool *show_msgs, bool unmeld)
{
    _equip_mpr(show_msgs, "You feel oddly protected from the elements.");
}

static void _DRAGONSKIN_unequip(item_def *item, bool *show_msgs)
{
    _equip_mpr(show_msgs, "You no longer feel protected from the elements.");
}

///////////////////////////////////////////////////
static void _BLACK_KNIGHT_HORSE_world_reacts(item_def *item)
{
    if (one_chance_in(10))
        did_god_conduct(DID_UNHOLY, 1);
}

///////////////////////////////////////////////////
static void _NIGHT_equip(item_def *item, bool *show_msgs, bool unmeld)
{
    update_vision_range();
    _equip_mpr(show_msgs, "The light fades from your surroundings.");
}

static void _NIGHT_unequip(item_def *item, bool *show_msgs)
{
    update_vision_range();
    _equip_mpr(show_msgs, "The light returns to your surroundings.");
}

///////////////////////////////////////////////////

static void _plutonium_sword_miscast(actor* victim, int power, int fail)
{
    MiscastEffect(victim, MELEE_MISCAST, SPTYP_TRANSMUTATION, power, fail,
                  "the plutonium sword", NH_NEVER);
}

static void _PLUTONIUM_SWORD_melee_effects(item_def* weapon, actor* attacker,
                                           actor* defender, bool mondied,
                                           int dam)
{
    if (!mondied && one_chance_in(5))
    {
        mpr("Mutagenic energy flows through the plutonium sword!");
        _plutonium_sword_miscast(defender, random2(9), random2(70));

        if (attacker->is_player())
            did_god_conduct(DID_CHAOS, 3);
    }
}

///////////////////////////////////////////////////

static void _WOE_melee_effects(item_def* weapon, actor* attacker,
                               actor* defender, bool mondied, int dam)
{
    const char *verb = "bugger", *adv = "";
    switch (random2(8))
    {
    case 0: verb = "cleave", adv = " in twain"; break;
    case 1: verb = "pulverise", adv = " into thin bloody mist"; break;
    case 2: verb = "hew", adv = " savagely"; break;
    case 3: verb = "fatally mangle", adv = ""; break;
    case 4: verb = "dissect", adv = " like a pig carcass"; break;
    case 5: verb = "chop", adv = " into pieces"; break;
    case 6: verb = "butcher", adv = " messily"; break;
    case 7: verb = "slaughter", adv = " joyfully"; break;
    }
    if (you.see_cell(attacker->pos()) || you.see_cell(defender->pos()))
    {
        mprf("%s %s%s %s%s.", attacker->name(DESC_THE).c_str(), verb,
            attacker->is_player() ? "" : "s", defender->name(DESC_THE).c_str(),
            adv);
    }

    if (!mondied)
        defender->hurt(attacker, defender->stat_hp());
}

///////////////////////////////////////////////////

static void _SPIDER_equip(item_def *item, bool *show_msgs, bool unmeld)
{
    _equip_mpr(show_msgs, "The boots cling to your feet.");
    you.check_clinging(false);
}

static void _SPIDER_unequip(item_def *item, bool *show_msgs)
{
    you.check_clinging(false);
}

///////////////////////////////////////////////////

static void _HELLFIRE_equip(item_def *item, bool *show_msgs, bool unmeld)
{
    _equip_mpr(show_msgs, "Your hands smoulder for a moment.");
}

static setup_missile_type _HELLFIRE_launch(item_def* item, bolt* beam,
                                           string* ammo_name, bool* returning)
{
    ASSERT(beam->item
           && beam->item->base_type == OBJ_MISSILES
           && !is_artefact(*(beam->item)));
    beam->item->special = SPMSL_EXPLODING; // so that it mulches

    beam->flavour = BEAM_HELLFIRE;
    beam->name    = "hellfire bolt";
    *ammo_name    = "a hellfire bolt";
    beam->colour  = LIGHTRED;
    beam->glyph   = DCHAR_FIRED_ZAP;

    bolt *expl   = new bolt(*beam);
    expl->is_explosion = true;
    expl->damage = dice_def(2, 5);
    expl->name   = "hellfire";

    beam->special_explosion = expl;
    return SM_FINISHED;
}

///////////////////////////////////////////////////

static void _ELEMENTAL_STAFF_melee_effects(item_def* item, actor* attacker,
                                        actor* defender, bool mondied, int dam)
{
    int evoc = attacker->skill(SK_EVOCATIONS, 27);

    if (mondied || !(x_chance_in_y(evoc, 729) || x_chance_in_y(evoc, 729)))
        return;

    int d = 10 + random2(15);

    const char *verb = "hit";
    switch (random2(4))
    {
    case 0:
        d = resist_adjust_damage(defender, BEAM_FIRE,
                                 defender->res_fire(), d);
        verb = "burn";
        break;
    case 1:
        d = resist_adjust_damage(defender, BEAM_COLD,
                                 defender->res_cold(), d);
        verb = "freeze";
        break;
    case 2:
        d = resist_adjust_damage(defender, BEAM_ELECTRICITY,
                                 defender->res_elec(), d);
        verb = "electrocute";
        break;
    case 3:
        d = defender->apply_ac(d);
        verb = "crush";
        break;
    }

    if (!d)
        return;

    mprf("%s %s %s.",
         attacker->name(DESC_THE).c_str(),
         attacker->is_player() ? verb : pluralise(verb).c_str(),
         defender->name(DESC_THE).c_str());
    defender->hurt(attacker, d);
}
