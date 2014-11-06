/**
 * @file
 * @brief Misc stuff.
**/

#include "AppHdr.h"

#include "effects.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <queue>
#include <set>
#include <stdio.h>
#include <string.h>

#include "abyss.h"
#include "act-iter.h"
#include "areas.h"
#include "bloodspatter.h"
#include "branch.h"
#include "cloud.h"
#include "coordit.h"
#include "database.h"
#include "delay.h"
#include "dgnevent.h"
#include "dgn-shoals.h"
#include "dungeon.h"
#include "english.h"
#include "exercise.h"
#include "fight.h"
#include "godabil.h"
#include "godpassive.h"
#include "hiscores.h"
#include "invent.h"
#include "itemprop.h"
#include "items.h"
#include "libutil.h"
#include "losglobal.h"
#include "macro.h"
#include "message.h"
#include "misc.h"
#include "mon-behv.h"
#include "mon-cast.h"
#include "mon-death.h"
#include "mon-pathfind.h"
#include "mon-place.h"
#include "mon-project.h"
#include "mutation.h"
#include "notes.h"
#include "player-stats.h"
#include "prompt.h"
#include "religion.h"
#include "rot.h"
#include "shout.h"
#include "skills.h"
#include "spl-clouds.h"
#include "spl-miscast.h"
#include "spl-summoning.h"
#include "stairs.h"
#include "state.h"
#include "stringutil.h"
#include "terrain.h"
#include "throw.h"
#include "travel.h"
#include "unwind.h"
#include "viewchar.h"
#include "xom.h"

static void _holy_word_player(int pow, holy_word_source_type source, actor *attacker)
{
    if (!you.undead_or_demonic())
        return;

    int hploss;

    // Holy word won't kill its user.
    if (attacker && attacker->is_player())
        hploss = max(0, you.hp / 2 - 1);
    else
        hploss = roll_dice(3, 15) + (random2(pow) / 3);

    if (!hploss)
        return;

    mpr("You are blasted by holy energy!");

    const char *aux = "holy word";

    kill_method_type type = KILLED_BY_SOMETHING;
    if (crawl_state.is_god_acting())
        type = KILLED_BY_DIVINE_WRATH;

    switch (source)
    {
    case HOLY_WORD_SCROLL:
        aux = "scroll of holy word";
        break;

    case HOLY_WORD_ZIN:
        aux = "Zin's holy word";
        break;

    case HOLY_WORD_TSO:
        aux = "the Shining One's holy word";
        break;
    }

    ouch(hploss, type, MID_NOBODY, aux);

    return;
}

void holy_word_monsters(coord_def where, int pow, holy_word_source_type source,
                        actor *attacker)
{
    pow = min(300, pow);

    // Is the player in this cell?
    if (where == you.pos())
        _holy_word_player(pow, source, attacker);

    // Is a monster in this cell?
    monster* mons = monster_at(where);
    if (!mons || !mons->alive() || !mons->undead_or_demonic())
        return;

    int hploss;

    // Holy word won't kill its user.
    if (attacker == mons)
        hploss = max(0, mons->hit_points / 2 - 1);
    else
        hploss = roll_dice(3, 15) + (random2(pow) / 10);

    if (hploss)
        if (source == HOLY_WORD_ZIN)
            simple_monster_message(mons, " is blasted by Zin's holy word!");
        else
            simple_monster_message(mons, " convulses!");
    mons->hurt(attacker, hploss, BEAM_MISSILE);

    if (!hploss || !mons->alive())
        return;
    // Holy word won't annoy or stun its user.
    if (attacker != mons)
    {
        // Currently, holy word annoys the monsters it affects
        // because it can kill them, and because hostile
        // monsters don't use it.
        // Tolerate unknown scroll, to not annoy Yred worshippers too much.
        if (attacker != NULL
            && (attacker != &you
                || source != HOLY_WORD_SCROLL
                || item_type_known(OBJ_SCROLLS, SCR_HOLY_WORD)))
        {
            behaviour_event(mons, ME_ANNOY, attacker);
        }

        if (mons->speed_increment >= 25)
            mons->speed_increment -= 20;
    }
}

void holy_word(int pow, holy_word_source_type source, const coord_def& where,
               bool silent, actor *attacker)
{
    if (!silent && attacker)
    {
        mprf("%s %s a Word of immense power!",
             attacker->name(DESC_THE).c_str(),
             attacker->conj_verb("speak").c_str());
    }

    for (radius_iterator ri(where, LOS_SOLID); ri; ++ri)
        holy_word_monsters(*ri, pow, source, attacker);
}

int torment_player(actor *attacker, int taux)
{
    ASSERT(!crawl_state.game_is_arena());

    int hploss = 0;

    if (!player_res_torment())
    {
        // Negative energy resistance can alleviate torment.
        hploss = max(0, you.hp * (50 - player_prot_life() * 5) / 100 - 1);
        // Statue form is only partial petrification.
        if (you.form == TRAN_STATUE || you.species == SP_GARGOYLE)
            hploss /= 2;

        if (hploss == 0 && player_prot_life() < 0)
            hploss = 1;
    }

    // Kiku protects you from torment to a degree.
    const bool kiku_shielding_player = player_kiku_res_torment();

    if (kiku_shielding_player)
    {
        if (hploss > 0)
        {
            if (random2(600) < you.piety) // 13.33% to 33.33% chance
            {
                hploss = 0;
                simple_god_message(" shields you from torment!");
            }
            else if (random2(250) < you.piety) // 24% to 80% chance
            {
                hploss -= random2(hploss - 1);
                simple_god_message(" partially shields you from torment!");
            }
        }
    }

    if (!hploss)
    {
        mpr("You feel a surge of unholy energy.");
        return 0;
    }

    mpr("Your body is wracked with pain!");

    const char *aux = "by torment";

    kill_method_type type = KILLED_BY_BEAM;
    if (invalid_monster_index(taux))
    {
        type = KILLED_BY_SOMETHING;
        if (crawl_state.is_god_acting())
            type = KILLED_BY_DIVINE_WRATH;

        switch (taux)
        {
        case TORMENT_CARDS:
        case TORMENT_SPELL:
            aux = "Symbol of Torment";
            break;

        case TORMENT_SPWLD:
            // XXX: If we ever make any other weapon / randart eligible
            // to torment, this will be incorrect.
            aux = "Sceptre of Torment";
            break;

        case TORMENT_SCROLL:
            aux = "a scroll of torment";
            break;

        case TORMENT_XOM:
            type = KILLED_BY_XOM;
            aux = "Xom's torment";
            break;

        case TORMENT_KIKUBAAQUDGHA:
            aux = "Kikubaaqudgha's torment";
            break;
        }
    }

    ouch(hploss, type, attacker? attacker->mid : MID_NOBODY, aux);

    return 1;
}

// torment_monsters() is called with power 0 because torment is
// UNRESISTABLE except for having torment resistance!  Even if we used
// maximum power of 1000, high level monsters and characters would save
// too often.  (GDL)

int torment_monsters(coord_def where, actor *attacker, int taux)
{
    int retval = 0;

    // Is the player in this cell?
    if (where == you.pos())
        retval = torment_player(attacker, taux);

    // Is a monster in this cell?
    monster* mons = monster_at(where);
    if (mons == NULL)
        return retval;

    if (!mons->alive() || mons->res_torment())
        return retval;

    int hploss = max(0, mons->hit_points * (50 - mons->res_negative_energy() * 5) / 100 - 1);

    if (!hploss && mons->res_negative_energy() < 0)
        hploss = 1;

    if (hploss)
    {
        simple_monster_message(mons, " convulses!");

        // Currently, torment doesn't annoy the monsters it affects
        // because it can't kill them, and because hostile monsters use
        // it.  It does alert them, though.
        // XXX: attacker isn't passed through "int torment()".
        behaviour_event(mons, ME_ALERT, attacker);
    }

    mons->hurt(attacker, hploss, BEAM_TORMENT_DAMAGE);

    if (hploss)
        retval = 1;

    return retval;
}

int torment(actor *attacker, int taux, const coord_def& where)
{
    int r = 0;
    for (radius_iterator ri(where, LOS_NO_TRANS); ri; ++ri)
        r += torment_monsters(*ri, attacker, taux);
    return r;
}

void cleansing_flame(int pow, int caster, coord_def where,
                     actor *attacker)
{
    ASSERT(!crawl_state.game_is_arena());

    const char *aux = "cleansing flame";

    bolt beam;

    if (caster < 0)
    {
        switch (caster)
        {
        case CLEANSING_FLAME_TSO:
            aux = "the Shining One's cleansing flame";
            break;
        }
    }

    beam.flavour      = BEAM_HOLY;
    beam.glyph        = dchar_glyph(DCHAR_FIRED_BURST);
    beam.damage       = dice_def(2, pow);
    beam.target       = you.pos();
    beam.name         = "golden flame";
    beam.colour       = YELLOW;
    beam.aux_source   = aux;
    beam.ex_size      = 2;
    beam.is_explosion = true;

    if (caster == CLEANSING_FLAME_GENERIC || caster == CLEANSING_FLAME_TSO)
    {
        beam.thrower   = KILL_MISC;
        beam.source_id = MID_NOBODY;
    }
    else if (attacker && attacker->is_player())
    {
        beam.thrower   = KILL_YOU;
        beam.source_id = MID_PLAYER;
    }
    else
    {
        // If there was no attacker, caster should have been
        // CLEANSING_FLAME_{GENERIC,TSO} which we handled above.
        ASSERT(attacker);

        beam.thrower   = KILL_MON;
        beam.source_id = attacker->mid;
    }

    beam.explode();
}

static string _who_banished(const string &who)
{
    return who.empty() ? who : " (" + who + ")";
}

void banished(const string &who)
{
    ASSERT(!crawl_state.game_is_arena());
    push_features_to_abyss();
    if (brdepth[BRANCH_ABYSS] == -1)
        return;

    if (!player_in_branch(BRANCH_ABYSS))
    {
        mark_milestone("abyss.enter",
                       "is cast into the Abyss!" + _who_banished(who));
    }

    if (player_in_branch(BRANCH_ABYSS))
    {
        if (level_id::current().depth < brdepth[BRANCH_ABYSS])
            down_stairs(DNGN_ABYSSAL_STAIR);
        else
        {
            // On Abyss:5 we can't go deeper; cause a shift to a new area
            mprf(MSGCH_BANISHMENT, "You are banished to a different region of the Abyss.");
            abyss_teleport();
        }
        return;
    }

    const string what = "Cast into the Abyss" + _who_banished(who);
    take_note(Note(NOTE_MESSAGE, 0, 0, what.c_str()), true);

    stop_delay(true);
    down_stairs(DNGN_ENTER_ABYSS);  // heh heh

    // Xom just might decide to interfere.
    if (you_worship(GOD_XOM) && who != "Xom" && who != "wizard command"
        && who != "a distortion unwield")
    {
        xom_maybe_reverts_banishment(false, false);
    }
}

