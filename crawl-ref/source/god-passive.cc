#include "AppHdr.h"

#include "god-passive.h"

#include <algorithm>
#include <cmath>

#include "act-iter.h"
#include "artefact.h"
#include "art-enum.h"
#include "branch.h"
#include "chardump.h"
#include "cloud.h"
#include "coordit.h"
#include "directn.h"
#include "env.h"
#include "eq-type-flags.h"
#include "fight.h"
#include "files.h"
#include "food.h"
#include "fprop.h"
#include "god-abil.h"
#include "god-item.h"
#include "god-prayer.h"
#include "invent.h" // in_inventory
#include "item-name.h"
#include "item-prop.h"
#include "item-status-flag-type.h"
#include "items.h"
#include "libutil.h"
#include "map-knowledge.h"
#include "melee-attack.h"
#include "message.h"
#include "mon-cast.h"
#include "mon-place.h"
#include "mon-util.h"
#include "output.h"
#include "religion.h"
#include "shout.h"
#include "skills.h"
#include "spl-clouds.h"
#include "state.h"
#include "status.h"
#include "stringutil.h"
#include "terrain.h"
#include "throw.h"
#include "unwind.h"
#include "view.h"

// TODO: template out the differences between this and god_power.
// TODO: use the display method rather than dummy powers in god_powers.
// TODO: finish using these for implementing passive abilities.
struct god_passive
{
    // 1-6 means it unlocks at that many stars of piety;
    // 0 means it is always present when in good standing with the god;
    // -1 means it is present even under penance;
    int rank;
    passive_t pasv;
    /** Message to be given on gain of this passive.
     *
     * If the string begins with an uppercase letter, it is treated as
     * a complete sentence. Otherwise, if it contains the substring " NOW ",
     * the string "You " is prepended. Otherwise, the string "You NOW " is
     * prepended to the message, with the " NOW " then being replaced.
     *
     * The substring "GOD" is replaced with the name of the god.
     * The substring " NOW " is replaced with " now " for messages about
     * gaining the ability, or " " for messages about having the ability.
     *
     * Examples:
     *   "have super powers"
     *     => "You have super powers", "You now have super powers."
     *   "are NOW powerful"
     *     => "You are powerful", "You are now powerful."
     *   "Your power is NOW great"
     *     => "Your power is great", "Your power is now great"
     *   "GOD NOW makes you powerful"
     *     => "Moloch makes you powerful", "Moloch now makes you powerful"
     *   "GOD grants you a boon"
     *     => "Moloch grants you a boon" (in all situations)
     *
     * FIXME: This member is currently unused.
     *
     * @see god_passive::loss.
     */
    const char* gain;
    /** Message to be given on loss of this passive.
     *
     * If empty, use the gain message. This makes sense only if the message
     * contains " NOW ", either explicitly or implicitly through not
     * beginning with a capital letter.
     *
     * The substring "GOD" is replaced with the name of the god.
     * The substring " NOW " is replaced with " no longer ".
     *
     * Examples:
     *   "have super powers"
     *     => "You no longer have super powers"
     *   "are NOW powerful"
     *     => "You are no longer powerful"
     *   "Your power is NOW great"
     *     => "Your power is no longer great"
     *   "GOD NOW makes you powerful"
     *     => "Moloch no longer makes you powerful"
     *   "GOD grants you a boon"
     *     => "Moloch grants you a boon" (probably incorrect)
     *
     * FIXME: This member is currently unused.
     *
     * @see god_passive::gain.
     */
    const char* loss;

    god_passive(int rank_, passive_t pasv_, const char* gain_,
                const char* loss_ = "")
        : rank{rank_}, pasv{pasv_}, gain{gain_}, loss{*loss_ ? loss_ : gain_}
    { }

    god_passive(int rank_, const char* gain_, const char* loss_ = "")
        : god_passive(rank_, passive_t::none, gain_, loss_)
    { }

    void display(bool gaining, const char* fmt) const
    {
        const char * const str = gaining ? gain : loss;
        if (isupper(str[0]))
            god_speaks(you.religion, str);
        else
            god_speaks(you.religion, make_stringf(fmt, str).c_str());
    }
};

