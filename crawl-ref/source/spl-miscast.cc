/**
 * @file
 * @brief Spell miscast class.
**/

#include "AppHdr.h"

#include "spl-miscast.h"

#include "externs.h"

#include <sstream>

#include "branch.h"
#include "colour.h"
#include "cloud.h"
#include "directn.h"
#include "effects.h"
#include "env.h"
#include "itemprop.h"
#include "kills.h"
#include "libutil.h"
#include "mapmark.h"
#include "message.h"
#include "misc.h"
#include "mon-cast.h"
#include "mon-place.h"
#include "mgen_data.h"
#include "mon-stuff.h"
#include "mon-util.h"
#include "mutation.h"
#include "player.h"
#include "player-stats.h"
#include "potion.h"
#include "religion.h"
#include "spl-clouds.h"
#include "spl-summoning.h"
#include "state.h"
#include "stuff.h"
#include "areas.h"
#include "terrain.h"
#include "transform.h"
#include "view.h"
#include "shout.h"
#include "unwind.h"
#include "viewchar.h"
#include "xom.h"

// This determines how likely it is that more powerful wild magic
// effects will occur.  Set to 100 for the old probabilities (although
// the individual effects have been made much nastier since then).
#define WILD_MAGIC_NASTINESS 150

#define MAX_RECURSE 100

MiscastEffect::MiscastEffect(actor* _target, int _source, spell_type _spell,
                             int _pow, int _fail, string _cause,
                             nothing_happens_when_type _nothing_happens,
                             int _lethality_margin, string _hand_str,
                             bool _can_plural) :
    target(_target), source(_source), cause(_cause), spell(_spell),
    school(SPTYP_NONE), pow(_pow), fail(_fail), level(-1), kc(KC_NCATEGORIES),
    kt(KILL_NONE), nothing_happens_when(_nothing_happens),
    lethality_margin(_lethality_margin), hand_str(_hand_str),
    can_plural_hand(_can_plural)
{
    ASSERT(is_valid_spell(_spell));
    unsigned int schools = get_spell_disciplines(_spell);
    ASSERT(schools != SPTYP_NONE);
    UNUSED(schools);

    init();
    do_miscast();
}

MiscastEffect::MiscastEffect(actor* _target, int _source,
                             spschool_flag_type _school, int _level,
                             string _cause,
                             nothing_happens_when_type _nothing_happens,
                             int _lethality_margin, string _hand_str,
                             bool _can_plural) :
    target(_target), source(_source), cause(_cause), spell(SPELL_NO_SPELL),
    school(_school), pow(-1), fail(-1), level(_level), kc(KC_NCATEGORIES),
    kt(KILL_NONE), nothing_happens_when(_nothing_happens),
    lethality_margin(_lethality_margin), hand_str(_hand_str),
    can_plural_hand(_can_plural)
{
    ASSERT(!_cause.empty());
    ASSERT(count_bits(_school) == 1);
    ASSERT(_school <= SPTYP_LAST_SCHOOL || _school == SPTYP_RANDOM);
    ASSERT_RANGE(level, 0, 3 + 1);

    init();
    do_miscast();
}

MiscastEffect::MiscastEffect(actor* _target, int _source,
                             spschool_flag_type _school, int _pow, int _fail,
                             string _cause,
                             nothing_happens_when_type _nothing_happens,
                             int _lethality_margin, string _hand_str,
                             bool _can_plural) :
    target(_target), source(_source), cause(_cause), spell(SPELL_NO_SPELL),
    school(_school), pow(_pow), fail(_fail), level(-1), kc(KC_NCATEGORIES),
    kt(KILL_NONE), nothing_happens_when(_nothing_happens),
    lethality_margin(_lethality_margin), hand_str(_hand_str),
    can_plural_hand(_can_plural)
{
    ASSERT(!_cause.empty());
    ASSERT(count_bits(_school) == 1);
    ASSERT(_school <= SPTYP_LAST_SCHOOL || _school == SPTYP_RANDOM);

    init();
    do_miscast();
}

MiscastEffect::~MiscastEffect()
{
    ASSERT(recursion_depth == 0);
}

void MiscastEffect::init()
{
    ASSERT(spell != SPELL_NO_SPELL && school == SPTYP_NONE
           || spell == SPELL_NO_SPELL && school != SPTYP_NONE);
    ASSERT(pow != -1 && fail != -1 && level == -1
           || pow == -1 && fail == -1 && level >= 0 && level <= 3);

    ASSERT(target != NULL);
    ASSERT(target->alive());

    ASSERT(lethality_margin == 0 || target->is_player());

    recursion_depth = 0;

    source_known = target_known = false;

    act_source = guilty = NULL;

    const bool death_curse = (cause.find("death curse") != string::npos);

    if (target->is_monster())
        target_known = you.can_see(target);
    else
        target_known = true;

    kill_source = source;
    if (source == WIELD_MISCAST || source == MELEE_MISCAST)
    {
        if (target->is_monster())
            kill_source = target->mindex();
        else
            kill_source = NON_MONSTER;
    }

    if (kill_source == NON_MONSTER)
    {
        kc           = KC_YOU;
        kt           = KILL_YOU_MISSILE;
        act_source   = &you;
        guilty       = &you;
        source_known = true;
    }
    else if (!invalid_monster_index(kill_source))
    {
        monster* mon_source = &menv[kill_source];
        ASSERT(mon_source->type != MONS_NO_MONSTER);

        act_source = guilty = mon_source;

        if (death_curse && target->is_monster()
            && target_as_monster()->confused_by_you())
        {
            kt = KILL_YOU_CONF;
        }
        else if (!death_curse && mon_source->confused_by_you()
                 && !mon_source->friendly())
        {
            kt = KILL_YOU_CONF;
        }
        else
            kt = KILL_MON_MISSILE;

        if (mon_source->friendly())
            kc = KC_FRIENDLY;
        else
            kc = KC_OTHER;

        source_known = you.can_see(mon_source);

        if (target_known && death_curse)
            source_known = true;
    }
    else
    {
        ASSERT(source == ZOT_TRAP_MISCAST
               || source == HELL_EFFECT_MISCAST
               || source == MISC_MISCAST
               || (source < 0 && -source < NUM_GODS));

        act_source = target;

        kc = KC_OTHER;
        kt = KILL_MISCAST;

        if (source == ZOT_TRAP_MISCAST)
        {
            source_known = target_known;

            if (target->is_monster()
                && target_as_monster()->confused_by_you())
            {
                kt = KILL_YOU_CONF;
            }
        }
        else if (source == MISC_MISCAST)
            source_known = true, guilty = &you;
        else
            source_known = true;
    }

    ASSERT(kc != KC_NCATEGORIES);
    ASSERT(kt != KILL_NONE);

    if (death_curse && !invalid_monster_index(kill_source))
    {
        if (starts_with(cause, "a "))
            cause.replace(cause.begin(), cause.begin() + 1, "an indirect");
        else if (starts_with(cause, "an "))
            cause.replace(cause.begin(), cause.begin() + 2, "an indirect");
        else
            cause = replace_all(cause, "death curse", "indirect death curse");
    }

    // source_known = false for MELEE_MISCAST so that melee miscasts
    // won't give a "nothing happens" message.
    if (source == MELEE_MISCAST)
        source_known = false;

    if (hand_str.empty())
        target->hand_name(true, &can_plural_hand);

    // Explosion stuff.
    beam.is_explosion = true;

    // [ds] Don't attribute the beam's cause to the actor, because the
    // death message will name the actor anyway.
    if (cause.empty())
        cause = get_default_cause(false);
    beam.aux_source  = cause;
    beam.beam_source = (0 <= kill_source && kill_source <= MHITYOU)
                       ? kill_source : MHITNOT;
    beam.thrower     = kt;
}

string MiscastEffect::get_default_cause(bool attribute_to_user) const
{
    // This is only for true miscasts, which means both a spell and that
    // the source of the miscast is the same as the target of the miscast.
    ASSERT_RANGE(source, 0, NON_MONSTER + 1);
    ASSERT(spell != SPELL_NO_SPELL);
    ASSERT(school == SPTYP_NONE);

    if (source == NON_MONSTER)
    {
        ASSERT(target->is_player());
        string str = "miscasting ";
        str += spell_title(spell);
        return str;
    }

    ASSERT(act_source->is_monster());
    ASSERT(act_source == target);

    if (attribute_to_user)
    {
        return (string(you.can_see(act_source) ? act_source->name(DESC_A)
                                               : "something")
                + " miscasting " + spell_title(spell));
    }
    else
        return string("miscast of ") + spell_title(spell);
}

bool MiscastEffect::neither_end_silenced()
{
    return (!silenced(you.pos()) && !silenced(target->pos()));
}

void MiscastEffect::do_miscast()
{
    ASSERT_RANGE(recursion_depth, 0, MAX_RECURSE);

    if (recursion_depth == 0)
        did_msg = false;

    unwind_var<int> unwind_depth(recursion_depth);
    recursion_depth++;

    // Repeated calls to do_miscast() on a single object instance have
    // killed a target which was alive when the object was created.
    if (!target->alive())
    {
        dprf("Miscast target '%s' already dead",
             target->name(DESC_PLAIN, true).c_str());
        return;
    }

    spschool_flag_type sp_type;
    int                severity;

    if (spell != SPELL_NO_SPELL)
    {
        vector<int> school_list;
        for (int i = 0; i <= SPTYP_LAST_EXPONENT; i++)
            if (spell_typematch(spell, 1 << i))
                school_list.push_back(i);

        unsigned int _school = school_list[random2(school_list.size())];
        sp_type = static_cast<spschool_flag_type>(1 << _school);
    }
    else
    {
        sp_type = school;
        if (sp_type == SPTYP_RANDOM)
        {
            int exp = (random2(SPTYP_LAST_EXPONENT));
            sp_type = (spschool_flag_type) (1 << exp);
        }
    }

    if (level != -1)
        severity = level;
    else
    {
        severity = (pow * fail * (10 + pow) / 7 * WILD_MAGIC_NASTINESS) / 100;

#if defined(DEBUG_DIAGNOSTICS) || defined(DEBUG_MISCAST)
        mprf(MSGCH_DIAGNOSTICS, "'%s' miscast power: %d",
             spell != SPELL_NO_SPELL ? spell_title(spell)
                                     : spelltype_short_name(sp_type),
             severity);
#endif

        if (random2(40) > severity && random2(40) > severity)
        {
            if (target->is_player())
                canned_msg(MSG_NOTHING_HAPPENS);
            return;
        }

        severity /= 100;
        severity = random2(severity);
        if (severity > 3)
            severity = 3;
        else if (severity < 0)
            severity = 0;
    }

#if defined(DEBUG_DIAGNOSTICS) || defined(DEBUG_MISCAST)
    mprf(MSGCH_DIAGNOSTICS, "Sptype: %s, severity: %d",
         spelltype_short_name(sp_type), severity);
#endif

    beam.ex_size            = 1;
    beam.name               = "";
    beam.damage             = dice_def(0, 0);
    beam.flavour            = BEAM_NONE;
    beam.msg_generated      = false;
    beam.in_explosion_phase = false;

    // Do this here since multiple miscasts (wizmode testing) might move
    // the target around.
    beam.source = target->pos();
    beam.target = target->pos();
    beam.use_target_as_pos = true;

    all_msg        = you_msg = mon_msg = mon_msg_seen = mon_msg_unseen = "";
    msg_ch         = MSGCH_PLAIN;
    sound_loudness = 0;

    if (source == ZOT_TRAP_MISCAST)
    {
        _zot();
        if (target->is_player())
            xom_is_stimulated(150);
        return;
    }

    switch (sp_type)
    {
    case SPTYP_CONJURATION:    _conjuration(severity);    break;
    case SPTYP_HEXES:
    case SPTYP_CHARMS:         _enchantment(severity);    break;
    case SPTYP_TRANSLOCATION:  _translocation(severity);  break;
    case SPTYP_SUMMONING:      _summoning(severity);      break;
    case SPTYP_NECROMANCY:     _necromancy(severity);     break;
    case SPTYP_TRANSMUTATION:  _transmutation(severity);  break;
    case SPTYP_FIRE:           _fire(severity);           break;
    case SPTYP_ICE:            _ice(severity);            break;
    case SPTYP_EARTH:          _earth(severity);          break;
    case SPTYP_AIR:            _air(severity);            break;
    case SPTYP_POISON:         _poison(severity);         break;
    case SPTYP_DIVINATION:
        // Divination miscasts have nothing in common between the player
        // and monsters.
        if (target->is_player())
            _divination_you(severity);
        else
            _divination_mon(severity);
        break;

    default:
        die("Invalid miscast spell discipline.");
    }

    if (target->is_player())
        xom_is_stimulated(severity * 50);
}

