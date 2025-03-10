/**
 * @file
 * @brief Monster spell casting.
**/

#include "AppHdr.h"

#include "mon-cast.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <unordered_set>

#include "abyss.h"
#include "act-iter.h"
#include "areas.h"
#include "attack.h"
#include "bloodspatter.h"
#include "branch.h"
#include "cleansing-flame-source-type.h"
#include "cloud.h"
#include "colour.h"
#include "coordit.h"
#include "database.h"
#include "delay.h"
#include "directn.h"
#include "english.h"
#include "env.h"
#include "tile-env.h"
#include "evoke.h"
#include "exclude.h"
#include "fight.h"
#include "fprop.h"
#include "ghost.h"
#include "god-passive.h"
#include "items.h"
#include "item-def.h"
#include "level-state-type.h"
#include "libutil.h"
#include "losglobal.h"
#include "makeitem.h"
#include "mapmark.h"
#include "message.h"
#include "misc.h"
#include "mon-act.h"
#include "mon-ai-action.h"
#include "mon-behv.h"
#include "mon-clone.h"
#include "mon-death.h"
#include "mon-gear.h"
#include "mon-pathfind.h"
#include "mon-pick.h"
#include "mon-place.h"
#include "mon-project.h"
#include "mon-speak.h"
#include "mon-tentacle.h"
#include "mutation.h"
#include "player-stats.h"
#include "random.h"
#include "religion.h"
#include "shout.h"
#include "spl-clouds.h"
#include "spl-damage.h"
#include "spl-goditem.h"
#include "spl-monench.h"
#include "spl-summoning.h"
#include "spl-transloc.h"
#include "spl-util.h"
#include "spl-zap.h" // spell_to_zap
#include "state.h"
#include "stepdown.h"
#include "stringutil.h"
#include "tag-version.h"
#include "target.h"
#include "teleport.h"
#include "terrain.h"
#include "tilepick.h"
#include "rltiles/tiledef-dngn.h"
#include "rltiles/tiledef-player.h"
#include "tileview.h"
#include "timed-effects.h"
#include "traps.h"
#include "travel.h"
#include "unwind.h"
#include "view.h"
#include "viewchar.h"
#include "xom.h"

static bool _valid_mon_spells[NUM_SPELLS];

static const string MIRROR_RECAST_KEY = "mirror_recast_time";

static bool _trace_los(const monster* agent, bool (*vulnerable)(const actor*));
static bool _vortex_vulnerable(const actor* victim);
static god_type _find_god(const monster &mons, mon_spell_slot_flags flags);
static monster* _get_allied_target(const monster &caster, bolt &tracer);
static void _fire_simple_beam(monster &caster, mon_spell_slot, bolt &beam);
static void _fire_direct_explosion(monster &caster, mon_spell_slot, bolt &beam);
static int  _mons_mesmerise(monster* mons, bool actual = true);
static int  _mons_cause_fear(monster* mons, bool actual = true);
static int  _mons_mass_confuse(monster* mons, bool actual = true);
static coord_def _mons_fragment_target(const monster &mons);
static void _maybe_throw_ally(const monster &mons);
static void _siren_sing(monster* mons, bool avatar);
static void _doom_howl(monster &mon);
static void _corrupt_locale(monster &mon);
static ai_action::goodness _monster_spell_goodness(monster* mon, mon_spell_slot slot);
static string _god_name(god_type god);
static coord_def _mons_ghostly_sacrifice_target(const monster &caster,
                                                bolt tracer);
static coord_def _mons_bomblet_target(const monster& caster);
static function<void(bolt&, const monster&, int)>
    _selfench_beam_setup(beam_type flavour);
static function<void(bolt&, const monster&, int)>
    _zap_setup(spell_type spell);
static function<void(bolt&, const monster&, int)>
    _buff_beam_setup(beam_type flavour);
static function<void(bolt&, const monster&, int)>
    _target_beam_setup(function<coord_def(const monster&)> targeter);
static void _setup_minor_healing(bolt &beam, const monster &caster,
                                 int = -1);
static void _setup_heal_other(bolt &beam, const monster &caster, int = -1);
static void _setup_creeping_frost(bolt &beam, const monster &caster, int pow);
static void _setup_creeping_shadow(bolt &beam, const monster &, int pow);
static void _setup_pyroclastic_surge(bolt &beam, const monster &caster, int pow);
static ai_action::goodness _negative_energy_spell_goodness(const actor* foe);
static ai_action::goodness _caster_sees_foe(const monster &caster);
static ai_action::goodness _foe_effect_viable(const monster &caster, duration_type d, enchant_type e);
static ai_action::goodness _foe_polymorph_viable(const monster &caster);
static ai_action::goodness _foe_sleep_viable(const monster &caster);
static ai_action::goodness _foe_tele_goodness(const monster &caster);
static ai_action::goodness _foe_wl_lower_goodness(const monster &caster);
static ai_action::goodness _foe_vitrify_goodness(const monster &caster);
static ai_action::goodness _foe_soul_splinter_goodness(const monster &caster);
static ai_action::goodness _still_winds_goodness(const monster &caster);
static ai_action::goodness _arcjolt_goodness(const monster &caster);
static ai_action::goodness _scorch_goodness(const monster& caster);
static ai_action::goodness _foe_near_wall(const monster &caster);
static ai_action::goodness _foe_not_nearby(const monster &caster);
static ai_action::goodness _foe_near_lava(const monster &caster);
static ai_action::goodness _mons_likes_blinking(const monster &caster);
static void _cast_injury_mirror(monster &mons, mon_spell_slot, bolt&);
static void _cast_smiting(monster &mons, mon_spell_slot slot, bolt&);
static void _cast_brain_bite(monster &mons, mon_spell_slot slot, bolt&);
static void _cast_resonance_strike(monster &mons, mon_spell_slot, bolt&);
static void _cast_call_down_lightning(monster &caster, mon_spell_slot, bolt&);
static void _cast_pyroclastic_surge(monster &caster, mon_spell_slot, bolt&);
static void _flay(const monster &caster, actor &defender, int damage);
static void _cast_still_winds(monster &caster, mon_spell_slot, bolt&);
static void _mons_summon_elemental(monster &caster, mon_spell_slot, bolt&);
static void _mons_summon_dancing_weapons(monster &caster, mon_spell_slot, bolt&);
static void _cast_divine_armament(monster& mons, mon_spell_slot slot, bolt&);
static void _mons_sticks_to_snakes(monster& mons, mon_spell_slot slot, bolt&);
static void _mons_shadow_puppet(monster& mons, mon_spell_slot slot, bolt&);
static void _mons_shadow_turret(monster& mons, mon_spell_slot slot, bolt&);
static bool _los_spell_worthwhile(const monster &caster, spell_type spell);
static void _setup_fake_beam(bolt& beam, const monster&, int = -1);
static void _branch_summon(monster &caster, mon_spell_slot slot, bolt&);
static void _branch_summon_helper(monster* mons, spell_type spell_cast);
static void _cast_marshlight(monster &caster, mon_spell_slot slot, bolt&);
static void _cast_foxfire(monster &caster, mon_spell_slot slot, bolt&);
static bool _prepare_ghostly_sacrifice(monster &caster, bolt &beam);
static void _setup_ghostly_beam(bolt &beam, int power, int dice);
static void _setup_ghostly_sacrifice_beam(bolt& beam, const monster& caster,
                                          int power);
static ai_action::goodness _seracfall_goodness(const monster &caster);
static bool _prepare_seracfall(monster &caster, bolt &beam);
static void _setup_seracfall_beam(bolt& beam, const monster& caster, int power);
static ai_action::goodness _grave_claw_goodness(const monster &caster);
static function<ai_action::goodness(const monster&)> _setup_hex_check(spell_type spell);
static ai_action::goodness _hexing_goodness(const monster &caster, spell_type spell);
static bool _torment_vulnerable(const actor* victim);
static void _cast_grasping_roots(monster &caster, mon_spell_slot, bolt&);
static int _monster_abjuration(const monster& caster, bool actual);
static ai_action::goodness _mons_will_abjure(const monster& mons);
static ai_action::goodness _should_irradiate(const monster& mons);
static void _whack(const actor &caster, actor &victim);
static bool _mons_cast_prisms(monster& caster, actor& foe, int pow, bool check_only);
static bool _mons_cast_hellfire_mortar(monster& caster, actor& foe, int pow, bool check_only);
static ai_action::goodness _hoarfrost_cannonade_goodness(const monster &caster);
static void _cast_wall_burst(monster &caster, bolt &beam, int radius = LOS_RADIUS);
static bool _cast_seismic_stomp(const monster& caster, bolt& beam, bool check_only = false);
static ai_action::goodness _foe_siphon_essence_goodness(const monster &caster);
static vector<actor*> _siphon_essence_victims (const actor& caster);
static void _cast_siphon_essence(monster &caster, mon_spell_slot, bolt&);
static ai_action::goodness _sojourning_bolt_goodness(const monster &caster);
static bool _cast_dominate_undead(const monster& caster, int pow, bool check_only);

enum spell_logic_flag
{
    MSPELL_NO_AUTO_NOISE = 1 << 0, ///< silent, or noise generated specially
};

DEF_BITFIELD(spell_logic_flags, spell_logic_flag);
constexpr spell_logic_flags MSPELL_LOGIC_NONE{};

struct mons_spell_logic
{
    /// Can casting this spell right now accomplish anything useful?
    function<ai_action::goodness(const monster&)> calc_goodness;
    /// Actually cast the given spell.
    function<void(monster&, mon_spell_slot, bolt&)> cast;
    /// Setup a targeting/effect beam for the given spell, if applicable.
    function<void(bolt&, const monster&, int)> setup_beam;
    /// What special behaviors does this spell have for monsters?
    spell_logic_flags flags;
    /// How much 'power' is this spell cast with per caster HD? If 0, ignore
    int power_hd_factor;
};

static ai_action::goodness _always_worthwhile(const monster &/*caster*/)
{
    return ai_action::good(); // or neutral?
}

static function<ai_action::goodness(const monster&)> _should_selfench(enchant_type ench);
static mons_spell_logic _conjuration_logic(spell_type spell);
static mons_spell_logic _hex_logic(spell_type spell,
                                   function<ai_action::goodness(const monster&)> extra_logic
                                   = nullptr,
                                   int power_hd_factor = 0);

/// How do monsters go about casting spells?
static const map<spell_type, mons_spell_logic> spell_to_logic = {
    { SPELL_MIGHT, {
        _should_selfench(ENCH_MIGHT),
        _fire_simple_beam,
        _selfench_beam_setup(BEAM_MIGHT),
    } },
    { SPELL_INVISIBILITY, {
        _should_selfench(ENCH_INVIS),
        _fire_simple_beam,
        _selfench_beam_setup(BEAM_INVISIBILITY),
    } },
    { SPELL_HASTE, {
        _should_selfench(ENCH_HASTE),
        _fire_simple_beam,
        _selfench_beam_setup(BEAM_HASTE),
    } },
    { SPELL_MINOR_HEALING, {
        [](const monster &caster) {
            return ai_action::good_or_bad(caster.hit_points <= caster.max_hit_points / 2);
        },
        _fire_simple_beam,
        _setup_minor_healing,
    } },
    { SPELL_SLUG_DART, _conjuration_logic(SPELL_SLUG_DART) },
    { SPELL_BECKONING, {
        [](const monster &caster)
        {
            const actor* foe = caster.get_foe();
            ASSERT(foe);
            return ai_action::good_or_impossible(
                                !adjacent(caster.pos(), foe->pos())
                                && !foe->is_stationary());
        },
        _fire_simple_beam,
        _zap_setup(SPELL_BECKONING)
    } },
    { SPELL_VAMPIRIC_DRAINING, {
        [](const monster &caster)
        {
            const actor* foe = caster.get_foe();
            ASSERT(foe);
            // always cast at < 1/3rd hp, never at > 2/3rd hp
            const bool low_hp = x_chance_in_y(caster.max_hit_points * 2 / 3
                                                - caster.hit_points,
                                              caster.max_hit_points / 3);
            if (!adjacent(caster.pos(), foe->pos()))
                return ai_action::impossible();

            if (!actor_is_susceptible_to_vampirism(*foe))
                return ai_action::impossible();

            return min(_negative_energy_spell_goodness(foe),
                       ai_action::good_or_bad(low_hp));
        },
        _fire_simple_beam,
        _zap_setup(SPELL_VAMPIRIC_DRAINING),
        MSPELL_NO_AUTO_NOISE,
    } },
    { SPELL_CANTRIP, {
        [](const monster &caster)
        {
            return ai_action::good_or_bad(caster.get_foe());
        },
        nullptr,
        nullptr,
    } },
    { SPELL_INJURY_MIRROR, {
        [](const monster &caster) {
            return ai_action::good_or_impossible(
                    !caster.has_ench(ENCH_MIRROR_DAMAGE)
                    && (!caster.props.exists(MIRROR_RECAST_KEY)
                        || you.elapsed_time >=
                           caster.props[MIRROR_RECAST_KEY].get_int()));
        },
        _cast_injury_mirror,
        nullptr,
        MSPELL_NO_AUTO_NOISE,
    } },
    { SPELL_DRAIN_LIFE, {
        [](const monster &caster) {
            return ai_action::good_or_bad(
                   _los_spell_worthwhile(caster, SPELL_DRAIN_LIFE)
                   && (!caster.friendly()
                       || !you.visible_to(&caster)
                       || player_prot_life(false) >= 3));
        },
        [](monster &caster, mon_spell_slot slot, bolt&) {
            const int splpow = mons_spellpower(caster, slot.spell);

            int damage = 0;
            fire_los_attack_spell(slot.spell, splpow, &caster, false, &damage);
            if (damage > 0 && caster.heal(damage))
                simple_monster_message(caster, " is healed.");
        },
        _zap_setup(SPELL_DRAIN_LIFE),
        MSPELL_NO_AUTO_NOISE,
        1,
    } },
    { SPELL_OZOCUBUS_REFRIGERATION, {
        [](const monster &caster) {
            return ai_action::good_or_bad(
                _los_spell_worthwhile(caster, SPELL_OZOCUBUS_REFRIGERATION)
                   && (!caster.friendly() || !you.visible_to(&caster)));
        },
        [](monster &caster, mon_spell_slot slot, bolt&) {
            const int splpow = mons_spellpower(caster, slot.spell);
            fire_los_attack_spell(slot.spell, splpow, &caster, false);
        },
        _zap_setup(SPELL_OZOCUBUS_REFRIGERATION),
        MSPELL_LOGIC_NONE,
        5,
    } },
    { SPELL_HOARFROST_CANNONADE, {
        _hoarfrost_cannonade_goodness,
        [](monster &caster, mon_spell_slot slot, bolt&) {
            const int pow = mons_spellpower(caster, slot.spell);
            cast_hoarfrost_cannonade(caster, pow, false);
        },
    } },
    { SPELL_HOARFROST_BULLET, {
        _always_worthwhile,
        [](monster &caster, mon_spell_slot slot, bolt& beam) {
            const int shot = caster.props.exists(HOARFROST_SHOTS_KEY)
                                ? caster.props[HOARFROST_SHOTS_KEY].get_int() : 0;
            const int pow = mons_spellpower(caster, slot.spell);
            if (shot == MAX_HOARFROST_SHOTS)
                zappy(ZAP_HOARFROST_BULLET_FINALE, pow, true, beam);
            else
                zappy(ZAP_HOARFROST_BULLET, pow, true, beam);

            _fire_simple_beam(caster, slot, beam);

            // Immediately destroy the cannon on its finale shot, otherwise
            // just injure it.
            if (shot == MAX_HOARFROST_SHOTS)
                monster_die(caster, KILL_TIMEOUT, NON_MONSTER);
            else
            {
                caster.hurt(&caster, caster.max_hit_points / 5, BEAM_MMISSILE,
                            KILLED_BY_BEAM);
                caster.props[HOARFROST_SHOTS_KEY] = (shot + 1);
            }
        },
        _zap_setup(SPELL_HOARFROST_BULLET),
    } },
    { SPELL_TROGS_HAND, {
        [](const monster &caster) {
            return ai_action::good_or_impossible(
                                       !caster.has_ench(ENCH_STRONG_WILLED)
                                    && !caster.has_ench(ENCH_REGENERATION));
        },
        [](monster &caster, mon_spell_slot, bolt&) {
            const string god = apostrophise(god_name(caster.god));
            const string msg = make_stringf(" invokes %s protection!",
                                            god.c_str());
            simple_monster_message(caster, msg.c_str(), false, MSGCH_MONSTER_SPELL);
            // Not spell_hd(spell_cast); this is an invocation
            const int dur = BASELINE_DELAY
                * min(4 + roll_dice(2, (caster.get_hit_dice() * 4) / 3 + 1),
                      100);
            caster.add_ench(mon_enchant(ENCH_STRONG_WILLED, 0, &caster, dur));
            caster.add_ench(mon_enchant(ENCH_REGENERATION, 0, &caster, dur));
            dprf("Trog's Hand cast (dur: %d aut)", dur);
        },
        nullptr,
        MSPELL_NO_AUTO_NOISE,
    } },
    { SPELL_LEDAS_LIQUEFACTION, {
        [](const monster &caster) {
            return ai_action::good_or_impossible(!caster.has_ench(ENCH_LIQUEFYING));
        },
        [](monster &caster, mon_spell_slot, bolt&) {
            if (you.can_see(caster))
            {
                mprf("%s liquefies the ground around %s!",
                     caster.name(DESC_THE).c_str(),
                     caster.pronoun(PRONOUN_REFLEXIVE).c_str());
                flash_view_delay(UA_MONSTER, BROWN, 80);
            }

            caster.add_ench(ENCH_LIQUEFYING);
            invalidate_agrid(true);
        },
        nullptr,
        MSPELL_NO_AUTO_NOISE,
    } },
    { SPELL_FORCEFUL_INVITATION, {
        _always_worthwhile,
        _branch_summon,
        nullptr,
        MSPELL_NO_AUTO_NOISE,
    } },
    { SPELL_PLANEREND, {
        _always_worthwhile,
        _branch_summon,
        nullptr,
        MSPELL_NO_AUTO_NOISE,
    } },
    { SPELL_MARSHLIGHT, {
       _always_worthwhile,
       _cast_marshlight,
    } },
    { SPELL_FOXFIRE, {
       _always_worthwhile,
       _cast_foxfire,
    } },
    { SPELL_SEARING_RAY, {
       _always_worthwhile,
       [](monster &caster, mon_spell_slot, bolt& beam) {
            const int pow = mons_spellpower(caster, SPELL_SEARING_RAY);
            cast_searing_ray(caster, pow, beam, false);
        },
        _zap_setup(SPELL_SEARING_RAY)
    } },
    { SPELL_STILL_WINDS, { _still_winds_goodness, _cast_still_winds } },
    { SPELL_ARCJOLT, { _arcjolt_goodness,
        [](monster &caster, mon_spell_slot, bolt&) {
            const int pow = mons_spellpower(caster, SPELL_ARCJOLT);
            cast_arcjolt(pow, caster, false);
        },
    } },
    { SPELL_SCORCH, { _scorch_goodness,
        [](monster &caster, mon_spell_slot, bolt&) {
            const int pow = mons_spellpower(caster, SPELL_SCORCH);
            cast_scorch(caster, pow, false);
        },
    } },
    { SPELL_KISS_OF_DEATH, { _always_worthwhile,
        [](monster &caster, mon_spell_slot slot, bolt& beam) {
            _fire_simple_beam(caster, slot, beam);
            caster.add_ench(mon_enchant(ENCH_DRAINED, 3, &caster, random_range(100, 200)));
        },
        _zap_setup(SPELL_KISS_OF_DEATH)
    } },
    { SPELL_SMITING, { _always_worthwhile, _cast_smiting, } },
    { SPELL_BRAIN_BITE, { _always_worthwhile, _cast_brain_bite, } },
    { SPELL_CALL_DOWN_LIGHTNING, { _foe_not_nearby, _cast_call_down_lightning, _zap_setup(SPELL_CALL_DOWN_LIGHTNING) } },
    { SPELL_RESONANCE_STRIKE, { _always_worthwhile, _cast_resonance_strike, } },
    { SPELL_CREEPING_FROST, { _foe_near_wall,
        [](monster &caster, mon_spell_slot, bolt& beam) {
            _cast_wall_burst(caster, beam);
        },
        _setup_creeping_frost } },
    { SPELL_PYROCLASTIC_SURGE, { _foe_near_lava, _cast_pyroclastic_surge, _setup_pyroclastic_surge } },
    { SPELL_FLAY, {
        [](const monster &caster) {
            const actor* foe = caster.get_foe(); // XXX: check vis?
            ASSERT(foe);
            return ai_action::good_or_impossible(!!(foe->holiness() & MH_NATURAL));
        },
        mons_cast_flay,
    } },
    { SPELL_PARALYSIS_GAZE, {
        _caster_sees_foe,
        [](monster &caster, mon_spell_slot, bolt&) {
            caster.get_foe()->paralyse(&caster, 2 + random2(3));
        },
    } },
    { SPELL_DRAINING_GAZE, {
        _caster_sees_foe,
        [](monster &caster, mon_spell_slot slot, bolt&) {
            flash_tile(caster.get_foe()->pos(), MAGENTA, 120, TILE_BOLT_DRAINING_GAZE);
            enchant_actor_with_flavour(caster.get_foe(), &caster,
                                       BEAM_DRAIN_MAGIC,
                                       mons_spellpower(caster, slot.spell));
        },
    } },
    { SPELL_WEAKENING_GAZE, {
        _caster_sees_foe,
        [](monster &caster, mon_spell_slot, bolt&) {
            caster.get_foe()->weaken(&caster, caster.get_hit_dice());
        },
    } },
    { SPELL_VITRIFYING_GAZE, {
        _caster_sees_foe,
        [](monster &caster, mon_spell_slot slot, bolt&) {
            enchant_actor_with_flavour(caster.get_foe(), &caster,
                                       BEAM_VITRIFYING_GAZE,
                                       mons_spellpower(caster, slot.spell));
        },
    } },
    { SPELL_WATER_ELEMENTALS, { _always_worthwhile, _mons_summon_elemental } },
    { SPELL_EARTH_ELEMENTALS, { _always_worthwhile, _mons_summon_elemental } },
    { SPELL_AIR_ELEMENTALS, { _always_worthwhile, _mons_summon_elemental } },
    { SPELL_FIRE_ELEMENTALS, { _always_worthwhile, _mons_summon_elemental } },
    { SPELL_STICKS_TO_SNAKES, { _always_worthwhile, _mons_sticks_to_snakes } },
    { SPELL_SHEZAS_DANCE, { _always_worthwhile, _mons_summon_dancing_weapons } },
    { SPELL_FLASHING_BALESTRA, { _foe_not_nearby, _fire_simple_beam,
                                 _zap_setup(SPELL_FLASHING_BALESTRA)
    } },
    { SPELL_PHANTOM_BLITZ, { _always_worthwhile, _fire_simple_beam,
                             _zap_setup(SPELL_PHANTOM_BLITZ)
    } },
    { SPELL_DIVINE_ARMAMENT, { _always_worthwhile, _cast_divine_armament } },
    { SPELL_HASTE_OTHER, {
        _always_worthwhile,
        _fire_simple_beam,
        _buff_beam_setup(BEAM_HASTE)
    } },
    { SPELL_MIGHT_OTHER, {
        _always_worthwhile,
        _fire_simple_beam,
        _buff_beam_setup(BEAM_MIGHT)
    } },
    { SPELL_INVISIBILITY_OTHER, {
        _always_worthwhile,
        _fire_simple_beam,
        _buff_beam_setup(BEAM_INVISIBILITY)
    } },
    { SPELL_HEAL_OTHER, {
        _always_worthwhile,
        _fire_simple_beam,
        _setup_heal_other,
    } },
    { SPELL_LRD, {
        _always_worthwhile,
        [](monster &caster, mon_spell_slot slot, bolt& pbolt) {
            const int splpow = mons_spellpower(caster, slot.spell);
            cast_fragmentation(splpow, &caster, pbolt.target, false);
        },
        _target_beam_setup(_mons_fragment_target),
        MSPELL_LOGIC_NONE, 6
    } },
    { SPELL_GHOSTLY_SACRIFICE, {
        _always_worthwhile,
        [](monster &caster, mon_spell_slot slot, bolt& pbolt) {
            if (_prepare_ghostly_sacrifice(caster, pbolt))
                _fire_direct_explosion(caster, slot, pbolt);
            else if (you.can_see(caster))
                canned_msg(MSG_NOTHING_HAPPENS);
        },
        _setup_ghostly_sacrifice_beam,
    } },
    { SPELL_SLOW, _hex_logic(SPELL_SLOW) },
    { SPELL_CONFUSE, _hex_logic(SPELL_CONFUSE) },
    { SPELL_BANISHMENT, _hex_logic(SPELL_BANISHMENT) },
    { SPELL_PARALYSE, _hex_logic(SPELL_PARALYSE) },
    { SPELL_VEX, _hex_logic(SPELL_VEX, [](const monster& caster) {
                return _foe_effect_viable(caster, DUR_VEXED, ENCH_VEXED);
    }, 5) },
    { SPELL_VITRIFY, _hex_logic(SPELL_VITRIFY, _foe_vitrify_goodness) },
    { SPELL_PETRIFY, _hex_logic(SPELL_PETRIFY) },
    { SPELL_PAIN, _hex_logic(SPELL_PAIN, [](const monster& caster) {
            const actor* foe = caster.get_foe();
            ASSERT(foe);
            // why does this check rTorment, not rN?
            return ai_action::good_or_impossible(_torment_vulnerable(foe));
    }) },
    { SPELL_MINDBURST, _hex_logic(SPELL_MINDBURST) },
    { SPELL_CORONA, _hex_logic(SPELL_CORONA, [](const monster& caster) {
            const actor* foe = caster.get_foe();
            ASSERT(foe);
            return ai_action::good_or_impossible(!foe->backlit());
    }) },
    { SPELL_POLYMORPH, _hex_logic(SPELL_POLYMORPH, _foe_polymorph_viable) },
    { SPELL_SLEEP, _hex_logic(SPELL_SLEEP, _foe_sleep_viable, 6) },
    { SPELL_HIBERNATION, _hex_logic(SPELL_HIBERNATION, _foe_sleep_viable) },
    { SPELL_TELEPORT_OTHER, _hex_logic(SPELL_TELEPORT_OTHER,
                                         _foe_tele_goodness) },
    { SPELL_DIMENSION_ANCHOR, _hex_logic(SPELL_DIMENSION_ANCHOR, nullptr, 6)},
    { SPELL_AGONY, _hex_logic(SPELL_AGONY, [](const monster &caster) {
            const actor* foe = caster.get_foe();
            ASSERT(foe);
            return ai_action::good_or_impossible(_torment_vulnerable(foe));
        }, 6)
    },
    { SPELL_AGONISING_TOUCH, _hex_logic(SPELL_AGONISING_TOUCH, [](const monster &caster) {
            const actor* foe = caster.get_foe();
            ASSERT(foe);
            return ai_action::good_or_impossible(_torment_vulnerable(foe));
        }, 6)
    },
    { SPELL_STRIP_WILLPOWER,
        _hex_logic(SPELL_STRIP_WILLPOWER, _foe_wl_lower_goodness, 6)
    },
    { SPELL_SENTINEL_MARK, _hex_logic(SPELL_SENTINEL_MARK, nullptr, 16) },
    { SPELL_SAP_MAGIC, {
        _always_worthwhile, _fire_simple_beam, _zap_setup(SPELL_SAP_MAGIC),
        MSPELL_LOGIC_NONE, 10,
    } },
    { SPELL_DRAIN_MAGIC, _hex_logic(SPELL_DRAIN_MAGIC, nullptr, 6) },
    { SPELL_VIRULENCE, _hex_logic(SPELL_VIRULENCE, [](const monster &caster) {
            const actor* foe = caster.get_foe();
            ASSERT(foe);
            return ai_action::good_or_impossible(foe->res_poison(false) < 3);
    }, 8) },
    { SPELL_GRASPING_ROOTS, {
        [](const monster &caster)
        {
            const actor* foe = caster.get_foe();
            ASSERT(foe);
            return ai_action::good_or_impossible(caster.can_constrict(*foe,
                                                                      CONSTRICT_ROOTS));
        }, _cast_grasping_roots, } },
    { SPELL_ABJURATION, {
        _mons_will_abjure,
        [] (const monster &caster, mon_spell_slot /*slot*/, bolt& /*beem*/) {
            _monster_abjuration(caster, true);
        }, nullptr, MSPELL_LOGIC_NONE, 20, } },
    { SPELL_CONCENTRATE_VENOM, {
        _always_worthwhile,
        _fire_simple_beam,
        _buff_beam_setup(BEAM_CONCENTRATE_VENOM)
    } },
    { SPELL_PLASMA_BEAM, {
        [](const monster &caster) {
            const int pow = mons_spellpower(caster, SPELL_PLASMA_BEAM);
            return ai_action::good_or_bad(mons_should_fire_plasma(pow, caster));
        },
        [](monster &caster, mon_spell_slot, bolt&) {
            const int pow = mons_spellpower(caster, SPELL_PLASMA_BEAM);
            cast_plasma_beam(pow, caster, false);
        }
    } },
    { SPELL_PERMAFROST_ERUPTION, {
        [](const monster &caster) {
            const int pow = mons_spellpower(caster, SPELL_PLASMA_BEAM);
            return ai_action::good_or_bad(mons_should_fire_permafrost(pow, caster));
        },
        [](monster &caster, mon_spell_slot, bolt&) {
            const int pow = mons_spellpower(caster, SPELL_PERMAFROST_ERUPTION);
            cast_permafrost_eruption(caster, pow, false);
        }
    } },
    { SPELL_BLINK, {
        _mons_likes_blinking,
        [] (monster &caster, mon_spell_slot /*slot*/, bolt& /*beem*/) {
            monster_blink(&caster);
        }
    }},
    { SPELL_BLINK_RANGE, {
        _mons_likes_blinking,
        [] (monster &caster, mon_spell_slot /*slot*/, bolt& /*beem*/) {
            blink_range(caster);
        }
    } },
    { SPELL_BLINK_AWAY, {
        _mons_likes_blinking,
        [] (monster &caster, mon_spell_slot /*slot*/, bolt& /*beem*/) {
            blink_away(&caster, true);
        }
    } },
    { SPELL_BLINK_CLOSE, {
        [](const monster &caster) {
            const actor* foe = caster.get_foe();
            ASSERT(foe);
            if (adjacent(caster.pos(), foe->pos()))
                return ai_action::bad();
            return _mons_likes_blinking(caster);
        },
        [] (monster &caster, mon_spell_slot /*slot*/, bolt& /*beem*/) {
            blink_close(caster);
        }
    } },
    { SPELL_ELECTROLUNGE, {
        [](const monster &caster) {
            const actor* foe = caster.get_foe();
            ASSERT(foe);
            const coord_def dest = get_electric_charge_landing_spot(caster, foe->pos());
            if (dest.origin())
                return ai_action::impossible();

            // Prevent friendlies from forcibly displacing the player, which can
            // be dangerous.
            if (caster.friendly() && you.pos() == dest)
                return ai_action::bad();

            return ai_action::good();
        },
        [] (monster &caster, mon_spell_slot /*slot*/, bolt& /*beem*/) {
            const int pow = mons_spellpower(caster, SPELL_ELECTROLUNGE);
            electric_charge(caster, pow, false, caster.get_foe()->pos());
        }
    } },
    { SPELL_CORRUPT_LOCALE, {
        [](const monster & /* caster */) {
            return ai_action::good_or_impossible(!player_in_branch(BRANCH_ABYSS));
        },
        [](monster &caster, mon_spell_slot, bolt&) {
            _corrupt_locale(caster);
        },
    }, },
    { SPELL_SERACFALL, {
        _seracfall_goodness,
        [](monster &caster, mon_spell_slot slot, bolt& pbolt) {
            if (_prepare_seracfall(caster, pbolt))
                _fire_simple_beam(caster, slot, pbolt);
            else if (you.can_see(caster))
                canned_msg(MSG_NOTHING_HAPPENS);
        },
        _setup_seracfall_beam,
        MSPELL_LOGIC_NONE,
        18, // 1.5x iceblast
    } },
    { SPELL_DAZZLING_FLASH, {
        [](const monster &caster)
        {
            // Run tracer version of spell. Power matters for range.
            const int pow = mons_spellpower(caster, SPELL_DAZZLING_FLASH);
            return ai_action::good_or_bad(
                cast_dazzling_flash(&caster, pow, false, true) == spret::success
            );
        },
        [](monster &caster, mon_spell_slot /*slot*/, bolt& /*pbolt*/) {
            const int pow = mons_spellpower(caster, SPELL_DAZZLING_FLASH);
            cast_dazzling_flash(&caster, pow, false);
        },
    } },
    { SPELL_SHADOW_BALL, _conjuration_logic(SPELL_SHADOW_BALL) },
    { SPELL_SHADOW_BEAM, _conjuration_logic(SPELL_SHADOW_BEAM) },
    { SPELL_SHADOW_SHARD, _conjuration_logic(SPELL_SHADOW_SHARD) },
    { SPELL_SHADOW_SHOT, _conjuration_logic(SPELL_SHADOW_SHOT) },
    { SPELL_SHADOW_PUPPET, { _always_worthwhile, _mons_shadow_puppet } },
    { SPELL_SHADOW_TURRET, { _always_worthwhile, _mons_shadow_turret } },
    { SPELL_CREEPING_SHADOW, { _foe_near_wall,
        [](monster &caster, mon_spell_slot, bolt& beam) {
            _cast_wall_burst(caster, beam, 5);
        },
        _setup_creeping_shadow } },
    { SPELL_SHADOW_PRISM, { _always_worthwhile,
       [](monster &caster, mon_spell_slot, bolt& beam){
            const int pow = mons_spellpower(caster, SPELL_SHADOW_PRISM);
            spret ret = cast_fulminating_prism(&caster, pow, beam.target, false, true);
            if (ret == spret::abort)
                mprf(MSGCH_WARN, "Failed to place prism at (%d, %d)", beam.target.x, beam.target.y);
        }
    } },
    { SPELL_SHADOW_TEMPEST, { _always_worthwhile,
       [](monster &caster, mon_spell_slot, bolt& beam) {
            vector<monster*> targs;
            for (monster_near_iterator mi(&caster, LOS_NO_TRANS); mi; ++mi)
                if (!mi->is_peripheral() && !mons_aligned(&caster, *mi))
                    targs.push_back(*mi);

            const unsigned int num_targs = (targs.size() * random_range(30, 50) / 100)
                                           + 1;
            shuffle_array(targs);
            const int draw_delay = 150 / num_targs;
            for (size_t i = 0; i < targs.size() && i < num_targs; ++i)
            {
                if (you.see_cell(targs[i]->pos()))
                    flash_tile(targs[i]->pos(), MAGENTA, draw_delay, TILE_BOLT_SHADOW_BLAST);
            }
            for (size_t i = 0; i < targs.size() && i < num_targs; ++i)
            {
                if (!targs[i]->alive())
                    continue;

                beam.source = targs[i]->pos();
                beam.target = targs[i]->pos();
                beam.fire();
            }
        },
        _zap_setup(SPELL_SHADOW_TEMPEST) } },
    { SPELL_SHADOW_BIND, { _always_worthwhile,
       [](monster &caster, mon_spell_slot, bolt&) {
            vector<monster*> targs;
            for (monster_near_iterator mi(&caster, LOS_NO_TRANS); mi; ++mi)
            {
                if (!mi->is_stationary() && !mi->is_firewood()
                    && !mons_aligned(&caster, *mi) && !mi->has_ench(ENCH_BOUND))
                {
                    targs.push_back(*mi);
                }
            }
            const int pow = mons_spellpower(caster, SPELL_SHADOW_BIND);
            const unsigned int num_targs = div_rand_round(pow, 30);
            shuffle_array(targs);
            for (size_t i = 0; i < targs.size() && i < num_targs; ++i)
            {
                targs[i]->add_ench(mon_enchant(ENCH_BOUND, 1, &caster,
                                                random_range(3, 5) * BASELINE_DELAY));
                if (you.can_see(*targs[i]))
                {
                    mprf("%s is pinned to %s own shadow.",
                         targs[i]->name(DESC_THE).c_str(),
                         targs[i]->pronoun(PRONOUN_POSSESSIVE).c_str());
                }

            }
        } } },
    { SPELL_SHADOW_DRAINING, { _always_worthwhile,
       [](monster &caster, mon_spell_slot, bolt&) {
            if (you.can_see(caster))
            {
                targeter_radius hitfunc(&caster, LOS_SOLID, 2);
                flash_view_delay(UA_MONSTER, DARKGREY, 200, &hitfunc);
                mprf("%s draws nearby shadows into %s.",
                    caster.name(DESC_THE).c_str(),
                    caster.pronoun(PRONOUN_REFLEXIVE).c_str());
            }

            dice_def dmg = zap_damage(ZAP_SHADOW_DRAIN,
                                      mons_spellpower(caster, SPELL_SHADOW_DRAINING),
                                      true);
            for (radius_iterator ri(caster.pos(), 2, C_SQUARE); ri; ++ri)
            {
                monster* m = monster_at(*ri);
                if (m && cell_see_cell(caster.pos(), *ri, LOS_NO_TRANS)
                    && !mons_aligned(&caster, m))
                {
                    m->hurt(&caster, dmg.roll());
                }
            }
        } } },
    { SPELL_GRAVE_CLAW, {
       _grave_claw_goodness,
       [](monster &caster, mon_spell_slot, bolt& beam) {
            const int pow = mons_spellpower(caster, SPELL_GRAVE_CLAW);
            cast_grave_claw(caster, beam.target, pow, false);
        },
    } },
    { SPELL_SHRED, {
       _always_worthwhile,
       [](monster &caster, mon_spell_slot, bolt&) {
            const int pow = mons_spellpower(caster, SPELL_SHRED);
            dice_def dmg = zap_damage(ZAP_SHRED, pow, true);
            for (adjacent_iterator ai(caster.pos()); ai; ++ai)
            {
                if (!monster_at(*ai) || monster_at(*ai)->wont_attack())
                    continue;

                // Don't shred things out of the player's sight
                if (!monster_los_is_valid(&caster, *ai))
                    continue;

                monster* victim = monster_at(*ai);
                int final = victim->apply_ac(dmg.roll());

                if (you.can_see(caster))
                {
                    mprf("%s shreds %s%s", caster.name(DESC_THE).c_str(),
                        victim->name(DESC_THE).c_str(),
                        final ? attack_strength_punctuation(final).c_str()
                                : ", but does no damage.");
                }

                if (final)
                {
                    bleed_onto_floor(victim->pos(), victim->type, final, true);
                    victim->hurt(&caster, final);
                }
            }
        },
    } },
    { SPELL_DEPLOY_BOMBLET, {
        [](const monster &caster)
        {
            if (!_mons_bomblet_target(caster).origin())
                return ai_action::good();
            else
                return ai_action::impossible();
        },
        [](monster &caster, mon_spell_slot, bolt& beam) {
            const coord_def targ = _mons_bomblet_target(caster);
            beam.source = targ;
            beam.target = targ;
            beam.draw_delay = 80;
            beam.fire();
            flash_tile(targ, WHITE, 110, TILE_BOLT_BOMBLET_LAUNCH);
            monarch_deploy_bomblet(caster, targ, true);
        },
        _zap_setup(SPELL_DEPLOY_BOMBLET) } },
    { SPELL_SEISMIC_STOMP, {
        [](const monster &caster)
        {
            bolt dummy;
            return ai_action::good_or_impossible(
                _cast_seismic_stomp(caster, dummy, true));
        },
       [](monster &caster, mon_spell_slot, bolt& beam) {
            _cast_seismic_stomp(caster, beam);
        },
        _zap_setup(SPELL_SEISMIC_STOMP) } },
    { SPELL_SIPHON_ESSENCE, { _foe_siphon_essence_goodness,
                              _cast_siphon_essence } },
    { SPELL_SOUL_SPLINTER, _hex_logic(SPELL_SOUL_SPLINTER, _foe_soul_splinter_goodness) },
    { SPELL_DOMINATE_UNDEAD, {
        [](const monster &caster) {
            return ai_action::good_or_impossible(_cast_dominate_undead(caster, 100, true));
        },
        [](monster &caster, mon_spell_slot, bolt&) {
            const int pow = mons_spellpower(caster, SPELL_DOMINATE_UNDEAD);
            _cast_dominate_undead(caster, pow, false);
        },
        nullptr, MSPELL_LOGIC_NONE, 30
    } }
};

