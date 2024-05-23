/**
 * @file
 * @brief Cloud creating spells.
**/

#include "AppHdr.h"

#include "spl-clouds.h"

#include <algorithm>

#include "cloud.h"
#include "coord.h"
#include "coordit.h"
#include "corpse.h"
#include "directn.h"
#include "english.h"
#include "env.h"
#include "fprop.h"
#include "fight.h"
#include "fineff.h" // explosion_fineff, etc
#include "items.h"
#include "level-state-type.h"
#include "message.h"
#include "mon-behv.h" // ME_WHACK
#include "ouch.h"
#include "random-pick.h"
#include "shout.h"
#include "spl-util.h"
#include "target.h"
#include "terrain.h"

spret cast_dreadful_rot(int pow, bool fail)
{
    if (cloud_at(you.pos()))
    {
        mpr("There's already a cloud here!");
        return spret::abort;
    }

    fail_check();

    const int min_dur = 6;
    const int max_dur = 9 + div_rand_round(pow, 10);
    you.props[MIASMA_IMMUNE_KEY] = true;
    place_cloud(CLOUD_MIASMA, you.pos(), random_range(min_dur, max_dur), &you);
    mpr("A part of your flesh rots into a cloud of miasma!");
    drain_player(27, true, true);

    return spret::success;
}

spret kindle_blastmotes(int pow, bool fail)
{
    if (cloud_at(you.pos()))
    {
        mpr("There's already a cloud here!");
        return spret::abort;
    }

    fail_check();

    // Really should be per-cloud, but skeptical people are changing power
    // between successive blastmote casts that often.
    you.props[BLASTMOTE_POWER_KEY] = pow;
    // Longish duration to support setting up silly traps.
    you.props[BLASTMOTE_IMMUNE_KEY] = true;
    place_cloud(CLOUD_BLASTMOTES, you.pos(), random_range(20, 30), &you);
    mpr("A cloud of volatile blastmotes flares up around you! Run!");

    return spret::success;
}

void explode_blastmotes_at(coord_def p)
{
    // Assumes all blastmotes are created by the player.
    // We could fix this in future by checking the 'killer'
    // associated with the cloud being deleted.
    delete_cloud(p);

    bolt beam;
    zappy(ZAP_BLASTMOTE, you.props[BLASTMOTE_POWER_KEY], false, beam);

    beam.target        = p;
    beam.source        = p;
    beam.source_id     = MID_PLAYER;
    beam.attitude      = ATT_FRIENDLY;
    beam.thrower       = KILL_YOU_MISSILE;
    beam.is_explosion  = true;
    beam.ex_size       = 1;

    const string boom  = "The cloud of blastmotes explodes!";
    const string sanct = "By Zin's power, the fiery explosion is contained.";
    explosion_fineff::schedule(beam, boom, sanct, EXPLOSION_FINEFF_CONCUSSION,
                               nullptr);
}

cloud_type spell_to_cloud(spell_type spell)
{
    static map<spell_type, cloud_type> cloud_map =
    {
        { SPELL_POISONOUS_CLOUD, CLOUD_POISON },
        { SPELL_FREEZING_CLOUD, CLOUD_COLD },
        { SPELL_HOLY_BREATH, CLOUD_HOLY },
    };

    return lookup(cloud_map, spell, CLOUD_NONE);
}

spret cast_big_c(int pow, spell_type spl, const actor *caster, bolt &beam,
                 bool fail)
{
    if (grid_distance(beam.target, you.pos()) > beam.range
        || !in_bounds(beam.target))
    {
        mpr("That is beyond the maximum range.");
        return spret::abort;
    }

    if (cell_is_solid(beam.target))
    {
        const char *feat = feat_type_name(env.grid(beam.target));
        mprf("You can't place clouds on %s.", article_a(feat).c_str());
        return spret::abort;
    }

    cloud_type cty = spell_to_cloud(spl);
    if (is_sanctuary(beam.target) && !is_harmless_cloud(cty))
    {
        mpr("You can't place harmful clouds in a sanctuary.");
        return spret::abort;
    }

    //XXX: there should be a better way to specify beam cloud types
    switch (spl)
    {
        case SPELL_POISONOUS_CLOUD:
            beam.flavour = BEAM_POISON;
            beam.name = "blast of poison";
            break;
        case SPELL_HOLY_BREATH:
            beam.flavour = BEAM_HOLY;
            beam.origin_spell = SPELL_HOLY_BREATH;
            break;
        case SPELL_FREEZING_CLOUD:
            beam.flavour = BEAM_COLD;
            beam.name = "freezing blast";
            break;
        default:
            break;
    }

    if (cty == CLOUD_NONE)
    {
        mpr("That kind of cloud doesn't exist!");
        return spret::abort;
    }

    beam.thrower           = KILL_YOU;
    beam.hit               = AUTOMATIC_HIT;
    beam.damage            = CONVENIENT_NONZERO_DAMAGE;
    beam.is_tracer         = true;
    beam.use_target_as_pos = true;
    beam.origin_spell      = spl;
    beam.affect_endpoint();
    if (beam.beam_cancelled)
        return spret::abort;

    fail_check();

    big_cloud(cty, caster, beam.target, pow, 8 + random2(3), -1);
    noisy(spell_effect_noise(spl), beam.target);
    return spret::success;
}