void direct_effect(monster* source, spell_type spell,
                   bolt &pbolt, actor *defender)
{
    monster* def = defender->as_monster();

    if (def)
    {
        // annoy the target
        behaviour_event(def, ME_ANNOY, source);
    }

    int damage_taken = 0;

    switch (spell)
    {
    case SPELL_SMITING:
        if (def)
            simple_monster_message(def, " is smitten.");
        else
            mpr("Something smites you!");

        pbolt.name       = "smiting";
        pbolt.flavour    = BEAM_MISSILE;
        pbolt.aux_source = "by divine providence";
        damage_taken     = 7 + random2avg(11, 2);
        break;

    case SPELL_AIRSTRIKE:
        // Damage averages 14 for 5HD, 18 for 10HD, 28 for 20HD, +50% if flying.
        if (def)
            simple_monster_message(def, " is struck by the twisting air!");
        else
        {
            if (you.flight_mode())
                mpr("The air twists around and violently strikes you in flight!");
            else
                mpr("The air twists around and strikes you!");
        }

        pbolt.name       = "airstrike";
        pbolt.flavour    = BEAM_AIR;
        pbolt.aux_source = "by the air";

        damage_taken     = 10 + 2 * source->get_hit_dice();

        damage_taken = defender->beam_resists(pbolt, damage_taken, false);

        // Previous method of damage calculation (in line with player
        // airstrike) had absurd variance.
        damage_taken = defender->apply_ac(random2avg(damage_taken, 3));
        break;

    case SPELL_WATERSTRIKE:
        if (feat_is_water(grd(defender->pos())))
        {
            if (defender->is_player() || you.can_see(defender))
            {
                if (defender->flight_mode())
                    mprf("The water rises up and strikes %s!", defender->name(DESC_THE).c_str());
                else
                    mprf("The water swirls and strikes %s!", defender->name(DESC_THE).c_str());
            }

            pbolt.name       = "waterstrike";
            pbolt.flavour    = BEAM_WATER;
            pbolt.aux_source = "by the raging water";

            damage_taken     = roll_dice(3, 7 + source->get_hit_dice());

            damage_taken = defender->beam_resists(pbolt, damage_taken, false);
            damage_taken = defender->apply_ac(damage_taken);
        }
        break;

    case SPELL_BRAIN_FEED:
        if (!def)
        {
            // lose_stat() must come last {dlb}
            if (one_chance_in(3)
                && lose_stat(STAT_INT, 1 + random2(3), source))
            {
                mpr("Something feeds on your intellect!");
                xom_is_stimulated(50);
            }
            else
                mpr("Something tries to feed on your intellect!");
        }
        break;

    case SPELL_HAUNT:
        if (!def)
            mpr("You feel haunted.");
        else
            mpr("You sense an evil presence.");
        mons_cast_haunt(source);
        break;

    case SPELL_SUMMON_SPECTRAL_ORCS:
        if (def)
            simple_monster_message(def, " is surrounded by Orcish apparitions.");
        else
            mpr("Orcish apparitions take form around you.");
        mons_cast_spectral_orcs(source);
        break;

    case SPELL_HOLY_FLAMES:
        if (holy_flames(source, defender))
        {
            if (!def)
                mpr("Blessed fire suddenly surrounds you!");
            else
                simple_monster_message(def, " is surrounded by blessed fire!");
        }
        break;

    case SPELL_CHAOTIC_MIRROR:
        if (x_chance_in_y(4, 10))
        {
            pbolt.name = "reflection of chaos";
            pbolt.source_id = source->mid;
            pbolt.aux_source = "chaotic mirror";
            pbolt.hit = AUTOMATIC_HIT;
            pbolt.is_beam = true;
            pbolt.ench_power = MAG_IMMUNE;
            pbolt.real_flavour = BEAM_CHAOTIC_REFLECTION;
            pbolt.fake_flavour();
            pbolt.real_flavour = pbolt.flavour;
            pbolt.damage = dice_def(1, 6);
            pbolt.use_target_as_pos = true;
            pbolt.source = pbolt.target = defender->pos();
            pbolt.affect_actor(defender);
            pbolt.source = pbolt.target = source->pos();
            pbolt.affect_actor(source);
        }
        else if (!def || you.can_see(def))
            canned_msg(MSG_NOTHING_HAPPENS);

        break;

    case SPELL_FLAY:
    {
        bool was_flayed = false;
        if (defender->is_player())
        {
            damage_taken = (6 + (you.hp * 18 / you.hp_max)) * you.hp_max / 100;
            damage_taken = min(damage_taken,
                               max(0, you.hp - 25 - random2(15)));
            if (damage_taken < 10)
                return;

            if (you.duration[DUR_FLAYED])
                was_flayed = true;

            you.duration[DUR_FLAYED] = max(you.duration[DUR_FLAYED],
                                           55 + random2(66));
        }
        else
        {
            monster* mon = defender->as_monster();

            damage_taken = (6 + (mon->hit_points * 18 / mon->max_hit_points))
                           * mon->max_hit_points / 100;
            damage_taken = min(damage_taken,
                               max(0, mon->hit_points - 25 - random2(15)));
            if (damage_taken < 10)
                return;

            if (mon->has_ench(ENCH_FLAYED))
            {
                was_flayed = true;
                mon_enchant flayed = mon->get_ench(ENCH_FLAYED);
                flayed.duration = min(flayed.duration + 30 + random2(50), 150);
                mon->update_ench(flayed);
            }
            else
            {
                mon_enchant flayed(ENCH_FLAYED, 1, source, 30 + random2(50));
                mon->add_ench(flayed);
            }
        }

        if (you.can_see(defender))
        {
            if (was_flayed)
            {
                mprf("Terrible wounds spread across more of %s body!",
                     defender->name(DESC_ITS).c_str());
            }
            else
            {
                mprf("Terrible wounds open up all over %s body!",
                     defender->name(DESC_ITS).c_str());
            }
        }

        if (defender->is_player())
        {
            // Bypassing ::hurt so that flay damage can ignore guardian spirit
            ouch(damage_taken, KILLED_BY_MONSTER, source->mid,
                 "flay_damage", you.can_see(source));
        }
        else
            defender->hurt(source, damage_taken, BEAM_NONE, true);
        defender->props["flay_damage"].get_int() += damage_taken;

        vector<coord_def> old_blood;
        CrawlVector &new_blood = defender->props["flay_blood"].get_vector();

        // Find current blood spatters
        for (radius_iterator ri(defender->pos(), LOS_SOLID); ri; ++ri)
        {
            if (env.pgrid(*ri) & FPROP_BLOODY)
                old_blood.push_back(*ri);
        }

        blood_spray(defender->pos(), defender->type, 20);

        // Compute and store new blood spatters
        unsigned int i = 0;
        for (radius_iterator ri(defender->pos(), LOS_SOLID); ri; ++ri)
        {
            if (env.pgrid(*ri) & FPROP_BLOODY)
            {
                if (i < old_blood.size() && old_blood[i] == *ri)
                    ++i;
                else
                    new_blood.push_back(*ri);
            }
        }
        damage_taken = 0;
        break;
    }

    case SPELL_PARALYSIS_GAZE:
        defender->paralyse(source, 2 + random2(3));
        break;

    case SPELL_CONFUSION_GAZE:
    {
        int confuse_power = 2 + random2(3);
        int res_margin =
            defender->check_res_magic(5 * source->get_experience_level()
                                      * confuse_power);
        if (res_margin > 0)
        {
            if (defender->is_player())
                canned_msg(MSG_YOU_RESIST);
            else // if (defender->is_monster())
            {
                const monster* foe = defender->as_monster();
                simple_monster_message(foe,
                                       mons_resist_string(foe, res_margin));
            }
            break;
        }

        defender->confuse(source, 2 + random2(3));
        break;
    }

    case SPELL_DRAINING_GAZE:
        enchant_actor_with_flavour(defender, source, BEAM_DRAIN_MAGIC,
                                   defender->get_experience_level() * 12);
        break;

    default:
        die("unknown direct_effect spell: %d", spell);
    }

    // apply damage and handle death, where appropriate {dlb}
    if (damage_taken > 0)
    {
        if (def)
            def->hurt(source, damage_taken);
        else
            ouch(damage_taken, KILLED_BY_BEAM, pbolt.source_id,
                 pbolt.aux_source.c_str());
    }
}

void random_uselessness(int scroll_slot)
{
    ASSERT(!crawl_state.game_is_arena());

    int temp_rand = random2(8);

    // If this isn't from a scroll, skip the first two possibilities.
    if (scroll_slot == -1)
        temp_rand = 2 + random2(6);

    switch (temp_rand)
    {
    case 0:
        mprf("The dust glows %s!", weird_glowing_colour().c_str());
        break;

    case 1:
        mprf("The scroll reassembles itself in your %s!",
             you.hand_name(true).c_str());
        inc_inv_item_quantity(scroll_slot, 1);
        break;

    case 2:
        if (you.weapon())
        {
            mprf("%s glows %s for a moment.",
                 you.weapon()->name(DESC_YOUR).c_str(),
                 weird_glowing_colour().c_str());
        }
        else
        {
            mprf("Your %s glow %s for a moment.",
                 you.hand_name(true).c_str(), weird_glowing_colour().c_str());
        }
        break;

    case 3:
        if (you.species == SP_MUMMY)
            mpr("Your bandages flutter.");
        else // if (you.can_smell())
            mprf("You smell %s.", weird_smell().c_str());
        break;

    case 4:
        mpr("You experience a momentary feeling of inescapable doom!");
        break;

    case 5:
        if (player_mutation_level(MUT_BEAK) || one_chance_in(3))
            mpr("Your brain hurts!");
        else if (you.species == SP_MUMMY || coinflip())
            mpr("Your ears itch!");
        else
            mpr("Your nose twitches suddenly!");
        break;

    case 6:
        mprf(MSGCH_SOUND, "You hear the tinkle of a tiny bell.");
        noisy(2, you.pos());
        cast_summon_butterflies(100);
        break;

    case 7:
        mprf(MSGCH_SOUND, "You hear %s.", weird_sound().c_str());
        noisy(2, you.pos());
        break;
    }
}

