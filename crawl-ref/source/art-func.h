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

#include "beam.h"          // For Lajatang of Order's silver damage
#include "cloud.h"         // For storm bow's and robe of clouds' rain
#include "english.h"       // For apostrophise
#include "exercise.h"      // For practise_evoking
#include "fight.h"
#include "food.h"          // For evokes
#include "ghost.h"         // For is_dragonkind ghost_demon datas
#include "godconduct.h"    // did_god_conduct
#include "godpassive.h"    // passive_t::want_curses
#include "mgen_data.h"     // For Sceptre of Asmodeus evoke
#include "mon-death.h"     // For demon axe's SAME_ATTITUDE
#include "mon-place.h"     // For Sceptre of Asmodeus evoke
#include "player.h"
#include "spl-cast.h"      // For evokes
#include "spl-damage.h"    // For the Singing Sword.
#include "spl-goditem.h"   // For Sceptre of Torment tormenting
#include "spl-miscast.h"   // For Staff of Wucad Mu and Scythe of Curses miscasts
#include "spl-summoning.h" // For Zonguldrok animating dead
#include "terrain.h"       // For storm bow
#include "view.h"          // For arc blade's discharge effect

// prop recording whether the singing sword has said hello yet
#define SS_WELCOME_KEY "ss_welcome"
// similarly, for the majin-bo
#define MB_WELCOME_KEY "mb_welcome"

/*******************
 * Helper functions.
 *******************/

static void _equip_mpr(bool* show_msgs, const char* msg,
                       msg_channel_type chan = MSGCH_PLAIN)
{
    bool do_show = true;
    if (show_msgs == nullptr)
        show_msgs = &do_show;

    if (*show_msgs)
        mprf(chan, "%s", msg);

    // Caller shouldn't give any more messages.
    *show_msgs = false;
}

/*******************
 * Unrand functions.
 *******************/

static bool _evoke_sceptre_of_asmodeus()
{
    if (!x_chance_in_y(you.skill(SK_EVOCATIONS, 100), 3000))
        return false;

    const monster_type mon = random_choose_weighted(
                                   3, MONS_EFREET,
                                   3, MONS_SUN_DEMON,
                                   3, MONS_BALRUG,
                                   2, MONS_HELLION,
                                   1, MONS_BRIMSTONE_FIEND);

    mgen_data mg(mon, BEH_CHARMED, you.pos(), MHITYOU,
                 MG_FORCE_BEH, you.religion);
    mg.set_summoned(&you, 0, 0);
    mg.extra_flags |= (MF_NO_REWARD | MF_HARD_RESET);

    monster *m = create_monster(mg);

    if (m)
    {
        mpr("The sceptre summons one of its servants.");
        did_god_conduct(DID_EVIL, 3);

        m->add_ench(mon_enchant(ENCH_FAKE_ABJURATION, 6));

        if (!player_angers_monster(m))
            mpr("You don't feel so good about this...");
    }
    else
        mpr("The air shimmers briefly.");

    return true;
}

static bool _ASMODEUS_evoke(item_def *item, bool* did_work, bool* unevokable)
{
    if (_evoke_sceptre_of_asmodeus())
    {
        make_hungry(200, false, true);
        *did_work = true;
        practise_evoking(1);
    }

    return false;
}

////////////////////////////////////////////////////
static void _CEREBOV_melee_effects(item_def* weapon, actor* attacker,
                                   actor* defender, bool mondied, int dam)
{
    if (dam)
    {
        if (defender->is_player()
            && defender->res_fire() <= 3
            && !you.duration[DUR_FIRE_VULN])
        {
            mpr("The sword of Cerebov burns away your fire resistance.");
            you.increase_duration(DUR_FIRE_VULN, 3 + random2(dam), 50);
        }
        if (defender->is_monster()
            && !mondied
            && !defender->as_monster()->res_damnation() // XXX: ???
            && !defender->as_monster()->has_ench(ENCH_FIRE_VULN))
        {
            if (you.can_see(*attacker))
            {
                mprf("The sword of Cerebov burns away %s fire resistance.",
                     defender->name(DESC_ITS).c_str());
            }
            defender->as_monster()->add_ench(
                mon_enchant(ENCH_FIRE_VULN, 1, attacker,
                            (3 + random2(dam)) * BASELINE_DELAY));
        }
    }
}

////////////////////////////////////////////////////

static void _CURSES_equip(item_def *item, bool *show_msgs, bool unmeld)
{
    _equip_mpr(show_msgs, "A shiver runs down your spine.");
    if (!unmeld)
    {
        MiscastEffect(&you, nullptr, WIELD_MISCAST, SPTYP_NECROMANCY, random2(9),
                      random2(70), "the scythe of Curses", NH_NEVER);
    }
}

