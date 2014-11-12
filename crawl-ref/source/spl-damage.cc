/**
 * @file
 * @brief Damage-dealing spells not already handled elsewhere.
 *           Other targeted spells are covered in spl-zap.cc.
**/

#include "AppHdr.h"

#include "spl-damage.h"

#include "act-iter.h"
#include "areas.h"
#include "butcher.h"
#include "cloud.h"
#include "colour.h"
#include "coordit.h"
#include "directn.h"
#include "english.h"
#include "env.h"
#include "fight.h"
#include "food.h"
#include "fprop.h"
#include "godabil.h"
#include "godconduct.h"
#include "itemname.h"
#include "items.h"
#include "losglobal.h"
#include "message.h"
#include "misc.h"
#include "mon-behv.h"
#include "mon-death.h"
#include "ouch.h"
#include "prompt.h"
#include "shout.h"
#include "spl-summoning.h"
#include "spl-util.h"
#include "stepdown.h"
#include "stringutil.h"
#include "target.h"
#include "terrain.h"
#include "transform.h"
#include "unicode.h"
#include "viewchar.h"
#include "view.h"

// This spell has two main advantages over Fireball:
//
// (1) The release is instantaneous, so monsters will not
//     get an action before the player... this allows the
//     player to hit monsters with a double fireball (this
//     is why we only allow one delayed fireball at a time,
//     if you want to allow for more, then the release should
//     take at least some amount of time).
//
//     The casting of this spell still costs a turn.  So
//     casting Delayed Fireball and immediately releasing
//     the fireball is only slightly different from casting
//     a regular Fireball (monsters act in the middle instead
//     of at the end).  This is why we allow for the spell
//     level discount so that Fireball is free with this spell
//     (so that it only costs 7 levels instead of 12 to have
//     both).
//
// (2) When the fireball is released, it is guaranteed to
//     go off... the spell only fails at this point.  This can
//     be a large advantage for characters who have difficulty
//     casting Fireball in their standard equipment.  However,
//     the power level for the actual fireball is determined at
//     release, so if you do swap out your enhancers you'll
//     get a less powerful ball when it's released. - bwr
//
spret_type cast_delayed_fireball(bool fail)
{
    if (you.attribute[ATTR_DELAYED_FIREBALL])
    {
        mpr("You are already charged.");
        return SPRET_ABORT;
    }

    fail_check();
    // Okay, this message is weak but functional. - bwr
    mpr("You feel magically charged.");
    you.attribute[ATTR_DELAYED_FIREBALL] = 1;
    return SPRET_SUCCESS;
}

void setup_fire_storm(const actor *source, int pow, bolt &beam)
{
    beam.name         = "great blast of fire";
    beam.ex_size      = 2 + (random2(pow) > 75);
    beam.flavour      = BEAM_LAVA;
    beam.real_flavour = beam.flavour;
    beam.glyph        = dchar_glyph(DCHAR_FIRED_ZAP);
    beam.colour       = RED;
    beam.source_id    = source->mid;
    // XXX: Should this be KILL_MON_MISSILE?
    beam.thrower      =
        source->is_player() ? KILL_YOU_MISSILE : KILL_MON;
    beam.aux_source.clear();
    beam.obvious_effect = false;
    beam.is_beam      = false;
    beam.is_tracer    = false;
    beam.is_explosion = true;
    beam.ench_power   = pow;      // used for radius
    beam.hit          = 20 + pow / 10;
    beam.damage       = calc_dice(8, 5 + pow);
    beam.origin_spell = SPELL_FIRE_STORM;
}

spret_type cast_fire_storm(int pow, bolt &beam, bool fail)
{
    if (distance2(beam.target, beam.source) > dist_range(beam.range))
    {
        mpr("That is beyond the maximum range.");
        return SPRET_ABORT;
    }

    if (cell_is_solid(beam.target))
    {
        const char *feat = feat_type_name(grd(beam.target));
        mprf("You can't place the storm on %s.", article_a(feat).c_str());
        return SPRET_ABORT;
    }

    fail_check();
    setup_fire_storm(&you, pow, beam);

    bolt tempbeam = beam;
    tempbeam.ex_size = (pow > 76) ? 3 : 2;
    tempbeam.is_tracer = true;

    tempbeam.explode(false);
    if (tempbeam.beam_cancelled)
    {
        canned_msg(MSG_OK);
        return SPRET_ABORT;
    }

    beam.refine_for_explosion();
    beam.explode(false);

    viewwindow();
    return SPRET_SUCCESS;
}

// No setup/cast split here as monster hellfire is completely different.
// Sad, but needed to maintain balance - monster hellfirers get asymmetric
// torment too.
bool cast_hellfire_burst(int pow, bolt &beam)
{
    beam.name              = "burst of hellfire";
    beam.aux_source        = "burst of hellfire";
    beam.ex_size           = 1;
    beam.flavour           = BEAM_HELLFIRE;
    beam.real_flavour      = beam.flavour;
    beam.glyph             = dchar_glyph(DCHAR_FIRED_BURST);
    beam.colour            = LIGHTRED;
    beam.source_id         = MID_PLAYER;
    beam.thrower           = KILL_YOU;
    beam.obvious_effect    = false;
    beam.is_beam           = false;
    beam.is_explosion      = true;
    beam.ench_power        = pow;      // used for radius
    beam.hit               = 20 + pow / 10;
    beam.damage            = calc_dice(6, 30 + pow);
    beam.smart_monster     = true;
    beam.attitude          = ATT_FRIENDLY;
    beam.friend_info.count = 0;
    beam.is_tracer         = true;

    beam.explode(false);

    if (beam.beam_cancelled)
    {
        canned_msg(MSG_OK);
        return false;
    }

    mpr("You call forth a pillar of hellfire!");

    beam.is_tracer = false;
    beam.in_explosion_phase = false;
    beam.explode(true);

    return true;
}

// XXX no friendly check
spret_type cast_chain_spell(spell_type spell_cast, int pow,
                            const actor *caster, bool fail)
{
    fail_check();
    bolt beam;

    // initialise beam structure
    switch (spell_cast)
    {
        case SPELL_CHAIN_LIGHTNING:
            beam.name           = "lightning arc";
            beam.aux_source     = "chain lightning";
            beam.glyph          = dchar_glyph(DCHAR_FIRED_ZAP);
            beam.flavour        = BEAM_ELECTRICITY;
            break;
        case SPELL_CHAIN_OF_CHAOS:
            beam.name           = "arc of chaos";
            beam.aux_source     = "chain of chaos";
            beam.glyph          = dchar_glyph(DCHAR_FIRED_ZAP);
            beam.flavour        = BEAM_CHAOS;
            break;
        default:
            die("buggy chain spell %d cast", spell_cast);
            break;
    }
    beam.source_id      = caster->mid;
    beam.thrower        = caster->is_player() ? KILL_YOU_MISSILE : KILL_MON_MISSILE;
    beam.range          = 8;
    beam.hit            = AUTOMATIC_HIT;
    beam.obvious_effect = true;
    beam.is_beam        = false;       // since we want to stop at our target
    beam.is_explosion   = false;
    beam.is_tracer      = false;
    beam.origin_spell   = spell_cast;

    if (const monster* mons = caster->as_monster())
        beam.source_name = mons->name(DESC_PLAIN, true);

    bool first = true;
    coord_def source, target;

    for (source = caster->pos(); pow > 0;
         pow -= 8 + random2(13), source = target)
    {
        // infinity as far as this spell is concerned
        // (Range - 1) is used because the distance is randomised and
        // may be shifted by one.
        int min_dist = MONSTER_LOS_RANGE - 1;

        int dist;
        int count = 0;

        target.x = -1;
        target.y = -1;

        for (monster_iterator mi; mi; ++mi)
        {
            if (invalid_monster(*mi))
                continue;

            // Don't arc to things we cannot hit.
            if (beam.ignores_monster(*mi))
                continue;

            dist = grid_distance(source, mi->pos());

            // check for the source of this arc
            if (!dist)
                continue;

            // randomise distance (arcs don't care about a couple of feet)
            dist += (random2(3) - 1);

            // always ignore targets further than current one
            if (dist > min_dist)
                continue;

            if (!cell_see_cell(source, mi->pos(), LOS_SOLID)
                || !cell_see_cell(caster->pos(), mi->pos(), LOS_SOLID_SEE))
            {
                continue;
            }

            count++;

            if (dist < min_dist)
            {
                // switch to looking for closer targets (but not always)
                if (!one_chance_in(10))
                {
                    min_dist = dist;
                    target = mi->pos();
                    count = 0;
                }
            }
            else if (target.x == -1 || one_chance_in(count))
            {
                // either first target, or new selected target at
                // min_dist == dist.
                target = mi->pos();
            }
        }

        // now check if the player is a target
        dist = grid_distance(source, you.pos());

        if (dist)       // i.e., player was not the source
        {
            // distance randomised (as above)
            dist += (random2(3) - 1);

            // select player if only, closest, or randomly selected
            if ((target.x == -1
                    || dist < min_dist
                    || (dist == min_dist && one_chance_in(count + 1)))
                && cell_see_cell(source, you.pos(), LOS_SOLID))
            {
                target = you.pos();
            }
        }

        const bool see_source = you.see_cell(source);
        const bool see_targ   = you.see_cell(target);

        if (target.x == -1)
        {
            if (see_source)
                mprf("The %s grounds out.", beam.name.c_str());

            break;
        }

        // Trying to limit message spamming here so we'll only mention
        // the thunder at the start or when it's out of LoS.
        switch (spell_cast)
        {
            case SPELL_CHAIN_LIGHTNING:
            {
                const char* msg = "You hear a mighty clap of thunder!";
                noisy(spell_effect_noise(SPELL_CHAIN_LIGHTNING), source,
                      (first || !see_source) ? msg : NULL);
                break;
            }
            case SPELL_CHAIN_OF_CHAOS:
                if (first && see_source)
                    mpr("A swirling arc of seething chaos appears!");
                break;
            default:
                break;
        }
        first = false;

        if (see_source && !see_targ)
            mprf("The %s arcs out of your line of sight!", beam.name.c_str());
        else if (!see_source && see_targ)
            mprf("The %s suddenly appears!", beam.name.c_str());

        beam.source = source;
        beam.target = target;
        switch (spell_cast)
        {
            case SPELL_CHAIN_LIGHTNING:
                beam.colour = LIGHTBLUE;
                beam.damage = calc_dice(5, 10 + pow * 2 / 3);
                break;
            case SPELL_CHAIN_OF_CHAOS:
                beam.colour       = ETC_RANDOM;
                beam.ench_power   = pow;
                beam.damage       = calc_dice(3, 5 + pow / 2);
                beam.real_flavour = BEAM_CHAOS;
                beam.flavour      = BEAM_CHAOS;
            default:
                break;
        }

        // Be kinder to the caster.
        if (target == caster->pos())
        {
            if (!(beam.damage.num /= 2))
                beam.damage.num = 1;
            if ((beam.damage.size /= 2) < 3)
                beam.damage.size = 3;
        }
        beam.fire();
    }

    return SPRET_SUCCESS;
}

static counted_monster_list _counted_monster_list_from_vector(
    vector<monster *> affected_monsters)
{
    counted_monster_list mons;
    for (auto mon : affected_monsters)
        mons.add(mon);
    return mons;
}

static bool _refrigerateable(const actor *agent, const actor *act)
{
    // Inconsistency: monsters suffer no damage at rC+++, players suffer
    // considerable damage.
    return act->is_player() || act->res_cold() < 3;
}

