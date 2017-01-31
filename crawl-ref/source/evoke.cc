/**
 * @file
 * @brief Functions for using some of the wackier inventory items.
**/

#include "AppHdr.h"

#include "evoke.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

#include "act-iter.h"
#include "areas.h"
#include "artefact.h"
#include "branch.h"
#include "chardump.h"
#include "cloud.h"
#include "coordit.h"
#include "decks.h"
#include "delay.h"
#include "directn.h"
#include "dungeon.h"
#include "english.h"
#include "env.h"
#include "exercise.h"
#include "fight.h"
#include "food.h"
#include "ghost.h"
#include "god-abil.h"
#include "god-conduct.h"
#include "god-wrath.h"
#include "invent.h"
#include "item-prop.h"
#include "items.h"
#include "item-use.h"
#include "libutil.h"
#include "losglobal.h"
#include "mapmark.h"
#include "melee-attack.h"
#include "message.h"
#include "mgen-data.h"
#include "misc.h"
#include "mon-behv.h"
#include "mon-clone.h"
#include "mon-pick.h"
#include "mon-place.h"
#include "mutant-beast.h"
#include "nearby-danger.h"
#include "place.h"
#include "player.h"
#include "player-stats.h"
#include "prompt.h"
#include "religion.h"
#include "shout.h"
#include "skills.h"
#include "spl-book.h"
#include "spl-cast.h"
#include "spl-clouds.h"
#include "spl-util.h"
#include "spl-zap.h"
#include "state.h"
#include "stringutil.h"
#include "target.h"
#include "terrain.h"
#include "throw.h"
#ifdef USE_TILE
 #include "tilepick.h"
#endif
#include "transform.h"
#include "traps.h"
#include "unicode.h"
#include "viewchar.h"
#include "view.h"
#include "xom.h"

static bool _reaching_weapon_attack(const item_def& wpn)
{
    if (you.confused())
    {
        canned_msg(MSG_TOO_CONFUSED);
        return false;
    }

    if (you.caught())
    {
        mprf("You cannot attack while %s.", held_status());
        return false;
    }

    bool targ_mid = false;
    dist beam;

    direction_chooser_args args;
    args.restricts = DIR_TARGET;
    args.mode = TARG_HOSTILE;
    args.range = 2;
    args.top_prompt = "Attack whom?";
    args.self = CONFIRM_CANCEL;
    targeter_reach hitfunc(&you, REACH_TWO);
    args.hitfunc = &hitfunc;

    direction(beam, args);

    if (!beam.isValid)
    {
        if (beam.isCancel)
            canned_msg(MSG_OK);
        return false;
    }

    if (beam.isMe())
    {
        canned_msg(MSG_UNTHINKING_ACT);
        return false;
    }

    const coord_def delta = beam.target - you.pos();
    const int x_distance  = abs(delta.x);
    const int y_distance  = abs(delta.y);
    monster* mons = monster_at(beam.target);
    // don't allow targeting of submerged monsters
    if (mons && mons->submerged())
        mons = nullptr;

    const int x_first_middle = you.pos().x + (delta.x)/2;
    const int y_first_middle = you.pos().y + (delta.y)/2;
    const int x_second_middle = beam.target.x - (delta.x)/2;
    const int y_second_middle = beam.target.y - (delta.y)/2;
    const coord_def first_middle(x_first_middle, y_first_middle);
    const coord_def second_middle(x_second_middle, y_second_middle);

    if (x_distance > 2 || y_distance > 2)
    {
        mpr("Your weapon cannot reach that far!");
        return false;
    }

    // Calculate attack delay now in case we have to apply it.
    const int attack_delay = you.attack_delay().roll();

    if (!feat_is_reachable_past(grd(first_middle))
        && !feat_is_reachable_past(grd(second_middle)))
    {
        canned_msg(MSG_SOMETHING_IN_WAY);
        return false;
    }

    // Failing to hit someone due to a friend blocking is infuriating,
    // shadow-boxing empty space is not (and would be abusable to wait
    // with no penalty).
    if (mons)
        you.apply_berserk_penalty = false;

    // Choose one of the two middle squares (which might be the same).
    const coord_def middle =
        !feat_is_reachable_past(grd(first_middle)) ? second_middle :
        !feat_is_reachable_past(grd(second_middle)) ? first_middle :
        random_choose(first_middle, second_middle);

    // Check for a monster in the way. If there is one, it blocks the reaching
    // attack 50% of the time, and the attack tries to hit it if it is hostile.

    // If we're attacking more than a space away...
    if (x_distance > 1 || y_distance > 1)
    {
        bool success = true;
        monster *midmons;
        if ((midmons = monster_at(middle))
            && !midmons->submerged())
        {
            // This chance should possibly depend on your skill with
            // the weapon.
            if (coinflip())
            {
                success = false;
                beam.target = middle;
                mons = midmons;
                targ_mid = true;
                if (mons->wont_attack())
                {
                    // Let's assume friendlies cooperate.
                    mpr("You could not reach far enough!");
                    you.time_taken = attack_delay;
                    make_hungry(3, true);
                    return true;
                }
            }
        }
        if (success)
            mpr("You reach to attack!");
        else
        {
            mprf("%s is in the way.",
                 mons->observable() ? mons->name(DESC_THE).c_str()
                                    : "Something you can't see");
        }
    }

    if (mons == nullptr)
    {
        // Must return true, otherwise you get a free discovery
        // of invisible monsters.
        mpr("You attack empty space.");
        you.time_taken = attack_delay;
        make_hungry(3, true);
        return true;
    }
    else if (!fight_melee(&you, mons))
    {
        if (targ_mid)
        {
            // turn_is_over may have been reset to false by fight_melee, but
            // a failed attempt to reach further should not be free; instead,
            // charge the same as a successful attempt.
            you.time_taken = attack_delay;
            make_hungry(3, true);
            you.turn_is_over = true;
        }
        else
            return false;
    }

    return true;
}

static bool _evoke_horn_of_geryon()
{
    bool created = false;

    if (silenced(you.pos()))
    {
        mpr("You can't produce a sound!");
        return false;
    }

    const int surge = pakellas_surge_devices();
    surge_power(you.spec_evoke() + surge);
    mprf(MSGCH_SOUND, "You produce a hideous howling noise!");
    did_god_conduct(DID_EVIL, 3);
    int num = 1;
    const int adjusted_power =
        player_adjust_evoc_power(you.skill(SK_EVOCATIONS, 10), surge);
    if (adjusted_power + random2(90) > 130)
        ++num;
    if (adjusted_power + random2(90) > 180)
        ++num;
    if (adjusted_power + random2(90) > 230)
        ++num;
    for (int n = 0; n < num; ++n)
    {
        monster* mon;
        beh_type beh = BEH_HOSTILE;
        bool will_anger = player_will_anger_monster(MONS_HELL_BEAST);

        if (!will_anger && random2(adjusted_power) > 7)
            beh = BEH_FRIENDLY;
        mgen_data mg(MONS_HELL_BEAST, beh, you.pos(), MHITYOU, MG_FORCE_BEH);
        mg.set_summoned(&you, 3, SPELL_NO_SPELL);
        mg.set_prox(PROX_CLOSE_TO_PLAYER);
        mon = create_monster(mg);
        if (mon)
            created = true;
        if (mon && will_anger)
        {
            mprf("%s is enraged by your holy aura!",
                 mon->name(DESC_THE).c_str());
        }
    }
    if (!created)
        mpr("Nothing answers your call.");
    return true;
}