static void _CURSES_world_reacts(item_def *item)
{
    // don't spam messages for ash worshippers
    if (one_chance_in(30) && !have_passive(passive_t::want_curses))
        curse_an_item(true);
}

static void _CURSES_melee_effects(item_def* weapon, actor* attacker,
                                  actor* defender, bool mondied, int dam)
{
    if (attacker->is_player())
        did_god_conduct(DID_EVIL, 3);
    if (!mondied && defender->holiness() == MH_NATURAL)
    {
        MiscastEffect(defender, attacker, MELEE_MISCAST, SPTYP_NECROMANCY,
                      random2(9), random2(70), "the scythe of Curses",
                      NH_NEVER);
    }
}

/////////////////////////////////////////////////////

static bool _DISPATER_evoke(item_def *item, bool* did_work, bool* unevokable)
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

    if (your_spells(SPELL_HURL_DAMNATION, power, false, false, true)
        == SPRET_ABORT)
    {
        *unevokable = true;
        return false;
    }

    mpr("You feel the staff feeding on your energy!");
    dec_hp(5 + random2avg(19, 2), false);
    dec_mp(2 + random2avg(5, 2));
    make_hungry(100, false, true);
    practise_evoking(coinflip() ? 2 : 1);

    return false;
}

////////////////////////////////////////////////////

// XXX: Staff giving a boost to poison spells is hardcoded in
// player_spec_poison()

static void _OLGREB_equip(item_def *item, bool *show_msgs, bool unmeld)
{
    if (you.can_smell())
        _equip_mpr(show_msgs, "You smell chlorine.");
    else
        _equip_mpr(show_msgs, "The staff glows a sickly green.");
}

static void _OLGREB_unequip(item_def *item, bool *show_msgs)
{
    if (you.can_smell())
        _equip_mpr(show_msgs, "The smell of chlorine vanishes.");
    else
        _equip_mpr(show_msgs, "The staff's sickly green glow vanishes.");
}

static bool _OLGREB_evoke(item_def *item, bool* did_work, bool* unevokable)
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
    if (your_spells(SPELL_OLGREBS_TOXIC_RADIANCE, power,
                    false, false, true) == SPRET_ABORT)
    {
        *unevokable = true;
        return false;
    }

    if (x_chance_in_y(you.skill(SK_EVOCATIONS, 100) + 100, 2000))
        your_spells(SPELL_VENOM_BOLT, power, false, false, true);

    dec_mp(4);
    make_hungry(50, false, true);
    practise_evoking(1);

    return false;
}

static void _OLGREB_melee_effects(item_def* weapon, actor* attacker,
                                  actor* defender, bool mondied, int dam)
{
    if (defender->alive())
        defender->poison(attacker, 2);
}

////////////////////////////////////////////////////

static void _power_pluses(item_def *item)
{
    item->plus  = min(you.hp / 10, 27);
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

    if (show_msgs == nullptr)
        show_msgs = &def_show;

    if (!*show_msgs)
        return;

    if (!item->props.exists(SS_WELCOME_KEY))
    {
        mprf(MSGCH_TALK, "The sword says, \"Hi! I'm the Singing Sword!\"");
        item->props[SS_WELCOME_KEY].get_bool() = true;
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
    int tier = (tension <= 0) ? 1 : (tension < 40) ? 2 : 3;
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
    if (!x_chance_in_y(7, (tier == 1) ? 1000 : (tier == 2) ? 100 : 10))
        return;

    // it will still struggle more with higher tension
    if (silent)
        tier = 0;

    if (tier == 3 && one_chance_in(10))
        tier++; // SCREAM -- double damage

    const char *tenname[] =  {"silenced", "no_tension", "low_tension",
                              "high_tension", "SCREAM"};
    const string key = tenname[tier];
    string msg = getSpeakString("singing sword " + key);

    const int loudness[] = {0, 0, 15, 25, 35};
    item_noise(*item, msg, loudness[tier]);

    if (tier < 3)
        return; // no damage on low tiers

    sonic_damage(tier == 4);
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
        torment(&you, TORMENT_SCEPTRE, you.pos());
        did_god_conduct(DID_EVIL, 1);
    }
}

static void _TORMENT_melee_effects(item_def* weapon, actor* attacker,
                                   actor* defender, bool mondied, int dam)
{
    if (coinflip())
        torment(attacker, TORMENT_SCEPTRE, attacker->pos());
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
    MiscastEffect(victim, nullptr, WIELD_MISCAST, SPTYP_DIVINATION, power, fail,
                  "the staff of Wucad Mu", NH_NEVER);
}

