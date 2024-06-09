#include "AppHdr.h"

#include "god-passive.h"

#include <algorithm>
#include <cmath>

#include "act-iter.h"
#include "areas.h"
#include "artefact.h"
#include "art-enum.h"
#include "branch.h"
#include "chardump.h"
#include "cloud.h"
#include "coordit.h"
#include "directn.h"
#include "env.h"
#include "fight.h"
#include "files.h"
#include "fineff.h"
#include "fprop.h"
#include "god-abil.h"
#include "god-prayer.h"
#include "invent.h" // in_inventory
#include "item-name.h"
#include "item-prop.h"
#include "item-status-flag-type.h"
#include "items.h"
#include "libutil.h"
#include "los.h"
#include "losglobal.h"
#include "map-knowledge.h"
#include "melee-attack.h"
#include "message.h"
#include "mon-cast.h"
#include "mon-death.h"
#include "mon-place.h"
#include "mon-util.h"
#include "output.h"
#include "player.h"
#include "religion.h"
#include "shout.h"
#include "skills.h"
#include "spl-clouds.h"
#include "spl-zap.h"
#include "state.h"
#include "stringutil.h"
#include "tag-version.h"
#include "terrain.h"
#include "throw.h"
#include "unwind.h"

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
              "GOD NOW shields you from mutations"
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
        {  -1, passive_t::umbra, "are NOW surrounded by an aura of darkness" },
        {  -1, passive_t::reaping, "can NOW harvest souls to fight along side you" },
        {  -1, passive_t::nightvision, "can NOW see well in the dark" },
        {  -1, passive_t::r_spectral_mist, "are NOW immune to spectral mist" },
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
        { -1, passive_t::no_allies,
              "are NOW prevented from gaining allies" },
    },

    // Makhleb
    {
        { -1, passive_t::restore_hp, "gain health from killing" },
    },

    // Sif Muna
    { },

    // Trog
    {
        { -1, passive_t::abjuration_protection,
              "GOD NOW protects your allies from abjuration"
        },
        {  0, passive_t::extend_berserk,
              "GOD NOW extends your berserk rage on killing"
        },
    },

    // Nemelex
    {
        // None
    },

    // Elyvilon
    {
        { -1, passive_t::lifesaving,
              "GOD carefully watches over you",
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
        { -1, passive_t::wrath_banishment,
              "GOD will NOW banish foes whenever another god meddles" },
        { -1, passive_t::map_rot_res_abyss,
              "remember the shape of the Abyss better" },
        {  5, passive_t::attract_abyssal_rune,
              "GOD will NOW help you find the Abyssal rune" },
    },

    // Beogh
    {
        {  1, passive_t::convert_orcs, "Orcs sometimes recognise you as one of their own" },
        {  5, passive_t::water_walk, "walk on water" },
    },

    // Jiyva
    {
        { -1, passive_t::neutral_slimes,
              "Slimes and eye monsters are NOW neutral towards you" },
        { -1, passive_t::jellies_army,
              "GOD NOW summons slimes to protect you" },
        { -1, passive_t::jelly_eating,
              "GOD NOW allows slimes to devour items" },
        {  0, passive_t::slime_wall_immune,
              "are NOW immune to slime covered walls" },
        {  1, passive_t::jelly_regen,
              "GOD NOW accelerates your HP and MP regeneration" },
        {  2, passive_t::resist_corrosion,
              "GOD NOW protects you from corrosion" },
        {  5, passive_t::spawn_slimes_on_hit,
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
        // TODO: this one should work regardless of penance, maybe?
        {  0, passive_t::slow_zot,
              "GOD will NOW slow Zot's hunt for you"
        },
        {  0, passive_t::slow_poison, "process poison slowly" },
    },

    // Ashenzari
    {
        { -1, passive_t::want_curses, "prefer cursed items" },
        {  0, passive_t::detect_portals, "sense portals" },
        {  0, passive_t::detect_montier, "sense threats" },
        {  0, passive_t::detect_items, "sense items" },
        {  0, passive_t::bondage_skill_boost,
              "get a skill boost from cursed items" },
        {  1, passive_t::identify_items, "sense the properties of items" },
        {  2, passive_t::sinv, "are NOW clear of vision" },
        {  3, passive_t::clarity, "are NOW clear of mind" },
        {  4, passive_t::avoid_traps, "avoid traps" },
        {  4, passive_t::scrying,
              "reveal the structure of the nearby dungeon" },
    },

    // Dithmenos
    {
        {  1, passive_t::dampen_noise,
              "You NOW dampen all noise in your surroundings." },
        {  2, passive_t::shadow_attacks,
              "Your attacks are NOW mimicked by a shadow" },
        {  2, passive_t::shadow_spells,
              "Your attack spells are NOW mimicked by a shadow" },
    },

    // Gozag
    {
        {  0, passive_t::goldify_corpses,
              "GOD NOW turns all corpses to gold" },
        {  0, passive_t::gold_aura, "have a gold aura" },
    },

    // Qazlal
    {
        {  0, passive_t::cloud_immunity, "and your divine allies are ADV immune to clouds" },
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

#if TAG_MAJOR_VERSION == 34
    // Pakellas
    { },
#endif

    // Uskayaw
    { },

    // Hepliaklqana
    {
        {  1, passive_t::frail,
              "GOD NOW siphons a part of your essence into your ancestor" },
        {  5, passive_t::transfer_drain,
              "drain nearby creatures when transferring your ancestor" },
    },

    // Wu Jian
    {
        { 0, passive_t::wu_jian_lunge, "perform damaging attacks by moving towards foes." },
        { 1, passive_t::wu_jian_whirlwind, "lightly attack monsters by moving around them." },
        { 2, passive_t::wu_jian_wall_jump, "perform airborne attacks in an area by jumping off a solid obstacle." },
    },

    // Ignis
    {
        { 0, passive_t::resist_fire, "resist fire." },
    }, // TODO
};
COMPILE_CHECK(ARRAYSZ(god_passives) == NUM_GODS);

static bool _god_gives_passive_if(god_type god, passive_t passive,
                                  function<bool(const god_passive&)> condition)
{
    const auto &pasvec = god_passives[god];
    return any_of(begin(pasvec), end(pasvec),
                  [&] (const god_passive &p) -> bool
                  { return p.pasv == passive && condition(p); });
}

bool god_gives_passive(god_type god, passive_t passive)
{
  return _god_gives_passive_if(god, passive,
                               [] (god_passive /*p*/) { return true; });
}

bool have_passive(passive_t passive)
{
    return _god_gives_passive_if(you.religion, passive,
                                 [] (god_passive p)
                                 {
                                     return piety_rank() >= p.rank
                                         && (!player_under_penance()
                                             || p.rank < 0);
                                 });
}

bool will_have_passive(passive_t passive)
{
  return _god_gives_passive_if(you.religion, passive,
                               [] (god_passive/*p*/) { return true; });
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

        if (lid == level_id::current() || !you.level_visited(lid))
            continue;

        dprf("Checking %s", lid.describe().c_str());

        level_excursion le;
        le.go_to(lid);
        while (true)
        {
            if (one_chance_in(200))
                break;

            const coord_def p = random_in_bounds();

            if (env.igrid(p) == NON_ITEM || testbits(env.pgrid(p), FPROP_NO_JIYVA))
                continue;

            for (stack_iterator si(p); si; ++si)
            {
                if (!item_is_jelly_edible(*si) || one_chance_in(4))
                    continue;

                if (one_chance_in(4))
                    break;

                dprf("Eating %s on %s",
                     si->name(DESC_PLAIN).c_str(), lid.describe().c_str());

                mpr("You hear a distant slurping noise.");
                item_was_destroyed(*si);
                destroy_item(si.index());
            }
            return;
        }
    }
}

static bool _two_handed()
{
    const item_def* wpn = you.slot_item(EQ_WEAPON, true);
    if (!wpn)
        return false;

    hands_reqd_type wep_type = you.hands_reqd(*wpn, true);
    return wep_type == HANDS_TWO;
}

static void _curse_boost_skills(const item_def &item)
{
    if (!item.props.exists(CURSE_KNOWLEDGE_KEY))
        return;

    for (auto& curse : item.props[CURSE_KNOWLEDGE_KEY].get_vector())
    {
        for (skill_type sk : curse_skills(curse))
        {
            if (you.skill_boost.count(sk))
                you.skill_boost[sk]++;
            else
                you.skill_boost[sk] = 1;
        }
    }
}

// Checks bondage and sets ash piety
void ash_check_bondage()
{
    if (!will_have_passive(passive_t::bondage_skill_boost))
        return;

#if TAG_MAJOR_VERSION == 34
    // Save compatibility for the new ash tag minor forgot to do this
    initialize_ashenzari_props();
#endif

    int num_cursed = 0, num_slots = 0;

    you.skill_boost.clear();
    for (int j = EQ_FIRST_EQUIP; j < NUM_EQUIP; j++)
    {
        const equipment_type i = static_cast<equipment_type>(j);

        // handles missing hand, octopode ring slots, finger necklace, species
        // armour restrictions, etc. Finger necklace slot counts.
        if (!you_can_wear(i))
            continue;

        // Coglin gizmo isn't a normal slot, not cursable.
        if (j == EQ_GIZMO)
            continue;

        // transformed away slots are still considered to be possibly bound
        num_slots++;
        if (you.equip[i] != -1)
        {
            const item_def& item = you.inv[you.equip[i]];
            if (item.cursed() && (i != EQ_WEAPON || is_weapon(item)))
            {
                if (i == EQ_WEAPON && _two_handed())
                    num_cursed += 2;
                else
                {
                    num_cursed++;
                    if (i == EQ_BODY_ARMOUR
                        && is_unrandom_artefact(item, UNRAND_LEAR))
                    {
                        num_cursed += 3;
                    }
                }
                if (!item_is_melded(item))
                    _curse_boost_skills(item);
            }
        }
    }

    set_piety(ASHENZARI_BASE_PIETY
              + (num_cursed * ASHENZARI_PIETY_SCALE) / num_slots);

    calc_hp(true);
    calc_mp(true);
}

// XXX: If this is called on an item in inventory, then auto_assign_item_slot
// needs to be called subsequently. However, moving an item in inventory
// invalidates its reference, which is a different behavior than for floor
// items, so we don't do it in this function.
bool god_id_item(item_def& item, bool silent)
{
    iflags_t old_ided = item.flags & ISFLAG_IDENT_MASK;

    if (have_passive(passive_t::identify_items))
    {
        // Don't identify runes, gems, or the orb, since this has no purpose
        // and might mess up other things.
        if (item_is_collectible(item))
            return false;

        if ((item.base_type == OBJ_JEWELLERY || item.base_type == OBJ_STAVES)
            && item_needs_autopickup(item))
        {
            item.props[NEEDS_AUTOPICKUP_KEY] = true;
        }
        set_ident_type(item, true);
        set_ident_flags(item, ISFLAG_IDENT_MASK);
    }

    iflags_t ided = item.flags & ISFLAG_IDENT_MASK;

    if (ided & ~old_ided)
    {
        if (item.props.exists(NEEDS_AUTOPICKUP_KEY) && is_useless_item(item))
            item.props.erase(NEEDS_AUTOPICKUP_KEY);

        if (&item == you.weapon())
            you.wield_change = true;

        if (!silent)
            mprf_nocap("%s", item.name(DESC_INVENTORY_EQUIP).c_str());

        seen_item(item);
        return true;
    }

    // nothing new
    return false;
}

static bool is_ash_portal(dungeon_feature_type feat)
{
    if (feat_is_portal_entrance(feat))
        return true;
    switch (feat)
    {
    case DNGN_ENTER_HELL:
    case DNGN_EXIT_DIS:
    case DNGN_EXIT_GEHENNA:
    case DNGN_EXIT_COCYTUS:
    case DNGN_EXIT_TARTARUS:
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
    const dungeon_feature_type feat = env.grid(where);
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
    const int scale = 10;
    const int skill_boost = scale * (you.skill_boost[sk] * 4 + 3) / 3;

    skill_points += skill_boost * (piety_rank() + 1) * max(scaled_skill, 1)
                    * species_apt_factor(sk) / scale;
    return skill_points;
}

int ash_skill_boost(skill_type sk, int scale)
{
    // It gives a bonus to skill points. The formula is:
    // ( curses * 4 / 3 + 1 ) * (piety_rank + 1) * skill_level

    unsigned int skill_points = you.skill_points[sk]
                  + get_crosstrain_points(sk)
                  + ash_skill_point_boost(sk, you.skill(sk, 10, true));

    int level = you.skills[sk];
    while (level < MAX_SKILL_LEVEL && skill_points >= skill_exp_needed(level + 1, sk))
        ++level;

    level = level * scale + get_skill_progress(sk, level, skill_points, scale);

    return min(level, MAX_SKILL_LEVEL * scale);
}

void ash_scrying()
{
    if (have_passive(passive_t::scrying))
    {
        magic_mapping(LOS_MAX_RANGE, 100, true, true, false, false, false,
                      you.pos());
    }
}

void gozag_move_level_gold_to_top()
{
    if (you_worship(GOD_GOZAG))
    {
        for (rectangle_iterator ri(0); ri; ++ri)
            gozag_move_gold_to_top(*ri);
    }
}

void gozag_move_gold_to_top(const coord_def p)
{
    for (int gold = env.igrid(p); gold != NON_ITEM;
         gold = env.item[gold].link)
    {
        if (env.item[gold].base_type == OBJ_GOLD)
        {
            unlink_item(gold);
            move_item_to_grid(&gold, p, true);
            break;
        }
    }
}

void gozag_count_level_gold()
{
    ASSERT(you.on_current_level);
    vector<coord_def> gold_places;
    int gold = 0;
    for (rectangle_iterator ri(0); ri; ++ri)
    {
        for (stack_iterator j(*ri); j; ++j)
        {
            if (j->base_type == OBJ_GOLD && !(j->flags & ISFLAG_UNOBTAINABLE))
            {
                gold += j->quantity;
                gold_places.push_back(*ri);
            }
        }
    }

    if (!player_in_branch(BRANCH_ABYSS))
        you.attribute[ATTR_GOLD_GENERATED] += gold;

    if (you_worship(GOD_GOZAG))
        for (auto pos : gold_places)
            gozag_move_gold_to_top(pos);
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
            if (feat_is_water(env.grid(*ai)))
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
        case BEAM_THUNDER:
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
bool does_ru_wanna_redirect(const monster &mon)
{
    return have_passive(passive_t::aura_of_power)
            && !mon.friendly()
            && you.see_cell_no_trans(mon.pos())
            && !mons_is_firewood(mon)
            && !mons_is_projectile(mon.type);
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

constexpr int DITH_MELEE_SHADOW_INTERVAL = 150;
#define DITH_SHADOW_INTERVAL_KEY "dith_shadow_interval"

static bool _shadow_will_act(bool spell, bool melee)
{
    const passive_t pasv = spell ? passive_t::shadow_spells
                                 : passive_t::shadow_attacks;
    if (!have_passive(pasv))
        return false;

    // If we're making a weapon shadow and haven't done so for a while,
    // guarantee it. (This helps consistency when fishing for shadowslip stab
    // opportunities a lot.)
    const int last_shadow = (you.props.exists(DITH_SHADOW_INTERVAL_KEY)
                                ? you.props[DITH_SHADOW_INTERVAL_KEY].get_int()
                                : 0);

    if (melee && you.elapsed_time > last_shadow)
        return true;

    const int minpiety = piety_breakpoint(rank_for_passive(pasv) - 1);

    // For attacks: 10% chance at minimum piety, 20% chance at 160 piety.
    // For spells:  15% chance at min piety, 25% chance at 160 piety.
    const int range = piety_breakpoint(5) - minpiety;
    return x_chance_in_y(10 + (spell ? 5 : 0)
                          + ((you.piety - minpiety) * 10 / range), 100);

    return true;
}

void dithmenos_cleanup_player_shadow(monster* shadow)
{
    if (shadow && dithmenos_get_player_shadow() == shadow)
        you.props.erase(DITH_SHADOW_MID_KEY);
}

monster* dithmenos_get_player_shadow()
{
    if (!you.props.exists(DITH_SHADOW_MID_KEY))
        return nullptr;
    const int mid = you.props[DITH_SHADOW_MID_KEY].get_int();
    return monster_by_mid(mid);
}

monster* create_player_shadow(coord_def pos, bool friendly, spell_type spell_known)
{
    // If a player shadow already exists, remove it first.
    // But save some state, in case it was currently in decoy mode.
    int decoy_duration = 0;
    for (monster_iterator mi; mi; ++mi)
    {
        if (mons_is_player_shadow(**mi))
        {
            if (mi->has_ench(ENCH_CHANGED_APPEARANCE))
                decoy_duration = mi->get_ench(ENCH_CHANGED_APPEARANCE).duration;
            monster_die(**mi, KILL_RESET, true);
            break;
        }
    }

    // Do a basic clone of the weapon (same base type, but no brand or plusses).
    // Also magical staves become plain staves instead.
    int wpn_index  = NON_ITEM;
    item_def* wpn = you.weapon();
    if (wpn && is_weapon(*wpn))
    {
        wpn_index = get_mitm_slot(10);
        if (wpn_index == NON_ITEM)
            return nullptr;
        item_def& new_item = env.item[wpn_index];
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

    mgen_data mg(MONS_PLAYER_SHADOW, friendly ? BEH_FRIENDLY : BEH_HOSTILE,
                 pos, MHITYOU, MG_AUTOFOE);
    mg.flags |= MG_FORCE_PLACE;

    // This is for weapon accuracy only. Spell HD is calculated differently.
    mg.hd = 7 + (you.experience_level * 5 / 4);
    mg.hp = you.experience_level + you.skill_rdiv(SK_INVOCATIONS, 3, 2);

    if (!friendly)
        mg.hp = mg.hp * 2;

    monster* mon = create_monster(mg);
    if (!mon)
    {
        if (wpn_index)
            destroy_item(wpn_index);
        return nullptr;
    }

    mon->flags      = MF_NO_REWARD | MF_JUST_SUMMONED | MF_SEEN
                    | MF_WAS_IN_VIEW | MF_HARD_RESET;

    if (wpn_index != NON_ITEM)
        mon->pickup_item(env.item[wpn_index], false, true);

    if (friendly)
    {
        mon->add_ench(mon_enchant(ENCH_FAKE_ABJURATION, 0, &you,
                                random_range(3, 5) * BASELINE_DELAY));
    }

    // Set damage based on xl, with a bonus for UC characters (since they won't
    // get the damage from their weapon on top of this)
    mon->props[DITH_SHADOW_ATTACK_KEY] = you.experience_level;
    if (wpn_index == NON_ITEM)
        mon->props[DITH_SHADOW_ATTACK_KEY].get_int() += you.skill(SK_UNARMED_COMBAT);

    if (spell_known != SPELL_NO_SPELL)
    {
        mon->spells.emplace_back(spell_known, 50, MON_SPELL_WIZARD);
        mon->props[CUSTOM_SPELLS_KEY] = true;
    }

    if (friendly)
        you.props[DITH_SHADOW_MID_KEY].get_int() = mon->mid;
    else
    {
        mon->props[DITH_SHADOW_ATTACK_KEY].get_int() += you.experience_level;
        mon->props[DITH_SHADOW_SPELLPOWER_KEY] = div_rand_round(you.experience_level * 2, 3);
    }

    // Now, if there was a previously-existing shadow that was in decoy mode,
    // update its tile and remap all existing misdirections onto the new shadow
    if (decoy_duration > 0)
    {
        dithmenos_change_shadow_appearance(*mon, decoy_duration - you.time_taken);
        for (monster_iterator mi; mi; ++mi)
        {
            if (mi->has_ench(ENCH_MISDIRECTED))
            {
                mon_enchant en = mi->get_ench(ENCH_MISDIRECTED);
                en.source = mon->mid;
                mi->update_ench(en);
            }
        }
    }

    return mon;
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

static bool _shadow_can_stand_here(coord_def pos)
{
    return in_bounds(pos) && !feat_is_solid(env.grid(pos))
            && !feat_is_trap(env.grid(pos))
            && pos != you.pos()
            // Player shadows can replace existing shadows
            && (!monster_at(pos) || mons_is_player_shadow(*monster_at(pos)))
            && you.see_cell_no_trans(pos);
}

// Attempt to find a place to put a melee player shadow to attack a given spot.
//
// It will first prefer being exactly on the opposite side of it compared to
// where the player is standing. If that is impossible, it will look for
// somewhere else in attack range of the target position, preferring to not
// be adjacent to the player while doing so (if possible).
//
// Returns (0, 0) if it could not find a spot
static coord_def _find_shadow_melee_position(coord_def target_pos, int reach)
{
    coord_def opposite = you.pos() + ((target_pos - you.pos()) * 2);

    // If the player is attacking with a reaching weapon from melee range, try
    // to make the shadow that appears behind the enemy appear non-adjacent.
    // (This helps Shadow Step with reaching weapons feel a little better.)
    if (reach > 1 && grid_distance(you.pos(), target_pos) < reach)
        opposite += (target_pos - you.pos()).sgn();

    if (_shadow_can_stand_here(opposite))
        return opposite;

    for (distance_iterator di(target_pos, true, true, reach); di; ++di)
    {
        if (grid_distance(you.pos(), *di) == 1)
            continue;

        if (_shadow_can_stand_here(*di))
            return *di;
    }

    for (distance_iterator di(target_pos, true, true, reach); di; ++di)
    {
        if (grid_distance(you.pos(), *di) > 1)
            continue;

        if (_shadow_can_stand_here(*di) && cell_see_cell(target_pos, *di, LOS_NO_TRANS))
            return *di;
    }

    return coord_def(0, 0);
}

void dithmenos_shadow_melee(actor* initial_target)
{
    if (!_shadow_will_act(false, true))
        return;

    // First, determine what we're hitting. Prefer the player's target if it's
    // still alive. If it isn't, look for something hostile that is also in
    // range of the player's melee attack.
    //
    // If we pick a target and there is no room to place a shadow in reach of
    // that target, try another target instead.
    const int reach = you.reach_range();
    monster* target = nullptr;
    coord_def pos;
    if (initial_target && initial_target->alive() && _in_melee_range(initial_target))
        pos = _find_shadow_melee_position(initial_target->pos(), reach);

    if (pos.origin())
    {
        for (distance_iterator di(you.pos(), true, true, reach); di; ++di)
        {
            if (!monster_at(*di) || mons_aligned(&you, monster_at(*di))
                || mons_is_firewood(*monster_at(*di)))
            {
                continue;
            }

            pos = _find_shadow_melee_position(*di, reach);
            if (!pos.origin())
            {
                target = monster_at(*di);
                break;
            }
        }
    }
    else
        target = initial_target->as_monster();

    // Still couldn't find a single space to stand to attack a single enemy
    if (!target)
        return;

    // Now create the shadow monster (or grab an existing one) and make it attack
    monster* mon = create_player_shadow(pos);
    if (!mon)
        return;

    mon->target     = target->pos();
    mon->foe        = target->mindex();

    fight_melee(mon, target);

    // Store this action's target so that it can be reused on future turns.
    if (target->alive())
        mon->props[DITH_SHADOW_LAST_TARGET_KEY].get_int() = target->mid;
    else
        mon->props.erase(DITH_SHADOW_LAST_TARGET_KEY);

    you.props[DITH_SHADOW_INTERVAL_KEY] = you.elapsed_time
        + random_range(DITH_MELEE_SHADOW_INTERVAL, DITH_MELEE_SHADOW_INTERVAL * 2);
}

// Determine the first monster that could be hit by an arrow fired from one spot
// to another spot (ignoring the player, since our shadow can shoot through us).
static monster* _find_shot_target(coord_def origin, coord_def aim)
{
    monster* targ = nullptr;
    ray_def ray;
    if (!find_ray(origin, aim, ray, opc_solid))
        return targ;

    while (ray.advance())
    {
        if (grid_distance(ray.pos(), origin) > LOS_RADIUS)
            break;
        else if (monster_at(ray.pos()))
            return monster_at(ray.pos());
    }

    return nullptr;
}

static coord_def _find_preferred_shadow_shoot_position(monster* target)
{
    // First, test spots at range 2-3 from the player
    vector<coord_def> spots;
    for (distance_iterator di(you.pos(), true, true, 3); di; ++di)
    {
        if (grid_distance(you.pos(), *di) == 1)
            continue;

        if (_shadow_can_stand_here(*di))
            spots.push_back(*di);
    }

    shuffle_array(spots);

    // Then test spots at range 1
    for (distance_iterator di(you.pos(), true, true, 2); di; ++di)
    {
        if (_shadow_can_stand_here(*di))
            spots.push_back(*di);
    }

    for (size_t i = 0; i < spots.size(); ++i)
    {
        if (_find_shot_target(spots[i], target->pos()) == target)
            return spots[i];
    }

    // There is no spot within shadow range that can shoot directly at the
    // preferred target.
    return coord_def();
}

static bool _simple_shot_tracer(coord_def source, coord_def target, mid_t source_mid = MID_PLAYER)
{
    bolt tracer;
    tracer.attitude = ATT_FRIENDLY;
    tracer.range = LOS_RADIUS;
    tracer.source = source;
    tracer.target = target;
    tracer.source_id = source_mid;
    tracer.damage = dice_def(100, 1);
    tracer.is_tracer = true;
    tracer.fire();

    return mons_should_fire(tracer);
}

// Gets all eligible enemy targets in view of the player, in random order
static vector<monster*> _get_shadow_targets()
{
    vector<monster*> valid_targs;
    for (monster_near_iterator mi(you.pos(), LOS_NO_TRANS); mi; ++mi)
    {
        if (you.can_see(**mi) && !mi->wont_attack() && !mons_is_firewood(**mi))
            valid_targs.push_back(*mi);
    }
    shuffle_array(valid_targs);
    return valid_targs;
}

// Gets all spots within a certain radius of the player where your shadow could
// be placed. (These will be sorted to prefer spots at distance 2-3 at random,
// with adjacent spots only at the end, so that the shadow will prefer to be
// non-adjacent, but not ignore adjacent positions if they are the only valid
// ones)
static vector<coord_def> _get_shadow_spots(bool check_silence = false)
{
    vector<coord_def> valid_spots;

    // First, test spots at range 2-3 from the player
    for (distance_iterator di(you.pos(), true, true, 3); di; ++di)
    {
        if (grid_distance(you.pos(), *di) == 1)
            continue;

        if (!_shadow_can_stand_here(*di))
            continue;

        if (check_silence && silenced(*di))
            continue;

        valid_spots.push_back(*di);
    }

    shuffle_array(valid_spots);

    // Then add adjacent-to-player spots last
    for (fair_adjacent_iterator ai(you.pos()); ai; ++ai)
    {
        if (check_silence && silenced(*ai))
            continue;

        if (_shadow_can_stand_here(*ai))
            valid_spots.push_back(*ai);
    }

    return valid_spots;
}

static coord_def _find_any_shadow_shoot_position(coord_def& aim)
{
    // Find all valid targets immediately, so we can properly iterate through
    // then in random order for each possible shadow position.
    vector<monster*> valid_targs = _get_shadow_targets();

    // Nothing to shoot at.
    if (valid_targs.empty())
        return coord_def();

    vector<coord_def> valid_spots = _get_shadow_spots();

    // Somehow, nowhere to stand.
    if (valid_spots.empty())
        return coord_def();

    // Now lets run through possible targets and see if aiming at any of
    // them does anything useful here.
    for (size_t i = 0; i < valid_spots.size(); ++i)
    {
        for (size_t j = 0; j < valid_targs.size(); ++j)
        {
            if (_simple_shot_tracer(valid_spots[i], valid_targs[j]->pos()))
            {
                aim = valid_targs[j]->pos();
                return valid_spots[i];
            }
        }
    }

    // Somehow, no valid spots can hit any valid targets
    return coord_def();
}

// Tests whether there is a still-active player shadow that is capable of
// performing a useful ranged attack from its current position at either what
// the player is attacking or at whatever it shot at on its last action.
//
// XXX: This doesn't ensure that they can actually hit the target in question,
//      since something else may have moved in the way of it since last action,
//      but that something is at least required to be another enemy worth
//      shooting at.
//
// Returns the target it's comfortable aiming at (or (0,0) if there isn't one)
static coord_def _shadow_already_okay_for_ranged(coord_def preferred_target)
{
    monster* shadow = dithmenos_get_player_shadow();
    if (!shadow)
        return coord_def();

    if (_simple_shot_tracer(shadow->pos(), preferred_target, shadow->mid))
        return preferred_target;
    else if (shadow->props.exists(DITH_SHADOW_LAST_TARGET_KEY))
    {
        monster* targ = monster_by_mid(shadow->props[DITH_SHADOW_LAST_TARGET_KEY].get_int());
        if (!targ || !targ->alive())
            return coord_def();


        if (_simple_shot_tracer(shadow->pos(), targ->pos(), shadow->mid))
            return targ->pos();
    }

    return coord_def();
}

void dithmenos_shadow_shoot(const dist &d, const item_def &item)
{
    ASSERT(d.isValid);
    if (!_shadow_will_act(false, false))
        return;
    // Determine preferred target for ranged shadow mimic by tracing along the
    // ray used by the player to fire their own shot and stopping at the first
    // hostile thing we see in that path.
    coord_def aim = d.target;
    monster* target = _find_shot_target(you.pos(), d.target);
    if (target && mons_aligned(&you, target))
        target = nullptr;

    coord_def pos;  // Position we want to place shadow if not already deployed
    coord_def existing_target; // Aim target if we are re-using shadow

    if (target)
    {
        existing_target = _shadow_already_okay_for_ranged(target->pos());
        if (!existing_target.origin())
            aim = existing_target;
        else
        {
            pos = _find_preferred_shadow_shoot_position(target);
            if (!pos.origin())
                aim = target->pos();
        }
    }

    // Either there was no preferred target, or it was not possible to aim
    // directly at the preferred target. Let's aim for any viable hostile from
    // a nearby space instead
    if (existing_target.origin() && pos.origin())
        pos = _find_any_shadow_shoot_position(aim);

    // Found nothing useful possible for the shadow to do, so do nothing
    if (existing_target.origin() && pos.origin())
        return;

    monster* mon = (existing_target.origin() ? create_player_shadow(pos)
                                             : dithmenos_get_player_shadow());

    if (!mon)
        return;

    // If we're reusing an existing shadow mimic, make sure it lasts at least
    // one turn.
    if (!existing_target.origin())
    {
        mon_enchant me = mon->get_ench(ENCH_FAKE_ABJURATION);
        me.duration += you.time_taken;
        mon->update_ench(me);
    }

    mon->props[DITH_SHADOW_ATTACK_KEY] = you.experience_level * 2 / 3;
    mon->target     = aim;
    mon->foe        = monster_at(aim)->mindex();

    // XXX: Code partially copied from handle_throw to populate fake/real
    //      projectiles, depending on whether the shadow is using a launcher
    //      or not, and whether can existing shadow is being reused or not.
    const item_def *launcher = mon->launcher();
    item_def *throwable = mon->missiles();
    item_def fake_proj;
    item_def *missile = &fake_proj;
    if (is_throwable(mon, item))
    {
        // If the shadow doesn't already have a missile item, make one for them
        if (!throwable)
        {
            const int ammo_index = get_mitm_slot(10);
            if (ammo_index == NON_ITEM)
            {
                monster_die(*mon, KILL_RESET, NON_MONSTER);
                return;
            }
            item_def& new_item = env.item[ammo_index];
            missile = &new_item;
        }

        // Set properties of the missile item it's using
        missile->base_type = item.base_type;
        missile->sub_type  = item.sub_type;
        missile->quantity  = 1;
        missile->rnd       = 1;
        missile->flags    |= ISFLAG_SUMMONED;

        if (!throwable)
            mon->pickup_item(*missile, false, true);
    }
    else if (launcher)
        populate_fake_projectile(*launcher, fake_proj);

    bolt shot;
    shot.target = aim;
    setup_monster_throw_beam(mon, shot);
    shot.item = missile;

    mons_throw(mon, shot);

    // Store this action's target so that it can be reused on future turns.
    if (monster_at(aim) && monster_at(aim)->alive())
        mon->props[DITH_SHADOW_LAST_TARGET_KEY].get_int() = monster_at(aim)->mid;
    else
        mon->props.erase(DITH_SHADOW_LAST_TARGET_KEY);
}

static int _shadow_zap_tracer(zap_type ztype, coord_def source, coord_def target)
{
    bolt tracer;

    zappy(ztype, 100, true, tracer);

    tracer.attitude = ATT_FRIENDLY;
    tracer.range = LOS_RADIUS;
    tracer.source = source;
    tracer.target = target;
    tracer.source_id = MID_PLAYER_SHADOW_DUMMY;
    tracer.is_tracer = true;
    tracer.fire();

    if (tracer.friend_info.power > 0)
        return 0;
    else
        return tracer.foe_info.power;
}

// Tries to find a vaguely 'best' spot to cast a given zap at some random
// enemy or cluster of enemies in view.
static coord_def _find_shadow_zap_position(zap_type ztype, coord_def& aim)
{
    // Find all valid targets immediately, so we can properly iterate through
    // then in random order for each possible shadow position.
    vector<monster*> valid_targs = _get_shadow_targets();

    // Nothing to shoot at.
    if (valid_targs.empty())
        return coord_def();

    // Check valid shadow spots that are NOT silenced.
    vector<coord_def> valid_spots = _get_shadow_spots(true);

    // Somehow, nowhere to stand.
    if (valid_spots.empty())
        return coord_def();

    coord_def best_spot, best_aim;
    int best_score = 0;

    // Now lets run through combinations of possible targets and positions and
    // try to find a vaguely 'best' shot.
    //
    // Since sometimes there will be a very large number of targets visible
    // (eg: Zigs, Vaults:5) without many of those targets being accessible,
    // we limit the number of successful shots we test to 10.
    int num_tests = 0;
    for (size_t i = 0; i < valid_spots.size(); ++i)
    {
        for (size_t j = 0; j < valid_targs.size(); ++j)
        {
            int score = _shadow_zap_tracer(ztype, valid_spots[i], valid_targs[j]->pos());
            if (score > best_score)
            {
                best_aim = valid_targs[j]->pos();
                best_spot = valid_spots[i];
                best_score = score;
                ++num_tests;
            }

            if (num_tests >= 10)
                break;
        }

        if (num_tests >= 10)
            break;
    }

    // Somehow, no valid spots can hit any valid targets
    if (best_aim.origin())
        return coord_def();

    aim = best_aim;
    return best_spot;
}

static coord_def _find_shadow_aoe_position(int radius, bool test_bind = false,
                                           bool near_walls = false)
{
    vector<monster*> valid_targs = _get_shadow_targets();

    // Nothing to shoot at.
    if (valid_targs.empty())
        return coord_def();

    // Check valid shadow spots that are NOT silenced.
    vector<coord_def> valid_spots = _get_shadow_spots(true);

    // Somehow, nowhere to stand.
    if (valid_spots.empty())
        return coord_def();

    coord_def best_spot;
    int best_score = 0;

    // Now let's see how many targets are in range from each shadow spot and
    // randomly pick the best of these.
    int num_tests = 0;
    for (size_t i = 0; i < valid_spots.size(); ++i)
    {
        int score = 0;
        for (size_t j = 0; j < valid_targs.size(); ++j)
        {
            if (grid_distance(valid_spots[i], valid_targs[j]->pos()) <= radius
                && cell_see_cell(valid_spots[i], valid_targs[j]->pos(), LOS_NO_TRANS)
                && (!test_bind || !valid_targs[j]->has_ench(ENCH_BOUND))
                && (!near_walls || near_visible_wall(valid_spots[i], valid_targs[j]->pos())))
            {
                score += valid_targs[j]->get_experience_level();
            }
        }

        if (score > best_score)
        {
            best_spot = valid_spots[i];
            best_score = score;
            ++num_tests;
        }
    }

    // Somehow, no valid spots can hit any valid targets
    if (best_spot.origin())
        return coord_def();

    return best_spot;
}

static coord_def _find_shadow_prism_position(coord_def& aim)
{
    // Create a sort of 'heatmap' by finding all foes, then iterating through
    // all spaces within prism explosion radius of each foe and adding that
    // foe's HD to that spot in the map. Then we can iterate to find the 'best'
    // spots to place a prism and afterward determine where to summon the shadow
    // so that it could put one there.
    //
    // (Yes, this doesn't account for monster's moving or hitting prisms, but
    // we don't need to be too smart about this. It's expected that some prisms
    // won't detonate usefully, but in exchange others will be very good)

    vector<monster*> valid_targs = _get_shadow_targets();

    // Nothing to shoot at.
    if (valid_targs.empty())
        return coord_def();

    // Check valid shadow spots that are NOT silenced (we'll use this later, but
    // we can save some work if somehow there aren't any).
    vector<coord_def> valid_spots = _get_shadow_spots(true);

    // Somehow, nowhere to stand.
    if (valid_spots.empty())
        return coord_def();

    SquareArray<int, LOS_MAX_RANGE> map;
    map.init(0);

    // Fill heatmap
    for (unsigned int i = 0; i < valid_targs.size(); ++i)
    {
        for (distance_iterator di(valid_targs[i]->pos(), false, true, 2); di; ++di)
        {
            if (actor_at(*di) || feat_is_solid(env.grid(*di)))
                continue;

            if (grid_distance(you.pos(), *di) > you.current_vision)
                continue;

            coord_def p = *di - you.pos();
            map(p) += valid_targs[i]->get_experience_level();
        }
    }

    vector<pair<coord_def, int>> targ_spots;
    for (int iy = -LOS_MAX_RANGE; iy < LOS_MAX_RANGE; ++iy)
    {
        for (int ix = -LOS_MAX_RANGE; ix < LOS_MAX_RANGE; ++ix)
        {
            coord_def p(ix, iy);
            if (map(p) > 0)
                targ_spots.emplace_back(p, map(p));
        }
    }

    // Sort valid prism spots from best to worst
    sort(targ_spots.begin(), targ_spots.end(),
            [](const pair<coord_def, int> &a, const pair<coord_def, int> &b)
                {return a.second > b.second;});

    // Try to place the shadow to cast at the best spot, but keep checking
    // other spots if somehow there is no casting position in range of them.
    for (size_t i = 0; i < targ_spots.size(); ++i)
    {
        for (size_t j = 0; j < valid_spots.size(); ++j)
        {
            coord_def p = you.pos() + targ_spots[i].first;
            if (p != valid_spots[j]
                && grid_distance(p, valid_spots[j]) <= spell_range(SPELL_SHADOW_PRISM, 100)
                && cell_see_cell(p, valid_spots[j], LOS_NO_TRANS)
                && cell_see_cell(you.pos(), valid_spots[j], LOS_NO_TRANS)
                && cell_see_cell(you.pos(), p, LOS_NO_TRANS))
            {
                aim = p;
                return valid_spots[j];
            }
        }
    }

    return coord_def();
}

static spell_type _get_shadow_spell(spell_type player_spell)
{
    vector<spell_type> spells;

    const spschools_type schools = get_spell_disciplines(player_spell);

    if (schools & spschool::fire)
        spells.push_back(SPELL_SHADOW_BALL);
    if (schools & spschool::ice)
        spells.push_back(SPELL_CREEPING_SHADOW);
    if (schools & spschool::earth)
        spells.push_back(SPELL_SHADOW_SHARD);
    if (schools & spschool::air)
        spells.push_back(SPELL_SHADOW_TEMPEST);
    if (schools & spschool::alchemy)
        spells.push_back(SPELL_SHADOW_PRISM);
    if (schools & spschool::conjuration)
        spells.push_back(SPELL_SHADOW_BEAM);
    if (schools & spschool::necromancy)
        spells.push_back(SPELL_SHADOW_DRAINING);
    if (schools & spschool::translocation)
        spells.push_back(SPELL_SHADOW_BIND);
    if (schools & spschool::hexes)
        spells.push_back(SPELL_SHADOW_TORPOR);
    if (schools & spschool::summoning)
        spells.push_back(SPELL_SHADOW_PUPPET);

    return spells[random2(spells.size())];
}

void dithmenos_shadow_spell(spell_type spell)
{
    if (!_shadow_will_act(true, false))
        return;

    spell_type shadow_spell = _get_shadow_spell(spell);

    coord_def pos, aim;

    switch (shadow_spell)
    {
        case SPELL_SHADOW_BALL:
        case SPELL_SHADOW_SHARD:
        case SPELL_SHADOW_BEAM:
        case SPELL_SHADOW_TORPOR:
            pos = _find_shadow_zap_position(spell_to_zap(shadow_spell), aim);
        break;

        case SPELL_SHADOW_PRISM:
            pos = _find_shadow_prism_position(aim);
            break;

        case SPELL_SHADOW_BIND:
            pos = _find_shadow_aoe_position(you.current_vision, true);
            break;

        case SPELL_SHADOW_DRAINING:
            pos = _find_shadow_aoe_position(2);
            break;

        case SPELL_CREEPING_SHADOW:
            pos = _find_shadow_aoe_position(4, false, true);
            break;

        case SPELL_SHADOW_TEMPEST:
            pos = _find_shadow_aoe_position(you.current_vision);

        default:
            pos = _get_shadow_spots()[0];
            break;
    }

    // If we weren't able to find a position to cast this spell usefully
    // (possibly because there are no valid targets in sight), bail.
    if (pos.origin())
        return;

    monster* mon = create_player_shadow(pos);
    if (!mon)
        return;

    mon->target = aim;
    if (monster_at(aim))
        mon->foe = monster_at(aim)->mindex();

    // Note spell, for if player examines the shadow
    mon->spells.emplace_back(shadow_spell, 200, MON_SPELL_WIZARD);
    mon->props[CUSTOM_SPELLS_KEY] = true;

    // Set spellpower based on xl and the level of spell cast
    int spell_hd = div_rand_round((you.experience_level + spell_difficulty(spell) * 4) * 2, 7);
    mon->props[DITH_SHADOW_SPELLPOWER_KEY] = spell_hd;

    bolt beam;
    setup_mons_cast(mon, beam, shadow_spell);
    beam.target = aim;
    mons_cast(mon, beam, shadow_spell, MON_SPELL_WIZARD);
}

void wu_jian_trigger_serpents_lash(bool wall_jump, const coord_def& old_pos)
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
        fire_final_effects();
    }

    if (you.attribute[ATTR_SERPENTS_LASH] == 0)
    {
        you.increase_duration(DUR_EXHAUSTED, 12 + random2(5));
        mpr("Your supernatural speed expires.");
    }

    if (!cell_is_solid(old_pos))
        check_place_cloud(CLOUD_DUST, old_pos, 2 + random2(3) , &you, 1, -1);
}

static void _wu_jian_increment_heavenly_storm()
{
    int storm = you.props[WU_JIAN_HEAVENLY_STORM_KEY].get_int();
    if (storm < WU_JIAN_HEAVENLY_STORM_MAX)
    {
        you.props[WU_JIAN_HEAVENLY_STORM_KEY].get_int()++;
        you.redraw_evasion = true;
    }
}

void wu_jian_heaven_tick()
{
    for (radius_iterator ai(you.pos(), 2, C_SQUARE, LOS_SOLID); ai; ++ai)
        if (!cell_is_solid(*ai))
            place_cloud(CLOUD_GOLD_DUST, *ai, 5 + random2(5), &you);

    noisy(12, you.pos());
}

void wu_jian_decrement_heavenly_storm()
{
    int storm = you.props[WU_JIAN_HEAVENLY_STORM_KEY].get_int();

    if (storm > 1)
    {
        you.props[WU_JIAN_HEAVENLY_STORM_KEY].get_int()--;
        you.set_duration(DUR_HEAVENLY_STORM, random_range(2, 3));
        you.redraw_evasion = true;
    }
    else
        wu_jian_end_heavenly_storm();
}

// TODO: why isn't this implemented as a duration end effect?
void wu_jian_end_heavenly_storm()
{
    you.props.erase(WU_JIAN_HEAVENLY_STORM_KEY);
    you.duration[DUR_HEAVENLY_STORM] = 0;
    you.redraw_evasion = true;
    invalidate_agrid(true);
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

    int attack_delay = you.attack_delay().roll();

    return div_rand_round(wall_jump ? 2 * move_delay : move_delay,
                          attack_delay * BASELINE_DELAY);
}

static bool _wu_jian_lunge(coord_def old_pos, coord_def new_pos,
                           bool check_only)
{
    coord_def lunge_direction = (new_pos - old_pos).sgn();
    coord_def potential_target = new_pos + lunge_direction;
    monster* mons = monster_at(potential_target);

    if (!mons || !_can_attack_martial(mons) || !mons->alive())
        return false;
    if (check_only)
        return true;

    if (you.props.exists(WU_JIAN_HEAVENLY_STORM_KEY))
        _wu_jian_increment_heavenly_storm();

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

    count_action(CACT_ABIL, ABIL_WU_JIAN_LUNGE);

    for (int i = 0; i < number_of_attacks; i++)
    {
        if (!mons->alive())
            break;
        melee_attack lunge(&you, mons);
        lunge.wu_jian_attack = WU_JIAN_ATTACK_LUNGE;
        lunge.launch_attack_set();
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

static bool _wu_jian_whirlwind(coord_def old_pos, coord_def new_pos,
                               bool check_only)
{
    const vector<monster*> targets = _get_whirlwind_targets(new_pos);
    if (targets.empty())
        return false;

    const vector<monster*> old_targets = _get_whirlwind_targets(old_pos);
    vector<monster*> common_targets;
    set_intersection(targets.begin(), targets.end(),
                     old_targets.begin(), old_targets.end(),
                     back_inserter(common_targets));

    if (check_only)
        return !common_targets.empty();

    bool did_at_least_one_attack = false;
    for (auto mons : common_targets)
    {
        if (!mons->alive())
            continue;

        if (you.props.exists(WU_JIAN_HEAVENLY_STORM_KEY))
            _wu_jian_increment_heavenly_storm();

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

        count_action(CACT_ABIL, ABIL_WU_JIAN_WHIRLWIND);

        for (int i = 0; i < number_of_attacks; i++)
        {
            if (!mons->alive())
                break;
            melee_attack whirlwind(&you, mons);
            whirlwind.wu_jian_attack = WU_JIAN_ATTACK_WHIRLWIND;
            whirlwind.wu_jian_number_of_targets = common_targets.size();
            whirlwind.launch_attack_set();
            did_at_least_one_attack = true;
        }
    }

    return did_at_least_one_attack;
}

static bool _wu_jian_trigger_martial_arts(coord_def old_pos,
                                          coord_def new_pos,
                                          bool check_only = false)
{
    if (new_pos == old_pos
        || you.duration[DUR_CONF]
        || you.weapon() && !is_melee_weapon(*you.weapon()))
    {
        return false;
    }

    bool attacked = false;

    if (have_passive(passive_t::wu_jian_lunge))
        attacked = _wu_jian_lunge(old_pos, new_pos, check_only);

    if (have_passive(passive_t::wu_jian_whirlwind))
        attacked |= _wu_jian_whirlwind(old_pos, new_pos, check_only);

    return attacked;
}

// Return a monster at pos which a wall jump could attack, nullptr if none.
monster *wu_jian_wall_jump_monster_at(const coord_def &pos)
{
    monster *target = monster_at(pos);
    if (target && target->alive() && _can_attack_martial(target))
        return target;
    return nullptr;
}

static vector<monster*> _wu_jian_wall_jump_monsters(const coord_def &pos)
{
    vector<monster*> targets;
    for (adjacent_iterator ai(pos, true); ai; ++ai)
        if (monster *target = wu_jian_wall_jump_monster_at(*ai))
            targets.push_back(target);
    return targets;
}

// Return true if wu_jian_wall_jump_effects() would attack a monster when
// called with you.pos() as pos.
bool wu_jian_wall_jump_triggers_attacks(const coord_def &pos)
{
    return !_wu_jian_wall_jump_monsters(pos).empty();
}

void wu_jian_wall_jump_effects()
{
    for (adjacent_iterator ai(you.pos(), true); ai; ++ai)
        if (!cell_is_solid(*ai))
            check_place_cloud(CLOUD_DUST, *ai, 1 + random2(3) , &you, 0, -1);

    vector<monster*> targets = _wu_jian_wall_jump_monsters(you.pos());
    for (auto target : targets)
    {
        if (!target->alive())
            continue;

        if (you.props.exists(WU_JIAN_HEAVENLY_STORM_KEY))
            _wu_jian_increment_heavenly_storm();

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
            aerial.launch_attack_set();
        }
    }
}

bool wu_jian_post_move_effects(bool did_wall_jump,
                               const coord_def& old_pos)
{
    bool attacked = false;
    if (!did_wall_jump)
        attacked = _wu_jian_trigger_martial_arts(old_pos, you.pos());

    if (you.turn_is_over)
        wu_jian_trigger_serpents_lash(did_wall_jump, old_pos);

    return attacked;
}

bool wu_jian_move_triggers_attacks(coord_def new_pos)
{
    return _wu_jian_trigger_martial_arts(you.pos(), new_pos, true);
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
 * Paralyse the monster in this cell, assuming one exists.
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

    // Don't pain bond monsters that aren't invested in fighting the player
    if (mons->wont_attack())
        return 0;

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

// Clean up after a duel ends. If we're still in the Arena, start a timer to
// kick the player back out (after they've had some time to loot), and if
// we've left the Arena via the gate, clear the timer.
void okawaru_handle_duel()
{
    if (player_in_branch(BRANCH_ARENA)
        && !okawaru_duel_active()
        && !you.duration[DUR_DUEL_COMPLETE])
    {
        you.set_duration(DUR_DUEL_COMPLETE, random_range(15, 25));
    }

    if (!player_in_branch(BRANCH_ARENA) && you.duration[DUR_DUEL_COMPLETE])
        you.duration[DUR_DUEL_COMPLETE] = 0;

}