int recharge_wand(bool known, string *pre_msg)
{
    int item_slot = -1;
    do
    {
        if (item_slot == -1)
        {
            item_slot = prompt_invent_item("Charge which item?", MT_INVLIST,
                                            OSEL_RECHARGE, true, true, false);
        }

        if (item_slot == PROMPT_NOTHING)
            return known ? -1 : 0;

        if (item_slot == PROMPT_ABORT)
        {
            if (known
                || crawl_state.seen_hups
                || yesno("Really abort (and waste the scroll)?", false, 0))
            {
                canned_msg(MSG_OK);
                return known ? -1 : 0;
            }
            else
            {
                item_slot = -1;
                continue;
            }
        }

        item_def &wand = you.inv[ item_slot ];

        if (!item_is_rechargeable(wand, known))
        {
            mpr("Choose an item to recharge, or Esc to abort.");
            more();

            // Try again.
            item_slot = -1;
            continue;
        }

        if (wand.base_type != OBJ_WANDS && wand.base_type != OBJ_RODS)
            return 0;

        if (wand.base_type == OBJ_WANDS)
        {
            int charge_gain = wand_charge_value(wand.sub_type);

            const int new_charges =
                max<int>(wand.charges,
                         min(charge_gain * 3,
                             wand.charges +
                             1 + random2avg(((charge_gain - 1) * 3) + 1, 3)));

            const bool charged = (new_charges > wand.plus);

            string desc;

            if (charged && item_ident(wand, ISFLAG_KNOW_PLUSES))
            {
                snprintf(info, INFO_SIZE, " and now has %d charge%s",
                         new_charges, new_charges == 1 ? "" : "s");
                desc = info;
            }

            if (known && pre_msg)
                mpr(pre_msg->c_str());

            mprf("%s %s for a moment%s.",
                 wand.name(DESC_YOUR).c_str(),
                 charged ? "glows" : "flickers",
                 desc.c_str());

            if (!charged && !item_ident(wand, ISFLAG_KNOW_PLUSES))
            {
                mprf("It has %d charges and is fully charged.", new_charges);
                set_ident_flags(wand, ISFLAG_KNOW_PLUSES);
            }

            // Reinitialise zap counts.
            wand.charges  = new_charges;
            wand.used_count = ZAPCOUNT_RECHARGED;
        }
        else // It's a rod.
        {
            bool work = false;

            if (wand.charge_cap < MAX_ROD_CHARGE * ROD_CHARGE_MULT)
            {
                wand.charge_cap += ROD_CHARGE_MULT * random_range(1, 2);

                if (wand.charge_cap > MAX_ROD_CHARGE * ROD_CHARGE_MULT)
                    wand.charge_cap = MAX_ROD_CHARGE * ROD_CHARGE_MULT;

                work = true;
            }

            if (wand.charges < wand.charge_cap)
            {
                wand.charges = wand.charge_cap;
                work = true;
            }

            if (wand.special < MAX_WPN_ENCHANT)
            {
                wand.rod_plus += random_range(1, 2);
                if (wand.rod_plus > MAX_WPN_ENCHANT)
                    wand.rod_plus = MAX_WPN_ENCHANT;

                work = true;
            }

            if (!work)
                return 0;

            if (known && pre_msg)
                mpr(pre_msg->c_str());

            mprf("%s glows for a moment.", wand.name(DESC_YOUR).c_str());
        }

        you.wield_change = true;
        return 1;
    }
    while (true);

    return 0;
}

// Berserking monsters cannot be ordered around.
static bool _follows_orders(monster* mon)
{
    return mon->friendly()
           && mon->type != MONS_GIANT_SPORE
           && !mon->berserk_or_insane()
           && !mons_is_conjured(mon->type)
           && !mon->has_ench(ENCH_HAUNTING);
}

// Sets foe target of friendly monsters.
// If allow_patrol is true, patrolling monsters get MHITNOT instead.
static void _set_friendly_foes(bool allow_patrol = false)
{
    for (monster_near_iterator mi(you.pos()); mi; ++mi)
    {
        if (!_follows_orders(*mi))
            continue;
        if (you.pet_target != MHITNOT && mi->behaviour == BEH_WITHDRAW)
        {
            mi->behaviour = BEH_SEEK;
            mi->patrol_point = coord_def(0, 0);
        }
        mi->foe = (allow_patrol && mi->is_patrolling() ? MHITNOT
                                                         : you.pet_target);
    }
}

static void _set_allies_patrol_point(bool clear = false)
{
    for (monster_near_iterator mi(you.pos()); mi; ++mi)
    {
        if (!_follows_orders(*mi))
            continue;
        mi->patrol_point = (clear ? coord_def(0, 0) : mi->pos());
        if (!clear)
            mi->behaviour = BEH_WANDER;
        else
            mi->behaviour = BEH_SEEK;
    }
}

static void _set_allies_withdraw(const coord_def &target)
{
    coord_def delta = target - you.pos();
    float mult = float(LOS_RADIUS * 2) / (float)max(abs(delta.x), abs(delta.y));
    coord_def rally_point = clamp_in_bounds(coord_def(delta.x * mult, delta.y * mult) + you.pos());

    for (monster_near_iterator mi(you.pos()); mi; ++mi)
    {
        if (!_follows_orders(*mi))
            continue;
        if (mons_class_flag(mi->type, M_STATIONARY))
            continue;
        mi->behaviour = BEH_WITHDRAW;
        mi->target = clamp_in_bounds(target);
        mi->patrol_point = rally_point;
        mi->foe = MHITNOT;

        mi->props.erase("last_pos");
        mi->props.erase("idle_point");
        mi->props.erase("idle_deadline");
        mi->props.erase("blocked_deadline");
    }
}

void yell(const actor* mon)
{
    ASSERT(!crawl_state.game_is_arena());

    bool targ_prev = false;
    int mons_targd = MHITNOT;
    dist targ;

    const string shout_verb = you.shout_verb(mon != NULL);
    string cap_shout = shout_verb;
    cap_shout[0] = toupper(cap_shout[0]);
    const int noise_level = you.shout_volume();

    if (you.cannot_speak())
    {
        if (mon)
        {
            if (you.paralysed() || you.duration[DUR_WATER_HOLD])
            {
                mprf("You feel a strong urge to %s, but "
                     "you are unable to make a sound!",
                     shout_verb.c_str());
            }
            else
            {
                mprf("You feel a %s rip itself from your throat, "
                     "but you make no sound!",
                     shout_verb.c_str());
            }
        }
        else
            mpr("You are unable to make a sound!");

        return;
    }

    if (mon)
    {
        mprf("You %s%s at %s!",
             shout_verb.c_str(),
             you.duration[DUR_RECITE] ? " your recitation" : "",
             mon->name(DESC_THE).c_str());
        noisy(noise_level, you.pos());
        return;
    }

    mprf(MSGCH_PROMPT, "What do you say?");
    mprf(" t - %s!", cap_shout.c_str());

    if (!you.berserk())
    {
        string previous;
        if (!(you.prev_targ == MHITNOT || you.prev_targ == MHITYOU))
        {
            const monster* target = &menv[you.prev_targ];
            if (target->alive() && you.can_see(target))
            {
                previous = "   p - Attack previous target.";
                targ_prev = true;
            }
        }

        mprf("Orders for allies: a - Attack new target.%s", previous.c_str());
        mpr("                   r - Retreat!             s - Stop attacking.");
        mpr("                   w - Wait here.           f - Follow me.");
    }
    mpr(" Anything else - Stay silent.");

    int keyn = get_ch();
    clear_messages();

    switch (keyn)
    {
    case '!':    // for players using the old keyset
    case 't':
        mprf(MSGCH_SOUND, "You %s%s!",
             shout_verb.c_str(),
             you.berserk() ? " wildly" : " for attention");
        noisy(noise_level, you.pos());
        zin_recite_interrupt();
        you.turn_is_over = true;
        return;

    case 'f':
    case 's':
        if (you.berserk())
        {
            canned_msg(MSG_TOO_BERSERK);
            return;
        }

        mons_targd = MHITYOU;
        if (keyn == 'f')
        {
            // Don't reset patrol points for 'Stop fighting!'
            _set_allies_patrol_point(true);
            mpr("Follow me!");
        }
        else
            mpr("Stop fighting!");
        break;

    case 'w':
        if (you.berserk())
        {
            canned_msg(MSG_TOO_BERSERK);
            return;
        }

        mpr("Wait here!");
        mons_targd = MHITNOT;
        _set_allies_patrol_point();
        break;

    case 'p':
        if (you.berserk())
        {
            canned_msg(MSG_TOO_BERSERK);
            return;
        }

        if (targ_prev)
        {
            mons_targd = you.prev_targ;
            break;
        }

    // fall through
    case 'a':
        if (you.berserk())
        {
            canned_msg(MSG_TOO_BERSERK);
            return;
        }

        if (env.sanctuary_time > 0)
        {
            if (!yesno("An ally attacking under your orders might violate "
                       "sanctuary; order anyway?", false, 'n'))
            {
                canned_msg(MSG_OK);
                return;
            }
        }

        {
            direction_chooser_args args;
            args.restricts = DIR_TARGET;
            args.mode = TARG_HOSTILE;
            args.needs_path = false;
            args.top_prompt = "Gang up on whom?";
            direction(targ, args);
        }

        if (targ.isCancel)
        {
            canned_msg(MSG_OK);
            return;
        }

        {
            bool cancel = !targ.isValid;
            if (!cancel)
            {
                const monster* m = monster_at(targ.target);
                cancel = (m == NULL || !you.can_see(m));
                if (!cancel)
                    mons_targd = m->mindex();
            }

            if (cancel)
            {
                canned_msg(MSG_NOTHING_THERE);
                return;
            }
        }
        break;

    case 'r':
        if (you.berserk())
        {
            canned_msg(MSG_TOO_BERSERK);
            return;
        }

        {
            direction_chooser_args args;
            args.restricts = DIR_TARGET;
            args.mode = TARG_ANY;
            args.needs_path = false;
            args.top_prompt = "Retreat in which direction?";
            direction(targ, args);
        }

        if (targ.isCancel)
        {
            canned_msg(MSG_OK);
            return;
        }

        if (targ.isValid)
        {
            mpr("Fall back!");
            mons_targd = MHITNOT;
        }

        _set_allies_withdraw(targ.target);
        break;

    default:
        canned_msg(MSG_OK);
        return;
    }

    zin_recite_interrupt();
    you.turn_is_over = true;
    you.pet_target = mons_targd;
    // Allow patrolling for "Stop fighting!" and "Wait here!"
    _set_friendly_foes(keyn == 's' || keyn == 'w');

    if (mons_targd != MHITNOT && mons_targd != MHITYOU)
        mpr("Attack!");

    noisy(noise_level, you.pos());
}

static inline dungeon_feature_type _vitrified_feature(dungeon_feature_type feat)
{
    switch (feat)
    {
    case DNGN_ROCK_WALL:
        return DNGN_CLEAR_ROCK_WALL;
    case DNGN_STONE_WALL:
        return DNGN_CLEAR_STONE_WALL;
    case DNGN_PERMAROCK_WALL:
        return DNGN_CLEAR_PERMAROCK_WALL;
    default:
        return feat;
    }
}

// Returns true if there was a visible change.
bool vitrify_area(int radius)
{
    if (radius < 2)
        return false;

    bool something_happened = false;
    for (radius_iterator ri(you.pos(), radius, C_POINTY); ri; ++ri)
    {
        const dungeon_feature_type grid = grd(*ri);
        const dungeon_feature_type newgrid = _vitrified_feature(grid);
        if (newgrid != grid)
        {
            grd(*ri) = newgrid;
            set_terrain_changed(*ri);
            something_happened = true;
        }
    }
    return something_happened;
}

