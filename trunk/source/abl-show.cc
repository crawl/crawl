/*
 *  File:       abl-show.cc
 *  Summary:    Functions related to special abilities.
 *  Written by: Linley Henzell
 *
 *  Change History (most recent first):
 *
 *  <6>    19mar2000     jmf    added elvish Glamour
 *  <5>     11/06/99     cdl    reduced power of minor destruction
 *
 *  <4>      9/25/99     cdl    linuxlib -> liblinux
 *
 *  <3>      5/20/99     BWR    Now use scan_randarts to
 *                              check for flags, rather than
 *                              only checking the weapon.
 *
 *  <2>      5/20/99     BWR    Extended screen line support
 *
 *  <1>      -/--/--     LRH             Created
 */

#include "AppHdr.h"
#include "abl-show.h"

#include <string.h>
#include <stdio.h>
#include <ctype.h>

#ifdef DOS
#include <conio.h>
#endif

#include "externs.h"

#include "beam.h"
#include "effects.h"
#include "food.h"
#include "it_use2.h"
#include "macro.h"
#include "message.h"
#include "misc.h"
#include "monplace.h"
#include "player.h"
#include "religion.h"
#include "skills.h"
#include "skills2.h"
#include "spl-cast.h"
#include "spl-util.h"
#include "spells1.h"
#include "spells2.h"
#include "spells3.h"
#include "spells4.h"
#include "stuff.h"
#include "transfor.h"
#include "view.h"


#ifdef LINUX
#include "liblinux.h"
#endif

// this all needs to be split into data/util/show files
// and the struct mechanism here needs to be rewritten (again)
// along with the display routine to piece the strings
// together dynamically ... I'm getting to it now {dlb}

// it makes more sense to think of them as an array
// of structs than two arrays that share common index
// values -- well, doesn't it? {dlb}
struct talent
{
    int which;
    int fail;
    bool is_invocation;
};

static FixedVector< talent, 52 >  Curr_abil;

static bool insert_ability( int which_ability );

// The description screen was way out of date with the actual costs.
// This table puts all the information in one place... -- bwr
// 
// The four numerical fields are: MP, HP, food, and piety.
// Note:  food_cost  = val + random2avg( val, 2 )
//        piety_cost = val + random2( (val + 1) / 2 + 1 );
static const struct ability_def Ability_List[] =
{
    // NON_ABILITY should always come first
    { ABIL_NON_ABILITY, "No ability", 0, 0, 0, 0, ABFLAG_NONE },
    { ABIL_SPIT_POISON, "Spit Poison", 0, 0, 40, 0, ABFLAG_BREATH },
    { ABIL_GLAMOUR, "Glamour", 5, 0, 40, 0, ABFLAG_DELAY },

    { ABIL_MAPPING, "Sense Surroundings", 0, 0, 30, 0, ABFLAG_NONE },
    { ABIL_TELEPORTATION, "Teleportation", 3, 0, 200, 0, ABFLAG_NONE },
    { ABIL_BLINK, "Blink", 1, 0, 50, 0, ABFLAG_NONE },

    { ABIL_BREATHE_FIRE, "Breathe Fire", 0, 0, 125, 0, ABFLAG_BREATH },
    { ABIL_BREATHE_FROST, "Breathe Frost", 0, 0, 125, 0, ABFLAG_BREATH },
    { ABIL_BREATHE_POISON, "Breathe Poison Gas", 0, 0, 125, 0, ABFLAG_BREATH },
    { ABIL_BREATHE_LIGHTNING, "Breathe Lightning", 0, 0, 125, 0, ABFLAG_BREATH },
    { ABIL_BREATHE_POWER, "Breathe Power", 0, 0, 125, 0, ABFLAG_BREATH },
    { ABIL_BREATHE_STICKY_FLAME, "Breathe Sticky Flame", 0, 0, 125, 0, ABFLAG_BREATH },
    { ABIL_BREATHE_STEAM, "Breathe Steam", 0, 0, 75, 0, ABFLAG_BREATH },

    // Handled with breath weapons, but doesn't cause a breathing delay
    { ABIL_SPIT_ACID, "Spit Acid", 0, 0, 125, 0, ABFLAG_NONE },

    { ABIL_FLY, "Fly", 3, 0, 100, 0, ABFLAG_NONE },
    { ABIL_SUMMON_MINOR_DEMON, "Summon Minor Demon", 3, 3, 75, 0, ABFLAG_NONE },
    { ABIL_SUMMON_DEMON, "Summon Demon", 5, 5, 150, 0, ABFLAG_NONE },
    { ABIL_HELLFIRE, "Hellfire", 8, 8, 200, 0, ABFLAG_NONE },
    { ABIL_TORMENT, "Torment", 9, 0, 250, 0, ABFLAG_PAIN },
    { ABIL_RAISE_DEAD, "Raise Dead", 5, 5, 150, 0, ABFLAG_NONE },
    { ABIL_CONTROL_DEMON, "Control Demon", 4, 4, 100, 0, ABFLAG_NONE },
    { ABIL_TO_PANDEMONIUM, "Gate Yourself to Pandemonium", 7, 0, 200, 0, ABFLAG_NONE },
    { ABIL_CHANNELING, "Channeling", 1, 0, 30, 0, ABFLAG_NONE },
    { ABIL_THROW_FLAME, "Throw Flame", 1, 1, 50, 0, ABFLAG_NONE },
    { ABIL_THROW_FROST, "Throw Frost", 1, 1, 50, 0, ABFLAG_NONE },
    { ABIL_BOLT_OF_DRAINING, "Bolt of Draining", 4, 4, 100, 0, ABFLAG_NONE },

    // FLY_II used to have ABFLAG_EXHAUSTION, but that's somewhat meaningless
    // as exhaustion's only (and designed) effect is preventing Berserk. -- bwr
    { ABIL_FLY_II, "Fly", 0, 0, 25, 0, ABFLAG_NONE },
    { ABIL_DELAYED_FIREBALL, "Release Delayed Fireball", 0, 0, 0, 0, ABFLAG_INSTANT },
    { ABIL_MUMMY_RESTORATION, "Restoration", 1, 0, 0, 0, ABFLAG_PERMANENT_MP },

    // EVOKE abilities use Evocations and come from items:
    // Mapping, Teleportation, and Blink can also come from mutations 
    // so we have to distinguish them (see above).  The off items 
    // below are labeled EVOKE because they only work now if the
    // player has an item with the evocable power (not just because
    // you used a wand, potion, or miscast effect).  I didn't see 
    // any reason to label them as "Evoke" in the text, they don't 
    // use or train Evocations (the others do).  -- bwr
    { ABIL_EVOKE_MAPPING, "Evoke Sense Surroundings", 0, 0, 30, 0, ABFLAG_NONE },
    { ABIL_EVOKE_TELEPORTATION, "Evoke Teleportation", 3, 0, 200, 0, ABFLAG_NONE },
    { ABIL_EVOKE_BLINK, "Evoke Blink", 1, 0, 50, 0, ABFLAG_NONE },

    { ABIL_EVOKE_BERSERK, "Evoke Berserk Rage", 0, 0, 0, 0, ABFLAG_NONE },

    { ABIL_EVOKE_TURN_INVISIBLE, "Evoke Invisibility", 2, 0, 250, 0, ABFLAG_NONE },
    { ABIL_EVOKE_TURN_VISIBLE, "Turn Visible", 0, 0, 0, 0, ABFLAG_NONE },
    { ABIL_EVOKE_LEVITATE, "Evoke Levitation", 1, 0, 100, 0, ABFLAG_NONE },
    { ABIL_EVOKE_STOP_LEVITATING, "Stop Levitating", 0, 0, 0, 0, ABFLAG_NONE },

    { ABIL_END_TRANSFORMATION, "End Transformation", 0, 0, 0, 0, ABFLAG_NONE },

    // INVOCATIONS:
    // Zin
    { ABIL_ZIN_REPEL_UNDEAD, "Repel Undead", 1, 0, 100, 0, ABFLAG_NONE },
    { ABIL_ZIN_HEALING, "Minor Healing", 2, 0, 50, 1, ABFLAG_NONE },
    { ABIL_ZIN_PESTILENCE, "Pestilence", 3, 0, 100, 2, ABFLAG_NONE },
    { ABIL_ZIN_HOLY_WORD, "Holy Word", 6, 0, 150, 3, ABFLAG_NONE },
    { ABIL_ZIN_SUMMON_GUARDIAN, "Summon Guardian", 7, 0, 150, 4, ABFLAG_NONE },

    // The Shining One
    { ABIL_TSO_REPEL_UNDEAD, "Repel Undead", 1, 0, 100, 0, ABFLAG_NONE },
    { ABIL_TSO_SMITING, "Smiting", 3, 0, 50, 2, ABFLAG_NONE },
    { ABIL_TSO_ANNIHILATE_UNDEAD, "Annihilate Undead", 3, 0, 50, 2, ABFLAG_NONE },
    { ABIL_TSO_THUNDERBOLT, "Thunderbolt", 5, 0, 100, 2, ABFLAG_NONE },
    { ABIL_TSO_SUMMON_DAEVA, "Summon Daeva", 8, 0, 150, 4, ABFLAG_NONE },

    // Kikubaaqudgha
    { ABIL_KIKU_RECALL_UNDEAD_SLAVES, "Recall Undead Slaves", 2, 0, 50, 0, ABFLAG_NONE },
    { ABIL_KIKU_ENSLAVE_UNDEAD, "Enslave Undead", 4, 0, 150, 3, ABFLAG_NONE },
    { ABIL_KIKU_INVOKE_DEATH, "Invoke Death", 4, 0, 250, 3, ABFLAG_NONE },

    // Yredelemnul
    { ABIL_YRED_ANIMATE_CORPSE, "Animate Corpse", 1, 0, 50, 0, ABFLAG_NONE },
    { ABIL_YRED_RECALL_UNDEAD, "Recall Undead Slaves", 2, 0, 50, 0, ABFLAG_NONE },
    { ABIL_YRED_ANIMATE_DEAD, "Animate Dead", 3, 0, 100, 1, ABFLAG_NONE },
    { ABIL_YRED_DRAIN_LIFE, "Drain Life", 6, 0, 200, 2, ABFLAG_NONE },
    { ABIL_YRED_CONTROL_UNDEAD, "Control Undead", 5, 0, 150, 2, ABFLAG_NONE },

    // Vehumet
    { ABIL_VEHUMET_CHANNEL_ENERGY, "Channel Energy", 0, 0, 50, 0, ABFLAG_NONE },

    // Okawaru
    { ABIL_OKAWARU_MIGHT, "Might", 2, 0, 50, 1, ABFLAG_NONE },
    { ABIL_OKAWARU_HEALING, "Healing", 2, 0, 75, 1, ABFLAG_NONE },
    { ABIL_OKAWARU_HASTE, "Haste", 5, 0, 100, 3, ABFLAG_NONE },

    // Makhleb
    { ABIL_MAKHLEB_MINOR_DESTRUCTION, "Minor Destruction", 1, 0, 20, 0, ABFLAG_NONE },
    { ABIL_MAKHLEB_LESSER_SERVANT_OF_MAKHLEB, "Lesser Servant of Makhleb", 2, 0, 50, 1, ABFLAG_NONE },
    { ABIL_MAKHLEB_MAJOR_DESTRUCTION, "Major Destruction", 4, 0, 100, 2, ABFLAG_NONE },
    { ABIL_MAKHLEB_GREATER_SERVANT_OF_MAKHLEB, "Greater Servant of Makhleb", 6, 0, 100, 3, ABFLAG_NONE },

    // Sif Muna
    { ABIL_SIF_MUNA_FORGET_SPELL, "Forget Spell", 5, 0, 0, 8, ABFLAG_NONE },

    // Trog
    { ABIL_TROG_BERSERK, "Berserk", 0, 0, 200, 0, ABFLAG_NONE },
    { ABIL_TROG_MIGHT, "Might", 0, 0, 200, 1, ABFLAG_NONE },
    { ABIL_TROG_HASTE_SELF, "Haste Self", 0, 0, 250, 3, ABFLAG_NONE },

    // Elyvilon
    { ABIL_ELYVILON_LESSER_HEALING, "Lesser Healing", 1, 0, 100, 0, ABFLAG_NONE },
    { ABIL_ELYVILON_PURIFICATION, "Purification", 2, 0, 150, 1, ABFLAG_NONE },
    { ABIL_ELYVILON_HEALING, "Healing", 2, 0, 250, 2, ABFLAG_NONE },
    { ABIL_ELYVILON_RESTORATION, "Restoration", 3, 0, 400, 3, ABFLAG_NONE },
    { ABIL_ELYVILON_GREATER_HEALING, "Greater Healing", 6, 0, 600, 4, ABFLAG_NONE },

    // These six are unused "evil" god abilities:
    { ABIL_CHARM_SNAKE, "Charm Snake", 6, 0, 200, 5, ABFLAG_NONE },
    { ABIL_TRAN_SERPENT_OF_HELL, "Turn into Demonic Serpent", 16, 0, 600, 8, ABFLAG_NONE },
    { ABIL_BREATHE_HELLFIRE, "Breathe Hellfire", 0, 8, 200, 0, ABFLAG_BREATH },

    { ABIL_ROTTING, "Rotting", 4, 4, 0, 2, ABFLAG_NONE },
    { ABIL_TORMENT_II, "Call Torment", 9, 0, 0, 3, ABFLAG_PAIN },
    { ABIL_SHUGGOTH_SEED, "Sow Shuggoth Seed", 12, 8, 0, 6, ABFLAG_NONE },

    { ABIL_RENOUNCE_RELIGION, "Renounce Religion", 0, 0, 0, 0, ABFLAG_NONE },
};


