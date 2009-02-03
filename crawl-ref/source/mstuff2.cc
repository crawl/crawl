/*
 *  File:       mstuff2.cc
 *  Summary:    Misc monster related functions.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 */

#include "AppHdr.h"
REVISION("$Rev$");

#include "mstuff2.h"

#include <string>
#include <string.h>
#include <stdio.h>
#include <algorithm>

#include "externs.h"

#include "arena.h"
#include "beam.h"
#include "database.h"
#include "debug.h"
#include "delay.h"
#include "effects.h"
#include "item_use.h"
#include "itemname.h"
#include "itemprop.h"
#include "items.h"
#include "Kills.h"
#include "message.h"
#include "misc.h"
#include "monplace.h"
#include "monspeak.h"
#include "monstuff.h"
#include "mon-util.h"
#include "player.h"
#include "randart.h"
#include "religion.h"
#include "spells1.h"
#include "spells3.h"
#include "spells4.h"
#include "spl-cast.h"
#include "spl-mis.h"
#include "spl-util.h"
#include "state.h"
#include "stuff.h"
#include "traps.h"
#include "view.h"

static int _monster_abjuration(const monsters *caster, bool actual);

static bool _mons_abjured(monsters *monster, bool nearby)
{
    if (nearby && _monster_abjuration(monster, false) > 0
        && coinflip())
    {
        _monster_abjuration(monster, true);
        return (true);
    }

    return (false);
}

static monster_type _pick_random_wraith()
{
    static monster_type wraiths[] =
    {
        MONS_WRAITH, MONS_SHADOW_WRAITH, MONS_FREEZING_WRAITH,
        MONS_SPECTRAL_WARRIOR
    };

    return wraiths[random2(sizeof(wraiths) / sizeof(*wraiths))];
}

static monster_type _pick_horrible_thing()
{
    return (one_chance_in(4) ? MONS_TENTACLED_MONSTROSITY
                             : MONS_ABOMINATION_LARGE);
}

static monster_type _pick_undead_summon()
{
    static monster_type undead[] =
    {
        MONS_NECROPHAGE, MONS_GHOUL, MONS_HUNGRY_GHOST, MONS_FLAYED_GHOST,
        MONS_ZOMBIE_SMALL, MONS_SKELETON_SMALL, MONS_SIMULACRUM_SMALL,
        MONS_FLYING_SKULL, MONS_FLAMING_CORPSE, MONS_MUMMY, MONS_VAMPIRE,
        MONS_WIGHT, MONS_WRAITH, MONS_SHADOW_WRAITH, MONS_FREEZING_WRAITH,
        MONS_SPECTRAL_WARRIOR, MONS_ZOMBIE_LARGE, MONS_SKELETON_LARGE,
        MONS_SIMULACRUM_LARGE, MONS_SHADOW
    };

    return undead[random2(sizeof(undead) / sizeof(*undead))];
}

static void _do_high_level_summon(monsters *monster, bool monsterNearby,
                                  spell_type spell_cast,
                                  monster_type (*mpicker)(), int nsummons)
{
    if (_mons_abjured(monster, monsterNearby))
        return;

    const int duration = std::min(2 + monster->hit_dice / 5, 6);

    for (int i = 0; i < nsummons; ++i)
    {
        monster_type which_mons = mpicker();

        if (which_mons == MONS_PROGRAM_BUG)
            continue;

        create_monster(
            mgen_data(which_mons, SAME_ATTITUDE(monster),
                      duration, spell_cast, monster->pos(), monster->foe));
    }
}

static bool _los_free_spell(spell_type spell_cast)
{
    return (spell_cast == SPELL_HELLFIRE_BURST
        || spell_cast == SPELL_BRAIN_FEED
        || spell_cast == SPELL_SMITING
        || spell_cast == SPELL_FIRE_STORM
        || spell_cast == SPELL_AIRSTRIKE);
}

void mons_cast(monsters *monster, bolt &pbolt, spell_type spell_cast,
               bool do_noise)
{
    // Always do setup.  It might be done already, but it doesn't
    // hurt to do it again (cheap).
    setup_mons_cast(monster, pbolt, spell_cast);

    // single calculation permissible {dlb}
    bool monsterNearby = mons_near(monster);

    int sumcount = 0;
    int sumcount2;
    int duration = 0;

#if DEBUG_DIAGNOSTICS
    mprf(MSGCH_DIAGNOSTICS, "Mon #%d casts %s (#%d)",
         monster_index(monster), spell_title(spell_cast), spell_cast);
#endif

    if (_los_free_spell(spell_cast) && !spell_is_direct_explosion(spell_cast))
    {
        if (monster->foe == MHITYOU || monster->foe == MHITNOT)
        {
            if (monsterNearby)
            {
                if (do_noise)
                    mons_cast_noise(monster, pbolt, spell_cast);
                direct_effect(monster, spell_cast, pbolt, &you);
            }
            return;
        }

        if (do_noise)
            mons_cast_noise(monster, pbolt, spell_cast);
        direct_effect(monster, spell_cast, pbolt, monster->get_foe());
        return;
    }

#ifdef DEBUG
    const unsigned int flags = get_spell_flags(spell_cast);

    ASSERT(!(flags & (SPFLAG_TESTING | SPFLAG_MAPPING)));

    // Targeted spells need a valid target.
    ASSERT(!(flags & SPFLAG_TARGETING_MASK) || in_bounds(pbolt.target));
#endif

    if (do_noise)
        mons_cast_noise(monster, pbolt, spell_cast);

    switch (spell_cast)
    {
    default:
        break;

    case SPELL_GREATER_HEALING:
        if (heal_monster(monster, 50 + random2avg(monster->hit_dice * 10, 2),
                         false))
        {
            simple_monster_message(monster, " is healed.");
        }
        return;

    case SPELL_BERSERKER_RAGE:
        monster->go_berserk(true);
        return;

    case SPELL_SUMMON_SMALL_MAMMALS:
    case SPELL_VAMPIRE_SUMMON:
        if (spell_cast == SPELL_SUMMON_SMALL_MAMMALS)
            sumcount2 = 1 + random2(4);
        else
            sumcount2 = 3 + random2(3) + monster->hit_dice / 5;

        for (sumcount = 0; sumcount < sumcount2; ++sumcount)
        {
            monster_type mon = MONS_GIANT_BAT;

            if (!one_chance_in(3))
            {
                int temp_rand = random2(4);

                mon = (temp_rand == 0) ? MONS_ORANGE_RAT :
                      (temp_rand == 1) ? MONS_GREEN_RAT :
                      (temp_rand == 2) ? MONS_GREY_RAT
                                       : MONS_RAT;

            }

            create_monster(
                mgen_data(mon, SAME_ATTITUDE(monster),
                          5, spell_cast, monster->pos(), monster->foe));
        }
        return;

    case SPELL_SHADOW_CREATURES:       // summon anything appropriate for level
        if (_mons_abjured(monster, monsterNearby))
            return;

        sumcount2 = 1 + random2(4) + random2(monster->hit_dice / 7 + 1);

        for (sumcount = 0; sumcount < sumcount2; ++sumcount)
        {
            create_monster(
                mgen_data(RANDOM_MONSTER, SAME_ATTITUDE(monster),
                          5, spell_cast, monster->pos(), monster->foe));
        }
        return;

    case SPELL_WATER_ELEMENTALS:
        if (_mons_abjured(monster, monsterNearby))
            return;

        sumcount2 = 1 + random2(4) + random2(monster->hit_dice / 7 + 1);

        for (sumcount = 0; sumcount < sumcount2; sumcount++)
        {
            create_monster(
                mgen_data(MONS_WATER_ELEMENTAL, SAME_ATTITUDE(monster), 3,
                          spell_cast, monster->pos(), monster->foe));
        }
        return;

    case SPELL_FAKE_RAKSHASA_SUMMON:
        sumcount2 = (coinflip() ? 2 : 3);

        for (sumcount = 0; sumcount < sumcount2; sumcount++)
        {
            create_monster(
                mgen_data(MONS_RAKSHASA_FAKE, SAME_ATTITUDE(monster),
                          3, spell_cast, monster->pos(), monster->foe));
        }
        return;

    case SPELL_SUMMON_DEMON: // class 2-4 demons
        if (_mons_abjured(monster, monsterNearby))
            return;

        sumcount2 = 1 + random2(2) + random2(monster->hit_dice / 10 + 1);

        duration  = std::min(2 + monster->hit_dice / 10, 6);
        for (sumcount = 0; sumcount < sumcount2; sumcount++)
        {
            create_monster(
                mgen_data(summon_any_demon(DEMON_COMMON),
                          SAME_ATTITUDE(monster), duration, spell_cast,
                          monster->pos(), monster->foe));
        }
        return;

    case SPELL_SUMMON_UGLY_THING:
        if (_mons_abjured(monster, monsterNearby))
            return;

        sumcount2 = 1 + random2(2) + random2(monster->hit_dice / 10 + 1);

        duration  = std::min(2 + monster->hit_dice / 10, 6);
        for (sumcount = 0; sumcount < sumcount2; ++sumcount)
        {
            const int chance = std::max(6 - (monster->hit_dice / 6), 1);
            monster_type mon = (one_chance_in(chance) ? MONS_VERY_UGLY_THING
                                                      : MONS_UGLY_THING);

            create_monster(
                mgen_data(mon, SAME_ATTITUDE(monster),
                          duration, spell_cast, monster->pos(), monster->foe));
        }
        return;

    case SPELL_ANIMATE_DEAD:
        // see special handling in monstuff::handle_spell() {dlb}
        animate_dead(monster, 5 + random2(5), SAME_ATTITUDE(monster),
                     monster->foe);
        return;

    case SPELL_CALL_IMP: // class 5 demons
        sumcount2 = 1 + random2(3) + random2(monster->hit_dice / 5 + 1);

        duration  = std::min(2 + monster->hit_dice / 5, 6);
        for (sumcount = 0; sumcount < sumcount2; ++sumcount)
        {
            create_monster(
                mgen_data(summon_any_demon(DEMON_LESSER),
                          SAME_ATTITUDE(monster),
                          duration, spell_cast, monster->pos(), monster->foe));
        }
        return;

    case SPELL_SUMMON_SCORPIONS:
        if (_mons_abjured(monster, monsterNearby))
            return;

        sumcount2 = 1 + random2(3) + random2( monster->hit_dice / 5 + 1);

        duration  = std::min(2 + monster->hit_dice / 5, 6);
        for (sumcount = 0; sumcount < sumcount2; ++sumcount)
        {
            create_monster(
                mgen_data(MONS_SCORPION, SAME_ATTITUDE(monster),
                          duration, spell_cast, monster->pos(), monster->foe));
        }
        return;

    case SPELL_SUMMON_UFETUBUS:
        sumcount2 = 2 + random2(2) + random2(monster->hit_dice / 5 + 1);

        duration  = std::min(2 + monster->hit_dice / 5, 6);

        for (sumcount = 0; sumcount < sumcount2; ++sumcount)
        {
            create_monster(
                mgen_data(MONS_UFETUBUS, SAME_ATTITUDE(monster),
                          duration, spell_cast, monster->pos(), monster->foe));
        }
        return;

    case SPELL_SUMMON_BEAST:       // Geryon
        create_monster(
            mgen_data(MONS_BEAST, SAME_ATTITUDE(monster),
                      4, spell_cast, monster->pos(), monster->foe));
        return;

    case SPELL_SUMMON_ICE_BEAST:
        create_monster(
            mgen_data(MONS_ICE_BEAST, SAME_ATTITUDE(monster),
                      5, spell_cast, monster->pos(), monster->foe));
        return;

    case SPELL_SUMMON_MUSHROOMS:   // Summon swarms of icky crawling fungi.
        if (_mons_abjured(monster, monsterNearby))
            return;

        sumcount2 = 1 + random2(2) + random2(monster->hit_dice / 4 + 1);

        duration  = std::min(2 + monster->hit_dice / 5, 6);
        for (int i = 0; i < sumcount2; ++i)
        {
            create_monster(
                mgen_data(MONS_WANDERING_MUSHROOM, SAME_ATTITUDE(monster),
                          duration, spell_cast, monster->pos(), monster->foe));
        }
        return;

    case SPELL_SUMMON_WRAITHS:
        _do_high_level_summon(monster, monsterNearby, spell_cast,
                              _pick_random_wraith, random_range(3, 6));
        return;

    case SPELL_SUMMON_HORRIBLE_THINGS:
        _do_high_level_summon(monster, monsterNearby, spell_cast,
                              _pick_horrible_thing, random_range(3, 5));
        return;

    case SPELL_CONJURE_BALL_LIGHTNING:
    {
        const int n = 2 + random2(monster->hit_dice / 4);
        for (int i = 0; i < n; ++i)
        {
            create_monster(
                mgen_data(MONS_BALL_LIGHTNING, SAME_ATTITUDE(monster),
                          2, spell_cast, monster->pos(), monster->foe));
        }
        return;
    }

    case SPELL_SUMMON_UNDEAD:      // Summon undead around player.
        _do_high_level_summon(monster, monsterNearby, spell_cast,
                              _pick_undead_summon,
                              2 + random2(2)
                                + random2(monster->hit_dice / 4 + 1));
        return;

    case SPELL_SYMBOL_OF_TORMENT:
        if (!monsterNearby || mons_friendly(monster))
            return;

        torment(monster_index(monster), monster->pos());
        return;

    case SPELL_SUMMON_GREATER_DEMON:
        if (_mons_abjured(monster, monsterNearby))
            return;

        sumcount2 = 1 + random2(monster->hit_dice / 10 + 1);

        duration  = std::min(2 + monster->hit_dice / 10, 6);
        for (sumcount = 0; sumcount < sumcount2; ++sumcount)
        {
            create_monster(
                mgen_data(summon_any_demon(DEMON_GREATER),
                          SAME_ATTITUDE(monster),
                          duration, spell_cast, monster->pos(), monster->foe));
        }
        return;

    // Journey -- Added in Summon Lizards and Draconian
    case SPELL_SUMMON_DRAKES:
        if (_mons_abjured(monster, monsterNearby))
            return;

        sumcount2 = 1 + random2(3) + random2(monster->hit_dice / 5 + 1);

        duration  = std::min(2 + monster->hit_dice / 10, 6);

        {
            std::vector<monster_type> monsters;

            for (sumcount = 0; sumcount < sumcount2; sumcount++)
            {
                monster_type mon = summon_any_dragon(DRAGON_LIZARD);

                if (mon == MONS_DRAGON)
                {
                    monsters.clear();
                    monsters.push_back(summon_any_dragon(DRAGON_DRAGON));
                    break;
                }

                monsters.push_back(mon);
            }

            for (int i = 0, size = monsters.size(); i < size; ++i)
            {
                create_monster(
                    mgen_data(monsters[i], SAME_ATTITUDE(monster),
                              duration, spell_cast,
                              monster->pos(), monster->foe));
            }
        }
        return;

    case SPELL_CANTRIP:
    {
        const bool friendly      = mons_friendly(monster);
        const bool buff_only     = !friendly && is_sanctuary(you.pos());
        bool need_friendly_stub  = false;
        const msg_channel_type channel = (friendly) ? MSGCH_FRIEND_ENCHANT
                                                    : MSGCH_MONSTER_ENCHANT;

        // Monster spell of uselessness, just prints a message.
        // This spell exists so that some monsters with really strong
        // spells (ie orc priest) can be toned down a bit. -- bwr
        //
        // XXX: Needs expansion, and perhaps different priest/mage flavours.
        switch (random2((buff_only || crawl_state.arena) ? 4 : 7))
        {
        case 0:
            simple_monster_message(monster, " glows brightly for a moment.",
                                   channel);
            break;
        case 1:
            simple_monster_message(monster, " looks stronger.",
                                   channel);
            break;
        case 2:
            simple_monster_message(monster, " becomes somewhat translucent.",
                                   channel);
            break;
        case 3:
            simple_monster_message(monster, "'s eyes start to glow.",
                                   channel);
            break;
        case 4:
            if (friendly)
                need_friendly_stub = true;
            else
                mpr("You feel troubled.");
            break;
        case 5:
            if (friendly)
                need_friendly_stub = true;
            else
                mpr("You feel a wave of unholy energy pass over you.");
            break;
        case 6:
        default:
            if (friendly)
                need_friendly_stub = true;
            else if (one_chance_in(20))
                mpr("You resist (whatever that was supposed to do).");
            else
                mpr("You resist.");
            break;
        }

        if (need_friendly_stub)
        {
            simple_monster_message(monster, " shimmers for a moment.",
                                   channel);
        }

        return;
    }
    }

    if (spell_is_direct_explosion(spell_cast))
    {
        const actor *foe = monster->get_foe();
        const bool need_more = foe && (foe == &you || see_grid(foe->pos()));
        pbolt.in_explosion_phase = false;
        pbolt.explode(need_more);
    }
    else
        pbolt.fire();
}

