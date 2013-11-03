/**
 * @file
 * @brief Misc stuff.
**/

#include "AppHdr.h"

#include "effects.h"

#include <cstdlib>
#include <string.h>
#include <stdio.h>
#include <algorithm>
#include <queue>
#include <set>
#include <cmath>

#include "externs.h"
#include "options.h"

#include "abyss.h"
#include "act-iter.h"
#include "areas.h"
#include "artefact.h"
#include "beam.h"
#include "branch.h"
#include "cloud.h"
#include "colour.h"
#include "coordit.h"
#include "database.h"
#include "delay.h"
#include "dgn-shoals.h"
#include "dgnevent.h"
#include "directn.h"
#include "dungeon.h"
#include "env.h"
#include "exercise.h"
#include "fight.h"
#include "fprop.h"
#include "godabil.h"
#include "godpassive.h"
#include "hints.h"
#include "hiscores.h"
#include "invent.h"
#include "itemname.h"
#include "itemprop.h"
#include "items.h"
#include "libutil.h"
#include "makeitem.h"
#include "map_knowledge.h"
#include "message.h"
#include "mgen_data.h"
#include "misc.h"
#include "mislead.h"
#include "mon-behv.h"
#include "mon-cast.h"
#include "mon-pathfind.h"
#include "mon-place.h"
#include "mon-project.h"
#include "mon-stuff.h"
#include "mon-util.h"
#include "mutation.h"
#include "notes.h"
#include "ouch.h"
#include "player-equip.h"
#include "player-stats.h"
#include "player.h"
#include "religion.h"
#include "shopping.h"
#include "shout.h"
#include "skills.h"
#include "skills2.h"
#include "spl-clouds.h"
#include "spl-miscast.h"
#include "spl-summoning.h"
#include "spl-util.h"
#include "stairs.h"
#include "state.h"
#include "stuff.h"
#include "terrain.h"
#include "transform.h"
#include "traps.h"
#include "travel.h"
#include "viewchar.h"
#include "xom.h"

static void _update_corpses(int elapsedTime);

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

    ouch(hploss, NON_MONSTER, type, aux);

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
    // Holy word won't annoy, slow, or frighten its user.
    if (attacker != mons)
    {
        // Currently, holy word annoys the monsters it affects
        // because it can kill them, and because hostile
        // monsters don't use it.
        if (attacker != NULL)
            behaviour_event(mons, ME_ANNOY, attacker);

        if (mons->speed_increment >= 25)
            mons->speed_increment -= 20;

        mons->add_ench(ENCH_FEAR);
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

    // We could use actor.get_los(), but maybe it's NULL.
    los_def los(where);
    los.update();
    for (radius_iterator ri(&los); ri; ++ri)
        holy_word_monsters(*ri, pow, source, attacker);
}