// Nasty things happen to people who spend too long in Hell.
static void _hell_effects(int time_delta)
{
    UNUSED(time_delta);
    if (!player_in_hell())
        return;

    // 50% chance at max piety
    if (you_worship(GOD_ZIN) && x_chance_in_y(you.piety, MAX_PIETY * 2)
        || is_sanctuary(you.pos()))
    {
        simple_god_message("'s power protects you from the chaos of Hell!");
        return;
    }

    string msg = getMiscString("hell_effect");
    if (msg.empty())
        msg = "Something hellishly buggy happens.";
    bool loud = starts_with(msg, "SOUND:");
    if (loud)
        msg.erase(0, 6);
    mprf(MSGCH_HELL_EFFECT, "%s", msg.c_str());
    if (loud)
        noisy(15, you.pos());

    spschool_flag_type which_miscast = SPTYP_RANDOM;

    int temp_rand = random2(27);
    if (temp_rand > 17)     // 9 in 27 odds {dlb}
    {
        temp_rand = random2(8);

        if (temp_rand > 3)  // 4 in 8 odds {dlb}
            which_miscast = SPTYP_NECROMANCY;
        else if (temp_rand > 1)     // 2 in 8 odds {dlb}
            which_miscast = SPTYP_SUMMONING;
        else if (temp_rand > 0)     // 1 in 8 odds {dlb}
            which_miscast = SPTYP_CONJURATION;
        else                // 1 in 8 odds {dlb}
            which_miscast = coinflip() ? SPTYP_HEXES : SPTYP_CHARMS;

        MiscastEffect(&you, HELL_EFFECT_MISCAST, which_miscast,
                      4 + random2(6), random2avg(97, 3),
                      "the effects of Hell");
    }
    else if (temp_rand > 7) // 10 in 27 odds {dlb}
    {
        monster_type which_beastie;

        // 60:40 miscast:summon split {dlb}
        switch (you.where_are_you)
        {
        case BRANCH_DIS:
            which_beastie = RANDOM_DEMON_GREATER;
            which_miscast = SPTYP_EARTH;
            break;

        case BRANCH_GEHENNA:
            which_beastie = MONS_BRIMSTONE_FIEND;
            which_miscast = SPTYP_FIRE;
            break;

        case BRANCH_COCYTUS:
            which_beastie = MONS_ICE_FIEND;
            which_miscast = SPTYP_ICE;
            break;

        case BRANCH_TARTARUS:
            which_beastie = MONS_SHADOW_FIEND;
            which_miscast = SPTYP_NECROMANCY;
            break;

        default:
            die("unknown hell branch");
        }

        if (x_chance_in_y(2, 5))
        {
            create_monster(
                mgen_data::hostile_at(which_beastie, "the effects of Hell",
                    true, 0, 0, you.pos()));
        }
        else
        {
            MiscastEffect(&you, HELL_EFFECT_MISCAST, which_miscast,
                          4 + random2(6), random2avg(97, 3),
                          "the effects of Hell");
        }
    }

    // NB: No "else" - 8 in 27 odds that nothing happens through
    //                 first chain. {dlb}
    // Also note that the following is distinct from and in
    // addition to the above chain.

    // Try to summon at least one and up to five random monsters. {dlb}
    if (one_chance_in(3))
    {
        mgen_data mg;
        mg.pos = you.pos();
        mg.foe = MHITYOU;
        mg.non_actor_summoner = "the effects of Hell";
        create_monster(mg);

        for (int i = 0; i < 4; ++i)
            if (one_chance_in(3))
                create_monster(mg);
    }
}

// This function checks whether we can turn a wall into a floor space and
// still keep a corridor-like environment. The wall in position x is a
// a candidate for switching if it's flanked by floor grids to two sides
// and by walls (any type) to the remaining cardinal directions.
//
//   .        #          2
//  #x#  or  .x.   ->   0x1
//   .        #          3
static bool _feat_is_flanked_by_walls(const coord_def &p)
{
    const coord_def adjs[] = { coord_def(p.x-1,p.y),
                               coord_def(p.x+1,p.y),
                               coord_def(p.x  ,p.y-1),
                               coord_def(p.x  ,p.y+1) };

    // paranoia!
    for (unsigned int i = 0; i < ARRAYSZ(adjs); ++i)
        if (!in_bounds(adjs[i]))
            return false;

    return feat_is_wall(grd(adjs[0])) && feat_is_wall(grd(adjs[1]))
              && feat_has_solid_floor(grd(adjs[2])) && feat_has_solid_floor(grd(adjs[3]))
           || feat_has_solid_floor(grd(adjs[0])) && feat_has_solid_floor(grd(adjs[1]))
              && feat_is_wall(grd(adjs[2])) && feat_is_wall(grd(adjs[3]));
}

// Sometimes if a floor is turned into a wall, a dead-end will be created.
// If this is the case, we need to make sure that it is at least two grids
// deep.
//
// Example: If a wall is built at X (A), two dead-ends are created, a short
//          and a long one. The latter is perfectly fine, but the former
//          looks a bit odd. If Y is chosen, this looks much better (B).
//
// #######    (A)  #######    (B)  #######
// ...XY..         ...#...         ....#..
// #.#####         #.#####         #.#####
//
// What this function does is check whether the neighbouring floor grids
// are flanked by walls on both sides, and if so, the grids following that
// also have to be floor flanked by walls.
//
//   czd
//   a.b   -> if (a, b == walls) then (c, d == walls) or return false
//   #X#
//    .
//
// Grid z may be floor or wall, either way we have a corridor of at least
// length 2.
static bool _deadend_check_wall(const coord_def &p)
{
    // The grids to the left and right of p are walls. (We already know that
    // they are symmetric, so only need to check one side. We also know that
    // the other direction, here up/down must then be non-walls.)
    if (feat_is_wall(grd[p.x-1][p.y]))
    {
        // Run the check twice, once in either direction.
        for (int i = -1; i <= 1; i++)
        {
            if (i == 0)
                continue;

            const coord_def a(p.x-1, p.y+i);
            const coord_def b(p.x+1, p.y+i);
            const coord_def c(p.x-1, p.y+2*i);
            const coord_def d(p.x+1, p.y+2*i);

            if (in_bounds(a) && in_bounds(b)
                && feat_is_wall(grd(a)) && feat_is_wall(grd(b))
                && (!in_bounds(c) || !in_bounds(d)
                    || !feat_is_wall(grd(c)) || !feat_is_wall(grd(d))))
            {
                return false;
            }
        }
    }
    else // The grids above and below p are walls.
    {
        for (int i = -1; i <= 1; i++)
        {
            if (i == 0)
                continue;

            const coord_def a(p.x+i  , p.y-1);
            const coord_def b(p.x+i  , p.y+1);
            const coord_def c(p.x+2*i, p.y-1);
            const coord_def d(p.x+2*i, p.y+1);

            if (in_bounds(a) && in_bounds(b)
                && feat_is_wall(grd(a)) && feat_is_wall(grd(b))
                && (!in_bounds(c) || !in_bounds(d)
                    || !feat_is_wall(grd(c)) || !feat_is_wall(grd(d))))
            {
                return false;
            }
        }
    }

    return true;
}

// Similar to the above, checks whether turning a wall grid into floor
// would create a short "dead-end" of only 1 grid.
//
// In the example below, X would create miniature dead-ends at positions
// a and b, but both Y and Z avoid this, and the resulting mini-mazes
// look much better.
//
// ########   (A)  ########     (B)  ########     (C)  ########
// #.....#.        #....a#.          #.....#.          #.....#.
// #.#YXZ#.        #.##.##.          #.#.###.          #.###.#.
// #.#.....        #.#b....          #.#.....          #.#.....
//
// In general, if a floor grid horizontally or vertically adjacent to the
// change target has a floor neighbour diagonally adjacent to the change
// target, the next neighbour in the same direction needs to be floor,
// as well.
static bool _deadend_check_floor(const coord_def &p)
{
    if (feat_is_wall(grd[p.x-1][p.y]))
    {
        for (int i = -1; i <= 1; i++)
        {
            if (i == 0)
                continue;

            const coord_def a(p.x, p.y+2*i);
            if (!in_bounds(a) || feat_has_solid_floor(grd(a)))
                continue;

            for (int j = -1; j <= 1; j++)
            {
                if (j == 0)
                    continue;

                const coord_def b(p.x+2*j, p.y+i);
                if (!in_bounds(b))
                    continue;

                const coord_def c(p.x+j, p.y+i);
                if (feat_has_solid_floor(grd(c)) && !feat_has_solid_floor(grd(b)))
                    return false;
            }
        }
    }
    else
    {
        for (int i = -1; i <= 1; i++)
        {
            if (i == 0)
                continue;

            const coord_def a(p.x+2*i, p.y);
            if (!in_bounds(a) || feat_has_solid_floor(grd(a)))
                continue;

            for (int j = -1; j <= 1; j++)
            {
                if (j == 0)
                    continue;

                const coord_def b(p.x+i, p.y+2*j);
                if (!in_bounds(b))
                    continue;

                const coord_def c(p.x+i, p.y+j);
                if (feat_has_solid_floor(grd(c)) && !feat_has_solid_floor(grd(b)))
                    return false;
            }
        }
    }

    return true;
}