const struct ability_def & get_ability_def( int abil )
/****************************************************/
{
    for (unsigned int i = 0; i < sizeof( Ability_List ); i++) 
    {
        if (Ability_List[i].ability == abil)
            return (Ability_List[i]);
    }

    return (Ability_List[0]);
}


const char * get_ability_name_by_index( char index )
/**************************************************/
{
    const struct ability_def &abil = get_ability_def( Curr_abil[index].which );

    return (abil.name);
}

 
const std::string make_cost_description( const struct ability_def &abil )
/***********************************************************************/
{
    char         tmp_buff[80];  // avoiding string steams for portability 
    std::string  ret = "";

    if (abil.mp_cost)
    {
        snprintf( tmp_buff, sizeof(tmp_buff), "%d%s MP", 
                  abil.mp_cost, 
                  (abil.flags & ABFLAG_PERMANENT_MP) ? " Permanent" : "" );

        ret += tmp_buff;
    }

    if (abil.hp_cost)
    {
        if (ret.length())
            ret += ", ";

        snprintf( tmp_buff, sizeof(tmp_buff), "%d%s HP", 
                  abil.hp_cost,
                  (abil.flags & ABFLAG_PERMANENT_HP) ? " Permanent" : "" );

        ret += tmp_buff;
    }

    if (abil.food_cost)
    {
        if (ret.length())
            ret += ", ";

        ret += "Food";   // randomized and amount hidden from player
    }

    if (abil.piety_cost)
    {
        if (ret.length())
            ret += ", ";

        ret += "Piety";  // randomized and amount hidden from player
    }

    if (abil.flags & ABFLAG_BREATH)
    {
        if (ret.length())
            ret += ", ";

        ret += "Breath"; 
    }

    if (abil.flags & ABFLAG_DELAY)
    {
        if (ret.length())
            ret += ", ";

        ret += "Delay";
    }

    if (abil.flags & ABFLAG_PAIN)
    {
        if (ret.length())
            ret += ", ";

        ret += "Pain";
    }

    if (abil.flags & ABFLAG_EXHAUSTION)
    {
        if (ret.length())
            ret += ", ";

        ret += "Exhaustion";
    }

    if (abil.flags & ABFLAG_INSTANT)
    {
        if (ret.length())
            ret += ", ";

        ret += "Instant"; // not really a cost, more of a bonus -bwr
    }

    // If we haven't output anything so far, then the effect has no cost
    if (!ret.length())
        ret += "None";

    return (ret);
}


/*
   Activates a menu which gives player access to all of their non-spell
   special abilities - Eg naga's spit poison, or the Invocations you get
   from worshipping. Generated dynamically - the function checks to see which
   abilities you have every time.
 */