// Logic for special-cased Aphotic Marionette hijacking of monster buffs to
// apply to the player instead.
static const map<spell_type, mons_spell_logic> marionette_spell_to_logic {
    { SPELL_HASTE, {
        [](const monster&) {
            return ai_action::good_or_impossible(!you.duration[DUR_HASTE] && !you.stasis());
        },
        [] (monster&, mon_spell_slot /*slot*/, bolt& /*beem*/) {
            haste_player(random_range(15, 25));
        }
    } },
    { SPELL_MIGHT, {
        [](const monster&) {
            return ai_action::good_or_impossible(!you.duration[DUR_MIGHT]);
        },
        [] (monster&, mon_spell_slot /*slot*/, bolt& /*beem*/) {
            mprf(MSGCH_DURATION, "You feel very mighty all of a sudden.");
            you.increase_duration(DUR_MIGHT, random_range(15, 25));
        }
    } },
    { SPELL_INVISIBILITY, {
        [](const monster&) {
            return ai_action::good_or_impossible(!(you.duration[DUR_INVIS] || you.backlit()));
        },
        [] (monster&, mon_spell_slot /*slot*/, bolt& /*beem*/) {
            mprf(MSGCH_DURATION, !you.duration[DUR_INVIS]
                                    ? "You fade into invisibility!"
                                    : "You fade further into invisibility.");
            you.increase_duration(DUR_INVIS, random_range(10, 15), 100);
        }
    } },
    { SPELL_BERSERKER_RAGE, {
        [](const monster&) {
            return ai_action::good_or_impossible(you.can_go_berserk(true, false, true));
        },
        [] (monster&, mon_spell_slot /*slot*/, bolt& /*beem*/) {
            you.go_berserk(true);
        }
    } },
    { SPELL_BATTLESPHERE, {
        [](const monster&) {
            return ai_action::good_or_impossible(!find_battlesphere(&you));
        },
        [] (monster& caster, mon_spell_slot /*slot*/, bolt& /*beem*/) {
            cast_battlesphere(&you, mons_spellpower(caster, SPELL_BATTLESPHERE), false);
        }
    } },
    { SPELL_MALIGN_GATEWAY, {
        [](const monster&) {
            return ai_action::good_or_impossible(can_cast_malign_gateway());
        },
        [] (monster&, mon_spell_slot /*slot*/, bolt& /*beem*/) {
            cast_malign_gateway(&you, 200);
        }
    } },
    { SPELL_OLGREBS_TOXIC_RADIANCE, {
        [](const monster&) {
            return ai_action::good_or_impossible(cast_toxic_radiance(&you, 100, false, true) == spret::success);
        },
        [] (monster&, mon_spell_slot /*slot*/, bolt& /*beem*/) {
            cast_toxic_radiance(&you, 100, false, false);
        }
    } },
    { SPELL_POLAR_VORTEX, {
        [](const monster&) {
            return ai_action::good_or_impossible(!you.duration[DUR_VORTEX]);
        },
        [] (monster& caster, mon_spell_slot /*slot*/, bolt& /*beem*/) {
            cast_polar_vortex(mons_spellpower(caster, SPELL_POLAR_VORTEX), false, true);
        }
    } },
};

static const mons_spell_logic* _get_spell_logic(const monster& caster, spell_type spell)
{
    // Use marionette overrides when appropriate
    if (caster.attitude == ATT_MARIONETTE)
    {
        const mons_spell_logic* logic = map_find(marionette_spell_to_logic, spell);
        if (logic)
            return logic;
    }

    return map_find(spell_to_logic, spell);
}

/// Create the appropriate casting logic for a simple conjuration.
static mons_spell_logic _conjuration_logic(spell_type spell)
{
    return { _always_worthwhile, _fire_simple_beam, _zap_setup(spell), };
}

/**
 * Create the appropriate casting logic for a simple mr-checking hex.
 *
 * @param spell             The hex in question; e.g. SPELL_CORONA.
 * @param extra_logic       An additional pre-casting condition, beyond the
 *                          normal hex logic.
 * @param power_hd_factor   If nonzero, how much spellpower the spell has per
 *                          caster HD.
 */
static mons_spell_logic _hex_logic(spell_type spell,
                                   function<ai_action::goodness(const monster&)> extra_logic,
                                   int power_hd_factor)
{
    function<ai_action::goodness(const monster&)> calc_goodness = nullptr;
    if (!extra_logic)
        calc_goodness = _setup_hex_check(spell);
    else
    {
        calc_goodness = [spell, extra_logic](const monster& caster) {
            return min(_hexing_goodness(caster, spell), extra_logic(caster));
        };
    }
    return { calc_goodness, _fire_simple_beam, _zap_setup(spell),
             MSPELL_LOGIC_NONE, power_hd_factor * ENCH_POW_FACTOR };
}

/**
 * Take the given beam and fire it, handling screen-refresh issues in the
 * process.
 *
 * @param caster    The monster casting the spell that produced the beam.
 * @param pbolt     A pre-setup & aimed spell beam. (For e.g. FIREBALL.)
 */
static void _fire_simple_beam(monster &/*caster*/, mon_spell_slot, bolt &pbolt)
{
    // If a monster just came into view and immediately cast a spell,
    // we need to refresh the screen before drawing the beam.
    viewwindow();
    update_screen();
    pbolt.fire();
}

/**
 * Take the given explosion and fire it, handling screen-refresh issues in the
 * process.
 *
 * @param caster    The monster casting the spell that produced the beam.
 * @param pbolt     A pre-setup & aimed spell beam. (For e.g. FIRE_STORM.)
 */
static void _fire_direct_explosion(monster &caster, mon_spell_slot, bolt &pbolt)
{
    // If a monster just came into view and immediately cast a spell,
    // we need to refresh the screen before drawing the beam.
    viewwindow();
    update_screen();
    const actor* foe = caster.get_foe();
    const bool need_more = foe && (foe->is_player()
                                   || you.see_cell(foe->pos()));
    pbolt.in_explosion_phase = false;
    pbolt.refine_for_explosion();
    pbolt.explode(need_more);
}

static ai_action::goodness _caster_sees_foe(const monster &caster)
{
    const actor* foe = caster.get_foe();
    ASSERT(foe);
    return ai_action::good_or_impossible(caster.can_see(*foe));
}

static ai_action::goodness _foe_polymorph_viable(const monster &caster)
{
    const actor* foe = caster.get_foe();
    ASSERT(foe);
    if (foe->is_player())
        return ai_action::good_or_impossible(!you.transform_uncancellable);
    else // too dangerous to let allies use
        return ai_action::good_or_bad(!caster.friendly());
}

static ai_action::goodness _foe_sleep_viable(const monster &caster)
{
    const actor* foe = caster.get_foe();
    ASSERT(foe);
    return ai_action::good_or_impossible(foe->can_sleep());
}

/// use when a dur/ench-type effect can only be cast once
static ai_action::goodness _foe_effect_viable(const monster &caster, duration_type d, enchant_type e)
{
    const actor* foe = caster.get_foe();
    ASSERT(foe);
    if (foe->is_player())
        return ai_action::good_or_impossible(!you.duration[d]);
    else
        return ai_action::good_or_impossible(!foe->as_monster()->has_ench(e));
}

static ai_action::goodness _foe_tele_goodness(const monster &caster)
{
    return _foe_effect_viable(caster, DUR_TELEPORT, ENCH_TP);
}

static ai_action::goodness _foe_wl_lower_goodness(const monster &caster)
{
    return _foe_effect_viable(caster, DUR_LOWERED_WL, ENCH_LOWERED_WL);
}

static ai_action::goodness _foe_vitrify_goodness(const monster &caster)
{
    return _foe_effect_viable(caster, DUR_VITRIFIED, ENCH_VITRIFIED);
}

static ai_action::goodness _foe_soul_splinter_goodness(const monster &caster)
{
    const actor* foe = caster.get_foe();
    ASSERT(foe);
    return ai_action::good_or_impossible(!!(foe->holiness() & (MH_NATURAL | MH_DEMONIC | MH_HOLY)));
}

static ai_action::goodness _foe_siphon_essence_goodness(const monster &caster)
{
    vector<actor*> victims = _siphon_essence_victims(caster);
    return ai_action::good_or_impossible(!victims.empty());
}

static ai_action::goodness _sojourning_bolt_goodness(const monster &caster)
{
    const actor* foe = caster.get_foe();
    ASSERT(foe);
    return ai_action::good_or_impossible(!(foe->is_player()
                                           && you.props.exists(SJ_TELEPORTITIS_SOURCE)));
}

/**
 * Build a function to set up a beam to buff the caster.
 *
 * @param flavour   The flavour to buff a caster with.
 * @return          A function that sets up a beam to buff its caster with
 *                  the given flavour.
 */
static function<void(bolt&, const monster&, int)>
    _selfench_beam_setup(beam_type flavour)
{
    return [flavour](bolt &beam, const monster &caster, int)
    {
        beam.flavour = flavour;
        beam.target = caster.pos();
    };
}

/**
 * Build a function that tests whether it's worth casting a buff spell that
 * applies the given enchantment to the caster.
 */
static function<ai_action::goodness(const monster&)> _should_selfench(enchant_type ench)
{
    return [ench](const monster &caster)
    {
        // keep non-summoned pals with haste from spamming constantly
        if (caster.friendly() && !caster.get_foe() && !caster.is_summoned())
            return ai_action::bad();
        return ai_action::good_or_impossible(!caster.has_ench(ench));
    };
}

/**
 * Build a function that sets up and targets a buffing beam at one of the
 * caster's allies. If the function fails to find an ally, the beam will be
 * targeted at an out-of-bounds tile to signal failure.
 *
 * @param flavour   The flavour to buff an ally with.
 * @return          A function that sets up a single-target buff beam.
 */
static function<void(bolt&, const monster&, int)>
    _buff_beam_setup(beam_type flavour)
{
    return [flavour](bolt &beam, const monster &caster, int)
    {
        beam.flavour = flavour;
        const monster* target = _get_allied_target(caster, beam);
        beam.target = target ? target->pos() : coord_def(GXM+1, GYM+1);
    };
}

/**
 * Build a function to set up a beam with spl-to-zap.
 *
 * @param spell     The spell for which beams will be set up.
 * @return          A function that sets up a beam to zap the given spell.
 */
static function<void(bolt&, const monster&, int)> _zap_setup(spell_type spell)
{
    return [spell](bolt &beam, const monster &, int power)
    {
        zappy(spell_to_zap(spell), power, true, beam);
    };
}

static void _setup_healing_beam(bolt &beam, const monster &caster)
{
    beam.damage   = dice_def(2, caster.spell_hd(SPELL_MINOR_HEALING) / 2);
    beam.flavour  = BEAM_HEALING;
}

static void _setup_minor_healing(bolt &beam, const monster &caster, int)
{
    _setup_healing_beam(beam, caster);
    beam.target = caster.pos();
}

static void _setup_heal_other(bolt &beam, const monster &caster, int)
{
    _setup_healing_beam(beam, caster);
    const monster* target = _get_allied_target(caster, beam);
    beam.target = target ? target->pos() : coord_def(GXM+1, GYM+1);
}

/**
 * Build a function that sets up a fake beam for targeting special spells.
 *
 * @param targeter     A function that finds a target for the given spell.
 *                      Expected to return an out-of-bounds coord on failure.
 * @return              A function that initializes a fake targeting beam.
 */
static function<void(bolt&, const monster&, int)>
    _target_beam_setup(function<coord_def(const monster&)> targeter)
{
    return [targeter](bolt& beam, const monster& caster, int)
    {
        _setup_fake_beam(beam, caster);
        beam.target = targeter(caster);
        beam.aimed_at_spot = true;  // to get noise to work properly
    };
}

static void _cast_injury_mirror(monster &mons, mon_spell_slot /*slot*/, bolt&)
{
    const string msg
        = make_stringf(" offers %s to %s, and fills with unholy energy.",
                       mons.pronoun(PRONOUN_REFLEXIVE).c_str(),
                       god_name(mons.god).c_str());
    simple_monster_message(mons, msg.c_str(), false, MSGCH_MONSTER_SPELL);
    mons.add_ench(mon_enchant(ENCH_MIRROR_DAMAGE, 0, &mons,
                              random_range(7, 9) * BASELINE_DELAY));
    mons.props[MIRROR_RECAST_KEY].get_int()
        = you.elapsed_time + 150 + random2(60);
}

static void _cast_smiting(monster &caster, mon_spell_slot slot, bolt&)
{
    const god_type god = _find_god(caster, slot.flags);
    actor* foe = caster.get_foe();
    ASSERT(foe);

    if (foe->is_player())
        mprf("%s smites you!", _god_name(god).c_str());
    else
        simple_monster_message(*foe->as_monster(), " is smitten.");

    foe->hurt(&caster, 7 + random2avg(11, 2), BEAM_MISSILE, KILLED_BY_BEAM,
              "", "by divine providence");
    _whack(caster, *foe);
}

static void _cast_brain_bite(monster &caster, mon_spell_slot slot, bolt&)
{
    actor* foe = caster.get_foe();
    ASSERT(foe);

    int dam_multiplier = 1;
    int drain = 0;

    if (foe->is_player())
    {
        if (you.magic_points <= you.max_magic_points / 5)
        {
            dam_multiplier = 2;
            mpr("Something gnaws heavily on your mind!");
            xom_is_stimulated(30);
        }
        else
            mpr("Something gnaws on your mind!");

    }
    else
    {
        monster* mon_foe = foe->as_monster();
        if (mon_foe->has_ench(ENCH_ANTIMAGIC))
        {
            dam_multiplier = 2;
            simple_monster_message(*foe->as_monster(), " mind is heavily gnawed upon.", true);
        }
        else
            simple_monster_message(*foe->as_monster(), " mind is gnawed upon.", true);
    }

    foe->hurt(&caster, (4 + random2avg(5, 2)) * dam_multiplier,
              BEAM_MISSILE, KILLED_BY_BEAM, "", "by psychic fangs");
    _whack(caster, *foe);

    if (foe->is_player())
    {
        drain = min(you.magic_points, max(1, you.max_magic_points / 5));
        if (drain > 0)
        {
            mprf(MSGCH_WARN, "You feel your power leaking away.");
            drain_mp(drain);
        }
    }
    else
    {
        enchant_actor_with_flavour(foe, &caster, BEAM_DRAIN_MAGIC,
                                   mons_spellpower(caster, slot.spell));
    }
}

static void _cast_grasping_roots(monster &caster, mon_spell_slot, bolt&)
{
    actor* foe = caster.get_foe();
    ASSERT(foe);

    const int pow = mons_spellpower(caster, SPELL_GRASPING_ROOTS);
    const int turns = 1 + random_range(div_rand_round(pow, 20),
                                       div_rand_round(pow, 12) + 1);
    dprf("Grasping roots turns: %d", turns);
    start_ranged_constriction(caster, *foe, turns, CONSTRICT_ROOTS);
}

static void _regen_monster(monster* mon, monster* source, int dur)
{
    mon->add_ench(mon_enchant(ENCH_REGENERATION, 0, source, dur));

    // Animate visuals
    bolt beam;
    beam.source = mon->pos();
    beam.target = mon->pos();
    beam.colour = ETC_HOLY;
    beam.range = LOS_RADIUS;
    beam.aimed_at_spot = true;
    beam.flavour = BEAM_VISUAL;
    beam.draw_delay = 3;
    beam.fire();
}

static void _cast_regenerate_other(monster* caster)
{
    int seen = 0;
    monster* targ = nullptr;
    for (monster_near_iterator mi(caster, LOS_NO_TRANS); mi; ++mi)
    {
        if (*mi != caster
            && mons_aligned(caster, *mi) && mi->hit_points < mi->max_hit_points
            && !mi->has_ench(ENCH_REGENERATION))
        {
            if (one_chance_in(++seen))
                targ = *mi;
        }
    }

    if (targ != nullptr)
    {
        const int pow = mons_spellpower(*caster, SPELL_REGENERATE_OTHER);
        int dur = (4 + roll_dice(2, pow / 20)) * BASELINE_DELAY;
        simple_monster_message(*targ, " wounds begin to rapidly close.", true);
        _regen_monster(targ, caster, dur);
    }
}

static void _cast_mass_regeneration(monster* caster)
{
    vector<monster*> targs;
    for (monster_near_iterator mi(caster, LOS_NO_TRANS); mi; ++mi)
    {
        if (mons_aligned(caster, *mi) && !mi->has_ench(ENCH_REGENERATION))
            targs.push_back(*mi);
    }

    if (targs.empty())
        return;

    const int pow = mons_spellpower(*caster, SPELL_MASS_REGENERATION);
    int dur = (3 + roll_dice(2, pow / 25)) * BASELINE_DELAY;
    simple_monster_message(*caster, " allies begin to heal rapidly.", true);
    for (monster* mon : targs)
        _regen_monster(mon, caster, dur);
}

static bool _cast_seismic_stomp(const monster& caster, bolt& beam, bool check_only)
{
    const int range = spell_range(SPELL_SEISMIC_STOMP, 4);
    vector<actor*> targs;
    for (actor_near_iterator mi(&caster, LOS_NO_TRANS); mi; ++mi)
    {
        if (grid_distance(mi->pos(), caster.pos()) <= range
            && !mi->is_firewood()
            && !mons_aligned(&caster, *mi))
        {
            if (check_only)
                return true;

            targs.push_back(*mi);
        }
    }

    if (targs.empty())
        return false;

    const int pow = mons_spellpower(caster, SPELL_SEISMIC_STOMP);
    const unsigned int num_targs = 2 + div_rand_round((int)max(0, pow - 50), 40);
    shuffle_array(targs);
    for (size_t i = 0; i < targs.size() && i < num_targs; ++i)
    {
        if (you.see_cell(targs[i]->pos()))
            flash_tile(targs[i]->pos(), YELLOW, 0);
    }
    animation_delay(50, true);

    for (size_t i = 0; i < targs.size() && i < num_targs; ++i)
    {
        if (!targs[i]->alive())
            continue;

        beam.source = targs[i]->pos();
        beam.target = targs[i]->pos();
        beam.fire();

        if (monster* mon = targs[i]->as_monster())
        {
            if (mon->alive() && !mon->airborne() && one_chance_in(3))
            {
                simple_monster_message(*mon, " stumbles on the uneven ground.");
                mon->speed_increment -= random_range(10, 13);
            }
        }
    }

    return true;
}

static vector<actor*> _siphon_essence_victims (const actor& caster) {

    vector<actor*> victims;

    for (actor_near_iterator acti(caster.pos(), LOS_NO_TRANS); acti; ++acti)
    {
        if (grid_distance(caster.pos(), acti->pos()) <= siphon_essence_range()
            && !acti->res_torment() && !acti->is_peripheral()
            && !mons_aligned(&caster, *acti))
        {
            victims.push_back(*acti);
        }
    }

    return victims;
}

// XXX: Unite with siphon_essence in ability.cc?
static void _cast_siphon_essence(monster &caster, mon_spell_slot, bolt&)
{
    int damage = 0;
    bool seen = false;

    if (you.see_cell(caster.pos()))
    {
        targeter_radius hitfunc(&caster, LOS_SOLID, 2);
        flash_view_delay(UA_MONSTER, DARKGREY, 200, &hitfunc);
        seen = true;
    }

    vector<actor*> victims = _siphon_essence_victims(caster);

    for (actor* victim : victims)
        damage += torment_cell(victim->pos(), &caster, TORMENT_SPELL);

    if (caster.hit_points != caster.max_hit_points && damage > 0)
    {
        int cap = 19 + caster.spell_hd(SPELL_SIPHON_ESSENCE) * 1.5;
        int heal = div_rand_round(min(damage, cap) * 2, 3);
        caster.heal(heal);

        if (seen)
            mprf("Stolen life floods into %s!", caster.name(DESC_THE).c_str());
        else
            mpr("Stolen life floods into an unseen void!");
    }
}

/// Is the given full-LOS attack spell worth casting for the given monster?
static bool _los_spell_worthwhile(const monster &mons, spell_type spell)
{
    return trace_los_attack_spell(spell, mons_spellpower(mons, spell), &mons)
           == spret::success;
}

/// Set up a fake beam, for noise-generating purposes (?)
static void _setup_fake_beam(bolt& beam, const monster&, int)
{
    beam.flavour  = BEAM_DESTRUCTION;
    beam.pierce   = true;
    // Doesn't take distance into account, but this is just a tracer so
    // we'll ignore that. We need some damage on the tracer so the monster
    // doesn't think the spell is useless against other monsters.
    beam.damage   = CONVENIENT_NONZERO_DAMAGE;
    beam.range    = LOS_RADIUS;
}

/**
 * Create a summoned monster.
 *
 * @param summoner      The monster doing the summoning.
 * @param mtyp          The type of monster to summon.
 * @param dur           The duration for which the monster should last (in aut).
 * @param slot          The spell & slot flags.
 * @param abjurable     Whether the monster is an abjurable summon.
 * @return              The summoned creature, if any.
 */

static monster* _summon(const monster &summoner, monster_type mtyp, int dur,
                        mon_spell_slot slot, bool abjurable = true)
{
    const god_type god = _find_god(summoner, slot.flags);
    return create_monster(
            mgen_data(mtyp, SAME_ATTITUDE((&summoner)), summoner.pos(),
                      summoner.foe, MG_NONE, god)
            .set_summoned(&summoner, slot.spell, dur, abjurable));
}

void init_mons_spells()
{
    monster fake_mon;
    fake_mon.type       = MONS_BLACK_DRACONIAN;
    fake_mon.hit_points = 1;
    fake_mon.mid = MID_NOBODY; // used indirectly, through _mon_special_name

    bolt pbolt;

    for (int i = 0; i < NUM_SPELLS; i++)
    {
        spell_type spell = (spell_type) i;

        _valid_mon_spells[i] = false;

        if (!is_valid_spell(spell))
            continue;

        if (setup_mons_cast(&fake_mon, pbolt, spell, false, true))
            _valid_mon_spells[i] = true;
    }
}

bool is_valid_mon_spell(spell_type spell)
{
    if (spell < 0 || spell >= NUM_SPELLS)
        return false;

    return _valid_mon_spells[spell];
}

/// Is the current spell being cast via player wizmode &z by a dummy mons?
static bool _is_wiz_cast()
{
#ifdef WIZARD
    // iffy logic but might be right enough
    return crawl_state.prev_cmd == CMD_WIZARD;
#else
    return false;
#endif
}

static bool _flavour_benefits_monster(beam_type flavour, monster& monster)
{
    switch (flavour)
    {
    case BEAM_HASTE:
        return !monster.has_ench(ENCH_HASTE);

    case BEAM_MIGHT:
        return !monster.has_ench(ENCH_MIGHT)
               && mons_has_attacks(monster);

    case BEAM_INVISIBILITY:
        return !monster.has_ench(ENCH_INVIS);

    case BEAM_HEALING:
        return monster.hit_points != monster.max_hit_points;

    case BEAM_AGILITY:
        return !monster.has_ench(ENCH_AGILE);

    case BEAM_RESISTANCE:
        return !monster.has_ench(ENCH_RESISTANCE);

    case BEAM_DOUBLE_HEALTH:
        return !monster.has_ench(ENCH_DOUBLED_HEALTH);

    case BEAM_CONCENTRATE_VENOM:
        return !monster.has_ench(ENCH_CONCENTRATE_VENOM)
               && (monster.has_spell(SPELL_SPIT_POISON)
                   || monster.has_attack_flavour(AF_POISON)
                   || monster.has_attack_flavour(AF_POISON_STRONG)
                   || monster.has_attack_flavour(AF_REACH_STING));

    default:
        return false;
    }
}

/**
 * Will the given monster consider buffing the given target? (Are they close
 * enough in type, genus, attitude, etc?)
 *
 * @param caster    The monster casting a targeted buff spell.
 * @param targ      The monster to be buffed.
 * @return          Whether the monsters are similar enough.
 */
static bool _monster_will_buff(const monster &caster, const monster &targ)
{
    if (targ.is_firewood())
        return false;

    if (!mons_aligned(&targ, &caster))
        return false;

    // don't buff only temporarily-aligned pals (charmed, hexed)
    if (!mons_atts_aligned(caster.temp_attitude(), targ.real_attitude()))
        return false;

    if (caster.type == MONS_IRONBOUND_CONVOKER
        || caster.type == MONS_AMAEMON
        || caster.type == MONS_BOUND_SOUL)
    {
        return true; // will buff any ally
    }

    if (targ.is_holy() && caster.is_holy())
        return true;

    const monster_type caster_genus = mons_genus(caster.type);

    // Order level buffs
    if (caster_genus == MONS_NAGA && mons_genus(targ.type) == MONS_SNAKE)
        return true;

    return mons_genus(targ.type) == caster_genus
           || mons_genus(targ.base_monster) == caster_genus;
}

/// Find an allied monster to cast a beneficial beam spell at.
static monster* _get_allied_target(const monster &caster, bolt &tracer)
{
    monster* selected_target = nullptr;
    int min_distance = tracer.range;

    for (monster_near_iterator targ(&caster, LOS_NO_TRANS); targ; ++targ)
    {
        if (*targ == &caster
            || !_monster_will_buff(caster, **targ)
            || !_flavour_benefits_monster(tracer.flavour, **targ))
        {
            continue;
        }

        // prefer the closest ally we can find (why?)
        const int targ_distance = grid_distance(targ->pos(), caster.pos());
        if (targ_distance < min_distance)
        {
            // Make sure we won't hit someone other than we're aiming at.
            tracer.target = targ->pos();
            fire_tracer(&caster, tracer);
            if (!mons_should_fire(tracer)
                || tracer.path_taken.back() != tracer.target)
            {
                continue;
            }

            min_distance = targ_distance;
            selected_target = *targ;
        }
    }

    return selected_target;
}

// Find an ally of the target to cast a hex at.
// Note that this deliberately does not target the player.
static bool _set_hex_target(monster* caster, bolt& pbolt)
{
    monster* selected_target = nullptr;
    int min_distance = INT_MAX;

    const actor *foe = caster->get_foe();
    if (!foe)
        return false;

    for (monster_near_iterator targ(caster, LOS_NO_TRANS); targ; ++targ)
    {
        if (*targ == caster)
            continue;

        const int targ_distance = grid_distance(targ->pos(), foe->pos());

        bool got_target = false;

        if (mons_aligned(*targ, foe)
            && !targ->has_ench(ENCH_CHARM)
            && !targ->has_ench(ENCH_HEXED)
            && !targ->is_firewood()
            && !_flavour_benefits_monster(pbolt.flavour, **targ))
        {
            got_target = true;
        }

        if (got_target && targ_distance < min_distance
            && targ_distance < pbolt.range)
        {
            // Make sure we won't hit an invalid target with this aim.
            pbolt.target = targ->pos();
            fire_tracer(caster, pbolt);
            if (!mons_should_fire(pbolt)
                || pbolt.path_taken.back() != pbolt.target)
            {
                continue;
            }

            min_distance = targ_distance;
            selected_target = *targ;
        }
    }

    if (selected_target)
    {
        pbolt.target = selected_target->pos();
        return true;
    }

    // Didn't find a target.
    return false;
}

static void _whack(const actor &caster, actor &target)
{
    if (target.alive() && target.is_monster())
        behaviour_event(target.as_monster(), ME_WHACK, &caster);
}

/**
 * What value do monsters multiply their hd with to get spellpower, for the
 * given spell?
 *
 * XXX: everything could be trivially exported to data.
 *
 * @param spell     The spell in question.
 * @return          A multiplier to HD for spellpower.
 *                  Value may exceed 200.
 */
static int _mons_power_hd_factor(spell_type spell)
{
    const mons_spell_logic* logic = map_find(spell_to_logic, spell);
    if (logic && logic->power_hd_factor)
        return logic->power_hd_factor;

    switch (spell)
    {
        case SPELL_CAUSE_FEAR:
            return 18 * ENCH_POW_FACTOR;

        case SPELL_DOOM_HOWL:
        case SPELL_MESMERISE:
            return 10 * ENCH_POW_FACTOR;

        case SPELL_SIREN_SONG:
        case SPELL_AVATAR_SONG:
            return 9 * ENCH_POW_FACTOR;

        case SPELL_MASS_CONFUSION:
        case SPELL_CONFUSION_GAZE:
            return 8 * ENCH_POW_FACTOR;

        case SPELL_CALL_DOWN_LIGHTNING:
            return 16;

        case SPELL_IOOD:
        case SPELL_FREEZE:
        case SPELL_FULMINANT_PRISM:
        case SPELL_IGNITE_POISON:
        case SPELL_ARCJOLT:
            return 8;

        case SPELL_MONSTROUS_MENAGERIE:
        case SPELL_BATTLESPHERE:
        case SPELL_IRRADIATE:
        case SPELL_FOXFIRE:
        case SPELL_MANIFOLD_ASSAULT:
        case SPELL_SHADOW_PRISM:
            return 6;

        case SPELL_SUMMON_DRAGON:
        case SPELL_SUMMON_HYDRA:
        case SPELL_MARTYRS_KNELL:
        case SPELL_HELLFIRE_MORTAR:
            return 5;

        case SPELL_CHAIN_OF_CHAOS:
        case SPELL_STICKS_TO_SNAKES:
            return 4;

        default:
            return 12;
    }
}

/**
 * Does this spell use spell_hd or just hit_dice for damage and accuracy?
 *
 * @param spell The spell in question.
 * @return True iff the spell should use spell_hd.
 */
bool mons_spell_is_spell(spell_type spell)
{
    switch (spell)
    {
        case SPELL_HOLY_BREATH:
        case SPELL_SPIT_ACID:
        case SPELL_CAUSTIC_BREATH:
        case SPELL_CHAOS_BREATH:
        case SPELL_COLD_BREATH:
        case SPELL_GLACIAL_BREATH:
        case SPELL_FIRE_BREATH:
        case SPELL_SEARING_BREATH:
        case SPELL_ELECTRICAL_BOLT:
        case SPELL_FLAMING_CLOUD:
            return false;
        default:
            return true;
    }
}

/**
 * What spellpower does a monster with the given spell_hd cast the given spell
 * at?
 *
 * @param spell     The spell in question.
 * @param hd        The spell_hd of the given monster.
 * @return          A spellpower value for the spell.
 */
int mons_power_for_hd(spell_type spell, int hd)
{
    const int power = hd * _mons_power_hd_factor(spell);
    if (spell == SPELL_PAIN)
        return max(50 * ENCH_POW_FACTOR, power);
    return power;
}

/**
 * What power does the given monster cast the given spell with?
 *
 * @param spell     The spell in question.
 * @param mons      The monster in question.
 * @return          A spellpower value for the spell.
 *                  May vary from call to call for certain weird spells.
 */
int mons_spellpower(const monster &mons, spell_type spell)
{
    return mons_power_for_hd(spell, mons.spell_hd(spell));
}

/**
 * What power does the given monster cast the given enchantment with?
 *
 * @param spell     The spell in question.
 * @param mons      The monster in question.
 * @param cap       The maximum power of the spell.
 * @return          A spellpower value for the spell, with ENCH_POW_FACTOR
 *                  removed & capped at maximum spellpower.
 */
static int _ench_power(spell_type spell, const monster &mons)
{
    const int cap = 200;
    return min(cap, mons_spellpower(mons, spell) / ENCH_POW_FACTOR);
}

int mons_spell_range(const monster &mons, spell_type spell)
{
    return mons_spell_range_for_hd(spell, mons.spell_hd(),
                                   mons.type == MONS_SPELLSPARK_SERVITOR
                                   && mons.summoner == MID_PLAYER);
}

/**
 * How much range does a monster of the given spell HD have with the given
 * spell?
 *
 * @param spell         The spell in question.
 * @param hd            The monster's effective HD for spellcasting purposes.
 * @param use_veh_bonus Whether to use Vehumet's range bonus (for player Servitor)
 * @return              -1 if the spell has an undefined range; else its range.
 */
int mons_spell_range_for_hd(spell_type spell, int hd, bool use_veh_bonus)
{
    const int power = mons_power_for_hd(spell, hd);
    return spell_range(spell, power, use_veh_bonus);
}

/**
 * What god is responsible for a spell cast by the given monster with the
 * given flags?
 *
 * Relevant for Smite messages & summons, sometimes.
 *
 * @param mons      The monster casting the spell.
 * @param flags     The slot flags;
 *                  e.g. MON_SPELL_NATURAL | MON_SPELL_NOISY.
 * @return          The god that is responsible for the spell.
 */
static god_type _find_god(const monster &mons, mon_spell_slot_flags flags)
{
    // Permanent wizard summons of Yred should have the same god even
    // though they aren't priests. This is so that e.g. the zombies of
    // Yred's bound souls will properly turn on you if you abandon
    // Yred.
    if (mons.god == GOD_YREDELEMNUL)
        return mons.god;

    // If this is a wizard spell, summons won't necessarily have the
    // same god. But intrinsic/priestly summons should.
    return flags & MON_SPELL_WIZARD ? GOD_NO_GOD : mons.god;
}

static spell_type _major_destruction_spell()
{
    return random_choose(SPELL_BOLT_OF_FIRE,
                         SPELL_FIREBALL,
                         SPELL_LIGHTNING_BOLT,
                         SPELL_STICKY_FLAME,
                         SPELL_IRON_SHOT,
                         SPELL_BOLT_OF_DRAINING,
                         SPELL_ORB_OF_ELECTRICITY);
}

static spell_type _legendary_destruction_spell()
{
    return random_choose_weighted(25, SPELL_FIREBALL,
                                  20, SPELL_ICEBLAST,
                                  15, SPELL_GHOSTLY_FIREBALL);
}