// Changes a small portion of a labyrinth by exchanging wall against floor
// grids in such a way that connectivity remains guaranteed.
void change_labyrinth(bool msg)
{
    int size = random_range(12, 24); // size of the shifted area (square)
    coord_def c1, c2; // upper left, lower right corners of the shifted area

    vector<coord_def> targets;

    // Try 10 times for an area that is little mapped.
    for (int tries = 10; tries > 0; --tries)
    {
        targets.clear();

        int x = random_range(LABYRINTH_BORDER, GXM - LABYRINTH_BORDER - size);
        int y = random_range(LABYRINTH_BORDER, GYM - LABYRINTH_BORDER - size);
        c1 = coord_def(x, y);
        c2 = coord_def(x + size, y + size);

        int count_known = 0;
        for (rectangle_iterator ri(c1, c2); ri; ++ri)
            if (env.map_knowledge(*ri).seen())
                count_known++;

        if (tries > 1 && count_known > size * size / 6)
            continue;

        // Fill a vector with wall grids that are potential targets for
        // swapping against floor, i.e. are flanked by walls to two cardinal
        // directions, and by floor on the two remaining sides.
        for (rectangle_iterator ri(c1, c2); ri; ++ri)
        {
            if (env.map_knowledge(*ri).seen() || !feat_is_wall(grd(*ri)))
                continue;

            // Skip on grids inside vaults so as not to disrupt them.
            if (map_masked(*ri, MMT_VAULT))
                continue;

            // Make sure we don't accidentally create "ugly" dead-ends.
            if (_feat_is_flanked_by_walls(*ri) && _deadend_check_floor(*ri))
                targets.push_back(*ri);
        }

        if (targets.size() >= 8)
            break;
    }

    if (targets.empty())
    {
        if (msg)
            mpr("No unexplored wall grids found!");
        return;
    }

    if (msg)
    {
        mprf(MSGCH_DIAGNOSTICS, "Changing labyrinth from (%d, %d) to (%d, %d)",
             c1.x, c1.y, c2.x, c2.y);

        string path_str = "";
        mprf(MSGCH_DIAGNOSTICS, "Here's the list of targets: ");
        for (unsigned int i = 0; i < targets.size(); i++)
        {
            snprintf(info, INFO_SIZE, "(%d, %d)  ", targets[i].x, targets[i].y);
            path_str += info;
        }
        mprf(MSGCH_DIAGNOSTICS, "%s", path_str.c_str());
        mprf(MSGCH_DIAGNOSTICS, "-> #targets = %u", (unsigned int)targets.size());
    }

#ifdef WIZARD
    // Remove old highlighted areas to make place for the new ones.
    if (you.wizard)
        for (rectangle_iterator ri(1); ri; ++ri)
            env.pgrid(*ri) &= ~(FPROP_HIGHLIGHT);
#endif

    // How many switches we'll be doing.
    const int max_targets = random_range(min((int) targets.size(), 12),
                                         min((int) targets.size(), 45));

    // Shuffle the targets, then pick the max_targets first ones.
    shuffle_array(targets);

    // For each of the chosen wall grids, calculate the path connecting the
    // two floor grids to either side, and block off one floor grid on this
    // path to close the circle opened by turning the wall into floor.
    for (int count = 0; count < max_targets; count++)
    {
        const coord_def c(targets[count]);
        // Maybe not valid anymore...
        if (!feat_is_wall(grd(c)) || !_feat_is_flanked_by_walls(c))
            continue;

        // Use the adjacent floor grids as source and destination.
        coord_def src(c.x-1,c.y);
        coord_def dst(c.x+1,c.y);
        if (!feat_has_solid_floor(grd(src)) || !feat_has_solid_floor(grd(dst)))
        {
            src = coord_def(c.x, c.y-1);
            dst = coord_def(c.x, c.y+1);
        }

        // Pathfinding from src to dst...
        monster_pathfind mp;
        bool success = mp.init_pathfind(src, dst, false, msg);
        if (!success)
        {
            if (msg)
                mprf(MSGCH_DIAGNOSTICS, "Something went badly wrong - no path found!");
            continue;
        }

        // Get the actual path.
        const vector<coord_def> path = mp.backtrack();

        // Replace the wall with floor, but preserve the old grid in case
        // we find no floor grid to swap with.
        // It's better if the change is done now, so the grid can be
        // treated as floor rather than a wall, and we don't need any
        // special cases.
        dungeon_feature_type old_grid = grd(c);
        grd(c) = DNGN_FLOOR;
        set_terrain_changed(c);

        // Add all floor grids meeting a couple of conditions to a vector
        // of potential switch points.
        vector<coord_def> points;
        for (unsigned int i = 0; i < path.size(); i++)
        {
            const coord_def p(path[i]);
            // The point must be inside the changed area.
            if (p.x < c1.x || p.x > c2.x || p.y < c1.y || p.y > c2.y)
                continue;

            // Only replace plain floor.
            if (grd(p) != DNGN_FLOOR)
                continue;

            // Don't change any grids we remember.
            if (env.map_knowledge(p).seen())
                continue;

            // We don't want to deal with monsters being shifted around.
            if (monster_at(p))
                continue;

            // Do not pick a grid right next to the original wall.
            if (abs(p.x-c.x) + abs(p.y-c.y) <= 1)
                continue;

            if (_feat_is_flanked_by_walls(p) && _deadend_check_wall(p))
                points.push_back(p);
        }

        if (points.empty())
        {
            // Take back the previous change.
            grd(c) = old_grid;
            set_terrain_changed(c);
            continue;
        }

        // Randomly pick one floor grid from the vector and replace it
        // with an adjacent wall type.
        const int pick = random_range(0, (int) points.size() - 1);
        const coord_def p(points[pick]);
        if (msg)
        {
            mprf(MSGCH_DIAGNOSTICS, "Switch %d (%d, %d) with %d (%d, %d).",
                 (int) old_grid, c.x, c.y, (int) grd(p), p.x, p.y);
        }
#ifdef WIZARD
        if (you.wizard)
        {
            // Highlight the switched grids.
            env.pgrid(c) |= FPROP_HIGHLIGHT;
            env.pgrid(p) |= FPROP_HIGHLIGHT;
        }
#endif

        // Shift blood some of the time.
        if (is_bloodcovered(c))
        {
            if (one_chance_in(4))
            {
                int wall_count = 0;
                coord_def old_adj(c);
                for (adjacent_iterator ai(c); ai; ++ai)
                    if (feat_is_wall(grd(*ai)) && one_chance_in(++wall_count))
                        old_adj = *ai;

                if (old_adj != c && maybe_bloodify_square(old_adj))
                    env.pgrid(c) &= (~FPROP_BLOODY);
            }
        }
        else if (one_chance_in(500))
        {
            // Rarely add blood randomly, accumulating with time...
            maybe_bloodify_square(c);
        }

        // Rather than use old_grid directly, replace with an adjacent
        // wall type, preferably stone, rock, or metal.
        old_grid = grd[p.x-1][p.y];
        if (!feat_is_wall(old_grid))
        {
            old_grid = grd[p.x][p.y-1];
            if (!feat_is_wall(old_grid))
            {
                if (msg)
                {
                    mprf(MSGCH_DIAGNOSTICS,
                         "No adjacent walls at pos (%d, %d)?", p.x, p.y);
                }
                old_grid = DNGN_STONE_WALL;
            }
            else if (old_grid != DNGN_ROCK_WALL && old_grid != DNGN_STONE_WALL
                     && old_grid != DNGN_METAL_WALL && !one_chance_in(3))
            {
                old_grid = grd[p.x][p.y+1];
            }
        }
        else if (old_grid != DNGN_ROCK_WALL && old_grid != DNGN_STONE_WALL
                 && old_grid != DNGN_METAL_WALL && !one_chance_in(3))
        {
            old_grid = grd[p.x+1][p.y];
        }
        grd(p) = old_grid;
        set_terrain_changed(p);

        // Shift blood some of the time.
        if (is_bloodcovered(p))
        {
            if (one_chance_in(4))
            {
                int floor_count = 0;
                coord_def new_adj(p);
                for (adjacent_iterator ai(c); ai; ++ai)
                    if (feat_has_solid_floor(grd(*ai)) && one_chance_in(++floor_count))
                        new_adj = *ai;

                if (new_adj != p && maybe_bloodify_square(new_adj))
                    env.pgrid(p) &= (~FPROP_BLOODY);
            }
        }
        else if (one_chance_in(100))
        {
            // Occasionally add blood randomly, accumulating with time...
            maybe_bloodify_square(p);
        }
    }

    // The directions are used to randomly decide where to place items that
    // have ended up in walls during the switching.
    vector<coord_def> dirs;
    dirs.push_back(coord_def(-1,-1));
    dirs.push_back(coord_def(0,-1));
    dirs.push_back(coord_def(1,-1));
    dirs.push_back(coord_def(-1, 0));

    dirs.push_back(coord_def(1, 0));
    dirs.push_back(coord_def(-1, 1));
    dirs.push_back(coord_def(0, 1));
    dirs.push_back(coord_def(1, 1));

    // Search the entire shifted area for stacks of items now stuck in walls
    // and move them to a random adjacent non-wall grid.
    for (rectangle_iterator ri(c1, c2); ri; ++ri)
    {
        if (!feat_is_wall(grd(*ri)) || igrd(*ri) == NON_ITEM)
            continue;

        if (msg)
        {
            mprf(MSGCH_DIAGNOSTICS,
                 "Need to move around some items at pos (%d, %d)...",
                 ri->x, ri->y);
        }
        // Search the eight possible directions in random order.
        shuffle_array(dirs);
        for (unsigned int i = 0; i < dirs.size(); i++)
        {
            const coord_def p = *ri + dirs[i];
            if (!in_bounds(p))
                continue;

            if (feat_has_solid_floor(grd(p)))
            {
                // Once a valid grid is found, move all items from the
                // stack onto it.
                move_items(*ri, p);

                if (msg)
                {
                    mprf(MSGCH_DIAGNOSTICS, "Moved items over to (%d, %d)",
                         p.x, p.y);
                }
                break;
            }
        }
    }

    // Recheck item coordinates, to make totally sure.
    fix_item_coordinates();

    // Finally, give the player a clue about what just happened.
    const int which = (silenced(you.pos()) ? 2 + random2(2)
                                           : random2(4));
    switch (which)
    {
    case 0: mpr("You hear an odd grinding sound!"); break;
    case 1: mpr("You hear the creaking of ancient gears!"); break;
    case 2: mpr("The floor suddenly vibrates beneath you!"); break;
    case 3: mpr("You feel a sudden draft!"); break;
    }
}

static void _handle_magic_contamination()
{
    int added_contamination = 0;

    // Scale has been increased by a factor of 1000, but the effect now happens
    // every turn instead of every 20 turns, so everything has been multiplied
    // by 50 and scaled to you.time_taken.

    if (you.duration[DUR_INVIS])
        added_contamination += 30;

    if (you.duration[DUR_HASTE])
        added_contamination += 30;

    // Is there a point to this? It's not even strong enough to cancel normal
    // dissipation, so it only slows it down. Shouldn't it cancel dissipation
    // like haste and invis do?
    if (you.duration[DUR_FINESSE])
        added_contamination += 20;

#if TAG_MAJOR_VERSION == 34
    if (you.duration[DUR_REGENERATION] && you.species == SP_DJINNI)
        added_contamination += 20;
#endif
    // The Orb halves dissipation (well a bit more, I had to round it),
    // but won't cause glow on its own -- otherwise it'd spam the player
    // with messages about contamination oscillating near zero.
    if (you.magic_contamination && orb_haloed(you.pos()))
        added_contamination += 13;

    // Normal dissipation
    if (!you.duration[DUR_INVIS] && !you.duration[DUR_HASTE])
        added_contamination -= 25;

    // Scaling to turn length
    added_contamination = div_rand_round(added_contamination * you.time_taken,
                                         BASELINE_DELAY);

    contaminate_player(added_contamination, false);
}

// Bad effects from magic contamination.
static void _magic_contamination_effects()
{
    mprf(MSGCH_WARN, "Your body shudders with the violent release "
                     "of wild energies!");

    const int contam = you.magic_contamination;

    // For particularly violent releases, make a little boom.
    if (contam > 10000 && coinflip())
    {
        bolt beam;

        beam.flavour      = BEAM_RANDOM;
        beam.glyph        = dchar_glyph(DCHAR_FIRED_BURST);
        beam.damage       = dice_def(3, div_rand_round(contam, 2000 ));
        beam.target       = you.pos();
        beam.name         = "magical storm";
        //XXX: Should this be MID_PLAYER?
        beam.source_id    = MID_NOBODY;
        beam.aux_source   = "a magical explosion";
        beam.ex_size      = max(1, min(9, div_rand_round(contam, 15000)));
        beam.ench_power   = div_rand_round(contam, 200);
        beam.is_explosion = true;

        // Undead enjoy extra contamination explosion damage because
        // the magical contamination has a harder time dissipating
        // through non-living flesh. :-)
        if (you.undead_state() != US_ALIVE)
            beam.damage.size *= 2;

        beam.explode();
    }

#if TAG_MAJOR_VERSION == 34
    const mutation_permanence_class mutclass = you.species == SP_DJINNI
        ? MUTCLASS_TEMPORARY
        : MUTCLASS_NORMAL;
#else
    const mutation_permanence_class mutclass = MUTCLASS_NORMAL;
#endif

    // We want to warp the player, not do good stuff!
    mutate(one_chance_in(5) ? RANDOM_MUTATION : RANDOM_BAD_MUTATION,
           "mutagenic glow", true, coinflip(), false, false, mutclass, false);

    // we're meaner now, what with explosions and whatnot, but
    // we dial down the contamination a little faster if its actually
    // mutating you.  -- GDL
    contaminate_player(-(random2(contam / 4) + 1000));
}
// Checks if the player should be hit with magic contaimination effects,
// then actually does it if they should be.
static void _handle_magic_contamination(int time_delta)
{
    UNUSED(time_delta);

    // [ds] Move magic contamination effects closer to b26 again.
    const bool glow_effect = get_contamination_level() > 1
            && x_chance_in_y(you.magic_contamination, 12000);

    if (glow_effect)
    {
        if (is_sanctuary(you.pos()))
        {
            mprf(MSGCH_GOD, "Your body momentarily shudders from a surge of wild "
                            "energies until Zin's power calms it.");
        }
        else
            _magic_contamination_effects();
    }
}

