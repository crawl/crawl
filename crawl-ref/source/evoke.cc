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
#include "cloud.h"
#include "coordit.h"
#include "decks.h"
#include "directn.h"
#include "dungeon.h"
#include "english.h"
#include "env.h"
#include "exercise.h"
#include "fight.h"
#include "food.h"
#include "ghost.h"
#include "godconduct.h"
#include "invent.h"
#include "itemprop.h"
#include "items.h"
#include "item_use.h"
#include "libutil.h"
#include "losglobal.h"
#include "mapmark.h"
#include "melee_attack.h"
#include "message.h"
#include "mgen_data.h"
#include "misc.h"
#include "mon-behv.h"
#include "mon-chimera.h"
#include "mon-clone.h"
#include "mon-pick.h"
#include "mon-place.h"
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

void shadow_lantern_effect()
{
    int n = div_rand_round(you.time_taken, 10);
    for (int i = 0; i < n; ++i)
    {
        if (you.magic_points > 0)
        {
            dec_mp(1);

            if (x_chance_in_y(you.skill_rdiv(SK_EVOCATIONS, 1, 5) + 1, 14))
            {
                create_monster(mgen_data(MONS_SHADOW, BEH_FRIENDLY, &you, 2,
                               MON_SUMM_LANTERN, you.pos(), MHITNOT));

                did_god_conduct(DID_NECROMANCY, 1);
            }
        }
        else
            expire_lantern_shadows();
    }
}

void expire_lantern_shadows()
{
    for (monster_iterator mi; mi; ++mi)
    {
        int stype = 0;
        if (mi->is_summoned(0, &stype) && stype == MON_SUMM_LANTERN)
            mi->del_ench(ENCH_ABJ);
    }
}

static bool _reaching_weapon_attack(const item_def& wpn)
{
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
    args.cancel_at_self = true;
    targetter_reach hitfunc(&you, REACH_TWO);
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

    if (you.confused())
        beam.confusion_fuzz(2);

    const coord_def delta = beam.target - you.pos();
    const int x_distance  = abs(delta.x);
    const int y_distance  = abs(delta.y);
    monster* mons = monster_at(beam.target);
    // don't allow targeting of submerged monsters (includes trapdoor spiders)
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
        return false; // Shouldn't happen with confused swings
    }

    // Calculate attack delay now in case we have to apply it.
    const int attack_delay = you.attack_delay(you.weapon());

    if (!feat_is_reachable_past(grd(first_middle))
        && !feat_is_reachable_past(grd(second_middle)))
    {
        // Might also be a granite statue/orcish idol which you
        // can reach _past_.
        if (you.confused())
        {
            mpr("You swing wildly and hit a wall.");
            you.time_taken = attack_delay;
            make_hungry(3, true);
            return true;
        }
        else
        {
            mpr("There's a wall in the way.");
            return false;
        }
    }

    // Failing to hit someone due to a friend blocking is infuriating,
    // shadow-boxing empty space is not (and would be abusable to wait
    // with no penalty).
    if (mons)
        you.apply_berserk_penalty = false;

    // Choose one of the two middle squares (which might be the same).
    const coord_def middle =
        (!feat_is_reachable_past(grd(first_middle)) ? second_middle :
         (!feat_is_reachable_past(grd(second_middle)) ? first_middle :
          (coinflip() ? first_middle : second_middle)));

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

        if (you.confused())
        {
            mprf("You swing wildly%s", beam.isMe() ?
                                       " and almost hit yourself!" : ".");
        }
        else
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