void MiscastEffect::do_msg(bool suppress_nothing_happens)
{
    ASSERT(!did_msg);

    if (target->is_monster() && !mons_near(target_as_monster()))
        return;

    did_msg = true;

    string msg;

    if (!all_msg.empty())
        msg = all_msg;
    else if (target->is_player())
        msg = you_msg;
    else if (!mon_msg.empty())
    {
        msg = mon_msg;
        // Monster might be unseen with hands that can't be seen.
        ASSERT(msg.find("@hand") == string::npos);
    }
    else
    {
        if (you.can_see(target))
            msg = mon_msg_seen;
        else
        {
            msg = mon_msg_unseen;
            // Can't see the hands of invisible monsters.
            ASSERT(msg.find("@hand") == string::npos);
        }
    }

    if (msg.empty())
    {
        if (!suppress_nothing_happens
            && (nothing_happens_when == NH_ALWAYS
                || (nothing_happens_when == NH_DEFAULT && source_known
                    && target_known)))
        {
            canned_msg(MSG_NOTHING_HAPPENS);
        }

        return;
    }

    bool plural;

    if (hand_str.empty())
    {
        msg = replace_all(msg, "@hand@",  target->hand_name(false, &plural));
        msg = replace_all(msg, "@hands@", target->hand_name(true));
    }
    else
    {
        plural = can_plural_hand;
        msg = replace_all(msg, "@hand@",  hand_str);
        if (can_plural_hand)
            msg = replace_all(msg, "@hands@", pluralise(hand_str));
        else
            msg = replace_all(msg, "@hands@", hand_str);
    }

    if (plural)
        msg = replace_all(msg, "@hand_conj@", "");
    else
        msg = replace_all(msg, "@hand_conj@", "s");

    if (target->is_monster())
    {
        msg = do_mon_str_replacements(msg, target_as_monster(), S_SILENT);
        if (!mons_has_body(target_as_monster()))
            msg = replace_all(msg, "'s body", "");
    }

    mpr(msg.c_str(), msg_ch);

    if (msg_ch == MSGCH_SOUND)
    {
        // Those monsters of normal or greater intelligence will realize that they
        // were the source of the sound.
        int src = target->is_player() ? you.mindex()
                : mons_intel(target_as_monster()) >= I_NORMAL ? target->mindex()
                : -1;
        noisy(sound_loudness, target->pos(), src);
    }
}

bool MiscastEffect::_ouch(int dam, beam_type flavour)
{
    // Delay do_msg() until after avoid_lethal().
    if (target->is_monster())
    {
        monster* mon_target = target_as_monster();

        do_msg(true);

        bolt beem;

        beem.flavour = flavour;
        dam = mons_adjust_flavoured(mon_target, beem, dam, true);
        mon_target->hurt(guilty, dam, BEAM_MISSILE, false);

        if (!mon_target->alive())
            monster_die(mon_target, kt, kill_source);
    }
    else
    {
        dam = check_your_resists(dam, flavour, cause);

        if (avoid_lethal(dam))
            return false;

        do_msg(true);

        kill_method_type method;

        if (source == NON_MONSTER && spell != SPELL_NO_SPELL)
            method = KILLED_BY_WILD_MAGIC;
        else if (source == ZOT_TRAP_MISCAST)
            method = KILLED_BY_TRAP;
        else if (source < 0 && -source < NUM_GODS)
        {
            god_type god = static_cast<god_type>(-source);

            if (god == GOD_XOM && !player_under_penance(GOD_XOM))
                method = KILLED_BY_XOM;
            else
                method = KILLED_BY_DIVINE_WRATH;
        }
        else
            method = KILLED_BY_SOMETHING;

        bool see_source = act_source && you.can_see(act_source);
        ouch(dam, kill_source, method, cause.c_str(), see_source);
    }

    return true;
}

bool MiscastEffect::_explosion()
{
    ASSERT(!beam.name.empty());
    ASSERT(beam.damage.num != 0);
    ASSERT(beam.damage.size != 0);
    ASSERT(beam.flavour != BEAM_NONE);

    int max_dam = beam.damage.num * beam.damage.size;
    max_dam = check_your_resists(max_dam, beam.flavour, cause);
    if (avoid_lethal(max_dam))
        return false;

    do_msg(true);
    beam.explode();

    return true;
}

bool MiscastEffect::_big_cloud(cloud_type cl_type, int cloud_pow, int size,
                               int spread_rate)
{
    if (avoid_lethal(2 * max_cloud_damage(cl_type, cloud_pow)))
        return false;

    do_msg(true);
    big_cloud(cl_type, guilty, target->pos(), cloud_pow, size, spread_rate);

    return true;
}

bool MiscastEffect::_lose_stat(stat_type which_stat, int8_t stat_loss)
{
    return lose_stat(which_stat, stat_loss, false, cause);
}

void MiscastEffect::_potion_effect(potion_type pot_eff, int pot_pow)
{
    if (target->is_player())
    {
        potion_effect(pot_eff, pot_pow, false, false);
        return;
    }

    switch (pot_eff)
    {
        case POT_BERSERK_RAGE:
            if (target->can_go_berserk())
            {
                target->go_berserk(false);
                break;
            }
            // Intentional fallthrough if that didn't work.
        case POT_SLOWING:
            target->slow_down(act_source, pot_pow);
            break;
        case POT_PARALYSIS:
            target->paralyse(act_source, pot_pow, cause);
            break;
        case POT_CONFUSION:
            target->confuse(act_source, pot_pow);
            break;

        default:
            die("unknown potion effect");
    }
}

bool MiscastEffect::_send_to_abyss()
{
    if ((player_in_branch(BRANCH_ABYSS) && x_chance_in_y(you.depth, brdepth[BRANCH_ABYSS]))
        || source == HELL_EFFECT_MISCAST)
    {
        return _malign_gateway(); // attempt to degrade to malign gateway
    }
    target->banish(act_source, cause);
    return true;
}

// XXX: Mostly duplicated from cast_malign_gateway.
bool MiscastEffect::_malign_gateway()
{
    coord_def point = find_gateway_location(&you);
    bool success = (point != coord_def(0, 0));

    if (success)
    {
        const int malign_gateway_duration = BASELINE_DELAY * (random2(5) + 5);
        env.markers.add(new map_malign_gateway_marker(point,
                                malign_gateway_duration,
                                false,
                                cause,
                                BEH_HOSTILE,
                                GOD_NO_GOD,
                                200));
        env.markers.clear_need_activate();
        env.grid(point) = DNGN_MALIGN_GATEWAY;

        noisy(10, point);
        all_msg = "The dungeon shakes, a horrible noise fills the air, and a "
                  "portal to some otherworldly place is opened!";
        msg_ch = MSGCH_WARN;
        do_msg();
    }

    return success;
}

bool MiscastEffect::avoid_lethal(int dam)
{
    if (lethality_margin <= 0 || (you.hp - dam) > lethality_margin)
        return false;

    if (recursion_depth == MAX_RECURSE)
    {
        // Any possible miscast would kill you, now that's interesting.
        if (you_worship(GOD_XOM))
            simple_god_message(" watches you with interest.");
        return true;
    }

    if (did_msg)
    {
#if defined(DEBUG_DIAGNOSTICS) || defined(DEBUG_MISCAST)
        mpr("Couldn't avoid lethal miscast: already printed message for this "
            "miscast.", MSGCH_ERROR);
#endif
        return false;
    }

#if defined(DEBUG_DIAGNOSTICS) || defined(DEBUG_MISCAST)
    mpr("Avoided lethal miscast.", MSGCH_DIAGNOSTICS);
#endif

    do_miscast();

    return true;
}

bool MiscastEffect::_create_monster(monster_type what, int abj_deg,
                                    bool alert)
{
    const god_type god =
        (crawl_state.is_god_acting()) ? crawl_state.which_god_acting()
                                      : GOD_NO_GOD;

    if (cause.empty())
        cause = get_default_cause(true);
    mgen_data data = mgen_data::hostile_at(what, cause, alert,
                                           abj_deg, 0, target->pos(), 0, god);

    if (source != HELL_EFFECT_MISCAST)
        data.extra_flags |= (MF_NO_REWARD | MF_HARD_RESET);

    // hostile_at() assumes the monster is hostile to the player,
    // but should be hostile to the target monster unless the miscast
    // is a result of either divine wrath or a Zot trap.
    if (target->is_monster() && !player_under_penance(god)
        && source != ZOT_TRAP_MISCAST)
    {
        monster* mon_target = target_as_monster();

        switch (mon_target->temp_attitude())
        {
            case ATT_FRIENDLY:     data.behaviour = BEH_HOSTILE; break;
            case ATT_HOSTILE:      data.behaviour = BEH_FRIENDLY; break;
            case ATT_GOOD_NEUTRAL:
            case ATT_NEUTRAL:
            case ATT_STRICT_NEUTRAL:
                data.behaviour = BEH_NEUTRAL;
            break;
        }

        if (alert)
            data.foe = mon_target->mindex();

        // No permanent allies from miscasts.
        if (data.behaviour == BEH_FRIENDLY && abj_deg == 0)
            data.abjuration_duration = 6;
    }

    // If data.abjuration_duration == 0, then data.summon_type will
    // simply be ignored.
    if (data.abjuration_duration != 0)
    {
        if (what == RANDOM_MOBILE_MONSTER)
            data.summon_type = SPELL_SHADOW_CREATURES;
        else if (player_under_penance(god))
            data.summon_type = MON_SUMM_WRATH;
        else if (source == ZOT_TRAP_MISCAST)
            data.summon_type = MON_SUMM_ZOT;
        else
            data.summon_type = MON_SUMM_MISCAST;
    }

    return create_monster(data);
}