static const vector<god_passive> god_passives[] =
{
    // no god
    { },

    // Zin
    {
        { -1, passive_t::protect_from_harm,
              "GOD sometimes watches over you",
              "GOD no longer watches over you"
        },
        { -1, passive_t::resist_mutation,
              "GOD can shield you from mutations",
              "GOD NOW you from mutations"
        },
        { -1, passive_t::resist_polymorph,
              "GOD can protect you from unnatural transformations",
              "GOD NOW protects you from unnatural transformations",
        },
        { -1, passive_t::resist_hell_effects,
              "GOD can protect you from effects of Hell",
              "GOD NOW protects you from effects of Hell"
        },
        { -1, passive_t::warn_shapeshifter,
              "GOD will NOW warn you about shapeshifters"
        },
        {
          6, passive_t::cleanse_mut_potions,
              "GOD cleanses your potions of mutation",
              "GOD no longer cleanses your potions of mutation",
        }
    },

    // TSO
    {
        { -1, passive_t::protect_from_harm,
              "GOD sometimes watches over you",
              "GOD no longer watches over you"
        },
        { -1, passive_t::abjuration_protection_hd,
              "GOD NOW protects your summons from abjuration" },
        { -1, passive_t::bless_followers_vs_evil,
              "GOD NOW blesses your followers when they kill evil beings" },
        { -1, passive_t::restore_hp_mp_vs_evil,
              "gain health and magic from killing evil beings" },
        { -1, passive_t::no_stabbing,
              "are NOW prevented from stabbing unaware creatures" },
        {  0, passive_t::halo, "are NOW surrounded by divine halo" },
    },

    // Kikubaaqudgha
    {
        {  2, passive_t::miscast_protection_necromancy,
              "GOD NOW protects you from necromancy miscasts"
              " and mummy death curses"
        },
        {  4, passive_t::resist_torment,
              "GOD NOW protects you from torment" },
    },

    // Yredelemnul
    {
        {  3, passive_t::nightvision, "can NOW see well in the dark" },
    },

    // Xom
    { },

    // Vehumet
    {
        { -1, passive_t::mp_on_kill,
              "have a chance to gain magical power from killing" },
        {  3, passive_t::spells_success,
              "are NOW less likely to miscast destructive spells" },
        {  4, passive_t::spells_range,
              "can NOW cast destructive spells farther" },
    },

    // Okawaru
    {
        // None
    },

    // Makhleb
    {
        { -1, passive_t::restore_hp, "gain health from killing" },
    },

    // Sif Muna
    {
        {  2, passive_t::miscast_protection,
              "GOD NOW protects you from miscasts" },
    },

    // Trog
    {
        { -1, passive_t::abjuration_protection,
              "GOD NOW protects your allies from abjuration"
        },
        {  0, passive_t::extend_berserk,
              "can NOW maintain berserk longer, and are less likely to pass out",
              "can NOW maintain berserk as long, and are more likely to pass out"
        },
    },

    // Nemelex
    {
        // None
    },

    // Elyvilon
    {
        { -1, passive_t::protect_from_harm,
              "GOD sometimes watches over you",
              "GOD no longer watches over you"
        },
        { -1, passive_t::protect_ally,
              "GOD can protect the life of your allies",
              "GOD NOW protects the life of your allies"
        },
    },

    // Lugonu
    {
        { -1, passive_t::safe_distortion,
              "are NOW protected from distortion unwield effects" },
        { -1, passive_t::map_rot_res_abyss,
              "remember the shape of the Abyss better" },
        {  5, passive_t::attract_abyssal_rune,
              "GOD will NOW help you find the Abyssal rune" },
    },

    // Beogh
    {
        { -1, passive_t::share_exp, "share experience with your followers" },
        {  3, passive_t::convert_orcs, "inspire orcs to join your side" },
        {  3, passive_t::bless_followers,
              "GOD will bless your followers",
              "GOD will no longer bless your followers"
        },
        {  5, passive_t::water_walk, "walk on water" },
    },

    // Jiyva
    {
        { -1, passive_t::neutral_slimes,
              "Slimes and eye monsters are NOW neutral towards you" },
        { -1, passive_t::jellies_army,
              "GOD NOW summons jellies to protect you" },
        { -1, passive_t::jelly_eating,
              "GOD NOW allows jellies to devour items" },
        { -1, passive_t::fluid_stats,
              "GOD NOW adjusts your attributes periodically" },
        {  0, passive_t::slime_wall_immune,
              "are NOW immune to slime covered walls" },
        {  2, passive_t::slime_feed,
              "Items consumed by your fellow slimes NOW feed you" },
        {  3, passive_t::resist_corrosion,
              "GOD NOW protects you from corrosion" },
        {  4, passive_t::slime_mp,
              "Items consumed by your fellow slimes NOW restore"
              " your magical power"
        },
        {  5, passive_t::slime_hp,
              "Items consumed by your fellow slimes NOW restore"
              " your health"
        },
        {  6, passive_t::spawn_slimes_on_hit,
              "spawn slimes when struck by massive blows" },
        {  6, passive_t::unlock_slime_vaults,
              "GOD NOW grants you access to the hidden treasures"
              " of the Slime Pits"
        },
    },

    // Fedhas
    {
        { -1, passive_t::pass_through_plants,
              "can NOW walk through plants" },
        { -1, passive_t::shoot_through_plants,
              "can NOW safely fire through allied plants" },
        {  0, passive_t::friendly_plants,
              "Allied plants are NOW friendly towards you" },
    },

    // Cheibriados
    {
        { -1, passive_t::no_haste,
              "are NOW protected from inadvertent hurry" },
        { -1, passive_t::slowed, "move less quickly" },
        {  0, passive_t::slow_orb_run,
              "GOD will NOW aid your escape with the Orb of Zot",
        },
        {  0, passive_t::stat_boost,
              "GOD NOW supports your attributes"
        },
        {  0, passive_t::slow_abyss,
              "GOD will NOW slow the Abyss"
        },
        // TODO: this one should work regardless of penance
        {  1, passive_t::slow_metabolism, "have a slowed metabolism" },
    },

    // Ashenzari
    {
        { -1, passive_t::want_curses, "prefer cursed items" },
        { -1, passive_t::detect_portals, "sense portals" },
        { -1, passive_t::identify_items, "sense the properties of items" },
        {  0, passive_t::auto_map, "have improved mapping abilities" },
        {  0, passive_t::detect_montier, "sense threats" },
        {  0, passive_t::detect_items, "sense items" },
        {  0, passive_t::search_traps,
              "are NOW better at searching for traps" },
        {  2, passive_t::bondage_skill_boost,
              "get a skill boost from cursed items" },
        {  3, passive_t::sinv, "are NOW clear of vision" },
        {  4, passive_t::clarity, "are NOW clear of mind" },
    },

    // Dithmenos
    {
        {  1, passive_t::nightvision, "can NOW see well in the dark" },
        {  1, passive_t::umbra, "are NOW surrounded by an umbra" },
        // TODO: this one should work regardless of penance.
        {  3, passive_t::hit_smoke, "emit smoke when hit" },
        {  4, passive_t::shadow_attacks,
              "Your attacks are NOW mimicked by a shadow" },
        {  4, passive_t::shadow_spells,
              "Your attack spells are NOW mimicked by a shadow" },
    },

    // Gozag
    {
        { -1, passive_t::detect_gold, "detect gold" },
        {  0, passive_t::goldify_corpses,
              "GOD NOW turns all corpses to gold" },
        {  0, passive_t::gold_aura, "have a gold aura" },
    },

    // Qazlal
    {
        {  0, passive_t::cloud_immunity, "are ADV immune to clouds" },
        {  1, passive_t::storm_shield,
              "generate elemental clouds to protect yourself" },
        {  4, passive_t::upgraded_storm_shield,
              "Your chances to be struck by projectiles are NOW reduced" },
        {  5, passive_t::elemental_adaptation,
              "Elemental attacks NOW leave you somewhat more resistant"
              " to them"
        }
    },

    // Ru
    {
        {  1, passive_t::aura_of_power,
              "Your enemies will sometimes fail their attack or even hit themselves",
              "Your enemies will NOW fail their attack or hit themselves"
        },
        {  2, passive_t::upgraded_aura_of_power,
              "Enemies that inflict damage upon you will sometimes receive"
              " a detrimental status effect",
              "Enemies that inflict damage upon you will NOW receive"
              " a detrimental status effect"
        },
    },

    // Pakellas
    {
        { -1, passive_t::no_mp_regen,
              "GOD NOW prevents you from regenerating your magical power" },
        { -1, passive_t::mp_on_kill, "have a chance to gain magical power from"
                                     " killing" },
        {  1, passive_t::bottle_mp,
              "GOD NOW collects and distills excess magic from your kills"
        },
    },

    // Uskayaw
    { },

    // Hepliaklqana
    {
        { -1, passive_t::frail,
              "GOD NOW siphons a part of your essence into your ancestor" },
        {  5, passive_t::transfer_drain,
              "drain nearby creatures when transferring your ancestor" },
    },

    // Wu Jian
    {
        { 0, passive_t::wu_jian_lunge, "perform damaging attacks by moving towards foes." },
        { 1, passive_t::wu_jian_whirlwind, "lightly attack and pin monsters in place by moving around them." },
        { 2, passive_t::wu_jian_wall_jump, "perform airborne attacks by moving against a solid obstacle." },
    },
};
COMPILE_CHECK(ARRAYSZ(god_passives) == NUM_GODS);

bool have_passive(passive_t passive)
{
    const auto &pasvec = god_passives[you.religion];
    return any_of(begin(pasvec), end(pasvec),
                  [passive] (const god_passive &p) -> bool
                  {
                      return p.pasv == passive
                          && piety_rank() >= p.rank
                          && (!player_under_penance() || p.rank < 0);
                  });
}

bool will_have_passive(passive_t passive)
{
    const auto &pasvec = god_passives[you.religion];
    return any_of(begin(pasvec), end(pasvec),
                  [passive] (const god_passive &p) -> bool
                  {
                      return p.pasv == passive;
                  });
}

// Returns a large number (10) if we will never get this passive.
int rank_for_passive(passive_t passive)
{
    const auto &pasvec = god_passives[you.religion];
    const auto found = find_if(begin(pasvec), end(pasvec),
                              [passive] (const god_passive &p) -> bool
                              {
                                  return p.pasv == passive;
                              });
    return found == end(pasvec) ? 10 : found->rank;
}

int chei_stat_boost(int piety)
{
    if (!have_passive(passive_t::stat_boost))
        return 0;
    if (piety < piety_breakpoint(0))  // Since you've already begun to slow down.
        return 1;
    if (piety >= piety_breakpoint(5))
        return 15;
    return (piety - 10) / 10;
}