bool activate_ability(void)
/*************************/
{
    unsigned char keyin = 0;
    unsigned char spc, spc2;

    int power;
    struct dist abild;
    struct bolt beam;
    struct dist spd;

    unsigned char abil_used;

    // early returns prior to generation of ability list {dlb}:
    if (you.conf)
    {
        mpr("You're too confused!");
        return (false);
    }

    if (you.berserker)
    {
        canned_msg(MSG_TOO_BERSERK);
        return (false);
    }

    // populate the array of structs {dlb}:
    if (!generate_abilities())
    {
        mpr("Sorry, you're not good enough to have a special ability.");
        return (false);
    }

    bool  need_redraw = false;
    bool  need_prompt = true;
    bool  need_getch  = true;

    for (;;)
    {
        if (need_redraw)
        {
            mesclr( true );
            redraw_screen();
        }

        if (need_prompt)
            mpr( "Use which ability? (? or * to list)", MSGCH_PROMPT );

        if (need_getch)
            keyin = get_ch();

        need_redraw = false;
        need_prompt = true;
        need_getch  = true;

        if (isalpha( keyin ))
        {
            break;
        }
        else if (keyin == '?' || keyin == '*')
        {
            keyin = show_abilities();

            need_getch  = false;
            need_redraw = true;
            need_prompt = true;
        }
        else if (keyin == ESCAPE || keyin == ' ' 
                || keyin == '\r' || keyin == '\n')
        {
            canned_msg( MSG_OK );    
            return (false);
        }
    }

    spc = (int) keyin;

    if (!isalpha( spc ))
    {
        mpr("You can't do that.");
        return (false);
    }

    spc2 = letter_to_index(spc);

    if (Curr_abil[spc2].which == -1)
    {
        mpr("You can't do that.");
        return (false);
    }

    abil_used = spc2;

    // some abilities don't need a hunger check
    bool hungerCheck = true;
    switch (Curr_abil[abil_used].which)
    {
        case ABIL_RENOUNCE_RELIGION:
        case ABIL_EVOKE_STOP_LEVITATING:
        case ABIL_EVOKE_TURN_VISIBLE:
        case ABIL_END_TRANSFORMATION:
        case ABIL_DELAYED_FIREBALL:
        case ABIL_MUMMY_RESTORATION:
            hungerCheck = false;
            break;
        default:
            break;
    }

    if (hungerCheck && you.hunger_state < HS_HUNGRY)
    {
        mpr("You're too hungry.");
        return (false);
    }

    // no turning back now... {dlb}
    const struct ability_def abil = get_ability_def(Curr_abil[abil_used].which);

    // currently only delayed fireball is instantaneous -- bwr
    you.turn_is_over = ((abil.flags & ABFLAG_INSTANT) ? 0 : 1);

    if (random2avg(100, 3) < Curr_abil[abil_used].fail)
    {
        mpr("You fail to use your ability.");
        return (false);
    }

    if (!enough_mp( abil.mp_cost, false ))
        return (false);

    if (!enough_hp( abil.hp_cost, false ))
        return (false);

    // Note: the costs will not be applied until after this switch 
    // statement... it's assumed that only failures have returned! -- bwr
    switch (abil.ability)
    {
    case ABIL_MUMMY_RESTORATION:
        mpr( "You infuse your body with magical energy." );
        restore_stat( STAT_ALL, false );
        unrot_hp( 100 );
        break;

    case ABIL_DELAYED_FIREBALL:
        // Note: power level of ball calculated at release -- bwr
        fireball( calc_spell_power( SPELL_DELAYED_FIREBALL, true ) );

        // only one allowed since this is instantaneous -- bwr
        you.attribute[ ATTR_DELAYED_FIREBALL ] = 0;
        break;

    case ABIL_GLAMOUR:
        if (you.duration[DUR_GLAMOUR])
        {
            canned_msg(MSG_CANNOT_DO_YET);
            return (false);
        }

        mpr("You use your Elvish wiles.");

        cast_glamour( 10 + random2(you.experience_level) 
                         + random2(you.experience_level) );

        you.duration[DUR_GLAMOUR] = 20 + random2avg(13, 3);
        break;

    case ABIL_SPIT_POISON:      // Naga + spit poison mutation
        if (you.duration[DUR_BREATH_WEAPON])
        {
            canned_msg(MSG_CANNOT_DO_YET);
            return (false);
        }
        else if (spell_direction(abild, beam) == -1)
        {
            canned_msg(MSG_OK);
            return (false);
        }
        else
        {
            mpr("You spit poison.");

            zapping( ZAP_SPIT_POISON,
                     you.experience_level 
                        + you.mutation[MUT_SPIT_POISON] * 5
                        + (you.species == SP_NAGA) * 10,
                     beam );

            you.duration[DUR_BREATH_WEAPON] = 3 + random2(5);
        }
        break;

    case ABIL_EVOKE_MAPPING:    // randarts
        mpr("You sense your surroundings.");

        magic_mapping(  3 + roll_dice( 2, you.skills[SK_EVOCATIONS] ),
                       40 + roll_dice( 2, you.skills[SK_EVOCATIONS] ) );

        exercise( SK_EVOCATIONS, 1 ); 
        break;

    case ABIL_MAPPING:          // Gnome + sense surrounds mut
        mpr("You sense your surroundings.");

        magic_mapping(  3 + roll_dice( 2, you.experience_level )
                            + you.mutation[MUT_MAPPING] * 10,
                       40 + roll_dice( 2, you.experience_level ) );
        break;

    case ABIL_EVOKE_TELEPORTATION:    // ring of teleportation
    case ABIL_TELEPORTATION:          // teleport mut
        if (you.mutation[MUT_TELEPORT_AT_WILL] == 3)
            you_teleport2( true, true ); // instant and to new area of Abyss
        else
            you_teleport();

        if (abil.ability == ABIL_EVOKE_TELEPORTATION)
            exercise( SK_EVOCATIONS, 1 );
        break;

    case ABIL_BREATHE_FIRE:
    case ABIL_BREATHE_FROST:
    case ABIL_BREATHE_POISON:
    case ABIL_BREATHE_LIGHTNING:
    case ABIL_SPIT_ACID:
    case ABIL_BREATHE_POWER:
    case ABIL_BREATHE_STICKY_FLAME:
    case ABIL_BREATHE_STEAM:
        if (you.duration[DUR_BREATH_WEAPON]
            && Curr_abil[abil_used].which != ABIL_SPIT_ACID)
        {
            canned_msg(MSG_CANNOT_DO_YET);
            return (false);
        }
        else if (spell_direction( abild, beam ) == -1)
        {
            canned_msg(MSG_OK);
            return (false);
        }

        switch (Curr_abil[abil_used].which)
        {
        case ABIL_BREATHE_FIRE:
            power = you.experience_level;
            power += you.mutation[MUT_BREATHE_FLAMES] * 4;

            if (you.attribute[ATTR_TRANSFORMATION] == TRAN_DRAGON)
                power += 12;

            // don't check for hell serpents - they get hell fire,
            // never regular fire (GDL)

            snprintf( info, INFO_SIZE, "You breathe fire%c", (power < 15)?'.':'!');
            mpr(info);

            zapping( ZAP_BREATHE_FIRE, power, beam);
            break;

        case ABIL_BREATHE_FROST:
            mpr("You exhale a wave of freezing cold.");
            zapping(ZAP_BREATHE_FROST, you.experience_level, beam);
            break;

        case ABIL_BREATHE_POISON:
            mpr("You exhale a blast of poison gas.");
            zapping(ZAP_BREATHE_POISON, you.experience_level, beam);
            break;

        case ABIL_BREATHE_LIGHTNING:
            mpr("You spit a bolt of lightning.");
            zapping(ZAP_LIGHTNING, (you.experience_level * 2), beam);
            break;

        case ABIL_SPIT_ACID:
            mpr("You spit acid.");
            zapping(ZAP_BREATHE_ACID, you.experience_level, beam);
            break;

        case ABIL_BREATHE_POWER:
            mpr("You spit a bolt of incandescent energy.");
            zapping(ZAP_BREATHE_POWER, you.experience_level, beam);
            break;

        case ABIL_BREATHE_STICKY_FLAME:
            mpr("You spit a glob of burning liquid.");
            zapping(ZAP_STICKY_FLAME, you.experience_level, beam);
            break;

        case ABIL_BREATHE_STEAM:
            mpr("You exhale a blast of scalding steam.");
            zapping(ZAP_BREATHE_STEAM, you.experience_level, beam);
            break;

        }

        if (Curr_abil[abil_used].which != ABIL_SPIT_ACID)
        {
            you.duration[DUR_BREATH_WEAPON] = 3 + random2(5)
                                                + random2(30 - you.experience_level);
        }

        if (Curr_abil[abil_used].which == ABIL_BREATHE_STEAM)
        {
            you.duration[DUR_BREATH_WEAPON] /= 2;
        }
        break;

    case ABIL_EVOKE_BLINK:      // randarts
    case ABIL_BLINK:            // mutation
        random_blink(true);

        if (abil.ability == ABIL_EVOKE_BLINK)
            exercise( SK_EVOCATIONS, 1 );
        break;

    case ABIL_EVOKE_BERSERK:    // amulet of rage, randarts
        if (you.hunger_state < HS_SATIATED)
        {
            mpr("You're too hungry to berserk.");
            return (false);
        }

        go_berserk(true);
        exercise( SK_EVOCATIONS, 1 );
        break;

    // fly (kenku) -- eventually becomes permanent (see acr.cc)
    case ABIL_FLY:
        cast_fly( you.experience_level * 4 );

        if (you.experience_level > 14)
        {
            mpr("You feel very comfortable in the air.");
            you.levitation = 100;
            you.duration[DUR_CONTROLLED_FLIGHT] = 100;
        }
        break;

    case ABIL_FLY_II:           // Fly (Draconians, or anything else with wings)
        if (you.exhausted)
        {
            mpr("You're too exhausted to fly.");
            return (false);
        }
        else if (you.burden_state != BS_UNENCUMBERED)
        {
            mpr("You're carrying too much weight to fly.");
            return (false);
        }
        else
        {
            cast_fly( you.experience_level * 2 );
            // you.attribute[ATTR_EXPENSIVE_FLIGHT] = 1;  // unused
        }
        break;

    // DEMONIC POWERS:
    case ABIL_SUMMON_MINOR_DEMON:
        summon_ice_beast_etc( you.experience_level * 4,
                                     summon_any_demon(DEMON_LESSER) );
        break;

    case ABIL_SUMMON_DEMON:
        summon_ice_beast_etc( you.experience_level * 4,
                                     summon_any_demon(DEMON_COMMON) );
        break;

    case ABIL_HELLFIRE:
        your_spells(SPELL_HELLFIRE, 20 + you.experience_level, false);
        break;

    case ABIL_TORMENT:
        if (you.is_undead)
        {
            mpr("The unliving cannot use this ability.");
            return (false);
        }

        torment(you.x_pos, you.y_pos);
        break;

    case ABIL_RAISE_DEAD:
        your_spells(SPELL_ANIMATE_DEAD, you.experience_level * 5, false);
        break;

    case ABIL_CONTROL_DEMON:
        if (spell_direction(abild, beam) == -1)
        {
            canned_msg(MSG_OK);
            return (false);
        }

        zapping(ZAP_CONTROL_DEMON, you.experience_level * 5, beam);
        break;

    case ABIL_TO_PANDEMONIUM:
        if (you.level_type == LEVEL_PANDEMONIUM)
        {
            mpr("You're already here.");
            return (false);
        }

        banished(DNGN_ENTER_PANDEMONIUM);
        break;

    case ABIL_CHANNELING:
        mpr("You channel some magical energy.");
        inc_mp(1 + random2(5), false);
        break;

    case ABIL_THROW_FLAME:
    case ABIL_THROW_FROST:
        if (spell_direction(abild, beam) == -1)
        {
            canned_msg(MSG_OK);
            return (false);
        }

        zapping( (Curr_abil[abil_used].which == ABIL_THROW_FLAME ? ZAP_FLAME 
                                                                 : ZAP_FROST),
                    you.experience_level * 3,
                    beam );
        break;

    case ABIL_BOLT_OF_DRAINING:
        if (spell_direction(abild, beam) == -1)
        {
            canned_msg(MSG_OK);
            return (false);
        }

        zapping(ZAP_NEGATIVE_ENERGY, you.experience_level * 6, beam);
        break;

    case ABIL_EVOKE_TURN_INVISIBLE:     // ring, randarts, darkness items
        if (you.hunger_state < HS_SATIATED)
        {
            mpr("You're too hungry to turn invisible.");
            return (false);
        }

        potion_effect( POT_INVISIBILITY, 2 * you.skills[SK_EVOCATIONS] + 5 );
        contaminate_player( 1 + random2(3) );
        exercise( SK_EVOCATIONS, 1 );
        break;

    case ABIL_EVOKE_TURN_VISIBLE:
        mpr("You feel less transparent.");
        you.invis = 1;
        break;

    case ABIL_EVOKE_LEVITATE:           // ring, boots, randarts
        potion_effect( POT_LEVITATION, 2 * you.skills[SK_EVOCATIONS] + 30 );
        exercise( SK_EVOCATIONS, 1 );
        break;

    case ABIL_EVOKE_STOP_LEVITATING:
        mpr("You feel heavy.");
        you.levitation = 1;
        break;

    case ABIL_END_TRANSFORMATION:
        mpr("You feel almost normal.");
        you.duration[DUR_TRANSFORMATION] = 2;
        break;

    // INVOCATIONS:
    case ABIL_ZIN_REPEL_UNDEAD:
    case ABIL_TSO_REPEL_UNDEAD:
        turn_undead(you.piety);

        if (!you.duration[DUR_REPEL_UNDEAD])
            mpr( "You feel a holy aura protecting you." );

        you.duration[DUR_REPEL_UNDEAD] += 8 
                                + roll_dice(2, 2 * you.skills[SK_INVOCATIONS]);

        if (you.duration[ DUR_REPEL_UNDEAD ] > 50)
            you.duration[ DUR_REPEL_UNDEAD ] = 50;

        exercise(SK_INVOCATIONS, 1);
        break;

    case ABIL_ZIN_HEALING:
        if (!cast_healing( 3 + (you.skills[SK_INVOCATIONS] / 6) ))
            break;

        exercise(SK_INVOCATIONS, 1 + random2(3));
        break;

    case ABIL_ZIN_PESTILENCE:
        mpr( "You call forth a swarm of pestilential beasts!" );

        if (!summon_swarm( you.skills[SK_INVOCATIONS] * 8, false, true ))
            mpr( "Nothing seems to have answered your call." );

        exercise( SK_INVOCATIONS, 2 + random2(4) );
        break;

    case ABIL_ZIN_HOLY_WORD:
        holy_word( you.skills[SK_INVOCATIONS] * 8 );
        exercise(SK_INVOCATIONS, 3 + random2(5));
        break;

    case ABIL_ZIN_SUMMON_GUARDIAN:
        summon_ice_beast_etc(you.skills[SK_INVOCATIONS] * 4, MONS_ANGEL);
        exercise(SK_INVOCATIONS, 8 + random2(10));
        break;

    case ABIL_TSO_SMITING:
        cast_smiting( you.skills[SK_INVOCATIONS] * 6 );
        exercise( SK_INVOCATIONS, (coinflip()? 3 : 2) );
        break;

    case ABIL_TSO_ANNIHILATE_UNDEAD:
        if (spell_direction(spd, beam) == -1)
        {
            canned_msg(MSG_OK);
            return (false);
        }

        zapping(ZAP_DISPEL_UNDEAD, you.skills[SK_INVOCATIONS] * 6, beam);
        exercise(SK_INVOCATIONS, 2 + random2(4));
        break;

    case ABIL_TSO_THUNDERBOLT:
        if (spell_direction(spd, beam) == -1)
        {
            canned_msg(MSG_OK);
            return (false);
        }

        zapping(ZAP_LIGHTNING, you.skills[SK_INVOCATIONS] * 6, beam);
        exercise(SK_INVOCATIONS, 3 + random2(6));
        break;

    case ABIL_TSO_SUMMON_DAEVA:
        summon_ice_beast_etc(you.skills[SK_INVOCATIONS] * 4, MONS_DAEVA);
        exercise(SK_INVOCATIONS, 8 + random2(10));
        break;

    case ABIL_KIKU_RECALL_UNDEAD_SLAVES:
        recall(1);
        exercise(SK_INVOCATIONS, 1);
        break;

    case ABIL_KIKU_ENSLAVE_UNDEAD:
        if (spell_direction(spd, beam) == -1)
        {
            canned_msg(MSG_OK);
            return (false);
        }

        zapping( ZAP_ENSLAVE_UNDEAD, you.skills[SK_INVOCATIONS] * 8, beam );
        exercise(SK_INVOCATIONS, 5 + random2(5));
        break;

    case ABIL_KIKU_INVOKE_DEATH:
        summon_ice_beast_etc(20 + you.skills[SK_INVOCATIONS] * 3, MONS_REAPER);
        exercise(SK_INVOCATIONS, 10 + random2(14));
        break;

    case ABIL_YRED_ANIMATE_CORPSE:
        mpr("You call on the dead to walk for you...");

        animate_a_corpse( you.x_pos, you.y_pos, BEH_FRIENDLY, 
                          you.pet_target, CORPSE_BODY );

        exercise(SK_INVOCATIONS, 2 + random2(4));
        break;

    case ABIL_YRED_RECALL_UNDEAD:
        recall(1);
        exercise(SK_INVOCATIONS, 2 + random2(4));
        break;

    case ABIL_YRED_ANIMATE_DEAD:
        mpr("You call on the dead to walk for you...");

        animate_dead( 1 + you.skills[SK_INVOCATIONS], BEH_FRIENDLY,
                      you.pet_target, 1 );

        exercise(SK_INVOCATIONS, 2 + random2(4));
        break;

    case ABIL_YRED_DRAIN_LIFE:
        drain_life( you.skills[SK_INVOCATIONS] );
        exercise(SK_INVOCATIONS, 2 + random2(4));
        break;

    case ABIL_YRED_CONTROL_UNDEAD:
        mass_enchantment( ENCH_CHARM, you.skills[SK_INVOCATIONS] * 8, MHITYOU );
        exercise(SK_INVOCATIONS, 3 + random2(4));
        break;

    case ABIL_VEHUMET_CHANNEL_ENERGY:
        mpr("You channel some magical energy.");

        inc_mp(1 + random2(you.skills[SK_INVOCATIONS] / 4 + 2), false);
        exercise(SK_INVOCATIONS, 1 + random2(3));
        break;

    case ABIL_OKAWARU_MIGHT:
        potion_effect( POT_MIGHT, you.skills[SK_INVOCATIONS] * 8 );
        exercise(SK_INVOCATIONS, 1 + random2(3));
        break;

    case ABIL_OKAWARU_HEALING:
        if (!cast_healing( 3 + (you.skills[SK_INVOCATIONS] / 6) ))
            break;

        exercise(SK_INVOCATIONS, 2 + random2(5));
        break;

    case ABIL_OKAWARU_HASTE:
        potion_effect( POT_SPEED, you.skills[SK_INVOCATIONS] * 8 );
        exercise(SK_INVOCATIONS, 3 + random2(7));
        break;

    case ABIL_MAKHLEB_MINOR_DESTRUCTION:
        if (spell_direction(spd, beam) == -1)
        {
            canned_msg(MSG_OK);
            return (false);
        }

        power = you.skills[SK_INVOCATIONS]
                    + random2( 1 + you.skills[SK_INVOCATIONS] )
                    + random2( 1 + you.skills[SK_INVOCATIONS] );

        switch (random2(5))
        {
        case 0: zapping( ZAP_FLAME,        power,     beam ); break;
        case 1: zapping( ZAP_PAIN,         power,     beam ); break;
        case 2: zapping( ZAP_STONE_ARROW,  power,     beam ); break;
        case 3: zapping( ZAP_ELECTRICITY,  power,     beam ); break;
        case 4: zapping( ZAP_BREATHE_ACID, power / 2, beam ); break;
        }

        exercise(SK_INVOCATIONS, 1);
        break;

    case ABIL_MAKHLEB_LESSER_SERVANT_OF_MAKHLEB:
        summon_ice_beast_etc( 20 + you.skills[SK_INVOCATIONS] * 3,
                              MONS_NEQOXEC + random2(5) );

        exercise(SK_INVOCATIONS, 2 + random2(3));
        break;

    case ABIL_MAKHLEB_MAJOR_DESTRUCTION:
        if (spell_direction(spd, beam) == -1)
        {
            canned_msg(MSG_OK);
            return (false);
        }

        power = you.skills[SK_INVOCATIONS] * 3
                    + random2( 1 + you.skills[SK_INVOCATIONS] )
                    + random2( 1 + you.skills[SK_INVOCATIONS] );

        switch (random2(8))
        {
        case 0: zapping( ZAP_FIRE,               power, beam ); break;
        case 1: zapping( ZAP_FIREBALL,           power, beam ); break;
        case 2: zapping( ZAP_LIGHTNING,          power, beam ); break;
        case 3: zapping( ZAP_NEGATIVE_ENERGY,    power, beam ); break;
        case 4: zapping( ZAP_STICKY_FLAME,       power, beam ); break;
        case 5: zapping( ZAP_IRON_BOLT,          power, beam ); break;
        case 6: zapping( ZAP_ORB_OF_ELECTRICITY, power, beam ); break;

        case 7:
            you.attribute[ATTR_DIVINE_LIGHTNING_PROTECTION] = 1;
            mpr("Makhleb hurls a blast of lightning!");

            // make a divine lightning bolt...
            beam.beam_source = NON_MONSTER;
            beam.type = SYM_BURST;
            beam.damage = dice_def( 3, 30 );
            beam.flavour = BEAM_ELECTRICITY;
            beam.target_x = you.x_pos;
            beam.target_y = you.y_pos;
            strcpy(beam.beam_name, "blast of lightning");
            beam.colour = LIGHTCYAN;
            beam.thrower = KILL_YOU;
            beam.aux_source = "Makhleb's lightning strike";
            beam.ex_size = 1 + you.skills[SK_INVOCATIONS] / 8;
            beam.isTracer = false;

            // ... and fire!
            explosion(beam);

            // protection down
            mpr("Your divine protection wanes.");
            you.attribute[ATTR_DIVINE_LIGHTNING_PROTECTION] = 0;
            break;
        }

        exercise(SK_INVOCATIONS, 3 + random2(5));
        break;

    case ABIL_MAKHLEB_GREATER_SERVANT_OF_MAKHLEB:
        summon_ice_beast_etc( 20 + you.skills[SK_INVOCATIONS] * 3,
                              MONS_EXECUTIONER + random2(5) );

        exercise(SK_INVOCATIONS, 6 + random2(6));
        break;

    case ABIL_TROG_BERSERK:
        // Trog abilities don't use or train invocations. 
        if (you.hunger_state < HS_SATIATED)
        {
            mpr("You're too hungry to berserk.");
            return (false);
        }

        go_berserk(true);
        break;

    case ABIL_TROG_MIGHT:
        // Trog abilities don't use or train invocations. 
        potion_effect( POT_MIGHT, 150 );
        break;

    case ABIL_TROG_HASTE_SELF:
        // Trog abilities don't use or train invocations. 
        potion_effect( POT_SPEED, 150 );
        break;

    case ABIL_SIF_MUNA_FORGET_SPELL:
        cast_selective_amnesia(true);
        break;

    case ABIL_ELYVILON_LESSER_HEALING:
        if (!cast_healing( 3 + (you.skills[SK_INVOCATIONS] / 6) ))
            break;

        exercise( SK_INVOCATIONS, 1 );
        break;

    case ABIL_ELYVILON_PURIFICATION:
        purification();
        exercise( SK_INVOCATIONS, 2 + random2(3) );
        break;

    case ABIL_ELYVILON_HEALING:
        if (!cast_healing( 10 + (you.skills[SK_INVOCATIONS] / 3) ))
            break;

        exercise( SK_INVOCATIONS, 3 + random2(5) );
        break;

    case ABIL_ELYVILON_RESTORATION:
        restore_stat( STAT_ALL, false );
        unrot_hp( 100 );

        exercise( SK_INVOCATIONS, 4 + random2(6) );
        break;

    case ABIL_ELYVILON_GREATER_HEALING:
        if (!cast_healing( 20 + you.skills[SK_INVOCATIONS] * 2 ))
            break;

        exercise( SK_INVOCATIONS, 6 + random2(10) );
        break;

    //jmf: intended as invocations from evil god(s):
    case ABIL_CHARM_SNAKE:
        cast_snake_charm( you.experience_level * 2
                            + you.skills[SK_INVOCATIONS] * 3 );

        exercise(SK_INVOCATIONS, 2 + random2(4));
        break;

    case ABIL_TRAN_SERPENT_OF_HELL:
        transform(10 + (you.experience_level * 2) +
                  (you.skills[SK_INVOCATIONS] * 3), TRAN_SERPENT_OF_HELL);

        exercise(SK_INVOCATIONS, 6 + random2(9));
        break;

    case ABIL_BREATHE_HELLFIRE:
        if (you.duration[DUR_BREATH_WEAPON])
        {
            canned_msg(MSG_CANNOT_DO_YET);
            return (false);
        }

        your_spells( SPELL_HELLFIRE, 20 + you.experience_level, false );

        you.duration[DUR_BREATH_WEAPON] +=
                        3 + random2(5) + random2(30 - you.experience_level);
        break;

    case ABIL_ROTTING:
        cast_rotting(you.experience_level * 2 + you.skills[SK_INVOCATIONS] * 3);
        exercise(SK_INVOCATIONS, 2 + random2(4));
        break;

    case ABIL_TORMENT_II:
        if (you.is_undead)
        {
            mpr("The unliving cannot use this ability.");
            return (false);
        }

        torment(you.x_pos, you.y_pos);
        exercise(SK_INVOCATIONS, 2 + random2(4));
        break;

    case ABIL_SHUGGOTH_SEED:
        if (you.duration[DUR_SHUGGOTH_SEED_RELOAD])
        {
            canned_msg(MSG_CANNOT_DO_YET);
            return (false);
        }

        cast_shuggoth_seed( you.experience_level * 2
                                + you.skills[SK_INVOCATIONS] * 3 );

        you.duration[DUR_SHUGGOTH_SEED_RELOAD] = 10 + random2avg(39, 2);
        exercise(SK_INVOCATIONS, 2 + random2(4));
        break;

    case ABIL_RENOUNCE_RELIGION:
        if (yesno("Really renounce your faith, foregoing its fabulous benefits?")
            && yesno( "Are you sure you won't change your mind later?" ))
        {
            excommunication();
        }
        else
        {
            canned_msg(MSG_OK);
        }
        break;

    default:
        mpr("Sorry, you can't do that.");
        break;
    }

    // All failures should have returned by this point, so we'll 
    // apply the costs -- its not too neat, but it works for now. -- bwr
    const int food_cost = abil.food_cost + random2avg(abil.food_cost, 2);
    const int piety_cost = abil.piety_cost + random2((abil.piety_cost + 1) / 2 + 1);

#if DEBUG_DIAGNOSTICS
    snprintf( info, INFO_SIZE, "Cost: mp=%d; hp=%d; food=%d; piety=%d",
              abil.mp_cost, abil.hp_cost, food_cost, piety_cost );

    mpr( info, MSGCH_DIAGNOSTICS );
#endif

    if (abil.mp_cost)
    {
        dec_mp( abil.mp_cost );
        if (abil.flags & ABFLAG_PERMANENT_MP)
            rot_mp(1);
    }

    if (abil.hp_cost)
    {
        dec_hp( abil.hp_cost, false );
        if (abil.flags & ABFLAG_PERMANENT_HP)
            rot_hp(1);
    }

    if (food_cost)
        make_hungry( food_cost, false );

    if (piety_cost)
        lose_piety( piety_cost );

    return (true);
}  // end activate_ability()