// hair or hair-equivalent (like bandages)
static bool _has_hair(actor* target)
{
    // Don't bother for monsters.
    if (target->is_monster())
        return false;

    return (!form_changed_physiology() && you.species != SP_GHOUL
            && you.species != SP_OCTOPODE
            && you.species != SP_TENGU && !player_genus(GENPC_DRACONIAN)
            && you.species != SP_GARGOYLE && you.species != SP_LAVA_ORC);
}

static string _hair_str(actor* target, bool &plural)
{
    ASSERT(target->is_player());

    if (you.species == SP_MUMMY)
    {
        plural = true;
        return "bandages";
    }
    else
    {
        plural = false;
        return "hair";
    }
}

void MiscastEffect::_conjuration(int severity)
{
    int num;
    switch (severity)
    {
    case 0:         // just a harmless message
        num = 10 + (_has_hair(target) ? 1 : 0);
        switch (random2(num))
        {
        case 0:
            you_msg      = "Sparks fly from your @hands@!";
            mon_msg_seen = "Sparks fly from @the_monster@'s @hands@!";
            break;
        case 1:
            you_msg      = "The air around you crackles with energy!";
            mon_msg_seen = "The air around @the_monster@ crackles "
                           "with energy!";
            break;
        case 2:
            you_msg      = "Wisps of smoke drift from your @hands@.";
            mon_msg_seen = "Wisps of smoke drift from @the_monster@'s "
                           "@hands@.";
            break;
        case 3:
            you_msg = "You feel a strange surge of energy!";
            // Monster messages needed.
            break;
        case 4:
            you_msg      = "You are momentarily dazzled by a flash of light!";
            mon_msg_seen = "@The_monster@ emits a flash of light!";
            break;
        case 5:
            you_msg      = "Strange energies run through your body.";
            mon_msg_seen = "@The_monster@ glows " + weird_glowing_colour() +
                           " for a moment.";
            break;
        case 6:
            you_msg      = "Your skin tingles.";
            mon_msg_seen = "@The_monster@ twitches.";
            break;
        case 7:
            you_msg      = "Your skin glows momentarily.";
            mon_msg_seen = "@The_monster@ glows momentarily.";
            // A small glow isn't going to make it past invisibility.
            break;
        case 8:
            // Set nothing; canned_msg(MSG_NOTHING_HAPPENS) will be taken
            // care of elsewhere.
            break;
        case 9:
            if (you.can_smell())
                all_msg = "You smell something strange.";
            else if (you.species == SP_MUMMY)
                you_msg = "Your bandages flutter.";
            break;
        case 10:
        {
            // Player only (for now).
            bool plural;
            string hair = _hair_str(target, plural);
            you_msg = make_stringf("Your %s stand%s on end.", hair.c_str(),
                                   plural ? "" : "s");
        }
        }
        do_msg();
        break;

    case 1:         // a bit less harmless stuff
        switch (random2(2))
        {
        case 0:
            you_msg        = "Smoke pours from your @hands@!";
            mon_msg_seen   = "Smoke pours from @the_monster@'s @hands@!";
            mon_msg_unseen = "Smoke appears from out of nowhere!";

            _big_cloud(CLOUD_GREY_SMOKE, 20, 7 + random2(7));
            break;
        case 1:
            you_msg      = "A wave of violent energy washes through your body!";
            mon_msg_seen = "@The_monster@ lurches violently!";
            _ouch(6 + random2avg(7, 2));
            break;
        }
        break;

    case 2:         // rather less harmless stuff
        switch (random2(2))
        {
        case 0:
            you_msg      = "Energy rips through your body!";
            mon_msg_seen = "@The_monster@ jerks violently!";
            _ouch(9 + random2avg(17, 2));
            break;
        case 1:
            you_msg        = "You are caught in a violent explosion!";
            mon_msg_seen   = "@The_monster@ is caught in a violent explosion!";
            mon_msg_unseen = "A violent explosion happens from out of thin "
                             "air!";

            beam.flavour = BEAM_MISSILE;
            beam.damage  = dice_def(3, 12);
            beam.name    = "explosion";
            beam.colour  = random_colour();

            _explosion();
            break;
        }
        break;

    case 3:         // considerably less harmless stuff
        switch (random2(2))
        {
        case 0:
            you_msg      = "You are blasted with magical energy!";
            mon_msg_seen = "@The_monster@ is blasted with magical energy!";
            // No message for invis monsters?
            _ouch(12 + random2avg(29, 2));
            break;
        case 1:
            all_msg = "There is a sudden explosion of magical energy!";

            beam.flavour = BEAM_MISSILE;
            beam.glyph   = dchar_glyph(DCHAR_FIRED_BURST);
            beam.damage  = dice_def(3, 20);
            beam.name    = "explosion";
            beam.colour  = random_colour();
            beam.ex_size = coinflip() ? 1 : 2;

            _explosion();
            break;
        }
    }
}

static void _your_hands_glow(actor* target, string& you_msg,
                             string& mon_msg_seen, bool pluralise)
{
    you_msg      = "Your @hands@ ";
    mon_msg_seen = "@The_monster@'s @hands@ ";
    // No message for invisible monsters.

    if (pluralise)
    {
        you_msg      += "glow";
        mon_msg_seen += "glow";
    }
    else
    {
        you_msg      += "glows";
        mon_msg_seen += "glows";
    }

    you_msg      += " momentarily.";
    mon_msg_seen += " momentarily.";
}

void MiscastEffect::_enchantment(int severity)
{
    switch (severity)
    {
    case 0:         // harmless messages only
        switch (random2(10))
        {
        case 0:
            _your_hands_glow(target, you_msg, mon_msg_seen, can_plural_hand);
            break;
        case 1:
            you_msg      = "The air around you crackles with energy!";
            mon_msg_seen = "The air around @the_monster@ crackles with"
                           " energy!";
            break;
        case 2:
            you_msg        = "Multicoloured lights dance before your eyes!";
            mon_msg_seen   = "Multicoloured lights dance around @the_monster@!";
            mon_msg_unseen = "Multicoloured lights dance in the air!";
            break;
        case 3:
            you_msg = "You feel a strange surge of energy!";
            // Monster messages needed.
            break;
        case 4:
            you_msg      = "Waves of light ripple over your body.";
            mon_msg_seen = "Waves of light ripple over @the_monster@'s body.";
            break;
        case 5:
            you_msg      = "Strange energies run through your body.";
            mon_msg_seen = "@The_monster@ glows " + weird_glowing_colour() +
                           " for a moment.";
            break;
        case 6:
            you_msg      = "Your skin tingles.";
            mon_msg_seen = "@The_monster@ twitches.";
            break;
        case 7:
            you_msg      = "Your skin glows momentarily.";
            mon_msg_seen = "@The_monster@'s body glows momentarily.";
            break;
        case 8:
            // Set nothing; canned_msg(MSG_NOTHING_HAPPENS) will be taken
            // care of elsewhere.
            break;
        case 9:
            if (neither_end_silenced())
            {
                all_msg        = "You hear something strange.";
                msg_ch         = MSGCH_SOUND;
                sound_loudness = 2;
                return;
            }
            else if (target->is_player())
                if (you.species == SP_OCTOPODE)
                    you_msg = "Your beak vibrates slightly."; // the only hard part
                else
                    you_msg = "Your skull vibrates slightly.";
            break;
        }
        do_msg();
        break;

    case 1:         // slightly annoying
        switch (random2(target->is_player() ? 2 : 1))
        {
        case 0:
            if (target->is_player())
                you.backlight();
            else
                target_as_monster()->add_ench(mon_enchant(ENCH_CORONA, 20,
                                                          guilty));
            break;
        case 1:
            random_uselessness();
            break;
        }
        break;

    case 2:         // much more annoying
        switch (random2(7))
        {
        case 0:
        case 1:
        case 2:
            if (target->is_player())
            {
                mpr("You sense a malignant aura.");
                curse_an_item();
                break;
            }
            // Intentional fall-through for monsters.
        case 3:
        case 4:
        case 5:
            _potion_effect(POT_SLOWING, 10);
            break;
        case 6:
            _potion_effect(POT_BERSERK_RAGE, 10);
            break;
        }
        break;

    case 3:         // potentially lethal
        // Only use first two cases for monsters.
        switch (random2(target->is_player() ? 4 : 2))
        {
        case 0:
            target->paralyse(act_source, 2 + random2(6), cause);
            break;
        case 1:
            _potion_effect(POT_CONFUSION, 10);
            break;
        case 2:
            contaminate_player(random2avg(18000, 3), spell != SPELL_NO_SPELL);
            break;
        case 3:
            do
                curse_an_item();
            while (!one_chance_in(3));
            mpr("You sense an overwhelmingly malignant aura!");
            break;
        }
        break;
    }
}

