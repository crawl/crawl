/*
 *  File:     spl-clouds.cc
 *  Summary:  Cloud creating spells.
 */

#include "AppHdr.h"

#include "spl-clouds.h"
#include "externs.h"

#include <algorithm>

#include "beam.h"
#include "cloud.h"
#include "coord.h"
#include "coordit.h"
#include "env.h"
#include "exercise.h"
#include "fprop.h"
#include "godconduct.h"
#include "itemprop.h"
#include "items.h"
#include "libutil.h"
#include "message.h"
#include "misc.h"
#include "mon-behv.h"
#include "mon-util.h"
#include "ouch.h"
#include "player.h"
#include "skills.h"
#include "spl-util.h"
#include "stuff.h"
#include "terrain.h"
#include "transform.h"
#include "viewchar.h"
#include "shout.h"

// Returns whether the spell was actually cast.
bool conjure_flame(int pow, const coord_def& where)
{
    // FIXME: This would be better handled by a flag to enforce max range.
    if (distance(where, you.pos()) > dist_range(spell_range(SPELL_CONJURE_FLAME,
                                                      pow, true))
        || !in_bounds(where))
    {
        mpr("That's too far away.");
        return (false);
    }

    if (you.trans_wall_blocking(where))
    {
        mpr("A translucent wall is in the way.");
        return (false);
    }

    if (cell_is_solid(where))
    {
        switch (grd(where))
        {
        case DNGN_WAX_WALL:
            mpr("The flames aren't hot enough to melt wax walls!");
            break;
        case DNGN_METAL_WALL:
            mpr("You can't ignite solid metal!");
            break;
        case DNGN_GREEN_CRYSTAL_WALL:
            mpr("You can't ignite solid crystal!");
            break;
        case DNGN_TREE:
            mpr("The flames aren't hot enough to burn down trees!");
            break;
        default:
            mpr("You can't ignite solid rock!");
            break;
        }
        return (false);
    }

    const int cloud = env.cgrid(where);

    if (cloud != EMPTY_CLOUD && env.cloud[cloud].type != CLOUD_FIRE)
    {
        mpr("There's already a cloud there!");
        return (false);
    }

    // Note that self-targeting is handled by SPFLAG_NOT_SELF.
    monster* mons = monster_at(where);
    if (mons)
    {
        if (you.can_see(mons))
        {
            mpr("You can't place the cloud on a creature.");
            return (false);
        }
        else
        {
            // FIXME: maybe should do _paranoid_option_disable() here?
            mpr("You see a ghostly outline there, and the spell fizzles.");
            return (true);      // Don't give free detection!
        }
    }

    if (cloud != EMPTY_CLOUD)
    {
        // Reinforce the cloud - but not too much.
        // It must be a fire cloud from a previous test.
        mpr("The fire roars with new energy!");
        const int extra_dur = 2 + std::min(random2(pow) / 2, 20);
        env.cloud[cloud].decay += extra_dur * 5;
        env.cloud[cloud].set_whose(KC_YOU);
    }
    else
    {
        const int durat = std::min(5 + (random2(pow)/2) + (random2(pow)/2), 23);
        place_cloud(CLOUD_FIRE, where, durat, KC_YOU);
        mpr("The fire roars!");
    }
    noisy(2, where);

    return (true);
}

// Assumes beem.range has already been set. -cao
bool stinking_cloud( int pow, bolt &beem )
{
    beem.name        = "stinking cloud";
    beem.colour      = GREEN;
    beem.damage      = dice_def( 1, 0 );
    beem.hit         = 20;
    beem.glyph       = dchar_glyph(DCHAR_FIRED_ZAP);
    beem.flavour     = BEAM_POTION_STINKING_CLOUD;
    beem.ench_power  = pow;
    beem.beam_source = MHITYOU;
    beem.thrower     = KILL_YOU;
    beem.is_beam     = false;
    beem.is_explosion = true;
    beem.aux_source.clear();

    // Fire tracer.
    beem.source            = you.pos();
    beem.can_see_invis     = you.can_see_invisible();
    beem.smart_monster     = true;
    beem.attitude          = ATT_FRIENDLY;
    beem.friend_info.count = 0;
    beem.is_tracer         = true;
    beem.fire();

    if (beem.beam_cancelled)
    {
        // We don't want to fire through friendlies.
        canned_msg(MSG_OK);
        return (false);
    }

    // Really fire.
    beem.is_tracer = false;
    beem.fire();

    return (true);
}

int cast_big_c(int pow, cloud_type cty, kill_category whose, bolt &beam)
{
    big_cloud( cty, whose, beam.target, pow, 8 + random2(3), -1 );
    noisy(2, beam.target);
    return (1);
}