static bool _evoke_horn_of_geryon(item_def &item)
{
    bool created = false;

    if (silenced(you.pos()))
    {
        mpr("You can't produce a sound!");
        return false;
    }

    mprf(MSGCH_SOUND, "You produce a hideous howling noise!");
    did_god_conduct(DID_UNHOLY, 3);
    int num = 1;
    if (you.skill(SK_EVOCATIONS, 10) + random2(90) > 130)
        ++num;
    if (you.skill(SK_EVOCATIONS, 10) + random2(90) > 180)
        ++num;
    if (you.skill(SK_EVOCATIONS, 10) + random2(90) > 230)
        ++num;
    for (int n = 0; n < num; ++n)
    {
        monster* mon;
        beh_type beh = BEH_HOSTILE;
        bool will_anger = player_will_anger_monster(MONS_HELL_BEAST);

        if (!will_anger && random2(you.skill(SK_EVOCATIONS, 10)) > 7)
            beh = BEH_FRIENDLY;
        mgen_data mg(MONS_HELL_BEAST, beh, &you, 3, SPELL_NO_SPELL, you.pos(),
                     MHITYOU, MG_FORCE_BEH, GOD_NO_GOD, MONS_HELL_BEAST, 0, BLACK,
                     PROX_CLOSE_TO_PLAYER);
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
    if (you.intel() <= 1)
    {
        mpr("You lack the intelligence to focus on the shapes in the ball.");
        return false;
    }

    if (you.confused())
    {
        mpr("You are unable to concentrate on the shapes in the ball.");
        return false;
    }

    if (you.magic_points == you.max_magic_points)
    {
        mpr("With no energy to recover, the crystal ball of energy is "
            "presently useless to you.");
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
 * Evoke the Disc of Storms, potentially hurling Shock, Lightning Bolt, or
 * Orb of Electricity in all directions around the player. Odds of doing so,
 * the number of zaps created, & their power all increase with Evocations.
 *
 * @return  Whether anything happened.
 */
bool disc_of_storms()
{
    const int fail_rate = 30 - you.skill(SK_EVOCATIONS);

    if (x_chance_in_y(fail_rate, 100))
    {
        canned_msg(MSG_NOTHING_HAPPENS);
        return false;
    }
    if (x_chance_in_y(fail_rate, 100))
    {
        mpr("The disc glows for a moment, then fades.");
        return false;
    }
    if (x_chance_in_y(fail_rate, 100))
    {
        mpr("Little bolts of electricity crackle over the disc.");
        return false;
    }

    const int disc_count
        = roll_dice(2, 1 + you.skill_rdiv(SK_EVOCATIONS, 1, 7));
    ASSERT(disc_count);

    mpr("The disc erupts in an explosion of electricity!");
    const int range = you.skill_rdiv(SK_EVOCATIONS, 1, 3) + 5; // 5--14
    const int power = 30 + you.skill(SK_EVOCATIONS, 2); // 30-84
    for (int i = 0; i < disc_count; ++i)
        _spray_lightning(range, power);

    // Let it rain.
    for (radius_iterator ri(you.pos(), LOS_NO_TRANS); ri; ++ri)
    {
        if (!in_bounds(*ri) || cell_is_solid(*ri))
            continue;

        if (one_chance_in(60 - you.skill(SK_EVOCATIONS)))
        {
            place_cloud(CLOUD_RAIN, *ri,
                        random2(you.skill(SK_EVOCATIONS)), &you);
        }
    }

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

static int _wand_range(zap_type ztype)
{
    // FIXME: Eventually we should have sensible values here.
    return LOS_RADIUS;
}

static int _max_wand_range()
{
    return LOS_RADIUS;
}

static targetter *_wand_targetter(const item_def *wand)
{
    int range = _wand_range(wand->zap());
    const int power = 15 + you.skill(SK_EVOCATIONS, 5) / 2;

    switch (wand->sub_type)
    {
    case WAND_FIREBALL:
        return new targetter_beam(&you, range, ZAP_FIREBALL, power, 1, 1);
    case WAND_LIGHTNING:
        return new targetter_beam(&you, range, ZAP_LIGHTNING_BOLT, power, 0, 0);
    case WAND_FLAME:
        return new targetter_beam(&you, range, ZAP_THROW_FLAME, power, 0, 0);
    case WAND_FIRE:
        return new targetter_beam(&you, range, ZAP_BOLT_OF_FIRE, power, 0, 0);
    case WAND_FROST:
        return new targetter_beam(&you, range, ZAP_THROW_FROST, power, 0, 0);
    case WAND_COLD:
        return new targetter_beam(&you, range, ZAP_BOLT_OF_COLD, power, 0, 0);
    case WAND_DIGGING:
        return new targetter_beam(&you, range, ZAP_DIG, power, 0, 0);
    default:
        return 0;
    }
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

    bolt beam;
    dist zap_wand;
    int item_slot;

    // Unless the character knows the type of the wand, the targeting
    // system will default to enemies. -- [ds]
    targ_mode_type targ_mode = TARG_HOSTILE;

    beam.set_agent(&you);
    beam.source_name = "you";

    if (inv_count() < 1)
    {
        canned_msg(MSG_NOTHING_CARRIED);
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

    const int mp_cost = wand_mp_cost();
    if (!enough_mp(mp_cost, false))
        return;

    if (slot != -1)
        item_slot = slot;
    else
    {
        item_slot = prompt_invent_item("Zap which item?",
                                       MT_INVLIST,
                                       OBJ_WANDS,
                                       true, true, true, 0, -1, nullptr,
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

    // If you happen to be wielding the wand, its display might change.
    if (you.equip[EQ_WEAPON] == item_slot)
        you.wield_change = true;

    const zap_type type_zapped = wand.zap();

    const bool has_charges = wand.charges > 0;
    if (!has_charges && wand.used_count == ZAPCOUNT_EMPTY)
    {
        mpr("This wand has no charges.");
        return;
    }

    // already know the type
    const bool alreadyknown = item_type_known(wand);
    // will waste charges
    const bool wasteful     = !item_ident(wand, ISFLAG_KNOW_PLUSES);
          bool invis_enemy  = false;
    const bool dangerous    = player_in_a_dangerous_place(&invis_enemy);
    targetter *hitfunc      = 0;

    if (!alreadyknown)
    {
        beam.effect_known = false;
        beam.effect_wanton = false;
    }
    else
    {
        switch (wand.sub_type)
        {
        case WAND_DIGGING:
        case WAND_TELEPORTATION:
            targ_mode = TARG_ANY;
            break;

        case WAND_HEAL_WOUNDS:
            if (you_worship(GOD_ELYVILON))
            {
                targ_mode = TARG_ANY;
                break;
            }
            // else intentional fall-through
        case WAND_HASTING:
        case WAND_INVISIBILITY:
            targ_mode = TARG_FRIEND;
            break;

        default:
            targ_mode = TARG_HOSTILE;
            break;
        }

        hitfunc = _wand_targetter(&wand);
    }
    const bool randeff = wand.sub_type == WAND_RANDOM_EFFECTS;

    const int power = (15 + you.skill(SK_EVOCATIONS, 5) / 2)
        * (player_mutation_level(MUT_MP_WANDS) + 6) / 6;
    const int tracer_range = (alreadyknown && !randeff)
                           ? _wand_range(type_zapped) : _max_wand_range();
    const string zap_title =
        "Zapping: " + get_menu_colour_prefix_tags(wand, DESC_INVENTORY)
                    + (wasteful ? " <lightred>(will waste charges)</lightred>"
                                : "");
    direction_chooser_args args;
    args.mode = targ_mode;
    args.range = tracer_range;
    args.top_prompt = zap_title;
    args.hitfunc = hitfunc;
    if (!randeff && testbits(get_spell_flags(zap_to_spell(type_zapped)),
                             SPFLAG_MR_CHECK))
    {
        args.get_desc_func = bind(desc_success_chance, placeholders::_1,
                                  zap_ench_power(type_zapped, power));
    }
    direction(zap_wand, args);

    if (hitfunc)
        delete hitfunc;

    if (!zap_wand.isValid)
    {
        if (zap_wand.isCancel)
            canned_msg(MSG_OK);
        return;
    }

    if (alreadyknown && zap_wand.target == you.pos())
    {
        if (wand.sub_type == WAND_TELEPORTATION
            && you.no_tele_print_reason(false, false))
        {
            return;
        }
        else if (wand.sub_type == WAND_HASTING
                 && stasis_blocks_effect(false, "%s prevents hasting.",
                                         0, nullptr, "You cannot haste."))
        {
            return;
        }
        else if (wand.sub_type == WAND_INVISIBILITY && dont_use_invis())
            return;
    }

    if (!has_charges)
    {
        canned_msg(MSG_NOTHING_HAPPENS);
        // It's an empty wand; inscribe it that way.
        wand.used_count = ZAPCOUNT_EMPTY;
        you.turn_is_over = true;
        return;
    }

    if (you.confused())
        zap_wand.confusion_fuzz();

    if (randeff)
    {
        beam.effect_known = false;
        beam.effect_wanton = alreadyknown;
    }

    beam.source   = you.pos();
    beam.attitude = ATT_FRIENDLY;
    beam.set_target(zap_wand);

    const bool aimed_at_self = (beam.target == you.pos());

    // Check whether we may hit friends, use "safe" values for random effects
    // and unknown wands (highest possible range, and unresistable beam
    // flavour). Don't use the tracer if firing at self.
    if (!aimed_at_self)
    {
        beam.range = tracer_range;
        if (!player_tracer(beam.effect_known ? type_zapped
                                             : ZAP_DEBUGGING_RAY,
                           power, beam, beam.effect_known ? 0 : 17))
        {
            return;
        }
    }

    // Zapping the wand isn't risky if you aim it away from all monsters
    // and yourself, unless there's a nearby invisible enemy and you're
    // trying to hit it at random.
    const bool risky = dangerous && (beam.friend_info.count
                                     || beam.foe_info.count
                                     || invis_enemy
                                     || aimed_at_self);

    if (risky && alreadyknown && wand.sub_type == WAND_RANDOM_EFFECTS)
    {
        // Xom loves it when you use a Wand of Random Effects and
        // there is a dangerous monster nearby...
        xom_is_stimulated(200);
    }

    // Reset range.
    beam.range = _wand_range(type_zapped);

#ifdef WIZARD
    if (you.wizard)
    {
        string str = wand.inscription;
        int wiz_range = strip_number_tag(str, "range:");
        if (wiz_range != TAG_UNFOUND)
            beam.range = wiz_range;
    }
#endif

    dec_mp(mp_cost, false);

    // zapping() updates beam.
    zapping(type_zapped, power, beam);

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

    // Identify if unknown.
    if (!alreadyknown)
    {
        set_ident_type(wand, ID_KNOWN_TYPE);
        mprf_nocap("%s", wand.name(DESC_INVENTORY_EQUIP).c_str());
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

    practise(EX_DID_ZAP_WAND);
    count_action(CACT_EVOKE, EVOC_WAND);
    alert_nearby_monsters();

    if (!alreadyknown && risky)
    {
        // Xom loves it when you use an unknown wand and there is a
        // dangerous monster nearby...
        xom_is_stimulated(200);
    }

    // Need to do this down here since auto_assign_item_slot may
    // move the item in memory.
    if (!alreadyknown)
        auto_assign_item_slot(wand);

    you.turn_is_over = true;
}

int recharge_wand(bool known, const string &pre_msg)
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

            if (known && !pre_msg.empty())
                mpr(pre_msg);

            mprf("%s glows for a moment.", wand.name(DESC_YOUR).c_str());
        }

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

static const pop_entry pop_beasts[] =
{ // Box of Beasts
  {  1,  3,  10,  DOWN, MONS_BUTTERFLY },
  {  1,  5,  100, DOWN, MONS_RAT   },
  {  1,  5,  100, DOWN, MONS_BAT   },
  {  2,  8,  100, PEAK, MONS_JACKAL },
  {  2, 10,  100, PEAK, MONS_ADDER },
  {  4, 13,  100, PEAK, MONS_HOUND },
  {  5, 15,  100, PEAK, MONS_WATER_MOCCASIN },
  {  5, 15,  100, PEAK, MONS_SKY_BEAST },
  {  8, 18,  100, PEAK, MONS_CROCODILE },
  {  8, 18,  100, PEAK, MONS_HOG },
  { 10, 20,  100, PEAK, MONS_ICE_BEAST },
  { 10, 20,  100, PEAK, MONS_YAK },
  { 10, 20,  100, PEAK, MONS_WYVERN },
  { 10, 20,  100, PEAK, MONS_WOLF },
  { 11, 22,  100, PEAK, MONS_ALLIGATOR },
  { 11, 22,  200, PEAK, MONS_POLAR_BEAR },
  { 11, 22,  100, PEAK, MONS_WARG },
  { 13, 25,  100, PEAK, MONS_ELEPHANT },
  { 13, 25,  100, PEAK, MONS_GRIFFON },
  { 13, 25,  100, PEAK, MONS_BLACK_BEAR },
  { 15, 27,   50, PEAK, MONS_CATOBLEPAS },
  { 15, 27,  100, PEAK, MONS_DEATH_YAK },
  { 16, 27,  100, PEAK, MONS_ANACONDA },
  { 16, 27,   50, PEAK, MONS_RAVEN },
  { 18, 29,   50, UP,   MONS_DIRE_ELEPHANT },
  { 20, 29,   50, UP,   MONS_FIRE_DRAGON },
  { 23, 32,   10, UP,   MONS_APIS },
  { 23, 32,   10, UP,   MONS_HELLEPHANT },
  { 23, 32,   10, UP,   MONS_GOLDEN_DRAGON },
  { 0,0,0,FLAT,MONS_0 }
};

static const pop_entry pop_spiders[] =
{ // Sack of Spiders
  {  0,  10,   10, DOWN, MONS_GIANT_MITE },
  {  0,  15,   50, DOWN, MONS_SPIDER },
  {  5,  20,  100, PEAK, MONS_TRAPDOOR_SPIDER },
  {  8,  27,  100, PEAK, MONS_REDBACK },
  { 12,  27,  100, PEAK, MONS_JUMPING_SPIDER },
  { 15,  27,  100, PEAK, MONS_ORB_SPIDER },
  { 18,  27,  100, PEAK, MONS_TARANTELLA },
  { 20,  27,  100, PEAK, MONS_WOLF_SPIDER },
  { 25,  27,    5,   UP, MONS_GHOST_MOTH },
  { 0,0,0,FLAT,MONS_0 }
};

static bool _box_of_beasts_veto_mon(monster_type mon)
{
    // Don't summon any beast that would anger your god.
    return player_will_anger_monster(mon);
}

static bool _box_of_beasts(item_def &box)
{
    mpr("You open the lid...");

    if (!box.plus)
    {
        mpr("...but the box appears empty, and falls apart.");
        ASSERT(in_inventory(box));
        dec_inv_item_quantity(box.link, 1);
        return false;
    }

    bool success = false;
    monster* mons = nullptr;

    if (!one_chance_in(3))
    {
        // Invoke mon-pick with the custom list
        const int pick_level = you.skill(SK_EVOCATIONS) + 5;
        monster_type mon = pick_monster_from(pop_beasts, pick_level,
                                             _box_of_beasts_veto_mon);

        // Second monster might be only half as good
        int pick_level_2 = random_range(max(1,div_rand_round(pick_level,2)),
                                        pick_level);
        monster_type mon2 = pick_monster_from(pop_beasts, pick_level_2,
                                              _box_of_beasts_veto_mon);

        // Third monster picked from anywhere up to max level
        int pick_level_3 = random_range(1, pick_level);
        monster_type mon3 = pick_monster_from(pop_beasts, pick_level_3,
                                              _box_of_beasts_veto_mon);

        if (mon && mon2 && mon3)
        {
            mgen_data mg = mgen_data(MONS_CHIMERA,
                                     BEH_FRIENDLY, &you,
                                     3 + random2(3), 0,
                                     you.pos(),
                                     MHITYOU, MG_AUTOFOE);
            mg.define_chimera(mon, mon2, mon3);
            mons = create_monster(mg);
            if (mons)
                success = true;
        }
        else
        {
            // we weren't able to come up with acceptable monsters
            mpr("...but nothing happens.");
            return false;
        }

    }

    if (success)
    {
        mpr("...and something leaps out!");
        xom_is_stimulated(10);
        did_god_conduct(DID_CHAOS, random_range(5,10));
        // Decrease charges
        box.charges--;
        box.used_count++;
        // Let each part announce itself
        for (int n = 0; n < NUM_CHIMERA_HEADS; ++n)
        {
            mons->ghost->acting_part = get_chimera_part(mons, n + 1);
            handle_monster_shouts(mons, true);
        }
        mons->ghost->acting_part = MONS_0;
    }
    else
        // Failed to create monster for some reason
        mpr("...but nothing happens.");

    return success;
}

static bool _sack_of_spiders(item_def &sack)
{
    mpr("You reach into the bag...");

    if (!sack.plus)
    {
        mpr("...but the bag is empty, and unravels at your touch.");
        ASSERT(in_inventory(sack));
        dec_inv_item_quantity(sack.link, 1);
        return false;
    }

    if (one_chance_in(5))
    {
        mpr("...but nothing happens.");
        return false;
    }

    bool success = false;
    int count = 1 + random2(3)
        + random2(div_rand_round(you.skill(SK_EVOCATIONS, 10), 40));
    for (int n = 0; n < count; n++)
    {
        // Invoke mon-pick with our custom list
        monster_type mon = pick_monster_from(pop_spiders,
                                             max(1, you.skill(SK_EVOCATIONS)),
                                             _box_of_beasts_veto_mon);
        mgen_data mg = mgen_data(mon,
                                 BEH_FRIENDLY, &you,
                                 3 + random2(4), 0,
                                 you.pos(),
                                 MHITYOU, MG_AUTOFOE);
        if (create_monster(mg))
            success = true;
    }

    if (success)
    {
        mpr("...and things crawl out!");
        // Also generate webs on hostile monsters and trap them.
        int rad = LOS_RADIUS / 2 + 2;
        for (monster_near_iterator mi(you.pos(), LOS_SOLID); mi; ++mi)
        {
            trap_def *trap = find_trap((*mi)->pos());
            // Don't destroy non-web traps or try to trap monsters
            // currently caught by something.
            if (you.pos().range((*mi)->pos()) > rad
                || (!trap && grd((*mi)->pos()) != DNGN_FLOOR)
                || (trap && trap->type != TRAP_WEB)
                || (*mi)->friendly()
                || (*mi)->caught())
            {
                continue;
            }

            int chance = 100 - (100 * (you.pos().range((*mi)->pos()) - 1) / rad)
                - 2 * (27 - you.skill(SK_EVOCATIONS));
            if (x_chance_in_y(chance, 100))
            {
                if (trap && trap->type == TRAP_WEB)
                    destroy_trap((*mi)->pos());

                if (place_specific_trap((*mi)->pos(), TRAP_WEB))
                {
                    // Reveal the trap
                    grd((*mi)->pos()) = DNGN_TRAP_WEB;
                    trap = find_trap((*mi)->pos());
                    trap->trigger(**mi);
                }
            }
        }
        // Decrease charges
        sack.charges--;
        sack.used_count++;
    }
    else
        // Failed to create monster for some reason
        mpr("...but nothing happens.");

    return success;
}

static bool _ball_of_energy()
{
    bool ret = false;

    mpr("You gaze into the crystal ball.");

    int use = random2(you.skill(SK_EVOCATIONS, 6));

    if (use < 2)
        lose_stat(STAT_INT, 1 + random2avg(7, 2), false, "using a ball of energy");
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

        if (random2avg(77 - you.skill(SK_EVOCATIONS, 2), 4) > proportional
            || one_chance_in(25))
        {
            mpr("You feel your power drain away!");
            dec_mp(you.magic_points);
        }
        else
        {
            mpr("You are suffused with power!");
            inc_mp(5 + random2avg(you.skill(SK_EVOCATIONS), 2));

            ret = true;
        }
    }

    return ret;
}

static int _num_evoker_elementals()
{
    int n = 1;
    if (you.skill(SK_EVOCATIONS, 10) + random2(70) > 110)
        ++n;
    if (you.skill(SK_EVOCATIONS, 10) + random2(70) > 170)
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
                               vector<bolt> &beams, vector<coord_def> &elementals,
                               int num)
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
            if (path.size() > 3)
                elementals.push_back(path.back());
        }
    }

    return !paths.empty();
}

static bool _lamp_of_fire()
{
    bolt base_beam;
    dist target;

    const int pow = 8 + you.skill_rdiv(SK_EVOCATIONS, 9, 4);
    if (spell_direction(target, base_beam, DIR_TARGET, TARG_HOSTILE, 8,
                        true, true, false, nullptr,
                        "Aim the lamp in which direction?", true, nullptr))
    {
        if (you.confused())
            target.confusion_fuzz();

        did_god_conduct(DID_FIRE, 6 + random2(3));

        mpr("The flames dance!");

        vector<bolt> beams;
        vector<coord_def> elementals;
        int num_trails = _num_evoker_elementals();

        _fill_flame_trails(you.pos(), target.target, beams, elementals, num_trails);

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
            beam.ench_power = 1 + (pow/10);
            beam.loudness   = 5;
            beam.fire();
        }

        beh_type attitude = BEH_FRIENDLY;
        if (player_will_anger_monster(MONS_FIRE_ELEMENTAL))
            attitude = BEH_HOSTILE;
        for (coord_def epos : elementals)
        {
            mgen_data mg(MONS_FIRE_ELEMENTAL, attitude, &you, 3,
                         SPELL_NO_SPELL, epos, 0,
                         MG_FORCE_BEH | MG_FORCE_PLACE, GOD_NO_GOD,
                         MONS_FIRE_ELEMENTAL, 0, BLACK, PROX_CLOSE_TO_PLAYER);
            mg.hd = 6 + (pow/20);
            create_monster(mg);
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

static int _gale_push_dist(const actor* agent, const actor* victim)
{
    int dist = 1 + you.skill_rdiv(SK_EVOCATIONS, 1, 10);

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

    int radius = min(7, 5 + div_rand_round(pow, 60));

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

        int push = _gale_push_dist(agent, act);
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
                if (you.can_see(act))
                    affected_monsters.add(act->as_monster());
            }
            else
                player_affected = true;
        }
    }

    // Now move clouds
    vector<int> cloud_list;
    for (distance_iterator di(agent->pos(), true, true, radius + 2); di; ++di)
    {
        if (env.cgrid(*di) != EMPTY_CLOUD
            && cell_see_cell(agent->pos(), *di, LOS_SOLID)
            && (target.origin()
                || _angle_between(agent->pos(), target, *di) <= PI/4.0))
        {
            cloud_list.push_back(env.cgrid(*di));
        }
    }

    for (int i = cloud_list.size() - 1; i >= 0; --i)
    {
        wind_beam.target = env.cloud[cloud_list[i]].pos;
        wind_beam.fire();

        int dist = env.cloud[cloud_list[i]].pos.distance_from(agent->pos());
        int push = (dist > 5 ? 2 : dist > 2 ? 3 : 4);

        for (unsigned int j = 0;
             j < wind_beam.path_taken.size() - 1 && push;
             ++j)
        {
            if (env.cgrid(wind_beam.path_taken[j]) == cloud_list[i])
            {
                coord_def newpos = wind_beam.path_taken[j+1];
                if (!cell_is_solid(newpos)
                    && env.cgrid(newpos) == EMPTY_CLOUD)
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
                            && env.cgrid(*di) == EMPTY_CLOUD)
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

static void _fan_of_gales_elementals()
{
    int radius = min(7, 5 + you.skill_rdiv(SK_EVOCATIONS, 1, 6));

    vector<coord_def> elementals;
    for (radius_iterator ri(you.pos(), radius, C_ROUND, true); ri; ++ri)
    {
        if (ri->distance_from(you.pos()) >= 3 && !monster_at(*ri)
            && !cell_is_solid(*ri)
            && cell_see_cell(you.pos(), *ri, LOS_NO_TRANS))
        {
            elementals.push_back(*ri);
        }
    }
    shuffle_array(elementals);

    int num_elementals = _num_evoker_elementals();

    bool created = false;
    beh_type attitude = BEH_FRIENDLY;
    if (player_will_anger_monster(MONS_AIR_ELEMENTAL))
        attitude = BEH_HOSTILE;
    for (int n = 0; n < min(num_elementals, (int)elementals.size()); ++n)
    {
        mgen_data mg (MONS_AIR_ELEMENTAL, attitude, &you, 3, SPELL_NO_SPELL,
                      elementals[n], 0, MG_FORCE_BEH | MG_FORCE_PLACE,
                      GOD_NO_GOD, MONS_AIR_ELEMENTAL, 0, BLACK,
                      PROX_CLOSE_TO_PLAYER);
        mg.hd = 6 + you.skill_rdiv(SK_EVOCATIONS, 2, 13);
        if (create_monster(mg))
            created = true;
    }
    if (created)
        mpr("The winds coalesce and take form.");
}

static bool _is_rock(dungeon_feature_type feat)
{
    return feat == DNGN_ROCK_WALL || feat == DNGN_CLEAR_ROCK_WALL
           || feat == DNGN_SLIMY_WALL;
}

static bool _is_rubble_source(dungeon_feature_type feat)
{
    switch (feat)
    {
        case DNGN_ROCK_WALL:
        case DNGN_CLEAR_ROCK_WALL:
        case DNGN_SLIMY_WALL:
        case DNGN_STONE_WALL:
        case DNGN_CLEAR_STONE_WALL:
        case DNGN_PERMAROCK_WALL:
            return true;

        default:
            return false;
    }
}

static bool _adjacent_to_rubble_source(coord_def pos)
{
    for (adjacent_iterator ai(pos); ai; ++ai)
    {
        if (_is_rubble_source(grd(*ai)) && you.see_cell_no_trans(*ai))
            return true;
    }

    return false;
}

static bool _stone_of_tremors()
{
    vector<coord_def> wall_pos;
    vector<coord_def> rubble_pos;
    vector<coord_def> door_pos;

    for (distance_iterator di(you.pos(), false, true, LOS_RADIUS); di; ++di)
    {
        if (_is_rubble_source(grd(*di)))
            wall_pos.push_back(*di);
        else if (feat_is_door(grd(*di)))
            door_pos.push_back(*di);
        else if (_adjacent_to_rubble_source(*di))
            rubble_pos.push_back(*di);
    }

    mpr("The dungeon trembles and rubble falls from the walls!");
    noisy(15, you.pos());

    bolt rubble;
    rubble.name       = "falling rubble";
    rubble.range      = 1;
    rubble.hit        = 10 + you.skill_rdiv(SK_EVOCATIONS, 1, 2);
    rubble.damage     = dice_def(3, 5 + you.skill(SK_EVOCATIONS));
    rubble.source_id  = MID_PLAYER;
    rubble.glyph      = dchar_glyph(DCHAR_FIRED_MISSILE);
    rubble.colour     = LIGHTGREY;
    rubble.flavour    = BEAM_MMISSILE;
    rubble.thrower    = KILL_YOU;
    rubble.pierce     = false;
    rubble.loudness   = 10;
    rubble.draw_delay = 0;

    // Hit the affected area with falling rubble.
    for (coord_def pos : rubble_pos)
    {
        rubble.source = rubble.target = pos;
        rubble.fire();
    }
    update_screen();
    delay(200);

    // Possibly shaft some monsters.
    for (monster_near_iterator mi(you.pos(), LOS_NO_TRANS); mi; ++mi)
    {
        if (grd(mi->pos()) == DNGN_FLOOR
            && is_valid_shaft_level()
            && x_chance_in_y(75 + you.skill(SK_EVOCATIONS, 2), 800))
        {
            mi->do_shaft();
        }
    }

    // Destroy doors.
    for (coord_def pos : door_pos)
    {
        destroy_wall(pos);
        mpr("The door collapses!");
    }

    // Collapse some walls and mark collapsed walls as valid elemental positions.
    int num_elementals = _num_evoker_elementals();
    for (coord_def pos : wall_pos)
    {
        if (_is_rock(grd(pos)) && one_chance_in(3))
        {
            destroy_wall(pos);
            rubble_pos.push_back(pos);
        }
    }
    shuffle_array(rubble_pos);

    // Create elementals.
    bool created = false;
    beh_type attitude = BEH_FRIENDLY;
    if (player_will_anger_monster(MONS_EARTH_ELEMENTAL))
        attitude = BEH_HOSTILE;

    for (int n = 0; n < min(num_elementals, (int)rubble_pos.size()); ++n)
    {
        // Skip occupied positions
        if (actor_at(rubble_pos[n]))
            continue;

        mgen_data mg(MONS_EARTH_ELEMENTAL, attitude, &you, 3, SPELL_NO_SPELL,
                     rubble_pos[n], 0, MG_FORCE_BEH | MG_FORCE_PLACE, GOD_NO_GOD,
                     MONS_EARTH_ELEMENTAL, 0, BLACK, PROX_CLOSE_TO_PLAYER);
        mg.hd = 6 + you.skill_rdiv(SK_EVOCATIONS, 2, 13);
        if (create_monster(mg))
            created = true;
    }
    if (created)
        mpr("The rubble rises up and takes form.");

    return true;
}

// Used for phials and water nymphs.
bool can_flood_feature(dungeon_feature_type feat)
{
    return feat == DNGN_FLOOR || feat == DNGN_SHALLOW_WATER;
}

static bool _phial_of_floods()
{
    dist target;
    bolt beam;

    zappy(ZAP_PRIMAL_WAVE, 25 + you.skill(SK_EVOCATIONS, 6), beam);
    beam.range = LOS_RADIUS;
    beam.thrower = KILL_YOU;
    beam.name = "flood of elemental water";
    beam.aimed_at_spot = true;

    if (spell_direction(target, beam, DIR_NONE, TARG_HOSTILE,
                        LOS_RADIUS, true, true, false, nullptr,
                        "Aim the phial where?"))
    {
        if (you.confused())
        {
            target.confusion_fuzz();
            beam.set_target(target);
        }
        beam.fire();

        vector<coord_def> elementals;
        // Flood the endpoint
        coord_def center = beam.path_taken.back();
        int num = 5 + you.skill_rdiv(SK_EVOCATIONS, 3, 5) + random2(7);
        int dur = 40 + you.skill_rdiv(SK_EVOCATIONS, 8, 3);
        for (distance_iterator di(center, true, false, 2); di && num > 0; ++di)
        {
            if (can_flood_feature(grd(*di))
                && cell_see_cell(center, *di, LOS_NO_TRANS))
            {
                num--;
                temp_change_terrain(*di, DNGN_SHALLOW_WATER,
                                    random_range(dur*2, dur*3) - (di.radius()*20),
                                    TERRAIN_CHANGE_FLOOD);
                elementals.push_back(*di);
            }
        }

        int num_elementals = _num_evoker_elementals();

        bool created = false;
        num = min(num_elementals,
                  min((int)elementals.size(), (int)elementals.size() / 5 + 1));
        beh_type attitude = BEH_FRIENDLY;
        if (player_will_anger_monster(MONS_WATER_ELEMENTAL))
            attitude = BEH_HOSTILE;
        for (int n = 0; n < num; ++n)
        {
            mgen_data mg (MONS_WATER_ELEMENTAL, attitude, &you, 3,
                          SPELL_NO_SPELL, elementals[n], 0,
                          MG_FORCE_BEH | MG_FORCE_PLACE, GOD_NO_GOD,
                          MONS_WATER_ELEMENTAL, 0, BLACK, PROX_CLOSE_TO_PLAYER);
            mg.hd = 6 + you.skill_rdiv(SK_EVOCATIONS, 2, 15);
            if (create_monster(mg))
                created = true;
        }
        if (created)
            mpr("The water rises up and takes form.");

        return true;
    }

    return false;
}

static void _expend_xp_evoker(item_def &item)
{
    evoker_debt(item.sub_type) = XP_EVOKE_DEBT;
}

static spret_type _phantom_mirror()
{
    bolt beam;
    monster* victim = nullptr;
    dist spd;
    targetter_smite tgt(&you, LOS_RADIUS, 0, 0);

    if (!spell_direction(spd, beam, DIR_TARGET, TARG_ANY,
                         LOS_RADIUS, false, true, false, nullptr,
                         "Aiming: <white>Phantom Mirror</white>", true,
                         &tgt))
    {
        return SPRET_ABORT;
    }
    victim = monster_at(beam.target);
    if (!victim || !you.can_see(victim))
    {
        if (beam.target == you.pos())
            mpr("You can't use the mirror on yourself.");
        else
            mpr("You can't see anything there to clone.");
        return SPRET_ABORT;
    }

    if (!actor_is_illusion_cloneable(victim))
    {
        mpr("The mirror can't reflect that.");
        return SPRET_ABORT;
    }

    if (player_will_anger_monster(victim))
    {
        if (player_mutation_level(MUT_NO_LOVE))
            mpr("The reflection would only feel hate for you!");
        else
            simple_god_message(" forbids your reflecting this monster.");
        return SPRET_ABORT;
    }

    monster* mon = clone_mons(victim, true);
    if (!mon)
    {
        canned_msg(MSG_NOTHING_HAPPENS);
        return SPRET_FAIL;
    }

    const int power = 5 + you.skill(SK_EVOCATIONS, 3);
    int dur = min(6, max(1, (you.skill(SK_EVOCATIONS, 1) / 4 + 1)
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

static bool _rod_spell(item_def& irod, bool check_range)
{
    ASSERT(irod.base_type == OBJ_RODS);

    const spell_type spell = spell_in_rod(static_cast<rod_type>(irod.sub_type));
    int mana = spell_mana(spell) * ROD_CHARGE_MULT;
    int power = calc_spell_power(spell, false, false, true, true);

    int food = spell_hunger(spell, true);

    if (you.undead_state() == US_UNDEAD)
        food = 0;

    if (food && (you.hunger_state == HS_STARVING || you.hunger <= food)
        && !you.undead_state())
    {
        canned_msg(MSG_NO_ENERGY);
        crawl_state.zero_turns_taken();
        return false;
    }

    if (spell == SPELL_THUNDERBOLT && you.props.exists("thunderbolt_last")
        && you.props["thunderbolt_last"].get_int() + 1 == you.num_turns)
    {
        // Starting it up takes 2 mana, continuing any amount up to 5.
        // You don't get to expend less (other than stopping the zap completely).
        mana = min(5 * ROD_CHARGE_MULT, (int)irod.plus);
        // Never allow using less than a whole point of charge.
        mana = max(mana, ROD_CHARGE_MULT);
        you.props["thunderbolt_mana"].get_int() = mana;
    }

    if (irod.plus < mana)
    {
        mpr("The rod doesn't have enough magic points.");
        crawl_state.zero_turns_taken();
        // Don't lose a turn for trying to evoke without enough MP - that's
        // needlessly cruel for an honest error.
        return false;
    }

    if (check_range && spell_no_hostile_in_range(spell, true))
    {
        // Abort if there are no hostiles within range, but flash the range
        // markers for a short while.
        mpr("You can't see any susceptible monsters within range! "
            "(Use <w>V</w> to cast anyway.)");

        if (Options.use_animations & UA_RANGE)
        {
            targetter_smite range(&you, calc_spell_range(spell, 0, true), 0, 0, true);
            range_view_annotator show_range(&range);
            delay(50);
        }
        crawl_state.zero_turns_taken();
        return false;
    }

    // All checks passed, we can cast the spell.
    if (you.confused())
        random_uselessness();
    else if (your_spells(spell, power, false, true) == SPRET_ABORT)
    {
        crawl_state.zero_turns_taken();
        return false;
    }

    make_hungry(food, true, true);
    irod.plus -= mana;
    you.wield_change = true;
    if (item_is_quivered(irod))
        you.redraw_quiver = true;
    you.turn_is_over = true;

    return true;
}

bool evoke_item(int slot, bool check_range)
{
    if (you.form == TRAN_WISP)
    {
        mpr("You cannot evoke items in this form.");
        return false;
    }

    if (you.berserk() && (slot == -1
                       || slot != you.equip[EQ_WEAPON]
                       || weapon_reach(*you.weapon()) <= 2))
    {
        canned_msg(MSG_TOO_BERSERK);
        return false;
    }
    else if (player_mutation_level(MUT_NO_ARTIFICE)
             && (slot == -1 || slot != you.equip[EQ_WEAPON]
                            || weapon_reach(*you.weapon()) <= 2))
    {
        return mpr("You cannot evoke magical items."), false;
    }

    if (slot == -1)
    {
        slot = prompt_invent_item("Evoke which item? (* to show all)",
                                   MT_INVLIST,
                                   OSEL_EVOKABLE, true, true, true, 0, -1,
                                   nullptr, OPER_EVOKE);

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

    int pract = 0; // By how much Evocations is practised.
    bool did_work   = false;  // Used for default "nothing happens" message.
    bool unevokable = false;

    const unrandart_entry *entry = is_unrandom_artefact(item)
        ? get_unrand_entry(item.special) : nullptr;

    if (entry && entry->evoke_func)
    {
        ASSERT(item_is_equipped(item));

        bool qret = entry->evoke_func(&item, &pract, &did_work, &unevokable);

        if (!unevokable)
            count_action(CACT_EVOKE, item.special);

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

        if (weapon_reach(item) > 2)
        {
            if (_reaching_weapon_attack(item))
            {
                pract    = 0;
                did_work = true;
            }
            else
                return false;
        }
        else
            unevokable = true;
        break;

    case OBJ_RODS:
        ASSERT(wielded);

        if (you.confused())
        {
            canned_msg(MSG_TOO_CONFUSED);
            return false;
        }

        if (!(pract = _rod_spell(you.inv[slot], check_range)))
            return false;

        did_work = true;  // _rod_spell() handled messages
        count_action(CACT_EVOKE, EVOC_ROD);
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
            && you.hunger_state == HS_STARVING)
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
            mpr("Your reserves of magic are already full.");
            return false;
        }
        else if (x_chance_in_y(you.skill(SK_EVOCATIONS, 100) + 1100, 4000))
        {
            mpr("You channel some magical energy.");
            inc_mp(1 + random2(3));
            make_hungry(50, false, true);
            pract = 1;
            did_work = true;
            count_action(CACT_EVOKE, OBJ_STAVES << 16 | STAFF_ENERGY);
        }
        break;

    case OBJ_MISCELLANY:
        did_work = true; // easier to do it this way for misc items

        if (player_mutation_level(MUT_NO_ARTIFICE))
            return mpr("You cannot evoke magical items."), false;

        if (is_deck(item))
        {
            evoke_deck(item);
            pract = 1;
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
            if (!evoker_is_charged(item))
            {
                mpr("That is presently inert.");
                return false;
            }
            wind_blast(&you, you.skill(SK_EVOCATIONS, 10), coord_def());
            _fan_of_gales_elementals();
            _expend_xp_evoker(item);
            pract = 1;
            break;

        case MISC_LAMP_OF_FIRE:
            if (!evoker_is_charged(item))
            {
                mpr("That is presently inert.");
                return false;
            }
            if (_lamp_of_fire())
            {
                _expend_xp_evoker(item);
                pract = 1;
            }
            else
                return false;

            break;

        case MISC_STONE_OF_TREMORS:
            if (!evoker_is_charged(item))
            {
                mpr("That is presently inert.");
                return false;
            }
            if (_stone_of_tremors())
            {
                _expend_xp_evoker(item);
                pract = 1;
            }
            else
                return false;
            break;

        case MISC_PHIAL_OF_FLOODS:
            if (!evoker_is_charged(item))
            {
                mpr("That is presently inert.");
                return false;
            }
            if (_phial_of_floods())
            {
                _expend_xp_evoker(item);
                pract = 1;
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
            if (_evoke_horn_of_geryon(item))
            {
                _expend_xp_evoker(item);
                pract = 1;
            }
            else
                return false;
            break;

        case MISC_BOX_OF_BEASTS:
            if (_box_of_beasts(item))
                pract = 1;
            break;

        case MISC_SACK_OF_SPIDERS:
            if (_sack_of_spiders(item))
                pract = 1;
            break;

        case MISC_CRYSTAL_BALL_OF_ENERGY:
            if (!_check_crystal_ball())
                unevokable = true;
            else if (_ball_of_energy())
                pract = 1;
            break;

        case MISC_DISC_OF_STORMS:
            if (disc_of_storms())
                pract = 1;
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
                    pract = 1;
                    break;
            }
            break;

        default:
            did_work = false;
            unevokable = true;
            break;
        }
        if (did_work && !unevokable)
            count_action(CACT_EVOKE, OBJ_MISCELLANY << 16 | item.sub_type);
        break;

    default:
        unevokable = true;
        break;
    }

    if (!did_work)
        canned_msg(MSG_NOTHING_HAPPENS);
    else if (pract > 0)
        practise(EX_DID_EVOKE_ITEM, pract);

    if (!unevokable)
        you.turn_is_over = true;
    else
        crawl_state.zero_turns_taken();

    return did_work;
}