static bool _refrigerateable_hitfunc(const actor *act)
{
    return _refrigerateable(&you, act);
}

static void _pre_refrigerate(actor* agent, bool player,
                             vector<monster *> affected_monsters)
{
    if (!affected_monsters.empty())
    {
        // Filter out affected monsters that we don't know for sure are there
        vector<monster*> seen_monsters;
        for (unsigned int i = 0; i < affected_monsters.size(); ++i)
        {
            if (you.can_see(affected_monsters[i]))
                seen_monsters.push_back(affected_monsters[i]);
        }

        if (!seen_monsters.empty())
        {
            counted_monster_list mons_list =
                _counted_monster_list_from_vector(seen_monsters);
            const string message =
                make_stringf("%s %s frozen.",
                            mons_list.describe(DESC_THE, true).c_str(),
                            conjugate_verb("be", mons_list.count() > 1).c_str());
            if (strwidth(message) < get_number_of_cols() - 2)
                mpr(message);
            else
            {
                // Exclamation mark to suggest that a lot of creatures were
                // affected.
                mprf("The monsters around %s are frozen!",
                    agent && agent->is_monster() && you.can_see(agent)
                    ? agent->as_monster()->name(DESC_THE).c_str()
                    : "you");
            }
        }
    }
}

static const dice_def _refrigerate_damage(int pow)
{
    return dice_def(3, 5 + pow / 10);
}

static int _refrigerate_player(actor* agent, int pow, int avg,
                               bool actual, bool added_effects)
{
    const dice_def dam_dice = _refrigerate_damage(pow);

    int hurted = check_your_resists((actual) ? dam_dice.roll() : avg,
                                    BEAM_COLD, "refrigeration", 0, actual);
    if (actual && hurted > 0)
    {
        mpr("You feel very cold.");
        if (agent && !agent->is_player())
        {
            ouch(hurted, KILLED_BY_BEAM, agent->mid,
                 "by Ozocubu's Refrigeration", true,
                 agent->as_monster()->name(DESC_A).c_str());
            expose_player_to_element(BEAM_COLD, 5, added_effects);

            // Note: this used to be 12!... and it was also applied even if
            // the player didn't take damage from the cold, so we're being
            // a lot nicer now.  -- bwr
        }
        else
        {
            ouch(hurted, KILLED_BY_FREEZING);
            you.increase_duration(DUR_NO_POTIONS, 7 + random2(9), 15);
        }
    }

    return hurted;
}

static int _refrigerate_monster(actor* agent, monster* target, int pow, int avg,
                                bool actual, bool added_effects)
{
    const dice_def dam_dice = _refrigerate_damage(pow);

    bolt beam;
    beam.flavour = BEAM_COLD;
    beam.thrower = (agent && agent->is_player()) ? KILL_YOU :
                   (agent)                       ? KILL_MON
                                                 : KILL_MISC;

    int hurted = mons_adjust_flavoured(target, beam,
                                       (actual) ? dam_dice.roll() : avg,
                                       actual);
    dprf("damage done: %d", hurted);

    if (actual)
    {
        behaviour_event(target, ME_ANNOY, agent,
                        agent ? agent->pos() : coord_def(0, 0));

        target->hurt(agent, hurted, BEAM_COLD);

        if (agent && agent->is_player()
            && (is_sanctuary(you.pos()) || is_sanctuary(target->pos())))
        {
            remove_sanctuary(true);
        }

        // Cold-blooded creatures can be slowed.
        if (target->alive())
            target->expose_to_element(BEAM_COLD, 5);
    }

    return hurted;
}

static bool _drain_lifeable(const actor* agent, const actor* act)
{
    if (act->res_negative_energy())
        return false;

    if (!agent)
        return true;

    const monster* mons = agent->as_monster();
    const monster* m = act->as_monster();

    return !(agent->is_player() && act->wont_attack()
             || mons && act->is_player() && mons->wont_attack()
             || mons && m && mons_atts_aligned(mons->attitude, m->attitude));
}

static bool _drain_lifeable_hitfunc(const actor* act)
{
    return _drain_lifeable(&you, act);
}

static int _drain_player(actor* agent, int pow, int avg,
                         bool actual, bool added_effects)
{
    if (actual)
    {
        monster* mons = agent ? agent->as_monster() : 0;
        ouch(avg, KILLED_BY_BEAM, mons ? mons->mid : MID_NOBODY,
             "by drain life");
    }

    return avg;
}

static int _drain_monster(actor* agent, monster* target, int pow, int avg,
                          bool actual, bool added_effects)
{
    if (actual)
    {
        if (agent && agent->is_player())
        {
            mprf("You draw life from %s.",
                 target->name(DESC_THE).c_str());
        }

        behaviour_event(target, ME_ANNOY, agent,
                        agent ? agent->pos() : coord_def(0, 0));

        target->hurt(agent, avg);

        if (target->alive() && you.can_see(target))
            print_wounds(target);
    }

    if (!target->is_summoned())
        return avg;

    return 0;
}

static void _post_drain_life(actor* agent, bool player,
                             vector<monster *> affected_monsters,
                             int pow, int total_damage)
{
    total_damage /= 2;

    total_damage = min(pow * 2, total_damage);

    if (total_damage && agent)
    {
        if (agent->is_player())
        {
            mpr("You feel life flooding into your body.");
            inc_hp(total_damage);
        }
        else
        {
            monster* mons = agent->as_monster();
            ASSERT(mons);
            if (mons->heal(total_damage))
                simple_monster_message(mons, " is healed.");
        }
    }
}

spret_type cast_los_attack_spell(spell_type spell, int pow, actor* agent,
                                 bool actual, bool added_effects, bool fail,
                                 bool allow_cancel)
{
    monster* mons = agent ? agent->as_monster() : NULL;

    colour_t flash_colour = BLACK;
    const char *player_msg = nullptr, *global_msg = nullptr,
               *mons_vis_msg = nullptr, *mons_invis_msg = nullptr;
    bool (*vulnerable)(const actor *, const actor *) = NULL;
    bool (*vul_hitfunc)(const actor *) = NULL;
    int (*damage_player)(actor *, int, int, bool, bool) = NULL;
    int (*damage_monster)(actor *, monster *, int, int, bool, bool) = NULL;
    void (*pre_hook)(actor*, bool, vector<monster *>) = NULL;
    void (*post_hook)(actor*, bool, vector<monster *>, int, int) = NULL;

    int hurted = 0;
    int this_damage = 0;
    int total_damage = 0;

    bolt beam;
    beam.source_id = agent->mid;
    beam.foe_ratio = 80;

    switch (spell)
    {
        case SPELL_OZOCUBUS_REFRIGERATION:
            player_msg = "The heat is drained from your surroundings.";
            global_msg = "Something drains the heat from around you.";
            mons_vis_msg = " drains the heat from the surrounding"
                           " environment!";
            mons_invis_msg = "The ambient heat is drained!";
            flash_colour = LIGHTCYAN;
            vulnerable = &_refrigerateable;
            vul_hitfunc = &_refrigerateable_hitfunc;
            damage_player = &_refrigerate_player;
            damage_monster = &_refrigerate_monster;
            pre_hook = &_pre_refrigerate;
            hurted = (3 + (5 + pow / 10)) / 2; // average
            break;

        case SPELL_DRAIN_LIFE:
            player_msg = "You draw life from your surroundings.";
            global_msg = "Something draws the life force from your"
                         " surroundings.";
            mons_vis_msg = " draws from the surrounding life force!";
            mons_invis_msg = "The surrounding life force dissipates!";
            flash_colour = DARKGREY;
            vulnerable = &_drain_lifeable;
            vul_hitfunc = &_drain_lifeable_hitfunc;
            damage_player = &_drain_player;
            damage_monster = &_drain_monster;
            post_hook = &_post_drain_life;
            hurted = 3 + random2(7) + random2(pow);
            break;

        default: return SPRET_ABORT;
    }

    if (agent && agent->is_player())
    {
        ASSERT(actual);
        targetter_los hitfunc(&you, LOS_NO_TRANS);
        {
            if (allow_cancel && stop_attack_prompt(hitfunc, "harm", vul_hitfunc))
                return SPRET_ABORT;
        }
        fail_check();

        mpr(player_msg);
        flash_view(UA_PLAYER, flash_colour, &hitfunc);
        more();
        clear_messages();
        flash_view(UA_PLAYER, 0);
    }
    else if (actual)
    {
        if (!agent)
            mpr(global_msg);
        else if (you.can_see(agent))
            simple_monster_message(mons, mons_vis_msg);
        else if (you.see_cell(agent->pos()))
            mpr(mons_invis_msg);

        flash_view_delay(UA_MONSTER, flash_colour, 300);
    }

    bool affects_you = false;
    vector<monster *> affected_monsters;

    for (actor_near_iterator ai((agent ? agent : &you)->pos(), LOS_NO_TRANS);
         ai; ++ai)
    {
        if ((*vulnerable)(agent, *ai))
        {
            if (ai->is_player())
                affects_you = true;
            else
                affected_monsters.push_back(ai->as_monster());
        }
    }

    // XXX: This ordering is kind of broken; it's to preserve the message
    // order from the original behaviour in the case of refrigerate.
    if (affects_you)
    {
        total_damage = (*damage_player)(agent, pow, hurted, actual,
                                        added_effects);
        if (!actual && mons)
        {
            if (mons->wont_attack())
            {
                beam.friend_info.count++;
                beam.friend_info.power +=
                    (you.get_experience_level() * this_damage / hurted);
            }
            else
            {
                beam.foe_info.count++;
                beam.foe_info.power +=
                    (you.get_experience_level() * this_damage / hurted);
            }
        }
    }

    if (actual && pre_hook)
        (*pre_hook)(agent, affects_you, affected_monsters);

    for (auto m : affected_monsters)
    {
        // Watch out for invalidation. Example: Ozocubu's refrigeration on
        // a bunch of giant spores that blow each other up.
        if (!m->alive())
            continue;

        this_damage = (*damage_monster)(agent, m, pow, hurted, actual,
                                        added_effects);
        total_damage += this_damage;

        if (!actual && mons)
        {
            if (mons_atts_aligned(m->attitude, mons->attitude))
            {
                beam.friend_info.count++;
                beam.friend_info.power += (m->get_hit_dice() * this_damage / hurted);
            }
            else
            {
                beam.foe_info.count++;
                beam.foe_info.power += (m->get_hit_dice() * this_damage / hurted);
            }
        }
    }

    if (actual)
    {
        if (post_hook)
        {
            (*post_hook)(agent, affects_you, affected_monsters, pow,
                         total_damage);
        }

        return SPRET_SUCCESS;
    }

    return mons_should_fire(beam) ? SPRET_SUCCESS : SPRET_ABORT;
}