// TODO: documentme
// NOTE: usually doesn't set target, but if set, should take precedence
bolt mons_spell_beam(const monster* mons, spell_type spell_cast, int power,
                     bool check_validity)
{
    ASSERT(power > 0);

    bolt beam;

    // Initialise to some bogus values so we can catch problems.
    beam.name         = "****";
    beam.colour       = 255;
    beam.hit          = -1;
    beam.damage       = dice_def(1, 0);
    beam.ench_power   = max(1, power / ENCH_POW_FACTOR); // U G H
    beam.glyph        = 0;
    beam.flavour      = BEAM_NONE;
    beam.thrower      = KILL_NON_ACTOR;
    beam.pierce       = false;
    beam.is_explosion = false;
    beam.attitude     = mons_attitude(*mons);

    beam.range = mons_spell_range(*mons, spell_cast);

    spell_type real_spell = spell_cast;

    if (spell_cast == SPELL_MAJOR_DESTRUCTION)
        real_spell = _major_destruction_spell();
    else if (spell_cast == SPELL_LEGENDARY_DESTRUCTION)
    {
        // ones with ranges too small are fixed in setup_mons_cast
        real_spell = _legendary_destruction_spell();
    }
    else if (spell_is_soh_breath(spell_cast))
    {
        // this will be fixed up later in mons_cast
        // XXX: is this necessary?
        real_spell = SPELL_FIRE_BREATH;
    }
    beam.glyph = dchar_glyph(DCHAR_FIRED_ZAP); // default
    beam.thrower = KILL_MON_MISSILE;
    beam.origin_spell = real_spell;
    beam.source_id = mons->mid;
    beam.source_name = mons->name(DESC_A, true);

    if (!mons_spell_is_spell(real_spell))
        power = mons_power_for_hd(real_spell, mons->get_hit_dice());

    const mons_spell_logic* logic = _get_spell_logic(*mons, spell_cast);
    if (logic && logic->setup_beam)
        logic->setup_beam(beam, *mons, power);

    // FIXME: more of these should use the zap_data[] struct from beam.cc!
    switch (real_spell)
    {
    case SPELL_ORB_OF_ELECTRICITY:
        beam.foe_ratio      = random_range(40, 55); // ...
        // fallthrough to other zaps
    case SPELL_MAGIC_DART:
    case SPELL_THROW_FLAME:
    case SPELL_THROW_FROST:
    case SPELL_VENOM_BOLT:
    case SPELL_POISON_ARROW:
    case SPELL_BOLT_OF_MAGMA:
    case SPELL_BOLT_OF_FIRE:
    case SPELL_BOLT_OF_COLD:
    case SPELL_THROW_ICICLE:
    case SPELL_SHOCK:
    case SPELL_LIGHTNING_BOLT:
    case SPELL_FIREBALL:
    case SPELL_ICEBLAST:
    case SPELL_LEHUDIBS_CRYSTAL_SPEAR:
    case SPELL_DISPEL_UNDEAD:
    case SPELL_BOLT_OF_DRAINING:
    case SPELL_STICKY_FLAME:
    case SPELL_STING:
    case SPELL_IRON_SHOT:
    case SPELL_BOMBARD:
    case SPELL_STONE_ARROW:
    case SPELL_FORCE_LANCE:
    case SPELL_CORROSIVE_BOLT:
    case SPELL_HIBERNATION:
    case SPELL_SLEEP:
    case SPELL_DIG:
    case SPELL_CHARMING:
    case SPELL_BOLT_OF_LIGHT:
    case SPELL_FASTROOT:
    case SPELL_WARP_SPACE:
    case SPELL_QUICKSILVER_BOLT:
    case SPELL_PRIMAL_WAVE:
    case SPELL_BLINKBOLT:
    case SPELL_STEAM_BALL:
    case SPELL_TELEPORT_OTHER:
    case SPELL_SANDBLAST:
    case SPELL_THROW_BOLAS:
    case SPELL_HARPOON_SHOT:
    case SPELL_SOJOURNING_BOLT:
    case SPELL_THROW_PIE:
    case SPELL_NOXIOUS_CLOUD:
    case SPELL_POISONOUS_CLOUD:
    case SPELL_THORN_VOLLEY:
    case SPELL_HURL_DAMNATION:
    case SPELL_SPIT_POISON:
    case SPELL_MIASMA_BREATH:      // death drake
    case SPELL_GHOSTLY_FIREBALL:
    case SPELL_FLASH_FREEZE:
    case SPELL_REBOUNDING_BLAZE:
    case SPELL_REBOUNDING_CHILL:
    case SPELL_SPIT_LAVA:
    case SPELL_HURL_SLUDGE:
    case SPELL_HOLY_BREATH:
    case SPELL_SPIT_ACID:
    case SPELL_CAUSTIC_BREATH:
    case SPELL_ELECTRICAL_BOLT:
    case SPELL_DISPEL_UNDEAD_RANGE:
    case SPELL_STUNNING_BURST:
    case SPELL_MALIGN_OFFERING:
    case SPELL_BOLT_OF_DEVASTATION:
    case SPELL_BORGNJORS_VILE_CLUTCH:
    case SPELL_CRYSTALLIZING_SHOT:
    case SPELL_HELLFIRE_MORTAR:
    case SPELL_MAGMA_BARRAGE:
    case SPELL_SHADOW_TORPOR:
        zappy(spell_to_zap(real_spell), power, true, beam);
        break;

    case SPELL_FREEZING_CLOUD: // battlesphere special-case
        zappy(ZAP_FREEZING_BLAST, power, true, beam);
        break;

    case SPELL_MALMUTATE:
        beam.flavour  = BEAM_MALMUTATE;
        break;

    case SPELL_FIRE_STORM:
        setup_fire_storm(mons, power, beam);
        beam.foe_ratio = random_range(40, 55);
        break;

    case SPELL_CALL_DOWN_DAMNATION: // Set explosion size for tracer
        zappy(spell_to_zap(real_spell), power, true, beam);
        beam.ex_size = 1;
        break;

    case SPELL_MEPHITIC_CLOUD:
        beam.name     = "stinking cloud";
        beam.damage   = dice_def(1, 0);
        beam.colour   = GREEN;
        beam.flavour  = BEAM_MEPHITIC;
        beam.hit      = 14 + power / 30;
        beam.is_explosion = true;
        break;

    case SPELL_METAL_SPLINTERS:
        zappy(spell_to_zap(real_spell), power, true, beam);
        beam.short_name = "metal splinters";
        break;

    case SPELL_SPLINTERSPRAY:
        zappy(spell_to_zap(real_spell), power, true, beam);
        beam.short_name = "splinters";
        break;

    case SPELL_BLINK_OTHER:
        beam.flavour    = BEAM_BLINK;
        break;

    case SPELL_BLINK_OTHER_CLOSE:
        beam.flavour    = BEAM_BLINK_CLOSE;
        break;

    case SPELL_FIRE_BREATH:
        zappy(spell_to_zap(real_spell), power, true, beam);
        beam.aux_source = "blast of fiery breath";
        beam.short_name = "flames";
        break;

    case SPELL_SEARING_BREATH:
        if (mons && mons->type == MONS_XTAHUA)
            power = power * 3/2;
        zappy(spell_to_zap(real_spell), power, true, beam);
        beam.aux_source  = "blast of searing breath";
        break;

    case SPELL_CHAOS_BREATH:
        zappy(spell_to_zap(real_spell), power, true, beam);
        beam.aux_source   = "blast of chaotic breath";
        break;

    case SPELL_COLD_BREATH:
        if (mons && mons_is_draconian(mons->type))
            power = power * 5 / 6;
        zappy(spell_to_zap(real_spell), power, true, beam);
        beam.aux_source = "blast of icy breath";
        beam.short_name = "frost";
        break;

    case SPELL_PORKALATOR:
        beam.name     = "porkalator";
        beam.glyph    = 0;
        beam.flavour  = BEAM_PORKALATOR;
        beam.thrower  = KILL_MON_MISSILE;
        break;

    case SPELL_IOOD:                  // tracer only
    case SPELL_PORTAL_PROJECTILE:     // for noise generation purposes
    case SPELL_GLACIATE:              // ditto
        _setup_fake_beam(beam, *mons);
        break;

    case SPELL_PETRIFYING_CLOUD:
        zappy(spell_to_zap(real_spell), power, true, beam);
        beam.foe_ratio = 30;
        break;

    case SPELL_ENSNARE:
        beam.name     = "stream of webbing";
        beam.colour   = WHITE;
        beam.glyph    = dchar_glyph(DCHAR_FIRED_MISSILE);
        beam.flavour  = BEAM_ENSNARE;
        beam.hit      = 22 + power / 20;
        break;

    case SPELL_GREATER_ENSNARE:
        beam.name     = "stream of webbing";
        beam.colour   = WHITE;
        beam.glyph    = dchar_glyph(DCHAR_FIRED_MISSILE);
        beam.flavour  = BEAM_ENSNARE;
        beam.hit      = 22 + power / 12;
        beam.aimed_at_spot = true;
        break;

    case SPELL_SPECTRAL_CLOUD:
        beam.name     = "spectral mist";
        beam.damage   = dice_def(0, 1);
        beam.colour   = CYAN;
        beam.flavour  = BEAM_MMISSILE;
        beam.hit      = AUTOMATIC_HIT;
        beam.pierce   = true;
        break;

    case SPELL_DIMENSION_ANCHOR:
        beam.flavour    = BEAM_DIMENSION_ANCHOR;
        break;

    // XXX: This seems needed to give proper spellcasting messages, even though
    //      damage is done via another means
    case SPELL_FREEZE:
        beam.flavour    = BEAM_COLD;
        break;

    case SPELL_FLAMING_CLOUD:
        zappy(spell_to_zap(real_spell), power, true, beam);
        beam.aux_source   = "blast of fiery breath";
        beam.short_name   = "flames";
        break;

    case SPELL_RAVENOUS_SWARM:
        zappy(spell_to_zap(real_spell), power, true, beam);
        beam.hit_verb     = "envelopes";
        break;

    case SPELL_THROW_BARBS:
        zappy(spell_to_zap(real_spell), power, true, beam);
        beam.hit_verb    = "skewers";
        break;

    case SPELL_DEATH_RATTLE:
        beam.name     = "vile air";
        beam.colour   = DARKGREY;
        beam.damage   = dice_def(2, 4);
        beam.hit      = AUTOMATIC_HIT;
        beam.flavour  = BEAM_DEATH_RATTLE;
        beam.foe_ratio = 30;
        beam.pierce   = true;
        break;

    case SPELL_MOURNING_WAIL:
        beam.name     = "wave of grief";
        beam.hit_verb = "passes through";
        beam.colour   = DARKGREY;
        beam.damage   = dice_def(0, 0);
        beam.hit      = AUTOMATIC_HIT;
        beam.flavour  = BEAM_NEG;
        beam.foe_ratio = 30;
        beam.pierce   = true;
        break;

    case SPELL_MARCH_OF_SORROWS:
        beam.name     = "howling sorrow";
        beam.colour   = DARKGREY;
        beam.damage   = dice_def(0, 0);
        beam.hit      = AUTOMATIC_HIT;
        beam.flavour  = BEAM_NEG;
        beam.foe_ratio = 30;
        beam.pierce   = true;
        break;

    // Special behaviour handled in _mons_upheaval
    // Hack so beam.cc allows us to correctly use that function
    case SPELL_UPHEAVAL:
        beam.flavour     = BEAM_RANDOM;
        beam.damage      = dice_def(3, 24);
        beam.hit         = AUTOMATIC_HIT;
        beam.glyph       = dchar_glyph(DCHAR_EXPLOSION);
        beam.ex_size     = 1;
        // A little lower than normal, so a stormcaller is willing to blast at
        // least a single summoned drake alongside the player.
        beam.foe_ratio   = 70;
        break;

    case SPELL_ERUPTION:
        beam.flavour     = BEAM_LAVA;
        beam.damage      = eruption_damage();
        beam.hit         = AUTOMATIC_HIT;
        beam.glyph       = dchar_glyph(DCHAR_EXPLOSION);
        beam.ex_size     = 1;
        break;

    case SPELL_PYRE_ARROW:
        zappy(spell_to_zap(real_spell), power, true, beam);

        // Purely flavor-based renames for less magical users
        if (mons->type == MONS_BOMBARDIER_BEETLE)
            beam.name = "burning spray";
        else if (mons->type == MONS_SUN_MOTH)
            beam.name = "flurry of pyrophoric scales";
        break;

    default:
        if (logic && logic->setup_beam) // already setup
            break;

        if (check_validity)
        {
            beam.flavour = NUM_BEAMS;
            return beam;
        }

        if (!is_valid_spell(real_spell))
        {
            die("Invalid spell #%d cast by %s", (int) real_spell,
                     mons->name(DESC_PLAIN, true).c_str());
        }

        die("Unknown monster spell '%s' cast by %s",
                 spell_title(real_spell),
                 mons->name(DESC_PLAIN, true).c_str());
    }

    if (beam.is_enchantment())
    {
        beam.hit = AUTOMATIC_HIT;
        beam.glyph = 0;
        beam.name = "";
    }

    // Avoid overshooting and potentially hitting the player.
    // Piercing beams' tracers already account for this.
    if (mons->temp_attitude() == ATT_FRIENDLY && !beam.pierce)
        beam.aimed_at_spot = true;

    return beam;
}

// Set up bolt structure for monster spell casting.
bool setup_mons_cast(const monster* mons, bolt &pbolt, spell_type spell_cast,
                     bool evoke,
                     bool check_validity)
{
    // always set these -- used by things other than fire_beam()

    pbolt.source_id = mons->mid;

    // Convenience for the hapless innocent who assumes that this
    // damn function does all possible setup. [ds]
    if (pbolt.target.origin())
        pbolt.target = mons->target;

    // Set bolt type and range.
    if (spell_is_direct_explosion(spell_cast))
    {
        pbolt.range = 0;
        pbolt.glyph = 0;
    }

    // The below are no-ops since they don't involve fire_tracer or beams.
    switch (spell_cast)
    {
    case SPELL_SUMMON_SMALL_MAMMAL:
    case SPELL_MAJOR_HEALING:
    case SPELL_WOODWEAL:
    case SPELL_SHADOW_CREATURES:       // summon anything appropriate for level
    case SPELL_FAKE_MARA_SUMMON:
    case SPELL_SUMMON_ILLUSION:
    case SPELL_SUMMON_DEMON:
    case SPELL_MONSTROUS_MENAGERIE:
    case SPELL_SUMMON_SPIDERS:
#if TAG_MAJOR_VERSION == 34
    case SPELL_ANIMATE_DEAD:
    case SPELL_TWISTED_RESURRECTION:
    case SPELL_SIMULACRUM:
#endif
    case SPELL_CALL_IMP:
    case SPELL_SUMMON_MINOR_DEMON:
    case SPELL_SUMMON_UFETUBUS:
    case SPELL_SUMMON_SIN_BEAST:  // Geryon
    case SPELL_SUMMON_UNDEAD:
    case SPELL_SUMMON_ICE_BEAST:
    case SPELL_SUMMON_MUSHROOMS:
    case SPELL_CONJURE_BALL_LIGHTNING:
    case SPELL_CONJURE_LIVING_SPELLS:
    case SPELL_SUMMON_DRAKES:
    case SPELL_SUMMON_HORRIBLE_THINGS:
    case SPELL_MALIGN_GATEWAY:
    case SPELL_SYMBOL_OF_TORMENT:
    case SPELL_CAUSE_FEAR:
    case SPELL_MESMERISE:
    case SPELL_SUMMON_GREATER_DEMON:
    case SPELL_BROTHERS_IN_ARMS:
    case SPELL_BERSERKER_RAGE:
    case SPELL_SPRINT:
#if TAG_MAJOR_VERSION == 34
    case SPELL_SWIFTNESS:
#endif
    case SPELL_CREATE_TENTACLES:
    case SPELL_CHAIN_LIGHTNING:    // the only user is reckless
    case SPELL_CHAIN_OF_CHAOS:
    case SPELL_SUMMON_EYEBALLS:
    case SPELL_CALL_TIDE:
    case SPELL_INK_CLOUD:
    case SPELL_SILENCE:
    case SPELL_AWAKEN_FOREST:
    case SPELL_DRUIDS_CALL:
    case SPELL_SUMMON_MORTAL_CHAMPION:
    case SPELL_SUMMON_HOLIES:
    case SPELL_SUMMON_DRAGON:
    case SPELL_SUMMON_HYDRA:
    case SPELL_FIRE_SUMMON:
#if TAG_MAJOR_VERSION == 34
    case SPELL_DEATHS_DOOR:
    case SPELL_OZOCUBUS_ARMOUR:
#endif
    case SPELL_OLGREBS_TOXIC_RADIANCE:
    case SPELL_SHATTER:
    case SPELL_BATTLESPHERE:
    case SPELL_WORD_OF_RECALL:
    case SPELL_INJURY_BOND:
    case SPELL_CALL_LOST_SOULS:
    case SPELL_BLINK_ALLIES_ENCIRCLE:
    case SPELL_MASS_CONFUSION:
    case SPELL_ENGLACIATION:
    case SPELL_AWAKEN_VINES:
    case SPELL_WALL_OF_BRAMBLES:
    case SPELL_WIND_BLAST:
    case SPELL_SUMMON_VERMIN:
    case SPELL_POLAR_VORTEX:
    case SPELL_DISCHARGE:
    case SPELL_IGNITE_POISON:
    case SPELL_SIGN_OF_RUIN:
    case SPELL_BLINK_ALLIES_AWAY:
    case SPELL_PHANTOM_MIRROR:
    case SPELL_SUMMON_MANA_VIPER:
    case SPELL_SUMMON_SCORPIONS:
    case SPELL_SUMMON_EMPEROR_SCORPIONS:
    case SPELL_BATTLECRY:
    case SPELL_WARNING_CRY:
    case SPELL_HUNTING_CALL:
    case SPELL_SEAL_DOORS:
    case SPELL_BERSERK_OTHER:
    case SPELL_SPELLSPARK_SERVITOR:
    case SPELL_THROW_ALLY:
    case SPELL_CORRUPTING_PULSE:
    case SPELL_SIREN_SONG:
    case SPELL_AVATAR_SONG:
    case SPELL_REPEL_MISSILES:
    case SPELL_SUMMON_SCARABS:
    case SPELL_CLEANSING_FLAME:
    case SPELL_DRAINING_GAZE:
    case SPELL_CONFUSION_GAZE:
    case SPELL_MARTYRS_KNELL:
    case SPELL_HAUNT:
    case SPELL_VANQUISHED_VANGUARD:
    case SPELL_BRAIN_BITE:
    case SPELL_HOLY_FLAMES:
    case SPELL_CALL_OF_CHAOS:
    case SPELL_AIRSTRIKE:
    case SPELL_WATERSTRIKE:
    case SPELL_ENTROPIC_WEAVE:
    case SPELL_SUMMON_EXECUTIONERS:
    case SPELL_DOOM_HOWL:
    case SPELL_PRAYER_OF_BRILLIANCE:
    case SPELL_GREATER_SERVANT_MAKHLEB:
    case SPELL_BIND_SOULS:
    case SPELL_DREAM_DUST:
    case SPELL_SPORULATE:
    case SPELL_ROLL:
    case SPELL_FORGE_LIGHTNING_SPIRE:
    case SPELL_SUMMON_TZITZIMITL:
    case SPELL_SUMMON_HELL_SENTINEL:
    case SPELL_STOKE_FLAMES:
    case SPELL_IRRADIATE:
    case SPELL_FUNERAL_DIRGE:
    case SPELL_MANIFOLD_ASSAULT:
    case SPELL_REGENERATE_OTHER:
    case SPELL_MASS_REGENERATION:
    case SPELL_BESTOW_ARMS:
    case SPELL_FULMINANT_PRISM:
    case SPELL_HELLFIRE_MORTAR:
    case SPELL_DAZZLING_FLASH:
        pbolt.range = 0;
        pbolt.glyph = 0;
        return true;
    default:
    {
        const mons_spell_logic* logic = _get_spell_logic(*mons, spell_cast);
        if (logic && logic->setup_beam == nullptr)
        {
            pbolt.range = 0;
            pbolt.glyph = 0;
            return true;
        }

        if (check_validity)
        {
            bolt beam = mons_spell_beam(mons, spell_cast, 1, true);
            return beam.flavour != NUM_BEAMS;
        }
        break;
    }
    }

    int power = evoke ? 30 + mons->get_hit_dice()
                      : mons_spellpower(*mons, spell_cast);

    // Laughing skulls get a power boost based on other nearby laughing skulls.
    // (Doing it here feels a little hacky, but I'm not sure where else it
    // should go, unless we duplicate bolt of draining just for their trick.)
    if (mons->type == MONS_LAUGHING_SKULL)
    {
        int skull_count = 0;
        for (distance_iterator di(mons->pos(), false, true, 5); di; ++di)
        {
            if (monster_at(*di) && monster_at(*di)->type == MONS_LAUGHING_SKULL
                && mons_aligned(mons, monster_at(*di))
                && mons->see_cell_no_trans(*di))
            {
                ++skull_count;
            }
        }

        // Power boost is +25% for each nearby skull, to a maximum of +100%
        int skull_mult = 100 + min(100, skull_count * 25);
        power = power * skull_mult / 100;
    }

    bolt theBeam = mons_spell_beam(mons, spell_cast, power);

    bolt_parent_init(theBeam, pbolt);
    if (!theBeam.target.origin())
        pbolt.target = theBeam.target;
    pbolt.source = mons->pos();
    pbolt.is_tracer = false;
    if (pbolt.aux_source.empty() && !pbolt.is_enchantment())
        pbolt.aux_source = pbolt.name;

    return true;
}

// XXX TODO: make this a zap, probably
dice_def eruption_damage()
{
    return dice_def(3, 24);
}

// Can 'binder' bind 'bound's soul with BIND_SOUL?
bool mons_can_bind_soul(monster* binder, monster* bound)
{
    return bound->holiness() & MH_NATURAL
            && mons_can_be_zombified(*bound)
            && !bound->has_ench(ENCH_BOUND_SOUL)
            && mons_aligned(binder, bound);
}

// Returns true if the spell is something you wouldn't want done if
// you had a friendly target... only returns a meaningful value for
// non-beam spells.
static bool _ms_direct_nasty(spell_type monspell)
{
    return !(get_spell_flags(monspell) & spflag::utility
             || spell_typematch(monspell, spschool::summoning));
}

// Checks if the foe *appears* to be immune to negative energy. We
// can't just use foe->res_negative_energy(), because that'll mean
// monsters will just "know" whether a player is fully life-protected.
// XX why use this logic for rN, but not rTorment, rElec, etc
static ai_action::goodness _negative_energy_spell_goodness(const actor* foe)
{
    ASSERT(foe);
    if (foe->is_player())
    {
        switch (you.undead_state())
        {
        case US_ALIVE:
            // Demonspawn are not demons, and statue form grants only
            // partial resistance.
            return ai_action::good();
        case US_SEMI_UNDEAD:
        default:
            return ai_action::bad();
        }
    }

    return ai_action::good_or_bad(!!(foe->holiness() & MH_NATURAL));
}

static bool _valid_blink_ally(const monster* caster, const monster* target)
{
    return mons_aligned(caster, target) && caster != target
           && !target->no_tele(true);
}

static bool _valid_encircle_ally(const monster* caster, const monster* target,
                                 const coord_def foepos)
{
    return _valid_blink_ally(caster, target)
           && target->pos().distance_from(foepos) > 1;
}

static bool _valid_blink_away_ally(const monster* caster, const monster* target,
                                   const coord_def foepos)
{
    return _valid_blink_ally(caster, target)
           && target->pos().distance_from(foepos) < 3;
}

static bool _valid_druids_call_target(const monster* caller, const monster* callee)
{
    return mons_aligned(caller, callee) && mons_is_beast(callee->type)
           && callee->get_experience_level() <= 20
           && !callee->is_shapeshifter()
           && !caller->see_cell(callee->pos())
           && mons_habitat(*callee) != HT_WATER
           && mons_habitat(*callee) != HT_LAVA
           && !callee->is_travelling();
}

static bool _mirrorable(const monster* agent, const monster* mon)
{
    return mon != agent
           && mons_aligned(mon, agent)
           && !mon->is_stationary()
           && !mon->is_summoned()
           && !mon->is_peripheral()
           && !mons_is_unique(mon->type);
}

static bool _valid_prayer_of_brilliance_ally(const monster* caster,
                                           const monster* target)
{
    return mons_aligned(caster, target) && caster != target
           && target->is_actual_spellcaster();
}


/**
 * Print the message that the player sees after a battlecry goes off.
 *
 * @param chief             The monster letting out the battlecry.
 * @param seen_affected     The affected monsters that are visible to the
 *                          player.
 * @param spell_cast        The battlecry type spell which is being cast.
 */
static void _print_battlecry_announcement(const monster& chief,
                                          vector<monster*> &seen_affected,
                                          spell_type spell_cast)
{
    // Disabling detailed frenzy announcement because it's so spammy.
    const msg_channel_type channel = chief.friendly() ? MSGCH_FRIEND_ENCHANT
                                                      : MSGCH_MONSTER_ENCHANT;

    if (seen_affected.size() == 1)
    {
        if (spell_cast == SPELL_BATTLECRY)
        {
            mprf(channel, "%s goes into a battle-frenzy!",
                seen_affected[0]->name(DESC_THE).c_str());
        }
        else if (spell_cast == SPELL_HUNTING_CALL)
        {
            mprf(channel, "%s picks up the pace!",
                seen_affected[0]->name(DESC_THE).c_str());
        }
        return;
    }

    // refer to the group by the most specific name possible. If they're all
    // one monster type; use that name; otherwise, use the genus name (since
    // we restrict this by genus).
    // Note: If we stop restricting by genus, use "foo's allies" instead.
    monster_type first_type = seen_affected[0]->type;
    const bool generic = any_of(
                                begin(seen_affected), end(seen_affected),
                                [=](const monster *m)
                                { return m->type != first_type; });
    monster_type group_type = generic ? mons_genus(chief.type) : first_type;

    const string ally_desc
        = pluralise_monster(mons_type_name(group_type, DESC_PLAIN));

    if (spell_cast == SPELL_BATTLECRY)
    {
        mprf(channel, "%s %s go into a battle-frenzy!",
            chief.friendly() ? "Your" : "The", ally_desc.c_str());
    }
    else if (spell_cast == SPELL_HUNTING_CALL)
    {
        mprf(channel, "%s %s pick up the pace!",
            chief.friendly() ? "Your" : "The", ally_desc.c_str());
    }
}

/**
 * Let loose a mighty battlecry, inspiring & strengthening nearby foes!
 *
 * @param chief         The monster letting out the battlecry.
 * @param spell_cast    The battlecry-type spell which is being cast.
 * @param check_only    Whether to perform a 'dry run', only checking whether
 *                      any monsters are potentially affected.
 * @return              Whether any monsters are (or would be) affected.
 */
static bool _battle_cry(const monster& chief, spell_type spell_cast,
                        bool check_only = false)
{
    const actor *foe = chief.get_foe();

    // Only let loose a battlecry if you have a valid target.
    if (!foe
        || foe->is_player() && chief.friendly()
        || !chief.see_cell_no_trans(foe->pos()))
    {
        return false;
    }

    // Don't try to make noise when silent.
    if (chief.is_silenced())
        return false;

    int affected = 0;

    vector<monster* > seen_affected;
    for (monster_near_iterator mi(chief.pos(), LOS_NO_TRANS); mi; ++mi)
    {
        const monster *mons = *mi;
        // can't buff yourself
        if (mons == &chief)
            continue;

        // only buff allies
        if (!mons_aligned(&chief, mons))
            continue;

        // no buffing confused/paralysed mons
        if (mons->berserk_or_frenzied()
            || mons->confused()
            || mons->cannot_act())
        {
            continue;
        }

        if (spell_cast == SPELL_BATTLECRY)
        {
            // already buffed
            if (mons->has_ench(ENCH_MIGHT))
                continue;
        }
        else if (spell_cast == SPELL_HUNTING_CALL)
        {
            // already fast
            if (mons->has_ench(ENCH_SWIFT) || mons->has_ench(ENCH_HASTE))
                continue;
        }

        // invalid battlecry target (wrong genus)
        if (mons_genus(mons->type) != mons_genus(chief.type))
            continue;

        if (check_only)
            return true; // just need to check

        const int dur = random_range(12, 20) * speed_to_duration(mi->speed);

        if (spell_cast == SPELL_BATTLECRY)
            mi->add_ench(mon_enchant(ENCH_MIGHT, 1, &chief, dur));
        else if (spell_cast == SPELL_HUNTING_CALL)
            mi->add_ench(mon_enchant(ENCH_SWIFT, 1, &chief, dur));

        affected++;
        if (you.can_see(**mi))
            seen_affected.push_back(*mi);

        if (mi->asleep())
            behaviour_event(*mi, ME_DISTURB, 0, chief.pos());
    }

    if (affected == 0)
        return false;

    // The yell happens whether you happen to see it or not.
    noisy(LOS_DEFAULT_RANGE, chief.pos(), chief.mid);

    if (!seen_affected.empty())
        _print_battlecry_announcement(chief, seen_affected, spell_cast);

    return true;
}

/**
 * Call upon the powers of chaos, applying mostly positive effects to nearby
 * allies!
 *
 * @param mons          The monster carrying out the call of chaos.
 * @param check_only    Whether to perform a 'dry run', only checking whether
 *                      any monsters are potentially affected.
 * @return              Whether any monsters are (or would be) affected.
 */
static bool _mons_call_of_chaos(const monster& mon, bool check_only = false)
{
    const actor *foe = mon.get_foe();

    if (!foe
        || foe->is_player() && mon.friendly()
        || !mon.see_cell_no_trans(foe->pos()))
    {
        return false;
    }

    int affected = 0;

    vector<monster* > seen_affected;
    for (monster_near_iterator mi(mon.pos(), LOS_NO_TRANS); mi; ++mi)
    {
        const monster *mons = *mi;
        // can't buff yourself
        if (mons == &mon)
            continue;

        // only buff allies
        if (!mons_aligned(&mon, mons))
            continue;

        if (mons->is_peripheral())
            continue;

        if (check_only)
            return true; // just need to check

        if (mi->asleep())
            behaviour_event(*mi, ME_DISTURB, 0, mon.pos());

        beam_type flavour = random_choose_weighted(150, BEAM_HASTE,
                                                   150, BEAM_MIGHT,
                                                   150, BEAM_DOUBLE_HEALTH,
                                                   150, BEAM_RESISTANCE,
                                                    15, BEAM_VULNERABILITY,
                                                    15, BEAM_BERSERK,
                                                    15, BEAM_POLYMORPH,
                                                    15, BEAM_INNER_FLAME);

        enchant_actor_with_flavour(*mi, &mon, flavour);
        flash_tile(mi->pos(), random_choose(RED, BLUE, GREEN, YELLOW, MAGENTA),
                   80, TILE_BOLT_CHAOS_BUFF);

        affected++;
        if (you.can_see(**mi))
            seen_affected.push_back(*mi);
    }

    if (affected == 0)
        return false;

    return true;
}

/**
 * @param mon: caster, currently only Mlioglotl
 */
static void _corrupt_locale(monster &mons)
{
    // Note that this is flagged as a bad() idea for the ai, so should
    // not be reached.
    if (player_in_branch(BRANCH_ABYSS))
        return;

    mprf("%s corrupts the dungeon around %s!",
         mons.name(DESC_THE).c_str(), mons.pronoun(PRONOUN_OBJECTIVE).c_str());

    lugonu_corrupt_level_monster(mons);
}

static void _set_door(set<coord_def> door, dungeon_feature_type feat)
{
    for (const auto &dc : door)
    {
        env.grid(dc) = feat;
        set_terrain_changed(dc);
    }
}

static int _tension_door_closed(set<coord_def> door,
                                dungeon_feature_type old_feat)
{
    // this unwind is a bit heavy, but because out-of-los clouds dissipate
    // instantly, they can be wiped out by these door tests.
    unwind_var<map<coord_def, cloud_struct>> cloud_state(env.cloud);
    _set_door(door, DNGN_CLOSED_DOOR);
    const int new_tension = get_tension(GOD_NO_GOD);
    _set_door(door, old_feat);
    return new_tension;
}

/**
 * Can any actors and items be pushed out of a doorway? An actor can be pushed
 * for purposes of this check if there is a habitable target location and the
 * actor is either the player or non-hostile. Items can be moved if there is
 * any free space.
 *
 * @param door the door position
 *
 * @return true if any actors and items can be pushed out of the door.
 */
static bool _can_force_door_shut(const coord_def& door)
{
    if (!feat_is_open_door(env.grid(door)))
        return false;

    set<coord_def> all_door;
    find_connected_identical(door, all_door);
    auto veto_spots = vector<coord_def>(all_door.begin(), all_door.end());
    auto door_spots = veto_spots;

    for (const auto &dc : all_door)
    {
        // Only attempt to push players and non-hostile monsters out of
        // doorways
        actor* act = actor_at(dc);
        if (act)
        {
            if (act->is_player()
                || act->is_monster()
                    && act->as_monster()->attitude != ATT_HOSTILE)
            {
                vector<coord_def> targets = get_push_spaces(dc, true, &veto_spots);
                if (targets.empty())
                    return false;
                veto_spots.push_back(targets.front());
            }
            else
                return false;
        }
        // If there are items in the way, see if there's room to push them
        // out of the way. Having push space for an actor doesn't guarantee
        // push space for items (e.g. with a flying actor over lava).
        if (env.igrid(dc) != NON_ITEM)
        {
            if (!has_push_spaces(dc, false, &door_spots))
                return false;
        }
    }

    // Didn't find any actors or items we couldn't displace
    return true;
}

/**
 * Get push spaces for an actor that maximize tension. If there are any push
 * spaces at all, this function is guaranteed to return something.
 *
 * @param pos the position of the actor
 * @param excluded a set of pre-excluded spots
 *
 * @return a vector of coordinates, empty if there are no push spaces at all.
 */
static vector<coord_def> _get_push_spaces_max_tension(const coord_def& pos,
                                            const vector<coord_def>* excluded)
{
    vector<coord_def> possible_spaces = get_push_spaces(pos, true, excluded);
    if (possible_spaces.empty())
        return possible_spaces;
    vector<coord_def> best;
    int max_tension = -1;
    actor *act = actor_at(pos);
    ASSERT(act);

    for (auto c : possible_spaces)
    {
        set<coord_def> all_door;
        find_connected_identical(pos, all_door);
        dungeon_feature_type old_feat = env.grid(pos);

        act->set_position(c);
        int new_tension = _tension_door_closed(all_door, old_feat);
        act->set_position(pos);

        if (new_tension == max_tension)
            best.push_back(c);
        else if (new_tension > max_tension)
        {
            max_tension = new_tension;
            best.clear();
            best.push_back(c);
        }
    }
    return best;
}

/**
 * Would forcing a door shut (possibly pushing the player) lower tension too
 * much?
 *
 * @param door the door to check
 *
 * @return true iff forcing the door shut won't lower tension by more than 1/3.
 */
static bool _should_force_door_shut(const coord_def& door)
{
    if (!feat_is_open_door(env.grid(door)))
        return false;

    dungeon_feature_type old_feat = env.grid(door);

    set<coord_def> all_door;
    find_connected_identical(door, all_door);
    auto veto_spots = vector<coord_def>(all_door.begin(), all_door.end());

    bool player_in_door = false;
    for (const auto &dc : all_door)
    {
        if (you.pos() == dc)
        {
            player_in_door = true;
            break;
        }
    }

    const int cur_tension = get_tension(GOD_NO_GOD);
    coord_def oldpos = you.pos();

    if (player_in_door)
    {
        coord_def newpos =
                _get_push_spaces_max_tension(you.pos(), &veto_spots).front();
        you.set_position(newpos);
    }

    const int new_tension = _tension_door_closed(all_door, old_feat);

    if (player_in_door)
        you.set_position(oldpos);

    dprf("Considering sealing cur tension: %d, new tension: %d",
         cur_tension, new_tension);

    // If closing the door would reduce player tension by too much, probably
    // it is scarier for the player to leave it open.
    //
    // Currently won't allow tension to be lowered by more than 33%.
    //
    // Also, if there's 0 tension, we require the door closure to create
    // tension, otherwise we'll probably just lock the player away from the
    // warden.
    return 1 + cur_tension * 66 <= new_tension * 100;
}

static bool _seal_doors_and_stairs(const monster* warden,
                                   bool check_only = false)
{
    ASSERT(warden);

    int num_closed = 0;
    int seal_duration = 80 + random2(80);
    bool player_pushed = false;
    bool had_effect = false;

    // Friendly wardens are already excluded by _monster_spell_goodness()
    if (!warden->can_see(you) || warden->foe != MHITYOU)
        return false;

    // Greedy iteration through doors/stairs that you can see.
    for (radius_iterator ri(you.pos(), LOS_RADIUS, C_SQUARE);
                 ri; ++ri)
    {
        // No closing doors, etc, through grates - can softlock.
        if (!cell_see_cell(warden->pos(), *ri, LOS_SOLID_SEE))
            continue;

        if (feat_is_open_door(env.grid(*ri)))
        {
            if (!_can_force_door_shut(*ri))
                continue;

            // If it's scarier to leave this door open, do so
            if (!_should_force_door_shut(*ri))
                continue;

            if (check_only)
                return true;

            set<coord_def> all_door;
            find_connected_identical(*ri, all_door);
            auto veto_spots = vector<coord_def>(all_door.begin(), all_door.end());
            auto door_spots = veto_spots;

            for (const auto &dc : all_door)
            {
                // If there are things in the way, push them aside
                // This is only reached for the player or non-hostile actors
                actor* act = actor_at(dc);
                if (act)
                {
                    vector<coord_def> targets =
                                _get_push_spaces_max_tension(dc, &veto_spots);
                    // at this point, _can_force_door_shut should have
                    // indicated that the door can be shut.
                    ASSERTM(!targets.empty(), "No push space from (%d,%d)",
                                                                dc.x, dc.y);
                    coord_def newpos = targets.front();

                    actor_at(dc)->move_to_pos(newpos);
                    if (act->is_player())
                    {
                        stop_delay(true);
                        player_pushed = true;
                    }
                    veto_spots.push_back(newpos);
                }
                push_items_from(dc, &door_spots);
            }

            // Close the door
            bool seen = false;
            vector<coord_def> excludes;
            for (const auto &dc : all_door)
            {
                dgn_close_door(dc);
                set_terrain_changed(dc);
                dungeon_events.fire_position_event(DET_DOOR_CLOSED, dc);

                if (is_excluded(dc))
                    excludes.push_back(dc);

                if (you.see_cell(dc))
                    seen = true;

                had_effect = true;
            }

            if (seen)
            {
                for (const auto &dc : all_door)
                {
                    if (env.map_knowledge(dc).seen())
                    {
                        env.map_knowledge(dc).set_feature(DNGN_CLOSED_DOOR);
#ifdef USE_TILE
                        tile_env.bk_bg(dc) = TILE_DNGN_CLOSED_DOOR;
#endif
                    }
                }

                update_exclusion_los(excludes);
                ++num_closed;
            }
        }

        // Try to seal the door
        if (feat_is_closed_door(env.grid(*ri)) && !feat_is_sealed(env.grid(*ri)))
        {
            if (check_only)
                return true;

            set<coord_def> all_door;
            find_connected_identical(*ri, all_door);
            const dungeon_feature_type sealed_feat =
                opc_default(*ri) == OPC_CLEAR ? DNGN_SEALED_CLEAR_DOOR
                                              : DNGN_SEALED_DOOR;
            for (const auto &dc : all_door)
            {
                temp_change_terrain(dc, sealed_feat, seal_duration,
                                    TERRAIN_CHANGE_DOOR_SEAL, warden->mid);
                had_effect = true;
            }
        }
        else if (feat_is_travelable_stair(env.grid(*ri)))
        {
            if (check_only)
                return true;

            dungeon_feature_type stype;
            if (feat_stair_direction(env.grid(*ri)) == CMD_GO_UPSTAIRS)
                stype = DNGN_SEALED_STAIRS_UP;
            else
                stype = DNGN_SEALED_STAIRS_DOWN;

            temp_change_terrain(*ri, stype, seal_duration,
                                TERRAIN_CHANGE_DOOR_SEAL, warden->mid);
            had_effect = true;
        }
    }

    if (had_effect)
    {
        ASSERT(!check_only);
        mprf(MSGCH_MONSTER_SPELL, "%s activates a sealing rune.",
                (warden->visible_to(&you) ? warden->name(DESC_THE, true).c_str()
                                          : "Someone"));
        if (num_closed > 1)
            mpr("The doors slam shut!");
        else if (num_closed == 1)
            mpr("A door slams shut!");

        if (player_pushed)
            mpr("You are pushed out of the doorway!");

        return true;
    }

    return false;
}

/// Does the given monster have a foe that's 3+ distance away?
static ai_action::goodness _foe_not_nearby(const monster &caster)
{
    const actor* foe = caster.get_foe();
    if (!foe
        || grid_distance(caster.pos(), foe->pos()) < 3
        || !caster.see_cell_no_trans(foe->pos()))
    {
        return ai_action::impossible();
    }
    return ai_action::good();
}

/// Cast the spell Call Down Lightning, blasting the target with a smitey lightning bolt. (It can miss.)
static void _cast_call_down_lightning(monster &caster, mon_spell_slot, bolt &beam)
{
    actor *foe = caster.get_foe();
    if (!foe)
        return;
    beam.source = foe->pos();
    beam.target = foe->pos();
    beam.draw_delay = 80;
    beam.fire();
}

/// Does the given monster have a foe that's adjacent to a wall, and can the
/// caster see that wall?
static ai_action::goodness _foe_near_wall(const monster &caster)
{
    const actor* foe = caster.get_foe();
    if (!foe)
        return ai_action::bad();

    if (near_visible_wall(caster.pos(), foe->pos()))
        return ai_action::good();
    return ai_action::bad();
}

static void _setup_creeping_frost(bolt &beam, const monster &, int pow)
{
    zappy(spell_to_zap(SPELL_CREEPING_FROST), pow, true, beam);
    beam.hit = AUTOMATIC_HIT;
    beam.name = "frost";
    beam.hit_verb = "grips";
    beam.aux_source = "creeping frost";
}

static void _setup_creeping_shadow(bolt &beam, const monster &, int pow)
{
    zappy(spell_to_zap(SPELL_CREEPING_SHADOW), pow, true, beam);
    beam.hit = AUTOMATIC_HIT;
    beam.name = "creeping shadows";
    beam.hit_verb = "grasp";
    beam.aux_source = "creeping shadow";
}

// Deals damage to all non-aligned creatures adjacent to visible walls within a
// given distance of the caster.
//
// (Damage type and amount is determine by the beam passed in)
static void _cast_wall_burst(monster &caster, bolt &beam, int radius)
{
    bool visible_effect = false;
    for (vision_iterator vi(caster); vi; ++vi)
    {
        if (grid_distance(caster.pos(), *vi) > radius
            || feat_is_solid(env.grid(*vi))
            || !near_visible_wall(caster.pos(), *vi))
        {
            continue;
        }

        visible_effect |= beam.explosion_draw_cell(*vi);

        // Just draw the burst if there isn't a hostile here, and skip damage.
        actor* target = actor_at(*vi);
        if (!target || mons_aligned(&caster, target))
            continue;

        // There's a hostile here; do the burst for real
        beam.source = *vi;
        beam.target = *vi;
        beam.fire();
    }
    if (visible_effect && Options.use_animations & UA_MONSTER)
        animation_delay(50, true);
}

static ai_action::goodness _mons_likes_blinking(const monster &caster)
{
    if (caster.no_tele())
        return ai_action::impossible();
    // Prefer to keep a polar vortex going rather than blink.
    if (caster.has_ench(ENCH_POLAR_VORTEX))
        return ai_action::bad();

    // Prefer not to blink if a friendly monster is targeting the player so that
    // they won't constantly blink while resting/travelling
    return ai_action::good_or_bad(caster.get_foe());
}

static ai_action::goodness _ally_needs_regeneration(const monster& caster)
{
    for (monster_near_iterator mi(&caster, LOS_NO_TRANS); mi; ++mi)
    {
        if (mons_aligned(&caster, *mi) && mi->hit_points < mi->max_hit_points
            && !mi->has_ench(ENCH_REGENERATION))
        {
            return ai_action::good();
        }
    }
    return ai_action::bad();
}

/// Can the caster see the given target's cell and lava next to (or under) them?
static bool _near_visible_lava(const monster &caster, const actor &target)
{
    if (!caster.see_cell_no_trans(target.pos()))
        return false;
    for (adjacent_iterator ai(target.pos(), false); ai; ++ai)
        if (feat_is_lava(env.grid(*ai)) && caster.see_cell_no_trans(*ai))
            return true;
    return false;
}