// Lists any abilities the player may possess
char show_abilities( void )
/*************************/
{
    int loopy = 0;
    char lines = 0;
    unsigned char anything = 0;
    char ki;
    bool can_invoke = false;

    const int num_lines = get_number_of_lines();

    for (loopy = 0; loopy < 52; loopy++)
    {
        if (Curr_abil[loopy].is_invocation)
        {
            can_invoke = true;
            break;
        }
    }


#ifdef DOS_TERM
    char buffer[4800];

    gettext(1, 1, 80, 25, buffer);
    window(1, 1, 80, 25);
#endif

    clrscr();
    cprintf("  Ability                           Cost                    Success");
    lines++;

    for (int do_invoke = 0; do_invoke < (can_invoke ? 2 : 1); do_invoke++)
    {
        if (do_invoke)
        {
            anything++;
            textcolor(BLUE);
            cprintf(EOL "    Invocations - ");
            textcolor(LIGHTGREY);
            lines++;
        }

        for (loopy = 0; loopy < 52; loopy++)
        {
            if (lines > num_lines - 2)
            {
                gotoxy(1, num_lines);
                cprintf("-more-");

                ki = getch();

                if (ki == ESCAPE)
                {
#ifdef DOS_TERM
                    puttext(1, 1, 80, 25, buffer);
#endif
                    return (ESCAPE);
                }

                if (ki >= 'A' && ki <= 'z')
                {
#ifdef DOS_TERM
                    puttext(1, 1, 80, 25, buffer);
#endif
                    return (ki);
                }

                if (ki == 0)
                    ki = getch();

                lines = 0;
                clrscr();
                gotoxy(1, 1);
                anything = 0;
            }

            if (Curr_abil[loopy].which != ABIL_NON_ABILITY
                && (do_invoke == Curr_abil[loopy].is_invocation))
            {
                anything++;

                if (lines > 0)
                    cprintf(EOL);

                lines++;

                const struct ability_def abil = get_ability_def( Curr_abil[loopy].which );

                cprintf( " %c - %s", index_to_letter(loopy), abil.name );

                // Output costs:
                gotoxy( 35, wherey() );

                std::string cost_str = make_cost_description( abil );

                if (cost_str.length() > 24)
                    cost_str = cost_str.substr( 0, 24 );

                cprintf( cost_str.c_str() );

                gotoxy(60, wherey());

                int spell_f = Curr_abil[loopy].fail;

                cprintf( (spell_f >= 100) ? "Useless"   :
                         (spell_f >   90) ? "Terrible"  :
                         (spell_f >   80) ? "Cruddy"    :
                         (spell_f >   70) ? "Bad"       :
                         (spell_f >   60) ? "Very Poor" :
                         (spell_f >   50) ? "Poor"      :
                         (spell_f >   40) ? "Fair"      :
                         (spell_f >   30) ? "Good"      :
                         (spell_f >   20) ? "Very Good" :
                         (spell_f >   10) ? "Great"     :
                         (spell_f >    0) ? "Excellent" :
                                            "Perfect" );

                gotoxy(70, wherey());
            }                              // end if conditional
        }                                  // end "for loopy"
    }

    if (anything > 0)
    {
        ki = getch();

        if (ki >= 'A' && ki <= 'z')
        {
#ifdef DOS_TERM
            puttext(1, 1, 80, 25, buffer);
#endif
            return (ki);
        }

        if (ki == 0)
            ki = getch();

#ifdef DOS_TERM
        puttext(1, 1, 80, 25, buffer);
#endif

        return (ki);
    }

#ifdef DOS_TERM
    puttext(1, 1, 80, 25, buffer);
#endif

    ki = getch();

    return (ki);
}                               // end show_abilities()