// Eat from one random off-level item stack.
void jiyva_eat_offlevel_items()
{
    // For wizard mode 'J' command
    if (!have_passive(passive_t::jelly_eating))
        return;

    if (crawl_state.game_is_sprint())
        return;

    while (true)
    {
        if (one_chance_in(200))
            break;

        const int branch = random2(NUM_BRANCHES);

        // Choose level based on main dungeon depth so that levels of
        // short branches aren't picked more often.
        ASSERT(brdepth[branch] <= MAX_BRANCH_DEPTH);
        const int level = random2(MAX_BRANCH_DEPTH) + 1;

        const level_id lid(static_cast<branch_type>(branch), level);

        if (lid == level_id::current() || !is_existing_level(lid))
            continue;

        dprf("Checking %s", lid.describe().c_str());

        level_excursion le;
        le.go_to(lid);
        while (true)
        {
            if (one_chance_in(200))
                break;

            const coord_def p = random_in_bounds();

            if (igrd(p) == NON_ITEM || testbits(env.pgrid(p), FPROP_NO_JIYVA))
                continue;

            for (stack_iterator si(p); si; ++si)
            {
                if (!item_is_jelly_edible(*si) || one_chance_in(4))
                    continue;

                if (one_chance_in(4))
                    break;

                dprf("Eating %s on %s",
                     si->name(DESC_PLAIN).c_str(), lid.describe().c_str());

                // Needs a message now to explain possible hp or mp
                // gain from jiyva_slurp_bonus()
                mpr("You hear a distant slurping noise.");
                jiyva_slurp_item_stack(*si);
                item_was_destroyed(*si);
                destroy_item(si.index());
            }
            return;
        }
    }
}

void ash_init_bondage(player *y)
{
    y->bondage_level = 0;
    for (int i = ET_WEAPON; i < NUM_ET; ++i)
        y->bondage[i] = 0;
}

static bool _two_handed()
{
    const item_def* wpn = you.slot_item(EQ_WEAPON, true);
    if (!wpn)
        return false;

    hands_reqd_type wep_type = you.hands_reqd(*wpn, true);
    return wep_type == HANDS_TWO;
}

void ash_check_bondage(bool msg)
{
    if (!will_have_passive(passive_t::bondage_skill_boost))
        return;

    int cursed[NUM_ET] = {0}, slots[NUM_ET] = {0};

    for (int j = EQ_FIRST_EQUIP; j < NUM_EQUIP; j++)
    {
        const equipment_type i = static_cast<equipment_type>(j);
        eq_type s;
        if (i == EQ_WEAPON)
            s = ET_WEAPON;
        else if (i == EQ_SHIELD)
            s = ET_SHIELD;
        else if (i <= EQ_MAX_ARMOUR)
            s = ET_ARMOUR;
        // Missing hands mean fewer rings
        else if (you.species != SP_OCTOPODE && i == EQ_LEFT_RING
                 && you.get_mutation_level(MUT_MISSING_HAND))
        {
            continue;
        }
        // Octopodes don't count these slots:
        else if (you.species == SP_OCTOPODE
                 && ((i == EQ_LEFT_RING || i == EQ_RIGHT_RING)
                     || (i == EQ_RING_EIGHT
                         && you.get_mutation_level(MUT_MISSING_HAND))))
        {
            continue;
        }
        // *Only* octopodes count these slots:
        else if (you.species != SP_OCTOPODE
                 && i >= EQ_RING_ONE && i <= EQ_RING_EIGHT)
        {
            continue;
        }
        // The macabre finger necklace's extra slot does count if equipped.
        else if (!player_equip_unrand(UNRAND_FINGER_AMULET)
                 && i == EQ_RING_AMULET)
        {
            continue;
        }
        else
            s = ET_JEWELS;

        // transformed away slots are still considered to be possibly bound
        if (you_can_wear(i))
        {
            slots[s]++;
            if (you.equip[i] != -1)
            {
                const item_def& item = you.inv[you.equip[i]];
                if (item.cursed() && (i != EQ_WEAPON || is_weapon(item)))
                {
                    if (s == ET_WEAPON
                        && (_two_handed()
                            || you.get_mutation_level(MUT_MISSING_HAND)))
                    {
                        cursed[ET_WEAPON] = 3;
                        cursed[ET_SHIELD] = 3;
                    }
                    else
                    {
                        cursed[s]++;
                        if (i == EQ_BODY_ARMOUR && is_unrandom_artefact(item, UNRAND_LEAR))
                            cursed[s] += 3;
                    }
                }
            }
        }
    }

    int8_t new_bondage[NUM_ET];
    int old_level = you.bondage_level;
    for (int s = ET_WEAPON; s < NUM_ET; s++)
    {
        if (slots[s] == 0)
            new_bondage[s] = -1;
        // That's only for 2 handed weapons.
        else if (cursed[s] > slots[s])
            new_bondage[s] = 3;
        else if (cursed[s] == slots[s])
            new_bondage[s] = 2;
        else if (cursed[s] > slots[s] / 2)
            new_bondage[s] = 1;
        else
            new_bondage[s] = 0;
    }

    you.bondage_level = 0;
    // kittehs don't obey hoomie rules!
    if (you.species == SP_FELID)
    {
        for (int i = EQ_LEFT_RING; i <= EQ_AMULET; ++i)
            if (you.equip[i] != -1 && you.inv[you.equip[i]].cursed())
                ++you.bondage_level;

        // Allow full bondage when all available slots are cursed.
        if (you.bondage_level == 3)
            ++you.bondage_level;
    }
    else
        for (int i = ET_WEAPON; i < NUM_ET; ++i)
            if (new_bondage[i] > 0)
                ++you.bondage_level;

    int flags = 0;
    if (msg)
    {
        for (int s = ET_WEAPON; s < NUM_ET; s++)
            if (new_bondage[s] != you.bondage[s])
                flags |= 1 << s;
    }

    you.skill_boost.clear();
    for (int s = ET_WEAPON; s < NUM_ET; s++)
    {
        you.bondage[s] = new_bondage[s];
        map<skill_type, int8_t> boosted_skills = ash_get_boosted_skills(eq_type(s));
        for (const auto &entry : boosted_skills)
        {
            you.skill_boost[entry.first] += entry.second;
            if (you.skill_boost[entry.first] > 3)
                you.skill_boost[entry.first] = 3;
        }

    }

    if (msg)
    {
        string desc = ash_describe_bondage(flags, you.bondage_level != old_level);
        if (!desc.empty())
            mprf(MSGCH_GOD, "%s", desc.c_str());
    }
}

string ash_describe_bondage(int flags, bool level)
{
    string desc;
    if (flags & ETF_WEAPON && flags & ETF_SHIELD
        && you.bondage[ET_WEAPON] != -1)
    {
        if (you.bondage[ET_WEAPON] == you.bondage[ET_SHIELD])
        {
            const string verb = make_stringf("are%s",
                                             you.bondage[ET_WEAPON] ? ""
                                                                    : " not");
            desc = you.hands_act(verb, "bound.\n");
        }
        else
        {
            // FIXME: what if you sacrificed a hand?
            desc = make_stringf("Your %s %s is bound but not your %s %s.\n",
                                you.bondage[ET_WEAPON] ? "weapon" : "shield",
                                you.hand_name(false).c_str(),
                                you.bondage[ET_WEAPON] ? "shield" : "weapon",
                                you.hand_name(false).c_str());
        }
    }
    else if (flags & ETF_WEAPON && you.bondage[ET_WEAPON] != -1)
    {
        desc = make_stringf("Your weapon %s is %sbound.\n",
                            you.hand_name(false).c_str(),
                            you.bondage[ET_WEAPON] ? "" : "not ");
    }
    else if (flags & ETF_SHIELD && you.bondage[ET_SHIELD] != -1)
    {
        desc = make_stringf("Your shield %s is %sbound.\n",
                            you.hand_name(false).c_str(),
                            you.bondage[ET_SHIELD] ? "" : "not ");
    }

    if (flags & ETF_ARMOUR && flags & ETF_JEWELS
        && you.bondage[ET_ARMOUR] == you.bondage[ET_JEWELS]
        && you.bondage[ET_ARMOUR] != -1)
    {
        desc += make_stringf("You are %s bound in armour %s jewellery.\n",
                             you.bondage[ET_ARMOUR] == 0 ? "not" :
                             you.bondage[ET_ARMOUR] == 1 ? "partially"
                                                         : "fully",
                             you.bondage[ET_ARMOUR] == 0 ? "or" : "and");
    }
    else
    {
        if (flags & ETF_ARMOUR && you.bondage[ET_ARMOUR] != -1)
        {
            desc += make_stringf("You are %s bound in armour.\n",
                                 you.bondage[ET_ARMOUR] == 0 ? "not" :
                                 you.bondage[ET_ARMOUR] == 1 ? "partially"
                                                             : "fully");
        }

        if (flags & ETF_JEWELS && you.bondage[ET_JEWELS] != -1)
        {
            desc += make_stringf("You are %s bound in jewellery.\n",
                                 you.bondage[ET_JEWELS] == 0 ? "not" :
                                 you.bondage[ET_JEWELS] == 1 ? "partially"
                                                             : "fully");
        }
    }

    if (level)
    {
        desc += make_stringf("You are %s bound.",
                             you.bondage_level == 0 ? "not" :
                             you.bondage_level == 1 ? "slightly" :
                             you.bondage_level == 2 ? "moderately" :
                             you.bondage_level == 3 ? "seriously" :
                             you.bondage_level == 4 ? "fully"
                                                    : "buggily");
    }

    return trim_string(desc);
}

