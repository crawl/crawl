/**
 * @file
 * @brief Spell miscast functions.
**/

#include "AppHdr.h"

#include "spl-miscast.h"

#include "attack.h"
#include "beam-type.h"
#include "fight.h"
#include "god-passive.h"
#include "message.h"
#include "mgen-data.h"
#include "monster.h"
#include "monster-type.h"
#include "mon-death.h"
#include "mon-place.h"
#include "religion.h"
#include "shout.h"
#include "spl-goditem.h"
#include "stringutil.h"
#include "xom.h"

struct miscast_datum
{
    beam_type flavour;
    vector<string> player_messages;
    vector<string> monster_seen_messages;
    vector<string> monster_unseen_messages;
    function<void (actor& target, actor* source, miscast_source_info mc_info,
                   int dam, string cause)> special;
};

static void _do_msg(actor& target, miscast_datum effect, int dam)
{
    if (!you.see_cell(target.pos()))
        return;
    string msg;

    if (target.is_player())
        msg = *random_iterator(effect.player_messages);
    else if (you.can_see(target))
        msg = *random_iterator(effect.monster_seen_messages);
    else
        msg = *random_iterator(effect.monster_unseen_messages);

    bool plural;

    msg = replace_all(msg, "@hand@",  target.hand_name(false, &plural));
    msg = replace_all(msg, "@hands@", target.hand_name(true));

    if (plural)
        msg = replace_all(msg, "@hand_conj@", "");
    else
        msg = replace_all(msg, "@hand_conj@", "s");

    if (target.is_monster())
    {
        msg = do_mon_str_replacements(msg, *target.as_monster(), S_SILENT);
        if (!mons_has_body(*target.as_monster()))
            msg = replace_all(msg, "'s body", "");
    }

    mpr(msg + attack_strength_punctuation(dam));
}

static void _ouch(actor& target, actor * source, miscast_source_info mc_info, int dam, beam_type flavour, string cause)
{
    bolt beem;

    beem.flavour = flavour;
    beem.source_name = cause;
    beem.damage = dice_def(1, dam);

    if (target.is_monster())
    {
        killer_type kt = KILL_NONE;

        if (source && source->is_player())
            kt           = KILL_YOU_MISSILE;
        else if (source && source->is_monster())
        {
            if (source->as_monster()->confused_by_you()
                && !source->as_monster()->friendly())
            {
                kt = KILL_YOU_CONF;
            }
            else
                kt = KILL_MON_MISSILE;
        }
        else
        {
            ASSERT(mc_info.source != miscast_source::melee);
            kt = KILL_MISCAST;
        }

        monster* mon_target = target.as_monster();

        dam = mons_adjust_flavoured(mon_target, beem, dam, true);
        // TODO: is attribution correct if the monster is killed in
        // mons_adjust_flavoured? A possible case is when flavour is
        // BEAM_NEG
        if (mon_target->alive())
        {
            mon_target->hurt(source, dam, beem.flavour, KILLED_BY_BEAM,
                             "", "", false);

            if (!mon_target->alive())
                monster_die(*mon_target, kt, actor_to_death_source(source));
        }
    }
    else
    {
        dam = check_your_resists(dam, flavour, cause, &beem);

        kill_method_type method;

        if (mc_info.source == miscast_source::spell)
            method = KILLED_BY_WILD_MAGIC;
        else if (mc_info.source == miscast_source::god)
            method = KILLED_BY_DIVINE_WRATH;
        else if (mc_info.source == miscast_source::melee
                 && source && source->is_monster())
        {
            method = KILLED_BY_MONSTER;
        }
        else
            method = KILLED_BY_SOMETHING;

        bool see_source = source && you.can_see(*source);
        ouch(dam, method, source ? source->mid : MID_NOBODY,
             cause.c_str(), see_source,
             source ? source->name(DESC_A, true).c_str() : nullptr);
    }
}