void mons_cast_noise(monsters *monster, bolt &pbolt, spell_type spell_cast)
{
    bool force_silent = false;

    spell_type real_spell = spell_cast;

    if (spell_cast == SPELL_DRACONIAN_BREATH)
    {
        int type = monster->type;
        if (mons_genus(type) == MONS_DRACONIAN)
            type = draco_subspecies(monster);

        switch (type)
        {
        case MONS_MOTTLED_DRACONIAN:
            real_spell = SPELL_STICKY_FLAME_SPLASH;
            break;

        case MONS_YELLOW_DRACONIAN:
            real_spell = SPELL_ACID_SPLASH;
            break;

        case MONS_PLAYER_GHOST:
            // Draining breath is silent.
            force_silent = true;
            break;

        default:
            break;
        }
    }
    else if (monster->type == MONS_SHADOW_DRAGON)
        // Draining breath is silent.
        force_silent = true;

    const bool unseen    = !you.can_see(monster);
    const bool silent    = silenced(monster->pos()) || force_silent;
    const bool no_silent = mons_class_flag(monster->type, M_SPELL_NO_SILENT);

    if (unseen && silent)
        return;

    const unsigned int flags = get_spell_flags(real_spell);

    const bool priest = mons_class_flag(monster->type, M_PRIEST);
    const bool wizard = mons_class_flag(monster->type, M_ACTUAL_SPELLS);
    const bool innate = !(priest || wizard || no_silent)
                       || (flags & SPFLAG_INNATE);

    int noise;
    if (silent
       || (innate
           && !mons_class_flag(monster->type, M_NOISY_SPELLS)
           && !(flags & SPFLAG_NOISY)
           && mons_genus(monster->type) != MONS_DRAGON))
    {
        noise = 0;
    }
    else
    {
        if (mons_genus(monster->type) == MONS_DRAGON)
            noise = get_shout_noise_level(S_ROAR);
        else
            noise = spell_noise(real_spell);
    }

    const std::string cast_str = " cast";

    const std::string    spell_name = spell_title(real_spell);
    const mon_body_shape shape      = get_mon_shape(monster);

    std::vector<std::string> key_list;

    // First try the spells name.
    if (shape <= MON_SHAPE_NAGA)
    {
        if (!innate && (priest || wizard))
            key_list.push_back(spell_name + cast_str + " real");
        if (mons_intel(monster) >= I_NORMAL)
            key_list.push_back(spell_name + cast_str + " gestures");
    }
    key_list.push_back(spell_name + cast_str);

    const unsigned int num_spell_keys = key_list.size();

    // Next the monster type name, then species name, then genus name.
    key_list.push_back(mons_type_name(monster->type, DESC_PLAIN) + cast_str);
    key_list.push_back(mons_type_name(mons_species(monster->type), DESC_PLAIN)
                       + cast_str);
    key_list.push_back(mons_type_name(mons_genus(monster->type), DESC_PLAIN)
                       + cast_str);

    // Last, generic wizard, priest or demon.
    if (wizard)
        key_list.push_back("wizard" + cast_str);
    else if (priest)
        key_list.push_back("priest" + cast_str);
    else if (mons_is_demon(monster->type))
        key_list.push_back("demon" + cast_str);

    const bool visible_beam = pbolt.type != 0 && pbolt.type != ' '
                           && pbolt.name[0] != '0'
                           && !pbolt.is_enchantment();

    const bool targeted = (flags & SPFLAG_TARGETING_MASK)
                       && (pbolt.target != monster->pos() || visible_beam);

    if (targeted)
    {
        // For targeted spells, try with the targeted suffix first.
        for (unsigned int i = key_list.size() - 1; i >= num_spell_keys; i--)
        {
            std::string str = key_list[i] + " targeted";
            key_list.insert(key_list.begin() + i, str);
        }

        // Generic beam messages.
        if (visible_beam)
        {
            key_list.push_back(pbolt.get_short_name() + " beam " + cast_str);
            key_list.push_back("beam catchall cast");
        }
    }

    std::string prefix;
    if (silent)
        prefix = "silent ";
    else if (unseen)
        prefix = "unseen ";

    std::string msg;
    for (unsigned int i = 0; i < key_list.size(); i++)
    {
        const std::string key = key_list[i];

        msg = getSpeakString(prefix + key);
        if (msg == "__NONE")
        {
            msg = "";
            break;
        }
        else if (msg == "__NEXT")
        {
            msg = "";
            if (i < num_spell_keys)
                i = num_spell_keys - 1;
            else if (ends_with(key, " targeted"))
                i++;
            continue;
        }
        else if (!msg.empty())
            break;

        // If we got no message and we're using the silent prefix, then
        // try again without the prefix.
        if (prefix != "silent")
            continue;

        msg = getSpeakString(key);
        if (msg == "__NONE")
        {
            msg = "";
            break;
        }
        else if (msg == "__NEXT")
        {
            msg = "";
            if (i < num_spell_keys)
                i = num_spell_keys - 1;
            else if (ends_with(key, " targeted"))
                i++;
            continue;
        }
        else if (!msg.empty())
            break;
    }

    if (msg.empty())
    {
        if (silent)
            return;

        noisy(noise, monster->pos());
        return;
    }

    /////////////////////
    // We have a message.
    /////////////////////

    const bool gestured = msg.find("Gesture") != std::string::npos
                       || msg.find(" gesture") != std::string::npos
                       || msg.find("Point") != std::string::npos
                       || msg.find(" point") != std::string::npos;

    bolt tracer = pbolt;
    if (targeted)
    {
        // For a targeted but rangeless spell make the range positive so that
        // fire_tracer() will fill out path_taken.
        if (pbolt.range == 0 && pbolt.target != monster->pos())
            tracer.range = ENV_SHOW_DIAMETER;

        fire_tracer(monster, tracer);
    }

    std::string targ_prep = "at";
    std::string target    = "nothing";

    if (!targeted)
        target = "NO TARGET";
    else if (pbolt.target == you.pos())
        target = "you";
    else if (pbolt.target == monster->pos())
        target = monster->pronoun(PRONOUN_REFLEXIVE);
    // Monsters should only use targeted spells while foe == MHITNOT
    // if they're targeting themselves.
    else if (monster->foe == MHITNOT && !monster->confused())
        target = "NONEXISTENT FOE";
    else if (!invalid_monster_index(monster->foe)
             && menv[monster->foe].type == -1)
    {
        target = "DEAD FOE";
    }
    else if (in_bounds(pbolt.target) && see_grid(pbolt.target))
    {
        int midx = mgrd(pbolt.target);
        if (midx != NON_MONSTER)
        {
            monsters* mtarg = &menv[midx];

            if (you.can_see(mtarg))
                target = mtarg->name(DESC_NOCAP_THE);
        }
    }

    // Monster might be aiming past the real target, or maybe some fuzz has
    // been applied because the target is invisible.
    if (target == "nothing" && targeted)
    {
        if (pbolt.aimed_at_spot)
        {
            int count = 0;
            for (adjacent_iterator ai(pbolt.target); ai; ++ai)
            {
                if (*ai == monster->pos())
                    continue;

                if (*ai == you.pos())
                {
                    targ_prep = "next to";
                    target    = "you";
                    break;
                }

                const int midx = mgrd(*ai);

                if (midx != NON_MONSTER && you.can_see(&menv[midx]))
                {
                    targ_prep = "next to";
                    if (one_chance_in(count++))
                        target = menv[midx].name(DESC_NOCAP_THE);
                }
            }
        }

        const bool visible_path      = visible_beam || gestured;
              bool mons_targ_aligned = false;

        const std::vector<coord_def> &path = tracer.path_taken;
        for (unsigned int i = 0; i < path.size(); i++)
        {
            const coord_def pos = path[i];

            if (pos == monster->pos())
                continue;

            const int midx = mgrd(pos);
            if (pos == you.pos())
            {
                // Be egotistical and assume that the monster is aiming at
                // the player, rather than the player being in the path of
                // a beam aimed at an ally.
                if (!mons_wont_attack(monster))
                {
                    targ_prep = "at";
                    target    = "you";
                    break;
                }
                // If the ally is confused or aiming at an invisible enemy,
                // with the player in the path, act like it's targeted at
                // the player if there isn't any visible target earlier
                // in the path.
                else if (target == "nothing")
                {
                    targ_prep         = "at";
                    target            = "you";
                    mons_targ_aligned = true;
                }
            }
            else if (visible_path && midx != NON_MONSTER
                     && you.can_see(&menv[midx]))
            {
                bool        is_aligned = mons_aligned(midx, monster->mindex());
                std::string name       = menv[midx].name(DESC_NOCAP_THE);

                if (target == "nothing")
                {
                    mons_targ_aligned = is_aligned;
                    target            = name;
                }
                // If the first target was aligned with the beam source then
                // the first subsequent non-aligned monster in the path will
                // take it's place.
                else if (mons_targ_aligned && !is_aligned)
                {
                    mons_targ_aligned = false;
                    target            = name;
                }
                targ_prep = "at";
            }
            else if (visible_path && target == "nothing")
            {
                int count = 0;
                for (adjacent_iterator ai(pbolt.target); ai; ++ai)
                {
                    if (*ai == monster->pos())
                        continue;

                    if (*ai == you.pos())
                    {
                        targ_prep = "past";
                        target    = "you";
                        break;
                    }

                    const int midx2 = mgrd(*ai);
                    if (midx2 != NON_MONSTER && you.can_see(&menv[midx2]))
                    {
                        targ_prep = "past";
                        if (one_chance_in(count++))
                            target = menv[midx2].name(DESC_NOCAP_THE);
                    }
                }
            }
        } // for (unsigned int i = 0; i < path.size(); i++)
    } // if (target == "nothing" && targeted)

    const actor*    foe   = monster->get_foe();
    const monsters* m_foe = (foe && foe->atype() == ACT_MONSTER) ?
                            dynamic_cast<const monsters*>(foe) : NULL;

    // If we still can't find what appears to be the target, and the
    // monster isn't just throwing the spell in a random direction,
    // we should be able to tell what the monster was aiming for if
    // we can see the monster's foe and the beam (or the beam path
    // implied by gesturing).  But only if the beam didn't actually hit
    // anything (but if it did hit something, why didn't that monster
    // show up in the beam's path?)
    if (targeted && target == "nothing" && foe != NULL
        && (tracer.foe_info.count + tracer.friend_info.count) == 0
        && (you.can_see(foe) || foe == &you) && !monster->confused()
        && (visible_beam || gestured))
    {
        if (foe == &you)
            target = "you";
        else
            target = m_foe->name(DESC_NOCAP_THE);

        if (pbolt.aimed_at_spot)
            targ_prep = "next to";
        else
            targ_prep = "past";
    }

    // If the monster gestures to create an invisible beam then
    // assume that anything close to the beam is the intended target.
    // Also, if the monster gestures to create a visible beam but it
    // misses still say that the monster gestured "at" the target,
    // rather than "past".
    if (gestured || target == "nothing")
        targ_prep = "at";

    msg = replace_all(msg, "@at@",     targ_prep);
    msg = replace_all(msg, "@target@", target);

    std::string beam_name;
    if (!targeted)
        beam_name = "NON TARGETED BEAM";
    else if (pbolt.name.empty())
        beam_name = "INVALID BEAM";
    else if (!tracer.seen)
        beam_name = "UNSEEN BEAM";
    else
        beam_name = pbolt.get_short_name();

    msg = replace_all(msg, "@beam@", beam_name);

    const msg_channel_type chan =
        (unseen                      ? MSGCH_SOUND :
         mons_friendly_real(monster) ? MSGCH_FRIEND_SPELL :
                                       MSGCH_MONSTER_SPELL);

    if (silent)
    {
        mons_speaks_msg(monster, msg, chan, true);
        return;
    }

    // noisy() returns true if the player heard the noise.
    if (noisy(noise, monster->pos()) || !unseen)
        mons_speaks_msg(monster, msg, chan);
}