static bool _WUCAD_MU_evoke(item_def *item, bool* did_work, bool* unevokable)
{
#if TAG_MAJOR_VERSION == 34
    if (you.species == SP_DJINNI)
    {
        mpr("The staff is unable to affect your essence.");
        *unevokable = true;
        return true;
    }

#endif
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
        did_god_conduct(DID_CHANNEL, 10, true);
        return false;
    }

    mpr("Magical energy flows into your mind!");

    inc_mp(3 + random2(5) + you.skill_rdiv(SK_EVOCATIONS, 1, 3));
    make_hungry(50, false, true);

    *did_work = true;
    practise_evoking(1);
    did_god_conduct(DID_CHANNEL, 10, true);

    return false;
}

///////////////////////////////////////////////////

// XXX: Always getting maximal vampiric drain is hardcoded in
// attack::apply_damage_brand()

static void _VAMPIRES_TOOTH_equip(item_def *item, bool *show_msgs, bool unmeld)
{
    if (you.undead_state() == US_ALIVE && !you_foodless())
    {
        _equip_mpr(show_msgs,
                   "You feel a strange hunger, and smell blood in the air...");
    }
    else if (you.species != SP_VAMPIRE)
        _equip_mpr(show_msgs, "You feel strangely empty.");
    // else let player-equip.cc handle message
}

///////////////////////////////////////////////////

// XXX: Pluses at creation time are hardcoded in make_item_unrandart()

static void _VARIABILITY_world_reacts(item_def *item)
{
    if (x_chance_in_y(2, 5))
        item->plus += (coinflip() ? +1 : -1);

    if (item->plus < -4)
        item->plus = -4;
    else if (item->plus > 16)
        item->plus = 16;
}

///////////////////////////////////////////////////

static void _ZONGULDROK_equip(item_def *item, bool *show_msgs, bool unmeld)
{
    _equip_mpr(show_msgs, "You sense an extremely unholy aura.");
}

static void _ZONGULDROK_melee_effects(item_def* weapon, actor* attacker,
                                      actor* defender, bool mondied, int dam)
{
    if (attacker->is_player())
    {
        did_god_conduct(DID_EVIL, 3);
        did_god_conduct(DID_CORPSE_VIOLATION, 3);
    }
}

///////////////////////////////////////////////////

static void _STORM_BOW_world_reacts(item_def *item)
{
    if (!one_chance_in(300))
        return;

    for (radius_iterator ri(you.pos(), 2, C_SQUARE, LOS_SOLID); ri; ++ri)
        if (!cell_is_solid(*ri) && !cloud_at(*ri) && one_chance_in(5))
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

    for (radius_iterator ri(you.pos(), 2, C_SQUARE, LOS_SOLID); ri; ++ri)
        if (!cell_is_solid(*ri) && !cloud_at(*ri) && one_chance_in(20))
            place_cloud(cloud, *ri, random2(10), &you, 1);
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
    {
        if (monster* mons = attacker->as_monster())
        {
            create_monster(
                mgen_data(summon_any_demon(RANDOM_DEMON_COMMON),
                          SAME_ATTITUDE(mons), mons->pos(), mons->foe)
                .set_summoned(mons, 6, SPELL_SUMMON_DEMON));
        }
        else
            cast_summon_demon(50+random2(100));
    }
}

static monster* _find_nearest_possible_beholder()
{
    for (distance_iterator di(you.pos(), true, true, LOS_RADIUS); di; ++di)
    {
        monster *mon = monster_at(*di);
        if (mon && you.can_see(*mon)
            && you.possible_beholder(mon)
            && mons_is_threatening(*mon))
        {
            return mon;
        }
    }

    return nullptr;
}

static void _DEMON_AXE_world_reacts(item_def *item)
{

    monster* mon = _find_nearest_possible_beholder();

    if (!mon)
        return;

    monster& closest = *mon;

    if (!you.beheld_by(closest))
    {
        mprf("Visions of slaying %s flood into your mind.",
             closest.name(DESC_THE).c_str());

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
        // This shouldn't clear sirens and merfolk avatars, but we lack the
        // information why they behold us -- usually, it's due to the axe.
        // Since unwielding it costs scrolls of rem curse, we might say getting
        // the demon away is enough of a shock to get you back to senses.
        you.clear_beholders();
        mpr("Your thirst for blood fades away.");
    }
}

///////////////////////////////////////////////////

static void _WYRMBANE_equip(item_def *item, bool *show_msgs, bool unmeld)
{
    _equip_mpr(show_msgs, species_is_draconian(you.species) || you.form == TRAN_DRAGON
                            ? "You feel an overwhelming desire to commit suicide."
                            : "You feel an overwhelming desire to slay dragons!");
}