static bool _is_slot_cursed(equipment_type eq)
{
    const item_def *worn = you.slot_item(eq, true);
    if (!worn || !worn->cursed())
        return false;

    if (eq == EQ_WEAPON)
        return is_weapon(*worn);
    return true;
}

bool god_id_item(item_def& item, bool silent)
{
    iflags_t old_ided = item.flags & ISFLAG_IDENT_MASK;
    iflags_t ided = 0;

    if (have_passive(passive_t::identify_items))
    {
        // Ashenzari (and other gods with both identify_items and want_curses)
        // ties identification of weapon/armour plusses to cursed slots.
        const bool ash = have_passive(passive_t::want_curses);

        // Don't identify runes or the orb, since this has no gameplay purpose
        // and might mess up other things.
        if (item.base_type == OBJ_RUNES || item_is_orb(item))
            return false;

        ided = ISFLAG_KNOW_CURSE;

        if ((item.base_type == OBJ_JEWELLERY || item.base_type == OBJ_STAVES)
            && item_needs_autopickup(item))
        {
            item.props["needs_autopickup"] = true;
        }

        if (is_weapon(item) || item.base_type == OBJ_ARMOUR)
            ided |= ISFLAG_KNOW_PROPERTIES | ISFLAG_KNOW_TYPE;

        if (item.base_type == OBJ_JEWELLERY)
            ided |= ISFLAG_IDENT_MASK;

        if (item.base_type == OBJ_ARMOUR
            && (!ash || _is_slot_cursed(get_armour_slot(item))))
        {
            ided |= ISFLAG_KNOW_PLUSES;
        }

        if (is_weapon(item)
            && (!ash || _is_slot_cursed(EQ_WEAPON)))
        {
            ided |= ISFLAG_KNOW_PLUSES;
        }
    }

    if (ided & ~old_ided)
    {
        if (ided & ISFLAG_KNOW_TYPE)
            set_ident_type(item, true);
        set_ident_flags(item, ided);

        if (item.props.exists("needs_autopickup") && is_useless_item(item))
            item.props.erase("needs_autopickup");

        if (&item == you.weapon())
            you.wield_change = true;

        if (!silent)
            mprf_nocap("%s", item.name(DESC_INVENTORY_EQUIP).c_str());

        seen_item(item);
        if (in_inventory(item))
            auto_assign_item_slot(item);
        return true;
    }

    // nothing new
    return false;
}

void ash_id_monster_equipment(monster* mon)
{
    if (!have_passive(passive_t::identify_items))
        return;

    bool id = false;

    for (unsigned int i = 0; i <= MSLOT_LAST_VISIBLE_SLOT; ++i)
    {
        if (mon->inv[i] == NON_ITEM)
            continue;

        item_def &item = mitm[mon->inv[i]];
        if ((i != MSLOT_WAND || !is_offensive_wand(item))
            && !item_is_branded(item))
        {
            continue;
        }

        if (i == MSLOT_WAND)
        {
            set_ident_type(OBJ_WANDS, item.sub_type, true);
            mon->props["wand_known"] = true;
        }
        else
            set_ident_flags(item, ISFLAG_KNOW_TYPE);

        id = true;
    }

    if (id)
        mon->props["ash_id"] = true;
}

static bool is_ash_portal(dungeon_feature_type feat)
{
    if (feat_is_portal_entrance(feat))
        return true;
    switch (feat)
    {
    case DNGN_ENTER_HELL:
    case DNGN_ENTER_ABYSS: // for completeness
    case DNGN_EXIT_THROUGH_ABYSS:
    case DNGN_EXIT_ABYSS:
    case DNGN_ENTER_PANDEMONIUM:
    case DNGN_EXIT_PANDEMONIUM:
    // DNGN_TRANSIT_PANDEMONIUM is too mundane
        return true;
    default:
        return false;
    }
}

// Yay for rectangle_iterator and radius_iterator not sharing a base type
static bool _check_portal(coord_def where)
{
    const dungeon_feature_type feat = grd(where);
    if (feat != env.map_knowledge(where).feat() && is_ash_portal(feat))
    {
        env.map_knowledge(where).set_feature(feat);
        set_terrain_mapped(where);

        if (!testbits(env.pgrid(where), FPROP_SEEN_OR_NOEXP))
        {
            env.pgrid(where) |= FPROP_SEEN_OR_NOEXP;
            if (!you.see_cell(where))
                return true;
        }
    }
    return false;
}

int ash_detect_portals(bool all)
{
    if (!have_passive(passive_t::detect_portals))
        return 0;

    int portals_found = 0;
    const int map_radius = LOS_DEFAULT_RANGE + 1;

    if (all)
    {
        for (rectangle_iterator ri(0); ri; ++ri)
        {
            if (_check_portal(*ri))
                portals_found++;
        }
    }
    else
    {
        for (radius_iterator ri(you.pos(), map_radius, C_SQUARE); ri; ++ri)
        {
            if (_check_portal(*ri))
                portals_found++;
        }
    }

    you.seen_portals += portals_found;
    return portals_found;
}

monster_type ash_monster_tier(const monster *mon)
{
    return monster_type(MONS_SENSED_TRIVIAL + monster_info(mon).threat);
}