// Adjust the player's stats if s/he's diseased (or recovering).
static void _recover_stats(int time_delta)
{
    UNUSED(time_delta);
    if (!you.disease)
    {
        bool recovery = true;

        // The better-fed you are, the faster your stat recovery.
        if (you.species == SP_VAMPIRE)
        {
            if (you.hunger_state == HS_STARVING)
            {
                // No stat recovery for starving vampires.
                recovery = false;
            }
            else if (you.hunger_state <= HS_HUNGRY)
            {
                // Halved stat recovery for hungry vampires.
                recovery = coinflip();
            }
        }

        // Slow heal 3 mutation stops stat recovery.
        if (player_mutation_level(MUT_SLOW_HEALING) == 3)
            recovery = false;

        // Rate of recovery equals one level of MUT_DETERIORATION.
        if (recovery && x_chance_in_y(4, 200))
            restore_stat(STAT_RANDOM, 1, false, true);
    }
    else
    {
        // If Cheibriados has slowed your biology, disease might
        // not actually do anything.
        if (one_chance_in(30)
            && !(you_worship(GOD_CHEIBRIADOS)
                 && you.piety >= piety_breakpoint(0)
                 && coinflip()))
        {
            mprf(MSGCH_WARN, "Your disease is taking its toll.");
            lose_stat(STAT_RANDOM, 1, false, "disease");
        }
    }
}

// Adjust the player's stats if s/he has the deterioration mutation.
static void _deteriorate(int time_delta)
{
    UNUSED(time_delta);
    if (player_mutation_level(MUT_DETERIORATION)
        && x_chance_in_y(player_mutation_level(MUT_DETERIORATION) * 5 - 1, 200))
    {
        lose_stat(STAT_RANDOM, 1, false, "deterioration mutation");
    }
}

// Exercise armour *xor* stealth skill: {dlb}
static void _wait_practice(int time_delta)
{
    UNUSED(time_delta);
    practise(EX_WAIT);
}

static void _lab_change(int time_delta)
{
    UNUSED(time_delta);
    if (player_in_branch(BRANCH_LABYRINTH))
        change_labyrinth();
}

// Update the abyss speed. This place is unstable and the speed can
// fluctuate. It's not a constant increase.
static void _abyss_speed(int time_delta)
{
    UNUSED(time_delta);
    if (!player_in_branch(BRANCH_ABYSS))
        return;

    if (you_worship(GOD_CHEIBRIADOS) && coinflip())
        ; // Speed change less often for Chei.
    else if (coinflip() && you.abyss_speed < 100)
        ++you.abyss_speed;
    else if (one_chance_in(5) && you.abyss_speed > 0)
        --you.abyss_speed;
}

static void _jiyva_effects(int time_delta)
{
    UNUSED(time_delta);

    if (!you_worship(GOD_JIYVA))
        return;

    if (one_chance_in(10))
    {
        int total_jellies = 1 + random2(5);
        bool success = false;
        for (int num_jellies = total_jellies; num_jellies > 0; num_jellies--)
        {
            // Spread jellies around the level.
            coord_def newpos;
            do
                newpos = random_in_bounds();
            while (grd(newpos) != DNGN_FLOOR
                       && grd(newpos) != DNGN_SHALLOW_WATER
                   || monster_at(newpos)
                   || env.cgrid(newpos) != EMPTY_CLOUD
                   || testbits(env.pgrid(newpos), FPROP_NO_JIYVA));

            mgen_data mg(MONS_JELLY, BEH_STRICT_NEUTRAL, 0, 0, 0, newpos,
                         MHITNOT, 0, GOD_JIYVA);
            mg.non_actor_summoner = "Jiyva";

            if (create_monster(mg))
                success = true;
        }

        if (success && !silenced(you.pos()))
        {
            switch (random2(3))
            {
                case 0:
                    simple_god_message(" gurgles merrily.");
                    break;
                case 1:
                    mprf(MSGCH_SOUND, "You hear %s splatter%s.",
                         total_jellies > 1 ? "a series of" : "a",
                         total_jellies > 1 ? "s" : "");
                    break;
                case 2:
                    simple_god_message(" says: Divide and consume!");
                    break;
            }
        }
    }

    if (x_chance_in_y(you.piety / 4, MAX_PIETY)
        && !player_under_penance() && one_chance_in(4))
    {
        jiyva_stat_action();
    }

    if (one_chance_in(25))
        jiyva_eat_offlevel_items();
}

static void _evolve(int time_delta)
{
    if (int lev = player_mutation_level(MUT_EVOLUTION))
        if (one_chance_in(2 / lev)
            && you.attribute[ATTR_EVOL_XP] * (1 + random2(10))
               > (int)exp_needed(you.experience_level + 1))
        {
            you.attribute[ATTR_EVOL_XP] = 0;
            mpr("You feel a genetic drift.");
            bool evol = one_chance_in(5) ?
                delete_mutation(RANDOM_BAD_MUTATION, "evolution", false) :
                mutate(coinflip() ? RANDOM_GOOD_MUTATION : RANDOM_MUTATION,
                       "evolution", false, false, false, false, MUTCLASS_NORMAL,
                       true);
            // it would kill itself anyway, but let's speed that up
            if (one_chance_in(10)
                && (!you.rmut_from_item()
                    || one_chance_in(10)))
            {
                evol |= delete_mutation(MUT_EVOLUTION, "end of evolution", false);
            }
            // interrupt the player only if something actually happened
            if (evol)
                more();
        }
}

// Get around C++ dividing integers towards 0.
static int _div(int num, int denom)
{
    div_t res = div(num, denom);
    return res.rem >= 0 ? res.quot : res.quot - 1;
}

struct timed_effect
{
    timed_effect_type effect;
    void              (*trigger)(int);
    int               min_time;
    int               max_time;
    bool              arena;
};

static struct timed_effect timed_effects[] =
{
    { TIMER_CORPSES,       rot_floor_items,               200,   200, true  },
    { TIMER_HELL_EFFECTS,  _hell_effects,                 200,   600, false },
    { TIMER_STAT_RECOVERY, _recover_stats,                100,   300, false },
    { TIMER_CONTAM,        _handle_magic_contamination,   200,   600, false },
    { TIMER_DETERIORATION, _deteriorate,                  100,   300, false },
    { TIMER_GOD_EFFECTS,   handle_god_time,               100,   300, false },
#if TAG_MAJOR_VERSION == 34
    { TIMER_SCREAM, NULL,                                   0,     0, false },
#endif
    { TIMER_FOOD_ROT,      rot_inventory_food,           100,   300, false },
    { TIMER_PRACTICE,      _wait_practice,                100,   300, false },
    { TIMER_LABYRINTH,     _lab_change,                  1000,  3000, false },
    { TIMER_ABYSS_SPEED,   _abyss_speed,                  100,   300, false },
    { TIMER_JIYVA,         _jiyva_effects,                100,   300, false },
    { TIMER_EVOLUTION,     _evolve,                      5000, 15000, false },
#if TAG_MAJOR_VERSION == 34
    { TIMER_BRIBE_TIMEOUT, NULL,                            0,     0, false },
#endif
};

// Do various time related actions...
void handle_time()
{
    int base_time = you.elapsed_time % 200;
    int old_time = base_time - you.time_taken;

    // The checks below assume the function is called at least
    // once every 50 elapsed time units.

    // Every 5 turns, spawn random monsters, not in Zotdef.
    if (_div(base_time, 50) > _div(old_time, 50)
        && !crawl_state.game_is_zotdef())
    {
        spawn_random_monsters();
        if (player_in_branch(BRANCH_ABYSS))
          for (int i = 1; i < you.depth; ++i)
                if (x_chance_in_y(i, 5))
                    spawn_random_monsters();
    }

    // Labyrinth and Abyss maprot.
    if (player_in_branch(BRANCH_LABYRINTH) || player_in_branch(BRANCH_ABYSS))
        forget_map(true);

    // Magic contamination from spells and Orb.
    if (!crawl_state.game_is_arena())
        _handle_magic_contamination();

    for (unsigned int i = 0; i < ARRAYSZ(timed_effects); i++)
    {
        if (crawl_state.game_is_arena() && !timed_effects[i].arena)
            continue;

        if (!timed_effects[i].trigger)
        {
            if (you.next_timer_effect[i] < INT_MAX)
                you.next_timer_effect[i] = INT_MAX;
            continue;
        }

        if (you.elapsed_time >= you.next_timer_effect[i])
        {
            int time_delta = you.elapsed_time - you.last_timer_effect[i];
            (timed_effects[i].trigger)(time_delta);
            you.last_timer_effect[i] = you.next_timer_effect[i];
            you.next_timer_effect[i] =
                you.last_timer_effect[i]
                + random_range(timed_effects[i].min_time,
                               timed_effects[i].max_time);
        }
    }
}

/**
 * Return the number of turns it takes for monsters to forget about the player
 * 50% of the time.
 *
 * @param   The intelligence of the monster.
 * @return  An average number of turns before the monster forgets.
 */
static int _mon_forgetfulness_time(mon_intel_type intelligence)
{
    switch (intelligence)
    {
        case I_HIGH:
            return 1000;
        case I_NORMAL:
        default:
            return 500;
        case I_ANIMAL:
        case I_REPTILE:
        case I_INSECT:
            return 250;
        case I_PLANT:
            return 125;
    }
}

/**
 * Make monsters forget about the player after enough time passes off-level.
 *
 * @param mon           The monster in question.
 * @param mon_turns     Monster turns. (Turns * monster speed)
 * @return              Whether the monster forgot about the player.
 */
static bool _monster_forget(monster* mon, int mon_turns)
{
    // After x turns, half of the monsters will have forgotten about the
    // player. A given monster has a 95% chance of forgetting the player after
    // 4*x turns.
    const int forgetfulness_time = _mon_forgetfulness_time(mons_intel(mon));
    const int forget_chances = mon_turns / forgetfulness_time;
    // n.b. this is an integer division, so if range < forgetfulness_time
    // nothing happens

    if (bernoulli(forget_chances, 0.5))
    {
        mon->behaviour = BEH_WANDER;
        mon->foe = MHITNOT;
        mon->target = random_in_bounds();
        return true;
    }

    return false;
}

/**
 * Make ranged monsters flee from the player during their time offlevel.
 *
 * @param mon           The monster in question.
 */
static void _monster_flee(monster *mon)
{
    mon->behaviour = BEH_FLEE;
    dprf("backing off...");

    if (mon->pos() != mon->target)
        return;
    // If the monster is on the target square, fleeing won't work.

    if (in_bounds(env.old_player_pos) && env.old_player_pos != mon->pos())
    {
        // Flee from player's old position if different.
        mon->target = env.old_player_pos;
        return;
    }

    // Randomise the target so we have a direction to flee.
    coord_def mshift(random2(3) - 1, random2(3) - 1);

    // Bounds check: don't let fleeing monsters try to run off the grid.
    const coord_def s = mon->target + mshift;
    if (!in_bounds_x(s.x))
        mshift.x = 0;
    if (!in_bounds_y(s.y))
        mshift.y = 0;

    mon->target.x += mshift.x;
    mon->target.y += mshift.y;

    return;
}