static ai_action::goodness _foe_near_lava(const monster &caster)
{
    const actor* foe = caster.get_foe();
    if (!foe)
        return ai_action::bad();

    if (_near_visible_lava(caster, *foe))
        return ai_action::good();
    return ai_action::bad();
}

static void _setup_pyroclastic_surge(bolt &beam, const monster &, int pow)
{
    zappy(spell_to_zap(SPELL_PYROCLASTIC_SURGE), pow, true, beam);
    beam.hit = AUTOMATIC_HIT;
    beam.name = "flame surge";
}

static bool _pyroclastic_surge(coord_def p, bolt &beam)
{
    beam.source = p;
    beam.target = p;
    beam.fire();
    return beam.explosion_draw_cell(p);
}

static void _cast_pyroclastic_surge(monster &caster, mon_spell_slot, bolt &beam)
{
    bool visible_effect = false;
    // Burn the player.
    if (!caster.wont_attack() && _near_visible_lava(caster, you))
        visible_effect |= _pyroclastic_surge(you.pos(), beam);

    // Burn the player's friends.
    for (vision_iterator vi(caster); vi; ++vi)
    {
        actor* target = actor_at(*vi);
        if (!target)
            continue;

        monster *mon = target->as_monster();
        if (!mon || mons_aligned(&caster, mon))
            continue;
        if (_near_visible_lava(caster, *mon))
            visible_effect |= _pyroclastic_surge(mon->pos(), beam);
    }
    if (visible_effect)
        animation_delay(25, true);
}

/// Should the given monster cast Arcjolt?
static ai_action::goodness _arcjolt_goodness(const monster &caster)
{
    vector<coord_def> targets = arcjolt_targets(caster, false);

    bolt tracer;
    tracer.foe_ratio = 100; // safety first
                            // XXX: rethink this if we put this on an enemy

    for (coord_def t : targets)
    {
        actor* ai = actor_at(t);
        if (!ai) continue;
        if (mons_aligned(&caster, ai))
        {
            tracer.friend_info.count++;
            tracer.friend_info.power +=
                    ai->is_player() ? you.experience_level
                                    : ai->as_monster()->get_experience_level();
        }
        else
        {
            tracer.foe_info.count++;
            tracer.foe_info.power +=
                    ai->is_player() ? you.experience_level
                                    : ai->as_monster()->get_experience_level();
        }
    }

    return mons_should_fire(tracer) ? ai_action::good() : ai_action::bad();
}

static ai_action::goodness _scorch_goodness(const monster& caster)
{
    auto targeter = make_unique<targeter_scorch>(caster, 3, true);
    for (auto ti = targeter->affected_iterator(AFF_MAYBE); ti; ++ti)
    {
        if (actor_at(*ti)->res_fire() < 3)
            return ai_action::good();
    }

    return ai_action::impossible();
}

/// Should the given monster cast Still Winds?
static ai_action::goodness _still_winds_goodness(const monster &caster)
{
    // if it's already running, don't start it again.
    if (env.level_state & LSTATE_STILL_WINDS)
        return ai_action::impossible();

    // just gonna annoy the player most of the time. don't try to be clever
    if (caster.wont_attack())
        return ai_action::bad();

    for (radius_iterator ri(caster.pos(), LOS_NO_TRANS); ri; ++ri)
    {
        const cloud_struct *cloud = cloud_at(*ri);
        if (!cloud)
            continue;

        // clouds the player might hide in are worrying.
        if (grid_distance(*ri, you.pos()) <= 3 // decent margin
            && is_opaque_cloud(cloud->type)
            && actor_cloud_immune(you, *cloud))
        {
            return ai_action::good();
        }

        // so are hazardous clouds on allies.
        const monster* mon = monster_at(*ri);
        if (mon && !actor_cloud_immune(*mon, *cloud))
            return ai_action::good();
    }

    // let's give a pass otherwise.
    return ai_action::bad();
}

static ai_action::goodness _hoarfrost_cannonade_goodness(const monster &caster)
{
    const actor* foe = caster.get_foe();
    if (!foe)
        return ai_action::bad();

    // Check if we already have at least one living cannon in LoS of our foe.
    // This should prevent recasting the spell before the cannon has a chance
    // to fire its final shots without making the monster refuse to resummon
    // if the player ducks around a corner.
    for (monster_near_iterator mi(foe->pos()); mi; ++mi)
    {
        if (mi->type == MONS_HOARFROST_CANNON && mi->summoner == caster.mid)
            return ai_action::bad();
    }

    return ai_action::good();
}

/// Cast the spell Still Winds, disabling clouds across the level temporarily.
static void _cast_still_winds(monster &caster, mon_spell_slot, bolt&)
{
    ASSERT(!(env.level_state & LSTATE_STILL_WINDS));
    caster.add_ench(ENCH_STILL_WINDS);
}

static bool _make_monster_angry(const monster* mon, monster* targ, bool actual)
{
    ASSERT(mon); // XXX: change to const monster &mon
    ASSERT(targ); // XXX: change to monster &targ
    if (mon->friendly() != targ->friendly())
        return false;

    // targ is guaranteed to have a foe (needs_berserk checks this).
    // Now targ needs to be closer to *its* foe than mon is (otherwise
    // mon might be in the way).

    coord_def victim;
    if (targ->foe == MHITYOU)
        victim = you.pos();
    else if (targ->foe != MHITNOT)
    {
        const monster* vmons = &env.mons[targ->foe];
        if (!vmons->alive())
            return false;
        victim = vmons->pos();
    }
    else
    {
        // Should be impossible. needs_berserk should find this case.
        die("angered by no foe");
    }

    // If mon may be blocking targ from its victim, don't try.
    if (victim.distance_from(targ->pos()) > victim.distance_from(mon->pos()))
        return false;

    if (!actual)
        return true;

    if (you.can_see(*mon))
    {
        if (mon->type == MONS_QUEEN_BEE && (targ->type == MONS_KILLER_BEE ||
                                            targ->type == MONS_MELIAI))
        {
            mprf("%s calls on %s to defend %s!",
                mon->name(DESC_THE).c_str(),
                targ->name(DESC_THE).c_str(),
                mon->pronoun(PRONOUN_OBJECTIVE).c_str());
        }
        else
            mprf("%s goads %s on!", mon->name(DESC_THE).c_str(),
                 targ->name(DESC_THE).c_str());
    }

    targ->go_berserk(false);

    return true;
}

static bool _incite_monsters(const monster* mon, bool actual)
{
    if (is_sanctuary(you.pos()) || is_sanctuary(mon->pos()))
        return false;

    // Only things both in LOS of the inciter and within radius 3.
    const int radius = 3;
    int goaded = 0;
    for (monster_near_iterator mi(mon->pos(), LOS_NO_TRANS); mi; ++mi)
    {
        // XXX: Ugly hack to skip the spellcaster rules for meliai.
        if (*mi == mon || !mi->needs_berserk(mon->type != MONS_QUEEN_BEE))
            continue;

        if (is_sanctuary(mi->pos()))
            continue;

        // Cannot goad other moths of wrath!
        if (mon->type == MONS_MOTH_OF_WRATH
            && mi->type == MONS_MOTH_OF_WRATH
        // Queen bees can only incite bees.
            || mon->type == MONS_QUEEN_BEE
               && mi->type != MONS_KILLER_BEE &&
                  mi->type != MONS_MELIAI)
        {
            continue;
        }

        if (grid_distance(mon->pos(), mi->pos()) > radius)
            continue;

        const bool worked = _make_monster_angry(mon, *mi, actual);
        if (worked && (!actual || !one_chance_in(3 * ++goaded)))
            return true;
    }

    return goaded > 0;
}

// Spells for a quick get-away.
// Currently only used to get out of a net.
static bool _ms_quick_get_away(spell_type monspell)
{
    return monspell == SPELL_BLINK;
}

// Is it worth bothering to invoke recall? (Currently defined by there being at
// least 3 things we could actually recall, and then with a probability inversely
// proportional to how many HD of allies are current nearby)
bool _should_recall(monster* caller)
{
    ASSERT(caller); // XXX: change to monster &caller
    // It's a long recitation - if we're winded, we can't use it.
    if (caller->has_ench(ENCH_BREATH_WEAPON))
        return false;

    int num = 0;
    for (monster_iterator mi; mi; ++mi)
    {
        if (mons_is_recallable(caller, **mi)
            && !caller->see_cell_no_trans((*mi)->pos()))
        {
            ++num;
        }
    }

    // Since there are reinforcements we could recall, do we think we need them?
    if (num > 2)
    {
        int ally_hd = 0;
        for (monster_near_iterator mi(caller->pos(), LOS_NO_TRANS); mi; ++mi)
        {
            if (*mi != caller && caller->can_see(**mi)
                && mons_aligned(caller, *mi)
                && !mi->is_firewood())
            {
                ally_hd += mi->get_experience_level();
            }
        }
        return 25 + roll_dice(2, 22) > ally_hd;
    }
    else
        return false;
}

/**
 * Recall a bunch of monsters!
 *
 * @param mons[in] the monster doing the recall
 * @param recall_target the max number of monsters to recall.
 * @returns whether anything was recalled.
 */
bool mons_word_of_recall(monster* mons, int recall_target)
{
    unsigned short num_recalled = 0;
    vector<monster* > mon_list;

    // Build the list of recallable monsters and randomize
    for (monster_iterator mi; mi; ++mi)
    {
        // Don't recall ourselves
        if (*mi == mons)
            continue;

        if (!mons_is_recallable(mons, **mi))
            continue;

        // Don't recall things that are already close to us
        if ((mons && mons->see_cell_no_trans((*mi)->pos()))
            || (!mons && you.see_cell_no_trans((*mi)->pos())))
        {
            continue;
        }

        mon_list.push_back(*mi);
    }
    shuffle_array(mon_list);

    const coord_def target   = (mons) ? mons->pos() : you.pos();
    const unsigned short foe = (mons) ? mons->foe   : short{MHITYOU};

    // Now actually recall things
    for (monster *mon : mon_list)
    {
        coord_def empty;
        const bool could_see = you.can_see(*mon);
        if (find_habitable_spot_near(target, mons_base_type(*mon),
                                     3, empty)
            && mon->move_to_pos(empty))
        {
            mon->behaviour = BEH_SEEK;
            mon->foe = foe;
            ++num_recalled;
            if (you.can_see(*mon))
                simple_monster_message(*mon, " is recalled.");
            else if (could_see)
                mprf("%s is recalled away.", mon->name(DESC_THE, true).c_str());
        }
        // Can only recall a couple things at once
        if (num_recalled == recall_target)
            break;
    }
    return num_recalled;
}

static bool _valid_vine_spot(coord_def p)
{
    if (actor_at(p) || !monster_habitable_grid(MONS_PLANT, p))
        return false;

    int num_trees = 0;
    bool valid_trees = false;
    for (adjacent_iterator ai(p); ai; ++ai)
    {
        if (feat_is_tree(env.grid(*ai)))
        {
            // Make sure this spot is not on a diagonal to its only adjacent
            // tree (so that the vines can pull back against the tree properly)
            if (num_trees || !((*ai-p).sgn().x != 0 && (*ai-p).sgn().y != 0))
            {
                valid_trees = true;
                break;
            }
            else
                ++num_trees;
        }
    }

    if (!valid_trees)
        return false;

    // Now the connectivity check
    return !plant_forbidden_at(p, true);
}

static bool _awaken_vines(monster* mon, bool test_only = false)
{
    if (_is_wiz_cast())
    {
        mpr("Sorry, this spell isn't supported for dummies!"); //mons dummy
        return false;
    }

    vector<coord_def> spots;
    for (radius_iterator ri(mon->pos(), LOS_NO_TRANS); ri; ++ri)
    {
        if (_valid_vine_spot(*ri))
            spots.push_back(*ri);
    }

    shuffle_array(spots);

    actor* foe = mon->get_foe();
    ASSERT(foe);

    int num_vines = 1 + random2(3);
    num_vines = min(num_vines, 3 - count_summons(mon, SPELL_AWAKEN_VINES));
    bool seen = false;

    for (coord_def spot : spots)
    {
        // Don't place vines where they can't see our target
        if (!cell_see_cell(spot, foe->pos(), LOS_NO_TRANS))
            continue;

        // Don't place a vine too near to another existing one
        bool too_close = false;
        for (distance_iterator di(spot, false, true, 3); di; ++di)
        {
            monster* m = monster_at(*di);
            if (m && m->type == MONS_SNAPLASHER_VINE)
            {
                too_close = true;
                break;
            }
        }
        if (too_close)
            continue;

        // We've found at least one valid spot, so the spell should be castable
        if (test_only)
            return true;

        // Actually place the vine and update properties
        if (monster* vine = create_monster(
            mgen_data(MONS_SNAPLASHER_VINE, SAME_ATTITUDE(mon), spot, mon->foe,
                        MG_FORCE_PLACE, mon->god)
            .set_summoned(mon, SPELL_AWAKEN_VINES, random_range(250, 380), false)))
        {
            --num_vines;
            if (you.can_see(*vine))
                seen = true;
        }

        // We've finished placing all our vines
        if (num_vines == 0)
            break;
    }

    if (test_only)
        return false;
    else
    {
        if (seen)
            mpr("Vines fly forth from the trees!");
        return true;
    }
}

static bool _place_druids_call_beast(const monster* druid, monster* beast,
                                     const actor* target)
{
    for (int t = 0; t < 20; ++t)
    {
        // Attempt to find some random spot out of the target's los to place
        // the beast (but not too far away).
        coord_def area_rnd;
        area_rnd.x = random_range(-11, 11);
        area_rnd.y = random_range(-11, 11);
        coord_def area = clamp_in_bounds(target->pos() + area_rnd);
        if (cell_see_cell(target->pos(), area, LOS_DEFAULT))
            continue;

        coord_def base_spot;
        int tries = 0;
        while (tries < 10 && base_spot.origin())
        {
            find_habitable_spot_near(area, mons_base_type(*beast), 3, base_spot);
            if (cell_see_cell(target->pos(), base_spot, LOS_DEFAULT))
                base_spot.reset();
            ++tries;
        }

        if (base_spot.origin())
            continue;

        beast->move_to_pos(base_spot);

        // Wake the beast up and calculate a path to the druid's target.
        // (Note that both BEH_WANDER and MTRAV_PATROL are necessary for it
        // to follow the given path and also not randomly wander off instead)
        beast->behaviour = BEH_WANDER;
        beast->foe = druid->foe;

        monster_pathfind mp;
        if (mp.init_pathfind(beast, target->pos()))
        {
            beast->travel_path = mp.calc_waypoints();
            if (!beast->travel_path.empty())
            {
                beast->target = beast->travel_path[0];
                beast->travel_target = MTRAV_PATROL;
            }
        }

        return true;
    }

    return false;
}

static void _cast_druids_call(const monster* mon)
{
    vector<monster*> mon_list;
    for (monster_iterator mi; mi; ++mi)
    {
        if (_valid_druids_call_target(mon, *mi))
            mon_list.push_back(*mi);
    }

    shuffle_array(mon_list);

    const actor* target = mon->get_foe();
    const int num = min((int)mon_list.size(),
                        mon->get_experience_level() > 10 ? random_range(2, 3)
                                                      : random_range(1, 2));

    for (int i = 0; i < num; ++i)
        _place_druids_call_beast(mon, mon_list[i], target);
}

static double _angle_between(coord_def origin, coord_def p1, coord_def p2)
{
    double ang0 = atan2(p1.x - origin.x, p1.y - origin.y);
    double ang  = atan2(p2.x - origin.x, p2.y - origin.y);
    return min(fabs(ang - ang0), fabs(ang - ang0 + 2 * PI));
}

// Does there already appear to be a bramble wall in this direction?
// We approximate this by seeing if there are at least two briar patches in
// a ray between us and our target, which turns out to be a pretty decent
// metric in practice.
static bool _already_bramble_wall(const monster* mons, coord_def targ)
{
    bolt tracer;
    tracer.source    = mons->pos();
    tracer.target    = targ;
    tracer.range     = 12;
    tracer.is_tracer = true;
    tracer.pierce    = true;
    tracer.fire();

    int briar_count = 0;
    bool targ_reached = false;
    for (coord_def p : tracer.path_taken)
    {
        if (!targ_reached && p == targ)
            targ_reached = true;
        else if (!targ_reached)
            continue;

        if (monster_at(p) && monster_at(p)->type == MONS_BRIAR_PATCH)
            ++briar_count;
    }

    return briar_count > 1;
}

static bool _wall_of_brambles(monster* mons)
{
    mgen_data briar_mg = mgen_data(MONS_BRIAR_PATCH, SAME_ATTITUDE(mons),
                                   coord_def(-1, -1), MHITNOT, MG_FORCE_PLACE);

    // We want to raise a defensive wall if we think our foe is moving to attack
    // us, and otherwise raise a wall further away to block off their escape.
    // (Each wall type uses different parameters)
    bool defensive = mons->props[FOE_APPROACHING_KEY].get_bool();

    coord_def aim_pos = you.pos();
    coord_def targ_pos = mons->pos();

    // A defensive wall cannot provide any cover if our target is already
    // adjacent, so don't bother creating one.
    if (defensive && mons->pos().distance_from(aim_pos) == 1)
        return false;

    // Don't raise a non-defensive wall if it looks like there's an existing one
    // in the same direction already (this looks rather silly to see walls
    // springing up in the distance behind already-closed paths, and probably
    // is more likely to aid the player than the monster)
    if (!defensive)
    {
        if (_already_bramble_wall(mons, aim_pos))
            return false;
    }

    // Select a random radius for the circle used draw an arc from (affects
    // both shape and distance of the resulting wall)
    int rad = (defensive ? random_range(3, 5)
                         : min(11, mons->pos().distance_from(you.pos()) + 6));

    // Adjust the center of the circle used to draw the arc of the wall if
    // we're raising one defensively, based on both its radius and foe distance.
    // (The idea is the ensure that our foe will end up on the other side of it
    // without always raising the wall in exactly the same shape and position)
    if (defensive)
    {
        coord_def adjust = (targ_pos - aim_pos).sgn();

        targ_pos += adjust;
        if (rad == 5)
            targ_pos += adjust;
        if (mons->pos().distance_from(aim_pos) == 2)
            targ_pos += adjust;
    }

    // XXX: There is almost certainly a better way to calculate the points
    //      along the desired arcs, though this code produces the proper look.
    vector<coord_def> points;
    for (distance_iterator di(targ_pos, false, false, rad); di; ++di)
    {
        if (di.radius() == rad || di.radius() == rad - 1)
        {
            if (!actor_at(*di) && !cell_is_solid(*di))
            {
                if (defensive && _angle_between(targ_pos, aim_pos, *di) <= PI/4.0
                    || (!defensive
                        && _angle_between(targ_pos, aim_pos, *di) <= PI/(4.2 + rad/6.0)))
                {
                    points.push_back(*di);
                }
            }
        }
    }

    bool seen = false;
    for (coord_def point : points)
    {
        briar_mg.pos = point;
        briar_mg.set_summoned(mons, SPELL_NO_SPELL, 80 + random2(100), false, false);
        monster* briar = create_monster(briar_mg, false);
        if (briar && you.can_see(*briar))
            seen = true;
    }

    if (seen)
        mpr("Thorny briars emerge from the ground!");

    return true;
}

/**
 * Make the given monster cast the spell "Corrupting Pulse", corrupting
 * (temporarily malmutating) all creatures in LOS.
 *
 * @param mons  The monster in question.
 */
static void _corrupting_pulse(monster *mons)
{
    if (cell_see_cell(you.pos(), mons->pos(), LOS_DEFAULT))
    {
        targeter_radius hitfunc(mons, LOS_SOLID);
        flash_view_delay(UA_MONSTER, MAGENTA, 300, &hitfunc);

        if (!is_sanctuary(you.pos())
            && cell_see_cell(you.pos(), mons->pos(), LOS_SOLID))
        {
            int num_mutations = one_chance_in(4) ? 2 : 1;
            for (int i = 0; i < num_mutations; ++i)
                temp_mutate(RANDOM_CORRUPT_MUTATION, "wretched star");
        }
    }

    for (radius_iterator ri(mons->pos(), LOS_RADIUS, C_SQUARE); ri; ++ri)
    {
        monster *m = monster_at(*ri);
        if (m && cell_see_cell(mons->pos(), *ri, LOS_SOLID_SEE)
            && !mons_aligned(mons, m))
        {
            m->malmutate(mons);
        }
    }
}

// Returns the clone just created (null otherwise)
monster* cast_phantom_mirror(monster* mons, monster* targ, int hp_perc,
                             int summ_type, int clone_index)
{
    // Create clone.
    monster *mirror = clone_mons(targ, true);

    // Abort if we failed to place the monster for some reason.
    if (!mirror)
        return nullptr;

    // Unentangle the real monster.
    if (targ->is_constricted())
        targ->stop_being_constricted();

    if (you.props.exists(BULLSEYE_TARGET_KEY)
        && (mid_t)you.props[BULLSEYE_TARGET_KEY].get_int() == targ->mid)
    {
        targ->del_ench(ENCH_BULLSEYE_TARGET);
        you.duration[DUR_DIMENSIONAL_BULLSEYE] = 0;
        you.props.erase(BULLSEYE_TARGET_KEY);
    }

    mons_clear_trapping_net(targ);

    // Don't leak the real one with the targeting interface.
    if (you.prev_targ == targ->mid)
    {
        you.prev_targ = MID_NOBODY;
        crawl_state.cancel_cmd_repeat();
    }
    targ->reset_client_id();

    mirror->mark_summoned(summ_type, summ_dur(5), true, true);
    mirror->add_ench(ENCH_PHANTOM_MIRROR);
    mirror->summoner = mons->mid;
    mirror->hit_points = max(mirror->hit_points * hp_perc / 100, 1);
    mirror->max_hit_points = max(mirror->max_hit_points * hp_perc / 100, 1);

    // Sometimes swap the two monsters, so as to disguise the original and the
    // copy. Swap chance is 1/2, then 1/3, 1/4, etc. This gives the caster an
    // even chance of ending up at any of the original or illusion positions.
    if (one_chance_in(clone_index + 2))
        targ->swap_with(mirror);

    return mirror;
}

static bool _trace_los(const monster* agent, bool (*vulnerable)(const actor*))
{
    bolt tracer;
    tracer.foe_ratio = 0;
    for (actor_near_iterator ai(agent, LOS_NO_TRANS); ai; ++ai)
    {
        if (agent == *ai || !vulnerable(*ai))
            continue;

        if (mons_aligned(agent, *ai))
        {
            tracer.friend_info.count++;
            tracer.friend_info.power +=
                    ai->is_player() ? you.experience_level
                                    : ai->as_monster()->get_experience_level();
        }
        else
        {
            tracer.foe_info.count++;
            tracer.foe_info.power +=
                    ai->is_player() ? you.experience_level
                                    : ai->as_monster()->get_experience_level();
        }
    }
    return mons_should_fire(tracer);
}

static bool _vortex_vulnerable(const actor* victim)
{
    if (!victim)
        return false;
    return !victim->res_polar_vortex();
}

static bool _torment_vulnerable(const actor* victim)
{
    if (!victim)
        return false;
    return !victim->res_torment();
}

static bool _mutation_vulnerable(const actor* victim)
{
    if (!victim)
        return false;
    return victim->can_mutate();
}

static void _prayer_of_brilliance(monster* agent)
{
    for (monster_near_iterator mi(agent, LOS_NO_TRANS); mi; ++mi)
    {
        if (!mons_aligned(*mi, agent))
            continue;
        if (_valid_prayer_of_brilliance_ally(agent, *mi))
        {
            if (!mi->has_ench(ENCH_EMPOWERED_SPELLS) && you.can_see(**mi))
            {
               mprf("%s spells are empowered by the prayer of brilliance!",
                    mi->name(DESC_ITS).c_str());
            }
            mi->add_ench(mon_enchant(ENCH_EMPOWERED_SPELLS, 1, agent));
        }
    }
}

static bool _glaciate_tracer(monster *caster, int pow, coord_def aim)
{
    targeter_cone hitfunc(caster, spell_range(SPELL_GLACIATE, pow));
    hitfunc.set_aim(aim);

    mon_attitude_type castatt = caster->temp_attitude();
    int friendly = 0, enemy = 0;

    for (const auto &entry : hitfunc.zapped)
    {
        if (entry.second <= 0)
            continue;

        const actor *victim = actor_at(entry.first);
        if (!victim)
            continue;

        if (mons_atts_aligned(castatt, victim->temp_attitude()))
        {
            if (victim->is_player() && !(caster->holiness() & MH_DEMONIC))
                return false; // never glaciate the player! except demons
            friendly += victim->get_experience_level();
        }
        else
            enemy += victim->get_experience_level();
    }

    return enemy > friendly;
}

/**
 * Get the fraction of damage done by the given beam that's to enemies,
 * multiplied by the given scale.
 */
static int _get_dam_fraction(const bolt &tracer, int scale)
{
    if (!tracer.foe_info.power)
        return 0;
    return tracer.foe_info.power * scale
            / (tracer.foe_info.power + tracer.friend_info.power);
}

/**
 * Pick a target for Ghostly Sacrifice.
 *
 *  @param  caster       The monster casting the spell.
 *  @return The target square, or an out of bounds coord if none was found.
 */
static coord_def _mons_ghostly_sacrifice_target(const monster &caster,
                                                bolt tracer)
{
    const int dam_scale = 1000;
    int best_dam_fraction = dam_scale / 2;
    coord_def best_target = coord_def(GXM+1, GYM+1); // initially out of bounds
    if (!in_bounds(caster.pos()))
        return best_target;
    tracer.ex_size = 1;

    for (monster_near_iterator mi(&caster, LOS_NO_TRANS); mi; ++mi)
    {
        if (*mi == &caster)
            continue; // don't blow yourself up!

        if (!mons_aligned(&caster, *mi))
            continue; // can only blow up allies ;)

        if (mons_is_projectile(mi->type))
            continue;

        tracer.target = mi->pos();
        fire_tracer(&caster, tracer, true, true);
        if (mons_is_threatening(**mi)) // only care about sacrificing real mons
            tracer.friend_info.power += mi->get_experience_level() * 2;

        const int dam_fraction = _get_dam_fraction(tracer, dam_scale);
        dprf("if sacrificing %s (at %d,%d): ratio %d/%d",
             mi->name(DESC_A, true).c_str(),
             best_target.x, best_target.y, dam_fraction, dam_scale);
        if (dam_fraction > best_dam_fraction)
        {
            best_target = mi->pos();
            best_dam_fraction = dam_fraction;
            dprf("setting best target");
        }
    }

    return best_target;
}

/// Everything short of the actual explosion. Returns whether to explode.
static bool _prepare_ghostly_sacrifice(monster &caster, bolt &beam)
{
    if (!in_bounds(beam.target))
        return false; // assert?

    monster* victim = monster_at(beam.target);
    if (!victim)
        return false; // assert?

    if (you.see_cell(victim->pos()))
    {
        mprf("%s animating energy erupts into ghostly fire!",
             apostrophise(victim->name(DESC_THE)).c_str());
    }
    monster_die(*victim, &caster, true);
    return true;
}

/// Setup a negative energy explosion.
static void _setup_ghostly_beam(bolt &beam, int power, int dice)
{
    beam.colour   = CYAN;
    beam.name     = "ghostly fireball";
    beam.damage   = dice_def(dice, 6 + power / 13);
    beam.hit      = 40;
    beam.flavour  = BEAM_NEG;
    beam.is_explosion = true;
}

/// Setup and target a ghostly sacrifice explosion.
static void _setup_ghostly_sacrifice_beam(bolt& beam, const monster& caster,
                                          int power)
{
    _setup_ghostly_beam(beam, power, 5);

    beam.target = _mons_ghostly_sacrifice_target(caster, beam);
    beam.aimed_at_spot = true;  // to get noise to work properly
}

// Choose a target for Launch Bomblet. Prefer our foe, if not already in ideal
// range of it. Pick some other valid hostile, if not.
static coord_def _mons_bomblet_target(const monster& caster)
{
    // Check if foe is valid first.
    actor* foe = caster.get_foe();
    if (foe && caster.can_see(*foe) && monster_los_is_valid(&caster, foe)
        && grid_distance(caster.pos(), foe->pos()) > 1
        && grid_distance(caster.pos(), foe->pos()) <= spell_range(SPELL_DEPLOY_BOMBLET, 0))
    {
        coord_def result;
        if (find_habitable_spot_near(foe->pos(), MONS_BOMBLET, 2, result))
            return foe->pos();
    }

    // Otherwise, check all nearby hostile monsters
    vector<coord_def> targs;
    for (actor_near_iterator ai(caster.pos()); ai; ++ai)
    {
        if (!ai->is_peripheral()
            && !mons_aligned(&caster, *ai)
            && grid_distance(caster.pos(), ai->pos()) > 1
            && grid_distance(caster.pos(), ai->pos()) <= spell_range(SPELL_DEPLOY_BOMBLET, 0)
            && caster.can_see(**ai)
            && monster_los_is_valid(&caster, *ai))
        {
            targs.push_back(ai->pos());
        }
    }

    // Do the 'habitable spot near' check as a second pass
    shuffle_array(targs);
    for (coord_def targ : targs)
    {
        coord_def result;
        if (find_habitable_spot_near(targ, MONS_BOMBLET, 2, result))
            return targ;
    }

    // Found nothing
    return coord_def();
}

static bool _can_injury_bond(const monster &protector, const monster &protectee)
{
    return mons_atts_aligned(protector.temp_attitude(),
                             protectee.temp_attitude())
        && !protectee.has_ench(ENCH_CHARM)
        && !protectee.has_ench(ENCH_HEXED)
        && !protectee.has_ench(ENCH_INJURY_BOND)
        && !mons_is_projectile(protectee)
        && !protectee.is_firewood()
        && &protector != &protectee;
}

/**
 * Pick a simulacrum for seracfall
 *
 *  @param  caster       The monster casting the spell.
 *  @return The target square, or an out of bounds coord if none was found.
 */
static coord_def _mons_seracfall_source(const monster &caster)
{
    bolt tracer;
    setup_mons_cast(&caster, tracer, SPELL_ICEBLAST);
    const int dam_scale = 1000;
    int best_dam_fraction = dam_scale / 2;
    coord_def best_source = coord_def(GXM+1, GYM+1); // initially out of bounds
    tracer.target  = caster.target;
    tracer.ex_size = 1;

    for (monster_near_iterator mi(&caster, LOS_NO_TRANS); mi; ++mi)
    {
        if (mi->type != MONS_SIMULACRUM)
            continue; // only hurl simulacra

        if (!mons_aligned(&caster, *mi))
            continue; // can only blow up allies ;)

        if (!you.see_cell(mi->pos()))
            continue; // don't hurl simulacra from out of player los

        // beam is shot as if the simulac is falling onto the player
        // so we treat the simulacrum as the "caster"
        fire_tracer(*mi, tracer);

        const int dam_fraction = _get_dam_fraction(tracer, dam_scale);
        dprf("if seracfalling %s (aim %d,%d): ratio %d/%d",
             mi->name(DESC_A, true).c_str(),
             tracer.target.x, tracer.target.y, dam_fraction, dam_scale);
        if (dam_fraction > best_dam_fraction)
        {
            best_source = mi->pos();
            best_dam_fraction = dam_fraction;
            dprf("setting best source (%d, %d)", best_source.x, best_source.y);
        }
    }

    return best_source;
}

static ai_action::goodness _seracfall_goodness(const monster &caster)
{
    return ai_action::good_or_impossible(in_bounds(_mons_seracfall_source(caster)));
}

static ai_action::goodness _grave_claw_goodness(const monster &caster)
{
    const actor* foe = caster.get_foe();
    if (!foe || !caster.can_see(*foe)
        || grid_distance(caster.pos(), foe->pos()) > spell_range(SPELL_GRAVE_CLAW, 0))
    {
        return ai_action::impossible();
    }

    // Slightly reduce spamming against players, to avoid locking them in place
    // on bad rolls.
    if (foe->is_player() && you.duration[DUR_NO_MOMENTUM])
        return ai_action::bad();

    return ai_action::good();
}


/// Everything short of the actual explosion. Returns whether to fire.
static bool _prepare_seracfall(monster &caster, bolt &beam)
{
    beam.source = _mons_seracfall_source(caster);
    monster* victim = monster_at(beam.source);
    if (!victim || victim->mid == caster.mid)
        return false; // assert?

    // Beam origin message handled here
    beam.seen = true;
    mprf("%s collapses into a mass of ice!", victim->name(DESC_THE).c_str());
    monster_die(*victim, &caster, true);
    return true;
}

/// Setup and target a seracfall.
static void _setup_seracfall_beam(bolt& beam, const monster& caster,
                                          int power)
{
    zappy(spell_to_zap(SPELL_SERACFALL), power, true, beam);
    beam.source = _mons_seracfall_source(caster);
    beam.name = "seracfall";
    beam.hit_verb = "batters";
}

static function<ai_action::goodness(const monster&)> _setup_hex_check(spell_type spell)
{
    return [spell](const monster& caster) {
        return _hexing_goodness(caster, spell);
    };
}

/**
 * Does the given monster think it's worth casting the given hex at its current
 * target?
 *
 * XXX: very strongly consider removing this logic!
 *
 * @param caster    The monster casting the hex.
 * @param spell     The spell to cast; e.g. SPELL_DIMENSIONAL_ANCHOR.
 * @return          Whether the monster thinks it's worth trying to beat the
 *                  defender's willpower.
 */
static ai_action::goodness _hexing_goodness(const monster &caster, spell_type spell)
{
    const actor* foe = caster.get_foe();
    ASSERT(foe);

    // Occasionally we don't estimate... just fire and see.
    if (one_chance_in(5))
        return ai_action::good();

    // Only intelligent monsters estimate.
    if (mons_intel(caster) < I_HUMAN)
        return ai_action::good();

    // Simulate Strip Willpower's 1/3 chance of ignoring WL
    if (spell == SPELL_STRIP_WILLPOWER && one_chance_in(3))
        return ai_action::good();

    // We'll estimate the target's resistance to magic, by first getting
    // the actual value and then randomising it.
    const int est_magic_resist = foe->willpower() + random2(60) - 30; // +-30
    const int power = ench_power_stepdown(mons_spellpower(caster, spell));

    // Determine the amount of chance allowed by the benefit from
    // the spell. The estimated difficulty is the probability
    // of rolling over 100 + diff on 2d100. -- bwr
    int diff = (spell == SPELL_PAIN
                || spell == SPELL_SLOW
                || spell == SPELL_CONFUSE) ? 0 : 50;

    return ai_action::good_or_bad(est_magic_resist - power <= diff);
}

/** Chooses a matching spell from this spell list, based on frequency.
 *
 *  @param[in]  spells     the monster spell list to search
 *  @param[in]  flag       what spflag the spell should match
 *  @return The spell chosen, or a slot containing SPELL_NO_SPELL and
 *          MON_SPELL_NO_FLAGS if no spell was chosen.
 */
static mon_spell_slot _pick_spell_from_list(const monster_spells &spells,
                                            spflag flag)
{
    spell_type spell_cast = SPELL_NO_SPELL;
    mon_spell_slot_flags slot_flags = MON_SPELL_NO_FLAGS;
    int weight = 0;
    for (const mon_spell_slot &slot : spells)
    {
        spell_flags flags = get_spell_flags(slot.spell);
        if (!(flags & flag))
            continue;

        weight += slot.freq;
        if (x_chance_in_y(slot.freq, weight))
        {
            spell_cast = slot.spell;
            slot_flags = slot.flags;
        }
    }

    return { spell_cast, 0, slot_flags };
}

/**
 * Are we a short distance from our target?
 *
 * @param  mons The monster checking distance from its target.
 * @return true if we have a target and are within LOS_DEFAULT_RANGE / 2 of that
 *         target, or false otherwise.
 */
static bool _short_target_range(const monster *mons)
{
    return mons->get_foe()
           && mons->pos().distance_from(mons->get_foe()->pos())
              < LOS_DEFAULT_RANGE / 2;
}

/**
 * Are we a long distance from our target?
 *
 * @param  mons The monster checking distance from its target.
 * @return true if we have a target and are outside LOS_DEFAULT_RANGE / 2 of
 *          that target, or false otherwise.
 */
static bool _long_target_range(const monster *mons)
{
    return mons->get_foe()
           && mons->pos().distance_from(mons->get_foe()->pos())
              > LOS_DEFAULT_RANGE / 2;
}

/// Does the given monster think it's in an emergency situation?
static bool _mons_in_emergency(const monster &mons)
{
    return mons.hit_points < mons.max_hit_points / 3;
}

static bool _spell_flags_invalid_for_situation(const monster& mons,
                                               const mon_spell_slot spell)
{
    return (spell.flags & MON_SPELL_EMERGENCY
            && !_mons_in_emergency(mons))
            || (spell.flags & MON_SPELL_SHORT_RANGE
                && !_short_target_range(&mons))
            || (spell.flags & MON_SPELL_LONG_RANGE
                && !_long_target_range(&mons));
}

/**
 * Choose a spell for the given monster to consider casting.
 *
 * @param mons              The monster considering casting a spell/
 * @param hspell_pass       The set of spells to choose from.
 * @param prefer_selfench   Whether to prefer self-enchantment spells, which
 *                          are more likely to be castable.
 * @return                  A spell to cast, or { SPELL_NO_SPELL }.
 */
static mon_spell_slot _find_spell_prospect(const monster &mons,
                                           const monster_spells &hspell_pass,
                                           bool prefer_selfench)
{

    // Setup spell.
    // If we didn't find a spell on the first pass, try a
    // self-enchantment.
    if (prefer_selfench)
        return _pick_spell_from_list(hspell_pass, spflag::selfench);

    // Monsters that are fleeing or pacified and leaving the
    // level will always try to choose an escape spell.
    if (mons_is_fleeing(mons) || mons.pacified())
        return _pick_spell_from_list(hspell_pass, spflag::escape);

    unsigned what = random2(200);
    unsigned int i = 0;
    for (; i < hspell_pass.size(); i++)
    {
        if (_spell_flags_invalid_for_situation(mons, hspell_pass[i]))
            continue;

        if (hspell_pass[i].freq >= what)
            break;
        what -= hspell_pass[i].freq;
    }

    // If we roll above the weight of the spell list,
    // don't cast a spell at all.
    if (i == hspell_pass.size())
        return { SPELL_NO_SPELL, 0, MON_SPELL_NO_FLAGS };

    return hspell_pass[i];
}

/**
 * Would it be a good idea for the given monster to cast the given spell?
 *
 * @param mons      The monster casting the spell.
 * @param spell     The spell in question; e.g. SPELL_FIREBALL.
 * @param beem      A beam with the spell loaded into it; used as a tracer.
 * @param ignore_good_idea      Whether to be almost completely indiscriminate
 *                              with beam spells. XXX: refactor this out?
 */