static bool is_dragonkind(const actor *act)
{
    if (mons_genus(act->mons_species()) == MONS_DRAGON
        || mons_genus(act->mons_species()) == MONS_DRAKE
        || mons_genus(act->mons_species()) == MONS_DRACONIAN)
    {
        return true;
    }

    if (act->is_player())
        return you.form == TRAN_DRAGON;

    // Else the actor is a monster.
    const monster* mon = act->as_monster();

    if (mons_is_zombified(*mon)
        && (mons_genus(mon->base_monster) == MONS_DRAGON
            || mons_genus(mon->base_monster) == MONS_DRAKE
            || mons_genus(mon->base_monster) == MONS_DRACONIAN))
    {
        return true;
    }

    if (mons_is_ghost_demon(mon->type)
        && species_is_draconian(mon->ghost->species))
    {
        return true;
    }

    return false;
}

static void _WYRMBANE_melee_effects(item_def* weapon, actor* attacker,
                                    actor* defender, bool mondied, int dam)
{
    if (!defender || !is_dragonkind(defender))
        return;

    // Since the target will become a DEAD MONSTER if it dies due to the extra
    // damage to dragons, we need to grab this information now.
    int hd = min(defender->get_experience_level(), 18);
    string name = defender->name(DESC_THE);

    if (!mondied)
    {
        mprf("%s %s!",
            defender->name(DESC_THE).c_str(),
            defender->conj_verb("convulse").c_str());

        defender->hurt(attacker, 1 + random2(3*dam/2));

        // Allow the lance to charge when killing dragonform felid players.
        mondied = defender->is_player() ? defender->as_player()->pending_revival
                                        : !defender->alive();
    }

    if (!mondied || defender->is_summoned()
        || (defender->is_monster()
            && testbits(defender->as_monster()->flags, MF_NO_REWARD)))
    {
        return;
    }

    // The cap can be reached by:
    // * iron dragon, golden dragon, pearl dragon (18)
    // * Xtahua (19)
    // * bone dragon, Serpent of Hell (20)
    // * Tiamat (22)
    // * pghosts (up to 27)
    dprf("Killed a drac with hd %d.", hd);

    if (weapon->plus < hd)
    {
        weapon->plus++;

        // Including you, if you were a dragonform felid with lives left.
        if (weapon->plus == 18)
        {
            mprf("<white>The lance glows brightly as it skewers %s. You feel "
                 "that it has reached its full power.</white>",
                 name.c_str());
        }
        else
        {
            mprf("<green>The lance glows as it skewers %s.</green>",
                 name.c_str());
        }

        you.wield_change = true;
    }
}

///////////////////////////////////////////////////

static void _UNDEADHUNTER_melee_effects(item_def* item, actor* attacker,
                                        actor* defender, bool mondied, int dam)
{
    if (defender->holiness() & MH_UNDEAD && !one_chance_in(3)
        && !mondied && dam)
    {
        mprf("%s %s blasted by disruptive energy!",
              defender->name(DESC_THE).c_str(),
              defender->conj_verb("be").c_str());
        defender->hurt(attacker, random2avg((1 + (dam * 3)), 3));
    }
}

///////////////////////////////////////////////////
static void _EOS_equip(item_def *item, bool *show_msgs, bool unmeld)
{
    invalidate_agrid(true);
}

static void _EOS_unequip(item_def *item, bool *show_msgs)
{
    invalidate_agrid(true);
}

///////////////////////////////////////////////////
static void _SHADOWS_equip(item_def *item, bool *show_msgs, bool unmeld)
{
    invalidate_agrid(true);
}

static void _SHADOWS_unequip(item_def *item, bool *show_msgs)
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
        did_god_conduct(DID_EVIL, 1);
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

static void _PLUTONIUM_SWORD_melee_effects(item_def* weapon, actor* attacker,
                                           actor* defender, bool mondied,
                                           int dam)
{
    if (!mondied && one_chance_in(5)
        && (!defender->is_monster()
             || !mons_immune_magic(*defender->as_monster())))
    {
        mpr("Mutagenic energy flows through the plutonium sword!");
        MiscastEffect(defender, attacker, MELEE_MISCAST, SPTYP_TRANSMUTATION,
                      random2(9), random2(70), "the plutonium sword", NH_NEVER);

        if (attacker->is_player())
            did_god_conduct(DID_CHAOS, 3);
    }
}

///////////////////////////////////////////////////