void MiscastEffect::_translocation(int severity)
{
    switch (severity)
    {
    case 0:         // harmless messages only
        switch (random2(10))
        {
        case 0:
            you_msg      = "Space warps around you.";
            mon_msg      = "Space warps around @the_monster@.";
            break;
        case 1:
            you_msg      = "The air around you crackles with energy!";
            mon_msg_seen = "The air around @the_monster@ crackles with "
                           "energy!";
            mon_msg_unseen = "The air around something crackles with energy!";
            break;
        case 2:
            you_msg      = "You feel a wrenching sensation.";
            mon_msg_seen = "@The_monster@ jerks violently for a moment.";
            break;
        case 3:
            you_msg = "You feel a strange surge of energy!";
            mon_msg = "There is a strange surge of energy around @the_monster@.";
            break;
        case 4:
            you_msg      = "You spin around.";
            mon_msg_seen = "@The_monster@ spins around.";
            break;
        case 5:
            you_msg      = "Strange energies run through your body.";
            mon_msg_seen = "@The_monster@ glows " + weird_glowing_colour() +
                           " for a moment.";
            mon_msg_unseen = "A spot of thin air glows "
                             + weird_glowing_colour() + " for a moment.";
            break;
        case 6:
            you_msg      = "Your skin tingles.";
            mon_msg_seen = "@The_monster@ twitches.";
            break;
        case 7:
            you_msg      = "The world appears momentarily distorted!";
            mon_msg_seen = "@The_monster@ appears momentarily distorted!";
            break;
        case 8:
            // Set nothing; canned_msg(MSG_NOTHING_HAPPENS) will be taken
            // care of elsewhere.
            break;
        case 9:
            you_msg      = "You feel uncomfortable.";
            mon_msg_seen = "@The_monster@ scowls.";
            break;
        }
        do_msg();
        break;

    case 1:         // mostly harmless
        switch (random2(6))
        {
        case 0:
        case 1:
        case 2:
            you_msg        = "You are caught in a localised field of spatial "
                             "distortion.";
            mon_msg_seen   = "@The_monster@ is caught in a localised field of "
                             "spatial distortion.";
            mon_msg_unseen = "A piece of empty space twists and distorts.";
            _ouch(4 + random2avg(9, 2));
            break;
        case 3:
        case 4:
            you_msg        = "Space bends around you!";
            mon_msg_seen   = "Space bends around @the_monster@!";
            mon_msg_unseen = "A piece of empty space twists and distorts.";
            if (_ouch(4 + random2avg(7, 2)) && target->alive() && !target->no_tele())
                target->blink(false);
            break;
        case 5:
            if (_create_monster(MONS_SPATIAL_VORTEX, 3))
                all_msg = "Space twists in upon itself!";
            do_msg();
            break;
        }
        break;

    case 2:         // less harmless
    reroll_2:
        switch (random2(7))
        {
        case 0:
        case 1:
        case 2:
            you_msg        = "You are caught in a strong localised spatial "
                             "distortion.";
            mon_msg_seen   = "@The_monster@ is caught in a strong localised "
                             "spatial distortion.";
            mon_msg_unseen = "A piece of empty space twists and writhes.";
            _ouch(9 + random2avg(23, 2));
            break;
        case 3:
        case 4:
            you_msg        = "Space warps around you!";
            mon_msg_seen   = "Space warps around @the_monster@!";
            mon_msg_unseen = "A piece of empty space twists and writhes.";
            _ouch(5 + random2avg(9, 2));
            if (target->alive())
            {
                // Same message as a harmless miscast, thus no permit_id.
                if (!target->no_tele(true, false))
                {
                    if (one_chance_in(3))
                        target->teleport(true);
                    else
                        target->blink(false);
                }
                if (target->alive())
                    _potion_effect(POT_CONFUSION, 40);
            }
            break;
        case 5:
        {
            bool success = false;

            for (int i = 1 + random2(3); i >= 0; --i)
            {
                if (_create_monster(MONS_SPATIAL_VORTEX, 3))
                    success = true;
            }

            if (success)
                all_msg = "Space twists in upon itself!";
            break;
        }
        case 6:
            if (!_send_to_abyss())
                goto reroll_2;
            break;
        }
        break;

    case 3:         // much less harmless
    reroll_3:
        // Don't use the last case for monsters.
        switch (random2(target->is_player() ? 4 : 3))
        {
        case 0:
            you_msg        = "You are caught in an extremely strong localised "
                             "spatial distortion!";
            mon_msg_seen   = "@The_monster@ is caught in an extremely strong "
                             "localised spatial distortion!";
            mon_msg_unseen = "A rift temporarily opens in the fabric of space!";
            _ouch(15 + random2avg(29, 2));
            break;
        case 1:
            you_msg        = "Space warps crazily around you!";
            mon_msg_seen   = "Space warps crazily around @the_monster@!";
            mon_msg_unseen = "A rift temporarily opens in the fabric of space!";
            if (_ouch(9 + random2avg(17, 2)) && target->alive())
            {
                if (!target->no_tele())
                    target->teleport(true);
                if (target->alive())
                    _potion_effect(POT_CONFUSION, 60);
            }
            break;
        case 2:
            if (!_send_to_abyss())
                goto reroll_3;
            break;
        case 3:
            contaminate_player(random2avg(18000, 3), spell != SPELL_NO_SPELL);
            break;
        }
        break;
    }
}

void MiscastEffect::_summoning(int severity)
{
    switch (severity)
    {
    case 0:         // harmless messages only
        switch (random2(10))
        {
        case 0:
            you_msg      = "Shadowy shapes form in the air around you, "
                           "then vanish.";
            mon_msg_seen = "Shadowy shapes form in the air around "
                           "@the_monster@, then vanish.";
            break;
        case 1:
            if (neither_end_silenced())
            {
                all_msg        = "You hear strange voices.";
                msg_ch         = MSGCH_SOUND;
                sound_loudness = 2;
            }
            else if (target->is_player())
                you_msg = "You feel momentarily dizzy.";
            break;
        case 2:
            you_msg = "Your head hurts.";
            // Monster messages needed.
            break;
        case 3:
            you_msg = "You feel a strange surge of energy!";
            // Monster messages needed.
            break;
        case 4:
            you_msg = "Your brain hurts!";
            // Monster messages needed.
            break;
        case 5:
            you_msg      = "Strange energies run through your body.";
            mon_msg_seen = "@The_monster@ glows " + weird_glowing_colour() +
                           " for a moment.";
            break;
        case 6:
            you_msg      = "The world appears momentarily distorted.";
            mon_msg_seen = "@The_monster@ appears momentarily distorted.";
            break;
        case 7:
            you_msg      = "Space warps around you.";
            mon_msg_seen = "Space warps around @the_monster@.";
            break;
        case 8:
            // Set nothing; canned_msg(MSG_NOTHING_HAPPENS) will be taken
            // care of elsewhere.
            break;
        case 9:
            if (neither_end_silenced())
            {
                you_msg        = "Distant voices call out to you!";
                mon_msg_seen   = "Distant voices call out to @the_monster@!";
                msg_ch         = MSGCH_SOUND;
                sound_loudness = 2;
            }
            else if (target->is_player())
                you_msg = "You feel watched.";
            break;
        }
        do_msg();
        break;

    case 1:         // a little bad
        switch (random2(6))
        {
        case 0:             // identical to translocation
        case 1:
        case 2:
            you_msg        = "You are caught in a localised spatial "
                             "distortion.";
            mon_msg_seen   = "@The_monster@ is caught in a localised spatial "
                             "distortion.";
            mon_msg_unseen = "An empty piece of space distorts and twists.";
            _ouch(5 + random2avg(9, 2));
            break;
        case 3:
            if (_create_monster(MONS_SPATIAL_VORTEX, 3))
                all_msg = "Space twists in upon itself!";
            do_msg();
            break;
        case 4:
        case 5:
            if (_create_monster(summon_any_demon(RANDOM_DEMON_LESSER), 5, true))
                all_msg = "Something appears in a flash of light!";
            do_msg();
            break;
        }
        break;

    case 2:         // more bad
        switch (random2(6))
        {
        case 0:
        {
            bool success = false;

            for (int i = 1 + random2(3); i >= 0; --i)
            {
                if (_create_monster(MONS_SPATIAL_VORTEX, 3))
                    success = true;
            }

            if (success)
                all_msg = "Space twists in upon itself!";
            do_msg();
            break;
        }

        case 1:
        case 2:
            if (_create_monster(summon_any_demon(RANDOM_DEMON_COMMON), 5, true))
                all_msg = "Something forms from out of thin air!";
            do_msg();
            break;

        case 3:
        case 4:
        case 5:
        {
            bool success = false;

            for (int i = 1 + random2(2); i >= 0; --i)
            {
                if (_create_monster(summon_any_demon(RANDOM_DEMON_LESSER), 5, true))
                    success = true;
            }

            if (success && neither_end_silenced())
            {
                you_msg        = "A chorus of chattering voices calls out to"
                                 " you!";
                mon_msg        = "A chorus of chattering voices calls out!";
                msg_ch         = MSGCH_SOUND;
                sound_loudness = 3;
            }
            do_msg();
            break;
        }
        }
        break;

    case 3:         // more bad
        bool reroll = true;

        while (reroll)
        {
            switch (random2(5))
            {
            case 0:
                if (_create_monster(MONS_ABOMINATION_SMALL, 0, true))
                    all_msg = "Something forms from out of thin air.";
                do_msg();
                reroll = false;
                break;

            case 1:
                if (_create_monster(summon_any_demon(RANDOM_DEMON_GREATER), 0, true))
                    all_msg = "You sense a hostile presence.";
                do_msg();
                reroll = false;
                break;

            case 2:
            {
                bool success = false;

                for (int i = 1 + random2(2); i >= 0; --i)
                {
                    if (_create_monster(summon_any_demon(RANDOM_DEMON_COMMON), 3, true))
                        success = true;
                }

                if (success)
                {
                    you_msg = "Something turns its malign attention towards "
                              "you...";
                    mon_msg = "You sense a malign presence.";
                }
                do_msg();
                reroll = false;
                break;
            }

            case 3:
                reroll = !_send_to_abyss();
                break;

            case 4:
                reroll = !_malign_gateway();
                break;
            }
        }

        break;
    }
}

void MiscastEffect::_divination_you(int severity)
{
    switch (severity)
    {
    case 0:         // just a harmless message
        switch (random2(10))
        {
        case 0:
            mpr("Weird images run through your mind.");
            break;
        case 1:
            if (!silenced(you.pos()))
            {
                mpr("You hear strange voices.", MSGCH_SOUND);
                noisy(2, you.pos());
            }
            else
                mpr("Your nose twitches.");
            break;
        case 2:
            mpr("Your head hurts.");
            break;
        case 3:
            mpr("You feel a strange surge of energy!");
            break;
        case 4:
            mpr("Your brain hurts!");
            break;
        case 5:
            mpr("Strange energies run through your body.");
            break;
        case 6:
            mpr("Everything looks hazy for a moment.");
            break;
        case 7:
            mpr("You seem to have forgotten something, but you can't remember what it was!");
            break;
        case 8:
            canned_msg(MSG_NOTHING_HAPPENS);
            break;
        case 9:
            mpr("You feel uncomfortable.");
            break;
        }
        break;

    case 1:         // more annoying things
        switch (random2(2))
        {
        case 0:
            mpr("You feel a little dazed.");
            break;
        case 1:
            potion_effect(POT_CONFUSION, 10);
            break;
        }
        break;

    case 2:         // even more annoying things
        switch (random2(2))
        {
        case 0:
            if (_lose_stat(STAT_INT, 1 + random2(3)))
            {
                if (you.is_undead)
                    mpr("You suddenly recall your previous life!");
                else
                    mpr("You have damaged your brain!");
            }
            else if (!did_msg)
                mpr("You have a terrible headache.");
            break;
        case 1:
            mpr("You lose your focus.");
            if (you.magic_points > 0 || you.species == SP_DJINNI)
            {
                drain_mp(3 + random2(10));
                mpr("You suddenly feel drained of magical energy!", MSGCH_WARN);
            }
            break;
        }

        potion_effect(POT_CONFUSION, 1);  // common to all cases here {dlb}
        break;

    case 3:         // nasty
        switch (random2(2))
        {
        case 0:
            mpr("You lose concentration completely!");
            if (you.magic_points > 0 || you.species == SP_DJINNI)
            {
                drain_mp(5 + random2(20));
                mpr("You suddenly feel drained of magical energy!", MSGCH_WARN);
            }
            break;
        case 1:
            if (_lose_stat(STAT_INT, 3 + random2(3)))
            {
                if (you.is_undead)
                    mpr("You suddenly recall your previous life!");
                else
                    mpr("You have damaged your brain!");
            }
            else if (!did_msg)
                mpr("You have a terrible headache.");
            break;
        }

        potion_effect(POT_CONFUSION, 1);  // common to all cases here {dlb}
        break;
    }
}