bool generate_abilities( void )
/*****************************/
{
    int loopy;
    int ability = -1;                   // used with draconian checks {dlb}

    // fill array of structs with "empty" values {dlb}:
    for (loopy = 0; loopy < 52; loopy++)
    {
        Curr_abil[loopy].which = ABIL_NON_ABILITY;
        Curr_abil[loopy].fail = 100;
        Curr_abil[loopy].is_invocation = false;
    }

    // first we do the racial abilities:
    
    // Mummies get the ability to restore HPs and stats, but it
    // costs permanent MP (and those can never be recovered).  -- bwr
    if (you.species == SP_MUMMY && you.experience_level >= 13)
    {
        insert_ability( ABIL_MUMMY_RESTORATION );
    }

    // checking for species-related abilities and mutagenic counterparts {dlb}:
    if (you.attribute[ATTR_TRANSFORMATION] == TRAN_NONE
        && ((you.species == SP_GREY_ELF && you.experience_level >= 5)
            || (you.species == SP_HIGH_ELF && you.experience_level >= 15)))
    {
        insert_ability( ABIL_GLAMOUR );
    }

    if (you.species == SP_NAGA)
    {
        if (you.mutation[MUT_BREATHE_POISON])
            insert_ability( ABIL_BREATHE_POISON );
        else
            insert_ability( ABIL_SPIT_POISON );
    }
    else if (you.mutation[MUT_SPIT_POISON])
    {
        insert_ability( ABIL_SPIT_POISON );
    }

    if (player_genus(GENPC_DRACONIAN))
    {
        if (you.experience_level >= 7)
        {
            ability = (
                (you.species == SP_GREEN_DRACONIAN)  ? ABIL_BREATHE_POISON :
                (you.species == SP_RED_DRACONIAN)    ? ABIL_BREATHE_FIRE :
                (you.species == SP_WHITE_DRACONIAN)  ? ABIL_BREATHE_FROST :
                (you.species == SP_GOLDEN_DRACONIAN) ? ABIL_SPIT_ACID :
                (you.species == SP_BLACK_DRACONIAN)  ? ABIL_BREATHE_LIGHTNING :
                (you.species == SP_PURPLE_DRACONIAN) ? ABIL_BREATHE_POWER :
                (you.species == SP_PALE_DRACONIAN)   ? ABIL_BREATHE_STEAM :
                (you.species == SP_MOTTLED_DRACONIAN)? ABIL_BREATHE_STICKY_FLAME:
                                                     -1);

            if (ability != -1)
                insert_ability( ability );
        }
    }

    //jmf: alternately put check elsewhere
    if ((you.level_type == LEVEL_DUNGEON
            && (you.species == SP_GNOME || you.mutation[MUT_MAPPING]))
        || (you.level_type == LEVEL_PANDEMONIUM
            && you.mutation[MUT_MAPPING] == 3))
    {
        insert_ability( ABIL_MAPPING );
    }

    if (!you.duration[DUR_CONTROLLED_FLIGHT] && !player_is_levitating())
    {
        // kenku can fly, but only from the ground
        // (until levitation 15, when it becomes permanent until revoked)
        //jmf: "upgrade" for draconians -- expensive flight
        if (you.species == SP_KENKU && you.experience_level >= 5)
            insert_ability( ABIL_FLY );
        else if (player_genus(GENPC_DRACONIAN) && you.mutation[MUT_BIG_WINGS])
            insert_ability( ABIL_FLY_II );
    }

    // demonic powers {dlb}:
    if (you.mutation[MUT_SUMMON_MINOR_DEMONS])
        insert_ability( ABIL_SUMMON_MINOR_DEMON );

    if (you.mutation[MUT_SUMMON_DEMONS])
        insert_ability( ABIL_SUMMON_DEMON );

    if (you.mutation[MUT_HURL_HELLFIRE])
        insert_ability( ABIL_HELLFIRE );

    if (you.mutation[MUT_CALL_TORMENT])
        insert_ability( ABIL_TORMENT );

    if (you.mutation[MUT_RAISE_DEAD])
        insert_ability( ABIL_RAISE_DEAD );

    if (you.mutation[MUT_CONTROL_DEMONS])
        insert_ability( ABIL_CONTROL_DEMON );

    if (you.mutation[MUT_PANDEMONIUM])
        insert_ability( ABIL_TO_PANDEMONIUM );

    if (you.mutation[MUT_CHANNEL_HELL])
        insert_ability( ABIL_CHANNELING );

    if (you.mutation[MUT_THROW_FLAMES])
        insert_ability( ABIL_THROW_FLAME );

    if (you.mutation[MUT_THROW_FROST])
        insert_ability( ABIL_THROW_FROST );

    if (you.mutation[MUT_SMITE])
        insert_ability( ABIL_BOLT_OF_DRAINING );

    if (you.duration[DUR_TRANSFORMATION])
        insert_ability( ABIL_END_TRANSFORMATION );

    if (you.mutation[MUT_BLINK]) 
        insert_ability( ABIL_BLINK );

    if (you.mutation[MUT_TELEPORT_AT_WILL])
        insert_ability( ABIL_TELEPORTATION );

    // gods take abilities away until penance completed -- bwr
    if (!player_under_penance() && !silenced( you.x_pos, you.y_pos ))
    {
        switch (you.religion)
        {
        case GOD_ZIN:
            if (you.piety >= 30)
                insert_ability( ABIL_ZIN_REPEL_UNDEAD );
            if (you.piety >= 50)
                insert_ability( ABIL_ZIN_HEALING );
            if (you.piety >= 75)
                insert_ability( ABIL_ZIN_PESTILENCE );
            if (you.piety >= 100)
                insert_ability( ABIL_ZIN_HOLY_WORD );
            if (you.piety >= 120)
                insert_ability( ABIL_ZIN_SUMMON_GUARDIAN );
            break;

        case GOD_SHINING_ONE:
            if (you.piety >= 30)
                insert_ability( ABIL_TSO_REPEL_UNDEAD );
            if (you.piety >= 50)
                insert_ability( ABIL_TSO_SMITING );
            if (you.piety >= 75)
                insert_ability( ABIL_TSO_ANNIHILATE_UNDEAD );
            if (you.piety >= 100)
                insert_ability( ABIL_TSO_THUNDERBOLT );
            if (you.piety >= 120)
                insert_ability( ABIL_TSO_SUMMON_DAEVA );
            break;

        case GOD_YREDELEMNUL:
            if (you.piety >= 30)
                insert_ability( ABIL_YRED_ANIMATE_CORPSE );
            if (you.piety >= 50)
                insert_ability( ABIL_YRED_RECALL_UNDEAD );
            if (you.piety >= 75)
                insert_ability( ABIL_YRED_ANIMATE_DEAD );
            if (you.piety >= 100)
                insert_ability( ABIL_YRED_DRAIN_LIFE );
            if (you.piety >= 120)
                insert_ability( ABIL_YRED_CONTROL_UNDEAD );
            break;

        case GOD_ELYVILON:
            if (you.piety >= 30)
                insert_ability( ABIL_ELYVILON_LESSER_HEALING );
            if (you.piety >= 50)
                insert_ability( ABIL_ELYVILON_PURIFICATION );
            if (you.piety >= 75)
                insert_ability( ABIL_ELYVILON_HEALING );
            if (you.piety >= 100)
                insert_ability( ABIL_ELYVILON_RESTORATION );
            if (you.piety >= 120)
                insert_ability( ABIL_ELYVILON_GREATER_HEALING );
            break;

        case GOD_MAKHLEB:
            if (you.piety >= 50)
                insert_ability( ABIL_MAKHLEB_MINOR_DESTRUCTION );
            if (you.piety >= 75)
                insert_ability( ABIL_MAKHLEB_LESSER_SERVANT_OF_MAKHLEB );
            if (you.piety >= 100)
                insert_ability( ABIL_MAKHLEB_MAJOR_DESTRUCTION );
            if (you.piety >= 120)
                insert_ability( ABIL_MAKHLEB_GREATER_SERVANT_OF_MAKHLEB );
            break;

        case GOD_KIKUBAAQUDGHA:
            if (you.piety >= 30)
                insert_ability( ABIL_KIKU_RECALL_UNDEAD_SLAVES );
            if (you.piety >= 75)
                insert_ability( ABIL_KIKU_ENSLAVE_UNDEAD );
            if (you.piety >= 120)
                insert_ability( ABIL_KIKU_INVOKE_DEATH );
            break;

        case GOD_OKAWARU:
            if (you.piety >= 30)
                insert_ability( ABIL_OKAWARU_MIGHT );
            if (you.piety >= 50)
                insert_ability( ABIL_OKAWARU_HEALING );
            if (you.piety >= 120)
                insert_ability( ABIL_OKAWARU_HASTE );
            break;

        case GOD_TROG:
            if (you.piety >= 30)
                insert_ability( ABIL_TROG_BERSERK );
            if (you.piety >= 50)
                insert_ability( ABIL_TROG_MIGHT );
            if (you.piety >= 100)
                insert_ability( ABIL_TROG_HASTE_SELF );
            break;

        case GOD_SIF_MUNA:
            if (you.piety >= 50)
                insert_ability( ABIL_SIF_MUNA_FORGET_SPELL );
            break;

        case GOD_VEHUMET:
            if (you.piety >= 100)
                insert_ability( ABIL_VEHUMET_CHANNEL_ENERGY );
            break;

        default:
            break;
        }
    }

    // and finally, the ability to opt-out of your faith {dlb}:
    if (you.religion != GOD_NO_GOD && !silenced( you.x_pos, you.y_pos ))
        insert_ability( ABIL_RENOUNCE_RELIGION );

    //jmf: check for breath weapons -- they're exclusive of each other I hope!
    //     better make better ones first.
    if (you.attribute[ATTR_TRANSFORMATION] == TRAN_SERPENT_OF_HELL)
    {
        insert_ability( ABIL_BREATHE_HELLFIRE );
    }
    else if (you.attribute[ATTR_TRANSFORMATION] == TRAN_DRAGON
                                        || you.mutation[MUT_BREATHE_FLAMES])
    {
        insert_ability( ABIL_BREATHE_FIRE );
    }

    // checking for unreleased delayed fireball
    if (you.attribute[ ATTR_DELAYED_FIREBALL ])
    {
        insert_ability( ABIL_DELAYED_FIREBALL );
    }

    // evocations from items:
    if (scan_randarts(RAP_BLINK))
        insert_ability( ABIL_EVOKE_BLINK );

    if (wearing_amulet(AMU_RAGE) || scan_randarts(RAP_BERSERK))
        insert_ability( ABIL_EVOKE_BERSERK );

    if (scan_randarts( RAP_MAPPING ))
        insert_ability( ABIL_EVOKE_MAPPING );

    if (player_equip( EQ_RINGS, RING_INVISIBILITY )
        || player_equip_ego_type( EQ_ALL_ARMOUR, SPARM_DARKNESS )
        || scan_randarts( RAP_INVISIBLE ))
    {
        // Now you can only turn invisibility off if you have an
        // activatable item.  Wands and potions allow will have 
        // to time out. -- bwr
        if (you.invis)
            insert_ability( ABIL_EVOKE_TURN_VISIBLE );
        else
            insert_ability( ABIL_EVOKE_TURN_INVISIBLE );
    }

    //jmf: "upgrade" for draconians -- expensive flight
    // note: this ability only applies to this counter
    if (player_equip( EQ_RINGS, RING_LEVITATION )
        || player_equip_ego_type( EQ_BOOTS, SPARM_LEVITATION )
        || scan_randarts( RAP_LEVITATE ))
    {
        // Now you can only turn levitation off if you have an
        // activatable item.  Potions and miscast effects will 
        // have to time out (this makes the miscast effect actually
        // a bit annoying). -- bwr
        if (you.levitation) 
            insert_ability( ABIL_EVOKE_STOP_LEVITATING );
        else
            insert_ability( ABIL_EVOKE_LEVITATE );
    }

    if (player_equip( EQ_RINGS, RING_TELEPORTATION )
        || scan_randarts( RAP_CAN_TELEPORT ))
    {
        insert_ability( ABIL_EVOKE_TELEPORTATION );
    }

    // this is a shameless kludge for the time being {dlb}:
    // still shameless. -- bwr
    for (loopy = 0; loopy < 52; loopy++)
    {
        if (Curr_abil[loopy].which != ABIL_NON_ABILITY)
            return (true);
    }

    return (false);
}                               // end generate_abilities()