static bool _check_crystal_ball()
{
#if TAG_MAJOR_VERSION == 34
    if (you.species == SP_DJINNI)
    {
        mpr("These balls have not yet been approved for use by djinn. "
            "(OOC: they're supposed to work, but need a redesign.)");
        return false;
    }
#endif

    if (you.confused())
    {
        canned_msg(MSG_TOO_CONFUSED);
        return false;
    }

    if (!enough_mp(1, false))
    {
        mpr("Your reserves of magic are too empty for the crystal ball to "
            "function.");
        return false;
    }

    if (you.magic_points == you.max_magic_points)
    {
        canned_msg(MSG_FULL_MAGIC);
        return false;
    }

    if (you.skill(SK_EVOCATIONS) < 2)
    {
        mpr("You lack the skill to use this item.");
        return false;
    }

    return true;
}

/**
 * Spray lightning in all directions. (Randomly: shock, lightning bolt, OoE.)
 *
 * @param range         The range of the beams. (As with all beams, eventually
 *                      capped at LOS.)
 * @param power         The power of the beams. (Affects damage.)
 */
static void _spray_lightning(int range, int power)
{
    const zap_type which_zap = random_choose(ZAP_SHOCK,
                                             ZAP_LIGHTNING_BOLT,
                                             ZAP_ORB_OF_ELECTRICITY);

    bolt beam;
    // range has no tracer, so randomness is ok
    beam.range = range;
    beam.source = you.pos();
    beam.target = you.pos() + coord_def(random2(13)-6, random2(13)-6);
    // Non-controlleable, so no player tracer.
    zapping(which_zap, power, beam);
}

/**
 * Evoke a lightning rod, creating an arc of lightning that can be sustained
 * by continuing to evoke.
 *
 * @return  Whether anything happened.
 */
static bool _lightning_rod()
{
    if (you.confused())
    {
        canned_msg(MSG_TOO_CONFUSED);
        return false;
    }

    const int surge = pakellas_surge_devices();
    surge_power(you.spec_evoke() + surge);
    const int power =
        player_adjust_evoc_power(5 + you.skill(SK_EVOCATIONS, 3), surge);

    const spret_type ret = your_spells(SPELL_THUNDERBOLT, power, false);

    if (ret == SPRET_ABORT)
        return false;

    return true;
}

/**
 * Spray lightning in all directions around the player.
 *
 * Quantity, range & power increase with level.
 */
void black_drac_breath()
{
    const int num_shots = roll_dice(2, 1 + you.experience_level / 7);
    const int range = you.experience_level / 3 + 5; // 5--14
    const int power = 25 + you.experience_level; // 25-52
    for (int i = 0; i < num_shots; ++i)
        _spray_lightning(range, power);
}

/**
 * Returns the MP cost of zapping a wand. Usually zero.
 */
int wand_mp_cost()
{
    // Update mutation-data.h when updating this value.
    return player_mutation_level(MUT_MP_WANDS) * 3;
}

void zap_wand(int slot)
{
    if (!form_can_use_wand())
    {
        mpr("You have no means to grasp a wand firmly enough.");
        return;
    }

    if (inv_count() < 1)
    {
        canned_msg(MSG_NOTHING_CARRIED);
        return;
    }

    if (you.confused())
    {
        canned_msg(MSG_TOO_CONFUSED);
        return;
    }

    if (you.berserk())
    {
        canned_msg(MSG_TOO_BERSERK);
        return;
    }

    if (player_mutation_level(MUT_NO_ARTIFICE))
    {
        mpr("You cannot evoke magical items.");
        return;
    }

    if (player_under_penance(GOD_PAKELLAS))
    {
        simple_god_message("'s wrath prevents you from evoking devices!",
                           GOD_PAKELLAS);
        return;
    }

    const int mp_cost = wand_mp_cost();
    if (!enough_mp(mp_cost, false))
        return;

    int item_slot;
    if (slot != -1)
        item_slot = slot;
    else
    {
        item_slot = prompt_invent_item("Zap which item?",
                                       MT_INVLIST,
                                       OBJ_WANDS,
                                       OPER_ZAP);
    }

    if (prompt_failed(item_slot))
        return;

    item_def& wand = you.inv[item_slot];
    if (wand.base_type != OBJ_WANDS)
    {
        mpr("You can't zap that!");
        return;
    }
    if (item_type_removed(wand.base_type, wand.sub_type))
    {
        mpr("Sorry, this wand was removed!");
        return;
    }
    // If you happen to be wielding the wand, its display might change.
    if (you.equip[EQ_WEAPON] == item_slot)
        you.wield_change = true;

    const bool has_charges = wand.charges > 0;
    if (!has_charges && wand.used_count == ZAPCOUNT_EMPTY)
    {
        mpr("This wand has no charges.");
        return;
    }

    // Will waste charges.
    const bool wasteful = !item_ident(wand, ISFLAG_KNOW_PLUSES);
    int power = (15 + you.skill(SK_EVOCATIONS, 7) / 2)
                * (player_mutation_level(MUT_MP_WANDS) + 3) / 3;

    const spell_type spell =
        spell_in_wand(static_cast<wand_type>(wand.sub_type));

    spret_type ret = your_spells(spell, power, false, &wand);

    if (ret == SPRET_ABORT)
        return;
    else if (ret == SPRET_FAIL)
    {
        canned_msg(MSG_NOTHING_HAPPENS);
        // It's an empty wand; inscribe it that way.
        wand.used_count = ZAPCOUNT_EMPTY;
        you.turn_is_over = true;
        return;
    }

    // Spend MP.
    dec_mp(mp_cost, false);

    // Take off a charge.
    wand.charges--;
    // And a few more, if you didn't know the wand's charges.
    int wasted_charges = 0;
    if (wasteful)
    {
#ifdef DEBUG_DIAGNOSTICS
        const int initial_charge = wand.plus;
#endif

        wasted_charges = 1 + random2(2); //1-2
        wand.charges = max(0, wand.charges - wasted_charges);

        dprf("Wasted %d charges (wand %d -> %d)", wasted_charges,
             initial_charge, wand.charges);
        mpr("Evoking this partially-identified wand wasted a few charges.");
    }

    // Zap counts count from the last recharge.
    if (wand.used_count == ZAPCOUNT_RECHARGED)
        wand.used_count = 0;
    // Increment zap count.
    if (wand.used_count >= 0)
    {
        wand.used_count++;
        if (wasteful)
            wand.used_count += wasted_charges;
    }

    if (item_type_known(wand)
        && (item_ident(wand, ISFLAG_KNOW_PLUSES)
            || you.skill_rdiv(SK_EVOCATIONS) > random2(27)))
    {
        if (!item_ident(wand, ISFLAG_KNOW_PLUSES))
        {
            mpr("Your skill with magical items lets you calculate "
                "the power of this device...");
        }

        mprf("This wand has %d charge%s left.",
             wand.plus, wand.plus == 1 ? "" : "s");

        set_ident_flags(wand, ISFLAG_KNOW_PLUSES);
    }
    // Mark as empty if necessary.
    if (wand.charges == 0 && wand.flags & ISFLAG_KNOW_PLUSES)
        wand.used_count = ZAPCOUNT_EMPTY;

    practise_evoking(1);
    count_action(CACT_EVOKE, EVOC_WAND);
    alert_nearby_monsters();

    you.turn_is_over = true;
}