// XXX: Monster divination miscasts.
void MiscastEffect::_divination_mon(int severity)
{
    // Nothing is appropriate for unmoving plants.
    if (mons_is_firewood(target_as_monster()))
        return;

    switch (severity)
    {
    case 0:         // just a harmless message
        mon_msg_seen = "@The_monster@ looks momentarily confused.";
        break;

    case 1:         // more annoying things
        switch (random2(2))
        {
        case 0:
            mon_msg_seen = "@The_monster@ looks slightly disoriented.";
            break;
        case 1:
            mon_msg_seen = "@The_monster@ looks disoriented.";
            target->confuse(
                act_source,
                1 + random2(3 + act_source->get_experience_level()));
            break;
        }
        break;

    case 2:         // even more annoying things
        mon_msg_seen = "@The_monster@ shudders.";
        target->confuse(
            act_source,
            5 + random2(3 + act_source->get_experience_level()));
        break;

    case 3:         // nasty
        mon_msg_seen = "@The_monster@ reels.";
        if (one_chance_in(7))
            target_as_monster()->forget_random_spell();
        target->confuse(
            act_source,
            8 + random2(3 + act_source->get_experience_level()));
        break;
    }
    do_msg();
}

void MiscastEffect::_necromancy(int severity)
{
    if (target->is_player() && you_worship(GOD_KIKUBAAQUDGHA)
        && !player_under_penance() && you.piety >= piety_breakpoint(1))
    {
        const bool death_curse = (cause.find("death curse") != string::npos);

        if (spell != SPELL_NO_SPELL)
        {
            // An actual necromancy miscast.
            if (x_chance_in_y(you.piety, 150))
            {
                canned_msg(MSG_NOTHING_HAPPENS);
                return;
            }
        }
        else if (death_curse)
        {
            if (coinflip())
            {
                simple_god_message(" averts the curse.");
                return;
            }
            else
            {
                simple_god_message(" partially averts the curse.");
                severity = max(severity - 1, 0);
            }
        }
    }

    switch (severity)
    {
    case 0:
        switch (random2(10))
        {
        case 0:
            if (you.can_smell())
                all_msg = "You smell decay.";
            else if (you.species == SP_MUMMY)
                you_msg = "Your bandages flutter.";
            break;
        case 1:
            if (neither_end_silenced())
            {
                all_msg        = "You hear strange and distant voices.";
                msg_ch         = MSGCH_SOUND;
                sound_loudness = 3;
            }
            else if (target->is_player())
                you_msg = "You feel homesick.";
            break;
        case 2:
            you_msg = "Pain shoots through your body.";
            mon_msg_seen = "@The_monster@ twitches violently.";
            break;
        case 3:
            if (you.species == SP_OCTOPODE)
                you_msg = "You feel numb.";
            else
                you_msg = "Your bones ache.";
            mon_msg_seen = "@The_monster@ pauses, visibly distraught.";
            break;
        case 4:
            you_msg      = "The world around you seems to dim momentarily.";
            mon_msg_seen = "@The_monster@ seems to dim momentarily.";
            break;
        case 5:
            you_msg      = "Strange energies run through your body.";
            mon_msg_seen = "@The_monster@ glows " + weird_glowing_colour() +
                           " for a moment.";
            break;
        case 6:
            you_msg      = "You shiver with cold.";
            mon_msg_seen = "@The_monster@ shivers with cold.";
            break;
        case 7:
            you_msg        = "You sense a malignant aura.";
            mon_msg_seen   = "@The_monster@ is briefly tinged with black.";
            mon_msg_unseen = "The air has a black tinge for a moment.";
            break;
        case 8:
            // Set nothing; canned_msg(MSG_NOTHING_HAPPENS) will be taken
            // care of elsewhere.
            break;
        case 9:
            you_msg      = "You feel very uncomfortable.";
            mon_msg_seen = "@The_monster@ scowls horribly.";
            break;
        }
        do_msg();
        break;

    case 1:         // a bit nasty
        switch (random2(3))
        {
        case 0:
            if (target->res_torment())
            {
                you_msg      = "You feel weird for a moment.";
                mon_msg_seen = "@The_monster@ has a weird expression for a "
                               "moment.";
            }
            else
            {
                you_msg      = "Pain shoots through your body!";
                mon_msg_seen = "@The_monster@ convulses with pain!";
                _ouch(5 + random2avg(15, 2));
            }
            break;
        case 1:
            you_msg      = "You feel horribly lethargic.";
            mon_msg_seen = "@The_monster@ looks incredibly listless.";
            _potion_effect(POT_SLOWING, 15);
            break;
        case 2:
            if (!target->res_rotting())
            {
                if (you.can_smell())
                {
                    // identical to a harmless message
                    all_msg = "You smell decay.";
                }

                target->rot(act_source, 1, 0, true);
            }
            else if (you.species == SP_MUMMY)
            {
                // Monster messages needed.
                you_msg = "Your bandages flutter.";
            }
            break;
        }
        if (!did_msg)
            do_msg();
        break;

    case 2:         // much nastier
        switch (random2(3))
        {
        case 0:
        {
            bool success = false;

            for (int i = random2(2); i >= 0; --i)
            {
                if (_create_monster(MONS_SHADOW, 2, true))
                    success = true;
            }

            if (success)
            {
                you_msg        = "Flickering shadows surround you.";
                mon_msg_seen   = "Flickering shadows surround @the_monster@.";
                mon_msg_unseen = "Shadows flicker in the thin air.";
            }
            do_msg();
            break;
        }

        case 1:
            you_msg      = "You are engulfed in negative energy!";
            mon_msg_seen = "@The_monster@ is engulfed in negative energy!";

            if (one_chance_in(3))
            {
                do_msg();
                target->drain_exp(act_source, false, 100);
                break;
            }

            // If we didn't do anything, just flow through...
            if (did_msg)
                break;

        case 2:
            if (target->res_torment())
            {
                you_msg      = "You feel weird for a moment.";
                mon_msg_seen = "@The_monster@ has a weird expression for a "
                               "moment.";
            }
            else
            {
                you_msg      = "You convulse helplessly as pain tears through "
                               "your body!";
                mon_msg_seen = "@The_monster@ convulses helplessly with pain!";
                _ouch(15 + random2avg(23, 2));
            }
            if (!did_msg)
                do_msg();
            break;
        }
        break;

    case 3:         // even nastier
        // Don't use last case for monsters.
        switch (random2(target->is_player() ? 6 : 5))
        {
        case 0:
            if (target->holiness() == MH_UNDEAD)
            {
                you_msg      = "Something just walked over your grave. No, "
                               "really!";
                mon_msg_seen = "@The_monster@ seems frightened for a moment.";
                do_msg();
            }
            else
                torment_monsters(target->pos(), 0, TORMENT_GENERIC);
            break;

        case 1:
            target->rot(act_source, random2avg(7, 2) + 1);
            break;

        case 2:
            if (_create_monster(MONS_SOUL_EATER, 4, true))
            {
                you_msg        = "Something reaches out for you...";
                mon_msg_seen   = "Something reaches out for @the_monster@...";
                mon_msg_unseen = "Something reaches out from thin air...";
            }
            do_msg();
            break;

        case 3:
            if (_create_monster(MONS_REAPER, 4, true))
            {
                you_msg        = "Death has come for you...";
                mon_msg_seen   = "Death has come for @the_monster@...";
                mon_msg_unseen = "Death appears from thin air...";
            }
            do_msg();
            break;

        case 4:
            you_msg      = "You are engulfed in negative energy!";
            mon_msg_seen = "@The_monster@ is engulfed in negative energy!";

            do_msg();
            target->drain_exp(act_source, false, 100);
            break;

            // If we didn't do anything, just flow through if it's the player.
            if (target->is_monster() || did_msg)
                break;

        case 5:
            _lose_stat(STAT_RANDOM, 1 + random2avg(7, 2));
            break;
        }
        break;
    }
}

void MiscastEffect::_transmutation(int severity)
{
    int num;
    switch (severity)
    {
    case 0:         // just a harmless message
        num = 10 + (_has_hair(target) ? 1 : 0);
        switch (random2(num))
        {
        case 0:
            _your_hands_glow(target, you_msg, mon_msg_seen, can_plural_hand);
            break;
        case 1:
            you_msg        = "The air around you crackles with energy!";
            mon_msg_seen   = "The air around @the_monster@ crackles with "
                             "energy!";
            mon_msg_unseen = "The thin air crackles with energy!";
            break;
        case 2:
            you_msg        = "Multicoloured lights dance before your eyes!";
            mon_msg_seen   = "Multicoloured lights dance around @the_monster@!";
            mon_msg_unseen = "Multicoloured lights dance in the air!";
            break;
        case 3:
            you_msg = "You feel a strange surge of energy!";
            // Monster messages needed.
            break;
        case 4:
            you_msg        = "Waves of light ripple over your body.";
            mon_msg_seen   = "Waves of light ripple over @the_monster@'s body.";
            mon_msg_unseen = "Waves of light ripple in the air.";
            break;
        case 5:
            you_msg      = "Strange energies run through your body.";
            mon_msg_seen = "@The_monster@ glows " + weird_glowing_colour() +
                           " for a moment.";
            break;
        case 6:
            you_msg      = "Your skin tingles.";
            mon_msg_seen = "@The_monster@ twitches.";
            break;
        case 7:
            you_msg      = "Your skin glows momentarily.";
            mon_msg_seen = "@The_monster@'s body glows momentarily.";
            break;
        case 8:
            // Set nothing; canned_msg(MSG_NOTHING_HAPPENS) will be taken
            // care of elsewhere.
            break;
        case 9:
            if (you.can_smell())
                all_msg = "You smell something strange.";
            else if (you.species == SP_MUMMY)
                you_msg = "Your bandages flutter.";
            break;
        case 10:
        {
            // Player only (for now).
            bool plural;
            string hair = _hair_str(target, plural);
            you_msg = make_stringf("Your %s momentarily turn%s into snakes!",
                                   hair.c_str(), plural ? "" : "s");
        }
        }
        do_msg();
        break;

    case 1:         // slightly annoying
        switch (random2(target->is_player() ? 2 : 1))
        {
        case 0:
            you_msg      = "Your body is twisted painfully.";
            mon_msg_seen = "@The_monster@'s body twists unnaturally.";
            _ouch(1 + random2avg(11, 2));
            break;
        case 1:
            random_uselessness();
            break;
        }
        break;

    case 2:         // much more annoying
        // Last case for players only.
        switch (random2(target->is_player() ? 5 : 4))
        {
        case 0:
            if (target->polymorph(0)) // short duration
                break;
        case 1:
            you_msg      = "Your body is twisted very painfully!";
            mon_msg_seen = "@The_monster@'s body twists and writhes.";
            _ouch(3 + random2avg(23, 2));
            break;
        case 2:
            target->petrify(guilty);
            break;
        case 3:
            _potion_effect(POT_CONFUSION, 10);
            break;
        case 4:
            contaminate_player(random2avg(18000, 3), spell != SPELL_NO_SPELL);
            break;
        }
        break;

    case 3:         // even nastier
        if (coinflip() && target->polymorph(200))
            break;

        switch (random2(3))
        {
        case 0:
            you_msg = "Your body is flooded with distortional energies!";
            mon_msg = "@The_monster@'s body is flooded with distortional "
                      "energies!";
            if (_ouch(3 + random2avg(18, 2)) && target->alive())
                if (target->is_player())
                {
                    contaminate_player(random2avg(34000, 3),
                                       spell != SPELL_NO_SPELL, false);
                }
                else
                    target->polymorph(0);
            break;

        case 1:
            // HACK: Avoid lethality before deleting mutation, since
            // afterwards a message would already have been given.
            if (lethality_margin > 0
                && (you.hp - lethality_margin) <= 27
                && avoid_lethal(you.hp))
            {
                return;
            }

            if (target->is_player())
            {
                you_msg = "You feel very strange.";
                delete_mutation(RANDOM_MUTATION, cause, true, false, false, false);
            }
            else
                target->polymorph(0);
            _ouch(5 + random2avg(23, 2));
            break;

        case 2:
            if (target->is_player())
            {
                // HACK: Avoid lethality before giving mutation, since
                // afterwards a message would already have been given.
                if (lethality_margin > 0
                    && (you.hp - lethality_margin) <= 27
                    && avoid_lethal(you.hp))
                {
                    return;
                }

                you_msg = "Your body is distorted in a weirdly horrible way!";
                // We don't need messages when the mutation fails,
                // because we give our own (which is justified anyway as
                // you take damage).
                mutate(RANDOM_BAD_MUTATION, cause, false, false);
                if (coinflip())
                    mutate(RANDOM_BAD_MUTATION, cause, false, false);
            }
            else
                target->malmutate(cause);
            _ouch(5 + random2avg(23, 2));
            break;
        }
        break;
    }
}

