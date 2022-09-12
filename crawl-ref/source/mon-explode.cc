/*
* @file
* @brief Monster explosion functionality.
**/

#include "AppHdr.h"

#include "mon-explode.h"

#include "beam.h"
#include "cloud.h"
#include "coordit.h"
#include "dungeon-char-type.h"
#include "english.h"
#include "env.h"
#include "fineff.h"
#include "fprop.h"
#include "message.h"
#include "monster.h"
#include "mon-death.h" // YOU_KILL
#include "mon-place.h"
#include "mon-util.h"
#include "mpr.h"
#include "spl-damage.h"
#include "spl-goditem.h"
#include "state.h"
#include "stringutil.h"
#include "target.h"
#include "terrain.h"
#include "torment-source-type.h"
#include "view.h"
#include "viewchar.h"

static void _setup_base_explosion(bolt & beam, const monster& origin)
{
    beam.is_tracer    = false;
    beam.is_explosion = true;
    beam.is_death_effect = true;
    beam.source_id    = origin.mid;
    beam.glyph        = dchar_glyph(DCHAR_FIRED_BURST);
    beam.source       = origin.pos();
    beam.source_name  = origin.base_name(DESC_BASENAME, true);
    beam.target       = origin.pos();
    beam.explode_noise_msg = "You hear an explosion!";

    if (!crawl_state.game_is_arena() && origin.attitude == ATT_FRIENDLY
        && !origin.is_summoned())
    {
        beam.thrower = KILL_YOU;
    }
    else
        beam.thrower = KILL_MON;

    beam.aux_source.clear();
    beam.attitude = origin.attitude;
}

static void _setup_inferno_explosion(bolt & beam, const monster& origin)
{
    _setup_base_explosion(beam, origin);
    setup_fire_storm(&origin, 100, beam);
    beam.refine_for_explosion();
}

void setup_spore_explosion(bolt & beam, const monster& origin)
{
    _setup_base_explosion(beam, origin);
    beam.flavour = BEAM_SPORE;
    beam.damage  = dice_def(3, 5 + origin.get_hit_dice());
    beam.name    = "explosion of spores";
    beam.colour  = LIGHTGREY;
    beam.ex_size = 1;
}

dice_def ball_lightning_damage(int hd)
{
    return dice_def(3, 5 + hd * 5 / 4);
}

static void _setup_lightning_explosion(bolt & beam, const monster& origin)
{
    _setup_base_explosion(beam, origin);
    beam.flavour   = BEAM_ELECTRICITY;
    beam.damage    = ball_lightning_damage(origin.get_hit_dice());
    beam.name      = "blast of lightning";
    beam.explode_noise_msg = "You hear a clap of thunder!";
    beam.colour    = LIGHTCYAN;
    beam.ex_size   = x_chance_in_y(origin.get_hit_dice(), 24) ? 3 : 2;
    if (origin.summoner)
        beam.origin_spell = SPELL_CONJURE_BALL_LIGHTNING;
    // Don't credit the player for ally-summoned ball lightning explosions.
    if (origin.summoner && origin.summoner != MID_PLAYER)
        beam.thrower = KILL_MON;
}

dice_def prism_damage(int hd, bool fully_powered)
{
    const int dice = fully_powered ? 3 : 2;
    return dice_def(dice, 6 + hd * 7 / 4);
}

static void _setup_prism_explosion(bolt& beam, const monster& origin)
{
    _setup_base_explosion(beam, origin);
    beam.flavour = BEAM_MMISSILE;
    beam.damage  = prism_damage(origin.get_hit_dice(), origin.prism_charge == 2);
    beam.name    = "blast of energy";
    beam.colour  = MAGENTA;
    beam.ex_size = origin.prism_charge;
    if (origin.summoner)
        beam.origin_spell = SPELL_FULMINANT_PRISM;
    dprf("prism hd: %d, damage: %dd%d", origin.get_hit_dice(),
         beam.damage.num, beam.damage.size);
}

static void _setup_bennu_explosion(bolt& beam, const monster& origin)
{
    _setup_base_explosion(beam, origin);
    beam.flavour = BEAM_NEG;
    beam.damage  = dice_def(3, 5 + origin.get_hit_dice() * 5 / 4);
    beam.name    = "pyre of ghostly fire";
    beam.explode_noise_msg = "You hear an otherworldly crackling!";
    beam.colour  = CYAN;
    beam.ex_size = 2;
}

static void _setup_inner_flame_explosion(bolt & beam, const monster& origin,
                                         actor* agent)
{
    _setup_base_explosion(beam, origin);
    const int size   = origin.body_size(PSIZE_BODY);
    beam.flavour     = BEAM_FIRE;
    beam.damage      = (size > SIZE_LARGE) ? dice_def(3, 25) :
                       (size > SIZE_TINY)  ? dice_def(3, 20) :
                                             dice_def(3, 15);
    beam.name        = "fiery explosion";
    beam.colour      = RED;
    beam.ex_size     = (size > SIZE_LARGE) ? 2 : 1;
    beam.source_name = origin.name(DESC_PLAIN, true);
    beam.origin_spell = SPELL_INNER_FLAME;
    beam.thrower     = (agent && agent->is_player()) ? KILL_YOU_MISSILE
                                                     : KILL_MON_MISSILE;
    if (agent)
        beam.source_id = agent->mid;
}

static void _setup_bloated_husk_explosion(bolt & beam, const monster& origin)
{
    _setup_base_explosion(beam, origin);
    beam.flavour = BEAM_MMISSILE;
    beam.damage  = dice_def(8, origin.get_hit_dice());
    beam.name    = "blast of putrescent gases";
    beam.explode_noise_msg = "You hear an high-pitched explosion!";
    beam.colour  = GREEN;
    beam.ex_size = 2;

}