static const map<spschool, miscast_datum> miscast_effects = {
    {
        spschool::conjuration,
        {
            BEAM_MMISSILE,
            {
                "Sparks fly from your @hands@",
                "The air around you crackles with energy",
                "You feel a strange surge of energy",
                "Strange energies run through your body",
                "A wave of violent energy washes through your body",
                "Energy rips through your body",
                "You are caught in a violent explosion",
                "You are blasted with magical energy",
            },
            {
                "Sparks fly from @the_monster@'s @hands@",
                "The air around @the_monster@ crackles with energy",
                "@The_monster@ emits a flash of light",
                "@The_monster@ lurches violently",
                "@The_monster@ jerks violently",
                "@The_monster@ is caught in a violent explosion",
                "@The_monster@ is blasted with magical energy",
            },
            {
                "A violent explosion happens from out of thin air",
                "There is a sudden explosion of magical energy",
            },
            nullptr
        }
    },
    {
        spschool::hexes,
        {
            BEAM_NONE,
            {
                "You feel off-balance for a moment",
                "Multicoloured lights dance before your eyes",
                "You feel a bit tired",
                "The light around you dims momentarily",
                "Strange energies run through your body",
                "You hear an indistinct dissonance whispering inside your mind",
                "The air around you crackles with energy",
                "You feel a strange surge of energy",
                "Waves of light ripple over your body",
                "You feel enfeebled",
                "Magic surges out from your body",
            },
            {
                "@The_monster@ looks off-balance for a moment",
                "Multicoloured lights dance around @the_monster@",
                "@The_monster@ looks sleepy for a second",
                "The light around @the_monster@ dims momentarily",
                "@The_monster@ twitches",
                "@The_monster@'s body glows momentarily",
                "The air around @the_monster@ crackles with energy",
                "Waves of light ripple over @the_monster@'s body",
                "Magic surges out from @the_monster@",
            },
            {
                "Multicoloured lights dance in the air",
                "A patch of light dims momentarily",
                "Magic surges out from thin air",
            },
            [] (actor& target, actor* source, miscast_source_info /*mc_info*/,
                int dam, string /*cause*/) {
                if (target.is_player())
                    debuff_player();
                else
                    debuff_monster(*target.as_monster());

                target.slow_down(source, dam);
            }
        },
    },
    {
        spschool::summoning,
        {
            BEAM_NONE,
            {
                "A fistful of eyes flickers past your view",
                "Distant voices call out in your mind",
                "Something forms from out of thin air",
                "A chorus of moans calls out in your mind",
                "Desperate hands claw out at you",
            },
            {
                "Desperate hands claw out at @the_monster@",
            },
            {
                "Desperate hands claw out at random",
            },
            [] (actor& target, actor* source,
                miscast_source_info mc_info,
                int dam, string cause) {

                mgen_data data = mgen_data::hostile_at(MONS_NAMELESS, true,
                                                       target.pos());
                data.extra_flags |= (MF_NO_REWARD | MF_HARD_RESET);

                // only give durable summons for true miscasts
                int abj_dur = mc_info.source == miscast_source::spell ? 0 : 4;
                data.set_summoned(source, abj_dur, SPELL_NO_SPELL, mc_info.god);
                data.set_non_actor_summoner(cause);
                if (abj_dur > 0)
                    data.summon_type = MON_SUMM_MISCAST;

                data.foe = target.mindex();
                data.hd = min(27, max(div_rand_round(2 * dam, 3), 1));

                if (target.is_monster())
                {
                    monster* mon_target = target.as_monster();

                    switch (mon_target->temp_attitude())
                    {
                        case ATT_FRIENDLY:
                            data.behaviour = BEH_HOSTILE;
                            break;
                        case ATT_HOSTILE:
                            data.behaviour = BEH_FRIENDLY;
                            break;
                        case ATT_GOOD_NEUTRAL:
                        case ATT_NEUTRAL:
                        case ATT_STRICT_NEUTRAL:
                            data.behaviour = BEH_NEUTRAL;
                        break;
                    }
                }

                const monster* mons = create_monster(data, false);

                if (!mons)
                    canned_msg(MSG_NOTHING_HAPPENS);

                return;
            },
        },
    },
    {
        spschool::necromancy,
        {
            BEAM_NEG,
            {
                "You hear strange and distant voices",
                "The world around you seems to dim momentarily",
                "Strange energies run through your body",
                "You shiver with cold",
                "You sense a malignant aura",
                "You feel very uncomfortable",
                "Pain shoots through your body",
                "You feel horribly lethargic",
                "Your flesh rots away",
                "Flickering shadows surround you",
                "You are engulfed in negative energy",
                "You convulse helplessly as pain tears through your body",
            },
            {
                "@The_monster@ twitches violently",
                "@The_monster@ pauses, visibly distraught",
                "@The_monster@ seems to dim momentarily",
                "@The_monster@ shivers with cold",
                "@The_monster@ is briefly tinged with black",
                "@The_monster@ scowls horribly",
                "@The_monster@ convulses with pain",
                "@The_monster@ looks incredibly listless",
                "@The_monster@ rots away",
                "Flickering shadows surround @the_monster@",
                "@The_monster@ is engulfed in negative energy",
                "@The_monster@ convulses helplessly with pain",
                "@The_monster@ seems frightened for a moment",
            },
            {
                "The air has a black tinge for a moment",
                "Shadows flicker in the thin air",
            },
            nullptr,
        },
    },
    {
        spschool::translocation,
        {
            BEAM_NONE,
            {
                "You catch a glimpse of the back of your own head",
                "The air around you crackles with energy",
                "You feel a wrenching sensation",
                "The floor and the ceiling briefly switch places",
                "You spin around",
                "Strange energies run through your body",
                "The world appears momentarily distorted",
                "You are caught in a field of spatial distortion",
                "Space bends around you",
                "Space twists in upon itself",
                "Space warps crazily around you",
            },
            {
                "The air around @the_monster@ crackles with energy",
                "@The_monster@ jerks violently for a moment",
                "@The_monster@ spins around",
                "@The_monster@ appears momentarily distorted",
                "@The_monster@ scowls",
                "@The_monster@ is caught in a field of spatial distortion",
                "Space bends around @the_monster@",
                "Space warps around @the_monster@",
                "Space warps crazily around @the_monster@",
            },
            {
                "The air around something crackles with energy",
                "A piece of empty space twists and distorts",
                "A rift temporarily opens in the fabric of space",
            },
            [] (actor& target, actor* source, miscast_source_info /*mc_info*/,
                int dam, string /*cause*/) {

                if (target.is_player())
                    you.increase_duration(DUR_DIMENSION_ANCHOR, dam, dam);
                else
                {
                     target.as_monster()->add_ench(
                         mon_enchant(ENCH_DIMENSION_ANCHOR,
                                     0, source, dam * BASELINE_DELAY));
                }
            }

        },
    },
    {
        spschool::transmutation,
        {
            BEAM_NONE,
            {
                "Multicoloured lights dance before your eyes",
                "You feel a strange surge of energy",
                "Strange energies run through your body",
                "Your body is twisted painfully",
                "Your limbs ache and wobble like jelly",
                "Your body is flooded with distortional energies",
                "You feel very strange",
            },
            {
                "The air around @the_monster@ crackles with energy",
                "Multicoloured lights dance around @the_monster@",
                "There is a strange surge of energy around @the_monster@",
                "Waves of light ripple over @the_monster@'s body",
                "@The_monster@ twitches",
                "@The_monster@'s body glows momentarily",
                "@The_monster@'s body twists unnaturally",
                "@The_monster@'s body twists and writhes",
                "@The_monster@'s body is flooded with distortional energies",
            },
            {
                "The thin air crackles with energy",
                "Multicoloured lights dance in the air",
                "Waves of light ripple in the air",
            },
            [] (actor& target, actor* /*source*/,
                miscast_source_info /*mc_info*/, int dam, string cause) {

                // Double existing contamination, plus more from the damage
                // roll
                if (target.is_player())
                {
                    contaminate_player(you.magic_contamination
                                       + dam * MISCAST_DIVISOR, true);
                }
                else
                    target.malmutate(cause);
            }

        },
    },
    {
        spschool::fire,
        {
            BEAM_FIRE,
            {
                "Sparks fly from your @hands@",
                "The air around you burns with energy",
                "Heat runs through your body",
                "You feel uncomfortably hot",
                "Fire whirls out from your @hands@",
                "Flames sear your flesh",
                "You are blasted with fire",
                "You are caught in a fiery explosion",
                "You are blasted with searing flames",
                "There is a sudden and violent explosion of flames",
            },
            {
                "Sparks fly from @the_monster@'s @hands@",
                "The air around @the_monster@ burns with energy",
                "Dim flames ripple over @the_monster@'s body",
                "Fire whirls out @the_monster@'s @hands@",
                "Flames sear @the_monster@",
                "@The_monster@ is blasted with fire",
                "@The_monster@ is caught in a fiery explosion",
                "@The_monster@ is blasted with searing flames",
            },
            {
                "Fire whirls out of nowhere",
                "A flame briefly burns in thin air",
                "Fire explodes from out of thin air",
                "A large flame burns hotly for a moment in the thin air",
            },
            nullptr,
        },
    },
    {
        spschool::ice,
        {
            BEAM_COLD,
            {
                "You shiver with cold",
                "A chill runs through your body",
                "Your @hands@ feel@hand_conj@ numb with cold",
                "A chill runs through your body",
                "You feel uncomfortably cold",
                "Frost covers your body",
                "You feel extremely cold",
                "You are covered in a thin layer of ice",
                "Heat is drained from your body",
                "You are caught in an explosion of ice and frost",
                "You are encased in ice",
            },
            {
                "@The_monster@ shivers with cold",
                "Frost covers @the_monster@'s body",
                "@The_monster@ shivers",
                "@The_monster@ is covered in a thin layer of ice",
                "Heat is drained from @the_monster@",
                "@The_monster@ is caught in an explosion of ice and frost",
                "@The_monster@ is encased in ice",
            },
            {
                "Wisps of condensation drift in the air",
                "Ice and frost explode from out of thin air",
                "An unseen figure is encased in ice",
            },
            nullptr,
        },
    },
    {
        spschool::air,
        {
            BEAM_ELECTRICITY,
            {
                "Sparks of electricity dance around you",
                "You are blasted with air",
                "There is a short, sharp shower of sparks",
                "The wind whips around you",
                "You feel a sudden electric shock",
                "Electricity courses through your body",
                "You are caught in an explosion of electrical discharges",
            },
            {
                "@The_monster@ bobs in the air for a moment",
                "Sparks of electricity dance between @the_monster@'s @hands@",
                "@The_monster@ is blasted with air",
                "@The_monster@ is briefly showered in sparks",
                "The wind whips around @the_monster@",
                "@The_monster@ is jolted",
                "@The_monster@ is caught in an explosion of electrical discharges",
                "@The_monster@ is struck by twisting air",
            },
            {
                "The wind whips",
                "Something invisible sparkles with electricity",
                "Electrical discharges explode from out of thin air",
                "The air madly twists around a spot",
            },
            nullptr,
        },
    },
    {
        spschool::earth,
        {
            BEAM_NONE, // use special effect to check AC
            {
                "Sand pours from your @hands@",
                "You feel a surge of energy from the ground",
                "The floor shifts beneath you alarmingly",
                "You are hit by flying rocks",
                "You are blasted with sand",
                "Rocks fall onto you out of nowhere",
                "You are caught in an explosion of flying shrapnel",
            },
            {
                "Sand pours from @the_monster@'s @hands@",
                "@The_monster@ is hit by flying rocks",
                "@The_monster@ is blasted with sand",
                "Rocks fall onto @the_monster@ out of nowhere",
                "@The_monster@ is caught in an explosion of flying shrapnel",
            },
            {
                "Flying rocks appear out of thin air",
                "A miniature sandstorm briefly appears",
                "Rocks fall out of nowhere",
                "Flying shrapnel explodes from thin air",
            },
            [] (actor& target, actor* source, miscast_source_info mc_info,
                int dam, string cause) {
                dam = target.apply_ac(dam, 0, ac_type::triple);
                _ouch(target, source, mc_info, dam, BEAM_FRAG, cause);
            }
        },
    },
    {
        spschool::poison,
        {
            BEAM_POISON,
            {
                "You feel odd",
                "You feel rather nauseous for a moment",
                "You feel incredibly sick",
            },
            {
                "@The_monster@ looks faint for a moment",
                "@The_monster@ has an odd expression for a moment",
                "@The_monster@ is briefly tinged with green",
                "@The_monster@ looks rather nauseous for a moment",
                "@The_monster@ looks incredibly sick",
            },
            {
                "The air has a green tinge for a moment",
            },
            nullptr,
        },
    },
};

