/**
 * @file
 * @brief Functions non-standard unrandarts uses.
**/

#pragma once

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

#include "areas.h"         // For silenced() and invalidate_agrid()
#include "attack.h"        // For attack_strength_punctuation()
#include "beam.h"          // For Lajatang of Order's silver damage
#include "bloodspatter.h"  // For Leech
#include "cloud.h"         // For robe of clouds' thunder and salamander's flame
#include "coordit.h"       // For distance_iterator()
#include "death-curse.h"   // For the Scythe of Curses
#include "english.h"       // For apostrophise
#include "exercise.h"      // For practise_evoking
#include "fight.h"
#include "fineff.h"        // For the Storm Queen's Shield
#include "god-conduct.h"   // did_god_conduct
#include "mgen-data.h"     // For Sceptre of Asmodeus
#include "melee-attack.h"  // For Fungal Fisticloak
#include "message.h"
#include "monster.h"
#include "mon-death.h"     // For demon axe's SAME_ATTITUDE
#include "mon-place.h"     // For Sceptre of Asmodeus
#include "nearby-danger.h" // For Zhor
#include "output.h"
#include "player.h"
#include "player-reacts.h" // For the consecrated labrys
#include "player-stats.h"
#include "showsymb.h"      // For Cigotuvi's Embrace
#include "spl-cast.h"      // For evokes
#include "spl-damage.h"    // For the Singing Sword.
#include "spl-goditem.h"   // For Sceptre of Torment tormenting
#include "spl-miscast.h"   // For Spellbinder and plutonium sword miscasts
#include "spl-monench.h"   // For Zhor's aura
#include "spl-summoning.h" // For Zonguldrok animating dead
#include "spl-transloc.h"  // For Autumn Katana's Manifold Assault
#include "tag-version.h"
#include "terrain.h"       // For storm bow
#include "rltiles/tiledef-main.h"
#include "unwind.h"        // For autumn katana
#include "view.h"          // For arc blade's discharge effect

// prop recording whether the singing sword has said hello yet
#define SS_WELCOME_KEY "ss_welcome"
// similarly, for the majin-bo
#define MB_WELCOME_KEY "mb_welcome"
// similarly, for the Skull of Zonguldrok
#define ZONGULDROK_WELCOME_KEY "zonguldrok_welcome"

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

////////////////////////////////////////////////////
static void _CEREBOV_melee_effects(item_def* /*weapon*/, actor* attacker,
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

static void _CONDEMNATION_equip(item_def */*item*/, bool *show_msgs, bool unmeld)
{
    if (!unmeld && you.species == SP_BARACHI)
        _equip_mpr(show_msgs, "You feel a strange sense of familiarity.");
}

static void _CONDEMNATION_unequip(item_def */*item*/, bool *show_msgs)
{
    if (you.species == SP_BARACHI)
        _equip_mpr(show_msgs, "You feel oddly sad, like being parted from an old friend.");
}

static void _CONDEMNATION_melee_effects(item_def* /*weapon*/, actor* attacker,
                                        actor* defender, bool mondied, int dam)
{
    if (!dam || mondied || defender->is_player())
        return;
    monster *mons = defender->as_monster();
    if (mons_intel(*mons) <= I_BRAINLESS)
        return;
    const int dur = random_range(40, 80);
    const bool was_guilty = mons->has_ench(ENCH_ANGUISH);
    if (mons->add_ench(mon_enchant(ENCH_ANGUISH, 0, attacker, dur)) && !was_guilty)
        simple_monster_message(*mons, " is haunted by guilt!");
}

////////////////////////////////////////////////////

static void _CURSES_equip(item_def */*item*/, bool *show_msgs, bool unmeld)
{
    _equip_mpr(show_msgs, "A shiver runs down your spine.");
    if (!unmeld)
        death_curse(you, nullptr, "the scythe of Curses", 0);
}

static void _CURSES_melee_effects(item_def* /*weapon*/, actor* attacker,
                                  actor* defender, bool mondied, int dam)
{
    if (attacker->is_player())
        did_god_conduct(DID_EVIL, 3);
    if (!mondied && defender->holiness() & (MH_NATURAL | MH_PLANT))
        death_curse(*defender, attacker, "the scythe of Curses", min(dam, 27));
}

////////////////////////////////////////////////////

static void _FINISHER_melee_effects(item_def* /*weapon*/, actor* attacker,
                                  actor* defender, bool mondied, int dam)
{
    // Can't kill a monster that's already dead.
    // Can't kill a monster if we don't do damage.
    // Don't insta-kill the player
    if (mondied || dam == 0 || defender->is_player())
        return;

    // Chance to insta-kill based on HD. From 1/4 for small HD popcorn down to
    // 1/10 for an Orb of Fire (compare to the 3/20 chance for banish or
    // instant teleport on distortion).
    if (x_chance_in_y(50 - defender->get_hit_dice(), 200))
    {
        monster* mons = defender->as_monster();
        mons->flags |= MF_EXPLODE_KILL;
        mons->hurt(attacker, INSTANT_DEATH);
    }
}

////////////////////////////////////////////////////

static void _THROATCUTTER_melee_effects(item_def* /*weapon*/, actor* attacker,
                                        actor* defender, bool mondied, int /*dam*/)
{
    // Can't kill a monster that's already dead.
    // Don't insta-kill the player
    if (mondied || defender->is_player())
        return;

    // Chance to insta-kill based on HP.
    // (Effectively extra AC-ignoring damage, sort of.)
    if (defender->stat_hp() <= 20 && one_chance_in(3))
    {
        monster* mons = defender->as_monster();
        if (you.can_see(*attacker))
        {
            const bool plural = attacker->is_monster();
            switch (get_mon_shape(mons_genus(mons->type)))
            {
            case MON_SHAPE_PLANT:
            case MON_SHAPE_ORB:
            case MON_SHAPE_BLOB:
            case MON_SHAPE_MISC:
                mprf("%s put%s %s out of %s misery!",
                     attacker->name(DESC_THE).c_str(),
                     plural ? "s" : "",
                     mons->name(DESC_THE).c_str(),
                     mons->pronoun(PRONOUN_POSSESSIVE).c_str());
                break;
            default: // yes, even fungi have heads :)
                mprf("%s behead%s %s%s!",
                     attacker->name(DESC_THE).c_str(),
                     plural ? "s" : "",
                     mons->name(DESC_THE).c_str(),
                     mons->heads() > 1 ? " thoroughly" : "");
            }
        }
        if (mons->num_heads > 1)
            mons->num_heads = 1; // mass chop those hydra heads
        mons->hurt(attacker, INSTANT_DEATH);
    }
}

////////////////////////////////////////////////////

static void _OLGREB_equip(item_def */*item*/, bool *show_msgs, bool /*unmeld*/)
{
    if (you.can_smell())
        _equip_mpr(show_msgs, "You smell chlorine.");
    else
        _equip_mpr(show_msgs, "The staff glows a sickly green.");
}

static void _OLGREB_unequip(item_def */*item*/, bool *show_msgs)
{
    if (you.can_smell())
        _equip_mpr(show_msgs, "The smell of chlorine vanishes.");
    else
        _equip_mpr(show_msgs, "The staff's sickly green glow vanishes.");
}

// Based on melee_attack::staff_damage(), but using only evocations skill.
static int _calc_olgreb_damage(actor* attacker, actor* defender)
{
    if (!x_chance_in_y(attacker->skill(SK_EVOCATIONS, 100), 1000))
        return 0;

    const int base_dam = random2(attacker->skill(SK_EVOCATIONS, 150) / 80);
    return resist_adjust_damage(defender, BEAM_POISON_ARROW, base_dam);
}


static void _OLGREB_melee_effects(item_def* /*weapon*/, actor* attacker,
                                  actor* defender, bool mondied,
                                  int /*dam*/)
{
    const int bonus_dam = _calc_olgreb_damage(attacker, defender);

    if (!mondied && bonus_dam)
    {
        mprf("%s %s %s%s",
             attacker->name(DESC_THE).c_str(),
             attacker->conj_verb("envenom").c_str(),
             defender->name(DESC_THE).c_str(),
             attack_strength_punctuation(bonus_dam).c_str());

        defender->hurt(attacker, bonus_dam);
        if (defender->alive())
            defender->poison(attacker, 2, true);
    }
}

////////////////////////////////////////////////////

static void _POWER_equip(item_def * /* item */, bool *show_msgs,
                         bool /*unmeld*/)
{
    _equip_mpr(show_msgs, "You sense an aura of extreme power.");
}

static void _POWER_melee_effects(item_def* /*weapon*/, actor* attacker,
                                 actor* defender, bool mondied, int /*dam*/)
{
    if (mondied)
        return;

    const int num_beams = div_rand_round(attacker->stat_hp(), 270);
    coord_def targ = defender->pos();

    for (int i = 0; i < num_beams; i++)
    {
        bolt beam;
        beam.thrower   = attacker->is_player() ? KILL_YOU : KILL_MON;
        beam.source    = attacker->pos();
        beam.source_id = attacker->mid;
        beam.attitude  = attacker->temp_attitude();
        beam.range = 4;
        beam.target = targ;
        zappy(ZAP_SWORD_BEAM, 100, false, beam);
        beam.fire();
    }
}

////////////////////////////////////////////////////

static void _HOLY_AXE_world_reacts(item_def *item)
{
    const int horror_level = current_horror_level();
    // Caps at the obsidian axe's base enchant.
    const int plus = min(horror_level * 3 + 4, 16);
    if (item->plus == plus)
        return;

    item->plus = plus;
    you.wield_change = true;
}

////////////////////////////////////////////////////

static void _SINGING_SWORD_equip(item_def *item, bool *show_msgs, bool /*unmeld*/)
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
    int tier = max(1, min(4, 1 + tension / 20));
    bool silent = silenced(you.pos());

    string old_name = get_artefact_name(*item);
    string new_name;
    if (silent)
        new_name = "Sulking Sword";
    else if (tier < 4)
        new_name = "Singing Sword";
    else
        new_name = "Screaming Sword";
    if (old_name != new_name)
    {
        set_artefact_name(*item, new_name);
        you.wield_change = true;
    }
}