static bool _should_cast_spell(const monster &mons, spell_type spell,
                               bolt &beem, bool ignore_good_idea)
{
    // beam-type spells requiring tracers
    if (get_spell_flags(spell) & spflag::needs_tracer)
    {
        const bool explode = spell_is_direct_explosion(spell);
        fire_tracer(&mons, beem, explode);
        // Good idea?
        return mons_should_fire(beem, ignore_good_idea);
    }

    // Spells with custom marionette logic get to bypass certain normal checks
    // (largely so that they will use some aggressive 'self-buffs' without
    // needing the presence of another enemy.)
    if (mons.attitude == ATT_MARIONETTE && spell_has_marionette_override(spell))
        return true;

    // All direct-effect/summoning/self-enchantments/etc.
    const actor *foe = mons.get_foe();
    if (_ms_direct_nasty(spell)
        && mons_aligned(&mons, (mons.foe == MHITYOU) ?
                        &you : foe)) // foe=get_foe() is nullptr for friendlies
    {                                // targeting you, which is bad here.
        return false;
    }

    // Don't use blinking spells in sight of a trap the player can see if we're
    // allied with the player; this might do more harm than good to the player
    // restrict to the ones the player can see to avoid an information leak
    // TODO: move this logic into _mons_likes_blinking?
    if (mons_aligned(&mons, &you)
        && (spell == SPELL_BLINK || spell == SPELL_BLINK_OTHER
            || spell == SPELL_BLINK_OTHER_CLOSE || spell == SPELL_BLINK_CLOSE
            || spell == SPELL_BLINK_RANGE || spell == SPELL_BLINK_AWAY
            || spell == SPELL_BLINK_ALLIES_ENCIRCLE || spell == SPELL_BLINKBOLT
            || spell == SPELL_BLINK_ALLIES_AWAY))
    {
        for (auto ri = radius_iterator(mons.pos(), LOS_NO_TRANS); ri; ++ri)
            if (feat_is_trap(env.grid(*ri)) && you.see_cell(*ri))
                return false;
    }


    if (mons.foe == MHITYOU || mons.foe == MHITNOT)
    {
        // XXX: Note the crude hack so that monsters can
        // use ME_ALERT to target (we should really have
        // a measure of time instead of peeking to see
        // if the player is still there). -- bwr
        return you.visible_to(&mons)
               || mons.target == you.pos() && coinflip();
    }

    ASSERT(foe);
    if (!mons.can_see(*foe))
        return false;

    return true;
}

/// How does Ru describe stopping the given monster casting a spell?
static string _ru_spell_stop_desc(monster &mons)
{
    if (mons.is_actual_spellcaster())
        return "cast a spell";
    if (mons.is_priest())
        return "pray";
    return "attack";
}

/// What spells can the given monster currently use?
static monster_spells _find_usable_spells(monster &mons)
{
    // TODO: make mons param const (requires waste_of_time param to be const)

    monster_spells hspell_pass(mons.spells);

    if (mons.is_silenced() || mons.is_shapeshifter())
    {
        erase_if(hspell_pass, [](const mon_spell_slot &t) {
            return t.flags & MON_SPELL_SILENCE_MASK;
        });
    }

    // Remove currently useless spells.
    erase_if(hspell_pass, [&](const mon_spell_slot &t) {
        return !ai_action::is_viable(_monster_spell_goodness(&mons, t))
        // Should monster not have selected dig by now,
        // it never will.
        || t.spell == SPELL_DIG;
    });

    // Erase zero-frequency spells
    erase_if(hspell_pass, [](const mon_spell_slot &t) {
        return t.freq == 0;});

    return hspell_pass;
}

/**
 * For the given spell and monster, try to find a target for the spell, and
 * and then see if it's a good idea to actually cast the spell at that target.
 * Return whether we succeeded in both.
 *
 * @param mons            The monster casting the spell. XXX: should be const
 * @param beem[in,out]    A targeting beam. Has a very few params already set.
 *                        (from setup_targeting_beam())
 * @param spell           The spell to be targeted and cast.
 * @param ignore_good_idea  Whether to ignore most targeting constraints (ru)
 */
static bool _target_and_justify_spell(monster &mons,
                                      bolt &beem,
                                      spell_type spell,
                                      bool ignore_good_idea)
{
    // Setup the spell.
    setup_mons_cast(&mons, beem, spell);

    switch (spell)
    {
        case SPELL_CHARMING:
            // Try to find an ally of the player to hex if we are
            // hexing the player.
            if (mons.foe == MHITYOU && !_set_hex_target(&mons, beem))
                return false;
            break;
        default:
            break;
    }

    // special beam targeting sets the beam's target to an out-of-bounds coord
    // if no valid target was found.
    const mons_spell_logic* logic = _get_spell_logic(mons, spell);
    if (logic && logic->setup_beam && !in_bounds(beem.target))
        return false;

    // Don't knockback something we're trying to constrict.
    const actor *victim = actor_at(beem.target);
    if (victim
        && beem.can_knockback()
        && !victim->is_stationary()
        && mons.is_constricting(*victim))
    {
        return false;
    }

    return _should_cast_spell(mons, spell, beem, ignore_good_idea);
}

/**
 * Let a monster choose a spell to cast; may be SPELL_NO_SPELL.
 *
 * @param mons          The monster doing the casting, potentially.
 *                      TODO: should be const (requires _ms_low_hitpoint_cast
                        param to be const)
 * @param orig_beem[in,out]     A beam. Has a very few params already set.
 *                              (from setup_targeting_beam())
 *                              TODO: split out targeting into another func
 * @param hspell_pass   A list of valid spells to consider casting.
 * @param ignore_good_idea      Whether to be almost completely indiscriminate
 *                              with beam spells. XXX: refactor this out?
 * @return              A spell to cast, or SPELL_NO_SPELL.
 */
static mon_spell_slot _choose_spell_to_cast(monster &mons,
                                            bolt &beem,
                                            const monster_spells &hspell_pass,
                                            bool ignore_good_idea)
{
    // Monsters caught in a net try to get away.
    // This is only urgent if enemies are around.
    // TODO this seems kind of pointless with a 1/15 chance?
    if (mon_enemies_around(&mons) && mons.caught() && one_chance_in(15))
        for (const mon_spell_slot &slot : hspell_pass)
            if (_ms_quick_get_away(slot.spell))
                return slot;

    bolt orig_beem = beem;

    // Promote the casting of useful spells for low-HP monsters.
    // (kraken should always cast their escape spell of inky).
    if (_mons_in_emergency(mons)
        && !mons_is_fleeing(mons)
        && one_chance_in(mons.type == MONS_KRAKEN ? 4 : 8))
    {
        // Note: There should always be at least some chance we don't
        // get here... even if the monster is on its last HP. That
        // way we don't have to worry about monsters infinitely casting
        // Healing on themselves (e.g. orc high priests).
        int found_spell = 0;
        mon_spell_slot chosen_slot = { SPELL_NO_SPELL, 0, MON_SPELL_NO_FLAGS };
        for (const mon_spell_slot &slot : hspell_pass)
        {
            bolt targ_beam = orig_beem;
            if (!_spell_flags_invalid_for_situation(mons, slot)
                && _target_and_justify_spell(mons, targ_beam, slot.spell,
                                             ignore_good_idea)
                && one_chance_in(++found_spell))
            {
                chosen_slot = slot;
                beem = targ_beam;
            }
        }

        if (chosen_slot.spell != SPELL_NO_SPELL)
            return chosen_slot;
    }

    // If nothing found by now, safe friendlies and good
    // neutrals will rarely cast.
    if (mons.wont_attack() && !mon_enemies_around(&mons) && !one_chance_in(10))
        return { SPELL_NO_SPELL, 0, MON_SPELL_NO_FLAGS };

    bool reroll = mons.has_ench(ENCH_EMPOWERED_SPELLS)
                  || mons.has_ench(ENCH_TOUCH_OF_BEOGH);
    for (int attempt = 0; attempt < 2; attempt++)
    {
        const bool prefer_selfench = attempt > 0 && coinflip();
        mon_spell_slot chosen_slot
            = _find_spell_prospect(mons, hspell_pass, prefer_selfench);

        // aura of brilliance gives monsters a bonus cast chance.
        if (chosen_slot.spell == SPELL_NO_SPELL && reroll)
        {
            chosen_slot = _find_spell_prospect(mons, hspell_pass,
                                               prefer_selfench);
            reroll = false;
        }

        // if we didn't roll a spell, don't make another attempt; bail.
        // (only give multiple attempts for targeting issues.)
        if (chosen_slot.spell == SPELL_NO_SPELL)
            return chosen_slot;

        // reset the beam
        beem = orig_beem;

        if (_target_and_justify_spell(mons, beem, chosen_slot.spell,
                                       ignore_good_idea))
        {
            ASSERT(chosen_slot.spell != SPELL_NO_SPELL);
            return chosen_slot;
        }
    }

    return { SPELL_NO_SPELL, 0, MON_SPELL_NO_FLAGS };
}

static bool _valid_caution_spell(spell_type type)
{
    switch (type)
    {
        // Don't consider being able to blink a valid reason to wait in place.
        // We might not be in range for anything else, and moving close is still
        // often better than doing nothing here.
        case SPELL_BLINK:
        case SPELL_BLINK_CLOSE:
            return false;

        default:
            return true;
    }
}

/**
 * Give a monster a chance to cast a spell.
 *
 * @param mons the monster that might cast.
 * @param return whether a spell was cast.
 */
bool handle_mon_spell(monster* mons)
{
    ASSERT(mons);

    if (is_sanctuary(mons->pos()) && !mons->wont_attack())
        return false;

    // Yes, there is a logic to this ordering {dlb}:
    // .. berserk check is necessary for out-of-sequence actions like emergency
    // slot spells {blue}
    if (mons->asleep()
        || mons->berserk_or_frenzied()
        || mons_is_confused(*mons, false)
        || !mons->has_spells())
    {
        return false;
    }

    const monster_spells hspell_pass = _find_usable_spells(*mons);

    // If no useful spells... cast no spell.
    if (!hspell_pass.size())
        return false;

    bolt beem = setup_targeting_beam(*mons);

    bool ignore_good_idea = false;
    if (does_ru_wanna_redirect(*mons))
    {
        ru_interference interference = get_ru_attack_interference_level();
        if (interference == DO_BLOCK_ATTACK)
        {
            const string message
                = make_stringf(" begins to %s, but is stunned by your conviction!",
                               _ru_spell_stop_desc(*mons).c_str());
            simple_monster_message(*mons, message.c_str(), false, MSGCH_GOD);
            mons->lose_energy(EUT_SPELL);
            return true;
        }
        if (interference == DO_REDIRECT_ATTACK)
        {
            int pfound = 0;
            for (radius_iterator ri(you.pos(),
                                    LOS_DEFAULT); ri; ++ri)
            {
                monster* new_target = monster_at(*ri);

                if (!new_target
                    || mons_is_projectile(new_target->type)
                    || new_target->is_firewood()
                    || new_target->friendly())
                {
                    continue;
                }

                ASSERT(new_target);

                if (one_chance_in(++pfound))
                {
                    mons->target = new_target->pos();
                    mons->foe = new_target->mindex();
                    beem.target = mons->target;
                    ignore_good_idea = true;
                }
            }

            if (ignore_good_idea)
            {
                mprf(MSGCH_GOD, "You redirect %s's attack!",
                     mons->name(DESC_THE).c_str());
            }
        }
    }

    const mon_spell_slot spell_slot
        = _choose_spell_to_cast(*mons, beem, hspell_pass, ignore_good_idea);
    const spell_type spell_cast = spell_slot.spell;
    const mon_spell_slot_flags flags = spell_slot.flags;

    // Should the monster *still* not have a spell, well, too bad {dlb}:
    if (spell_cast == SPELL_NO_SPELL)
    {
        // If we didn't choose to cast a spell this turn and we're cautious,
        // let's see if there's an aggressive spell that we *could* have.
        if (mons->flags & MF_CAUTIOUS)
        {
            mons->props.erase(MON_SPELL_USABLE_KEY);

            for (unsigned int i = 0; i < hspell_pass.size(); ++i)
            {
                bolt test_beam = beem;

                if (_valid_caution_spell(hspell_pass[i].spell)
                    && !_spell_flags_invalid_for_situation(*mons, hspell_pass[i])
                    && _target_and_justify_spell(*mons, test_beam, hspell_pass[i].spell,
                                                ignore_good_idea))
                {
                    mons->props[MON_SPELL_USABLE_KEY] = true;
                    break;
                }
            }
        }

        return false;
    }

    // Check for antimagic if casting a spell spell.
    if (mons->has_ench(ENCH_ANTIMAGIC) && flags & MON_SPELL_ANTIMAGIC_MASK
        && !x_chance_in_y(4 * BASELINE_DELAY,
                          4 * BASELINE_DELAY
                          + mons->get_ench(ENCH_ANTIMAGIC).duration))
    {
        // This may be a bad idea -- if we decide monsters shouldn't
        // lose a turn like players do not, please make this just return.
        simple_monster_message(*mons, " falters for a moment.");
        mons->lose_energy(EUT_SPELL);
        return true;
    }

    // Dragons now have a time-out on their breath weapons, draconians too!
    if (flags & MON_SPELL_BREATH)
        setup_breath_timeout(mons);

    // FINALLY! determine primary spell effects {dlb}:
    const bool battlesphere = mons->props.exists(BATTLESPHERE_KEY);
    if (!(get_spell_flags(spell_cast) & spflag::utility))
        make_mons_stop_fleeing(mons);

    mons_cast(mons, beem, spell_cast, flags);
    if (battlesphere && battlesphere_can_mirror(spell_cast))
        trigger_battlesphere(mons);
    if (flags & MON_SPELL_WIZARD && mons->has_ench(ENCH_SAP_MAGIC))
    {
        mons->add_ench(mon_enchant(ENCH_ANTIMAGIC, 0,
                                   mons->get_ench(ENCH_SAP_MAGIC).agent(),
                                   6 * BASELINE_DELAY));
    }

    // Reflection, fireballs, etc.
    if (!mons->alive())
        return true;

    // Living spells die once they are cast.
    if (mons->type == MONS_LIVING_SPELL)
    {
        monster_die(*mons, KILL_RESET, NON_MONSTER);
        return true;
    }

    if (!(flags & MON_SPELL_INSTANT))
    {
        mons->lose_energy(EUT_SPELL);
        return true;
    }

    return false; // to let them do something else
}

// Performs simple targeting and justification for a given spell, by a given
// monster.
static bool _setup_simple_mons_cast(monster& mons, spell_type spell,
                                    bolt& beam, mon_spell_slot& slot)
{
    for (const mon_spell_slot& _slot : mons.spells)
    {
        if (_slot.spell == spell)
        {
            slot = _slot;
            break;
        }
    }
    if (slot.spell == SPELL_NO_SPELL)
        return false;

    // Check if the spell is viable and target it
    if (!ai_action::is_viable(_monster_spell_goodness(&mons, slot)))
        return false;
    if (!_target_and_justify_spell(mons, beam, spell, false))
        return false;

    return true;
}

// Checks whether it would be possible for a given monster to cast a given
// spell, using its current foe and other state.
bool is_mons_cast_possible(monster& mons, spell_type spell)
{
    mon_spell_slot slot;
    bolt beam;
    return _setup_simple_mons_cast(mons, spell, beam, slot);
}

// Attempts to have a given monster cast a given spell, while still performing
// normal setup and target justification.
//
// Returns whether the spell was cast.
bool try_mons_cast(monster& mons, spell_type spell)
{
    // Perform setup (and return false if we fail)
    mon_spell_slot slot;
    bolt beam;
    if (!_setup_simple_mons_cast(mons, spell, beam, slot))
        return false;

    // Actually cast the spell
    mons_cast(&mons, beam, spell, slot.flags);

    return true;
}

bool spell_has_marionette_override(spell_type spell)
{
    return map_find(marionette_spell_to_logic, spell) != nullptr;
}

static int _monster_abjure_target(monster* target, int pow, bool actual)
{
    if (!target->is_abjurable())
        return 0;

    int duration = target->get_ench(ENCH_SUMMON_TIMER).duration;

    pow = max(20, fuzz_value(pow, 40, 25));

    if (!actual)
        return pow > 40 || pow >= duration;

    // TSO and Trog's abjuration protection.
    bool shielded = false;
    if (have_passive(passive_t::abjuration_protection_hd))
    {
        pow = pow * (30 - target->get_hit_dice()) / 30;
        if (pow < duration)
        {
            simple_god_message(" protects your fellow warrior from evil "
                               "magic!");
            shielded = true;
        }
    }
    else if (have_passive(passive_t::abjuration_protection))
    {
        pow = pow / 2;
        if (pow < duration)
        {
            simple_god_message(" shields your ally from puny magic!");
            shielded = true;
        }
    }
    else if (is_sanctuary(target->pos()))
    {
        pow = 0;
        mprf(MSGCH_GOD, "Zin's power protects your fellow warrior from evil magic!");
        shielded = true;
    }

    dprf("Abj: dur: %d, pow: %d, ndur: %d", duration, pow, duration - pow);

    mon_enchant timer = target->get_ench(ENCH_SUMMON_TIMER);
    if (!target->lose_ench_duration(timer, pow))
    {
        if (!shielded)
            simple_monster_message(*target, " shudders.");
        return 1;
    }

    return 0;
}

static int _monster_abjuration(const monster& caster, bool actual)
{
    int maffected = 0;

    if (actual)
        mpr("Send 'em back where they came from!");

    const int pow = mons_spellpower(caster, SPELL_ABJURATION);

    for (monster_near_iterator mi(caster.pos(), LOS_NO_TRANS); mi; ++mi)
    {
        if (!mons_aligned(&caster, *mi))
            maffected += _monster_abjure_target(*mi, pow, actual);
    }

    return maffected;
}

static ai_action::goodness _mons_will_abjure(const monster& mons)
{
    return ai_action::good_or_bad(_monster_abjuration(mons, false) > 0);
}

static void _haunt_fixup(monster* summon, coord_def pos)
{
    actor* victim = actor_at(pos);
    if (victim && victim != summon)
    {
        summon->add_ench(mon_enchant(ENCH_HAUNTING, 1, victim,
                                     INFINITE_DURATION));
        summon->foe = victim->mindex();
    }
}

static monster_type _pick_horrible_thing()
{
    return one_chance_in(4) ? MONS_TENTACLED_MONSTROSITY
                            : MONS_ABOMINATION_LARGE;
}

static monster_type _pick_undead_summon()
{
    static monster_type undead[] =
    {
        MONS_NECROPHAGE, MONS_JIANGSHI, MONS_FLAYED_GHOST, MONS_ZOMBIE,
        MONS_SKELETON, MONS_SIMULACRUM, MONS_SPECTRAL_THING, MONS_LAUGHING_SKULL,
        MONS_MUMMY, MONS_VAMPIRE, MONS_WIGHT, MONS_WRAITH, MONS_SHADOW_WRAITH,
        MONS_FREEZING_WRAITH, MONS_PHANTASMAL_WARRIOR, MONS_SHADOWGHAST
    };

    return RANDOM_ELEMENT(undead);
}

static monster_type _pick_vermin()
{
    return random_choose_weighted(6, MONS_HELL_RAT,
                                  3, MONS_SWAMP_WORM,
                                  2, MONS_GOLIATH_FROG,
                                  2, MONS_TARANTELLA,
                                  2, MONS_JUMPING_SPIDER,
                                  3, MONS_DEMONIC_CRAWLER);
}

static monster_type _pick_drake()
{
    return random_choose_weighted(5, MONS_SWAMP_DRAKE,
                                  6, MONS_WIND_DRAKE,
                                  8, MONS_RIME_DRAKE,
                                  7, MONS_DEATH_DRAKE,
                                  4, MONS_LINDWURM);
}

static void _do_high_level_summon(monster* mons, spell_type spell_cast,
                                  monster_type (*mpicker)(), int nsummons,
                                  god_type god, const coord_def *target = nullptr,
                                  void (*post_hook)(monster*, coord_def)
                                      = nullptr)
{
    const int duration = min(2 + mons->spell_hd(spell_cast) / 5, 6);

    for (int i = 0; i < nsummons; ++i)
    {
        monster_type which_mons = mpicker();

        if (which_mons == MONS_NO_MONSTER)
            continue;

        monster* summon = create_monster(
            mgen_data(which_mons, SAME_ATTITUDE(mons),
                      target ? *target : mons->pos(), mons->foe, MG_NONE, god)
            .set_summoned(mons, spell_cast, summ_dur(duration)));
        if (summon && post_hook)
            post_hook(summon, target ? *target : mons->pos());
    }
}


static void _mons_summon_elemental(monster &mons, mon_spell_slot slot, bolt&)
{
    static const map<spell_type, monster_type> elemental_types = {
        { SPELL_WATER_ELEMENTALS, MONS_WATER_ELEMENTAL },
        { SPELL_FIRE_ELEMENTALS, MONS_FIRE_ELEMENTAL },
        { SPELL_EARTH_ELEMENTALS, MONS_EARTH_ELEMENTAL },
        { SPELL_AIR_ELEMENTALS, MONS_AIR_ELEMENTAL },
    };

    const monster_type* mtyp = map_find(elemental_types, slot.spell);
    ASSERT(mtyp);

    const int spell_hd = mons.spell_hd(slot.spell);
    const int count = 1 + (spell_hd > 15) + random2(spell_hd / 7 + 1);

    for (int i = 0; i < count; i++)
        _summon(mons, *mtyp, summ_dur(3), slot);
}

static void _mons_sticks_to_snakes(monster& mons, mon_spell_slot slot, bolt&)
{
    const int pow = mons_spellpower(mons, SPELL_STICKS_TO_SNAKES);
    monster_type type = MONS_WATER_MOCCASIN;

    // HD > 15
    if (pow > 60)
        type = MONS_ANACONDA;
    // HD > 8
    else if (pow > 32)
        type = MONS_BLACK_MAMBA;

    for (int i = 0; i < 2; ++i)
        _summon(mons, type, summ_dur(2), slot);
}

static void _mons_shadow_puppet(monster& mons, mon_spell_slot slot, bolt&)
{
    const int pow = mons_spellpower(mons, SPELL_SHADOW_PUPPET);
    const god_type god = _find_god(mons, slot.flags);
    mgen_data mg = mgen_data(MONS_SHADOW_PUPPET, SAME_ATTITUDE((&mons)), mons.pos(),
                             mons.foe, MG_NONE, god);
    mg.set_summoned(&mons, slot.spell, summ_dur(2));
    mg.hd = 1 + div_rand_round(pow, 15);
    mg.hd += div_rand_round(max(0, pow - 80), 11);
    create_monster(mg);
}

static void _mons_shadow_turret(monster& mons, mon_spell_slot slot, bolt&)
{
    const int pow = mons_spellpower(mons, SPELL_SHADOW_TURRET);
    const god_type god = _find_god(mons, slot.flags);
    mgen_data mg = mgen_data(MONS_SHADOW_TURRET, SAME_ATTITUDE((&mons)), mons.pos(),
                             mons.foe, MG_NONE, god).set_range(1, 2);
    mg.set_summoned(&mons, slot.spell, summ_dur(2), false);
    mg.hd = 2 + div_rand_round(pow, 15);
    mg.hd += div_rand_round(max(0, pow - 80), 11);
    create_monster(mg);
}

static void _mons_summon_dancing_weapons(monster &mons, mon_spell_slot slot, bolt&)
{
    // TODO: scale by power?
    const int count = random_range(2, 4);
    for (int i = 0; i < count; i++)
    {
        monster* mon = _summon(mons, MONS_DANCING_WEAPON, summ_dur(3), slot);
        if (mon)
            mon->add_ench(ENCH_HASTE);
    }
}

static void _cast_divine_armament(monster& mons, mon_spell_slot slot, bolt&)
{
    monster* weapon = _summon(mons, MONS_DANCING_WEAPON, summ_dur(2), slot);

    // If we created a base weapon, adjust it to match the caster
    if (weapon)
    {
        const int tier = mons.get_experience_level() > 17 ? 3
                         : mons.get_experience_level() > 11 ? 2
                         : 1;

        int pow = min(100, mons.get_experience_level() * 5);

        // Choose weapon type consistently based on the caster
        item_def& wpn(*weapon->weapon());

        switch (tier)
        {
            case 3:
                wpn.sub_type = mons.mid % 2 == 0 ? WPN_EXECUTIONERS_AXE : WPN_BARDICHE;
                set_item_ego_type(wpn, OBJ_WEAPONS, SPWPN_SPEED);
                wpn.plus = 0;
                pow = 100;
                break;

            case 2:
                wpn.sub_type = mons.mid % 2 == 0 ? WPN_BATTLEAXE : WPN_GLAIVE;
                set_item_ego_type(wpn, OBJ_WEAPONS, SPWPN_ELECTROCUTION);
                wpn.plus = 0;
                pow = 70 + mons.get_experience_level();
                break;

            default:
            case 1:
                wpn.sub_type = mons.mid % 2 == 0 ? WPN_WAR_AXE : WPN_TRIDENT;
                set_item_ego_type(wpn, OBJ_WEAPONS, SPWPN_NORMAL);
                wpn.plus = 0;
                pow = 50 + mons.get_experience_level() * 2;
                break;
        }

        ghost_demon newstats;
        newstats.init_dancing_weapon(wpn, pow);
        weapon->set_ghost(newstats);
        weapon->ghost_demon_init();
    }
}

static void _mons_cast_haunt(monster* mons)
{
    ASSERT(mons->get_foe());
    const coord_def fpos = mons->get_foe()->pos();

    _do_high_level_summon(mons, SPELL_HAUNT, pick_random_wraith,
                          random_range(2, 3), GOD_NO_GOD, &fpos, _haunt_fixup);
}

static void _mons_cast_summon_illusion(monster* mons, spell_type spell)
{
    actor *foe = mons->get_foe();
    if (!foe || !actor_is_illusion_cloneable(foe))
        return;

    mons_summon_illusion_from(mons, foe, spell);
}

static void _cast_mortal_champion(monster* mons)
{
    ASSERT(mons->get_foe());

    monster_type type = coinflip() ? MONS_SPRIGGAN_DEFENDER
                                   : MONS_DEEP_ELF_BLADEMASTER;

    if (monster *mortal = create_monster(
            mgen_data(type, SAME_ATTITUDE(mons), mons->pos(), mons->foe, MG_NONE,
                      mons->god)
            .set_summoned(mons, SPELL_SUMMON_MORTAL_CHAMPION, summ_dur(3))))
    {
        // Replace their weapons with short blades of holy wrath- aside from
        // feeling more holy, it avoids the spriggans' demon whips.
        weapon_type wpn = WPN_RAPIER;
        armour_type arm = ARM_PEARL_DRAGON_ARMOUR;

        destroy_item(mortal->inv[MSLOT_WEAPON]);

        if (type == MONS_SPRIGGAN_DEFENDER)
        {
            wpn = WPN_QUICK_BLADE;
            if (mortal->inv[MSLOT_SHIELD] == NON_ITEM)
            {
                give_specific_item(mortal, items(false, OBJ_WEAPONS,
                                   ARM_BUCKLER, 0, SPARM_FORBID_EGO));
            }
        }
        else if (type == MONS_DEEP_ELF_BLADEMASTER)
        {
            destroy_item(mortal->inv[MSLOT_ALT_WEAPON]);
            give_specific_item(mortal, items(false, OBJ_WEAPONS, wpn, 0, SPWPN_HOLY_WRATH));
        }

        give_specific_item(mortal, items(false, OBJ_WEAPONS, wpn, 0, SPWPN_HOLY_WRATH));
        give_specific_item(mortal, items(false, OBJ_ARMOUR, arm, 0, SPARM_FORBID_EGO));

        // Re-mark the items we just gave as summoned
        mortal->mark_summoned();
    }
}


static void _cast_vanquished_vanguard(monster* mons)
{
    ASSERT(mons->get_foe());
    const coord_def fpos = mons->get_foe()->pos();

    int pow = mons->get_experience_level();

    for (int i = 0; i < 3; ++i)
    {
        monster_type type = MONS_ORC_WARRIOR;
        if (x_chance_in_y(max(0, pow - 10), 20))
            type = MONS_ORC_WARLORD;
        else if (x_chance_in_y(max(0, pow - 5), 20))
            type = MONS_ORC_KNIGHT;

        if (monster *orc = create_monster(
                mgen_data(MONS_SPECTRAL_THING, SAME_ATTITUDE(mons), fpos,
                          mons->foe, MG_NONE, mons->god)
                .set_summoned(mons, SPELL_VANQUISHED_VANGUARD, summ_dur(3))
                .set_base(type)))
        {
            weapon_type wpn = WPN_TRIDENT;
            armour_type arm = ARM_SCALE_MAIL;
#ifdef USE_TILE
            orc->props[MONSTER_TILE_KEY] = TILEP_MONS_FALLEN_ORC_WARRIOR;
#endif
            if (type == MONS_ORC_KNIGHT)
            {
                wpn = WPN_HALBERD;
                arm = ARM_CHAIN_MAIL;
#ifdef USE_TILE
                orc->props[MONSTER_TILE_KEY] = TILEP_MONS_FALLEN_ORC_KNIGHT;
#endif
            }
            else if (type == MONS_ORC_WARLORD)
            {
                wpn = WPN_BARDICHE;
                arm = ARM_PLATE_ARMOUR;
#ifdef USE_TILE
                orc->props[MONSTER_TILE_KEY] = TILEP_MONS_FALLEN_ORC_WARLORD;
#endif
            }

            give_specific_item(orc, items(false, OBJ_WEAPONS, wpn, 0, SPWPN_FORBID_BRAND));
            give_specific_item(orc, items(false, OBJ_ARMOUR, arm, 0, SPARM_FORBID_EGO));

            // Mark the items we just gave the orc as summoned
            orc->mark_summoned();
        }
    }
}

static bool _mons_cast_freeze(monster* mons)
{
    actor *target = mons->get_foe();
    if (!target)
        return false;
    if (grid_distance(mons->pos(), target->pos()) > 1)
        return false;

    const int pow = mons_spellpower(*mons, SPELL_FREEZE);

    // We use non-random damage for monster Freeze so that the damage display
    // is simple to display to players without being misleading.
    const int base_damage = zap_damage(ZAP_FREEZE, pow, true, false).roll();
    const int damage = resist_adjust_damage(target, BEAM_COLD, base_damage);

    if (you.can_see(*target))
    {
        mprf("%s %s frozen%s", target->name(DESC_THE).c_str(),
                              target->conj_verb("are").c_str(),
                              attack_strength_punctuation(damage).c_str());
    }

    // Resist messaging, needs to happen after so we get the correct message
    // order; resist_adjust_damage is used to compute the punctuation.
    if (target->is_player())
        check_your_resists(base_damage, BEAM_COLD, "");
    else
    {
        bolt beam;
        beam.flavour = BEAM_COLD;
        mons_adjust_flavoured(target->as_monster(), beam, base_damage);
    }

    target->hurt(mons, damage, BEAM_COLD, KILLED_BY_FREEZING);

    if (target->alive())
    {
        target->expose_to_element(BEAM_COLD, damage, mons);
        _whack(*mons, *target);
    }

    return true;
}

void setup_breath_timeout(monster* mons)
{
    if (mons->has_ench(ENCH_BREATH_WEAPON))
        return;

    const int timeout = roll_dice(1, 5);

    dprf("breath timeout: %d", timeout);

    mon_enchant breath_timeout = mon_enchant(ENCH_BREATH_WEAPON, 1, mons,
                                             timeout * BASELINE_DELAY);
    mons->add_ench(breath_timeout);
}

/**
 * Maybe mesmerise the player.
 *
 * This function decides whether or not it is possible for the player to become
 * mesmerised by mons. It will return a variety of values depending on whether
 * or not this can succeed or has succeeded; finally, it will add mons to the
 * player's list of beholders.
 *
 * @param mons      The monster doing the mesmerisation.
 * @param actual    Whether or not we are actually casting the spell. If false,
 *                  no messages are emitted.
 * @return          0 if the player could be mesmerised but wasn't, 1 if the
 *                  player was mesmerised, -1 if the player couldn't be
 *                  mesmerised.
**/
static int _mons_mesmerise(monster* mons, bool actual)
{
    ASSERT(mons); // XXX: change to monster &mons
    bool already_mesmerised = you.beheld_by(*mons);

    if (!you.visible_to(mons)             // Don't mesmerise while invisible.
        || (!you.can_see(*mons)           // Or if we are, and you're aren't
            && !already_mesmerised)       // already mesmerised by us.
        || !player_can_hear(mons->pos())  // Or if you're silenced, or we are.
        || you.berserk()                  // Or if you're berserk.
        || mons->has_ench(ENCH_CONFUSION) // Or we're confused,
        || mons_is_fleeing(*mons)          // fleeing,
        || mons->pacified()               // pacified,
        || mons->friendly())              // or friendly!
    {
        return -1;
    }

    if (actual)
    {
        if (!already_mesmerised)
        {
            simple_monster_message(*mons, " attempts to bespell you!");
            flash_view(UA_MONSTER, LIGHTMAGENTA);
        }
        else
        {
            mprf("%s draws you further into %s thrall.",
                    mons->name(DESC_THE).c_str(),
                    mons->pronoun(PRONOUN_POSSESSIVE).c_str());
        }
    }

    const int pow = _ench_power(SPELL_MESMERISE, *mons);
    const int will_check = you.check_willpower(mons, pow);

    // Don't mesmerise if you pass an WL check or have clarity.
    // If you're already mesmerised, you cannot resist further.
    if ((will_check > 0 || you.clarity()
         || you.duration[DUR_MESMERISE_IMMUNE]) && !already_mesmerised)
    {
        if (actual)
        {
            if (you.clarity())
                canned_msg(MSG_YOU_UNAFFECTED);
            else if (you.duration[DUR_MESMERISE_IMMUNE] && !already_mesmerised)
                canned_msg(MSG_YOU_RESIST);
            else
                mprf("You%s", you.resist_margin_phrase(will_check).c_str());
        }

        return 0;
    }

    you.add_beholder(*mons);

    return 1;
}

static ai_action::goodness _should_irradiate(const monster &mons)
{
    // make allied monsters extra reluctant to irradiate in melee.
    if (mons.wont_attack() && adjacent(you.pos(), mons.pos()))
        return ai_action::bad();

    bolt tracer;
    tracer.flavour = BEAM_MMISSILE;
    tracer.range = 0;
    tracer.is_explosion = true;
    tracer.ex_size = 1;
    tracer.source = tracer.target = mons.pos();
    tracer.hit = AUTOMATIC_HIT;
    tracer.damage = dice_def(999, 1);
    fire_tracer(&mons, tracer, true, true);
    return mons_should_fire(tracer) ? ai_action::good() : ai_action::bad();
}

// Check whether targets might be scared.
// Returns 0, if targets can be scared but the attempt failed or wasn't made.
// Returns 1, if targets are scared.
// Returns -1, if targets can never be scared.
static int _mons_cause_fear(monster* mons, bool actual)
{
    if (actual)
    {
        if (you.can_see(*mons))
            simple_monster_message(*mons, " radiates an aura of fear!");
        else if (you.see_cell(mons->pos()))
            mpr("An aura of fear fills the air!");
    }

    int retval = -1;

    const int pow = _ench_power(SPELL_CAUSE_FEAR, *mons);

    if (mons->see_cell_no_trans(you.pos())
        && mons->can_see(you)
        && !mons->wont_attack()
        && !you.afraid_of(mons))
    {
        if (!you.can_feel_fear(false))
        {
            if (actual)
                canned_msg(MSG_YOU_UNAFFECTED);
        }
        else if (!actual)
            retval = 0;
        else
        {
            const int res_margin = you.check_willpower(mons, pow);
            if (!you.can_feel_fear(true))
                canned_msg(MSG_YOU_UNAFFECTED);
            else if (res_margin > 0)
                mprf("You%s", you.resist_margin_phrase(res_margin).c_str());
            else if (you.add_fearmonger(mons))
            {
                retval = 1;
                you.increase_duration(DUR_AFRAID, 10 + random2avg(pow / 10, 4));
            }
        }
    }

    for (monster_near_iterator mi(mons->pos(), LOS_NO_TRANS); mi; ++mi)
    {
        if (*mi == mons)
            continue;

        // Invulnerable-willed, unnatural and "firewood" monsters are
        // immune to being scared. Same-aligned monsters are
        // never affected, even though they aren't immune.
        // Will not further scare a monster that is already afraid.
        if (mons_invuln_will(**mi)
            || !(mi->holiness() & MH_NATURAL)
            || mi->is_firewood()
            || mons_aligned(*mi, mons)
            || mi->has_ench(ENCH_FEAR))
        {
            continue;
        }

        retval = max(retval, 0);

        if (!actual)
            continue;

        // It's possible to scare this monster. If its magic
        // resistance fails, do so.
        int res_margin = mi->check_willpower(mons, pow);
        if (res_margin > 0)
        {
            simple_monster_message(**mi,
                mi->resist_margin_phrase(res_margin).c_str());
            continue;
        }

        if (mi->add_ench(mon_enchant(ENCH_FEAR, 0, mons)))
        {
            retval = 1;

            if (you.can_see(**mi))
                simple_monster_message(**mi, " looks frightened!");

            behaviour_event(*mi, ME_SCARE, mons);
        }
    }

    if (actual && retval == 1 && you.see_cell(mons->pos()))
        flash_view_delay(UA_MONSTER, DARKGREY, 300);

    return retval;
}

static int _mons_mass_confuse(monster* mons, bool actual)
{
    int retval = -1;

    const int pow = _ench_power(SPELL_MASS_CONFUSION, *mons);

    if (mons->see_cell_no_trans(you.pos())
        && mons->can_see(you)
        && !mons->wont_attack())
    {
        retval = 0;

        if (actual)
        {
            const int willpower = you.check_willpower(mons, pow);
            if (willpower > 0)
                mprf("You%s", you.resist_margin_phrase(willpower).c_str());
            else
            {
                you.confuse(mons, 5 + random2(3));
                retval = 1;
            }
        }
    }

    for (monster_near_iterator mi(mons->pos(), LOS_NO_TRANS); mi; ++mi)
    {
        if (*mi == mons)
            continue;

        if (mons_invuln_will(**mi) || mi->is_firewood() || mons_aligned(*mi, mons))
            continue;

        retval = max(retval, 0);

        int res_margin = mi->check_willpower(mons, pow);
        if (res_margin > 0)
        {
            if (actual)
            {
                simple_monster_message(**mi,
                    mi->resist_margin_phrase(res_margin).c_str());
            }
            continue;
        }
        if (actual)
        {
            retval = 1;
            mi->confuse(mons, 5 + random2(3));
        }
    }

    return retval;
}

static coord_def _mons_fragment_target(const monster &mon)
{
    coord_def target(GXM+1, GYM+1);
    if (!in_bounds(mon.pos()))
        return target;
    const monster *mons = &mon; // TODO: rewriteme
    const int pow = mons_spellpower(*mons, SPELL_LRD);

    const int range = mons_spell_range(*mons, SPELL_LRD);
    int maxpower = 0;
    for (distance_iterator di(mons->pos(), true, true, range); di; ++di)
    {
        bool temp;

        if (!cell_see_cell(mons->pos(), *di, LOS_SOLID))
            continue;

        bolt beam;
        if (!setup_fragmentation_beam(beam, pow, mons, *di, true, nullptr,
                                      temp))
        {
            continue;
        }

        beam.range = range;
        fire_tracer(mons, beam, true);
        if (!mons_should_fire(beam))
            continue;

        if (beam.foe_info.count > 0
            && beam.foe_info.power > maxpower)
        {
            maxpower = beam.foe_info.power;
            target = *di;
        }
    }

    return target;
}

static void _blink_allies_encircle(const monster* mon)
{
    vector<monster*> allies;
    const coord_def foepos = mon->get_foe()->pos();

    for (monster_near_iterator mi(mon, LOS_NO_TRANS); mi; ++mi)
    {
        if (_valid_encircle_ally(mon, *mi, foepos))
            allies.push_back(*mi);
    }
    shuffle_array(allies);

    int count = random_range(3, 6);

    for (monster *ally : allies)
    {
        coord_def empty;
        if (!find_habitable_spot_near(foepos, mons_base_type(*ally), 1, empty))
            continue;
        if (!ally->blink_to(empty))
            continue;
        // XXX: This seems an awkward way to give a message for something
        // blinking from out of sight into sight. Probably could use a
        // more general solution.
        if (!(ally->flags & MF_WAS_IN_VIEW)
            && ally->flags & MF_SEEN)
        {
            simple_monster_message(*ally, " blinks into view!");
        }
        ally->behaviour = BEH_SEEK;
        ally->foe = mon->foe;
        ally->drain_action_energy();

        if (!--count)
            break;
    }
}

