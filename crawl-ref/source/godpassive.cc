#include "AppHdr.h"

#include "godpassive.h"

#include <algorithm>
#include <cmath>

#include "artefact.h"
#include "art-enum.h"
#include "branch.h"
#include "cloud.h"
#include "coordit.h"
#include "directn.h"
#include "env.h"
#include "fight.h"
#include "files.h"
#include "food.h"
#include "fprop.h"
#include "godabil.h"
#include "goditem.h"
#include "godprayer.h"
#include "invent.h" // in_inventory
#include "itemname.h"
#include "itemprop.h"
#include "items.h"
#include "libutil.h"
#include "melee_attack.h"
#include "message.h"
#include "mon-cast.h"
#include "mon-death.h"
#include "mon-place.h"
#include "mon-util.h"
#include "player-equip.h"
#include "religion.h"
#include "shout.h"
#include "skills.h"
#include "state.h"
#include "stringutil.h"
#include "terrain.h"
#include "throw.h"

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
    const char* gain;
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

static const vector<god_passive> god_passives[NUM_GODS] =
{
    // no god
    { },

    // Zin
    {
        { -1, passive_t::protect_from_harm, "GOD sometimes watches over you" },
        { -1, passive_t::resist_mutation, "GOD can shield you from mutations" },
        { -1, passive_t::resist_polymorph,
              "GOD can protect you from unnatural transformations" },
        { -1, passive_t::resist_hell_effects,
              "GOD can protect you from effects of Hell" },
        { -1, passive_t::warn_shapeshifter,
              "GOD will warn you about shapeshifters" },
    },

    // TSO
    {
        { -1, passive_t::protect_from_harm, "GOD sometimes watches over you" },
        { -1, passive_t::abjuration_protection_hd,
              "GOD protects your summons from abjuration" },
        { -1, passive_t::bless_followers_vs_evil,
              "GOD blesses your followers when they kill evil beings"
        },
        { -1, passive_t::restore_hp_mp_vs_evil,
              "gain health and magic from killing evil beings" },
        {  0, passive_t::halo, "are surrounded by divine halo" },
    },

    // Kikubaaqudgha
    {
        {  2, passive_t::miscast_protection_necromancy,
              "GOD protects you from necromancy miscasts and mummy death curses"
        },
        {  4, passive_t::resist_torment, "GOD protects you from torment" },
    },

    // Yredelemnul
    {
        {  3, passive_t::nightvision, "can see well in the dark" },
    },

    // Xom
    { },

    // Vehumet
    {
        { -1, passive_t::mp_on_kill, "have a chance to gain mana when you kill" },
        {  3, passive_t::spells_success, "are less likely to miscast destructive spells" },
        {  4, passive_t::spells_range, "can cast destructive spells farther" },
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
        {  2, passive_t::miscast_protection, "GOD protects you from miscasts" },
    },

    // Trog
    {
        { -1, passive_t::abjuration_protection,
              "GOD protects your allies from abjuration" },
        {  0, passive_t::extend_berserk, "can maintain berserk longer and you "
                                         "are less likely to pass out" },
    },

    // Nemelex
    {
        {  0, passive_t::cards_power, "cards are more powerful" }
    },

    // Elyvilon
    {
        { -1, passive_t::protect_from_harm, "GOD sometimes watches over you" },
        { -1, passive_t::protect_ally, "can protect the life of your allies" },
    },

    // Lugonu
    {
        { -1, passive_t::safe_distortion,
              "protected from distortion unwield effects" },
        { -1, passive_t::map_rot_res_abyss,
              "remember the shape of the Abyss better" },
        {  5, passive_t::attract_abyssal_rune,
              "GOD will help you find the Abyssal rune.",
              "GOD will no longer help you find the Abyssal rune."
        },
    },

    // Beogh
    {
        { -1, passive_t::share_exp, "share experience with your followers" },
        {  3, passive_t::convert_orcs, "inspire orcs to join your side" },
        {  3, passive_t::bless_followers,
              "GOD will bless your followers.",
              "GOD will no longer bless your followers."
        },
        {  5, passive_t::water_walk, "walk on water" },
    },

    // Jiyva
    {
        { -1, passive_t::neutral_slimes, "slimes and eye monsters are neutral towards you" },
        { -1, passive_t::jellies_army, "GOD summons jellies to protect you" },
        { -1, passive_t::jelly_eating, "GOD allows jellies to devour items" },
        { -1, passive_t::fluid_stats, "GOD adjusts your attributes periodically" },
        {  2, passive_t::slime_feed, "items consumed by your fellow slimes feed you" },
        {  3, passive_t::resist_corrosion, "GOD protects your from corrosion" },
        {  4, passive_t::slime_mp, "items consumed by your fellow slimes restores your mana reserve" },
        {  5, passive_t::slime_hp, "items consumed by your fellow slimes restores your health" },
        {  6, passive_t::unlock_slime_vaults, "GOD grants you access to the hidden treasures of the Slime Pits" },
    },

    // Fedhas
    {
        { -1, passive_t::pass_through_plants, "can walk through plants" },
        { -1, passive_t::shoot_through_plants, "can safely fire through allied plants" },
        {  0, passive_t::friendly_plants, "Allied plants are friendly towards you" },
    },

    // Cheibriados
    {
        { -1, passive_t::no_haste, "are protected from inadvertent hurry" },
        { -1, passive_t::slowed, "move less quickly" },
        {  0, passive_t::slow_orb_run,
              "GOD will aid your escape with the Orb of Zot.",
              "GOD will no longer aid your escape with the Orb of Zot."
        },
        {  0, passive_t::stat_boost,
              "GOD supports your attributes",
              "GOD no longer supports your attributes",
        },
        {  0, passive_t::slow_abyss,
              "GOD will slow the abyss.",
              "GOD will no longer slow the abyss."
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
        {  0, passive_t::search_traps, "are better at searching for traps" },
        {  2, passive_t::bondage_skill_boost,
              "get a skill boost from cursed items" },
        {  3, passive_t::sinv, "are clear of vision" },
        {  4, passive_t::clarity, "are clear of mind" },
    },

    // Dithmenos
    {
        {  1, passive_t::nightvision, "can see well in the dark" },
        {  1, passive_t::umbra, "are surrounded by an umbra" },
        // TODO: this one should work regardless of penance.
        {  3, passive_t::hit_smoke, "emit smoke when hit" },
        {  4, passive_t::shadow_attacks,
              "Your attacks are mimicked by a shadow.",
              "Your attacks are no longer mimicked by a shadow."
        },
        {  4, passive_t::shadow_spells,
              "Your attack spells are mimicked by a shadow.",
              "Your attack spells are no longer mimicked by a shadow."
        },
    },

    // Gozag
    {
        { -1, passive_t::detect_gold, "detect gold" },
        {  0, passive_t::goldify_corpses, "GOD turns all corpses to gold." },
        {  0, passive_t::gold_aura, "have a gold aura" },
    },

    // Qazlal
    {
        {  0, passive_t::cloud_immunity, "clouds can't harm you" },
        {  1, passive_t::storm_shield, "generate elemental clouds to protect you" },
        {  4, passive_t::upgraded_storm_shield, "chances to be struck by projectiles are reduced" },
        {  5, passive_t::elemental_adaptation, "elemental attacks leaves you somewhat more resistant to themxo" }
    },

    // Ru
    {
        {  1, passive_t::aura_of_power, "your enemies will sometime fail their attack or even hit themselves" },
        {  2, passive_t::upgraded_aura_of_power, "enemies that inflict damage upon you will sometime receive a detrimental status effect" },
    },

    // Pakellas
    {
        { -1, passive_t::no_mp_regen, "GOD prevents you from regenerating your mana reserve" },
        { -1, passive_t::mp_on_kill, "have a chance to gain mana when you kill" },
        {  0, passive_t::identify_devices, "GOD identifies your wands and rods" },
        {  1, passive_t::bottle_mp, "GOD collects and distills excess magic from your kills" },
    },

    // Uskayaw
    { },

    // Hepliaklqana
    {
        { -1, passive_t::frail, "GOD siphons a part of your essence into your ancestor" },
        {  5, passive_t::transfer_drain, "drain nearby creatures when transferring your ancestor" },
    },

    // Ieoh Jian
    {
        { 0, passive_t::spawn_weapon_on_hit, "your melee attacks can spawn flying weapons nearby" },
        { 2, passive_t::martial_weapon_mastery, "perform acrobatic attacks on the move with melee weapons" },
        { 3, passive_t::afterimage, "leave a distracting afterimage after swapping weapons" },
        { 5, passive_t::pressure_points, "slow and paralyse foes as you attack while moving" },
    },
};

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
                 && player_mutation_level(MUT_MISSING_HAND))
        {
            continue;
        }
        // Octopodes don't count these slots:
        else if (you.species == SP_OCTOPODE
                 && ((i == EQ_LEFT_RING || i == EQ_RIGHT_RING)
                     || (i == EQ_RING_EIGHT
                         && player_mutation_level(MUT_MISSING_HAND))))
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
                            || player_mutation_level(MUT_MISSING_HAND)))
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

        if (is_weapon(item) || item.base_type == OBJ_RODS
            || item.base_type == OBJ_ARMOUR)
        {
            ided |= ISFLAG_KNOW_PROPERTIES | ISFLAG_KNOW_TYPE;
        }

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
            && !item_is_branded(item)
            && item.base_type != OBJ_RODS)
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
    const int map_radius = LOS_RADIUS + 1;

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
                 && item_is_evokable(*wpn, false, false, false, false, false))
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