static void _SINGING_SWORD_melee_effects(item_def* weapon, actor* attacker,
                                         actor* /* defender */,
                                         bool /*mondied*/, int /*dam*/)
{
    int tier;

    if (attacker->is_player())
        tier = max(1, min(4, 1 + get_tension(GOD_NO_GOD) / 20));
    // Don't base the sword on player state when the player isn't wielding it.
    else
        tier = 1;

    if (silenced(attacker->pos()))
        tier = 0;

    dprf(DIAG_COMBAT, "Singing sword tension: %d, tier: %d",
                attacker->is_player() ? get_tension(GOD_NO_GOD) : -1, tier);

    // Not as spammy at low tension. Max chance reached at tier 3, allowing
    // tier 0 to have a high chance so that the sword is likely to express its
    // unhappiness with being silenced.
    if (!x_chance_in_y(6, (tier == 1) ? 24: (tier == 2) ? 16: 12))
        return;

    const char *tenname[] =  {"silenced", "no_tension", "low_tension",
                              "high_tension", "SCREAM"};
    const string key = tenname[tier];
    string msg = getSpeakString("singing sword " + key);

    const int loudness[] = {0, 0, 20, 30, 40};

    item_noise(*weapon, *attacker, msg, loudness[tier]);

    if (tier < 1)
        return; // Can't cast when silenced.

    const int spellpower = 100 + 13 * (tier - 1) + (tier == 4 ? 36 : 0);
    fire_los_attack_spell(SPELL_SONIC_WAVE, spellpower, attacker);
}
////////////////////////////////////////////////////

static void _PRUNE_equip(item_def */*item*/, bool *show_msgs, bool /*unmeld*/)
{
    if (you.undead_state() == US_ALIVE)
        _equip_mpr(show_msgs, "You struggle to resist the curse of the Prune...");
    else
        _equip_mpr(show_msgs, "The curse of the Prune has no hold on the dead.");
}

static void _PRUNE_unequip(item_def */*item*/, bool *show_msgs)
{
    if (you.undead_state() == US_ALIVE)
        _equip_mpr(show_msgs, "The curse of the Prune lifts from you.");
}

////////////////////////////////////////////////////

static void _LIGHTNING_SCALES_equip(item_def */*item*/, bool *show_msgs, bool /*unmeld*/)
{
    _equip_mpr(show_msgs, "You feel lightning quick.");
}

static void _LIGHTNING_SCALES_unequip(item_def */*item*/, bool *show_msgs)
{
    _equip_mpr(show_msgs, "You feel rather sluggish.");
}

////////////////////////////////////////////////////

static void _TORMENT_equip(item_def */*item*/, bool *show_msgs, bool /*unmeld*/)
{
    _equip_mpr(show_msgs, "A terrible, searing pain shoots up your arm!");
}