static void _blink_allies_away(const monster* mon)
{
    vector<monster*> allies;
    const coord_def foepos = mon->get_foe()->pos();

    for (monster_near_iterator mi(mon, LOS_NO_TRANS); mi; ++mi)
    {
        if (_valid_blink_away_ally(mon, *mi, foepos))
            allies.push_back(*mi);
    }
    shuffle_array(allies);

    int count = max(1, mon->spell_hd(SPELL_BLINK_ALLIES_AWAY) / 8
                       + random2(mon->spell_hd(SPELL_BLINK_ALLIES_AWAY) / 4));

    for (unsigned int i = 0; i < allies.size() && count; ++i)
    {
        if (blink_away(allies[i], &you, false))
            count--;
    }
}

struct branch_summon_pair
{
    branch_type     origin;
    const vector<pop_entry> pop;
};

static branch_summon_pair _invitation_summons[] =
{
  { BRANCH_SNAKE,
    { // Snake enemies
      {  1,   1,   80, FLAT, MONS_BLACK_MAMBA },
      {  1,   1,   40, FLAT, MONS_NAGA_WARRIOR },
      {  1,   1,   20, FLAT, MONS_MANA_VIPER },
    }},
  { BRANCH_SPIDER,
    { // Spider enemies
      {  1,   1,   60, FLAT, MONS_TARANTELLA },
      {  1,   1,   80, FLAT, MONS_WOLF_SPIDER },
      {  1,   1,   20, FLAT, MONS_ORB_SPIDER },
    }},
  { BRANCH_SWAMP,
    { // Swamp enemies
      {  1,   1,   80, FLAT, MONS_BOG_BODY },
      {  1,   1,   60, FLAT, MONS_SWAMP_DRAKE },
      {  1,   1,   40, FLAT, MONS_GOLIATH_FROG },
    }},
  { BRANCH_SHOALS,
    { // Shoals enemies
      {  1,   1,   60, FLAT, MONS_SNAPPING_TURTLE },
      {  1,   1,   40, FLAT, MONS_MANTICORE },
      {  1,   1,   20, FLAT, MONS_MERFOLK_AQUAMANCER },
    }},
  { BRANCH_ELF,
    { // Elf enemies
      {  1,   1,   50, FLAT, MONS_DEEP_ELF_AIR_MAGE },
      {  1,   1,   50, FLAT, MONS_DEEP_ELF_FIRE_MAGE },
      {  1,   1,   40, FLAT, MONS_DEEP_ELF_KNIGHT },
    }},
  { BRANCH_VAULTS,
    { // Vaults enemies
      {  1,   1,   60, FLAT, MONS_VERY_UGLY_THING },
      {  1,   1,   40, FLAT, MONS_IRONBOUND_FROSTHEART },
      {  1,   1,   20, FLAT, MONS_VAULT_SENTINEL },
    }},
  { BRANCH_CRYPT,
    { // Crypt enemies
      {  1,   1,   80, FLAT, MONS_FREEZING_WRAITH },
      {  1,   1,   40, FLAT, MONS_SKELETAL_WARRIOR },
      {  1,   1,   40, FLAT, MONS_NECROMANCER },
    }},
};

static branch_summon_pair _planerend_summons[] =
{
  { BRANCH_SLIME,
    { // Slime enemies
      {  1,   1,   80, FLAT, MONS_SLIME_CREATURE }, // changed to titanic below
      {  1,   1,  100, FLAT, MONS_ROCKSLIME },
      {  1,   1,  100, FLAT, MONS_ACID_BLOB },
    }},
  { BRANCH_ELF,
    { // Elf enemies
      {  1,   1,  100, FLAT, MONS_DEEP_ELF_SORCERER },
      {  1,   1,  100, FLAT, MONS_DEEP_ELF_HIGH_PRIEST },
      {  1,   1,   60, FLAT, MONS_DEEP_ELF_BLADEMASTER },
    }},
  { BRANCH_VAULTS,
    { // Vaults enemies
      {  1,   1,   80, FLAT, MONS_IRONBOUND_THUNDERHULK },
      {  1,   1,   40, FLAT, MONS_IRONBOUND_CONVOKER },
      {  1,   1,  100, FLAT, MONS_WAR_GARGOYLE },
    }},
  { BRANCH_CRYPT,
    { // Crypt enemies
      {  1,   1,  100, FLAT, MONS_VAMPIRE_KNIGHT },
      {  1,   1,  100, FLAT, MONS_FLAYED_GHOST },
      {  1,   1,   80, FLAT, MONS_REVENANT_SOULMONGER },
    }},
  { BRANCH_TOMB,
    { // Tomb enemies
      {  1,   1,   60, FLAT, MONS_ANCIENT_CHAMPION },
      {  1,   1,  100, FLAT, MONS_GUARDIAN_SPHINX },
      {  1,   1,  100, FLAT, MONS_MUMMY_PRIEST },
    }},
  { BRANCH_ABYSS,
    { // Abyss enemies
      {  1,   1,   80, FLAT, MONS_APOCALYPSE_CRAB },
      {  1,   1,  100, FLAT, MONS_STARCURSED_MASS },
      {  1,   1,   40, FLAT, MONS_TENTACLED_STARSPAWN },
    }},
  { BRANCH_ZOT,
    { // Zot enemies
      {  1,   1,   40, FLAT, MONS_DRACONIAN_STORMCALLER },
      {  1,   1,  100, FLAT, MONS_GOLDEN_DRAGON },
      {  1,   1,   80, FLAT, MONS_MOTH_OF_WRATH },
    }},
};

static void _branch_summon(monster &mons, mon_spell_slot slot, bolt&)
{
    _branch_summon_helper(&mons, slot.spell);
}

static void _branch_summon_helper(monster* mons, spell_type spell_cast)
{
    // TODO: rewrite me! (should use maps, vectors, const monster&...)
    branch_summon_pair *summon_list;
    size_t list_size;
    int which_branch;
    static const string INVITATION_KEY = "invitation_branch";

    switch (spell_cast)
    {
        case SPELL_FORCEFUL_INVITATION:
            summon_list = _invitation_summons;
            list_size = ARRAYSZ(_invitation_summons);
            if (!mons->props.exists(INVITATION_KEY))
                mons->props[INVITATION_KEY].get_byte() = random2(list_size);
            which_branch = mons->props[INVITATION_KEY].get_byte();
            break;
        case SPELL_PLANEREND:
            summon_list = _planerend_summons;
            list_size = ARRAYSZ(_planerend_summons);
            which_branch = random2(list_size);
            break;
        default:
            die("Unsupported branch summon spell %s!",
                 spell_title(spell_cast));
    }
    const int num_summons = random_range(1, 3);

    if (you.see_cell(mons->pos()))
    {
        string msg = getSpeakString("branch summon cast prefix");
        if (!msg.empty())
        {
            msg  = replace_all(msg, "@The_monster@", mons->name(DESC_THE));
            msg += " ";
            msg += branches[summon_list[which_branch].origin].longname;
            msg += "!";
            mprf(mons->wont_attack() ? MSGCH_FRIEND_ENCHANT
                                     : MSGCH_MONSTER_ENCHANT,
                 "%s", msg.c_str());
        }
    }

    for (int i = 0; i < num_summons; i++)
    {
        monster_type type = pick_monster_from(summon_list[which_branch].pop, 1);
        if (type == MONS_NO_MONSTER)
            continue;

        mgen_data mg(type, SAME_ATTITUDE(mons), mons->pos(), mons->foe);
        mg.set_summoned(mons, spell_cast, summ_dur(spell_cast == SPELL_PLANEREND ? 2 : 1));
        if (type == MONS_SLIME_CREATURE)
            mg.props[MGEN_BLOB_SIZE] = 5;
        create_monster(mg);
    }
}

static void _cast_marshlight(monster &mons, mon_spell_slot, bolt&)
{
    const int pow = mons_spellpower(mons, SPELL_MARSHLIGHT);
    cast_foxfire(mons, pow, false, true);
}

static void _cast_foxfire(monster &mons, mon_spell_slot, bolt&)
{
    const int pow = mons_spellpower(mons, SPELL_FOXFIRE);
    cast_foxfire(mons, pow, false, false);
}

void mons_cast_flay(monster &caster, mon_spell_slot, bolt&)
{
    actor* defender = caster.get_foe();
    ASSERT(defender);

    int damage_taken = 0;
    if (defender->is_player())
        damage_taken = max(0, you.hp * 25 / 100 - 1);
    else
    {
        monster* mon = defender->as_monster();
        damage_taken = max(0, mon->hit_points * 25 / 100 - 1);
    }

    _flay(caster, *defender, damage_taken);
}

/**
 * Attempt to flay the given target, dealing 'temporary' damage that heals when
 * a flayed ghost nearby dies.
 *
 * @param caster    The flayed ghost doing the flaying. (Mostly irrelevant.)
 * @param defender  The thing being flayed.
 * @param damage    How much flaying damage to do.
 */
static void _flay(const monster &caster, actor &defender, int damage)
{
    if (damage <= 0)
        return;

    bool was_flayed = false;

    if (defender.is_player())
    {
        if (you.duration[DUR_FLAYED])
            was_flayed = true;

        you.duration[DUR_FLAYED] = max(you.duration[DUR_FLAYED],
                                       55 + random2(66));
    }
    else
    {
        monster* mon = defender.as_monster();
        const int added_dur = 30 + random2(50);

        if (mon->has_ench(ENCH_FLAYED))
        {
            was_flayed = true;
            mon_enchant flayed = mon->get_ench(ENCH_FLAYED);
            flayed.duration = min(flayed.duration + added_dur, 150);
            mon->update_ench(flayed);
        }
        else
        {
            mon_enchant flayed(ENCH_FLAYED, 1, &caster, added_dur);
            mon->add_ench(flayed);
        }
    }

    if (you.can_see(defender))
    {
        if (was_flayed)
        {
            mprf("Terrible wounds spread across more of %s body!",
                 defender.name(DESC_ITS).c_str());
        }
        else
        {
            mprf("Terrible wounds open up all over %s body!",
                 defender.name(DESC_ITS).c_str());
        }
    }

    // Due to Deep Dwarf damage shaving, the player may take less than the intended
    // amount of damage. Keep track of the actual amount of damage done by comparing
    // hp before and after the player is hurt; use this as the actual value for
    // flay damage to prevent the player from regaining extra hp when it wears off

    const int orig_hp = defender.stat_hp();

    defender.hurt(&caster, damage, BEAM_NONE,
                  KILLED_BY_MONSTER, "", FLAY_DAMAGE_KEY, true);
    defender.props[FLAY_DAMAGE_KEY].get_int() += orig_hp - defender.stat_hp();

    vector<coord_def> old_blood;
    CrawlVector &new_blood = defender.props[FLAY_BLOOD_KEY].get_vector();

    // Find current blood spatters
    for (radius_iterator ri(defender.pos(), LOS_SOLID); ri; ++ri)
    {
        if (env.pgrid(*ri) & FPROP_BLOODY)
            old_blood.push_back(*ri);
    }

    blood_spray(defender.pos(), defender.type, 20);

    // Compute and store new blood spatters
    unsigned int i = 0;
    for (radius_iterator ri(defender.pos(), LOS_SOLID); ri; ++ri)
    {
        if (env.pgrid(*ri) & FPROP_BLOODY)
        {
            if (i < old_blood.size() && old_blood[i] == *ri)
                ++i;
            else
                new_blood.push_back(*ri);
        }
    }
}

/// What nonliving creatures are adjacent to the given location?
static vector<const actor*> _find_nearby_constructs(const monster &caster,
                                                    coord_def pos)
{
    vector<const actor*> nearby_constructs;
    for (adjacent_iterator ai(pos); ai; ++ai)
    {
        const actor* act = actor_at(*ai);
        if (act && act->holiness() & MH_NONLIVING && mons_aligned(&caster, act))
            nearby_constructs.push_back(act);
    }
    return nearby_constructs;
}

/// How many nonliving creatures are adjacent to the given location?
static int _count_nearby_constructs(const monster &caster, coord_def pos)
{
    return _find_nearby_constructs(caster, pos).size();
}

/// What's a good description of nonliving creatures adjacent to the given point?
static string _describe_nearby_constructs(const monster &caster, coord_def pos)
{
    const vector<const actor*> nearby_constructs
        = _find_nearby_constructs(caster, pos);
    if (!nearby_constructs.size())
        return "";

    const string name = nearby_constructs.back()->name(DESC_THE);
    if (nearby_constructs.size() == 1)
        return make_stringf(" and %s", name.c_str());

    for (auto act : nearby_constructs)
        if (act->name(DESC_THE) != name)
            return " and the adjacent constructs";
    return make_stringf(" and %s", pluralise_monster(name).c_str());
}

/// Cast Resonance Strike, blasting the caster's target with smitey damage.
static void _cast_resonance_strike(monster &caster, mon_spell_slot, bolt&)
{
    actor* target = caster.get_foe();
    if (!target)
        return;

    // base damage 3d(spell hd)
    const int pow = caster.spell_hd(SPELL_RESONANCE_STRIKE);
    dice_def dice = resonance_strike_base_damage(pow);
    // + 1 die for every 2 adjacent constructs, up to a total of 7 dice when
    // fully surrounded by 8 constructs
    const int constructs = _count_nearby_constructs(caster, target->pos());
    dice.num += div_rand_round(constructs, 2);
    const int dam = target->apply_ac(dice.roll());
    const string constructs_desc
        = _describe_nearby_constructs(caster, target->pos());

    if (you.see_cell(target->pos()))
    {
        mprf("A blast of power from the earth%s strikes %s!",
             constructs_desc.c_str(),
             target->name(DESC_THE).c_str());
    }
    target->hurt(&caster, dam, BEAM_MISSILE, KILLED_BY_BEAM,
                 "", "by a resonance strike");
    _whack(caster, *target);
}

static bool _spell_charged(monster *mons)
{
    mon_enchant ench = mons->get_ench(ENCH_SPELL_CHARGED);
    if (ench.ench == ENCH_NONE || ench.degree < max_mons_charge(mons->type))
    {
        if (ench.ench == ENCH_NONE)
        {
            mons->add_ench(mon_enchant(ENCH_SPELL_CHARGED, 1, mons,
                                       INFINITE_DURATION));
        }
        else
        {
            ench.degree++;
            mons->update_ench(ench);
        }

        if (!you.can_see(*mons))
            return false;
        string msg =
            getSpeakString(make_stringf("%s charge",
                                        mons->name(DESC_PLAIN, true).c_str())
                           .c_str());
        if (!msg.empty())
        {
            msg = do_mon_str_replacements(msg, *mons);
            mprf(mons->wont_attack() ? MSGCH_FRIEND_ENCHANT
                 : MSGCH_MONSTER_ENCHANT, "%s", msg.c_str());
        }
        return false;
    }
    mons->del_ench(ENCH_SPELL_CHARGED);
    return true;
}

/// How much damage does the given monster do when casting Waterstrike?
dice_def waterstrike_damage(int spell_hd)
{
    return dice_def(3, 7 + spell_hd);
}

/**
 * How much damage does the given monster do when casting Resonance Strike,
 * assuming no allied constructs are boosting damage?
 */
dice_def resonance_strike_base_damage(int spell_hd)
{
    return dice_def(3, spell_hd);
}

static const int MIN_DREAM_SUCCESS_POWER = 25;

static void _sheep_message(int num_sheep, int sleep_pow, bool seen, actor& foe)
{
    string message;

    // Determine messaging based on sleep strength.
    if (sleep_pow >= 125)
        message = "You are overwhelmed by glittering dream dust!";
    else if (sleep_pow >= 75)
        message = "The dream sheep are wreathed in dream dust.";
    else if (sleep_pow >= MIN_DREAM_SUCCESS_POWER)
    {
        message = make_stringf("The dream sheep shake%s wool and sparkle%s.",
                               num_sheep == 1 ? "s its" : " their",
                               num_sheep == 1 ? "s": "");
    }
    else // if sleep fails
    {
        message = make_stringf("The dream sheep ruffle%s wool and motes of "
                               "dream dust sparkle, to no effect.",
                               num_sheep == 1 ? "s its" : " their");
    }

    if (foe.is_player())
    {
        const char* drowsy = sleep_pow ? " You feel drowsy..." : "";
        if (!seen)
        {
            mprf(MSGCH_MONSTER_SPELL,
                 "Motes of dream dust float from an unseen source.%s",
                 drowsy);
            return;
        }
        mprf(MSGCH_MONSTER_SPELL, "%s%s", message.c_str(), drowsy);
        return;
    }

    if (!you.see_cell(foe.pos()))
        return;

    const string foe_name = foe.name(DESC_THE);
    const auto chan = foe.as_monster()->friendly() ? MSGCH_MONSTER_SPELL
                                                   : MSGCH_FRIEND_SPELL;
    if (!seen)
    {
        if (!sleep_pow)
        {
            mprf(chan, "Motes of dream dust float from an unseen source.");
            mprf("%s is unaffected.", foe_name.c_str());
            return;
        }

        mprf(chan,
             "As motes of dream dust float from an unseen source, %s falls asleep.",
             foe_name.c_str());
        return;
    }

    const char* pluralize = num_sheep == 1 ? "s": "";
    if (sleep_pow)
    {
        mprf(chan,
             "As the sheep sparkle%s and sway%s, %s falls asleep.",
             pluralize,
             pluralize,
             foe_name.c_str());
        return;
    }

    mprf(chan,
         "The dream sheep attempt%s to lull %s to sleep.",
         pluralize,
         foe_name.c_str());
    mprf("%s is unaffected.", foe_name.c_str());
}

static void _dream_sheep_sleep(monster& mons, actor& foe)
{
    // Shepherd the dream sheep.
    int num_sheep = 0;
    bool seen = false;
    for (monster_near_iterator mi(foe.pos(), LOS_NO_TRANS); mi; ++mi)
    {
        if (mi->type == MONS_DREAM_SHEEP)
        {
            num_sheep++;
            if (!seen && you.can_see(**mi))
                seen = true;
        }
    }

    // The correlation between amount of sheep and duration of
    // sleep is randomised, but bounds are 5 to 20 turns of sleep.
    // More dream sheep are both more likely to succeed and to have a
    // stronger effect. Too-weak attempts get blanked.
    // Special note: a single sheep has a 1 in 25 chance to succeed.
    int sleep_pow = min(150, random2(num_sheep * 25) + 1);
    if (sleep_pow < MIN_DREAM_SUCCESS_POWER)
        sleep_pow = 0;

    // Communicate with the player.
    _sheep_message(num_sheep, sleep_pow, seen, foe);

    // Put the player to sleep.
    if (sleep_pow)
    {
        const int dur = (sleep_pow / 20 + random_range(1, 3) + 3) * BASELINE_DELAY;
        foe.put_to_sleep(&mons, dur, false);
    }
}

// Draconian stormcaller upheaval. Simplified compared to the player version.
// Noisy! Causes terrain changes.
// Also used for Salamander Tyrants
// TODO: Could use further simplification.
static void _mons_upheaval(monster& mons, actor& /*foe*/, bool randomize)
{
    bolt beam;
    beam.source_id   = mons.mid;
    beam.source_name = mons.name(DESC_THE).c_str();
    beam.thrower     = KILL_MON_MISSILE;
    beam.range       = LOS_RADIUS;
    beam.damage      = dice_def(3, 24);
    beam.foe_ratio   = random_range(20, 30);
    beam.hit         = AUTOMATIC_HIT;
    beam.glyph       = dchar_glyph(DCHAR_EXPLOSION);
    beam.loudness    = 10;
    beam.draw_delay  = 0;
    beam.target = mons.target;
    string message = "";

    switch (randomize ? random2(4) : 0)
    {
        case 0:
            beam.name      = "blast of magma";
            beam.flavour   = BEAM_LAVA;
            beam.colour    = RED;
            beam.tile_beam = TILE_BOLT_MAGMA;
            beam.hit_verb  = "engulfs";
            message        = "Magma suddenly erupts from the ground!";
            break;
        case 1:
            beam.name      = "blast of ice";
            beam.flavour   = BEAM_ICE;
            beam.colour    = WHITE;
            beam.tile_beam = TILE_BOLT_ICEBLAST;
            message        = "A blizzard blasts the area with ice!";
            break;
        case 2:
            beam.name      = "cutting wind";
            beam.flavour   = BEAM_AIR;
            beam.colour    = LIGHTGRAY;
            beam.tile_beam = TILE_BOLT_STRONG_AIR;
            message        = "A storm cloud blasts the area with cutting wind!";
            break;
        case 3:
            beam.name    = "blast of rubble";
            beam.flavour = BEAM_FRAG;
            beam.colour  = BROWN;
            message      = "The ground shakes violently, spewing rubble!";
            break;
        default:
            break;
    }

    vector<coord_def> affected;
    affected.push_back(beam.target);

    for (adjacent_iterator ai(beam.target); ai; ++ai)
    {
        if (in_bounds(*ai) && !cell_is_solid(*ai)
            // Proper Upheaval can't hit the caster
            && (*ai != mons.pos() || !randomize))
        {
            affected.push_back(*ai);
        }
    }

    shuffle_array(affected);

    if (Options.use_animations & UA_MONSTER)
    {
        for (coord_def pos : affected)
        {
            beam.draw(pos);
            // No need to delay if we are not refreshing
            if (!Options.reduce_animations)
                scaled_delay(25); // should this refresh?
        }

        if (Options.reduce_animations)
            animation_delay(25, true);
    }

    bool made_lava = false;
    for (coord_def pos : affected)
    {
        beam.source = pos;
        beam.target = pos;
        beam.fire();

        switch (beam.flavour)
        {
            case BEAM_LAVA:
                if (env.grid(pos) == DNGN_FLOOR && !actor_at(pos)
                    && (!made_lava || coinflip()))
                {
                    temp_change_terrain(
                        pos, DNGN_LAVA,
                        random2(14) * BASELINE_DELAY,
                        TERRAIN_CHANGE_FLOOD);

                    made_lava = true;
                }
                break;
            case BEAM_AIR:
                if (!cell_is_solid(pos) && !cloud_at(pos) && coinflip())
                    place_cloud(CLOUD_STORM, pos, random2(7), &mons);
                break;
            default:
                break;
        }
    }
}

static void _mons_vortex(monster *mons)
{
    if (you.can_see(*mons))
    {
        bool flying = mons->airborne();
        mprf("A freezing vortex appears %s%s%s!",
             flying ? "around " : "and lifts ",
             mons->name(DESC_THE).c_str(),
             flying ? "" : " up!");
    }
    else if (you.see_cell(mons->pos()))
        mpr("A freezing vortex appears out of thin air!");

    const int ench_dur = 60;

    mons->props[POLAR_VORTEX_KEY].get_int() = you.elapsed_time;
    mon_enchant me(ENCH_POLAR_VORTEX, 0, mons, ench_dur);
    mons->add_ench(me);

    if (mons->has_ench(ENCH_FLIGHT))
    {
        mon_enchant me2 = mons->get_ench(ENCH_FLIGHT);
        me2.duration = me.duration;
        mons->update_ench(me2);
    }
    else
        mons->add_ench(mon_enchant(ENCH_FLIGHT, 0, mons, ench_dur));
}

static bool _cast_dirge(monster& caster, bool check_only = false)
{
    const actor *foe = caster.get_foe();

    // Only use if the chanter has a visible enemy
    if (!foe
        || foe->is_player() && caster.friendly()
        || !caster.see_cell_no_trans(foe->pos()))
    {
        return false;
    }

    // Don't try to make noise when silent.
    if (caster.is_silenced())
        return false;

    vector<monster* > targs;
    for (monster_near_iterator mi(caster.pos(), LOS_NO_TRANS); mi; ++mi)
    {
        monster *mons = *mi;

        // Exclude various invalid targets
        if (mons == &caster
            || mons->holiness() != MH_UNDEAD
            || !mons_aligned(&caster, mons)
            || mons->confused()
            || mons->cannot_act())
        {
            continue;
        }

        if (mons->has_ench(ENCH_MIGHT) && mons->has_ench(ENCH_SWIFT))
            continue;

        // We found at least one valid target; this is good enough.
        if (check_only)
            return true;

        targs.push_back(mons);
    }

    if (targs.size() == 0)
        return false;

    // Randomize targets slightly, prioritizing allies close to one's foe
    shuffle_array(targs);
    near_to_far_sorter sorter = {foe->pos()};
    sort(targs.begin(), targs.end(), sorter);

    int num_affected = min((int)targs.size(), random_range(2, 4));
    for (int i = 0; i < num_affected; ++i)
    {
        monster* mons = targs[i];

        // Both the might and the swiftness have the same duration.
        int buffRange = random_range(3, 5);
        mons->add_ench(mon_enchant(ENCH_MIGHT, 1, &caster, buffRange
                                                           * BASELINE_DELAY));
        mons->add_ench(mon_enchant(ENCH_SWIFT, 1, &caster, buffRange
                                                           * BASELINE_DELAY));

        if (mons->asleep())
            behaviour_event(mons, ME_DISTURB, 0, caster.pos());

        if (you.can_see(*mons))
            simple_monster_message(*mons, " is empowered.");
    }

    return true;
}

// Some monsters wear armour but shouldn't wield weapons, so we can't just
// check equipment status: check their attack types too.
static bool _bestow_arms_attack_types_valid(monster* mon)
{
    bool valid = false;

    for (int i = 0; i < MAX_NUM_ATTACKS; ++i)
    {
        const mon_attack_def attk = mons_attack_spec(*mon, 0);
        if (attk.type == AT_HIT || attk.type == AT_WEAP_ONLY)
            valid = true;
    }

    return valid;
}

static void _cast_bestow_arms(monster& caster)
{
    vector<monster*> targs;

    // Compile list of eligable targets (and check if there are viable targets
    // for two-handers and ranged weapons while we're at it)
    bool two_hand_eligable = false;
    bool ranged_eligable = false;
    for (monster_near_iterator mi(caster.pos(), LOS_NO_TRANS); mi; ++mi)
    {
        if (mons_aligned(&caster, *mi) && !mi->has_ench(ENCH_ARMED)
            && (mons_itemuse(**mi) >= MONUSE_STARTING_EQUIPMENT)
            && _bestow_arms_attack_types_valid(*mi))
        {
            // Skip enemies with unrandarts, which are special enough to not
            // replace.
            if (mi->weapon() && is_unrandom_artefact(*mi->weapon()))
                continue;

            targs.push_back(*mi);

            if (!mi->shield())
                two_hand_eligable = true;

            // Attempting to give a ranged weapon to a dual-wielder is strange
            // and somewhat buggy, so let's not.
            if (!mons_wields_two_weapons(**mi))
                ranged_eligable = true;
        }
    }

    // Pick a weapon to give everyone
    item_def wpn;
    wpn.base_type = OBJ_WEAPONS;
    wpn.sub_type = random_choose_weighted(
                        two_hand_eligable ? 14 : 0, WPN_BARDICHE,
                        two_hand_eligable ? 14 : 0, WPN_GLAIVE,
                        11, WPN_DEMON_TRIDENT,
                        15, WPN_EVENINGSTAR,
                        15, WPN_DOUBLE_SWORD,
                        6, WPN_QUICK_BLADE,
                        ranged_eligable && two_hand_eligable ? 12 : 0, WPN_LONGBOW,
                        ranged_eligable && two_hand_eligable ? 10 : 0, WPN_TRIPLE_CROSSBOW,
                        ranged_eligable ? 8 : 0, WPN_HAND_CANNON);

    wpn.brand = random_choose_weighted(11, SPWPN_FLAMING,
                                       11, SPWPN_FREEZING,
                                       7,  SPWPN_SPEED,
                                       7,  SPWPN_VAMPIRISM,
                                       4,  SPWPN_CHAOS,
                                       1,  SPWPN_DISTORTION);

    wpn.plus = random_range(4, 9);
    wpn.flags |= (ISFLAG_SUMMONED | ISFLAG_IDENTIFIED);
    wpn.quantity = 1;

    if (you.can_see(caster))
    {
        mprf(MSGCH_MONSTER_SPELL, "%s arms its allies with %s.", caster.name(DESC_THE).c_str(),
                                    pluralise(wpn.name(DESC_PLAIN, false, true)).c_str());
    }

    int dur = random_range(20, 37) * BASELINE_DELAY;

    shuffle_array(targs);
    int count = random_range(5, 7);

    for (monster* mon : targs)
    {
        // Skip if this monster is not a valid recipient of this weapon
        if (mon->hands_reqd(wpn) == HANDS_TWO && mon->shield())
            continue;
        if (mons_wields_two_weapons(*mon) && is_ranged_weapon_type(wpn.sub_type))
            continue;

        item_def clone = wpn;

        // Save their previous weapons, if they were using any
        if (mon->inv[MSLOT_WEAPON] != NON_ITEM)
        {
            mon->props[OLD_ARMS_KEY] = env.item[mon->inv[MSLOT_WEAPON]];
            destroy_item(mon->inv[MSLOT_WEAPON]);
        }
        if (mon->inv[MSLOT_ALT_WEAPON] != NON_ITEM)
        {
            mon->props[OLD_ARMS_ALT_KEY] = env.item[mon->inv[MSLOT_ALT_WEAPON]];
            destroy_item(mon->inv[MSLOT_ALT_WEAPON]);
        }

        give_specific_item(mon, clone);
        mon->add_ench(mon_enchant(ENCH_ARMED, 1, &caster, dur));

        if (you.see_cell(mon->pos()))
            flash_tile(mon->pos(), MAGENTA, 50);

        if (--count == 0)
            break;
    }

    // Set cooldown (to prevent bestowing different types of arms to different
    // subsets of visible allies).
    caster.props[ARMS_COOLDOWN_KEY] = you.elapsed_time + dur;
}

static bool _mons_cast_prisms(monster& caster, actor& foe, int pow, bool check_only)
{
    // Determine which pair of locations to summon prisms on. The spots must
    // both be within 2 tiles of our foe, and also inside our spell range.
    // (And ideally not adjacent!)

    vector<coord_def> pos;
    for (radius_iterator ri(foe.pos(), 2, C_SQUARE, LOS_NO_TRANS); ri; ++ri)
    {
        if (grid_distance(caster.pos(), *ri) <= spell_range(SPELL_FULMINANT_PRISM, pow)
            && !actor_at(*ri) && !feat_is_solid(env.grid(*ri)))
        {
            // If we're just looking for a valid position, we found one.
            if (check_only)
                return true;

            pos.push_back(*ri);
        }
    }

    // Didn't find any empty space to place it
    if (pos.empty())
        return false;

    // Place the first prism
    int index = random2(pos.size());
    cast_fulminating_prism(&caster, pow, pos[index], false);

    // Then look at all valid locations that are non-adjacent to the prism we
    // placed and pick one of those for the second prism
    vector<coord_def> pos2;
    for (coord_def p : pos)
    {
        if (grid_distance(p, pos[index]) > 1)
            pos2.push_back(p);
    }

    if (!pos2.empty())
        cast_fulminating_prism(&caster, pow, pos2[random2(pos2.size())], false);

    return true;
}

static bool _mons_cast_hellfire_mortar(monster& caster, actor& foe, int pow, bool check_only)
{
    // The general heuristic used here is:
    // Try aiming at all spaces in a 5x5 square around our foe's location, picking
    // the first one that creates a mortar path at least 3 tiles long before running
    // into any actor, and that at least 3 tiles along that path have a viable shot
    // at something with bolt of magma.
    //
    // (Iteration among those spaces is random, but prefering ones adjacent to the
    // foe first.)

    vector<coord_def> possible_targets;
    for (distance_iterator di(foe.pos(), true, true, 2); di; ++di)
    {
        if (in_bounds(*di) && cell_see_cell(foe.pos(), *di, LOS_NO_TRANS))
            possible_targets.push_back(*di);
    }

    coord_def found_target;
    for (size_t i = 0; i < possible_targets.size(); ++i)
    {
        bolt tracer;
        zappy(ZAP_HELLFIRE_MORTAR_DIG, pow, true, tracer);
        tracer.range = LOS_RADIUS;
        tracer.source = caster.pos();
        tracer.target = possible_targets[i];
        tracer.source_id = caster.mid;
        tracer.origin_spell = SPELL_HELLFIRE_MORTAR;
        tracer.is_tracer = true;
        tracer.fire();

        // Skip paths that are less than 3 tiles long (which generally requires
        // them to be 4 tiles long, since the last tile will be some obstruction)
        if (tracer.path_taken.size() < 4)
            continue;

        size_t len;
        for (len = 0; len < tracer.path_taken.size(); ++len)
        {
            if (actor_at(tracer.path_taken[len]))
                break;
        }

        // Skip paths that run into an obstruction before 3 tiles
        if (len < 3)
            continue;

        // Now that we've determined the path is long enough, let's see if we
        // can shoot at something useful on at least 3 spaces of this path
        int useful_count = 0;
        for (size_t j = 0; j < len; ++j)
        {
            if (!in_bounds(tracer.path_taken[j]))
                break;

            // These tracers are from the perspective a possible morter at these spots,
            // but since that doesn't exist yet, we assume that the caster is firing
            // them. Possibly this doesn't result in exact results, but mostly should
            // be close enough, I think.
            bolt magma_tracer = mons_spell_beam(&caster, SPELL_BOLT_OF_MAGMA, 100);
            magma_tracer.source = tracer.path_taken[j];
            magma_tracer.target = foe.pos();
            magma_tracer.source_id = caster.mid;
            magma_tracer.attitude = mons_attitude(caster);
            magma_tracer.is_tracer = true;
            magma_tracer.foe_ratio = 100;
            magma_tracer.fire();

            if (mons_should_fire(magma_tracer))
                useful_count++;

            // This path has enough useful shots to take!
            if (useful_count == 3)
            {
                found_target = possible_targets[i];
                break;
            }
        }

        // If this target was found to be valid, stop looking at other ones.
        if (!found_target.origin())
            break;
    }

    // If we didn't find anything, stop.
    if (found_target.origin())
        return false;

    // If this is just a tracer, we know we've found a valid target.
    if (check_only)
        return true;

    // Actually cast it!
    bolt beam = mons_spell_beam(&caster, SPELL_HELLFIRE_MORTAR, pow);
    beam.target = found_target;
    cast_hellfire_mortar(caster, beam, pow, false);

    return true;
}

static bool _cast_dominate_undead(const monster& caster, int pow, bool check_only)
{
    vector<actor*> targs;
    for (actor_near_iterator ai(caster.pos()); ai; ++ai)
    {
        if (mons_aligned(&caster, *ai) || !(ai->holiness() & MH_UNDEAD)
            || ai->willpower() == WILL_INVULN)
        {
            continue;
        }

        // Not friendly, but already 'charmed'
        if (ai->is_player() && you.duration[DUR_VEXED])
            continue;

        // Found at least one valid target
        if (check_only)
            return true;

        targs.push_back(*ai);
    }

    if (check_only && targs.empty())
        return false;

    for (actor* targ : targs)
    {
        if (targ->is_monster())
        {
            monster* mon = targ->as_monster();
            const int res_margin = mon->check_willpower(&caster, pow / ENCH_POW_FACTOR);
            if (res_margin > 0)
            {
                simple_monster_message(*mon,
                    mon->resist_margin_phrase(res_margin).c_str());
                continue;
            }

            simple_monster_message(*mon, " is compelled to serve!");
            mon->add_ench(mon_enchant(ENCH_HEXED, 0, &caster));
            flash_tile(mon->pos(), BLUE);
        }
        else if (targ->is_player())
        {
            const int res_margin = you.check_willpower(&caster, pow / ENCH_POW_FACTOR);
            if (res_margin > 0)
                canned_msg(MSG_YOU_RESIST);
            else
            {
                const string msg = make_stringf("lash out against %s attempt to control you!",
                                                caster.name(DESC_ITS).c_str());
                you.vex(&caster, random_range(3, 5), caster.name(DESC_A).c_str(), msg);
            }
        }
    }

    return true;
}

/**
 *  Make this monster cast a spell
 *
 *  @param mons       The monster casting
 *  @param pbolt      The beam, possibly containing pre-done setup, to use
 *                    for the spell. Not a reference because this function
                      shouldn't affect the original copy.
 *  @param spell_cast The spell to be cast.
 *  @param slot_flags The spell slot flags in mons->spells (is it an
 *                    invocation, natural, shouty, etc.?)
 *  @param do_noise   Whether to make noise (including casting messages).
 */