void big_cloud(cloud_type cl_type, kill_category whose,
               const coord_def& where, int pow, int size, int spread_rate,
               int colour, std::string name, std::string tile)
{
    big_cloud(cl_type, whose, cloud_struct::whose_to_killer(whose),
              where, pow, size, spread_rate, colour, name, tile);
}

void big_cloud(cloud_type cl_type, killer_type killer,
               const coord_def& where, int pow, int size, int spread_rate,
               int colour, std::string name, std::string tile)
{
    big_cloud(cl_type, cloud_struct::killer_to_whose(killer), killer,
              where, pow, size, spread_rate, colour, name, tile);
}

void big_cloud(cloud_type cl_type, kill_category whose, killer_type killer,
               const coord_def& where, int pow, int size, int spread_rate,
               int colour, std::string name, std::string tile)
{
    apply_area_cloud(make_a_normal_cloud, where, pow, size,
                     cl_type, whose, killer, spread_rate, colour, name, tile);
}

void cast_ring_of_flames(int power)
{
    // You shouldn't be able to cast this in the rain. {due}
    if (in_what_cloud(CLOUD_RAIN))
    {
        mpr("Your spell sizzles in the rain.");
        return;
    }
    you.increase_duration(DUR_FIRE_SHIELD,
                          5 + (power / 10) + (random2(power) / 5), 50,
                          "The air around you leaps into flame!");
    manage_fire_shield(1);
}

void manage_fire_shield(int delay)
{
    ASSERT(you.duration[DUR_FIRE_SHIELD]);

    int old_dur = you.duration[DUR_FIRE_SHIELD];

    you.duration[DUR_FIRE_SHIELD]-= delay;
    if (you.duration[DUR_FIRE_SHIELD] < 0)
        you.duration[DUR_FIRE_SHIELD] = 0;

    if (!you.duration[DUR_FIRE_SHIELD])
    {
        mpr("Your ring of flames gutters out.", MSGCH_DURATION);
        return;
    }

    int threshold = get_expiration_threshold(DUR_FIRE_SHIELD);


    if (old_dur > threshold && you.duration[DUR_FIRE_SHIELD] < threshold)
        mpr("Your ring of flames is guttering out.", MSGCH_WARN);

    // Place fire clouds all around you
    for (adjacent_iterator ai(you.pos()); ai; ++ai)
        if (!feat_is_solid(grd(*ai)) && env.cgrid(*ai) == EMPTY_CLOUD)
            place_cloud( CLOUD_FIRE, *ai, 1 + random2(6), KC_YOU );
}

void corpse_rot()
{
    for (radius_iterator ri(you.pos(), 6, C_ROUND, you.get_los_no_trans());
         ri; ++ri)
    {
        if (!is_sanctuary(*ri) && env.cgrid(*ri) == EMPTY_CLOUD)
            for (stack_iterator si(*ri); si; ++si)
                if (si->base_type == OBJ_CORPSES && si->sub_type == CORPSE_BODY)
                {
                    // Found a corpse.  Skeletonise it if possible.
                    if (!mons_skeleton(si->plus))
                        destroy_item(si->index());
                    else
                        turn_corpse_into_skeleton(*si);

                    place_cloud(CLOUD_MIASMA, *ri, 4+random2avg(16, 3), KC_YOU);

                    // Don't look for more corpses here.
                    break;
                }
    }

    if (you.can_smell())
        mpr("You smell decay.");

    // Should make zombies decay into skeletons?
}

int make_a_normal_cloud(coord_def where, int pow, int spread_rate,
                        cloud_type ctype, kill_category whose,
                        killer_type killer, int colour, std::string name,
                        std::string tile)
{
    if (killer == KILL_NONE)
        killer = cloud_struct::whose_to_killer(whose);

    place_cloud( ctype, where,
                 (3 + random2(pow / 4) + random2(pow / 4) + random2(pow / 4)),
                 whose, killer, spread_rate, colour, name, tile );

    return 1;
}