int torment_player(actor *attacker, int taux)
{
    ASSERT(!crawl_state.game_is_arena());

    // [dshaligram] Switched to using ouch() instead of dec_hp() so that
    // notes can also track torment and activities can be interrupted
    // correctly.
    int hploss = 0;

    if (!player_res_torment(false))
    {
        // Negative energy resistance can alleviate torment.
        hploss = max(0, you.hp * (50 - player_prot_life() * 5) / 100 - 1);
        // Statue form is only partial petrification.
        if (you.form == TRAN_STATUE || you.species == SP_GARGOYLE)
            hploss /= 2;
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

    const char *aux = "torment";

    kill_method_type type = KILLED_BY_MONSTER;
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
            aux = "scroll of torment";
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

    ouch(hploss, attacker? attacker->mindex() : MHITNOT, type, aux);

    if (!kiku_shielding_player)
        maybe_id_resist(BEAM_NEG);

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
    los_def los(where);
    los.update();
    int r = 0;
    for (radius_iterator ri(&los); ri; ++ri)
    {
        if (attacker && !attacker->see_cell_no_trans(*ri))
            continue;
        r += torment_monsters(*ri, attacker, taux);
    }
    return r;
}

void immolation(int pow, immolation_source_type source, bool known)
{
    ASSERT(!crawl_state.game_is_arena());

    const char *aux = "immolation";

    bolt beam;

    switch (source)
    {
    case IMMOLATION_SCROLL:
        aux = "a scroll of immolation";
        break;

    case IMMOLATION_AFFIX:
        aux = "a fiery explosion";
        break;

    case IMMOLATION_TOME:
        aux = "an exploding Tome of Destruction";
        break;

    default:;
    }

    beam.flavour       = BEAM_FIRE;
    beam.glyph         = dchar_glyph(DCHAR_FIRED_BURST);
    beam.damage        = dice_def(3, pow);
    beam.target        = you.pos();
    beam.name          = "fiery explosion";
    beam.colour        = RED;
    beam.aux_source    = aux;
    beam.ex_size       = 2;
    beam.is_explosion  = true;
    beam.effect_known  = known;
    beam.affects_items = source == IMMOLATION_TOME;
    beam.thrower       = KILL_YOU;
    beam.beam_source   = NON_MONSTER;

    beam.explode();
}

static bool _conduct_electricity_affects_actor(const bolt& beam,
                                               const actor* victim)
{
    return (victim->alive() && victim->res_elec() <= 0
            && victim->ground_level());
}

static bool _conduct_electricity_damage(bolt &beam, actor* victim,
                                        int &dmg, string &dmg_msg)
{
    dmg = (10 + random2(15)) / 2;
    return false;
}

static bool _conduct_electricity_aoe(bolt& beam, const coord_def& target)
{
    if (feat_is_water(grd(target)))
        return true;

    return false;
}

void conduct_electricity(coord_def where, actor *attacker)
{
    bolt beam;

    beam.flavour       = BEAM_ELECTRICITY;
    beam.glyph         = dchar_glyph(DCHAR_FIRED_BURST);
    beam.damage        = dice_def(1, 15);
    beam.target        = where;
    beam.name          = "electric current";
    beam.hit_verb      = "shocks";
    beam.colour        = ETC_ELECTRICITY;
    beam.aux_source    = "arcing electricity";
    beam.ex_size       = 1;
    beam.is_explosion  = true;
    beam.effect_known  = true;
    beam.affects_items = false;
    beam.aoe_funcs.push_back(_conduct_electricity_aoe);
    beam.damage_funcs.push_back(_conduct_electricity_damage);
    beam.affect_func   = _conduct_electricity_affects_actor;

    if (attacker && attacker->is_player())
    {
        beam.thrower     = KILL_YOU;
        beam.beam_source = NON_MONSTER;
    }
    else
    {
        beam.thrower     = KILL_MON;
        beam.beam_source = attacker ? attacker->mindex() : MHITNOT;
    }

    beam.explode(false, true);
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
        beam.thrower     = KILL_MISC;
        beam.beam_source = NON_MONSTER;
    }
    else if (attacker && attacker->is_player())
    {
        beam.thrower     = KILL_YOU;
        beam.beam_source = NON_MONSTER;
    }
    else
    {
        // If there was no attacker, caster should have been
        // CLEANSING_FLAME_{GENERIC,TSO} which we handled above.
        ASSERT(attacker);

        beam.thrower     = KILL_MON;
        beam.beam_source = attacker->mindex();
    }

    beam.explode();
}

