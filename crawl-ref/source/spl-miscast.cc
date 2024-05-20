/**
 * @file
 * @brief Spell miscast functions.
**/

#include "AppHdr.h"

#include "spl-miscast.h"

#include "attack.h"
#include "beam-type.h"
#include "database.h"
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
    function<void (actor& target, actor* source, miscast_source_info mc_info,
                   int dam, string cause)> special;
};

static void _do_msg(actor& target, spschool which, int dam)
{
    if (!you.see_cell(target.pos()))
        return;

    string db_key = string(spelltype_long_name(which)) + " miscast ";
    if (target.is_player())
        db_key += "player";
    else if (you.can_see(target))
        db_key += "monster";
    else
        db_key += "unseen";

    string msg = getSpeakString(db_key);

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
        {
            msg = replace_all(msg, "'s body", "");
            msg = replace_all(msg, "'s skin", "");
        }
    }
    // For monsters this is done inside do_mon_str_replacements
    else
        msg = maybe_pick_random_substring(msg);

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
            nullptr
        }
    },
    {
        spschool::hexes,
        {
            BEAM_NONE,
            [] (actor& target, actor* source, miscast_source_info /*mc_info*/,
                int dam, string /*cause*/) {
                target.slow_down(source, dam);
            }
        },
    },
    {
        spschool::summoning,
        {
            BEAM_NONE,
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
#if TAG_MAJOR_VERSION == 34
                    case ATT_OLD_STRICT_NEUTRAL:
#endif
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
            nullptr,
        },
    },
    {
        spschool::translocation,
        {
            BEAM_NONE,
            [] (actor& target, actor* source, miscast_source_info /*mc_info*/,
                int dam, string /*cause*/) {

                if (target.is_player())
                {
                    // number arbitrarily chosen & needs more playtesting
                    const int dur = div_rand_round(dam, 2);
                    you.set_duration(DUR_DIMENSION_ANCHOR, dur, dur);
                    you.set_duration(DUR_NO_MOMENTUM, dur, dur,
                                     "You are magically locked in place.");
                }
                else
                {
                    // TODO: monster version? something else?
                     target.as_monster()->add_ench(
                         mon_enchant(ENCH_DIMENSION_ANCHOR,
                                     0, source, dam * BASELINE_DELAY));
                }
            }

        },
    },
    {
        spschool::fire,
        {
            BEAM_FIRE,
            nullptr,
        },
    },
    {
        spschool::ice,
        {
            BEAM_COLD,
            nullptr,
        },
    },
    {
        spschool::air,
        {
            BEAM_ELECTRICITY,
            nullptr,
        },
    },
    {
        spschool::earth,
        {
            BEAM_NONE, // use special effect to check AC
            [] (actor& target, actor* source, miscast_source_info mc_info,
                int dam, string cause) {
                dam = target.apply_ac(dam, 0, ac_type::triple);
                _ouch(target, source, mc_info, dam, BEAM_FRAG, cause);
            }
        },
    },
    {
        spschool::alchemy,
        {
            BEAM_NONE,
            [] (actor& target, actor* source, miscast_source_info /*mc_info*/,
                int dam, string /*cause*/)
            {
                target.poison(source, dam * 5 / 2);
            },
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

    // only monster spells should lack schools altogether, and they should
    // only be castable under wizmode
    ASSERT(you.wizard && (get_spell_flags(spell) & spflag::monster)
            || !school_list.empty());

    if (school_list.empty())
        return;

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

    // Don't summon friendly nameless horrors if they would always turn hostile.
    if (source && source->is_player()
        && school == spschool::summoning
        && you.allies_forbidden())
    {
        return;
    }

    miscast_datum effect = miscast_effects.find(school)->second;

    int dam = div_rand_round(roll_dice(level, fail + level), MISCAST_DIVISOR);

    if (effect.flavour == BEAM_NONE)
    {
        ASSERT(effect.special);

        _do_msg(target, school, 0);
        effect.special(target, source, mc_info, dam, cause);
    }
    else
    {
        _do_msg(target, school,
                resist_adjust_damage(&target, effect.flavour, dam));
        _ouch(target, source, mc_info, dam, effect.flavour, cause);
    }

    if (target.is_player())
        xom_is_stimulated( (level / 3) * 50);
}