/*
 * A cloud_func that places an individual cloud as part of a cloud area. This
 * function is called by apply_area_cloud();
 *
 * @param where       The location of the cloud.
 * @param pow         The spellpower of the spell placing the clouds, which
 *                    determines how long the cloud will last.
 * @param spread_rate How quickly the cloud spreads.
 * @param ctype       The type of cloud to place.
 * @param agent       Any agent that may have caused the cloud. If this is the
 *                    player, god conducts are applied.
 * @param excl_rad    How large of an exclusion radius to make around the
 *                    cloud.
 * @returns           The number of clouds made, which is always 1.
 *
*/
static int _make_a_normal_cloud(coord_def where, int pow, int spread_rate,
                                cloud_type ctype, const actor *agent,
                                int excl_rad)
{
    place_cloud(ctype, where,
                (3 + random2(pow / 4) + random2(pow / 4) + random2(pow / 4)),
                agent, spread_rate, excl_rad);

    return 1;
}

/*
 * Make a large area of clouds centered on a given place. This never creates
 * player exclusions.
 *
 * @param ctype       The type of cloud to place.
 * @param agent       Any agent that may have caused the cloud. If this is the
 *                    player, god conducts are applied.
 * @param where       The location of the cloud.
 * @param pow         The spellpower of the spell placing the clouds, which
 *                    determines how long the cloud will last.
 * @param size        How large a radius of clouds to place from the `where'
 *                    argument.
 * @param spread_rate How quickly the cloud spreads.
*/
void big_cloud(cloud_type cl_type, const actor *agent,
               const coord_def& where, int pow, int size, int spread_rate)
{
    // The starting point _may_ be a place no cloud can be placed on.
    apply_area_cloud(_make_a_normal_cloud, where, pow, size,
                     cl_type, agent, spread_rate, -1);
}

void holy_flames(monster* caster, actor* defender)
{
    const coord_def pos = defender->pos();
    int cloud_count = 0;
    const int dur = 8 + random2avg(caster->get_hit_dice() * 3, 2);

    for (adjacent_iterator ai(pos); ai; ++ai)
    {
        if (!in_bounds(*ai)
            || cell_is_solid(*ai)
            || is_sanctuary(*ai)
            || monster_at(*ai))
        {
            continue;
        }

        place_cloud(CLOUD_HOLY, *ai, dur, caster);

        cloud_count++;
    }

    if (cloud_count)
    {
        if (defender->is_player())
            mpr("Blessed fire suddenly surrounds you!");
        else
            simple_monster_message(*defender->as_monster(),
                                   " is surrounded by blessed fire!");
    }
}

spret scroll_of_poison(bool scroll_unknown)
{
    int created = 0;
    bool unknown_unseen = false;
    for (radius_iterator ri(you.pos(), LOS_NO_TRANS); ri; ++ri)
    {
        if (cell_is_solid(*ri))
            continue;
        if (cloud_type_at(*ri) != CLOUD_NONE)
            continue;
        const actor* act = actor_at(*ri);
        if (act != nullptr)
        {
            unknown_unseen = unknown_unseen || !you.can_see(*act);
            continue;
        }

        place_cloud(CLOUD_POISON, *ri, 10 + random2(11), &you);
        ++created;
    }

    if (created > 0)
    {
        mpr("The air fills with toxic fumes!");
        return spret::success;
    }
    if (!scroll_unknown && !unknown_unseen)
    {
        mpr("There's no open space to fill with poison.");
        return spret::abort;
    }
    canned_msg(MSG_NOTHING_HAPPENS);
    return spret::fail;
}