// Spell miscasts, contamination pluss the miscast effect
// This is only used for the player
void miscast_effect(spell_type spell, int fail)
{

    // All spell failures give a bit of magical radiation.
    // Failure is a function of power squared multiplied by how
    // badly you missed the spell. High power spells can be
    // quite nasty: 9 * 9 * 90 / 500 = 15 points of
    // contamination!
    const int nastiness = spell_difficulty(spell) * spell_difficulty(spell)
                          * fail + 250;
    const int cont_points = 2 * nastiness;

    contaminate_player(cont_points, true);

    // No evil effects other than contam for minor miscasts
    if (nastiness <= MISCAST_THRESHOLD + 250)
    {
        canned_msg(MSG_NOTHING_HAPPENS);
        return;
    }

    vector<spschool> school_list;
    for (const auto bit : spschools_type::range())
        if (spell_typematch(spell, bit))
            school_list.push_back(bit);

    spschool school = *random_iterator(school_list);

    if (school == spschool::necromancy
        && have_passive(passive_t::miscast_protection_necromancy))
    {
        if (x_chance_in_y(you.piety, piety_breakpoint(5)))
        {
            simple_god_message(" protects you from your miscast "
                               "necromantic spell!");
            return;
        }
    }

    miscast_effect(you, nullptr, {miscast_source::spell},
                   school,
                   spell_difficulty(spell),
                   raw_spell_fail(spell), string("miscasting ") + spell_title(spell));
}

// Miscasts from other sources (god wrath, spellbinder melee, wild magic card,
// wizmode, hell effects)
void miscast_effect(actor& target, actor* source, miscast_source_info mc_info,
                    spschool school, int level, int fail, string cause)
{
    if (!target.alive())
    {
        dprf("Miscast target '%s' already dead",
             target.name(DESC_PLAIN, true).c_str());
        return;
    }

    if (school == spschool::random)
        school = spschools_type::exponent(random2(SPSCHOOL_LAST_EXPONENT + 1));

    miscast_datum effect =  miscast_effects.find(school)->second;

    int dam = div_rand_round(roll_dice(level, fail + level), MISCAST_DIVISOR);

    if (effect.flavour == BEAM_NONE)
    {
        ASSERT(effect.special);

        _do_msg(target, effect, 0);
        effect.special(target, source, mc_info, dam, cause);
    }
    else
    {
        _do_msg(target, effect,
                resist_adjust_damage(&target, effect.flavour, dam));
        _ouch(target, source, mc_info, dam, effect.flavour, cause);
    }

    if (target.is_player())
        xom_is_stimulated( (level / 3) * 50);
}