void mons_cast(monster* mons, bolt pbolt, spell_type spell_cast,
               mon_spell_slot_flags slot_flags, bool do_noise)
{
    // check sputtercast state for e.g. orb spiders. assumption: all
    // sputtercasting monsters have one charge status and use it for all of
    // their spells.
    if (max_mons_charge(mons->type) > 0 && !_spell_charged(mons))
        return;

    if (spell_is_soh_breath(spell_cast))
    {
        const vector<spell_type> *breaths = soh_breath_spells(spell_cast);
        ASSERT(breaths);
        ASSERT(mons->heads() == (int)breaths->size());

        for (spell_type head_spell : *breaths)
        {
            if (!mons->get_foe())
                return;
            setup_mons_cast(mons, pbolt, head_spell);
            mons_cast(mons, pbolt, head_spell, slot_flags, do_noise);
        }

        return;
    }

    if (spell_cast == SPELL_LEGENDARY_DESTRUCTION)
    {
        if (do_noise)
        {
            mons_cast_noise(mons, pbolt, SPELL_LEGENDARY_DESTRUCTION,
                            slot_flags);
        }

        setup_mons_cast(mons, pbolt, SPELL_LEGENDARY_DESTRUCTION);
        mons_cast(mons, pbolt, _legendary_destruction_spell(), slot_flags,
                  false);
        if (!mons->get_foe())
            return;
        setup_mons_cast(mons, pbolt, SPELL_LEGENDARY_DESTRUCTION);
        mons_cast(mons, pbolt, _legendary_destruction_spell(), slot_flags,
                  false);
        return;
    }

    // Maybe cast abjuration instead of certain summoning spells.
    if (mons->can_see(you) &&
        get_spell_flags(spell_cast) & spflag::mons_abjure && one_chance_in(3)
        && ai_action::is_viable(_mons_will_abjure(*mons)))
    {
        mons_cast(mons, pbolt, SPELL_ABJURATION, slot_flags, do_noise);
        return;
    }

    bool evoke {slot_flags & MON_SPELL_EVOKE};
    // Always do setup. It might be done already, but it doesn't hurt
    // to do it again (cheap).
    setup_mons_cast(mons, pbolt, spell_cast, evoke);

    // single calculation permissible {dlb}
    const spell_flags flags = get_spell_flags(spell_cast);
    actor* const foe = mons->get_foe();
    const mons_spell_logic* logic = _get_spell_logic(*mons, spell_cast);
    const mon_spell_slot slot = {spell_cast, 0, slot_flags};

    int sumcount = 0;
    int sumcount2;
    int duration = 0;

    dprf("Mon #%d casts %s (#%d)",
         mons->mindex(), spell_title(spell_cast), spell_cast);
    ASSERT(!(flags & spflag::testing));
    // Targeted spells need a valid target.
    // Wizard-mode cast monster spells may target the boundary (shift-dir).
    ASSERT(map_bounds(pbolt.target) || !(flags & spflag::targeting_mask));

    if (spell_cast == SPELL_PORTAL_PROJECTILE
        || logic && (logic->flags & MSPELL_NO_AUTO_NOISE))
    {
        do_noise = false;       // Spell itself does the messaging.
    }

    if (do_noise)
        mons_cast_noise(mons, pbolt, spell_cast, slot_flags);

    if (logic && logic->cast)
    {
        logic->cast(*mons, slot, pbolt);
        return;
    }

    const god_type god = _find_god(*mons, slot_flags);
    const int splpow = evoke ? 30 + mons->get_hit_dice()
                             : mons_spellpower(*mons, spell_cast);

    switch (spell_cast)
    {
    default:
        break;

    case SPELL_WATERSTRIKE:
    {
        pbolt.flavour    = BEAM_WATER;

        ASSERT(foe);
        int damage_taken = waterstrike_damage(mons->spell_hd(spell_cast)).roll();
        damage_taken = foe->beam_resists(pbolt, damage_taken, false);
        damage_taken = foe->apply_ac(damage_taken);

        if (you.can_see(*foe))
        {
                mprf("The water %s and strikes %s%s",
                        foe->airborne() ? "rises up" : "swirls",
                        foe->name(DESC_THE).c_str(),
                        attack_strength_punctuation(damage_taken).c_str());
        }

        foe->hurt(mons, damage_taken, BEAM_MISSILE, KILLED_BY_BEAM,
                      "", "by the raging water");
        _whack(*mons, *foe);
        return;
    }

    case SPELL_AIRSTRIKE:
    {
        pbolt.flavour = BEAM_AIR;

        int empty_space = 0;
        ASSERT(foe);
        for (adjacent_iterator ai(foe->pos()); ai; ++ai)
            if (!monster_at(*ai) && !cell_is_solid(*ai))
                empty_space++;

        int damage_taken = empty_space * 2
                         + random2avg(2 + div_rand_round(splpow, 7), 2);
        damage_taken = foe->beam_resists(pbolt, damage_taken, false);

        damage_taken = foe->apply_ac(damage_taken);

        if (you.can_see(*foe))
        {
            tileidx_t tile = TILE_BOLT_DEFAULT_WHITE;

            mprf("%s and strikes %s%s",
                 airstrike_intensity_display(empty_space, tile).c_str(),
                 foe->name(DESC_THE).c_str(),
                 attack_strength_punctuation(damage_taken).c_str());

            flash_tile(foe->pos(), WHITE, 60, tile);
        }

        foe->hurt(mons, damage_taken, BEAM_MISSILE, KILLED_BY_BEAM,
                  "", "by the air");
        _whack(*mons, *foe);
        return;
    }

    case SPELL_HOLY_FLAMES:
        holy_flames(mons, foe);
        return;

    case SPELL_VANQUISHED_VANGUARD:
        // Wizard mode creates a dummy friendly monster, with no foe.
        if (!foe)
            return;
        if (foe->is_player())
            mpr("The long-dead rise up around you.");
        else if (you.can_see(*foe))
            mprf("The long-dead rise up around %s.", foe->name(DESC_THE).c_str());
        _cast_vanquished_vanguard(mons);
        return;

    case SPELL_SUMMON_MORTAL_CHAMPION:
        if (!foe)
            return;
        _cast_mortal_champion(mons);
        return;

    case SPELL_HAUNT:
        ASSERT(foe);
        if (foe->is_player())
            mpr("You feel haunted.");
        else
            mpr("You sense an evil presence.");
        _mons_cast_haunt(mons);
        return;

    case SPELL_MARTYRS_KNELL:
        cast_martyrs_knell(mons, splpow, false);
        return;

    // SPELL_SLEEP_GAZE ;)
    case SPELL_DREAM_DUST:
        _dream_sheep_sleep(*mons, *foe);
        return;

    case SPELL_CONFUSION_GAZE:
    {
        ASSERT(foe);
        const int res_margin = foe->check_willpower(mons, splpow / ENCH_POW_FACTOR);
        if (res_margin > 0)
        {
            if (you.can_see(*foe))
            {
                mprf("%s%s",
                     foe->name(DESC_THE).c_str(),
                     foe->resist_margin_phrase(res_margin).c_str());
            }
            return;
        }

        foe->confuse(mons, 5 + random2(3));
        return;
    }

    case SPELL_WOODWEAL:
        if (mons->heal(35 + random2(mons->spell_hd(spell_cast) * 3)))
            simple_monster_message(*mons, " is healed.");
        return;

    case SPELL_MAJOR_HEALING:
        if (mons->heal(50 + random2avg(mons->spell_hd(spell_cast) * 10, 2)))
            simple_monster_message(*mons, " is healed.");
        return;

    case SPELL_BERSERKER_RAGE:
        mons->props.erase(BROTHERS_KEY);
        mons->go_berserk(true);
        return;

#if TAG_MAJOR_VERSION == 34
    // Replaced with monster-specific version.
    case SPELL_SWIFTNESS:
#endif
    case SPELL_SPRINT:
        mons->add_ench(ENCH_SWIFT);
        simple_monster_message(*mons, " puts on a burst of speed!");
        return;

    case SPELL_SILENCE:
        mons->add_ench(ENCH_SILENCE);
        invalidate_agrid(true);
        simple_monster_message(*mons, " surroundings become eerily quiet.", true);
        return;

    case SPELL_CALL_TIDE:
        if (player_in_branch(BRANCH_SHOALS))
        {
            const int tide_duration = BASELINE_DELAY
                * random_range(80, 200, 2);
            mons->add_ench(mon_enchant(ENCH_TIDE, 0, mons,
                                       tide_duration));
            mons->props[TIDE_CALL_TURN].get_int() = you.num_turns;
            if (simple_monster_message(*
                    mons,
                    " sings a water chant to call the tide!"))
            {
                flash_view_delay(UA_MONSTER, ETC_WATER, 300);
            }
        }
        return;

    case SPELL_INK_CLOUD:
        if (!feat_is_water(env.grid(mons->pos())))
            return;

        big_cloud(CLOUD_INK, mons, mons->pos(), 30, 30);

        simple_monster_message(*
            mons,
            " squirts a massive cloud of ink into the water!");
        return;

    case SPELL_SUMMON_SMALL_MAMMAL:
        sumcount2 = 1 + random2(3);

        for (sumcount = 0; sumcount < sumcount2; ++sumcount)
        {
            monster_type rats[] = { MONS_QUOKKA, MONS_RIVER_RAT, MONS_RAT };

            const monster_type mon = (one_chance_in(3) ? MONS_BAT
                                                       : RANDOM_ELEMENT(rats));
            create_monster(
                mgen_data(mon, SAME_ATTITUDE(mons), mons->pos(), mons->foe,
                          MG_NONE, god)
                .set_summoned(mons, spell_cast, summ_dur(5)));
        }
        return;

    case SPELL_SHADOW_CREATURES:       // summon anything appropriate for level
    {
        level_id place = level_id::current();

        sumcount2 = 1 + random2(mons->spell_hd(spell_cast) / 5 + 1);

        for (sumcount = 0; sumcount < sumcount2; ++sumcount)
        {
            create_monster(
                mgen_data(RANDOM_MOBILE_MONSTER, SAME_ATTITUDE(mons),
                          mons->pos(), mons->foe, MG_NONE, god)
                          .set_summoned(mons, spell_cast, summ_dur(5))
                          .set_place(place));
        }
        return;
    }

    case SPELL_SUMMON_ILLUSION:
        _mons_cast_summon_illusion(mons, spell_cast);
        return;

    case SPELL_CREATE_TENTACLES:
        mons_create_tentacles(mons);
        return;

    case SPELL_FAKE_MARA_SUMMON:
    {
        // We only want there to be two fakes, which, plus Mara, means
        // a total of three Maras; if we already have two, give up, otherwise
        // we want to summon either one more or two more.
        const int n_wanted = 2 - count_summons(mons, SPELL_FAKE_MARA_SUMMON);
        if (n_wanted <= 0)
            return;

        for (int i = 0; i < n_wanted; i++)
            cast_phantom_mirror(mons, mons, 50, SPELL_FAKE_MARA_SUMMON, i);

        if (you.can_see(*mons))
        {
            mprf("%s shimmers and seems to become %s!",
                 mons->name(DESC_THE).c_str(),
                 number_in_words(n_wanted + 1).c_str());
        }
    }

        return;

    case SPELL_SUMMON_DEMON: // class 3-4 demons
        // if you change this, please update art-func.h:_DEMON_AXE_melee_effects
        sumcount2 = 1 + random2(mons->spell_hd(spell_cast) / 10 + 1);

        duration  = min(2 + mons->spell_hd(spell_cast) / 10, 6);
        for (sumcount = 0; sumcount < sumcount2; sumcount++)
        {
            create_monster(
                mgen_data(summon_any_demon(RANDOM_DEMON_COMMON, true),
                          SAME_ATTITUDE(mons), mons->pos(), mons->foe, MG_NONE, god)
                .set_summoned(mons, spell_cast, summ_dur(duration)));
        }
        return;

    case SPELL_MONSTROUS_MENAGERIE:
        cast_monstrous_menagerie(mons, splpow, mons->god);
        return;

    case SPELL_SUMMON_SPIDERS:
        summon_spiders(*mons, splpow, mons->god);
        return;

    case SPELL_CALL_IMP:
        duration  = min(2 + mons->spell_hd(spell_cast) / 5, 6);
        create_monster(
            mgen_data(MONS_CERULEAN_IMP,
                      SAME_ATTITUDE(mons), mons->pos(), mons->foe, MG_NONE, god)
            .set_summoned(mons, spell_cast, summ_dur(duration)));
        return;

    case SPELL_SUMMON_MINOR_DEMON: // class 5 demons
        sumcount2 = 1 + random2(3);

        duration  = min(2 + mons->spell_hd(spell_cast) / 5, 6);
        for (sumcount = 0; sumcount < sumcount2; ++sumcount)
        {
            create_monster(
                mgen_data(summon_any_demon(RANDOM_DEMON_LESSER, true),
                          SAME_ATTITUDE(mons), mons->pos(), mons->foe, MG_NONE, god)
                .set_summoned(mons, spell_cast, summ_dur(duration)));
        }
        return;

    case SPELL_SUMMON_UFETUBUS:
        sumcount2 = 2 + random2(2);

        duration  = min(2 + mons->spell_hd(spell_cast) / 5, 6);

        for (sumcount = 0; sumcount < sumcount2; ++sumcount)
        {
            create_monster(
                mgen_data(MONS_UFETUBUS, SAME_ATTITUDE(mons), mons->pos(),
                          mons->foe, MG_NONE, god)
                .set_summoned(mons, spell_cast, summ_dur(duration)));
        }
        return;

    case SPELL_SUMMON_SIN_BEAST:  // Geryon
        create_monster(
            mgen_data(MONS_SIN_BEAST, SAME_ATTITUDE(mons), mons->pos(),
                      mons->foe, MG_NONE, god).set_summoned(mons, spell_cast, summ_dur(3)));
        return;

    case SPELL_SUMMON_ICE_BEAST:
        _summon(*mons, MONS_ICE_BEAST, summ_dur(5), slot);
        return;

    case SPELL_SUMMON_MUSHROOMS:   // Summon a ring of icky crawling fungi.
        // Wizard mode creates a dummy friendly monster, with no foe.
        if (!foe)
            return;
        sumcount2 = 2 + random2(mons->spell_hd(spell_cast) / 4 + 1);
        duration  = min(2 + mons->spell_hd(spell_cast) / 5, 6);
        for (int i = 0; i < sumcount2; ++i)
        {
            create_monster(
                mgen_data(one_chance_in(3) ? MONS_DEATHCAP
                                           : MONS_WANDERING_MUSHROOM,
                          SAME_ATTITUDE(mons), foe->pos(), mons->foe, MG_FORCE_PLACE,
                          god).set_summoned(mons, spell_cast, summ_dur(duration))
                              .set_range(1, 2));
        }
        return;

    case SPELL_SUMMON_HORRIBLE_THINGS:
        _do_high_level_summon(mons, spell_cast, _pick_horrible_thing,
                              random_range(3, 5), god);
        return;

    case SPELL_MALIGN_GATEWAY:
        if (!can_cast_malign_gateway())
        {
            dprf("ERROR: %s can't cast malign gateway, but is casting anyway! "
                 "Counted %d gateways.", mons->name(DESC_THE).c_str(),
                 count_malign_gateways());
        }
        cast_malign_gateway(mons, 200);
        return;

    case SPELL_CONJURE_BALL_LIGHTNING:
    {
        const int hd = mons->spell_hd(spell_cast);
        const int pow = mons_power_for_hd(spell_cast, hd);
        const int n = 3;
        for (int i = 0; i < n; ++i)
        {
            mgen_data mg = mgen_data(MONS_BALL_LIGHTNING, SAME_ATTITUDE(mons),
                                     mons->pos(), mons->foe, MG_NONE, god)
                            .set_summoned(mons, spell_cast, random_range(40, 70), false, false);
            mg.hd = mons_ball_lightning_hd(pow);
            create_monster(mg);
        }
        return;
    }

    case SPELL_CONJURE_LIVING_SPELLS:
    {
        const int hd = mons->spell_hd(spell_cast);
        const spell_type spell = living_spell_type_for(mons->type);
        const int n = living_spell_count(spell, true);
        // XXX: will crash if wizmode player tries to cast?
        ASSERT(spell != SPELL_NO_SPELL);
        for (int i = 0; i < n; ++i)
        {
            mgen_data mg = mgen_data(MONS_LIVING_SPELL, SAME_ATTITUDE(mons),
                                     mons->pos(), mons->foe, MG_NONE, god)
                            .set_summoned(mons, spell_cast, summ_dur(1));
            mg.props[CUSTOM_SPELL_LIST_KEY].get_vector().push_back(spell);
            mg.hd = hd;
            monster* living = create_monster(mg);

           if (living)
           {
#ifdef USE_TILE
                if (spell == SPELL_LEHUDIBS_CRYSTAL_SPEAR)
                    living->props[MONSTER_TILE_KEY] = TILEP_MONS_LIVING_SPELL_CRYSTAL;
                else if (spell == SPELL_PETRIFY)
                    living->props[MONSTER_TILE_KEY] = TILEP_MONS_LIVING_SPELL_EARTH;
                else if (spell == SPELL_SMITING)
                    living->props[MONSTER_TILE_KEY] = TILEP_MONS_LIVING_SPELL_HOLY;
                else if (spell == SPELL_ICEBLAST)
                    living->props[MONSTER_TILE_KEY] = TILEP_MONS_LIVING_SPELL_ICE;
#endif
           }
        }
        return;
    }

    case SPELL_SUMMON_UNDEAD:
        _do_high_level_summon(mons, spell_cast, _pick_undead_summon,
                              2 + random2(mons->spell_hd(spell_cast) / 5 + 1),
                              god);
        return;

    case SPELL_BROTHERS_IN_ARMS:
    {
        // Invocation; don't use spell_hd
        int power = (mons->get_hit_dice() * 20)
                          + random2(mons->get_hit_dice() * 5);
        power -= random2(mons->get_hit_dice() * 5); // force a sequence point
        monster_type to_summon;

        if (mons->type == MONS_SPRIGGAN_BERSERKER)
        {
            monster_type berserkers[] = { MONS_POLAR_BEAR, MONS_ELEPHANT,
                                          MONS_DEATH_YAK };
            to_summon = RANDOM_ELEMENT(berserkers);
        }
        else
        {
            monster_type berserkers[] = { MONS_BLACK_BEAR, MONS_OGRE, MONS_TROLL,
                                           MONS_TWO_HEADED_OGRE, MONS_DEEP_TROLL };
            to_summon = RANDOM_ELEMENT(berserkers);
        }

        summon_berserker(power, mons, to_summon);
        mons->props[BROTHERS_KEY].get_int()++;
        return;
    }

    case SPELL_SYMBOL_OF_TORMENT:
        torment(mons, TORMENT_SPELL, mons->pos());
        return;

    case SPELL_MESMERISE:
        _mons_mesmerise(mons);
        return;

    case SPELL_CAUSE_FEAR:
        _mons_cause_fear(mons);
        return;

    case SPELL_OLGREBS_TOXIC_RADIANCE:
        cast_toxic_radiance(mons, splpow);
        return;

    case SPELL_SHATTER:
        mons_shatter(mons);
        return;

    case SPELL_IRRADIATE:
        cast_irradiate(splpow, *mons, false);
        return;

    case SPELL_SUMMON_GREATER_DEMON:
        duration  = min(2 + mons->spell_hd(spell_cast) / 10, 6);

        create_monster(
            mgen_data(summon_any_demon(RANDOM_DEMON_GREATER, true),
                      SAME_ATTITUDE(mons), mons->pos(), mons->foe, MG_NONE, god)
            .set_summoned(mons, spell_cast, summ_dur(duration)));
        return;

    // Journey -- Added in Summon Lizards
    case SPELL_SUMMON_DRAKES:
        sumcount2 = 1 + random2(mons->spell_hd(spell_cast) / 5 + 1);

        duration  = min(2 + mons->spell_hd(spell_cast) / 10, 6);

        {
            vector<monster_type> monsters;

            for (sumcount = 0; sumcount < sumcount2; ++sumcount)
            {
                monster_type mon = _pick_drake();
                monsters.push_back(mon);
            }

            for (monster_type type : monsters)
            {
                create_monster(
                    mgen_data(type, SAME_ATTITUDE(mons), mons->pos(),
                              mons->foe, MG_NONE, god)
                    .set_summoned(mons, spell_cast, summ_dur(duration)));
            }
        }
        return;

    case SPELL_DRUIDS_CALL:
        _cast_druids_call(mons);
        return;

    case SPELL_BATTLESPHERE:
        cast_battlesphere(mons, min(splpow, 200), false);
        return;

    case SPELL_POLAR_VORTEX:
    {
        _mons_vortex(mons);
        return;
    }

    case SPELL_SUMMON_HOLIES: // Holy monsters.
        sumcount2 = 1 + random2(2); // sequence point
        sumcount2 += random2(mons->spell_hd(spell_cast) / 4 + 1);

        duration  = min(2 + mons->spell_hd(spell_cast) / 5, 6);
        for (int i = 0; i < sumcount2; ++i)
        {
            create_monster(
                mgen_data(random_choose_weighted(
                            100, MONS_ANGEL,     80,  MONS_CHERUB,
                            50,  MONS_DAEVA,      1,  MONS_OPHAN),
                          SAME_ATTITUDE(mons), mons->pos(), mons->foe,
                          MG_NONE, god).set_summoned(mons, spell_cast, summ_dur(duration)));
        }
        return;

    case SPELL_BLINK_OTHER:
    {
        // Allow the caster to comment on moving the foe.
        string msg = getSpeakString(mons->name(DESC_PLAIN) + " blink_other");
        if (!msg.empty() && msg != "__NONE")
        {
            mons_speaks_msg(mons, msg, MSGCH_TALK,
                            silenced(you.pos()) || silenced(mons->pos()));
        }
        break;
    }

    case SPELL_BLINK_OTHER_CLOSE:
    {
        // Allow the caster to comment on moving the foe.
        string msg = getSpeakString(mons->name(DESC_PLAIN)
                                    + " blink_other_close");
        if (!msg.empty() && msg != "__NONE")
        {
            mons_speaks_msg(mons, msg, MSGCH_TALK,
                            silenced(you.pos()) || silenced(mons->pos()));
        }
        break;
    }

    case SPELL_CHAIN_LIGHTNING:
        cast_chain_lightning(splpow, *mons, false);
        return;

    case SPELL_CHAIN_OF_CHAOS:
        cast_chain_spell(spell_cast, splpow, mons);
        return;

    case SPELL_SUMMON_EYEBALLS:
        sumcount2 = 1 + random2(mons->spell_hd(spell_cast) / 7 + 1);

        duration = min(2 + mons->spell_hd(spell_cast) / 10, 6);

        for (sumcount = 0; sumcount < sumcount2; sumcount++)
        {
            const monster_type mon = random_choose_weighted(
                                       100, MONS_GLASS_EYE,
                                        60, MONS_GOLDEN_EYE,
                                        40, MONS_SHINING_EYE,
                                        20, MONS_GREAT_ORB_OF_EYES,
                                        10, MONS_EYE_OF_DEVASTATION);

            create_monster(
                mgen_data(mon, SAME_ATTITUDE(mons), mons->pos(), mons->foe,
                          MG_NONE, god).set_summoned(mons, spell_cast, summ_dur(duration)));
        }
        return;

    case SPELL_IOOD:
        cast_iood(mons, splpow, &pbolt);
        return;

    case SPELL_AWAKEN_FOREST:
        if (!mons->friendly() && have_passive(passive_t::friendly_plants))
        {
            if (you.can_see(*mons))
            {
                mprf("%s commands the forest to attack, but nothing happens.",
                     mons->name(DESC_THE).c_str());
            }
            return;
        }

        duration = (8 + random2avg(mons->spell_hd(spell_cast) * 3 / 2, 2))
                   * BASELINE_DELAY;

        mons->add_ench(mon_enchant(ENCH_AWAKEN_FOREST, 0, mons, duration));
        // Actually, it's a boolean marker... save for a sanity check.
        env.forest_awoken_until = you.elapsed_time + duration;

        env.forest_is_hostile = !mons->friendly();

        // You may be unable to see the monster, but notice an affected tree.
        forest_message(mons->pos(), "The forest starts to sway and rumble!");
        return;

    case SPELL_SUMMON_DRAGON:
        cast_summon_dragon(mons, splpow, god);
        return;

    case SPELL_SUMMON_HYDRA:
        cast_summon_hydra(mons, splpow, god);
        return;

    case SPELL_FORGE_LIGHTNING_SPIRE:
    {
        monster* spire = _summon(*mons, MONS_LIGHTNING_SPIRE, summ_dur(2), slot, false);
        if (spire && !silenced(spire->pos()))
            mpr("An electric hum fills the air.");
        return;
    }

    case SPELL_FIRE_SUMMON:
        sumcount2 = 1 + random2(mons->spell_hd(spell_cast) / 5 + 1);

        duration = min(2 + mons->spell_hd(spell_cast) / 10, 6);

        for (sumcount = 0; sumcount < sumcount2; sumcount++)
        {
            const monster_type mon = random_choose_weighted(
                                       3, MONS_BALRUG,
                                       2, MONS_HELLION,
                                       1, MONS_BRIMSTONE_FIEND);

            create_monster(
                mgen_data(mon, SAME_ATTITUDE(mons), mons->pos(), mons->foe,
                          MG_NONE, god).set_summoned(mons, spell_cast, summ_dur(duration)));
        }
        return;

    case SPELL_SUMMON_TZITZIMITL:
        _summon(*mons, MONS_TZITZIMITL, summ_dur(2), slot);
        return;

    case SPELL_SUMMON_HELL_SENTINEL:
        _summon(*mons, MONS_HELL_SENTINEL, summ_dur(2), slot);
        return;

    case SPELL_WORD_OF_RECALL:
    {
        mon_enchant chant_timer = mon_enchant(ENCH_WORD_OF_RECALL, 1, mons, 30);
        mons->add_ench(chant_timer);
        return;
    }

    case SPELL_INJURY_BOND:
    {
        simple_monster_message(*mons,
            make_stringf(" begins to accept %s allies' injuries.",
                         mons->pronoun(PRONOUN_POSSESSIVE).c_str()).c_str());
        // FIXME: allies preservers vs the player
        for (monster_near_iterator mi(mons, LOS_NO_TRANS); mi; ++mi)
        {
            if (_can_injury_bond(*mons, **mi))
            {
                mon_enchant bond = mon_enchant(ENCH_INJURY_BOND, 1, mons,
                                               40 + random2(80));
                mi->add_ench(bond);
            }
        }

        return;
    }

    case SPELL_CALL_LOST_SOULS:
        sumcount2 = x_chance_in_y(3, 4) ? 2 : 3;

        for (sumcount = 0; sumcount < sumcount2; sumcount++)
        {
            create_monster(mgen_data(MONS_LOST_SOUL, SAME_ATTITUDE(mons),
                                     mons->pos(), mons->foe, MG_NONE, god)
                           .set_summoned(mons, spell_cast, summ_dur(2)));
        }
        return;

    case SPELL_BLINK_ALLIES_ENCIRCLE:
        _blink_allies_encircle(mons);
        return;

    case SPELL_MASS_CONFUSION:
        _mons_mass_confuse(mons);
        return;

    case SPELL_ENGLACIATION:
        if (you.can_see(*mons))
            simple_monster_message(*mons, " radiates an aura of cold.");
        else if (mons->see_cell_no_trans(you.pos()))
            mpr("A wave of cold passes over you.");
        apply_area_visible([splpow, mons] (coord_def where) {
            return englaciate(where, min(splpow, 200), mons);
        }, mons->pos());
        return;

    case SPELL_AWAKEN_VINES:
        _awaken_vines(mons);
        return;

    case SPELL_WALL_OF_BRAMBLES:
        // If we can't cast this for some reason (can be expensive to determine
        // at every call to _monster_spell_goodness), refund the energy for it so that
        // the caster can do something else
        if (!_wall_of_brambles(mons))
        {
            mons->speed_increment +=
                get_monster_data(mons->type)->energy_usage.spell;
        }
        return;

    case SPELL_WIND_BLAST:
    {
        // Wind blast is stopped by FFT_SOLID features.
        if (foe && cell_see_cell(mons->pos(), foe->pos(), LOS_SOLID))
            wind_blast(mons, splpow, foe->pos());
        return;
    }

    case SPELL_FREEZE:
        _mons_cast_freeze(mons);
        return;

    case SPELL_SUMMON_VERMIN:
        _do_high_level_summon(mons, spell_cast, _pick_vermin,
                              one_chance_in(4) ? 3 : 2 , god);
        return;

    case SPELL_DISCHARGE:
        cast_discharge(min(200, splpow), *mons);
        return;

    case SPELL_PORTAL_PROJECTILE:
    {
        // Swap weapons if necessary so that that happens before the spell
        // casting message.
        const item_def *weapon = mons->mslot_item(MSLOT_WEAPON);
        const item_def *swap = mons->mslot_item(MSLOT_ALT_WEAPON);
        if (swap && is_range_weapon(*swap) && (!weapon || !is_range_weapon(*weapon)))
            mons->swap_weapons();
        mons_cast_noise(mons, pbolt, spell_cast, slot_flags);
        handle_throw(mons, pbolt, true, false);
        return;
    }

    case SPELL_IGNITE_POISON:
        cast_ignite_poison(mons, splpow, false);
        return;

    case SPELL_SIGN_OF_RUIN:
        cast_sign_of_ruin(*mons, foe->pos(), random_range(25, 35) * BASELINE_DELAY);
        return;

    case SPELL_BLINK_ALLIES_AWAY:
        _blink_allies_away(mons);
        return;

    case SPELL_GLACIATE:
    {
        ASSERT(foe);
        cast_glaciate(mons, splpow, foe->pos());
        return;
    }

    case SPELL_PHANTOM_MIRROR:
    {
        // Find appropriate ally to clone.
        vector<monster*> targets;
        for (monster_near_iterator mi(mons); mi; ++mi)
        {
            if (_mirrorable(mons, *mi))
                targets.push_back(*mi);
        }

        // If we've found something, mirror it.
        if (targets.size())
        {
            monster* targ = targets[random2(targets.size())];
            if (cast_phantom_mirror(mons, targ))
                simple_monster_message(*targ, " shimmers and seems to become two!");
        }
        return;
    }

    case SPELL_SUMMON_MANA_VIPER:
    {
        const int num_vipers = 1 + random2(mons->spell_hd(spell_cast) / 5 + 1);
        for (int i = 0; i < num_vipers; ++i)
            _summon(*mons, MONS_MANA_VIPER, summ_dur(2), slot);
        return;
    }

    case SPELL_SUMMON_SCORPIONS:
    {
        const int max_scorps = 1 + div_rand_round(splpow, 42);
        const int num_scorps = random_range(1, max_scorps);
        for (int i = 0; i < num_scorps; ++i)
            _summon(*mons, MONS_SCORPION, summ_dur(2), slot);
        return;
    }

    case SPELL_SUMMON_EMPEROR_SCORPIONS:
    {
        const int num_scorps = 1 + random2(mons->spell_hd(spell_cast) / 5 + 1);
        for (int i = 0; i < num_scorps; ++i)
            _summon(*mons, MONS_EMPEROR_SCORPION, summ_dur(5), slot);
        return;
    }

    case SPELL_BATTLECRY:
        _battle_cry(*mons, SPELL_BATTLECRY);
        return;

    case SPELL_WARNING_CRY:
        noisy(spell_effect_noise(SPELL_WARNING_CRY), mons->pos(), mons->mid);
        return;

    case SPELL_HUNTING_CALL:
        _battle_cry(*mons, SPELL_HUNTING_CALL);
        return;

    case SPELL_SEAL_DOORS:
        _seal_doors_and_stairs(mons);
        return;

    case SPELL_BERSERK_OTHER:
        _incite_monsters(mons, true);
        return;

    case SPELL_SPELLSPARK_SERVITOR:
    {
        const int pow = 7 * mons->spell_hd(spell_cast);
        monster* servitor = _summon(*mons, MONS_SPELLSPARK_SERVITOR, summ_dur(4), slot, false);
        if (servitor)
            init_servitor(servitor, mons, pow);
        else if (you.can_see(*mons))
            canned_msg(MSG_NOTHING_HAPPENS);
        return;
    }

    case SPELL_CORRUPTING_PULSE:
        _corrupting_pulse(mons);
        return;

    case SPELL_THROW_ALLY:
        _maybe_throw_ally(*mons);
        return;

    case SPELL_SIREN_SONG:
        _siren_sing(mons, false);
        return;

    case SPELL_AVATAR_SONG:
        _siren_sing(mons, true);
        return;

    case SPELL_REPEL_MISSILES:
        simple_monster_message(*mons, " begins repelling missiles!");
        mons->add_ench(mon_enchant(ENCH_REPEL_MISSILES, 1, mons, INFINITE_DURATION));
        return;

    case SPELL_SUMMON_SCARABS:
    {
        const int num_scarabs = 1 + random2(mons->spell_hd(spell_cast) / 5 + 1);
        for (int i = 0; i < num_scarabs; ++i)
            _summon(*mons, MONS_DEATH_SCARAB, summ_dur(2), slot);
        return;
    }

    case SPELL_CLEANSING_FLAME:
        simple_monster_message(*mons, " channels a blast of cleansing flame!");
        cleansing_flame(5 + (5 * mons->spell_hd(spell_cast) / 12),
                        cleansing_flame_source::spell, mons->pos(), mons);
        return;

    case SPELL_ENTROPIC_WEAVE:
        ASSERT(foe);
        flash_tile(foe->pos(), YELLOW, 120, TILE_BOLT_ENTROPIC_WEAVE);
        foe->corrode(mons, "the entropic weave");
        return;

    case SPELL_SUMMON_EXECUTIONERS:
    {
        const int num_exec = 1 + random2(mons->spell_hd(spell_cast) / 5 + 1);
        duration = min(2 + mons->spell_hd(spell_cast) / 10, 6);
        for (int i = 0; i < num_exec; ++i)
            _summon(*mons, MONS_EXECUTIONER, summ_dur(duration), slot);
        return;
    }

    case SPELL_DOOM_HOWL:
        _doom_howl(*mons);
        return;

    case SPELL_CALL_OF_CHAOS:
        _mons_call_of_chaos(*mons);
        return;

    case SPELL_PRAYER_OF_BRILLIANCE:
        _prayer_of_brilliance(mons);
        return;

    case SPELL_BIND_SOULS:
        simple_monster_message(*mons, " binds the souls of nearby monsters.");
        for (monster_near_iterator mi(mons, LOS_NO_TRANS); mi; ++mi)
        {
            if (*mi == mons)
                continue;
            if (mons_can_bind_soul(mons, *mi))
            {
                mi->add_ench(
                    mon_enchant(ENCH_BOUND_SOUL, 0, mons,
                                random_range(10, 30) * BASELINE_DELAY));
            }
        }
        return;


    case SPELL_GREATER_SERVANT_MAKHLEB:
    {
        const monster_type servants[] = { MONS_EXECUTIONER, MONS_GREEN_DEATH,
                                          MONS_BLIZZARD_DEMON, MONS_BALRUG,
                                          MONS_CACODEMON };
        _summon(*mons, RANDOM_ELEMENT(servants), summ_dur(5), slot);
        return;
    }

    case SPELL_UPHEAVAL:
        _mons_upheaval(*mons, *foe, true);
        return;

    case SPELL_ERUPTION:
        _mons_upheaval(*mons, *foe, false);
        return;

    case SPELL_SPORULATE:
    {
        mgen_data mgen (MONS_BALLISTOMYCETE_SPORE,
                mons->friendly() ? BEH_FRIENDLY : BEH_HOSTILE, mons->pos(),
                mons->foe);
        mgen.set_summoned(mons, SPELL_SPORULATE, random_range(40, 70), false, false);
        // Add 1HD to the spore for each additional HD the spawner has.
        mgen.hd = mons_class_hit_dice(MONS_BALLISTOMYCETE_SPORE) +
            max(0, mons->spell_hd() - mons_class_hit_dice(mons->type));

        create_monster(mgen);

        return;
    }

    case SPELL_ROLL:
        mons->add_ench(ENCH_ROLLING);
        simple_monster_message(*mons,
                " curls into a ball and begins rolling!");
        return;

    case SPELL_STOKE_FLAMES:
        _summon(*mons, MONS_CREEPING_INFERNO, summ_dur(2), slot);
        return;

    case SPELL_BOMBARD:
    {
        const coord_def targ = pbolt.target;
        _fire_simple_beam(*mons, slot, pbolt);
        if (mons->alive())
            mons->stumble_away_from(targ, "the blast");
        return;
    }

    case SPELL_SANDBLAST:
    {
        _fire_simple_beam(*mons, slot, pbolt);
        if (mons->alive())
            mons->lose_energy(EUT_SPELL, 2);
        return;
    }

    case SPELL_FUNERAL_DIRGE:
        _cast_dirge(*mons);
        return;

    case SPELL_MANIFOLD_ASSAULT:
        cast_manifold_assault(*mons, mons_spellpower(*mons, SPELL_MANIFOLD_ASSAULT), false);
        return;

    case SPELL_REGENERATE_OTHER:
        _cast_regenerate_other(mons);
        return;

    case SPELL_MASS_REGENERATION:
        _cast_mass_regeneration(mons);
        return;

    case SPELL_BESTOW_ARMS:
        _cast_bestow_arms(*mons);
        return;

    case SPELL_FULMINANT_PRISM:
        _mons_cast_prisms(*mons, *mons->get_foe(),
                          mons_spellpower(*mons, SPELL_FULMINANT_PRISM), false);
        return;

    case SPELL_HELLFIRE_MORTAR:
        _mons_cast_hellfire_mortar(*mons, *mons->get_foe(),
                                   mons_spellpower(*mons, SPELL_HELLFIRE_MORTAR), false);
        return;
    }

    if (spell_is_direct_explosion(spell_cast))
        _fire_direct_explosion(*mons, slot, pbolt);
    else
        _fire_simple_beam(*mons, slot, pbolt);
}

int living_spell_count(spell_type spell, bool random)
{
    if (spell == SPELL_LEHUDIBS_CRYSTAL_SPEAR)
        return random ? random_range(1, 2) : 2;
    return random ? random_range(2, 3) : 3;
}

spell_type living_spell_type_for(monster_type mtyp)
{
    switch (mtyp)
    {
    case MONS_EARTHEN_TOME:
        return SPELL_PETRIFY;
    case MONS_CRYSTAL_TOME:
        return SPELL_LEHUDIBS_CRYSTAL_SPEAR;
    case MONS_DIVINE_TOME:
    case MONS_ORC_APOSTLE:
        return SPELL_SMITING;
    case MONS_FROSTBOUND_TOME:
        return SPELL_ICEBLAST;
    default:
        return SPELL_NO_SPELL;
    }
}

static int _noise_level(const monster* mons, spell_type spell,
                        bool silent, mon_spell_slot_flags slot_flags)
{
    const spell_flags flags = get_spell_flags(spell);

    int noise;

    if (silent
        || (slot_flags & MON_SPELL_INNATE_MASK
            && !(slot_flags & MON_SPELL_NOISY)
            && !(flags & spflag::noisy)))
    {
        noise = 0;
    }
    // Dragons are extra loud.
    else if (mons_genus(mons->type) == MONS_DRAGON
             && (slot_flags & MON_SPELL_INNATE_MASK
                 || slot_flags & MON_SPELL_VOCAL))
    {
        noise = get_shout_noise_level(S_LOUD_ROAR);
    }
    else
        noise = spell_noise(spell);

    return noise;
}

static void _speech_keys(vector<string>& key_list,
                         const monster* mons, const bolt& pbolt,
                         spell_type spell, mon_spell_slot_flags slot_flags,
                         bool targeted)
{
    const string cast_str = " cast";

    // Can't use copy-initialization 'wizard = slot_flags & ...' here,
    // because the bitfield-to-bool conversion is not implicit.
    const bool wizard  {slot_flags & MON_SPELL_WIZARD};
    const bool priest  {slot_flags & MON_SPELL_PRIEST};
    const bool natural {slot_flags & MON_SPELL_NATURAL
                        || slot_flags & MON_SPELL_VOCAL};
    const bool magical {slot_flags & MON_SPELL_MAGICAL};

    const mon_body_shape shape = get_mon_shape(*mons);
    const string    spell_name = spell_title(spell);
    const bool      real_spell = priest || wizard;

    // Before just using generic per-spell and per-monster casts, try
    // per-monster, per-spell, with the monster type name, then the
    // species name, then the genus name, then wizard/priest/magical/natural.
    // We don't include "real" or "gestures" here since that can be
    // be determined from the monster type; or "targeted" since that
    // can be determined from the spell.
    key_list.push_back(spell_name + " "
                       + mons_type_name(mons->type, DESC_PLAIN) + cast_str);
    key_list.push_back(spell_name + " "
                       + mons_type_name(mons_species(mons->type), DESC_PLAIN)
                       + cast_str);
    key_list.push_back(spell_name + " "
                       + mons_type_name(mons_genus(mons->type), DESC_PLAIN)
                       + cast_str);
    if (wizard)
    {
        key_list.push_back(make_stringf("%s %swizard%s",
                               spell_name.c_str(),
                               mon_shape_is_humanoid(shape) ? ""
                                                            : "non-humanoid ",
                               cast_str.c_str()));
    }
    else if (priest)
        key_list.push_back(spell_name + " priest" + cast_str);
    else if (magical)
        key_list.push_back(spell_name + " magical" + cast_str);
    else if (natural)
        key_list.push_back(spell_name + " natural" + cast_str);


    // Now try just the spell's name.
    if (mon_shape_is_humanoid(shape))
    {
        if (real_spell)
            key_list.push_back(spell_name + cast_str + " real");
        if (mons_intel(*mons) >= I_HUMAN)
            key_list.push_back(spell_name + cast_str + " gestures");
    }

    // XXX: Give the final shot from hoarfrost cannonade a different message.
    //      (If only I could see a nicer way to do this...)
    if (spell == SPELL_HOARFROST_BULLET
        && mons->props.exists(HOARFROST_SHOTS_KEY)
        && mons->props[HOARFROST_SHOTS_KEY].get_int() == MAX_HOARFROST_SHOTS)
    {
        key_list.push_back(spell_name + cast_str + " finale");
    }

    key_list.push_back(spell_name + cast_str);

    // Only postfix "targeted" after this point.
    const unsigned int num_spell_keys = key_list.size();

    // Next the monster type name, then species name, then genus name.
    key_list.push_back(mons_type_name(mons->type, DESC_PLAIN) + cast_str);
    key_list.push_back(mons_type_name(mons_species(mons->type), DESC_PLAIN)
                       + cast_str);
    key_list.push_back(mons_type_name(mons_genus(mons->type), DESC_PLAIN)
                       + cast_str);

    // Last, generic wizard, priest or magical.
    if (wizard)
    {
        key_list.push_back(make_stringf("%swizard%s",
                               mon_shape_is_humanoid(shape) ? ""
                                                            : "non-humanoid ",
                               cast_str.c_str()));
    }
    else if (priest)
        key_list.push_back("priest" + cast_str);
    else if (magical)
        key_list.push_back("magical" + cast_str);

    if (targeted)
    {
        // For targeted spells, try with the targeted suffix first.
        for (unsigned int i = key_list.size() - 1; i >= num_spell_keys; i--)
        {
            string str = key_list[i] + " targeted";
            key_list.insert(key_list.begin() + i, str);
        }

        // Generic beam messages.
        if (pbolt.visible())
        {
            key_list.push_back(pbolt.get_short_name() + " beam " + cast_str);
            key_list.emplace_back("beam catchall cast");
        }
    }
}