// Screaming Sword
void sonic_damage(bool scream)
{
    if (is_sanctuary(you.pos()))
        return;

    // First build the message.
    counted_monster_list affected_monsters;

    for (monster_near_iterator mi(you.pos(), LOS_SOLID); mi; ++mi)
        if (!silenced(mi->pos()))
            affected_monsters.add(*mi);

    /* dpeg sez:
       * damage applied to everyone but the wielder (reasoning: the sword
         does not like competitors, so no allies)
       -- so sorry, Beoghites, Jiyvaites and the rest.
    */

    if (!affected_monsters.empty())
    {
        const string message =
            make_stringf("%s %s hurt by the noise.",
                         affected_monsters.describe().c_str(),
                         conjugate_verb("be", affected_monsters.count() > 1).c_str());
        if (strwidth(message) < get_number_of_cols() - 2)
            mpr(message);
        else
        {
            // Exclamation mark to suggest that a lot of creatures were
            // affected.
            mpr("The monsters around you reel from the noise!");
        }
    }

    // Now damage the creatures.
    for (monster_near_iterator mi(you.pos(), LOS_SOLID); mi; ++mi)
    {
        if (silenced(mi->pos()))
            continue;
        int hurt = (random2(2) + 1) * (random2(2) + 1) * (random2(3) + 1)
                 + (random2(3) + 1) + 1;
        if (scream)
            hurt = max(hurt * 2, 16);
        int cap = scream ? mi->max_hit_points / 2 : mi->max_hit_points * 3 / 10;
        hurt = min(hurt, max(cap, 1));
        // not so much damage if you're a n00b
        hurt = div_rand_round(hurt * you.experience_level, 27);
        /* per dpeg:
         * damage is universal (well, only to those who can hear, but not sure
           we can determine that in-game), i.e. smiting, no resists
        */
        dprf("damage done: %d", hurt);
        mi->hurt(&you, hurt);

        if (is_sanctuary(mi->pos()))
            remove_sanctuary(true);
    }
}

spret_type vampiric_drain(int pow, monster* mons, bool fail)
{
    if (mons == NULL || mons->submerged())
    {
        fail_check();
        canned_msg(MSG_NOTHING_CLOSE_ENOUGH);
        // Cost to disallow freely locating invisible/submerged
        // monsters.
        return SPRET_SUCCESS;
    }

    if (mons->observable() && mons->holiness() != MH_NATURAL)
    {
        mpr("Draining that being is not a good idea.");
        return SPRET_ABORT;
    }

    god_conduct_trigger conducts[3];
    disable_attack_conducts(conducts);

    const bool abort = stop_attack_prompt(mons, false, you.pos());

    if (!abort && !fail)
    {
        set_attack_conducts(conducts, mons);

        behaviour_event(mons, ME_WHACK, &you, you.pos());
    }

    enable_attack_conducts(conducts);

    if (abort)
    {
        canned_msg(MSG_OK);
        return SPRET_ABORT;
    }

    fail_check();

    if (!mons->alive())
    {
        canned_msg(MSG_NOTHING_HAPPENS);
        return SPRET_SUCCESS;
    }

    // Monster might be invisible.
    if (mons->holiness() == MH_UNDEAD || mons->holiness() == MH_DEMONIC)
    {
        mpr("Aaaarggghhhhh!");
        dec_hp(random2avg(39, 2) + 10, false, "vampiric drain backlash");
        return SPRET_SUCCESS;
    }

    if (mons->holiness() != MH_NATURAL || mons->res_negative_energy())
    {
        canned_msg(MSG_NOTHING_HAPPENS);
        return SPRET_SUCCESS;
    }

    // The practical maximum of this is about 25 (pow @ 100). - bwr
    int hp_gain = 3 + random2avg(9, 2) + random2(pow) / 7;

    hp_gain = min(mons->hit_points, hp_gain);
    hp_gain = min(you.hp_max - you.hp, hp_gain);

    if (!hp_gain)
    {
        canned_msg(MSG_NOTHING_HAPPENS);
        return SPRET_SUCCESS;
    }

    const bool mons_was_summoned = mons->is_summoned();

    mons->hurt(&you, hp_gain);

    if (mons->alive())
        print_wounds(mons);

    hp_gain = div_rand_round(hp_gain, 2);

    if (hp_gain && !mons_was_summoned && !you.duration[DUR_DEATHS_DOOR])
    {
        mpr("You feel life coursing into your body.");
        inc_hp(hp_gain);
    }

    return SPRET_SUCCESS;
}

spret_type cast_freeze(int pow, monster* mons, bool fail)
{
    pow = min(25, pow);

    if (mons == NULL || mons->submerged())
    {
        fail_check();
        canned_msg(MSG_NOTHING_CLOSE_ENOUGH);
        // If there's no monster there, you still pay the costs in
        // order to prevent locating invisible/submerged monsters.
        return SPRET_SUCCESS;
    }

    god_conduct_trigger conducts[3];
    disable_attack_conducts(conducts);

    const bool abort = stop_attack_prompt(mons, false, you.pos());

    if (!abort && !fail)
    {
        set_attack_conducts(conducts, mons);

        mprf("You freeze %s.", mons->name(DESC_THE).c_str());

        behaviour_event(mons, ME_ANNOY, &you);
    }

    enable_attack_conducts(conducts);

    if (abort)
    {
        canned_msg(MSG_OK);
        return SPRET_ABORT;
    }

    fail_check();

    bolt beam;
    beam.flavour = BEAM_COLD;
    beam.thrower = KILL_YOU;

    const int orig_hurted = roll_dice(1, 3 + pow / 3);
    int hurted = mons_adjust_flavoured(mons, beam, orig_hurted);
    mons->hurt(&you, hurted);

    if (mons->alive())
    {
        mons->expose_to_element(BEAM_COLD, orig_hurted);
        print_wounds(mons);

        const int cold_res = mons->res_cold();
        if (cold_res <= 0)
        {
            const int stun = (1 - cold_res) * random2(2 + pow/5);
            mons->speed_increment -= stun;
        }
    }

    return SPRET_SUCCESS;
}

spret_type cast_airstrike(int pow, const dist &beam, bool fail)
{
    if (cell_is_solid(beam.target))
    {
        canned_msg(MSG_SPELL_FIZZLES);
        return SPRET_ABORT;
    }

    monster* mons = monster_at(beam.target);
    if (!mons || mons->submerged())
    {
        fail_check();
        canned_msg(MSG_SPELL_FIZZLES);
        return SPRET_SUCCESS; // still losing a turn
    }

    if (mons->res_wind())
    {
        if (mons->observable())
        {
            mprf("But air would do no harm to %s!",
                 mons->name(DESC_THE).c_str());
            return SPRET_ABORT;
        }

        fail_check();
        mprf("The air twists arounds and harmlessly tosses %s around.",
             mons->name(DESC_THE).c_str());
        // Bailing out early, no need to upset the gods or the target.
        return SPRET_SUCCESS; // you still did discover the invisible monster
    }

    god_conduct_trigger conducts[3];
    disable_attack_conducts(conducts);

    if (stop_attack_prompt(mons, false, you.pos()))
        return SPRET_ABORT;

    fail_check();
    set_attack_conducts(conducts, mons);

    mprf("The air twists around and %sstrikes %s!",
         mons->flight_mode() ? "violently " : "",
         mons->name(DESC_THE).c_str());
    noisy(spell_effect_noise(SPELL_AIRSTRIKE), beam.target);

    behaviour_event(mons, ME_ANNOY, &you);

    enable_attack_conducts(conducts);

    int hurted = 8 + random2(random2(4) + (random2(pow) / 6)
                   + (random2(pow) / 7));

    bolt pbeam;
    pbeam.flavour = BEAM_AIR;

#ifdef DEBUG_DIAGNOSTICS
    const int preac = hurted;
#endif
    hurted = mons->apply_ac(mons->beam_resists(pbeam, hurted, false));
    dprf("preac: %d, postac: %d", preac, hurted);

    mons->hurt(&you, hurted);
    if (mons->alive())
        print_wounds(mons);

    return SPRET_SUCCESS;
}

// Just to avoid typing this over and over.
// Returns true if monster died. -- bwr
static bool _player_hurt_monster(monster& m, int damage,
                                 beam_type flavour = BEAM_MISSILE)
{
    if (damage > 0)
    {
        m.hurt(&you, damage, flavour, false);

        if (m.alive())
        {
            print_wounds(&m);
            behaviour_event(&m, ME_WHACK, &you);
        }
        else
        {
            monster_die(&m, KILL_YOU, NON_MONSTER);
            return true;
        }
    }

    return false;
}

// Here begin the actual spells:
static int _shatter_mon_dice(const monster *mon)
{
    if (!mon)
        return 0;

    // Removed a lot of silly monsters down here... people, just because
    // it says ice, rock, or iron in the name doesn't mean it's actually
    // made out of the substance. - bwr
    switch (mon->type)
    {
    // Double damage to stone, metal and crystal.
    case MONS_EARTH_ELEMENTAL:
    case MONS_USHABTI:
    case MONS_STATUE:
    case MONS_GARGOYLE:
    case MONS_IRON_ELEMENTAL:
    case MONS_IRON_GOLEM:
    case MONS_WAR_GARGOYLE:
    case MONS_CRYSTAL_GUARDIAN:
    case MONS_OBSIDIAN_STATUE:
    case MONS_ORANGE_STATUE:
    case MONS_ROXANNE:
        return 6;

    // 1/3 damage to liquids.
    case MONS_WATER_ELEMENTAL:
        return 1;

    default:
        const bool petrifying = mon->petrifying();
        const bool petrified = mon->petrified();

        // Extra damage to petrifying/petrified things.
        // Undo the damage reduction as well; base damage is 4 : 6.
        if (petrifying || petrified)
            return petrifying ? 6 : 12;
        // No damage to insubstantials.
        else if (mon->is_insubstantial())
            return 0;
        // 1/3 damage to fliers and slimes.
        else if (mons_flies(mon) || mons_is_slime(mon))
            return 1;
        // 3/2 damage to ice.
        else if (mon->is_icy())
            return coinflip() ? 5 : 4;
        // Double damage to bone.
        else if (mon->is_skeletal())
            return 6;
        // Normal damage to everything else.
        else
            return 3;
    }
}

static int _shatter_monsters(coord_def where, int pow, actor *agent)
{
    dice_def dam_dice(0, 5 + pow / 3); // Number of dice set below.
    monster* mon = monster_at(where);

    if (mon == NULL || mon == agent)
        return 0;

    dam_dice.num = _shatter_mon_dice(mon);
    int damage = max(0, dam_dice.roll() - random2(mon->armour_class()));

    if (damage <= 0)
        return 0;

    if (agent->is_player())
    {
        _player_hurt_monster(*mon, damage);

        if (is_sanctuary(you.pos()) || is_sanctuary(mon->pos()))
            remove_sanctuary(true);
    }
    else
        mon->hurt(agent, damage);

    return damage;
}

static int _shatter_walls(coord_def where, int pow, actor *agent)
{
    int chance = 0;

    // if not in-bounds then we can't really shatter it -- bwr
    if (!in_bounds(where))
        return 0;

    if (env.markers.property_at(where, MAT_ANY, "veto_shatter") == "veto")
        return 0;

    const dungeon_feature_type grid = grd(where);

    switch (grid)
    {
    case DNGN_CLOSED_DOOR:
    case DNGN_RUNED_DOOR:
    case DNGN_OPEN_DOOR:
    case DNGN_SEALED_DOOR:
        if (you.see_cell(where))
            mpr("A door shatters!");
        chance = 100;
        break;

    case DNGN_GRATE:
        if (you.see_cell(where))
            mpr("An iron grate is ripped into pieces!");
        chance = 100;
        break;

    case DNGN_METAL_WALL:
        chance = pow / 10;
        break;

    case DNGN_ORCISH_IDOL:
    case DNGN_GRANITE_STATUE:
        chance = 50;
        break;

    case DNGN_CLEAR_STONE_WALL:
    case DNGN_STONE_WALL:
        chance = pow / 6;
        break;

    case DNGN_CLEAR_ROCK_WALL:
    case DNGN_ROCK_WALL:
    case DNGN_SLIMY_WALL:
        chance = pow / 4;
        break;

    case DNGN_GREEN_CRYSTAL_WALL:
        chance = 50;
        break;

    case DNGN_TREE:
        chance = 33;
        break;

    default:
        break;
    }

    if (x_chance_in_y(chance, 100))
    {
        noisy(spell_effect_noise(SPELL_SHATTER), where);

        destroy_wall(where);

        if (agent->is_player() && grid == DNGN_ORCISH_IDOL)
            did_god_conduct(DID_DESTROY_ORCISH_IDOL, 8);
        if (agent->is_player() && feat_is_tree(grid))
            did_god_conduct(DID_KILL_PLANT, 1);

        return 1;
    }

    return 0;
}