void MiscastEffect::_fire(int severity)
{
    switch (severity)
    {
    case 0:         // just a harmless message
        switch (random2(10))
        {
        case 0:
            you_msg      = "Sparks fly from your @hands@!";
            mon_msg_seen = "Sparks fly from @the_monster@'s @hands@!";
            break;
        case 1:
            you_msg      = "The air around you burns with energy!";
            mon_msg_seen = "The air around @the_monster@ burns with energy!";
            break;
        case 2:
            you_msg      = "Wisps of smoke drift from your @hands@.";
            mon_msg_seen = "Wisps of smoke drift from @the_monster@'s @hands@.";
            break;
        case 3:
            you_msg = "You feel a strange surge of energy!";
            // Monster messages needed.
            break;
        case 4:
            if (you.can_smell())
                all_msg = "You smell smoke.";
            else if (you.species == SP_MUMMY)
                you_msg = "Your bandages flutter.";
            break;
        case 5:
            you_msg = "Heat runs through your body.";
            // Monster messages needed.
            break;
        case 6:
            you_msg = "You feel uncomfortably hot.";
            // Monster messages needed.
            break;
        case 7:
            you_msg      = "Lukewarm flames ripple over your body.";
            mon_msg_seen = "Dim flames ripple over @the_monster@'s body.";
            break;
        case 8:
            // Set nothing; canned_msg(MSG_NOTHING_HAPPENS) will be taken
            // care of elsewhere.
            break;
        case 9:
            if (neither_end_silenced())
            {
                all_msg        = "You hear a sizzling sound.";
                msg_ch         = MSGCH_SOUND;
                sound_loudness = 2;
            }
            else if (target->is_player())
                you_msg = "You feel like you have heartburn.";
            break;
        }
        do_msg();
        break;

    case 1:         // a bit less harmless stuff
        switch (random2(2))
        {
        case 0:
            you_msg        = "Smoke pours from your @hands@!";
            mon_msg_seen   = "Smoke pours from @the_monster@'s @hands@!";
            mon_msg_unseen = "Smoke appears out of nowhere!";

            _big_cloud(random_smoke_type(), 20, 7 + random2(7));
            break;

        case 1:
            you_msg      = "Flames sear your flesh.";
            mon_msg_seen = "Flames sear @the_monster@.";
            if (target->res_fire() < 0)
            {
                if (!_ouch(2 + random2avg(13, 2)))
                    return;
            }
            else
                do_msg();
            if (target->alive())
                target->expose_to_element(BEAM_FIRE, 3);
            break;
        }
        break;

    case 2:         // rather less harmless stuff
        switch (random2(2))
        {
        case 0:
            you_msg        = "You are blasted with fire.";
            mon_msg_seen   = "@The_monster@ is blasted with fire.";
            mon_msg_unseen = "A flame briefly burns in thin air.";
            if (_ouch(5 + random2avg(29, 2), BEAM_FIRE) && target->alive())
                target->expose_to_element(BEAM_FIRE, 5);
            break;

        case 1:
            you_msg        = "You are caught in a fiery explosion!";
            mon_msg_seen   = "@The_monster@ is caught in a fiery explosion!";
            mon_msg_unseen = "Fire explodes from out of thin air!";

            beam.flavour = BEAM_FIRE;
            beam.glyph   = dchar_glyph(DCHAR_FIRED_BURST);
            beam.damage  = dice_def(3, 14);
            beam.name    = "explosion";
            beam.colour  = RED;

            _explosion();
            break;
        }
        break;

    case 3:         // considerably less harmless stuff
        switch (random2(3))
        {
        case 0:
            you_msg        = "You are blasted with searing flames!";
            mon_msg_seen   = "@The_monster@ is blasted with searing flames!";
            mon_msg_unseen = "A large flame burns hotly for a moment in the "
                             "thin air.";
            if (_ouch(9 + random2avg(33, 2), BEAM_FIRE) && target->alive())
                target->expose_to_element(BEAM_FIRE, 10);
            break;
        case 1:
            all_msg = "There is a sudden and violent explosion of flames!";

            beam.flavour = BEAM_FIRE;
            beam.glyph   = dchar_glyph(DCHAR_FIRED_BURST);
            beam.damage  = dice_def(3, 20);
            beam.name    = "fireball";
            beam.colour  = RED;
            beam.ex_size = coinflip() ? 1 : 2;

            _explosion();
            break;

        case 2:
        {
            you_msg      = "You are covered in liquid flames!";
            mon_msg_seen = "@The_monster@ is covered in liquid flames!";
            do_msg();

            if (target->is_player())
                napalm_player(random2avg(7,3) + 1, cause);
            else
            {
                monster* mon_target = target_as_monster();
                mon_target->add_ench(mon_enchant(ENCH_STICKY_FLAME,
                    min(4, 1 + random2(mon_target->hit_dice) / 2),
                    guilty));
            }
            break;
        }
        }
        break;
    }
}

void MiscastEffect::_ice(int severity)
{
    const dungeon_feature_type feat = grd(target->pos());

    const bool frostable_feat =
        (feat == DNGN_FLOOR || feat_altar_god(feat) != GOD_NO_GOD
         || feat_is_staircase(feat) || feat_is_water(feat));

    const string feat_name = (feat == DNGN_FLOOR ? "the " : "") +
        feature_description_at(target->pos(), false, DESC_THE);

    int num;
    switch (severity)
    {
    case 0:         // just a harmless message
        num = 10 + (frostable_feat ? 1 : 0);
        switch (random2(num))
        {
        case 0:
            you_msg      = "You shiver with cold.";
            mon_msg_seen = "@The_monster@ shivers with cold.";
            break;
        case 1:
            you_msg = "A chill runs through your body.";
            // Monster messages needed.
            break;
        case 2:
            you_msg        = "Wisps of condensation drift from your @hands@.";
            mon_msg_seen   = "Wisps of condensation drift from @the_monster@'s "
                             "@hands@.";
            mon_msg_unseen = "Wisps of condensation drift in the air.";
            break;
        case 3:
            you_msg = "You feel a strange surge of energy!";
            // Monster messages needed.
            break;
        case 4:
            you_msg      = "Your @hands@ feel@hand_conj@ numb with cold.";
            // Monster messages needed.
            break;
        case 5:
            you_msg = "A chill runs through your body.";
            // Monster messages needed.
            break;
        case 6:
            you_msg = "You feel uncomfortably cold.";
            // Monster messages needed.
            break;
        case 7:
            you_msg      = "Frost covers your body.";
            mon_msg_seen = "Frost covers @the_monster@'s body.";
            break;
        case 8:
            // Set nothing; canned_msg(MSG_NOTHING_HAPPENS) will be taken
            // care of elsewhere.
            break;
        case 9:
            if (neither_end_silenced())
            {
                all_msg        = "You hear a crackling sound.";
                msg_ch         = MSGCH_SOUND;
                sound_loudness = 2;
            }
            else if (target->is_player())
                you_msg = "A snowflake lands on your nose.";
            break;
        case 10:
            if (feat_is_water(feat))
                all_msg  = "A thin layer of ice forms on " + feat_name;
            else
                all_msg  = "Frost spreads across " + feat_name;
            break;
        }
        do_msg();
        break;

    case 1:         // a bit less harmless stuff
        switch (random2(2))
        {
        case 0:
            mpr("You feel extremely cold.");
            // Monster messages needed.
            break;
        case 1:
            you_msg      = "You are covered in a thin layer of ice.";
            mon_msg_seen = "@The_monster@ is covered in a thin layer of ice.";
            if (target->res_cold() < 0)
            {
                if (!_ouch(4 + random2avg(5, 2)))
                    return;
            }
            else
                do_msg();
            if (target->alive())
                target->expose_to_element(BEAM_COLD, 2, true, false);
            break;
        }
        break;

    case 2:         // rather less harmless stuff
        switch (random2(2))
        {
        case 0:
            you_msg = "Heat is drained from your body.";
            // Monster messages needed.
            if (_ouch(5 + random2(6) + random2(7), BEAM_COLD) && target->alive())
                target->expose_to_element(BEAM_COLD, 4);
            break;

        case 1:
            you_msg        = "You are caught in an explosion of ice and frost!";
            mon_msg_seen   = "@The_monster@ is caught in an explosion of "
                             "ice and frost!";
            mon_msg_unseen = "Ice and frost explode from out of thin air!";

            beam.flavour = BEAM_COLD;
            beam.glyph   = dchar_glyph(DCHAR_FIRED_BURST);
            beam.damage  = dice_def(3, 11);
            beam.name    = "explosion";
            beam.colour  = WHITE;

            _explosion();
            break;
        }
        break;

    case 3:         // less harmless stuff
        switch (random2(2))
        {
        case 0:
            you_msg      = "You are blasted with ice!";
            mon_msg_seen = "@The_monster@ is blasted with ice!";
            if (_ouch(9 + random2avg(23, 2), BEAM_ICE) && target->alive())
                target->expose_to_element(BEAM_COLD, 9);
            break;
        case 1:
            you_msg        = "Freezing gasses pour from your @hands@!";
            mon_msg_seen   = "Freezing gasses pour from @the_monster@'s "
                             "@hands@!";

            _big_cloud(CLOUD_COLD, 20, 8 + random2(4));
            break;
        }
        break;
    }
}

static bool _on_floor(actor* target)
{
    return (target->ground_level() && grd(target->pos()) == DNGN_FLOOR);
}