map<skill_type, int8_t> ash_get_boosted_skills(eq_type type)
{
    const int bondage = you.bondage[type];
    map<skill_type, int8_t> boost;
    if (bondage <= 0)
        return boost;

    // Include melded.
    const item_def* wpn = you.slot_item(EQ_WEAPON, true);
    const item_def* armour = you.slot_item(EQ_BODY_ARMOUR, true);
    const int evp = armour ? -property(*armour, PARM_EVASION) / 10 : 0;
    switch (type)
    {
    case (ET_WEAPON):
        ASSERT(wpn);

        // Boost weapon skill. Plain "staff" means an unrand magical staff,
        // boosted later.
        if (wpn->base_type == OBJ_WEAPONS
            && wpn->sub_type != WPN_STAFF)
        {
            boost[item_attack_skill(*wpn)] = bondage;
        }
        // Staves that have a melee effect, powered by evocations.
        if (staff_uses_evocations(*wpn))
        {
            boost[SK_EVOCATIONS] = 1;
            boost[SK_STAVES] = 1;

        }
        // Staves with an evokable ability but no melee effect.
        else if (is_weapon(*wpn)
                 && item_is_evokable(*wpn, false, false, false, false))
        {
            boost[SK_EVOCATIONS] = 2;
        }
        // Other magical staves.
        else if (wpn->base_type == OBJ_STAVES)
            boost[SK_SPELLCASTING] = 2;
        break;

    case (ET_SHIELD):
        if (bondage == 2)
            boost[SK_SHIELDS] = 1;
        break;

    // Bonus for bounded armour depends on body armour type.
    case (ET_ARMOUR):
        if (evp < 6)
        {
            boost[SK_STEALTH] = bondage;
            boost[SK_DODGING] = bondage;
        }
        else if (evp < 12)
        {
            boost[SK_DODGING] = bondage;
            boost[SK_ARMOUR] = bondage;
        }
        else
            boost[SK_ARMOUR] = bondage + 1;
        break;

    // Boost all spell schools and evoc (to give some appeal to melee).
    case (ET_JEWELS):
        for (skill_type sk = SK_FIRST_MAGIC_SCHOOL; sk <= SK_LAST_MAGIC; ++sk)
            boost[sk] = bondage;
        boost[SK_EVOCATIONS] = bondage;
        break;

    default:
        die("Unknown equipment type.");
    }

    return boost;
}

/**
 * Does the player have an ash skill boost for a particular skill?
 */
bool ash_has_skill_boost(skill_type sk)
{
    return have_passive(passive_t::bondage_skill_boost)
           && you.skill_boost.count(sk) && you.skill_boost.find(sk)->second;
}

/**
 * Calculate the ash skill point boost for skill sk.
 *
 * @param scaled_skill the skill level to calculate it for, scaled by 10.
 *
 * @return the skill point bonus to use.
 */
unsigned int ash_skill_point_boost(skill_type sk, int scaled_skill)
{
    unsigned int skill_points = 0;

    skill_points += (you.skill_boost[sk] * 2 + 1) * (piety_rank() + 1)
                    * max(scaled_skill, 1) * species_apt_factor(sk);
    return skill_points;
}

int ash_skill_boost(skill_type sk, int scale)
{
    // It gives a bonus to skill points. The formula is:
    // factor * (piety_rank + 1) * skill_level
    // low bonus    -> factor = 3
    // medium bonus -> factor = 5
    // high bonus   -> factor = 7

    unsigned int skill_points = you.skill_points[sk]
                  + get_crosstrain_points(sk)
                  + ash_skill_point_boost(sk, you.skill(sk, 10, true));

    int level = you.skills[sk];
    while (level < MAX_SKILL_LEVEL && skill_points >= skill_exp_needed(level + 1, sk))
        ++level;

    level = level * scale + get_skill_progress(sk, level, skill_points, scale);

    return min(level, MAX_SKILL_LEVEL * scale);
}

int gozag_gold_in_los(actor *whom)
{
    if (!have_passive(passive_t::gold_aura))
        return 0;

    int gold_count = 0;

    for (radius_iterator ri(whom->pos(), LOS_RADIUS, C_SQUARE, LOS_DEFAULT);
         ri; ++ri)
    {
        for (stack_iterator j(*ri); j; ++j)
        {
            if (j->base_type == OBJ_GOLD)
                ++gold_count;
        }
    }

    return gold_count;
}

int qazlal_sh_boost(int piety)
{
    if (!have_passive(passive_t::storm_shield))
        return 0;

    return min(piety, piety_breakpoint(5)) / 10;
}

// Not actually passive, but placing it here so that it can be easily compared
// with Qazlal's boost.
int tso_sh_boost()
{
    if (!you.duration[DUR_DIVINE_SHIELD])
        return 0;

    return you.attribute[ATTR_DIVINE_SHIELD];
}

void qazlal_storm_clouds()
{
    if (!have_passive(passive_t::storm_shield))
        return;

    // You are a *storm*. You are pretty loud!
    noisy(min((int)you.piety, piety_breakpoint(5)) / 10, you.pos());

    const int radius = you.piety >= piety_breakpoint(3) ? 2 : 1;

    vector<coord_def> candidates;
    for (radius_iterator ri(you.pos(), radius, C_SQUARE, LOS_SOLID, true);
         ri; ++ri)
    {
        int count = 0;
        if (cell_is_solid(*ri) || cloud_at(*ri))
            continue;

        // Don't create clouds over firewood
        const monster * mon = monster_at(*ri);
        if (mon != nullptr && mons_is_firewood(*mon))
            continue;

        // No clouds in corridors.
        for (adjacent_iterator ai(*ri); ai; ++ai)
            if (!cell_is_solid(*ai))
                count++;

        if (count >= 5)
            candidates.push_back(*ri);
    }
    const int count =
        div_rand_round(min((int)you.piety, piety_breakpoint(5))
                       * candidates.size() * you.time_taken,
                       piety_breakpoint(5) * 7 * BASELINE_DELAY);
    if (count < 0)
        return;
    shuffle_array(candidates);
    int placed = 0;
    for (unsigned int i = 0; placed < count && i < candidates.size(); i++)
    {
        bool water = false;
        for (adjacent_iterator ai(candidates[i]); ai; ++ai)
        {
            if (feat_is_watery(grd(*ai)))
                water = true;
        }

        // No flame clouds over water to avoid steam generation.
        cloud_type ctype;
        do
        {
            ctype = random_choose(CLOUD_FIRE, CLOUD_COLD, CLOUD_STORM,
                                  CLOUD_DUST);
        } while (water && ctype == CLOUD_FIRE);

        place_cloud(ctype, candidates[i], random_range(3, 5), &you);
        placed++;
    }
}

/**
 * Handle Qazlal's elemental adaptation.
 * This should be called (exactly once) for physical, fire, cold, and electrical damage.
 * Right now, it is called only from expose_player_to_element. This may merit refactoring.
 *
 * @param flavour the beam type.
 * @param strength The adaptations will trigger strength in (11 - piety_rank()) times. In practice, this is mostly called with a value of 2.
 */
void qazlal_element_adapt(beam_type flavour, int strength)
{
    if (strength <= 0
        || !have_passive(passive_t::elemental_adaptation)
        || !x_chance_in_y(strength, 11 - piety_rank()))
    {
        return;
    }

    beam_type what = BEAM_NONE;
    duration_type dur = NUM_DURATIONS;
    string descript = "";
    switch (flavour)
    {
        case BEAM_FIRE:
        case BEAM_LAVA:
        case BEAM_STICKY_FLAME:
        case BEAM_STEAM:
            what = BEAM_FIRE;
            dur = DUR_QAZLAL_FIRE_RES;
            descript = "fire";
            break;
        case BEAM_COLD:
        case BEAM_ICE:
            what = BEAM_COLD;
            dur = DUR_QAZLAL_COLD_RES;
            descript = "cold";
            break;
        case BEAM_ELECTRICITY:
            what = BEAM_ELECTRICITY;
            dur = DUR_QAZLAL_ELEC_RES;
            descript = "electricity";
            break;
        case BEAM_MMISSILE: // for LCS, iron shot
        case BEAM_MISSILE:
        case BEAM_FRAG:
            what = BEAM_MISSILE;
            dur = DUR_QAZLAL_AC;
            descript = "physical attacks";
            break;
        default:
            return;
    }

    if (what != BEAM_FIRE && you.duration[DUR_QAZLAL_FIRE_RES])
    {
        mprf(MSGCH_DURATION, "Your resistance to fire fades away.");
        you.duration[DUR_QAZLAL_FIRE_RES] = 0;
    }

    if (what != BEAM_COLD && you.duration[DUR_QAZLAL_COLD_RES])
    {
        mprf(MSGCH_DURATION, "Your resistance to cold fades away.");
        you.duration[DUR_QAZLAL_COLD_RES] = 0;
    }

    if (what != BEAM_ELECTRICITY && you.duration[DUR_QAZLAL_ELEC_RES])
    {
        mprf(MSGCH_DURATION, "Your resistance to electricity fades away.");
        you.duration[DUR_QAZLAL_ELEC_RES] = 0;
    }

    if (what != BEAM_MISSILE && you.duration[DUR_QAZLAL_AC])
    {
        mprf(MSGCH_DURATION, "Your resistance to physical damage fades away.");
        you.duration[DUR_QAZLAL_AC] = 0;
        you.redraw_armour_class = true;
    }

    mprf(MSGCH_GOD, "You feel %sprotected from %s.",
         you.duration[dur] > 0 ? "more " : "", descript.c_str());

    // was scaled by 10 * strength. But the strength parameter is used so inconsistently that
    // it seems like a constant would be better, based on the typical value of 2.
    you.increase_duration(dur, 20, 80);

    if (what == BEAM_MISSILE)
        you.redraw_armour_class = true;
}