static int _shatter_player_dice()
{
    if (you.is_insubstantial())
        return 0;
    else if (you.petrified())
        return 12; // reduced later
    else if (you.petrifying())
        return 6;  // reduced later
    // Same order as for monsters -- petrified flyers get hit hard, skeletal
    // flyers get no extra damage.
    else if (you.airborne())
        return 1;
    else if (you.form == TRAN_STATUE || you.species == SP_GARGOYLE)
        return 6;
    else if (you.form == TRAN_ICE_BEAST)
        return coinflip() ? 5 : 4;
    else
        return 3;
}

/**
 * Is this a valid target for shatter?
 *
 * @param act     The actor being considered
 * @return        Whether the actor will take damage from shatter.
 */
static bool _shatterable(const actor *act)
{
    if (act->is_player())
        return _shatter_player_dice();
    return _shatter_mon_dice(act->as_monster());
}

spret_type cast_shatter(int pow, bool fail)
{
    {
        targetter_los hitfunc(&you, LOS_ARENA);
        if (stop_attack_prompt(hitfunc, "harm", _shatterable))
            return SPRET_ABORT;
    }

    fail_check();
    const bool silence = silenced(you.pos());

    if (silence)
        mpr("The dungeon shakes!");
    else
    {
        noisy(spell_effect_noise(SPELL_SHATTER), you.pos());
        mprf(MSGCH_SOUND, "The dungeon rumbles!");
    }

    run_animation(ANIMATION_SHAKE_VIEWPORT, UA_PLAYER);

    int dest = 0;
    for (distance_iterator di(you.pos(), true, true, LOS_RADIUS); di; ++di)
    {
        // goes from the center out, so newly dug walls recurse
        if (!cell_see_cell(you.pos(), *di, LOS_SOLID))
            continue;

        _shatter_monsters(*di, pow, &you);
        dest += _shatter_walls(*di, pow, &you);
    }

    if (dest && !silence)
        mprf(MSGCH_SOUND, "Ka-crash!");

    return SPRET_SUCCESS;
}

static int _shatter_player(int pow, actor *wielder, bool devastator = false)
{
    if (wielder->is_player())
        return 0;

    dice_def dam_dice(_shatter_player_dice(), 5 + pow / 3);

    int damage = max(0, dam_dice.roll() - random2(you.armour_class()));

    if (damage > 0)
    {
        mpr(damage > 15 ? "You shudder from the earth-shattering force."
                        : "You shudder.");
        if (devastator)
            ouch(damage, KILLED_BY_MONSTER, wielder->mid);
        else
            ouch(damage, KILLED_BY_BEAM, wielder->mid, "by Shatter");
    }

    return damage;
}

bool mons_shatter(monster* caster, bool actual)
{
    const bool silence = silenced(caster->pos());
    int foes = 0;

    if (actual)
    {
        if (silence)
        {
            mprf("The dungeon shakes around %s!",
                 caster->name(DESC_THE).c_str());
        }
        else
        {
            noisy(spell_effect_noise(SPELL_SHATTER), caster->pos(), caster->mid);
            mprf(MSGCH_SOUND, "The dungeon rumbles around %s!",
                 caster->name(DESC_THE).c_str());
        }
    }

    int pow = 5 + div_rand_round(caster->get_hit_dice() * 9, 2);

    int dest = 0;
    for (distance_iterator di(caster->pos(), true, true, LOS_RADIUS); di; ++di)
    {
        // goes from the center out, so newly dug walls recurse
        if (!cell_see_cell(caster->pos(), *di, LOS_SOLID))
            continue;

        if (actual)
        {
            _shatter_monsters(*di, pow, caster);
            if (*di == you.pos())
                _shatter_player(pow, caster);
            dest += _shatter_walls(*di, pow, caster);
        }
        else
        {
            if (you.pos() == *di)
                foes -= _shatter_player_dice();
            if (const monster *victim = monster_at(*di))
            {
                dprf("[%s]", victim->name(DESC_PLAIN, true).c_str());
                foes += _shatter_mon_dice(victim)
                     * (victim->wont_attack() ? -1 : 1);
            }
        }
    }

    if (dest && !silence)
        mprf(MSGCH_SOUND, "Ka-crash!");

    if (actual)
        run_animation(ANIMATION_SHAKE_VIEWPORT, UA_MONSTER);

    if (!caster->wont_attack())
        foes *= -1;

    if (!actual)
        dprf("Shatter foe HD: %d", foes);

    return foes > 0; // doesn't matter if actual
}

void shillelagh(actor *wielder, coord_def where, int pow)
{
    bolt beam;
    beam.name = "shillelagh";
    beam.flavour = BEAM_VISUAL;
    beam.set_agent(wielder);
    beam.colour = BROWN;
    beam.glyph = dchar_glyph(DCHAR_EXPLOSION);
    beam.range = 1;
    beam.ex_size = 1;
    beam.is_explosion = true;
    beam.source = wielder->pos();
    beam.target = where;
    beam.hit = AUTOMATIC_HIT;
    beam.loudness = 7;
    beam.explode();

    counted_monster_list affected_monsters;
    for (adjacent_iterator ai(where, false); ai; ++ai)
    {
        monster *mon = monster_at(*ai);
        if (!mon || !mon->alive() || mon->submerged()
            || mon->is_insubstantial() || !you.can_see(mon)
            || mon == wielder)
        {
            continue;
        }
        affected_monsters.add(mon);
    }
    if (!affected_monsters.empty())
    {
        const string message =
            make_stringf("%s shudder%s.",
                         affected_monsters.describe().c_str(),
                         affected_monsters.count() == 1? "s" : "");
        if (strwidth(message) < get_number_of_cols() - 2)
            mpr(message);
        else
            mpr("There is a shattering impact!");
    }

    // need to do this again to do the actual damage
    for (adjacent_iterator ai(where, false); ai; ++ai)
        _shatter_monsters(*ai, pow * 3 / 2, wielder);

    if ((you.pos() - wielder->pos()).abs() <= 2 && in_bounds(you.pos()))
        _shatter_player(pow, wielder, true);
}

/**
 * Irradiate the given cell. (Per the spell.)
 *
 * @param where     The cell in question.
 * @param pow       The power with which the spell is being cast.
 * @param aux       Not sure; seems to be always 0?
 * @param agent     The agent (player or monster) doing the irradiating.
 */
static int _irradiate_cell(coord_def where, int pow, int aux, actor *agent)
{
    monster *mons = monster_at(where);
    if (!mons)
        return 0; // XXX: handle damaging the player for mons casts...?

    if (you.can_see(mons))
    {
        mprf("%s is blasted with magical radiation!",
             mons->name(DESC_THE).c_str());
    }

    const int dice = 6;
    const int max_dam = 30 + div_rand_round(pow, 2);
    const dice_def dam_dice = calc_dice(dice, max_dam);
    const int dam = dam_dice.roll();
    dprf("irr for %d (%d pow, max %d)", dam, pow, max_dam);

    if (agent->is_player())
        _player_hurt_monster(*mons, dam, BEAM_MMISSILE);
    else
        mons->hurt(agent, dam, BEAM_MMISSILE);

    if (mons->alive())
        mons->malmutate("");

    return 1;
}

/**
 * Attempt to cast the spell "Irradiate", damaging & deforming enemies around
 * the player.
 *
 * @param pow   The power at which the spell is being cast.
 * @param who   The actor doing the irradiating.
 * @param fail  Whether the player has failed to cast the spell.
 * @return      The result of casting the spell. (Success or failure.)
 */
spret_type cast_irradiate(int powc, actor* who, bool fail)
{
    fail_check();

    ASSERT(who);
    if (who->is_player())
        mpr("You erupt in a fountain of uncontrolled magic!");
    else
    {
        simple_monster_message(who->as_monster(),
                               " erupts in a fountain of uncontrolled magic!");
    }

    apply_random_around_square(_irradiate_cell, who->pos(), true, powc, 8, who);

    if (who->is_player())
    {
        contaminate_player(1500 + random2(1500)); // on avg, a bit under 50% of
                                                  // yellow contam
                                                  // another cast might or
                                                  // might not push you over
    }
    return SPRET_SUCCESS;
}

static int _ignite_poison_affect_item(item_def& item, bool in_inv, bool tracer = false)
{
    int strength = 0;

    if (item.base_type == OBJ_MISSILES && item.special == SPMSL_POISONED)
    {
        // Burn poison ammo.
        strength = item.quantity;
    }
    else if (item.base_type == OBJ_POTIONS)
    {
        // Burn poisonous potions.
        switch (item.sub_type)
        {
        case POT_DEGENERATION:
        case POT_POISON:
            strength = 10 * item.quantity;
            break;
        default:
            break;
        }
    }
    else if (item.base_type == OBJ_CORPSES &&
             item.sub_type == CORPSE_BODY &&
             carrion_is_poisonous(item))
    {
        strength = mons_weight(item.mon_type) / 25;
    }
    else if (item.base_type == OBJ_FOOD &&
             item.sub_type == FOOD_CHUNK &&
             carrion_is_poisonous(item))
    {
        strength += 30 * item.quantity;
    }

    if (strength)
    {
        if (item.base_type == OBJ_CORPSES
            && item.sub_type == CORPSE_BODY
            && mons_skeleton(item.mon_type)
            && !tracer)
        {
            turn_corpse_into_skeleton(item);
        }
        else if (!tracer)
        {
            item_was_destroyed(item);
            destroy_item(item.index());
        }
    }

    return strength;
}

// How much work can we consider we'll have done by igniting a cloud here?
// Considers a cloud under a susceptible ally bad, a cloud under a a susceptible
// enemy good, and other clouds relatively unimportant.
static int _ignite_tracer_cloud_value(coord_def where, actor *agent)
{
    actor* act = actor_at(where);
    if (act)
    {
        int dam = resist_adjust_damage(act, BEAM_FIRE, act->res_fire(), 40, true);
        return mons_aligned(act, agent) ? -dam : dam;
    }
    // We've done something, but its value is indeterminate
    else
        return 1;
}

static int _ignite_poison_objects(coord_def where, int pow, int, actor *agent)
{
    UNUSED(pow);

    const bool tracer = (pow == -1);  // Only testing damage, not dealing it

    int strength = 0;

    for (stack_iterator si(where); si; ++si)
        strength += _ignite_poison_affect_item(*si, false, tracer);

    if (strength > 0)
    {
        if (tracer)
            return _ignite_tracer_cloud_value(where, agent);

        place_cloud(CLOUD_FIRE, where,
                    strength + roll_dice(3, strength / 4), agent);
    }

    return strength;
}