// Note: we're trying for a behaviour where the player gets
// to keep their assigned invocation slots if they get excommunicated
// and then rejoin (but if they spend time with another god we consider 
// the old invocation slots void and erase them).  We also try to 
// protect any bindings the character might have made into the 
// traditional invocation slots (A-E and X). -- bwr
void set_god_ability_helper( int abil, char letter )
/**************************************************/
{
    int i;
    const int index = letter_to_index( letter );

    for (i = 0; i < 52; i++)
    {
        if (you.ability_letter_table[i] == abil)
            break;
    }

    if (i == 52)        // ability is not already assigned
    {
        // if slot is unoccupied, move in
        if (you.ability_letter_table[index] == ABIL_NON_ABILITY)
            you.ability_letter_table[index] = abil;
    }
}

void set_god_ability_slots( void )
/********************************/
{
    ASSERT( you.religion != GOD_NO_GOD );

    int i;

    set_god_ability_helper( ABIL_RENOUNCE_RELIGION, 'X' );

    int num_abil = 0;
    int abil_start = ABIL_NON_ABILITY;

    switch (you.religion)
    {
    case GOD_ZIN:
        abil_start = ABIL_ZIN_REPEL_UNDEAD;
        num_abil = 5; 
        break;

    case GOD_SHINING_ONE:
        abil_start = ABIL_TSO_REPEL_UNDEAD;
        num_abil = 5; 
        break;

    case GOD_KIKUBAAQUDGHA:
        abil_start = ABIL_KIKU_RECALL_UNDEAD_SLAVES;
        num_abil = 3; 
        break;

    case GOD_YREDELEMNUL:
        abil_start = ABIL_YRED_ANIMATE_CORPSE;
        num_abil = 5; 
        break;

    case GOD_VEHUMET:
        abil_start = ABIL_VEHUMET_CHANNEL_ENERGY;
        num_abil = 1; 
        break;

    case GOD_OKAWARU:
        abil_start = ABIL_OKAWARU_MIGHT;
        num_abil = 3; 
        break;

    case GOD_MAKHLEB:
        abil_start = ABIL_MAKHLEB_MINOR_DESTRUCTION;
        num_abil = 4; 
        break;

    case GOD_SIF_MUNA:
        abil_start = ABIL_SIF_MUNA_FORGET_SPELL;
        num_abil = 1; 
        break;

    case GOD_TROG:
        abil_start = ABIL_TROG_BERSERK;
        num_abil = 3; 
        break;

    case GOD_ELYVILON:
        abil_start = ABIL_ELYVILON_LESSER_HEALING;
        num_abil = 5; 
        break;

    case GOD_NEMELEX_XOBEH:
    case GOD_XOM:
    default:
        break;
    }

    // clear out other god invocations:
    for (i = 0; i < 52; i++)
    {
        const int abil = you.ability_letter_table[i];

        if ((abil >= ABIL_ZIN_REPEL_UNDEAD      // is a god ability
                    && abil <= ABIL_ELYVILON_GREATER_HEALING)
            && (num_abil == 0           // current god does have abilities
                || abil < abil_start    // not one of current god's abilities
                || abil >= abil_start + num_abil))
        {
            you.ability_letter_table[i] = ABIL_NON_ABILITY;
        }
    }

    // finally, add in current god's invocaions in traditional slots:
    if (num_abil)
    {
        for (i = 0; i < num_abil; i++)
        {
            set_god_ability_helper( abil_start + i, 
                    (Options.lowercase_invocations ? 'a' : 'A') + i );
        }
    }
}