// Set up bolt structure for monster spell casting.
void setup_mons_cast(monsters *monster, bolt &pbolt,
                     spell_type spell_cast)
{
    // always set these -- used by things other than fire_beam()

    // [ds] Used to be 12 * MHD and later buggily forced to -1 downstairs.
    // Setting this to a more realistic number now that that bug is
    // squashed.
    pbolt.ench_power = 4 * monster->hit_dice;

    if (spell_cast == SPELL_TELEPORT_SELF)
        pbolt.ench_power = 2000;

    pbolt.beam_source = monster_index(monster);

    // Convenience for the hapless innocent who assumes that this
    // damn function does all possible setup. [ds]
    if (pbolt.target.origin())
        pbolt.target = monster->target;

    // set bolt type and range
    if (_los_free_spell(spell_cast))
    {
        pbolt.range = 0;
        switch (spell_cast)
        {
        case SPELL_BRAIN_FEED:
            pbolt.type = DMNBM_BRAIN_FEED;
            return;
        case SPELL_SMITING:
            pbolt.type = DMNBM_SMITING;
            return;
        default:
            // Other spells get normal setup:
            break;
        }
    }

    // The below are no-ops since they don't involve direct_effect,
    // fire_tracer, or beam.
    switch (spell_cast)
    {
    case SPELL_SUMMON_SMALL_MAMMALS:
    case SPELL_GREATER_HEALING:
    case SPELL_VAMPIRE_SUMMON:
    case SPELL_SHADOW_CREATURES:       // summon anything appropriate for level
    case SPELL_FAKE_RAKSHASA_SUMMON:
    case SPELL_SUMMON_DEMON:
    case SPELL_SUMMON_UGLY_THING:
    case SPELL_ANIMATE_DEAD:
    case SPELL_CALL_IMP:
    case SPELL_SUMMON_SCORPIONS:
    case SPELL_SUMMON_UFETUBUS:
    case SPELL_SUMMON_BEAST:       // Geryon
    case SPELL_SUMMON_UNDEAD:      // summon undead around player
    case SPELL_SUMMON_ICE_BEAST:
    case SPELL_SUMMON_MUSHROOMS:
    case SPELL_CONJURE_BALL_LIGHTNING:
    case SPELL_SUMMON_DRAKES:
    case SPELL_SUMMON_HORRIBLE_THINGS:
    case SPELL_SUMMON_WRAITHS:
    case SPELL_SYMBOL_OF_TORMENT:
    case SPELL_SUMMON_GREATER_DEMON:
    case SPELL_CANTRIP:
    case SPELL_BERSERKER_RAGE:
    case SPELL_WATER_ELEMENTALS:
    case SPELL_BLINK:
    case SPELL_CONTROLLED_BLINK:
        return;
    default:
        break;
    }

    // Need to correct this for power of spellcaster
    int power = 12 * monster->hit_dice;

    bolt theBeam         = mons_spells(monster, spell_cast, power);

    pbolt.colour         = theBeam.colour;
    pbolt.range          = theBeam.range;
    pbolt.hit            = theBeam.hit;
    pbolt.damage         = theBeam.damage;

    if (theBeam.ench_power != -1)
        pbolt.ench_power = theBeam.ench_power;

    pbolt.type           = theBeam.type;
    pbolt.flavour        = theBeam.flavour;
    pbolt.thrower        = theBeam.thrower;
    pbolt.name           = theBeam.name;
    pbolt.short_name     = theBeam.short_name;
    pbolt.is_beam        = theBeam.is_beam;
    pbolt.source         = monster->pos();
    pbolt.is_tracer      = false;
    pbolt.is_explosion   = theBeam.is_explosion;
    pbolt.ex_size        = theBeam.ex_size;

    pbolt.foe_ratio      = theBeam.foe_ratio;

    if (!pbolt.is_enchantment())
        pbolt.aux_source = pbolt.name;
    else
        pbolt.aux_source.clear();

    if (spell_cast == SPELL_HASTE
        || spell_cast == SPELL_INVISIBILITY
        || spell_cast == SPELL_LESSER_HEALING
        || spell_cast == SPELL_TELEPORT_SELF)
    {
        pbolt.target = monster->pos();
    }
}