/**
 * Determine whether a Ru worshipper will attempt to interfere with an attack
 * against the player.
 *
 * @return bool Whether or not whether the worshipper will attempt to interfere.
 */
bool does_ru_wanna_redirect(monster* mon)
{
    return have_passive(passive_t::aura_of_power)
            && !mon->friendly()
            && you.see_cell(mon->pos())
            && !mons_is_firewood(*mon)
            && !mon->submerged()
            && !mons_is_projectile(mon->type);
}

/**
 * Determine which, if any, action Ru takes on a possible attack.
 *
 * @return ru_interference
 */
ru_interference get_ru_attack_interference_level()
{
    int r = random2(100);
    int chance = div_rand_round(you.piety, 16);

    // 10% chance of stopping any attack at max piety
    if (r < chance)
        return DO_BLOCK_ATTACK;

    // 5% chance of redirect at max piety
    else if (r < chance + div_rand_round(chance, 2))
        return DO_REDIRECT_ATTACK;

    else
        return DO_NOTHING;
}

static bool _shadow_acts(bool spell)
{
    const passive_t pasv = spell ? passive_t::shadow_spells
                                 : passive_t::shadow_attacks;
    if (!have_passive(pasv))
        return false;

    const int minpiety = piety_breakpoint(rank_for_passive(pasv) - 1);

    // 10% chance at minimum piety; 50% chance at 200 piety.
    const int range = MAX_PIETY - minpiety;
    const int min   = range / 5;
    return x_chance_in_y(min + ((range - min)
                                * (you.piety - minpiety)
                                / (MAX_PIETY - minpiety)),
                         2 * range);
}

monster* shadow_monster(bool equip)
{
    if (monster_at(you.pos()))
        return nullptr;

    int wpn_index  = NON_ITEM;

    // Do a basic clone of the weapon.
    item_def* wpn = you.weapon();
    if (equip
        && wpn
        && is_weapon(*wpn))
    {
        wpn_index = get_mitm_slot(10);
        if (wpn_index == NON_ITEM)
            return nullptr;
        item_def& new_item = mitm[wpn_index];
        if (wpn->base_type == OBJ_STAVES)
        {
            new_item.base_type = OBJ_WEAPONS;
            new_item.sub_type  = WPN_STAFF;
        }
        else
        {
            new_item.base_type = wpn->base_type;
            new_item.sub_type  = wpn->sub_type;
        }
        new_item.quantity = 1;
        new_item.rnd = 1;
        new_item.flags   |= ISFLAG_SUMMONED;
    }

    monster* mon = get_free_monster();
    if (!mon)
    {
        if (wpn_index)
            destroy_item(wpn_index);
        return nullptr;
    }

    mon->type       = MONS_PLAYER_SHADOW;
    mon->behaviour  = BEH_SEEK;
    mon->attitude   = ATT_FRIENDLY;
    mon->flags      = MF_NO_REWARD | MF_JUST_SUMMONED | MF_SEEN
                    | MF_WAS_IN_VIEW | MF_HARD_RESET;
    mon->hit_points = you.hp;
    mon->set_hit_dice(min(27, max(1,
                                  you.skill_rdiv(wpn_index != NON_ITEM
                                                 ? item_attack_skill(mitm[wpn_index])
                                                 : SK_UNARMED_COMBAT, 10, 20)
                                  + you.skill_rdiv(SK_FIGHTING, 10, 20))));
    mon->set_position(you.pos());
    mon->mid        = MID_PLAYER;
    mon->inv[MSLOT_WEAPON]  = wpn_index;
    mon->inv[MSLOT_MISSILE] = NON_ITEM;

    mgrd(you.pos()) = mon->mindex();

    return mon;
}

void shadow_monster_reset(monster *mon)
{
    if (mon->inv[MSLOT_WEAPON] != NON_ITEM)
        destroy_item(mon->inv[MSLOT_WEAPON]);
    if (mon->inv[MSLOT_MISSILE] != NON_ITEM)
        destroy_item(mon->inv[MSLOT_MISSILE]);

    mon->reset();
}

/**
 * Check if the player is in melee range of the target.
 *
 * Certain effects, e.g. distortion blink, can cause monsters to leave melee
 * range between the initial hit & the shadow mimic.
 *
 * XXX: refactor this with attack/fight code!
 *
 * @param target    The creature to be struck.
 * @return          Whether the player is melee range of the target, using
 *                  their current weapon.
 */
static bool _in_melee_range(actor* target)
{
    const int dist = (you.pos() - target->pos()).rdist();
    return dist <= you.reach_range();
}

void dithmenos_shadow_melee(actor* target)
{
    if (!target
        || !target->alive()
        || !_in_melee_range(target)
        || !_shadow_acts(false))
    {
        return;
    }

    monster* mon = shadow_monster();
    if (!mon)
        return;

    mon->target     = target->pos();
    mon->foe        = target->mindex();

    fight_melee(mon, target);

    shadow_monster_reset(mon);
}

void dithmenos_shadow_throw(const dist &d, const item_def &item)
{
    ASSERT(d.isValid);
    if (!_shadow_acts(false))
        return;

    monster* mon = shadow_monster();
    if (!mon)
        return;

    int ammo_index = get_mitm_slot(10);
    if (ammo_index != NON_ITEM)
    {
        item_def& new_item = mitm[ammo_index];
        new_item.base_type = item.base_type;
        new_item.sub_type  = item.sub_type;
        new_item.quantity  = 1;
        new_item.rnd = 1;
        new_item.flags    |= ISFLAG_SUMMONED;
        mon->inv[MSLOT_MISSILE] = ammo_index;

        mon->target = clamp_in_bounds(d.target);

        bolt beem;
        beem.set_target(d);
        setup_monster_throw_beam(mon, beem);
        beem.item = &mitm[mon->inv[MSLOT_MISSILE]];
        mons_throw(mon, beem, mon->inv[MSLOT_MISSILE]);
    }

    shadow_monster_reset(mon);
}

void dithmenos_shadow_spell(bolt* orig_beam, spell_type spell)
{
    if (!orig_beam)
        return;

    const coord_def target = orig_beam->target;

    if (orig_beam->target.origin()
        || (orig_beam->is_enchantment() && !is_valid_mon_spell(spell))
        || orig_beam->flavour == BEAM_ENSLAVE
           && monster_at(target) && monster_at(target)->friendly()
        || !_shadow_acts(true))
    {
        return;
    }

    monster* mon = shadow_monster();
    if (!mon)
        return;

    // Don't let shadow spells get too powerful.
    mon->set_hit_dice(max(1,
                          min(3 * spell_difficulty(spell),
                              you.experience_level) / 2));

    mon->target = clamp_in_bounds(target);
    if (actor_at(target))
        mon->foe = actor_at(target)->mindex();

    spell_type shadow_spell = spell;
    if (!orig_beam->is_enchantment())
    {
        shadow_spell = (orig_beam->pierce) ? SPELL_SHADOW_BOLT
                                           : SPELL_SHADOW_SHARD;
    }

    bolt beem;
    beem.target = target;
    beem.aimed_at_spot = orig_beam->aimed_at_spot;

    mprf(MSGCH_FRIEND_SPELL, "%s mimicks your spell!",
         mon->name(DESC_THE).c_str());
    mons_cast(mon, beem, shadow_spell, MON_SPELL_WIZARD, false);

    shadow_monster_reset(mon);
}