// returns index to Curr_abil, -1 on failure
static int find_ability_slot( int which_ability )
/***********************************************/
{
    int  slot;
    for (slot = 0; slot < 52; slot++)
    {
        if (you.ability_letter_table[slot] == which_ability)
            break;
    }

    // no requested slot, find new one and make it prefered.
    if (slot == 52)  
    {
        // skip over a-e if player prefers them for invocations
        for (slot = (Options.lowercase_invocations ? 5 : 0); slot < 52; slot++)
        {
            if (you.ability_letter_table[slot] == ABIL_NON_ABILITY)
                break;
        }

        // if we skipped over a-e to reserve them, try them now
        if (Options.lowercase_invocations && slot == 52)
        {
            for (slot = 5; slot >= 0; slot--)
            {
                if (you.ability_letter_table[slot] == ABIL_NON_ABILITY)
                    break;
            }
        }

        // All letters are assigned, check Curr_abil and try to steal a letter
        if (slot == 52)   
        {
            // backwards, to protect the low lettered slots from replacement
            for (slot = 51; slot >= 0; slot--)
            {
                if (Curr_abil[slot].which == ABIL_NON_ABILITY)
                    break;
            }

            // no slots at all == no hope of adding
            if (slot < 0) 
                return (-1);
        }

        // this ability now takes over this slot
        you.ability_letter_table[slot] = which_ability;
    }

    return (slot);
}