static void _SNAKEBITE_melee_effects(item_def* weapon, actor* attacker,
                                     actor* defender, bool mondied, int dam)
{
    if (!mondied && x_chance_in_y(2, 5))
    {
        curare_actor(attacker, defender, 2, "curare",
                     attacker->name(DESC_PLAIN));
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
    case 1: verb = "pulverise", adv = " into a thin bloody mist"; break;
    case 2: verb = "hew", adv = " savagely"; break;
    case 3: verb = "fatally mangle", adv = ""; break;
    case 4: verb = "dissect", adv = " like a pig carcass"; break;
    case 5: verb = "chop", adv = " into pieces"; break;
    case 6: verb = "butcher", adv = " messily"; break;
    case 7: verb = "slaughter", adv = " joyfully"; break;
    }
    if (you.see_cell(attacker->pos()) || you.see_cell(defender->pos()))
    {
        mprf("%s %s %s%s.", attacker->name(DESC_THE).c_str(),
             attacker->conj_verb(verb).c_str(),
             (attacker == defender ? defender->pronoun(PRONOUN_REFLEXIVE)
                                   : defender->name(DESC_THE)).c_str(),
             adv);
    }

    if (!mondied)
        defender->hurt(attacker, defender->stat_hp());
}

///////////////////////////////////////////////////

static void _DAMNATION_equip(item_def *item, bool *show_msgs, bool unmeld)
{
    _equip_mpr(show_msgs, you.hands_act("smoulder", "for a moment.").c_str());
}

static setup_missile_type _DAMNATION_launch(item_def* item, bolt* beam,
                                           string* ammo_name, bool* returning)
{
    ASSERT(beam->item
           && beam->item->base_type == OBJ_MISSILES
           && !is_artefact(*(beam->item)));
    beam->item->special = SPMSL_EXPLODING; // so that it mulches
    beam->item->props[DAMNATION_BOLT_KEY].get_bool() = true;

    beam->name    = "damnation bolt";
    *ammo_name    = "a damnation bolt";
    beam->colour  = LIGHTRED;
    beam->glyph   = DCHAR_FIRED_ZAP;

    bolt *expl   = new bolt(*beam);
    expl->flavour = BEAM_DAMNATION;
    expl->is_explosion = true;
    expl->damage = dice_def(3, 12);
    expl->name   = "damnation";

    beam->special_explosion = expl;
    return SM_FINISHED;
}

///////////////////////////////////////////////////

/**
 * Calculate the bonus damage that the Elemental Staff does with an attack of
 * the given flavour.
 *
 * @param flavour   The elemental flavour of attack; may be BEAM_NONE for earth
 *                  (physical) attacks.
 * @param defender  The victim of the attack. (Not const because checking res
 *                  may end up IDing items on monsters... )
 * @return          The amount of damage that the defender will receive.
 */
static int _calc_elemental_staff_damage(beam_type flavour,
                                        actor* defender)
{
    const int base_bonus_dam = 10 + random2(15);

    if (flavour == BEAM_NONE) // earth
        return defender->apply_ac(base_bonus_dam);

    return resist_adjust_damage(defender, flavour, base_bonus_dam);
}

static void _ELEMENTAL_STAFF_melee_effects(item_def*, actor* attacker,
                                           actor* defender, bool mondied,
                                           int)
{
    const int evoc = attacker->skill(SK_EVOCATIONS, 27);
    if (mondied || !(x_chance_in_y(evoc, 27*27) || x_chance_in_y(evoc, 27*27)))
        return;

    const char *verb = nullptr;
    beam_type flavour = BEAM_NONE;

    switch (random2(4))
    {
    case 0:
        verb = "burn";
        flavour = BEAM_FIRE;
        break;
    case 1:
        verb = "freeze";
        flavour = BEAM_COLD;
        break;
    case 2:
        verb = "electrocute";
        flavour = BEAM_ELECTRICITY;
        break;
    default:
        dprf("Bad damage type for elemental staff; defaulting to earth");
        // fallthrough to earth
    case 3:
        verb = "crush";
        break;
    }

    const int bonus_dam = _calc_elemental_staff_damage(flavour, defender);

    if (bonus_dam <= 0)
        return;

    mprf("%s %s %s.",
         attacker->name(DESC_THE).c_str(),
         attacker->conj_verb(verb).c_str(),
         (attacker == defender ? defender->pronoun(PRONOUN_REFLEXIVE)
                               : defender->name(DESC_THE)).c_str());

    defender->hurt(attacker, bonus_dam, flavour);

    if (defender->alive() && flavour != BEAM_NONE)
        defender->expose_to_element(flavour, 2);
}

///////////////////////////////////////////////////

static void _ARC_BLADE_equip(item_def *item, bool *show_msgs, bool unmeld)
{
    _equip_mpr(show_msgs, "The arc blade crackles to life.");
}

static void _ARC_BLADE_unequip(item_def *item, bool *show_msgs)
{
    _equip_mpr(show_msgs, "The arc blade stops crackling.");
}