static string _who_banished(const string &who)
{
    return (who.empty() ? who : " (" + who + ")");
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
            mpr("You are banished to a different region of the Abyss.", MSGCH_BANISHMENT);
            abyss_teleport(true);
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

bool forget_spell(void)
{
    ASSERT(!crawl_state.game_is_arena());

    if (!you.spell_no)
        return false;

    // find a random spell to forget:
    int slot = -1;
    int num  = 0;

    for (int i = 0; i < MAX_KNOWN_SPELLS; i++)
    {
        if (you.spells[i] != SPELL_NO_SPELL)
        {
            num++;
            if (one_chance_in(num))
                slot = i;
        }
    }

    if (slot == -1)              // should never happen though
        return false;

    mprf("Your knowledge of %s becomes hazy all of a sudden, and you forget "
         "the spell!", spell_title(you.spells[slot]));

    del_spell_from_memory_by_slot(slot);
    return true;
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

        damage_taken     = 10 + 2 * source->hit_dice;

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

            damage_taken     = roll_dice(3, 8 + source->hit_dice);

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

    case SPELL_MISLEAD:
        if (!def)
            mons_cast_mislead(source);
        else
            defender->confuse(source, source->hit_dice * 12);
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

    default:
        die("unknown direct_effect spell: %d", spell);
    }

    // apply damage and handle death, where appropriate {dlb}
    if (damage_taken > 0)
    {
        if (def)
            def->hurt(source, damage_taken);
        else
            ouch(damage_taken, pbolt.beam_source, KILLED_BY_BEAM,
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
        mpr("You hear the tinkle of a tiny bell.", MSGCH_SOUND);
        noisy(2, you.pos());
        cast_summon_butterflies(100);
        break;

    case 7:
        mprf(MSGCH_SOUND, "You hear %s.", weird_sound().c_str());
        noisy(2, you.pos());
        break;
    }
}

int recharge_wand(int item_slot, bool known, string *pre_msg)
{
    do
    {
        if (item_slot == -1)
        {
            item_slot = prompt_invent_item("Charge which item?", MT_INVLIST,
                                            OSEL_RECHARGE, true, true, false);
        }
        if (prompt_failed(item_slot))
            return -1;

        item_def &wand = you.inv[ item_slot ];

        if (!item_is_rechargeable(wand, known))
        {
            mpr("Choose an item to recharge, or Esc to abort.");
            if (Options.auto_list)
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
                max<int>(
                    wand.plus,
                    min(charge_gain * 3,
                             wand.plus +
                             1 + random2avg(((charge_gain - 1) * 3) + 1, 3)));

            const bool charged = (new_charges > wand.plus);

            string desc;

            if (charged && item_ident(wand, ISFLAG_KNOW_PLUSES))
            {
                snprintf(info, INFO_SIZE, " and now has %d charge%s",
                         new_charges, new_charges == 1 ? "" : "s");
                desc = info;
            }

            if (pre_msg)
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
            wand.plus  = new_charges;
            wand.plus2 = ZAPCOUNT_RECHARGED;
        }
        else // It's a rod.
        {
            bool work = false;

            if (wand.plus2 < MAX_ROD_CHARGE * ROD_CHARGE_MULT)
            {
                wand.plus2 += ROD_CHARGE_MULT * random_range(1,2);

                if (wand.plus2 > MAX_ROD_CHARGE * ROD_CHARGE_MULT)
                    wand.plus2 = MAX_ROD_CHARGE * ROD_CHARGE_MULT;

                work = true;
            }

            if (wand.plus < wand.plus2)
            {
                wand.plus = wand.plus2;
                work = true;
            }

            if (wand.special < MAX_WPN_ENCHANT)
            {
                wand.special += random_range(1, 2);
                if (wand.special > MAX_WPN_ENCHANT)
                    wand.special = MAX_WPN_ENCHANT;

                work = true;
            }

            if (!work)
                return 0;

            if (pre_msg)
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
    return (mon->friendly()
            && mon->type != MONS_GIANT_SPORE
            && mon->type != MONS_BATTLESPHERE
            && mon->type != MONS_SPECTRAL_WEAPON
            && !mon->berserk_or_insane()
            && !mon->is_projectile()
            && !mon->has_ench(ENCH_HAUNTING));
}

// Sets foe target of friendly monsters.
// If allow_patrol is true, patrolling monsters get MHITNOT instead.
static void _set_friendly_foes(bool allow_patrol = false)
{
    for (monster_near_iterator mi(you.pos()); mi; ++mi)
    {
        if (!_follows_orders(*mi))
            continue;
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
        mi->behaviour = BEH_WITHDRAW;
        mi->target = target;
        mi->patrol_point = rally_point;
        mi->foe = MHITNOT;

        mi->props.erase("last_pos");
        mi->props.erase("idle_point");
        mi->props.erase("idle_deadline");
        mi->props.erase("blocked_deadline");
    }
}

void yell(bool force)
{
    ASSERT(!crawl_state.game_is_arena());

    if (you.duration[DUR_WATER_HOLD] && !you.res_water_drowning())
    {
        mpr("You cannot shout while unable to breathe!");
        return;
    }

    bool targ_prev = false;
    int mons_targd = MHITNOT;
    dist targ;

    const string shout_verb = you.shout_verb();
    string cap_shout = shout_verb;
    cap_shout[0] = toupper(cap_shout[0]);

    int noise_level = 12; // "shout"

    // Tweak volume for different kinds of vocalisation.
    if (shout_verb == "roar")
        noise_level = 18;
    else if (shout_verb == "hiss")
        noise_level = 8;
    else if (shout_verb == "squeak")
        noise_level = 4;
    else if (shout_verb == "__NONE")
        noise_level = 0;
    else if (shout_verb == "yell")
        noise_level = 14;
    else if (shout_verb == "scream")
        noise_level = 16;

    if (you.cannot_speak() || !form_has_mouth())
        noise_level = 0;

    if (noise_level == 0)
    {
        if (force)
        {
            if (!form_has_mouth())
                mpr("You have no mouth, and you must scream!");
            else if (shout_verb == "__NONE" || you.paralysed())
            {
                mprf("You feel a strong urge to %s, but "
                     "you are unable to make a sound!",
                     shout_verb == "__NONE" ? "scream"
                                            : shout_verb.c_str());
            }
            else
            {
                mprf("You feel a %s rip itself from your throat, "
                     "but you make no sound!",
                     shout_verb.c_str());
            }
        }
        else if (!form_has_mouth())
            mpr("You have no mouth!");
        else
            mpr("You are unable to make a sound!");

        return;
    }

    if (force)
    {
        if (you.duration[DUR_RECITE])
            mpr("You feel yourself shouting your recitation.");
        else
            mprf("A %s rips itself from your throat!", shout_verb.c_str());
        noisy(noise_level, you.pos());
        return;
    }

    mpr("What do you say?", MSGCH_PROMPT);
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
    mprf(" Anything else - Stay silent%s.",
         one_chance_in(20) ? " (and be thought a fool)" : "");

    int keyn = get_ch();
    mesclr();

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

static void _hell_effects()
{
    if ((you_worship(GOD_ZIN) && x_chance_in_y(you.piety, MAX_PIETY))
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
    mpr(msg.c_str(), MSGCH_HELL_EFFECT);
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

    return (feat_is_wall(grd(adjs[0])) && feat_is_wall(grd(adjs[1]))
               && feat_has_solid_floor(grd(adjs[2])) && feat_has_solid_floor(grd(adjs[3]))
            || feat_has_solid_floor(grd(adjs[0])) && feat_has_solid_floor(grd(adjs[1]))
               && feat_is_wall(grd(adjs[2])) && feat_is_wall(grd(adjs[3])));
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
        mpr("Here's the list of targets: ", MSGCH_DIAGNOSTICS);
        for (unsigned int i = 0; i < targets.size(); i++)
        {
            snprintf(info, INFO_SIZE, "(%d, %d)  ", targets[i].x, targets[i].y);
            path_str += info;
        }
        mpr(path_str.c_str(), MSGCH_DIAGNOSTICS);
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
            {
                mpr("Something went badly wrong - no path found!",
                    MSGCH_DIAGNOSTICS);
            }
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

static bool _food_item_needs_time_check(item_def &item)
{
    if (!item.defined())
        return false;

    if (item.base_type != OBJ_CORPSES
        && item.base_type != OBJ_FOOD
        && item.base_type != OBJ_POTIONS)
    {
        return false;
    }

    if (item.base_type == OBJ_CORPSES
        && item.sub_type > CORPSE_SKELETON)
    {
        return false;
    }

    if (item.base_type == OBJ_FOOD && item.sub_type != FOOD_CHUNK)
        return false;

    if (item.base_type == OBJ_POTIONS && !is_blood_potion(item))
        return false;

    // The object specifically asks not to be checked:
    if (item.props.exists(CORPSE_NEVER_DECAYS))
        return false;

    return true;
}

#define ROTTING_WARNED_KEY "rotting_warned"

static void _rot_inventory_food(int time_delta)
{
    // Update all of the corpses and food chunks in the player's
    // inventory. {should be moved elsewhere - dlb}
    bool burden_changed_by_rot = false;
    vector<char> rotten_items;

    int num_chunks         = 0;
    int num_chunks_gone    = 0;
    int num_bones          = 0;
    int num_bones_gone     = 0;
    int num_corpses        = 0;
    int num_corpses_rotted = 0;
    int num_corpses_gone   = 0;

    for (int i = 0; i < ENDOFPACK; i++)
    {
        item_def &item(you.inv[i]);

        if (item.quantity < 1)
            continue;

        if (!_food_item_needs_time_check(item))
            continue;

        if (item.base_type == OBJ_POTIONS)
        {
            // Also handles messaging.
            if (maybe_coagulate_blood_potions_inv(item))
                burden_changed_by_rot = true;
            continue;
        }

        if (item.base_type == OBJ_FOOD)
            num_chunks++;
        else if (item.sub_type == CORPSE_SKELETON)
            num_bones++;
        else
            num_corpses++;

        // Food item timed out -> make it disappear.
        if ((time_delta / 20) >= item.special)
        {
            if (item.base_type == OBJ_FOOD)
            {
                if (you.equip[EQ_WEAPON] == i)
                    unwield_item();

                // In case time_delta >= 220
                if (!item.props.exists(ROTTING_WARNED_KEY))
                    num_chunks_gone++;

                item_was_destroyed(item);
                destroy_item(item);
                burden_changed_by_rot = true;

                continue;
            }

            // The item is of type carrion.
            if (item.sub_type == CORPSE_SKELETON
                || !mons_skeleton(item.mon_type))
            {
                if (you.equip[EQ_WEAPON] == i)
                    unwield_item();

                if (item.sub_type == CORPSE_SKELETON)
                    num_bones_gone++;
                else
                    num_corpses_gone++;

                item_was_destroyed(item);
                destroy_item(item);
                burden_changed_by_rot = true;
                continue;
            }

            turn_corpse_into_skeleton(item);
            if (you.equip[EQ_WEAPON] == i)
                you.wield_change = true;
            burden_changed_by_rot = true;

            num_corpses_rotted++;
            continue;
        }

        // If it hasn't disappeared, reduce the rotting timer.
        item.special -= (time_delta / 20);

        if (food_is_rotten(item)
            && (item.special + (time_delta / 20) > ROTTING_CORPSE))
        {
            rotten_items.push_back(index_to_letter(i));
            if (you.equip[EQ_WEAPON] == i)
                you.wield_change = true;
        }
    }

    //mv: messages when chunks/corpses become rotten
    if (!rotten_items.empty())
    {
        string msg = "";

        // Races that can't smell don't care, and trolls are stupid and
        // don't care.
        if (you.can_smell() && you.species != SP_TROLL)
        {
            int temp_rand = 0; // Grr.
            int level = player_mutation_level(MUT_SAPROVOROUS);
            if (!level && you.species == SP_VAMPIRE)
                level = 1;

            switch (level)
            {
            // level 1 and level 2 saprovores, as well as vampires, aren't so touchy
            case 1:
            case 2:
                temp_rand = random2(8);
                msg = (temp_rand  < 5) ? "You smell something rotten." :
                      (temp_rand == 5) ? "You smell rotting flesh." :
                      (temp_rand == 6) ? "You smell decay."
                                       : "There is something rotten in your inventory.";
                break;

            // level 3 saprovores like it
            case 3:
                temp_rand = random2(8);
                msg = (temp_rand  < 5) ? "You smell something rotten." :
                      (temp_rand == 5) ? "The smell of rotting flesh makes you hungry." :
                      (temp_rand == 6) ? "You smell decay. Yum-yum."
                                       : "Wow! There is something tasty in your inventory.";
                break;

            default:
                temp_rand = random2(8);
                msg = (temp_rand  < 5) ? "You smell something rotten." :
                      (temp_rand == 5) ? "The smell of rotting flesh makes you sick." :
                      (temp_rand == 6) ? "You smell decay. Yuck!"
                                       : "Ugh! There is something really disgusting in your inventory.";
                break;
            }
        }
        else
            msg = "Something in your inventory has become rotten.";

        mprf(MSGCH_ROTTEN_MEAT, "%s (slot%s %s)",
             msg.c_str(),
             rotten_items.size() > 1 ? "s" : "",
             comma_separated_line(rotten_items.begin(),
                                  rotten_items.end()).c_str());

        learned_something_new(HINT_ROTTEN_FOOD);
    }

    if (burden_changed_by_rot)
    {
        if ((num_chunks_gone + num_bones_gone + num_corpses_gone
             + num_corpses_rotted) > 0)
        {
            string msg;
            if (num_chunks_gone == num_chunks
                && num_bones_gone == num_bones
                && (num_corpses_gone + num_corpses_rotted) == num_corpses)
            {
                msg = "All of the ";
            }
            else
                msg = "Some of the ";

            vector<string> strs;
            if (num_chunks_gone > 0)
                strs.push_back("chunks of flesh");
            if (num_bones_gone > 0)
                strs.push_back("skeletons");
            if ((num_corpses_gone + num_corpses_rotted) > 0)
                strs.push_back("corpses");

            msg += comma_separated_line(strs.begin(), strs.end());
            msg += " in your inventory have ";

            if (num_corpses_rotted == 0)
                msg += "completely ";
            else if ((num_chunks_gone + num_bones_gone
                      + num_corpses_gone) == 0)
            {
                msg += "partially ";
            }
            else
                msg += "completely or partially ";

            msg += "rotted away.";
            mprf(MSGCH_ROTTEN_MEAT, "%s", msg.c_str());
        }
        burden_change();
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

    if (you.duration[DUR_REGENERATION] && you.species == SP_DJINNI)
        added_contamination += 20;

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

static void _magic_contamination_effects()
{
    // [ds] Move magic contamination effects closer to b26 again.
    const bool glow_effect = get_contamination_level() > 1
            && x_chance_in_y(you.magic_contamination, 12000);

    if (glow_effect && is_sanctuary(you.pos()))
    {
        mpr("Your body momentarily shudders from a surge of wild "
            "energies until Zin's power calms it.", MSGCH_GOD);
    }
    else if (glow_effect)
    {
        mpr("Your body shudders with the violent release "
            "of wild energies!", MSGCH_WARN);

        // For particularly violent releases, make a little boom.
        if (you.magic_contamination > 10000 && coinflip())
        {
            bolt beam;

            beam.flavour      = BEAM_RANDOM;
            beam.glyph        = dchar_glyph(DCHAR_FIRED_BURST);
            beam.damage       = dice_def(3,
                                 div_rand_round(you.magic_contamination, 2000));
            beam.target       = you.pos();
            beam.name         = "magical storm";
            beam.beam_source  = NON_MONSTER;
            beam.aux_source   = "a magical explosion";
            beam.ex_size      = max(1, min(9,
                               div_rand_round(you.magic_contamination, 15000)));
            beam.ench_power   = div_rand_round(you.magic_contamination, 200);
            beam.is_explosion = true;

            // Undead enjoy extra contamination explosion damage because
            // the magical contamination has a harder time dissipating
            // through non-living flesh. :-)
            if (you.is_undead)
                beam.damage.size *= 2;

            beam.explode();
        }

        // We want to warp the player, not do good stuff!
        mutate(one_chance_in(5) ? RANDOM_MUTATION : RANDOM_BAD_MUTATION,
               "mutagenic glow", true,
               coinflip(),
               false, false, false, false,
               you.species == SP_DJINNI);

        // we're meaner now, what with explosions and whatnot, but
        // we dial down the contamination a little faster if its actually
        // mutating you.  -- GDL
        contaminate_player(-(random2(you.magic_contamination / 4) + 1000));
    }
}

// Get around C++ dividing integers towards 0.
static int _div(int num, int denom)
{
    div_t res = div(num, denom);
    return (res.rem >= 0 ? res.quot : res.quot - 1);
}

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

    // Every 20 turns, a variety of other effects.
    if (! (_div(base_time, 200) > _div(old_time, 200)))
        return;

    int time_delta = 200;

    // Update all of the corpses, food chunks, and potions of blood on
    // the floor.
    _update_corpses(time_delta);

    if (crawl_state.game_is_arena())
        return;

    // Nasty things happen to people who spend too long in Hell.
    if (player_in_hell() && coinflip())
        _hell_effects();

    // Adjust the player's stats if s/he's diseased (or recovering).
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

        // Slow heal mutation.  Applied last.
        // Each level reduces your stat recovery by one third.
        if (player_mutation_level(MUT_SLOW_HEALING) > 0
            && x_chance_in_y(player_mutation_level(MUT_SLOW_HEALING), 3))
        {
            recovery = false;
        }

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
            mpr("Your disease is taking its toll.", MSGCH_WARN);
            lose_stat(STAT_RANDOM, 1, false, "disease");
        }
    }

    // Bad effects from magic contamination.
    if (coinflip())
        _magic_contamination_effects();

    // Adjust the player's stats if s/he has the deterioration mutation.
    if (player_mutation_level(MUT_DETERIORATION)
        && x_chance_in_y(player_mutation_level(MUT_DETERIORATION) * 5 - 1, 200))
    {
        lose_stat(STAT_RANDOM, 1, false, "deterioration mutation");
    }

    // Check to see if an upset god wants to do something to the player.
    handle_god_time();

    if (player_mutation_level(MUT_SCREAM)
        && x_chance_in_y(3 + player_mutation_level(MUT_SCREAM) * 3, 100)
        && !(you.duration[DUR_WATER_HOLD] && !you.res_water_drowning()))
    {
        yell(true);
    }

    _rot_inventory_food(time_delta);

    // Exercise armour *xor* stealth skill: {dlb}
    practise(EX_WAIT);

    // From time to time change a section of the labyrinth.
    if (player_in_branch(BRANCH_LABYRINTH) && one_chance_in(10))
        change_labyrinth();

    if (player_in_branch(BRANCH_ABYSS))
    {
        // Update the abyss speed. This place is unstable and the speed can
        // fluctuate. It's not a constant increase.
        if (you_worship(GOD_CHEIBRIADOS) && coinflip())
            ; // Speed change less often for Chei.
        else if (coinflip() && you.abyss_speed < 100)
            ++you.abyss_speed;
        else if (one_chance_in(5) && you.abyss_speed > 0)
            --you.abyss_speed;
    }

    if (you_worship(GOD_JIYVA) && one_chance_in(10))
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

    if (you_worship(GOD_JIYVA) && x_chance_in_y(you.piety / 4, MAX_PIETY)
        && !player_under_penance() && one_chance_in(4))
    {
        jiyva_stat_action();
    }

    if (you_worship(GOD_JIYVA) && one_chance_in(25))
        jiyva_eat_offlevel_items();

    if (int lev = player_mutation_level(MUT_EVOLUTION))
        if (one_chance_in(100 / lev)
            && you.attribute[ATTR_EVOL_XP] * (1 + random2(10))
               > (int)exp_needed(you.experience_level + 1))
        {
            you.attribute[ATTR_EVOL_XP] = 0;
            mpr("You feel a genetic drift.");
            bool evol = one_chance_in(5) ?
                delete_mutation(RANDOM_BAD_MUTATION, "evolution", false) :
                mutate(coinflip() ? RANDOM_GOOD_MUTATION : RANDOM_MUTATION,
                       "evolution", false, false, false, false, false, true);
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

// Move monsters around to fake them walking around while player was
// off-level. Also let them go back to sleep eventually.
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

    if (mon->is_projectile())
    {
        iood_catchup(mon, turns);
        return;
    }

    // Let sleeping monsters lie.
    if (mon->asleep() || mon->paralysed())
        return;

    const int range = (turns * mon->speed) / 10;
    const int moves = (range > 50) ? 50 : range;

    // probably too annoying even for DEBUG_DIAGNOSTICS
    dprf("mon #%d: range %d; "
         "pos (%d,%d); targ %d(%d,%d); flags %" PRIx64,
         mon->mindex(), range, mon->pos().x, mon->pos().y,
         mon->foe, mon->target.x, mon->target.y, mon->flags);

    if (range <= 0)
        return;

    // After x turns, half of the monsters will have forgotten about the
    // player, and a quarter has gone to sleep. A given monster has a
    // 95% chance of forgetting the player after 4*x turns, and going to
    // sleep after 10*x turns.
    int x = 0; // Quiet unitialized variable compiler warning.
    switch (mons_intel(mon))
    {
    case I_HIGH:
        x = 1000;
        break;
    case I_NORMAL:
        x = 500;
        break;
    case I_ANIMAL:
    case I_REPTILE:
    case I_INSECT:
        x = 250;
        break;
    case I_PLANT:
        x = 125;
        break;
    }

    bool changed = false;
    for  (int i = 0; i < range/x; i++)
    {
        if (mon->behaviour == BEH_SLEEP)
            break;

        if (coinflip())
        {
            changed = true;
            if (coinflip())
                mon->behaviour = BEH_SLEEP;
            else
            {
                mon->behaviour = BEH_WANDER;
                mon->foe = MHITNOT;
                mon->target = random_in_bounds();
            }
        }
    }

    if (mons_has_ranged_attack(mon) && !changed)
    {
        // If we're doing short time movement and the monster has a
        // ranged attack (missile or spell), then the monster will
        // flee to gain distance if it's "too close", else it will
        // just shift its position rather than charge the player. -- bwr
        if (grid_distance(mon->pos(), mon->target) < 3)
        {
            mon->behaviour = BEH_FLEE;

            // If the monster is on the target square, fleeing won't
            // work.
            if (mon->pos() == mon->target)
            {
                if (in_bounds(env.old_player_pos)
                    && env.old_player_pos != mon->pos())
                {
                    // Flee from player's old position if different.
                    mon->target = env.old_player_pos;
                }
                else
                {
                    coord_def mshift(random2(3) - 1, random2(3) - 1);

                    // Bounds check: don't let fleeing monsters try to
                    // run off the grid.
                    const coord_def s = mon->target + mshift;
                    if (!in_bounds_x(s.x))
                        mshift.x = 0;
                    if (!in_bounds_y(s.y))
                        mshift.y = 0;

                    // Randomise the target so we have a direction to
                    // flee.
                    mon->target.x += mshift.x;
                    mon->target.y += mshift.y;
                }
            }

            dprf("backing off...");
        }
        else
        {
            shift_monster(mon, mon->pos());
            dprf("shifted to (%d, %d)", mon->pos().x, mon->pos().y);
            return;
        }
    }

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

    if (!shift_monster(mon, pos))
        shift_monster(mon, mon->pos());

    // Submerge monsters that fell asleep, as on placement.
    if (changed && mon->behaviour == BEH_SLEEP
        && monster_can_submerge(mon, grd(mon->pos()))
        && !one_chance_in(5))
    {
        mon->add_ench(ENCH_SUBMERGED);
    }

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

    _update_corpses(elapsedTime);
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

        mi->heal(div_rand_round(turns * mons_off_level_regen_rate(*mi), 100));

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

// A comparison struct for use in an stl priority queue.
template<typename T>
struct greater_second
{
    // The stl priority queue is a max queue and uses < as the default
    // comparison.  We want a min queue so we have to use a > operation
    // here.
    bool operator()(const T & left, const T & right)
    {
        return (left.second > right.second);
    }
};

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
            spawned_count++;
            if (you.see_cell(ring_points.at(i)))
                seen_count++;
    }

    return spawned_count;
}

// Collect lists of points that are within LOS (under the given env map),
// unoccupied, and not solid (walls/statues).
void collect_radius_points(vector<vector<coord_def> > &radius_points,
                           const coord_def &origin, const los_base* los)
{
    radius_points.clear();
    radius_points.resize(LOS_RADIUS);

    // Just want to associate a point with a distance here for convenience.
    typedef pair<coord_def, int> coord_dist;

    // Using a priority queue because squares don't make very good circles at
    // larger radii.  We will visit points in order of increasing euclidean
    // distance from the origin (not path distance).
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
            if (!los->see_cell(temp.first))
                continue;

            coord_def local = temp.first - origin;

            temp.second = local.abs();

            idx = temp.first.x + temp.first.y * X_WIDTH;

            if (visited_indices.find(idx) == visited_indices.end()
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

    los_def los(corpse.pos, opc_solid);

    collect_radius_points(radius_points, corpse.pos, &los);

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
                   corpse.colour);

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
                                  corpse.colour),
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

            if (visited_indices.find(index) == visited_indices.end()
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
    if (seen_targets > 0)
    {
        string what  = seen_targets  > 1 ? "Some toadstools"
                                         : "A toadstool";
        string where = seen_corpses  > 1 ? "nearby corpses" :
                       seen_corpses == 1 ? "a nearby corpse"
                                         : "the ground";
        mprf("%s grow%s from %s.",
             what.c_str(), seen_targets > 1 ? "" : "s", where.c_str());

        return true;
    }

    return false;
}

//---------------------------------------------------------------
//
// update_corpses
//
// Update all of the corpses and food chunks on the floor.
//
//---------------------------------------------------------------
static void _update_corpses(int elapsedTime)
{
    if (elapsedTime <= 0)
        return;

    const int rot_time = elapsedTime / 20;

    for (int c = 0; c < MAX_ITEMS; ++c)
    {
        item_def &it = mitm[c];

        if (!_food_item_needs_time_check(it))
            continue;

        if (it.base_type == OBJ_POTIONS)
        {
            if (is_shop_item(it))
                continue;
            maybe_coagulate_blood_potions_floor(c);
            continue;
        }

        if (rot_time >= it.special && !is_being_butchered(it))
        {
            if (it.base_type == OBJ_FOOD)
                destroy_item(c);
            else
            {
                if (it.sub_type == CORPSE_SKELETON
                    || !mons_skeleton(it.mon_type))
                {
                    item_was_destroyed(it);
                    destroy_item(c);
                }
                else
                    turn_corpse_into_skeleton(it);
            }
        }
        else
            it.special -= rot_time;
    }
}

static void _recharge_rod(item_def &rod, int aut, bool in_inv)
{
    if (rod.base_type != OBJ_RODS || rod.plus >= rod.plus2)
        return;

    // Skill calculations with a massive scale would overflow, cap it.
    // The worst case, a -3 rod, takes 17000 aut to fully charge.
    // -4 rods don't recharge at all.
    aut = min(aut, MAX_ROD_CHARGE * ROD_CHARGE_MULT * 10);

    int rate = 4 + rod.special;

    rate *= 10 * aut + skill_bump(SK_EVOCATIONS, aut);
    rate = div_rand_round(rate, 100);

    if (rate > rod.plus2 - rod.plus) // Prevent overflow
        rate = rod.plus2 - rod.plus;

    // With this, a +0 rod with no skill gets 1 mana per 25.0 turns

    if (rod.plus / ROD_CHARGE_MULT != (rod.plus + rate) / ROD_CHARGE_MULT)
    {
        if (item_is_equipped(rod, true))
            you.wield_change = true;
    }

    rod.plus += rate;

    if (in_inv && rod.plus == rod.plus2)
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
            splash_with_acid(strength, NON_MONSTER, false,
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

void recharge_elemental_evokers(int exp)
{
    vector<item_def*> evokers;
    for (int item = 0; item < ENDOFPACK; ++item)
    {
        if (is_elemental_evoker(you.inv[item]) && you.inv[item].plus2 > 0)
            evokers.push_back(&you.inv[item]);
    }

    int xp_factor = max(min((int)exp_needed(you.experience_level+1, 0) * 2 / 7,
                             you.experience_level * 425),
                        you.experience_level*4 + 30)
                    / (3 + you.skill_rdiv(SK_EVOCATIONS, 2, 13));

    for (unsigned int i = 0; i < evokers.size(); ++i)
    {
        item_def* evoker = evokers[i];
        evoker->plus2 -= div_rand_round(exp, xp_factor);
        if (evoker->plus2 <= 0)
        {
            evoker->plus2 = 0;
            mprf("Your %s has recharged.", evoker->name(DESC_QUALNAME).c_str());
        }
    }
}