void MiscastEffect::_earth(int severity)
{
    int num;
    switch (severity)
    {
    case 0:         // just a harmless message
    case 1:
        num = 11 + (_on_floor(target) ? 2 : 0);
        switch (random2(num))
        {
        case 0:
            you_msg = "You feel earthy.";
            // Monster messages needed.
            break;
        case 1:
            you_msg        = "You are showered with tiny particles of grit.";
            mon_msg_seen   = "@The_monster@ is showered with tiny particles "
                             "of grit.";
            mon_msg_unseen = "Tiny particles of grit hang in the air for a "
                             "moment.";
            break;
        case 2:
            you_msg        = "Sand pours from your @hands@.";
            mon_msg_seen   = "Sand pours from @the_monster@'s @hands@.";
            mon_msg_unseen = "Sand pours from out of thin air.";
            break;
        case 3:
            you_msg = "You feel a surge of energy from the ground.";
            // Monster messages needed.
            break;
        case 4:
            if (neither_end_silenced())
            {
                all_msg        = "You hear a distant rumble.";
                msg_ch         = MSGCH_SOUND;
                sound_loudness = 2;
            }
            else if (target->is_player())
                you_msg = "You sympathise with the stones.";
            break;
        case 5:
            you_msg = "You feel gritty.";
            // Monster messages needed.
            break;
        case 6:
            you_msg      = "You feel momentarily lethargic.";
            mon_msg_seen = "@The_monster@ looks momentarily listless.";
            break;
        case 7:
            you_msg        = "Motes of dust swirl before your eyes.";
            mon_msg_seen   = "Motes of dust swirl around @the_monster@.";
            mon_msg_unseen = "Motes of dust swirl around in the air.";
            break;
        case 8:
            // Set nothing; canned_msg(MSG_NOTHING_HAPPENS) will be taken
            // care of elsewhere.
            break;
        case 9:
        {
            bool          pluralised = true;
            string        feet       = you.foot_name(true, &pluralised);
            ostringstream str;

            str << "Your " << feet << (pluralised ? " feel" : " feels")
                << " warm.";

            you_msg = str.str();
            // Monster messages needed.
            break;
        }
        case 10:
            if (target->cannot_move())
            {
                you_msg      = "You briefly vibrate.";
                mon_msg_seen = "@The_monster@ briefly vibrates.";
            }
            else
            {
                you_msg      = "You momentarily stiffen.";
                mon_msg_seen = "@The_monster@ momentarily stiffens.";
            }
            break;
        case 11:
            all_msg = "The floor vibrates.";
            break;
        case 12:
            all_msg = "The floor shifts beneath you alarmingly!";
            break;
        }
        do_msg();
        break;

    case 2:         // slightly less harmless stuff
        switch (random2(1))
        {
        case 0:
            switch (random2(3))
            {
            case 0:
                you_msg        = "You are hit by flying rocks!";
                mon_msg_seen   = "@The_monster@ is hit by flying rocks!";
                mon_msg_unseen = "Flying rocks appear out of thin air!";
                break;
            case 1:
                you_msg        = "You are blasted with sand!";
                mon_msg_seen   = "@The_monster@ is blasted with sand!";
                mon_msg_unseen = "A miniature sandstorm briefly appears!";
                break;
            case 2:
                you_msg        = "Rocks fall onto you out of nowhere!";
                mon_msg_seen   = "Rocks fall onto @the_monster@ out of "
                                 "nowhere!";
                mon_msg_unseen = "Rocks fall out of nowhere!";
                break;
            }
            _ouch(target->apply_ac(random2avg(13, 2) + 10));
            break;
        }
        break;

    case 3:         // less harmless stuff
        switch (random2(1))
        {
        case 0:
            you_msg        = "You are caught in an explosion of flying "
                             "shrapnel!";
            mon_msg_seen   = "@The_monster@ is caught in an explosion of "
                             "flying shrapnel!";
            mon_msg_unseen = "Flying shrapnel explodes from thin air!";

            beam.flavour = BEAM_FRAG;
            beam.glyph   = dchar_glyph(DCHAR_FIRED_BURST);
            beam.damage  = dice_def(3, 15);
            beam.name    = "explosion";
            beam.colour  = CYAN;

            if (one_chance_in(5))
                beam.colour = BROWN;
            if (one_chance_in(5))
                beam.colour = LIGHTCYAN;

            _explosion();
            break;
        }
        break;
    }
}

void MiscastEffect::_air(int severity)
{
    int num;
    switch (severity)
    {
    case 0:         // just a harmless message
        num = 9;
        if (target->is_player())
            num += 3 + _has_hair(target);
        switch (random2(num))
        {
        case 0:
            you_msg      = "You feel momentarily weightless.";
            mon_msg_seen = "@The_monster@ bobs in the air for a moment.";
            break;
        case 1:
            you_msg      = "Wisps of vapour drift from your @hands@.";
            mon_msg_seen = "Wisps of vapour drift from @the_monster@'s "
                           "@hands@.";
            break;
        case 2:
        {
            bool pluralised = true;
            if (!hand_str.empty())
                pluralised = can_plural_hand;
            else
                target->hand_name(true, &pluralised);

            if (pluralised)
            {
                you_msg      = "Sparks of electricity dance between your "
                               "@hands@.";
                mon_msg_seen = "Sparks of electricity dance between "
                               "@the_monster@'s @hands@.";
            }
            else
            {
                you_msg      = "Sparks of electricity dance over your "
                               "@hand@.";
                mon_msg_seen = "Sparks of electricity dance over "
                               "@the_monster@'s @hand@.";
            }
            break;
        }
        case 3:
            you_msg      = "You are blasted with air!";
            mon_msg_seen = "@The_monster@ is blasted with air!";
            break;
        case 4:
            if (neither_end_silenced())
            {
                all_msg        = "You hear a whooshing sound.";
                msg_ch         = MSGCH_SOUND;
                sound_loudness = 2;
            }
            else if (you.can_smell())
                all_msg = "You smell ozone.";
            else if (you.species == SP_MUMMY)
                you_msg = "Your bandages flutter.";
            break;
        case 5:
            // Set nothing; canned_msg(MSG_NOTHING_HAPPENS) will be taken
            // care of elsewhere.
            break;
        case 6:
            if (neither_end_silenced())
            {
                all_msg        = "You hear a crackling sound.";
                msg_ch         = MSGCH_SOUND;
                sound_loudness = 2;
            }
            else if (you.can_smell())
                all_msg = "You smell something musty.";
            else if (you.species == SP_MUMMY)
                you_msg = "Your bandages flutter.";
            break;
        case 7:
            you_msg      = "There is a short, sharp shower of sparks.";
            mon_msg_seen = "@The_monster@ is briefly showered in sparks.";
            break;
        case 8:
            if (silenced(you.pos()))
            {
               you_msg        = "The wind whips around you!";
               mon_msg_seen   = "The wind whips around @the_monster@!";
               mon_msg_unseen = "The wind whips!";
            }
            else
            {
               you_msg        = "The wind howls around you!";
               mon_msg_seen   = "The wind howls around @the_monster@!";
               mon_msg_unseen = "The wind howls!";
            }
            break;
        case 9:
            you_msg = "Ouch! You gave yourself an electric shock.";
            // Monster messages needed.
            break;
        case 10:
            you_msg = "You feel a strange surge of energy!";
            // Monster messages needed.
            break;
        case 11:
            you_msg = "You feel electric!";
            // Monster messages needed.
            break;
        case 12:
        {
            // Player only (for now).
            bool plural;
            string hair = _hair_str(target, plural);
            you_msg = make_stringf("Your %s stand%s on end.", hair.c_str(),
                                   plural ? "" : "s");
            break;
        }
        }
        do_msg();
        break;

    case 1:         // rather less harmless stuff
        switch (random2(3))
        {
        case 0:
            you_msg        = "Electricity courses through your body.";
            mon_msg_seen   = "@The_monster@ is jolted!";
            mon_msg_unseen = "Something invisible sparkles with electricity.";
            _ouch(4 + random2avg(9, 2), BEAM_ELECTRICITY);
            break;
        case 1:
            you_msg        = "Noxious gasses pour from your @hands@!";
            mon_msg_seen   = "Noxious gasses pour from @the_monster@'s "
                             "@hands@!";
            mon_msg_unseen = "Noxious gasses appear from out of thin air!";

            _big_cloud(CLOUD_MEPHITIC, 20, 9 + random2(4));
            break;
        case 2:
            you_msg        = "You are under the weather.";
            mon_msg_seen   = "@The_monster@ looks under the weather.";
            mon_msg_unseen = "Inclement weather forms around a spot in thin air.";

            _big_cloud(CLOUD_RAIN, 20, 20 + random2(20));
            break;
        }
        break;

    case 2:         // much less harmless stuff
        switch (random2(2))
        {
        case 0:
            you_msg        = "You are caught in an explosion of electrical "
                             "discharges!";
            mon_msg_seen   = "@The_monster@ is caught in an explosion of "
                             "electrical discharges!";
            mon_msg_unseen = "Electrical discharges explode from out of "
                             "thin air!";

            beam.flavour = BEAM_ELECTRICITY;
            beam.glyph   = dchar_glyph(DCHAR_FIRED_BURST);
            beam.damage  = dice_def(3, 8);
            beam.name    = "explosion";
            beam.colour  = LIGHTBLUE;
            beam.ex_size = one_chance_in(4) ? 1 : 2;

            _explosion();
            break;
        case 1:
            you_msg        = "Venomous gasses pour from your @hands@!";
            mon_msg_seen   = "Venomous gasses pour from @the_monster@'s "
                             "@hands@!";
            mon_msg_unseen = "Venomous gasses pour forth from the thin air!";

            _big_cloud(CLOUD_POISON, 20, 8 + random2(5));
            break;
        }
        break;

    case 3:         // even less harmless stuff
        switch (random2(3))
        {
        case 0:
            if (_create_monster(MONS_BALL_LIGHTNING, 3))
                all_msg = "A ball of electricity appears!";
            do_msg();
            break;
        case 1:
            you_msg        = "The air twists around and strikes you!";
            mon_msg_seen   = "@The_monster@ is struck by twisting air!";
            mon_msg_unseen = "The air madly twists around a spot.";
            _ouch(12 + random2avg(29, 2), BEAM_AIR);
            break;
        case 2:
            if (_create_monster(MONS_TWISTER, 1))
                all_msg = "A huge vortex of air appears!";
            do_msg();
            break;
        }
        break;
    }
}