static void _wu_jian_trigger_serpents_lash(const coord_def& old_pos,
                                           bool wall_jump)
{
    if (you.attribute[ATTR_SERPENTS_LASH] == 0)
       return;

    if (wall_jump && you.attribute[ATTR_SERPENTS_LASH] == 1)
    {
        // No turn manipulation, since we are only refunding half a wall jump's
        // time (the walk speed modifier for this special case is already
        // factored in main.cc)
        you.attribute[ATTR_SERPENTS_LASH] = 0;
    }
    else
    {
        you.turn_is_over = false;
        you.elapsed_time_at_last_input = you.elapsed_time;
        you.attribute[ATTR_SERPENTS_LASH] -= wall_jump ? 2 : 1;
        you.redraw_status_lights = true;
        update_turn_count();
    }

    if (you.attribute[ATTR_SERPENTS_LASH] == 0)
    {
        you.increase_duration(DUR_EXHAUSTED, 12 + random2(5));
        mpr("Your supernatural speed expires.");
    }

    if (!cell_is_solid(old_pos))
        check_place_cloud(CLOUD_DUST, old_pos, 2 + random2(3) , &you, 1, -1);
}

void wu_jian_heaven_tick()
{
    if (you.attribute[ATTR_HEAVENLY_STORM] == 0)
        return;

    // TODO: this is still ridiculous. REWRITEME!
    if (you.attribute[ATTR_HEAVENLY_STORM] <= 10)
        you.attribute[ATTR_HEAVENLY_STORM] -= 1;
    else if (you.attribute[ATTR_HEAVENLY_STORM] <= 15)
        you.attribute[ATTR_HEAVENLY_STORM] -= 2;
    else if (you.attribute[ATTR_HEAVENLY_STORM] <= 20)
        you.attribute[ATTR_HEAVENLY_STORM] -= 3;
    else if (you.attribute[ATTR_HEAVENLY_STORM] <= 30)
        you.attribute[ATTR_HEAVENLY_STORM] -= 5;
    else
        you.attribute[ATTR_HEAVENLY_STORM] -= 10;

    for (radius_iterator ai(you.pos(), 2, C_SQUARE, LOS_SOLID); ai; ++ai)
    {
        if (!cell_is_solid(*ai))
            place_cloud(CLOUD_GOLD_DUST, *ai, 5 + random2(5), &you);
    }

    noisy(15, you.pos());

    if (you.attribute[ATTR_HEAVENLY_STORM] == 0)
        end_heavenly_storm();
    else
        you.duration[DUR_HEAVENLY_STORM] = WU_JIAN_HEAVEN_TICK_TIME;
}

void end_heavenly_storm()
{
    you.attribute[ATTR_HEAVENLY_STORM] = 0;
    mprf(MSGCH_GOD, "The heavenly storm settles.");
}

bool wu_jian_has_momentum(wu_jian_attack_type attack_type)
{
    return you.attribute[ATTR_SERPENTS_LASH]
           && attack_type != WU_JIAN_ATTACK_NONE
           && attack_type != WU_JIAN_ATTACK_TRIGGERED_AUX;
}

static bool _can_attack_martial(const monster* mons)
{
    return !(mons->wont_attack()
             || mons_is_firewood(*mons)
             || mons_is_projectile(mons->type)
             || !you.can_see(*mons));
}

// A mismatch between attack speed and move speed may cause any particular
// martial attack to be doubled, tripled, or not happen at all. Given enough
// time moving, you would have made the same amount of attacks as tabbing.
static int _wu_jian_number_of_attacks(bool wall_jump)
{
    // Under the effect of serpent's lash, move delay is normalized to
    // 10 aut for every character, to avoid punishing fast races.
    const int move_delay = you.attribute[ATTR_SERPENTS_LASH]
                           ? 100
                           : player_movement_speed() * player_speed();

    int attack_delay;

    {
        // attack_delay() is dependent on you.time_taken, which won't be set
        // appropriately during a movement turn. This temporarily resets
        // you.time_taken to the initial value (see `_prep_input`) used for
        // basic, simple, melee attacks.
        // TODO: can `attack_delay` be changed to not depend on you.time_taken?
        unwind_var<int> reset_speed(you.time_taken, player_speed());
        attack_delay = you.attack_delay().roll();
    }

    return div_rand_round(wall_jump ? 2 * move_delay : move_delay,
                          attack_delay * BASELINE_DELAY);
}

static bool _wu_jian_lunge(const coord_def& old_pos)
{
    coord_def lunge_direction = (you.pos() - old_pos).sgn();
    coord_def potential_target = you.pos() + lunge_direction;
    monster* mons = monster_at(potential_target);

    if (!mons || !_can_attack_martial(mons) || !mons->alive())
        return false;

    if (you.attribute[ATTR_HEAVENLY_STORM] > 0)
        you.attribute[ATTR_HEAVENLY_STORM] += 2;

    you.apply_berserk_penalty = false;

    const int number_of_attacks = _wu_jian_number_of_attacks(false);

    if (number_of_attacks == 0)
    {
        mprf("You lunge at %s, but your attack speed is too slow for a blow "
             "to land.", mons->name(DESC_THE).c_str());
        return false;
    }
    else
    {
        mprf("You lunge%s at %s%s.",
             wu_jian_has_momentum(WU_JIAN_ATTACK_LUNGE) ?
                 " with incredible momentum" : "",
             mons->name(DESC_THE).c_str(),
             number_of_attacks > 1 ? ", in a flurry of attacks" : "");
    }

    count_action(CACT_INVOKE, ABIL_WU_JIAN_LUNGE);

    for (int i = 0; i < number_of_attacks; i++)
    {
        if (!mons->alive())
            break;
        melee_attack lunge(&you, mons);
        lunge.wu_jian_attack = WU_JIAN_ATTACK_LUNGE;
        lunge.attack();
    }

    return true;
}

// Monsters adjacent to the given pos that are valid targets for whirlwind.
static vector<monster*> _get_whirlwind_targets(coord_def pos)
{
    vector<monster*> targets;
    for (adjacent_iterator ai(pos, true); ai; ++ai)
        if (monster_at(*ai) && _can_attack_martial(monster_at(*ai)))
            targets.push_back(monster_at(*ai));
    sort(targets.begin(), targets.end());
    return targets;
}