static void _ARC_BLADE_melee_effects(item_def* weapon, actor* attacker,
                                     actor* defender, bool mondied,
                                     int dam)
{
    if (!mondied && one_chance_in(3))
    {
        const int pow = 75 + random2avg(75, 2);
        const int num_targs = 1 + random2(random_range(1, 3) + pow / 20);
        int dam_dealt = 0;
        for (int i = 0; defender->alive() && i < num_targs; i++)
            dam_dealt += discharge_monsters(defender->pos(), pow, attacker);
        if (dam_dealt > 0)
            scaled_delay(100);
        else
        {
            if (you.can_see(*attacker))
                mpr("The arc blade crackles.");
            else
                mpr("You hear the crackle of electricity.");
        }
    }
}

///////////////////////////////////////////////////

static void _SPELLBINDER_melee_effects(item_def* weapon, actor* attacker,
                                       actor* defender, bool mondied,
                                       int dam)
{
    // Only cause miscasts if the target has magic to disrupt.
    if (defender->antimagic_susceptible()
        && !mondied)
    {
        spschools_type school = SPTYP_NONE;
        if (defender->is_player())
        {
            for (int i = 0; i < you.spell_no; i++)
                school |= get_spell_disciplines(you.spells[i]);
        }
        else
        {
            const monster* mons = defender->as_monster();
            for (const mon_spell_slot &slot : mons->spells)
                school |= get_spell_disciplines(slot.spell);
        }
        if (school != SPTYP_NONE)
        {
            vector<spschool_flag_type> schools;
            for (const auto bit : spschools_type::range())
                if (testbits(school, bit))
                    schools.push_back(bit);

            ASSERT(schools.size() > 0);
            MiscastEffect(defender, attacker, MELEE_MISCAST,
                          schools[random2(schools.size())],
                          random2(9),
                          random2(70), "the demon whip \"Spellbinder\"",
                          NH_NEVER);
        }
    }
}

///////////////////////////////////////////////////

static void _ORDER_melee_effects(item_def* item, actor* attacker,
                                         actor* defender, bool mondied, int dam)
{
    if (!mondied)
    {
        string msg = "";
        int silver_dam = silver_damages_victim(defender, dam, msg);
        if (silver_dam)
        {
            if (you.can_see(*defender))
                mpr(msg);
            defender->hurt(attacker, silver_dam);
        }
    }
}

///////////////////////////////////////////////////

static void _FIRESTARTER_equip(item_def *item, bool *show_msgs, bool unmeld)
{
    _equip_mpr(show_msgs, "You are filled with an inner flame.");
}

static void _FIRESTARTER_unequip(item_def *item, bool *show_msgs)
{
    _equip_mpr(show_msgs, "Your inner flame fades away.");
}

static void _FIRESTARTER_melee_effects(item_def* weapon, actor* attacker,
                                   actor* defender, bool mondied, int dam)
{
    if (dam)
    {
        if (defender->is_monster()
            && !mondied
            && !defender->as_monster()->has_ench(ENCH_INNER_FLAME))
        {
            mprf("%s is filled with an inner flame.",
                 defender->name(DESC_THE).c_str());
            defender->as_monster()->add_ench(
                mon_enchant(ENCH_INNER_FLAME, 0, attacker,
                            (3 + random2(dam)) * BASELINE_DELAY));
        }
    }
}

///////////////////////////////////////////////////

static void _CHILLY_DEATH_equip(item_def *item, bool *show_msgs, bool unmeld)
{
    _equip_mpr(show_msgs, "The dagger glows with an icy blue light!");
}

static void _CHILLY_DEATH_unequip(item_def *item, bool *show_msgs)
{
    _equip_mpr(show_msgs, "The dagger stops glowing.");
}

static void _CHILLY_DEATH_melee_effects(item_def* weapon, actor* attacker,
                                   actor* defender, bool mondied, int dam)
{
    if (dam)
    {
        if (defender->is_monster()
            && !mondied
            && !defender->as_monster()->has_ench(ENCH_FROZEN))
        {
            mprf("%s is flash-frozen.",
                 defender->name(DESC_THE).c_str());
            defender->as_monster()->add_ench(
                mon_enchant(ENCH_FROZEN, 0, attacker,
                            (5 + random2(dam)) * BASELINE_DELAY));
        }
        else if (defender->is_player()
            && !you.duration[DUR_FROZEN])
        {
            mprf(MSGCH_WARN, "You are encased in ice.");
            you.increase_duration(DUR_FROZEN, 5 + random2(dam));
        }
    }
}

///////////////////////////////////////////////////

static void _FLAMING_DEATH_equip(item_def *item, bool *show_msgs, bool unmeld)
{
    _equip_mpr(show_msgs, "The scimitar bursts into red hot flame!");
}

static void _FLAMING_DEATH_unequip(item_def *item, bool *show_msgs)
{
    _equip_mpr(show_msgs, "The scimitar stops flaming.");
}