struct monster_explosion {
    function<void(bolt&, const monster&)> prep_explode;
    string sanct_effect;
};

static const map<monster_type, monster_explosion> explosions {
    { MONS_BALLISTOMYCETE_SPORE, { setup_spore_explosion } },
    { MONS_BALL_LIGHTNING, { _setup_lightning_explosion } },
    { MONS_LURKING_HORROR, { nullptr, "torment is averted" } },
    { MONS_FULMINANT_PRISM, { _setup_prism_explosion } },
    { MONS_BENNU, { _setup_bennu_explosion, "fires are quelled" } },
    { MONS_BLOATED_HUSK, { _setup_bloated_husk_explosion } },
    { MONS_CREEPING_INFERNO, { _setup_inferno_explosion } },
};

// When this monster dies, does it explode?
bool mon_explodes_on_death(monster_type mc)
{
    return explosions.find(mc) != explosions.end();
}

bool monster_explodes(const monster &mons)
{
    if (mons.has_ench(ENCH_INNER_FLAME))
        return true;
    if (!mon_explodes_on_death(mons.type))
        return false;
    if (mons.type == MONS_FULMINANT_PRISM && mons.prism_charge <= 0)
        return false;
    return true;
}

bool explode_monster(monster* mons, killer_type killer,
                             bool pet_kill, bool wizard)
{
    if (mons->hit_points > 0 || mons->hit_points <= -15 || wizard
        || killer == KILL_RESET || killer == KILL_DISMISSED
        || killer == KILL_BANISHED)
    {
        if (killer != KILL_TIMEOUT)
            return false;
    }

    bolt beam;
    const monster_type type = mons->type;
    string sanct_msg = "";
    string boom_msg = make_stringf("%s explodes!", mons->full_name(DESC_THE).c_str());
    actor* agent = nullptr;
    bool inner_flame = false;

    auto it = explosions.find(type);
    if (it != explosions.end())
    {
        const monster_explosion &explosion = it->second;
        if (explosion.prep_explode)
            explosion.prep_explode(beam, *mons);
        string effect = "explosion is contained";
        if (explosion.sanct_effect != "")
            effect = explosion.sanct_effect;
        sanct_msg = string("By Zin's power, ") +
                    apostrophise(mons->name(DESC_THE)) + " " +
                    effect + ".";
        if (type == MONS_BENNU)
            boom_msg = make_stringf("%s blazes out!", mons->full_name(DESC_THE).c_str());
    }
    else
    {
        if (!mons->has_ench(ENCH_INNER_FLAME))
        {
            msg::streams(MSGCH_DIAGNOSTICS) << "Unknown spore type: "
                                            << static_cast<int>(type)
                                            << endl;
            return false;
        }
        mon_enchant i_f = mons->get_ench(ENCH_INNER_FLAME);
        ASSERT(i_f.ench == ENCH_INNER_FLAME);
        agent = actor_by_mid(i_f.source);
        _setup_inner_flame_explosion(beam, *mons, agent);
        // This might need to change if monsters ever get the ability to cast
        // Inner Flame...
        if (agent && agent->is_player())
            mons_add_blame(mons, "hexed by the player character");
        else if (agent)
            mons_add_blame(mons, "hexed by " + agent->name(DESC_A, true));
        mons->flags    |= MF_EXPLODE_KILL;
        sanct_msg       = "By Zin's power, the fiery explosion is contained.";
        beam.aux_source = "ignited by their inner flame";
        inner_flame = true;
    }

    if (beam.aux_source.empty())
    {
        if (type == MONS_BENNU)
        {
            if (YOU_KILL(killer))
                beam.aux_source = "ignited by themself";
            else if (pet_kill)
                beam.aux_source = "ignited by their pet";
        }
        else
        {
            if (YOU_KILL(killer))
                beam.aux_source = "set off by themself";
            else if (pet_kill)
                beam.aux_source = "set off by their pet";
        }
    }

    if (is_sanctuary(mons->pos()))
    {
        if (you.can_see(*mons))
            mprf(MSGCH_GOD, "%s", sanct_msg.c_str());
        return false;
    }

    if (type == MONS_LURKING_HORROR)
        torment(mons, TORMENT_LURKING_HORROR, mons->pos());

    // Detach monster from the grid first, so it doesn't get hit by
    // its own explosion. (GDL)
    // Unless it's a phoenix, where this isn't much of a concern.
    env.mgrid(mons->pos()) = NON_MONSTER;

    // The explosion might cause a monster to be placed where the bomb
    // used to be, so make sure that env.mgrid() doesn't get cleared a second
    // time (causing the new monster to become floating) when
    // mons->reset() is called.
    if (type == MONS_BALLISTOMYCETE_SPORE)
        mons->set_position(coord_def(0,0));

    // Exploding kills the monster a bit earlier than normal.
    mons->hit_points = -16;

    // FIXME: show_more == you.see_cell(mons->pos())
    if (type == MONS_LURKING_HORROR)
    {
        targeter_radius hitfunc(mons, LOS_SOLID);
        flash_view_delay(UA_MONSTER, DARKGRAY, 300, &hitfunc);
    }
    else
    {
        const auto typ = inner_flame ? EXPLOSION_FINEFF_INNER_FLAME
                                     : EXPLOSION_FINEFF_GENERIC;
        explosion_fineff::schedule(beam, boom_msg, sanct_msg, typ, agent);
    }

    // Monster died in explosion, so don't re-attach it to the grid.
    return true;
}