static bool _wu_jian_whirlwind(const coord_def& old_pos)
{
    bool did_at_least_one_attack = false;

    const vector<monster*> targets = _get_whirlwind_targets(you.pos());
    if (targets.empty())
        return did_at_least_one_attack;

    const vector<monster*> old_targets = _get_whirlwind_targets(old_pos);
    vector<monster*> common_targets;
    set_intersection(targets.begin(), targets.end(),
                     old_targets.begin(), old_targets.end(),
                     back_inserter(common_targets));

    for (auto mons : common_targets)
    {
        if (!mons->alive())
            continue;

        if (you.attribute[ATTR_HEAVENLY_STORM] > 0)
            you.attribute[ATTR_HEAVENLY_STORM] += 2;

        // Pin has a longer duration than one player turn, but gets cleared
        // before its duration expires by wu_jian_end_of_turn_effects. This is
        // necessary to make sure it works well with Wall Jump's longer aut
        // count.
        mons->del_ench(ENCH_WHIRLWIND_PINNED);
        mons->add_ench(mon_enchant(ENCH_WHIRLWIND_PINNED, 2, nullptr,
                                   BASELINE_DELAY * 5));

        you.apply_berserk_penalty = false;

        const int number_of_attacks = _wu_jian_number_of_attacks(false);
        if (number_of_attacks == 0)
        {
            mprf("You spin to attack %s, but your attack speed is too slow for "
                 "a blow to land.", mons->name(DESC_THE).c_str());
            continue;
        }
        else
        {
            mprf("You spin and attack %s%s%s.",
                 mons->name(DESC_THE).c_str(),
                 number_of_attacks > 1 ? " repeatedly" : "",
                 wu_jian_has_momentum(WU_JIAN_ATTACK_WHIRLWIND) ?
                     ", with incredible momentum" : "");
        }

        count_action(CACT_INVOKE, ABIL_WU_JIAN_WHIRLWIND);

        for (int i = 0; i < number_of_attacks; i++)
        {
            if (!mons->alive())
                break;
            melee_attack whirlwind(&you, mons);
            whirlwind.wu_jian_attack = WU_JIAN_ATTACK_WHIRLWIND;
            whirlwind.wu_jian_number_of_targets = common_targets.size();
            whirlwind.attack();
            if (!did_at_least_one_attack)
              did_at_least_one_attack = true;
        }
    }

    return did_at_least_one_attack;
}

static bool _wu_jian_trigger_martial_arts(const coord_def& old_pos)
{
    bool did_wu_jian_attacks = false;

    if (you.pos() == old_pos || you.duration[DUR_CONF])
        return did_wu_jian_attacks;

    if (have_passive(passive_t::wu_jian_lunge))
        did_wu_jian_attacks = _wu_jian_lunge(old_pos);

    if (have_passive(passive_t::wu_jian_whirlwind))
        did_wu_jian_attacks |= _wu_jian_whirlwind(old_pos);

    return did_wu_jian_attacks;
}

void wu_jian_wall_jump_effects(const coord_def& old_pos)
{
    vector<monster*> targets;
    for (adjacent_iterator ai(you.pos(), true); ai; ++ai)
    {
        monster* target = monster_at(*ai);
        if (target && _can_attack_martial(target) && target->alive())
            targets.push_back(target);

        if (!cell_is_solid(*ai))
            check_place_cloud(CLOUD_DUST, *ai, 1 + random2(3) , &you, 0, -1);
    }

    for (auto target : targets)
    {
        if (!target->alive())
            continue;

        if (you.attribute[ATTR_HEAVENLY_STORM] > 0)
            you.attribute[ATTR_HEAVENLY_STORM] += 2;

        you.apply_berserk_penalty = false;

        // Twice the attacks as Wall Jump spends twice the time
        const int number_of_attacks = _wu_jian_number_of_attacks(true);
        if (number_of_attacks == 0)
        {
            mprf("You attack %s from above, but your attack speed is too slow"
                 " for a blow to land.", target->name(DESC_THE).c_str());
            continue;
        }
        else
        {
            mprf("You %sattack %s from above%s.",
                 number_of_attacks > 1 ? "repeatedly " : "",
                 target->name(DESC_THE).c_str(),
                 wu_jian_has_momentum(WU_JIAN_ATTACK_WALL_JUMP) ?
                     ", with incredible momentum" : "");
        }

        for (int i = 0; i < number_of_attacks; i++)
        {
            if (!target->alive())
                break;

            melee_attack aerial(&you, target);
            aerial.wu_jian_attack = WU_JIAN_ATTACK_WALL_JUMP;
            aerial.wu_jian_number_of_targets = targets.size();
            aerial.attack();
        }
    }
}

void wu_jian_end_of_turn_effects()
{
    // This guarantees that the whirlwind pin status is capped to one turn of
    // monster movement.
    for (monster_iterator mi; mi; ++mi)
        if (mi->has_ench(ENCH_WHIRLWIND_PINNED)
            && !you.attribute[ATTR_SERPENTS_LASH])
        {
            mi->lose_ench_levels(mi->get_ench(ENCH_WHIRLWIND_PINNED), 1, true);
        }

    you.attribute[ATTR_WALL_JUMP_READY] = 0;
}

bool wu_jian_post_move_effects(bool did_wall_jump,
                               const coord_def& initial_position)
{
    bool did_wu_jian_attacks = false;

    if (!did_wall_jump)
        did_wu_jian_attacks = _wu_jian_trigger_martial_arts(initial_position);

    if (you.turn_is_over)
        _wu_jian_trigger_serpents_lash(initial_position, did_wall_jump);

    return did_wu_jian_attacks;
}

/**
 * check if the monster in this cell exists and is a valid target for Uskayaw
 */
static int _check_for_uskayaw_targets(coord_def where)
{
    if (!cell_has_valid_target(where))
        return 0;
    monster* mons = monster_at(where);
    ASSERT(mons);

    if (mons_is_firewood(*mons))
        return 0;

    return 1;
}

/**
 * Paralyze the monster in this cell, assuming one exists.
 *
 * Duration increases with invocations and experience level, and decreases
 * with target HD. The duration is pretty low, maxing out at 40 AUT.
 */
static int _prepare_audience(coord_def where)
{
    if (!_check_for_uskayaw_targets(where))
        return 0;

    monster* mons = monster_at(where);

    int power =  max(1, random2(1 + you.skill(SK_INVOCATIONS, 2))
                 + you.experience_level - mons->get_hit_dice());
    int duration = min(max(10, 5 + power), 40);
    mons->add_ench(mon_enchant(ENCH_PARALYSIS, 1, &you, duration));

    return 1;
}

/**
 * On hitting *** piety, all the monsters are paralysed by their appreciation
 * for your dance.
 */
void uskayaw_prepares_audience()
{
    int count = apply_area_visible(_check_for_uskayaw_targets, you.pos());
    if (count > 0)
    {
        simple_god_message(" prepares the audience for your solo!");
        apply_area_visible(_prepare_audience, you.pos());

        // Increment a delay timer to prevent players from spamming this ability
        // via piety loss and gain. Timer is in AUT.
        you.props[USKAYAW_AUDIENCE_TIMER] = 300 + random2(201);
    }
    else // Reset the timer because we didn't actually execute.
        you.props[USKAYAW_AUDIENCE_TIMER] = 0;
}

/**
 * Apply pain bond to the monster in this cell.
 */
static int _bond_audience(coord_def where)
{
    if (!_check_for_uskayaw_targets(where))
        return 0;

    monster* mons = monster_at(where);

    int power = you.skill(SK_INVOCATIONS, 7) + you.experience_level
                 - mons->get_hit_dice();
    int duration = 20 + random2avg(power, 2);
    mons->add_ench(mon_enchant(ENCH_PAIN_BOND, 1, &you, duration));

    return 1;
}

/**
 * On hitting **** piety, all the monsters are pain bonded.
 */
void uskayaw_bonds_audience()
{
    int count = apply_area_visible(_check_for_uskayaw_targets, you.pos());
    if (count > 1)
    {
        simple_god_message(" links your audience in an emotional bond!");
        apply_area_visible(_bond_audience, you.pos());

        // Increment a delay timer to prevent players from spamming this ability
        // via piety loss and gain. Timer is in AUT.
        you.props[USKAYAW_BOND_TIMER] = 300 + random2(201);
    }
    else // Reset the timer because we didn't actually execute.
        you.props[USKAYAW_BOND_TIMER] = 0;
}