/**
 * Make a monster take a number of moves toward (or away from, if fleeing)
 * their current target, very crudely.
 *
 * @param mon       The mon in question.
 * @param moves     The number of moves to take.
 */
static void _catchup_monster_move(monster* mon, int moves)
{
    coord_def pos(mon->pos());

    // Dirt simple movement.
    for (int i = 0; i < moves; ++i)
    {
        coord_def inc(mon->target - pos);
        inc = coord_def(sgn(inc.x), sgn(inc.y));

        if (mons_is_retreating(mon))
            inc *= -1;

        // Bounds check: don't let shifting monsters try to run off the
        // grid.
        const coord_def s = pos + inc;
        if (!in_bounds_x(s.x))
            inc.x = 0;
        if (!in_bounds_y(s.y))
            inc.y = 0;

        if (inc.origin())
            break;

        const coord_def next(pos + inc);
        const dungeon_feature_type feat = grd(next);
        if (feat_is_solid(feat)
            || monster_at(next)
            || !monster_habitable_grid(mon, feat))
        {
            break;
        }

        pos = next;
    }

    if (!mon->shift(pos))
        mon->shift(mon->pos());
}

/**
 * Move monsters around to fake them walking around while player was
 * off-level.
 *
 * Does not account for monster move speeds.
 *
 * Also make them forget about the player over time.
 *
 * @param mon       The monster under consideration
 * @param turns     The number of offlevel player turns to simulate.
 */
static void _catchup_monster_moves(monster* mon, int turns)
{
    // Summoned monsters might have disappeared.
    if (!mon->alive())
        return;

    // Expire friendly summons
    if (mon->friendly() && mon->is_summoned() && !mon->is_perm_summoned())
    {
        // You might still see them disappear if you were quick
        if (turns > 2)
            monster_die(mon, KILL_DISMISSED, NON_MONSTER);
        else
        {
            mon_enchant abj  = mon->get_ench(ENCH_ABJ);
            abj.duration = 0;
            mon->update_ench(abj);
        }
        return;
    }

    // Don't move non-land or stationary monsters around.
    if (mons_primary_habitat(mon) != HT_LAND
        || mons_is_zombified(mon)
           && mons_class_primary_habitat(mon->base_monster) != HT_LAND
        || mon->is_stationary())
    {
        return;
    }

    // Don't shift giant spores since that would disrupt their trail.
    if (mon->type == MONS_GIANT_SPORE)
        return;

    // special movement code for ioods, boulder beetles...
    if (mon->is_projectile())
    {
        iood_catchup(mon, turns);
        return;
    }

    // Let sleeping monsters lie.
    if (mon->asleep() || mon->paralysed())
        return;



    const int mon_turns = (turns * mon->speed) / 10;
    const int moves = min(mon_turns, 50);

    // probably too annoying even for DEBUG_DIAGNOSTICS
    dprf("mon #%d: range %d; "
         "pos (%d,%d); targ %d(%d,%d); flags %" PRIx64,
         mon->mindex(), mon_turns, mon->pos().x, mon->pos().y,
         mon->foe, mon->target.x, mon->target.y, mon->flags);

    if (mon_turns <= 0)
        return;


    // did the monster forget about the player?
    const bool forgot = _monster_forget(mon, mon_turns);

    // restore behaviour later if we start fleeing
    unwind_var<beh_type> saved_beh(mon->behaviour);

    if (!forgot && mons_has_ranged_attack(mon))
    {
        // If we're doing short time movement and the monster has a
        // ranged attack (missile or spell), then the monster will
        // flee to gain distance if it's "too close", else it will
        // just shift its position rather than charge the player. -- bwr
        if (grid_distance(mon->pos(), mon->target) >= 3)
        {
            mon->shift(mon->pos());
            dprf("shifted to (%d, %d)", mon->pos().x, mon->pos().y);
            return;
        }

        _monster_flee(mon);
    }

    _catchup_monster_move(mon, moves);

    dprf("moved to (%d, %d)", mon->pos().x, mon->pos().y);
}

//---------------------------------------------------------------
//
// update_level
//
// Update the level when the player returns to it.
//
//---------------------------------------------------------------
void update_level(int elapsedTime)
{
    ASSERT(!crawl_state.game_is_arena());

    // In ZotDef, no time passes while off-level.
    if (crawl_state.game_is_zotdef())
        return;

    const int turns = elapsedTime / 10;

#ifdef DEBUG_DIAGNOSTICS
    int mons_total = 0;

    dprf("turns: %d", turns);
#endif

    rot_floor_items(elapsedTime);
    shoals_apply_tides(turns, true, turns < 5);
    timeout_tombs(turns);
    recharge_rods(turns, true);

    if (env.sanctuary_time)
    {
        if (turns >= env.sanctuary_time)
            remove_sanctuary();
        else
            env.sanctuary_time -= turns;
    }

    dungeon_events.fire_event(
        dgn_event(DET_TURN_ELAPSED, coord_def(0, 0), turns * 10));

    for (monster_iterator mi; mi; ++mi)
    {
#ifdef DEBUG_DIAGNOSTICS
        mons_total++;
#endif

        // Pacified monsters often leave the level now.
        if (mi->pacified() && turns > random2(40) + 21)
        {
            make_mons_leave_level(*mi);
            continue;
        }

        // Following monsters don't get movement.
        if (mi->flags & MF_JUST_SUMMONED)
            continue;

        // XXX: Allow some spellcasting (like Healing and Teleport)? - bwr
        // const bool healthy = (mi->hit_points * 2 > mi->max_hit_points);

        mi->heal(div_rand_round(turns * mi->off_level_regen_rate(), 100));

        // Handle nets specially to remove the trapping property of the net.
        if (mi->caught())
            mi->del_ench(ENCH_HELD, true);

        _catchup_monster_moves(*mi, turns);

        mi->foe_memory = max(mi->foe_memory - turns, 0);

        if (turns >= 10 && mi->alive())
            mi->timeout_enchantments(turns / 10);
    }

#ifdef DEBUG_DIAGNOSTICS
    dprf("total monsters on level = %d", mons_total);
#endif

    for (int i = 0; i < MAX_CLOUDS; i++)
        delete_cloud(i);
}

// Basically we want to break a circle into n_arcs equal sized arcs and find
// out which arc the input point pos falls on.
static int _arc_decomposition(const coord_def & pos, int n_arcs)
{
    float theta = atan2((float)pos.y, (float)pos.x);

    if (pos.x == 0 && pos.y != 0)
        theta = pos.y > 0 ? PI / 2 : -PI / 2;

    if (theta < 0)
        theta += 2 * PI;

    float arc_angle = 2 * PI / n_arcs;

    theta += arc_angle / 2.0f;

    if (theta >= 2 * PI)
        theta -= 2 * PI;

    return static_cast<int> (theta / arc_angle);
}

int place_ring(vector<coord_def> &ring_points,
               const coord_def &origin,
               mgen_data prototype,
               int n_arcs,
               int arc_occupancy,
               int &seen_count)
{
    shuffle_array(ring_points);

    int target_amount = ring_points.size();
    int spawned_count = 0;
    seen_count = 0;

    vector<int> arc_counts(n_arcs, arc_occupancy);

    for (unsigned i = 0;
         spawned_count < target_amount && i < ring_points.size();
         i++)
    {
        int direction = _arc_decomposition(ring_points.at(i)
                                           - origin, n_arcs);

        if (arc_counts[direction]-- <= 0)
            continue;

        prototype.pos = ring_points.at(i);

        if (create_monster(prototype, false))
        {
            spawned_count++;
            if (you.see_cell(ring_points.at(i)))
                seen_count++;
        }
    }

    return spawned_count;
}

// Collect lists of points that are within LOS (under the given env map),
// unoccupied, and not solid (walls/statues).
void collect_radius_points(vector<vector<coord_def> > &radius_points,
                           const coord_def &origin, los_type los)
{
    radius_points.clear();
    radius_points.resize(LOS_RADIUS);

    // Just want to associate a point with a distance here for convenience.
    typedef pair<coord_def, int> coord_dist;

    // Using a priority queue because squares don't make very good circles at
    // larger radii.  We will visit points in order of increasing euclidean
    // distance from the origin (not path distance).  We want a min queue
    // based on the distance, so we use greater_second as the comparator.
    priority_queue<coord_dist, vector<coord_dist>,
                   greater_second<coord_dist> > fringe;

    fringe.push(coord_dist(origin, 0));

    set<int> visited_indices;

    int current_r = 1;
    int current_thresh = current_r * (current_r + 1);

    int max_distance = LOS_RADIUS * LOS_RADIUS + 1;

    while (!fringe.empty())
    {
        coord_dist current = fringe.top();
        // We're done here once we hit a point that is farther away from the
        // origin than our maximum permissible radius.
        if (current.second > max_distance)
            break;

        fringe.pop();

        int idx = current.first.x + current.first.y * X_WIDTH;
        if (!visited_indices.insert(idx).second)
            continue;

        while (current.second > current_thresh)
        {
            current_r++;
            current_thresh = current_r * (current_r + 1);
        }

        // We don't include radius 0.  This is also a good place to check if
        // the squares are already occupied since we want to search past
        // occupied squares but don't want to consider them valid targets.
        if (current.second && !actor_at(current.first))
            radius_points[current_r - 1].push_back(current.first);

        for (adjacent_iterator i(current.first); i; ++i)
        {
            coord_dist temp(*i, current.second);

            // If the grid is out of LOS, skip it.
            if (!cell_see_cell(origin, temp.first, los))
                continue;

            coord_def local = temp.first - origin;

            temp.second = local.abs();

            idx = temp.first.x + temp.first.y * X_WIDTH;

            if (!visited_indices.count(idx)
                && in_bounds(temp.first)
                && !cell_is_solid(temp.first))
            {
                fringe.push(temp);
            }
        }

    }
}

// Place a partial ring of toadstools around the given corpse.  Returns
// the number of mushrooms spawned.  A return of 0 indicates no
// mushrooms were placed -> some sort of failure mode was reached.
static int _mushroom_ring(item_def &corpse, int & seen_count,
                          beh_type toadstool_behavior)
{
    // minimum number of mushrooms spawned on a given ring
    unsigned min_spawn = 2;

    seen_count = 0;

    vector<vector<coord_def> > radius_points;

    collect_radius_points(radius_points, corpse.pos, LOS_SOLID);

    // So what we have done so far is collect the set of points at each radius
    // reachable from the origin with (somewhat constrained) 8 connectivity,
    // now we will choose one of those radii and spawn mushrooms at some
    // of the points along it.
    int chosen_idx = random2(LOS_RADIUS);

    unsigned max_size = 0;
    for (unsigned i = 0; i < LOS_RADIUS; ++i)
    {
        if (radius_points[i].size() >= max_size)
        {
            max_size = radius_points[i].size();
            chosen_idx = i;
        }
    }

    chosen_idx = random2(chosen_idx + 1);

    // Not enough valid points?
    if (radius_points[chosen_idx].size() < min_spawn)
        return 0;

    mgen_data temp(MONS_TOADSTOOL,
                   toadstool_behavior, 0, 0, 0,
                   coord_def(),
                   MHITNOT,
                   MG_FORCE_PLACE,
                   GOD_NO_GOD,
                   MONS_NO_MONSTER,
                   0,
                   corpse.get_colour());

    float target_arc_len = 2 * sqrtf(2.0f);

    int n_arcs = static_cast<int> (ceilf(2 * PI * (chosen_idx + 1)
                                   / target_arc_len));

    int spawned_count = place_ring(radius_points[chosen_idx], corpse.pos, temp,
                                   n_arcs, 1, seen_count);

    return spawned_count;
}