bool monster_random_space(const monsters *monster, coord_def& target,
                          bool forbid_sanctuary)
{
    int tries = 0;
    while (tries++ < 1000)
    {
        target = random_in_bounds();

        // Don't land on top of another monster.
        if (mgrd(target) != NON_MONSTER || target == you.pos())
            continue;

        if (is_sanctuary(target) && forbid_sanctuary)
            continue;

        if (monster_habitable_grid(monster, grd(target)))
            return (true);
    }

    return (false);
}

bool monster_random_space(monster_type mon, coord_def& target,
                          bool forbid_sanctuary)
{
    monsters dummy;
    dummy.type = mon;

    return monster_random_space(&dummy, target, forbid_sanctuary);
}

void monster_teleport(monsters *monster, bool instan, bool silent)
{
    if (!instan)
    {
        if (monster->del_ench(ENCH_TP))
        {
            if (!silent)
                simple_monster_message(monster, " seems more stable.");
        }
        else
        {
            if (!silent)
                simple_monster_message(monster, " looks slightly unstable.");

            monster->add_ench( mon_enchant(ENCH_TP, 0, KC_OTHER,
                                           random_range(20, 30)) );
        }

        return;
    }

    bool was_seen = player_monster_visible(monster) && mons_near(monster)
                    && !mons_is_lurking(monster);

    if (!silent)
        simple_monster_message(monster, " disappears!");

    const coord_def oldplace = monster->pos();

    // Pick the monster up.
    mgrd(oldplace) = NON_MONSTER;

    mons_clear_trapping_net(monster);

    coord_def newpos;
    if (monster_random_space(monster, newpos, !mons_wont_attack(monster)))
        monster->moveto(newpos);

    mgrd(monster->pos()) = monster_index(monster);

    // Mimics change form/colour when teleported.
    if (mons_is_mimic( monster->type ))
    {
        int old_type    = monster->type;
        monster->type   = MONS_GOLD_MIMIC + random2(5);
        monster->colour = get_mimic_colour( monster );

        // If it's changed form, you won't recognize it.
        // This assumes that a non-gold mimic turning into another item of
        // the same description is really, really unlikely.
        if (old_type != MONS_GOLD_MIMIC || monster->type != MONS_GOLD_MIMIC)
            was_seen = false;
    }

    const bool now_visible = mons_near(monster);
    if (!silent && now_visible)
    {
        if (was_seen)
            simple_monster_message(monster, " reappears nearby!");
        else
        {
            // Even if it doesn't interrupt an activity (the player isn't
            // delayed, the monster isn't hostile) we still want to give
            // a message.
            activity_interrupt_data ai(monster, "thin air");
            if (!interrupt_activity(AI_SEE_MONSTER, ai))
                simple_monster_message(monster, " appears out of thin air!");
        }
    }

    if (player_monster_visible(monster) && now_visible)
        seen_monster(monster);

    monster->check_redraw(oldplace);
    monster->apply_location_effects(oldplace);

    // Teleporting mimics change form - if they reappear out of LOS, they are
    // no longer known.
    if (mons_is_mimic(monster->type))
    {
        if (now_visible)
            monster->flags |= MF_KNOWN_MIMIC;
        else
            monster->flags &= ~MF_KNOWN_MIMIC;
    }
}

void setup_generic_throw(struct monsters *monster, struct bolt &pbolt)
{
    // FIXME we should use a sensible range here
    pbolt.range = LOS_RADIUS;
    pbolt.beam_source = monster_index(monster);

    pbolt.type    = dchar_glyph(DCHAR_FIRED_MISSILE);
    pbolt.flavour = BEAM_MISSILE;
    pbolt.thrower = KILL_MON_MISSILE;
    pbolt.aux_source.clear();
    pbolt.is_beam = false;
}