static string _speech_message(const monster &mon,
                              const vector<string>& key_list,
                              bool silent, bool unseen)
{
    string prefix;
    if (silent)
        prefix = "silent ";
    else if (unseen)
        prefix = "unseen ";

    string msg;
    for (const string &key : key_list)
    {
#ifdef DEBUG_MONSPEAK
        dprf(DIAG_SPEECH, "monster casting lookup: %s%s",
             prefix.c_str(), key.c_str());
#endif

        msg = getSpeakString(prefix + key);

        if (msg == "__NONE")
        {
            msg = "";
            break;
        }

        if (!msg.empty() && !invalid_msg(mon, msg))
            break;

        // If we got no message and we're using the silent prefix, then
        // try again without the prefix.
        if (prefix != "silent ")
            continue;

        msg = getSpeakString(key);
        if (msg == "__NONE")
        {
            msg = "";
            break;
        }
        else if (!msg.empty())
            break;
    }

    return msg;
}

static void _speech_fill_target(string& targ_prep, string& target,
                                const monster* mons, const bolt& pbolt,
                                bool gestured)
{
    targ_prep = "at";
    target    = "nothing";

    bolt tracer = pbolt;
    // For a targeted but rangeless spell make the range positive so that
    // fire_tracer() will fill out path_taken.
    if (pbolt.range == 0 && pbolt.target != mons->pos())
        tracer.range = ENV_SHOW_DIAMETER;
    fire_tracer(mons, tracer);

    if (pbolt.target == you.pos())
        target = "you";
    else if (pbolt.target == mons->pos())
        target = mons->pronoun(PRONOUN_REFLEXIVE);
    // Monsters should only use targeted spells while foe == MHITNOT
    // if they're targeting themselves.
    else if (mons->foe == MHITNOT && !mons_is_confused(*mons, true))
        target = "NONEXISTENT FOE";
    else if (!invalid_monster_index(mons->foe)
             && env.mons[mons->foe].type == MONS_NO_MONSTER)
    {
        target = "DEAD FOE";
    }
    else if (in_bounds(pbolt.target) && you.see_cell(pbolt.target))
    {
        if (const monster* mtarg = monster_at(pbolt.target))
        {
            if (you.can_see(*mtarg))
                target = mtarg->name(DESC_THE);
        }
    }

    const bool visible_path      = pbolt.visible() || gestured;

    // Monster might be aiming past the real target, or maybe some fuzz has
    // been applied because the target is invisible.
    if (target == "nothing")
    {
        if (pbolt.aimed_at_spot || pbolt.origin_spell == SPELL_DIG)
        {
            int count = 0;
            for (adjacent_iterator ai(pbolt.target); ai; ++ai)
            {
                const actor* act = actor_at(*ai);
                if (act && act != mons && you.can_see(*act))
                {
                    targ_prep = "next to";

                    if (act->is_player() || one_chance_in(++count))
                        target = act->name(DESC_THE);

                    if (act->is_player())
                        break;
                }
            }

            if (targ_prep == "at")
            {
                if (env.grid(pbolt.target) != DNGN_FLOOR)
                {
                    target = feature_description(env.grid(pbolt.target),
                                                 NUM_TRAPS, "", DESC_THE);
                }
                else
                    target = "thin air";
            }

            return;
        }

        bool mons_targ_aligned = false;

        for (const coord_def &pos : tracer.path_taken)
        {
            if (pos == mons->pos())
                continue;

            const monster* m = monster_at(pos);
            if (pos == you.pos())
            {
                // Be egotistical and assume that the monster is aiming at
                // the player, rather than the player being in the path of
                // a beam aimed at an ally.
                if (!mons->wont_attack())
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
            else if (visible_path && m && you.can_see(*m))
            {
                bool is_aligned  = mons_aligned(m, mons);
                string name = m->name(DESC_THE);

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
                    const actor* act = monster_at(*ai);
                    if (act && act != mons && you.can_see(*act))
                    {
                        targ_prep = "past";
                        if (act->is_player()
                            || one_chance_in(++count))
                        {
                            target = act->name(DESC_THE);
                        }

                        if (act->is_player())
                            break;
                    }
                }
            }
        } // for (const coord_def pos : path)
    } // if (target == "nothing" && targeted)

    const actor* foe = mons->get_foe();

    // If we still can't find what appears to be the target, and the
    // monster isn't just throwing the spell in a random direction,
    // we should be able to tell what the monster was aiming for if
    // we can see the monster's foe and the beam (or the beam path
    // implied by gesturing). But only if the beam didn't actually hit
    // anything (but if it did hit something, why didn't that monster
    // show up in the beam's path?)
    if (target == "nothing"
        && (tracer.foe_info.count + tracer.friend_info.count) == 0
        && foe != nullptr
        && you.can_see(*foe)
        && !mons->confused()
        && visible_path)
    {
        target = foe->name(DESC_THE);
        targ_prep = (pbolt.aimed_at_spot ? "next to" : "past");
    }

    // If the monster gestures to create an invisible beam then
    // assume that anything close to the beam is the intended target.
    // Also, if the monster gestures to create a visible beam but it
    // misses still say that the monster gestured "at" the target,
    // rather than "past".
    if (gestured || target == "nothing")
        targ_prep = "at";

    // "throws whatever at something" is better than "at nothing"
    if (target == "nothing")
        target = "something";
}

void mons_cast_noise(monster* mons, const bolt &pbolt,
                     spell_type spell_cast, mon_spell_slot_flags slot_flags)
{
    const bool unseen = !you.can_see(*mons);
    const bool silent = silenced(mons->pos());

    if (unseen && silent)
        return;

    int noise = _noise_level(mons, spell_cast, silent, slot_flags);

    const spell_flags spflags = get_spell_flags(spell_cast);
    const bool targeted = bool(spflags & spflag::targeting_mask);

    vector<string> key_list;
    _speech_keys(key_list, mons, pbolt, spell_cast, slot_flags, targeted);

    string msg = _speech_message(*mons, key_list, silent, unseen);

    if (msg.empty())
    {
        if (silent)
            return;

        noisy(noise, mons->pos(), mons->mid);
        return;
    }

    // FIXME: we should not need to look at the message text.
    const bool gestured = msg.find("Gesture") != string::npos
                          || msg.find(" gesture") != string::npos
                          || msg.find("Point") != string::npos
                          || msg.find(" point") != string::npos;

    string targ_prep = "at";
    string target    = "NO_TARGET";

    if (targeted)
        _speech_fill_target(targ_prep, target, mons, pbolt, gestured);

    msg = replace_all(msg, "@at@",     targ_prep);
    msg = replace_all(msg, "@target@", target);

    string beam_name;
    if (!targeted)
        beam_name = "NON TARGETED BEAM";
    else if (pbolt.name.empty())
        beam_name = "INVALID BEAM";
    else
        beam_name = pbolt.get_short_name();

    msg = replace_all(msg, "@beam@", beam_name);

    const msg_channel_type chan =
        (unseen              ? MSGCH_SOUND :
         mons->friendly()    ? MSGCH_FRIEND_SPELL
                             : MSGCH_MONSTER_SPELL);

    if (silent || noise == 0)
        mons_speaks_msg(mons, msg, chan, true);
    else if (noisy(noise, mons->pos(), mons->mid) || !unseen)
    {
        // noisy() returns true if the player heard the noise.
        mons_speaks_msg(mons, msg, chan);
    }
}

static const int MIN_THROW_DIST = 2;

static bool _valid_throw_dest(const actor &thrower, const actor &victim,
                              const coord_def pos)
{
    return thrower.pos().distance_from(pos) >= MIN_THROW_DIST
           && !actor_at(pos)
           && victim.is_habitable(pos)
           && thrower.see_cell(pos);
}

/**
 * Choose a landing site for a monster that is throwing someone.
 *
 * @param thrower      The monster performing the toss.
 * @param victim       The actor being thrown.
 * @param rater        A function that takes thrower, victim, and an arbitrary
 *                     coord_def and determines how good the throw is; a higher
 *                     number is better.
 * @return             The coord_def of one of the best (as determined by rater)
 *                     possible landing sites for a toss.
 *                     If no valid site is found, returns the origin (0,0).
 */
static coord_def _choose_throwing_target(const monster &thrower,
                            const actor &victim,
                            function<int (const monster&, const actor&,
                                          coord_def)> rater)
{
    int best_site_score = -1;
    vector<coord_def> best_sites;

    for (distance_iterator di(thrower.pos(), true, true, LOS_RADIUS); di; ++di)
    {
        ray_def ray;
        // Unusable landing sites.
        if (!_valid_throw_dest(thrower, victim, *di)
            || !find_ray(thrower.pos(), *di, ray, opc_solid_see))
        {
            continue;
        }

        const int site_score = rater(thrower, victim, *di);
        if (site_score > best_site_score)
        {
            best_site_score = site_score;
            best_sites.clear();
        }
        if (site_score == best_site_score)
            best_sites.push_back(*di);
    }

    // No valid landing site found.
    if (!best_sites.size())
        return coord_def(0,0);

    const coord_def best_site = best_sites[random2(best_sites.size())];
    return best_site;
}

static bool _will_throw_ally(const monster& thrower, const monster& throwee)
{
    switch (thrower.type)
    {
    case MONS_ROBIN:
        return throwee.mons_species() == MONS_GOBLIN;
    case MONS_POLYPHEMUS:
        return mons_genus(throwee.type) == MONS_YAK;
    case MONS_IRON_GIANT:
        return !throwee.is_peripheral();
    default:
        return false;
    }
}

static monster* _find_ally_to_throw(const monster &mons)
{
    const actor *foe = mons.get_foe();
    if (!foe)
        return nullptr;

    int furthest_dist = -1;

    monster* best = nullptr;
    for (fair_adjacent_iterator ai(mons.pos(), true); ai; ++ai)
    {
        monster* throwee = monster_at(*ai);

        if (!throwee || !throwee->alive() || !mons_aligned(&mons, throwee)
            || !_will_throw_ally(mons, *throwee))
        {
            continue;
        }

        // Don't try to throw anything constricted.
        if (throwee->is_constricted())
            continue;

        // Don't try to throw tentacles or their parts.
        // Both too big and too easy to disconnect.
        if (mons_is_tentacle_or_tentacle_segment(throwee->type))
            continue;

        // otherwise throw whoever's furthest from our target.
        const int dist = grid_distance(throwee->pos(), foe->pos());
        if (dist > furthest_dist)
        {
            best = throwee;
            furthest_dist = dist;
        }
    }

    if (best != nullptr)
        dprf("found a monster to toss");
    else
        dprf("couldn't find anyone to toss");
    return best;
}

/**
 * Toss an ally at the monster's foe, landing them in the given square after
 * maybe dealing a pittance of damage.
 *
 * XXX: some duplication with tentacle toss code
 *
 * @param thrower       The monster doing the throwing.
 * @param throwee       The monster being tossed.
 * @param chosen_dest   The location of the square throwee should land on.
 */
static void _throw_ally_to(const monster &thrower, monster &throwee,
                           const coord_def chosen_dest)
{
    ASSERT_IN_BOUNDS(chosen_dest);
    ASSERT(!throwee.is_constricted());

    actor* foe = thrower.get_foe();
    ASSERT(foe);

    const coord_def old_pos = throwee.pos();
    const bool thrower_seen = you.can_see(thrower);
    const bool throwee_was_seen = you.can_see(throwee);
    const bool throwee_will_be_seen = throwee.visible_to(&you)
                                      && you.see_cell(chosen_dest);
    const bool throwee_seen = throwee_was_seen || throwee_will_be_seen;

    if (!(throwee.flags & MF_WAS_IN_VIEW))
        throwee.seen_context = SC_THROWN_IN;

    if (thrower_seen || throwee_seen)
    {
        const string destination = you.can_see(*foe) ?
                                   make_stringf("at %s",
                                                foe->name(DESC_THE).c_str()) :
                                   "out of sight";

        mprf("%s throws %s %s!",
             (thrower_seen ? thrower.name(DESC_THE).c_str() : "Something"),
             (throwee_seen ? throwee.name(DESC_THE, true).c_str() : "something"),
             destination.c_str());

        bolt beam;
        beam.range   = INFINITE_DISTANCE;
        beam.hit     = AUTOMATIC_HIT;
        beam.name    = throwee.name(DESC_THE, true);
        beam.flavour = BEAM_VISUAL;
        beam.source  = thrower.pos();
        beam.target  = chosen_dest;
        beam.glyph   = mons_char(throwee.type);
        const monster_info mi(&throwee);
        beam.colour  = mi.colour();

        beam.draw_delay = 30; // Make beam animation somewhat slower than normal.
        beam.aimed_at_spot = true;
        beam.fire();
    }

    throwee.move_to_pos(chosen_dest);
    throwee.apply_location_effects(old_pos);
    throwee.check_redraw(old_pos);

    const string killed_by = make_stringf("Hit by %s thrown by %s",
                                          throwee.name(DESC_A, true).c_str(),
                                          thrower.name(DESC_PLAIN, true).c_str());
    const int dam = foe->apply_ac(random2(thrower.get_hit_dice() * 2));
    foe->hurt(&thrower, dam, BEAM_NONE, KILLED_BY_BEAM, "", killed_by, true);
    _whack(thrower, *foe);

    // wake sleepy goblins
    behaviour_event(&throwee, ME_DISTURB, &thrower, throwee.pos());
}

static int _throw_ally_site_score(const monster& thrower, const actor& /*throwee*/,
                                  coord_def pos)
{
    const actor *foe = thrower.get_foe();
    if (!foe || !adjacent(foe->pos(), pos))
        return -2;
    return grid_distance(thrower.pos(), pos);
}

static void _maybe_throw_ally(const monster &mons)
{
    monster* throwee = _find_ally_to_throw(mons);
    if (!throwee)
        return;

    const coord_def toss_target =
        _choose_throwing_target(mons, *static_cast<actor*>(throwee),
                                _throw_ally_site_score);

    if (toss_target.origin())
        return;

    _throw_ally_to(mons, *throwee, toss_target);
}

/**
 * Check if a siren or merfolk avatar should sing its song.
 *
 * @param mons   The singing monster.
 * @param avatar Whether to use the more powerful "avatar song".
 * @return       Whether the song should be sung.
 */
static ai_action::goodness _siren_goodness(monster* mons, bool avatar)
{
    // Don't behold observer in the arena.
    if (crawl_state.game_is_arena())
        return ai_action::impossible();

    // Don't behold player already half down or up the stairs.
    if (player_stair_delay())
    {
        dprf("Taking stairs, don't mesmerise.");
        return ai_action::impossible();
    }

    // Won't sing if either of you silenced, or it's friendly,
    // confused, fleeing, or leaving the level.
    if (mons->has_ench(ENCH_CONFUSION)
        || mons_is_fleeing(*mons)
        || mons->pacified()
        || mons->friendly()
        || !player_can_hear(mons->pos()))
    {
        return ai_action::bad();
    }

    // Don't even try on berserkers. Sirens know their limits.
    // (merfolk avatars should still sing since their song has other effects)
    if (!avatar && you.berserk())
        return ai_action::bad();

    // If the mer is trying to mesmerise you anew, only sing half as often.
    if (!you.beheld_by(*mons) && mons->foe == MHITYOU && you.can_see(*mons)
        && coinflip())
    {
        return ai_action::bad();
    }

    // We can do it!
    return ai_action::good();
}

/**
 * Have a monster attempt to cast Doom Howl.
 *
 * @param mon   The howling monster.
 */
static void _doom_howl(monster &mon)
{
    const int pow = _ench_power(SPELL_DOOM_HOWL, mon);
    const int willpower = you.check_willpower(&mon, pow);
    const string effect = willpower > 0 ?
                            make_stringf("but you%s",
                                         you.resist_margin_phrase(willpower).c_str()) :
                            "and it begins to echo in your mind!";
    mprf("%s unleashes a %s howl, %s",
         mon.name(DESC_THE).c_str(),
         silenced(mon.pos()) ? "silent" : "terrible",
         effect.c_str());
    noisy(spell_effect_noise(SPELL_DOOM_HOWL), mon.pos(), mon.mid);
    if (willpower <= 0)
    {
        flash_tile(you.pos(), BLACK, 120, TILE_BOLT_DOOM_HOWL);
        you.duration[DUR_DOOM_HOWL] = random_range(120, 180);
        mon.props[DOOM_HOUND_HOWLED_KEY] = true;
    }
}

/**
 * Have a siren or merfolk avatar attempt to mesmerize the player.
 *
 * @param mons   The singing monster.
 * @param avatar Whether to use the more powerful "avatar song".
 */
static void _siren_sing(monster* mons, bool avatar)
{
    const msg_channel_type spl = (mons->friendly() ? MSGCH_FRIEND_SPELL
                                                       : MSGCH_MONSTER_SPELL);
    const bool already_mesmerised = you.beheld_by(*mons);

    noisy(LOS_DEFAULT_RANGE, mons->pos(), mons->mid);

    if (avatar && !mons->has_ench(ENCH_MERFOLK_AVATAR_SONG))
        mons->add_ench(mon_enchant(ENCH_MERFOLK_AVATAR_SONG, 0, mons, 70));

    if (you.can_see(*mons))
    {
        const char * const song_adj = already_mesmerised ? "its luring"
                                                         : "a haunting";
        const string song_desc = make_stringf(" chants %s song.", song_adj);
        simple_monster_message(*mons, song_desc.c_str(), false, spl);
    }
    else
    {
        mprf(MSGCH_SOUND, "You hear %s.",
                          already_mesmerised ? "a luring song" :
                          coinflip()         ? "a haunting song"
                                             : "an eerie melody");

        // If you're already mesmerised by an invisible siren, it
        // can still prolong the enchantment.
        if (!already_mesmerised)
            return;
    }

    // power is the same for siren & avatar song, so just use siren
    const int pow = _ench_power(SPELL_SIREN_SONG, *mons);
    const int willpower = you.check_willpower(mons, pow);

    // Once mesmerised by a particular monster, you cannot resist anymore.
    if (you.duration[DUR_MESMERISE_IMMUNE]
        || !already_mesmerised
           && (willpower > 0 || you.clarity()))
    {
        if (you.clarity())
            canned_msg(MSG_YOU_UNAFFECTED);
        else if (you.duration[DUR_MESMERISE_IMMUNE] && !already_mesmerised)
            canned_msg(MSG_YOU_RESIST);
        else
            mprf("You%s", you.resist_margin_phrase(willpower).c_str());
        return;
    }

    you.add_beholder(*mons);
}

// Checks to see if a particular spell is worth casting in the first place.
ai_action::goodness monster_spell_goodness(monster* mon, spell_type spell)
{
    actor *foe = mon->get_foe();
    const bool friendly = mon->friendly();

    if (!foe && (get_spell_flags(spell) & spflag::targeting_mask))
        return ai_action::impossible();

    // Keep friendly summoners from spamming summons constantly.
    if (friendly && !foe && spell_typematch(spell, spschool::summoning))
        return ai_action::bad();

    // Don't use abilities while rolling.
    if (mon->has_ench(ENCH_ROLLING))
        return ai_action::impossible();

    // Don't try to cast spells at players who are stepped from time.
    if (foe && foe->is_player() && you.duration[DUR_TIME_STEP])
        return ai_action::impossible();

    if (!mon->wont_attack())
    {
        if (spell_harms_area(spell) && env.sanctuary_time > 0)
            return ai_action::impossible();

        if (spell_harms_target(spell) && is_sanctuary(mon->target))
            return ai_action::impossible();
    }

    // Don't bother casting a summon spell if we're already at its cap
    // (Marionettes pass their summons onto the player, so count for them instead)
    if (summons_are_capped(spell))
    {
        if (mon->attitude == ATT_MARIONETTE)
        {
            if (count_summons(&you, spell) >= summons_limit(spell, false))
                return ai_action::impossible();
        }
        else if (count_summons(mon, spell) >= summons_limit(spell, false))
            return ai_action::impossible();
    }

    const mons_spell_logic* logic = _get_spell_logic(*mon, spell);
    if (logic && logic->calc_goodness)
        return logic->calc_goodness(*mon);

    const bool no_clouds = env.level_state & LSTATE_STILL_WINDS;

    // Eventually, we'll probably want to be able to have monsters
    // learn which of their elemental bolts were resisted and have those
    // handled here as well. - bwr
    switch (spell)
    {
    case SPELL_CALL_TIDE:
        if (!player_in_branch(BRANCH_SHOALS) || mon->has_ench(ENCH_TIDE))
            return ai_action::impossible();
        else if (!foe || (env.grid(mon->pos()) == DNGN_DEEP_WATER
                     && env.grid(foe->pos()) == DNGN_DEEP_WATER))
        {
            return ai_action::bad();
        }
        else
            return ai_action::good();

    case SPELL_MOURNING_WAIL:
    case SPELL_BOLT_OF_DRAINING:
    case SPELL_MALIGN_OFFERING:
    case SPELL_GHOSTLY_FIREBALL:
        ASSERT(foe);
        return _negative_energy_spell_goodness(foe);

    case SPELL_DISPEL_UNDEAD:
    case SPELL_DISPEL_UNDEAD_RANGE:
        ASSERT(foe);
        return ai_action::good_or_bad(!!(foe->holiness() & MH_UNDEAD));

    case SPELL_BERSERKER_RAGE:
        return ai_action::good_or_impossible(mon->needs_berserk(false));

#if TAG_MAJOR_VERSION == 34
    case SPELL_SWIFTNESS:
#endif
    case SPELL_SPRINT:
        return ai_action::good_or_impossible(!mon->has_ench(ENCH_SWIFT));

    case SPELL_MAJOR_HEALING:
        return ai_action::good_or_bad(mon->hit_points <= mon->max_hit_points / 2);

    case SPELL_WOODWEAL:
    {
        bool touch_wood = false;
        for (adjacent_iterator ai(mon->pos()); ai; ai++)
        {
            if (feat_is_tree(env.grid(*ai)))
            {
                touch_wood = true;
                break;
            }
            const monster *adj_mons = monster_at(*ai);
            if (adj_mons && mons_genus(adj_mons->type) == MONS_SHAMBLING_MANGROVE)
            {
                touch_wood = true;
                break;
            }
        }
        if (!touch_wood)
            return ai_action::impossible();
        return ai_action::good_or_bad(mon->hit_points <= mon->max_hit_points / 2);
    }

    case SPELL_BLINK_OTHER:
    case SPELL_BLINK_OTHER_CLOSE:
        return _foe_effect_viable(*mon, DUR_DIMENSION_ANCHOR, ENCH_DIMENSION_ANCHOR);

    case SPELL_SOJOURNING_BOLT:
        return ai_action::good_or_impossible(_sojourning_bolt_goodness(*mon).first);

    case SPELL_DREAM_DUST:
        return _foe_sleep_viable(*mon);

    // Mara shouldn't cast player ghost if he can't see the player
    case SPELL_SUMMON_ILLUSION:
        ASSERT(foe);
        return ai_action::good_or_impossible(mon->see_cell_no_trans(foe->pos())
               && mon->can_see(*foe)
               && actor_is_illusion_cloneable(foe));

    case SPELL_AWAKEN_FOREST:
        if (mon->has_ench(ENCH_AWAKEN_FOREST)
               || env.forest_awoken_until > you.elapsed_time)
        {
            return ai_action::impossible();
        }
        return ai_action::good_or_bad(forest_near_enemy(mon));

    case SPELL_BATTLESPHERE:
        return ai_action::good_or_bad((!mon->friendly() || mon->get_foe())
                                      && !find_battlesphere(mon));

    case SPELL_INJURY_BOND:
        for (monster_near_iterator mi(mon, LOS_NO_TRANS); mi; ++mi)
            if (_can_injury_bond(*mon, **mi))
                return ai_action::good(); // We found at least one target; that's enough.
        return ai_action::bad();

    case SPELL_BLINK_ALLIES_ENCIRCLE:
        ASSERT(foe);
        if (!mon->see_cell_no_trans(foe->pos()) || !mon->can_see(*foe))
            return ai_action::impossible();

        for (monster_near_iterator mi(mon, LOS_NO_TRANS); mi; ++mi)
            if (_valid_encircle_ally(mon, *mi, foe->pos()))
                return ai_action::good(); // We found at least one valid ally; that's enough.
        return ai_action::bad();

    case SPELL_AWAKEN_VINES:
        return ai_action::good_or_impossible(count_summons(mon, SPELL_AWAKEN_VINES) <= 3
                    && _awaken_vines(mon, true)); // test cast: would anything happen?

    case SPELL_WATERSTRIKE:
        ASSERT(foe);
        return ai_action::good_or_impossible(feat_is_water(env.grid(foe->pos())));

    // Don't use unless our foe is close to us and there are no allies already
    // between the two of us
    case SPELL_WIND_BLAST:
        ASSERT(foe);
        if (foe->pos().distance_from(mon->pos()) < 4)
        {
            bolt tracer;
            tracer.target = foe->pos();
            tracer.range  = LOS_RADIUS;
            tracer.hit    = AUTOMATIC_HIT;
            fire_tracer(mon, tracer);

            actor* act = actor_at(tracer.path_taken.back());
            // XX does this handle multiple actors?
            return ai_action::good_or_bad(!act || !mons_aligned(mon, act));
        }
        else
            return ai_action::bad(); // no close foe

    case SPELL_BROTHERS_IN_ARMS:
        return ai_action::good_or_bad(
            !mon->props.exists(BROTHERS_KEY)
               || mon->props[BROTHERS_KEY].get_int() < 2);

    case SPELL_HOLY_FLAMES:
        return ai_action::good_or_impossible(!no_clouds);

    case SPELL_FREEZE:
        ASSERT(foe);
        return ai_action::good_or_impossible(adjacent(mon->pos(), foe->pos()));

    case SPELL_IRRADIATE:
        return _should_irradiate(*mon);

    case SPELL_DRUIDS_CALL:
        // Don't cast unless there's at least one valid target
        for (monster_iterator mi; mi; ++mi)
            if (_valid_druids_call_target(mon, *mi))
                return ai_action::good();
        return ai_action::impossible();

    case SPELL_MESMERISE:
        // Don't spam mesmerisation if you're already mesmerised
        // TODO: is this really the right place for randomization like this?
        // or is there a better way to handle repeated mesmerise? Long-run:
        // the goodness should be lower in this case which would prioritize
        // better actions, if actions were calculated comparatively.
        return ai_action::good_or_bad(!you.beheld_by(*mon) || coinflip());

    case SPELL_DISCHARGE:
        // TODO: possibly check for friendlies nearby?
        // Perhaps it will be used recklessly like chain lightning...
        return ai_action::good_or_bad(foe && adjacent(foe->pos(), mon->pos()));

    case SPELL_PORTAL_PROJECTILE:
    {
        bolt beam;
        beam.source    = mon->pos();
        beam.target    = mon->target;
        beam.source_id = mon->mid;
        return ai_action::good_or_bad(handle_throw(mon, beam, true, true));
    }

    case SPELL_SIGN_OF_RUIN:
        return ai_action::good_or_impossible(foe &&
                                             cast_sign_of_ruin(*mon, foe->pos(), 1, true)
                                                == spret::success);

    case SPELL_BLINK_ALLIES_AWAY:
        ASSERT(foe);
        if (!mon->see_cell_no_trans(foe->pos()) && !mon->can_see(*foe))
            return ai_action::impossible();

        for (monster_near_iterator mi(mon, LOS_NO_TRANS); mi; ++mi)
            if (_valid_blink_away_ally(mon, *mi, foe->pos()))
                return ai_action::good();
        return ai_action::bad();

    // Don't let clones duplicate anything, to prevent exponential explosion
    case SPELL_FAKE_MARA_SUMMON:
        return ai_action::good_or_impossible(!mon->has_ench(ENCH_PHANTOM_MIRROR));

    case SPELL_PHANTOM_MIRROR:
        if (mon->has_ench(ENCH_PHANTOM_MIRROR))
            return ai_action::impossible();
        for (monster_near_iterator mi(mon); mi; ++mi)
        {
            // A single valid target is enough.
            if (_mirrorable(mon, *mi))
                return ai_action::good();
        }
        return ai_action::impossible();

    case SPELL_THROW_BARBS:
    case SPELL_HARPOON_SHOT:
        // Don't fire if we can hit.
        ASSERT(foe);
        return ai_action::good_or_bad(grid_distance(mon->pos(), foe->pos())
                                      > mon->reach_range());

    case SPELL_BATTLECRY:
        return ai_action::good_or_bad(_battle_cry(*mon, SPELL_BATTLECRY,
                                      true));

    case SPELL_WARNING_CRY:
        return ai_action::good_or_bad(!friendly);

    case SPELL_HUNTING_CALL:
        return ai_action::good_or_bad(_battle_cry(*mon, SPELL_HUNTING_CALL,
                                      true));

    case SPELL_FUNERAL_DIRGE:
        return ai_action::good_or_bad(_cast_dirge(*mon, true));

    case SPELL_CONJURE_BALL_LIGHTNING:
        if (!!(mon->holiness() & MH_DEMONIC))
            return ai_action::good(); // demons are rude
        return ai_action::good_or_bad(
                        !(friendly && (you.res_elec() <= 0 || you.hp <= 50)));

    case SPELL_SEAL_DOORS:
        return ai_action::good_or_bad(!friendly && _seal_doors_and_stairs(mon, true));

    //XXX: unify with the other SPELL_FOO_OTHER spells?
    case SPELL_BERSERK_OTHER:
        return ai_action::good_or_bad(_incite_monsters(mon, false));

    case SPELL_CAUSE_FEAR:
        return ai_action::good_or_bad(_mons_cause_fear(mon, false) >= 0);

    case SPELL_MASS_CONFUSION:
        return ai_action::good_or_bad(_mons_mass_confuse(mon, false) >= 0);

    case SPELL_THROW_ALLY:
        return ai_action::good_or_impossible(_find_ally_to_throw(*mon));

    case SPELL_CREATE_TENTACLES:
        return ai_action::good_or_impossible(mons_available_tentacles(mon));

    case SPELL_WORD_OF_RECALL:
        return ai_action::good_or_bad(_should_recall(mon));

    case SPELL_SHATTER:
        return ai_action::good_or_bad(!friendly && mons_shatter(mon, false));

    case SPELL_SYMBOL_OF_TORMENT:
        if (you.visible_to(mon)
            && friendly
            && !you.res_torment()
            && !player_kiku_res_torment())
        {
            return ai_action::bad();
        }
        else
            return ai_action::good_or_bad(_trace_los(mon, _torment_vulnerable));

    case SPELL_CHAIN_LIGHTNING:
        if (you.visible_to(mon) && friendly)
            return ai_action::bad(); // don't zap player
        return ai_action::good_or_bad(_trace_los(mon, [] (const actor * /*a*/)
                                                          { return true; }));

    case SPELL_CHAIN_OF_CHAOS:
        if (you.visible_to(mon) && friendly)
            return ai_action::bad(); // don't zap player
        return ai_action::good_or_bad(_trace_los(mon, [] (const actor * /*a*/)
                                                          { return true; }));

    case SPELL_CORRUPTING_PULSE:
        if (you.visible_to(mon) && friendly)
            return ai_action::bad(); // don't zap player
        else
            return ai_action::good_or_bad(_trace_los(mon, _mutation_vulnerable));

    case SPELL_POLAR_VORTEX:
        if (mon->has_ench(ENCH_POLAR_VORTEX) || mon->has_ench(ENCH_POLAR_VORTEX_COOLDOWN))
            return ai_action::impossible();
        else if (you.visible_to(mon) && friendly && !(mon->holiness() & MH_DEMONIC))
            return ai_action::bad(); // don't zap player (but demons are rude)
        else
            return ai_action::good_or_bad(_trace_los(mon, _vortex_vulnerable));

    case SPELL_ENGLACIATION:
        return ai_action::good_or_bad(foe && mon->see_cell_no_trans(foe->pos())
                                        && foe->res_cold() <= 0);

    case SPELL_OLGREBS_TOXIC_RADIANCE:
        if (mon->has_ench(ENCH_TOXIC_RADIANCE))
            return ai_action::impossible();
        else
        {
            return ai_action::good_or_bad(
                cast_toxic_radiance(mon, 100, false, true) == spret::success);
        }

    case SPELL_IGNITE_POISON:
        return ai_action::good_or_bad(
            cast_ignite_poison(mon, 0, false, true) == spret::success);

    case SPELL_GLACIATE:
        ASSERT(foe);
        return ai_action::good_or_bad(
            _glaciate_tracer(mon, mons_spellpower(*mon, spell), foe->pos()));

    case SPELL_MALIGN_GATEWAY:
        return ai_action::good_or_bad(can_cast_malign_gateway());

    case SPELL_SIREN_SONG:
        return _siren_goodness(mon, false);

    case SPELL_AVATAR_SONG:
        return _siren_goodness(mon, true);

    case SPELL_REPEL_MISSILES:
        return ai_action::good_or_impossible(!mon->has_ench(ENCH_REPEL_MISSILES));

    case SPELL_CONFUSION_GAZE:
        // why is this handled here unlike other gazes?
        return _caster_sees_foe(*mon);

    case SPELL_CLEANSING_FLAME:
    {
        bolt tracer;
        setup_cleansing_flame_beam(tracer,
                                   5 + (7 * mon->spell_hd(spell)) / 12,
                                   cleansing_flame_source::spell,
                                   mon->pos(), mon);
        fire_tracer(mon, tracer, true);
        return ai_action::good_or_bad(mons_should_fire(tracer));
    }

    case SPELL_DOOM_HOWL:
        ASSERT(foe);
        if (!foe->is_player() || you.duration[DUR_DOOM_HOWL]
                || mon->props[DOOM_HOUND_HOWLED_KEY]
                || mon->is_summoned())
        {
            return ai_action::impossible();
        }
        return ai_action::good();

    case SPELL_CALL_OF_CHAOS:
        return ai_action::good_or_bad(_mons_call_of_chaos(*mon, true));

    case SPELL_PRAYER_OF_BRILLIANCE:
        if (!foe || !mon->can_see(*foe))
            return ai_action::bad();

        for (monster_near_iterator mi(mon, LOS_NO_TRANS); mi; ++mi)
            if (_valid_prayer_of_brilliance_ally(mon, *mi)
                && !mi->has_ench(ENCH_EMPOWERED_SPELLS))
            {
                return ai_action::good();
            }
        return ai_action::bad();

    case SPELL_BIND_SOULS:
        for (monster_near_iterator mi(mon, LOS_NO_TRANS); mi; ++mi)
        {
            if (*mi == mon)
                continue;
            if (mons_can_bind_soul(mon, *mi))
                return ai_action::good();
        }
        return ai_action::bad();

    case SPELL_REGENERATE_OTHER:
    case SPELL_MASS_REGENERATION:
        return _ally_needs_regeneration(*mon);

    case SPELL_POISONOUS_CLOUD:
    case SPELL_FREEZING_CLOUD:
    case SPELL_MEPHITIC_CLOUD:
    case SPELL_NOXIOUS_CLOUD:
    case SPELL_SPECTRAL_CLOUD:
    case SPELL_FLAMING_CLOUD:
    case SPELL_CHAOS_BREATH:
    case SPELL_DEATH_RATTLE:
    case SPELL_MIASMA_BREATH:
    case SPELL_RAVENOUS_SWARM:
        return ai_action::good_or_impossible(!no_clouds);

    case SPELL_MARCH_OF_SORROWS:
        ASSERT(foe);
        if (_negative_energy_spell_goodness(foe) == ai_action::good())
            return ai_action::good_or_impossible(!no_clouds);
        else return ai_action::bad();

    case SPELL_ROLL:
    {
        const coord_def delta = mon->props[MMOV_KEY].get_coord();
        // We can roll if we have a foe we're not already adjacent to and where
        // we have a move that brings us closer.
        return ai_action::good_or_bad(
                mon->get_foe()
                && !adjacent(mon->pos(), mon->get_foe()->pos())
                && !delta.origin()
                && mon_can_move_to_pos(mon, delta));
    }

    case SPELL_MANIFOLD_ASSAULT:
        return ai_action::good_or_impossible(
                cast_manifold_assault(*mon, -1, false, false) != spret::abort);

    case SPELL_BESTOW_ARMS:
    {
        // Don't use while on cooldown
        if (mon->props.exists(ARMS_COOLDOWN_KEY)
            && you.elapsed_time < mon->props[ARMS_COOLDOWN_KEY].get_int())
        {
            return ai_action::impossible();
        }

        for (monster_near_iterator mi(mon->pos(), LOS_NO_TRANS); mi; ++mi)
        {
            if (mons_aligned(mon, *mi) && !mi->has_ench(ENCH_ARMED)
                && (mons_itemuse(**mi) >= MONUSE_STARTING_EQUIPMENT)
                && _bestow_arms_attack_types_valid(*mi))
            {
                return ai_action::good();
            }
        }

        return ai_action::impossible();
    }

    case SPELL_FULMINANT_PRISM:
    {
        // Only place prisms if none are already out
        for (monster_iterator mi; mi; ++mi)
        {
            if (mi->type == MONS_FULMINANT_PRISM && mi->summoner == mon->mid)
                return ai_action::bad();
        }

        return ai_action::good_or_impossible(
            _mons_cast_prisms(*mon, *mon->get_foe(), 100, true));
    }

    case SPELL_HELLFIRE_MORTAR:
    {
        return ai_action::good_or_impossible(
            _mons_cast_hellfire_mortar(*mon, *mon->get_foe(), 100, true));
    }

#if TAG_MAJOR_VERSION == 34
    case SPELL_SUMMON_SWARM:
    case SPELL_INNER_FLAME:
    case SPELL_ANIMATE_DEAD:
    case SPELL_SIMULACRUM:
    case SPELL_DEATHS_DOOR:
    case SPELL_OZOCUBUS_ARMOUR:
    case SPELL_TELEPORT_SELF:
#endif
    case SPELL_NO_SPELL:
        return ai_action::bad();

    default:
        // is this the right approach for this case? There are currently around
        // 280 spells not handled in this switch, most of them purely
        // targeted, removed, or handled via spell_to_logic.
        return ai_action::good();
    }
}

static ai_action::goodness _monster_spell_goodness(monster* mon, mon_spell_slot slot)
{
    if (slot.flags & MON_SPELL_BREATH && mon->has_ench(ENCH_BREATH_WEAPON))
        return ai_action::impossible();

    return monster_spell_goodness(mon, slot.spell);
}

static string _god_name(god_type god)
{
    return god_has_name(god) ? god_name(god) : "Something";
}