int ash_skill_boost(skill_type sk, int scale)
{
    // It gives a bonus to skill points. The formula is:
    // factor * (piety_rank + 1) * skill_level
    // low bonus    -> factor = 3
    // medium bonus -> factor = 5
    // high bonus   -> factor = 7

    unsigned int skill_points = you.skill_points[sk];

    for (skill_type cross : get_crosstrain_skills(sk))
        skill_points += you.skill_points[cross] * 2 / 5;

    skill_points += (you.skill_boost[sk] * 2 + 1) * (piety_rank() + 1)
                    * max(you.skill(sk, 10, true), 1) * species_apt_factor(sk);

    int level = you.skills[sk];
    while (level < 27 && skill_points >= skill_exp_needed(level + 1, sk))
        ++level;

    level = level * scale + get_skill_progress(sk, level, skill_points, scale);

    return min(level, 27 * scale);
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
// with Qazlal's boost. Here you.attribute[ATTR_DIVINE_SHIELD] was set
// to 3 + you.skill_rdiv(SK_INVOCATIONS, 1, 5) (and decreases at end of dur).
int tso_sh_boost()
{
    if (!you.duration[DUR_DIVINE_SHIELD])
        return 0;

    return you.attribute[ATTR_DIVINE_SHIELD] * 4;
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

/**
 * ID the charges of wands and rods in the player's possession.
 */
void pakellas_id_device_charges()
{
    for (int which_item = 0; which_item < ENDOFPACK; which_item++)
    {
        if (!you.inv[which_item].defined()
            || !(you.inv[which_item].base_type == OBJ_WANDS
                 || you.inv[which_item].base_type == OBJ_RODS)
            || item_ident(you.inv[which_item], ISFLAG_KNOW_PLUSES)
               && item_ident(you.inv[which_item], ISFLAG_KNOW_TYPE))
        {
            continue;
        }
        if (you.inv[which_item].base_type == OBJ_RODS)
            set_ident_flags(you.inv[which_item], ISFLAG_KNOW_TYPE);
        set_ident_flags(you.inv[which_item], ISFLAG_KNOW_PLUSES);
        mprf_nocap("%s",
                   menu_colour_item_name(you.inv[which_item],
                                               DESC_INVENTORY).c_str());
    }
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
    const int dist = (you.pos() - target->pos()).abs();
    return dist < 2 || (dist <= 2 && you.reach_range() != REACH_NONE);
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

// Returns references to all IEOH JIAN weapon monsters, sorted by age (slot).
vector<monster*> find_ieoh_jian_manifested_weapons(bool owned_by_you)
{
    vector<monster*> monsters;

    if (!owned_by_you)
    {
        for (auto& monster: menv)
            if (monster.type == MONS_IEOH_JIAN_WEAPON && monster.weapon() && monster.weapon()->props.exists(IEOH_JIAN_SLOT))
                monsters.emplace_back(&monster);

        std::sort(monsters.begin(), monsters.end(), [](monster* a, monster* b) {
                return (a->weapon()->props[IEOH_JIAN_SLOT].get_int() < b->weapon()->props[IEOH_JIAN_SLOT].get_int());
        });
    }
    else
    {
        for (auto& monster: menv)
            if (monster.type == MONS_IEOH_JIAN_WEAPON && monster.weapon() && !monster.weapon()->props.exists(IEOH_JIAN_SLOT))
                monsters.emplace_back(&monster);
    }

    return monsters;
}

// Gives weights to Ieoh Jian weapons and brands based on an arbitrary tier, the player level and piety.
//
// The chance to manifest certain "tiers" of weapons and brands keeps changing as you increase in level and 
// piety. Tiers overlap to some extent until you end up only receiving weapons and brands from the highest tier.
static int _weight_by_tier(int tier)
{
    int level = you.get_experience_level();
    int effective_level = level + 2 * piety_rank(you.piety); // up to 39
    int weight = 0;
    switch (tier)
    {
    case 0:
        weight = max(0, 15 - abs(effective_level - 10));
        break;
    case 1:
        weight = max(0, 15 - abs(effective_level - 20));
        break;
    case 2:
        weight = max(0, 15 - abs(effective_level - 30));
        break;
    default:
        break;
    }

    return weight;
}

static const int _ieoh_jian_num_weapons = 35;

static const FixedVector<int, _ieoh_jian_num_weapons> _ieoh_jian_weapon_types
(
    // M&F
    WPN_WHIP,
    WPN_MACE,
    WPN_FLAIL,
    WPN_MORNINGSTAR,
    WPN_DIRE_FLAIL,
    WPN_EVENINGSTAR,
    WPN_GREAT_MACE,
    WPN_DEMON_WHIP,
    WPN_GIANT_CLUB,
    WPN_GIANT_SPIKED_CLUB,

    // SB
    WPN_DAGGER,
    WPN_QUICK_BLADE,
    WPN_SHORT_SWORD,
    WPN_RAPIER,

    // LB
    WPN_FALCHION,
    WPN_LONG_SWORD,
    WPN_SCIMITAR,
    WPN_GREAT_SWORD,
    WPN_DEMON_BLADE,
    WPN_DOUBLE_SWORD,
    WPN_TRIPLE_SWORD,

    // Polearms
    WPN_SPEAR,
    WPN_TRIDENT,
    WPN_HALBERD,
    WPN_SCYTHE,
    WPN_GLAIVE,
    WPN_BARDICHE,
    WPN_DEMON_TRIDENT,

    // Axes
    WPN_HAND_AXE,
    WPN_WAR_AXE,
    WPN_BROAD_AXE,
    WPN_BATTLEAXE,
    WPN_EXECUTIONERS_AXE,

    // Staves
    WPN_QUARTERSTAFF,
    WPN_LAJATANG
);

static item_def ieoh_jian_choose_weapon()
{
    FixedVector<int, _ieoh_jian_num_weapons> weights
    (
        //// M&F
        _weight_by_tier(0), //WPN_WHIP,
        2 * _weight_by_tier(0),//WPN_MACE,

        _weight_by_tier(0),//WPN_FLAIL,
        _weight_by_tier(1),//WPN_MORNINGSTAR,
        _weight_by_tier(1),//WPN_DIRE_FLAIL,
        _weight_by_tier(1),//WPN_EVENINGSTAR,
        _weight_by_tier(1),//WPN_GREAT_MACE,
        4 * _weight_by_tier(2),//WPN_DEMON_WHIP,
        2 * _weight_by_tier(1),//WPN_GIANT_CLUB,
        2 * _weight_by_tier(2),//WPN_GIANT_SPIKED_CLUB,

        //// SB
        2 * _weight_by_tier(0),//WPN_DAGGER,
        2 * _weight_by_tier(1) + 3 * _weight_by_tier(2),//WPN_QUICK_BLADE,
        2 * _weight_by_tier(0),//WPN_SHORT_SWORD,
        2 * _weight_by_tier(1) + _weight_by_tier(2),//WPN_RAPIER,

        //// LB
        2 * _weight_by_tier(0),//WPN_FALCHION,
        2 * _weight_by_tier(0),//WPN_LONG_SWORD,
        2 * _weight_by_tier(1),//WPN_SCIMITAR,
        2 * _weight_by_tier(1),//WPN_GREAT_SWORD,
        _weight_by_tier(2),//WPN_DEMON_BLADE,
        2 * _weight_by_tier(2),//WPN_DOUBLE_SWORD,
        _weight_by_tier(2),//WPN_TRIPLE_SWORD,

        //// Polearms
        _weight_by_tier(0),//WPN_SPEAR,
        _weight_by_tier(0),//WPN_TRIDENT,
        _weight_by_tier(0),//WPN_HALBERD,
        _weight_by_tier(0) + _weight_by_tier(1),//WPN_SCYTHE,
        2 * _weight_by_tier(1),//WPN_GLAIVE,
        _weight_by_tier(1) + _weight_by_tier(2),//WPN_BARDICHE,
        3 * _weight_by_tier(2),//WPN_DEMON_TRIDENT,

        //// Axes
        3 * _weight_by_tier(0),//WPN_HAND_AXE,
        _weight_by_tier(0),//WPN_WAR_AXE,
        2 * _weight_by_tier(1),//WPN_BROAD_AXE,
        2 * _weight_by_tier(1),//WPN_BATTLEAXE,
        4 * _weight_by_tier(2),//WPN_EXECUTIONERS_AXE,

        //// Polearms
        4 * _weight_by_tier(0) + _weight_by_tier(1),//WPN_QUARTERSTAFF,
        3 * _weight_by_tier(1) + 4 * _weight_by_tier(2)//WPN_LAJATANG
    );

    dprf("Choosing with the following tier weights: %d %d %d",
         _weight_by_tier(0), _weight_by_tier(1), _weight_by_tier(2));
    auto manifested = find_ieoh_jian_manifested_weapons(false);

    // We get rid of all base types for which there is already a manifested IJC weapon.
    for (auto monster: manifested)
        for (int i = 0; i != _ieoh_jian_num_weapons; i++)
            if (weapon_attack_skill((weapon_type)_ieoh_jian_weapon_types[i]) == weapon_attack_skill(monster->weapon()->sub_type))
                weights[i] = 0;
    if (you.weapon() && you.weapon()->props.exists(IEOH_JIAN_SLOT))
        for (int i = 0; i != _ieoh_jian_num_weapons; i++)
            if (weapon_attack_skill((weapon_type)_ieoh_jian_weapon_types[i]) == weapon_attack_skill(you.weapon()->sub_type))
                weights[i] = 0;

    // We take out all types that our race can't wield.
    for (int i = 0; i != _ieoh_jian_num_weapons; i++)
        if (!you_could_wield_weapon_type((weapon_type)_ieoh_jian_weapon_types[i]))
                weights[i] = 0;

    item_def weapon;
    weapon.base_type = OBJ_WEAPONS;
    weapon.sub_type = _ieoh_jian_weapon_types[random_choose_weighted(weights)];
    weapon.quantity = 1;

    // From 0 to 9, piety based, with some variance.
    weapon.plus = piety_rank(you.piety) + random2(3);

    FixedVector<int, 3> brand_weights
    (
        _weight_by_tier(0), // No brand
        _weight_by_tier(1), // vorpal
        _weight_by_tier(2) // speed
    );

    switch (random_choose_weighted(brand_weights))
    {
        case 0:
            weapon.brand = SPWPN_NORMAL;
            break;
        case 1:
            weapon.brand = SPWPN_VORPAL;
            break;
        case 2:
            weapon.brand = SPWPN_SPEED;
            break;
        default:
            break;
    }

    set_ident_type(weapon, true);

    return weapon;
}

// Keeps Ieoh Jian interested and returns TRUE if they'd be fine with spawning a new weapon.
bool ieoh_jian_interest()
{
    if(you.duration[DUR_IEOH_JIAN_BOREDOM] > 0)
        you.duration[DUR_IEOH_JIAN_BOREDOM] = 0;

    you.duration[DUR_IEOH_JIAN_INTEREST] = IEOH_JIAN_ATTENTION_SPAN;
   
    auto manifested = find_ieoh_jian_manifested_weapons(false);
    auto yours = find_ieoh_jian_manifested_weapons(true);
    manifested.insert(manifested.end(), yours.begin(), yours.end());
    auto manifested_num = manifested.size();

    if (manifested_num >= IEOH_JIAN_WEAPON_SLOTS)
        return false;

    if ((you.duration[DUR_IEOH_JIAN_ACTIVITY_BACKOFF] == 0))
        return true;

    return false;
}

void ieoh_jian_despawn_weapon()
{
    // We kill any ordinary IJC weapons first, in order of age.
    if (ieoh_jian_kill_oldest_weapon())
        return;

    // If there aren't any left, but there is an animated weapon belonging to
    // the player, we pull it back to the player's hand, killing the ghost.
    // This has the side effect of shattering the IJC weapon that the player was
    // wielding, if applicable.
    auto monsters = find_ieoh_jian_manifested_weapons(true);
    if (!monsters.empty())
    {
        auto monster = monsters.at(0);
        monster->ieoh_jian_swap_weapon_with_player(true);

        if (you.weapon())
            mprf(MSGCH_GOD, "%s flies back to your hands!", you.weapon()->name(DESC_YOUR, false, true).c_str());
        else
            mprf(MSGCH_GOD, "%s flies back to your hands, but you're too encumbered to catch it!", monster->weapon()->name(DESC_YOUR, false, true).c_str());

        if (monster->alive())
            monster_die(monster, KILL_RESET, NON_MONSTER);

        return;
    }

    // Finally, if the player is wielding an ICJ weapon, it is shattered.
    if (you.weapon() && you.weapon()->props.exists(IEOH_JIAN_SLOT))
    {
        mprf("%s shatters in your hands!", you.weapon()->name(DESC_YOUR, false, true).c_str());
        int inventory_index = you.equip[EQ_WEAPON];
        unwield_item(false, true);
        dec_inv_item_quantity(inventory_index, 1);
        check_place_cloud(CLOUD_DUST, you.pos(), 2 + random2(4), &you, 5 + random2(15), -1);
    }

    // The backoff is cleared so you can quickly climb back from a lost weapon.
    you.duration[DUR_IEOH_JIAN_ACTIVITY_BACKOFF] = 0;
}

void ieoh_jian_spawn_weapon(const coord_def& position)
{
    // We check if the ICJ is interested in helping by sending more weapons. 
    if (!ieoh_jian_interest())
        return;

    auto manifested = find_ieoh_jian_manifested_weapons(false);
    auto theirs_num = manifested.size();
    auto yours = find_ieoh_jian_manifested_weapons(true);
    manifested.insert(manifested.end(), yours.begin(), yours.end());

    item_def wpn = ieoh_jian_choose_weapon();
    wpn.props[IEOH_JIAN_SLOT] = (int)(theirs_num + 1);

    mgen_data mg(MONS_IEOH_JIAN_WEAPON,
                 BEH_FRIENDLY,
                 position,
                 MHITYOU,
                 MG_FORCE_BEH | MG_FORCE_PLACE,
                 GOD_IEOH_JIAN);
    mg.props[IEOH_JIAN_WEAPON] = wpn;

    int power = you.skill(weapon_attack_skill(wpn.sub_type), 4, false);
    mg.props[IEOH_JIAN_POWER] = power;

    monster * const mons = create_monster(mg);

    if (!mons)
    {
        dprf("Failed to animate Ieoh Jian weapon");
        return;
    }

    you.duration[DUR_IEOH_JIAN_ACTIVITY_BACKOFF] = 1.8 * IEOH_JIAN_ATTENTION_SPAN * (1 + theirs_num);
    mprf(MSGCH_GOD, "%s manifests from thin air!", wpn.name(DESC_A, false, true).c_str());
}

static bool _dont_attack_martial(const monster* mons)
{
    return mons->friendly() || mons->good_neutral() || mons->strict_neutral();
}

static void _ieoh_jian_lunge(const coord_def& old_pos)
{
    coord_def lunge_direction = (you.pos() - old_pos).sgn();
    coord_def potential_target = you.pos() + lunge_direction;
    monster* mons = monster_at(potential_target);

    if (!mons || _dont_attack_martial(mons))
        return;
   
    mprf("You lunge at %s!", mons->name(DESC_THE).c_str());
    melee_attack lunge(&you, mons);
    lunge.is_ieoh_jian_martial = true;
    lunge.attack();
}

static void _ieoh_jian_whirlwind(const coord_def& old_pos)
{
    vector<monster*> targets;

    coord_def dir = coord_def(1,0);
    for (int i = 0; i < 8; ++i)
    {
        monster* target = monster_at(you.pos() + dir);
        if (target && !_dont_attack_martial(target))
            targets.push_back(target);

        dir = rotate_adjacent(dir, 1);
    }

    if (targets.empty())
        return;

    vector<monster*> old_targets;
    for (int i = 0; i < 8; ++i)
    {
        monster* target = monster_at(old_pos + dir);
        if (target && !_dont_attack_martial(target))
            old_targets.push_back(target);

        dir = rotate_adjacent(dir, 1);
    }

    sort(targets.begin(), targets.end()); 
    sort(old_targets.begin(), old_targets.end()); 
    vector<monster*> common_targets;
    set_intersection(targets.begin(), targets.end(), 
                     old_targets.begin(), old_targets.end(),
                     back_inserter(common_targets));

    for (auto mons : common_targets)
    {
        mprf("You spin and strike %s!", mons->name(DESC_THE).c_str());
        melee_attack whirlwind(&you, mons);
        whirlwind.is_ieoh_jian_martial = true;
        whirlwind.attack();
    }
}

void ieoh_jian_perform_martial_attacks(const coord_def& old_pos)
{
    if (!you.weapon())
        return;

    auto weapon_skill = weapon_attack_skill(you.weapon()->sub_type);

    if (weapon_skill == SK_SHORT_BLADES || weapon_skill == SK_AXES)
        _ieoh_jian_lunge(old_pos);

    if (weapon_skill == SK_LONG_BLADES || weapon_skill == SK_MACES_FLAILS)
        _ieoh_jian_whirlwind(old_pos);
}

bool ieoh_jian_can_pole_vault(const coord_def& target)
{
   bool able = have_passive(passive_t::martial_weapon_mastery)
                                  && feat_can_pole_vault_against(grd(target))
                                  && you.weapon() 
                                  && (weapon_attack_skill(you.weapon()->sub_type) == SK_STAVES
                                      || weapon_attack_skill(you.weapon()->sub_type) == SK_POLEARMS);
   
   if (!able) 
       return false;

   auto pole_vault_direction = (you.pos() - target).sgn();
   auto pole_vault_landing_spot = (you.pos() + pole_vault_direction + pole_vault_direction);

    if (feat_is_solid(grd(you.pos() + pole_vault_direction))
        || !you.is_habitable(pole_vault_landing_spot)
        || actor_at(pole_vault_landing_spot))
    {
        mprf("You have no room to pole vault.");
        return false;
    }

    return true;
}

void ieoh_jian_pole_vault_effects()
{
    coord_def dir = coord_def(1,0);

    for (int i = 0; i < 8; ++i)
    {
        monster* target = monster_at(you.pos() + dir);
        if (target && !_dont_attack_martial(target))
        {
            mprf("You attack %s while airborne!", target->name(DESC_THE).c_str());
            melee_attack aerial(&you, target);
            aerial.is_ieoh_jian_martial = true;
            aerial.attack();
        }

        if(!cell_is_solid(you.pos() + dir))
            check_place_cloud(CLOUD_DUST, you.pos() + dir, 1 + random2(3) , &you, 0, -1);
        dir = rotate_adjacent(dir, 1);
    }
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