bool mons_throw(struct monsters *monster, struct bolt &pbolt, int hand_used)
{
    std::string ammo_name;

    bool returning = false;

    int baseHit = 0, baseDam = 0;       // from thrown or ammo
    int ammoHitBonus = 0, ammoDamBonus = 0;     // from thrown or ammo
    int lnchHitBonus = 0, lnchDamBonus = 0;     // special add from launcher
    int exHitBonus   = 0, exDamBonus = 0; // 'extra' bonus from skill/dex/str
    int lnchBaseDam  = 0;

    int hitMult  = 0;
    int damMult  = 0;
    int diceMult = 100;

    // Some initial convenience & initializations.
    int wepClass  = mitm[hand_used].base_type;
    int wepType   = mitm[hand_used].sub_type;

    int weapon    = monster->inv[MSLOT_WEAPON];
    int lnchType  = (weapon != NON_ITEM) ? mitm[weapon].sub_type : 0;

    mon_inv_type slot = get_mon_equip_slot(monster, mitm[hand_used]);
    ASSERT(slot != NUM_MONSTER_SLOTS);

    const bool skilled = mons_class_flag(monster->type, M_FIGHTER);

    monster->lose_energy(EUT_MISSILE);
    const int throw_energy = monster->action_energy(EUT_MISSILE);

    // Dropping item copy, since the launched item might be different.
    item_def item = mitm[hand_used];
    item.quantity = 1;
    if (mons_friendly(monster))
        item.flags |= ISFLAG_DROPPED_BY_ALLY;

    setup_missile_beam(monster, pbolt, item, ammo_name, returning);

    // FIXME we should actually determine a sensible range here
    pbolt.range         = LOS_RADIUS;
    pbolt.aimed_at_spot = returning;

    const launch_retval projected =
        is_launched(monster, monster->mslot_item(MSLOT_WEAPON),
                    mitm[hand_used]);

    // extract launcher bonuses due to magic
    if (projected == LRET_LAUNCHED)
    {
        lnchHitBonus = mitm[weapon].plus;
        lnchDamBonus = mitm[weapon].plus2;
        lnchBaseDam  = property(mitm[weapon], PWPN_DAMAGE);
    }

    // extract weapon/ammo bonuses due to magic
    ammoHitBonus = item.plus;
    ammoDamBonus = item.plus2;

    // Archers get a boost from their melee attack.
    if (mons_class_flag(monster->type, M_ARCHER))
    {
        const mon_attack_def attk = mons_attack_spec(monster, 0);
        if (attk.type == AT_SHOOT)
            ammoDamBonus += random2avg(attk.damage, 2);
    }

    if (projected == LRET_THROWN)
    {
        // Darts are easy.
        if (wepClass == OBJ_MISSILES && wepType == MI_DART)
        {
            baseHit = 11;
            hitMult = 40;
            damMult = 25;
        }
        else
        {
            baseHit = 6;
            hitMult = 30;
            damMult = 25;
        }

        baseDam = property(item, PWPN_DAMAGE);

        if (wepClass == OBJ_MISSILES)   // throw missile
        {
            // ammo damage needs adjusting here - OBJ_MISSILES
            // don't get separate tohit/damage bonuses!
            ammoDamBonus = ammoHitBonus;

            // [dshaligram] Thrown stones/darts do only half the damage of
            // launched stones/darts. This matches 4.0 behaviour.
            if (wepType == MI_DART || wepType == MI_STONE
                || wepType == MI_SLING_BULLET)
            {
                baseDam = div_rand_round(baseDam, 2);
            }
        }

        // give monster "skill" bonuses based on HD
        exHitBonus = (hitMult * monster->hit_dice) / 10 + 1;
        exDamBonus = (damMult * monster->hit_dice) / 10 + 1;
    }

    // Monsters no longer gain unfair advantages with weapons of
    // fire/ice and incorrect ammo.  They now have the same restrictions
    // as players.

          int  bow_brand  = SPWPN_NORMAL;
    const int  ammo_brand = get_ammo_brand(item);

    if (projected == LRET_LAUNCHED)
    {
        bow_brand = get_weapon_brand(mitm[monster->inv[MSLOT_WEAPON]]);

        switch (lnchType)
        {
        case WPN_BLOWGUN:
            baseHit = 12;
            hitMult = 60;
            damMult = 0;
            lnchDamBonus = 0;
            break;
        case WPN_BOW:
        case WPN_LONGBOW:
            baseHit = 0;
            hitMult = 60;
            damMult = 35;
            // monsters get half the launcher damage bonus,
            // which is about as fair as I can figure it.
            lnchDamBonus = (lnchDamBonus + 1) / 2;
            break;
        case WPN_CROSSBOW:
            baseHit = 4;
            hitMult = 70;
            damMult = 30;
            break;
        case WPN_HAND_CROSSBOW:
            baseHit = 2;
            hitMult = 50;
            damMult = 20;
            break;
        case WPN_SLING:
            baseHit = 10;
            hitMult = 40;
            damMult = 20;
            // monsters get half the launcher damage bonus,
            // which is about as fair as I can figure it.
            lnchDamBonus /= 2;
            break;
        }

        // Launcher is now more important than ammo for base damage.
        baseDam = property(item, PWPN_DAMAGE);
        if (lnchBaseDam)
            baseDam = lnchBaseDam + random2(1 + baseDam);

        // missiles don't have pluses2;  use hit bonus
        ammoDamBonus = ammoHitBonus;

        exHitBonus = (hitMult * monster->hit_dice) / 10 + 1;
        exDamBonus = (damMult * monster->hit_dice) / 10 + 1;

        if (!baseDam && elemental_missile_beam(bow_brand, ammo_brand))
            baseDam = 4;

        // [dshaligram] This is a horrible hack - we force beam.cc to
        // consider this beam "needle-like".
        if (wepClass == OBJ_MISSILES && wepType == MI_NEEDLE)
            pbolt.ench_power = AUTOMATIC_HIT;

        // elven bow w/ elven arrow, also orcish
        if (get_equip_race(mitm[monster->inv[MSLOT_WEAPON]])
                == get_equip_race(mitm[monster->inv[MSLOT_MISSILE]]))
        {
            baseHit++;
            baseDam++;

            if (get_equip_race(mitm[monster->inv[MSLOT_WEAPON]]) == ISFLAG_ELVEN)
                pbolt.hit++;
        }

        // POISON brand launchers poison ammo
        if (bow_brand == SPWPN_VENOM && ammo_brand == SPMSL_NORMAL)
            set_item_ego_type(item, OBJ_MISSILES, SPMSL_POISONED);

        // Vorpal brand increases damage dice size.
        if (bow_brand == SPWPN_VORPAL)
            diceMult = diceMult * 130 / 100;

        // As do steel ammo.
        if (ammo_brand == SPMSL_STEEL)
            diceMult = diceMult * 150 / 100;

        // Note: we already have throw_energy taken off.  -- bwr
        int speed_delta = 0;
        if (lnchType == WPN_CROSSBOW)
        {
            if (bow_brand == SPWPN_SPEED)
            {
                // Speed crossbows take 50% less time to use than
                // ordinary crossbows.
                speed_delta = div_rand_round(throw_energy * 2, 5);
            }
            else
            {
                // Ordinary crossbows take 20% more time to use
                // than ordinary bows.
                speed_delta = -div_rand_round(throw_energy, 5);
            }
        }
        else if (bow_brand == SPWPN_SPEED)
        {
            // Speed bows take 50% less time to use than
            // ordinary bows.
            speed_delta = div_rand_round(throw_energy, 2);
        }

        monster->speed_increment += speed_delta;
    }

    // Chaos overides flame and frost
    if (pbolt.flavour != BEAM_MISSILE)
    {
        baseHit    += 2;
        exDamBonus += 6;
    }

    // monster intelligence bonus
    if (mons_intel(monster) == I_HIGH)
        exHitBonus += 10;

    // Now, if a monster is, for some reason, throwing something really
    // stupid, it will have baseHit of 0 and damage of 0.  Ah well.
    std::string msg = monster->name(DESC_CAP_THE);
    msg += ((projected == LRET_LAUNCHED) ? " shoots " : " throws ");

    if (!pbolt.name.empty() && projected == LRET_LAUNCHED)
        msg += article_a(pbolt.name);
    else
    {
        // build shoot message
        msg += item.name(DESC_NOCAP_A);

        // build beam name
        pbolt.name = item.name(DESC_PLAIN, false, false, false);
    }
    msg += ".";

    if (monster->visible())
    {
        mpr(msg.c_str());

        if (projected == LRET_LAUNCHED
               && item_type_known(mitm[monster->inv[MSLOT_WEAPON]])
            || projected == LRET_THROWN
               && mitm[hand_used].base_type == OBJ_MISSILES)
        {
            set_ident_flags(mitm[hand_used], ISFLAG_KNOW_TYPE);
        }
    }

    // [dshaligram] When changing bolt names here, you must edit
    // hiscores.cc (scorefile_entry::terse_missile_cause()) to match.
    char throw_buff[ITEMNAME_SIZE];
    if (projected == LRET_LAUNCHED)
    {
        snprintf(throw_buff, sizeof(throw_buff), "Shot with a%s %s by %s",
                 (is_vowel(pbolt.name[0]) ? "n" : ""), pbolt.name.c_str(),
                 monster->name(DESC_NOCAP_A).c_str());
    }
    else
    {
        snprintf(throw_buff, sizeof(throw_buff), "Hit by a%s %s thrown by %s",
                 (is_vowel(pbolt.name[0]) ? "n" : ""), pbolt.name.c_str(),
                 monster->name(DESC_NOCAP_A).c_str());
    }

    pbolt.aux_source = throw_buff;

    // Add everything up.
    pbolt.hit = baseHit + random2avg(exHitBonus, 2) + ammoHitBonus;
    pbolt.damage =
        dice_def(1, baseDam + random2avg(exDamBonus, 2) + ammoDamBonus);

    if (projected == LRET_LAUNCHED)
    {
        pbolt.damage.size += lnchDamBonus;
        pbolt.hit += lnchHitBonus;
    }
    pbolt.damage.size = diceMult * pbolt.damage.size / 100;

    if (monster->has_ench(ENCH_BATTLE_FRENZY))
    {
        const mon_enchant ench = monster->get_ench(ENCH_BATTLE_FRENZY);

#ifdef DEBUG_DIAGNOSTICS
        const dice_def orig_damage = pbolt.damage;
#endif

        pbolt.damage.size = pbolt.damage.size * (115 + ench.degree * 15) / 100;

#ifdef DEBUG_DIAGNOSTICS
        mprf(MSGCH_DIAGNOSTICS, "%s frenzy damage: %dd%d -> %dd%d",
             monster->name(DESC_PLAIN).c_str(),
             orig_damage.num, orig_damage.size,
             pbolt.damage.num, pbolt.damage.size);
#endif
    }

    // Skilled archers get better to-hit and damage.
    if (skilled)
    {
        pbolt.hit         = pbolt.hit * 120 / 100;
        pbolt.damage.size = pbolt.damage.size * 120 / 100;
    }

    scale_dice(pbolt.damage);

    // decrease inventory
    bool really_returns;
    if (returning && !one_chance_in(mons_power(monster->type) + 3))
        really_returns = true;
    else
        really_returns = false;

    pbolt.drop_item = !really_returns;
    pbolt.fire();

    // The item can be destroyed before returning.
    if (really_returns && thrown_object_destroyed(&item, pbolt.target, true))
    {
        really_returns = false;
    }

    if (really_returns)
    {
        // Fire beam in reverse.
        pbolt.setup_retrace();
        viewwindow(true, false);
        pbolt.fire();
        msg::stream << "The weapon returns "
                    << (player_monster_visible(monster)?
                          ("to " + monster->name(DESC_NOCAP_THE))
                        : "whence it came from")
                    << "!" << std::endl;

        // Player saw the item return.
        if (!is_artefact(item))
        {
            // Since this only happens for non-artefacts, also mark properties
            // as known.
            set_ident_flags(mitm[hand_used],
                            ISFLAG_KNOW_TYPE | ISFLAG_KNOW_PROPERTIES);
        }
    }
    else if (dec_mitm_item_quantity(hand_used, 1))
        monster->inv[returning ? slot : MSLOT_MISSILE] = NON_ITEM;

    if (pbolt.special_explosion != NULL)
        delete pbolt.special_explosion;

    return (true);
}

static void _scale_draconian_breath(bolt& beam, int drac_type)
{
    int scaling = 100;
    switch(drac_type)
    {
    case MONS_RED_DRACONIAN:
        beam.name       = "searing blast";
        beam.aux_source = "blast of searing breath";
        scaling         = 65;
        break;

    case MONS_WHITE_DRACONIAN:
        beam.name       = "chilling blast";
        beam.aux_source = "blast of chilling breath";
        beam.short_name = "frost";
        scaling         = 65;
        break;

    case MONS_PLAYER_GHOST: // draconians only
        beam.name       = "blast of negative energy";
        beam.aux_source = "blast of draining breath";
        beam.flavour    = BEAM_NEG;
        beam.colour     = DARKGREY;
        scaling         = 65;
        break;
    }
    beam.damage.size = scaling * beam.damage.size / 100;
}

static spell_type _draco_type_to_breath(int drac_type)
{
    switch (drac_type)
    {
    case MONS_BLACK_DRACONIAN:   return SPELL_LIGHTNING_BOLT;
    case MONS_MOTTLED_DRACONIAN: return SPELL_STICKY_FLAME_SPLASH;
    case MONS_YELLOW_DRACONIAN:  return SPELL_ACID_SPLASH;
    case MONS_GREEN_DRACONIAN:   return SPELL_POISONOUS_CLOUD;
    case MONS_PURPLE_DRACONIAN:  return SPELL_ISKENDERUNS_MYSTIC_BLAST;
    case MONS_RED_DRACONIAN:     return SPELL_FIRE_BREATH;
    case MONS_WHITE_DRACONIAN:   return SPELL_COLD_BREATH;
    case MONS_PALE_DRACONIAN:    return SPELL_STEAM_BALL;

    // Handled later.
    case MONS_PLAYER_GHOST:      return SPELL_DRACONIAN_BREATH;

    default:
        DEBUGSTR("Invalid monster using draconian breath spell");
        break;
    }

    return (SPELL_DRACONIAN_BREATH);
}