static int _ignite_poison_clouds(coord_def where, int pow, int, actor *agent)
{
    UNUSED(pow);

    const bool tracer = (pow == -1);  // Only testing damage, not dealing it

    const int i = env.cgrid(where);
    if (i != EMPTY_CLOUD)
    {
        cloud_struct& cloud = env.cloud[i];

        if (tracer && (cloud.type == CLOUD_MEPHITIC
                       || cloud.type == CLOUD_POISON))
        {
            return _ignite_tracer_cloud_value(where, agent);
        }

        if (cloud.type == CLOUD_MEPHITIC)
        {
            cloud.decay /= 2;

            if (cloud.decay < 1)
                cloud.decay = 1;
        }
        else if (cloud.type != CLOUD_POISON)
            return false;

        cloud.type = CLOUD_FIRE;
        cloud.whose = agent->kill_alignment();
        cloud.killer = agent->is_player() ? KILL_YOU_MISSILE : KILL_MON_MISSILE;
        cloud.source = agent->mid;
        return true;
    }

    return false;
}

static int _ignite_poison_monsters(coord_def where, int pow, int, actor *agent)
{
    bolt beam;
    beam.flavour = BEAM_FIRE;   // This is dumb, only used for adjust!

    const bool tracer = (pow == -1);  // Only testing damage, not dealing it
    if (tracer)                       // Give some fake damage to test resists
        pow = 100;

    dice_def dam_dice(0, 5 + pow/7);  // Dice added below if applicable.

    // If a monster casts Ignite Poison, it can't hit itself.
    // This doesn't apply to the other functions: it can ignite
    // clouds or items where it's standing!

    monster* mon = monster_at(where);
    if (mon == NULL || mon == agent)
        return 0;

    // Monsters which have poison corpses or poisonous attacks.
    if (mons_is_poisoner(mon))
        dam_dice.num = 3;

    // Monsters which are poisoned:
    int strength = 0;

    // First check for player poison.
    mon_enchant ench = mon->get_ench(ENCH_POISON);
    if (ench.ench != ENCH_NONE)
        strength += ench.degree;

    // Strength is now the sum of both poison types
    // (although only one should actually be present at a given time).
    dam_dice.num += strength;

    int damage = dam_dice.roll();
    damage = mons_adjust_flavoured(mon, beam, damage, false);
    if (damage > 0)
    {
        if (tracer)
            return mons_aligned(mon, agent) ? -1 * damage : damage;
        else
        {
            simple_monster_message(mon, " seems to burn from within!");

            dprf("Dice: %dd%d; Damage: %d", dam_dice.num, dam_dice.size, damage);

            mon->hurt(agent, damage);

            if (mon->alive())
            {
                behaviour_event(mon, ME_WHACK, agent);

                // Monster survived, remove any poison.
                mon->del_ench(ENCH_POISON);
                print_wounds(mon);
            }
            else
            {
                monster_die(mon,
                            agent->is_player() ? KILL_YOU : KILL_MON,
                            agent->mindex());
            }

            return 1;
        }
    }

    return 0;
}

static bool _player_has_poisonous_physiology()
{
    return player_mutation_level(MUT_SPIT_POISON)
           || player_mutation_level(MUT_STINGER)
           || you.form == TRAN_SPIDER // poison attack
           || (!form_changed_physiology()
               && (you.species == SP_GREEN_DRACONIAN       // poison breath
                   || you.species == SP_KOBOLD             // poisonous corpse
                   || you.species == SP_NAGA));           // spit poison
}

static int _ignite_poison_player(coord_def where, int pow, int, actor *agent)
{
    if (agent->is_player() || where != you.pos())
        return 0;

    const bool tracer = (pow == -1);  // Only testing damage, not dealing it
    if (tracer)                       // Give some fake damage to test resists
        pow = 100;

    int str = 0;        // Amount of poison for the spell to work with

    // Player is poisonous.
    if (_player_has_poisonous_physiology())
        str = 2;

    // Use the greater of the degree of player poisoning or the player's own
    // natural poison (meaning that poisoned kobolds are not affected much
    // worse than poisoned members of other races), but step down heavily beyond
    // light poisoning (or we could easily one-shot a heavily poisoned character)
    str = max(str, (int)stepdown((double)you.duration[DUR_POISONING]/5000, 2.25));

    int damage = roll_dice(str, 5 + pow/7);
    if (damage)
    {
        const int resist = player_res_fire();
        damage = resist_adjust_damage(&you, BEAM_FIRE, resist, damage, true);

        if (tracer)
            return mons_aligned(&you, agent) ? -1 * damage : damage;
        else
        {
            if (resist > 0)
                mpr("You feel like your blood is boiling!");
            else if (resist < 0)
                mpr("The poison in your system burns terribly!");
            else
                mpr("The poison in your system burns!");

            ouch(damage, KILLED_BY_MONSTER, agent->mid,
                agent->as_monster()->name(DESC_A).c_str());

            if (you.duration[DUR_POISONING] > 0)
            {
                mprf(MSGCH_RECOVERY, "You are no longer poisoned.");
                you.duration[DUR_POISONING] = 0;
            }
        }
    }

    if (damage)
        return 1;
    else
        return 0;
}

static bool maybe_abort_ignite()
{
    // Fire cloud immunity.
    if (you.duration[DUR_FIRE_SHIELD] || you.mutation[MUT_IGNITE_BLOOD])
        return false;

    string prompt = "You are standing ";

    // XXX XXX XXX major code duplication (ChrisOelmueller)

    const int i = env.cgrid(you.pos());
    if (i != EMPTY_CLOUD)
    {
        cloud_struct& cloud = env.cloud[i];

        if (cloud.type == CLOUD_MEPHITIC || cloud.type == CLOUD_POISON)
        {
            prompt += "in a cloud of ";
            prompt += cloud_type_name(cloud.type, true);
            prompt += "! Ignite poison anyway?";
            return !yesno(prompt.c_str(), false, 'n');
        }
    }

    // Now check for items at player position.
    item_def item;
    for (stack_iterator si(you.pos()); si; ++si)
    {
        item = *si;

        if (item.base_type == OBJ_MISSILES && item.special == SPMSL_POISONED)
        {
            prompt += "over ";
            prompt += (item.quantity == 1 ? "a " : "") + (item.name(DESC_PLAIN));
            prompt += "! Ignite poison anyway?";
            return !yesno(prompt.c_str(), false, 'n');
        }
        else if (item.base_type == OBJ_POTIONS && item_type_known(item))
        {
            switch (item.sub_type)
            {
            case POT_DEGENERATION:
            case POT_POISON:
                prompt += "over ";
                prompt += (item.quantity == 1 ? "a " : "") + (item.name(DESC_PLAIN));
                prompt += "! Ignite poison anyway?";
                return !yesno(prompt.c_str(), false, 'n');
            default:
                break;
            }
        }
        else if (item.base_type == OBJ_CORPSES
                 && item.sub_type == CORPSE_BODY
                 && carrion_is_poisonous(item))
        {
            prompt += "over ";
            prompt += (item.quantity == 1 ? "a " : "") + (item.name(DESC_PLAIN));
            prompt += "! Ignite poison anyway?";
            return !yesno(prompt.c_str(), false, 'n');
        }
        else if (item.base_type == OBJ_FOOD &&
                 item.sub_type == FOOD_CHUNK &&
                 carrion_is_poisonous(item))
        {
            prompt += "over ";
            prompt += (item.quantity == 1 ? "a " : "") + (item.name(DESC_PLAIN));
            prompt += "! Ignite poison anyway?";
            return !yesno(prompt.c_str(), false, 'n');
        }
    }

    return false;
}

bool ignite_poison_affects(const actor* act)
{
    if (act->is_player())
        return _player_has_poisonous_physiology() || you.duration[DUR_POISONING];
    else
    {
        return mons_is_poisoner(act->as_monster())
               || act->as_monster()->has_ench(ENCH_POISON);
    }
}

spret_type cast_ignite_poison(actor* agent, int pow, bool fail, bool mon_tracer)
{
    if (agent->is_player())
    {
        if (maybe_abort_ignite())
        {
            canned_msg(MSG_OK);
            return SPRET_ABORT;
        }
        fail_check();
    }
    else if (mon_tracer)
    {
        // Estimate how much useful effect we'd get if we cast the spell now
        int work = 0;
        work += apply_area_visible(_ignite_poison_clouds, -1, agent);
        work += apply_area_visible(_ignite_poison_objects, -1, agent);
        work += apply_area_visible(_ignite_poison_monsters, -1, agent);
        work += apply_area_visible(_ignite_poison_player, -1, agent);

        return work > 0 ? SPRET_SUCCESS : SPRET_ABORT;
    }

    targetter_los hitfunc(agent, LOS_NO_TRANS);
    flash_view_delay(
        agent->is_player()
            ? UA_PLAYER
            : UA_MONSTER,
        RED, 100, &hitfunc);

    mprf("%s %s the poison in %s surroundings!", agent->name(DESC_THE).c_str(),
         agent->conj_verb("ignite").c_str(),
         agent->pronoun(PRONOUN_POSSESSIVE).c_str());

    apply_area_visible(_ignite_poison_clouds, pow, agent);
    apply_area_visible(_ignite_poison_objects, pow, agent);
    apply_area_visible(_ignite_poison_monsters, pow, agent);
    // Only relevant if a monster is casting this spell (never hurts the caster)
    apply_area_visible(_ignite_poison_player, pow, agent);

    return SPRET_SUCCESS;
}

void local_ignite_poison(coord_def pos, int pow, actor* agent)
{
    _ignite_poison_clouds(pos, pow, 0, agent);
    _ignite_poison_objects(pos, pow, 0, agent);
    _ignite_poison_monsters(pos, pow, 0, agent);
    _ignite_poison_player(pos, pow, 0, agent);
}

int discharge_monsters(coord_def where, int pow, int, actor *agent)
{
    actor* victim = actor_at(where);

    if (!victim)
        return 0;

    int damage = (agent == victim) ? 1 + random2(3 + pow / 15)
                                   : 3 + random2(5 + pow / 10
                                                 + (random2(pow) / 10));

    bolt beam;
    beam.flavour = BEAM_ELECTRICITY; // used for mons_adjust_flavoured

    dprf("Static discharge on (%d,%d) pow: %d", where.x, where.y, pow);
    if (victim->is_player())
    {
        mpr("You are struck by lightning.");
        damage = 1 + random2(3 + pow / 15);
        dprf("You: static discharge damage: %d", damage);
        damage = check_your_resists(damage, BEAM_ELECTRICITY,
                                    "static discharge");
        ouch(damage, KILLED_BY_BEAM, agent->mid, "by static electricity", true,
             agent->is_player() ? "you" : agent->name(DESC_A).c_str());
    }
    else if (victim->res_elec() > 0)
    {
        // Shock serpents conduct electricity just fine.
        if (victim->type != MONS_SHOCK_SERPENT)
            return 0;
    }
    else
    {
        monster* mons = victim->as_monster();
        ASSERT(mons);

        dprf("%s: static discharge damage: %d",
             mons->name(DESC_PLAIN, true).c_str(), damage);
        damage = mons_adjust_flavoured(mons, beam, damage);

        if (damage)
        {
            mprf("%s is struck by lightning.",
                 mons->name(DESC_THE).c_str());
            if (agent->is_player())
                _player_hurt_monster(*mons, damage);
            else
                mons->hurt(agent->as_monster(), damage);
        }
    }

    // Recursion to give us chain-lightning -- bwr
    // Low power slight chance added for low power characters -- bwr
    if ((pow >= 10 && !one_chance_in(4)) || (pow >= 3 && one_chance_in(10)))
    {
        mpr("The lightning arcs!");
        pow /= (coinflip() ? 2 : 3);
        damage += apply_random_around_square(discharge_monsters, where,
                                             true, pow, 1, agent);
    }
    else if (damage > 0)
    {
        // Only printed if we did damage, so that the messages in
        // cast_discharge() are clean. -- bwr
        mpr("The lightning grounds out.");
    }

    return damage;
}