// Returns a vector of cloud types created by this potion type.
// FIXME: Heavily duplicated code.
static std::vector<int> _get_evaporate_result(int potion)
{
    std::vector <int> beams;
    bool random_potion = false;
    switch (potion)
    {
    case POT_STRONG_POISON:
    case POT_POISON:
        beams.push_back(BEAM_POTION_POISON);
        break;

    case POT_DEGENERATION:
        beams.push_back(BEAM_POTION_POISON);
        beams.push_back(BEAM_POTION_MIASMA);
        break;

    case POT_DECAY:
        beams.push_back(BEAM_POTION_MIASMA);
        break;

    case POT_PARALYSIS:
    case POT_CONFUSION:
    case POT_SLOWING:
        beams.push_back(BEAM_POTION_STINKING_CLOUD);
        break;

    case POT_WATER:
    case POT_PORRIDGE:
        beams.push_back(BEAM_POTION_STEAM);
        break;

    case POT_BLOOD:
    case POT_BLOOD_COAGULATED:
        beams.push_back(BEAM_POTION_STINKING_CLOUD);
        // deliberate fall through
    case POT_BERSERK_RAGE:
        beams.push_back(BEAM_POTION_FIRE);
        beams.push_back(BEAM_POTION_STEAM);
        break;

    case POT_MUTATION:
        beams.push_back(BEAM_POTION_MUTAGENIC);
        // deliberate fall-through
    case POT_GAIN_STRENGTH:
    case POT_GAIN_DEXTERITY:
    case POT_GAIN_INTELLIGENCE:
    case POT_EXPERIENCE:
    case POT_MAGIC:
        beams.push_back(BEAM_POTION_FIRE);
        beams.push_back(BEAM_POTION_COLD);
        beams.push_back(BEAM_POTION_POISON);
        beams.push_back(BEAM_POTION_MIASMA);
        random_potion = true;
        break;

    default:
        beams.push_back(BEAM_POTION_FIRE);
        beams.push_back(BEAM_POTION_STINKING_CLOUD);
        beams.push_back(BEAM_POTION_COLD);
        beams.push_back(BEAM_POTION_POISON);
        beams.push_back(BEAM_POTION_BLUE_SMOKE);
        beams.push_back(BEAM_POTION_STEAM);
        random_potion = true;
        break;
    }

    std::vector<int> clouds;
    for (unsigned int k = 0; k < beams.size(); ++k)
        clouds.push_back(beam2cloud((beam_type) beams[k]));

    if (random_potion)
    {
        // handled in beam.cc
        clouds.push_back(CLOUD_FIRE);
        clouds.push_back(CLOUD_STINK);
        clouds.push_back(CLOUD_COLD);
        clouds.push_back(CLOUD_POISON);
        clouds.push_back(CLOUD_BLUE_SMOKE);
        clouds.push_back(CLOUD_STEAM);
    }

    return (clouds);
}

// Returns a comma-separated list of all cloud types potentially created
// by this potion type. Doesn't respect the different probabilities.
std::string get_evaporate_result_list(int potion)
{
    std::vector<int> clouds = _get_evaporate_result(potion);
    std::sort(clouds.begin(), clouds.end());

    std::vector<std::string> clouds_list;

    int old_cloud = -1;
    for (unsigned int k = 0; k < clouds.size(); ++k)
    {
        const int new_cloud = clouds[k];
        if (new_cloud == old_cloud)
            continue;

        // This relies on all smoke types being handled as blue.
        if (new_cloud == CLOUD_BLUE_SMOKE)
            clouds_list.push_back("coloured smoke");
        else
            clouds_list.push_back(cloud_type_name((cloud_type) new_cloud));

        old_cloud = new_cloud;
    }

    return comma_separated_line(clouds_list.begin(), clouds_list.end(),
                                " or ", ", ");
}