bolt mons_spells( monsters *mons, spell_type spell_cast, int power )
{
    ASSERT(power > 0);

    bolt beam;

    // Initialize to some bogus values so we can catch problems.
    beam.name         = "****";
    beam.colour       = 1000;
    beam.hit          = -1;
    beam.damage       = dice_def( 1, 0 );
    beam.ench_power   = -1;
    beam.type         = 0;
    beam.flavour      = BEAM_NONE;
    beam.thrower      = KILL_MISC;
    beam.is_beam      = false;
    beam.is_explosion = false;

    // Sandblast is different, and gets range updated later
    if (spell_cast != SPELL_SANDBLAST)
        beam.range = spell_range(spell_cast, power, true);

    const int drac_type = (mons_genus(mons->type) == MONS_DRACONIAN)
                            ? draco_subspecies(mons) : mons->type;

    spell_type real_spell = spell_cast;

    if (spell_cast == SPELL_DRACONIAN_BREATH)
        real_spell = _draco_type_to_breath(drac_type);

    beam.type = dchar_glyph(DCHAR_FIRED_ZAP); // default
    beam.thrower = KILL_MON_MISSILE;

    // FIXME: this should use the zap_data[] struct from beam.cc!
    switch (real_spell)
    {
    case SPELL_MAGIC_DART:
        beam.colour   = LIGHTMAGENTA;
        beam.name     = "magic dart";
        beam.damage   = dice_def( 3, 4 + (power / 100) );
        beam.hit      = AUTOMATIC_HIT;
        beam.flavour  = BEAM_MMISSILE;
        break;

    case SPELL_THROW_FLAME:
        beam.colour   = RED;
        beam.name     = "puff of flame";
        beam.damage   = dice_def( 3, 5 + (power / 40) );
        beam.hit      = 25 + power / 40;
        beam.flavour  = BEAM_FIRE;
        break;

    case SPELL_THROW_FROST:
        beam.colour   = WHITE;
        beam.name     = "puff of frost";
        beam.damage   = dice_def( 3, 5 + (power / 40) );
        beam.hit      = 25 + power / 40;
        beam.flavour  = BEAM_COLD;
        break;

    case SPELL_SANDBLAST:
        beam.colour   = BROWN;
        beam.name     = "rocky blast";
        beam.damage   = dice_def( 3, 5 + (power / 40) );
        beam.hit      = 20 + power / 40;
        beam.flavour  = BEAM_FRAG;
        beam.range    = 2;      // spell_range() is wrong here
        break;

    case SPELL_DISPEL_UNDEAD:
        beam.flavour  = BEAM_DISPEL_UNDEAD;
        beam.damage   = dice_def( 3, std::min(6 + power / 10, 40) );
        beam.is_beam  = true;
        break;

    case SPELL_PARALYSE:
        beam.flavour  = BEAM_PARALYSIS;
        beam.is_beam  = true;
        break;

    case SPELL_SLOW:
        beam.flavour  = BEAM_SLOW;
        beam.is_beam  = true;
        break;

    case SPELL_HASTE:              // (self)
        beam.flavour  = BEAM_HASTE;
        break;

    case SPELL_BACKLIGHT:
        beam.flavour  = BEAM_BACKLIGHT;
        beam.is_beam  = true;
        break;

    case SPELL_CONFUSE:
        beam.flavour  = BEAM_CONFUSION;
        beam.is_beam  = true;
        break;

    case SPELL_SLEEP:
        beam.flavour  = BEAM_SLEEP;
        beam.is_beam  = true;
        break;

    case SPELL_POLYMORPH_OTHER:
        beam.flavour  = BEAM_POLYMORPH;
        beam.is_beam  = true;
        // Be careful with this one.
        // Having allies mutate you is infuriating.
        beam.foe_ratio = 1000;
        break;

    case SPELL_VENOM_BOLT:
        beam.name     = "bolt of poison";
        beam.damage   = dice_def( 3, 6 + power / 13 );
        beam.colour   = LIGHTGREEN;
        beam.flavour  = BEAM_POISON;
        beam.hit      = 19 + power / 20;
        beam.is_beam  = true;
        break;

    case SPELL_POISON_ARROW:
        beam.name     = "poison arrow";
        beam.damage   = dice_def( 3, 7 + power / 12 );
        beam.colour   = LIGHTGREEN;
        beam.type     = dchar_glyph(DCHAR_FIRED_MISSILE);
        beam.flavour  = BEAM_POISON_ARROW;
        beam.hit      = 20 + power / 25;
        break;

    case SPELL_BOLT_OF_MAGMA:
        beam.name     = "bolt of magma";
        beam.damage   = dice_def( 3, 8 + power / 11 );
        beam.colour   = RED;
        beam.flavour  = BEAM_LAVA;
        beam.hit      = 17 + power / 25;
        beam.is_beam  = true;
        break;

    case SPELL_BOLT_OF_FIRE:
        beam.name     = "bolt of fire";
        beam.damage   = dice_def( 3, 8 + power / 11 );
        beam.colour   = RED;
        beam.flavour  = BEAM_FIRE;
        beam.hit      = 17 + power / 25;
        beam.is_beam  = true;
        break;

    case SPELL_ICE_BOLT:
        beam.name     = "bolt of ice";
        beam.damage   = dice_def( 3, 8 + power / 11 );
        beam.colour   = WHITE;
        beam.flavour  = BEAM_ICE;
        beam.hit      = 17 + power / 25;
        beam.is_beam  = true;
        break;

    case SPELL_BOLT_OF_COLD:
        beam.name     = "bolt of cold";
        beam.damage   = dice_def( 3, 8 + power / 11 );
        beam.colour   = WHITE;
        beam.flavour  = BEAM_COLD;
        beam.hit      = 17 + power / 25;
        beam.is_beam  = true;
        break;

    case SPELL_FREEZING_CLOUD:
        beam.name     = "freezing blast";
        beam.damage   = dice_def( 2, 9 + power / 11 );
        beam.colour   = WHITE;
        beam.flavour  = BEAM_COLD;
        beam.hit      = 17 + power / 25;
        beam.is_beam  = true;
        beam.is_big_cloud = true;
        break;

    case SPELL_SHOCK:
        beam.name     = "zap";
        beam.damage   = dice_def( 1, 8 + (power / 20) );
        beam.colour   = LIGHTCYAN;
        beam.flavour  = BEAM_ELECTRICITY;
        beam.hit      = 17 + power / 20;
        beam.is_beam  = true;
        break;

    case SPELL_LIGHTNING_BOLT:
        beam.name     = "bolt of lightning";
        beam.damage   = dice_def( 3, 10 + power / 17 );
        beam.colour   = LIGHTCYAN;
        beam.flavour  = BEAM_ELECTRICITY;
        beam.hit      = 16 + power / 40;
        beam.is_beam  = true;
        break;

    case SPELL_INVISIBILITY:
        beam.flavour  = BEAM_INVISIBILITY;
        break;

    case SPELL_FIREBALL:
        beam.colour   = RED;
        beam.name     = "fireball";
        beam.damage   = dice_def( 3, 7 + power / 10 );
        beam.hit      = 40;
        beam.flavour  = BEAM_FIRE;
        beam.foe_ratio = 60;
        beam.is_explosion = true;
        break;

    case SPELL_FIRE_STORM:
        setup_fire_storm(mons, power / 2, beam);
        beam.foe_ratio = random_range(40, 55);
        break;

    case SPELL_ICE_STORM:
        beam.name           = "great blast of cold";
        beam.colour         = BLUE;
        beam.damage         = calc_dice( 10, 18 + power / 2 );
        beam.hit            = 20 + power / 10;    // 50: 25   100: 30
        beam.ench_power     = power;              // used for radius
        beam.flavour        = BEAM_ICE;           // half resisted
        beam.is_explosion   = true;
        beam.foe_ratio      = random_range(40, 55);
        break;

    case SPELL_HELLFIRE_BURST:
        beam.aux_source   = "burst of hellfire";
        beam.name         = "hellfire";
        beam.ex_size      = 1;
        beam.flavour      = BEAM_HELLFIRE;
        beam.is_explosion = true;
        beam.colour       = RED;
        beam.aux_source.clear();
        beam.is_tracer    = false;
        beam.hit          = 20;
        beam.damage       = mons_foe_is_mons(mons) ? dice_def(5, 7)
                                                   : dice_def(3, 20);
        break;

    case SPELL_LESSER_HEALING:
        beam.flavour  = BEAM_HEALING;
        beam.hit      = 25 + (power / 5);
        break;

    case SPELL_TELEPORT_SELF:
        beam.flavour  = BEAM_TELEPORT;
        break;

    case SPELL_TELEPORT_OTHER:
        beam.flavour  = BEAM_TELEPORT;
        beam.is_beam  = true;
        break;

    case SPELL_LEHUDIBS_CRYSTAL_SPEAR:      // was splinters
        beam.name     = "crystal spear";
        beam.damage   = dice_def( 3, 16 + power / 10 );
        beam.colour   = WHITE;
        beam.type     = dchar_glyph(DCHAR_FIRED_MISSILE);
        beam.flavour  = BEAM_MMISSILE;
        beam.hit      = 22 + power / 20;
        break;

    case SPELL_DIG:
        beam.flavour  = BEAM_DIGGING;
        beam.is_beam  = true;
        break;

    case SPELL_BOLT_OF_DRAINING:      // negative energy
        beam.name     = "bolt of negative energy";
        beam.damage   = dice_def( 3, 6 + power / 13 );
        beam.colour   = DARKGREY;
        beam.flavour  = BEAM_NEG;
        beam.hit      = 16 + power / 35;
        beam.is_beam  = true;
        break;

    case SPELL_ISKENDERUNS_MYSTIC_BLAST: // mystic blast
        beam.colour     = LIGHTMAGENTA;
        beam.name       = "orb of energy";
        beam.short_name = "energy";
        beam.damage     = dice_def( 3, 7 + (power / 14) );
        beam.hit        = 20 + (power / 20);
        beam.flavour    = BEAM_MMISSILE;
        break;

    case SPELL_STEAM_BALL:
        beam.colour   = LIGHTGREY;
        beam.name     = "ball of steam";
        beam.damage   = dice_def( 3, 7 + (power / 15) );
        beam.hit      = 20 + power / 20;
        beam.flavour  = BEAM_STEAM;
        break;

    case SPELL_PAIN:
        beam.flavour    = BEAM_PAIN;
        beam.damage     = dice_def( 1, 7 + (power / 20) );
        beam.ench_power = std::max(50, 8 * mons->hit_dice);
        beam.is_beam    = true;
        break;

    case SPELL_STICKY_FLAME_SPLASH:
    case SPELL_STICKY_FLAME:
        beam.colour   = RED;
        beam.name     = "sticky flame";
        beam.damage   = dice_def( 3, 3 + power / 50 );
        beam.hit      = 18 + power / 15;
        beam.flavour  = BEAM_FIRE;
        break;

    case SPELL_POISONOUS_CLOUD:
        beam.name     = "blast of poison";
        beam.damage   = dice_def( 3, 3 + power / 25 );
        beam.colour   = LIGHTGREEN;
        beam.flavour  = BEAM_POISON;
        beam.hit      = 18 + power / 25;
        beam.is_beam  = true;
        beam.is_big_cloud = true;
        break;

    case SPELL_ENERGY_BOLT:        // eye of devastation
        beam.colour     = YELLOW;
        beam.name       = "bolt of energy";
        beam.short_name = "energy";
        beam.damage     = dice_def( 3, 20 );
        beam.hit        = 15 + power / 30;
        beam.flavour    = BEAM_NUKE; // a magical missile which destroys walls
        beam.is_beam    = true;
        break;

    case SPELL_STING:              // sting
        beam.colour   = GREEN;
        beam.name     = "sting";
        beam.damage   = dice_def( 1, 6 + power / 25 );
        beam.hit      = 60;
        beam.flavour  = BEAM_POISON;
        break;

    case SPELL_BOLT_OF_IRON:
        beam.colour   = LIGHTCYAN;
        beam.name     = "iron bolt";
        beam.damage   = dice_def( 3, 8 + (power / 9) );
        beam.hit      = 20 + (power / 25);
        beam.type     = dchar_glyph(DCHAR_FIRED_MISSILE);
        beam.flavour  = BEAM_MMISSILE;   // similarly unresisted thing
        break;

    case SPELL_STONE_ARROW:
        beam.colour   = LIGHTGREY;
        beam.name     = "stone arrow";
        beam.damage   = dice_def( 3, 5 + (power / 10) );
        beam.hit      = 14 + power / 35;
        beam.type     = dchar_glyph(DCHAR_FIRED_MISSILE);
        beam.flavour  = BEAM_MMISSILE;   // similarly unresisted thing
        break;

    case SPELL_POISON_SPLASH:
        beam.colour   = GREEN;
        beam.name     = "splash of poison";
        beam.damage   = dice_def( 1, 4 + power / 10 );
        beam.hit      = 16 + power / 20;
        beam.flavour  = BEAM_POISON;
        break;

    case SPELL_ACID_SPLASH:
        beam.colour   = YELLOW;
        beam.name     = "splash of acid";
        beam.damage   = dice_def( 3, 7 );
        beam.hit      = 20 + (3 * mons->hit_dice);
        beam.flavour  = BEAM_ACID;
        break;

    case SPELL_DISINTEGRATE:
        beam.flavour    = BEAM_DISINTEGRATION;
        beam.ench_power = 50;
        beam.damage     = dice_def( 1, 30 + (power / 10) );
        beam.is_beam    = true;
        break;

    case SPELL_MEPHITIC_CLOUD:          // swamp drake, player ghost
        beam.name     = "foul vapour";
        beam.damage   = dice_def( 3, 2 + power / 25 );
        beam.colour   = GREEN;
        // FIXME: Players don't get the poison effect, only monsters
        // do. This should be changed (probably by changing monsters).
        beam.flavour  = BEAM_POISON;
        beam.hit      = 14 + power / 30;
        beam.is_beam  = true;
        beam.is_big_cloud = true;
        break;

    case SPELL_MIASMA:            // death drake
        beam.name     = "foul vapour";
        beam.damage   = dice_def( 3, 5 + power / 24 );
        beam.colour   = DARKGREY;
        beam.flavour  = BEAM_MIASMA;
        beam.hit      = 17 + power / 20;
        beam.is_beam  = true;
        beam.is_big_cloud = true;
        break;

    case SPELL_QUICKSILVER_BOLT:   // Quicksilver dragon
        beam.colour     = random_colour();
        beam.name       = "bolt of energy";
        beam.short_name = "energy";
        beam.damage     = dice_def( 3, 25 );
        beam.hit        = 16 + power / 25;
        beam.flavour    = BEAM_MMISSILE;
        break;

    case SPELL_HELLFIRE:           // fiend's hellfire
        beam.name         = "hellfire";
        beam.aux_source   = "blast of hellfire";
        beam.colour       = RED;
        beam.damage       = dice_def( 3, 25 );
        beam.hit          = 24;
        beam.flavour      = BEAM_HELLFIRE;
        beam.is_beam      = true;
        beam.is_explosion = true;
        break;

    case SPELL_METAL_SPLINTERS:
        beam.name       = "spray of metal splinters";
        beam.short_name = "metal splinters";
        beam.damage     = dice_def( 3, 20 + power / 20 );
        beam.colour     = CYAN;
        beam.flavour    = BEAM_FRAG;
        beam.hit        = 19 + power / 30;
        beam.is_beam    = true;
        break;

    case SPELL_BANISHMENT:
        beam.flavour  = BEAM_BANISH;
        beam.is_beam  = true;
        break;

    case SPELL_BLINK_OTHER:
        beam.flavour    = BEAM_BLINK;
        beam.is_beam    = true;
        break;

    case SPELL_FIRE_BREATH:
        beam.name       = "blast of flame";
        beam.aux_source = "blast of fiery breath";
        beam.damage     = dice_def( 3, (mons->hit_dice * 2) );
        beam.colour     = RED;
        beam.hit        = 30;
        beam.flavour    = BEAM_FIRE;
        beam.is_beam    = true;
        break;

    case SPELL_COLD_BREATH:
        beam.name       = "blast of cold";
        beam.aux_source = "blast of icy breath";
        beam.short_name = "frost";
        beam.damage     = dice_def( 3, (mons->hit_dice * 2) );
        beam.colour     = WHITE;
        beam.hit        = 30;
        beam.flavour    = BEAM_COLD;
        beam.is_beam    = true;
        break;

    case SPELL_DRACONIAN_BREATH:
        beam.damage      = dice_def( 3, (mons->hit_dice * 2) );
        beam.hit         = 30;
        beam.is_beam     = true;
        break;

    default:
        if (!is_valid_spell(real_spell))
            DEBUGSTR("Invalid spell #%d cast by %s", (int) real_spell,
                     mons->name(DESC_PLAIN, true).c_str());

        DEBUGSTR("Unknown monster spell '%s' cast by %s",
                 spell_title(real_spell),
                 mons->name(DESC_PLAIN, true).c_str());

        return (beam);
    }

    if (beam.is_enchantment())
    {
        beam.type = dchar_glyph(DCHAR_SPACE);
        beam.name = "0";
    }

    if (spell_cast == SPELL_DRACONIAN_BREATH)
        _scale_draconian_breath(beam, drac_type);

    // Accuracy is lowered by one quarter if the dragon is attacking
    // a target that is wielding a weapon of dragon slaying (which
    // makes the dragon/draconian avoid looking at the foe).
    // FIXME: This effect is not yet implemented for player draconians
    // or characters in dragon form breathing at monsters wielding a
    // weapon with this brand.
    if (is_dragonkind(mons))
    {
        if (actor *foe = mons->get_foe())
        {
            if (const item_def *weapon = foe->weapon())
            {
                if (get_weapon_brand(*weapon) == SPWPN_DRAGON_SLAYING)
                {
                    beam.hit *= 3;
                    beam.hit /= 4;
                }
            }
        }
    }

    return (beam);
}