static bool _safe_discharge(coord_def where, vector<const monster *> &exclude)
{
    for (adjacent_iterator ai(where); ai; ++ai)
    {
        const monster *mon = monster_at(*ai);
        if (!mon)
            continue;

        if (find(exclude.begin(), exclude.end(), mon) == exclude.end())
        {
            // Harmless to these monsters, so don't prompt about hitting them
            if (mon->res_elec() > 0)
                continue;

            if (stop_attack_prompt(mon, false, where))
                return false;

            exclude.push_back(mon);
            if (!_safe_discharge(mon->pos(), exclude))
                return false;
        }
    }

    return true;
}

spret_type cast_discharge(int pow, bool fail)
{
    fail_check();

    vector<const monster *> exclude;
    if (!_safe_discharge(you.pos(), exclude))
        return SPRET_ABORT;

    const int num_targs = 1 + random2(random_range(1, 3) + pow / 20);
    const int dam = apply_random_around_square(discharge_monsters, you.pos(),
                                               true, pow, num_targs, &you);

    dprf("Arcs: %d Damage: %d", num_targs, dam);

    if (dam == 0)
    {
        if (coinflip())
            mpr("The air around you crackles with electrical energy.");
        else
        {
            const bool plural = coinflip();
            mprf("%s blue arc%s ground%s harmlessly %s you.",
                 plural ? "Some" : "A",
                 plural ? "s" : "",
                 plural ? " themselves" : "s itself",
                 plural ? "around" : (coinflip() ? "beside" :
                                      coinflip() ? "behind" : "before"));
        }
    }
    return SPRET_SUCCESS;
}

bool setup_fragmentation_beam(bolt &beam, int pow, const actor *caster,
                              const coord_def target, bool allow_random,
                              bool get_max_distance, bool quiet,
                              const char **what, bool &should_destroy_wall,
                              bool &hole)
{
    beam.flavour     = BEAM_FRAG;
    beam.glyph       = dchar_glyph(DCHAR_FIRED_BURST);
    beam.source_id   = caster->mid;
    beam.thrower     = caster->is_player() ? KILL_YOU : KILL_MON;
    beam.ex_size     = 1;
    beam.source      = you.pos();
    beam.hit         = AUTOMATIC_HIT;

    beam.source_name = caster->name(DESC_PLAIN, true);
    beam.aux_source = "by Lee's Rapid Deconstruction"; // for direct attack

    beam.target = target;

    // Number of dice vary... 3 is easy/common, but it can get as high as 6.
    beam.damage = dice_def(0, 5 + pow / 5);

    monster* mon = monster_at(target);
    const dungeon_feature_type grid = grd(target);

    if (target == you.pos())
    {
        const bool petrified = (you.petrified() || you.petrifying());

        if (you.form == TRAN_STATUE || you.species == SP_GARGOYLE)
        {
            beam.name       = "blast of rock fragments";
            beam.colour     = BROWN;
            beam.damage.num = you.form == TRAN_STATUE ? 3 : 2;
            return true;
        }
        else if (petrified)
        {
            beam.name       = "blast of petrified fragments";
            beam.colour     = mons_class_colour(player_mons(true));
            beam.damage.num = 3;
            return true;
        }
        else if (you.form == TRAN_ICE_BEAST) // blast of ice
        {
            beam.name       = "icy blast";
            beam.colour     = WHITE;
            beam.damage.num = 3;
            beam.flavour    = BEAM_ICE;
            return true;
        }
    }
    else if (mon && (caster->is_monster() || (you.can_see(mon))))
    {
        switch (mon->type)
        {
        case MONS_TOENAIL_GOLEM:
            beam.damage.num = 3;
            beam.name       = "blast of toenail fragments";
            beam.colour     = RED;
            break;

        case MONS_IRON_ELEMENTAL:
        case MONS_IRON_GOLEM:
        case MONS_WAR_GARGOYLE:
            beam.name       = "blast of metal fragments";
            beam.colour     = CYAN;
            beam.damage.num = 4;
            break;

        case MONS_EARTH_ELEMENTAL:
        case MONS_USHABTI:
        case MONS_STATUE:
        case MONS_GARGOYLE:
            beam.name       = "blast of rock fragments";
            beam.colour     = BROWN;
            beam.damage.num = 3;
            break;

        case MONS_OBSIDIAN_STATUE:
        case MONS_ORANGE_STATUE:
        case MONS_CRYSTAL_GUARDIAN:
        case MONS_ROXANNE:
            beam.ex_size    = 2;
            beam.damage.num = 4;
            if (mon->type == MONS_OBSIDIAN_STATUE)
            {
                beam.name       = "blast of obsidian shards";
                beam.colour     = DARKGREY;
            }
            else if (mon->type == MONS_ORANGE_STATUE)
            {
                beam.name       = "blast of orange crystal shards";
                beam.colour     = LIGHTRED;
            }
            else if (mon->type == MONS_CRYSTAL_GUARDIAN)
            {
                beam.name       = "blast of crystal shards";
                beam.colour     = GREEN;
            }
            else
            {
                beam.name       = "blast of sapphire shards";
                beam.colour     = BLUE;
            }
            break;

        default:
            const bool petrified = (mon->petrified() || mon->petrifying());

            // Petrifying or petrified monsters can be exploded.
            if (petrified)
            {
                monster_info minfo(mon);
                beam.name       = "blast of petrified fragments";
                beam.colour     = minfo.colour();
                beam.damage.num = 3;
                break;
            }
            else if (mon->is_icy()) // blast of ice
            {
                beam.name       = "icy blast";
                beam.colour     = WHITE;
                beam.damage.num = 3;
                beam.flavour    = BEAM_ICE;
                break;
            }
            else if (mon->is_skeletal()) // blast of bone
            {
                beam.name   = "blast of bone shards";
                beam.colour = LIGHTGREY;
                beam.damage.num = 3;
                break;
            }
            // Targeted monster not shatterable, try the terrain instead.
            goto do_terrain;
        }

        beam.aux_source = beam.name;

        // Got a target, let's blow it up.
        return true;
    }

  do_terrain:

    if (env.markers.property_at(target, MAT_ANY,
                                "veto_fragmentation") == "veto")
    {
        if (caster->is_player() && !quiet)
        {
            mprf("%s seems to be unnaturally hard.",
                 feature_description_at(target, false, DESC_THE, false).c_str());
        }
        return false;
    }

    switch (grid)
    {
    // Stone and rock terrain
    case DNGN_ORCISH_IDOL:
        if (!caster->is_player())
            return false; // don't let monsters blow up orcish idols

        if (what && *what == NULL)
            *what = "stone idol";
        // fall-through
    case DNGN_ROCK_WALL:
    case DNGN_SLIMY_WALL:
    case DNGN_STONE_WALL:
    case DNGN_CLEAR_ROCK_WALL:
    case DNGN_CLEAR_STONE_WALL:
        if (what && *what == NULL)
            *what = "wall";
        // fall-through
    case DNGN_GRANITE_STATUE:   // normal rock -- big explosion
        if (what && *what == NULL)
            *what = "statue";

        beam.name       = "blast of rock fragments";
        beam.damage.num = 3;

        if ((grid == DNGN_ORCISH_IDOL
             || grid == DNGN_GRANITE_STATUE
             || grid == DNGN_GRATE
             || pow >= 35 && grid == DNGN_ROCK_WALL
                 && (allow_random && one_chance_in(3)
                     || !allow_random && get_max_distance)
             || pow >= 35 && grid == DNGN_CLEAR_ROCK_WALL
                 && (allow_random && one_chance_in(3)
                     || !allow_random && get_max_distance)
             || pow >= 50 && grid == DNGN_STONE_WALL
                 && (allow_random && one_chance_in(10)
                     || !allow_random && get_max_distance)
             || pow >= 50 && grid == DNGN_CLEAR_STONE_WALL
                 && (allow_random && one_chance_in(10)
                     || !allow_random && get_max_distance)))
        {
            beam.ex_size = 2;
            should_destroy_wall = true;
        }
        break;

    // Metal -- small but nasty explosion
    case DNGN_METAL_WALL:
        if (what)
            *what = "metal wall";
        // fall through
    case DNGN_GRATE:
        if (what && *what == NULL)
            *what = "iron grate";
        beam.name       = "blast of metal fragments";
        beam.damage.num = 4;

        if (pow >= 75 && (allow_random && x_chance_in_y(pow / 5, 500)
                          || !allow_random && get_max_distance)
            || grid == DNGN_GRATE)
        {
            beam.damage.num += 2;
            should_destroy_wall     = true;
        }
        break;

    // Crystal
    case DNGN_GREEN_CRYSTAL_WALL:       // crystal -- large & nasty explosion
        if (what)
            *what = "crystal wall";
        beam.ex_size    = 2;
        beam.name       = "blast of crystal shards";
        beam.damage.num = 4;

        if (allow_random && coinflip()
            || !allow_random && get_max_distance)
        {
            beam.ex_size = 3;
            should_destroy_wall = true;
        }
        break;

    // Stone doors and arches

    case DNGN_OPEN_DOOR:
    case DNGN_CLOSED_DOOR:
    case DNGN_RUNED_DOOR:
    case DNGN_SEALED_DOOR:
        // Doors always blow up, stone arches never do (would cause problems).
        if (what)
            *what = "door";
        should_destroy_wall = true;

        // fall-through
    case DNGN_STONE_ARCH:          // Floor -- small explosion.
        if (what && *what == NULL)
            *what = "stone arch";
        hole            = false;  // to hit monsters standing on doors
        beam.name       = "blast of rock fragments";
        beam.damage.num = 3;
        break;

    default:
        // Couldn't find a monster or wall to shatter - abort casting!
        if (caster->is_player() && !quiet)
            mpr("You can't deconstruct that!");
        return false;
    }

    // If it was recoloured, use that colour instead.
    if (env.grid_colours(target))
        beam.colour = env.grid_colours(target);
    else
    {
        beam.colour = element_colour(get_feature_def(grid).colour,
                                     false, target);
    }

    beam.aux_source = beam.name;

    return true;
}

