/*
 *  File:       spl-book.cc
 *  Summary:    Spellbook/Staff contents array and management functions
 *  Written by: Josh Fishman
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 *      24jun2000  jmf  Changes to many books; addition of Assassinations;
 *                      alteration of type-printout to match new bitfield.
 *  <1> 19mar2000  jmf  Created by taking stuff from dungeon.cc, spells0.cc,
 *                      spells.cc, shopping.cc
 */

#include "AppHdr.h"
#include "spl-book.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef DOS
#include <conio.h>
#endif

#include "externs.h"
#include "cio.h"
#include "debug.h"
#include "delay.h"
#include "food.h"
#include "invent.h"
#include "itemname.h"
#include "itemprop.h"
#include "items.h"
#include "it_use3.h"
#include "player.h"
#include "religion.h"
#include "spl-cast.h"
#include "spl-util.h"
#include "stuff.h"

#define SPELLBOOK_SIZE 8
#define NUMBER_SPELLBOOKS 61

static spell_type spellbook_template_array[NUMBER_SPELLBOOKS][SPELLBOOK_SIZE] =
{
    // 0 - Minor Magic I
    {SPELL_MAGIC_DART,
     SPELL_SUMMON_SMALL_MAMMAL,
     SPELL_THROW_FLAME,
     SPELL_BLINK,
     SPELL_SLOW,
     SPELL_MEPHITIC_CLOUD,
     SPELL_CONJURE_FLAME,
     SPELL_NO_SPELL,
     },
    // 1 - Minor Magic II
    {SPELL_MAGIC_DART,
     SPELL_THROW_FROST,
     SPELL_BLINK,
     SPELL_STICKS_TO_SNAKES,
     SPELL_SLOW,
     SPELL_MEPHITIC_CLOUD,
     SPELL_OZOCUBUS_ARMOUR,
     SPELL_NO_SPELL,
     },
    // 2 - Minor Magic III
    {SPELL_MAGIC_DART,
     SPELL_SUMMON_SMALL_MAMMAL,
     SPELL_BLINK,
     SPELL_REPEL_MISSILES,
     SPELL_SLOW,
     SPELL_SUMMON_LARGE_MAMMAL,
     SPELL_MEPHITIC_CLOUD,
     SPELL_NO_SPELL,
     },
    // 3 - Book of Conjurations I - Fire and Earth
    {SPELL_MAGIC_DART,
     SPELL_THROW_FLAME,
     SPELL_STONE_ARROW,
     SPELL_CONJURE_FLAME,
     SPELL_BOLT_OF_MAGMA,
     SPELL_FIREBALL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     },
    // 4 - Book of Conjurations II - Air and Ice
    {SPELL_MAGIC_DART,
     SPELL_THROW_FROST,
     SPELL_MEPHITIC_CLOUD,
     SPELL_DISCHARGE,
     SPELL_BOLT_OF_COLD,
     SPELL_FREEZING_CLOUD,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     },
    // 5 - Book of Flames
    {SPELL_FLAME_TONGUE,
     SPELL_THROW_FLAME,
     SPELL_CONJURE_FLAME,
     SPELL_STICKY_FLAME,
     SPELL_BOLT_OF_FIRE,
     SPELL_FIREBALL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     },
    // 6 - Book of Frost
    {SPELL_FREEZE,
     SPELL_THROW_FROST,
     SPELL_OZOCUBUS_ARMOUR,
     SPELL_ICE_BOLT,
     SPELL_SUMMON_ICE_BEAST,
     SPELL_FREEZING_CLOUD,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     },
    // 7 - Book of Summonings
    {SPELL_ABJURATION_I,
     SPELL_RECALL,
     SPELL_SUMMON_LARGE_MAMMAL,
     SPELL_SHADOW_CREATURES,
     SPELL_SUMMON_WRAITHS,
     SPELL_SUMMON_HORRIBLE_THINGS,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     },
    // 8 - Book of Fire
    {SPELL_EVAPORATE,
     SPELL_FIRE_BRAND,
     SPELL_SUMMON_ELEMENTAL,
     SPELL_BOLT_OF_MAGMA,
     SPELL_DELAYED_FIREBALL,
     SPELL_IGNITE_POISON,
     SPELL_RING_OF_FLAMES,
     SPELL_NO_SPELL,
     },
    // 9 - Book of Ice
    {SPELL_FREEZING_AURA,
     SPELL_SLEEP,
     SPELL_CONDENSATION_SHIELD,
     SPELL_BOLT_OF_COLD,
     SPELL_OZOCUBUS_REFRIGERATION,
     SPELL_SIMULACRUM,
     SPELL_MASS_SLEEP,
     SPELL_NO_SPELL,
     },

    // 10 - Book of Surveyances
    {SPELL_DETECT_SECRET_DOORS,
     SPELL_DETECT_TRAPS,
     SPELL_DETECT_ITEMS,
     SPELL_MAGIC_MAPPING,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL
     },
    // 11 - Book of Spatial Translocations
    {SPELL_APPORTATION,
     SPELL_PORTALED_PROJECTILE,
     SPELL_BLINK,
     SPELL_RECALL,
     SPELL_TELEPORT_OTHER,
     SPELL_TELEPORT_SELF,
     SPELL_CONTROL_TELEPORT,
     SPELL_NO_SPELL,
     },
    // 12 - Book of Enchantments (fourth one)
    {SPELL_LEVITATION,
     SPELL_SELECTIVE_AMNESIA,
     SPELL_REMOVE_CURSE,
     SPELL_CAUSE_FEAR,
     SPELL_EXTENSION,
     SPELL_DEFLECT_MISSILES,
     SPELL_HASTE,
     SPELL_NO_SPELL,
     },
    // 13 - Young Poisoner's Handbook
    {SPELL_STING,
     SPELL_CURE_POISON_II,
     SPELL_MEPHITIC_CLOUD,
     SPELL_POISON_WEAPON,
     SPELL_VENOM_BOLT,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     },
    // 14 - Book of the Tempests
    {SPELL_DISCHARGE,
     SPELL_LIGHTNING_BOLT,
     SPELL_FIREBALL,
     SPELL_SHATTER,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     },
    // 15 - Book of Death
    {SPELL_CORPSE_ROT,
     SPELL_BONE_SHARDS,
     SPELL_LETHAL_INFUSION,
     SPELL_AGONY,
     SPELL_BOLT_OF_DRAINING,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     },
    // 16 - Book of Hinderance
    {SPELL_CONFUSING_TOUCH,
     SPELL_SLOW,
     SPELL_CONFUSE,
     SPELL_PARALYSE,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     },
    // 17 - Book of Changes
    {SPELL_FULSOME_DISTILLATION,
     SPELL_STICKS_TO_SNAKES,
     SPELL_EVAPORATE,
     SPELL_SPIDER_FORM,
     SPELL_ICE_FORM,
     SPELL_DIG,
     SPELL_BLADE_HANDS,
     SPELL_NO_SPELL,
     },
    // 18 - Book of Transfigurations
    {SPELL_SANDBLAST,
     SPELL_POLYMORPH_OTHER,
     SPELL_STATUE_FORM,
     SPELL_ALTER_SELF,
     SPELL_DRAGON_FORM,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     },
    // 19 - Book of Practical Magic
    {SPELL_PROJECTED_NOISE,
     SPELL_SELECTIVE_AMNESIA,
     SPELL_DETECT_CURSE,
     SPELL_DIG,
     SPELL_REMOVE_CURSE,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     },

    // 20 - Book of War Chants
    {SPELL_FIRE_BRAND,
     SPELL_FREEZING_AURA,
     SPELL_REPEL_MISSILES,
     SPELL_BERSERKER_RAGE,
     SPELL_REGENERATION,
     SPELL_HASTE,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     },
    // 21 - Book of Clouds
    {SPELL_EVAPORATE,
     SPELL_MEPHITIC_CLOUD,
     SPELL_CONJURE_FLAME,
     SPELL_POISONOUS_CLOUD,
     SPELL_FREEZING_CLOUD,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     },
    // 22 - Book of Healing
    {SPELL_CURE_POISON_I,
     SPELL_LESSER_HEALING,
     SPELL_GREATER_HEALING,
     SPELL_PURIFICATION,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     },
    // 23 - Book of Necromancy 
    {SPELL_PAIN,
     SPELL_ANIMATE_SKELETON,
     SPELL_VAMPIRIC_DRAINING,
     SPELL_REGENERATION,
     SPELL_DISPEL_UNDEAD,
     SPELL_ANIMATE_DEAD,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     },
    // 24 - Necronomicon -- Kikubaaqudgha special
    {SPELL_SYMBOL_OF_TORMENT,
     SPELL_CONTROL_UNDEAD,
     SPELL_SUMMON_WRAITHS,
     SPELL_DEATHS_DOOR,
     SPELL_NECROMUTATION,
     SPELL_DEATH_CHANNEL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     },
    // 25 - Book of Callings
    {SPELL_SUMMON_SMALL_MAMMAL,
     SPELL_STICKS_TO_SNAKES,
     SPELL_CALL_IMP,
     SPELL_SUMMON_ELEMENTAL,
     SPELL_SUMMON_SCORPIONS,
     SPELL_SUMMON_ICE_BEAST,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     },
    // 26 - Book of Charms
    {SPELL_BACKLIGHT,
     SPELL_REPEL_MISSILES,
     SPELL_SLEEP,
     SPELL_CONFUSE,
     SPELL_ENSLAVEMENT,
     SPELL_SILENCE,
     SPELL_INVISIBILITY,
     SPELL_NO_SPELL,
     },
    // 27 - Book of Demonology  -- Vehumet special
    {SPELL_ABJURATION_I,
     SPELL_RECALL,
     SPELL_CALL_IMP,
     SPELL_SUMMON_DEMON,
     SPELL_DEMONIC_HORDE,
     SPELL_SUMMON_GREATER_DEMON,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     },
    // 28 - Book of Air
    {SPELL_SHOCK,
     SPELL_SWIFTNESS,
     SPELL_REPEL_MISSILES,
     SPELL_LEVITATION,
     SPELL_MEPHITIC_CLOUD,
     SPELL_DISCHARGE,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     },

    // Removing Air Walk... it's currently broken in a number or ways:
    // - the AC/EV bonus probably means very little to any character
    //   capable of casting a level nine spell
    // - the penalty of dropping inventory is a bit harsh considering
    //   the above
    // - the fire/ice susceptibility doesn't work... the AC/EV bonus
    //   cancel it out (so you'd hardly be wary of them since they
    //   can't really do any damage).
    // - there is no need for another invulnerability spell (which is
    //   what this spell is trying to be), one is more than enough...
    //   let people use necromancy.
    // - there is no need for another air spell... air spells are
    //   already very common (ie. this level nine spell occured in
    //   two books!)

    // 29 - Book of the Sky
    {SPELL_SUMMON_ELEMENTAL,
     SPELL_INSULATION,
     SPELL_AIRSTRIKE,
     SPELL_FLY,
     SPELL_SILENCE,
     SPELL_DEFLECT_MISSILES,
     SPELL_LIGHTNING_BOLT,
     SPELL_CONJURE_BALL_LIGHTNING, 
     },

    // 30 - Book of Divinations
    {SPELL_DETECT_SECRET_DOORS,
     SPELL_DETECT_CREATURES,
     SPELL_DETECT_ITEMS,
     SPELL_DETECT_CURSE,
     SPELL_SEE_INVISIBLE,
     SPELL_FORESCRY,
     SPELL_IDENTIFY,
     SPELL_NO_SPELL,
     },
    // 31 - Book of the Warp
    {SPELL_BANISHMENT,
     SPELL_WARP_BRAND,
     SPELL_DISPERSAL,
     SPELL_PORTAL,
     SPELL_CONTROLLED_BLINK,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     },
    // 32 - Book of Envenomations
    {SPELL_SPIDER_FORM,
     SPELL_POISON_AMMUNITION,
     SPELL_SUMMON_SCORPIONS,
     SPELL_RESIST_POISON,
     SPELL_OLGREBS_TOXIC_RADIANCE,
     SPELL_POISONOUS_CLOUD,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     },
    // 33 - Book of Annihilations -- Vehumet special
    {SPELL_ISKENDERUNS_MYSTIC_BLAST,
     SPELL_POISON_ARROW,
     SPELL_CHAIN_LIGHTNING,
     SPELL_LEHUDIBS_CRYSTAL_SPEAR,
     SPELL_ICE_STORM,
     SPELL_FIRE_STORM,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     },
    // 34 - Book of Unlife
    {SPELL_SUBLIMATION_OF_BLOOD,
     SPELL_ANIMATE_DEAD,
     SPELL_TWISTED_RESURRECTION,
     SPELL_BORGNJORS_REVIVIFICATION,
     SPELL_EXCRUCIATING_WOUNDS,
     SPELL_SIMULACRUM,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     },

    // 35 - Tome of Destruction
    {SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     },
    // 36 - Book of Control
    {SPELL_ENSLAVEMENT,
     SPELL_TAME_BEASTS,
     SPELL_MASS_CONFUSION,
     SPELL_CONTROL_UNDEAD,
     SPELL_CONTROL_TELEPORT,
     SPELL_MASS_SLEEP,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     },
    // 37 - Book of Morphology
    {SPELL_FRAGMENTATION,
     SPELL_POLYMORPH_OTHER,
     SPELL_ALTER_SELF,
     SPELL_CIGOTUVIS_DEGENERATION,
     // SPELL_IGNITE_POISON,    // moved to Fire which was a bit slim -- bwr
     SPELL_SHATTER,
     // SPELL_AIR_WALK,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     },
    // 38 - Book of Tukima
    {SPELL_SURE_BLADE,
     SPELL_TUKIMAS_VORPAL_BLADE,
     SPELL_TUKIMAS_DANCE,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     },
    // 39 - Book of Geomancy
    {SPELL_SANDBLAST,
     SPELL_STONESKIN,
     SPELL_PASSWALL,
     SPELL_STONE_ARROW,
     SPELL_SUMMON_ELEMENTAL,
     SPELL_FRAGMENTATION,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     },

    // 40 - Book of Earth
    {SPELL_MAXWELLS_SILVER_HAMMER,
     SPELL_MAGIC_MAPPING,
     SPELL_DIG,
     SPELL_STATUE_FORM,
     SPELL_BOLT_OF_IRON,
     SPELL_SHATTER,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL
     },
    // 41 - manuals of all kinds
    {SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     },
    // 42 - Book of Wizardry
    {SPELL_DETECT_CREATURES,
     SPELL_SUMMON_ELEMENTAL,
     SPELL_MAGIC_MAPPING,
     SPELL_TELEPORT_SELF,
     SPELL_FIREBALL,
     SPELL_IDENTIFY,
     SPELL_HASTE,
     SPELL_NO_SPELL,
     },
    // 43 - Book of Power
    {SPELL_ANIMATE_DEAD,
     SPELL_TELEPORT_OTHER,
     SPELL_VENOM_BOLT,
     SPELL_BOLT_OF_IRON,
     SPELL_INVISIBILITY,
     SPELL_MASS_CONFUSION,
     SPELL_POISONOUS_CLOUD,
     SPELL_NO_SPELL,
     },
    // 44 - Book of Cantrips      //jmf: added 04jan2000
    {SPELL_CONFUSING_TOUCH,
     SPELL_ANIMATE_SKELETON,
     SPELL_SUMMON_SMALL_MAMMAL,
     SPELL_DETECT_SECRET_DOORS,
     SPELL_APPORTATION,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     },

    // 45 - Book of Party Tricks  //jmf: added 04jan2000
    {SPELL_SUMMON_BUTTERFLIES,
     SPELL_APPORTATION,
     SPELL_PROJECTED_NOISE,
     SPELL_BLINK,
     SPELL_LEVITATION,
     SPELL_INTOXICATE,
     SPELL_NO_SPELL,  //jmf: SPELL_ERINGYAS_SURPRISING_BOUQUET, when finished
     SPELL_NO_SPELL,
     },

    // 46 - Book of Beasts //jmf: added 19mar2000
    {SPELL_SUMMON_SMALL_MAMMAL,
     SPELL_STICKS_TO_SNAKES,
     SPELL_DETECT_CREATURES,
     SPELL_SUMMON_LARGE_MAMMAL,
     SPELL_TAME_BEASTS,
     SPELL_DRAGON_FORM,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     },

    // 47 - Book of Stalking //jmf: 24jun2000
    {SPELL_STING,
     SPELL_SURE_BLADE,
     SPELL_PROJECTED_NOISE,
     SPELL_MEPHITIC_CLOUD,
     SPELL_POISON_WEAPON,
     SPELL_PARALYSE,
     SPELL_INVISIBILITY,
     SPELL_NO_SPELL,
     },

    // 48 -- unused
    {SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     },

    // 49 - unused
    {SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     },

    // Rods - start at 50.

    // 50 - Rod of smiting
    {SPELL_SMITING,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     },

    // 51 - Rod of summoning
    {SPELL_ABJURATION_I,
     SPELL_RECALL,
     SPELL_SUMMON_ELEMENTAL,
     SPELL_SWARM,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     },
    // 52 - Rod of destruction (fire)
    {SPELL_THROW_FLAME,
     SPELL_BOLT_OF_FIRE,
     SPELL_FIREBALL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     },
    // 53 - Rod of destruction (ice)
    {SPELL_THROW_FROST,
     SPELL_ICE_BOLT,
     SPELL_FREEZING_CLOUD,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     },
    // 54 - Rod of destruction (iron, fireball, lightning)
    {SPELL_BOLT_OF_IRON,
     SPELL_FIREBALL,
     SPELL_LIGHTNING_BOLT,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     },
    // 55 - Rod of destruction (bolts)
    {SPELL_BOLT_OF_INACCURACY,
     SPELL_BOLT_OF_MAGMA,
     SPELL_BOLT_OF_COLD,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     },
    // 56 - Rod of warding
    {SPELL_ABJURATION_I,
     SPELL_CONDENSATION_SHIELD,
     SPELL_CAUSE_FEAR,
     SPELL_DEFLECT_MISSILES,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     },
    // 57 - Rod of discovery
    {SPELL_DETECT_SECRET_DOORS,
     SPELL_DETECT_TRAPS,
     SPELL_DETECT_ITEMS,
     SPELL_MAGIC_MAPPING,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     },
    // 58 - Rod of demonology
    {SPELL_ABJURATION_I,
     SPELL_RECALL,
     SPELL_CALL_IMP,
     SPELL_SUMMON_DEMON,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     },
    // 59 - Rod of striking
    {SPELL_STRIKING,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     },
    // 60 - Rod of venom
    {SPELL_CURE_POISON_II,
     SPELL_VENOM_BOLT,
     SPELL_POISON_ARROW,
     SPELL_POISONOUS_CLOUD,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL,
     SPELL_NO_SPELL
    },
};

spell_type which_spell_in_book(int sbook_type, int spl)
{
    ASSERT( sbook_type >= 0 );
    ASSERT( sbook_type < NUMBER_SPELLBOOKS );
    return spellbook_template_array[sbook_type][spl];
}                               // end which_spell_in_book()

// If fs is not NULL, updates will be to the formatted_string instead of
// the display.
int spellbook_contents( item_def &book, read_book_action_type action,
                        formatted_string *fs )
{
    int spelcount = 0;
    int i, j;
    bool update_screen = !fs;

    const int spell_levels = player_spell_levels();
    const int type = book.book_number();

    bool spell_skills = false;

    for (i = SK_SPELLCASTING; i <= SK_POISON_MAGIC; i++) 
    {
        if (you.skills[i])
        {
            spell_skills = true;
            break;
        }
    }

    set_ident_flags( book, ISFLAG_KNOW_TYPE );

    formatted_string out;
    out.textcolor(LIGHTGREY);

    out.cprintf( "%s", book.name(DESC_CAP_THE).c_str() );

    out.cprintf( EOL EOL " Spells                             Type                      Level" EOL );

    for (j = 0; j < SPELLBOOK_SIZE; j++)
    {
        spell_type stype = which_spell_in_book(type, j);
        if (stype == SPELL_NO_SPELL)
            continue;

        out.cprintf(" ");

        bool knowsSpell = false;
        for (i = 0; i < 25 && !knowsSpell; i++)
        {
            knowsSpell = (you.spells[i] == stype);
        }

        const int level_diff = spell_difficulty( stype );
        const int levels_req = spell_levels_required( stype );

        int colour = DARKGREY;
        if (action == RBOOK_USE_STAFF)
        {
            if (you.experience_level >= level_diff 
                && (book.base_type == OBJ_BOOKS?
                        you.magic_points >= level_diff
                      : book.plus >= level_diff * ROD_CHARGE_MULT))
            {
                colour = LIGHTGREY;
            }
        }
        else
        {
            if (knowsSpell)
                colour = LIGHTGREY;
            else if (you.experience_level >= level_diff 
                        && spell_levels >= levels_req
                        && spell_skills)
            {
                colour = LIGHTBLUE;
            }
        }

        out.textcolor( colour );

        // Old:
        // textcolor(knowsSpell ? DARKGREY : LIGHTGREY);
        //              was: ? LIGHTGREY : LIGHTBLUE

        char strng[2];

        strng[0] = index_to_letter(spelcount);
        strng[1] = 0;

        out.cprintf(strng);
        out.cprintf(" - ");

        out.cprintf( "%s", spell_title(stype) );
        out.gotoxy( 35, -1 );

        
        if (action == RBOOK_USE_STAFF)
            out.cprintf( "Evocations" );
        else
        {
            bool already = false;

            for (i = 0; i <= SPTYP_LAST_EXPONENT; i++)
            {
                if (spell_typematch( stype, 1 << i ))
                {
                    if (already)
                        out.cprintf( "/" );

                    out.cprintf( "%s", spelltype_name( 1 << i ) );
                    already = true;
                }
            }
        }

        out.gotoxy( 65, -1 );

        char sval[3];

        itoa( level_diff, sval, 10 );
        out.cprintf( sval );
        out.cprintf( EOL );
        spelcount++;
    }

    out.textcolor(LIGHTGREY);
    out.cprintf(EOL);

    switch (action)
    {
    case RBOOK_USE_STAFF:
        out.cprintf( "Select a spell to cast." EOL );
        break;

    case RBOOK_MEMORISE:
        out.cprintf( "Select a spell to memorise (%d level%s available)." EOL,
                 spell_levels, (spell_levels == 1) ? "" : "s" );
        break;

    case RBOOK_READ_SPELL:
        out.cprintf( "Select a spell to read its description." EOL );
        break;

    default:
        break;
    }

    if (fs)
        *fs = out;

    int keyn = 0;
    if (update_screen)
    {
        cursor_control coff(false);
        clrscr();

        out.display();

        keyn = c_getch();
    }

    return (keyn);     // try to figure out that for which this is used {dlb}
}

//jmf: was in shopping.cc
int book_rarity(unsigned char which_book)
{
    switch (which_book)
    {
    case BOOK_MINOR_MAGIC_I:
    case BOOK_MINOR_MAGIC_II:
    case BOOK_MINOR_MAGIC_III:
    case BOOK_SURVEYANCES:
    case BOOK_HINDERANCE:
    case BOOK_CANTRIPS: //jmf: added 04jan2000
        return 1;

    case BOOK_CHANGES:
    case BOOK_CHARMS:
        return 2;

    case BOOK_CONJURATIONS_I:
    case BOOK_CONJURATIONS_II:
    case BOOK_PRACTICAL_MAGIC:
    case BOOK_NECROMANCY:
    case BOOK_CALLINGS:
    case BOOK_WIZARDRY:
        return 3;

    case BOOK_FLAMES:
    case BOOK_FROST:
    case BOOK_AIR:
    case BOOK_GEOMANCY:
        return 4;

    case BOOK_YOUNG_POISONERS:
    case BOOK_STALKING:    //jmf: added 24jun2000
    case BOOK_WAR_CHANTS:
        return 5;

    case BOOK_CLOUDS:
    case BOOK_POWER:
        return 6;

    case BOOK_ENCHANTMENTS:
    case BOOK_PARTY_TRICKS:     //jmf: added 04jan2000
        return 7;

    case BOOK_TRANSFIGURATIONS:
    case BOOK_DIVINATIONS:
        return 8;

    case BOOK_FIRE:
    case BOOK_ICE:
    case BOOK_SKY:
    case BOOK_EARTH:
    case BOOK_UNLIFE:
    case BOOK_CONTROL:
    case BOOK_SPATIAL_TRANSLOCATIONS:
        return 10;

    case BOOK_TEMPESTS:
    case BOOK_DEATH:
        return 11;

    case BOOK_MUTATIONS:
    case BOOK_BEASTS:           //jmf: added 23mar2000
        return 12;

    case BOOK_ENVENOMATIONS:
    case BOOK_WARP:
        return 15;

    case BOOK_TUKIMA:
        return 16;

    case BOOK_SUMMONINGS:
        return 18;

    case BOOK_ANNIHILATIONS: // Vehumet special
    case BOOK_DEMONOLOGY:    // Vehumet special
    case BOOK_NECRONOMICON:  // Kikubaaqudgha special
    case BOOK_MANUAL:
        return 20;

    case BOOK_DESTRUCTION:
        return 30;

    case BOOK_HEALING:
        return 100;

    default:
        return 1;
    }
}                               // end book_rarity()

bool is_valid_spell_in_book( int splbook, int spell )
{
    return which_spell_in_book(splbook, spell) != SPELL_NO_SPELL;
}

static bool which_spellbook( int &book, int &spell )
{
    const int avail_levels = player_spell_levels();

    // Knowing delayed fireball will allow Fireball to be learned for free -bwr
    if (avail_levels < 1 && !player_has_spell(SPELL_DELAYED_FIREBALL))
    {
        mpr("You can't memorise any more spells yet.");
        return (false);
    }
    else if (inv_count() < 1)
    {
        canned_msg(MSG_NOTHING_CARRIED);
        return (false);
    }

    mprf("You can memorise %d more level%s of spells.",
         avail_levels, (avail_levels > 1) ? "s" : "" );

    book = prompt_invent_item("Memorise from which spellbook?", MT_INVLIST,
                              OBJ_BOOKS );
    if (book == PROMPT_ABORT)
    {
        canned_msg( MSG_OK );
        return (false);
    }

    if (you.inv[book].base_type != OBJ_BOOKS
        || you.inv[book].sub_type == BOOK_MANUAL)
    {
        mpr("That isn't a spellbook!");
        return (false);
    }

    if (you.inv[book].sub_type == BOOK_DESTRUCTION)
    {
        tome_of_power( book );
        return (false);
    }

    spell = read_book( you.inv[book], RBOOK_MEMORISE );
    clrscr();

    return (true);
}                               // end which_spellbook()

// Returns false if the player cannot read/memorize from the book, 
// and true otherwise. -- bwr
static bool player_can_read_spellbook( const item_def &book )
{
    if (book.base_type != OBJ_BOOKS)
        return (true);

    if ((book.sub_type == BOOK_ANNIHILATIONS
            && you.religion != GOD_VEHUMET
            && (you.skills[SK_CONJURATIONS] < 10
                || you.skills[SK_SPELLCASTING] < 6))
        || (book.sub_type == BOOK_DEMONOLOGY
            && you.religion != GOD_VEHUMET
            && (you.skills[SK_SUMMONINGS] < 10
                || you.skills[SK_SPELLCASTING] < 6))
        || (book.sub_type == BOOK_NECRONOMICON
            && you.religion != GOD_KIKUBAAQUDGHA
            && (you.skills[SK_NECROMANCY] < 10
                || you.skills[SK_SPELLCASTING] < 6)))
    {
        return (false);
    }

    return (true);
}


void mark_had_book(int booktype)
{
    you.had_book[booktype] = true;

    if ( booktype == BOOK_MINOR_MAGIC_I
         || booktype == BOOK_MINOR_MAGIC_II
         || booktype == BOOK_MINOR_MAGIC_III)
    {
        you.had_book[BOOK_MINOR_MAGIC_I] = true;
        you.had_book[BOOK_MINOR_MAGIC_II] = true;
        you.had_book[BOOK_MINOR_MAGIC_III] = true;
    }
    else if (booktype == BOOK_CONJURATIONS_I
             || booktype == BOOK_CONJURATIONS_II)
    {
        you.had_book[BOOK_CONJURATIONS_I] = true;
        you.had_book[BOOK_CONJURATIONS_II] = true;
    }
}

int read_book( item_def &book, read_book_action_type action )
{
    if (book.base_type == OBJ_BOOKS && !player_can_read_spellbook( book ))
    {
        mpr( "This book is beyond your current level of understanding." );
        more();
        return (0);
    }

    // remember that this function is called from staff spells as well:
    const int keyin = spellbook_contents( book, action );

    if (book.base_type == OBJ_BOOKS)
        mark_had_book(book.sub_type);

    redraw_screen();

    /* Put special book effects in another function which can be called from
       memorise as well */

    set_ident_flags( book, ISFLAG_KNOW_TYPE );

    return (keyin);
}                               // end read_book()

// recoded to answer whether an UNDEAD_STATE is
// barred from a particular spell passed to the
// function - note that the function can be expanded
// to prevent memorisation of certain spells by
// the living by setting up an US_ALIVE case returning
// a value of false for a set of spells ... might be
// an idea worth further consideration - 12mar2000 {dlb}
bool undead_cannot_memorise(spell_type spell, char being)
{
    switch (being)
    {
    case US_HUNGRY_DEAD:
        switch (spell)
        {
        //case SPELL_REGENERATION:
        case SPELL_BORGNJORS_REVIVIFICATION:
        case SPELL_CURE_POISON_II:
        case SPELL_DEATHS_DOOR:
        case SPELL_NECROMUTATION:
        case SPELL_RESIST_POISON:
        case SPELL_SYMBOL_OF_TORMENT:
        case SPELL_TAME_BEASTS:
        case SPELL_BERSERKER_RAGE:
            return true;
        default:
            return false;
        }
        break;

    case US_UNDEAD:
        switch (spell)
        {
        case SPELL_AIR_WALK:
        case SPELL_ALTER_SELF:
        case SPELL_BLADE_HANDS:
        case SPELL_BORGNJORS_REVIVIFICATION:
        case SPELL_CURE_POISON_II:
        case SPELL_DEATHS_DOOR:
        case SPELL_DRAGON_FORM:
        case SPELL_GLAMOUR:
        case SPELL_ICE_FORM:
        case SPELL_INTOXICATE:
        case SPELL_NECROMUTATION:
        case SPELL_PASSWALL:
        case SPELL_REGENERATION:
        case SPELL_RESIST_POISON:
        case SPELL_SPIDER_FORM:
        case SPELL_STATUE_FORM:
        case SPELL_SUMMON_HORRIBLE_THINGS:
        case SPELL_SYMBOL_OF_TORMENT:
        case SPELL_TAME_BEASTS:
        case SPELL_BERSERKER_RAGE:
            return true;
        default:
            return false;
        }
        break;
    }

    return false;
}                               // end undead_cannot_memorise()

bool learn_spell(void)
{
    int chance = 0;
    int levels_needed = 0;
    int book, spell;
    int index;

    int i;
    int j = 0;

    for (i = SK_SPELLCASTING; i <= SK_POISON_MAGIC; i++)
    {
        if (you.skills[i])
            j++;
    }

    if (j == 0)
    {
        mpr("You can't use spell magic! I'm afraid it's scrolls only for now.");
        return (false);
    }

    if (you.duration[DUR_CONF])
    {
        mpr("You are too confused!");
        return (false);
    }

    if (you.duration[DUR_BERSERKER])
    {
        canned_msg(MSG_TOO_BERSERK);
        return (false);
    }

    if (!which_spellbook( book, spell ))
        return (false);

    mesclr(true);
    redraw_screen();

    if ( !isalpha(spell) )
    {
        canned_msg( MSG_HUH );
        return (false);
    }

    index = letter_to_index( spell );

    if (index >= SPELLBOOK_SIZE ||
        !is_valid_spell_in_book( you.inv[book].sub_type, index ))
    {
        canned_msg( MSG_HUH );
        return (false);
    }

    spell_type specspell = which_spell_in_book(you.inv[book].sub_type,index);

    if (specspell == SPELL_NO_SPELL)
    {
        canned_msg( MSG_HUH );
        return (false);
    }

    // You can always memorise selective amnesia:
    if (you.spell_no == 21 && specspell != SPELL_SELECTIVE_AMNESIA)
    {
        mpr("Your head is already too full of spells!");
        return (false);
    }

    if (you.is_undead && spell_typematch(specspell, SPTYP_HOLY))
    {
        mpr("You cannot use this type of magic!");
        return (false);
    }

    if (undead_cannot_memorise(specspell, you.is_undead))
    {
        mpr("You cannot use this spell.");
        return (false);
    }

    for (i = 0; i < 25; i++)
    {
        if (you.spells[i] == specspell)
        {
            mpr("You already know that spell!");
            return (false);
        }
    }

    levels_needed = spell_levels_required( specspell );

    if (player_spell_levels() < levels_needed)
    {
        mpr("You can't memorise that many levels of magic yet!");
        return (false);
    }

    if (you.experience_level < spell_difficulty(specspell))
    {
        mpr("You're too inexperienced to learn that spell!");
        return (false);
    }

    chance = spell_fail(specspell);
    const int temp_rand1 = random2(3);
    const int temp_rand2 = random2(4);

    mprf("This spell is %s %s to %s.",
         ((chance >= 80) ? "very" :
          (chance >= 60) ? "quite" :
          (chance >= 45) ? "rather" :
          (chance >= 30) ? "somewhat"
                         : "not that"),
         ((temp_rand1 == 0) ? "difficult" :
          (temp_rand1 == 1) ? "tricky"
                            : "challenging"),
         ((temp_rand2 == 0) ? "memorise" :
          (temp_rand2 == 1) ? "commit to memory" :
          (temp_rand2 == 2) ? "learn"
                            : "absorb"));

    snprintf(info, INFO_SIZE, "Memorise %s?", spell_title(specspell));
    if ( !yesno(info, true, 0, false) )
    {
        canned_msg( MSG_OK );
        return (false);
    }

    if (you.mutation[MUT_BLURRY_VISION] > 0
        && random2(4) < you.mutation[MUT_BLURRY_VISION])
    {
        mpr("The writing blurs into unreadable gibberish.");
        you.turn_is_over = true;
        return (false);
    }

    if (random2(40) + random2(40) + random2(40) < chance)
    {
        mpr("You fail to memorise the spell.");
        you.turn_is_over = true;

        if (you.inv[ book ].sub_type == BOOK_NECRONOMICON)
        {
            mpr("The pages of the Necronomicon glow with a dark malevolence...");
            miscast_effect( SPTYP_NECROMANCY, 8, random2avg(88, 3), 100, 
                            "reading the Necronomicon" );
        }
        else if (you.inv[ book ].sub_type == BOOK_DEMONOLOGY)
        {
            mpr("This book does not appreciate being disturbed by one of your ineptitude!");
            miscast_effect( SPTYP_SUMMONING, 7, random2avg(88, 3), 100,
                            "reading the book of Demonology" );
        }
        else if (you.inv[ book ].sub_type == BOOK_ANNIHILATIONS)
        {
            mpr("This book does not appreciate being disturbed by one of your ineptitude!");
            miscast_effect( SPTYP_CONJURATION, 8, random2avg(88, 3), 100,
                            "reading the book of Annihilations" );
        }

#ifdef WIZARD
        if (!you.wizard)
            return (false);
        else if (!yesno("Memorise anyway?"))
            return (false);
#else 
        return (false);
#endif
    }

    start_delay( DELAY_MEMORISE, spell_difficulty( specspell ), specspell );
    you.turn_is_over = true;

    did_god_conduct( DID_SPELL_CASTING, 2 + random2(5) );

    return (true);
}                               // end which_spell()

int count_staff_spells(const item_def &item, bool need_id)
{
    if (item.base_type != OBJ_STAVES)
        return (-1);
    
    if (need_id && !item_type_known(item))
        return (0);

    const int type = item.book_number();
    if ( !item_is_rod(item) || type == -1)
        return (0);

    int nspel = 0;
    while (nspel < SPELLBOOK_SIZE && is_valid_spell_in_book(type, nspel))
        ++nspel;

    return (nspel);
}

// Returns a measure of the rod spell power disrupted by a worn shield.
int rod_shield_leakage()
{
    const item_def *shield = player_shield();
    int leakage = 100;

    if (shield)
    {
        const int shield_type = shield->sub_type;
        leakage = shield_type == ARM_BUCKLER? 125 :
                  shield_type == ARM_SHIELD ? 150 :
                                              200;
        // Adjust for shields skill.
        leakage -= ((leakage - 100) * 5 / 10) * you.skills[SK_SHIELDS] / 27;
    }
    return (leakage);
}

int staff_spell( int staff )
{
    item_def& istaff(you.inv[staff]);
    // Spell staves are mostly for the benefit of non-spellcasters, so we're 
    // not going to involve INT or Spellcasting skills for power. -- bwr
    int powc = (5 + you.skills[SK_EVOCATIONS] 
                 + roll_dice( 2, you.skills[SK_EVOCATIONS] ))
                * 100
                / rod_shield_leakage(); 

    if (!item_is_rod(istaff))
    {
        canned_msg(MSG_NOTHING_HAPPENS);
        return (-1);
    }

    if (!item_type_known(istaff))
    {
        set_ident_flags( istaff, ISFLAG_KNOW_TYPE );
        you.wield_change = true;
    }

    const int num_spells = count_staff_spells(istaff, false);

    int keyin = 0;
    if (num_spells == 0)
    {
        canned_msg(MSG_NOTHING_HAPPENS);  // shouldn't happen
        return (0);
    }
    else if (num_spells == 1)
    {
        keyin = 'a';  // automatically selected if it's the only option
    }
    else
    {
        mprf(MSGCH_PROMPT,
             "Evoke which spell from the rod ([a-%c] spell [?*] list)? ",
             'a' + num_spells - 1 );

        // Note that auto_list is ignored here.
        keyin = get_ch();
        
        if (keyin == '?' || keyin == '*')
        {
            keyin = read_book( you.inv[staff], RBOOK_USE_STAFF );
            // [ds] read_book sets turn_is_over.
            you.turn_is_over = false;
        }
    }

    if ( !isalpha(keyin) )
    {
        canned_msg(MSG_HUH);
        return -1;
    }

    const int idx = letter_to_index( keyin );

    // converting sub_type into book index type
    const int type = istaff.book_number();

    if ((idx >= SPELLBOOK_SIZE) || !is_valid_spell_in_book(type, idx))
    {
        canned_msg(MSG_HUH);
        return -1;
    }

    const spell_type spell = which_spell_in_book( type, idx );
    const int mana = spell_mana( spell ) * ROD_CHARGE_MULT;
    const int diff = spell_difficulty( spell );

    int food = spell_hunger(spell);

    // For now player_energy() is always 0, because you've got to
    // be wielding the rod...
    if (you.is_undead == US_UNDEAD || player_energy() > 0)
    {
        food = 0;
    }
    else
    {
        food -= 10 * you.skills[SK_EVOCATIONS];
        if (food < diff * 5)
            food = diff * 5;
    }
        
    if (food && (you.hunger_state < HS_HUNGRY || you.hunger <= food))
    {
        mpr("You don't have the energy to cast that spell.");
        return (-1);
    }
    
    if (istaff.plus < mana)
    {
        mpr("The rod doesn't have enough magic points.");
        // Don't lose a turn for trying to evoke without enough MP - that's
        // needlessly cruel for an honest error.
        return (-1);
    }

    if (you.experience_level < diff)
    {
        mprf("You need to be at least level %d to use that.", diff);
        return (-1);
    }

    // All checks passed, we can cast the spell
    if (your_spells(spell, powc, false) == SPRET_ABORT)
        return (-1);

    make_hungry( food, true );

    istaff.plus -= mana;

    you.wield_change = true;
    you.turn_is_over = true;

    return (roll_dice( 1, 1 + spell_difficulty(spell) / 2 ));
}