static void _TORMENT_melee_effects(item_def* /*weapon*/, actor* attacker,
                                   actor* /*defender*/, bool /*mondied*/,
                                   int /*dam*/)
{
    if (one_chance_in(5))
        torment(attacker, TORMENT_SCEPTRE, attacker->pos());
}

/////////////////////////////////////////////////////

static void _TROG_equip(item_def */*item*/, bool *show_msgs, bool /*unmeld*/)
{
    _equip_mpr(show_msgs, "You feel the exhaustion of battles past.");
    player_end_berserk();
}

static void _TROG_unequip(item_def */*item*/, bool *show_msgs)
{
    _equip_mpr(show_msgs, "You feel less violent.");
}

///////////////////////////////////////////////////

static void _VARIABILITY_melee_effects(item_def* /*weapon*/, actor* attacker,
                                       actor* /*defender*/, bool mondied,
                                       int /*dam*/)
{
    if (!mondied && one_chance_in(5))
    {
        const int pow = 75 + random2avg(75, 2);
        if (you.can_see(*attacker))
            mpr("The mace of Variability scintillates.");
        cast_chain_spell(SPELL_CHAIN_OF_CHAOS, pow, attacker);
    }
}

///////////////////////////////////////////////////

static void _ZONGULDROK_equip(item_def */*item*/, bool *show_msgs,
                              bool /*unmeld*/)
{
    _equip_mpr(show_msgs, "You sense an extremely unholy aura.");
}

static void _ZONGULDROK_melee_effects(item_def* /*weapon*/, actor* attacker,
                                      actor* /*defender*/, bool /*mondied*/,
                                      int /*dam*/)
{
    if (attacker->is_player())
        did_god_conduct(DID_EVIL, 3);
}

///////////////////////////////////////////////////

static void _GONG_melee_effects(item_def* /*item*/, actor* wearer,
                                actor* /*attacker*/, bool /*dummy*/,
                                int /*dam*/)
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

static void _STORM_QUEEN_melee_effects(item_def* /*item*/, actor* wearer,
                                       actor* attacker, bool /*dummy*/,
                                       int /*dam*/)
{
    // Discharge does 3d(4 + pow*3/2) damage, so each point of power does
    // an average of another 9/4 points of retaliation damage (~2).
    // Let's try 3d7 damage at 1/3 chance. This is broadly comparable to
    // elec brand - same average damage per trigger, higher trigger chance,
    // but checks (half) AC - and triggers on block instead of attack :)
    if (!attacker || !one_chance_in(3)) return;
    shock_discharge_fineff::schedule(wearer, *attacker,
                                     wearer->pos(), 3,
                                     "shield");

}

///////////////////////////////////////////////////