spret_type cast_fragmentation(int pow, const actor *caster,
                              const coord_def target, bool fail)
{
    bool should_destroy_wall = false;
    bool hole                = true;
    const char *what         = NULL;
    const dungeon_feature_type grid = grd(target);

    bolt beam;

    // should_destroy_wall is an output argument.
    if (!setup_fragmentation_beam(beam, pow, caster, target, true, false,
                                  false, &what, should_destroy_wall, hole))
    {
        return SPRET_ABORT;
    }

    if (caster->is_player())
    {
        if (grid == DNGN_ORCISH_IDOL)
        {
            if (!yesno("Really insult Beogh by defacing this idol?",
                       false, 'n'))
            {
                canned_msg(MSG_OK);
                return SPRET_ABORT;
            }
        }

        bolt tempbeam;
        bool temp;
        setup_fragmentation_beam(tempbeam, pow, caster, target, false, true,
                                 true, NULL, temp, temp);
        tempbeam.is_tracer = true;
        tempbeam.explode(false);
        if (tempbeam.beam_cancelled)
        {
            canned_msg(MSG_OK);
            return SPRET_ABORT;
        }
    }

    monster* mon = monster_at(target);

    fail_check();

    if (what != NULL) // Terrain explodes.
    {
        if (you.see_cell(target))
            mprf("The %s shatters!", what);
        if (should_destroy_wall)
            destroy_wall(target);
    }
    else if (target == you.pos()) // You explode.
    {
        mpr("You shatter!");

        ouch(beam.damage.roll(), KILLED_BY_BEAM, caster->mid,
             "by Lee's Rapid Deconstruction", true,
             caster->is_player() ? "you"
                                 : caster->name(DESC_A).c_str());
    }
    else // Monster explodes.
    {
        if (you.see_cell(target))
            mprf("%s shatters!", mon->name(DESC_THE).c_str());

        if (caster->is_player())
        {
            if (_player_hurt_monster(*mon, beam.damage.roll(),
                                     BEAM_DISINTEGRATION))
            {
                beam.damage.num += 2;
            }
        }
        else
        {
            mon->hurt(caster, beam.damage.roll(), BEAM_DISINTEGRATION);
            if (!mon->alive())
                beam.damage.num += 2;
        }
    }

    beam.explode(true, hole);

    // Monsters shouldn't be able to blow up idols,
    // but this check is here just in case...
    if (caster->is_player() && grid == DNGN_ORCISH_IDOL)
        did_god_conduct(DID_DESTROY_ORCISH_IDOL, 8);

    return SPRET_SUCCESS;
}

int wielding_rocks()
{
    const item_def* wpn = you.weapon();
    if (!wpn || wpn->base_type != OBJ_MISSILES)
        return 0;
    else if (wpn->sub_type == MI_STONE)
        return 1;
    else if (wpn->sub_type == MI_LARGE_ROCK)
        return 2;
    else
        return 0;
}

spret_type cast_sandblast(int pow, bolt &beam, bool fail)
{
    zap_type zap = ZAP_SMALL_SANDBLAST;
    switch (wielding_rocks())
    {
    case 1:
        zap = ZAP_SANDBLAST;
        break;
    case 2:
        zap = ZAP_LARGE_SANDBLAST;
        break;
    default:
        break;
    }

    const spret_type ret = zapping(zap, pow, beam, true, NULL, fail);

    if (ret == SPRET_SUCCESS && zap != ZAP_SMALL_SANDBLAST)
        dec_inv_item_quantity(you.equip[EQ_WEAPON], 1);

    return ret;
}

static bool _elec_not_immune(const actor *act)
{
    return act->res_elec() < 3;
}

spret_type cast_thunderbolt(actor *caster, int pow, coord_def aim, bool fail)
{
    coord_def prev;
    if (caster->props.exists("thunderbolt_last")
        && caster->props["thunderbolt_last"].get_int() + 1 == you.num_turns)
    {
        prev = caster->props["thunderbolt_aim"].get_coord();
    }

    targetter_thunderbolt hitfunc(caster, spell_range(SPELL_THUNDERBOLT, pow),
                                  prev);
    hitfunc.set_aim(aim);

    if (caster->is_player())
    {
        if (stop_attack_prompt(hitfunc, "zap", _elec_not_immune))
            return SPRET_ABORT;
    }

    fail_check();

    int juice = prev.origin() ? 2 * ROD_CHARGE_MULT
                              : caster->props["thunderbolt_mana"].get_int();
    bolt beam;
    beam.name              = "lightning";
    beam.aux_source        = "rod of lightning";
    beam.flavour           = BEAM_ELECTRICITY;
    beam.glyph             = dchar_glyph(DCHAR_FIRED_BURST);
    beam.colour            = LIGHTCYAN;
    beam.range             = 1;
    // Dodging a horizontal arc is nearly impossible: you'd have to fall prone
    // or jump high.
    beam.hit               = prev.origin() ? 10 + pow / 20 : 1000;
    beam.ac_rule           = AC_PROPORTIONAL;
    beam.set_agent(caster);
#ifdef USE_TILE
    beam.tile_beam = -1;
#endif
    beam.draw_delay = 0;

    for (const auto &entry : hitfunc.zapped)
    {
        if (entry.second <= 0)
            continue;

        beam.draw(entry.first);
    }

    scaled_delay(200);

    beam.glyph = 0; // FIXME: a hack to avoid "appears out of thin air"

    for (const auto &entry : hitfunc.zapped)
    {
        if (entry.second <= 0)
            continue;

        // beams are incredibly spammy in debug mode
        if (!actor_at(entry.first))
            continue;

        int arc = hitfunc.arc_length[entry.first.range(hitfunc.origin)];
        ASSERT(arc > 0);
        dprf("at distance %d, arc length is %d",
             entry.first.range(hitfunc.origin), arc);
        beam.source = beam.target = entry.first;
        beam.source.x -= sgn(beam.source.x - hitfunc.origin.x);
        beam.source.y -= sgn(beam.source.y - hitfunc.origin.y);
        beam.damage = dice_def(div_rand_round(juice, ROD_CHARGE_MULT),
                               div_rand_round(30 + pow / 6, arc + 2));
        beam.fire();
    }

    caster->props["thunderbolt_last"].get_int() = you.num_turns;
    caster->props["thunderbolt_aim"].get_coord() = aim;

    noisy(15 + div_rand_round(juice, ROD_CHARGE_MULT), hitfunc.origin);

    return SPRET_SUCCESS;
}

// Find an enemy who would suffer from Awaken Forest.
actor* forest_near_enemy(const actor *mon)
{
    const coord_def pos = mon->pos();

    for (radius_iterator ri(pos, LOS_NO_TRANS); ri; ++ri)
    {
        actor* foe = actor_at(*ri);
        if (!foe || mons_aligned(foe, mon))
            continue;

        for (adjacent_iterator ai(*ri); ai; ++ai)
            if (feat_is_tree(grd(*ai)) && cell_see_cell(pos, *ai, LOS_DEFAULT))
                return foe;
    }

    return NULL;
}

// Print a message only if you can see any affected trees.
void forest_message(const coord_def pos, const string &msg, msg_channel_type ch)
{
    for (radius_iterator ri(pos, LOS_DEFAULT); ri; ++ri)
        if (feat_is_tree(grd(*ri))
            && cell_see_cell(you.pos(), *ri, LOS_DEFAULT))
        {
            mprf(ch, "%s", msg.c_str());
            return;
        }
}

void forest_damage(const actor *mon)
{
    const coord_def pos = mon->pos();
    const int hd = mon->get_hit_dice();

    if (one_chance_in(4))
    {
        forest_message(pos, random_choose(
            "The trees move their gnarly branches around.",
            "You feel roots moving beneath the ground.",
            "Branches wave dangerously above you.",
            "Trunks creak and shift.",
            "Tree limbs sway around you.",
            0), MSGCH_TALK_VISUAL);
    }

    for (radius_iterator ri(pos, LOS_NO_TRANS); ri; ++ri)
    {
        actor* foe = actor_at(*ri);
        if (!foe || mons_aligned(foe, mon))
            continue;

        if (is_sanctuary(foe->pos()))
            continue;

        for (adjacent_iterator ai(*ri); ai; ++ai)
            if (feat_is_tree(grd(*ai)) && cell_see_cell(pos, *ai, LOS_NO_TRANS))
            {
                int evnp = foe->melee_evasion(mon, EV_IGNORE_PHASESHIFT);
                int dmg = 0;
                string msg;

                if (!apply_chunked_AC(1, evnp))
                {
                    msg = random_choose(
                            "@foe@ @is@ waved at by a branch.",
                            "A tree reaches out but misses @foe@.",
                            "A root lunges up near @foe@.",
                            0);
                }
                else if (!apply_chunked_AC(1, foe->melee_evasion(mon) - evnp))
                {
                    msg = random_choose(
                            "A branch passes through @foe@!",
                            "A tree reaches out and and passes through @foe@!",
                            "A root lunges and passes through @foe@ from below.",
                            0);
                }
                else if (!(dmg = foe->apply_ac(hd + random2(hd), hd * 2 - 1,
                                               AC_PROPORTIONAL)))
                {
                    msg = random_choose(
                            "@foe@ @is@ scraped by a branch!",
                            "A tree reaches out and scrapes @foe@!",
                            "A root barely touches @foe@ from below.",
                            0);
                }
                else
                {
                    msg = random_choose(
                        "@foe@ @is@ hit by a branch!",
                        "A tree reaches out and hits @foe@!",
                        "A root smacks @foe@ from below.",
                        0);
                }

                msg = replace_all(replace_all(msg,
                    "@foe@", foe->name(DESC_THE)),
                    "@is@", foe->conj_verb("be"));
                if (you.see_cell(foe->pos()))
                    mpr(msg);

                if (dmg <= 0)
                    break;

                if (foe->is_player())
                {
                    ouch(dmg, KILLED_BY_BEAM, mon->mid,
                         "by angry trees", true);
                }
                else
                    foe->hurt(mon, dmg);

                break;
            }
    }
}

vector<bolt> get_spray_rays(const actor *caster, coord_def aim, int range,
                            int max_rays, int max_spacing)
{
    coord_def aim_dir = (caster->pos() - aim).sgn();

    int num_targets = 0;
    vector<bolt> beams;
    int range2 = dist_range(range);

    bolt base_beam;

    base_beam.set_agent(const_cast<actor *>(caster));
    base_beam.attitude = caster->is_player() ? ATT_FRIENDLY
                                             : caster->as_monster()->attitude;
    base_beam.is_tracer = true;
    base_beam.is_targeting = true;
    base_beam.dont_stop_player = true;
    base_beam.friend_info.dont_stop = true;
    base_beam.foe_info.dont_stop = true;
    base_beam.range = range;
    base_beam.source = caster->pos();
    base_beam.target = aim;

    bolt center_beam = base_beam;
    center_beam.hit = AUTOMATIC_HIT;
    center_beam.fire();
    center_beam.target = center_beam.path_taken.back();
    center_beam.hit = 1;
    center_beam.fire();
    center_beam.is_tracer = false;
    center_beam.dont_stop_player = false;
    center_beam.foe_info.dont_stop = false;
    center_beam.friend_info.dont_stop = false;
    beams.push_back(center_beam);

    for (distance_iterator di(aim, false, false, max_spacing); di; ++di)
    {
        if (monster_at(*di))
        {
            coord_def delta = caster->pos() - *di;

            //Don't aim secondary rays at friendlies
            if (mons_aligned(caster, monster_at(*di)))
                continue;

            if (!caster->can_see(monster_at(*di)))
                continue;

            //Don't try to aim at a target if it's out of range
            if (delta.abs() > range2)
                continue;

            //Don't try to aim at targets in the opposite direction of main aim
            if (abs(aim_dir.x - delta.sgn().x) + abs(aim_dir.y - delta.sgn().y) >= 2)
                continue;

            //Test if this beam stops at a location used by any prior beam
            bolt testbeam = base_beam;
            testbeam.target = *di;
            testbeam.hit = AUTOMATIC_HIT;
            testbeam.fire();
            bool duplicate = false;

            for (unsigned int i = 0; i < beams.size(); ++i)
            {
                if (testbeam.path_taken.back() == beams[i].target)
                {
                    duplicate = true;
                    continue;
                }
            }
            if (!duplicate)
            {
                bolt tempbeam = base_beam;
                tempbeam.target = testbeam.path_taken.back();
                tempbeam.fire();
                tempbeam.is_tracer = false;
                tempbeam.is_targeting = false;
                tempbeam.dont_stop_player = false;
                tempbeam.foe_info.dont_stop = false;
                tempbeam.friend_info.dont_stop = false;
                beams.push_back(tempbeam);
                num_targets++;
            }

            if (num_targets == max_rays - 1)
              break;
        }
    }

    return beams;
}