// Try to spawn 'target_count' mushrooms around the position of
// 'corpse'.  Returns the number of mushrooms actually spawned.
// Mushrooms radiate outwards from the corpse following bfs with
// 8-connectivity.  Could change the expansion pattern by using a
// priority queue for sequencing (priority = distance from origin under
// some metric).
int spawn_corpse_mushrooms(item_def& corpse,
                           int target_count,
                           int& seen_targets,
                           beh_type toadstool_behavior,
                           bool distance_as_time)

{
    seen_targets = 0;
    if (target_count == 0)
        return 0;

    int c_size = 8;
    int permutation[] = {0, 1, 2, 3, 4, 5, 6, 7};

    int placed_targets = 0;

    queue<coord_def> fringe;
    set<int> visited_indices;

    // Slight chance of spawning a ring of mushrooms around the corpse (and
    // skeletonising it) if the corpse square is unoccupied.
    if (!actor_at(corpse.pos) && one_chance_in(100))
    {
        int ring_seen;
        // It's possible no reasonable ring can be found, in that case we'll
        // give up and just place a toadstool on top of the corpse (probably).
        int res = _mushroom_ring(corpse, ring_seen, toadstool_behavior);

        if (res)
        {
            corpse.special = 0;

            if (you.see_cell(corpse.pos))
                mpr("A ring of toadstools grows before your very eyes.");
            else if (ring_seen > 1)
                mpr("Some toadstools grow in a peculiar arc.");
            else if (ring_seen > 0)
                mpr("A toadstool grows.");

            seen_targets = -1;

            return res;
        }
    }

    visited_indices.insert(X_WIDTH * corpse.pos.y + corpse.pos.x);
    fringe.push(corpse.pos);

    while (!fringe.empty())
    {
        coord_def current = fringe.front();

        fringe.pop();

        monster* mons = monster_at(current);

        bool player_occupant = you.pos() == current;

        // Is this square occupied by a non mushroom?
        if (mons && mons->mons_species() != MONS_TOADSTOOL
            || player_occupant && !you_worship(GOD_FEDHAS)
            || !can_spawn_mushrooms(current))
        {
            continue;
        }

        if (!mons)
        {
            monster *mushroom = create_monster(
                        mgen_data(MONS_TOADSTOOL,
                                  toadstool_behavior,
                                  0,
                                  0,
                                  0,
                                  current,
                                  MHITNOT,
                                  MG_FORCE_PLACE,
                                  GOD_NO_GOD,
                                  MONS_NO_MONSTER,
                                  0,
                                  corpse.get_colour()),
                                  false);

            if (mushroom)
            {
                // Going to explicitly override the die-off timer in
                // this case (this condition means we got called from
                // fedhas_fungal_bloom() or similar, and are creating a
                // lot of toadstools at once that should die off
                // quickly).
                if (distance_as_time)
                {
                    coord_def offset = corpse.pos - current;

                    int dist = static_cast<int>(sqrtf(offset.abs()) + 0.5);

                    // Trying a longer base duration...
                    int time_left = random2(8) + dist * 8 + 8;

                    time_left *= 10;

                    mon_enchant temp_en(ENCH_SLOWLY_DYING, 1, 0, time_left);
                    mushroom->update_ench(temp_en);
                }

                placed_targets++;
                if (current == you.pos())
                {
                    mprf("A toadstool grows %s.",
                         player_has_feet() ? "at your feet" : "before you");
                    current = mushroom->pos();
                }
                else if (you.see_cell(current))
                    seen_targets++;
            }
            else
                continue;
        }

        // We're done here if we placed the desired number of mushrooms.
        if (placed_targets == target_count)
            break;

        // Wish adjacent_iterator had a random traversal.
        shuffle_array(permutation, c_size);

        for (int count = 0; count < c_size; ++count)
        {
            coord_def temp = current + Compass[permutation[count]];

            int index = temp.x + temp.y * X_WIDTH;

            if (!visited_indices.count(index)
                && in_bounds(temp)
                && mons_class_can_pass(MONS_TOADSTOOL, grd(temp)))
            {
                visited_indices.insert(index);
                fringe.push(temp);
            }
        }
    }

    return placed_targets;
}

int mushroom_prob(item_def & corpse)
{
    int low_threshold = 5;
    int high_threshold = FRESHEST_CORPSE - 5;

    // Expect this many trials over a corpse's lifetime since this function
    // is called once for every 10 units of rot_time.
    int step_size = 10;
    float total_trials = (high_threshold - low_threshold) / step_size;

    // Chance of producing no mushrooms (not really because of weight_factor
    // below).
    float p_failure = 0.5f;

    float trial_prob_f = 1 - powf(p_failure, 1.0f / total_trials);

    // The chance of producing mushrooms depends on the weight of the
    // corpse involved.  Humans weigh 550 so we will take that as the
    // base factor here.
    float weight_factor = item_mass(corpse) / 550.0f;

    trial_prob_f *= weight_factor;

    int trial_prob = static_cast<int>(100 * trial_prob_f);

    return trial_prob;
}

bool mushroom_spawn_message(int seen_targets, int seen_corpses)
{
    if (seen_targets <= 0)
        return false;

    string what  = seen_targets  > 1 ? "Some toadstools"
                                     : "A toadstool";
    string where = seen_corpses  > 1 ? "nearby corpses" :
                   seen_corpses == 1 ? "a nearby corpse"
                                     : "the ground";
    mprf("%s grow%s from %s.",
         what.c_str(), seen_targets > 1 ? "" : "s", where.c_str());

    return true;
}

static void _recharge_rod(item_def &rod, int aut, bool in_inv)
{
    if (rod.base_type != OBJ_RODS || rod.charges >= rod.charge_cap)
        return;

    // Skill calculations with a massive scale would overflow, cap it.
    // The worst case, a -3 rod, takes 17000 aut to fully charge.
    // -4 rods don't recharge at all.
    aut = min(aut, MAX_ROD_CHARGE * ROD_CHARGE_MULT * 10);

    int rate = 4 + rod.special;

    rate *= 10 * aut;
    if (in_inv)
        rate += skill_bump(SK_EVOCATIONS, aut);
    rate = div_rand_round(rate, 100);

    if (rate > rod.charge_cap - rod.charges) // Prevent overflow
        rate = rod.charge_cap - rod.charges;

    // With this, a +0 rod with no skill gets 1 mana per 25.0 turns

    if (rod.plus / ROD_CHARGE_MULT != (rod.plus + rate) / ROD_CHARGE_MULT)
    {
        if (item_equip_slot(rod) == EQ_WEAPON)
            you.wield_change = true;
        if (item_is_quivered(rod))
            you.redraw_quiver = true;
    }

    rod.plus += rate;

    if (in_inv && rod.charges == rod.charge_cap)
    {
        msg::stream << "Your " << rod.name(DESC_QUALNAME) << " has recharged."
                    << endl;
        if (is_resting())
            stop_running();
    }

    return;
}

void recharge_rods(int aut, bool level_only)
{
    if (!level_only)
    {
        for (int item = 0; item < ENDOFPACK; ++item)
            _recharge_rod(you.inv[item], aut, true);
    }

    for (int item = 0; item < MAX_ITEMS; ++item)
        _recharge_rod(mitm[item], aut, false);
}

/**
 * Applies a temporary corrosion debuff to an actor.
 */
void corrode_actor(actor *act, const char* corrosion_source)
{
    // Don't corrode spectral weapons.
    if (act->is_monster()
        && mons_is_avatar(act->as_monster()->type))
    {
        return;
    }

    // rCorr protects against 50% of corrosion.
    if (act->res_corr() && coinflip())
    {
        dprf("rCorr protects.");
        return;
    }

    const string csrc = uppercase_first(corrosion_source);

    if (act->is_player())
    {
        // always increase duration, but...
        you.increase_duration(DUR_CORROSION, 10 + roll_dice(2, 4), 50,
                              make_stringf("%s corrodes your equipment!",
                                           csrc.c_str()).c_str());

        // the more corrosion you already have, the lower the odds of more
        const int prev_corr = you.props["corrosion_amount"].get_int();
        if (x_chance_in_y(prev_corr, prev_corr + 9))
            return;

        you.props["corrosion_amount"].get_int()++;
        you.redraw_armour_class = true;
        you.wield_change = true;
        return;
    }

    if (act->type == MONS_PLAYER_SHADOW)
        return; // it's just a temp copy of the item

    if (you.see_cell(act->pos()))
    {
        if (act->type == MONS_DANCING_WEAPON)
        {
            mprf("%s corrodes %s!",
                 csrc.c_str(),
                 act->name(DESC_THE).c_str());
        }
        else
        {
            mprf("%s corrodes %s equipment!",
                 csrc.c_str(),
                 apostrophise(act->name(DESC_THE)).c_str());
        }
    }

    act->as_monster()->add_ench(mon_enchant(ENCH_CORROSION, 0));
}

void slime_wall_damage(actor* act, int delay)
{
    ASSERT(act);

    int walls = 0;
    for (adjacent_iterator ai(act->pos()); ai; ++ai)
        if (env.grid(*ai) == DNGN_SLIMY_WALL)
            walls++;

    if (!walls)
        return;

    const int depth = player_in_branch(BRANCH_SLIME) ? you.depth : 1;

    // Up to 1d6 damage per wall per slot.
    const int strength = div_rand_round(depth * walls * delay, BASELINE_DELAY);

    if (act->is_player())
    {
        if (!you_worship(GOD_JIYVA) || you.penance[GOD_JIYVA])
        {
            you.splash_with_acid(NULL, strength, false,
                                (walls > 1) ? "The walls burn you!"
                                            : "The wall burns you!");
        }
    }
    else
    {
        monster* mon = act->as_monster();

        // Slime native monsters are immune to slime walls.
        if (mons_is_slime(mon))
            return;

        const int dam = resist_adjust_damage(mon, BEAM_ACID, mon->res_acid(),
                                              roll_dice(2, strength));
        if (dam > 0 && you.can_see(mon))
        {
            mprf((walls > 1) ? "The walls burn %s!" : "The wall burns %s!",
                  mon->name(DESC_THE).c_str());
        }
        mon->hurt(NULL, dam, BEAM_ACID);
    }
}

void recharge_xp_evokers(int exp)
{
    FixedVector<item_def*, NUM_MISCELLANY> evokers(nullptr);
    list_charging_evokers(evokers);

    int xp_factor = max(min((int)exp_needed(you.experience_level+1, 0) * 2 / 7,
                             you.experience_level * 425),
                        you.experience_level*4 + 30)
                    / (3 + you.skill_rdiv(SK_EVOCATIONS, 2, 13));

    for (int i = 0; i < NUM_MISCELLANY; ++i)
    {
        item_def* evoker = evokers[i];
        if (!evoker)
            continue;
        evoker->evoker_debt -= div_rand_round(exp, xp_factor);
        if (evoker->evoker_debt <= 0)
        {
            evoker->evoker_debt = 0;
            mprf("Your %s has recharged.", evoker->name(DESC_QUALNAME).c_str());
        }
    }
}