static void _FLAMING_DEATH_melee_effects(item_def* weapon, actor* attacker,
                                   actor* defender, bool mondied, int dam)
{
    if (!mondied && (dam > 2 && one_chance_in(3)))
    {
        if (defender->is_player())
            napalm_player(random2avg(7, 3) + 1, attacker->name(DESC_A, true));
        else
        {
            napalm_monster(
                defender->as_monster(),
                attacker,
                min(4, 1 + random2(attacker->get_hit_dice())/2));
        }
    }
}

///////////////////////////////////////////////////

static void _MAJIN_equip(item_def *item, bool *show_msgs, bool unmeld)
{
    if (!you.max_magic_points)
        return;

    const bool should_msg = !show_msgs || *show_msgs;
    _equip_mpr(show_msgs, "You feel a darkness envelop your magic.");

    if (!item->props.exists(MB_WELCOME_KEY) && should_msg)
    {
        const string msg = "A voice whispers, \"" +
                           getSpeakString("majin-bo greeting") + "\"";
        mprf(MSGCH_TALK, "%s", msg.c_str());
        item->props[MB_WELCOME_KEY].get_bool() = true;
    }
}

static void _MAJIN_unequip(item_def *item, bool *show_msgs)
{
    if (you.max_magic_points)
    {
        _equip_mpr(show_msgs,
                   "The darkness slowly releases its grasp on your magic.");
    }
}

///////////////////////////////////////////////////

static int _octorings_worn()
{
    int worn = 0;

    for (int i = EQ_LEFT_RING; i < NUM_EQUIP; ++i)
    {
        if (you.melded[i] || you.equip[i] == -1)
            continue;

        item_def& ring = you.inv[you.equip[i]];
        if (is_unrandom_artefact(ring, UNRAND_OCTOPUS_KING_RING))
            worn++;
    }

    return worn;
}

static void _OCTOPUS_KING_equip(item_def *item, bool *show_msgs, bool unmeld)
{
    int rings = _octorings_worn();

    if (rings == 8)
        _equip_mpr(show_msgs, "You feel like a king!");
    else if (rings)
        _equip_mpr(show_msgs, "You feel regal.");
    item->plus = 8 + 2 * rings;
}

static void _OCTOPUS_KING_world_reacts(item_def *item)
{
    item->plus = 8 + 2 * _octorings_worn();
}

///////////////////////////////////////////////////

static void _CAPTAIN_equip(item_def *item, bool *show_msgs, bool unmeld)
{
    if (you_worship(GOD_SHINING_ONE))
    {
        _equip_mpr(show_msgs,
                   "You feel dishonourable wielding this.");
    }
    else
    {
        _equip_mpr(show_msgs,
                   "You feel a cutthroat vibe.");
    }
}

static void _CAPTAIN_melee_effects(item_def* weapon, actor* attacker,
                                actor* defender, bool mondied, int dam)
{
    // Player disarming sounds like a bad idea; monster-on-monster might
    // work but would be complicated.
    if (coinflip()
        && dam >= (3 + random2(defender->get_hit_dice()))
        && !x_chance_in_y(defender->get_hit_dice(), random2(20) + dam*4)
        && attacker->is_player()
        && defender->is_monster()
        && !mondied)
    {
        item_def *wpn = defender->as_monster()->disarm();
        if (wpn)
        {
            mprf("The captain's cutlass flashes! You lacerate %s!!",
                defender->name(DESC_THE).c_str());
            mprf("%s %s falls to the floor!",
                apostrophise(defender->name(DESC_THE)).c_str(),
                wpn->name(DESC_PLAIN).c_str());
            defender->hurt(attacker, 18 + random2(18));
            did_god_conduct(DID_UNCHIVALRIC_ATTACK, 3);
        }
    }
}

///////////////////////////////////////////////////

static void _FENCERS_equip(item_def *item, bool *show_msgs, bool unmeld)
{
    _equip_mpr(show_msgs, "En garde!");
}

///////////////////////////////////////////////////

static void _ETHERIC_CAGE_equip(item_def *item, bool *show_msgs, bool unmeld)
{
    _equip_mpr(show_msgs, "You sense a greater flux of ambient magical fields.");
}

static void _ETHERIC_CAGE_world_reacts(item_def *item)
{
    const int delay = you.time_taken;
    ASSERT(delay > 0);

    // coinflip() chance of 1 MP per turn.
    if (player_regenerates_mp())
        inc_mp(binomial(div_rand_round(delay, BASELINE_DELAY), 1, 2));
}

///////////////////////////////////////////////////

static void _ETERNAL_TORMENT_equip(item_def *item, bool *show_msgs, bool unmeld)
{
    calc_hp();
}