static bool _dazzle_can_hit(const actor *act)
{
    if (act->is_monster())
    {
        const monster* mons = act->as_monster();
        bolt testbeam;
        testbeam.thrower = KILL_YOU;
        zappy(ZAP_DAZZLING_SPRAY, 100, testbeam);

        return !mons_is_avatar(mons->type)
               && mons_species(mons->type) != MONS_BUSH
               && !fedhas_shoot_through(testbeam, mons);
    }
    else
        return false;
}

spret_type cast_dazzling_spray(int pow, coord_def aim, bool fail)
{
    int range = spell_range(SPELL_DAZZLING_SPRAY, pow);

    targetter_spray hitfunc(&you, range, ZAP_DAZZLING_SPRAY);
    hitfunc.set_aim(aim);
    if (stop_attack_prompt(hitfunc, "fire towards", _dazzle_can_hit))
        return SPRET_ABORT;

    fail_check();

    if (hitfunc.beams.size() == 0)
    {
        mpr("You can't see any targets in that direction!");
        return SPRET_ABORT;
    }

    for (unsigned int i = 0; i < hitfunc.beams.size(); ++i)
    {
        zappy(ZAP_DAZZLING_SPRAY, pow, hitfunc.beams[i]);
        hitfunc.beams[i].fire();
    }

    return SPRET_SUCCESS;
}

static bool _toxic_can_affect(const actor *act)
{
    if (act->is_monster() && act->as_monster()->submerged())
        return false;

    // currently monsters are still immune at rPois 1
    return act->res_poison() < (act->is_player() ? 3 : 1);
}

spret_type cast_toxic_radiance(actor *agent, int pow, bool fail, bool mon_tracer)
{
    if (agent->is_player())
    {
        targetter_los hitfunc(&you, LOS_NO_TRANS);
        {
            if (stop_attack_prompt(hitfunc, "poison", _toxic_can_affect))
                return SPRET_ABORT;
        }
        fail_check();

        if (!you.duration[DUR_TOXIC_RADIANCE])
            mpr("You begin to radiate toxic energy.");
        else
            mpr("Your toxic radiance grows in intensity.");

        you.increase_duration(DUR_TOXIC_RADIANCE, 3 + random2(pow/20), 15);

        flash_view_delay(UA_PLAYER, GREEN, 300, &hitfunc);

        return SPRET_SUCCESS;
    }
    else if (mon_tracer)
    {
        for (actor_near_iterator ai(agent, LOS_NO_TRANS); ai; ++ai)
        {
            if (!_toxic_can_affect(*ai) || mons_aligned(agent, *ai))
                continue;
            else
                return SPRET_SUCCESS;
        }

        // Didn't find any susceptible targets
        return SPRET_ABORT;
    }
    else
    {
        monster* mon_agent = agent->as_monster();
        simple_monster_message(mon_agent,
                               " begins to radiate toxic energy.");

        mon_agent->add_ench(mon_enchant(ENCH_TOXIC_RADIANCE, 1, mon_agent,
                                        (5 + random2avg(pow/15, 2)) * BASELINE_DELAY));

        targetter_los hitfunc(mon_agent, LOS_NO_TRANS);
        flash_view_delay(UA_MONSTER, GREEN, 300, &hitfunc);

        return SPRET_SUCCESS;
    }
}

void toxic_radiance_effect(actor* agent, int mult)
{
    int pow;
    if (agent->is_player())
        pow = calc_spell_power(SPELL_OLGREBS_TOXIC_RADIANCE, true);
    else
        pow = agent->as_monster()->get_hit_dice() * 8;

    bool break_sanctuary = (agent->is_player() && is_sanctuary(you.pos()));

    for (actor_near_iterator ai(agent->pos(), LOS_NO_TRANS); ai; ++ai)
    {
        if (!_toxic_can_affect(*ai))
            continue;

        // Monsters can skip hurting friendlies
        if (agent->is_monster() && mons_aligned(agent, *ai))
            continue;

        int dam = roll_dice(1, 3 + pow / 25) * mult;

        // Only applied if the player is also not the agent; Done early so that
        // distance falloff won't frequently reduce damage to 1.
        if (ai->is_player())
            dam = dam * 5 / 2;

        // Give monster OTR a weaker distance falloff than player OTR
        if (agent->is_player())
            dam = dam * 3 / (2 + ai->pos().distance_from(agent->pos()));
        else
            dam = dam * 9 / (8 + ai->pos().distance_from(agent->pos()));

        dam = resist_adjust_damage(*ai, BEAM_POISON, ai->res_poison(),
                                   dam, true);

        if (ai->is_player())
        {
            // We take direct damage only if we're not the agent, but we
            // still get poisoned
            if (!agent->is_player())
            {
                ouch(dam, KILLED_BY_BEAM, agent->mid,
                    "by Olgreb's Toxic Radiance", true,
                    agent->as_monster()->name(DESC_A).c_str());

                poison_player(dam * 4 / 3, agent->name(DESC_A), "toxic radiance",
                              false);
            }
            else
            {
                poison_player(roll_dice(2, 3), agent->name(DESC_A),
                              "toxic radiance", true);
            }
        }
        else
        {
            behaviour_event(ai->as_monster(), ME_ANNOY, agent, agent->pos());
            ai->hurt(agent, dam, BEAM_POISON);
            if (coinflip() || !ai->as_monster()->has_ench(ENCH_POISON))
                poison_monster(ai->as_monster(), agent, 1);

            if (agent->is_player() && is_sanctuary(ai->pos()))
                break_sanctuary = true;
        }
    }

    if (break_sanctuary)
        remove_sanctuary(true);
}

spret_type cast_searing_ray(int pow, bolt &beam, bool fail)
{
    const spret_type ret = zapping(ZAP_SEARING_RAY_I, pow, beam, true, NULL, fail);

    if (ret == SPRET_SUCCESS)
    {
        // Special value, used to avoid terminating ray immediately, since we
        // took a non-wait action on this turn (ie: casting it)
        you.attribute[ATTR_SEARING_RAY] = -1;
        you.props["searing_ray_target"].get_coord() = beam.target;
        you.props["searing_ray_aimed_at_spot"].get_bool() = beam.aimed_at_spot;
    }

    return ret;
}

void handle_searing_ray()
{
    ASSERT_RANGE(you.attribute[ATTR_SEARING_RAY], 1, 4);

    // All of these effects interrupt a channeled ray
    if (you.confused() || you.berserk())
    {
        end_searing_ray();
        return;
    }

    if (!enough_mp(1, true))
    {
        mpr("Without enough magic to sustain it, your searing ray dissipates.");
        end_searing_ray();
        return;
    }

    const zap_type zap = zap_type(ZAP_SEARING_RAY_I + (you.attribute[ATTR_SEARING_RAY]-1));
    const int pow = calc_spell_power(SPELL_SEARING_RAY, true);

    bolt beam;
    beam.thrower = KILL_YOU_MISSILE;
    beam.range   = calc_spell_range(SPELL_SEARING_RAY, pow);
    beam.source  = you.pos();
    beam.target  = you.props["searing_ray_target"].get_coord();
    beam.aimed_at_spot = you.props["searing_ray_aimed_at_spot"].get_bool();

    // If friendlies have moved into the beam path, give a chance to abort
    if (!player_tracer(zap, pow, beam))
    {
        mpr("You stop channeling your searing ray.");
        end_searing_ray();
        return;
    }

    zappy(zap, pow, beam);

    aim_battlesphere(&you, SPELL_SEARING_RAY, pow, beam);
    beam.fire();
    trigger_battlesphere(&you, beam);

    dec_mp(1);

    if (++you.attribute[ATTR_SEARING_RAY] > 3)
        end_searing_ray();
}

void end_searing_ray()
{
    you.attribute[ATTR_SEARING_RAY] = 0;
    you.props.erase("searing_ray_target");
    you.props.erase("searing_ray_aimed_at_spot");
}

spret_type cast_glaciate(actor *caster, int pow, coord_def aim, bool fail)
{
    const int range = spell_range(SPELL_GLACIATE, pow);
    targetter_cone hitfunc(caster, range);
    hitfunc.set_aim(aim);

    if (caster->is_player())
    {
        if (stop_attack_prompt(hitfunc, "glaciate"))
            return SPRET_ABORT;
    }

    fail_check();

    bolt beam;
    beam.name              = "great icy blast";
    beam.aux_source        = "great icy blast";
    beam.flavour           = BEAM_ICE;
    beam.glyph             = dchar_glyph(DCHAR_EXPLOSION);
    beam.colour            = WHITE;
    beam.range             = 1;
    beam.hit               = AUTOMATIC_HIT;
    beam.source_id         = caster->mid;
    beam.hit_verb          = "engulfs";
    beam.origin_spell      = SPELL_GLACIATE;
    beam.set_agent(caster);
#ifdef USE_TILE
    beam.tile_beam = -1;
#endif
    beam.draw_delay = 0;

    for (int i = 1; i <= range; i++)
    {
        for (const auto &entry : hitfunc.sweep[i])
        {
            if (entry.second <= 0)
                continue;

            beam.draw(entry.first);
        }
        scaled_delay(25);
    }

    scaled_delay(100);

    if (you.can_see(caster) || caster->is_player())
    {
        mprf("%s %s a mighty blast of ice!",
             caster->name(DESC_THE).c_str(),
             caster->conj_verb("conjure").c_str());
    }

    beam.glyph = 0;

    for (int i = 1; i <= range; i++)
    {
        for (const auto &entry : hitfunc.sweep[i])
        {
            if (entry.second <= 0)
                continue;

            const int eff_range = max(3, (6 * i / LOS_RADIUS));

            // At or within range 3, this is equivalent to the old Ice Storm
            // damage.
            beam.damage =
                caster->is_player()
                    ? calc_dice(7, (66 + 3 * pow) / eff_range)
                    : calc_dice(10, (54 + 3 * pow / 2) / eff_range);

            if (actor_at(entry.first))
            {
                beam.source = beam.target = entry.first;
                beam.source.x -= sgn(beam.source.x - hitfunc.origin.x);
                beam.source.y -= sgn(beam.source.y - hitfunc.origin.y);
                beam.fire();
            }
            place_cloud(CLOUD_COLD, entry.first,
                        (18 + random2avg(45,2)) / eff_range, caster);
        }
    }

    noisy(spell_effect_noise(SPELL_GLACIATE), hitfunc.origin);

    return SPRET_SUCCESS;
}

spret_type cast_random_bolt(int pow, bolt& beam, bool fail)
{
    // Need to use a 'generic' tracer regardless of the actual beam type,
    // to account for the possibility of both bouncing and irresistable damage
    // (even though only one of these two ever occurs on the same bolt type).
    bolt tracer = beam;
    if (!player_tracer(ZAP_RANDOM_BOLT_TRACER, 200, tracer))
        return SPRET_ABORT;

    fail_check();

    zap_type zap = random_choose(ZAP_BOLT_OF_FIRE,
                                 ZAP_BOLT_OF_COLD,
                                 ZAP_VENOM_BOLT,
                                 ZAP_BOLT_OF_DRAINING,
                                 ZAP_QUICKSILVER_BOLT,
                                 ZAP_CRYSTAL_BOLT,
                                 ZAP_LIGHTNING_BOLT,
                                 ZAP_CORROSIVE_BOLT,
                                 -1);
    zapping(zap, pow * 7 / 6 + 15, beam, false);

    return SPRET_SUCCESS;
}