static void _DEMON_AXE_melee_effects(item_def* /*item*/, actor* attacker,
                                     actor* defender, bool /*mondied*/,
                                     int /*dam*/)
{
    if (defender->is_peripheral())
        return;

    if (one_chance_in(10))
    {
        if (monster* mons = attacker->as_monster())
        {
            create_monster(
                mgen_data(summon_any_demon(RANDOM_DEMON_COMMON),
                          SAME_ATTITUDE(mons), mons->pos(), mons->foe)
                .set_summoned(mons, SPELL_SUMMON_DEMON, summ_dur(6)));
        }
        else if (!you.allies_forbidden())
        {
            monster_type type = (one_chance_in(3) ? random_demon_by_tier(3)
                                                  : random_demon_by_tier(4));
            if (create_monster(
                    mgen_data(type, BEH_COPY, you.pos(), MHITYOU, MG_FORCE_BEH | MG_AUTOFOE)
                    .set_summoned(&you, SPELL_SUMMON_DEMON, summ_dur(6))))
            {
                mpr("A gate to Pandemonium opens briefly!");
            }
        }

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

static void _DEMON_AXE_world_reacts(item_def */*item*/)
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

static void _DEMON_AXE_unequip(item_def */*item*/, bool */*show_msgs*/)
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

static void _WYRMBANE_equip(item_def */*item*/, bool *show_msgs, bool /*unmeld*/)
{
    _equip_mpr(show_msgs,
               species::is_draconian(you.species)
                || you.form == transformation::dragon
                   ? "You feel an uncomfortable desire to slay dragons."
                   : "You feel an overwhelming desire to slay dragons!");
}

static void _WYRMBANE_melee_effects(item_def* weapon, actor* attacker,
                                    actor* defender, bool mondied, int dam)
{
    if (!defender || !defender->is_dragonkind())
        return;

    // Since the target will become a DEAD MONSTER if it dies due to the extra
    // damage to dragons, we need to grab this information now.
    const int hd = defender->dragon_level();
    string name = defender->name(DESC_THE);

    if (!mondied)
    {
        int bonus_dam = 1 + random2(3 * dam / 2);
        mprf("%s %s%s",
            defender->name(DESC_THE).c_str(),
            defender->conj_verb("convulse").c_str(),
            attack_strength_punctuation(bonus_dam).c_str());

        defender->hurt(attacker, bonus_dam);

        // Allow the lance to charge when killing dragonform felid players.
        mondied = defender->is_player() ? defender->as_player()->pending_revival
                                        : !defender->alive();
    }

    if (!mondied || !hd)
        return;

    // The cap can be reached by:
    // * iron dragon, golden dragon, pearl dragon, wyrmhole (18)
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

static void _UNDEADHUNTER_melee_effects(item_def* /*item*/, actor* attacker,
                                        actor* defender, bool mondied, int dam)
{
    if (defender->holiness() & MH_UNDEAD && !one_chance_in(3)
        && !mondied && dam)
    {
        int bonus_dam = random2avg((1 + (dam * 3)), 3);
        mprf("%s %s blasted by disruptive energy%s",
              defender->name(DESC_THE).c_str(),
              defender->conj_verb("be").c_str(),
              attack_strength_punctuation(bonus_dam).c_str());
        defender->hurt(attacker, bonus_dam);
    }
}

///////////////////////////////////////////////////
static void _EOS_equip(item_def */*item*/, bool */*show_msgs*/, bool /*unmeld*/)
{
    invalidate_agrid(true);
}

static void _EOS_unequip(item_def */*item*/, bool */*show_msgs*/)
{
    invalidate_agrid(true);
}

///////////////////////////////////////////////////
static void _BRILLIANCE_equip(item_def */*item*/, bool */*show_msgs*/, bool /*unmeld*/)
{
    invalidate_agrid(true);
}

static void _BRILLIANCE_unequip(item_def */*item*/, bool */*show_msgs*/)
{
    invalidate_agrid(true);
}

///////////////////////////////////////////////////
static void _SHADOWS_equip(item_def */*item*/, bool */*show_msgs*/, bool /*unmeld*/)
{
    invalidate_agrid(true);
}

static void _SHADOWS_unequip(item_def */*item*/, bool */*show_msgs*/)
{
    invalidate_agrid(true);
}

///////////////////////////////////////////////////
static void _DEVASTATOR_equip(item_def */*item*/, bool *show_msgs,
                              bool /*unmeld*/)
{
    _equip_mpr(show_msgs, "Time to lay down the shillelagh law.");
}

static void _DEVASTATOR_melee_effects(item_def* /*item*/, actor* attacker,
                                      actor* defender, bool /*mondied*/,
                                      int dam)
{
    if (dam)
        shillelagh(attacker, defender->pos(), dam);
}

///////////////////////////////////////////////////
static void _DRAGONSKIN_equip(item_def */*item*/, bool *show_msgs, bool /*unmeld*/)
{
    _equip_mpr(show_msgs, "You feel oddly protected from the elements.");
}

static void _DRAGONSKIN_unequip(item_def */*item*/, bool *show_msgs)
{
    _equip_mpr(show_msgs, "You no longer feel protected from the elements.");
}

///////////////////////////////////////////////////
static void _BLACK_KNIGHT_HORSE_world_reacts(item_def */*item*/)
{
    if (x_chance_in_y(you.time_taken, 10 * BASELINE_DELAY))
        did_god_conduct(DID_EVIL, 1);
}

///////////////////////////////////////////////////
static void _NIGHT_equip(item_def */*item*/, bool *show_msgs, bool /*unmeld*/)
{
    update_vision_range();
    _equip_mpr(show_msgs, "The light fades from your surroundings.");
}

static void _NIGHT_unequip(item_def */*item*/, bool *show_msgs)
{
    update_vision_range();
    _equip_mpr(show_msgs, "The light returns to your surroundings.");
}

///////////////////////////////////////////////////

static const vector<string> plutonium_player_msg = {
};

static void _PLUTONIUM_SWORD_melee_effects(item_def* weapon,
                                           actor* attacker, actor* defender,
                                           bool mondied, int /*dam*/)
{
    if (!mondied && one_chance_in(5) && defender->can_mutate())
    {
        if (you.can_see(*attacker))
        {
            mprf("Mutagenic energy flows through %s!",
                 weapon->name(DESC_THE, false, false, false).c_str());
        }

        if (attacker->is_player())
            did_god_conduct(DID_CHAOS, 3);

        if (one_chance_in(10))
        {
            defender->polymorph(0); // Low duration if applied to the player.
            return;
        }

        if (defender->is_monster())
            defender->malmutate(attacker, "the plutonium sword");
        else
        {
            mpr(random_choose("Your body deforms painfully.",
                              "Your limbs ache and wobble like jelly.",
                              "Your body is flooded with magical radiation."));
            contaminate_player(random_range(3500, 6500));
        }
        defender->hurt(attacker, random_range(5, 25));
    }
}

///////////////////////////////////////////////////

static void _SNAKEBITE_melee_effects(item_def* /*weapon*/, actor* attacker,
                                     actor* defender, bool mondied, int /*dam*/)
{
    if (!mondied && x_chance_in_y(2, 5))
        curare_actor(attacker, defender, "curare", attacker->name(DESC_PLAIN));
}

///////////////////////////////////////////////////

static void _WOE_melee_effects(item_def* /*weapon*/, actor* attacker,
                               actor* defender, bool mondied, int /*dam*/)
{
    const char *verb = "bugger", *adv = "";
    switch (random2(8))
    {
    case 0: verb = "cleave", adv = " in twain"; break;
    case 1: verb = "pulverise", adv = " into a thin bloody mist"; break;
    case 2: verb = "hew", adv = " violently"; break;
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

    if (defender->as_monster()->has_blood())
    {
        blood_spray(defender->pos(), defender->as_monster()->type,
                    random_range(5, 10));
    }
    defender->as_monster()->flags |= MF_EXPLODE_KILL;
}

///////////////////////////////////////////////////

static void _DAMNATION_launch(bolt* beam)
{

    beam->name    = "damnation bolt";
    beam->colour  = LIGHTRED;
    beam->glyph   = DCHAR_FIRED_ZAP;

    bolt *expl   = new bolt(*beam);
    expl->flavour = BEAM_DAMNATION;
    expl->is_explosion = true;
    expl->damage = dice_def(2, 14);
    expl->name   = "damnation";
    expl->tile_explode = TILE_BOLT_DAMNATION;

    beam->special_explosion = expl;
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
    int preac = 10 + random2(15);

    if (flavour == BEAM_FIRE || flavour == BEAM_COLD)
        preac = div_rand_round(preac * 5, 4);

    const ac_type ac_check = flavour == BEAM_ELECTRICITY ? ac_type::half
                                                         : ac_type::normal;
    const int postac = defender->apply_ac(preac, 0, ac_check);

    return resist_adjust_damage(defender, flavour, postac);
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
        // XXX TODO removeme
        dprf("Bad damage type for elemental staff; defaulting to earth");
        // fallthrough to earth
    case 3:
        verb = "crush";
        flavour = BEAM_MMISSILE;
        break;
    }

    const int bonus_dam = _calc_elemental_staff_damage(flavour, defender);

    if (bonus_dam <= 0)
        return;

    mprf("%s %s %s%s",
         attacker->name(DESC_THE).c_str(),
         attacker->conj_verb(verb).c_str(),
         (attacker == defender ? defender->pronoun(PRONOUN_REFLEXIVE)
                               : defender->name(DESC_THE)).c_str(),
         attack_strength_punctuation(bonus_dam).c_str());

    defender->hurt(attacker, bonus_dam, flavour);

    if (defender->alive() && flavour != BEAM_NONE)
        defender->expose_to_element(flavour, 2, attacker);
}

///////////////////////////////////////////////////

static void _ARC_BLADE_equip(item_def */*item*/, bool *show_msgs, bool /*unmeld*/)
{
    _equip_mpr(show_msgs, "The arc blade crackles to life.");
}

static void _ARC_BLADE_unequip(item_def */*item*/, bool *show_msgs)
{
    _equip_mpr(show_msgs, "The arc blade stops crackling.");
}

static void _ARC_BLADE_melee_effects(item_def* /*weapon*/, actor* attacker,
                                     actor* defender, bool /*mondied*/,
                                     int /*dam*/)
{
    if (one_chance_in(3))
    {
        const int pow = 100 + random2avg(100, 2);

        if (you.can_see(*attacker))
            mpr("The arc blade crackles.");
        else
            mpr("You hear the crackle of electricity.");

        if (attacker->pos().distance_from(defender->pos()) <= 1)
            cast_discharge(pow, *attacker, false, false);
        else
            discharge_at_location(pow, *attacker, defender->pos());
    }
}

///////////////////////////////////////////////////

static void _SPELLBINDER_melee_effects(item_def* /*weapon*/, actor* attacker,
                                       actor* defender, bool mondied,
                                       int dam)
{
    // Only cause miscasts if the target has magic to disrupt.
    if (defender->antimagic_susceptible()
        && !mondied)
    {
        miscast_effect(*defender, attacker, {miscast_source::melee},
                       spschool::random, random_range(1, 9), dam,
                       "the demon whip \"Spellbinder\"");
    }
}

///////////////////////////////////////////////////

static void _ORDER_melee_effects(item_def* /*item*/, actor* attacker,
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
        else if (dam > 0)
            defender->hurt(attacker, 1 + random2(dam) / 3);
    }
}

///////////////////////////////////////////////////

static void _FIRESTARTER_equip(item_def */*item*/, bool *show_msgs,
                               bool /*unmeld*/)
{
    _equip_mpr(show_msgs, "You are filled with an inner flame.");
}

static void _FIRESTARTER_unequip(item_def */*item*/, bool *show_msgs)
{
    _equip_mpr(show_msgs, "Your inner flame fades away.");
}

static void _FIRESTARTER_melee_effects(item_def* /*weapon*/, actor* attacker,
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

////////////////////////////////////////////////////
static void _FORCE_LANCE_melee_effects(item_def* /*weapon*/, actor* attacker,
                                       actor* defender, bool mondied, int dam)
{
    if (mondied || !dam || !one_chance_in(3)) return;
    // max power on a !!! hit (ie 36+ damage), but try to make some damage
    // quite likely to beat AC on any collision.
    const int collide_damage = 7 + roll_dice(3, div_rand_round(min(36, dam), 4));
    defender->knockback(*attacker, 1, collide_damage, "blow");
}

///////////////////////////////////////////////////

#if TAG_MAJOR_VERSION == 34
static void _CHILLY_DEATH_equip(item_def */*item*/, bool *show_msgs,
                                bool /*unmeld*/)
{
    _equip_mpr(show_msgs, "The dagger glows with an icy blue light!");
}

static void _CHILLY_DEATH_unequip(item_def */*item*/, bool *show_msgs)
{
    _equip_mpr(show_msgs, "The dagger stops glowing.");
}

static void _CHILLY_DEATH_melee_effects(item_def* /*weapon*/, actor* attacker,
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
#endif

///////////////////////////////////////////////////

#if TAG_MAJOR_VERSION == 34
static void _FLAMING_DEATH_equip(item_def */*item*/, bool *show_msgs,
                                 bool /*unmeld*/)
{
    _equip_mpr(show_msgs, "The scimitar bursts into red hot flame!");
}

static void _FLAMING_DEATH_unequip(item_def */*item*/, bool *show_msgs)
{
    _equip_mpr(show_msgs, "The scimitar stops flaming.");
}

static void _FLAMING_DEATH_melee_effects(item_def* /*weapon*/, actor* attacker,
                                   actor* defender, bool mondied, int dam)
{
    if (!mondied && (dam > 2 && one_chance_in(3)))
    {
        if (defender->is_player())
            sticky_flame_player(5, 10, attacker->name(DESC_A, true));
        else
        {
            sticky_flame_monster(
                defender->as_monster(),
                attacker,
                min(4, 1 + random2(attacker->get_hit_dice())/2));
        }
    }
}
#endif

///////////////////////////////////////////////////

static void _MAJIN_equip(item_def *item, bool *show_msgs, bool /*unmeld*/)
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

static void _MAJIN_unequip(item_def */*item*/, bool *show_msgs)
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

    vector<item_def*> rings = you.equipment.get_slot_items(SLOT_RING);
    for (item_def* ring : rings)
        if (is_unrandom_artefact(*ring, UNRAND_OCTOPUS_KING_RING))
            worn++;

    return worn;
}

static void _OCTOPUS_KING_equip(item_def *item, bool *show_msgs,
                                bool /*unmeld*/)
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

static void _CAPTAIN_melee_effects(item_def* /*weapon*/, actor* attacker,
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
        }
    }
}

///////////////////////////////////////////////////

static void _FENCERS_equip(item_def */*item*/, bool *show_msgs, bool /*unmeld*/)
{
    _equip_mpr(show_msgs, "En garde!");
}

#if TAG_MAJOR_VERSION == 34
///////////////////////////////////////////////////

static void _ETHERIC_CAGE_equip(item_def */*item*/, bool *show_msgs,
                                bool /*unmeld*/)
{
    _equip_mpr(show_msgs, "You sense a greater flux of ambient magical fields.");
}

static void _ETHERIC_CAGE_world_reacts(item_def */*item*/)
{
    const int delay = you.time_taken;
    ASSERT(delay > 0);

    // coinflip() chance of 1 MP per turn. Be sure to change
    // _get_overview_resistances to match!
    if (player_regenerates_mp())
        inc_mp(binomial(div_rand_round(delay, BASELINE_DELAY), 1, 2));
}

///////////////////////////////////////////////////

static void _ETERNAL_TORMENT_equip(item_def */*item*/, bool */*show_msgs*/,
                                   bool /*unmeld*/)
{
    calc_hp();
}

static void _ETERNAL_TORMENT_world_reacts(item_def */*item*/)
{
    if (one_chance_in(10))
        did_god_conduct(DID_EVIL, 1);
}


static void _ETERNAL_TORMENT_unequip(item_def */*item*/, bool */*show_msgs*/)
{
    calc_hp();
}
#endif

///////////////////////////////////////////////////

static void _VINES_equip(item_def */*item*/, bool *show_msgs, bool /*unmeld*/)
{
    _equip_mpr(show_msgs, "The vines latch onto your body!");
}

static void _VINES_unequip(item_def */*item*/, bool *show_msgs)
{
    _equip_mpr(show_msgs, "The vines fall away from your body!");
}

///////////////////////////////////////////////////

static void _KRYIAS_equip(item_def */*item*/, bool *show_msgs, bool /*unmeld*/)
{
    _equip_mpr(show_msgs, "Your attunement to healing potions increases.");
}

static void _KRYIAS_unequip(item_def */*item*/, bool *show_msgs)
{
    _equip_mpr(show_msgs, "Your attunement to healing potions decreases.");
}

///////////////////////////////////////////////////

static void _FROSTBITE_melee_effects(item_def* /*weapon*/, actor* attacker,
                                     actor* defender, bool /*mondied*/,
                                     int /*dam*/)
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

// Big killing blows give a bloodsplosion effect sometimes
static void _LEECH_melee_effects(item_def* /*item*/, actor* attacker,
                                 actor* defender, bool mondied, int dam)
{
    if (attacker->is_player() && defender->has_blood()
        && mondied && x_chance_in_y(dam, 729))
    {
        simple_monster_message(*(defender->as_monster()),
                               " liquefies into a cloud of blood!");
        blood_spray(defender->pos(), defender->type, 50);
    }
}

///////////////////////////////////////////////////

static void _THERMIC_ENGINE_equip(item_def *item, bool *show_msgs,
                                  bool /*unmeld*/)
{
    _equip_mpr(show_msgs, "The engine hums to life!");
    item->plus = 2;
}

static void _THERMIC_ENGINE_unequip(item_def *item, bool *show_msgs)
{
    _equip_mpr(show_msgs, "The engine shudders to a halt.");
    item->plus = 2;
}

static void _THERMIC_ENGINE_melee_effects(item_def* weapon, actor* attacker,
                                   actor* defender, bool mondied, int dam)
{
    if (weapon->plus < 14)
    {
        weapon->plus += 2;

        if (weapon->plus > 14)
            weapon->plus = 14;

        you.wield_change = true;
    }

    if (mondied)
        return;

    // the flaming brand has already been applied at this point
    const int bonus_dam = resist_adjust_damage(defender, BEAM_COLD,
                                               random2(dam) / 2 + 1);
    if (bonus_dam > 0)
    {
        mprf("%s %s %s.",
            attacker->name(DESC_THE).c_str(),
            attacker->conj_verb("freeze").c_str(),
            (attacker == defender ? defender->pronoun(PRONOUN_REFLEXIVE)
                                : defender->name(DESC_THE)).c_str());

        defender->hurt(attacker, bonus_dam, BEAM_COLD);
        if (defender->alive())
            defender->expose_to_element(BEAM_COLD, 2, attacker);
    }
}

static void _THERMIC_ENGINE_world_reacts(item_def *item)
{
    if (item->plus > 2)
    {
        item->plus -= div_rand_round(you.time_taken, BASELINE_DELAY);

        if (item->plus < 2)
            item->plus = 2;

        you.wield_change = true;
    }
}

///////////////////////////////////////////////////

static void _ZHOR_world_reacts(item_def */*item*/)
{
    if (!you.time_taken)
        return;

    if (there_are_monsters_nearby(true, false, false)
        && one_chance_in(7 * div_rand_round(BASELINE_DELAY, you.time_taken)))
    {
        cast_englaciation(30, false);
    }
}

////////////////////////////////////////////////////

static void _BATTLE_unequip(item_def */*item*/, bool */*show_msgs*/)
{
    end_battlesphere(find_battlesphere(&you), false);
}

static void _BATTLE_world_reacts(item_def */*item*/)
{
    if (!find_battlesphere(&you)
        && there_are_monsters_nearby(true, true, false)
        && stop_summoning_reason(MR_RES_POISON, M_FLIES).empty())
    {
        const int pow = div_rand_round(15 + you.skill(SK_CONJURATIONS, 15), 3);
        cast_battlesphere(&you, pow, false);
        did_god_conduct(DID_WIZARDLY_ITEM, 10);
    }
}

////////////////////////////////////////////////////

static void _EMBRACE_unequip(item_def *item, bool *show_msgs)
{
    int &armour = item->props[EMBRACE_ARMOUR_KEY].get_int();
    if (armour > 0)
    {
        _equip_mpr(show_msgs, "Your corpse armour falls away.");
        armour = 0;
        item->plus = get_unrand_entry(item->unrand_idx)->plus;
    }
}

/**
 * Iterate over all corpses in LOS and harvest them.
 *
 * @return            The total number of corpses destroyed.
 */
static int _harvest_corpses()
{
    int harvested = 0;

    for (radius_iterator ri(you.pos(), LOS_NO_TRANS); ri; ++ri)
    {
        for (stack_iterator si(*ri, true); si; ++si)
        {
            item_def &item = *si;
            // Don't encourage hoarding old skeletons. Full corpses only.
            if (!item.is_type(OBJ_CORPSES, CORPSE_BODY))
                continue;

            // forbid harvesting orcs under Beogh
            const monster_type monnum
                = static_cast<monster_type>(item.orig_monnum);
            if (you.religion == GOD_BEOGH && mons_genus(monnum) == MONS_ORC)
                continue;

            did_god_conduct(DID_EVIL, 1);

            ++harvested;

            // don't spam animations
            if (harvested <= 5)
            {
                bolt beam;
                beam.source = *ri;
                beam.target = you.pos();
                beam.glyph = get_item_glyph(item).ch;
                beam.colour = item.get_colour();
                beam.range = LOS_RADIUS;
                beam.aimed_at_spot = true;
                beam.item = &item;
                beam.flavour = BEAM_VISUAL;
                beam.draw_delay = 3;
                beam.fire();
                viewwindow();
                update_screen();
            }

            destroy_item(item.index());
        }
    }

    return harvested;
}

static void _EMBRACE_world_reacts(item_def *item)
{
    int &armour = item->props[EMBRACE_ARMOUR_KEY].get_int();
    const int harvested = _harvest_corpses();
    // diminishing returns for more corpses
    for (int i = 0; i < harvested; i++)
        armour += div_rand_round(100 * 100, (armour + 100));

    // decay over time - 1 turn per 'armour' base, 0.5 turns at 400 'armour'
    armour -= div_rand_round(you.time_taken * (armour + 400), 10 * 400);

    const int last_plus = item->plus;
    const int base_plus = get_unrand_entry(item->unrand_idx)->plus;
    if (armour <= 0)
    {
        armour = 0;
        item->plus = base_plus;
        if (last_plus > base_plus)
            mpr("Your corpse armour falls away.");
    }
    else
    {
        item->plus = base_plus + 1 + (armour-1) * 6 / 100;
        if (item->plus < last_plus)
            mpr("A chunk of your corpse armour falls away.");
        else if (last_plus == base_plus)
            mpr("The bodies of the dead rush to embrace you!");
        else if (item->plus > last_plus)
            mpr("Your shell of carrion and bone grows thicker.");
    }

    if (item->plus != last_plus)
        you.redraw_armour_class = true;
}

////////////////////////////////////////////////////

static void _manage_fire_shield()
{
    // Melt ice armour entirely.
    maybe_melt_player_enchantments(BEAM_FIRE, 100);

    surround_actor_with_cloud(&you, CLOUD_FIRE);
}

static void _SALAMANDER_equip(item_def * /* item */, bool * show_msgs,
                              bool /* unmeld */)
{
    _equip_mpr(show_msgs, "The air around you leaps into flame!");
    _manage_fire_shield();
}

static void _SALAMANDER_unequip(item_def * /* item */, bool * show_msgs)
{
   _equip_mpr(show_msgs, "Your ring of flames gutters out.");
}

static void _SALAMANDER_world_reacts(item_def * /* item */)
{
    _manage_fire_shield();
}

////////////////////////////////////////////////////

static void _GUARD_unequip(item_def *item, bool * show_msgs)
{
    monster *spectral_weapon = find_spectral_weapon(*item);
    if (spectral_weapon)
    {
        _equip_mpr(show_msgs, "Your spectral weapon disappears.");
        end_spectral_weapon(spectral_weapon, false, true);
    }
}

////////////////////////////////////////////////////

static void _WUCAD_MU_equip(item_def */*item*/, bool *show_msgs,
                            bool /*unmeld*/)
{
    if (you.has_mutation(MUT_HP_CASTING))
    {
        _equip_mpr(show_msgs, "The crystal ball is unable to connect with your "
                              "magical essence.");
    }
}

////////////////////////////////////////////////////

static void _SEVEN_LEAGUE_BOOTS_equip(item_def * /*item*/, bool *show_msgs,
                                      bool /*unmeld*/)
{
    _equip_mpr(show_msgs, "You feel ready to stride towards your foes.");
}

static void _SEVEN_LEAGUE_BOOTS_unequip(item_def * /*item*/, bool *show_msgs)
{
    _equip_mpr(show_msgs, "You no longer feel ready to stride towards your "
                          "foes.");
}

////////////////////////////////////////////////////

static void _RCLOUDS_world_reacts(item_def */*item*/)
{
    for (radius_iterator ri(you.pos(), 2, C_SQUARE, LOS_SOLID); ri; ++ri)
    {
        monster* m = monster_at(*ri);
        if (m && !m->wont_attack() && mons_is_threatening(*m)
            && !cell_is_solid(*ri) && !cloud_at(*ri)
            && x_chance_in_y(you.time_taken, 7 * BASELINE_DELAY))
        {
            mprf("Storm clouds gather above %s.", m->name(DESC_THE).c_str());
            place_cloud(CLOUD_STORM, *ri, random_range(4, 8), &you);
        }
    }
}

////////////////////////////////////////////////////

static void _POWER_GLOVES_equip(item_def * /*item*/, bool *show_msgs,
                                bool /*unmeld*/)
{
    if (you.has_mutation(MUT_HP_CASTING))
    {
        _equip_mpr(show_msgs, "The gloves are unable to connect with your "
                              "magical essence.");
    }
    else
        _equip_mpr(show_msgs, "You feel an incredible surge of magic.");
}

static void _POWER_GLOVES_unequip(item_def * /*item*/, bool *show_msgs)
{
    if (!you.has_mutation(MUT_HP_CASTING))
        _equip_mpr(show_msgs, "The surge of magic dissipates.");
}

////////////////////////////////////////////////////

static void _DREAMSHARD_NECKLACE_equip(item_def * /*item*/, bool *show_msgs,
                                      bool /*unmeld*/)
{
    _equip_mpr(show_msgs, "You feel a whimsical energy watch over you.");
}

static void _DREAMSHARD_NECKLACE_unequip(item_def * /* item */, bool * show_msgs)
{
    _equip_mpr(show_msgs, "The world feels relentlessly logical and grey.");
}

////////////////////////////////////////////////////

static void _AUTUMN_KATANA_melee_effects(item_def* /*weapon*/, actor* attacker,
    actor* defender, bool /*mondied*/, int /*dam*/)
{
    // HACK: yes this is in a header but it's only included once
    static bool _slicing = false;

    if (!one_chance_in(5) || _slicing || !defender)
        return;

    unwind_bool nonrecursive_space(_slicing, true);

    // If attempting to cast manifold assault would abort (likely because of no
    // valid targets in range), do nothing
    if (cast_manifold_assault(*attacker, 0, false, false, defender) == spret::abort)
        return;

    mprf("%s slice%s through the folds of space itself!",
         attacker->name(DESC_THE).c_str(),
         attacker->is_player() ? "" : "s");

    // Casting with 100 power = up to 4 targets hit
    cast_manifold_assault(*attacker, 100, false, true, defender);
}

///////////////////////////////////////////////////

static void _FINGER_AMULET_world_reacts(item_def */*item*/)
{
    did_god_conduct(DID_EVIL, 1);
}

///////////////////////////////////////////////////

static void _reset_victory_stats(item_def *item)
{
    int &bonus_stats = item->props[VICTORY_STAT_KEY].get_int();
    if (bonus_stats > 0)
    {
        bonus_stats = 0;
        item->plus = get_unrand_entry(item->unrand_idx)->plus;
        artefact_set_property(*item, ARTP_SLAYING, bonus_stats);
        artefact_set_property(*item, ARTP_INTELLIGENCE, bonus_stats);
        mprf(MSGCH_WARN, "%s stops glowing.", item->name(DESC_THE, false, true,
                                                         false).c_str());

        you.redraw_armour_class = true;
        notify_stat_change();
    }
}

static void _VICTORY_unequip(item_def *item, bool */*show_msgs*/)
{
    if (!you.unrand_equipped(UNRAND_VICTORY, true))
        _reset_victory_stats(item);
}

#define VICTORY_STAT_CAP 7

static void _VICTORY_death_effects(item_def *item, monster* mons,
                                   killer_type killer)
{
    // No bonus for killing friendlies, neutrals, summons, etc.
    if (killer != KILL_YOU && killer != KILL_YOU_MISSILE
        || !mons_gives_xp(*mons, you))
    {
        return;
    }

    const mon_threat_level_type threat = mons_threat_level(*mons);

    // Increased chance of victory bonus from more dangerous mons.
    // Using threat for this is kludgy, but easily visible to players.
    if (threat == MTHRT_NASTY || (threat == MTHRT_TOUGH && x_chance_in_y(1, 4)))
    {
        int &bonus_stats = item->props[VICTORY_STAT_KEY].get_int();
        if (bonus_stats < VICTORY_STAT_CAP)
        {
            bonus_stats++;
            item->plus = bonus_stats;
            artefact_set_property(*item, ARTP_SLAYING, bonus_stats);
            artefact_set_property(*item, ARTP_INTELLIGENCE, bonus_stats);
            mprf(MSGCH_GOD, GOD_OKAWARU, "%s glows%s.",
                 item->name(DESC_THE, false, true, false).c_str(),
                 bonus_stats == VICTORY_STAT_CAP ? " brightly" : "");

            you.redraw_armour_class = true;
            notify_stat_change();
        }
    }
}

static void _VICTORY_world_reacts(item_def *item)
{
    if (you.props.exists(VICTORY_CONDUCT_KEY))
    {
        _reset_victory_stats(item);
        you.props.erase(VICTORY_CONDUCT_KEY);
    }
}

static void _VICTORY_equip(item_def *item, bool */*show_msgs*/, bool /*unmeld*/)
{
    _VICTORY_world_reacts(item);
}

////////////////////////////////////////////////////

static void _ASMODEUS_melee_effects(item_def* /*weapon*/, actor* attacker,
                                    actor* defender, bool /*mondied*/,
                                    int /*dam*/)
{
    if (!attacker->is_player() || you.allies_forbidden())
        return;

    if (defender->is_peripheral() || defender->is_summoned())
        return;

    if (one_chance_in(10))
    {
        const monster_type demon = random_choose_weighted(
                                       3, MONS_BALRUG,
                                       2, MONS_HELLION,
                                       1, MONS_BRIMSTONE_FIEND);

        mgen_data mg(demon, BEH_FRIENDLY, you.pos(), MHITYOU,
                     MG_FORCE_BEH | MG_AUTOFOE);
        mg.set_summoned(&you, SPELL_FIRE_SUMMON, summ_dur(4));

        if (create_monster(mg))
        {
            mpr("The sceptre summons one of its terrible servants.");
            did_god_conduct(DID_EVIL, 3);
        }
    }
}

////////////////////////////////////////////////////

static void _DOOM_KNIGHT_melee_effects(item_def* /*item*/, actor* attacker,
                                        actor* defender, bool mondied, int /*dam*/)
{
    if (!mondied)
    {
        int bonus_dam = random2avg((1 + defender->stat_maxhp() / 10), 3);
        mprf("%s %s%s",
            defender->name(DESC_THE).c_str(),
            defender->conj_verb("convulse").c_str(),
            attack_strength_punctuation(bonus_dam).c_str());
        defender->hurt(attacker, bonus_dam);
    }
}

///////////////////////////////////////////////////
static void _CHARLATANS_ORB_equip(item_def */*item*/, bool */*show_msgs*/, bool /*unmeld*/)
{
    invalidate_agrid(true);
    calc_hp(true);
    calc_mp(true);
}

static void _CHARLATANS_ORB_unequip(item_def */*item*/, bool */*show_msgs*/)
{
    invalidate_agrid(true);
    calc_hp(true);
    calc_mp(true);
}

/////////////////////////////////////////////////////
static void _SKULL_OF_ZONGULDROK_equip(item_def *item, bool *show_msgs, bool /*unmeld*/)
{
    const bool should_msg = !show_msgs || *show_msgs;
    if (should_msg)
    {
        const string key = !item->props.exists(ZONGULDROK_WELCOME_KEY)
                                ? "zonguldrok greeting"
                                : "zonguldrok reprise";

        const string msg = "A voice whispers, \"" + getSpeakString(key) + "\"";
        mprf(MSGCH_TALK, "%s", msg.c_str());
        item->props[ZONGULDROK_WELCOME_KEY].get_bool() = true;
    }
}

static void _SKULL_OF_ZONGULDROK_unequip(item_def */*item*/, bool *show_msgs)
{
    const bool should_msg = !show_msgs || *show_msgs;
    if (should_msg)
    {
        const string msg = "A voice whispers, \"" +
                            getSpeakString("zonguldrok farewell") + "\"";
        mprf(MSGCH_TALK, "%s", msg.c_str());
    }
}

///////////////////////////////////////////////////
static void _FISTICLOAK_equip(item_def */*item*/, bool *show_msgs, bool unmeld)
{
    if (!unmeld)
    {
        _equip_mpr(show_msgs, getSpeakString("fungus thoughts").c_str());

        if (you_worship(GOD_FEDHAS))
            god_speaks(GOD_FEDHAS, "Fedhas smiles on your commitment to the cycle of life.");
    }
}

static void _FISTICLOAK_unequip(item_def */*item*/, bool *show_msgs)
{
    const bool should_msg = !show_msgs || *show_msgs;
    if (should_msg)
        mpr("Your thoughts feel a little more lonely.");
}

static void _FISTICLOAK_world_reacts(item_def */*item*/)
{
    // First, a chance of flavor message.
    if (one_chance_in(1500))
        mprf(MSGCH_TALK, "%s", getSpeakString("fungus thoughts").c_str());

    // Now, the chance for our shroompunch
    if (!one_chance_in(4))
        return;

    vector<monster*> targs;
    for (adjacent_iterator ai(you.pos()); ai; ++ai)
        if (monster* mon = monster_at(*ai))
            if (you.can_see(*mon) && !mon->wont_attack() && !mon->is_firewood())
                targs.push_back(mon);

    if (targs.empty())
        return;

    monster* targ = targs[random2(targs.size())];

    melee_attack shred(&you, targ);
    shred.player_do_aux_attack(UNAT_FUNGAL_FISTICLOAK);
}

///////////////////////////////////////////////////
static void _VAINGLORY_equip(item_def */*item*/, bool *show_msgs, bool unmeld)
{
    if (!unmeld)
        _equip_mpr(show_msgs, "You feel supremely confident.");
}

static void _VAINGLORY_unequip(item_def */*item*/, bool */*show_msgs*/)
{
    invalidate_agrid(true);
}