static int _monster_abjure_square(const coord_def &pos,
                                  int pow, int actual,
                                  int wont_attack)
{
    const int mindex = mgrd(pos);
    if (mindex == NON_MONSTER)
        return (0);

    monsters *target = &menv[mindex];
    if (!target->alive()
        || ((bool)wont_attack == mons_wont_attack_real(target)))
    {
        return (0);
    }

    int duration;

    if (!target->is_summoned(&duration))
        return (0);

    pow = std::max(20, fuzz_value(pow, 40, 25));

    if (!actual)
        return (pow > 40 || pow >= duration);

    // TSO and Trog's abjuration protection.
    bool shielded = false;
    if (you.religion == GOD_SHINING_ONE)
    {
        pow = pow * (30 - target->hit_dice) / 30;
        if (pow < duration)
        {
            simple_god_message(" protects your fellow warrior from evil "
                               "magic!");
            shielded = true;
        }
    }
    else if (you.religion == GOD_TROG)
    {
        pow = pow * 4 / 5;
        if (pow < duration)
        {
            simple_god_message(" shields your ally from puny magic!");
            shielded = true;
        }
    }
    else if (is_sanctuary(target->pos()))
    {
        mpr("Zin's power protects your fellow warrior from evil magic!",
            MSGCH_GOD);
        return (0);
    }

#ifdef DEBUG_DIAGNOSTICS
    mprf(MSGCH_DIAGNOSTICS, "Abj: dur: %d, pow: %d, ndur: %d",
         duration, pow, duration - pow);
#endif

    mon_enchant abj = target->get_ench(ENCH_ABJ);
    if (!target->lose_ench_duration(abj, pow) && !shielded)
        simple_monster_message(target, " shudders.");

    return (1);
}

static int _apply_radius_around_square( const coord_def &c, int radius,
                                int (*fn)(const coord_def &, int, int, int),
                                int pow, int par1, int par2)
{
    int res = 0;
    for (int yi = -radius; yi <= radius; ++yi)
    {
        const coord_def c1(c.x - radius, c.y + yi);
        const coord_def c2(c.x + radius, c.y + yi);
        if (in_bounds(c1))
            res += fn(c1, pow, par1, par2);
        if (in_bounds(c2))
            res += fn(c2, pow, par1, par2);
    }

    for (int xi = -radius + 1; xi < radius; ++xi)
    {
        const coord_def c1(c.x + xi, c.y - radius);
        const coord_def c2(c.x + xi, c.y + radius);
        if (in_bounds(c1))
            res += fn(c1, pow, par1, par2);
        if (in_bounds(c2))
            res += fn(c2, pow, par1, par2);
    }
    return (res);
}

static int _monster_abjuration(const monsters *caster, bool actual)
{
    const bool wont_attack = mons_wont_attack_real(caster);
    int maffected = 0;

    if (actual)
        mpr("Send 'em back where they came from!");

    int pow = std::min(caster->hit_dice * 90, 2500);

    // Abjure radius.
    for (int rad = 1; rad < 5 && pow >= 30; ++rad)
    {
        int number_hit =
            _apply_radius_around_square(caster->pos(), rad,
                                        _monster_abjure_square,
                                        pow, actual, wont_attack);

        maffected += number_hit;

        // Each affected monster drops power.
        //
        // We could further tune this by the actual amount of abjuration
        // damage done to each summon, but the player will probably never
        // notice. :-)
        while (number_hit-- > 0)
            pow = pow * 90 / 100;

        pow /= 2;
    }
    return (maffected);
}

bool silver_statue_effects(monsters *mons)
{
    actor *foe = mons->get_foe();
    if (foe && mons->can_see(foe) && !one_chance_in(3))
    {
        const std::string msg =
            "'s eyes glow " + weird_glowing_colour() + '.';
        simple_monster_message(mons, msg.c_str(), MSGCH_WARN);

        create_monster(
            mgen_data(
                summon_any_demon((coinflip() ? DEMON_COMMON
                                             : DEMON_LESSER)),
                SAME_ATTITUDE(mons), 5, 0, foe->pos(), mons->foe));
        return (true);
    }
    return (false);
}

bool orange_statue_effects(monsters *mons)
{
    actor *foe = mons->get_foe();
    if (foe && mons->can_see(foe) && !one_chance_in(3))
    {
        if (you.can_see(foe))
        {
            if (foe == &you)
                mprf(MSGCH_WARN, "A hostile presence attacks your mind!");
            else if (you.can_see(mons))
                mprf(MSGCH_WARN, "%s fixes %s piercing gaze on %s.",
                     mons->name(DESC_CAP_THE).c_str(),
                     mons->pronoun(PRONOUN_NOCAP_POSSESSIVE).c_str(),
                     foe->name(DESC_NOCAP_THE).c_str());
        }

        MiscastEffect( foe, monster_index(mons), SPTYP_DIVINATION,
                       random2(15), random2(150),
                       "an orange crystal statue");
        return (true);
    }

    return (false);
}