void MiscastEffect::_poison(int severity)
{
    switch (severity)
    {
    case 0:         // just a harmless message
        switch (random2(10))
        {
        case 0:
            you_msg      = "You feel mildly nauseous.";
            mon_msg_seen = "@The_monster@ briefly looks nauseous.";
            break;
        case 1:
            you_msg      = "You feel slightly ill.";
            mon_msg_seen = "@The_monster@ briefly looks sick.";
            break;
        case 2:
            you_msg      = "Wisps of poison gas drift from your @hands@.";
            mon_msg_seen = "Wisps of poison gas drift from @the_monster@'s "
                           "@hands@.";
            break;
        case 3:
            you_msg = "You feel a strange surge of energy!";
            // Monster messages needed.
            break;
        case 4:
            you_msg      = "You feel faint for a moment.";
            mon_msg_seen = "@The_monster@ looks faint for a moment.";
            break;
        case 5:
            you_msg      = "You feel sick.";
            mon_msg_seen = "@The_monster@ looks sick.";
            break;
        case 6:
            you_msg      = "You feel odd.";
            mon_msg_seen = "@The_monster@ has an odd expression for a "
                           "moment.";
            break;
        case 7:
            you_msg      = "You feel weak for a moment.";
            mon_msg_seen = "@The_monster@ looks weak for a moment.";
            break;
        case 8:
            // Set nothing; canned_msg(MSG_NOTHING_HAPPENS) will be taken
            // care of elsewhere.
            break;
        case 9:
            you_msg        = "Your vision is briefly tinged with green.";
            mon_msg_seen   = "@The_monster@ is briefly tinged with green.";
            mon_msg_unseen = "The air has a green tinge for a moment.";
            break;
        }
        do_msg();
        break;

    case 1:         // a bit less harmless stuff
        switch (random2(2))
        {
        case 0:
            if (target->res_poison() <= 0)
            {
                you_msg      = "You feel sick.";
                mon_msg_seen = "@The_monster@ looks sick.";
                _do_poison(2 + random2(3));
            }
            do_msg();
            break;

        case 1:
            you_msg        = "Noxious gasses pour from your @hands@!";
            mon_msg_seen   = "Noxious gasses pour from @the_monster@'s "
                             "@hands@!";
            mon_msg_unseen = "Noxious gasses pour forth from the thin air!";
            place_cloud(CLOUD_MEPHITIC, target->pos(), 2 + random2(4), guilty);
            break;
        }
        break;

    case 2:         // rather less harmless stuff
        // Don't use last case for monsters.
        switch (random2(target->is_player() ? 3 : 2))
        {
        case 0:
            if (target->res_poison() <= 0)
            {
                you_msg      = "You feel very sick.";
                mon_msg_seen = "@The_monster@ looks very sick.";
                _do_poison(3 + random2avg(9, 2));
            }
            do_msg();
            break;

        case 1:
            you_msg        = "Noxious gasses pour from your @hands@!";
            mon_msg_seen   = "Noxious gasses pour from @the_monster@'s "
                             "@hands@!";
            mon_msg_unseen = "Noxious gasses pour forth from the thin air!";

            _big_cloud(CLOUD_MEPHITIC, 20, 8 + random2(5));
            break;

        case 2:
            if (player_res_poison() > 0)
                canned_msg(MSG_NOTHING_HAPPENS);
            else
                _lose_stat(STAT_RANDOM, 1);
            break;
        }
        break;

    case 3:         // less harmless stuff
        // Don't use last case for monsters.
        switch (random2(target->is_player() ? 3 : 2))
        {
        case 0:
            if (target->res_poison() <= 0)
            {
                you_msg      = "You feel incredibly sick.";
                mon_msg_seen = "@The_monster@ looks incredibly sick.";
                _do_poison(10 + random2avg(19, 2));
            }
            do_msg();
            break;
        case 1:
            you_msg        = "Venomous gasses pour from your @hands@!";
            mon_msg_seen   = "Venomous gasses pour from @the_monster@'s "
                             "@hands@!";
            mon_msg_unseen = "Venomous gasses pour forth from the thin air!";

            _big_cloud(CLOUD_POISON, 20, 7 + random2(7));
            break;
        case 2:
            if (player_res_poison() > 0)
                canned_msg(MSG_NOTHING_HAPPENS);
            else
                _lose_stat(STAT_RANDOM, 1 + random2avg(5, 2));
            break;
        }
        break;
    }
}

void MiscastEffect::_do_poison(int amount)
{
    if (target->is_player())
        poison_player(amount, cause, "residual poison");
    else
        target->poison(act_source, amount);
}

void MiscastEffect::_zot()
{
    bool success = false;
    int roll;
    switch (random2(4))
    {
    case 0:    // mainly explosions
        beam.name = "explosion";
        beam.damage = dice_def(3, 20);
        beam.ex_size = coinflip() ? 1 : 2;
        beam.glyph   = dchar_glyph(DCHAR_FIRED_BURST);
        switch (random2(7))
        {
        case 0:
        case 1:
            all_msg = "There is a sudden explosion of flames!";
            beam.flavour = BEAM_FIRE;
            beam.colour  = RED;
            _explosion();
            break;
        case 2:
        case 3:
            all_msg = "There is a sudden explosion of frost!";
            beam.flavour = BEAM_COLD;
            beam.colour  = WHITE;
            _explosion();
            break;
        case 4:
            all_msg = "There is a sudden blast of acid!";
            beam.name    = "acid blast";
            beam.flavour = BEAM_ACID;
            beam.colour  = YELLOW;
            _explosion();
            break;
        case 5:
            if (_create_monster(MONS_BALL_LIGHTNING, 3))
                all_msg = "A ball of electricity appears!";
            do_msg();
            break;
        case 6:
            if (_create_monster(MONS_TWISTER, 1))
                all_msg = "A huge vortex of air appears!";
            do_msg();
            break;
        }
        break;
    case 1:    // summons
    reroll_1:
        switch (random2(9))
        {
        case 0:
            if (_create_monster(MONS_SOUL_EATER, 4, true))
            {
                you_msg        = "Something reaches out for you...";
                mon_msg_seen   = "Something reaches out for @the_monster@...";
                mon_msg_unseen = "Something reaches out from thin air...";
            }
            do_msg();
            break;
        case 1:
            if (_create_monster(MONS_REAPER, 4, true))
            {
                you_msg        = "Death has come for you...";
                mon_msg_seen   = "Death has come for @the_monster@...";
                mon_msg_unseen = "Death appears from thin air...";
            }
            do_msg();
            break;
        case 2:
            if (_create_monster(summon_any_demon(RANDOM_DEMON_GREATER), 0, true))
                all_msg = "You sense a hostile presence.";
            do_msg();
            break;
        case 3:
        case 4:
            for (int i = 1 + random2(2); i >= 0; --i)
            {
                if (_create_monster(summon_any_demon(RANDOM_DEMON_COMMON), 0, true))
                    success = true;
            }
            if (success)
            {
                you_msg = "Something turns its malign attention towards "
                          "you...";
                mon_msg = "You sense a malign presence.";
                do_msg();
            }
            break;
        case 5:
            for (int i = 3 + random2(5); i >= 0; --i)
            {
                if (_create_monster(summon_any_demon(RANDOM_DEMON_LESSER), 0, true))
                    success = true;
            }
            if (success && neither_end_silenced())
            {
                you_msg        = "A chorus of chattering voices calls out to"
                                 " you!";
                mon_msg        = "A chorus of chattering voices calls out!";
                msg_ch         = MSGCH_SOUND;
                sound_loudness = 3;
                do_msg();
            }
            break;
        case 6:
        case 7:
            for (int i = 2 + random2(3); i >= 0; --i)
            {
                if (_create_monster(RANDOM_MOBILE_MONSTER, 4, true))
                    success = true;
            }
            if (success)
            {
                all_msg = "Wisps of shadow whirl around...";
                do_msg();
            }
            break;
        case 8:
            if (!_malign_gateway())
                goto reroll_1;
            break;
        }
        break;
    case 2:
    case 3:    // other misc stuff
    reroll_2:
        // Cases at the end are for players only.
        switch (random2(target->is_player() ? 15 : 10))
        {
        case 0:
            target->paralyse(act_source, 2 + random2(4), cause);
            break;
        case 1:
            target->petrify(guilty);
            break;
        case 2:
            target->rot(act_source, 0, 3 + random2(3));
            break;
        case 3:
            if (!_send_to_abyss())
                goto reroll_2;
            break;
        case 4:
            you_msg      = "You feel incredibly sick.";
            mon_msg_seen = "@The_monster@ looks incredibly sick.";
            _do_poison(10 + random2avg(19, 2));
            do_msg();
            break;
        case 5:
            if (!target->is_player())
                target->polymorph(0);
            else if (coinflip())
            {
                you_msg = "You feel very strange.";
                delete_mutation(RANDOM_MUTATION, cause, true, false, false, false);
                do_msg();
            }
            else
            {
                you_msg = "Your body is distorted in a weirdly horrible way!";
                mutate(RANDOM_BAD_MUTATION, cause, false, false);
                if (coinflip())
                    mutate(RANDOM_BAD_MUTATION, cause, false, false);
                do_msg();
            }
            break;
        case 6:
        case 7:
            roll = random2(3); // Give 2 of 3 effects.
            if (roll != 0)
                _potion_effect(POT_CONFUSION, 15);
            if (roll != 1)
                _potion_effect(POT_SLOWING, 15);
            if (roll != 2)
            {
                you_msg        = "Space warps around you!";
                mon_msg_seen   = "Space warps around @the_monster@!";
                mon_msg_unseen = "A piece of empty space twists and writhes.";
                _ouch(5 + random2avg(9, 2));
                if (target->alive())
                {
                    if (!target->no_tele())
                    {
                        if (one_chance_in(3))
                            target->teleport(true);
                        else
                            target->blink(false);
                    }
                }
            }
            break;
        case 8:
            you_msg      = "You are engulfed in negative energy!";
            mon_msg_seen = "@The_monster@ is engulfed in negative energy!";
            do_msg();
            target->drain_exp(act_source, false, 100);
            break;
        case 9:
            if (target->is_player())
                contaminate_player(2000 + random2avg(13000, 2), false);
            else
                target->polymorph(0);
            break;
        case 10:
            if (you.magic_points > 0)
            {
                dec_mp(10 + random2(21));
                mpr("You suddenly feel drained of magical energy!", MSGCH_WARN);
            }
            break;
        case 11:
        {
            vector<string> wands;
            for (int i = 0; i < ENDOFPACK; ++i)
            {
                if (!you.inv[i].defined())
                    continue;

                if (you.inv[i].base_type == OBJ_WANDS)
                {
                    const int charges = you.inv[i].plus;
                    if (charges > 0 && coinflip())
                    {
                        you.inv[i].plus -= min(1 + random2(wand_charge_value(you.inv[i].sub_type)), charges);
                        // Display new number of charges when messaging.
                        wands.push_back(you.inv[i].name(DESC_PLAIN));
                    }
                }
            }
            if (!wands.empty())
                mpr_comma_separated_list("Magical energy is drained from your ", wands);
            else
                do_msg(); // For canned_msg(MSG_NOTHING_HAPPENS)
            break;
        }
        case 12:
            _lose_stat(STAT_RANDOM, 1 + random2avg((coinflip() ? 7 : 4), 2));
            break;
        case 13:
            mpr("An unnatural silence engulfs you.");
            you.increase_duration(DUR_SILENCE, 10 + random2(21), 30);
            invalidate_agrid(true);
            break;
        case 14:
            if (mons_word_of_recall(NULL) == 0)
                canned_msg(MSG_NOTHING_HAPPENS);
            break;
        }
        break;
    }
}