// Assumes beem.range is already set -cao
bool cast_evaporate(int pow, bolt& beem, int pot_idx)
{
    ASSERT(you.inv[pot_idx].base_type == OBJ_POTIONS);
    item_def& potion = you.inv[pot_idx];

    beem.name        = "potion";
    beem.colour      = potion.colour;
    beem.glyph       = dchar_glyph(DCHAR_FIRED_FLASK);
    beem.beam_source = MHITYOU;
    beem.thrower     = KILL_YOU_MISSILE;
    beem.is_beam     = false;
    beem.aux_source.clear();

    beem.hit        = you.dex() / 2 + roll_dice( 2, you.skills[SK_THROWING] / 2 + 1 );
    beem.damage     = dice_def( 1, 0 );  // no damage, just producing clouds
    beem.ench_power = pow;               // used for duration only?
    beem.is_explosion = true;

    beem.flavour    = BEAM_POTION_STINKING_CLOUD;
    beam_type tracer_flavour = BEAM_MMISSILE;

    switch (potion.sub_type)
    {
    case POT_STRONG_POISON:
        beem.ench_power *= 2;
        // deliberate fall-through
    case POT_POISON:
        beem.flavour   = BEAM_POTION_POISON;
        tracer_flavour = BEAM_POISON;
        break;

    case POT_DEGENERATION:
        beem.effect_known = false;
        beem.flavour   = (coinflip() ? BEAM_POTION_POISON : BEAM_POTION_MIASMA);
        tracer_flavour = BEAM_MIASMA;
        beem.ench_power *= 2;
        break;

    case POT_DECAY:
        beem.flavour   = BEAM_POTION_MIASMA;
        tracer_flavour = BEAM_MIASMA;
        beem.ench_power *= 2;
        break;

    case POT_PARALYSIS:
        beem.ench_power *= 2;
        // fall through
    case POT_CONFUSION:
    case POT_SLOWING:
        tracer_flavour = beem.flavour = BEAM_POTION_STINKING_CLOUD;
        break;

    case POT_WATER:
    case POT_PORRIDGE:
        tracer_flavour = beem.flavour = BEAM_POTION_STEAM;
        break;

    case POT_BLOOD:
    case POT_BLOOD_COAGULATED:
        if (one_chance_in(3))
            break; // stinking cloud
        // deliberate fall through
    case POT_BERSERK_RAGE:
        beem.effect_known = false;
        beem.flavour = (coinflip() ? BEAM_POTION_FIRE : BEAM_POTION_STEAM);
        if (potion.sub_type == POT_BERSERK_RAGE)
            tracer_flavour = BEAM_FIRE;
        else
            tracer_flavour = BEAM_RANDOM;
        break;

    case POT_MUTATION:
        // Maybe we'll get a mutagenic cloud.
        if (coinflip())
        {
            beem.effect_known = true;
            tracer_flavour = beem.flavour = BEAM_POTION_MUTAGENIC;
            break;
        }
        // if not, deliberate fall through for something random

    case POT_GAIN_STRENGTH:
    case POT_GAIN_DEXTERITY:
    case POT_GAIN_INTELLIGENCE:
    case POT_EXPERIENCE:
    case POT_MAGIC:
        beem.effect_known = false;
        switch (random2(5))
        {
        case 0:   beem.flavour = BEAM_POTION_FIRE;   break;
        case 1:   beem.flavour = BEAM_POTION_COLD;   break;
        case 2:   beem.flavour = BEAM_POTION_POISON; break;
        case 3:   beem.flavour = BEAM_POTION_MIASMA; break;
        default:  beem.flavour = BEAM_POTION_RANDOM; break;
        }
        tracer_flavour = BEAM_RANDOM;
        break;

    default:
        beem.effect_known = false;
        switch (random2(12))
        {
        case 0:   beem.flavour = BEAM_POTION_FIRE;            break;
        case 1:   beem.flavour = BEAM_POTION_STINKING_CLOUD;  break;
        case 2:   beem.flavour = BEAM_POTION_COLD;            break;
        case 3:   beem.flavour = BEAM_POTION_POISON;          break;
        case 4:   beem.flavour = BEAM_POTION_RANDOM;          break;
        case 5:   beem.flavour = BEAM_POTION_BLUE_SMOKE;      break;
        case 6:   beem.flavour = BEAM_POTION_BLACK_SMOKE;     break;
        default:  beem.flavour = BEAM_POTION_STEAM;           break;
        }
        tracer_flavour = BEAM_RANDOM;
        break;
    }

    // Fire tracer. FIXME: use player_tracer() here!
    beem.source         = you.pos();
    beem.can_see_invis  = you.can_see_invisible();
    beem.smart_monster  = true;
    beem.attitude       = ATT_FRIENDLY;
    beem.beam_cancelled = false;
    beem.is_tracer      = true;
    beem.friend_info.reset();

    beam_type real_flavour = beem.flavour;
    beem.flavour           = tracer_flavour;
    beem.fire();

    if (beem.beam_cancelled)
    {
        // We don't want to fire through friendlies or at ourselves.
        canned_msg(MSG_OK);
        return (false);
    }

    practise(EX_WILL_THROW_POTION);

    // Really fire.
    beem.flavour = real_flavour;
    beem.is_tracer = false;
    beem.fire();

    // Use up a potion.
    if (is_blood_potion(potion))
        remove_oldest_blood_potion(potion);

    dec_inv_item_quantity(pot_idx, 1);

    return (true);
}

int holy_flames(monster* caster, actor* defender)
{
    const coord_def pos = defender->pos();
    int cloud_count = 0;

    for ( adjacent_iterator ai(pos); ai; ++ai )
    {
        if (!in_bounds(*ai)
            || env.cgrid(*ai) != EMPTY_CLOUD
            || feat_is_solid(grd(*ai))
            || is_sanctuary(*ai)
            || monster_at(*ai))
        {
            continue;
        }

        place_cloud(CLOUD_HOLY_FLAMES, *ai, caster->hit_dice * 5, KILL_MON);

        cloud_count++;
    }

    return cloud_count;
}