int recharge_wand(bool known, const string &pre_msg, int num, int den)
{
    int item_slot = -1;
    bool divine = num >= 0 && den >= 0;
    do
    {
        if (item_slot == -1)
        {
            item_slot = prompt_invent_item("Charge which item?", MT_INVLIST,
                                           OSEL_RECHARGE,
                                           OPER_ANY,
                                           invprompt_flag::escape_only);
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

        int charge_gain = wand_max_charges(wand) / 3;

        const int new_charges =
            divine
            ? min<int>(charge_gain * 3,
                       max<int>(wand.charges + 1,
                                wand.charges + 3 * charge_gain * num / den))
            : max<int>(wand.charges,
                       min(charge_gain * 3,
                           wand.charges +
                           1 + random2avg(((charge_gain - 1) * 3) + 1, 3)));

        const bool charged = (new_charges > wand.plus);

        string desc;

        if (charged && item_ident(wand, ISFLAG_KNOW_PLUSES))
        {
            desc = make_stringf(" and now has %d charge%s",
                                new_charges, new_charges == 1 ? "" : "s");
        }

        if (known && !pre_msg.empty())
            mpr(pre_msg);

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

        you.wield_change = true;
        return 1;
    }
    while (true);

    return 0;
}

// return a slot that has manual for given skill, or -1 if none exists
// in case of multiple manuals the one with the fewest charges is returned
int manual_slot_for_skill(skill_type skill)
{
    int slot = -1;
    int charges = -1;

    FixedVector<item_def,ENDOFPACK>::const_pointer iter = you.inv.begin();
    for (;iter!=you.inv.end(); ++iter)
    {
        if (iter->base_type != OBJ_BOOKS || iter->sub_type != BOOK_MANUAL)
            continue;

        if (iter->skill != skill || iter->skill_points == 0)
            continue;

        if (slot != -1 && iter->skill_points > charges)
            continue;

        slot = iter - you.inv.begin();
        charges = iter->skill_points;
    }

    return slot;
}

bool skill_has_manual(skill_type skill)
{
    return manual_slot_for_skill(skill) != -1;
}

void finish_manual(int slot)
{
    item_def& manual(you.inv[slot]);
    const skill_type skill = static_cast<skill_type>(manual.plus);

    mprf("You have finished your manual of %s and toss it away.",
         skill_name(skill));
    dec_inv_item_quantity(slot, 1);
}

void get_all_manual_charges(vector<int> &charges)
{
    charges.clear();

    FixedVector<item_def,ENDOFPACK>::const_pointer iter = you.inv.begin();
    for (;iter!=you.inv.end(); ++iter)
    {
        if (iter->base_type != OBJ_BOOKS || iter->sub_type != BOOK_MANUAL)
            continue;

        charges.push_back(iter->skill_points);
    }
}

void set_all_manual_charges(const vector<int> &charges)
{
    auto charge_iter = charges.begin();
    for (item_def &item : you.inv)
    {
        if (item.base_type != OBJ_BOOKS || item.sub_type != BOOK_MANUAL)
            continue;

        ASSERT(charge_iter != charges.end());
        item.skill_points = *charge_iter;
        charge_iter++;
    }
    ASSERT(charge_iter == charges.end());
}

string manual_skill_names(bool short_text)
{
    skill_set skills;

    FixedVector<item_def,ENDOFPACK>::const_pointer iter = you.inv.begin();
    for (;iter!=you.inv.end(); ++iter)
    {
        if (iter->base_type != OBJ_BOOKS
            || iter->sub_type != BOOK_MANUAL
            || is_useless_item(*iter))
        {
            continue;
        }

        skills.insert(static_cast<skill_type>(iter->plus));
    }

    if (short_text && skills.size() > 1)
    {
        char buf[40];
        sprintf(buf, "%lu skills", (unsigned long) skills.size());
        return string(buf);
    }
    else
        return skill_names(skills);
}

static const pop_entry pop_spiders[] =
{ // Sack of Spiders
  {  0,  13,   40, FALL, MONS_WORKER_ANT },
  {  0,  13,   80, FALL, MONS_SOLDIER_ANT },
  {  6,  19,   80, PEAK, MONS_REDBACK},
  {  8,  27,   90, PEAK, MONS_REDBACK },
  { 10,  27,   10, SEMI, MONS_ORB_SPIDER },
  { 12,  29,  100, PEAK, MONS_JUMPING_SPIDER },
  { 13,  29,  110, PEAK, MONS_TARANTELLA },
  { 15,  29,  120, PEAK, MONS_WOLF_SPIDER },
  { 21,  27,   18, RISE, MONS_GHOST_MOTH },
  { 0,0,0,FLAT,MONS_0 }
};

static bool _box_of_beasts(item_def &box)
{
    const int surge = pakellas_surge_devices() + you.spec_evoke();
    surge_power(surge);
    mpr("You open the lid...");

    // two rolls to reduce std deviation - +-6 so can get < max even at 27 sk
    const int hd_min = min(27,
                           player_adjust_evoc_power(
                               you.skill(SK_EVOCATIONS)
                               + random2(7) - random2(7), surge));
    const int tier = mutant_beast_tier(hd_min);
    ASSERT(tier < NUM_BEAST_TIERS);

    mgen_data mg(MONS_MUTANT_BEAST, BEH_FRIENDLY, you.pos(), MHITYOU,
                 MG_AUTOFOE);
    mg.set_summoned(&you, 3 + random2(3), 0);

    auto &avoids = mg.props[MUTANT_BEAST_AVOID_FACETS].get_vector();
    for (int facet = BF_FIRST; facet <= BF_LAST; ++facet)
        if (god_hates_beast_facet(you.religion, static_cast<beast_facet>(facet)))
            avoids.push_back(facet);
    mg.hd = beast_tiers[tier];
    dprf("hd %d (min %d, tier %d)", mg.hd, hd_min, tier);
    const monster* mons = create_monster(mg);

    if (!mons)
    {
        // Failed to create monster for some reason
        mpr("...but nothing happens.");
        return false;
    }

    mprf("...and %s %s out!",
         mons->name(DESC_A).c_str(), mons->airborne() ? "flies" : "leaps");
    did_god_conduct(DID_CHAOS, random_range(5,10));

    // After unboxing a beast, chance to break.
    if (one_chance_in(3))
    {
        mpr("The now-empty box falls apart.");
        ASSERT(in_inventory(box));
        dec_inv_item_quantity(box.link, 1);
    }

    return true;
}

static bool _sack_of_spiders_veto_mon(monster_type mon)
{
   // Don't summon any beast that would anger your god.
    return player_will_anger_monster(mon);
}


static bool _sack_of_spiders(item_def &sack)
{
    const int surge = pakellas_surge_devices() + you.spec_evoke();
    surge_power(surge);
    mpr("You reach into the bag...");

    const int evo_skill = you.skill(SK_EVOCATIONS);
    int count = player_adjust_evoc_power(
            1 + random2(2) + random2(div_rand_round(evo_skill * 10, 30)), surge);
    const int power = player_adjust_evoc_power(evo_skill, surge);

    if (x_chance_in_y(5, 10 + power))
    {
        mpr("...but nothing happens.");
        return false;
    }

    bool success = false;

    for (int n = 0; n < count; n++)
    {
        // Invoke mon-pick with our custom list
        monster_type mon = pick_monster_from(pop_spiders,
                                             max(1, min(27,
                                             player_adjust_evoc_power(
                                                 you.skill(SK_EVOCATIONS),
                                                 surge))),
                                             _sack_of_spiders_veto_mon);
        mgen_data mg(mon, BEH_FRIENDLY, you.pos(), MHITYOU, MG_AUTOFOE);
        mg.set_summoned(&you, 3 + random2(4), 0);
        if (create_monster(mg))
            success = true;
    }

    if (success)
    {
        mpr("...and things crawl out!");
        // Also generate webs on hostile monsters and trap them.
        const int rad = LOS_RADIUS / 2 + 2;
        for (monster_near_iterator mi(you.pos(), LOS_SOLID); mi; ++mi)
        {
            trap_def *trap = trap_at((*mi)->pos());
            // Don't destroy non-web traps or try to trap monsters
            // currently caught by something.
            if (you.pos().distance_from((*mi)->pos()) > rad
                || (!trap && grd((*mi)->pos()) != DNGN_FLOOR)
                || (trap && trap->type != TRAP_WEB)
                || (*mi)->friendly()
                || (*mi)->caught())
            {
                continue;
            }

            // web chance increases with proximity & evo skill
            // code here uses double negatives; sorry! i blame the other guy
            // don't try to make surge affect web chance; too messy.
            const int web_dist_factor
                = 100 * (you.pos().distance_from((*mi)->pos()) - 1) / rad;
            const int web_skill_factor = 2 * (27 - you.skill(SK_EVOCATIONS));
            const int web_chance = 100 - web_dist_factor - web_skill_factor;
            if (x_chance_in_y(web_chance, 100))
            {
                if (trap && trap->type == TRAP_WEB)
                    destroy_trap((*mi)->pos());

                place_specific_trap((*mi)->pos(), TRAP_WEB);
                // Reveal the trap
                grd((*mi)->pos()) = DNGN_TRAP_WEB;
                trap = trap_at((*mi)->pos());
                trap->trigger(**mi);
            }

        }
        // After gettin' some bugs, check for destruction.
        if (one_chance_in(3))
        {
            mpr("The now-empty bag unravels in your hand.");
            ASSERT(in_inventory(sack));
            dec_inv_item_quantity(sack.link, 1);
        }
    }
    else
        // Failed to create monster for some reason
        mpr("...but nothing happens.");

    return success;
}

static bool _make_zig(item_def &zig)
{
    if (feat_is_critical(grd(you.pos())))
    {
        mpr("You can't place a gateway to a ziggurat here.");
        return false;
    }
    for (int lev = 1; lev <= brdepth[BRANCH_ZIGGURAT]; lev++)
    {
        if (is_level_on_stack(level_id(BRANCH_ZIGGURAT, lev))
            || you.where_are_you == BRANCH_ZIGGURAT)
        {
            mpr("Finish your current ziggurat first!");
            return false;
        }
    }

    ASSERT(in_inventory(zig));
    dec_inv_item_quantity(zig.link, 1);
    dungeon_terrain_changed(you.pos(), DNGN_ENTER_ZIGGURAT);
    mpr("You set the figurine down, and a mystic portal to a ziggurat forms.");
    return true;
}

static bool _ball_of_energy()
{
    bool ret = false;

    mpr("You gaze into the crystal ball.");
    const int surge = pakellas_surge_devices();
    surge_power(you.spec_evoke() + surge);

    int use = player_adjust_evoc_power(random2(you.skill(SK_EVOCATIONS, 6)),
                                       surge);

    if (use < 2)
        lose_stat(STAT_INT, 1 + random2avg(5, 2));
    else if (use < 5 && enough_mp(1, true))
    {
        mpr("You feel your power drain away!");
        dec_mp(you.magic_points);
    }
    else if (use < 10)
        confuse_player(10 + random2(10));
    else
    {
        int proportional = (you.magic_points * 100) / you.max_magic_points;

        if (random2avg(
                77 - player_adjust_evoc_power(you.skill(SK_EVOCATIONS, 2),
                                              surge), 4)
            > proportional
            || one_chance_in(25))
        {
            mpr("You feel your power drain away!");
            dec_mp(you.magic_points);
        }
        else
        {
            mpr("You are suffused with power!");
            inc_mp(
                player_adjust_evoc_power(
                    5 + random2avg(you.skill(SK_EVOCATIONS), 2), surge));

            ret = true;
        }
    }

    did_god_conduct(DID_CHANNEL, 5, true);

    return ret;
}

static int _num_evoker_elementals(int surge)
{
    int n = 1;
    const int adjusted_power =
        player_adjust_evoc_power(you.skill(SK_EVOCATIONS, 10), surge);
    if (adjusted_power + random2(70) > 110)
        ++n;
    if (adjusted_power + random2(70) > 170)
        ++n;
    return n;
}

static vector<coord_def> _get_jitter_path(coord_def source, coord_def target,
                                          bool jitter_start,
                                          bolt &beam1, bolt &beam2)
{
    const int NUM_TRIES = 10;
    const int RANGE = 8;

    bolt trace_beam;
    trace_beam.source = source;
    trace_beam.target = target;
    trace_beam.aimed_at_spot = false;
    trace_beam.is_tracer = true;
    trace_beam.range = RANGE;
    trace_beam.fire();

    coord_def aim_dir = (source - target).sgn();

    if (trace_beam.path_taken.back() != source)
        target = trace_beam.path_taken.back();

    if (jitter_start)
    {
        for (int n = 0; n < NUM_TRIES; ++n)
        {
            coord_def jitter = clamp_in_bounds(target + coord_def(random_range(-2, 2),
                                                                  random_range(-2, 2)));
            if (jitter == target || jitter == source || cell_is_solid(jitter))
                continue;

            trace_beam.target = jitter;
            trace_beam.fire();

            coord_def delta = source - trace_beam.path_taken.back();
            //Don't try to aim at targets in the opposite direction of main aim
            if ((abs(aim_dir.x - delta.sgn().x) + abs(aim_dir.y - delta.sgn().y) >= 2)
                 && !delta.origin())
            {
                continue;
            }

            target = trace_beam.path_taken.back();
            break;
        }
    }

    vector<coord_def> path = trace_beam.path_taken;
    unsigned int mid_i = (path.size() / 2);
    coord_def mid = path[mid_i];

    for (int n = 0; n < NUM_TRIES; ++n)
    {
        coord_def jitter = clamp_in_bounds(mid + coord_def(random_range(-3, 3),
                                                           random_range(-3, 3)));
        if (jitter == mid || jitter.distance_from(mid) < 2 || jitter == source
            || cell_is_solid(jitter)
            || !cell_see_cell(source, jitter, LOS_NO_TRANS)
            || !cell_see_cell(target, jitter, LOS_NO_TRANS))
        {
            continue;
        }

        trace_beam.aimed_at_feet = false;
        trace_beam.source = jitter;
        trace_beam.target = target;
        trace_beam.fire();

        coord_def delta1 = source - jitter;
        coord_def delta2 = source - trace_beam.path_taken.back();

        //Don't try to aim at targets in the opposite direction of main aim
        if (abs(aim_dir.x - delta1.sgn().x) + abs(aim_dir.y - delta1.sgn().y) >= 2
            || abs(aim_dir.x - delta2.sgn().x) + abs(aim_dir.y - delta2.sgn().y) >= 2)
        {
            continue;
        }

        // Don't make l-turns
        coord_def delta = jitter-target;
        if (!delta.x || !delta.y)
            continue;

        if (find(begin(path), end(path), jitter) != end(path))
            continue;

        mid = jitter;
        break;
    }

    beam1.source = source;
    beam1.target = mid;
    beam1.range = RANGE;
    beam1.aimed_at_spot = true;
    beam1.is_tracer = true;
    beam1.fire();
    beam1.is_tracer = false;

    beam2.source = mid;
    beam2.target = target;
    beam2.range = max(int(RANGE - beam1.path_taken.size()), mid.distance_from(target));
    beam2.is_tracer = true;
    beam2.fire();
    beam2.is_tracer = false;

    vector<coord_def> newpath;
    newpath.insert(newpath.end(), beam1.path_taken.begin(), beam1.path_taken.end());
    newpath.insert(newpath.end(), beam2.path_taken.begin(), beam2.path_taken.end());

    return newpath;
}

static bool _check_path_overlap(const vector<coord_def> &path1,
                                const vector<coord_def> &path2, int match_len)
{
    int max_len = min(path1.size(), path2.size());
    match_len = min(match_len, max_len-1);

    // Check for overlap with previous path
    int matchs = 0;
    for (int i = 0; i < max_len; ++i)
    {
        if (path1[i] == path2[i])
            ++matchs;
        else
            matchs = 0;

        if (matchs >= match_len)
            return true;
    }

    return false;
}

static bool _fill_flame_trails(coord_def source, coord_def target,
                               vector<bolt> &beams, int num)
{
    const int NUM_TRIES = 10;
    vector<vector<coord_def> > paths;
    for (int n = 0; n < num; ++n)
    {
        int tries = 0;
        vector<coord_def> path;
        bolt beam1, beam2;
        while (++tries <= NUM_TRIES && path.empty())
        {
            path = _get_jitter_path(source, target, !paths.empty(), beam1, beam2);
            for (const vector<coord_def> &oldpath : paths)
            {
                if (_check_path_overlap(path, oldpath, 3))
                {
                    path.clear();
                    beam1 = bolt();
                    beam2 = bolt();
                    break;
                }
            }
        }

        if (!path.empty())
        {
            paths.push_back(path);
            beams.push_back(beam1);
            beams.push_back(beam2);
        }
    }

    return !paths.empty();
}

static bool _lamp_of_fire()
{
    bolt base_beam;
    dist target;
    direction_chooser_args args;
    args.restricts = DIR_TARGET;
    args.mode = TARG_HOSTILE;
    args.top_prompt = "Aim the lamp in which direction?";
    args.self = CONFIRM_CANCEL;
    if (spell_direction(target, base_beam, &args))
    {
        if (you.confused())
            target.confusion_fuzz();

        const int surge = pakellas_surge_devices();
        surge_power(you.spec_evoke() + surge);
        did_god_conduct(DID_FIRE, 6 + random2(3));

        mpr("The flames dance!");

        vector<bolt> beams;
        int num_trails = _num_evoker_elementals(surge);

        _fill_flame_trails(you.pos(), target.target, beams, num_trails);

        const int pow =
            player_adjust_evoc_power(8 + you.skill_rdiv(SK_EVOCATIONS, 14, 4),
                                     surge);
        for (bolt &beam : beams)
        {
            if (beam.source == beam.target)
                continue;

            beam.flavour    = BEAM_FIRE;
            beam.colour     = RED;
            beam.source_id  = MID_PLAYER;
            beam.thrower    = KILL_YOU;
            beam.pierce     = true;
            beam.name       = "trail of fire";
            beam.hit        = 10 + (pow/8);
            beam.damage     = dice_def(2, 5 + pow/4);
            beam.ench_power = 3 + (pow/5);
            beam.loudness   = 5;
            beam.fire();
        }
        return true;
    }

    return false;
}

struct dist_sorter
{
    coord_def pos;
    bool operator()(const actor* a, const actor* b)
    {
        return a->pos().distance_from(pos) > b->pos().distance_from(pos);
    }
};

static int _gale_push_dist(const actor* agent, const actor* victim, int pow)
{
    int dist = 1 + random2(pow / 20);

    if (victim->airborne())
        dist++;

    if (victim->body_size(PSIZE_BODY) < SIZE_MEDIUM)
        dist++;
    else if (victim->body_size(PSIZE_BODY) > SIZE_BIG)
        dist /= 2;
    else if (victim->body_size(PSIZE_BODY) > SIZE_MEDIUM)
        dist -= 1;

    int range = victim->pos().distance_from(agent->pos());
    if (range > 5)
        dist -= 2;
    else if (range > 2)
        dist--;

    if (dist < 0)
        return 0;
    else
        return dist;
}

static double _angle_between(coord_def origin, coord_def p1, coord_def p2)
{
    double ang0 = atan2(p1.x - origin.x, p1.y - origin.y);
    double ang  = atan2(p2.x - origin.x, p2.y - origin.y);
    return min(fabs(ang - ang0), fabs(ang - ang0 + 2 * PI));
}

void wind_blast(actor* agent, int pow, coord_def target, bool card)
{
    vector<actor *> act_list;

    int radius = min(5, 4 + div_rand_round(pow, 60));

    for (actor_near_iterator ai(agent->pos(), LOS_SOLID); ai; ++ai)
    {
        if (ai->is_stationary()
            || ai->pos().distance_from(agent->pos()) > radius
            || ai->pos() == agent->pos() // so it's never aimed_at_feet
            || !target.origin()
               && _angle_between(agent->pos(), target, ai->pos()) > PI/4.0)
        {
            continue;
        }

        act_list.push_back(*ai);
    }

    dist_sorter sorter = {agent->pos()};
    sort(act_list.begin(), act_list.end(), sorter);

    bolt wind_beam;
    wind_beam.hit             = AUTOMATIC_HIT;
    wind_beam.pierce          = true;
    wind_beam.affects_nothing = true;
    wind_beam.source          = agent->pos();
    wind_beam.range           = LOS_RADIUS;
    wind_beam.is_tracer       = true;

    map<actor *, coord_def> collisions;

    bool player_affected = false;
    counted_monster_list affected_monsters;

    for (actor *act : act_list)
    {
        wind_beam.target = act->pos();
        wind_beam.fire();

        int push = _gale_push_dist(agent, act, pow);
        bool pushed = false;

        for (unsigned int j = 0; j < wind_beam.path_taken.size() - 1 && push;
             ++j)
        {
            if (wind_beam.path_taken[j] == act->pos())
            {
                coord_def newpos = wind_beam.path_taken[j+1];
                if (!actor_at(newpos) && !cell_is_solid(newpos)
                    && act->can_pass_through(newpos)
                    && act->is_habitable(newpos))
                {
                    act->move_to_pos(newpos);
                    if (act->is_player())
                        stop_delay(true);
                    --push;
                    pushed = true;
                }
                else //Try to find an alternate route to push
                {
                    bool success = false;
                    for (adjacent_iterator di(newpos); di; ++di)
                    {
                        if (adjacent(*di, act->pos())
                            && di->distance_from(agent->pos())
                                == newpos.distance_from(agent->pos())
                            && !actor_at(*di) && !cell_is_solid(*di)
                            && act->can_pass_through(*di)
                            && act->is_habitable(*di))
                        {
                            act->move_to_pos(*di);
                            if (act->is_player())
                                stop_delay(true);

                            --push;
                            pushed = true;

                            // Adjust wind path for moved monster
                            wind_beam.target = *di;
                            wind_beam.fire();
                            success = true;
                            break;
                        }
                    }

                    // If no luck, they slam into something.
                    if (!success)
                        collisions.insert(make_pair(act, newpos));
                }
            }
        }

        if (pushed)
        {
            if (act->is_monster())
            {
                act->as_monster()->speed_increment -= random2(6) + 4;
                if (you.can_see(*act))
                    affected_monsters.add(act->as_monster());
            }
            else
                player_affected = true;
        }
    }

    // Now move clouds
    vector<coord_def> cloud_list;
    for (distance_iterator di(agent->pos(), true, true, radius + 2); di; ++di)
    {
        if (cloud_at(*di)
            && cell_see_cell(agent->pos(), *di, LOS_SOLID)
            && (target.origin()
                || _angle_between(agent->pos(), target, *di) <= PI/4.0))
        {
            cloud_list.push_back(*di);
        }
    }

    for (int i = cloud_list.size() - 1; i >= 0; --i)
    {
        wind_beam.target = cloud_list[i];
        wind_beam.fire();

        int dist = cloud_list[i].distance_from(agent->pos());
        int push = (dist > 5 ? 2 : dist > 2 ? 3 : 4);

        for (unsigned int j = 0;
             j < wind_beam.path_taken.size() - 1 && push;
             ++j)
        {
            if (wind_beam.path_taken[j] == cloud_list[i])
            {
                coord_def newpos = wind_beam.path_taken[j+1];
                if (!cell_is_solid(newpos)
                    && !cloud_at(newpos))
                {
                    swap_clouds(newpos, wind_beam.path_taken[j]);
                    --push;
                }
                else //Try to find an alternate route to push
                {
                    for (distance_iterator di(wind_beam.path_taken[j],
                         false, true, 1); di; ++di)
                    {
                        if (di->distance_from(agent->pos())
                                == newpos.distance_from(agent->pos())
                            && *di != agent->pos() // never aimed_at_feet
                            && !cell_is_solid(*di)
                            && !cloud_at(*di))
                        {
                            swap_clouds(*di, wind_beam.path_taken[j]);
                            --push;
                            wind_beam.target = *di;
                            wind_beam.fire();
                            j--;
                            break;
                        }
                    }
                }
            }
        }
    }

    if (agent->is_player())
    {
        const string source = card ? "card" : "fan";

        if (pow > 120)
            mprf("A mighty gale blasts forth from the %s!", source.c_str());
        else
            mprf("A fierce wind blows from the %s.", source.c_str());
    }

    noisy(8, agent->pos());

    if (player_affected)
        mpr("You are blown backwards!");

    if (!affected_monsters.empty())
    {
        const string message =
            make_stringf("%s %s blown away by the wind.",
                         affected_monsters.describe().c_str(),
                         conjugate_verb("be", affected_monsters.count() > 1).c_str());
        if (strwidth(message) < get_number_of_cols() - 2)
            mpr(message);
        else
            mpr("The monsters around you are blown away!");
    }

    for (auto it : collisions)
        if (it.first->alive())
            it.first->collide(it.second, agent, pow);
}

static bool _phial_of_floods()
{
    dist target;
    bolt beam;

    const int base_pow = 10 + you.skill(SK_EVOCATIONS, 4); // placeholder?
    zappy(ZAP_PRIMAL_WAVE, base_pow, false, beam);
    beam.range = LOS_RADIUS;
    beam.aimed_at_spot = true;

    direction_chooser_args args;
    args.mode = TARG_HOSTILE;
    args.top_prompt = "Aim the phial where?";
    if (spell_direction(target, beam, &args)
        && player_tracer(ZAP_PRIMAL_WAVE, base_pow, beam))
    {
        if (you.confused())
        {
            target.confusion_fuzz();
            beam.set_target(target);
        }

        const int surge = pakellas_surge_devices();
        surge_power(you.spec_evoke() + surge);
        const int power = player_adjust_evoc_power(base_pow, surge);
        // use real power to recalc hit/dam
        zappy(ZAP_PRIMAL_WAVE, power, false, beam);

        beam.fire();

        vector<coord_def> elementals;
        // Flood the endpoint
        coord_def center = beam.path_taken.back();
        int num = player_adjust_evoc_power(
                      5 + you.skill_rdiv(SK_EVOCATIONS, 3, 5) + random2(7),
                      surge);
        int dur = player_adjust_evoc_power(
                      40 + you.skill_rdiv(SK_EVOCATIONS, 8, 3),
                      surge);
        for (distance_iterator di(center, true, false, 2); di && num > 0; ++di)
        {
            const dungeon_feature_type feat = grd(*di);
            if ((feat == DNGN_FLOOR || feat == DNGN_SHALLOW_WATER)
                && cell_see_cell(center, *di, LOS_NO_TRANS))
            {
                num--;
                temp_change_terrain(*di, DNGN_SHALLOW_WATER,
                                    random_range(dur*2, dur*3) - (di.radius()*20),
                                    TERRAIN_CHANGE_FLOOD);
                elementals.push_back(*di);
            }
        }

        int num_elementals = _num_evoker_elementals(surge);

        bool created = false;
        num = min(num_elementals,
                  min((int)elementals.size(), (int)elementals.size() / 5 + 1));
        beh_type attitude = BEH_FRIENDLY;
        if (player_will_anger_monster(MONS_WATER_ELEMENTAL))
            attitude = BEH_HOSTILE;
        for (int n = 0; n < num; ++n)
        {
            mgen_data mg (MONS_WATER_ELEMENTAL, attitude, elementals[n], 0,
                          MG_FORCE_BEH | MG_FORCE_PLACE);
            mg.set_summoned(&you, 3, SPELL_NO_SPELL);
            mg.set_prox(PROX_CLOSE_TO_PLAYER);
            mg.hd = player_adjust_evoc_power(
                        6 + you.skill_rdiv(SK_EVOCATIONS, 2, 15), surge);
            if (create_monster(mg))
                created = true;
        }
        if (created)
            mpr("The water rises up and takes form.");

        return true;
    }

    return false;
}

void expend_xp_evoker(item_def &item)
{
    evoker_debt(item.sub_type) = XP_EVOKE_DEBT;
}

static spret_type _phantom_mirror()
{
    bolt beam;
    monster* victim = nullptr;
    dist spd;
    targeter_smite tgt(&you, LOS_RADIUS, 0, 0);

    direction_chooser_args args;
    args.restricts = DIR_TARGET;
    args.needs_path = false;
    args.self = CONFIRM_CANCEL;
    args.top_prompt = "Aiming: <white>Phantom Mirror</white>";
    args.hitfunc = &tgt;
    if (!spell_direction(spd, beam, &args))
        return SPRET_ABORT;
    victim = monster_at(beam.target);
    if (!victim || !you.can_see(*victim))
    {
        if (beam.target == you.pos())
            mpr("You can't use the mirror on yourself.");
        else
            mpr("You can't see anything there to clone.");
        return SPRET_ABORT;
    }

    // Mirrored monsters (including by Mara, rakshasas) can still be
    // re-reflected.
    if (!actor_is_illusion_cloneable(victim)
        && !victim->has_ench(ENCH_PHANTOM_MIRROR))
    {
        mpr("The mirror can't reflect that.");
        return SPRET_ABORT;
    }

    if (player_will_anger_monster(*victim))
    {
        if (player_mutation_level(MUT_NO_LOVE))
            mpr("The reflection would only feel hate for you!");
        else
            simple_god_message(" forbids your reflecting this monster.");
        return SPRET_ABORT;
    }

    const int surge = pakellas_surge_devices();
    surge_power(you.spec_evoke() + surge);

    monster* mon = clone_mons(victim, true);
    if (!mon)
    {
        canned_msg(MSG_NOTHING_HAPPENS);
        return SPRET_FAIL;
    }
    const int power = player_adjust_evoc_power(5 + you.skill(SK_EVOCATIONS, 3),
                                               surge);
    int dur = min(6, max(1,
                         player_adjust_evoc_power(
                             you.skill(SK_EVOCATIONS, 1) / 4 + 1,
                             surge)
                         * (100 - victim->check_res_magic(power)) / 100));

    mon->attitude = ATT_FRIENDLY;
    mon->mark_summoned(dur, true, SPELL_PHANTOM_MIRROR);

    mon->summoner = MID_PLAYER;
    mons_add_blame(mon, "mirrored by the player character");
    mon->add_ench(ENCH_PHANTOM_MIRROR);
    mon->add_ench(mon_enchant(ENCH_DRAINED,
                              div_rand_round(mon->get_experience_level(), 3),
                              &you, INFINITE_DURATION));

    mon->behaviour = BEH_SEEK;
    set_nearest_monster_foe(mon);

    mprf("You reflect %s with the mirror, and the mirror shatters!",
         victim->name(DESC_THE).c_str());

    return SPRET_SUCCESS;
}

bool evoke_check(int slot, bool quiet)
{
    const bool reaching = slot != -1 && slot == you.equip[EQ_WEAPON]
                          && !you.melded[EQ_WEAPON]
                          && weapon_reach(*you.weapon()) > REACH_NONE;

    if (you.berserk() && !reaching)
    {
        if (!quiet)
            canned_msg(MSG_TOO_BERSERK);
        return false;
    }
    return true;
}

bool evoke_item(int slot, bool check_range)
{
    if (!evoke_check(slot))
        return false;

    if (slot == -1)
    {
        slot = prompt_invent_item("Evoke which item? (* to show all)",
                                   MT_INVLIST,
                                   OSEL_EVOKABLE, OPER_EVOKE);

        if (prompt_failed(slot))
            return false;
    }
    else if (!check_warning_inscriptions(you.inv[slot], OPER_EVOKE))
        return false;

    ASSERT(slot >= 0);

#ifdef ASSERTS // Used only by an assert
    const bool wielded = (you.equip[EQ_WEAPON] == slot);
#endif /* DEBUG */

    item_def& item = you.inv[slot];
    // Also handles messages.
    if (!item_is_evokable(item, true, false, false, true))
        return false;

    bool did_work   = false;  // "Nothing happens" message
    bool unevokable = false;

    const unrandart_entry *entry = is_unrandom_artefact(item)
        ? get_unrand_entry(item.unrand_idx) : nullptr;

    if (entry && entry->evoke_func)
    {
        ASSERT(item_is_equipped(item));

        if (you.confused())
        {
            canned_msg(MSG_TOO_CONFUSED);
            return false;
        }

        bool qret = entry->evoke_func(&item, &did_work, &unevokable);

        if (!unevokable)
            count_action(CACT_EVOKE, item.unrand_idx);

        // what even _is_ this return value?
        if (qret)
            return did_work;
    }
    else switch (item.base_type)
    {
    case OBJ_WANDS:
        zap_wand(slot);
        return true;

    case OBJ_WEAPONS:
        ASSERT(wielded);

        if (weapon_reach(item) > REACH_NONE)
        {
            if (_reaching_weapon_attack(item))
                did_work = true;
            else
                return false;
        }
        else
            unevokable = true;
        break;

    case OBJ_STAVES:
        ASSERT(wielded);
        if (item.sub_type != STAFF_ENERGY)
        {
            unevokable = true;
            break;
        }

        if (you.confused())
        {
            canned_msg(MSG_TOO_CONFUSED);
            return false;
        }

        if (you.undead_state() == US_ALIVE && !you_foodless()
            && you.hunger_state <= HS_STARVING)
        {
            canned_msg(MSG_TOO_HUNGRY);
            return false;
        }
        else if (you.magic_points >= you.max_magic_points
#if TAG_MAJOR_VERSION == 34
                 && (you.species != SP_DJINNI || you.hp == you.hp_max)
#endif
                )
        {
            canned_msg(MSG_FULL_MAGIC);
            return false;
        }
        else if (x_chance_in_y(apply_enhancement(
                                   you.skill(SK_EVOCATIONS, 100) + 1100,
                                   you.spec_evoke()),
                               4000))
        {
            mpr("You channel some magical energy.");
            inc_mp(1 + random2(3));
            make_hungry(50, false, true);
            did_work = true;
            practise_evoking(1);
            count_action(CACT_EVOKE, STAFF_ENERGY, OBJ_STAVES);

            did_god_conduct(DID_CHANNEL, 1, true);
        }
        break;

    case OBJ_MISCELLANY:
        did_work = true; // easier to do it this way for misc items

        if ((player_mutation_level(MUT_NO_ARTIFICE)
             || player_under_penance(GOD_PAKELLAS))
            && !is_deck(item)
            && item.sub_type != MISC_ZIGGURAT)
        {
            if (player_mutation_level(MUT_NO_ARTIFICE))
                mpr("You cannot evoke magical items.");
            else
            {
                simple_god_message("'s wrath prevents you from evoking "
                                   "devices!", GOD_PAKELLAS);
            }
            return false;
        }

        if (is_deck(item))
        {
            evoke_deck(item);
            practise_using_deck();
            count_action(CACT_EVOKE, EVOC_DECK);
            break;
        }

        switch (item.sub_type)
        {
#if TAG_MAJOR_VERSION == 34
        case MISC_BOTTLED_EFREET:
            canned_msg(MSG_NOTHING_HAPPENS);
            return false;
#endif

        case MISC_FAN_OF_GALES:
        {
            if (!evoker_is_charged(item))
            {
                mpr("That is presently inert.");
                return false;
            }

            const int surge = pakellas_surge_devices();
            surge_power(you.spec_evoke() + surge);
            wind_blast(&you,
                       player_adjust_evoc_power(you.skill(SK_EVOCATIONS, 15),
                                                surge),
                       coord_def());
            expend_xp_evoker(item);
            practise_evoking(3);
            break;
        }

        case MISC_LAMP_OF_FIRE:
            if (!evoker_is_charged(item))
            {
                mpr("That is presently inert.");
                return false;
            }
            if (_lamp_of_fire())
            {
                expend_xp_evoker(item);
                practise_evoking(3);
            }
            else
                return false;

            break;

#if TAG_MAJOR_VERSION == 34
        case MISC_STONE_OF_TREMORS:
            canned_msg(MSG_NOTHING_HAPPENS);
            return false;
#endif

        case MISC_PHIAL_OF_FLOODS:
            if (!evoker_is_charged(item))
            {
                mpr("That is presently inert.");
                return false;
            }
            if (_phial_of_floods())
            {
                expend_xp_evoker(item);
                practise_evoking(3);
            }
            else
                return false;
            break;

        case MISC_HORN_OF_GERYON:
            if (!evoker_is_charged(item))
            {
                mpr("That is presently inert.");
                return false;
            }
            if (_evoke_horn_of_geryon())
            {
                expend_xp_evoker(item);
                practise_evoking(3);
            }
            else
                return false;
            break;

        case MISC_BOX_OF_BEASTS:
            if (_box_of_beasts(item))
                practise_evoking(1);
            break;

        case MISC_SACK_OF_SPIDERS:
            if (_sack_of_spiders(item))
                practise_evoking(1);
            break;

        case MISC_CRYSTAL_BALL_OF_ENERGY:
            if (!_check_crystal_ball())
                unevokable = true;
            else if (_ball_of_energy())
                practise_evoking(1);
            break;

        case MISC_LIGHTNING_ROD:
            if (!evoker_is_charged(item))
            {
                mpr("That is presently inert.");
                return false;
            }
            if (_lightning_rod())
            {
                practise_evoking(1);
                if (you.props["thunderbolt_charge"].get_int() >=
                    LIGHTNING_MAX_CHARGE)
                {
                    mpr("The lightning rod overheats!");
                    expend_xp_evoker(item);
                }
            }
            else
                return false;
            break;

        case MISC_QUAD_DAMAGE:
            mpr("QUAD DAMAGE!");
            you.duration[DUR_QUAD_DAMAGE] = 30 * BASELINE_DELAY;
            ASSERT(in_inventory(item));
            dec_inv_item_quantity(item.link, 1);
            invalidate_agrid(true);
            break;

        case MISC_PHANTOM_MIRROR:
            switch (_phantom_mirror())
            {
                default:
                case SPRET_ABORT:
                    return false;

                case SPRET_SUCCESS:
                    ASSERT(in_inventory(item));
                    dec_inv_item_quantity(item.link, 1);
                    // deliberate fall-through
                case SPRET_FAIL:
                    practise_evoking(1);
                    break;
            }
            break;

        case MISC_ZIGGURAT:
            // Don't set did_work to false, _make_zig handles the message.
            unevokable = !_make_zig(item);
            break;

        default:
            did_work = false;
            unevokable = true;
            break;
        }
        if (did_work && !unevokable)
            count_action(CACT_EVOKE, item.sub_type, OBJ_MISCELLANY);
        break;

    default:
        unevokable = true;
        break;
    }

    if (!did_work)
        canned_msg(MSG_NOTHING_HAPPENS);

    if (!unevokable)
        you.turn_is_over = true;
    else
        crawl_state.zero_turns_taken();

    return did_work;
}