static void _ETERNAL_TORMENT_world_reacts(item_def *item)
{
    if (one_chance_in(10))
        did_god_conduct(DID_EVIL, 1);
}


static void _ETERNAL_TORMENT_unequip(item_def *item, bool *show_msgs)
{
    calc_hp();
}

///////////////////////////////////////////////////

static void _VINES_equip(item_def *item, bool *show_msgs, bool unmeld)
{
    _equip_mpr(show_msgs, "The vines latch onto your body!");
}

static void _VINES_unequip(item_def *item, bool *show_msgs)
{
    _equip_mpr(show_msgs, "The vines fall away from your body!");
}

///////////////////////////////////////////////////

static void _KRYIAS_equip(item_def *item, bool *show_msgs, bool unmeld)
{
    if (you.species == SP_DEEP_DWARF)
        _equip_mpr(show_msgs, "You feel no connection to the armour.");
    else
        _equip_mpr(show_msgs, "Your attunement to healing devices increases!");
}

static void _KRYIAS_unequip(item_def *item, bool *show_msgs)
{
        _equip_mpr(show_msgs, "Your attunement to healing devices decreases.");
}

///////////////////////////////////////////////////

static void _FROSTBITE_melee_effects(item_def* weapon, actor* attacker,
                                    actor* defender, bool mondied, int dam)
{
    coord_def spot = defender->pos();
    if (!cell_is_solid(spot)
        && !cloud_at(spot)
        && one_chance_in(5))
    {
         place_cloud(CLOUD_COLD, spot, random_range(4, 8), attacker, 0);
    }
}

///////////////////////////////////////////////////

// Vampiric effect triggers on every hit, see attack::apply_damage_brand()

static void _LEECH_equip(item_def *item, bool *show_msgs, bool unmeld)
{
    if (you.undead_state() == US_ALIVE && !you_foodless())
        _equip_mpr(show_msgs, "You feel a powerful hunger.");
    else if (you.species != SP_VAMPIRE)
        _equip_mpr(show_msgs, "You feel very empty.");
    // else let player-equip.cc handle message
}


///////////////////////////////////////////////////

static void _THERMIC_ENGINE_equip(item_def *item, bool *show_msgs, bool unmeld)
{
    _equip_mpr(show_msgs, "The engine hums to life!");
}

static void _THERMIC_ENGINE_unequip(item_def *item, bool *show_msgs)
{
    _equip_mpr(show_msgs, "The engine shudders to a halt.");
}

static void _THERMIC_ENGINE_melee_effects(item_def* weapon, actor* attacker,
                                   actor* defender, bool mondied, int dam)
{
    if (mondied)
        return;

    const int bonus_dam = resist_adjust_damage(defender, BEAM_COLD,
                                               random2(dam) / 2 + 1);

    if (bonus_dam <= 0)
        return;

    mprf("%s %s %s.",
         attacker->name(DESC_THE).c_str(),
         attacker->conj_verb("freeze").c_str(),
         (attacker == defender ? defender->pronoun(PRONOUN_REFLEXIVE)
                               : defender->name(DESC_THE)).c_str());

    defender->hurt(attacker, bonus_dam, BEAM_COLD);

    if (defender->alive())
        defender->expose_to_element(BEAM_COLD, 2);
}

static bool _THERMIC_ENGINE_evoke(item_def *item, bool* did_work, bool* unevokable)
{
    if (!enough_hp(12, true))
    {
        mpr("You're too close to death to use this item.");
        *unevokable = true;
        return true;
    }
    if (!enough_mp(3, false))
    {
        *unevokable = true;
        return true;
    }

    if (!x_chance_in_y(you.skill(SK_EVOCATIONS, 100), 3000))
        return false;

    bool success = false;
    bool angered = false;

    const int how_many = 5 + random2(5);

    for (int i = 0; i < how_many; ++i)
    {
        monster_type mon = random_demon_by_tier(one_chance_in(15) ? 4 : 5);

        mgen_data mg(mon, BEH_CHARMED, you.pos(), MHITYOU,
                 MG_FORCE_BEH, you.religion);
        mg.set_summoned(&you, 0, 0);
        mg.extra_flags |= (MF_NO_REWARD | MF_HARD_RESET);
        monster *m = create_monster(mg);

        if (m)
        {
            m->add_ench(mon_enchant(ENCH_FAKE_ABJURATION, 3));

            if (player_angers_monster(m))
                angered = true;

            success = true;
        }
    }

    if (success)
    {
        mpr("You release some of the engine's servants.");
        did_god_conduct(DID_EVIL, 3);

        if (angered)
            mpr("You don't feel so good about this...");
    }

    mpr("You feel the engine feeding on your energy!");
    dec_hp(11, false);
    dec_mp(3);
    practise_evoking(1);

    return true;
}