static bool insert_ability( int which_ability ) 
/**********************************************/
{
    ASSERT( which_ability != ABIL_NON_ABILITY );

    int failure = 0;
    bool perfect = false;  // is perfect
    bool invoc = false;

    // Look through the table to see if there's a preference, else 
    // find a new empty slot for this ability. -- bwr
    const int slot = find_ability_slot( which_ability );
    if (slot == -1)
        return (false);

    Curr_abil[slot].which = which_ability;

    switch (which_ability)
    {
    // begin spell abilities
    case ABIL_DELAYED_FIREBALL:
    case ABIL_MUMMY_RESTORATION:
        perfect = true;
        failure = 0;
        break;

    // begin species abilities - some are mutagenic, too {dlb}
    case ABIL_GLAMOUR:
        failure = 50 - (you.experience_level * 2);
        break;

    case ABIL_SPIT_POISON:
        failure = ((you.species == SP_NAGA) ? 20 : 40)
                        - 10 * you.mutation[MUT_SPIT_POISON] 
                        - you.experience_level;
        break;

    case ABIL_EVOKE_MAPPING:
        failure = 30 - you.skills[SK_EVOCATIONS]; 
        break;

    case ABIL_MAPPING:
        failure = ((you.species == SP_GNOME) ? 20 : 40) 
                        - 10 * you.mutation[MUT_MAPPING]
                        - you.experience_level;
        break;

    case ABIL_BREATHE_FIRE:
        failure = ((you.species == SP_RED_DRACONIAN) ? 30 : 50)
                        - 10 * you.mutation[MUT_BREATHE_FLAMES]
                        - you.experience_level;

        if (you.attribute[ATTR_TRANSFORMATION] == TRAN_DRAGON)
            failure -= 20;
        break;

    case ABIL_BREATHE_FROST:
    case ABIL_BREATHE_POISON:
    case ABIL_SPIT_ACID:
    case ABIL_BREATHE_LIGHTNING:
    case ABIL_BREATHE_POWER:
    case ABIL_BREATHE_STICKY_FLAME:
        failure = 30 - you.experience_level;

        if (you.attribute[ATTR_TRANSFORMATION] == TRAN_DRAGON)
            failure -= 20;
        break;

    case ABIL_BREATHE_STEAM:
        failure = 20 - you.experience_level;

        if (you.attribute[ATTR_TRANSFORMATION] == TRAN_DRAGON)
            failure -= 20;
        break;

    case ABIL_FLY:              // this is for kenku {dlb}
        failure = 45 - (3 * you.experience_level);
        break;

    case ABIL_FLY_II:           // this is for draconians {dlb}
        failure = 45 - (you.experience_level + you.strength);
        break;
        // end species abilties (some mutagenic)

        // begin demonic powers {dlb}
    case ABIL_THROW_FLAME:
    case ABIL_THROW_FROST:
        failure = 10 - you.experience_level;
        break;

    case ABIL_SUMMON_MINOR_DEMON:
        failure = 27 - you.experience_level;
        break;

    case ABIL_CHANNELING:
    case ABIL_BOLT_OF_DRAINING:
        failure = 30 - you.experience_level;
        break;

    case ABIL_CONTROL_DEMON:
        failure = 35 - you.experience_level;
        break;

    case ABIL_SUMMON_DEMON:
        failure = 40 - you.experience_level;
        break;

    case ABIL_TO_PANDEMONIUM:
        failure = 57 - (you.experience_level * 2);
        break;

    case ABIL_HELLFIRE:
    case ABIL_RAISE_DEAD:
        failure = 50 - you.experience_level;
        break;

    case ABIL_TORMENT:
        failure = 60 - you.experience_level;
        break;

    case ABIL_BLINK:
        failure = 30 - (10 * you.mutation[MUT_BLINK]) - you.experience_level;
        break;

    case ABIL_TELEPORTATION:
        failure = ((you.mutation[MUT_TELEPORT_AT_WILL] > 1) ? 30 : 50) 
                    - you.experience_level;
        break;
        // end demonic powers {dlb}

        // begin transformation abilities {dlb}
    case ABIL_END_TRANSFORMATION:
        perfect = true;
        failure = 0;
        break;

    case ABIL_BREATHE_HELLFIRE:
        failure = 32 - you.experience_level;
        break;
        // end transformation abilities {dlb}
        //
        // begin item abilities - some possibly mutagenic {dlb}
    case ABIL_EVOKE_TURN_INVISIBLE:
    case ABIL_EVOKE_TELEPORTATION:
        failure = 60 - 2 * you.skills[SK_EVOCATIONS];
        break;

    case ABIL_EVOKE_TURN_VISIBLE:
    case ABIL_EVOKE_STOP_LEVITATING:
        perfect = true;
        failure = 0;
        break;

    case ABIL_EVOKE_LEVITATE:
    case ABIL_EVOKE_BLINK:
        failure = 40 - 2 * you.skills[SK_EVOCATIONS];
        break;

    case ABIL_EVOKE_BERSERK:
        failure = 50 - 2 * you.skills[SK_EVOCATIONS];

        if (you.species == SP_TROLL)
            failure -= 30;
        else if (player_genus(GENPC_DWARVEN) || you.species == SP_HILL_ORC 
                || you.species == SP_OGRE)
        {
            failure -= 10;
        }
        break;
        // end item abilities - some possibly mutagenic {dlb}

        // begin invocations {dlb}
    case ABIL_ELYVILON_PURIFICATION:
        invoc = true;
        failure = 20 - (you.piety / 20) - (5 * you.skills[SK_INVOCATIONS]);
        break;

    case ABIL_ZIN_REPEL_UNDEAD:
    case ABIL_TSO_REPEL_UNDEAD:
    case ABIL_KIKU_RECALL_UNDEAD_SLAVES:
    case ABIL_OKAWARU_MIGHT:
    case ABIL_ELYVILON_LESSER_HEALING:
        invoc = true;
        failure = 30 - (you.piety / 20) - (6 * you.skills[SK_INVOCATIONS]);
        break;

    // These three are Trog abilities... Invocations means nothing -- bwr
    case ABIL_TROG_BERSERK:    // piety >= 30
        invoc = true;
        failure = 30 - you.piety;       // starts at 0%
        break;

    case ABIL_TROG_MIGHT:         // piety >= 50
        invoc = true;
        failure = 80 - you.piety;       // starts at 30%
        break;

    case ABIL_TROG_HASTE_SELF:       // piety >= 100
        invoc = true;
        failure = 160 - you.piety;      // starts at 60%
        break;

    case ABIL_YRED_ANIMATE_CORPSE:
        invoc = true;
        failure = 40 - (you.piety / 20) - (3 * you.skills[SK_INVOCATIONS]);
        break;

    case ABIL_ZIN_HEALING:
    case ABIL_TSO_SMITING:
    case ABIL_OKAWARU_HEALING:
    case ABIL_MAKHLEB_MINOR_DESTRUCTION:
    case ABIL_SIF_MUNA_FORGET_SPELL:
    case ABIL_KIKU_ENSLAVE_UNDEAD:
    case ABIL_YRED_ANIMATE_DEAD:
    case ABIL_MAKHLEB_LESSER_SERVANT_OF_MAKHLEB:
    case ABIL_ELYVILON_HEALING:
        invoc = true;
        failure = 40 - (you.piety / 20) - (5 * you.skills[SK_INVOCATIONS]);
        break;

    case ABIL_VEHUMET_CHANNEL_ENERGY:
        invoc = true;
        failure = 40 - you.intel - you.skills[SK_INVOCATIONS];
        break;

    case ABIL_YRED_RECALL_UNDEAD:
        invoc = true;
        failure = 50 - (you.piety / 20) - (you.skills[SK_INVOCATIONS] * 4);
        break;

    case ABIL_ZIN_PESTILENCE:
    case ABIL_TSO_ANNIHILATE_UNDEAD:
        invoc = true;
        failure = 60 - (you.piety / 20) - (5 * you.skills[SK_INVOCATIONS]);
        break;

    case ABIL_MAKHLEB_MAJOR_DESTRUCTION:
    case ABIL_YRED_DRAIN_LIFE:
        invoc = true;
        failure = 60 - (you.piety / 25) - (you.skills[SK_INVOCATIONS] * 4);
        break;

    case ABIL_ZIN_HOLY_WORD:
    case ABIL_TSO_THUNDERBOLT:
    case ABIL_ELYVILON_RESTORATION:
    case ABIL_YRED_CONTROL_UNDEAD:
    case ABIL_OKAWARU_HASTE:
    case ABIL_MAKHLEB_GREATER_SERVANT_OF_MAKHLEB:
        invoc = true;
        failure = 70 - (you.piety / 25) - (you.skills[SK_INVOCATIONS] * 4);
        break;

    case ABIL_ZIN_SUMMON_GUARDIAN:
    case ABIL_TSO_SUMMON_DAEVA:
    case ABIL_KIKU_INVOKE_DEATH:
    case ABIL_ELYVILON_GREATER_HEALING:
        invoc = true;
        failure = 80 - (you.piety / 25) - (you.skills[SK_INVOCATIONS] * 4);
        break;

        //jmf: following for to-be-created gods
    case ABIL_CHARM_SNAKE:
        invoc = true;
        failure = 40 - (you.piety / 20) - (3 * you.skills[SK_INVOCATIONS]);
        break;

    case ABIL_TRAN_SERPENT_OF_HELL:
        invoc = true;
        failure = 80 - (you.piety / 25) - (you.skills[SK_INVOCATIONS] * 4);
        break;

    case ABIL_ROTTING:
        invoc = true;
        failure = 60 - (you.piety / 20) - (5 * you.skills[SK_INVOCATIONS]);
        break;

    case ABIL_TORMENT_II:
        invoc = true;
        failure = 70 - (you.piety / 25) - (you.skills[SK_INVOCATIONS] * 4);
        break;

    case ABIL_SHUGGOTH_SEED:
        invoc = true;
        failure = 85 - (you.piety / 25) - (you.skills[SK_INVOCATIONS] * 3);
        break;

    case ABIL_RENOUNCE_RELIGION:
        invoc = true;
        perfect = true;
        failure = 0;
        break;

        // end invocations {dlb}
    default:
        failure = -1;
        break;
    }

    // Perfect abilities are things like "renounce religion", which
    // shouldn't have a failure rate ever. -- bwr
    if (failure <= 0 && !perfect)
        failure = 1;

    if (failure > 100)
        failure = 100;

    Curr_abil[slot].fail = failure;
    Curr_abil[slot].is_invocation = invoc;

    return (true);
}                               // end insert_ability()