bool orc_battle_cry(monsters *chief)
{
    const actor *foe = chief->get_foe();
    int affected = 0;

    if (foe
        && (foe != &you || !mons_friendly(chief))
        && !silenced(chief->pos())
        && chief->can_see(foe)
        && coinflip())
    {
        const int boss_index = monster_index(chief);
        const int level = chief->hit_dice > 12? 2 : 1;
        std::vector<monsters*> seen_affected;
        for (int i = 0; i < MAX_MONSTERS; ++i)
        {
            monsters *mon = &menv[i];
            if (mon != chief
                && mon->alive()
                && mons_species(mon->type) == MONS_ORC
                && mons_aligned(boss_index, i)
                && mon->hit_dice < chief->hit_dice
                && !mon->has_ench(ENCH_BERSERK)
                && !mon->cannot_move()
                && !mon->confused()
                && chief->can_see(mon))
            {
                mon_enchant ench = mon->get_ench(ENCH_BATTLE_FRENZY);
                if (ench.ench == ENCH_NONE || ench.degree < level)
                {
                    const int dur =
                        random_range(12, 20) * speed_to_duration(mon->speed);

                    if (ench.ench != ENCH_NONE)
                    {
                        ench.degree   = level;
                        ench.duration = std::max(ench.duration, dur);
                        mon->update_ench(ench);
                    }
                    else
                    {
                        mon->add_ench(mon_enchant(ENCH_BATTLE_FRENZY, level,
                                                  KC_OTHER, dur));
                    }

                    affected++;
                    if (you.can_see(mon))
                        seen_affected.push_back(mon);

                    if (mon->asleep())
                        behaviour_event(mon, ME_DISTURB, MHITNOT, chief->pos());
                }
            }
        }

        if (affected)
        {
            if (you.can_see(chief) && player_can_hear(chief->pos()))
            {
                mprf(MSGCH_SOUND, "%s roars a battle-cry!",
                     chief->name(DESC_CAP_THE).c_str());
            }

            // The yell happens whether you happen to see it or not.
            noisy(15, chief->pos());

            // Disabling detailed frenzy announcement because it's so spammy.
            const msg_channel_type channel =
                        mons_friendly_real(chief) ? MSGCH_MONSTER_ENCHANT
                                                  : MSGCH_FRIEND_ENCHANT;

            if (!seen_affected.empty())
            {
                std::string who;
                if (seen_affected.size() == 1)
                {
                    who = seen_affected[0]->name(DESC_CAP_THE);
                    mprf(channel, "%s goes into a battle-frenzy!", who.c_str());
                }
                else
                {
                    int type = seen_affected[0]->type;
                    for (unsigned int i = 0; i < seen_affected.size(); i++)
                    {
                        if (seen_affected[i]->type != type)
                        {
                            // just mention plain orcs
                            type = MONS_ORC;
                            break;
                        }
                    }
                    who = get_monster_data(type)->name;

                    mprf(channel, "%s %s go into a battle-frenzy!",
                         mons_friendly(chief) ? "Your" : "The",
                         pluralise(who).c_str());
                }
            }
        }
    }
    // Orc battle cry doesn't cost the monster an action.
    return (false);
}

static bool _make_monster_angry(const monsters *mon, monsters *targ)
{
    if (mons_friendly_real(mon) != mons_friendly_real(targ))
        return (false);

    // targ is guaranteed to have a foe (needs_berserk checks this).
    // Now targ needs to be closer to *its* foe than mon is (otherwise
    // mon might be in the way).

    coord_def victim;
    if (targ->foe == MHITYOU)
        victim = you.pos();
    else if (targ->foe != MHITNOT)
    {
        const monsters *vmons = &menv[targ->foe];
        if (!vmons->alive())
            return (false);
        victim = vmons->pos();
    }
    else
    {
        // Should be impossible. needs_berserk should find this case.
        ASSERT(false);
        return (false);
    }

    // If mon may be blocking targ from its victim, don't try.
    if (victim.distance_from(targ->pos()) > victim.distance_from(mon->pos()))
        return (false);

    if (mons_near(mon) && player_monster_visible(mon))
    {
        mprf("%s goads %s on!", mon->name(DESC_CAP_THE).c_str(),
             targ->name(DESC_NOCAP_THE).c_str());
    }

    targ->go_berserk(false);

    return (true);
}

bool moth_incite_monsters(const monsters *mon)
{
    if (is_sanctuary(you.pos()) || is_sanctuary(mon->pos()))
        return false;

    int goaded = 0;
    for (int i = 0; i < MAX_MONSTERS; ++i)
    {
        monsters *targ = &menv[i];
        if (targ == mon || !targ->alive() || !targ->needs_berserk())
            continue;

        if (mon->pos().distance_from(targ->pos()) > 3)
            continue;

        if (is_sanctuary(targ->pos()))
            continue;

        // Cannot goad other moths of wrath!
        if (targ->type == MONS_MOTH_OF_WRATH)
            continue;

        if (_make_monster_angry(mon, targ) && !one_chance_in(3 * ++goaded))
            return (true);
    }

    return (false);
}

void mons_clear_trapping_net(monsters *mon)
{
    if (!mons_is_caught(mon))
        return;

    const int net = get_trapping_net(mon->pos());
    if (net != NON_ITEM)
        remove_item_stationary(mitm[net]);

    mon->del_ench(ENCH_HELD, true);
}

bool mons_clonable(const monsters* mon, bool needs_adjacent)
{
    // No uniques, pandemonium lords or player ghosts.  Also, figuring
    // out the name for the clone of a named monster isn't worth it.
    if (mons_is_unique(mon->type) || mon->is_named() || mon->ghost.get())
        return (false);

    if (needs_adjacent)
    {
        // Is there space for the clone?
        bool square_found = false;
        for (int i = 0; i < 8; i++)
        {
            const coord_def p = mon->pos() + Compass[i];

            if (in_bounds(p) && p != you.pos() && mgrd(p) == NON_MONSTER
                && monster_habitable_grid(mon, grd(p)))
            {
                square_found = true;
                break;
            }
        }
        if (!square_found)
            return (false);
    }

    // Is the monster carrying an artefact?
    for (int i = 0; i < NUM_MONSTER_SLOTS; i++)
    {
        const int index = mon->inv[i];

        if (index == NON_ITEM)
            continue;

        if (is_artefact(mitm[index]))
            return (false);
    }

    return (true);
}

int clone_mons(const monsters* orig, bool quiet, bool* obvious,
               coord_def pos)
{
    // Is there an open slot in menv?
    int midx = NON_MONSTER;
    for (int i = 0; i < MAX_MONSTERS; i++)
        if (menv[i].type == -1)
        {
            midx = i;
            break;
        }

    if (midx == NON_MONSTER)
        return (NON_MONSTER);

    if (!in_bounds(pos))
    {
        // Find an adjacent square.
        int squares = 0;
        for (int i = 0; i < 8; i++)
        {
            const coord_def p = orig->pos() + Compass[i];

            if (in_bounds(p) && p != you.pos() && mgrd(p) == NON_MONSTER
                && monster_habitable_grid(orig, grd(p)))
            {
                if (one_chance_in(++squares))
                    pos = p;
            }
        }

        if (squares == 0)
            return (NON_MONSTER);
    }

    ASSERT(mgrd(pos) == NON_MONSTER && you.pos() != pos);

    monsters &mon(menv[midx]);

    mon          = *orig;
    mon.position = pos;
    mgrd(pos)    = midx;

    // Duplicate objects, or unequip them if they can't be duplicated.
    for (int i = 0; i < NUM_MONSTER_SLOTS; i++)
    {
        const int old_index = orig->inv[i];

        if (old_index == NON_ITEM)
            continue;

        const int new_index = get_item_slot(0);
        if (new_index == NON_ITEM)
        {
            mon.unequip(mitm[old_index], i, 0, true);
            mon.inv[i] = NON_ITEM;
            continue;
        }

        mon.inv[i]      = new_index;
        mitm[new_index] = mitm[old_index];
    }

    bool _obvious;
    if (obvious == NULL)
        obvious = &_obvious;
    *obvious = false;

    if (you.can_see(orig) && you.can_see(&mon))
    {
        if (!quiet)
            simple_monster_message(orig, " is duplicated!");
        *obvious = true;
    }

    mark_interesting_monst(&mon, mon.behaviour);
    if (you.can_see(&mon))
    {
        seen_monster(&mon);
        viewwindow(true, false);
    }

    if (crawl_state.arena)
        arena_placed_monster(&mon);

    return (midx);
}

std::string summoned_poof_msg(const monsters* monster, bool plural)
{
    int  summon_type = 0;
    bool valid_mon   = false;
    if (monster != NULL && !invalid_monster(monster))
    {
        (void) monster->is_summoned(NULL, &summon_type);
        valid_mon = true;
    }

    std::string msg      = "disappear%s in a puff of smoke";
    bool        no_chaos = false;

    switch (summon_type)
    {
    case SPELL_SHADOW_CREATURES:
        msg      = "dissolve%s into shadows";
        no_chaos = true;
        break;

    case MON_SUMM_CHAOS:
        msg = "degenerate%s into a cloud of primal chaos";
        break;

    case MON_SUMM_WRATH:
    case MON_SUMM_AID:
        if (valid_mon && is_good_god(monster->god))
        {
            msg      = "dissolve%s into sparkling lights";
            no_chaos = true;
        }
        break;
    }

    if (valid_mon)
    {
        if (monster->god == GOD_XOM && !no_chaos && one_chance_in(10)
            || monster->type == MONS_CHAOS_SPAWN)
        {
            msg = "degenerate%s into a cloud of primal chaos";
        }

        if (mons_is_holy(monster) && summon_type != SPELL_SHADOW_CREATURES
            && summon_type != MON_SUMM_CHAOS)
        {
            msg = "dissolve%s into sparkling lights";
        }
    }

    // Conjugate.
    msg = make_stringf(msg.c_str(), plural ? "" : "s");

    return (msg);
}

std::string summoned_poof_msg(const int midx, const item_def &item)
{
    if (midx == NON_MONSTER)
        return summoned_poof_msg(static_cast<const monsters*>(NULL), item);
    else
        return summoned_poof_msg(&menv[midx], item);
}

std::string summoned_poof_msg(const monsters* monster, const item_def &item)
{
    ASSERT(item.flags & ISFLAG_SUMMONED);

    return summoned_poof_msg(monster, item.quantity > 1);
}
